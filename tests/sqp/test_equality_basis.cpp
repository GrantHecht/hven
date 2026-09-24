// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_equality_basis.cpp — the independent equality basis component (M7 W2 E1),
// unit level: u1 two networks, u2 duplicates / zero row / scaled duplicate and the
// coefficient-cap refusal, u3 a graded no-gap spectrum, u4 fixed-column elimination
// and the gap bands, u5 lift/fold against a dense minimum-norm oracle, u6 the rhs
// projection against the dense range projector, u7 determinism, plus verify(); fix
// round 1: extreme scaling and non-finite entries, a Gram-hidden deficiency, the
// I + C^T C solve check, and byte-identical groups; E1b (the production oracle
// through hven::linear): t1 the adversarial Gram set, t2 the 1e10-scaled duplicate,
// t3 the certificate's fast path and its fail-closed arm, t4 the near-cap lift, t8 the
// numerical-rank contract (fix round 1). The oracle is dense
// (CompleteOrthogonalDecomposition / JacobiSVD) inside the test.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
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

// lift() whose solve check must pass.
Vec lifted(const EqualityBasis &basis, const Vec &lambda_B) {
    Vec out;
    EXPECT_TRUE(basis.lift(lambda_B, out));
    return out;
}

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
    // The certificate's fast path cannot settle a redundant balance row.
    EXPECT_GT(basis.deficiency_lo(), 0); // the reducing path ran
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
    // E1b (T3): a SAWPATH-like graded spectrum has eigenvalues on both sides of the
    // shifts' band, so the counts at t_hi and t_lo differ: no gap, never dependent.
    EXPECT_EQ(basis.refusal(), EqualityBasisRefusal::kSpectrum);
    EXPECT_GT(basis.deficiency_hi(), basis.deficiency_lo());
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
    ASSERT_TRUE(basis.reduce(qp));
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
    EXPECT_GT(basis.deficiency_lo(), 0); // the reducing path ran
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
    const Vec lam = lifted(basis, lambda_B);
    ASSERT_EQ(lam.size(), 9);
    EXPECT_LE((A.transpose() * lam - AB.transpose() * lambda_B).lpNorm<Eigen::Infinity>(), 1e-14);
    const Vec oracle = dense_min_norm(A, AB.transpose() * lambda_B);
    EXPECT_LE((lam - oracle).lpNorm<Eigen::Infinity>(), 1e-12);
    EXPECT_LE((basis.fold(lam) - lambda_B).lpNorm<Eigen::Infinity>(), 1e-14);

    // Permutation equivariance: lift(fold(.)) is the minimum-norm representative.
    const Vec lam_any = Vec::LinSpaced(9, 1.0, -4.0);
    const Vec rep = lifted(basis, basis.fold(lam_any));
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
    const Vec p_rep = lifted(pbasis, pbasis.fold(p_lam));
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
    ASSERT_TRUE(basis.reduce(qp));
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
    EXPECT_LE(full.deficiency_lo(), 0); // the reducing path did not run
    EXPECT_FALSE(full.reduces());
    const Vec y(Vec::LinSpaced(3, 1.0, 2.0));
    EXPECT_EQ(lifted(full, y), y);
    EXPECT_EQ(full.fold(y), y);
    QpProblem qp;
    qp.H = SpMatRM(11, 11);
    qp.g = Vec::Zero(11);
    qp.Ae = to_sparse(F);
    qp.be = y;
    qp.Ai = SpMatRM(0, 11);
    qp.bi = Vec(0);
    qp.lower = -free_box(11);
    qp.upper = free_box(11);
    const SpMatRM Ae_before = qp.Ae;
    ASSERT_TRUE(full.reduce(qp));
    ASSERT_EQ(qp.Ae.nonZeros(), Ae_before.nonZeros());
    EXPECT_EQ(std::memcmp(qp.Ae.valuePtr(), Ae_before.valuePtr(),
                          sizeof(double) * static_cast<std::size_t>(qp.Ae.nonZeros())),
              0);
    EXPECT_EQ(std::memcmp(qp.be.data(), y.data(), sizeof(double) * 3), 0);
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

