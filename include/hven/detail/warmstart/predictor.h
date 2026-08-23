// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// predictor.h — THE TANGENTIAL PREDICTOR: given a converged WarmStart at
// parameter p and a step dp, produce a FIRST-ORDER-ACCURATE WarmStart at
// p + dp by solving ONE parametric-sensitivity KKT system on the active set
// FROZEN at the warm start.
//
// THE SYSTEM. At a KKT point of
//
//     min f(x;p)  s.t.  cE(x;p) = 0,  cI(x;p) <= 0,  l(p) <= x <= u(p)
//
// (nlp_model.h's conventions verbatim: cI <= 0, lambda_i >= 0, and
// stationarity grad f + Je^T lambda_e + Ji^T lambda_i - z = 0), differentiating
// the active-set KKT conditions with respect to p along dp gives
//
//     W dx + Je^T dlambda_e + JA^T dlambda_A - dz = -(d/dp grad_x L) dp
//     Je dx                                       = -(d/dp cE) dp
//     JA dx                                       = -(d/dp cI_A) dp
//     dx_i                                        =  (d/dp bound_i) dp,  i pinned
//
// with W the exact Lagrangian Hessian at (x, lambda) and A the frozen active
// inequality set. That is exactly the linear system kkt_assembly.h already
// assembles for the QP engine, with a DIFFERENT right-hand side -- which is
// the whole idea: the predictor is a right-hand-side change, not new linear
// algebra. Solving it and adding (dx, dlambda, dz) to the warm start's own
// (x, lambda, z) is the classical parametric-sensitivity ("tangential")
// predictor of Fiacco's sensitivity theorem; the four activity repairs below
// are sIpopt's fix-relax scheme, each realized as a Schur border over the
// same factorization.
//
// WHAT IT COSTS: ONE extra model evaluation (at ONE probe parameter value --
// see PARAMETER DERIVATIVES below), ONE Hessian/Jacobian evaluation at the
// warm start's own point, and ONE KKT factorization. Every activity change
// afterwards is a Schur-complement BORDER over that same factorization,
// never a second factorization.
//
// --- PARAMETER DERIVATIVES: ONE DIRECTIONAL FORWARD DIFFERENCE -------------
//
// The right-hand side needs the p-derivatives of grad_x L, cE, cI and the
// bounds, but ONLY ever contracted against dp -- i.e. one DIRECTIONAL
// derivative each, never a full parameter Jacobian. So the model is evaluated
// at exactly one extra point,
//
//     p_probe = p + h * dp/||dp||,   h = fd_step_scale * sqrt(eps) * max(1, ||p||),
//
// and each needed derivative-times-dp is (g(p_probe) - g(p)) * ||dp||/h, at
// the SAME x and the SAME multipliers. h is the standard forward-difference
// step, balancing O(h) truncation against O(eps/h) cancellation.
//
// --- THE THREE ERROR TERMS -------------------------------------------------
//
// A prediction carries (1) the LINEARIZATION error O(||dp||^2), inherent to
// any first-order predictor; (2) the FINITE-DIFFERENCE error above,
// O(||dp||*sqrt(eps)) -- which is identically zero for a model whose data
// depend on p through the identity map, so its absence in a measurement
// proves nothing about the difference; and (3) the REGULARIZATION error of
// the KKT system itself: this file assembles the SAME Tikhonov-regularized K
// the QP engine does, with delta/mu taken from the warm start's own
// effective values, so the step carries a RELATIVE error of order delta.
// On a family whose solution path is affine in p, term (3) dominates and is
// proportional to delta * ||dp|| with a family-dependent constant (the
// system's conditioning); tests/sqp/test_predictor.cpp's affine-path tests
// measure at two deltas precisely so that floor is identified by how it
// MOVES WITH THE KNOB. NO ITERATIVE REFINEMENT is applied against the
// unregularized operator, deliberately: at any dp a caller would actually
// take, term (1) dominates term (3) by orders.
//
// --- PARAMETER RESTORATION, PART OF THE CONTRACT ---------------------------
//
// `model` is taken by MUTABLE reference because probing requires
// set_parameters (non-const by nlp_model.h's design), and predict() RESTORES
// the model's entry parameters before returning -- on the throwing paths
// too, via an RAII restorer. A caller never has to save/restore around a
// predict() call.
//
// --- ACTIVITY CHANGES: FIX-RELAX, RATIO-TESTED ------------------------------
//
// A frozen-set linearization is valid only while the active set holds.
// Around a threshold it does not, and the four repairs below are applied
// over the SAME factorized K0 (different borders each round, never a second
// factorization):
//
//   FIX   a free variable whose predicted value crosses a bound: CLAMP it to
//         that bound and PIN it (BorderOps::pin_variable with diagonal
//         -dual_mu, and a border right-hand side of "bound at p+dp minus x"
//         -- the displacement that lands the variable exactly ON the bound).
//   RELAX a pinned variable whose predicted bound multiplier has the WRONG
//         SIGN (z < 0 at a lower bound, z > 0 at an upper bound): drop its
//         pin border. The variable's own stationarity row additionally has
//         the frozen z subtracted from its right-hand side, which forces the
//         released multiplier to land at numerically zero (exactly 0.0 on
//         MKL; an O(1e-34) residue on Accelerate) instead of staying at its
//         frozen value -- at a genuine crossing the frozen z is O(||dp||),
//         so omitting this would silently cost the predictor its order.
//   DROP  an active inequality row whose predicted multiplier goes negative:
//         deactivate it with BorderOps::delete_k0_row, whose border
//         right-hand side is set to -lambda_j so the row's multiplier
//         INCREMENT lands the predicted multiplier at 0 (a rhs of 0 there
//         would pin the INCREMENT and leave the multiplier at its frozen
//         value -- the same first-order error as the relax case).
//   ADD   an inactive inequality row the predicted point VIOLATES: activate
//         it with BorderOps::add_ineq_row, right-hand side -(cI_j +
//         d cI_j dp), the linearized residual that drives the row back to
//         equality.
//
// Each variable and each row may change status AT MOST ONCE per predict()
// call, which bounds the loop at n + mi + 1 rounds and makes cycling
// impossible. Set PredictorOptions::allow_activity_change = false to take
// the raw frozen-set step with no repairs at all.
//
// WHERE THE REPAIRS ARE APPLIED: THE RATIO TEST. The repairs are NOT all
// evaluated at the full step -- on a family stepping across an activation
// threshold that overshoots catastrophically, pinning dozens of variables
// the true solution keeps free, and the loop cannot repair its own overshoot
// (a run clamped flat against a bound reads zero multipliers throughout, so
// no RELAX trigger ever fires). Instead the prediction is read as a path in
// a scalar homotopy variable t (the model at p + t*dp, t in [0, 1]): the
// frozen-set sensitivity system is a DIRECTION along that path, every
// quantity the repairs test is AFFINE in t with known slope, and the loop
// advances only to the EARLIEST crossing (ties together), changes those
// entities' status, and re-solves over the remaining interval. On a
// piecewise-affine family the resulting path IS the solution path,
// breakpoint for breakpoint, exact up to the regularization floor on both
// sides of a threshold; on a curved family the O(||dp||^2) linearization
// error is unchanged -- the direction is still the frozen-set tangent, only
// its length is now bounded. This is the parametric active-set homotopy
// (the qpOASES lineage), composed with sIpopt's border-per-repair scheme,
// which is what makes a per-breakpoint loop affordable.
//
// One Schur solve per breakpoint instead of one per round;
// PredictorOptions::max_activity_rounds caps how many are paid (truncation
// semantics there), and predict()'s `reached_t` out-parameter reports how
// far the path got.
//
// WEAKLY ACTIVE ROWS ARE KEPT, NOT DROPPED -- a deliberate decision, and the
// one place this file's behaviour at a DEGENERATE warm start is not forced
// by the mathematics. Where strict complementarity fails (an active
// constraint with multiplier exactly 0), the solution path is only
// DIRECTIONALLY differentiable: a first-order predictor must pick one
// branch. This file picks KEEP: the DROP test fires only on a STRICTLY
// negative predicted multiplier, beyond kDualSignTol chosen so a multiplier
// that is zero to within the warm start's convergence accuracy counts as
// zero. Reasons: relaxing on a vanishing multiplier is what sIpopt's pairing
// avoids; keeping a row the true path also keeps costs nothing and is exact;
// and the failure mode of the wrong choice is bounded and self-correcting --
// the prediction is a WARM START, and the consuming solve re-derives the
// active set for itself.
//
// --- WHAT IT DOES NOT DO ---------------------------------------------------
//
// NO HOT-START REUSE; predict() ALWAYS FACTORIZES ITS OWN KKT SYSTEM, and
// the returned WarmStart therefore NEVER carries a `hot` handle. Three
// reasons, worth recording against any future attempt:
//   1. A HotState's K0 is the engine's LAST SUBPROBLEM's matrix, split
//      between K0 proper and live borders by a ledger this layer would have
//      to replicate to reuse it correctly -- duplicating engine-internal
//      bookkeeping here would couple the predictor to the engine's border
//      strategy, the coupling this file is deliberately without.
//   2. Back-solving against a SHARED factorization is not provably free of
//      side effects (SchurComplement takes the KKT factor by non-const
//      reference), and a DETACH-style private copy is not available either
//      (BorderState is deliberately non-copyable AND non-movable).
//   3. Even a private copy would be INERT: the engine's reuse gate includes
//      `values_hash`, a fingerprint over H/Ae/Ai VALUES. The predictor
//      factorizes at the warm point x and parameter p; the consuming solve
//      linearizes at x + dx and p + dp -- different values for any model
//      worth predicting for, so the handle would be refused by the gate.
//
// TWO RIGHT-HAND-SIDE CONVENTIONS, ON PURPOSE. The constraint rows carry
// the PURE sensitivity term (`-d_ce`, `-d_ci`), omitting the warm start's
// own base residual; the pin borders carry `bound_at(p+dp) - x_i`, which
// INCLUDES it. Both residuals are zero at a converged warm start, so the
// two agree wherever the contract is met; when it is NOT met (a stale warm
// start), the pin form is free and puts a variable that should be ON its
// bound THERE, while folding constraint residuals in would turn the step
// into sensitivity-plus-Newton-correction -- a DIFFERENT object whose
// O(||dp||^2) claim would be conditional on the residual. This file keeps
// the pure form so the order claim is unconditional.
//
// A NOTE ON INFINITE BOUNDS. `d_lower`/`d_upper` are differences of the
// model's bound vectors, so a model reporting a TRUE +/-inf bound produces
// inf - inf = NaN in that component. That NaN is CONTAINED rather than
// accidental: every read of those two vectors is either behind a
// std::isfinite check on the resulting bound (the FIX crossing test) or
// reached only for a variable already PINNED at that bound, which by
// definition has a finite one. A NaN can therefore never enter the
// right-hand side. Any new read of d_lower/d_upper must preserve that
// property.
//
// NO SqpDriver DEPENDENCY, on purpose: the predictor is a standalone layer
// over the same linear algebra the engine uses; continuation.h composes
// predict() with solve().
//
// NEVER MUTATES `warm`, taken by const reference and copied into the
// returned object; the caller's WarmStart (and any `hot` handle it carries)
// is untouched and remains exactly as usable as it was.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/detail/kkt/border_ops.h>
#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/kkt/schur_complement.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/working_set.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

