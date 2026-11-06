// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-2 B3: the shared options' limits and their validation, on both engines.
//
//  * The work ceiling (CommonOptions::max_factorizations) ends a solve with
//    kWorkLimit at an iteration boundary; the time limit (time_limit_seconds)
//    with kTimeLimit. Both empty by default, and an empty or never-binding
//    limit is the unlimited solve, bit for bit.
//  * The SQP minor budget (SolveBudget::max_minor_iterations) reports kWorkLimit.
//  * Each refused value is refused by name: CommonOptions' new fields at
//    validate(), a SolveBudget member below 1 at solve entry, the reserved
//    dense-arm values on both engines.
//  * Silent by default: a default-built solver of either engine writes nothing
//    to stdout during a solve.

#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/model/problem.h"
#include "support/hs071_problem.h"

namespace {

using hven::CommonOptions;
using hven::Index;
using hven::IpmOptions;
using hven::IpmResult;
using hven::IpmSolver;
using hven::Problem;
using hven::SolveBudget;
using hven::SolveStatus;
using hven::SqpOptions;
using hven::SqpResult;
using hven::SqpSolver;
using hven::Verbosity;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

Eigen::VectorXd hs071_start() {
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

std::shared_ptr<hven_drivers_tests::Hs071Problem> hs071() {
    return std::make_shared<hven_drivers_tests::Hs071Problem>();
}

IpmResult ipm_solve(const IpmOptions &o, SolveBudget budget = {}) {
    Problem p(hs071());
    IpmSolver s(o);
    return s.solve(p, hs071_start(), {}, budget);
}

SqpResult sqp_solve(const SqpOptions &o, SolveBudget budget = {}) {
    Problem p(hs071());
    SqpSolver s(o);
    return s.solve(p, hs071_start(), {}, budget);
}

// validate() refuses the value and names the field.
template <typename Options> void expect_refused(const Options &o, const std::string &field) {
    try {
        hven::validate(o);
        ADD_FAILURE() << "expected " << field << " to be refused";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find(field), std::string::npos) << e.what();
    }
}

// The text validate() throws for @p o, or "" when it accepts.
template <typename Options> std::string refusal_text(const Options &o) {
    try {
        hven::validate(o);
    } catch (const std::invalid_argument &e) {
        return e.what();
    }
    return "";
}

template <typename Options> void refuses_the_common_values() {
    {
        Options o;
        o.common.infinite_bound = 0.0;
        expect_refused(o, "common.infinite_bound");
        o.common.infinite_bound = -1.0;
        expect_refused(o, "common.infinite_bound");
        o.common.infinite_bound = kNaN;
        expect_refused(o, "common.infinite_bound");
        o.common.infinite_bound = kInf; // "every finite value is a real bound"
        EXPECT_NO_THROW(hven::validate(o));
    }
    for (const double bad : {0.0, -1.0, kNaN, kInf}) {
        Options o;
        o.common.time_limit_seconds = bad;
        expect_refused(o, "common.time_limit_seconds");
    }
    {
        Options o;
        o.common.max_factorizations = -1;
        expect_refused(o, "common.max_factorizations");
        o.common.max_factorizations = 0; // legal: stop at the first boundary
        EXPECT_NO_THROW(hven::validate(o));
    }
    for (const double bad : {0.0, -1e-6, kNaN, kInf}) {
        Options o;
        o.common.optimality_tol = bad;
        expect_refused(o, "common.optimality_tol");
        // One field per message: the other tolerance, which is fine, is not named.
        EXPECT_EQ(refusal_text(o).find("common.feasibility_tol"), std::string::npos);
        Options f;
        f.common.feasibility_tol = bad;
        expect_refused(f, "common.feasibility_tol");
        EXPECT_EQ(refusal_text(f).find("common.optimality_tol"), std::string::npos);
    }
    {
        // Above INT_MAX the iteration cap would narrow to int; refused by name.
        Options o;
        o.common.max_iterations = Index{std::numeric_limits<int>::max()} + 1;
        expect_refused(o, "common.max_iterations");
        o.common.max_iterations = std::numeric_limits<int>::max();
        EXPECT_NO_THROW(hven::validate(o));
    }
    {
        Options o;
        o.common.verbosity = static_cast<Verbosity>(4);
        expect_refused(o, "common.verbosity");
    }
    {
        Options o;
        o.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
        expect_refused(o, "linear.dense_factorization");
        o.linear.dense_factorization = hven::linear::DensePolicy::kAuto;
        expect_refused(o, "linear.dense_factorization");
        Options d;
        d.linear.max_dense_dimension = 50;
        expect_refused(d, "linear.max_dense_dimension");
    }
}

} // namespace

