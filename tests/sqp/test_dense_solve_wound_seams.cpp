// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// THE SSN TIER'S DENSE SOLVE GATES UNDER A WOUNDED FACTOR (C341, the dense-solve wound seam).
// No correct dense factorization produces a Newton step whose normwise backward error exceeds
// 1e-8 (the M8 dense gate's argument), so the two refusal branches -- an unshifted step the dense
// solve check refuses, a shifted step the shifted gate refuses -- and the proximal ladder that
// answers both are reached through DenseSolveFaultInjector (hven/detail/linear/fault_injection.h):
// a wounded set of numerics answers every solve times `scale`, the check's refinement solves
// included, so refinement cannot heal it.
//
// Also here (C341 F4): a step from a shifted factor is never exempt from the merit test.

#include <gtest/gtest.h>

#include <hven/detail/linear/fault_injection.h>
#include <hven/detail/qp/ssn_engine.h>

namespace {

using namespace hven;
using namespace hven::detail;
using hven::linear::DensePolicy;
using Wound = hven::linear::detail::testing::DenseSolveFaultInjector;

// min 1/2 ||x||^2 - x1 - 2 x2 s.t. x0 + x1 + x2 = 1 (and, when `duplicate`, the same row again:
// every KKT matrix is then singular whatever its scaling, so the dense arm answers it shifted),
// x1 <= 0.25, box [-10, 10]^3.
QpProblem small_qp(bool duplicate, double second_rhs = 1.0) {
    QpProblem qp;
    qp.H = Eigen::MatrixXd::Identity(3, 3).sparseView();
    qp.g = Vec::Zero(3);
    qp.g << 0.0, -1.0, -2.0;
    Eigen::MatrixXd Ae(duplicate ? 2 : 1, 3);
    if (duplicate) {
        Ae << 1.0, 1.0, 1.0, 1.0, 1.0, 1.0;
        qp.be = Vec(2);
        qp.be << 1.0, second_rhs;
    } else {
        Ae << 1.0, 1.0, 1.0;
        qp.be = Vec::Constant(1, 1.0);
    }
    qp.Ae = Ae.sparseView();
    Eigen::MatrixXd Ai(1, 3);
    Ai << 0.0, 1.0, 0.0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec::Constant(1, 0.25);
    qp.lower = Vec::Constant(3, -10.0);
    qp.upper = Vec::Constant(3, 10.0);
    return qp;
}

// At mu = 0, so the duplicated rows' KKT carries no dual regularization to make it decidable.
SsnResult ssn_solve(const QpProblem &qp, const SsnOptions &so = SsnOptions{}) {
    QpOptions o;
    o.dual_regularization = 0.0;
    SsnEngine engine(o, 0, DensePolicy::kAlways);
    SsnResult r;
    engine.solve(qp, SsnStart{}, so, &r);
    return r;
}

void arm(int max_numerics, double scale = -1.0) {
    Wound::reset();
    Wound::max_numerics = max_numerics;
    Wound::scale = scale;
    Wound::active = true;
}

class SsnDenseSolveWound : public ::testing::Test {
  protected:
    void TearDown() override { Wound::reset(); }
};

} // namespace

// The control: unwounded, the dense arm solves both fixtures and refuses nothing; the duplicated
// rows' KKT is shifted, the single row's is not.
TEST_F(SsnDenseSolveWound, TheControlsRefuseNothing) {
    const SsnResult a = ssn_solve(small_qp(false));
    EXPECT_EQ(a.status, QpStatus::kOptimal);
    EXPECT_EQ(a.dense_solve_residual_refusals, 0);
    EXPECT_EQ(a.dense_shifted_solve_refusals, 0);
    EXPECT_EQ(a.dense_singular_refactors, 0);
    const SsnResult b = ssn_solve(small_qp(true));
    EXPECT_EQ(b.status, QpStatus::kOptimal);
    EXPECT_GE(b.dense_singular_refactors, 1);
    EXPECT_EQ(b.dense_shifted_solve_refusals, 0);
    EXPECT_EQ(b.dense_solve_residual_refusals, 0);
}

