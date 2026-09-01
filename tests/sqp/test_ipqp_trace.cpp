// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_ipqp_trace.cpp -- M6 W1 task 8: the IPQP tier's ledger-record pin and
// the seven schema v0 event-struct pins (the W4 hook). No pin here reads a
// serialized trace (spec section 7's own scoping) -- every assertion reads
// the STRUCT an emit site built, via a small recording IpqpTraceSink.

#include <vector>

#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/ipqp_trace.h>
#include <hven/drivers/sqp_driver.h>

#include "support/hs_problems.h"

namespace hven::solvers {
namespace {

// --- fixture helpers, repeated from test_ipqp_engine.cpp for the same
// reason test_ledger.cpp repeats test_qp_engine.cpp's: this file stays
// self-contained. ---------------------------------------------------------

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

    void on_ipqp_iter(const IpqpTraceIterEvent &e) override { iters.push_back(e); }
    void on_ipqp_reg(const IpqpTraceRegEvent &e) override { regs.push_back(e); }
    void on_ipqp_restart(const IpqpTraceRestartEvent &e) override { restarts.push_back(e); }
    void on_ipqp_route(const IpqpTraceRouteEvent &e) override { routes.push_back(e); }
    void on_ipqp_certify(const IpqpTraceCertifyEvent &e) override { certifies.push_back(e); }
    void on_ipqp_escape(const IpqpTraceEscapeEvent &e) override { escapes.push_back(e); }
    void on_qp_mode(const QpModeTraceEvent &e) override { modes.push_back(e); }
};

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

