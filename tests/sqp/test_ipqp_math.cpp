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

#include <cstddef>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include "hven/core/types.h"
#include "hven/detail/interior/barrier_math.h"
#include "hven/detail/interior/bound_set.h"
#include "hven/detail/qp/ipqp_math.h"

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

// ---------------------------------------------------------------------------
// The five mirrors.
// ---------------------------------------------------------------------------

TEST(IpqpMathTest, BoundBarrierObjectiveMirrorsTheNlpKernel) {
    Bounds b = fixture();
    const BoundSet s = bound_set_of(b);

    const double got = ip::ipqp_bound_barrier_objective(b.x, b.l, b.u, kMu, b.n);
    const double want = ip::bound_barrier_objective(b.x, s, kMu);

    EXPECT_DOUBLE_EQ(got, want);
    // The damping is actually in there: the fixture has two one-sided
    // variables, so dropping kappa_d would move the answer.
    EXPECT_NE(got, ip::bound_barrier_objective(b.x, BoundSet{}, kMu));
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

    ip::ipqp_accumulate_bound_dual_terms(b.zl, b.zu, b.n, got);
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
    EXPECT_EQ(ip::kIpqpKappaD, hven::solvers::kKappaD);

    // At the sentinel exactly, and beyond it, a bound is ABSENT.
    EXPECT_FALSE(ip::ipqp_has_lower(-1e20));
    EXPECT_FALSE(ip::ipqp_has_lower(-std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(ip::ipqp_has_upper(1e20));
    EXPECT_FALSE(ip::ipqp_has_upper(std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(ip::ipqp_has_lower(-1e19));
    EXPECT_TRUE(ip::ipqp_has_upper(1e19));
}

} // namespace
