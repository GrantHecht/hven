// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_symmetric_factor_refusals.cpp — the SymmetricFactor surface's refusals, asserted by
// exception type AND message so a refusal that moves to the wrong reason is caught: the
// Options ranges, every analyze() input rule, the call-order rules (factorize / solve before
// analyze or factorize), the shape rules of every solve entry, the partial-solve refusals,
// factorize_equilibrated's and retake_on_platform's dense-only refusals, and the state a
// refused call leaves behind. MKL-only (the platform-neutral surface is covered by
// test_symmetric_factor_dense_arm.cpp and the Apple suite).

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Sparse>
#include <gtest/gtest.h>

#include <hven/linear/symmetric_factor.h>

namespace hven::linear {
namespace {

using Options = SymmetricFactor::Options;

SpMatRM upper_csr(const Mat &values) {
    const Index n = values.rows();
    std::vector<Eigen::Triplet<double>> triplets;
    for (Index i = 0; i < n; ++i) {
        for (Index j = i; j < n; ++j) {
            if (i == j || values(i, j) != 0.0) {
                triplets.emplace_back(static_cast<int>(i), static_cast<int>(j), values(i, j));
            }
        }
    }
    SpMatRM A(n, n);
    A.setFromTriplets(triplets.begin(), triplets.end());
    A.makeCompressed();
    return A;
}

Mat spd4() {
    Mat A(4, 4);
    A << 2, -1, 0, 0, /**/ -1, 2, -1, 0, /**/ 0, -1, 2, -1, /**/ 0, 0, -1, 2;
    return A;
}

// Runs f and expects exception type E whose message contains every fragment.
template <class E, class F> void expect_throw(F &&f, std::initializer_list<const char *> fragments) {
    try {
        f();
        ADD_FAILURE() << "expected an exception";
    } catch (const E &e) {
        const std::string message(e.what());
        for (const char *fragment : fragments) {
            EXPECT_NE(message.find(fragment), std::string::npos)
                << "missing '" << fragment << "' in: " << message;
        }
    }
}

template <class F> void expect_invalid(F &&f, std::initializer_list<const char *> fragments) {
    expect_throw<std::invalid_argument>(std::forward<F>(f), fragments);
}
template <class F> void expect_logic(F &&f, std::initializer_list<const char *> fragments) {
    expect_throw<std::logic_error>(std::forward<F>(f), fragments);
}

// A factor analyzed and factorized on spd4().
struct Ready {
    SymmetricFactor factor{Options{}};
    SpMatRM A = upper_csr(spd4());
    Ready() {
        factor.analyze(A);
        const FactorizeOutcome out = factor.factorize(A);
        EXPECT_EQ(out.status, FactorizeOutcome::Status::kOk);
    }
};

// ---------------------------------------------------------------------------
// Options ranges: refused at construction, by name, with the offending value.
// ---------------------------------------------------------------------------

TEST(SymmetricFactorRefusals, NegativeRangesAreRefusedAtConstructionByName) {
    Options o;
    o.pivot_perturbation_exponent = -1;
    expect_invalid([&] { SymmetricFactor f(o); },
                   {"pivot_perturbation_exponent must be >= 0", "got -1"});
    o = {};
    o.max_refinement_steps = -3;
    expect_invalid([&] { SymmetricFactor f(o); }, {"max_refinement_steps must be >= 0", "got -3"});
    o = {};
    o.conditional_reproducibility_threads = -2;
    expect_invalid([&] { SymmetricFactor f(o); },
                   {"conditional_reproducibility_threads must be >= 0", "got -2"});
    o = {};
    o.threads = -4;
    expect_invalid([&] { SymmetricFactor f(o); }, {"SymmetricFactor", "num_threads must be >= 0", "got -4"});
}

TEST(SymmetricFactorRefusals, NulloptKnobsMeanLeaveTheBackendDefaultAndAreAccepted) {
    Options o;
    o.pivot_perturbation_exponent = std::nullopt;
    o.max_refinement_steps = std::nullopt;
    EXPECT_NO_THROW(SymmetricFactor f(o));
    o.pivot_perturbation_exponent = 0; // zero is a legal exponent
    o.max_refinement_steps = 0;
    EXPECT_NO_THROW(SymmetricFactor f(o));
}

TEST(SymmetricFactorRefusals, AnAccelerateOnlyToleranceIsRefusedOnTheMklBackendWithItsValue) {
    Options o;
    o.accelerate_zero_tol = 1e-12;
    expect_invalid([&] { SymmetricFactor f(o); },
                   {"accelerate_zero_tol is an Accelerate-only option", "std::nullopt", "1e-12"});
}

TEST(SymmetricFactorRefusals, AnUnknownDecompositionIsALogicErrorNamingTheValue) {
    Options o;
    o.decomposition = static_cast<Decomposition>(7);
    expect_logic([&] { SymmetricFactor f(o); }, {"decomposition 7", "kLdlt is the one"});
}

TEST(SymmetricFactorRefusals, EnumValuesOutsideTheirRangeAreRefusedWhenTheSessionIsBuilt) {
    // The enumerations are mapped to backend codes by an explicit switch, so a value cast in
    // from outside the declared set is refused by name rather than written to the backend.
    const SpMatRM A = upper_csr(spd4());
    {
        Options o;
        o.ordering = static_cast<Options::Ordering>(42);
        SymmetricFactor f(o);
        expect_invalid([&] { f.analyze(A); }, {"unknown Options::Ordering value (42)"});
    }
    {
        Options o;
        o.pivot_strategy = static_cast<Options::PivotStrategy>(42);
        SymmetricFactor f(o);
        expect_invalid([&] { f.analyze(A); }, {"unknown Options::PivotStrategy value (42)"});
    }
    {
        Options o;
        o.solve_parallelism = static_cast<Options::SolveParallelism>(42);
        SymmetricFactor f(o);
        expect_invalid([&] { f.analyze(A); }, {"unknown Options::SolveParallelism value (42)"});
    }
    {
        Options o;
        o.factorization_algorithm = static_cast<Options::FactorizationAlgorithm>(42);
        SymmetricFactor f(o);
        expect_invalid([&] { f.analyze(A); },
                       {"unknown Options::FactorizationAlgorithm value (42)"});
    }
}

TEST(SymmetricFactorRefusals, MatrixScalingNamesTheMissingWeightedMatching) {
    Options o;
    o.matrix_scaling = true;
    expect_invalid([&] { SymmetricFactor f(o); },
                   {"matrix_scaling == true requires weighted_matching == true",
                    "weighted_matching = false"});
}

TEST(SymmetricFactorRefusals, TwoLevelFactorizationNamesTheOrderingAndTheScalingItExcludes) {
    Options o;
    o.factorization_algorithm = Options::FactorizationAlgorithm::kTwoLevel;
    o.ordering = Options::Ordering::kMinimumDegree;
    expect_invalid([&] { SymmetricFactor f(o); },
                   {"kTwoLevel requires ordering == kNestedDissection or kParallelNestedDissection"});
    o.ordering = Options::Ordering::kNestedDissection;
    o.weighted_matching = true;
    expect_invalid([&] { SymmetricFactor f(o); },
                   {"kTwoLevel requires matrix_scaling == false and weighted_matching"});
}

// ---------------------------------------------------------------------------
// analyze(): every input rule, by message; a refused analyze changes nothing.
// ---------------------------------------------------------------------------

TEST(SymmetricFactorRefusals, AnalyzeRefusesAnUncompressedMatrixAndSaysHowToFixIt) {
    SymmetricFactor f{Options{}};
    SpMatRM A = upper_csr(spd4());
    A.uncompress();
    expect_invalid([&] { f.analyze(A); },
                   {"SymmetricFactor::analyze", "must be compressed", "makeCompressed()",
                    "4x4", "stored entries"});
    EXPECT_EQ(f.counters().analyze_count, 0);
}

TEST(SymmetricFactorRefusals, AnalyzeRefusesNonSquareEmptyAndLowerTriangleInputs) {
    SymmetricFactor f{Options{}};
    expect_invalid([&] { f.analyze(SpMatRM(3, 4)); }, {"must be square, got 3x4"});

    SpMatRM empty(0, 0);
    empty.makeCompressed();
    expect_invalid([&] { f.analyze(empty); }, {"must be non-empty", "0x0"});

    // An entry below the diagonal: the surface takes the upper triangle only.
    std::vector<Eigen::Triplet<double>> t = {{0, 0, 1.0}, {1, 0, 0.5}, {1, 1, 1.0}};
    SpMatRM lower(2, 2);
    lower.setFromTriplets(t.begin(), t.end());
    lower.makeCompressed();
    expect_invalid([&] { f.analyze(lower); },
                   {"stores an entry at (1, 0)", "below the diagonal", "UPPER TRIANGLE"});

    // A row with no structural diagonal, even though the value would be zero.
    std::vector<Eigen::Triplet<double>> u = {{0, 0, 1.0}, {0, 1, 0.5}, {1, 2, 1.0}, {2, 2, 1.0}};
    SpMatRM no_diag(3, 3);
    no_diag.setFromTriplets(u.begin(), u.end());
    no_diag.makeCompressed();
    expect_invalid([&] { f.analyze(no_diag); },
                   {"row 1 has no diagonal entry", "insert an explicit zero"});
    EXPECT_EQ(f.counters().analyze_count, 0);
}

// ---------------------------------------------------------------------------
// Call order.
// ---------------------------------------------------------------------------

TEST(SymmetricFactorRefusals, FactorizeBeforeAnalyzeIsALogicErrorForEveryFactorizeEntry) {
    SymmetricFactor f{Options{}};
    const SpMatRM A = upper_csr(spd4());
    expect_logic([&] { f.factorize(A); }, {"SymmetricFactor::factorize", "called before analyze()"});
    expect_logic([&] { f.factorize(A, SymmetricFactor::PatternCheck::kAssumeAnalyzed); },
                 {"called before analyze()"});
    const std::vector<double> scale(4, 1.0);
    expect_logic([&] { f.factorize_equilibrated(A, 2, scale.data(), 0.0); },
                 {"factorize_equilibrated", "called before analyze()"});
    expect_logic([&] { f.retake_on_platform(A); }, {"retake_on_platform", "called before analyze()"});
    EXPECT_EQ(f.counters().factorize_count, 0);
}

TEST(SymmetricFactorRefusals, SolveBeforeAFactorizationIsALogicErrorAndCountsNothing) {
    SymmetricFactor f{Options{}};
    const SpMatRM A = upper_csr(spd4());
    f.analyze(A); // analyzed, never factorized
    Vec b = Vec::Ones(4);
    Vec x(4);
    expect_logic([&] { f.solve(b, x); }, {"solve"});
    Mat B = Mat::Ones(4, 2);
    Mat X(4, 2);
    expect_logic([&] { f.solve(B, X); }, {"solve"});
    expect_logic([&] { f.solve_partial(SymmetricFactor::SolvePhase::kForward, b, x); },
                 {"solve_partial"});
    EXPECT_EQ(f.counters().solve_count, 0);
    EXPECT_EQ(f.counters().partial_solve_count, 0);
    EXPECT_FALSE(f.supports_partial_solve());
}

TEST(SymmetricFactorRefusals, FactorizeRefusesAnUncompressedMatrixEvenWhenThePatternCheckIsSkipped) {
    Ready r;
    SpMatRM A = r.A;
    A.uncompress();
    const Index verifies = r.factor.counters().pattern_verify_count;
    expect_invalid([&] { r.factor.factorize(A); }, {"SymmetricFactor::factorize", "must be compressed"});
    expect_invalid(
        [&] { r.factor.factorize(A, SymmetricFactor::PatternCheck::kAssumeAnalyzed); },
        {"must be compressed"});
    // The storage refusal precedes the pattern walk, so a refused call is not counted as a
    // verification, and the factorization already held is untouched.
    EXPECT_EQ(r.factor.counters().pattern_verify_count, verifies);
    Vec b = Vec::Ones(4);
    Vec x(4);
    EXPECT_NO_THROW(r.factor.solve(b, x));
    EXPECT_LE((Vec(spd4() * x) - b).lpNorm<Eigen::Infinity>(), 1e-12);
}

// ---------------------------------------------------------------------------
// Solve shapes: vector, matrix and partial entries each name what disagreed.
// ---------------------------------------------------------------------------

TEST(SymmetricFactorRefusals, EverySolveEntryNamesTheShapeThatDisagreed) {
    Ready r;
    Vec b3 = Vec::Ones(3);
    Vec b4 = Vec::Ones(4);
    Vec x3(3);
    Vec x4(4);
    expect_invalid([&] { r.factor.solve(b3, x3); },
                   {"SymmetricFactor::solve", "right-hand side has 3 entries", "dimension is 4"});
    expect_invalid([&] { r.factor.solve(b4, x3); },
                   {"solution has 3 entries but the right-hand side has 4"});

    Mat B3 = Mat::Ones(3, 2);
    Mat B4 = Mat::Ones(4, 2);
    Mat X42(4, 2);
    Mat X43(4, 3);
    Mat X32(3, 2);
    expect_invalid([&] { r.factor.solve(B3, X32); },
                   {"right-hand side has 3 rows", "dimension is 4"});
    expect_invalid([&] { r.factor.solve(B4, X43); },
                   {"solution is 4x3 but the right-hand side is 4x2", "shapes must match"});
    expect_invalid([&] { r.factor.solve(B4, X32); }, {"solution is 3x2"});
    EXPECT_EQ(r.factor.counters().solve_count, 0) << "a refused solve is not counted";

    // The same shapes through the shared handle.
    const auto handle = r.factor.share();
    expect_invalid([&] { handle->solve(b3, x3); },
                   {"SymmetricFactor::solve", "right-hand side has 3 entries"});
    expect_invalid([&] { handle->solve(B4, X43); }, {"solution is 4x3"});

    // A multi-column solve into a strided block of a larger matrix (the scratch path) is exact.
    Mat big = Mat::Zero(6, 5);
    Mat B = Mat::Zero(4, 2);
    B.col(0) = Vec::Ones(4);
    B.col(1) = Vec::LinSpaced(4, 1.0, 4.0);
    r.factor.solve(B, big.block(1, 2, 4, 2));
    EXPECT_LE((spd4() * big.block(1, 2, 4, 2) - B).lpNorm<Eigen::Infinity>(), 1e-12);
    EXPECT_EQ(big.row(0).norm(), 0.0);
    EXPECT_EQ(big.col(0).norm(), 0.0);
    EXPECT_EQ(big.row(5).norm(), 0.0);
}

TEST(SymmetricFactorRefusals, PartialSolvesRefuseShapesAndUnknownPhasesByName) {
    Ready r;
    ASSERT_TRUE(r.factor.supports_partial_solve());
    Vec b3 = Vec::Ones(3);
    Vec b4 = Vec::Ones(4);
    Vec x3(3);
    Vec x4(4);
    expect_invalid(
        [&] { r.factor.solve_partial(SymmetricFactor::SolvePhase::kForward, b3, x3); },
        {"SymmetricFactor::solve_partial", "right-hand side has 3 entries", "dimension is 4"});
    expect_invalid(
        [&] { r.factor.solve_partial(SymmetricFactor::SolvePhase::kDiagonal, b4, x3); },
        {"solution has 3 entries but the right-hand side has 4"});
    expect_invalid(
        [&] { r.factor.solve_partial(static_cast<SymmetricFactor::SolvePhase>(9), b4, x4); },
        {"unknown solve phase (9)"});
    EXPECT_EQ(r.factor.counters().partial_solve_count, 0);

    // The three phases compose to the full solve.
    Vec y(4), z(4), w(4);
    r.factor.solve_partial(SymmetricFactor::SolvePhase::kForward, b4, y);
    r.factor.solve_partial(SymmetricFactor::SolvePhase::kDiagonal, y, z);
    r.factor.solve_partial(SymmetricFactor::SolvePhase::kBackward, z, w);
    Vec full(4);
    r.factor.solve(b4, full);
    EXPECT_LE((w - full).lpNorm<Eigen::Infinity>(), 1e-12);
    EXPECT_EQ(r.factor.counters().partial_solve_count, 3);
}

// ---------------------------------------------------------------------------
// The dense arm's entries refuse a sparse-selected factor.
// ---------------------------------------------------------------------------

TEST(SymmetricFactorRefusals, FactorizeEquilibratedAndRetakeNeedADenseSelectedPattern) {
    Ready r; // dense_factorization defaults to kNever: a sparse-arm factor
    const std::vector<double> scale(4, 1.0);
    expect_logic([&] { r.factor.factorize_equilibrated(r.A, 2, scale.data(), 0.0); },
                 {"factorize_equilibrated", "dense-selected pattern", "DenseSingularPolicy::kReport"});
    expect_logic([&] { r.factor.retake_on_platform(r.A); },
                 {"retake_on_platform", "needs a dense-selected pattern"});
    EXPECT_EQ(r.factor.counters().dense_shift_refactor_count, 0);
    EXPECT_EQ(r.factor.counters().dense_retake_count, 0);
}

TEST(SymmetricFactorRefusals, FactorizeEquilibratedChecksItsScalarsBeforeAnythingElse) {
    Options o;
    o.dense_factorization = DensePolicy::kAlways;
    o.dense_singular_policy = DenseSingularPolicy::kReport;
    SymmetricFactor f(o);
    const SpMatRM A = upper_csr(spd4());
    f.analyze(A);
    std::vector<double> scale(4, 1.0);

    expect_invalid([&] { f.factorize_equilibrated(A, 2, scale.data(), -1.0); },
                   {"needs a finite sigma >= 0", "n_primal <= 4", "sigma = -1", "n_primal = 2"});
    expect_invalid(
        [&] { f.factorize_equilibrated(A, 2, scale.data(), std::numeric_limits<double>::quiet_NaN()); },
        {"needs a finite sigma >= 0"});
    expect_invalid(
        [&] { f.factorize_equilibrated(A, 2, scale.data(), std::numeric_limits<double>::infinity()); },
        {"needs a finite sigma >= 0"});
    expect_invalid([&] { f.factorize_equilibrated(A, -1, scale.data(), 0.0); }, {"n_primal = -1"});
    expect_invalid([&] { f.factorize_equilibrated(A, 5, scale.data(), 0.0); }, {"n_primal = 5"});
    expect_invalid([&] { f.factorize_equilibrated(A, 2, nullptr, 0.0); },
                   {"needs a finite sigma >= 0", "a scale"});

    scale[2] = 0.0;
    expect_invalid([&] { f.factorize_equilibrated(A, 2, scale.data(), 0.0); },
                   {"scale[2] = 0 is not positive finite"});
    scale[2] = -1.0;
    expect_invalid([&] { f.factorize_equilibrated(A, 2, scale.data(), 0.0); },
                   {"scale[2] = -1"});
    scale[2] = std::numeric_limits<double>::infinity();
    expect_invalid([&] { f.factorize_equilibrated(A, 2, scale.data(), 0.0); }, {"scale[2] = inf"});
    scale[2] = std::numeric_limits<double>::quiet_NaN();
    expect_invalid([&] { f.factorize_equilibrated(A, 2, scale.data(), 0.0); }, {"scale[2] = nan"});
    EXPECT_EQ(f.counters().dense_shift_refactor_count, 0);

    // A legal call on a dense-selected, report-policy pattern runs and is counted.
    scale.assign(4, 1.0);
    const FactorizeOutcome out = f.factorize_equilibrated(A, 2, scale.data(), 0.0);
    EXPECT_EQ(out.arm, FactorArm::kDense);
    EXPECT_EQ(f.counters().dense_shift_refactor_count, 1);
}

TEST(SymmetricFactorRefusals, EquilibratedFactorizationOnTheSparseFallbackPolicyIsRefused) {
    Options o;
    o.dense_factorization = DensePolicy::kAlways; // default singular policy: kSparseFallback
    SymmetricFactor f(o);
    const SpMatRM A = upper_csr(spd4());
    f.analyze(A);
    const std::vector<double> scale(4, 1.0);
    expect_logic([&] { f.factorize_equilibrated(A, 2, scale.data(), 0.0); },
                 {"DenseSingularPolicy::kReport"});
    // retake_on_platform needs only the dense selection, not the report policy.
    const FactorizeOutcome first = f.factorize(A);
    ASSERT_EQ(first.status, FactorizeOutcome::Status::kOk);
    EXPECT_NO_THROW(f.retake_on_platform(A));
    EXPECT_EQ(f.counters().dense_retake_count, 1);
}

// ---------------------------------------------------------------------------
// The dense arm refuses what it cannot do.
// ---------------------------------------------------------------------------

TEST(SymmetricFactorRefusals, ADenseFactorHasNoPartialSolvesAndSaysSo) {
    Options o;
    o.dense_factorization = DensePolicy::kAlways;
    SymmetricFactor f(o);
    const SpMatRM A = upper_csr(spd4());
    f.analyze(A);
    ASSERT_EQ(f.factorize(A).status, FactorizeOutcome::Status::kOk);
    EXPECT_FALSE(f.supports_partial_solve());
    Vec b = Vec::Ones(4);
    Vec x(4);
    expect_logic([&] { f.solve_partial(SymmetricFactor::SolvePhase::kForward, b, x); },
                 {"dense arm's", "no partial solves", "supports_partial_solve()"});
    EXPECT_EQ(f.counters().partial_solve_count, 0);
}

// ---------------------------------------------------------------------------
// Handles.
// ---------------------------------------------------------------------------

TEST(SymmetricFactorRefusals, AdoptRefusesANullHandleByName) {
    expect_invalid([] { SymmetricFactor::adopt(nullptr); }, {"SymmetricFactor::adopt", "must not be null"});
}

TEST(SymmetricFactorRefusals, AHandleOutlivingItsFactorStillSolvesAndRefusesTheSameShapes) {
    std::shared_ptr<const Factorization> handle;
    {
        Ready r;
        handle = r.factor.share();
    }
    Vec b = Vec::Ones(4);
    Vec x(4);
    EXPECT_NO_THROW(handle->solve(b, x));
    EXPECT_LE((Vec(spd4() * x) - b).lpNorm<Eigen::Infinity>(), 1e-12);
    EXPECT_EQ(handle->inertia().state, InertiaEvidence::State::kObserved);
    EXPECT_EQ(handle->inertia().positive, 4);
    Vec x2(2);
    expect_invalid([&] { handle->solve(b, x2); }, {"solution has 2 entries"});
}

TEST(SymmetricFactorRefusals, ThreadCountSettersRefuseNegativesNamingTheEntryPoint) {
    SymmetricFactor f{Options{}};
    expect_invalid([&] { f.set_threads(-1); }, {"num_threads must be >= 0", "got -1"});
    EXPECT_EQ(f.threads(), 0);
    f.set_threads(2);
    EXPECT_EQ(f.threads(), 2);
}

} // namespace
} // namespace hven::linear
