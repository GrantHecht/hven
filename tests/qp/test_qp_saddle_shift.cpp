// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_saddle_shift.cpp -- M7 W2 SDL: THE SADDLE EXIT's SHIFTED SEARCH (qp_engine.h, 4c).
// Two restoration sub-solve QPs of the exact-Hessian TRAIN cell YORKNET (m7 fc567ce9; n = 824,
// me = 256, nonconvex) on which the walk ended kNumericalError on a saddle-exit MISS. Their final
// faces are full rank (normalized sigma_min ~1e-2) and carry a genuine negative curvature of the
// reduced Hessian (qp29: -2.3e-3 and -4.7e-7; qp30: -2.3e-3) beside a cluster of ~20
// rounding-level eigenvalues. The face's own mu-free factor perturbs that cluster's pivots
// (kSuspect), the inverse operator's Lanczos sees only them, and every Ritz candidate reads a
// curvature of ~1e-15: no verified direction. Expected: the shifted search finds the direction,
// the ride leaves the saddle, and the walk certifies a KKT point read here independently --
// rows, box, signs, stationarity, complementarity, and a reduced Hessian on the final face that
// is PSD to tolerance. MINOR AND FACTORIZATION COUNTS ARE PRINTED, NEVER ASSERTED.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::Index;
using hven::test_support::QpDump;

namespace {

constexpr auto kBorder = WorkingSetLinearAlgebra::kSchurBorder;
constexpr auto kRefactorize = WorkingSetLinearAlgebra::kRefactorize;

QpSolution solve_fixture(const char *file, WorkingSetLinearAlgebra alg, QpDump &d) {
    d = hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + file);
    QpOptions o = d.opts;
    o.ws_algebra = alg;
    EXPECT_TRUE(o.independent_working_set) << "the dump is a `default`-label solve";
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    std::printf(
        "[ sdl ] %s %s status=%d minors=%ld facts=%ld f=%.12g saddle_exits=%ld "
        "misses=%ld shifted=%ld shifted_dirs=%ld\n",
        file, alg == kBorder ? "border" : "refactorize", static_cast<int>(s.status),
        static_cast<long>(s.counters.minor_iters), static_cast<long>(s.counters.factorizations),
        hven::test_support::qp_dump_objective(d, s.x), static_cast<long>(s.counters.saddle_exits),
        static_cast<long>(s.counters.saddle_exit_misses),
        static_cast<long>(s.counters.saddle_shifted_searches),
        static_cast<long>(s.counters.saddle_shifted_directions));
    return s;
}

// The smallest eigenvalue of Z^T H Z, Z an orthonormal basis of the null space of the equality
// rows on the final face's free columns (every pin and TR-held index fixed), relative to
// max(1, ||H||_max). Dense, test-only (as test_qp_refused_exchange.cpp's).
double face_min_curvature(const QpDump &d, const QpSolution &s, Index &null_dim) {
    const QpProblem &qp = d.qp;
    const Index n = qp.n();
    std::vector<Index> free_cols;
    for (Index i = 0; i < n; ++i) {
        const bool tr_held = static_cast<std::size_t>(i) < s.tr_active.size() &&
                             s.tr_active[static_cast<std::size_t>(i)];
        if (s.bound_state[static_cast<std::size_t>(i)] == BoundState::kFree && !tr_held) {
            free_cols.push_back(i);
        }
    }
    const auto nf = static_cast<Index>(free_cols.size());
    const Eigen::MatrixXd ae = Eigen::MatrixXd(qp.Ae);
    Eigen::MatrixXd h = Eigen::MatrixXd::Zero(n, n); // H holds the upper triangle
    for (Index r = 0; r < qp.H.outerSize(); ++r) {
        for (decltype(qp.H)::InnerIterator it(qp.H, r); it; ++it) {
            h(it.row(), it.col()) = it.value();
            h(it.col(), it.row()) = it.value();
        }
    }
    Eigen::MatrixXd af(qp.me(), nf);
    Eigen::MatrixXd hf(nf, nf);
    for (Index c = 0; c < nf; ++c) {
        af.col(c) = ae.col(free_cols[static_cast<std::size_t>(c)]);
        for (Index r = 0; r < nf; ++r) {
            hf(r, c) =
                h(free_cols[static_cast<std::size_t>(r)], free_cols[static_cast<std::size_t>(c)]);
        }
    }
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(af, Eigen::ComputeFullV);
    const Eigen::VectorXd sv = svd.singularValues();
    Index rank = 0;
    for (Index k = 0; k < sv.size(); ++k) {
        if (sv(k) > 1e-10 * sv(0)) {
            ++rank;
        }
    }
    null_dim = nf - rank;
    if (null_dim == 0) {
        return INFINITY;
    }
    const Eigen::MatrixXd z = svd.matrixV().rightCols(null_dim);
    const Eigen::MatrixXd rh = z.transpose() * hf * z;
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(rh);
    return es.eigenvalues()(0) / std::max(1.0, h.cwiseAbs().maxCoeff());
}

