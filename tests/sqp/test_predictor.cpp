// tests/test_predictor.cpp — Phase-4 Task 9: the TANGENTIAL PREDICTOR
// (predictor.h), i.e. the parametric-sensitivity step that turns a converged
// WarmStart at p into a first-order-accurate WarmStart at p + dp.
//
// WHAT IS ASSERTED, AND WHY EACH FAMILY WAS PICKED FOR IT
// (tests/sqp/support/parametric_families.h has the analytic paths):
//
//   1. ORDER OF ACCURACY (PredictorTracksSmoothPath). On F2 -- the only one of
//      the three families whose solution path is genuinely CURVED in p -- the
//      predictor's error against x_star(p + dp) is measured at three step
//      sizes and the two successive RATIOS are pinned in a window around 4.
//      Halving dp quartering the error IS the O(||dp||^2) claim; asserting a
//      single absolute error against a hand-picked constant would not be.
//
//   2. THE AFFINE PATHS, WHERE THE ONLY ERROR LEFT IS THE REGULARIZATION
//      (PredictorIsExactUpToRegularizationOnAffinePaths). F1 and F3 are QPs
//      whose x_star(p) is piecewise LINEAR in p (F1's branches are
//      x = (p, 1-p), (p, 0.8), (0.8, 1-p); F3's is x_i = i*min(p, u/(n-1))),
//      so a FIRST-order predictor has ZERO linearization error there and what
//      remains is measurable in isolation. What remains is the KKT system's
//      TIKHONOV REGULARIZATION: predict() assembles the same
//      delta*I / -mu*I-regularized K the QP engine does (kkt_assembly.h), with
//      delta and mu taken from the warm start's own effective values, so the
//      step it reads back carries a relative error of order delta. Measured,
//      at the default delta = mu = 1e-8: 2.0e-9 on F1 at ||dp|| = 0.2 (i.e.
//      1.0 * delta * ||dp||) and 7.6e-7 on F3 at ||dp|| = 0.15 (~500 * delta *
//      ||dp||, the spring chain's own conditioning). Both tests therefore
//      assert TWICE: once at the default regularization, and once at
//      delta = mu = 1e-12, where the error must fall by the same four orders.
//      That second measurement is what identifies the residual AS the
//      regularization floor rather than a first-order defect -- an O(||dp||^2)
//      error would not move at all when delta does. (An iterative-refinement
//      step against the unregularized operator would remove this floor; it is
//      deliberately not implemented -- see predictor.h's own note -- because
//      the floor sits many orders below the O(||dp||^2) linearization error at
//      any dp a caller would actually take.)
//
//   3. BOUND CROSSING (PredictorHandlesBoundCrossing). F1 stepped ACROSS its
//      p = 0.2 activation threshold, in BOTH directions: 0.15 -> 0.25 relaxes
//      x2's upper bound, 0.25 -> 0.15 activates it. The predicted activity is
//      compared against the analytic far-side active set.
//
//   4. WEAK COMPLEMENTARITY (PredictorKeepsWeaklyActiveRow). F1's linear row
//      is geometrically active with multiplier IDENTICALLY ZERO across its
//      whole middle branch (the families header's DEGENERACY note), so strict
//      complementarity fails there and a predictor that assumes lambda > 0 on
//      every active row -- or that drops a row the instant its predicted
//      multiplier is not strictly positive -- misbehaves exactly there. The
//      documented decision (predictor.h's WEAKLY ACTIVE ROWS note) is to KEEP
//      such a row; this test pins that, and pins that the prediction is still
//      consumable by a solve.
//
//   5. ACCELERATION (PredictedWarmAcceleratesSolve). The end-to-end claim: a
//      solve at p + dp seeded by the PREDICTION takes strictly fewer majors
//      than the same solve seeded by the unpredicted warm object. Both counts
//      are pinned, not just their order, so a regression that makes the
//      predicted arm merely "still fewer" by accident is visible.
//
//   6. THE THREE RIGHT-HAND-SIDE TERMS F1-F3 CANNOT REACH, added in fix round
//      1 (PredictorTracksConstraintsThatMoveWithP,
//      PredictorActivatesConstraintsThatMoveWithP). F1, F2 and F3 all have
//      p-INDEPENDENT cE, cI and bounds -- only their objectives move -- so
//      d/dp cE, d/dp cI and d/dp bound are identically zero in items 1-5 and
//      the Task-9 review showed by mutation that sign errors in any of them
//      left the whole file green. F4 and F5 invert that (p-independent
//      objective, p-dependent constraints), so each term is the only thing
//      moving the answer. F4 additionally has parameter_dim() == 2, which is
//      the only place the DIRECTIONAL finite difference runs with a direction
//      other than +/-1.
//
//   7. THE OUTCOME OBSERVABLE (PredictorReportsWhichPathItTook,
//      DegradationIsReportedRatherThanThrown), also fix round 1: two of
//      predict()'s three paths return the identity prediction, and a caller's
//      accounting must not conflate a legitimate zero step with a failure to
//      predict.
//
// ACTIVITY IS COMPARED THE WAY TASK 8 ESTABLISHED: GEOMETRICALLY (the
// constraint holds with equality at the predicted point) wherever F1's zero
// multiplier makes working-set membership indeterminate, and field-for-field
// against WarmStart::bound_active where it is determinate (F1's bound
// activations are strictly complementary on both sides -- z crosses zero with
// slope 1 -- so the BOX is a clean fixture even though the ROW is not).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/sqp/nlp_model.h>
#include <hven/detail/sqp/predictor.h>
#include <hven/detail/sqp/sqp_driver.h>
#include <hven/detail/sqp/sqp_types.h>
#include <hven/detail/sqp/warm_start.h>

#include "support/derivative_check.h"
#include "support/parametric_families.h"
#include "support/scale_problems.h"

namespace hven::solvers {
namespace {

using test_support::AnalyticActiveSet;
using test_support::F1BoxQp;
using test_support::F2CircleNlp;
using test_support::F3SpringChain;
using test_support::F4MovingConstraints;
using test_support::F5MovingThreshold;

// The regularization floor the affine-path tests measure against: predict()
// inherits WarmStart::primal_delta/dual_mu, which a default solve leaves at
// QpOptions' own 1e-8, and drives them to 1e-12 for the second measurement.
constexpr double kTightReg = 1e-12;

// The warm start the predictor consumes must be converged far tighter than
// the accuracy being measured off it: at dp = 0.01 the O(||dp||^2) error on
// F2 is ~3e-5, so a warm start carrying 1e-8 of its own error would put that
// same 1e-8 into every prediction and the affine-path floors below (down to
// 1e-13) would be measuring the solver's stopping tolerance instead.
SqpOptions tight_options() {
    SqpOptions opts;
    opts.kkt_tol = 1e-11;
    opts.feas_tol = 1e-11;
    opts.max_iter = 80;
    return opts;
}

Vec p_vec(double p) { return Vec::Constant(1, p); }

// A converged WarmStart at parameter p, from a COLD solve -- the object the
// predictor is contractually fed.
WarmStart converged_warm(ParametricNlpModel &model, double p,
                         const SqpOptions &opts = tight_options()) {
    model.set_parameters(p_vec(p));
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);
    EXPECT_EQ(sol.status, SqpStatus::kOptimal) << "cold solve at p = " << p << " did not converge";
    return sol.warm_start;
}

// The same warm start with its effective regularization overridden -- the
// knob predict() reads to build its KKT system (see this file's banner item 2).
WarmStart with_regularization(WarmStart warm, double reg) {
    warm.primal_delta = reg;
    warm.dual_mu = reg;
    return warm;
}

// WarmStart::bound_active's encoding, read off a point geometrically. Same
// notion tests/test_parametric_families.cpp uses, and the only one that is
// determinate at a zero multiplier.
std::vector<std::int8_t> geometric_bound_active(const NlpModel &model, const Vec &x, double tol) {
    std::vector<std::int8_t> out(static_cast<std::size_t>(model.n()), 0);
    const Vec &lo = model.lower();
    const Vec &up = model.upper();
    for (Index i = 0; i < model.n(); ++i) {
        const auto k = static_cast<std::size_t>(i);
        if (std::isfinite(lo(i)) && std::abs(x(i) - lo(i)) <= tol) {
            out[k] = -1;
        } else if (std::isfinite(up(i)) && std::abs(up(i) - x(i)) <= tol) {
            out[k] = +1;
        }
    }
    return out;
}

std::string to_text(const std::vector<std::int8_t> &v) {
    std::string s = "[";
    for (std::int8_t b : v) {
        s += fmt::format("{} ", static_cast<int>(b));
    }
    return s + "]";
}

// A model that REFUSES to be re-posed anywhere but at its entry parameters:
// the probe point p + h*dp/||dp|| is outside its domain and set_parameters
// throws there. This is the cheapest honest route to predict()'s degradation
// handler -- the LINEAR-ALGEBRA route into it (a singular Schur complement, a
// Pardiso error) is not reachable by any fixture, as the Task-9 review
// confirmed. What this pins is therefore the handler and its REPORTING, not
// the particular failure that reaches it; the model-rejects-the-probe case is
// itself one of the failures predictor.h's VALIDATE-THEN-CATCH-EVERYTHING note
// names, and is the realistic one (a parameter domain boundary).
//
// It rejects in EITHER exception type on request, which is the second thing
// this fixture pins: predictor.h's degradation net is by PHASE (everything
// after caller validation), not by TYPE. An earlier version caught only
// std::runtime_error, and schur_complement.h reports two of its own LAPACKE
// failures as std::invalid_argument -- so a type-based net let real numerical
// failures escape a function documented not to throw on them.
class ProbeRejectingF1 : public F1BoxQp {
  public:
    ProbeRejectingF1(double p0, bool reject_as_invalid_argument)
        : F1BoxQp(p0), entry_(p0), as_invalid_argument_(reject_as_invalid_argument) {}

