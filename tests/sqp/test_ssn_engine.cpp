// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>

#include "support/ssn_fixtures.h"

using namespace hven::solvers;
using hven::Index;
using hven::Vec;

namespace {

// THE ANALYTIC FIXTURES live in tests/sqp/support/ssn_fixtures.h, shared with
// bench/ssn_safeguard_probe.cpp so the safeguards note has a committed
// reproduction vehicle (review fix round 1, I5). The banner that used to sit
// here -- every iteration count below is an OBSERVED VALUE measured with
// clang++ against MKL Pardiso, ============ MKL-OBSERVED. NOT YET
// RE-VERIFIED ON APPLE ACCELERATE. ============ -- travels with them.
using namespace hven::solvers::test_support;

QpOptions default_opts() { return QpOptions{}; }

// **THE FACTORIZATION INVARIANT, IN ONE PLACE** (review fix round 1, C1). A
// CERTIFYING solve under kFull pays one factorization per accepted step plus
// ONE more: the second-order verification read at the point it certifies
// (ssn_engine.h section 7b). kBare issues no second-order certificate and pays
// exactly one per step. Every benign fixture asserts through this helper, so
// the cost of the certificate is stated once and checked everywhere -- and a
// regression that refactorized per branch change still shows up as a mismatch
// rather than being absorbed.
Index certified_factorizations(Index iters) { return iters + 1; }

SsnOptions bare_opts() {
    SsnOptions s;
    s.safeguards = SsnSafeguards::kBare;
    return s;
}

// The implied partition at the point a solve RETURNED, as a string, so a
// trajectory can be compared state-by-state. 'A'/'.' per inequality row, then
// 'L'/'U'/'X'/'F' per variable.
std::string partition_string(const SsnResult &res) {
    std::string p;
    for (const bool a : res.ineq_active) {
        p += a ? 'A' : '.';
    }
    p += '|';
    for (const BoundState b : res.bound_state) {
        p += b == BoundState::kAtLower   ? 'L'
             : b == BoundState::kAtUpper ? 'U'
             : b == BoundState::kFixed   ? 'X'
                                         : 'F';
    }
    return p;
}

// The walk's answer to the same QP, used as the independent oracle: the two
// kernels share qp_problem.h, the sign convention and the regularizers, and
// nothing else, so agreement is real evidence rather than a tautology.
QpSolution walk_solution(const QpProblem &qp) {
    QpEngine engine(default_opts());
    return engine.solve(qp);
}

} // namespace

// =============================================================================
// (a) Exact primal seed + correct hint: ONE iteration
// =============================================================================
TEST(SsnEngineLocal, CorrectHintFromExactPrimalIsOneIteration) {
    const QpProblem qp = two_row_qp();
    SsnEngine engine(default_opts());

    SsnStart start;
    start.x = Vec(2);
    start.x << 1.0, 1.0; // the exact primal solution
    // Multipliers deliberately NOT seeded: the hint alone has to recover them.
    start.activity_hint.ineq = {true, false};

    SsnOptions sopts;
    SsnResult res;
    engine.solve(qp, start, sopts, &res);

    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.escape_reason, SsnEscape::kNone);
    EXPECT_EQ(res.iters, 1);
    EXPECT_EQ(res.counters.ssn_iters, 1);
    EXPECT_LT(res.fb_residual, sopts.fb_tol);
    EXPECT_NEAR(res.x(0), 1.0, 1e-8);
    EXPECT_NEAR(res.x(1), 1.0, 1e-8);
    EXPECT_NEAR(res.lambda_i(0), 1.0, 1e-8);
    EXPECT_NEAR(res.lambda_i(1), 0.0, 1e-8);
    // One PDAS solve; no second branch selection ever happened, so nothing
    // could have flipped. The verification attempt is deliberately not counted
    // here (ssn_engine.h section 7b) -- it is not a step, so it cannot flip a
    // partition.
    EXPECT_EQ(res.counters.ssn_bulk_flips, 0);
    // **THE COST OF THE CERTIFICATE, PINNED** (review fix round 1, C1). One
    // accepted step and TWO factorizations: the step's own, and the
    // second-order verification at the point being certified. Under kBare --
    // which issues no second-order certificate at all -- it is one and one, and
    // BareModeReproducesTheTask3Trajectories pins that. This is the sharpest
    // single number in the fix: it is the G1 warm-hand-off payoff path, and the
    // extra factorization is what buys "kOptimal means a minimizer".
    EXPECT_EQ(res.iters, 1);
    EXPECT_EQ(res.factorizations, 2);
    {
        SsnEngine bare_engine(default_opts());
        SsnResult bare_res;
        bare_engine.solve(qp, start, bare_opts(), &bare_res);
        EXPECT_EQ(bare_res.status, QpStatus::kOptimal);
        EXPECT_EQ(bare_res.iters, 1);
        EXPECT_EQ(bare_res.factorizations, 1);
    }

    // The independent oracle.
    const QpSolution walk = walk_solution(qp);
    EXPECT_NEAR(res.x(0), walk.x(0), 1e-6);
    EXPECT_NEAR(res.x(1), walk.x(1), 1e-6);
}

// The convergence test runs BEFORE each step (ssn_engine.h), so a start point
// that is already a KKT point costs zero STEPS. This is the companion half of
// the fixture above: it pins WHERE the test sits, which is what makes
// "1 iteration" above mean "one step was needed".
//
// **AND IT PINS THE ONE FACTORIZATION THE CERTIFICATE COSTS** (review fix
// round 1, C1). The safeguarded engine may not certify a point nothing has
// looked at, so a zero-STEP exit is not a zero-FACTORIZATION exit any more:
// it pays the second-order verification, and the symbolic analysis that the
// first factorization of a new structure always carries. kBare, which makes no
// second-order claim, still costs nothing at all -- and the gap between the two
// arms below IS the price of the fix, stated as a number rather than as prose.
TEST(SsnEngineLocal, ExactPrimalDualSeedCostsNoStep) {
    const QpProblem qp = two_row_qp();

    SsnStart start;
    start.x = Vec(2);
    start.x << 1.0, 1.0;
    start.lambda_i = Vec(2);
    start.lambda_i << 1.0, 0.0;

    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, start, SsnOptions{}, &res);

        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.escape_reason, SsnEscape::kNone);
        EXPECT_EQ(res.iters, 0);
        EXPECT_EQ(res.factorizations, 1);
        EXPECT_EQ(res.symbolic_analyses, 1);
        // The verification takes no step, so it moves no other counter.
        EXPECT_EQ(res.counters.ssn_backtracks, 0);
        EXPECT_EQ(res.counters.ssn_bulk_flips, 0);
        EXPECT_EQ(res.counters.ssn_prox_updates, 0);
    }
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, start, bare_opts(), &res);

        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 0);
        EXPECT_EQ(res.factorizations, 0);
        EXPECT_EQ(res.symbolic_analyses, 0);
    }
}

// =============================================================================
// C1 (review fix round 1): NO kOptimal WITHOUT INERTIA EVIDENCE AT THE POINT
// =============================================================================
//
// **THE DEFECT THIS PINS WAS A WRONG ANSWER, AND IT WAS REACHED BY THE EXACT
// PATH TASK 5 BUILDS.** indefinite_qp's saddle (0.5, 0.125) is a first-order
// KKT point of the same system its true solution (0.5, -1) satisfies -- F
// vanishes at both -- so a convergence test that runs before any factorization
// certifies it in ZERO factorizations when the caller SEEDS there, which is
// what a warm hand-off (re-solve from the previous answer) does. The
// safeguarded engine now opens a verification attempt instead: one
// factorization, at the caller's own regularization, at THIS point.
//
// The three arms are the whole contract:
//   kBare      -- still returns the saddle as kOptimal. It is the POSITIVE
//                 CONTROL and its banner has always said so; if this arm ever
//                 goes green the control has stopped controlling.
//   kFull      -- kIndefinite, 0 steps, exactly 1 factorization, and it never
//                 climbs the ladder (escalating cannot move a point whose F is
//                 already zero).
//   kFull at the TRUE solution -- kOptimal, because the active bound's own
//                 D^{-1} penalty makes the primal block positive definite
//                 there. The gate refuses saddles, not nonconvex QPs.
TEST(SsnEngineLocal, SaddleSeedIsNotCertifiedWithoutInertiaEvidence) {
    const QpProblem qp = indefinite_qp();

    SsnStart saddle;
    saddle.x = Vec(2);
    saddle.x << 0.5, 0.125;

    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, saddle, bare_opts(), &res);
        // ===== THE POSITIVE CONTROL: bare mode returns the WRONG ANSWER =====
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.factorizations, 0);
        EXPECT_NEAR(res.x(1), 0.125, 1e-12);
    }
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, saddle, SsnOptions{}, &res);
        EXPECT_NE(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.escape_reason, SsnEscape::kIndefinite);
        EXPECT_EQ(res.iters, 0);
        EXPECT_EQ(res.factorizations, 1);
        EXPECT_EQ(res.counters.ssn_prox_updates, 0);
        EXPECT_DOUBLE_EQ(res.prox_sigma, 0.0);
        EXPECT_NE(res.escape_detail.find("first-order KKT"), std::string::npos);
        // The escape is reported at EVERY budget, including the one that
        // forbids steps outright -- the verification is not a step.
        for (const Index budget : {Index{0}, Index{1}, Index{25}}) {
            SsnEngine e2(default_opts());
            SsnOptions s;
            s.hard_budget = budget;
            SsnResult r2;
            e2.solve(qp, saddle, s, &r2);
            EXPECT_EQ(r2.escape_reason, SsnEscape::kIndefinite) << "budget " << budget;
            EXPECT_EQ(r2.factorizations, 1) << "budget " << budget;
        }
    }
    {
        // **THE VERDICT IS READ AT THE CALLER'S OWN REGULARIZATION, NOT AT THE
        // LADDER'S** (ssn_engine.h section 7b), and this arm is what makes that
        // a tested property rather than a documented intention. With the
        // proximal term started at sigma = 100 the same K is
        // H + (delta + 100) I -- and H + 100 I is positive definite for this
        // fixture, so a verification that read ITS inertia would answer kOk and
        // certify the saddle. The engine drops sigma to 0 for the verification,
        // gets kWrong, escapes -- and restores the ladder's own sigma for
        // reporting, which the last assertion pins.
        SsnEngine engine(default_opts());
        SsnOptions s;
        s.prox_sigma_init = 100.0;
        SsnResult res;
        engine.solve(qp, saddle, s, &res);
        EXPECT_NE(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.escape_reason, SsnEscape::kIndefinite);
        EXPECT_EQ(res.factorizations, 1);
        EXPECT_DOUBLE_EQ(res.prox_sigma, 100.0) << "the ladder's sigma is restored for reporting";
    }
    {
        // Seeded at the TRUE solution, the same gate certifies.
        SsnEngine engine(default_opts());
        SsnStart opt;
        opt.x = Vec(2);
        opt.x << 0.5, -1.0;
        opt.z = Vec(2);
        opt.z << 0.0, 2.25; // the lower bound's multiplier: -(-2*-1) - 0.25
        SsnResult res;
        engine.solve(qp, opt, SsnOptions{}, &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.escape_reason, SsnEscape::kNone);
        EXPECT_EQ(res.iters, 0);
        EXPECT_EQ(res.factorizations, 1);
    }
}

// A hinted-ACTIVE row must be driven ONTO its constraint, which is only visible
// from a seed that is not already on it: the row's branch residual is the SLACK
// s_k, not phi(s_k, lambda_k) -- and at the exact-primal seed of the fixture
// above those two are both zero, so that fixture cannot tell them apart. From
// the interior seed x = (0, 0) the active row has s_0 = 2 and lambda_0 = 0,
// where phi(2, 0) = 0 exactly: a kernel that fed phi into the hinted row would
// ask the step for NO primal motion at all and could not finish in one
// iteration. (Named mutant M6 in the Task-3 report; this test is what kills
// it.)
TEST(SsnEngineLocal, CorrectHintFromInteriorSeedSnapsOntoTheConstraint) {
    const QpProblem qp = two_row_qp();
    SsnEngine engine(default_opts());

    SsnStart start;
    start.x = Vec::Zero(2); // strictly interior: both slacks positive
    start.activity_hint.ineq = {true, false};

    SsnOptions sopts;
    SsnResult res;
    engine.solve(qp, start, sopts, &res);

    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.iters, 1);
    EXPECT_LT(res.fb_residual, sopts.fb_tol);
    // The regularizers leave an O(delta) = O(1e-8) offset; the point is that
    // one step crosses the whole distance from the interior to the face.
    EXPECT_NEAR(res.x(0), 1.0, 1e-6);
    EXPECT_NEAR(res.x(1), 1.0, 1e-6);
    EXPECT_NEAR(res.lambda_i(0), 1.0, 1e-6);
    EXPECT_NEAR(res.lambda_i(1), 0.0, 1e-6);
}

// =============================================================================
// (b) WRONG hint: still converges, and the flip is visible
// =============================================================================
//
// The wrong hint {row 0 inactive, row 1 active} makes step 0 a PDAS solve on
// {row 1}: x - 2*1 + lambda1*(1,-1) = 0 with x0 - x1 = 5 gives lambda1 = -2.5
// and x = (4.5, -0.5) -- a point at which row 0's slack is -2 (violated) and
// row 1's multiplier is negative. Both rows therefore contradict the hint on
// the next branch selection, which is the bulk flip.
//
// ============ OBSERVED VALUE, AND IT IS 6, NOT THE BRIEF'S 4. ============
// The Task-3 brief pre-registered "converges <= 4 iterations" as a guess; the
// measured figure on this fixture -- and on every 2-row variant tried (a
// near-parallel second row, a second row whose boundary passes within 0.5 of
// x*, the "no rows active" hint, and no hint at all) -- is 6, and the cap here
// is that measurement.
//
// THE MECHANISM IS STRUCTURAL AND PROVABLE (this wording is the review's, fix
// round 1 M3; the earlier "the step HALVES the multiplier" was true only of
// the first step -- the next moves by a factor 3.93 -- and the identity below
// is both stronger and fixture-independent).
//
// **STATED AT dual_mu = 0, WHERE IT IS EXACT** (round 2 correction; the
// unqualified version below used to read as though it held at the shipped
// default, and it does not). A hinted-ACTIVE row lands with slack
// s+ = -mu * d lambda -- exactly zero only when mu = 0, because beta = 0 makes
// that row's diagonal -mu rather than 0 (ssn_engine.h section 4). Take mu = 0
// for the derivation, so a wrong hint leaves the pair at exactly
// (s, lambda) = (0, negative). There alpha = 1 and beta = 2 IDENTICALLY in
// lambda -- rho = |lambda| = -lambda, so alpha = 1 - 0/rho and
// beta = 1 - lambda/(-lambda) -- and phi(0, lambda) = 2*lambda is linear. The
// scaled FB row a^T dx - (beta/alpha) d lambda = phi/alpha is therefore
// a^T dx - 2 d lambda = 2 lambda, and since s+ = b - a^T(x+dx) = -a^T dx this
// is identically
//
//     s+ + 2*lambda+ = 0        (exact at mu = 0)
//
// for the step taken OUT OF that configuration. **The step is confined to a
// line through the origin, and the root of a non-degenerate row
// (lambda* = 0, s* > 0) does not lie on it** -- lambda+ = 0 would force
// s+ = 0. So ONE Newton step cannot repair a wrongly-hinted-active row, for
// ANY hint magnitude and ANY problem scale: the extra step is structural, not
// a property of this fixture or of how wrong the hint was.
//
// UNDER THE SHIPPED dual_mu = 1e-8 the identity holds to O(mu * |d lambda|)
// rather than exactly, and the conclusion is unaffected -- an O(1e-8) miss is
// not a root. Measured on this fixture, for the step out of the s = 0
// configuration: |s+ + 2*lambda+| is 4.4e-16 (machine zero) at mu = 0,
// 1.250000e-08 at mu = 1e-8 and 1.250002e-06 at mu = 1e-6, against
// mu*|d lambda| of 0, 1.25e-08 and 1.25e-06 -- i.e. it tracks mu*|d lambda| to
// every digit printed, over four orders of magnitude in mu.
//
// **THE SIGNED mu-FORM, supplied by the Fable kernel review (its O2)**, for
// anyone who needs more than the magnitude. Carrying the hinted step's landing
// slack s0 = -mu*d lambda_1 > 0 into the next step and expanding
// alpha = 1 - s0/|lambda|, beta = 2, phi ~ s0 + 2*lambda through the scaled row
// gives, to first order,
//
//     s+ + 2*lambda+ = 2*s0 - (2*s0/|lambda| + mu) * d lambda.
//
// On this fixture (s0 = 2.5e-8, lambda = -2.5, d lambda = +1.25) that is
// 5e-8 - 3.75e-8 = +1.25e-8, matching the measurement including its SIGN. Note
// the naive signed extension of the mu = 0 identity, -mu*d lambda = -1.25e-8,
// is sign-WRONG: the carried s0 and the alpha-perturbation enter at the same
// O(mu*d lambda) order and dominate. This file published only the magnitude
// until the review derived the alpha term, which is why the unsigned statement
// above is the one every consumer uses.
//
// Measured trajectory (||F||inf, lambda_1, s_1): 5.0 (-2.5, 2.5e-8),
// 1.545 (-1.25, 2.5), 0.330 (-0.318, 4.36), 1.23e-2, 1.52e-5, 2.32e-11 -- note
// s_1 = 2.5 = -2*lambda_1 after step 2 to 8 digits, and that step 1's landing
// slack is 2.5e-8 = -mu*d lambda, not 0. Two steps to leave the s = 0
// configuration, then textbook quadratic convergence. A cap of 4 is not
// reachable by choosing a gentler fixture; it would need Task 4's safeguards
// or a min-map (rather than FB) branch, and the brief mandates FB.
//
// The cost is also COMMON-MODE rather than per-row (measured by the reviewer
// on the box family: 1 wrongly hinted row costs 7 iterations, 100 wrongly
// hinted rows cost 8) -- every such row sits at (0, lambda<0) and traverses
// the same line in parallel.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, WrongHintRecoversWithABulkFlip) {
    const QpProblem qp = two_row_qp();
    SsnEngine engine(default_opts());

    SsnStart start;
    start.x = Vec(2);
    start.x << 1.0, 1.0;
    start.activity_hint.ineq = {false, true}; // exactly wrong

    SsnOptions sopts;
    SsnResult res;
    engine.solve(qp, start, sopts, &res);

    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.escape_reason, SsnEscape::kNone);
    EXPECT_LE(res.iters, 6); // observed: exactly 6 -- see the banner above
    EXPECT_GE(res.counters.ssn_bulk_flips, 1);
    EXPECT_LT(res.fb_residual, sopts.fb_tol);
    EXPECT_NEAR(res.x(0), 1.0, 1e-6);
    EXPECT_NEAR(res.x(1), 1.0, 1e-6);
    EXPECT_NEAR(res.lambda_i(0), 1.0, 1e-6);
    EXPECT_NEAR(res.lambda_i(1), 0.0, 1e-6);
    EXPECT_EQ(res.factorizations, certified_factorizations(res.iters));
}