// A banded collocation-shaped Jacobian: a state recurrence per interval plus the
// initial conditions (full rank).
SpMatRM collocation_chain(Index kIntervals = 400) {
    constexpr Index kStates = 3;
    constexpr Index kControls = 2;
    constexpr Index kPerNode = kStates + kControls;
    const Index n = (kIntervals + 1) * kPerNode;
    const Index me = (kIntervals + 1) * kStates;
    const double h = 1.0 / static_cast<double>(kIntervals);
    std::vector<Eigen::Triplet<double>> trip;
    for (Index s = 0; s < kStates; ++s) {
        trip.emplace_back(static_cast<int>(s), static_cast<int>(s), 1.0);
    }
    for (Index k = 0; k < kIntervals; ++k) {
        for (Index s = 0; s < kStates; ++s) {
            const auto row = static_cast<int>(kStates + k * kStates + s);
            const Index here = k * kPerNode;
            const Index next = (k + 1) * kPerNode;
            trip.emplace_back(row, static_cast<int>(next + s), 1.0);
            trip.emplace_back(row, static_cast<int>(here + s), -1.0);
            trip.emplace_back(row, static_cast<int>(here + (s + 1) % kStates), -h);
            trip.emplace_back(row, static_cast<int>(here + kStates + s % kControls), -h);
        }
    }
    SpMatRM Je(me, n);
    Je.setFromTriplets(trip.begin(), trip.end());
    Je.makeCompressed();
    return Je;
}

// T3 (E1b): the certificate's fast path and its fail-closed direction. A full-rank
// network and a banded collocation Jacobian are certified in ONE Gram factorization
// (n_neg(G - t_hi I) = 0 on clean evidence); a network with a redundant balance row
// takes the reducing path (t_hi, t_lo, the masked retained set at t_hi and at 0).
TEST(EqualityBasis, T3CertificateFastPathIsOneFactorization) {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(4, 5);
    add_network(A, 0, 0, {{0, 1}, {1, 2}, {2, 3}, {0, 2}, {1, 3}});
    EqualityBasis net;
    net.compute(to_sparse(A), -free_box(5), free_box(5));
    ASSERT_EQ(net.verdict(), EqualityBasisVerdict::kReduced);
    EXPECT_EQ(net.deficiency_hi(), 1);
    EXPECT_EQ(net.deficiency_lo(), 1);
    EXPECT_EQ(net.gram_factorizations(), 4);
    EXPECT_GT(net.deficiency_lo(), 0); // the reducing path ran
    ASSERT_EQ(net.dropped().size(), 1u);
    // The tie rule: nodes 1 and 2 have the largest degree (3); the higher index wins.
    EXPECT_EQ(net.dropped()[0], 2);

    const Eigen::MatrixXd B = A.topRows(3);
    EqualityBasis trimmed;
    trimmed.compute(to_sparse(B), -free_box(5), free_box(5));
    EXPECT_EQ(trimmed.verdict(), EqualityBasisVerdict::kFullRank);
    EXPECT_EQ(trimmed.gram_factorizations(), 1);
    EXPECT_EQ(trimmed.deficiency_hi(), 0);
    EXPECT_EQ(trimmed.deficiency_lo(), -1);
    EXPECT_LE(trimmed.deficiency_lo(), 0); // the reducing path did not run

    const SpMatRM Je = collocation_chain();
    EqualityBasis chain;
    chain.compute(Je, -free_box(Je.cols()), free_box(Je.cols()));
    EXPECT_EQ(chain.verdict(), EqualityBasisVerdict::kFullRank);
    EXPECT_EQ(chain.gram_factorizations(), 1);
    EXPECT_LE(chain.deficiency_lo(), 0); // the reducing path did not run
    EXPECT_TRUE(chain.dropped().empty());
    EXPECT_EQ(chain.min_retained_pivot(), std::sqrt(chain.shift_hi()));
    EXPECT_EQ(chain.shift_hi(), 1e4 * chain.shift_lo());

    // A 2000-interval chain is full rank with eigenvalues of G in [t_lo, t_hi): n_neg
    // is 3 at t_hi and 0 at t_lo, which certifies full rank with the margin t_lo.
    const SpMatRM Jl = collocation_chain(2000);
    EqualityBasis longc;
    longc.compute(Jl, -free_box(Jl.cols()), free_box(Jl.cols()));
    EXPECT_EQ(longc.verdict(), EqualityBasisVerdict::kFullRank);
    EXPECT_GT(longc.deficiency_hi(), 0);
    EXPECT_EQ(longc.deficiency_lo(), 0);
    EXPECT_EQ(longc.gram_factorizations(), 2);
    EXPECT_LE(longc.deficiency_lo(), 0); // the reducing path did not run
    EXPECT_EQ(longc.min_retained_pivot(), std::sqrt(longc.shift_lo()));
}

