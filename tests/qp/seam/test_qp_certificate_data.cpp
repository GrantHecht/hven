// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_certificate_data.cpp -- M8 QPC: THE CERTIFICATE'S DATA HALF (walk_engine.h, section 5).
//
// Every row test the walk certifies on excuses a row by min(dual_mu*|lambda|,
// kInfeasibilityAbsorbTol * row_scale) on top of feas_tol * row_scale. With |lambda| ~ 1e5 the
// excuse is the 1e-6 cap: QC8 DALLASM qp7 under the P-1a prototypes was certified kOptimal at
// f = -92397.2848 against the optimum -92397.2698 with one equality row 72.9x feas_tol off and the
// complementarity product 7e6x feas_tol, because the final working set is itself inconsistent at
// 7.3e-8. The unregularized EQP of that working set reproduces the false point, so no face
// re-solve catches it. The data half reads the exported point from the QP's own data -- rows
// <= 10 feas_tol relative to each row's scale, each row's residual x multiplier product over its
// own LOCAL dual scale <= 100 feas_tol, free-block stationarity beyond rounding <= 10 opt_tol, no
// multiplier off the working set, finite vectors -- and refuses the certificate (kNumericalError)
// otherwise.
//
// The false point is not reachable through a natural walk at m8 (the prototypes that produced it
// are not in the tree), so the fixtures present it through the certificate seam
// (certificate_seam.h) on the production statements that follow: a point off one row by less than
// the footprint excuse and more than the data half allows. Before QPC each presented point below
// marked "refused" came back kOptimal.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/qp/walk/walk_engine.h>

#include "qp/support/qp_dump.h"

using namespace hven;
using namespace hven::detail;
using hven::Index;
using hven::Vec;
using hven::test_support::QpDump;

namespace {

using Cert = hven::detail::testing::QpCertificateSeam;
using Obs = hven::detail::testing::QpWalkObserver;

struct DataSeamGuard {
    DataSeamGuard() {
        Obs::reset();
        Cert::reset();
    }
    ~DataSeamGuard() {
        Cert::reset();
        Obs::reset();
    }
};

constexpr QpWorkingSetAlgebra kAlgebras[] = {QpWorkingSetAlgebra::kSchurBorder,
                                             QpWorkingSetAlgebra::kRefactorize};

QpOptions opts_for(QpWorkingSetAlgebra alg) {
    QpOptions o;
    o.working_set_algebra = alg;
    return o;
}

/// min 1/2|x|^2 - c (x0 + x1)  s.t.  x0 + x1 = 1. Optimum x = (0.5, 0.5), lambda_e = c - 0.5
/// (stationarity x + g + lambda a = 0 with g = (-c, -c)); row scale max(1, |b|, |a|_1 |x|inf) = 1.
QpProblem line_qp(double c) {
    QpProblem qp;
    const Index n = 2;
    qp.H = Eigen::MatrixXd::Identity(n, n).sparseView();
    qp.g = Vec::Constant(n, -c);
    Eigen::MatrixXd A(1, n);
    A << 1.0, 1.0;
    qp.Ae = A.sparseView();
    qp.be = Vec::Constant(1, 1.0);
    qp.Ai.resize(0, n);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(n, -10.0);
    qp.upper = Vec::Constant(n, 10.0);
    return qp;
}

/// Presents x = (0.5 + d, 0.5), z = 0 to every certificate read of one solve of line_qp(c): the
/// row residual is d (row scale 1), the product d * |c - 0.5|.
QpSolution present_line(double c, double d, QpWorkingSetAlgebra alg) {
    Cert::reset();
    Cert::x = Vec(2);
    Cert::x << 0.5 + d, 0.5;
    Cert::z = Vec::Zero(2);
    Cert::armed = true;
    QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
    QpSolution s = engine.solve(line_qp(c));
    Cert::armed = false;
    return s;
}

/// min 1/2|x|^2 + g'x on the box, x0 at its LOWER bound 0 with z0 = +k (g0 = k), x1 free at 0.25.
QpProblem box_qp(double k) {
    QpProblem qp;
    const Index n = 2;
    qp.H = Eigen::MatrixXd::Identity(n, n).sparseView();
    qp.g = Vec(n);
    qp.g << k, -0.25;
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    qp.Ai.resize(0, n);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(n, 0.0);
    qp.upper = Vec::Constant(n, 1.0);
    return qp;
}

void expect_refused(const QpSolution &s) {
    ASSERT_GE(Cert::presentations, 1) << "the certificate never read the presented point";
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
    EXPECT_EQ(s.counters.certificate_gate_refusals, 1);
    EXPECT_EQ(s.counters.certificate_bound_refusals, 0); // the data half refused, no other half
    EXPECT_EQ(s.counters.certificate_sign_refusals, 0);
    EXPECT_EQ(s.lambda_e.lpNorm<Eigen::Infinity>(), 0.0); // a refusal exports no prices
    EXPECT_EQ(s.z.lpNorm<Eigen::Infinity>(), 0.0);
}

void expect_certified(const QpSolution &s) {
    ASSERT_GE(Cert::presentations, 1) << "the certificate never read the presented point";
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_EQ(s.counters.certificate_gate_refusals, 0);
}

// The data half recomputed here, independently of detail::terminal_kkt: the worst relative row
// (equalities, inequalities one-sided, bounds against the dump's box with the trust region) and
// the worst |residual|/row_scale * |multiplier| over equalities, active rows and pinned bounds.
struct DataReading {
    double rows = 0.0;
    double product = 0.0;
};

DataReading read_data(const QpDump &d, const QpSolution &s) {
    const QpProblem &qp = d.qp;
    DataReading r;
    const double xmag = s.x.lpNorm<Eigen::Infinity>();
    const Eigen::MatrixXd Ae(qp.Ae);
    const Eigen::MatrixXd Ai(qp.Ai);
    for (Index j = 0; j < qp.me(); ++j) {
        const double rs = std::max({1.0, std::abs(qp.be(j)), Ae.row(j).cwiseAbs().sum() * xmag});
        const double rel = std::abs(Ae.row(j).dot(s.x) - qp.be(j)) / rs;
        r.rows = std::max(r.rows, rel);
        r.product = std::max(r.product, rel * std::abs(s.lambda_e(j)));
    }
    for (Index j = 0; j < qp.mi(); ++j) {
        const double rs = std::max({1.0, std::abs(qp.bi(j)), Ai.row(j).cwiseAbs().sum() * xmag});
        const double res = Ai.row(j).dot(s.x) - qp.bi(j);
        r.rows = std::max(r.rows, std::max(0.0, res) / rs);
        if (s.ineq_active[static_cast<std::size_t>(j)]) {
            r.product = std::max(r.product, std::abs(res) / rs * std::abs(s.lambda_i(j)));
        }
    }
    for (Index i = 0; i < qp.n(); ++i) {
        const double below = qp.lower(i) - s.x(i);
        const double above = s.x(i) - qp.upper(i);
        const double bound = below >= above ? qp.lower(i) : qp.upper(i);
        r.rows = std::max(r.rows, std::max(below, above) / std::max({1.0, std::abs(bound), xmag}));
    }
    return r;
}

QpSolution solve_dump(const QpDump &d, QpWorkingSetAlgebra alg) {
    QpOptions o = d.opts;
    o.working_set_algebra = alg;
    return hven::test_support::solve_qp_dump(d, o);
}

QpDump load(const char *file) {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + file);
}

