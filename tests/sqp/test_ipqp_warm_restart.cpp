// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_ipqp_warm_restart.cpp -- M6 W1 task 7 pins for the interior-point tier's
// SUBPROBLEM-LEVEL warm restart (spec section 5). The DRIVER half is in
// test_sqp_warm_currency.cpp. Every value pin is shown fallible by a neighbouring fixture.

#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/ipqp_engine.h>
#include <hven/drivers/sqp_driver.h>

#include "support/parametric_families.h"

namespace hven::solvers {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

SpMatRM diag_upper(const std::vector<double> &d) {
    const Index n = static_cast<Index>(d.size());
    SpMatRM m(n, n);
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        t.emplace_back(static_cast<int>(i), static_cast<int>(i), d[static_cast<std::size_t>(i)]);
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

QpOptions tight_opts() {
    QpOptions o;
    o.tr_radius = kInf;
    return o;
}

/// min 0.5 x^2 over x in [-1, 1]. The minimiser is the INTERIOR point x = 0, so a seed can carry
/// a positive price on BOTH sides of the same bound and still be converged -- exactly the
/// configuration the signed-z flattening cannot represent.
QpProblem interior_qp() {
    QpProblem qp;
    qp.H = diag_upper({1.0});
    qp.g = vec({0.0});
    qp.Ae = SpMatRM(0, 1);
    qp.Ai = SpMatRM(0, 1);
    qp.be = Vec(0);
    qp.bi = Vec(0);
    qp.lower = vec({-1.0});
    qp.upper = vec({1.0});
    return qp;
}

/// min |x - (1, 2)|^2 over [-5, 5]^2 -- a strictly convex box QP whose
/// minimiser is interior, used where a warm start has to be visibly STALE.
QpProblem stale_qp() {
    QpProblem qp;
    qp.H = diag_upper({2.0, 2.0});
    qp.g = vec({-2.0, -4.0});
    qp.Ae = SpMatRM(0, 2);
    qp.Ai = SpMatRM(0, 2);
    qp.be = Vec(0);
    qp.bi = Vec(0);
    qp.lower = vec({-5.0, -5.0});
    qp.upper = vec({5.0, 5.0});
    return qp;
}

/// min |x - (1, 2)|^2 over [-5, 5]^2 with one inequality row -- the shape the
/// repair-off fixtures need, because `s` and `lambda_i` are what the lever's
/// domain gate is about.
QpProblem row_qp() {
    QpProblem qp = stale_qp();
    SpMatRM ai(1, 2);
    std::vector<Eigen::Triplet<double>> t;
    t.emplace_back(0, 0, 1.0);
    ai.setFromTriplets(t.begin(), t.end());
    ai.makeCompressed();
    qp.Ai = ai;
    qp.bi = vec({3.0});
    return qp;
}

/// A seed shaped for `qp`, with every block at its declared width.
IpqpSeed seed_for(const QpProblem &qp) {
    IpqpSeed seed;
    seed.x = Vec::Zero(qp.n());
    seed.s = Vec::Zero(qp.mi());
    seed.lambda_e = Vec::Zero(qp.me());
    seed.lambda_i = Vec::Zero(qp.mi());
    seed.zl = Vec::Zero(qp.n());
    seed.zu = Vec::Zero(qp.n());
    seed.zeta = Vec::Zero(qp.n());
    seed.lambda_est_e = Vec::Zero(qp.me());
    seed.lambda_est_i = Vec::Zero(qp.mi());
    return seed;
}

/// The converged, two-sided-price seed the split pins are built on: at x = 0 both bound
/// distances are 1, so the pair products ARE the two prices, the point is centred within the
/// section 5.2 test, and the tier converges at iteration 0 without repairing anything.
IpqpSeed centred_full_seed() {
    IpqpSeed seed = seed_for(interior_qp());
    seed.zl = vec({3.0e-9});
    seed.zu = vec({1.0e-9});
    seed.mu = 2.0e-9;
    seed.grade = IpqpRestartGrade::kFullWarm;
    return seed;
}

/// The SAME information after the crossover's signed-z flattening: z = zL - zU
/// = 2e-9, re-split as (max(z, 0), max(-z, 0)). This is what the base-warm
/// grade can see, and the loss is the whole reason the polish extension exists.
IpqpSeed centred_base_seed() {
    IpqpSeed seed = seed_for(interior_qp());
    seed.zl = vec({2.0e-9});
    seed.zu = vec({0.0});
    seed.mu = 0.0; // absent: no payload mu on the core-only grade.
    seed.grade = IpqpRestartGrade::kBaseWarm;
    return seed;
}

} // namespace

// ---------------------------------------------------------------------------
// The boundary: a seed is a caller assertion about THIS subproblem
// ---------------------------------------------------------------------------

TEST(IpqpWarmRestart, AMalformedSeedIsRefusedAtTheBoundary) {
    const QpProblem qp = interior_qp();
    IpqpEngine tier(tight_opts());

    // A block at the wrong width.
    IpqpSeed wrong_width = seed_for(qp);
    wrong_width.zl = vec({1.0, 2.0});
    EXPECT_THROW(tier.solve(qp, &wrong_width, IpqpOptions{}, SolveOverrides{}),
                 std::invalid_argument);

    // A non-finite component.
    IpqpSeed not_finite = seed_for(qp);
    not_finite.x = vec({std::numeric_limits<double>::quiet_NaN()});
    EXPECT_THROW(tier.solve(qp, &not_finite, IpqpOptions{}, SolveOverrides{}),
                 std::invalid_argument);

    // A non-finite payload mu. The DRIVER degrades this cold (plan section 7
    // note (f)); a seed that reached the engine carrying one is a caller error.
    IpqpSeed bad_mu = seed_for(qp);
    bad_mu.mu = std::numeric_limits<double>::infinity();
    EXPECT_THROW(tier.solve(qp, &bad_mu, IpqpOptions{}, SolveOverrides{}), std::invalid_argument);

    // A seed labelled COLD. A cold solve is a null seed, not a labelled one.
    IpqpSeed cold_label = seed_for(qp);
    cold_label.grade = IpqpRestartGrade::kCold;
    EXPECT_THROW(tier.solve(qp, &cold_label, IpqpOptions{}, SolveOverrides{}),
                 std::invalid_argument);

    // AND THE SAME SEED, WELL FORMED, IS ACCEPTED -- so the four refusals above
    // are about the defect and not about the path.
    IpqpSeed good = seed_for(qp);
    good.zl = vec({1.0e-3});
    good.zu = vec({1.0e-3});
    good.mu = 1.0e-3;
    const IpqpResult r = tier.solve(qp, &good, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.restart_grade, IpqpRestartGrade::kFullWarm);
}

TEST(IpqpWarmRestart, AColdSolveReportsTheColdGradeAndTouchesNoWarmCounter) {
    const QpProblem qp = interior_qp();
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(r.restart_grade, IpqpRestartGrade::kCold);
    EXPECT_EQ(r.counters.ipqp_restart_repairs, 0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_restart_shift_max, 0.0);
    EXPECT_EQ(r.counters.ipqp_mu_adopted, 0);
    EXPECT_EQ(r.counters.ipqp_warm_restart_abandoned, 0);
}

// ---------------------------------------------------------------------------
// Section 5.4 -- the grades, and what the flattening costs
// ---------------------------------------------------------------------------

// THE FULL GRADE DELIVERS THE PAYLOAD'S SPLIT UNCHANGED. The seed is centred and already meets
// this problem's relative target, so the tier converges before it steps and the exported prices
// ARE the ingested ones, bitwise -- a statement about the ingest rather than about the solve.
TEST(IpqpWarmRestart, TheFullGradeCarriesTheTwoSidedSplitUnflattened) {
    const QpProblem qp = interior_qp();
    const IpqpSeed seed = centred_full_seed();
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &seed, IpqpOptions{}, SolveOverrides{});

