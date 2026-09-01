// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_ipqp_seams.cpp -- the M6 W1 task 5 pins for the code paths no
// LEGAL subproblem can reach on this backend: section 2.2's evidence-failure
// policy, and the two terminal inertia states the section 2.2 item 4
// certification read can report.
//
// THIS FILE RUNS ONLY ON THE `hven_ipqp_seam_tests` TARGET
// (tests/CMakeLists.txt), which recompiles the tier's own sources with
// HVEN_TESTING and does NOT link hven::hven. See docs/testing.md for the
// convention and tests/linear/test_fault_injection.cpp for the layer below.
//
// WHY THESE PATHS NEED A SEAM, in the words of the measurements that found
// them (task 4's report, section F8): MKL's static pivot perturbation fires on
// matrices the ladder's own `delta` growth resolves BEFORE any terminal
// reading, so no legal subproblem reaches the ladder's ceiling still
// perturbed; and no MKL path declines to report inertia at all, so
// `InertiaEvidence::State::kQueryFailed` never arrives. The `kUnavailable`
// state the specification names is an ACCELERATE path this machine cannot run
// -- under CLAUDE.md section 6's never-fabricate rule that arm stays
// UNOBSERVED on real hardware, and what is pinned here is the POLICY the tier
// applies when it is told that state, which is a different (and checkable)
// claim.
//
// EVERY INJECTION IS ASSERTED TO HAVE HAPPENED. `IpqpInertiaEvidenceInjector::
// injections` is checked in every fixture, so an injector that silently
// stopped applying fails its own pin instead of passing as a clean solve.

#include <limits>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/ipqp_fault_injection.h>

