// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// continuation.h — THE CONTINUATION DRIVER: the sweep loop that follows a
// parametric NLP's solution path from p0 to p1 by paying ONE cold solve at
// p0 and then only warm ones.
//
// This is the workflow the whole warm-start subsystem exists for. Nothing
// here is a new algorithm: it is a COMPOSITION LAYER over WarmStart + the
// 3-arg solve (the hand-off between consecutive parameter values), the `hot`
// factorization handle (reachable only when the caller raises
// SqpOptions::start_level to kHot; the predictor drops `hot` by contract, so
// a PREDICTED sweep is kWarm by construction), SqpOptions::warm_full_step
// (which stops the funnel from re-globalizing a warm start from scratch, and
// engages automatically on every step past the first here), SqpOptions::
// budget_mode (see BUDGET EXHAUSTION below), and predict() -- the tangential
// predictor that turns the warm start at p into a first-order-accurate one
// at p + dp.
//
// ---------------------------------------------------------------------
// THE LOOP
//
//   1. Solve COLD at p0. If that fails there is no path to follow: return
//      immediately with reached_p1 == false. A kBudgetExhausted there is not a
//      failure but a continue, under the SAME rule and cap every later
//      parameter value gets -- see BUDGET EXHAUSTION below.
//   2. Repeat: propose p_next = p_cur + dp * direction, CLAMPED at p1
//      (direction is the unit vector along p1 - p0, dp an arc length along
//      that segment, so a multi-parameter sweep moves along the SEGMENT and
//      not one coordinate at a time). Build the seed -- predict() applied to
//      the last good warm start when use_predictor, the raw last good warm
//      start otherwise -- re-pose the model at p_next and solve warm.
//        - kOptimal: accept. p_cur := p_next, the warm start advances, and dp
//          GROWS by `grow` (capped at dp_max) if the step cost <= target_majors.
//        - kBudgetExhausted: see BUDGET EXHAUSTION below -- neither.
//        - anything else: reject. dp SHRINKS by `shrink` and the step is
//          retried from the LAST GOOD warm start, at the last good p. Below
//          dp_min the run fails.
//   3. Landing exactly on p1 ends the sweep with reached_p1 == true.
//
// WHY THE RETRY GOES BACK TO THE LAST GOOD WARM START rather than reusing the
// failed solve's own exit state: warm_start.h guarantees the failed object is
// SAFE to feed forward, not that it is BETTER than what preceded it. A solve
// that ran out of trust region or stalled is, by construction, somewhere the
// method could not make progress. The last CONVERGED point is a genuine KKT
// point of a nearby problem, which is the only thing this layer has any
// theory about.
//
// THE SHRINK IS APPLIED TO THE STEP THAT ACTUALLY FAILED, NOT TO THE
// CONTROLLER'S dp. The proposal is CLAMPED to the endpoint, so the step
// actually attempted is step_dp = min(dp, total - s_cur), which near p1 can
// be far shorter than dp. Shrinking dp itself would leave the CLAMPED
// proposal unchanged there, repeating the identical failing solve once per
// halving until dp decayed below the remainder -- a cost of
// ceil(log(step_dp/dp) / log(shrink)) wasted full solves, unbounded as the
// remainder shrinks relative to dp. Setting dp = shrink * step_dp instead is
// identical wherever no clamp was in effect (step_dp == dp) and strictly
// stronger where one was: the retry is always shorter than the attempt that
// just failed.
//
// TERMINATION IS GUARANTEED, and it is worth stating because the loop has no
// iteration cap. Every ACCEPTED step advances the arc position by at least
// dp_min (dp is never used below dp_min -- the run fails first), so there are
// at most ||p1 - p0|| / dp_min acceptances. Between two acceptances dp only
// SHRINKS, geometrically, from at most dp_max to below dp_min, which is a
// bounded number of rejections; and a budget-exhaustion chain at one proposal
// is capped explicitly (kBudgetContinuationsMax). No arbitrary max-steps knob
// is therefore introduced, because none is needed to bound the loop.
//
// ---------------------------------------------------------------------
// BUDGET EXHAUSTION IS A CONTINUE, NOT A SHRINK -- the one status this loop
// treats as neither success nor failure.
//
// When the caller's driver runs with SqpOptions::budget_mode, a solve that
// exhausts max_iter reports kBudgetExhausted and returns THE BEST ITERATE IT
// VISITED BY THE FUNNEL'S OWN ORDERING (feasibility first, min f as the
// tie-break) rather than the last one reached -- a hand-off designed for this
// loop. The honest reading of it is "the step was too expensive for one
// budget slice", not "the step was too long".
//
// This loop therefore RE-SOLVES AT THE SAME p_next, seeded by the returned
// warm start, rather than shrinking dp and moving p_next back. Shrinking
// would be wrong twice over: it discards progress the solve genuinely made,
// and it treats a BUDGET decision -- which the caller set, and can raise --
// as evidence about the PROBLEM's geometry, which is what dp is supposed to
// track.
//
// IT IS CAPPED, because "continue at the same p" is exactly the shape an
// infinite loop takes. kBudgetContinuationsMax consecutive exhaustions at one
// proposal demote it to an ordinary failure -- dp shrinks, the retry goes back
// to the last good warm start -- on the argument that a proposal which has
// consumed several full budgets without converging is no longer merely
// expensive. The cap resets whenever the proposal changes.
//
// THE COLD STEP AT p0 GETS THE SAME RULE AND THE SAME CAP. The only
// difference is what the cap can do there: at p0 the proposal is the caller's
// own endpoint, so there is no dp to shrink and the demotion simply ends the
// sweep.
//
// THE STEP CONTROLLER IS TOLD ABOUT THE COST. dp grows only when the TOTAL
// majors spent arriving at a proposal -- summed over its budget continuations,
// not just the last slice -- is within target_majors. A proposal that took
// three budgets is not a cheap step and must not lengthen the next one.
//
// With budget_mode OFF (the default) none of this is reachable:
// sqp_types.h guarantees kBudgetExhausted is NEVER reported then, and every
// max_iter exhaustion arrives as kMaxIter, which is an ordinary failure here.
//
// ---------------------------------------------------------------------
// RETRY ECONOMICS -- what a REJECTED proposal costs, a different question
// from which proposals are made, and on large sweeps the dominant one: failed
// proposals have been measured to account for the large majority of a sweep's
// QP minor iterations when each is paid at full price. Two rules address it,
// and both are INVISIBLE ON A SWEEP THAT NEVER FAILS -- a requirement rather
// than a remark:
//
//   (1) ContinuationOptions::probe_budget: the proposal's FIRST solve gets a
//       minor-iteration budget scaled from what the last CONVERGED step cost
//       (floored by continuation_detail::kProbeBudgetFloor); a solve still
//       running when the budget is spent is abandoned
//       (SqpCounters::probe_budget_stops, and
//       ContinuationResult::proposals_abandoned here). A COST rule, not a
//       step-length rule: it never touches the dp arithmetic. What abandonment
//       cannot do: lose an answer (the convergence test runs before the budget
//       test on the same major, so an over-budget solve AT a KKT point is kept
//       as kOptimal); make a failure free (the check is between majors, so the
//       major that crossed the budget was paid); or fail a sweep that would
//       otherwise have succeeded except in the honest sense that a shorter
//       step is then attempted instead.
//   (2) ContinuationOptions::suspend_growth_after_failure: the first accepted
//       step after a failure does not lengthen the next proposal. This one IS
//       a step-length rule and changes which parameter values are visited --
//       without it, the growth rule measurably re-proposes the length that
//       just failed.
//
// ---------------------------------------------------------------------
// THE PREDICTOR IS ALWAYS ASKED FOR ITS OUTCOME. predict() takes an optional
// PredictorOutcome out-parameter, and this loop passes a non-null one on every
// call: a prediction that could not be computed DEGRADES to the identity and
// reports kDegraded rather than throwing, and a sweep that passed nullptr
// would be indistinguishable from a healthy one except by being slower. So:
//
//   - ContinuationStep::predictor_outcome records what predict() actually did,
//     per step, and is EMPTY only where predict() was not called at all (the
//     cold first step, a budget continuation, use_predictor == false).
//   - ContinuationStep::predictor_used is true ONLY for kPredicted. A
//     kDegraded or kZeroStep step took the identity, and reporting that as a
//     prediction taken would assert something false.
//   - ContinuationResult::predictor_degradations aggregates the kDegraded
//     count, which is the number a caller should actually look at.
//
// A degraded prediction is never fatal: the identity IS the raw warm start, so
// the step simply proceeds unpredicted.
//
// ---------------------------------------------------------------------
// dp IS AN ARC LENGTH AND direction IS THE UNIT VECTOR ALONG p1 - p0, so for a
// multi-parameter model the proposal is p_cur + dp * (p1 - p0)/||p1 - p0||.
// The vector handed to predict() is the parameter-space step EXACTLY --
// p_next - p_cur -- which is what predict()'s own directional finite
// difference requires to describe the move actually being made. The final
// proposal is set to p1 ITSELF rather than to p0 + total*direction, so the
// sweep lands on the caller's endpoint bit-for-bit instead of a rounded
// reconstruction of it.
//
// ---------------------------------------------------------------------
// WHAT THIS HEADER DOES NOT HAVE, AND WHY:
//
//   - NO `SqpOptions solve_options` FIELD ON ContinuationOptions. SqpDriver
//     copies its SqpOptions AT CONSTRUCTION and exposes neither a setter nor
//     a getter, so a driver passed in BY REFERENCE cannot be re-configured
//     per sweep; such a field could only ever have been inert. The per-step
//     solver configuration is the DRIVER's, set by the caller when
//     constructing it.
//   - NO PredictorOptions FIELD. predict() is called with its defaults. This
//     is a genuine (small) limitation rather than an impossibility -- unlike
//     solve_options, such a field WOULD work -- and it is left out only
//     because no caller needs to retune fd_step_scale mid-sweep. Adding it
//     later is source-compatible.
//   - NO ITERATION CAP. See TERMINATION IS GUARANTEED above.

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <hven/detail/warmstart/predictor.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

