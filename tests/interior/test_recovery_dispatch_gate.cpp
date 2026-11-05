// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

///////////////////////////////////////////////////////////////////////////////
// Unit test for the recovery-dispatch gate (IpmSolver globalization).
//
// The merit line search records its accept/reject verdict on the per-iteration
// IpmIterate (accepted_), and alg_impl drives the RecoveryChain hook only when
// that verdict is "rejected" AND the KKT step direction was usable (GoodStep).
// The gate condition is factored into should_dispatch_recovery() so it has a
// single definition; this test exercises that predicate directly and then
// drives it end-to-end with:
//   - a stub AcceptanceStrategy, whose classic_line_search stamps the accepted_
//     signal onto Citer exactly as the real merit variants do at the merit
//     test, and
//   - a recording RecoveryChain, which counts on_step_rejected invocations,
// asserting the hook fires on a rejected step, stays silent on an accepted
// step, and stays silent when the step direction was non-finite (no line
// search runs, so Citer keeps its fresh default and GoodStep is false).
///////////////////////////////////////////////////////////////////////////////

#include "solver_test_utils.h"

#include "hven/detail/drivers/ipm_solver_impl.h"
#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/classic_adaptive_governor.h"
#include "hven/detail/globalization/filter_acceptance.h"
#include "hven/detail/globalization/funnel_acceptance.h"
#include "hven/detail/globalization/globalization_mechanism.h"
#include "hven/detail/globalization/modern_merit.h"
#include "hven/detail/globalization/monitored_governor.h"
#include "hven/detail/globalization/recovery_chain.h"
#include "hven/drivers/ipm_iterate.h"
#include "hven/drivers/ipm_options.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include <Eigen/Core>

namespace {

using hven::AcceptanceMode;
using hven::AcceptanceStrategy;
using hven::BarrierGovernorMode;
using hven::FilterAcceptance;
using hven::FunnelAcceptance;
using hven::GlobalizationMechanism;
using hven::Index;
using hven::IpmIterate;
using hven::IpmOptions;
using hven::IpmSolver;
using hven::kRecoveryDepthUnresolved;
using hven::MonitoredBarrierGovernor;
using hven::ProgressMeasures;
using hven::RecoveryChain;
using hven::should_dispatch_recovery;
using hven::SolverContext;
using TychoTest::InertSolverContext;

// Stub AcceptanceStrategy: classic_line_search stamps the configured
// accept/reject verdict onto Citer.accepted_ (as the real merit variants do at
// the merit test) and returns a unit step. The generic filter/funnel hooks are
// unused here.
class GateStubAcceptance : public AcceptanceStrategy {
  public:
    bool drives_classic_path() const override { return true; }
    explicit GateStubAcceptance(bool accept) : accept_(accept) {}

    bool is_iterate_acceptable(const ProgressMeasures &, const ProgressMeasures &,
                               const ProgressMeasures &, double, double) override {
        return false;
    }
    bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &,
                                               const ProgressMeasures &) const override {
        return false;
    }
    void reset() override {}

    double classic_line_search(hven::LineSearchMode, double, double, double, double,
                               Eigen::VectorXd &, Eigen::VectorXd &, Eigen::VectorXd &,
                               Eigen::VectorXd &, Eigen::VectorXd &, IpmIterate &Citer,
                               const std::vector<IpmIterate> &) override {
        Citer.accepted = accept_;
        return 1.0;
    }

  private:
    bool accept_;
};

// Recording RecoveryChain: counts hook invocations so the test can assert the
// gate fires it exactly when expected. Returns kAcceptAsIs and touches none of
// its arguments (this test exercises the dispatch GATE, not the correction
// itself — see test_soc.cpp for the SOC policy).
class GateRecordingRecovery : public RecoveryChain {
  public:
    Action on_step_rejected(IpmIterate &, const std::vector<IpmIterate> &, SolverContext &,
                            AcceptanceStrategy &, GlobalizationMechanism &, hven::LineSearchMode,
                            double, double, double, double, Eigen::VectorXd &, Eigen::VectorXd &,
                            Eigen::VectorXd &, Eigen::VectorXd &, Eigen::VectorXd &, double &,
                            double &, double &, Index &, int &, Index &) override {
        ++calls_;
        return Action::kAcceptAsIs;
    }
    void reset() override {}

