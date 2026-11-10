// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// THE WALK UNDER THE DENSE ARM (M8 DN-1, design §13 T13). Two behavioural facts the dense arm
// changes, pinned so they are tested rather than merely argued:
//
//   1. A K0 Pardiso perturbs (the walk reads kSuspect) is read kOk by the dense arm, which has
//      no perturbation mechanism; QpCounters::inertia_suspect_verdicts moves accordingly.
//   2. The mu = 0 re-read of a kWrong verdict (QpEngine::reread_wrong_inertia). On Pardiso a
//      near-singular mu-free face reads kSuspect and R-C' asks for a confirming read at the
//      pass's mu; the dense arm reads a trusted verdict directly (or, on an exactly singular
//      face, falls back to Pardiso and reads exactly what it reads today). Both arms' outcome
//      and dep_inertia_reread_flips are pinned.
//
// NO FIRING FIXTURE for the dense R-C' flip itself: a dense-taken kOk at mu = 0 that flips a
// kWrong WITHOUT the confirming read at the pass's mu (design §5.3) is not reached by either
// fixture below -- the saddle's kWrong stands and the singular PSD face's re-reads behave as
// pinned. That path is argued, not tested here; DN-1's S3 leg (dep_inertia_reread_flips per
// cell, kNever vs kAlways, over DN-0's roster) is its first observation.
//
// In hven_qp_walk_seam_tests for the E2-D4 dump fixtures; no seam is read.

#include <algorithm>
#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/linear/fault_injection.h>
#include <hven/detail/qp/eqp_solve.h>
#include <hven/detail/qp/qp_engine.h>

#include "../common_support/route_reference.h"
#include "support/qp_dump.h"

namespace {

using namespace hven;
using namespace hven::detail;
using hven::linear::DensePolicy;
using hven::linear::FactorArm;
using hven::linear::InertiaEvidence;

double objective(const QpProblem &qp, const Vec &x) {
    return 0.5 * x.dot(qp.H.selfadjointView<Eigen::Upper>() * x) + qp.g.dot(x);
}

// H = [[1, 1, 0], [1, h, 0], [0, 0, 1]], g = -1, x2 = 0, box [-1, 1]^3, with
// h = 1e-10 - delta + 1/(1 + delta) for the default primal regularization delta = 1e-8: K0's
// leading block [[1 + delta, 1], [1, h + delta]] has a second pivot of 1e-10 after the first is
// eliminated. MEASURED: Pardiso perturbs it (perturbed_pivots 1, counts 3/1) at 1e-9 .. 1e-14,
// and leaves an isolated diagonal pivot down to 1e-24 unperturbed; Bunch-Kaufman takes it with
// the same counts. 1e-10 is six orders above roundoff, so the dense read is decidable.
QpProblem tiny_pivot_qp() {
    QpProblem qp;
    const double delta = 1e-8;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(3, 3);
    Hd(0, 0) = 1.0;
    Hd(0, 1) = 1.0;
    Hd(1, 1) = 1e-10 - delta + 1.0 / (1.0 + delta);
    Hd(2, 2) = 1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(3, -1.0);
    Eigen::MatrixXd Aed(1, 3);
    Aed << 0, 0, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 0.0);
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -1.0);
    qp.upper = Vec::Constant(3, 1.0);
    return qp;
}

// M8 DN-1d: every SQP factor built by sqp_kkt_options reports under a dense policy, and only
// there (the factor layer refuses kReport under kNever).
TEST(QpDenseArm, SqpKktOptionsReportUnderADensePolicyOnly) {
    using hven::linear::DenseSingularPolicy;
    EXPECT_EQ(sqp_kkt_options(0, DensePolicy::kNever).dense_singular_policy,
              DenseSingularPolicy::kSparseFallback);
    EXPECT_EQ(sqp_kkt_options(0, DensePolicy::kAlways).dense_singular_policy,
              DenseSingularPolicy::kReport);
    EXPECT_EQ(sqp_kkt_options(0, DensePolicy::kAuto).dense_singular_policy,
              DenseSingularPolicy::kReport);
}

SpMatRM upper_of(const Eigen::MatrixXd &m) {
    SpMatRM A = m.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    for (Index i = 0; i < m.rows(); ++i) {
        A.coeffRef(i, i) += 0.0; // every diagonal stored
    }
    A.makeCompressed();
    return A;
}