// Tuning for predict().
struct PredictorOptions {
    // Multiplies the forward-difference step h = sqrt(eps)*max(1, ||p||) used
    // for the parameter derivatives (see this header's PARAMETER DERIVATIVES
    // note). 1.0 is the textbook choice; a model whose parameter dependence is
    // unusually stiff or unusually smooth in one direction can retune it here.
    // Must be > 0 and finite.
    double fd_step_scale = 1.0;

    // false takes the RAW frozen-set step: no clamping, no pinning, no
    // dropping -- the pure linearization, which is what a caller measuring
    // the sensitivity system itself wants. true (the default) runs the
    // ratio-tested fix-relax loop described in this header.
    bool allow_activity_change = true;

    // How many BREAKPOINTS the ratio-tested path may stop at (see this
    // header's RATIO TEST section). Ignored entirely when
    // allow_activity_change is false. Must be >= 0; 0 means "take the raw
    // frozen-set step but keep it inside the first crossing".
    //
    // WHY THERE IS A CAP AT ALL, AND IT IS NOT ONLY COST. Cost: each
    // breakpoint is one Schur solve and one border update over the retained
    // factorization. The each-entity-changes-status-once rule already bounds
    // the loop at n + mi + 1 rounds and makes termination a PROOF rather than
    // a budget; this is the BUDGET, and the smaller of the two governs. The
    // reason that actually sets the default is ACCURACY: the direction is
    // recomputed at every breakpoint but always from the SAME factorized K0 --
    // W, Je and Ji are frozen at the warm point and the model is never
    // re-evaluated along the path (this file is a predictor, not a predictor-
    // corrector). On a piecewise-affine family that costs nothing; on a CURVED
    // family following the stale tangent further is more extrapolation, not
    // more prediction -- measured on the project's curved collocation fixture,
    // where an uncapped path both drops rows the solution keeps active and
    // raises the consuming solve's minor count against one stopped after a few
    // breakpoints.
    //
    // WHAT TRUNCATION MEANS, because it is the one new failure mode the ratio
    // test introduces: when the cap is reached the path STOPS at the
    // breakpoint it reached, at some t <= 1, and the returned WarmStart is the
    // point and activity THERE -- a converged-path point at p + t*dp offered
    // as a seed for p + dp. That is deliberately the conservative end: it is
    // never behind the unpredicted warm start (which is t = 0) and never past a
    // crossing it has not accounted for, so the worst a truncated prediction
    // can do is predict LESS. It is NOT reported as kDegraded -- a step was
    // computed and applied, which is what that enum distinguishes; it IS
    // visible through `reached_t < 1.0` (see predict()'s WHAT `reached_t`
    // MEANS note).
    //
    // THE DEFAULT OF 4 is an engineering choice for HEADROOM, taken inside
    // the flat region of the measured consuming-solve-cost-vs-budget curve
    // rather than at either edge -- not that curve's argmin and not claimed
    // to be. The right value is genuinely family-dependent, which is why this
    // is an option and not a constant. Measure when you retune it; do not
    // assume more rounds is more accuracy.
    Index max_activity_rounds = 4;
};

