// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_ipqp_engine.cpp -- the M6 W1 task 4 pins for the
// interior-point QP tier's cold solve, its clamp-centred box, its domain gate
// and its (rho, delta) ladder.
//
// THE ORACLE IS THE WALK: `QpEngine` cross-checks the tier on every convex
// fixture here -- two independently written kernels and two residual
// implementations. EVERY VALUE PIN IS SHOWN FALLIBLE by a neighbouring fixture.

#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>

#include "support/ipqp_test_support.h"

namespace hven::solvers {
namespace {

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

/// The same objective with one inequality row (x1 + x2 <= 1) and one equality
/// row switched on by the caller.
QpProblem general_qp(bool with_eq, bool with_iq) {
    QpProblem qp = box_qp(-10.0, 10.0);
    if (with_eq) {
        qp.Ae = dense_rows({{1.0, -1.0}}, 2);
        qp.be = vec({0.5});
    }
    if (with_iq) {
        qp.Ai = dense_rows({{1.0, 1.0}}, 2);
        qp.bi = vec({1.0});
    }
    return qp;
}

/// A BADLY SCALED but well-posed three-variable QP: the Hessian's blocks span
/// `S^2` and both constraint rows carry the same spread, so at the pins' `S = 1e6`
/// a missing unscale-on-export could not possibly go unnoticed.
QpProblem scaled_qp(double S) {
    QpProblem qp;
    qp.H = dense_upper({{2.0 / S, 0.0, 0.0}, {0.0, 2.0, 0.0}, {0.0, 0.0, 2.0 * S}});
    qp.g = vec({-2.0 / S, -4.0, -6.0 * S});
    qp.Ae = dense_rows({{S, 1.0, 1.0 / S}}, 3);
    qp.be = vec({S});
    qp.Ai = dense_rows({{1.0 / S, S, 1.0}}, 3);
    qp.bi = vec({S});
    qp.lower = vec({-10.0, -10.0, -10.0});
    qp.upper = vec({10.0, 10.0, 10.0});
    return qp;
}

/// Codex's C0 fixture: ONE variable, no rows, ABSENT bounds, `g = 0`. The origin
/// is stationary on iteration 0, so the solve converges with `rho_sched` still at
/// `ipqp_rho_init`. `c` is the whole Hessian: negative makes the problem concave.
QpProblem free_scalar_qp(double c) {
    QpProblem qp;
    qp.H = dense_upper({{c}});
    qp.g = vec({0.0});
    qp.Ae = dense_rows({}, 1);
    qp.Ai = dense_rows({}, 1);
    qp.be = Vec(0);
    qp.bi = Vec(0);
    qp.lower = vec({-1.0e20});
    qp.upper = vec({1.0e20});
    return qp;
}

QpOptions tight_opts() {
    QpOptions o;
    o.tr_radius = kInf;
    return o;
}

} // namespace

// ---------------------------------------------------------------------------
// T4.a -- the immutable clamp-centred box
// ---------------------------------------------------------------------------

TEST(IpqpBoxTest, TheCentreIsRefineOnFacesOwnClampedOrigin) {
    QpProblem qp = box_qp(0.0, 0.0);
    qp.lower = vec({3.0, -8.0});
    qp.upper = vec({7.0, -2.0});

    const IpqpBox box = make_ipqp_box(qp, 1.0);

    // clamp(0, l, u): 0 is BELOW [3, 7] and ABOVE [-8, -2], so the centre is
    // the nearer endpoint in each case -- exactly src/qp/qp_engine.cpp's
    // `c = min(max(0, lo), up)`.
    EXPECT_DOUBLE_EQ(box.centre(0), 3.0);
    EXPECT_DOUBLE_EQ(box.centre(1), -2.0);
    EXPECT_DOUBLE_EQ(box.lo_eff(0), 3.0); // max(3, 3 - 1)
    EXPECT_DOUBLE_EQ(box.up_eff(0), 4.0); // min(7, 3 + 1)
    EXPECT_DOUBLE_EQ(box.lo_eff(1), -3.0);
    EXPECT_DOUBLE_EQ(box.up_eff(1), -2.0);
    EXPECT_EQ(box.zero_width_index, -1);

    // MUTATION NON-VACUITY: a PLAIN-ORIGIN centre would give a different window on
    // this very problem ([3, 1] and [-1, -2], both CROSSED), which is the silent
    // disagreement with refine_on_face that the clamp rule exists to prevent.
    EXPECT_NE(box.up_eff(0), std::min(qp.upper(0), 0.0 + 1.0));
    EXPECT_NE(box.lo_eff(1), std::max(qp.lower(1), 0.0 - 1.0));
}

TEST(IpqpBoxTest, AnInfiniteRadiusReproducesTheDeclaredBoxExactly) {
    const QpProblem qp = box_qp(-1e20, 5.0);
    const IpqpBox box = make_ipqp_box(qp, kInf);
    EXPECT_DOUBLE_EQ(box.lo_eff(0), qp.lower(0));
    EXPECT_DOUBLE_EQ(box.up_eff(0), qp.upper(0));

    const IpqpBounds b = make_ipqp_bounds(box);
    EXPECT_EQ(b.num_lower, 0); // -1e20 is the ABSENT sentinel, not a bound.
    EXPECT_EQ(b.num_upper, 2);
    EXPECT_TRUE(b.in_domain());
}

TEST(IpqpBoxTest, MakeIpqpBoundsRefusesABoxWhoseBlocksDisagree) {
    // FIX ROUND 1, CM2 / CLAUDE.md section 4. `make_ipqp_bounds` is PUBLIC and takes
    // an IpqpBox by reference, so blocks of different lengths can arrive; Eigen's own
    // assert is compiled out under NDEBUG, so the explicit guard is the only guard.
    IpqpBox box;
    box.centre = vec({0.0, 0.0});
    box.lo_eff = vec({-1.0, -1.0});
    box.up_eff = vec({1.0});
    EXPECT_THROW(make_ipqp_bounds(box), std::invalid_argument);

    box.up_eff = vec({1.0, 1.0});
    box.centre = vec({0.0});
    EXPECT_THROW(make_ipqp_bounds(box), std::invalid_argument);

    // MUTATION NON-VACUITY: the agreeing box goes through.
    box.centre = vec({0.0, 0.0});
    IpqpBounds b;
    ASSERT_NO_THROW(b = make_ipqp_bounds(box));
    EXPECT_EQ(b.num_lower, 2);
    EXPECT_EQ(b.num_upper, 2);
}

TEST(IpqpBoxTest, MalformedRadiiAreRefusedAtTheBoundary) {
    const QpProblem qp = box_qp(-1.0, 1.0);
    EXPECT_THROW(make_ipqp_box(qp, -1.0), std::invalid_argument);
    EXPECT_THROW(make_ipqp_box(qp, std::nan("")), std::invalid_argument);
    EXPECT_NO_THROW(make_ipqp_box(qp, 0.0)); // 0 is a legal radius; it DECLINES.
}

// ---------------------------------------------------------------------------
// T4.b -- the pinned-variable domain gate
// ---------------------------------------------------------------------------

TEST(IpqpDomainGateTest, ADeclarationPinnedVariableDeclinesAndIsNotAnEscape) {
    QpProblem qp = box_qp(-1.0, 1.0);
    qp.lower(1) = 0.25;
    qp.upper(1) = 0.25;

    IpqpEngine engine(tight_opts());
    const IpqpResult r = engine.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});

    EXPECT_TRUE(r.declined_pinned);
    EXPECT_EQ(r.counters.ipqp_declined_pinned, 1);
    // A DECLINE IS NOT AN ESCAPE: the tier never ran.
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
    EXPECT_EQ(r.counters.ipqp_escapes, 0);
    EXPECT_EQ(r.counters.ipqp_iters, 0);
    EXPECT_EQ(r.counters.ipqp_factorizations, 0);
}