// =============================================================================
// (c) Bounds-only box QP, n = 50, deliberately wrong ("random-sign") hint
// =============================================================================
//
// ============ OBSERVED VALUE, AND IT IS 8, NOT THE BRIEF'S 6. ============
// The brief registered "<= 6 iterations (values pinned after measurement with
// justification)"; the measurement AT n = 50 is 8 and the cap here is that
// measurement. The two extra steps a wrong hint costs are the same fixed
// "leave the s = 0 configuration" cost the two-row fixture's banner derives
// (s+ + 2*lambda+ = 0 at mu = 0), and the reviewer measured them to be
// COMMON-MODE: one
// wrongly hinted row costs the same as a hundred.
//
// **PER-HINT VALUES AT THIS SIZE, EXACT** (fix round 1, M1 -- an earlier
// version of this banner quoted a range across a grid it had not measured on
// the shipped fixture): at n = 50, scrambled = 8, all-at-lower = 8,
// no hint = 6, against the WALK's 30 minor iterations. The n-scaling of these
// numbers lives in BoxQpCountGrowsSlowlyInN below, which is where the honest
// growth statement is; it does NOT belong here, and this banner used to make a
// flatness claim that its own family contradicts.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, BoxQpFromScrambledHint) {
    const Index n = 50;
    const QpProblem qp = box_qp(n);
    SsnEngine engine(default_opts());

    SsnStart start;
    start.activity_hint.bounds.assign(static_cast<std::size_t>(n), BoundState::kFree);
    for (Index j = 0; j < n; ++j) {
        // Deterministic scramble: a third of the variables pinned to each
        // bound, a third free, with no relation to the true solution.
        const int r = static_cast<int>(j % 3);
        start.activity_hint.bounds[static_cast<std::size_t>(j)] =
            r == 0 ? BoundState::kAtLower : (r == 1 ? BoundState::kAtUpper : BoundState::kFree);
    }

    SsnOptions sopts;
    SsnResult res;
    engine.solve(qp, start, sopts, &res);

    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.escape_reason, SsnEscape::kNone);
    EXPECT_LE(res.iters, 8); // observed: exactly 8 -- see the banner above
    EXPECT_GE(res.counters.ssn_bulk_flips, 1);
    EXPECT_LT(res.fb_residual, sopts.fb_tol);
    EXPECT_EQ(res.factorizations, certified_factorizations(res.iters));

    // Against the walk, coordinate by coordinate, plus the bound-multiplier
    // sign convention (z >= 0 at an active lower bound, <= 0 at an upper one).
    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < n; ++j) {
        EXPECT_NEAR(res.x(j), walk.x(j), 1e-6) << "coordinate " << j;
        EXPECT_GE(res.x(j), qp.lower(j) - 1e-6) << "coordinate " << j;
        EXPECT_LE(res.x(j), qp.upper(j) + 1e-6) << "coordinate " << j;
        if (res.x(j) < qp.lower(j) + 1e-6) {
            EXPECT_GE(res.z(j), -1e-6) << "coordinate " << j;
        } else if (res.x(j) > qp.upper(j) - 1e-6) {
            EXPECT_LE(res.z(j), 1e-6) << "coordinate " << j;
        } else {
            EXPECT_NEAR(res.z(j), 0.0, 1e-6) << "coordinate " << j;
        }
    }
}

// =============================================================================
// (d) Equality-only QP: EXACTLY one iteration (it is one KKT solve)
// =============================================================================
TEST(SsnEngineLocal, EqualityOnlyIsOneKktSolve) {
    const QpProblem qp = equality_only_qp();
    SsnEngine engine(default_opts());

    SsnOptions sopts;
    SsnResult res;
    engine.solve(qp, SsnStart{}, sopts, &res);

    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.iters, 1);
    // One KKT solve, plus the second-order verification (fix round 1, C1). F
    // is AFFINE on this fixture, so "one step" is still the whole claim.
    EXPECT_EQ(res.factorizations, certified_factorizations(res.iters));
    EXPECT_EQ(res.counters.ssn_bulk_flips, 0);
    EXPECT_LT(res.fb_residual, sopts.fb_tol);

    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < qp.n(); ++j) {
        EXPECT_NEAR(res.x(j), walk.x(j), 1e-6) << "coordinate " << j;
    }
}

// =============================================================================
// THE MIXED-BLOCK CONFIGURATION (review fix round 1, I1)
// =============================================================================
//
// The one shape every Task-5 subproblem has, and the one shape the shipped
// suite did not cover. Asserted against the walk across ALL FOUR blocks --
// x, lambda_e, lambda_i, z -- because the offset bug this closes
// (`outer[n + me + k]` -> `outer[n + k]`) is invisible in any single block on
// any single-block fixture.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, MixedEqualityInequalityAndBoundsMatchesTheWalk) {
    const QpProblem qp = mixed_block_qp();
    ASSERT_EQ(qp.me(), 1);
    ASSERT_EQ(qp.mi(), 3);

    SsnEngine engine(default_opts());
    SsnOptions sopts;
    SsnResult res;
    engine.solve(qp, SsnStart{}, sopts, &res);

    ASSERT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.escape_reason, SsnEscape::kNone);
    EXPECT_LT(res.fb_residual, sopts.fb_tol);
    EXPECT_EQ(res.factorizations, certified_factorizations(res.iters));

    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < qp.n(); ++j) {
        EXPECT_NEAR(res.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
        EXPECT_NEAR(res.z(j), walk.z(j), 1e-6) << "z(" << j << ")";
    }
    for (Index r = 0; r < qp.me(); ++r) {
        EXPECT_NEAR(res.lambda_e(r), walk.lambda_e(r), 1e-6) << "lambda_e(" << r << ")";
    }
    for (Index k = 0; k < qp.mi(); ++k) {
        EXPECT_NEAR(res.lambda_i(k), walk.lambda_i(k), 1e-6) << "lambda_i(" << k << ")";
        EXPECT_GE(res.lambda_i(k), -1e-6) << "lambda_i(" << k << ")";
    }

    // The fixture is only load-bearing if its active set is genuinely mixed:
    // at least one inequality row active AND at least one bound active AND at
    // least one of each slack. Asserted so a later edit to the data cannot
    // quietly turn this back into a single-block fixture.
    int ineq_on = 0, bound_on = 0;
    for (Index k = 0; k < qp.mi(); ++k) {
        ineq_on += res.ineq_active[static_cast<std::size_t>(k)] ? 1 : 0;
    }
    for (Index j = 0; j < qp.n(); ++j) {
        bound_on += res.bound_state[static_cast<std::size_t>(j)] != BoundState::kFree ? 1 : 0;
    }
    EXPECT_GT(ineq_on, 0);
    EXPECT_LT(ineq_on, qp.mi());
    EXPECT_GT(bound_on, 0);
    EXPECT_LT(bound_on, qp.n());
}

// =============================================================================
// THE ACTIVITY EXPORT (review fix round 1, M4)
// =============================================================================
//
// SsnResult::ineq_active / bound_state are the engine's own partition applied
// to the returned iterate. They are write-only here, but Task 5 hands them to
// the warm-start export and the stable-face refinement, so they have to agree
// with what the walk independently identifies on the same QP.
TEST(SsnEngineLocal, ActivityExportAgreesWithTheWalk) {
    const QpProblem cases[] = {mixed_block_qp(), box_qp(20), two_row_qp()};
    for (const QpProblem &qp : cases) {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, SsnStart{}, SsnOptions{}, &res);
        ASSERT_EQ(res.status, QpStatus::kOptimal);

        ASSERT_EQ(static_cast<Index>(res.ineq_active.size()), qp.mi());
        ASSERT_EQ(static_cast<Index>(res.bound_state.size()), qp.n());

        const QpSolution walk = walk_solution(qp);
        ASSERT_EQ(walk.status, QpStatus::kOptimal);
        for (Index k = 0; k < qp.mi(); ++k) {
            EXPECT_EQ(res.ineq_active[static_cast<std::size_t>(k)],
                      walk.ineq_active[static_cast<std::size_t>(k)])
                << "inequality row " << k;
        }
        for (Index j = 0; j < qp.n(); ++j) {
            EXPECT_EQ(res.bound_state[static_cast<std::size_t>(j)],
                      walk.bound_state[static_cast<std::size_t>(j)])
                << "variable " << j;
        }
    }
}

// The export is populated on EVERY exit route, including one that took no step
// and one that escaped -- it reads the returned iterate, not the loop's
// history, so there is no path on which it can be missing.
TEST(SsnEngineLocal, ActivityExportIsPopulatedOnEveryExit) {
    const QpProblem qp = mixed_block_qp();
    SsnEngine engine(default_opts());

    SsnOptions budget_zero;
    budget_zero.hard_budget = 0;
    SsnResult none;
    engine.solve(qp, SsnStart{}, budget_zero, &none);
    ASSERT_EQ(none.escape_reason, SsnEscape::kBudget);
    EXPECT_EQ(none.iters, 0);
    EXPECT_EQ(static_cast<Index>(none.ineq_active.size()), qp.mi());
    EXPECT_EQ(static_cast<Index>(none.bound_state.size()), qp.n());

    // A converged solve fed straight back in costs zero steps, and its export
    // must reproduce itself -- the round trip Task 5's hand-off depends on.
    SsnResult first;
    engine.solve(qp, SsnStart{}, SsnOptions{}, &first);
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    SsnStart again;
    again.x = first.x;
    again.lambda_e = first.lambda_e;
    again.lambda_i = first.lambda_i;
    again.z = first.z;
    SsnResult second;
    engine.solve(qp, again, SsnOptions{}, &second);
    EXPECT_EQ(second.status, QpStatus::kOptimal);
    EXPECT_EQ(second.iters, 0);
    EXPECT_EQ(second.ineq_active, first.ineq_active);
    EXPECT_EQ(second.bound_state, first.bound_state);
}

// =============================================================================
// SEEDED z AGAINST AN ABSENT BOUND (review fix round 1, M6)
// =============================================================================
TEST(SsnEngineLocal, SeededZOnAnAbsentBoundThrows) {
    QpProblem qp = two_row_qp(); // both bounds at the +/-1e20 sentinel
    SsnEngine engine(default_opts());
    SsnResult res;

    SsnStart lower_side;
    lower_side.z = Vec::Zero(2);
    lower_side.z(0) = 1.0; // prices an absent LOWER bound
    EXPECT_THROW(engine.solve(qp, lower_side, SsnOptions{}, &res), std::invalid_argument);

    SsnStart upper_side;
    upper_side.z = Vec::Zero(2);
    upper_side.z(1) = -1.0; // prices an absent UPPER bound
    EXPECT_THROW(engine.solve(qp, upper_side, SsnOptions{}, &res), std::invalid_argument);

    // Exactly zero is fine -- that IS the correct value for an absent side.
    SsnStart zeroed;
    zeroed.z = Vec::Zero(2);
    EXPECT_NO_THROW(engine.solve(qp, zeroed, SsnOptions{}, &res));

    // And on a variable that HAS the bound, the same mass is accepted.
    qp.lower = Vec::Constant(2, -5.0);
    SsnStart priced;
    priced.z = Vec::Zero(2);
    priced.z(0) = 1.0;
    EXPECT_NO_THROW(engine.solve(qp, priced, SsnOptions{}, &res));
}

// =============================================================================
// THE PER-SOLVE TRUST-REGION SEAM (Fable kernel review, I1)
// =============================================================================
//
// The funnel driver solves EVERY subproblem under a per-solve TR override
// (sqp_driver.h:5021, and SOC re-solves at :5397) and reads QpSolution's
// tr_active back to adapt the radius (:5259). The kernel had no such input, and
// the caller-side workaround -- folding [x0-D, x0+D] into lower/upper -- cannot
// express the TR-pin/real-bound distinction, so the driver's grow/shrink logic
// and the warm-start export would both have consumed polluted activity. That is
// exactly what qp_problem.h's tr_active contract exists to prevent.
//
// This fixture is the discriminating one: variable 0's REAL upper bound (0.5)
// is tighter than the radius, variable 1's is not (100 vs. a radius of 1). So
// at the solution
//
//   x0 is held by a GENUINE bound  -> bound_state kAtUpper, z(0) != 0, tr_active false
//   x1 is held by the TRUST REGION -> bound_state kFree,    z(1) == 0, tr_active true
//
// A kernel that did not separate the two would report x1 as kAtUpper with a
// nonzero z -- a radius masquerading as a constraint.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
namespace {

// min 1/2||x||^2 - 10(x0 + x1)  s.t.  -100 <= x <= (0.5, 100)
// Unconstrained minimizer (10, 10) is far outside, so whatever bounds the
// solve, binds.
QpProblem tr_probe_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -10.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -100.0);
    qp.upper = Vec(2);
    qp.upper << 0.5, 100.0;
    return qp;
}

} // namespace

TEST(SsnEngineLocal, TrustRegionPinsAreSeparatedFromRealBounds) {
    const QpProblem qp = tr_probe_qp();
    SolveOverrides ov;
    ov.tr_radius = 1.0; // about the start point x0 = 0

    SsnEngine engine(default_opts());
    SsnStart start;
    start.x = Vec::Zero(2);
    SsnOptions sopts;
    SsnResult res;
    engine.solve(qp, start, sopts, ov, &res);

    ASSERT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_LT(res.fb_residual, sopts.fb_tol);
    // Variable 0 stops at its real bound 0.5; variable 1 at the TR face 0 + 1.
    EXPECT_NEAR(res.x(0), 0.5, 1e-6);
    EXPECT_NEAR(res.x(1), 1.0, 1e-6);

    ASSERT_EQ(static_cast<Index>(res.tr_active.size()), 2);
    EXPECT_FALSE(res.tr_active[0]) << "a real bound is not a trust-region pin";
    EXPECT_TRUE(res.tr_active[1]) << "the radius binds here";

    // bound_state is a REAL-BOUND-ONLY view...
    EXPECT_EQ(res.bound_state[0], BoundState::kAtUpper);
    EXPECT_EQ(res.bound_state[1], BoundState::kFree);
    // ... and z carries no trust-region dual at all.
    EXPECT_LT(res.z(0), -1e-6) << "at an active UPPER bound z is negative";
    EXPECT_DOUBLE_EQ(res.z(1), 0.0) << "TR duals are internal and never exposed";

    // The walk, solving the SAME QP under the SAME override, agrees on every
    // one of those four exports -- which is what makes this the walk's contract
    // rather than a private convention.
    QpEngine walk_engine(default_opts());
    const QpSolution walk = walk_engine.solve(qp, ov);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < 2; ++j) {
        EXPECT_NEAR(res.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
        EXPECT_NEAR(res.z(j), walk.z(j), 1e-6) << "z(" << j << ")";
        EXPECT_EQ(res.bound_state[static_cast<std::size_t>(j)],
                  walk.bound_state[static_cast<std::size_t>(j)])
            << "bound_state(" << j << ")";
        EXPECT_EQ(res.tr_active[static_cast<std::size_t>(j)],
                  walk.tr_active[static_cast<std::size_t>(j)])
            << "tr_active(" << j << ")";
    }
}

// =============================================================================
// THE TRUST-REGION EXIT CONTRACT (review fix round 1, I3; also the coverage
// gap the opus review's M11 named -- no fixture ran the SAFEGUARDS under a
// BINDING radius, which is the configuration Task 5 uses on every major)
// =============================================================================
//
// **THE TRUST REGION IS SOFT IN THIS KERNEL AND THE EXPORT MUST SAY SO.** It
// enters as FB bound ROWS, an FB row holds only at a root, and the line search
// damps the step rather than projecting it -- so an ESCAPED exit can return a
// point far outside the box. Task 5's funnel forms its step as x - x0 and its
// ratio test presumes ||d||inf <= Delta; without a stated contract it would
// presume it of an escaped point too.
//
// ============ MKL-OBSERVED. NOT RE-VERIFIED ON APPLE ACCELERATE. ============
// The violation factors below are trajectory properties and are asserted as
// inequalities for that reason; the CONTRACT (bounded on a certifying exit,
// reported and unbounded on an escape) is exact and is asserted exactly.
TEST(SsnEngineLocal, TrustRegionIsSoftAndTheExitContractSaysSo) {
    const QpProblem qp = cycling_qp_3var();
    const double radius = 0.01;

    SolveOverrides ov;
    ov.tr_radius = radius;

    // --- an ESCAPED exit may be far outside the radius, and reports it ----
    double worst_factor = 0.0;
    for (const Index budget : {Index{1}, Index{2}, Index{3}, Index{4}, Index{5}}) {
        SsnEngine engine(default_opts());
        SsnOptions s;
        s.hard_budget = budget;
        SsnResult res;
        engine.solve(qp, SsnStart{}, s, ov, &res);
        ASSERT_NE(res.escape_reason, SsnEscape::kNone) << "budget " << budget;
        // x is nowhere near the box -- x0 is the origin, so the effective box
        // is [-0.01, 0.01]^3 and the returned point is over a hundred times
        // that. This is the measured number the finding was raised on.
        const double inf_x = res.x.cwiseAbs().maxCoeff();
        EXPECT_GT(inf_x, 10.0 * radius) << "budget " << budget;
        // tr_violation reports it, exactly, against the effective box.
        EXPECT_NEAR(res.tr_violation, inf_x - radius, 1e-9) << "budget " << budget;
        worst_factor = std::max(worst_factor, inf_x / radius);
    }
    EXPECT_GT(worst_factor, 100.0) << "the finding measured 133x .. 160x";

    // **AND A FULL BUDGET DOES NOT RESCUE IT ON THIS FIXTURE**, which is the
    // finding the M11 coverage gap was hiding: run to the default budget under
    // the same tight radius and the safeguarded engine ESCAPES rather than
    // converging (kNoContraction at 15 accepted steps), returning a point 73x
    // the radius out. A binding trust region is not a benign perturbation of a
    // solve, and Task 5 must budget for a subproblem that escapes under one.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, SsnStart{}, SsnOptions{}, ov, &res);
        EXPECT_NE(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.escape_reason, SsnEscape::kNoContraction);
        EXPECT_GT(res.tr_violation, 10.0 * radius);
    }

    // --- a CERTIFYING exit respects the box to the tolerance and no more --
    //
    // mixed_block_qp is the shape every Task-5 subproblem has (equalities AND
    // inequality rows AND two-sided bounds), and at radius 0.2 the radius binds
    // on FIVE of its eight variables all the way to convergence -- so the
    // safeguards, the classification and the certificate all run under a
    // binding trust region, which no fixture covered before.
    {
        const QpProblem mixed = mixed_block_qp();
        SolveOverrides tight;
        tight.tr_radius = 0.2;
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(mixed, SsnStart{}, SsnOptions{}, tight, &res);
        ASSERT_EQ(res.status, QpStatus::kOptimal);
        ASSERT_EQ(res.escape_reason, SsnEscape::kNone);
        EXPECT_EQ(res.factorizations, certified_factorizations(res.iters));
        // The whole bound of the contract: 1/(2 - sqrt(2)) * fb_tol.
        EXPECT_LE(res.tr_violation, detail::kSsnComplementarityFactor * SsnOptions{}.fb_tol);
        for (Index j = 0; j < mixed.n(); ++j) {
            EXPECT_LE(std::abs(res.x(j)), 0.2 + 1e-6) << "x(" << j << ")";
        }
        // The radius genuinely bound -- otherwise the assertions above are
        // vacuous -- and the TR/real separation survived it.
        Index n_tr = 0;
        for (const bool t : res.tr_active) {
            n_tr += t ? 1 : 0;
        }
        EXPECT_EQ(n_tr, 5);
        for (Index j = 0; j < mixed.n(); ++j) {
            if (res.tr_active[static_cast<std::size_t>(j)]) {
                EXPECT_EQ(res.bound_state[static_cast<std::size_t>(j)], BoundState::kFree)
                    << "a TR pin must not read as a real bound, j = " << j;
                EXPECT_DOUBLE_EQ(res.z(j), 0.0) << "TR duals stay internal, j = " << j;
            }
        }
    }

    // --- no radius: the field is exactly zero, on every exit --------------
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, SsnStart{}, SsnOptions{}, &res);
        EXPECT_DOUBLE_EQ(res.tr_violation, 0.0);
        SsnEngine e2(default_opts());
        SsnOptions s;
        s.hard_budget = 2;
        SsnResult r2;
        e2.solve(qp, SsnStart{}, s, &r2);
        ASSERT_EQ(r2.escape_reason, SsnEscape::kBudget);
        EXPECT_DOUBLE_EQ(r2.tr_violation, 0.0);
    }
}

// A radius too loose to bind must change NOTHING -- not the answer, not a bit
// of it. max(lower, x0 - D) returns `lower` EXACTLY when the radius is loose,
// so every downstream value is bit-identical to a solve with no override.
TEST(SsnEngineLocal, LooseTrustRegionIsBitIdenticalToNoOverride) {
    const QpProblem qp = box_qp(20); // every bound finite, |x| <= 1

    SsnEngine plain_engine(default_opts());
    SsnResult plain;
    plain_engine.solve(qp, SsnStart{}, SsnOptions{}, &plain);
    ASSERT_EQ(plain.status, QpStatus::kOptimal);

    SolveOverrides loose;
    loose.tr_radius = 1e6;
    SsnEngine tr_engine(default_opts());
    SsnResult with_tr;
    tr_engine.solve(qp, SsnStart{}, SsnOptions{}, loose, &with_tr);

    EXPECT_EQ(with_tr.status, plain.status);
    EXPECT_EQ(with_tr.iters, plain.iters);
    EXPECT_EQ(with_tr.factorizations, plain.factorizations);
    EXPECT_EQ(with_tr.fb_residual, plain.fb_residual); // exact, not NEAR
    for (Index j = 0; j < qp.n(); ++j) {
        EXPECT_EQ(with_tr.x(j), plain.x(j)) << "x(" << j << ")";
        EXPECT_EQ(with_tr.z(j), plain.z(j)) << "z(" << j << ")";
        EXPECT_FALSE(with_tr.tr_active[static_cast<std::size_t>(j)]);
    }
    EXPECT_EQ(with_tr.bound_state, plain.bound_state);
    EXPECT_EQ(with_tr.ineq_active, plain.ineq_active);
}