void expect_shifted_exit_to_kkt(const char *file, const char *premise, WorkingSetLinearAlgebra alg,
                                double f_release) {
    QpDump d;
    const QpSolution s = solve_fixture(file, alg, d);
    // The premise: in-cell, this subproblem ended kNumericalError on a saddle-exit miss.
    EXPECT_NE(d.result.find(premise), std::string::npos) << d.result;
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_GE(s.counters.saddle_shifted_directions, 1);
    EXPECT_GE(s.counters.saddle_exits, 1);
    EXPECT_EQ(s.counters.saddle_exit_misses, 0);
    EXPECT_EQ(s.counters.certificate_sign_refusals, 0);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    EXPECT_LE(k.eq, 1e-7);
    EXPECT_LE(k.box, 1e-9 * std::max(1.0, s.x.lpNorm<Eigen::Infinity>()));
    EXPECT_LE(k.stationarity, 1e-8);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-8);
    Index null_dim = 0;
    const double curv = face_min_curvature(d, s, null_dim);
    std::printf("[ sdl ] %s face null_dim=%ld min_curvature=%.3e\n", file,
                static_cast<long>(null_dim), curv);
    EXPECT_GE(curv, -1e-9) << "negative curvature on the certified face";
    // Below the in-cell saddle's objective: the ride descended.
    const double f = hven::test_support::qp_dump_objective(d, s.x);
    EXPECT_LT(f, hven::test_support::qp_dump_objective(d, d.x_final));
#ifdef NDEBUG
    EXPECT_NEAR(f, f_release, 1e-6 * std::max(1.0, std::abs(f_release)));
#else
    (void)f_release;
#endif
}

} // namespace

// The Release values measured at the landing (the walk's certified points).
constexpr double kQp29Release = -0.102164084607;
constexpr double kQp30Release = -0.0254650240685;

TEST(QpSdlSaddleShift, YorknetRestorationQp29Border) {
    expect_shifted_exit_to_kkt("SDL_YORKNET_qp29.txt", "status=3 minors=5 ", kBorder, kQp29Release);
}
TEST(QpSdlSaddleShift, YorknetRestorationQp29Refactorize) {
    expect_shifted_exit_to_kkt("SDL_YORKNET_qp29.txt", "status=3 minors=5 ", kRefactorize,
                               kQp29Release);
}
TEST(QpSdlSaddleShift, YorknetRestorationQp30Border) {
    expect_shifted_exit_to_kkt("SDL_YORKNET_qp30.txt", "status=3 minors=17 ", kBorder,
                               kQp30Release);
}
TEST(QpSdlSaddleShift, YorknetRestorationQp30Refactorize) {
    expect_shifted_exit_to_kkt("SDL_YORKNET_qp30.txt", "status=3 minors=17 ", kRefactorize,
                               kQp30Release);
}
