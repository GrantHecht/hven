// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-5 G6: IpmCounters, the regrouped IpmResult (the per-phase diagnostics and
// last_ran_phase) and IpmSolveRecord::counters. Every read below is of a value a
// real solve produced; the wiring tests fail when a field is dropped or crossed.

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/core/solver_counters.h>
#include <hven/detail/model/nlp_adapter.h>
#include <hven/detail/model/problem_access.h>
#include <hven/ipm/ipm_solver.h>
#include <hven/ipm/ipm_solver_types.h>
#include <hven/model/nlp.h>

#include "model/support/hs071_problem.h"

namespace {

using hven::IpmPhaseReport;
using hven::IpmResult;

IpmResult ipm_counters_solve_hs071(hven::IpmOptions o, hven::Ledger *ledger = nullptr) {
    o.common.verbosity = hven::Verbosity::kSilent;
    hven::IpmSolver solver(o);
    if (ledger != nullptr) {
        solver.attach_ledger(ledger, "ipm");
    }
    const auto model =
        hven::detail::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    return solver.solve(hven::detail::problem_ref(*model), hven_drivers_tests::hs071_start());
}

// The Maratos problem: min 2 (x0^2 + x1^2 - 1) - x0  s.t.  x0^2 + x1^2 = 1. A full step from a
// feasible start is rejected, so the second-order correction and the watchdog have work.
struct IpmCountersMaratos final : hven::Nlp {
    hven::NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl.setConstant(-inf);
        xu.setConstant(inf);
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(hven::ConstVecRef x) const override {
        return 2.0 * (x[0] * x[0] + x[1] * x[1] - 1.0) - x[0];
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        g << 4.0 * x[0] - 1.0, 4.0 * x[1];
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g << x[0] * x[0] + x[1] * x[1] - 1.0;
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
        v << 2.0 * x[0], 2.0 * x[1];
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        v << 4.0 * obj_factor + 2.0 * lambda[0], 4.0 * obj_factor + 2.0 * lambda[0];
    }
    std::string name() const override { return "IpmCountersMaratos"; }
};

IpmResult ipm_counters_solve_maratos(hven::IpmOptions o, double radius, double angle) {
    o.common.verbosity = hven::Verbosity::kSilent;
    hven::IpmSolver solver(o);
    const auto model = hven::detail::make_nlp_program(std::make_shared<IpmCountersMaratos>());
    hven::Vec x0(2);
    x0 << radius * std::cos(angle), radius * std::sin(angle);
    return solver.solve(hven::detail::problem_ref(*model), x0);
}

} // namespace

TEST(IpmCounters, SecondOrderCorrectionsAreCountedAsSocStepsAndNotAsWatchdogArms) {
    hven::IpmOptions o;
    o.advanced.max_second_order_corrections = 4;
    const IpmResult on = ipm_counters_solve_maratos(o, 1.0, 0.5);
    ASSERT_EQ(on.status, hven::SolveStatus::kOptimal);
    EXPECT_GT(on.counters.soc_steps, 0) << "non-vacuous: the correction ran";
    EXPECT_EQ(on.counters.watchdog_activations, 0);
    // Every correction attempt is a rejection the chain resolved at depth [0] or went on past.
    EXPECT_GE(on.counters.soc_steps, on.counters.recovery_depth_histogram[0]);
    EXPECT_GT(on.counters.recovery_depth_histogram[0], 0);
    o.advanced.max_second_order_corrections = 0;
    const IpmResult off = ipm_counters_solve_maratos(o, 1.0, 0.5);
    EXPECT_EQ(off.counters.soc_steps, 0);
}