namespace hven::solvers {
namespace {

using hven::linear::InertiaEvidence;
using Injector = detail::testing::IpqpInertiaEvidenceInjector;
using Observer = detail::testing::IpqpInertiaReadObserver;

constexpr double kInf = std::numeric_limits<double>::infinity();

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

QpOptions tight_opts() {
    QpOptions o;
    o.tr_radius = kInf;
    return o;
}

/// A strictly convex QP with both row blocks -- solved in eleven iterations
/// with a standing certificate when nothing is injected, which is what makes
/// every assertion below a statement about the INJECTION.
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

/// Evidence in a state OTHER than kObserved, with the counts left at the
/// linear layer's own invalid sentinel `-1`. Section 2.2: "the counts are
/// never zero-filled or inferred" -- so the fixture hands the tier exactly
/// what a real failed query hands it, and nothing more plausible-looking.
InertiaEvidence unusable(InertiaEvidence::State state) {
    InertiaEvidence e;
    e.state = state;
    e.n_pos = -1;
    e.n_neg = -1;
    e.n_zero = -1;
    e.zero_is_derived = false;
    e.perturbed_pivots = std::nullopt;
    return e;
}

/// A perfectly good reading that ALSO reports perturbed pivots. Section 2.2:
/// "the inertia is not evidence about the assembled matrix -- the backend
/// factorized a different one." The counts are the RIGHT ones on purpose, so
/// the pin cannot pass by the tier rejecting the numbers instead of the
/// perturbation.
InertiaEvidence perturbed(Index n_pos, Index n_neg) {
    InertiaEvidence e;
    e.state = InertiaEvidence::State::kObserved;
    e.n_pos = n_pos;
    e.n_neg = n_neg;
    e.n_zero = 0;
    e.zero_is_derived = true;
    e.perturbed_pivots = 3;
    return e;
}

class IpqpSeamTest : public ::testing::Test {
  protected:
    void SetUp() override {
        Injector::reset();
        Observer::reset();
    }
    void TearDown() override {
        Injector::reset();
        Observer::reset();
    }
};

} // namespace

// ---------------------------------------------------------------------------
// The seam itself
// ---------------------------------------------------------------------------

TEST_F(IpqpSeamTest, WithNothingInjectedTheTierSolvesExactlyAsItDoesInProduction) {
    // THE CONTROL. Without this every pin below could be measuring the seam's
    // own presence rather than the injected scenario.
    Observer::active = true;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

    EXPECT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
    EXPECT_FALSE(r.certificate_downgraded);
    EXPECT_FALSE(r.inertia_evidence_failed);
    EXPECT_EQ(Injector::injections, 0);

    // The observer saw every reading the tier took -- one per iteration plus
    // the section 2.2 item 4 certification read -- and they were REAL: MKL
    // reports observed inertia with a pivot count present and zero.
    EXPECT_GT(Observer::reads, r.counters.ipqp_iters);
    EXPECT_EQ(Observer::final_reads, 1);
    EXPECT_EQ(Observer::last_final.state, InertiaEvidence::State::kObserved);
    ASSERT_TRUE(Observer::last_final.perturbed_pivots.has_value());
    EXPECT_EQ(*Observer::last_final.perturbed_pivots, 0);
}

// ---------------------------------------------------------------------------
// Section 2.2's evidence-failure policy -- the MID-SOLVE readings
// ---------------------------------------------------------------------------

TEST_F(IpqpSeamTest, AnUnusableEvidenceStateStepsAtAConservativeFloorAndDowngradesTheSolve) {
    // SECTION 2.2, VERBATIM: "InertiaEvidence::State != kObserved
    // (kQueryFailed, or kUnavailable as on some Accelerate paths): a step is
    // permitted ONLY at a conservative rho floor AND the certificate is
    // downgraded for the whole solve."
    //
    // BOTH STATES, because section 2.2 names both and the tier must not
    // distinguish them: a query that failed and a backend that cannot answer
    // leave the tier with exactly the same amount of evidence, which is none.
    for (const InertiaEvidence::State state :
         {InertiaEvidence::State::kQueryFailed, InertiaEvidence::State::kUnavailable}) {
        SCOPED_TRACE(static_cast<int>(state));
        Injector::reset();
        Observer::reset();
        Injector::active = true;
        Injector::on_final_read = false; // the mid-solve policy is what is under test
        Injector::evidence = unusable(state);
        Observer::active = true;

        IpqpEngine tier(tight_opts());
        const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

        // THE INJECTION HAPPENED.
        ASSERT_GT(Injector::injections, 0);

        // A STEP WAS PERMITTED: the solve did not stop at the first unusable
        // reading. This is the assertion that separates task 5's behaviour
        // from task 4's, which terminated here with kNumerical.
        EXPECT_GT(r.counters.ipqp_iters, 0);
        EXPECT_NE(r.escape_reason, IpqpEscape::kNumerical);

        // AT A CONSERVATIVE FLOOR: the conservative floor was armed at the
        // POLICY'S own constant and never climbed from there. Pinned against
        // `kIpqpEvidenceFailureRhoFloor` and NOT against `kIpqpLadderInit`,
        // which happens to hold the same value today: a retune of the ladder's
        // first rung on ladder evidence must FAIL this pin rather than move the
        // evidence-failure policy along with it (co-review I-3).
        EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, detail::kIpqpEvidenceFailureRhoFloor);
        EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);

        // AND THE CERTIFICATE IS DOWNGRADED FOR THE WHOLE SOLVE.
        EXPECT_TRUE(r.inertia_evidence_failed);
        EXPECT_TRUE(r.certificate_downgraded);
        EXPECT_NE(r.status, QpStatus::kOptimal);

        // THE COUNTS WERE NEVER ZERO-FILLED. What the tier read is what the
        // backend would have handed it: invalid counts and an ABSENT pivot
        // count, not a plausible-looking zero.
        EXPECT_EQ(Observer::last_injected.state, state);
        EXPECT_EQ(Observer::last_injected.n_pos, -1);
        EXPECT_EQ(Observer::last_injected.n_neg, -1);
        EXPECT_EQ(Observer::last_injected.n_zero, -1);
        EXPECT_FALSE(Observer::last_injected.perturbed_pivots.has_value());
        // The section 2.2 item 4 read was NOT injected on this arm, and the
        // observer proves it: what the tier read there is the backend's own
        // observed evidence, which is what makes the outcome above a
        // statement about the MID-SOLVE policy alone.
        EXPECT_EQ(Observer::last_final.state, InertiaEvidence::State::kObserved);
    }
}