// THE HS90 CATASTROPHE (E0 roster, HS90 factorization 5, eqp site, mu = 0): a working row
// 1.5e-17 x e_2 against an rhs of -0.13 -- inconsistent and singular to working precision. The
// site response factors it equilibrated and shifted; kNever never responds.
TEST(QpDenseArm, FactorizeCheckedShiftsAFlaggedFactorAndReturnsTheSiteRegularization) {
    Eigen::MatrixXd m = Eigen::MatrixXd::Zero(3, 3);
    m(0, 0) = m(1, 1) = 2.00000001;
    m(1, 2) = m(2, 1) = -0x1.14b1907515e67p-56;
    const SpMatRM K = upper_of(m);
    const DenseShiftSite site{2, 1e-8, 0.0};

    KktFactor sparse(0, DensePolicy::kNever);
    const CheckedFactorization cs = factorize_checked(sparse, K, site);
    EXPECT_FALSE(cs.shifted);
    EXPECT_FALSE(sparse.dense_shifted);
    EXPECT_EQ(sparse.dense_scale.size(), 0);
    EXPECT_EQ(cs.delta, 1e-8);
    EXPECT_EQ(cs.mu, 0.0);

    KktFactor dense(0, DensePolicy::kAlways);
    Index flagged = 0;
    Index refactors = 0;
    Index retakes = 0;
    CheckedFactorization cd;
    {
        const DenseTallyScope tally(dense, flagged, refactors, retakes);
        cd = factorize_checked(dense, K, site);
    }
    EXPECT_TRUE(cd.shifted);
    EXPECT_FALSE(cd.retaken);
    EXPECT_TRUE(dense.dense_shifted);
    EXPECT_EQ(dense.dense_scale.size(), 3);
    EXPECT_EQ(cd.outcome.arm, FactorArm::kDense);
    EXPECT_EQ(cd.delta, 1e-8);
    EXPECT_EQ(cd.mu, 0.0);
    ASSERT_TRUE(cd.outcome.inertia.perturbed_pivots.has_value());
    EXPECT_FALSE(cd.outcome.inertia.numerically_singular);
    EXPECT_EQ(inertia_verdict(cd.outcome.inertia, 2, 1), InertiaVerdict::kSuspect);
    EXPECT_EQ(flagged, 1);
    EXPECT_EQ(refactors, 1);
    EXPECT_EQ(retakes, 0);
    Vec rhs(3);
    rhs << 0.0, -0x1.fb815d5e24b25p-53, -0x1.0a2549bd45f2fp-3;
    const Vec y = solve_vec(dense, rhs);
    EXPECT_TRUE(y.allFinite());
    EXPECT_LE(std::abs(y(0)), 1e-12);

    // The next factorization of a healthy K clears the response.
    const SpMatRM I3 = upper_of(Eigen::MatrixXd::Identity(3, 3));
    KktFactor again(0, DensePolicy::kAlways);
    (void)factorize_checked(again, K, site);
    ASSERT_TRUE(again.dense_shifted);
    (void)factorize_checked(again, I3, site);
    EXPECT_FALSE(again.dense_shifted);
    EXPECT_EQ(again.dense_scale.size(), 0);
}

// Ruiz on the upper triangle: rows of max 4 and 1/4 scale to max 1; a zero row keeps 1.
TEST(QpDenseArm, EquilibrationScaleBalancesRowsAndKeepsZeroRows) {
    Eigen::MatrixXd m = Eigen::MatrixXd::Zero(3, 3);
    m(0, 0) = 4.0;
    m(1, 1) = 0.25;
    const Vec d = equilibration_scale(upper_of(m));
    EXPECT_EQ(d(0), 0.5);
    EXPECT_EQ(d(1), 2.0);
    EXPECT_EQ(d(2), 1.0);
}

