#include <random>

#include <gtest/gtest.h>

#include <hven/detail/sqp/qp_engine.h>

#include "support/dense_oracle.h"

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

TEST(QpWarmStart, PerturbedSequenceCollapsesMinorIterations) {
    // Solve random_strictly_convex(6, 4, seed) cold; then perturb g by 1e-3
    // and re-solve warm from the previous solution. Assert (a) same answer as
    // the oracle on the perturbed problem, (b) warm minor_iters <= 2, and
    // (c) warm minor_iters < cold minor_iters on the perturbed problem.
    auto qp = random_strictly_convex(6, 4, 7);
    QpEngine engine{QpOptions{}};
    auto cold = engine.solve(qp);
    qp.g.array() += 1e-3;
    auto oracle = solve_dense_oracle(qp);
    auto warm = engine.solve(qp, cold);
    auto cold2 = engine.solve(qp);
    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-7);
    EXPECT_LE(warm.counters.minor_iters, 2);
    EXPECT_LT(warm.counters.minor_iters, cold2.counters.minor_iters);
}

TEST(QpWarmStart, WarmStartMarginAtBusierOptimum) {
    // Same scenario as PerturbedSequenceCollapsesMinorIterations, but on
    // random_strictly_convex(6, 4, 20), whose optimum carries 2 active
    // inequalities rather than 1. Traced: cold = cold2 = 4, warm = 2 (margin
    // of 2 major iterations collapsed, vs. 1 for seed 7) -- a companion
    // fixture where the warm-start payoff is larger, not a repeat of the
    // plan-mandated seed-7 case above (left untouched).
    auto qp = random_strictly_convex(6, 4, 20);
    QpEngine engine{QpOptions{}};
    auto cold = engine.solve(qp);
    qp.g.array() += 1e-3;
    auto oracle = solve_dense_oracle(qp);
    auto warm = engine.solve(qp, cold);
    auto cold2 = engine.solve(qp);
    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-7);
    EXPECT_LE(warm.counters.minor_iters, 2);
    EXPECT_LT(warm.counters.minor_iters, cold2.counters.minor_iters);
}

TEST(QpWarmStart, StaleSeedStillConverges) {
    // Seed with a deliberately wrong working set: mark every inequality row
    // active even though none of them are violated at the seed point x = 0
    // (random_strictly_convex keeps bi strictly positive, so x = 0 is
    // strictly feasible). The regularized KKT assembly stays factorizable
    // for any working set handed to it (assemble_kkt's delta/mu ridge -- the
    // QPBLUR-style safeguard documented in qp_engine.h's "Degeneracy"
    // section), so no basis repair is needed before the ordinary drop rule
    // can pare the spurious rows away: each is not truly binding, so its
    // priced multiplier comes out negative and drop_worst() removes it,
    // iteration by iteration, until the working set matches the true active
    // set and the point is certified kOptimal.
    const QpProblem qp = random_strictly_convex(6, 4, 7);
    const auto oracle = solve_dense_oracle(qp);

    QpSolution seed;
    seed.status = QpStatus::kOptimal;
    seed.x = Vec::Zero(qp.n());
    seed.bound_state.assign(static_cast<std::size_t>(qp.n()), BoundState::kFree);
    seed.ineq_active.assign(static_cast<std::size_t>(qp.mi()), true);
    seed.lambda_e = Vec::Zero(qp.me());
    seed.lambda_i = Vec::Zero(qp.mi());
    seed.z = Vec::Zero(qp.n());

    const auto warm = QpEngine{QpOptions{}}.solve(qp, seed);

    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-7);

    // Guard against a broken/skipped ingestion path masquerading as success:
    // with bi > 0, x = 0 is strictly feasible, so if seed.ineq_active were
    // silently dropped this degenerates into an ordinary cold solve (which
    // does converge -- it just wouldn't be testing ingestion at all). The
    // seed marks all 4 rows active while the oracle shows only 1 truly is,
    // so ingesting the seed forces 3 rows the engine believes are active but
    // must discover are not. drop_worst() removes at most one candidate per
    // call, and it is only consulted on a major iteration where the EQP step
    // is already negligible (a "checkpoint" iteration) -- so shedding those 3
    // stale rows costs at least 3 checkpoint iterations, plus one more to
    // find nothing left to drop and certify kOptimal: minor_iters >= 3 + 1 =
    // 4 whenever ingestion actually happened. A plain cold solve of this same
    // problem takes only 3 major iterations (traced), so this bound is not
    // reachable by accident if the load were skipped.
    //
    // Mutation-verified: temporarily making start_point() skip the
    // seed->ineq_active loop drops warm.counters.minor_iters to 3 (matching
    // the untouched cold solve exactly) and this assertion catches it; see
    // task-10-report.md's "Fix round 1" section for the recorded counts.
    int stale_rows = 0;
    for (Index i = 0; i < qp.mi(); ++i) {
        if (!oracle.ineq_active[static_cast<std::size_t>(i)]) {
            ++stale_rows;
        }
    }
    EXPECT_GE(warm.counters.minor_iters, stale_rows + 1);
}

