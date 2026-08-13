// test_qp_engine_tr.cpp — Task 9: the l-infinity trust-region soft-bound
// block (qp_engine.h "Section 6"), the interface the Phase-3 SQP driver needs
// from this QP engine.
//
// Effective bounds lo_eff = max(lower, x0 - Delta), up_eff = min(upper,
// x0 + Delta) are computed once, about the solve's OWN start point x0, and
// participate in the ratio test / pins exactly like real bounds. What makes
// them a SEPARATE concept from a real bound is entirely in the reporting:
// QpSolution::tr_active marks which indices were pinned by the TR side
// rather than the real one, bound_state reports such a variable kFree (never
// kAtLower/kAtUpper/kFixed), z reports 0 there (TR duals are internal, never
// exposed -- a caller reads tr_active to detect a binding radius), and
// warm-start seed ingestion never carries a TR pin into the next solve (see
// (d) below).
//
// tr_radius defaults to +inf (off). (a) is the guard for the header
// contract's BIT-IDENTICAL OFF PATH claim: every one of the other 108 tests
// in this suite already exercises that default with the field untouched, so
// this file only needs one fixture solved both ways to pin the claim itself.

#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/sqp/qp_engine.h>

#include "support/dense_oracle.h"

using namespace hven::solvers;

namespace {

constexpr WorkingSetLinearAlgebra kBothModes[] = {WorkingSetLinearAlgebra::kSchurBorder,
                                                  WorkingSetLinearAlgebra::kRefactorize};

// --- Battery fixtures -----------------------------------------------------

// min 1/2(x0^2 + x1^2) - x0 - 2 x1  s.t. x0 + x1 <= 1, 0 <= x <= 10. Repeated
// verbatim from test_qp_engine.cpp so this file stays self-contained (see
// that file's own note on the convention).
QpProblem simple_box_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

// min 1/2||x - (10,10)||^2, genuinely unconstrained: lower/upper are both
// beyond the +/-1e20 "effectively infinite" convention, and there are no
// general rows at all. Cold start clamp(0, l, u) = (0,0).
QpProblem unconstrained_quadratic_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -10.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

// Same objective as unconstrained_quadratic_qp, but with a REAL bound on x0
// (upper(0) = 0.5) tighter than a tr_radius of 1, and a real upper(1) = 10
// looser than that same radius -- so one variable's pin is real-bound-caused
// and the other's is TR-caused, on the same solve.
QpProblem tighter_real_bound_on_x0_qp() {
    QpProblem qp = unconstrained_quadratic_qp();
    qp.upper(0) = 0.5;
    qp.upper(1) = 10.0;
    return qp;
}

// Same objective again, but with FINITE, loose real bounds (+/-100) on both
// variables -- loose enough that tr_radius in {1, 5} below always binds
// first, but finite enough that start_point()'s seed-ingestion guard
// (`up < kEngineInfBound`) cannot itself suppress a wrongly-carried-over
// TR pin the way it trivially would under +/-1e20 bounds. This is what makes
// (d) below a real trap for a seed-ingestion bug, not just a restatement of
// an unrelated guard.
QpProblem loosely_bounded_quadratic_qp() {
    QpProblem qp = unconstrained_quadratic_qp();
    qp.lower = Vec::Constant(2, -100.0);
    qp.upper = Vec::Constant(2, 100.0);
    return qp;
}

// min 1/2(x0^2 - x1^2) over [-1,1]^2 (test_qp_engine_indefinite.cpp's
// fixture, repeated verbatim per this suite's self-contained-file
// convention). H = diag(1, -1), g = 0. Unconstrained stationary point is the
// SADDLE (0,0); the box turns it into two true minimizers x = (0, +/-1).
QpProblem saddle_box_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(2, 2);
    Hd(0, 0) = 1.0;
    Hd(1, 1) = -1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1.0);
    qp.upper = Vec::Constant(2, 1.0);
    return qp;
}

void expect_all_free_and_untracked(const QpSolution &sol, Index n) {
    ASSERT_EQ(static_cast<Index>(sol.bound_state.size()), n);
    ASSERT_EQ(static_cast<Index>(sol.tr_active.size()), n);
    for (Index i = 0; i < n; ++i) {
        EXPECT_EQ(sol.bound_state[static_cast<std::size_t>(i)], BoundState::kFree);
    }
}

// min 1/2(x0^2 + x1^2) - 0.7 x0, genuinely unconstrained (real bounds beyond
// +/-1e20): the unconstrained minimizer is x0* = 0.7, x1* = 0. Kept in this
// anonymous namespace (internal linkage) rather than at file scope: a
// same-named fixture helper defined the same way in another translation unit
// (e.g. a Task-6 test file) would otherwise be a duplicate-symbol link error
// once both object files are linked into one test binary.
QpProblem shrink_retry_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -0.7, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

} // namespace

