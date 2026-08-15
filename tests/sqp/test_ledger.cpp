#include <random>

#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/sqp/sqp_driver.h>

#include "support/parametric_families.h"

using namespace hven::solvers;

namespace {

// Repeated verbatim from test_qp_engine.cpp so this file stays self-contained
// (see that file's fixture-block comment for the rationale).
QpProblem random_strictly_convex(int n, int mi, unsigned seed) {
    // H = M^T M + I is symmetric positive definite by construction, so the QP
    // is strictly convex and its optimum is unique. bi is kept strictly
    // positive so x = 0 is strictly feasible (the problem is never infeasible
    // and the cold start is never shifted).
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(-1.0, 1.0);

    Eigen::MatrixXd M(n, n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            M(i, j) = unit(rng);
        }
    }
    const Eigen::MatrixXd Hd = M.transpose() * M + Eigen::MatrixXd::Identity(n, n);

    QpProblem qp;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    for (int i = 0; i < n; ++i) {
        qp.g(i) = 2.0 * unit(rng);
    }
    qp.Ae.resize(0, n);
    qp.be = Vec(0);

    Eigen::MatrixXd Aid(mi, n);
    for (int r = 0; r < mi; ++r) {
        for (int j = 0; j < n; ++j) {
            Aid(r, j) = unit(rng);
        }
    }
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(mi);
    for (int r = 0; r < mi; ++r) {
        qp.bi(r) = 0.25 + std::abs(unit(rng));
    }
    qp.lower = Vec::Constant(n, -1.5);
    qp.upper = Vec::Constant(n, 1.5);
    return qp;
}

TEST(Ledger, ColdWarmPairRecordsAndSummary) {
    // Solve random_strictly_convex(6, 4, 7) cold and warm with ledger attached.
    // Assert two records, warm flag on second, and summary_table() contains labels
    // and non-zero counters.
    auto qp = random_strictly_convex(6, 4, 7);
    QpEngine engine{QpOptions{}};

    Ledger ledger;
    engine.attach_ledger(&ledger, "test");

    // Solve cold
    auto cold = engine.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);

    // Perturb g and solve warm from cold solution
    qp.g.array() += 1e-3;
    auto warm = engine.solve(qp, cold);
    ASSERT_EQ(warm.status, QpStatus::kOptimal);

    // Check ledger has two records
    const auto &records = ledger.records();
    ASSERT_EQ(records.size(), 2);

    // Check first record is cold (not warm)
    EXPECT_FALSE(records[0].warm);
    EXPECT_EQ(records[0].label, "test_0");
    EXPECT_GT(records[0].counters.minor_iters, 0);
    EXPECT_GT(records[0].counters.factorizations, 0);

    // Check second record is warm
    ASSERT_TRUE(records[1].warm);
    EXPECT_EQ(records[1].label, "test_1");
    EXPECT_GT(records[1].counters.minor_iters, 0);

    // Check summary_table() contains both labels and non-zero counters
    const std::string summary = ledger.summary_table();
    EXPECT_NE(summary.find("test_0"), std::string::npos);
    EXPECT_NE(summary.find("test_1"), std::string::npos);
    EXPECT_NE(summary.find("no"), std::string::npos);  // cold = not warm
    EXPECT_NE(summary.find("yes"), std::string::npos); // warm = warm
}

TEST(Ledger, ExceptionDoesNotAdvanceCounter) {
    // Attach a ledger, call solve() on a malformed QpProblem (dimension
    // mismatch so validate() throws), expect the exception, assert records()
    // is empty, then solve a good problem and verify it gets label prefix_0
    // (counter did not advance on failed solve).
    QpEngine engine{QpOptions{}};

    Ledger ledger;
    engine.attach_ledger(&ledger, "exc");

    // Create a malformed QpProblem: H is n x n but g has wrong dimension
    QpProblem bad_qp;
    bad_qp.H = Eigen::SparseMatrix<double>(5, 5);
    bad_qp.g = Vec(6); // dimension mismatch: n=5 but g.size()=6
    bad_qp.Ae.resize(0, 5);
    bad_qp.be = Vec(0);
    bad_qp.Ai.resize(0, 5);
    bad_qp.bi = Vec(0);
    bad_qp.lower = Vec::Constant(5, -1.0);
    bad_qp.upper = Vec::Constant(5, 1.0);

    // Solve on bad_qp should throw (validate() checks dimensions)
    EXPECT_THROW(engine.solve(bad_qp), std::invalid_argument);

    // No record should be emitted on exception
    EXPECT_EQ(ledger.records().size(), 0);

    // Now solve a good problem; it should get label exc_0 (counter at 0,
    // not 1), proving the counter did not advance on the failed solve.
    auto good_qp = random_strictly_convex(5, 2, 42);
    auto good = engine.solve(good_qp);
    ASSERT_EQ(good.status, QpStatus::kOptimal);

    ASSERT_EQ(ledger.records().size(), 1);
    EXPECT_EQ(ledger.records()[0].label, "exc_0");
    EXPECT_FALSE(ledger.records()[0].warm);
}

// PHASE-5 TASK 2. SqpSolveRecord::wall_seconds is populated on a driver
// solve() call -- std::chrono::steady_clock timed around solve_impl alone
// (sqp_driver.h). This asserts only that it is POPULATED (> 0 on a solve
// that did real work), never a magnitude: ledger.h's own note on the field
// states it is informational, machine-dependent and never asserted on a
// specific value by any test, and this one is no exception.
TEST(Ledger, DriverSolveRecordsWallSeconds) {
    test_support::F1BoxQp model(0.5);
    SqpDriver driver{SqpOptions{}};

    Ledger ledger;
    driver.attach_ledger(&ledger, "wall");

    const SqpSolution sol = driver.solve(model, model.start_point());
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);

    ASSERT_EQ(ledger.sqp_records().size(), 1u);
    EXPECT_GT(ledger.sqp_records()[0].wall_seconds, 0.0);

    // A second solve on the same driver gets its OWN wall_seconds, not the
    // first's carried forward -- the two need not be equal, but both must be
    // populated (still no magnitude asserted on either).
    const SqpSolution sol2 = driver.solve(model, model.start_point());
    ASSERT_EQ(sol2.status, SqpStatus::kOptimal);
    ASSERT_EQ(ledger.sqp_records().size(), 2u);
    EXPECT_GT(ledger.sqp_records()[1].wall_seconds, 0.0);
}

} // namespace
