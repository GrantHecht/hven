// continuation.cpp -- the continuation driver, `run_continuation`, and the
// input validation it runs first, `continuation_detail::validate`, carved out
// of detail/warmstart/continuation.h.
//
// M3 PHASE-C T8, the last carve of the phase-C T-series.
//
// ---------------------------------------------------------------------------
// WHY IT COSTS NOTHING, READ OUT OF THE OBJECT FILES.
//
// run_continuation runs ONCE PER SWEEP and loops per continuation STEP, and
// each of its steps is a whole SqpDriver::solve() -- so the call into this
// function is arithmetically the cheapest thing in any program that makes it.
// The object files agree: at the base commit it emitted a weak copy in exactly
// five objects (bench_scale, test_b1_gate, test_continuation, test_hs_sweeps,
// test_warm_start_battery) and every call to it was already a DIRECT
// relocation to that copy. No src/ object defined it at all -- like the
// crossover T7 moved, this is a public entry point with no caller inside the
// library.
//
// TWO THINGS TRAVEL WITH IT, and both were counted before the carve rather
// than assumed:
//
//   * continuation_detail::validate, called EXACTLY ONCE from run_continuation
//     in every one of the five objects -- i.e. it was never inlined into it.
//     It moves here so the validate-first discipline stays next to the loop it
//     guards.
//   * the `record` CLOSURE's out-of-line operator(), which lived in its own
//     COMDAT section and was called THREE times in every one of the five
//     objects. It is defined inside the moved body, so it travels with it. A
//     relocation census that looked only at `.text` would not have seen those
//     three calls at all -- the T5 review's F1, on the target side.
//
// ---------------------------------------------------------------------------
// THE FP, AND WHAT CARRIES THE PROOF.
//
// The step-size policy computes with doubles: the shrink/grow updates of dp,
// the arc position s_cur against `total`, the dp_min floor, and the
// predictor's degraded-outcome accounting. Those decide how many steps a sweep
// takes and at which parameter values, so they decide which solves happen at
// all -- this is the carve in the series whose arithmetic has the longest
// lever on a counter.
//
// Since M3 phase-C U0 there is ONE uniform flag regime: this TU, the header,
// and every consumer compile with the same options, so these expressions are
// compiled exactly as they were compiled inline. The falsifier is the same one
// T4, T5, T6 and T7 accepted: the proof is BIT-IDENTITY of every asserted
// counter across the 57-cell walk census and the 17-cell bench. Any counter
// movement means the arithmetic moved, and the carve is reverted rather than
// re-derived.
//
// ---------------------------------------------------------------------------
// The algorithm, the budget rule, the predictor interaction, what the sweep
// leaves the model posed at, and every "why" stay in the header. This file
// carries the executable form.

#include <hven/detail/warmstart/continuation.h>

namespace hven::solvers {

namespace continuation_detail {

void validate(const ParametricNlpModel &model, const Vec &p0, const Vec &p1,
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

ContinuationResult run_continuation(ParametricNlpModel &model, const Vec &p0, const Vec &p1,
                                    SqpDriver &driver, const ContinuationOptions &opts) {
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

} // namespace hven::solvers
