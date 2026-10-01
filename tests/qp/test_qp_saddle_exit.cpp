// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_saddle_exit.cpp -- M7 W2 NC3: THE SADDLE EXIT (qp_engine.h section 4c), on the walk
// seam (docs/testing.md). A trusted-kWrong dead end -- a stationary point of a face whose
// reduced Hessian is indefinite, nothing left to drop -- used to end the solve kNumericalError
// (the certification branch of section 4b). The exit searches the face for a VERIFIED
// negative-curvature direction and rides it; nothing is certified by the exit itself.
//
// The dumps are the C10 census's (tests/qp/fixtures/e2-d4/C10_*, the E2-D4 format; the
// `result` line is the in-cell outcome at m7 1f8d3118, `site=1335` the dead end's exit). The
// elastic QPs of STEENBRB/D/E are feasible and their final face carries negative curvature
// (dense oracle in the NC3 report: Z'HZ minima -12.3 / -250 / -250); the walk now leaves
// each saddle and ends at a lower objective on a face whose reduced Hessian is POSITIVE
// DEFINITE, which this file checks itself (a dense QR of the final working set). No global
// oracle exists for these nonconvex QPs, so no objective is pinned: the assertions are the
// second-order point, the KKT battery and the decrease.
//
// MINOR AND FACTORIZATION COUNTS ARE PRINTED, NEVER ASSERTED (Release and Debug round these
// subproblems differently; see test_qp_standstill_retreat.cpp).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::Index;
using hven::Vec;
using hven::test_support::QpDump;
using Obs = hven::solvers::detail::testing::QpWalkObserver;
using Action = hven::solvers::detail::testing::QpWalkAction;
using Record = hven::solvers::detail::testing::QpWalkMinorRecord;

