// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_equality_basis.cpp — the independent equality basis component (M7 W2 E1),
// unit level: u1 two networks, u2 duplicates / zero row / scaled duplicate and the
// coefficient-cap refusal, u3 a graded no-gap spectrum, u4 fixed-column elimination
// and the gap bands, u5 lift/fold against a dense minimum-norm oracle, u6 the rhs
// projection against the dense range projector, u7 determinism, plus verify().
// The oracle is dense (CompleteOrthogonalDecomposition / JacobiSVD) inside the test.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <random>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/drivers/equality_basis.h>

namespace hven::solvers::detail {
namespace {

using Arc = std::pair<Index, Index>;

SpMatRM to_sparse(const Eigen::MatrixXd &A) {
    SpMatRM S = A.sparseView();
    S.makeCompressed();
    return S;
}

// Node-arc incidence rows (+1 tail, -1 head) of a graph, columns offset by `col0`.
void add_network(Eigen::MatrixXd &A, Index row0, Index col0, const std::vector<Arc> &arcs) {
    for (std::size_t a = 0; a < arcs.size(); ++a) {
        A(row0 + arcs[a].first, col0 + static_cast<Index>(a)) = 1.0;
        A(row0 + arcs[a].second, col0 + static_cast<Index>(a)) = -1.0;
    }
}

// Two disjoint networks: 4 nodes / 5 arcs and 5 nodes / 6 arcs.
Eigen::MatrixXd two_networks() {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(9, 11);
    add_network(A, 0, 0, {{0, 1}, {1, 2}, {2, 3}, {0, 2}, {1, 3}});
    add_network(A, 4, 5, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {0, 4}, {1, 3}});
    return A;
}

Vec free_box(Index n) { return Vec::Constant(n, 1e20); }

// Dense minimum-norm lambda with A(:,F)^T lambda = g(F).
Vec dense_min_norm(const Eigen::MatrixXd &AF, const Vec &gF) {
    Eigen::CompleteOrthogonalDecomposition<Eigen::MatrixXd> cod(AF.transpose());
    return cod.solve(gF);
}

TEST(EqualityBasis, U1TwoNetworksDropOneRowPerComponent) {
    const Eigen::MatrixXd A = two_networks();
    const SpMatRM Je = to_sparse(A);
    EqualityBasis basis;
    basis.compute(Je, -free_box(11), free_box(11));
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kReduced);
    ASSERT_EQ(basis.dropped().size(), 2u);
    EXPECT_LT(basis.dropped()[0], 4);
    EXPECT_GE(basis.dropped()[1], 4);
    EXPECT_EQ(basis.retained().size(), 7u);
    EXPECT_LE(basis.max_backward_residual(), 1e-14);

    const auto &C = basis.coefficients();
    for (Index j = 0; j < C.cols(); ++j) {
        for (Eigen::SparseMatrix<double>::InnerIterator it(C, j); it; ++it) {
            const Index rk = basis.retained()[static_cast<std::size_t>(it.row())];
            const bool same_component =
                (rk < 4) == (basis.dropped()[static_cast<std::size_t>(j)] < 4);
            if (std::abs(it.value()) > 1e-12) {
                EXPECT_TRUE(same_component);
                EXPECT_NEAR(std::abs(it.value()), 1.0, 1e-12);
            }
        }
    }
    // Je(:,F)^T N = 0: each dropped row is reconstructed exactly by C.
    for (std::size_t j = 0; j < basis.dropped().size(); ++j) {
        Vec rec = A.row(basis.dropped()[j]).transpose();
        for (Eigen::SparseMatrix<double>::InnerIterator it(C, static_cast<Index>(j)); it; ++it) {
            rec -= it.value() *
                   A.row(basis.retained()[static_cast<std::size_t>(it.row())]).transpose();
        }
        EXPECT_LE(rec.lpNorm<Eigen::Infinity>(), 1e-14);
    }
    EXPECT_TRUE(basis.verify(Je));
}

TEST(EqualityBasis, U2DuplicateZeroAndScaledDuplicateAreDropped) {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(6, 6);
    A.row(0) << 1.0, 2.0, 0.0, 0.0, -1.0, 0.0;
    A.row(1) << 0.0, 1.0, 3.0, 0.0, 0.0, 1.0;
    A.row(2) << 0.5, 0.0, 0.0, 4.0, 0.0, -2.0;
    A.row(3) = A.row(0);        // duplicate
    A.row(4).setZero();         // zero row
    A.row(5) = 1e10 * A.row(1); // scaled duplicate
    const SpMatRM Je = to_sparse(A);
    EqualityBasis basis;
    basis.compute(Je, -free_box(6), free_box(6));
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kReduced);
    ASSERT_EQ(basis.dropped().size(), 3u);
    EXPECT_NE(std::find(basis.dropped().begin(), basis.dropped().end(), 4), basis.dropped().end());
    // Equilibrated coefficients: the 1e10 duplicate reconstructs with |C~| = 1.
    EXPECT_NEAR(basis.max_coefficient(), 1.0, 1e-12);
    EXPECT_TRUE(basis.verify(Je));
}