// How the sweep proposes and re-sizes its parameter steps. Every field is a
// CALLER-VISIBLE POLICY: none of them changes what a converged step means,
// only which parameter values are visited and in what order.
//
// PER-STEP SOLVER CONFIGURATION IS THE SqpDriver's, not this struct's -- see
// this header's WHAT THIS HEADER DOES NOT HAVE note.
struct ContinuationOptions {
    // The first proposal's arc length along the p0 -> p1 segment. Must be
    // finite, > 0, and within [dp_min, dp_max].
    double dp_init = 0.1;
    // The step-length FLOOR, and the run's own failure criterion: a shrink
    // that would take dp below this value fails the sweep instead (a RETURN
    // with reached_p1 == false, never a throw). Must be > 0.
    double dp_min = 1e-4;
    // The step-length CEILING the growth rule expands toward. Must be
    // >= dp_min.
    double dp_max = 0.5;
    // Grow dp after a step that converged within this many majors -- counting
    // the WHOLE cost of arriving at that proposal, including any budget
    // continuations (this header's BUDGET EXHAUSTION note). Must be >= 0; 0
    // means "only an immediate convergence is cheap enough to lengthen the
    // next step", which is legal and simply makes growth rare.
    int target_majors = 5;
    // Multiplier applied to dp after a cheap step, capped at dp_max. Must be
    // finite and >= 1 (a "growth" rule that shrank would fight the shrink rule
    // below and could stall the sweep at dp_min without ever failing it).
    double grow = 2.0;
    // Multiplier applied to dp after a failed step. Must be in (0, 1).
    double shrink = 0.5;
    // When true (the default) each step's seed is predict() applied to the
    // last good warm start; when false the raw last good warm start is fed
    // forward unchanged. This is the predictor's own A/B lever, in the same
    // spirit as SqpOptions::enable_soc / adaptive_mu / warm_full_step: with it
    // off the sweep still works, it is simply slower, and the difference is
    // what tests/test_continuation.cpp's PredictorOffCostsMore measures.
    bool use_predictor = true;

