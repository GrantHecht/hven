#pragma once

// continuation.h — PHASE-4 TASK 10: THE CONTINUATION DRIVER, the sweep loop
// that follows a parametric NLP's solution path from p0 to p1 by paying ONE
// cold solve at p0 and then only warm ones.
//
// This is the workflow the whole warm-start subsystem exists for. Nothing here
// is a new algorithm: it is a COMPOSITION LAYER over what Tasks 2-9 already
// built, and it deliberately adds NOTHING to SqpDriver itself.
//
//   Task 2/3  WarmStart + the 3-arg solve(model, x0, warm) -- the hand-off
//             between two consecutive parameter values.
//   Task 4    the `hot` factorization handle, offered through that same
//             object (reachable only when the caller raises
//             SqpOptions::start_level to kHot; the predictor drops `hot` by
//             contract, so a PREDICTED sweep is kWarm by construction).
//   Task 5    SqpOptions::warm_full_step -- the Kungurtsev-Diehl rule that
//             stops the funnel from re-globalizing a warm start from scratch.
//             It engages automatically on every step past the first here,
//             since every one of them resolves warm.
//   Task 6    SqpOptions::budget_mode -- see BUDGET EXHAUSTION below, the one
//             status this loop treats as neither success nor failure.
//   Task 9    predict() -- the tangential predictor, which turns the warm
//             start at p into a first-order-accurate one at p + dp.
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
// method could not make progress; seeding the retry from there hands the
// shorter step the same bad position that just defeated the longer one. The
// last CONVERGED point is a genuine KKT point of a nearby problem, which is
// the only thing this layer has any theory about.
//
// THE SHRINK IS APPLIED TO THE STEP THAT ACTUALLY FAILED, NOT TO THE
// CONTROLLER'S dp -- and this is a FIX, made in review round 1, to a rule that
// was previously wrong near p1.
//
// The proposal is CLAMPED to the endpoint, so the step actually attempted is
// step_dp = min(dp, total - s_cur), which near p1 can be far shorter than dp.
// The original rule shrank dp itself. When dp was much larger than the
// remainder, shrinking it left the CLAMPED proposal unchanged, so the identical
// failing solve was repeated -- once per halving -- until dp finally decayed
// below the remainder. The cost is
// ceil(log(step_dp/dp) / log(shrink)) wasted full solves: SIX for dp = 0.5
// against a 0.01 remainder at shrink = 0.5, and unbounded as the remainder
// shrinks relative to dp. An earlier version of this note claimed that cost was
// "bounded (one wasted attempt)"; that claim was FALSE and is retracted here.
// The same defect had a second face: the run could report the dp < dp_min
// failure without ever having ATTEMPTED a step shorter than the clamped
// remainder.
//
// So the failure branch sets dp = shrink * step_dp. Since step_dp <= dp by
// construction, this is IDENTICAL to the old rule wherever no clamp was in
// effect (the ordinary mid-sweep case, where step_dp == dp) and strictly
// stronger where one was: the retry is always shorter than the attempt that
// just failed, which is what "shrink and retry" was supposed to mean
// everywhere. Growth after a success can still re-propose a clamped endpoint
// that failed earlier, but that now costs ONE attempt, because the shrink that
// follows it bites immediately.
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
// treats as neither success nor failure, and the decision the brief left open.
//
// When the caller's driver runs with SqpOptions::budget_mode, a solve that
// exhausts max_iter reports kBudgetExhausted and returns THE BEST ITERATE IT
// VISITED BY THE FUNNEL'S OWN ORDERING (feasibility first, min f as the
// tie-break) rather than the last one reached. sqp_types.h's own note on that
// option says what the ordering was chosen for, in as many words: budgeted
// mode asks "what point should a CONTINUATION DRIVER pick up from". So the
// returned object is not the wreckage of a failed step -- it is a hand-off
// designed for this loop, and the honest reading of it is "the step was too
// expensive for one budget slice", not "the step was too long".
//
// This loop therefore RE-SOLVES AT THE SAME p_next, seeded by the returned
// warm start, rather than shrinking dp and moving p_next back. Shrinking would
// be the wrong response twice over: it discards progress the solve genuinely
// made (the returned iterate is closer to the path than the one it started
// from), and it treats a BUDGET decision -- which the caller set, and can
// raise -- as evidence about the PROBLEM's geometry, which is what dp is
// supposed to track.
//
// IT IS CAPPED, because "continue at the same p" is exactly the shape an
// infinite loop takes. kBudgetContinuationsMax consecutive exhaustions at one
// proposal demote it to an ordinary failure -- dp shrinks, the retry goes back
// to the last good warm start -- on the argument that a proposal which has
// consumed several full budgets without converging is no longer merely
// expensive. The cap resets whenever the proposal changes.
//
// THE COLD STEP AT p0 GETS THE SAME RULE AND THE SAME CAP. Nothing about the
// argument above is specific to a warm step, and making p0 the one parameter
// value where an exhaustion is fatal would have been an asymmetry with no
// reasoning behind it. The only difference is what the CAP can do there: at p0
// the proposal is the caller's own endpoint, so there is no dp to shrink and
// the demotion simply ends the sweep.
//
// THE STEP CONTROLLER IS TOLD ABOUT THE COST. dp grows only when the TOTAL
// majors spent arriving at a proposal -- summed over its budget continuations,
// not just the last slice -- is within target_majors. A proposal that took
// three budgets is not a cheap step and must not lengthen the next one.
//
// With budget_mode OFF (the default) none of this is reachable: sqp_types.h
// guarantees kBudgetExhausted is NEVER reported then, and every max_iter
// exhaustion arrives as kMaxIter, which is an ordinary failure here.
//
// ---------------------------------------------------------------------
// RETRY ECONOMICS (PHASE-6 TASK 1) -- what a REJECTED proposal costs, which
// is a different question from which proposals are made, and on the corpus
// that motivated this phase it was the dominant one.
//
// THE MEASUREMENT THAT PROVOKED IT. On the F7 sweep at nx = 10^5
// (docs/notes/2026-08-01-psiopt-first-comparison.md, section 4.5(b), from
// prototypes/psiopt_bridge/results/sweep_n20000_warm.csv): 14 attempts placed
// 9 converged steps, and the FIVE failed proposals accounted for 84 % of the
// sweep's QP minor iterations and 86 % of its wall. Every one of them was
// paid at FULL PRICE -- the loop below learned that a proposal was bad only
// when the driver gave up on it, several majors and several thousand minors
// later. Two separate defects hide in that sentence, and this section names
// both because the fix is two rules, not one:
//
//   (1) A FAILING PROPOSAL WAS NOT DETECTED UNTIL IT FINISHED FAILING. The
//       repair is ContinuationOptions::probe_budget: the proposal's first
//       solve is given a minor-iteration budget scaled from what the last
//       CONVERGED step cost, and a solve still running when it is spent is
//       abandoned (SqpCounters::probe_budget_stops, and
//       ContinuationResult::proposals_abandoned here). The rule is
//       deliberately a COST rule and not a step-length rule: it never touches
//       the dp arithmetic (a failure still sets dp = shrink * step_dp from the
//       same base), so the only thing it sets out to change is how much was
//       spent discovering that a proposal fails.
//
//       IT CAN STILL MOVE THE PROPOSAL SEQUENCE, in exactly one way, and the
//       claim is stated this carefully because the measurement caught it: a
//       step that would have CONVERGED can be abandoned if its first major
//       spends nearly all of the minors it needed (the budget is tested
//       between majors -- see below). The sweep then shrinks and re-reaches
//       the same parameter value in more, cheaper steps. Measured: it does not
//       happen anywhere on the healthy corpus or on either test fixture, and
//       it happens ONCE on the nx = 10^5 sweep, which still ends up at 0.54x
//       the minors (docs/notes/2026-08-02-controller-retry-economics.md, §3
//       and §7(2)).
//
//   (2) THE CONTROLLER RE-PROPOSED THE LENGTH THAT HAD JUST FAILED. In that
//       same sweep the growth rule doubled dp back to the failing length
//       after each successful shrink-step, and four of the five failed
//       proposals sit at that one length. (What suspending growth actually
//       BUYS is smaller than that pattern suggests -- one failure of the
//       five, 0.80x of the minors, measured in the note's section 4 -- since
//       the hard region still has to be crossed.) The repair is
//       ContinuationOptions::suspend_growth_after_failure: the first accepted
//       step after a failure does not lengthen the next proposal. This one IS
//       a step-length rule and does change which parameter values are
//       visited -- fewer, cheaper failures, at the price of reaching dp_max
//       one step later on a path that turns easy again.
//
// WHY A COST RATIO AND NOT AN ABSOLUTE MINOR COUNT. The budget's baseline is
// the last converged step's own cost because that is the only scale this
// layer has any evidence about: it knows nothing about n, about the active
// set, or about the caller's QpOptions::max_iter, and a constant would have
// to be re-derived for every problem size. What it does know is that the
// steps of a continuation are supposed to be CHEAP AND SIMILAR -- that is the
// whole premise of following a path -- so a proposal that has already cost a
// multiple of its predecessor and is still going is, by this layer's own
// theory, not on the path any more.
//
// WHAT ABANDONMENT CANNOT DO, stated so nobody reads more into it:
//   - It cannot lose an answer. The driver's convergence test is evaluated
//     before the budget test on the same major, so an over-budget solve that
//     is AT a KKT point is reported kOptimal and kept (sqp_driver.h's PROBE
//     BUDGET note, part 3).
//   - It cannot make a failure free. The check is between majors, so an
//     abandoned proposal still paid for the major that crossed its budget --
//     one QP solve, up to the caller's own minor cap. The floor on a failed
//     proposal's cost is one major, and this rule buys the difference between
//     that floor and however many majors the driver would have spent.
//   - It cannot fail a sweep that would otherwise have succeeded... except in
//     the one honest sense that a shorter step is then attempted instead, and
//     a run that would have converged at the longer step now converges at the
//     shorter one (or, at dp_min, fails as it always could). An abandoned
//     proposal is a REJECTED proposal, with exactly the consequences the
//     rejection branch has always had.
//
// BOTH RULES ARE INVISIBLE ON A SWEEP THAT NEVER FAILS, and that is a
// requirement rather than a remark: the healthy corpus (the F3/F7 battery
// rows, the fixed-step sweeps, bench --self-check) must be BIT-IDENTICAL to
// the pre-Task-1 loop. The probe budget can only fire on a solve that is
// still running after exceeding its budget -- a converged one is never
// touched -- and the growth suspension is armed only by a failure.
//
// ---------------------------------------------------------------------
// THE PREDICTOR IS ALWAYS ASKED FOR ITS OUTCOME. predict() takes an optional
// PredictorOutcome out-parameter, and this loop passes a non-null one on every
// call. predictor.h's own note explains why a looping caller must: a
// prediction that could not be computed DEGRADES to the identity (the
// unpredicted warm start) and reports kDegraded rather than throwing, and the
// net that produces it includes a model BUG surfacing at the probe point. A
// sweep that passed nullptr would be indistinguishable from a healthy one
// except by being slower. So:
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
// the step simply proceeds unpredicted, which is exactly the use_predictor ==
// false behaviour for that one step.
//
// ---------------------------------------------------------------------
// dp IS AN ARC LENGTH AND direction IS THE UNIT VECTOR ALONG p1 - p0, so for a
// multi-parameter model the proposal is p_cur + dp * (p1 - p0)/||p1 - p0||.
// The vector handed to predict() is then the parameter-space step EXACTLY --
// p_next - p_cur, not dp and not some axis -- which is what predict()'s own
// directional finite difference (p + h*dp/||dp||, rescaled by ||dp||/h)
// requires to describe the move actually being made. The final proposal is set
// to p1 ITSELF rather than to p0 + total*direction, so the sweep lands on the
// caller's endpoint bit-for-bit instead of a rounded reconstruction of it.
//
// ---------------------------------------------------------------------
// WHAT THIS HEADER DOES NOT HAVE, AND WHY (deviations from the task brief,
// stated here rather than left to be noticed):
//
//   - NO `SqpOptions solve_options` FIELD ON ContinuationOptions. The brief
//     listed one; it cannot be honoured against the brief's own signature.
//     SqpDriver copies its SqpOptions AT CONSTRUCTION (sqp_driver.h) and
//     exposes neither a setter nor a getter, so a driver passed in BY
//     REFERENCE cannot be re-configured per sweep and its configuration cannot
//     even be read back to check consistency. The field could therefore only
//     ever have been inert -- an options struct that silently governs nothing,
//     which is precisely the kind of quiet lie project rule T6 exists to
//     forbid. The per-step solver configuration is the DRIVER's, set by the
//     caller when constructing it; budget_mode, warm_full_step, start_level
//     and the tolerances all compose through that object and are read by this
//     loop only through the SqpStatus/SqpCounters it returns.
//   - NO PredictorOptions FIELD. predict() is called with its defaults. This
//     is a genuine (small) limitation rather than an impossibility -- unlike
//     solve_options, such a field WOULD work -- and it is left out only
//     because no caller or test in this phase needs to retune fd_step_scale
//     mid-sweep. Adding it later is source-compatible.
//   - NO ITERATION CAP. See TERMINATION IS GUARANTEED above.

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <tycho_sqp/nlp_model.h>
#include <tycho_sqp/predictor.h>
#include <tycho_sqp/sqp_driver.h>
#include <tycho_sqp/sqp_types.h>
#include <tycho_sqp/types.h>
#include <tycho_sqp/warm_start.h>