// --- Task 5: hot-start K0 factorization reuse -------------------------

TEST(QpWarmStart, WarmResolveWithUnchangedHReusesFactorization) {
    // The headline case: cold-solve on a border-mode engine (the default),
    // perturb g ONLY, and warm re-solve on the SAME engine instance. K0's
    // values depend on H/Ae/Ai/delta/mu -- never on g or b (assemble_kkt_
    // core never reads them) -- and the warm seed IS the cold solve's exact
    // exit working set, so all three reuse conditions hold and the engine
    // must skip K0's assembly and factorization entirely.
    auto qp = random_strictly_convex(6, 4, 20);
    QpEngine engine{QpOptions{}};
    auto cold = engine.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);

    qp.g.array() += 1e-3;
    auto oracle = solve_dense_oracle(qp);
    auto warm = engine.solve(qp, cold);

    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-7);
    EXPECT_EQ(warm.counters.factorizations, 0);
}

// --- Task 2: per-solve overrides join the hot-start reuse key -------------

TEST(QpWarmStart, DeltaMuOverrideForcesRefactorization) {
    // Companion to WarmResolveWithUnchangedHReusesFactorization directly
    // above: IDENTICAL setup (same fixture, same g perturbation, same seed --
    // the reuse-HIT case) except the warm re-solve also overrides dual_mu to
    // 10x its engine default. K0's regularized diagonal is built from the
    // EFFECTIVE dual_mu (assemble_kkt_core), so this changes every value on
    // that diagonal without touching a single H/Ae/Ai byte -- exactly the
    // case reuse-eligibility condition (d) exists to catch. Conditions
    // (a)/(b)/(c) still all hold (this is byte-for-byte the reuse-hit
    // fixture), so a correct engine must refactorize SOLELY because of the
    // resolved (primal_delta, dual_mu) pair, and still land on the right
    // answer for the NEW, larger dual_mu.
    auto qp = random_strictly_convex(6, 4, 20);
    QpEngine engine{QpOptions{}};
    auto cold = engine.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);

    qp.g.array() += 1e-3;
    auto oracle = solve_dense_oracle(qp);

    SolveOverrides overrides;
    overrides.dual_mu = QpOptions{}.dual_mu * 10.0; // 1e-7 vs the engine's 1e-8 default.
    auto warm = engine.solve(qp, cold, overrides);

    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-6);
    EXPECT_GE(warm.counters.factorizations, 1);

    // MUTATION-VERIFY (recorded in task-2-report.md): temporarily dropping
    // `eff_opts.primal_delta == prev_border_effective_delta && eff_opts.
    // dual_mu == prev_border_effective_mu` from qp_engine.h's reuse_eligible
    // expression makes this assertion fail (warm.counters.factorizations
    // reads 0 -- the stale K0, built for the OLD dual_mu, is wrongly reused)
    // while every other test in the suite still passes, since it is the only
    // fixture in the whole battery that changes dual_mu between two solve()
    // calls on one engine instance.
}