TEST(IpqpDomainGateTest, AZeroRadiusRetryDeclines) {
    const QpProblem qp = box_qp(-1.0, 1.0);
    IpqpEngine engine(tight_opts());
    SolveOverrides ov;
    ov.tr_radius = 0.0;
    const IpqpResult r = engine.solve(qp, nullptr, IpqpOptions{}, ov);
    EXPECT_TRUE(r.declined_pinned);
    EXPECT_EQ(r.counters.ipqp_declined_pinned, 1);

    // MUTATION NON-VACUITY: the same engine, the same problem, a positive
    // radius -- the gate must NOT fire.
    ov.tr_radius = 0.5;
    const IpqpResult ok = engine.solve(qp, nullptr, IpqpOptions{}, ov);
    EXPECT_FALSE(ok.declined_pinned);
    EXPECT_EQ(ok.counters.ipqp_declined_pinned, 0);
}

TEST(IpqpDomainGateTest, ACrossedDeclaredBoxDeclinesRatherThanThrowing) {
    QpProblem qp = box_qp(-1.0, 1.0);
    qp.lower(0) = 2.0;
    qp.upper(0) = 1.0;
    IpqpEngine engine(tight_opts());
    IpqpResult r;
    ASSERT_NO_THROW(r = engine.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{}));
    EXPECT_TRUE(r.declined_pinned);
}

// ---------------------------------------------------------------------------
// Convergence -- the walk is the oracle
// ---------------------------------------------------------------------------

namespace {

void expect_agrees_with_walk(const QpProblem &qp, double tol = 1e-6) {
    QpOptions o = tight_opts();
    QpEngine walk(o);
    const QpSolution ref = walk.solve(qp);
    ASSERT_EQ(ref.status, QpStatus::kOptimal);

    IpqpEngine tier(o);
    const IpqpResult got = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(got.status, QpStatus::kOptimal) << "escape " << static_cast<int>(got.escape_reason);
    ASSERT_EQ(got.x.size(), ref.x.size());
    EXPECT_LT((got.x - ref.x).lpNorm<Eigen::Infinity>(), tol)
        << "tier " << got.x.transpose() << " vs walk " << ref.x.transpose();
}

} // namespace

TEST(IpqpConvergenceTest, BoxConstrainedInteriorSolutionAgreesWithTheWalk) {
    expect_agrees_with_walk(box_qp(-10.0, 10.0));
}

TEST(IpqpConvergenceTest, BoxConstrainedActiveSolutionAgreesWithTheWalk) {
    expect_agrees_with_walk(box_qp(-0.5, 0.5));
}

TEST(IpqpConvergenceTest, OneInequalityRowAgreesWithTheWalk) {
    expect_agrees_with_walk(general_qp(false, true));
}

TEST(IpqpConvergenceTest, OneEqualityRowAgreesWithTheWalk) {
    expect_agrees_with_walk(general_qp(true, false));
}

TEST(IpqpConvergenceTest, BothRowBlocksAgreeWithTheWalk) {
    expect_agrees_with_walk(general_qp(true, true));
}

TEST(IpqpConvergenceTest, ATrustRegionWindowIsHonouredAndAgreesWithTheWalk) {
    const QpProblem qp = box_qp(-10.0, 10.0);
    QpOptions o = tight_opts();
    o.tr_radius = 0.5;
    QpEngine walk(o);
    const QpSolution ref = walk.solve(qp);
    ASSERT_EQ(ref.status, QpStatus::kOptimal);

    IpqpEngine tier(o);
    const IpqpResult got = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(got.status, QpStatus::kOptimal);
    EXPECT_LT((got.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-6);
    // The window really bound: both coordinates sit at the radius.
    EXPECT_NEAR(got.x(0), 0.5, 1e-6);
    EXPECT_NEAR(got.x(1), 0.5, 1e-6);
}

// ---------------------------------------------------------------------------
// The convex-inertness pin, and its non-vacuity partner
// ---------------------------------------------------------------------------

TEST(IpqpLadderTest, AConvexSubproblemLeavesTheInertiaMachineryProvablyInert) {
    IpqpEngine tier(tight_opts());
    const IpqpResult r =
        tier.solve(general_qp(true, true), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_EQ(r.counters.ipqp_inertia_retries, 0);
    EXPECT_EQ(r.counters.ipqp_reg_increases, 0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, 0.0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_last, 0.0);
    EXPECT_EQ(r.counters.ipqp_iters_at_elevated_rho, 0);
    // The certificate stands, and the required final read is what says so.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
    EXPECT_FALSE(r.certificate_downgraded);
}

TEST(IpqpLadderTest, AnIndefiniteSubproblemArmsTheLadderAndWalksToTheBound) {
    QpProblem qp = box_qp(-10.0, 10.0);
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1000.0}});

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});

    // THE NON-VACUITY PARTNER of the convex-inertness pin above: the three
    // counters that are structurally zero on a convex subproblem are nonzero
    // here, so "provably inert" is a measurement rather than an absence.
    EXPECT_GT(r.counters.ipqp_inertia_retries, 0);
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);
    EXPECT_GT(r.counters.ipqp_iters_at_elevated_rho, 0);
    EXPECT_NE(r.status, QpStatus::kOptimal);

    // THE LADDER IS NOT MONOTONE, AND THIS IS WHERE THAT IS PINNED (T4b). Algorithm
    // IC retries `rho_dem_last / 3` at every iteration, so the memory ends BELOW the
    // solve's own peak. `.superpowers/w1-t4b-report.md`.
    EXPECT_LT(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);
    EXPECT_GT(r.counters.ipqp_ladder_reclimbs, 0)
        << "and the /3 probe really is being refused and re-climbed here";

    // WHERE IT GOES: `H = diag(2, -1000)` with `g = (-2, -4)` on `[-10, 10]^2` has
    // its minimizers at the ENDS of the negative-curvature coordinate. Before T4b
    // this fixture froze at a fixed point and burnt its budget (`kBudget`).
    EXPECT_NEAR(r.x(0), 1.0, 1e-6);
    EXPECT_NEAR(r.x(1), 10.0, 1e-9);
    // THE FIXTURE WHOSE MEASURED FAILURE MOTIVATED THE PERTURBED-PIVOT RE-ROUTE. What
    // is asserted is the OUTCOME, not the route counters: whether the backend reports
    // a perturbed pivot here is flag-sensitive. `.superpowers/w1-t4b-report.md`.
    EXPECT_GT(r.counters.ipqp_iters, 0)
        << "the solve TAKES STEPS -- the pre-T4b rule's zero-iteration kNumerical is the failure "
           "the re-route exists to remove";
    EXPECT_EQ(r.counters.ipqp_pivot_reroute_dual_fallback, 0)
        << "whatever the primal route did here, it was never exhausted";
    EXPECT_LT(r.counters.ipqp_iters, IpqpOptions{}.ipqp_hard_iter_cap)
        << "the budget is no longer what stops it -- mechanism 4's freeze is gone";
    EXPECT_NE(r.escape_reason, IpqpEscape::kBudget);

    // ... AND WHAT STOPS IT INSTEAD, pinned rather than described: the barrier
    // endgame ends at the strict-positivity guard once `u - x` falls below `ulp(u)`.
    // A missed CERTIFICATE, not a wrong answer. `.superpowers/w1-t4b-report.md`.
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNumerical);
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0)
        << "no certifying exit was reached, so the required read was never paid";
}

