// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_ipqp_trace.cpp -- M6 W1 task 8: the IPQP tier's ledger-record pin and
// the seven schema v0 event-struct pins, via a small recording IpqpTraceSink.
// See .superpowers/w1-t8-report.md FIX ROUND 1 for the falsifiability evidence.

#include <algorithm>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/ipqp_trace.h>
#include <hven/drivers/sqp_driver.h>

#include "support/hs_problems.h"

namespace hven::solvers {
namespace {

// --- fixture helpers, repeated from test_ipqp_engine.cpp / test_ipqp_
// certification.cpp for the same reason test_ledger.cpp repeats
// test_qp_engine.cpp's: this file stays self-contained. -------------------

SpMatRM dense_upper(const std::vector<std::vector<double>> &a) {
    const Index n = static_cast<Index>(a.size());
    SpMatRM m(n, n);
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        for (Index j = i; j < n; ++j) {
            if (a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] != 0.0) {
                t.emplace_back(static_cast<int>(i), static_cast<int>(j),
                               a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
            }
        }
    }
    m.setFromTriplets(t.begin(), t.end());
    m.makeCompressed();
    return m;
}

SpMatRM dense_rows(const std::vector<std::vector<double>> &a, Index cols) {
    const Index rows = static_cast<Index>(a.size());
    SpMatRM m(rows, cols);
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < rows; ++i) {
        for (Index j = 0; j < cols; ++j) {
            if (a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] != 0.0) {
                t.emplace_back(static_cast<int>(i), static_cast<int>(j),
                               a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
            }
        }
    }
    m.setFromTriplets(t.begin(), t.end());
    m.makeCompressed();
    return m;
}

Vec vec(const std::vector<double> &v) {
    Vec out(static_cast<Index>(v.size()));
    for (std::size_t i = 0; i < v.size(); ++i) {
        out(static_cast<Index>(i)) = v[i];
    }
    return out;
}

/// A strictly convex box-constrained QP: min |x - (1, 2)|^2 over [lo, up].
/// mi == me == 0 by construction -- the fixture EVERY exact res_p==0 pin
/// below relies on.
QpProblem box_qp(double lo, double up) {
    QpProblem qp;
    qp.H = dense_upper({{2.0, 0.0}, {0.0, 2.0}});
    qp.g = vec({-2.0, -4.0});
    qp.Ae = dense_rows({}, 2);
    qp.Ai = dense_rows({}, 2);
    qp.be = Vec(0);
    qp.bi = Vec(0);
    qp.lower = vec({lo, lo});
    qp.upper = vec({up, up});
    return qp;
}

QpOptions tight_opts() {
    QpOptions o;
    o.tr_radius = std::numeric_limits<double>::infinity();
    return o;
}

/// A strictly convex QP with both row blocks (test_ipqp_certification.cpp's
/// own `convex_qp`), the kStall fixture's problem half.
QpProblem convex_qp() {
    QpProblem qp;
    qp.H = dense_upper({{2.0, 0.0}, {0.0, 2.0}});
    qp.g = vec({-2.0, -4.0});
    qp.Ae = dense_rows({{1.0, -1.0}}, 2);
    qp.be = vec({0.5});
    qp.Ai = dense_rows({{1.0, 1.0}}, 2);
    qp.bi = vec({1.0});
    qp.lower = vec({-10.0, -10.0});
    qp.upper = vec({10.0, 10.0});
    return qp;
}

/// test_ipqp_certification.cpp's own: a crawl-rate `ipqp_tau` forces the
/// early-stall detector to fire on `convex_qp()` (a genuine `kStall`).
IpqpOptions crawling_opts(double tau) {
    IpqpOptions io;
    io.ipqp_tau = tau;
    return io;
}

/// A one-variable infeasible QP (test_ipqp_certification.cpp's own):
/// `x <= -5` and `x >= 5`, the kInfeasibleSuspect fixture's problem half.
QpProblem infeasible_scalar_qp() {
    QpProblem qp;
    qp.H = dense_upper({{1.0}});
    qp.g = vec({0.0});
    qp.Ae = dense_rows({}, 1);
    qp.be = Vec(0);
    qp.Ai = dense_rows({{1.0}, {-1.0}}, 1);
    qp.bi = vec({-5.0, -5.0});
    qp.lower = vec({-10.0});
    qp.upper = vec({10.0});
    return qp;
}

