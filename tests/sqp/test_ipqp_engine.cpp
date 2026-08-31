// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_ipqp_engine.cpp -- the M6 W1 task 4 pins for the
// interior-point QP tier's cold solve, its clamp-centred box, its domain gate
// and its (rho, delta) ladder.
//
// THE ORACLE IS THE WALK. `QpEngine` is the tier's cross-check on every convex
// fixture here: two independently written kernels, two independently written
// relative-KKT residual implementations (plan ruling 5 gave this tier its own
// on purpose), agreeing on the same point at the QP layer's own tolerances is
// a stronger statement than either kernel agreeing with a hand-computed
// answer.
//
// EVERY VALUE PIN IS SHOWN FALLIBLE. Each block below that asserts a counter
// or a classification also asserts a NEIGHBOURING fixture where the same
// assertion would fail, so a pin that silently stopped measuring anything is
// caught by its own partner rather than by a later reader.

#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>

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
/// `S^2`, and both constraint rows carry the same spread. `S = 1e6` is the
/// value the pins below use, so the Ruiz diagonal is far from 1 in every
/// block and a missing unscale-on-export could not possibly go unnoticed.
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

/// Codex's C0 fixture: ONE variable, no rows, ABSENT bounds, `g = 0`. The
/// origin is stationary on iteration 0, so the solve converges before section
/// 3.2's schedule has advanced even once and `rho_sched` is still
/// `ipqp_rho_init`. `c` is the whole Hessian: negative makes the problem
/// concave and unbounded, positive makes it a one-dimensional least squares.
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

    // MUTATION NON-VACUITY: a PLAIN-ORIGIN centre would give a different
    // window on this very problem, which is the silent disagreement with
    // refine_on_face that the clamp rule exists to prevent. Show that the two
    // rules genuinely differ here rather than asserting the equality of two
    // things that happen to coincide.
    // A plain-origin window would be [max(3, -1), min(7, 1)] = [3, 1] on
    // variable 0 and [max(-8, -1), min(-2, 1)] = [-1, -2] on variable 1: both
    // CROSSED, and both different from what the clamp rule produced.
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
    // FIX ROUND 1, CM2 / CLAUDE.md section 4. `make_ipqp_bounds` is PUBLIC and
    // takes an IpqpBox by reference, so a hand-built one -- task 6's routing
    // chain will build one, and this test does -- can present blocks of
    // different lengths. The count loop indexes `up_eff` with `lo_eff`'s
    // length, and Eigen's own assert is compiled out under NDEBUG, so without
    // an explicit guard this is an out-of-bounds READ in Release.
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

TEST(IpqpLadderTest, AnIndefiniteSubproblemArmsTheLadderAndDoesNotCertify) {
    QpProblem qp = box_qp(-10.0, 10.0);
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1000.0}});

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});

    // THE NON-VACUITY PARTNER of the pin above: the same three counters that
    // are structurally zero on a convex subproblem are nonzero here, so
    // "provably inert" is a measurement rather than an absence.
    EXPECT_GT(r.counters.ipqp_inertia_retries, 0);
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_last, r.counters.ipqp_rho_demanded_max);
    EXPECT_GT(r.counters.ipqp_iters_at_elevated_rho, 0);
    EXPECT_NE(r.status, QpStatus::kOptimal);

    // WHAT THIS FIXTURE DOES *NOT* CLAIM, stated because the shape of the
    // outcome is worth pinning even though nothing certifies it. Spec 2.2
    // RETRACTS any convergence claim for a nonconvex QP -- the proximal terms
    // do not convexify it, and the inertia-demanded shift is a MODIFICATION,
    // not part of the proximal problem the section 3.2 gate measures. So the
    // tier converges to the modified problem, the gate (correctly, at
    // `rho_sched`) sees no contraction on the real one, and the solve runs out
    // its iteration budget. That is the honest outcome and section 2.3 routes
    // it onward; the early-stall test of section 6.2 (task 5) is what will
    // shorten the 60 iterations to ~5.
    EXPECT_EQ(r.escape_reason, IpqpEscape::kBudget);
    // ... and the required final read was never paid, because no certifying
    // exit was reached. `ipqp_final_inertia_read` is structurally 0 there --
    // its own doc comment says so, and this is that statement exercised.
    EXPECT_EQ(r.counters.ipqp_final_inertia_read, 0);
}

