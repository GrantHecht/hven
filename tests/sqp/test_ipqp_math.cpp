// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The interior-point QP tier's barrier kernels (M6 W1 T3.b, ipqp_math.h).
//
// Each kernel in ipqp_math.h claims to MIRROR a named kernel in
// barrier_math.h -- same arithmetic, QP shape instead of the NLP engine's
// reduced-space BoundSet. That claim is testable, so it is tested: every pin
// below builds the BoundSet/BoundDualState the QP data is equivalent to, runs
// BOTH kernels, and requires them to agree to within 4 ULP (EXPECT_DOUBLE_EQ)
// -- close enough that a changed FORMULA fails, loose enough that the build's
// fast-math regime is allowed to reassociate two differently-shaped loops over
// identical arithmetic. The kernels state their accumulation order anyway
// (lowers then uppers, ascending index, matching the NLP kernels' list walk),
// so the mirror claim is about the arithmetic, not about a tolerance absorbing
// a real difference: a dropped damping term, a sign, or a missing bound class
// moves these by far more than 4 ULP, and each pin is paired with a
// non-vacuity check that proves it can move at all.
//
// The exception is the complementarity reduction, which has no mirror on
// purpose (barrier_math.h's banner: the NLP reduction's .sum() order feeds mu
// and is ULP-load-bearing). It is pinned against a hand-computed value
// instead.
//
// Every pin is written so that it can fail: each is paired with a
// non-vacuity check that perturbs an input and requires the result to move.

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include "hven/core/types.h"
#include "hven/detail/interior/barrier_math.h"
#include "hven/detail/interior/bound_set.h"
#include "hven/detail/qp/ipqp_math.h"
#include "hven/detail/qp/qp_engine.h"
#include "hven/detail/qp/ssn_engine.h"

