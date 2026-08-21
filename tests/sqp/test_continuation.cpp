// tests/sqp/test_continuation.cpp — Phase-4 Task 10: the CONTINUATION DRIVER
// (continuation.h), the sweep loop that composes everything the phase built --
// warm ingest (Task 3), the hot handle (Task 4), the Kungurtsev-Diehl
// full-step-first rule (Task 5), budgeted mode (Task 6) and the tangential
// predictor (Task 9) -- into the workflow the whole subsystem exists for:
// follow a solution path x*(p) from p0 to p1, paying one cold solve and then
// only warm ones.
//
// WHAT IS ASSERTED, AND WHY EACH FAMILY WAS PICKED FOR IT
// (tests/sqp/support/parametric_families.h has the analytic paths):
//
//   (a) SweepF1CrossesActivationsWarm. F1, p: 0 -> 1. The sweep must reach p1,
//       every step past the first must have RESOLVED to kWarm or above (a
//       sweep that silently re-solves cold is the failure this whole phase
//       exists to prevent, and it is invisible in the answers), the solved
//       point must sit on x_star(p) at every step, and the observed
//       (geometric) active set must equal the ANALYTIC one at every step --
//       including across BOTH of F1's activation thresholds, p = 0.2 and
//       p = 0.8, which the sweep's own dp schedule brackets. F1 is the right
//       family for this precisely because its middle branch is WEAKLY ACTIVE
//       (the row is geometrically active with multiplier identically zero --
//       the families header's DEGENERACY note), so the sweep crosses
//       activation thresholds where strict complementarity fails. Activity is
//       therefore compared GEOMETRICALLY, the only notion that is determinate
//       at a zero multiplier, and only for the BOX (whose multipliers cross
//       zero transversally); the row's working-set membership is deliberately
//       not asserted.
//
//   (b) AdaptiveDpGrowsAndShrinks. F2, p: 0 -> 1, across the activation
//       threshold p* = 0.78615137775742329. F2 is the only family whose path
//       is genuinely CURVED in p, so it is the one on which the step
//       controller has anything to control: dp must GROW on the smooth branch
//       (a step strictly longer than dp_init is asserted) and the run must
//       still cross p*. The dp sequence itself is pinned, not just its length.
//
//   (c) PredictorOffCostsMore. The same F2 sweep with use_predictor = false:
//       the predictor is an ACCELERATOR, so turning it off must cost strictly
//       more majors. Both totals are pinned, not just their order, so a
//       regression that leaves the predicted arm merely "still cheaper" by
//       accident is visible.
//
//   (d) TwoParameterSweepFollowsTheDirection. F4, parameter_dim() == 2, along
//       a deliberately non-axis-aligned segment. This is the only test in the
//       file where "dp" is a genuine VECTOR: it pins that every proposal lands
//       on the segment p0 + s*(p1-p0)/||p1-p0|| and that the predictor is
//       handed the parameter-space step vector p_next - p_cur rather than a
//       scalar times some axis.
//
//   (e) DegradedPredictionsAreCountedNotHidden. A model that rejects the
//       predictor's PROBE point (Task 9's ProbeRejectingF1 pattern, narrowed
//       to a mid-sweep window) makes predict() return kDegraded -- the
//       identity prediction -- for the steps inside that window. Three things
//       must then hold at once: the sweep still completes (the fallback to the
//       raw warm start engages), those steps' records say kDegraded and
//       predictor_used == false rather than claiming a prediction was taken,
//       and the result's aggregate degradation count is nonzero. predictor.h's
//       outcome out-parameter exists for exactly this reading; a driver that
//       passed nullptr could not tell this sweep from a healthy one, and would
//       report only that it was slower.
//
//   (f) ShrinkRetriesFromTheLastGoodWarm. The failure half of the step
//       controller, which no successful sweep exercises: a driver whose
//       max_iter is too small for a long step makes that step fail, and the
//       run must SHRINK dp, retry from the last GOOD warm (not from the
//       failed point) and still reach p1.
//
//   (i) BudgetExhaustionContinuesAtTheSameParameter. continuation.h's one
//       open design decision made testable: with SqpOptions::budget_mode on,
//       an exhausted step is neither accepted nor rejected -- the sweep
//       re-solves AT THE SAME p from the hand-off budgeted mode returns,
//       rather than shrinking dp. The control arm is the identical driver
//       budget with budget_mode off, where the same exhaustions arrive as
//       kMaxIter and the retry moves the proposal back instead.
//
//   (j)/(k) FIX ROUND 1, the two Medium findings, both pinned on SolvedStartF2
//       -- a fixture with max_iter = 0 whose cold solve converges at zero
//       majors and whose every warm step fails at zero majors, so the
//       trajectory is the step controller's ARITHMETIC with the solver held
//       constant. (j) NearEndpointFailureDoesNotRepeatTheSameProposal: the
//       shrink now applies to the clamped step that actually failed, so a
//       proposal truncated to a small remainder is attempted ONCE rather than
//       once per halving of dp (six times, in this fixture's own numbers).
//       (k) BudgetContinuationCapDemotesToAShrink: the budget cap's chain
//       length and its demotion into an ordinary shrink, which the review
//       found dead in the suite.
//
//   ZeroLengthSweepIsJustTheColdSolve / ColdStepFollowsTheSameBudgetRule --
//       fix round 1's two Minors: the p0 == p1 early return, and the cold step
//       participating in the budget rule exactly as every later p does (with a
//       budget_mode-off control, since the difference is otherwise invisible).
//
//   (g)/(h) The two T6 contracts stated in the brief: a p0/p1 dimension
//       mismatch THROWS std::invalid_argument, and a run that cannot get
//       through RETURNS with reached_p1 == false rather than throwing.
//
// ACCELERATE STANDING RULE. Every trajectory pin in this file (step counts,
// major totals, dp sequences) was MEASURED ON MKL PARDISO, which is the only
// backend available in this session. The QP engine's factorization backend can
// change which trial steps are accepted, hence how many majors a step costs,
// hence -- through target_majors -- the dp schedule itself, so these are
// backend-sensitive numbers in the same way tests/test_sqp_driver.cpp's
// suspect_escalations pins are. A first Accelerate run that lands on different
// counts is a RE-MEASUREMENT, not automatically a defect; what is NOT
// backend-sensitive, and must hold on any backend, is everything asserted
// analytically here: reached_p1, the levels, x on x_star(p), the active set
// against active_set(p), and predictor-on costing strictly less than
// predictor-off.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Dense>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/warmstart/continuation.h>
#include <hven/detail/warmstart/predictor.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>

#include "support/parametric_families.h"
#include "support/scale_problems.h"

namespace hven::solvers {
namespace {

using test_support::F1BoxQp;
using test_support::F2CircleNlp;
using test_support::F4MovingConstraints;
using test_support::F7CollocationChain;

// The sweeps are measured against analytic paths, so every solve along them
// must be converged far tighter than the agreement being asserted.
SqpOptions sweep_options() {
    SqpOptions opts;
    opts.kkt_tol = 1e-10;
    opts.feas_tol = 1e-10;
    opts.max_iter = 60;
    return opts;
}

Vec p_vec(double p) { return Vec::Constant(1, p); }

// WarmStart::bound_active's encoding read off a point GEOMETRICALLY -- the
// same helper tests/test_predictor.cpp uses, and the only notion of activity
// that is determinate where a multiplier vanishes (F1's DEGENERACY note).
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

// A one-line dump of a whole sweep, attached to every pinned assertion so a
// re-measurement (a backend change, a tuning change) reads the new trajectory
// off the failure message instead of needing a debugger.
std::string trajectory(const ContinuationResult &res) {
    std::string s;
    for (const ContinuationStep &st : res.steps) {
        std::string p_text;
        for (Index i = 0; i < st.p.size(); ++i) {
            p_text += fmt::format("{:.17g} ", st.p(i));
        }
        s += fmt::format("  p=[{}] dp={:.17g} status={} level={} majors={} pred={}{}\n", p_text,
                         st.dp, to_string(st.status), to_string(st.level), st.counters.major_iters,
                         st.predictor_used ? "yes" : "no",
                         st.predictor_outcome
                             ? fmt::format(" outcome={}", to_string(*st.predictor_outcome))
                             : std::string());
    }
    return s;
}

// F4, recording every parameter value it is ever posed at. The predictor's
// probe (p + h*dp/||dp||, h ~ 1.5e-8) shows up in that log as a MICROSCOPIC
// move, and its direction is dp/||dp|| exactly -- so the log is a direct
// read-out of the vector the continuation driver handed to predict(), which is
// otherwise invisible from the outside. This is the only way to pin that the
// driver passes the parameter-space STEP VECTOR rather than a scalar times
// some axis: on F4 a wrongly-directed prediction still converges in one major,
// so no cost-based assertion can see it.
class DirectionRecordingF4 : public F4MovingConstraints {
  public:
    DirectionRecordingF4(double a, double b) : F4MovingConstraints(a, b) {
        seen_.push_back(parameters());
    }

    void set_parameters(const Vec &p) override {
        F4MovingConstraints::set_parameters(p);
        seen_.push_back(p);
    }

    const std::vector<Vec> &seen() const { return seen_; }

  private:
    std::vector<Vec> seen_;
};

// F2 started AT its own analytic solution, which turns the step controller
// into something that can be pinned by arithmetic instead of by solver
// behaviour. With SqpOptions::max_iter = 0 this model produces exactly two
// outcomes, both at ZERO majors and neither depending on the QP engine:
//
//   - the COLD solve at p0 converges IMMEDIATELY (start_point() IS x*(p0), so
//     the convergence test fires before any subproblem is built -- sqp_types.h:
//     "a solve that converges immediately reports major_iters == 0");
//   - every WARM step FAILS, because F2's path is curved and a first-order
//     prediction across any usable dp lands O(||dp||^2) away, far outside the
//     1e-10 tolerance these tests run at, with no budget to close the gap.
//
// So the whole trajectory is the controller's own shrink/clamp/cap arithmetic
// with the solver held constant, which is what the two tests below need: they
// are pinning step-length bookkeeping, and a fixture whose failures depended on
// how many majors a real solve happened to spend would pin something else.
class SolvedStartF2 : public F2CircleNlp {
  public:
    explicit SolvedStartF2(double p0) : F2CircleNlp(p0) {}