TEST_F(IpqpSeamTest, TheConservativeFloorIsPaidOnceAndTheSolveStillFinishes) {
    // THE COST CLAIM, and it is why the policy is "a step at a conservative
    // floor" rather than "a ladder". A ladder has no stopping criterion when
    // the reading can never come back right, so it would spend the whole
    // ceiling's worth of factorizations and take the same step at the end.
    // One extra factorization is the price.
    Injector::active = true;
    Injector::on_final_read = false;
    Injector::evidence = unusable(InertiaEvidence::State::kQueryFailed);

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_TRUE(r.inertia_evidence_failed);

    // The clean solve of the same problem, for the comparison.
    Injector::reset();
    IpqpEngine clean(tight_opts());
    const IpqpResult c = clean.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(c.status, QpStatus::kOptimal);

    // THE FLOOR IS THE LADDER'S ABSOLUTE FIRST RUNG, and the solve still
    // reaches the SAME ANSWER as the clean one rather than the proximally
    // biased point a level-proportional floor produces. That is the whole
    // content of decision 1 at the branch: measured with `rho * 100` instead,
    // this fixture converged to x = (0.0026, 0.0049) against (0.75, 0.25) and
    // burned its entire 60-iteration budget.
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, detail::kIpqpEvidenceFailureRhoFloor);
    EXPECT_LT(r.counters.ipqp_iters, IpqpOptions{}.ipqp_hard_iter_cap);
    EXPECT_NEAR(r.x(0), c.x(0), 1e-6);
    EXPECT_NEAR(r.x(1), c.x(1), 1e-6);
    // The extra factorizations the policy bought are BOUNDED and small: the
    // floor is applied once, so the only rungs paid are the ones where the
    // schedule had already decayed below it.
    EXPECT_LE(r.counters.ipqp_inertia_retries, r.counters.ipqp_iters);
    EXPECT_EQ(r.counters.ipqp_inertia_retries, r.counters.ipqp_reg_increases);
}

TEST_F(IpqpSeamTest, AnAbsentPerturbedPivotCountReachesTheTierAsAbsentAndNotAsZero) {
    // SECTION 2.2'S HONESTY RULE, and T3.a's whole reason for existing:
    // `KktFactorization`'s three cached ints collapse an ABSENT pivot count
    // to the integer 0, which on a backend that does count pivots means "none
    // were perturbed". The tier reads `inertia_evidence()` instead, and the
    // difference is invisible in every output the tier produces -- so the
    // observer is what makes it checkable.
    //
    // The injected reading is a GOOD one with the pivot count absent
    // (Accelerate's honest state). The tier must certify: absent is not a
    // perturbation report.
    InertiaEvidence e;
    e.state = InertiaEvidence::State::kObserved;
    e.n_pos = 3; // n + mi = 2 + 1
    e.n_neg = 2; // me + mi = 1 + 1
    e.n_zero = 0;
    e.zero_is_derived = true;
    e.perturbed_pivots = std::nullopt;

    Injector::active = true;
    Injector::evidence = e;
    Observer::active = true;

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

    ASSERT_GT(Injector::injections, 0);
    EXPECT_FALSE(Observer::last.perturbed_pivots.has_value()); // nullopt, NEVER 0
    EXPECT_EQ(Observer::last.n_pos, 3);
    EXPECT_EQ(Observer::last.n_neg, 2);
    // The signature agreed and the pivot count was absent rather than
    // nonzero, so nothing here is a reason to withhold a certificate.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
    EXPECT_FALSE(r.certificate_downgraded);
    EXPECT_EQ(r.status, QpStatus::kOptimal);

    // MUTATION PARTNER: the SAME reading with the count PRESENT and nonzero
    // is a perturbation report, and the outcome inverts. Absent and zero and
    // nonzero are three states, and this pair is what proves the tier reads
    // all three apart.
    Injector::reset();
    Observer::reset();
    InertiaEvidence p = e;
    p.perturbed_pivots = 3;
    Injector::active = true;
    Injector::evidence = p;
    IpqpEngine tier2(tight_opts());
    const IpqpResult r2 = tier2.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_NE(r2.status, QpStatus::kOptimal);
    // A perturbed reading on EVERY factorization exhausts the `delta` ladder
    // -- section 2.2's remedy for a perturbed pivot is a larger DUAL shift,
    // and there is no shift at which a fabricated perturbation report stops --
    // so the solve stops before it can reach the section 2.2 item 4 read at
    // all. `certificate_downgraded` is therefore FALSE here and that is
    // correct rather than a gap: there was no certifying exit to downgrade.
    EXPECT_EQ(r2.escape_reason, IpqpEscape::kNumerical);
    EXPECT_FALSE(r2.certificate_downgraded);
    EXPECT_EQ(r2.counters.ipqp_escape_numerical, 1);
}