// WHICH PATH predict() TOOK -- because TWO of the three paths return the
// IDENTITY prediction and a caller's accounting must not conflate them.
//
//   kPredicted  a sensitivity step was computed and applied.
//   kZeroStep   dp == 0: nothing to predict. The returned object is the input
//               warm start and that is the CORRECT answer, not a failure.
//   kDegraded   the step could not be computed (see predict()'s DEGRADES
//               note) and the returned object is the input warm start as a
//               fallback. A caller that reports this as a prediction taken is
//               asserting something false -- which is precisely why this is an
//               enum and not a bool: continuation.h's ledger has to count
//               kPredicted and kDegraded separately, and a sweep in which
//               every predict() degraded must not look identical to one in
//               which every predict() succeeded.
//
// Reported through an optional out-parameter rather than a field on WarmStart
// (warm_start.h is the shared value object EVERY solve emits, where a
// predictor-only field would be meaningless on almost every instance) and
// rather than a new return type (a defaulted out-param leaves every existing
// call site compiling unchanged).
enum class PredictorOutcome { kPredicted, kZeroStep, kDegraded };

// Defined in src/drivers/sqp_print.cpp with the other solver-enum printers:
// printing belongs in a .cpp TU, and code-generating this switch into every
// TU including this header was measurable build cost for nothing.
const char *to_string(PredictorOutcome outcome);

namespace predictor_detail {

// A predicted bound multiplier of the wrong sign, or a predicted inequality
// multiplier below -kDualSignTol, is what triggers a RELAX/DROP. The test
// applies this threshold relative to the frozen multiplier's own magnitude,
// floored at 1 (kDualSignTol * max(1, |multiplier|)), and sits at 1e-9
// because that is the scale below which a converged warm start's
// multipliers are indistinguishable from zero (SqpOptions::kkt_tol defaults
// to 1e-8). A LOOSER threshold would start relaxing genuinely active
// constraints; a TIGHTER one would relax on roundoff, which at a weakly
// active row is precisely the misbehaviour the WEAKLY ACTIVE ROWS decision
// exists to avoid.
constexpr double kDualSignTol = 1e-9;

// A predicted point counts as having CROSSED a bound (or violated an inactive
// inequality) only past this much, again scaled by the quantity's own
// magnitude. Genuine crossings are O(||dp||) -- vastly above this -- so the
// threshold's only job is to keep roundoff at a variable that already sits on
// its bound from manufacturing an activity change.
constexpr double kGeomTol = 1e-10;

// Two entities whose ratio-tested crossing points are within
// this much of each other (RELATIVE to the interval still to be traversed)
// cross TOGETHER and change status in the same round. Ties are not an edge
// case but the normal situation at a threshold a family reaches
// symmetrically; breaking such a tie arbitrarily would spend an extra Schur
// solve to advance a step of length zero. 1e-9 is loose enough to catch a tie
// that survives the two different arithmetic paths its members' ratios were
// computed by, and tight enough that genuinely distinct crossings in this
// project's fixtures sit orders of magnitude clear of it.
constexpr double kTauTieTol = 1e-9;

// With less than this much of the path left to traverse, the loop stops where
// it is. Every border right-hand side below divides by the remaining interval
// (each is "the displacement that lands this quantity where it belongs AT
// t = 1", spread over what is left), so this is what keeps that division away
// from zero; and there is nothing left to predict in the last 1e-12 of a step.
//
// The one path that reaches it: a breakpoint whose ratio test lands exactly on
// t = 1. The activity change is applied (the loop applies before re-testing)
// but no further solve is made, so that entity's own multiplier is left at the
// value the last solve gave it rather than re-derived on the new set. That is
// the right trade -- it is a warm start, the consuming solve re-derives -- and
// it needs a crossing at literally the end of the step to happen at all.
constexpr double kMinRemaining = 1e-12;

// Restores a ParametricNlpModel's parameters on every exit path, including a
// throw. Non-copyable, non-movable: there is exactly one, on the stack of the
// predict() call that made the probe.
class ParameterRestorer {
  public:
    ParameterRestorer(ParametricNlpModel &model, Vec p) : model_(model), p_(std::move(p)) {}
    ParameterRestorer(const ParameterRestorer &) = delete;
    ParameterRestorer &operator=(const ParameterRestorer &) = delete;

    ~ParameterRestorer() noexcept {
        // set_parameters' only documented throw is a size mismatch, and p_
        // came from this same model's parameters(), so it cannot fire here.
        // A destructor may not throw regardless, and a model that throws
        // anything else from set_parameters is already outside its own
        // contract -- which is why the happy path restores EXPLICITLY,
        // before returning, rather than leaving the restore to this
        // destructor.
        try {
            model_.set_parameters(p_);
        } catch (...) {
        }
    }

  private:
    ParametricNlpModel &model_;
    Vec p_;
};

// What one live Schur-complement border of the predictor's system is ABOUT.
// Kept parallel to SchurComplement's own add_border ORDER (drop_border(k)
// re-indexes, and this vector is erased at the same index), which is how a
// pin can be found and released by variable index later.
//
// A border's right-hand-side entry is NOT stored here: it is a function of
// WHERE ON THE PATH the system is being solved, so it is derived once per
// round from `kind` and `target` (see border_rhs below). Caching it would be
// one more thing to keep in step with t.
struct PredictorBorder {
    enum class Kind {
        kPin,     // target = variable index; pins x_target at a bound
        kRowDrop, // target = Ai row index (in K0's initial working set); deactivates it
        kRowAdd,  // target = Ai row index (not in K0); activates it
    };
    Kind kind;
    Index target;
};

// One entity that the trigger tests say must change status,
// together with the point on the path at which its own quantity actually
// crosses. The loop advances to the smallest `tau` and applies only the
// changes that tie with it.
struct PredictorBreakpoint {
    enum class Kind { kFix, kRelax, kDrop, kAdd };
    Kind kind;
    Index target;     // variable index (kFix/kRelax) or Ai row index (kDrop/kAdd)
    BoundState state; // kFix only: which bound was reached
    double tau;       // distance along the REMAINING interval, in [0, remaining]
};

// The model quantities predict() needs at ONE parameter value, all at the
// warm start's own x and multipliers.
struct ModelSample {
    Vec grad_lagrangian; // grad f + Je^T lambda_e + Ji^T lambda_i (NOT including -z)
    Vec ce, ci;
    Vec lower, upper;
};

inline ModelSample sample_model(const NlpModel &model, const Vec &x, const Vec &lambda_e,
                                const Vec &lambda_i) {
    ModelSample s;
    s.grad_lagrangian = model.eval_grad(x);
    if (model.me() > 0) {
        s.grad_lagrangian += model.eval_jac_e(x).transpose() * lambda_e;
    }
    if (model.mi() > 0) {
        s.grad_lagrangian += model.eval_jac_i(x).transpose() * lambda_i;
    }
    s.ce = model.eval_ce(x);
    s.ci = model.eval_ci(x);
    s.lower = model.lower();
    s.upper = model.upper();
    return s;
}

} // namespace predictor_detail