TEST(IpmCounters, TheWatchdogArmsAreCountedAsWatchdogActivationsAndNotAsSocSteps) {
    hven::IpmOptions o;
    o.advanced.acceptance_mode = hven::IpmAcceptanceMode::kFunnel;
    o.advanced.max_second_order_corrections = 0;
    o.advanced.watchdog = true;
    const IpmResult r = ipm_counters_solve_maratos(o, 2.0, 1.6);
    ASSERT_EQ(r.status, hven::SolveStatus::kOptimal);
    EXPECT_GT(r.counters.watchdog_activations, 0) << "non-vacuous: the watchdog armed";
    EXPECT_EQ(r.counters.soc_steps, 0);
    o.advanced.watchdog = false;
    const IpmResult off = ipm_counters_solve_maratos(o, 2.0, 1.6);
    EXPECT_EQ(off.counters.watchdog_activations, 0);
}

TEST(IpmCounters, TheTableNamesEveryFieldOfTheGroup) {
    // A field added to or dropped from the struct without its X() entry moves
    // sizeof; the array counts for five.
    EXPECT_EQ(hven::kIpmCountersFieldCount, 14u);
    static_assert(sizeof(hven::IpmCounters) ==
                  (hven::kIpmCountersFieldCount + 4) * sizeof(hven::Index));
    // Shared names, spelled as the SQP group spells them.
    hven::IpmCounters c;
    hven::SqpCounters s;
    c.factorizations = s.factorizations;
    c.symbolic_analyses = s.symbolic_analyses;
    c.restoration_entries = s.restoration_entries;
    c.restoration_iters = s.restoration_iters;
    c.soc_steps = s.soc_steps;
    c.nonfinite_step_rollbacks = s.nonfinite_step_rollbacks;
    c.polish_ignored = s.polish_ignored;
    c.start_level_used = s.start_level_used;
    c.payload_ignored = s.payload_ignored;
    c.dense_solve_residual_refusals = s.dense_solve_residual_refusals;
    c.dense_factorizations = s.dense_factorizations;
    SUCCEED();
}

TEST(IpmCounters, ASolveFillsTheWholeSolveGroupAndTheWorkAgrees) {
    const IpmResult r = ipm_counters_solve_hs071({});
    ASSERT_EQ(r.status, hven::SolveStatus::kOptimal);
    EXPECT_GT(r.counters.factorizations, 0);
    EXPECT_EQ(r.counters.factorizations, r.work.factorizations);
    // A fresh solver: this call's analyses are all of its lifetime's, and at least one.
    EXPECT_GT(r.counters.symbolic_analyses, 0);
    EXPECT_EQ(r.counters.symbolic_analyses, r.kkt_analyses_total);
    EXPECT_EQ(r.counters.payload_ignored, 0);
    EXPECT_EQ(r.counters.polish_ignored, 0);
    EXPECT_EQ(r.counters.restoration_entries, 0);
    EXPECT_EQ(r.counters.restoration_iters, 0);
    EXPECT_EQ(r.counters.dense_solve_residual_refusals, 0);
    EXPECT_EQ(r.counters.start_level_used, hven::StartLevel::kCold) << "no payload";
}

// M8 AP-5 G7 (Q7): the convergence block's feasibility summary is max(equality,
// inequality) of the same row, on a problem with both kinds of row.
TEST(IpmConvergence, FeasibilityIsTheMaxOfTheEqualityAndInequalityResiduals) {
    hven::IpmOptions o;
    o.common.max_iterations = 3; // an unconverged row, so both residuals are nonzero
    const IpmResult r = ipm_counters_solve_hs071(o);
    ASSERT_EQ(r.status, hven::SolveStatus::kIterationLimit);
    const hven::IpmConvergence &c = r.convergence;
    ASSERT_TRUE(std::isfinite(c.equality) && std::isfinite(c.inequality));
    ASSERT_NE(c.equality, c.inequality) << "premise: the max is not decided by a tie";
    EXPECT_EQ(c.feasibility, std::max(c.equality, c.inequality));
    EXPECT_TRUE(std::isfinite(c.stationarity));
    EXPECT_TRUE(std::isfinite(c.complementarity));
}