TEST(Limits, IpmValidateRefusesTheNewCommonValues) { refuses_the_common_values<IpmOptions>(); }

TEST(Limits, SqpValidateRefusesTheNewCommonValues) { refuses_the_common_values<SqpOptions>(); }

// One validator serves both engines, so the same bad value reads the same message on both
// (the engines differ only in the floors: iterations and threads >= 1 on the IPM, >= 0 on
// the SQP).
TEST(Limits, BothEnginesRefuseACommonValueWithTheSameText) {
    const auto same = [](auto edit) {
        IpmOptions i;
        SqpOptions q;
        edit(i.common);
        edit(q.common);
        const std::string ti = refusal_text(i);
        EXPECT_FALSE(ti.empty());
        EXPECT_EQ(ti, refusal_text(q));
    };
    same([](CommonOptions &c) { c.optimality_tol = kNaN; });
    same([](CommonOptions &c) { c.feasibility_tol = -1.0; });
    same([](CommonOptions &c) { c.max_factorizations = -1; });
    same([](CommonOptions &c) { c.time_limit_seconds = kNaN; });
    same([](CommonOptions &c) { c.infinite_bound = kNaN; });

    // The floors differ (and the text names the floor).
    IpmOptions i;
    SqpOptions q;
    i.common.max_iterations = q.common.max_iterations = 0;
    EXPECT_NE(refusal_text(i).find("common.max_iterations"), std::string::npos);
    EXPECT_EQ(refusal_text(q), "");
    i.common.max_iterations = q.common.max_iterations = 1;
    i.common.threads = q.common.threads = 0;
    EXPECT_NE(refusal_text(i).find("common.threads"), std::string::npos);
    EXPECT_EQ(refusal_text(q), "");
}

TEST(Limits, AnEngagedQpMaxIterBelowOneIsRefused) {
    SqpOptions o;
    EXPECT_FALSE(o.qp.max_iterations.has_value());
    EXPECT_NO_THROW(hven::validate(o));
    o.qp.max_iterations = 0;
    expect_refused(o, "qp.max_iterations");
    o.qp.max_iterations = -5;
    expect_refused(o, "qp.max_iterations");
    o.qp.max_iterations = 1;
    EXPECT_NO_THROW(hven::validate(o));
}

TEST(Limits, RelaxBoundsNeedsAPositiveRelaxFactor) {
    IpmOptions o;
    o.fixed_variable_treatment = hven::IpmFixedVariableTreatment::kRelaxBounds;
    o.bound_relax_factor = 0.0;
    expect_refused(o, "bound_relax_factor");
    o.bound_relax_factor = kNaN;
    expect_refused(o, "bound_relax_factor");
    o.bound_relax_factor = 1e-8;
    EXPECT_NO_THROW(hven::validate(o));
    o.fixed_variable_treatment = hven::IpmFixedVariableTreatment::kMakeParameter;
    o.bound_relax_factor = 0.0; // legal for the other treatments
    EXPECT_NO_THROW(hven::validate(o));
}

TEST(Limits, IpmValidateRefusesTheTerminationAndLinearValues) {
    for (const double bad : {0.0, -1.0, kNaN, kInf}) {
        IpmOptions o;
        o.termination.complementarity_tol = bad;
        expect_refused(o, "termination.complementarity_tol");
    }
    {
        IpmOptions o;
        o.termination.complementarity_tol = 1e-1; // above acceptable_barrier_tol
        expect_refused(o, "termination.acceptable_barrier_tol");
    }
    {
        IpmOptions o;
        o.common.max_iterations = 0;
        expect_refused(o, "common.max_iterations");
    }
    {
        IpmOptions o;
        o.linear.solve_parallelism = static_cast<hven::IpmSolveParallelism>(2);
        expect_refused(o, "linear.solve_parallelism");
    }
    {
        IpmOptions o;
        o.advanced.acceptance_mode = static_cast<hven::IpmAcceptanceMode>(9);
        expect_refused(o, "advanced.acceptance_mode");
    }
}