namespace {

constexpr auto kBorder = WorkingSetLinearAlgebra::kSchurBorder;
constexpr auto kRefactorize = WorkingSetLinearAlgebra::kRefactorize;

const char *alg_name(WorkingSetLinearAlgebra alg) {
    return alg == kBorder ? "border" : "refactorize";
}

double objective(const QpProblem &qp, const Vec &x) {
    return 0.5 * x.dot(qp.H.selfadjointView<Eigen::Upper>() * x) + qp.g.dot(x);
}

struct Walk {
    QpSolution solution;
    std::vector<Record> records;
    bool truncated = false;
};

template <class Solve> Walk observed(Solve &&solve) {
    Obs::reset();
    Obs::active = true;
    Walk w;
    w.solution = solve();
    w.records = Obs::records;
    w.truncated = Obs::truncated;
    Obs::active = false;
    Obs::reset();
    return w;
}

/// The smallest eigenvalue of the reduced Hessian on the null space of the point's own working
/// set (the equality rows, the active inequality rows, every pinned column -- trust-region pins
/// included), from a dense QR of the free block. NaN for a face with no free direction.
double reduced_hessian_min(const QpProblem &qp, const QpSolution &s) {
    const Index n = qp.n();
    std::vector<Index> freec;
    for (Index i = 0; i < n; ++i) {
        const auto si = static_cast<std::size_t>(i);
        const bool tr = si < s.tr_active.size() && s.tr_active[si];
        if (s.bound_state[si] == BoundState::kFree && !tr) {
            freec.push_back(i);
        }
    }
    const auto nf = static_cast<Index>(freec.size());
    if (nf == 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::vector<Index> rows;
    for (Index j = 0; j < qp.mi(); ++j) {
        if (s.ineq_active[static_cast<std::size_t>(j)]) {
            rows.push_back(j);
        }
    }
    const Index m = qp.me() + static_cast<Index>(rows.size());
    const Eigen::MatrixXd Ae = Eigen::MatrixXd(qp.Ae);
    const Eigen::MatrixXd Ai = Eigen::MatrixXd(qp.Ai);
    Eigen::MatrixXd AF(m, nf);
    for (Index k = 0; k < nf; ++k) {
        for (Index r = 0; r < qp.me(); ++r) {
            AF(r, k) = Ae(r, freec[static_cast<std::size_t>(k)]);
        }
        for (std::size_t r = 0; r < rows.size(); ++r) {
            AF(qp.me() + static_cast<Index>(r), k) =
                Ai(rows[r], freec[static_cast<std::size_t>(k)]);
        }
    }
    Eigen::MatrixXd Z;
    if (m == 0) {
        Z = Eigen::MatrixXd::Identity(nf, nf);
    } else {
        const Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(AF.transpose());
        const Index rank = qr.rank();
        if (rank == nf) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        const Eigen::MatrixXd Q = qr.householderQ();
        Z = Q.rightCols(nf - rank);
    }
    const Eigen::MatrixXd Hu = Eigen::MatrixXd(qp.H); // the upper triangle
    const Eigen::MatrixXd H = Hu.selfadjointView<Eigen::Upper>();
    Eigen::MatrixXd HF(nf, nf);
    for (Index a = 0; a < nf; ++a) {
        for (Index b = 0; b < nf; ++b) {
            HF(a, b) = H(freec[static_cast<std::size_t>(a)], freec[static_cast<std::size_t>(b)]);
        }
    }
    const Eigen::MatrixXd R = Z.transpose() * HF * Z;
    const Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(R, Eigen::EigenvaluesOnly);
    return es.eigenvalues()(0);
}

/// THE DESCENT INVARIANT of every saddle pass (4c, THE SADDLE EXIT): the direction ridden has
/// NEGATIVE curvature and a slope no higher than its own resolution, and the objective the
/// next pass starts from is not above this pass's -- strictly below whenever the ride moved.
void expect_saddle_passes_descend(const std::vector<Record> &records, const QpCounters &c) {
    Index exits = 0;
    Index misses = 0;
    for (std::size_t k = 0; k < records.size(); ++k) {
        const Record &r = records[k];
        if (!r.saddle_exit) {
            continue;
        }
        if (!std::isfinite(r.saddle_curvature)) {
            ++misses;
            continue;
        }
        ++exits;
        EXPECT_LT(r.saddle_curvature, 0.0) << "minor " << r.minor;
        EXPECT_LE(r.saddle_slope, r.saddle_slope_tol)
            << "a saddle ride ascended a slope the arithmetic can read, minor " << r.minor;
        if (k + 1 < records.size() && std::isfinite(records[k + 1].obj_in) &&
            std::isfinite(r.obj_in)) {
            const double next = records[k + 1].obj_in;
            if (r.dx_inf > r.step_tol) {
                EXPECT_LT(next, r.obj_in)
                    << "a saddle ride that moved did not descend, minor " << r.minor;
            } else {
                EXPECT_LE(next, r.obj_in) << "minor " << r.minor;
            }
        }
    }
    EXPECT_EQ(exits, c.saddle_exits);
    EXPECT_EQ(misses, c.saddle_exit_misses);
}

// THE ELASTIC SADDLES. Premise (the dump's own result line): in-cell kNumericalError at the
// dead end. Expected: kOptimal strictly below the saddle's objective, the KKT battery, a
// positive-definite reduced Hessian on the final face, and at least one saddle exit taken.
void expect_saddle_left(const char *file, WorkingSetLinearAlgebra alg) {
    const QpDump d =
        hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + file);
    ASSERT_NE(d.result.find("status=3"), std::string::npos) << d.result;
    ASSERT_NE(d.result.find("site=1335"), std::string::npos) << d.result;
    const double f_saddle = objective(d.qp, d.x_final);

    QpOptions o = d.opts;
    o.ws_algebra = alg;
    ASSERT_TRUE(o.independent_working_set) << "the dump is a `default`-label solve";
    const Walk w = observed([&] { return hven::test_support::solve_qp_dump(d, o); });
    const QpSolution &s = w.solution;
    ASSERT_FALSE(w.truncated);

    const double f = objective(d.qp, s.x);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    const double rh = reduced_hessian_min(d.qp, s);
    std::printf("[ nc3 ] %s %s status=%d minors=%ld facts=%ld f=%.12g f_saddle=%.12g "
                "redH_min=%.6g eq=%.2e stat=%.2e saddle_exits=%ld misses=%ld solves=%ld\n",
                file, alg_name(alg), static_cast<int>(s.status),
                static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations), f, f_saddle, rh, k.eq, k.stationarity,
                static_cast<long>(s.counters.saddle_exits),
                static_cast<long>(s.counters.saddle_exit_misses),
                static_cast<long>(s.counters.saddle_direction_solves));

    EXPECT_GE(s.counters.saddle_exits, 1);
    expect_saddle_passes_descend(w.records, s.counters);
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    if (s.status != QpStatus::kOptimal) {
        return;
    }
    EXPECT_LT(f, f_saddle - 1e-6 * std::max(1.0, std::abs(f_saddle)))
        << "the exit must end strictly below the saddle it left";
    EXPECT_LE(k.eq, 1e-8);
    EXPECT_LE(k.ineq, 1e-8);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-8);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-8);
    EXPECT_GT(rh, 0.0) << "the final face is not second-order: reduced Hessian min " << rh;
}

// THE MISSES, which must stay misses: no ride without a VERIFIED direction.
//   STEENBRB qp3 is a restoration LP (H == 0) whose walk stops AT its optimum and still reads
//   kWrong there -- a false kWrong on a zero-curvature face (registered, not this round's): there
//   is no curvature to find, and the exit must not ride.
//   STEENBRE qp2's final face carries Z'HZ = -249.9 against a Hessian of norm 6.2e15: -4e-14 of
//   hessian_scale, below curvature_tol, so not a negative curvature the engine can resolve.
void expect_no_ride(const char *file, WorkingSetLinearAlgebra alg) {
    const QpDump d =
        hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + file);
    ASSERT_NE(d.result.find("site=1335"), std::string::npos) << d.result;
    QpOptions o = d.opts;
    o.ws_algebra = alg;
    const Walk w = observed([&] { return hven::test_support::solve_qp_dump(d, o); });
    const QpSolution &s = w.solution;
    std::printf("[ nc3 ] %s %s status=%d minors=%ld f=%.12g saddle_exits=%ld misses=%ld\n", file,
                alg_name(alg), static_cast<int>(s.status),
                static_cast<long>(s.counters.minor_iters), objective(d.qp, s.x),
                static_cast<long>(s.counters.saddle_exits),
                static_cast<long>(s.counters.saddle_exit_misses));
    EXPECT_EQ(s.counters.saddle_exits, 0);
    EXPECT_GE(s.counters.saddle_exit_misses, 1) << "the search did not run at the dead end";
    EXPECT_NE(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal))
        << "no rule of this round certifies this point";
    expect_saddle_passes_descend(w.records, s.counters);
}