namespace tycho::sqp {

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

    // PHASE-6 TASK 1 -- THE PROBE BUDGET, in multiples of the LAST CONVERGED
    // step's own QP minor-iteration cost. See this header's RETRY ECONOMICS
    // note for what it is for and what it was measured to buy; only the knob
    // is described here.
    //
    // Each proposal's FIRST solve is given a minor budget of
    // probe_budget * (minors the last converged step cost). A solve that
    // reaches a major having spent that many minors without converging is
    // ABANDONED -- the driver returns kMaxIter with
    // SqpCounters::probe_budget_stops == 1 (sqp_driver.h's PROBE BUDGET
    // note) -- and this loop treats that exactly as any other failed
    // proposal: shrink, retry from the last good warm start. It is a
    // FAILED-PROPOSAL DETECTOR rather than a step-length rule -- it touches
    // no part of the dp arithmetic -- though a FALSE abandonment does move
    // the proposal sequence, by the ordinary shrink path; see this header's
    // RETRY ECONOMICS note for the measured frequency of that.
    //
    // THE BUDGET IS NEVER SMALLER THAN continuation_detail::
    // kProbeBudgetFloor, whatever this multiplier and the last step's cost
    // work out to -- see that constant for the measured counter-example that
    // guard exists for, and for where it binds.
    //
    // THIS MULTIPLIER IS, AS OF FIX ROUND 1, AN UNMEASURED TERM, and saying
    // so is more useful than the sweep table it came from. On every corpus
    // this task ran, the FLOOR is what sets the budget:
    //   - in-suite, every fixture's converged steps cost 2-80 minors, so
    //     max(k * last_good, 200) == 200 for any k this project would ship --
    //     which is why the full-suite mutations that delete the ratio term
    //     (M8, M9 in the note's section 6.1) both SURVIVE;
    //   - at nx = 10^5 the floor still sets 10 of the 16 budgeted steps, and
    //     the arm run at probe_budget = 1 -- where the floor sets ALL of them,
    //     i.e. a floor-only controller -- is counter-for-counter IDENTICAL to
    //     the shipped default. The one arm where the ratio demonstrably
    //     changed anything is probe_budget = 4, and it changed it for the
    //     WORSE (3 909 minors against 3 637).
    // So the default of 2 is not a tuned value: it is the more conservative
    // of two measured-equal settings, kept because a pure constant cannot
    // scale (a corpus whose converged steps cost thousands of minors needs
    // the ratio, and this project will meet one). Anyone tempted to tune it
    // should first find a corpus where it binds -- there is none here.
    //
    // 0 DISABLES THE PROBE BUDGET (a negative value is rejected by
    // validate()), and disabling it recovers the pre-Task-1 loop only
    // TOGETHER WITH suspend_growth_after_failure = false: that lever is a
    // step-length rule and changes which parameter values are visited on any
    // sweep that fails, so probe_budget = 0 ALONE is not the old controller.
    // (Measured, same grid: BASE gives 14 attempts / 6 732 minors; the
    // suspension alone gives 15 / 5 409 --
    // prototypes/psiopt_bridge/results/sweep_n20000_warm_task1_suspension_only.csv.)
    //
    // MUST BE >= 0.
    int probe_budget = 2;