    ASSERT_EQ(r.status, QpStatus::kOptimal);
    ASSERT_EQ(r.counters.ipqp_iters, 0) << "the seed already meets the target, so nothing moved";
    EXPECT_EQ(r.restart_grade, IpqpRestartGrade::kFullWarm);
    EXPECT_EQ(r.zl(0), seed.zl(0));
    EXPECT_EQ(r.zu(0), seed.zu(0));
    // A CENTRED SEED PAYS NO REPAIR, which is what keeps the counter a signal.
    EXPECT_EQ(r.counters.ipqp_restart_repairs, 0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_restart_shift_max, 0.0);
}

// THE BASE GRADE IS A DOCUMENTED DEGRADATION, NOT AN EQUIVALENT. Same point,
// same solve, same convergence -- and a DIFFERENT split, because the signed z
// the crossover flattens to cannot say that both sides were priced.
TEST(IpqpWarmRestart, TheBaseGradeConvergesButLosesTheSplit) {
    const QpProblem qp = interior_qp();
    const IpqpSeed seed = centred_base_seed();
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &seed, IpqpOptions{}, SolveOverrides{});

    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.restart_grade, IpqpRestartGrade::kBaseWarm);

    const IpqpSeed full = centred_full_seed();
    EXPECT_NE(r.zl(0), full.zl(0))
        << "the flattened value cannot reproduce the payload's lower price";
    EXPECT_NE(r.zu(0), full.zu(0))
        << "nor its upper one -- this is the loss the polish extension exists to avoid";
    // The exact zero the re-split produced on the upper side is repaired: a
    // price at 0 is legitimate, and the tier needs a strict interior.
    EXPECT_GT(r.zu(0), 0.0);
    EXPECT_EQ(r.counters.ipqp_restart_repairs, 1);
    EXPECT_GT(r.counters.ipqp_restart_shift_max, 0.0);
}