TEST(IpqpLadderTest, TheICTrialIsRefusedAndTheReclimbIsCounted) {
    // THIS REPLACES `TheMonotoneFloorRefusesADecreaseAndCountsItAsAFlap`: T4b deletes
    // the monotone floor, so `ipqp_ladder_reclimbs` replaces `ipqp_rho_flaps`. The
    // fixture is chosen flag-stable: `.superpowers/w1-t4b-report.md`.
    QpProblem qp = box_qp(-10.0, 10.0);
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1000.0}});

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});

    // THE RECLIMBS, AS AN EXACT COUNT: eight iterations of this solve have a
    // `rho_dem_last / kIpqpLadderDown` value refused and re-escalate past it. A
    // perturbed-driven escalation is NOT one. `.superpowers/w1-t4b-report.md`.
    EXPECT_EQ(r.counters.ipqp_pivot_reroute_dual_fallback, 0);
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("t4b_reclimb_accelerate", "UNOBSERVED -- the exact reclimb count is MKL-only");
    EXPECT_GT(r.counters.ipqp_ladder_reclimbs, 0);
#else
    EXPECT_EQ(r.counters.ipqp_ladder_reclimbs, 8);
#endif
    // ... and the memory ends far below the peak, which is the non-monotone
    // statement again, here beside the counter that measures its cost.
    EXPECT_LT(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);
    // A RECLIMB IS AN ITERATION, NOT A RUNG: it can never exceed the number of
    // iterations that ran the ladder at all.
    EXPECT_LE(r.counters.ipqp_ladder_reclimbs, r.counters.ipqp_iters_at_elevated_rho);
    // ... and each one costs at least the refused factorization plus its
    // replacement, so the rejection count dominates it.
    EXPECT_LE(r.counters.ipqp_ladder_reclimbs, r.counters.ipqp_inertia_retries);

    // THE T4b IDENTITY. Class (a) (a decrease the monotone floor refused) no
    // longer exists, so every gated advance is either a decrease or a class
    // (c) advance that moved nothing. This solve's advances all moved.
    EXPECT_EQ(r.counters.ipqp_prox_center_updates, r.counters.ipqp_reg_decreases);
#ifndef USE_ACCELERATE_SPARSE
    EXPECT_EQ(r.counters.ipqp_prox_center_updates, 7);
#endif

    // MUTATION NON-VACUITY: the same solve on a CONVEX Hessian never arms the
    // ladder, so there is no memory to probe and no reclimb to count, while
    // the same gate still produces decreases.
    QpProblem convex = box_qp(-10.0, 10.0);
    IpqpEngine tier2(tight_opts());
    const IpqpResult c = tier2.solve(convex, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(c.status, QpStatus::kOptimal);
    EXPECT_EQ(c.counters.ipqp_ladder_reclimbs, 0);
    EXPECT_DOUBLE_EQ(c.counters.ipqp_rho_demanded_max, 0.0);
    EXPECT_GT(c.counters.ipqp_reg_decreases, 0);
    EXPECT_EQ(c.counters.ipqp_prox_center_updates, c.counters.ipqp_reg_decreases);
}

TEST(IpqpLadderTest, TheABSOLUTEFloorIsNeitherADecreaseNorAnythingElse) {
    // FIX ROUND 1, I2, RE-PINNED AT T4b: a gated advance whose quantities are already
    // on the absolute `ipqp_reg_floor` is a class (c) advance -- the prox centre
    // moves, nothing else does, and no counter fires. The fixture starts near it.
    IpqpOptions io;
    io.ipqp_rho_init = 1.0e-9;   // reg_floor * 10
    io.ipqp_delta_init = 1.0e-9; // ... and the dual side with it
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(general_qp(true, true), nullptr, io, SolveOverrides{});

    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_DOUBLE_EQ(r.rho, io.ipqp_reg_floor);
    EXPECT_DOUBLE_EQ(r.delta, io.ipqp_reg_floor);
    // A CONVEX subproblem never arms the ladder at all, so there is no IC
    // memory and nothing to reclimb however many advances hit the absolute
    // floor.
    EXPECT_EQ(r.counters.ipqp_ladder_reclimbs, 0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, 0.0);

    // THE EXACT ACCOUNTING, pinned as numbers rather than as an inequality (fix round
    // 2): five gated advances, the last three moving NOTHING, which is class (c). The
    // advance counts are exact trajectory pins and are MKL-scoped (T4b F5).
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("t4b_absolute_floor_accelerate",
                   "UNOBSERVED -- the exact advance counts are MKL-only");
#else
    EXPECT_EQ(r.counters.ipqp_prox_center_updates, 5); // T10b: 4 at the 0.1 placeholder.
    EXPECT_EQ(r.counters.ipqp_reg_decreases, 2);
    // ... so the identity's residual IS the class-(c) count, and it is 2 here. The
    // section 7 counter table has no field for that class, so it is pinned by
    // arithmetic on the fields that do exist rather than left unstated.
    EXPECT_EQ(r.counters.ipqp_prox_center_updates - r.counters.ipqp_reg_decreases, 3);
#endif
    EXPECT_GE(r.counters.ipqp_prox_center_updates, r.counters.ipqp_reg_decreases)
        << "the class-(c) residual is a count and is never negative";
}

TEST(IpqpLadderTest, TheIdentityHoldsOnALongConvexSolveThatNeverArmsTheLadder) {
    // THE SECOND, INDEPENDENT WITNESS for I2, at a tight tolerance and a raised cap so
    // the section 3.2 gate gets more chances to fire. It is NOT the "past the 11th
    // advance" fixture: the gate is a CONTRACTION test, so advances stay rare.
    IpqpOptions io;
    io.ipqp_converge_slack = 1.0; // the tightest the band allows
    io.ipqp_hard_iter_cap = 400;
    io.ipqp_min_mu = 1e-16;

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(general_qp(true, true), nullptr, io, SolveOverrides{});

    ASSERT_EQ(r.status, QpStatus::kOptimal);
    ASSERT_GT(r.counters.ipqp_iters, 11);
    ASSERT_GT(r.counters.ipqp_prox_center_updates, 0);
    EXPECT_EQ(r.counters.ipqp_ladder_reclimbs, 0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, 0.0);
    EXPECT_EQ(r.counters.ipqp_prox_center_updates, r.counters.ipqp_reg_decreases);
}

TEST(IpqpLadderTest, TheFinalReadCatchesASaddleTheLadderWouldOtherwiseCertify) {
    // THE CERTIFICATION READ IS ONE FACTORIZATION, NOT A LADDER, and this fixture is
    // why: the origin is an exact KKT point of the barrier problem at every mu, so a
    // read that CLIMBED the ladder would certify this SADDLE as a minimum.
    QpProblem qp = box_qp(-10.0, 10.0);
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1.0}});
    qp.g = vec({0.0, 0.0});

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});

    // It converged: the iteration count is small and the residuals are inside
    // the target, so this is a CERTIFYING exit that was then downgraded, not a
    // solve that fell over.
    EXPECT_LE(r.counters.ipqp_iters, 10);
    EXPECT_LE(r.residuals.worst(), tight_opts().opt_tol * IpqpOptions{}.ipqp_converge_slack);
    EXPECT_NEAR(r.x(0), 0.0, 1e-9);
    EXPECT_NEAR(r.x(1), 0.0, 1e-9);

    // ... and the certificate did NOT stand.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
    EXPECT_TRUE(r.certificate_downgraded);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
    EXPECT_EQ(r.status, QpStatus::kNumericalError);
    // The ladder DID fire and DID reach a rho at which the inertia reads
    // right -- which is precisely the value a climbing certification read
    // would have certified at.
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, r.rho);

    // FIX ROUND 1, I4: AN EXACT COUNT, not `> 0` -- every iteration takes its step
    // with a nonzero inertia-demanded modification, so the counter equals the
    // iteration count. Unmoved by fix round 2's N1: `.superpowers/w1-t4-report.md`.
