#include <gtest/gtest.h>
#include <hven/detail/kkt/kkt_calls.h>

using namespace hven::solvers;
using hven::SpMatRM;
using hven::Vec;
using hven::linear::SymmetricFactor;

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

// Composes backward(diagonal(forward(rhs))) through the SQP configuration's
// factor -- the migrated pins run through sqp_kkt_options(), not the
// backend's default Options, so they assert the SQP seam's behavior (this
// suite is hven's partial-solve path's first real consumer,
// docs/retarget-design-sqp.md §10 item 1).
Vec composed_partials(const detail::KktFactor &kkt, const Vec &b) {
    Vec y(b.size());
    Vec z(b.size());
    Vec x(b.size());
    kkt.factor.solve_partial(SymmetricFactor::SolvePhase::kForward, b, y);
    kkt.factor.solve_partial(SymmetricFactor::SolvePhase::kDiagonal, y, z);
    kkt.factor.solve_partial(SymmetricFactor::SolvePhase::kBackward, z, x);
    return x;
}

} // namespace

TEST(KktPartialSolve, ComposedPartialsMatchFullSolve) {
    Eigen::MatrixXd D(3, 3);
    D << 2, 0, 1, 0, 3, 1, 1, 1, 0;
    SpMatRM K = upper_with_structural_diag(D);
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K);
#ifdef USE_ACCELERATE_SPARSE
    // OBSERVED (results note §(b)): the gate is false on this backend --
    // bare L/D/Lᵀ subfactor solves run in Accelerate's internal AMD-permuted,
    // equilibration-scaled basis, so their composition diverges O(1) from the
    // full solve EVEN ON THIS CLEAN FACTORIZATION (measured through the
    // dissolved seam: ‖x_full − x_composed‖ = 3.355e+00 here, full-solve
    // residual 2.264e-15; hven's supports_partial_solve() is false always on
    // Accelerate because no perturbation evidence exists there, so
    // composability is unverifiable -- symmetric_factor.h). Unlike Pardiso,
    // whose phases 331/332/333 fold P and scaling in, no trustworthy
    // three-stage composition exists, so the divergence -- not the match --
    // is the pinned behavior.
    ASSERT_FALSE(kkt.factor.supports_partial_solve());
    Vec b(3);
    b << 1, 2, 3;
    Vec x_full = detail::solve_vec(kkt, b);
    Vec x_composed = composed_partials(kkt, b);
    EXPECT_GT((x_full - x_composed).norm(), 1.0);
#else
    ASSERT_TRUE(kkt.factor.supports_partial_solve());
    Vec b(3);
    b << 1, 2, 3;
    Vec x_full = detail::solve_vec(kkt, b);
    Vec x_composed = composed_partials(kkt, b);
    EXPECT_LT((x_full - x_composed).norm(), 1e-12);
#endif
}

// Regression pin for WHY supports_partial_solve() is gated per-factorization:
// when pardiso perturbs a pivot (here: the exactly-singular matrix from
// InertiaProbe.ExactlySingular), the composed partial solves diverge from the
// full phase-33 solve by O(1e8) with no error raised. Observed (fix round 1):
// x_full = [3, 4.0e8, -2] vs x_composed = [3, 2.0e8, -2]. The gate --
// supports_partial_solve() false on any perturbed pivot (and false without a
// usable factorization at all, symmetric_factor.h's pinned
// false-before-factorization contract) -- is what keeps callers (Task 8's
// Schur path) off this silently-wrong branch. See
// docs/notes/2026-07-27-pardiso-inertia-findings.md.
//
// On Accelerate this test passes with the same assertions but partly for a
// different reason: composition diverges from unfolded P/S regardless of
// pivot health (the gate is false always there), and the trust signal for
// this matrix is the natively-measured zero class rather than a
// perturbed-pivot count; see
// docs/notes/2026-07-29-accelerate-audit-results.md and
// docs/retarget-design-sqp.md §4.1.
TEST(KktPartialSolve, GateClosesOnPerturbedPivots) {
    Eigen::MatrixXd D(3, 3);
    D << 1, 0, 1, 0, 0, 0, 1, 0, 0;
    SpMatRM K = upper_with_structural_diag(D);
    detail::KktFactor kkt;
    const hven::linear::FactorizeOutcome outcome = detail::factorize_checked(kkt, K);
    ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk);
#ifdef USE_ACCELERATE_SPARSE
    ASSERT_EQ(outcome.inertia.n_zero, 1); // precondition: the zero-pivot trust signal
#else
    ASSERT_TRUE(outcome.inertia.perturbed_pivots.has_value());
    ASSERT_EQ(*outcome.inertia.perturbed_pivots, 1); // precondition for this pin
#endif
    EXPECT_FALSE(kkt.factor.supports_partial_solve());

    // Composition computed anyway, to pin the divergence the gate guards.
    Vec b(3);
    b << 1, 2, 3;
    Vec x_full = detail::solve_vec(kkt, b);
    Vec x_composed = composed_partials(kkt, b);
    EXPECT_GT((x_full - x_composed).norm(), 1.0);
}
