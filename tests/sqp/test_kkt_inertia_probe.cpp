#include <gtest/gtest.h>
#include <tycho_sqp/kkt_system.h>

#include <fmt/format.h>

using namespace tycho::sqp;

// Near-singular KKT: H = diag(1, eps), A = [1 0]. As eps -> 0 the reduced
// Hessian degenerates; probe what pardiso reports for inertia and
// perturbed-pivot counts across eps = 1e-6 .. 1e-14.
//
// NOTE: gtest's RecordProperty() overwrites any previously recorded property
// with the same key within a test, so per-iteration keys must be unique (an
// earlier draft of this test reused "eps"/"pos"/"neg"/"perturbed" across the
// loop and silently lost every case but the last).
//
// PINNED (see docs/notes/2026-07-27-pardiso-inertia-findings.md for the full
// table and derivation): with pardiso's default settings (iparm[12] = 1,
// maximum-weight matching/scaling enabled), this decoupled-diagonal
// near-singular block factors with the *exact* analytic inertia (2, 1) and
// *zero* perturbed pivots at every tested eps down to 1e-14 -- there is no
// gradual degradation to pin here, the reported inertia is exact throughout
// the probed range.
TEST(InertiaProbe, NearSingularReducedHessian) {
    for (double eps : {1e-6, 1e-10, 1e-12, 1e-14}) {
        Eigen::MatrixXd D(3, 3);
        D << 1, 0, 1, 0, eps, 0, 1, 0, 0;
        SpMatU K = D.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
        K.makeCompressed();
        KktSystem kkt{QpOptions{}};
        kkt.factorize(K);

        const std::string tag = fmt::format("{:.0e}", eps);
        RecordProperty(fmt::format("eps_{}", tag).c_str(), fmt::format("{:.3e}", eps));
        RecordProperty(fmt::format("pos_{}", tag).c_str(), std::to_string(kkt.num_pos_eigs()));
        RecordProperty(fmt::format("neg_{}", tag).c_str(), std::to_string(kkt.num_neg_eigs()));
        RecordProperty(fmt::format("perturbed_{}", tag).c_str(),
                       std::to_string(kkt.num_perturbed_pivots()));

        // Exact analytic inertia is (2, 1, 0) for every eps > 0 (the decoupled
        // diagonal entry contributes one positive eigenvalue == eps; the
        // remaining 2x2 block [[1,1],[1,0]] contributes one positive and one
        // negative eigenvalue independent of eps). Observed: pardiso reports
        // this exactly, with no perturbed pivots, at eps in
        // {1e-6, 1e-10, 1e-12, 1e-14}.
        EXPECT_EQ(kkt.num_pos_eigs(), 2) << "eps=" << eps;
        EXPECT_EQ(kkt.num_neg_eigs(), 1) << "eps=" << eps;
        EXPECT_EQ(kkt.num_perturbed_pivots(), 0) << "eps=" << eps;
    }
}

// eps = 0 exactly: the diagonal entry is structurally zero, so the matrix has
// one exact zero eigenvalue (true inertia is (1 pos, 1 neg, 1 zero) -- pardiso
// has no "zero" bucket, so num_pos_eigs() + num_neg_eigs() cannot equal 3
// unless it treats the zero pivot as one sign or the other).
//
// PINNED observed behavior: pardiso does NOT throw (no error -4). It silently
// perturbs the single zero pivot (num_perturbed_pivots() == 1) and reports it
// as if it were positive, yielding num_pos_eigs() == 2, num_neg_eigs() == 1 --
// i.e. inertia that looks exactly like the near-singular (eps > 0) case above,
// with no signal other than the perturbed-pivot counter to distinguish "exact
// analytic inertia" from "one pivot was fabricated by the solver."
//
// ACCELERATE CONTRAST (docs/notes/2026-07-29-accelerate-audit-results.md
// §(a)): LDLTTPP also does not throw, but it reports the TRUE analytic
// inertia (1, 1, 1) -- the zero pivot lands in Accelerate's own zero bucket
// rather than being fabricated as positive -- with num_perturbed_pivots()
// (mapped to that zero bucket on this backend) still reading 1. Same trust
// signal, honest sign counts; see the results note for the full comparison.
TEST(InertiaProbe, ExactlySingular) {
    Eigen::MatrixXd D(3, 3);
    D << 1, 0, 1, 0, 0, 0, 1, 0, 0;
    SpMatU K = D.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    K.makeCompressed();
    KktSystem kkt{QpOptions{}};

    EXPECT_NO_THROW(kkt.factorize(K));

    RecordProperty("pos", std::to_string(kkt.num_pos_eigs()));
    RecordProperty("neg", std::to_string(kkt.num_neg_eigs()));
    RecordProperty("perturbed", std::to_string(kkt.num_perturbed_pivots()));

// OBSERVED on Accelerate (2026-07-29, results note §(a)): LDLTTPP completes
// without error or SparseMatrixIsSingular and reports the TRUE analytic
// inertia (1, 1, 1) -- the zero pivot lands in the zero bucket, which
// num_perturbed_pivots() maps to on this backend, instead of being silently
// fabricated as positive the way Pardiso's perturbation does. Same trust
// signal (counter == 1), honest sign counts.
#ifdef USE_ACCELERATE_SPARSE
    EXPECT_EQ(kkt.num_pos_eigs(), 1);
    EXPECT_EQ(kkt.num_neg_eigs(), 1);
    EXPECT_EQ(kkt.num_perturbed_pivots(), 1);
#else
    EXPECT_EQ(kkt.num_pos_eigs(), 2);
    EXPECT_EQ(kkt.num_neg_eigs(), 1);
    EXPECT_EQ(kkt.num_perturbed_pivots(), 1);
#endif
}