    void set_parameters(const Vec &p) override {
        if (p.size() != 1 || p(0) != entry_) {
            if (as_invalid_argument_) {
                throw std::invalid_argument("ProbeRejectingF1: parameter outside the domain");
            }
            throw std::runtime_error("ProbeRejectingF1: parameter outside the domain");
        }
        F1BoxQp::set_parameters(p);
    }

  private:
    double entry_;
    bool as_invalid_argument_;
};

// ---------------------------------------------------------------------
// 0. THE TWO FIX-ROUND-1 FAMILIES, gated before anything is measured off
//    them: their derivatives must be self-consistent and their analytic
//    paths must be where a cold solve actually lands. Everything below
//    treats F4/F5's closed forms as ground truth, so this comes first --
//    the same TRANSCRIPTION/PATH gate structure
//    tests/test_parametric_families.cpp applies to F1-F3.
// ---------------------------------------------------------------------
TEST(Predictor, MovingConstraintFamiliesMatchTheirAnalyticPaths) {
    const SqpOptions opts = tight_options();
    {
        SCOPED_TRACE("F4MovingConstraints");
        F4MovingConstraints model;
        const Vec x_probe = (Vec(3) << 0.3, -0.2, 0.7).finished();
        for (const Vec &p : {(Vec(2) << 0.15, 0.15).finished(), (Vec(2) << 0.30, -0.10).finished(),
                             (Vec(2) << -0.20, -0.05).finished()}) {
            model.set_parameters(p);
            EXPECT_TRUE(test_support::assert_gradient(model, x_probe, 1e-6));
            EXPECT_TRUE(test_support::assert_jacobians(model, x_probe, 1e-6));
            EXPECT_TRUE(test_support::assert_hessian(model, x_probe, Vec::Constant(1, 0.4),
                                                     Vec::Constant(1, 0.3), 1e-6));

            SqpDriver driver(opts);
            const SqpSolution sol = driver.solve(model);
            ASSERT_EQ(sol.status, SqpStatus::kOptimal) << "p = " << p.transpose();
            EXPECT_LE((sol.x - F4MovingConstraints::x_star(p)).lpNorm<Eigen::Infinity>(), 1e-8)
                << "x = " << sol.x.transpose() << " vs "
                << F4MovingConstraints::x_star(p).transpose();
            EXPECT_NEAR(sol.f, F4MovingConstraints::f_star(p), 1e-9);
            EXPECT_LE(
                (sol.lambda_e - F4MovingConstraints::lambda_e_star(p)).lpNorm<Eigen::Infinity>(),
                1e-7);
            EXPECT_LE(
                (sol.lambda_i - F4MovingConstraints::lambda_i_star(p)).lpNorm<Eigen::Infinity>(),
                1e-7);
            EXPECT_LE((sol.z - F4MovingConstraints::z_star(p)).lpNorm<Eigen::Infinity>(), 1e-7);
            EXPECT_EQ(geometric_bound_active(model, sol.x, 1e-8),
                      F4MovingConstraints::active_set(p).bound_active);
        }
    }
    {
        SCOPED_TRACE("F5MovingThreshold");
        F5MovingThreshold model;
        const Vec x_probe = (Vec(2) << 0.4, -0.3).finished();
        for (double p : {-0.10, 0.10, 0.35}) {
            model.set_parameters(p_vec(p));
            EXPECT_TRUE(test_support::assert_gradient(model, x_probe, 1e-6));
            EXPECT_TRUE(test_support::assert_jacobians(model, x_probe, 1e-6));
            EXPECT_TRUE(
                test_support::assert_hessian(model, x_probe, Vec(0), Vec::Constant(1, 0.2), 1e-6));

            SqpDriver driver(opts);
            const SqpSolution sol = driver.solve(model);
            ASSERT_EQ(sol.status, SqpStatus::kOptimal) << "p = " << p;
            EXPECT_LE((sol.x - F5MovingThreshold::x_star(p)).lpNorm<Eigen::Infinity>(), 1e-8)
                << "x = " << sol.x.transpose();
            EXPECT_NEAR(sol.f, F5MovingThreshold::f_star(p), 1e-9);
            EXPECT_LE(
                (sol.lambda_i - F5MovingThreshold::lambda_i_star(p)).lpNorm<Eigen::Infinity>(),
                1e-7);
            EXPECT_LE((sol.z - F5MovingThreshold::z_star(p)).lpNorm<Eigen::Infinity>(), 1e-7);
        }
    }
}

// ---------------------------------------------------------------------
// 0b. THE THREE RIGHT-HAND-SIDE TERMS F1-F3 CANNOT REACH.
//
// F1, F2 and F3 all have p-independent cE, cI and bounds, so d/dp cE,
// d/dp cI and d/dp bound are identically zero in every test written against
// them and a sign error in any of the three passes unnoticed (the Task-9
// review proved it by mutation). These two tests are written against families
// whose OBJECTIVE is p-independent and whose CONSTRAINTS are where p enters,
// so each of the three terms is not merely present but is the ONLY thing
// moving the answer.
// ---------------------------------------------------------------------
TEST(Predictor, PredictorTracksConstraintsThatMoveWithP) {
    // F4: p enters the equality, the inequality and a bound, and nothing else.
    // dp is deliberately NOT axis-aligned so the directional finite difference
    // (p + h*dp/||dp||, rescaled by ||dp||/h) runs with a real direction --
    // every other fixture in this file has parameter_dim() == 1, where that
    // normalization is only ever +/-1.
    const Vec p0 = (Vec(2) << 0.15, 0.15).finished(); // a = 0.30, both active
    const Vec dp = (Vec(2) << 0.06, 0.04).finished(); // a -> 0.40
    const Vec p1 = p0 + dp;
    ASSERT_TRUE(F4MovingConstraints::constraints_active(p0));
    ASSERT_TRUE(F4MovingConstraints::constraints_active(p1));

    F4MovingConstraints model;
    model.set_parameters(p0);
    SqpDriver driver(tight_options());
    const SqpSolution sol = driver.solve(model);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    const WarmStart warm = sol.warm_start;

    model.set_parameters(p0);
    const WarmStart pred = predict(model, warm, dp);

    // The path is affine in p on this branch, so there is no linearization
    // error at all and the residual is the regularization floor (measured
    // 1.08e-9 at delta = 1e-8, ||dp|| = 0.0721 -- the same delta*||dp|| law F1
    // shows, with this family's own constant ~1.4 rather than F1's 1.0).
    // x_star(p1) = (0.4, 0.4, 0.4) -- the equality row sets x0, the inequality
    // row sets x1, the moving lower bound sets x2, one per RHS term.
    EXPECT_LE((pred.x - F4MovingConstraints::x_star(p1)).lpNorm<Eigen::Infinity>(), 5e-9)
        << "x_pred = " << pred.x.transpose()
        << " vs x_star = " << F4MovingConstraints::x_star(p1).transpose();
    // The multipliers move with p too, and z pins the bound term a second way:
    // a wrong bound derivative that happened to be masked in x (the pinned
    // component is clamped to the bound value) still shows here.
    EXPECT_NEAR(pred.lambda_e(0), F4MovingConstraints::lambda_e_star(p1)(0), 5e-9);
    EXPECT_NEAR(pred.lambda_i(0), F4MovingConstraints::lambda_i_star(p1)(0), 5e-9);
    EXPECT_NEAR(pred.z(2), F4MovingConstraints::z_star(p1)(2), 5e-9);
    EXPECT_EQ(pred.bound_active, F4MovingConstraints::active_set(p1).bound_active);
    EXPECT_EQ(pred.ineq_active[0], 1);

    // THE SAME TWO-KNOB IDENTIFICATION as the F1/F3 affine tests, with one
    // extra knob turned -- and the reason is a finding worth recording. On F1
    // the finite-difference term is EXACTLY ZERO by accident of that family's
    // a(p) = p: the probe stores p + h*u, and differencing a quantity that
    // depends on p through the identity map recovers exactly the perturbation
    // that was stored, rounding and all. F4's a(p) = p1 + p2 does NOT cancel
    // that way -- the two roundings of the sum differ -- so F4 shows the
    // forward-difference floor of predictor.h's term (2) at ~7.9e-11, and
    // BELOW delta ~ 1e-10 that floor, not the regularization, is what remains
    // (measured: 8.86e-11 at delta = 1e-10 against 7.87e-11 at delta = 1e-12 --
    // barely moving, which is exactly how a term that does not depend on delta
    // behaves). Raising fd_step_scale removes it at no cost here because a(p)
    // is AFFINE in p, so the larger step buys the usual cancellation reduction
    // with zero truncation penalty. With both floors out of the way the
    // residual is 7.8e-13, i.e. the prediction on this family is exact.
    PredictorOptions wide_fd;
    wide_fd.fd_step_scale = 100.0;
    model.set_parameters(p0);
    const WarmStart sharp = predict(model, with_regularization(warm, kTightReg), dp, wide_fd);
    EXPECT_LE((sharp.x - F4MovingConstraints::x_star(p1)).lpNorm<Eigen::Infinity>(), 5e-12)
        << "x_pred = " << sharp.x.transpose();
}

TEST(Predictor, PredictorActivatesConstraintsThatMoveWithP) {
    // F5: at p = -0.1 nothing is active and the warm start sits at the origin,
    // so dx from the frozen (empty) active set is ZERO -- the step is driven
    // entirely by the constraints moving. Crossing p = 0 must therefore fire
    // BOTH repairs from the parameter derivatives alone: ADD the row (from
    // d/dp cI) and FIX the bound (from d/dp lower).
    constexpr double kP = -0.1;
    constexpr double kDp = 0.2;
    F5MovingThreshold model(kP);
    const WarmStart warm = converged_warm(model, kP);
    ASSERT_EQ(warm.ineq_active[0], 0) << "fixture premise: nothing active at p < 0";
    ASSERT_EQ(warm.bound_active[1], 0);

    model.set_parameters(p_vec(kP));
    const WarmStart pred = predict(model, warm, p_vec(kDp));
    const double p_far = kP + kDp;

    // Both branches are affine, so the far side is hit exactly (up to the
    // regularization floor, delta*||dp|| = 2e-9): x*(0.1) = (0.1, 0.1).
    EXPECT_LE((pred.x - F5MovingThreshold::x_star(p_far)).lpNorm<Eigen::Infinity>(), 5e-9)
        << "x_pred = " << pred.x.transpose();
    EXPECT_EQ(pred.ineq_active[0], 1) << "the ADD repair did not fire";
    EXPECT_EQ(pred.bound_active[1], -1) << "the FIX repair did not fire";
    EXPECT_NEAR(pred.lambda_i(0), F5MovingThreshold::lambda_i_star(p_far)(0), 5e-9);
    EXPECT_NEAR(pred.z(1), F5MovingThreshold::z_star(p_far)(1), 5e-9);
    // The clamped variable is exactly ON the moved bound, not near it.
    EXPECT_DOUBLE_EQ(pred.x(1), p_far);

    // And it is consumable: seeded at p_far, the solve confirms the far side.
    model.set_parameters(p_vec(p_far));
    SqpDriver driver(tight_options());
    const SqpSolution sol = driver.solve(model, model.start_point(), pred);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_LE((sol.x - F5MovingThreshold::x_star(p_far)).lpNorm<Eigen::Infinity>(), 1e-8);
}

// ---------------------------------------------------------------------
// 1. ORDER OF ACCURACY -- the O(||dp||^2) claim, measured.
// ---------------------------------------------------------------------
TEST(Predictor, PredictorTracksSmoothPath) {
    // p = 0.9 is on F2's ACTIVE branch (p* = 0.786...), comfortably clear of
    // the threshold: every dp below keeps p + dp on the same branch, so the
    // frozen active set the predictor works on is the correct one throughout
    // and what is being measured is purely the linearization error.
    constexpr double kP = 0.9;
    const std::vector<double> steps = {0.04, 0.02, 0.01};

    F2CircleNlp model(kP);
    const WarmStart warm = converged_warm(model, kP);
    ASSERT_TRUE(warm.valid);
    ASSERT_LE((warm.x - F2CircleNlp::x_star(kP)).norm(), 1e-9);

    std::vector<double> errors;
    for (double dp : steps) {
        model.set_parameters(p_vec(kP));
        const WarmStart pred = predict(model, warm, p_vec(dp));
        EXPECT_TRUE(pred.valid);
        // predict() must leave the model at its ENTRY parameters (predictor.h's
        // PARAMETER RESTORATION contract).
        EXPECT_DOUBLE_EQ(model.p(), kP);
        EXPECT_TRUE(F2CircleNlp::constraint_active(kP + dp)) << "dp = " << dp << " crossed p*";
        errors.push_back((pred.x - F2CircleNlp::x_star(kP + dp)).norm());
    }

    // THE RATIO WINDOW. A second-order predictor's error is
    // e(dp) = C dp^2 (1 + O(dp)), so halving dp gives e_k/e_{k+1} = 4(1 + O(dp))
    // -- approaching 4 FROM BELOW here, since the next term has the sign that
    // makes the larger step relatively cheaper. Measured: 4.952e-4, 1.248e-4,
    // 3.131e-5, i.e. ratios 3.9695 and 3.9848 (a 0.8% and a 0.4% deficit,
    // halving with dp exactly as an O(dp) correction should). [3.7, 4.3] holds
    // that drift with several times the observed margin while excluding first
    // order (ratio 2) and third (ratio 8) by a wide margin, which is the
    // discrimination this test exists for.
    ASSERT_EQ(errors.size(), 3u);
    for (std::size_t k = 0; k + 1 < errors.size(); ++k) {
        ASSERT_GT(errors[k + 1], 0.0);
        const double ratio = errors[k] / errors[k + 1];
        EXPECT_GT(ratio, 3.7) << fmt::format("dp {} -> {}: errors {:.6e} / {:.6e} = {:.4f}",
                                             steps[k], steps[k + 1], errors[k], errors[k + 1],
                                             ratio);
        EXPECT_LT(ratio, 4.3) << fmt::format("dp {} -> {}: errors {:.6e} / {:.6e} = {:.4f}",
                                             steps[k], steps[k + 1], errors[k], errors[k + 1],
                                             ratio);
    }
    // And the absolute scale is what a second-order method at these steps
    // should give. The ratio test ALONE cannot pin the constant: a predictor
    // that returned warm.x unchanged has a self-similar error too (it would
    // show ratios of exactly 2, which the window above excludes, but a
    // half-strength step would not) -- this bound is what pins it.
    EXPECT_LT(errors.back(), 1e-4);
}

// ---------------------------------------------------------------------
// 2. THE AFFINE PATHS: the only error left is the regularization floor.
// ---------------------------------------------------------------------
TEST(Predictor, PredictorIsExactUpToRegularizationOnAffinePaths) {
    {
        // F1 branch M (0.2 <= p <= 0.8): x*(p) = (p, 1-p), affine in p.
        SCOPED_TRACE("F1 branch M");
        constexpr double kP = 0.45;
        constexpr double kDp = 0.2; // 0.45 -> 0.65, both interior to branch M
        F1BoxQp model(kP);
        const WarmStart warm = converged_warm(model, kP);
        ASSERT_DOUBLE_EQ(warm.primal_delta, 1e-8) << "fixture premise: the default regularization";

        model.set_parameters(p_vec(kP));
        const WarmStart pred = predict(model, warm, p_vec(kDp));
        // delta * ||dp|| = 2e-9; measured 2.000e-09.
        EXPECT_LE((pred.x - F1BoxQp::x_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 5e-9)
            << "x_pred = " << pred.x.transpose()
            << " vs x_star = " << F1BoxQp::x_star(kP + kDp).transpose();

        model.set_parameters(p_vec(kP));
        const WarmStart sharp = predict(model, with_regularization(warm, kTightReg), p_vec(kDp));
        // Four orders less regularization, four orders less error: 2.001e-13.
        EXPECT_LE((sharp.x - F1BoxQp::x_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 5e-13)
            << "x_pred = " << sharp.x.transpose();
    }
    {
        // F3 free branch: x*_i(p) = i*p, affine in p, with an EQUALITY row
        // (the pin x_1 = 0) in the sensitivity system -- the only family here
        // that exercises me > 0.
        SCOPED_TRACE("F3 free branch");
        constexpr double kP = 0.20;
        constexpr double kDp = 0.15; // 0.20 -> 0.35, both below p_act = 0.5
        F3SpringChain model(/*n=*/12, /*p_act=*/0.5, /*p0=*/kP);
        const WarmStart warm = converged_warm(model, kP);

        model.set_parameters(p_vec(kP));
        const WarmStart pred = predict(model, warm, p_vec(kDp));
        // ~500 * delta * ||dp||, the chain's own conditioning: measured
        // 7.590e-07 in x and 9.900e-08 in lambda_e.
        EXPECT_LE((pred.x - model.x_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 2e-6)
            << "x_pred = " << pred.x.transpose();
        EXPECT_LE((pred.lambda_e - model.lambda_e_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 5e-7);

        model.set_parameters(p_vec(kP));
        const WarmStart sharp = predict(model, with_regularization(warm, kTightReg), p_vec(kDp));
        // Measured 7.619e-11 and 9.940e-12 -- the same four orders.
        EXPECT_LE((sharp.x - model.x_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 2e-10)
            << "x_pred = " << sharp.x.transpose();
        EXPECT_LE((sharp.lambda_e - model.lambda_e_star(kP + kDp)).lpNorm<Eigen::Infinity>(),
                  5e-11);
    }
}

// ---------------------------------------------------------------------
// 3. BOUND CROSSING, BOTH DIRECTIONS.
// ---------------------------------------------------------------------
TEST(Predictor, PredictorHandlesBoundCrossing) {
    {
        // RELAX: p = 0.15 (branch L, x2 pinned at its 0.8 upper bound,
        // z2 = -0.05) stepped to p = 0.25 (branch M, no bound active). The
        // frozen pin's predicted multiplier changes sign, so the pin must be
        // RELEASED for the prediction to land on the far-side path.
        SCOPED_TRACE("F1 0.15 -> 0.25 (bound relaxes)");
        constexpr double kP = 0.15;
        constexpr double kDp = 0.10;
        F1BoxQp model(kP);
        const WarmStart warm = converged_warm(model, kP);
        ASSERT_EQ(warm.bound_active[1], +1) << "fixture premise: x2 starts at its upper bound";

        model.set_parameters(p_vec(kP));
        const WarmStart pred = predict(model, warm, p_vec(kDp));
        const double p_far = kP + kDp;
        // The regularization floor again (measured 7.5e-10), not a
        // linearization error: branch M is affine.
        EXPECT_LE((pred.x - F1BoxQp::x_star(p_far)).lpNorm<Eigen::Infinity>(), 5e-9)
            << "x_pred = " << pred.x.transpose();
        const AnalyticActiveSet far = F1BoxQp::active_set(p_far);
        EXPECT_EQ(pred.bound_active, far.bound_active)
            << "predicted " << to_text(pred.bound_active) << " vs analytic "
            << to_text(far.bound_active);
        // The released bound's multiplier must go to (numerically) zero, not
        // merely change sign -- a free variable carries no z, and
        // predictor.h's RELAX note is about precisely that term.
#ifdef USE_ACCELERATE_SPARSE
        // D18 (docs/notes/2026-07-31-accelerate-second-pass-results.md): the
        // zero predictor.h's RELAX note describes is arithmetic cancellation
        // THROUGH THE LINEAR SOLVE, not an assignment -- MKL Pardiso happens
        // to cancel bit-exactly (EXPECT_DOUBLE_EQ, the #else arm, holds
        // there), but Accelerate's factorization leaves a residue, observed
        // at -7.7037197775489434e-34 (against a frozen z of -0.05, i.e.
        // ~1.5e-32 of it -- thirty orders below the O(||dp||) term this
        // subtraction exists to remove). 1e-12 comfortably separates that
        // residue from any genuine O(||dp||) = O(1e-1) leftover frozen value,
        // which is the only thing this assertion is actually guarding
        // against.
        EXPECT_NEAR(pred.z(1), 0.0, 1e-12);
#else
        EXPECT_DOUBLE_EQ(pred.z(1), 0.0);
#endif
        // Geometric cross-check, independent of the reported flags.
        model.set_parameters(p_vec(p_far));
        EXPECT_EQ(geometric_bound_active(model, pred.x, 1e-8), far.bound_active);
    }
    {
        // FIX: the same crossing taken the other way. p = 0.25 (branch M, no
        // bound active) stepped to p = 0.15, where x2's unconstrained target
        // 1 - p = 0.85 is above the 0.8 ceiling: the predicted x2 crosses and
        // must be CLAMPED and PINNED.
        SCOPED_TRACE("F1 0.25 -> 0.15 (bound activates)");
        constexpr double kP = 0.25;
        constexpr double kDp = -0.10;
        F1BoxQp model(kP);
        const WarmStart warm = converged_warm(model, kP);
        ASSERT_EQ(warm.bound_active[1], 0) << "fixture premise: x2 starts free";

        model.set_parameters(p_vec(kP));
        const WarmStart pred = predict(model, warm, p_vec(kDp));
        const double p_far = kP + kDp;
        EXPECT_LE((pred.x - F1BoxQp::x_star(p_far)).lpNorm<Eigen::Infinity>(), 5e-9)
            << "x_pred = " << pred.x.transpose();
        const AnalyticActiveSet far = F1BoxQp::active_set(p_far);
        EXPECT_EQ(pred.bound_active, far.bound_active)
            << "predicted " << to_text(pred.bound_active) << " vs analytic "
            << to_text(far.bound_active);
        // The clamped variable sits EXACTLY on the bound, not merely near it:
        // that is what "clamped and pinned" means as opposed to "extrapolated
        // past and then rounded".
        EXPECT_DOUBLE_EQ(pred.x(1), F1BoxQp::kBoxUpper);
        // z2 at the newly active UPPER bound must carry that side's sign, and
        // the analytic value there is p - 0.2 = -0.05.
        EXPECT_NEAR(pred.z(1), F1BoxQp::z_star(p_far)(1), 1e-8);
        // The far side has the row strictly INACTIVE (0.15 + 0.8 < 1), so a
        // row the warm start carried must not survive the step. (At p = 0.25
        // the row is weakly active, so whether the warm start carried it is a
        // solver-internal choice -- what is asserted is the far side, which is
        // determinate: there the row is strictly inactive.)
        model.set_parameters(p_vec(p_far));
        EXPECT_LT(model.eval_ci(pred.x)(0), -1e-3);
        EXPECT_EQ(pred.ineq_active[0], 0);
        EXPECT_NEAR(pred.lambda_i(0), 0.0, 1e-14);
    }
}

// ---------------------------------------------------------------------
// 4. WEAK COMPLEMENTARITY -- F1's middle branch.
// ---------------------------------------------------------------------
TEST(Predictor, PredictorKeepsWeaklyActiveRow) {
    // Across all of 0.2 <= p <= 0.8, F1's row x1 + x2 <= 1 is geometrically
    // ACTIVE with multiplier exactly 0 (the families header's STEP 1): strict
    // complementarity fails on the entire branch. A predictor that treats
    // "active" as "multiplier > 0" would drop the row; one that treats a
    // predicted multiplier of -1e-17 as a sign change would drop it too.
    constexpr double kP = 0.40;
    constexpr double kDp = 0.20; // 0.40 -> 0.60, both interior to branch M
    F1BoxQp model(kP);
    const WarmStart warm = converged_warm(model, kP);
    ASSERT_NEAR(warm.lambda_i(0), 0.0, 1e-9) << "fixture premise: the row is weakly active";

    model.set_parameters(p_vec(kP));
    const WarmStart pred = predict(model, warm, p_vec(kDp));
    const double p_far = kP + kDp;

    // (i) It does not crash, and the prediction is still accurate to the
    //     regularization floor on this (affine) branch -- the zero multiplier
    //     costs no accuracy.
    EXPECT_LE((pred.x - F1BoxQp::x_star(p_far)).lpNorm<Eigen::Infinity>(), 5e-9)
        << "x_pred = " << pred.x.transpose();
    EXPECT_NEAR(pred.lambda_i(0), 0.0, 1e-12);

    // (ii) THE DECISION, PINNED: a weakly-active row is KEPT in the predicted
    //      activity when the warm start carried it, rather than dropped for
    //      failing a strict-positivity test. See predictor.h's WEAKLY ACTIVE
    //      ROWS note for the justification (sIpopt's fix-relax pairing relaxes
    //      only on a STRICTLY negative multiplier, for the same reason).
    //      Whether the WARM START carried it is a solver-internal choice at a
    //      zero multiplier, so that is an OBSERVATION recorded here rather
    //      than a contract -- what is asserted is that predict() PRESERVES it.
    EXPECT_EQ(pred.ineq_active[0], warm.ineq_active[0])
        << "warm carried ineq_active[0] = " << static_cast<int>(warm.ineq_active[0])
        << ", prediction reports " << static_cast<int>(pred.ineq_active[0]);
    // Geometrically the row IS active on the far side, whatever the flag says.
    model.set_parameters(p_vec(p_far));
    EXPECT_NEAR(model.eval_ci(pred.x)(0), 0.0, 1e-8);

    // (iii) The prediction is still CONSUMABLE: a solve seeded by it converges
    //       to the far-side path (the (c)-style consumption the brief asks be
    //       shown not to break at a zero multiplier).
    SqpDriver driver(tight_options());
    const SqpSolution sol = driver.solve(model, model.start_point(), pred);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_LE((sol.x - F1BoxQp::x_star(p_far)).lpNorm<Eigen::Infinity>(), 1e-8);
}

// ---------------------------------------------------------------------
// 5. ACCELERATION -- the end-to-end claim.
// ---------------------------------------------------------------------
TEST(Predictor, PredictedWarmAcceleratesSolve) {
    // F2, on the curved active branch, stepped far enough that the raw warm
    // start is a genuinely stale guess: p = 0.90 -> 1.10 moves x_star by ~0.05
    // and lambda by ~0.4.
    constexpr double kP = 0.90;
    constexpr double kDp = 0.20;
    F2CircleNlp model(kP);
    const WarmStart warm = converged_warm(model, kP);

    model.set_parameters(p_vec(kP));
    const WarmStart pred = predict(model, warm, p_vec(kDp));

    const SqpOptions opts = tight_options();
    model.set_parameters(p_vec(kP + kDp));

    SqpDriver plain_driver(opts);
    const SqpSolution plain = plain_driver.solve(model, model.start_point(), warm);
    SqpDriver pred_driver(opts);
    const SqpSolution boosted = pred_driver.solve(model, model.start_point(), pred);

    ASSERT_EQ(plain.status, SqpStatus::kOptimal);
    ASSERT_EQ(boosted.status, SqpStatus::kOptimal);
    EXPECT_LE((plain.x - F2CircleNlp::x_star(kP + kDp)).norm(), 1e-8);
    EXPECT_LE((boosted.x - F2CircleNlp::x_star(kP + kDp)).norm(), 1e-8);

    // BOTH COUNTS ARE PINNED, not merely their order: "strictly fewer" alone
    // would still pass if a regression made both arms slower by the same
    // amount, which is half of what this comparison is meant to catch.
    EXPECT_EQ(plain.counters.major_iters, 4);
    EXPECT_EQ(boosted.counters.major_iters, 3);
    EXPECT_LT(boosted.counters.major_iters, plain.counters.major_iters)
        << "predicted " << boosted.counters.major_iters << " vs plain "
        << plain.counters.major_iters;
}

// ---------------------------------------------------------------------
// CONTRACT SURFACE: what predict() promises about its inputs.
// ---------------------------------------------------------------------
TEST(Predictor, PredictorNeverMutatesItsInputs) {
    constexpr double kP = 0.9;
    F2CircleNlp model(kP);
    const WarmStart warm = converged_warm(model, kP);

    model.set_parameters(p_vec(kP));
    const Vec x_before = warm.x;
    const Vec lam_before = warm.lambda_i;
    const std::vector<std::int8_t> bounds_before = warm.bound_active;

    const WarmStart pred = predict(model, warm, p_vec(0.05));

    EXPECT_EQ(warm.x, x_before);
    EXPECT_EQ(warm.lambda_i, lam_before);
    EXPECT_EQ(warm.bound_active, bounds_before);
    EXPECT_DOUBLE_EQ(model.p(), kP) << "predict() must restore the model's entry parameters";
    // The structure fingerprint is p-independent by ParametricNlpModel's own
    // precondition, so it carries across the step unchanged -- which is what
    // makes the prediction ingestible by a solve at p + dp at all.
    EXPECT_EQ(pred.structure_hash, warm.structure_hash);
    EXPECT_TRUE(pred.valid);
    // predict() never reuses a caller's hot handle (predictor.h's NO HOT-START
    // REUSE note), so it never claims one either.
    EXPECT_EQ(pred.hot, nullptr);
}

TEST(Predictor, PredictorIsTheIdentityOnAZeroStep) {
    constexpr double kP = 0.45;
    F1BoxQp model(kP);
    const WarmStart warm = converged_warm(model, kP);

    model.set_parameters(p_vec(kP));
    const WarmStart pred = predict(model, warm, Vec::Zero(1));
    EXPECT_EQ(pred.x, warm.x);
    EXPECT_EQ(pred.lambda_i, warm.lambda_i);
    EXPECT_EQ(pred.bound_active, warm.bound_active);
    EXPECT_TRUE(pred.valid);
    // "on every path, including the zero-step identity" -- predictor.h's NO
    // HOT-START REUSE note says so, so this path asserts it too.
    EXPECT_EQ(pred.hot, nullptr);
}

// ---------------------------------------------------------------------
// THE OUTCOME OBSERVABLE (fix round 1). Two of the three paths return the
// IDENTITY prediction, and Task 10's continuation accounting has to tell them
// apart -- a sweep in which every predict() degraded must not be reportable as
// a sweep in which every predict() succeeded.
// ---------------------------------------------------------------------
TEST(Predictor, PredictorReportsWhichPathItTook) {
    constexpr double kP = 0.45;
    F1BoxQp model(kP);
    const WarmStart warm = converged_warm(model, kP);

    // The sentinel is set to the WRONG value before each call, so a predict()
    // that failed to write would leave the assertion failing rather than
    // accidentally passing.
    PredictorOutcome outcome = PredictorOutcome::kDegraded;
    model.set_parameters(p_vec(kP));
    predict(model, warm, p_vec(0.1), PredictorOptions{}, &outcome);
    EXPECT_EQ(outcome, PredictorOutcome::kPredicted);

    outcome = PredictorOutcome::kPredicted;
    predict(model, warm, Vec::Zero(1), PredictorOptions{}, &outcome);
    EXPECT_EQ(outcome, PredictorOutcome::kZeroStep);

    // Reporting is optional: the four-argument form must still compile and
    // behave identically (it is what every other test in this file calls).
    EXPECT_NO_THROW(predict(model, warm, p_vec(0.1)));
}

TEST(Predictor, DegradationIsReportedRatherThanThrown) {
    constexpr double kP = 0.45;
    // The warm start comes from an ORDINARY F1 (the rejecting model refuses to
    // be re-posed at all, so it cannot be solved through the usual helper);
    // the two are the same problem at the same p, which is all predict()
    // requires of a warm start.
    F1BoxQp plain(kP);
    const WarmStart warm = converged_warm(plain, kP);

    // BOTH exception types, because the net is by phase and not by type: a
    // std::invalid_argument raised AFTER caller validation is a numerical
    // failure (schur_complement.h raises two of its own that way), not a
    // caller error, and must degrade rather than escape.
    for (bool as_invalid_argument : {false, true}) {
        SCOPED_TRACE(as_invalid_argument ? "std::invalid_argument" : "std::runtime_error");
        ProbeRejectingF1 model(kP, as_invalid_argument);
        PredictorOutcome outcome = PredictorOutcome::kPredicted;
        WarmStart pred;
        // It does not THROW -- that is the whole contract: a predictor that
        // cannot predict declines, it does not kill the caller's sweep.
        ASSERT_NO_THROW(pred = predict(model, warm, p_vec(0.1), PredictorOptions{}, &outcome));
        EXPECT_EQ(outcome, PredictorOutcome::kDegraded);
        // The returned object is the caller's own warm start -- never a
        // half-updated point.
        EXPECT_EQ(pred.x, warm.x);
        EXPECT_EQ(pred.lambda_i, warm.lambda_i);
        EXPECT_EQ(pred.bound_active, warm.bound_active);
        EXPECT_TRUE(pred.valid);
        EXPECT_EQ(pred.hot, nullptr);
        // And the model is left exactly as it was found.
        EXPECT_DOUBLE_EQ(model.p(), kP);
    }
}

// allow_activity_change = false is the RAW frozen-set step: the same F1
// crossing as above must then extrapolate straight past the bound instead of
// clamping to it, which is the observable that distinguishes the two modes.
TEST(Predictor, FrozenActivityModeTakesTheRawStep) {
    constexpr double kP = 0.25;
    constexpr double kDp = -0.10;
    F1BoxQp model(kP);
    const WarmStart warm = converged_warm(model, kP);

    PredictorOptions frozen;
    frozen.allow_activity_change = false;
    model.set_parameters(p_vec(kP));
    const WarmStart pred = predict(model, warm, p_vec(kDp), frozen);

    EXPECT_GT(pred.x(1), F1BoxQp::kBoxUpper + 1e-3)
        << "x_pred = " << pred.x.transpose() << " should have crossed the 0.8 ceiling unrepaired";
    EXPECT_EQ(pred.bound_active[1], 0);
}

TEST(Predictor, PredictorRejectsMismatchedDimensions) {
    F1BoxQp model(0.45);
    const WarmStart warm = converged_warm(model, 0.45);
    model.set_parameters(p_vec(0.45));

    // dp against parameter_dim()
    EXPECT_THROW(predict(model, warm, Vec::Zero(2)), std::invalid_argument);
    // a warm start from a DIFFERENT model shape
    WarmStart foreign = warm;
    foreign.x = Vec::Zero(5);
    EXPECT_THROW(predict(model, foreign, p_vec(0.1)), std::invalid_argument);
    // a cold (default-constructed) object has nothing to predict FROM
    EXPECT_THROW(predict(model, WarmStart{}, p_vec(0.1)), std::invalid_argument);
    // a non-finite step
    EXPECT_THROW(predict(model, warm, p_vec(std::nan(""))), std::invalid_argument);
    // a nonsensical finite-difference step
    PredictorOptions bad;
    bad.fd_step_scale = 0.0;
    EXPECT_THROW(predict(model, warm, p_vec(0.1), bad), std::invalid_argument);
    // and none of the rejections may leave the model re-posed
    EXPECT_DOUBLE_EQ(model.p(), 0.45);
}

// The predictor is an ACCELERATOR: its whole output must be safe to feed into
// a solve even where its activity guess is wrong. F2 stepped ACROSS its own
// activation threshold is the case a frozen-set linearization has to REPAIR
// (the constraint switches from inactive to active -- predictor.h's ADD move),
// and the assertion is not accuracy but SAFETY: the seeded solve converges to
// the true far-side path.
TEST(Predictor, PredictionAcrossAnInequalityThresholdIsStillSafeToConsume) {
    const double p_star = F2CircleNlp::p_activation();
    const double kP = p_star - 0.05;
    const double kDp = 0.10; // lands 0.05 past the threshold
    F2CircleNlp model(kP);
    const WarmStart warm = converged_warm(model, kP);
    ASSERT_FALSE(F2CircleNlp::constraint_active(kP));
    ASSERT_TRUE(F2CircleNlp::constraint_active(kP + kDp));

    model.set_parameters(p_vec(kP));
    const WarmStart pred = predict(model, warm, p_vec(kDp));
    // The ADD move fired: the row the warm start had inactive is active in the
    // prediction, and its multiplier is the positive one the far side carries.
    EXPECT_EQ(pred.ineq_active[0], 1);
    EXPECT_GT(pred.lambda_i(0), 0.0);

    model.set_parameters(p_vec(kP + kDp));
    SqpDriver driver(tight_options());
    const SqpSolution sol = driver.solve(model, model.start_point(), pred);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_LE((sol.x - F2CircleNlp::x_star(kP + kDp)).norm(), 1e-8);
}

// The DROP move, on a STRICTLY complementary row -- the mirror of the test
// above and the only fixture here whose dropped row carries a NONZERO
// multiplier. F1's row is weakly active (lambda == 0), so dropping it there
// cannot distinguish a correct multiplier update from no update at all; F2
// stepped DOWN across p* can, because the row it deactivates arrives carrying
// lambda = ||a(p)|| - 1 > 0 and must leave carrying exactly zero.
TEST(Predictor, PredictorDropsAStrictlyActiveRowAcrossItsThreshold) {
    const double p_star = F2CircleNlp::p_activation();
    const double kP = p_star + 0.05;
    const double kDp = -0.10; // lands 0.05 short of the threshold
    F2CircleNlp model(kP);
    const WarmStart warm = converged_warm(model, kP);
    ASSERT_TRUE(F2CircleNlp::constraint_active(kP));
    ASSERT_FALSE(F2CircleNlp::constraint_active(kP + kDp));
    ASSERT_GT(warm.lambda_i(0), 0.05) << "fixture premise: the row leaves with a real multiplier";

    model.set_parameters(p_vec(kP));
    const WarmStart pred = predict(model, warm, p_vec(kDp));

    EXPECT_EQ(pred.ineq_active[0], 0);
    // EXACTLY zero: a deactivated row's multiplier is not "small", it is gone.
    // Leaving the frozen 0.09 in place would be a first-order error in the
    // stationarity the next solve is seeded with, and predictor.h's DROP note
    // is about precisely this right-hand side.
    EXPECT_DOUBLE_EQ(pred.lambda_i(0), 0.0);
    // And the point lands on the far-side (unconstrained) path to the usual
    // second order: measured 2.7e-3 at ||dp|| = 0.1.
    EXPECT_LE((pred.x - F2CircleNlp::x_star(kP + kDp)).norm(), 1e-2)
        << "x_pred = " << pred.x.transpose();
}

// =====================================================================
// PHASE-5 TASK 6 -- THE RATIO-TESTED PATH (predictor.h's own RATIO TEST
// section), i.e. the repair of the Phase-4 battery's open item O-2.
//
// WHAT O-2 WAS. The four fix-relax repairs used to be evaluated at the FULL
// step: solve the frozen-set system for all of dp, then change the status of
// every entity the resulting point came out inconsistent for. Across an
// ACTIVATION THRESHOLD that is wrong in a specific and expensive way -- the
// frozen-set step overshoots, so far more entities look inconsistent than
// actually change, and all of them are changed at once. The battery measured
// the cost on F3 at n = 1000 and the tests below re-measure it as the thing
// that must not come back.
//
// WHY THE FIXTURES ARE THE ONES THEY ARE:
//   * F3 AT n = 1000 IS THE O-2 CELL ITSELF (docs/notes/2026-07-30-warm-start-
//     battery-results.md section 5.3). It is also the cleanest possible
//     statement of the defect, because its true active set across the
//     threshold is exactly ONE variable at every n, so "how many did the
//     predictor pin" has an unambiguous right answer.
//   * F5 IS THE TIE. Its row and its bound cross at literally the same point
//     on the path, so it is the fixture that fails if a ratio test advances to
//     the first crossing and forgets that a crossing can have more than one
//     member. NOTE, honestly, that this one does NOT discriminate against the
//     PRE-Task-6 scheme -- the old full-step loop got F5 right, because at
//     n = 2 there is nothing to overshoot. It discriminates against a NAIVE
//     ratio test, which is a different and equally real way to get this wrong,
//     and it was written after a mutation run showed that a ratio test which
//     applies only the single argmin breakpoint breaks exactly here.
//     NOTE FURTHER, and this is a correction the review round supplied (M-2):
//     F5 does NOT cover predictor_detail::kTauTieTol's numeric VALUE either.
//     Its two crossing points are bit-identical doubles, so removing the
//     tolerance band entirely and comparing `tau == tau_star` leaves F5
//     passing. What covers the band is F7 below: without it, a symmetric pair
//     of nodes costs two rounds instead of one, the whole budget sweep shifts
//     by one column, and the F7 cell's pinned minor count moves 9 -> 8.
//   * F7 IS THE CURVED, MANY-JUNCTION CASE (tests/sqp/support/scale_problems.h),
//     and the one where the old scheme did not merely cost minors but produced
//     a seed the consuming solve could not converge from at all.
// =====================================================================

// (1) THE O-2 CELL. F3's ceiling admits exactly ONE active node at the
// optimum on the clamped branch -- never a front (the families header's
// derivation) -- so a prediction that steps across p_act = 0.5 has a
// one-variable answer to get right. The pre-Task-6 predictor pinned NINETY at
// n = 1000; the ratio-tested path pins the one, and the counters below are the
// point of the whole task.
//
// PINNED AS OBSERVED VALUES (MKL Pardiso, clang++, Release and Debug agreeing).
// MEASURED AGAINST THE PRE-TASK-6 HEADER, which on this exact fixture reports
// 90 pinned variables, 4.45e+1 of prediction error, and 6 majors / 195 minors /
// 6 factorizations in the consuming solve -- the battery's §5.3 numbers,
// reproduced here at the predict() level rather than through a sweep. The
// comparators in the same currency: the SAME step seeded by the UNPREDICTED
// warm start costs 8 majors / 24 minors, and a cold solve at p1 costs 9 / 27.
TEST(PredictorRatioTest, InheritsTheTrueActivitySetAcrossAnActivationThreshold) {
    constexpr Index kN = 1000;
    constexpr double kP = 0.35;  // free branch
    constexpr double kDp = 0.20; // lands on the clamped branch, crossing p_act = 0.5
    F3SpringChain model(kN, /*p_act=*/0.5, kP);
    ASSERT_DOUBLE_EQ(model.p_activation(), 0.5);

    SqpOptions opts;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    opts.max_iter = 60;
    opts.adaptive_mu = false;
    const WarmStart warm = converged_warm(model, kP, opts);
    ASSERT_EQ(warm.bound_active, model.active_set(kP).bound_active)
        << "fixture premise: nothing is on the ceiling at p = 0.35";

    model.set_parameters(p_vec(kP));
    PredictorOutcome outcome = PredictorOutcome::kDegraded;
    double reached_t = -1.0;
    const WarmStart pred =
        predict(model, warm, p_vec(kDp), PredictorOptions{}, &outcome, &reached_t);
    ASSERT_EQ(outcome, PredictorOutcome::kPredicted);
    // EXACTLY 1.0: F3's path is piecewise affine, so one breakpoint IS the whole
    // path and a prediction that stopped short of p + dp here would be a defect,
    // not a budget. Written as == rather than NEAR because predict() ASSIGNS the
    // terminal t (predictor.h's WHAT `reached_t` MEANS note) precisely so that
    // this assertion is legitimate.
    EXPECT_EQ(reached_t, 1.0) << "the ratio-tested path did not reach p + dp";

    // THE ASSERTION THE TASK EXISTS FOR: the predicted activity IS the analytic
    // far-side activity, field for field -- one variable, the last one.
    EXPECT_EQ(pred.bound_active, model.active_set(kP + kDp).bound_active)
        << "pinned "
        << std::count_if(pred.bound_active.begin(), pred.bound_active.end(),
                         [](std::int8_t b) { return b != 0; })
        << " variables; the analytic far side has exactly one (index " << kN - 1 << ")";

    // And the point is now the warm start's OWN error away from x*(p + dp)
    // rather than nine per cent of the solution. 9.6e-2 is not a predictor
    // floor: F3's Hessian is the path Laplacian, whose conditioning is O(n^2),
    // so the warm start's 1e-8 KKT residual is already ~1e-2 of x error at
    // n = 1000 and the prediction inherits it. The bound is stated against the
    // pre-Task-6 44.5 it replaces, not as an accuracy claim about predict()
    // -- for that see PredictorIsExactUpToRegularizationOnAffinePaths, and
    // this file's banner item 2 for the floor those measure.
    EXPECT_LE((pred.x - model.x_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 1e-1);

    // THE COST, which is what O-2 was reported in. Pinned exactly.
    model.set_parameters(p_vec(kP + kDp));
    SqpDriver seeded(opts);
    const SqpSolution boosted = seeded.solve(model, pred.x, pred);
    ASSERT_EQ(boosted.status, SqpStatus::kOptimal);
    EXPECT_EQ(boosted.counters.major_iters, 1);
    EXPECT_EQ(boosted.counters.qp_minor_iters, 2);
    EXPECT_EQ(boosted.counters.factorizations, 1);

    model.set_parameters(p_vec(kP + kDp));
    SqpDriver plain(opts);
    const SqpSolution unpredicted = plain.solve(model, warm.x, warm);
    ASSERT_EQ(unpredicted.status, SqpStatus::kOptimal);
    EXPECT_EQ(unpredicted.counters.qp_minor_iters, 24);
    // The comparison the battery could not make before: the predicted seed is
    // now CHEAPER than the unpredicted one in the currency O-2 named, not 10x
    // dearer.
    EXPECT_LT(boosted.counters.qp_minor_iters, unpredicted.counters.qp_minor_iters);
    EXPECT_LE((boosted.x - model.x_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 1e-6);
}

// (2) A BREAKPOINT CAN HAVE MORE THAN ONE MEMBER. F5's row (-x1 + p <= 0) and
// F5's lower bound (p <= x2) are both driven by p through the same difference,
// from a warm start where x = (0, 0), so they reach their respective
// thresholds at the SAME point on the path. A ratio test that advanced to the
// first crossing and applied only ONE change would need a second breakpoint to
// pick up the other; this pins that it does not, by running the prediction on
// a budget of exactly ONE breakpoint and requiring it to still be exact.
TEST(PredictorRatioTest, SimultaneousCrossingsAreOneBreakpoint) {
    constexpr double kP = -0.1;  // both constraints inactive
    constexpr double kDp = 0.25; // lands at p = 0.15, both active
    F5MovingThreshold model(kP);
    const WarmStart warm = converged_warm(model, kP);
    ASSERT_EQ(warm.ineq_active[0], 0);
    ASSERT_EQ(warm.bound_active[1], 0);

    PredictorOptions one_breakpoint;
    one_breakpoint.max_activity_rounds = 1;
    model.set_parameters(p_vec(kP));
    PredictorOutcome outcome = PredictorOutcome::kDegraded;
    double reached_t = -1.0;
    const WarmStart pred = predict(model, warm, p_vec(kDp), one_breakpoint, &outcome, &reached_t);
    ASSERT_EQ(outcome, PredictorOutcome::kPredicted);
    // ONE breakpoint was enough to reach p + dp, which is the whole claim: if
    // the two crossings were taken as two breakpoints this budget would have
    // truncated and reached_t would be 0.4 (the shared crossing point), not 1.
    EXPECT_EQ(reached_t, 1.0) << "a budget of ONE was not enough -- the shared crossing was "
                                 "split across two breakpoints";

    EXPECT_EQ(pred.ineq_active[0], 1) << "the ADD repair did not fire on the shared breakpoint";
    EXPECT_EQ(pred.bound_active[1], -1) << "the FIX repair did not fire on the shared breakpoint";
    // Both of F5's branches are affine, so one breakpoint is the WHOLE path
    // and the prediction is exact to the regularization floor.
    EXPECT_LE((pred.x - F5MovingThreshold::x_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 5e-9)
        << "x_pred = " << pred.x.transpose();
    EXPECT_NEAR(pred.lambda_i(0), F5MovingThreshold::lambda_i_star(kP + kDp)(0), 5e-9);
    EXPECT_NEAR(pred.z(1), F5MovingThreshold::z_star(kP + kDp)(1), 5e-9);
}

// (3) TRUNCATION IS CONSERVATIVE, NOT WRONG -- AND IT IS VISIBLE. A budget of
// ZERO breakpoints stops the path at the first crossing, so on the same F5 step
// the prediction must land exactly ON the threshold -- x = (0, 0) at p = 0,
// with nothing yet active -- rather than extrapolating past it (which is what
// allow_activity_change = false does, and the two must stay distinguishable).
// The result is still a WarmStart a solve consumes.
//
// AND `reached_t` REPORTS IT. F5's shared crossing sits at p = 0, i.e. 0.4 of
// the way along dp = 0.25 from p = -0.1, so a caller that asked is told the
// prediction covered 40 % of the step it requested. That observable is the
// review round's Important-1: without it a truncated prediction is
// indistinguishable from a complete one, since both report kPredicted.
TEST(PredictorRatioTest, AZeroRoundBudgetStopsAtTheFirstCrossing) {
    constexpr double kP = -0.1;
    constexpr double kDp = 0.25;
    F5MovingThreshold model(kP);
    const WarmStart warm = converged_warm(model, kP);

    PredictorOptions truncate;
    truncate.max_activity_rounds = 0;
    model.set_parameters(p_vec(kP));
    PredictorOutcome outcome = PredictorOutcome::kDegraded;
    double reached_t = -1.0;
    const WarmStart pred = predict(model, warm, p_vec(kDp), truncate, &outcome, &reached_t);
    EXPECT_EQ(outcome, PredictorOutcome::kPredicted)
        << "truncation is a prediction, not a degradation -- see "
           "PredictorOptions::max_activity_rounds";
    // 0.4 = (0 - (-0.1)) / 0.25, the analytic position of the shared crossing.
    EXPECT_NEAR(reached_t, 0.4, 1e-12) << "reached_t must locate the truncation, not merely "
                                          "signal it";
    EXPECT_LT(reached_t, 1.0);

    // p = 0 is where both thresholds sit and x*(0) = (0, 0).
    EXPECT_LE((pred.x - F5MovingThreshold::x_star(0.0)).lpNorm<Eigen::Infinity>(), 1e-12)
        << "x_pred = " << pred.x.transpose();
    EXPECT_EQ(pred.ineq_active[0], 0) << "the path stopped AT the crossing, not past it";
    EXPECT_EQ(pred.bound_active[1], 0);

    model.set_parameters(p_vec(kP + kDp));
    SqpDriver driver(tight_options());
    const SqpSolution sol = driver.solve(model, pred.x, pred);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_LE((sol.x - F5MovingThreshold::x_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 1e-8);
}

// (3b) THE WORST CASE THE ROUND BUDGET CAN PRODUCE, AND THE ONE THE REVIEW
// ROUND (Important-1) required to be observable: a prediction that returns THE
// LITERAL IDENTITY while reporting kPredicted.
//
// It needs a ZERO-LENGTH first crossing, which needs a STALE warm start -- a
// point already marginally on the wrong side of a constraint the step is moving
// further away from. predictor.h's `crossing()` clamps such an entity's ratio
// to 0 (documented there), so the first breakpoint is at t = 0, and a budget of
// 0 stops the path before applying it. `predict()` then returns its input
// unchanged, having computed a step and taken none of it.
//
// WHY THIS MATTERS RATHER THAN BEING A CURIOSITY: PredictorOutcome exists (Task
// 9, fix round 1, at a review's request) precisely because two of its three
// paths return the identity and "a caller's accounting must not conflate them".
// A truncation-to-zero is a THIRD way to return the identity, and it reports
// the SAME enumerator a full prediction does. `reached_t == 0.0` is what tells
// them apart, and that is the whole reason the out-parameter was added.
//
// A stale warm start is explicitly inside predict()'s contract ("A
// stale-but-well-shaped warm start is NOT rejected"), so this is a legal call,
// not a poked-at invariant.
TEST(PredictorRatioTest, ATruncationToTheIdentityIsVisibleToTheCaller) {
    constexpr double kP = -0.1;
    constexpr double kDp = 0.25;
    F5MovingThreshold model(kP);
    WarmStart stale = converged_warm(model, kP);
    ASSERT_EQ(stale.bound_active[1], 0) << "fixture premise: x2 is FREE at p = -0.1";
    // x2 marginally BELOW its lower bound (which is p = -0.1) and moving away
    // from it -- the bound rises with p at rate dp while dx2 is 0, so the gap
    // only widens and the crossing is behind us: tau = 0.
    stale.x(1) = kP - 1e-6;

    PredictorOptions truncate;
    truncate.max_activity_rounds = 0;
    model.set_parameters(p_vec(kP));
    PredictorOutcome outcome = PredictorOutcome::kDegraded;
    double reached_t = -1.0;
    const WarmStart pred = predict(model, stale, p_vec(kDp), truncate, &outcome, &reached_t);

    // The pair, and only the pair, is unambiguous.
    EXPECT_EQ(outcome, PredictorOutcome::kPredicted)
        << "a step WAS computed, so kDegraded would be a lie";
    EXPECT_EQ(reached_t, 0.0) << "the caller cannot see that nothing was traversed";
    // ...and the object really is the identity, which is what makes the report
    // load-bearing rather than decorative.
    EXPECT_EQ(pred.x, stale.x);
    EXPECT_EQ(pred.bound_active, stale.bound_active);
    EXPECT_EQ(pred.ineq_active, stale.ineq_active);

    // The default budget does NOT truncate here -- the same call with the
    // shipping options takes the step -- so this test is about the budget, not
    // about staleness.
    model.set_parameters(p_vec(kP));
    double full_t = -1.0;
    const WarmStart full = predict(model, stale, p_vec(kDp), PredictorOptions{}, &outcome, &full_t);
    EXPECT_EQ(full_t, 1.0);
    EXPECT_NE(full.x, stale.x);
}

// (4) THE CURVED, MANY-JUNCTION CASE, AND THE STRONGEST STATEMENT AVAILABLE
// OF WHAT O-2 COST. F7 is a collocation chain whose path constraint is a ball
// on the state and whose active WINDOW widens with p, so a step across its
// threshold changes many rows at once and the true junctions are NOT where a
// single frozen-set step puts them.
//
// The pre-Task-6 predictor, on this exact cell, pinned 34 CONTROL BOUNDS that
// are free at the solution and handed the consuming solve a seed it could not
// converge from: kNumericalError after 1000 QP minor iterations and 15
// factorizations. That is the failure mode this test exists to keep out, and
// it is a stronger claim than a minor count -- the assertion is CONVERGENCE.
//
// THE HONEST RESIDUAL, pinned here rather than left in prose: the ratio-tested
// path is still a FIRST-ORDER path (predictor.h's max_activity_rounds note),
// and on a family this curved it UNDER-activates -- the prediction carries far
// fewer rows than the analytic far side. It is a good seed and no longer a
// catastrophic one; it is not an exact one, and this fixture is where that
// shows.
TEST(PredictorRatioTest, SurvivesACurvedManyJunctionThresholdCrossing) {
    using test_support::F7CollocationChain;
    constexpr Index kNodes = 20;
    constexpr double kP = 0.55;  // window already open
    constexpr double kDp = 0.20; // widens it a long way
    F7CollocationChain model(kNodes, /*states=*/3, /*controls=*/2, kP, /*radius=*/1.0);

    SqpOptions opts;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    opts.max_iter = 60;
    opts.adaptive_mu = false;
    const WarmStart warm = converged_warm(model, kP, opts);

    model.set_parameters(p_vec(kP));
    PredictorOutcome outcome = PredictorOutcome::kDegraded;
    double reached_t = -1.0;
    const WarmStart pred =
        predict(model, warm, p_vec(kDp), PredictorOptions{}, &outcome, &reached_t);
    ASSERT_EQ(outcome, PredictorOutcome::kPredicted);

    // THIS CELL TRUNCATES AT THE SHIPPING DEFAULT, and says so. F7's window
    // opens at many nodes across one dp, so the default budget of 4 breakpoints
    // is spent early: the prediction covers only ~13.5 % of the requested step.
    // Recorded as an assertion rather than a footnote because it is the honest
    // answer to "is truncation reachable in practice" -- it is, on this
    // project's one curved family, at the default -- and because it is what
    // makes `reached_t` worth having (predictor.h's WHAT `reached_t` MEANS).
    // The prediction is still USEFUL at that length: the consuming solve below
    // converges in fewer minors than the plain warm arm's 13.
    EXPECT_GT(reached_t, 0.0);
    EXPECT_LT(reached_t, 1.0) << "if this cell stops truncating, the budget or the family "
                                 "changed and this test's prose is stale";
    EXPECT_NEAR(reached_t, 0.135, 0.005);

    // No spurious box activity: F7's controls are strictly inside their box at
    // every p in the design range, and the pre-Task-6 prediction put 34 of them
    // on it.
    EXPECT_EQ(std::count_if(pred.bound_active.begin(), pred.bound_active.end(),
                            [](std::int8_t b) { return b != 0; }),
              0)
        << "the prediction pinned control bounds that are free at the solution";

    model.set_parameters(p_vec(kP + kDp));
    SqpDriver seeded(opts);
    const SqpSolution boosted = seeded.solve(model, pred.x, pred);
    // THE CLAIM: it converges at all. Pre-Task-6 this was kNumericalError.
    ASSERT_EQ(boosted.status, SqpStatus::kOptimal) << "status = " << to_string(boosted.status);
    EXPECT_EQ(boosted.counters.major_iters, 3);
    EXPECT_EQ(boosted.counters.qp_minor_iters, 9);
    EXPECT_LE((boosted.x - model.x_star(kP + kDp)).lpNorm<Eigen::Infinity>(), 1e-6);

    // THE RESIDUAL, stated as a measurement: the prediction is short of the
    // analytic far-side window, which is the linearization's limit and not an
    // activity-inheritance defect.
    const auto analytic = model.active_set(kP + kDp);
    const auto count = [](const auto &v) {
        return std::count_if(v.begin(), v.end(), [](auto e) { return e != 0; });
    };
    EXPECT_GT(count(analytic.ineq_active), count(pred.ineq_active))
        << "if this ever fails the residual is gone and this test's prose is stale";
}

// (5) The round budget is a caller input and is validated like every other
// one (project rule T6).
TEST(PredictorRatioTest, RejectsANegativeRoundBudget) {
    F1BoxQp model(0.45);
    const WarmStart warm = converged_warm(model, 0.45);
    model.set_parameters(p_vec(0.45));
    PredictorOptions bad;
    bad.max_activity_rounds = -1;
    EXPECT_THROW(predict(model, warm, p_vec(0.1), bad), std::invalid_argument);
    EXPECT_DOUBLE_EQ(model.p(), 0.45) << "a rejection may not leave the model re-posed";
}

// =====================================================================
// PHASE-6 FINAL FIX WAVE (W1) -- **THE EMITTED PRICE IS NEVER NEGATIVE.**
//
// THE BYPASS THIS PINS. `lambda_i >= 0` is a WarmStart PRECONDITION
// (warm_start.h's SIGN CONVENTIONS), and sqp_driver.h gates it at kSeeded ONLY,
// on the argument that every producer able to clear a HASH gate is either
// non-negative by construction or "bounded by 1e-9 relative (predictor.h's
// kDualSignTol)". predict() carries structure_hash forward, so its output does
// clear the hash gate -- and the second half of that argument was reading a
// RELATIVE bound as though it were an absolute one. The DROP ratio test retains
// a row whose end-of-segment multiplier is >= -kDualSignTol * max(1, |lambda|):
// at |lambda| = 1e9 that admits -1, which is not noise.
//
// THE FIXTURE MAKES THE RETAINED VALUE EXACT RATHER THAN INCIDENTAL, because a
// tuned near-miss on a curved model would be pinning a bisection. On
//
//     min  -A (1 - p) x + 1/2 x^2      s.t.  cI(x) = x - 1 <= 0,   n = 1
//
// the row binds while A(1 - p) > 1, with lambda(p) = A (1 - p) - 1 EXACTLY --
// affine in p. **THE OBJECTIVE'S CURVATURE IS 1 AND ONLY ITS GRADIENT CARRIES
// A, WHICH IS WHAT MAKES THE FIXTURE ROBUST**: the predictor's frozen-set
// sensitivity is [H, 1; 1, -mu][dx; dlambda] = [-A dp; 0], so dlambda =
// -A dp / (1 + mu H), so the RELATIVE error in the end-of-segment multiplier is
// mu * H. The retention band is 1e-9 RELATIVE (kDualSignTol), so a fixture can
// only land inside it when mu * H << 1e-9 -- which is why the curvature must
// NOT carry A: with H = 1 and the tightened mu = 1e-12 below the error is 1e-12
// relative (about 1e-3 absolute against a 1.0-wide band), whereas the same
// model written with H = A would put mu * H at 1e-3 and could not be aimed at
// the band at all.
//
// Warm at p = 0 gives lambda = A - 1 = 1e9 - 1, so the band
// kDualSignTol * max(1, |lambda|) is ~1.0 wide. The step is chosen from the
// MEASURED warm price -- dp = (lambda_warm + 1/2) / A -- so the path ends at
// lambda = -0.5 +/- 1e-3: strictly negative, materially so, and comfortably
// INSIDE the band, so the ratio test KEEPS the row and (pre-W1) emitted -0.5.
// =====================================================================
class W1ScaledPriceLine : public ParametricNlpModel {
  public:
    explicit W1ScaledPriceLine(double p0) : p_(p0) {}

    Index n() const override { return 1; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    // The unconstrained minimizer, and the value the row is compared against.
    double slope() const { return kScale * (1.0 - p_); }
    double eval_f(const Vec &x) const override { return -slope() * x(0) + 0.5 * x(0) * x(0); }
    Vec eval_grad(const Vec &x) const override { return Vec::Constant(1, x(0) - slope()); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(0) - 1.0); }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatU h(1, 1);
        h.insert(0, 0) = obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 1);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 1);
        j.insert(0, 0) = 1.0;
        j.makeCompressed();
        return j;
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    // AT the constrained solution, so the cold solve below is one major and its
    // multiplier is the analytic one rather than an iteration's residue.
    Vec start_point() const override { return Vec::Constant(1, 1.0); }

    Index parameter_dim() const override { return 1; }
    Vec parameters() const override { return Vec::Constant(1, p_); }
    void set_parameters(const Vec &p) override { p_ = p(0); }

    static constexpr double kScale = 1e9;
    // lambda*(p) = A (1 - p) - 1 while the row binds.
    static double lambda_star(double p) { return kScale * (1.0 - p) - 1.0; }

  private:
    double p_;
    Vec lower_ = Vec::Constant(1, -1e20);
    Vec upper_ = Vec::Constant(1, 1e20);
};

// A price of 1e9 puts the driver's own dual regularization ~10 into the
// stationarity measure, so tight_options()' 1e-11 is unreachable here and would
// be measuring the wrong thing. What this fixture needs from the warm start is
// the SIGN of a multiplier, not eleven digits of it.
SqpOptions scaled_price_options() {
    SqpOptions opts;
    opts.kkt_tol = 1e-6;
    opts.feas_tol = 1e-6;
    opts.max_iter = 40;
    return opts;
}

TEST(Predictor, TheEmittedInequalityPriceIsNeverNegative) {
    W1ScaledPriceLine model(0.0);
    const WarmStart warm =
        with_regularization(converged_warm(model, 0.0, scaled_price_options()), kTightReg);
    ASSERT_NEAR(W1ScaledPriceLine::lambda_star(0.0), warm.lambda_i(0), 1e3)
        << "fixture premise: the warm price is ~1e9, so the retention band is ~1.0 wide";
    ASSERT_EQ(warm.ineq_active[0], 1) << "fixture premise: the row binds at p = 0";

    // Chosen from the MEASURED price so the path ends at lambda = -0.5 whatever
    // the warm solve's last digits were -- see the fixture banner.
    const double kDp = (warm.lambda_i(0) + 0.5) / W1ScaledPriceLine::kScale;
    model.set_parameters(p_vec(0.0));
    const WarmStart pred = predict(model, warm, p_vec(kDp));

    // The ratio test KEPT the row -- the retained value is inside the band, so
    // no DROP breakpoint fires and this is the bypass, not a drop.
    EXPECT_EQ(pred.ineq_active[0], 1)
        << "fixture premise: the multiplier stays inside kDualSignTol * |lambda|, so the row is "
           "retained rather than dropped -- which is exactly the path W1 closes";
    // THE PIN. Pre-W1 this read -0.5: a materially negative price on an ACTIVE
    // row, in an object carrying a MATCHING structure_hash, i.e. an ingest at
    // kWarm where nothing gates the sign at all.
    EXPECT_GE(pred.lambda_i(0), 0.0)
        << "predictor.h emitted " << pred.lambda_i(0)
        << "; WarmStart's SIGN CONVENTIONS make lambda_i >= 0 a precondition its own producers "
           "must honour";
    EXPECT_DOUBLE_EQ(pred.lambda_i(0), 0.0) << "clamped to zero, not merely made small";

    // AND THE OBJECT IS STILL CONSUMABLE: the clamp costs the next solve
    // majors, never the answer. Just past the crossing the row no longer binds,
    // so the truth is the unconstrained minimizer x = A (1 - p) ~ 0.5.
    model.set_parameters(p_vec(kDp));
    SqpDriver driver(scaled_price_options());
    const SqpSolution sol = driver.solve(model, pred.x, pred);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), model.slope(), 1e-6);
    EXPECT_LT(model.slope(), 1.0) << "fixture premise: the row is no longer binding at p + dp";
}

// THE FROZEN-SET PATH REACHES THE SAME EMISSION WITHOUT THE RATIO TEST, which
// is why W1's clamp is unconditional on the sign rather than restricted to
// kDualSignTol's band: `allow_activity_change = false` takes the whole step raw,
// so the emitted multiplier can be negative by any amount at all.
TEST(Predictor, TheFrozenSetStepAlsoEmitsANonNegativePrice) {
    W1ScaledPriceLine model(0.0);
    const WarmStart warm = converged_warm(model, 0.0, scaled_price_options());
    model.set_parameters(p_vec(0.0));

    PredictorOptions frozen;
    frozen.allow_activity_change = false;
    const WarmStart pred = predict(model, warm, p_vec(1.5), frozen);

    EXPECT_EQ(pred.ineq_active[0], 1) << "the frozen set is frozen -- the row stays nominated";
    EXPECT_GE(pred.lambda_i(0), 0.0)
        << "pre-W1 this emitted lambda = A (1 - 1.5) - 1 = -5e8 on an ACTIVE row -- five orders "
           "OUTSIDE the retention band, which is why the clamp is unconditional on the sign";
    EXPECT_DOUBLE_EQ(pred.lambda_i(0), 0.0);
}
} // namespace
} // namespace hven::solvers