#ifndef USE_ACCELERATE_SPARSE
    EXPECT_EQ(r.counters.ipqp_iters, 3);
    EXPECT_EQ(r.counters.ipqp_iters_at_elevated_rho, 3);
#endif
    EXPECT_EQ(r.counters.ipqp_iters_at_elevated_rho, r.counters.ipqp_iters);
    // The gated advances are classified exactly once each (I2).
    EXPECT_EQ(r.counters.ipqp_prox_center_updates, r.counters.ipqp_reg_decreases);

    // ALGORITHM IC'S OWN TRAJECTORY, PINNED RUNG BY RUNG (T4b): ten factorizations
    // and six rejections over three iterations, one backend's measurement and so
    // MKL-scoped (T4b F5). Rung-by-rung: `.superpowers/w1-t4b-report.md`.
#ifdef USE_ACCELERATE_SPARSE
    RecordProperty("t4b_saddle_trajectory_accelerate",
                   "UNOBSERVED -- the exact ladder trajectory is MKL-only");
#else
    EXPECT_EQ(r.counters.ipqp_factorizations, 10);
    EXPECT_EQ(r.counters.ipqp_inertia_retries, 6);
    EXPECT_EQ(r.counters.ipqp_ladder_reclimbs, 0)
        << "the /3 value is ACCEPTED on both of the iterations that try it (100/3 and 100/9), so "
           "there is nothing to re-escalate past -- and after fix round 1 widened the charge to "
           "cover the zero-trial route as well, a zero here is a measurement rather than a blind "
           "spot: this fixture's cost is the retried ZERO-trial, which `ipqp_inertia_retries` "
           "already carries";
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, 100.0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_last, 100.0 / 3.0 / 3.0);
#endif
    // `ipqp_pivot_reroute_primal` is not pinned here: an exact count would pin the
    // backend's perturbation threshold (concern C3). The fallback needs two failed
    // primal rungs, so it is structural. (T4b F4; `.superpowers/w1-t4b-report.md`.)
    EXPECT_EQ(r.counters.ipqp_pivot_reroute_dual_fallback, 0);
    EXPECT_DOUBLE_EQ(r.rho_mod, r.counters.ipqp_rho_demanded_last)
        << "`rho_mod` is the modification the LAST step ran at, which on this fixture is also "
           "the memory's final value";

    // MUTATION NON-VACUITY: the same fixture with the sign of the second
    // curvature flipped is convex, converges to the same point, and the same
    // read STANDS.
    QpProblem convex = qp;
    convex.H = dense_upper({{2.0, 0.0}, {0.0, 1.0}});
    IpqpEngine tier2(tight_opts());
    const IpqpResult c = tier2.solve(convex, nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(c.status, QpStatus::kOptimal);
    EXPECT_EQ(c.counters.ipqp_final_inertia_read, 0);
    EXPECT_FALSE(c.certificate_downgraded);
    EXPECT_DOUBLE_EQ(c.counters.ipqp_rho_demanded_max, 0.0);
    EXPECT_EQ(c.counters.ipqp_iters_at_elevated_rho, 0);
}

TEST(IpqpLadderTest, TheFinalReadDropsToTheSCHEDULEFLOORNotToWhereverTheScheduleStopped) {
    // FIX ROUND 1, C0 (Codex critical): section 2.2 item 4's "the schedule's residual
    // level" is the level the schedule DECAYS TO, `ipqp_reg_floor`. Reading off
    // `H + rho_sched I` instead would certify a concave problem at its MAXIMUM.
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(free_scalar_qp(-1.0), nullptr, IpqpOptions{}, SolveOverrides{});

    // The premise of the fixture, asserted rather than assumed: it converged
    // immediately, so the schedule never moved.
    EXPECT_EQ(r.counters.ipqp_iters, 0);
    EXPECT_EQ(r.counters.ipqp_reg_decreases, 0);
    EXPECT_DOUBLE_EQ(r.rho, IpqpOptions{}.ipqp_rho_init);

    // ... and the certificate did not stand.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 1);
    EXPECT_TRUE(r.certificate_downgraded);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
    EXPECT_NE(r.status, QpStatus::kOptimal);
    // The read cost EXACTLY the one factorization section 2.2 item 4 prices
    // it at, on a solve that took no iterations at all.
    EXPECT_EQ(r.counters.ipqp_factorizations, 1);

    // MUTATION NON-VACUITY: the convex twin reaches the same final read by the same
    // route -- converged at iteration 0, schedule untouched -- and CERTIFIES, so the
    // pin above is about the curvature, not about the fixture being degenerate.
    IpqpEngine tier2(tight_opts());
    const IpqpResult c = tier2.solve(free_scalar_qp(1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(c.counters.ipqp_iters, 0);
    EXPECT_EQ(c.counters.ipqp_final_inertia_read, 0);
    EXPECT_FALSE(c.certificate_downgraded);
    EXPECT_EQ(c.status, QpStatus::kOptimal);
    EXPECT_EQ(c.counters.ipqp_factorizations, 1);
}

TEST(IpqpBudgetTest, TheFactorizationCapIsCheckedBeforeEVERYFactorizationLadderRungsIncluded) {
    // FIX ROUND 1, I6: the cap used to be consulted once per iteration, so a single
    // ladder could outrun it. A cap that a ladder can walk through is not a cap.
    // `.superpowers/w1-t4-report.md`.
    QpProblem qp = box_qp(-10.0, 10.0);
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1.0e12}});
    IpqpOptions io;
    io.ipqp_max_factorizations = 1;

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, io, SolveOverrides{});

    EXPECT_EQ(r.counters.ipqp_factorizations, 1);
    // A BUDGET STOP, NOT AN INERTIA VERDICT: no second factorization ran, so
    // there is no reading to classify as indefinite or numerical.
    EXPECT_EQ(r.escape_reason, IpqpEscape::kBudget);
    EXPECT_EQ(r.status, QpStatus::kMaxIter);
    EXPECT_EQ(r.counters.ipqp_iters, 0);
    // FIX ROUND 2, N1, AND THIS IS ITS DISCRIMINATING FIXTURE: the ladder raised
    // `rho` and the cap then refused the rung's own factorization, so no step was
    // taken. The counter says "iterations TAKEN", and there were none: this is 0.
    EXPECT_EQ(r.counters.ipqp_iters_at_elevated_rho, 0);
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0); // the ladder DID arm.

    // MUTATION NON-VACUITY: the same fixture with the cap lifted really does
    // want more than one factorization, so the pin above is measuring the cap
    // rather than a solve that happened to need one.
    IpqpOptions loose;
    IpqpEngine tier2(tight_opts());
    const IpqpResult r2 = tier2.solve(qp, nullptr, loose, SolveOverrides{});
    EXPECT_GT(r2.counters.ipqp_factorizations, 1);
    // WITHOUT the cap the same subproblem takes many steps and stops for a reason of its
    // own, so the cap plainly changed the outcome. (Before T4b this read `kIndefinite`: the
    // ladder was applied unscaled then. `.superpowers/w1-t4b-report.md`.)
    // T10b: at the measured `ipqp_init_mu` this extreme fixture (H22 = -1e12) ends
    // kOptimal in Release and kNumericalError in Debug -- a knife-edge the status
    // cannot carry. What the mutation needs is that the CAP is not what stopped it.
    EXPECT_NE(r2.escape_reason, IpqpEscape::kBudget);
    EXPECT_NE(r2.status, QpStatus::kMaxIter);
    EXPECT_GT(r2.counters.ipqp_iters, 1);
}

