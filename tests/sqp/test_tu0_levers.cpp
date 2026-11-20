// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_tu0_levers.cpp -- TU-0's tuning levers on the SQP engine, behaviour side. The
// validation predicates, the defaults against their old constants and the options dump are in
// tests/solver/test_options*.cpp; this file asserts that each non-default value reaches its
// consumer and changes what the consumer does.
//
// Route-specific readings (a trajectory, a counter) sit under HVEN_TEST_ROUTE_REFERENCE; what
// holds on every route (a factor is a power of two, a history field reads the option) is
// asserted unconditionally.

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/model/problem_access.h>
#include <hven/detail/sqp/globalization/trust_region.h>
#include <hven/detail/sqp/problem_scaling.h>
#include <hven/detail/sqp/sqp_solver_impl.h>

#include "model/support/hs_problems.h"
#include "support/route_reference.h"

namespace hven {
namespace {

using detail::kTrGrowThreshold;
using detail::ProblemScaling;
using detail::ScalingRule;
using detail::trust_region_grows;

// ---------------------------------------------------------------------------
// GL-R1: the growth rule, at the helper
// ---------------------------------------------------------------------------

// (failure_step, tr_binding, delta_finite, pred_df, actual_df, feasibility_step)
struct GrowCase {
    bool failure_step;
    bool tr_binding;
    bool delta_finite;
    double pred_df;
    double actual_df;
    bool feasibility_step;
};

bool grows(SqpTrustRegionGrowth rule, const GrowCase &c) {
    return trust_region_grows(rule, c.failure_step, c.tr_binding, c.delta_finite, c.pred_df,
                              c.actual_df, c.feasibility_step);
}

TEST(Tu0TrustRegionGrowth, TheRatioArmIsTodaysRule) {
    const auto R = SqpTrustRegionGrowth::kRatio;
    // ratio at the threshold exactly grows; just below does not
    EXPECT_TRUE(grows(R, {false, true, true, 1.0, kTrGrowThreshold, false}));
    EXPECT_FALSE(grows(R, {false, true, true, 1.0, kTrGrowThreshold - 1e-9, false}));
    // no predicted decrease: never
    EXPECT_FALSE(grows(R, {false, true, true, 0.0, 5.0, false}));
    EXPECT_FALSE(grows(R, {false, true, true, -1.0, 5.0, false}));
    // an h-type step with a poor ratio does not grow under kRatio
    EXPECT_FALSE(grows(R, {false, true, true, 1.0, 0.0, true}));
}

TEST(Tu0TrustRegionGrowth, TheFeasibilityArmAlsoGrowsOnAnAcceptedHTypeStep) {
    const auto A = SqpTrustRegionGrowth::kRatioOrFeasibilityStep;
    // the ratio case still grows
    EXPECT_TRUE(grows(A, {false, true, true, 1.0, 1.0, false}));
    // an h-type accepted step grows without the ratio, even with pred_df <= 0
    EXPECT_TRUE(grows(A, {false, true, true, 1.0, 0.0, true}));
    EXPECT_TRUE(grows(A, {false, true, true, -3.0, -3.0, true}));
    // an f-type step with a poor ratio does not
    EXPECT_FALSE(grows(A, {false, true, true, 1.0, 0.1, false}));
}

TEST(Tu0TrustRegionGrowth, TheAnyAcceptedArmNeedsNoRatioAtAll) {
    const auto K = SqpTrustRegionGrowth::kAnyAccepted;
    EXPECT_TRUE(grows(K, {false, true, true, 1.0, 0.1, false}));
    EXPECT_TRUE(grows(K, {false, true, true, 0.0, -2.0, false}));
    EXPECT_TRUE(
        grows(K, {false, true, true, std::numeric_limits<double>::quiet_NaN(), 0.0, false}));
}

TEST(Tu0TrustRegionGrowth, EveryArmKeepsTodaysGuards) {
    for (const auto rule :
         {SqpTrustRegionGrowth::kRatio, SqpTrustRegionGrowth::kRatioOrFeasibilityStep,
          SqpTrustRegionGrowth::kAnyAccepted}) {
        SCOPED_TRACE(static_cast<int>(rule));
        // a failed subproblem's own step never grows the radius
        EXPECT_FALSE(grows(rule, {true, true, true, 1.0, 1.0, true}));
        // an inactive radius never grows
        EXPECT_FALSE(grows(rule, {false, false, true, 1.0, 1.0, true}));
        // an infinite radius never grows (min(inf*2, max) would shrink it)
        EXPECT_FALSE(grows(rule, {false, true, false, 1.0, 1.0, true}));
    }
}

TEST(Tu0TrustRegionGrowth, TheArmReachesTheDriverAndMovesTheRadius) {
    // Rosenbrock from far away under a small radius: the radius binds on most accepted steps, and
    // the ratio is poor on some of them, so the arms part.
    const auto run = [](SqpTrustRegionGrowth g) {
        test_support::Hs1Model model;
        SqpOptions o;
        o.trust_region_initial = 0.05;
        o.trust_region_max = 100.0;
        o.common.max_iterations = 400;
        o.advanced.trust_region_growth = g;
        return SqpSolver(o).solve(detail::problem_ref(model), model.start_point());
    };
    const SqpResult base = run(SqpTrustRegionGrowth::kRatio);
    const SqpResult any = run(SqpTrustRegionGrowth::kAnyAccepted);
    const SqpResult feas = run(SqpTrustRegionGrowth::kRatioOrFeasibilityStep);
    // Every arm still solves the problem.
    for (const SqpResult *r : {&base, &any, &feas}) {
        EXPECT_EQ(r->status, SolveStatus::kOptimal);
    }
    const auto radii = [](const SqpResult &r) {
        std::vector<double> v;
        for (const IterationRecord &row : r.history) {
            v.push_back(row.sqp->trust_radius);
        }
        return v;
    };
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_NE(radii(base), radii(any)) << "kAnyAccepted grew the radius where kRatio did not";
    // No h-type steps arise on an unconstrained problem: the feasibility arm is kRatio there.
    EXPECT_EQ(radii(base), radii(feas));
#else
    test_support::record_off_reference_route(
        "tu0_growth_radii", fmt::format("base_iters={} any_iters={} same={}", base.history.size(),
                                        any.history.size(), radii(base) == radii(any)));
#endif
}

// ---------------------------------------------------------------------------
// SCL-R2: row factors rounded to a power of two
// ---------------------------------------------------------------------------

bool is_power_of_two(double v) {
    int e = 0;
    return v > 0.0 && std::frexp(v, &e) == 0.5;
}

SpMatRM rows_matrix(std::initializer_list<double> row_norms) {
    SpMatRM m(static_cast<Index>(row_norms.size()), 1);
    Index r = 0;
    for (const double v : row_norms) {
        m.insert(r++, 0) = v;
    }
    m.makeCompressed();
    return m;
}

TEST(Tu0RowScalingPowerOfTwo, OffIsTheUnroundedRule) {
    ScalingRule rule;
    ASSERT_FALSE(rule.power_of_two_rows);
    const ProblemScaling s = detail::compute_problem_scaling(
        Vec::Constant(1, 1.0), rows_matrix({1.0e6, 3.0e4}), rows_matrix({7.0e3}), rule);
    EXPECT_DOUBLE_EQ(s.eq_rows(0), 1.0e-4);
    EXPECT_DOUBLE_EQ(s.eq_rows(1), 100.0 / 3.0e4);
    EXPECT_DOUBLE_EQ(s.ineq_rows(0), 100.0 / 7.0e3);
}

TEST(Tu0RowScalingPowerOfTwo, OnRoundsEachRowFactorToTheNearestPowerOfTwo) {
    ScalingRule rule;
    rule.power_of_two_rows = true;
    const ProblemScaling s = detail::compute_problem_scaling(
        Vec::Constant(1, 1.0e6), rows_matrix({1.0e6, 3.0e4, 1.0}), rows_matrix({7.0e3}), rule);
    // 1e-4 -> 2^-13 (log2 = -13.29); 100/3e4 = 3.33e-3 -> 2^-8 (-8.23); 100/7e3 = 1.43e-2 -> 2^-6
    // (-6.13); a row below max_gradient keeps the identity, which is 2^0.
    EXPECT_DOUBLE_EQ(s.eq_rows(0), std::ldexp(1.0, -13));
    EXPECT_DOUBLE_EQ(s.eq_rows(1), std::ldexp(1.0, -8));
    EXPECT_DOUBLE_EQ(s.eq_rows(2), 1.0);
    EXPECT_DOUBLE_EQ(s.ineq_rows(0), std::ldexp(1.0, -6));
    // The objective factor is not rounded: 100 / 1e6 = 1e-4 exactly.
    EXPECT_DOUBLE_EQ(s.obj, 1.0e-4);
}

TEST(Tu0RowScalingPowerOfTwo, TheRoundedFactorStaysInsideTheClamp) {
    ScalingRule rule;
    rule.power_of_two_rows = true;
    rule.max_gradient = 1.0;
    rule.factor_limit = 10.0; // floor(log2 10) = 3: factors confined to [2^-3, 2^3]
    const ProblemScaling s = detail::compute_problem_scaling(
        Vec::Constant(1, 1.0), rows_matrix({1.0e6}), SpMatRM(0, 1), rule);
    EXPECT_DOUBLE_EQ(s.eq_rows(0), 0.125);
    EXPECT_GE(s.eq_rows(0), 1.0 / rule.factor_limit);
}

class UnevenRows : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override { return 0.5 * (x(0) * x(0) + x(1) * x(1)); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, 3.0e4 * (x(0) - 1.0)); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(1) - 2.0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 3.0e4;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(2); }

