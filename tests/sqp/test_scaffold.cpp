// Scaffold smoke test: proves the toolchain matches the tycho incorporation
// target — vendored Eigen + fmt compile, and the platform sparse backend
// (MKL Pardiso / Apple Accelerate) links and factorizes through Eigen's
// wrapper the same way tycho's solvers/linear interfaces do.

#include <gtest/gtest.h>

#include <Eigen/Dense>
#include <Eigen/SparseCore>
#include <fmt/format.h>

#ifdef USE_ACCELERATE_SPARSE
#include <Eigen/AccelerateSupport>
#else
#include <Eigen/PardisoSupport>
#endif

namespace {

// 1D Laplacian (tridiagonal SPD), the standard sparse test matrix.
Eigen::SparseMatrix<double> laplacian_1d(int n) {
    std::vector<Eigen::Triplet<double>> trips;
    trips.reserve(3 * n);
    for (int i = 0; i < n; ++i) {
        trips.emplace_back(i, i, 2.0);
        if (i > 0)
            trips.emplace_back(i, i - 1, -1.0);
        if (i < n - 1)
            trips.emplace_back(i, i + 1, -1.0);
    }
    Eigen::SparseMatrix<double> A(n, n);
    A.setFromTriplets(trips.begin(), trips.end());
    return A;
}

TEST(Scaffold, EigenDenseSolve) {
    Eigen::Matrix3d A;
    A << 4, 1, 0, 1, 3, 1, 0, 1, 2;
    Eigen::Vector3d b(1, 2, 3);
    Eigen::Vector3d x = A.ldlt().solve(b);
    EXPECT_LT((A * x - b).norm(), 1e-12);
}

TEST(Scaffold, SparseBackendSolve) {
    const int n = 50;
    Eigen::SparseMatrix<double> A = laplacian_1d(n);
    Eigen::VectorXd b = Eigen::VectorXd::Ones(n);

#ifdef USE_ACCELERATE_SPARSE
    Eigen::AccelerateLDLT<Eigen::SparseMatrix<double>> solver;
#else
    Eigen::PardisoLDLT<Eigen::SparseMatrix<double>> solver;
#endif
    solver.compute(A);
    ASSERT_EQ(solver.info(), Eigen::Success)
        << fmt::format("sparse factorization failed for n = {}", n);
    Eigen::VectorXd x = solver.solve(b);
    EXPECT_LT((A * x - b).norm(), 1e-9 * b.norm());
}

TEST(Scaffold, FmtFormats) { EXPECT_EQ(fmt::format("{:.3f}", 1.5), "1.500"); }

} // namespace
