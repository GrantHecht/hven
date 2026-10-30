// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_disproved_entry.cpp -- M7 W0 T4.
//
// TWO THINGS ARE PINNED HERE, and the vehicle both rest on is
// support/disproved_entry_vehicle.h (which carries the whole argument for why it
// had to be constructed and why the tier's detector fires on it).
//
//  (b) THE DISPROVED-ENTRY CHARGE (M6 ledger Q-S3 (a), T8's rider). The ruling
//      is that the K = 3 retirement tally is charged on a DISPROVAL as well as
//      on a confirmed infeasibility, because "a disproved suspicion is evidence
//      that the tier's DETECTOR is unsuited to this problem". Until now that
//      decision had no pin, for want of a driver fixture that disproves at all.
//      It is pinned three ways below: one disproval at K = 1 retires the tier
//      (the charge, minimally); three consecutive disprovals at the shipped
//      K = 3 retire it at major 3 (the charge, at the shipped threshold); and
//      the same one disproval at K = 3 does NOT retire (the tally advanced, it
//      did not fire).
//
//  (a) THE RESEED (`SqpOptions::ipqp_reseed_after_disproval`). With the toggle
//      on, the disproved entry's certified solution seeds the NEXT tier entry
//      through the existing staged-seed slot, and the entry is visible as a
//      BASE-warm restart on the `ipqp.restart` trace line. With it off -- the
//      shipped default -- the next entry is cold, which is today's behaviour.
//
// THE COUNTERS ARE THE ASSERTION (CLAUDE.md section 7); the CSV the measurement
// test prints is the T4 artifact's raw input and asserts nothing by itself.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/core/solver_status.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/drivers/trace.h>

#include "support/disproved_entry_vehicle.h"

namespace hven::solvers {
namespace {

using test_support::disproved_entry_model;
using test_support::DisprovedEntryCell;

/// Counts the three trace lines this task reads: the tier's escape class and its
/// section 6.3 evidence, the restart GRADE each tier entry started at, and the
/// fallback's verdict. Everything else is a no-op -- a sink must implement the
/// pure virtuals, not care about them.
struct Reader : TraceSink {
    Index cold = 0;
    Index base = 0;
    Index full = 0;
    Index suspect_escapes = 0;
    Index other_escapes = 0;
    Index disproved = 0;
    Index relaxed = 0;
    Index exhausted = 0;
    Index rung_b = 0;
    double first_dual_growth = 0.0;
    double first_primal_improvement = 0.0;
    double first_primal_end = 0.0;

    void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
    void on_qp_mode(const QpModeTraceEvent &) override {}

    void on_ipqp_restart(const IpqpTraceRestartEvent &event) override {
        switch (event.grade) {
        case IpqpTraceRestartGrade::kCold:
            ++cold;
            break;
        case IpqpTraceRestartGrade::kBase:
            ++base;
            break;
        case IpqpTraceRestartGrade::kFull:
            ++full;
            break;
        }
    }