// The one retry and the last resort. [[1, 1], [1, 1]] with n_primal = 1 is exactly singular; its
// equilibrated shift diag(+s, -s) has determinant -s^2, exactly zero in floating point at
// s = 2^-30 and taken at 256 s. With both shifted attempts read as singular
// (DenseFactorizeFaultInjector; the retake is the platform's own) the platform retakes.
TEST(QpDenseArm, FactorizeCheckedRetriesOnceThenRetakesOnThePlatform) {
    const DenseShiftSite site{1, 0.0, 0.0};
    Eigen::MatrixXd m = Eigen::MatrixXd::Ones(2, 2);
    {
        KktFactor k(0, DensePolicy::kAlways);
        Index flagged = 0, refactors = 0, retakes = 0;
        CheckedFactorization c;
        {
            const DenseTallyScope tally(k, flagged, refactors, retakes);
            c = factorize_checked(k, upper_of(m), site);
        }
        EXPECT_TRUE(c.shifted);
        EXPECT_FALSE(c.retaken);
        EXPECT_EQ(flagged, 1); // the original; the exactly singular first shift is a retry
        EXPECT_EQ(refactors, 2);
        EXPECT_EQ(k.shifted_exact_singular, 1);
        EXPECT_EQ(retakes, 0);
    }
    {
        using hven::linear::detail::testing::DenseFactorizeFaultInjector;
        KktFactor k(0, DensePolicy::kAlways);
        Index flagged = 0, refactors = 0, retakes = 0;
        CheckedFactorization c;
        DenseFactorizeFaultInjector::pass = 1;
        DenseFactorizeFaultInjector::active = true;
        {
            const DenseTallyScope tally(k, flagged, refactors, retakes);
            c = factorize_checked(k, upper_of(m), site);
        }
        DenseFactorizeFaultInjector::active = false;
        EXPECT_FALSE(c.shifted);
        EXPECT_TRUE(c.retaken);
        EXPECT_FALSE(k.dense_shifted);
        EXPECT_EQ(c.outcome.arm, FactorArm::kSparse);
        EXPECT_EQ(flagged, 1);
        EXPECT_EQ(refactors, 2);
        EXPECT_EQ(retakes, 1);
    }
}

// min 0.5 |x|^2 - x2 subject to rows of Ae at mu = 0, through solve_eqp on both arms.
QpProblem rows_qp(const Eigen::MatrixXd &Ae, const Vec &be, double h2) {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(3, 3);
    Hd(2, 2) = h2;
    qp.H = Hd.sparseView();
    qp.g = Vec::Zero(3);
    qp.g(1) = -1.0;
    qp.Ae = Ae.sparseView();
    qp.be = be;
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -10.0);
    qp.upper = Vec::Constant(3, 10.0);
    return qp;
}

struct EqpRun {
    EqpResult res;
    Index flagged = 0;
    Index refactors = 0;
    Index retakes = 0;
    InertiaVerdict verdict = InertiaVerdict::kWrong;
};

EqpRun eqp_on(DensePolicy policy, const QpProblem &qp, const QpOptions &opts) {
    KktFactor k(0, policy);
    EqpRun r;
    {
        const DenseTallyScope tally(k, r.flagged, r.refactors, r.retakes);
        r.res = solve_eqp(qp, WorkingSet(3, 0), Vec::Zero(3), k, opts);
    }
    r.verdict = inertia_verdict(k.factor.inertia(), 3, qp.me());
    return r;
}

// A RANK-DEFICIENT WORKING SET at mu = 0: duplicated rows, consistent and inconsistent. The dense
// factor is flagged and shifted (its inertia reads kSuspect); the consistent answer solves K and is
// the sparse arm's; the inconsistent one misses K by O(1) and is refused. Nothing reaches the
// platform.
TEST(QpDenseArm, ARankDeficientWorkingSetAtMuZeroIsShiftedAndVerified) {
    QpOptions opts;
    opts.dual_regularization = 0.0;
    Eigen::MatrixXd Ae(2, 3);
    Ae << 1.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    {
        const QpProblem qp = rows_qp(Ae, Vec::Constant(2, 1.0), 1.0);
        const EqpRun s = eqp_on(DensePolicy::kNever, qp, opts);
        const EqpRun d = eqp_on(DensePolicy::kAlways, qp, opts);
        EXPECT_EQ(d.flagged, 1);
        EXPECT_EQ(d.refactors, 1);
        EXPECT_EQ(d.retakes, 0);
        EXPECT_EQ(d.verdict, InertiaVerdict::kSuspect);
        EXPECT_FALSE(d.res.shifted_refused);
        EXPECT_FALSE(d.res.residual_refused);
        EXPECT_NEAR(d.res.x(0) + d.res.x(1), 1.0, 1e-9);
        EXPECT_LE((d.res.x - s.res.x).lpNorm<Eigen::Infinity>(), 1e-6);
    }
    {
        Vec be(2);
        be << 1.0, 1.5;
        const QpProblem qp = rows_qp(Ae, be, 1.0);
        const EqpRun d = eqp_on(DensePolicy::kAlways, qp, opts);
        EXPECT_EQ(d.refactors, 1);
        EXPECT_EQ(d.retakes, 0);
        EXPECT_TRUE(d.res.shifted_refused);
        EXPECT_TRUE(d.res.residual_refused);
    }
}