namespace {

using hven::Index;
using hven::solvers::BoundDualState;
using hven::solvers::BoundSet;

namespace ip = hven::solvers::detail;

constexpr double kInf = 1e20;

// The fixture: four variables covering every bound class the tier can meet.
//   0  two-sided        -> no damping on either side
//   1  lower-only       -> damped on the lower side
//   2  upper-only       -> damped on the upper side
//   3  free             -> no barrier term at all
struct Bounds {
    Eigen::VectorXd x, l, u, zl, zu;
    Index n = 4;
};

Bounds fixture() {
    Bounds b;
    b.x.resize(4);
    b.l.resize(4);
    b.u.resize(4);
    b.zl.resize(4);
    b.zu.resize(4);
    b.x << 0.5, 2.25, -1.5, 7.0;
    b.l << -1.0, 1.0, -kInf, -kInf;
    b.u << 3.0, kInf, 0.75, kInf;
    // Zero at an absent bound, which is the dense shape's convention.
    b.zl << 0.4, 1.25, 0.0, 0.0;
    b.zu << 0.9, 0.0, 2.5, 0.0;
    return b;
}

// The same bounds in the NLP engine's shape: two index/value lists in
// ascending variable order, with the damping indicator materialized.
BoundSet bound_set_of(const Bounds &b) {
    std::vector<int> li, ui;
    std::vector<double> lv, uv, ld, ud;
    for (Index i = 0; i < b.n; ++i) {
        const bool has_l = b.l[i] > -kInf;
        const bool has_u = b.u[i] < kInf;
        if (has_l) {
            li.push_back(static_cast<int>(i));
            lv.push_back(b.l[i]);
            ld.push_back(has_u ? 0.0 : 1.0);
        }
        if (has_u) {
            ui.push_back(static_cast<int>(i));
            uv.push_back(b.u[i]);
            ud.push_back(has_l ? 0.0 : 1.0);
        }
    }
    BoundSet s;
    s.lower_idx_.resize(static_cast<Index>(li.size()));
    s.lower_val_.resize(static_cast<Index>(lv.size()));
    s.lower_damp_.resize(static_cast<Index>(ld.size()));
    for (std::size_t k = 0; k < li.size(); ++k) {
        s.lower_idx_[static_cast<Index>(k)] = li[k];
        s.lower_val_[static_cast<Index>(k)] = lv[k];
        s.lower_damp_[static_cast<Index>(k)] = ld[k];
    }
    s.upper_idx_.resize(static_cast<Index>(ui.size()));
    s.upper_val_.resize(static_cast<Index>(uv.size()));
    s.upper_damp_.resize(static_cast<Index>(ud.size()));
    for (std::size_t k = 0; k < ui.size(); ++k) {
        s.upper_idx_[static_cast<Index>(k)] = ui[k];
        s.upper_val_[static_cast<Index>(k)] = uv[k];
        s.upper_damp_[static_cast<Index>(k)] = ud[k];
    }
    return s;
}

BoundDualState dual_state_of(const Bounds &b, const BoundSet &s) {
    BoundDualState z;
    z.z_lower_.resize(s.lower_idx_.size());
    z.z_upper_.resize(s.upper_idx_.size());
    for (Index k = 0; k < s.lower_idx_.size(); ++k) {
        z.z_lower_[k] = b.zl[s.lower_idx_[k]];
    }
    for (Index k = 0; k < s.upper_idx_.size(); ++k) {
        z.z_upper_[k] = b.zu[s.upper_idx_[k]];
    }
    return z;
}

constexpr double kMu = 0.125;

// Bit-level comparison, for the one pin whose whole point is that two
// groupings of the same arithmetic round differently: EXPECT_DOUBLE_EQ (4 ULP)
// and even EXPECT_EQ would pass values this test has to separate.
std::uint64_t bits(double v) { return std::bit_cast<std::uint64_t>(v); }

// ---------------------------------------------------------------------------
// The five mirrors.
// ---------------------------------------------------------------------------

TEST(IpqpMathTest, BoundBarrierObjectiveMirrorsTheNlpKernel) {
    Bounds b = fixture();
    const BoundSet s = bound_set_of(b);

    const double got = ip::ipqp_bound_barrier_objective(b.x, b.l, b.u, kMu, b.n);
    const double want = ip::bound_barrier_objective(b.x, s, kMu);

    EXPECT_DOUBLE_EQ(got, want);

    // And against arithmetic, in the kernel's own order -- lowers ascending,
    // then uppers -- so the pin does not rest on two implementations agreeing
    // with each other. Distances: var0 lower 1.5, var1 lower 1.25, var0 upper
    // 2.5, var2 upper 2.25; var3 is free. Damping rides the two one-sided
    // entries only.
    const double undamped =
        -kMu * std::log(1.5) + -kMu * std::log(1.25) + -kMu * std::log(2.5) + -kMu * std::log(2.25);
    const double damping = ip::kIpqpKappaD * kMu * 1.25 + ip::kIpqpKappaD * kMu * 2.25;
    EXPECT_NEAR(got, undamped + damping, 1e-14);

    // THE DAMPING PIN. Making both one-sided variables two-sided -- with the
    // second bound placed so its own log term is known and subtracted off --
    // removes exactly the damping and nothing else. A kernel that dropped
    // kappa_d would make these two agree.
    Bounds two_sided = b;
    two_sided.u[1] = 2.25 + 1.25; // var1 gains an upper bound at distance 1.25
    two_sided.l[2] = -1.5 - 2.25; // var2 gains a lower bound at distance 2.25
    const double two_sided_psi =
        ip::ipqp_bound_barrier_objective(two_sided.x, two_sided.l, two_sided.u, kMu, two_sided.n);
    const double added_logs = -kMu * std::log(1.25) + -kMu * std::log(2.25);
    EXPECT_NEAR(got - (two_sided_psi - added_logs), damping, 1e-14);
    EXPECT_GT(damping, 0.0);
}

TEST(IpqpMathTest, BoundBarrierObjectiveMovesWhenAnInputMoves) {
    Bounds b = fixture();
    const double base = ip::ipqp_bound_barrier_objective(b.x, b.l, b.u, kMu, b.n);

    b.x[2] = -1.75; // further from its upper bound
    EXPECT_NE(ip::ipqp_bound_barrier_objective(b.x, b.l, b.u, kMu, b.n), base);

    // And a variable that is FREE contributes nothing, so moving it does not.
    Bounds c = fixture();
    c.x[3] = -400.0;
    EXPECT_EQ(ip::ipqp_bound_barrier_objective(c.x, c.l, c.u, kMu, c.n), base);
}

TEST(IpqpMathTest, BoundBarrierGradientMirrorsTheNlpKernel) {
    const Bounds b = fixture();
    const BoundSet s = bound_set_of(b);

    // Both accumulate onto a nonzero base, which is how the callers use them
    // -- so the pin also covers that neither overwrites.
    Eigen::VectorXd base(4);
    base << 1.0, -2.0, 0.5, 3.0;
    Eigen::VectorXd got = base;
    Eigen::VectorXd want = base;

    ip::ipqp_accumulate_bound_barrier_gradient(b.x, b.l, b.u, kMu, b.n, got);
    ip::accumulate_bound_barrier_gradient(b.x, s, kMu, want);

    for (Index i = 0; i < b.n; ++i) {
        EXPECT_DOUBLE_EQ(got[i], want[i]) << "component " << i;
    }
    // Non-vacuity: the kernel actually wrote to the bounded components and
    // left the free one alone.
    EXPECT_NE(got[0], base[0]);
    EXPECT_NE(got[1], base[1]);
    EXPECT_NE(got[2], base[2]);
    EXPECT_EQ(got[3], base[3]);
}

TEST(IpqpMathTest, BoundDualTermsMirrorTheNlpKernel) {
    const Bounds b = fixture();
    const BoundSet s = bound_set_of(b);
    const BoundDualState z = dual_state_of(b, s);

    Eigen::VectorXd base(4);
    base << 1.0, -2.0, 0.5, 3.0;
    Eigen::VectorXd got = base;
    Eigen::VectorXd want = base;

    ip::ipqp_accumulate_bound_dual_terms(b.l, b.u, b.zl, b.zu, b.n, got);
    ip::accumulate_bound_dual_terms(s, z, want);

    for (Index i = 0; i < b.n; ++i) {
        EXPECT_DOUBLE_EQ(got[i], want[i]) << "component " << i;
    }
    EXPECT_NE(got[0], base[0]);
    EXPECT_EQ(got[3], base[3]);

    // The z-form is UNDAMPED, and therefore differs from the mu-form at a
    // point off the central path -- the reason both kernels exist.
    Eigen::VectorXd mu_form = base;
    ip::ipqp_accumulate_bound_barrier_gradient(b.x, b.l, b.u, kMu, b.n, mu_form);
    EXPECT_NE(mu_form[1], got[1]);
}

// I2(a). The kernel's grouping is part of the mirror. `gx += (zU - zL)` and
// `(gx + (-zL)) + zU` are not the same computation: at gx = 1e16, zL = 1e16,
// zU = 1 the subtraction 1 - 1e16 rounds back to -1e16 and the fused form
// returns 0 where the mirror returns 1. A dual-residual norm built from this
// kernel feeds a convergence decision, so this pin compares BITS -- 0 and 1
// are 4 ULP apart nowhere, but a 2-ULP regrouping would slip past
// EXPECT_DOUBLE_EQ, which is how the first round's version of this kernel got
// through.
TEST(IpqpMathTest, BoundDualTermsReproduceTheMirrorsGroupingBitForBit) {
    Bounds b;
    b.n = 1;
    b.x.resize(1);
    b.l.resize(1);
    b.u.resize(1);
    b.zl.resize(1);
    b.zu.resize(1);
    b.x << 0.5;
    b.l << -1.0; // two-sided, so BOTH terms land on the same accumulator
    b.u << 3.0;
    b.zl << 1e16;
    b.zu << 1.0;

    const BoundSet s = bound_set_of(b);
    const BoundDualState z = dual_state_of(b, s);

    Eigen::VectorXd got(1), want(1);
    got << 1e16;
    want << 1e16;
    ip::ipqp_accumulate_bound_dual_terms(b.l, b.u, b.zl, b.zu, b.n, got);
    ip::accumulate_bound_dual_terms(s, z, want);

    EXPECT_EQ(bits(got[0]), bits(want[0]));
    // And what that value IS, so the pin does not rest on two implementations
    // agreeing: (1e16 - 1e16) + 1 == 1.
    EXPECT_EQ(bits(got[0]), bits(1.0));
    // The grouping this kernel must NOT have. Stated as an expression rather
    // than described, so the pin names the defect it excludes.
    EXPECT_NE(bits(got[0]), bits(1e16 + (b.zu[0] - b.zl[0])));
}

// I2(b). The one kernel in this file whose NLP mirror is structurally immune
// to a stale multiplier -- it walks index lists, so an unbounded variable is
// unreachable. A dense loop is not immune unless it asks l/u, and T4.b's
// IpqpBounds may well hand through a reused buffer.
TEST(IpqpMathTest, BoundDualTermsIgnoreWhateverSitsAtAnAbsentBound) {
    Bounds b = fixture();
    // var3 is free; var1 has no upper bound; var2 has no lower bound. Fill
    // every absent slot with a value that would be impossible to miss.
    b.zl[2] = -1e300;
    b.zl[3] = 7e11;
    b.zu[1] = 3e299;
    b.zu[3] = -5e10;

    const BoundSet s = bound_set_of(b);
    const BoundDualState z = dual_state_of(b, s);

    Eigen::VectorXd base(4);
    base << 1.0, -2.0, 0.5, 3.0;
    Eigen::VectorXd got = base;
    Eigen::VectorXd want = base;
    ip::ipqp_accumulate_bound_dual_terms(b.l, b.u, b.zl, b.zu, b.n, got);
    ip::accumulate_bound_dual_terms(s, z, want);

    for (Index i = 0; i < b.n; ++i) {
        EXPECT_EQ(bits(got[i]), bits(want[i])) << "component " << i;
    }
    // The free variable in particular is untouched, not merely close.
    EXPECT_EQ(bits(got[3]), bits(base[3]));
}

TEST(IpqpMathTest, BoundSigmaMirrorsTheNlpKernel) {
    const Bounds b = fixture();
    const BoundSet s = bound_set_of(b);
    const BoundDualState z = dual_state_of(b, s);

    Eigen::VectorXd base(4);
    base << 8.0, 8.0, 8.0, 8.0; // the proximal term rho the tier already wrote
    Eigen::VectorXd got = base;
    Eigen::VectorXd want = base;

    ip::ipqp_accumulate_bound_sigma(b.x, b.l, b.u, b.zl, b.zu, b.n, got);
    ip::accumulate_bound_sigma(b.x, s, z, want);

    for (Index i = 0; i < b.n; ++i) {
        EXPECT_DOUBLE_EQ(got[i], want[i]) << "component " << i;
    }
    // Accumulated onto the base rather than replacing it, and undamped.
    EXPECT_GT(got[0], base[0]);
    EXPECT_EQ(got[3], base[3]);
}

TEST(IpqpMathTest, AugmentBoundComplementarityMirrorsTheNlpKernel) {
    const Bounds b = fixture();
    const BoundSet s = bound_set_of(b);
    const BoundDualState z = dual_state_of(b, s);

    // A nonempty base, so the count-weighted fold is exercised rather than the
    // empty-base branch.
    const Index base_count = 3;
    double got_avg = 0.75, got_min = 0.25, got_max = 1.5;
    double want_avg = 0.75, want_min = 0.25, want_max = 1.5;

    ip::ipqp_augment_bound_complementarity(b.x, b.l, b.u, b.zl, b.zu, b.n, base_count, got_avg,
                                           got_min, got_max);
    ip::augment_bound_complementarity(b.x, s, z, static_cast<int>(base_count), want_avg, want_min,
                                      want_max);

    EXPECT_DOUBLE_EQ(got_avg, want_avg);
    EXPECT_DOUBLE_EQ(got_min, want_min);
    EXPECT_DOUBLE_EQ(got_max, want_max);
    // Non-vacuity: the fold moved the aggregates it was given.
    EXPECT_NE(got_avg, 0.75);

    // The empty-base branch, where the union aggregates are the bound
    // aggregates outright.
    double e_avg = 0.0, e_min = 0.0, e_max = 0.0;
    double w_avg = 0.0, w_min = 0.0, w_max = 0.0;
    ip::ipqp_augment_bound_complementarity(b.x, b.l, b.u, b.zl, b.zu, b.n, 0, e_avg, e_min, e_max);
    ip::augment_bound_complementarity(b.x, s, z, 0, w_avg, w_min, w_max);
    EXPECT_DOUBLE_EQ(e_avg, w_avg);
    EXPECT_DOUBLE_EQ(e_min, w_min);
    EXPECT_DOUBLE_EQ(e_max, w_max);
}

TEST(IpqpMathTest, AugmentBoundComplementarityLeavesAnUnboundedProblemAlone) {
    Bounds b = fixture();
    b.l.setConstant(-kInf);
    b.u.setConstant(kInf);

    double avg = 0.75, lo = 0.25, hi = 1.5;
    ip::ipqp_augment_bound_complementarity(b.x, b.l, b.u, b.zl, b.zu, b.n, 3, avg, lo, hi);

    EXPECT_EQ(avg, 0.75);
    EXPECT_EQ(lo, 0.25);
    EXPECT_EQ(hi, 1.5);
}

// ---------------------------------------------------------------------------
// The local reduction, which deliberately has no mirror.
// ---------------------------------------------------------------------------

TEST(IpqpMathTest, SlackComplementarityReducesInTheDeclaredOrder) {
    Eigen::VectorXd s(3), lam(3);
    s << 2.0, 0.5, 4.0;
    lam << 1.0, 8.0, 0.25;

    double avg = -1.0, lo = -1.0, hi = -1.0;
    ip::ipqp_slack_complementarity(s, lam, 3, avg, lo, hi);

    // Pairs are 2.0, 4.0, 1.0 -- accumulated ascending, one at a time.
    EXPECT_DOUBLE_EQ(avg, (2.0 + 4.0 + 1.0) / 3.0);
    EXPECT_EQ(lo, 1.0);
    EXPECT_EQ(hi, 4.0);

    // Non-vacuity.
    lam[2] = 4.0;
    ip::ipqp_slack_complementarity(s, lam, 3, avg, lo, hi);
    EXPECT_EQ(hi, 16.0);
}

TEST(IpqpMathTest, SlackComplementarityOnAnEmptyBlockIsZero) {
    const Eigen::VectorXd empty(0);
    double avg = 9.0, lo = 9.0, hi = 9.0;
    ip::ipqp_slack_complementarity(empty, empty, 0, avg, lo, hi);
    EXPECT_EQ(avg, 0.0);
    EXPECT_EQ(lo, 0.0);
    EXPECT_EQ(hi, 0.0);
}

// The two reductions compose the way the tier uses them: slacks first, bounds
// folded in, and the slack aggregates are NOT re-reduced by the fold. Pinned
// against pairs computed by hand from the fixture, so the composition is
// checked against arithmetic rather than against itself.
TEST(IpqpMathTest, TheTwoReductionsComposeIntoOneUnionAggregate) {
    const Bounds b = fixture();
    Eigen::VectorXd s(2), lam(2);
    s << 2.0, 0.5;
    lam << 1.0, 8.0;

    double avg = 0.0, lo = 0.0, hi = 0.0;
    ip::ipqp_slack_complementarity(s, lam, 2, avg, lo, hi);
    ASSERT_DOUBLE_EQ(avg, 3.0); // (2.0 + 4.0) / 2
    ip::ipqp_augment_bound_complementarity(b.x, b.l, b.u, b.zl, b.zu, b.n, 2, avg, lo, hi);

    // FOUR bound pairs, lowers then uppers:
    //   var0 lower (0.5+1.0)*0.4   = 0.6
    //   var1 lower (2.25-1.0)*1.25 = 1.5625
    //   var0 upper (3.0-0.5)*0.9   = 2.25
    //   var2 upper (0.75+1.5)*2.5  = 5.625
    // var3 is free and contributes none.
    const double bound_sum = 0.6 + 1.5625 + 2.25 + 5.625;
    EXPECT_DOUBLE_EQ(avg, (3.0 * 2.0 + bound_sum) / 6.0);
    EXPECT_DOUBLE_EQ(lo, 0.6);   // min-of-mins: the bound side wins
    EXPECT_DOUBLE_EQ(hi, 5.625); // max-of-maxes: likewise
}

// ---------------------------------------------------------------------------
// The absent-bound sentinel.
// ---------------------------------------------------------------------------

TEST(IpqpMathTest, TheAbsentBoundSentinelMatchesTheEnginesOwn) {
    EXPECT_EQ(ip::kIpqpInfBound, 1e20);
    // The header claims identity with BOTH engines' own restatements, so both
    // are pinned: a change to either would otherwise drift silently, which is
    // the whole failure mode a restated constant has.
    EXPECT_EQ(ip::kIpqpInfBound, ip::kEngineInfBound);
    EXPECT_EQ(ip::kIpqpInfBound, ip::kSsnInfBound);
    EXPECT_EQ(ip::kIpqpKappaD, hven::solvers::kKappaD);

    // At the sentinel exactly, and beyond it, a bound is ABSENT.
    EXPECT_FALSE(ip::ipqp_has_lower(-1e20));
    EXPECT_FALSE(ip::ipqp_has_lower(-std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(ip::ipqp_has_upper(1e20));
    EXPECT_FALSE(ip::ipqp_has_upper(std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(ip::ipqp_has_lower(-1e19));
    EXPECT_TRUE(ip::ipqp_has_upper(1e19));
}

// ---------------------------------------------------------------------------
// The section 2.2 item 4 read's critical-cone bound curvature (T4b fix round 1)
// ---------------------------------------------------------------------------

TEST(IpqpMathTest, TheCriticalConeSigmaDropsWeaklyActiveSidesAndNothingElse) {
    // FOUR INDICES, ONE PER REGIME, so the rule is exercised as a partition
    // rather than as one case. `weak_scale = 1e-3` throughout.
    //
    //   0  STRONGLY ACTIVE at LOWER: gap 1e-9 (tiny), z 2.0 (priced).
    //      The multiplier test fails -> curvature KEPT.
    //   1  WEAKLY ACTIVE at LOWER:   gap 1e-6, z 1e-6. Both below -> DROPPED.
    //   2  INACTIVE:                 gap 5.0, z 1e-9. The gap test fails ->
    //      KEPT (and negligible anyway).
    //   3  MIXED: strongly active at LOWER, weakly active at UPPER. The rule is
    //      PER SIDE, so the lower side's curvature survives and the upper's
    //      does not -- which is what the critical cone actually says.
    const double kInfB = ip::kIpqpInfBound;
    hven::Vec x(4), l(4), u(4), zl(4), zu(4);
    x << 1.0e-9, 1.0e-6, 5.0, 1.0e-9;
    l << 0.0, 0.0, 0.0, 0.0;
    u << kInfB, kInfB, 10.0, 1.0e-6;
    zl << 2.0, 1.0e-6, 1.0e-9, 3.0;
    zu << 0.0, 0.0, 1.0e-9, 1.0e-6;

    hven::Vec plain = hven::Vec::Zero(4);
    ip::ipqp_accumulate_bound_sigma(x, l, u, zl, zu, 4, plain);

    // `weak_scale <= 0` REPRODUCES THE ORDINARY KERNEL EXACTLY -- same loops,
    // same order, same `+=`. This is the property the convex corpus's
    // bit-identity rests on, so it is pinned as an exact equality rather than
    // as a tolerance.
    hven::Vec off = hven::Vec::Zero(4);
    ip::ipqp_accumulate_bound_sigma_critical_cone(x, l, u, zl, zu, 4, 0.0, off);
    for (hven::Index i = 0; i < 4; ++i) {
        EXPECT_DOUBLE_EQ(off(i), plain(i)) << "i = " << i;
    }
    hven::Vec negative = hven::Vec::Zero(4);
    ip::ipqp_accumulate_bound_sigma_critical_cone(x, l, u, zl, zu, 4, -1.0, negative);
    for (hven::Index i = 0; i < 4; ++i) {
        EXPECT_DOUBLE_EQ(negative(i), plain(i)) << "i = " << i;
    }

    hven::Vec cone = hven::Vec::Zero(4);
    ip::ipqp_accumulate_bound_sigma_critical_cone(x, l, u, zl, zu, 4, 1.0e-3, cone);

    // 0 -- strongly active: kept, unchanged.
    EXPECT_DOUBLE_EQ(cone(0), plain(0));
    EXPECT_GT(cone(0), 1.0e8);
    // 1 -- weakly active: dropped to nothing.
    EXPECT_DOUBLE_EQ(cone(1), 0.0);
    EXPECT_DOUBLE_EQ(plain(1), 1.0); // and it really was order ONE before
    // 2 -- inactive: kept, unchanged.
    EXPECT_DOUBLE_EQ(cone(2), plain(2));
    // 3 -- mixed: the LOWER side survives, the UPPER does not.
    EXPECT_DOUBLE_EQ(cone(3), zl(3) / (x(3) - l(3)));
    EXPECT_LT(cone(3), plain(3));
    // (The difference is checked RELATIVELY, not by exact subtraction: the
    // lower side's curvature here is 3e9 and the upper's is 1, so the
    // difference of the two totals is not representable to the last bit.)
    EXPECT_NEAR(plain(3) - cone(3), zu(3) / (u(3) - x(3)), 1.0e-6);

    // AN ABSENT BOUND IS NEVER "WEAK": the sentinel guard runs first, so an
    // index with no bound at all contributes nothing on either path and cannot
    // be miscounted as a dropped side.
    EXPECT_DOUBLE_EQ(cone(0), zl(0) / (x(0) - l(0)))
        << "index 0 has no upper bound and the upper loop must have skipped it";
}

// T4c fix round 2 (tycho fold T2/T3): `ipqp_classify_barrier_noise` exercised
// as a pure function, apart from any solve. See
// `.superpowers/w1-t4c-report.md`.

TEST(IpqpBarrierNoiseTest, ASideTrackingTheBarrierExactlyIsSuspectWithExponentOne) {
    // z = mu / s at a fixed box half-width s -- the gate-8 exposed member's
    // own identity -- so z_cur / z_prev == mu_cur / mu_prev exactly and
    // e == 1 exactly, well inside T1's [0.9, 1.1] pin.
    const double s = 1.0e-5;
    const auto v = ip::ipqp_classify_barrier_noise(1.0e-6 / s, 1.0e-8 / s, 1.0e-6, 1.0e-8);
    EXPECT_EQ(v.cls, ip::IpqpBarrierNoiseClass::kSuspect);
    EXPECT_NEAR(v.exponent, 1.0, 1.0e-9);
}

TEST(IpqpBarrierNoiseTest, ASideHoldingItsPriceIsPricedWithExponentNearZero) {
    // z ~ z* constant across a mu change: the HS-row e-pin, synthesized because
    // no real HS row band-counts (T4c report, fix round 2).
    const auto v = ip::ipqp_classify_barrier_noise(0.5, 0.5, 1.0e-6, 1.0e-8);
    EXPECT_EQ(v.cls, ip::IpqpBarrierNoiseClass::kPriced);
    EXPECT_GE(v.exponent, -0.1);
    EXPECT_LE(v.exponent, 0.1);
}

TEST(IpqpBarrierNoiseTest, NoPriorAcceptedIterateIsUninformative) {
    // `prev_mu <= 0` is the sentinel a fresh attempt (fewer than two
    // accepted iterates, R1) leaves behind -- the classifier itself must
    // never treat that as data.
    const auto v = ip::ipqp_classify_barrier_noise(1.0, 1.0, 0.0, 1.0e-8);
    EXPECT_EQ(v.cls, ip::IpqpBarrierNoiseClass::kUninformative);
    EXPECT_TRUE(std::isnan(v.exponent));
}

TEST(IpqpBarrierNoiseTest, AMuRatioAboveOneHalfIsUninformative) {
    // mu moved by less than half over the step -- too close to trust the
    // exponent (fold's own fallback rule), including the unit-ratio case.
    EXPECT_EQ(ip::ipqp_classify_barrier_noise(1.0, 1.0, 1.0e-6, 6.0e-7).cls,
              ip::IpqpBarrierNoiseClass::kUninformative);
    EXPECT_EQ(ip::ipqp_classify_barrier_noise(1.0, 1.0, 1.0e-6, 1.0e-6).cls,
              ip::IpqpBarrierNoiseClass::kUninformative)
        << "unit mu ratio";
}

TEST(IpqpBarrierNoiseTest, ASideThatJustBecameActiveIsUninformativeNeverSuspectNeverPriced) {
    // T3: z_prev == 0 at k-1 with z_cur > 0 at k -- the side was NOT active
    // one iterate ago, so there is no ratio to read.
    const auto v = ip::ipqp_classify_barrier_noise(0.0, 0.5, 1.0e-6, 1.0e-8);
    EXPECT_EQ(v.cls, ip::IpqpBarrierNoiseClass::kUninformative);
}

TEST(IpqpBarrierNoiseTest, NonfiniteInputsAreUninformative) {
    const double kNaN = std::numeric_limits<double>::quiet_NaN();
    const double kInfin = std::numeric_limits<double>::infinity();
    EXPECT_EQ(ip::ipqp_classify_barrier_noise(kNaN, 0.5, 1.0e-6, 1.0e-8).cls,
              ip::IpqpBarrierNoiseClass::kUninformative);
    EXPECT_EQ(ip::ipqp_classify_barrier_noise(0.5, kInfin, 1.0e-6, 1.0e-8).cls,
              ip::IpqpBarrierNoiseClass::kUninformative);
}

// T4c fix round 3 (Codex re-review issue 1): a finite ratio that UNDERFLOWS
// to exactly 0.0 must not reach `log` -- it produced a finite `e` (a signed
// zero) under the round-2 formula, misclassified as `kPriced`.

TEST(IpqpBarrierNoiseTest, MuRatioUnderflowIsUninformativeNotAFiniteNegativeZero) {
    // z_ratio = 2 > 1 (log positive): the naive formula's `e` here is a
    // finite `-0.0`, not NaN -- exactly the defect this guard closes.
    const auto v = ip::ipqp_classify_barrier_noise(1.0, 2.0, std::numeric_limits<double>::max(),
                                                   std::numeric_limits<double>::min());
    EXPECT_EQ(v.cls, ip::IpqpBarrierNoiseClass::kUninformative);
    EXPECT_TRUE(std::isnan(v.exponent));
}

TEST(IpqpBarrierNoiseTest, MuRatioUnderflowWithADecreasingZStillClassifiesUninformative) {
    // The mirror sign: z_ratio < 1 (log negative) makes the naive `e` a
    // finite `+0.0` instead -- both signs must be caught, not just one.
    const auto v = ip::ipqp_classify_barrier_noise(2.0, 1.0, std::numeric_limits<double>::max(),
                                                   std::numeric_limits<double>::min());
    EXPECT_EQ(v.cls, ip::IpqpBarrierNoiseClass::kUninformative);
}

TEST(IpqpBarrierNoiseTest, ZRatioUnderflowIsUninformative) {
    const auto v = ip::ipqp_classify_barrier_noise(
        std::numeric_limits<double>::max(), std::numeric_limits<double>::min(), 1.0e-6, 1.0e-8);
    EXPECT_EQ(v.cls, ip::IpqpBarrierNoiseClass::kUninformative);
}

TEST(IpqpBarrierNoiseTest, BothRatiosUnderflowIsUninformative) {
    const double lo = std::numeric_limits<double>::min();
    const double hi = std::numeric_limits<double>::max();
    const auto v = ip::ipqp_classify_barrier_noise(hi, lo, hi, lo);
    EXPECT_EQ(v.cls, ip::IpqpBarrierNoiseClass::kUninformative);
}

// T4c fix round 3 (settler ruling on T3/F2): `ipqp_barrier_noise_flag` is
// the exact function `ipqp_engine.cpp`'s kOk branch calls, so this pins the
// wiring directly rather than paralleling it.

TEST(IpqpBarrierNoiseFlagTest, AnAmbiguousPerSideVerdictFiresEvenInAnInformativeSolve) {
    EXPECT_TRUE(ip::ipqp_barrier_noise_flag(/*informative=*/true, /*noise_count=*/0,
                                            /*any_side_uninformative=*/true, /*band_count=*/2));
    EXPECT_FALSE(ip::ipqp_barrier_noise_flag(true, 0, false, 2))
        << "non-vacuity: no suspect side and no ambiguity must NOT fire";
    EXPECT_FALSE(ip::ipqp_barrier_noise_flag(false, 5, true, 0))
        << "uninformative history reads the band count alone, ignoring noise/ambiguity";
    EXPECT_TRUE(ip::ipqp_barrier_noise_flag(false, 0, false, 3));
}

} // namespace