// ---------------------------------------------------------------------------
// The BOUNDED perturbed-pivot re-route (T4b fix round 1, settler ruling R2)
// ---------------------------------------------------------------------------

/// An indefinite box QP whose ladder arms on the first iteration: `x1 = 0` is
/// an exact KKT point of the barrier problem at every `mu` on a symmetric box,
/// so the tier converges there in three iterations and the section 2.2 item 4
/// read catches the saddle. Used here only for its LADDER, which is what the
/// re-route rides.
QpProblem armed_saddle_qp() {
    QpProblem qp;
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1.0}});
    qp.g = vec({0.0, 0.0});
    qp.Ae = dense_rows({}, 2);
    qp.be = Vec(0);
    qp.Ai = dense_rows({}, 2);
    qp.bi = Vec(0);
    qp.lower = vec({-10.0, -10.0});
    qp.upper = vec({10.0, 10.0});
    return qp;
}

TEST_F(IpqpSeamTest, AnUNCLEARABLEPerturbationFallsBackToTheDualShiftAfterTwoPrimalRungs) {
    // T4b FIX ROUND 1, SETTLER RULING R2 -- THE RE-ROUTE'S BOUND, AND THE
    // DUAL-CAUSE CASE IT EXISTS FOR.
    //
    // T4b re-routed a perturbed-pivot report to the PRIMAL ladder while that
    // ladder is armed, because a uniform shift of the Ruiz-scaled system can
    // ANNIHILATE a scaled diagonal (Ruiz normalizes a dominant diagonal to
    // almost exactly `-1`, and `rho_dem = 1` is a rung of Algorithm IC's own
    // first climb) and that singularity is primal. But a perturbation whose
    // cause is DUAL -- near-dependent equality or inequality rows -- is cleared
    // by NO primal rung, and an unbounded re-route would ride the ladder to
    // `ipqp_reg_max` and escape on exhaustion to reach an answer the dual
    // escalation gives in one factorization.
    //
    // WHY THIS IS A SEAM TEST AND NOT A QP. The honest instrument would be the
    // pivot BLOCK the backend perturbed, and
    // `hven::linear::InertiaEvidence` does not carry pivot LOCATIONS -- only
    // counts. So the engine cannot ask, and neither can a fixture: no legal
    // QP in this suite produces a perturbation the primal ladder cannot clear
    // (MKL's static pivoting resolves the row-degenerate cases through the
    // tier's own `delta`, which is why this header exists at all). The
    // scenario is reached by injecting a reading of that shape, which is a
    // POLICY witness in this header's own vocabulary: it pins what the tier
    // DOES with an unclearable perturbation, not that a backend produces one.
    Injector::active = true;
    Injector::skip_first = 2; // let the ladder ARM on two real wrong readings
    Injector::evidence = perturbed(/*n_pos=*/2, /*n_neg=*/0);

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(armed_saddle_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

    ASSERT_GT(Injector::injections, 0) << "the injection must have applied, or nothing is tested";

    // THE BOUND, EXACTLY: two primal escalations, then the fallback. Not one,
    // not the whole ladder.
    EXPECT_EQ(r.counters.ipqp_pivot_reroute_primal, detail::kIpqpPivotReroutePrimalMax);
    EXPECT_EQ(r.counters.ipqp_pivot_reroute_dual_fallback, 1);
    EXPECT_EQ(r.counters.ipqp_pivot_reroute_primal, 2)
        << "the constant is two, and the pin names the number as well as the constant so a "
           "retune is visible here";

    // ... AND THE FALLBACK COSTS EXACTLY TWO EXTRA FACTORIZATIONS over the
    // pre-T4b always-dual rule, which is the price the ruling accepted.
    Injector::reset();
    Observer::reset();
    Injector::active = true;
    Injector::skip_first = 2;
    Injector::evidence = perturbed(/*n_pos=*/2, /*n_neg=*/0);
    Injector::on_iteration_reads = true;
    IpqpOptions capped;
    capped.ipqp_max_factorizations = 3; // the two real reads plus one
    IpqpEngine tier2(tight_opts());
    const IpqpResult r2 = tier2.solve(armed_saddle_qp(), nullptr, capped, SolveOverrides{});
    EXPECT_EQ(r2.counters.ipqp_factorizations, 3);
    EXPECT_LE(r2.counters.ipqp_pivot_reroute_primal, detail::kIpqpPivotReroutePrimalMax);

    // A PERTURBATION AT AN UNARMED LADDER IS NOT RE-ROUTED AT ALL -- the
    // pre-T4b dual rule, unchanged, and the reason the convex corpus is
    // bit-identical across T4b. Neither route counter fires.
    Injector::reset();
    Observer::reset();
    Injector::active = true;
    Injector::evidence = perturbed(/*n_pos=*/3, /*n_neg=*/2);
    IpqpEngine tier3(tight_opts());
    const IpqpResult r3 = tier3.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(r3.counters.ipqp_pivot_reroute_primal, 0);
    EXPECT_EQ(r3.counters.ipqp_pivot_reroute_dual_fallback, 0);
    EXPECT_EQ(r3.escape_reason, IpqpEscape::kNumerical);
}

TEST_F(IpqpSeamTest, AStepTakenAfterADeltaEscalationIsBuiltFromTheSCHEDULEsDelta) {
    // T4b FIX ROUND 1, SETTLER RULING R3 -- THE DUAL HALF OF THE 2.1
    // SEPARATION, PINNED ON A STEP THAT ACTUALLY GETS TAKEN.
    //
    // Round 1 passed the LADDER-SETTLED `delta` to `build_rhs` while the
    // section 3.2 gate measured at `delta_sched`, so an iteration whose
    // perturbed branch raised `delta` from 8 to 800 solved the 800
    // dual-proximal system while the gate watched the 8 one -- mechanism 4's
    // shape on the dual side. R3 reverses that: the escalated `delta` enters
    // the DIAGONAL only.
    //
    // WHY THE INJECTION WINDOW. A perturbation injected FOREVER can only be
    // observed at a terminal state, and no step is ever taken there, so it
    // cannot distinguish the two wirings. `max_injections` closes the window
    // after ONE replacement: the first iteration's read is perturbed (so
    // `delta` escalates once), the next read is the real one (so the ladder
    // succeeds and a STEP IS TAKEN), and that step is built from whichever
    // `delta` the code passes. The two wirings give different iterates and
    // therefore different iteration counts, which is what this pin holds down.
    // THE INJECTION LANDS MID-SOLVE, NOT AT ITERATION 0, and that placement is
    // load-bearing AND MEASURED. The dual proximal term is
    // `delta (y - lambda_est)`, and wherever the gate has just advanced the two
    // are equal, so the term is ZERO and the two wirings are indistinguishable.
    // A placement scan over `skip_first` (run out of tree against both wirings)
    // shows 0, 1, 3 and 7 give identical trajectories and 2, 4, 5, 6 and 8 do
    // not; `4` is taken because its separation is the widest -- the round-1
    // wiring reaches the optimum in 13 iterations and 15 factorizations there,
    // this one in 11 and 13.
    Injector::active = true;
    Injector::on_final_read = false;
    Injector::skip_first = 4;
    Injector::max_injections = 1;
    Injector::evidence = perturbed(/*n_pos=*/3, /*n_neg=*/2);

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

    EXPECT_EQ(Injector::injections, 1) << "exactly one read was corrupted";
    // The escalation really happened: one extra rejection and one extra
    // factorization over the un-injected solve, and NO primal re-route (the
    // ladder was unarmed, which is the ordinary dual route).
    EXPECT_EQ(r.counters.ipqp_inertia_retries, 1);
    EXPECT_EQ(r.counters.ipqp_pivot_reroute_primal, 0);
    EXPECT_EQ(r.counters.ipqp_pivot_reroute_dual_fallback, 0);
    // The solve then recovers and certifies.
    EXPECT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
    // THE SCHEDULE'S OWN DUAL VALUE IS WHAT LEAVES THE SOLVE, never the
    // escalated one.
    EXPECT_LE(r.delta, IpqpOptions{}.ipqp_delta_init);

    // THE PIN IS THE INJECTED SOLVE'S EXACT TRAJECTORY. The escalation changes
    // the MATRIX of the recovering factorization legitimately (that is what
    // `delta` is for), so the injected solve is NOT expected to reach the clean
    // solve's iterate -- what is pinned is that it reaches THIS one, which is
    // the trajectory the schedule's `delta` in the right-hand side produces. A
    // source mutation passing the LADDER-SETTLED `delta` to `build_rhs` -- the
    // round-1 wiring -- moves it; that mutation was run out of tree and is
    // recorded with its numbers in the T4b fix-round-1 report, exactly as
    // gate 9's two mutations are. It is not runnable from here.
    IpqpEngine clean_tier(tight_opts());
    Injector::reset();
    Observer::reset();
    const IpqpResult clean =
        clean_tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(clean.x.size(), r.x.size());
    EXPECT_EQ(clean.counters.ipqp_inertia_retries, 0)
        << "the clean solve pays no rejection -- the injection is the whole difference";
    EXPECT_LT((r.x - clean.x).lpNorm<Eigen::Infinity>(), 1e-6)
        << "both solves reach the same optimum; only the route differs";
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("t4b_r3_accelerate", "UNOBSERVED -- the exact injected trajectory is MKL-only");
#else
    EXPECT_EQ(r.counters.ipqp_iters, 11)
        << "the round-1 wiring (the ladder-settled delta in build_rhs) reaches 13 here";
    EXPECT_EQ(r.counters.ipqp_factorizations, 13) << "the round-1 wiring reaches 15 here";
#endif
}

// ---------------------------------------------------------------------------
// The section 2.2 item 4 read's two terminal states (task 4's F8 gap)
// ---------------------------------------------------------------------------

TEST_F(IpqpSeamTest, ATerminalUNREADABLEFinalReadIsTwoAndEscapesNumerical) {
    // PLAN SECTION 7 NOTE (h): "an UNREADABLE inertia (evidence state not
    // observed)" is `ipqp_final_inertia_read == 2` and the NUMERICAL escape
    // class -- never `1`, which is reserved for a reading that WAS taken and
    // disagreed and which routes to SSN as a saddle-suspect instead.
    //
    // THE EVIDENCE-FAILURE POLICY DOES NOT REACH THIS BRANCH, which is task
    // 5's own ruling: the policy permits A STEP, and the item 4 read takes no
    // step. There is nothing left to permit, so the certificate falls.
    Injector::active = true;
    Injector::on_iteration_reads = false; // let the solve converge normally
    Injector::evidence = unusable(InertiaEvidence::State::kQueryFailed);

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

    EXPECT_EQ(Injector::injections, 1);      // exactly the one certification read
    EXPECT_FALSE(r.inertia_evidence_failed); // no MID-solve failure happened
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 2);
    EXPECT_TRUE(r.certificate_downgraded);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNumerical);
    EXPECT_EQ(r.status, QpStatus::kNumericalError);
    EXPECT_EQ(r.counters.ipqp_escapes, 1);
    EXPECT_EQ(r.counters.ipqp_escape_numerical, 1);
    EXPECT_EQ(r.counters.ipqp_escape_indefinite, 0);
}

