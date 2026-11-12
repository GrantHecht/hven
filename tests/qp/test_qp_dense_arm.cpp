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
#include <limits>
#include <string>
#include <vector>

#include <Eigen/Eigenvalues>
#include <gtest/gtest.h>

#include <hven/detail/kkt/bordered_eqp.h>
#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/linear/fault_injection.h>
#include <hven/detail/qp/eqp_solve.h>
#include <hven/detail/qp/qp_engine.h>

#include "../common_support/route_reference.h"
#include "support/qp_dump.h"

namespace hven::detail {
// QpEngine's named friend, DEFINED here for this executable (test_qp_engine_indefinite.cpp
// defines its own in hven_tests): the two terminal sites, read directly.
struct QpEngineTestAccess {
    static bool terminal_mu_free(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                                 Vec &x, Vec &lambda_e, Vec &lambda_i, Vec &z, QpCounters &counters,
                                 const QpOptions &opts) {
        return eng.refine_terminal_mu_free(qp, ws, x, lambda_e, lambda_i, z, counters, opts);
    }
    static bool terminal_polish(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                                Vec &x, Vec &lambda_e, Vec &lambda_i, Vec &z, QpCounters &counters,
                                const QpOptions &opts) {
        const EqpResult none; // no pass answer: stage 1 cannot adopt, stage 2 solves
        return eng.polish_terminal_point(qp, ws, none, x, lambda_e, lambda_i, z, counters, opts);
    }
    // The polish's adoption test alone, on a candidate the caller supplies.
    static bool polish_would_adopt(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                                   const EqpResult &cand, Vec x, Vec lambda_e, Vec lambda_i, Vec z,
                                   const QpOptions &opts) {
        const TerminalKkt before = terminal_kkt(qp, ws, x, lambda_e, lambda_i, z);
        return eng.adopt_polished_candidate(qp, ws, cand, before, opts, x, lambda_e, lambda_i, z);
    }
    // K0 for `ws` and its borders, as border_candidate builds them.
    static void build_border(const QpEngine &eng, const QpProblem &qp, const WorkingSet &k0_ws,
                             const WorkingSet &ws, BorderState &border, QpCounters &counters,
                             const QpOptions &opts) {
        eng.rebuild_k0(qp, k0_ws, border, counters, opts);
        eng.sync_borders(qp, ws, border, counters, opts);
    }
    // The two verdict-site face refinements, at an iterate with zero multipliers.
    static bool border_verdict_refine(const QpEngine &eng, const QpProblem &qp,
                                      const WorkingSet &ws, Vec &x, BorderState &border,
                                      QpCounters &counters, const QpOptions &opts) {
        const VerdictInputs in(qp, x);
        return eng.refine_face_for_verdict(qp, ws, x, in.aix, in.ai_norm, in.lambda_i, in.ae_norm,
                                           in.lambda_e, border, counters, opts);
    }
    static bool eliminated_verdict_refine(const QpEngine &eng, const QpProblem &qp,
                                          const WorkingSet &ws, Vec &x, QpCounters &counters,
                                          const QpOptions &opts) {
        const VerdictInputs in(qp, x);
        const KktFactor kkt(0, eng.dense_policy());
        EliminatedFace face; // armed but not holding `ws`: the twin factorizes its own
        face.factorized = true;
        return eng.refine_eliminated_face_for_verdict(qp, ws, x, in.aix, in.ai_norm, in.lambda_i,
                                                      in.ae_norm, in.lambda_e, kkt, face, counters,
                                                      opts);
    }

    // The verdict refinements' acceptance measure, at zero multipliers.
    static double face_measure(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                               const Vec &x, const QpOptions &opts) {
        const VerdictInputs in(qp, x);
        return eng.working_face_measure(qp, ws, x, in.aix, in.ai_norm, in.lambda_i, in.ae_norm,
                                        in.lambda_e, opts);
    }

