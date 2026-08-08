#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "hven/core/pattern_hash.h"
#include "hven/core/types.h"
#include "hven/linear/symmetric_factor.h"

namespace {

using hven::Index;
using hven::Mat;
using hven::SpMatRM;
using hven::Vec;
using hven::linear::Factorization;
using hven::linear::FactorizeOutcome;
using hven::linear::InertiaEvidence;
using hven::linear::SymmetricFactor;

// Every fixture below is a small matrix with an exactly representable
// solution (integer entries, integer right-hand sides), so the round trip
// through the factorization is only a few ULPs wide and a 1e-14 relative
// bound holds without slack.
constexpr double kTightRelTol = 1e-14;

// |actual - expected| <= rel_tol * max(1, |expected|): a relative bound above
// 1 that degrades to an absolute 1e-14 for small entries, matching the
// convention the dense factor's suite already uses.
void expect_vec_near(const Vec &actual, const Vec &expected, double rel_tol = kTightRelTol) {
    ASSERT_EQ(actual.size(), expected.size());
    for (Index i = 0; i < actual.size(); ++i) {
        const double e = expected(i);
        EXPECT_NEAR(actual(i), e, rel_tol * std::max(1.0, std::abs(e))) << "at entry " << i;
    }
}

// Bit-for-bit equality, entry by entry: used where the contract says two
// solves ran against the identical factorization with nothing interleaved,
// so anything short of identical would be a real finding.
void expect_vec_identical(const Vec &actual, const Vec &expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (Index i = 0; i < actual.size(); ++i) {
        EXPECT_EQ(actual(i), expected(i)) << "at entry " << i;
    }
}

// Builds the upper-triangle CSR form the surface takes: entry (i, j >= i) is
// stored when it is on the diagonal (always, even at value zero -- the
// backend needs the structural diagonal) or when `pattern(i, j)` is nonzero.
// Passing `pattern` separately is what lets a test vary VALUES while holding
// the sparsity pattern fixed, which is the whole point of the analyze-once
// lifecycle.
SpMatRM upper_csr(const Mat &values, const Mat &pattern) {
    const Index n = values.rows();
    std::vector<Eigen::Triplet<double>> triplets;
    for (Index i = 0; i < n; ++i) {
        for (Index j = i; j < n; ++j) {
            if (i == j || pattern(i, j) != 0.0) {
                triplets.emplace_back(static_cast<int>(i), static_cast<int>(j), values(i, j));
            }
        }
    }
    SpMatRM A(n, n);
    A.setFromTriplets(triplets.begin(), triplets.end());
    A.makeCompressed();
    return A;
}

SpMatRM upper_csr(const Mat &values) { return upper_csr(values, values); }

// 4x4 discrete Laplacian tridiag(-1, 2, -1): symmetric positive definite,
// inertia (4, 0, 0).
Mat spd4() {
    Mat A(4, 4);
    A << 2, -1, 0, 0, /**/ -1, 2, -1, 0, /**/ 0, -1, 2, -1, /**/ 0, 0, -1, 2;
    return A;
}

// 3x3 saddle point: a 2x2 positive definite Hessian block bordered by one
// full-rank constraint row. Analytic inertia (2 positive, 1 negative, 0
// zero) -- the canonical shape the KKT oracle sees.
Mat saddle3() {
    Mat K(3, 3);
    K << 2, 0, 1, /**/ 0, 3, 1, /**/ 1, 1, 0;
    return K;
}

// 3x3 exactly singular: in the reordering {0,2},{1} it is block diagonal,
// with the {0,2} block contributing one positive and one negative eigenvalue
// and the {1} block contributing an exactly zero one. Analytic inertia
// (1, 1, 1).
Mat singular3() {
    Mat K(3, 3);
    K << 1, 0, 1, /**/ 0, 0, 0, /**/ 1, 0, 0;
    return K;
}

SymmetricFactor::Options default_options() { return SymmetricFactor::Options{}; }

// Factorizes and asserts the backend was happy, so the tests below can read
// like the lifecycle they are exercising rather than like error handling.
void factorize_ok(SymmetricFactor &factor, const SpMatRM &A) {
    const FactorizeOutcome outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kOk)
        << "backend error " << outcome.backend_code;
}

} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