constexpr double kQp7Oracle = -92397.26984847; // the pinned optimum (test_qp_implied_blocker.cpp)

} // namespace

// The rows' bound: 10 feas_tol at row scale 1. The scaled product (d / 2 over the dual scale 1) is
// inside 100 feas_tol for every d here, so the rows decide; the footprint excuse at R is
// max(feas_tol, min(1e-8 * 0.5, 1e-6)) = 5e-9, so R certified d = 1.01e-8 kOptimal through the gray
// contract (a gray-zone exit).
TEST(QpCertificateData, ARowPastTenFeasTolIsRefusedAndOneInsideIsCertified) {
    DataSeamGuard guard;
    const double ftol = QpOptions{}.feasibility_tol;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        expect_refused(present_line(1.0, 10.1 * ftol, alg));
        expect_certified(present_line(1.0, 9.9 * ftol, alg));
        expect_refused(present_line(1.0, -10.1 * ftol, alg)); // an equality row is two-sided
    }
}

// THE PRODUCT IS READ AGAINST THE ROW'S LOCAL DUAL SCALE (settler C567). On line_qp(1000),
// lambda_e = 999.5 and the row's columns carry |g_i| = 1000, |(Hx)_i| = 0.5, |(Ae'lambda_e)_i| =
// 999.5, joined by |lambda_e| ||a||inf = 999.5: local scale 1000. A row 5 feas_tol off reads an
// unscaled product of 5e-6 (50x past 100 feas_tol: what the first QPC build refused) and a scaled
// one of 5e-9: certified, as the same presentation is at c = 1 -- scaling the objective by 1000
// scales every multiplier and every stationarity term by 1000 and must not change the verdict.
TEST(QpCertificateData, TheProductIsReadAgainstTheRowsLocalScale) {
    DataSeamGuard guard;
    const double ftol = QpOptions{}.feasibility_tol;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        expect_certified(present_line(1000.0, 5.0 * ftol, alg));
        expect_certified(present_line(1.0, 5.0 * ftol, alg));
        expect_refused(present_line(1000.0, 10.1 * ftol, alg)); // the rows still decide
    }
}

/// min 1/2|x|^2 - c (x0 + x1)  s.t.  x0 + x1 <= 1 (c > 0.5: binding). Optimum (0.5, 0.5),
/// lambda_i = c - 0.5.
QpProblem ineq_line_qp(double c) {
    QpProblem qp = line_qp(c);
    qp.Ai = qp.Ae;
    qp.bi = qp.be;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    return qp;
}

// A WORKING ROW HELD SLACK. x = (0.5 - d, 0.5) is d INSIDE the binding row x0 + x1 <= 1, so the
// rows read 0 (a one-sided row is not violated by slack) and the point is a KKT point of the row
// shifted by d, not of the QP: lambda_i = 999.5 prices a row that is not active. The scaled product
// is d * 999.5 / 1000 (local scale 1000); free-block stationarity d / 1000 (0.11 opt_tol at most
// here). Bound 100 feas_tol: d = 110 feas_tol refused, d = 90 feas_tol certified. (The shape of the
// HS15 and B1Gate refusals.)
TEST(QpCertificateData, AWorkingRowSlackPastAHundredFeasTolIsRefused) {
    DataSeamGuard guard;
    const double ftol = QpOptions{}.feasibility_tol;
    const auto present = [](double d, QpWorkingSetAlgebra alg) {
        Cert::reset();
        Cert::x = Vec(2);
        Cert::x << 0.5 - d, 0.5;
        Cert::z = Vec::Zero(2);
        Cert::armed = true;
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        QpSolution s = engine.solve(ineq_line_qp(1000.0));
        Cert::armed = false;
        return s;
    };
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const QpSolution far = present(110.0 * ftol, alg);
        ASSERT_GE(Cert::presentations, 1);
        EXPECT_EQ(static_cast<int>(far.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(far.counters.certificate_gate_refusals, 1);
        const QpSolution near = present(90.0 * ftol, alg);
        expect_certified(near);
        ASSERT_EQ(near.lambda_i.size(), 1);
        EXPECT_NEAR(near.lambda_i(0), 999.5, 1e-6);
    }
}

// A NaN in the exported point is no certificate. R refused it already (dev/R-tdd.txt), through the
// paths the gray-row reading of a NaN row takes before the data half is reached, so this pins the
// status only; the data half's own NaN rule (certificate_data_half_holds) is pinned directly below.
TEST(QpCertificateData, ANanPointIsRefused) {
    DataSeamGuard guard;
    const QpSolution s = present_line(1.0, std::numeric_limits<double>::quiet_NaN(),
                                      QpWorkingSetAlgebra::kRefactorize);
    ASSERT_GE(Cert::presentations, 1);
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
}

TEST(QpCertificateData, TheVerdictRefusesNanAndReadsEveryBoundInclusively) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double ftol = 1e-9;
    const double otol = 1e-8; // distinct from ftol, so a disjunct read against the wrong one fails
    const auto reading = [](double rows, double product, double stat, double off = 0.0,
                            bool finite = true, double slack = 0.0) {
        CertificateReading r;
        r.rows = rows;
        r.product = product;
        r.stationarity_beyond_rounding = stat;
        r.off_working = off;
        r.finite = finite;
        r.slack = slack;
        return r;
    };
    EXPECT_TRUE(certificate_data_half_holds(
        reading(10.0 * ftol, 100.0 * ftol, 10.0 * otol, 0.0, true, 100.0 * ftol), ftol, otol));
    // The slack disjunct (settler C568): 100 feas_tol, inclusive; NaN refuses.
    EXPECT_FALSE(
        certificate_data_half_holds(reading(0.0, 0.0, 0.0, 0.0, true, 100.1 * ftol), ftol, otol));
    EXPECT_FALSE(certificate_data_half_holds(reading(0.0, 0.0, 0.0, 0.0, true, nan), ftol, otol));
    EXPECT_FALSE(certificate_data_half_holds(reading(nan, 0.0, 0.0), ftol, otol));
    EXPECT_FALSE(certificate_data_half_holds(reading(0.0, nan, 0.0), ftol, otol));
    EXPECT_FALSE(certificate_data_half_holds(reading(0.0, 0.0, nan), ftol, otol));
    EXPECT_FALSE(certificate_data_half_holds(reading(0.0, 0.0, 0.0, nan), ftol, otol));
    EXPECT_FALSE(certificate_data_half_holds(reading(10.01 * ftol, 0.0, 0.0), ftol, otol));
    EXPECT_FALSE(certificate_data_half_holds(reading(0.0, 100.1 * ftol, 0.0), ftol, otol));
    EXPECT_FALSE(certificate_data_half_holds(reading(0.0, 0.0, 10.01 * otol), ftol, otol));
    EXPECT_TRUE(certificate_data_half_holds(reading(0.0, 0.0, 9.99 * otol), ftol, otol));
    // Off the working set: exactly zero, however small.
    EXPECT_FALSE(certificate_data_half_holds(reading(0.0, 0.0, 0.0, 1e-300), ftol, otol));
    // A non-finite exported vector refuses on its own, every reading clean.
    EXPECT_FALSE(certificate_data_half_holds(reading(0.0, 0.0, 0.0, 0.0, false), ftol, otol));
    EXPECT_EQ(kCertificateGateRowFactor, 10.0);
    EXPECT_EQ(kCertificateGateComplFactor, 100.0);
    EXPECT_EQ(kCertificateGateStationarityFactor, 10.0);
    EXPECT_EQ(kCertificateGateSlackFactor, 100.0);
    EXPECT_EQ(kCertificateGateRoundingZeroFactor, 100.0);
}

