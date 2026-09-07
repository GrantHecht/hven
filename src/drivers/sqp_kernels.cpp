// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// sqp_kernels.cpp — the elastic ladder and the certified feasibility fallback,
// and the three helpers that serve only them.
//
// M6 W5 T6 CUT (d). This file is the second half of a MEASURED boundary, and
// it exists on measured terms rather than on tidiness. `sqp_driver.cpp`'s own
// banner explains why every free function in that TU sat beside the loop: the
// inliner sees their bodies there, and `solve_impl` and its neighbours are the
// only in-library callers any of them have. Cut (d) tests exactly one claim
// against that — that these six definitions are COLD ENOUGH to leave, and that
// moving them costs no measurable runtime.
//
// WHAT MOVED, and nothing else did (ownership doc §1.7, §10.4):
//
//   * `run_elastic_ladder`         — external linkage, UNCHANGED. Declared in
//                                    `hven/drivers/sqp_driver.h` before and
//                                    after; no test TU ever saw its body.
//   * `certified_feasibility_fallback` — the same.
//   * `elastic_initial_rho`        — anonymous namespace, kernels-only.
//   * `elastic_evidence_seed`      — anonymous namespace, kernels-only.
//   * `emit_qp_mode_line`          — anonymous namespace, kernels-only.
//   * `trace_outcome_of`           — the ONE linkage change of the whole cut:
//                                    anonymous namespace -> `detail::`, with a
//                                    SOURCE-PRIVATE declaration in
//                                    `sqp_kernels_internal.h`. See that header.
//
// `predicted_decrease` did NOT move. It is called from the ladder (once, after
// the rung loop, on an optimal result) and from `run_major`, and it is already
// declared in the public header — so the ladder reaches it here exactly as it
// did when both were in one TU, through a call that was ALREADY a relocated
// call to an external symbol in the pre-split object. This is the boundary's
// one REVERSE-direction edge, and it is the one the plan's "loop-used
// arithmetic is not cold" sentence is actually about.
//
// FP arithmetic crosses this TU boundary under the SAME uniform flag regime as
// every other TU here (CLAUDE.md §7): both files compile with one flag set,
// this one is PCH-opted-out exactly as `sqp_driver.cpp` is, and LTO is OFF on
// both sides and was not touched by the cut. Every asserted counter must be
// bit-identical across the boundary — a counter delta here is a FAILED CARVE,
// to be reverted or redrawn, never a re-derivation.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/drivers/sqp_driver.h>

#include "sqp_kernels_internal.h"