TEST(IpqpLadderTest, TheMonotoneFloorRefusesADecreaseAndCountsItAsAFlap) {
    // A MILDLY indefinite Hessian and a SMALL `rho_0`, chosen so the ladder
    // fires early (raising the monotone floor to 1) and the section 3.2 gate
    // then earns a decrease the floor must refuse. Both halves matter: without
    // the ladder there is no floor to violate, and without the gate firing
    // there is no decrease to refuse.
    QpProblem qp = box_qp(-10.0, 10.0);
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1.0}});
    IpqpOptions io;
    io.ipqp_rho_init = 1.0e-2;

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, io, SolveOverrides{});

    EXPECT_EQ(r.counters.ipqp_rho_flaps, 1);
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0);
    // A FLAP AND ONLY A FLAP (fix round 2, I2). `delta` carries no monotone
    // floor (plan section 7 note (g)), so the very advance whose `rho` move
    // the floor refused still moved `delta` 8 -> 0.8 -- and the rule chosen,
    // stated on both counters' doc comments, is that ONE ADVANCE IS ONE
    // CLASSIFICATION: the refusal is the defining event, so this advance
    // scores a flap and NOT also a decrease. Round 1 counted both and broke
    // the exclusivity the two fields document.
    EXPECT_EQ(r.counters.ipqp_reg_decreases, 0);
    // THE IDENTITY, structural on any solve whose schedule has not bottomed
    // out: every gated advance is classified exactly once.
    EXPECT_EQ(r.counters.ipqp_prox_center_updates,
              r.counters.ipqp_reg_decreases + r.counters.ipqp_rho_flaps);

    // MUTATION NON-VACUITY: the same solve on a CONVEX Hessian raises no
    // floor, so the same gate produces decreases and no flaps at all.
    QpProblem convex = box_qp(-10.0, 10.0);
    IpqpEngine tier2(tight_opts());
    const IpqpResult c = tier2.solve(convex, nullptr, io, SolveOverrides{});
    ASSERT_EQ(c.status, QpStatus::kOptimal);
    EXPECT_EQ(c.counters.ipqp_rho_flaps, 0);
    EXPECT_GT(c.counters.ipqp_reg_decreases, 0);
    EXPECT_EQ(c.counters.ipqp_prox_center_updates,
              c.counters.ipqp_reg_decreases + c.counters.ipqp_rho_flaps);
}

TEST(IpqpLadderTest, TheABSOLUTEFloorIsNotAFlapAndDoesNotCoFireWithADecrease) {
    // FIX ROUND 1, I2. The flap counter reads the MONOTONE (inertia-demanded)
    // floor only. Reading `max(reg_floor, rho_floor)` instead made the
    // ABSOLUTE floor -- a setting every schedule decays onto -- look like a
    // down-then-up cycle on a solve that never had a monotone floor, and it
    // co-fired with the `ipqp_reg_decreases` the field's doc comment excludes.
    //
    // At the shipped defaults that state is only reached past the 11th gated
    // advance, which no fixture in this file gets to. Rather than build a
    // fixture long enough to stumble into it, this one starts the schedule
    // two advances above the floor, so BOTH quantities sit on the absolute
    // floor for the rest of a perfectly ordinary convex solve.
    IpqpOptions io;
    io.ipqp_rho_init = 1.0e-9;   // reg_floor * 10
    io.ipqp_delta_init = 1.0e-9; // ... and the dual side with it
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(general_qp(true, true), nullptr, io, SolveOverrides{});

    ASSERT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_DOUBLE_EQ(r.rho, io.ipqp_reg_floor);
    EXPECT_DOUBLE_EQ(r.delta, io.ipqp_reg_floor);
    // A CONVEX subproblem has no monotone floor at all, so no advance here can
    // be a monotone-floor violation however many hit the absolute one. This is
    // I2's whole content.
    EXPECT_EQ(r.counters.ipqp_rho_flaps, 0);
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, 0.0);

    // THE EXACT THREE-WAY ACCOUNTING, pinned as three numbers rather than as
    // an inequality (fix round 2: "do not pin a gap"). Four gated advances:
    // the first two still had room to move -- `1e-9 * 0.1` does not land on
    // `1e-10` exactly in binary, so the schedule takes two steps to settle on
    // the floor -- and the last two moved NOTHING, which is class (c): neither
    // a decrease (nothing moved) nor a flap (the ABSOLUTE floor is a setting,
    // not evidence about this subproblem's curvature).
    EXPECT_EQ(r.counters.ipqp_prox_center_updates, 4);
    EXPECT_EQ(r.counters.ipqp_reg_decreases, 2);
    EXPECT_EQ(r.counters.ipqp_rho_flaps, 0);
    // ... so the identity's residual IS the class-(c) count, and it is 2 here.
    // The section 7 counter table has no field for that class and this task
    // does not invent one, so it is pinned by arithmetic on the three that do
    // exist rather than left unstated.
    EXPECT_EQ(r.counters.ipqp_prox_center_updates - r.counters.ipqp_reg_decreases -
                  r.counters.ipqp_rho_flaps,
              2);
}

