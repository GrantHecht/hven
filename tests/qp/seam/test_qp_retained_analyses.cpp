// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).
//
// THE RETAINED ANALYSES (M8 PF-5; detail/kkt/kkt_factor.h), at the engines:
//   - THE DISCARD: a swapped-in session whose refactorization fails (the platform factorize
//     fault injector, linear/fault_injection.h) is replaced by an unanalyzed session, so its
//     retained (session_id, epoch) -- the pair EliminatedFace::holds and the hot-start reuse
//     gate compare -- is never live again without committed numerics;
//   - THE LIFETIME: the walk's K0 factor and the SSN engine's factor free their retained
//     sessions at QP-solve exit, so a pattern that left in one solve is analyzed again in
//     the next;
//   - THE COUNT: QpCounters::symbolic_analysis_reuses on a measured walk subproblem.
//
// The injector is built for a session that never factorized; here it fails a session that
// has (a retained one). It skips the backend call, so the session's previous numerics stay
// inside it untouched while the adapter reports kBackendError and holds the epoch -- which
// is exactly the stale-pair state the discard exists for (docs/testing.md).

#include <cstdint>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/core/errors.h>
#include <hven/core/pattern_hash.h>
#include <hven/detail/kkt/kkt_factor.h>
#include <hven/detail/linear/fault_injection.h>
#include <hven/detail/qp/ssn/ssn_engine.h>
#include <hven/detail/qp/walk/walk_engine.h>

#include "qp/support/qp_dump.h"

using namespace hven;
using namespace hven::detail;
using hven::linear::DensePolicy;
using hven::linear::FactorizeOutcome;

namespace {

// K = [[I + diag, J^T], [J, -1e-8 I]] with J's single off-diagonal column per row chosen
// by `shift`: equal shifts give equal patterns.
SpMatRM small_kkt(Index n, Index m, Index shift, double scale) {
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        t.emplace_back(i, i, 2.0 + scale * static_cast<double>(i % 3));
    }
    for (Index j = 0; j < m; ++j) {
        t.emplace_back(j, n + j, 1.0);
        t.emplace_back((j + shift) % n == j ? (j + 1) % n : (j + shift) % n, n + j, 0.5 * scale);
        t.emplace_back(n + j, n + j, -1e-8);
    }
    SpMatRM K(n + m, n + m);
    K.setFromTriplets(t.begin(), t.end());
    K.makeCompressed();
    return K;
}

struct InjectorOn {
    InjectorOn() { hven::linear::detail::testing::FactorizeFaultInjector::active = true; }
    ~InjectorOn() { hven::linear::detail::testing::FactorizeFaultInjector::active = false; }
    InjectorOn(const InjectorOn &) = delete;
    InjectorOn &operator=(const InjectorOn &) = delete;
};

// A QP with equality rows whose sparsity (and so K0's pattern) is set by `shift`.
QpProblem small_qp(Index shift) {
    const Index n = 6;
    QpProblem qp;
    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(n, n) * 2.0;
    qp.H = H.sparseView();
    qp.g = Vec::LinSpaced(n, -1.0, 1.0);
    Eigen::MatrixXd Ae = Eigen::MatrixXd::Zero(2, n);
    Ae(0, 0) = 1.0;
    Ae(0, (1 + shift) % n) = 1.0;
    Ae(1, 2) = 1.0;
    Ae(1, (3 + shift) % n) = -1.0;
    qp.Ae = Ae.sparseView();
    qp.be = Vec::Constant(2, 0.25);
    Eigen::MatrixXd Ai = Eigen::MatrixXd::Zero(2, n);
    Ai(0, 4) = 1.0;
    Ai(1, 5) = 1.0;
    Ai(1, shift % n) = 1.0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec::Constant(2, -0.1); // both rows active at the answer
    qp.lower = Vec::Constant(n, -10.0);
    qp.upper = Vec::Constant(n, 10.0);
    return qp;
}

} // namespace