// (a) QpOptions{}.tr_radius is +inf by construction, and a solve with the
// field left untouched must be byte-for-byte identical to one with it set to
// std::numeric_limits<double>::infinity() explicitly -- the header
// contract's BIT-IDENTICAL OFF PATH claim.
TEST(QpEngineTr, TrOffIsBitIdenticalDefault) {
    EXPECT_EQ(QpOptions{}.tr_radius, std::numeric_limits<double>::infinity());

    const QpProblem qp = simple_box_qp();
    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions default_opts;
        default_opts.ws_algebra = algebra;

        QpOptions explicit_inf_opts;
        explicit_inf_opts.ws_algebra = algebra;
        explicit_inf_opts.tr_radius = std::numeric_limits<double>::infinity();

        QpEngine eng_default{default_opts};
        QpEngine eng_explicit{explicit_inf_opts};
        const QpSolution a = eng_default.solve(qp);
        const QpSolution b = eng_explicit.solve(qp);

        EXPECT_EQ(a.status, b.status);
        EXPECT_TRUE((a.x - b.x).isZero(0.0));
        EXPECT_TRUE((a.z - b.z).isZero(0.0));
        EXPECT_TRUE((a.lambda_e - b.lambda_e).isZero(0.0));
        EXPECT_TRUE((a.lambda_i - b.lambda_i).isZero(0.0));
        EXPECT_EQ(a.bound_state, b.bound_state);
        EXPECT_EQ(a.ineq_active, b.ineq_active);
        EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
        EXPECT_EQ(a.counters.schur_updates, b.counters.schur_updates);
        EXPECT_EQ(a.counters.minor_iters, b.counters.minor_iters);
    }
}

// (b) min 1/2||x - (10,10)||^2 from x0 = 0 (unconstrained), tr_radius = 1:
// the true optimum (10,10) lies far outside the TR window [-1,1]^2 built
// about x0, so the engine must clamp to the window's corner (1,1). Neither
// variable has any real bound at all, so both pins are entirely TR-caused.
TEST(QpEngineTr, TrClampsTheStep) {
    const QpProblem qp = unconstrained_quadratic_qp();
    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions opts;
        opts.ws_algebra = algebra;
        opts.tr_radius = 1.0;
        QpEngine eng{opts};
        const QpSolution sol = eng.solve(qp);

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 1.0, 1e-8);
        EXPECT_NEAR(sol.x(1), 1.0, 1e-8);

        ASSERT_EQ(sol.tr_active.size(), 2u);
        EXPECT_TRUE(sol.tr_active[0]);
        EXPECT_TRUE(sol.tr_active[1]);
        expect_all_free_and_untracked(sol, 2);

        EXPECT_EQ(sol.z(0), 0.0);
        EXPECT_EQ(sol.z(1), 0.0);
    }
}

// (c) Same objective, but x0's real upper bound (0.5) is tighter than a
// tr_radius of 1 while x1's real upper bound (10) is looser than it. x0 must
// report its REAL bound normally (kAtUpper, tr_active false, a genuinely
// priced nonzero z); x1 must report the TR exclusion (kFree, tr_active true,
// z forced to 0).
TEST(QpEngineTr, TrInsideRealBounds) {
    const QpProblem qp = tighter_real_bound_on_x0_qp();
    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions opts;
        opts.ws_algebra = algebra;
        opts.tr_radius = 1.0;
        QpEngine eng{opts};
        const QpSolution sol = eng.solve(qp);

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 0.5, 1e-8);
        EXPECT_NEAR(sol.x(1), 1.0, 1e-8);

        ASSERT_EQ(sol.tr_active.size(), 2u);
        EXPECT_FALSE(sol.tr_active[0]);
        EXPECT_TRUE(sol.tr_active[1]);

        EXPECT_EQ(sol.bound_state[0], BoundState::kAtUpper);
        EXPECT_EQ(sol.bound_state[1], BoundState::kFree);

        // Real-bound multiplier is genuinely priced (z <= 0 at an active
        // upper bound; grad = x - 10 = -9.5 there).
        EXPECT_NEAR(sol.z(0), -9.5, 1e-6);
        // TR dual is internal, never exposed.
        EXPECT_EQ(sol.z(1), 0.0);
    }
}