    // THE PROBE BUDGET, in multiples of the LAST CONVERGED step's own QP
    // minor-iteration cost -- see this header's RETRY ECONOMICS note for what
    // it is for; only the knob is described here.
    //
    // Each proposal's FIRST solve is given a minor budget of
    // probe_budget * (minors the last converged step cost). A solve that
    // reaches a major having spent that many minors without converging is
    // ABANDONED -- the driver returns kMaxIter with
    // SqpCounters::probe_budget_stops == 1 -- and this loop treats that
    // exactly as any other failed proposal: shrink, retry from the last good
    // warm start. It is a FAILED-PROPOSAL DETECTOR rather than a step-length
    // rule -- it touches no part of the dp arithmetic -- though a FALSE
    // abandonment does move the proposal sequence, by the ordinary shrink
    // path.
    //
    // THE BUDGET IS NEVER SMALLER THAN continuation_detail::
    // kProbeBudgetFloor, whatever this multiplier and the last step's cost
    // work out to -- on every corpus measured so far it is that floor, not
    // this ratio, which sets the budget in practice. The default of 2 is not
    // a tuned value but the more conservative of two measured-equal settings,
    // kept because a pure constant cannot scale: a corpus whose converged
    // steps cost thousands of minors needs the ratio. Anyone tempted to tune
    // it should first find a corpus where it binds.
    //
    // 0 DISABLES THE PROBE BUDGET (a negative value is rejected by
    // validate()), and disabling it recovers the pre-probe-budget loop only
    // TOGETHER WITH suspend_growth_after_failure = false: that lever is a
    // step-length rule and changes which parameter values are visited on any
    // sweep that fails, so probe_budget = 0 ALONE is not the old controller.
    //
    // MUST BE >= 0.
    int probe_budget = 2;

