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

// M6 W5 T8.8 MOVED THIS PIN, and the move is DECLARED: before that task
// `sqp_kkt_options()` took no argument and hard-coded `num_threads = 0`, so the
// pin read "the SQP exposes no per-instance thread control". It now reads "the
// DEFAULT is still 0, and a count handed in is the count that comes out" -- the
// default-argument form keeps every pre-T8.8 caller's configuration bit for bit,
// which is what makes the U0 replay at threads = 0 an identity by construction.
// Every OTHER field is asserted here exactly as it was.
TEST(SqpKktOptions, MatchesTheApprovedKktConfiguration) {
    const hven::linear::SymmetricFactor::Options o = sqp_kkt_options();

    EXPECT_EQ(o.kind, hven::linear::FactorKind::kLDLT);
    EXPECT_EQ(o.num_threads, 0) << "the default argument is the pre-T8.8 hard-coded value";
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

// The other half of the moved pin (M6 W5 T8.8): the count handed in is the
// count that comes out, and nothing else in the configuration moves with it.
// `sqp_kkt_options` is the SOLE options factory for `detail::KktFactor`, so
// this plus `KktFactor`'s own constructor below is the whole plumbing the
// walk/SSN tier's temporaries depend on -- they die inside a call and no
// boundary observation can read their count, so CONSTRUCTION-RULE coverage
// (this pin, plus the report's `grep -n 'KktFactor \w*;' src/ include/`, which
// finds no library site that default-constructs one) is what covers them. An
// HVEN_TESTING construction observer in qp_engine.cpp is REGISTERED for W6 if
// the temporaries are ever to be OBSERVED rather than argued.
TEST(SqpKktOptions, TheThreadCountHandedInIsTheOneThatComesOut) {
    const hven::linear::SymmetricFactor::Options two = sqp_kkt_options(2);
    EXPECT_EQ(two.num_threads, 2);

    const hven::linear::SymmetricFactor::Options zero = sqp_kkt_options(0);
    EXPECT_EQ(zero.num_threads, 0);

    // Nothing but the thread count moves: the two option sets differ in one
    // field, checked on the ones the SQP deliberately pins above.
    EXPECT_EQ(two.kind, zero.kind);
    EXPECT_EQ(two.ordering, zero.ordering);
    EXPECT_EQ(two.pivot_strategy, zero.pivot_strategy);
    EXPECT_EQ(two.factorization_algorithm, zero.factorization_algorithm);
    EXPECT_EQ(two.solve_parallelism, zero.solve_parallelism);
    EXPECT_EQ(two.cnr_threads, zero.cnr_threads);
    EXPECT_EQ(two.pivot_perturb_exp.has_value(), zero.pivot_perturb_exp.has_value());
    EXPECT_EQ(two.max_refinement_iters.has_value(), zero.max_refinement_iters.has_value());
}

// The KktFactor constructor is the single point every walk/SSN-tier factor is
// built through, so this is the plumbing pin for all six library construction
// sites at once (the K0 border, the walk's per-solve local, the EQP-refine
// temporary, the verdict-refine fallback, the SSN tier's persistent factor and
// the parametric predictor's one factorization).
//
// It reads the LIVE factor (`SymmetricFactor::num_threads()` reads through to
// the backend session), not the options struct that configured it.
TEST(KktFactor, IsConstructedAtTheThreadCountItIsGiven) {
    const detail::KktFactor two(2);
    EXPECT_EQ(two.factor.num_threads(), 2);

    const detail::KktFactor defaulted;
    EXPECT_EQ(defaulted.factor.num_threads(), 0)
        << "the default argument is the backend default -- the pre-T8.8 behaviour bit for bit";
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

// M6 W2 T7, and the measurement that says NO decline exists on the eliminated
// twin's MISS branch: what an EXACTLY SINGULAR KKT (`dual_mu = 0`, a working
// row dependent on the face, zero (2,2)) does under sqp_kkt_options().
//
// Red here = qp_engine.h's twin declaration is owed a re-read.
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
    // ABSENT EVIDENCE IS REPORTED ABSENT (CLAUDE.md section 6), never as a
    // green no-op: this has not been run on real Mac hardware.
    (void)k;
    GTEST_SKIP() << "UNOBSERVED on Accelerate: whether it perturbs an exactly singular KKT or "
                    "reports the singularity as an error has not been observed on real Mac "
                    "hardware (CLAUDE.md section 6).";
#else
    // MKL Pardiso's default static pivoting perturbs the tiny pivots and
    // returns success, so factorize_checked does NOT throw -- which is why the
    // twin has no decline to reach and a throw there is a real fault (fix 3).
    ASSERT_NO_THROW(factorize_checked(k, K));
    const hven::linear::InertiaEvidence ev = k.factor.inertia();
    EXPECT_EQ(ev.state, hven::linear::InertiaEvidence::State::kObserved);
    EXPECT_EQ(ev.n_zero, 0);
    EXPECT_GT(ev.perturbed_pivots.value_or(0), 0);
#endif
}