TEST(Limits, ABudgetMemberBelowOneIsRefusedAtSolveEntryOnBothEngines) {
    const SolveBudget no_iterations{.max_iterations = 0};
    const SolveBudget no_minors{.max_minor_iterations = 0};
    for (const SolveBudget &b : {no_iterations, no_minors}) {
        EXPECT_THROW(ipm_solve(IpmOptions{}, b), std::invalid_argument);
        EXPECT_THROW(sqp_solve(SqpOptions{}, b), std::invalid_argument);
    }
    EXPECT_THROW(hven::validate(SolveBudget{.max_iterations = -3}), std::invalid_argument);
    // Above INT_MAX the budget would narrow to int: refused, by name, on both engines.
    const SolveBudget too_many{.max_iterations = Index{std::numeric_limits<int>::max()} + 1};
    EXPECT_THROW(hven::validate(too_many), std::invalid_argument);
    EXPECT_THROW(ipm_solve(IpmOptions{}, too_many), std::invalid_argument);
    EXPECT_THROW(sqp_solve(SqpOptions{}, too_many), std::invalid_argument);
    EXPECT_NO_THROW(hven::validate(SolveBudget{.max_iterations = std::numeric_limits<int>::max()}));
    EXPECT_NO_THROW(hven::validate(SolveBudget{}));
    EXPECT_NO_THROW(hven::validate(SolveBudget{.max_iterations = 1, .max_minor_iterations = 1}));
}

// ---------------------------------------------------------------------------
// The work ceiling
// ---------------------------------------------------------------------------

TEST(Limits, IpmWorkLimitStopsAtTheBoundaryWithTheCurrentPoint) {
    const IpmResult free_run = ipm_solve(IpmOptions{});
    ASSERT_EQ(free_run.status, SolveStatus::kOptimal);
    const hven::Index free_facts = free_run.kkt_factor_counters.factorize_count;
    ASSERT_GT(free_facts, 4);

    IpmOptions o;
    o.common.max_factorizations = 3;
    const IpmResult capped = ipm_solve(o);
    EXPECT_EQ(capped.status, SolveStatus::kWorkLimit);
    ASSERT_EQ(capped.phases.size(), 1u);
    EXPECT_EQ(capped.phases[0].stop_reason, hven::IpmStopReason::kWorkLimit);
    EXPECT_GE(capped.kkt_factor_counters.factorize_count, 3);
    EXPECT_LT(capped.kkt_factor_counters.factorize_count, free_facts);
    EXPECT_LT(capped.iterations, free_run.iterations);
    EXPECT_TRUE(capped.x.allFinite());

    // 0 stops at the first boundary.
    o.common.max_factorizations = 0;
    const IpmResult none = ipm_solve(o);
    EXPECT_EQ(none.status, SolveStatus::kWorkLimit);
    EXPECT_LE(none.iterations, 1);
}

TEST(Limits, AnIpmWorkLimitEndsThePhaseSequence) {
    IpmOptions o;
    o.phases = {hven::IpmPhase::kSolve, hven::IpmPhase::kOptimize};
    o.common.max_factorizations = 0;
    const IpmResult r = ipm_solve(o);
    EXPECT_EQ(r.status, SolveStatus::kWorkLimit);
    ASSERT_EQ(r.phases.size(), 2u);
    EXPECT_TRUE(r.phases[0].ran);
    EXPECT_FALSE(r.phases[1].ran);
}

TEST(Limits, ANeverBindingIpmLimitIsTheUnlimitedSolveBitForBit) {
    const IpmResult free_run = ipm_solve(IpmOptions{});
    IpmOptions o;
    o.common.max_factorizations = 1'000'000;
    o.common.time_limit_seconds = 1.0e6;
    const IpmResult limited = ipm_solve(o);
    EXPECT_EQ(limited.status, free_run.status);
    EXPECT_EQ(limited.iterations, free_run.iterations);
    EXPECT_EQ(limited.kkt_factor_counters.factorize_count,
              free_run.kkt_factor_counters.factorize_count);
    for (Eigen::Index i = 0; i < free_run.x.size(); ++i) {
        EXPECT_EQ(limited.x[i], free_run.x[i]) << i;
    }
}