    int calls_ = 0;
};

// Inert GlobalizationMechanism: GateRecordingRecovery ignores it, so its bodies are
// never reached; present only to satisfy the on_step_rejected signature.
class GateUnusedMechanism : public GlobalizationMechanism {
  public:
    double compute_step(hven::LineSearchMode, double, double, double, double, Eigen::VectorXd &,
                        Eigen::VectorXd &, Eigen::VectorXd &, Eigen::VectorXd &, Eigen::VectorXd &,
                        AcceptanceStrategy &, double &, double &, IpmIterate &,
                        const std::vector<IpmIterate> &, SolverContext &) override {
        ADD_FAILURE() << "mechanism must not be reached: the recording recovery ignores it";
        return 1.0;
    }
    void max_primal_dual_step(Eigen::VectorXd &, Eigen::VectorXd &, double, double &, double &,
                              const SolverContext &) override {
        ADD_FAILURE() << "mechanism must not be reached: the recording recovery ignores it";
    }
    void reset() override {}
};

// Faithful replica of alg_impl's recovery-hook wiring: dispatch only when the
// shared gate predicate says so. The extra working-set arguments are inert here
// (GateRecordingRecovery ignores them) and bound to local dummies.
void drive_gate(bool good_step, IpmIterate &citer, RecoveryChain &recovery,
                AcceptanceStrategy &acceptance, const std::vector<IpmIterate> &iters,
                SolverContext &ctx) {
    if (should_dispatch_recovery(good_step, citer)) {
        GateUnusedMechanism mechanism;
        Eigen::VectorXd v;
        double alpha = 1.0, alphap = 1.0, alphad = 1.0;
        Index soc_steps = 0;
        int resolved_depth = kRecoveryDepthUnresolved;
        Index watchdog_activations = 0;
        recovery.on_step_rejected(citer, iters, ctx, acceptance, mechanism,
                                  hven::LineSearchMode::kAugmentedLagrangian, 1.0, 1e-3, 0.0, 0.0,
                                  v, v, v, v, v, alpha, alphap, alphad, soc_steps, resolved_depth,
                                  watchdog_activations);
    }
}

TEST(RecoveryDispatchGate, PredicateTruthTable) {
    IpmIterate rejected;
    rejected.accepted = false;
    IpmIterate accepted;
    accepted.accepted = true;

    EXPECT_TRUE(should_dispatch_recovery(/*good_step=*/true, rejected));
    EXPECT_FALSE(should_dispatch_recovery(/*good_step=*/true, accepted));
    EXPECT_FALSE(should_dispatch_recovery(/*good_step=*/false, rejected));
    EXPECT_FALSE(should_dispatch_recovery(/*good_step=*/false, accepted));
}