    // PHASE-6 TASK 1 -- THE FAILURE-HISTORY TERM, the one place a failure is
    // allowed to inform the NEXT proposal's length rather than only this
    // one's. When true (the default), the growth rule is SKIPPED for the
    // first accepted step after a failed proposal: dp is left where the
    // shrink put it for one more step instead of immediately doubling back
    // toward the length that just failed.
    //
    // It exists because the measured pathology is not that dp was mis-sized
    // once, it is that dp was RE-PROPOSED: on the nx = 10^5 sweep the
    // controller grew back to the failing length four consecutive times, and
    // four of the five failed proposals in that sweep are that one rule
    // firing (docs/notes/2026-08-02-controller-retry-economics.md). This is
    // the standard hysteresis of a rejection-aware step controller ("do not
    // increase the step immediately after a rejection"), stated as a lever
    // so it can be swept and turned off.
    //
    // A SWEEP WITH NO FAILED PROPOSAL CANNOT SEE THIS FIELD AT ALL, in
    // either setting: the suspension is armed only by a failure.
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
    // ASKED note argues a looping caller must actually look at -- a sweep in
    // which every prediction degraded is otherwise indistinguishable from a
    // healthy one that happened to be slow.
    Index total_majors = 0;
    Index predictor_calls = 0;
    Index predictor_degradations = 0;