    // THE FAILURE-HISTORY TERM, the one place a failure is allowed to inform
    // the NEXT proposal's length rather than only this one's. When true (the
    // default), the growth rule is SKIPPED for the first accepted step after
    // a failed proposal: dp is left where the shrink put it for one more step
    // instead of immediately doubling back toward the length that just
    // failed. This is the standard hysteresis of a rejection-aware step
    // controller ("do not increase the step immediately after a rejection"),
    // stated as a lever so it can be swept and turned off.
    //
    // A SWEEP WITH NO FAILED PROPOSAL CANNOT SEE THIS FIELD AT ALL, in either
    // setting: the suspension is armed only by a failure.
    bool suspend_growth_after_failure = true;
};

// ONE SOLVE ATTEMPT along the sweep -- successful or not. A failed attempt is
// recorded too: `status` would be a pointless field if it could only ever read
// kOptimal, and a caller diagnosing a sweep that stopped short needs to see
// WHERE it stopped and what the driver said there.
struct ContinuationStep {
    // The parameter value this attempt was solved AT (the proposal, not the
    // point it was proposed from).
    Vec p;
    // The point the solve returned -- SqpSolution::x. On a successful step
    // this is x*(p) to the driver's own tolerance, so the sequence of these
    // IS the computed solution path, which is the sweep's actual product.
    Vec x;
    // The arc length that produced this proposal, MEASURED FROM THE LAST
    // CONVERGED POINT -- so p == p_last_good + dp * direction exactly, which
    // is the outward-visible form of "a retry is proposed from the last GOOD
    // warm start". It is NOT the distance from the previously RECORDED step,
    // which after a failure is the point that failed. 0 on the cold first step
    // (there was no proposal); on a BUDGET CONTINUATION it repeats the
    // preceding attempt's value unchanged, because the proposal did not move
    // and the invariant above still has to hold.
    //
    // IT CAN BE SHORTER THAN THE dp THE CONTROLLER HOLDS, because the last
    // proposal before p1 is clamped to land exactly on p1. Two consecutive
    // attempts can therefore record the SAME dp while the controller's own dp
    // changed underneath them -- read the controller's value off the p
    // sequence, not off this field.
    double dp = 0.0;
    SqpStatus status = SqpStatus::kOptimal;
    // The driver's own counters for this one solve; ContinuationResult::
    // total_majors is the sum of counters.major_iters over every step.
    SqpCounters counters;
    // The warm-start level this solve RESOLVED to (SqpCounters::
    // start_level_used, i.e. what was observed to happen, not what was
    // offered). kCold on the first step by construction; a kCold on any later
    // step means the hand-off silently broke, which is invisible in the
    // answers and is the single most useful thing in this record.
    StartLevel level = StartLevel::kCold;
    // TRUE ONLY when predict() actually produced a sensitivity step
    // (PredictorOutcome::kPredicted) that was then fed to this solve. False on
    // the cold step, with use_predictor off, on a budget continuation, and --
    // the case worth naming -- when predict() DECLINED (kDegraded/kZeroStep)
    // and the identity was used instead.
    bool predictor_used = false;
    // What predict() reported, or EMPTY where it was not called at all. See
    // this header's THE PREDICTOR IS ALWAYS ASKED note for why this is
    // recorded per step rather than inferred from predictor_used.
    std::optional<PredictorOutcome> predictor_outcome;
};