TEST(RecoveryDispatchGate, StubAcceptanceDrivesHook) {
    // Minimal SolverContext for the recovery signature. GateRecordingRecovery
    // never dereferences it (the hook only bumps its counter), so an inert,
    // all-zero-dimension context suffices.
    InertSolverContext inert;
    SolverContext ctx = inert.ctx();
    const std::vector<IpmIterate> iters;
    Eigen::VectorXd v; // inert working vectors for the stub line search

    // Rejected step (GoodStep): the stub stamps accepted_ = false; gate fires.
    {
        GateRecordingRecovery recovery;
        GateStubAcceptance acceptance(/*accept=*/false);
        IpmIterate citer;
        acceptance.classic_line_search(hven::LineSearchMode::kAugmentedLagrangian, 1.0, 1e-3, 0.0,
                                       0.0, v, v, v, v, v, citer, iters);
        EXPECT_FALSE(citer.accepted);
        drive_gate(/*good_step=*/true, citer, recovery, acceptance, iters, ctx);
        EXPECT_EQ(recovery.calls_, 1);
    }

    // Accepted step (GoodStep): the stub stamps accepted_ = true; gate silent.
    {
        GateRecordingRecovery recovery;
        GateStubAcceptance acceptance(/*accept=*/true);
        IpmIterate citer;
        acceptance.classic_line_search(hven::LineSearchMode::kAugmentedLagrangian, 1.0, 1e-3, 0.0,
                                       0.0, v, v, v, v, v, citer, iters);
        EXPECT_TRUE(citer.accepted);
        drive_gate(/*good_step=*/true, citer, recovery, acceptance, iters, ctx);
        EXPECT_EQ(recovery.calls_, 0);
    }

    // Non-finite step direction (!GoodStep): alg_impl runs no line search, so
    // Citer keeps its fresh default (accepted_ == false); the gate stays silent
    // because GoodStep is false.
    {
        GateRecordingRecovery recovery;
        GateStubAcceptance acceptance(/*accept=*/false); // never reached (gate stays silent)
        IpmIterate citer;                                // no line search ran
        EXPECT_FALSE(citer.accepted);
        drive_gate(/*good_step=*/false, citer, recovery, acceptance, iters, ctx);
        EXPECT_EQ(recovery.calls_, 0);
    }
}

// Settings::validate() no longer rejects the SOC / extended-backtracking knobs
// in combination with a generic-path acceptance strategy: those links re-drive
// the acceptance backtrack through the mechanism, which dispatches to the
// generic AcceptanceStrategy surface (see the guard's removal in ipm_solver.cpp and
// GlobalizationMechanism::run_acceptance_backtrack). Exercise both generic
// strategies against both knobs, paired with the monotone-barrier opt-in
// funnel/filter separately require (so the OTHER, still-live guard does not
// mask the result).
TEST(RecoveryDispatchGate, ValidateAcceptsFunnelWithMaxSoc) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFunnel;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    settings.advanced.max_second_order_corrections = 1;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateAcceptsFunnelWithLsExtendedIters) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFunnel;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    settings.advanced.max_extended_backtracks = 1;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateAcceptsFilterWithMaxSoc) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFilter;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    settings.advanced.max_second_order_corrections = 1;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateAcceptsFilterWithLsExtendedIters) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFilter;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    settings.advanced.max_extended_backtracks = 1;
    EXPECT_NO_THROW(hven::validate(settings));
}

// Settings::validate()'s feasibility-restoration budget guard: max_feas_rest_
// must be non-negative (0 disables restoration entry; negative is invalid). The
// restoration mode itself composes with every acceptance strategy and governor
// — validate() adds no new combination restrictions for it.
TEST(RecoveryDispatchGate, ValidateRejectsNegativeMaxFeasRest) {
    IpmOptions settings;
    settings.advanced.max_restoration_entries = -1;
    EXPECT_THROW(hven::validate(settings), std::invalid_argument);
}

TEST(RecoveryDispatchGate, ValidateAcceptsZeroMaxFeasRest) {
    IpmOptions settings;
    settings.advanced.max_restoration_entries = 0;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateAcceptsRestorationWithClassicMerit) {
    IpmOptions settings;
    settings.advanced.restoration_mode = hven::RestorationMode::kProximalSwitch;
    settings.advanced.acceptance_mode = AcceptanceMode::kClassicMerit;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateAcceptsRestorationWithFilterMonitored) {
    IpmOptions settings;
    settings.advanced.restoration_mode = hven::RestorationMode::kProximalSwitch;
    settings.advanced.acceptance_mode = AcceptanceMode::kFilter;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    EXPECT_NO_THROW(hven::validate(settings));
}