// The radius is a VALUE, not a structure. Under any finite radius every
// variable has both bound rows, so K's pattern is invariant across radius
// changes -- which is what makes the driver's shrink-retry loop free rather
// than a rebuild per attempt. Dropping to no-radius IS a structure change (rows
// disappear), and that one must still rebuild.
TEST(SsnEngineLocal, TrustRegionRadiusIsNotPartOfTheStructureKey) {
    QpProblem qp = tr_probe_qp();
    qp.lower(1) = -1e20; // one genuinely unbounded side, so the row counts differ
    SsnEngine engine(default_opts());

    SsnStart start;
    start.x = Vec::Zero(2);

    SolveOverrides ov;
    ov.tr_radius = 1.0;
    SsnResult first;
    engine.solve(qp, start, SsnOptions{}, ov, &first);
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    EXPECT_EQ(first.pattern_rebuilds, 1);

    // A shrink, then a growth: neither re-derives the pattern.
    for (double radius : {0.25, 0.5, 4.0, 1e3}) {
        SolveOverrides next;
        next.tr_radius = radius;
        SsnResult res;
        engine.solve(qp, start, SsnOptions{}, next, &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal) << "radius " << radius;
        EXPECT_EQ(res.pattern_rebuilds, 0) << "radius " << radius << " must reuse the pattern";
        EXPECT_EQ(res.symbolic_analyses, 0) << "radius " << radius;
    }

    // Removing the radius removes two rows -- a real structure change.
    SsnResult no_tr;
    engine.solve(qp, start, SsnOptions{}, &no_tr);
    EXPECT_EQ(no_tr.pattern_rebuilds, 1);
}

// The other two SolveOverrides fields resolve by qp_types.h's rules too. dual_mu
// is the observable one: on a rank-deficient active set the regularizer picks
// the multiplier split, so overriding it to 0 must visibly abandon the
// minimum-norm answer the engine default produces.
TEST(SsnEngineLocal, RegularizerOverridesResolveLikeTheWalks) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -2.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 1, 1, 1; // identical rows: the dual is non-unique
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(2, 2.0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);

    SsnStart start;
    start.activity_hint.ineq = {true, true};

    SsnEngine engine(default_opts());
    SsnResult defaulted;
    engine.solve(qp, start, SsnOptions{}, &defaulted);
    ASSERT_EQ(defaulted.status, QpStatus::kOptimal);
    EXPECT_NEAR(defaulted.lambda_i(0), 0.5, 1e-6);
    EXPECT_NEAR(defaulted.lambda_i(1), 0.5, 1e-6);

    SolveOverrides no_mu;
    no_mu.dual_mu = 0.0; // an explicit zero, not the negative sentinel
    SsnResult unregularized;
    engine.solve(qp, start, SsnOptions{}, no_mu, &unregularized);
    ASSERT_EQ(unregularized.status, QpStatus::kOptimal);
    // Still a valid dual (the pair must sum to 1) but no longer min-norm.
    EXPECT_NEAR(unregularized.lambda_i(0) + unregularized.lambda_i(1), 1.0, 1e-6);
    EXPECT_GT(std::abs(unregularized.lambda_i(0) - unregularized.lambda_i(1)), 0.5)
        << "without dual_mu the split is whatever the factorization produces";
    // The primal is unaffected either way.
    EXPECT_NEAR(unregularized.x(0), 1.0, 1e-6);
    EXPECT_NEAR(unregularized.x(1), 1.0, 1e-6);
}

// primal_delta's own behavioral counterpart to the dual_mu test above --
// before this test the field had only NaN-validation coverage
// (OverrideValidationMatchesTheWalksPrecondition below), asymmetric with
// dual_mu's demonstrated effect (T3 re-review; final branch review WAVE
// #10).
//
// primal_delta does NOT move the CONVERGED point on a fixture that
// converges -- that is this file's own documented invariant (section 7's
// ladder comment: "sigma costs ITERATIONS, never ACCURACY, and the
// iteration's fixed points remain exact, unregularized KKT points", and
// primal_delta occupies the same diagonal slot sigma does). The shipped
// default (1e-8) here converges to the EXACT unregularized minimizer of an
// ill-conditioned diagonal H (H00 = 1e-6 vs H11 = 1.0): x0 = -g0/H00 = 2e6,
// x1 = -g1/H11 = 2. So the observable effect of the override has to be on
// WHETHER/HOW it converges, not on the answer -- and on this same
// ill-conditioned fixture, overriding primal_delta up to 0.01 (1e6x the
// shipped default) is enough regularization to make the Newton direction
// stop being a descent direction for the FB merit, and the solve escapes
// with kNumericalError instead of certifying (measured; not re-derived from
// a formula, since the merit/Armijo interaction is engine-private).
TEST(SsnEngineLocal, PrimalDeltaOverrideChangesWhetherTheSolveConverges) {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(2, 2);
    Hd(0, 0) = 1e-6;
    Hd(1, 1) = 1.0;
    qp.H = Hd.sparseView();
    qp.g = Vec::Constant(2, -2.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);

    SsnEngine engine(default_opts());
    SsnResult defaulted;
    engine.solve(qp, SsnStart{}, SsnOptions{}, &defaulted);
    ASSERT_EQ(defaulted.status, QpStatus::kOptimal);
    EXPECT_NEAR(defaulted.x(0), 2.0e6, 1.0);
    EXPECT_NEAR(defaulted.x(1), 2.0, 1e-6);

    SolveOverrides big_delta;
    big_delta.primal_delta = 0.01; // 1e6x the shipped default
    SsnResult regularized;
    engine.solve(qp, SsnStart{}, SsnOptions{}, big_delta, &regularized);
    EXPECT_EQ(regularized.status, QpStatus::kNumericalError)
        << "primal_delta = 0.01 must be the override's own effect: the same fixture converges "
           "cleanly at the shipped default above";
    EXPECT_EQ(regularized.escape_reason, SsnEscape::kNoContraction);
}

TEST(SsnEngineLocal, OverrideValidationMatchesTheWalksPrecondition) {
    const QpProblem qp = tr_probe_qp();
    SsnEngine engine(default_opts());
    SsnResult res;

    SolveOverrides negative;
    negative.tr_radius = -1.0;
    EXPECT_THROW(engine.solve(qp, SsnStart{}, SsnOptions{}, negative, &res), std::invalid_argument);

    SolveOverrides nan_radius;
    nan_radius.tr_radius = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(engine.solve(qp, SsnStart{}, SsnOptions{}, nan_radius, &res),
                 std::invalid_argument);

    SolveOverrides nan_delta;
    nan_delta.primal_delta = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(engine.solve(qp, SsnStart{}, SsnOptions{}, nan_delta, &res),
                 std::invalid_argument);

    SolveOverrides nan_mu;
    nan_mu.dual_mu = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(engine.solve(qp, SsnStart{}, SsnOptions{}, nan_mu, &res), std::invalid_argument);

    // Zero is a legal radius (it pins every variable at the start point).
    SolveOverrides zero;
    zero.tr_radius = 0.0;
    EXPECT_NO_THROW(engine.solve(qp, SsnStart{}, SsnOptions{}, zero, &res));
    EXPECT_TRUE(res.tr_active[0]);
    EXPECT_TRUE(res.tr_active[1]);
}

// =============================================================================
// phi's CANCELLATION-FREE FORM (Fable kernel review, M-2)
// =============================================================================
//
// The naive a + b - sqrt(a^2+b^2) has an absolute error floor of ~ulp(rho)/2 on
// far-slack rows: fl(sqrt(fl(s*s))) == s exactly in IEEE double, and
// fl(s + lambda) == s for any |lambda| < ulp(s)/2, so a row with s = 1e6 and
// lambda = 1e-14 -- true phi ~1e-14 -- evaluates to EXACTLY ZERO. It
// under-reports rather than adding noise, which is why no stall exists today;
// what it costs is an additive slop on the exit certificate, and Task 4's merit
// 1/2||F||^2 would inherit the same quantization.
//
// The shipped form 2ab/(a+b+rho) is algebraically identical and carries no
// cancellation. Asserted here against the naive expression directly, because
// the difference is invisible through ||F||inf: the offending row's multiplier
// contributes the SAME order to stationarity as its phi does to the residual,
// so the infinity norm cannot separate them. This is the only place the
// improvement is observable, which is exactly why it is worth pinning.
TEST(SsnEngineLocal, FbIsEvaluatedWithoutCancellation) {
    auto naive = [](double a, double b) { return a + b - std::sqrt(a * a + b * b); };

    // The far-slack regime the review measured: the naive form returns 0, the
    // shipped one returns the true value to a relative 1e-12.
    const double far_slacks[] = {1e3, 1e6, 1e8};
    const double tiny_multipliers[] = {1e-14, 1e-16, 1e-18};
    for (double s : far_slacks) {
        for (double lam : tiny_multipliers) {
            EXPECT_DOUBLE_EQ(naive(s, lam), 0.0) << "naive floor assumption, s=" << s;
            const double phi = detail::ssn_fb(s, lam);
            EXPECT_GT(phi, 0.0) << "s=" << s << " lam=" << lam;
            EXPECT_NEAR(phi / lam, 1.0, 1e-12) << "s=" << s << " lam=" << lam;
        }
    }

    // Everywhere the naive form is already accurate the two must agree, so the
    // change is a strict improvement and not a different function: the kink,
    // both one-sided axes, mixed signs, and the ordinary O(1) regime.
    const double vals[] = {-4.0, -1.0, -1e-8, 0.0, 1e-8, 0.25, 1.0, 3.0};
    for (double a : vals) {
        for (double b : vals) {
            EXPECT_NEAR(detail::ssn_fb(a, b), naive(a, b),
                        1e-12 * std::max(1.0, std::abs(naive(a, b))))
                << "a=" << a << " b=" << b;
        }
    }
    EXPECT_DOUBLE_EQ(detail::ssn_fb(0.0, 0.0), 0.0);
    // The defining property, on both branches of the shipped implementation.
    EXPECT_DOUBLE_EQ(detail::ssn_fb(2.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(detail::ssn_fb(0.0, 2.0), 0.0);
}

// =============================================================================
// THE CONTRADICTORY kFixed HINT (Fable kernel review, M-3)
// =============================================================================
//
// export_activity reports kFixed when BOTH of a variable's bound rows come out
// implied-active, which a noisy or escaped iterate can produce on an l < u
// variable -- and that export is what Task 5 re-ingests as a hint, so the
// kernel can feed itself this. bound_hint_active then marks both rows active
// and the first step solves the contradictory pair {x_j = l_j, x_j = u_j}.
//
// **THE SHIPPED CHOICE IS DOCUMENTED RECOVERY, NOT REJECTION** (rationale in
// ssn_engine.h's bound_hint_active banner: rejecting would make a warm-start
// hand-off throw because the previous solve stopped somewhere noisy). This test
// is that choice's contract: it must converge, and to the right point.
TEST(SsnEngineLocal, ContradictoryFixedHintRecovers) {
    QpProblem qp = box_qp(12); // l = -1 < u = 1 on every variable
    SsnEngine engine(default_opts());

    SsnStart start;
    // Every variable hinted "fixed" though none is: the first step asks for
    // x_j = -1 and x_j = +1 simultaneously, on all twelve.
    start.activity_hint.bounds.assign(12, BoundState::kFixed);

    SsnOptions sopts;
    SsnResult res;
    ASSERT_NO_THROW(engine.solve(qp, start, sopts, &res));

    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.escape_reason, SsnEscape::kNone);
    EXPECT_LT(res.fb_residual, sopts.fb_tol);
    EXPECT_LE(res.iters, 9); // observed: 8 -- the cost of a bad hint, not a wrong answer

    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < qp.n(); ++j) {
        EXPECT_NEAR(res.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
    }
}

// The other half of the kFixed contract, and the half that says WHY the hint is
// honoured rather than ignored: on a genuinely fixed variable (l == u) a
// kFixed hint is CORRECT, and treating it as "both rows active" is what makes
// the first step the exact PDAS solve. A kernel that quietly read kFixed as
// "neither row active" would still converge on the fixture above -- the FB
// branch repairs anything -- so only this direction pins the semantics.
TEST(SsnEngineLocal, FixedVariableHintIsHonouredAsBothRowsActive) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(3, 3).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(3, -2.0); // unconstrained minimum at (2, 2, 2)
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -1e20);
    qp.upper = Vec::Constant(3, 1e20);
    qp.lower(0) = 0.5; // variable 0 is genuinely FIXED at 0.5
    qp.upper(0) = 0.5;

    SsnEngine engine(default_opts());
    SsnStart start;
    start.activity_hint.bounds = {BoundState::kFixed, BoundState::kFree, BoundState::kFree};

    SsnOptions sopts;
    SsnResult res;
    engine.solve(qp, start, sopts, &res);

    ASSERT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_LT(res.fb_residual, sopts.fb_tol);
    EXPECT_NEAR(res.x(0), 0.5, 1e-6);
    EXPECT_NEAR(res.x(1), 2.0, 1e-6);
    EXPECT_NEAR(res.x(2), 2.0, 1e-6);

    // **TWO STEPS, NOT ONE, AND THE REASON IS WORTH RECORDING**: the hinted
    // step solves the fixed variable's two rows as a rank-deficient equality
    // pair, and the min-norm split it lands on is unconstrained in SIGN --
    // measured (-0.75, +0.75) where the solution needs (0, 1.5). One FB step
    // then pushes the negative one to zero. So even a perfectly correct hint
    // costs 2 on a genuinely fixed variable; "one PDAS solve" is the
    // one-sided-bound story, not this one.
    EXPECT_EQ(res.iters, 2);

    // The hint is nonetheless doing real work -- without it the same QP costs 6.
    SsnEngine cold_engine(default_opts());
    SsnResult cold;
    cold_engine.solve(qp, SsnStart{}, sopts, &cold);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);
    EXPECT_EQ(cold.iters, 6);
    EXPECT_LT(res.iters, cold.iters) << "kFixed must be read as BOTH rows active";

    // An l == u variable reads kFixed, matching the walk's convention -- see
    // export_activity's structurally-fixed pass for why the structural fact
    // wins over the partition (which alone would say kAtUpper here).
    EXPECT_EQ(res.bound_state[0], BoundState::kFixed);
    EXPECT_EQ(res.bound_state[1], BoundState::kFree);

    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    EXPECT_EQ(res.bound_state[0], walk.bound_state[0]);
    for (Index j = 0; j < 3; ++j) {
        EXPECT_NEAR(res.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
        EXPECT_NEAR(res.z(j), walk.z(j), 1e-6) << "z(" << j << ")";
    }
}

// =============================================================================
// THE FIXED PATTERN (ssn_engine.h section 3)
// =============================================================================
//
// One symbolic analysis for the first solve of a structure; ZERO for every
// Newton step after the first (a branch change moves a diagonal VALUE, never
// a slot) and ZERO for a later QP of the same structure on the same engine.
TEST(SsnEngineLocal, SymbolicAnalysisIsPaidOncePerStructure) {
    QpProblem qp = box_qp(20);
    SsnEngine engine(default_opts());

    SsnStart start;
    start.activity_hint.bounds.assign(20, BoundState::kAtLower);

    SsnResult first;
    engine.solve(qp, start, SsnOptions{}, &first);
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    ASSERT_GT(first.iters, 1) << "the fixture must take several steps for this to mean anything";
    // Several factorizations, ONE analysis -- the whole point.
    EXPECT_EQ(first.factorizations, certified_factorizations(first.iters));
    EXPECT_EQ(first.symbolic_analyses, 1);

    // A DIFFERENT QP of identical structure: same sparsity everywhere, new
    // numbers. Nothing is re-analyzed.
    qp.g *= -1.0;
    SsnResult second;
    engine.solve(qp, start, SsnOptions{}, &second);
    ASSERT_EQ(second.status, QpStatus::kOptimal);
    EXPECT_GT(second.iters, 0);
    EXPECT_EQ(second.symbolic_analyses, 0);
    EXPECT_EQ(second.factorizations, certified_factorizations(second.iters));

    // A structurally DIFFERENT QP does pay again -- otherwise the assertion
    // above would also pass for an engine that never analyzes at all.
    const QpProblem other = box_qp(21);
    SsnResult third;
    SsnStart cold;
    engine.solve(other, cold, SsnOptions{}, &third);
    EXPECT_EQ(third.symbolic_analyses, 1);
}

// The pattern must not depend on which branch a row is in: the SAME QP solved
// from an all-active hint and from an all-inactive hint produces the same
// matrix structure, so the second solve re-analyzes nothing.
TEST(SsnEngineLocal, BranchChangeDoesNotChangeThePattern) {
    const QpProblem qp = box_qp(12);
    SsnEngine engine(default_opts());

    SsnStart all_lower;
    all_lower.activity_hint.bounds.assign(12, BoundState::kAtLower);
    SsnStart all_free;
    all_free.activity_hint.bounds.assign(12, BoundState::kFree);

    SsnResult a;
    engine.solve(qp, all_lower, SsnOptions{}, &a);
    SsnResult b;
    engine.solve(qp, all_free, SsnOptions{}, &b);

    ASSERT_EQ(a.status, QpStatus::kOptimal);
    ASSERT_EQ(b.status, QpStatus::kOptimal);
    EXPECT_EQ(a.symbolic_analyses, 1);
    EXPECT_EQ(b.symbolic_analyses, 0);
    for (Index j = 0; j < qp.n(); ++j) {
        EXPECT_NEAR(a.x(j), b.x(j), 1e-8) << "coordinate " << j;
    }
}

// The pattern is reused across solve() CALLS, not merely across Newton steps
// (review fix round 1, M5). Before this, `symbolic_analyses` told only half the
// story: Pardiso's phase 11 was skipped, but this file still rebuilt K from a
// triplet list -- an O(nnz log nnz) sort per subproblem, which is the same
// order as the factorization it precedes at the sizes Task 6 runs.
// `pattern_rebuilds` is the other half.
TEST(SsnEngineLocal, PatternIsReusedAcrossSolvesOfTheSameStructure) {
    QpProblem qp = mixed_block_qp();
    SsnEngine engine(default_opts());

    SsnResult first;
    engine.solve(qp, SsnStart{}, SsnOptions{}, &first);
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    EXPECT_EQ(first.pattern_rebuilds, 1);
    EXPECT_EQ(first.symbolic_analyses, 1);

    // Same structure, different numbers everywhere the values live: H, g, Ae,
    // Ai, be, bi and the bound VALUES all move, and none of them is structure.
    QpProblem moved = qp;
    moved.g *= -0.75;
    moved.be *= 2.0;
    moved.bi *= 0.5;
    moved.H.coeffs() *= 1.3;
    moved.Ae.coeffs() *= 1.1;
    moved.Ai.coeffs() *= -1.0;
    moved.lower = Vec::Constant(qp.n(), -1.2);
    moved.upper = Vec::Constant(qp.n(), 0.9);

    SsnResult second;
    engine.solve(moved, SsnStart{}, SsnOptions{}, &second);
    ASSERT_EQ(second.status, QpStatus::kOptimal);
    EXPECT_EQ(second.pattern_rebuilds, 0) << "same structure must not re-derive the pattern";
    EXPECT_EQ(second.symbolic_analyses, 0);

    // The refreshed values must be RIGHT, not merely cheap -- the reuse path
    // zeroes and re-accumulates, and a bug there would show up as a wrong
    // answer, not as a wrong counter.
    const QpSolution walk = walk_solution(moved);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < moved.n(); ++j) {
        EXPECT_NEAR(second.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
    }

    // A structural change -- one fewer finite bound -- forces the rebuild back.
    QpProblem unbounded_one = qp;
    unbounded_one.lower(0) = -1e20;
    SsnResult third;
    engine.solve(unbounded_one, SsnStart{}, SsnOptions{}, &third);
    EXPECT_EQ(third.pattern_rebuilds, 1);

    // ... and so does a change in a matrix's SPARSITY (same dimensions, one
    // extra stored entry), which the dimension check alone would miss.
    QpProblem denser = qp;
    Eigen::MatrixXd Aid = Eigen::MatrixXd(denser.Ai);
    Aid(0, 7) = 0.25; // a structurally new nonzero
    denser.Ai = Aid.sparseView();
    SsnResult fourth;
    engine.solve(denser, SsnStart{}, SsnOptions{}, &fourth);
    EXPECT_EQ(fourth.pattern_rebuilds, 1);
}