TEST(QpWarmStart, HChangeForcesRefactorization) {
    // Companion to the above: perturb H's (0,0) diagonal instead of g. H's
    // values-hash fingerprint now differs from what the persisted K0
    // factorization was built from, so reuse must NOT be taken even though
    // the seed working set still matches the previous exit exactly -- the
    // engine refactorizes K0 exactly once (symbolic reuse still applies
    // inside KktSystem since the sparsity PATTERN is unchanged).
    auto qp = random_strictly_convex(6, 4, 20);
    QpEngine engine{QpOptions{}};
    auto cold = engine.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);

    qp.H.coeffRef(0, 0) += 1e-3;
    auto oracle = solve_dense_oracle(qp);
    auto warm = engine.solve(qp, cold);

    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-7);
    EXPECT_EQ(warm.counters.factorizations, 1);
}

TEST(QpWarmStart, SeedWorkingSetMismatchForcesRefactorization) {
    // Condition (b) pinning test. H/Ae/Ai are untouched (only g is
    // perturbed, same as the reuse-hit case above), but the SEED handed to
    // the warm solve is deliberately NOT the previous exit's working set --
    // every inequality row is marked active, mirroring StaleSeedStillConverges
    // -- so the persisted border stack does not represent this seed and
    // reuse must not be taken.
    auto qp = random_strictly_convex(6, 4, 20);
    QpEngine engine{QpOptions{}};
    auto cold = engine.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);

    qp.g.array() += 1e-3;
    auto oracle = solve_dense_oracle(qp);

    QpSolution seed = cold;
    seed.ineq_active.assign(static_cast<std::size_t>(qp.mi()), true);
    ASSERT_NE(seed.ineq_active, cold.ineq_active)
        << "fixture must actually differ from cold's exit working set";

    auto warm = engine.solve(qp, seed);

    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-7);
    EXPECT_GE(warm.counters.factorizations, 1);
}

TEST(QpWarmStart, InfeasibleExitInvalidatesReuseCache) {
    // Invalidation-policy test. Build a QP that is genuinely kInfeasible
    // THROUGH THE LOOP (contradictory inequality rows, not the crossed-
    // bounds short-circuit, which never touches the border machinery at
    // all), so the engine's persisted border_ state after that solve is a
    // real, populated (if uncertified) border stack. Then repair only `bi`
    // (K0's values never depend on bi) and warm re-solve using the
    // infeasible solve's own exit as the seed.
    //
    // Were the invalidation policy broken (e.g. armed on any exit rather
    // than only kOptimal), this seed would trivially satisfy condition (b)
    // -- it IS the previous "exit" working set -- and conditions (a)/(c)
    // also hold (H/Ai untouched), so a broken policy would wrongly skip
    // refactorization here. The correct policy forces at least one.
    QpProblem qp;
    qp.H = SpMatU(2, 2);
    qp.H.insert(0, 0) = 1.0;
    qp.H.insert(1, 1) = 1.0;
    qp.H.makeCompressed();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(2, 2);
    qp.Ai.insert(0, 0) = 1.0;  // row 0: x0 <= bi(0)
    qp.Ai.insert(1, 0) = -1.0; // row 1: -x0 <= bi(1)
    qp.Ai.makeCompressed();
    qp.bi = Vec(2);
    qp.bi(0) = -1.0; // x0 <= -1
    qp.bi(1) = -1.0; // -x0 <= -1  =>  x0 >= 1  (contradicts row 0)
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);

    QpEngine engine{QpOptions{}};
    auto infeasible = engine.solve(qp);
    ASSERT_EQ(infeasible.status, QpStatus::kInfeasible);

    qp.bi(1) = 10.0; // -x0 <= 10  =>  x0 >= -10, trivially satisfied: now feasible.
    auto oracle = solve_dense_oracle(qp);
    auto warm = engine.solve(qp, infeasible);

    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-7);
    EXPECT_GE(warm.counters.factorizations, 1);
}

