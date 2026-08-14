#include <gtest/gtest.h>

#include <hven/detail/sqp/kkt_calls.h>

using namespace hven::solvers;
using namespace hven::solvers::detail;

namespace {

SpMatU make_kkt3(double h00 = 2.0) {
    SpMatU K(3, 3);
    K.insert(0, 0) = h00;
    K.insert(0, 2) = 1.0;
    K.insert(1, 1) = 3.0;
    K.insert(1, 2) = 1.0;
    K.insert(2, 2) = 0.0;
    K.makeCompressed();
    return K;
}

SpMatU make_kkt3_different_pattern() {
    SpMatU K(3, 3);
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
    const SpMatU K = make_kkt3();

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
    const SpMatU first = make_kkt3();
    const SpMatU second = make_kkt3(4.0);

    ASSERT_EQ(factorize_checked(k, first).status, hven::linear::FactorizeOutcome::Status::kOk);
    EXPECT_FALSE(needs_analysis(k, second));

    ASSERT_EQ(factorize_checked(k, second).status, hven::linear::FactorizeOutcome::Status::kOk);
    EXPECT_EQ(k.factor.counters().analyze_count, 1);
    EXPECT_EQ(k.factor.counters().factorize_count, 2);
}

TEST(KktFactor, NeedsAnalysisPreservesCallSiteCounting) {
    KktFactor k;
    const SpMatU first = make_kkt3();
    const SpMatU same_pattern = make_kkt3(4.0);
    const SpMatU changed_pattern = make_kkt3_different_pattern();
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

    EXPECT_EQ(symbolic_analyses, 2);
    EXPECT_EQ(k.factor.counters().analyze_count, symbolic_analyses);
    EXPECT_EQ(k.factor.counters().factorize_count, 3);
}