TEST_F(IpqpSeamTest, ATerminalPERTURBEDFinalReadIsAlsoTwoAndAlsoEscapesNumerical) {
    // FIX ROUND 1'S C0b, PINNED END TO END FOR THE FIRST TIME. A perturbed
    // factorization "is not evidence about the assembled matrix" (spec 2.2),
    // so it is not "a reading that disagreed" -- it is NO reading, and the
    // honest outcome is note (h)'s `2` plus kNumerical, exactly as for an
    // unobservable state. The injected counts are the CORRECT ones, so this
    // cannot pass by the tier rejecting the numbers.
    Injector::active = true;
    Injector::on_iteration_reads = false;
    Injector::evidence = perturbed(/*n_pos=*/3, /*n_neg=*/2);

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

    EXPECT_EQ(Injector::injections, 1);
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 2);
    EXPECT_TRUE(r.certificate_downgraded);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNumerical);
    EXPECT_EQ(r.counters.ipqp_escape_numerical, 1);
    // AND EXACTLY ONE FACTORIZATION was spent on the read -- fix round 1
    // deleted the perturbed retry, and this is the cost statement section 2.2
    // item 4 makes as a number ("+1 factorization per certified guarded
    // solve") held down on the perturbed path too.
    Injector::reset();
    IpqpEngine clean(tight_opts());
    const IpqpResult c = clean.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(r.counters.ipqp_factorizations, c.counters.ipqp_factorizations);
}