// THE ACOPP30 SHAPE: a second row 1.76 x the first plus a 1e-16 perturbation, at mu = 0 -- a
// near-null pivot the pivot factorization takes. Flagged, shifted, and the sparse arm's answer.
TEST(QpDenseArm, ANearNullPivotIsShiftedToTheSparseAnswer) {
    QpOptions opts;
    opts.dual_regularization = 0.0;
    Eigen::MatrixXd Ae(2, 3);
    Ae << 1.0, 2.0, 0.0, 1.76, 3.52 + 1e-16 * 3.52, 0.0;
    Vec be(2);
    be << 1.0, 1.76;
    const QpProblem qp = rows_qp(Ae, be, 1.0);
    const EqpRun s = eqp_on(DensePolicy::kNever, qp, opts);
    const EqpRun d = eqp_on(DensePolicy::kAlways, qp, opts);
    EXPECT_EQ(d.refactors, 1);
    EXPECT_EQ(d.retakes, 0);
    EXPECT_LE(std::abs(d.res.x(0) + 2.0 * d.res.x(1) - 1.0), 1e-9);
    EXPECT_LE((d.res.x - s.res.x).lpNorm<Eigen::Infinity>(), 1e-6);
}

// AN EXACT-ZERO COLUMN: x3 has no curvature and no row, and the primal regularization is 0 --
// the dense arm reports it exactly singular and the shift recovers x3 = 0.
TEST(QpDenseArm, AnExactZeroColumnIsShiftedNotRetaken) {
    QpOptions opts;
    opts.primal_regularization = 0.0;
    opts.dual_regularization = 0.0;
    Eigen::MatrixXd Ae(1, 3);
    Ae << 1.0, 1.0, 0.0;
    const QpProblem qp = rows_qp(Ae, Vec::Constant(1, 1.0), 0.0);
    const EqpRun d = eqp_on(DensePolicy::kAlways, qp, opts);
    EXPECT_EQ(d.flagged, 1);
    EXPECT_EQ(d.refactors, 1);
    EXPECT_EQ(d.retakes, 0);
    EXPECT_NEAR(d.res.x(2), 0.0, 1e-12);
    EXPECT_NEAR(d.res.x(0) + d.res.x(1), 1.0, 1e-9);
}

// Badly scaled, well posed (curvature over 12 decades): never flagged, the sparse arm's answer.
TEST(QpDenseArm, ABadlyScaledWellPosedFaceIsNotFlagged) {
    QpOptions opts;
    Eigen::MatrixXd Ae(1, 3);
    Ae << 1.0, 1e-3, 1e3;
    QpProblem qp = rows_qp(Ae, Vec::Constant(1, 1.0), 1e6);
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(3, 3);
    Hd(0, 0) = 1e-6;
    Hd(2, 2) = 1e6;
    qp.H = Hd.sparseView();
    const EqpRun s = eqp_on(DensePolicy::kNever, qp, opts);
    const EqpRun d = eqp_on(DensePolicy::kAlways, qp, opts);
    EXPECT_EQ(d.flagged, 0);
    EXPECT_EQ(d.refactors, 0);
    EXPECT_LE((d.res.x - s.res.x).lpNorm<Eigen::Infinity>(),
              1e-8 * std::max(1.0, s.res.x.lpNorm<Eigen::Infinity>()));
}