// STATIONARITY, re-read on the EXPORTED point (settler C553 Q3). x = (0.5 + d, 0.5 - d) stays ON
// the row x0 + x1 = 1 (rows 0, product 0), so only stationarity moves: with the walk's
// lambda_e = c - 0.5 = 0.5 the free-block residual x + g + lambda_e a is (d, -d) against the scale
// max(1, |Hx|, |g|, |A'lambda|) = 1, i.e. d relative. The KKT-point test read the walk's own
// point before the seam presents this one, so the data half is the only reader of it. Bound:
// 10 opt_tol.
TEST(QpCertificateData, AnOffStationaryExportedPointIsRefused) {
    DataSeamGuard guard;
    const double otol = QpOptions{}.optimality_tol;
    const auto present = [](double d, QpWorkingSetAlgebra alg) {
        Cert::reset();
        Cert::x = Vec(2);
        Cert::x << 0.5 + d, 0.5 - d;
        Cert::z = Vec::Zero(2);
        Cert::armed = true;
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        QpSolution s = engine.solve(line_qp(1.0));
        Cert::armed = false;
        return s;
    };
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const QpSolution far = present(10.1 * otol, alg);
        expect_refused(far);
        const QpSolution near = present(9.9 * otol, alg);
        expect_certified(near);
        // The certified answer is the presented point, and its residual is what the gate read.
        ASSERT_EQ(near.x.size(), 2);
        EXPECT_NEAR(near.x(0) + near.x(1), 1.0, 1e-15);
        EXPECT_NEAR(std::abs(near.x(0) + near.lambda_e(0) - 1.0), 9.9 * otol, 1e-15);
    }
}

// A BOUND, read the same way: x0 pinned at its lower bound 0 with z0 = 5e3. The bound half
// excuses min(dual_mu * 5e3, 1e-6) = 1e-6 (times its 10x margin), so x0 = -1e-7 was certified at R;
// the data half reads rows 1e-7 = 100 feas_tol and the product 5e-4.
TEST(QpCertificateData, ABoundInsideTheFootprintExcuseButPastTheDataHalfIsRefused) {
    DataSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        Cert::reset();
        Cert::x = Vec(2);
        Cert::x << -1e-7, 0.25;
        Cert::z = Vec(2);
        Cert::z << 5e3, 0.0;
        Cert::armed = true;
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        const QpSolution s = engine.solve(box_qp(5e3));
        Cert::armed = false;
        expect_refused(s);
    }
}

