// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_ipqp_mu0_rule.cpp -- M7 W2 A2's two IPQP toggles:
// `IpqpOptions::ipqp_mu0_rule` (the complementarity-derived mu_0) and
// `IpqpOptions::ipqp_mu_residual_floor` (A1-R1's residual-proportional floor).
// Design and rule text: docs/notes/2026-10-m7-w2-a2-mu0-design.md sections 3-5.

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/drivers/trace.h>

#include "support/indefinite_fixtures.h"

namespace hven::solvers {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

SpMatRM dense(const std::vector<std::vector<double>> &a, Index cols, bool upper_only) {
    const Index rows = static_cast<Index>(a.size());
    SpMatRM m(rows, cols);
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < rows; ++i) {
        for (Index j = upper_only ? i : 0; j < cols; ++j) {
            const double v = a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
            if (v != 0.0) {
                t.emplace_back(static_cast<int>(i), static_cast<int>(j), v);
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

/// min 0.5 h x^2 + g x over [-1, 1]: no rows, two bound sides. The rule's closed form below.
QpProblem scalar_box_qp(double h, double g) {
    QpProblem qp;
    qp.H = dense({{h}}, 1, true);
    qp.g = vec({g});
    qp.Ae = dense({}, 1, false);
    qp.Ai = dense({}, 1, false);
    qp.be = Vec(0);
    qp.bi = Vec(0);
    qp.lower = vec({-1.0});
    qp.upper = vec({1.0});
    return qp;
}

/// min |x - (1, 2)|^2 with x1 - x2 = 0.5, x1 + x2 <= 1 and a [-10, 10] box: every block.
QpProblem general_qp() {
    QpProblem qp;
    qp.H = dense({{2.0, 0.0}, {0.0, 2.0}}, 2, true);
    qp.g = vec({-2.0, -4.0});
    qp.Ae = dense({{1.0, -1.0}}, 2, false);
    qp.be = vec({0.5});
    qp.Ai = dense({{1.0, 1.0}}, 2, false);
    qp.bi = vec({1.0});
    qp.lower = vec({-10.0, -10.0});
    qp.upper = vec({10.0, 10.0});
    return qp;
}

/// Ten inequality rows `x_k <= c_k` over a box, minimizer at `x = target`: rows 0..4 bind, the
/// rest are slack by decades -- a small population of rows at very different distances.
QpProblem many_rows_qp() {
    const Index n = 10;
    QpProblem qp;
    std::vector<std::vector<double>> h(n, std::vector<double>(n, 0.0));
    std::vector<std::vector<double>> a(n, std::vector<double>(n, 0.0));
    std::vector<double> g(n), b(n), lo(n, -100.0), up(n, 100.0);
    for (Index k = 0; k < n; ++k) {
        h[k][k] = 1.0;
        a[k][k] = 1.0;
        g[k] = -1.0; // unconstrained minimizer x = 1
        b[k] = k < 5 ? 0.5 : 1.0 + std::pow(10.0, static_cast<double>(k - 9));
    }
    qp.H = dense(h, n, true);
    qp.g = vec(g);
    qp.Ae = dense({}, n, false);
    qp.be = Vec(0);
    qp.Ai = dense(a, n, false);
    qp.bi = vec(b);
    qp.lower = vec(lo);
    qp.upper = vec(up);
    return qp;
}

/// No rows and absent bounds: no complementary pair at all.
QpProblem free_qp() {
    QpProblem qp;
    qp.H = dense({{2.0}}, 1, true);
    qp.g = vec({-1.0});
    qp.Ae = dense({}, 1, false);
    qp.Ai = dense({}, 1, false);
    qp.be = Vec(0);
    qp.bi = Vec(0);
    qp.lower = vec({-1.0e20});
    qp.upper = vec({1.0e20});
    return qp;
}

QpOptions untrusted_region() {
    QpOptions o;
    o.tr_radius = kInf;
    return o;
}

IpqpOptions with_rule(IpqpMu0Rule rule) {
    IpqpOptions o;
    o.ipqp_mu0_rule = rule;
    return o;
}

IpqpOptions with_floor(bool on) {
    IpqpOptions o;
    o.ipqp_mu_residual_floor = on;
    return o;
}

class IterSink : public TraceSink {
  public:
    std::vector<IpqpTraceIterEvent> iters;
    void on_ipqp_iter(const IpqpTraceIterEvent &e) override { iters.push_back(e); }
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const QpModeTraceEvent &) override {}
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {}
};

struct Traced {
    IpqpResult r;
    std::vector<IpqpTraceIterEvent> iters;
};

Traced solve_traced(const QpProblem &qp, const IpqpOptions &iopts, const IpqpSeed *seed = nullptr) {
    IpqpEngine tier(untrusted_region());
    IterSink sink;
    tier.attach_trace(&sink);
    Traced t;
    t.r = tier.solve(qp, seed, iopts, SolveOverrides{});
    t.iters = sink.iters;
    return t;
}

void expect_same_trajectory(const IpqpResult &a, const IpqpResult &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.escape_reason, b.escape_reason);
    EXPECT_EQ(a.counters.ipqp_iters, b.counters.ipqp_iters);
    ASSERT_EQ(a.x.size(), b.x.size());
    EXPECT_TRUE((a.x.array() == b.x.array()).all()) << a.x.transpose() << " vs " << b.x.transpose();
    EXPECT_EQ(a.mu, b.mu);
}

// ---------------------------------------------------------------------------
// The A0 surface: validation, defaults, dump
// ---------------------------------------------------------------------------

TEST(IpqpMu0RuleOptions, BothTogglesDefaultOff) {
    const IpqpOptions o;
    EXPECT_EQ(o.ipqp_mu0_rule, IpqpMu0Rule::kFixed);
    EXPECT_FALSE(o.ipqp_mu_residual_floor);
}

TEST(IpqpMu0RuleOptions, ValidateAcceptsTheTwoRulesAndRefusesTheSentinelByName) {
    SqpOptions o;
    o.ipqp.ipqp_mu0_rule = IpqpMu0Rule::kFixed;
    EXPECT_NO_THROW(validate(o));
    o.ipqp.ipqp_mu0_rule = IpqpMu0Rule::kComplementarity;
    EXPECT_NO_THROW(validate(o));
    for (const IpqpMu0Rule bad : {IpqpMu0Rule::kIpqpMu0RuleCount, static_cast<IpqpMu0Rule>(7)}) {
        o.ipqp.ipqp_mu0_rule = bad;
        try {
            validate(o);
            ADD_FAILURE() << "validate accepted ipqp_mu0_rule " << static_cast<int>(bad);
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("ipqp.ipqp_mu0_rule"), std::string::npos)
                << e.what();
        }
    }
}

// A bool refuses nothing: both states are legal, and the tier itself (which validates through
// the same owner) accepts both.
TEST(IpqpMu0RuleOptions, TheResidualFloorBoolHasNoRefusableValue) {
    SqpOptions o;
    for (const bool v : {false, true}) {
        o.ipqp.ipqp_mu_residual_floor = v;
        EXPECT_NO_THROW(validate(o));
        EXPECT_NO_THROW(solve_traced(scalar_box_qp(2.0, 1e-3), with_floor(v)));
    }
}

TEST(IpqpMu0RuleOptions, TheDumpCarriesBothLinesInEitherState) {
    SqpOptions o;
    std::ostringstream off;
    dump_options(o, off);
    EXPECT_NE(off.str().find("ipqp.ipqp_mu0_rule = kFixed\n"), std::string::npos);
    EXPECT_NE(off.str().find("ipqp.ipqp_mu_residual_floor = false\n"), std::string::npos);
    o.ipqp.ipqp_mu0_rule = IpqpMu0Rule::kComplementarity;
    o.ipqp.ipqp_mu_residual_floor = true;
    std::ostringstream on;
    dump_options(o, on);
    EXPECT_NE(on.str().find("ipqp.ipqp_mu0_rule = kComplementarity\n"), std::string::npos);
    EXPECT_NE(on.str().find("ipqp.ipqp_mu_residual_floor = true\n"), std::string::npos);
}

// ---------------------------------------------------------------------------
// kComplementarity -- the rule (design page section 3)
// ---------------------------------------------------------------------------

// THE CLOSED FORM. At x = 0 with unit bound distances, the affine step is dx = -g / (h + rho + 2)
// = -g/12 at h = 2, rho_0 = 8; the estimate is (zl, zu) = (g/12, -g/12); the 1.5 shift adds g/8,
// the 0.5 shift g/16; so (zl, zu) = (13g/48, 5g/48) and mu_0 = 18g/96 = 0.1875 g.
TEST(IpqpMu0Rule, TheDerivedMuZeroMatchesTheClosedFormOnABoundOnlyQp) {
    const double g = 1e-3;
    const Traced t = solve_traced(scalar_box_qp(2.0, g), with_rule(IpqpMu0Rule::kComplementarity));
    ASSERT_FALSE(t.iters.empty());
    EXPECT_NEAR(t.iters.front().mu, 0.1875 * g, 1e-12 * g);
    EXPECT_EQ(t.r.counters.ipqp_mu0_derived, 1);
    EXPECT_EQ(t.r.counters.ipqp_mu0_fallbacks, 0);
    EXPECT_EQ(t.r.status, QpStatus::kOptimal);
    EXPECT_NEAR(t.r.x(0), -g / 2.0, 1e-6);
}

// The same QP under kFixed starts at the setting; the derived start is not that.
TEST(IpqpMu0Rule, KFixedStartsAtTheSettingAndReadsNoCounter) {
    const Traced t = solve_traced(scalar_box_qp(2.0, 1e-3), IpqpOptions{});
    ASSERT_FALSE(t.iters.empty());
    EXPECT_EQ(t.iters.front().mu, IpqpOptions{}.ipqp_init_mu);
    EXPECT_EQ(t.r.counters.ipqp_mu0_derived, 0);
    EXPECT_EQ(t.r.counters.ipqp_mu0_fallbacks, 0);
    EXPECT_EQ(t.r.counters.ipqp_mu_floor_binds, 0);
}

// Above the ceiling (mu_derived = 1.875 at g = 10): the kFixed start, one factorization dearer,
// the same trajectory.
TEST(IpqpMu0Rule, ADerivedValueAboveTheCeilingFallsBackToTheFixedStart) {
    const QpProblem qp = scalar_box_qp(2.0, 10.0);
    const Traced off = solve_traced(qp, IpqpOptions{});
    const Traced on = solve_traced(qp, with_rule(IpqpMu0Rule::kComplementarity));
    ASSERT_FALSE(on.iters.empty());
    EXPECT_EQ(on.iters.front().mu, IpqpOptions{}.ipqp_init_mu);
    EXPECT_EQ(on.r.counters.ipqp_mu0_derived, 0);
    EXPECT_EQ(on.r.counters.ipqp_mu0_fallbacks, 1);
    EXPECT_EQ(on.r.counters.ipqp_factorizations, off.r.counters.ipqp_factorizations + 1);
    expect_same_trajectory(on.r, off.r);
}

// Below the floor (1.875e-14 at g = 1e-13) and exactly zero (g = 0): both fall back.
TEST(IpqpMu0Rule, ADerivedValueBelowTheFloorOrAZeroEstimateFallsBack) {
    for (const double g : {1e-13, 0.0}) {
        const Traced on =
            solve_traced(scalar_box_qp(2.0, g), with_rule(IpqpMu0Rule::kComplementarity));
        ASSERT_FALSE(on.iters.empty()) << g;
        EXPECT_EQ(on.iters.front().mu, IpqpOptions{}.ipqp_init_mu) << g;
        EXPECT_EQ(on.r.counters.ipqp_mu0_fallbacks, 1) << g;
        EXPECT_EQ(on.r.counters.ipqp_mu0_derived, 0) << g;
    }
}

// Raising the ceiling admits the g = 10 value: the band test reads `ipqp_init_mu`, not a constant.
TEST(IpqpMu0Rule, TheBandIsTheOptionsOwnCeiling) {
    IpqpOptions o = with_rule(IpqpMu0Rule::kComplementarity);
    o.ipqp_init_mu = 10.0;
    const Traced on = solve_traced(scalar_box_qp(2.0, 10.0), o);
    ASSERT_FALSE(on.iters.empty());
    EXPECT_NEAR(on.iters.front().mu, 1.875, 1e-12);
    EXPECT_EQ(on.r.counters.ipqp_mu0_derived, 1);
}

// Every block (equality, inequality, bounds), both paths. At the shipped ceiling both QPs fall
// back (their O(1) prices and box distances put mu_derived near 6 and 14); with the ceiling at
// 1e6 the derived start is taken. Either way the tier converges to the walk's answer.
TEST(IpqpMu0Rule, AQpWithEveryBlockConvergesToTheWalksAnswerOnBothPaths) {
    for (const QpProblem &qp : {general_qp(), many_rows_qp()}) {
        QpEngine walk(untrusted_region());
        const QpSolution ref = walk.solve(qp);
        ASSERT_EQ(ref.status, QpStatus::kOptimal);
        for (const double ceiling : {IpqpOptions{}.ipqp_init_mu, 1e6}) {
            IpqpOptions o = with_rule(IpqpMu0Rule::kComplementarity);
            o.ipqp_init_mu = ceiling;
            const Traced on = solve_traced(qp, o);
            EXPECT_EQ(on.r.status, QpStatus::kOptimal) << ceiling;
            EXPECT_EQ(on.r.counters.ipqp_mu0_derived, ceiling > 1.0 ? 1 : 0) << ceiling;
            EXPECT_EQ(on.r.counters.ipqp_mu0_fallbacks, ceiling > 1.0 ? 0 : 1) << ceiling;
            EXPECT_LT((on.r.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-6) << ceiling;
        }
    }
}

// A REGRESSION PIN, observed at the commit that landed the rule (the arithmetic's closed form is
// the scalar test above): the derived mu_0 of the two row QPs with the ceiling lifted. It is what
// catches a change to the row and equality halves of the estimate, which no closed form here does.
TEST(IpqpMu0Rule, TheDerivedMuZeroOfTheRowQpsIsPinned) {
    IpqpOptions o = with_rule(IpqpMu0Rule::kComplementarity);
    o.ipqp_init_mu = 1e6;
    const std::vector<std::pair<QpProblem, double>> cases = {{general_qp(), 5.9569897959183518},
                                                             {many_rows_qp(), 13.594305475000516}};
    for (const auto &[qp, want] : cases) {
        const Traced t = solve_traced(qp, o);
        ASSERT_FALSE(t.iters.empty());
        EXPECT_NEAR(t.iters.front().mu, want, 1e-10 * want);
    }
}

TEST(IpqpMu0Rule, AQpWithNoPairReadsNeitherCounter) {
    const Traced on = solve_traced(free_qp(), with_rule(IpqpMu0Rule::kComplementarity));
    EXPECT_EQ(on.r.counters.ipqp_mu0_derived, 0);
    EXPECT_EQ(on.r.counters.ipqp_mu0_fallbacks, 0);
    expect_same_trajectory(on.r, solve_traced(free_qp(), IpqpOptions{}).r);
}

// A warm start keeps section 5.3's clamp: the rule is not consulted.
TEST(IpqpMu0Rule, AWarmStartIsUntouched) {
    const QpProblem qp = general_qp();
    IpqpEngine first(untrusted_region());
    ASSERT_EQ(first.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{}).status, QpStatus::kOptimal);
    ASSERT_NE(first.warm_carry(), nullptr);
    const IpqpSeed seed = *first.warm_carry();
    const Traced off = solve_traced(qp, IpqpOptions{}, &seed);
    const Traced on = solve_traced(qp, with_rule(IpqpMu0Rule::kComplementarity), &seed);
    EXPECT_EQ(on.r.counters.ipqp_mu0_derived, 0);
    EXPECT_EQ(on.r.counters.ipqp_mu0_fallbacks, 0);
    EXPECT_EQ(on.r.counters.ipqp_factorizations, off.r.counters.ipqp_factorizations);
    expect_same_trajectory(on.r, off.r);
}

// Forced to fall back (g scaled up), the solve must then be OFF's: the ladder's memory is
// restored, so the first iteration's climb and the high-water mark are OFF's own. The concave
// scalar (h = -100, far past rho_0 + the unit point's curvature 2) is the case where the
// estimate's own ladder MUST climb, which is what makes the restore observable.
TEST(IpqpMu0Rule, AFallbackLeavesTheLadderMemoryAsOffFindsIt) {
    Index climbed = 0;
    for (QpProblem qp :
         {test_support::indefinite_equality_qp(), test_support::indefinite_equality_and_row_qp(),
          test_support::two_negative_eigenvalue_row_qp(), scalar_box_qp(-100.0, 1.0)}) {
        qp.g *= 1e4;
        const Traced off = solve_traced(qp, IpqpOptions{});
        const Traced on = solve_traced(qp, with_rule(IpqpMu0Rule::kComplementarity));
        ASSERT_EQ(on.r.counters.ipqp_mu0_fallbacks, 1);
        if (on.r.counters.ipqp_inertia_retries > off.r.counters.ipqp_inertia_retries) {
            ++climbed;
        }
        expect_same_trajectory(on.r, off.r);
        EXPECT_EQ(on.r.counters.ipqp_rho_demanded_max, off.r.counters.ipqp_rho_demanded_max);
        EXPECT_EQ(on.r.counters.ipqp_ladder_reclimbs, off.r.counters.ipqp_ladder_reclimbs);
        EXPECT_GE(on.r.counters.ipqp_inertia_retries, off.r.counters.ipqp_inertia_retries);
    }
    EXPECT_GT(climbed, 0) << "no fixture's estimate climbed the ladder: the pin is vacuous";
}

// ---------------------------------------------------------------------------
// ipqp_mu_residual_floor -- A1-R1 (design page section 4)
// ---------------------------------------------------------------------------

/// The floor's count re-derived from the trace: `floor = mu_s * r / max(1, r_s)` with
/// `(mu_s, r_s)` the first event's, `r = max(res_p, res_d)`; a bind is `floor > sigma * mu` and
/// `floor > ipqp_min_mu`.
Index binds_from_trace(const std::vector<IpqpTraceIterEvent> &iters, double min_mu) {
    if (iters.empty()) {
        return 0;
    }
    const double mu_s = iters.front().mu;
    const double r_s = std::max(iters.front().res_p, iters.front().res_d);
    Index binds = 0;
    for (const IpqpTraceIterEvent &e : iters) {
        const double floor_k = mu_s * std::max(e.res_p, e.res_d) / std::max(1.0, r_s);
        if (floor_k > e.sigma * e.mu && floor_k > min_mu) {
            ++binds;
        }
    }
    return binds;
}

TEST(IpqpMuResidualFloor, TheCounterIsTheNeighbourhoodRuleReadOffTheTrace) {
    // The last starts nearly stationary (r_s ~ 1e-3), where `max(1, r_s)` and `r_s` part ways.
    for (const QpProblem &qp :
         {general_qp(), many_rows_qp(), scalar_box_qp(2.0, 10.0), scalar_box_qp(2.0, 1e-3)}) {
        const Traced on = solve_traced(qp, with_floor(true));
        EXPECT_EQ(on.r.counters.ipqp_mu_floor_binds,
                  binds_from_trace(on.iters, IpqpOptions{}.ipqp_min_mu));
        EXPECT_EQ(on.r.counters.ipqp_mu0_derived + on.r.counters.ipqp_mu0_fallbacks, 0);
    }
}

TEST(IpqpMuResidualFloor, ItBindsOnARowPopulationAndStopsAtTheTiersContract) {
    const QpProblem qp = many_rows_qp();
    QpEngine walk(untrusted_region());
    const QpSolution ref = walk.solve(qp);
    ASSERT_EQ(ref.status, QpStatus::kOptimal);
    const Traced on = solve_traced(qp, with_floor(true));
    const Traced off = solve_traced(qp, IpqpOptions{});
    EXPECT_GT(on.r.counters.ipqp_mu_floor_binds, 0);
    EXPECT_EQ(off.r.counters.ipqp_mu_floor_binds, 0);
    EXPECT_EQ(on.r.status, QpStatus::kOptimal);
    EXPECT_TRUE(ipqp_residuals_meet_target(on.r.residuals, untrusted_region(), with_floor(true)));
    // The floor stops the tier at its contract (mu ~ 4e-8 here) instead of a collapse to the
    // 1e-12 floor, so the barrier bias on the 1e-4-slack rows is visible: 1e-3, not 1e-6.
    EXPECT_GT(on.r.mu, off.r.mu);
    EXPECT_LT((on.r.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-3);
}

// A floor that never binds changes nothing, bit for bit.
TEST(IpqpMuResidualFloor, AFloorThatNeverBindsIsBitIdenticalToOff) {
    const QpProblem qp = free_qp();
    const Traced on = solve_traced(qp, with_floor(true));
    ASSERT_EQ(on.r.counters.ipqp_mu_floor_binds, 0);
    expect_same_trajectory(on.r, solve_traced(qp, IpqpOptions{}).r);
}

// The warm-kill re-takes the anchor at the cold restart: a warm attempt killed on its first
// pass (budget 0) is followed by exactly the cold solve's trajectory and binds.
TEST(IpqpMuResidualFloor, AWarmKillReAnchorsAtTheColdRestart) {
    const QpProblem qp = many_rows_qp();
    IpqpOptions o = with_floor(true);
    const Traced cold = solve_traced(qp, o);
    ASSERT_GT(cold.r.counters.ipqp_mu_floor_binds, 0);

    // A seed for THIS problem, perturbed off its solution so the warm attempt is real.
    IpqpEngine solved(untrusted_region());
    ASSERT_EQ(solved.solve(qp, nullptr, IpqpOptions{}, SolveOverrides{}).status,
              QpStatus::kOptimal);
    IpqpSeed seed = *solved.warm_carry();
    seed.x.array() += 0.3;
    o.ipqp_warm_iter_budget = 0;
    const Traced killed = solve_traced(qp, o, &seed);
    ASSERT_EQ(killed.r.counters.ipqp_warm_restart_abandoned, 1);
    EXPECT_EQ(killed.r.counters.ipqp_mu_floor_binds, cold.r.counters.ipqp_mu_floor_binds);
    EXPECT_EQ(killed.r.counters.ipqp_iters, cold.r.counters.ipqp_iters);
    EXPECT_TRUE((killed.r.x.array() == cold.r.x.array()).all());
}

} // namespace
} // namespace hven::solvers