TEST(QpDenseArm, APerturbedK0ReadsSuspectOnPardisoAndOkOnTheDenseArm) {
    const QpProblem qp = tiny_pivot_qp();
    // The premise, at the factor: the seed K0 is perturbed on the platform session and
    // dense-taken, unperturbed, on the dense arm, with the same sign counts.
    {
        QpOptions opts;
        WorkingSet ws(3, 0);
        const KktAssembly a = assemble_kkt_full(qp, ws, opts);
        KktFactor sparse(0, DensePolicy::kNever);
        KktFactor dense(0, DensePolicy::kAlways);
        const auto os = factorize_checked(sparse, a.K);
        const auto od = factorize_checked(dense, a.K);
        ASSERT_EQ(od.arm, FactorArm::kDense);
        ASSERT_FALSE(od.inertia.perturbed_pivots.has_value());
#ifndef __APPLE__
        ASSERT_TRUE(os.inertia.perturbed_pivots.has_value());
        ASSERT_GE(*os.inertia.perturbed_pivots, 1);
        EXPECT_EQ(inertia_verdict(os.inertia, 3, 1), InertiaVerdict::kSuspect);
#endif
        EXPECT_EQ(od.inertia.positive, 3);
        EXPECT_EQ(od.inertia.negative, 1);
#ifndef __APPLE__
        EXPECT_EQ(os.inertia.positive, 3);
        EXPECT_EQ(os.inertia.negative, 1);
#endif
        EXPECT_EQ(inertia_verdict(od.inertia, 3, 1), InertiaVerdict::kOk);
    }

    for (const auto algebra :
         {QpWorkingSetAlgebra::kSchurBorder, QpWorkingSetAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == QpWorkingSetAlgebra::kSchurBorder ? "border" : "refactorize");
        QpOptions opts;
        opts.working_set_algebra = algebra;
        const QpEngine sparse_engine(opts, 0, DensePolicy::kNever);
        const QpEngine dense_engine(opts, 0, DensePolicy::kAlways);
        const QpSolution s = sparse_engine.solve(qp);
        const QpSolution d = dense_engine.solve(qp);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(static_cast<int>(d.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_NEAR(d.x(2), 0.0, 1e-12);
        EXPECT_NEAR(objective(qp, d.x), objective(qp, s.x), 1e-7);
#ifndef __APPLE__
        EXPECT_GE(s.counters.inertia_suspect_verdicts, 1);
#endif
        EXPECT_EQ(d.counters.inertia_suspect_verdicts, 0);
        EXPECT_EQ(d.counters.suspect_escalations, 0);
    }
}

// The mu = 0 re-read on a genuinely indefinite face (test_qp_anti_cycling_arm.cpp's
// AMuFreeSuspectReadDoesNotCertifyASaddle): the walk must leave the saddle on both arms. On
// Pardiso the mu-free read is a perturbed kSuspect that the confirming read refuses; on the
// dense arm it is a trusted kWrong or, on an exactly singular face, Pardiso's own kSuspect
// through the fallback. Either way the kWrong stands: no flip.
TEST(QpDenseArm, TheMuFreeRereadDoesNotCertifyASaddleOnEitherArm) {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(4, 4);
    Hd(0, 0) = 0.75;
    Hd(0, 2) = -1.0;
    Hd(0, 3) = 0.5;
    Hd(1, 1) = 1.0;
    Hd(2, 2) = 1.0;
    Hd(2, 3) = -1.0;
    Hd(3, 3) = -1.0;
    qp.H = Hd.sparseView();
    qp.g = Vec::Zero(4);
    Eigen::MatrixXd Ae = Eigen::MatrixXd::Zero(1, 4);
    Ae(0, 0) = 1.0;
    Ae(0, 2) = -1.0;
    qp.Ae = Ae.sparseView();
    qp.be = Vec::Zero(1);
    qp.Ai.resize(0, 4);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(4, -1.0);
    qp.upper = Vec::Constant(4, 1.0);
    qp.lower(0) = 0.0;
    qp.upper(0) = 0.0;
    for (const DensePolicy policy : {DensePolicy::kNever, DensePolicy::kAlways}) {
        for (const auto algebra :
             {QpWorkingSetAlgebra::kRefactorize, QpWorkingSetAlgebra::kSchurBorder}) {
            SCOPED_TRACE(policy == DensePolicy::kAlways ? "dense" : "sparse");
            SCOPED_TRACE(algebra == QpWorkingSetAlgebra::kSchurBorder ? "border" : "refactorize");
            QpOptions o;
            o.max_iterations = 40;
            o.independent_working_set = true;
            o.working_set_algebra = algebra;
            const QpEngine engine(o, 0, policy);
            const QpSolution s = engine.solve(qp);
            EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
            EXPECT_NEAR(objective(qp, s.x), -0.5, 1e-12);
            EXPECT_NEAR(std::abs(s.x(3)), 1.0, 1e-12);
            EXPECT_EQ(s.counters.dep_inertia_rereads, 1);
            EXPECT_EQ(s.counters.dep_inertia_reread_flips, 0);
        }
    }
}

// The mu = 0 re-read on a singular PSD face (ACOPP57's primary QP at major 3,
// test_qp_anti_cycling_arm.cpp's AMuFreeInertiaReReadCertifiesTheSingularPsdFaceOfAcopp57):
// lambda_min(Z'HZ) = +3.0e-12. On Pardiso the mu-free read is kSuspect and is confirmed by a kOk
// read at the pass's mu, and the border path's kWrong flips. On the dense arm the border path's K0
// reads kOk in the first place. Both arms reach the optimum, and every re-read that happens
// flips.
TEST(QpDenseArm, TheMuFreeRereadCertifiesASingularPsdFaceOnBothArms) {
    const hven::test_support::QpDump d = hven::test_support::load_qp_dump(
        std::string(HVEN_QP_E2D4_FIXTURES) + "/ACOPP57_def-qp0.txt");
    for (const DensePolicy policy : {DensePolicy::kNever, DensePolicy::kAlways}) {
        for (const auto algebra :
             {QpWorkingSetAlgebra::kRefactorize, QpWorkingSetAlgebra::kSchurBorder}) {
            SCOPED_TRACE(policy == DensePolicy::kAlways ? "dense" : "sparse");
            SCOPED_TRACE(algebra == QpWorkingSetAlgebra::kSchurBorder ? "border" : "refactorize");
            QpOptions o = d.opts;
            o.working_set_algebra = algebra;
            const QpEngine engine(o, d.threads, policy);
            const QpSolution s = d.has_seed ? engine.solve(d.qp, d.seed, d.overrides)
                                            : engine.solve(d.qp, d.overrides);
            EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
            EXPECT_LE(std::abs(hven::test_support::qp_dump_objective(d, s.x) - 0.243742606211),
                      1e-2 * 0.243742606211);
            EXPECT_EQ(s.counters.dep_inertia_reread_flips, s.counters.dep_inertia_rereads);
#if HVEN_TEST_ROUTE_REFERENCE
            // MEASURED (reference build): on Pardiso the border path's K0 reads kWrong twice
            // and both re-reads flip; on the dense arm that K0 reads kOk and nothing is re-read.
            // The refactorize walk reads kOk on both arms.
            if (algebra == QpWorkingSetAlgebra::kSchurBorder && policy == DensePolicy::kNever) {
                EXPECT_GE(s.counters.dep_inertia_reread_flips, 1);
                EXPECT_GE(s.counters.inertia_wrong_verdicts, 1);
            } else {
                EXPECT_EQ(s.counters.dep_inertia_rereads, 0);
                EXPECT_EQ(s.counters.inertia_wrong_verdicts, 0);
            }
#endif
        }
    }
}

// THE ALLINITC SHAPE (E0 roster: SQP ALLINITC's mu-free systems, K11 -> 2.7e9 against a working
// row ~1.3e-8): H = [[2^31, -9.5], [-9.5, 13]], one row a e_1 with a = 2^-26, at mu = 0 -- the
// polish's own system. Flagged for SCALING (D K D is well conditioned): a shift of 2^-27 max|K|
// = 16 on the dual row would replace the dual curvature a^2/2^31 ~ 1e-25 outright; the
// equilibrated shift answers the system the factor was handed, verified by its residual. The
// reference is K's own solution in long double.
TEST(QpDenseArm, TheAllinitcShapeMuFreeSystemIsAnsweredOnTheDenseArm) {
    const double h11 = std::ldexp(1.0, 31);
    const double a = std::ldexp(1.0, -26);
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << h11, -9.5, -9.5, 13.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << 19.0 - a, -26.0;
    Eigen::MatrixXd row(1, 2);
    row << a, 0.0;
    qp.Ae = row.sparseView();
    qp.be = Vec::Zero(1);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    QpOptions opts;
    opts.dual_regularization = 0.0;

    KktFactor k(0, DensePolicy::kAlways);
    Index flagged = 0, refactors = 0, retakes = 0;
    EqpResult r;
    {
        const DenseTallyScope tally(k, flagged, refactors, retakes);
        r = solve_eqp(qp, WorkingSet(2, 0), Vec::Zero(2), k, opts);
    }
    EXPECT_EQ(flagged, 1);
    EXPECT_EQ(refactors, 1);
    EXPECT_EQ(retakes, 0);
    EXPECT_TRUE(k.dense_shifted);
    EXPECT_EQ(inertia_verdict(k.factor.inertia(), 2, 1), InertiaVerdict::kSuspect);
    EXPECT_FALSE(r.shifted_refused);
    EXPECT_FALSE(r.residual_refused);

    // [[h11 + delta, -9.5, a], [-9.5, 13 + delta, 0], [a, 0, 0]] y = (-g1, -g2, 0).
    using LD = long double;
    const LD delta = opts.primal_regularization;
    const LD x2 = LD{26} / (LD{13} + delta);
    const LD lam = (-(LD{19} - LD{a}) + LD{9.5} * x2) / LD{a};
    EXPECT_LE(std::abs(r.x(0)), 1e-15);
    EXPECT_NEAR(r.x(1), static_cast<double>(x2), 1e-14);
    EXPECT_NEAR(r.lambda_e(0), static_cast<double>(lam), 1e-6);
}

// THE EQUILIBRATED MONITOR. diag(K_A, t B) with K_A = 2.7e9 and t = 1.234e-18,
// B = [[1, 0.7], [0.7, 0.49 (1 + 3e-8)]] (equilibrated near-null curvature ~8 sigma, so each
// refinement step contracts that component ~9x): ||rhs - K y||inf is the big row's roundoff floor
// from the first step on and stalls, refusing an answer whose equilibrated residual is still
// ~1e-3; ||D (rhs - K y)||inf keeps falling to roundoff.
TEST(QpDenseArm, TheUnscaledMonitorStallsWhereTheEquilibratedOneConverges) {
    const double t = 1.234e-18;
    Eigen::MatrixXd m = Eigen::MatrixXd::Zero(3, 3);
    m(0, 0) = 2707302431.897003;
    m(1, 1) = t;
    m(1, 2) = m(2, 1) = 0.7 * t;
    m(2, 2) = 0.49 * t * (1.0 + 3e-8);
    Vec b(3);
    b << 27906.13193018271, 0.3 * std::sqrt(t), -0.1 * std::sqrt(t);
    const SpMatRM K = upper_of(m);
    KktFactor k(0, DensePolicy::kAlways);
    ASSERT_TRUE(factorize_checked(k, K, DenseShiftSite{3, 0.0, 0.0}).shifted);
    const Vec &d = k.dense_scale;
    const auto residual = [&](const Vec &y) {
        return Vec(b - K.selfadjointView<Eigen::Upper>() * y);
    };
    const auto rho = [&](const Vec &y) {
        return residual(y).cwiseProduct(d).lpNorm<Eigen::Infinity>() /
               b.cwiseProduct(d).lpNorm<Eigen::Infinity>();
    };
    const Vec y0 = solve_vec(k, b);
    // The premise: refinement monitored in ||.||inf stops above the tolerance.
    Vec yu = y0;
    double un = residual(yu).lpNorm<Eigen::Infinity>();
    for (Index step = 0; step < kEqpShiftRefineSteps; ++step) {
        const Vec cand = yu + solve_vec(k, residual(yu));
        const double cu = residual(cand).lpNorm<Eigen::Infinity>();
        if (!(cu < un)) {
            break;
        }
        yu = cand;
        un = cu;
    }
    EXPECT_GT(rho(yu), 1e-4);
    Vec y = y0;
    (void)refine_shifted_solve(K, b, k, y);
    EXPECT_LE(rho(y), kEqpShiftedResidualTol);
}

} // namespace
