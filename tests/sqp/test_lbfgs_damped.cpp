// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_lbfgs_damped.cpp -- the damped compact L-BFGS class in ISOLATION (no driver, no
// QP). The oracle is an explicit dense recursive damped BFGS that applies the SAME sigma-rebuild
// sequence (each step: B0 = sigma_k*I, replay the stored pairs), written independently of the
// class's compact / eigen machinery.

#include <cmath>
#include <functional>
#include <limits>
#include <random>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/globalization/sqp/lbfgs_damped.h>

namespace {

using Eigen::MatrixXd;
using Eigen::VectorXd;
using hven::solvers::detail::LbfgsDamped;
using hven::solvers::detail::LbfgsDampedOptions;
using hven::solvers::detail::ResetReason;
using hven::solvers::detail::SkipReason;
using hven::solvers::detail::UpdateOutcome;

// Dense recursive damped BFGS with the sigma-rebuild convention.
struct DenseReference {
    int n;
    int memory;
    double sigma;
    std::vector<VectorXd> s, r;

    DenseReference(int n_, int m_, double sigma0) : n(n_), memory(m_), sigma(sigma0) {}

    MatrixXd B() const {
        MatrixXd b = sigma * MatrixXd::Identity(n, n);
        for (std::size_t i = 0; i < s.size(); ++i) {
            const VectorXd Bs = b * s[i];
            b = b - Bs * Bs.transpose() / s[i].dot(Bs) + r[i] * r[i].transpose() / s[i].dot(r[i]);
        }
        return b;
    }
    // Assumes the offered pair passes every skip rule (the tests choose such pairs).
    void update(const VectorXd &sv, const VectorXd &y) {
        const VectorXd Bs = B() * sv;
        const double sBs = sv.dot(Bs);
        const double sy = sv.dot(y);
        const double theta = sy >= 0.2 * sBs ? 1.0 : 0.8 * sBs / (sBs - sy);
        const VectorXd rv = theta * y + (1.0 - theta) * Bs;
        s.push_back(sv);
        r.push_back(rv);
        if (static_cast<int>(s.size()) > memory) {
            s.erase(s.begin());
            r.erase(r.begin());
        }
        sigma = std::clamp(rv.squaredNorm() / sv.dot(rv), 1e-8, 1e8);
    }
};

MatrixXd dense_of(const LbfgsDamped &q) {
    const int n = q.dimension();
    MatrixXd b(n, n);
    for (int j = 0; j < n; ++j) {
        VectorXd e = VectorXd::Zero(n);
        e(j) = 1.0;
        b.col(j) = q.apply(e);
    }
    return b;
}

double rel(const MatrixXd &a, const MatrixXd &b) {
    return (a - b).norm() / std::max(1.0, b.norm());
}

// A random symmetric H with eigenvalues in [lo, hi] (negative lo exercises the damping).
MatrixXd random_hessian(std::mt19937 &g, int n, double lo, double hi) {
    std::normal_distribution<double> nd;
    MatrixXd A(n, n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            A(i, j) = nd(g);
    Eigen::HouseholderQR<MatrixXd> qr(A);
    const MatrixXd Q = qr.householderQ();
    VectorXd d(n);
    for (int i = 0; i < n; ++i)
        d(i) = lo + (hi - lo) * (static_cast<double>(i) / (n - 1));
    return Q * d.asDiagonal() * Q.transpose();
}

VectorXd random_vec(std::mt19937 &g, int n) {
    std::normal_distribution<double> nd;
    VectorXd v(n);
    for (int i = 0; i < n; ++i)
        v(i) = nd(g);
    return v;
}

std::string message_of(const std::function<void()> &f) {
    try {
        f();
    } catch (const std::invalid_argument &e) {
        return e.what();
    }
    return "<no throw>";
}

} // namespace

TEST(LbfgsDamped, CompactMatchesDenseRecursiveReferenceEveryStep) {
    for (unsigned seed = 1; seed <= 5; ++seed) {
        std::mt19937 g(seed);
        const int n = 12;
        const MatrixXd H = random_hessian(g, n, -3.0, 1.0); // indefinite -> damping fires
        LbfgsDamped q(n, 1.0, {.memory = 4});
        DenseReference ref(n, 4, 1.0);
        for (int k = 0; k < 9; ++k) {
            const VectorXd s = random_vec(g, n);
            const VectorXd y = H * s;
            ASSERT_EQ(q.update(s, y, 1.0), UpdateOutcome::kUpdated)
                << "seed " << seed << " k " << k;
            ref.update(s, y);
            EXPECT_LT(rel(dense_of(q), ref.B()), 1e-12) << "seed " << seed << " k " << k;
            EXPECT_EQ(q.pairs(), static_cast<int>(ref.s.size()));
            EXPECT_NEAR(q.sigma(), ref.sigma, 1e-12 * ref.sigma);
        }
        EXPECT_GT(q.counters().damped, 0);
    }
}

TEST(LbfgsDamped, SecantHoldsForTheNewestPair) {
    std::mt19937 g(7);
    const int n = 15;
    const MatrixXd H = random_hessian(g, n, -0.5, 4.0);
    LbfgsDamped q(n, 2.0);
    for (int k = 0; k < 6; ++k) {
        const VectorXd s = random_vec(g, n);
        ASSERT_EQ(q.update(s, H * s, 1.0), UpdateOutcome::kUpdated);
        const VectorXd r = q.stored_r().back();
        EXPECT_LT((q.apply(s) - r).norm() / r.norm(), 1e-10) << "k " << k;
    }
}

TEST(LbfgsDamped, SpectrumLiftAndOrthonormality) {
    std::mt19937 g(11);
    const int n = 14;
    const MatrixXd H = random_hessian(g, n, -2.0, 6.0);
    LbfgsDamped q(n, 1.5, {.memory = 3});
    for (int k = 0; k < 5; ++k) {
        const VectorXd s = random_vec(g, n);
        ASSERT_EQ(q.update(s, H * s, 1.0), UpdateOutcome::kUpdated);
    }
    const MatrixXd B = dense_of(q);
    const VectorXd ev = Eigen::SelfAdjointEigenSolver<MatrixXd>(B).eigenvalues();
    // PD, and the extremes are the ones read from Theta.
    EXPECT_GT(ev.minCoeff(), 0.0);
    EXPECT_GT(q.min_eigenvalue(), 0.0);
    EXPECT_NEAR(ev.minCoeff(), q.min_eigenvalue(), 1e-10 * ev.maxCoeff());
    EXPECT_NEAR(ev.maxCoeff(), q.max_eigenvalue(), 1e-10 * ev.maxCoeff());
    // U orthonormal.
    const MatrixXd &U = q.eigenvectors();
    EXPECT_LT((U.transpose() * U - MatrixXd::Identity(U.cols(), U.cols())).norm(), 1e-12);
    // sigma I + U+ Theta+ U+^T - V V^T reconstructs B.
    const auto &L = q.mixed_lift();
    const MatrixXd recon = L.sigma * MatrixXd::Identity(n, n) +
                           L.u_plus * L.theta_plus.asDiagonal() * L.u_plus.transpose() -
                           L.v * L.v.transpose();
    EXPECT_LT(rel(recon, B), 1e-12);
    EXPECT_GT(L.u_plus.cols(), 0);
    EXPECT_GT(L.v.cols(), 0) << "an indefinite-curvature run should produce negative theta";
    EXPECT_GT(L.theta_plus.minCoeff(), 0.0);
}

TEST(LbfgsDamped, LiftScalesDominateTwiceTheColumnOneNorm) {
    std::mt19937 g(13);
    const int n = 16;
    const MatrixXd H = random_hessian(g, n, -1.0, 3.0);
    LbfgsDamped q(n, 1.0, {.memory = 4});
    for (int k = 0; k < 6; ++k) {
        const VectorXd s = random_vec(g, n);
        q.update(s, H * s, 1.0);
    }
    const auto &L = q.mixed_lift();
    ASSERT_EQ(L.kappa_plus.size(), L.u_plus.cols());
    ASSERT_EQ(L.kappa_v.size(), L.v.cols());
    for (int i = 0; i < L.u_plus.cols(); ++i) {
        EXPECT_GE(L.kappa_plus(i), 2.0 * L.u_plus.col(i).lpNorm<1>());
        EXPECT_GE(L.kappa_plus(i), 2.0); // orthonormal column: ||.||_1 >= 1
    }
    for (int i = 0; i < L.v.cols(); ++i) {
        EXPECT_GE(L.kappa_v(i), 2.0 * L.v.col(i).lpNorm<1>());
    }
    // The point of the scale: under ||p||_inf <= Delta the scaled auxiliary stays in the window.
    const double delta = 0.7;
    const VectorXd p = delta * VectorXd::Ones(n).cwiseProduct(random_vec(g, n).cwiseSign());
    for (int i = 0; i < L.u_plus.cols(); ++i) {
        EXPECT_LE(std::abs(L.u_plus.col(i).dot(p)) / L.kappa_plus(i), 0.5 * delta + 1e-15);
    }
}

TEST(LbfgsDamped, ApplyEqualsDenseB) {
    std::mt19937 g(17);
    const int n = 10;
    const MatrixXd H = random_hessian(g, n, -1.0, 2.0);
    LbfgsDamped q(n, 1.0, {.memory = 3});
    DenseReference ref(n, 3, 1.0);
    for (int k = 0; k < 6; ++k) {
        const VectorXd s = random_vec(g, n);
        q.update(s, H * s, 1.0);
        ref.update(s, H * s);
    }
    const VectorXd v = random_vec(g, n);
    EXPECT_LT((q.apply(v) - ref.B() * v).norm() / (ref.B() * v).norm(), 1e-12);
}

TEST(LbfgsDamped, EmptyWindowIsSigmaIdentity) {
    LbfgsDamped q(5, 3.0);
    const VectorXd v = VectorXd::LinSpaced(5, 1.0, 5.0);
    EXPECT_LT((q.apply(v) - 3.0 * v).norm(), 1e-15);
    EXPECT_EQ(q.pairs(), 0);
    EXPECT_EQ(q.mixed_lift().u_plus.cols(), 0);
    EXPECT_EQ(q.mixed_lift().v.cols(), 0);
    EXPECT_DOUBLE_EQ(q.min_eigenvalue(), 3.0);
    EXPECT_DOUBLE_EQ(q.max_eigenvalue(), 3.0);
}

TEST(LbfgsDamped, WindowWrapsAtMemoryPlusThree) {
    std::mt19937 g(19);
    const int n = 20, m = 4;
    const MatrixXd H = random_hessian(g, n, 0.5, 4.0);
    LbfgsDamped q(n, 1.0, {.memory = m});
    std::vector<VectorXd> all;
    for (int k = 0; k < m + 3; ++k) {
        all.push_back(random_vec(g, n));
        ASSERT_EQ(q.update(all.back(), H * all.back(), 1.0), UpdateOutcome::kUpdated);
    }
    EXPECT_EQ(q.counters().updates, m + 3);
    EXPECT_EQ(q.counters().pairs_in_memory, m);
    ASSERT_EQ(q.pairs(), m);
    for (int i = 0; i < m; ++i) {
        EXPECT_EQ(q.stored_s()[static_cast<std::size_t>(i)], all[static_cast<std::size_t>(3 + i)]);
    }
    EXPECT_LE(q.eigenvectors().cols(), 2 * m);
}

TEST(LbfgsDamped, SkipNonFinite) {
    LbfgsDamped q(4, 1.0);
    VectorXd s = VectorXd::Ones(4), y = VectorXd::Ones(4);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    VectorXd sn = s;
    sn(2) = nan;
    VectorXd yi = y;
    yi(0) = inf;
    EXPECT_EQ(q.update(sn, y, 1.0), UpdateOutcome::kSkippedNonFinite);
    EXPECT_EQ(q.update(s, yi, 1.0), UpdateOutcome::kSkippedNonFinite);
    EXPECT_EQ(q.update(s, y, nan), UpdateOutcome::kSkippedNonFinite);
    EXPECT_EQ(q.counters().skipped[static_cast<std::size_t>(SkipReason::kNonFinite)], 3);
    EXPECT_EQ(q.counters().updates, 0);
    EXPECT_EQ(q.pairs(), 0);
}

TEST(LbfgsDamped, SkipZeroStepUsesTheIterateScale) {
    LbfgsDamped q(3, 1.0, {.zero_step_scale = 1e-12});
    const VectorXd y = VectorXd::Ones(3);
    // ||s||_inf = 5e-13 <= 1e-12 * max(1, 1): skipped.
    EXPECT_EQ(q.update(VectorXd::Constant(3, 5e-13), y, 1.0), UpdateOutcome::kSkippedZeroStep);
    // Same step, iterate of size 1e3: threshold 1e-9, still skipped; a step of 5e-10 is skipped
    // at x_inf = 1e3 but accepted at x_inf = 1.
    EXPECT_EQ(q.update(VectorXd::Constant(3, 5e-10), y, 1e3), UpdateOutcome::kSkippedZeroStep);
    EXPECT_EQ(q.update(VectorXd::Constant(3, 5e-10), 5e-10 * VectorXd::Ones(3), 1.0),
              UpdateOutcome::kUpdated);
    EXPECT_EQ(q.counters().skipped[static_cast<std::size_t>(SkipReason::kZeroStep)], 2);
}

TEST(LbfgsDamped, SkipCurvatureWhenBsHasNoCurvatureAlongS) {
    // B = 1e-12 I; one pair (s = e1, y = e3) damps to r = 0.8 e3 + 0.2e-12 e1, so along e1 the
    // curvature is 2e-13 while sigma = rr/sr clamps to 1e8. The gate is noise-limited by nature
    // (s^T B s is a difference of numbers ~sigma), so the computed value sits at rounding level,
    // below eps_mach * ||s||^2 * sigma = 2.2e-8. The curvature floor and conditioning cap are
    // relaxed so the setup pair enters the window.
    LbfgsDamped q(6, 1e-12, {.curvature_floor = 0.0, .conditioning_cap = 1e300});
    VectorXd e1 = VectorXd::Zero(6), e3 = VectorXd::Zero(6);
    e1(0) = 1.0;
    e3(2) = 1.0;
    ASSERT_EQ(q.update(e1, e3, 1.0), UpdateOutcome::kUpdated);
    ASSERT_EQ(q.pairs(), 1);
    EXPECT_DOUBLE_EQ(q.sigma(), 1e8);
    ASSERT_LE(e1.dot(q.apply(e1)), std::numeric_limits<double>::epsilon() * q.sigma());
    EXPECT_EQ(q.update(e1, e1, 1.0), UpdateOutcome::kSkippedCurvature);
    EXPECT_EQ(q.counters().skipped[static_cast<std::size_t>(SkipReason::kCurvature)], 1);
}

TEST(LbfgsDamped, SkipConditioningBothArms) {
    // (a) s^T r < 1e-8 ||s||^2: B = 1e-9 I and y = 0 give r = 0.2e-9 s.
    {
        LbfgsDamped q(4, 1e-9);
        EXPECT_EQ(q.update(VectorXd::Ones(4), VectorXd::Zero(4), 1.0),
                  UpdateOutcome::kSkippedConditioning);
        EXPECT_EQ(q.counters().skipped[static_cast<std::size_t>(SkipReason::kConditioning)], 1);
    }
    // (b) ||r||^2 / s^T r > 1e8: y has a huge component orthogonal to s (theta = 1 as s^T y = 1).
    {
        LbfgsDamped q(4, 1.0);
        VectorXd s = VectorXd::Zero(4), y = VectorXd::Zero(4);
        s(0) = 1.0;
        y(0) = 1.0;
        y(1) = 1e6;
        EXPECT_EQ(q.update(s, y, 1.0), UpdateOutcome::kSkippedConditioning);
        EXPECT_EQ(q.pairs(), 0);
    }
}

TEST(LbfgsDamped, SigmaIsClampedToTheBand) {
    // Lower clamp: B0 = 1e-9 I, y = 1e-10 s damps to r ~ 2e-10 s -> sigma 2e-10 -> 1e-8.
    LbfgsDamped lo(4, 1e-9, {.curvature_floor = 0.0});
    ASSERT_EQ(lo.update(VectorXd::Ones(4), 1e-10 * VectorXd::Ones(4), 1.0),
              UpdateOutcome::kUpdated);
    EXPECT_DOUBLE_EQ(lo.sigma(), 1e-8);
    // Upper clamp: y = 1e10 s -> sigma 1e10 -> 1e8 (cap relaxed so the pair is admitted).
    LbfgsDamped hi(4, 1.0, {.conditioning_cap = 1e12});
    ASSERT_EQ(hi.update(VectorXd::Ones(4), 1e10 * VectorXd::Ones(4), 1.0), UpdateOutcome::kUpdated);
    EXPECT_DOUBLE_EQ(hi.sigma(), 1e8);
}

TEST(LbfgsDamped, RankDeficientWindowDropsTheOldestPair) {
    std::mt19937 g(23);
    const int n = 8;
    const VectorXd s = random_vec(g, n);
    const VectorXd y = 2.0 * s + 0.3 * random_vec(g, n);
    LbfgsDamped q(n, 1.0);
    ASSERT_EQ(q.update(s, y, 1.0), UpdateOutcome::kUpdated);
    EXPECT_EQ(q.pairs(), 1);
    // The identical pair again: W has repeated columns, so the oldest copy is dropped.
    ASSERT_EQ(q.update(s, y, 1.0), UpdateOutcome::kUpdated);
    EXPECT_EQ(q.pairs(), 1);
    EXPECT_EQ(q.counters().rank_dropped, 1);
    EXPECT_EQ(q.counters().updates, 2);
    EXPECT_LT((q.apply(s) - q.stored_r().back()).norm() / q.stored_r().back().norm(), 1e-10);
    // Exact scalar curvature (y = c s): r parallel to sigma*s, every pair dropped, B = sigma I.
    LbfgsDamped c(n, 1.0);
    ASSERT_EQ(c.update(s, 3.0 * s, 1.0), UpdateOutcome::kUpdated);
    EXPECT_EQ(c.pairs(), 0);
    EXPECT_DOUBLE_EQ(c.sigma(), 3.0);
    const VectorXd v = random_vec(g, n);
    EXPECT_LT((c.apply(v) - 3.0 * v).norm(), 1e-14 * v.norm());
}

TEST(LbfgsDamped, MoreColumnsThanDimensionIsTrimmed) {
    std::mt19937 g(29);
    const int n = 5; // 2p <= n  =>  at most 2 pairs
    const MatrixXd H = random_hessian(g, n, 0.5, 3.0);
    LbfgsDamped q(n, 1.0, {.memory = 10});
    for (int k = 0; k < 6; ++k) {
        const VectorXd s = random_vec(g, n);
        q.update(s, H * s, 1.0);
        EXPECT_LE(2 * q.pairs(), n);
    }
    EXPECT_GT(q.counters().rank_dropped, 0);
    EXPECT_GT(q.min_eigenvalue(), 0.0);
}

TEST(LbfgsDamped, SmallEigenColumnsAreTruncated) {
    std::mt19937 g(31);
    const int n = 10;
    const VectorXd s = random_vec(g, n);
    // Nearly scalar curvature: the single non-trivial eigenvalue is ~1e-9*sigma.
    const VectorXd y = 3.0 * s + 1e-9 * random_vec(g, n);
    LbfgsDamped keep(n, 1.0, {.truncation_tol = 0.0});
    LbfgsDamped cut(n, 1.0); // default 1e-6
    ASSERT_EQ(keep.update(s, y, 1.0), UpdateOutcome::kUpdated);
    ASSERT_EQ(cut.update(s, y, 1.0), UpdateOutcome::kUpdated);
    ASSERT_GT(keep.eigenvalues().size(), 0);
    EXPECT_LT(cut.eigenvalues().size(), keep.eigenvalues().size());
    for (int i = 0; i < cut.eigenvalues().size(); ++i) {
        EXPECT_GT(std::abs(cut.eigenvalues()(i)), 1e-6 * cut.sigma());
    }
    // The truncation moves B by at most tau'*sigma in the operator norm.
    const MatrixXd d = dense_of(keep) - dense_of(cut);
    EXPECT_LE(d.operatorNorm(), 1e-6 * cut.sigma());
}

TEST(LbfgsDamped, ResetClearsTheWindowAndCountsByReason) {
    std::mt19937 g(37);
    const int n = 9;
    const MatrixXd H = random_hessian(g, n, 0.5, 3.0);
    LbfgsDamped q(n, 2.5);
    for (int k = 0; k < 3; ++k) {
        const VectorXd s = random_vec(g, n);
        q.update(s, H * s, 1.0);
    }
    ASSERT_GT(q.pairs(), 0);
    q.reset(ResetReason::kRadiusFloor);
    EXPECT_EQ(q.pairs(), 0);
    EXPECT_DOUBLE_EQ(q.sigma(), 2.5);
    EXPECT_EQ(q.counters().pairs_in_memory, 0);
    EXPECT_EQ(q.counters().resets[static_cast<std::size_t>(ResetReason::kRadiusFloor)], 1);
    q.reset(ResetReason::kConsecutiveSkips);
    q.reset(ResetReason::kConsecutiveSkips);
    EXPECT_EQ(q.counters().resets[static_cast<std::size_t>(ResetReason::kConsecutiveSkips)], 2);
    const VectorXd v = random_vec(g, n);
    EXPECT_LT((q.apply(v) - 2.5 * v).norm(), 1e-14 * v.norm());
    q.set_sigma0(4.0);
    EXPECT_DOUBLE_EQ(q.sigma(), 4.0);
}

TEST(LbfgsDamped, BoundaryInputsThrowNamingTheField) {
    LbfgsDamped q(4, 1.0);
    const VectorXd ok = VectorXd::Ones(4), bad = VectorXd::Ones(3);
    EXPECT_NE(message_of([&] { q.update(bad, ok, 1.0); }).find("update: s"), std::string::npos);
    EXPECT_NE(message_of([&] { q.update(ok, bad, 1.0); }).find("update: y"), std::string::npos);
    EXPECT_NE(message_of([&] { q.apply(bad); }).find("apply: v"), std::string::npos);
    VectorXd nf = ok;
    nf(1) = std::numeric_limits<double>::quiet_NaN();
    const std::string m = message_of([&] { q.apply(nf); });
    EXPECT_NE(m.find("apply: v"), std::string::npos);
    EXPECT_NE(m.find("non-finite"), std::string::npos);
    EXPECT_NE(message_of([] { LbfgsDamped(0, 1.0); }).find("n"), std::string::npos);
    EXPECT_NE(message_of([] { LbfgsDamped(3, -1.0); }).find("sigma0"), std::string::npos);
    EXPECT_NE(
        message_of([] { LbfgsDamped(3, std::numeric_limits<double>::infinity()); }).find("sigma0"),
        std::string::npos);
    EXPECT_NE(message_of([] { LbfgsDamped(3, 1.0, {.memory = 0}); }).find("options.memory"),
              std::string::npos);
    EXPECT_NE(message_of([&] { q.set_sigma0(0.0); }).find("sigma0"), std::string::npos);
}