TEST(IpqpLadderTest, AnExhaustedLadderStopsAtTheCeilingExactlyAndReportsIndefinite) {
    QpProblem qp = box_qp(-10.0, 10.0);
    // Curvature no regularization below the 1e6 ceiling can dominate.
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1.0e12}});

    // EQUILIBRATION OFF, AND THAT IS THE FIXTURE (T4b): the modification is a uniform
    // shift of the RUIZ-SCALED system, so with equilibration ON the ladder never comes
    // near its ceiling and this test would be asserting nothing.
    IpqpOptions io;
    io.ipqp_ruiz = false;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, io, SolveOverrides{});

    EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
    EXPECT_EQ(r.status, QpStatus::kNumericalError);
    // THE CAP-SLACK LESSON (spec 3.2, Amendment G): repeated multiplication by 100
    // lands on 999999.9999999998 rather than on 1e6, so an exact `>=` ceiling test
    // grants one more rung. The relative guard closes it and the top rung is exact.
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, io.ipqp_reg_max);
    EXPECT_GE(r.counters.ipqp_rho_demanded_max, io.ipqp_reg_max * (1.0 - detail::kSsnProxCapSlack));

    // THE REFUSED CEILING RUNG DOES NOT ENTER IC'S MEMORY (fix round 1, CX3).
    // `ipqp_rho_demanded_last` is the shift the last SUCCESSFUL modified
    // factorization ran at; the ceiling produced none, so it must not be recorded.
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_last, 0.0)
        << "no modified factorization on this solve ever succeeded, so the memory stays empty";
    EXPECT_LT(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);
    EXPECT_DOUBLE_EQ(r.rho_mod, 0.0) << "and no step was taken at any modification either";

    // NON-VACUITY: a solve whose ladder DOES succeed at a modified value
    // records exactly that value, so the pin above is about the refused rung
    // and not about the field being dead.
    IpqpEngine tier2(tight_opts());
    const IpqpResult ok =
        tier2.solve(box_qp(-10.0, 10.0), nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_DOUBLE_EQ(ok.counters.ipqp_rho_demanded_last, 0.0) << "convex: never armed";
    QpProblem saddle = box_qp(-10.0, 10.0);
    saddle.H = dense_upper({{2.0, 0.0}, {0.0, -1.0}});
    saddle.g = vec({0.0, 0.0});
    IpqpEngine tier3(tight_opts());
    const IpqpResult sd = tier3.solve(saddle, nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_GT(sd.counters.ipqp_rho_demanded_last, 0.0)
        << "and a solve whose modified factorizations DID succeed records the last of them";
}

// ---------------------------------------------------------------------------
// A9 -- the verify-once discipline
// ---------------------------------------------------------------------------

TEST(IpqpFactorizationTest, EachTierEntryPaysExactlyOneSymbolicPassOrOnePatternVerify) {
    const QpProblem qp = general_qp(true, true);
    IpqpEngine tier(tight_opts());

    const IpqpResult first = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    // The entry that LAYS OUT the pattern pays the analysis; compute() does not verify
    // (kkt_factorization.cpp), so its verify count is 0. That is plan section 7 note
    // (a)'s own premise, applied to the one entry the note's "+1" cannot describe.
    EXPECT_EQ(first.counters.ipqp_symbolic_analyses, 1);
    EXPECT_EQ(first.counters.ipqp_pattern_verifies, 0);

    const IpqpResult second = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(second.status, QpStatus::kOptimal);
    // Every LATER entry hoists the analysis and pays EXACTLY ONE verify --
    // the one-time O(nnz) payment -- with every other factorization in the
    // entry running kAssumeAnalyzed.
    EXPECT_EQ(second.counters.ipqp_symbolic_analyses, 0);
    EXPECT_EQ(second.counters.ipqp_pattern_verifies, 1);
    EXPECT_GT(second.counters.ipqp_factorizations, 1);

    // MUTATION NON-VACUITY: the kill switch really kills it.
    IpqpOptions io;
    io.ipqp_hoist_symbolic = false;
    const IpqpResult third = tier.solve(qp, nullptr, io, SolveOverrides{});
    ASSERT_EQ(third.status, QpStatus::kOptimal);
    EXPECT_EQ(third.counters.ipqp_symbolic_analyses, 1);
    EXPECT_EQ(third.counters.ipqp_pattern_verifies, 0);
}

TEST(IpqpFactorizationTest, PredictorAndCorrectorShareOneFactorizationPerIteration) {
    const QpProblem qp = general_qp(true, true);
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    // Amendment E's re-entrancy assertion, taken as a counter identity: TWO
    // triangular solves per iteration and no more, so both right-hand sides
    // went through one numeric factorization.
    EXPECT_EQ(r.counters.ipqp_solves, 2 * r.counters.ipqp_iters);
    // One factorization per iteration plus the section 2.2 item 4 final read,
    // and nothing else, on a subproblem whose ladder never fired.
    ASSERT_EQ(r.counters.ipqp_inertia_retries, 0);
    EXPECT_EQ(r.counters.ipqp_factorizations, r.counters.ipqp_iters + 1);
}

TEST(IpqpFactorizationTest,
     DisablingTheFinalReadDowngradesUnconditionallyAndSavesTheFactorization) {
    const QpProblem qp = general_qp(true, true);
    IpqpOptions io;
    io.ipqp_require_final_inertia = false;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, io, SolveOverrides{});

    EXPECT_TRUE(r.certificate_downgraded);
    EXPECT_NE(r.status, QpStatus::kOptimal);
    // FIX ROUND 1, I5 (settler ruling): a DOWNGRADE, NOT AN ESCAPE. Turning the read
    // off to save a factorization must not manufacture a census entry or a section
    // 6.1 K = 3 charge. `3` is "not performed (option off)", kept apart from `2`.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 3);
    EXPECT_EQ(r.escape_reason, IpqpEscape::kNone);
    EXPECT_EQ(r.counters.ipqp_escape_indefinite, 0); // the census is task 5's.
    // ... and it really did save the factorization.
    EXPECT_EQ(r.counters.ipqp_factorizations, r.counters.ipqp_iters);

    // MUTATION NON-VACUITY: the same solve with the read ON pays the extra
    // factorization, reads 0, and certifies.
    IpqpEngine tier2(tight_opts());
    const IpqpResult on = tier2.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(on.status, QpStatus::kOptimal);
    EXPECT_FALSE(on.certificate_downgraded);
    EXPECT_EQ(on.counters.ipqp_final_inertia_read, 0);
    EXPECT_EQ(on.counters.ipqp_factorizations, on.counters.ipqp_iters + 1);
}