  private:
    Vec lower_ = Vec::Constant(2, -10.0);
    Vec upper_ = Vec::Constant(2, 10.0);
};

TEST(Tu0RowScalingPowerOfTwo, TheOptionReachesTheDriversInstalledFactors) {
    const auto solve = [](bool pow2) {
        UnevenRows model;
        SqpOptions o;
        o.row_scaling = true;
        o.row_scaling_power_of_two = pow2;
        return SqpSolver(o).solve(detail::problem_ref(model), model.start_point());
    };
    const SqpResult off = solve(false);
    const SqpResult on = solve(true);
    ASSERT_TRUE(off.scaling.active);
    ASSERT_TRUE(on.scaling.active);
    // 100 / 3e4 = 3.33e-3 unrounded; the rounded factor is 2^-8.
    EXPECT_DOUBLE_EQ(off.scaling.row_min, 100.0 / 3.0e4);
    EXPECT_TRUE(is_power_of_two(on.scaling.row_min)) << on.scaling.row_min;
    EXPECT_DOUBLE_EQ(on.scaling.row_min, std::ldexp(1.0, -8));
    // The answer does not depend on the row scale.
    EXPECT_EQ(on.status, SolveStatus::kOptimal);
    EXPECT_NEAR(on.x(0), 1.0, 1e-6);
    EXPECT_NEAR(on.x(1), 0.0, 1e-6);
}

