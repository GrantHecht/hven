#include <gtest/gtest.h>
#include <tycho_sqp/kkt_system.h>
using namespace tycho::sqp;

TEST(KktPartialSolve, ComposedPartialsMatchFullSolve) {
    Eigen::MatrixXd D(3, 3);
    D << 2, 0, 1, 0, 3, 1, 1, 1, 0;
    SpMatU K = D.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    K.makeCompressed();
    KktSystem kkt{QpOptions{}};
    kkt.factorize(K);
#ifdef USE_ACCELERATE_SPARSE
    // OBSERVED (results note §(b)): the gate is hardwired false on this
    // backend -- bare L/D/Lᵀ subfactor solves run in Accelerate's internal
    // AMD-permuted, equilibration-scaled basis, so their composition diverges
    // O(1) from the full solve EVEN ON THIS CLEAN FACTORIZATION (measured
    // ‖x_full − x_composed‖ = 3.355e+00 here, full-solve residual 2.264e-15).
    // Unlike Pardiso, whose phases 331/332/333 fold P and scaling in, no
    // trustworthy three-stage composition exists, so the divergence -- not
    // the match -- is the pinned behavior.
    ASSERT_FALSE(kkt.supports_partial_solve());
    Vec b(3);
    b << 1, 2, 3;
    Vec x_full = kkt.solve(b);
    Vec x_composed = kkt.solve_backward(kkt.solve_diagonal(kkt.solve_forward(b)));
    EXPECT_GT((x_full - x_composed).norm(), 1.0);
#else
    ASSERT_TRUE(kkt.supports_partial_solve());
    Vec b(3);
    b << 1, 2, 3;
    Vec x_full = kkt.solve(b);
    Vec x_composed = kkt.solve_backward(kkt.solve_diagonal(kkt.solve_forward(b)));
    EXPECT_LT((x_full - x_composed).norm(), 1e-12);
#endif
}

// Regression pin for WHY supports_partial_solve() is gated per-factorization:
// when pardiso perturbs a pivot (here: the exactly-singular matrix from
// InertiaProbe.ExactlySingular), the composed partial solves diverge from the
// full phase-33 solve by O(1e8) with no error raised. Observed (fix round 1):
// x_full = [3, 4.0e8, -2] vs x_composed = [3, 2.0e8, -2]. The gate
// supports_partial_solve() == (active && num_perturbed_pivots() == 0) is what
// keeps callers (Task 8's Schur path) off this silently-wrong branch. See
// docs/notes/2026-07-27-pardiso-inertia-findings.md.
//
// On Accelerate this test passes with the same assertions but partly for a
// different reason: composition diverges from unfolded P/S regardless of
// pivot health, and the counter's 1 is a zero-pivot count; see
// docs/notes/2026-07-29-accelerate-audit-results.md.
TEST(KktPartialSolve, GateClosesOnPerturbedPivots) {
    Eigen::MatrixXd D(3, 3);
    D << 1, 0, 1, 0, 0, 0, 1, 0, 0;
    SpMatU K = D.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    K.makeCompressed();
    KktSystem kkt{QpOptions{}};
    kkt.factorize(K);
    ASSERT_EQ(kkt.num_perturbed_pivots(), 1); // precondition for this pin
    EXPECT_FALSE(kkt.supports_partial_solve());

    // Composition computed anyway, to pin the divergence the gate guards.
    Vec b(3);
    b << 1, 2, 3;
    Vec x_full = kkt.solve(b);
    Vec x_composed = kkt.solve_backward(kkt.solve_diagonal(kkt.solve_forward(b)));
    EXPECT_GT((x_full - x_composed).norm(), 1.0);
}