// A, B, then A' (A's pattern, new values) with the platform factorization failing: the hit
// swaps A's retained session in, its refactorization fails, and the KktFactor discards it.
// An EliminatedFace captured on A before it was retained no longer holds; the factor has
// no session; A' then pays a fresh analysis and factorizes cleanly.
TEST(QpRetainedAnalyses, AFailedRefactorizationOfASwappedInSessionDiscardsIt) {
    KktFactor k(0, DensePolicy::kNever);
    const SpMatRM A = small_kkt(12, 5, 2, 1.0);
    const SpMatRM B = small_kkt(12, 5, 3, 1.0);
    const SpMatRM A2 = small_kkt(12, 5, 2, 2.0);
    ASSERT_EQ(hven::pattern_hash(A), hven::pattern_hash(A2));

    ASSERT_EQ(factorize_checked(k, A).status, FactorizeOutcome::Status::kOk);
    EliminatedFace face;
    face.factorized = true;
    face.session_id = k.factor.session_id();
    face.epoch = k.factor.epoch();
    const auto holds_identity = [&] {
        return face.session_id == k.factor.session_id() && face.epoch == k.factor.epoch();
    };
    ASSERT_TRUE(holds_identity());

    ASSERT_EQ(factorize_checked(k, B).status, FactorizeOutcome::Status::kOk);
    ASSERT_EQ(k.retained.size(), 1u);
    ASSERT_FALSE(holds_identity());

    {
        const InjectorOn fault;
        const AnalysisReuseTally reuses;
        EXPECT_THROW(factorize_checked(k, A2), hven::BackendError);
        EXPECT_EQ(reuses.reuses(), 1) << "the failure must come from a swapped-in session";
    }
    // Discarded: no session, nothing analyzed, the stale pair dead.
    EXPECT_EQ(k.factor.session_id(), 0u);
    EXPECT_FALSE(k.analyzed);
    EXPECT_FALSE(holds_identity());
    // B (the outgoing session at the swap) is still retained; A's entry is gone.
    ASSERT_EQ(k.retained.size(), 1u);
    EXPECT_EQ(k.retained[0].pattern_hash, hven::pattern_hash(B));
    // The discarded session's counts stay with the KktFactor.
    EXPECT_EQ(kkt_counters(k).factorize_count, 3);

    const AnalysisReuseTally reuses;
    const hven::linear::AnalysisTally analyses;
    ASSERT_EQ(factorize_checked(k, A2).status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(reuses.reuses(), 0);
    EXPECT_EQ(analyses.analyses(), 1);
    EXPECT_FALSE(holds_identity());
    const Vec b = Vec::Ones(A2.rows());
    const Vec x = solve_vec(k, b);
    const Eigen::MatrixXd dense = Eigen::MatrixXd(A2).selfadjointView<Eigen::Upper>();
    EXPECT_LT((dense * x - b).cwiseAbs().maxCoeff(), 1e-8);
    // B is still a hit.
    ASSERT_EQ(factorize_checked(k, B).status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(reuses.reuses(), 1);
}

// The walk frees K0's retained sessions at every solve's exit. Solves on structures A then B
// move K0's pattern on the same (sole-owned) factor, so A's session is retained during the
// second solve; it must be gone by its exit, and a third solve on A analyzes again.
TEST(QpRetainedAnalyses, TheWalkFreesK0sRetainedSessionsAtSolveExit) {
    QpOptions o;
    ASSERT_EQ(o.working_set_algebra, QpWorkingSetAlgebra::kSchurBorder);
    const QpEngine engine(o, 0, DensePolicy::kNever);
    const QpProblem a = small_qp(0);
    const QpProblem b = small_qp(1);
    const auto retained_after = [&](const QpProblem &qp) {
        const QpSolution s = engine.solve(qp);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        const std::shared_ptr<const HotState> hot = engine.hot_state();
        EXPECT_NE(hot, nullptr);
        EXPECT_NE(hot != nullptr ? hot->border : nullptr, nullptr);
        return (hot != nullptr && hot->border != nullptr) ? hot->border->kkt.retained.size()
                                                          : std::size_t{99};
    };
    EXPECT_EQ(retained_after(a), 0u);
    EXPECT_EQ(retained_after(b), 0u);
    const QpSolution third = engine.solve(a);
    EXPECT_EQ(third.counters.symbolic_analysis_reuses, 0);
    EXPECT_GE(third.counters.symbolic_analyses, 1);
}

// The SSN engine frees its factor's retained sessions at every solve's exit: A, B, A on
// one engine pays an analysis on the third solve and reports no reuse.
TEST(QpRetainedAnalyses, TheSsnEngineFreesItsRetainedSessionsAtSolveExit) {
    QpOptions o;
    SsnEngine engine(o, 0, DensePolicy::kNever);
    const QpProblem a = small_qp(0);
    const QpProblem b = small_qp(1);
    SsnOptions so;
    SsnResult r;
    engine.solve(a, SsnStart{}, so, &r);
    EXPECT_EQ(r.symbolic_analyses, 1);
    engine.solve(b, SsnStart{}, so, &r);
    EXPECT_EQ(r.symbolic_analyses, 1);
    EXPECT_EQ(r.symbolic_analysis_reuses, 0);
    const hven::linear::AnalysisTally analyses;
    engine.solve(a, SsnStart{}, so, &r);
    EXPECT_EQ(r.symbolic_analyses, 1);
    EXPECT_EQ(r.symbolic_analysis_reuses, 0);
    EXPECT_EQ(analyses.analyses(), 1);
}

// THE COUNT on a measured subproblem: a YORKNET restoration QP whose walk returns its working
// set to faces it already factorized (23 returns on the reference build). Every return is a
// reuse, counted in QpCounters::symbolic_analysis_reuses and absent from the platform analyses.
TEST(QpRetainedAnalyses, AWalkSubproblemCountsItsReuses) {
    const test_support::QpDump d =
        test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/YRK_YORKNET_qp25.txt");
    const hven::linear::AnalysisTally analyses;
    const AnalysisReuseTally reuses;
    const QpSolution s = test_support::solve_qp_dump(d, d.opts);
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_EQ(s.counters.symbolic_analysis_reuses, reuses.reuses());
    ::testing::Test::RecordProperty("reuses",
                                    static_cast<int>(s.counters.symbolic_analysis_reuses));
    ::testing::Test::RecordProperty("analyses", static_cast<int>(analyses.analyses()));
#ifndef __APPLE__
    EXPECT_GT(s.counters.symbolic_analysis_reuses, 0);
#endif
}