TEST(EqualityBasis, U2CoefficientCapRefusesTheSplit) {
    // a2 = a0 + a1 with a0 ⟂ a1: every basis choice needs |C~| >= 1/sqrt(2).
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(3, 3);
    A.row(0) << 1.0, 0.0, 0.0;
    A.row(1) << 0.0, 1.0, 0.0;
    A.row(2) << 1.0, 1.0, 0.0;
    const SpMatRM Je = to_sparse(A);
    EqualityBasis accepted;
    accepted.compute(Je, -free_box(3), free_box(3));
    EXPECT_EQ(accepted.verdict(), EqualityBasisVerdict::kReduced);

    EqualityBasisTolerances tol;
    tol.coef_cap = 0.5;
    EqualityBasis refused;
    refused.compute(Je, -free_box(3), free_box(3), tol);
    EXPECT_EQ(refused.verdict(), EqualityBasisVerdict::kUndecidable);
    EXPECT_FALSE(refused.reduces());
    EXPECT_EQ(refused.retained().size(), 3u);
}

TEST(EqualityBasis, U3GradedNoGapSpectrumIsUndecidable) {
    const Index me = 30;
    const Index n = 40;
    std::mt19937 rng(20260922u);
    std::normal_distribution<double> nd(0.0, 1.0);
    Eigen::MatrixXd G1(me, me), G2(n, n);
    for (Index i = 0; i < me; ++i) {
        for (Index j = 0; j < me; ++j) {
            G1(i, j) = nd(rng);
        }
    }
    for (Index i = 0; i < n; ++i) {
        for (Index j = 0; j < n; ++j) {
            G2(i, j) = nd(rng);
        }
    }
    const Eigen::MatrixXd U = Eigen::HouseholderQR<Eigen::MatrixXd>(G1).householderQ();
    const Eigen::MatrixXd V = Eigen::HouseholderQR<Eigen::MatrixXd>(G2).householderQ();
    Eigen::MatrixXd S = Eigen::MatrixXd::Zero(me, n);
    for (Index k = 0; k < me; ++k) {
        S(k, k) = std::pow(10.0, -12.0 * static_cast<double>(k) / static_cast<double>(me - 1));
    }
    Eigen::MatrixXd A = U * S * V.transpose();
    for (Index r = 0; r < me; ++r) {
        const double lo = std::log10(1.4);
        const double hi = std::log10(6e10);
        A.row(r) *=
            std::pow(10.0, lo + (hi - lo) * static_cast<double>(r) / static_cast<double>(me - 1));
    }
    const SpMatRM Je = to_sparse(A);
    EqualityBasis basis;
    basis.compute(Je, -free_box(n), free_box(n));
    EXPECT_EQ(basis.verdict(), EqualityBasisVerdict::kUndecidable);
    EXPECT_FALSE(basis.reduces());
    EXPECT_EQ(basis.retained().size(), static_cast<std::size_t>(me));

    // reduce() leaves the problem byte-identical.
    QpProblem qp;
    qp.H = SpMatRM(n, n);
    qp.g = Vec::Ones(n);
    qp.Ae = Je;
    qp.be = Vec::LinSpaced(me, -1.0, 1.0);
    qp.Ai = SpMatRM(0, n);
    qp.bi = Vec(0);
    qp.lower = -free_box(n);
    qp.upper = free_box(n);
    const QpProblem before = qp;
    basis.reduce(qp);
    ASSERT_EQ(qp.Ae.nonZeros(), before.Ae.nonZeros());
    EXPECT_EQ(std::memcmp(qp.Ae.valuePtr(), before.Ae.valuePtr(),
                          sizeof(double) * static_cast<std::size_t>(qp.Ae.nonZeros())),
              0);
    EXPECT_EQ(
        std::memcmp(qp.be.data(), before.be.data(), sizeof(double) * static_cast<std::size_t>(me)),
        0);
}

// A graph whose two fixed bridge arcs (3 and 5) cut it into three free pieces:
// {0,1,2} triangle, bridge (2,3), {3,4}, bridge (4,5), {5,6}.
struct FixedArcNetwork {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(7, 7);
    Vec lower = Vec::Constant(7, -10.0);
    Vec upper = Vec::Constant(7, 10.0);
    FixedArcNetwork() {
        add_network(A, 0, 0, {{0, 1}, {1, 2}, {0, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}});
        lower(3) = upper(3) = 0.7;
        lower(5) = upper(5) = -0.4;
    }
};