// classic_merit is unaffected by the widened guard: max_soc_/ls_extended_iters_
// combine with it exactly as before.
TEST(RecoveryDispatchGate, ValidateAcceptsClassicMeritWithMaxSoc) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kClassicMerit;
    settings.advanced.max_second_order_corrections = 1;
    settings.advanced.max_extended_backtracks = 1;
    EXPECT_NO_THROW(hven::validate(settings));
}

// merit composes with the recovery knobs too (no monotone opt-in needed — only
// funnel/filter carry that separate requirement).
TEST(RecoveryDispatchGate, ValidateAcceptsMeritWithMaxSoc) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kMerit;
    settings.advanced.max_second_order_corrections = 1;
    settings.advanced.max_extended_backtracks = 1;
    EXPECT_NO_THROW(hven::validate(settings));
}

// Every recovery link (SOC, extended backtracking, watchdog) combines freely
// with every acceptance strategy, including the two non-classic strategies.
TEST(RecoveryDispatchGate, ValidateAcceptsFunnelWithWatchdog) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFunnel;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    settings.advanced.watchdog = true;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateAcceptsFilterWithWatchdog) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFilter;
    // M7 W2 D1 (2026-09-18): the governor is written EXPLICITLY here, where it
    // used to be inherited. This case is about the watchdog composing with a
    // non-classic strategy above the FREE-ONLY governor, opted into by
    // never_monotone -- and never_monotone is contradictory with the monitored
    // governor, which is now the default. Writing classic_adaptive restores
    // exactly the combination this test was written to exercise; leaving it out
    // would silently turn the case into a refusal test.
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kClassicAdaptive;
    settings.advanced.never_monotone = true;
    settings.advanced.watchdog = true;
    EXPECT_NO_THROW(hven::validate(settings));
}

// Settings::validate()'s barrier_governor_mode/never_monotone truth table (see the
// guard's comment in ipm_solver.cpp): funnel/filter with barrier_governor_mode=
// classic_adaptive (monitored is the default since M7 W2 D1, 2026-09-18) and
// never_monotone=false rejects; either opt-in (barrier_governor_mode=monitored, or
// never_monotone=true) accepts; the two opt-ins together are a direct
// contradiction and reject; classic_merit/merit are unaffected in every
// combination.

TEST(RecoveryDispatchGate, ValidateRejectsFunnelWithClassicAdaptiveGovernor) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFunnel;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kClassicAdaptive;
    settings.advanced.never_monotone = false;
    EXPECT_THROW(hven::validate(settings), std::invalid_argument);
}

TEST(RecoveryDispatchGate, ValidateRejectsFilterWithClassicAdaptiveGovernor) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFilter;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kClassicAdaptive;
    settings.advanced.never_monotone = false;
    EXPECT_THROW(hven::validate(settings), std::invalid_argument);
}

TEST(RecoveryDispatchGate, ValidateAcceptsFunnelWithMonitoredGovernor) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFunnel;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateAcceptsFilterWithMonitoredGovernor) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFilter;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateAcceptsFunnelWithNeverMonotone) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFunnel;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kClassicAdaptive;
    settings.advanced.never_monotone = true;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateAcceptsFilterWithNeverMonotone) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kFilter;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kClassicAdaptive;
    settings.advanced.never_monotone = true;
    EXPECT_NO_THROW(hven::validate(settings));
}

TEST(RecoveryDispatchGate, ValidateRejectsNeverMonotoneWithMonitoredGovernor) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kClassicMerit;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    settings.advanced.never_monotone = true;
    EXPECT_THROW(hven::validate(settings), std::invalid_argument);
}

TEST(RecoveryDispatchGate, ValidateAcceptsClassicMeritWithClassicAdaptiveGovernor) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kClassicMerit;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kClassicAdaptive;
    settings.advanced.never_monotone = false;
    EXPECT_NO_THROW(hven::validate(settings));
}

