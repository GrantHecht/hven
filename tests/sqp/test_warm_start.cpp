// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_warm_start.cpp — Phase-4 Task 2: the WarmStart value object
// (warm_start.h) SqpDriver::solve() emits on SqpSolution::warm_start.
//
// THREE CHECKS, per the task brief:
//   (a) a normal, converged solve (HS7) populates warm_start fully: valid,
//       x/multiplier sizes and values, the activity vectors consistent with
//       HS7's own bound-free, inequality-free shape, and the
//       globalization/regularization fields actually set (not left at their
//       "unset" sentinels).
//   (b) a solve that stops on kMaxIter (never converges) still emits a valid
//       warm object whose x is the point the last history row describes --
//       i.e. the driver's best-known iterate, not a stale or cleared one.
//   (c) structure_hash is a pure function of the model's H/Ae/Ai SPARSITY:
//       equal across two independent solves of the SAME model, different
//       across two structurally different models (HS7 vs HS10).
//
// This file does not exercise warm-START SEEDING (feeding a WarmStart back
// into a solve) -- that is Task 3 and beyond; this task is the value object
// and its population alone.

#include <bit>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/globalization/sqp/globalization.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>

#include "support/hs_problems.h"
#include "support/parametric_families.h"
#include "support/scale_problems.h"

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::test_support::F1BoxQp;
using hven::solvers::test_support::F7CollocationChain;
using hven::solvers::test_support::make_hs;

namespace {

// (a) HS7, DEFAULT options: converges in a handful of majors (see
// test_sqp_driver.cpp's SqpDriverEquality battery), so by the time it exits a
// subproblem HAS been built and the funnel HAS been reset -- both
// globalization fields are therefore expected to be real values, not their
// "unset" sentinels.
TEST(WarmStart, SolvedHs7PopulatesEveryField) {
    const auto p = make_hs(7);
    SqpOptions opts;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    const WarmStart &w = sol.warm_start;

    EXPECT_TRUE(w.valid);

    // Primal point matches the reported solution exactly (same field, same
    // solve -- this is a same-object sanity check, not a numerical one).
    ASSERT_EQ(w.x.size(), sol.x.size());
    EXPECT_EQ(w.x, sol.x);

    // Multiplier sizes match the model's own (me, mi, n).
    EXPECT_EQ(w.lambda_e.size(), p.model->me());
    EXPECT_EQ(w.lambda_i.size(), p.model->mi());
    EXPECT_EQ(w.z.size(), p.model->n());

    // HS7 has NO inequality constraints and NO finite bounds (see
    // hs_problems.h's Hs7Model): the activity vectors must be sized
    // accordingly and every bound entry must read "free" (0) -- there is
    // nothing for the reported active set to disagree with.
    EXPECT_EQ(w.ineq_active.size(), static_cast<std::size_t>(p.model->mi()));
    ASSERT_EQ(w.bound_active.size(), static_cast<std::size_t>(p.model->n()));
    for (std::int8_t b : w.bound_active) {
        EXPECT_EQ(b, 0) << "HS7 has no finite bounds; nothing can be active";
    }

    // The QP engine's own working set at exit is consistent WITH the same
    // report: same dimensions, no rows in the working set (mi() == 0), and
    // every variable free.
    EXPECT_EQ(w.qp_working_set.n(), p.model->n());
    EXPECT_EQ(w.qp_working_set.mi(), p.model->mi());
    EXPECT_TRUE(w.qp_working_set.active_ineq().empty());
    for (BoundState b : w.qp_working_set.bound_state()) {
        EXPECT_EQ(b, BoundState::kFree);
    }

    // Globalization/regularization fields: HS7 takes several majors to
    // converge (test_sqp_driver.cpp measures 7 from the region of
    // attraction), so a subproblem was built, the funnel was reset, and
    // these must all have moved off their "unset" (-1 / 0) sentinels.
    EXPECT_GT(w.funnel_width, 0.0);
    EXPECT_GT(w.tr_radius, 0.0);
    EXPECT_EQ(w.primal_delta, opts.qp.primal_delta);
    EXPECT_GT(w.dual_mu, 0.0);
    EXPECT_NE(w.structure_hash, 0u);
}

// (b) A solve that NEVER converges (kMaxIter) still emits a valid warm
// object, and its x is the driver's own best-known (i.e. last-reported)
// iterate -- checked against the history the same solve produced, which is
// exactly what a caller re-deriving "did this warm object describe the point
// the solve actually stopped at" would do. Fixture ported from
// test_sqp_driver.cpp's SqpDriverTrustRegion.StrategyIsPluggable: HS5 with an
// accept-everything strategy at tr_init = 2 reproduces Task 4's permanent
// 2-cycle and burns the whole max_iter budget.
TEST(WarmStart, MaxIterSolveEmitsValidWarmObjectAtBestKnownIterate) {
    class AcceptEverything final : public GlobalizationStrategy {
      public:
        void reset(double) override {}
        StepVerdict judge(const StepContext &) override { return StepVerdict::kAcceptF; }
    };

    const auto p = make_hs(5);
    SqpOptions opts;
    opts.tr_init = 2.0;
    opts.max_iter = 20;
    opts.make_strategy = []() -> std::unique_ptr<GlobalizationStrategy> {
        return std::make_unique<AcceptEverything>();
    };
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);

    ASSERT_EQ(sol.status, SqpStatus::kMaxIter);
    ASSERT_FALSE(sol.history.empty());

    const WarmStart &w = sol.warm_start;
    EXPECT_TRUE(w.valid);
    ASSERT_EQ(w.x.size(), sol.x.size());
    EXPECT_EQ(w.x, sol.x);

    // sol.x IS the best-known iterate on a kMaxIter exit stopped AT an
    // iterate (sqp_types.h's SqpCounters note): the LAST history row
    // describes that exact point, so its recorded f must match the model's
    // f at warm_start.x bit-for-bit (both are the SAME ev.f, never
    // recomputed).
    EXPECT_DOUBLE_EQ(p.model->eval_f(w.x), sol.history.back().f);

    // Sizes are still correct even though the solve never certified
    // anything.
    EXPECT_EQ(w.lambda_e.size(), p.model->me());
    EXPECT_EQ(w.lambda_i.size(), p.model->mi());
    EXPECT_EQ(w.z.size(), p.model->n());
    EXPECT_EQ(w.ineq_active.size(), static_cast<std::size_t>(p.model->mi()));
    EXPECT_EQ(w.bound_active.size(), static_cast<std::size_t>(p.model->n()));
}

// (c) structure_hash is a function of the MODEL's H/Ae/Ai sparsity alone:
// two independent solves of the same model agree, and HS7 (me=1, mi=0)
// disagrees with HS10 (me=0, mi=1) -- different constraint-block shapes AND
// different Hessian sparsity (HS7's is diagonal-only; HS10's has an
// off-diagonal entry), so a collision would need both hashes to happen to
// agree by accident, not by construction.
TEST(WarmStart, StructureHashMatchesSameModelDiffersAcrossModels) {
    SqpOptions opts;

    const auto hs7a = make_hs(7);
    const auto hs7b = make_hs(7);
    const auto hs10 = make_hs(10);

    SqpDriver driver_a(opts);
    SqpDriver driver_b(opts);
    SqpDriver driver_c(opts);

    const SqpSolution sol_a = driver_a.solve(*hs7a.model);
    const SqpSolution sol_b = driver_b.solve(*hs7b.model);
    const SqpSolution sol_c = driver_c.solve(*hs10.model);

    ASSERT_EQ(sol_a.status, SqpStatus::kOptimal);
    ASSERT_EQ(sol_b.status, SqpStatus::kOptimal);
    ASSERT_EQ(sol_c.status, SqpStatus::kOptimal);

    ASSERT_NE(sol_a.warm_start.structure_hash, 0u);
    ASSERT_NE(sol_c.warm_start.structure_hash, 0u);
    EXPECT_EQ(sol_a.warm_start.structure_hash, sol_b.warm_start.structure_hash)
        << "two solves of the SAME model must agree";
    EXPECT_NE(sol_a.warm_start.structure_hash, sol_c.warm_start.structure_hash)
        << "HS7 and HS10 have different constraint-block shapes and Hessian sparsity";
}

// (c2) PHASE-5 TASK 0 -- O-1 REPAIR A, make_warm_start's ZERO-MAJOR PATH.
// A solve that converges before it ever builds a subproblem (`qp_built ==
// false`, `major_iters == 0`) used to emit structure_hash == 0, which the
// ingest rule reads as "no model was seen" and treats exactly like a mismatch:
// the hand-off was `valid` and unusable at the same time, and the whole warm
// chain broke on it (docs/notes/2026-07-30-warm-start-battery-results.md,
// O-1). make_warm_start now PROBES the model's structure at that exit
// instead. What this pins is the property the ingest side actually needs --
// not merely "non-zero", but EQUAL to the hash an ordinary subproblem-building
// exit of the same model produces, since the ingest compares against its own
// probe of that same pattern. The third solve is the end-to-end consequence:
// the chain survives a zero-major link.
TEST(WarmStart, ZeroMajorSolveEmitsTheSameStructureHashAsABuiltSubproblem) {
    SqpOptions opts;
    const auto p = make_hs(7);

    SqpDriver built_driver(opts);
    const SqpSolution built = built_driver.solve(*p.model);
    ASSERT_EQ(built.status, SqpStatus::kOptimal);
    ASSERT_GT(built.counters.major_iters, 0) << "this arm is the subproblem-BUILDING comparator";
    ASSERT_NE(built.warm_start.structure_hash, 0u);

    // Re-solving AT the answer converges on the very first convergence test,
    // so no subproblem is ever built -- the exact condition O-1 is about.
    SqpDriver zero_driver(opts);
    const SqpSolution zero = zero_driver.solve(*p.model, built.x, built.warm_start);
    ASSERT_EQ(zero.status, SqpStatus::kOptimal);
    ASSERT_EQ(zero.counters.major_iters, 0) << "already optimal: nothing is linearized";

    EXPECT_NE(zero.warm_start.structure_hash, 0u)
        << "the probe is what a zero-major exit hashes instead of writing the 0 sentinel";
    EXPECT_EQ(zero.warm_start.structure_hash, built.warm_start.structure_hash)
        << "structural_hash reads H/Ae/Ai's PATTERN only, so probing the model at the exit "
           "point must reproduce what building at any point would have hashed";

    // The consequence: the zero-major solve's own hand-off is ingestible.
    SqpDriver chained_driver(opts);
    const SqpSolution chained = chained_driver.solve(*p.model, zero.warm_start.x, zero.warm_start);
    EXPECT_EQ(chained.counters.start_level_used, StartLevel::kWarm)
        << "a zero-major link no longer breaks the chain";
    EXPECT_EQ(chained.status, SqpStatus::kOptimal);
}

// =============================================================================
// PHASE-4 TASK 3: solve(model, x0, warm) -- cold/warm level resolution and
// ingest.
// =============================================================================

// A small perturbation of HS7's objective: f(x) = log(1+x1^2) - x2 + eps*x1.
// The added term is LINEAR, so it changes neither Hs7Model's Hessian
// SPARSITY nor its equality constraint at all -- only eval_f/eval_grad are
// overridden, and eval_hess/eval_jac_e/eval_jac_i/bounds/start_point are
// inherited unchanged. Its structure_hash is therefore IDENTICAL to
// Hs7Model's own (structural_hash hashes sparsity pattern only, never
// values -- qp_engine.h), which is exactly what lets a HS7 warm object
// resolve to kWarm here.
class Hs7PerturbedModel : public hven::solvers::test_support::Hs7Model {
  public:
    explicit Hs7PerturbedModel(double eps) : eps_(eps) {}

    double eval_f(const Vec &x) const override {
        return hven::solvers::test_support::Hs7Model::eval_f(x) + eps_ * x(0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g = hven::solvers::test_support::Hs7Model::eval_grad(x);
        g(0) += eps_;
        return g;
    }

  private:
    double eps_;
};

// (a) Feeding HS7's own converged warm object back into a FRESH solve of the
// SAME problem resolves kWarm and converges in far fewer majors than the
// cold solve did. Both counts are OBSERVED-VALUE pins (this suite's house
// style, test_hs_battery.cpp's own convention): 9 cold majors from the
// published start (this driver's own default options; see this file's
// StructureHashMatchesSameModelDiffersAcrossModels for the same solve run
// under the same options), 0 warm majors -- x0 := warm.x lands exactly on
// the point the cold solve already certified, so evaluate_kkt's convergence
// test at iter 0 fires before any subproblem is built.
TEST(WarmStart, WarmMatchesColdOnIdenticalProblem) {
    SqpOptions opts;

    const auto p_cold = make_hs(7);
    SqpDriver cold_driver(opts);
    const SqpSolution cold = cold_driver.solve(*p_cold.model);
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    EXPECT_EQ(cold.counters.major_iters, 9);
    EXPECT_EQ(cold.counters.start_level_used, StartLevel::kCold);

    const auto p_warm = make_hs(7);
    SqpDriver warm_driver(opts);
    const SqpSolution warm =
        warm_driver.solve(*p_warm.model, p_warm.model->start_point(), cold.warm_start);

    ASSERT_EQ(warm.status, SqpStatus::kOptimal);
    EXPECT_EQ(warm.counters.start_level_used, StartLevel::kWarm);
    EXPECT_LE(warm.counters.major_iters, 2);
    EXPECT_EQ(warm.counters.major_iters, 0);
}

// PHASE-4 TASK 7 (cold-vs-warm ledger instrumentation): the exact same
// cold/warm HS7 pair as WarmMatchesColdOnIdenticalProblem just above, this
// time with a Ledger attached to each driver -- the shape the Phase-6
// benchmark actually drives. Two SqpSolveRecord rows land in the SAME
// Ledger (attach_ledger takes a pointer; nothing about it ties a Ledger to
// one driver instance), and this is the failing-first TDD case Step 1 of
// the task brief names: the cold record must read kCold, the warm one
// kWarm, and ledger.sqp_summary_table() must render the Level column.
TEST(WarmStart, LedgerRecordsColdVsWarmStartLevel) {
    SqpOptions opts;
    Ledger ledger;

    const auto p_cold = make_hs(7);
    SqpDriver cold_driver(opts);
    cold_driver.attach_ledger(&ledger, "cold");
    const SqpSolution cold = cold_driver.solve(*p_cold.model);
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    ASSERT_EQ(cold.counters.start_level_used, StartLevel::kCold);

    const auto p_warm = make_hs(7);
    SqpDriver warm_driver(opts);
    warm_driver.attach_ledger(&ledger, "warm");
    const SqpSolution warm =
        warm_driver.solve(*p_warm.model, p_warm.model->start_point(), cold.warm_start);
    ASSERT_EQ(warm.status, SqpStatus::kOptimal);
    ASSERT_EQ(warm.counters.start_level_used, StartLevel::kWarm);

    ASSERT_EQ(ledger.sqp_records().size(), 2u);
    const SqpSolveRecord &cold_rec = ledger.sqp_records()[0];
    const SqpSolveRecord &warm_rec = ledger.sqp_records()[1];

    EXPECT_EQ(cold_rec.label, "cold_0");
    EXPECT_EQ(cold_rec.start_level_used, StartLevel::kCold);
    EXPECT_EQ(warm_rec.label, "warm_0");
    EXPECT_EQ(warm_rec.start_level_used, StartLevel::kWarm);

    // The flat fields never disagree with the counters they were copied
    // from in the same statement list (ledger.h's SqpSolveRecord note).
    EXPECT_EQ(cold_rec.start_level_used, cold_rec.counters.start_level_used);
    EXPECT_EQ(warm_rec.start_level_used, warm_rec.counters.start_level_used);

    // Neither run reaches kHot (this fixture never offers a `hot` handle
    // opt-in beyond the default StartLevel::kWarm ceiling), so
    // factorizations_saved is 0 on both -- a correct reading, not a missing
    // one (see that field's own note).
    EXPECT_EQ(cold_rec.factorizations_saved, 0);
    EXPECT_EQ(warm_rec.factorizations_saved, 0);

    const std::string table = ledger.sqp_summary_table();
    EXPECT_NE(table.find("Level"), std::string::npos) << table;
    EXPECT_NE(table.find("Cold"), std::string::npos) << table;
    EXPECT_NE(table.find("Warm"), std::string::npos) << table;
}

// (b) A warm object from one problem (HS7: n=2, me=1, mi=0) fed to a
// STRUCTURALLY DIFFERENT one (HS10: n=2, me=0, mi=1 -- same n, but the
// duals/qp_working_set dimensions differ, so warm_dims_plausible is false
// and the structure_hash probe is never even attempted) must degrade to a
// cold solve rather than throw or misbehave. "Cold-equivalent" is checked
// directly against a genuine 2-arg cold solve of HS10 from the SAME x0,
// which is the strongest form of the claim (not merely "some status/count",
// but bit-for-bit the same run) -- and both counts are pinned besides.
TEST(WarmStart, StaleWarmIsSafe) {
    SqpOptions opts;

    const auto p7 = make_hs(7);
    SqpDriver driver7(opts);
    const SqpSolution sol7 = driver7.solve(*p7.model);
    ASSERT_EQ(sol7.status, SqpStatus::kOptimal);
    ASSERT_TRUE(sol7.warm_start.valid);

    const auto p10 = make_hs(10);
    const Vec x0_10 = p10.model->start_point();

    SqpDriver cold_driver10(opts);
    const SqpSolution cold10 = cold_driver10.solve(*p10.model, x0_10);
    ASSERT_EQ(cold10.status, SqpStatus::kOptimal);
    EXPECT_EQ(cold10.counters.major_iters, 14);

    SqpDriver stale_driver10(opts);
    const SqpSolution stale = stale_driver10.solve(*p10.model, x0_10, sol7.warm_start);

    EXPECT_EQ(stale.counters.start_level_used, StartLevel::kCold);
    EXPECT_EQ(stale.status, cold10.status);
    EXPECT_EQ(stale.counters.major_iters, cold10.counters.major_iters);
    EXPECT_EQ(stale.x, cold10.x);
    EXPECT_DOUBLE_EQ(stale.f, cold10.f);
}

// (c) A HS7 warm object fed into a NEARBY problem (HS7 with its objective
// perturbed by a small linear term) resolves kWarm (same structure_hash --
// see Hs7PerturbedModel's own note) and converges in strictly fewer majors
// than a cold solve of the SAME perturbed problem. Both counts are pinned
// observed values; the mutation-verify step (ignoring the seeded duals)
// regresses this count, which is what makes it a positive test of the dual
// seeding rather than only of x0/activity.
TEST(WarmStart, PerturbedWarmBeatsCold) {
    SqpOptions opts;

    const auto p7 = make_hs(7);
    SqpDriver driver7(opts);
    const SqpSolution sol7 = driver7.solve(*p7.model);
    ASSERT_EQ(sol7.status, SqpStatus::kOptimal);

    Hs7PerturbedModel perturbed(1.0e-3);

    SqpDriver cold_driver(opts);
    const SqpSolution cold = cold_driver.solve(perturbed, perturbed.start_point());
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    EXPECT_EQ(cold.counters.major_iters, 9);

    SqpDriver warm_driver(opts);
    const SqpSolution warm = warm_driver.solve(perturbed, perturbed.start_point(), sol7.warm_start);
    ASSERT_EQ(warm.status, SqpStatus::kOptimal);
    EXPECT_EQ(warm.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(warm.counters.major_iters, 1);
    EXPECT_LT(warm.counters.major_iters, cold.counters.major_iters);
}

// (d) CARRIED FROM TASK 2's REVIEW: HS7-only testing left the sign-mapping
// branches of ineq_active/bound_active unverified because HS7 has neither
// inequalities nor finite bounds. HS76 (hs_problems.h) has both, active at
// its own documented solution: g1 (ci1) is active (its comment's x* sums to
// exactly 5 on the first general row) while g2/g3 are strictly satisfied,
// and x3 sits at its lower bound 0 while x1/x2/x4 are strictly positive --
// so this is a positive check of the +1/-1/1 encodings warm_start.h
// specifies, not merely of the all-zero case HS7 exercises.
TEST(WarmStart, ActivityMappingMarksKnownActiveRowsOnHs76) {
    const auto p = make_hs(76);
    SqpOptions opts;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);

    const WarmStart &w = sol.warm_start;
    ASSERT_TRUE(w.valid);
    ASSERT_EQ(w.ineq_active.size(), static_cast<std::size_t>(p.model->mi()));
    ASSERT_EQ(w.bound_active.size(), static_cast<std::size_t>(p.model->n()));

    // g1 active, g2/g3 not.
    EXPECT_EQ(w.ineq_active[0], 1) << "g1 (ci1) is active at HS76's solution";
    EXPECT_EQ(w.ineq_active[1], 0) << "g2 (ci2) is strictly satisfied";
    EXPECT_EQ(w.ineq_active[2], 0) << "g3 (ci3) is strictly satisfied";

    // x3 (index 2) at its lower bound 0; x1/x2/x4 free.
    EXPECT_EQ(w.bound_active[0], 0);
    EXPECT_EQ(w.bound_active[1], 0);
    EXPECT_EQ(w.bound_active[2], -1) << "x3 sits at its lower bound 0";
    EXPECT_EQ(w.bound_active[3], 0);

    // The engine's own working set agrees with the same report.
    EXPECT_EQ(w.qp_working_set.active_ineq(), (std::vector<Index>{0}));
    EXPECT_EQ(w.qp_working_set.bound_state()[2], BoundState::kAtLower);
}

// =============================================================================
// PHASE-4 TASK 4: hot level -- retained-factorization reuse across driver
// solves.
// =============================================================================

namespace {

// A local re-derivation of test_sqp_driver.cpp's ScaledRowModel (that file's
// own copy lives in an anonymous namespace there, so it is not reachable from
// this TU): n=2, me=1, mi=0, H = hval*obj_scale*I and Ae = [a, a] on EVERY
// call regardless of x -- constant by construction, exactly the shape
// qp_engine.h's HOT-START REUSE conditions (a)/(c) need to hold trivially
// across two otherwise-unrelated solves. `hval` is this file's own addition:
// build_subproblem (sqp_driver.h) always calls eval_hess with obj_scale = 1.0,
// so it is the only way to change H's VALUE independently of its PATTERN,
// which the mutation-style test below needs and obj_scale alone cannot give.
class ScaledRowModel : public NlpModel {
  public:
    ScaledRowModel(double a, double lo, Vec x0, double hval = 1.0)
        : a_(a), lo_(lo), x0_(x0), hval_(hval) {}
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return 0.5 * hval_ * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return hval_ * x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << a_ * x(0) + a_ * x(1) - 2.0 * a_;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * hval_;
        h.insert(1, 1) = obj_scale * hval_;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = a_;
        j.insert(0, 1) = a_;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        lower_ = Vec::Constant(2, -kScaledRowInfBound);
        lower_(0) = lo_;
        return lower_;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kScaledRowInfBound);
        return u;
    }
    Vec start_point() const override { return x0_; }