// (d) A TR-clamped solve's own working set must not leak into a warm
// re-solve with a DIFFERENT (larger) radius: the seed's TR pins are reported
// kFree (rule (a)), so start_point()'s seed-ingestion sees nothing to pin,
// and the re-solve's own effective bounds are recomputed about ITS start
// point (the seed's x, clamped by the real bounds) with the NEW radius.
//
// tr_radius lives on QpOptions, fixed per QpEngine instance, so varying it
// across the cold/warm pair requires two engine instances -- this exercises
// seed ingestion (start_point), not this engine's own hot-start K0 reuse
// (which is instance-local and orthogonal to what this test checks; see the
// header contract's HOT-START REUSE INTERACTION note).
TEST(QpEngineTr, WarmSeedIgnoresTrActivity) {
    const QpProblem qp = loosely_bounded_quadratic_qp();
    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions cold_opts;
        cold_opts.ws_algebra = algebra;
        cold_opts.tr_radius = 1.0;
        QpEngine cold_eng{cold_opts};
        const QpSolution cold = cold_eng.solve(qp);

        ASSERT_EQ(cold.status, QpStatus::kOptimal);
        EXPECT_NEAR(cold.x(0), 1.0, 1e-8);
        EXPECT_NEAR(cold.x(1), 1.0, 1e-8);
        ASSERT_EQ(cold.tr_active.size(), 2u);
        EXPECT_TRUE(cold.tr_active[0]);
        EXPECT_TRUE(cold.tr_active[1]);
        expect_all_free_and_untracked(cold, 2);

        QpOptions warm_opts;
        warm_opts.ws_algebra = algebra;
        warm_opts.tr_radius = 5.0;
        QpEngine warm_eng{warm_opts};
        const QpSolution warm = warm_eng.solve(qp, cold);

        // New x0 = clamp(cold.x = (1,1), real bounds [-100,100]) = (1,1).
        // New window: [1-5, 1+5] = [-4, 6], intersected with the real
        // [-100,100] box -> [-4, 6] (TR tighter on both sides here). The
        // true optimum (10,10) is still outside it, so the re-solve moves
        // PAST the old radius's corner (1,1) to the new one, (6,6).
        EXPECT_EQ(warm.status, QpStatus::kOptimal);
        EXPECT_NEAR(warm.x(0), 6.0, 1e-8);
        EXPECT_NEAR(warm.x(1), 6.0, 1e-8);
        ASSERT_EQ(warm.tr_active.size(), 2u);
        EXPECT_TRUE(warm.tr_active[0]);
        EXPECT_TRUE(warm.tr_active[1]);
        expect_all_free_and_untracked(warm, 2);

        // A buggy seed ingestion that resurrected the old kAtUpper pin would
        // pin x at the OLD, wrong location before the ratio test ever runs,
        // collapsing this to a near-trivial (and wrongly-answered) solve;
        // the correct behavior needs real work to walk the new window.
        EXPECT_GT(warm.counters.minor_iters, 1);
    }
}

// (e) saddle_box_qp with tr_radius = 0.5 from x0 = 0: the TR window
// [-0.5,0.5]^2 is tighter than the real box [-1,1]^2 on both variables, so
// the minimizers move from x1 = +/-1 to x1 = +/-0.5 (x0's positive-curvature
// term still settles at the interior point 0). Checked against
// enumerate_local_minimizers run on the EFFECTIVE-bounds problem directly --
// the oracle for this exact case is "the same saddle_box derivation, just
// with the box narrowed to +/-0.5" (test_qp_engine_indefinite.cpp derives
// the +/-1 case by hand; the argument is unchanged under a narrower box).
TEST(QpEngineTr, IndefiniteInteractionWithSaddleBox) {
    const QpProblem qp = saddle_box_qp();

    QpProblem effective_qp = qp;
    effective_qp.lower = Vec::Constant(2, -0.5);
    effective_qp.upper = Vec::Constant(2, 0.5);
    const std::vector<QpSolution> minimizers = enumerate_local_minimizers(effective_qp);
    ASSERT_FALSE(minimizers.empty());

    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions opts;
        opts.ws_algebra = algebra;
        opts.tr_radius = 0.5;
        QpEngine eng{opts};
        const QpSolution sol = eng.solve(qp);

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 0.0, 1e-8);
        EXPECT_NEAR(std::abs(sol.x(1)), 0.5, 1e-8);

        double best = std::numeric_limits<double>::infinity();
        for (const auto &m : minimizers) {
            best = std::min(best, (sol.x - m.x).lpNorm<Eigen::Infinity>());
        }
        EXPECT_LT(best, 1e-7);

        ASSERT_EQ(sol.tr_active.size(), 2u);
        EXPECT_FALSE(sol.tr_active[0]); // x0 is interior/free, not pinned at all
        EXPECT_TRUE(sol.tr_active[1]);  // x1 is TR-pinned (real bound is +/-1)
        EXPECT_EQ(sol.bound_state[0], BoundState::kFree);
        EXPECT_EQ(sol.bound_state[1], BoundState::kFree);
        EXPECT_EQ(sol.z(1), 0.0);
    }
}