// ---------------------------------------------------------------------------
// M7-15: the adaptive dual-regularization schedule's ceiling
// ---------------------------------------------------------------------------

TEST(Tu0AdaptiveDualRegularizationMax, TheFirstMajorsMuIsTheCeilingQuantizedToADecade) {
    const auto first_mu = [](double ceiling) {
        test_support::Hs1Model model;
        SqpOptions o;
        o.advanced.adaptive_dual_regularization_max = ceiling;
        const SqpResult r = SqpSolver(o).solve(detail::problem_ref(model), model.start_point());
        EXPECT_FALSE(r.history.empty());
        return r.history.front().sqp->regularization_dual;
    };
    // The default reproduces the old constant exactly.
    EXPECT_EQ(first_mu(detail::kAdaptiveMuMax), 1e-8);
    EXPECT_EQ(first_mu(1e-8), first_mu(SqpOptions{}.advanced.adaptive_dual_regularization_max));
    // A raised ceiling is what the first major runs at (quantized to the decade).
    EXPECT_EQ(first_mu(1e-6), 1e-6);
    EXPECT_EQ(first_mu(1e-4), 1e-4);
    EXPECT_EQ(first_mu(1.0), 1.0);
    // And at the floor of the range the schedule is pinned to the floor.
    EXPECT_EQ(first_mu(1e-12), 1e-12);
}

TEST(Tu0AdaptiveDualRegularizationMax, ItBoundsEveryMajorsMuFromAbove) {
    test_support::Hs1Model model;
    SqpOptions o;
    o.advanced.adaptive_dual_regularization_max = 1e-6;
    const SqpResult r = SqpSolver(o).solve(detail::problem_ref(model), model.start_point());
    for (const IterationRecord &row : r.history) {
        if (row.sqp->qp_solved) {
            EXPECT_LE(row.sqp->regularization_dual, 1e-6 * (1.0 + 1e-12));
        }
    }
}

// ---------------------------------------------------------------------------
// The SSN levers, through the driver (kSsn mode)
// ---------------------------------------------------------------------------
//
// Each lever is read at SsnOptions and tested there (test_ssn_engine.cpp); these cells assert the
// driver forwards each one. The readings are counters of the kSsn subproblems, so they are the
// route: reference build only, recorded elsewhere.

SqpResult solve_ssn(NlpModel &model, const SqpOptions &base) {
    SqpOptions o = base;
    o.qp_mode = QpMode::kSsn;
    o.common.max_iterations = 100;
    return SqpSolver(o).solve(detail::problem_ref(model), model.start_point());
}

