#include <gtest/gtest.h>
#include <hven/detail/kkt/kkt_calls.h>

#include <fmt/format.h>

using namespace hven::solvers;
using hven::SpMatRM;
using hven::linear::InertiaEvidence;

namespace {

// hven's SymmetricFactor validates the STRUCTURAL diagonal at analyze() --
// every row's diagonal entry must be present in the pattern, value zero or
// not (symmetric_factor.h; the engine's own assemblies emit it
// unconditionally). The dissolved seam forwarded the pattern unvalidated, so
// these dense-built fixtures could drop the zero diagonal via sparseView();
// this builder keeps it explicit, exactly as test_kkt_calls.cpp's fixture
// does.
static SpMatRM upper_with_structural_diag(const Eigen::MatrixXd &D) {
    SpMatRM K(D.rows(), D.cols());
    for (Eigen::Index r = 0; r < D.rows(); ++r) {
        for (Eigen::Index c = r; c < D.cols(); ++c) {
            if (r == c || D(r, c) != 0.0) {
                K.insert(r, c) = D(r, c);
            }
        }
    }
    K.makeCompressed();
    return K;
}

} // namespace

// Near-singular KKT: H = diag(1, eps), A = [1 0]. As eps -> 0 the reduced
// Hessian degenerates; probe what the backend reports for inertia and
// perturbed-pivot counts across eps = 1e-6 .. 1e-14, read through the SQP
// configuration (sqp_kkt_options() via KktFactor) as InertiaEvidence.
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
        SpMatRM K = upper_with_structural_diag(D);
        detail::KktFactor kkt;
        const hven::linear::FactorizeOutcome outcome = detail::factorize_checked(kkt, K);
        ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk) << "eps=" << eps;
        const InertiaEvidence e = outcome.inertia;

        const std::string tag = fmt::format("{:.0e}", eps);
        RecordProperty(fmt::format("eps_{}", tag).c_str(), fmt::format("{:.3e}", eps));
        RecordProperty(fmt::format("pos_{}", tag).c_str(), std::to_string(e.n_pos));
        RecordProperty(fmt::format("neg_{}", tag).c_str(), std::to_string(e.n_neg));
        RecordProperty(fmt::format("perturbed_{}", tag).c_str(),
                       std::to_string(e.perturbed_pivots.value_or(-1)));

        // Exact analytic inertia is (2, 1, 0) for every eps > 0 (the decoupled
        // diagonal entry contributes one positive eigenvalue == eps; the
        // remaining 2x2 block [[1,1],[1,0]] contributes one positive and one
        // negative eigenvalue independent of eps). Observed: pardiso reports
        // this exactly, with no perturbed pivots, at eps in
        // {1e-6, 1e-10, 1e-12, 1e-14}.
        ASSERT_EQ(e.state, InertiaEvidence::State::kObserved) << "eps=" << eps;
        EXPECT_EQ(e.n_pos, 2) << "eps=" << eps;
        EXPECT_EQ(e.n_neg, 1) << "eps=" << eps;
#ifdef USE_ACCELERATE_SPARSE
        // Accelerate carries no perturbed-pivot counter; absence is the
        // honest state (InertiaEvidence's own contract) and the zero class
        // is measured natively.
        EXPECT_FALSE(e.perturbed_pivots.has_value()) << "eps=" << eps;
        EXPECT_FALSE(e.zero_is_derived) << "eps=" << eps;
        EXPECT_EQ(e.n_zero, 0) << "eps=" << eps;
#else
        ASSERT_TRUE(e.perturbed_pivots.has_value()) << "eps=" << eps;
        EXPECT_EQ(*e.perturbed_pivots, 0) << "eps=" << eps;
#endif
    }
}

// eps = 0 exactly: the diagonal entry is structurally zero, so the matrix has
// one exact zero eigenvalue (true inertia is (1 pos, 1 neg, 1 zero) -- pardiso
// has no "zero" bucket, so n_pos + n_neg cannot equal 3
// unless it treats the zero pivot as one sign or the other).
//
// PINNED observed behavior: pardiso does NOT throw (no error -4). It silently
// perturbs the single zero pivot (perturbed_pivots == 1) and reports it
// as if it were positive, yielding n_pos == 2, n_neg == 1 --
// i.e. inertia that looks exactly like the near-singular (eps > 0) case above,
// with no signal other than the perturbed-pivot counter to distinguish "exact
// analytic inertia" from "one pivot was fabricated by the solver."
//
// ACCELERATE CONTRAST (docs/notes/2026-07-29-accelerate-audit-results.md
// §(a)): LDLTTPP also does not throw, but it reports the TRUE analytic
// inertia (1, 1, 1) -- the zero pivot lands in Accelerate's own zero bucket
// rather than being fabricated as positive. Through InertiaEvidence that
// zero bucket is now the NATIVELY-MEASURED n_zero (zero_is_derived ==
// false), with perturbed_pivots honestly absent -- the same trust signal the
// dissolved twin folded into its perturbed-pivot reading, carried through
// the honest channel (docs/retarget-design-sqp.md §4.1).
TEST(InertiaProbe, ExactlySingular) {
    Eigen::MatrixXd D(3, 3);
    D << 1, 0, 1, 0, 0, 0, 1, 0, 0;
    SpMatRM K = upper_with_structural_diag(D);
    detail::KktFactor kkt;

    hven::linear::FactorizeOutcome outcome;
    EXPECT_NO_THROW(outcome = detail::factorize_checked(kkt, K));
    ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk);
    const InertiaEvidence e = outcome.inertia;
    ASSERT_EQ(e.state, InertiaEvidence::State::kObserved);

    RecordProperty("pos", std::to_string(e.n_pos));
    RecordProperty("neg", std::to_string(e.n_neg));
    RecordProperty("perturbed", std::to_string(e.perturbed_pivots.value_or(-1)));
    RecordProperty("zero", std::to_string(e.n_zero));

// OBSERVED on Accelerate (2026-07-29, results note §(a)): LDLTTPP completes
// without error or SparseMatrixIsSingular and reports the TRUE analytic
// inertia (1, 1, 1) -- the zero pivot lands in the zero bucket (natively
// measured through n_zero on this backend), instead of being silently
// fabricated as positive the way Pardiso's perturbation does. Same trust
// signal (a nonzero zero class), honest sign counts.
#ifdef USE_ACCELERATE_SPARSE
    EXPECT_EQ(e.n_pos, 1);
    EXPECT_EQ(e.n_neg, 1);
    EXPECT_FALSE(e.zero_is_derived);
    EXPECT_EQ(e.n_zero, 1);
    EXPECT_FALSE(e.perturbed_pivots.has_value());
#else
    EXPECT_EQ(e.n_pos, 2);
    EXPECT_EQ(e.n_neg, 1);
    ASSERT_TRUE(e.perturbed_pivots.has_value());
    EXPECT_EQ(*e.perturbed_pivots, 1);
#endif
}