// (Fix round 1, findings I1/I2) The real same-instance regression the
// header's original (wrong) citation should have pointed at: ONE QpEngine,
// one FIXED tr_radius, a cold solve followed by a warm solve from that same
// solve's own answer as the seed. The TR window is centered on the SOLVE's
// own start point (the seed's x, clamped by the real bounds), and that x is
// the cold solve's ANSWER rather than its original x0 = 0 -- exactly the
// "moving trust region" a Phase-3 SQP driver produces by re-centering at
// each accepted iterate. H/Ae/Ai (and both hot-start fingerprints) are
// byte-identical between the two calls; only the bounds shift.
//
// The fixture makes the shift change WHICH mechanism pins x0, not just the
// pinned value: real bound 1.5 is looser than the cold window (up_eff = 1),
// so x0 is TR-pinned there; it is tighter than the warm, shifted window
// (up_eff would be 2), so x0 is REAL-bound-pinned there instead. Both solves
// are checked against solve_dense_oracle run on that solve's own
// effective-bounds problem, built by hand with the engine's own
// lo_eff/up_eff formula.
TEST(QpEngineTr, SameInstanceWarmSolveWithShiftedTrWindow) {
    QpProblem qp = unconstrained_quadratic_qp();
    qp.upper(0) = 1.5;

    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions opts;
        opts.ws_algebra = algebra;
        opts.tr_radius = 1.0;
        QpEngine eng{opts}; // ONE instance for both solves below.

        const QpSolution cold = eng.solve(qp);
        ASSERT_EQ(cold.status, QpStatus::kOptimal);

        // Cold window: x0 = (0,0), lo_eff = (-1,-1), up_eff =
        // (min(1.5,1)=1, min(1e20,1)=1). x0's real bound (1.5) is looser
        // than the window (1), so x0 is TR-pinned there, same as x1.
        QpProblem cold_effective = qp;
        cold_effective.lower = Vec::Constant(2, -1.0);
        cold_effective.upper = Vec(2);
        cold_effective.upper << 1.0, 1.0;
        const QpSolution cold_oracle = solve_dense_oracle(cold_effective);
        EXPECT_TRUE((cold.x - cold_oracle.x).isZero(1e-8));
        ASSERT_EQ(cold.tr_active.size(), 2u);
        EXPECT_TRUE(cold.tr_active[0]);
        EXPECT_TRUE(cold.tr_active[1]);
        expect_all_free_and_untracked(cold, 2);

        const QpSolution warm = eng.solve(qp, cold);
        ASSERT_EQ(warm.status, QpStatus::kOptimal);

        // Warm window: x0 = cold.x = (1,1) (clamped by the real bounds,
        // unaffected here), lo_eff = (0,0), up_eff = (min(1.5,2)=1.5,
        // min(1e20,2)=2). The real bound (1.5) is now TIGHTER than the
        // window (2) for x0, so x0 flips to a REAL-bound pin; x1 stays
        // TR-pinned (its real bound is still absent).
        QpProblem warm_effective = qp;
        warm_effective.lower = Vec::Constant(2, 0.0);
        warm_effective.upper = Vec(2);
        warm_effective.upper << 1.5, 2.0;
        const QpSolution warm_oracle = solve_dense_oracle(warm_effective);
        EXPECT_TRUE((warm.x - warm_oracle.x).isZero(1e-8));

        ASSERT_EQ(warm.tr_active.size(), 2u);
        EXPECT_FALSE(warm.tr_active[0]); // now real-bound-pinned, not TR
        EXPECT_TRUE(warm.tr_active[1]);  // still TR-pinned
        EXPECT_EQ(warm.bound_state[0], BoundState::kAtUpper);
        EXPECT_EQ(warm.bound_state[1], BoundState::kFree);
        // Real-bound multiplier at x0 is genuinely priced (z <= 0 at an
        // active upper bound; grad = x - 10 = -8.5 there); the TR dual at
        // x1 is not exposed.
        EXPECT_NEAR(warm.z(0), -8.5, 1e-6);
        EXPECT_EQ(warm.z(1), 0.0);

        // Reuse-eligibility (Task 5 carry-forward: never blindly assert
        // factorizations == 0 on a warm re-solve -- assert what is actually
        // observed, with the mechanism named). OBSERVED (border mode): the
        // cold exit's RAW ws.bound_state() -- what border_exit_bound_state_
        // stores -- shows both variables pinned (kAtUpper, kAtUpper; the
        // internal state BEFORE this feature's bound_state-reporting
        // exclusion rewrites the copy handed back in QpSolution).
        // start_point()'s seed ingestion cannot reproduce either pin from
        // `cold` (both report kFree per rule (a), and seed ingestion never
        // reads tr_active), so the fresh seed ws is (kFree, kFree) before
        // the loop's first iteration -- a mismatch against the stored exit
        // state that correctly fails reuse-eligibility condition (b) and
        // forces a K0 rebuild. This is the SAME "a bound change alters
        // ws.bound_state() at the seed and correctly kills reuse" mechanism
        // the header documents, triggered here by the window re-centering
        // rather than by tr_radius itself changing (which can't happen on
        // one instance -- see the header's PHASE-3 DESIGN FRICTION note).
        if (algebra == WorkingSetLinearAlgebra::kSchurBorder) {
            EXPECT_GT(warm.counters.factorizations, 0);
        }
    }
}

// --- Task 2: per-solve overrides (SolveOverrides) --------------------------