// merit is a generic-path strategy like funnel/filter but is explicitly
// unaffected by the monotone-safeguard guard -- only funnel/filter are gated.
TEST(RecoveryDispatchGate, ValidateAcceptsMeritWithClassicAdaptiveGovernor) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kMerit;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kClassicAdaptive;
    settings.advanced.never_monotone = false;
    EXPECT_NO_THROW(hven::validate(settings));
}

// classic_merit + monitored is allowed opt-in (bit-identity is about the
// DEFAULT governor selection, not about excluding classic_merit from pairing
// with the monitored governor).
TEST(RecoveryDispatchGate, ValidateAcceptsClassicMeritWithMonitoredGovernor) {
    IpmOptions settings;
    settings.advanced.acceptance_mode = AcceptanceMode::kClassicMerit;
    settings.advanced.barrier_governor_mode = BarrierGovernorMode::kMonitored;
    EXPECT_NO_THROW(hven::validate(settings));
}

} // namespace

// Settings::acceptance_strategy_ = funnel/filter must construct the matching
// concrete strategy and drive the generic AcceptanceStrategy path, exactly
// like the modern merit family (see test_merit_rules.cpp's DrivesGenericPath).
//
// Test access: these two cases call the private
// IpmSolver::rebuild_globalization_components() and read the private
// acceptance_ member directly, so they are declared as friends in ipm_solver.h
// (narrowly, by their gtest-generated class names) rather than IpmSolver
// exposing a public rebuild hook for this alone. gtest TEST() macros expand
// to a class at the enclosing scope, and a friend declaration cannot name a
// class inside an anonymous namespace from a production header, so these two
// cases live at global scope instead of inside the anonymous namespace above.

TEST(RecoveryDispatchGate, FunnelSelectionConstructsFunnelAcceptance) {
    hven::IpmSolverImpl solver;
    {
        auto o = solver.options();
        o.advanced.acceptance_mode = hven::AcceptanceMode::kFunnel;
        // The monotone-barrier opt-in that validate() requires alongside
        // funnel/filter. Written here because set_options() checks the WHOLE
        // value -- before M6 W5 T8.3 this test wrote the strategy straight into
        // the mutable settings() reference, which no per-field setter and no
        // whole-struct check ever saw. never_monotone rather than a governor
        // change, so the only dispatch input that moves is the acceptance one
        // this test is about.
        //
        // M7 W2 D1 (2026-09-18) made the governor a WRITE rather than an
        // inheritance, and that sentence above is why: the default governor is
        // now monitored, never_monotone contradicts it, and the pre-event
        // classic_adaptive has to be named for the acceptance strategy to stay
        // the only moving dispatch input.
        o.advanced.barrier_governor_mode = hven::BarrierGovernorMode::kClassicAdaptive;
        o.advanced.never_monotone = true;
        solver.set_options(std::move(o));
    }
    solver.rebuild_globalization_components();
    hven::AcceptanceStrategy *acceptance = solver.acceptance_.get();
    ASSERT_NE(dynamic_cast<hven::FunnelAcceptance *>(acceptance), nullptr);
    EXPECT_FALSE(acceptance->drives_classic_path());
}