// The load-bearing property of the whole surface: three numeric
// factorizations on one sparsity pattern cost exactly one symbolic analysis,
// and every solve in between is correct.
TEST(SymmetricFactor, AnalyzeOnceFactorizeManyOnAFixedPattern) {
    const Mat dense = spd4();
    const Vec x_exact = (Vec(4) << 1.0, 2.0, 3.0, 4.0).finished();
    const Vec b = dense * x_exact;

    SymmetricFactor factor(default_options());
    factor.analyze(upper_csr(dense));
    EXPECT_EQ(factor.counters().analyze_count, 1);
    EXPECT_EQ(factor.epoch(), 0u);

    // Scaling the matrix scales the solution by the reciprocal; the scale
    // factors are powers of two, so the expected values stay exact.
    const double scales[] = {1.0, 2.0, 4.0};
    for (int round = 0; round < 3; ++round) {
        const Mat scaled = scales[round] * dense;
        factorize_ok(factor, upper_csr(scaled, dense));

        Vec x(4);
        factor.solve(b, x);
        expect_vec_near(x, Vec(x_exact / scales[round]));

        EXPECT_EQ(factor.counters().analyze_count, 1) << "factorize() must never re-analyze";
        EXPECT_EQ(factor.counters().factorize_count, round + 1);
        EXPECT_EQ(factor.counters().solve_count, round + 1);
        EXPECT_EQ(factor.epoch(), static_cast<std::uint64_t>(round + 1));
    }
}

TEST(SymmetricFactor, FactorizeRejectsAForeignPattern) {
    SymmetricFactor factor(default_options());
    factor.analyze(upper_csr(spd4()));

    // Same size, different sparsity pattern: an entry the analysis never saw.
    Mat other = spd4();
    other(0, 3) = 1.0;
    other(3, 0) = 1.0;

    EXPECT_THROW(factor.factorize(upper_csr(other)), std::invalid_argument);
    EXPECT_EQ(factor.counters().factorize_count, 0);
    EXPECT_EQ(factor.counters().analyze_count, 1) << "a rejected factorize must not re-analyze";
}

TEST(SymmetricFactor, MisuseOrderingThrows) {
    SymmetricFactor factor(default_options());
    const SpMatRM A = upper_csr(spd4());

    // factorize before analyze
    EXPECT_THROW(factor.factorize(A), std::runtime_error);

    Vec b = Vec::Ones(4);
    Vec x(4);

    // solve before factorize, with and without a symbolic analysis in hand
    EXPECT_THROW(factor.solve(b, x), std::runtime_error);
    EXPECT_THROW(factor.share(), std::runtime_error);
    factor.analyze(A);
    EXPECT_THROW(factor.solve(b, x), std::runtime_error);
    EXPECT_THROW(factor.solve_partial(SymmetricFactor::SolvePhase::kForward, b, x),
                 std::runtime_error);
    EXPECT_THROW(factor.share(), std::runtime_error);

    // ... and none of that counted as work.
    EXPECT_EQ(factor.counters().factorize_count, 0);
    EXPECT_EQ(factor.counters().solve_count, 0);
    EXPECT_EQ(factor.counters().partial_solve_count, 0);
}