// QpProblem does NOT require its matrices to be compressed, and the structure
// key must be exact for an uncompressed one too -- an uncompressed
// SparseMatrix keeps per-row gaps that its raw innerIndexPtr()/nonZeros() pair
// does not describe, so a key read off those arrays could collide two
// different structures and reuse a pattern that does not fit. The key iterates
// instead. This test drives the reuse path with an uncompressed Ai.
TEST(SsnEngineLocal, PatternKeyIsExactForUncompressedInput) {
    // n = 3, two inequality rows, no equalities, no finite bounds. Built IN
    // PLACE, because SparseMatrix's assignment operator compresses -- an
    // uncompressed matrix can only reach QpProblem by being inserted into.
    auto make = [](Index second_row_first_col) {
        QpProblem qp;
        qp.H = Eigen::MatrixXd::Identity(3, 3)
                   .triangularView<Eigen::Upper>()
                   .toDenseMatrix()
                   .sparseView();
        qp.g = Vec::Constant(3, -2.0);
        qp.Ae.resize(0, 3);
        qp.be = Vec(0);
        qp.Ai.resize(2, 3);
        qp.Ai.reserve(Eigen::VectorXi::Constant(2, 3));
        qp.Ai.insert(0, 0) = 1.0;
        qp.Ai.insert(0, 1) = 1.0;
        qp.Ai.insert(1, second_row_first_col) = 1.0;
        qp.Ai.insert(1, 2) = 1.0;
        qp.bi = Vec(2);
        qp.bi << 2.0, 2.0;
        qp.lower = Vec::Constant(3, -1e20);
        qp.upper = Vec::Constant(3, 1e20);
        return qp;
    };

    QpProblem qp = make(1); // row 1 stores columns {1, 2}
    ASSERT_FALSE(qp.Ai.isCompressed());

    SsnEngine engine(default_opts());
    SsnResult first;
    engine.solve(qp, SsnStart{}, SsnOptions{}, &first);
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    EXPECT_EQ(first.pattern_rebuilds, 1);

    // Same structure again: reused, and still right.
    SsnResult second;
    engine.solve(qp, SsnStart{}, SsnOptions{}, &second);
    ASSERT_EQ(second.status, QpStatus::kOptimal);
    EXPECT_EQ(second.pattern_rebuilds, 0);
    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < 3; ++j) {
        EXPECT_NEAR(second.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
    }

    // A DIFFERENT structure with the SAME dimensions and the SAME nonZeros():
    // row 1 stores columns {0, 2} instead of {1, 2}. This is exactly the case a
    // key read off the raw index arrays of an uncompressed matrix could fail to
    // separate, and reusing the previous pattern here would put Ai's entries in
    // the wrong columns of K.
    const QpProblem shifted = make(0);
    ASSERT_EQ(shifted.Ai.nonZeros(), qp.Ai.nonZeros());
    SsnResult third;
    engine.solve(shifted, SsnStart{}, SsnOptions{}, &third);
    ASSERT_EQ(third.status, QpStatus::kOptimal);
    EXPECT_EQ(third.pattern_rebuilds, 1) << "a moved nonzero is a different structure";
    const QpSolution shifted_walk = walk_solution(shifted);
    ASSERT_EQ(shifted_walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < 3; ++j) {
        EXPECT_NEAR(third.x(j), shifted_walk.x(j), 1e-6) << "x(" << j << ")";
    }
}

// Two bound LAYOUTS with the same NUMBER of bound rows are different
// structures, and the row count alone cannot tell them apart. This is the case
// the first fix-round sweep found uncovered: a mutation deleting the bound-row
// contribution to the structure key SURVIVED, because every other bound change
// in the suite also changes mb.
//
//   A: lower bounds on variables 0 and 1  -> rows on vars {0, 1}, signs {-,-}
//   B: lower bounds on variables 0 and 2  -> rows on vars {0, 2}, signs {-,-}
//
// Both have mb = 2 AND THE SAME SIGN SEQUENCE, which isolates the VARIABLE half
// of the key: it is the only thing separating them. (Deliberate -- a first
// attempt used "both bounds of variable 0" against "one bound each", which two
// different halves of the key both separate, and a mutant that kept only the
// signs survived it.) Their bound blocks put off-diagonal entries in different
// COLUMNS of K, so reusing A's pattern for B would write B's values into A's
// slots -- silently, and with a wrong answer rather than a slow one.
TEST(SsnEngineLocal, PatternKeySeparatesBoundLayoutsOfEqualSize) {
    auto with_lower_on = [](Index a, Index b) {
        QpProblem qp;
        qp.H = Eigen::MatrixXd::Identity(3, 3)
                   .triangularView<Eigen::Upper>()
                   .toDenseMatrix()
                   .sparseView();
        qp.g = Vec::Zero(3); // unconstrained minimum at the origin
        qp.Ae.resize(0, 3);
        qp.be = Vec(0);
        qp.Ai.resize(0, 3);
        qp.bi = Vec(0);
        qp.lower = Vec::Constant(3, -1e20);
        qp.upper = Vec::Constant(3, 1e20);
        qp.lower(a) = 1.0; // both bounds ACTIVE at the solution, so a
        qp.lower(b) = 1.0; // misplaced entry cannot hide
        return qp;
    };

    const QpProblem a = with_lower_on(0, 1);
    const QpProblem b = with_lower_on(0, 2);

    SsnEngine engine(default_opts());
    SsnResult ra;
    engine.solve(a, SsnStart{}, SsnOptions{}, &ra);
    ASSERT_EQ(ra.status, QpStatus::kOptimal);
    EXPECT_EQ(ra.pattern_rebuilds, 1);
    EXPECT_NEAR(ra.x(0), 1.0, 1e-6);
    EXPECT_NEAR(ra.x(1), 1.0, 1e-6);
    EXPECT_NEAR(ra.x(2), 0.0, 1e-6);

    SsnResult rb;
    engine.solve(b, SsnStart{}, SsnOptions{}, &rb);
    ASSERT_EQ(rb.status, QpStatus::kOptimal);
    EXPECT_EQ(rb.pattern_rebuilds, 1) << "same mb and signs, different variables";
    EXPECT_NEAR(rb.x(0), 1.0, 1e-6);
    EXPECT_NEAR(rb.x(1), 0.0, 1e-6);
    EXPECT_NEAR(rb.x(2), 1.0, 1e-6);
    EXPECT_EQ(rb.bound_state[0], BoundState::kAtLower);
    EXPECT_EQ(rb.bound_state[1], BoundState::kFree);
    EXPECT_EQ(rb.bound_state[2], BoundState::kAtLower);

    const QpSolution walk = walk_solution(b);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < 3; ++j) {
        EXPECT_NEAR(rb.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
        EXPECT_NEAR(rb.z(j), walk.z(j), 1e-6) << "z(" << j << ")";
    }
}

// A sign FLIP at a constant bound layout -- same mb, same variable sequence,
// identical H/Ae/Ai patterns; variable 1 trades its lower bound for its upper
// -- moves a VALUE, not a slot: the bound block's off-diagonal entry stays in
// the same column of K, so reusing the pattern here would be CORRECT. The key
// still rebuilds, DELIBERATELY (see bound_rows_match_cached in ssn_engine.h:
// an over-conservative key costs one avoidable rebuild in exactly this case
// and buys never re-deriving the argument) -- and that conservatism is pinned
// currency, not a preference: the pre-H3 in-hash key rebuilt here, rebuild
// counters are asserted across the corpus, so the composite key must
// reproduce the decision exactly. This pins the sign half of the bound-row
// conjunct, which the exact comparison made killable -- the old in-hash sign
// term was recorded as knowingly UNkillable, because no fixture can forge the
// FNV collision that would distinguish it from the var term it was mixed in
// with. A comparison is distinguishable: a mutant dropping the sign check
// reuses here and fails the rebuild count below.
TEST(SsnEngineLocal, SignFlipAtConstantBoundLayoutForcesRebuild) {
    auto box_on_var1 = [](bool lower_side) {
        QpProblem qp;
        qp.H = Eigen::MatrixXd::Identity(3, 3)
                   .triangularView<Eigen::Upper>()
                   .toDenseMatrix()
                   .sparseView();
        qp.g = Vec::Zero(3);
        // Push variable 1 through its bound so the bound is ACTIVE at the
        // solution and a misplaced or stale entry cannot hide.
        qp.g(1) = lower_side ? 3.0 : -3.0;
        qp.Ae.resize(0, 3);
        qp.be = Vec(0);
        qp.Ai.resize(0, 3);
        qp.bi = Vec(0);
        qp.lower = Vec::Constant(3, -1e20);
        qp.upper = Vec::Constant(3, 1e20);
        if (lower_side) {
            qp.lower(1) = -1.0; // row (var 1, sign -1)
        } else {
            qp.upper(1) = 1.0; // row (var 1, sign +1)
        }
        return qp;
    };

    const QpProblem a = box_on_var1(true);
    const QpProblem b = box_on_var1(false);

    SsnEngine engine(default_opts());
    SsnResult ra;
    engine.solve(a, SsnStart{}, SsnOptions{}, &ra);
    ASSERT_EQ(ra.status, QpStatus::kOptimal);
    EXPECT_EQ(ra.pattern_rebuilds, 1);
    EXPECT_NEAR(ra.x(1), -1.0, 1e-6);
    EXPECT_EQ(ra.bound_state[1], BoundState::kAtLower);

    SsnResult rb;
    engine.solve(b, SsnStart{}, SsnOptions{}, &rb);
    ASSERT_EQ(rb.status, QpStatus::kOptimal);
    EXPECT_EQ(rb.pattern_rebuilds, 1) << "same layout, flipped sign: the conservative rebuild";
    EXPECT_NEAR(rb.x(1), 1.0, 1e-6);
    EXPECT_EQ(rb.bound_state[1], BoundState::kAtUpper);

    const QpSolution walk = walk_solution(b);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    for (Index j = 0; j < 3; ++j) {
        EXPECT_NEAR(rb.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
    }

    // The equal side of the conjunct: an identical re-solve still reuses.
    SsnResult rb2;
    engine.solve(b, SsnStart{}, SsnOptions{}, &rb2);
    ASSERT_EQ(rb2.status, QpStatus::kOptimal);
    EXPECT_EQ(rb2.pattern_rebuilds, 0) << "identical structure and bound rows: reuse";
}

// =============================================================================
// HOW THE COUNT SCALES IN n (review fix round 1, I2)
// =============================================================================
//
// ============ THE COUNT IS NOT FLAT IN n. IT GROWS, SLOWLY. ============
// An earlier version of the fixture-(c) banner claimed "6 or 8, never more,
// never fewer" and "flat in n"; both were wrong, and were contradicted inside
// this very family. The measured grid on the SHIPPED box_qp (M-matrix H,
// off-diagonal -0.3), default fb_tol:
//
//     n        |  12   20   50  100  200  400   walk minors
//     ---------+---------------------------------------------
//     scramble |   7    7    8    8    9    9    9 14 30 53 62 62
//     all-lower|   8    8    8    8    9    9
//     no hint  |   6    6    6    6    7    7
//
// So: 7 -> 9 over a 33x range in n, against the walk's 9 -> 62. Very slow
// growth, NOT no growth.
//
// **WHY THIS MATTERS BEYOND HONESTY: G3 IS A NO-GROWTH GATE.** This banner is
// currently the only in-repo evidence about the kernel's n-scaling, so it must
// not be quotable as flatness. What it actually supports is "growth far slower
// than the walk's on this family", which is a different and weaker claim; Task
// 6's corpus is what settles G3, and this fixture family -- a bounds-only
// M-matrix box, with no safeguards -- is not it.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, BoxQpCountGrowsSlowlyInN) {
    struct Cell {
        Index n;
        Index scramble_cap;
        Index no_hint_cap;
    };
    // Caps ARE the measurements in the table above, and **EVERY CELL THE
    // BANNER CITES IS PINNED HERE** (round 2: n = 20 and n = 100 were quoted
    // in the table but not asserted, so two of the six columns were prose
    // rather than evidence). n = 400 costs ~4 ms and the whole test ~25 ms, so
    // pinning all six is affordable per-commit; n = 20 and n = 100 are also the
    // two cells that carry the growth STEPS -- 7 -> 8 happens between 20 and
    // 50, and 8 -> 9 between 100 and 200 -- so a regression that moved either
    // boundary would previously have been invisible to the suite.
    const Cell cells[] = {{12, 7, 6},  {20, 7, 6},  {50, 8, 6},
                          {100, 8, 6}, {200, 9, 7}, {400, 9, 7}};

    for (const Cell &c : cells) {
        const QpProblem qp = box_qp(c.n);
        for (int hinted = 0; hinted < 2; ++hinted) {
            SsnEngine engine(default_opts());
            SsnStart start;
            if (hinted != 0) {
                start.activity_hint.bounds.assign(static_cast<std::size_t>(c.n), BoundState::kFree);
                for (Index j = 0; j < c.n; ++j) {
                    const int r = static_cast<int>(j % 3);
                    start.activity_hint.bounds[static_cast<std::size_t>(j)] =
                        r == 0 ? BoundState::kAtLower
                               : (r == 1 ? BoundState::kAtUpper : BoundState::kFree);
                }
            }
            SsnOptions sopts;
            SsnResult res;
            engine.solve(qp, start, sopts, &res);
            EXPECT_EQ(res.status, QpStatus::kOptimal) << "n = " << c.n << ", hinted = " << hinted;
            EXPECT_LE(res.iters, hinted != 0 ? c.scramble_cap : c.no_hint_cap)
                << "n = " << c.n << ", hinted = " << hinted;
            EXPECT_LT(res.fb_residual, sopts.fb_tol) << "n = " << c.n;
        }
    }
}

// =============================================================================
// BUDGET AND ESCAPES
// =============================================================================
TEST(SsnEngineLocal, HardBudgetStopsWithABudgetEscape) {
    const QpProblem qp = two_row_qp();
    SsnEngine engine(default_opts());

    SsnOptions sopts;
    sopts.hard_budget = 1; // not enough from a cold seed
    SsnResult res;
    engine.solve(qp, SsnStart{}, sopts, &res);

    EXPECT_EQ(res.status, QpStatus::kMaxIter);
    EXPECT_EQ(res.escape_reason, SsnEscape::kBudget);
    EXPECT_EQ(res.iters, 1);
    EXPECT_EQ(res.counters.ssn_escapes, 1);

    // hard_budget = 0 is legal and means "test the start point, take no step".
    sopts.hard_budget = 0;
    SsnResult none;
    engine.solve(qp, SsnStart{}, sopts, &none);
    EXPECT_EQ(none.status, QpStatus::kMaxIter);
    EXPECT_EQ(none.escape_reason, SsnEscape::kBudget);
    EXPECT_EQ(none.iters, 0);
    EXPECT_EQ(none.factorizations, 0);
}

// A Newton step that comes back non-finite is REPORTED, not thrown: solver
// outcomes come back as a status, only caller errors throw. The trigger here
// is a non-finite objective gradient, which is the one route to this branch
// that is deterministic on both backends -- an exactly singular K is NOT, since
// Pardiso answers a zero pivot by PERTURBING it (the factorization
// evidence's perturbed-pivot count) and returning a finite, meaningless step rather than
// an error, which this file verified directly on three rank-deficient and
// zero-Hessian probes before settling on this one.
TEST(SsnEngineLocal, NonFiniteStepIsReportedNotThrown) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << std::numeric_limits<double>::infinity(), 1.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);

    SsnEngine engine(default_opts());
    SsnResult res;
    ASSERT_NO_THROW(engine.solve(qp, SsnStart{}, SsnOptions{}, &res));
    EXPECT_EQ(res.status, QpStatus::kNumericalError);
    EXPECT_EQ(res.escape_reason, SsnEscape::kSingular);
    EXPECT_FALSE(res.escape_detail.empty());
    EXPECT_EQ(res.counters.ssn_escapes, 1);
    EXPECT_EQ(res.iters, 0); // the bad step is discarded, never applied
}

// =============================================================================
// THE FB DIAGONAL CARRIES dual_mu, AND THAT IS OBSERVABLE
// =============================================================================
//
// The per-row FB diagonal is -(beta/alpha_f + mu), not -(beta/alpha_f): the
// same QpOptions::dual_mu the walk's KKT assembly applies (kkt_assembly.h). On
// a well-posed active set mu is invisible at 1e-8, so the property has to be
// asserted where it is NOT: a RANK-DEFICIENT active set, here two identical
// active rows. The dual is then non-unique -- any (t, 1-t) is a valid
// multiplier pair -- and the regularized system picks the MINIMUM-NORM one,
// (0.5, 0.5), by construction rather than by pivot order. Drop mu and the
// answer becomes whatever the factorization happens to produce; measured, it
// is (0, 1). Both are legitimate duals, which is exactly why this needs an
// assertion rather than an accident. (Named mutant M12 in the Task-3 report.)
TEST(SsnEngineLocal, DualRegularizationPicksTheMinimumNormMultiplier) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -2.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 1, 1, 1; // identical rows: the active set is rank deficient
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(2, 2.0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);

    SsnEngine engine(default_opts());
    SsnStart start;
    start.activity_hint.ineq = {true, true};
    SsnResult res;
    engine.solve(qp, start, SsnOptions{}, &res);

    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.iters, 1);
    EXPECT_NEAR(res.x(0), 1.0, 1e-6);
    EXPECT_NEAR(res.x(1), 1.0, 1e-6);
    EXPECT_NEAR(res.lambda_i(0), 0.5, 1e-6);
    EXPECT_NEAR(res.lambda_i(1), 0.5, 1e-6);
}

// =============================================================================
// TOLERANCE DERIVATION AND INPUT VALIDATION
// =============================================================================
TEST(SsnEngineLocal, FbTolDerivationAndItsBound) {
    EXPECT_DOUBLE_EQ(ssn_fb_tol_from_kkt_tol(1e-6), 1e-6);
    EXPECT_DOUBLE_EQ(ssn_fb_tol_from_kkt_tol(1e-9), 1e-9);
    EXPECT_THROW(ssn_fb_tol_from_kkt_tol(0.0), std::invalid_argument);
    EXPECT_THROW(ssn_fb_tol_from_kkt_tol(-1.0), std::invalid_argument);
    EXPECT_DOUBLE_EQ(SsnOptions{}.fb_tol, SqpOptions{}.kkt_tol);

    // The two-sided bound ssn_engine.h section 5 derives, checked directly:
    // (2 - sqrt(2)) * min(a, b) <= phi(a, b) <= min(a, b) for a, b >= 0.
    const double lo = 2.0 - std::sqrt(2.0);
    for (double a = 0.0; a <= 4.0; a += 0.25) {
        for (double b = 0.0; b <= 4.0; b += 0.25) {
            const double phi = a + b - std::sqrt(a * a + b * b);
            const double m = std::min(a, b);
            EXPECT_GE(phi, lo * m - 1e-12) << a << ", " << b;
            EXPECT_LE(phi, m + 1e-12) << a << ", " << b;
        }
    }
    EXPECT_NEAR(detail::kSsnComplementarityFactor, 1.0 / lo, 1e-15);
}

TEST(SsnEngineLocal, CallerErrorsThrow) {
    const QpProblem qp = two_row_qp();
    SsnEngine engine(default_opts());
    SsnResult res;

    EXPECT_THROW(engine.solve(qp, SsnStart{}, SsnOptions{}, nullptr), std::invalid_argument);

    SsnOptions bad = SsnOptions{};
    bad.fb_tol = 0.0;
    EXPECT_THROW(engine.solve(qp, SsnStart{}, bad, &res), std::invalid_argument);
    bad = SsnOptions{};
    bad.hard_budget = -1;
    EXPECT_THROW(engine.solve(qp, SsnStart{}, bad, &res), std::invalid_argument);
    bad = SsnOptions{};
    bad.soft_budget = -1;
    EXPECT_THROW(engine.solve(qp, SsnStart{}, bad, &res), std::invalid_argument);

    SsnStart wrong_x;
    wrong_x.x = Vec::Zero(3);
    EXPECT_THROW(engine.solve(qp, wrong_x, SsnOptions{}, &res), std::invalid_argument);

    SsnStart wrong_hint;
    wrong_hint.activity_hint.ineq = {true};
    EXPECT_THROW(engine.solve(qp, wrong_hint, SsnOptions{}, &res), std::invalid_argument);

    SsnStart wrong_bound_hint;
    wrong_bound_hint.activity_hint.bounds.assign(5, BoundState::kFree);
    EXPECT_THROW(engine.solve(qp, wrong_bound_hint, SsnOptions{}, &res), std::invalid_argument);

    // The Task-4 fields are size-checked even though they are never read, so a
    // caller who fills one wrongly is told rather than silently ignored.
    SsnStart wrong_slacks;
    wrong_slacks.slacks = Vec::Zero(7);
    EXPECT_THROW(engine.solve(qp, wrong_slacks, SsnOptions{}, &res), std::invalid_argument);
    SsnStart wrong_prox;
    wrong_prox.prox_center_lambda = Vec::Zero(7);
    EXPECT_THROW(engine.solve(qp, wrong_prox, SsnOptions{}, &res), std::invalid_argument);
}