TEST(EqualityBasis, U4FixedColumnsSplitTheNetworkIntoThreePieces) {
    const FixedArcNetwork net;
    const SpMatRM Je = to_sparse(net.A);
    EqualityBasis free_basis;
    free_basis.compute(Je, Vec::Constant(7, -10.0), Vec::Constant(7, 10.0));
    EXPECT_EQ(free_basis.dropped().size(), 1u);

    EqualityBasis basis;
    basis.compute(Je, net.lower, net.upper);
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kReduced);
    ASSERT_EQ(basis.dropped().size(), 3u);

    // Supplies balanced per piece with the fixed flows at their values.
    Vec x = Vec::Zero(7);
    x << 0.3, -0.2, 0.5, 0.7, 0.1, -0.4, 0.25;
    const Vec supply = net.A * x;
    Vec ce = net.A * x - supply;
    Vec gap;
    std::vector<EqualityGapBand> band;
    basis.read_gaps(ce, Je, x, 1e-6, gap, band);
    for (std::size_t j = 0; j < band.size(); ++j) {
        EXPECT_EQ(band[j], EqualityGapBand::kNoise) << j;
    }
    // The gap does not depend on where x sits on the fixed columns.
    Vec x_off = x;
    x_off(3) = 2.0;
    x_off(5) = 1.5;
    const Vec ce_off = net.A * x_off - supply;
    Vec gap_off;
    basis.read_gaps(ce_off, Je, x_off, 1e-6, gap_off, band);
    EXPECT_LE((gap_off - gap).lpNorm<Eigen::Infinity>(), 1e-14);
    // Roundoff of the fixed-value shift is not "noise" relative to |w| ~ 0, but it
    // is feasible: the noise band is relative to the residuals, not their inputs.
    for (std::size_t j = 0; j < band.size(); ++j) {
        EXPECT_NE(band[j], EqualityGapBand::kInconsistent) << j;
    }
    // A supply error of 1e-3 on node 5 is inconsistent; 5e-9 is feasible.
    for (const double err : {1e-3, 5e-9}) {
        Vec s = supply;
        s(5) += err;
        const Vec ce_e = net.A * x - s;
        basis.read_gaps(ce_e, Je, x, 1e-6, gap, band);
        Index flagged = 0;
        for (std::size_t j = 0; j < band.size(); ++j) {
            if (band[j] != EqualityGapBand::kNoise) {
                ++flagged;
                EXPECT_EQ(band[j],
                          err > 1e-6 ? EqualityGapBand::kInconsistent : EqualityGapBand::kFeasible);
                EXPECT_NEAR(std::abs(gap(static_cast<Index>(j))), err, 1e-14);
            }
        }
        EXPECT_EQ(flagged, 1);
    }
}

TEST(EqualityBasis, U5LiftIsTheMinimumNormMultiplierAndFoldInvertsIt) {
    const Eigen::MatrixXd A = two_networks();
    const SpMatRM Je = to_sparse(A);
    EqualityBasis basis;
    basis.compute(Je, -free_box(11), free_box(11));
    ASSERT_TRUE(basis.reduces());
    const Index nb = static_cast<Index>(basis.retained().size());
    Eigen::MatrixXd AB(nb, 11);
    for (Index k = 0; k < nb; ++k) {
        AB.row(k) = A.row(basis.retained()[static_cast<std::size_t>(k)]);
    }
    const Vec lambda_B = Vec::LinSpaced(nb, -2.0, 3.5);
    const Vec lam = basis.lift(lambda_B);
    ASSERT_EQ(lam.size(), 9);
    EXPECT_LE((A.transpose() * lam - AB.transpose() * lambda_B).lpNorm<Eigen::Infinity>(), 1e-14);
    const Vec oracle = dense_min_norm(A, AB.transpose() * lambda_B);
    EXPECT_LE((lam - oracle).lpNorm<Eigen::Infinity>(), 1e-12);
    EXPECT_LE((basis.fold(lam) - lambda_B).lpNorm<Eigen::Infinity>(), 1e-14);

    // Permutation equivariance: lift(fold(.)) is the minimum-norm representative.
    const Vec lam_any = Vec::LinSpaced(9, 1.0, -4.0);
    const Vec rep = basis.lift(basis.fold(lam_any));
    EXPECT_LE((rep - dense_min_norm(A, A.transpose() * lam_any)).lpNorm<Eigen::Infinity>(), 1e-12);
    std::vector<Index> perm(9);
    std::iota(perm.begin(), perm.end(), 0);
    std::reverse(perm.begin(), perm.end());
    std::swap(perm[2], perm[6]);
    Eigen::MatrixXd PA(9, 11);
    Vec p_lam(9);
    for (Index i = 0; i < 9; ++i) {
        PA.row(i) = A.row(perm[static_cast<std::size_t>(i)]);
        p_lam(i) = lam_any(perm[static_cast<std::size_t>(i)]);
    }
    EqualityBasis pbasis;
    pbasis.compute(to_sparse(PA), -free_box(11), free_box(11));
    const Vec p_rep = pbasis.lift(pbasis.fold(p_lam));
    for (Index i = 0; i < 9; ++i) {
        EXPECT_NEAR(p_rep(i), rep(perm[static_cast<std::size_t>(i)]), 1e-12);
    }
}