// The whole sweep.
struct ContinuationResult {
    // Every attempt, in order. steps.front() is always the cold solve at p0
    // (a sweep always attempts at least that much).
    std::vector<ContinuationStep> steps;
    // True exactly when a step converged AT p1. False on every failure exit,
    // including the cold solve at p0 failing.
    bool reached_p1 = false;
    // The warm start of the LAST STEP THAT CONVERGED -- the natural resume
    // point, and the solution's own hand-off object when reached_p1. Its
    // parameter value is the last kOptimal step's `p`. If NO step converged
    // (the cold solve at p0 failed), this is that failed solve's own warm
    // start, which warm_start.h guarantees is still safe to feed forward.
    WarmStart final_warm;

    // Aggregate accounting over `steps`. Sums rather than new measurements:
    // total_majors is sum of counters.major_iters, predictor_calls counts the
    // predict() calls made, predictor_degradations the subset that reported
    // kDegraded. The last is the number this header's THE PREDICTOR IS ALWAYS
    // ASKED note argues a looping caller must actually look at.
    Index total_majors = 0;
    Index predictor_calls = 0;
    Index predictor_degradations = 0;

    // THE RETRY-COST SPLIT. Every FAILED attempt in `steps` falls in exactly
    // one of these two, so their sum is the number of failed attempts and
    // neither one ever counts a converged step. They are the pair a caller
    // reads to answer "what did this sweep pay for proposals it then threw
    // away, and how much of that did the probe budget catch".
    //
    // "EVERY FAILED ATTEMPT" INCLUDES THE COLD SOLVE AT p0, which is a step
    // record like any other and is classified by the same
    // probe_budget_stops test as the loop's own. A sweep whose p0 never
    // converged therefore reports one failure here, not zero.
    //
    //   proposals_abandoned   attempts cut short by the probe budget
    //                         (ContinuationOptions::probe_budget), i.e. whose
    //                         solve came back with
    //                         SqpCounters::probe_budget_stops == 1. The work
    //                         they cost is real and is in their step records
    //                         like any other -- "abandoned" means the sweep
    //                         stopped buying majors for them, not that they
    //                         were free.
    //   proposals_full_cost   attempts that failed on their own terms, having
    //                         been allowed to run to whatever exit the driver
    //                         chose (kNumericalError, kMaxIter, kInfeasible,
    //                         a demoted budget-exhaustion chain, ...). With
    //                         the probe budget off this is every failure, and
    //                         proposals_abandoned is identically 0.
    //
    // A BUDGET-EXHAUSTION CONTINUATION IS NOT A FAILURE and is in neither;
    // only the attempt that ends a chain by being DEMOTED counts, once, as a
    // full-cost failure.
    Index proposals_abandoned = 0;
    Index proposals_full_cost = 0;
};