// ---------------------------------------------------------------------------
// Section 5.3 -- the mu_0 clamp
// ---------------------------------------------------------------------------

TEST(IpqpWarmRestart, ThePayloadMuIsAdoptedOnlyWhenItRaisesTheMeasuredFloor) {
    const QpProblem qp = interior_qp();
    IpqpEngine tier(tight_opts());

    // The payload names a HIGHER barrier level than the point supports, so it
    // binds: `max(mu_meas, kappa * mu_payload)` takes the payload term.
    IpqpSeed raises = centred_full_seed();
    raises.mu = 1.0e-4;
    const IpqpResult r = tier.solve(qp, &raises, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(r.counters.ipqp_mu_adopted, 1);

    // The same payload with adoption DISABLED (`kappa = 0`, a documented
    // reading of the field): the clamp reads the measured floor only.
    IpqpOptions off;
    off.ipqp_mu_adopt_factor = 0.0;
    const IpqpResult r_off = tier.solve(qp, &raises, off, SolveOverrides{});
    EXPECT_EQ(r_off.counters.ipqp_mu_adopted, 0);

    // And a payload BELOW the measured floor never lowers it, so nothing is
    // adopted even at the default factor.
    IpqpSeed lower = centred_full_seed();
    lower.mu = 1.0e-15;
    const IpqpResult r_low = tier.solve(qp, &lower, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(r_low.counters.ipqp_mu_adopted, 0);
}

TEST(IpqpWarmRestart, ThePayloadMuCanNeverRaiseMuAboveTheColdDefault) {
    const QpProblem qp = interior_qp();
    IpqpEngine tier(tight_opts());
    IpqpSeed huge = centred_full_seed();
    huge.mu = 1.0e6;

    IpqpOptions o;
    o.ipqp_init_mu = 1.0e-3;
    const IpqpResult r = tier.solve(qp, &huge, o, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    // The clamp's ceiling is the cold default, so an absurd payload cannot
    // make this solve start looser than a cold one would have.
    EXPECT_LE(r.mu, o.ipqp_init_mu);
}

// ---------------------------------------------------------------------------
// Section 5.5 -- the warm-kill (fixture A12's mechanism)
// ---------------------------------------------------------------------------

// A12, AS THE MECHANISM RATHER THAN AS TYCHO'S NUMBERS (plan risk R7). A stale primal seed on a
// perturbed problem: the warm attempt is abandoned at the clamped budget, the counter fires
// exactly once, and the COLD path recovers the answer the tier would have reached from cold.
TEST(IpqpWarmRestart, AStaleSeedIsAbandonedAtTheClampedBudgetAndColdRecovers) {
    const QpProblem qp = stale_qp();
    IpqpEngine tier(tight_opts());

    const IpqpResult cold = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(cold.status, QpStatus::kOptimal);

    // The stale seed: a point from the far corner of the box, priced as if the
    // solution were there. This is the "stale primal" shape, not a random one.
    IpqpSeed stale = seed_for(qp);
    stale.x = vec({-4.9, -4.9});
    stale.zl = vec({1.0e-9, 1.0e-9});
    stale.zu = vec({1.0e-9, 1.0e-9});
    stale.mu = 1.0e-9;

    IpqpOptions killed;
    killed.ipqp_warm_iter_budget = 1;
    IpqpEngine killed_tier(tight_opts());
    const IpqpResult r = killed_tier.solve(qp, &stale, killed, SolveOverrides{});

    EXPECT_EQ(r.counters.ipqp_warm_restart_abandoned, 1) << "the kill fired, exactly once";
    EXPECT_EQ(r.status, QpStatus::kOptimal) << "and the cold restart recovered the answer";
    EXPECT_NEAR(r.x(0), 1.0, 1e-6);
    EXPECT_NEAR(r.x(1), 2.0, 1e-6);
    // THE KILL IS NOT AN ESCAPE: it has no census bucket and never reaches the
    // section 6.1 ladder.
    EXPECT_EQ(r.counters.ipqp_escapes, 0);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);

    // NON-VACUITY: the SAME seed under the shipped budget is not abandoned, so
    // the kill above is about the budget and not about the seed.
    IpqpEngine patient(tight_opts());
    const IpqpResult ok = patient.solve(qp, &stale, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(ok.counters.ipqp_warm_restart_abandoned, 0);
    EXPECT_EQ(ok.status, QpStatus::kOptimal);
}

// PLAN SECTION 7 NOTE (c) + FIX ROUND 1 R2: the effective warm budget is
// `min(ipqp_warm_iter_budget, effective ipqp_max_iter)`; even where the clamp makes the two
// EQUAL, the warm-kill still preempts the ordinary escape. `.superpowers/w1-t7-report.md`.
TEST(IpqpWarmRestart, TheWarmBudgetIsClampedButStillPreemptsTheOrdinaryEscape) {
    const QpProblem qp = stale_qp();
    IpqpSeed stale = seed_for(qp);
    stale.x = vec({-4.9, -4.9});
    stale.zl = vec({1.0e-9, 1.0e-9});
    stale.zu = vec({1.0e-9, 1.0e-9});
    stale.mu = 1.0e-9;

    IpqpOptions clamped;
    clamped.ipqp_warm_iter_budget = 1000; // far above the cap below
    clamped.ipqp_hard_iter_cap = 2;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &stale, clamped, SolveOverrides{});
    EXPECT_EQ(r.counters.ipqp_warm_restart_abandoned, 1)
        << "the clamped warm budget equals the iteration budget, and the kill still fires";
    // AND THE COLD ATTEMPT RAN: its own budget re-based at the kill, so the
    // solve took more iterations than the single clamped budget allows.
    EXPECT_GT(r.counters.ipqp_iters, clamped.ipqp_hard_iter_cap);

    // NON-VACUITY: a COLD solve under the same cap escapes on budget, so the
    // extra iterations above are the restart's and not the cap's slack.
    IpqpEngine cold_tier(tight_opts());
    const IpqpResult c = cold_tier.solve(qp, nullptr, clamped, SolveOverrides{});
    EXPECT_EQ(c.escape_reason, IpqpEscape::kBudget);
    EXPECT_EQ(c.counters.ipqp_warm_restart_abandoned, 0);
    EXPECT_LE(c.counters.ipqp_iters, clamped.ipqp_hard_iter_cap);
}

// THE BUDGETS ARE PER ATTEMPT, not a shared pool: section 5.5 makes a SECOND overrun an ordinary
// budget escape -- the ordinary budget, not its remainder. The fixture calibrates itself: the
// cold solve's own iteration count IS the cap, so a shared pool would escape and this converges.
TEST(IpqpWarmRestart, TheColdRestartGetsTheOrdinaryBudgetAndNotItsRemainder) {
    const QpProblem qp = stale_qp();
    IpqpEngine measure(tight_opts());
    const IpqpResult cold = measure.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(cold.status, QpStatus::kOptimal);
    ASSERT_GT(cold.counters.ipqp_iters, 0);

    IpqpSeed stale = seed_for(qp);
    stale.x = vec({-4.9, -4.9});
    stale.zl = vec({1.0e-9, 1.0e-9});
    stale.zu = vec({1.0e-9, 1.0e-9});
    stale.mu = 1.0e-9;

    IpqpOptions o;
    o.ipqp_warm_iter_budget = 1;
    o.ipqp_hard_iter_cap = cold.counters.ipqp_iters; // exactly what cold needs
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &stale, o, SolveOverrides{});

    EXPECT_EQ(r.counters.ipqp_warm_restart_abandoned, 1);
    EXPECT_EQ(r.status, QpStatus::kOptimal)
        << "the cold restart had the whole ordinary budget, so it converged";
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
    // AND THE ABANDONED COST IS STILL VISIBLE: the counters are solve totals,
    // so the warm attempt's one iteration is reported, not hidden.
    EXPECT_EQ(r.counters.ipqp_iters, cold.counters.ipqp_iters + 1);
}

// THE EXTREME OF THE FIELD'S OWN BAND, which the section 6.1 ladder fixtures
// rely on to hold themselves cold: budget 0 kills before the warm attempt can
// take a step, so the trajectory is the cold one.
TEST(IpqpWarmRestart, AZeroWarmBudgetKillsBeforeTheFirstStepAndRunsTheColdTrajectory) {
    const QpProblem qp = stale_qp();
    IpqpSeed stale = seed_for(qp);
    stale.x = vec({-4.9, -4.9});
    stale.zl = vec({1.0e-9, 1.0e-9});
    stale.zu = vec({1.0e-9, 1.0e-9});
    stale.mu = 1.0e-9;

    IpqpOptions zero;
    zero.ipqp_warm_iter_budget = 0;
    IpqpEngine warm_tier(tight_opts());
    const IpqpResult w = warm_tier.solve(qp, &stale, zero, SolveOverrides{});

    IpqpEngine cold_tier(tight_opts());
    const IpqpResult c = cold_tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});

    EXPECT_EQ(w.counters.ipqp_warm_restart_abandoned, 1);
    EXPECT_EQ(w.counters.ipqp_iters, c.counters.ipqp_iters);
    EXPECT_EQ(w.x(0), c.x(0));
    EXPECT_EQ(w.x(1), c.x(1));
    EXPECT_EQ(w.zl(0), c.zl(0));
    EXPECT_EQ(w.zu(0), c.zu(0));
}