TEST(SymmetricFactor, AnalyzeRejectsMalformedInput) {
    SymmetricFactor factor(default_options());

    // Not compressed.
    SpMatRM uncompressed(3, 3);
    uncompressed.insert(0, 0) = 1.0;
    uncompressed.insert(1, 1) = 1.0;
    uncompressed.insert(2, 2) = 1.0;
    EXPECT_THROW(factor.analyze(uncompressed), std::invalid_argument);

    // Not square.
    SpMatRM rectangular(2, 3);
    rectangular.insert(0, 0) = 1.0;
    rectangular.makeCompressed();
    EXPECT_THROW(factor.analyze(rectangular), std::invalid_argument);

    // Empty.
    SpMatRM empty(0, 0);
    empty.makeCompressed();
    EXPECT_THROW(factor.analyze(empty), std::invalid_argument);

    // Stores the lower triangle too -- the surface takes the upper triangle.
    SpMatRM both_triangles(2, 2);
    both_triangles.insert(0, 0) = 2.0;
    both_triangles.insert(0, 1) = 1.0;
    both_triangles.insert(1, 0) = 1.0;
    both_triangles.insert(1, 1) = 2.0;
    both_triangles.makeCompressed();
    EXPECT_THROW(factor.analyze(both_triangles), std::invalid_argument);

    // A row with no structural diagonal.
    SpMatRM no_diagonal(2, 2);
    no_diagonal.insert(0, 0) = 2.0;
    no_diagonal.insert(0, 1) = 1.0;
    no_diagonal.makeCompressed();
    EXPECT_THROW(factor.analyze(no_diagonal), std::invalid_argument);

    EXPECT_EQ(factor.counters().analyze_count, 0);
}

TEST(SymmetricFactor, SolveRejectsSizeMismatch) {
    SymmetricFactor factor(default_options());
    const SpMatRM A = upper_csr(spd4());
    factor.analyze(A);
    factorize_ok(factor, A);

    Vec short_rhs = Vec::Ones(3);
    Vec x4(4);
    EXPECT_THROW(factor.solve(short_rhs, x4), std::invalid_argument);

    Vec b4 = Vec::Ones(4);
    Vec short_x(3);
    EXPECT_THROW(factor.solve(b4, short_x), std::invalid_argument);

    Mat RHS = Mat::Ones(4, 2);
    Mat X_wrong(4, 3);
    EXPECT_THROW(factor.solve(RHS, X_wrong), std::invalid_argument);

    EXPECT_EQ(factor.counters().solve_count, 0);
}

TEST(SymmetricFactor, UnimplementedFactorKindsThrow) {
    SymmetricFactor::Options llt = default_options();
    llt.kind = hven::linear::FactorKind::kLLT;
    EXPECT_THROW(SymmetricFactor{llt}, std::logic_error);

    SymmetricFactor::Options lu = default_options();
    lu.kind = hven::linear::FactorKind::kLU;
    EXPECT_THROW(SymmetricFactor{lu}, std::logic_error);
}

// =============================================================================
// Inertia evidence
// =============================================================================

TEST(SymmetricFactor, InertiaOfAnSpdMatrixIsAllPositive) {
    const SpMatRM A = upper_csr(spd4());
    SymmetricFactor factor(default_options());
    factor.analyze(A);
    const FactorizeOutcome outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kOk);

    const InertiaEvidence &evidence = outcome.inertia;
    EXPECT_EQ(evidence.state, InertiaEvidence::State::kObserved);
    EXPECT_EQ(evidence.n_pos, 4);
    EXPECT_EQ(evidence.n_neg, 0);
    EXPECT_EQ(evidence.n_zero, 0);
    EXPECT_TRUE(evidence.zero_is_derived) << "MKL derives the zero class rather than reporting it";
    ASSERT_TRUE(evidence.perturbed_pivots.has_value())
        << "the perturbed-pivot counter is always present on this backend";
    EXPECT_GE(*evidence.perturbed_pivots, 0);

    // inertia() reports the same evidence as the outcome did.
    const InertiaEvidence queried = factor.inertia();
    EXPECT_EQ(queried.state, evidence.state);
    EXPECT_EQ(queried.n_pos, evidence.n_pos);
    EXPECT_EQ(queried.n_neg, evidence.n_neg);
    EXPECT_EQ(queried.n_zero, evidence.n_zero);
}