    // PHASE-6 TASK 1 -- THE RETRY-COST SPLIT. Every FAILED attempt in `steps`
    // falls in exactly one of these two, so their sum is the number of failed
    // attempts and neither one ever counts a converged step. They are the
    // pair a caller reads to answer "what did this sweep pay for proposals it
    // then threw away, and how much of that did the probe budget catch".
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
    // A BUDGET-EXHAUSTION CONTINUATION IS NOT A FAILURE and is in neither:
    // this header's BUDGET EXHAUSTION note says why (it is a continue). Only
    // the attempt that ends a chain by being DEMOTED counts, once, as a full-
    // cost failure.
    Index proposals_abandoned = 0;
    Index proposals_full_cost = 0;
};

namespace continuation_detail {

// Consecutive kBudgetExhausted solves allowed at ONE proposal before it is
// demoted to an ordinary failure (this header's BUDGET EXHAUSTION note). 3 is
// an implementation choice, not a paper constant: it is small enough that a
// genuinely stuck proposal costs a bounded multiple of the caller's own
// max_iter and large enough that a step needing "a bit more than one budget"
// -- the case the whole continue rule exists for -- is not thrown away on its
// first slice. It is inert whenever SqpOptions::budget_mode is off, which is
// the default, since kBudgetExhausted cannot be reported then.
inline constexpr int kBudgetContinuationsMax = 3;

// PHASE-6 TASK 1. THE EVIDENCE FLOOR under every probe budget, in QP minor
// iterations: no proposal is ever abandoned for having spent fewer than this
// many minors, however cheap the last converged step was.
//
// IT IS NOT A TUNING PARAMETER, IT IS A GUARD, and it exists because a pure
// ratio has no meaning at a tiny baseline.
//
// WHY THERE IS A FLOOR AT ALL -- the cheap counter-example. F7 at N = 100
// nodes (n = 500), swept p: 0.3 -> 0.9 with dp_init = 0.2, is a sweep in
// which NOTHING fails; its first two steps converge in 2 minors each, so a
// FLOORLESS probe_budget = 2 budgets the third at 4 minors and abandons it
// even though it converges in 47. That single false abandonment cascades:
// tests/test_continuation.cpp's HealthySweepIsBitIdenticalUnderTheProbeBudget
// is exactly this sweep, and dropping the floor turns its 3 steps / 51 minors
// into a shrink-and-retry pile (the same experiment over the longer
// p: 0.3 -> 1.05 range measured 4 steps / 63 minors -> 25 steps / 298
// minors, a 4.7x REGRESSION on a healthy sweep). Fix round 1's M2 mutation
// puts a number on it corpus-wide: with the floor removed, EIGHT tests fail,
// six of them sweeps that predate this task.
//
// WHY 200 AND NOT 100 -- the number this constant actually is, derived from
// the case that BINDS. The binding control is the Task-9 head-to-head grid at
// N = 2000 nodes (n = 10^4), whose second step legitimately costs 126 minors
// over 3 majors after a 22-minor predecessor: at the top of its third major
// it has spent 110, so any floor at or below 110 abandons a step that was
// about to converge. Measured at floor = 100: that sweep goes from 5 steps /
// 270 minors to 7 steps / 304 minors -- a healthy-corpus control MOVED, which
// this task is not allowed to do. 200 clears the binding case by 1.8x and
// leaves every control cell bit-identical (the note's section 5).
//
// WHERE IT BINDS, MEASURED RATHER THAN ASSUMED (fix round 1 correction: this
// comment previously claimed the floor "never binds" at nx = 10^5, which this
// task's own artifact refutes). Recomputing max(2 * last_good_minors, 200)
// step by step down sweep_n20000_warm_task1_after.csv -- the baseline
// advances only on a CONVERGED step -- gives budgets
// 334, 334, 334, 356, 200, 214, 214, and 200 for every step after that:
// THE FLOOR SETS THE BUDGET ON 10 OF THE 16 BUDGETED STEPS, including two of
// the five abandonments (the ones following 74- and 44-minor steps). The
// honest general statement is therefore the opposite of the old one: on every
// corpus this task has measured, the floor is the operative term and the
// ratio is headroom above it -- see ContinuationOptions::probe_budget for
// what that means for the multiplier.
inline constexpr Index kProbeBudgetFloor = 200;

// T6 validation of the caller's inputs, all of it BEFORE the model is touched,
// so a rejected call cannot leave the model re-posed (the same
// validate-first discipline predictor.h adopted in Task 9's fix round).
inline void validate(const ParametricNlpModel &model, const Vec &p0, const Vec &p1,
                     const ContinuationOptions &opts) {
    const Index np = model.parameter_dim();
    if (p0.size() != p1.size()) {
        throw std::invalid_argument(
            fmt::format("run_continuation: p0 has size {} and p1 size {}; the endpoints of a "
                        "sweep must have the same dimension",
                        p0.size(), p1.size()));
    }
    if (p0.size() != np) {
        throw std::invalid_argument(fmt::format("run_continuation: p0/p1 have size {}, expected {} "
                                                "(= model.parameter_dim())",
                                                p0.size(), np));
    }
    if (!p0.allFinite() || !p1.allFinite()) {
        throw std::invalid_argument(
            "run_continuation: p0 or p1 has a non-finite entry; a sweep has no direction to "
            "follow from one");
    }
    if (!(opts.dp_init > 0.0) || !std::isfinite(opts.dp_init)) {
        throw std::invalid_argument(fmt::format(
            "run_continuation: ContinuationOptions::dp_init is {}, must be finite and > 0",
            opts.dp_init));
    }
    if (!(opts.dp_min > 0.0)) {
        throw std::invalid_argument(
            fmt::format("run_continuation: ContinuationOptions::dp_min is {}, must be > 0 (it is "
                        "the step-length floor the run fails below)",
                        opts.dp_min));
    }
    if (!(opts.dp_max >= opts.dp_min)) {
        throw std::invalid_argument(
            fmt::format("run_continuation: ContinuationOptions::dp_max ({}) must be >= dp_min ({})",
                        opts.dp_max, opts.dp_min));
    }
    if (opts.dp_init < opts.dp_min || opts.dp_init > opts.dp_max) {
        throw std::invalid_argument(
            fmt::format("run_continuation: ContinuationOptions::dp_init ({}) must lie in "
                        "[dp_min, dp_max] = [{}, {}]",
                        opts.dp_init, opts.dp_min, opts.dp_max));
    }
    if (!(opts.grow >= 1.0) || !std::isfinite(opts.grow)) {
        throw std::invalid_argument(fmt::format(
            "run_continuation: ContinuationOptions::grow is {}, must be finite and >= 1",
            opts.grow));
    }
    if (!(opts.shrink > 0.0) || !(opts.shrink < 1.0)) {
        throw std::invalid_argument(
            fmt::format("run_continuation: ContinuationOptions::shrink is {}, must lie in (0, 1)",
                        opts.shrink));
    }
    if (opts.target_majors < 0) {
        throw std::invalid_argument(
            fmt::format("run_continuation: ContinuationOptions::target_majors is {}, must be >= 0",
                        opts.target_majors));
    }
    if (opts.probe_budget < 0) {
        throw std::invalid_argument(
            fmt::format("run_continuation: ContinuationOptions::probe_budget is {}, must be >= 0 "
                        "(0 disables the probe budget; it is a MULTIPLE of the last converged "
                        "step's minor-iteration cost, so a negative one has no reading)",
                        opts.probe_budget));
    }
}

} // namespace continuation_detail

