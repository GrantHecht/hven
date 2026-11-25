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
// <= 10 feas_tol, product <= 100 feas_tol, relative to each row's scale -- and refuses the
// certificate (kNumericalError) otherwise.
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

// The rows' bound: 10 feas_tol at row scale 1. lambda_e = 0.5 keeps the product (d / 2) inside
// 100 feas_tol for every d here, so the rows decide; the footprint excuse at R is
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

// The product's bound: 100 feas_tol. With lambda_e = 999.5 a residual of 5 feas_tol (inside the
// rows' bound and far inside R's 1e-6 excuse, which R certified without a gray return) is a
// product of 5e-6: refused. At the boundary: 0.11 feas_tol is a product of 1.1e-7 (refused) and
// 0.09 feas_tol one of 9e-8 (certified).
TEST(QpCertificateData, AProductPastAHundredFeasTolIsRefused) {
    DataSeamGuard guard;
    const double ftol = QpOptions{}.feasibility_tol;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        expect_refused(present_line(1000.0, 5.0 * ftol, alg));
        expect_refused(present_line(1000.0, 0.11 * ftol, alg));
        expect_certified(present_line(1000.0, 0.09 * ftol, alg));
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

TEST(QpCertificateData, TheVerdictRefusesNanAndReadsBothBoundsInclusively) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double ftol = 1e-9;
    EXPECT_TRUE(certificate_data_half_holds(10.0 * ftol, 100.0 * ftol, ftol));
    EXPECT_FALSE(certificate_data_half_holds(nan, 0.0, ftol));
    EXPECT_FALSE(certificate_data_half_holds(0.0, nan, ftol));
    EXPECT_FALSE(certificate_data_half_holds(10.01 * ftol, 0.0, ftol));
    EXPECT_FALSE(certificate_data_half_holds(0.0, 100.1 * ftol, ftol));
    EXPECT_EQ(kCertificateGateRowFactor, 10.0);
    EXPECT_EQ(kCertificateGateComplFactor, 100.0);
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
// m8 (P-1a deciders, D-4): no in-repo fixture exit exceeds either bound.
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