/// A recording sink: every event is appended to its own vector, in arrival
/// order, so a test can inspect exactly what the emit sites built.
class RecordingTraceSink : public IpqpTraceSink {
  public:
    std::vector<IpqpTraceIterEvent> iters;
    std::vector<IpqpTraceRegEvent> regs;
    std::vector<IpqpTraceRestartEvent> restarts;
    std::vector<IpqpTraceRouteEvent> routes;
    std::vector<IpqpTraceCertifyEvent> certifies;
    std::vector<IpqpTraceEscapeEvent> escapes;
    std::vector<QpModeTraceEvent> modes;
    std::vector<SqpFallbackVerdictTraceEvent> fallbacks;

    void on_ipqp_iter(const IpqpTraceIterEvent &e) override { iters.push_back(e); }
    void on_ipqp_reg(const IpqpTraceRegEvent &e) override { regs.push_back(e); }
    void on_ipqp_restart(const IpqpTraceRestartEvent &e) override { restarts.push_back(e); }
    void on_ipqp_route(const IpqpTraceRouteEvent &e) override { routes.push_back(e); }
    void on_ipqp_certify(const IpqpTraceCertifyEvent &e) override { certifies.push_back(e); }
    void on_ipqp_escape(const IpqpTraceEscapeEvent &e) override { escapes.push_back(e); }
    void on_qp_mode(const QpModeTraceEvent &e) override { modes.push_back(e); }
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &e) override {
        fallbacks.push_back(e);
    }
};

// R3: the escape evidence block is a verbatim copy of IpqpResult's own two
// evidence structs, so every scalar field is compared here -- a dropped or
// mis-copied field fails one of these, not a `fired`-only check.
void expect_stall_evidence_eq(const IpqpStallEvidence &a, const IpqpStallEvidence &b) {
    EXPECT_EQ(a.fired, b.fired);
    EXPECT_EQ(a.window, b.window);
    EXPECT_DOUBLE_EQ(a.mu_ratio, b.mu_ratio);
    EXPECT_DOUBLE_EQ(a.residual_improvement, b.residual_improvement);
    EXPECT_DOUBLE_EQ(a.min_alpha, b.min_alpha);
    EXPECT_DOUBLE_EQ(a.max_step_alpha, b.max_step_alpha);
}

void expect_vec_eq(const Vec &a, const Vec &b) {
    ASSERT_EQ(a.size(), b.size());
    if (a.size() > 0) {
        EXPECT_TRUE((a.array() == b.array()).all());
    }
}

void expect_infeasibility_evidence_eq(const IpqpInfeasibilityEvidence &a,
                                      const IpqpInfeasibilityEvidence &b) {
    EXPECT_EQ(a.fired, b.fired);
    EXPECT_EQ(a.exhaustion_route, b.exhaustion_route);
    EXPECT_EQ(a.window, b.window);
    EXPECT_DOUBLE_EQ(a.primal_start, b.primal_start);
    EXPECT_DOUBLE_EQ(a.primal_end, b.primal_end);
    EXPECT_DOUBLE_EQ(a.primal_improvement, b.primal_improvement);
    EXPECT_DOUBLE_EQ(a.dual_norm_start, b.dual_norm_start);
    EXPECT_DOUBLE_EQ(a.dual_norm_end, b.dual_norm_end);
    EXPECT_DOUBLE_EQ(a.dual_growth, b.dual_growth);
    EXPECT_DOUBLE_EQ(a.dual_step_growth, b.dual_step_growth);
    EXPECT_EQ(a.farkas_corroborated, b.farkas_corroborated);
    EXPECT_DOUBLE_EQ(a.farkas_residual, b.farkas_residual);
    EXPECT_DOUBLE_EQ(a.farkas_gap, b.farkas_gap);
    EXPECT_DOUBLE_EQ(a.least_infeasible_primal, b.least_infeasible_primal);
    EXPECT_DOUBLE_EQ(a.least_infeasible_mu, b.least_infeasible_mu);
    expect_vec_eq(a.least_infeasible_x, b.least_infeasible_x);
    expect_vec_eq(a.least_infeasible_s, b.least_infeasible_s);
    expect_vec_eq(a.least_infeasible_lambda_i, b.least_infeasible_lambda_i);
    expect_vec_eq(a.least_infeasible_zl, b.least_infeasible_zl);
    expect_vec_eq(a.least_infeasible_zu, b.least_infeasible_zu);
}

// ===========================================================================
// DELIVERABLE 3 -- the ledger-record pin.
// ===========================================================================