    void on_ipqp_escape(const IpqpTraceEscapeEvent &event) override {
        if (event.reason != IpqpTraceEscapeReason::kInfeasibleSuspect) {
            ++other_escapes;
            return;
        }
        if (suspect_escapes == 0) {
            first_dual_growth = event.evidence.infeasibility.dual_growth;
            first_primal_improvement = event.evidence.infeasibility.primal_improvement;
            first_primal_end = event.evidence.infeasibility.primal_end;
        }
        ++suspect_escapes;
    }

    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) override {
        switch (event.verdict) {
        case SqpFallbackVerdict::kDisproved:
            ++disproved;
            break;
        case SqpFallbackVerdict::kRelaxed:
            ++relaxed;
            break;
        case SqpFallbackVerdict::kExhausted:
            ++exhausted;
            break;
        case SqpFallbackVerdict::kRungB:
            ++rung_b;
            break;
        case SqpFallbackVerdict::kUnfired:
            break;
        }
    }
};

struct CellRun {
    SqpResult sol;
    Reader trace;
};

/// The SHIPPED retirement threshold, READ FROM THE OPTIONS STRUCT rather than
/// written out (fix round 1, Minor 1). Every pin below that means "the shipped
/// K" passes this rather than a literal, so a change to
/// `IpqpOptions::ipqp_retire_after` cannot leave
/// `ThreeConsecutiveDisprovalsRetireTheTierAtTheShippedThreshold` quietly
/// passing at a threshold that is no longer shipped. The premise test asserts
/// what the pins assume it to be.
const Index kShippedRetireAfter = IpqpOptions{}.ipqp_retire_after;

/// One solve of the shipped cell. `max_iter`, `retire_after` and `reseed` are the
/// only knobs any test below turns; everything else is the shipped default.
CellRun run_cell(Index max_iter, Index retire_after, bool reseed) {
    SqpOptions opts;
    opts.qp_mode = QpMode::kIpm;
    opts.max_iter = max_iter;
    opts.ipqp.ipqp_retire_after = retire_after;
    opts.ipqp_reseed_after_disproval = reseed;
    opts.common.threads = 1;

    const test_support::QpAsNlpModel model = disproved_entry_model(DisprovedEntryCell{});
    CellRun out;
    SqpSolver driver(opts);
    driver.attach_trace(&out.trace);
    out.sol = driver.solve(model);
    return out;
}

// ---------------------------------------------------------------------------
// (b) -- the fixture premise, stated once so every pin below is non-vacuous.
// ---------------------------------------------------------------------------

TEST(SqpDriverDisprovedEntry, TheVehicleEscapesInfeasibleSuspectOnAConsistentSubproblem) {
    // THE TWO PREMISES THE PINS BELOW ARE WRITTEN AGAINST, asserted where they are
    // stated rather than assumed (fix round 1, Minors 1 and 2).
    //
    // (i) The shipped K. Every "shipped threshold" pin in this file reads
    //     `kShippedRetireAfter`, but the NUMBERS those pins expect -- three majors,
    //     three escapes, retirement AT major 3 -- were derived at K = 3. If the
    //     shipped default moves, this line fails first and says so, instead of a
    //     counter pin failing with a number and no reason.
    ASSERT_EQ(kShippedRetireAfter, 3)
        << "IpqpOptions::ipqp_retire_after moved; the charge pins in this file were "
           "derived at K = 3 and must be re-derived at the new threshold";

    // (ii) The vehicle's own "inside every window" premise. Its header claims the
    //      consistent witness (x_k = target, s = 0) is inside EVERY window the
    //      driver can build, i.e. |target| < tr_init <= box. `disproved_entry_qp`
    //      validates only the second half -- it cannot see the DRIVER's radius --
    //      so the first half is pinned here, against the default `run_cell` leaves
    //      alone. Without it the tier's suspicion could become TRUE inside the
    //      window and the disproval below would be about nothing.
    const DisprovedEntryCell cell;
    EXPECT_LT(std::abs(cell.target), SqpOptions{}.tr_init)
        << "the witness must be reachable inside the FIRST subproblem's window";
    EXPECT_LE(SqpOptions{}.tr_init, cell.box)
        << "... and the window must not exceed the box the vehicle validates against";

    const CellRun r =
        run_cell(/*max_iter=*/1, /*retire_after=*/kShippedRetireAfter, /*reseed=*/false);

    // THE ESCAPE IS THE SUSPICION, not another class: the routing chain branches
    // on `escape_reason`, and only kInfeasibleSuspect reaches the certified
    // fallback whose rung A can disprove anything.
    EXPECT_EQ(r.sol.counters.ipqp.ipqp_escapes, 1);
    EXPECT_EQ(r.sol.counters.ipqp.ipqp_escape_infeasible_suspect, 1);
    EXPECT_EQ(r.trace.suspect_escapes, 1);
    EXPECT_EQ(r.trace.other_escapes, 0);

    // AND IT FIRED ON ITS OWN TWO CONJUNCTS, read off the escape's own evidence
    // block rather than asserted from the outside: the dual norm grew past
    // detail::kSsnDualGrowthFactor (1e4) across the stall window while the
    // primal residual stayed above the feasibility target and did NOT improve.
    // Measured on this cell: growth 1e6, improvement -0.094 (it got worse).
    EXPECT_GE(r.trace.first_dual_growth, 1.0e4)
        << "the growth conjunct -- if this drops below 1e4 the cell has stopped "
           "firing the detector and every pin in this file is about nothing";
    EXPECT_GT(r.trace.first_dual_growth, 1.0e5) << "and with two decades of margin, not one part "
                                                   "in fifty";
    EXPECT_LT(r.trace.first_primal_improvement, 0.01) << "the flat-on-the-floor conjunct";
    EXPECT_GT(r.trace.first_primal_end, 0.0) << "above the feasibility target";

    // THE SUBPROBLEM WAS CONSISTENT, and the fallback is what proves it: rung A's
    // elastic copy CLOSED its slacks, so its answer is the unrelaxed
    // subproblem's. That is what makes the escape a false positive rather than a
    // correct suspicion the ladder then relaxed.
    EXPECT_EQ(r.sol.counters.ipqp_suspicion_disproved, 1);
    EXPECT_EQ(r.trace.disproved, 1);
    EXPECT_EQ(r.trace.relaxed, 0);
    EXPECT_EQ(r.trace.exhausted, 0);
    EXPECT_EQ(r.trace.rung_b, 0);
    EXPECT_EQ(r.sol.counters.ipqp_fallback_rung_b, 0);

    // AND THE VERDICT IS NOT kInfeasible. A disproval is the one fallback arm
    // that may not synthesize one -- the relaxation shut, so there is nothing to
    // certify infeasible.
    EXPECT_NE(r.sol.status, SolveStatus::kInfeasible);
    EXPECT_FALSE(r.sol.infeasibility_certified);
    ASSERT_FALSE(r.sol.history.empty());
    EXPECT_EQ(r.sol.history.front().qp_status, QpStatus::kOptimal)
        << "the disproved major's step came back optimal, from rung A";
    EXPECT_TRUE(r.sol.history.front().elastic_applied);
}

// ---------------------------------------------------------------------------
// (b) -- THE CHARGE. Q-S3 (a) says a disproval advances the K tally like any
// other escape. Three readings of the same fact, on the same cell.
// ---------------------------------------------------------------------------

TEST(SqpDriverDisprovedEntry, OneDisprovalChargesTheTallyAndAtKEqualsOneRetiresTheTier) {
    // K = 1 makes the CHARGE itself observable at the driver: the only counter a
    // caller can read the tally through is `ipqp_tier_retired_after`, which moves
    // when the tally reaches K. At K = 1 one charge is the whole threshold, so a
    // non-zero value here says exactly "the disproved entry was charged".
    const CellRun one = run_cell(/*max_iter=*/1, /*retire_after=*/1, /*reseed=*/false);
    ASSERT_EQ(one.sol.counters.ipqp_suspicion_disproved, 1) << "fixture premise";
    EXPECT_EQ(one.sol.counters.ipqp.ipqp_tier_retired_after, 1)
        << "Q-S3 (a): THE CHARGE STANDS ON A DISPROVAL. The escape is recorded on "
           "the ladder ABOVE the routing branch (src/drivers/sqp_solver.cpp's "
           "`ipqp_ladder.record`), so the fallback's later success does not "
           "un-charge it -- a disproved suspicion is evidence that the DETECTOR "
           "is unsuited to this problem";

    // ... and at the shipped K = 3 the same single disproval charges without
    // retiring, which is the other half of the same statement.
    const CellRun three =
        run_cell(/*max_iter=*/1, /*retire_after=*/kShippedRetireAfter, /*reseed=*/false);
    ASSERT_EQ(three.sol.counters.ipqp_suspicion_disproved, 1) << "fixture premise";
    EXPECT_EQ(three.sol.counters.ipqp.ipqp_tier_retired_after, 0)
        << "one charge is not three: the tally advanced, retirement did not fire";
}

TEST(SqpDriverDisprovedEntry, ThreeConsecutiveDisprovalsRetireTheTierAtTheShippedThreshold) {
    // THE CHARGE AT THE SHIPPED THRESHOLD, with nothing but disprovals feeding
    // the tally: three majors, three tier entries, three escapes, three
    // disprovals, and retirement at major 3. If a disproval did NOT charge, the
    // tally would never reach 3 and this value would be 0.
    // ONE MAJOR PER CHARGE, so the budget IS the threshold: K majors, K escapes,
    // K disprovals, retirement at major K. Written against `kShippedRetireAfter`
    // throughout rather than against the literal 3 (fix round 1, Minor 1).
    const CellRun r = run_cell(/*max_iter=*/kShippedRetireAfter,
                               /*retire_after=*/kShippedRetireAfter, /*reseed=*/false);
    EXPECT_EQ(r.sol.counters.major_iters, kShippedRetireAfter);
    EXPECT_EQ(r.sol.counters.ipqp.ipqp_escapes, kShippedRetireAfter);
    EXPECT_EQ(r.sol.counters.ipqp.ipqp_escape_infeasible_suspect, kShippedRetireAfter);
    EXPECT_EQ(r.sol.counters.ipqp_suspicion_disproved, kShippedRetireAfter);
    EXPECT_EQ(r.trace.disproved, kShippedRetireAfter);
    EXPECT_EQ(r.trace.relaxed + r.trace.exhausted + r.trace.rung_b, 0)
        << "every entry disproved -- no other verdict feeds this tally";
    EXPECT_EQ(r.sol.counters.ipqp.ipqp_tier_retired_after, kShippedRetireAfter);
    EXPECT_NE(r.sol.status, SolveStatus::kInfeasible);

    // AND THE CARRY WAS DROPPED AT EVERY ONE OF THEM: every entry started cold,
    // which is the state (a) exists to recover.
    EXPECT_EQ(r.trace.cold, kShippedRetireAfter);
    EXPECT_EQ(r.trace.base, 0);
    EXPECT_EQ(r.trace.full, 0);
    EXPECT_EQ(r.sol.counters.ipqp.ipqp_restart_repairs, 0);
}

// ---------------------------------------------------------------------------
// (a) -- THE RESEED, on the same cell, as a paired A/B.
// ---------------------------------------------------------------------------

TEST(SqpDriverDisprovedEntry, TheDefaultIsOffAndOffIsTodaysColdRestart) {
    // THE SHIPPED DEFAULT, stated where a change to it would be caught.
    EXPECT_FALSE(SqpOptions{}.ipqp_reseed_after_disproval);

    // ... and an explicit `false` is the same run as the default: this is the
    // OFF arm of the pair below, written out so the ON arm's numbers have a
    // stated baseline rather than an implied one.
    const CellRun off =
        run_cell(/*max_iter=*/3, /*retire_after=*/kShippedRetireAfter, /*reseed=*/false);
    EXPECT_EQ(off.trace.cold, 3);
    EXPECT_EQ(off.trace.base + off.trace.full, 0);
    EXPECT_EQ(off.sol.counters.ipqp_suspicion_disproved, 3);
    EXPECT_EQ(off.sol.counters.ipqp.ipqp_tier_retired_after, 3);
    // RE-DERIVED AT M7 W2 E4 FIX ROUND 2 (a declared mover): kMaxIter -> kOptimal on
    // the OFF arm. `max_iter = 3` is the fixture's own budget and the point of the cell
    // is the three cold entries and the three disproved suspicions above -- all four of
    // which are unchanged -- while the solve's terminal status at that budget is not
    // what the toggle is about. E4's answers reach the model's optimum inside the three
    // majors; the ON arm's own status pin (the pair's other half) is measured there.
    EXPECT_EQ(off.sol.status, SolveStatus::kOptimal);
}

TEST(SqpDriverDisprovedEntry, TheReseedMakesTheNextEntryBaseWarmAndTheTierStopsEscaping) {
    const CellRun on =
        run_cell(/*max_iter=*/3, /*retire_after=*/kShippedRetireAfter, /*reseed=*/true);

    // THE OBSERVATION THE TOGGLE OWNS: entry 1 is cold (nothing staged yet),
    // entry 2 is BASE-warm -- the disproved entry's certified solution, whose
    // signed bound price alone is what a base-warm grade is named for -- and
    // entry 3 is FULL-warm, because entry 2 no longer escaped and so left the
    // engine's own cross-major carry alive.
    EXPECT_EQ(on.trace.cold, 1);
    EXPECT_EQ(on.trace.base, 1) << "the reseeded entry, through the existing staged-seed slot";
    EXPECT_EQ(on.trace.full, 1) << "and the carry the un-escaped entry 2 left behind";
    EXPECT_EQ(on.sol.counters.ipqp.ipqp_restart_repairs, 2)
        << "both warm entries needed the section 5.2 repair -- the seed carries duals "
           "and a zero primal block by construction";

    // AND WHAT IT BUYS ON THIS CELL: the second entry no longer escapes, so the
    // tally never reaches K, the tier is not retired, and the solve converges
    // inside the same three majors the OFF arm spends without converging.
    EXPECT_EQ(on.sol.counters.ipqp.ipqp_escapes, 1);
    EXPECT_EQ(on.sol.counters.ipqp_suspicion_disproved, 1);
    EXPECT_EQ(on.sol.counters.ipqp.ipqp_tier_retired_after, 0);
    EXPECT_EQ(on.sol.status, SolveStatus::kOptimal);
    EXPECT_EQ(on.sol.counters.major_iters, 3);

    // THE SAME ANSWER, NOT A DIFFERENT ONE: the OFF arm reaches the same
    // objective with more work and no certificate, so the win is the path, not
    // the point.
    const CellRun off =
        run_cell(/*max_iter=*/3, /*retire_after=*/kShippedRetireAfter, /*reseed=*/false);
    EXPECT_LT(on.sol.counters.qp_minor_iters, off.sol.counters.qp_minor_iters);
    EXPECT_NEAR(on.sol.f, off.sol.f, 1.0e-6);
}

TEST(SqpDriverDisprovedEntry, TheReseedIsInertWhereNothingIsDisproved) {
    // THE TOGGLE TOUCHES ONE ARM. A cell whose first major never escapes never
    // reaches route 5 at all, so both states must be counter-identical -- this is
    // what makes "default OFF" and "ON" the same library everywhere else.
    DisprovedEntryCell quiet;
    quiet.rho = 1.0; // the price is what drives the dual growth the detector reads
    quiet.pull = 0.0;
    for (const Index max_iter : {1, 4}) {
        SqpCounters counters[2];
        for (int arm = 0; arm < 2; ++arm) {
            SqpOptions opts;
            opts.qp_mode = QpMode::kIpm;
            opts.max_iter = max_iter;
            opts.ipqp_reseed_after_disproval = arm == 1;
            opts.common.threads = 1;
            SqpSolver driver(opts);
            const test_support::QpAsNlpModel model = disproved_entry_model(quiet);
            counters[arm] = driver.solve(model).counters;
        }
        EXPECT_EQ(counters[0].ipqp_suspicion_disproved, 0)
            << "premise: this cell disproves nothing";
        EXPECT_EQ(counters[0].major_iters, counters[1].major_iters) << "max_iter " << max_iter;
        EXPECT_EQ(counters[0].qp_minor_iters, counters[1].qp_minor_iters)
            << "max_iter " << max_iter;
        EXPECT_EQ(counters[0].factorizations, counters[1].factorizations)
            << "max_iter " << max_iter;
        EXPECT_EQ(counters[0].ipqp.ipqp_iters, counters[1].ipqp.ipqp_iters)
            << "max_iter " << max_iter;
        EXPECT_EQ(counters[0].ipqp.ipqp_escapes, counters[1].ipqp.ipqp_escapes)
            << "max_iter " << max_iter;
    }
}

// ---------------------------------------------------------------------------
// The measurement. Prints one CSV row per (state, max_iter) cell; asserts only
// the fixture premise, because the numbers themselves are the T4 artifact's.
// ---------------------------------------------------------------------------

/// One CSV block: the header plus one row per `max_iter` budget, for ONE toggle
/// state. SPLIT BY STATE deliberately -- each is its own gtest name, so a
/// `perf stat` pass can price the two arms in separate processes rather than
/// one process running both (CLAUDE.md section 7's instruction line).
void measure_state(bool reseed) {
    std::fputs("state,max_iter,retire_after,status,majors,escapes,suspect,disproved,relaxed,"
               "rung_b,retired_after,entries_cold,entries_base,entries_full,restart_repairs,"
               "qp_minor_iters,factorizations,ipqp_iters,ipqp_facts,elastic_activations,"
               "elastic_escalations,f\n",
               stdout);
    for (const Index max_iter : {1, 2, 3, 6}) {
        const CellRun r = run_cell(max_iter, /*retire_after=*/kShippedRetireAfter, reseed);
        ASSERT_GE(r.sol.counters.ipqp_suspicion_disproved, 1)
            << "the cell must disprove at all, or the row below is about nothing";
        std::fputs(
            fmt::format(
                "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{:.12g}\n",
                reseed ? "on" : "off", max_iter, 3, to_string(r.sol.status),
                r.sol.counters.major_iters, r.sol.counters.ipqp.ipqp_escapes,
                r.sol.counters.ipqp.ipqp_escape_infeasible_suspect,
                r.sol.counters.ipqp_suspicion_disproved, r.trace.relaxed,
                r.sol.counters.ipqp_fallback_rung_b, r.sol.counters.ipqp.ipqp_tier_retired_after,
                r.trace.cold, r.trace.base, r.trace.full, r.sol.counters.ipqp.ipqp_restart_repairs,
                r.sol.counters.qp_minor_iters, r.sol.counters.factorizations,
                r.sol.counters.ipqp.ipqp_iters, r.sol.counters.ipqp.ipqp_factorizations,
                r.sol.counters.elastic_activations, r.sol.counters.elastic_escalations, r.sol.f)
                .c_str(),
            stdout);
        std::fflush(stdout);
    }
}

TEST(M7T4DisprovedEntry, TheReseedMeasuredOff) { measure_state(/*reseed=*/false); }

TEST(M7T4DisprovedEntry, TheReseedMeasuredOn) { measure_state(/*reseed=*/true); }

} // namespace
} // namespace hven::solvers