TEST(IpqpLadderTest, TheIdentityHoldsOnALongConvexSolveWithNoMonotoneFloor) {
    // THE SECOND, INDEPENDENT WITNESS for I2, run at a tight tolerance and a
    // raised cap so the solve takes materially more iterations than any other
    // fixture in this file (13 against the usual 5-11) and the section 3.2
    // gate gets many more chances to fire.
    //
    // WHY THIS IS NOT THE "PAST THE 11TH GATED ADVANCE" FIXTURE, said plainly:
    // the gate is a CONTRACTION test, so gated advances are far rarer than
    // iterations -- this solve takes 13 iterations and 5 advances. Reaching a
    // 12th advance on a convex fixture is not a matter of running longer; it
    // is a matter of the residual halving twelve times before convergence,
    // which at these tolerances does not happen. THE STATE that a past-11
    // fixture was a proxy for -- both quantities parked on the absolute floor
    // with the gate still firing -- is reached deterministically by the
    // fixture above instead, by starting the schedule two steps from the floor
    // rather than eleven. The state is what the pin is about; the trip length
    // is not.
    IpqpOptions io;
    io.ipqp_converge_slack = 1.0; // the tightest the band allows
    io.ipqp_hard_iter_cap = 400;
    io.ipqp_min_mu = 1e-16;

    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(general_qp(true, true), nullptr, io, SolveOverrides{});

    ASSERT_EQ(r.status, QpStatus::kOptimal);
    ASSERT_GT(r.counters.ipqp_iters, 11);
    ASSERT_GT(r.counters.ipqp_prox_center_updates, 0);
    EXPECT_EQ(r.counters.ipqp_rho_flaps, 0);
    EXPECT_EQ(r.counters.ipqp_prox_center_updates,
              r.counters.ipqp_reg_decreases + r.counters.ipqp_rho_flaps);
}