TEST(SymmetricFactor, InertiaOfASaddlePointIsObserved) {
    const SpMatRM K = upper_csr(saddle3());
    SymmetricFactor factor(default_options());
    factor.analyze(K);
    const FactorizeOutcome outcome = factor.factorize(K);
    ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kOk);

    EXPECT_EQ(outcome.inertia.state, InertiaEvidence::State::kObserved);
    EXPECT_EQ(outcome.inertia.n_pos, 2);
    EXPECT_EQ(outcome.inertia.n_neg, 1);
    EXPECT_EQ(outcome.inertia.n_zero, 0);
    ASSERT_TRUE(outcome.inertia.perturbed_pivots.has_value());
    EXPECT_EQ(*outcome.inertia.perturbed_pivots, 0)
        << "a well-conditioned saddle point needs no pivot perturbation";
}

// An exactly singular matrix, whose analytic inertia is (1, 1, 1).
//
// PINNED OBSERVED BEHAVIOR, not the analytic answer: this backend does not
// report the zero class. It perturbs the zero pivot into a signed one, counts
// that in the perturbed-pivot counter, and returns positive/negative counts
// that sum to the dimension -- so the DERIVED zero count reads 0 and the only
// signal that a sign was fabricated rather than measured is
// perturbed_pivots > 0. That is exactly the reason perturbed_pivots is part
// of the evidence and why a driver must consult it before trusting an
// inertia. The invariants asserted first (state, derivation flag, the
// subtraction itself) hold regardless; the counts after them are what this
// backend was observed to produce.
TEST(SymmetricFactor, InertiaOfASingularMatrixDerivesItsZeroClass) {
    const SpMatRM K = upper_csr(singular3());
    SymmetricFactor factor(default_options());
    factor.analyze(K);
    const FactorizeOutcome outcome = factor.factorize(K);
    ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kOk)
        << "this backend perturbs its way through a singular matrix rather than failing";

    const InertiaEvidence &evidence = outcome.inertia;
    EXPECT_EQ(evidence.state, InertiaEvidence::State::kObserved);
    EXPECT_TRUE(evidence.zero_is_derived);
    EXPECT_EQ(evidence.n_zero, 3 - evidence.n_pos - evidence.n_neg)
        << "the zero class is the derived remainder, by construction";

    ASSERT_TRUE(evidence.perturbed_pivots.has_value());
    EXPECT_GT(*evidence.perturbed_pivots, 0)
        << "the perturbed-pivot counter is the only tell that this factorization is not exact";
    EXPECT_EQ(evidence.n_pos, 2);
    EXPECT_EQ(evidence.n_neg, 1);
    EXPECT_EQ(evidence.n_zero, 0);
}

// =============================================================================
// Partial solves
// =============================================================================

TEST(SymmetricFactor, PartialSolvesComposeToTheFullSolve) {
    const Mat dense = saddle3();
    const SpMatRM K = upper_csr(dense);
    const Vec x_exact = (Vec(3) << 1.0, 2.0, -3.0).finished();
    const Vec b = dense * x_exact;

    SymmetricFactor factor(default_options());
    factor.analyze(K);
    factorize_ok(factor, K);

    ASSERT_TRUE(factor.supports_partial_solve());

    Vec x_full(3);
    factor.solve(b, x_full);
    expect_vec_near(x_full, x_exact);

    Vec y(3);
    Vec z(3);
    Vec x_composed(3);
    factor.solve_partial(SymmetricFactor::SolvePhase::kForward, b, y);
    factor.solve_partial(SymmetricFactor::SolvePhase::kDiagonal, y, z);
    factor.solve_partial(SymmetricFactor::SolvePhase::kBackward, z, x_composed);

    expect_vec_near(x_composed, x_full);
    EXPECT_EQ(factor.counters().solve_count, 1);
    EXPECT_EQ(factor.counters().partial_solve_count, 3)
        << "partial solves are counted separately; three of them are not one solve";
}