TEST(IpmCounters, TheLedgerRecordCarriesTheResultsCounters) {
    hven::Ledger ledger;
    const IpmResult r = ipm_counters_solve_hs071({}, &ledger);
    ASSERT_EQ(ledger.ipm_records().size(), 1u);
    const hven::IpmSolveRecord &rec = ledger.ipm_records()[0];
    const hven::IpmCounters &a = rec.counters;
    const hven::IpmCounters &b = r.counters;
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.f, b.f) << #f;
    HVEN_IPM_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
    EXPECT_GT(a.factorizations, 0);
    // The scalar fields are read from the group.
    EXPECT_EQ(rec.factorizations, b.factorizations);
    EXPECT_EQ(rec.analyses, b.symbolic_analyses);
    EXPECT_EQ(rec.soc_steps_taken, b.soc_steps);
    EXPECT_EQ(rec.watchdog_activations, b.watchdog_activations);
}

TEST(IpmPhaseDiagnostics, TheFilterDefaultReportsTheFilterAndTheMonitoredGovernorOnly) {
    const IpmResult r = ipm_counters_solve_hs071({});
    const IpmPhaseReport *p = hven::last_ran_phase(r);
    ASSERT_NE(p, nullptr);
    EXPECT_FALSE(p->funnel_width.has_value());
    ASSERT_TRUE(p->filter_size.has_value());
    EXPECT_GT(*p->filter_size, 0);
    EXPECT_TRUE(p->filter_resets.has_value());
    EXPECT_TRUE(p->monotone_switches.has_value());
    EXPECT_TRUE(p->monotone_iterations.has_value());
    // Restoration is off by default: no strategy, nothing reported.
    EXPECT_FALSE(p->restoration_entries.has_value());
    EXPECT_FALSE(p->restoration_iters.has_value());
    EXPECT_FALSE(p->proximal_regularization_primal.has_value());
    EXPECT_FALSE(p->proximal_regularization_dual.has_value());
    EXPECT_EQ(p->kkt_factor_status, hven::IpmKktFactorStatus::kSuccess);
}

TEST(IpmPhaseDiagnostics, TheFunnelReportsItsWidthAndNotTheFilter) {
    hven::IpmOptions o;
    o.advanced.acceptance_mode = hven::IpmAcceptanceMode::kFunnel;
    const IpmResult r = ipm_counters_solve_hs071(o);
    const IpmPhaseReport *p = hven::last_ran_phase(r);
    ASSERT_NE(p, nullptr);
    ASSERT_TRUE(p->funnel_width.has_value());
    EXPECT_GT(*p->funnel_width, 0.0);
    EXPECT_FALSE(p->filter_size.has_value());
    EXPECT_FALSE(p->filter_resets.has_value());
}

TEST(IpmPhaseDiagnostics, TheProximalModeReportsItsShiftsAndRestorationItsCounts) {
    hven::IpmOptions o;
    o.advanced.inertia_mode = hven::IpmInertiaMode::kProximalRegularization;
    o.advanced.restoration_mode = hven::IpmRestorationMode::kL1Nested;
    const IpmResult r = ipm_counters_solve_hs071(o);
    const IpmPhaseReport *p = hven::last_ran_phase(r);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(p->proximal_regularization_primal.has_value());
    EXPECT_TRUE(p->proximal_regularization_dual.has_value());
    ASSERT_TRUE(p->restoration_entries.has_value());
    ASSERT_TRUE(p->restoration_iters.has_value());
    // One phase: the whole-solve counters are that phase's values.
    EXPECT_EQ(r.counters.restoration_entries, *p->restoration_entries);
    EXPECT_EQ(r.counters.restoration_iters, *p->restoration_iters);
}

TEST(IpmPhaseDiagnostics, LastRanPhaseSkipsThePhasesThatDidNotRun) {
    IpmResult r;
    EXPECT_EQ(hven::last_ran_phase(r), nullptr);
    r.phases.resize(3);
    EXPECT_EQ(hven::last_ran_phase(r), nullptr);
    r.phases[0].ran = true;
    r.phases[1].ran = true;
    EXPECT_EQ(hven::last_ran_phase(r), &r.phases[1]);
    r.phases[2].ran = true;
    EXPECT_EQ(hven::last_ran_phase(r), &r.phases[2]);
}