TEST(IpqpTrace, LedgerRecordsOneSolveRecordPerSubproblemWithTheIpqpPrefix) {
    IpqpEngine engine(tight_opts());
    Ledger ledger;
    engine.attach_ledger(&ledger, "ipqp");

    const IpqpResult ok = engine.solve(box_qp(-1.0, 1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(ok.status, QpStatus::kOptimal);

    QpProblem declined = box_qp(-1.0, 1.0);
    declined.lower(1) = 0.25;
    declined.upper(1) = 0.25;
    const IpqpResult dec = engine.solve(declined, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_TRUE(dec.declined_pinned) << "the fixture must decline, or the second row is vacuous";

    ASSERT_EQ(ledger.records().size(), 2u) << "one row per subproblem, decline included (I9)";
    const SolveRecord &r0 = ledger.records()[0];
    const SolveRecord &r1 = ledger.records()[1];
    EXPECT_EQ(r0.label, "ipqp0");
    EXPECT_EQ(r1.label, "ipqp1");
    EXPECT_FALSE(r0.warm);
    EXPECT_EQ(r0.status, ok.status);
    EXPECT_EQ(r0.counters.minor_iters, ok.counters.ipqp_iters);
    EXPECT_EQ(r0.counters.factorizations, ok.counters.ipqp_factorizations);
    EXPECT_EQ(r0.counters.symbolic_analyses, ok.counters.ipqp_symbolic_analyses);
    EXPECT_EQ(r1.status, dec.status);
    EXPECT_EQ(r1.counters.minor_iters, 0) << "the tier never ran on a decline";
    EXPECT_EQ(r1.counters.factorizations, 0);
    EXPECT_EQ(r1.counters.symbolic_analyses, 0)
        << "the tier never ran on a decline (Codex ledger note)";

    // NON-VACUITY: detached, a third solve must not grow this ledger.
    engine.attach_ledger(nullptr, "");
    (void)engine.solve(box_qp(-1.0, 1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(ledger.records().size(), 2u);
}

// ===========================================================================
// DELIVERABLE 4 -- the seven event-struct pins.
// ===========================================================================

// R3: exact checks throughout -- solve/major/it; alpha_p/d fold-min against
// the counters; last event's rho/delta against IpqpResult's; res_p == 0
// (mi=me=0); inertia/perturbed against the real MKL read. Report has detail.
TEST(IpqpTrace, IterEventsCarryTheDriverSetMajorAndCrossCheckAgainstTheResult) {
    IpqpEngine tier(tight_opts());
    RecordingTraceSink sink;
    tier.attach_trace(&sink);
    EXPECT_EQ(tier.last_trace_solve_id(), 0) << "no solve has run with the sink attached yet";

    const IpqpResult first =
        tier.solve(box_qp(-1.0, 1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    ASSERT_FALSE(sink.iters.empty()) << "a converged solve took at least one iteration";
    EXPECT_EQ(tier.last_trace_solve_id(), 0);

    double min_alpha_p = std::numeric_limits<double>::infinity();
    double min_alpha_d = std::numeric_limits<double>::infinity();
    for (const IpqpTraceIterEvent &ev : sink.iters) {
        EXPECT_EQ(ev.solve, 0);
        EXPECT_EQ(ev.major, 0) << "no driver set it -- the engine's own default";
        EXPECT_GT(ev.alpha_p, 0.0);
        EXPECT_LE(ev.alpha_p, 1.0);
        EXPECT_GT(ev.alpha_d, 0.0);
        EXPECT_LE(ev.alpha_d, 1.0);
        EXPECT_GT(ev.sigma, 0.0) << "Mehrotra's centering parameter, defined in (0, 1]";
        EXPECT_LE(ev.sigma, 1.0);
        EXPECT_GT(ev.mu, 0.0);
        EXPECT_GT(ev.res_d, 0.0) << "never exactly 0 on a real solve -- catches a zero-fill";
        EXPECT_GT(ev.res_c, 0.0);
        EXPECT_NE(ev.res_d, ev.res_c) << "non-vacuity only -- the exact cold-start pin below is "
                                         "what actually catches a res_d/res_c swap";
        EXPECT_EQ(ev.res_p, 0.0) << "box_qp has mi == me == 0, so primal_eq/primal_iq are 0";
        EXPECT_EQ(ev.facts, "");
        ASSERT_TRUE(ev.inertia.has_value()) << "a real MKL read observes on this fixture";
        EXPECT_EQ((*ev.inertia)[0], 2) << "n=2, positive definite, mi=0";
        EXPECT_EQ((*ev.inertia)[1], 0);
        EXPECT_EQ((*ev.inertia)[2], 0);
#ifndef USE_ACCELERATE_SPARSE
        EXPECT_TRUE(ev.zero_derived) << "MKL Pardiso always derives n_zero by subtraction";
#endif
        ASSERT_TRUE(ev.perturbed.has_value()) << "MKL always reports a pivot count";
        EXPECT_EQ(*ev.perturbed, 0) << "well-conditioned fixture, no perturbation";
        min_alpha_p = std::min(min_alpha_p, ev.alpha_p);
        min_alpha_d = std::min(min_alpha_d, ev.alpha_d);
    }
    EXPECT_DOUBLE_EQ(min_alpha_p, first.counters.ipqp_alpha_p_min);
    EXPECT_DOUBLE_EQ(min_alpha_d, first.counters.ipqp_alpha_d_min);
    // Cold-start duals make res_c bit-exact against ipqp_init_mu here (x0 ==
    // box centre, unit distances); a res_d/res_c swap breaks this. See
    // .superpowers/w1-t8-report.md FIX ROUND 3.
    EXPECT_DOUBLE_EQ(sink.iters.front().res_c, IpqpOptions{}.ipqp_init_mu)
        << "cold-start complementarity is exactly mu_0 by construction on this fixture";
    // `it` is a dense 1..N sequence, one entry per completed iteration.
    for (std::size_t i = 0; i < sink.iters.size(); ++i) {
        EXPECT_EQ(sink.iters[i].it, static_cast<Index>(i) + 1);
    }
    EXPECT_EQ(sink.iters.back().it, first.counters.ipqp_iters);
    EXPECT_DOUBLE_EQ(sink.iters.back().rho, first.rho);
    EXPECT_DOUBLE_EQ(sink.iters.back().delta, first.delta);
    // IpqpResult is one more residual/mu probe past the last emitted event,
    // so it can only be MORE converged -- exact ordering, not a bound
    // picked to pass. See w1-t8-report.md FIX ROUND 2 for the trace.
    EXPECT_LT(first.mu, sink.iters.back().mu);
    EXPECT_LT(first.residuals.stationarity, sink.iters.back().res_d);
    EXPECT_LT(first.residuals.complementarity, sink.iters.back().res_c);

    tier.set_trace_major(7);
    const IpqpResult second =
        tier.solve(box_qp(-1.0, 1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(second.status, QpStatus::kOptimal);
    EXPECT_EQ(tier.last_trace_solve_id(), 1);
    std::vector<IpqpTraceIterEvent> second_events;
    for (const IpqpTraceIterEvent &ev : sink.iters) {
        if (ev.solve == 1) {
            EXPECT_EQ(ev.major, 7);
            second_events.push_back(ev);
        }
    }
    ASSERT_FALSE(second_events.empty());
    EXPECT_DOUBLE_EQ(second_events.back().rho, second.rho);
    EXPECT_DOUBLE_EQ(second_events.back().delta, second.delta);
    EXPECT_LT(second.mu, second_events.back().mu);
    EXPECT_LT(second.residuals.stationarity, second_events.back().res_d);
    EXPECT_LT(second.residuals.complementarity, second_events.back().res_c);
}

// H = diag(2, -1000) on [-10, 10]^2 (test_ipqp_engine.cpp's own indefinite
// fixture): arms the ladder and ends in a genuine kNumerical escape, so it
// doubles as the escape-event fixture below too.
QpProblem indefinite_qp() {
    QpProblem qp = box_qp(-10.0, 10.0);
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1000.0}});
    return qp;
}

TEST(IpqpTrace, RegEventsPartitionIntoTheLadderClimbAndTheScheduleDecrease) {
    IpqpEngine tier(tight_opts());
    RecordingTraceSink sink;
    tier.attach_trace(&sink);
    const IpqpResult r = tier.solve(indefinite_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_GT(r.counters.ipqp_reg_increases, 0) << "the fixture must arm the ladder";

    // R3: rho/delta bounded against IpqpOptions' own bands, not just >= 0.
    // A down event's band is tight (rho_sched/delta_sched verbatim); an up
    // event adds the ladder's own escalation, so its band is twice as wide.
    const IpqpOptions defaults;
    Index up_count = 0;
    Index down_count = 0;
    for (const IpqpTraceRegEvent &ev : sink.regs) {
        EXPECT_GT(ev.delta, 0.0);
        EXPECT_LE(ev.delta, defaults.ipqp_reg_max);
        if (ev.dir == IpqpTraceRegDir::kUp) {
            ++up_count;
            // A real MKL run never reports evidence failure on this legal
            // fixture (docs/testing.md: that path is a seam-only injection).
            EXPECT_EQ(ev.reason, IpqpTraceRegReason::kInertia);
            EXPECT_GT(ev.rho, 0.0);
            EXPECT_LE(ev.rho, 2.0 * defaults.ipqp_reg_max);
        } else {
            ++down_count;
            EXPECT_EQ(ev.reason, IpqpTraceRegReason::kAccept);
            EXPECT_GE(ev.rho, defaults.ipqp_reg_floor);
            EXPECT_LE(ev.rho, defaults.ipqp_reg_max);
        }
    }
    EXPECT_EQ(up_count, r.counters.ipqp_reg_increases)
        << "one ipqp.reg(up) event per ladder rung the counter charges";
    EXPECT_EQ(down_count, r.counters.ipqp_reg_decreases)
        << "one ipqp.reg(down) event per schedule decrease the counter charges";
}

TEST(IpqpTrace, RestartEventReportsTheGradeAndTheRepairFactsAsMeasured) {
    IpqpEngine tier(tight_opts());
    RecordingTraceSink sink;
    tier.attach_trace(&sink);

    const QpProblem qp = box_qp(-1.0, 1.0);
    const IpqpResult cold = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(cold.status, QpStatus::kOptimal);
    ASSERT_EQ(sink.restarts.size(), 1u);
    EXPECT_EQ(sink.restarts[0].grade, IpqpTraceRestartGrade::kCold);
    EXPECT_FALSE(sink.restarts[0].repaired);
    EXPECT_FALSE(sink.restarts[0].abandoned);
    // Cold-start determinism (ipqp_engine.cpp's own `cold_start` comment):
    // mu0 == ipqp_init_mu exactly, and no seed means no payload.
    EXPECT_EQ(sink.restarts[0].mu0, IpqpOptions{}.ipqp_init_mu);
    EXPECT_EQ(sink.restarts[0].mu_payload, 0.0);

    // ALIASING: `warm_carry()` points INTO the engine, and solve() commits
    // new state through that same pointer -- read `mu`/`zl`/`zu` before that
    // call, never after (ipqp_engine.cpp's own "COPIED FIRST" comment).
    const IpqpSeed *carry = tier.warm_carry();
    ASSERT_NE(carry, nullptr) << "a finite converged solve arms the cross-major carry";
    const double carry_mu_before = carry->mu;
    const double carry_sum_z_before = carry->zl.sum() + carry->zu.sum();
    const IpqpResult warm = tier.solve(qp, carry, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    ASSERT_EQ(sink.restarts.size(), 2u);
    const IpqpTraceRestartEvent &rev = sink.restarts[1];
    EXPECT_NE(rev.grade, IpqpTraceRestartGrade::kCold) << "a real seed was offered";
    EXPECT_EQ(rev.repaired, warm.counters.ipqp_restart_repairs != 0);
    EXPECT_DOUBLE_EQ(rev.mu_payload, carry_mu_before);
    EXPECT_GE(rev.mu0, IpqpOptions{}.ipqp_min_mu);
    EXPECT_LE(rev.mu0, IpqpOptions{}.ipqp_init_mu);
    // R3/R4: shift_p/shift_d recomputed from the SAY formula, not merely
    // bounded -- npd=4 and sum_d=upper-lower are dimension-derived; sum_z
    // uses the seed's raw prices. See w1-t8-report.md FIX ROUND 2's derivation.
    constexpr double kSayFraction = detail::kIpqpSayTargetFraction; // 0.5, ipqp_engine.h
    const double npd = 4.0;
    const double sum_d = 2.0 * (qp.upper(0) - qp.lower(0)); // both sides finite on both variables
    const double predicted_shift_d = kSayFraction * rev.mu0 * npd / sum_d;
    EXPECT_DOUBLE_EQ(rev.shift_d, predicted_shift_d);
    if (carry_sum_z_before > 0.0) {
        const double predicted_shift_p = kSayFraction * rev.mu0 * npd / carry_sum_z_before;
        EXPECT_DOUBLE_EQ(rev.shift_p, predicted_shift_p)
            << "fails if the repair actually moved a price -- see this block's own note";
    }
    EXPECT_NE(rev.shift_p, rev.shift_d) << "R4: distinct, not the shared shift_max";
    EXPECT_EQ(rev.adopted, warm.counters.ipqp_mu_adopted != 0);
    EXPECT_EQ(rev.abandoned, warm.counters.ipqp_warm_restart_abandoned != 0);
}

TEST(IpqpTrace, CertifyEventReportsTheFinalReadAndFiresOnlyWhenOneWasTaken) {
    const QpProblem qp = box_qp(-1.0, 1.0);

    IpqpEngine tier(tight_opts());
    RecordingTraceSink sink;
    tier.attach_trace(&sink);
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    ASSERT_EQ(r.counters.ipqp_final_inertia_read, 0);
    ASSERT_EQ(sink.certifies.size(), 1u);
    EXPECT_EQ(sink.certifies[0].final_inertia, IpqpTraceFinalInertia::kOk);
    EXPECT_EQ(sink.certifies[0].downgraded, r.certificate_downgraded);

    // GUARD, NON-VACUOUS: read declined -> read==3, no slot in the schema's
    // three-value enum, so the event must not fire (docs/testing.md).
    IpqpEngine tier2(tight_opts());
    RecordingTraceSink sink2;
    tier2.attach_trace(&sink2);
    IpqpOptions io2;
    io2.ipqp_require_final_inertia = false;
    const IpqpResult r2 = tier2.solve(qp, nullptr, io2, SolveOverrides{});
    ASSERT_EQ(r2.counters.ipqp_final_inertia_read, 3);
    EXPECT_TRUE(sink2.certifies.empty());
}

TEST(IpqpTrace, EscapeEventReportsTheReasonAndFiresOnlyOnAGenuineEscape) {
    IpqpEngine tier(tight_opts());
    RecordingTraceSink sink;
    tier.attach_trace(&sink);
    const IpqpResult r = tier.solve(indefinite_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.escape_reason, IpqpEscape::kNumerical);
    ASSERT_EQ(sink.escapes.size(), 1u);
    EXPECT_EQ(sink.escapes[0].reason, IpqpTraceEscapeReason::kNumerical);
    // kNumerical carries neither evidence block.
    EXPECT_FALSE(sink.escapes[0].evidence.stall.fired);
    EXPECT_FALSE(sink.escapes[0].evidence.infeasibility.fired);
    EXPECT_TRUE(sink.certifies.empty()) << "M5: no certify event on an escaped solve";

    // NON-VACUITY: a clean convex solve escapes nothing, so nothing fires.
    IpqpEngine tier2(tight_opts());
    RecordingTraceSink sink2;
    tier2.attach_trace(&sink2);
    const IpqpResult ok = tier2.solve(box_qp(-1.0, 1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(ok.escape_reason, IpqpEscape::kNone);
    EXPECT_TRUE(sink2.escapes.empty());
}

// R3/R2: a genuine kStall and a genuine kInfeasibleSuspect, each checked
// field-by-field against the SAME IpqpResult the event was built from, not
// merely which reason fired.
TEST(IpqpTrace, EscapeEventCarriesTheFullEvidenceBlockVerbatim) {
    {
        IpqpEngine tier(tight_opts());
        RecordingTraceSink sink;
        tier.attach_trace(&sink);
        const IpqpResult r =
            tier.solve(convex_qp(), nullptr, crawling_opts(1e-3), SolveOverrides{});
        // T10b MOVED THIS CRAWL RATE TO 1e-4 AND FIX ROUND 1 MOVES IT BACK: on the REPAIRED
        // fixture the second window improves 0.18% at the shipped `ipqp_init_mu` (0.25% at
        // 0.1), both under 6.2's 1% floor. `.superpowers/w1-t10b-fix1-report.md` item A.
        ASSERT_EQ(r.escape_reason, IpqpEscape::kStall);
        // AND ITS COST, so a drift back toward the budget escape fails here rather than
        // silently re-reading the same reason off a different trajectory (Codex, medium 1).
        EXPECT_EQ(r.counters.ipqp_iters, 10);
        ASSERT_EQ(sink.escapes.size(), 1u);
        EXPECT_EQ(sink.escapes[0].reason, IpqpTraceEscapeReason::kStall);
        ASSERT_TRUE(r.stall_evidence.fired) << "the fixture must actually fire, or this is vacuous";
        expect_stall_evidence_eq(sink.escapes[0].evidence.stall, r.stall_evidence);
        expect_infeasibility_evidence_eq(sink.escapes[0].evidence.infeasibility,
                                         r.infeasibility_evidence);
    }
    {
        IpqpEngine tier(tight_opts());
        RecordingTraceSink sink;
        tier.attach_trace(&sink);
        const IpqpResult r =
            tier.solve(infeasible_scalar_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(r.escape_reason, IpqpEscape::kInfeasibleSuspect);
        ASSERT_EQ(sink.escapes.size(), 1u);
        EXPECT_EQ(sink.escapes[0].reason, IpqpTraceEscapeReason::kInfeasibleSuspect);
        ASSERT_TRUE(r.infeasibility_evidence.fired) << "the fixture must actually fire, or vacuous";
        expect_stall_evidence_eq(sink.escapes[0].evidence.stall, r.stall_evidence);
        expect_infeasibility_evidence_eq(sink.escapes[0].evidence.infeasibility,
                                         r.infeasibility_evidence);
    }
}

// ipqp.route / qp.mode are DRIVER-emitted, so this pin runs the driver, not the bare engine.
// Three measured HS members take three different section 2.3 rows -- HS6 refines, HS3 reaches
// SSN via a refusal, HS10 escapes -- covering all three `to` values and `qp.mode` outcomes.
TEST(IpqpTrace, DriverRouteAndQpModeEventsMatchTheRoutingCounters) {
    RecordingTraceSink sink;
    Index refine_accepted = 0;
    Index to_ssn = 0;
    Index to_walk_real = 0; // ipqp_to_walk minus the declines, which fire no event.
    Index total_iters = 0;
    Index total_uncertain = 0;

    for (int number : {6, 3, 10}) {
        auto p = test_support::make_hs(number);
        SqpOptions o;
        o.qp_mode = QpMode::kIpm;
        o.max_iter = 60;
        SqpDriver driver(o);
        driver.attach_trace(&sink);
        const SqpSolution s = driver.solve(*p.model);
        const IpqpCounters &c = s.counters.ipqp;
        refine_accepted += c.ipqp_refine_accepted;
        to_ssn += c.ipqp_to_ssn;
        to_walk_real += c.ipqp_to_walk - c.ipqp_declined_pinned;
        total_iters += c.ipqp_iters;
        total_uncertain += c.ipqp_face_uncertain;
    }
    ASSERT_GT(refine_accepted, 0) << "HS6 must refine, or the refine row is vacuous";
    ASSERT_GT(to_ssn, 0) << "HS3 must reach the SSN warm grade, or the ssn row is vacuous";
    ASSERT_GT(to_walk_real, 0) << "HS10 must escape, or the walk row is vacuous";

    Index refine_events = 0, ssn_events = 0, walk_events = 0;
    Index uncertain_sum = 0;
    Index face_rows_sum = 0, face_bounds_sum = 0;
    for (const IpqpTraceRouteEvent &ev : sink.routes) {
        switch (ev.to) {
        case IpqpTraceRouteTo::kRefine:
            ++refine_events;
            break;
        case IpqpTraceRouteTo::kSsn:
            ++ssn_events;
            break;
        case IpqpTraceRouteTo::kWalk:
            ++walk_events;
            break;
        }
        EXPECT_GE(ev.face_rows, 0);
        EXPECT_GE(ev.face_bounds, 0);
        uncertain_sum += ev.uncertain;
        face_rows_sum += ev.face_rows;
        face_bounds_sum += ev.face_bounds;
    }
    // No aggregated counter exists to fold-sum against (unlike below) --
    // `ires.ineq_active`/`bound_state` never reach any caller. NON-VACUITY
    // is what's reachable: a mutation that always reports 0 fails this.
    EXPECT_GT(face_bounds_sum, 0) << "HS6/HS10 box-constrain variables -- some face must show one";
    EXPECT_GT(face_rows_sum, 0) << "HS3's inequality rows put some face_rows > 0 too";
    EXPECT_EQ(refine_events, refine_accepted);
    EXPECT_EQ(ssn_events, to_ssn);
    EXPECT_EQ(walk_events, to_walk_real);
    // R3: an exact fold-sum against the aggregated counter, not a per-event
    // bound -- every `uncertain` value is accounted for, not merely >= 0.
    EXPECT_EQ(uncertain_sum, total_uncertain);
    EXPECT_EQ(sink.routes.size(), sink.modes.size())
        << "the two are emitted together, one pair per consulted subproblem";

    Index optimal_count = 0, routed_count = 0, escaped_count = 0;
    Index iters_sum = 0;
    for (const QpModeTraceEvent &ev : sink.modes) {
        EXPECT_EQ(ev.mode, IpqpTraceQpMode::kIpqp);
        EXPECT_EQ(ev.facts, "");
        EXPECT_GE(ev.iters, 0);
        iters_sum += ev.iters;
        if (ev.outcome == IpqpTraceOutcome::kOptimal) {
            ++optimal_count;
        } else if (ev.outcome == IpqpTraceOutcome::kRouted) {
            ++routed_count;
        } else {
            ++escaped_count;
        }
    }
    EXPECT_EQ(optimal_count, refine_accepted);
    EXPECT_EQ(routed_count, to_ssn);
    EXPECT_EQ(escaped_count, to_walk_real);
    // R3: an exact fold-sum against the aggregated ipqp_iters counter.
    EXPECT_EQ(iters_sum, total_iters);
}

// Fix round 3 -- face_rows/face_bounds swap-falsifiability; see
// .superpowers/w1-t8-report.md FIX ROUND 3.

/// n=2, mi=0, box [-1,1]^2, unconstrained min (3,3) -- both bounds bind.
/// face_rows is structurally 0 (no inequality row exists).
class BoundActiveOnlyModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return x(0) * x(0) - 6.0 * x(0) + x(1) * x(1) - 6.0 * x(1);
    }
    Vec eval_grad(const Vec &x) const override { return vec({2.0 * x(0) - 6.0, 2.0 * x(1) - 6.0}); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        return obj_scale * dense_upper({{2.0, 0.0}, {0.0, 2.0}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = vec({-1.0, -1.0});
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = vec({1.0, 1.0});
        return u;
    }
    Vec start_point() const override { return Vec::Zero(2); }
};

/// n=2, mi=1, no finite bound anywhere -- face_bounds is structurally 0.
/// x1+x2>=1 binds at x*=(0.5,0.5); unconstrained min (0,0) violates it.
class RowActiveOnlyModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return x(0) * x(0) + x(1) * x(1); }
    Vec eval_grad(const Vec &x) const override { return vec({2.0 * x(0), 2.0 * x(1)}); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return vec({1.0 - x(0) - x(1)}); }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        return obj_scale * dense_upper({{2.0, 0.0}, {0.0, 2.0}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        std::vector<Eigen::Triplet<double>> t{{0, 0, -1.0}, {0, 1, -1.0}};
        j.setFromTriplets(t.begin(), t.end());
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        static const Vec l = vec({-1e20, -1e20}); // "no bound", ipqp_math.h's kIpqpInfBound
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = vec({1e20, 1e20});
        return u;
    }
    Vec start_point() const override { return vec({1.0, 1.0}); } // feasible: 1-1-1 <= 0
};

TEST(IpqpTrace, RouteEventFaceRowsAndFaceBoundsAreSwapFalsifiable) {
    {
        RecordingTraceSink sink;
        SqpOptions o;
        o.qp_mode = QpMode::kIpm;
        SqpDriver driver(o);
        driver.attach_trace(&sink);
        BoundActiveOnlyModel model;
        const SqpSolution s = driver.solve(model);
        ASSERT_EQ(s.status, SqpStatus::kOptimal);
        ASSERT_FALSE(sink.routes.empty()) << "at least one subproblem was consulted";
        Index face_rows_sum = 0, face_bounds_sum = 0;
        for (const IpqpTraceRouteEvent &ev : sink.routes) {
            face_rows_sum += ev.face_rows;
            face_bounds_sum += ev.face_bounds;
        }
        EXPECT_EQ(face_rows_sum, 0) << "mi == 0 here -- no inequality row can ever be active";
        EXPECT_GT(face_bounds_sum, 0) << "both variables bind their bound -- non-vacuous";
    }
    {
        RecordingTraceSink sink;
        SqpOptions o;
        o.qp_mode = QpMode::kIpm;
        SqpDriver driver(o);
        driver.attach_trace(&sink);
        RowActiveOnlyModel model;
        const SqpSolution s = driver.solve(model);
        ASSERT_EQ(s.status, SqpStatus::kOptimal);
        ASSERT_FALSE(sink.routes.empty()) << "at least one subproblem was consulted";
        Index face_rows_sum = 0, face_bounds_sum = 0;
        for (const IpqpTraceRouteEvent &ev : sink.routes) {
            face_rows_sum += ev.face_rows;
            face_bounds_sum += ev.face_bounds;
        }
        EXPECT_EQ(face_bounds_sum, 0) << "no finite bound anywhere -- bound_state is kFree always";
        EXPECT_GT(face_rows_sum, 0) << "the inequality row binds -- non-vacuous";
    }
}

} // namespace
} // namespace hven::solvers