// =============================================================================
// THE BARE MODE IS TASK 3, AND THAT IS ASSERTED RATHER THAN CLAIMED
// =============================================================================
//
// Task 3's own test file pinned an iteration count on every fixture above.
// SsnSafeguards::kBare must reproduce every one of them, because bare mode is
// not a re-implementation of the local method -- it is the SAME function with
// the safeguards switched off, so any divergence would mean a safeguard leaked
// into a path that is meant to be free of them.
//
// This is also what discharges the "the default changed" risk honestly: the
// numbers below are Task 3's, and the numbers the tests ABOVE assert are the
// safeguarded engine's. Where the two differ, both are recorded.
//
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, BareModeReproducesTheTask3Trajectories) {
    // (a) exact primal seed + correct hint -> 1 iteration.
    {
        SsnEngine engine(default_opts());
        SsnStart start;
        start.x = Vec(2);
        start.x << 1.0, 1.0;
        start.activity_hint.ineq = {true, false};
        SsnResult res;
        engine.solve(two_row_qp(), start, bare_opts(), &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 1);
    }
    // (b) wrong hint -> exactly 6, one bulk flip.
    {
        SsnEngine engine(default_opts());
        SsnStart start;
        start.x = Vec(2);
        start.x << 1.0, 1.0;
        start.activity_hint.ineq = {false, true};
        SsnResult res;
        engine.solve(two_row_qp(), start, bare_opts(), &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 6);
        EXPECT_EQ(res.counters.ssn_bulk_flips, 1);
    }
    // (c) n = 50 box, scrambled hint -> exactly 8.
    {
        SsnEngine engine(default_opts());
        SsnStart start;
        start.activity_hint.bounds.assign(50, BoundState::kFree);
        for (Index j = 0; j < 50; ++j) {
            const int r = static_cast<int>(j % 3);
            start.activity_hint.bounds[static_cast<std::size_t>(j)] =
                r == 0 ? BoundState::kAtLower : (r == 1 ? BoundState::kAtUpper : BoundState::kFree);
        }
        SsnResult res;
        engine.solve(box_qp(50), start, bare_opts(), &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 8);
    }
    // (d) equality only -> exactly 1.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(equality_only_qp(), SsnStart{}, bare_opts(), &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 1);
    }
    // The mixed-block fixture -> exactly 8.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(mixed_block_qp(), SsnStart{}, bare_opts(), &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 8);
    }
    // And every safeguard counter stays structurally zero, on every one of
    // them -- the assertion Task 3 left behind, kept where it is still true.
    {
        SsnEngine engine(default_opts());
        const QpProblem cases[] = {two_row_qp(), box_qp(20), equality_only_qp(), mixed_block_qp()};
        for (const QpProblem &qp : cases) {
            SsnResult res;
            engine.solve(qp, SsnStart{}, bare_opts(), &res);
            EXPECT_EQ(res.counters.ssn_backtracks, 0);
            EXPECT_EQ(res.counters.ssn_prox_updates, 0);
            EXPECT_EQ(res.counters.ssn_uncertain_peak, 0);
            EXPECT_EQ(res.prox_sigma, 0.0);
            // **kBare PAYS NO VERIFICATION**, so Task 3's exact contract holds
            // here verbatim -- one factorization per step, no plus one. That
            // asymmetry against certified_factorizations() above is the whole
            // measured cost of the fix-round-1 certificate.
            EXPECT_EQ(res.factorizations, res.iters);
            // Bare mode never classifies, so the uncertain export is sized and
            // uniformly false -- never missing, never true.
            EXPECT_EQ(static_cast<Index>(res.ineq_uncertain.size()), qp.mi());
            EXPECT_EQ(static_cast<Index>(res.bound_uncertain.size()), qp.n());
            for (const bool u : res.ineq_uncertain) {
                EXPECT_FALSE(u);
            }
            for (const bool u : res.bound_uncertain) {
                EXPECT_FALSE(u);
            }
        }
    }
}

// =============================================================================
// TASK-4 (a): THE CHR-CLASS CYCLING COUNTER-EXAMPLE
// =============================================================================
//
// The POSITIVE CONTROL half is what makes this test worth having: it asserts
// that the bare iteration really does fail, so "the safeguarded one converges"
// is a comparison rather than a tautology. Both fixtures are small, strictly
// convex, well conditioned and feasible (see their banners), so nothing here
// is a conditioning or a modelling artefact.
//
// **WHAT CYCLES IS A REPEATED PARTITION, AND THE PERIOD IS 6 AND 9, NOT 2.**
// The brief pre-registered "cycles between two partitions"; the measured
// behaviour on both instances is a longer orbit -- the 2-variable fixture
// returns to its step-0 partition at step 6, the 3-variable one at step 9 --
// and the assertion below is written as the measurement (a repeat at ANY
// separation of two or more inside ten steps) rather than as the guess.
//
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
// The iteration counts and the orbit lengths are trajectory properties. What
// is NOT backend-sensitive is the shape of the claim: bare fails inside the
// budget, safeguarded reaches the walk's answer.
TEST(SsnEngineLocal, ChrCyclingBareCyclesAndSafeguardedConverges) {
    struct Cell {
        const char *name;
        QpProblem qp;
        SsnStart start;
        Index bare_flips;
        Index full_iters;
    };
    const Cell cells[] = {
        {"2var wrong hint", cycling_qp_2var(), cycling_start_2var(), 13, 9},
        {"3var cold", cycling_qp_3var(), SsnStart{}, 10, 11},
    };

    for (const Cell &c : cells) {
        const QpSolution walk = walk_solution(c.qp);
        ASSERT_EQ(walk.status, QpStatus::kOptimal) << c.name;

        // --- positive control: BARE burns the whole budget and orbits -------
        SsnEngine bare_engine(default_opts());
        SsnResult bare;
        bare_engine.solve(c.qp, c.start, bare_opts(), &bare);
        EXPECT_EQ(bare.status, QpStatus::kMaxIter) << c.name;
        EXPECT_EQ(bare.escape_reason, SsnEscape::kBudget) << c.name;
        EXPECT_EQ(bare.iters, SsnOptions{}.hard_budget) << c.name;
        EXPECT_EQ(bare.counters.ssn_bulk_flips, c.bare_flips) << c.name;
        EXPECT_GT(bare.fb_residual, SsnOptions{}.fb_tol) << c.name;

        // The orbit itself: step the budget and look for a repeated partition
        // at a separation of >= 2 within ten steps.
        std::vector<std::string> trace;
        for (Index k = 0; k <= 10; ++k) {
            SsnEngine e(default_opts());
            SsnOptions so = bare_opts();
            so.hard_budget = k;
            SsnResult r;
            e.solve(c.qp, c.start, so, &r);
            ASSERT_NE(r.status, QpStatus::kOptimal) << c.name << " converged at k = " << k;
            trace.push_back(partition_string(r));
        }
        bool orbited = false;
        for (std::size_t i = 0; i + 2 < trace.size() && !orbited; ++i) {
            for (std::size_t j = i + 2; j < trace.size(); ++j) {
                if (trace[i] == trace[j]) {
                    orbited = true;
                    break;
                }
            }
        }
        EXPECT_TRUE(orbited) << c.name << ": no partition repeated within ten bare steps";

        // --- the safeguarded engine solves it -------------------------------
        SsnEngine full_engine(default_opts());
        SsnResult full;
        full_engine.solve(c.qp, c.start, SsnOptions{}, &full);
        EXPECT_EQ(full.status, QpStatus::kOptimal) << c.name;
        EXPECT_EQ(full.escape_reason, SsnEscape::kNone) << c.name;
        EXPECT_EQ(full.iters, c.full_iters) << c.name;
        EXPECT_LT(full.fb_residual, SsnOptions{}.fb_tol) << c.name;
        EXPECT_GT(full.counters.ssn_backtracks, 0) << c.name;
        for (Index j = 0; j < c.qp.n(); ++j) {
            EXPECT_NEAR(full.x(j), walk.x(j), 1e-6) << c.name << " x(" << j << ")";
        }
    }
}

// =============================================================================
// TASK-4 (b): A WEAKLY ACTIVE ROW FINISHES IN THE UNCERTAIN SET
// =============================================================================
//
// Row 1 of weakly_active_qp has c* = 0 AND lambda* = 0, so the binary rule
// "active iff lambda > s" is comparing two quantities that are both zero to
// within rounding -- a coin flip whose outcome the ACTIVITY EXPORT would
// otherwise present as fact. That is the failure this leg of the CHR partition
// exists to prevent, and it is measurable on this fixture in both directions:
//
//   * the export must never carry the row as inactive AND certain -- the
//     third set exists so that a coin flip is not exported as fact, and the
//     uncertain peak shows it fires on this solve; and
//   * bare mode -- which has no third set -- reads the tie the other way from
//     at least one start (the second cell below), reporting the row ACTIVE
//     where the walk reports it inactive, with a multiplier of 7e-28.
//
// It is also the honest place to record what the uncertain set does NOT buy:
// it moves no iteration count anywhere in this file, and the sweep in
// docs/notes/2026-08-07-ssn-safeguards.md section 4 found no cell where it
// changes an outcome. Its demonstrated value is the export.
//
// "No oscillation counter growth" is the brief's third clause and is asserted
// as a BAND on ssn_bulk_flips -- see the assertion for the band and its
// grounds.
//
// EVERY SENTENCE ABOVE WITH A DIRECTION IN IT -- which way the coin lands,
// which mode gets it "wrong", the single flip, AND WHETHER THE TWO MODES
// DISAGREE -- IS AN OBSERVATION, NOT AN ASSERTION. Nothing below asserts one;
// the second cell RECORDS its readings instead. What is asserted is what holds
// whichever way either coin lands: the tie is SEEN (ssn_uncertain_peak == 1),
// the safeguarded engine never exports row 1 as inactive-and-certain (from
// EITHER start), bare mode's uncertain flag is structurally false, and at
// least one bulk flip happened.
//
// WHY NO DIRECTION IS PINNED (M6 W0.4). This fixture used to carry per-backend
// direction pins -- MKL's ineq_active[1]/ineq_uncertain[1] and
// ssn_bulk_flips == 1, Accelerate's mirrored arm with ssn_bulk_flips == 4 --
// ruled in by the gate-B execution review (2026-08-15), Verdict 2 item 2, on
// the strength of ten stable CI runs per backend. They are RETIRED. They rested
// on the coin being stable WITHIN a backend, and it is not: on 2026-08-26 a
// main-push CI run failed exactly these assertions and a rerun of the SAME
// COMMIT passed, the coin landing the other way within MKL. That is the flake
// class docs/notes/2026-08-15-linux-runner-divergence-register.md L-1 has
// tracked since 2026-08-14, whose own mechanism note already named an exact pin
// on a tie-decided counter as the plausible root cause. A pin a rerun of
// identical source can break is not evidence about the library, and M3-4's own
// finding -- at c* = 0 the row IS at its boundary, so the twin readings are
// both correct readings -- is precisely why there is no direction there to pin.
// The bare-vs-full CONTRAST went with them, on harder evidence still: the L-1
// register's occurrence-3 failure table records
// `bool(bare.ineq_active[1]) != bool(full.ineq_active[1])` failing with actual
// "true vs true" -- under the flipped reading BOTH modes report the row active,
// and both then land (active, certain), which is a legitimate reading of the
// tie on each side. There is no portable contrast to assert, so the second cell
// records its two readings rather than comparing them.
//
// The DIVERGENCE ITSELF stays documented (that is what M3-4 records); only the
// assertions on it are gone.
//
// The register's re-open trigger is unchanged (Verdict 2 item 4): a crossover
// measurement attributing hint-quality loss to tie misclassification, first
// remedy to evaluate being margin-based uncertain membership. That would give
// this fixture a real property to pin; a coin direction never was one.
TEST(SsnEngineLocal, WeaklyActiveRowFinishesUncertain) {
    const QpProblem qp = weakly_active_qp();
    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    ASSERT_NEAR(walk.lambda_i(1), 0.0, 1e-12); // weakly active: zero multiplier
    ASSERT_FALSE(walk.ineq_active[1]);

    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, SsnStart{}, SsnOptions{}, &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 7);
        EXPECT_NEAR(res.x(0), 1.0, 1e-6);
        EXPECT_NEAR(res.x(1), 1.0, 1e-6);
        // Row 0 is strictly active and row 1 is the weakly active one.
        EXPECT_TRUE(res.ineq_active[0]);
        EXPECT_FALSE(res.ineq_uncertain[0]);
        // THE TIE IS SEEN: the peak says the third set held a row at some
        // point in this solve. True on both backends, and on whichever side
        // of the coin either lands.
        EXPECT_EQ(res.counters.ssn_uncertain_peak, 1);
        // THE ONE READING OF THE TIE THAT IS WRONG, and the whole of what is
        // asserted about row 1's end state: the safeguarded engine never ends
        // with the row dropped as a FACT. Inactive and certain at once throws
        // away a constraint the fixture put exactly on the boundary;
        // active, or uncertain, are both honest readings of c* = 0 and either
        // is accepted here.
        EXPECT_TRUE(res.ineq_active[1] || res.ineq_uncertain[1])
            << "the tie was returned inactive AND certain -- a weakly active row "
               "presented as fact";
        // A LOWER BOUND ONLY: the trajectory does flip at least once. The exact
        // counts this cell has produced -- 1 (MKL nominal), 3 (MKL, the
        // 2026-08-26 within-lane flip), 4 (Accelerate) -- are tie resolution,
        // not algorithm, and are not asserted. NO CEILING, because one cannot
        // fire: ssn_bulk_flips grows by at most one per pass and the first pass
        // cannot flip, so `iters == 7` asserted above already bounds it at 6.
        // `iters == 7` IS the under-damping guard here -- an engine that
        // oscillated on this cell would spend passes it does not have, the way
        // the cycling fixture in this file does at its budget -- and a flip
        // ceiling would only restate it, less directly and on a tie-decided
        // counter.
        EXPECT_GE(res.counters.ssn_bulk_flips, 1);
    }

    // The coin flip, caught: from x0 = (6, -5) bare mode reports the weakly
    // active row ACTIVE and the safeguarded engine reports it inactive AND
    // uncertain. Both are "correct" readings of a tie; only one of them says so.
    //
    // ON ACCELERATE BOTH COINS LAND THE OTHER WAY (M3-4): bare reads the row
    // inactive and the safeguarded engine reads it active. The contrast
    // survives THAT swap -- but it does not survive the within-lane flip, which
    // is the failure mode this fixture actually suffers: the L-1 register's
    // occurrence-3 table records this very comparison failing "true vs true".
    // So NOTHING about the two readings is asserted here. They are RECORDED
    // (RecordProperty, visible in the ctest XML) so the cell still reports what
    // it saw, and only the two properties that hold on either side of either
    // coin are checked.
    {
        SsnStart start;
        start.x = Vec(2);
        start.x << 6.0, -5.0;

        SsnEngine bare_engine(default_opts());
        SsnResult bare;
        bare_engine.solve(qp, start, bare_opts(), &bare);
        EXPECT_EQ(bare.status, QpStatus::kOptimal);

        SsnEngine full_engine(default_opts());
        SsnResult full;
        full_engine.solve(qp, start, SsnOptions{}, &full);
        EXPECT_EQ(full.status, QpStatus::kOptimal);

        // RECORDED, NOT ASSERTED -- see this cell's note. Whether the two modes
        // read the tie differently is itself a coin, and the L-1 register has
        // it landing both ways within one lane.
        RecordProperty("bare_ineq_active_1", bare.ineq_active[1] ? 1 : 0);
        RecordProperty("full_ineq_active_1", full.ineq_active[1] ? 1 : 0);
        RecordProperty("full_ineq_uncertain_1", full.ineq_uncertain[1] ? 1 : 0);
        RecordProperty("full_ssn_bulk_flips", static_cast<int>(full.counters.ssn_bulk_flips));
        // The safeguarded engine owes the same never-inactive-and-certain
        // property from this start as from the cold one.
        EXPECT_TRUE(full.ineq_active[1] || full.ineq_uncertain[1])
            << "the tie was returned inactive AND certain by the safeguarded engine";
        // Bare mode's uncertain flag is STRUCTURALLY false on both backends and
        // on either side of the coin -- bare mode has no third set to report a
        // row uncertain in -- so this one is a property, not a direction.
        EXPECT_FALSE(bare.ineq_uncertain[1]);
    }
}

// =============================================================================
// TASK-4 (c): AN INFEASIBLE QP IS DIAGNOSED, NEVER CERTIFIED
// =============================================================================
//
// The three clauses of the brief, and one more that matters as much: the
// diagnosis must not depend on the budget. Bare mode spends every step it is
// given and reports kBudget -- which a caller answers by raising the budget,
// which is exactly the wrong move -- while the safeguarded engine reaches the
// same verdict at 7 accepted steps whether the budget is 25 or 100.
TEST(SsnEngineLocal, InfeasibleQpIsSuspectedNotCertified) {
    const QpProblem qp = contradictory_qp();
    // The walk agrees, independently, that this QP has no feasible point.
    EXPECT_EQ(walk_solution(qp).status, QpStatus::kInfeasible);

    for (const Index budget : {25, 100}) {
        SsnOptions sopts;
        sopts.hard_budget = budget;

        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, SsnStart{}, sopts, &res);
        EXPECT_EQ(res.status, QpStatus::kInfeasible) << "budget " << budget;
        EXPECT_EQ(res.escape_reason, SsnEscape::kInfeasibleSuspect) << "budget " << budget;
        EXPECT_NE(res.status, QpStatus::kOptimal) << "budget " << budget;
        EXPECT_EQ(res.iters, 7) << "budget " << budget;
        EXPECT_LT(res.iters, budget) << "budget " << budget; // inside hard_budget
        EXPECT_EQ(res.counters.ssn_escapes, 1) << "budget " << budget;
        EXPECT_FALSE(res.escape_detail.empty()) << "budget " << budget;
        // The residual it stopped at is the infeasibility itself, not noise.
        EXPECT_NEAR(res.fb_residual, 0.4, 1e-3) << "budget " << budget;

        // The positive control: without the telemetry the same QP looks like a
        // budget problem, at every budget.
        SsnOptions bare = bare_opts();
        bare.hard_budget = budget;
        SsnEngine bare_engine(default_opts());
        SsnResult bare_res;
        bare_engine.solve(qp, SsnStart{}, bare, &bare_res);
        EXPECT_EQ(bare_res.escape_reason, SsnEscape::kBudget) << "budget " << budget;
        EXPECT_EQ(bare_res.iters, budget) << "budget " << budget;
    }
}