// ---------------------------------------------------------------------------
// A SYNTHETIC SADDLE REACHED WITHOUT A DROP. min -x0^2 + x1^2 - 0.1 x0 subject to the two rows
// x0 <= 1 and -x0 <= 1, x unbounded: the start face (no row active) is indefinite and has no
// finite bound to pin, so the start repair cannot fix it; the EQP walks to the face's only
// stationary point (-0.05, 0), a saddle, where nothing can be dropped. The two local
// minimizers are (1, 0) at f = -1.1 and (-1, 0) at f = -0.9; the exit's direction is +/-e0
// and either is a correct answer.
// ---------------------------------------------------------------------------
QpProblem row_bounded_saddle() {
    QpProblem qp;
    Eigen::MatrixXd H(2, 2);
    H << -2.0, 0.0, 0.0, 2.0;
    qp.H = H.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -0.1, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Ai(2, 2);
    Ai << 1.0, 0.0, -1.0, 0.0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec::Ones(2);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

} // namespace

TEST(QpNc3SaddleExit, ASyntheticSaddleReachedWithoutADropIsLeftForALocalMinimizer) {
    const QpProblem qp = row_bounded_saddle();
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        QpOptions o;
        o.max_iter = 60;
        o.ws_algebra = alg;

        // The PREMISE, on the arm this round does not touch: with the option off the exit is
        // not tried, and the walk ends at the saddle.
        o.independent_working_set = false;
        const QpSolution off = QpEngine{o}.solve(qp);
        EXPECT_EQ(static_cast<int>(off.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(off.counters.saddle_exits + off.counters.saddle_exit_misses, 0);
        EXPECT_NEAR(off.x(0), -0.05, 1e-8);
        EXPECT_NEAR(off.x(1), 0.0, 1e-8);

        o.independent_working_set = true;
        const Walk w = observed([&] { return QpEngine{o}.solve(qp); });
        const QpSolution &s = w.solution;
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(s.counters.saddle_exits, 1);
        EXPECT_EQ(s.counters.saddle_exit_misses, 0);
        EXPECT_NEAR(std::abs(s.x(0)), 1.0, 1e-9);
        EXPECT_NEAR(s.x(1), 0.0, 1e-9);
        const double f = objective(qp, s.x);
        EXPECT_NEAR(f, s.x(0) > 0.0 ? -1.1 : -0.9, 1e-9);
        EXPECT_GT(reduced_hessian_min(qp, s), 0.0);
        expect_saddle_passes_descend(w.records, s.counters);
        // The saddle pass rode e0 to its row: an addition, recorded as the ride it is.
        bool rode = false;
        for (const Record &r : w.records) {
            if (r.saddle_exit && r.action == Action::kRideStepped) {
                rode = true;
                EXPECT_LT(r.saddle_curvature, 0.0);
            }
        }
        EXPECT_TRUE(rode);
    }
}

TEST(QpNc3SaddleExit, SteenbrbElasticQp2Border) {
    expect_saddle_left("C10_STEENBRB_qp2.txt", kBorder);
}
TEST(QpNc3SaddleExit, SteenbrbElasticQp2Refactorize) {
    expect_saddle_left("C10_STEENBRB_qp2.txt", kRefactorize);
}
TEST(QpNc3SaddleExit, SteenbrdElasticQp1Border) {
    expect_saddle_left("C10_STEENBRD_qp1.txt", kBorder);
}
TEST(QpNc3SaddleExit, SteenbrdElasticQp1Refactorize) {
    expect_saddle_left("C10_STEENBRD_qp1.txt", kRefactorize);
}
TEST(QpNc3SaddleExit, SteenbreElasticQp1Border) {
    expect_saddle_left("C10_STEENBRE_qp1.txt", kBorder);
}
TEST(QpNc3SaddleExit, SteenbreElasticQp1Refactorize) {
    expect_saddle_left("C10_STEENBRE_qp1.txt", kRefactorize);
}

TEST(QpNc3SaddleExit, AZeroCurvatureDeadEndIsNotRiddenSteenbrbRestorationLpQp3) {
    expect_no_ride("C10_STEENBRB_qp3.txt", kBorder);
    expect_no_ride("C10_STEENBRB_qp3.txt", kRefactorize);
}
TEST(QpNc3SaddleExit, ACurvatureBelowResolutionIsNotRiddenSteenbreQp2) {
    expect_no_ride("C10_STEENBRE_qp2.txt", kBorder);
}