// Sweep `model` from p0 to p1, re-solving at each proposal with `driver` and
// returning every attempt. See this header's THE LOOP note for the semantics
// and BUDGET EXHAUSTION for the one status that is neither success nor
// failure.
//
// THROWS std::invalid_argument (project rule T6, sizes always in the message)
// on endpoints whose dimensions disagree with each other or with
// model.parameter_dim(), on a non-finite endpoint, and on options that cannot
// be honoured. Validation runs BEFORE the model is touched.
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
inline ContinuationResult run_continuation(ParametricNlpModel &model, const Vec &p0, const Vec &p1,
                                           SqpDriver &driver,
                                           const ContinuationOptions &opts = {}) {
    continuation_detail::validate(model, p0, p1, opts);

    ContinuationResult out;

    // Records one attempt and folds it into the aggregates.
    const auto record = [&out](const Vec &p, double dp, const SqpSolution &sol,
                               const std::optional<PredictorOutcome> &outcome, bool used) {
        ContinuationStep step;
        step.p = p;
        step.x = sol.x;
        step.dp = dp;
        step.status = sol.status;
        step.counters = sol.counters;
        step.level = sol.counters.start_level_used;
        step.predictor_used = used;
        step.predictor_outcome = outcome;
        out.total_majors += sol.counters.major_iters;
        out.steps.push_back(std::move(step));
    };

    // ---- STEP 0: THE ONE COLD SOLVE ----------------------------------
    model.set_parameters(p0);
    SqpSolution sol = driver.solve(model, model.start_point());
    record(p0, 0.0, sol, std::nullopt, /*used=*/false);

    // THE COLD STEP PARTICIPATES IN THE BUDGET RULE TOO (fix round 1). An
    // earlier version returned on any non-kOptimal status here, which made p0
    // the one parameter value in the whole sweep where kBudgetExhausted was
    // fatal rather than a continue -- an asymmetry with no argument behind it,
    // since a p0 needing two budget slices is in exactly the position every
    // later p is when it needs two. Same rule, same cap, same reason: re-solve
    // AT p0 from the hand-off budgeted mode returned.
    //
    // THE ONE THING THAT IS GENUINELY DIFFERENT HERE, and it is a consequence
    // rather than a choice: there is no dp to shrink at p0, because the
    // proposal is p0 itself and the caller fixed it. So the cap's demotion has
    // nothing to demote TO and the sweep ends -- which is the same outcome any
    // other unrecoverable failure at p0 has.
    // The cold solve's own minor cost is the FIRST probe budget's baseline
    // (see the probe-budget block in the loop below), summed over its budget
    // continuations for the same reason majors_at_proposal is: what a
    // proposal cost is the whole of what arriving at it cost.
    Index minors_at_proposal = sol.counters.qp_minor_iters;
    for (int cold_continuations = 0;
         sol.status == SqpStatus::kBudgetExhausted &&
         cold_continuations < continuation_detail::kBudgetContinuationsMax;
         ++cold_continuations) {
        const WarmStart handoff = sol.warm_start;
        sol = driver.solve(model, handoff.x, handoff);
        minors_at_proposal += sol.counters.qp_minor_iters;
        record(p0, 0.0, sol, std::nullopt, /*used=*/false);
    }

    out.final_warm = sol.warm_start;
    if (sol.status != SqpStatus::kOptimal) {
        return out; // no path to follow from a p0 that did not solve
    }

    const Vec segment = p1 - p0;
    const double total = segment.norm();
    if (!(total > 0.0)) {
        out.reached_p1 = true; // p1 == p0: the cold solve IS the sweep
        return out;
    }
    const Vec direction = segment / total;

    Vec p_cur = p0;
    WarmStart warm_cur = sol.warm_start;
    double s_cur = 0.0; // arc position of p_cur along the segment
    double dp = opts.dp_init;

    // Live state of the CURRENT proposal. `continuing` is true only while a
    // budget-exhaustion chain is open at it (this header's BUDGET EXHAUSTION
    // note), in which case p_next/s_next/step_dp are reused unchanged and the
    // seed is the exhausted solve's own hand-off.
    Vec p_next = p_cur;
    double s_next = 0.0;
    double step_dp = 0.0;
    Index majors_at_proposal = 0;
    int budget_continuations = 0;
    bool continuing = false;
    WarmStart budget_warm;

    // PHASE-6 TASK 1 (this header's RETRY ECONOMICS note). `last_good_minors`
    // is the whole minor cost of arriving at the last CONVERGED parameter
    // value -- the cold solve at p0 to begin with -- and is the only thing
    // the probe budget is scaled from. `growth_suspended` is the
    // failure-history term: armed by a failed proposal, spent by the next
    // accepted one.
    Index last_good_minors = minors_at_proposal;
    bool growth_suspended = false;

    while (true) {
        WarmStart seed;
        std::optional<PredictorOutcome> reported;
        bool predictor_used = false;

        if (continuing) {
            seed = budget_warm; // same p_next, same step_dp: re-solve, do not re-predict
        } else {
            s_next = std::min(s_cur + dp, total);
            // The last proposal is p1 ITSELF, not a reconstruction of it.
            p_next = (s_next >= total) ? p1 : Vec(p0 + s_next * direction);
            step_dp = s_next - s_cur;
            majors_at_proposal = 0;
            minors_at_proposal = 0;

            seed = warm_cur;
            if (opts.use_predictor) {
                // predict() reads its base parameters off the model, so the
                // model must be posed where warm_cur was TAKEN -- which it is
                // not after a failed attempt at some other p_next.
                model.set_parameters(p_cur);
                // Pre-set to a value no path writes on purpose: a predict()
                // that failed to report would be caught by the assertions this
                // feeds, not silently read back as a plausible outcome.
                PredictorOutcome outcome = PredictorOutcome::kZeroStep;
                seed = predict(model, warm_cur, Vec(p_next - p_cur), PredictorOptions{}, &outcome);
                reported = outcome;
                ++out.predictor_calls;
                if (outcome == PredictorOutcome::kDegraded) {
                    ++out.predictor_degradations;
                }
                // ONLY kPredicted is a prediction taken; the other two return
                // the identity, i.e. the unpredicted warm start.
                predictor_used = (outcome == PredictorOutcome::kPredicted);
            }
        }

        // PHASE-6 TASK 1. THE PROBE BUDGET, armed on a PROPOSAL'S FIRST SOLVE
        // only. A budget continuation (`continuing`) is deliberately left
        // unbudgeted: it is not a new proposal, it is the caller's own
        // budgeted mode finishing a solve this loop already decided to keep
        // paying for, and it is capped by kBudgetContinuationsMax already.
        // ONLY `probe_budget == 0` disarms it. A zero BASELINE
        // (`last_good_minors == 0`, e.g. the first proposal of a sweep) does
        // NOT: kProbeBudgetFloor is the max() below and still arms the budget
        // at 200 minors. This comment used to claim the opposite (final fix
        // wave, W10). See ContinuationOptions::probe_budget, whose own note
        // already says the floor is what sets the budget on every corpus this
        // project has run.
        const Index probe_minors =
            (!continuing && opts.probe_budget > 0)
                ? std::max(static_cast<Index>(opts.probe_budget) * last_good_minors,
                           continuation_detail::kProbeBudgetFloor)
                : 0;

        model.set_parameters(p_next);
        // seed.x is the COLD FALLBACK only (sqp_driver.h ignores x0 whenever
        // `warm` resolves warm or hot); passing the seed's own point keeps a
        // degraded-to-cold solve starting from the nearest thing available
        // rather than from the model's generic start point.
        sol = driver.solve(model, seed.x, seed, probe_minors);
        majors_at_proposal += sol.counters.major_iters;
        minors_at_proposal += sol.counters.qp_minor_iters;
        record(p_next, step_dp, sol, reported, predictor_used);

        if (sol.status == SqpStatus::kOptimal) {
            p_cur = p_next;
            s_cur = s_next;
            warm_cur = sol.warm_start;
            out.final_warm = warm_cur;
            continuing = false;
            budget_continuations = 0;
            // The baseline the NEXT proposal's probe budget is scaled from
            // (Task 1) -- the whole cost of arriving here, on the same
            // reading majors_at_proposal has.
            last_good_minors = minors_at_proposal;
            if (s_cur >= total) {
                out.reached_p1 = true;
                return out;
            }
            // The cost that decides the next step's length is the WHOLE cost
            // of arriving here, budget continuations included.
            //
            // THE FAILURE-HISTORY TERM (Task 1) sits in front of that rule and
            // nowhere else: one accepted step after a failure does not
            // lengthen the next proposal, however cheap it was. See
            // ContinuationOptions::suspend_growth_after_failure. Note the
            // suspension is SPENT here whether or not the growth test would
            // have passed -- it is "the step after a failure", not "the next
            // step that would have grown", so a sweep cannot bank it.
            if (growth_suspended) {
                growth_suspended = false;
            } else if (majors_at_proposal <= static_cast<Index>(opts.target_majors)) {
                dp = std::min(dp * opts.grow, opts.dp_max);
            }
            continue;
        }

        if (sol.status == SqpStatus::kBudgetExhausted &&
            budget_continuations < continuation_detail::kBudgetContinuationsMax) {
            ++budget_continuations;
            continuing = true;
            budget_warm = sol.warm_start;
            continue;
        }

        // An ordinary failure (or a budget chain that ran past its cap): the
        // proposal was too long. Shrink and retry FROM THE LAST GOOD WARM.
        continuing = false;
        budget_continuations = 0;
        // PHASE-6 TASK 1. Which KIND of failure this was -- caught by the
        // probe budget, or paid in full -- and the arming of the
        // failure-history term. Both read the solve that just came back:
        // probe_budget_stops is set by, and only by, the driver's own
        // budget exit (sqp_driver.h).
        if (sol.counters.probe_budget_stops > 0) {
            ++out.proposals_abandoned;
        } else {
            ++out.proposals_full_cost;
        }
        growth_suspended = opts.suspend_growth_after_failure;
        // FROM THE STEP THAT ACTUALLY FAILED, not from the controller's dp --
        // step_dp <= dp always (the endpoint clamp), so this is the old
        // dp *= shrink wherever no clamp was in effect and strictly stronger
        // where one was. See this header's THE SHRINK IS APPLIED TO THE STEP
        // THAT ACTUALLY FAILED note for the defect this repairs.
        dp = step_dp * opts.shrink;
        if (dp < opts.dp_min) {
            return out; // reached_p1 stays false
        }
    }
}

} // namespace tycho::sqp