// Unarmed, the small QPs certify their optima.
TEST(QpCertificateData, TheSmallQpsCertifyUnarmed) {
    DataSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        QpEngine e1{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        const QpSolution a = e1.solve(line_qp(1000.0));
        ASSERT_EQ(static_cast<int>(a.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_NEAR(a.lambda_e(0), 999.5, 1e-6);
        QpEngine e2{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        const QpSolution b = e2.solve(box_qp(5e3));
        ASSERT_EQ(static_cast<int>(b.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(Cert::presentations, 0);
    }
}

// THE NATURAL SHAPE: QC8 DALLASM qp7 (the in-cell elastic rung of major 2). Its optimum carries
// equality multipliers up to ~2e5. The point presented is the walk's own optimum moved off ONE
// equality row -- the one with the largest |lambda_e| -- by 72.9 feas_tol * row_scale and off no
// other working row (the minimum-norm move on the free columns), the signature the P-1a
// prototypes' false certificate read. R certified it kOptimal (rows within the 1e-6 excuse).
TEST(QpCertificateData, Qc8DallasmQp7TheP1aFalseCertificateShapeIsRefused) {
    DataSeamGuard guard;
    const QpDump d = load("QC8_DALLASM_qp7.txt");
    const QpProblem &qp = d.qp;
    const double ftol = d.opts.feasibility_tol;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        Cert::reset();
        const QpSolution s = solve_dump(d, alg);
        ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_LE(std::abs(hven::test_support::qp_dump_objective(d, s.x) - kQp7Oracle),
                  1e-9 * std::abs(kQp7Oracle));
        Index jstar = 0;
        for (Index j = 1; j < qp.me(); ++j) {
            if (std::abs(s.lambda_e(j)) > std::abs(s.lambda_e(jstar))) {
                jstar = j;
            }
        }
        ASSERT_GT(std::abs(s.lambda_e(jstar)), 1e4) << "the premise: a large multiplier";
        // The working rows (equalities, then the active inequalities) on the free columns.
        std::vector<Index> free_cols;
        for (Index i = 0; i < qp.n(); ++i) {
            const auto si = static_cast<std::size_t>(i);
            if (s.bound_state[si] == BoundState::kFree && !s.tr_active[si]) {
                free_cols.push_back(i);
            }
        }
        std::vector<Index> act;
        for (Index j = 0; j < qp.mi(); ++j) {
            if (s.ineq_active[static_cast<std::size_t>(j)]) {
                act.push_back(j);
            }
        }
        const Eigen::MatrixXd Ae(qp.Ae);
        const Eigen::MatrixXd Ai(qp.Ai);
        const Index rows = qp.me() + static_cast<Index>(act.size());
        Eigen::MatrixXd A(rows, static_cast<Index>(free_cols.size()));
        for (Index c = 0; c < A.cols(); ++c) {
            const Index i = free_cols[static_cast<std::size_t>(c)];
            A.block(0, c, qp.me(), 1) = Ae.col(i);
            for (std::size_t k = 0; k < act.size(); ++k) {
                A(qp.me() + static_cast<Index>(k), c) = Ai(act[k], i);
            }
        }
        const double xmag = s.x.lpNorm<Eigen::Infinity>();
        const double rs =
            std::max({1.0, std::abs(qp.be(jstar)), Ae.row(jstar).cwiseAbs().sum() * xmag});
        Vec e = Vec::Zero(rows);
        e(jstar) = 72.9 * ftol * rs;
        const Eigen::CompleteOrthogonalDecomposition<Eigen::MatrixXd> cod(A);
        const Vec move = cod.solve(e);
        ASSERT_LE((A * move - e).lpNorm<Eigen::Infinity>(), 1e-3 * e(jstar)) << "row not isolated";
        Vec xp = s.x;
        for (Index c = 0; c < A.cols(); ++c) {
            xp(free_cols[static_cast<std::size_t>(c)]) += move(c);
        }
        std::printf("[ qpc-data ] qp7 %s row %ld lambda %.6g rs %.6g |move| %.3g\n",
                    alg == QpWorkingSetAlgebra::kSchurBorder ? "border" : "refactorize",
                    static_cast<long>(jstar), s.lambda_e(jstar), rs,
                    move.lpNorm<Eigen::Infinity>());
        Cert::reset();
        Cert::x = xp;
        Cert::z = s.z;
        Cert::armed = true;
        const QpSolution t = solve_dump(d, alg);
        Cert::armed = false;
        expect_refused(t);
    }
}

// THE NATURAL FIXTURES, unarmed: every kOptimal exit of the QC8 / SDL DALLASM dumps, both
// algebras, meets the data half as recomputed here (independently of terminal_kkt). Measured at
// m8 (P-1a deciders, D-4): no in-repo fixture exit exceeds either bound. The product is read
// UNSCALED here (stricter than the gate's dual-scaled reading).
TEST(QpCertificateData, NaturalDallasmFixturesCertifyInsideTheDataHalf) {
    DataSeamGuard guard;
    const char *files[] = {"QC8_DALLASM_qp1.txt",     "QC8_DALLASM_qp7.txt",
                           "QC8_DALLASM_entry6.txt",  "DALLASM_def-qp0.txt",
                           "SDL_DALLASM_QN_qp10.txt", "SDL6_DALLASM_QN_qp0.txt"};
    Index optimal = 0;
    for (const char *file : files) {
        const QpDump d = load(file);
        for (const auto alg : kAlgebras) {
            const QpSolution s = solve_dump(d, alg);
            if (s.status != QpStatus::kOptimal) {
                continue;
            }
            ++optimal;
            const DataReading r = read_data(d, s);
            const double ftol = d.opts.feasibility_tol;
            std::printf("[ qpc-data ] %s %s rows/ftol=%.3g product/ftol=%.3g\n", file,
                        alg == QpWorkingSetAlgebra::kSchurBorder ? "border" : "refactorize",
                        r.rows / ftol, r.product / ftol);
            EXPECT_LE(r.rows, 10.0 * ftol) << file;
            EXPECT_LE(r.product, 100.0 * ftol) << file;
        }
    }
    EXPECT_GE(optimal, 6); // qp1, qp7, entry6 on both algebras at least
}

// ---------------------------------------------------------------------------------------------
// THE ROW-LOCAL SCALE (settler C567; the pre-freeze review's F-SLACK). A GLOBAL dual scale lets
// a block that does not touch a row switch the row's product off: below, a decoupled variable x2
// pinned at its lower bound under z2 = 1e9 made the global scale 1e9, so a working row held 0.1
// SLACK under lambda_i = 999.5 read 0.1 * 999.5 / 1e9 = 1e-7 = 100 feas_tol and certified
// x = (0.4, 0.5) against the optimum (0.5, 0.5): an objective error of ~100 on |f*| ~ 1000. Its
// rows read 0 (the slack is on the feasible side) and its stationarity 0.1 / 1e9 = 1e-10.

/// ineq_line_qp(c) plus x2 in [0, 10] with g2 = big: x2 pins at 0 under z2 = big, decoupled.
QpProblem decoupled_ineq_qp(double c, double big) {
    QpProblem qp;
    const Index n = 3;
    qp.H = Eigen::MatrixXd::Identity(n, n).sparseView();
    qp.g = Vec(n);
    qp.g << -c, -c, big;
    Eigen::MatrixXd A(1, n);
    A << 1.0, 1.0, 0.0;
    qp.Ai = A.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    qp.lower = Vec(n);
    qp.lower << -10.0, -10.0, 0.0;
    qp.upper = Vec::Constant(n, 10.0);
    return qp;
}

TEST(QpCertificateData, ADecoupledLargeDualCannotHideASlackWorkingRow) {
    DataSeamGuard guard;
    const double ftol = QpOptions{}.feasibility_tol;
    const double big = 1e9;
    // THE PREMISE: the global reading passed it.
    ASSERT_LE(0.1 * 999.5 / big, 100.0 * ftol);
    const auto present = [&](double d, QpWorkingSetAlgebra alg) {
        Cert::reset();
        Cert::x = Vec(3);
        Cert::x << 0.5 - d, 0.5, 0.0;
        Cert::z = Vec(3);
        Cert::z << 0.0, 0.0, big;
        Cert::armed = true;
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        QpSolution s = engine.solve(decoupled_ineq_qp(1000.0, big));
        Cert::armed = false;
        return s;
    };
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        const QpSolution natural = engine.solve(decoupled_ineq_qp(1000.0, big));
        ASSERT_EQ(static_cast<int>(natural.status), static_cast<int>(QpStatus::kOptimal));
        ASSERT_EQ(natural.bound_state[2], BoundState::kAtLower);
        ASSERT_NEAR(natural.lambda_i(0), 999.5, 1e-6);
        ASSERT_NEAR(natural.z(2), big, 1e-6 * big);
        const QpSolution far = present(0.1, alg);
        ASSERT_GE(Cert::presentations, 1);
        EXPECT_EQ(static_cast<int>(far.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(far.counters.certificate_gate_refusals, 1);
        // The row-local bound, as on the coupled line: 110 feas_tol refused, 90 certified.
        const QpSolution past = present(110.0 * ftol, alg);
        EXPECT_EQ(static_cast<int>(past.status), static_cast<int>(QpStatus::kNumericalError));
        expect_certified(present(90.0 * ftol, alg));
    }
}

/// box_qp(k) plus x2 in [0, 1] with g2 = big: x0 pins under z0 = k, x2 under z2 = big.
QpProblem decoupled_box_qp(double k, double big) {
    QpProblem qp = box_qp(k);
    const Index n = 3;
    qp.H = Eigen::MatrixXd::Identity(n, n).sparseView();
    qp.g.conservativeResize(n);
    qp.g(2) = big;
    qp.Ae.resize(0, n);
    qp.Ai.resize(0, n);
    qp.lower = Vec::Constant(n, 0.0);
    qp.upper = Vec::Constant(n, 1.0);
    return qp;
}

// THE PINNED-BOUND ANALOGUE: x0 pinned at its lower bound 0 under z0 = 1000 but presented at
// x0 = d INSIDE its box (rows 0): a KKT point of the bound shifted by d. Local scale of bound 0 =
// max(|g0| = 1000, |(Hx)_0| = d, |z0| = 1000) = 1000, so the reading is d * 1000 / 1000 = d;
// globally it was d * 1000 / 1e9.
TEST(QpCertificateData, ADecoupledLargeDualCannotHideASlackPinnedBound) {
    DataSeamGuard guard;
    const double ftol = QpOptions{}.feasibility_tol;
    const double big = 1e9;
    ASSERT_LE(0.1 * 1000.0 / big, 100.0 * ftol); // the premise: the global reading passed it
    const auto present = [&](double d, QpWorkingSetAlgebra alg) {
        Cert::reset();
        Cert::x = Vec(3);
        Cert::x << d, 0.25, 0.0;
        Cert::z = Vec(3);
        Cert::z << 1000.0, 0.0, big;
        Cert::armed = true;
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        QpSolution s = engine.solve(decoupled_box_qp(1000.0, big));
        Cert::armed = false;
        return s;
    };
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        const QpSolution natural = engine.solve(decoupled_box_qp(1000.0, big));
        ASSERT_EQ(static_cast<int>(natural.status), static_cast<int>(QpStatus::kOptimal));
        ASSERT_EQ(natural.bound_state[0], BoundState::kAtLower);
        ASSERT_EQ(natural.bound_state[2], BoundState::kAtLower);
        expect_refused(present(0.1, alg));
        expect_refused(present(110.0 * ftol, alg));
        expect_certified(present(90.0 * ftol, alg));
    }
}

// THE READING ITSELF, computed directly on the F-SLACK point (no walk): the product is the
// row-local quotient, to rounding, and off the working set nothing is read.
TEST(QpCertificateData, TheReadingDividesEachProductByItsOwnColumnsScale) {
    const QpProblem qp = decoupled_ineq_qp(1000.0, 1e9);
    WorkingSet ws(3, 1);
    ws.add_ineq(0);
    ws.bound_state()[2] = BoundState::kAtLower;
    Vec x(3);
    x << 0.4, 0.5, 0.0;
    const Vec le(0);
    const Vec li = Vec::Constant(1, 999.5);
    Vec z(3);
    z << 0.0, 0.0, 1e9;
    const CertificateReading r = certificate_reading(qp, ws, x, le, li, z);
    EXPECT_TRUE(r.finite);
    EXPECT_EQ(r.rows, 0.0);
    EXPECT_EQ(r.off_working, 0.0);
    EXPECT_NEAR(r.product, 0.1 * 999.5 / 1000.0, 1e-15);
    // The bound x2 is exactly at its bound: no product. The stationarity is global, 0.1 / 1e9.
    EXPECT_NEAR(r.stationarity, 0.1 / 1e9, 1e-20);
    // The same point with the decoupled block removed reads the same product.
    const QpProblem coupled = ineq_line_qp(1000.0);
    WorkingSet ws2(2, 1);
    ws2.add_ineq(0);
    Vec x2(2);
    x2 << 0.4, 0.5;
    const CertificateReading r2 = certificate_reading(coupled, ws2, x2, le, li, Vec::Zero(2));
    EXPECT_EQ(r2.product, r.product);
}

// NaN-STICKY (pre-freeze review S8): a NaN in the QP's own DATA, with every exported vector
// finite, reaches the verdict. A plain running max drops it (std::max(acc, NaN) == acc).
TEST(QpCertificateData, TheReadingKeepsANanInTheProblemData) {
    QpProblem qp = ineq_line_qp(1000.0);
    WorkingSet ws(2, 1);
    ws.add_ineq(0);
    const Vec x = Vec::Constant(2, 0.5);
    const Vec le(0);
    const Vec li = Vec::Constant(1, 999.5);
    const Vec z = Vec::Zero(2);
    ASSERT_TRUE(certificate_data_half_holds(certificate_reading(qp, ws, x, le, li, z), 1e-9, 1e-9));
    qp.bi(0) = std::numeric_limits<double>::quiet_NaN();
    const CertificateReading r = certificate_reading(qp, ws, x, le, li, z);
    EXPECT_TRUE(r.finite);
    EXPECT_TRUE(std::isnan(r.rows));
    EXPECT_TRUE(std::isnan(r.product));
    EXPECT_FALSE(certificate_data_half_holds(r, 1e-9, 1e-9));
    QpProblem qb = ineq_line_qp(1000.0);
    qb.upper(1) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(std::isnan(certificate_reading(qb, ws, x, le, li, z).rows));
    EXPECT_NEAR(nan_sticky_max(1.0, 2.0), 2.0, 0.0);
    EXPECT_TRUE(std::isnan(nan_sticky_max(std::numeric_limits<double>::quiet_NaN(), 2.0)));
    EXPECT_TRUE(std::isnan(nan_sticky_max(2.0, std::numeric_limits<double>::quiet_NaN())));
}

/// ineq_line_qp(c) plus a second, slack row x0 - x1 <= 5 that never enters the working set.
QpProblem two_row_qp(double c) {
    QpProblem qp = ineq_line_qp(c);
    Eigen::MatrixXd A(2, 2);
    A << 1.0, 1.0, 1.0, -1.0;
    qp.Ai = A.sparseView();
    qp.bi = Vec(2);
    qp.bi << 1.0, 5.0;
    return qp;
}

// OFF THE WORKING SET (pre-freeze review S2): a multiplier on a non-working row, or on a free
// variable, is exported unread by every other reading, so the data half refuses any nonzero one.
TEST(QpCertificateData, AMultiplierOffTheWorkingSetIsRefused) {
    DataSeamGuard guard;
    const auto present = [](double off_row, double off_z, QpWorkingSetAlgebra alg) {
        Cert::reset();
        Cert::x = Vec::Constant(2, 0.5);
        Cert::z = Vec(2);
        Cert::z << 0.0, off_z;
        Cert::lambda_i = Vec(2);
        Cert::lambda_i << 999.5, off_row;
        Cert::armed = true;
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        QpSolution s = engine.solve(two_row_qp(1000.0));
        Cert::armed = false;
        return s;
    };
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        const QpSolution natural = engine.solve(two_row_qp(1000.0));
        ASSERT_EQ(static_cast<int>(natural.status), static_cast<int>(QpStatus::kOptimal));
        ASSERT_TRUE(natural.ineq_active[0]);
        ASSERT_FALSE(natural.ineq_active[1]);
        EXPECT_EQ(natural.lambda_i(1), 0.0);
        expect_certified(present(0.0, 0.0, alg));
        const QpSolution row = present(1e-12, 0.0, alg);
        ASSERT_GE(Cert::presentations, 1);
        EXPECT_EQ(static_cast<int>(row.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(row.counters.certificate_gate_refusals, 1);
        const QpSolution zed = present(0.0, 1e-12, alg);
        EXPECT_EQ(static_cast<int>(zed.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(zed.counters.certificate_gate_refusals, 1);
    }
}

// A NaN EXPORTED MULTIPLIER, end to end through the data half (settler C566: a zero-false-
// certificate requirement). line_qp(1)'s optimum presented with lambda_e = NaN: every other
// certificate reading (the bound half, the sign half, the gray-row contract) lets it through to
// the data half, which refuses it -- through the finiteness guard and through the NaN-sticky
// product and stationarity, either of which alone refuses it.
TEST(QpCertificateData, ANanExportedMultiplierIsRefusedByTheDataHalf) {
    DataSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        Cert::reset();
        Cert::x = Vec::Constant(2, 0.5);
        Cert::z = Vec::Zero(2);
        Cert::lambda_e = Vec::Constant(1, std::numeric_limits<double>::quiet_NaN());
        Cert::armed = true;
        QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
        const QpSolution s = engine.solve(line_qp(1.0));
        Cert::armed = false;
        ASSERT_GE(Cert::presentations, 1);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(s.counters.certificate_gate_refusals, 1)
            << "a NaN multiplier must reach the data half and be refused there";
        EXPECT_EQ(s.counters.certificate_bound_refusals, 0);
        EXPECT_EQ(s.counters.certificate_sign_refusals, 0);
    }
}

// THE ROUNDING FLOOR is c_i u m_i with u = eps / 2, the unit roundoff (pre-freeze review S1).
// One free column, H = 0, g = 1, one equality row a = 1 priced at lambda_e = -1 + 3 eps: the
// residual g + a lambda_e is EXACTLY 3 eps (Sterbenz), from c = 2 terms of magnitude m = 2 - 3 eps
// over the scale 1. Beyond a floor of 2 u m = eps (2 - 3 eps) it reads ~eps; beyond 2 eps m it
// would read 0.
TEST(QpCertificateData, TheRoundingFloorIsTheUnitRoundoff) {
    constexpr double eps = std::numeric_limits<double>::epsilon();
    QpProblem qp;
    qp.H.resize(1, 1);
    qp.g = Vec::Constant(1, 1.0);
    Eigen::MatrixXd A(1, 1);
    A << 1.0;
    qp.Ae = A.sparseView();
    qp.be = Vec::Constant(1, 0.25);
    qp.Ai.resize(0, 1);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(1, -10.0);
    qp.upper = Vec::Constant(1, 10.0);
    const WorkingSet ws(1, 0);
    const Vec x = Vec::Constant(1, 0.25);
    const Vec le = Vec::Constant(1, -1.0 + 3.0 * eps);
    const CertificateReading r = certificate_reading(qp, ws, x, le, Vec(0), Vec::Zero(1));
    ASSERT_EQ(r.stationarity, 3.0 * eps); // the normwise residual, exactly
    EXPECT_NEAR(r.stationarity_beyond_rounding, 3.0 * eps - eps * (2.0 - 3.0 * eps), 1e-3 * eps);
    EXPECT_GT(r.stationarity_beyond_rounding, 0.5 * eps);
}

// THE LOCAL SCALE's terms, each made the one that decides (C567's definition, read directly).
// (a) A pinned bound whose |z_i| exceeds |g_i|, |(Hx)_i| and |(A'lambda)_i|: x0 pinned at its
//     lower bound 0 and read at x0 = 1e-3 with z0 = 4, g0 = 1: product = 1e-3 * 4 / 4.
// (b) A working row whose join |lambda_j| ||a_j||inf exceeds every column term: two working rows
//     10 x0 <= 5.001 and -10 x0 <= -4.999 priced at 5 each, so (A'lambda)_0 = 50 - 50 = 0 and the
//     column's largest term is |g0| = 1; each row is 1e-3 slack and reads rel * 5 / max(1, 5 * 10).
TEST(QpCertificateData, TheLocalScaleJoinsTheBoundMultiplierAndTheRowsOwnTerm) {
    {
        QpProblem qp;
        qp.H = Eigen::MatrixXd::Identity(1, 1).sparseView();
        qp.g = Vec::Constant(1, 1.0);
        qp.Ae.resize(0, 1);
        qp.be = Vec(0);
        qp.Ai.resize(0, 1);
        qp.bi = Vec(0);
        qp.lower = Vec::Constant(1, 0.0);
        qp.upper = Vec::Constant(1, 1.0);
        WorkingSet ws(1, 0);
        ws.bound_state()[0] = BoundState::kAtLower;
        const CertificateReading r = certificate_reading(qp, ws, Vec::Constant(1, 1e-3), Vec(0),
                                                         Vec(0), Vec::Constant(1, 4.0));
        EXPECT_NEAR(r.product, 1e-3, 1e-18);
    }
    {
        QpProblem qp;
        qp.H = Eigen::MatrixXd::Identity(1, 1).sparseView();
        qp.g = Vec::Constant(1, 1.0);
        qp.Ae.resize(0, 1);
        qp.be = Vec(0);
        Eigen::MatrixXd A(2, 1);
        A << 10.0, -10.0;
        qp.Ai = A.sparseView();
        qp.bi = Vec(2);
        qp.bi << 5.001, -4.999;
        qp.lower = Vec::Constant(1, -10.0);
        qp.upper = Vec::Constant(1, 10.0);
        WorkingSet ws(1, 2);
        ws.add_ineq(0);
        ws.add_ineq(1);
        const CertificateReading r = certificate_reading(qp, ws, Vec::Constant(1, 0.5), Vec(0),
                                                         Vec::Constant(2, 5.0), Vec::Zero(1));
        // Row 0's scale is max(1, 5.001, 10 * 0.5) = 5.001, row 1's max(1, 4.999, 5) = 5.
        const double rel = std::max(std::abs(5.0 - 5.001) / 5.001, std::abs(-5.0 + 4.999) / 5.0);
        EXPECT_NEAR(r.product, rel * 5.0 / 50.0, 1e-15);
    }
}

// THE SLACK TEST (settler C568). Any product-type scaling lets a multiplier that is small against
// the stationarity terms on its own columns excuse a slack of up to the footprint: TRAIN-238's
// HS101 sqp-qn certified three exits with its working inequality I2 held 471-578 feas_tol SLACK
// under lambda = 100 / 1000, local scale 1.3e5 (row-local product 0.44-4.2 feas_tol). So a working
// row or pinned bound whose multiplier is not rounding-zero must sit within 100 feas_tol (relative
// to its row scale) of its bound: refuse iff slack_j / row_scale_j > 100 feas_tol AND
// |mult_j| ||a_j||inf > 100 u s_j (s_j the row-local scale, u = eps / 2).
//
// THE HS101 SHAPE, one column: min 1/2 h x0^2 + g0 x0 s.t. x0 <= 1 with g0 = -S, h = S - m, so the
// unconstrained minimizer S / (S - m) > 1, the row binds and its multiplier is m. Presented:
// x0 = 1 - d with lambda_i = m + h d -- the EXACT KKT point of the row shifted by d (stationarity
// h x0 + g0 + lambda_i = 0, rows 0), whose row-local product d m / S certifies for every d here.

/// min 1/2 (S - m) x0^2 - S x0  s.t.  x0 <= 1, x0 in [-10, 10]: optimum x0 = 1, lambda_i = m.
QpProblem small_price_row_qp(double S, double m) {
    QpProblem qp;
    qp.H = Eigen::MatrixXd::Constant(1, 1, S - m).sparseView();
    qp.g = Vec::Constant(1, -S);
    qp.Ae.resize(0, 1);
    qp.be = Vec(0);
    qp.Ai = Eigen::MatrixXd::Constant(1, 1, 1.0).sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Constant(1, -10.0);
    qp.upper = Vec::Constant(1, 10.0);
    return qp;
}

TEST(QpCertificateData, AWorkingRowSlackUnderASmallMultiplierIsRefused) {
    DataSeamGuard guard;
    const double ftol = QpOptions{}.feasibility_tol;
    constexpr double S = 1.3e5;
    for (const double m : {100.0, 1000.0}) {
        SCOPED_TRACE(m);
        const double h = S - m;
        const auto present = [&](double d, QpWorkingSetAlgebra alg) {
            Cert::reset();
            Cert::x = Vec::Constant(1, 1.0 - d);
            Cert::z = Vec::Zero(1);
            Cert::lambda_i = Vec::Constant(1, m + h * d);
            Cert::armed = true;
            QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
            QpSolution s = engine.solve(small_price_row_qp(S, m));
            Cert::armed = false;
            return s;
        };
        // THE PREMISE: the row-local product passes HS101's slacks (and so does every other
        // reading but the slack): the product test alone certified them.
        for (const double d : {471.0 * ftol, 578.0 * ftol}) {
            WorkingSet ws(1, 1);
            ws.add_ineq(0);
            const CertificateReading r =
                certificate_reading(small_price_row_qp(S, m), ws, Vec::Constant(1, 1.0 - d), Vec(0),
                                    Vec::Constant(1, m + h * d), Vec::Zero(1));
            ASSERT_EQ(r.rows, 0.0);
            ASSERT_LE(r.product, 100.0 * ftol);
            ASSERT_LE(r.stationarity_beyond_rounding, 1e-12);
            ASSERT_EQ(r.off_working, 0.0);
            EXPECT_NEAR(r.slack, d, 1e-15);
        }
        for (const auto alg : kAlgebras) {
            SCOPED_TRACE(static_cast<int>(alg));
            QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
            const QpSolution natural = engine.solve(small_price_row_qp(S, m));
            ASSERT_EQ(static_cast<int>(natural.status), static_cast<int>(QpStatus::kOptimal));
            ASSERT_TRUE(natural.ineq_active[0]);
            ASSERT_NEAR(natural.lambda_i(0), m, 1e-6 * m);
            expect_refused(present(471.0 * ftol, alg));
            expect_refused(present(578.0 * ftol, alg));
            expect_refused(present(101.0 * ftol, alg));
            expect_certified(present(99.0 * ftol, alg));
        }
    }
}

/// The pinned-bound analogue: min 1/2 S x0^2 + (m - S) x0 on [1, 10]: the unconstrained minimizer
/// (S - m) / S < 1, so x0 pins at its lower bound 1 under z0 = m.
QpProblem small_price_bound_qp(double S, double m) {
    QpProblem qp;
    qp.H = Eigen::MatrixXd::Constant(1, 1, S).sparseView();
    qp.g = Vec::Constant(1, m - S);
    qp.Ae.resize(0, 1);
    qp.be = Vec(0);
    qp.Ai.resize(0, 1);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(1, 1.0);
    qp.upper = Vec::Constant(1, 10.0);
    return qp;
}

// Presented: x0 = 1 + d INSIDE its box with z0 = m + S d, the exact KKT point of the bound shifted
// by d. Row scale max(1, |bound| = 1, |x|inf = 1 + d), so the relative slack is d / (1 + d).
TEST(QpCertificateData, APinnedBoundSlackUnderASmallMultiplierIsRefused) {
    DataSeamGuard guard;
    const double ftol = QpOptions{}.feasibility_tol;
    constexpr double S = 1.3e5;
    for (const double m : {100.0, 1000.0}) {
        SCOPED_TRACE(m);
        const auto present = [&](double d, QpWorkingSetAlgebra alg) {
            Cert::reset();
            Cert::x = Vec::Constant(1, 1.0 + d);
            Cert::z = Vec::Constant(1, m + S * d);
            Cert::armed = true;
            QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
            QpSolution s = engine.solve(small_price_bound_qp(S, m));
            Cert::armed = false;
            return s;
        };
        {
            const double d = 500.0 * ftol;
            WorkingSet ws(1, 0);
            ws.bound_state()[0] = BoundState::kAtLower;
            const CertificateReading r =
                certificate_reading(small_price_bound_qp(S, m), ws, Vec::Constant(1, 1.0 + d),
                                    Vec(0), Vec(0), Vec::Constant(1, m + S * d));
            ASSERT_EQ(r.rows, 0.0);
            ASSERT_LE(r.product, 100.0 * ftol); // the premise: the product test certified it
            ASSERT_LE(r.stationarity_beyond_rounding, 1e-12);
            EXPECT_NEAR(r.slack, d / (1.0 + d), 1e-15);
        }
        for (const auto alg : kAlgebras) {
            SCOPED_TRACE(static_cast<int>(alg));
            QpEngine engine{opts_for(alg), 0, hven::linear::DensePolicy::kNever};
            const QpSolution natural = engine.solve(small_price_bound_qp(S, m));
            ASSERT_EQ(static_cast<int>(natural.status), static_cast<int>(QpStatus::kOptimal));
            ASSERT_EQ(natural.bound_state[0], BoundState::kAtLower);
            ASSERT_NEAR(natural.z(0), m, 1e-6 * m);
            expect_refused(present(500.0 * ftol, alg));
            expect_refused(present(101.0 * ftol, alg));
            expect_certified(present(99.0 * ftol, alg));
        }
    }
}

// THE ROUNDING-ZERO EXEMPTION (the CRESC4 shape: TRAIN-238 certified two exits with a working row
// 9.4e6 / 6.5e7 feas_tol slack under lambda ~ 1e-21, local scale 161). A multiplier with
// |mult_j| ||a_j||inf <= 100 u s_j prices nothing double can resolve on its columns, so its row is
// weakly active and its slack is no defect. Read directly: ineq_line_qp(0.25)'s unconstrained
// optimum (0.25, 0.25) sits 0.5 inside x0 + x1 <= 1; with the row WORKING, lambda_i = 1e-21
// (threshold 100 u 0.25 = 2.8e-15) holds and lambda_i = 1e-12 refuses on the slack alone; the
// bound twin likewise (box_qp(-0.5): x0 = 0.5 free-optimal, pinned at 0 under a rounding-zero z0).
TEST(QpCertificateData, ARoundingZeroMultiplierExemptsItsSlack) {
    constexpr double u = 0.5 * std::numeric_limits<double>::epsilon();
    const double ftol = 1e-9;
    const double otol = 1e-6;
    {
        const QpProblem qp = ineq_line_qp(0.25);
        WorkingSet ws(2, 1);
        ws.add_ineq(0);
        const Vec x = Vec::Constant(2, 0.25);
        const auto read = [&](double lam) {
            return certificate_reading(qp, ws, x, Vec(0), Vec::Constant(1, lam), Vec::Zero(2));
        };
        ASSERT_LT(1e-21, 100.0 * u * 0.25);
        ASSERT_GT(1e-12, 100.0 * u * 0.25);
        const CertificateReading zero = read(1e-21);
        EXPECT_EQ(zero.slack, 0.0);
        EXPECT_TRUE(certificate_data_half_holds(zero, ftol, otol));
        const CertificateReading priced = read(1e-12);
        EXPECT_NEAR(priced.slack, 0.5, 1e-15);
        EXPECT_LE(priced.product, 100.0 * ftol);
        EXPECT_LE(priced.stationarity_beyond_rounding, 10.0 * otol);
        EXPECT_FALSE(certificate_data_half_holds(priced, ftol, otol));
        // Just either side of the threshold itself (s = 0.25 is |g_i| = |(Hx)_i|).
        EXPECT_EQ(read(0.99 * 100.0 * u * 0.25).slack, 0.0);
        EXPECT_NEAR(read(1.01 * 100.0 * u * 0.25).slack, 0.5, 1e-15);
    }
    {
        QpProblem qp = box_qp(-0.5);
        qp.g << -0.5, -0.25;
        WorkingSet ws(2, 0);
        ws.bound_state()[0] = BoundState::kAtLower;
        Vec x(2);
        x << 0.5, 0.25;
        const auto read = [&](double z0) {
            Vec z = Vec::Zero(2);
            z(0) = z0;
            return certificate_reading(qp, ws, x, Vec(0), Vec(0), z);
        };
        EXPECT_EQ(read(1e-21).slack, 0.0);
        EXPECT_TRUE(certificate_data_half_holds(read(1e-21), ftol, otol));
        EXPECT_NEAR(read(1e-12).slack, 0.5, 1e-15);
        EXPECT_FALSE(certificate_data_half_holds(read(1e-12), ftol, otol));
    }
}

// THE SLACK IS ONE-SIDED AND READ ONLY ON WORKING ROWS AND PINNED BOUNDS: a non-working slack row
// reads nothing (its multiplier must be zero anyway, off_working), and an equality row's residual
// is the rows reading's (10 feas_tol, two-sided), never the slack's.
TEST(QpCertificateData, TheSlackReadsOnlyWorkingRowsAndPinnedBounds) {
    const QpProblem qp = two_row_qp(1000.0);
    WorkingSet ws(2, 2);
    ws.add_ineq(0);
    const Vec x = Vec::Constant(2, 0.5); // row 1 (x0 - x1 <= 5) is 5 slack, not working
    Vec li(2);
    li << 999.5, 0.0;
    const CertificateReading r = certificate_reading(qp, ws, x, Vec(0), li, Vec::Zero(2));
    EXPECT_EQ(r.slack, 0.0);
    EXPECT_TRUE(certificate_data_half_holds(r, 1e-9, 1e-6));
    const QpProblem eq = line_qp(1000.0);
    const WorkingSet wse(2, 0);
    Vec xe(2);
    xe << 0.5 + 5e-9, 0.5;
    const CertificateReading re =
        certificate_reading(eq, wse, xe, Vec::Constant(1, 999.5), Vec(0), Vec::Zero(2));
    EXPECT_EQ(re.slack, 0.0);
}
