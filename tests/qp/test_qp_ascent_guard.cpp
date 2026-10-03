// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_ascent_guard.cpp -- M7 W2 W1: THE ASCENT GUARD (qp_engine.h section 4c), on the walk
// seam (docs/testing.md). An ordinary EQP step on a face whose reduced Hessian is indefinite
// heads for the face's stationary point, a saddle; when its direction p holds the working set
// and (Hx + g)'p > 0, the objective rises for every step in (0, 1]. The guard rides -p (descent,
// negative curvature) to its first blocker instead.
//
// The dump is the C11 census's BRITGAS elastic qp1 (tests/qp/fixtures/e2-d4/C11_BRITGAS_qp1.txt;
// `result` = the in-cell outcome at m7 21369067: kMaxIter at the 6980-minor cap, site=667). Its
// walk cycled through such steps (329 in-face ascending ordinary steps; the trace is in the W1
// report). No global oracle exists for this nonconvex QP: the assertions are the descent of every
// in-face step, the KKT battery, and a positive-definite reduced Hessian on the final face.
//
// MINOR AND FACTORIZATION COUNTS ARE PRINTED, NEVER ASSERTED.

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

Walk observed(const QpDump &d, const QpOptions &o) {
    Obs::reset();
    Obs::active = true;
    Walk w;
    w.solution = hven::test_support::solve_qp_dump(d, o);
    w.records = Obs::records;
    w.truncated = Obs::truncated;
    Obs::active = false;
    Obs::reset();
    return w;
}

/// The smallest eigenvalue of the reduced Hessian on the null space of the point's own working
/// set (equality rows, active inequality rows, every pinned column), from a dense QR of the
/// free block. NaN for a face with no free direction. (test_qp_saddle_exit.cpp's reading.)
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
        const Index c = freec[static_cast<std::size_t>(k)];
        for (Index r = 0; r < qp.me(); ++r) {
            AF(r, k) = Ae(r, c);
        }
        for (std::size_t r = 0; r < rows.size(); ++r) {
            AF(qp.me() + static_cast<Index>(r), k) = Ai(rows[r], c);
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
    const Eigen::MatrixXd Hu = Eigen::MatrixXd(qp.H);
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

/// THE DESCENT INVARIANT OF EVERY IN-FACE STEP. Consecutive records are a pass and its
/// successor, so the successor's `obj_in` is the objective the pass reached. Counted over the
/// passes that MOVED along a direction holding their working set (`step_in_face`, so the
/// row-restoring steps of an iterate off its rows are excluded): an ordinary step must not rise
/// past the rounding floor, and a guard ride must decrease strictly.
struct InFaceTally {
    Index ordinary = 0;      ///< in-face ordinary steps that moved
    Index ordinary_rose = 0; ///< ... whose objective rose past the floor
    Index rides = 0;         ///< guard rides that moved
    Index rides_not_down = 0;
    double worst_rise = 0.0;
};

InFaceTally in_face_tally(const std::vector<Record> &records) {
    InFaceTally t;
    for (std::size_t k = 0; k + 1 < records.size(); ++k) {
        const Record &r = records[k];
        const double next = records[k + 1].obj_in;
        if (!std::isfinite(r.obj_in) || !std::isfinite(next) || !(r.dx_inf > r.step_tol)) {
            continue;
        }
        const double floor = 1e-12 * std::max(1.0, std::abs(r.obj_in));
        if (r.ascent_ride) {
            ++t.rides;
            t.rides_not_down += next < r.obj_in ? 0 : 1;
        } else if (r.step_in_face == 1 &&
                   (r.action == Action::kAdd || r.action == Action::kMoveOnly)) {
            ++t.ordinary;
            t.ordinary_rose += next - r.obj_in > floor ? 1 : 0;
            t.worst_rise = std::max(t.worst_rise, next - r.obj_in);
        }
    }
    return t;
}

QpDump britgas_qp1() {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) +
                                            "/C11_BRITGAS_qp1.txt");
}

void expect_britgas_qp1(WorkingSetLinearAlgebra alg) {
    const QpDump d = britgas_qp1();
    // The premise: the in-cell walk ended at its minor cap (site 667).
    ASSERT_NE(d.result.find("status=1"), std::string::npos) << d.result;
    ASSERT_NE(d.result.find("site=667"), std::string::npos) << d.result;
    QpOptions o = d.opts;
    o.ws_algebra = alg;
    ASSERT_TRUE(o.independent_working_set) << "the dump is a `default`-label solve";
    const Walk w = observed(d, o);
    const QpSolution &s = w.solution;
    ASSERT_FALSE(w.truncated);
    const InFaceTally t = in_face_tally(w.records);
    const double f = objective(d.qp, s.x);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    const double rh = s.status == QpStatus::kOptimal ? reduced_hessian_min(d.qp, s)
                                                     : std::numeric_limits<double>::quiet_NaN();
    std::printf("[ w1 ] C11_BRITGAS_qp1 %s status=%d minors=%ld facts=%ld f=%.12g redH_min=%.6g "
                "ascent_rides=%ld in_face_ordinary=%ld rose=%ld worst_rise=%.3e rides=%ld "
                "rides_not_down=%ld eq=%.2e stat=%.2e comp=%.2e\n",
                alg_name(alg), static_cast<int>(s.status),
                static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations), f, rh,
                static_cast<long>(s.counters.ascent_rides), static_cast<long>(t.ordinary),
                static_cast<long>(t.ordinary_rose), t.worst_rise, static_cast<long>(t.rides),
                static_cast<long>(t.rides_not_down), k.eq, k.stationarity, k.complementarity);

    EXPECT_EQ(t.ordinary_rose, 0) << "an in-face ordinary step ascended, by up to " << t.worst_rise;
    EXPECT_EQ(t.rides_not_down, 0) << "a guard ride that moved did not descend";
    EXPECT_GE(s.counters.ascent_rides, 1);
    Index flagged = 0;
    for (const Record &r : w.records) {
        flagged += r.ascent_ride ? 1 : 0;
    }
    EXPECT_EQ(flagged, s.counters.ascent_rides);
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_LE(k.eq, 1e-8);
    EXPECT_LE(k.ineq, 1e-8);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-8);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-8);
    // NaN is a VERTEX (no free direction on the final face), where second order is vacuous.
    EXPECT_TRUE(std::isnan(rh) || rh > 0.0)
        << "the final face is not second-order: reduced Hessian min " << rh;
}

} // namespace

TEST(QpW1AscentGuard, BritgasElasticQp1Border) { expect_britgas_qp1(kBorder); }

TEST(QpW1AscentGuard, BritgasElasticQp1Refactorize) { expect_britgas_qp1(kRefactorize); }

// THE OPTION-OFF ARM IS UNTOUCHED: the guard is part of the independent working set's walk.
TEST(QpW1AscentGuard, TheOptionOffWalkTakesNoGuardRide) {
    const QpDump d = britgas_qp1();
    for (const auto alg : {kBorder, kRefactorize}) {
        QpOptions o = d.opts;
        o.ws_algebra = alg;
        o.independent_working_set = false;
        const QpSolution s = hven::test_support::solve_qp_dump(d, o);
        std::printf("[ w1 ] option-off %s status=%d minors=%ld\n", alg_name(alg),
                    static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters));
        EXPECT_EQ(s.counters.ascent_rides, 0);
    }
}