    // NON-VACUITY: detached, a third solve must not grow this ledger.
    engine.attach_ledger(nullptr, "");
    (void)engine.solve(box_qp(-1.0, 1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(ledger.records().size(), 2u);
}

// ===========================================================================
// DELIVERABLE 4 -- the seven event-struct pins.
// ===========================================================================

TEST(IpqpTrace, IterEventsAreNumberedPerAttachAndCarryTheDriverSetMajor) {
    IpqpEngine tier(tight_opts());
    RecordingTraceSink sink;
    tier.attach_trace(&sink);
    EXPECT_EQ(tier.last_trace_solve_id(), 0) << "no solve has run with the sink attached yet";

    const IpqpResult first =
        tier.solve(box_qp(-1.0, 1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    ASSERT_FALSE(sink.iters.empty()) << "a converged solve took at least one iteration";
    EXPECT_EQ(tier.last_trace_solve_id(), 0);
    for (const IpqpTraceIterEvent &ev : sink.iters) {
        EXPECT_EQ(ev.solve, 0);
        EXPECT_EQ(ev.major, 0) << "no driver set it -- the engine's own default";
        EXPECT_GT(ev.alpha_p, 0.0);
        EXPECT_LE(ev.alpha_p, 1.0);
        EXPECT_GT(ev.alpha_d, 0.0);
        EXPECT_LE(ev.alpha_d, 1.0);
    }
    // `it` is a dense 1..N sequence, one entry per completed iteration.
    for (std::size_t i = 0; i < sink.iters.size(); ++i) {
        EXPECT_EQ(sink.iters[i].it, static_cast<Index>(i) + 1);
    }
    EXPECT_EQ(sink.iters.back().it, first.counters.ipqp_iters);

    tier.set_trace_major(7);
    const IpqpResult second =
        tier.solve(box_qp(-1.0, 1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(second.status, QpStatus::kOptimal);
    EXPECT_EQ(tier.last_trace_solve_id(), 1);
    Index second_events = 0;
    for (const IpqpTraceIterEvent &ev : sink.iters) {
        if (ev.solve == 1) {
            ++second_events;
            EXPECT_EQ(ev.major, 7);
        }
    }
    EXPECT_GT(second_events, 0);
}

// H = diag(2, -1000) on [-10, 10]^2 (test_ipqp_engine.cpp's own indefinite
// fixture): arms the ladder heavily and ends in a genuine kNumerical escape,
// which is what makes it double as the reg-event AND the escape-event
// fixture below.
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

    Index up_count = 0;
    Index down_count = 0;
    for (const IpqpTraceRegEvent &ev : sink.regs) {
        if (ev.dir == IpqpTraceRegDir::kUp) {
            ++up_count;
            // A real MKL run never reports evidence failure on this legal
            // fixture (docs/testing.md: that path is a seam-only injection).
            EXPECT_EQ(ev.reason, IpqpTraceRegReason::kInertia);
        } else {
            ++down_count;
            EXPECT_EQ(ev.reason, IpqpTraceRegReason::kAccept);
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

    const IpqpSeed *carry = tier.warm_carry();
    ASSERT_NE(carry, nullptr) << "a finite converged solve arms the cross-major carry";
    const IpqpResult warm = tier.solve(qp, carry, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    ASSERT_EQ(sink.restarts.size(), 2u);
    const IpqpTraceRestartEvent &rev = sink.restarts[1];
    EXPECT_NE(rev.grade, IpqpTraceRestartGrade::kCold) << "a real seed was offered";
    EXPECT_EQ(rev.repaired, warm.counters.ipqp_restart_repairs != 0);
    // Both fields carry the ONE undifferentiated shift the engine measures
    // (this struct's own doc comment) -- pinned equal, not independently.
    EXPECT_DOUBLE_EQ(rev.shift_p, warm.counters.ipqp_restart_shift_max);
    EXPECT_DOUBLE_EQ(rev.shift_d, warm.counters.ipqp_restart_shift_max);
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

    // GUARD, NON-VACUOUS: with the read declined (`ipqp_require_final_inertia
    // = false`), `ipqp_final_inertia_read == 3` -- "not performed" -- and the
    // schema's three-value `final_inertia` enum has no slot for it, so the
    // event must not fire.
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
    EXPECT_FALSE(sink.escapes[0].stall.fired);
    EXPECT_FALSE(sink.escapes[0].infeasibility.fired);

    // NON-VACUITY: a clean convex solve escapes nothing, so nothing fires.
    IpqpEngine tier2(tight_opts());
    RecordingTraceSink sink2;
    tier2.attach_trace(&sink2);
    const IpqpResult ok = tier2.solve(box_qp(-1.0, 1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(ok.escape_reason, IpqpEscape::kNone);
    EXPECT_TRUE(sink2.escapes.empty());
}

// ipqp.route / qp.mode -- driver-emitted, so this pin runs the DRIVER, not
// the bare engine. Three HS members, measured (test_ipqp_dispatch.cpp), take
// three different rows of the section 2.3 table: HS6 refines and certifies
// on every major, HS3 reaches the SSN warm grade via a refusal, HS10 escapes
// on every major. Together they exercise all three `to` values and all three
// `qp.mode` outcomes on one fixture set.
TEST(IpqpTrace, DriverRouteAndQpModeEventsMatchTheRoutingCounters) {
    RecordingTraceSink sink;
    Index refine_accepted = 0;
    Index to_ssn = 0;
    Index to_walk_real = 0; // ipqp_to_walk minus the declines, which fire no event.

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
    }
    ASSERT_GT(refine_accepted, 0) << "HS6 must refine, or the refine row is vacuous";
    ASSERT_GT(to_ssn, 0) << "HS3 must reach the SSN warm grade, or the ssn row is vacuous";
    ASSERT_GT(to_walk_real, 0) << "HS10 must escape, or the walk row is vacuous";

    Index refine_events = 0, ssn_events = 0, walk_events = 0;
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
        EXPECT_GE(ev.uncertain, 0);
        EXPECT_GE(ev.face_rows, 0);
        EXPECT_GE(ev.face_bounds, 0);
    }
    EXPECT_EQ(refine_events, refine_accepted);
    EXPECT_EQ(ssn_events, to_ssn);
    EXPECT_EQ(walk_events, to_walk_real);
    EXPECT_EQ(sink.routes.size(), sink.modes.size())
        << "the two are emitted together, one pair per consulted subproblem";

    Index optimal_count = 0, routed_count = 0, escaped_count = 0;
    for (const QpModeTraceEvent &ev : sink.modes) {
        EXPECT_EQ(ev.mode, IpqpTraceQpMode::kIpqp);
        EXPECT_GE(ev.iters, 0);
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
}

} // namespace
} // namespace hven::solvers