// Refinement is a full-solve setting. The backend requires it off during a
// phase-split solve and produces a silently wrong answer otherwise, so the
// implementation forces it off around every partial solve regardless of
// configuration. The check: with refinement configured ON, the composed
// partials still reproduce a full solve computed with refinement OFF.
TEST(SymmetricFactor, ConfiguredRefinementDoesNotLeakIntoPartialSolves) {
    const Mat dense = saddle3();
    const SpMatRM K = upper_csr(dense);
    const Vec x_exact = (Vec(3) << 1.0, 2.0, -3.0).finished();
    const Vec b = dense * x_exact;

    SymmetricFactor unrefined(default_options());
    unrefined.analyze(K);
    factorize_ok(unrefined, K);
    Vec reference(3);
    const hven::linear::SolveInfo unrefined_info = unrefined.solve(b, reference);
    ASSERT_TRUE(unrefined_info.refinement_iters.has_value());
    EXPECT_EQ(*unrefined_info.refinement_iters, 0) << "refinement was configured off";

    SymmetricFactor::Options refined = default_options();
    refined.max_refinement_iters = 2;
    SymmetricFactor factor(refined);
    factor.analyze(K);
    factorize_ok(factor, K);
    ASSERT_TRUE(factor.supports_partial_solve());

    // The configured refinement is real and reported, not merely accepted.
    Vec x_refined(3);
    const hven::linear::SolveInfo refined_info = factor.solve(b, x_refined);
    ASSERT_TRUE(refined_info.refinement_iters.has_value());
    EXPECT_EQ(*refined_info.refinement_iters, 2);
    expect_vec_near(x_refined, x_exact);

    Vec y(3);
    Vec z(3);
    Vec x_composed(3);
    const hven::linear::SolveInfo forward =
        factor.solve_partial(SymmetricFactor::SolvePhase::kForward, b, y);
    const hven::linear::SolveInfo diagonal =
        factor.solve_partial(SymmetricFactor::SolvePhase::kDiagonal, y, z);
    const hven::linear::SolveInfo backward =
        factor.solve_partial(SymmetricFactor::SolvePhase::kBackward, z, x_composed);

    // Partial solves ran with refinement off, whatever the engine was
    // configured with.
    EXPECT_EQ(forward.refinement_iters.value_or(-1), 0);
    EXPECT_EQ(diagonal.refinement_iters.value_or(-1), 0);
    EXPECT_EQ(backward.refinement_iters.value_or(-1), 0);

    // The composition still reproduces a full solve computed with refinement
    // off -- the refinement setting did not leak into it.
    expect_vec_near(x_composed, reference);

    // ... and the setting was RESTORED afterward: the next full solve gets
    // its refinement back. Drop the restore and this reads 0.
    Vec x_after(3);
    const hven::linear::SolveInfo after = factor.solve(b, x_after);
    EXPECT_EQ(after.refinement_iters.value_or(-1), 2)
        << "the refinement cap must survive a partial solve";
    expect_vec_near(x_after, x_exact);
}

// Why supports_partial_solve() exists at all. On a factorization with a
// perturbed pivot, the composed partials do not merely lose a few digits --
// they return a different answer, by eight orders of magnitude, with no error
// raised anywhere. A caller that composes partials without consulting the
// predicate gets that silently.
TEST(SymmetricFactor, ComposedPartialsDivergeWhenTheGateIsClosed) {
    const Mat dense = singular3();
    const SpMatRM K = upper_csr(dense);
    const Vec b = (Vec(3) << 1.0, 2.0, 3.0).finished();

    SymmetricFactor factor(default_options());
    factor.analyze(K);
    const FactorizeOutcome outcome = factor.factorize(K);
    ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kOk);
    ASSERT_FALSE(factor.supports_partial_solve());

    Vec x_full(3);
    factor.solve(b, x_full);

    Vec y(3);
    Vec z(3);
    Vec x_composed(3);
    factor.solve_partial(SymmetricFactor::SolvePhase::kForward, b, y);
    factor.solve_partial(SymmetricFactor::SolvePhase::kDiagonal, y, z);
    factor.solve_partial(SymmetricFactor::SolvePhase::kBackward, z, x_composed);

    EXPECT_GT((x_composed - x_full).norm(), 1.0)
        << "the gate is closed on this factorization precisely because these disagree";
}

