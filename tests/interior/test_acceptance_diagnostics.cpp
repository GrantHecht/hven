// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

///////////////////////////////////////////////////////////////////////////////
// Unit tests for AcceptanceStrategy::append_diagnostics() — the solver-level
// observability hook added alongside the funnel/filter acceptance strategies.
//
// Covers:
//   - the default (base-class) body is a no-op: a strategy that does not
//     override it leaves the hven::solvers::IpmResult untouched;
//   - a fake strategy's override IS invoked through the virtual dispatch, the
//     same call shape run_phase_sequence() uses (see interior_point_solver.cpp);
//   - the two real overrides (FunnelAcceptance, FilterAcceptance) report their
//     documented fields (funnel_acceptance.h / filter_acceptance.h);
//   - a default-constructed hven::solvers::IpmResult carries the three sentinel
//     values (-1.0 / -1 / -1) documented on the fields in interior_point_solver.h.
//
// UNITY RULE: anonymous namespace does not protect names against the unity
// build — every helper/class here is prefixed Diag* to stay globally unique
// across tests/cpp/ (grep-confirmed no other "Diag"-prefixed symbol exists).
///////////////////////////////////////////////////////////////////////////////

#include "progress_measures_test_utils.h"

#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/filter_acceptance.h"
#include "hven/detail/globalization/funnel_acceptance.h"
#include "hven/drivers/interior_point_solver.h"

#include <gtest/gtest.h>

#include <cmath>

namespace {

using hven::solvers::AcceptanceStrategy;
using hven::solvers::FilterAcceptance;
using hven::solvers::FunnelAcceptance;
using hven::solvers::InteriorPointSolver;
using hven::solvers::kFunnelInfeasibilityFactor;
using hven::solvers::ProgressMeasures;
using TychoTest::pm;

// Bare AcceptanceStrategy: implements only the pure-virtual surface and does
// NOT override append_diagnostics(), so it exercises the base class's default
// no-op body. drives_classic_path() is arbitrary here (never read by these
// tests); is_iterate_acceptable()/is_infeasibility_sufficiently_reduced()
// return fixed values since neither is exercised by these tests either.
class DiagBareAcceptance : public AcceptanceStrategy {
  public:
    bool drives_classic_path() const override { return false; }
    bool is_iterate_acceptable(const ProgressMeasures &, const ProgressMeasures &,
                               const ProgressMeasures &, double, double) override {
        return true;
    }
    bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &,
                                               const ProgressMeasures &) const override {
        return true;
    }
    void reset() override {}
};

// Fake AcceptanceStrategy whose append_diagnostics() override is invoked as a
// unit: increments a call counter and stamps a recognizable value into
// last_funnel_width_ so the test can tell the OVERRIDE ran (as opposed to the
// base's no-op).
class DiagFakeAcceptance : public AcceptanceStrategy {
  public:
    bool drives_classic_path() const override { return false; }
    bool is_iterate_acceptable(const ProgressMeasures &, const ProgressMeasures &,
                               const ProgressMeasures &, double, double) override {
        return true;
    }
    bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &,
                                               const ProgressMeasures &) const override {
        return true;
    }
    void reset() override {}

    void append_diagnostics(hven::solvers::IpmResult &result) const override {
        ++calls_;
        result.last_funnel_width = 42.0;
    }

    mutable int calls_ = 0;
};

// The default body is a no-op: a freshly default-constructed hven::solvers::IpmResult at its
// sentinel values is untouched by a strategy that doesn't override append_diagnostics.
TEST(AcceptanceDiagnostics, DefaultIsNoop) {
    DiagBareAcceptance strategy;
    // A default hven::solvers::IpmResult IS the reset state (M6 W5 T8.4): every sentinel the
    // retired reset_accumulators() wrote is a default member initializer now.
    hven::solvers::IpmResult result;
    ASSERT_DOUBLE_EQ(result.last_funnel_width, -1.0);
    ASSERT_EQ(result.last_filter_size, -1);
    ASSERT_EQ(result.last_filter_resets, -1);

    // Call it through a base-class reference, exactly like run_phase_sequence()
    // does through the acceptance_ unique_ptr<AcceptanceStrategy>.
    AcceptanceStrategy &base = strategy;
    base.append_diagnostics(result);

    EXPECT_DOUBLE_EQ(result.last_funnel_width, -1.0);
    EXPECT_EQ(result.last_filter_size, -1);
    EXPECT_EQ(result.last_filter_resets, -1);
}

// A strategy's override IS invoked through virtual dispatch off a base-class
// reference (the exact call shape run_phase_sequence() uses:
// this->acceptance_->append_diagnostics(this->result_)).
TEST(AcceptanceDiagnostics, FakeStrategyOverrideIsInvoked) {
    DiagFakeAcceptance strategy;
    // A default hven::solvers::IpmResult IS the reset state (M6 W5 T8.4): every sentinel the
    // retired reset_accumulators() wrote is a default member initializer now.
    hven::solvers::IpmResult result;

    AcceptanceStrategy &base = strategy;
    base.append_diagnostics(result);

    EXPECT_EQ(strategy.calls_, 1);
    EXPECT_DOUBLE_EQ(result.last_funnel_width, 42.0);

    base.append_diagnostics(result);
    EXPECT_EQ(strategy.calls_, 2);
}