// T3 (E1b), the fail-closed arm: a perturbed pivot is never evidence. A perturbation
// exponent of 1 (a threshold of ~1e-1 ||A||) makes Pardiso perturb pivots of the
// certificate factorization, and the split refuses with kCertificate. MKL only:
// Accelerate's zeroTolerance at this exponent is 1e-1 eps and perturbs nothing
// (its fail-closed input is n_zero, which this fixture cannot provoke).
#if !defined(__APPLE__)
TEST(EqualityBasis, T3PerturbedPivotRefusesTheCertificate) {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(4, 5);
    add_network(A, 0, 0, {{0, 1}, {1, 2}, {2, 3}, {0, 2}, {1, 3}});
    EqualityBasisTolerances tol;
    tol.gram_pivot_perturb_exp = 1;
    EqualityBasis refused;
    refused.compute(to_sparse(A), -free_box(5), free_box(5), tol);
    EXPECT_EQ(refused.verdict(), EqualityBasisVerdict::kUndecidable);
    EXPECT_EQ(refused.refusal(), EqualityBasisRefusal::kCertificate);
    EXPECT_FALSE(refused.reduces());
    EXPECT_EQ(refused.retained().size(), 4u);
}
#endif

// T1 (E1b), the adversarial Gram set (fix round 1, I1): rows e1, e1 + d e2, e2 with
// d = 5e-4 are dependent (a1 = a0 + d a2 exactly), yet a Cholesky of the formed Gram
// has every pivot above 1e-5. The eigenvalues of the equilibrated Gram are about
// {0, 1, 2}, so the shifted inertia counts k = 1 at both shifts whatever the pivot
// order: DEPENDENT. The null vector is ~(1, -sqrt(1 + d^2), d) in equilibrated units;
// rows 0 and 1 tie within 1e-6 and the higher index (1) is dropped with C = (1, d).
TEST(EqualityBasis, T1AdversarialGramSetIsDependentUnderTheCertificate) {
    constexpr double kDelta = 5e-4;
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(3, 3);
    A(0, 0) = 1.0;
    A(1, 0) = 1.0;
    A(1, 1) = kDelta;
    A(2, 1) = 1.0;
    EqualityBasis basis;
    basis.compute(to_sparse(A), -free_box(3), free_box(3));
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kReduced);
    EXPECT_EQ(basis.deficiency_hi(), 1);
    EXPECT_EQ(basis.deficiency_lo(), 1);
    EXPECT_EQ(basis.gram_factorizations(), 4);
    ASSERT_EQ(basis.dropped(), std::vector<Index>{1});
    ASSERT_EQ(basis.retained(), (std::vector<Index>{0, 2}));
    const Eigen::MatrixXd C(basis.coefficients());
    EXPECT_NEAR(C(0, 0), 1.0, 1e-14);
    EXPECT_NEAR(C(1, 0), kDelta, 1e-17);
    EXPECT_TRUE(basis.verify(to_sparse(A)));
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(A);
    EXPECT_LT(svd.singularValues()(2), 1e-12);
}