namespace hven::solvers {

// THE MAPPER, in `detail::` and declared source-privately. It is the one symbol
// this cut gives external linkage, and it has it solely because the boundary
// runs between its callers -- three here, two in `sqp_driver.cpp`.
namespace detail {

IpqpTraceOutcome trace_outcome_of(QpStatus status) {
    return status == QpStatus::kOptimal ? IpqpTraceOutcome::kOptimal : IpqpTraceOutcome::kEscaped;
}

} // namespace detail

namespace {

// AMENDMENT H, Q-S4 AT c = 1, WITH W2 T5's TWO CAPS: the FIRED block's multiplier norm places the
// first rung, FLOORED at today's start and capped by the escalation headroom and the
// dual-regularization safety margin -- sqp_driver.h's THE PLACEMENT BOUND has the rule.
double elastic_initial_rho(const ElasticSeedSource &seed, double dual_mu, bool &ceiling_hit) {
    ceiling_hit = false;
    if (seed.evidence == nullptr || !seed.evidence->fired ||
        !std::isfinite(seed.evidence->dual_norm_start)) {
        return kElasticRhoInit;
    }
    const double priced = std::max(kElasticRhoInit, seed.evidence->dual_norm_start);
    double cap = kElasticRhoMax / kElasticRhoFactor;
    // A dual_mu of zero or worse prices nothing -- the walk's misfire is proportional to the
    // product, so no product means no cap, and the headroom one still stands.
    if (std::isfinite(dual_mu) && dual_mu > 0.0) {
        cap = std::min(cap, kElasticRhoDualMuSafety / dual_mu);
    }
    // THE FLOOR OUTRANKS BOTH CAPS: a cap below kElasticRhoInit would enter the ladder cheaper
    // than W1's own start, which amendment H's floor exists to forbid.
    const double rho_0 = std::max(kElasticRhoInit, std::min(priced, cap));
    // CLAMPED IS READ OFF THE RESULT, NOT OFF THE CAP (fix round 1): at dual_mu >= 1e-4 the
    // margin's cap sits BELOW the floor, the floor wins, and `priced > cap` would report a clamp
    // on a placement that is exactly W1's own.
    ceiling_hit = rho_0 < priced;
    return rho_0;
}

// THE EVIDENCE ARM'S SEED: a WORKING SET, not a point. Which rows and bounds are TIGHT at the
// least-infeasible iterate by spec 2.3 item 2's RATIO rule on that iterate's own duals, and
// which slack columns start CLOSED because their row is no longer violated there.
QpSolution elastic_evidence_seed(const ElasticQp &e, const QpProblem &qp,
                                 const IpqpInfeasibilityEvidence &ev, const SqpOptions &opts) {
    const Index n = e.n_orig;
    const Index me = qp.me();
    const Index mi = qp.mi();
    QpSolution s;
    s.status = QpStatus::kOptimal;
    // THE PRIMAL IS ZEROED, exactly as in elastic_seed: it is the engine's window CENTRE, and in
    // step variables that centre is p = 0. Everything this function carries is the WORKING SET.
    s.x = Vec::Zero(e.qp.n());
    s.bound_state.assign(static_cast<std::size_t>(e.qp.n()), BoundState::kFree);
    s.ineq_active.assign(static_cast<std::size_t>(mi), false);
    // A BLOCK THAT NEVER FIRED IS NOT EVIDENCE, however its fields are sized: no hint at all,
    // which with elastic_initial_rho's own floor makes a non-fired arm W1's ladder exactly.
    if (!ev.fired || ev.least_infeasible_x.size() != n) {
        return s;
    }
    const Vec &x = ev.least_infeasible_x;
    // THE STATED FALLBACK, not a gap: without the duals the ratio rule cannot be applied, so
    // activity is GEOMETRIC -- distance against feas_tol. On a row that distance is the TRUE
    // residual, negative and so always active, where the ratio arm reads the barrier slack.
    const bool have_duals = ev.least_infeasible_s.size() == mi &&
                            ev.least_infeasible_lambda_i.size() == mi &&
                            ev.least_infeasible_zl.size() == n &&
                            ev.least_infeasible_zu.size() == n && ev.least_infeasible_mu > 0.0;
    const double kappa = opts.ipqp.ipqp_face_kappa;
    const double mu = std::max(ev.least_infeasible_mu, 0.0);
    // Spec 2.3 item 2, three-way and never forced: a pair whose members are both tiny is
    // UNCERTAIN, and an uncertain index is simply left OUT of the seed's working set.
    auto tight = [&](double distance, double dual) {
        return have_duals ? (distance < kappa * dual && dual > mu) : distance <= opts.feas_tol;
    };

    const Vec eq_resid = me > 0 ? Vec(qp.be - qp.Ae * x) : Vec(0);
    const Vec iq_slack = mi > 0 ? Vec(qp.bi - qp.Ai * x) : Vec(0);
    for (Index j = 0; j < mi; ++j) {
        const double distance = have_duals ? ev.least_infeasible_s(j) : iq_slack(j);
        const double dual = have_duals ? ev.least_infeasible_lambda_i(j) : 0.0;
        s.ineq_active[static_cast<std::size_t>(j)] = tight(distance, dual);
    }
    // THE EFFECTIVE BOX, never the original bounds: zl/zu are the multipliers of the
    // WINDOW-CLAMPED bounds the escaped solve actually carried, and `e.qp`'s own original block
    // IS that same clamp -- so a variable pressed on a trust-region face is seen, not skipped.
    for (Index i = 0; i < n; ++i) {
        const double lo_eff = e.qp.lower(i);
        const double up_eff = e.qp.upper(i);
        const bool at_lo = std::isfinite(lo_eff) &&
                           tight(x(i) - lo_eff, have_duals ? ev.least_infeasible_zl(i) : 0.0);
        const bool at_up = std::isfinite(up_eff) &&
                           tight(up_eff - x(i), have_duals ? ev.least_infeasible_zu(i) : 0.0);
        const auto u = static_cast<std::size_t>(i);
        if (at_lo && at_up) {
            s.bound_state[u] = BoundState::kFixed;
        } else if (at_lo) {
            s.bound_state[u] = BoundState::kAtLower;
        } else if (at_up) {
            s.bound_state[u] = BoundState::kAtUpper;
        }
    }

    // THE SLACK COLUMNS, the one thing no failed-solve seed can say: a row RELAXED at p_ref
    // (build_elastic_subproblem's own point) may be satisfied at THIS one, and its slack then
    // starts CLOSED at its lower bound instead of free. Geometric by nature -- there is no dual.
    for (Index k = 0; k < me; ++k) {
        if (e.eq_slack[static_cast<std::size_t>(k)] != kNoSlack &&
            std::abs(eq_resid(k)) <= opts.feas_tol) {
            s.bound_state[static_cast<std::size_t>(e.eq_slack[static_cast<std::size_t>(k)])] =
                BoundState::kAtLower;
        }
    }
    for (Index j = 0; j < mi; ++j) {
        if (e.ineq_slack[static_cast<std::size_t>(j)] != kNoSlack &&
            iq_slack(j) >= -opts.feas_tol) {
            s.bound_state[static_cast<std::size_t>(e.ineq_slack[static_cast<std::size_t>(j)])] =
                BoundState::kAtLower;
        }
    }
    return s;
}

/// @brief THE FREE EMIT (M6 W4 T5), for the two kernel call sites that live in
/// free functions and so have no `SqpDriver::emit_trace_qp_mode` to reach.
///
/// The null check is HERE, once, so a caller with no sink pays one predictable
/// branch and no event construction -- the same shape the driver's member has.
void emit_qp_mode_line(TraceSink *sink, IpqpTraceQpMode mode, IpqpTraceOutcome outcome,
                       QpModeSite site, Index iters) {
    if (sink == nullptr) {
        return;
    }
    QpModeTraceEvent mev;
    mev.mode = mode;
    mev.outcome = outcome;
    mev.iters = iters;
    mev.site = site;
    sink->on_qp_mode(mev);
}

} // namespace

ElasticLadderReport run_elastic_ladder(QpEngine &engine, const QpProblem &qp,
                                       const ElasticSeedSource &seed, double window,
                                       const SqpOptions &opts, SqpCounters &out,
                                       std::optional<double> rho_0_override, TraceSink *sink) {
    // VALIDATED AT THE BOUNDARY (CLAUDE.md section 4): a negative or NaN window crosses the
    // elastic box silently -- `build_elastic_subproblem` clamps lo/up against it with no check.
    if (!(window >= 0.0)) {
        throw std::invalid_argument(
            fmt::format("run_elastic_ladder: window is {}, expected >= 0 (+inf legal)", window));
    }
    // AND THE OVERRIDE ON THE SAME TERMS (fix round 1): it is CLAMPED rather than read, so a NaN
    // would resolve silently to the floor and a caller's mistake would look like a placement.
    if (rho_0_override.has_value() && !(*rho_0_override > 0.0)) {
        throw std::invalid_argument(
            fmt::format("run_elastic_ladder: rho_0_override is {}, expected > 0 (+inf legal, "
                        "clamped to kElasticRhoMax)",
                        *rho_0_override));
    }
    ++out.elastic_activations;

    // BOTH HARD-WIRED SITES MOVE TOGETHER -- the construction's penalty and the ladder's own
    // `rho` -- or the ladder would start at one penalty and escalate from another.
    bool rho0_ceiling_hit = false;
    // AN OVERRIDDEN PLACEMENT IS THE CALLER'S OWN, not a reading of the evidence: it is clamped
    // into the ladder's own range and reported as UNCLAMPED, because no cap refused anything.
    const double rho_0 = rho_0_override.has_value()
                             ? std::min(kElasticRhoMax, std::max(kElasticRhoInit, *rho_0_override))
                             : elastic_initial_rho(seed, opts.qp.dual_mu, rho0_ceiling_hit);
    if (rho0_ceiling_hit) {
        ++out.elastic_rho0_ceiling_hits;
    }
    ElasticQp elastic = build_elastic_subproblem(qp, window, rho_0, opts.feas_tol);
    QpSolution seed_elastic =
        seed.evidence != nullptr
            ? elastic_evidence_seed(elastic, qp, *seed.evidence, opts)
            : elastic_seed(elastic, seed.failed != nullptr ? *seed.failed : QpSolution{});

    QpSolution qs_e;
    // THE STALL EARLY-EXIT's own state: the PREVIOUS rung's
    // augmented solution, valid once has_prev_rung is true
    // (i.e. from the second solve on), so it can be compared
    // against the CURRENT rung's -- see THE STALL EARLY-EXIT
    // note above for the derivation.
    QpSolution qs_e_prev;
    bool has_prev_rung = false;
    double rho = rho_0;
    for (;;) {
        // DEFAULT OVERRIDES: the +inf tr_radius sentinel, because
        // the radius is already in the box above -- passing it
        // here would cap the SLACKS at Delta too.
        const SolveOverrides elastic_overrides;
        qs_e = engine.solve(elastic.qp, seed_elastic, elastic_overrides);
        out.qp_minor_iters += qs_e.counters.minor_iters;
        out.factorizations += qs_e.counters.factorizations;
        out.eqp_refine_steps += qs_e.counters.eqp_refine_steps;
        out.border_refine_steps += qs_e.counters.border_refine_steps;
        out.verdict_refine_steps += qs_e.counters.verdict_refine_steps;
        out.suspect_escalations += qs_e.counters.suspect_escalations;
        out.symbolic_analyses += qs_e.counters.symbolic_analyses;
        // ONE LINE PER RUNG (M6 W4 T5), written where the rung has run and
        // BEFORE the four breaks below, so a climb stopping here still reports.
        //
        // The count over a solve is `elastic_activations + elastic_escalations`.
        emit_qp_mode_line(sink, IpqpTraceQpMode::kWalk, detail::trace_outcome_of(qs_e.status),
                          QpModeSite::kElasticRung, qs_e.counters.minor_iters);
        if (qs_e.status != QpStatus::kOptimal) {
            break;
        }
        // MATERIALLY NONZERO IS MEASURED ON THE VIOLATION, not on
        // the scaled variable: feas_tol is a tolerance on
        // constraint violation, and sigma_j is a change of units.
        const Vec v = elastic.slack_violations(qs_e.x);
        const double v_max = v.size() > 0 ? v.maxCoeff() : 0.0;
        if (v_max <= opts.feas_tol || !(rho < kElasticRhoMax)) {
            break;
        }
        // THE STALL EARLY-EXIT. This rung left the augmented
        // solution where the PREVIOUS one left it -- so, per THE
        // STALL EARLY-EXIT note above, escalating further only
        // re-solves the same reduced system at a larger rho it
        // never reads. Stop here instead of paying for rungs
        // whose answer is already in hand. Compared on the FULL
        // augmented x (original block AND slacks), not just the
        // slack violations `v` above: a stall is "this rung
        // changed nothing", and the slacks alone cannot rule out
        // a p that moved while s happened not to.
        if (opts.elastic_ladder_early_exit && has_prev_rung &&
            (qs_e.x - qs_e_prev.x).lpNorm<Eigen::Infinity>() <=
                kElasticStallScale * std::max(1.0, qs_e_prev.x.lpNorm<Eigen::Infinity>())) {
            break;
        }
        rho = std::min(rho * kElasticRhoFactor, kElasticRhoMax);
        set_elastic_penalty(elastic, rho);
        ++out.elastic_escalations;
        qs_e_prev = qs_e;
        has_prev_rung = true;
        // CHAIN THE SEED. Only g changes between rungs, so
        // H/Ae/Ai's hashes and the effective (primal_delta,
        // dual_mu) pair are already unchanged -- but qp_engine.h's
        // HOT-START REUSE condition (b) needs the seed working set
        // to equal the IMMEDIATELY PRECEDING solve's exit working
        // set, and re-seeding every rung from the original
        // kInfeasible solve fails it on rung 2 and after.
        // Measured: one K0 rebuild per rung (7 factorizations for
        // the ladder) against 1 with the chain. seed.x is zeroed
        // for the standing reason (see WARM SEEDING): it is the
        // engine's window CENTER.
        seed_elastic = qs_e;
        seed_elastic.x.setZero();
    }

    // THE EXHAUSTION SIGNATURE (see the note): the ladder is
    // spent and the tier has nothing to offer -- the relaxation
    // is still materially open, no admissible step reduces the
    // LINEARIZED violation, and the model promises no objective
    // decrease either.
    const Vec s_final =
        qs_e.status == QpStatus::kOptimal ? elastic.slack_violations(qs_e.x) : Vec::Zero(0);
    const double slack_l1 = s_final.size() > 0 ? s_final.lpNorm<1>() : 0.0;
    const Vec p_elastic =
        qs_e.status == QpStatus::kOptimal ? Vec(qs_e.x.head(qp.n())) : Vec::Zero(qp.n());
    // GUARDED LIKE promises_f/usable (fix round 1): on a non-kOptimal rung s_final
    // is Zero(0), so an unguarded `closed` would read TRUE off a VACUOUS zero --
    // inert at today's call site, a landmine for T3's consumer of the report.
    const bool closed = qs_e.status == QpStatus::kOptimal && slack_l1 <= opts.feas_tol;
    const bool reduced =
        qs_e.status == QpStatus::kOptimal && slack_l1 <= elastic.violation_l1 - opts.feas_tol;
    // EXACT ZERO, DELIBERATELY: see THE KNIFE-EDGE RULING above
    // for why a tolerance was considered and not added here.
    const bool promises_f =
        qs_e.status == QpStatus::kOptimal && predicted_decrease(qp, p_elastic) > 0.0;
    const bool usable = qs_e.status == QpStatus::kOptimal && (closed || reduced || promises_f);

    // ONE STRUCT-FILL, NO RE-DERIVATION: each field is the expression the driver
    // read in place. `elastic` and `qs_e` move out LAST, after p_elastic, the
    // flags and the three row fields have read them.
    ElasticLadderReport report;
    report.slack_l1 = slack_l1;
    report.p_elastic = p_elastic;
    report.closed = closed;
    report.reduced = reduced;
    report.promises_f = promises_f;
    report.usable = usable;
    report.qp_status = qs_e.status;
    report.qp_minor_iters = qs_e.counters.minor_iters;
    report.qp_factorizations = qs_e.counters.factorizations;
    report.step_norm = p_elastic.size() > 0 ? p_elastic.lpNorm<Eigen::Infinity>() : 0.0;
    report.rho_0 = rho_0;
    report.rho0_ceiling_hit = rho0_ceiling_hit;
    report.elastic = std::move(elastic);
    report.qs_e = std::move(qs_e);
    return report;
}

QpSolution certified_feasibility_fallback(QpEngine &engine, const QpProblem &qp, const NlpEval &ev,
                                          const QpSolution *seed,
                                          const IpqpInfeasibilityEvidence &evidence,
                                          const SolveOverrides &overrides, const SqpOptions &opts,
                                          double window, SqpCounters &out, SqpIterate &row,
                                          std::optional<ElasticLadderReport> &fallback_report,
                                          SqpFallbackVerdictTraceEvent &verdict, TraceSink *sink) {
    // VALIDATED AT THE BOUNDARY, on the same terms as the ladder's own (CLAUDE.md section 4).
    // P6's "no evidence CONTENT throws" is untouched: this is the window, not the block.
    if (!(window >= 0.0)) {
        throw std::invalid_argument(fmt::format(
            "certified_feasibility_fallback: window is {}, expected >= 0 (+inf legal)", window));
    }
    // THE EVIDENCE IS RECORDED FROM THE PARAMETER, before anything runs and whichever rung
    // answers: these two are telemetry on this row, never an input to the verdict (pin P7).
    row.ipqp_least_infeasible_primal = evidence.least_infeasible_primal;
    row.ipqp_farkas_corroborated = evidence.farkas_corroborated;
    fallback_report.reset();
    // ONE EVENT PER ENTRY, RESET FIRST: an entry that returns early still describes itself, and
    // a stale event from the previous major would describe the wrong one.
    verdict = SqpFallbackVerdictTraceEvent{};
    (void)ev;
    (void)seed;
    // RUNG B DIRECTLY, WITHOUT ENTERING RUNG A AT ALL: a block that never FIRED is not
    // evidence, so a caller carrying none gets W1's body exactly -- one cold walk, no
    // activation charged and NO counter in the partition (pin P5).
    if (!evidence.fired) {
        QpSolution cold = engine.solve(qp, overrides);
        emit_qp_mode_line(sink, IpqpTraceQpMode::kWalk, detail::trace_outcome_of(cold.status),
                          QpModeSite::kFallbackRungB, cold.counters.minor_iters);
        return cold;
    }

    // RUNG A -- THE ELASTIC QP, ALWAYS. Feasible by construction, so the suspicion is answered
    // by SOLVING something rather than by accumulating symptoms (this header's ELASTIC TIER
    // note). The EVIDENCE arm: the tier escaped with no QpSolution to map a seed from.
    const ElasticSeedSource elastic_seed_source{nullptr, &evidence};
    // EVERY ACTIVATION THIS ROUTE RAISED, whatever the engine then did with it (fix round 1):
    // the counter is named for activations, so the first attempt is charged here and the retry
    // below charges its own -- `elastic_activations == walk-route + elastic_from_ipqp_escape`.
    ++out.elastic_from_ipqp_escape;
    ElasticLadderReport report =
        run_elastic_ladder(engine, qp, elastic_seed_source, window, opts, out, std::nullopt, sink);
    // THE ENTRY'S CLAMP, kept across the retry below: an OVERRIDE placement reads UNCLAMPED, and
    // a clamp the engine then declined is the most interesting row this telemetry has.
    const bool entry_ceiling_hit = report.rho0_ceiling_hit;
    // THE RETRY AT THE FLOOR (W2 T5): a price the engine declined is a failed hint, not a
    // verdict on the reformulation, so W1's own penalty gets one attempt before rung B. It is
    // charged as the second activation it is, and the partition reads THIS ladder's outcome.
    if (report.qp_status != QpStatus::kOptimal && report.rho_0 > kElasticRhoInit) {
        ++out.elastic_floor_retries;
        ++out.elastic_from_ipqp_escape;
        verdict.floor_retry = true;
        report = run_elastic_ladder(engine, qp, elastic_seed_source, window, opts, out,
                                    kElasticRhoInit, sink);
    }
    // ONE BOOL ACROSS THE RETRY, so the row, the event and `elastic_rho0_ceiling_hits` all read
    // THIS ENTRY's placement rather than the surviving ladder's.
    report.rho0_ceiling_hit = entry_ceiling_hit || report.rho0_ceiling_hit;
    verdict.entered_rung_a = true;
    verdict.rho_0 = report.rho_0;
    verdict.rho0_ceiling_hit = report.rho0_ceiling_hit;
    verdict.qp_minor_iters = report.qp_minor_iters;
    verdict.qp_factorizations = report.qp_factorizations;
    // RUNG B -- THE COLD WALK, ON A RUNG A THE ENGINE DECLINED. The engine can still refuse a
    // feasible problem, and W1's body is what carries that refusal: the walk's solution goes
    // back UNCHANGED, with no report, so the routing chain behaves exactly as it did (pin P4).
    if (report.qp_status != QpStatus::kOptimal) {
        ++out.ipqp_fallback_rung_b;
        verdict.verdict = SqpFallbackVerdict::kRungB;
        QpSolution cold = engine.solve(qp, overrides);
        emit_qp_mode_line(sink, IpqpTraceQpMode::kWalk, detail::trace_outcome_of(cold.status),
                          QpModeSite::kFallbackRungB, cold.counters.minor_iters);
        return cold;
    }
    // RUNG A OWNS THE ANSWER, whichever verdict it carries: the three arms below are the
    // rung-A-owned side of the ENTRY partition, whose fourth arm is the rung-B return above.
    QpSolution qs =
        elastic_project(report.elastic, qp, report.qs_e, /*carry_multipliers=*/report.closed);
    if (!report.usable) {
        // THE EXHAUSTION CERTIFICATE, and the only kInfeasible this function SYNTHESIZES (rung
        // B's own passes through verbatim): the relaxation is still open at the ladder's
        // ceiling. The projected block travels for SHAPE only -- the status forbids taking it.
        qs.status = QpStatus::kInfeasible;
        verdict.verdict = SqpFallbackVerdict::kExhausted;
    } else if (report.closed) {
        // A FALSE SUSPICION, and the counter that grades section 6.3's detector: the relaxation
        // shut, so this answer IS the unrelaxed subproblem's.
        ++out.ipqp_suspicion_disproved;
        verdict.verdict = SqpFallbackVerdict::kDisproved;
    } else {
        verdict.verdict = SqpFallbackVerdict::kRelaxed;
    }
    // THE RUNGS' COUNTERS ARE ALREADY IN `out` -- run_elastic_ladder folds every one of them
    // there -- so the returned solution carries NONE: the driver accumulates whatever it is
    // handed, and a second copy would double-charge this major's minor iters and factorizations.
    qs.counters = QpCounters{};
    fallback_report = std::move(report);
    return qs;
}

} // namespace hven::solvers