// (f) ShrinkRadiusRetryReusesHotStart. Models a Phase-3 SHRINK-RADIUS RETRY:
// one ENGINE, the SAME fixed base point (cold start, x0 = (0,0), on BOTH
// calls -- exactly the "re-solve the rejected step's own QP subproblem at a
// smaller Delta, from the SAME x_k" shape of a real retry loop, not a
// warm-start chained from the first call's own ANSWER), first at Delta = 1
// via SolveOverrides, then again at Delta = 0.5 via SolveOverrides on the
// SAME engine instance. Before Task 2 this required two separate QpEngine
// instances (tr_radius being per-instance const), forfeiting hot-start reuse
// entirely -- see the header contract's PHASE-3 DESIGN FRICTION note.
//
// HAND-DERIVED GEOMETRY. H = I, g = (-0.7, 0): the unconstrained optimum is
// (0.7, 0), independent of any window.
//   Delta = 1, x0 = (0,0): window [-1,1]^2. 0.7 is INTERIOR to [-1,1], and 0
//   is trivially interior on x1 -- so this solve's own EXIT working set has
//   NO pin at all (bound_state == (kFree, kFree), tr_active == (false,
//   false)): "optimum interior to Delta = 1" from the brief.
//   Delta = 0.5, x0 = (0,0) (the SAME cold start, not the first solve's
//   answer): window [-0.5,0.5]^2. 0.7 now lies OUTSIDE [-0.5,0.5], so x0 is
//   TR-pinned at up_eff = 0.5; x1's optimum (0) is still interior. This
//   solve's own exit has EXACTLY ONE pin.
//
// WHY THE PAIR CANNOT *ALSO* BOTH CARRY A PIN AT EXIT AND STILL REUSE. A
// tempting stronger fixture would pin the SAME variable in BOTH solves'
// exits (e.g. g = (-2, 0), pinned at up_eff = 1 AND up_eff = 0.5). That
// fixture is UNREACHABLE for factorizations == 0 when the pair is CHAINED
// (each solve seeded from the previous one's own returned QpSolution), for a
// reason orthogonal to this task: rule (a) (see the header's "Section 6" and
// HOT-START REUSE INTERACTION notes) reports a TR-pinned index kFree, so
// chaining from the engine's own output can never reproduce that pin as a
// seed hint at all -- the fresh seed ws is just kFree there, mismatching
// whatever the previous solve's real internal exit was, and reuse-eligibility
// condition (b) (seed ws == previous exit ws) fails.
//
// This is narrower than "a TR-attributed pin and reuse are mutually
// exclusive" -- they are NOT, and it is worth being precise about why, since
// a HAND-BUILT seed (bound_state set directly, as in
// QpWarmStart.StaleSeedStillConverges) can still reproduce a bound pin as a
// seed hint, and start_point() honors it by snapping x(i) to the REAL bound
// value at that instant -- which is a tie with the real bound only AT THAT
// SAME PIN, not a ban on TR activity anywhere else in the solve. The loop is
// free to release that hinted pin (an ordinary drop, if it is not where the
// EQP wants to sit) and land on a DIFFERENT, genuinely TR-attributed pin
// before the loop ends -- reuse-eligibility only ever compares the PRE-LOOP
// seed ws against the PRIOR call's exit, never against what THIS call's own
// loop later discovers. Verified directly: seeding kAtUpper (real bounds
// +/-3) forces x0 = 3 for a Delta=1 solve whose true optimum is 1.5, so the
// hint is dropped and TR-pins at lo_eff = 2 instead (tr_active true); seeding
// the NEXT solve (Delta=0.5) with a hand-built kAtLower hint matching that
// internal exit forces x0 = -3 there, and it exits TR-pinned again (at
// up_eff = -2.5) with factorizations == 0. So what actually blocks "pinned in
// both radii" here is specifically the CHAINING approach (rule (a) erasing
// the previous pin from the seed the engine itself hands back), not a
// property of TR pins and reuse in general. This fixture keeps the FIRST
// solve's exit pin-free -- satisfying condition (b) trivially against the
// SECOND solve's own cold (kFree, kFree) seed -- and lets the pin appear
// fresh, cheaply, inside the SECOND solve's own loop (an ordinary ratio-test
// block, a pin_variable BORDER on the already-reused K0, not a rebuild).
TEST(QpEngineTr, ShrinkRadiusRetryReusesHotStart) {
    const QpProblem qp = shrink_retry_qp();

    QpProblem cold_effective = qp;
    cold_effective.lower = Vec::Constant(2, -1.0);
    cold_effective.upper = Vec::Constant(2, 1.0);
    const QpSolution cold_oracle = solve_dense_oracle(cold_effective);

    QpProblem warm_effective = qp;
    warm_effective.lower = Vec::Constant(2, -0.5);
    warm_effective.upper = Vec::Constant(2, 0.5);
    const QpSolution warm_oracle = solve_dense_oracle(warm_effective);

    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions opts;
        opts.ws_algebra = algebra;
        QpEngine eng{opts}; // ONE instance for both retry radii.

        SolveOverrides cold_overrides;
        cold_overrides.tr_radius = 1.0;
        const QpSolution cold = eng.solve(qp, cold_overrides);

        ASSERT_EQ(cold.status, QpStatus::kOptimal);
        EXPECT_TRUE((cold.x - cold_oracle.x).isZero(1e-8));
        ASSERT_EQ(cold.tr_active.size(), 2u);
        EXPECT_FALSE(cold.tr_active[0]); // interior to Delta = 1
        EXPECT_FALSE(cold.tr_active[1]);
        EXPECT_EQ(cold.bound_state[0], BoundState::kFree);
        EXPECT_EQ(cold.bound_state[1], BoundState::kFree);

        SolveOverrides warm_overrides;
        warm_overrides.tr_radius = 0.5;
        const QpSolution warm = eng.solve(qp, warm_overrides); // SAME cold x0 = (0,0), no seed.

        ASSERT_EQ(warm.status, QpStatus::kOptimal);
        EXPECT_TRUE((warm.x - warm_oracle.x).isZero(1e-8));
        ASSERT_EQ(warm.tr_active.size(), 2u);
        EXPECT_TRUE(warm.tr_active[0]); // TR-pinned at Delta = 0.5
        EXPECT_FALSE(warm.tr_active[1]);
        EXPECT_EQ(warm.bound_state[0], BoundState::kFree); // rule (a): TR pin reports kFree
        EXPECT_EQ(warm.bound_state[1], BoundState::kFree);
        EXPECT_EQ(warm.z(0), 0.0); // TR dual never exposed
        EXPECT_EQ(warm.z(1), 0.0);

        // The whole point: SolveOverrides lets ONE engine instance serve both
        // radii, so the retry's K0 is REUSED outright (border mode only --
        // kRefactorize has no such concept and always factorizes at least
        // once per working-set change).
        if (algebra == WorkingSetLinearAlgebra::kSchurBorder) {
            EXPECT_EQ(warm.counters.factorizations, 0);
        }
    }
}