// T2 (E1b): a duplicate scaled by 1e10 is exact. Equilibrated, rows 1 and 3 are the
// same unit vector: k = 1, they tie, row 3 is dropped with C~ = 1 and C = 1e10.
TEST(EqualityBasis, T2ScaledDuplicateIsExact) {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(4, 6);
    A.row(0) << 1.0, 2.0, 0.0, 0.0, -1.0, 0.0;
    A.row(1) << 0.0, 1.0, 3.0, 0.0, 0.0, 1.0;
    A.row(2) << 0.5, 0.0, 0.0, 4.0, 0.0, -2.0;
    A.row(3) = 1e10 * A.row(1);
    const SpMatRM Je = to_sparse(A);
    EqualityBasis basis;
    basis.compute(Je, -free_box(6), free_box(6));
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kReduced);
    EXPECT_EQ(basis.deficiency_hi(), 1);
    EXPECT_EQ(basis.deficiency_lo(), 1);
    ASSERT_EQ(basis.dropped(), std::vector<Index>{3});
    constexpr double kU = std::numeric_limits<double>::epsilon() / 2.0;
    EXPECT_LE(std::abs(basis.max_coefficient() - 1.0), 4.0 * kU);
    const Eigen::MatrixXd C(basis.coefficients());
    EXPECT_LE(std::abs(C(1, 0) / 1e10 - 1.0), 4.0 * kU);
    EXPECT_LE(std::abs(C(0, 0)), 1e-6);
    EXPECT_LE(std::abs(C(2, 0)), 1e-6);
    EXPECT_TRUE(basis.verify(Je));
    const Vec y = Vec::LinSpaced(3, 1.0, -2.0);
    Vec lam;
    double lift_bound = -1.0;
    ASSERT_TRUE(basis.lift(y, lam, &lift_bound));
    EXPECT_LE(lift_bound, 1e-14 * y.norm());
    EXPECT_LE((basis.fold(lam) - y).lpNorm<Eigen::Infinity>(), 1e-14 * 1e10);
    Vec t;
    double proj_bound = -1.0;
    EXPECT_TRUE(basis.project_rhs(Vec::Constant(1, 0.5), t, &proj_bound));
    EXPECT_LE(proj_bound, 1e-14 * 0.5);
    EXPECT_TRUE(t.allFinite());
}