  private:
    struct VerdictInputs {
        VerdictInputs(const QpProblem &qp, const Vec &x)
            : aix(qp.mi() > 0 ? Vec(qp.Ai * x) : Vec(0)), ai_norm(Vec::Zero(qp.mi())),
              lambda_i(Vec::Zero(qp.mi())), ae_norm(Vec::Zero(qp.me())),
              lambda_e(Vec::Zero(qp.me())) {
            for (Index j = 0; j < qp.mi(); ++j) {
                ai_norm(j) = qp.Ai.row(j).cwiseAbs().sum();
            }
            for (Index j = 0; j < qp.me(); ++j) {
                ae_norm(j) = qp.Ae.row(j).cwiseAbs().sum();
            }
        }
        Vec aix, ai_norm, lambda_i, ae_norm, lambda_e;
    };
};
} // namespace hven::detail

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
TEST(QpDenseArm, FactorizeCheckedShiftsAFlaggedFactor) {
    Eigen::MatrixXd m = Eigen::MatrixXd::Zero(3, 3);
    m(0, 0) = m(1, 1) = 2.00000001;
    m(1, 2) = m(2, 1) = -0x1.14b1907515e67p-56;
    const SpMatRM K = upper_of(m);
    const DenseShiftSite site{2};

    KktFactor sparse(0, DensePolicy::kNever);
    (void)factorize_checked(sparse, K, site);
    EXPECT_FALSE(sparse.dense_shifted);
    EXPECT_EQ(sparse.dense_scale.size(), 0);

    KktFactor dense(0, DensePolicy::kAlways);
    Index flagged = 0;
    Index refactors = 0;
    Index retakes = 0;
    hven::linear::FactorizeOutcome cd;
    {
        const DenseTallyScope tally(dense, flagged, refactors, retakes);
        cd = factorize_checked(dense, K, site);
    }
    EXPECT_TRUE(dense.dense_shifted);
    EXPECT_EQ(dense.dense_scale.size(), 3);
    EXPECT_EQ(cd.arm, FactorArm::kDense);
    ASSERT_TRUE(cd.inertia.perturbed_pivots.has_value());
    EXPECT_FALSE(cd.inertia.numerically_singular);
    EXPECT_EQ(inertia_verdict(cd.inertia, 2, 1), InertiaVerdict::kSuspect);
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

// A THROW BEFORE THE NEXT FACTORIZATION (analyze()'s input check) leaves the shifted numerics
// solvable, so it leaves the marker that describes them set: the marker may over-claim, never
// under-claim.
TEST(QpDenseArm, AThrowBeforeTheNextFactorizationKeepsTheShiftMarker) {
    Eigen::MatrixXd m = Eigen::MatrixXd::Zero(3, 3);
    m(0, 0) = m(1, 1) = 2.00000001;
    m(1, 2) = m(2, 1) = -0x1.14b1907515e67p-56;
    const SpMatRM K = upper_of(m);
    KktFactor k(0, DensePolicy::kAlways);
    (void)factorize_checked(k, K, DenseShiftSite{2});
    ASSERT_TRUE(k.dense_shifted);
    // A new pattern with an entry below the diagonal: analyze() refuses it.
    SpMatRM bad = upper_of(Eigen::MatrixXd::Identity(3, 3));
    bad.coeffRef(2, 0) = 1.0;
    bad.makeCompressed();
    EXPECT_THROW((void)factorize_checked(k, bad, DenseShiftSite{2}), std::invalid_argument);
    EXPECT_TRUE(solve_vec(k, Vec::Ones(3)).allFinite()); // still solvable
    EXPECT_TRUE(k.dense_shifted);
    EXPECT_EQ(k.dense_scale.size(), 3);
}

// A THROW FROM THE FIRST EQUILIBRATED ATTEMPT leaves the marker set: it is raised before that
// attempt, so a throw from it or from the retake never leaves shifted numerics unmarked. [[1, 1],
// [1, 1]] is exactly singular (flagged); a shift site claiming 3 primal rows on a 2x2 K is
// refused by factorize_equilibrated itself.
TEST(QpDenseArm, AThrowFromTheFirstEquilibratedAttemptLeavesTheShiftMarkerSet) {
    const SpMatRM K = upper_of(Eigen::MatrixXd::Ones(2, 2));
    KktFactor k(0, DensePolicy::kAlways);
    EXPECT_FALSE(k.dense_shifted);
    EXPECT_THROW((void)factorize_checked(k, K, DenseShiftSite{3}), std::invalid_argument);
    EXPECT_TRUE(k.dense_shifted);
    EXPECT_EQ(k.dense_scale.size(), 2);
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
    const DenseShiftSite site{1};
    Eigen::MatrixXd m = Eigen::MatrixXd::Ones(2, 2);
    {
        KktFactor k(0, DensePolicy::kAlways);
        Index flagged = 0, refactors = 0, retakes = 0;
        {
            const DenseTallyScope tally(k, flagged, refactors, retakes);
            (void)factorize_checked(k, upper_of(m), site);
        }
        EXPECT_TRUE(k.dense_shifted);
        EXPECT_EQ(flagged, 1); // the original; the exactly singular first shift is a retry
        EXPECT_EQ(refactors, 2);
        EXPECT_EQ(k.shifted_exact_singular, 1);
        EXPECT_EQ(retakes, 0);
    }
    {
        using hven::linear::detail::testing::DenseFactorizeFaultInjector;
        KktFactor k(0, DensePolicy::kAlways);
        Index flagged = 0, refactors = 0, retakes = 0;
        hven::linear::FactorizeOutcome c;
        DenseFactorizeFaultInjector::pass = 1;
        DenseFactorizeFaultInjector::active = true;
        {
            const DenseTallyScope tally(k, flagged, refactors, retakes);
            c = factorize_checked(k, upper_of(m), site);
        }
        DenseFactorizeFaultInjector::active = false;
        EXPECT_FALSE(k.dense_shifted);
        EXPECT_EQ(c.arm, FactorArm::kSparse);
        EXPECT_EQ(flagged, 1);
        EXPECT_EQ(refactors, 2);
        EXPECT_EQ(retakes, 1);
    }
}

// THE RETRY ON A REAL MATRIX: a shifted factor flagged again. K = diag(P1, P2), all primal, every
// row's largest entry exactly 1 (so D = I): P1 = [[1, 1], [1, 1]] is exactly singular; P2 =
// [[1, 1], [1, 1 - 2s + 2^-52]] at s = 2^-30 has an eigenvalue ~ -s, so P2 + sI keeps a pivot of
// 2^-52 (rcond ~ 5e-17: flagged, not exact) and P2 + 256 s I is taken.
TEST(QpDenseArm, TheRetryAnswersAShiftThatIsItselfFlagged) {
    const double s = kDenseEquilibratedShift;
    Eigen::MatrixXd m = Eigen::MatrixXd::Zero(4, 4);
    m.topLeftCorner(2, 2).setOnes();
    m(2, 2) = 1.0;
    m(2, 3) = m(3, 2) = 1.0;
    m(3, 3) = 1.0 - 2.0 * s + std::ldexp(1.0, -52);
    const SpMatRM K = upper_of(m);
    ASSERT_EQ(equilibration_scale(K), Vec::Ones(4));
    KktFactor k(0, DensePolicy::kAlways);
    Index flagged = 0, refactors = 0, retakes = 0;
    {
        const DenseTallyScope tally(k, flagged, refactors, retakes);
        (void)factorize_checked(k, K, DenseShiftSite{4});
    }
    EXPECT_TRUE(k.dense_shifted);
    EXPECT_EQ(flagged, 1);
    EXPECT_EQ(refactors, 2);
    EXPECT_EQ(k.shifted_exact_singular, 0); // the first shift was flagged, not exactly singular
    EXPECT_EQ(retakes, 0);
    // One estimate: the exact singularity and the 256 s factor's D-ratio need none.
    EXPECT_EQ(k.factor.counters().dense_condition_estimate_count, 1);
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
    (void)factorize_checked(k, K, DenseShiftSite{3});
    ASSERT_TRUE(k.dense_shifted);
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

namespace {

// A NEARLY INCONSISTENT DUPLICATED ROW on a small right-hand side: x0 + x1 = 1e-3 and
// x0 + x1 = 1e-3 + 1e-9, g = (0, -1e-3, 0), at mu = 0. Every KKT matrix of the face is exactly
// singular, so the dense arm answers it equilibrated and shifted. The shifted answer meets each
// row to 5e-10 -- inside feas_tol = 1e-9 at the rows' unit scale -- but misses K by
// rho ~ 5e-7 > kEqpShiftedResidualTol: a refused answer every row-based acceptance would take.
QpProblem near_inconsistent_qp() {
    Eigen::MatrixXd Ae(2, 3);
    Ae << 1.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    Vec be(2);
    be << 1e-3, 1e-3 + 1e-9;
    QpProblem qp = rows_qp(Ae, be, 1.0);
    qp.g(1) = -1e-3;
    return qp;
}

QpOptions mu_free_opts() {
    QpOptions o;
    o.dual_regularization = 0.0;
    return o;
}

} // namespace

// The premise of the three fixtures below, at solve_eqp: refused by rho, and every row met.
TEST(QpDenseArm, ANearlyInconsistentFaceIsRefusedByRhoWithItsRowsMet) {
    const QpProblem qp = near_inconsistent_qp();
    const EqpRun d = eqp_on(DensePolicy::kAlways, qp, mu_free_opts());
    EXPECT_EQ(d.refactors, 1);
    EXPECT_EQ(d.retakes, 0);
    EXPECT_TRUE(d.res.shifted_refused);
    EXPECT_TRUE(d.res.residual_refused);
    ASSERT_TRUE(d.res.x.allFinite());
    const Vec r = qp.Ae * d.res.x - qp.be;
    EXPECT_LE(r.lpNorm<Eigen::Infinity>(), 1e-9);
}

// THE TERMINAL MU-FREE REFINEMENT does not adopt a refused shifted answer, though its
// row-and-bound adoption test would.
TEST(QpDenseArm, TheMuFreeTerminalRefinementRejectsARefusedShiftedAnswer) {
    const QpProblem qp = near_inconsistent_qp();
    const QpOptions opts; // the walk's options: refine_terminal_mu_free zeroes mu itself
    const QpEngine engine(opts, 0, DensePolicy::kAlways);
    const WorkingSet ws(3, 0);
    Vec x = Vec::Zero(3);
    Vec lambda_e = Vec::Zero(2);
    Vec lambda_i = Vec::Zero(0);
    Vec z = Vec::Zero(3);
    QpCounters counters;
    EXPECT_FALSE(QpEngineTestAccess::terminal_mu_free(engine, qp, ws, x, lambda_e, lambda_i, z,
                                                      counters, opts));
    EXPECT_EQ(x, Vec::Zero(3));
    EXPECT_EQ(counters.dense_shifted_solve_refusals, 1);
    EXPECT_EQ(counters.terminal_mu_refine_rescues, 0);
}

// THE TERMINAL POLISH does not adopt a refused shifted answer, though it improves every KKT
// residual of the incumbent (x = 0, rows violated by 1e-3).
TEST(QpDenseArm, TheTerminalPolishRejectsARefusedShiftedAnswer) {
    const QpProblem qp = near_inconsistent_qp();
    const QpOptions opts;
    const QpEngine engine(opts, 0, DensePolicy::kAlways);
    const WorkingSet ws(3, 0);
    Vec x = Vec::Zero(3);
    Vec lambda_e = Vec::Zero(2);
    Vec lambda_i = Vec::Zero(0);
    Vec z = Vec::Zero(3);
    QpCounters counters;
    EXPECT_FALSE(QpEngineTestAccess::terminal_polish(engine, qp, ws, x, lambda_e, lambda_i, z,
                                                     counters, opts));
    EXPECT_EQ(x, Vec::Zero(3));
    EXPECT_EQ(counters.dense_shifted_solve_refusals, 1);
    EXPECT_EQ(counters.terminal_polish_rejected, 1);
    EXPECT_EQ(counters.terminal_polish_adopted, 0);
}

// THE BORDER PATH'S SHIFTED-K0 GATE, REFUSING (the walk's default algebra): K0 is the face's
// whole KKT matrix, exactly singular, so it holds shifted numerics; the bordered answer meets the
// rows but misses the unshifted bordered system by rho ~ 5e-7, so it is refused and the pass falls
// to the eliminated path, whose solve_eqp refuses too and the walk escalates instead of stepping.
// On the refactorize algebra the same eliminated refusal, and no K0.
TEST(QpDenseArm, AShiftedK0BorderAnswerMissingRhoFallsToTheEliminatedPath) {
    const QpProblem qp = near_inconsistent_qp();
    for (const auto algebra :
         {QpWorkingSetAlgebra::kSchurBorder, QpWorkingSetAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == QpWorkingSetAlgebra::kSchurBorder ? "border" : "refactorize");
        QpOptions o = mu_free_opts();
        o.working_set_algebra = algebra;
        const QpEngine sparse_engine(o, 0, DensePolicy::kNever);
        const QpEngine dense_engine(o, 0, DensePolicy::kAlways);
        const QpSolution s = sparse_engine.solve(qp);
        const QpSolution d = dense_engine.solve(qp);
        EXPECT_EQ(s.counters.dense_shifted_solve_refusals, 0);
        EXPECT_EQ(s.counters.dense_shifted_k0_fallbacks, 0);
        EXPECT_EQ(d.counters.dense_platform_retakes, 0);
        if (algebra == QpWorkingSetAlgebra::kSchurBorder) {
            // Every border refusal is followed by the eliminated path's own refusal.
            EXPECT_GE(d.counters.dense_shifted_k0_fallbacks, 1);
            EXPECT_GE(d.counters.dense_shifted_solve_refusals,
                      2 * d.counters.dense_shifted_k0_fallbacks);
        } else {
            EXPECT_GE(d.counters.dense_shifted_solve_refusals, 1);
            EXPECT_EQ(d.counters.dense_shifted_k0_fallbacks, 0);
        }
        // AT THE EXHAUSTED LADDER the refused shifted answer is terminal, never stepped on: every
        // pass is refused, the ladder escalates to its last rung, and the walk ends
        // kNumericalError on the iterate it started from.
        EXPECT_EQ(static_cast<int>(d.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(d.counters.suspect_escalations, kMaxSuspectEscalations);
        EXPECT_EQ(d.counters.dense_shifted_terminations, 1);
        EXPECT_EQ(d.x, Vec::Zero(3));
        EXPECT_EQ(s.counters.dense_shifted_terminations, 0);
    }
}

// THE BORDER PATH'S SHIFTED-K0 GATE, PASSING: a consistent duplicated row at mu = 0 makes K0
// exactly singular, so it holds shifted numerics; the bordered answer solves the unshifted
// bordered system, is used (the border path's refinement runs, nothing falls back), and is the
// sparse arm's answer.
TEST(QpDenseArm, AShiftedK0BorderAnswerMeetingRhoIsUsed) {
    Eigen::MatrixXd Ae(2, 3);
    Ae << 1.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    const QpProblem qp = rows_qp(Ae, Vec::Constant(2, 1.0), 1.0);
    QpOptions o = mu_free_opts();
    o.working_set_algebra = QpWorkingSetAlgebra::kSchurBorder;
    const QpEngine sparse_engine(o, 0, DensePolicy::kNever);
    const QpEngine dense_engine(o, 0, DensePolicy::kAlways);
    const QpSolution s = sparse_engine.solve(qp);
    const QpSolution d = dense_engine.solve(qp);
    EXPECT_EQ(static_cast<int>(d.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_GE(d.counters.dense_singular_refactors, 1);
    EXPECT_EQ(d.counters.dense_platform_retakes, 0);
    EXPECT_GE(d.counters.border_refine_steps, 1);
    EXPECT_EQ(d.counters.dense_shifted_k0_fallbacks, 0);
    EXPECT_EQ(d.counters.dense_shifted_solve_refusals, 0);
    EXPECT_EQ(d.counters.eqp_residual_refusals, 0);
    EXPECT_NEAR(d.x(0) + d.x(1), 1.0, 1e-9);
    EXPECT_LE((d.x - s.x).lpNorm<Eigen::Infinity>(), 1e-6);
}

namespace {

// K0 = the free block of a QP with no rows (n0 = 3): [[1, 1, 0], [1, 1 - sigma, 0], [0, 0, 0]],
// sigma = kDenseEquilibratedShift. Ruiz leaves it unscaled (every nonzero row's max is 1); x3 is
// an exact zero column, so the factor is exactly singular and answered shifted; the leading block
// carries an eigenvalue ~ -sigma/2, which the +sigma shift turns into ~ +sigma/2. Along that
// eigenvector a step through the shifted factor maps the residual r to (1 - (-1)) r = 2r.
SpMatRM sign_flip_k0() {
    Eigen::MatrixXd m = Eigen::MatrixXd::Zero(3, 3);
    m(0, 0) = 1.0;
    m(0, 1) = m(1, 0) = 1.0;
    m(1, 1) = 1.0 - kDenseEquilibratedShift;
    return upper_of(m);
}

QpOptions no_reg_opts() {
    QpOptions o;
    o.primal_regularization = 0.0;
    o.dual_regularization = 0.0;
    return o;
}

} // namespace

// THE BORDER REFINEMENT'S FIRST STEP ON A SHIFTED K0 is kept only while its residual falls: from
// sol = 0 with rhs on the sign-flipped eigenvector, the step doubles the residual, so the answer
// stays where it was (and is the gated one). On an unshifted factor the step stays unconditional.
TEST(QpDenseArm, TheBorderRefinementKeepsAShiftedFirstStepOnlyWhileItFalls) {
    const SpMatRM K0 = sign_flip_k0();
    KktFactor k(0, DensePolicy::kAlways);
    (void)factorize_checked(k, K0, DenseShiftSite{3});
    ASSERT_TRUE(k.dense_shifted);
    const QpOptions opts = no_reg_opts();
    SchurComplement schur(k, opts);
    ASSERT_EQ(schur.dim(), 0);
    const Eigen::MatrixXd upper = Eigen::MatrixXd(K0);
    const Eigen::MatrixXd full = upper.selfadjointView<Eigen::Upper>();
    const Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(full);
    Index neg = 0;
    for (Index i = 1; i < 3; ++i) {
        if (es.eigenvalues()(i) < es.eigenvalues()(neg)) {
            neg = i;
        }
    }
    ASSERT_LT(es.eigenvalues()(neg), 0.0);
    const Vec rhs = es.eigenvectors().col(neg);
    const Vec sol = Vec::Zero(3);
    const std::vector<Vec> no_v;
    const std::vector<double> no_d;
    const auto res_inf = [&](const Vec &y) {
        return bordered_residual(K0, 3, no_v, no_d, rhs, y, opts).lpNorm<Eigen::Infinity>();
    };
    // The premise: the unconditional step doubles the residual.
    const Vec stepped = refine_bordered_solve(K0, 3, no_v, no_d, schur, rhs, sol, opts);
    ASSERT_GT(res_inf(stepped), 1.5 * res_inf(sol));
    Index steps = 0;
    const Vec kept = refine_bordered_solve_iterated(K0, 3, no_v, no_d, schur, rhs, sol, opts,
                                                    &steps, nullptr, nullptr, /*shifted_k0=*/true);
    EXPECT_EQ(kept, sol);
    EXPECT_EQ(steps, 1); // spent, not kept
    const Vec taken = refine_bordered_solve_iterated(K0, 3, no_v, no_d, schur, rhs, sol, opts,
                                                     &steps, nullptr, nullptr,
                                                     /*shifted_k0=*/false);
    EXPECT_EQ(taken, stepped);
}

// THE PER-BLOCK RATIO, directly: one K0 block in D's metric and one border block unscaled, each
// against its own rhs; a zero-rhs block against the whole rhs in its metric.
TEST(QpDenseArm, TheBorderedShiftedRatioReadsEachBlockAgainstItsOwnRhs) {
    Eigen::MatrixXd m = Eigen::MatrixXd::Identity(2, 2);
    const SpMatRM K0 = upper_of(m);
    const std::vector<Vec> v{Vec::Unit(2, 0)};
    const std::vector<double> bd{0.0};
    const Vec scale = Vec::Constant(2, 0.5);
    // K_b = [[1, 0, 1], [0, 1, 0], [1, 0, 0]]; the exact answer of rhs = (b0, b1, c) is
    // (c, b1, b0 - c).
    const auto exact = [](const Vec &rhs) {
        Vec y(3);
        y << rhs(2), rhs(1), rhs(0) - rhs(2);
        return y;
    };
    {
        // K0 rows large, border row small: a border-row miss of 1e-7 relative to its own rhs
        // refuses, though against the whole rhs it is 1e-13.
        Vec rhs(3);
        rhs << 1e6, 1e6, 1.0;
        Vec y = exact(rhs);
        EXPECT_LE(bordered_shifted_residual_ratio(K0, v, bd, rhs, y, scale), 1e-15);
        y(0) += 1e-7; // the border row reads x0: its residual is 1e-7; K0's row 0 moves by 1e-7
        const double rho = bordered_shifted_residual_ratio(K0, v, bd, rhs, y, scale);
        EXPECT_GT(rho, kEqpShiftedResidualTol);
        EXPECT_NEAR(rho, 1e-7, 1e-10);
    }
    {
        // A zero-rhs border block reads against the whole rhs (unscaled), and a zero-rhs K0
        // block against the whole rhs in D's metric (border rows weighted 1).
        Vec rhs(3);
        rhs << 4.0, 0.0, 0.0;
        Vec y = exact(rhs);
        y(0) += 1e-9; // the border row (x0) misses by 1e-9 against ||rhs|| = 4
        y(2) -= 1e-9; // K0 row 0 (x0 + border) is exact again: only the border block reads a miss
        // The border block alone: 1e-9 against the whole rhs, unscaled, 4.
        EXPECT_NEAR(bordered_shifted_residual_ratio(K0, v, bd, rhs, y, scale), 1e-9 / 4.0, 1e-16);
        Vec rhs2(3);
        rhs2 << 0.0, 0.0, 3.0;
        Vec y2 = exact(rhs2);
        y2(1) += 1e-9; // K0 row 1 residual 1e-9, scaled 0.5e-9, against max(0, 3) = 3
        EXPECT_NEAR(bordered_shifted_residual_ratio(K0, v, bd, rhs2, y2, scale), 0.5e-9 / 3.0,
                    1e-16);
    }
    {
        // No borders: the K0 block alone (C284's form).
        const std::vector<Vec> none;
        const std::vector<double> nd;
        Vec rhs(2);
        rhs << 2.0, 1.0;
        Vec y = rhs;
        y(1) += 1e-6;
        EXPECT_NEAR(bordered_shifted_residual_ratio(K0, none, nd, rhs, y, scale), 1e-6 / 2.0,
                    1e-12);
        EXPECT_EQ(bordered_shifted_residual_ratio(K0, none, nd, Vec::Zero(2), Vec::Zero(2), scale),
                  0.0);
        y(0) = std::numeric_limits<double>::quiet_NaN();
        EXPECT_TRUE(std::isnan(bordered_shifted_residual_ratio(K0, none, nd, rhs, y, scale)));
    }
    EXPECT_THROW(
        (void)bordered_shifted_residual_ratio(K0, v, bd, Vec::Zero(3), Vec::Zero(3), Vec::Ones(3)),
        std::invalid_argument);
}

namespace {

// Two duplicated equality rows x0 + x1 = 1 (consistent: K0 exactly singular at mu = 0, so it
// holds shifted numerics) and one inequality row a'x <= b the walk borders in.
QpProblem rows_plus_ineq_qp(double a0, double a1, double b) {
    Eigen::MatrixXd Ae(2, 3);
    Ae << 1.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    QpProblem qp = rows_qp(Ae, Vec::Constant(2, 1.0), 1.0);
    Eigen::MatrixXd Ai(1, 3);
    Ai << a0, a1, 0.0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec::Constant(1, b);
    return qp;
}

struct BorderRun {
    EqpResult res;
    Index ledger = 0;
    bool shifted = false;
    // The gate's two blocks on the first Schur solve, read separately (the gate's own
    // definitions: bordered_eqp.h's THE SHIFTED-K0 GATE): rho_k0 in K0's metric against its own
    // rhs, rho_border unscaled against the borders' own rhs.
    double rho_k0 = 0.0;
    double rho_border = 0.0;
};

// solve_bordered_eqp through a shifted K0 built on no working rows, with row 0 of Ai and a pin of
// x2 at its upper bound bordered in (m = 2).
BorderRun border_with_live_borders(const QpProblem &qp) {
    const QpOptions o = mu_free_opts();
    const QpEngine engine(o, 0, DensePolicy::kAlways);
    BorderState border(0, DensePolicy::kAlways);
    QpCounters counters;
    const WorkingSet k0_ws(3, qp.mi());
    WorkingSet ws(3, qp.mi());
    ws.add_ineq(0);
    ws.bound_state()[2] = BoundState::kAtUpper;
    QpEngineTestAccess::build_border(engine, qp, k0_ws, ws, border, counters, o);
    BorderRun r;
    r.ledger = static_cast<Index>(border.ledger.size());
    r.shifted = border.kkt.dense_shifted;
    r.res = solve_bordered_eqp(qp, border.k0, border.k0_rows, *border.schur, border.ledger, ws,
                               Vec::Zero(3), o, &border.kkt.dense_scale);
    const Vec x0 = Vec::Zero(3);
    const BorderedSystem sys =
        build_bordered_system(qp, border.k0, border.k0_rows, border.ledger, ws, x0, o);
    const Vec sol = border.schur->solve(sys.rhs);
    const Index n0 = border.k0.K.rows();
    const Vec res =
        bordered_regularized_residual(border.k0.K, 3, sys.border_v, sys.border_d, sys.rhs, sol);
    const Vec &d = border.kkt.dense_scale;
    r.rho_k0 = res.head(n0).cwiseProduct(d).lpNorm<Eigen::Infinity>() /
               sys.rhs.head(n0).cwiseProduct(d).lpNorm<Eigen::Infinity>();
    r.rho_border = res.tail(r.ledger).lpNorm<Eigen::Infinity>() /
                   sys.rhs.tail(r.ledger).lpNorm<Eigen::Infinity>();
    return r;
}

} // namespace

// THE BORDER GATE WITH LIVE BORDERS, PASSING: an independent inequality row x0 - x1 <= 0 and a
// pin, over a shifted K0; the bordered system is consistent and the answer is used.
TEST(QpDenseArm, AShiftedK0AnswerWithLiveBordersMeetingRhoIsUsed) {
    QpProblem qp = rows_plus_ineq_qp(1.0, -1.0, 0.0);
    qp.upper(2) = 0.25;
    const BorderRun r = border_with_live_borders(qp);
    ASSERT_TRUE(r.shifted);
    EXPECT_EQ(r.ledger, 2);
    EXPECT_FALSE(r.res.shifted_refused);
    EXPECT_FALSE(r.res.residual_refused);
    EXPECT_NEAR(r.res.x(0), 0.5, 1e-9);
    EXPECT_NEAR(r.res.x(1), 0.5, 1e-9);
    EXPECT_EQ(r.res.x(2), 0.25);
}

// THE BORDER GATE WITH LIVE BORDERS, REFUSING: the bordered inequality row x0 + x1 <= 1 - 1e-6,
// held at equality, contradicts the consistent duplicated rows K0 holds by 1e-6 -- the bordered
// system is inconsistent, so no answer solves it and the gate refuses.
TEST(QpDenseArm, AShiftedK0AnswerWithLiveBordersMissingRhoIsRefused) {
    QpProblem qp = rows_plus_ineq_qp(1.0, 1.0, 1.0 - 1e-6);
    qp.upper(2) = 0.25;
    const BorderRun r = border_with_live_borders(qp);
    ASSERT_TRUE(r.shifted);
    EXPECT_EQ(r.ledger, 2);
    EXPECT_TRUE(r.res.shifted_refused);
    EXPECT_TRUE(r.res.residual_refused);
}

// THE BORDER BLOCK ALONE DECIDES: a feasible bordered row x0 - x1 <= 1e-10 (held at equality,
// consistent with the duplicated rows) beside a pin at 0, so the borders' own rhs is 1e-10. K0
// meets rho (~5e-10 against 1e-8); the border row's residual is the rounding of an O(1) sum,
// ~1e-16 -- a miss only against its own tiny rhs (~2e-6). Without the border block's reading the
// answer would be used.
TEST(QpDenseArm, TheBorderBlockAloneCanRefuseAShiftedK0Answer) {
    QpProblem qp = rows_plus_ineq_qp(1.0, -1.0, 1e-10);
    qp.upper(2) = 0.0;
    const BorderRun r = border_with_live_borders(qp);
    ASSERT_TRUE(r.shifted);
    EXPECT_EQ(r.ledger, 2);
    EXPECT_LE(r.rho_k0, kEqpShiftedResidualTol);
    EXPECT_GT(r.rho_border, 10.0 * kEqpShiftedResidualTol);
    EXPECT_TRUE(r.res.shifted_refused);
    EXPECT_TRUE(r.res.residual_refused);
}

// THE SAME-ROWS RULE: a shifted K0 reports every pivot perturbed, yet on the same working rows it
// is never rebuilt. The walk below runs several border passes on no working rows (the pin of x1
// arrives as a border); K0 is factored once.
TEST(QpDenseArm, AShiftedK0OnTheSameRowsIsFactoredOnce) {
    Eigen::MatrixXd Ae(2, 3);
    Ae << 1.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    QpProblem qp = rows_qp(Ae, Vec::Constant(2, 1.0), 1.0);
    qp.upper(1) = 0.25;
    QpOptions o = mu_free_opts();
    o.working_set_algebra = QpWorkingSetAlgebra::kSchurBorder;
    const QpEngine engine(o, 0, DensePolicy::kAlways);
    const QpSolution d = engine.solve(qp);
    EXPECT_EQ(static_cast<int>(d.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_GE(d.counters.minor_iters, 2);
    EXPECT_GE(d.counters.border_refine_steps, 2);
    EXPECT_EQ(d.counters.dense_shifted_k0_fallbacks, 0);
    EXPECT_EQ(d.counters.dense_singular_refactors, 1);
    EXPECT_EQ(d.counters.factorizations, 1);
    EXPECT_NEAR(d.x(1), 0.25, 1e-12);
    EXPECT_NEAR(d.x(0), 0.75, 1e-9);
}

// THE VERDICT-SITE REFINEMENTS' REPLAY GATES. x2 is free, flat (H22 = 0, mu = 0) and in no row,
// so K's x2 row and column are exactly zero and its stationarity row g2 = -1e-6 is unanswerable:
// the shifted answer puts x2 at |g2|/sigma and misses K by rho ~ 3e-7 there, on every refinement
// step alike, so neither the solve's own refinement nor its step keeps anything and the face row
// x0 + x1 = 1 is left open by its single shifted solve (~1e-9 against a target of ~1e-11 at
// feasibility_tol = 1e-15). One verdict-site step closes that row (the premise, checked below on
// the eliminated twin's own system), so the walk-point test would ADOPT the refused answer; both
// replay gates refuse it first, and x is left alone.
namespace {

QpProblem flat_free_variable_qp() {
    Eigen::MatrixXd Ae(1, 3);
    Ae << 1.0, 1.0, 0.0;
    QpProblem qp = rows_qp(Ae, Vec::Constant(1, 1.0), 0.0);
    qp.g << 0.0, -3.0, -1e-6;
    qp.lower = Vec::Constant(3, -1e6);
    qp.upper = Vec::Constant(3, 1e6);
    return qp;
}

QpOptions flat_free_variable_opts() {
    QpOptions o = mu_free_opts();
    o.primal_regularization = 0.0;
    o.feasibility_tol = 1e-15;
    return o;
}

} // namespace

TEST(QpDenseArm, TheVerdictRefinementsRefuseAShiftedReplayThatMissesRho) {
    const QpProblem qp = flat_free_variable_qp();
    const QpOptions o = flat_free_variable_opts();
    const QpEngine engine(mu_free_opts(), 0, DensePolicy::kAlways);
    const WorkingSet ws(3, 0);
    {
        // THE PREMISE, on the system the eliminated twin re-forms: the replay is refused, leaves
        // the row open, and one step through the same factor closes it.
        const KktAssembly a = assemble_kkt(qp, ws, o);
        ASSERT_EQ(a.K.rows(), 4);
        KktFactor k(0, DensePolicy::kAlways);
        factorize_checked(k, a.K, DenseShiftSite{3});
        ASSERT_TRUE(k.dense_shifted);
        Vec rhs(4);
        rhs << -qp.g(0), -qp.g(1), -qp.g(2), qp.be(0);
        Vec y = solve_vec(k, rhs);
        EXPECT_EQ(refine_shifted_solve(a.K, rhs, k, y), 1); // spent, and not kept
        EXPECT_GT(shifted_residual_ratio(a.K, rhs, k, y), 10.0 * kEqpShiftedResidualTol);
        // face_row_target at this point: its 1e-14 floor of row_scale = |row|_1 * xmag = 2 |x2|.
        const double row_target = 1e-14 * 2.0 * std::abs(y(2));
        EXPECT_GT(std::abs(y(0) + y(1) - 1.0), 10.0 * row_target);
        const Vec stepped = y + solve_vec(k, rhs - a.K.selfadjointView<Eigen::Upper>() * y);
        EXPECT_LT(std::abs(stepped(0) + stepped(1) - 1.0), row_target);
    }
    {
        Vec x = Vec::Zero(3);
        QpCounters counters;
        EXPECT_FALSE(QpEngineTestAccess::eliminated_verdict_refine(engine, qp, ws, x, counters, o));
        EXPECT_EQ(x, Vec::Zero(3));
        EXPECT_EQ(counters.dense_singular_refactors, 1);
        EXPECT_EQ(counters.verdict_refine_steps, 0);
    }
    {
        BorderState border(0, DensePolicy::kAlways);
        QpCounters counters;
        QpEngineTestAccess::build_border(engine, qp, ws, ws, border, counters, o);
        ASSERT_TRUE(border.kkt.dense_shifted);
        Vec x = Vec::Zero(3);
        EXPECT_FALSE(
            QpEngineTestAccess::border_verdict_refine(engine, qp, ws, x, border, counters, o));
        EXPECT_EQ(x, Vec::Zero(3));
        EXPECT_EQ(counters.verdict_refine_steps, 0);
    }
}

// THE BORDER VERDICT GATE WITH LIVE BORDERS (m = 2), AND ITS PLACEMENT. The bordered rows
// 3e-6 x1 + x2 <= 0.25 + 1e-3 (held at equality) and the pin x2 = 0.25 are nearly parallel, so the
// Schur complement is ill-conditioned (its guard is raised here) and the FIRST Schur solve misses
// rho (~1e-7 against 1e-8); the shifted iterated refinement brings the replay under rho (checked
// below). Large multipliers (g0 = g1 = 1e8) keep the face rows open at the refinement's noise
// floor, so one verdict step then closes them. The gate is on the first solve, before refinement:
// the answer is refused and x is left alone. A gate placed after the refinement would adopt it (x0,
// x1 ~ -332, 333; mutation-checked).
TEST(QpDenseArm, TheBorderVerdictGateRefusesALiveBorderReplayThatOnlyRefinementRepairs) {
    QpProblem qp = rows_plus_ineq_qp(0.0, 0.0, 0.25 + 1e-3);
    Eigen::MatrixXd Ai(1, 3);
    Ai << 0.0, 3e-6, 1.0;
    qp.Ai = Ai.sparseView();
    qp.g(0) = 1e8;
    qp.g(1) = 1e8;
    qp.upper(2) = 0.25;
    QpOptions o = mu_free_opts();
    o.schur_condition_max = 1e16;
    o.feasibility_tol = 1e-15; // the face rows' target stays tight enough to leave the face open
    const QpEngine engine(o, 0, DensePolicy::kAlways);
    BorderState border(0, DensePolicy::kAlways);
    QpCounters counters;
    const WorkingSet k0_ws(3, qp.mi());
    WorkingSet ws(3, qp.mi());
    ws.add_ineq(0);
    ws.bound_state()[2] = BoundState::kAtUpper;
    QpEngineTestAccess::build_border(engine, qp, k0_ws, ws, border, counters, o);
    ASSERT_TRUE(border.kkt.dense_shifted);
    ASSERT_EQ(border.ledger.size(), 2U);
    Vec x = Vec::Zero(3);
    {
        // THE PREMISE: first solve misses rho, the refined replay meets it.
        const BorderedSystem sys =
            build_bordered_system(qp, border.k0, border.k0_rows, border.ledger, ws, x, o);
        const Vec first = border.schur->solve(sys.rhs);
        const Vec refined = refine_bordered_solve_iterated(
            border.k0.K, 3, sys.border_v, sys.border_d, *border.schur, sys.rhs, first, o, nullptr,
            nullptr, nullptr, true);
        const double rho_first = bordered_shifted_residual_ratio(
            border.k0.K, sys.border_v, sys.border_d, sys.rhs, first, border.kkt.dense_scale);
        const double rho_refined = bordered_shifted_residual_ratio(
            border.k0.K, sys.border_v, sys.border_d, sys.rhs, refined, border.kkt.dense_scale);
        EXPECT_GT(rho_first, kEqpShiftedResidualTol);
        EXPECT_LE(rho_refined, kEqpShiftedResidualTol);
    }
    EXPECT_FALSE(QpEngineTestAccess::border_verdict_refine(engine, qp, ws, x, border, counters, o));
    EXPECT_EQ(x, Vec::Zero(3));
    EXPECT_EQ(counters.verdict_refine_steps, 0);
}

// THE TERMINAL POLISH'S RHO REJECTION, on its own: the incumbent carries multipliers large enough
// that the polish's adoption test WOULD take the refused shifted answer (the premise, checked
// directly); the rho rejection is what keeps it out.
TEST(QpDenseArm, TheTerminalPolishRhoRejectionHoldsWhereAdoptionWouldSucceed) {
    const QpProblem qp = near_inconsistent_qp();
    const QpOptions opts;
    const QpEngine engine(opts, 0, DensePolicy::kAlways);
    const WorkingSet ws(3, 0);
    const Vec x0 = Vec::Zero(3);
    const Vec le0 = Vec::Constant(2, 1e3);
    const Vec li0 = Vec::Zero(0);
    const Vec z0 = Vec::Zero(3);
    const EqpRun d = eqp_on(DensePolicy::kAlways, qp, mu_free_opts());
    ASSERT_TRUE(d.res.shifted_refused);
    ASSERT_TRUE(
        QpEngineTestAccess::polish_would_adopt(engine, qp, ws, d.res, x0, le0, li0, z0, opts));
    Vec x = x0;
    Vec lambda_e = le0;
    Vec lambda_i = li0;
    Vec z = z0;
    QpCounters counters;
    EXPECT_FALSE(QpEngineTestAccess::terminal_polish(engine, qp, ws, x, lambda_e, lambda_i, z,
                                                     counters, opts));
    EXPECT_EQ(x, x0);
    EXPECT_EQ(lambda_e, le0);
    EXPECT_EQ(counters.terminal_polish_rejected, 1);
    EXPECT_EQ(counters.terminal_polish_adopted, 0);
}

// A NaN AT A NON-FIRST INDEX REFUSES (inf_norm.h): every gate and keep-while-falling monitor
// reads its residual NaN-sticky, so a NaN past element 0 -- in the middle, or last, past every
// packet width -- refuses or stops exactly as one in element 0 does. Sizes 9 and 17 put the last
// index in the scalar tail of the 2-, 4- and 8-lane reductions.
namespace {

constexpr Index kNanSizes[] = {9, 17};

std::vector<Index> nan_indices(Index n) { return {1, n / 2, n - 1}; }

// A factor of the diagonal 2*I on the platform session, whose sparse factor keeps a NaN where it
// was put; `shifted` marks it as dense shifted numerics with D = 1, which is all the shifted
// gates and refine_shifted_solve read.
KktFactor diagonal_factor(const SpMatRM &K, bool shifted) {
    KktFactor k(0, DensePolicy::kNever);
    (void)factorize_checked(k, K, DenseShiftSite{K.rows()});
    k.dense_shifted = shifted;
    k.dense_scale = shifted ? Vec(Vec::Ones(K.rows())) : Vec();
    return k;
}

} // namespace

TEST(QpNanGates, TheShiftedRatioReadsANonFirstNaNAndTheGateRefuses) {
    for (const Index n : kNanSizes) {
        const SpMatRM K = upper_of(2.0 * Eigen::MatrixXd::Identity(n, n));
        const KktFactor k = diagonal_factor(K, true);
        const Vec rhs = Vec::Constant(n, 2.0);
        ASSERT_EQ(shifted_residual_ratio(K, rhs, k, Vec::Ones(n)), 0.0);
        ASSERT_FALSE(shifted_solve_refused(K, rhs, k, Vec::Ones(n)));
        for (const Index j : nan_indices(n)) {
            Vec y = Vec::Ones(n);
            y(j) = std::numeric_limits<double>::quiet_NaN();
            EXPECT_TRUE(std::isnan(shifted_residual_ratio(K, rhs, k, y))) << n << " " << j;
            EXPECT_TRUE(shifted_solve_refused(K, rhs, k, y)) << n << " " << j;
        }
    }
}

TEST(QpNanGates, TheBorderedShiftedRatioReadsANonFirstNaN) {
    for (const Index n : kNanSizes) {
        const SpMatRM K0 = upper_of(Eigen::MatrixXd::Identity(n, n));
        const Vec scale = Vec::Constant(n, 0.5);
        const std::vector<Vec> none;
        const std::vector<double> nd;
        const std::vector<Vec> v{Vec::Unit(n, 0)};
        const std::vector<double> bd{0.0};
        Vec rhs = Vec::Constant(n, 1.0);
        ASSERT_EQ(bordered_shifted_residual_ratio(K0, none, nd, rhs, rhs, scale), 0.0);
        // With one border reading x0: rhs (1, ..., 1, c), exact answer (c, 1, ..., 1, 1 - c).
        Vec rhs_b(n + 1);
        rhs_b << rhs, 0.25;
        Vec exact_b(n + 1);
        exact_b << rhs, 0.75;
        exact_b(0) = 0.25;
        ASSERT_LE(bordered_shifted_residual_ratio(K0, v, bd, rhs_b, exact_b, scale), 1e-15);
        for (const Index j : nan_indices(n)) {
            Vec y = rhs;
            y(j) = std::numeric_limits<double>::quiet_NaN();
            EXPECT_TRUE(std::isnan(bordered_shifted_residual_ratio(K0, none, nd, rhs, y, scale)))
                << n << " " << j;
            Vec yb = exact_b;
            yb(j) = std::numeric_limits<double>::quiet_NaN();
            EXPECT_TRUE(std::isnan(bordered_shifted_residual_ratio(K0, v, bd, rhs_b, yb, scale)))
                << n << " " << j;
        }
        // The border block's own entry, last of all.
        Vec yb = exact_b;
        yb(n) = std::numeric_limits<double>::quiet_NaN();
        EXPECT_TRUE(std::isnan(bordered_shifted_residual_ratio(K0, v, bd, rhs_b, yb, scale)));
    }
}

TEST(QpNanGates, TheShiftedRefinementMonitorDoesNotFallToANonFirstNaN) {
    for (const Index n : kNanSizes) {
        const SpMatRM K = upper_of(2.0 * Eigen::MatrixXd::Identity(n, n));
        const KktFactor k = diagonal_factor(K, true);
        const Vec rhs = Vec::Constant(n, 2.0);
        {
            // The premise, finite: from 0.5 one step lands on the answer and is kept.
            Vec y = Vec::Constant(n, 0.5);
            EXPECT_EQ(refine_shifted_solve(K, rhs, k, y), 2);
            EXPECT_EQ(y, Vec::Ones(n));
        }
        for (const Index j : nan_indices(n)) {
            // The same start with a NaN at j: the residual reads NaN, so the first step's
            // candidate cannot read as falling and nothing is kept.
            Vec y0 = Vec::Constant(n, 0.5);
            y0(j) = std::numeric_limits<double>::quiet_NaN();
            Vec y = y0;
            EXPECT_EQ(refine_shifted_solve(K, rhs, k, y), 1) << n << " " << j;
            for (Index i = 0; i < n; ++i) {
                if (i != j) {
                    EXPECT_EQ(y(i), y0(i)) << n << " " << j << " " << i;
                }
            }
        }
    }
}

TEST(QpNanGates, TheUnregularizedPolishDoesNotFallToANonFirstNaN) {
    for (const Index n : kNanSizes) {
        KktAssembly a;
        a.K = upper_of(2.0 * Eigen::MatrixXd::Identity(n, n));
        a.rhs_shift = Vec::Zero(n);
        KktFactor k = diagonal_factor(a.K, false);
        QpOptions o;
        o.primal_regularization = 0.0;
        o.dual_regularization = 0.0;
        const Vec rhs0 = Vec::Constant(n, 2.0);
        {
            // The premise, finite: a 1e-8 miss is polished away.
            Vec y = Vec::Constant(n, 1.0 + 1e-8);
            EXPECT_GE(polish_eqp_core(a, rhs0, o, k, n, 0, kEqpPolishSteps, y), 1);
        }
        for (const Index j : nan_indices(n)) {
            Vec y0 = Vec::Constant(n, 1.0 + 1e-8);
            y0(j) = std::numeric_limits<double>::quiet_NaN();
            Vec y = y0;
            EXPECT_EQ(polish_eqp_core(a, rhs0, o, k, n, 0, kEqpPolishSteps, y), 0) << n << " " << j;
            for (Index i = 0; i < n; ++i) {
                if (i != j) {
                    EXPECT_EQ(y(i), y0(i)) << n << " " << j << " " << i;
                }
            }
        }
    }
}

TEST(QpNanGates, TheVerdictFaceMeasureKeepsANaNInALaterRow) {
    // Three equality rows x_i = 1; a NaN in x reaches only its own row's residual.
    const Eigen::MatrixXd Ae = Eigen::MatrixXd::Identity(3, 3);
    const QpProblem qp = rows_qp(Ae, Vec::Ones(3), 1.0);
    const QpOptions o;
    const QpEngine engine(o, 0, DensePolicy::kNever);
    const WorkingSet ws(3, qp.mi());
    ASSERT_EQ(QpEngineTestAccess::face_measure(engine, qp, ws, Vec::Ones(3), o), 0.0);
    for (const Index j : {1, 2}) {
        Vec x = Vec::Ones(3);
        x(j) = std::numeric_limits<double>::quiet_NaN();
        EXPECT_TRUE(std::isnan(QpEngineTestAccess::face_measure(engine, qp, ws, x, o))) << j;
    }
}