  private:
    static constexpr double kScaledRowInfBound = 1e20;
    double a_, lo_;
    Vec x0_;
    double hval_;
    mutable Vec lower_;
};

// FIX ROUND 1: n=2, me=0, mi=1 -- test_qp_engine_border.cpp's own
// simple_box_qp() shape (H=I, one general row x0+x1<=bi, box [0,10]^2),
// reachable through an NlpModel so it can drive SqpDriver rather than
// QpEngine directly. UNLIKE ScaledRowModel (equality-only, no general row),
// this model's optimum can activate BOTH a general row AND a bound
// simultaneously -- exactly the shape qp_engine.h's own
// RebuildUnderTinySchurCapMatchesRefactorize needs to force a genuine
// mid-solve K0 rebuild under a tiny QpOptions::schur_cap, which is what the
// SYMMETRIC generation-counter case (below) needs: a producer whose OWN
// top-level reuse conditions (a)-(d) keep passing across several of its own
// solves (so it never detaches) can still rebuild K0 mid-solve, purely from
// schur_cap pressure, changing BorderState::generation without changing
// anything (a)-(d) can see. c0/c1 are the (otherwise free -- see
// warm_start.h's own note that g/b never enter the reuse key) linear-term
// coefficients that move the optimum; `bi` is the row's own RHS, likewise
// free of the hash.
class RowAndBoundModel : public NlpModel {
  public:
    RowAndBoundModel(double c0, double c1, double bi, Vec x0)
        : c0_(c0), c1_(c1), bi_(bi), x0_(x0) {}
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * x.squaredNorm() - c0_ * x(0) - c1_ * x(1);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g = x;
        g(0) -= c0_;
        g(1) -= c1_;
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c << x(0) + x(1) - bi_;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Zero(2);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 10.0);
        return u;
    }
    Vec start_point() const override { return x0_; }

  private:
    double c0_, c1_, bi_;
    Vec x0_;
};

} // namespace

// THE BRIEF'S TEST, SUCCESS HALF: solve a constant-Hessian model, feed its
// warm_start (including the Task-4 hot handle) BACK, UNCHANGED IN H/Ae, into
// a solve on a FRESH SqpDriver/QpEngine instance that has never solved
// anything of its own -- exactly the cross-instance hand-off Task 4 exists
// for. THE FIRST subproblem on that brand-new engine must skip K0's assembly
// and factorization outright (qp_factorizations == 0), and start_level_used
// must read kHot.
//
// THE CONSUMER'S BOUND IS DELIBERATELY DIFFERENT (lo = 1.0, not the
// producer's 1.5) -- bounds are NOT part of H/Ae/Ai and so never enter
// structural_hash/values_hash (qp_engine.h), but they DO move the true
// optimum. Feeding the producer's exact optimum (1.5, 0.5) back "unchanged"
// into a problem whose own optimum is elsewhere (here, (1, 1): the
// unconstrained-along-the-equality minimizer, since 1.0 <= 1 no longer binds)
// means the consumer's very first convergence check does NOT fire, so a REAL
// first subproblem is solved and qp_factorizations == 0 is a meaningful pin
// rather than a vacuous "no QP ever ran" one. qp_engine.h's own
// ingest_seed_working_set re-pins a seeded bound at the CURRENT solve's own
// bound value regardless of where the driver's x geometrically sits, so the
// working-set SHAPE the seed carries (var0 pinned, var1 free) still matches
// the producer's exit shape exactly -- condition (b) holds despite the
// different number.
TEST(WarmStart, HotReusesFactorization) {
    const double a = 1e-3;
    SqpOptions opts;
    // Keep the effective (primal_delta, dual_mu) pair -- condition (d) --
    // identical across both solves by construction, rather than by luck:
    // with the schedule off, every solve on this driver runs at the engine's
    // own fixed defaults.
    opts.adaptive_mu = false;
    // kWarm is the CEILING's default (sqp_types.h's SqpOptions::start_level);
    // kHot must be explicitly raised to, exactly like enable_soc/adaptive_mu.
    opts.start_level = StartLevel::kHot;

    ScaledRowModel producer(a, /*lo=*/1.5, Vec::Zero(2));
    SqpDriver driver1(opts);
    const SqpSolution sol1 = driver1.solve(producer);
    ASSERT_EQ(sol1.status, SqpStatus::kOptimal);
    ASSERT_TRUE(sol1.warm_start.valid);
    ASSERT_NE(sol1.warm_start.hot, nullptr)
        << "border mode (this driver's default ws_algebra) must commit a hot handle on a clean "
           "kOptimal exit";

    ScaledRowModel consumer(a, /*lo=*/1.0, Vec::Zero(2));
    SqpDriver driver2(opts); // a FRESH engine instance: never solved anything of its own
    const SqpSolution sol2 = driver2.solve(consumer, consumer.start_point(), sol1.warm_start);

    ASSERT_FALSE(sol2.history.empty());
    ASSERT_TRUE(sol2.history[0].qp_solved) << "the bound change must force a real first subproblem";
    EXPECT_EQ(sol2.history[0].qp_factorizations, 0)
        << "the FIRST subproblem on a brand-new engine instance must reuse the producer's K0 "
           "outright via the hot handle -- THE PIN";
    EXPECT_EQ(sol2.counters.start_level_used, StartLevel::kHot);
    EXPECT_EQ(sol2.status, SqpStatus::kOptimal);
}

// BITWISE comparison of two doubles, the idiom the three R6 pins share: the
// bit patterns, not `==`. See the call sites below for why that distinction is
// the one R6 asks for.
void expect_same_bits(double a, double b, const std::string &what) {
    EXPECT_EQ(std::bit_cast<std::uint64_t>(a), std::bit_cast<std::uint64_t>(b))
        << what << ": " << a << " vs " << b;
}

// M5 R6: HOT-STATE REUSE IS NEVER OBSERVABLE IN ANSWERS.
//
// The test above pins that the reuse HAPPENS. This one pins the consequence
// M5's R6 asks for: the solve that reused the producer's K0 must answer
// exactly what the same solve answers with the reuse withheld -- same point,
// same prices, same terminal residuals, same iterate path -- so a consumer can
// never tell from an answer whether an engine was hot.
//
// THE CONTROL IS THE SAME WARM OBJECT WITH ITS HANDLE STRIPPED. Everything
// else about the two solves is identical by construction: the same fixture,
// the same start point, the same options, the same ingested x/duals/activity.
// Only `hot` differs, so only the reuse differs -- which is what makes this a
// measurement of the reuse rather than of two loosely similar runs.
//
// THE ONE COUNTER THAT MAY DIFFER IS THE ONE THAT COUNTS THE SAVED WORK, and
// it is pinned by an EXACT relation rather than exempted: withholding the
// handle costs exactly one more factorization, the one the fast path skipped.
// Every other counter, and the whole history, must match. Wall time is not
// read here at all -- it is informational, and skipping a factorization is
// expected to move it.
//
// EXACT EQUALITY, no margins: the reuse is answer-neutral BY CONSTRUCTION (the
// same K0, factorized once instead of twice), so a tolerance here would be a
// claim that it perturbs the answer slightly -- the very claim R6 forbids.
TEST(WarmStart, HotReuseIsNeverAnswerObservable) {
    const double a = 1e-3;
    SqpOptions opts;
    opts.adaptive_mu = false; // condition (d), held by construction
    opts.start_level = StartLevel::kHot;

    ScaledRowModel producer(a, /*lo=*/1.5, Vec::Zero(2));
    SqpDriver producer_driver(opts);
    const SqpSolution produced = producer_driver.solve(producer);
    ASSERT_EQ(produced.status, SqpStatus::kOptimal);
    ASSERT_NE(produced.warm_start.hot, nullptr);

    ScaledRowModel consumer(a, /*lo=*/1.0, Vec::Zero(2));

    SqpDriver hot_driver(opts);
    const SqpSolution hot = hot_driver.solve(consumer, consumer.start_point(), produced.warm_start);

    // The control: byte-for-byte the same warm object, minus the handle.
    WarmStart without_handle = produced.warm_start;
    without_handle.hot = nullptr;
    SqpDriver warm_driver(opts);
    const SqpSolution warm = warm_driver.solve(consumer, consumer.start_point(), without_handle);

    // THE FIXTURE PREMISE: one solve really was hot and the other really was
    // not. Without this the identity below would pass vacuously on two cold
    // solves.
    ASSERT_EQ(hot.counters.start_level_used, StartLevel::kHot);
    ASSERT_EQ(warm.counters.start_level_used, StartLevel::kWarm);
    ASSERT_FALSE(hot.history.empty());
    ASSERT_FALSE(warm.history.empty());
    ASSERT_EQ(hot.history[0].qp_factorizations, 0);
    ASSERT_GE(warm.history[0].qp_factorizations, 1);

    // THE ANSWER, bit for bit -- read as BITS, not as `==`: EXPECT_EQ on two
    // doubles calls -0.0 and +0.0 equal though their bits differ, and would
    // call NaN unequal to itself. Same idiom as the other two R6 pins.
    EXPECT_EQ(hot.status, warm.status);
    expect_same_bits(hot.f, warm.f, "objective");
    expect_same_bits(hot.stationarity, warm.stationarity, "stationarity");
    expect_same_bits(hot.feasibility, warm.feasibility, "feasibility");
    expect_same_bits(hot.complementarity, warm.complementarity, "complementarity");
    expect_same_bits(hot.kkt_residual, warm.kkt_residual, "kkt_residual");
    ASSERT_EQ(hot.x.size(), warm.x.size());
    for (Index i = 0; i < hot.x.size(); ++i) {
        expect_same_bits(hot.x(i), warm.x(i), "primal " + std::to_string(i));
        expect_same_bits(hot.z(i), warm.z(i), "bound multiplier " + std::to_string(i));
    }
    ASSERT_EQ(hot.lambda_e.size(), warm.lambda_e.size());
    for (Index i = 0; i < hot.lambda_e.size(); ++i) {
        expect_same_bits(hot.lambda_e(i), warm.lambda_e(i),
                         "equality multiplier " + std::to_string(i));
    }
    ASSERT_EQ(hot.lambda_i.size(), warm.lambda_i.size());
    for (Index i = 0; i < hot.lambda_i.size(); ++i) {
        expect_same_bits(hot.lambda_i(i), warm.lambda_i(i),
                         "inequality multiplier " + std::to_string(i));
    }

    // THE PATH, not only the endpoint.
    ASSERT_EQ(hot.history.size(), warm.history.size());
    for (std::size_t k = 0; k < hot.history.size(); ++k) {
        const SqpIterate &h = hot.history[k];
        const SqpIterate &w = warm.history[k];
        const std::string row = "row " + std::to_string(k) + " ";
        expect_same_bits(h.f, w.f, row + "f");
        expect_same_bits(h.stationarity, w.stationarity, row + "stationarity");
        expect_same_bits(h.feasibility, w.feasibility, row + "feasibility");
        expect_same_bits(h.complementarity, w.complementarity, row + "complementarity");
        expect_same_bits(h.kkt_residual, w.kkt_residual, row + "kkt_residual");
        expect_same_bits(h.violation_l1, w.violation_l1, row + "violation_l1");
        expect_same_bits(h.tr_radius, w.tr_radius, row + "tr_radius");
        expect_same_bits(h.step_norm, w.step_norm, row + "step_norm");
        EXPECT_EQ(h.verdict, w.verdict) << "row " << k;
        EXPECT_EQ(h.qp_status, w.qp_status) << "row " << k;
        EXPECT_EQ(h.qp_minor_iters, w.qp_minor_iters) << "row " << k;
    }

    // THE WORK, with the saved factorization named exactly.
    EXPECT_EQ(warm.counters.factorizations, hot.counters.factorizations + 1)
        << "the hot solve skips exactly the one K0 factorization the handle carried";
    EXPECT_EQ(hot.counters.major_iters, warm.counters.major_iters);
    EXPECT_EQ(hot.counters.qp_minor_iters, warm.counters.qp_minor_iters);
    EXPECT_EQ(hot.counters.steps_accepted, warm.counters.steps_accepted);
    EXPECT_EQ(hot.counters.rejected_steps, warm.counters.rejected_steps);
    EXPECT_EQ(hot.counters.soc_steps, warm.counters.soc_steps);
    EXPECT_EQ(hot.counters.elastic_activations, warm.counters.elastic_activations);
    EXPECT_EQ(hot.counters.restoration_iters, warm.counters.restoration_iters);
    EXPECT_EQ(hot.counters.border_refine_steps, warm.counters.border_refine_steps);
}

// THE BRIEF'S TEST, DEGRADATION HALF: changing one H VALUE (same pattern,
// same n/me/mi, same nonzero positions) between the producer and the
// consumer must force a genuine refactorization on the consumer's first
// subproblem and report start_level_used == kWarm, not kHot -- Task 4's
// "any failure degrades to kWarm silently" contract, pinned on the specific
// failure mode qp_engine.h's condition (c) exists to catch. The bound is
// ALSO changed here (same lo = 1.0 as the success test above), for the same
// reason as there: without it the objective's uniform rescaling by `hval`
// would leave (1.5, 0.5) exactly optimal for the consumer too (scaling a
// convex quadratic's argmin does not move it), and the solve would converge
// in zero majors with no subproblem to observe at all.
TEST(WarmStart, ChangedHValueDegradesToWarmWithRefactorization) {
    const double a = 1e-3;
    SqpOptions opts;
    opts.adaptive_mu = false;
    opts.start_level = StartLevel::kHot;

    ScaledRowModel producer(a, /*lo=*/1.5, Vec::Zero(2));
    SqpDriver driver1(opts);
    const SqpSolution sol1 = driver1.solve(producer);
    ASSERT_EQ(sol1.status, SqpStatus::kOptimal);
    ASSERT_NE(sol1.warm_start.hot, nullptr);

    ScaledRowModel consumer(a, /*lo=*/1.0, Vec::Zero(2), /*hval=*/2.0);
    SqpDriver driver2(opts);
    const SqpSolution sol2 = driver2.solve(consumer, consumer.start_point(), sol1.warm_start);

    ASSERT_FALSE(sol2.history.empty());
    ASSERT_TRUE(sol2.history[0].qp_solved);
    EXPECT_GE(sol2.history[0].qp_factorizations, 1)
        << "a values-hash mismatch (condition (c)) must force a genuine refactorization on the "
           "first subproblem -- THE PIN";
    EXPECT_EQ(sol2.counters.start_level_used, StartLevel::kWarm)
        << "start_level_used records what was OBSERVED (a refactorization happened), not merely "
           "what warm.hot offered";
    EXPECT_EQ(sol2.status, SqpStatus::kOptimal);
}