    Vec start_point() const override { return F2CircleNlp::x_star(p()); }
};

// A model whose PROBE point is outside its domain over a window of the sweep:
// set_parameters rejects a move that is nonzero but smaller than kProbeBand
// while the model currently sits inside [lo, hi]. The predictor's forward
// difference steps h = sqrt(eps)*max(1, ||p||) ~ 1.5e-8, five orders below any
// step this file's sweeps take, so this rejects EXACTLY the probe and nothing
// the sweep itself does -- which is what makes the degradation mid-sweep
// rather than fatal.
//
// This is Task 9's ProbeRejectingF1 pattern (tests/test_predictor.cpp), which
// rejected every parameter but its entry value; that shape cannot be swept at
// all, so the rejection is narrowed to the probe here. It is still one of the
// three cases predictor.h's VALIDATE-THEN-CATCH-EVERYTHING note names as
// inside the degradation net, and still the realistic one (a parameter domain
// boundary).
class ProbeRejectingWindowF1 : public F1BoxQp {
  public:
    static constexpr double kProbeBand = 1e-6;

    ProbeRejectingWindowF1(double p0, double lo, double hi) : F1BoxQp(p0), lo_(lo), hi_(hi) {}

    void set_parameters(const Vec &p) override {
        if (p.size() == 1) {
            const double cur = this->p();
            const double move = std::abs(p(0) - cur);
            if (move > 0.0 && move < kProbeBand && cur >= lo_ && cur <= hi_) {
                throw std::runtime_error(
                    fmt::format("ProbeRejectingWindowF1: probe at {} is outside the domain", p(0)));
            }
        }
        F1BoxQp::set_parameters(p);
    }