// (g) DeltaMuOverride correctness + refactorize-mode interaction. kRefactorize
// has no hot-start reuse machinery at all (every working-set change pays a
// fresh factorize), so the only thing worth asserting there is that
// SolveOverrides is still resolved and threaded correctly into the values
// that actually reach the KKT assembly -- primal_delta/dual_mu change the
// REGULARIZED answer by a tiny, computable amount even away from any
// artifact regime, which this pins against the dense oracle (whose own KKT
// solve carries no regularization at all, so the two agree to a tolerance
// set by the override's own size, not by machine epsilon).
TEST(QpEngineTr, OverridesResolveCorrectlyUnderRefactorizeMode) {
    QpProblem qp = shrink_retry_qp();
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    const QpSolution oracle = solve_dense_oracle(qp);

    QpOptions opts;
    opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    QpEngine eng{opts};

    SolveOverrides overrides;
    overrides.primal_delta = 1e-6;
    overrides.dual_mu = 1e-6;
    const QpSolution sol = eng.solve(qp, overrides);

    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_LT((sol.x - oracle.x).norm(), 1e-5);
}

// (h) Forwarding guard (the 118-test byte-identical contract): the plain
// 1-arg/2-arg solve() overloads must still forward a default-constructed
// SolveOverrides and be byte-for-byte identical to calling the 3-arg/4-arg
// overloads with an explicit default-constructed SolveOverrides, across both
// ws_algebra modes and both a cold and a warm call on ONE engine instance.
//
// HONESTY NOTE: this is a REFACTOR FENCE, not independent proof of byte
// identity. `solve(qp)` and `solve(qp, seed)` literally read
// `return run(qp, nullptr, false, SolveOverrides{});` /
// `return run(qp, &seed, true, SolveOverrides{});` -- so this test mostly
// re-confirms that the forwarding line itself was typed correctly, which a
// diff of qp_engine.h already shows. The REAL evidence that nothing else
// changed is the pre-existing 118 tests, none of which use SolveOverrides at
// all and all of which still pass unmodified; this test is a guard against a
// future edit accidentally threading a non-default SolveOverrides (or a
// stale local) into one of the plain overloads, not a claim that byte
// identity was otherwise in doubt.
TEST(QpEngineTr, PlainOverloadsForwardDefaultOverridesByteIdentically) {
    const QpProblem qp = tighter_real_bound_on_x0_qp();

    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions opts;
        opts.ws_algebra = algebra;
        opts.tr_radius = 1.0;

        QpEngine plain_eng{opts};
        QpEngine overrides_eng{opts};

        const QpSolution plain_cold = plain_eng.solve(qp);
        const QpSolution overrides_cold = overrides_eng.solve(qp, SolveOverrides{});

        EXPECT_EQ(plain_cold.status, overrides_cold.status);
        EXPECT_TRUE((plain_cold.x - overrides_cold.x).isZero(0.0));
        EXPECT_TRUE((plain_cold.z - overrides_cold.z).isZero(0.0));
        EXPECT_TRUE((plain_cold.lambda_e - overrides_cold.lambda_e).isZero(0.0));
        EXPECT_TRUE((plain_cold.lambda_i - overrides_cold.lambda_i).isZero(0.0));
        EXPECT_EQ(plain_cold.bound_state, overrides_cold.bound_state);
        EXPECT_EQ(plain_cold.tr_active, overrides_cold.tr_active);
        EXPECT_EQ(plain_cold.ineq_active, overrides_cold.ineq_active);
        EXPECT_EQ(plain_cold.counters.factorizations, overrides_cold.counters.factorizations);
        EXPECT_EQ(plain_cold.counters.schur_updates, overrides_cold.counters.schur_updates);
        EXPECT_EQ(plain_cold.counters.minor_iters, overrides_cold.counters.minor_iters);

        const QpSolution plain_warm = plain_eng.solve(qp, plain_cold);
        const QpSolution overrides_warm = overrides_eng.solve(qp, overrides_cold, SolveOverrides{});

        EXPECT_EQ(plain_warm.status, overrides_warm.status);
        EXPECT_TRUE((plain_warm.x - overrides_warm.x).isZero(0.0));
        EXPECT_TRUE((plain_warm.z - overrides_warm.z).isZero(0.0));
        EXPECT_TRUE((plain_warm.lambda_e - overrides_warm.lambda_e).isZero(0.0));
        EXPECT_TRUE((plain_warm.lambda_i - overrides_warm.lambda_i).isZero(0.0));
        EXPECT_EQ(plain_warm.bound_state, overrides_warm.bound_state);
        EXPECT_EQ(plain_warm.tr_active, overrides_warm.tr_active);
        EXPECT_EQ(plain_warm.ineq_active, overrides_warm.ineq_active);
        EXPECT_EQ(plain_warm.counters.factorizations, overrides_warm.counters.factorizations);
        EXPECT_EQ(plain_warm.counters.schur_updates, overrides_warm.counters.schur_updates);
        EXPECT_EQ(plain_warm.counters.minor_iters, overrides_warm.counters.minor_iters);
    }
}