TEST(IpqpLadderTest, TheFinalReadCatchesASaddleTheLadderWouldOtherwiseCertify) {
    // THE CERTIFICATION READ IS ONE FACTORIZATION, NOT A LADDER, and this is
    // the fixture that says why. H = diag(2, -1) with g = 0 and a SYMMETRIC
    // box makes the origin an exact KKT point of the barrier problem at every
    // mu -- the two bound multipliers balance -- so the tier converges to it
    // in three iterations without ever moving x. It is a SADDLE.
    //
    // The section 3.2 schedule has decayed `rho` to 0.8 by then, and the
    // in-loop ladder has raised the working `rho` to 80, at which
    // `H + rho I + Sigma_b` is positive definite and the inertia reads
    // correct. A certification read that CLIMBED the same ladder would
    // therefore find the inertia it was looking for and report the
    // certificate as standing -- a saddle point certified as a minimum, which
    // is exactly the wrong-answer class ssn_engine.h:24 warns about for its
    // own kernel. Dropping to the SCHEDULE'S level and reading ONCE is what
    // catches it.
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

    // FIX ROUND 1, I4: AN EXACT COUNT, not `> 0`. Every one of this
    // fixture's three iterations takes its step at the inertia-demanded
    // floor rather than at the schedule's own level, so the counter must
    // equal the iteration count. Sampling it BEFORE the ladder ran -- the
    // earlier code -- returned 2, missing the iteration whose own ladder
    // first raised the floor, which is exactly the off-by-one this pin
    // exists to hold down.
    // FIX ROUND 2, N1: STILL 3, and the reason is worth recording rather
    // than leaving the unchanged number to look like an oversight. N1 moved
    // the increment behind the step, so only iterations that actually complete
    // are counted -- and on this fixture all three armed iterations do
    // complete, so the count is unmoved. The fixture where the two readings
    // DIVERGE is the cap-1 budget one, which arms the ladder and then takes no
    // step at all: see
    // `TheFactorizationCapIsCheckedBeforeEVERYFactorizationLadderRungsIncluded`.
    EXPECT_EQ(r.counters.ipqp_iters, 3);
    EXPECT_EQ(r.counters.ipqp_iters_at_elevated_rho, 3);
    EXPECT_EQ(r.counters.ipqp_iters_at_elevated_rho, r.counters.ipqp_iters);
    // The gated advances are classified exactly once each (I2).
    EXPECT_EQ(r.counters.ipqp_prox_center_updates,
              r.counters.ipqp_reg_decreases + r.counters.ipqp_rho_flaps);
    // Three iterations, one ladder rung, one certification read.
    EXPECT_EQ(r.counters.ipqp_factorizations, 5);
    EXPECT_EQ(r.counters.ipqp_inertia_retries, 1);

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
    // FIX ROUND 1, C0 (Codex critical; settler ruling on section 2.2 item 4's
    // "the schedule's residual level" = the level the schedule DECAYS TO,
    // i.e. `ipqp_reg_floor`).
    //
    // H = [-1], g = 0, no rows, no bounds. The origin is stationary on
    // ITERATION 0 -- every residual is exactly zero there -- so the solve
    // converges before the section 3.2 gate has advanced the schedule even
    // once, and `rho_sched` is still `ipqp_rho_init` = 8. Reading the
    // certificate off `H + rho_sched I` = [7] finds the target inertia
    // (1, 0, 0) and certifies a CONCAVE, UNBOUNDED problem as optimal at its
    // MAXIMUM. Dropping to the floor instead reads [-1 + 1e-10] and catches
    // it.
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

    // MUTATION NON-VACUITY: the convex twin reaches the same final read by the
    // same route -- converged at iteration 0, schedule untouched -- and
    // CERTIFIES. So the pin above is about the curvature, not about the
    // fixture being degenerate.
    IpqpEngine tier2(tight_opts());
    const IpqpResult c = tier2.solve(free_scalar_qp(1.0), nullptr, IpqpOptions{}, SolveOverrides{});
    EXPECT_EQ(c.counters.ipqp_iters, 0);
    EXPECT_EQ(c.counters.ipqp_final_inertia_read, 0);
    EXPECT_FALSE(c.certificate_downgraded);
    EXPECT_EQ(c.status, QpStatus::kOptimal);
    EXPECT_EQ(c.counters.ipqp_factorizations, 1);
}

TEST(IpqpBudgetTest, TheFactorizationCapIsCheckedBeforeEVERYFactorizationLadderRungsIncluded) {
    // FIX ROUND 1, I6. The cap used to be consulted once per iteration, so a
    // single ladder could outrun it: measured at cap 1 on a strongly
    // indefinite Hessian, the first ladder paid THREE factorizations before
    // anything stopped it. A cap that a ladder can walk through is not a cap.
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
    // FIX ROUND 2, N1, AND THIS IS ITS DISCRIMINATING FIXTURE. The one
    // factorization this solve paid read WRONG, so the ladder raised `rho` to
    // 800 -- elevated by any reading of the word -- and the cap then refused
    // the rung's own factorization. No step was taken. The counter says
    // "iterations TAKEN", and there were none: this must be 0. Counting it
    // where round 1 did (right after the ladder settles, before the budget and
    // terminal-read rejections) returned 1 for an iteration that never
    // happened.
    EXPECT_EQ(r.counters.ipqp_iters_at_elevated_rho, 0);
    EXPECT_GT(r.counters.ipqp_rho_demanded_max, 0.0); // the ladder DID arm.

    // MUTATION NON-VACUITY: the same fixture with the cap lifted really does
    // want more than one factorization, so the pin above is measuring the cap
    // rather than a solve that happened to need one.
    IpqpOptions loose;
    IpqpEngine tier2(tight_opts());
    const IpqpResult r2 = tier2.solve(qp, nullptr, loose, SolveOverrides{});
    EXPECT_GT(r2.counters.ipqp_factorizations, 1);
    EXPECT_EQ(r2.escape_reason, IpqpEscape::kIndefinite);
}