// T4 (E1b): a multi-column lift with C = 2^26 [u1, u1 + 2^-26 u2], nearly parallel
// columns in exactly representable powers of two. Rows b1 = e1, b2 = e2,
// d1 = 2^26 e1, d2 = 2^26 e1 + e2: the tie rule drops rows 3 then 2 and the
// coefficients are exact, C = [2^26 2^26; 0 1]. I + C^T C has eigenvalues ~2^53 and
// ~1.5 (condition ~6e15); K's is sqrt(1 + ||C||^2) ~ 1e8. z = (1, -2^26) gives
// w* = C^T z = (2^26, 0) and y = z + C w* = (1 + 2^52, -2^26), all exact.
TEST(EqualityBasis, T4NearCapMultiColumnLiftMeetsItsForwardEstimate) {
    constexpr double k26 = 67108864.0; // 2^26
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(4, 2);
    A(0, 0) = 1.0;
    A(1, 1) = 1.0;
    A(2, 0) = k26;
    A(3, 0) = k26;
    A(3, 1) = 1.0;
    const SpMatRM Je = to_sparse(A);
    EqualityBasis basis;
    basis.compute(Je, -free_box(2), free_box(2));
    ASSERT_EQ(basis.verdict(), EqualityBasisVerdict::kReduced);
    ASSERT_EQ(basis.dropped(), (std::vector<Index>{2, 3}));
    const Eigen::MatrixXd C(basis.coefficients());
    ASSERT_EQ(C.rows(), 2);
    ASSERT_EQ(C.cols(), 2);
    EXPECT_EQ(C(0, 0), k26);
    EXPECT_EQ(C(1, 0), 0.0);
    EXPECT_EQ(C(0, 1), k26);
    EXPECT_EQ(C(1, 1), 1.0);

    const Vec z = (Vec(2) << 1.0, -k26).finished();
    const Vec w_star = (Vec(2) << k26, 0.0).finished();
    const Vec y = (Vec(2) << 1.0 + k26 * k26, -k26).finished();
    Vec lam;
    double bound = -1.0;
    ASSERT_TRUE(basis.lift(y, lam, &bound));
    ASSERT_EQ(lam.size(), 4);
    // The reported inflated a-posteriori estimate covers w's error here and is within
    // the acceptance.
    const Vec w = (Vec(2) << lam(2), lam(3)).finished();
    EXPECT_LE((w - w_star).norm(), bound);
    EXPECT_LE(bound, 1e-8 * y.norm());
    // lambda_B = y - C w: the fold is y to the rounding of that one product.
    constexpr double kU = std::numeric_limits<double>::epsilon() / 2.0;
    const Vec lamB = (Vec(2) << lam(0), lam(1)).finished();
    EXPECT_LE((basis.fold(lam) - y).norm(), 8.0 * kU * (y.norm() + C.cwiseAbs().norm() * w.norm()));
    // Optimality of the representative: lambda_D = C^T lambda_B to roundoff scaled by ||C||.
    EXPECT_LE((w - C.transpose() * lamB).norm(), 64.0 * kU * C.norm() * y.norm());
    // And lambda_B is z to the estimate's image through C.
    EXPECT_LE((lamB - z).norm(), C.norm() * bound + 8.0 * kU * y.norm());

    // The projection through the same K, [0; -gamma]: here ||b|| = ||gamma|| does not
    // grow with C while |K||x^| ~ |C||w| does, so the rounding term alone,
    // gamma_{w+1} ||C|| ||w|| ~ 4e-8, exceeds 1e-8 ||gamma||. The projection REFUSES
    // (the pending-refusal transition in the driver) and its reported inflated
    // a-posteriori estimate still covers the error: t = C w^ is within ||C|| times it of
    // the closed form.
    const Vec gamma = (Vec(2) << 1.0, -1.0).finished();
    Vec t;
    double pbound = -1.0;
    EXPECT_FALSE(basis.project_rhs(gamma, t, &pbound));
    EXPECT_GT(pbound, 1e-8 * gamma.norm());
    EXPECT_LE(pbound, 1e-7 * gamma.norm());
    // Closed form: det(I + C^T C) = 3 2^52 + 2 (exact), w = (2^53 + 2, -(2^53 + 1)) / det,
    // t = C w = (2^26 / det, -(2^53 + 1) / det).
    const double det = 3.0 * k26 * k26 + 2.0;
    const Vec t_ref = (Vec(2) << k26 / det, -(2.0 * k26 * k26) / det - 1.0 / det).finished();
    EXPECT_LE((t - t_ref).norm(), C.norm() * pbound + 16.0 * kU * C.norm() * t.norm());
}