TEST(Limits, SqpWorkLimitStopsAtTheBoundaryWithTheCurrentPoint) {
    const SqpResult free_run = sqp_solve(SqpOptions{});
    ASSERT_EQ(free_run.status, SolveStatus::kOptimal);
    ASSERT_GT(free_run.counters.factorizations, 2);

    SqpOptions o;
    o.common.max_factorizations = 1;
    const SqpResult capped = sqp_solve(o);
    EXPECT_EQ(capped.status, SolveStatus::kWorkLimit);
    EXPECT_GE(capped.counters.factorizations, 1);
    EXPECT_LT(capped.counters.major_iters, free_run.counters.major_iters);
    EXPECT_EQ(capped.counters.probe_budget_stops, 0);
    EXPECT_TRUE(capped.x.allFinite());

    o.common.max_factorizations = 0;
    const SqpResult none = sqp_solve(o);
    EXPECT_EQ(none.status, SolveStatus::kWorkLimit);
    EXPECT_EQ(none.counters.factorizations, 0);
    EXPECT_EQ(none.counters.major_iters, 0);
}

TEST(Limits, ANeverBindingSqpLimitIsTheUnlimitedSolveBitForBit) {
    const SqpResult free_run = sqp_solve(SqpOptions{});
    SqpOptions o;
    o.common.max_factorizations = 1'000'000;
    o.common.time_limit_seconds = 1.0e6;
    const SqpResult limited = sqp_solve(o);
    EXPECT_EQ(limited.status, free_run.status);
    EXPECT_EQ(limited.counters.major_iters, free_run.counters.major_iters);
    EXPECT_EQ(limited.counters.factorizations, free_run.counters.factorizations);
    EXPECT_EQ(limited.counters.qp_minor_iters, free_run.counters.qp_minor_iters);
    for (Eigen::Index i = 0; i < free_run.x.size(); ++i) {
        EXPECT_EQ(limited.x[i], free_run.x[i]) << i;
    }
}

// The SQP minor budget is a work ceiling and reports kWorkLimit (owner decision
// 5's relabel; it reported kIterationLimit before M8 AP-2 B3). The point is unchanged.
TEST(Limits, TheSqpMinorBudgetReportsWorkLimit) {
    const SqpResult r = sqp_solve(SqpOptions{}, SolveBudget{.max_minor_iterations = 1});
    EXPECT_EQ(r.status, SolveStatus::kWorkLimit);
    EXPECT_EQ(r.counters.probe_budget_stops, 1);
}

// ---------------------------------------------------------------------------
// The time limit. Wall-clock: the only stable assertion is the label, at a
// limit no solve can meet. Never used in a measured run.
// ---------------------------------------------------------------------------

TEST(Limits, AnUnmeetableTimeLimitReportsTimeLimitOnBothEngines) {
    IpmOptions io;
    io.common.time_limit_seconds = std::numeric_limits<double>::min();
    EXPECT_EQ(ipm_solve(io).status, SolveStatus::kTimeLimit);

    SqpOptions so;
    so.common.time_limit_seconds = std::numeric_limits<double>::min();
    const SqpResult r = sqp_solve(so);
    EXPECT_EQ(r.status, SolveStatus::kTimeLimit);
    EXPECT_EQ(r.counters.major_iters, 0);
}

// ---------------------------------------------------------------------------
// Silent by default (owner decision 4)
// ---------------------------------------------------------------------------

TEST(Verbosity, DefaultSolversOfBothEnginesWriteNothingToStdout) {
    std::fflush(stdout);
    testing::internal::CaptureStdout();
    {
        Problem p(hs071());
        IpmSolver ipm;
        SqpSolver sqp;
        (void)ipm.solve(p, hs071_start());
        (void)sqp.solve(p, hs071_start());
    }
    std::fflush(stdout);
    const std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out, "");
}