// =============================================================================
// TASK-4 (c), CONTINUED: BOTH ROUTES INTO THE DIAGNOSIS, AND BOTH SHAPES
// =============================================================================
//
// The telemetry has two entry points and they answer different questions, so
// each needs a fixture that reaches it and no other. Without this test either
// route can be deleted and the suite stays green on the strength of the other
// -- which is exactly what the first mutation sweep measured (S3 and S3b both
// survived; this test is what kills them).
//
// The two ROUTES:
//   * STANDING   -- the stall window fills while the duals diverge, on an
//                   iteration that keeps ACCEPTING steps (slow_infeasible_qp);
//   * EXHAUSTION -- the Armijo schedule finds no descent at all while the duals
//                   have already diverged (contradictory_qp, and the
//                   inconsistent equality block).
//
// The two SHAPES: contradictory INEQUALITY rows, where the FB complementarity
// mechanism is what diverges, and an inconsistent EQUALITY block, where F is
// AFFINE and there is no FB row in the problem at all. An SQP linearization
// produces the second shape far more often than the first.
TEST(SsnEngineLocal, InfeasibilityIsReachedByBothRoutesAndOnBothShapes) {
    struct Cell {
        const char *name;
        QpProblem qp;
        const char *route; // a phrase unique to that route's message
        Index full_iters;
        double stop_residual;
    };
    // ============ MKL-OBSERVED (review fix round 1, I2). ============ Both
    // routes' conjuncts were reworked so a FEASIBLE badly-scaled or
    // proximally damped trajectory cannot reach them, and the counts below are
    // that rework's re-derivation, from
    // `./build/bench/hven_sqp_ssn_safeguard_probe trace`:
    //   * contradictory_qp   7 accepted steps (unmoved), diagnosed on the
    //     exhaustion route at attempt 7 -- its duals multiply by 154 on the
    //     last accepted step, which is the new per-step conjunct;
    //   * inconsistent_equality_qp  1 step (unmoved), 5.0e7x on that one step;
    //   * slow_infeasible_qp 13 -> 14, because the standing route now measures
    //     improvement over a WINDOW rather than per step and re-arms its growth
    //     reference with it: the verdict lands one accepted step later and at
    //     the residual floor 1.3605 rather than at 1.3347 on the way down.
    Cell cells[] = {
        {"contradictory rows", contradictory_qp(), "found no descent at all", 7, 0.4},
        {"inconsistent equalities", inconsistent_equality_qp(), "found no descent at all", 1, 0.5},
        {"slow infeasible", slow_infeasible_qp(), "accepted steps that did not improve", 14,
         1.3605},
    };
    for (Cell &c : cells) {
        EXPECT_EQ(walk_solution(c.qp).status, QpStatus::kInfeasible) << c.name;

        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(c.qp, SsnStart{}, SsnOptions{}, &res);
        EXPECT_EQ(res.status, QpStatus::kInfeasible) << c.name;
        EXPECT_EQ(res.escape_reason, SsnEscape::kInfeasibleSuspect) << c.name;
        EXPECT_EQ(res.iters, c.full_iters) << c.name;
        EXPECT_NEAR(res.fb_residual, c.stop_residual, 1e-3) << c.name;
        EXPECT_NE(res.escape_detail.find(c.route), std::string::npos)
            << c.name << " took the wrong route: " << res.escape_detail;

        // And bare mode reads every one of them as a budget problem.
        SsnEngine bare_engine(default_opts());
        SsnResult bare;
        bare_engine.solve(c.qp, SsnStart{}, bare_opts(), &bare);
        EXPECT_EQ(bare.escape_reason, SsnEscape::kBudget) << c.name;
        EXPECT_EQ(bare.iters, SsnOptions{}.hard_budget) << c.name;
    }
}

// =============================================================================
// THE OTHER DIRECTION: A FEASIBLE QP MUST NEVER BE REPORTED INFEASIBLE
// (review fix round 1, I2; Fable review F2)
// =============================================================================
//
// **kInfeasible IS THE STRONGEST STATUS THIS FILE CAN RETURN AND THE ONLY ONE A
// DRIVER ANSWERS WITH RESTORATION RATHER THAN A BUDGET BUMP**, so its false
// positives are worth more than its false negatives. The first implementation
// had it backwards on all three of the conjunct's degrees of freedom, and this
// fixture is what the rework is scored on: a QP the WALK solves in two minor
// iterations, on which the pre-fix telemetry reported kInfeasibleSuspect at 4
// attempts because its true multipliers (1.5e6, 1.2e6) sit two orders above the
// growth threshold and never come back below it.
//
// Measured over a 20 000-QP census of the same shape (the fixture's own
// generator, bench/ssn_safeguard_probe.cpp), pre-fix against post-fix:
//   * FALSE kInfeasible on feasible QPs:  48.3%  ->  0.0%   (and 58.9% -> 0.0%
//     with the objective scaled a further 1e3);
//   * escapes of any kind:                9 794  ->  4 664 -- the false
//     diagnosis was PRE-EMPTING the proximal ladder that solves these cells,
//     so removing it converts 5 130 escapes into solves;
//   * and the price, on a 30 000-QP census of genuinely INFEASIBLE QPs:
//     diagnosed 99.7% -> 56.5%. The undiagnosed ones exit kNoContraction or
//     kBudget, which Task 5 routes to the walk exactly as it routes a
//     diagnosis -- a slower answer, never a wrong one.
// ============ MKL-OBSERVED. NOT RE-VERIFIED ON APPLE ACCELERATE. ============
TEST(SsnEngineLocal, FeasibleButBadlyScaledIsNeverReportedInfeasible) {
    const QpProblem qp = badly_scaled_feasible_qp();

    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal) << "the fixture must be FEASIBLE";
    ASSERT_GT(walk.lambda_i.cwiseAbs().maxCoeff(), 1e5)
        << "and its multipliers must clear the growth threshold, or it proves nothing";

    SsnEngine engine(default_opts());
    SsnResult res;
    engine.solve(qp, SsnStart{}, SsnOptions{}, &res);

    // The assertion the finding is about, stated as strongly as it can be.
    EXPECT_NE(res.status, QpStatus::kInfeasible);
    EXPECT_NE(res.escape_reason, SsnEscape::kInfeasibleSuspect);

    // And the shipped conjuncts do better than merely not lying: the solve
    // converges, because the diagnosis is no longer pre-empting the ladder.
    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.escape_reason, SsnEscape::kNone);
    EXPECT_EQ(res.iters, 8);
    // 8 accepted steps + 2 attempts that escalated instead of stepping + the
    // certificate's own. This is the one benign-ish cell where
    // certified_factorizations() does NOT apply, and it is why the invariant is
    // an inequality: iters <= factorizations, never an equality under kFull.
    EXPECT_EQ(res.factorizations, 11);
    EXPECT_LT(res.iters, res.factorizations);
    EXPECT_EQ(res.counters.ssn_prox_updates, 2) << "the ladder is what solves this cell";
    EXPECT_GT(res.counters.ssn_backtracks, 0);
    for (Index j = 0; j < qp.n(); ++j) {
        EXPECT_NEAR(res.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
    }
}

// The two second-line guards on the stall window (review fix round 1, I2), and
// the one cell in this file that can see them. Everything gentler is already
// covered by the windowed improvement demand and the re-armed growth reference,
// which is exactly why these two needed a fixture of their own: without one,
// removing either passes the whole suite.
// ============ MKL-OBSERVED. NOT RE-VERIFIED ON APPLE ACCELERATE. ============
TEST(SsnEngineLocal, StallWindowGuardsKeepABrutallyScaledFeasibleQpHonest) {
    const QpProblem qp = brutally_scaled_feasible_qp();

    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal) << "the fixture must be FEASIBLE";

    SsnEngine engine(default_opts());
    SsnResult res;
    engine.solve(qp, SsnStart{}, SsnOptions{}, &res);

    // The assertion: whatever this kernel does with a QP this badly scaled, it
    // may not claim the QP has no solution.
    EXPECT_NE(res.status, QpStatus::kInfeasible);
    EXPECT_NE(res.escape_reason, SsnEscape::kInfeasibleSuspect);
    // And what it does instead, pinned so the honest outcome is a pin too:
    // it runs out of descent and says so, which Task 5 routes to the walk.
    EXPECT_EQ(res.escape_reason, SsnEscape::kNoContraction);
    EXPECT_EQ(res.iters, 5);
    EXPECT_EQ(res.factorizations, 13);
}

// The verification attempt is NOT a step, so it may not move a step counter --
// and this is the fixture that can tell (review fix round 1, C1). Its certified
// point classifies differently from the partition its last accepted step was
// taken under, so counting the verification's classification would report one
// bulk flip too many.
TEST(SsnEngineLocal, TheVerificationAttemptMovesNoStepCounter) {
    const QpProblem qp = late_reclassification_qp();

    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);

    SsnEngine engine(default_opts());
    SsnResult res;
    engine.solve(qp, SsnStart{}, SsnOptions{}, &res);

    ASSERT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.iters, 9);
    EXPECT_EQ(res.factorizations, certified_factorizations(res.iters));
    // ===== THE PIN: 4 flips over 9 steps, not 5 over 9 steps + a verification.
    EXPECT_EQ(res.counters.ssn_bulk_flips, 4);
    EXPECT_EQ(res.counters.ssn_uncertain_peak, 0);
    for (Index j = 0; j < qp.n(); ++j) {
        EXPECT_NEAR(res.x(j), walk.x(j), 1e-6) << "x(" << j << ")";
    }
}

// =============================================================================
// TASK-4 (d): THE BUDGET MACHINERY, AND TRUTHFUL COUNTERS
// =============================================================================
TEST(SsnEngineLocal, BudgetsAreEnforcedAndCountersAreTruthful) {
    // hard_budget: a cell forced past its budget stops with kBudget, and the
    // counters describe what was actually paid.
    {
        SsnOptions sopts;
        sopts.hard_budget = 3;
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(cycling_qp_3var(), SsnStart{}, sopts, &res);
        EXPECT_EQ(res.status, QpStatus::kMaxIter);
        EXPECT_EQ(res.escape_reason, SsnEscape::kBudget);
        EXPECT_EQ(res.counters.ssn_iters, res.iters);
        EXPECT_LE(res.iters, sopts.hard_budget);
        EXPECT_LE(res.factorizations, sopts.hard_budget);
        EXPECT_LE(res.iters, res.factorizations); // the Task-4 invariant
        EXPECT_EQ(res.counters.ssn_escapes, 1);
    }

    // soft_budget: crossing it ARMS the proximal term, which is the whole of
    // the "warns via counter" contract -- ssn_prox_updates leaves zero, the
    // solve still converges, and the answer does not move.
    {
        const QpProblem qp = weakly_active_qp();
        SsnEngine relaxed_engine(default_opts());
        SsnResult relaxed;
        relaxed_engine.solve(qp, SsnStart{}, SsnOptions{}, &relaxed);
        ASSERT_EQ(relaxed.status, QpStatus::kOptimal);
        EXPECT_EQ(relaxed.counters.ssn_prox_updates, 0); // 7 steps, budget 12
        EXPECT_EQ(relaxed.prox_sigma, 0.0);

        SsnOptions tight;
        tight.soft_budget = 2;
        SsnEngine tight_engine(default_opts());
        SsnResult armed;
        tight_engine.solve(qp, SsnStart{}, tight, &armed);
        EXPECT_EQ(armed.status, QpStatus::kOptimal);
        EXPECT_EQ(armed.counters.ssn_prox_updates, 1);
        EXPECT_GT(armed.prox_sigma, 0.0);
        EXPECT_NEAR(armed.x(0), relaxed.x(0), 1e-6);
        EXPECT_NEAR(armed.x(1), relaxed.x(1), 1e-6);
    }
}

// =============================================================================
// THE LINE SEARCH, AND ITS ITERATION-0 EXEMPTION -- BOTH HINT POLARITIES
// =============================================================================
//
// The mechanism the Fable kernel review's O5 identified, asserted from both
// ends. A WRONG hint's first step RAISES the residual by a factor of 5 on this
// fixture; a monotone Armijo rule would reject it, backtrack to the floor, and
// escape. A CORRECT hint's first step lands at the solution and would pass the
// test anyway, so the exemption gives up nothing where it matters.
//
// **THIS IS WHAT GATE G1 IS SCORED ON.** The one-iteration correct-hint payoff
// is the warm-start claim of the whole phase; a safeguard that destroyed it
// would be a safeguard that lost the phase.
TEST(SsnEngineLocal, LineSearchExemptsAHintedFirstStepInBothPolarities) {
    const QpProblem qp = two_row_qp();

    // --- the wrong polarity: the exempted step raises ||F|| 1.0 -> 5.0 ------
    SsnStart wrong;
    wrong.x = Vec(2);
    wrong.x << 1.0, 1.0;
    wrong.activity_hint.ineq = {false, true};

    {
        SsnOptions seed_only;
        seed_only.hard_budget = 0;
        SsnEngine engine(default_opts());
        SsnResult at_seed;
        engine.solve(qp, wrong, seed_only, &at_seed);
        EXPECT_NEAR(at_seed.fb_residual, 1.0, 1e-12);

        SsnOptions one_step;
        one_step.hard_budget = 1;
        SsnEngine engine2(default_opts());
        SsnResult after;
        engine2.solve(qp, wrong, one_step, &after);
        EXPECT_EQ(after.iters, 1); // taken, not rejected
        EXPECT_EQ(after.counters.ssn_backtracks, 0);
        // **AND IT RAISED THE RESIDUAL FOURFOLD.** The kernel review measured
        // 5.0 here; that figure is bare mode's, and it is still exactly 5.0
        // (asserted below). Under the shipped safeguards the DUAL PROJECTION
        // clips the wrongly hinted row's multiplier from -2.5 to 0 before the
        // residual is measured, and phi(-2, 0) = -4 rather than phi(-2, -2.5)
        // = -5. Either way the step is a fourfold-or-worse INCREASE that a
        // monotone Armijo rule would reject, which is the whole point.
        EXPECT_NEAR(after.fb_residual, 4.0, 1e-6);

        SsnOptions bare_step = bare_opts();
        bare_step.hard_budget = 1;
        SsnEngine engine3(default_opts());
        SsnResult bare_after;
        engine3.solve(qp, wrong, bare_step, &bare_after);
        EXPECT_NEAR(bare_after.fb_residual, 5.0, 1e-6); // the review's figure
    }

    // The whole solve still converges, and to the right answer.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, wrong, SsnOptions{}, &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 6);
        EXPECT_NEAR(res.x(0), 1.0, 1e-6);
        EXPECT_NEAR(res.x(1), 1.0, 1e-6);
    }

    // --- the correct polarity: one iteration, and no backtrack at all ------
    {
        SsnStart right;
        right.x = Vec(2);
        right.x << 1.0, 1.0;
        right.activity_hint.ineq = {true, false};
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, right, SsnOptions{}, &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 1);
        EXPECT_EQ(res.counters.ssn_backtracks, 0);
        // One step, and the certificate's own factorization beside it.
        EXPECT_EQ(res.factorizations, certified_factorizations(res.iters));
    }

    // --- and an UNHINTED first step is NOT exempt --------------------------
    //
    // The exemption is as narrow as the mechanism: iteration 0 AND a hint. A
    // cold first step is an ordinary FB Newton step and is line-searched like
    // every other one, which on this fixture costs four backtracks and one
    // extra iteration against bare mode's undamped six.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(qp, SsnStart{}, SsnOptions{}, &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 7);
        EXPECT_EQ(res.counters.ssn_backtracks, 4);

        SsnEngine bare_engine(default_opts());
        SsnResult bare;
        bare_engine.solve(qp, SsnStart{}, bare_opts(), &bare);
        EXPECT_EQ(bare.iters, 6);
        EXPECT_EQ(bare.counters.ssn_backtracks, 0);
    }
}

// =============================================================================
// THE DUAL PROJECTION
// =============================================================================
//
// The kernel review's one available mitigation for the wrong-hint tax (its O5;
// no derivative re-selection can help, because the landing configuration is a
// DIFFERENTIABLE point of phi). Two things are asserted: the invariant it
// establishes -- the returned inequality and bound multipliers are non-negative
// on every route, including an escaped one -- and its measured effect.
//
// **MEASURED, AND IT IS A UNIFORM WIN**: over the eleven fixture/start cells of
// docs/notes/2026-08-07-ssn-safeguards.md section 4, the projection reduces or
// matches the accepted-step count on every one and increases it on none
// (cycling 2-var 11 -> 9, cycling 3-var 14 -> 11, box50 scrambled 8 -> 7,
// box400 cold 7 -> 6, mixed cold 9 -> 8). The two cells pinned below are the
// ones whose counts this file also pins elsewhere.
TEST(SsnEngineLocal, DualProjectionKeepsMultipliersFeasible) {
    struct Cell {
        const char *name;
        QpProblem qp;
        SsnStart start;
    };
    Cell cells[] = {
        {"cycling 2var", cycling_qp_2var(), cycling_start_2var()},
        {"cycling 3var", cycling_qp_3var(), SsnStart{}},
        {"contradictory", contradictory_qp(), SsnStart{}},
        {"box50 scrambled", box_qp(50), SsnStart{}},
        {"mixed", mixed_block_qp(), SsnStart{}},
    };
    for (Cell &c : cells) {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(c.qp, c.start, SsnOptions{}, &res);
        for (Index k = 0; k < c.qp.mi(); ++k) {
            EXPECT_GE(res.lambda_i(k), 0.0) << c.name << " lambda_i(" << k << ")";
        }
        // z is the SIGNED recombination of two non-negative bound-row
        // multipliers, so its sign is free -- what the projection guarantees is
        // that each SIDE is non-negative, which shows up as z agreeing with the
        // side the export says is active.
        for (Index j = 0; j < c.qp.n(); ++j) {
            if (res.bound_state[static_cast<std::size_t>(j)] == BoundState::kAtLower) {
                EXPECT_GE(res.z(j), 0.0) << c.name << " z(" << j << ")";
            }
            if (res.bound_state[static_cast<std::size_t>(j)] == BoundState::kAtUpper) {
                EXPECT_LE(res.z(j), 0.0) << c.name << " z(" << j << ")";
            }
        }
    }
}

// =============================================================================
// THE INERTIA GATE: A SADDLE POINT IS NOT AN ANSWER
// =============================================================================
//
// The sharpest safeguard in the set, because bare mode does not merely take
// longer here -- it returns kOptimal at a point that is not a minimizer, with a
// residual of 5e-9. F vanishes at every KKT point of the QP, so no
// residual-based test can tell the saddle (0.5, 0.125) from the solution
// (0.5, -1); only the FACTORIZATION knows, through its inertia.
//
// The safeguarded engine climbs the proximal ladder (which does make the
// primal block definite, at sigma = 100 -- the verdict flips to kOk there) and
// then finds that the FB merit has a stationary point that is not a root, which
// is the honest end of the road for a root-finding method on a nonconvex
// subproblem: it ESCAPES. Task 5 routes an escaped subproblem back to the walk,
// which solves this instance in 3 minor iterations by riding the negative
// curvature to the bound -- the division of labour the two kernels are for.
TEST(SsnEngineLocal, IndefiniteHessianEscapesRatherThanCertifyingASaddle) {
    const QpProblem qp = indefinite_qp();

    const QpSolution walk = walk_solution(qp);
    ASSERT_EQ(walk.status, QpStatus::kOptimal);
    EXPECT_NEAR(walk.x(0), 0.5, 1e-9);
    EXPECT_NEAR(walk.x(1), -1.0, 1e-9);

    // The positive control, and it is a WRONG ANSWER rather than a slow one.
    SsnEngine bare_engine(default_opts());
    SsnResult bare;
    bare_engine.solve(qp, SsnStart{}, bare_opts(), &bare);
    EXPECT_EQ(bare.status, QpStatus::kOptimal);
    EXPECT_EQ(bare.iters, 1);
    EXPECT_LT(bare.fb_residual, SsnOptions{}.fb_tol);
    EXPECT_NEAR(bare.x(0), 0.5, 1e-6);
    EXPECT_NEAR(bare.x(1), 0.125, 1e-6); // the saddle, certified
    const double bare_obj = 0.5 * (bare.x(0) * bare.x(0) - 2.0 * bare.x(1) * bare.x(1)) +
                            (-0.5 * bare.x(0) + 0.25 * bare.x(1));
    const double walk_obj = 0.5 * (walk.x(0) * walk.x(0) - 2.0 * walk.x(1) * walk.x(1)) +
                            (-0.5 * walk.x(0) + 0.25 * walk.x(1));
    EXPECT_GT(bare_obj, walk_obj + 1.0); // -0.109 against -1.375

    // The safeguarded engine refuses.
    SsnEngine engine(default_opts());
    SsnResult res;
    engine.solve(qp, SsnStart{}, SsnOptions{}, &res);
    EXPECT_NE(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.escape_reason, SsnEscape::kNoContraction);
    EXPECT_FALSE(res.escape_detail.empty());
    EXPECT_EQ(res.counters.ssn_escapes, 1);
    // The proximal ladder ran, all the way to its ceiling.
    EXPECT_GT(res.counters.ssn_prox_updates, 0);
    EXPECT_DOUBLE_EQ(res.prox_sigma, detail::kSsnProxMax);
    // And an attempt that paid a factorization without taking a step is
    // visible exactly where the contract says it is.
    EXPECT_LT(res.iters, res.factorizations);
}

