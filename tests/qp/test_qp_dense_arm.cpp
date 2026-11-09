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

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_calls.h>
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

} // namespace