TEST(IpqpLadderTest, AnExhaustedLadderStopsAtTheCeilingExactlyAndReportsIndefinite) {
    QpProblem qp = box_qp(-10.0, 10.0);
    // Curvature no regularization below the 1e6 ceiling can dominate.
    qp.H = dense_upper({{2.0, 0.0}, {0.0, -1.0e12}});

    IpqpOptions io;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, io, SolveOverrides{});

    EXPECT_EQ(r.escape_reason, IpqpEscape::kIndefinite);
    EXPECT_EQ(r.status, QpStatus::kNumericalError);
    // THE CAP-SLACK LESSON, INHERITED AS A FIX AND NOT ONLY AS A CONSTANT
    // (spec 3.2, Amendment G): repeated multiplication by 100 lands on
    // 999999.9999999998 rather than on 1e6, so an exact `>=` ceiling test
    // grants one more rung -- a whole numeric factorization -- for a 2.3e-10
    // relative increase. The relative guard closes it, and the top rung is
    // then the DOCUMENTED ceiling exactly, which is what this asserts.
    EXPECT_DOUBLE_EQ(r.counters.ipqp_rho_demanded_max, io.ipqp_reg_max);
    EXPECT_GE(r.counters.ipqp_rho_demanded_max, io.ipqp_reg_max * (1.0 - detail::kSsnProxCapSlack));
}

// ---------------------------------------------------------------------------
// A9 -- the verify-once discipline
// ---------------------------------------------------------------------------