// =============================================================================
// THE UNCERTAIN BAND'S HYSTERESIS AND ITS TIE POLICY
// =============================================================================
//
// The band is measured on |alpha - beta|, so its two endpoints are exact: a
// strictly active or strictly inactive row sits at 1 and a row on the kink ray
// sits at 0. That makes the enter/leave thresholds checkable arithmetic rather
// than a claim about a trajectory.
TEST(SsnEngineLocal, UncertainBandGeometryAndValidation) {
    // The geometry the constant is derived from: |alpha - beta| = sqrt(2) *
    // |sin(theta - pi/4)| for (s, lambda) = rho (cos theta, sin theta).
    for (double theta = -3.0; theta <= 3.0; theta += 0.25) {
        const double s = std::cos(theta);
        const double lam = std::sin(theta);
        const double rho = 1.0;
        const double a = 1.0 - s / rho;
        const double b = 1.0 - lam / rho;
        EXPECT_NEAR(std::abs(a - b), std::sqrt(2.0) * std::abs(std::sin(theta - M_PI / 4.0)), 1e-12)
            << "theta = " << theta;
    }
    // The pure states are at 1, so a threshold of 1 would swallow them -- which
    // is why the validation excludes it.
    EXPECT_NEAR(std::abs(1.0 - 0.0), 1.0, 1e-15); // strictly active (alpha, beta) = (1, 0)
    EXPECT_LT(detail::kSsnUncertainEnter * detail::kSsnUncertainLeaveRatio, 1.0);
    EXPECT_GT(detail::kSsnUncertainLeaveRatio, 1.0); // a band, not a point

    const QpProblem qp = two_row_qp();
    SsnEngine engine(default_opts());
    SsnResult res;
    SsnOptions bad;
    bad.uncertain_tol = 1.0;
    EXPECT_THROW(engine.solve(qp, SsnStart{}, bad, &res), std::invalid_argument);
    bad.uncertain_tol = -1e-16;
    EXPECT_THROW(engine.solve(qp, SsnStart{}, bad, &res), std::invalid_argument);
    bad = SsnOptions{};
    bad.prox_sigma_init = -1.0;
    EXPECT_THROW(engine.solve(qp, SsnStart{}, bad, &res), std::invalid_argument);

    // --- THE HYSTERESIS AND THE SYMMETRIC DAMPING, PINNED AT A WIDE BAND ---
    //
    // **THIS CELL RUNS AT uncertain_tol = 0.9, WHICH IS NOT A RECOMMENDED
    // SETTING**, and the reason is stated rather than hidden: at the shipped
    // 0.1 the band is too narrow for any fixture in this file to keep a row
    // inside it across steps, so neither the hysteresis nor the symmetry of the
    // damping is observable at all -- a mutation removing either one survives
    // the whole suite (Task-4 report, mutations S7 and S13). Measured over
    // 60 794 random cells at tau in {0.1, 0.3, 0.9}: ZERO differences at 0.1 and
    // 0.3, and over a thousand at 0.9. So the band has to be widened to see
    // them, and this is that measurement made into an assertion.
    //
    // What it shows, and it is the only positive evidence either constant has:
    // **the hysteresis buys PARTITION STABILITY.** With it, this solve makes
    // ONE bulk flip; without it, four. That is exactly what a hysteresis band is
    // for, and it is the Phase-6 Task-3 lesson in miniature (do not re-decide
    // the thing that just moved).
    //
    // **AND IT IS NOT A QUALITY CLAIM ABOUT tau = 0.9.** At this band the
    // shipped configuration runs out of budget on a QP it solves in 11 steps at
    // tau = 0.1 -- which is precisely why 0.1 is what ships and why
    // docs/notes/2026-08-07-ssn-safeguards.md section 4.1 records 0.5 as where
    // the band starts costing outcomes. This cell discriminates; it does not
    // recommend.
    // ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
    {
        SsnOptions wide;
        wide.uncertain_tol = 0.9;
        SsnEngine wide_engine(default_opts());
        SsnResult wide_res;
        wide_engine.solve(cycling_qp_3var(), SsnStart{}, wide, &wide_res);
        EXPECT_EQ(wide_res.status, QpStatus::kMaxIter);
        EXPECT_EQ(wide_res.counters.ssn_bulk_flips, 1); // 4 without the hysteresis
        EXPECT_EQ(wide_res.counters.ssn_uncertain_peak, 1);
        EXPECT_EQ(wide_res.counters.ssn_prox_updates, 4); // 0 without, 1 if damped on alpha only
        EXPECT_DOUBLE_EQ(wide_res.prox_sigma, 1.0);
        EXPECT_EQ(wide_res.iters, 22);
    }

    // uncertain_tol = 0 disables the set outright, INCLUDING for the exact tie
    // that a "<= 0" test would otherwise capture.
    SsnOptions off;
    off.uncertain_tol = 0.0;
    SsnEngine off_engine(default_opts());
    SsnResult off_res;
    off_engine.solve(weakly_active_qp(), SsnStart{}, off, &off_res);
    EXPECT_EQ(off_res.status, QpStatus::kOptimal);
    EXPECT_EQ(off_res.counters.ssn_uncertain_peak, 0);
    for (const bool u : off_res.ineq_uncertain) {
        EXPECT_FALSE(u);
    }
}

// =============================================================================
// THE SAFEGUARDS ARE FREE ON THE BENIGN FIXTURES
// =============================================================================
//
// The claim the whole default rests on: turning them on costs nothing where
// nothing is wrong. Asserted as an ENVELOPE rather than as equality, because
// the line search legitimately trades one extra accepted step for a shorter one
// on two of these cells (and the dual projection legitimately saves a step on
// three others) -- what must hold is that no benign fixture regresses
// materially, that the proximal ladder never arms, and that the answers agree.
TEST(SsnEngineLocal, SafeguardsAreFreeOnTheBenignFixtures) {
    struct Cell {
        const char *name;
        QpProblem qp;
        Index bare_iters;
        Index full_iters;
    };
    Cell cells[] = {
        {"two_row", two_row_qp(), 6, 7},
        {"box20", box_qp(20), 6, 6},
        {"box50", box_qp(50), 6, 6},
        {"box400", box_qp(400), 7, 6},
        {"equality", equality_only_qp(), 1, 1},
        {"mixed", mixed_block_qp(), 8, 8},
    };
    for (Cell &c : cells) {
        SsnEngine bare_engine(default_opts());
        SsnResult bare;
        bare_engine.solve(c.qp, SsnStart{}, bare_opts(), &bare);
        SsnEngine full_engine(default_opts());
        SsnResult full;
        full_engine.solve(c.qp, SsnStart{}, SsnOptions{}, &full);

        EXPECT_EQ(bare.status, QpStatus::kOptimal) << c.name;
        EXPECT_EQ(full.status, QpStatus::kOptimal) << c.name;
        EXPECT_EQ(bare.iters, c.bare_iters) << c.name;
        EXPECT_EQ(full.iters, c.full_iters) << c.name;
        EXPECT_LE(full.iters, bare.iters + 1) << c.name;
        // The escalation ladder never arms on a healthy solve -- which is what
        // makes soft_budget = 12 provably inert on all of them.
        EXPECT_EQ(full.counters.ssn_prox_updates, 0) << c.name;
        EXPECT_EQ(full.prox_sigma, 0.0) << c.name;
        EXPECT_EQ(full.factorizations, certified_factorizations(full.iters)) << c.name;
        for (Index j = 0; j < c.qp.n(); ++j) {
            EXPECT_NEAR(full.x(j), bare.x(j), 1e-6) << c.name << " x(" << j << ")";
        }
    }
}

// =============================================================================
// THE SAFEGUARD COUNTERS ARE LIVE
// =============================================================================
//
// This replaces Task 3's Task3LeavesTheSafeguardCountersAtZero, which existed
// to be deleted here. Each of the three counters is asserted NONZERO on a
// fixture that moves it, so a half-wired safeguard cannot land unnoticed --
// and each is asserted ZERO in bare mode by
// BareModeReproducesTheTask3Trajectories, which is the other half of the same
// guard.
// =====================================================================
// PHASE-7 TASK 6b (docket D6) -- THE ESCAPE-REASON CENSUS, AT THE KERNEL.
//
// Five of the six census buckets are written HERE, beside `ssn_escapes`, so
// that a bare-kernel probe reads the same distribution the driver aggregates.
// This asserts the two properties the census is only useful under: it
// partitions the total, and it names the RIGHT reason.
//
// (The sixth bucket, `ssn_escape_gate_refused`, has no SsnEscape value behind
// it and is driver-scale only; it is asserted in
// SqpDriverSsnMode.TheEscapeReasonCensusPartitionsTheEscapeCount.)
// =====================================================================
TEST(SsnEngineLocal, TheEscapeReasonCensusMatchesTheReportedReason) {
    const auto census_sum = [](const SsnCounters &c) {
        return c.ssn_escape_budget + c.ssn_escape_singular + c.ssn_escape_no_contraction +
               c.ssn_escape_infeasible_suspect + c.ssn_escape_indefinite +
               c.ssn_escape_gate_refused;
    };

    // kIndefinite -- the saddle-seeded indefinite fixture, whose escape reason
    // IndefiniteHessianEscapesRatherThanCertifyingASaddle already pins.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(indefinite_qp(), SsnStart{}, SsnOptions{}, &res);
        ASSERT_NE(res.escape_reason, SsnEscape::kNone) << "the fixture's own premise";
        EXPECT_EQ(res.counters.ssn_escapes, 1);
        EXPECT_EQ(census_sum(res.counters), 1) << "the census partitions the total";
        // The ladder climbs and exhausts here, so the reported reason is the
        // one the census must name -- read off the result rather than
        // hard-coded, so this stays a CONSISTENCY assertion if the fixture's
        // own reason is ever re-ruled.
        switch (res.escape_reason) {
        case SsnEscape::kBudget:
            EXPECT_EQ(res.counters.ssn_escape_budget, 1);
            break;
        case SsnEscape::kSingular:
            EXPECT_EQ(res.counters.ssn_escape_singular, 1);
            break;
        case SsnEscape::kNoContraction:
            EXPECT_EQ(res.counters.ssn_escape_no_contraction, 1);
            break;
        case SsnEscape::kInfeasibleSuspect:
            EXPECT_EQ(res.counters.ssn_escape_infeasible_suspect, 1);
            break;
        case SsnEscape::kIndefinite:
            EXPECT_EQ(res.counters.ssn_escape_indefinite, 1);
            break;
        case SsnEscape::kNone:
            FAIL() << "guarded by the ASSERT above";
        }
    }

    // kBudget -- a hard budget of 1 on a fixture that needs more than one step.
    {
        SsnEngine engine(default_opts());
        SsnOptions s;
        s.hard_budget = 1;
        SsnResult res;
        engine.solve(cycling_qp_3var(), SsnStart{}, s, &res);
        ASSERT_EQ(res.escape_reason, SsnEscape::kBudget) << "the fixture's own premise";
        EXPECT_EQ(res.counters.ssn_escape_budget, 1);
        EXPECT_EQ(census_sum(res.counters), res.counters.ssn_escapes);
    }

    // A CERTIFYING solve writes nothing at all -- the opposite error, and the
    // one that would make every corpus row look like a hand-off.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(cycling_qp_3var(), SsnStart{}, SsnOptions{}, &res);
        ASSERT_EQ(res.escape_reason, SsnEscape::kNone) << "the fixture's own premise";
        EXPECT_EQ(res.counters.ssn_escapes, 0);
        EXPECT_EQ(census_sum(res.counters), 0);
    }
}

TEST(SsnEngineLocal, SafeguardCountersAreLive) {
    // ssn_backtracks: the cycling fixture backtracks its way out of the orbit.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(cycling_qp_3var(), SsnStart{}, SsnOptions{}, &res);
        EXPECT_EQ(res.counters.ssn_backtracks, 16);
    }
    // ssn_prox_updates: the indefinite fixture climbs the whole ladder.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(indefinite_qp(), SsnStart{}, SsnOptions{}, &res);
        // **SEVEN RUNGS, NOT EIGHT** (review fix round 1, I1). The ladder is
        // 1e-6, 1e-4, 1e-2, 1, 1e2, 1e4, 1e6 -- and the eighth update this
        // used to pin was a FLOATING-POINT ACCIDENT, not a rung: six
        // multiplications by 100 land on 999999.9999999998, strictly below the
        // 1e6 cap, so an exact `>=` guard granted one more escalation that
        // raised sigma by 2.3e-10 relative and cost a numeric factorization
        // plus 13 backtracks. detail::kSsnProxCapSlack closes it, and the
        // ladder's own length is asserted rather than observed.
        EXPECT_EQ(res.counters.ssn_prox_updates, detail::kSsnProxRungs);
        EXPECT_EQ(res.counters.ssn_prox_updates, 7);
        // **AND THE TOP RUNG IS THE CAP EXACTLY, NOT THE LADDER'S ROUNDING OF
        // IT.** EXPECT_EQ, not EXPECT_DOUBLE_EQ: repeated multiplication lands
        // on 999999.9999999998, which is inside DOUBLE_EQ's 4-ULP tolerance and
        // therefore invisible to it -- and SsnResult::prox_sigma is a value a
        // caller reads back to restart a proximal sequence
        // (SsnOptions::prox_sigma_init), so "the documented 1e6" has to be the
        // documented 1e6.
        EXPECT_EQ(res.prox_sigma, detail::kSsnProxMax);
    }
    // **THE CEILING GUARD ITSELF**, which the rung count above cannot reach:
    // the ladder snaps its top rung onto the cap, so on any solve that CLIMBS
    // there the exact test and the slack test agree. A caller restarting a
    // proximal sequence through SsnOptions::prox_sigma_init is the case that
    // separates them -- a sigma a hair under the cap (the drifted value the
    // ladder used to produce, and exactly what a caller who read prox_sigma
    // back from an older build would hand in) must read as AT the ceiling, not
    // as one rung below it.
    {
        SsnEngine engine(default_opts());
        SsnOptions s;
        s.prox_sigma_init = detail::kSsnProxMax * (1.0 - 1e-15);
        SsnResult res;
        engine.solve(indefinite_qp(), SsnStart{}, s, &res);
        EXPECT_EQ(res.counters.ssn_prox_updates, 0)
            << "a sigma inside the cap's slack is the CEILING, not a rung below it";
        EXPECT_EQ(res.escape_reason, SsnEscape::kNoContraction);
    }
    // ssn_uncertain_peak: the weakly active row.
    {
        SsnEngine engine(default_opts());
        SsnResult res;
        engine.solve(weakly_active_qp(), SsnStart{}, SsnOptions{}, &res);
        EXPECT_EQ(res.counters.ssn_uncertain_peak, 1);
    }
}

// SqpOptions::qp_mode and SqpCounters::ssn land in sqp_types.h ahead of the
// driver that will read them (Task 5). Until then the default must be the
// walk and the aggregate must be zero -- this is the guard on the
// byte-identity invariant the task carries.
TEST(SsnEngineLocal, DriverSurfaceIsDeclaredButInert) {
    EXPECT_EQ(SqpOptions{}.qp_mode, QpMode::kWalk);
    const SqpCounters c;
    EXPECT_EQ(c.ssn.ssn_iters, 0);
    EXPECT_EQ(c.ssn.ssn_bulk_flips, 0);
    EXPECT_EQ(c.ssn.ssn_backtracks, 0);
    EXPECT_EQ(c.ssn.ssn_prox_updates, 0);
    EXPECT_EQ(c.ssn.ssn_escapes, 0);
    EXPECT_EQ(c.ssn.ssn_uncertain_peak, 0);
}

// =============================================================================
// PHASE-7 TASK 6b PHASE B -- R5: THE DEFERRED CERTIFICATION (Gould's lemma)
// =============================================================================
//
// The kernel half of the R5 lever. Three properties, and they are the three
// that make the driver-side saving sound rather than merely cheaper:
//
//   1. THE COST MOVES, THE ANSWER DOES NOT. A deferring solve pays exactly one
//      factorization fewer than the shipped one and returns the same point at
//      the same residual; closing the deferral pays that factorization and the
//      totals agree again. So the lever is a RELOCATION of the certificate's
//      cost, and the driver's saving comes from not needing the relocated copy
//      at all -- never from certifying less.
//   2. THE VERDICT IS THE SAME VERDICT. Seeded at indefinite_qp's saddle -- the
//      fixture section 7b exists for -- the deferred route WITHDRAWS the
//      certificate exactly as the in-loop route refuses it, with the same
//      escape reason and the same census bucket.
//   3. THE CONTRACT IS ENFORCED, NOT DOCUMENTED. A pending verdict is evidence
//      about a matrix; abandoning it is a caller error and throws, because the
//      alternative is a caller believing a certificate nothing ever verified.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, DeferredCertificationRelocatesExactlyOneFactorization) {
    const QpProblem qp = two_row_qp();

    SsnResult shipped;
    {
        SsnEngine engine(default_opts());
        engine.solve(qp, SsnStart{}, SsnOptions{}, &shipped);
    }
    ASSERT_EQ(shipped.status, QpStatus::kOptimal);
    ASSERT_FALSE(shipped.certification_deferred);
    EXPECT_EQ(shipped.factorizations, certified_factorizations(shipped.iters));

    SsnEngine engine(default_opts());
    SsnOptions sopts;
    sopts.defer_certification = true;
    SsnResult res;
    engine.solve(qp, SsnStart{}, sopts, &res);

    // The exit is provisional, and it is one factorization cheaper.
    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.escape_reason, SsnEscape::kNone);
    EXPECT_TRUE(res.certification_deferred);
    EXPECT_TRUE(engine.has_deferred_certification());
    EXPECT_EQ(res.factorizations, shipped.factorizations - 1);
    EXPECT_EQ(res.iters, shipped.iters);
    EXPECT_EQ(res.fb_residual, shipped.fb_residual);
    EXPECT_EQ((res.x - shipped.x).cwiseAbs().maxCoeff(), 0.0);
    EXPECT_EQ((res.lambda_i - shipped.lambda_i).cwiseAbs().maxCoeff(), 0.0);
    EXPECT_EQ(res.bound_state, shipped.bound_state);
    EXPECT_EQ(res.ineq_active, shipped.ineq_active);
    EXPECT_EQ(res.ineq_uncertain, shipped.ineq_uncertain);

    // Closing it pays the relocated factorization and reaches the same total.
    EXPECT_TRUE(engine.finish_deferred_certification(&res));
    EXPECT_FALSE(engine.has_deferred_certification());
    EXPECT_EQ(res.factorizations, shipped.factorizations);
    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_EQ(res.counters.ssn_escapes, 0);
}

// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, DeferredCertificationWithdrawsTheSaddleCertificate) {
    const QpProblem qp = indefinite_qp();
    SsnStart at_saddle;
    at_saddle.x = Vec(2);
    at_saddle.x << 0.5, 0.125;

    SsnResult shipped;
    {
        SsnEngine engine(default_opts());
        engine.solve(qp, at_saddle, SsnOptions{}, &shipped);
    }
    ASSERT_EQ(shipped.escape_reason, SsnEscape::kIndefinite) << "section 7b's own fixture";
    ASSERT_EQ(shipped.counters.ssn_escape_indefinite, 1);

    SsnEngine engine(default_opts());
    SsnOptions sopts;
    sopts.defer_certification = true;
    SsnResult res;
    engine.solve(qp, at_saddle, sopts, &res);
    // Provisionally certified -- which is exactly why the caller OWES the
    // closing call and may not simply read the status.
    EXPECT_EQ(res.status, QpStatus::kOptimal);
    EXPECT_TRUE(res.certification_deferred);
    EXPECT_EQ(res.counters.ssn_escapes, 0);

    EXPECT_FALSE(engine.finish_deferred_certification(&res));
    EXPECT_EQ(res.status, QpStatus::kNumericalError);
    EXPECT_EQ(res.escape_reason, SsnEscape::kIndefinite);
    EXPECT_EQ(res.counters.ssn_escapes, 1);
    EXPECT_EQ(res.counters.ssn_escape_indefinite, 1);
    EXPECT_EQ(res.factorizations, shipped.factorizations);
    EXPECT_NE(res.escape_detail.find("deferred verification"), std::string::npos);
}

// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, APendingDeferredCertificationMayNotBeAbandoned) {
    const QpProblem qp = two_row_qp();
    SsnOptions sopts;
    sopts.defer_certification = true;

    SsnEngine engine(default_opts());
    SsnResult res;
    engine.solve(qp, SsnStart{}, sopts, &res);
    ASSERT_TRUE(engine.has_deferred_certification());

    // A second solve would overwrite the matrix the pending verdict is about.
    EXPECT_THROW(engine.solve(qp, SsnStart{}, sopts, &res), std::invalid_argument);
    // Discarding is the named way out, and it is idempotent only in the sense
    // that a second discard is itself a caller error.
    EXPECT_NO_THROW(engine.discard_deferred_certification());
    EXPECT_FALSE(engine.has_deferred_certification());
    EXPECT_THROW(engine.discard_deferred_certification(), std::invalid_argument);
    EXPECT_THROW(engine.finish_deferred_certification(&res), std::invalid_argument);
    // And with nothing pending the engine solves again normally.
    EXPECT_NO_THROW(engine.solve(qp, SsnStart{}, sopts, &res));
    EXPECT_TRUE(engine.has_deferred_certification());
    engine.discard_deferred_certification();

    // An ESCAPED solve defers nothing, so the contract cannot be tripped by a
    // caller that only ever sees escapes.
    SsnOptions escaping = sopts;
    escaping.hard_budget = 1;
    SsnEngine e2(default_opts());
    SsnResult esc;
    e2.solve(cycling_qp_3var(), SsnStart{}, escaping, &esc);
    EXPECT_NE(esc.escape_reason, SsnEscape::kNone);
    EXPECT_FALSE(esc.certification_deferred);
    EXPECT_FALSE(e2.has_deferred_certification());

    // Under kBare the option is inert: bare mode has no verification to defer.
    SsnOptions bare_deferring = bare_opts();
    bare_deferring.defer_certification = true;
    SsnEngine e3(default_opts());
    SsnResult bres;
    e3.solve(qp, SsnStart{}, bare_deferring, &bres);
    EXPECT_EQ(bres.status, QpStatus::kOptimal);
    EXPECT_FALSE(bres.certification_deferred);
    EXPECT_FALSE(e3.has_deferred_certification());
}

// =============================================================================
// PHASE-7 TASK 6b PHASE B -- R1: THE RESIDUAL-DRIVEN (LEVENBERG-MARQUARDT) sigma
// =============================================================================
//
// Two properties, and they are the two the arm's reading depends on:
//
//   1. kResidualArmed IS PROVABLY INERT WHERE THE LADDER NEVER ARMS. Every
//      benign fixture reproduces the shipped trajectory BIT FOR BIT under it --
//      which is what makes its win on the ill-conditioned population
//      attributable to the ill-conditioning rather than to a global change of
//      step. kResidualAlways is asserted to be genuinely different in the same
//      breath, so "inert" cannot be an artefact of the option not being read.
//   2. THE LADDER SURVIVES UNDERNEATH AS A MONOTONE FLOOR. On the fixture whose
//      ladder climbs to the ceiling, the residual rule still reaches the
//      ceiling, still exhausts it, and still escapes with the SAME reason -- so
//      the lever changes the SIZE of the shift and not the escape routes.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, TheResidualSigmaRuleIsInertUntilTheLadderArms) {
    struct Cell {
        const char *name;
        QpProblem qp;
        SsnStart start;
    };
    std::vector<Cell> cells;
    cells.push_back({"two_row cold", two_row_qp(), SsnStart{}});
    cells.push_back({"box50 cold", box_qp(50), SsnStart{}});
    cells.push_back({"mixed cold", mixed_block_qp(), SsnStart{}});
    cells.push_back({"weakly active cold", weakly_active_qp(), SsnStart{}});
    {
        Cell c{"two_row correct hint", two_row_qp(), SsnStart{}};
        c.start.x = Vec(2);
        c.start.x << 1.0, 1.0;
        c.start.activity_hint.ineq = {true, false};
        cells.push_back(c);
    }

    Index differing_under_always = 0;
    for (const Cell &c : cells) {
        SCOPED_TRACE(c.name);
        SsnResult shipped;
        {
            SsnEngine e(default_opts());
            e.solve(c.qp, c.start, SsnOptions{}, &shipped);
        }
        ASSERT_EQ(shipped.status, QpStatus::kOptimal);
        ASSERT_EQ(shipped.counters.ssn_prox_updates, 0) << "the premise: the ladder never armed";

        SsnOptions armed;
        armed.sigma_rule = SsnSigmaRule::kResidualArmed;
        SsnEngine e2(default_opts());
        SsnResult res;
        e2.solve(c.qp, c.start, armed, &res);
        EXPECT_EQ(res.iters, shipped.iters);
        EXPECT_EQ(res.factorizations, shipped.factorizations);
        EXPECT_EQ(res.counters.ssn_backtracks, shipped.counters.ssn_backtracks);
        EXPECT_EQ(res.counters.ssn_prox_updates, 0);
        EXPECT_EQ(res.prox_sigma, 0.0);
        EXPECT_EQ((res.x - shipped.x).cwiseAbs().maxCoeff(), 0.0);

        SsnOptions always;
        always.sigma_rule = SsnSigmaRule::kResidualAlways;
        SsnEngine e3(default_opts());
        SsnResult ares;
        e3.solve(c.qp, c.start, always, &ares);
        EXPECT_GT(ares.counters.ssn_prox_updates, 0) << "kResidualAlways is never inert";
        differing_under_always += ares.factorizations != shipped.factorizations ? 1 : 0;
    }
    EXPECT_GT(differing_under_always, 0)
        << "if NO cell moved under kResidualAlways the option is not being read at all";
}

// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, TheResidualSigmaRuleKeepsTheLadderAsAMonotoneFloor) {
    const QpProblem qp = indefinite_qp();
    SsnResult shipped;
    {
        SsnEngine e(default_opts());
        e.solve(qp, SsnStart{}, SsnOptions{}, &shipped);
    }
    ASSERT_EQ(shipped.escape_reason, SsnEscape::kNoContraction);
    ASSERT_DOUBLE_EQ(shipped.prox_sigma, detail::kSsnProxMax)
        << "the premise: the ladder topped out";

    for (const SsnSigmaRule rule : {SsnSigmaRule::kResidualArmed, SsnSigmaRule::kResidualAlways}) {
        SsnOptions s;
        s.sigma_rule = rule;
        SsnEngine e(default_opts());
        SsnResult res;
        e.solve(qp, SsnStart{}, s, &res);
        // The ROUTE is unchanged: the ladder still climbs, still tops out, and
        // still exhausts. Only the size of the shift between rungs can move.
        EXPECT_EQ(res.escape_reason, SsnEscape::kNoContraction);
        EXPECT_DOUBLE_EQ(res.prox_sigma, detail::kSsnProxMax);
        EXPECT_EQ(res.counters.ssn_prox_updates >= detail::kSsnProxRungs, true)
            << "every rung is still climbed";
    }

    // --- AND THE NORMALIZATION IS READ, on a fixture scaled hard enough to
    // --- make it visible ------------------------------------------------
    //
    // brutally_scaled_feasible_qp's objective is scaled 1e9, so its START
    // residual is ~1e9 too. r_k = ||F_k||inf / max(1, ||F_0||inf) is therefore
    // EXACTLY 1 at the seed, so the rule's own first answer is sigma = c = 1.0
    // -- a number that does not depend on the problem's scaling at all, which
    // is what normalizing is for. Pin the normalization away (r_k =
    // ||F_k||inf raw) and min(r, r^2) is ~1e9 on the first attempt, i.e.
    // clamped straight to the 1e6 ceiling: a different rule, six orders out.
    {
        // ONE ATTEMPT, ladder armed at attempt 0, and read the sigma it left.
        // r_0 = ||F_0||inf / max(1, ||F_0||inf) is EXACTLY 1 on any fixture
        // whose start residual exceeds 1, so min(r, r^2) = 1 and the rule's
        // own answer is sigma = c = 1.0 to the digit -- independent of the
        // problem's scaling, which is the whole point of normalizing.
        SsnOptions s;
        s.sigma_rule = SsnSigmaRule::kResidualArmed;
        s.soft_budget = 0;
        s.hard_budget = 1;
        SsnEngine e(default_opts());
        SsnResult res;
        e.solve(brutally_scaled_feasible_qp(), SsnStart{}, s, &res);
        EXPECT_DOUBLE_EQ(res.prox_sigma, detail::kSsnLmSigmaC)
            << "without the normalization this fixture's 1e9 residual clamps straight to the "
               "1e6 ceiling instead";

        // The shipped ladder answers the SAME question from its own history
        // instead: it arms at kSsnProxInit and, when the attempt's line search
        // finds no descent, climbs ONE rung -- four orders below the size the
        // residual asks for, which is the whole of R1's mechanism in two
        // numbers.
        SsnOptions ladder = s;
        ladder.sigma_rule = SsnSigmaRule::kLadder;
        SsnEngine e2(default_opts());
        SsnResult lres;
        e2.solve(brutally_scaled_feasible_qp(), SsnStart{}, ladder, &lres);
        EXPECT_DOUBLE_EQ(lres.prox_sigma, detail::kSsnProxInit * detail::kSsnProxGrowth);
    }
}

// =============================================================================
// PHASE-7 TASK 6b PHASE B -- R2: THE WATCHDOG
// =============================================================================
//
// The rule has to do two things at once, and this asserts both on the SAME
// fixture the shipped exemption is derived on:
//   * ON A CORRECT HINT it must be INVISIBLE. The one-iteration warm-start
//      payoff is the phase's whole claim, and a safeguard that spent even one
//      factorization there would have lost it.
//   * ON A WRONG HINT it must actually RETURN. The exemption's failure mode is
//      a first step that is both accepted and bad; the watchdog's answer is the
//      stored best point, and the counter that says so is watchdog_returns.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, TheWatchdogIsInvisibleOnACorrectHintAndReturnsOnAWrongOne) {
    const QpProblem qp = two_row_qp();

    SsnStart right;
    right.x = Vec(2);
    right.x << 1.0, 1.0;
    right.activity_hint.ineq = {true, false};

    SsnStart wrong;
    wrong.x = Vec(2);
    wrong.x << 1.0, 1.0;
    wrong.activity_hint.ineq = {false, true};

    for (const Index q : {Index{1}, Index{2}}) {
        SCOPED_TRACE(fmt::format("q = {}", q));
        SsnOptions wd;
        wd.hint_rule = SsnHintRule::kWatchdog;
        wd.watchdog_q = q;

        // --- the correct polarity: identical to the exemption, to the unit ---
        SsnEngine e(default_opts());
        SsnResult res;
        e.solve(qp, right, wd, &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.iters, 1);
        EXPECT_EQ(res.factorizations, certified_factorizations(1));
        EXPECT_EQ(res.counters.ssn_backtracks, 0);
        EXPECT_EQ(res.watchdog_returns, 0)
            << "a hint that works must never reach the watchdog's judgement";

        // --- the wrong polarity: the same answer, reached differently -------
        SsnEngine e2(default_opts());
        SsnResult wres;
        e2.solve(qp, wrong, wd, &wres);
        EXPECT_EQ(wres.status, QpStatus::kOptimal);
        EXPECT_NEAR(wres.x(0), 1.0, 1e-6);
        EXPECT_NEAR(wres.x(1), 1.0, 1e-6);
        // **q DECIDES WHETHER IT RETURNS AT ALL, AND THE RETURN IS FREE HERE**
        // -- both observed, both pinned rather than smoothed over. This
        // fixture's wrongly hinted first step raises ||F|| 1.0 -> 4.0. At
        // q = 1 the window is judged one step later, the recovery has not
        // landed, the watchdog restores the seed -- and the solve still
        // finishes in the SHIPPED 6 steps / 7 factorizations, because the
        // return costs no factorization (the verdict is taken before the
        // factorization, not after) and the restored point is the seed the
        // rejected step came from. At q = 2 the recovery is inside the window,
        // the window closes on its own, and the watchdog is invisible.
        //
        // So on this cell the safeguard is FREE in both configurations. That is
        // NOT the general finding -- the arm's population reading moves in both
        // directions and is what the report is scored on -- and pinning the
        // cheap cell here is exactly what makes the expensive ones in the
        // report a measurement rather than an impression.
        EXPECT_EQ(wres.watchdog_returns, q == 1 ? 1 : 0);
        EXPECT_EQ(wres.iters, 6);
        EXPECT_EQ(wres.factorizations, certified_factorizations(6));
    }

    // --- AND THE RESTORE ITSELF, PINNED ON THE CELL THAT SEPARATES IT -----
    //
    // The two cells above are cheap because the point the watchdog restores
    // happens to be the point the rejected step came from, so "close the
    // window" and "close the window AND go back" reach the same iterate. This
    // cell is the probe's own `two_row wrong hint` -- the SAME QP and hint from
    // the COLD primal (x = 0) rather than from x = (1, 1) -- on which they do
    // NOT: the relaxed step lands somewhere the iteration would otherwise keep,
    // and returning to the stored best point costs two attempts and reaches a
    // different trajectory (9 / 8 against the shipped 7 / 6).
    //
    // It is here because a watchdog that closes its window without returning is
    // not a watchdog at all -- it is the shipped exemption with a counter -- and
    // nothing else in the suite can tell the two apart.
    {
        SsnStart cold_wrong;
        cold_wrong.activity_hint.ineq = {false, true};

        SsnResult shipped;
        {
            SsnEngine e(default_opts());
            e.solve(qp, cold_wrong, SsnOptions{}, &shipped);
        }
        EXPECT_EQ(shipped.iters, 6);
        EXPECT_EQ(shipped.factorizations, certified_factorizations(6));

        SsnOptions wd;
        wd.hint_rule = SsnHintRule::kWatchdog;
        wd.watchdog_q = 1;
        SsnEngine e(default_opts());
        SsnResult res;
        e.solve(qp, cold_wrong, wd, &res);
        EXPECT_EQ(res.status, QpStatus::kOptimal);
        EXPECT_EQ(res.watchdog_returns, 1);
        EXPECT_EQ(res.iters, 8);
        EXPECT_EQ(res.factorizations, certified_factorizations(8));
        EXPECT_NEAR(res.x(0), 1.0, 1e-6);
        EXPECT_NEAR(res.x(1), 1.0, 1e-6);
    }

    // The shipped rule never sets the instrument, on either polarity.
    for (const SsnStart &st : {right, wrong}) {
        SsnEngine e(default_opts());
        SsnResult res;
        e.solve(qp, st, SsnOptions{}, &res);
        EXPECT_EQ(res.watchdog_returns, 0);
    }

    // q = 0 is a monotone rule wearing the watchdog's name, and is rejected.
    SsnOptions bad;
    bad.hint_rule = SsnHintRule::kWatchdog;
    bad.watchdog_q = 0;
    SsnEngine e(default_opts());
    SsnResult res;
    EXPECT_THROW(e.solve(qp, right, bad, &res), std::invalid_argument);
}

// =============================================================================
// PHASE-7 TASK 6b PHASE B -- R4: THE FARKAS GATE
// =============================================================================
//
// THE WHOLE LEVER IN TWO CELLS, and they are the two the trade is between:
//
//   * contradictory_qp is GENUINELY INFEASIBLE. The gate must keep the
//     diagnosis -- and must record that it was the CERTIFICATE that fired it,
//     not the symptoms alone.
//   * false_infeasible_trap_qp is FEASIBLE (the walk certifies it) and the
//     shipped symptom conjuncts call it kInfeasible. The gate must refuse.
//
// A rule that only passed the first cell is the shipped rule; a rule that only
// passed the second is a rule that never fires. Both, on the same option, is
// the measurement.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SsnEngineLocal, TheFarkasGateKeepsTheDiagnosisAndRefusesTheFeasibleTrap) {
    SsnOptions gated;
    gated.infeasibility_rule = SsnInfeasibilityRule::kFarkasGated;

    // --- (a) the genuinely infeasible cell ------------------------------
    {
        SsnResult shipped;
        {
            SsnEngine e(default_opts());
            e.solve(contradictory_qp(), SsnStart{}, SsnOptions{}, &shipped);
        }
        ASSERT_EQ(shipped.escape_reason, SsnEscape::kInfeasibleSuspect);
        EXPECT_EQ(shipped.farkas_fired, 0) << "the shipped rule has no certificate to fire";
        EXPECT_EQ(shipped.farkas_refusals, 0);

        SsnEngine e(default_opts());
        SsnResult res;
        e.solve(contradictory_qp(), SsnStart{}, gated, &res);
        EXPECT_EQ(res.status, QpStatus::kInfeasible);
        EXPECT_EQ(res.escape_reason, SsnEscape::kInfeasibleSuspect);
        EXPECT_EQ(res.farkas_fired, 1) << "the certificate, not the symptoms, issued this exit";
        EXPECT_EQ(res.factorizations, shipped.factorizations)
            << "the gate is matvec-only: it may not cost a factorization";
    }

    // --- (b) the FEASIBLE cell the shipped rule mislabels ----------------
    {
        const QpProblem qp = false_infeasible_trap_qp();
        QpEngine walk(default_opts());
        const QpSolution truth = walk.solve(qp);
        ASSERT_EQ(truth.status, QpStatus::kOptimal) << "the premise: this QP is FEASIBLE";

        SsnResult shipped;
        {
            SsnEngine e(default_opts());
            e.solve(qp, SsnStart{}, SsnOptions{}, &shipped);
        }
        ASSERT_EQ(shipped.status, QpStatus::kInfeasible)
            << "the premise: the shipped conjuncts get this one wrong";
        ASSERT_EQ(shipped.escape_reason, SsnEscape::kInfeasibleSuspect);

        SsnEngine e(default_opts());
        SsnResult res;
        e.solve(qp, SsnStart{}, gated, &res);
        EXPECT_NE(res.status, QpStatus::kInfeasible) << "no Farkas direction, no certificate";
        EXPECT_NE(res.escape_reason, SsnEscape::kInfeasibleSuspect);
        EXPECT_EQ(res.farkas_fired, 0);
        EXPECT_GE(res.farkas_refusals, 1) << "the symptoms armed and the certificate refused";
    }

    // --- (c) THE CELL THAT SEPARATES THE CERTIFICATE'S TWO HALVES --------
    //
    // A FEASIBLE narrow strip between two exactly-opposite rows. The residual
    // conjunct alone is satisfied there for free -- y = (1, 1) annihilates a
    // pair that sums to zero -- so it is the OBJECTIVE conjunct <b, y> < 0,
    // and only it, that keeps this QP from being called infeasible. Negate the
    // strip's width and the identical construction IS an infeasible pair.
    {
        const QpProblem qp = narrow_strip_feasible_qp();
        QpEngine walk(default_opts());
        const QpSolution truth = walk.solve(qp);
        ASSERT_EQ(truth.status, QpStatus::kOptimal) << "the premise: the strip is NON-EMPTY";

        SsnResult shipped;
        {
            SsnEngine e(default_opts());
            e.solve(qp, SsnStart{}, SsnOptions{}, &shipped);
        }
        ASSERT_EQ(shipped.status, QpStatus::kInfeasible)
            << "the premise: the shipped conjuncts get this one wrong too";

        SsnEngine e(default_opts());
        SsnResult res;
        e.solve(qp, SsnStart{}, gated, &res);
        EXPECT_NE(res.status, QpStatus::kInfeasible);
        EXPECT_EQ(res.farkas_fired, 0);
        EXPECT_GE(res.farkas_refusals, 1);
    }

    // --- (d) THE SIGN-CONE PROJECTION, AND WHAT IT COSTS -----------------
    //
    // An INFEASIBLE pair on which the projection changes the verdict: with it
    // the increment leaves the cone and the certificate is refused, so this
    // cell exits on the numerical route rather than reporting kInfeasible.
    // That is a recall loss and it is pinned as one -- see the fixture's own
    // note. Without the projection the same cell reports kInfeasible on
    // evidence Farkas' lemma does not admit, so the loss is the price of the
    // certificate being a certificate.
    {
        const QpProblem qp = sign_cone_discriminating_infeasible_qp();
        SsnResult shipped;
        {
            SsnEngine e(default_opts());
            e.solve(qp, SsnStart{}, SsnOptions{}, &shipped);
        }
        ASSERT_EQ(shipped.status, QpStatus::kInfeasible)
            << "the premise: the shipped symptoms diagnose this one";

        SsnEngine e(default_opts());
        SsnResult res;
        e.solve(qp, SsnStart{}, gated, &res);
        EXPECT_NE(res.status, QpStatus::kInfeasible);
        EXPECT_EQ(res.farkas_fired, 0);
        EXPECT_GE(res.farkas_refusals, 1);
    }
}