// The tangential predictor. See this header's banner for the system solved,
// the fix-relax repairs, and the three contracts (parameter restoration, no
// input mutation, never a `hot` handle).
//
// THROWS std::invalid_argument (sizes always in the message) on a cold
// `warm`, a `warm` whose blocks do not match `model`'s (n, me, mi), a dp of
// the wrong size or a non-finite dp, or an fd_step_scale that is not finite
// and positive. A
// stale-but-well-shaped warm start is NOT rejected: predicting from a point
// that is not actually a KKT point of the model produces a poor prediction,
// which is a warm start like any other, not an error.
//
// DEGRADES to the IDENTITY prediction -- a copy of `warm` at the same point --
// in two cases, which `outcome` tells apart: dp == 0 (kZeroStep: nothing to
// predict, and the identity IS the answer), and a failure anywhere in the
// numerical section (kDegraded). The second is a deliberate choice: this layer
// is an ACCELERATOR, and the worst outcome it may impose on a caller is "no
// acceleration". The returned object is the caller's own warm start, exactly
// as valid at p as it ever was -- never a half-updated point.
//
// THE VALIDATE-THEN-CATCH-EVERYTHING TAXONOMY. The split is by PHASE, not by
// exception TYPE: exception types have crossed type lines historically (the
// dense factor path has reported both illegal-argument and runtime failures
// under different names), so a type-keyed net either leaks or goes stale on
// exactly such a change. EVERY caller-input check runs FIRST and throws;
// from that point on, nothing that can fail is the caller's fault in a way
// this layer can act on, and ANY std::exception is caught and reported as
// kDegraded. What is inside that net, all of it "cannot predict here" rather
// than "you called this wrong":
//   - the linear algebra (Pardiso, a singular or ill-conditioned Schur
//     complement), whatever type it reports through;
//   - the MODEL ITSELF at the probe parameter p + h*dp/||dp||, which a model
//     is entitled to reject (a parameter domain boundary is a real thing,
//     and the correct response to "the probe point is outside the model" is
//     to decline to predict, not to kill the caller's sweep);
//   - as a last resort, this file's own internal invariants (drop_pin's
//     std::logic_error), which cannot fire -- see its comment.
// Nothing is SWALLOWED: `outcome` is the report, and a caller that passes
// nullptr for it has said it does not want one.
//
// `outcome` (optional; nullptr = do not report) is written on every path that
// RETURNS. A path that THROWS writes nothing, because there is no result for
// it to describe -- a caller reading it after catching would be reading its
// own stale value. A caller running predict() in a LOOP should pass a
// non-null outcome and count kDegraded: a sweep in which every step silently
// degraded is otherwise indistinguishable from one that worked, and merely
// slower.
//
// --- WHAT `reached_t` MEANS -------------------------------------------------
//
// `reached_t` (optional; nullptr = do not report) is THE FRACTION OF dp THE
// RETURNED PREDICTION ACTUALLY TRAVERSED -- the endpoint of the ratio-tested
// path, in the homotopy variable t this header's RATIO TEST section defines.
// It is written on exactly the paths `outcome` is: every path that RETURNS,
// none that throws.
//
// WHY IT EXISTS, AND WHY IT IS NOT A FOURTH PredictorOutcome. The round budget
// (PredictorOptions::max_activity_rounds) can stop the path short, and a
// truncated prediction is STILL A PREDICTION -- a step was computed and applied
// -- so kDegraded would be a lie and a new enumerator would change a
// cross-layer contract continuation.h's ledger counts against. But
// "kPredicted" alone cannot distinguish a prediction that crossed the whole
// step from one that stopped at 5 % of it, and at a budget of 0 with a
// zero-length first crossing it cannot even distinguish one from THE IDENTITY.
// A plain double closes that without touching the enum: `reached_t < 1.0` IS
// the truncation signal, and it says by how much.
//
// THE FULL CONTRACT. The PAIR is what is unambiguous, not either alone:
//
//   (kZeroStep,  0.0)        dp == 0. Nothing to traverse; the identity IS the
//                            correct answer at p + dp, because p + dp == p.
//   (kDegraded,  0.0)        the step could not be computed. Nothing was
//                            traversed and the returned object is the input.
//   (kPredicted, 0.0)        A STEP WAS COMPUTED AND NONE OF IT WAS TAKEN: the
//                            first crossing is at t = 0 and the budget stopped
//                            there. The returned object IS the identity, and
//                            this pair is the only way a caller can see that.
//   (kPredicted, 0 < t < 1)  TRUNCATED: a genuine partial prediction at
//                            p + t*dp, offered as a seed for p + dp.
//   (kPredicted, 1.0)        the path crossed the whole step. EXACTLY 1.0 --
//                            the terminal advance ASSIGNS it rather than
//                            accumulating to it, so `*reached_t == 1.0` is a
//                            legitimate test and not a floating-point hope.
//
// With allow_activity_change = false the path never stops early, so
// `reached_t` is 1.0 on every non-degenerate call and the parameter is
// uninformative -- which is correct: there is no truncation in that mode.
inline WarmStart predict(ParametricNlpModel &model, const WarmStart &warm, const Vec &dp,
                         const PredictorOptions &opts = {}, PredictorOutcome *outcome = nullptr,
                         double *reached_t = nullptr) {
    const auto report = [outcome](PredictorOutcome value) {
        if (outcome != nullptr) {
            *outcome = value;
        }
    };
    const auto report_t = [reached_t](double value) {
        if (reached_t != nullptr) {
            *reached_t = value;
        }
    };
    const Index n = model.n();
    const Index me = model.me();
    const Index mi = model.mi();
    const Index np = model.parameter_dim();

    // ---- CALLER-INPUT VALIDATION -------------------------------------
    if (!warm.valid) {
        throw std::invalid_argument(
            "predict: warm.valid is false (a cold WarmStart carries no point, multipliers or "
            "activity to predict from); construct the prediction's base from a solve");
    }
    if (opts.fd_step_scale <= 0.0 || !std::isfinite(opts.fd_step_scale)) {
        throw std::invalid_argument(
            fmt::format("predict: PredictorOptions::fd_step_scale is {}, must be finite and > 0",
                        opts.fd_step_scale));
    }
    if (opts.max_activity_rounds < 0) {
        throw std::invalid_argument(fmt::format(
            "predict: PredictorOptions::max_activity_rounds is {}, must be >= 0 (0 means "
            "'stop the predicted path at the first activity change')",
            opts.max_activity_rounds));
    }
    if (dp.size() != np) {
        throw std::invalid_argument(fmt::format(
            "predict: dp has size {}, expected {} (= model.parameter_dim())", dp.size(), np));
    }
    if (!dp.allFinite()) {
        throw std::invalid_argument("predict: dp has a non-finite entry");
    }
    if (warm.x.size() != n || warm.z.size() != n) {
        throw std::invalid_argument(
            fmt::format("predict: warm.x has size {} and warm.z size {}, both expected {} "
                        "(= model.n())",
                        warm.x.size(), warm.z.size(), n));
    }
    if (warm.lambda_e.size() != me || warm.lambda_i.size() != mi) {
        throw std::invalid_argument(
            fmt::format("predict: warm.lambda_e has size {} (expected {} = model.me()) and "
                        "warm.lambda_i size {} (expected {} = model.mi())",
                        warm.lambda_e.size(), me, warm.lambda_i.size(), mi));
    }
    if (warm.qp_working_set.n() != n || warm.qp_working_set.mi() != mi) {
        throw std::invalid_argument(
            fmt::format("predict: warm.qp_working_set is ({}, {}), expected ({}, {}) "
                        "(= model.n(), model.mi())",
                        warm.qp_working_set.n(), warm.qp_working_set.mi(), n, mi));
    }

    // BELOW THIS LINE EVERY EXIT RETURNS; nothing above it does, and a throwing
    // path must leave both out-parameters alone (predict()'s own note on
    // `outcome`). 0.0 is the answer for both identity exits and for the
    // degradation handler -- none of them traverses anything -- so setting it
    // once here is what makes "no return path forgets to report" structural
    // rather than a thing to check at each `return`. The one success path
    // overwrites it with the t it reached.
    report_t(0.0);

    // The prediction starts as a copy of its base -- every field the step does
    // not touch (structure_hash, the funnel width, the trust-region radius,
    // the effective regularization) carries forward unchanged, which is what
    // makes the result ingestible by a solve at p + dp. `hot` is the one field
    // deliberately dropped; see this header's NO HOT-START REUSE note.
    WarmStart identity = warm;
    identity.hot.reset();

    const double dp_norm = dp.norm();
    if (!(dp_norm > 0.0)) {
        report(PredictorOutcome::kZeroStep);
        return identity; // no probe, no factorization: the identity IS the answer
    }

    try {
        const Vec p_base = model.parameters();
        predictor_detail::ParameterRestorer restore_params(model, p_base);

        const Vec &x = warm.x;
        const predictor_detail::ModelSample base =
            predictor_detail::sample_model(model, x, warm.lambda_e, warm.lambda_i);
        SpMatRM H = model.eval_hess(x, 1.0, warm.lambda_e, warm.lambda_i);
        H.makeCompressed();
        Eigen::SparseMatrix<double, Eigen::RowMajor> Je = model.eval_jac_e(x);
        Je.makeCompressed();
        Eigen::SparseMatrix<double, Eigen::RowMajor> Ji = model.eval_jac_i(x);
        Ji.makeCompressed();

        // ---- THE ONE EXTRA MODEL EVALUATION ------------------------------
        const double h = opts.fd_step_scale * std::sqrt(std::numeric_limits<double>::epsilon()) *
                         std::max(1.0, p_base.norm());
        const Vec p_probe = p_base + (h / dp_norm) * dp;
        model.set_parameters(p_probe);
        const predictor_detail::ModelSample probe =
            predictor_detail::sample_model(model, x, warm.lambda_e, warm.lambda_i);
        // Restore EXPLICITLY here rather than leaving it to restore_params: every
        // line below reads a model that must be back at p, and the RAII object is
        // the backstop for the throwing paths, not the mechanism.
        model.set_parameters(p_base);

        // (d/dp g) dp for each quantity, from the single directional difference.
        const double fd_scale = dp_norm / h;
        const Vec d_grad_lagrangian = fd_scale * (probe.grad_lagrangian - base.grad_lagrangian);
        const Vec d_ce = fd_scale * (probe.ce - base.ce);
        const Vec d_ci = fd_scale * (probe.ci - base.ci);
        const Vec d_lower = fd_scale * (probe.lower - base.lower);
        const Vec d_upper = fd_scale * (probe.upper - base.upper);

        // "This bound, at path fraction t" -- ONE definition, read by all four
        // sites that need it: the FIX trigger test and its ratio test, the pin
        // border's right-hand side, and the exact clamp of out.x at the end.
        // Writing the expression four times invited four chances to disagree,
        // and three of the four would then be invisible in out.x. See this
        // header's note on infinite bounds for why the NaN a true +/-inf bound
        // produces here is contained rather than accidental.
        //
        // The bounds move AFFINELY in t because d_lower/d_upper are single
        // directional differences (this header's PARAMETER DERIVATIVES note),
        // so no accuracy claim changes by evaluating them part-way.
        const auto bound_at_frac = [&](Index i, BoundState state, double t) {
            return state == BoundState::kAtUpper ? base.upper(i) + t * d_upper(i)
                                                 : base.lower(i) + t * d_lower(i);
        };

        // ---- THE SENSITIVITY SYSTEM --------------------------------------
        //
        // A QpProblem is the carrier kkt_assembly.h/border_ops.h consume; here it
        // holds the model's own linearization at (x, lambda) IN X-SPACE (the
        // driver's subproblem is in STEP space and shifts the bounds by -x; this
        // one never needs that, since nothing below reads qp.lower/qp.upper --
        // assemble_kkt_full eliminates no variable, so it has no bound value to
        // substitute -- and the bounds the fix-relax loop tests against are the
        // model's own, at p and at p + dp).
        QpProblem qp;
        qp.H = H;
        qp.g = base.grad_lagrangian;
        qp.Ae = Je;
        qp.be = -base.ce;
        qp.Ai = Ji;
        qp.bi = -base.ci;
        qp.lower = base.lower;
        qp.upper = base.upper;
        qp.validate();

        // The regularization the warm start's own last subproblem was solved with,
        // so the predictor's K0 is the same matrix the engine would have built.
        // -1 is warm_start.h's "never populated" sentinel for both.
        QpOptions qopts;
        if (warm.primal_delta > 0.0) {
            qopts.primal_delta = warm.primal_delta;
        }
        if (warm.dual_mu > 0.0) {
            qopts.dual_mu = warm.dual_mu;
        }

        const std::vector<Index> &rows0 = warm.qp_working_set.active_ineq();
        WorkingSet ws0(n, mi);
        for (Index row : rows0) {
            ws0.add_ineq(row);
        }
        const KktAssembly k0 = assemble_kkt_full(qp, ws0, qopts);
        const Index k0_rows = k0.K.rows(); // n + me + rows0.size()

        // Position of each frozen active row within K0's working-row block, or -1.
        std::vector<Index> row_pos(static_cast<std::size_t>(mi), -1);
        for (std::size_t k = 0; k < rows0.size(); ++k) {
            row_pos[static_cast<std::size_t>(rows0[k])] = static_cast<Index>(k);
        }

        // The predicted activity, initialized to the FROZEN one and mutated by the
        // fix-relax loop.
        std::vector<BoundState> bound_state = warm.qp_working_set.bound_state();
        std::vector<char> row_active(static_cast<std::size_t>(mi), 0);
        for (Index row : rows0) {
            row_active[static_cast<std::size_t>(row)] = 1;
        }
        // Released pins: their stationarity row carries the -z correction that
        // drives the released multiplier to zero (see the RELAX note).
        // Under the ratio test a pin is released exactly WHERE its multiplier
        // reaches zero, so this correction is normally identically zero and
        // the vector exists to keep a first-round release bit-for-bit what a
        // full-step release was, and to absorb any roundoff at the release
        // point.
        std::vector<char> bound_released(static_cast<std::size_t>(n), 0);
        // Each entity changes status at most once -- the loop's termination proof.
        std::vector<char> var_touched(static_cast<std::size_t>(n), 0);
        std::vector<char> row_touched(static_cast<std::size_t>(mi), 0);

        // ---- THE PATH STATE -----------------------------------------------
        //
        // `t` is the fraction of dp already traversed; everything below with a
        // `_cur` suffix is the iterate AT p + t*dp, and everything with a `d_`
        // prefix that is read off a solve is a RATE, per unit t. t == 0 and one
        // solve straight to t == 1 is the plain frozen-set step.
        double t = 0.0;
        Vec x_cur = x;
        Vec lambda_e_cur = warm.lambda_e;
        Vec lambda_i_cur = warm.lambda_i;
        // A FREE variable carries no bound multiplier, whatever the warm start
        // happens to store for it.
        Vec z_cur = Vec::Zero(n);
        for (Index i = 0; i < n; ++i) {
            if (bound_state[static_cast<std::size_t>(i)] != BoundState::kFree) {
                z_cur(i) = warm.z(i);
            }
        }
        // The LINEARIZED cI at (x_cur, p + t*dp) -- the only cI this file ever
        // has, since it never re-evaluates the model along the path (that would
        // be a corrector, not a predictor; see THE ONE EXTRA MODEL EVALUATION
        // step above).
        Vec ci_cur = base.ci;

        Vec dx = Vec::Zero(n);

        detail::KktFactor kkt;
        detail::factorize_checked(kkt, k0.K); // THE one factorization
        SchurComplement schur(kkt, qopts);
        std::vector<predictor_detail::PredictorBorder> borders;

        // add_border FIRST, then record: the ledger below must never claim a
        // border the SchurComplement does not have (add_border can throw --
        // an ill-conditioned C -- and this function's degradation path returns
        // the identity prediction rather than unwinding the two by hand).
        const auto add_pin = [&](Index i) {
            schur.add_border(BorderOps::pin_variable(i, k0_rows), -qopts.dual_mu);
            borders.push_back({predictor_detail::PredictorBorder::Kind::kPin, i});
        };
        const auto drop_pin = [&](Index i) {
            for (std::size_t b = 0; b < borders.size(); ++b) {
                if (borders[b].kind == predictor_detail::PredictorBorder::Kind::kPin &&
                    borders[b].target == i) {
                    schur.drop_border(static_cast<Index>(b));
                    borders.erase(borders.begin() + static_cast<std::ptrdiff_t>(b));
                    return;
                }
            }
            // UNREACHABLE: every non-free variable is pinned before the loop
            // starts, a pin is dropped only for a variable whose bound_state
            // still says pinned, and var_touched lets each variable change
            // status once. Stated as a throw anyway rather than as an assert a
            // Release build compiles out; the degradation handler catches it,
            // so the worst it can do is report kDegraded and hand back the
            // caller's own warm start.
            throw std::logic_error(
                fmt::format("predict: no pin border to release for variable {}", i));
        };

        // The frozen pins, as borders over K0 (full mode pins by bordering,
        // never by elimination -- kkt_assembly.h's assemble_kkt_full note).
        for (Index i = 0; i < n; ++i) {
            if (bound_state[static_cast<std::size_t>(i)] != BoundState::kFree) {
                add_pin(i);
            }
        }

        // ONE border's right-hand-side entry, at the current path position.
        //
        // EVERY CASE IS THE SAME SENTENCE: "the rate, per unit t, that lands
        // this border's own quantity where it must be AT t = 1, starting from
        // where it is NOW" -- hence the division by the interval still to be
        // traversed. At a breakpoint the quantity is already exactly where it
        // belongs (that is what the ratio test computed), so the leading term
        // vanishes and what is left is the pure sensitivity rate.
        //
        // The pin case is where this header's TWO RIGHT-HAND-SIDE CONVENTIONS
        // decision lives: it carries the base displacement, so a variable
        // that is supposed to be ON its bound is put there rather than
        // displaced from wherever a stale warm start left it.
        const auto border_rhs = [&](const predictor_detail::PredictorBorder &bd, double remaining) {
            switch (bd.kind) {
            case predictor_detail::PredictorBorder::Kind::kPin:
                return (bound_at_frac(bd.target, bound_state[static_cast<std::size_t>(bd.target)],
                                      1.0) -
                        x_cur(bd.target)) /
                       remaining;
            case predictor_detail::PredictorBorder::Kind::kRowDrop:
                return -lambda_i_cur(bd.target) / remaining;
            case predictor_detail::PredictorBorder::Kind::kRowAdd:
                break;
            }
            return -(ci_cur(bd.target) / remaining + d_ci(bd.target));
        };

        // Each entity toggles at most once, so the loop cannot run longer than
        // this however generous PredictorOptions::max_activity_rounds is; the
        // option is the budget and this is the proof. See that option's comment.
        const Index max_rounds =
            std::min<Index>(opts.max_activity_rounds, n + mi + 1); // each entity toggles once
        for (Index round = 0;; ++round) {
            // --- right-hand side, rebuilt each round (the relax correction
            //     and the border list both change with the activity) ---
            const double remaining = 1.0 - t;
            // NOTHING LEFT TO TRAVERSE. This is checked BEFORE the right-hand
            // side is built, not after the solve, because every border's rhs
            // DIVIDES by `remaining` (see border_rhs) and a zero divisor here
            // would put an infinity into the system rather than merely wasting
            // a solve. It cannot fire on round 0 -- t is 0 there, so the raw
            // frozen-set step always gets its one solve -- and past that it is
            // the exit taken when the previous round's breakpoint landed
            // exactly on p + dp. See predictor_detail::kMinRemaining.
            if (remaining <= predictor_detail::kMinRemaining) {
                break;
            }
            const Index m = static_cast<Index>(borders.size());
            Vec rhs = Vec::Zero(k0_rows + m);
            rhs.head(n) = -d_grad_lagrangian;
            for (Index i = 0; i < n; ++i) {
                if (bound_released[static_cast<std::size_t>(i)] != 0) {
                    rhs(i) -= z_cur(i) / remaining;
                }
            }
            for (Index r = 0; r < me; ++r) {
                rhs(n + r) = -d_ce(r);
            }
            for (std::size_t k = 0; k < rows0.size(); ++k) {
                rhs(n + me + static_cast<Index>(k)) = -d_ci(rows0[k]);
            }
            for (Index b = 0; b < m; ++b) {
                rhs(k0_rows + b) = border_rhs(borders[static_cast<std::size_t>(b)], remaining);
            }

            const Vec sol = schur.solve(rhs);

            // --- read the DIRECTION (per unit t) off the solution ---------
            dx = sol.head(n);
            Vec d_lambda_e = Vec::Zero(me);
            for (Index r = 0; r < me; ++r) {
                d_lambda_e(r) = sol(n + r);
            }
            Vec d_lambda_i = Vec::Zero(mi);
            for (std::size_t k = 0; k < rows0.size(); ++k) {
                // A DROPPED row's rate is forced to -lambda_j/remaining by its
                // delete_k0_row border's right-hand side, so this same line
                // lands its multiplier at exactly 0 at the end of the interval
                // -- and at a ratio-tested drop lambda_j is already 0, so the
                // rate is 0 and it STAYS there.
                d_lambda_i(rows0[k]) = sol(n + me + static_cast<Index>(k));
            }
            Vec d_z = Vec::Zero(n);
            for (Index i = 0; i < n; ++i) {
                if (bound_released[static_cast<std::size_t>(i)] != 0) {
                    d_z(i) = -z_cur(i) / remaining; // the rhs correction, as a rate
                }
            }
            for (Index b = 0; b < m; ++b) {
                const predictor_detail::PredictorBorder &bd = borders[static_cast<std::size_t>(b)];
                const double y = sol(k0_rows + b);
                switch (bd.kind) {
                case predictor_detail::PredictorBorder::Kind::kPin:
                    // The pin multiplier enters stationarity as +y where the
                    // model's own convention has -z, so dz = -y. A newly
                    // pinned variable has z_cur == 0 and the same line gives
                    // its whole multiplier rate.
                    d_z(bd.target) = -y;
                    break;
                case predictor_detail::PredictorBorder::Kind::kRowAdd:
                    d_lambda_i(bd.target) = y;
                    break;
                case predictor_detail::PredictorBorder::Kind::kRowDrop:
                    break; // handled through the K0 row's own rate above
                }
            }
            // cI's rate along the path, needed by the ADD trigger and its ratio
            // test. mi == 0 (F3, and every bound-only QP) skips the matvec.
            Vec d_ci_path = Vec::Zero(mi);
            if (mi > 0) {
                d_ci_path = qp.Ai * dx + d_ci;
            }

            // Advances the path by `tau` of the remaining interval and stops
            // the loop; `tau == remaining` is the ordinary "nothing else
            // happens between here and p + dp" exit.
            const auto advance = [&](double tau) {
                t += tau;
                x_cur += tau * dx;
                lambda_e_cur += tau * d_lambda_e;
                lambda_i_cur += tau * d_lambda_i;
                z_cur += tau * d_z;
                if (mi > 0) {
                    ci_cur += tau * d_ci_path;
                }
            };
            // Takes the WHOLE remainder and lands t on EXACTLY 1.0.
            //
            // The assignment is DEFENSIVE: `t + (1.0 - t)` is not bit-exactly 1
            // for every t a breakpoint can leave behind, while
            // `reached_t == 1.0` is a contract callers and tests read as "not
            // truncated" and out.x's final clamp evaluates the bounds AT t, so
            // a last-ulp shortfall would move a clamped component off the
            // bound at p + dp. Assigning the value the path mathematically
            // reached costs nothing and removes both questions.
            const auto advance_to_end = [&]() {
                advance(remaining);
                t = 1.0;
            };

            if (!opts.allow_activity_change) {
                advance_to_end(); // THE RAW FROZEN-SET STEP: t straight to 1
                break;
            }

            // --- FIX / RELAX / DROP / ADD: WHICH, AND WHERE ---------------
            //
            // Each entity's TRIGGER test is evaluated at the value its quantity
            // would take at t = 1 -- the plain full-step test, unchanged.
            // What the ratio test adds is that a triggered entity then reports
            // WHERE on [t, 1] its own quantity crosses, and only the earliest
            // crossings are applied.
            std::vector<predictor_detail::PredictorBreakpoint> hits;
            const auto crossing = [remaining](double value_now, double rate) {
                // value_now >= 0 is the condition being violated; rate < 0 is
                // what violates it. Clamped into [0, remaining] because a
                // warm start that is already (slightly) on the wrong side
                // reports value_now < 0, and a crossing is never past the end
                // of the interval the trigger test just found it inside.
                if (!(rate < 0.0)) {
                    return remaining;
                }
                return std::clamp(value_now / -rate, 0.0, remaining);
            };
            for (Index i = 0; i < n; ++i) {
                const auto k = static_cast<std::size_t>(i);
                if (var_touched[k] != 0) {
                    continue;
                }
                const BoundState state = bound_state[k];
                if (state == BoundState::kFixed) {
                    continue; // lower == upper: no sign condition to violate
                }
                if (state == BoundState::kFree) {
                    const double xi = x_cur(i) + remaining * dx(i);
                    const double lo = bound_at_frac(i, BoundState::kAtLower, 1.0);
                    const double up = bound_at_frac(i, BoundState::kAtUpper, 1.0);
                    if (std::isfinite(lo) &&
                        xi < lo - predictor_detail::kGeomTol * std::max(1.0, std::abs(lo))) {
                        hits.push_back(
                            {predictor_detail::PredictorBreakpoint::Kind::kFix, i,
                             BoundState::kAtLower,
                             crossing(x_cur(i) - bound_at_frac(i, BoundState::kAtLower, t),
                                      dx(i) - d_lower(i))});
                    } else if (std::isfinite(up) &&
                               xi > up + predictor_detail::kGeomTol * std::max(1.0, std::abs(up))) {
                        hits.push_back(
                            {predictor_detail::PredictorBreakpoint::Kind::kFix, i,
                             BoundState::kAtUpper,
                             crossing(bound_at_frac(i, BoundState::kAtUpper, t) - x_cur(i),
                                      d_upper(i) - dx(i))});
                    }
                } else {
                    const double tol =
                        predictor_detail::kDualSignTol * std::max(1.0, std::abs(warm.z(i)));
                    const double z_end = z_cur(i) + remaining * d_z(i);
                    // z >= 0 is required at a LOWER bound and z <= 0 at an
                    // UPPER one, so the quantity that must stay nonnegative is
                    // z or -z respectively; one `crossing` call covers both.
                    const double sign = state == BoundState::kAtLower ? 1.0 : -1.0;
                    if (sign * z_end < -tol) {
                        hits.push_back({predictor_detail::PredictorBreakpoint::Kind::kRelax, i,
                                        state, crossing(sign * z_cur(i), sign * d_z(i))});
                    }
                }
            }
            for (Index j = 0; j < mi; ++j) {
                const auto k = static_cast<std::size_t>(j);
                if (row_touched[k] != 0) {
                    continue;
                }
                if (row_active[k] != 0) {
                    if (row_pos[k] < 0) {
                        continue; // added this call; its multiplier is fresh
                    }
                    const double tol =
                        predictor_detail::kDualSignTol * std::max(1.0, std::abs(warm.lambda_i(j)));
                    if (lambda_i_cur(j) + remaining * d_lambda_i(j) >= -tol) {
                        continue; // includes the WEAKLY ACTIVE case -- kept
                    }
                    hits.push_back({predictor_detail::PredictorBreakpoint::Kind::kDrop, j,
                                    BoundState::kFree, crossing(lambda_i_cur(j), d_lambda_i(j))});
                } else {
                    if (ci_cur(j) + remaining * d_ci_path(j) <=
                        predictor_detail::kGeomTol * std::max(1.0, std::abs(base.ci(j)))) {
                        continue;
                    }
                    hits.push_back({predictor_detail::PredictorBreakpoint::Kind::kAdd, j,
                                    BoundState::kFree, crossing(-ci_cur(j), -d_ci_path(j))});
                }
            }

            if (hits.empty()) {
                advance_to_end(); // the active set holds all the way to p + dp
                break;
            }

            // --- ADVANCE TO THE FIRST CROSSING, AND CHANGE ONLY IT --------
            double tau_star = remaining;
            for (const auto &hit : hits) {
                tau_star = std::min(tau_star, hit.tau);
            }
            // THE BOUNDARY CASE, and why it is not lumped in with the rest:
            // `crossing` returns `remaining` for a triggered entity whose own
            // rate does not actually carry it across inside the interval (the
            // already-on-the-wrong-side-but-moving-back case). If EVERY hit is
            // like that, the first "crossing" is the end of the step, and the
            // path is not truncated at all -- it reached t = 1 with a slightly
            // under-populated working set. Saying so exactly is what keeps
            // `reached_t == 1.0` meaning "not cut short"; accumulating
            // t + remaining here would report ~1.0 for a full path and leave a
            // caller unable to tell it from a truncation at 0.9999999999.
            if (tau_star >= remaining) {
                advance_to_end();
            } else {
                advance(tau_star);
            }
            if (round >= max_rounds) {
                // TRUNCATION -- see PredictorOptions::max_activity_rounds, and
                // predict()'s WHAT `reached_t` MEANS note for how a caller sees
                // it. `round` is the number of breakpoints already APPLIED, so
                // the path stops ON this crossing with its status unchanged: a
                // point that is exactly on the path and exactly on the
                // constraint it just reached, which is the conservative place
                // to stop. Applying the change and stopping would leave the
                // last leg's direction unspent instead. (`t` here is 1.0 in the
                // boundary case just above, and strictly below 1 otherwise --
                // so this break is not by itself a report of truncation, and
                // `reached_t` is.)
                break;
            }
            const double tie = tau_star + predictor_detail::kTauTieTol * remaining;
            for (const auto &hit : hits) {
                if (hit.tau > tie) {
                    continue; // a later breakpoint; re-tested on the next round
                }
                const auto k = static_cast<std::size_t>(hit.target);
                switch (hit.kind) {
                case predictor_detail::PredictorBreakpoint::Kind::kFix:
                    bound_state[k] = hit.state;
                    // The variable is ON its bound here -- that is what tau
                    // solved for -- so SAY so exactly, and the pin border's
                    // right-hand side reduces to the pure sensitivity rate
                    // instead of re-correcting a roundoff-sized displacement.
                    x_cur(hit.target) = bound_at_frac(hit.target, hit.state, t);
                    add_pin(hit.target);
                    var_touched[k] = 1;
                    break;
                case predictor_detail::PredictorBreakpoint::Kind::kRelax:
                    drop_pin(hit.target);
                    bound_state[k] = BoundState::kFree;
                    bound_released[k] = 1;
                    var_touched[k] = 1;
                    break;
                case predictor_detail::PredictorBreakpoint::Kind::kDrop:
                    schur.add_border(BorderOps::delete_k0_row(me + row_pos[k], me, n, k0_rows),
                                     0.0);
                    borders.push_back(
                        {predictor_detail::PredictorBorder::Kind::kRowDrop, hit.target});
                    row_active[k] = 0;
                    row_touched[k] = 1;
                    break;
                case predictor_detail::PredictorBreakpoint::Kind::kAdd:
                    schur.add_border(BorderOps::add_ineq_row(qp, hit.target, k0_rows),
                                     -qopts.dual_mu);
                    borders.push_back(
                        {predictor_detail::PredictorBorder::Kind::kRowAdd, hit.target});
                    row_active[k] = 1;
                    row_touched[k] = 1;
                    break;
                }
            }
        }
        // ---- THE PREDICTED WARM START ------------------------------------
        WarmStart out = identity;
        out.x = x_cur;
        // A clamped variable sits EXACTLY on its bound: the border's -dual_mu
        // diagonal leaves the pin equation satisfied only to O(dual_mu * y), and a
        // warm start whose "active" variable is 1e-16 off its bound is a warm start
        // whose activity the next solve has to re-derive. Assigning the bound value
        // is free and removes the question.
        //
        // AT THE FRACTION THE PATH ACTUALLY REACHED, which is t == 1 on every
        // untruncated prediction but is the honest answer when the round
        // budget stopped the path short: out.x is then a point at p + t*dp,
        // and pinning its clamped variables to the bound at p + dp would put
        // them somewhere the rest of the vector is not.
        for (Index i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            switch (bound_state[k]) {
            case BoundState::kAtLower:
            case BoundState::kFixed:
                out.x(i) = bound_at_frac(i, BoundState::kAtLower, t);
                break;
            case BoundState::kAtUpper:
                out.x(i) = bound_at_frac(i, BoundState::kAtUpper, t);
                break;
            case BoundState::kFree:
                break;
            }
        }
        out.lambda_e = lambda_e_cur;
        out.lambda_i = lambda_i_cur;
        out.z = z_cur;

        out.qp_working_set = WorkingSet(n, mi);
        out.qp_working_set.bound_state() = bound_state;
        out.bound_active.assign(static_cast<std::size_t>(n), 0);
        for (Index i = 0; i < n; ++i) {
            const auto k = static_cast<std::size_t>(i);
            switch (bound_state[k]) {
            case BoundState::kAtLower:
                out.bound_active[k] = -1;
                break;
            case BoundState::kAtUpper:
            case BoundState::kFixed: // warm_start.h's bound_active note
                out.bound_active[k] = +1;
                break;
            case BoundState::kFree:
                break;
            }
        }
        out.ineq_active.assign(static_cast<std::size_t>(mi), 0);
        for (Index j = 0; j < mi; ++j) {
            if (row_active[static_cast<std::size_t>(j)] != 0) {
                out.qp_working_set.add_ineq(j);
                out.ineq_active[static_cast<std::size_t>(j)] = 1;
                // THE EMITTED PRICE IS NEVER NEGATIVE. `lambda_i >= 0` is a
                // WarmStart PRECONDITION (warm_start.h's SIGN CONVENTIONS),
                // gated in the driver at kSeeded only -- and this producer's
                // output reaches solve() at kWarm/kHot (predict() carries
                // structure_hash forward), where it is not gated at all. So
                // the object this function emits has to honour the convention
                // itself.
                //
                // The DROP ratio test above keeps a row whose end-of-segment
                // multiplier is >= -kDualSignTol * max(1, |warm.lambda_i(j)|).
                // That factor is RELATIVE, so the retained value is unbounded
                // in ABSOLUTE terms -- a large frozen price admits a materially
                // negative retained one. Clamping to zero costs nothing that
                // the ratio test meant to keep: that branch's point is "this
                // multiplier is zero to within noise, do not spend a
                // breakpoint on it".
                //
                // THE CLAMP IS UNCONDITIONAL ON THE SIGN, not restricted to
                // the noise band, because the ratio test is not the only path
                // here: the FROZEN-SET step (`!opts.allow_activity_change`)
                // and a ROUND-BUDGET TRUNCATION both reach this loop without
                // having run it, and a raw frozen step can carry any negative
                // value at all. Zeroing is the conservative direction in every
                // case -- it can only make the emitted point less stationary,
                // which costs the next solve majors, where retaining the value
                // can cost it the ANSWER.
                if (out.lambda_i(j) < 0.0) {
                    out.lambda_i(j) = 0.0;
                }
            } else {
                // An inactive row carries no multiplier: leaving a dropped row's
                // frozen lambda in place would hand the next solve a multiplier
                // its own activity guess says should not exist.
                out.lambda_i(j) = 0.0;
            }
        }
        out.valid = true;
        report(PredictorOutcome::kPredicted);
        // The ONLY overwrite of the 0.0 set before the try block. t == 1.0 is a
        // full prediction; anything less is the round budget having stopped the
        // path, and t == 0.0 here says the returned object IS the identity even
        // though a step was computed -- see predict()'s WHAT `reached_t` MEANS.
        report_t(t);
        return out;
    } catch (const std::exception &) {
        // EVERY failure after validation, whatever type it reports through --
        // see this function's VALIDATE-THEN-CATCH-EVERYTHING note for what is
        // inside this net and why the earlier type-based split was wrong. The
        // caller gets its own warm start back, unmodified, and `outcome` says
        // so; ParameterRestorer (still in scope during the unwind) has already
        // put the model back at its entry parameters.
        report(PredictorOutcome::kDegraded);
        return identity;
    }
}

} // namespace hven::solvers