// T8 (E1b fix round 1, settler F1): THE NUMERICAL-RANK CONTRACT on the reviewer's pair,
// rows (1, 0) and (1, delta), full row rank for every delta > 0. On unit-norm rows
// lambda_min(G) = 1 - 1/sqrt(1 + delta^2) ~ delta^2 / 2, the retained-row residual is
// ~delta, and every delta below is built from the thresholds the code itself reports
// (tau_lo()/tau_hi() in sigma units, shift_lo()/shift_hi() in eigenvalue units), so the
// fixture follows the thresholds if they move:
//   delta = tau_lo / 10          DEPENDENT by contract (the contract's direction, not a
//                                defect): one row dropped;
//   delta = sqrt(tau_lo tau_hi)  and 10 tau_hi: the counts show k = 1 at both shifts but
//                                the residual exceeds tau_lo: UNDECIDABLE, nothing dropped;
//   delta with lambda_min = sqrt(t_lo t_hi): RETAINED on AMENDMENT 1 (n_neg 1 at t_hi,
//                                0 at t_lo; two factorizations);
//   delta = 10 sqrt(2 t_hi):     RETAINED on the fast path (one factorization).
TEST(EqualityBasis, T8NumericalRankContractOnTheTwoRowPair) {
    auto pair = [](double delta) {
        Eigen::MatrixXd A = Eigen::MatrixXd::Zero(2, 2);
        A(0, 0) = 1.0;
        A(1, 0) = 1.0;
        A(1, 1) = delta;
        return to_sparse(A);
    };
    EqualityBasis probe;
    probe.compute(pair(0.0), -free_box(2), free_box(2));
    ASSERT_EQ(probe.verdict(), EqualityBasisVerdict::kReduced);
    const double tau_lo = probe.tau_lo();
    const double tau_hi = probe.tau_hi();
    const double t_lo = probe.shift_lo();
    const double t_hi = probe.shift_hi();
    ASSERT_TRUE(tau_lo > 0.0 && tau_hi > tau_lo && t_lo > 0.0 && t_hi > t_lo);
    // The contract's formulas, pinned (fix round 2, settler G1): eps is the double
    // machine epsilon (2^-52 = 2u), m the nonempty rows, |F| the free columns, me the
    // equality rows and ||G||_inf the row Gram's infinity norm on the unit-norm rows,
    // all computed here from the fixture's own data. Each side evaluates the same
    // operations in the same order on exactly representable inputs, so the comparison
    // is exact.
    {
        const Eigen::MatrixXd A0 = Eigen::MatrixXd(pair(0.0));
        const double eps = std::numeric_limits<double>::epsilon();
        const auto me = static_cast<double>(A0.rows());
        const auto n_free = static_cast<double>(A0.cols()); // free_box: every column free
        double m = 0.0;
        Eigen::MatrixXd M = Eigen::MatrixXd::Zero(A0.rows(), A0.cols());
        for (Index r = 0; r < A0.rows(); ++r) {
            const double nr = A0.row(r).norm();
            if (nr > 0.0) {
                m += 1.0;
                M.row(r) = A0.row(r) / nr;
            }
        }
        const Eigen::MatrixXd G = M * M.transpose();
        const double g_inf = G.cwiseAbs().rowwise().sum().maxCoeff();
        ASSERT_EQ(m, 2.0);
        ASSERT_EQ(g_inf, 2.0);
        EXPECT_EQ(tau_lo, 20.0 * (n_free + me) * eps);
        EXPECT_EQ(tau_hi, 1e4 * (20.0 * (n_free + me) * eps));
        EXPECT_EQ(t_lo, std::max(20.0 * m * eps, 1e-10) * g_inf);
        EXPECT_EQ(t_hi, 1e4 * (std::max(20.0 * m * eps, 1e-10) * g_inf));
    }
    // The ordering the regimes rely on: the residual threshold sits far below the
    // shifts' sigma scale.
    ASSERT_LT(10.0 * tau_hi, std::sqrt(t_lo));

    {
        const SpMatRM J = pair(tau_lo / 10.0);
        EqualityBasis dep;
        dep.compute(J, -free_box(2), free_box(2));
        EXPECT_EQ(dep.verdict(), EqualityBasisVerdict::kReduced);
        EXPECT_EQ(dep.deficiency_hi(), 1);
        EXPECT_EQ(dep.deficiency_lo(), 1);
        EXPECT_EQ(dep.dropped(), std::vector<Index>{1});
        EXPECT_LE(dep.max_backward_residual(), tau_lo);
        EXPECT_TRUE(dep.verify(J));
    }
    for (const double delta : {std::sqrt(tau_lo * tau_hi), 10.0 * tau_hi}) {
        EqualityBasis und;
        und.compute(pair(delta), -free_box(2), free_box(2));
        EXPECT_EQ(und.verdict(), EqualityBasisVerdict::kUndecidable) << delta;
        EXPECT_EQ(und.refusal(), EqualityBasisRefusal::kSpectrum) << delta;
        EXPECT_EQ(und.deficiency_hi(), 1) << delta;
        EXPECT_EQ(und.deficiency_lo(), 1) << delta;
        EXPECT_FALSE(und.reduces()) << delta;
        EXPECT_EQ(und.retained(), (std::vector<Index>{0, 1})) << delta;
    }
    {
        EqualityBasis amend;
        amend.compute(pair(std::sqrt(2.0 * std::sqrt(t_lo * t_hi))), -free_box(2), free_box(2));
        EXPECT_EQ(amend.verdict(), EqualityBasisVerdict::kFullRank);
        EXPECT_EQ(amend.deficiency_hi(), 1);
        EXPECT_EQ(amend.deficiency_lo(), 0);
        EXPECT_EQ(amend.gram_factorizations(), 2);
        EXPECT_TRUE(amend.dropped().empty());
    }
    {
        EqualityBasis fast;
        fast.compute(pair(10.0 * std::sqrt(2.0 * t_hi)), -free_box(2), free_box(2));
        EXPECT_EQ(fast.verdict(), EqualityBasisVerdict::kFullRank);
        EXPECT_EQ(fast.deficiency_hi(), 0);
        EXPECT_EQ(fast.gram_factorizations(), 1);
        EXPECT_TRUE(fast.dropped().empty());
    }
}