namespace continuation_detail {

// Consecutive kBudgetExhausted solves allowed at ONE proposal before it is
// demoted to an ordinary failure (this header's BUDGET EXHAUSTION note). 3
// is an implementation choice, not a paper constant: small enough that a
// genuinely stuck proposal costs a bounded multiple of the caller's own
// max_iter, large enough that a step needing "a bit more than one budget"
// -- the case the whole continue rule exists for -- is not thrown away on
// its first slice. It is inert whenever SqpOptions::budget_mode is off,
// which is the default.
inline constexpr int kBudgetContinuationsMax = 3;

// THE EVIDENCE FLOOR under every probe budget, in QP minor iterations: no
// proposal is ever abandoned for having spent fewer than this many minors,
// however cheap the last converged step was.
//
// IT IS NOT A TUNING PARAMETER, IT IS A GUARD: a pure ratio has no meaning
// at a tiny baseline. On a healthy sweep whose steps converge in 2 minors,
// a floorless probe_budget = 2 budgets the third step at 4 minors and
// abandons it even though it converges in far more -- a single false
// abandonment that cascades into a shrink-and-retry pile (measured as a
// multi-x minor-count REGRESSION on healthy sweeps; deleting this floor
// fails a broad set of existing tests). The value 200 clears by ~1.8x the
// largest legitimately-cheap step observed on the binding control sweep at
// N = 2000 nodes, where a floor of 100 measurably moved a healthy-corpus
// cell. WHERE it binds is measured rather than assumed: on large sweeps the
// floor sets the budget on most steps -- the honest general statement is
// that on every corpus measured the floor is the operative term and
// ContinuationOptions::probe_budget is headroom above it.
inline constexpr Index kProbeBudgetFloor = 200;

// Validation of the caller's inputs, all of it BEFORE the model is touched,
// so a rejected call cannot leave the model re-posed (the same
// validate-first discipline predictor.h uses).
//
// The body is in src/warmstart/continuation.cpp, next to run_continuation's
// -- its only caller.
void validate(const ParametricNlpModel &model, const Vec &p0, const Vec &p1,
              const ContinuationOptions &opts);

} // namespace continuation_detail

// Sweep `model` from p0 to p1, re-solving at each proposal with `driver` and
// returning every attempt. See this header's THE LOOP note for the semantics
// and BUDGET EXHAUSTION for the one status that is neither success nor
// failure.
//
// THROWS std::invalid_argument (sizes always in the message) on endpoints
// whose dimensions disagree with each other or with model.parameter_dim(),
// on a non-finite endpoint, and on options that cannot be honoured.
// Validation runs BEFORE the model is touched.
//
// A SWEEP THAT CANNOT GET THROUGH IS A RETURN, NOT A THROW: reached_p1 is
// false, `steps` ends with the attempt that failed, and `final_warm` is the
// last good hand-off. That is the deliberate division -- a caller error is an
// exception, a numerical outcome is a value, and "this path could not be
// followed all the way" is a numerical outcome.
//
// EXCEPTIONS FROM THE MODEL OR THE DRIVER ARE NOT CAUGHT and propagate. This
// layer degrades a FAILED SOLVE, which is a status it can read and act on; it
// has no basis for turning "the model threw" into a step-length decision, and
// swallowing it would hide a defect behind a slow sweep. Note the one place
// where a model throw IS absorbed: predict()'s own probe evaluation, which
// predictor.h catches and reports as kDegraded -- counted here, never hidden.
//
// THE MODEL IS LEFT POSED AT THE LAST PARAMETER VALUE ATTEMPTED, which on a
// successful sweep is p1. This function does not restore p0.
//
// The body is in src/warmstart/continuation.cpp, together with
// continuation_detail::validate above: this function runs once per sweep,
// each of its steps is a whole SqpDriver::solve(), and it was never inlined.
// The algorithm, the budget rule and every "why" stay here.
ContinuationResult run_continuation(ParametricNlpModel &model, const Vec &p0, const Vec &p1,
                                    SqpDriver &driver, const ContinuationOptions &opts = {});

} // namespace hven::solvers