TEST(EqualityBasis, U6ProjectedRhsIsTheMinimumNormConsistentCorrection) {
    const Eigen::MatrixXd A = two_networks();
    const SpMatRM Je = to_sparse(A);
    EqualityBasis basis;
    basis.compute(Je, -free_box(11), free_box(11));
    ASSERT_TRUE(basis.reduces());
    QpProblem qp;
    qp.H = SpMatRM(11, 11);
    qp.g = Vec::Zero(11);
    qp.Ae = Je;
    qp.be = Vec::LinSpaced(9, -1.0, 2.0); // inconsistent on both components
    qp.Ai = SpMatRM(0, 11);
    qp.bi = Vec(0);
    qp.lower = -free_box(11);
    qp.upper = free_box(11);
    const Vec be_full = qp.be;
    basis.reduce(qp);
    ASSERT_EQ(qp.me(), 7);
    // Implied full correction delta: t on B, and on D the residual the retained
    // system forces on each dropped row.
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Vec d =
        Eigen::MatrixXd(qp.Ae).jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(qp.be);
    EXPECT_LE((Eigen::MatrixXd(qp.Ae) * d - qp.be).lpNorm<Eigen::Infinity>(), 1e-13);
    const Vec delta = A * d - be_full;
    // Oracle: delta = -(I - A A^+) be.
    const Vec proj = A * svd.solve(be_full);
    const Vec delta_oracle = proj - be_full;
    EXPECT_LE((delta - delta_oracle).lpNorm<Eigen::Infinity>(), 1e-12);
}

TEST(EqualityBasis, U7DeterministicAndIdentityWhenFullRank) {
    const Eigen::MatrixXd A = two_networks();
    const SpMatRM Je = to_sparse(A);
    EqualityBasis a, b;
    a.compute(Je, -free_box(11), free_box(11));
    b.compute(Je, -free_box(11), free_box(11));
    EXPECT_EQ(a.retained(), b.retained());
    EXPECT_EQ(a.dropped(), b.dropped());
    ASSERT_EQ(a.coefficients().nonZeros(), b.coefficients().nonZeros());
    EXPECT_EQ(std::memcmp(a.coefficients().valuePtr(), b.coefficients().valuePtr(),
                          sizeof(double) * static_cast<std::size_t>(a.coefficients().nonZeros())),
              0);
    EXPECT_EQ(a.jacobian_hash(), b.jacobian_hash());

    // Full rank: kFullRank, and every operation is the identity.
    Eigen::MatrixXd F = A.topRows(3);
    EqualityBasis full;
    full.compute(to_sparse(F), -free_box(11), free_box(11));
    EXPECT_EQ(full.verdict(), EqualityBasisVerdict::kFullRank);
    EXPECT_FALSE(full.reduces());
    const Vec y(Vec::LinSpaced(3, 1.0, 2.0));
    EXPECT_EQ(full.lift(y), y);
    EXPECT_EQ(full.fold(y), y);
}

TEST(EqualityBasis, VerifyTracksTheDependencyAcrossValueChanges) {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(2, 3);
    A.row(0) << 1.0, -1.0, 0.0;
    A.row(1) = 2.0 * A.row(0);
    EqualityBasis basis;
    basis.compute(to_sparse(A), -free_box(3), free_box(3));
    ASSERT_EQ(basis.dropped().size(), 1u);
    EXPECT_TRUE(basis.verify(to_sparse(A)));
    Eigen::MatrixXd B = A;
    B.row(1) = 3.0 * A.row(0); // still dependent, different coefficient
    EXPECT_FALSE(basis.verify(to_sparse(B)));
    Eigen::MatrixXd Cm = A;
    Cm(1, 2) = 1e-3; // independent
    EXPECT_FALSE(basis.verify(to_sparse(Cm)));
    EXPECT_NE(EqualityBasis::hash_jacobian(to_sparse(A)),
              EqualityBasis::hash_jacobian(to_sparse(B)));
    EXPECT_EQ(basis.group_hash(to_sparse(A), 0), basis.group_hash(to_sparse(A), 0));
    EXPECT_NE(basis.group_hash(to_sparse(A), 0), basis.group_hash(to_sparse(B), 0));
}

} // namespace
} // namespace hven::solvers::detail