TEST(RecoveryDispatchGate, FilterSelectionConstructsFilterAcceptance) {
    hven::IpmSolverImpl solver;
    {
        auto o = solver.options();
        o.advanced.acceptance_mode = hven::AcceptanceMode::kFilter;
        // The monotone-barrier opt-in that validate() requires alongside
        // funnel/filter. Written here because set_options() checks the WHOLE
        // value -- before M6 W5 T8.3 this test wrote the strategy straight into
        // the mutable settings() reference, which no per-field setter and no
        // whole-struct check ever saw. never_monotone rather than a governor
        // change, so the only dispatch input that moves is the acceptance one
        // this test is about.
        //
        // M7 W2 D1 (2026-09-18): the governor is written, not inherited -- see
        // the funnel case above for the reason. Note that `filter` is now the
        // DEFAULT acceptance strategy, so this test would pass without the
        // acceptance write; it is kept explicit because what it pins is the
        // DISPATCH (selector -> constructed component), not the default.
        o.advanced.barrier_governor_mode = hven::BarrierGovernorMode::kClassicAdaptive;
        o.advanced.never_monotone = true;
        solver.set_options(std::move(o));
    }
    solver.rebuild_globalization_components();
    hven::AcceptanceStrategy *acceptance = solver.acceptance_.get();
    ASSERT_NE(dynamic_cast<hven::FilterAcceptance *>(acceptance), nullptr);
    EXPECT_FALSE(acceptance->drives_classic_path());
}

// IpmOptions::barrier_governor_mode = monitored must construct
// MonitoredBarrierGovernor, and classic_adaptive must construct
// ClassicAdaptiveGovernor. BOTH ARE ASSERTED DIRECTLY, and the second one is
// asserted here because M7 W2 D1 (2026-09-18) made it impossible to leave to
// inference.
//
// Until that event this comment read "the default, classic_adaptive,
// constructs ClassicAdaptiveGovernor -- covered implicitly by every other test
// in this file, which never touches barrier_governor_mode and still
// solves/validates against the classic governor". The event moved the default
// to monitored, so every test that leaves the governor alone now exercises the
// MONITORED path and the implicit coverage that sentence claimed is gone. The
// free-only governor keeps a direct dispatch pin instead: a test that writes it.
//
// Test access: same pattern as the funnel/filter construction tests above --
// calls the private rebuild_globalization_components() and reads the private
// governor_ member, so it is declared as a friend in ipm_solver.h and lives at
// global scope (see the comment above for why).
TEST(RecoveryDispatchGate, MonitoredSelectionConstructsMonitoredGovernor) {
    hven::IpmSolverImpl solver;
    {
        auto o = solver.options();
        o.advanced.barrier_governor_mode = hven::BarrierGovernorMode::kMonitored;
        solver.set_options(std::move(o));
    }
    solver.rebuild_globalization_components();
    hven::BarrierGovernor *governor = solver.governor_.get();
    ASSERT_NE(dynamic_cast<hven::MonitoredBarrierGovernor *>(governor), nullptr);
    EXPECT_FALSE(governor->in_monotone_mode());
}

// The other half of that dispatch, direct since M7 W2 D1 (see the comment
// above). classic_adaptive is no longer the default, so nothing else in this
// file constructs it any more.
//
// The DEFAULT arm is asserted alongside it, in the same test, because the two
// claims are one fact: the selector decides, and the default is one value of
// the selector rather than a separate mechanism.
TEST(RecoveryDispatchGate, ClassicAdaptiveSelectionConstructsClassicAdaptiveGovernor) {
    {
        hven::IpmSolverImpl solver;
        auto o = solver.options();
        o.advanced.barrier_governor_mode = hven::BarrierGovernorMode::kClassicAdaptive;
        // classic_adaptive under the DEFAULT acceptance strategy (filter) is
        // guard 1's refusal, so this arm also writes the pre-event acceptance
        // strategy -- which is what the pair classic_adaptive + classic_merit
        // was before the event, and the only combination in which the free-only
        // governor needs no opt-in.
        o.advanced.acceptance_mode = hven::AcceptanceMode::kClassicMerit;
        solver.set_options(std::move(o));
        solver.rebuild_globalization_components();
        hven::BarrierGovernor *governor = solver.governor_.get();
        ASSERT_NE(dynamic_cast<hven::ClassicAdaptiveGovernor *>(governor), nullptr);
        EXPECT_EQ(dynamic_cast<hven::MonitoredBarrierGovernor *>(governor), nullptr);
    }
    {
        // A solver left entirely alone constructs the governor the shipped
        // default names, which since W2 D1 is the monitored one.
        hven::IpmSolverImpl solver;
        ASSERT_EQ(solver.options().advanced.barrier_governor_mode,
                  hven::BarrierGovernorMode::kMonitored);
        solver.rebuild_globalization_components();
        ASSERT_NE(dynamic_cast<hven::MonitoredBarrierGovernor *>(solver.governor_.get()), nullptr);
    }
}