TEST_F(IpqpSeamTest, AWrongTerminalReadStaysIndefiniteAndIsNotFoldedIntoNumerical) {
    // THE PARTITION'S OTHER SIDE, pinned beside the two above so the boundary
    // note (h) draws is exercised from both directions: a reading that WAS
    // observed and DISAGREED is `1` and kIndefinite, and it must not drift
    // into the numerical class that the two unusable states above land in.
    InertiaEvidence wrong;
    wrong.state = InertiaEvidence::State::kObserved;
    wrong.n_pos = 2; // one short of n + mi = 3, and n_pos + n_neg still == dim
    wrong.n_neg = 3;
    wrong.n_zero = 0;
    wrong.zero_is_derived = true;
    wrong.perturbed_pivots = 0;

    Injector::active = true;
    Injector::on_iteration_reads = false;
    Injector::evidence = wrong;

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

    EXPECT_EQ(Injector::injections, 1);
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
    EXPECT_EQ(r.counters.ipqp_escape_indefinite, 1);
    EXPECT_EQ(r.counters.ipqp_escape_numerical, 0);
}

TEST_F(IpqpSeamTest, TheSkipCountLetsASolveConvergeBeforeItsLastReadingIsCorrupted) {
    // The seam's own contract, exercised: `skip_first` counts ELIGIBLE reads,
    // so a fixture can leave the trajectory untouched and corrupt only what
    // decides the certificate. This is also the pin that the two flags and
    // the skip counter compose -- three knobs that silently disagreed would
    // make every fixture above ambiguous about what it injected.
    Injector::active = true;
    Injector::on_iteration_reads = true;
    Injector::on_final_read = true;
    Injector::skip_first = 1000; // more reads than any solve here takes
    Injector::evidence = unusable(InertiaEvidence::State::kQueryFailed);

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(convex_qp(), nullptr, IpqpOptions{}, SolveOverrides{});

    EXPECT_EQ(Injector::injections, 0);
    EXPECT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_FALSE(r.certificate_downgraded);
    EXPECT_GT(Injector::skip_first, 0); // and the budget was consumed, not ignored
    EXPECT_LT(Injector::skip_first, 1000);
}

} // namespace hven::solvers
