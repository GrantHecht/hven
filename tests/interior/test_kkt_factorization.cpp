// The engine-side KKT factorization: lifecycle, and the evidence projection
// the inertia machinery reads. The projection is the interesting half -- it is
// where the linear layer's honest, optional evidence becomes the plain
// integers the engine consumes -- so each row of it is checked against a
// system whose inertia is known by hand.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/interior/kkt_factorization.h"
#include "hven/model/nlp_solver.h"

namespace {

using hven::SpMatRM;
using hven::solvers::KktFactorization;

// Upper triangle of a symmetric matrix, in the row-major CSR convention the
// factor requires: a structural diagonal in every row, nothing below it.
SpMatRM kkt_upper_from_triplets(int n, const std::vector<Eigen::Triplet<double>> &entries) {
    SpMatRM m(n, n);
    m.setFromTriplets(entries.begin(), entries.end());
    m.makeCompressed();
    return m;
}

// diag(2, -3): one positive and one negative eigenvalue, so the inertia the
// backend reports is known without solving anything.
std::vector<Eigen::Triplet<double>> indefinite_2x2() { return {{0, 0, 2.0}, {1, 1, -3.0}}; }

KktFactorization::Options mkl_like_options() {
    KktFactorization::Options opts;
    opts.kind = hven::linear::FactorKind::kLDLT;
    opts.num_threads = 1;
    return opts;
}

TEST(KktFactorizationTest, ComputeReportsTheInertiaTheEngineReads) {
    KktFactorization kkt(mkl_like_options());
    kkt.matrix() = kkt_upper_from_triplets(2, indefinite_2x2());

    kkt.compute();

    EXPECT_EQ(kkt.peigs(), 1);
    EXPECT_EQ(kkt.neigs(), 1);
    EXPECT_EQ(kkt.info(), Eigen::Success);
    // The perturbed-pivot projection is an integer whichever backend produced
    // it: a count where one exists, and 0 -- not an absence the engine has no
    // way to read -- where none does.
    EXPECT_GE(kkt.ppivs(), 0);
    // A factor size is always reported, in whichever unit this backend uses.
    EXPECT_GT(kkt.factor_mem(), 0);
}

TEST(KktFactorizationTest, RefactorizeReusesTheSymbolicAndTracksNewValues) {
    KktFactorization kkt(mkl_like_options());
    kkt.matrix() = kkt_upper_from_triplets(2, indefinite_2x2());
    kkt.compute();

    // Same pattern, both diagonal entries now positive: the inertia moves and
    // the numeric factorization alone is enough to see it.
    kkt.matrix().coeffRef(1, 1) = 5.0;
    kkt.refactorize();

    EXPECT_EQ(kkt.peigs(), 2);
    EXPECT_EQ(kkt.neigs(), 0);
    EXPECT_EQ(kkt.info(), Eigen::Success);
}

TEST(KktFactorizationTest, SolveReturnsTheNewtonDirectionForTheAssembledSystem) {
    KktFactorization kkt(mkl_like_options());
    kkt.matrix() = kkt_upper_from_triplets(2, indefinite_2x2());
    kkt.compute();

    Eigen::VectorXd rhs(2);
    rhs << 4.0, 9.0;
    Eigen::VectorXd x(2);
    kkt.solve(rhs, x);

    EXPECT_NEAR(x[0], 2.0, 1e-12);
    EXPECT_NEAR(x[1], -3.0, 1e-12);
}

TEST(KktFactorizationTest, RefactorizeBeforeAnySymbolicAnalysisThrows) {
    KktFactorization kkt(mkl_like_options());
    kkt.matrix() = kkt_upper_from_triplets(2, indefinite_2x2());

    EXPECT_THROW(kkt.refactorize(), std::runtime_error);
}

TEST(KktFactorizationTest, RefactorizeAgainstADifferentPatternThrows) {
    KktFactorization kkt(mkl_like_options());
    kkt.matrix() = kkt_upper_from_triplets(2, indefinite_2x2());
    kkt.compute();

    // A pattern change is what compute() exists for; reaching it through
    // refactorize() would factorize a structure the symbolic never saw.
    kkt.matrix() = kkt_upper_from_triplets(2, {{0, 0, 2.0}, {0, 1, 1.0}, {1, 1, -3.0}});
    EXPECT_THROW(kkt.refactorize(), std::invalid_argument);

    // compute() is the recovery, and it leaves the object usable again.
    EXPECT_NO_THROW(kkt.compute());
    EXPECT_EQ(kkt.info(), Eigen::Success);
}

TEST(KktFactorizationTest, ReleaseDropsTheBufferAndTheFactorization) {
    KktFactorization kkt(mkl_like_options());
    kkt.matrix() = kkt_upper_from_triplets(2, indefinite_2x2());
    kkt.compute();

    kkt.release();

    EXPECT_EQ(kkt.matrix().rows(), 0);
    EXPECT_EQ(kkt.matrix().cols(), 0);
    Eigen::VectorXd rhs = Eigen::VectorXd::Zero(2);
    Eigen::VectorXd x(2);
    EXPECT_THROW(kkt.solve(rhs, x), std::runtime_error);
}

TEST(KktFactorizationTest, ReconfigureRebuildsTheFactorAndDropsTheAnalysis) {
    KktFactorization kkt(mkl_like_options());
    kkt.matrix() = kkt_upper_from_triplets(2, indefinite_2x2());
    kkt.compute();

    KktFactorization::Options opts = mkl_like_options();
    opts.pivot_perturb_exp = 10;
    kkt.reconfigure(opts);

    // The options are baked into the session the symbolic lives in, so the new
    // configuration starts without one.
    EXPECT_THROW(kkt.refactorize(), std::runtime_error);
    EXPECT_NO_THROW(kkt.compute());
    EXPECT_EQ(kkt.peigs(), 1);
    EXPECT_EQ(kkt.neigs(), 1);
}

// The thread count lives in the same backend session the symbolic analysis
// does, so changing it necessarily costs the analysis -- and NOT changing it
// must cost nothing, since the solver refreshes this on every solve entry.
TEST(KktFactorizationTest, SettingTheSameThreadCountKeepsTheAnalysis) {
    KktFactorization kkt(mkl_like_options());
    kkt.matrix() = kkt_upper_from_triplets(2, indefinite_2x2());
    kkt.compute();

    EXPECT_FALSE(kkt.set_num_threads(kkt.num_threads()));
    EXPECT_NO_THROW(kkt.refactorize());
    EXPECT_EQ(kkt.peigs(), 1);
}

TEST(KktFactorizationTest, ChangingTheThreadCountRebuildsAndReportsTheDroppedAnalysis) {
    KktFactorization kkt(mkl_like_options());
    kkt.matrix() = kkt_upper_from_triplets(2, indefinite_2x2());
    kkt.compute();

    EXPECT_TRUE(kkt.set_num_threads(2));
    EXPECT_EQ(kkt.num_threads(), 2);
    EXPECT_THROW(kkt.refactorize(), std::runtime_error);
    EXPECT_NO_THROW(kkt.compute());
}

// ---------------------------------------------------------------------------
// Settings the sparse linear surface cannot carry.
//
// Each of these had a real effect through the backend interface the engine
// used to own, and has no route through the surface that replaced it. The
// contract is that they are REJECTED where they are read, not accepted and
// quietly ignored -- a setting that silently stops working is worse than one
// that fails loudly, because the caller goes on believing it holds.
// ---------------------------------------------------------------------------

// A one-variable unconstrained problem, only ever transcribed -- these tests
// reject at configuration time and never reach a solve.
struct KktConfigRejectProblem : hven::solvers::NLPProblem {
    int num_vars() const override { return 1; }
    int num_cons() const override { return 0; }
    int num_jac_nonzeros() const override { return 0; }
    int num_hess_nonzeros() const override { return 1; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {
        xl << -std::numeric_limits<double>::infinity();
        xu << std::numeric_limits<double>::infinity();
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[0];
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * x[0];
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void jac_structure(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>) const override {}
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd>,
                  Eigen::Ref<Eigen::VectorXd>) const override {}
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd>,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = obj_factor * 2.0;
    }
    std::string name() const override { return "KktConfigRejectProblem"; }
};

// A thread count changed AFTER transcription must still reach the backend at
// the next solve. The factor holds the count in the same session the symbolic
// analysis lives in, so the refresh at solve entry drops that analysis -- and
// the solve has to notice, or the entry factorization asks to reuse a symbolic
// that is no longer there. The second solve below is the one that matters: the
// first leaves the analysis in place for it to invalidate.
TEST(KktFactorizationTest, AThreadCountChangedAfterTranscriptionStillSolves) {
    hven::solvers::NLPSolver solver(std::make_shared<KktConfigRejectProblem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0(1);
    x0 << 3.0;

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);

    solver.optimizer_->settings().qp_threads_ = solver.optimizer_->settings().qp_threads_ + 1;
    EXPECT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
}

#if defined(USE_ACCELERATE_SPARSE)

// Iterative refinement: the surface stores the cap on this backend and
// performs no refinement, where the engine's previous interface ran its own
// loop. A nonzero cap would be inert.
TEST(KktFactorizationTest, AccelerateRejectsANonzeroRefinementCap) {
    hven::solvers::NLPSolver solver(std::make_shared<KktConfigRejectProblem>());
    solver.optimizer_->settings().qp_ref_steps_ = 2;
    EXPECT_THROW(solver.optimizer_->set_nlp(solver.nlp_), std::invalid_argument);
}

// The pivot tolerance is fixed at the value the engine has always requested.
TEST(KktFactorizationTest, AccelerateRejectsANonDefaultPivotTolerance) {
    hven::solvers::NLPSolver solver(std::make_shared<KktConfigRejectProblem>());
    solver.optimizer_->settings().accel_pivot_tolerance_ = 0.05;
    EXPECT_THROW(solver.optimizer_->set_nlp(solver.nlp_), std::invalid_argument);
}

#else

// The surface calls the backend silently and exposes no message-level control.
TEST(KktFactorizationTest, MklRejectsBackendMessageOutput) {
    hven::solvers::NLPSolver solver(std::make_shared<KktConfigRejectProblem>());
    solver.optimizer_->settings().qp_print_ = true;
    EXPECT_THROW(solver.optimizer_->set_nlp(solver.nlp_), std::invalid_argument);
}

// Only the backend's own documented pivoting-strategy codes are expressible;
// the undocumented ones this enum also carries are not passed through as raw
// integers.
TEST(KktFactorizationTest, MklRejectsAnUndocumentedPivotingStrategyCode) {
    hven::solvers::NLPSolver solver(std::make_shared<KktConfigRejectProblem>());
    solver.optimizer_->settings().qp_pivot_strategy_ =
        hven::solvers::InteriorPointSolver::QPPivotModes::E13;
    EXPECT_THROW(solver.optimizer_->set_nlp(solver.nlp_), std::invalid_argument);
}

#endif

} // namespace