// Settings::merit_penalty_rule_ must reach the constructed ModernMeritAcceptance:
// the rule is a constructor argument, so a selection bug here is invisible to
// test_merit_rules.cpp (which constructs ModernMeritAcceptance directly) and to
// the type-only checks above (both rules produce the same concrete type).
//
// The rule is discriminated behaviourally, by driving one acceptance test whose
// hand-computed penalty trajectory differs between the two rules (both cases are
// truth-tabled from the papers in test_merit_rules.cpp):
//   - flexible, Curtis & Nocedal Region II — cur(θ=2,f=10), tri(θ=1,f=12),
//     pred(m_θ=2,m_f=3): rejected at π_l, accepted at π_u, so π_l is raised to
//     ≈ 0.2. The WMNO rule never touches π_l.
//   - wmno, Waltz-Morales-Nocedal-Orban Eq. (3.6) — cur(θ=1,f=10),
//     tri(θ=0.5,f=12), pred(m_θ=1,m_f=−9): τ = 10 > ν₀ = 1, so ν is bumped to
//     11. The flexible rule never touches ν.
//
// Test access: same friend pattern as the selection tests above (private
// rebuild_globalization_components() + private acceptance_), hence global scope.
TEST(RecoveryDispatchGate, MeritPenaltyRuleSelectionReachesTheStrategy) {
    using hven::MeritPenaltyRule;
    using hven::ModernMeritAcceptance;
    using hven::ProgressMeasures;
    const auto pm = [](double infeasibility, double objective) {
        ProgressMeasures p;
        p.infeasibility = infeasibility;
        p.objective = objective;
        return p;
    };

    {
        hven::IpmSolverImpl solver;
        {
            auto o = solver.options();
            o.advanced.acceptance_mode = hven::AcceptanceMode::kMerit;
            o.advanced.merit_penalty_rule = MeritPenaltyRule::kFlexible;
            solver.set_options(std::move(o));
        }
        solver.rebuild_globalization_components();
        auto *merit = dynamic_cast<ModernMeritAcceptance *>(solver.acceptance_.get());
        ASSERT_NE(merit, nullptr);
        EXPECT_TRUE(
            merit->is_iterate_acceptable(pm(2.0, 10.0), pm(1.0, 12.0), pm(2.0, 3.0), 1.0, 1.0));
        EXPECT_NEAR(merit->flex_pi_l(), 0.2, 1e-6); // the flexible rule ran
    }

    {
        hven::IpmSolverImpl solver;
        {
            auto o = solver.options();
            o.advanced.acceptance_mode = hven::AcceptanceMode::kMerit;
            solver.set_options(std::move(o));
        }
        // wmno is the default; set it explicitly so the contrast is stated.
        {
            auto o = solver.options();
            o.advanced.merit_penalty_rule = MeritPenaltyRule::kSinglePenalty;
            solver.set_options(std::move(o));
        }
        solver.rebuild_globalization_components();
        auto *merit = dynamic_cast<ModernMeritAcceptance *>(solver.acceptance_.get());
        ASSERT_NE(merit, nullptr);
        EXPECT_TRUE(
            merit->is_iterate_acceptable(pm(1.0, 10.0), pm(0.5, 12.0), pm(1.0, -9.0), 1.0, 1.0));
        EXPECT_DOUBLE_EQ(merit->wmno_penalty(), 11.0);            // the WMNO rule ran
        EXPECT_DOUBLE_EQ(merit->flex_pi_l(), hven::kFlexInitPiL); // flexible did not
    }
}
