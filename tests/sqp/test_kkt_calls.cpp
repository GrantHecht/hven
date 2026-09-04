// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <gtest/gtest.h>

#include <hven/detail/kkt/kkt_calls.h>

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using namespace hven::solvers::detail;

namespace {

SpMatRM make_kkt3(double h00 = 2.0) {
    SpMatRM K(3, 3);
    K.insert(0, 0) = h00;
    K.insert(0, 2) = 1.0;
    K.insert(1, 1) = 3.0;
    K.insert(1, 2) = 1.0;
    K.insert(2, 2) = 0.0;
    K.makeCompressed();
    return K;
}

SpMatRM make_kkt3_different_pattern() {
    SpMatRM K(3, 3);
    K.insert(0, 0) = 2.0;
    K.insert(1, 1) = 3.0;
    K.insert(1, 2) = 1.0;
    K.insert(2, 2) = 0.0;
    K.makeCompressed();
    return K;
}

} // namespace

TEST(SqpKktOptions, MatchesTheApprovedKktConfiguration) {
    const hven::linear::SymmetricFactor::Options o = sqp_kkt_options();

    EXPECT_EQ(o.kind, hven::linear::FactorKind::kLDLT);
    EXPECT_EQ(o.num_threads, 0);
    EXPECT_FALSE(o.pivot_perturb_exp.has_value());
    EXPECT_FALSE(o.max_refinement_iters.has_value());
    EXPECT_EQ(o.ordering, hven::linear::SymmetricFactor::Options::Ordering::kBackendDefault);
    EXPECT_FALSE(o.weighted_matching);
    EXPECT_FALSE(o.matrix_scaling);
    EXPECT_EQ(o.pivot_strategy,
              hven::linear::SymmetricFactor::Options::PivotStrategy::kBackendDefault);
    EXPECT_EQ(o.factorization_algorithm,
              hven::linear::SymmetricFactor::Options::FactorizationAlgorithm::kBackendDefault);
    EXPECT_EQ(o.solve_parallelism,
              hven::linear::SymmetricFactor::Options::SolveParallelism::kBackendDefault);
    EXPECT_EQ(o.cnr_threads, 0);
    EXPECT_FALSE(o.collect_factor_mflops);
    EXPECT_FALSE(o.accelerate_zero_tolerance.has_value());
}

TEST(KktFactor, FreshAnalyzeFactorizeAndSolve) {
    KktFactor k;
    const SpMatRM K = make_kkt3();

    EXPECT_FALSE(k.analyzed);
    EXPECT_TRUE(needs_analysis(k, K));
    EXPECT_EQ(k.factor.counters().analyze_count, 0);
    EXPECT_EQ(k.factor.counters().factorize_count, 0);

    const hven::linear::FactorizeOutcome outcome = factorize_checked(k, K);

    ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk);
    EXPECT_TRUE(k.analyzed);
    EXPECT_EQ(k.analyzed_pattern, hven::pattern_hash(K));
    EXPECT_FALSE(needs_analysis(k, K));
    EXPECT_EQ(k.factor.counters().analyze_count, 1);
    EXPECT_EQ(k.factor.counters().factorize_count, 1);

    Vec rhs(3);
    rhs << 1.0, 2.0, 0.5;
    const Vec x = solve_vec(k, rhs);
    const Eigen::MatrixXd dense_k = Eigen::MatrixXd(K).selfadjointView<Eigen::Upper>();
    EXPECT_LT((dense_k * x - rhs).norm(), 1e-10);
    EXPECT_EQ(k.factor.counters().solve_count, 1);
}

TEST(KktFactor, RefactorizesAValueChangeWithoutReanalyzing) {
    KktFactor k;
    const SpMatRM first = make_kkt3();
    const SpMatRM second = make_kkt3(4.0);

    ASSERT_EQ(factorize_checked(k, first).status, hven::linear::FactorizeOutcome::Status::kOk);
    EXPECT_FALSE(needs_analysis(k, second));

    ASSERT_EQ(factorize_checked(k, second).status, hven::linear::FactorizeOutcome::Status::kOk);
    EXPECT_EQ(k.factor.counters().analyze_count, 1);
    EXPECT_EQ(k.factor.counters().factorize_count, 2);
}