// ---------------------------------------------------------------------------
// Budgets, determinism, equilibration
// ---------------------------------------------------------------------------

TEST(IpqpBudgetTest, TheHardCapIsTheBudgetOfLastResortAndReportsMaxIter) {
    IpqpOptions io;
    io.ipqp_hard_iter_cap = 1;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(general_qp(true, true), nullptr, io, SolveOverrides{});
    EXPECT_EQ(r.escape_reason, IpqpEscape::kBudget);
    EXPECT_EQ(r.status, QpStatus::kMaxIter);
    EXPECT_EQ(r.counters.ipqp_iters, 1);
}

TEST(IpqpBudgetTest, TheFactorizationCapBindsIndependentlyOfTheIterationCap) {
    IpqpOptions io;
    io.ipqp_max_factorizations = 2;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(general_qp(true, true), nullptr, io, SolveOverrides{});
    EXPECT_EQ(r.escape_reason, IpqpEscape::kBudget);
    EXPECT_LT(r.counters.ipqp_iters, io.ipqp_hard_iter_cap);
}

TEST(IpqpBudgetTest, AFinalReadTheBudgetREFUSEDIsNotPerformedRatherThanUnreadable) {
    // FIX ROUND 2, N2 (settler ruling): `2` is ATTEMPTED-and-unusable, so a
    // certification factorization the cap refused belongs with the option-off case,
    // `3`. The cap is calibrated from the solve itself rather than hard-coded.
    const QpProblem qp = general_qp(true, true);
    IpqpEngine calib(tight_opts());
    const IpqpResult base = calib.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(base.status, QpStatus::kOptimal);
    ASSERT_EQ(base.counters.ipqp_factorizations, base.counters.ipqp_iters + 1);

    IpqpOptions io;
    io.ipqp_max_factorizations = base.counters.ipqp_iters;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, io, SolveOverrides{});

    // Every iteration ran; only the certification read was refused.
    EXPECT_EQ(r.counters.ipqp_iters, base.counters.ipqp_iters);
    EXPECT_EQ(r.counters.ipqp_factorizations, io.ipqp_max_factorizations);
    // A READ THAT NEVER HAPPENED IS 3 -- kept apart from `2`, which the census
    // routes to `ipqp_escape_numerical` and which would misattribute a budget
    // stop as a numerical failure.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 3);
    EXPECT_TRUE(r.certificate_downgraded);
    // ... and the escape is still the budget, because the budget is genuinely
    // why this solve stopped. `3` is never its own escape class.
    EXPECT_EQ(r.escape_reason, IpqpEscape::kBudget);
    EXPECT_NE(r.status, QpStatus::kOptimal);

    // MUTATION NON-VACUITY: one more factorization and the same solve pays the
    // read, gets 0, and certifies.
    io.ipqp_max_factorizations = base.counters.ipqp_iters + 1;
    IpqpEngine tier2(tight_opts());
    const IpqpResult ok = tier2.solve(qp, nullptr, io, SolveOverrides{});
    EXPECT_EQ(ok.counters.ipqp_final_inertia_read, 0);
    EXPECT_FALSE(ok.certificate_downgraded);
    EXPECT_EQ(ok.status, QpStatus::kOptimal);
}

TEST(IpqpDeterminismTest, TwoColdSolvesOfTheSameProblemAgreeBitForBit) {
    const QpProblem qp = general_qp(true, true);
    IpqpEngine a(tight_opts());
    IpqpEngine b(tight_opts());
    const IpqpResult ra = a.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    const IpqpResult rb = b.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(ra.status, QpStatus::kOptimal);
    EXPECT_EQ(ra.counters.ipqp_iters, rb.counters.ipqp_iters);
    for (Index i = 0; i < ra.x.size(); ++i) {
        EXPECT_DOUBLE_EQ(ra.x(i), rb.x(i));
    }
    EXPECT_DOUBLE_EQ(ra.mu, rb.mu);
}