TEST(SymmetricFactor, SupportsPartialSolveIsFalseBeforeAFactorization) {
    SymmetricFactor factor(default_options());
    EXPECT_FALSE(factor.supports_partial_solve()) << "nothing composable exists yet";

    factor.analyze(upper_csr(spd4()));
    EXPECT_FALSE(factor.supports_partial_solve()) << "a symbolic analysis is not a factorization";
}

// The predicate is exactly "this factorization perturbed no pivots", and the
// singular fixture is a case where the backend really does perturb one.
TEST(SymmetricFactor, SupportsPartialSolveTracksPerturbedPivots) {
    const SpMatRM K = upper_csr(singular3());
    SymmetricFactor factor(default_options());
    factor.analyze(K);
    const FactorizeOutcome outcome = factor.factorize(K);
    ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kOk);

    ASSERT_TRUE(outcome.inertia.perturbed_pivots.has_value());
    EXPECT_EQ(factor.supports_partial_solve(), *outcome.inertia.perturbed_pivots == 0);
    EXPECT_FALSE(factor.supports_partial_solve())
        << "this fixture is here because it does perturb a pivot";
}

// =============================================================================
// Multi-RHS and threading
// =============================================================================

TEST(SymmetricFactor, MultiRhsSolveMatchesPerColumnSolves) {
    const Mat dense = spd4();
    const SpMatRM A = upper_csr(dense);
    SymmetricFactor factor(default_options());
    factor.analyze(A);
    factorize_ok(factor, A);

    Mat X_exact(4, 2);
    X_exact.col(0) << 1.0, 2.0, 3.0, 4.0;
    X_exact.col(1) << -1.0, 0.5, 2.0, -3.0;
    const Mat RHS = dense * X_exact;

    Mat X(4, 2);
    factor.solve(RHS, X);
    expect_vec_near(Vec(X.col(0)), Vec(X_exact.col(0)));
    expect_vec_near(Vec(X.col(1)), Vec(X_exact.col(1)));
    EXPECT_EQ(factor.counters().solve_count, 1) << "one call is one solve, whatever its width";

    // The same answers through a strided destination (a block of a taller
    // matrix), which takes the copy-back path rather than solving into the
    // caller's buffer directly.
    Mat tall = Mat::Zero(5, 2);
    factor.solve(RHS, tall.topRows(4));
    expect_vec_near(Vec(tall.topRows(4).col(0)), Vec(X_exact.col(0)));
    expect_vec_near(Vec(tall.topRows(4).col(1)), Vec(X_exact.col(1)));
    EXPECT_EQ(tall.row(4).cwiseAbs().sum(), 0.0)
        << "the solve stayed inside the block it was given";
}

TEST(SymmetricFactor, ZeroColumnSolveIsANoOpAndCountsAsNoWork) {
    const SpMatRM A = upper_csr(spd4());
    SymmetricFactor factor(default_options());
    factor.analyze(A);
    factorize_ok(factor, A);

    Mat RHS(4, 0);
    Mat X(4, 0);
    factor.solve(RHS, X);
    EXPECT_EQ(factor.counters().solve_count, 0) << "nothing reached the backend";
}