// ---------------------------------------------------------------------------
// Section 5.1 flow (b) -- the cross-major carry
// ---------------------------------------------------------------------------

TEST(IpqpWarmRestart, TheCarryIsArmedByASolveAndSpentByTheNextOne) {
    const QpProblem qp = stale_qp();
    IpqpEngine tier(tight_opts());

    EXPECT_EQ(tier.warm_carry(), nullptr) << "a fresh engine carries nothing";
    const IpqpResult first = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    ASSERT_NE(tier.warm_carry(), nullptr);
    EXPECT_EQ(tier.warm_carry()->x, first.x) << "the carry IS the state the solve finished at";
    EXPECT_EQ(tier.warm_carry()->zl, first.zl);
    EXPECT_EQ(tier.warm_carry()->grade, IpqpRestartGrade::kFullWarm)
        << "the carry keeps zL/zU/mu unflattened, which is what the full grade names";

    // Re-solving the SAME subproblem from the carry costs strictly less: the
    // carry is a converged point of exactly this problem.
    const IpqpResult second = tier.solve(qp, tier.warm_carry(), IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(second.status, QpStatus::kOptimal);
    EXPECT_LT(second.counters.ipqp_iters, first.counters.ipqp_iters);

    tier.reset_warm_carry();
    EXPECT_EQ(tier.warm_carry(), nullptr);
}

// A trust-region shrink-retry does not reset the seed (spec 5.1 amendment D), and that rests on
// this: a solve that produced nothing usable leaves the previous carry standing. A DECLINED
// subproblem is the reachable case -- the tier returns before it has a workspace at all.
TEST(IpqpWarmRestart, ASolveThatProducedNothingLeavesThePreviousCarryStanding) {
    IpqpEngine tier(tight_opts());
    const IpqpResult first = tier.solve(stale_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    ASSERT_NE(tier.warm_carry(), nullptr);
    const Vec carried = tier.warm_carry()->x;

    // A zero-width box: the domain gate declines pre-solve.
    QpProblem pinned = stale_qp();
    pinned.lower = vec({1.0, 1.0});
    pinned.upper = vec({1.0, 5.0});
    const IpqpResult declined = tier.solve(pinned, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_TRUE(declined.declined_pinned);

    ASSERT_NE(tier.warm_carry(), nullptr);
    EXPECT_EQ(tier.warm_carry()->x, carried);
}

// ---------------------------------------------------------------------------
// The repair lever (fix round 1, ruling R1)
// ---------------------------------------------------------------------------

// `ipqp_warm_repair = false` DISABLES THE REPAIR, NEVER THE VALIDATION: a
// seed the repair would have had to fix degrades COLD instead of being
// consumed raw. `.superpowers/w1-t7-report.md` FIX ROUND 2.
TEST(IpqpWarmRestart, RepairOffDegradesADefectiveSeedColdInsteadOfConsumingIt) {
    const QpProblem qp = row_qp();
    IpqpSeed absent_slack = seed_for(qp);
    absent_slack.zl = vec({1.0e-3, 1.0e-3});
    absent_slack.zu = vec({1.0e-3, 1.0e-3});
    absent_slack.lambda_i = vec({1.0e-3});
    absent_slack.s = vec({0.0}); // the ABSENT convention both producers use
    absent_slack.mu = 1.0e-3;

    IpqpOptions off;
    off.ipqp_warm_repair = false;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &absent_slack, off, SolveOverrides{});
    EXPECT_EQ(r.restart_grade, IpqpRestartGrade::kCold) << "the seed was degraded, not consumed";
    EXPECT_EQ(r.status, QpStatus::kOptimal) << "and the cold solve is a real solve";
    EXPECT_TRUE(r.x.allFinite());
    EXPECT_EQ(r.counters.ipqp_restart_repairs, 0);
    EXPECT_EQ(r.counters.ipqp_mu_adopted, 0);

    // NON-VACUITY, the same payload with the repair ON: the absent slack is
    // recomputed from `bi - Ai x` and the seed IS consumed, at its own grade.
    IpqpEngine repaired(tight_opts());
    const IpqpResult ok = repaired.solve(qp, &absent_slack, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(ok.restart_grade, IpqpRestartGrade::kFullWarm);
    EXPECT_EQ(ok.status, QpStatus::kOptimal);
    // ... and the DEGRADED run really ran the cold path: same iterate, bitwise,
    // as a solve that was never handed a seed at all.
    IpqpEngine plain(tight_opts());
    const IpqpResult c = plain.solve(qp, nullptr, off, SolveOverrides{});
    EXPECT_EQ(r.counters.ipqp_iters, c.counters.ipqp_iters);
    EXPECT_EQ(r.x, c.x);
    EXPECT_EQ(r.zl, c.zl);

    // AND A SEED THAT NEEDS NOTHING IS STILL CONSUMED with the repair off --
    // so the degrade above is about the defect, not about the lever.
    IpqpSeed clean = seed_for(qp);
    clean.zl = vec({1.0e-3, 1.0e-3});
    clean.zu = vec({1.0e-3, 1.0e-3});
    clean.lambda_i = vec({1.0e-3});
    clean.s = vec({1.0});
    clean.mu = 1.0e-3;
    IpqpEngine unrepaired(tight_opts());
    const IpqpResult u = unrepaired.solve(qp, &clean, off, SolveOverrides{});
    EXPECT_EQ(u.restart_grade, IpqpRestartGrade::kFullWarm);
    EXPECT_EQ(u.counters.ipqp_restart_repairs, 0) << "the repair did not run";
}

// F2: the section 5.3 clamp counts on EVERY branch, the repair-off one
// included -- a path that clamps without counting is a hole in the currency
// section 7 makes the asserted evidence for T7.4.
TEST(IpqpWarmRestart, AdoptionIsCountedOnTheRepairOffBranchToo) {
    const QpProblem qp = row_qp();
    IpqpSeed clean = seed_for(qp);
    clean.zl = vec({1.0e-9, 1.0e-9});
    clean.zu = vec({1.0e-9, 1.0e-9});
    clean.lambda_i = vec({1.0e-9});
    clean.s = vec({1.0});
    clean.mu = 1.0e-4; // binds the clamp from above

    IpqpOptions off;
    off.ipqp_warm_repair = false;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &clean, off, SolveOverrides{});
    ASSERT_EQ(r.restart_grade, IpqpRestartGrade::kFullWarm) << "the seed must be consumed";
    EXPECT_EQ(r.counters.ipqp_mu_adopted, 1);

    // NON-VACUITY on the same branch: a payload below the measured floor
    // adopts nothing.
    IpqpSeed low = clean;
    low.mu = 1.0e-15;
    IpqpEngine tier2(tight_opts());
    const IpqpResult t = tier2.solve(qp, &low, off, SolveOverrides{});
    ASSERT_EQ(t.restart_grade, IpqpRestartGrade::kFullWarm);
    EXPECT_EQ(t.counters.ipqp_mu_adopted, 0);
}

// F3: section 5.5's trust threshold has TWO halves, and the relative predicate
// carries only one. A seed whose residuals scale small but whose raw barrier
// level is far above the tier's target is NOT trusted at iteration zero.
TEST(IpqpWarmRestart, AHighBarrierSeedIsNotTrustedAtIterationZero) {
    const QpProblem qp = interior_qp();
    IpqpSeed high = seed_for(qp);
    // Equal prices, so the stationarity residual cancels exactly and the
    // relative complementarity divides by the multiplier scale -- both halves
    // of the relative test pass while mu itself is 1e-2.
    high.zl = vec({1.0e-2});
    high.zu = vec({1.0e-2});
    high.mu = 1.0e-2;
    high.grade = IpqpRestartGrade::kFullWarm;

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &high, IpqpOptions{}, SolveOverrides{});
    EXPECT_GT(r.counters.ipqp_iters, 0)
        << "an untrusted warm seed may not be adopted as converged before it takes a step";
    EXPECT_EQ(r.status, QpStatus::kOptimal);

    // NON-VACUITY: the SAME shape at a barrier level inside the target IS
    // trusted and finishes at iteration zero.
    const IpqpSeed trusted = centred_full_seed();
    IpqpEngine tier2(tight_opts());
    const IpqpResult t = tier2.solve(qp, &trusted, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(t.counters.ipqp_iters, 0);
}

// F4: the box clamp and the slack recompute are repairs like any other -- a
// repair that moved the ingested seed may not report zero.
TEST(IpqpWarmRestart, TheBoxClampCountsAsARepair) {
    const QpProblem qp = interior_qp();
    IpqpSeed outside = centred_full_seed();
    outside.x = vec({-5.0}); // outside the declared box entirely

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &outside, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.counters.ipqp_restart_repairs, 1);
    EXPECT_GT(r.counters.ipqp_restart_shift_max, 3.0)
        << "the clamp moved x by about 4, and the honest-magnitude field must say so";

    // NON-VACUITY: the same seed already inside the box pays no clamp.
    const IpqpSeed inside = centred_full_seed();
    IpqpEngine tier2(tight_opts());
    const IpqpResult t = tier2.solve(qp, &inside, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(t.counters.ipqp_restart_repairs, 0);
}

// ---------------------------------------------------------------------------
// Determinism (M5 R5, restated for the warm path)
// ---------------------------------------------------------------------------

TEST(IpqpWarmRestart, TheSameSeedTwiceProducesBitIdenticalResults) {
    const QpProblem qp = stale_qp();
    IpqpSeed seed = seed_for(qp);
    seed.x = vec({-2.0, 3.0});
    seed.zl = vec({1.0e-3, 2.0e-3});
    seed.zu = vec({3.0e-3, 4.0e-3});
    seed.mu = 5.0e-3;

    IpqpEngine a(tight_opts());
    IpqpEngine b(tight_opts());
    const IpqpResult ra = a.solve(qp, &seed, IpqpOptions{}, SolveOverrides{});
    const IpqpResult rb = b.solve(qp, &seed, IpqpOptions{}, SolveOverrides{});

    ASSERT_EQ(ra.status, QpStatus::kOptimal);
    EXPECT_EQ(ra.counters.ipqp_iters, rb.counters.ipqp_iters);
    EXPECT_EQ(ra.counters.ipqp_factorizations, rb.counters.ipqp_factorizations);
    EXPECT_EQ(ra.counters.ipqp_restart_repairs, rb.counters.ipqp_restart_repairs);
    EXPECT_DOUBLE_EQ(ra.counters.ipqp_restart_shift_max, rb.counters.ipqp_restart_shift_max);
    EXPECT_EQ(ra.x, rb.x);
    EXPECT_EQ(ra.zl, rb.zl);
    EXPECT_EQ(ra.zu, rb.zu);
    EXPECT_EQ(ra.mu, rb.mu);
}

// ---------------------------------------------------------------------------
// Section 5.2 -- the repair, and the lever that turns it off
// ---------------------------------------------------------------------------

TEST(IpqpWarmRestart, TheRepairClampsAWrongSignedPriceAndRecordsItsLargestShift) {
    const QpProblem qp = stale_qp();
    IpqpSeed bad = seed_for(qp);
    bad.x = vec({0.0, 0.0});
    bad.zl = vec({-5.0, 1.0e-3}); // a negative price: not a price at all
    bad.zu = vec({1.0e-3, 1.0e-3});
    bad.mu = 1.0e-3;

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, &bad, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.counters.ipqp_restart_repairs, 1);
    EXPECT_GE(r.counters.ipqp_restart_shift_max, 5.0)
        << "the honest-magnitude field reports the size of the largest component move";

    // NON-VACUITY: a seed that needs no repair reports neither.
    IpqpSeed fine = seed_for(qp);
    fine.x = vec({0.0, 0.0});
    fine.zl = vec({1.0e-3, 1.0e-3});
    fine.zu = vec({1.0e-3, 1.0e-3});
    fine.mu = 5.0e-3;
    IpqpEngine tier2(tight_opts());
    const IpqpResult ok = tier2.solve(qp, &fine, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(ok.counters.ipqp_restart_repairs, 0);
    EXPECT_DOUBLE_EQ(ok.counters.ipqp_restart_shift_max, 0.0);
}

// ---------------------------------------------------------------------------
// A12 -- the perturbed-continuation cell, at the driver
// ---------------------------------------------------------------------------

// FIXTURE A12 IN ITS hven SHAPE (plan section 3; risk R7 -- the MECHANISM, not tycho's numbers).
// F3's spring chain has a bound-activation threshold at p = 0.5, so a value exported on the FREE
// branch and re-staged on the CLAMPED one is stale. See `.superpowers/w1-t7-report.md`.
TEST(IpqpWarmRestart, APerturbedContinuationAcrossAnActivationThresholdIsAbandonedAndRecovers) {
    const auto solve_continuation = [](Index warm_budget) {
        test_support::F3SpringChain model(20, 0.5, 0.25);
        SqpOptions o;
        o.qp_mode = QpMode::kIpm;
        o.ipqp.ipqp_warm_iter_budget = warm_budget;
        SqpDriver driver(o);

        // The FREE-branch solve, and its currency.
        const SqpSolution seeded = driver.solve(model, model.start_point());
        EXPECT_EQ(seeded.status, SolveStatus::kOptimal);
        const WarmStartData payload = driver.export_warm_start();

        // The perturbation: past the activation threshold, so the exported
        // point's active set is the wrong one for the problem being solved.
        model.set_parameters(Vec::Constant(1, 0.8));
        driver.stage_warm_start(payload);
        return driver.solve(model, payload.primal_);
    };

    const SqpSolution killed = solve_continuation(1);
    EXPECT_EQ(killed.status, SolveStatus::kOptimal)
        << "the cold restart recovers the answer the warm attempt was not reaching";
    EXPECT_GE(killed.counters.ipqp.ipqp_warm_restart_abandoned, 1)
        << "and the kill fired inside the clamped budget";

    // NON-VACUITY: at the shipped budget the same continuation is not
    // abandoned, so the row above is about the budget and not about the cell.
    const SqpSolution patient = solve_continuation(IpqpOptions{}.ipqp_warm_iter_budget);
    EXPECT_EQ(patient.status, SolveStatus::kOptimal);
    EXPECT_EQ(patient.counters.ipqp.ipqp_warm_restart_abandoned, 0);

    // AND THE TWO AGREE ON THE ANSWER: a killed warm attempt is a cost, never a
    // different problem.
    ASSERT_EQ(killed.x.size(), patient.x.size());
    EXPECT_LT((killed.x - patient.x).lpNorm<Eigen::Infinity>(), 1e-6);
}

} // namespace hven::solvers