TEST(KktFactor, NeedsAnalysisPreservesCallSiteCounting) {
    KktFactor k;
    const SpMatRM first = make_kkt3();
    const SpMatRM same_pattern = make_kkt3(4.0);
    const SpMatRM changed_pattern = make_kkt3_different_pattern();
    Index symbolic_analyses = 0;

    if (needs_analysis(k, first)) {
        ++symbolic_analyses;
    }
    ASSERT_EQ(factorize_checked(k, first).status, hven::linear::FactorizeOutcome::Status::kOk);

    if (needs_analysis(k, same_pattern)) {
        ++symbolic_analyses;
    }
    ASSERT_EQ(factorize_checked(k, same_pattern).status,
              hven::linear::FactorizeOutcome::Status::kOk);

    if (needs_analysis(k, changed_pattern)) {
        ++symbolic_analyses;
    }
    ASSERT_EQ(factorize_checked(k, changed_pattern).status,
              hven::linear::FactorizeOutcome::Status::kOk);

    // B2 review I-2. The engaged-decision path records `analyzed_pattern` from
    // the value `analysis_decision()` computed at the call site rather than
    // recomputing it in `factorize_checked`; nothing previously asserted that
    // VALUE on the changed-pattern (engaged) branch -- only the disengaged
    // branch is pinned above (line 67). Pin both the recorded value and its
    // downstream consequence (no further analysis is needed against the same
    // pattern).
    EXPECT_EQ(k.analyzed_pattern, hven::pattern_hash(changed_pattern));
    EXPECT_FALSE(needs_analysis(k, changed_pattern));

    // B4 M-1, restored. The dissolved KktSystem.PatternHashDetectsChange
    // ended on exactly this re-read -- `EXPECT_EQ(kkt.num_neg_eigs(), 1)`
    // after a cross-pattern re-factorize, with the comment "must re-analyze
    // internally, not corrupt state" on the re-factorize line above it. This
    // test is its successor and inherited the pattern-change coverage, but
    // asserted only the OUTCOME STATUS across the change, not the inertia
    // value; that gap was disclosed at gate B as an assertion-strength
    // regression, and the verdict's rule is that disclosed regressions decay
    // toward zero rather than accumulate.
    //
    // Status alone is the weaker claim: a re-analysis that silently kept the
    // FIRST pattern's symbolic structure could still return kOk while
    // reporting the wrong inertia off a stale factor. The inertia value is
    // what makes "did not corrupt state" checkable -- the same reason the
    // original assertion existed. Read AFTER the third factorize_checked,
    // the cross-pattern one, because that is the call the original's re-read
    // followed (its K2 is this test's changed_pattern, entry for entry).
    const hven::linear::InertiaEvidence after_pattern_change = k.factor.inertia();
    ASSERT_EQ(after_pattern_change.state, hven::linear::InertiaEvidence::State::kObserved);
    EXPECT_EQ(after_pattern_change.n_neg, 1);

    EXPECT_EQ(symbolic_analyses, 2);
    EXPECT_EQ(k.factor.counters().analyze_count, symbolic_analyses);
    EXPECT_EQ(k.factor.counters().factorize_count, 3);
}

// M6 W2 T7 fix 2, and the measurement the eliminated twin's MISS-branch decline
// rests on: what an EXACTLY SINGULAR KKT does under sqp_kkt_options(). It is
// the `dual_mu = 0` case -- a working row dependent on the face, zero (2,2).
TEST(SqpKktOptions, ARankDeficientKktIsPerturbedRatherThanFailedOnThisBackend) {
    // K = [[I, A^T], [A, 0]] with A = [[1, 0], [1, 0]]: rank 1, so K is exactly
    // singular. Upper triangle only, with the structural diagonal the backend
    // requires in every row.
    SpMatRM K(4, 4);
    K.insert(0, 0) = 1.0;
    K.insert(0, 2) = 1.0;
    K.insert(0, 3) = 1.0;
    K.insert(1, 1) = 1.0;
    K.insert(2, 2) = 0.0;
    K.insert(3, 3) = 0.0;
    K.makeCompressed();

    KktFactor k;
#if defined(__APPLE__)
    // UNOBSERVED on Accelerate (CLAUDE.md §6): a backend that reports a
    // singular factorization as an error instead of perturbing would make
    // factorize_checked throw here, which is the twin's decline path.
    (void)k;
#else
    // MKL Pardiso's default static pivoting perturbs the tiny pivots and
    // returns success, so factorize_checked does NOT throw and the twin's
    // decline is NOT reached by this route. The fix-2 report says so.
    ASSERT_NO_THROW(factorize_checked(k, K));
    const hven::linear::InertiaEvidence ev = k.factor.inertia();
    EXPECT_EQ(ev.state, hven::linear::InertiaEvidence::State::kObserved);
    EXPECT_EQ(ev.n_zero, 0);
    EXPECT_GT(ev.perturbed_pivots.value_or(0), 0);
#endif
}
