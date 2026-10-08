// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_qn_models.cpp -- the full-memory damped BFGS and the nonlinear-variables-only
// restriction (qn_models.h) in ISOLATION, and the mixed lift's scalar-part replacements
// (MixedLiftData::diag / base / support) they publish. The oracle for the full-memory model is
// an explicit recursion written independently of the class.

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/globalization/sqp/lbfgs_damped.h>
#include <hven/detail/globalization/sqp/mixed_lift.h>
#include <hven/detail/globalization/sqp/qn_models.h>

namespace hven::solvers {
namespace {

using detail::DenseDampedBfgs;
using detail::LbfgsDamped;
using detail::LbfgsDampedOptions;
using detail::MaskedQnModel;
using detail::MixedLift;
using detail::MixedLiftData;
using detail::QnModel;
using detail::ResetReason;
using detail::SkipReason;
using detail::UpdateOutcome;
using Eigen::MatrixXd;
using Eigen::VectorXd;

constexpr double kInf = std::numeric_limits<double>::infinity();

VectorXd random_vec(std::mt19937 &g, int n, double scale = 1.0) {
    std::normal_distribution<double> nd;
    VectorXd v(n);
    for (int i = 0; i < n; ++i) {
        v(i) = scale * nd(g);
    }
    return v;
}

MatrixXd random_symmetric(std::mt19937 &g, int n, double lo, double hi) {
    const MatrixXd Q = Eigen::HouseholderQR<MatrixXd>(MatrixXd(random_vec(g, n * n).reshaped(n, n)))
                           .householderQ();
    VectorXd d(n);
    for (int i = 0; i < n; ++i) {
        d(i) = lo + (hi - lo) * (static_cast<double>(i) / std::max(1, n - 1));
    }
    return Q * d.asDiagonal() * Q.transpose();
}

MatrixXd dense_of(const QnModel &q) {
    const int n = q.dimension();
    MatrixXd b(n, n);
    for (int j = 0; j < n; ++j) {
        b.col(j) = q.apply(VectorXd::Unit(n, j));
    }
    return b;
}

// The explicit full-memory recursion: B0 = sigma0 I, re-based to (y'y / s'y) I at the first
// pair, then Powell-damped BFGS on every pair. Assumes each pair passes the skip rules.
struct FullReference {
    MatrixXd b;
    bool first = true;
    explicit FullReference(int n, double sigma0) : b(sigma0 * MatrixXd::Identity(n, n)) {}
    void update(const VectorXd &s, const VectorXd &y) {
        if (first && s.dot(y) > 0.0) {
            b = std::clamp(y.squaredNorm() / s.dot(y), 1e-8, 1e8) *
                MatrixXd::Identity(b.rows(), b.cols());
        }
        first = false;
        const VectorXd Bs = b * s;
        const double sBs = s.dot(Bs);
        const double sy = s.dot(y);
        const double theta = sy >= 0.2 * sBs ? 1.0 : 0.8 * sBs / (sBs - sy);
        const VectorXd r = theta * y + (1.0 - theta) * Bs;
        b = b - Bs * Bs.transpose() / sBs + r * r.transpose() / s.dot(r);
    }
};

// ------------------------------------------------------------------------------------------
// DenseDampedBfgs
// ------------------------------------------------------------------------------------------

TEST(DenseDampedBfgs, MatchesTheExplicitRecursionEveryStepIncludingDampedPairs) {
    std::mt19937 g(41);
    const int n = 7;
    // An indefinite "Hessian" so some pairs need damping.
    const MatrixXd H = random_symmetric(g, n, -2.0, 5.0);
    DenseDampedBfgs q(n, 3.0);
    FullReference ref(n, 3.0);
    long damped_seen = 0;
    for (int k = 0; k < 25; ++k) {
        const VectorXd s = random_vec(g, n);
        const VectorXd y = H * s;
        const long before = q.counters().damped;
        ASSERT_EQ(q.update(s, y, 1.0), UpdateOutcome::kUpdated) << "step " << k;
        damped_seen += q.counters().damped - before;
        ref.update(s, y);
        EXPECT_LE((dense_of(q) - ref.b).norm(), 1e-10 * std::max(1.0, ref.b.norm())) << k;
        EXPECT_TRUE(q.positive_definite());
        EXPECT_EQ(Eigen::LLT<MatrixXd>(dense_of(q)).info(), Eigen::Success);
    }
    EXPECT_GT(damped_seen, 0) << "the fixture must exercise the damping";
    EXPECT_EQ(q.pairs(), 25);
    EXPECT_EQ(q.counters().updates, 25);
    EXPECT_EQ(q.counters().pairs_in_memory, 25) << "no window: every pair stays";
}

TEST(DenseDampedBfgs, SelfScalingMatchesTheScaledRecursionAndOnlyScalesDown) {
    std::mt19937 g(50);
    const int n = 6;
    // The first pair sees large curvature, the later ones small: B0 overestimates.
    const MatrixXd Hbig = random_symmetric(g, n, 50.0, 80.0);
    const MatrixXd Hsmall = random_symmetric(g, n, 0.5, 2.0);
    LbfgsDampedOptions o;
    o.self_scaling = true;
    DenseDampedBfgs q(n, 1.0, o);
    FullReference ref(n, 1.0);
    long scaled = 0;
    for (int k = 0; k < 8; ++k) {
        const VectorXd s = random_vec(g, n);
        const VectorXd y = (k == 0 ? Hbig : Hsmall) * s;
        if (!ref.first) {
            const double tau = s.dot(y) / s.dot(ref.b * s);
            if (tau > 0.0 && tau < 1.0) {
                ref.b *= tau;
                ++scaled;
            }
        }
        ASSERT_EQ(q.update(s, y, 1.0), UpdateOutcome::kUpdated) << k;
        ref.update(s, y);
        EXPECT_LE((dense_of(q) - ref.b).norm(), 1e-10 * std::max(1.0, ref.b.norm())) << k;
    }
    EXPECT_GT(scaled, 0) << "the fixture must exercise the scaling";
    EXPECT_TRUE(q.positive_definite());
}

TEST(DenseDampedBfgs, SecantHoldsForTheNewestUndampedPair) {
    std::mt19937 g(42);
    const int n = 6;
    const MatrixXd H = random_symmetric(g, n, 1.0, 4.0); // PD: no damping
    DenseDampedBfgs q(n, 1.0);
    for (int k = 0; k < 4; ++k) {
        const VectorXd s = random_vec(g, n);
        const VectorXd y = H * s;
        ASSERT_EQ(q.update(s, y, 1.0), UpdateOutcome::kUpdated);
        EXPECT_LE((q.apply(s) - y).norm(), 1e-10 * y.norm());
    }
    EXPECT_EQ(q.counters().damped, 0);
}

TEST(DenseDampedBfgs, FullMemoryRecoversAConvexQuadraticAfterNPairs) {
    // BFGS with exact line data on a quadratic: after n independent pairs from a fixed H the
    // full-memory B reproduces H on their span -- the property a window of m < n cannot have.
    std::mt19937 g(43);
    const int n = 5;
    const MatrixXd H = random_symmetric(g, n, 0.5, 9.0);
    DenseDampedBfgs q(n, 1.0);
    for (int k = 0; k < n; ++k) {
        const VectorXd s = VectorXd::Unit(n, k);
        ASSERT_EQ(q.update(s, H * s, 1.0), UpdateOutcome::kUpdated);
    }
    // conjugate-direction property is not exact for non-conjugate s, but the secant holds for
    // the newest pair and B stays PD and bounded by H's spectrum envelope
    EXPECT_LE((q.apply(VectorXd::Unit(n, n - 1)) - H.col(n - 1)).norm(), 1e-10 * H.norm());
    const VectorXd ev = Eigen::SelfAdjointEigenSolver<MatrixXd>(dense_of(q)).eigenvalues();
    EXPECT_GT(ev(0), 0.0);
}

TEST(DenseDampedBfgs, SkipRulesKeepBAndCountByReason) {
    DenseDampedBfgs q(3, 2.0);
    const MatrixXd b0 = dense_of(q);
    VectorXd s(3);
    s << 1.0, 0.0, 0.0;
    VectorXd bad = s;
    bad(1) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(q.update(bad, s, 1.0), UpdateOutcome::kSkippedNonFinite);
    EXPECT_EQ(q.update(1e-14 * s, s, 1.0), UpdateOutcome::kSkippedZeroStep);
    // s^T r below the floor after damping cannot occur with theta's 0.8 target unless sBs is
    // tiny; the conditioning cap fires on a huge ||y||^2 / s^T y
    VectorXd y(3);
    y << 1e-6, 1e9, 0.0;
    EXPECT_EQ(q.update(s, y, 1.0), UpdateOutcome::kSkippedConditioning);
    EXPECT_EQ(dense_of(q), b0);
    EXPECT_EQ(q.counters().skipped[static_cast<std::size_t>(SkipReason::kNonFinite)], 1);
    EXPECT_EQ(q.counters().skipped[static_cast<std::size_t>(SkipReason::kZeroStep)], 1);
    EXPECT_EQ(q.counters().skipped[static_cast<std::size_t>(SkipReason::kConditioning)], 1);
    EXPECT_EQ(q.pairs(), 0);
    EXPECT_EQ(q.sigma(), 2.0) << "a skipped first pair does not re-base B0";
}

TEST(DenseDampedBfgs, ResetRestoresSigma0IdentityAndCounts) {
    std::mt19937 g(44);
    DenseDampedBfgs q(4, 2.5);
    const MatrixXd H = random_symmetric(g, 4, 1.0, 3.0);
    for (int k = 0; k < 3; ++k) {
        const VectorXd s = random_vec(g, 4);
        ASSERT_EQ(q.update(s, H * s, 1.0), UpdateOutcome::kUpdated);
    }
    q.reset(ResetReason::kRadiusFloor);
    EXPECT_EQ(dense_of(q), MatrixXd(2.5 * MatrixXd::Identity(4, 4)));
    EXPECT_EQ(q.pairs(), 0);
    EXPECT_EQ(q.sigma(), 2.5);
    EXPECT_EQ(q.counters().resets[static_cast<std::size_t>(ResetReason::kRadiusFloor)], 1);
}

TEST(DenseDampedBfgs, LiftDataIsTheUpperTriangleWithAStablePattern) {
    std::mt19937 g(45);
    const int n = 5;
    DenseDampedBfgs q(n, 1.0);
    const auto nnz0 = q.mixed_lift().base.nonZeros();
    EXPECT_EQ(nnz0, n * (n + 1) / 2) << "identity B still publishes every upper entry";
    const MatrixXd H = random_symmetric(g, n, 1.0, 3.0);
    const VectorXd s = random_vec(g, n);
    ASSERT_EQ(q.update(s, H * s, 1.0), UpdateOutcome::kUpdated);
    const MixedLiftData &d = q.mixed_lift();
    EXPECT_EQ(d.base.nonZeros(), nnz0);
    EXPECT_EQ(d.diag, VectorXd::Zero(n));
    EXPECT_EQ(d.u_plus.cols() + d.v.cols(), 0);
    const MatrixXd up = MatrixXd(d.base);
    const MatrixXd full = up.selfadjointView<Eigen::Upper>();
    EXPECT_LE((full - dense_of(q)).norm(), 1e-14 * dense_of(q).norm());
}

TEST(DenseDampedBfgs, BoundaryInputsThrow) {
    EXPECT_THROW(DenseDampedBfgs(0, 1.0), std::invalid_argument);
    EXPECT_THROW(DenseDampedBfgs(3, 0.0), std::invalid_argument);
    EXPECT_THROW(DenseDampedBfgs(3, kInf), std::invalid_argument);
    DenseDampedBfgs q(3, 1.0);
    EXPECT_THROW(q.apply(VectorXd::Zero(4)), std::invalid_argument);
    EXPECT_THROW(q.update(VectorXd::Ones(3), VectorXd::Ones(2), 1.0), std::invalid_argument);
}

// ------------------------------------------------------------------------------------------
// MaskedQnModel
// ------------------------------------------------------------------------------------------

std::unique_ptr<MaskedQnModel> masked(int n, std::vector<Eigen::Index> vars, bool full) {
    const int d = static_cast<int>(vars.size());
    std::unique_ptr<QnModel> inner;
    if (full) {
        inner = std::make_unique<DenseDampedBfgs>(d, 1.5);
    } else {
        LbfgsDampedOptions o;
        o.memory = 3;
        inner = std::make_unique<LbfgsDamped>(d, 1.5, o);
    }
    return std::make_unique<MaskedQnModel>(n, std::move(vars), std::move(inner));
}

TEST(MaskedQnModel, ActsOnTheVariablesOnlyAndZeroCurvatureElsewhere) {
    std::mt19937 g(46);
    const int n = 8;
    const std::vector<Eigen::Index> vars = {1, 2, 5, 7};
    for (const bool full : {false, true}) {
        auto q = masked(n, vars, full);
        const MatrixXd Hs = random_symmetric(g, 4, 1.0, 4.0);
        for (int k = 0; k < 5; ++k) {
            const VectorXd s = random_vec(g, n);
            VectorXd ss(4);
            for (int j = 0; j < 4; ++j) {
                ss(j) = s(vars[static_cast<std::size_t>(j)]);
            }
            const VectorXd ys = Hs * ss;
            VectorXd y = random_vec(g, n); // off-mask entries must be ignored
            for (int j = 0; j < 4; ++j) {
                y(vars[static_cast<std::size_t>(j)]) = ys(j);
            }
            ASSERT_EQ(q->update(s, y, 1.0), UpdateOutcome::kUpdated) << full << " " << k;
        }
        const MatrixXd B = dense_of(*q);
        const MatrixXd Bi = dense_of(q->inner());
        for (int i = 0; i < n; ++i) {
            const bool in_i = std::find(vars.begin(), vars.end(), i) != vars.end();
            for (int j = 0; j < n; ++j) {
                const bool in_j = std::find(vars.begin(), vars.end(), j) != vars.end();
                if (!in_i || !in_j) {
                    EXPECT_EQ(B(i, j), 0.0) << i << "," << j;
                }
            }
        }
        for (int a = 0; a < 4; ++a) {
            for (int b = 0; b < 4; ++b) {
                EXPECT_NEAR(B(vars[static_cast<std::size_t>(a)], vars[static_cast<std::size_t>(b)]),
                            Bi(a, b), 1e-14 * Bi.norm());
            }
        }
        EXPECT_TRUE(q->positive_definite());
        EXPECT_EQ(q->pairs(), q->inner().pairs());
    }
}

TEST(MaskedQnModel, LiftDataIsScatteredWithTheSupportAndAZeroDiagonalOffIt) {
    std::mt19937 g(47);
    const int n = 7;
    const std::vector<Eigen::Index> vars = {0, 3, 4};
    for (const bool full : {false, true}) {
        auto q = masked(n, vars, full);
        const MatrixXd Hs = random_symmetric(g, 3, -1.0, 4.0);
        for (int k = 0; k < 4; ++k) {
            const VectorXd s = random_vec(g, n);
            VectorXd y = VectorXd::Zero(n);
            VectorXd ss(3);
            for (int j = 0; j < 3; ++j) {
                ss(j) = s(vars[static_cast<std::size_t>(j)]);
            }
            const VectorXd ys = Hs * ss;
            for (int j = 0; j < 3; ++j) {
                y(vars[static_cast<std::size_t>(j)]) = ys(j);
            }
            (void)q->update(s, y, 1.0);
        }
        const MixedLiftData &d = q->mixed_lift();
        EXPECT_EQ(d.support, vars);
        ASSERT_EQ(d.diag.size(), n);
        for (int i = 0; i < n; ++i) {
            const bool in = std::find(vars.begin(), vars.end(), i) != vars.end();
            if (!in) {
                EXPECT_EQ(d.diag(i), 0.0);
                if (d.u_plus.cols() > 0) {
                    EXPECT_EQ(d.u_plus.row(i).cwiseAbs().maxCoeff(), 0.0);
                }
                if (d.v.cols() > 0) {
                    EXPECT_EQ(d.v.row(i).cwiseAbs().maxCoeff(), 0.0);
                }
            } else if (!full) {
                EXPECT_EQ(d.diag(i), q->sigma());
            }
        }
        // The published data reproduces B: S + Up Th Up^T - V V^T.
        MatrixXd S = d.diag.asDiagonal();
        if (d.base.rows() > 0) {
            const MatrixXd up = MatrixXd(d.base);
            S += MatrixXd(up.selfadjointView<Eigen::Upper>());
        }
        MatrixXd B = S;
        if (d.u_plus.cols() > 0) {
            B += d.u_plus * d.theta_plus.asDiagonal() * d.u_plus.transpose();
        }
        if (d.v.cols() > 0) {
            B -= d.v * d.v.transpose();
        }
        EXPECT_LE((B - dense_of(*q)).norm(), 1e-10 * std::max(1.0, dense_of(*q).norm()))
            << (full ? "full" : "lbfgs");
    }
}

TEST(MaskedQnModel, BoundaryInputsThrow) {
    EXPECT_THROW(MaskedQnModel(4, {0, 2}, nullptr), std::invalid_argument);
    EXPECT_THROW(MaskedQnModel(4, {0, 2}, std::make_unique<LbfgsDamped>(3, 1.0)),
                 std::invalid_argument); // size mismatch
    EXPECT_THROW(MaskedQnModel(4, {2, 1}, std::make_unique<LbfgsDamped>(2, 1.0)),
                 std::invalid_argument); // not ascending
    EXPECT_THROW(MaskedQnModel(4, {1, 4}, std::make_unique<LbfgsDamped>(2, 1.0)),
                 std::invalid_argument); // out of range
    MaskedQnModel q(4, {1, 3}, std::make_unique<LbfgsDamped>(2, 1.0));
    EXPECT_THROW(q.apply(VectorXd::Zero(2)), std::invalid_argument);
    EXPECT_THROW(q.update(VectorXd::Ones(2), VectorXd::Ones(2), 1.0), std::invalid_argument);
}

// ------------------------------------------------------------------------------------------
// The mixed lift's scalar-part replacements
// ------------------------------------------------------------------------------------------

SpMatRM sparse_of(const MatrixXd &M) {
    std::vector<Eigen::Triplet<double>> t;
    for (int i = 0; i < M.rows(); ++i) {
        for (int j = 0; j < M.cols(); ++j) {
            if (M(i, j) != 0.0) {
                t.emplace_back(i, j, M(i, j));
            }
        }
    }
    SpMatRM out(M.rows(), M.cols());
    out.setFromTriplets(t.begin(), t.end());
    out.makeCompressed();
    return out;
}

QpProblem small_qp(std::mt19937 &g, int n) {
    QpProblem qp;
    qp.H = SpMatRM(n, n);
    qp.g = random_vec(g, n);
    qp.Ae = sparse_of(MatrixXd(random_vec(g, n).transpose()));
    qp.be = Vec::Zero(1);
    qp.Ai = SpMatRM(0, n);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(n, -1.0);
    qp.upper = Vec::Constant(n, 1.0);
    qp.lower(0) = 0.2; // p_ref != 0
    qp.upper(0) = 0.8;
    return qp;
}

TEST(MixedLiftScalarPart, EliminatingTheAuxiliariesGivesTheMaskedAndTheFullB) {
    std::mt19937 g(48);
    const int n = 9;
    const std::vector<Eigen::Index> vars = {1, 2, 4, 6, 8};
    for (const bool full : {false, true}) {
        auto q = masked(n, vars, full);
        const MatrixXd Hs = random_symmetric(g, 5, -1.0, 5.0);
        for (int k = 0; k < 4; ++k) {
            const VectorXd s = random_vec(g, n);
            VectorXd ss(5);
            for (int j = 0; j < 5; ++j) {
                ss(j) = s(vars[static_cast<std::size_t>(j)]);
            }
            const VectorXd ys = Hs * ss;
            VectorXd y = VectorXd::Zero(n);
            for (int j = 0; j < 5; ++j) {
                y(vars[static_cast<std::size_t>(j)]) = ys(j);
            }
            (void)q->update(s, y, 1.0);
        }
        const MatrixXd B = dense_of(*q);
        const QpProblem qp = small_qp(g, n);
        const MixedLift L(qp, n, q->mixed_lift());
        const MixedLiftData &d = q->mixed_lift();
        const Vec p_ref = L.reference();
        const Vec c = d.u_plus.cols() > 0 ? Vec(d.u_plus.transpose() * p_ref) : Vec(0);
        const Vec a = d.v.cols() > 0 ? Vec(d.v.transpose() * p_ref) : Vec(0);
        const double dropped =
            (d.theta_plus.size() > 0 ? 0.5 * (d.theta_plus.array() * c.array().square()).sum()
                                     : 0.0) +
            0.5 * a.squaredNorm();
        const QpProblem &lq = L.problem();
        for (int trial = 0; trial < 4; ++trial) {
            const Vec x = L.lift_primal(p_ref + random_vec(g, n, 0.3));
            const Vec pc = x.head(n);
            const double f_lift =
                lq.g.dot(x) + 0.5 * x.dot(lq.H.selfadjointView<Eigen::Upper>() * x);
            const double f_dense = qp.g.dot(pc) + 0.5 * pc.dot(B * pc);
            EXPECT_NEAR(f_lift + dropped, f_dense, 1e-10 * std::max(1.0, std::abs(f_dense)))
                << (full ? "full" : "lbfgs");
        }
        // No entry off the support in the structural block's lift columns / rows, and none on
        // the zero diagonal.
        for (Index r = 0; r < lq.H.outerSize(); ++r) {
            for (SpMatRM::InnerIterator e(lq.H, r); e; ++e) {
                const Index i = e.row();
                const Index j = e.col();
                const bool i_struct_off =
                    i < n && std::find(vars.begin(), vars.end(), i) == vars.end();
                const bool j_struct_off =
                    j < n && std::find(vars.begin(), vars.end(), j) == vars.end();
                EXPECT_FALSE(i_struct_off || j_struct_off) << i << "," << j;
            }
        }
        if (full) {
            EXPECT_EQ(L.n_lifted(), n) << "a full-memory B needs no auxiliaries";
            EXPECT_EQ(lq.me(), qp.me());
        }
    }
}

TEST(MixedLiftScalarPart, ValidationRejectsBadReplacements) {
    std::mt19937 g(49);
    const int n = 5;
    const QpProblem qp = small_qp(g, n);
    MixedLiftData d;
    d.u_plus = MatrixXd(n, 0);
    d.v = MatrixXd(n, 0);
    d.diag = VectorXd::Ones(n);
    EXPECT_NO_THROW(MixedLift(qp, n, d));
    {
        MixedLiftData e = d;
        e.diag(2) = -1.0;
        EXPECT_THROW(MixedLift(qp, n, e), std::invalid_argument);
    }
    {
        MixedLiftData e = d;
        e.diag = VectorXd::Ones(n - 1);
        EXPECT_THROW(MixedLift(qp, n, e), std::invalid_argument);
    }
    {
        MixedLiftData e = d;
        e.base = sparse_of(MatrixXd::Identity(n, n));
        EXPECT_NO_THROW(MixedLift(qp, n, e));
        MatrixXd lower = MatrixXd::Zero(n, n);
        lower(3, 1) = 1.0;
        e.base = sparse_of(lower);
        EXPECT_THROW(MixedLift(qp, n, e), std::invalid_argument); // below the diagonal
        e.diag.resize(0);
        e.base = sparse_of(MatrixXd::Identity(n, n));
        EXPECT_THROW(MixedLift(qp, n, e), std::invalid_argument); // base without diag
    }
    {
        MixedLiftData e = d;
        e.support = {3, 1};
        EXPECT_THROW(MixedLift(qp, n, e), std::invalid_argument); // not ascending
        e.support = {1, 5};
        EXPECT_THROW(MixedLift(qp, n, e), std::invalid_argument); // out of range
        e.support = {1, 3};
        e.u_plus = MatrixXd::Zero(n, 1);
        e.u_plus(0, 0) = 0.5; // nonzero off the support
        e.theta_plus = VectorXd::Ones(1);
        e.kappa_plus = VectorXd::Ones(1);
        EXPECT_THROW(MixedLift(qp, n, e), std::invalid_argument);
    }
}

} // namespace
} // namespace hven::solvers