// AN UNSHIFTED STEP REFUSED: the first Newton step's numerics wounded -- the check's refinement
// cannot heal a sign flip -- so the step is refused, the proximal ladder takes one rung, and the
// next factorization (healthy) finishes the solve at the control's answer.
TEST_F(SsnDenseSolveWound, ARefusedUnshiftedStepTakesOneProximalRung) {
    const SsnResult base = ssn_solve(small_qp(false));
    arm(1);
    const SsnResult r = ssn_solve(small_qp(false));
    EXPECT_EQ(Wound::numerics_wounded, 1);
    EXPECT_GE(Wound::solves_wounded, 2); // the step and the refinement it spent
    EXPECT_EQ(r.dense_solve_residual_refusals, 1);
    EXPECT_EQ(r.dense_shifted_solve_refusals, 0);
    EXPECT_EQ(r.counters.ssn_prox_updates, base.counters.ssn_prox_updates + 1);
    EXPECT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_LE((r.x - base.x).lpNorm<Eigen::Infinity>(), 1e-6);
}

// EVERY STEP REFUSED: one rung per refusal, and kSingular once the ladder is at its ceiling; no
// step is ever taken.
TEST_F(SsnDenseSolveWound, RefusalsUntilTheLadderIsExhaustedEndTheTierSingular) {
    arm(-1);
    const SsnResult r = ssn_solve(small_qp(false));
    EXPECT_EQ(r.status, QpStatus::kNumericalError);
    EXPECT_EQ(r.escape_reason, SsnEscape::kSingular);
    EXPECT_EQ(r.iters, 0);
    EXPECT_GE(r.dense_solve_residual_refusals, 2);
    EXPECT_EQ(r.counters.ssn_prox_updates, r.dense_solve_residual_refusals - 1);
    EXPECT_EQ(Wound::numerics_wounded, r.dense_solve_residual_refusals);
}

// A SHIFTED STEP REFUSED: on the duplicated rows every factorization is a shift response; the
// wounded one's step fails the shifted gate (its own refinement keeps nothing), the proximal ladder
// takes one rung, and the solve finishes.
TEST_F(SsnDenseSolveWound, ARefusedShiftedStepTakesOneProximalRung) {
    const SsnResult base = ssn_solve(small_qp(true));
    arm(1);
    const SsnResult r = ssn_solve(small_qp(true));
    EXPECT_EQ(Wound::numerics_wounded, 1);
    EXPECT_EQ(r.dense_shifted_solve_refusals, 1);
    EXPECT_EQ(r.dense_solve_residual_refusals, 0);
    EXPECT_EQ(r.counters.ssn_prox_updates, base.counters.ssn_prox_updates + 1);
    EXPECT_EQ(r.status, QpStatus::kOptimal);
    EXPECT_LE((r.x - base.x).lpNorm<Eigen::Infinity>(), 1e-6);
}

// A STEP FROM A SHIFTED FACTOR FACES THE MERIT TEST (C341 F4), even where every other step is
// exempt: under SsnSafeguards::kBare no step is line-searched -- the sparse arm's solve of the
// inconsistent duplicated rows (x0 + x1 + x2 = 1 and = 1.5) backtracks nothing -- but the dense
// arm's steps on the same rows come from shifted factors, and they backtrack.
TEST(SsnShiftedStep, AShiftedStepIsNeverExemptFromTheMeritTest) {
    QpOptions o;
    o.dual_regularization = 0.0;
    SsnOptions so;
    so.safeguards = SsnSafeguards::kBare;
    const QpProblem qp = small_qp(true, 1.5);
    SsnEngine sparse(o, 0, DensePolicy::kNever);
    SsnResult rs;
    sparse.solve(qp, SsnStart{}, so, &rs);
    // A sparse factor the platform perturbed is refactored equilibrated (M8 sparse-equil) and its
    // steps are gated like a shifted one's: not exempt either. Exempt iff nothing was refactored.
    if (rs.sparse_equilibrated_refactors > 0) {
        EXPECT_GT(rs.counters.ssn_backtracks, 0);
    } else {
        EXPECT_EQ(rs.counters.ssn_backtracks, 0);
    }
    EXPECT_EQ(rs.dense_singular_refactors, 0);
    SsnEngine dense(o, 0, DensePolicy::kAlways);
    SsnResult rd;
    dense.solve(qp, SsnStart{}, so, &rd);
    EXPECT_GE(rd.dense_singular_refactors, 1);
    EXPECT_GT(rd.counters.ssn_backtracks, 0);
}
