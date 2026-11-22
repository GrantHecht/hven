// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// hven::linear::AnalysisTally (PF-4): the boundary count of platform symbolic
// analyses and of the repeats among them, across SymmetricFactor instances.
// Backend-agnostic: built and run on both platforms.

#include <vector>

#include <gtest/gtest.h>

#include <hven/core/types.h>
#include <hven/linear/symmetric_factor.h>

namespace {

using hven::Index;
using hven::Mat;
using hven::SpMatRM;
using hven::linear::AnalysisTally;
using hven::linear::DensePolicy;
using hven::linear::SymmetricFactor;

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

// tridiag(-1, 2, -1) of order n, optionally with a (0, n-1) coupling that
// changes the pattern and nothing else about its character.
SpMatRM laplacian(Index n, bool corner) {
    Mat A = Mat::Zero(n, n);
    for (Index i = 0; i < n; ++i) {
        A(i, i) = 2.0;
        if (i + 1 < n) {
            A(i, i + 1) = -1.0;
            A(i + 1, i) = -1.0;
        }
    }
    if (corner) {
        A(0, n - 1) = -0.5;
        A(n - 1, 0) = -0.5;
    }
    return upper_csr(A);
}

SymmetricFactor::Options platform() {
    SymmetricFactor::Options o;
    o.dense_factorization = DensePolicy::kNever;
    return o;
}

TEST(AnalysisTally, CountsEveryInstanceAndTheRepeatsAmongThem) {
    const SpMatRM a = laplacian(6, false);
    const SpMatRM b = laplacian(6, true);
    AnalysisTally tally;
    {
        SymmetricFactor f1(platform());
        f1.analyze(a);
        SymmetricFactor f2(platform()); // a second instance, the same pattern: a repeat
        f2.analyze(a);
        SymmetricFactor f3(platform());
        f3.analyze(b); // a new pattern
        f3.analyze(a); // the same instance back on a pattern the tally has seen
    }
    EXPECT_EQ(tally.analyses(), 4);
    EXPECT_EQ(tally.repeats(), 2);
}

TEST(AnalysisTally, ValuesDoNotMatterThePatternDoes) {
    SpMatRM a = laplacian(5, false);
    SpMatRM scaled = a;
    scaled.coeffs() *= 3.0;
    AnalysisTally tally;
    SymmetricFactor f1(platform());
    f1.analyze(a);
    SymmetricFactor f2(platform());
    f2.analyze(scaled);
    EXPECT_EQ(tally.analyses(), 2);
    EXPECT_EQ(tally.repeats(), 1);
}

TEST(AnalysisTally, AnOptionThatReachesTheSymbolicPhaseIsPartOfTheKey) {
    const SpMatRM a = laplacian(6, false);
    SymmetricFactor::Options amd = platform();
    amd.ordering = SymmetricFactor::Options::Ordering::kMinimumDegree;
    AnalysisTally tally;
    SymmetricFactor f1(platform());
    f1.analyze(a);
    SymmetricFactor f2(amd);
    f2.analyze(a);
    EXPECT_EQ(tally.analyses(), 2);
    EXPECT_EQ(tally.repeats(), 0);
    // The thread count does not reach the symbolic phase.
    SymmetricFactor::Options one = platform();
    one.threads = 1;
    SymmetricFactor f3(one);
    f3.analyze(a);
    EXPECT_EQ(tally.repeats(), 1);
}

TEST(AnalysisTally, TheDenseArmRunsNoSymbolicPhaseAndIsNotCounted) {
    SymmetricFactor::Options dense;
    dense.dense_factorization = DensePolicy::kAlways;
    AnalysisTally tally;
    SymmetricFactor f(dense);
    f.analyze(laplacian(4, false));
    EXPECT_EQ(f.counters().analyze_count, 1);
    EXPECT_EQ(tally.analyses(), 0);
    EXPECT_EQ(tally.repeats(), 0);
}

TEST(AnalysisTally, NestedTalliesEachSeeTheirOwnExtent) {
    const SpMatRM a = laplacian(6, false);
    AnalysisTally outer;
    SymmetricFactor f1(platform());
    f1.analyze(a);
    {
        AnalysisTally inner;
        SymmetricFactor f2(platform());
        f2.analyze(a); // a repeat for the outer tally, the first for the inner one
        EXPECT_EQ(inner.analyses(), 1);
        EXPECT_EQ(inner.repeats(), 0);
    }
    SymmetricFactor f3(platform());
    f3.analyze(laplacian(6, true));
    EXPECT_EQ(outer.analyses(), 3);
    EXPECT_EQ(outer.repeats(), 1);
}

TEST(AnalysisTally, NothingIsCountedWithoutAnOpenTally) {
    {
        SymmetricFactor f(platform());
        f.analyze(laplacian(4, false));
    }
    AnalysisTally tally;
    EXPECT_EQ(tally.analyses(), 0);
}

// ---------------------------------------------------------------------------
// hven::linear::SolveTally / SolveContextScope (PF-2b): the boundary count of
// backend solves, their right-hand-side columns, split by the open context.
// ---------------------------------------------------------------------------

using hven::linear::SolveContext;
using hven::linear::SolveContextScope;
using hven::linear::SolveTally;

SymmetricFactor factored(DensePolicy policy) {
    SymmetricFactor::Options o;
    o.dense_factorization = policy;
    SymmetricFactor f(o);
    const SpMatRM a = laplacian(6, false);
    f.analyze(a);
    f.factorize(a);
    return f;
}

TEST(SolveTally, CountsCallsAndColumnsInTheOrdinaryContext) {
    for (const DensePolicy policy : {DensePolicy::kNever, DensePolicy::kAlways}) {
        SCOPED_TRACE(static_cast<int>(policy));
        SymmetricFactor f = factored(policy);
        SolveTally tally;
        const hven::Vec b = hven::Vec::Ones(6);
        hven::Vec x(6);
        f.solve(b, x);
        const hven::Mat B = hven::Mat::Ones(6, 3);
        hven::Mat X(6, 3);
        f.solve(B, X);
        EXPECT_EQ(tally.solves(SolveContext::kOrdinary), 2);
        EXPECT_EQ(tally.rhs_columns(SolveContext::kOrdinary), 4);
        for (const SolveContext c :
             {SolveContext::kDetector, SolveContext::kPolish, SolveContext::kBorderRefine}) {
            EXPECT_EQ(tally.solves(c), 0);
            EXPECT_EQ(tally.rhs_columns(c), 0);
        }
    }
}

TEST(SolveTally, ScopesLabelSolvesAndNestAndRestore) {
    SymmetricFactor f = factored(DensePolicy::kNever);
    SolveTally tally;
    const hven::Vec b = hven::Vec::Ones(6);
    hven::Vec x(6);
    {
        SolveContextScope detector(SolveContext::kDetector);
        f.solve(b, x);
        {
            SolveContextScope polish(SolveContext::kPolish);
            f.solve(b, x);
            f.solve(b, x);
        }
        f.solve(b, x); // back in the enclosing scope
    }
    f.solve(b, x); // and back to ordinary
    EXPECT_EQ(tally.solves(SolveContext::kDetector), 2);
    EXPECT_EQ(tally.solves(SolveContext::kPolish), 2);
    EXPECT_EQ(tally.solves(SolveContext::kBorderRefine), 0);
    EXPECT_EQ(tally.solves(SolveContext::kOrdinary), 1);
}

TEST(SolveTally, APartialSolveCountsOnePartialCall) {
    SymmetricFactor f = factored(DensePolicy::kNever);
    if (!f.supports_partial_solve()) {
        GTEST_SKIP() << "this backend's partial solves do not compose";
    }
    SolveTally tally;
    const hven::Vec b = hven::Vec::Ones(6);
    hven::Vec y(6);
    f.solve_partial(SymmetricFactor::SolvePhase::kForward, b, y);
    EXPECT_EQ(tally.solves(SolveContext::kOrdinary), 1);
    EXPECT_EQ(tally.rhs_columns(SolveContext::kOrdinary), 1);
}

TEST(SolveTally, ABorderRefineScopeIsItsOwnBucketAndAHandleSolveCounts) {
    SymmetricFactor f = factored(DensePolicy::kNever);
    auto handle = f.share();
    SolveTally tally;
    const hven::Vec b = hven::Vec::Ones(6);
    hven::Vec x(6);
    {
        SolveContextScope scope(SolveContext::kBorderRefine);
        handle->solve(b, x);
    }
    EXPECT_EQ(tally.solves(SolveContext::kBorderRefine), 1);
    EXPECT_EQ(tally.rhs_columns(SolveContext::kBorderRefine), 1);
    EXPECT_EQ(tally.solves(SolveContext::kOrdinary), 0);
}

TEST(SolveTally, NestedTalliesEachSeeTheirOwnExtentAndNoneCountsWithoutOne) {
    SymmetricFactor f = factored(DensePolicy::kNever);
    const hven::Vec b = hven::Vec::Ones(6);
    hven::Vec x(6);
    f.solve(b, x); // no tally open
    SolveTally outer;
    f.solve(b, x);
    {
        SolveTally inner;
        f.solve(b, x);
        EXPECT_EQ(inner.solves(SolveContext::kOrdinary), 1);
    }
    EXPECT_EQ(outer.solves(SolveContext::kOrdinary), 2);
}

} // namespace