// Extreme row scaling (fix round 1, C1): the row norms are scaled, so rows at
// 1e-160, 1e+160 and 1e-200 are independent and stay; a dependent pair at either
// extreme keeps exactly one row; a non-finite entry makes the split undecidable.
TEST(EqualityBasis, ExtremeScalingNeverDropsAnIndependentRow) {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(4, 5);
    A.row(0) << 1e-160, 2e-160, 0.0, 0.0, 0.0;
    A.row(1) << 0.0, 1e160, 3e160, 0.0, 0.0;
    A.row(2) << 0.0, 0.0, 1e-200, 1e-200, 0.0;
    A.row(3) << 1e160, 0.0, 0.0, 1e-160, 5e159;
    EqualityBasis basis;
    basis.compute(to_sparse(A), -free_box(5), free_box(5));
    EXPECT_EQ(basis.verdict(), EqualityBasisVerdict::kFullRank);
    EXPECT_EQ(basis.refusal(), EqualityBasisRefusal::kNone);
    EXPECT_TRUE(basis.dropped().empty());

    for (const double scale : {1e150, 1e-170}) {
        Eigen::MatrixXd P = Eigen::MatrixXd::Zero(3, 3);
        P.row(0) << scale, -scale, 0.0;
        P.row(1) = 2.0 * P.row(0);
        P.row(2) << 0.0, scale, scale;
        EqualityBasis pair;
        pair.compute(to_sparse(P), -free_box(3), free_box(3));
        ASSERT_EQ(pair.verdict(), EqualityBasisVerdict::kReduced) << scale;
        ASSERT_EQ(pair.dropped().size(), 1u) << scale;
        EXPECT_NE(pair.dropped()[0], 2) << scale;
        // The pair's own coefficient is 2 (or 1/2); row 2's is roundoff at most.
        for (Eigen::SparseMatrix<double>::InnerIterator it(pair.coefficients(), 0); it; ++it) {
            const Index rk = pair.retained()[static_cast<std::size_t>(it.row())];
            EXPECT_TRUE(std::isfinite(it.value())) << scale;
            if (rk == 2) {
                EXPECT_LE(std::abs(it.value()), 1e-14) << scale;
            } else {
                EXPECT_NEAR(std::abs(it.value()), pair.dropped()[0] == 1 ? 2.0 : 0.5, 1e-14)
                    << scale;
            }
        }
        EXPECT_TRUE(pair.verify(to_sparse(P))) << scale;
    }

    for (const double bad :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        Eigen::MatrixXd B = two_networks();
        B(3, 1) = bad;
        EqualityBasis refused;
        refused.compute(to_sparse(B), -free_box(11), free_box(11));
        EXPECT_EQ(refused.verdict(), EqualityBasisVerdict::kUndecidable) << bad;
        EXPECT_EQ(refused.refusal(), EqualityBasisRefusal::kNonFinite) << bad;
        EXPECT_FALSE(refused.reduces()) << bad;
        EXPECT_EQ(refused.retained().size(), 9u) << bad;
    }
}