TEST(SymmetricFactor, ExplicitThreadCountChangesNeitherCountersNorSolutions) {
    const Mat dense = spd4();
    const SpMatRM A = upper_csr(dense);
    const Vec x_exact = (Vec(4) << 1.0, 2.0, 3.0, 4.0).finished();
    const Vec b = dense * x_exact;

    SymmetricFactor::Options single = default_options();
    single.num_threads = 1;

    SymmetricFactor threaded(single);
    threaded.analyze(A);
    factorize_ok(threaded, A);
    Vec x_threaded(4);
    threaded.solve(b, x_threaded);

    SymmetricFactor defaulted(default_options());
    defaulted.analyze(A);
    factorize_ok(defaulted, A);
    Vec x_default(4);
    defaulted.solve(b, x_default);

    // Counters are exact across thread settings; values are compared at
    // tolerance, never bitwise -- a different thread count is allowed to
    // reassociate the arithmetic.
    EXPECT_EQ(threaded.counters().analyze_count, defaulted.counters().analyze_count);
    EXPECT_EQ(threaded.counters().factorize_count, defaulted.counters().factorize_count);
    EXPECT_EQ(threaded.counters().solve_count, defaulted.counters().solve_count);
    expect_vec_near(x_threaded, x_exact);
    expect_vec_near(x_default, x_exact);
}

// =============================================================================
// Epochs and shared handles
// =============================================================================

TEST(SymmetricFactor, SharedHandleOutlivesItsOriginator) {
    const Mat dense = saddle3();
    const SpMatRM K = upper_csr(dense);
    const Vec x_exact = (Vec(3) << 1.0, 2.0, -3.0).finished();
    const Vec b = dense * x_exact;

    Vec before_share(3);
    Vec through_handle(3);
    std::shared_ptr<const Factorization> handle;
    {
        SymmetricFactor factor(default_options());
        factor.analyze(K);
        factorize_ok(factor, K);
        factor.solve(b, before_share);

        handle = factor.share();
        EXPECT_EQ(handle->pattern_hash(), hven::pattern_hash(K));
        EXPECT_EQ(handle->epoch(), factor.epoch());

        // Sharing does not empty the originator: it keeps solving.
        Vec after_share(3);
        factor.solve(b, after_share);
        expect_vec_identical(after_share, before_share);
    }

    // The originator is gone; the handle still owns the session.
    handle->solve(b, through_handle);
    // Nothing was interleaved between the two solves, so this is not merely
    // close -- it is the same factorization answering the same question.
    expect_vec_identical(through_handle, before_share);

    const InertiaEvidence evidence = handle->inertia();
    EXPECT_EQ(evidence.state, InertiaEvidence::State::kObserved);
    EXPECT_EQ(evidence.n_pos, 2);
    EXPECT_EQ(evidence.n_neg, 1);
}

TEST(SymmetricFactor, AdoptingACurrentHandleReusesItsNumerics) {
    const Mat dense = saddle3();
    const SpMatRM K = upper_csr(dense);
    const Vec b = (Vec(3) << 1.0, 2.0, 3.0).finished();

    SymmetricFactor originator(default_options());
    originator.analyze(K);
    factorize_ok(originator, K);
    Vec expected(3);
    originator.solve(b, expected);

    std::shared_ptr<const Factorization> handle = originator.share();
    SymmetricFactor adopter = SymmetricFactor::adopt(handle);

    EXPECT_EQ(adopter.epoch(), originator.epoch());
    EXPECT_EQ(adopter.counters().analyze_count, 0)
        << "adopting performs no analysis, so it counts none";

    Vec adopted_solution(3);
    adopter.solve(b, adopted_solution);
    expect_vec_identical(adopted_solution, expected);

    // The adopted symbolic is reusable: a factorize through the adopter does
    // not analyze anything.
    factorize_ok(adopter, K);
    EXPECT_EQ(adopter.counters().analyze_count, 0);
    EXPECT_EQ(adopter.counters().factorize_count, 1);
    EXPECT_EQ(originator.epoch(), adopter.epoch())
        << "one session, one epoch: co-owners see the same numerics";
}