// PHASE-4 TASK 7 (cold-vs-warm ledger instrumentation): factorizations_saved
// (ledger.h's SqpSolveRecord) MUTATION-VERIFIED against the exact pair of
// fixtures above, re-run with a ledger attached to `driver2` in each. Same
// code path, one value mutated (H's byte content on the consumer) --
// start_level_used flips kHot -> kWarm and factorizations_saved must flip
// 1 -> 0 in lockstep, since ledger.h defines it as exactly
// (start_level_used == kHot) ? 1 : 0. A field that stayed at 1 (or landed at
// some other constant) after this mutation would mean the "derived, never an
// independent measurement" claim in that header's note is false.
TEST(WarmStart, LedgerFactorizationsSavedTracksHotVsDegradedWarm) {
    const double a = 1e-3;
    SqpOptions opts;
    opts.adaptive_mu = false;
    opts.start_level = StartLevel::kHot;

    Ledger ledger;

    // THE HOT CASE (HotReusesFactorization's own fixture): a clean hand-off,
    // no value change, the first subproblem genuinely reuses K0.
    {
        ScaledRowModel producer(a, /*lo=*/1.5, Vec::Zero(2));
        SqpDriver driver1(opts);
        const SqpSolution sol1 = driver1.solve(producer);
        ASSERT_EQ(sol1.status, SqpStatus::kOptimal);
        ASSERT_NE(sol1.warm_start.hot, nullptr);

        ScaledRowModel consumer(a, /*lo=*/1.0, Vec::Zero(2));
        SqpDriver driver2(opts);
        driver2.attach_ledger(&ledger, "hot");
        const SqpSolution sol2 = driver2.solve(consumer, consumer.start_point(), sol1.warm_start);
        ASSERT_EQ(sol2.status, SqpStatus::kOptimal);
        ASSERT_EQ(sol2.counters.start_level_used, StartLevel::kHot);
    }

    // THE DEGRADED CASE (ChangedHValueDegradesToWarmWithRefactorization's own
    // fixture): one H value changed on the consumer, forcing a genuine
    // refactorization -- start_level_used degrades to kWarm.
    {
        ScaledRowModel producer(a, /*lo=*/1.5, Vec::Zero(2));
        SqpDriver driver1(opts);
        const SqpSolution sol1 = driver1.solve(producer);
        ASSERT_EQ(sol1.status, SqpStatus::kOptimal);
        ASSERT_NE(sol1.warm_start.hot, nullptr);

        ScaledRowModel consumer(a, /*lo=*/1.0, Vec::Zero(2), /*hval=*/2.0);
        SqpDriver driver2(opts);
        driver2.attach_ledger(&ledger, "warm");
        const SqpSolution sol2 = driver2.solve(consumer, consumer.start_point(), sol1.warm_start);
        ASSERT_EQ(sol2.status, SqpStatus::kOptimal);
        ASSERT_EQ(sol2.counters.start_level_used, StartLevel::kWarm);
    }

    ASSERT_EQ(ledger.sqp_records().size(), 2u);
    const SqpSolveRecord &hot_rec = ledger.sqp_records()[0];
    const SqpSolveRecord &warm_rec = ledger.sqp_records()[1];

    EXPECT_EQ(hot_rec.start_level_used, StartLevel::kHot);
    EXPECT_EQ(hot_rec.factorizations_saved, 1) << "THE PIN: kHot saved exactly its one skip";
    EXPECT_EQ(warm_rec.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(warm_rec.factorizations_saved, 0)
        << "THE PIN: a degraded-to-kWarm resolution saves nothing, and that 0 is a correct "
           "reading, not a missing measurement -- see ledger.h's SqpSolveRecord note";
}

// =============================================================================
// FIX ROUND 1: the aliasing hazard the review found -- BorderState::
// generation (qp_engine.h's fifth reuse condition) and the DETACH-on-refusal
// fix (qp_engine.h's rebuild_k0/reuse_eligible block).
// =============================================================================

// THE REVIEW'S OWN DEMONSTRATED PROBE (task-4-review.md, finding 1): a
// producer emits a hot handle, then SOLVES AGAIN on its own engine before
// the handle is ever consumed -- an entirely ordinary thing to do with a
// WarmStart, a plain copyable value the caller is invited to store -- and
// only THEN is the ORIGINAL handle fed to a fresh consumer. Before Fix
// Round 1 this certified kHot with qp_factorizations == 0 against the
// INTERMEDIATE problem's K0: a silent wrong answer (the review measured 55
// majors to recover vs. a control's 1, on its own fixture).
//
// THIS FIXTURE USES RowAndBoundModel with QpOptions::schur_cap FORCED TO 1,
// which is the shape that reproduces the failure WITHOUT relying on
// producer's own top-level reuse check failing (a values/structural
// mismatch there would trigger the DETACH fix -- qp_engine.h's Q3 -- on its
// own, which already keeps a REFUSED adoption from touching a shared
// object, and does not need the generation counter to do it). Here H/Ai are
// BYTE-IDENTICAL across every solve below (only the linear term c0/c1 and
// the row's own bi move -- neither enters qp_engine.h's structural/values
// hash), so producer's OWN top-level reuse conditions (a)-(d) keep passing
// across its own two further solves and it NEVER detaches -- it keeps
// mutating the SAME shared BorderState the very first handle still names.
// The tiny schur_cap forces a genuine mid-solve K0 rebuild on EACH of
// producer's solves (test_qp_engine_border.cpp's own
// RebuildUnderTinySchurCapMatchesRefactorize technique), bumping
// BorderState::generation each time even though (a)-(d) alone see nothing
// change.
//
// THE SEQUENCE:
//   1. driver1 solves model_a (row x0+x1<=1, optimum (0,1): row AND the
//      x0>=0 bound both active -- test_qp_engine_border.cpp's own
//      simple_box_qp() shape). sol1.warm_start is captured as `handle`
//      BEFORE anything else touches driver1.
//   2. CONTROL consumes `handle` immediately, on a FRESH driver, against
//      model_b (same H/Ai, bi nudged from 1 to 0.9 -- moves the optimum
//      without touching the hash) -- nothing has mutated the shared object
//      yet, so this is the "clean hand-off" this mechanism exists to make
//      fast, and it pins the major count/answer the POISONED run below is
//      compared against.
//   3. driver1 (the SAME engine that produced `handle`) solves an
//      unrelated DETOUR problem next (optimum (0,0): BOTH bounds active,
//      the row inactive) -- a real, mid-solve K0 rebuild, forced by
//      schur_cap.
//   4. driver1 solves model_b -- the SAME problem CONTROL solved -- warm
//      from the detour's own exit. This walks the shared object BACK to a
//      row-active/x0-pinned shape (matching `handle`'s own shape again,
//      just at a LATER generation), via another forced rebuild.
//   5. POISONED consumes the ORIGINAL `handle` (from step 1, unaffected by
//      steps 3-4 -- WarmStart/HotState are immutable value copies; only the
//      BorderState they share ownership of was mutated) on a FRESH driver,
//      against model_b -- the identical problem CONTROL solved.
//
// Every one of (a)-(d) matches for POISONED exactly as it did for CONTROL
// (same H/Ai bytes, same effective primal_delta/dual_mu, same committed
// exit shape) -- only BorderState::generation differs, since driver1's own
// two solves in between moved it. That is exactly the case
// qp_engine.h's condition (e) note calls the SYMMETRIC one.
TEST(WarmStart, ProducerSolvingAgainBeforeHandleConsumedDegradesToWarm) {
    SqpOptions opts;
    opts.adaptive_mu = false;
    opts.start_level = StartLevel::kHot;
    opts.tr_init = std::numeric_limits<double>::infinity();
    opts.qp.schur_cap = 1;

    RowAndBoundModel model_a(/*c0=*/1.0, /*c1=*/2.0, /*bi=*/1.0, Vec::Zero(2));
    SqpDriver driver1(opts);
    const SqpSolution sol1 = driver1.solve(model_a);
    ASSERT_EQ(sol1.status, SqpStatus::kOptimal);
    ASSERT_NE(sol1.warm_start.hot, nullptr);
    const WarmStart handle = sol1.warm_start; // captured BEFORE driver1 solves again

    // --- CONTROL: consume `handle` immediately, before anything else
    // touches the shared object.
    RowAndBoundModel model_b(/*c0=*/1.0, /*c1=*/2.0, /*bi=*/0.9, Vec::Zero(2));
    SqpDriver driver_control(opts);
    const SqpSolution control = driver_control.solve(model_b, model_b.start_point(), handle);
    ASSERT_EQ(control.status, SqpStatus::kOptimal);
    ASSERT_FALSE(control.history.empty());
    ASSERT_TRUE(control.history[0].qp_solved);
    EXPECT_EQ(control.history[0].qp_factorizations, 0)
        << "the clean hand-off must reuse outright -- this is the baseline POISONED is judged "
           "against";
    EXPECT_EQ(control.counters.start_level_used, StartLevel::kHot);

    // --- driver1 solves TWO MORE problems on its OWN engine: an unrelated
    // detour, then model_b itself (warm from the detour) -- neither ever
    // fails driver1's OWN top-level reuse check (same H/Ai bytes
    // throughout), so driver1 never detaches; both are real, schur_cap-
    // forced mid-solve rebuilds on the SAME shared BorderState `handle`
    // still names.
    RowAndBoundModel detour(/*c0=*/-1.0, /*c1=*/-1.0, /*bi=*/5.0, Vec::Zero(2));
    const SqpSolution sol_detour = driver1.solve(detour, detour.start_point(), sol1.warm_start);
    ASSERT_EQ(sol_detour.status, SqpStatus::kOptimal);

    const SqpSolution sol_return =
        driver1.solve(model_b, model_b.start_point(), sol_detour.warm_start);
    ASSERT_EQ(sol_return.status, SqpStatus::kOptimal);

    // --- POISONED: a FRESH consumer adopts the ORIGINAL (now-stale)
    // `handle` against the SAME problem CONTROL solved.
    SqpDriver driver_poisoned(opts);
    const SqpSolution poisoned = driver_poisoned.solve(model_b, model_b.start_point(), handle);

    ASSERT_FALSE(poisoned.history.empty());
    ASSERT_TRUE(poisoned.history[0].qp_solved);
    EXPECT_GE(poisoned.history[0].qp_factorizations, 1)
        << "a stale generation must force a genuine refactorization -- THE PIN";
    EXPECT_EQ(poisoned.counters.start_level_used, StartLevel::kWarm)
        << "degrades silently rather than certifying kHot against a handle whose factorization "
           "has moved on since it was emitted";
    EXPECT_EQ(poisoned.status, SqpStatus::kOptimal);
    EXPECT_EQ(poisoned.counters.major_iters, control.counters.major_iters)
        << "same seed, same problem -- correctly RECOMPUTED rather than wrongly reused, so the "
           "trajectory matches the control exactly";
    EXPECT_EQ(poisoned.x, control.x);
}

// FIX ROUND 1 (Q3): a REFUSED adoption must not corrupt the shared
// BorderState for anyone ELSE still relying on it -- in particular, for
// the PRODUCER's own engine, which keeps using the SAME object and whose
// own per-instance fingerprint members (border_valid_/border_structural_
// hash_/etc.) are NOT shared and so cannot detect that a DIFFERENT engine
// mutated the object out from under it (that is exactly what
// BorderState::generation, pinned above, is for -- but generation only
// helps because the FIX here stops the mutation from ever landing on a
// shared object at all). Before this fix, qp_engine.h's `!reuse_eligible`
// branch cleared schur/latched/k0_rows/ledger ON THE SHARED OBJECT and the
// NEXT rebuild_k0() call -- driven by the REFUSING engine's OWN problem --
// refactorized it, silently overwriting whatever the producer had built.
TEST(WarmStart, RefusedAdoptionDoesNotCorruptProducersOwnState) {
    const double a = 1e-3;
    SqpOptions opts;
    opts.adaptive_mu = false;
    opts.start_level = StartLevel::kHot;

    ScaledRowModel producer(a, /*lo=*/1.5, Vec::Zero(2));
    SqpDriver driver1(opts);
    const SqpSolution sol1 = driver1.solve(producer);
    ASSERT_EQ(sol1.status, SqpStatus::kOptimal);
    ASSERT_NE(sol1.warm_start.hot, nullptr);

    // A consumer with a CHANGED H VALUE adopts `sol1.warm_start` and
    // refuses it -- exactly ChangedHValueDegradesToWarmWithRefactorization's
    // own scenario, run here purely for its SIDE EFFECT (or, post-fix, lack
    // thereof) on the shared BorderState.
    ScaledRowModel consumer_bad(a, /*lo=*/1.0, Vec::Zero(2), /*hval=*/2.0);
    SqpDriver driver_bad(opts);
    const SqpSolution bad =
        driver_bad.solve(consumer_bad, consumer_bad.start_point(), sol1.warm_start);
    ASSERT_EQ(bad.status, SqpStatus::kOptimal);
    ASSERT_EQ(bad.counters.start_level_used, StartLevel::kWarm)
        << "the refusal itself, as expected";

    // driver1's OWN NEXT SOLVE, warm-seeded from its OWN prior exit -- must
    // still reuse its OWN state outright. Under the pre-fix bug,
    // driver_bad's refusal above would have rebuilt the SAME shared
    // BorderState for consumer_bad's hval=2 problem, and this solve would
    // have silently inherited that WRONG K0: driver1's own fingerprint
    // members are per-engine and untouched by driver_bad, so its OWN
    // reuse_eligible check would still read true -- the corruption is
    // invisible to the very check meant to catch it, which is exactly why
    // the fix has to be DETACH rather than a smarter check.
    ScaledRowModel producer_again(a, /*lo=*/1.2, Vec::Zero(2));
    const SqpSolution sol_again =
        driver1.solve(producer_again, producer_again.start_point(), sol1.warm_start);

    ASSERT_EQ(sol_again.status, SqpStatus::kOptimal);
    ASSERT_FALSE(sol_again.history.empty());
    ASSERT_TRUE(sol_again.history[0].qp_solved);
    EXPECT_EQ(sol_again.history[0].qp_factorizations, 0)
        << "driver_bad's refused adoption must not have touched driver1's OWN BorderState -- THE "
           "PIN";

    Vec x_star(2);
    x_star << 1.2, 0.8; // a fixed, lo=1.2: x0 pins at 1.2, x1 = 2 - 1.2 = 0.8
    EXPECT_LT((sol_again.x - x_star).norm(), 1e-3)
        << "and the answer must be the correct one for THIS problem (up to the engine's own "
           "default regularization footprint), not one contaminated by consumer_bad's hval=2";
}

// =============================================================================
// FIX ROUND 2: the use_count()-guarded detach (perf regression on the
// ordinary, never-shared path).
// =============================================================================

// Re-review finding 2c: detaching UNCONDITIONALLY on every `!reuse_eligible`
// (Fix Round 1's own code) meant a fresh BorderState -- hence a fresh
// KktFactor, with no cached Pardiso sparsity pattern -- on every
// value-changing major, even when `border_` was never shared with anyone.
// HS38's H changes at essentially every major (a real, non-toy SQP
// trajectory), so its own engine's top-level reuse check fails on
// essentially every major and a detach would fire that often. Measured by
// the re-review with a temporary counter (since reverted): 48 Pardiso
// phase-11 analyses across this exact solve pre-guard, vs 1 with the
// use_count() > 1 guard in place (qp_engine.h's reuse_eligible block) --
// this driver's engine_ is never shared with any other engine, so
// use_count() reads 1 at every one of those majors and the SAME
// BorderState/KktFactor is wiped and reused in place instead of replaced,
// keeping the cached sparsity pattern alive across every refactorization.
TEST(WarmStart, NeverSharedSolvePaysOneSymbolicAnalysis) {
    const auto p = make_hs(38);
    SqpOptions opts;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_GT(sol.counters.factorizations, 10)
        << "this fixture is only a meaningful regression net if H actually changes enough to "
           "force many refactorizations -- observed 48 on the re-review's own machine";
    EXPECT_EQ(sol.counters.symbolic_analyses, 1)
        << "a never-shared solve must pay Pardiso's phase-11 symbolic analysis ONCE for the "
           "whole solve, reusing the cached sparsity pattern across every refactorization -- "
           "THE PIN";
}

// =============================================================================
// PHASE-4 TASK 5: the Kungurtsev-Diehl FULL-STEP-FIRST rule and its watchdog.
// =============================================================================
//
// THE FIXTURE FAMILY, AND WHY IT IS NOT THE PERTURBED-HS7 ONE ABOVE. The
// brief's first choice was PerturbedWarmBeatsCold's own fixture at a larger
// perturbation, on the expectation that the funnel would be what costs the
// warm solve its majors. IT IS NOT, and that was MEASURED before these tests
// were written. THE SWEEP, stated so it is reproducible: Hs7PerturbedModel fed
// HS7's own converged warm object under DEFAULT options, eps over the grid
// {1e-3, 1e-2, 1e-1, 1, 2, 5, 10, 15, 20}, gated (warm_full_step = false)
// against full-step (the default):
//
//     eps      1e-3  1e-2  1e-1   1    2    5   10   15   20
//     gated majors  1     2     2    4    4    6    7    7    7
//     gated REJECTIONS  0 0     0    0    0    0    0    0    0
//     full-step majors  1 2     2    4    4    6    7    7    7
//
// with f bit-identical in every column and no watchdog anywhere. The funnel
// rejects NOTHING on this family -- the ingested width is orders of magnitude
// larger than any h those steps produce -- so full-step mode takes the
// identical steps and can only ever TIE. It cannot be "strictly below" a count
// the funnel was never inflating.
//
// WHAT DOES INFLATE IT is the OTHER half of a warm object: the MULTIPLIERS.
// They enter the subproblem through the Hessian of the Lagrangian, so a warm
// object whose duals are materially wrong for the problem being solved
// produces a badly-modelled first subproblem -- and THAT is what the funnel
// refuses, over and over, halving the radius each time until the step is small
// enough to be judged on curvature it can actually see. That is exactly [KD]'s
// "standard globalization actively interferes with warm starts", and it is the
// regime the rule was written for: the duals are re-priced by the very first
// QP that is allowed to take a step, so ONE unjudged full step is enough to
// repair them.
//
// So the fixtures below feed a problem its OWN converged warm object with
// lambda_e SCALED, which is the smallest change that produces the effect (x,
// the activity, the funnel width and the radius are all exactly the converged
// solve's -- only the dual moves) and which is, at scale = -1, literally the
// brief's "poisoned duals (negate lambda)". The two tests differ in WHICH
// REGIME that leaves the full steps in, and that difference is the whole point
// of the pair:
//   * test (a), HS7 at scale = -2: the wrong Hessian makes the funnel shrink
//     14 times, and the full step is good enough to converge anyway -- the
//     rule pays.
//   * test (b), HS40 at scale = -1: the full steps DIVERGE (||KKT||inf 0.94 ->
//     5.96 -> 2.28 -> 18.0 -> larger), and the watchdog is what saves the
//     solve. HS40 rather than HS7 because HS7's own full steps do NOT diverge
//     at any poison tried (-2 through -1000 all converge in 6 majors: the
//     trust region caps the first step, so a bigger poison changes only the
//     first residual and not the trajectory) -- the divergence signal needs a
//     problem whose linearization is misleading over the WHOLE radius, which
//     HS40's three coupled equalities are and HS7's single one is not.
// Neither fixture can reach the QP engine's suspect gate (K0 is nonsingular at
// every point either run visits -- observed suspect_escalations == 0
// throughout, asserted below), so per the Accelerate audit's standing rule
// these pins are not expected to need per-backend treatment.

// (a) FULL STEP FIRST CONVERGES IN FEWER MAJORS. Both counts are
// observed-value pins on the SAME warm object and the SAME problem, differing
// only in SqpOptions::warm_full_step -- which is the A/B this whole task is.
TEST(WarmStart, WarmFullStepConvergesInFewMajors) {
    const auto p7 = make_hs(7);
    SqpOptions base;
    SqpDriver seed_driver(base);
    const SqpSolution seed = seed_driver.solve(*p7.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);

    // The warm object, with the dual (and nothing else) made wrong for this
    // problem. -2 * 0.2887 = -0.577, which flips the sign of the Lagrangian
    // Hessian's constraint term.
    WarmStart mispriced = seed.warm_start;
    mispriced.lambda_e *= -2.0;

    // --- Task 3's behaviour: the funnel judges the first trial like any
    // other, and spends the solve shrinking the radius under it.
    SqpOptions funnel_opts = base;
    funnel_opts.warm_full_step = false;
    const auto p_funnel = make_hs(7);
    SqpDriver funnel_driver(funnel_opts);
    const SqpSolution gated =
        funnel_driver.solve(*p_funnel.model, p_funnel.model->start_point(), mispriced);
    ASSERT_EQ(gated.status, SqpStatus::kOptimal);
    ASSERT_EQ(gated.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(gated.counters.major_iters, 16);
    EXPECT_EQ(gated.counters.rejected_steps, 14)
        << "the funnel-gated run's cost IS its rejections -- this is what the rule removes";
    EXPECT_EQ(gated.counters.full_step_majors, 0);
    EXPECT_EQ(gated.counters.watchdog_restores, 0);

    // --- Task 5's behaviour: the full step is taken, the QP re-prices the
    // duals, and the solve converges.
    const auto p_full = make_hs(7);
    SqpDriver full_driver(base); // warm_full_step defaults ON
    const SqpSolution full =
        full_driver.solve(*p_full.model, p_full.model->start_point(), mispriced);

    EXPECT_EQ(full.status, SqpStatus::kOptimal);
    EXPECT_EQ(full.counters.start_level_used, StartLevel::kWarm);
    EXPECT_GE(full.counters.full_step_majors, 1)
        << "the mode must actually have run -- THE PIN this test would otherwise pass vacuously";
    EXPECT_EQ(full.counters.full_step_majors, 6) << "every major of this run is a full step";
    EXPECT_EQ(full.counters.major_iters, 6);
    EXPECT_LT(full.counters.major_iters, gated.counters.major_iters);
    EXPECT_EQ(full.counters.rejected_steps, 0);
    EXPECT_EQ(full.counters.watchdog_restores, 0)
        << "a converging full-step run exits through the ordinary KKT test, not the watchdog";
    EXPECT_EQ(full.counters.suspect_escalations, 0);

    // THE SAFETY INVARIANT, checked rather than asserted about: the point this
    // solve certified must pass the STANDARD KKT check, re-derived here from
    // the model with the driver's own evaluate_kkt -- the same function the
    // globalized path exits through.
    const SqpKkt certified =
        evaluate_kkt(*p_full.model, full.x, full.lambda_e, full.lambda_i, base.feas_tol);
    EXPECT_LE(certified.stationarity, base.kkt_tol);
    EXPECT_LE(certified.feasibility, base.feas_tol);
    EXPECT_NEAR(full.f, gated.f, 1e-8) << "and it is the SAME solution the gated run found";
}

// (b) THE WATCHDOG. HS40 (n=4, me=3) fed its own converged warm object with
// lambda_e NEGATED -- the brief's poison, verbatim, at default options.
//
// WHAT WAS MEASURED, and it is the divergence signal rather than the stall
// one: the full-step run's ||KKT||inf goes 9.44e-1 (the warm point, and the
// best iterate of the whole mode) -> 5.96 -> 2.28 -> 1.80e1 -> larger still,
// so kWarmResidualGrowthMax = 2 consecutive growths is reached on the FIFTH
// measurement, after 4 full-step majors. The watchdog then restores the warm
// point itself -- x moves BACK by three accepted unit steps, which no other
// mechanism in this driver can do -- re-bases the funnel there, and the
// ordinary globalized method takes it the rest of the way.
//
// THE MODE LOSES ON THIS FIXTURE, and that is what makes it the right test:
// the run costs 23 majors where the funnel-gated one costs 21 and a cold solve
// costs 4. The claim under test is not that the rule always wins -- test (a)
// is where it wins -- but that when it does not, the SOLVE IS STILL CORRECT:
// the watchdog bounds the damage at kWarmResidualGrowthMax majors and hands
// back a point no worse than the one the mode started from.
TEST(WarmStart, WatchdogRestoresOnDivergence) {
    const auto p40 = make_hs(40);
    SqpOptions opts;
    SqpDriver seed_driver(opts);
    const SqpSolution seed = seed_driver.solve(*p40.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);

    WarmStart poisoned = seed.warm_start;
    poisoned.lambda_e = -poisoned.lambda_e; // THE POISON: negate lambda

    const auto p_cold = make_hs(40);
    SqpDriver cold_driver(opts);
    const SqpSolution cold = cold_driver.solve(*p_cold.model, p_cold.model->start_point());
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);

    const auto p_wd = make_hs(40);
    SqpDriver wd_driver(opts);
    const SqpSolution wd = wd_driver.solve(*p_wd.model, p_wd.model->start_point(), poisoned);

    EXPECT_EQ(wd.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(wd.counters.watchdog_restores, 1) << "THE PIN";
    EXPECT_EQ(wd.counters.full_step_majors, 4)
        << "the mode is cut short after exactly kWarmResidualGrowthMax consecutive growths -- a "
           "TRAJECTORY pin (observed 4 on this machine's MKL Pardiso build, Release and Debug "
           "alike, and independently reproduced by the Task-5 review): unlike the "
           "watchdog_restores/status/f assertions around it, this one reads a nonlinear run's "
           "step-by-step path and is the most likely of these pins to want per-backend treatment "
           "on Accelerate";
    EXPECT_LT(wd.counters.full_step_majors, wd.counters.major_iters)
        << "and the solve carried on, globalized, after the restore";
    EXPECT_EQ(wd.status, SqpStatus::kOptimal);
    EXPECT_NEAR(wd.f, cold.f, 1e-8);
    EXPECT_EQ(wd.counters.suspect_escalations, 0);

    // THE RESTORED POINT IS THE BEST ONE THE MODE SAW, checked where it is
    // visible: the row the watchdog re-measured (trial 4, the pass the mode
    // ended on) must describe the warm point, not the diverged iterate that
    // preceded it.
    ASSERT_GT(wd.history.size(), 4u);
    EXPECT_LT(wd.history[4].kkt_residual, wd.history[3].kkt_residual)
        << "the pass that ended the mode records the RESTORED iterate";
    EXPECT_DOUBLE_EQ(wd.history[4].kkt_residual, wd.history[0].kkt_residual)
        << "and the best iterate here is the warm point the mode started from";

    // The safety invariant again, on the run that took the watchdog exit: a
    // certified point must pass the standard check.
    const SqpKkt certified =
        evaluate_kkt(*p_wd.model, wd.x, wd.lambda_e, wd.lambda_i, opts.feas_tol);
    EXPECT_LE(certified.stationarity, opts.kkt_tol);
    EXPECT_LE(certified.feasibility, opts.feas_tol);

    // PHASE-4 TASK 7 (closing Task 5's F5 carry): row 4 is the ONLY one the
    // watchdog rebased (SqpCounters::watchdog_restores == 1, the pin above),
    // so it is the only row whose watchdog_restored flag should be set --
    // every other row, including the terminal stopped-AT-iterate one, must
    // read false.
    for (std::size_t i = 0; i < wd.history.size(); ++i) {
        EXPECT_EQ(wd.history[i].watchdog_restored, i == 4)
            << "row " << i << " (expected watchdog_restored == " << (i == 4) << ")";
    }

    // And the printer renders it: the WD column has a marker on trial 4's
    // row and nowhere else.
    const std::string table = format_iteration_table(wd);
    std::istringstream lines(table);
    std::string line;
    std::size_t data_row = 0;
    bool marker_seen_at_expected_row = false;
    while (std::getline(lines, line)) {
        if (line.empty() || line.find("Trial") != std::string::npos ||
            line.find("Status:") != std::string::npos ||
            line.find("Start Level:") != std::string::npos ||
            line.find_first_not_of('-') == std::string::npos) {
            continue;
        }
        const bool has_marker = line.find('*') != std::string::npos;
        EXPECT_EQ(has_marker, data_row == 4) << "table row " << data_row << ": " << line;
        if (has_marker && data_row == 4) {
            marker_seen_at_expected_row = true;
        }
        ++data_row;
    }
    EXPECT_TRUE(marker_seen_at_expected_row) << table;
}

// FIX ROUND 1 (F1): THE OTHER WATCHDOG EXIT -- THE STALL WINDOW. Both fixtures
// above take the DIVERGENCE exit (kWarmResidualGrowthMax consecutive growths);
// this one takes the exit the growth signal provably cannot catch.
//
// THE FIXTURE IS TASK 2's OWN DOCUMENTED 2-CYCLE, warm-started. HS5 at a
// radius of 2 with an accept-everything strategy is exactly the permanent
// 2-cycle MaxIterSolveEmitsValidWarmObjectAtBestKnownIterate (above) pins as
// burning a whole 20-major budget -- and under the full-step mode the funnel
// IS accept-everything, so the same cycle reappears without any caller-supplied
// strategy at all. The warm object is HS5's own converged one with two fields
// overwritten: `x` back to the published start point (a warm start that is
// merely a poor one, not a wrong one) and `tr_radius` to 2, which is the
// radius the cycle needs. Everything else -- duals, activity, funnel width,
// structure hash -- is the real solve's.
//
// WHY THE GROWTH SIGNAL CANNOT FIRE HERE, which is the whole point of having
// two: the residual ALTERNATES 3.50 | 1.4365, 2.3776, 1.4365, 2.3776, 1.4365,
// (2.3776) -- it never grows on two consecutive majors, so fs_growth_in_a_row
// resets on every second one and never reaches kWarmResidualGrowthMax = 2. What
// does fire is the window: 1.4365 is a new best at trial 1 and is only ever
// EQUALLED afterwards, never beaten (a repeat is not "<"), so
// fs_majors_since_best reaches kWarmFullStepWindow = 5 on the measurement after
// trial 5. The restore then lands on that best iterate, the funnel takes over,
// rejects once at Delta = 2, halves to 1 and converges.
//
// SO THE CYCLE ROUTE IS WHAT THIS ACHIEVES, not the routed-QP-failure route the
// Task-5 report also argued would burn the window. That argument stands
// unexecuted (its trace is in sqp_driver.h's INTERACTION WITH THE SUSPECT GATE
// paragraph); a run of routed failures is hard to sustain for five majors
// because the driver's retry is one-shot per chain and a second CONSECUTIVE
// failure ends the solve, so the window would have to be filled by
// non-adjacent failures interleaved with successful-but-unimproving majors.
TEST(WarmStart, WatchdogStallExitBreaksAFullStepCycle) {
    SqpOptions opts;
    opts.tr_init = 2.0;
    opts.max_iter = 40;

    const auto p5 = make_hs(5);
    SqpDriver seed_driver(opts);
    const SqpSolution seed = seed_driver.solve(*p5.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);

    WarmStart cycling = seed.warm_start;
    cycling.x = p5.model->start_point(); // a POOR warm point, not a wrong one
    cycling.tr_radius = 2.0;             // the radius Task 2's 2-cycle needs

    const auto p_run = make_hs(5);
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p_run.model, p_run.model->start_point(), cycling);

    ASSERT_EQ(sol.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(sol.counters.watchdog_restores, 1) << "THE PIN -- the stall exit fired";
    EXPECT_EQ(sol.counters.full_step_majors, kWarmFullStepWindow + 1)
        << "the window is measured on the FIRST major and closes on the sixth measurement, so "
           "exactly kWarmFullStepWindow + 1 majors run under the mode";
    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_EQ(sol.counters.suspect_escalations, 0);

    // THE CYCLE ITSELF, pinned so the fixture cannot silently stop cycling and
    // leave this test asserting the watchdog on a run that never needed it.
    ASSERT_GT(sol.history.size(), 6u);
    EXPECT_DOUBLE_EQ(sol.history[3].kkt_residual, sol.history[1].kkt_residual);
    EXPECT_DOUBLE_EQ(sol.history[5].kkt_residual, sol.history[1].kkt_residual);
    EXPECT_DOUBLE_EQ(sol.history[4].kkt_residual, sol.history[2].kkt_residual);
    EXPECT_GT(sol.history[2].kkt_residual, sol.history[1].kkt_residual)
        << "the cycle's other leg is WORSE, so this is a stall and not a plateau of equals";

    // ... and the growth signal never reaching 2 is what makes the window
    // load-bearing: no two CONSECUTIVE majors of the mode both grow.
    for (std::size_t k = 1; k + 1 < 6; ++k) {
        const bool grew_here = sol.history[k].kkt_residual > sol.history[k - 1].kkt_residual;
        const bool grew_next = sol.history[k + 1].kkt_residual > sol.history[k].kkt_residual;
        EXPECT_FALSE(grew_here && grew_next)
            << "two consecutive growths at k=" << k << " would have fired the DIVERGENCE exit "
            << "instead, and this fixture would stop testing the window";
    }

    // The restore landed on the best iterate the mode saw (trial 1's leg of the
    // cycle), not on the worse leg the mode was standing on.
    EXPECT_DOUBLE_EQ(sol.history[6].kkt_residual, sol.history[1].kkt_residual);

    // And the solve is correct: HS5's own optimum, as the gated run finds it.
    SqpOptions gated_opts = opts;
    gated_opts.warm_full_step = false;
    const auto p_gated = make_hs(5);
    SqpDriver gated_driver(gated_opts);
    const SqpSolution gated =
        gated_driver.solve(*p_gated.model, p_gated.model->start_point(), cycling);
    ASSERT_EQ(gated.status, SqpStatus::kOptimal);
    EXPECT_EQ(gated.counters.watchdog_restores, 0);
    EXPECT_NEAR(sol.f, gated.f, 1e-8);

    // The safety invariant, on the stall exit too.
    const SqpKkt certified =
        evaluate_kkt(*p_run.model, sol.x, sol.lambda_e, sol.lambda_i, opts.feas_tol);
    EXPECT_LE(certified.stationarity, opts.kkt_tol);
    EXPECT_LE(certified.feasibility, opts.feas_tol);
}

// FIX ROUND 1 (F3): THE WATCHDOG'S Eq.-(13) RE-BASE IS CLAMPED, AND HERE IS A
// RUN WHERE THE CLAMP BINDS.
//
// THE HAZARD. Eq. (13) gives tau_+ = (1-kappa)h + kappa*tau, which is <= tau
// IFF h <= tau. The watchdog restores the best iterate BY ||KKT||inf, an
// inf-norm measure that says nothing about that point's l1 violation, so an
// unclamped re-base can WIDEN the funnel mid-solve -- and the monotonically
// non-increasing width is exactly what KLV Thm. 1 case 1 sums.
//
// WHY THE WIDTH IS OBSERVABLE HERE, which is what makes this testable at all:
// NO VERDICT THE MODE RETURNS MUTATES tau (globalization.h), so from the
// Task-3 ingest until the watchdog the width is FROZEN at its ingested value,
// which this test re-derives from the published constants and h0. Stopping the
// solve on the very pass the watchdog fires (max_iter == the number of
// full-step majors) then leaves warm_start.funnel_width holding the
// POST-RESTORE width and nothing else.
//
// THE FIXTURE. HS7 at a radius of 5, warm from its own solve with `x` set back
// to the published start point (h0 = 25, so the ingest lands tau at 65.625) and
// lambda_e scaled by -500. The mode's unit steps wander to h ~ 3.3e2 while the
// residual falls, so the BEST iterate by ||KKT||inf carries h = 3.30e2 -- five
// times the width. Unclamped, the re-base would put tau at
// 0.5*329.69 + 0.5*65.625 = 197.66 (MEASURED by removing the clamp); clamped,
// it stays at 65.625.
//
// This configuration is rare: a sweep of 3240 (problem, radius, dual poison,
// warm-x) combinations over all 27 shipped HS problems produced 330 watchdog
// restores of which 6 bind, on 3 distinct fixtures. It is a guard against a
// class, not a fix for a failure any shipped fixture was hitting.
TEST(WarmStart, WatchdogRebaseNeverWidensTheFunnel) {
    SqpOptions seed_opts;
    seed_opts.tr_init = 5.0;
    const auto p7 = make_hs(7);
    SqpDriver seed_driver(seed_opts);
    const SqpSolution seed = seed_driver.solve(*p7.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);

    WarmStart w = seed.warm_start;
    w.lambda_e *= -500.0;
    w.x = p7.model->start_point();
    w.tr_radius = 5.0;

    // The budget is the number of full-step majors this fixture runs, so the
    // solve stops on the pass the watchdog fires and the exit width is the
    // one the re-base just wrote.
    SqpOptions opts;
    opts.tr_init = 5.0;
    opts.max_iter = 8;

    const auto p_run = make_hs(7);
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p_run.model, p_run.model->start_point(), w);

    ASSERT_EQ(sol.counters.start_level_used, StartLevel::kWarm);
    ASSERT_EQ(sol.counters.watchdog_restores, 1) << "the fixture must reach the watchdog";
    ASSERT_EQ(sol.counters.full_step_majors, 8);
    ASSERT_EQ(sol.status, SqpStatus::kMaxIter) << "stopped ON the watchdog pass, by construction";

    // The ingested width, re-derived exactly as sqp_driver.h's Task-3 ingest
    // computes it: reset() by Eq. (9), then one Eq. (13) blend against the
    // remembered width floored at kappa_bar * h0.
    ASSERT_FALSE(sol.history.empty());
    const double h0 = sol.history[0].violation_l1;
    const double tau_reset = std::max(kFunnelTauBar, kFunnelKappaBar * h0);
    const double tau_ingest =
        (1.0 - detail::kFunnelKappa) * std::max(w.funnel_width, kFunnelKappaBar * h0) +
        detail::kFunnelKappa * tau_reset;

    // THE CLAMP BINDS ON THIS RUN: the restored point's own h is above the
    // width the mode froze, so an unclamped Eq. (13) would have widened.
    const double h_restored = sol.history.back().violation_l1;
    ASSERT_GT(h_restored, tau_ingest)
        << "this fixture only tests the clamp while the restore lands outside the funnel";
    EXPECT_GT((1.0 - detail::kFunnelKappa) * h_restored + detail::kFunnelKappa * tau_ingest,
              tau_ingest)
        << "and the unclamped update really is a widening -- stated as arithmetic, not asserted "
           "about the code";

    // THE PIN: Thm-1 monotonicity survives the watchdog. Removing the clamp
    // measures 197.657 here against a 65.625 ingest.
    EXPECT_LE(sol.warm_start.funnel_width, tau_ingest)
        << "the watchdog's re-base must never widen the funnel -- THE PIN";
    EXPECT_DOUBLE_EQ(sol.warm_start.funnel_width, tau_ingest)
        << "and with the clamp binding, Eq. (13) degenerates to tau_+ = tau exactly";
}

// FIX ROUND 1 (F2): THE DOWNSTREAM MACHINERY, EXECUTED RATHER THAN ARGUED.
// sqp_driver.h's FULL-STEP-FIRST WARM RULE note claims the elastic tier, the
// restoration phase and the failure routing are INHERITED by the mode rather
// than re-implemented -- true by construction (the mode only changes a
// verdict), but until this test nothing in the suite drove the mode into any
// of them. The Task-5 review's own probe found the case and it is reproduced
// here verbatim: HS40 with lambda_e scaled by -10 (a harder poison than
// WatchdogRestoresOnDivergence's -1) walks the mode through a watchdog restore,
// then a rejection, then a linearization the elastic tier cannot close, then
// the restoration phase -- and still lands on HS40's own optimum.
//
// WHAT IS PINNED IS THE INTERPLAY, not the trajectory: that each mechanism
// fired at all (>= 1 activation / restoration major), that the mode ended
// exactly once, and that the answer is right. The two counters that ARE pinned
// exactly (full_step_majors, watchdog_restores) carry the same
// machine/backend caveat as WatchdogRestoresOnDivergence's -- observed on this
// machine's MKL Pardiso build in Release and Debug, and independently by the
// review.
TEST(WarmStart, FullStepModeInheritsElasticAndRestoration) {
    SqpOptions opts;

    const auto p40 = make_hs(40);
    SqpDriver seed_driver(opts);
    const SqpSolution seed = seed_driver.solve(*p40.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);

    WarmStart poisoned = seed.warm_start;
    poisoned.lambda_e *= -10.0;

    const auto p_cold = make_hs(40);
    SqpDriver cold_driver(opts);
    const SqpSolution cold = cold_driver.solve(*p_cold.model, p_cold.model->start_point());
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);

    const auto p_run = make_hs(40);
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p_run.model, p_run.model->start_point(), poisoned);

    ASSERT_EQ(sol.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_NEAR(sol.f, cold.f, 1e-6)
        << "the restoration phase moves the iterate, so this lands on HS40's optimum to the "
           "phase's own accuracy rather than to the 1e-8 the un-restored runs hit";

    // THE INTERPLAY -- each mechanism reached, none of them re-implemented.
    EXPECT_EQ(sol.counters.watchdog_restores, 1) << "the mode ended once, through the watchdog";
    EXPECT_EQ(sol.counters.full_step_majors, 6);
    EXPECT_GE(sol.counters.elastic_activations, 1)
        << "a full-step trajectory reached a linearization the plain QP called infeasible";
    EXPECT_GE(sol.counters.restoration_iters, 1)
        << "and the elastic tier's exhaustion raised a restoration request the driver serviced";
    EXPECT_FALSE(sol.infeasibility_certified) << "HS40 is feasible; the phase RESUMED";
    EXPECT_EQ(sol.counters.suspect_escalations, 0);

    // The restoration happened AFTER the mode had already ended: the mode
    // cannot be armed across a restoration resume (both flags are cleared
    // there), so no row after the kRestore may be a full-step one. Checked
    // through the counters, which is the only place it is visible: every
    // full-step major precedes the restore, so the mode's own count cannot
    // exceed the index of the kRestore row.
    std::size_t restore_row = sol.history.size();
    for (std::size_t k = 0; k < sol.history.size(); ++k) {
        if (sol.history[k].verdict == StepVerdict::kRestore) {
            restore_row = k;
            break;
        }
    }
    ASSERT_LT(restore_row, sol.history.size()) << "the fixture must actually reach a kRestore row";
    EXPECT_LE(static_cast<std::size_t>(sol.counters.full_step_majors), restore_row);

    const SqpKkt certified =
        evaluate_kkt(*p_run.model, sol.x, sol.lambda_e, sol.lambda_i, opts.feas_tol);
    EXPECT_LE(certified.stationarity, opts.kkt_tol);
    EXPECT_LE(certified.feasibility, opts.feas_tol);
}

