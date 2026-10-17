// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_refused_exchange.cpp -- M7 W2 YRK: THE REFUSED-BLOCKER EXCHANGE (qp_engine.h,
// Degeneracy). Two restoration sub-solve QPs of the exact-Hessian TRAIN cell YORKNET (m7
// 2282f86b, restoration majors 1 and 2; n = 824 = 312 + 2 * 256 slacks, me = 256, nonconvex: 10
// negative eigenvalues), both FEASIBLE by an external oracle (HiGHS phase 1 minimum l1 violation
// 0), on which the walk ended kNumericalError at the add site's dead end standing OFF its rows
// (residual 0.40) on a FULL-RANK working set (normalized sigma_min 2e-4 / 4e-4):
//   * a slack bound (qp26: n_178 = x746; e_746 lies at relative distance 2.8e-5 from the set's
//     span, nearly -row 178 on the free columns) blocks at zero motion; its regularized test is
//     UNDECIDABLE (rho 5.3e-5) and it is admitted as a deferred add;
//   * the re-check detector, whose threshold scales with the LARGEST member norm (~132 here),
//     calls the grown set dependent, removes it and marks it known-dependent at the point;
//   * blocking again, D3(b) refuses it with no coefficients, a second slack bound goes the same
//     way, the pass-over violates both by 2e-8 / 4e-8 > feas_tol, the rank repair misses (full
//     rank), and N3 ends the solve.
// Expected: the end replaced by an exchange on the refused blocker's mu-free coefficients, and a
// KKT point the walk certifies, read here independently -- rows, box, signs, stationarity,
// complementarity, and the reduced Hessian on the final face's null space (the inertia the
// walk's own exit asserts). No oracle optimum (nonconvex). MINOR AND FACTORIZATION COUNTS ARE
// PRINTED, NEVER ASSERTED.

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
    std::printf("[ yrk ] %s %s status=%d minors=%ld facts=%ld f=%.12g refused_exchanges=%ld "
                "refusals=%ld blkterm=%ld misses=%ld\n",
                file, alg == kBorder ? "border" : "refactorize", static_cast<int>(s.status),
                static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations),
                hven::test_support::qp_dump_objective(d, s.x),
                static_cast<long>(s.counters.dep_refused_exchanges),
                static_cast<long>(s.counters.dep_known_dependent_refusals),
                static_cast<long>(s.counters.dep_blocker_terminations),
                static_cast<long>(s.counters.rank_repair_misses));
    return s;
}

// The smallest eigenvalue of Z^T H Z, Z an orthonormal basis of the null space of the equality
// rows on the final face's free columns (every pin and TR-held index fixed), relative to
// max(1, ||H||_max); +inf when the face is a vertex. Dense, test-only.
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

void expect_exchanged_to_kkt(const char *file, const char *premise, WorkingSetLinearAlgebra alg,
                             double f_release) {
    QpDump d;
    const QpSolution s = solve_fixture(file, alg, d);
    // The premise: in-cell, this feasible subproblem ended kNumericalError at the dead end.
    EXPECT_NE(d.result.find(premise), std::string::npos) << d.result;
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_GE(s.counters.dep_refused_exchanges, 1);
    EXPECT_EQ(s.counters.dep_blocker_terminations, 0);
    EXPECT_EQ(s.counters.dep_certificate_rejections, 0);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    EXPECT_LE(k.eq, 1e-7);
    // The walk holds bounds to feas_tol * max(1, |bound|, ||x||inf) (the TR box reaches 16 here).
    EXPECT_LE(k.box, 1e-9 * std::max(1.0, s.x.lpNorm<Eigen::Infinity>()));
    EXPECT_LE(k.stationarity, 1e-8);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-8);
    for (std::size_t i = 0; i < s.bound_state.size(); ++i) {
        const double zi = s.z(static_cast<Index>(i));
        if (s.bound_state[i] == BoundState::kAtLower) {
            EXPECT_GE(zi, -1e-9 * k.scale) << "lower pin " << i;
        } else if (s.bound_state[i] == BoundState::kAtUpper) {
            EXPECT_LE(zi, 1e-9 * k.scale) << "upper pin " << i;
        }
    }
    Index null_dim = 0;
    const double curv = face_min_curvature(d, s, null_dim);
    std::printf("[ yrk ] %s face null_dim=%ld min_curvature=%.3e\n", file,
                static_cast<long>(null_dim), curv);
    EXPECT_GE(curv, -1e-9) << "negative curvature on the certified face";
#ifdef NDEBUG
    // Release: the walk's own certified point (no oracle optimum: a nonconvex restoration QP).
    EXPECT_NEAR(hven::test_support::qp_dump_objective(d, s.x), f_release,
                1e-6 * std::max(1.0, std::abs(f_release)));
#else
    (void)f_release;
#endif
}

} // namespace

// The Release values measured at the landing (the walk's certified points).
constexpr double kQp26Release = 0.133368173127;
constexpr double kQp25Release = -1398.63412864;

TEST(QpYrkRefusedExchange, YorknetRestorationQp26Border) {
    expect_exchanged_to_kkt("YRK_YORKNET_qp26.txt", "status=3 minors=9 ", kBorder, kQp26Release);
}
TEST(QpYrkRefusedExchange, YorknetRestorationQp26Refactorize) {
    expect_exchanged_to_kkt("YRK_YORKNET_qp26.txt", "status=3 minors=9 ", kRefactorize,
                            kQp26Release);
}
TEST(QpYrkRefusedExchange, YorknetRestorationQp25Border) {
    expect_exchanged_to_kkt("YRK_YORKNET_qp25.txt", "status=3 minors=35 ", kBorder, kQp25Release);
}
TEST(QpYrkRefusedExchange, YorknetRestorationQp25Refactorize) {
    expect_exchanged_to_kkt("YRK_YORKNET_qp25.txt", "status=3 minors=35 ", kRefactorize,
                            kQp25Release);
}