// The staleness contract: a handle's epoch is fixed at emission while the
// engine's is live, so a refactorization by the originator is detectable
// after the fact -- and adopting the now-stale handle refuses its numerics
// while still reusing its symbolic.
TEST(SymmetricFactor, AdoptingAStaleHandleRefusesNumericsButKeepsTheSymbolic) {
    const Mat dense = saddle3();
    const SpMatRM K = upper_csr(dense);
    const Vec b = (Vec(3) << 1.0, 2.0, 3.0).finished();

    SymmetricFactor originator(default_options());
    originator.analyze(K);
    factorize_ok(originator, K);

    std::shared_ptr<const Factorization> handle = originator.share();
    const std::uint64_t emitted_epoch = handle->epoch();
    EXPECT_EQ(emitted_epoch, 1u);

    Vec at_emission(3);
    handle->solve(b, at_emission);

    // New values on the same pattern.
    const Mat updated = 2.0 * dense;
    factorize_ok(originator, upper_csr(updated, dense));

    EXPECT_EQ(handle->epoch(), emitted_epoch) << "the handle's epoch is fixed at emission";
    EXPECT_EQ(originator.epoch(), emitted_epoch + 1) << "the engine's epoch is live";

    // The handle holds no snapshot it could serve: solving through it now
    // answers with the session's CURRENT numerics, not the ones it was
    // emitted for. That is the documented hazard the epoch exists to make
    // detectable -- and doubling the matrix halves the solution, so the two
    // are plainly different answers rather than the same one twice.
    Vec through_handle_now(3);
    handle->solve(b, through_handle_now);
    Vec originator_now(3);
    originator.solve(b, originator_now);
    expect_vec_identical(through_handle_now, originator_now);
    expect_vec_near(through_handle_now, Vec(at_emission / 2.0));

    SymmetricFactor adopter = SymmetricFactor::adopt(handle);
    Vec x(3);
    EXPECT_THROW(adopter.solve(b, x), std::runtime_error);
    EXPECT_THROW(adopter.share(), std::runtime_error);
    EXPECT_FALSE(adopter.supports_partial_solve());
    EXPECT_EQ(adopter.inertia().state, InertiaEvidence::State::kUnavailable)
        << "an engine refused the numerics does not report their evidence as its own";

    // The symbolic survived: the first factorize reuses it and clears the
    // refusal.
    factorize_ok(adopter, K);
    EXPECT_EQ(adopter.counters().analyze_count, 0) << "no re-analysis on the adopted symbolic";
    adopter.solve(b, x);

    Vec expected(3);
    originator.solve(b, expected);
    // Both engines drive the same session, so they see the same numerics.
    expect_vec_identical(x, expected);
}

TEST(SymmetricFactor, AdoptingAHandleForAnotherPatternRequiresAFreshAnalysis) {
    const SpMatRM K = upper_csr(saddle3());
    SymmetricFactor originator(default_options());
    originator.analyze(K);
    factorize_ok(originator, K);
    std::shared_ptr<const Factorization> handle = originator.share();

    SymmetricFactor adopter = SymmetricFactor::adopt(handle);

    // A different pattern is a contract violation against the adopted
    // structural key, not a numeric outcome.
    const SpMatRM other = upper_csr(spd4());
    EXPECT_THROW(adopter.factorize(other), std::invalid_argument);
    EXPECT_EQ(adopter.counters().analyze_count, 0);

    // The recovery is an explicit analysis, from which point the handle
    // contributes nothing.
    adopter.analyze(other);
    EXPECT_EQ(adopter.counters().analyze_count, 1);
    factorize_ok(adopter, other);

    // The originator's own session is untouched by the adopter's
    // re-analysis: its factorization still solves.
    Vec b = Vec::Ones(3);
    Vec x(3);
    originator.solve(b, x);
    EXPECT_EQ(originator.counters().analyze_count, 1);

    // Epochs never run backwards across a re-analysis.
    EXPECT_GT(adopter.epoch(), handle->epoch());
}

TEST(SymmetricFactor, AdoptRejectsANullHandle) {
    EXPECT_THROW(SymmetricFactor::adopt(nullptr), std::invalid_argument);
}