// (c) THE A/B LEVER OFF REPRODUCES TASK 3 EXACTLY. Both of Task 3's own warm
// fixtures are re-run here with warm_full_step = false and compared against
// the counts pinned in WarmMatchesColdOnIdenticalProblem and
// PerturbedWarmBeatsCold above -- the point being that turning the lever off
// is a genuine restoration of the earlier behaviour and not merely a similar
// one. (Those two tests themselves run with the lever at its DEFAULT, i.e.
// ON, and still pass: on that family the mode takes the same steps the funnel
// accepted anyway -- see this section's own fixture note.)
TEST(WarmStart, FullStepDisabledMatchesTask3) {
    SqpOptions opts;
    opts.warm_full_step = false;

    const auto p7 = make_hs(7);
    SqpDriver driver7(opts);
    const SqpSolution sol7 = driver7.solve(*p7.model);
    ASSERT_EQ(sol7.status, SqpStatus::kOptimal);
    EXPECT_EQ(sol7.counters.major_iters, 9) << "Task 3's cold pin";
    EXPECT_EQ(sol7.counters.full_step_majors, 0) << "a cold solve can never engage the mode";

    // WarmMatchesColdOnIdenticalProblem's pins.
    const auto p_same = make_hs(7);
    SqpDriver same_driver(opts);
    const SqpSolution same =
        same_driver.solve(*p_same.model, p_same.model->start_point(), sol7.warm_start);
    ASSERT_EQ(same.status, SqpStatus::kOptimal);
    EXPECT_EQ(same.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(same.counters.major_iters, 0);
    EXPECT_EQ(same.counters.full_step_majors, 0);
    EXPECT_EQ(same.counters.watchdog_restores, 0);

    // PerturbedWarmBeatsCold's pins.
    Hs7PerturbedModel perturbed(1.0e-3);
    SqpDriver cold_driver(opts);
    const SqpSolution cold = cold_driver.solve(perturbed, perturbed.start_point());
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    EXPECT_EQ(cold.counters.major_iters, 9);

    SqpDriver warm_driver(opts);
    const SqpSolution warm = warm_driver.solve(perturbed, perturbed.start_point(), sol7.warm_start);
    ASSERT_EQ(warm.status, SqpStatus::kOptimal);
    EXPECT_EQ(warm.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(warm.counters.major_iters, 1);
    EXPECT_EQ(warm.counters.full_step_majors, 0);
    EXPECT_EQ(warm.counters.watchdog_restores, 0);
}

// =============================================================================
// PHASE-4 TASK 6: budgeted mode -- bounded-iteration solves that return a
// usable iterate and a warm handoff for a continuation driver.
// =============================================================================
//
// THE FIXTURE, AND WHY IT IS HS26 FROM A CUSTOM START RATHER THAN HS26'S OWN
// PUBLISHED ONE. HS26 is the brief's named slow-converging fixture (17
// majors from its own start, measured; see hs_problems.h's own note on its
// singular Hessian at the solution). Its PUBLISHED start (-2.6, 2, 2)
// happens, by coincidence of that literature value, to already satisfy
// HS26's one equality constraint EXACTLY (h0 = 0) -- which would make the
// funnel's own ordering (min h, tie-break min f) trivially pick the start
// point on every budgeted exit before genuine convergence, since h can never
// go below 0 and this fixture's h never returns to bit-exact 0 once it
// leaves. That would test nothing but the degenerate case. x0 = (-2, 2, 2)
// (nudged off the manifold, still HS26, via the 2-arg solve(model, x0)
// overload) keeps the slow tail and gives h a genuine trajectory to order:
// MEASURED, h goes 3 -> 0.0625 -> 0.581827 (a rise) -> 0.0715955 -> ...,
// non-monotone early and strictly decreasing from there, so the
// min-h/tie-f iterate is neither the first nor the last point at every
// truncation -- exactly what is needed to demonstrate the ordering rather
// than assume it.

// (a) THE BRIEF'S OWN TEST. max_iter = 3 (three subproblems solved) leaves
// the loop stopped AT the 4th iterate (history rows 0-3, the last unsolved --
// SqpCounters' own "stopped AT an iterate" shape). MEASURED trajectory over
// those four rows:
//     trial   f            h            KKT
//     0       16           3            8
//     1       4.0625       0.0625       4
//     2       0.01234568   0.581827     0.581827
//     3       0.002438653  0.0715955    0.0715955
// The min-h row is trial 1 (h = 0.0625), NOT trial 3 (the last iterate, and
// also the min-KKT row) -- the two orderings this task and Task 5's watchdog
// use genuinely disagree on this fixture, which is exactly the case
// sqp_types.h's SqpOptions::budget_mode note argues for. Budgeted mode must
// report trial 1's own (x, f), not trial 3's.
TEST(WarmStart, BudgetReturnsUsableIterate) {
    const auto p = make_hs(26);
    Vec x0(3);
    x0 << -2.0, 2.0, 2.0;

    SqpOptions opts;
    opts.max_iter = 3;
    opts.budget_mode = true;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model, x0);

    ASSERT_EQ(sol.status, SqpStatus::kBudgetExhausted);
    EXPECT_EQ(sol.counters.major_iters, 3);
    ASSERT_EQ(sol.history.size(), 4u);
    EXPECT_EQ(sol.counters.suspect_escalations, 0)
        << "the Accelerate standing rule: this fixture must not reach the suspect gate";

    // Recompute the funnel's own ordering (min h, tie-break min f) over the
    // trajectory just recorded, independent of the driver's own tracking --
    // this is exactly what a caller checking "did I get the best one" would
    // do, per the brief's "assert against history".
    std::size_t best = 0;
    for (std::size_t k = 1; k < sol.history.size(); ++k) {
        const SqpIterate &a = sol.history[k];
        const SqpIterate &b = sol.history[best];
        if (a.violation_l1 < b.violation_l1 || (a.violation_l1 == b.violation_l1 && a.f < b.f)) {
            best = k;
        }
    }
    ASSERT_EQ(best, 1u) << "THE PIN: the best-by-(h, f) row is trial 1, neither the first nor the "
                           "last -- see this test's own fixture note";
    EXPECT_LT(sol.history[best].violation_l1, sol.history.back().violation_l1)
        << "and it beats the last iterate's own h, which is what makes it worth reporting instead";
    EXPECT_NE(best, sol.history.size() - 1)
        << "the mutation this test exists to catch: reporting the LAST iterate instead of the "
           "best one would make this a vacuous no-op fixture";

    // sol.f is bit-identical to that row's own f (both are the SAME ev.f,
    // never recomputed -- the same invariant
    // MaxIterSolveEmitsValidWarmObjectAtBestKnownIterate checks for kMaxIter).
    EXPECT_DOUBLE_EQ(sol.f, sol.history[best].f);
    EXPECT_NEAR(sol.f, 4.0625, 1e-6);

    // h and the KKT residual AT THE RETURNED x, independently re-derived from
    // the model rather than trusted from the driver's own bookkeeping, must
    // match that same row.
    const NlpEval ev_at_x = eval_nlp(*p.model, sol.x);
    EXPECT_DOUBLE_EQ(constraint_violation_l1(ev_at_x), sol.history[best].violation_l1);
    const SqpKkt kkt_at_x =
        evaluate_kkt(*p.model, sol.x, sol.lambda_e, sol.lambda_i, opts.feas_tol);
    EXPECT_DOUBLE_EQ(kkt_at_x.residual(), sol.history[best].kkt_residual);

    // THE PIN this test would otherwise pass vacuously without: the returned
    // point is NOT the min-KKT one (trial 3's KKT = 0.0715955, strictly
    // smaller than trial 1's 4) -- the two orderings really do disagree here.
    EXPECT_GT(sol.history[best].kkt_residual, sol.history.back().kkt_residual);

    EXPECT_TRUE(sol.warm_start.valid);
    ASSERT_EQ(sol.warm_start.x.size(), sol.x.size());
    EXPECT_EQ(sol.warm_start.x, sol.x);
    EXPECT_EQ(sol.warm_start.lambda_e, sol.lambda_e);
    EXPECT_EQ(sol.warm_start.lambda_i, sol.lambda_i);
}

// (b) CHAINED BUDGETED SOLVES. Three successive budget_mode solves, each
// seeded from the previous one's own warm_start, on the SAME fixture as (a)
// (max_iter = 6 per round rather than 3, so the chain has room to make
// progress each round instead of only ever re-discovering the same best
// iterate). MEASURED: round 1 (cold, 2-arg overload) and round 2 (warm) both
// exhaust their 6-major budget and report kBudgetExhausted; round 3 (warm)
// converges in 4 majors. Total majors across all three: 6 + 6 + 4 = 16 --
// IDENTICAL to a single unbudgeted solve of the same fixture from the same
// x0 (also measured at 16), i.e. this fixture's own budget slicing costs
// NO majors beyond what one continuous solve would have spent -- the
// re-entry overhead this fixture happens to pin is exactly zero.
TEST(WarmStart, ChainedBudgetSolvesReachOptimal) {
    const auto p = make_hs(26);
    Vec x0(3);
    x0 << -2.0, 2.0, 2.0;

    SqpOptions opts;
    opts.budget_mode = true;
    opts.max_iter = 6;

    SqpDriver d1(opts);
    const SqpSolution s1 = d1.solve(*p.model, x0);
    ASSERT_EQ(s1.status, SqpStatus::kBudgetExhausted);
    EXPECT_EQ(s1.counters.major_iters, 6);
    EXPECT_EQ(s1.counters.start_level_used, StartLevel::kCold)
        << "the 2-arg overload is always cold by construction";
    EXPECT_EQ(s1.counters.suspect_escalations, 0);
    ASSERT_TRUE(s1.warm_start.valid);

    SqpDriver d2(opts);
    const SqpSolution s2 = d2.solve(*p.model, s1.warm_start.x, s1.warm_start);
    ASSERT_EQ(s2.status, SqpStatus::kBudgetExhausted);
    EXPECT_EQ(s2.counters.major_iters, 6);
    EXPECT_EQ(s2.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(s2.counters.suspect_escalations, 0);
    ASSERT_TRUE(s2.warm_start.valid);

    SqpDriver d3(opts);
    const SqpSolution s3 = d3.solve(*p.model, s2.warm_start.x, s2.warm_start);
    ASSERT_EQ(s3.status, SqpStatus::kOptimal) << "THE PIN: the third round finally converges";
    EXPECT_EQ(s3.counters.major_iters, 4);
    EXPECT_EQ(s3.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(s3.counters.suspect_escalations, 0);

    const Index total_majors =
        s1.counters.major_iters + s2.counters.major_iters + s3.counters.major_iters;
    EXPECT_EQ(total_majors, 16) << "THE PIN: total majors across the chain";

    // The reference: one CONTINUOUS unbudgeted solve of the same fixture from
    // the same x0. The chain must not cost more majors than this -- "budget
    // slicing must not cost extra majors beyond re-entry overhead" -- and
    // this fixture's own overhead is measured at exactly zero.
    SqpOptions cold_opts;
    SqpDriver cold_driver(cold_opts);
    const SqpSolution cold = cold_driver.solve(*p.model, x0);
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    EXPECT_EQ(cold.counters.major_iters, 16);
    EXPECT_EQ(total_majors, cold.counters.major_iters)
        << "this fixture's re-entry overhead is measured at zero -- the chain costs no more than "
           "one continuous solve";

    const SqpKkt certified = evaluate_kkt(*p.model, s3.x, s3.lambda_e, s3.lambda_i, opts.feas_tol);
    EXPECT_LE(certified.stationarity, opts.kkt_tol);
    EXPECT_LE(certified.feasibility, opts.feas_tol);
    EXPECT_NEAR(s3.f, cold.f, 1e-8);
}

// (c) budget_mode = false is BYTE-IDENTICAL to Phase 3 on the same fixture
// and the same truncation as (a): kMaxIter, reported at the LAST iterate
// (trial 3), not the best-by-(h, f) one.
TEST(WarmStart, BudgetModeOffMatchesMaxIterExactly) {
    const auto p = make_hs(26);
    Vec x0(3);
    x0 << -2.0, 2.0, 2.0;

    SqpOptions opts;
    opts.max_iter = 3;
    ASSERT_FALSE(opts.budget_mode) << "default is off";

    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model, x0);

    ASSERT_EQ(sol.status, SqpStatus::kMaxIter);
    EXPECT_EQ(sol.counters.major_iters, 3);
    ASSERT_EQ(sol.history.size(), 4u);

    // The LAST row (trial 3), not the min-h row (a)'s solve picks out.
    EXPECT_DOUBLE_EQ(sol.f, sol.history.back().f);
#ifdef USE_ACCELERATE_SPARSE
    // Origin divergence entry D15. The note that carried it
    // (docs/notes/2026-07-30-accelerate-at-head-results.md) did NOT migrate
    // into hven, and the register that succeeds it here
    // (docs/notes/2026-08-14-accelerate-divergence-register.md, "Why this
    // file exists at this path") has no row for D15 -- the observation is
    // quoted in full below, and this comment is its citable record here.
    // What was observed: the f-pin
    // below is MKL-measured. On Accelerate, three truncated majors on HS26
    // (rank-deficient at the solution) amplify last-digit linear-algebra
    // differences into ~8.69e-12 of f -- the 1e-9 x-pin and every structural
    // assertion in this test (kMaxIter, 3 majors, history size 4, and the
    // sol.f == sol.history.back().f claim above, the one the test exists for)
    // hold bitwise on both backends. Value is Accelerate-observed (macOS
    // 26.5.2 / AppleClang 21) and will be re-verified on the next Mac
    // session.
    EXPECT_NEAR(sol.f, 0.0024386526444139557, 1e-12);
#else
    EXPECT_NEAR(sol.f, 0.0024386526357234916, 1e-12);
#endif
    Vec x_star(3);
    x_star << 0.88391769091059114, 0.88391769091059103, 1.1061399129348335;
    EXPECT_LT((sol.x - x_star).norm(), 1e-9);
}

// =============================================================================
// PHASE-4 TASK 12: from_interior_point -- the interior-point crossover
// (warm_start.h). THE KNITRO CROSSOVER PATTERN: an IP method runs to
// near-KKT, then hands off to an active-set method (this driver) for the
// final polish -- in tycho this is exactly how an IPM solve would seed
// this driver.
// =============================================================================

// (a) HS14 -- ONE ACTIVE INEQUALITY, no finite bounds. An IP-like solution is
// synthesized from HS14's own converged point: x perturbed by 1e-7, and the
// active row's `slack_i` set to the mu/lambda complementarity residue
// (mu = 1e-9, slack_i = -mu/lambda_i -- see from_interior_point's own note on
// why `slack_i` is the cI(x) VALUE, i.e. the NEGATED IP slack).
//
// THE (e) FINDING, PINNED THROUGH PHASE 5 AND **INVERTED BY PHASE-6 TASK 5**.
// from_interior_point has no model to hash, so `structure_hash == 0`
// (unconditionally, by construction), and sqp_driver.h's ingest treats hash == 0
// exactly like a mismatch, never a match. THROUGH PHASE 5 that resolved
// `driver.solve(model, crossover.x, crossover)` to StartLevel::kCold, `x` was
// read from the CALLER's own `x0` argument rather than `warm.x`, and this test
// pinned the honest consequence: the 3-arg call was BYTE-IDENTICAL to the 2-arg
// `solve(model, crossover.x)`, i.e. all the crossover bought was a good x0 a
// caller read off the object and passed explicitly.
//
// A HASH MISMATCH NOW RESOLVES `StartLevel::kSeeded`, which takes the values
// without asking about provenance (warm_start.h's StartLevel note), so the
// duals and the activity hint reach the solve and the two calls are NO LONGER
// identical -- which is the whole point of the level. This test keeps both arms
// and now pins the DIFFERENCE, measured:
//
//   arm            level    majors   f
//   3-arg (seeded) kSeeded  0        1.3934652338252069
//   2-arg (x only) kCold    1        1.3934649806892776
//
// THE ZERO-MAJOR ARM IS A LEGITIMATE CERTIFICATE, NOT A B-1 DEFECT, and the
// distinction is exactly the one Phase-5 Task 7b drew: the IP point here is
// x* - 1e-7 carrying HS14's own exact duals on a row that is geometrically
// active (slack -mu/lambda, mu = 1e-9), so it IS a KKT point of the posed
// problem to within kkt_tol and the driver is entitled to say so for free. The
// f gap between the arms, 2.5e-7, is the distance between "converged to within
// kkt_tol at the IP point" and "took one more QP step" -- both inside tolerance,
// and the seeded arm's is the one the crossover asked for. The B-1 clear did not
// fire because no row is slack; O-1's legitimate-zero-major feature is what
// permits the exit, and tests/test_b1_gate.cpp's seeded twin of
// AZeroMajorWarmSolveWithALiveInequalityPriceStillCertifies guards it directly.
TEST(WarmStart, CrossoverRecoversExactSolveHs14) {
    const auto p = make_hs(14);
    SqpOptions opts;
    SqpDriver seed_driver(opts);
    const SqpSolution seed = seed_driver.solve(*p.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);
    ASSERT_EQ(p.model->mi(), 1);
    ASSERT_GT(seed.lambda_i(0), 0.0) << "HS14's own inequality is active at its optimum";

    const double mu = 1.0e-9;
    Vec slack_i(1);
    slack_i(0) = -mu / seed.lambda_i(0); // s_i = mu/lambda_i; slack_i (cI value) = -s_i

    const Vec x_ip = seed.x - Vec::Constant(p.model->n(), 1.0e-7);
    const Vec z_lower = Vec::Zero(p.model->n());
    const Vec z_upper = Vec::Zero(p.model->n());

    const WarmStart crossover =
        from_interior_point(x_ip, seed.lambda_e, seed.lambda_i, slack_i, z_lower, z_upper,
                            p.model->lower(), p.model->upper());

    EXPECT_TRUE(crossover.valid);
    EXPECT_EQ(crossover.structure_hash, 0u);
    EXPECT_EQ(crossover.hot, nullptr);
    EXPECT_DOUBLE_EQ(crossover.funnel_width, -1.0);
    EXPECT_DOUBLE_EQ(crossover.tr_radius, -1.0);
    EXPECT_DOUBLE_EQ(crossover.primal_delta, -1.0);
    EXPECT_DOUBLE_EQ(crossover.dual_mu, -1.0);
    ASSERT_EQ(crossover.ineq_active.size(), 1u);
    EXPECT_EQ(crossover.ineq_active[0], 1) << "the mu/lambda residue must read as active";
    EXPECT_EQ(crossover.qp_working_set.active_ineq(), (std::vector<Index>{0}));
    EXPECT_EQ(crossover.bound_active, (std::vector<std::int8_t>{0, 0}))
        << "HS14 has no finite bounds";

    const auto p_warm = make_hs(14);
    SqpDriver warm_driver(opts);
    const SqpSolution warm_sol = warm_driver.solve(*p_warm.model, crossover.x, crossover);
    ASSERT_EQ(warm_sol.status, SqpStatus::kOptimal);
    EXPECT_EQ(warm_sol.counters.major_iters, 0)
        << "THE PIN (observed, Phase-6 Task 5): the seeded duals certify the IP point outright";
    ASSERT_EQ(warm_sol.warm_start.ineq_active.size(), 1u);
    EXPECT_EQ(warm_sol.warm_start.ineq_active[0], 1)
        << "the certified exit active set matches HS14's own known active row -- exact. Note this "
           "is attributed from the INGESTED SEED, not from a QpSolution: no subproblem ran";

    // THE (e) FINDING, INVERTED: hash == 0 no longer forces kCold, it forces
    // kSeeded -- the level that takes values, not provenance.
    EXPECT_EQ(warm_sol.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(warm_sol.counters.n_seeded, 1);
    EXPECT_EQ(warm_sol.counters.seeded_clamped, 0)
        << "this crossover's own duals are HS14's exact, strictly positive ones";

    const auto p_xonly = make_hs(14);
    SqpDriver xonly_driver(opts);
    const SqpSolution xonly_sol = xonly_driver.solve(*p_xonly.model, crossover.x);
    ASSERT_EQ(xonly_sol.status, SqpStatus::kOptimal);
    EXPECT_EQ(xonly_sol.counters.start_level_used, StartLevel::kCold)
        << "the 2-arg overload is cold by construction -- this is the control arm";
    EXPECT_EQ(xonly_sol.counters.major_iters, 1) << "THE PIN (observed)";
    EXPECT_LT(warm_sol.counters.major_iters, xonly_sol.counters.major_iters)
        << "crossover-WITH-duals vs crossover-x-ONLY: THE (e) PIN, inverted. Through Phase 5 these "
           "were IDENTICAL because the duals could not reach the driver at all; kSeeded is what "
           "lets them, and this is the counted benefit that buys.";
    // Both arms land on the same answer to within the tolerance each was
    // entitled to stop at -- the seeded arm is not cheaper by being wrong.
    EXPECT_LT((xonly_sol.x - warm_sol.x).cwiseAbs().maxCoeff(), 1e-6);
    EXPECT_NEAR(xonly_sol.f, warm_sol.f, 1e-6);
}

// (a') F1BoxQp at p = 0.1 (branch L: x2 sits at its UPPER bound, the general
// row strictly INACTIVE with a zero multiplier -- parametric_families.h's own
// DEGENERACY note) -- the BOUND-activity branch HS14 above cannot exercise
// (HS14 has no finite bounds at all). Also exercises `slack_i` on an
// INACTIVE row: away from zero, no mu/lambda residue involved (lambda_i == 0
// there identically, so no complementarity residue is even defined).
TEST(WarmStart, CrossoverRecoversExactSolveF1) {
    const double pv = 0.1;
    F1BoxQp model(pv);
    ASSERT_LT(pv, F1BoxQp::kPLow) << "branch L, by construction of this fixture";

    const Vec x_star = F1BoxQp::x_star(pv);
    const Vec lambda_e_star = F1BoxQp::lambda_e_star(pv);
    const Vec lambda_i_star = F1BoxQp::lambda_i_star(pv);
    const Vec z_star = F1BoxQp::z_star(pv);
    const auto active = F1BoxQp::active_set(pv);
    ASSERT_EQ(active.bound_active[1], 1) << "x2 at its upper bound on branch L";
    ASSERT_EQ(active.ineq_active[0], 0) << "the row is strictly inactive on branch L";
    ASSERT_DOUBLE_EQ(lambda_i_star(0), 0.0);

    const Vec x_ip = x_star - Vec::Constant(model.n(), 1.0e-7);
    Vec slack_i(1);
    slack_i(0) = model.eval_ci(x_star)(0); // inactive row: the real, away-from-zero cI value
    Vec z_lower = Vec::Zero(model.n());
    Vec z_upper = Vec::Zero(model.n());
    z_upper(1) = -z_star(1); // z_star(1) < 0 (active upper); IP-style z_upper is >= 0

    const WarmStart crossover = from_interior_point(x_ip, lambda_e_star, lambda_i_star, slack_i,
                                                    z_lower, z_upper, model.lower(), model.upper());

    EXPECT_TRUE(crossover.valid);
    EXPECT_EQ(crossover.structure_hash, 0u);
    ASSERT_EQ(crossover.bound_active.size(), 2u);
    EXPECT_EQ(crossover.bound_active[0], 0);
    EXPECT_EQ(crossover.bound_active[1], 1)
        << "the near-zero gap at the upper bound, against a real z, reads as active";
    ASSERT_EQ(crossover.ineq_active.size(), 1u);
    EXPECT_EQ(crossover.ineq_active[0], 0) << "lambda_i == 0 fails the dual_tol gate";
    EXPECT_EQ(crossover.qp_working_set.bound_state()[1], BoundState::kAtUpper);

    F1BoxQp model_warm(pv);
    SqpOptions opts;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model_warm, crossover.x, crossover);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_NEAR(sol.f, F1BoxQp::f_star(pv), 1e-6);
    EXPECT_LT((sol.x - x_star).norm(), 1e-6) << "F1's own analytic optimum, recovered";

    // MEASURED: this fixture converges in EXACTLY 0 majors -- x0 already
    // satisfies the driver's own KKT test before any subproblem is built
    // (the same "immediate convergence" case WarmMatchesColdOnIdenticalProblem
    // above exercises, here reached from the crossover's x0 rather than an
    // exact warm x).
    ASSERT_EQ(sol.counters.major_iters, 0);
    EXPECT_EQ(sol.counters.start_level_used, StartLevel::kSeeded)
        << "PHASE-6 TASK 5: a crossover object's hash == 0 resolves kSeeded, not kCold";

    // THE ACTIVITY HAND-OFF ON A ZERO-MAJOR EXIT -- **THIS ASSERTION FLIPPED IN
    // PHASE-6 TASK 5**, and it flipped for a reason worth stating rather than
    // re-pinning silently.
    //
    // THROUGH PHASE 5 this read `{0, 0}` with the note: "activity can only be
    // attributed from a QpSolution a subproblem actually produced, so a solve
    // that never builds one reports every entry FREE regardless of the point's
    // true geometric position". That was correct and remains correct as far as
    // it goes -- but it rested on there being NO ingested seed either, which
    // was guaranteed only because the crossover resolved kCold. It now resolves
    // kSeeded, so the driver holds the crossover's OWN activity hint and
    // make_warm_start attributes the exit from that seed, exactly as it does on
    // a zero-major kWarm exit. The emitted `bound_active[1] == 1` is therefore
    // the crossover's own (already checked, correct) reading of x2 sitting at
    // its upper bound, carried through a solve that had no reason to disagree
    // with it -- a strict improvement over emitting "nothing known".
    ASSERT_EQ(sol.warm_start.bound_active.size(), 2u);
    EXPECT_EQ(sol.warm_start.bound_active, (std::vector<std::int8_t>{0, 1}))
        << "0 majors ran, but a SEED was ingested: the exit re-emits the seeded activity";
    EXPECT_EQ(sol.warm_start.bound_active, crossover.bound_active)
        << "and it is exactly the hint that went in, not a re-derivation";
}

// (b) DEGENERATE ROW LEFT FREE: a row with |slack_i| == 1e-9 AND
// lambda_i == 1e-9 -- both small, the classic IP-near-convergence
// degeneracy -- must be left FREE (the safe direction; the destination QP
// decides), never certified active.
TEST(WarmStart, CrossoverDegenerateRowLeftFree) {
    const Vec x = Vec::Zero(1);
    const Vec lambda_e = Vec(0);
    Vec lambda_i(1);
    lambda_i(0) = 1.0e-9;
    Vec slack_i(1);
    slack_i(0) = 1.0e-9;
    const Vec z_lower = Vec::Zero(1);
    const Vec z_upper = Vec::Zero(1);
    const Vec lower = Vec::Constant(1, -1.0e20);
    const Vec upper = Vec::Constant(1, 1.0e20);

    const WarmStart crossover =
        from_interior_point(x, lambda_e, lambda_i, slack_i, z_lower, z_upper, lower, upper);

    ASSERT_EQ(crossover.ineq_active.size(), 1u);
    EXPECT_EQ(crossover.ineq_active[0], 0)
        << "small residual AND small dual: left FREE -- exact, THE PIN";
    EXPECT_TRUE(crossover.qp_working_set.active_ineq().empty());
}

// (c) DIMENSION MISMATCHES THROW, with the offending sizes in the message
// (T6). One check per validated vector; a baseline, correctly-sized call
// first establishes that none of these throws when every size is right.
TEST(WarmStart, CrossoverDimensionMismatchesThrow) {
    const Vec x = Vec::Zero(2);
    const Vec lambda_e = Vec::Zero(1);
    const Vec lambda_i = Vec::Zero(1);
    const Vec slack_i = Vec::Zero(1);
    const Vec z_lower = Vec::Zero(2);
    const Vec z_upper = Vec::Zero(2);
    const Vec lower = Vec::Constant(2, -1.0);
    const Vec upper = Vec::Constant(2, 1.0);

    EXPECT_NO_THROW(
        from_interior_point(x, lambda_e, lambda_i, slack_i, z_lower, z_upper, lower, upper));

    auto expect_throw_with = [](auto &&call, const std::string &has_size,
                                const std::string &expected) {
        try {
            call();
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            const std::string msg = e.what();
            EXPECT_NE(msg.find(has_size), std::string::npos) << msg;
            EXPECT_NE(msg.find(expected), std::string::npos) << msg;
        }
    };

    expect_throw_with(
        [&] {
            from_interior_point(x, lambda_e, lambda_i, Vec::Zero(3), z_lower, z_upper, lower,
                                upper);
        },
        "has size 3", "expected 1");
    expect_throw_with(
        [&] {
            from_interior_point(x, lambda_e, lambda_i, slack_i, Vec::Zero(5), z_upper, lower,
                                upper);
        },
        "has size 5", "expected 2");
    expect_throw_with(
        [&] {
            from_interior_point(x, lambda_e, lambda_i, slack_i, z_lower, Vec::Zero(7), lower,
                                upper);
        },
        "has size 7", "expected 2");
    expect_throw_with(
        [&] {
            from_interior_point(x, lambda_e, lambda_i, slack_i, z_lower, z_upper, Vec::Zero(9),
                                upper);
        },
        "has size 9", "expected 2");
    expect_throw_with(
        [&] {
            from_interior_point(x, lambda_e, lambda_i, slack_i, z_lower, z_upper, lower,
                                Vec::Zero(4));
        },
        "has size 4", "expected 2");
}

// (d) WRONG-SIGN ROBUSTNESS: an IP solution can deliver a slightly NEGATIVE
// lambda_i on a row that IS geometrically active (near-convergence dual
// noise). The inference must NOT mark it active (the `lambda_i > dual_tol`
// gate), and the warm solve must still converge -- the QP re-derives the
// correct active set from a real linearization regardless of what this
// object's activity vector claimed.
TEST(WarmStart, CrossoverWrongSignDualLeavesRowFree) {
    const auto p = make_hs(14);
    SqpOptions opts;
    SqpDriver seed_driver(opts);
    const SqpSolution seed = seed_driver.solve(*p.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);
    ASSERT_GT(seed.lambda_i(0), 0.0) << "genuinely active at the true optimum";

    Vec slack_i(1);
    slack_i(0) = -1.0e-9; // still geometrically active: near zero, feasible side
    Vec lambda_i_wrong(1);
    lambda_i_wrong(0) = -1.0e-8; // WRONG SIGN: an IP solver's own near-convergence noise

    const Vec z_lower = Vec::Zero(p.model->n());
    const Vec z_upper = Vec::Zero(p.model->n());

    const WarmStart crossover =
        from_interior_point(seed.x, seed.lambda_e, lambda_i_wrong, slack_i, z_lower, z_upper,
                            p.model->lower(), p.model->upper());
    ASSERT_EQ(crossover.ineq_active.size(), 1u);
    EXPECT_EQ(crossover.ineq_active[0], 0)
        << "a negative lambda_i must never be certified active regardless of residual -- THE PIN";
    EXPECT_TRUE(crossover.qp_working_set.active_ineq().empty());
    EXPECT_DOUBLE_EQ(crossover.lambda_i(0), -1.0e-8)
        << "and the PRICE is carried verbatim: the dual_tol filter governs MEMBERSHIP only";

    const auto p_warm = make_hs(14);
    SqpDriver warm_driver(opts);
    const SqpSolution sol = warm_driver.solve(*p_warm.model, crossover.x, crossover);
    EXPECT_EQ(sol.status, SqpStatus::kOptimal)
        << "the QP re-derives the correct active set regardless of the wrong-signed dual";
    EXPECT_NEAR(sol.f, seed.f, 1e-6);

    // PHASE-6 TASK 5: THE OTHER HALF OF THIS FIXTURE'S STORY, which through
    // Phase 5 could not be told at all because the object resolved kCold and
    // its price never reached the driver. It now resolves kSeeded, so the
    // -1e-8 reaches the ingest -- and THE SEEDED DUAL CLAMP zeroes it and
    // counts it. Note the B-1 clear does NOT get there first: the row is
    // geometrically active (slack -1e-9, well inside feas_tol), which is
    // exactly why this configuration was the one the clear could not fix and
    // why the clamp had to exist. -1e-8 is two orders inside
    // kSeededDualClampTol = 1e-6, so it clamps rather than degrading.
    EXPECT_EQ(sol.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(sol.counters.seeded_clamped, 1)
        << "THE CLAMP PIN: the wrong-signed price on a geometrically ACTIVE row is zeroed at "
           "ingest and counted -- the closure of O-B1-4 on the one route that produces it";
}

// =============================================================================
// PHASE-6 TASK 5: StartLevel::kSeeded -- "trusts values, not provenance".
// See warm_start.h's StartLevel note for the contract and sqp_driver.h's
// WARM-START INGEST / THE SEEDED DUAL CLAMP notes for the resolution ladder.
// =============================================================================

// The P2 probe (sqp_driver.h's THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY;
// Task-7b review): min x s.t. cI1 = x <= 0, cI2 = -x - 1 <= 0, n = 1, no
// finite bounds. True solution x = -1, f = -1.
class P2TwoRowLine : public NlpModel {
  public:
    Index n() const override { return 1; }
    Index me() const override { return 0; }
    Index mi() const override { return 2; }

    double eval_f(const Vec &x) const override { return x(0); }
    Vec eval_grad(const Vec &) const override { return Vec::Constant(1, 1.0); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(2);
        c << x(0), -x(0) - 1.0;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
        SpMatRM h(1, 1);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 1);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(2, 1);
        j.insert(0, 0) = 1.0;
        j.insert(1, 0) = -1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(1); }

  private:
    Vec lower_ = Vec::Constant(1, -1e20);
    Vec upper_ = Vec::Constant(1, 1e20);
};

// Builds a hash-less WarmStart by hand at a given (x, lambda_i) with an empty
// activity hint -- the shape a foreign solver or a caller's own assembly
// produces, and the shape kSeeded exists to admit.
WarmStart hand_seed(const Vec &x, const Vec &lambda_i, Index mi) {
    WarmStart w;
    w.x = x;
    w.lambda_e = Vec(0);
    w.lambda_i = lambda_i;
    w.z = Vec::Zero(x.size());
    w.ineq_active.assign(static_cast<std::size_t>(mi), 0);
    w.bound_active.assign(static_cast<std::size_t>(x.size()), 0);
    w.qp_working_set = WorkingSet(x.size(), mi);
    w.structure_hash = 0;
    w.valid = true;
    return w;
}

// (1) THE GROSS WRONG-SIGN PRICE DEGRADES THE OBJECT TO kCold -- the closure of
// the P2 hole, on the exact input the Task-7b review measured.
//
// PRE-TASK-5 THIS INPUT WAS UNREACHABLE THROUGH solve(): a hand-built object
// carries structure_hash == 0 and resolved kCold, so the false certificate P2
// documents needed a hash-forging caller to reproduce. kSeeded makes hand-built
// objects ingestible, which is precisely what would have made P2 reachable --
// so the clamp had to land in the same task. Here it does: -1 on a
// GEOMETRICALLY ACTIVE row is six orders outside kSeededDualClampTol, the
// object degrades, and the solve runs cold to the TRUE answer instead of
// certifying f = 0 in zero majors.
TEST(WarmStart, SeededGrossWrongSignPriceDegradesToCold) {
    P2TwoRowLine model;
    SqpOptions opts;
    SqpDriver driver(opts);

    Vec lambda_i(2);
    lambda_i << -1.0, 0.5; // the P2 input, verbatim
    const WarmStart bad = hand_seed(Vec::Zero(1), lambda_i, 2);

    const SqpSolution sol = driver.solve(model, model.start_point(), bad);

    EXPECT_EQ(sol.counters.start_level_used, StartLevel::kCold)
        << "THE PIN: a wrong-signed price at O(1) is not a seed, it is garbage";
    EXPECT_EQ(sol.counters.n_seeded, 0);
    EXPECT_EQ(sol.counters.seeded_clamped, 0)
        << "a degraded object ingested NOTHING, so it clamped nothing either";
    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_GT(sol.counters.major_iters, 0) << "pre-closure this certified f = 0 in ZERO majors";
    EXPECT_NEAR(sol.x(0), -1.0, 1e-7);
    EXPECT_NEAR(sol.f, -1.0, 1e-7) << "the truth; the P2 defect reported 0";

    // The degraded solve is a genuine cold solve of the caller's own x0 -- not
    // a half-ingested hybrid. Checked at the strongest available standard,
    // against a real 2-arg cold solve from the same point.
    P2TwoRowLine cold_model;
    SqpDriver cold_driver(opts);
    const SqpSolution cold = cold_driver.solve(cold_model, cold_model.start_point());
    EXPECT_EQ(sol.status, cold.status);
    EXPECT_EQ(sol.counters.major_iters, cold.counters.major_iters);
    EXPECT_EQ(sol.x, cold.x);
    EXPECT_EQ(sol.f, cold.f);
}

// (2) THE CLAMP'S OWN BOUNDARY, BOTH SIDES, on one fixture -- the same
// discipline test_b1_gate.cpp's TheActivityBoundaryIsFeasTolOnBothSides applies
// to the B-1 gate. Only the magnitude of the negative price differs between the
// arms; it straddles kSeededDualClampTol.
TEST(WarmStart, SeededDualClampBoundaryIsExactOnBothSides) {
    SqpOptions opts;

    for (const bool inside : {true, false}) {
        SCOPED_TRACE(inside ? "inside the band" : "outside the band");
        P2TwoRowLine model;
        SqpDriver driver(opts);
        Vec lambda_i(2);
        // Row 0 (x <= 0) is geometrically ACTIVE at x = 0, so its price
        // survives the B-1 clear and meets the clamp. Row 1 is strictly slack
        // and is cleared first, by contract.
        lambda_i << (inside ? -0.5 * kSeededDualClampTol : -2.0 * kSeededDualClampTol), 0.0;
        const SqpSolution sol =
            driver.solve(model, model.start_point(), hand_seed(Vec::Zero(1), lambda_i, 2));

        if (inside) {
            EXPECT_EQ(sol.counters.start_level_used, StartLevel::kSeeded);
            EXPECT_EQ(sol.counters.seeded_clamped, 1);
        } else {
            EXPECT_EQ(sol.counters.start_level_used, StartLevel::kCold);
            EXPECT_EQ(sol.counters.seeded_clamped, 0);
        }
        // Either way the ANSWER is right: the clamp is a seed-quality
        // judgement, never a correctness one.
        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_NEAR(sol.f, -1.0, 1e-7);
    }
}

// (3) THE B-1 CLEAR RUNS FIRST, AND THE ORDER IS OBSERVABLE. A price that is
// NEGATIVE AND GROSS but sits on a STRICTLY SLACK row must be CLEARED (dropped
// as stale bookkeeping) rather than degrading the object -- which is the whole
// reason sqp_driver.h fixes that order. Reversing the two blocks turns this
// fixture from kSeeded into kCold, so this test is the order's executable
// specification and not merely its description.
TEST(WarmStart, SeededB1ClearRunsBeforeTheSignClampAndTheOrderIsObservable) {
    P2TwoRowLine model;
    SqpOptions opts;
    SqpDriver driver(opts);

    Vec lambda_i(2);
    // Row 1 (-x - 1 <= 0) has cI = -1 at x = 0: STRICTLY slack, six orders
    // outside feas_tol. Its -3.0 price is stale bookkeeping the B-1 clear
    // zeroes. Row 0 is active and correctly signed.
    lambda_i << 1.0, -3.0;
    const SqpSolution sol =
        driver.solve(model, model.start_point(), hand_seed(Vec::Zero(1), lambda_i, 2));

    EXPECT_EQ(sol.counters.start_level_used, StartLevel::kSeeded)
        << "THE ORDER PIN: a gross negative price on a STRICTLY SLACK row is cleared, not "
           "degraded -- clamp-before-clear would report kCold here and kill the level on both "
           "its intended producers";
    EXPECT_EQ(sol.counters.seeded_clamped, 0)
        << "the clear got there first, so the clamp saw a zero and had nothing to do";
    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_NEAR(sol.f, -1.0, 1e-7);
}

// (4) THE INGEST GATE: incompatible dimensions and non-finite values both
// resolve kCold, with no hash involved either way.
TEST(WarmStart, SeededGateRejectsIncompatibleAndNonFiniteObjects) {
    SqpOptions opts;

    // (a) DIMENSIONALLY INCOMPATIBLE -- HS7's own hand-off (n=2, me=1, mi=0)
    // into HS10 (n=2, me=0, mi=1). Already pinned kCold by
    // WarmStart.StaleWarmIsSafe; re-asserted here as the seeded gate's first
    // conjunct so that a future widening of the gate fails in this file too.
    const auto p7 = make_hs(7);
    SqpDriver d7{SqpOptions{}};
    const SqpSolution s7 = d7.solve(*p7.model);
    ASSERT_EQ(s7.status, SqpStatus::kOptimal);

    const auto p10 = make_hs(10);
    SqpDriver d10{opts};
    const SqpSolution stale = d10.solve(*p10.model, p10.model->start_point(), s7.warm_start);
    EXPECT_EQ(stale.counters.start_level_used, StartLevel::kCold)
        << "kSeeded still requires (n, me, mi) compatibility -- it drops the HASH, not the shape";
    EXPECT_EQ(stale.counters.n_seeded, 0);

    // (b) NON-FINITE. A dimensionally perfect, hash-less object carrying a NaN
    // in any ingested vector must resolve kCold rather than being seeded onto a
    // point the model cannot measure. Three arms, one per ingested vector.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (int which = 0; which < 3; ++which) {
        SCOPED_TRACE(which == 0 ? "x" : which == 1 ? "lambda_e" : "lambda_i");
        const auto p6 = make_hs(6);
        SqpDriver seed_driver{SqpOptions{}};
        const SqpSolution seeded = seed_driver.solve(*p6.model);
        ASSERT_EQ(seeded.status, SqpStatus::kOptimal);

        WarmStart poisoned = seeded.warm_start;
        poisoned.structure_hash = 0; // force the seeded route, not the warm one
        if (which == 0) {
            poisoned.x(0) = nan;
        } else if (which == 1 && poisoned.lambda_e.size() > 0) {
            poisoned.lambda_e(0) = nan;
        } else if (which == 2 && poisoned.lambda_i.size() > 0) {
            poisoned.lambda_i(0) = nan;
        } else {
            continue; // HS6 has no rows of this kind; nothing to poison
        }

        const auto p6b = make_hs(6);
        SqpDriver driver{opts};
        const SqpSolution sol = driver.solve(*p6b.model, p6b.model->start_point(), poisoned);
        EXPECT_EQ(sol.counters.start_level_used, StartLevel::kCold)
            << "a non-finite ingested vector resolves kCold -- the caller-input rule x0 itself is "
               "held to, applied to the other route the same values arrive by";
        EXPECT_EQ(sol.status, SqpStatus::kOptimal) << "and the cold solve is unharmed";
    }
}

// M3 FINAL REVIEW, S-3. THE FINITENESS GATE COVERS THE HASH-MATCHING ROUTE
// TOO. Test (4)(b) above deliberately zeroes structure_hash to force the
// SEEDED route, which is where the gate used to live; this one leaves the hash
// INTACT so the object resolves kWarm (or kHot) on its own merits and then
// poisons an ingested vector. solver_counters.h states the rule without a
// route qualifier -- "kCold ... when any ingested vector is non-finite" -- and
// the argument the old placement rested on ("a hash-matching object came from
// a solve whose exits are finite by construction") is a fact about objects
// this library PRODUCES, not about the aggregate a caller may hand in with
// every field public.
TEST(WarmStart, NonFiniteObjectResolvesColdEvenWithAMatchingHash) {
    const double nan = std::numeric_limits<double>::quiet_NaN();

    const auto seed_p = make_hs(7);
    SqpDriver seed_driver{SqpOptions{}};
    const SqpSolution seed = seed_driver.solve(*seed_p.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);
    ASSERT_NE(seed.warm_start.structure_hash, 0u)
        << "the object must carry a real hash, or this test is test (4)(b) again";

    // THE CONTROL first: unpoisoned, this exact object resolves above kSeeded,
    // so the kCold below is the NaN and not some unrelated mismatch.
    {
        const auto p = make_hs(7);
        SqpDriver driver{SqpOptions{}};
        const SqpSolution warm = driver.solve(*p.model, p.model->start_point(), seed.warm_start);
        EXPECT_GE(static_cast<int>(warm.counters.start_level_used),
                  static_cast<int>(StartLevel::kWarm))
            << "the unpoisoned object earns the hash-matching route";
        EXPECT_EQ(warm.status, SqpStatus::kOptimal);
    }

    for (int which = 0; which < 3; ++which) {
        SCOPED_TRACE(which == 0 ? "x" : which == 1 ? "lambda_e" : "lambda_i");
        WarmStart poisoned = seed.warm_start;
        if (which == 0) {
            poisoned.x(0) = nan;
        } else if (which == 1 && poisoned.lambda_e.size() > 0) {
            poisoned.lambda_e(0) = nan;
        } else if (which == 2 && poisoned.lambda_i.size() > 0) {
            poisoned.lambda_i(0) = nan;
        } else {
            continue; // HS7 has no rows of this kind; nothing to poison
        }
        ASSERT_NE(poisoned.structure_hash, 0u) << "the hash is deliberately left matching";

        const auto p = make_hs(7);
        SqpDriver driver{SqpOptions{}};
        const SqpSolution sol = driver.solve(*p.model, p.model->start_point(), poisoned);
        EXPECT_EQ(sol.counters.start_level_used, StartLevel::kCold)
            << "a matching hash does not buy an ingest of a NaN";
        EXPECT_EQ(sol.counters.n_seeded, 0);
        EXPECT_EQ(sol.status, SqpStatus::kOptimal) << "and the cold solve is unharmed";
        EXPECT_TRUE(sol.x.allFinite()) << "the NaN was never ingested";
    }
}

// (5) THE THREE REFUSALS. A kSeeded ingest must take x/duals/activity and
// NOTHING ELSE: no trust-region radius, no funnel width, no Kungurtsev-Diehl
// window. Isolated by feeding the SAME object twice, once with the ceiling at
// kWarm (it has a matching hash, so it resolves kWarm and takes all three) and
// once at kSeeded (it takes none of them) -- then reading the one counter that
// reports the KD window directly.
TEST(WarmStart, SeededRefusesFunnelTrustRegionAndFullStepState) {
    SqpOptions warm_opts;
    ASSERT_TRUE(warm_opts.warm_full_step) << "the lever is on by default, or this test is vacuous";

    const auto p7 = make_hs(7);
    SqpDriver seed_driver{SqpOptions{}};
    const SqpSolution seed = seed_driver.solve(*p7.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);
    ASSERT_NE(seed.warm_start.structure_hash, 0u);
    ASSERT_GE(seed.warm_start.tr_radius, 0.0) << "the object really does carry a radius";
    ASSERT_GE(seed.warm_start.funnel_width, 0.0) << "and a funnel width";

    // Hs7PerturbedModel keeps HS7's sparsity (so the hash matches and the kWarm
    // arm is reachable) while moving the solution enough that the warm arm has
    // real majors to spend under the full-step mode -- see PerturbedWarmBeatsCold.
    Hs7PerturbedModel warm_model(1.0e-3);
    SqpDriver warm_driver(warm_opts);
    const SqpSolution warm =
        warm_driver.solve(warm_model, warm_model.start_point(), seed.warm_start);
    ASSERT_EQ(warm.counters.start_level_used, StartLevel::kWarm);
    ASSERT_EQ(warm.status, SqpStatus::kOptimal);

    SqpOptions seeded_opts = warm_opts;
    seeded_opts.start_level = StartLevel::kSeeded;
    Hs7PerturbedModel seeded_model(1.0e-3);
    SqpDriver seeded_driver(seeded_opts);
    const SqpSolution seeded =
        seeded_driver.solve(seeded_model, seeded_model.start_point(), seed.warm_start);
    ASSERT_EQ(seeded.counters.start_level_used, StartLevel::kSeeded);
    ASSERT_EQ(seeded.status, SqpStatus::kOptimal);

    // THE KD WINDOW is the one refusal with a dedicated counter, so it is the
    // one asserted directly rather than inferred from a trajectory.
    EXPECT_GT(warm.counters.full_step_majors, 0)
        << "the kWarm arm really does arm the full-step mode, or this comparison is vacuous";
    EXPECT_EQ(seeded.counters.full_step_majors, 0)
        << "THE PIN: a seeded object never arms the Kungurtsev-Diehl full-step window";

    // THE TRUST REGION AND THE FUNNEL WIDTH, pinned together as a TOTAL
    // refusal: two objects differing ONLY in those two fields must produce a
    // BYTE-IDENTICAL seeded solve, while producing a demonstrably different
    // kWarm one. That is a stronger statement than reading one radius off one
    // history row, and it cannot go vacuous -- the kWarm arm is required to
    // notice the difference the seeded arm is required to ignore.
    WarmStart poisoned_state = seed.warm_start;
    poisoned_state.tr_radius = 0.05;     // well below opts.tr_init
    poisoned_state.funnel_width = 1.0e6; // absurdly loose
    ASSERT_NE(poisoned_state.tr_radius, seed.warm_start.tr_radius);

    Hs7PerturbedModel warm_model_b(1.0e-3);
    SqpDriver warm_driver_b(warm_opts);
    const SqpSolution warm_b =
        warm_driver_b.solve(warm_model_b, warm_model_b.start_point(), poisoned_state);
    ASSERT_EQ(warm_b.counters.start_level_used, StartLevel::kWarm);

    Hs7PerturbedModel seeded_model_b(1.0e-3);
    SqpDriver seeded_driver_b(seeded_opts);
    const SqpSolution seeded_b =
        seeded_driver_b.solve(seeded_model_b, seeded_model_b.start_point(), poisoned_state);
    ASSERT_EQ(seeded_b.counters.start_level_used, StartLevel::kSeeded);

    ASSERT_FALSE(warm.history.empty());
    ASSERT_FALSE(warm_b.history.empty());
    ASSERT_FALSE(seeded.history.empty());
    ASSERT_FALSE(seeded_b.history.empty());
    EXPECT_NE(warm.history.front().tr_radius, warm_b.history.front().tr_radius)
        << "the kWarm arm READS the carried radius, or the refusal below proves nothing";
    EXPECT_DOUBLE_EQ(seeded.history.front().tr_radius, SqpOptions{}.tr_init)
        << "THE PIN: tr_radius is refused, so a seeded solve starts at opts.tr_init";
    EXPECT_DOUBLE_EQ(seeded_b.history.front().tr_radius, SqpOptions{}.tr_init);
    EXPECT_EQ(seeded.counters.major_iters, seeded_b.counters.major_iters)
        << "THE PIN: the seeded arm is byte-identical across a poisoned funnel/TR state";
    EXPECT_EQ(seeded.x, seeded_b.x);
    EXPECT_EQ(seeded.f, seeded_b.f);

    // Both arms still solve the problem; the refusals cost correctness nothing.
    EXPECT_NEAR(seeded.f, warm.f, 1e-6);
}

// (6) THE LEDGER'S LEVEL HISTOGRAM (ledger.h's StartLevelHistogram) counts what
// actually resolved, across all four levels, and accounts for every record.
TEST(WarmStart, LedgerLevelHistogramCountsEveryResolvedLevel) {
    Ledger ledger;
    SqpOptions opts;

    const auto p = make_hs(6);
    SqpDriver driver(opts);
    driver.attach_ledger(&ledger, "hist");

    // 1: cold (2-arg overload).
    const SqpSolution cold = driver.solve(*p.model);
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    // 2: warm (matching hash).
    const SqpSolution warm = driver.solve(*p.model, p.model->start_point(), cold.warm_start);
    ASSERT_EQ(warm.counters.start_level_used, StartLevel::kWarm);
    // 3: seeded (hash erased).
    WarmStart hashless = cold.warm_start;
    hashless.structure_hash = 0;
    const SqpSolution seeded = driver.solve(*p.model, p.model->start_point(), hashless);
    ASSERT_EQ(seeded.counters.start_level_used, StartLevel::kSeeded);

    const StartLevelHistogram h = ledger.level_histogram();
    EXPECT_EQ(h.cold, 1);
    EXPECT_EQ(h.seeded, 1);
    EXPECT_EQ(h.warm, 1);
    EXPECT_EQ(h.hot, 0);
    EXPECT_EQ(h.total(), static_cast<Index>(ledger.sqp_records().size()))
        << "the four columns must account for every record";

    EXPECT_NE(ledger.sqp_summary_table().find("Seeded"), std::string::npos)
        << ledger.sqp_summary_table();
}

// =============================================================================
// PHASE-7 TASK 0: the collocation-scale activity repair
// (warm_start.h's ACTIVITY INFERENCE note;
// docs/notes/2026-08-06-activity-tol-repair.md).
// =============================================================================
//
// THE DEFECT THESE ARMS CLOSE, from docs/notes/2026-08-04-kseeded-ingest.md
// section 6.2: the pre-repair rule
// `|slack_i(j)| < activity_tol * max(1, |lambda_i(j)|)` is ABSOLUTE for every
// multiplier below O(1), and a collocation multiplier is O(h). On F7 at
// p = 0.9 the live prices run 1.41e-4 to 8.08e-3 at N = 100 and shrink like h
// under refinement, while the barrier residue an IP method stops at is
// mu/lambda and GROWS as the price shrinks -- so the shipped 1e-6 default
// inferred an EMPTY active set exactly where the hint is worth the most.
//
// THE FIXTURE IS A CENTRAL-PATH POINT, NOT THE PHASE-6 BATTERY RECIPE, and
// the difference is the whole reason these arms can measure a FALSE-active
// count at all. test_warm_start_battery.cpp's CrossoverChain fixture takes
// its duals from a converged SQP solve, so every inactive row carries
// lambda_i == 0 EXACTLY and no rule can over-infer on it. A real IP method
// prices EVERY row (lambda_i(j) * s_j = mu on all of them, active or not), so
// an inactive row arrives with a small but strictly positive price -- and
// whether the inference can tell that price from a live one is precisely the
// question. These fixtures therefore synthesize the central path itself:
// s_j = mu/lambda_i*(j) on F7's analytically active rows, s_j = -cI_j(x*) on
// the rest, and lambda_i(j) = mu/s_j everywhere. Everything is analytic
// (x_star/lambda_i_star/active_set), so no arm here runs a solve and the
// whole set is milliseconds even at N = 1600.
namespace ip_repair {

struct IpIterate {
    Vec x, lambda_e, lambda_i, slack_i, z_lower, z_upper;
};

// The central-path hand-off described above. `disp` displaces the primal
// deterministically (no RNG), exactly as the Phase-6 battery fixture does; it
// is deliberately irrelevant to the ROW inference, which reads duals and
// slacks alone, and is carried so these fixtures stay a hand-off rather than
// a table of multipliers.
IpIterate f7_ip_iterate(const F7CollocationChain &model, double p, double mu, double disp) {
    const Vec x_star = model.x_star(p);
    const Vec lambda_i_star = model.lambda_i_star(p);
    const Vec ci_star = model.eval_ci(x_star);
    const auto analytic = model.active_set(p);

    IpIterate it;
    it.x = x_star;
    for (Index i = 0; i < model.n(); ++i) {
        it.x(i) += disp * std::sin(static_cast<double>(i));
    }
    it.lambda_e = model.lambda_e_star(p);
    it.lambda_i = Vec::Zero(model.mi());
    it.slack_i = Vec::Zero(model.mi());
    for (Index j = 0; j < model.mi(); ++j) {
        const bool active = analytic.ineq_active[static_cast<std::size_t>(j)] != 0;
        const double s = active ? mu / lambda_i_star(j) : -ci_star(j);
        it.slack_i(j) = -s;      // this project's convention: the cI VALUE
        it.lambda_i(j) = mu / s; // the central path, on every row
    }
    // F7's control box is inactive at the optimum for every p in the design
    // range (|u*| = 0.5 < 1), so both IP bound multipliers vanish -- the same
    // reading test_warm_start_battery.cpp's crossover chain makes.
    it.z_lower = Vec::Zero(model.n());
    it.z_upper = Vec::Zero(model.n());
    return it;
}

struct Score {
    Index inferred = 0;     // rows the hint proposed active
    Index recovered = 0;    // ... that F7's analytic active set agrees with
    Index false_active = 0; // ... that it does not
    Index analytic = 0;     // |analytic active set|
};

Score score_hint(const F7CollocationChain &model, double p, const std::vector<std::uint8_t> &hint) {
    const auto analytic = model.active_set(p);
    Score s;
    for (Index j = 0; j < model.mi(); ++j) {
        const bool truth = analytic.ineq_active[static_cast<std::size_t>(j)] != 0;
        s.analytic += truth ? 1 : 0;
        if (hint[static_cast<std::size_t>(j)] == 0) {
            continue;
        }
        ++s.inferred;
        if (truth) {
            ++s.recovered;
        } else {
            ++s.false_active;
        }
    }
    return s;
}

// The PRE-REPAIR rule, transcribed verbatim from the header as it stood at
// Phase-6 HEAD. Kept executable rather than quoted so the "FAILS at 0/92
// today" claim in the task brief is a measurement this suite re-derives on
// every run, not a number in a comment that nothing checks.
Score score_pre_repair(const F7CollocationChain &model, double p, const IpIterate &it,
                       double activity_tol, double dual_tol) {
    std::vector<std::uint8_t> hint(static_cast<std::size_t>(model.mi()), 0);
    for (Index j = 0; j < model.mi(); ++j) {
        const bool active =
            std::abs(it.slack_i(j)) < activity_tol * std::max(1.0, std::abs(it.lambda_i(j))) &&
            it.lambda_i(j) > dual_tol;
        hint[static_cast<std::size_t>(j)] = active ? 1 : 0;
    }
    return score_hint(model, p, hint);
}

// CANDIDATE B, the rejected repair (the brief's complementarity-ratio test):
// active iff lambda_i(j) * max(1, |cI_j|)^-1 dominates mu, i.e.
// lambda_i(j) >= nu * mu_hat * max(1, |slack_i(j)|), with the SAME data-derived
// mu_hat and the SAME nu = 10 the adopted rule uses, so the two columns differ
// in the formula alone. Lives here rather than behind a switch in the header
// because a rejected candidate should be reproducible without being shippable.
Score score_candidate_b(const F7CollocationChain &model, double p, const IpIterate &it,
                        double dual_tol) {
    double complementarity = 0.0;
    Index priced = 0;
    for (Index j = 0; j < model.mi(); ++j) {
        if (it.lambda_i(j) > 0.0) {
            complementarity += it.lambda_i(j) * std::abs(it.slack_i(j));
            ++priced;
        }
    }
    const double mu_hat = priced == 0 ? 0.0 : complementarity / static_cast<double>(priced);
    std::vector<std::uint8_t> hint(static_cast<std::size_t>(model.mi()), 0);
    for (Index j = 0; j < model.mi(); ++j) {
        const bool active =
            it.lambda_i(j) > dual_tol &&
            it.lambda_i(j) >= kIpActivityFactor * mu_hat * std::max(1.0, std::abs(it.slack_i(j)));
        hint[static_cast<std::size_t>(j)] = active ? 1 : 0;
    }
    return score_hint(model, p, hint);
}

Score score_crossover(const F7CollocationChain &model, double p, const IpIterate &it,
                      const IpCrossoverOptions &opts = {}) {
    const WarmStart crossover =
        from_interior_point(it.x, it.lambda_e, it.lambda_i, it.slack_i, it.z_lower, it.z_upper,
                            model.lower(), model.upper(), opts);
    EXPECT_TRUE(crossover.valid);
    EXPECT_EQ(crossover.structure_hash, 0u);
    EXPECT_EQ(crossover.qp_working_set.active_ineq().size(),
              static_cast<std::size_t>(
                  std::count(crossover.ineq_active.begin(), crossover.ineq_active.end(), 1)))
        << "the working set and the activity vector must agree row for row";
    return score_hint(model, p, crossover.ineq_active);
}

constexpr double kP = 0.9; // the head-to-head note's own sweep parameter
constexpr double kDisp = 1e-7;

} // namespace ip_repair

// (a) THE REPAIR'S HEADLINE: a converged IP hand-off at collocation scale is
// read TRUTHFULLY on the shipped defaults, at both barrier levels the record
// names -- mu = 1e-8 (the IPM bridge's own bar_tol default, and the value
// docs/notes/2026-08-04-kseeded-ingest.md section 6.2 measured 0-of-92 at) and
// mu = 1e-9 (the value the Phase-4 crossover recipe uses).
//
// MEASURED (MKL Pardiso, clang++ Release, MKL_NUM_THREADS=1; the inference is
// pure arithmetic on analytic data, so these counts are backend- and
// build-independent -- Mac re-verification is a formality, not a risk):
//
//   mu     pre-repair (activity_tol = 1e-6)   repaired (defaults)
//   1e-8   0 of 92 recovered, 0 false          92 of 92, 0 false
//   1e-9   84 of 92 recovered, 0 false         92 of 92, 0 false
//
// The 84 at mu = 1e-9 is worth the line it costs: the pre-repair rule is not
// uniformly empty at N = 100, it is empty at the barrier level a real bridge
// hands over at and 91 % correct one decade tighter -- which is exactly the
// signature of an ABSOLUTE test standing next to a residue that scales.
TEST(WarmStart, CrossoverActivityInferenceRecoversTheCollocationActiveSet) {
    using namespace ip_repair;
    F7CollocationChain model(/*nodes=*/100, /*states=*/3, /*controls=*/2, kP, /*radius=*/1.0);
    ASSERT_EQ(model.n(), 500) << "nx = 5N: the head-to-head note's sweep_n100 shape";
    ASSERT_EQ(model.mi(), 100) << "one path row per node";

    for (const double mu : {1e-8, 1e-9}) {
        SCOPED_TRACE(fmt::format("mu = {:g}", mu));
        const IpIterate it = f7_ip_iterate(model, kP, mu, kDisp);

        const Score repaired = score_crossover(model, kP, it);
        ASSERT_EQ(repaired.analytic, 92) << "F7's own 92-row active window at p = 0.9, N = 100";
        EXPECT_EQ(repaired.recovered, 92) << "THE PIN (observed): every analytically active row";
        EXPECT_EQ(repaired.false_active, 0) << "THE PIN (observed): and nothing else";
        EXPECT_GE(static_cast<double>(repaired.recovered) / static_cast<double>(repaired.analytic),
                  0.90)
            << "the task's own acceptance bar, met with the exact counts pinned above";

        const Score pre = score_pre_repair(model, kP, it, /*activity_tol=*/1e-6, /*dual_tol=*/1e-6);
        EXPECT_EQ(pre.recovered, mu == 1e-8 ? 0 : 84)
            << "THE DEFECT, re-derived: the pre-repair rule at its shipped default";
        EXPECT_LT(pre.recovered, repaired.recovered) << "and the repair is strictly better";
    }
}

// (a') THE GAP WIDENS WITH REFINEMENT -- the half of the defect record that
// makes it a defect rather than a mis-set knob, since lambda ~ h shrinks under
// refinement while mu does not. mu = 1e-9 throughout, so the ONLY thing moving
// is the mesh.
//
// MEASURED (backend banner as above):
//
//   N      analytic   pre-repair @1e-6   repaired (defaults)
//   100    92         84  (91.3 %)       92 of 92,   0 false
//   400    370        250 (67.6 %)       370 of 370, 0 false
//   1600   1486       0   (0.0 %)        1484 of 1486, 0 false
//
// THE TWO ROWS THE REPAIR STILL MISSES AT N = 1600 ARE NOT RECOVERABLE BY ANY
// DUAL-SIDE THRESHOLD, and that is worth more than the two counts. Measured on
// this fixture: the two weakest genuinely-active prices are 3.626e-7 each,
// while the two strongest barrier-noise prices on INACTIVE rows are 8.497e-7
// each -- the live and the noise populations OVERLAP at this mesh and barrier
// level, because a junction-adjacent row's true price has fallen below what
// mu/s* leaves on a strictly slack one. No threshold can be both complete and
// clean there; the adopted rule's 5.003e-7 sits between the two and takes the
// CONSERVATIVE side (2 missed, 0 invented), which is the direction this
// header's ambiguity argument has always chosen. The arm below pins the
// overlap itself, and pins that `dual_tol` is NOT the binding floor here --
// lowering it two decades recovers nothing.
TEST(WarmStart, CrossoverActivityInferenceHoldsUnderMeshRefinement) {
    using namespace ip_repair;
    constexpr double kMu = 1e-9;
    struct Arm {
        Index nodes;
        Index analytic, repaired_recovered, pre_recovered;
    };
    const std::vector<Arm> arms = {{100, 92, 92, 84}, {400, 370, 370, 250}, {1600, 1486, 1484, 0}};

    for (const Arm &arm : arms) {
        SCOPED_TRACE(fmt::format("N = {} nodes (nx = {})", arm.nodes, 5 * arm.nodes));
        F7CollocationChain model(arm.nodes, 3, 2, kP, 1.0);
        const IpIterate it = f7_ip_iterate(model, kP, kMu, kDisp);

        const Score repaired = score_crossover(model, kP, it);
        EXPECT_EQ(repaired.analytic, arm.analytic);
        EXPECT_EQ(repaired.recovered, arm.repaired_recovered) << "THE PIN (observed)";
        EXPECT_EQ(repaired.false_active, 0) << "THE PIN (observed): no row invented, at any mesh";

        const Score pre = score_pre_repair(model, kP, it, 1e-6, 1e-6);
        EXPECT_EQ(pre.recovered, arm.pre_recovered)
            << "THE DEFECT, re-derived at this mesh: it widens to nothing by N = 1600";
    }

    // THE ATTRIBUTION ARM: the two missed rows are an OVERLAP, not a floor.
    F7CollocationChain fine(1600, 3, 2, kP, 1.0);
    const IpIterate it = f7_ip_iterate(fine, kP, kMu, kDisp);
    const auto analytic = fine.active_set(kP);
    double weakest_live = std::numeric_limits<double>::infinity();
    double loudest_noise = 0.0;
    for (Index j = 0; j < fine.mi(); ++j) {
        const double price = it.lambda_i(j);
        if (analytic.ineq_active[static_cast<std::size_t>(j)] != 0) {
            weakest_live = std::min(weakest_live, price);
        } else {
            loudest_noise = std::max(loudest_noise, price);
        }
    }
    EXPECT_NEAR(weakest_live, 3.6258e-7, 1e-11) << "THE PIN (observed)";
    EXPECT_NEAR(loudest_noise, 8.4968e-7, 1e-11) << "THE PIN (observed)";
    EXPECT_LT(weakest_live, loudest_noise)
        << "THE FINDING: at N = 1600 with mu = 1e-9 the live and barrier-noise price populations "
           "OVERLAP, so no dual-side threshold recovers all 1486 rows without inventing at least "
           "two -- the residue is a property of the hand-off, not of a tolerance";

    // ... and `dual_tol` is not the floor a caller should reach for here: two
    // decades of headroom on it recovers NOTHING (the rule's own 5.003e-7
    // threshold is what excludes the two live rows) while it lets the 8.497e-7
    // noise pair straight in. The overlap costs two rows in whichever
    // direction it is paid, which is the sharpest available statement that the
    // residue belongs to the hand-off and not to a tolerance.
    IpCrossoverOptions lowered;
    lowered.dual_tol = 1e-8;
    const Score all = score_crossover(fine, kP, it, lowered);
    EXPECT_EQ(all.recovered, 1484) << "THE PIN (observed): unchanged by dual_tol";
    EXPECT_EQ(all.false_active, 2)
        << "THE PIN (observed): the two noise rows dual_tol was quietly blocking";
}

// (b) A LOOSE HAND-OFF MUST NOT OVER-INFER. mu = 1e-4 is four decades looser
// than a crossover would normally hand over at, and on the central path that
// means every INACTIVE row carries a price of mu/s* -- up to 6.97e-3 here,
// which is larger than most of F7's genuinely live prices. This is the
// adversarial direction for any dual-side rule and the bound below is where
// the adopted one lands.
//
// MEASURED: 92 rows inferred, 84 correct, **8 false** -- a false-active
// fraction of 8/92 = 8.70 % of the hint. The bound is pinned at 10 %: one
// significant figure above the observation, so a formula change that doubles
// the over-inference fails while the measurement's own last digit is not a
// tripwire. The 8 rows are the ones nearest the two junctions, where a live
// price and a barrier-noise price genuinely meet.
//
// THE HONEST QUALIFICATION, AND IT IS A CARRY RATHER THAN A PASS: the same
// arm at N = 1600 infers 114 rows of which **all 114 are false**, because by
// then every live price (<= 5.00e-4) sits under the threshold a mu of 1e-4
// forces (1.00e-3) while the barrier-noise prices on inactive rows do not.
// Identification is simply not available from a hand-off that loose, and
// warm_start.h's note states the regime (kIpActivityFactor * mu_hat >
// ||lambda_i||inf * activity_rel_tol) so a caller can test for it. Pinned
// below so the carry cannot rot silently.
TEST(WarmStart, CrossoverLooseIpIterateDoesNotOverInfer) {
    using namespace ip_repair;
    constexpr double kLooseMu = 1e-4;
    F7CollocationChain model(100, 3, 2, kP, 1.0);
    const IpIterate it = f7_ip_iterate(model, kP, kLooseMu, kDisp);

    const Score s = score_crossover(model, kP, it);
    EXPECT_EQ(s.inferred, 92) << "THE PIN (observed)";
    EXPECT_EQ(s.recovered, 84) << "THE PIN (observed)";
    EXPECT_EQ(s.false_active, 8) << "THE PIN (observed)";
    ASSERT_GT(s.inferred, 0);
    EXPECT_LE(static_cast<double>(s.false_active) / static_cast<double>(s.inferred), 0.10)
        << "the pinned false-active bound: 8/92 = 8.70 % observed";

    // The carry, executable: a loose hand-off on a FINE mesh is not merely
    // imprecise, it is inverted.
    F7CollocationChain fine(1600, 3, 2, kP, 1.0);
    const IpIterate fine_it = f7_ip_iterate(fine, kP, kLooseMu, kDisp);
    const Score fine_score = score_crossover(fine, kP, fine_it);
    EXPECT_EQ(fine_score.inferred, 114) << "THE PIN (observed)";
    EXPECT_EQ(fine_score.recovered, 0)
        << "THE PIN (observed): at mu = 1e-4 and N = 1600 the hint is 100 % wrong -- the regime "
           "warm_start.h's ACTIVITY INFERENCE note tells a caller to test for";
}

// THE BAKE-OFF, KEPT EXECUTABLE. The task brief named two candidate repairs
// and required the loser's failure to be MEASURED rather than argued:
//   (A) adopted -- lambda_i(j) >= nu * max(mu_hat, ||lambda_i||inf * eps_rel)
//   (B) rejected -- lambda_i(j) * max(1, |cI_j|)^-1 dominates mu, i.e.
//                   lambda_i(j) >= nu * mu_hat * max(1, |slack_i(j)|)
// Both carry the same data-derived mu_hat, the same nu = kIpActivityFactor and
// the same dual_tol, so the columns differ in the formula alone.
//
// MEASURED, central-path hand-offs, recovery / false-active:
//
//   cell              A (adopted)        B (rejected)
//   N=100  mu=1e-9    92/92,    0        92/92,    0
//   N=400  mu=1e-9    370/370,  0        370/370,  2
//   N=400  mu=1e-8    370/370,  2        370/370,  4
//   N=1600 mu=1e-9    1484/1486, 0       1484/1486, 0
//
// A IS ADOPTED ON A MEASURED DIFFERENCE, NOT A TIE-BREAK. Recovery is equal in
// every cell -- both rules clear the brief's primary criterion identically --
// and the separation is entirely in the false-active column of the REFINEMENT
// arms, where B invents rows A does not. The mechanism is visible in the two
// formulas: B's floor is nu * mu_hat, an ABSOLUTE quantity in dual units, so
// it does not follow ||lambda_i||inf down as the mesh refines; A's second term
// does exactly that, and it is the term that dominates on every converged
// hand-off (at N = 400, mu = 1e-9: 2.005e-6 against B's 1e-8). B is thus the
// old defect in a new place -- an absolute floor next to a shrinking dual --
// which is the reason to reject it beyond the counts.
TEST(WarmStart, CrossoverActivityRepairBakeOffRecordsTheRejectedCandidate) {
    using namespace ip_repair;
    struct Cell {
        Index nodes;
        double mu;
        Index recovered; // both candidates, equal by measurement
        Index a_false, b_false;
    };
    const std::vector<Cell> cells = {
        {100, 1e-9, 92, 0, 0},
        {400, 1e-9, 370, 0, 2},
        {400, 1e-8, 370, 2, 4},
        {1600, 1e-9, 1484, 0, 0},
    };

    for (const Cell &cell : cells) {
        SCOPED_TRACE(fmt::format("N = {} nodes, mu = {:g}", cell.nodes, cell.mu));
        F7CollocationChain model(cell.nodes, 3, 2, kP, 1.0);
        const IpIterate it = f7_ip_iterate(model, kP, cell.mu, kDisp);

        const Score a = score_crossover(model, kP, it);
        const Score b = score_candidate_b(model, kP, it, /*dual_tol=*/1e-6);
        EXPECT_EQ(a.recovered, cell.recovered) << "THE PIN (observed), candidate A";
        EXPECT_EQ(b.recovered, cell.recovered) << "THE PIN (observed), candidate B";
        EXPECT_EQ(a.false_active, cell.a_false) << "THE PIN (observed), candidate A";
        EXPECT_EQ(b.false_active, cell.b_false) << "THE PIN (observed), candidate B";
        EXPECT_LE(a.false_active, b.false_active)
            << "the adoption criterion, re-derived: A never invents a row B does not";
    }
}

// (c) THE EMPTY-INFERENCE CASE. A hand-off whose prices are all at the noise
// floor infers NOTHING -- and that must be an ordinary, quiet outcome: a valid
// object, an empty hint, a kSeeded resolution (values without provenance is
// still what the object is), and no throw. This is the case the repair could
// most easily have broken, since the repaired rule divides the population into
// "live" and "noise" using the population's own scale, and a population that
// is ALL noise has a scale too.
//
// It also pins `SqpCounters::ip_activity_inferred` on both sides: 0 here,
// nonzero on the crossover that does infer (the second half below).
TEST(WarmStart, CrossoverAllZeroDualsInferNothingAndStillResolveSeeded) {
    const auto p = make_hs(14);
    ASSERT_EQ(p.model->mi(), 1);

    Vec lambda_i_noise(1);
    lambda_i_noise(0) = 1.0e-12; // strictly positive, four decades under dual_tol
    Vec slack_i(1);
    slack_i(0) = -1.0e-9;
    const Vec z_lower = Vec::Zero(p.model->n());
    const Vec z_upper = Vec::Zero(p.model->n());
    const Vec x0 = p.model->start_point();

    WarmStart crossover;
    ASSERT_NO_THROW(crossover =
                        from_interior_point(x0, Vec::Zero(p.model->me()), lambda_i_noise, slack_i,
                                            z_lower, z_upper, p.model->lower(), p.model->upper()));
    EXPECT_TRUE(crossover.valid) << "an inference that finds nothing is not a failure";
    EXPECT_EQ(crossover.ineq_active, (std::vector<std::uint8_t>{0}));
    EXPECT_TRUE(crossover.qp_working_set.active_ineq().empty());
    EXPECT_EQ(crossover.bound_active, (std::vector<std::int8_t>{0, 0}));

    SqpOptions opts;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model, crossover.x, crossover);
    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_EQ(sol.counters.start_level_used, StartLevel::kSeeded)
        << "an empty hint costs the hint, never the level";
    EXPECT_EQ(sol.counters.n_seeded, 1);
    EXPECT_EQ(sol.counters.ip_activity_inferred, 0)
        << "THE PIN: nothing was proposed, and the counter says so rather than staying silent";

    // The other side of the counter: the Phase-4 crossover recipe on the same
    // problem, whose one row IS live, reports the hint it handed over.
    SqpDriver seed_driver(opts);
    const SqpSolution seed = seed_driver.solve(*p.model);
    ASSERT_EQ(seed.status, SqpStatus::kOptimal);
    ASSERT_GT(seed.lambda_i(0), 0.0);
    Vec live_slack(1);
    live_slack(0) = -1.0e-9 / seed.lambda_i(0);
    const WarmStart live =
        from_interior_point(seed.x, seed.lambda_e, seed.lambda_i, live_slack, z_lower, z_upper,
                            p.model->lower(), p.model->upper());
    ASSERT_EQ(live.qp_working_set.active_ineq().size(), 1u);
    SqpDriver live_driver(opts);
    const SqpSolution live_sol = live_driver.solve(*p.model, live.x, live);
    EXPECT_EQ(live_sol.counters.start_level_used, StartLevel::kSeeded);
    EXPECT_EQ(live_sol.counters.ip_activity_inferred, 1)
        << "THE PIN: HS14's one active row, offered and counted";

    // AND THE SCOPING, which is the counter's whole contract: a kWarm ingest
    // carries an activity hint too -- this one pins the same single row -- and
    // must still report 0, because at kWarm the hint's provenance is confirmed
    // and its size answers no question a reader has (SqpCounters'
    // ip_activity_inferred note). Without this arm the scoping is unpinned.
    SqpDriver warm_driver(opts);
    ASSERT_FALSE(seed.warm_start.qp_working_set.active_ineq().empty())
        << "the control arm needs a NON-empty hint to be worth anything";
    const SqpSolution warm_sol =
        warm_driver.solve(*p.model, p.model->start_point(), seed.warm_start);
    ASSERT_EQ(warm_sol.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(warm_sol.counters.ip_activity_inferred, 0)
        << "THE PIN: identically 0 above kSeeded, hint or no hint";
}

// THE EXTREME-DUAL TRAP, CLOSED AND PINNED. warm_start.h documented this from
// Phase 4 as a defect it could not fix: under the OLD rule
// `|slack_i(j)| < activity_tol * max(1, |lambda_i(j)|)`, a hand-off pricing a
// row at 1e6 against a residual of 0.9 is certified ACTIVE, because
// activity_tol * 1e6 == 1 at the old 1e-6 default. The header could only tell
// callers not to do it ("only feed this function points that are already
// near-complementary"), since the rule had no independent measure of how
// converged the caller's iterate was.
//
// THE REPAIRED RULE HAS ONE -- `mu_hat` -- AND IT CLOSES THE TRAP FOR FREE: a
// hand-off that inconsistent raises its own bar (mu_hat = 9e5, threshold
// 9e6), so the 1e6 price is left FREE. Nothing was added for this case; it
// falls out of reading the slack aggregately instead of per row.
//
// THE PADDING IS THE POINT OF THE SECOND HALF. `mu_hat` is a mean over the
// PRICED rows, not over all of them, and 999 unpriced rows are exactly what
// tells the two apart: averaged over all 1000 the same hand-off reports
// mu_hat = 9e2, a threshold of 9e3, and the trap re-opens. A model's SIZE must
// not decide whether its hand-off is trusted.
TEST(WarmStart, CrossoverExtremeDualAgainstALargeResidualIsRefused) {
    const Vec x = Vec::Zero(1);
    const Vec lower = Vec::Constant(1, -1.0e20);
    const Vec upper = Vec::Constant(1, 1.0e20);
    const Vec z_lower = Vec::Zero(1);
    const Vec z_upper = Vec::Zero(1);

    // (i) the trap verbatim: one row, an extreme price, a residual nowhere
    // near zero. The OLD rule read 0.9 < 1e-6 * 1e6 == 1 and said ACTIVE.
    {
        Vec lambda_i(1);
        lambda_i(0) = 1.0e6;
        Vec slack_i(1);
        slack_i(0) = -0.9;
        const WarmStart crossover =
            from_interior_point(x, Vec(0), lambda_i, slack_i, z_lower, z_upper, lower, upper);
        EXPECT_EQ(crossover.ineq_active, (std::vector<std::uint8_t>{0}))
            << "THE PIN: an inconsistent hand-off raises its own bar and the row stays FREE";
        EXPECT_TRUE(crossover.qp_working_set.active_ineq().empty());
    }

    // (ii) the same hand-off padded with 999 rows the producer left unpriced.
    // The verdict must not move: mu_hat is a property of the hand-off's priced
    // rows, not of the model's row count.
    {
        constexpr Index kRows = 1000;
        Vec lambda_i = Vec::Zero(kRows);
        lambda_i(0) = 1.0e6;
        Vec slack_i = Vec::Constant(kRows, -1.0);
        slack_i(0) = -0.9;
        const WarmStart crossover =
            from_interior_point(x, Vec(0), lambda_i, slack_i, z_lower, z_upper, lower, upper);
        ASSERT_EQ(crossover.ineq_active.size(), static_cast<std::size_t>(kRows));
        EXPECT_EQ(crossover.ineq_active[0], 0)
            << "THE PIN: unchanged by 999 unpriced rows -- averaging over ALL of them would put "
               "mu_hat at 9e2, the threshold at 9e3, and re-open the trap";
        EXPECT_TRUE(crossover.qp_working_set.active_ineq().empty());
    }
}

// THE BOUND HALF, AT COLLOCATION SCALE -- the twin of the row defect, pinned.
//
// THIS ARM EXISTS BECAUSE THE FIRST ROUND'S REASONING FOR NOT WRITING IT WAS
// WRONG, and the correction is worth stating: the repair note originally
// recorded the bound-half mutation (revert the bound rule to the absolute test,
// leaving the rows repaired) as UNFIXTURABLE until Phase 7's kBoundArc corpus
// cells, on the argument that no MODEL in this tree presents an active variable
// bound at collocation scale -- which is true (F7's control box is inactive at
// every p in the design range, kControlAmp = 0.5 < kControlBound = 1.0, and the
// small families' bound duals are O(1) where old and new rules agree). But the
// bound half needs no model: from_interior_point takes raw vectors, and
// CrossoverExtremeDualAgainstALargeResidualIsRefused above is already a fully
// synthetic hand-off that this suite accepts as a mutation kill. The same
// standard applied here kills the bound mutation in a dozen lines, one task
// earlier than the corpus.
//
// THE FIXTURE IS THE ROW DEFECT'S GEOMETRY IN BOUND UNITS: an IP hand-off at
// mu = 1e-8 whose live bound price is O(h)-small (1e-3) and whose gap is
// therefore mu/z = 1e-5, next to a second variable carrying only barrier noise
// (z = 1e-8, gap 1.0). Repaired: threshold
// = 10 * max(mu_hat = 1e-8, z_inf * 1e-4 = 1e-7) = 1e-6, so the 1e-3 price is
// ACTIVE and the 1e-8 one is not. Pre-repair: the absolute test asks
// gap = 1e-5 < 1e-6 * max(1, 1e-3) = 1e-6, which FAILS, and the live bound is
// reported FREE -- the empty-hint outcome the whole task exists to repair.
//
// BOTH SIDES ARE EXERCISED because each estimates its OWN mu_hat and dual norm
// (warm_start.h's note on why the two are separate populations): the second
// block mirrors the first onto the upper bound and must read kAtUpper.
TEST(WarmStart, CrossoverBoundActivityIsInferredAtCollocationScale) {
    constexpr double kMu = 1.0e-8;
    constexpr double kLivePrice = 1.0e-3; // an O(h) bound dual, the defect's scale
    constexpr double kNoisePrice = 1.0e-8;

    // ---- the LOWER side ----
    {
        Vec x(2);
        x(0) = kMu / kLivePrice; // 1e-5: the central path's own gap at that price
        x(1) = kMu / kNoisePrice;
        const Vec lower = Vec::Zero(2);
        const Vec upper = Vec::Constant(2, 1.0e20);
        Vec z_lower(2);
        z_lower << kLivePrice, kNoisePrice;
        const Vec z_upper = Vec::Zero(2);

        const WarmStart crossover =
            from_interior_point(x, Vec(0), Vec(0), Vec(0), z_lower, z_upper, lower, upper);
        ASSERT_EQ(crossover.bound_active.size(), 2u);
        EXPECT_EQ(crossover.bound_active[0], -1)
            << "THE PIN: an O(h) bound price against its own mu/z gap reads ACTIVE at the lower "
               "bound -- the pre-repair absolute rule reported FREE here";
        EXPECT_EQ(crossover.qp_working_set.bound_state()[0], BoundState::kAtLower);
        EXPECT_EQ(crossover.bound_active[1], 0)
            << "THE PIN: and the barrier-noise price stays FREE (dual_tol)";
        EXPECT_EQ(crossover.qp_working_set.bound_state()[1], BoundState::kFree);
    }

    // ---- the UPPER side, mirrored ----
    {
        Vec x(2);
        x(0) = 1.0 - kMu / kLivePrice;
        x(1) = 1.0 - kMu / kNoisePrice;
        const Vec lower = Vec::Constant(2, -1.0e20);
        const Vec upper = Vec::Constant(2, 1.0);
        const Vec z_lower = Vec::Zero(2);
        Vec z_upper(2);
        z_upper << kLivePrice, kNoisePrice;

        const WarmStart crossover =
            from_interior_point(x, Vec(0), Vec(0), Vec(0), z_lower, z_upper, lower, upper);
        ASSERT_EQ(crossover.bound_active.size(), 2u);
        EXPECT_EQ(crossover.bound_active[0], 1) << "THE PIN: the mirror reads kAtUpper";
        EXPECT_EQ(crossover.qp_working_set.bound_state()[0], BoundState::kAtUpper);
        EXPECT_EQ(crossover.bound_active[1], 0);
    }
}

// THE FALSE-ACTIVE GUARD, AT THE BARRIER LEVEL A REAL CROSSOVER ARRIVES AT.
//
// The refinement arm above measures `mu = 1e-9`, where nothing is invented at
// any mesh -- but the level the shipped bridge hands over at is `mu = 1e-8`
// (the IPM engine's own bar_tol default, and the level
// docs/notes/2026-08-04-kseeded-ingest.md section 6.2 measured the defect at).
// One decade is the whole difference, and this arm is where it shows.
//
// MEASURED (MKL Pardiso, clang++ Release; pure arithmetic on analytic data, so
// backend-independent), recovered / FALSE-ACTIVE:
//
//   N      analytic   mu = 1e-9        mu = 1e-8
//   1600   1486       1484 /  0        1484 / 12
//   3200   2972       2964 /  2        2964 / 24
//   6400   5946       5916 /  4        5916 / 46
//
// RECALL IS UNAFFECTED BY mu; ONLY THE FALSE-ACTIVE COLUMN MOVES, and the
// mechanism is the same defect class this task repaired, seen from the other
// side. The rule's own threshold is 10 * ||lambda_i||inf * 1e-4, which follows
// the prices down under refinement -- 5.00e-7 at N = 1600 -- and therefore
// falls BELOW the absolute `dual_tol` of 1e-6 somewhere around N ~ 800. Past
// that mesh `dual_tol` is the ONLY thing holding the barrier-noise prices out,
// and it is absolute while those prices are mu/|cI| and scale with mu: at
// N = 1600 the loudest is 8.50e-7 when mu = 1e-9 (held) and 8.50e-6 when
// mu = 1e-8 (not held). The guard therefore fails about one decade of mu away
// from where it was measured, and the boundary has a closed form:
//
//   false actives appear once  mu  >  max(rule threshold, dual_tol) * min |cI|
//                                     over the strictly slack rows
//
// which at N >= 800 is just `mu > dual_tol * min|cI|` -- 1.2e-9 at N = 1600.
//
// THE DEGRADATION IS MILD AND IS NOT A CLIFF: the false-active FRACTION of the
// hint stays near 0.8 % at every mesh (12/1496, 24/2988, 46/5962), so the count
// grows with N only because the hint does. The rule is not redesigned to chase
// these cells -- see the note's section 5.3 for why the suppression question
// stays deferred to corpus telemetry -- but Task 1's crossover cells at fine
// meshes must either hand over at mu <= 1e-9 or carry a hint-quality tag, and
// that is what this arm exists to make impossible to forget.
TEST(WarmStart, CrossoverFalseActiveGuardDegradesAtTheOperativeBarrierLevel) {
    using namespace ip_repair;
    struct Arm {
        Index nodes;
        Index analytic, recovered;
        Index false_at_1e9, false_at_1e8;
    };
    const std::vector<Arm> arms = {
        {1600, 1486, 1484, 0, 12},
        {3200, 2972, 2964, 2, 24},
        {6400, 5946, 5916, 4, 46},
    };

    for (const Arm &arm : arms) {
        SCOPED_TRACE(fmt::format("N = {} nodes (nx = {})", arm.nodes, 5 * arm.nodes));
        F7CollocationChain model(arm.nodes, 3, 2, kP, 1.0);

        const Score tight = score_crossover(model, kP, f7_ip_iterate(model, kP, 1e-9, kDisp));
        EXPECT_EQ(tight.analytic, arm.analytic);
        EXPECT_EQ(tight.recovered, arm.recovered) << "THE PIN (observed), mu = 1e-9";
        EXPECT_EQ(tight.false_active, arm.false_at_1e9) << "THE PIN (observed), mu = 1e-9";

        const Score operative = score_crossover(model, kP, f7_ip_iterate(model, kP, 1e-8, kDisp));
        EXPECT_EQ(operative.recovered, arm.recovered)
            << "THE PIN (observed): recall does not move with mu -- only the guard does";
        EXPECT_EQ(operative.false_active, arm.false_at_1e8) << "THE PIN (observed), mu = 1e-8";
        ASSERT_GT(operative.inferred, 0);
        EXPECT_LE(static_cast<double>(operative.false_active) /
                      static_cast<double>(operative.inferred),
                  0.01)
            << "the degradation is mild: the false-active FRACTION stays under 1 % at every mesh, "
               "so the count grows with N only because the hint does";
    }

    // THE MECHANISM, EXECUTABLE, at the first mesh where it bites: the rule's
    // own threshold has fallen below dual_tol, so dual_tol is the sole
    // false-active guard -- and at mu = 1e-8 the loudest barrier-noise price
    // has risen above it.
    F7CollocationChain fine(1600, 3, 2, kP, 1.0);
    const IpIterate it = f7_ip_iterate(fine, kP, 1e-8, kDisp);
    const auto analytic = fine.active_set(kP);
    double dual_inf = 0.0, noise_max = 0.0;
    for (Index j = 0; j < fine.mi(); ++j) {
        dual_inf = std::max(dual_inf, it.lambda_i(j));
        if (analytic.ineq_active[static_cast<std::size_t>(j)] == 0) {
            noise_max = std::max(noise_max, it.lambda_i(j));
        }
    }
    const IpCrossoverOptions defaults;
    const double threshold =
        kIpActivityFactor * std::max(1e-8, dual_inf * defaults.activity_rel_tol);
    EXPECT_NEAR(threshold, 5.003e-7, 1e-10) << "THE PIN (observed): the rule's own floor";
    EXPECT_LT(threshold, defaults.dual_tol)
        << "past N ~ 800 the rule's floor is BELOW dual_tol, which is what makes dual_tol the sole "
           "false-active guard at fine meshes";
    EXPECT_NEAR(noise_max, 8.497e-6, 1e-9) << "THE PIN (observed): mu/min|cI| on a slack row";
    EXPECT_GT(noise_max, defaults.dual_tol)
        << "and at the operative barrier level the guard has stopped guarding -- the 12 false "
           "actives above are exactly the rows in this band";
}

// =====================================================================
// PHASE-7 TASK 5: THE PROXIMAL CARRY IS A HASH-GATED WARM FIELD.
//
// `WarmStart::prox_center_x` / `prox_center_lambda` / `prox_sigma`, gated by
// `has_prox_center`, carry the semismooth-Newton kernel's proximal level
// across solves. This file owns the INGEST RULE for every warm field, so it
// owns this one: the carry sits behind `warm_state_ingest`
// (StartLevel::kWarm and above), exactly like `tr_radius`, `funnel_width`,
// `primal_delta` and `dual_mu`, and for exactly their reason -- a proximal
// level is a statement about how hard ONE model's subproblems were, so an
// object that cannot say which model it came from may not set it.
//
// THE LEVER ITSELF (`SqpOptions::ssn_prox_carry`, default false) and the sweep
// behind its default are tests/test_sqp_driver.cpp's SsnProxCarry arms. What
// is asserted here is the GATE, in both directions, and the emission contract.
// =====================================================================
TEST(WarmStartProxCarry, TheCarryIsHashGatedAndIsNeverEmittedByAWalkSolve) {
    using hven::solvers::test_support::make_hs;

    // (1) A kWalk SOLVE EMITS NOTHING. This is the byte-identity property
    // restated as an assertion: at the shipped default no SSN subproblem runs,
    // so `has_prox_center` is false and both vectors are empty on every object
    // this project emits today.
    auto walk_problem = make_hs(27);
    SqpOptions walk_opts;
    walk_opts.max_iter = 60;
    SqpDriver walk_driver(walk_opts);
    const SqpSolution walk = walk_driver.solve(*walk_problem.model);
    ASSERT_EQ(SqpStatus::kOptimal, walk.status);
    EXPECT_FALSE(walk.warm_start.has_prox_center);
    EXPECT_DOUBLE_EQ(0.0, walk.warm_start.prox_sigma);
    EXPECT_EQ(0, walk.warm_start.prox_center_x.size());
    EXPECT_EQ(0, walk.warm_start.prox_center_lambda.size());

    // (2) A kSsn SOLVE WHOSE LADDER ARMED EMITS THE FULL BLOCK, sized to the
    // model.
    auto ssn_problem = make_hs(27);
    SqpOptions ssn_opts = walk_opts;
    ssn_opts.qp_mode = QpMode::kSsn;
    SqpDriver ssn_driver(ssn_opts);
    const SqpSolution ssn = ssn_driver.solve(*ssn_problem.model);
    ASSERT_EQ(SqpStatus::kOptimal, ssn.status);
    ASSERT_TRUE(ssn.warm_start.has_prox_center) << "fixture premise: HS27's ladder arms";
    EXPECT_GT(ssn.warm_start.prox_sigma, 0.0);
    // FIX ROUND 1: THE LEVEL AND THE CENTRE ARE TWO DIFFERENT HIGH-WATER MARKS.
    // The level is taken over EVERY SSN subproblem (an exhausted ladder is
    // exactly the evidence the carry transmits); the centre only over CERTIFYING
    // ones, because an escaped iterate is a diverged point by construction. On
    // HS27 the max-sigma subproblem escapes, so the block ships a real level
    // with both vectors empty -- which warm_start.h's own field note admits
    // ("n, or empty"). tests/test_sqp_driver.cpp's
    // TheCarriedProximalCentreIsNeverAnEscapedIterate pins both polarities.
    EXPECT_EQ(0, ssn.warm_start.prox_center_x.size())
        << "HS27's ladder is climbed on a subproblem that then ESCAPES";
    EXPECT_EQ(0, ssn.warm_start.prox_center_lambda.size());

    // A SOLVE WHOSE LADDER ARMS ON A CERTIFYING SUBPROBLEM DOES SHIP THE
    // VECTORS, so the emptiness above is a property of HS27's trajectory and
    // not of the export having been switched off.
    auto certifying_problem = make_hs(7);
    SqpDriver certifying_driver(ssn_opts);
    const SqpSolution certifying = certifying_driver.solve(*certifying_problem.model);
    ASSERT_EQ(SqpStatus::kOptimal, certifying.status);
    ASSERT_TRUE(certifying.warm_start.has_prox_center);
    EXPECT_EQ(certifying_problem.model->n(), certifying.warm_start.prox_center_x.size());
    EXPECT_EQ(certifying_problem.model->me() + certifying_problem.model->mi(),
              certifying.warm_start.prox_center_lambda.size());

    // (3) THE HASH GATE. The SAME object with its hash erased -- which is what
    // mesh_transfer.h and from_interior_point emit by construction -- resolves
    // kSeeded, and a kSeeded object may not set a proximal level. The carry is
    // therefore NOT read, which is observable: the receiving solve reproduces
    // the carry-OFF ladder count exactly.
    auto receiver_problem = make_hs(27);
    WarmStart hashed = ssn.warm_start;
    hashed.x = receiver_problem.model->start_point();
    WarmStart hashless = hashed;
    hashless.structure_hash = 0;

    SqpOptions carry_on = ssn_opts;
    carry_on.ssn_prox_carry = true;

    SqpDriver hashed_driver(carry_on);
    const SqpSolution from_hashed = hashed_driver.solve(
        *receiver_problem.model, receiver_problem.model->start_point(), hashed, 0);
    ASSERT_EQ(StartLevel::kWarm, from_hashed.counters.start_level_used);

    SqpDriver hashless_driver(carry_on);
    const SqpSolution from_hashless = hashless_driver.solve(
        *receiver_problem.model, receiver_problem.model->start_point(), hashless, 0);
    ASSERT_EQ(StartLevel::kSeeded, from_hashless.counters.start_level_used)
        << "fixture premise: erasing the hash drops the object to kSeeded";

    SqpOptions carry_off = ssn_opts;
    SqpDriver control_driver(carry_off);
    const SqpSolution control = control_driver.solve(
        *receiver_problem.model, receiver_problem.model->start_point(), hashed, 0);

    EXPECT_EQ(0, from_hashed.counters.ssn.ssn_prox_updates)
        << "kWarm + lever on: the carry is read and the ladder starts armed";
    EXPECT_EQ(control.counters.ssn.ssn_prox_updates, from_hashless.counters.ssn.ssn_prox_updates)
        << "THE GATE: a kSeeded object's proximal level is refused even with the lever ON, so "
           "the seeded solve costs exactly what a solve with no carry at all costs";
    EXPECT_GT(from_hashless.counters.ssn.ssn_prox_updates,
              from_hashed.counters.ssn.ssn_prox_updates)
        << "and the two levels are distinguishable, which is what makes the gate observable "
           "rather than merely stated";
}

} // namespace