TEST(Tu0SsnLevers, TheLmSigmaCoefficientAndExponentReachTheEngine) {
    const auto run = [](double c, double p) {
        test_support::Hs6Model model;
        SqpOptions o;
        o.advanced.ssn_sigma_rule = SsnSigmaRule::kResidualAlways;
        o.advanced.ssn_lm_sigma_c = c;
        o.advanced.ssn_lm_sigma_exponent = p;
        return solve_ssn(model, o);
    };
    const SqpResult base = run(1.0, 2.0);
    const SqpResult bigger_c = run(1e3, 2.0);
    const SqpResult linear = run(1.0, 1.0);
    for (const SqpResult *r : {&base, &bigger_c, &linear}) {
        EXPECT_EQ(r->status, SolveStatus::kOptimal);
    }
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_NE(base.counters.ssn.ssn_prox_updates, bigger_c.counters.ssn.ssn_prox_updates);
    EXPECT_NE(base.counters.factorizations, linear.counters.factorizations);
#else
    test_support::record_off_reference_route(
        "tu0_ssn_lm",
        fmt::format("prox_updates base={} c={} fact base={} p1={}",
                    base.counters.ssn.ssn_prox_updates, bigger_c.counters.ssn.ssn_prox_updates,
                    base.counters.factorizations, linear.counters.factorizations));
#endif
}

TEST(Tu0SsnLevers, TheUncertainToleranceReachesTheEngine) {
    const auto peak = [](double tol) {
        test_support::Hs6Model model;
        SqpOptions o;
        o.advanced.ssn_uncertain_tol = tol;
        return solve_ssn(model, o).counters.ssn.ssn_uncertain_peak;
    };
    // 0 disables the set outright, so no row is ever uncertain.
    EXPECT_EQ(peak(0.0), 0);
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_GE(peak(0.9), 1) << "a wide band catches a row the default 0.1 band does not";
    EXPECT_EQ(peak(0.1), peak(SqpOptions{}.advanced.ssn_uncertain_tol));
#else
    test_support::record_off_reference_route("tu0_ssn_uncertain",
                                             fmt::format("peak(0.9)={}", peak(0.9)));
#endif
}

TEST(Tu0SsnLevers, TheFarkasResidualToleranceReachesTheEngine) {
    const auto run = [](double tol) {
        test_support::Hs10Model model;
        SqpOptions o;
        o.advanced.ssn_infeasibility_rule = SsnInfeasibilityRule::kFarkasGated;
        o.advanced.ssn_farkas_residual_tol = tol;
        return solve_ssn(model, o);
    };
    const SqpResult dflt = run(1e-6);
    const SqpResult tight = run(1e-12);
    EXPECT_EQ(dflt.status, SolveStatus::kOptimal);
    EXPECT_EQ(tight.status, SolveStatus::kOptimal);
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_NE(dflt.counters.factorizations, tight.counters.factorizations);
    EXPECT_NE(dflt.counters.ssn.ssn_escape_infeasible_suspect,
              tight.counters.ssn.ssn_escape_infeasible_suspect);
#else
    test_support::record_off_reference_route(
        "tu0_ssn_farkas", fmt::format("fact default={} tight={}", dflt.counters.factorizations,
                                      tight.counters.factorizations));
#endif
}

// ---------------------------------------------------------------------------
// qp.max_schur_border_dimension = 0: the border is never admitted
// ---------------------------------------------------------------------------

TEST(Tu0SchurBorder, ZeroIsAValidSettingAndTheSolveStillConverges) {
    const auto run = [](Index cap) {
        test_support::Hs76Model model;
        SqpOptions o;
        o.qp.max_schur_border_dimension = cap;
        o.common.max_iterations = 100;
        return SqpSolver(o).solve(detail::problem_ref(model), model.start_point());
    };
    const SqpResult def = run(128);
    const SqpResult zero = run(0);
    EXPECT_EQ(def.status, SolveStatus::kOptimal);
    EXPECT_EQ(zero.status, SolveStatus::kOptimal);
    EXPECT_NEAR(zero.f, def.f, 1e-6 * std::max(1.0, std::abs(def.f)));
    EXPECT_LT((zero.x - def.x).cwiseAbs().maxCoeff(), 1e-5);
}

TEST(Tu0SchurBorder, ZeroAlsoSolvesAnInequalityConstrainedProblemInTheRefactorizeAlgebraToo) {
    for (const auto algebra :
         {QpWorkingSetAlgebra::kSchurBorder, QpWorkingSetAlgebra::kRefactorize}) {
        test_support::Hs15Model model;
        SqpOptions o;
        o.qp.max_schur_border_dimension = 0;
        o.qp.working_set_algebra = algebra;
        o.common.max_iterations = 100;
        const SqpResult r = SqpSolver(o).solve(detail::problem_ref(model), model.start_point());
        EXPECT_EQ(r.status, SolveStatus::kOptimal) << static_cast<int>(algebra);
    }
}

} // namespace
} // namespace hven