TEST(QpWarmStart, StructureChangeAtConstantValueBytesForcesRefactorization) {
    // Condition (a) pinning test. mix_values (detail::values_hash's helper)
    // mixes rows/cols/nnz as a separator ahead of the value bytes, which
    // already catches a SHAPE change (e.g. an mi change) on its own -- so
    // proving structural_hash is genuinely necessary needs a collision that
    // survives that hardening too: qp1 and qp2 below have IDENTICAL rows,
    // cols, and nnz for Ai, and the exact same value byte sequence in
    // row-major iteration order ([1.0, -1.0] from row 0 then row 1), so
    // detail::values_hash(qp1) == detail::values_hash(qp2) even after
    // hardening. What differs is WHICH COLUMN row 1's entry sits in (col 1
    // in qp1, col 0 in qp2) -- i.e. row 1 constrains a different variable --
    // which only detail::structural_hash (mixing the actual index arrays)
    // can tell apart. A reuse decision that consulted values_hash without
    // structural_hash would carry qp1's K0 (row 1 touching x1) into qp2's
    // border stack (row 1 touching x0 instead), which the reviewer traced to
    // out-of-range rhs indexing against the stale K0 in the general case.
    //
    // Both rows are made genuinely active in both problems (not merely
    // present-but-inactive), and qp2 is warm-solved directly from qp1's own
    // exit -- same mi, same n, so the seed is ingested for real rather than
    // matching vacuously -- so condition (b) is satisfied by the ordinary
    // warm-start path this whole file otherwise exercises.
    QpProblem qp1; // Ai: row 0 = [1, 0], row 1 = [0, -1] (row 1 touches x1).
    qp1.H = SpMatU(2, 2);
    qp1.H.insert(0, 0) = 2.0;
    qp1.H.insert(1, 1) = 2.0;
    qp1.H.makeCompressed();
    qp1.g = Vec::Zero(2);
    qp1.Ae.resize(0, 2);
    qp1.be = Vec(0);
    qp1.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(2, 2);
    qp1.Ai.insert(0, 0) = 1.0;
    qp1.Ai.insert(1, 1) = -1.0;
    qp1.Ai.makeCompressed();
    qp1.bi = Vec(2);
    qp1.bi(0) = -1.0; // x0 <= -1: binding (unconstrained minimizer is x0 = 0).
    qp1.bi(1) = -1.0; // -x1 <= -1  =>  x1 >= 1: binding (unconstrained minimizer is x1 = 0).
    qp1.lower = Vec::Constant(2, -10.0);
    qp1.upper = Vec::Constant(2, 10.0);

    QpEngine engine{QpOptions{}};
    auto cold1 = engine.solve(qp1);
    ASSERT_EQ(cold1.status, QpStatus::kOptimal);
    ASSERT_TRUE(cold1.ineq_active[0]);
    ASSERT_TRUE(cold1.ineq_active[1]);

    // Ai: row 0 = [1, 0] (unchanged), row 1 = [-1, 0] (now touches x0, not
    // x1) -- same rows/cols/nnz, same value bytes [1.0, -1.0], different
    // column pattern.
    QpProblem qp2 = qp1;
    qp2.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(2, 2);
    qp2.Ai.insert(0, 0) = 1.0;
    qp2.Ai.insert(1, 0) = -1.0;
    qp2.Ai.makeCompressed();
    qp2.bi(0) = -1.0; // x0 <= -1 (same as qp1).
    qp2.bi(1) = 1.0;  // -x0 <= 1  =>  x0 >= -1: paired with row 0, pins x0 = -1;
                      // x1 is now unconstrained by Ai entirely.
    ASSERT_EQ(detail::values_hash(qp1), detail::values_hash(qp2))
        << "fixture must actually collide under values_hash for this to pin structural_hash";
    ASSERT_NE(detail::structural_hash(qp1), detail::structural_hash(qp2))
        << "fixture must actually differ under structural_hash to be a meaningful test";

    auto oracle = solve_dense_oracle(qp2);
    auto warm = engine.solve(qp2, cold1);

    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-7);
    EXPECT_GE(warm.counters.factorizations, 1);
}