TEST(Verbosity, OneLineTurnsTheConsoleOn) {
    std::fflush(stdout);
    testing::internal::CaptureStdout();
    {
        Problem p(hs071());
        IpmOptions o;
        o.common.verbosity = Verbosity::kIterations;
        IpmSolver ipm(o);
        (void)ipm.solve(p, hs071_start());
    }
    std::fflush(stdout);
    const std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("IpmSolver"), std::string::npos);
    EXPECT_EQ(out.find("InteriorPointSolver"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Where a limit binds, and what it does to the rest of the solve
// ---------------------------------------------------------------------------

// A limit that binds in the SECOND phase: the first phase completes, the second
// is stopped, the sequence ends there and the third phase never runs. The limit
// counts the whole solve, so it is found by scanning for the smallest ceiling
// that leaves the first phase intact.
TEST(Limits, AnIpmLimitBindingInASecondPhaseEndsTheSequenceThere) {
    IpmOptions o;
    o.phases = {hven::IpmPhase::kSolve, hven::IpmPhase::kOptimize, hven::IpmPhase::kOptimize};
    const IpmResult free_run = ipm_solve(o);
    ASSERT_EQ(free_run.phases.size(), 3u);
    ASSERT_TRUE(free_run.phases[0].ran);
    ASSERT_TRUE(free_run.phases[1].ran);
    const hven::Index free_facts = free_run.kkt_factor_counters.factorize_count;

    bool found = false;
    for (hven::Index limit = 1; limit < free_facts && !found; ++limit) {
        o.common.max_factorizations = limit;
        const IpmResult r = ipm_solve(o);
        if (r.status != SolveStatus::kWorkLimit || r.phases.size() != 3u ||
            r.phases[0].stop_reason == hven::IpmStopReason::kWorkLimit) {
            continue; // the limit bound in the first phase (or not at all)
        }
        found = true;
        EXPECT_TRUE(r.phases[0].ran);
        EXPECT_NE(r.phases[0].status, SolveStatus::kWorkLimit); // phase 0 completed on its own
        EXPECT_TRUE(r.phases[1].ran);
        EXPECT_EQ(r.phases[1].status, SolveStatus::kWorkLimit);
        EXPECT_EQ(r.phases[1].stop_reason, hven::IpmStopReason::kWorkLimit);
        EXPECT_FALSE(r.phases[2].ran) << "a limit bounds the whole solve: the sequence ends";
        EXPECT_GE(r.kkt_factor_counters.factorize_count, limit);
        EXPECT_TRUE(r.x.allFinite());
    }
    EXPECT_TRUE(found) << "no ceiling below " << free_facts << " bound in the second phase";
}

// A limit exit returns the iterate the solve stopped on, never return_best's
// substitute. Premise, asserted: scored by objective, a cap exit with return_best
// does substitute at some iteration count on this problem, so the comparison below
// can tell.
TEST(Limits, ALimitExitIgnoresReturnBest) {
    constexpr auto kCriterion = hven::IpmBestIterateCriterion::kObjective;
    bool substitution_seen = false;
    for (hven::Index cap = 1; cap <= 8; ++cap) {
        IpmOptions plain;
        plain.common.max_iterations = cap;
        plain.best_iterate_criterion = kCriterion;
        IpmOptions best = plain;
        best.return_best = true;
        const IpmResult a = ipm_solve(plain);
        const IpmResult b = ipm_solve(best);
        if (a.status == SolveStatus::kIterationLimit &&
            (a.x - b.x).lpNorm<Eigen::Infinity>() > 0.0) {
            substitution_seen = true;
        }
    }
    EXPECT_TRUE(substitution_seen) << "return_best never moved a cap exit: the test is vacuous";

    for (const hven::Index limit : {0, 1, 2, 3, 4, 5}) {
        IpmOptions plain;
        plain.common.max_factorizations = limit;
        plain.best_iterate_criterion = kCriterion;
        IpmOptions best = plain;
        best.return_best = true;
        const IpmResult a = ipm_solve(plain);
        const IpmResult b = ipm_solve(best);
        if (a.status != SolveStatus::kWorkLimit) {
            continue; // converged before the ceiling
        }
        ASSERT_EQ(b.status, SolveStatus::kWorkLimit) << limit;
        ASSERT_EQ(a.x.size(), b.x.size());
        for (Eigen::Index i = 0; i < a.x.size(); ++i) {
            EXPECT_EQ(a.x[i], b.x[i]) << "limit " << limit << " coordinate " << i;
        }
        EXPECT_EQ(a.f, b.f) << limit;
    }
}

// set_options() between two solves re-arms a limit: each solve measures its own
// factorizations and clock, so a replaced ceiling binds again, and replacing it
// with a roomy one (or none) lets the next solve finish.
TEST(Limits, SetOptionsBetweenSolvesRearmsTheIpmLimit) {
    Problem p(hs071());
    IpmOptions tight;
    tight.common.max_factorizations = 3;
    IpmSolver s(tight);
    const IpmResult first = s.solve(p, hs071_start());
    ASSERT_EQ(first.status, SolveStatus::kWorkLimit);

    IpmOptions roomy;
    roomy.common.max_factorizations = 1'000'000;
    s.set_options(roomy);
    EXPECT_EQ(s.solve(p, hs071_start()).status, SolveStatus::kOptimal);

    s.set_options(tight);
    const IpmResult again = s.solve(p, hs071_start());
    EXPECT_EQ(again.status, SolveStatus::kWorkLimit);
    EXPECT_EQ(again.iterations, first.iterations); // the same ceiling stops it at the same place

    IpmOptions looser = tight;
    looser.common.max_factorizations = 5;
    s.set_options(looser);
    const IpmResult five = s.solve(p, hs071_start());
    EXPECT_EQ(five.status, SolveStatus::kWorkLimit);
    EXPECT_GT(five.iterations, first.iterations);

    s.set_options(IpmOptions{}); // no limit
    EXPECT_EQ(s.solve(p, hs071_start()).status, SolveStatus::kOptimal);
}

TEST(Limits, SetOptionsBetweenSolvesRearmsTheSqpLimit) {
    Problem p(hs071());
    SqpOptions tight;
    tight.common.max_factorizations = 1;
    SqpSolver s(tight);
    const SqpResult first = s.solve(p, hs071_start());
    ASSERT_EQ(first.status, SolveStatus::kWorkLimit);

    SqpOptions roomy;
    roomy.common.max_factorizations = 1'000'000;
    s.set_options(roomy);
    EXPECT_EQ(s.solve(p, hs071_start()).status, SolveStatus::kOptimal);

    s.set_options(tight);
    const SqpResult again = s.solve(p, hs071_start());
    EXPECT_EQ(again.status, SolveStatus::kWorkLimit);
    EXPECT_EQ(again.counters.factorizations, first.counters.factorizations);
}

// A cap and a limit reached at the same boundary report the limit, as on the
// SQP engine. The ceiling is the factorizations the capped solve spent, so the
// two meet exactly at the cap's last iteration; one factorization more and the
// cap is the only reason to stop.
TEST(Limits, AnIpmCapAndLimitTieReportsTheLimit) {
    IpmOptions capped;
    capped.common.max_iterations = 3;
    const IpmResult cap_only = ipm_solve(capped);
    ASSERT_EQ(cap_only.status, SolveStatus::kIterationLimit);
    ASSERT_EQ(cap_only.phases[0].stop_reason, hven::IpmStopReason::kIterationLimit);
    const hven::Index spent = cap_only.kkt_factor_counters.factorize_count;

    IpmOptions tie = capped;
    tie.common.max_factorizations = spent;
    const IpmResult tied = ipm_solve(tie);
    EXPECT_EQ(tied.status, SolveStatus::kWorkLimit);
    EXPECT_EQ(tied.phases[0].stop_reason, hven::IpmStopReason::kWorkLimit);
    EXPECT_EQ(tied.kkt_factor_counters.factorize_count, spent);
    EXPECT_EQ(tied.iterations, cap_only.iterations);
    for (Eigen::Index i = 0; i < cap_only.x.size(); ++i) {
        EXPECT_EQ(tied.x[i], cap_only.x[i]) << i; // the same point, a different label
    }

    IpmOptions loose = capped;
    loose.common.max_factorizations = spent + 1;
    const IpmResult not_tied = ipm_solve(loose);
    EXPECT_EQ(not_tied.status, SolveStatus::kIterationLimit);
    EXPECT_EQ(not_tied.phases[0].stop_reason, hven::IpmStopReason::kIterationLimit);

    // A tie in the first phase of a sequence also ends the sequence.
    IpmOptions two_phase = tie;
    two_phase.phases = {hven::IpmPhase::kOptimize, hven::IpmPhase::kSolve};
    const IpmResult seq = ipm_solve(two_phase);
    EXPECT_EQ(seq.status, SolveStatus::kWorkLimit);
    ASSERT_EQ(seq.phases.size(), 2u);
    EXPECT_FALSE(seq.phases[1].ran);
}