// The lift system's check (fix round 1, I2; E1b: K with its inflated a-posteriori estimate): an
// acceptance below roundoff refuses the split at compute time (kLift); the default
// accepts it and every solve passes.
TEST(EqualityBasis, LiftSystemCheckRefusesTheSplit) {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(5, 4);
    A.row(0) << 1.0, 0.3, 0.0, 0.0;
    A.row(1) << 0.0, 1.0, 0.7, 0.0;
    A.row(2) << 0.0, 0.0, 1.0, 0.1;
    A.row(3) = 0.37 * A.row(0) - 1.9 * A.row(1) + 0.11 * A.row(2);
    A.row(4) = -2.3 * A.row(0) + 0.29 * A.row(2);
    EqualityBasis ok;
    ok.compute(to_sparse(A), -free_box(4), free_box(4));
    ASSERT_EQ(ok.verdict(), EqualityBasisVerdict::kReduced);
    ASSERT_EQ(ok.dropped().size(), 2u);
    Vec lam;
    EXPECT_TRUE(ok.lift(Vec::LinSpaced(3, 1.0, -1.0), lam));
    Vec t;
    EXPECT_TRUE(ok.project_rhs(Vec::LinSpaced(2, 0.5, -0.25), t));
    EXPECT_TRUE(t.allFinite());

    EqualityBasisTolerances strict;
    strict.lift_forward_tol = 1e-300;
    EqualityBasis refused;
    refused.compute(to_sparse(A), -free_box(4), free_box(4), strict);
    EXPECT_EQ(refused.verdict(), EqualityBasisVerdict::kUndecidable);
    EXPECT_EQ(refused.refusal(), EqualityBasisRefusal::kLift);
    EXPECT_FALSE(refused.reduces());
    Vec same;
    EXPECT_TRUE(refused.lift(Vec::LinSpaced(5, 1.0, -1.0), same));
    EXPECT_EQ(same, Vec::LinSpaced(5, 1.0, -1.0));
}

// Group identity is byte identity (fix round 1, M1).
TEST(EqualityBasis, GroupBytesAreTheGroupsValues) {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(2, 3);
    A.row(0) << 1.0, -1.0, 0.0;
    A.row(1) = 2.0 * A.row(0);
    EqualityBasis basis;
    basis.compute(to_sparse(A), -free_box(3), free_box(3));
    ASSERT_EQ(basis.dropped().size(), 1u);
    EXPECT_EQ(basis.group_bytes(to_sparse(A), 0), basis.group_bytes(to_sparse(A), 0));
    Eigen::MatrixXd B = A;
    B(0, 0) = std::nextafter(1.0, 2.0);
    EXPECT_NE(basis.group_bytes(to_sparse(A), 0), basis.group_bytes(to_sparse(B), 0));
}

} // namespace
} // namespace hven::solvers::detail