// (i) Fix round 1 [Important]: a negative, non-sentinel tr_radius silently
// crosses lo_eff/up_eff in a Release build (section 6's CROSSED EFFECTIVE
// BOUNDS CANNOT HAPPEN proof assumes Delta >= 0, guarded only by an `assert`
// NDEBUG compiles out) -- probed at tr_radius = -0.5 on a [-2,2]^2 box, which
// returned kOptimal at (-0.5, 0) with no diagnostic. QpEngine::solve now
// rejects it explicitly, before anything else in that call can run, through
// BOTH override-taking overloads.
TEST(QpEngineTr, OverrideValidationRejectsNegativeRadius) {
    QpProblem qp = unconstrained_quadratic_qp();
    qp.lower = Vec::Constant(2, -2.0);
    qp.upper = Vec::Constant(2, 2.0);

    QpEngine eng{QpOptions{}};

    SolveOverrides bad;
    bad.tr_radius = -0.5;

    EXPECT_THROW(eng.solve(qp, bad), std::invalid_argument);

    QpSolution seed;
    seed.status = QpStatus::kOptimal;
    seed.x = Vec::Zero(2);
    seed.bound_state.assign(2, BoundState::kFree);
    seed.ineq_active.assign(0, false);
    seed.lambda_e = Vec(0);
    seed.lambda_i = Vec(0);
    seed.z = Vec::Zero(2);
    EXPECT_THROW(eng.solve(qp, seed, bad), std::invalid_argument);
}

// (j) Companion validation coverage: NaN is rejected for all three fields
// (tr_radius has no legitimate NaN reading at all; primal_delta/dual_mu's
// legitimate sentinel is any NEGATIVE value, never NaN).
TEST(QpEngineTr, OverrideValidationRejectsNaN) {
    const QpProblem qp = unconstrained_quadratic_qp();
    QpEngine eng{QpOptions{}};
    const double nan = std::numeric_limits<double>::quiet_NaN();

    SolveOverrides nan_radius;
    nan_radius.tr_radius = nan;
    EXPECT_THROW(eng.solve(qp, nan_radius), std::invalid_argument);

    SolveOverrides nan_delta;
    nan_delta.primal_delta = nan;
    EXPECT_THROW(eng.solve(qp, nan_delta), std::invalid_argument);

    SolveOverrides nan_mu;
    nan_mu.dual_mu = nan;
    EXPECT_THROW(eng.solve(qp, nan_mu), std::invalid_argument);
}

// (k) Fix round 1 [M7]: the Task-6-shaped reuse case with a LIVE, non-empty
// working set surviving a tr_radius override change -- companion to (f)
// above, which deliberately kept the first solve's exit pin-free. Here the
// pin is a REAL bound (not TR), tight enough (+/-0.6) to bind before either
// window does, and it is carried forward by the ORDINARY warm-chaining idiom
// (seeded from the engine's own returned QpSolution -- rule (a) does not
// erase a real-bound pin's reported bound_state, only a TR one's), so the
// SAME live kAtUpper pin sits in the border ledger across the Delta = 1 -> 0.5
// change and reuse still holds.
//
// GEOMETRY. H = I, g = (-2, 0): x0's unconstrained optimum is 2, x1's is 0.
// Real bounds +/-0.6 on both variables.
//   Cold, Delta = 1, x0 = (0,0): window [-1,1] is LOOSER than the real bound
//   (0.6 < 1), so the REAL bound binds first: x* = (0.6, 0), bound_state =
//   (kAtUpper, kFree), tr_active = (false, false) (up_eff = min(0.6, 1) = 0.6
//   ties the real bound exactly).
//   Warm, chained from cold (x = (0.6, 0), bound_state = (kAtUpper, kFree)
//   reported HONESTLY -- not TR, so rule (a) leaves it alone), Delta = 0.5:
//   new x0 = clamp(0.6, real bounds) = 0.6 (the ingestion hint re-forces the
//   same value, a no-op). New window for x0 is [0.1, 1.1] intersected with
//   the real bound -> up_eff = min(0.6, 1.1) = 0.6, STILL tied to the real
//   bound (x0 already sits there, so no Delta > 0 can loosen past it). Same
//   pin, same side, still non-TR, in BOTH solves.
// Since the real bound governs at both radii, the engine's own effective
// bounds equal the real ones throughout, so the dense oracle is run directly
// on `qp` (no hand-narrowed copy needed, unlike (f)).
TEST(QpEngineTr, ShrinkRadiusRetryWithALiveRealBoundPinChainsAndReuses) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -2.0, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -0.6);
    qp.upper = Vec::Constant(2, 0.6);

    const QpSolution oracle = solve_dense_oracle(qp);

    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions opts;
        opts.ws_algebra = algebra;
        QpEngine eng{opts};

        SolveOverrides cold_overrides;
        cold_overrides.tr_radius = 1.0;
        const QpSolution cold = eng.solve(qp, cold_overrides);

        ASSERT_EQ(cold.status, QpStatus::kOptimal);
        EXPECT_TRUE((cold.x - oracle.x).isZero(1e-8));
        EXPECT_EQ(cold.bound_state[0], BoundState::kAtUpper);
        EXPECT_EQ(cold.bound_state[1], BoundState::kFree);
        ASSERT_EQ(cold.tr_active.size(), 2u);
        EXPECT_FALSE(cold.tr_active[0]); // real bound, not TR
        EXPECT_FALSE(cold.tr_active[1]);

        SolveOverrides warm_overrides;
        warm_overrides.tr_radius = 0.5;
        const QpSolution warm =
            eng.solve(qp, cold, warm_overrides); // chained, ordinary warm-start.

        ASSERT_EQ(warm.status, QpStatus::kOptimal);
        EXPECT_TRUE((warm.x - oracle.x).isZero(1e-8));
        EXPECT_EQ(warm.bound_state[0], BoundState::kAtUpper); // same live, non-TR pin
        EXPECT_EQ(warm.bound_state[1], BoundState::kFree);
        ASSERT_EQ(warm.tr_active.size(), 2u);
        EXPECT_FALSE(warm.tr_active[0]);
        EXPECT_FALSE(warm.tr_active[1]);

        if (algebra == WorkingSetLinearAlgebra::kSchurBorder) {
            EXPECT_EQ(warm.counters.factorizations, 0);
        }
    }
}