// FunnelAcceptance::append_diagnostics() reports the current width_ verbatim.
// θ₀ = 4.0 ⇒ κ̄·θ₀ = 1.5·4 = 6.0 > τ̄ = 1.0 ⇒ τ = 6.0 (init rule, see
// funnel_acceptance.h). Priming trial well outside the funnel so the base's
// membership test rejects it before any width update runs.
TEST(AcceptanceDiagnostics, FunnelReportsWidth) {
    FunnelAcceptance funnel;
    const bool primed = funnel.is_iterate_acceptable(pm(4.0, 0.0, 0.0), pm(100.0, 0.0, 0.0),
                                                     pm(0.0, 0.0, 0.0), 1.0, 1.0);
    ASSERT_FALSE(primed);
    ASSERT_DOUBLE_EQ(funnel.funnel_width(), kFunnelInfeasibilityFactor * 4.0);
    // A default hven::solvers::IpmResult IS the reset state (M6 W5 T8.4): every sentinel the
    // retired reset_accumulators() wrote is a default member initializer now.
    hven::solvers::IpmResult result;
    static_cast<AcceptanceStrategy &>(funnel).append_diagnostics(result);

    EXPECT_DOUBLE_EQ(result.last_funnel_width, funnel.funnel_width());
    EXPECT_DOUBLE_EQ(result.last_funnel_width, kFunnelInfeasibilityFactor * 4.0);
    // Untouched by FunnelAcceptance's override (funnel doesn't report filter
    // fields).
    EXPECT_EQ(result.last_filter_size, -1);
    EXPECT_EQ(result.last_filter_resets, -1);
}

// FunnelAcceptance::append_diagnostics() reports the -1.0 sentinel when the
// acceptance test never ran (e.g. phase converged at initial iterate). The
// width_ stays at its +∞ uninitialized sentinel if is_iterate_acceptable was
// never called; append_diagnostics converts +∞ to -1.0.
TEST(AcceptanceDiagnostics, DiagFunnelUninitializedWidthSentinel) {
    FunnelAcceptance funnel;
    // Do NOT call is_iterate_acceptable — width_ remains at +∞.
    ASSERT_FALSE(std::isfinite(funnel.funnel_width()));
    // A default hven::solvers::IpmResult IS the reset state (M6 W5 T8.4): every sentinel the
    // retired reset_accumulators() wrote is a default member initializer now.
    hven::solvers::IpmResult result;
    static_cast<AcceptanceStrategy &>(funnel).append_diagnostics(result);

    EXPECT_DOUBLE_EQ(result.last_funnel_width, -1.0);
    // Untouched by FunnelAcceptance's override.
    EXPECT_EQ(result.last_filter_size, -1);
    EXPECT_EQ(result.last_filter_resets, -1);
}

// FilterAcceptance::append_diagnostics() reports filter_size() and
// filter_resets(). One accepted H-type step (m_f = 0 routes every call to the
// H-type delegate) augments the filter to size 1 and leaves the reset counter
// at 0 (nowhere near the kFilterResetTrigger streak).
TEST(AcceptanceDiagnostics, FilterReportsSizeAndResets) {
    FilterAcceptance filter;
    const bool accepted = filter.is_iterate_acceptable(pm(/*theta=*/4.0, /*phi=*/20.0, 0.0),
                                                       pm(/*theta=*/1.0, /*phi=*/20.0, 0.0),
                                                       pm(0.0, 0.0, 0.0), 1.0, 1.0);
    ASSERT_TRUE(accepted);
    ASSERT_EQ(filter.filter_size(), 1u);
    ASSERT_EQ(filter.filter_resets(), 0);
    // A default hven::solvers::IpmResult IS the reset state (M6 W5 T8.4): every sentinel the
    // retired reset_accumulators() wrote is a default member initializer now.
    hven::solvers::IpmResult result;
    static_cast<AcceptanceStrategy &>(filter).append_diagnostics(result);

    EXPECT_EQ(result.last_filter_size, 1);
    EXPECT_EQ(result.last_filter_resets, 0);
    // Untouched by FilterAcceptance's override (filter doesn't report the
    // funnel field).
    EXPECT_DOUBLE_EQ(result.last_funnel_width, -1.0);
}

// A DEFAULT hven::solvers::IpmResult IS the reset state (M6 W5 T8.4). reset_accumulators()
// was retired because a solve builds a fresh result rather than clearing a
// carried one, so what used to be "reset restores the sentinels" is now "the
// sentinels are the defaults" -- the same promise, made by the type instead of
// by a call. A field written on one value cannot reach the next.
TEST(AcceptanceDiagnostics, ADefaultResultCarriesTheSentinels) {
    hven::solvers::IpmResult written;
    written.last_funnel_width = 6.0;
    written.last_filter_size = 3;
    written.last_filter_resets = 2;

    const hven::solvers::IpmResult fresh;
    EXPECT_DOUBLE_EQ(fresh.last_funnel_width, -1.0);
    EXPECT_EQ(fresh.last_filter_size, -1);
    EXPECT_EQ(fresh.last_filter_resets, -1);
    // And the base's own discipline, which the same default carries: every
    // shared diagnostic UNMEASURED rather than zero.
    EXPECT_TRUE(std::isnan(fresh.stationarity));
    EXPECT_TRUE(std::isnan(fresh.feasibility_e));
    EXPECT_TRUE(std::isnan(fresh.feasibility_i));
    EXPECT_TRUE(std::isnan(fresh.complementarity));
    EXPECT_EQ(fresh.ce.size(), 0);
    EXPECT_EQ(fresh.ci.size(), 0);
}

} // namespace
