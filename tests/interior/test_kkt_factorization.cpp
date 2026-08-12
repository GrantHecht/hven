// The engine-side KKT factorization: lifecycle, and the evidence projection
// the inertia machinery reads. The projection is the interesting half -- it is
// where the linear layer's honest, optional evidence becomes the plain
// integers the engine consumes -- so each row of it is checked against a
// system whose inertia is known by hand.

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/interior/kkt_factorization.h"

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

} // namespace