TEST(IpqpRuizTest, EquilibrationRoundTripsAndBothArmsLandOnTheWalksOwnPoint) {
    const QpProblem qp = scaled_qp(1.0e6);
    QpOptions o = tight_opts();
    QpEngine walk(o);
    const QpSolution ref = walk.solve(qp);
    ASSERT_EQ(ref.status, QpStatus::kOptimal);

    IpqpOptions on;
    IpqpOptions off;
    off.ipqp_ruiz = false;

    IpqpEngine ta(o);
    IpqpEngine tb(o);
    const IpqpResult ra = ta.solve(qp, nullptr, on, SolveOverrides{});
    const IpqpResult rb = tb.solve(qp, nullptr, off, SolveOverrides{});

    ASSERT_EQ(ra.status, QpStatus::kOptimal);
    ASSERT_EQ(rb.status, QpStatus::kOptimal);
    // EVERY PIN IS ON UNSCALED QUANTITIES (spec 4.3, non-negotiable). The
    // equilibration diagonal spans six decades in every block here, so a right-hand
    // side scaled going in and not unscaled coming out would move the answer.
    EXPECT_LT((ra.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-7);
    EXPECT_LT((rb.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-7);
    EXPECT_LT((ra.x - rb.x).lpNorm<Eigen::Infinity>(), 1e-9);
    // ANSWER-NEUTRALITY IS THE CLAIM, AND IT IS WHAT WAS MEASURED: on every fixture
    // in this file the two arms take the same iterations and factorizations. No
    // trajectory-changing fixture exists at this scale; task 9's battery is where.
    EXPECT_EQ(ra.counters.ipqp_iters, rb.counters.ipqp_iters);
}

TEST(IpqpRuizTest, ABadlyScaledSubproblemStillAgreesWithTheWalk) {
    for (const double S : {1.0, 1.0e2, 1.0e4, 1.0e6}) {
        const QpProblem qp = scaled_qp(S);
        QpOptions o = tight_opts();
        QpEngine walk(o);
        const QpSolution ref = walk.solve(qp);
        ASSERT_EQ(ref.status, QpStatus::kOptimal) << "S = " << S;
        IpqpEngine tier(o);
        const IpqpResult got = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
        ASSERT_EQ(got.status, QpStatus::kOptimal) << "S = " << S;
        EXPECT_LT((got.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-6) << "S = " << S;
    }
}

TEST(IpqpRuizTest, TheRelativeStoppingRuleIsFOLDEDGLOBALLYAndThatHasAMeasuredPrice) {
    // A DOCUMENTED LIMIT, PINNED SO A CHANGE TO IT IS VISIBLE -- not a bug: the ONE
    // globally-folded `max(1, ...)` scale (plan ruling 5) lets a coordinate of tiny
    // curvature stop far out in ABSOLUTE terms. `.superpowers/w1-t4-report.md`.
    QpProblem qp;
    qp.H = dense_upper({{2.0e6, 0.0}, {0.0, 2.0e-6}});
    qp.g = vec({-2.0e6, -4.0e-6});
    qp.Ae = dense_rows({}, 2);
    qp.be = Vec(0);
    qp.Ai = dense_rows({{1.0e3, 1.0e-3}}, 2);
    qp.bi = vec({1.0e3});
    qp.lower = vec({-10.0, -10.0});
    qp.upper = vec({10.0, 10.0});

    QpOptions o = tight_opts();
    QpEngine walk(o);
    const QpSolution ref = walk.solve(qp);
    ASSERT_EQ(ref.status, QpStatus::kOptimal);
    IpqpEngine tier(o);
    const IpqpResult got = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(got.status, QpStatus::kOptimal);

    // The well-scaled coordinate is right; the badly-scaled one is not, and
    // both facts are asserted so neither can drift unnoticed.
    EXPECT_NEAR(got.x(0), ref.x(0), 1e-5);
    EXPECT_GT(std::abs(got.x(1) - ref.x(1)), 1e-2);
    // ... and the certificate is honest about WHY: every relative residual is
    // inside the target the tier was asked for.
    EXPECT_LE(got.residuals.stationarity, o.opt_tol * IpqpOptions{}.ipqp_converge_slack);
    EXPECT_LE(got.residuals.primal_iq, o.feas_tol * IpqpOptions{}.ipqp_converge_slack);
}

// ---------------------------------------------------------------------------
// The face classification
// ---------------------------------------------------------------------------

TEST(IpqpFaceTest, AnActiveBoundIsClassifiedActiveAndAnInactiveOneInactive) {
    // Solution is pinned at the upper bound in both coordinates.
    const QpProblem qp = box_qp(-10.0, 0.5);
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);

    EXPECT_EQ(r.upper_face[0], IpqpFace::kActive);
    EXPECT_EQ(r.upper_face[1], IpqpFace::kActive);
    EXPECT_EQ(r.lower_face[0], IpqpFace::kInactive);
    EXPECT_EQ(r.lower_face[1], IpqpFace::kInactive);
    EXPECT_EQ(r.bound_state[0], BoundState::kAtUpper);
    EXPECT_EQ(r.bound_state[1], BoundState::kAtUpper);
    EXPECT_LT(r.z(0), 0.0); // QpSolution's sign convention at an upper bound.
    EXPECT_FALSE(r.tr_active[0]);

    // MUTATION NON-VACUITY: widen the box so nothing is active, and the same
    // three assertions must all flip.
    IpqpEngine tier2(tight_opts());
    const IpqpResult free_r =
        tier2.solve(box_qp(-10.0, 10.0), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(free_r.status, QpStatus::kOptimal);
    EXPECT_EQ(free_r.upper_face[0], IpqpFace::kInactive);
    EXPECT_EQ(free_r.bound_state[0], BoundState::kFree);
    EXPECT_NEAR(free_r.z(0), 0.0, 1e-6);
}

TEST(IpqpFaceTest, ATrustRegionPinIsReportedThroughTrActiveAndNotThroughBoundState) {
    const QpProblem qp = box_qp(-10.0, 10.0);
    QpOptions o = tight_opts();
    o.tr_radius = 0.5;
    IpqpEngine tier(o);
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);

    // QpSolution::tr_active's contract verbatim: a TR-pinned variable reports
    // kFree, carries no bound multiplier, and is visible ONLY here.
    EXPECT_TRUE(r.tr_active[0]);
    EXPECT_TRUE(r.tr_active[1]);
    EXPECT_EQ(r.bound_state[0], BoundState::kFree);
    EXPECT_DOUBLE_EQ(r.z(0), 0.0);
}

// THE TWO EXPORT INVARIANTS THIS TIER RE-DERIVES (M6 W1 task 6 fix round 1).
// `qp_engine.h`'s export contract says a third producer must RE-DERIVE rather than
// inherit; these pin the re-derivation directly on the engine, not via a driver.
TEST(IpqpFaceTest, AFreeVariableCarriesExactlyZeroAndAnAbsentBoundIsNeverPriced) {
    // A WIDE box with a FINITE trust region -- the configuration that makes both
    // invariants non-trivial: under a finite radius the barrier carries a (zl, zu)
    // pair at every index, and the optimum (1, 2) is strictly inside the real box.
    const QpProblem qp = box_qp(-10.0, 10.0);
    QpOptions o = tight_opts();
    o.tr_radius = 20.0; // finite, but wide enough not to pin anything
    IpqpEngine tier(o);
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);

    // (6b) `bound_state[i] == kFree ==> z(i) == 0.0`, EXACTLY -- not "small".
    // The barrier residue at an inactive bound is ~ mu / distance, which is
    // nonzero at every finite tolerance; the invariant is an equality.
    for (Index i = 0; i < qp.n(); ++i) {
        ASSERT_EQ(r.bound_state[static_cast<std::size_t>(i)], BoundState::kFree)
            << "index " << i << ": the fixture's premise is that nothing is active";
        EXPECT_DOUBLE_EQ(r.z(i), 0.0)
            << "index " << i << ": a free variable carries no bound multiplier";
    }
    // NON-VACUITY: the residue really is there to be dropped. An ACTIVE bound
    // on the same objective is priced, so the zeros above are the invariant's
    // doing rather than an all-zero barrier block.
    IpqpEngine pinned(o);
    const IpqpResult active =
        pinned.solve(box_qp(-10.0, 0.5), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(active.status, QpStatus::kOptimal);
    ASSERT_EQ(active.bound_state[0], BoundState::kAtUpper);
    EXPECT_LT(active.z(0), 0.0);
}

TEST(IpqpFaceTest, AnAbsentRealBoundIsNeverPricedEvenWhenTheTrustRegionSuppliesOne) {
    // NO LOWER BOUND AT ALL (the +/-1e20 absent sentinel), a finite radius, and an
    // optimum ON the upper bound. `SsnEngine::solve` REFUSES a start whose z prices
    // an absent bound, which is how this defect first surfaced.
    const QpProblem qp = box_qp(-1e20, 0.5);
    QpOptions o = tight_opts();
    o.tr_radius = 1.0;
    IpqpEngine tier(o);
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);

    ASSERT_EQ(r.bound_state[0], BoundState::kAtUpper) << "the fixture's premise";
    EXPECT_LT(r.z(0), 0.0) << "the REAL upper bound is priced, with QpSolution's sign convention";
    // And the absent lower side contributes NOTHING to it: the exported price
    // is the upper block alone, so `zl` -- whatever the barrier put there
    // against the trust region's own lower wall -- does not appear.
    EXPECT_DOUBLE_EQ(r.z(0), -r.zu(0))
        << "the absent-side test reads the REAL bound, so an absent lower side contributes 0 "
           "however large its raw zl is (the residue there is a trust-region dual, and TR duals "
           "are internal -- qp_problem.h)";
}

// ---------------------------------------------------------------------------
// Boundary refusals and instrumentation
// ---------------------------------------------------------------------------

TEST(IpqpBoundaryTest, AWarmSeedIsRefusedRatherThanSilentlyIgnored) {
    const QpProblem qp = box_qp(-1.0, 1.0);
    IpqpSeed seed;
    seed.x = vec({0.0, 0.0});
    IpqpEngine tier(tight_opts());
    EXPECT_THROW(tier.solve(qp, &seed, IpqpOptions{}, SolveOverrides{}), std::invalid_argument);
}

TEST(IpqpBoundaryTest, MalformedOptionsAndOverridesAreRefused) {
    const QpProblem qp = box_qp(-1.0, 1.0);
    IpqpEngine tier(tight_opts());

    IpqpOptions bad;
    bad.ipqp_tau = 1.5;
    EXPECT_THROW(tier.solve(qp, nullptr, bad, SolveOverrides{}), std::invalid_argument);

    IpqpOptions bad2;
    bad2.ipqp_converge_slack = 0.5; // below 1: the carried T1 band.
    EXPECT_THROW(tier.solve(qp, nullptr, bad2, SolveOverrides{}), std::invalid_argument);

    IpqpOptions bad3;
    bad3.ipqp_rho_init = 1e-20; // below ipqp_reg_floor: the carried T1 band.
    EXPECT_THROW(tier.solve(qp, nullptr, bad3, SolveOverrides{}), std::invalid_argument);

    SolveOverrides ov;
    ov.tr_radius = -1.0;
    EXPECT_THROW(tier.solve(qp, nullptr, IpqpOptions{}, ov), std::invalid_argument);

    QpProblem malformed = qp;
    malformed.g = vec({1.0});
    EXPECT_THROW(tier.solve(malformed, nullptr, IpqpOptions{}, SolveOverrides{}),
                 std::invalid_argument);
}

TEST(IpqpBoundaryTest, TheLedgerRecordsOneRowPerSolveAndNoneForAThrow) {
    const QpProblem qp = general_qp(true, true);
    Ledger ledger;
    IpqpEngine tier(tight_opts());
    tier.attach_ledger(&ledger, "ipqp-");

    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(ledger.records().size(), 1u);
    EXPECT_EQ(ledger.records()[0].label, "ipqp-0");
    EXPECT_EQ(ledger.records()[0].status, r.status);
    EXPECT_EQ(ledger.records()[0].counters.minor_iters, r.counters.ipqp_iters);
    EXPECT_EQ(ledger.records()[0].counters.factorizations, r.counters.ipqp_factorizations);

    IpqpOptions bad;
    bad.ipqp_tau = 2.0;
    EXPECT_THROW(tier.solve(qp, nullptr, bad, SolveOverrides{}), std::invalid_argument);
    EXPECT_EQ(ledger.records().size(), 1u);

    const IpqpResult again = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(ledger.records().size(), 2u);
    // The throwing call did NOT consume a label -- QpEngine::attach_ledger's
    // own contract, kept here.
    EXPECT_EQ(ledger.records()[1].label, "ipqp-1");
    EXPECT_EQ(again.status, QpStatus::kOptimal);

    // FIX ROUND 1, I9: A DECLINE IS AN OUTCOME AND EMITS ITS ROW. It returns before
    // the solve loop, but it returns NORMALLY, and the contract is one row per
    // non-throwing solve -- a decline that consumed no label would stop the count.
    SolveOverrides pinned;
    pinned.tr_radius = 0.0;
    const IpqpResult declined = tier.solve(qp, nullptr, IpqpOptions{}, pinned);
    ASSERT_TRUE(declined.declined_pinned);
    ASSERT_EQ(ledger.records().size(), 3u);
    EXPECT_EQ(ledger.records()[2].label, "ipqp-2");
    EXPECT_EQ(ledger.records()[2].status, declined.status);
    // The tier never ran, so the projected work columns read 0 -- which is the
    // correct reading, not a missing measurement.
    EXPECT_EQ(ledger.records()[2].counters.minor_iters, 0);
    EXPECT_EQ(ledger.records()[2].counters.factorizations, 0);
}

TEST(IpqpCounterTest, TheRoutingAndWarmGroupsStayAtZeroBecauseTheyAreDriverScale) {
    // T4 FIX ROUND 1, CM3: the routing and warm counter groups are left untouched by
    // this ENGINE, group-wide. Still true after task 6 -- routing is written by the
    // DRIVER's chain and the warm group by task 7's seed path.
    const QpProblem qp = general_qp(true, true);
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);

    // ROUTING (task 6): the tier classifies the face and reports it, but it
    // never routes, never hands off to tier 3, and never retires itself.
    EXPECT_EQ(r.counters.ipqp_refine_accepted, 0);
    EXPECT_EQ(r.counters.ipqp_refine_refused, 0);
    EXPECT_EQ(r.counters.ipqp_to_refine, 0);
    EXPECT_EQ(r.counters.ipqp_to_ssn, 0);
    EXPECT_EQ(r.counters.ipqp_to_walk, 0);
    EXPECT_EQ(r.counters.ipqp_tier_retired_after, 0);

    // WARM (task 7): a cold solve pays no repair, adopts no payload mu, and
    // has no warm restart to abandon. Every one of these is structurally 0
    // until the seed path exists.
    EXPECT_EQ(r.counters.ipqp_restart_repairs, 0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_restart_shift_max, 0.0);
    EXPECT_EQ(r.counters.ipqp_mu_adopted, 0);
    EXPECT_EQ(r.counters.ipqp_warm_restart_abandoned, 0);

    // MUTATION NON-VACUITY for the group as a whole: the counters this task
    // DOES populate are nonzero on the same solve, so the zeros above are a
    // statement about scope and not about a struct nobody wrote to.
    EXPECT_GT(r.counters.ipqp_iters, 0);
    EXPECT_GT(r.counters.ipqp_factorizations, 0);
    EXPECT_GT(r.counters.ipqp_prox_center_updates, 0);
}