  private:
    double lo_, hi_;
};

// ---------------------------------------------------------------------
// (a) F1, p: 0 -> 1 -- warm all the way, across both activation thresholds.
// ---------------------------------------------------------------------
TEST(Continuation, SweepF1CrossesActivationsWarm) {
    // PINNED (measured, MKL Pardiso -- see this file's ACCELERATE note).
    constexpr std::size_t kSteps = 5;
    // ONE major per step, cold step included: F1 is a QP, so a warm start on
    // the right branch is already its solution and the step that crosses a
    // threshold costs exactly the one major that re-picks the active set.
    constexpr Index kTotalMajors = 5;

    F1BoxQp model(0.0);
    SqpDriver driver(sweep_options());
    ContinuationOptions copts;

    const ContinuationResult res = run_continuation(model, p_vec(0.0), p_vec(1.0), driver, copts);

    ASSERT_TRUE(res.reached_p1) << "the sweep did not reach p1:\n" << trajectory(res);
    ASSERT_EQ(res.steps.size(), kSteps) << "step trajectory changed:\n" << trajectory(res);
    EXPECT_EQ(res.total_majors, kTotalMajors) << "major total changed:\n" << trajectory(res);

    // The first step is the ONE cold solve the sweep pays; everything after it
    // must have resolved warm, which is the whole claim of the subsystem.
    EXPECT_EQ(res.steps.front().level, StartLevel::kCold);
    EXPECT_EQ(res.steps.front().p(0), 0.0);
    EXPECT_DOUBLE_EQ(res.steps.back().p(0), 1.0) << "the last step must land exactly on p1";

    for (std::size_t i = 0; i < res.steps.size(); ++i) {
        const ContinuationStep &st = res.steps[i];
        SCOPED_TRACE(fmt::format("step {} at p = {}", i, st.p(0)));
        EXPECT_EQ(st.status, SqpStatus::kOptimal);
        if (i > 0) {
            EXPECT_GE(st.level, StartLevel::kWarm) << "a step past the first re-solved COLD:\n"
                                                   << trajectory(res);
        }
        // On the analytic path, and with the analytic active set.
        const Vec want_x = F1BoxQp::x_star(st.p(0));
        EXPECT_LT((st.x - want_x).norm(), 1e-8) << "off x_star(p)";
        model.set_parameters(st.p);
        const auto got = geometric_bound_active(model, st.x, 1e-8);
        const auto want = F1BoxQp::active_set(st.p(0)).bound_active;
        EXPECT_EQ(got, want) << "active set " << to_text(got) << " != analytic " << to_text(want);
    }

    // BOTH thresholds are crossed, and the crossing is where the box activity
    // changes. kPLow: x2 leaves its upper bound. kPHigh: x1 reaches its.
    bool crossed_low = false, crossed_high = false;
    for (std::size_t i = 1; i < res.steps.size(); ++i) {
        const double a = res.steps[i - 1].p(0);
        const double b = res.steps[i].p(0);
        const auto sa = F1BoxQp::active_set(a).bound_active;
        const auto sb = F1BoxQp::active_set(b).bound_active;
        if (a < F1BoxQp::kPLow && b >= F1BoxQp::kPLow) {
            crossed_low = true;
            EXPECT_EQ(sa[1], +1) << "x2 must be at its upper bound below kPLow";
            EXPECT_EQ(sb[1], 0) << "x2 must be free above kPLow";
        }
        if (a < F1BoxQp::kPHigh && b >= F1BoxQp::kPHigh) {
            crossed_high = true;
            EXPECT_EQ(sa[0], 0) << "x1 must be free below kPHigh";
            EXPECT_EQ(sb[0], +1) << "x1 must be at its upper bound above kPHigh";
        }
    }
    EXPECT_TRUE(crossed_low) << "no step bracketed kPLow = 0.2:\n" << trajectory(res);
    EXPECT_TRUE(crossed_high) << "no step bracketed kPHigh = 0.8:\n" << trajectory(res);
}

// ---------------------------------------------------------------------
// (b) F2, p: 0 -> 1 -- the step controller on a curved path.
// ---------------------------------------------------------------------
TEST(Continuation, AdaptiveDpGrowsAndShrinks) {
    // PINNED (measured, MKL Pardiso). The dp sequence IS the step controller's
    // output, so it is pinned entry by entry rather than only by length.
    // 0.1 -> 0.2 -> 0.4 is the growth rule (every step costs 1 major, well
    // inside target_majors = 5); the final 0.3 is the CLAMP onto p1, not a
    // shrink -- this sweep never needs one, and the shrink half of the
    // controller is exercised by ShrinkRetriesFromTheLastGoodWarm below. The
    // brief's name for this test covers both halves; only growth is reachable
    // on a path this well behaved.
    const std::vector<double> kDp = {0.0, 0.1, 0.2, 0.4, 0.3};
    // 1 major per step except the last, which crosses p* (the constraint
    // activates) and costs 3.
    constexpr Index kTotalMajors = 7;

    F2CircleNlp model(0.0);
    SqpDriver driver(sweep_options());
    ContinuationOptions copts;

    const ContinuationResult res = run_continuation(model, p_vec(0.0), p_vec(1.0), driver, copts);

    ASSERT_TRUE(res.reached_p1) << "the sweep did not reach p1:\n" << trajectory(res);
    ASSERT_EQ(res.steps.size(), kDp.size()) << "dp sequence length changed:\n" << trajectory(res);
    EXPECT_EQ(res.total_majors, kTotalMajors) << "major total changed:\n" << trajectory(res);

    bool grew = false;
    for (std::size_t i = 0; i < res.steps.size(); ++i) {
        SCOPED_TRACE(fmt::format("step {} at p = {}", i, res.steps[i].p(0)));
        EXPECT_NEAR(res.steps[i].dp, kDp[i], 1e-12) << "dp sequence changed:\n" << trajectory(res);
        if (res.steps[i].dp > copts.dp_init) {
            grew = true;
        }
        EXPECT_EQ(res.steps[i].status, SqpStatus::kOptimal);
        EXPECT_LT((res.steps[i].x - F2CircleNlp::x_star(res.steps[i].p(0))).norm(), 1e-8);
    }
    EXPECT_TRUE(grew) << "dp never grew past dp_init on a path this smooth:\n" << trajectory(res);

    // The run crosses the activation threshold: some consecutive pair brackets
    // p*, and the constraint is inactive on the low side and active on the high
    // side -- the active-set change the sweep has to carry a warm start across.
    const double p_star = F2CircleNlp::p_activation();
    bool crossed = false;
    for (std::size_t i = 1; i < res.steps.size(); ++i) {
        const double a = res.steps[i - 1].p(0);
        const double b = res.steps[i].p(0);
        if (a < p_star && b >= p_star) {
            crossed = true;
            EXPECT_FALSE(F2CircleNlp::constraint_active(a));
            EXPECT_TRUE(F2CircleNlp::constraint_active(b));
        }
    }
    EXPECT_TRUE(crossed) << "no step bracketed p* = " << p_star << ":\n" << trajectory(res);
}

// ---------------------------------------------------------------------
// (c) The predictor's own A/B: the same sweep with it switched off.
// ---------------------------------------------------------------------
TEST(Continuation, PredictorOffCostsMore) {
    // PINNED (measured, MKL Pardiso): both arms, so a regression that merely
    // preserves the inequality is still visible.
    // Both arms take the SAME five parameter values (F2's steps all cost 1
    // major on the smooth branch either way, so the dp schedule is identical);
    // the whole difference is the step across p*, which costs 3 majors from a
    // predicted seed and 5 from the raw one. The predictor's value here is
    // therefore exactly what it claims to be -- a better starting point for
    // the one step where the active set changes.
    constexpr Index kMajorsWithPredictor = 7;
    constexpr Index kMajorsWithout = 9;

    F2CircleNlp model_on(0.0);
    SqpDriver driver_on(sweep_options());
    ContinuationOptions on;
    const ContinuationResult res_on =
        run_continuation(model_on, p_vec(0.0), p_vec(1.0), driver_on, on);

    F2CircleNlp model_off(0.0);
    SqpDriver driver_off(sweep_options());
    ContinuationOptions off;
    off.use_predictor = false;
    const ContinuationResult res_off =
        run_continuation(model_off, p_vec(0.0), p_vec(1.0), driver_off, off);

    ASSERT_TRUE(res_on.reached_p1) << trajectory(res_on);
    ASSERT_TRUE(res_off.reached_p1) << trajectory(res_off);

    EXPECT_EQ(res_on.total_majors, kMajorsWithPredictor) << "predictor-ON trajectory changed:\n"
                                                         << trajectory(res_on);
    EXPECT_EQ(res_off.total_majors, kMajorsWithout) << "predictor-OFF trajectory changed:\n"
                                                    << trajectory(res_off);
    EXPECT_GT(res_off.total_majors, res_on.total_majors)
        << "the predictor is an ACCELERATOR: switching it off must cost more majors";

    // The OFF arm must not be cheaper merely because it took a different (say,
    // shorter) path: it calls predict() zero times and reports no outcome.
    EXPECT_EQ(res_off.predictor_calls, 0);
    EXPECT_EQ(res_off.predictor_degradations, 0);
    for (const ContinuationStep &st : res_off.steps) {
        EXPECT_FALSE(st.predictor_used);
        EXPECT_FALSE(st.predictor_outcome.has_value());
    }
    EXPECT_GT(res_on.predictor_calls, 0);
    EXPECT_EQ(res_on.predictor_degradations, 0) << "a healthy sweep must not degrade:\n"
                                                << trajectory(res_on);
}

// ---------------------------------------------------------------------
// (d) F4, parameter_dim() == 2 -- the direction is a genuine vector.
// ---------------------------------------------------------------------
TEST(Continuation, TwoParameterSweepFollowsTheDirection) {
    // PINNED (measured, MKL Pardiso).
    constexpr std::size_t kSteps = 4;

    const Vec p0 = (Vec(2) << 0.05, 0.05).finished();
    const Vec p1 = (Vec(2) << 0.50, 0.30).finished();
    // Deliberately NOT axis-aligned and NOT the diagonal, so a driver that
    // stepped one coordinate at a time, or that used ||dp|| with some fixed
    // direction, would leave the segment immediately.
    const Vec dir = (p1 - p0) / (p1 - p0).norm();

    DirectionRecordingF4 model(p0(0), p0(1));
    SqpDriver driver(sweep_options());
    ContinuationOptions copts;

    const ContinuationResult res = run_continuation(model, p0, p1, driver, copts);

    ASSERT_TRUE(res.reached_p1) << "the sweep did not reach p1:\n" << trajectory(res);
    ASSERT_EQ(res.steps.size(), kSteps) << "step trajectory changed:\n" << trajectory(res);
    EXPECT_TRUE(res.steps.back().p.isApprox(p1, 0.0)) << "the last step must land exactly on p1";

    double s = 0.0;
    for (std::size_t i = 0; i < res.steps.size(); ++i) {
        const ContinuationStep &st = res.steps[i];
        SCOPED_TRACE(fmt::format("step {} at p = ({}, {})", i, st.p(0), st.p(1)));
        EXPECT_EQ(st.status, SqpStatus::kOptimal);
        if (i > 0) {
            EXPECT_GE(st.level, StartLevel::kWarm);
        }
        // ON THE SEGMENT: p = p0 + s*dir with s the accumulated arc length, so
        // the proposal really is p + dp*direction in R^2.
        s += st.dp;
        const Vec want_p = p0 + s * dir;
        EXPECT_LT((st.p - want_p).norm(), 1e-12) << "step left the p0 -> p1 segment";
        EXPECT_LT((st.x - F4MovingConstraints::x_star(st.p)).norm(), 1e-8) << "off x_star(p)";
    }
    EXPECT_NEAR(s, (p1 - p0).norm(), 1e-12) << "the dp sequence must sum to the segment length";
    EXPECT_EQ(res.predictor_degradations, 0) << trajectory(res);
    EXPECT_EQ(res.total_majors, 4) << "step cost changed:\n" << trajectory(res); // 1 per step

    // THE VECTOR HANDED TO predict(), read off the model's own parameter log:
    // every MICROSCOPIC re-posing is the predictor's forward-difference probe
    // (or the restore that follows it), and its direction is dp/||dp||, which
    // must be the segment's unit direction and nothing else. Collinearity
    // rather than equality, because the restore step runs the same move
    // backwards.
    Index probes = 0;
    const std::vector<Vec> &seen = model.seen();
    for (std::size_t i = 1; i < seen.size(); ++i) {
        const Vec move = seen[i] - seen[i - 1];
        const double len = move.norm();
        if (len > 0.0 && len < 1e-6) {
            ++probes;
            EXPECT_NEAR(std::abs(move.dot(dir) / len), 1.0, 1e-9)
                << "the predictor's probe at " << seen[i - 1].transpose()
                << " moved off the p0 -> p1 direction: the driver handed predict() something "
                   "other than the parameter-space step vector";
        }
    }
    EXPECT_EQ(probes, res.predictor_calls * 2)
        << "expected one probe and one restore per predict() call; saw " << probes << " for "
        << res.predictor_calls << " calls";
}

// ---------------------------------------------------------------------
// (e) A degraded prediction must be COUNTED, not hidden behind a slow sweep.
// ---------------------------------------------------------------------
TEST(Continuation, DegradedPredictionsAreCountedNotHidden) {
    // The window is chosen to sit strictly inside the sweep, so some steps
    // predict normally and some degrade -- a fixture where EVERY step degraded
    // could not tell "counted" from "always reported".
    ProbeRejectingWindowF1 model(0.0, /*lo=*/0.05, /*hi=*/0.75);
    SqpDriver driver(sweep_options());
    ContinuationOptions copts;

    const ContinuationResult res = run_continuation(model, p_vec(0.0), p_vec(1.0), driver, copts);

    // The sweep COMPLETES: a declined prediction costs speed, never the run.
    ASSERT_TRUE(res.reached_p1) << "a degraded prediction killed the sweep:\n" << trajectory(res);

    // PINNED (measured): 4 predict() calls -- one from each of p = 0, 0.1, 0.3,
    // 0.7 -- of which the first is OUTSIDE the window and succeeds and the
    // other three are inside it and degrade. Pinning the split, not just
    // "some", is what keeps the fixture partial: a change that pushed the
    // window over the whole sweep would still satisfy "> 0".
    EXPECT_EQ(res.predictor_calls, 4) << trajectory(res);
    EXPECT_EQ(res.predictor_degradations, 3)
        << "the probe-rejecting window produced no degradation -- the fixture, not the "
           "driver, is what this would indict:\n"
        << trajectory(res);
    EXPECT_LT(res.predictor_degradations, res.predictor_calls)
        << "EVERY prediction degraded; the window is supposed to be partial:\n"
        << trajectory(res);

    Index counted_degraded = 0, counted_predicted = 0;
    for (std::size_t i = 0; i < res.steps.size(); ++i) {
        const ContinuationStep &st = res.steps[i];
        SCOPED_TRACE(fmt::format("step {} at p = {}", i, st.p(0)));
        EXPECT_EQ(st.status, SqpStatus::kOptimal);
        if (i == 0) {
            EXPECT_FALSE(st.predictor_outcome.has_value()) << "the cold step predicts nothing";
            continue;
        }
        ASSERT_TRUE(st.predictor_outcome.has_value());
        if (*st.predictor_outcome == PredictorOutcome::kDegraded) {
            ++counted_degraded;
            // THE POINT OF THE TEST: the record must not claim a prediction was
            // taken when the predictor declined to make one.
            EXPECT_FALSE(st.predictor_used) << "a kDegraded step reported predictor_used == true";
            // THE FALLBACK ENGAGED: the raw warm start was still ingested, so
            // the step is warm even though it is unpredicted.
            EXPECT_GE(st.level, StartLevel::kWarm)
                << "the degraded step fell all the way back to a COLD solve";
        } else {
            EXPECT_EQ(*st.predictor_outcome, PredictorOutcome::kPredicted);
            EXPECT_TRUE(st.predictor_used);
            ++counted_predicted;
        }
        EXPECT_LT((st.x - F1BoxQp::x_star(st.p(0))).norm(), 1e-8) << "off x_star(p)";
    }
    EXPECT_EQ(counted_degraded, res.predictor_degradations)
        << "the aggregate count disagrees with the per-step records:\n"
        << trajectory(res);
    EXPECT_EQ(counted_degraded + counted_predicted, res.predictor_calls);
}

// ---------------------------------------------------------------------
// (f) The failure half of the step controller.
// ---------------------------------------------------------------------
TEST(Continuation, ShrinkRetriesFromTheLastGoodWarm) {
    // A budget too small for a long step across F2's curved branch: the first
    // proposal fails, dp shrinks, and the retry -- from the LAST GOOD warm, at
    // the last good p, not from the failed point -- gets through.
    SqpOptions tight = sweep_options();
    tight.max_iter = 2;

    F2CircleNlp model(0.0);
    SqpDriver driver(tight);
    ContinuationOptions copts;
    copts.dp_init = 0.5;
    copts.dp_max = 0.5;
    copts.dp_min = 1e-3;

    const ContinuationResult res = run_continuation(model, p_vec(0.0), p_vec(1.0), driver, copts);

    ASSERT_TRUE(res.reached_p1) << "the shrink/retry path failed to recover:\n" << trajectory(res);
    // PINNED (measured, MKL Pardiso). A long trajectory, and the most
    // backend-sensitive pin in the file: every entry is one max_iter = 2 solve
    // either converging or not. Re-measure off the failure message.
    // FIX ROUND 1 (Q2) MOVED THESE: 15 steps / 26 majors before the shrink was
    // retargeted at the clamped step. The step that vanished is the second of
    // two byte-identical `p = 1, dp = 0.25, MaxIter` attempts -- the repeat
    // this file's NearEndpointFailureDoesNotRepeatTheSameProposal now pins
    // against directly.
    //
    // PHASE-6 TASK 1 MOVED THEM AGAIN, 14 steps / 24 majors -> 13 / 22, and
    // this is a MARKED CORRECTION rather than a regression: the sweep now
    // reaches p1 in one attempt and two majors FEWER. The lever responsible is
    // ContinuationOptions::suspend_growth_after_failure (default on), which
    // stops the step after a failure from doubling dp straight back toward the
    // length that just failed. It is NOT the probe budget -- asserted directly
    // below, and structurally impossible here anyway: F2 is a two-variable
    // problem whose whole sweep costs a handful of minors, far under
    // continuation_detail::kProbeBudgetFloor, so no budget can ever be
    // exhausted on it. Everything this test actually contracts for (the
    // structural assertions below, and reaching p1 at all) is unchanged.
    ASSERT_EQ(res.steps.size(), 13u) << "shrink trajectory changed:\n" << trajectory(res);
    EXPECT_EQ(res.total_majors, 22) << "shrink trajectory changed:\n" << trajectory(res);
    EXPECT_EQ(res.proposals_abandoned, 0) << "the probe budget is not what moved this fixture:\n"
                                          << trajectory(res);

    // THE CONTRACT, asserted structurally rather than by pinning dp values
    // (which the endpoint clamp makes non-monotone -- see ContinuationStep::dp):
    //   1. every proposal is measured from the LAST CONVERGED point, so
    //      p == p_last_good + dp. That IS "retry from the last good warm
    //      start", seen from outside: a retry seeded from the failed point
    //      would be proposed from there instead.
    //   2. a retry never goes past the point that just failed, and never
    //      behind the last good one.
    //   3. some retry is strictly shorter than the attempt it follows.
    double p_good = 0.0;
    bool saw_failure = false, saw_strict_shrink = false;
    for (std::size_t i = 1; i < res.steps.size(); ++i) {
        const ContinuationStep &st = res.steps[i];
        SCOPED_TRACE(fmt::format("step {} at p = {}", i, st.p(0)));
        EXPECT_NEAR(st.p(0), p_good + st.dp, 1e-12)
            << "a proposal was not measured from the last converged point:\n"
            << trajectory(res);
        if (res.steps[i - 1].status != SqpStatus::kOptimal) {
            saw_failure = true;
            EXPECT_LE(st.dp, res.steps[i - 1].dp) << "a retry was LONGER than the step it follows";
            EXPECT_GT(st.p(0), p_good) << "a retry fell behind the last converged point";
            EXPECT_LE(st.p(0), res.steps[i - 1].p(0)) << "a retry overshot the point that failed";
            if (st.dp < res.steps[i - 1].dp) {
                saw_strict_shrink = true;
            }
        }
        if (st.status == SqpStatus::kOptimal) {
            p_good = st.p(0);
        }
    }
    ASSERT_TRUE(saw_failure) << "no step failed, so the shrink path was never taken:\n"
                             << trajectory(res);
    EXPECT_TRUE(saw_strict_shrink) << "dp never strictly shrank after a failure:\n"
                                   << trajectory(res);

    // Every SUCCESSFUL step still sits on the analytic path -- a retry from a
    // stale or half-failed warm start would show up here first.
    for (const ContinuationStep &st : res.steps) {
        if (st.status == SqpStatus::kOptimal) {
            EXPECT_LT((st.x - F2CircleNlp::x_star(st.p(0))).norm(), 1e-7)
                << "a recovered step is off x_star(p) at p = " << st.p(0);
        }
    }
    EXPECT_TRUE(res.final_warm.valid);
}

// ---------------------------------------------------------------------
// (i) BUDGET EXHAUSTION IS A CONTINUE, NOT A SHRINK -- continuation.h's own
//     design decision, and the one status the loop treats as neither success
//     nor failure. Same starved driver as (f), with budget_mode ON, so the
//     identical exhaustions arrive as kBudgetExhausted instead of kMaxIter.
// ---------------------------------------------------------------------
TEST(Continuation, BudgetExhaustionContinuesAtTheSameParameter) {
    SqpOptions budgeted = sweep_options();
    budgeted.max_iter = 2;
    budgeted.budget_mode = true;

    F2CircleNlp model(0.0);
    SqpDriver driver(budgeted);
    ContinuationOptions copts;
    copts.dp_init = 0.5;
    copts.dp_max = 0.5;
    copts.dp_min = 1e-3;

    const ContinuationResult res = run_continuation(model, p_vec(0.0), p_vec(1.0), driver, copts);
    ASSERT_TRUE(res.reached_p1) << "the budgeted sweep did not get through:\n" << trajectory(res);

    // At least one exhaustion, and every one of them is followed by an attempt
    // at the SAME parameter value with the SAME dp -- the proposal did not
    // move and dp was not shrunk -- seeded by the exhausted solve's own
    // hand-off, which is why no new prediction is made for it.
    Index exhausted = 0, continued = 0;
    for (std::size_t i = 0; i < res.steps.size(); ++i) {
        if (res.steps[i].status != SqpStatus::kBudgetExhausted) {
            continue;
        }
        ++exhausted;
        if (i + 1 >= res.steps.size()) {
            continue; // the sweep ended on it; nothing follows to check
        }
        const ContinuationStep &next = res.steps[i + 1];
        if (next.p(0) != res.steps[i].p(0)) {
            continue; // the cap demoted it to an ordinary failure
        }
        ++continued;
        EXPECT_EQ(next.dp, res.steps[i].dp) << "a budget continuation shrank dp:\n"
                                            << trajectory(res);
        EXPECT_FALSE(next.predictor_outcome.has_value())
            << "a budget continuation re-ran the predictor for a zero step:\n"
            << trajectory(res);
        EXPECT_FALSE(next.predictor_used);
        EXPECT_GE(next.level, StartLevel::kWarm)
            << "the exhausted solve's hand-off was not ingested:\n"
            << trajectory(res);
    }
    EXPECT_GT(exhausted, 0) << "budget_mode never fired; the fixture, not the driver, is what "
                               "this would indict:\n"
                            << trajectory(res);
    EXPECT_GT(continued, 0) << "no exhaustion was followed by a continuation at the same p:\n"
                            << trajectory(res);

    // The control that makes the decision visible AS a decision: with
    // budget_mode OFF the identical driver budget produces kMaxIter, an
    // ordinary failure, and the retry moves the proposal BACK -- so the two
    // arms take measurably different routes to p1.
    SqpOptions plain = budgeted;
    plain.budget_mode = false;
    F2CircleNlp model_plain(0.0);
    SqpDriver driver_plain(plain);
    const ContinuationResult plain_res =
        run_continuation(model_plain, p_vec(0.0), p_vec(1.0), driver_plain, copts);
    ASSERT_TRUE(plain_res.reached_p1) << trajectory(plain_res);
    for (const ContinuationStep &st : plain_res.steps) {
        EXPECT_NE(st.status, SqpStatus::kBudgetExhausted)
            << "kBudgetExhausted must be unreachable with budget_mode off";
    }
    EXPECT_NE(res.steps.size(), plain_res.steps.size())
        << "the continue rule and the shrink rule produced the same trajectory, so nothing "
           "here distinguishes them:\n"
        << trajectory(res) << "vs\n"
        << trajectory(plain_res);
}

// ---------------------------------------------------------------------
// (j) FIX ROUND 1, Q2: near p1 a failing proposal must not be REPEATED.
//
//     The proposal is clamped to the remaining distance, so shrinking the
//     CONTROLLER's dp need not shrink the STEP. The original rule did exactly
//     that and re-ran the identical failing solve once per halving until dp
//     decayed below the remainder. This fixture is the reviewer's worked case
//     -- dp_init = 0.5 against a remainder of 0.01 -- where that cost six
//     identical full solves; the rule now shrinks from the step that actually
//     failed, so it costs ONE.
// ---------------------------------------------------------------------
TEST(Continuation, NearEndpointFailureDoesNotRepeatTheSameProposal) {
    SqpOptions starved = sweep_options();
    starved.max_iter = 0; // every warm step fails at zero majors; see SolvedStartF2

    SolvedStartF2 model(0.0);
    SqpDriver driver(starved);
    ContinuationOptions copts;
    copts.dp_init = 0.5;
    copts.dp_max = 0.5;
    copts.dp_min = 1e-3;

    // total = 0.01, i.e. FIFTY TIMES shorter than dp_init: every proposal from
    // p0 is clamped until dp itself falls below the remainder.
    const ContinuationResult res = run_continuation(model, p_vec(0.0), p_vec(0.01), driver, copts);

    EXPECT_FALSE(res.reached_p1) << trajectory(res);

    // THE PINNED COST. Under the OLD rule the first proposal (p = 0.01, the
    // clamped remainder) was attempted SIX times identically -- dp went
    // 0.5 -> 0.25 -> 0.125 -> 0.0625 -> 0.03125 -> 0.015625 while the clamp
    // held the step at 0.01 -- before the seventh attempt was finally shorter.
    // Under the fixed rule it is attempted ONCE and every later proposal is
    // strictly shorter than the one before it.
    Index worst_repeat = 0, run_length = 0;
    for (std::size_t i = 0; i < res.steps.size(); ++i) {
        if (i > 0 && res.steps[i].p(0) == res.steps[i - 1].p(0)) {
            ++run_length;
        } else {
            run_length = 1;
        }
        worst_repeat = std::max(worst_repeat, run_length);
    }
    EXPECT_EQ(worst_repeat, 1) << "a failing proposal was repeated verbatim " << worst_repeat
                               << " times; the shrink is not biting on the clamped step:\n"
                               << trajectory(res);

    // The whole trajectory is arithmetic: 0.01 (clamped), then 0.005, 0.0025,
    // 0.00125, at which point dp = 6.25e-4 < dp_min = 1e-3 and the run fails.
    const std::vector<double> kP = {0.0, 0.01, 0.005, 0.0025, 0.00125};
    ASSERT_EQ(res.steps.size(), kP.size()) << "shrink arithmetic changed:\n" << trajectory(res);
    for (std::size_t i = 0; i < kP.size(); ++i) {
        EXPECT_NEAR(res.steps[i].p(0), kP[i], 1e-15) << "at step " << i << ":\n" << trajectory(res);
    }
    EXPECT_EQ(res.steps.front().status, SqpStatus::kOptimal) << "the cold solve must converge";
    for (std::size_t i = 1; i < res.steps.size(); ++i) {
        EXPECT_NE(res.steps[i].status, SqpStatus::kOptimal);
        // From i = 2 on, because step 1 is compared against the cold step,
        // whose dp is 0 by definition rather than by any shrink.
        if (i >= 2) {
            EXPECT_LT(res.steps[i].dp, res.steps[i - 1].dp)
                << "proposal " << i << " was not strictly shorter than the one that just failed:\n"
                << trajectory(res);
        }
    }
    EXPECT_EQ(res.total_majors, 0) << "the fixture is supposed to spend no majors at all";
    // RE-MEASURED, PHASE-5 TASK 0 (O-1's repair): these steps report
    // level == kWarm. They used to report kCold, and the note here used to
    // explain why that was a property of the degenerate fixture rather than a
    // warm-ingest regression: a zero-budget solve builds no subproblem, so its
    // WarmStart::structure_hash stayed at the "never computed" sentinel and the
    // next solve's ingest could not match it. sqp_driver.h's make_warm_start
    // now PROBES the model's structure on exactly that exit, so a solve that
    // spends no major still hands off a hash-valid object and the chain
    // survives. NOTHING ELSE IN THIS FIXTURE MOVED -- the same five proposals,
    // the same shrink arithmetic, the same statuses, still zero majors
    // (asserted above) -- because a starved solve that ingests a warm point it
    // is already standing on does exactly as much work as one that does not:
    // none.
    for (std::size_t i = 1; i < res.steps.size(); ++i) {
        EXPECT_EQ(res.steps[i].level, StartLevel::kWarm) << trajectory(res);
    }
}

// ---------------------------------------------------------------------
// (k) FIX ROUND 1, Q1: the budget-continuation CAP, and its demotion.
//
//     The cap is the loop's only guard against "continue at the same p"
//     becoming an infinite loop, and the review found its demotion branch dead
//     in the suite: raising kBudgetContinuationsMax to 1000 left every test
//     green. This fixture trips it deterministically -- with max_iter = 0 and
//     budget_mode on, every warm attempt exhausts its budget at zero majors and
//     the hand-off it returns is the point it started from, so continuing can
//     never converge and the cap is the only thing that ends the chain.
// ---------------------------------------------------------------------
TEST(Continuation, BudgetContinuationCapDemotesToAShrink) {
    SqpOptions starved = sweep_options();
    starved.max_iter = 0;
    starved.budget_mode = true;

    SolvedStartF2 model(0.0);
    SqpDriver driver(starved);
    ContinuationOptions copts;
    copts.dp_init = 0.5;
    copts.dp_max = 0.5;
    copts.dp_min = 0.2; // a high floor, so the run ends after two capped chains

    const ContinuationResult res = run_continuation(model, p_vec(0.0), p_vec(1.0), driver, copts);

    EXPECT_FALSE(res.reached_p1) << trajectory(res);
    // 1 cold + two chains of (1 attempt + 3 continuations) at p = 0.5 and 0.25.
    ASSERT_EQ(res.steps.size(), 9u) << "the cap's arithmetic changed:\n" << trajectory(res);

    // Every chain is EXACTLY kBudgetContinuationsMax + 1 attempts long: the
    // cap's whole job. A cap that never fired would run to dp_min at the FIRST
    // p and never produce a second chain; a cap of a different size would
    // change these run lengths.
    std::vector<std::pair<double, Index>> chains; // (p, consecutive attempts)
    for (std::size_t i = 1; i < res.steps.size(); ++i) {
        EXPECT_EQ(res.steps[i].status, SqpStatus::kBudgetExhausted)
            << "step " << i << " is not an exhaustion:\n"
            << trajectory(res);
        if (!chains.empty() && chains.back().first == res.steps[i].p(0)) {
            ++chains.back().second;
        } else {
            chains.emplace_back(res.steps[i].p(0), 1);
        }
    }
    ASSERT_EQ(chains.size(), 2u) << "expected two capped chains:\n" << trajectory(res);
    for (const auto &chain : chains) {
        EXPECT_EQ(chain.second, 4)
            << "chain at p = " << chain.first << " ran " << chain.second << " attempts, not 1 + "
            << "kBudgetContinuationsMax:\n"
            << trajectory(res);
    }

    // THE DEMOTION ITSELF: the second chain sits at a STRICTLY SHORTER step
    // from the same base, i.e. the capped chain was converted into an ordinary
    // failure and dp was shrunk -- not merely abandoned.
    EXPECT_DOUBLE_EQ(chains[0].first, 0.5);
    EXPECT_DOUBLE_EQ(chains[1].first, 0.25);
    EXPECT_LT(res.steps.back().dp, res.steps[1].dp) << trajectory(res);

    // A continuation re-solves rather than re-predicting, and ingests the
    // exhausted solve's hand-off -- the same claims (i) makes, re-checked here
    // where the chain is long enough for them to be load-bearing.
    for (std::size_t i = 2; i < res.steps.size(); ++i) {
        if (res.steps[i].p(0) != res.steps[i - 1].p(0)) {
            continue; // the first attempt of a chain: predicted, not continued
        }
        EXPECT_FALSE(res.steps[i].predictor_outcome.has_value()) << trajectory(res);
        EXPECT_EQ(res.steps[i].dp, res.steps[i - 1].dp) << trajectory(res);
    }
}

// ---------------------------------------------------------------------
// (g)/(h) The two T6 contracts.
// ---------------------------------------------------------------------
// FIX ROUND 1, M3: the p0 == p1 early return -- a semantic claim ("the cold
// solve IS the sweep") that no test executed.
TEST(Continuation, ZeroLengthSweepIsJustTheColdSolve) {
    F1BoxQp model(0.3);
    SqpDriver driver(sweep_options());

    const ContinuationResult res = run_continuation(model, p_vec(0.3), p_vec(0.3), driver);

    EXPECT_TRUE(res.reached_p1) << "p1 == p0 is reached by definition:\n" << trajectory(res);
    ASSERT_EQ(res.steps.size(), 1u) << trajectory(res);
    EXPECT_EQ(res.steps.front().status, SqpStatus::kOptimal);
    EXPECT_EQ(res.steps.front().level, StartLevel::kCold);
    EXPECT_EQ(res.steps.front().dp, 0.0);
    EXPECT_EQ(res.predictor_calls, 0) << "there is no step to predict across";
    EXPECT_LT((res.steps.front().x - F1BoxQp::x_star(0.3)).norm(), 1e-8);
    EXPECT_TRUE(res.final_warm.valid);
}

// M3 FINAL REVIEW, S-5. A SEGMENT WHOSE SQUARE UNDERFLOWS IS STILL A SEGMENT.
// p0 = 0, p1 = 1e-300: `segment.norm()` squares before it sums, so 1e-300
// becomes 0 and the old length test read that as "p1 == p0" -- returning
// reached_p1 == true after a single cold solve at p0, having never proposed p1
// at all. reached_p1 is the field a caller trusts to mean "the sweep arrived",
// so that was a wrong answer rather than a rounding nicety. stableNorm() scales
// the largest magnitude out before summing and returns 1e-300, and the p1 == p0
// claim is now the exact componentwise equality it always asserted.
TEST(Continuation, SubnormalSegmentIsParameterizedRatherThanCalledZeroLength) {
    constexpr double kTiny = 1e-300;
    ASSERT_EQ(kTiny * kTiny, 0.0)
        << "the fixture depends on the SQUARE underflowing, not the value";

    F1BoxQp model(0.0);
    SqpDriver driver(sweep_options());
    const ContinuationResult res = run_continuation(model, p_vec(0.0), p_vec(kTiny), driver);

    EXPECT_TRUE(res.reached_p1) << trajectory(res);
    // TWO steps, not one: the cold solve at p0 and a solve AT p1. One step
    // with reached_p1 set is precisely the dishonest report this item removes.
    ASSERT_EQ(res.steps.size(), 2u) << "p1 was never proposed:\n" << trajectory(res);
    EXPECT_EQ(res.steps.back().p(0), kTiny) << trajectory(res);
    EXPECT_EQ(res.steps.back().status, SqpStatus::kOptimal) << trajectory(res);
    EXPECT_EQ(res.steps.back().dp, kTiny) << trajectory(res);

    // AND THE EXACT-EQUALITY ARM STILL SHORT-CIRCUITS. ZeroLengthSweepIsJust
    // TheColdSolve above pins the p0 == p1 case at 0.3; repeated here at 0.0
    // so the two neighbouring behaviours are stated side by side, and so a
    // regression that made the equality test approximate again would swallow
    // the sweep above rather than fail only there.
    F1BoxQp same(0.0);
    SqpDriver same_driver(sweep_options());
    const ContinuationResult zero = run_continuation(same, p_vec(0.0), p_vec(0.0), same_driver);
    EXPECT_TRUE(zero.reached_p1) << trajectory(zero);
    EXPECT_EQ(zero.steps.size(), 1u) << trajectory(zero);
}

// M3 FINAL REVIEW, S-4. THE FAILED COLD SOLVE IS A FAILED ATTEMPT. The pair
// (proposals_abandoned, proposals_full_cost) is documented to sum to the number
// of failed attempts in `steps`; on a sweep whose p0 never converged -- the one
// case where every recorded step is a failure -- both used to come back 0.
TEST(Continuation, FailingInitialPointIsCountedAsAFailedAttempt) {
    // (a) THE DIRECT FAILURE. max_iter = 0 with budget_mode off reports
    // kMaxIter at p0, so the sweep records one step and returns.
    {
        SqpOptions starved = sweep_options();
        starved.max_iter = 0;
        F1BoxQp model(0.0);
        SqpDriver driver(starved);

        const ContinuationResult res = run_continuation(model, p_vec(0.0), p_vec(1.0), driver);
        ASSERT_EQ(res.steps.size(), 1u) << trajectory(res);
        ASSERT_NE(res.steps.front().status, SqpStatus::kOptimal) << trajectory(res);
        EXPECT_EQ(res.proposals_abandoned + res.proposals_full_cost, 1)
            << "the sum invariant must hold on a sweep whose only step failed:\n"
            << trajectory(res);
        // No probe budget is ever armed at p0 (the cold solve is unbudgeted),
        // so the classification lands on full_cost -- the same test the loop
        // applies to its own failures, reaching the same answer.
        EXPECT_EQ(res.proposals_abandoned, 0) << trajectory(res);
        EXPECT_EQ(res.proposals_full_cost, 1) << trajectory(res);
    }

    // (b) THE BUDGET CHAIN PAST ITS CAP -- ColdStepFollowsTheSameBudgetRule's
    // fixture, read through the retry-cost split. The chain itself is a
    // sequence of CONTINUATIONS, which continuation.h says are in neither
    // counter; only the attempt the cap demotes counts, ONCE.
    {
        SqpOptions starved = sweep_options();
        starved.max_iter = 0;
        starved.budget_mode = true;
        F2CircleNlp model(0.6);
        SqpDriver driver(starved);

        const ContinuationResult res = run_continuation(model, p_vec(0.6), p_vec(1.0), driver);
        ASSERT_EQ(res.steps.size(), 4u) << trajectory(res);
        EXPECT_EQ(res.proposals_abandoned + res.proposals_full_cost, 1)
            << "a demoted chain is ONE failed attempt, not four and not zero:\n"
            << trajectory(res);
    }

    // (c) THE CONTROL. A sweep whose p0 DOES converge is untouched -- this is
    // the counter-neutrality argument for the change, asserted rather than
    // asserted-about: every existing proposals_* pin in this file is on a
    // fixture of exactly this shape.
    {
        F1BoxQp model(0.3);
        SqpDriver driver(sweep_options());
        const ContinuationResult res = run_continuation(model, p_vec(0.3), p_vec(0.5), driver);
        ASSERT_TRUE(res.reached_p1) << trajectory(res);
        EXPECT_EQ(res.proposals_abandoned + res.proposals_full_cost, 0)
            << "a clean sweep records no failed attempt:\n"
            << trajectory(res);
    }
}

// FIX ROUND 1, M1: the COLD step follows the same budget rule as every other
// parameter value -- an exhaustion at p0 is a continue, not a fatal.
TEST(Continuation, ColdStepFollowsTheSameBudgetRule) {
    SqpOptions starved = sweep_options();
    starved.max_iter = 0;
    starved.budget_mode = true;

    // start_point() is NOT the solution here (F2's own (0.5, 0.5) at p = 0.6),
    // so the cold solve at p0 cannot converge within a zero budget and exhausts
    // -- repeatedly, since each continuation re-solves from a hand-off that has
    // not moved. The cap is what ends it.
    F2CircleNlp model(0.6);
    SqpDriver driver(starved);

    ContinuationResult res;
    ASSERT_NO_THROW(res = run_continuation(model, p_vec(0.6), p_vec(1.0), driver));

    EXPECT_FALSE(res.reached_p1) << trajectory(res);
    // 1 + kBudgetContinuationsMax attempts, ALL at p0: the sweep never proposed
    // a second parameter value. An implementation that treated the cold
    // exhaustion as fatal would record exactly one step here.
    ASSERT_EQ(res.steps.size(), 4u) << "the cold step is not following the budget rule:\n"
                                    << trajectory(res);
    for (const ContinuationStep &st : res.steps) {
        EXPECT_EQ(st.p(0), 0.6) << "a cold continuation moved p:\n" << trajectory(res);
        EXPECT_EQ(st.dp, 0.0);
        EXPECT_EQ(st.status, SqpStatus::kBudgetExhausted) << trajectory(res);
        EXPECT_FALSE(st.predictor_outcome.has_value()) << "nothing is predicted at p0";
    }
    EXPECT_EQ(res.predictor_calls, 0);
    EXPECT_TRUE(res.final_warm.valid) << "the last hand-off is still safe to feed forward";

    // The CONTROL: the same starved driver with budget_mode OFF reports
    // kMaxIter, which is an ordinary failure, and the sweep stops after one
    // step -- so the four-step trajectory above is the budget rule and not
    // merely "a failing cold solve retries".
    SqpOptions plain = starved;
    plain.budget_mode = false;
    F2CircleNlp model_plain(0.6);
    SqpDriver driver_plain(plain);
    const ContinuationResult plain_res =
        run_continuation(model_plain, p_vec(0.6), p_vec(1.0), driver_plain);
    EXPECT_FALSE(plain_res.reached_p1);
    EXPECT_EQ(plain_res.steps.size(), 1u) << trajectory(plain_res);
    EXPECT_EQ(plain_res.steps.front().status, SqpStatus::kMaxIter);
}

// =============================================================================
// PHASE-6 TASK 1 -- RETRY ECONOMICS (continuation.h's RETRY ECONOMICS note).
//
// THE FIXTURE, and why it is this one. F7CollocationChain
// (tests/sqp/support/scale_problems.h) at N = 100 nodes (n = 500 variables),
// swept p: 0.3 -> 0.9 with dp_init = 0.4. The FIRST proposal, 0.3 -> 0.7,
// lands in the WIDE-WINDOW regime -- the same regime in which every one of
// the five failed proposals of the nx = 10^5 sweep failed
// (docs/notes/2026-08-01-psiopt-first-comparison.md section 4.5(b)) -- and
// fails there as kNumericalError with its QP pinned at the minor cap. Its
// shape is the pathology in miniature: pre-task it costs 1002 of the sweep's
// 1053 minors, 95 %, against the 10^5 sweep's 84 %.
//
// THE BRIEF PROPOSED CROSSING p_saturation() INSTEAD, and this file does NOT,
// deliberately. At p >= R the family's node-0 path row is active while node 0
// is also fixed outright by the initial-condition rows, so LICQ fails and K0
// is exactly singular before regularization -- scale_problems.h's own THE ONE
// DEGENERACY note names that geometry as the one the Accelerate standing rule
// forbids a fixture to present. Sweeping to p = 1.05 was tried while this
// test was being built (it does reach p1, and the failing proposal is the
// same wide-window one, not the saturation crossing), and was then discarded
// because a fixture that no second backend may run is not a fixture this
// project keeps. Everything asserted below is inside the family's design
// range 0 < p < R.
//
// ACCELERATE STANDING RULE, as for every other pin in this file: the counts
// below were MEASURED ON MKL PARDISO. What is backend-INDEPENDENT, and is
// asserted as such, is the ORDERING -- an abandoned proposal costs strictly
// less than the same proposal paid in full, the two arms propose the same
// parameter values, and the converged steps are identical between them.
TEST(Continuation, ProbeBudgetBoundsAFailingProposal) {
    const auto make_options = [] {
        // The bench's own settings (bench/bench_scale.cpp bench_options), so
        // this fixture and the measured sweeps it stands in for are solved by
        // the same driver configuration.
        SqpOptions opts;
        opts.kkt_tol = 1e-8;
        opts.feas_tol = 1e-8;
        opts.max_iter = 100;
        opts.adaptive_mu = false;
        opts.start_level = StartLevel::kWarm;
        opts.warm_full_step = true;
        // MARKED CORRECTION, PHASE-6 TASK 4 (M6). This line is NEW and it
        // restores, EXPLICITLY, the QP minor cap that was the library default
        // when this fixture was built. It is not a workaround -- it is the
        // escape hatch M6 was required to keep (qp_types.h's QpOptions::max_iter
        // precedence rule), used for exactly the case it exists for.
        //
        // WHY IT IS NEEDED, and it is a RESULT rather than a maintenance
        // chore: under M6's size-derived default this fixture's failing
        // proposal (0.3 -> 0.7 at N = 100 nodes, base = 800, derived cap
        // 4000) simply CONVERGES -- 4 majors, kOptimal -- and the sweep
        // reaches p1 in three steps with nothing to abandon. The pathology
        // this fixture was built to miniaturize WAS the cap
        // (docs/notes/2026-08-03-crash-basis.md Sec. 6). Arm D below asserts
        // that new fact directly, at the shipped default; arms A-C keep
        // testing the PROBE BUDGET, which is what this test is about, on the
        // failure they were pinned against.
        opts.qp.max_iter = 500;
        return opts;
    };
    const auto make_continuation_options = [](int probe_budget) {
        ContinuationOptions copts;
        copts.use_predictor = false;
        copts.dp_init = 0.4;
        copts.dp_max = 0.4;
        copts.probe_budget = probe_budget;
        // Isolate the probe budget: the failure-history term is the OTHER
        // lever this task added and has its own test below.
        copts.suspend_growth_after_failure = false;
        return copts;
    };

    // ---- ARM A: the pre-task behaviour, probe_budget = 0 ------------------
    F7CollocationChain model_off(100, 3, 2, 0.3, 1.0);
    SqpDriver driver_off(make_options());
    const ContinuationResult off = run_continuation(model_off, p_vec(0.3), p_vec(0.9), driver_off,
                                                    make_continuation_options(0));

    ASSERT_TRUE(off.reached_p1) << trajectory(off);
    ASSERT_EQ(off.steps.size(), 4u) << trajectory(off);
    EXPECT_EQ(off.steps[1].status, SqpStatus::kNumericalError) << trajectory(off);
    EXPECT_EQ(off.proposals_abandoned, 0) << "nothing can be abandoned with the budget off";
    EXPECT_EQ(off.proposals_full_cost, 1) << trajectory(off);
    // OBSERVED VALUES (MKL Pardiso, clang++, Release AND Debug): the failed
    // proposal costs 3 majors / 1002 minors / 303 factorizations. 1002 is two
    // full QP minor caps (QpOptions::max_iter = 500) plus two -- the shape
    // section 4.5(b) calls "cap-pinned".
    EXPECT_EQ(off.steps[1].counters.major_iters, 3) << trajectory(off);
    EXPECT_EQ(off.steps[1].counters.qp_minor_iters, 1002) << trajectory(off);
    // PER-BACKEND, origin entry D19, re-adjudicated at M3 gate B and recorded
    // as M3-2 in docs/notes/2026-08-14-accelerate-divergence-register.md. (The
    // origin note this used to cite, docs/notes/2026-08-01-accelerate-
    // register-3.md from Grant's Mac pass at BASE fe47aef, did NOT migrate
    // into hven; the register above is where the entry lives here, and it says
    // so.) This ONE counter is the whole of the Phase-6 Accelerate divergence
    // register: Apple Accelerate reads 304 here where MKL Pardiso reads 303 --
    // on the FAILED, thrown-away proposal, with majors, minors, status, arms B
    // and C and every backend-independent claim in this file matching exactly.
    //
    // IT WAS LEFT AS AN MKL-ONLY PIN so the next Mac pass would re-adjudicate a
    // number rather than a range. That pass has now happened, on macOS CI
    // (runner class github-macos26-arm64, Release, AppleClang), ten times
    // across the m3 branch -- from
    // https://github.com/GrantHecht/hven/actions/runs/31736708703 (0ba1b9f) to
    // https://github.com/GrantHecht/hven/actions/runs/31824897327 (6535566) --
    // and it re-adjudicated the same number every time. So the arm below is
    // written, still as a NUMBER on each backend rather than a widened
    // inequality. D19's Debug half is not re-observed here: this lane has no
    // Debug leg, and that claim stays on the origin pass's authority.
    //
    // PHASE-6 TASK 4 (M6) DID NOT MOVE IT. The cap this arm runs at is the
    // pre-M6 500, restored explicitly in make_options above, so arms A-C are
    // the SAME trajectory the Mac pass measured; the 303/304 history therefore
    // still applies unchanged. Arms D and E below are NEW and are MKL-observed
    // only -- they have no Apple value yet and must not be given one until a
    // Mac pass produces it.
#ifdef USE_ACCELERATE_SPARSE
    EXPECT_EQ(off.steps[1].counters.factorizations, 304)
        << "Apple Accelerate (origin D19, re-adjudicated as M3-2); MKL Pardiso reads 303\n"
        << trajectory(off);
#else
    EXPECT_EQ(off.steps[1].counters.factorizations, 303)
        << "MKL Pardiso; Accelerate observed 304 (origin D19, re-adjudicated as M3-2)\n"
        << trajectory(off);
#endif
    EXPECT_EQ(off.steps[1].counters.probe_budget_stops, 0);

    // ---- ARM B: the default probe budget ---------------------------------
    F7CollocationChain model_on(100, 3, 2, 0.3, 1.0);
    SqpDriver driver_on(make_options());
    const ContinuationResult on =
        run_continuation(model_on, p_vec(0.3), p_vec(0.9), driver_on, make_continuation_options(2));

    ASSERT_TRUE(on.reached_p1) << trajectory(on);
    ASSERT_EQ(on.steps.size(), off.steps.size())
        << "abandonment is a COST rule: it must not move a single proposal\n"
        << trajectory(on);
    for (std::size_t i = 0; i < on.steps.size(); ++i) {
        EXPECT_EQ(on.steps[i].p(0), off.steps[i].p(0)) << "step " << i << "\n" << trajectory(on);
        EXPECT_EQ(on.steps[i].dp, off.steps[i].dp) << "step " << i << "\n" << trajectory(on);
    }
    EXPECT_EQ(on.proposals_abandoned, 1) << trajectory(on);
    EXPECT_EQ(on.proposals_full_cost, 0) << trajectory(on);
    EXPECT_EQ(on.steps[1].status, SqpStatus::kMaxIter)
        << "an abandoned proposal reports the ordinary stopped-at-an-iterate status\n"
        << trajectory(on);
    EXPECT_EQ(on.steps[1].counters.probe_budget_stops, 1) << trajectory(on);

    // THE ASSERTION THE TASK EXISTS FOR, in its backend-independent form: the
    // failed proposal costs strictly less than it did at full price, and is
    // bounded by its own budget plus the ONE major that crossed it.
    EXPECT_LT(on.steps[1].counters.qp_minor_iters, off.steps[1].counters.qp_minor_iters)
        << trajectory(on);
    EXPECT_LT(on.steps[1].counters.major_iters, off.steps[1].counters.major_iters)
        << trajectory(on);
    // OBSERVED: 2 majors / 502 minors / 14 factorizations against 3 / 1002 /
    // 303 -- the budget here is the floor (continuation_detail::
    // kProbeBudgetFloor = 200; the cold step at p = 0.3 costs 2 minors, so
    // the ratio term is 4), and the crossing major is a cap-pinned QP.
    EXPECT_EQ(on.steps[1].counters.major_iters, 2) << trajectory(on);
    EXPECT_EQ(on.steps[1].counters.qp_minor_iters, 502) << trajectory(on);
    EXPECT_EQ(on.steps[1].counters.factorizations, 14) << trajectory(on);

    // ---- ARM C: the same budget under SqpOptions::budget_mode -------------
    // THE TWO BUDGETS MUST NOT FIGHT (sqp_driver.h's PROBE BUDGET note, part
    // 4). Budgeted mode's own exhaustion status means "continue at the SAME
    // parameter value from this hand-off"; a probe-budget stop means the
    // opposite. So a probe stop keeps the ordinary kMaxIter status even with
    // budgeted mode on, and this loop shrinks rather than continuing --
    // asserted here because the branch is otherwise unreachable in a suite
    // whose every other continuation fixture runs with budget_mode off.
    SqpOptions budgeted = make_options();
    budgeted.budget_mode = true;
    F7CollocationChain model_bm(100, 3, 2, 0.3, 1.0);
    SqpDriver driver_bm(budgeted);
    const ContinuationResult bm =
        run_continuation(model_bm, p_vec(0.3), p_vec(0.9), driver_bm, make_continuation_options(2));
    ASSERT_TRUE(bm.reached_p1) << trajectory(bm);
    EXPECT_EQ(bm.proposals_abandoned, 1) << trajectory(bm);
    EXPECT_EQ(bm.steps[1].status, SqpStatus::kMaxIter)
        << "a probe-budget stop reported budgeted mode's own status, which would make the "
           "controller re-solve AT the proposal it meant to abandon:\n"
        << trajectory(bm);
    EXPECT_EQ(bm.steps[1].counters.probe_budget_stops, 1) << trajectory(bm);
    // ... and the sweep moved the proposal BACK rather than repeating it.
    EXPECT_LT(bm.steps[2].p(0), bm.steps[1].p(0)) << trajectory(bm);

    // ---- ARM D: THE SAME SWEEP AT THE SHIPPED DEFAULT CAP (M6) -----------
    // PHASE-6 TASK 4. Arms A-C restore the pre-M6 fixed cap of 500 explicitly
    // (see make_options). This arm removes that line and nothing else, so the
    // solve runs at the size-derived default -- base = n + mi + #bounded =
    // 500 + 100 + 200 = 800, cap = max(500, 5 * 800) = 4000 -- and the
    // proposal arms A-C spend 1002 minors failing is simply SOLVED.
    //
    // THIS IS M6's WHOLE ARGUMENT, IN ONE ASSERTION, and it is the mutation
    // target for the cap formula: revert the coefficient or the sentinel and
    // this arm goes back to a kNumericalError at step 1.
    SqpOptions derived_cap = make_options();
    derived_cap.qp.max_iter = 0; // the sentinel -- the shipped default
    F7CollocationChain model_m6(100, 3, 2, 0.3, 1.0);
    SqpDriver driver_m6(derived_cap);
    const ContinuationResult m6 =
        run_continuation(model_m6, p_vec(0.3), p_vec(0.9), driver_m6, make_continuation_options(0));
    ASSERT_TRUE(m6.reached_p1) << trajectory(m6);
    for (const auto &step : m6.steps) {
        EXPECT_EQ(step.status, SqpStatus::kOptimal) << trajectory(m6);
    }
    EXPECT_EQ(m6.proposals_full_cost, 0) << "no proposal fails at the size-derived cap\n"
                                         << trajectory(m6);
    // OBSERVED (MKL PARDISO ONLY, clang++, Release): 3 steps rather than arm A's
    // 4 -- the sweep no longer has to back off after a failure -- with the
    // recovered proposal costing 4 majors. NO APPLE VALUE EXISTS FOR ARMS D/E:
    // they are newer than the Mac pass that produced D19 (see the flag on arm
    // A's factorization pin above), and a per-backend arm is only ever written
    // from an observation. If this fails on Accelerate, the count belongs in
    // the divergence register, not in a widened bound here.
    EXPECT_EQ(m6.steps.size(), 3u) << trajectory(m6);
    EXPECT_EQ(m6.steps[1].counters.major_iters, 4) << trajectory(m6);

    // THE HONEST PRICE, ASSERTED IN BOTH DIRECTIONS because it goes both ways
    // and a one-sided assertion here would be flattery. The recovered
    // proposal costs MORE minors than the capped failure it replaces -- 1417
    // against 1002 -- because 1002 was a REFUSAL, not a solve, and the demand
    // at that proposal was always higher than the cap. What M6 buys is not a
    // cheaper step; it is a step that ARRIVES, and therefore a sweep that does
    // not have to back off and re-cross the same range.
    Index m6_minors = 0, off_minors = 0;
    for (const auto &step : m6.steps) {
        m6_minors += step.counters.qp_minor_iters;
    }
    for (const auto &step : off.steps) {
        off_minors += step.counters.qp_minor_iters;
    }
    EXPECT_GT(m6.steps[1].counters.qp_minor_iters, off.steps[1].counters.qp_minor_iters)
        << "a refusal is cheaper than a solve; that is not a defect\n"
        << trajectory(m6);

    // ---- ARM E: M6's cap AND the probe budget together -------------------
    // AND THE UNCOMFORTABLE HALF, MEASURED RATHER THAN OMITTED. On THIS
    // fixture the whole capped sweep costs FEWER minors than the whole
    // recovered one (1053 against 1431, 1.36x) -- because the controller was
    // using the cap as a de facto probe budget: the refusal cost 1002, the
    // back-off to p = 0.5 then crossed the range on two cheap warm steps, and
    // that path is cheaper than solving the long proposal outright. M6 removes
    // the refusal and the controller then pays full price for the long
    // proposal.
    //
    // THE TWO LEVERS ARE COMPLEMENTS, WHICH IS WHY THIS ARM EXISTS. Turn
    // Task 1's probe budget on ALONGSIDE the size-derived cap and the
    // controller gets its early stop back explicitly, from a budget that says
    // what it means, instead of implicitly from a cap that was refusing
    // solvable subproblems. See docs/notes/2026-08-03-crash-basis.md Sec. 6.3.
    F7CollocationChain model_both(100, 3, 2, 0.3, 1.0);
    SqpDriver driver_both(derived_cap);
    const ContinuationResult both = run_continuation(model_both, p_vec(0.3), p_vec(0.9),
                                                     driver_both, make_continuation_options(2));
    ASSERT_TRUE(both.reached_p1) << trajectory(both);
    Index both_minors = 0;
    for (const auto &step : both.steps) {
        both_minors += step.counters.qp_minor_iters;
    }
    ::testing::Test::RecordProperty("m6_sweep_minors",
                                    fmt::format("capped={} derived={} derived_plus_budget={}",
                                                off_minors, m6_minors, both_minors));
    EXPECT_GT(m6_minors, off_minors)
        << "the capped sweep is CHEAPER here -- the finding, not a regression\n"
        << "m6=" << m6_minors << " off=" << off_minors << "\n"
        << trajectory(m6);
    // AND THE PROBE BUDGET DOES **NOT** RECOVER IT, which is why this arm is
    // asserted rather than merely recorded. OBSERVED: 1053 capped / 1431
    // size-derived / 1464 size-derived + budget. The budget still ENGAGES --
    // the long proposal is abandoned, exactly as arm B's is -- but the
    // back-off step it forces is itself expensive at the larger cap, so the
    // sweep total does not come back down. Task 1's lever and M6 are not
    // substitutes for each other on this fixture, and claiming they compose
    // would have been the comfortable reading rather than the measured one.
    EXPECT_EQ(both.proposals_abandoned, 1)
        << "the probe budget must still engage under the size-derived cap\n"
        << trajectory(both);
    EXPECT_GT(both_minors, off_minors) << "both=" << both_minors << " off=" << off_minors << "\n"
                                       << trajectory(both);

    // AND THE CONVERGED STEPS ARE UNTOUCHED -- what was bought was thrown-away
    // work, not answers. (This is also what makes the sweep's own product,
    // the solution path, identical between the two arms.)
    for (std::size_t i : {0u, 2u, 3u}) {
        EXPECT_EQ(on.steps[i].status, SqpStatus::kOptimal) << "step " << i << trajectory(on);
        EXPECT_EQ(on.steps[i].counters.major_iters, off.steps[i].counters.major_iters)
            << "step " << i << "\n"
            << trajectory(on);
        EXPECT_EQ(on.steps[i].counters.qp_minor_iters, off.steps[i].counters.qp_minor_iters)
            << "step " << i << "\n"
            << trajectory(on);
        EXPECT_EQ(on.steps[i].counters.factorizations, off.steps[i].counters.factorizations)
            << "step " << i << "\n"
            << trajectory(on);
        EXPECT_LT((on.steps[i].x - off.steps[i].x).norm(), 1e-12) << "step " << i;
    }
}

// THE CONTROL, and the constraint this task was given rather than a nicety:
// on a sweep where NO proposal fails, the probe budget must be invisible --
// every counter of every step bit-identical to the budget-off run. The same
// F7 fixture at dp_init = 0.2, which crosses the same parameter range without
// a failure (its longest step, 0.5 -> 0.9, costs 4 majors / 47 minors).
//
// IT IS ALSO THE FLOOR'S OWN COUNTER-EXAMPLE. This sweep's first two steps
// cost 2 minors each, so a probe budget scaled by ratio ALONE would budget
// the 47-minor step at 4 minors and abandon it; continuation_detail::
// kProbeBudgetFloor is what stops that, and dropping the floor makes this
// test fail -- its 3 steps / 51 minors become a shrink-and-retry pile.
// **THE 25-STEP / 298-MINOR FIGURE BELONGS TO THE LONGER p: 0.3 -> 1.05
// VARIANT OF THE SAME EXPERIMENT** (4 steps / 63 minors -> 25 / 298, a 4.7x
// regression), NOT to this 0.3 -> 0.9 sweep; this comment used to attribute it
// here (final fix wave, W11). Both readings are in continuation.h's
// kProbeBudgetFloor note and `2026-08-02-controller-retry-economics.md` §2.3,
// which say explicitly that the two ranges are one experiment at two lengths.
TEST(Continuation, HealthySweepIsBitIdenticalUnderTheProbeBudget) {
    const auto run = [](int probe_budget) {
        SqpOptions opts;
        opts.kkt_tol = 1e-8;
        opts.feas_tol = 1e-8;
        opts.max_iter = 100;
        opts.adaptive_mu = false;
        opts.start_level = StartLevel::kWarm;
        opts.warm_full_step = true;
        ContinuationOptions copts;
        copts.use_predictor = false;
        copts.dp_init = 0.2;
        copts.probe_budget = probe_budget;
        F7CollocationChain model(100, 3, 2, 0.3, 1.0);
        SqpDriver driver(opts);
        return run_continuation(model, p_vec(0.3), p_vec(0.9), driver, copts);
    };

    const ContinuationResult off = run(0);
    const ContinuationResult on = run(2);

    ASSERT_TRUE(off.reached_p1) << trajectory(off);
    ASSERT_EQ(off.proposals_full_cost + off.proposals_abandoned, 0)
        << "the control sweep must not fail anywhere:\n"
        << trajectory(off);
    ASSERT_EQ(off.steps.size(), 3u) << trajectory(off);

    EXPECT_TRUE(on.reached_p1);
    EXPECT_EQ(on.proposals_abandoned, 0) << trajectory(on);
    EXPECT_EQ(on.total_majors, off.total_majors) << trajectory(on);
    ASSERT_EQ(on.steps.size(), off.steps.size()) << trajectory(on);
    for (std::size_t i = 0; i < on.steps.size(); ++i) {
        const SqpCounters &a = on.steps[i].counters;
        const SqpCounters &b = off.steps[i].counters;
        EXPECT_EQ(on.steps[i].p(0), off.steps[i].p(0)) << "step " << i;
        EXPECT_EQ(on.steps[i].dp, off.steps[i].dp) << "step " << i;
        EXPECT_EQ(on.steps[i].status, off.steps[i].status) << "step " << i;
        EXPECT_EQ(on.steps[i].level, off.steps[i].level) << "step " << i;
        EXPECT_EQ(a.major_iters, b.major_iters) << "step " << i;
        EXPECT_EQ(a.qp_minor_iters, b.qp_minor_iters) << "step " << i;
        EXPECT_EQ(a.factorizations, b.factorizations) << "step " << i;
        EXPECT_EQ(a.steps_accepted, b.steps_accepted) << "step " << i;
        EXPECT_EQ(a.rejected_steps, b.rejected_steps) << "step " << i;
        EXPECT_EQ(a.border_refine_steps, b.border_refine_steps) << "step " << i;
        EXPECT_EQ(a.full_step_majors, b.full_step_majors) << "step " << i;
        EXPECT_EQ(a.probe_budget_stops, 0) << "step " << i;
        EXPECT_EQ(a.evals_full, b.evals_full) << "step " << i;
        EXPECT_EQ(a.evals_values, b.evals_values) << "step " << i;
        EXPECT_EQ(on.steps[i].x, off.steps[i].x) << "step " << i; // bit-for-bit
    }
}

// THE FAILURE-HISTORY TERM, in isolation: after a failed proposal the first
// accepted step does not lengthen the next one. On this fixture that is a
// COST (an extra step), which is why it is measured here rather than
// asserted to be an improvement -- the sweep it pays for itself on is the
// nx = 10^5 one, in the note.
//
// The dp sequence IS the assertion. With the term off, the retry at 0.5
// succeeds in 1 major (<= target_majors) and dp doubles straight back to the
// 0.4 that just failed, which happens to succeed from the new base; with it
// on, 0.7 is reached by a second 0.2 step first.
TEST(Continuation, GrowthSuspensionSkipsTheStepAfterAFailure) {
    const auto run = [](bool suspend) {
        SqpOptions opts;
        opts.kkt_tol = 1e-8;
        opts.feas_tol = 1e-8;
        opts.max_iter = 100;
        opts.adaptive_mu = false;
        opts.start_level = StartLevel::kWarm;
        opts.warm_full_step = true;
        ContinuationOptions copts;
        copts.use_predictor = false;
        copts.dp_init = 0.4;
        copts.dp_max = 0.4;
        copts.probe_budget = 2;
        copts.suspend_growth_after_failure = suspend;
        F7CollocationChain model(100, 3, 2, 0.3, 1.0);
        SqpDriver driver(opts);
        return run_continuation(model, p_vec(0.3), p_vec(0.9), driver, copts);
    };

    const ContinuationResult grew = run(false);
    const ContinuationResult held = run(true);

    ASSERT_TRUE(grew.reached_p1) << trajectory(grew);
    ASSERT_TRUE(held.reached_p1) << trajectory(held);

    // OBSERVED (MKL Pardiso): the p sequences differ in exactly the way the
    // rule describes.
    ASSERT_EQ(grew.steps.size(), 4u) << trajectory(grew);
    EXPECT_NEAR(grew.steps[3].dp, 0.4, 1e-12) << "the growth rule doubled back:\n"
                                              << trajectory(grew);
    ASSERT_EQ(held.steps.size(), 5u) << trajectory(held);
    EXPECT_NEAR(held.steps[3].dp, 0.2, 1e-12) << "the step after the failure must not grow:\n"
                                              << trajectory(held);
    EXPECT_NEAR(held.steps[4].dp, 0.2, 1e-12) << trajectory(held);
    // The suspension is spent by ONE accepted step, not held forever: step 4
    // is proposed after step 3 succeeded, and step 3's own success is what
    // released the rule (dp would have grown again from step 4 on had the
    // sweep not landed on p1).
    EXPECT_EQ(held.steps[4].p(0), 0.9) << trajectory(held);
}

TEST(Continuation, RejectsMismatchedDimensions) {
    F1BoxQp model(0.0);
    SqpDriver driver(sweep_options());

    // p0 and p1 of different sizes.
    EXPECT_THROW(run_continuation(model, p_vec(0.0), Vec::Zero(2), driver), std::invalid_argument);
    // Both consistent with each other but not with the model.
    EXPECT_THROW(run_continuation(model, Vec::Zero(2), Vec::Ones(2), driver),
                 std::invalid_argument);
    // A non-finite endpoint.
    EXPECT_THROW(run_continuation(model, p_vec(0.0),
                                  p_vec(std::numeric_limits<double>::quiet_NaN()), driver),
                 std::invalid_argument);
    // Options that cannot be honoured.
    ContinuationOptions bad;
    bad.dp_min = 1.0;
    bad.dp_init = 0.1;
    EXPECT_THROW(run_continuation(model, p_vec(0.0), p_vec(1.0), driver, bad),
                 std::invalid_argument);
    // PHASE-6 TASK 1: a negative probe budget has no reading (0 is the
    // disable value), so it is a caller error rather than a silent clamp.
    ContinuationOptions negative_budget;
    negative_budget.probe_budget = -1;
    EXPECT_THROW(run_continuation(model, p_vec(0.0), p_vec(1.0), driver, negative_budget),
                 std::invalid_argument);
}

TEST(Continuation, FailedRunReturnsRatherThanThrows) {
    // max_iter = 0: no solve along this sweep can converge, so even the FIRST
    // (cold) one fails. The contract is a RETURN with reached_p1 == false and
    // the failure recorded, never a throw.
    SqpOptions starved = sweep_options();
    starved.max_iter = 0;

    F1BoxQp model(0.0);
    SqpDriver driver(starved);

    ContinuationResult res;
    ASSERT_NO_THROW(res = run_continuation(model, p_vec(0.0), p_vec(1.0), driver));
    EXPECT_FALSE(res.reached_p1);
    ASSERT_EQ(res.steps.size(), 1u) << trajectory(res);
    EXPECT_NE(res.steps.front().status, SqpStatus::kOptimal);
    // warm_start.h's contract: even a failed solve's exit state is safe to
    // feed forward, so the result still carries one.
    EXPECT_TRUE(res.final_warm.valid);

    // The OTHER failure shape: the step controller shrinks all the way to the
    // floor without a step ever getting through. dp_min is set just below
    // dp_init so exactly one shrink exhausts it.
    SqpOptions one_major = sweep_options();
    one_major.max_iter = 1;
    F2CircleNlp model2(0.0);
    SqpDriver driver2(one_major);
    ContinuationOptions floored;
    floored.dp_init = 0.5;
    floored.dp_max = 0.5;
    floored.dp_min = 0.4;

    ContinuationResult res2;
    ASSERT_NO_THROW(res2 = run_continuation(model2, p_vec(0.0), p_vec(1.0), driver2, floored));
    EXPECT_FALSE(res2.reached_p1) << trajectory(res2);
    EXPECT_GE(res2.steps.size(), 2u) << trajectory(res2);
    EXPECT_NE(res2.steps.back().status, SqpStatus::kOptimal) << trajectory(res2);
}

} // namespace
} // namespace hven::solvers