TEST(IpqpFactorizationTest, EachTierEntryPaysExactlyOneSymbolicPassOrOnePatternVerify) {
    const QpProblem qp = general_qp(true, true);
    IpqpEngine tier(tight_opts());

    const IpqpResult first = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(first.status, QpStatus::kOptimal);
    // The entry that LAYS OUT the pattern pays the analysis; compute() does
    // not verify (kkt_factorization.cpp), so its verify count is 0. That is
    // plan section 7 note (a)'s own premise, applied to the one entry the
    // note's "+1" cannot describe.
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
    // FIX ROUND 1, I5 (settler ruling): a DOWNGRADE, NOT AN ESCAPE. Turning
    // the read off to save one factorization must not manufacture a census
    // entry and a section 6.1 K = 3 retirement charge on a solve that
    // converged cleanly. `3` is the distinct "not performed (option off)"
    // value, kept apart from `2` (attempted, evidence unusable).
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
    // FIX ROUND 2, N2 (settler ruling). The 0/1/2/3 contract defines `2` as
    // ATTEMPTED-and-unusable, which is why it maps to the numerical class. A
    // certification factorization the cap refused was never attempted, so it
    // belongs with the option-off case: `3`, not performed.
    //
    // The cap is calibrated from the solve itself rather than hard-coded, so
    // the fixture stays honest if the trajectory ever moves: run once at the
    // defaults to learn the iteration count, then re-run with exactly that
    // many factorizations -- enough for every iteration, one short of the
    // final read.
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
    // EVERY PIN IS ON UNSCALED QUANTITIES (spec 4.3, non-negotiable). This is
    // the pin's own fallibility argument, not a restatement of it: the
    // equilibration diagonal on this fixture spans six decades in every block,
    // so a right-hand side scaled going in and NOT unscaled coming out --
    // the one mistake the round trip can make -- would move the returned
    // point by those six decades. Landing on the walk's own answer is only
    // possible if the round trip is exact.
    EXPECT_LT((ra.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-7);
    EXPECT_LT((rb.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-7);
    EXPECT_LT((ra.x - rb.x).lpNorm<Eigen::Infinity>(), 1e-9);
    // ANSWER-NEUTRALITY IS THE CLAIM, AND IT IS WHAT WAS MEASURED: on every
    // fixture in this file the two arms take the same number of iterations and
    // the same number of factorizations, differing only in the last digit or
    // two of the residual. That is section 4.3's own statement ("it changes
    // nothing the SQP sees") observed rather than assumed. A fixture on which
    // the equilibration changes the TRAJECTORY has not been found at this
    // scale; task 9's acceptance battery, which runs real collocation-sized
    // subproblems, is where one would show up.
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
    // A DOCUMENTED LIMIT, PINNED SO A CHANGE TO IT IS VISIBLE -- not a bug.
    // The residual contract (plan ruling 5) folds ONE `max(1, ...)` scale over
    // the whole stationarity residual, which is exactly the discipline
    // detail::free_block_stationarity models. On a subproblem whose Hessian
    // blocks span twelve decades that fold is dominated by the LARGE block, so
    // a coordinate with tiny curvature can stop far from its optimum in
    // ABSOLUTE terms while the relative test reads converged.
    //
    // Measured here: the tier certifies at x2 = 5.4e-5 where the answer is
    // ~1.0. The certificate is not false -- the objective is within 3e-12
    // RELATIVE of optimal -- and section 2.3's routing chain is what closes
    // the gap: the tier converges to `ipqp_converge_slack x` the QP
    // tolerances and hands the face to the exact tier-3 refinement, which owns
    // the last two decades. This test pins the SHAPE of that hand-off's input,
    // so a future change to the fold (a per-block scale is the obvious
    // candidate, and it is a SPEC change, not a code change) shows up here.
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

    // FIX ROUND 1, I9: A DECLINE IS AN OUTCOME AND EMITS ITS ROW. It returns
    // before the solve loop, but it returns NORMALLY, and the contract is one
    // row per non-throwing solve -- task 6's routing chain wants to see the
    // subproblems the tier refused just as much as the ones it solved, and a
    // decline that consumed no label would make the labels stop counting
    // solves.
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

TEST(IpqpCounterTest, TheRoutingAndWarmGroupsStayAtZeroBecauseTheyAreTasksSixAndSeven) {
    // FIX ROUND 1, CM3. The report claims the routing and warm counter groups
    // are left untouched by this task; only the escape census had an
    // executable pin for it. This is that claim, group-wide -- so a later task
    // that starts writing one of these fields without moving its own pins
    // fails here rather than in a sweep column nobody is watching.
    const QpProblem qp = general_qp(true, true);
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{});
    ASSERT_EQ(r.status, QpStatus::kOptimal);

    // ROUTING (task 6): the tier classifies the face and reports it, but it
    // never routes, never hands off to tier 3, and never retires itself.
    EXPECT_EQ(r.counters.ipqp_refine_accepted, 0);
    EXPECT_EQ(r.counters.ipqp_refine_refused, 0);
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

TEST(IpqpCounterTest, TheEscapeCensusStaysAtZeroBecauseItIsTaskFives) {
    // Stated as a pin rather than left implicit: this task classifies the
    // escape REASON and leaves the six census counters at 0, which keeps the
    // sum-to-`ipqp_escapes` invariant TRUE rather than half-populated.
    IpqpOptions io;
    io.ipqp_hard_iter_cap = 1;
    IpqpEngine tier(tight_opts());
    const IpqpResult r = tier.solve(general_qp(true, true), nullptr, io, SolveOverrides{});
    ASSERT_EQ(r.escape_reason, IpqpEscape::kBudget);
    EXPECT_EQ(r.counters.ipqp_escapes, 0);
    EXPECT_EQ(r.counters.ipqp_escape_budget + r.counters.ipqp_escape_stall +
                  r.counters.ipqp_escape_indefinite + r.counters.ipqp_escape_numerical +
                  r.counters.ipqp_escape_infeasible_suspect,
              r.counters.ipqp_escapes);
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
    EXPECT_EQ(r.counters.ipqp_prox_center_updates,
              r.counters.ipqp_reg_decreases + r.counters.ipqp_rho_flaps);
    EXPECT_LT(r.rho, IpqpOptions{}.ipqp_rho_init);
    EXPECT_GE(r.rho, IpqpOptions{}.ipqp_reg_floor);
}

} // namespace hven::solvers