TEST(IpqpCounterTest, TheEscapeCensusCountsABudgetEscapeExactlyOnce) {
    // WAS `TheEscapeCensusStaysAtZeroBecauseItIsTaskFives` -- task 4's boundary pin,
    // replaced (not deleted) now that task 5 owns the census. The invariant is
    // unchanged; it now holds at 1 == 1 instead of trivially at 0 == 0.
    IpqpOptions io;
    io.ipqp_hard_iter_cap = 1;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(general_qp(true, true), nullptr, io, SolveOverrides{});
    ASSERT_EQ(r.escape_reason, IpqpEscape::kBudget);
    EXPECT_EQ(r.counters.ipqp_escapes, 1);
    EXPECT_EQ(r.counters.ipqp_escape_budget, 1);
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(r.counters));

    // NON-VACUITY: the same fixture with the cap lifted escapes nothing, so
    // the census above is a measurement of THIS solve and not a constant.
    IpqpEngine clean(tight_opts());
    const IpqpResult ok =
        clean.solve(general_qp(true, true), nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(ok.escape_reason, IpqpEscape::kNone);
    EXPECT_EQ(ok.counters.ipqp_escapes, 0);
    EXPECT_EQ(ok.counters.ipqp_escape_budget, 0);
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(ok.counters));
}

TEST(IpqpCounterTest, TheAlphaFloorsAreObservedAndStayInsideTheUnitInterval) {
    const QpProblem qp = general_qp(true, true);
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    ASSERT_GT(r.counters.ipqp_iters, 0);
    // The default is +infinity ("no step observed yet"), so a finite reading
    // here proves the min-fold ran at all.
    EXPECT_TRUE(std::isfinite(r.counters.ipqp_alpha_p_min));
    EXPECT_TRUE(std::isfinite(r.counters.ipqp_alpha_d_min));
    EXPECT_GT(r.counters.ipqp_alpha_p_min, 0.0);
    EXPECT_LE(r.counters.ipqp_alpha_p_min, 1.0);
    EXPECT_GT(r.counters.ipqp_alpha_d_min, 0.0);
    EXPECT_LE(r.counters.ipqp_alpha_d_min, 1.0);
}

TEST(IpqpCounterTest, TheGatedScheduleMovesAndAdvancesTheProximalCentre) {
    const QpProblem qp = general_qp(true, true);
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);
    // The decrease is GATED, so it fires only on measured contraction -- but
    // it must fire on a solve that converges, or the gate is unreachable.
    EXPECT_GT(r.counters.ipqp_reg_decreases, 0);
    EXPECT_EQ(r.counters.ipqp_prox_center_updates, r.counters.ipqp_reg_decreases);
    EXPECT_LT(r.rho, IpqpOptions{}.ipqp_rho_init);
    EXPECT_GE(r.rho, IpqpOptions{}.ipqp_reg_floor);
}

} // namespace hven::solvers