TEST(QpWarmStart, ExceptionBetweenSolvesInvalidatesReuseCache) {
    // Invalidation-policy test, exception variant (companion to
    // InfeasibleExitInvalidatesReuseCache above). A malformed problem thrown
    // from qp.validate() sits BETWEEN two solves that would otherwise take
    // the reuse fast path exactly like WarmResolveWithUnchangedHReusesFactorization
    // above -- same problem, only g perturbed, same seed. The pessimistic
    // invalidation at the very top of run() must still have cleared
    // border_valid_ even though the throw happens before border_candidate
    // (or even ws construction) ever runs.
    auto qp = random_strictly_convex(6, 4, 20);
    QpEngine engine{QpOptions{}};
    auto cold = engine.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);

    QpProblem bad = qp;
    bad.g = Vec(qp.n() + 1); // dimension mismatch: validate() throws before anything else runs.
    EXPECT_THROW(engine.solve(bad), std::invalid_argument);

    qp.g.array() += 1e-3;
    auto oracle = solve_dense_oracle(qp);
    auto warm = engine.solve(qp, cold);

    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - oracle.x).norm(), 1e-7);
    EXPECT_GE(warm.counters.factorizations, 1);
}

// n=2, one general row, one bound -- test_qp_engine_border.cpp's own
// simple_box_qp() shape, parameterized so two problems can share H/Ai
// (hash-identical) while their optima differ (c0/c1/bi never enter the
// hash). Repeated here rather than shared, matching this file's own
// "self-contained" convention (see random_strictly_convex's own comment).
QpProblem row_and_bound_qp(double c0, double c1, double bi) {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -c0, -c1;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, bi);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

// FIX ROUND 2 (re-review finding 2b): hot_state() must emit the COMMITTED
// generation (border_generation_, this engine's own last-trusted value),
// never a LIVE re-read off the shared object -- the re-review's R3 probe,
// reproduced here at the public QpEngine API. P emits h1; a second engine C
// adopts h1 against a value-consistent problem (same H/Ai, only c0/c1/bi
// differ, so C's own gate passes and it is never detached -- see
// HotState's OWNERSHIP note for why that is the only way the shared object
// is ever mutated) and needs one schur_cap-forced mid-solve rebuild,
// bumping the SHARED object's live generation. P -- which never solves
// again -- then calls hot_state() a SECOND time. Before this fix, reading
// `border_->generation` live would emit a SELF-CONSISTENT FORGED handle:
// P's own fingerprint fields (still describing qpA) paired with a
// generation that actually describes C's mutation, which a later adopter's
// condition (e) would wrongly accept.
TEST(QpWarmStart, HotStateEmitsCommittedGenerationNotLive) {
    QpOptions opts;
    opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    opts.schur_cap = 1;

    QpProblem qpA = row_and_bound_qp(1.0, 2.0, 1.0); // optimum (0, 1): row + x0 bound active
    QpEngine engine_p{opts};
    const QpSolution sol_a = engine_p.solve(qpA);
    ASSERT_EQ(sol_a.status, QpStatus::kOptimal);
    const auto h1 = engine_p.hot_state();
    ASSERT_NE(h1, nullptr);
    const std::uint64_t h1_generation = h1->generation;

    // optimum (0, 0): BOTH bounds active, the row inactive -- a genuine
    // shape change from qpA's own exit, which is what forces the mid-solve
    // rebuild under schur_cap == 1 (test_qp_engine_border.cpp's own
    // RebuildUnderTinySchurCapMatchesRefactorize technique).
    QpProblem qpB = row_and_bound_qp(-1.0, -1.0, 5.0);
    QpSolution seed = sol_a;
    seed.x = Vec::Zero(2);
    QpEngine engine_c{opts};
    const QpSolution sol_c = engine_c.solve(qpB, seed, SolveOverrides{}, h1);
    ASSERT_EQ(sol_c.status, QpStatus::kOptimal);
    ASSERT_TRUE(sol_c.counters.k0_reused)
        << "C's own gate must pass (byte-identical H/Ai) for it to ever touch the shared object "
           "at all -- a refused adoption would detach instead, per Q3, and never reach it";
    ASSERT_GE(sol_c.counters.factorizations, 1)
        << "and it must have actually rebuilt, which is what bumps the SHARED generation";

    const auto h2 = engine_p.hot_state();
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->border.get(), h1->border.get()) << "still the same shared object";
    EXPECT_EQ(h2->generation, h1_generation)
        << "hot_state() must emit the COMMITTED generation, not a live re-read off the "
           "possibly-shared object -- THE PIN for the forged-handle scenario";
}

} // namespace