// (l) Fix round 1 [C1]: THE WINDOW-CONSISTENCY RULE. A seeded bound-state
// hint used to be materialized onto x BEFORE section 6 computed lo_eff/up_eff
// about x -- so a seeded pin at a bound FAR from the intended center silently
// re-centered the whole trust region on that bound, and the returned step
// violated the radius the caller asked for. Zeroing seed.x (the SQP driver's
// TR-centering discipline, sqp_driver.h's WARM SEEDING note) does NOT prevent
// it: the zero is overwritten by the pin before the window is ever computed.
//
// THE REPRO, hand-derived. H = I, g = (-100, -100), real box [0, 6]^2: the
// unconstrained minimizer (100, 100) is far outside the box, so an
// unrestricted solve exits at the REAL upper bound (6, 6) with bound_state
// (kAtUpper, kAtUpper) -- an honest, non-TR pin that rule (a) reports
// verbatim. Feed that back as a seed with x ZEROED, at Delta = 5 / 2.5 /
// 1.25 about the center x0 = clamp(0) = 0. Every one of those windows is
// [0, Delta], so the answer must be (Delta, Delta). Before this fix all three
// returned (6, 6) -- the pin moved the center to 6, making the window
// [6 - Delta, 6], and 6 is |x|inf = 6 against a radius of 1.25.
//
// THE RULE, now documented in the header contract: the window is computed
// about the CLAMPED SEED PRIMAL, before any bound-state hint is materialized;
// a hint whose bound falls OUTSIDE that window is DROPPED (the index arrives
// kFree and the loop re-derives its activity from the effective bounds like
// any other). A hint INSIDE the window is honoured exactly as before, which
// is what ShrinkRadiusRetryWithALiveRealBoundPinChainsAndReuses guards.
TEST(QpEngineTr, SeededOppositeBoundPinDoesNotRecenterTheWindow) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -100.0, -100.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 6.0);

    for (const auto algebra : kBothModes) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        QpOptions opts;
        opts.ws_algebra = algebra;
        QpEngine eng{opts};

        const QpSolution far = eng.solve(qp); // no radius: straight to the real bound
        ASSERT_EQ(far.status, QpStatus::kOptimal);
        EXPECT_NEAR(far.x(0), 6.0, 1e-9);
        ASSERT_EQ(far.bound_state.size(), 2u);
        EXPECT_EQ(far.bound_state[0], BoundState::kAtUpper);
        EXPECT_EQ(far.bound_state[1], BoundState::kAtUpper);

        for (const double radius : {5.0, 2.5, 1.25}) {
            QpSolution seed = far;
            seed.x.setZero(); // the SQP driver's TR-centering discipline
            SolveOverrides overrides;
            overrides.tr_radius = radius;
            const QpSolution warm = eng.solve(qp, seed, overrides);

            ASSERT_EQ(warm.status, QpStatus::kOptimal) << "radius " << radius;
            // The window is [0, radius] about the center 0, so the step is
            // the radius itself -- TR-attributed, hence bound_state kFree
            // (rule (a)) and tr_active true.
            EXPECT_NEAR(warm.x(0), radius, 1e-8) << "radius " << radius;
            EXPECT_NEAR(warm.x(1), radius, 1e-8) << "radius " << radius;
            EXPECT_LE(warm.x.lpNorm<Eigen::Infinity>(), radius * (1.0 + 1e-9))
                << "the returned point must respect the radius it was solved at";
            ASSERT_EQ(warm.tr_active.size(), 2u);
            EXPECT_TRUE(warm.tr_active[0]) << "radius " << radius;
            EXPECT_EQ(warm.bound_state[0], BoundState::kFree) << "radius " << radius;
        }
    }
}
