// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_sqp_restoration.cpp — Task 9: the restoration phase and the
// rapid infeasibility detection it makes possible.
//
// Three batteries:
//
//   RestorationModel.*      -- the l1 feasibility problem as an NlpModel
//                              wrapper, unit-tested away from the driver: the
//                              exactness of the reformulation (its start point
//                              is feasible with objective exactly h), all four
//                              derivative blocks against finite differences,
//                              and the sigma scaling that Task 8's carry asks
//                              this construction to apply.
//   SqpDriverRestoration.*  -- the phase itself: an infeasible NLP CERTIFIED
//                              (with the subgradient certificate re-derived
//                              in-test from the model, not read off the
//                              driver), a stalled-but-feasible NLP RECOVERED
//                              and resumed to its optimum, the once-per-solve
//                              cap, idleness on the clean battery, and the two
//                              authoritative triggers the phase now serves.
//   SqpDriverRadius.*       -- the radius rules Task 9 owns: the floor
//                              (KLV Algorithm 4's alpha_min analogue) and the
//                              +inf degeneracy, both measured against the
//                              behaviour they replaced.
//
// EVERY ITERATION COUNT HERE IS AN OBSERVATION ON THIS MACHINE and the
// assertions are looser than the observation, per test_sqp_solver.cpp's
// standing convention. The two fixtures' OPTIMA and CERTIFICATES, by contrast,
// are hand-derived in the comments and asserted tightly -- they are properties
// of the problems, not of the run.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>

#include "support/derivative_check.h"
#include "support/hs_problems.h"

#include "hven/core/compiler.h"

// by-value oracle of the in-place hot path; migration is a separate task
HVEN_SUPPRESS_DEPRECATED_BEGIN

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::test_support::assert_gradient;
using hven::solvers::test_support::assert_hessian;
using hven::solvers::test_support::assert_jacobians;
using hven::solvers::test_support::HsProblem;
using hven::solvers::test_support::make_hs;

namespace {

constexpr double kInfBound = 1e20;

// =========================================================================
// FIXTURE 1 -- A PROVABLY INFEASIBLE NLP.
//
//     min  x0 + x1   s.t.  cE1(x) = x0^2 + x1^2 - 1 = 0
//                          cE2(x) = x0 + x1 - 2     = 0
//
// THE PROOF OF INFEASIBILITY, which is why this fixture certifies rather than
// merely fails to converge: cE1 = 0 says ||x||_2 = 1, and Cauchy-Schwarz gives
// |x0 + x1| = |(1,1) . x| <= ||(1,1)||_2 * ||x||_2 = sqrt(2) * 1 = sqrt(2)
// < 2, so cE2 = 0 is unreachable on the circle. The two constraints have no
// common solution anywhere in R^2.
//
// THE h-MINIMIZER, hand-derived, because the test asserts the point and not
// only the status. h(x) = |x0^2 + x1^2 - 1| + |x0 + x1 - 2| is symmetric under
// swapping x0 and x1, so look on the diagonal x0 = x1 = t (the symmetry
// argument is not a proof that the minimizer is there, but the certificate
// asserted in the test IS the proof that the point found is stationary):
//     h(t,t) = |2t^2 - 1| + |2t - 2|  for t <= 1
//            = (1 - 2t^2) + (2 - 2t)  = 3 - 2t - 2t^2   on t <= 1/sqrt(2),
//                                                        decreasing in t
//            = (2t^2 - 1) + (2 - 2t)  = 2t^2 - 2t + 1    on 1/sqrt(2) <= t <= 1,
//                                                        increasing (t > 1/2)
// so the minimum over the diagonal is at t = 1/sqrt(2), where
//     h* = 0 + (2 - sqrt(2)) = 0.585786437626905.
// At that point the CIRCLE is satisfied exactly and only the line is violated:
// cE1 = 0, cE2 = sqrt(2) - 2 < 0.
//
// THE SUBGRADIENT CERTIFICATE there, which is what the driver must return:
// 0 in dh(x) means t*grad cE1 + sign(cE2)*grad cE2 = 0 for some t in [-1,1]
// (t is the free selector on the EXACTLY SATISFIED row):
//     t*(2x0, 2x1) - (1, 1) = t*(sqrt(2), sqrt(2)) - (1,1) = 0
//        => t = 1/sqrt(2) = 0.7071..., which is inside [-1,1].
// So the expected multipliers are lambda_e = (1/sqrt(2), -1) exactly.
// =========================================================================
class InfeasibleCircleLineModel : public NlpModel {
  public:
    explicit InfeasibleCircleLineModel(double s0 = 2.0, double s1 = 2.0) : s0_(s0), s1_(s1) {}

    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return x(0) + x(1); }
    Vec eval_grad(const Vec &) const override { return Vec::Ones(2); }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << x(0) * x(0) + x(1) * x(1) - 1.0, x(0) + x(1) - 2.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    // f is linear and cE2 is linear, so the whole Lagrangian Hessian is
    // lambda_e(0) * hess(cE1) = 2*lambda_e(0)*I.
    SpMatRM eval_hess(const Vec &, double, const Vec &lambda_e, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_e(0);
        h.insert(1, 1) = 2.0 * lambda_e(0);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(2, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.insert(1, 0) = 1.0;
        j.insert(1, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    // (2, 2) is on the diagonal, where grad cE1 = (4,4) is PARALLEL to
    // grad cE2 = (1,1) while the two rows demand different right-hand sides --
    // so the linearization is inconsistent there and the ELASTIC TIER is the
    // first mechanism to fire. See the test for the measured trajectory.
    Vec start_point() const override {
        Vec x(2);
        x << s0_, s1_;
        return x;
    }

  private:
    double s0_, s1_;
};

// =========================================================================
// FIXTURE 2 -- A FEASIBLE NLP WHOSE START STALLS THE FUNNEL.
//
//     min  -10 x1 + 5 x1^2 - 0.01 x0 + 0.0005 x0^2
//     s.t. cE(x) = x1 + 1000 x0^2 = 0,      x_start = (0, 0.5)
//
// THE CONSTRAINT AND THE START ARE test_sqp_solver.cpp's
// SecondOrderInfeasibleModel VERBATIM -- the reviewer-documented route to a
// funnel stall, and the fixture the restoration gate's own test is built on.
// WHAT IS CHANGED IS THE OBJECTIVE, and only in x1: the -10 x1 of that fixture
// is unbounded along the feasible manifold's own direction, so a solve resumed
// after restoration wanders instead of converging (measured: 40 majors, still
// at h = 6.3, when this file was written). Adding the +5 x1^2 term puts a
// genuine minimum where restoration lands, which is what makes this a RECOVERY
// fixture rather than a second stall fixture.
//
// THE STALL, hand-derived (identical to the sibling fixture's, since neither
// the constraint nor the start moved). Je = (2000 x0, 1) = (0, 1) at the start,
// so the linearized equality forces p1 = -0.5 EXACTLY at every radius, and the
// true violation of the trial point is pure second-order error:
//     cE(x + p) = (0.5 + p1) + 1000 p0^2 = 1000 p0^2.
// The objective's -0.01 x0 term against its 0.001 curvature puts the
// unconstrained p0 at 10, so p0 saturates the radius and h_new = 1000*Delta^2:
//     Delta = 8   -> h_new = 64000 > tau = 100   REJECT (rejections = 0)
//     Delta = 4   -> h_new = 16000 > 100         REJECT (1)
//     Delta = 2   -> h_new =  4000 > 100         REJECT (2)
//     Delta = 1   -> h_new =  1000 > 100         REJECT (3)
//     Delta = 0.5 -> h_new =   250 > 100         RESTORE (4 = kRestoreMinRejections)
// with pred_df < 0 throughout (the model pays 3.75 for the constraint-forced
// move in x1 -- f's x1 part goes from -3.75 at x1 = 0.5 to 0 at x1 = 0 -- and
// recovers only 0.01*Delta from x0), h_new >= h_old = 0.5, and tau_0 =
// max(100, 1.25*0.5) = 100. THE RADIUS SCHEDULE STARTS AT 8, NOT AT THE 4 THE
// SIBLING FIXTURE USES, because the gate is now 4 rather than 3 and at
// Delta = 0.25 the trial (h_new = 62.5 <= 100) would be funnel-acceptable --
// see globalization.h's kRestoreMinRejections for the re-derivation.
//
// THE SOLUTION, hand-derived. On the feasible manifold x1 = -1000 x0^2,
//     F(x0) = 10000 x0^2 + 5e6 x0^4 - 0.01 x0 + 0.0005 x0^2,
//     F'(x0) = 20000.001 x0 + 2e7 x0^3 - 0.01 = 0.
// The cubic term is 2.5e-12 at the root and is negligible, so
//     x0* = 0.01 / 20000.001 = 4.99999975e-7,
//     x1* = -1000 x0*^2      = -2.49999975e-10,
//     f*  = F(x0*)           = -2.5000000e-9   (2.5e-9 - 5e-9, to 8 digits).
// KKT there: grad f = (-0.01, -10) to 6 digits, Je = (0.001, 1), so
// lambda_e = 10 from the x1 row and the x0 row closes exactly:
// -0.01 + 0.001*10 = 0.
// =========================================================================
class StalledValleyModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return -10.0 * x(1) + 5.0 * x(1) * x(1) - 0.01 * x(0) + 0.0005 * x(0) * x(0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 0.001 * x(0) - 0.01, 10.0 * x(1) - 10.0;
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(1) + 1000.0 * x(0) * x(0);
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 0.001 + lambda_e(0) * 2000.0;
        h.insert(1, 1) = obj_scale * 10.0;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 2000.0 * x(0);
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 0.0, 0.5;
        return x;
    }
};

// =========================================================================
// FIXTURE 3 -- test_sqp_solver.cpp's InconsistentBoundedModel, copied here
// because this file uses it for a DIFFERENT purpose: its restoration problem
// is the smallest case in the suite where the FEASIBILITY problem's start
// point is ALREADY its solution, which is the normal state of affairs when
// restoration is entered at an infeasible stationary point and is what
// sqp_solver.h's kZeroStepScale short-circuit exists for.
//
//     min 1/2 ||x||^2  s.t.  cE(x) = x0 - 5 = 0,  0 <= x <= 1.
//
// x0 = 5 is outside the box, so no radius makes the linearization consistent
// and the NLP itself is infeasible.
// =========================================================================
class BoxBlockedEqualityModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(0) - 5.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 1.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Zero(2);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Ones(2);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 0.5); }
};

// A model with a general INEQUALITY, used only to exercise the wrapper's
// inequality blocks (its si slacks, their Jacobian column and their share of
// the objective), which the two fixtures above never reach: min x0 s.t.
// cI = x0^2 + x1^2 - 4 <= 0, -3 <= x <= 3.
class CircleInequalityModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return x(0); }
    Vec eval_grad(const Vec &) const override {
        Vec g(2);
        g << 1.0, 0.0;
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c << x(0) * x(0) + x(1) * x(1) - 4.0;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &lambda_i) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_i(0);
        h.insert(1, 1) = 2.0 * lambda_i(0);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -3.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 3.0);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 2.0); }
};

// --- Observers ------------------------------------------------------------

// Wraps the real FunnelStrategy and records what the driver did to it. The
// RESUME hook is the field this file exists to observe: a driver that resumed
// from restoration must have re-based the funnel exactly once, through
// resume_from_restoration and NOT through a second reset().
struct FunnelLog {
    std::vector<double> resets;  // h0 at each reset()
    std::vector<double> resumes; // h_restored at each resume_from_restoration()
    std::vector<double> width_after_resume;
    std::vector<StepVerdict> verdict;
    Index judged = 0;
};

class RecordingFunnel final : public GlobalizationStrategy {
  public:
    explicit RecordingFunnel(FunnelLog *log) : log_(log) {}

    void reset(double h0) override {
        inner_.reset(h0);
        log_->resets.push_back(h0);
    }
    void resume_from_restoration(double h) override {
        inner_.resume_from_restoration(h);
        log_->resumes.push_back(h);
        log_->width_after_resume.push_back(inner_.width());
    }
    StepVerdict judge(const StepContext &ctx) override {
        const StepVerdict v = inner_.judge(ctx);
        ++log_->judged;
        log_->verdict.push_back(v);
        return v;
    }

  private:
    FunnelStrategy inner_;
    FunnelLog *log_;
};

void record_funnel_into(SqpOptions &opts, FunnelLog *log) {
    opts.make_strategy = [log]() -> std::unique_ptr<GlobalizationStrategy> {
        return std::make_unique<RecordingFunnel>(log);
    };
}

// THE SUBGRADIENT CERTIFICATE, recomputed from the MODEL at the returned
// point. This is the in-test check the brief asks for, and it deliberately
// does NOT call the driver's evaluate_kkt or the restoration wrapper: it reads
// only sqp_solver_types.h's documented claim about what a certified kInfeasible exit
// returns, and re-derives it from eval_ce/eval_ci/eval_jac_* directly.
//
//     stationarity = ||Je^T lambda_e + Ji^T lambda_i - z||inf   (NO grad f term
//                    -- that absence is what makes this a certificate of
//                    INFEASIBILITY rather than of optimality)
//     selector     = how far lambda_e leaves [-1,1] / lambda_i leaves [0,1]
//     sign_error   = how far lambda_e(i) is from sign(cE_i) on a VIOLATED row
//                    (and lambda_i(j) from 1 on a violated inequality)
struct SubgradientCertificate {
    double stationarity = 0.0;
    double selector = 0.0;
    double sign_error = 0.0;
    double h = 0.0;
};

SubgradientCertificate check_certificate(const NlpModel &model, const SqpResult &sol,
                                         double row_tol) {
    SubgradientCertificate c;
    Vec r = Vec::Zero(model.n());
    if (model.me() > 0) {
        r += Eigen::MatrixXd(model.eval_jac_e(sol.x)).transpose() * sol.lambda_e;
    }
    if (model.mi() > 0) {
        r += Eigen::MatrixXd(model.eval_jac_i(sol.x)).transpose() * sol.lambda_i;
    }
    r -= sol.z;
    c.stationarity = r.lpNorm<Eigen::Infinity>();

    if (model.me() > 0) {
        const Vec ce = model.eval_ce(sol.x);
        for (Index i = 0; i < model.me(); ++i) {
            c.h += std::abs(ce(i));
            c.selector = std::max(c.selector, std::abs(sol.lambda_e(i)) - 1.0);
            if (std::abs(ce(i)) > row_tol) {
                const double want = ce(i) > 0.0 ? 1.0 : -1.0;
                c.sign_error = std::max(c.sign_error, std::abs(sol.lambda_e(i) - want));
            }
        }
    }
    if (model.mi() > 0) {
        const Vec ci = model.eval_ci(sol.x);
        for (Index j = 0; j < model.mi(); ++j) {
            c.h += std::max(0.0, ci(j));
            c.selector = std::max(c.selector, std::max(-sol.lambda_i(j), sol.lambda_i(j) - 1.0));
            if (ci(j) > row_tol) {
                c.sign_error = std::max(c.sign_error, std::abs(sol.lambda_i(j) - 1.0));
            }
        }
    }
    c.selector = std::max(0.0, c.selector);
    return c;
}

} // namespace

// =========================================================================
// RestorationModel -- the wrapper, away from the driver.
// =========================================================================

// THE EXACTNESS OF THE REFORMULATION, which everything else rests on: the
// start point is FEASIBLE for the wrapper and its objective is EXACTLY
// h(x_entry), and at any wrapper-feasible point the objective is an upper
// bound on h. Checked on a model with both constraint types.
TEST(RestorationModel, IsTheSmoothL1FeasibilityProblem) {
    CircleInequalityModel ineq;
    InfeasibleCircleLineModel eq;

    { // equalities only: n + 2*me variables, same me, no inequalities
        Vec x(2);
        x << 2.0, 2.0;
        const NlpEval ev = eval_nlp(eq, x);
        const RestorationModel r(eq, x, ev);
        EXPECT_EQ(r.n(), 2 + 2 * 2);
        EXPECT_EQ(r.me(), 2);
        EXPECT_EQ(r.mi(), 0);

        const Vec y0 = r.start_point();
        EXPECT_TRUE(r.original_x(y0).isApprox(x));
        // FEASIBLE BY CONSTRUCTION: cE(x) + sigma*(sp - sm) == 0 exactly.
        EXPECT_LT(r.eval_ce(y0).lpNorm<Eigen::Infinity>(), 1e-12);
        // AND ITS OBJECTIVE IS h(x): cE = (7, 2) at (2,2), so h = 9.
        EXPECT_DOUBLE_EQ(constraint_violation_l1(ev), 9.0);
        EXPECT_DOUBLE_EQ(r.eval_f(y0), 9.0);
        // The slacks are one-sided: a positive residual loads sm, not sp.
        for (Index k = 0; k < 2; ++k) {
            EXPECT_DOUBLE_EQ(y0(2 + k), 0.0) << "sp must be 0 where cE > 0";
            EXPECT_GT(y0(2 + 2 + k), 0.0) << "sm carries a positive violation";
        }
        // The x-block keeps the caller's bounds; the slacks are >= 0.
        for (Index i = 0; i < 2; ++i) {
            EXPECT_DOUBLE_EQ(r.lower()(i), eq.lower()(i));
            EXPECT_DOUBLE_EQ(r.upper()(i), eq.upper()(i));
        }
        for (Index k = 2; k < r.n(); ++k) {
            EXPECT_DOUBLE_EQ(r.lower()(k), 0.0);
            EXPECT_GT(r.upper()(k), 0.0);
        }
        // THE OBJECTIVE HAS NO x-BLOCK -- the property that makes a stationary
        // point of this problem a stationary point of h and not of a trade-off.
        const Vec g = r.eval_grad(y0);
        EXPECT_DOUBLE_EQ(g(0), 0.0);
        EXPECT_DOUBLE_EQ(g(1), 0.0);
        for (Index k = 2; k < r.n(); ++k) {
            EXPECT_GT(g(k), 0.0);
        }
    }
    { // inequalities only: one si per row, relaxed downward
        Vec x(2);
        x << 2.0, 2.0; // cI = 4 + 4 - 4 = 4 > 0
        const NlpEval ev = eval_nlp(ineq, x);
        const RestorationModel r(ineq, x, ev);
        EXPECT_EQ(r.n(), 2 + 1);
        EXPECT_EQ(r.me(), 0);
        EXPECT_EQ(r.mi(), 1);

        const Vec y0 = r.start_point();
        EXPECT_DOUBLE_EQ(constraint_violation_l1(ev), 4.0);
        EXPECT_DOUBLE_EQ(r.eval_f(y0), 4.0);
        // cI(x) - sigma*si == 0 at the start: the slack absorbs the whole
        // violation, so the row is satisfied (<= 0) exactly.
        EXPECT_LT(std::abs(r.eval_ci(y0)(0)), 1e-12);
        // An UNDER-satisfied slack leaves the row violated -- i.e. the
        // objective really does bound h from above rather than replacing it.
        Vec y = y0;
        y(2) *= 0.5;
        EXPECT_GT(r.eval_ci(y)(0), 0.0);
        EXPECT_LT(r.eval_f(y), 4.0) << "and the objective is smaller there, which is why the "
                                       "wrapper's own constraints are what keep it honest";
    }
}

// ALL FOUR DERIVATIVE BLOCKS, against central differences -- the same
// transcription guard every model in tests/sqp/support/hs_problems.h passes
// (test_nlp_model.cpp), pointed at the wrapper. This is the check that the
// Jacobian's constant slack columns, the linear objective's gradient and the
// obj_scale = 0 Hessian pass-through are all what the header says they are.
TEST(RestorationModel, DerivativesMatchFiniteDifferences) {
    InfeasibleCircleLineModel eq;
    CircleInequalityModel ineq;
    // FIX ROUND 1 [I2]: A MODEL WITH A NONZERO OBJECTIVE HESSIAN. Both models
    // above have LINEAR objectives, so hess(f) == 0 and the wrapper's
    // obj_scale = 0 pass-through was unobservable -- mutating it to 1.0
    // survived the whole suite. f = 1/2||x||^2 has hess(f) = I, which the
    // wrapper's own Lagrangian must NOT contain (its objective is the linear
    // slack sum), so the mutation now shows up as an I-sized discrepancy.
    BoxBlockedEqualityModel quad;

    Vec x(2);
    x << 2.0, 2.0;
    const RestorationModel re(eq, x, eval_nlp(eq, x));
    const RestorationModel ri(ineq, x, eval_nlp(ineq, x));
    Vec xq(2);
    xq << 0.5, 0.5; // inside the [0,1] box, off the diagonal of its own bounds
    const RestorationModel rq(quad, xq, eval_nlp(quad, xq));

    // Two points each, per the derivative checker's own convention: the start
    // point and a perturbed one (slacks moved off their bounds, x moved off
    // the diagonal, so no block is accidentally zero).
    for (const RestorationModel *r : {&re, &ri, &rq}) {
        Vec y = r->start_point();
        Vec y2 = y;
        for (Index k = 0; k < y2.size(); ++k) {
            y2(k) += 0.25 * static_cast<double>(k + 1);
        }
        const Vec le = Vec::Constant(r->me(), -0.4);
        const Vec li = Vec::Constant(r->mi(), 0.6);
        for (const Vec *at : {&y, &y2}) {
            EXPECT_TRUE(assert_gradient(*r, *at, 1e-7));
            EXPECT_TRUE(assert_jacobians(*r, *at, 1e-6));
            EXPECT_TRUE(assert_hessian(*r, *at, le, li, 1e-5));
        }
    }
}

// TASK 8'S CARRY, APPLIED AND PINNED: the constructed slack column is scaled
// to the JACOBIAN ROW IT JOINS, sigma = max(1, ||grad c_i||inf), so the two
// entries of an augmented row stay within an order of magnitude of each other
// at any constraint scale. Task 8's fix rounds 1-2 measured what happens when
// they do not (a unit column against a row of magnitude S loses the O(1)
// objective tie-break and the solve drifts off the true optimum) and named
// this construction as the next place to check.
//
// Asserted at three scales on the SAME geometry, reading the wrapper's own
// Jacobian rather than an outcome, so the property is pinned where it lives.
TEST(RestorationModel, ScalesSlackColumnsToTheirJacobianRow) {
    for (const double s : {1.0, 1e4, 1e8}) {
        SCOPED_TRACE(fmt::format("scale {:g}", s));
        // grad cI = 2*s*x at x = (s, 0) is (2*s^2, 0) -- an arbitrary row scale
        // without touching the model's own class.
        CircleInequalityModel model;
        Vec x(2);
        x << 1.0, 0.5 * s; // ||grad cI||inf = max(2, s) = s for s >= 2
        const NlpEval ev = eval_nlp(model, x);
        const RestorationModel r(model, x, ev);

        const double row_max = std::max(2.0, s);
        const double sigma = std::max(1.0, row_max);
        ASSERT_EQ(r.slack_scale_i().size(), 1);
        EXPECT_DOUBLE_EQ(r.slack_scale_i()(0), sigma);

        // The augmented row is [ 2x0, 2x1, -sigma ]: the constructed entry is
        // within a factor of 1 of the row's largest Jacobian entry, by
        // construction, at every scale.
        const Eigen::MatrixXd J = Eigen::MatrixXd(r.eval_jac_i(r.start_point()));
        ASSERT_EQ(J.cols(), 3);
        EXPECT_DOUBLE_EQ(J(0, 2), -sigma);
        EXPECT_LE(std::abs(J(0, 2)), row_max) << "a column must never be scaled UP";
        EXPECT_GE(std::abs(J(0, 2)) * row_max, 1.0);

        // AND THE OBJECTIVE STILL PRICES THE ACTUAL VIOLATION, which is what
        // keeps h -- not some rescaled surrogate -- the thing being minimized:
        // the slack VARIABLE is the violation in units of sigma, the objective
        // COEFFICIENT is sigma, so the product is the violation itself. Read at
        // the START's x, which is the entry clamped into [-3, 3] (M7 W2 ladder,
        // L2: x1 = 0.5 s is outside for s >= 1e4); the sigmas stay the entry's.
        const Vec xs = r.original_x(r.start_point());
        EXPECT_DOUBLE_EQ(r.eval_f(r.start_point()), constraint_violation_l1(eval_nlp(model, xs)));
    }
}

// QC-1 -- A SLACK'S BOUND ACTIVITY IS READ IN ITS OWN UNITS. One steep linear row,
// c(x) = 100 (x0 - x1), entered at x = (2.5e-7, 0): the row misses by 2.5e-5, sigma = 100,
// and the wrapper's start puts sm = 2.5e-7 -- inside feas_tol of its zero bound but outside
// feas_tol / sigma. The scalar tolerance reads that slack as bound-active, `z` absorbs its
// objective coefficient and the wrapper's stationarity is 0 at zero multipliers: the
// sub-solve's major-0 "convergence" behind ACOPR57's false certificate. The per-variable
// tolerance the driver hands its sub-driver reads it free, and the unpriced coefficient
// sigma is the stationarity.
class SteepRowModel final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(0) * x(0) + x(1) * x(1); }
    Vec eval_grad(const Vec &x) const override { return 2.0 * x; }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, 100.0 * (x(0) - x(1))); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * obj_scale;
        h.insert(1, 1) = 2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 100.0;
        j.insert(0, 1) = -100.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -10.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 10.0);
        return u;
    }
    Vec start_point() const override { return Vec::Zero(2); }
};

TEST(RestorationModel, BoundActivityIsTheViolationToleranceInEachSlacksUnits) {
    constexpr double kTol = 1e-6;
    SteepRowModel model;
    Vec x(2);
    x << 2.5e-7, 0.0;
    const RestorationModel r(model, x, eval_nlp(model, x));
    ASSERT_EQ(r.n(), 4); // (x0, x1, sp, sm)
    ASSERT_DOUBLE_EQ(r.slack_scale_e()(0), 100.0);

    const Vec t = r.bound_activity_tolerance(kTol);
    ASSERT_EQ(t.size(), r.n());
    EXPECT_EQ(t(0), kTol);
    EXPECT_EQ(t(1), kTol);
    EXPECT_EQ(t(2), kTol / 100.0);
    EXPECT_EQ(t(3), kTol / 100.0);

    const Vec y = r.start_point();
    ASSERT_EQ(y(2), 0.0);
    ASSERT_NEAR(y(3), 2.5e-7, 1e-20);
    const NlpEval ev = eval_nlp(r, y);
    const Vec le = Vec::Zero(1);
    const Vec li = Vec(0);
    const SqpKkt scalar =
        detail::evaluate_kkt_over(r.n(), r.me(), r.mi(), r.lower(), r.upper(), ev, y, le, li, kTol);
    const SqpKkt per =
        detail::evaluate_kkt_over(r.n(), r.me(), r.mi(), r.lower(), r.upper(), ev, y, le, li, t);
    EXPECT_EQ(scalar.stationarity, 0.0) << "the defect: sm reads bound-active, z absorbs sigma";
    EXPECT_EQ(scalar.z(3), 100.0);
    EXPECT_EQ(per.stationarity, 100.0) << "sm is free, so its coefficient sigma is unpriced";
    EXPECT_EQ(per.z(3), 0.0);
    EXPECT_EQ(per.z(2), 100.0) << "sp, at its bound exactly, is active under both";

    // AT A CONSTANT VECTOR THE TWO OVERLOADS ARE ONE MEASUREMENT, field for field.
    const SqpKkt same = detail::evaluate_kkt_over(r.n(), r.me(), r.mi(), r.lower(), r.upper(), ev,
                                                  y, le, li, Vec::Constant(r.n(), kTol));
    EXPECT_EQ(same.stationarity, scalar.stationarity);
    EXPECT_EQ(same.feasibility, scalar.feasibility);
    EXPECT_EQ(same.complementarity, scalar.complementarity);
    EXPECT_EQ(same.z, scalar.z);
    EXPECT_EQ(same.grad_lag, scalar.grad_lag);

    // A MIS-SIZED TOLERANCE IS REFUSED AT THE BOUNDARY, not read past its end.
    EXPECT_THROW(detail::evaluate_kkt_over(r.n(), r.me(), r.mi(), r.lower(), r.upper(), ev, y, le,
                                           li, Vec::Constant(r.n() - 1, kTol)),
                 std::invalid_argument);
}

// A model with BOTH row kinds and different row scales: cE = 10 x0 - 1 (sigma_e = 10),
// cI = 3 x1 - 1 (sigma_i = 3), n = 2, me = mi = 1, wide box.
class MixedScaleRowsModel final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override { return x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return 2.0 * x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << 10.0 * x(0) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c << 3.0 * x(1) - 1.0;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * obj_scale;
        h.insert(1, 1) = 2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 10.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 1) = 3.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -10.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 10.0);
        return u;
    }
    Vec start_point() const override { return Vec::Zero(2); }
};

// THE MIXED CASE (me >= 1 AND mi >= 1): every sp, sm and si slot carries tol over ITS row's
// sigma, at the indices the wrapper's layout (x, sp, sm, si) gives. With sigma_e != sigma_i an
// index slip anywhere in bound_activity_tolerance (e.g. si written at nx + me + j, which is the
// sm block) puts the wrong sigma in a slot and fails here.
TEST(RestorationModel, BoundActivityToleranceReachesEverySlackSlotWithBothRowKinds) {
    constexpr double kTol = 1e-6;
    MixedScaleRowsModel model;
    Vec x(2);
    x << 0.0, 0.0;
    const RestorationModel r(model, x, eval_nlp(model, x));
    ASSERT_EQ(r.me(), 1);
    ASSERT_EQ(r.mi(), 1);
    ASSERT_EQ(r.n(), 5); // (x0, x1, sp, sm, si)
    const double se = r.slack_scale_e()(0);
    const double si_scale = r.slack_scale_i()(0);
    ASSERT_EQ(se, 10.0);
    ASSERT_EQ(si_scale, 3.0);

    const Index nx = model.n();
    const Index sp = nx, sm = nx + r.me(), si = nx + 2 * r.me();
    ASSERT_EQ(r.original_x(r.start_point()).size(), nx);
    ASSERT_EQ(r.start_point().size(), r.n());

    const Vec t = r.bound_activity_tolerance(kTol);
    ASSERT_EQ(t.size(), r.n());
    EXPECT_EQ(t(0), kTol);
    EXPECT_EQ(t(1), kTol);
    EXPECT_EQ(t(sp), kTol / se);
    EXPECT_EQ(t(sm), kTol / se);
    EXPECT_EQ(t(si), kTol / si_scale);
}

// =========================================================================
// THE PROXIMITY TERM (M7 W2 M2b) -- the wrapper's half.
//
// `SqpOptions::restoration_proximity = zeta` puts
// `(zeta/2) ||D (x - x_k)||^2` on the feasibility wrapper's OBJECTIVE, with
// `d_i = min(1, 1/|x_{k,i}|)` and `x_k` the requesting iterate. The design page
// is docs/notes/2026-09-m7-w2-m2-restoration-design.md section 3; the three
// tests below are its test plan's items 1 and 2.
// =========================================================================

// ITEM 1 -- THE TERM IN ISOLATION, against hand-computed values, with d_i
// exercised at all three of the IPM lane's branches (|x_k,i| == 0, < 1, > 1)
// and at zeta == 0.
TEST(RestorationModelProximity, IsTheHandComputedTermAtEveryScalingBranch) {
    CircleInequalityModel model; // n = 2, me = 0, mi = 1
    Vec x_entry(2);
    x_entry << 2.0, 2.0;
    const NlpEval ev = eval_nlp(model, x_entry);

    // THE THREE BRANCHES ON ONE CENTRE IS IMPOSSIBLE AT n = 2, so the branches
    // are read one pair at a time and the |x| == 0 branch gets its own centre.
    {
        Vec centre(2);
        centre << 0.0, 0.5; // branch 1 (|x| == 0, no division) and branch 2 (< 1)
        const RestorationModel r(model, x_entry, ev, centre, 3.0);
        EXPECT_DOUBLE_EQ(r.proximity(), 3.0);
        // d = (1, 1) -- min(1, 1/|x|) is 1 below 1 and AT the zero, where the
        // first branch is what keeps 1/0 out of the arithmetic.
        Vec y = r.start_point();
        y(0) = 1.0; // x - centre = ( 1.0, -0.5 ) at d = (1, 1)
        y(1) = 0.0;
        // READ AS A DIFFERENCE between two points sharing a slack block, so the
        // slack sum cancels and what is asserted is the TERM. P is NOT zero at
        // the start point here: the centre is not the construction point, which
        // is the fired-seed-guard case the sibling test below pins.
        const double p_start = 0.5 * 3.0 * ((2.0 - 0.0) * (2.0 - 0.0) + (2.0 - 0.5) * (2.0 - 0.5));
        const double p_y = 0.5 * 3.0 * (1.0 * 1.0 + 0.5 * 0.5);
        EXPECT_DOUBLE_EQ(r.eval_f(y) - r.eval_f(r.start_point()), p_y - p_start);
        EXPECT_DOUBLE_EQ(r.eval_f(r.start_point()), constraint_violation_l1(ev) + p_start);

        const Vec g = r.eval_grad(y);
        EXPECT_DOUBLE_EQ(g(0), 3.0 * 1.0 * 1.0 * (1.0 - 0.0));
        EXPECT_DOUBLE_EQ(g(1), 3.0 * 1.0 * 1.0 * (0.0 - 0.5));
        EXPECT_DOUBLE_EQ(g(2), r.slack_scale_i()(0)) << "the slack block is untouched by the term";
    }
    {
        Vec centre(2);
        // branch 3, twice: d = (0.4, 0.5). INSIDE the box [-3, 3]: the wrapper
        // clamps its centre (M7 W2 ladder, L2), so an outside centre would be
        // read at the box (RestorationModelClampedStart pins that).
        centre << 2.5, 2.0;
        const RestorationModel r(model, x_entry, ev, centre, 2.0);
        Vec y = r.start_point();
        y(0) = 0.0; // x - centre = (-2.5, -2)
        y(1) = 0.0;
        const double d0 = 0.4, d1 = 0.5;
        const double p_y = 0.5 * 2.0 * ((d0 * -2.5) * (d0 * -2.5) + (d1 * -2.0) * (d1 * -2.0));
        const double p_start = 0.5 * 2.0 * ((d0 * (2.0 - 2.5)) * (d0 * (2.0 - 2.5)) + 0.0 * 0.0);
        EXPECT_DOUBLE_EQ(r.eval_f(y) - r.eval_f(r.start_point()), p_y - p_start);
        const Vec g = r.eval_grad(y);
        EXPECT_DOUBLE_EQ(g(0), 2.0 * d0 * d0 * (-2.5));
        EXPECT_DOUBLE_EQ(g(1), 2.0 * d1 * d1 * (-2.0));

        // THE CURVATURE IS THE TERM'S OWN SECOND DERIVATIVE, zeta*d_i^2 on the
        // x-block diagonal and nothing anywhere else -- and it is the one thing
        // on this wrapper `obj_scale` reaches.
        const Vec lambda_i = Vec::Zero(1);
        const Vec lambda_e = Vec::Zero(0);
        const Eigen::MatrixXd H = Eigen::MatrixXd(r.eval_hess(y, 1.0, lambda_e, lambda_i));
        EXPECT_DOUBLE_EQ(H(0, 0), 2.0 * d0 * d0);
        EXPECT_DOUBLE_EQ(H(1, 1), 2.0 * d1 * d1);
        EXPECT_DOUBLE_EQ(H(0, 1), 0.0);
        for (Index i = 2; i < r.n(); ++i) {
            EXPECT_DOUBLE_EQ(H(i, i), 0.0) << "no slack carries curvature";
        }
        const Eigen::MatrixXd H_half = Eigen::MatrixXd(r.eval_hess(y, 0.5, lambda_e, lambda_i));
        EXPECT_DOUBLE_EQ(H_half(0, 0), 0.5 * 2.0 * d0 * d0)
            << "obj_scale multiplies hess(f), and the proximity term IS part of f";
    }
    { // zeta == 0: the term is not there at all, whatever the centre says
        Vec centre(2);
        centre << 4.0, 2.0;
        const RestorationModel r(model, x_entry, ev, centre, 0.0);
        EXPECT_DOUBLE_EQ(r.proximity(), 0.0);
        Vec y = r.start_point();
        y(0) = 0.0;
        y(1) = 0.0;
        EXPECT_DOUBLE_EQ(r.eval_f(y), r.eval_f(r.start_point()));
        EXPECT_DOUBLE_EQ(r.eval_grad(y)(0), 0.0);
        EXPECT_DOUBLE_EQ(r.eval_grad(y)(1), 0.0);
    }
    // THE BOUNDARY REFUSES WHAT SqpOptions::validate REFUSES, at the wrapper's
    // own door: this class is constructible by anyone holding an NlpModel.
    Vec centre(2);
    centre << 1.0, 1.0;
    EXPECT_THROW(RestorationModel(model, x_entry, ev, centre, -1.0), std::invalid_argument);
    EXPECT_THROW(
        RestorationModel(model, x_entry, ev, centre, std::numeric_limits<double>::quiet_NaN()),
        std::invalid_argument);
    EXPECT_THROW(
        RestorationModel(model, x_entry, ev, centre, std::numeric_limits<double>::infinity()),
        std::invalid_argument);
    EXPECT_THROW(RestorationModel(model, x_entry, ev, Vec::Ones(3), 1.0), std::invalid_argument);
    // ...and a WRONG-SIZED centre at the OFF state is not an error, because the
    // vector is never read there. Stated as a test so the asymmetry is
    // deliberate rather than discovered.
    EXPECT_NO_THROW(RestorationModel(model, x_entry, ev, Vec::Ones(3), 0.0));
}

// ITEM 2 -- THE OFF PROOF IN CODE, NOT ONLY IN ARITHMETIC. At zeta == 0 the
// three evaluators must return today's values AND today's sparsity pattern: a
// zero triplet would still enter the pattern and move the sub-solve's symbolic
// analysis, so the OFF state has to be a BRANCH rather than an added zero. The
// pattern is compared entry by entry, and the zeta > 0 arm shows what the
// branch is hiding -- exactly nx_ new diagonal entries.
TEST(RestorationModelProximity, AtZeroItIsTheWrapperThatShips) {
    InfeasibleCircleLineModel model; // n = 2, me = 2, mi = 0: an inner Hessian with
    Vec x_entry(2);                  // a diagonal the term would otherwise hide in
    x_entry << 2.0, 2.0;
    const NlpEval ev = eval_nlp(model, x_entry);
    Vec centre(2);
    centre << -3.0, 7.0; // DELIBERATELY far from x_entry: at OFF it must not matter

    const RestorationModel ships(model, x_entry, ev);            // the 3-argument wrapper
    const RestorationModel off(model, x_entry, ev, centre, 0.0); // the same thing, spelled long
    const RestorationModel on(model, x_entry, ev, centre, 0.5);

    Vec y = ships.start_point();
    y(0) = 0.25;
    y(1) = -1.5;
    const Vec lambda_e = Vec::Constant(2, 0.5);
    const Vec lambda_i = Vec(0);

    EXPECT_DOUBLE_EQ(off.eval_f(y), ships.eval_f(y));
    const Vec g_off = off.eval_grad(y);
    const Vec g_ships = ships.eval_grad(y);
    ASSERT_EQ(g_off.size(), g_ships.size());
    for (Index i = 0; i < g_ships.size(); ++i) {
        EXPECT_DOUBLE_EQ(g_off(i), g_ships(i)) << "gradient entry " << i;
    }

    const SpMatRM H_ships = ships.eval_hess(y, 1.0, lambda_e, lambda_i);
    const SpMatRM H_off = off.eval_hess(y, 1.0, lambda_e, lambda_i);
    const SpMatRM H_on = on.eval_hess(y, 1.0, lambda_e, lambda_i);

    ASSERT_EQ(H_off.nonZeros(), H_ships.nonZeros());
    for (Index k = 0; k < H_ships.nonZeros(); ++k) {
        EXPECT_EQ(H_off.innerIndexPtr()[k], H_ships.innerIndexPtr()[k]) << "pattern entry " << k;
        EXPECT_DOUBLE_EQ(H_off.valuePtr()[k], H_ships.valuePtr()[k]) << "value entry " << k;
    }
    for (Index r = 0; r <= H_ships.rows(); ++r) {
        EXPECT_EQ(H_off.outerIndexPtr()[r], H_ships.outerIndexPtr()[r]) << "row pointer " << r;
    }

    // AND THE ON ARM IS WHAT THE BRANCH IS KEEPING OUT. This model's inner
    // Hessian already carries both diagonal entries, so the term ADDS to them
    // rather than extending the pattern -- setFromTriplets sums duplicates --
    // which is the harder of the two cases to get right.
    EXPECT_EQ(H_on.nonZeros(), H_ships.nonZeros())
        << "the pattern is unchanged HERE because the inner diagonal already exists; the "
           "sibling test below is the case where it grows";
    const Eigen::MatrixXd D_on = Eigen::MatrixXd(H_on);
    const Eigen::MatrixXd D_ships = Eigen::MatrixXd(H_ships);
    const double d0 = 1.0 / 3.0; // min(1, 1/|-3|)
    const double d1 = 1.0 / 7.0; // min(1, 1/|7|)
    // Written as a SUM rather than as a difference: the term is three orders
    // below the inner entry here, and `(a + x) - a` is not `x` in floating
    // point at that ratio -- so the assertion is built the way the code builds
    // the value (setFromTriplets sums the duplicate) and is exact.
    EXPECT_DOUBLE_EQ(D_on(0, 0), D_ships(0, 0) + 0.5 * d0 * d0);
    EXPECT_DOUBLE_EQ(D_on(1, 1), D_ships(1, 1) + 0.5 * d1 * d1);
}

// AND THE PATTERN REALLY DOES GROW when the inner Hessian has none of its own:
// the same claim on a LINEAR model, whose Lagrangian Hessian is structurally
// empty, where every one of the nx_ diagonal entries is new. That growth is a
// change to the sub-solve's symbolic analysis and is the reason the OFF state
// had to be a branch.
namespace {
// n = 2, one LINEAR equality, a linear objective: eval_hess is empty at every
// argument, which is what makes it the right instrument here.
class LinearPairModel final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(0) + x(1); }
    Vec eval_grad(const Vec &) const override { return Vec::Ones(2); }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(0) + x(1) - 2.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 3.0); }
};
} // namespace

TEST(RestorationModelProximity, AtAPositiveWeightThePatternGainsTheDiagonal) {
    LinearPairModel model;
    Vec x_entry(2);
    x_entry << 3.0, 3.0;
    const NlpEval ev = eval_nlp(model, x_entry);
    Vec centre(2);
    centre << 2.0, 0.5; // d = (0.5, 1): one of each branch

    const RestorationModel off(model, x_entry, ev);
    const RestorationModel on(model, x_entry, ev, centre, 0.25);
    const Vec y = off.start_point();
    const Vec lambda_e = Vec::Constant(1, 0.7);
    const Vec lambda_i = Vec(0);

    const SpMatRM H_off = off.eval_hess(y, 1.0, lambda_e, lambda_i);
    const SpMatRM H_on = on.eval_hess(y, 1.0, lambda_e, lambda_i);
    EXPECT_EQ(H_off.nonZeros(), 0)
        << "the model is linear: the OFF wrapper has no curvature at all";
    EXPECT_EQ(H_on.nonZeros() - H_off.nonZeros(), 2)
        << "one new diagonal entry per ORIGINAL variable, and none on the slacks";
    const Eigen::MatrixXd D_on = Eigen::MatrixXd(H_on);
    EXPECT_DOUBLE_EQ(D_on(0, 0), 0.25 * 0.5 * 0.5);
    EXPECT_DOUBLE_EQ(D_on(1, 1), 0.25 * 1.0 * 1.0);

    // THE ENTRIES ARE STRUCTURAL, not value-dependent: nlp_model.h's PATTERN
    // INVARIANCE precondition forbids a pattern that moves with the arguments,
    // so obj_scale = 0 must emit the same nonZeros as obj_scale = 1.
    const SpMatRM H_zero_scale = on.eval_hess(y, 0.0, lambda_e, lambda_i);
    EXPECT_EQ(H_zero_scale.nonZeros(), H_on.nonZeros());
    EXPECT_DOUBLE_EQ(Eigen::MatrixXd(H_zero_scale)(0, 0), 0.0);
}

// THE SMOOTH-l1 IDENTITY'S zeta > 0 SIBLING (the brief's fixture list; the
// design page's section 7 pin). The wrapper's objective at its own start point
// is h(x_entry) at zeta = 0 AND at zeta > 0 whenever the centre is the
// construction point -- which is every solve whose seed guard did not fire. When
// the guard DOES fire the two differ by P(x_entry) exactly, and that is the
// price of centring the term on the REQUESTING ITERATE (restoration.h's class
// note, and the ruling recorded in the design page's section 11 item 8).
TEST(RestorationModel, IsTheSmoothL1FeasibilityProblemAtAPositiveProximity) {
    InfeasibleCircleLineModel eq;
    Vec x_entry(2);
    x_entry << 2.0, 2.0;
    const NlpEval ev = eval_nlp(eq, x_entry);
    const double zeta = 4.0;

    { // THE UNFIRED GUARD: centre == construction point, and the identity holds
        const RestorationModel r(eq, x_entry, ev, x_entry, zeta);
        const Vec y0 = r.start_point();
        EXPECT_DOUBLE_EQ(constraint_violation_l1(ev), 9.0);
        EXPECT_DOUBLE_EQ(r.eval_f(y0), 9.0) << "P(x_entry) = 0 when the centre IS x_entry";
        // The wrapper is still FEASIBLE at its start point -- the term is on the
        // objective and touches no constraint.
        EXPECT_LT(r.eval_ce(y0).lpNorm<Eigen::Infinity>(), 1e-12);
        // ...and the x-block of the gradient is now zero for a REASON -- y0's
        // x-block is the centre -- rather than structurally.
        EXPECT_DOUBLE_EQ(r.eval_grad(y0)(0), 0.0);
        EXPECT_DOUBLE_EQ(r.eval_grad(y0)(1), 0.0);
    }
    { // THE FIRED GUARD: centre = the requesting iterate, construction at a seed
        Vec x_k(2);
        x_k << 3.0, 1.0;
        const RestorationModel r(eq, x_entry, ev, x_k, zeta);
        const Vec y0 = r.start_point();
        const double d0 = 1.0 / 3.0, d1 = 1.0;
        const double p0 =
            0.5 * zeta *
            ((d0 * (2.0 - 3.0)) * (d0 * (2.0 - 3.0)) + (d1 * (2.0 - 1.0)) * (d1 * (2.0 - 1.0)));
        EXPECT_DOUBLE_EQ(r.eval_f(y0), 9.0 + p0)
            << "the identity is a zeta = 0 fact, or an unfired-guard fact -- and this is the "
               "exact, declared, amount by which it fails otherwise";
        EXPECT_GT(p0, 0.0);
        // WHAT DOES NOT MOVE: the reformulation's EXACTNESS. The term is part of
        // the objective being minimized, not of the violation being measured, so
        // the wrapper's constraints and its slack scaling are untouched.
        EXPECT_LT(r.eval_ce(y0).lpNorm<Eigen::Infinity>(), 1e-12);
        const RestorationModel ships(eq, x_entry, ev);
        EXPECT_TRUE(
            Eigen::MatrixXd(r.eval_jac_e(y0)).isApprox(Eigen::MatrixXd(ships.eval_jac_e(y0))));
        EXPECT_TRUE(r.start_point().isApprox(ships.start_point()));
    }
}

// =========================================================================
// SqpDriverRestoration -- the phase.
// =========================================================================

// TEST (a). A PROVABLY INFEASIBLE NLP is CERTIFIED: the driver returns
// kInfeasible in bounded iterations, at a point the test independently
// verifies to be a stationary point of h.
//
// THE MEASURED TRAJECTORY from (2, 2) in border mode, which is the whole
// mechanism in three rows: the linearization is inconsistent on the diagonal
// (grad cE1 = (4,4) is parallel to grad cE2 = (1,1) with incompatible
// right-hand sides), so the ELASTIC TIER fires on every major; two elastic
// steps are h-reducing and are accepted (h: 9 -> 1.78 -> 1); at (1,1) the
// relaxation is open at rho_max, nothing reduces the linearized violation and
// the model promises no objective decrease -- the tier is EXHAUSTED, which is
// KLV Algorithm 5's authoritative trigger -- and the restoration phase runs.
// It converges in 4 majors to (1/sqrt(2), 1/sqrt(2)).
//
// THE TWO ALGEBRA MODES REACH THE SAME VERDICT BY DIFFERENT ROUTES AND AT
// VERY DIFFERENT COST, and fix round 1 corrected this note, which used to
// claim more invariance than the measurements support. In REFACTORIZE mode the
// elastic solve at (1,1) comes back marginally USABLE rather than exhausted,
// so the tier never signals and the solve instead grinds the radius down to
// the FLOOR (60 majors), which raises the request through Task 9's other
// authoritative trigger.
//
// THE INVARIANCE IS CONDITIONAL ON THE BUDGET, and that is the part the
// earlier text got wrong. Measured, max_iter swept:
//
//     max_iter     border                    refactorize
//     10..60       kInfeasible, 3 + 4        kMaxIter, max_iter + 0
//     >= 65        kInfeasible, 3 + 4        kInfeasible, 60 + 5
//
// i.e. below 65 the two modes return DIFFERENT STATUSES -- refactorize never
// reaches restoration at all and reports a budget outcome. Above it, the
// certificate is identical: same point to 6 digits, same multipliers, same h.
// So the verdict is a property of the problem GIVEN ENOUGH BUDGET, and the
// budget it needs is mode-dependent by a factor of nine on this fixture. The
// route difference is in the elastic tier's borderline usability test (Task
// 8's), not in this task's mechanism; the STATUS divergence under budget is
// carried to Task 11's battery.
//
// THE FIXTURE THEREFORE RUNS AT max_iter = 200, not at the default 100: the
// refactorize path needs 65 and a default-budget run would sit one regression
// away from flipping status rather than failing an assertion. The margin is
// explicit here instead of implicit there.
TEST(SqpDriverRestoration, InfeasibleNlpCertifies) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        InfeasibleCircleLineModel model;
        SqpOptions opts;
        opts.max_iter = 200; // observed need: 7 (border), 65 (refactorize)
        opts.qp.ws_algebra = algebra;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);

        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified)
            << "the status alone does not say whether a certificate exists; this flag does";
        EXPECT_GE(sol.counters.restoration_iters, 1) << "the phase must have RUN, not been skipped";
        // BOUNDED, and bounded by the VERDICT rather than by the budget: the
        // solve stops because the feasibility problem converged, with majors
        // to spare on both phases (observed 3 + 4 in border mode, 60 + 5 in
        // refactorize -- see the note above). The margin is asserted as a
        // FACTOR, not as "one less than the budget", so that a regression that
        // merely doubles the cost fails here instead of silently flipping the
        // status to kMaxIter.
        EXPECT_LT((sol.counters.major_iters + sol.counters.restoration_iters) * 2, opts.max_iter)
            << "observed 7 (border) and 65 (refactorize) against a budget of " << opts.max_iter;
        EXPECT_LE(sol.counters.restoration_iters, 20)
            << "the feasibility problem is small and must not be where the budget goes";

        // THE POINT: the h-minimizer derived in the fixture comment.
        const double t = 1.0 / std::sqrt(2.0);
        EXPECT_NEAR(sol.x(0), t, 1e-5);
        EXPECT_NEAR(sol.x(1), t, 1e-5);

        // THE CERTIFICATE, recomputed from the model (see check_certificate).
        // Its four parts together say: 0 is a subgradient of h at the returned
        // point, and h there is above the feasibility tolerance -- which is
        // exactly the Byrd-Curtis-Nocedal infeasible-stationary-point verdict.
        const SubgradientCertificate c = check_certificate(model, sol, opts.feas_tol);
        EXPECT_NEAR(c.h, 2.0 - std::sqrt(2.0), 1e-6);
        EXPECT_GT(c.h, opts.feas_tol) << "a certificate of infeasibility at a FEASIBLE point "
                                         "would be a contradiction, not a certificate";
        EXPECT_LE(c.stationarity, opts.kkt_tol) << "0 must be a subgradient of h here";
        EXPECT_LE(c.selector, 1e-9) << "the multipliers must be admissible selectors";
        EXPECT_LE(c.sign_error, 1e-5) << "a VIOLATED row's selector is pinned to its sign";
        // And the selector on the exactly-satisfied circle row is the interior
        // value the hand derivation predicts -- the part of the certificate
        // that could not have come out right by accident. (It is interior by a
        // wide margin, |lambda_e(0)| = 0.707 against the bound of 1, so this
        // is a genuine interior selector and not a saturated one.)
        EXPECT_NEAR(sol.lambda_e(0), t, 1e-5);
        EXPECT_NEAR(sol.lambda_e(1), -1.0, 1e-5);

        ::testing::Test::RecordProperty(
            "certificate", fmt::format("h={:.10g} stat={:.3g} le=({:.10g},{:.10g}) majors={}+{}",
                                       c.h, c.stationarity, sol.lambda_e(0), sol.lambda_e(1),
                                       sol.counters.major_iters, sol.counters.restoration_iters));
    }
}

// THE CERTIFICATE GATE AT A POSITIVE PROXIMITY WEIGHT (M7 W2 M2b; the design
// page's sections 3.4 and 11 question 1). Three arms on the fixture directly
// above, which is the project's certified exit:
//
//   zeta = 0        -- the gate is `0.0 <= kkt_tol` and the certified exit is
//                      the one that ships, asserted here at the field's OFF
//                      value written EXPLICITLY, so the OFF half of the gate is
//                      pinned rather than inherited;
//   zeta tiny       -- the phase moves, the residual stays under kkt_tol, and
//                      the certificate SURVIVES. This arm is what makes the
//                      gate a test of the RESIDUAL: a mutant reading
//                      `certified = (zeta == 0)` passes the other two and dies
//                      here;
//   zeta = 1e-2     -- the residual is above kkt_tol and the flag is REFUSED
//                      while the status stays kInfeasible. What the refusal
//                      withholds is the CLAIM.
//
// The quantity is the UNCORRECTED residual `||J^T lambda - z_sub||_inf`, which
// `check_certificate` already computes and which needs NO change for any of
// this -- that helper is byte-identical across the three arms, and the
// correction the design page's first draft proposed (subtracting grad P from z)
// would have made it identically zero at every wrapper-stationary point, i.e.
// certified everything.
TEST(SqpDriverRestorationProximity, TheCertificateIsGatedOnTheUncorrectedResidual) {
    struct Arm {
        double zeta;
        bool certified;
        const char *what;
    };
    for (const Arm arm : {Arm{0.0, true, "OFF -- the exit that ships"},
                          Arm{1e-12, true, "on, but not doing any work at the returned point"},
                          Arm{1e-2, false, "on and doing work -- the claim is withheld"}}) {
        SCOPED_TRACE(fmt::format("zeta={:g} ({})", arm.zeta, arm.what));
        InfeasibleCircleLineModel model;
        SqpOptions opts;
        opts.max_iter = 200;
        opts.restoration_proximity = arm.zeta;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);

        EXPECT_EQ(sol.status, SolveStatus::kInfeasible)
            << "the STATUS is not what the term moves -- the flag is";
        EXPECT_GE(sol.counters.restoration_iters, 1) << "the phase must have RUN";
        EXPECT_EQ(sol.infeasibility_certified, arm.certified);

        // AND THE GATE'S OWN QUANTITY, recomputed from the model by the same
        // helper the certified exit has always been checked with.
        const SubgradientCertificate c = check_certificate(model, sol, opts.feas_tol);
        EXPECT_GT(c.h, opts.feas_tol) << "a certificate at a FEASIBLE point would be a "
                                         "contradiction -- the h half is unmoved by the term";
        if (arm.certified) {
            EXPECT_LE(c.stationarity, opts.kkt_tol)
                << "0 is a subgradient of h at the returned point";
        } else {
            EXPECT_GT(c.stationarity, opts.kkt_tol)
                << "the returned point is stationary for h + P and NOT for h, and that gap is "
                   "exactly what the driver refused to certify";
        }
        ::testing::Test::RecordProperty(
            fmt::format("zeta_{:g}", arm.zeta),
            fmt::format("certified={} stat={:.6g} h={:.6g} x=({:.8g},{:.8g}) majors={}+{}",
                        sol.infeasibility_certified, c.stationarity, c.h, sol.x(0), sol.x(1),
                        sol.counters.major_iters, sol.counters.restoration_iters));
    }
}

// PHASE-5 TASK 4 -- THE MECHANISM BEHIND THE ALGEBRA-MODE STATUS DIVERGENCE
// THE TEST ABOVE DOCUMENTS, characterized and ruled BENIGN. The Phase-3 close
// carried it forward as "genuinely a distinct mechanism, still unexplained at
// depth" (docs/notes/2026-07-29-phase-3-close-carries.md); the analysis is
// section 4 of docs/notes/2026-07-31-schur-cap-policy.md and this test is its
// evidence, kept executable rather than quoted.
//
// WHAT DIVERGES. Under a budget below 65, border mode certifies kInfeasible in
// 3 + 4 majors and refactorize mode reports kMaxIter (it needs 60 + 5). The
// trajectories part company at MAJOR 1, and not by rounding: the elastic step
// there has |p|inf = 0.125 in border mode and 0.365 in refactorize mode.
//
// WHY IT IS NOT A DIVERGENCE IN THE ANSWER. At that major the elastic
// relaxation is still open, so elastic_project zeroes the multipliers
// (sqp_solver.h's MULTIPLIERS ARE NOT CARRIED OUT OF AN OPEN RELAXATION); this
// model's Lagrangian Hessian is 2*lambda_e(0)*I, so with lambda_e == 0 the
// SUBPROBLEM HESSIAN IS IDENTICALLY ZERO and the subproblem is a LINEAR
// PROGRAM. Its optimal set is a face, not a point: both modes land on the SAME
// face -- both iterates satisfy x0 + x1 = 2 exactly and give the same f -- and
// each returns a different, equally optimal point of it. Which point comes
// back selects which trajectory follows, and only a budget turns that into a
// status difference.
//
// THE FACE IS A NUMERICAL OBJECT, NOT AN EXACT ONE. The engine does not solve
// the model's H; it solves H + primal_delta*I (1e-8 at the defaults), which is
// formally strictly convex and so has a unique optimum. The precise statement
// is therefore: the MODEL subproblem's optimal set is a face, and the
// regularized problem selects a point on it that is determined only at O(delta)
// -- a 1e-8 curvature spread across an O(1) face is settled by whatever the
// linear algebra does in its last digits, which is why the two modes land 0.24
// apart while agreeing on the objective to 1e-15. The assertions below are
// written for that reading: they check the face membership exactly (x0 + x1 = 2
// is a CONSTRAINT-side identity, not a curvature one) and the separation only
// as "materially nonzero".
//
// SO THE EQUIVALENCE ORACLE IS NOT BROKEN. qp_engine.h scopes the
// border/refactorize equivalence claim to CONVEX H; H == 0 is convex but not
// strictly convex, so it delivers a common optimal VALUE and not a common
// optimal POINT. qp_types.h's WorkingSetLinearAlgebra note carries that caveat
// explicitly since this task.
//
// AND IT IS NOT A MODE RANKING. Border mode's 3-major certification is a
// property of the EXACTLY SYMMETRIC start (2, 2), where grad cE1 = 2x*(1,1) is
// parallel to grad cE2 = (1,1) and the whole trajectory stays on the diagonal.
// Perturbing the start by 1e-6 destroys it: measured majors for
// (2, 1.999999) / (2, 1.99) / (2, 1.9) / (2, 1.5) are 61/47/44/47 in border
// mode and 46/43/57/44 in refactorize, i.e. both modes grind and neither wins.
// Every one of those eight runs returns kInfeasible at the same certified
// point (0.707107, 0.707107).
TEST(SqpDriverRestoration, AlgebraModeDivergenceIsANonUniqueSubproblemOptimum) {
    // (a) THE OPTIMAL FACE. Stop both modes after the major where they part
    // company and read the iterate each landed on.
    Vec x_border(2), x_refac(2);
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        InfeasibleCircleLineModel model;
        SqpOptions opts;
        // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
        opts.equality_basis = false;
        // M7 W2 E2 (S7): the two algebras' different points on the face belong
        // to the pre-E2 walk; with the independent working set both land on
        // x0 + x1 = 2.25, the same point.
        opts.qp.independent_working_set = false;
        opts.max_iter = 2;
        opts.qp.ws_algebra = algebra;
        // M7 W2 R2: the restoration-route fixtures on this family pin the
        // pre-R2 ROUTE, so the failed-subproblem step is off (see
        // SqpDriverRestorationSeed.TheFailedStepKeepsTheCertifiedAnswer for the default).
        opts.qp_failure_step = false;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);
#ifdef USE_ACCELERATE_SPARSE
        if (algebra == WorkingSetLinearAlgebra::kSchurBorder) {
            // Origin divergence entry D14. The note that carried it
            // (docs/notes/2026-07-30-accelerate-at-head-results.md) did NOT
            // migrate into hven, and the register that succeeds it here
            // (docs/notes/2026-08-14-accelerate-divergence-register.md, "Why
            // this file exists at this path") has no row for D14 -- the
            // observation is quoted in full below and in the second D14 block
            // further down, and those comments are its citable record here.
            // What was observed: on
            // Accelerate, border mode departs from the MKL mechanism at major
            // 0 -- the first elastic subproblem exits QpStatus::kMaxIter with
            // |p|inf == 0 instead of taking MKL's 0.125 elastic step, and the
            // driver routes the failure into restoration immediately. At the
            // max_iter = 2 cap the solve is therefore mid-restoration, not
            // sitting in an open elastic relaxation, so lambda_e(0) here is a
            // RESTORATION-PHASE multiplier and is not zero. Value is
            // Accelerate-observed (macOS 26.5.2 / AppleClang 21) and will be
            // re-verified on the next Mac session.
            EXPECT_NEAR(sol.lambda_e(0), 0.25000000000000011, 1e-12)
                << "mid-restoration multiplier at the max_iter=2 cap (Accelerate, D14)";
        } else {
            // Refactorize mode still matches the MKL mechanism on Accelerate.
            EXPECT_EQ(sol.lambda_e(0), 0.0) << "an open elastic relaxation carries no multipliers";
        }
#else
        // The multipliers the NEXT subproblem would be built from are zero,
        // which is what makes that subproblem's Hessian 2*lambda_e(0)*I == 0
        // and its optimum a face rather than a point.
        EXPECT_EQ(sol.lambda_e(0), 0.0) << "an open elastic relaxation carries no multipliers";
#endif
        (algebra == WorkingSetLinearAlgebra::kSchurBorder ? x_border : x_refac) = sol.x;
    }

#ifdef USE_ACCELERATE_SPARSE
    // Origin divergence entry D14, second half (see the first block above for
    // why the origin note is cited as not-migrated and this comment is the
    // record): on Accelerate, border mode's iterate at the cap is mid-restoration
    // -- x = (1.50000000031, 1.49999999969), sum 3.0000000000000004 -- not on
    // the open-relaxation LP face x0 + x1 == 2 that MKL and refactorize mode
    // both reach. Refactorize mode is unaffected. Values are
    // Accelerate-observed (macOS 26.5.2 / AppleClang 21) and will be
    // re-verified on the next Mac session; the x-components are quoted to 11
    // significant digits in the origin note, hence the looser tolerance there
    // than on the (full-precision) sum.
    EXPECT_NEAR(x_border(0), 1.50000000031, 1e-9);
    EXPECT_NEAR(x_border(1), 1.49999999969, 1e-9);
    EXPECT_NEAR(x_border(0) + x_border(1), 3.0000000000000004, 1e-12)
        << "mid-restoration point, not the LP face (Accelerate, D14)";
    EXPECT_NEAR(x_refac(0) + x_refac(1), 2.0, 1e-12);
    // Still the same CLASS of claim part (a) makes on MKL: the two modes land
    // on materially different points, just for a different reason here (one
    // is mid-restoration, not a different face point of the same relaxation).
    EXPECT_GT((x_border - x_refac).lpNorm<Eigen::Infinity>(), 1e-3)
        << "the two modes no longer diverge on this fixture; the mechanism "
           "this test documents (as adapted for Accelerate, D14) has changed";
#else
    // RE-DERIVED AT M7 W2 E4 FIX ROUND 2 (a declared mover), and the reading is now
    // the one the Accelerate arm above already carried (origin divergence D14): at
    // the cap the BORDER mode's iterate is MID-RESTORATION -- x = (1.125, 1.125),
    // sum 2.2499999999999964 -- rather than on the open-relaxation LP face
    // x0 + x1 == 2, which refactorize mode still reaches (1.9999999999999996).
    // That is a different reason for the same divergence, and the fixture's claim --
    // the two modes land on materially different points, which is a choice among
    // answers and not a disagreement about one -- is asserted unchanged below.
    // The border point is a symmetric interior point of the restoration's own path,
    // so nothing about it is out of the box or off a row.
    // BOTH ITERATES SIT ON THE SAME OPTIMAL FACE x0 + x1 = 2 ...
    EXPECT_NEAR(x_border(0), 1.125, 1e-12);
    EXPECT_NEAR(x_border(1), 1.125, 1e-12);
    EXPECT_NEAR(x_border(0) + x_border(1), 2.25, 1e-12) << "mid-restoration point, not the LP face";
    EXPECT_NEAR(x_refac(0) + x_refac(1), 2.0, 1e-12);
    // ... and they are DIFFERENT points of it. This is the divergence, stated
    // as what it is: a choice among equally optimal answers, not a
    // disagreement about the answer. Observed (1, 1) versus
    // (1.240116, 0.759884).
    EXPECT_GT((x_border - x_refac).lpNorm<Eigen::Infinity>(), 1e-3)
        << "the two modes no longer pick different points of the LP face; the "
           "mechanism this test documents has changed";
#endif

    // (b) FRAGILITY. Break the start's symmetry by 1e-6 and border mode loses
    // its cheap certification, while BOTH modes still return the same
    // certified point -- so the divergence is a property of this start, not a
    // property of either algebra mode.
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        InfeasibleCircleLineModel model(2.0, 1.999999);
        SqpOptions opts;
        // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
        opts.equality_basis = false;
        opts.max_iter = 200;
        opts.qp.ws_algebra = algebra;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);
        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified);
        const double t = 1.0 / std::sqrt(2.0);
        EXPECT_NEAR(sol.x(0), t, 1e-5);
        EXPECT_NEAR(sol.x(1), t, 1e-5);
        EXPECT_GT(sol.counters.major_iters, 10)
            << "the symmetric start's 3-major certification must not survive a "
               "1e-6 perturbation; if it does, this fixture is no longer the "
               "knife-edge the analysis says it is";
    }
}

// TEST (b). A FEASIBLE NLP whose start stalls the funnel is RECOVERED: the
// phase runs, the main loop resumes from the restored point and converges to
// the fixture's hand-derived optimum.
//
// THE OBSERVABLE MECHANISM, all four parts asserted rather than inferred from
// the status: restoration was entered, the funnel was RE-BASED (once, through
// resume_from_restoration and not through a second reset), the radius was
// restarted at the documented fraction of tr_init, and the trial that raised
// the request was the fifth consecutive rejection at the start point -- i.e.
// the funnel's own signature, at the re-derived gate.
TEST(SqpDriverRestoration, RestorationRecoversAndResumes) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        StalledValleyModel model;
        SqpOptions opts;
        opts.tr_init = 8.0; // the radius schedule in the fixture comment
        opts.max_iter = 60;
        opts.qp.ws_algebra = algebra;
        FunnelLog log;
        record_funnel_into(opts, &log);

        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);

        ASSERT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_FALSE(sol.infeasibility_certified) << "a RECOVERED solve certifies nothing";
        EXPECT_GE(sol.counters.restoration_iters, 1);

        // THE OPTIMUM, hand-derived in the fixture comment.
        EXPECT_NEAR(sol.x(0), 4.99999975e-7, 1e-11);
        EXPECT_NEAR(sol.x(1), -2.49999975e-10, 1e-13);
        EXPECT_NEAR(sol.f, -2.5e-9, 1e-11);
        EXPECT_NEAR(sol.lambda_e(0), 10.0, 1e-6);
        EXPECT_LE(sol.history.back().kkt_residual, opts.kkt_tol);

        // THE STALL, and that it is the FUNNEL's signature that broke it: five
        // consecutive rejections at the start point (rejections 0..4, the
        // fifth trial being the first at which conjunct (e) can hold), the
        // fifth carrying the kRestore verdict.
        ASSERT_GE(sol.history.size(), 6u);
        for (std::size_t k = 0; k < 5; ++k) {
            SCOPED_TRACE(fmt::format("trial {}", k));
            EXPECT_DOUBLE_EQ(sol.history[k].f, sol.history[0].f) << "the iterate must not move";
            EXPECT_DOUBLE_EQ(sol.history[k].tr_radius, 8.0 / std::pow(2.0, k));
        }
        for (std::size_t k = 0; k < 4; ++k) {
            EXPECT_EQ(sol.history[k].verdict, StepVerdict::kReject) << "trial " << k;
        }
        EXPECT_EQ(sol.history[4].verdict, StepVerdict::kRestore)
            << "exactly kRestoreMinRejections rejections precede the request";
        EXPECT_FALSE(sol.history[4].elastic_applied)
            << "this is the funnel's own signature, not the elastic tier's exhaustion";

        // THE FUNNEL WAS RE-BASED, NOT RE-INITIALIZED. reset() ran once (at the
        // start point, h0 = 0.5); the resume ran once, at a point whose h is
        // below feas_tol; and Eq. (13) took the width DOWN from 100 to
        // 0.5*h + 0.5*100, i.e. essentially halved it. A driver that called
        // reset(h_restored) instead would have put it back to
        // max(100, 1.25*h) = 100.
        ASSERT_EQ(log.resets.size(), 1u);
        EXPECT_DOUBLE_EQ(log.resets[0], 0.5);
        ASSERT_EQ(log.resumes.size(), 1u);
        EXPECT_LE(log.resumes[0], opts.feas_tol);
        ASSERT_EQ(log.width_after_resume.size(), 1u);
        EXPECT_NEAR(log.width_after_resume[0], 50.0, 1e-9)
            << "Eq. (13) at h ~ 0: tau_+ = 0.5*h + 0.5*100";

        // THE RADIUS RESTARTED AT THE DOCUMENTED FRACTION of tr_init, at the
        // first row after the request.
        EXPECT_DOUBLE_EQ(sol.history[5].tr_radius, kRestoreRadiusFactor * opts.tr_init);
        // And the restored iterate really is a different (feasible) point.
        EXPECT_LE(sol.history[5].violation_l1, opts.feas_tol);
        EXPECT_GT(sol.history[4].violation_l1, opts.feas_tol);

        ::testing::Test::RecordProperty(
            "recovery",
            fmt::format("majors={} restoration={} accepted={} f={:.10g}", sol.counters.major_iters,
                        sol.counters.restoration_iters, sol.counters.steps_accepted, sol.f));
    }
}

// TEST (c). The phase is INERT on every problem that does not need it. Same
// fixture set and same options as SqpDriverElastic.ElasticIsIdleOnCleanProblems
// (test_sqp_solver.cpp), which is the sibling claim for the tier below it.
TEST(SqpDriverRestoration, RestorationIsIdleOnCleanProblems) {
    struct Case {
        int number;
        SqpOptions opts;
    };
    std::vector<Case> cases;
    {
        SqpOptions o;
        o.max_iter = 60;
        cases.push_back({1, o});
        cases.push_back({3, o});
    }
    {
        SqpOptions o;
        cases.push_back({5, o});
        cases.push_back({6, o});
        cases.push_back({76, o});
    }
    {
        SqpOptions o;
        o.tr_init = 10.0;
        o.kkt_tol = 1e-8;
        o.feas_tol = 1e-8;
        cases.push_back({7, o});
    }

    for (const Case &c : cases) {
        SCOPED_TRACE(fmt::format("HS{}", c.number));
        const HsProblem p = make_hs(c.number);
        FunnelLog log;
        SqpOptions opts = c.opts;
        record_funnel_into(opts, &log);
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(*p.model);
        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_FALSE(sol.infeasibility_certified);
        EXPECT_EQ(sol.counters.restoration_iters, 0);
        EXPECT_TRUE(log.resumes.empty()) << "the funnel must not be re-based on a clean solve";
        EXPECT_EQ(log.resets.size(), 1u);
        EXPECT_EQ(
            std::count_if(sol.history.begin(), sol.history.end(),
                          [](const SqpIterate &h) { return h.verdict == StepVerdict::kRestore; }),
            0);
    }
}

// ONE RESTORATION PER SOLVE, and what the second request reports. The fixture
// is test_sqp_solver.cpp's SecondOrderInfeasibleModel geometry with the
// ORIGINAL unbounded-along-the-manifold objective -- i.e. this file's
// StalledValleyModel with the +5 x1^2 term removed -- which is precisely the
// case the cap exists for: restoration recovers a feasible point, the
// optimality phase walks back out to a second stall, and a driver without a cap
// would alternate. See sqp_solver.h's ONE RESTORATION PER SOLVE paragraph for
// why the cap is set at one and what would relax it.
namespace {
class RunawayValleyModel : public StalledValleyModel {
  public:
    double eval_f(const Vec &x) const override {
        return -10.0 * x(1) - 0.01 * x(0) + 0.0005 * (x(0) * x(0) + x(1) * x(1));
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 0.001 * x(0) - 0.01, 0.001 * x(1) - 10.0;
        return g;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 0.001 + lambda_e(0) * 2000.0;
        h.insert(1, 1) = obj_scale * 0.001;
        h.makeCompressed();
        return h;
    }
};
} // namespace

TEST(SqpDriverRestoration, SecondRequestIsCappedAndReported) {
    RunawayValleyModel model;
    SqpOptions opts;
    opts.tr_init = 8.0;
    opts.max_iter = 60;
    FunnelLog log;
    record_funnel_into(opts, &log);
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    // The first request is serviced and the solve genuinely resumes...
    EXPECT_EQ(log.resumes.size(), 1u);
    EXPECT_GE(sol.counters.restoration_iters, 1);
    EXPECT_GT(sol.counters.steps_accepted, 1) << "the resumed loop made real progress";
    // ...and the second is reported rather than serviced -- as kNumericalError,
    // never kInfeasible (M7 W2 ladder, L1; re-pinned from kInfeasible: this
    // fixture's NLP is feasible, so kInfeasible was the wrong answer).
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
    EXPECT_FALSE(sol.infeasibility_certified)
        << "the cap exit makes NO claim about the model -- and this fixture's NLP is in fact "
           "feasible, so a certificate here would be a wrong answer, not a conservative one";
    ASSERT_FALSE(sol.history.empty());
    EXPECT_EQ(sol.history.back().verdict, StepVerdict::kRestore);
    // The counters CANNOT tell this exit from the certified one -- both
    // report restoration_iters > 0. Nor is the last row's verdict a reliable
    // discriminator in general: it is kRestore here (this request came from
    // the funnel's own signature, as does a certified exit reached the same
    // way), but a certified exit reached via the RADIUS FLOOR route instead
    // leaves kReject on its last row (sqp_solver.h's RESTORATION PHASE note
    // has the corrected decision table; SqpDriverRadius.
    // FloorRaisesTheRestorationRequest exercises that route). So neither
    // field says which exit this is -- the status and the flag do
    // (sqp_solver_types.h's SqpResult::infeasibility_certified).
    EXPECT_GE(sol.counters.restoration_iters, 1);
    EXPECT_LT(sol.counters.major_iters, opts.max_iter)
        << "the cap must END the solve, not let it grind to the budget";
    ::testing::Test::RecordProperty("capped", fmt::format("majors={} restoration={} f={:.10g}",
                                                          sol.counters.major_iters,
                                                          sol.counters.restoration_iters, sol.f));
}

// KLV ALGORITHM 2'S ||d|| = 0 SHORT-CIRCUIT (sqp_solver.h's kZeroStepScale).
// The cleanest instance in the project is the restoration phase's OWN
// feasibility problem entered at an infeasible stationary point, where the
// start point is already the solution: the subproblem returns p = 0 (measured
// at 1.3e-23), and the funnel -- comparing a predicted and an actual decrease
// that are both rounding noise -- rejects it. Without the short-circuit the
// multipliers that make the point stationary are discarded on every trial and
// the solve rejects 64 times to the radius floor (measured before the fix);
// with it the QP's prices are adopted and the next convergence test passes.
//
// PINNED ON THE MECHANISM, not only the outcome: the strategy is never asked
// to judge the zero step (Algorithm 2 tests ||d|| = 0 AHEAD of the funnel
// condition), so a recording funnel sees zero judge calls before the accepted
// row.
TEST(SqpDriverRestoration, ZeroStepIsAcceptedAsAKktPoint) {
    BoxBlockedEqualityModel model;
    Vec x(2);
    x << 1.0, 0.0; // the point the outer solve's elastic tier leaves it at
    const NlpEval ev = eval_nlp(model, x);
    const RestorationModel feasibility(model, x, ev);

    SqpOptions opts;
    FunnelLog log;
    record_funnel_into(opts, &log);
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(feasibility, feasibility.start_point());

    EXPECT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_EQ(sol.counters.major_iters, 1) << "one subproblem to price the multipliers, no more";
    EXPECT_EQ(sol.counters.rejected_steps, 0);
    ASSERT_GE(sol.history.size(), 2u);
    EXPECT_EQ(sol.history[0].verdict, StepVerdict::kAcceptF);
    EXPECT_LT(sol.history[0].step_norm, 1e-12);
    EXPECT_EQ(log.judged, 0) << "the strategy is bypassed, per Algorithm 2's ordering";
    // The multipliers the accepted zero step delivered are what certify it:
    // stationarity in the slack sp forces lambda_e = -1 = sign(cE) at x0 = 1.
    EXPECT_NEAR(sol.lambda_e(0), -1.0, 1e-9);
    EXPECT_LE(sol.history.back().stationarity, opts.kkt_tol);
}

// THE OUTER SOLVE OF THE SAME MODEL, end to end: a genuinely infeasible NLP
// whose linearization is inconsistent at every radius now CERTIFIES instead of
// reporting the elastic tier's exhaustion verbatim. This is
// SqpDriverElastic.RhoEscalationIsBoundedAndSignals' successor claim -- that
// test pins the tier's own signature, this one pins what the signature now
// buys.
TEST(SqpDriverRestoration, ExhaustedElasticTierEntersRestoration) {
    BoxBlockedEqualityModel model;
    SqpOptions opts;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
    EXPECT_TRUE(sol.infeasibility_certified);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_GE(sol.counters.restoration_iters, 1);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_TRUE(sol.history.back().elastic_applied)
        << "the trigger here is Algorithm 5's, not the funnel's signature";
    EXPECT_EQ(sol.history.back().verdict, StepVerdict::kRestore);

    // x0 = 5 is unreachable inside [0,1], and the closest the box allows is
    // x0 = 1 with h = 4. The certificate: grad cE = (1,0) and lambda_e = -1 =
    // sign(cE), balanced by the bound price z = (-1, 0) at the ACTIVE upper
    // bound -- a stationary point of h whose stationarity lives entirely in
    // the normal cone of the box.
    EXPECT_NEAR(sol.x(0), 1.0, 1e-9);
    const SubgradientCertificate c = check_certificate(model, sol, opts.feas_tol);
    EXPECT_NEAR(c.h, 4.0, 1e-9);
    EXPECT_LE(c.stationarity, opts.kkt_tol);
    EXPECT_LE(c.selector, 1e-9);
    EXPECT_LE(c.sign_error, 1e-9);
    EXPECT_NEAR(sol.z(0), -1.0, 1e-9) << "at an ACTIVE UPPER bound the price must be <= 0";
}

// THE SHARED DECLARED DIAGNOSTICS OF THAT EXIT DESCRIBE THE RETURNED DUALS
// (M6 W5 T8.4 fix1, astra item 2(ii)). This is the fixture where the
// distinction bites. At x = (1, 0) the declared objective gradient is (1, 0)
// and the returned bound price is the CERTIFICATE's z = (-1, 0), so the
// declared stationarity |grad L - z|inf is 1 -- an infeasible point is not a
// stationary point of the declared problem, and the result must not say it is.
// T8.4's scaled arm computed the shared four from a RE-MEASURED bound price
// (grad L at the active bound, i.e. 0) while RETURNING the certificate's, and
// so reported 0 on the scaled path and 1 on the unscaled one for the same
// solve of the same model.
TEST(SqpDriverRestoration, TheSharedDiagnosticsOfACertifiedExitUseTheReturnedPrices) {
    BoxBlockedEqualityModel model;
    SqpOptions opts;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
    ASSERT_TRUE(sol.infeasibility_certified);
    ASSERT_NEAR(sol.x(0), 1.0, 1e-9);
    ASSERT_NEAR(sol.z(0), -1.0, 1e-9);

    // INDEPENDENTLY, from the model itself at exactly the vectors this call
    // returns. A test may evaluate; a solve may not.
    const NlpEval ev = eval_nlp(model, sol.x);
    const DeclaredDiagnostics d =
        compute_declared_diagnostics(sol.x, sol.lambda_e, sol.lambda_i, sol.z, ev.grad, ev.Je,
                                     ev.Ji, ev.ce, ev.ci, model.lower(), model.upper(), {});
    EXPECT_NEAR(d.stationarity, sol.stationarity, 1e-9);
    EXPECT_NEAR(d.feasibility_e, sol.feasibility_e, 1e-9);
    EXPECT_NEAR(d.feasibility_i, sol.feasibility_i, 1e-9);
    EXPECT_NEAR(d.complementarity, sol.complementarity, 1e-9);
    // And the numbers themselves, hand-derived above: the price the caller gets
    // back does NOT close the declared stationarity at this point, and it must
    // not be made to look as though it does.
    EXPECT_NEAR(sol.stationarity, 1.0, 1e-6);
    EXPECT_NEAR(sol.feasibility_e, 4.0, 1e-6);
}

// THE SAME EXHAUSTION IN THE OTHER TWO MODES (W2 T4, owner ruling Q-O3): the
// route and the certificate do not depend on which kernel solved the
// subproblems -- kIpm reaches it through the certified fallback's own ladder.
TEST(SqpDriverRestoration, ExhaustedElasticTierEntersRestorationUnderEveryMode) {
    for (const QpMode mode : {QpMode::kSsn, QpMode::kIpm}) {
        SCOPED_TRACE(mode == QpMode::kSsn ? "ssn" : "ipm");
        BoxBlockedEqualityModel model;
        SqpOptions opts;
        opts.qp_mode = mode;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);

        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified);
        EXPECT_GE(sol.counters.restoration_iters, 1);
        ASSERT_FALSE(sol.history.empty());
        EXPECT_TRUE(sol.history.back().elastic_applied);
        EXPECT_EQ(sol.history.back().verdict, StepVerdict::kRestore);
        EXPECT_NEAR(sol.x(0), 1.0, 1e-9);
        const SubgradientCertificate c = check_certificate(model, sol, opts.feas_tol);
        EXPECT_NEAR(c.h, 4.0, 1e-9);
        EXPECT_LE(c.stationarity, opts.kkt_tol);
        EXPECT_LE(c.selector, 1e-9);
        EXPECT_LE(c.sign_error, 1e-9);
        EXPECT_NEAR(sol.z(0), -1.0, 1e-9);
    }
}

// =========================================================================
// SqpDriverRadius -- the two radius rules Task 9 owns.
// =========================================================================

// THE RADIUS FLOOR IS A TRIGGER, NOT A CLAMP -- KLV Algorithm 4's "alpha <
// alpha_min" in trust-region form. The fixture is the infeasible circle/line
// model from an OFF-DIAGONAL start, where the funnel's own signature never
// fires (its conjuncts fail: the trial usually does reduce h, so (d) is false)
// and the solve instead grinds the radius down. When the next halving would
// cross tr_min the driver enters restoration, which then certifies -- so the
// floor is what turns an otherwise budget-bound grind into a verdict.
//
// THREE THINGS ARE ASSERTED AND THEY ARE THE WHOLE CLAIM:
//   (1) the STRATEGY's last verdict is kReject, i.e. the funnel did NOT ask
//       for restoration -- the floor escalated an ordinary rejection;
//   (2) no radius ever goes below tr_min, and the last one is the smallest
//       value above it, so the floor was reached exactly and not overshot;
//   (3) tr_min is a REAL KNOB: raising it by six orders of magnitude reaches
//       the same verdict at the same point in 20 fewer majors.
// RE-PREMISED AND RENAMED AT M7 W2 E4 FIX ROUND 2 (ruling E4-19's class; old name
// `FloorRaisesTheRestorationRequest`), and this is the SECOND time this fixture's
// trigger has moved out from under it -- the U0 note below records the first.
//
// WHAT HAPPENS NOW, MEASURED: on this model the E4 driver's first step is ACCEPTED
// (verdict kAccept at tr_radius 1, h 1) and the elastic tier's exhaustion raises
// the restoration request on the very next major (kRestore, tr_radius still 1). The
// radius NEVER SHRINKS, so no floor value can be reached, and route (3) -- the
// floor as a trigger -- is not exercised on this model at all. The U0 remedy (move
// the floor above the radius at which route (2) fires) has no analogue here: there
// is no grind to place a floor inside.
//
// WHAT IS ASSERTED NOW: that the solve reaches the SAME verdict, the same
// certificate and the same point by route (2), that the funnel still never asks,
// and that the radius never crosses the floor -- plus, explicitly, that the floor
// was NOT the trigger, so a return of the grind fails this test and route (3)'s
// read-out can be reinstated from the history above.
//
// REGISTERED (E4 fix round 2): route (3) -- the radius floor as a restoration
// trigger -- has NO fixture on the E4 engine. It needs a model whose trial is
// rejected often enough to grind the radius down; this one no longer is. The rule
// itself is unchanged code.
TEST(SqpDriverRadius, TheElasticExhaustionPreemptsTheRadiusFloorOnThisModel) {
    struct Run {
        double tr_min;
        Index majors = 0;
        double last_radius = 0.0;
    };
    // FLOOR VALUES MOVED AT U0 (unified flags, 2026-08-16, declared): 1e-10
    // -> 1e-5 and 1e-4 -> 1e-2. Under -ffast-math the elastic tier's
    // exhaustion (restoration route (2), KLV Algorithm 5's authoritative
    // trigger) fires on this fixture at Delta = 2^-18 ~ 3.8e-6 -- sixteen
    // halvings before a 1e-10 floor -- so the original floor could never be
    // reached in Release and the test was measuring the wrong trigger. Both
    // floors now sit ABOVE that radius, so the floor pre-empts route (2) in
    // BOTH configs and the test's subject -- route (3) exists, is reached
    // exactly, and is a real knob -- is back to being what is measured. The
    // knob claim (3) below is unchanged in form: the higher floor still
    // saves exactly the halvings between the two floors.
    std::vector<Run> runs{{1e-5}, {1e-2}};

    for (Run &run : runs) {
        SCOPED_TRACE(fmt::format("tr_min {:g}", run.tr_min));
        InfeasibleCircleLineModel model(1.0, 0.0);
        SqpOptions opts;
        opts.tr_min = run.tr_min;
        FunnelLog log;
        record_funnel_into(opts, &log);
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);
        run.majors = sol.counters.major_iters;
        ASSERT_FALSE(sol.history.empty());
        run.last_radius = sol.history.back().tr_radius;

        // (1) THE FUNNEL DID NOT ASK. Its signature never fired anywhere in
        // the solve, so every kRestore on this path came from the driver.
        EXPECT_EQ(std::count(log.verdict.begin(), log.verdict.end(), StepVerdict::kRestore), 0)
            << "if the signature fires here this test is measuring the wrong trigger";
        ASSERT_FALSE(log.verdict.empty());
        // RE-DERIVED: kReject -> kAcceptH. The strategy's last verdict is an h-type ACCEPT
        // now, because the step is taken and route (2) raises the request on the
        // next major; the floor never escalates an ordinary rejection here.
        EXPECT_EQ(log.verdict.back(), StepVerdict::kAcceptH);
        EXPECT_TRUE(log.resumes.empty()) << "this fixture is infeasible; there is nothing to "
                                            "resume to";

        // (2) THE FLOOR HELD, AND WAS REACHED. Every radius is >= tr_min, and
        // the last one is within one halving of it -- which is exactly the
        // condition the driver escalates on (the NEXT shrink would cross).
        for (const SqpIterate &h : sol.history) {
            EXPECT_GE(h.tr_radius, opts.tr_min) << "trial " << h.trial;
        }
        // RE-DERIVED, AND THE SENSE IS REVERSED: the request is raised WELL ABOVE the
        // floor, because it is route (2)'s and not route (3)'s. Asserted as the
        // negation so that a grind back down to the floor fails here.
        EXPECT_GE(run.last_radius * 0.5, opts.tr_min)
            << "the radius reached the floor again: route (3) is back and this fixture's "
               "original read-out can be reinstated";
        EXPECT_EQ(run.last_radius, 1.0) << "the radius never shrank at all";

        // And the phase ran, and produced the same verdict and the same
        // certificate as the elastic tier's route does (InfeasibleNlpCertifies).
        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified);
        EXPECT_GE(sol.counters.restoration_iters, 1);
        const SubgradientCertificate c = check_certificate(model, sol, opts.feas_tol);
        EXPECT_NEAR(c.h, 2.0 - std::sqrt(2.0), 1e-6);
        EXPECT_LE(c.stationarity, opts.kkt_tol);
        EXPECT_LE(c.selector, 1e-9);
    }

    // (3) tr_min IS INERT ON THIS MODEL NOW, which is the same statement as "the
    // floor is not the trigger" read from the knob's end: three orders of magnitude
    // of tr_min buy nothing because no halving ever happens. RE-DERIVED from
    // `LT`/`GT` (20 majors saved at the higher floor) to equality, so that a knob
    // that starts biting again fails here.
    EXPECT_EQ(runs[1].majors, runs[0].majors) << "tr_min moved the trajectory: the floor is a "
                                                 "trigger again on this model";
    EXPECT_EQ(runs[1].last_radius, runs[0].last_radius);
    ::testing::Test::RecordProperty(
        "floor_knob",
        fmt::format("tr_min=1e-10 -> {} majors (last delta {:.3g}); tr_min=1e-4 -> "
                    "{} majors (last delta {:.3g})",
                    runs[0].majors, runs[0].last_radius, runs[1].majors, runs[1].last_radius));
}

// THE +inf DEGENERACY, fixed and measured against what it replaced. Task 6
// carried this forward as a known defect: at tr_init = +inf a rejection could
// not shrink the radius (inf/2 is inf), so a solve that ever rejected rejected
// forever. MEASURED ON THIS FIXTURE SET before the fix (Task 9's own
// re-measurement, tr_max = 10, max_iter = 60):
//     HS1  kMaxIter, 59 of 60 trials rejected, f = 8.97 against f* = 0
//     HS5  kMaxIter, 59 of 60 trials rejected, f = -1.02 against f* = -1.913
//     HS7  kNumericalError after 2 majors
// With the first shrink landing on tr_max, all three converge to their cited
// f*. The three problems that never reject (HS3/HS6/HS76) are unaffected and
// are included to pin that: their radius stays +inf for the whole solve, which
// is what "no trust region" is supposed to mean.
TEST(SqpDriverRadius, InfiniteInitialRadiusShrinksToTrMax) {
    struct Case {
        int number;
        bool rejects;
    };
    for (const Case c : {Case{1, true}, Case{3, false}, Case{5, true}, Case{6, false},
                         Case{7, true}, Case{76, false}}) {
        SCOPED_TRACE(fmt::format("HS{}", c.number));
        const HsProblem p = make_hs(c.number);
        SqpOptions opts;
        opts.tr_init = std::numeric_limits<double>::infinity();
        opts.tr_max = 10.0;
        opts.max_iter = 60;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(*p.model);

        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_NEAR(sol.f, p.f_star, 1e-6);
        EXPECT_EQ(sol.counters.rejected_steps > 0, c.rejects);

        const bool ever_finite =
            std::any_of(sol.history.begin(), sol.history.end(),
                        [](const SqpIterate &h) { return std::isfinite(h.tr_radius); });
        EXPECT_EQ(ever_finite, c.rejects)
            << "a solve that never rejects must never materialize a radius";
        if (c.rejects) {
            // The FIRST finite radius is tr_max itself, never tr_max/2 and
            // never some other large constant.
            const auto it =
                std::find_if(sol.history.begin(), sol.history.end(),
                             [](const SqpIterate &h) { return std::isfinite(h.tr_radius); });
            ASSERT_NE(it, sol.history.end());
            EXPECT_DOUBLE_EQ(it->tr_radius, opts.tr_max);
        }
    }
}

// The floor joins the option validation, on the same terms as the rest of the
// radius triple: positive, and not above either end of the range it floors.
TEST(SqpDriverRadius, RejectsUnusableFloors) {
    {
        SqpOptions opts;
        opts.tr_min = 0.0;
        EXPECT_THROW(SqpSolver{opts}, std::invalid_argument);
    }
    {
        SqpOptions opts;
        opts.tr_min = -1.0;
        EXPECT_THROW(SqpSolver{opts}, std::invalid_argument);
    }
    {
        SqpOptions opts;
        opts.tr_min = std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(SqpSolver{opts}, std::invalid_argument);
    }
    {
        SqpOptions opts; // tr_init = 1.0
        opts.tr_min = 2.0;
        EXPECT_THROW(SqpSolver{opts}, std::invalid_argument);
    }
    {
        SqpOptions opts; // a +inf tr_init still has to clear tr_max
        opts.tr_init = std::numeric_limits<double>::infinity();
        opts.tr_max = 1.0;
        opts.tr_min = 2.0;
        EXPECT_THROW(SqpSolver{opts}, std::invalid_argument);
    }
    { // and the default triple is admissible
        EXPECT_NO_THROW(SqpSolver{SqpOptions{}});
    }
}

// SqpDriverRestorationSeed -- W2 T4: the phase starts at a request site's
// CANDIDATE only when its MEASURED violation is below the entry's. Every
// "taken" pin here has a "refused" sibling on the same site (non-vacuity).

namespace {
// Counts model calls, so the decision-driven eval counters are read beside an
// independent count -- test_sqp_solver.cpp's SqpDriverEvalEconomics
// convention, and the only thing that caught a desync last time.
class SeedCountingModel final : public NlpModel {
  public:
    explicit SeedCountingModel(double s0, double s1) : inner_(s0, s1) {}

    Index n() const override { return inner_.n(); }
    Index me() const override { return inner_.me(); }
    Index mi() const override { return inner_.mi(); }
    double eval_f(const Vec &x) const override { return inner_.eval_f(x); }
    Vec eval_grad(const Vec &x) const override {
        ++n_grad;
        return inner_.eval_grad(x);
    }
    Vec eval_ce(const Vec &x) const override {
        ++n_ce;
        return inner_.eval_ce(x);
    }
    Vec eval_ci(const Vec &x) const override { return inner_.eval_ci(x); }
    SpMatRM eval_hess(const Vec &x, double o, const Vec &le, const Vec &li) const override {
        return inner_.eval_hess(x, o, le, li);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        ++n_jac_e;
        return inner_.eval_jac_e(x);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return inner_.eval_jac_i(x);
    }
    const Vec &lower() const override { return inner_.lower(); }
    const Vec &upper() const override { return inner_.upper(); }
    Vec start_point() const override { return inner_.start_point(); }

    mutable Index n_grad = 0, n_ce = 0, n_jac_e = 0;

  private:
    InfeasibleCircleLineModel inner_;
};

// The circle/line fixture with cE POISONED to NaN away from the start, so
// that every candidate a request site can offer is a point the model cannot
// measure.
class NanAwayFromStartModel final : public InfeasibleCircleLineModel {
  public:
    Vec eval_ce(const Vec &x) const override {
        Vec c = InfeasibleCircleLineModel::eval_ce(x);
        if ((x - start_point()).lpNorm<Eigen::Infinity>() > 1e-12) {
            c.setConstant(std::numeric_limits<double>::quiet_NaN());
        }
        return c;
    }
};

// The circle/line fixture with the JACOBIAN (not the values) poisoned to NaN
// at the k-th DISTINCT point eval_jac_e is queried at -- ordinal-keyed, no
// driver knowledge: a point re-queried (the values-then-full upgrade) counts once.
class JacobianPoisonedAtOrdinalModel final : public InfeasibleCircleLineModel {
  public:
    explicit JacobianPoisonedAtOrdinalModel(Index k) : InfeasibleCircleLineModel(2.0, 2.0), k_(k) {}

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j = InfeasibleCircleLineModel::eval_jac_e(x);
        Index ord = -1;
        for (Index i = 0; i < static_cast<Index>(points_.size()); ++i) {
            if (points_[i].size() == x.size() && points_[i] == x) {
                ord = i + 1;
                break;
            }
        }
        if (ord < 0) {
            points_.push_back(x);
            ord = static_cast<Index>(points_.size());
        }
        if (ord == k_) {
            j.valuePtr()[0] = std::numeric_limits<double>::quiet_NaN();
            ++poisoned;
        }
        return j;
    }

    mutable Index poisoned = 0;

  private:
    Index k_;
    mutable std::vector<Vec> points_;
};

// ROW FACTORS THAT ACTUALLY SPREAD, inside a box: row 0's Jacobian is 1e4 and
// rows 1-2's are 90, so W0.2 scales row 0 by 1e-2 and leaves the others at 1
// (measured row_min 0.01, row_max 1) -- min 1/2||x||^2 on 0 <= x <= 1.
class ScaledRowSpreadModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 3; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(3);
        c << kSteep * (x(0) - 5.0), kFlat * (x(0) + 0.001 * x(1)), kFlat * (x(0) - 0.001 * x(1));
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(3, 2);
        j.insert(0, 0) = kSteep;
        j.insert(1, 0) = kFlat;
        j.insert(1, 1) = 0.001 * kFlat;
        j.insert(2, 0) = kFlat;
        j.insert(2, 1) = -0.001 * kFlat;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Zero(2);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Ones(2);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 0.5, 0.5;
        return x;
    }

    static constexpr double kSteep = 1.0e4;
    static constexpr double kFlat = 90.0;

  protected:
    ScaledRowSpreadModel() = default;
};

// The same model, watching every point the solve evaluates it at -- which is
// how the restoration START is observed from outside the driver, the seeded
// candidate included.
class BoxWatchingSpreadModel final : public ScaledRowSpreadModel {
  public:
    BoxWatchingSpreadModel() = default;

    double eval_f(const Vec &x) const override {
        this->watch(x);
        return ScaledRowSpreadModel::eval_f(x);
    }
    Vec eval_ce(const Vec &x) const override {
        this->watch(x);
        return ScaledRowSpreadModel::eval_ce(x);
    }

    mutable double worst_excursion = 0.0;
    mutable Index points = 0;

  private:
    void watch(const Vec &x) const {
        ++points;
        const double out_lo = (lower() - x).maxCoeff();
        const double out_hi = (x - upper()).maxCoeff();
        worst_excursion = std::max(worst_excursion, std::max(out_lo, out_hi));
    }
};

Index seeded_rows(const SqpResult &sol) {
    Index k = 0;
    for (const SqpIterate &row : sol.history) {
        k += row.restoration_seed_used ? 1 : 0;
    }
    return k;
}

const SqpIterate *seeded_row(const SqpResult &sol) {
    for (const SqpIterate &row : sol.history) {
        if (row.restoration_seed_used) {
            return &row;
        }
    }
    return nullptr;
}

const QpMode kSeedModes[3] = {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm};
const char *seed_mode_name(QpMode m) {
    return m == QpMode::kWalk ? "walk" : (m == QpMode::kSsn ? "ssn" : "ipm");
}
} // namespace

// ===========================================================================
// THE SEED FAMILY'S RE-PREMISE (M7 W2 E4 fix round 2, ruling E4-21(b))
// ===========================================================================
//
// SEVEN fixtures in this file and one in the proximity group read
// `seeded_rows(sol) == 0` where they pinned 1. It is ONE ROUTE CHANGE, at one
// line of `src/drivers/sqp_solver.cpp`: the seed guard takes an offered candidate
// only when it MEASURES BETTER,
//
//     if (constraint_violation_l1(probe) < h_entry) { ... take = true; }
//
// and the margin it decides on is a KNIFE EDGE on every one of these fixtures.
// Their own comments record it: `TheTakenCandidateIsRecordedOnTheRequestingRow`
// measured h = 1.00143238 for the candidate against 1.00143320 for the entry
// (8.2e-07 RELATIVE), `TheRadiusFloorSeedsFromTheRejectedTrial` 1.00053814
// against 1.00053914 (1.0e-06), and `TheGuardAndThePhaseRunOnTheCallerScale`
// 49999.999999992513 against 50000 (1.5e-13). M7 W2 E4 changes every QP answer in
// the solve -- the proximal EQP, its unregularized polish, the terminal polish --
// so the trial and ladder steps these candidates ARE differ, and on margins that
// small the comparison goes the other way.
//
// THE GUARD IS NOT AT FAULT AND NOTHING IS LOST: the request is still raised, the
// restoration phase still RUNS from the requesting iterate
// (`restoration_iters >= 1`, asserted below where the fixture asserted it before),
// and every one of these fixtures still reaches ITS OWN ANSWER -- kInfeasible,
// certified, at the analytic point -- which is what the driver is judged on. What
// is gone is a candidate measuring better on a knife edge, and a fixture cannot
// hold a premise the arithmetic no longer supports.
//
// SO EACH IS RE-PREMISED AS A DECLARED EVENT, with the seed's own coverage kept
// wherever it does not need a TAKEN candidate: the guard's refusal arms
// (`ANonFiniteEvaluationAtTheCandidateIsRefused`, and the Jacobian screen's
// M7-kill half) are unmoved, and the tests that needed a take are RENAMED to say
// that the candidate is offered and refused.
//   TheTakenCandidateIsRecordedOnTheRequestingRow
//     -> TheOfferedCandidateNoLongerMeasuresBetterAndIsNotTaken
//   TheSeedIsNotGatedOnTheQpMode
//     -> TheSeedOfferIsNotGatedOnTheQpModeAndIsRefusedInEveryOne
//   TheExhaustedLadderSiteSeedsFromItsOwnStep
//     -> TheExhaustedLadderSiteOffersItsOwnStepAndTheGuardRefusesIt
//   TheRadiusFloorSeedsFromTheRejectedTrial
//     -> TheRadiusFloorOffersTheRejectedTrialAndTheGuardRefusesIt
//   TheGuardAndThePhaseRunOnTheCallerScale  (name unchanged: it still does)
//   TheSeedsEvaluationIsCountedExactlyOnce  (name unchanged: counts re-derived)
//   AJacobianPoisonedCandidateIsRefusedNotTaken (name unchanged: still true, and
//     now at the h screen rather than at the Jacobian one)
//   SqpDriverRestorationProximity.TheSeedGuardFiresAndTheStartObjectiveIsHPlusP
//
// M7 W2 R2, THE ROUTE WRITTEN OUT. On this model the walk fails on the way to the
// request (kMaxIter / kNumericalError), and the R2 default judges the failed
// subproblem's own step and often takes it, which moves WHERE the request is
// raised and what the knife-edge candidate measures. The fixtures above pin the
// ROUTE, so each sets `qp_failure_step = false` and keeps every pinned value; the
// default's own reading on the same model is
// SqpDriverRestorationSeed.TheFailedStepKeepsTheCertifiedAnswer (the same certified
// kInfeasible at the same analytic point).

// THE R2 DEFAULT ON THE FAMILY'S KNIFE-EDGE MODEL: whatever route the failed
// subproblems' own steps take it by, the answer is the one the family pins --
// kInfeasible, certified, at the analytic point -- on both walks.
TEST(SqpDriverRestorationSeed, TheFailedStepKeepsTheCertifiedAnswer) {
    for (const bool independent : {false, true}) {
        SCOPED_TRACE(independent ? "independent working set" : "pre-E2 walk");
        InfeasibleCircleLineModel model(2.0, 1.999999);
        SqpOptions opts;
        opts.qp.independent_working_set = independent;
        opts.max_iter = 200;
        ASSERT_TRUE(opts.qp_failure_step);
        const SqpResult sol = SqpSolver(opts).solve(model);
        EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
        EXPECT_TRUE(sol.infeasibility_certified);
        const double t = 1.0 / std::sqrt(2.0);
        EXPECT_NEAR(sol.x(0), t, 1e-5);
        EXPECT_NEAR(sol.x(1), t, 1e-5);
        EXPECT_EQ(sol.counters.nonfinite_subproblems, 0);
    }
}

// A CANDIDATE THAT MEASURES BETTER IS TAKEN, and the row that RAISED the
// request records it. Measured at the exhaustion (Release, MKL, clang++): the
// ladder's own step reads h = 1.00143238 against the entry's 1.00143320.
TEST(SqpDriverRestorationSeed, TheOfferedCandidateNoLongerMeasuresBetterAndIsNotTaken) {
    InfeasibleCircleLineModel model(2.0, 1.999999);
    SqpOptions opts;
    // M7 W2 E2 (S7): the taken candidate belongs to the pre-E2 walk (Release takes none with it
    // on).
    opts.qp.independent_working_set = false;
    opts.max_iter = 200;
    opts.qp_failure_step = false; // M7 W2 R2: the pre-R2 route (see the family's note)
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    // RE-PREMISED (see the family note above): 1 -> 0, the candidate is offered and
    // the guard refuses it because it no longer measures better.
    ASSERT_EQ(0, seeded_rows(sol)) << "the candidate measures better again: the guard took it";
    EXPECT_EQ(nullptr, seeded_row(sol));
    EXPECT_GE(sol.counters.restoration_iters, 1)
        << "the phase RAN from the requesting iterate -- the half that never needed the seed";
    // WHICH SITE offered it is build-arithmetic-dependent on this knife-edge
    // fixture -- Release reaches the ladder's exhaustion, Debug the radius floor
    // -- so the site is pinned elsewhere, on fixtures that agree in both.

    // AND THE ANSWER IS UNCHANGED by the better start.
    EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
    EXPECT_TRUE(sol.infeasibility_certified);
    const double t = 1.0 / std::sqrt(2.0);
    EXPECT_NEAR(sol.x(0), t, 1e-5);
    EXPECT_NEAR(sol.x(1), t, 1e-5);
}

// AND THE SEED IS NOT GATED ON THE KERNEL (owner ruling Q-O3). The fixture is
// the refactorize arm of InfeasibleNlpCertifies, MEASURED to take a trial-site
// candidate in all three modes in BOTH configs -- so EQ(1) is falsifiable.
TEST(SqpDriverRestorationSeed, TheSeedOfferIsNotGatedOnTheQpModeAndIsRefusedInEveryOne) {
    for (const QpMode mode : kSeedModes) {
        SCOPED_TRACE(seed_mode_name(mode));
        InfeasibleCircleLineModel model;
        SqpOptions opts;
        // M7 W2 E2: the entry repair releases a seed pin dependent on the
        // equalities on this knife-edge model, and the candidate premise below
        // belongs to the pre-E2 walk's trajectory (answer unchanged either way).
        opts.qp.independent_working_set = false;
        opts.max_iter = 200;
        opts.qp.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
        opts.qp_mode = mode;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);

        // RE-PREMISED (the family note): 1 -> 0 in all three modes. The mutation
        // evidence this pin rested on (a `qp_mode == kWalk` gate leaves the OTHER
        // two kernels at 0, docs/notes/data/2026-09-m6-w2-t4-r4/) is no longer
        // distinguishable here, because no mode takes a candidate. What IS still
        // asserted per mode, and is the claim that matters for the ruling behind
        // it (owner Q-O3), is that the OFFER and the ANSWER are mode-independent:
        // every mode reaches the same certified point below.
        EXPECT_EQ(0, seeded_rows(sol)) << "a mode took the seed: the margins have moved back";
        EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
        EXPECT_TRUE(sol.infeasibility_certified);
        const double t = 1.0 / std::sqrt(2.0);
        EXPECT_NEAR(sol.x(0), t, 1e-5);
        EXPECT_NEAR(sol.x(1), t, 1e-5);
    }
}

// M7 W2 E2 (settler S7): the seed fixtures on this model keep the pre-E2 walk
// for their candidate premise; this one runs the SAME configuration with the
// independent working set ON and asserts the ANSWER, so the on-path is
// covered on the circle-line model too.
TEST(SqpDriverRestorationSeed, TheIndependentWorkingSetReachesTheSameCertifiedAnswer) {
    for (const QpMode mode : kSeedModes) {
        SCOPED_TRACE(seed_mode_name(mode));
        InfeasibleCircleLineModel model;
        SqpOptions opts;
        ASSERT_TRUE(opts.qp.independent_working_set);
        opts.max_iter = 200;
        opts.qp.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
        opts.qp_mode = mode;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);
        EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
        EXPECT_TRUE(sol.infeasibility_certified);
        const double t = 1.0 / std::sqrt(2.0);
        EXPECT_NEAR(sol.x(0), t, 1e-5);
        EXPECT_NEAR(sol.x(1), t, 1e-5);
    }
}

// THE EXHAUSTED-LADDER SITE'S OWN TAKEN PIN, site-specific and config-stable:
// the requesting row carries the ladder's kRestore verdict, so this is the
// elastic route's arm and not the radius floor's.
TEST(SqpDriverRestorationSeed, TheExhaustedLadderSiteOffersItsOwnStepAndTheGuardRefusesIt) {
    for (const QpMode mode : kSeedModes) {
        SCOPED_TRACE(seed_mode_name(mode));
        InfeasibleCircleLineModel model(2.0, 1.999999);
        SqpOptions opts;
        // M7 W2 E2 (S7): the taken seed on this knife-edge model belongs to the
        // pre-E2 walk's trajectory (the ipm mode takes none with the option on).
        opts.qp.independent_working_set = false;
        opts.max_iter = 200;
        opts.qp_mode = mode;
        // Scaling is what makes the ROUTE config-stable on this knife-edge
        // fixture (measured: kRestore in both configs with it, kReject in Debug
        // without); the seeding rule itself is the same at either site.
        opts.enable_scaling = true;
        opts.qp_failure_step = false; // M7 W2 R2: the pre-R2 route (see the family's note)
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);

        // RE-PREMISED (the family note): 1 -> 0. THE SITE IS STILL THE LADDER'S
        // EXHAUSTION, which is this fixture's actual subject, and it is asserted
        // directly off the history instead of through the seeded row.
        ASSERT_EQ(0, seeded_rows(sol));
        EXPECT_EQ(nullptr, seeded_row(sol));
        bool saw_restore = false;
        for (const SqpIterate &r : sol.history) {
            saw_restore = saw_restore || r.verdict == StepVerdict::kRestore;
        }
        EXPECT_TRUE(saw_restore) << "the ladder's exhaustion, not the floor";
        EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
        EXPECT_TRUE(sol.infeasibility_certified);
    }
}

// THE SAME MODEL, THE SAME SITE, A WORSE CANDIDATE: from the symmetric start
// the exhausted ladder's step measures h = 3.0 against the entry's 1.0.
// `reduced` is false at any exhaustion, so only the measurement can tell.
TEST(SqpDriverRestorationSeed, AWorseElasticCandidateIsRefusedOnTheSameModel) {
    InfeasibleCircleLineModel model;
    SqpOptions opts;
    // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
    opts.equality_basis = false;
    opts.max_iter = 200;
    opts.qp_failure_step = false; // M7 W2 R2: the pre-R2 route (see the family's note)
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    // M7 W2 QC-8 (declared mover, Debug only): 0 -> 1. The implied blocker changes one of
    // this model's walk QPs under Debug's rounding, and Debug's trajectory then reaches the
    // ladder's exhaustion only at major 39 (40 rows, against Release's 4) from an iterate
    // where the elastic candidate measures BETTER (h = 1.0000382 at the entry), so the guard
    // takes it. The site is still reached with an offer under kRestore and the solve still
    // ends certified kInfeasible (asserted below in both configs); the WORSE-candidate
    // refusal this fixture is named for is Release's reading. Release unchanged.
    // M7 W2 SNX (declared mover, Debug only): 1 -> 0. THE STAGNATION READING (qp_engine.h)
    // takes Debug's walk off the idle run that delayed its request, and Debug reaches the
    // ladder's exhaustion where Release does: the refusal is now both configurations' reading.
    EXPECT_EQ(0, seeded_rows(sol)) << "h(x + p_elastic) = 3 > 1 = h(x): refused";
    ASSERT_FALSE(sol.history.empty());
    EXPECT_TRUE(sol.history.back().elastic_applied) << "and the site was REACHED with an offer";
    EXPECT_EQ(StepVerdict::kRestore, sol.history.back().verdict);
    EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
    EXPECT_TRUE(sol.infeasibility_certified);
}

// A ZERO STEP IS NOT A CANDIDATE. This fixture's ladder exhausts AT the bound
// with p_elastic = 0, so x + p_elastic IS x and a "seeded" flag would be a
// lie. Every mode: no seed, and the certificate is the file's usual one.
TEST(SqpDriverRestorationSeed, AZeroElasticStepIsNotACandidate) {
    for (const QpMode mode : kSeedModes) {
        SCOPED_TRACE(seed_mode_name(mode));
        BoxBlockedEqualityModel model;
        SqpOptions opts;
        opts.qp_mode = mode;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);

        EXPECT_EQ(0, seeded_rows(sol));
        // WHAT THE ZERO-STEP CHECK BUYS is not the refusal (a candidate equal to
        // x fails the strict guard anyway) but NOT PAYING for the guard query:
        // deleting it spends a sixth full evaluation here. Measured, both configs.
        EXPECT_EQ(5, sol.counters.evals_full) << "no guard evaluation was spent on a zero step";
        EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
        EXPECT_TRUE(sol.infeasibility_certified);
        EXPECT_NEAR(sol.x(0), 1.0, 1e-9);
    }
}

// A CANDIDATE THE MODEL CANNOT MEASURE IS REFUSED: every offer here is a
// TRIAL whose bundle is non-finite, so the site refuses it on the bundle --
// before any query, and without reclassifying the trial's own.
TEST(SqpDriverRestorationSeed, ANonFiniteEvaluationAtTheCandidateIsRefused) {
    NanAwayFromStartModel model;
    SqpOptions opts;
    opts.max_iter = 60;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(0, seeded_rows(sol));
    // THE COUNT IS THE PIN: discriminating on the measured h instead of on the
    // OFFER's shape sent this NaN-h trial down the unmeasured arm and spent a
    // fresh full query there. Measured at 3 in both configs.
    EXPECT_EQ(3, sol.counters.evals_full) << "a measured non-finite trial is refused for FREE";
    EXPECT_GE(sol.counters.restoration_iters, 1) << "fixture premise: the phase RAN";
    ASSERT_FALSE(sol.history.empty());
    // THE NON-FINITE-POINT GUARD (`cand.x->allFinite()`) IS DELIBERATELY
    // UNPINNED and disowned here: no kOptimal elastic rung and no judged trial
    // can produce a non-finite POINT, so it is defensive, not reachable.
}

// A CANDIDATE WITH A NON-FINITE JACOBIAN (values finite): jacobian_values_finite
// (sqp_solver.cpp) is the only screen against it. k sweeps [1, 40) -- EXISTS a
// refused-candidate k (certifies, no seed) and NO k with M7's taken-then-died signature.
TEST(SqpDriverRestorationSeed, AJacobianPoisonedCandidateIsRefusedNotTaken) {
    // THE CANDIDATE'S OWN k is build-arithmetic-dependent in general; every
    // OTHER k is unaffected by the M7 mutation below, since the guard is read
    // only inside the candidate-seeding block enter_restoration owns.
    bool found_refused_signature = false;
    Index refused_at_k = -1;
    for (Index k = 1; k < 40; ++k) {
        SCOPED_TRACE(fmt::format("k = {}", k));
        JacobianPoisonedAtOrdinalModel model(k);
        SqpOptions opts;
        // M7 W2 E2: the entry repair releases a seed pin dependent on the
        // equalities on this knife-edge model, and the candidate premise below
        // belongs to the pre-E2 walk's trajectory (answer unchanged either way).
        opts.qp.independent_working_set = false;
        opts.max_iter = 200;
        opts.qp.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);

        const bool refused_signature =
            sol.status == SolveStatus::kInfeasible && sol.infeasibility_certified &&
            seeded_rows(sol) == 0 && sol.counters.restoration_iters >= 1 &&
            sol.counters.nonfinite_step_rollbacks == 0 && model.poisoned == 1;
        // THE KILL: mutant M7 (jacobian_values_finite dropped) reaches this
        // signature at the candidate's own k -- taken, poisoned, sub-solve dies.
        const bool m7_signature = seeded_rows(sol) == 1 && model.poisoned >= 1 &&
                                  sol.status == SolveStatus::kNumericalError &&
                                  sol.counters.restoration_iters == 0;
        EXPECT_FALSE(m7_signature) << "the Jacobian screen let a poisoned candidate through and "
                                      "the sub-solve died on it (M7's signature)";
        // RE-PREMISED (M7 W2 R2-1, 0a83158e; rulings C60/C61). BASE: k = 2..8 ended
        // numerical_error at (1.118, 1.118) -- the poisoned point was stepped from.
        // HEAD: the first major after the commit finds the poisoned Jacobian non-finite
        // and rolls the step back (nonfinite_step_rollbacks >= 1); the solve then reaches
        // the CORRECT certified kInfeasible at the analytic point (0.7071, 0.7071)
        // without a non-finite subproblem. The poisoned ordinal is still fetched
        // (poisoned >= 1), and the kill half above holds at every k.
        if (k >= 2 && k <= 8) {
            EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
            EXPECT_TRUE(sol.infeasibility_certified);
            EXPECT_NEAR(sol.x(0), std::sqrt(0.5), 1e-3);
            EXPECT_NEAR(sol.x(1), std::sqrt(0.5), 1e-3);
            EXPECT_EQ(sol.counters.nonfinite_subproblems, 0);
            EXPECT_GE(model.poisoned, 1);
            EXPECT_GE(sol.counters.nonfinite_step_rollbacks, 1);
        }
        if (refused_signature) {
            found_refused_signature = true;
            refused_at_k = k;
        }
    }
    // THE NAME IS KEPT (it appears in frozen inventories and mutant records): the Jacobian
    // screen still refuses any poisoned candidate it is shown, and no k takes one.
    // RE-PREMISED AGAIN (M7 W2 R2-1): the screen's refused-candidate signature is keyed on
    // `nonfinite_step_rollbacks == 0` too -- a poisoned ordinal that the step rollback
    // handles (k = 2..8, asserted in the loop) is not the screen refusing a candidate.
    //
    // RE-PREMISED (the family note). The refused-candidate signature required
    // `model.poisoned == 1`, i.e. that the candidate's DERIVATIVES were fetched and
    // the Jacobian screen then refused it. With the h comparison refusing the
    // candidate first, no k in [1, 40) reaches the poisoned ordinal at all, so the
    // existential premise is unreachable and is re-derived as its absence.
    //
    // THE SAFETY HALF IS UNCHANGED AND IS WHAT THIS TEST IS FOR: `m7_signature` is
    // asserted FALSE at every k inside the loop, so mutant M7 (the
    // `jacobian_values_finite` screen dropped) is still killed here -- a poisoned
    // candidate taken, the sub-solve dying on it, seeded_rows 1 and
    // restoration_iters 0 -- and the screen's own arm keeps that coverage. What the
    // change costs is the demonstration that the screen is REACHED on this model,
    // and that is stated rather than hidden.
    EXPECT_FALSE(found_refused_signature)
        << "a k in [1, 40) reached the Jacobian screen again (k = " << refused_at_k
        << "): the h guard no longer refuses the candidate first, and the existential "
           "premise above can be reinstated";
    EXPECT_EQ(-1, refused_at_k);
}

// THE RADIUS FLOOR'S SITE, whose candidate is the REJECTED TRIAL and whose
// violation is already judged (so the guard is free). Measured at the floor
// (Release): the trial reads h = 1.00053814 against the entry's 1.00053914.
TEST(SqpDriverRestorationSeed, TheOffDiagonalStartOffersTheLadderStepAndTheGuardRefusesIt) {
    InfeasibleCircleLineModel model(1.0, 0.0);
    SqpOptions opts;
    opts.tr_min = 1e-5;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    // RE-PREMISED (the family note): 1 -> 0. The FLOOR'S OWN SIGNATURE -- a
    // kReject verdict on the requesting row, where the ladder's exhaustion writes
    // kRestore -- is this fixture's subject and is asserted off the history.
    ASSERT_EQ(0, seeded_rows(sol));
    EXPECT_EQ(nullptr, seeded_row(sol));
    // RE-DERIVED WITH THE SITE (E4 fix round 2, and see
    // `TheElasticExhaustionPreemptsTheRadiusFloorOnThisModel` for the measurement):
    // on this model the radius never shrinks, so the floor is not the site any more.
    // The request is raised by the elastic tier's exhaustion, whose signature on the
    // requesting row is kRestore where the floor's is kReject, and the seed offered
    // there is the LADDER's own step rather than a rejected trial. MEASURED: the
    // whole history is kAccept then kRestore, with tr_radius 1 on both rows.
    bool saw_restore_row = false;
    bool saw_reject_row = false;
    for (const SqpIterate &r : sol.history) {
        saw_restore_row = saw_restore_row || r.verdict == StepVerdict::kRestore;
        saw_reject_row = saw_reject_row || r.verdict == StepVerdict::kReject;
    }
    EXPECT_TRUE(saw_restore_row) << "the ladder's exhaustion is the site now";
    EXPECT_FALSE(saw_reject_row)
        << "a kReject row is back: the radius floor is the site again and this fixture's "
           "original read-out can be reinstated";
    EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
    EXPECT_TRUE(sol.infeasibility_certified);
}

// THE GUARD, AND THE PHASE, ARE ON THE CALLER'S SCALE, pinned where the two
// scales genuinely differ: row 0 carries the factor 1e-2 and rows 1-2 carry 1,
// so engine-scale h and caller-scale h rank this box's two ends oppositely.
TEST(SqpDriverRestorationSeed, TheGuardAndThePhaseRunOnTheCallerScale) {
    for (const QpMode mode : kSeedModes) {
        SCOPED_TRACE(seed_mode_name(mode));
        BoxWatchingSpreadModel model;
        SqpOptions opts;
        // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
        opts.equality_basis = false;
        // M7 W2 E2 (S7): the taken seed belongs to the pre-E2 walk's trajectory
        // (the ipm mode takes none with the option on).
        opts.qp.independent_working_set = false;
        opts.max_iter = 60;
        opts.qp_mode = mode;
        opts.enable_scaling = true;
        SqpSolver driver(opts);
        const SqpResult sol = driver.solve(model);

        // THE FIXTURE'S PREMISE, asserted rather than assumed: scaling ran and
        // the row factors are NON-UNIFORM by two orders.
        ASSERT_TRUE(sol.scaling.active);
        EXPECT_NEAR(sol.scaling.row_min, 0.01, 1e-12);
        EXPECT_NEAR(sol.scaling.row_max, 1.0, 1e-12);

        // RE-PREMISED (the family note): 1 -> 0. The two scales' OPPOSITE ordering
        // of the offer was a 1.5e-13 relative margin (50000 -> 49999.999999992513
        // taken on the caller scale, 500 -> 500.00000000006105 refused on the
        // engine's), which is the thinnest of the family's edges; E4's answers put
        // both sides of it on the refusing side. What this fixture still asserts,
        // and it is the ruling's own subject, is that the guard and the phase read
        // the CALLER's scale: the phase minimizes the caller-scale h and lands at
        // x0 = 1 where the engine-scale measure would have stopped at x0 = 0.
        ASSERT_EQ(0, seeded_rows(sol));
        EXPECT_EQ(nullptr, seeded_row(sol));
        bool saw_restore_row = false;
        for (const SqpIterate &r : sol.history) {
            saw_restore_row = saw_restore_row || r.verdict == StepVerdict::kRestore;
        }
        EXPECT_TRUE(saw_restore_row);
        // AND THE TWO SCALES ORDER THE OFFER OPPOSITELY, which is what makes
        // this pin bite: measured caller 50000 -> 49999.999999992513 (taken) and
        // engine 500 -> 500.00000000006105 (refused), on a tiny x0 displacement.
        EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
        // The phase minimizes that SAME caller-scale h and lands at x0 = 1, where
        // the engine-scale measure would have stopped at x0 = 0.
        EXPECT_TRUE(sol.infeasibility_certified);
        EXPECT_NEAR(sol.x(0), 1.0, 1e-9);
        // RE-PINNED (M7 W2 SDL, declared): x(1) was pinned at 0. On the box the
        // caller-scale h is flat in x(1) wherever x(0) >= 0.001 x(1) (the two kFlat rows'
        // x(1) terms cancel), so x(1) is any point of that face; walk and ssn reached 0
        // through a restoration QP certified kOptimal with a lower pin priced -1e10, which
        // the certificate's sign half now refuses, and they land at 0.5. Asserted: on the box.
        EXPECT_GE(sol.x(1), 0.0);
        EXPECT_LE(sol.x(1), 1.0);

        // AND NO POINT THE SOLVE EVALUATED LEFT THE BOX, the seeded start
        // included. The QP carries the box (build_subproblem), so the clamp on
        // x + p_elastic is DEFENSIVE and is deliberately left unpinned.
        EXPECT_GT(model.points, 0);
        EXPECT_LE(model.worst_excursion, 1e-9) << "worst box excursion over every evaluated point";
    }
}

// THE SEED'S EVALUATION IS COUNTED ONCE. A taken TRIAL candidate is an
// UPGRADE of a query already charged values-only, so it is RECLASSIFIED
// (--values, ++full) -- both halves are pinned separately below.
TEST(SqpDriverRestorationSeed, TheSeedsEvaluationIsCountedExactlyOnce) {
    SeedCountingModel model(2.0, 2.0);
    SqpOptions opts;
    // M7 W2 E2: the entry repair releases a seed pin dependent on the
    // equalities on this knife-edge model, and the candidate premise below
    // belongs to the pre-E2 walk's trajectory (answer unchanged either way).
    opts.qp.independent_working_set = false;
    // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
    opts.equality_basis = false;
    opts.max_iter = 200;
    opts.qp.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    // THE TAKE SURVIVES HERE -- this is the one fixture of the family whose
    // candidate still measures better -- so the in-place upgrade still happens and
    // the claim is unchanged. Only the two COUNTS are re-derived at M7 W2 E4 fix
    // round 2 (a declared counter move, the trajectory being a different one):
    // `evals_full` 27 -> 28 and `evals_values` 40/41 -> 36, with the PARTITION
    // below -- the independent cross-check that no query is double-counted or lost
    // -- unchanged and still asserted.
    ASSERT_EQ(1, seeded_rows(sol)) << "fixture premise: a candidate was TAKEN";
    // `evals_values` is CONFIG-SPLIT (measured 36 Release / 35 Debug at E4 fix round 2;
    // the trajectory's length differs by one rejected trial between the configs, which
    // is what the pin this replaces already recorded as 41/40). `evals_full` is 28 in
    // both, and the PARTITION below is what the test is for.
    EXPECT_EQ(28, sol.counters.evals_full);
#ifdef NDEBUG
    EXPECT_EQ(36, sol.counters.evals_values) << "evals_values was " << sol.counters.evals_values;
#else
    EXPECT_EQ(35, sol.counters.evals_values) << "evals_values was " << sol.counters.evals_values;
#endif
    // n_grad and n_jac_e do NOT equal evals_full (the wrapper's gradient is
    // constant, so a sub-solve's full query never reaches the inner one), which
    // is why the PARTITION is the independent cross-check.
    EXPECT_EQ(sol.counters.evals_full + sol.counters.evals_values, model.n_ce)
        << "the two counters PARTITION the model queries -- a fresh eval_nlp in place of the "
           "in-place upgrade adds an eval_ce and breaks this";
    EXPECT_GT(model.n_jac_e, 0);
}

// =========================================================================
// THE RESTORATION EXCURSION RECORD (M7 W2 M2, the A0 signals page's S5).
// =========================================================================
//
// FIVE COUNTERS AND NO BEHAVIOUR: `restoration_entries` plus the four
// absolutes `(h_in, h_out, f_in, f_out)` of the LAST measured entry. The
// A-SQP read's section M2 measured a restoration that bought feasibility at
// eleven orders of objective and showed that NO counter said so -- the phase
// produces no history row by contract, and `restoration_iters` is a count.
// These four are that reading, and the tests below pin them AGAINST EVIDENCE
// THE RECORD DOES NOT PRODUCE: the requesting row's own `violation_l1` and
// `f`, the value the funnel is re-based on, the row at the restored point, and
// a certificate's independently recomputed h. A record that agreed only with
// itself would pin nothing.

TEST(SqpDriverRestorationExcursion, TheResumeArmRecordsTheCallersOwnExcursion) {
    StalledValleyModel model;
    SqpOptions opts;
    opts.tr_init = 8.0; // SqpDriverRestoration.RestorationRecoversAndResumes' options
    opts.max_iter = 60;
    FunnelLog log;
    record_funnel_into(opts, &log);
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    ASSERT_EQ(log.resumes.size(), 1u) << "fixture premise: exactly one RESUME";
    EXPECT_EQ(sol.counters.restoration_entries, 1)
        << "one phase ran and was measured, whatever its iteration count";
    EXPECT_GT(sol.counters.restoration_iters, 0);

    // THE ENTRY HALF IS THE REQUESTING ITERATE'S OWN MEASUREMENT. Row 4 is the
    // row that carries the kRestore verdict (the sibling test pins that), and
    // its `violation_l1` and `f` are computed by the driver from the SAME
    // bundle this record reads -- so the agreement is exact, not approximate,
    // and a record taken at the wrong point (the seed, or the restored point)
    // would break it.
    ASSERT_GE(sol.history.size(), 6u);
    ASSERT_EQ(sol.history[4].verdict, StepVerdict::kRestore);
    EXPECT_DOUBLE_EQ(sol.counters.restoration_h_in, sol.history[4].violation_l1);
    EXPECT_DOUBLE_EQ(sol.counters.restoration_f_in, sol.history[4].f);
    EXPECT_GT(sol.counters.restoration_h_in, opts.feas_tol)
        << "the phase was entered at an INFEASIBLE point -- that is what h_in is for";

    // THE EXIT HALF. `h_out` is the `h_r` the funnel was re-based with; the
    // resume decision read `restoration_feas_out` (M7 W2 SNR), the inf-norm
    // the l1 `h_out` bounds, so both sit inside feas_tol on a resume.
    EXPECT_LE(sol.counters.restoration_h_out, opts.feas_tol);
    EXPECT_LE(sol.counters.restoration_feas_out, opts.feas_tol);
    EXPECT_LE(sol.counters.restoration_feas_out, sol.counters.restoration_h_out);
    EXPECT_DOUBLE_EQ(sol.counters.restoration_h_out, log.resumes[0]);
    // And `f_out` is the objective the solve CARRIES FORWARD: the restored
    // point becomes the iterate, so the first row after the resume reports it.
    EXPECT_DOUBLE_EQ(sol.counters.restoration_f_out, sol.history[5].f);
    // AND THE SUB-SOLVE'S OWN VERDICT (M7 W2 M3a). On THIS fixture the phase
    // both converged and returned a point inside `feas_tol`, so the record
    // names `kOptimal`. It is NOT the resume's cause: the parent takes that
    // decision on `restoration_feas_out <= feas_tol` alone, before it looks at this
    // status at all (fix1 corrected the note that said otherwise). Pinning it
    // here is what makes the field's OTHER values evidence rather than
    // decoration.
    EXPECT_EQ(sol.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
    // THE EXCURSION THIS FIXTURE MADE, recorded rather than asserted as a
    // bound: it is a recovery, and a benign one.
    ::testing::Test::RecordProperty(
        "excursion", fmt::format("entries={} h {:.10g} -> {:.10g}  f {:.10g} -> {:.10g}",
                                 sol.counters.restoration_entries, sol.counters.restoration_h_in,
                                 sol.counters.restoration_h_out, sol.counters.restoration_f_in,
                                 sol.counters.restoration_f_out));
}

TEST(SqpDriverRestorationExcursion, TheTerminalArmRecordsThePointItReports) {
    InfeasibleCircleLineModel model;
    SqpOptions opts;
    opts.max_iter = 200;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
    ASSERT_TRUE(sol.infeasibility_certified) << "fixture premise: the CERTIFIED terminal arm";
    EXPECT_EQ(sol.counters.restoration_entries, 1);

    // h_out ABOVE feas_tol is what says this entry took the other arm -- no
    // second field is needed to tell the two apart.
    EXPECT_GT(sol.counters.restoration_h_out, opts.feas_tol);
    // AND IT IS THE h AT THE POINT THE RESULT REPORTS, recomputed from the
    // model by the certificate helper and not by the engine.
    const SubgradientCertificate c = check_certificate(model, sol, opts.feas_tol);
    EXPECT_NEAR(sol.counters.restoration_h_out, c.h, 1e-12);
    // `f_out` is the objective of that same reported point -- on this arm the
    // result's own `f` (the solve is unscaled, so no map stands between them).
    EXPECT_DOUBLE_EQ(sol.counters.restoration_f_out, sol.f);
    // The entry half is the requesting row's, which on a terminal exit is the
    // LAST row: the phase pushes none of its own.
    ASSERT_FALSE(sol.history.empty());
    EXPECT_DOUBLE_EQ(sol.counters.restoration_h_in, sol.history.back().violation_l1);
    EXPECT_DOUBLE_EQ(sol.counters.restoration_f_in, sol.history.back().f);
    // AND THE ARM IS NAMED, NOT INFERRED (M7 W2 M3a). A CERTIFIED terminal
    // exit is reachable from the sub-solve's own `kOptimal` and from nothing
    // else -- the gate that sets `infeasibility_certified` hangs off that
    // switch arm -- so this pin and the `ASSERT_TRUE` above are two readings
    // of one fact, taken from opposite ends of the phase.
    EXPECT_EQ(sol.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
}

TEST(SqpDriverRestorationExcursion, ARefusedSecondRequestLeavesTheFirstEntrysRecord) {
    RunawayValleyModel model;
    SqpOptions opts;
    opts.tr_init = 8.0; // SqpDriverRestoration.SecondRequestIsCappedAndReported' options
    opts.max_iter = 60;
    FunnelLog log;
    record_funnel_into(opts, &log);
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    // kNumericalError since the M7 W2 ladder round (L1): a refused request is never
    // spelled kInfeasible.
    ASSERT_EQ(sol.status, SolveStatus::kNumericalError);
    ASSERT_EQ(log.resumes.size(), 1u) << "fixture premise: one serviced request, one refused";
    // THE CAP EXIT RAN NO PHASE AND MEASURED NOTHING, so it moves neither the
    // count nor the four absolutes: this solve requested restoration TWICE and
    // reports ONE entry.
    EXPECT_EQ(sol.counters.restoration_entries, 1);
    // And the record is still the SERVICED entry's -- which resumed, so its
    // h_out is inside feas_tol even though the solve ends unsuccessfully. A
    // reader that inferred the exit from the record alone would be wrong, and
    // that is the point of `infeasibility_certified` being the field that says
    // which exit this is.
    EXPECT_LE(sol.counters.restoration_h_out, opts.feas_tol);
    EXPECT_DOUBLE_EQ(sol.counters.restoration_h_out, log.resumes[0]);
    EXPECT_FALSE(sol.infeasibility_certified);
    // The sub-status is the SERVICED entry's too: the refusal ran no sub-solve,
    // so there is no second status for it to overwrite this one with.
    EXPECT_EQ(sol.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
}

TEST(SqpDriverRestorationExcursion, ACleanSolveHoldsNoReading) {
    const HsProblem p = make_hs(1);
    SqpOptions opts;
    opts.max_iter = 60;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(*p.model);

    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    ASSERT_EQ(sol.counters.restoration_iters, 0) << "fixture premise: the phase never ran";
    // THE ABSENCE READING IS THE COUNT, and the four absolutes hold their
    // defaults -- which is why the count and not a sentinel is what a consumer
    // asks: 0.0 is a legal observed violation and a legal observed objective.
    EXPECT_EQ(sol.counters.restoration_entries, 0);
    EXPECT_EQ(sol.counters.restoration_h_in, 0.0);
    EXPECT_EQ(sol.counters.restoration_h_out, 0.0);
    EXPECT_EQ(sol.counters.restoration_f_in, 0.0);
    EXPECT_EQ(sol.counters.restoration_f_out, 0.0);
    // AND THE SUB-STATUS SAYS SO ITSELF (M7 W2 M3a, corrected in fix1). It is
    // the one field of this record that CAN: the four absolutes above have no
    // spare value -- `0.0` is a legal violation and a legal objective -- so
    // their absence is the count, while this field's own enum carries a
    // `kNotRun` zero and spends it here. It shipped as a bare `SolveStatus`
    // defaulting to `kOptimal`, which made this line read "the feasibility
    // sub-solve converged" on a solve that never ran one; that is what fix1
    // withdrew. THE ASSERTION IS THE WHOLE POINT OF THE CHANGE.
    EXPECT_EQ(sol.counters.restoration_sub_status, RestorationSubStatus::kNotRun);
}

// M7 W2 M3a. THE FIELD IS NOT A CONSTANT, and this is the fixture that says so:
// ONE model, ONE restoration phase, TWO different sub-solve outcomes, told apart
// by this key and by nothing else the counters carry.
//
// `InfeasibleCircleLineModel` is the certified fixture above. Given room, its
// feasibility sub-solve CONVERGES -- that is what makes the certificate gate
// reachable at all. Given a budget of five majors for the whole solve, the
// parent lends the sub-solve what is left (`ropts.max_iter = eff_max_iter -
// spent`, sqp_solver.cpp) and the sub-solve spends it: it returns `kMaxIter`
// from the same call site, at the same arm of the phase, having measured a
// point the parent then reports.
//
// WHAT WAS UNOBSERVABLE BEFORE. The parent's own status is NOT this reading: it
// spells the sub-solve's `kOptimal`-but-still-infeasible arm and the sub-solve's
// own `kInfeasible` arm identically (`kInfeasible`), which is why M7 W2 M2b had
// to infer its rows' arms from row shape. Here the two solves differ in the
// parent status too -- but the pin that matters is that the RECORD names the
// sub-solve's verdict, and the two records differ.
TEST(SqpDriverRestorationExcursion, TheSubStatusTellsACutOffSubSolveFromAConvergedOne) {
    InfeasibleCircleLineModel model;

    SqpOptions roomy;
    // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
    roomy.equality_basis = false;
    roomy.max_iter = 200;
    roomy.qp_failure_step = false; // M7 W2 R2: the pre-R2 route (see the seed family's note)
    const SqpResult converged = SqpSolver(roomy).solve(model);
    ASSERT_EQ(converged.status, SolveStatus::kInfeasible);
    ASSERT_EQ(converged.counters.restoration_entries, 1);

    SqpOptions cramped;
    // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
    cramped.equality_basis = false;
    // CONFIG-SPLIT SINCE M7 W2 E4 FIX ROUND 2, for the reason
    // `TheSubStatusDiscriminatesAtOneParentStatus` states in full: at 5 majors Debug
    // never reaches the request (`restoration_entries` 0), so there is no cut-off
    // sub-solve to tell from a converged one; one more major reaches it.
    // M7 W2 QC-8 (declared, Debug only): 6 -> 43. Debug's trajectory now raises the
    // request at major 40 (`AWorseElasticCandidateIsRefusedOnTheSameModel`'s note); the
    // cut-off window measured at budgets 41-44 (sub kMaxIter, 1-4 sub-iterations), 45 on
    // converges. 43 keeps the sub-solve's three iterations pinned below. Release unchanged.
    // M7 W2 SNX (declared, Debug only): 43 -> 5. Debug's request falls where Release's does
    // again (`AWorseElasticCandidateIsRefusedOnTheSameModel`'s note), so the split is gone.
    cramped.max_iter = 5;
    cramped.qp_failure_step = false; // M7 W2 R2: the pre-R2 route, as `roomy`
    const SqpResult cut_off = SqpSolver(cramped).solve(model);
    ASSERT_EQ(cut_off.counters.restoration_entries, 1)
        << "fixture premise: the phase RAN and was measured on this budget too";

    // THE TWO READINGS, AND THEY DISAGREE.
    EXPECT_EQ(converged.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
    EXPECT_EQ(cut_off.counters.restoration_sub_status, RestorationSubStatus::kMaxIter);
    EXPECT_NE(converged.counters.restoration_sub_status, cut_off.counters.restoration_sub_status);

    // AND EACH AGREES WITH THE EXIT IT IMPLIES. The certificate gate hangs off
    // the sub-solve's `kOptimal` arm, so a cut-off sub-solve cannot certify --
    // and this is a REFUSAL BY ARM, not a refusal by residual.
    EXPECT_TRUE(converged.infeasibility_certified);
    EXPECT_FALSE(cut_off.infeasibility_certified);
    EXPECT_EQ(cut_off.status, SolveStatus::kMaxIter);

    // The record is the not-feasible-enough arm's on BOTH solves: neither
    // resumed, and `restoration_feas_out` above `feas_tol` is what says so (the
    // resume decision's reading since M7 W2 SNR; `h_out` is the l1 sum beside it).
    EXPECT_GT(converged.counters.restoration_feas_out, roomy.feas_tol);
    EXPECT_GT(cut_off.counters.restoration_feas_out, cramped.feas_tol);
    EXPECT_GT(converged.counters.restoration_h_out, roomy.feas_tol);
    EXPECT_GT(cut_off.counters.restoration_h_out, cramped.feas_tol);
    // The sub-solve did real work before the budget ended it -- a phase that
    // returned at once would make the pin above a statement about nothing.
    // RE-DERIVED AT M7 W2 E4 FIX ROUND 2 (a declared counter move): `> 1` -> `== 1`.
    // The cut-off arm's budget is `max_iter = 5` for the WHOLE solve, and E4's QP
    // answers reach the request one major earlier, so the sub-solve gets one
    // iteration of it instead of two before the budget ends. One iteration is still
    // real work -- `restoration_sub_status` reads kMaxIter rather than kNotRun, which
    // is the discrimination this test exists for and is asserted above -- and the
    // pin is kept as an equality so the margin cannot quietly go to zero.
    // M7 W2 BPS (declared mover, Debug only): 1 -> 3 on Debug's 6-major budget -- the
    // bordered answer's certificate (bordered_eqp.h) brings the request forward there, so
    // the sub-solve gets more of the budget; Release unchanged.
    // M7 W2 SNX (declared, Debug only): 3 -> 1, with the budget above.
    EXPECT_EQ(cut_off.counters.restoration_iters, 1);
}

// M7 W2 M3a fix1. THE PARENT'S STATUS DISCRIMINATES NONE OF THE THREE, and this
// fixture is that statement made three times at ONE parent status.
//
// The fixture above tells two sub-solve outcomes apart, but its two solves
// differ in the PARENT's status too (`infeasible` against `max_iter`), so a
// reader could take the parent's status for the discriminator and be right by
// accident. The review asked for the same reading at a FIXED parent status.
// Here every solve ends `kMaxIter` and the three records read `not_run`,
// `optimal` and `max_iter` -- so the field is carrying information that exists
// nowhere else on the result.
//
// AND THE FIRST ARM IS THE ONE FIX1 ADDED (the settler's withdrawal of "default
// kOptimal + restoration_entries == 0"). `RunawayValleyModel` on a five-major
// budget REQUESTS restoration -- its last row carries `kRestore` -- and is
// REFUSED for want of majors, which is one of the record's three refusals: the
// phase never ran, nothing was measured, and the record says `not_run` rather
// than a legal success value. Under the shipped-then-withdrawn representation
// this solve reported `optimal` for a feasibility sub-solve it never started,
// AT A PARENT STATUS THAT LOOKS EXACTLY LIKE THE ARM BELOW IT.
TEST(SqpDriverRestorationExcursion, TheSubStatusDiscriminatesAtOneParentStatus) {
    // ARM 1 -- requested, refused for budget, NEVER RAN.
    RunawayValleyModel valley_refused;
    SqpOptions refused;
    // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
    refused.equality_basis = false;
    refused.tr_init = 8.0;
    refused.max_iter = 5;
    const SqpResult never_ran = SqpSolver(refused).solve(valley_refused);
    ASSERT_EQ(never_ran.status, SolveStatus::kMaxIter);
    ASSERT_FALSE(never_ran.history.empty());
    ASSERT_EQ(never_ran.history.back().verdict, StepVerdict::kRestore)
        << "fixture premise: the request WAS raised on the last major -- the absence "
           "below is a refused phase, not an uneventful solve";
    ASSERT_EQ(never_ran.counters.restoration_entries, 0)
        << "fixture premise: the refusal ran no phase and measured nothing";
    EXPECT_EQ(never_ran.counters.restoration_iters, 0);
    EXPECT_EQ(never_ran.counters.restoration_sub_status, RestorationSubStatus::kNotRun);

    // ARM 2 -- the SAME model and the SAME options but five more majors: the
    // phase runs, converges, and the parent RESUMES on `feas_out <= feas_tol`;
    // the solve then spends the rest of its budget and ends at the same status
    // as arm 1.
    RunawayValleyModel valley_resumed;
    SqpOptions resumed;
    // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
    resumed.equality_basis = false;
    resumed.tr_init = 8.0;
    resumed.max_iter = 10;
    const SqpResult sub_optimal = SqpSolver(resumed).solve(valley_resumed);
    ASSERT_EQ(sub_optimal.status, SolveStatus::kMaxIter);
    ASSERT_EQ(sub_optimal.counters.restoration_entries, 1);
    EXPECT_EQ(sub_optimal.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
    EXPECT_LE(sub_optimal.counters.restoration_feas_out, resumed.feas_tol)
        << "this entry RESUMED, and restoration_feas_out is the number that decision was "
           "taken on";
    EXPECT_LE(sub_optimal.counters.restoration_h_out, resumed.feas_tol);

    // ARM 3 -- a phase that ran and was CUT OFF, at the same parent status
    // again. `InfeasibleCircleLineModel` on five majors: the sub-solve spends
    // the majors the parent lends it and returns `kMaxIter` with h still above
    // feas_tol, so the parent reports that status as its own.
    InfeasibleCircleLineModel circle;
    SqpOptions cramped;
    // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
    cramped.equality_basis = false;
    // THE BUDGET IS CONFIG-SPLIT SINCE M7 W2 E4 FIX ROUND 2 (declared), so that the
    // CLAIM survives in BOTH configurations rather than only in Release. MEASURED: at
    // 5 majors Debug's trajectory never reaches the request at all
    // (`restoration_entries` 0, `restoration_sub_status` kNotRun), so arm 3 collapses
    // into arm 1 and the three-record read-out becomes a two-record one. One more
    // major reaches it. E4's QP answers changed where on this trajectory the request
    // falls, and Debug's arithmetic puts it one major later than Release's; the ARM is
    // the same arm -- a phase that RAN and was CUT OFF -- and that is what is asserted.
    // M7 W2 QC-8 (declared, Debug only): 6 -> 43, as `TheSubStatusTellsACutOffSubSolve
    // FromAConvergedOne` re-derives it (Debug's request now falls at major 40).
    // M7 W2 SNX (declared, Debug only): 43 -> 5, as that test re-derives it.
    cramped.max_iter = 5;
    // M7 W2 R2: the pre-R2 route (see the seed family's note) -- with the
    // failed-subproblem step this budget ends before the request is raised.
    cramped.qp_failure_step = false;
    const SqpResult sub_cut_off = SqpSolver(cramped).solve(circle);
    ASSERT_EQ(sub_cut_off.status, SolveStatus::kMaxIter);
    ASSERT_EQ(sub_cut_off.counters.restoration_entries, 1);
    EXPECT_EQ(sub_cut_off.counters.restoration_sub_status, RestorationSubStatus::kMaxIter);
    EXPECT_GT(sub_cut_off.counters.restoration_feas_out, cramped.feas_tol) << "not resumed";
    EXPECT_GT(sub_cut_off.counters.restoration_h_out, cramped.feas_tol);
    EXPECT_FALSE(sub_cut_off.infeasibility_certified)
        << "the certificate gate hangs off the sub-solve's kOptimal arm";

    // THE POINT, STATED AS AN ASSERTION. One parent status, three records.
    EXPECT_EQ(never_ran.status, sub_optimal.status);
    EXPECT_EQ(never_ran.status, sub_cut_off.status);
    EXPECT_NE(never_ran.counters.restoration_sub_status,
              sub_optimal.counters.restoration_sub_status);
    EXPECT_NE(never_ran.counters.restoration_sub_status,
              sub_cut_off.counters.restoration_sub_status);
    EXPECT_NE(sub_optimal.counters.restoration_sub_status,
              sub_cut_off.counters.restoration_sub_status);
}

// ---------------------------------------------------------------------------
// THE OPTION ITSELF (M7 W2 M2b): validation and the dump line.
// ---------------------------------------------------------------------------

TEST(RestorationProximityOption, RefusesANegativeNanOrInfiniteWeightByNameAndValue) {
    SqpOptions o;
    o.restoration_proximity = -1.0;
    try {
        SqpSolver driver(o);
        FAIL() << "a negative proximity weight was accepted";
    } catch (const std::invalid_argument &e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("restoration_proximity"), std::string::npos) << what;
        EXPECT_NE(what.find("-1"), std::string::npos) << what;
    }
    o.restoration_proximity = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(SqpSolver{o}, std::invalid_argument)
        << "the !(w >= 0) form is what catches the NaN";
    o.restoration_proximity = std::numeric_limits<double>::infinity();
    EXPECT_THROW(SqpSolver{o}, std::invalid_argument)
        << "+inf is not 'very heavy': it is +inf everywhere but the centre";
    // AND 0 IS ACCEPTED -- it is the term's OFF state and the shipped value,
    // not an ill-formed one.
    o.restoration_proximity = 0.0;
    EXPECT_NO_THROW(SqpSolver{o});
    o.restoration_proximity = 1e-3;
    EXPECT_NO_THROW(SqpSolver{o});
}

TEST(RestorationProximityOption, ShipsAtZeroAndCarriesADumpLineInDeclarationPosition) {
    EXPECT_DOUBLE_EQ(SqpOptions{}.restoration_proximity, 0.0)
        << "NO DEFAULT MOVES at M7 W2 M2b: the term lands OFF and its weight is uncalibrated";
    SqpOptions o;
    o.restoration_proximity = 0.25;
    std::ostringstream os;
    dump_options(o, os);
    const std::string text = os.str();
    EXPECT_NE(text.find("restoration_proximity = 0.25\n"), std::string::npos) << text;
    // DECLARATION POSITION: after M1's pair and before the `common.` block,
    // which is what the drift pin's line count in tests/drivers/
    // test_options_dump.cpp is counting.
    EXPECT_LT(text.find("qp_cap_retry"), text.find("restoration_proximity"));
    EXPECT_LT(text.find("restoration_proximity"), text.find("common.threads"));
}

// M7 W2 P1E -- the sufficient-reduction resume's boundary and its certificate guard.
TEST(RestorationResumeReductionOption, RefusesAFractionOutsideTheUnitIntervalByNameAndValue) {
    SqpOptions o;
    EXPECT_DOUBLE_EQ(o.restoration_resume_reduction, 0.0) << "OFF by default";
    for (const double bad : {-0.5, 1.0, 2.0, std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        o.restoration_resume_reduction = bad;
        try {
            SqpSolver driver(o);
            FAIL() << "accepted " << bad;
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("restoration_resume_reduction"), std::string::npos)
                << e.what();
        }
    }
    for (const double good : {0.0, 1e-3, 0.999}) {
        o.restoration_resume_reduction = good;
        EXPECT_NO_THROW(SqpSolver{o}) << good;
    }
    o.restoration_resume_reduction = 0.25;
    std::ostringstream os;
    dump_options(o, os);
    EXPECT_NE(os.str().find("restoration_resume_reduction = 0.25\n"), std::string::npos);
}

// A CERTIFYING kOptimal never resumes by reduction, however far it cut h.
TEST(RestorationResumeReductionOption, ACertifyingSubSolveKeepsItsCertificate) {
    InfeasibleCircleLineModel model;
    SqpOptions opts;
    opts.max_iter = 200;
    opts.restoration_resume_reduction = 0.999;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);
    ASSERT_EQ(sol.counters.restoration_entries, 1);
    // THE PREMISE: the entry cut h below the fraction, so only the guard refuses it.
    ASSERT_LE(sol.counters.restoration_h_out, 0.999 * sol.counters.restoration_h_in);
    EXPECT_EQ(sol.counters.restoration_reduction_resumes, 0);
    EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
    EXPECT_TRUE(sol.infeasibility_certified);
}

// =========================================================================
// FIX ROUND 1 (M7 W2 M2b) -- the certificate's finite-tolerance boundary, the
// seed guard driven end to end, and the refusals' message content.
// =========================================================================
namespace {

// The fixture model, WATCHING every point its values are asked for -- which is
// how the requesting iterate and the seed the phase actually started from are
// recovered from outside the driver. Only `eval_f`/`eval_ce` are hooked: those
// two are what every values-or-full query on this model goes through, and the
// derivative-only refresh the seed guard's MEASURED arm takes is deliberately
// not counted as a new point.
class WatchedCircleLineModel final : public InfeasibleCircleLineModel {
  public:
    explicit WatchedCircleLineModel(double s0 = 2.0, double s1 = 2.0)
        : InfeasibleCircleLineModel(s0, s1) {}

    double eval_f(const Vec &x) const override {
        points.push_back(x);
        return InfeasibleCircleLineModel::eval_f(x);
    }
    Vec eval_ce(const Vec &x) const override {
        points.push_back(x);
        return InfeasibleCircleLineModel::eval_ce(x);
    }

    mutable std::vector<Vec> points;
};

// THE PROXIMITY TERM'S OWN GRADIENT NORM, `zeta ||D^2 (x - x_k)||_inf`, lives
// HERE and no longer in the wrapper: fix round 1 removed
// `RestorationModel::proximity_residual_inf`, because the driver must not have
// a cheap almost-right quantity within reach of the certificate. The test keeps
// it, because the fixture below has to ASSERT that this is the quantity the old
// gate would have passed.
double proximity_gradient_inf(double zeta, const Vec &x, const Vec &x_k) {
    double r = 0.0;
    for (Index i = 0; i < x.size(); ++i) {
        const double a = std::abs(x_k(i));
        const double d = a > 1.0 ? 1.0 / a : 1.0;
        r = std::max(r, std::abs(zeta * d * d * (x(i) - x_k(i))));
    }
    return r;
}

// The REQUESTING ITERATE `x_k`, recovered from the S5 excursion record: it is
// the watched point whose h AND f both equal the record's entry half
// (core/solver_counters.h -- `restoration_h_in`/`restoration_f_in` are taken at
// the requesting iterate, before the seed guard can move the start). Searched
// from the back so that the match nearest the request wins. Re-evaluated
// through an UNWATCHED model so the search does not extend the list it walks.
const Vec *recover_requesting_iterate(const std::vector<Vec> &pts, double h_in, double f_in,
                                      double s0, double s1) {
    const InfeasibleCircleLineModel plain(s0, s1);
    for (std::size_t k = pts.size(); k-- > 0;) {
        const NlpEval ev = eval_nlp(plain, pts[k]);
        const double h = constraint_violation_l1(ev);
        if (std::abs(h - h_in) <= 1e-12 * std::max(1.0, std::abs(h_in)) &&
            std::abs(ev.f - f_in) <= 1e-12 * std::max(1.0, std::abs(f_in))) {
            return &pts[k];
        }
    }
    return nullptr;
}

// THE CANCELLATION FIXTURE'S TWO NUMBERS, and why they are these rather than
// round ones. The fixture needs a point where the proxy `||grad P||_inf` sits
// BELOW `kkt_tol` while the exact residual `||J^T lambda - z_sub||_inf` sits
// ABOVE it, and the gap between them is the sub-solve's own terminal residual
// `e`, which on this fixture is 3.5805e-07 and does NOT go to zero (that is why
// the OTHER certificate fixture, whose sub-solve converges to 4.9e-11, cannot
// see this defect at any weight).
//
// MEASURED, on both configurations, by a sweep over (zeta, kkt_tol) whose
// transcript is in `gates-fix1/` and whose two axes behave differently:
// `kkt_tol` does not move this fixture's trajectory at all in this range -- it
// only moves the threshold -- while `zeta` does, and at most weights Debug and
// Release then take different restoration paths. `zeta = 1.5e-06` is one of the
// weights at which they do NOT: it leaves the path alone, so the two configs
// agree to 1e-12 on the residual.
//
//     ||grad P||_inf  = 4.5482e-07   (the OLD gate's quantity)
//     ||J^T l - z||   = 8.1287e-07   (Release) / 8.1278e-07 (Debug)
//
// `kkt_tol` is then placed at the MIDPOINT of that window, 6.3e-07, which gives
// the two premises a margin of 28% and 29% respectively. Both are asserted
// below, so a fixture that drifts out of its window FAILS here instead of
// silently testing nothing.
constexpr double kCancellationZeta = 1.5e-6;
// RE-CALIBRATED AT M7 W2 E4 FIX ROUND 2 (a declared re-derivation of the
// threshold, not of the claim). The window is the same two quantities and it is
// still OPEN; E4 moved the exact residual down into it, so the midpoint moved with
// it. MEASURED at this round's engine, Release:
//
//     ||grad P||_inf   = 4.3934000928e-07   (the OLD gate's quantity)
//     ||J^T l - z||    = 5.3620559926e-07   (the exact residual)
//
// against 4.5482e-07 / 8.1287e-07 before. `kkt_tol` is placed at the midpoint
// again, 4.88e-07, which leaves each premise a margin of 11 % where it had 28 %
// and 29 %. The window NARROWED because the exact residual improved, which is the
// round's own doing (the terminal polish reaches the face's mu-free answer), and
// the fixture keeps its power exactly: both premises are asserted below, so a
// fixture that drifts out of this narrower window fails here rather than testing
// nothing. REGISTERED: at 11 % this is the thinnest the window has been, and a
// future round that moves the residual another 20 % closes it -- the fixture then
// needs a new point rather than a new threshold.
// AND IT IS CONFIG-SPLIT since the same round, because the two configurations' windows
// no longer overlap usefully. MEASURED:
//
//              proxy                exact residual        midpoint    margin
//   Release    4.3933982708e-07*    5.3620559926e-07      4.88e-07    11 %
//   Debug      4.3933982708e-07     4.5566300666e-07      4.475e-07   1.9 %
//   (*Release's proxy is 4.3934000928e-07; the two agree to 4e-13.)
//
// Debug's exact residual is 15 % lower than Release's, so its window is 3.7 % wide and
// any single threshold inside it has at most 1.9 % of margin on each premise. That is
// thin, it is DECLARED thin, and it is still a pin rather than a skip: both values are
// deterministic per build, and a drift of 2 % fails this test -- which is exactly what
// the fixture is for. REGISTERED: Debug's window is the narrowest of the two and will
// close first.
#ifdef NDEBUG
constexpr double kCancellationKktTol = 4.88e-7;
#else
constexpr double kCancellationKktTol = 4.475e-7;
#endif

} // namespace

// THE FINITE-TOLERANCE CANCELLATION FIXTURE (fix round 1; the review's critical
// finding). The first implementation gated `infeasibility_certified` on
// `||grad P||_inf <= kkt_tol`, on the identity that at an EXACTLY stationary
// point of the wrapper that norm IS `||J^T lambda - z_sub||_inf`. A sub-solve
// returns kOptimal at a FINITE tolerance, so what it delivers is
//
//     e = grad P + J^T lambda - z_sub,   ||e||_inf <= kkt_tol,
//
// and the certificate needs `||e - grad P||_inf`, which the proxy bounds only
// by `2*kkt_tol`. This fixture is a point where the two really do separate: the
// proxy is BELOW kkt_tol and the exact residual is ABOVE it, so the old
// expression certifies a point that is not a certified infeasible stationary
// point of `h`. It FAILS on the pre-fix code (measured once, transcript in
// `gates-fix1/`) and passes on the exact rule.
//
// The seeded variant of the fixture is used because its restoration sub-solve
// terminates with a real residual -- 3.5805e-07 -- rather than at machine
// precision: with `e` negligible the proxy and the exact residual coincide and
// no separation exists at ANY weight, which is exactly why the first round's
// three-arm fixture could not see the defect.
TEST(SqpDriverRestorationProximity, TheProxyWouldCertifyWhereTheExactResidualRefuses) {
    WatchedCircleLineModel model(2.0, 1.999999);
    SqpOptions opts;
    // M7 W2 E2: in Debug the E2 walk's end point satisfies the exact residual
    // (5.5e-7 < 6.3e-7) and is then rightly certified; premise 2 belongs to the
    // pre-E2 walk's trajectory.
    opts.qp.independent_working_set = false;
    opts.max_iter = 200;
    opts.kkt_tol = kCancellationKktTol;
    opts.restoration_proximity = kCancellationZeta;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    ASSERT_EQ(SolveStatus::kInfeasible, sol.status);
    ASSERT_GE(sol.counters.restoration_iters, 1) << "the phase must have RUN";
    ASSERT_EQ(1, sol.counters.restoration_entries) << "one measured excursion to read x_k from";

    const Vec *x_k = recover_requesting_iterate(model.points, sol.counters.restoration_h_in,
                                                sol.counters.restoration_f_in, 2.0, 1.999999);
    ASSERT_NE(nullptr, x_k) << "the requesting iterate must be recoverable from the S5 record";

    // THE FIXTURE'S TWO PREMISES, MEASURED HERE rather than assumed, so that a
    // drift out of the window fails this test instead of hollowing it out.
    const double proxy = proximity_gradient_inf(kCancellationZeta, sol.x, *x_k);
    const SubgradientCertificate c = check_certificate(model, sol, opts.feas_tol);
    EXPECT_LE(proxy, opts.kkt_tol)
        << "premise 1: the OLD proxy passes here (" << proxy << " vs " << opts.kkt_tol << ")";
    EXPECT_GT(c.stationarity, opts.kkt_tol)
        << "premise 2: the EXACT residual does not (" << c.stationarity << ")";
    EXPECT_GT(c.h, opts.feas_tol) << "and the point is genuinely infeasible";

    // THE VERDICT. The old expression sets this true; the exact rule refuses.
    EXPECT_FALSE(sol.infeasibility_certified)
        << "the returned point is NOT a certified infeasible stationary point of h: its exact "
           "subgradient residual is "
        << c.stationarity << " against a kkt_tol of " << opts.kkt_tol;

    ::testing::Test::RecordProperty(
        "cancellation", fmt::format("zeta={:g} proxy={:.6g} exact={:.6g} kkt_tol={:g} certified={}",
                                    kCancellationZeta, proxy, c.stationarity, opts.kkt_tol,
                                    sol.infeasibility_certified));
}

// THE SEED GUARD, DRIVEN END TO END AT zeta > 0 (fix round 1; the review's
// "that test constructs the two points directly"). The sibling fixture
// `RestorationModel.IsTheSmoothL1FeasibilityProblemAtAPositiveProximity` pins
// the `h + P` start objective on HAND-MADE points. This one makes the driver's
// own guard fire, recovers the requesting iterate from the S5 record and the
// candidates the guard could have taken from the watched points, and pins the
// identity on those.
//
// WHAT IT ASSERTS, and why it quantifies rather than guesses. The guard takes
// an offered candidate iff h there is below h at the entry; which candidate was
// offered is a property of the route (elastic ladder vs radius floor) and is
// build-arithmetic-dependent on this knife-edge fixture, so the test asserts
// the identity over EVERY watched point that the guard's own test admits. For
// any such point the wrapper's start objective is h + P with P > 0 -- never h
// -- and that is the declared price of centring on `x_k`
// (detail/globalization/sqp/restoration.h's class note).
TEST(SqpDriverRestorationProximity, TheSeedGuardFiresAndTheStartObjectiveIsHPlusP) {
    WatchedCircleLineModel model(2.0, 1.999999);
    SqpOptions opts;
    // M7 W2 E2 (S7): the taken candidate belongs to the pre-E2 walk (Release takes none with it
    // on).
    opts.qp.independent_working_set = false;
    opts.max_iter = 200;
    opts.restoration_proximity = 0.25;
    opts.qp_failure_step = false; // M7 W2 R2: the pre-R2 route (see the seed family's note)
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    // END TO END: the REAL guard fired, on the row that raised the request.
    // RE-PREMISED (M7 W2 E4 fix round 2, ruling E4-21(b); see the seed family's note
    // above): 1 -> 0. The offered candidate no longer measures better than the
    // requesting iterate, so the guard refuses it and the phase starts where the
    // request was raised. THE CLAIM THIS FIXTURE IS NAMED FOR IS UNCHANGED and is
    // asserted below: the restoration objective the phase minimizes is h PLUS the
    // proximity term P, centred on the REQUESTING iterate -- which is the same
    // point either way, and is in fact easier to read now that the guard has not
    // moved the start.
    ASSERT_EQ(0, seeded_rows(sol)) << "the candidate measures better again: the guard took it";
    EXPECT_EQ(nullptr, seeded_row(sol));
    ASSERT_GE(sol.counters.restoration_iters, 1) << "the phase RAN from the requesting iterate";
    ASSERT_EQ(1, sol.counters.restoration_entries);

    const Vec *x_k = recover_requesting_iterate(model.points, sol.counters.restoration_h_in,
                                                sol.counters.restoration_f_in, 2.0, 1.999999);
    ASSERT_NE(nullptr, x_k);

    const InfeasibleCircleLineModel plain(2.0, 1.999999);
    const double h_in = sol.counters.restoration_h_in;
    Index candidates = 0;
    for (const Vec &x_entry : model.points) {
        const NlpEval ev = eval_nlp(plain, x_entry);
        const double h_entry = constraint_violation_l1(ev);
        if (!(h_entry < h_in)) {
            continue; // not a point the guard's own test would have taken
        }
        ++candidates;
        // THE SHIPPED (zeta = 0) WRAPPER: the objective at its own start point
        // IS h(x_entry) -- the smooth-l1 identity, unconditional.
        const RestorationModel off(plain, x_entry, ev);
        EXPECT_DOUBLE_EQ(off.eval_f(off.start_point()), h_entry);

        // THE zeta > 0 WRAPPER THE DRIVER BUILDS, centred on the REQUESTING
        // iterate: the same objective plus exactly P(x_entry).
        const RestorationModel on(plain, x_entry, ev, *x_k, opts.restoration_proximity);
        double p = 0.0;
        for (Index i = 0; i < x_entry.size(); ++i) {
            const double a = std::abs((*x_k)(i));
            const double d = a > 1.0 ? 1.0 / a : 1.0;
            const double t = d * (x_entry(i) - (*x_k)(i));
            p += t * t;
        }
        p *= 0.5 * opts.restoration_proximity;
        EXPECT_GT(p, 0.0) << "a candidate the guard took is not the requesting iterate";
        EXPECT_DOUBLE_EQ(on.eval_f(on.start_point()), h_entry + p)
            << "the start-objective identity is a zeta = 0 fact, or an unfired-guard fact";
        // AND THE EXACTNESS OF THE REFORMULATION IS UNTOUCHED: the term is on
        // the objective being minimized, not on the violation being measured.
        EXPECT_LT(on.eval_ce(on.start_point()).lpNorm<Eigen::Infinity>(), 1e-12);
    }
    EXPECT_GE(candidates, 1) << "the guard fired, so at least one better-measuring point exists";
}

// THE REFUSALS' MESSAGE CONTENT (fix round 1). CLAUDE.md section 4 requires a
// library refusal to fold the diagnostic into the exception it throws; a test
// that only checks the TYPE cannot tell a useful message from an empty one.
// Both boundaries are checked: the options door and the wrapper's own.
TEST(SqpDriverRestorationProximity, TheRefusalsNameTheFieldTheValueAndTheRemedy) {
    auto message_of = [](const std::function<void()> &f) -> std::string {
        try {
            f();
        } catch (const std::invalid_argument &e) {
            return e.what();
        } catch (const std::exception &e) {
            return std::string("<wrong exception type: ") + e.what() + ">";
        }
        return "<no exception>";
    };
    auto says = [](const std::string &msg, const char *needle) {
        EXPECT_NE(std::string::npos, msg.find(needle))
            << "the message does not mention '" << needle << "':\n"
            << msg;
    };

    for (const double bad : {-1.0, std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()}) {
        SCOPED_TRACE(fmt::format("options, zeta={}", bad));
        const std::string msg = message_of([bad] {
            SqpOptions opts;
            opts.restoration_proximity = bad;
            SqpSolver driver(opts);
        });
        says(msg, "restoration_proximity"); // the FIELD
        says(msg, "finite");                // what is wrong
        says(msg, ">= 0");                  // the admissible set
        says(msg, "0 is OFF");              // the remedy
    }
    { // and the value itself is in the message, not just the rule it broke
        const std::string msg = message_of([] {
            SqpOptions opts;
            opts.restoration_proximity = -2.5;
            SqpSolver driver(opts);
        });
        says(msg, "-2.5");
    }

    CircleInequalityModel model;
    Vec x_entry(2);
    x_entry << 2.0, 2.0;
    const NlpEval ev = eval_nlp(model, x_entry);
    Vec centre(2);
    centre << 1.0, 1.0;
    for (const double bad : {-1.0, std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()}) {
        SCOPED_TRACE(fmt::format("wrapper, zeta={}", bad));
        const std::string msg = message_of(
            [&] { static_cast<void>(RestorationModel(model, x_entry, ev, centre, bad)); });
        says(msg, "RestorationModel");
        says(msg, "proximity");
        says(msg, ">= 0");
        says(msg, "0 = OFF");
    }
    { // the WRONG-SIZED CENTRE says both sizes and what the centre is for
        const std::string msg = message_of(
            [&] { static_cast<void>(RestorationModel(model, x_entry, ev, Vec::Ones(3), 1.0)); });
        says(msg, "x_centre");
        says(msg, "size 3");
        says(msg, "expected 2");
        says(msg, "requesting iterate");
    }
}

// =========================================================================
// FIX ROUND 2 (M7 W2 M2b) -- the OFF path made unchanged BY CONSTRUCTION, and
// the production centre falsified rather than restated.
// =========================================================================

// THE OFF PATH TAKES NO FLOPS AND READS NO BUNDLE (fix round 2, the review's
// remaining Important 1). Fix round 1 computed the certificate's residual
// unconditionally, from a COPY of the engine-scale bundle divided back through
// the row factors. That conversion is dimensionally right but is NOT floating-
// point identity, so on a SCALED solve the recomputed number can differ in its
// last bits from the raw one the sub-solve actually tested -- and at zeta = 0,
// where the pre-M2b code certified on the sub-solve's own kOptimal alone, a
// last-bit difference at the tolerance boundary could REFUSE a certificate that
// used to be issued. Margin in the measured evidence is not a proof of that
// boundary.
//
// So the OFF path no longer recomputes anything: `feasibility_subsolve_certifies`
// returns true at zeta == 0 without touching the bundle. This test is the
// STRUCTURAL guarantee of that, and it does not read like a comment: the bundle
// handed to the gate is POISONED with NaN, so any route through
// `subgradient_certificate_residual_inf` yields NaN, and `NaN <= kkt_tol` is
// false. A gate that reads the bundle at zeta == 0 fails the first arm; a gate
// that stops reading it at zeta > 0 fails the second.
TEST(SqpDriverRestorationProximity, TheZeroWeightGateNeverReadsTheBundle) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double kkt_tol = 1e-8;

    Vec lambda_e(1), lambda_i, z(2);
    lambda_e << 3.0;
    z << 6.0, 0.0;

    // Je = [2 0], so J^T lambda = (6, 0) and the CLEAN bundle's residual is
    // exactly 0 -- the one number that makes the third arm's `true` mean "the
    // residual was computed and passed" rather than "nothing was checked".
    NlpEval clean;
    clean.Je.resize(1, 2);
    clean.Je.insert(0, 0) = 2.0;
    clean.Je.makeCompressed();

    NlpEval poisoned = clean;
    poisoned.Je.coeffRef(0, 0) = nan;

    Index reads = 0;
    const auto poisoned_bundle = [&reads, &poisoned]() {
        ++reads;
        return poisoned;
    };
    const auto clean_bundle = [&reads, &clean]() {
        ++reads;
        return clean;
    };

    EXPECT_TRUE(
        feasibility_subsolve_certifies(0.0, kkt_tol, lambda_e, lambda_i, z, poisoned_bundle))
        << "at the shipped weight the sub-solve's own kOptimal IS the certificate";
    EXPECT_EQ(0, reads) << "the OFF path must not read, copy or rescale the bundle at all -- a "
                           "gate that routes zeta = 0 through the residual would have read it "
                           "here, and would then have refused on the NaN";

    EXPECT_FALSE(
        feasibility_subsolve_certifies(1e-30, kkt_tol, lambda_e, lambda_i, z, poisoned_bundle))
        << "at ANY positive weight the residual is what decides, and a NaN residual is not "
           "<= kkt_tol";
    EXPECT_EQ(1, reads) << "the ON path reads the bundle exactly once";

    EXPECT_TRUE(feasibility_subsolve_certifies(1e-30, kkt_tol, lambda_e, lambda_i, z, clean_bundle))
        << "and the ON path is the RESIDUAL, not a blanket refusal: this bundle's is 0";
    EXPECT_EQ(2, reads);
}

// AND THE PIN THAT GOES WITH IT: a SCALED solve whose certified exit is
// asserted at an EXPLICIT zeta = 0 (fix round 2). The structural test above
// proves the gate's dispatch; this proves the shipped configuration the
// dispatch protects -- non-trivial row factors, two orders apart, on the
// fixture whose whole point is that the engine and caller scales order its
// offers oppositely. If the OFF path ever routes through the rescaled copy
// again, this is where a boundary refusal on a real scaled solve shows up.
TEST(SqpDriverRestorationProximity, TheZeroWeightCertifiesOnAScaledSolve) {
    BoxWatchingSpreadModel model;
    SqpOptions opts;
    // The fixture's inconsistency is a dependent row pair; the equality basis removes it.
    opts.equality_basis = false;
    opts.max_iter = 60;
    opts.enable_scaling = true;
    opts.restoration_proximity = 0.0; // EXPLICIT: this is the field's OFF value
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    // THE FIXTURE'S PREMISE: scaling RAN, and the factors are not all 1.
    ASSERT_TRUE(sol.scaling.active) << "an unscaled solve would test nothing here";
    EXPECT_NEAR(sol.scaling.row_min, 0.01, 1e-12);
    EXPECT_NEAR(sol.scaling.row_max, 1.0, 1e-12);
    ASSERT_LT(sol.scaling.row_min, sol.scaling.row_max) << "the factors must be NON-TRIVIAL";

    ASSERT_EQ(SolveStatus::kInfeasible, sol.status);
    ASSERT_GE(sol.counters.restoration_iters, 1) << "the phase must have RUN";
    EXPECT_TRUE(sol.infeasibility_certified)
        << "a scaled OFF solve certifies exactly as it did before the proximity term existed";
    EXPECT_NEAR(sol.x(0), 1.0, 1e-9);
    EXPECT_NEAR(sol.x(1), 0.0, 1e-9);
}

// THE PRODUCTION CENTRE, FALSIFIED (fix round 2, the review's Important 3; the
// fixture rebuilt in fix round 2b). The sibling fixture above drives the real
// seed guard and then checks the `h + P` identity on a wrapper the TEST
// constructs with `x_k`; that restates the wiring rather than observing it, so
// a production regression to centring on `*x_start` -- the candidate the guard
// moved the start to -- would not fail it.
//
// THIS ONE READS THE CENTRE OUT OF WHAT PRODUCTION RETURNED, with no seam and
// no private access. A kOptimal sub-solve leaves its own x-block stationarity
// residual
//
//     e(c) = grad P_c(x_r) + J(x_r)^T lambda - z,    ||e(c)||_inf <= kkt_tol
//
// where `c` is the centre the wrapper was built with. `x_r`, `lambda` and `z`
// are all REPORTED fields, so `e(c)` is computable from outside for any
// candidate centre -- and only the true centre makes it small. The verdict is
// therefore two-sided: `e(x_k)` at tolerance, and `e` at every point the guard
// could have started from instead an order of magnitude ABOVE it. `*x_start` is
// one of those points, so a regression to it fails here.
//
// WHY THE FIXTURE IS PURPOSE-BUILT, AND NOT ONE OF THIS FILE'S CIRCLE/LINE
// STARTS. `e(c) - e(x_k)` is `zeta * D^2 * (x_k - c)` componentwise, so the
// verdict discriminates only in proportion to the SEPARATION between the
// requesting iterate and the candidate centres. On a seeded circle/line start
// that separation is whatever a knife-edge route happens to offer: restoration
// is requested there only once the solve has stalled at an infeasible
// stationary point, where every offer -- and every later point of the
// restoration phase itself -- sits within microns of `x_k`. Fix round 2 chose
// its start by SCANNING for one that separated, measured the scan in Debug, and
// the scan then found nothing in Release (the circle/line route is a known
// Debug/Release bifurcation), so the premise failed and the test could render
// no verdict at all. The separation is now O(1) BY CONSTRUCTION:
//
//   * THE REQUEST IS AT MAJOR 0, at the start point, so `x_k` is known at
//     design time and is asserted against the S5 record rather than recovered
//     by search.
//   * THE OFFER IS THE REJECTED TRIAL AT THE FULL RADIUS. `tr_min` is raised to
//     5 against a `tr_init` of 8, so the FIRST rejection crosses the floor
//     (8 * kTrShrinkFactor = 4 < 5) and the radius never collapses: the
//     candidate the guard is handed is the major-0 trial, `tr_init` away from
//     `x_k` in x1 and 2.06 away in x0.
//   * THE TRIAL MEASURES BETTER AND IS STILL REFUSED, which is what makes the
//     guard fire on it. `h` falls (1.65 -> 0.425, so the guard takes it) while
//     `f` rises by 4088 against a predicted decrease of 8, so Eq. (11)'s Armijo
//     test refuses the step -- an f-TYPE rejection, since `pred_df = 8` clears
//     `kFunnelDelta * h_old^2 = 2.72`.
//   * AND THE RESTORATION PHASE STAYS AWAY. The proximity weight is 0.2 against
//     an h-slope of 0.8 at `x_k`, so the wrapper's minimizer is at x0 = -1,
//     TWO away from `x_k`, and the phase runs from the seed to it without
//     coming back: every point it evaluates keeps |x0 - 1| >= 1.93.
//
// MEASURED IN BOTH CONFIGURATIONS (Debug and Release, clang++, MKL; every run
// records them as a test property, and the fix-round-2b transcripts carry them):
// the ten evaluated points are the SAME ten in both, so
//
//     nearest admissible candidate   1.9375        (both)
//     smallest wrong-centre residual 3.8750e-01    (both)
//     right-centre residual          1.1102e-16 Debug / 1.6653e-16 Release
//
// The wrong-centre side clears its own `10 * kkt_tol` bar by 3.875e+04, and the
// 100x margin below is ASSERTED rather than merely observed. The two configs
// agreeing point-for-point is the property fix round 2 lacked: its start was
// chosen by a Debug-only scan of a route that bifurcates.
namespace {
constexpr double kCentreZeta = 0.2;
// The separation premise, asserted rather than scanned: the fixture's own
// construction puts the nearest admissible candidate at 1.9375.
constexpr double kCentreMinSeparation = 1.0;
// And the wrong-centre verdict's margin over the bar it is asserted against.
constexpr double kCentreWrongCentreMargin = 100.0;

// THE FIXTURE ITSELF.
//
//     min  x1 + x1^4   s.t.  cE(x) = 0.1 (x0 + 3)^2 + 0.05 = 0,   x_start = (1, 0)
//
// cE is bounded below by 0.05, so the NLP is INFEASIBLE and the solve ends in
// restoration; h(x) = cE(x) depends on x0 alone, with h(x_start) = 1.65.
//
// MAJOR 0, HAND-DERIVED (every number above comes from here).
//   J = (0.2 (x0 + 3), 0) = (0.8, 0) at the start, so the linearized equality
//   forces p0 = -1.65/0.8 = -2.0625 and leaves p1 to the objective. The
//   Lagrangian Hessian is sigma * diag(0, 12 x1^2) + lambda * diag(0.2, 0),
//   which is ZERO at the start (x1 = 0, lambda = 0), so the subproblem is an LP
//   in p1 and p1 saturates the radius at -8.
//     trial      x = (-1.0625, -8),  h = 0.42539  (< 1.65: the guard TAKES it)
//     pred_df    = -g^T p = 8        (>= 0.999 * 1.65^2 = 2.7198: f-TYPE)
//     actual_df  = 0 - 4088 = -4088  (< 1e-4 * 8: Eq. (11) REFUSES)
//   so the verdict is kReject, rejections_at_iterate becomes 1, and
//   shrink_hits_floor(8) is true because 8 * 0.5 = 4 < tr_min = 5. The floor
//   site raises the request and offers the trial it just refused.
//
// THE RESTORATION PHASE, hand-derived the same way. The wrapper minimizes
// cE(x) + (zeta/2) ||D (x - x_k)||^2 with D = I (both components of x_k are <= 1
// in modulus), i.e. 0.1 (x0+3)^2 + 0.05 + 0.1 [(x0-1)^2 + x1^2], whose minimizer
// is x* = ((zeta - 6a)/(2a + zeta), 0) = (-1, 0) at a = 0.1, zeta = 0.2 -- two
// away from `x_k`, where h = 0.45 is still far above feas_tol. The phase reaches
// it in three majors through (-0.9375, 0), so the closest it ever comes to `x_k`
// is 1.9375, and the solve exits kInfeasible UNCERTIFIED: at a displacement of
// two the uncorrected residual is `zeta * 2 = 0.4`, which is what M2b's gate
// refuses (see the EXPECT_FALSE below).
class SeparatedRestorationModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double t = x(1);
        return t + t * t * t * t;
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 0.0, 1.0 + 4.0 * x(1) * x(1) * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << kSepA * (x(0) + kSepShift) * (x(0) + kSepShift) + kSepFloor;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * kSepA * lambda_e(0);
        h.insert(1, 1) = obj_scale * 12.0 * x(1) * x(1);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 2.0 * kSepA * (x(0) + kSepShift);
        j.insert(0, 1) = 0.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 1.0, 0.0;
        return x;
    }

    static constexpr double kSepA = 0.1;
    static constexpr double kSepShift = 3.0;
    static constexpr double kSepFloor = 0.05;
};

// The same model, WATCHING: `eval_f` and `eval_ce` are what every values-or-full
// query goes through, so the recorded list contains every point the solve --
// the restoration phase included -- ever measured. Recovered from outside the
// driver exactly as WatchedCircleLineModel's list is.
class WatchedSeparatedRestorationModel final : public SeparatedRestorationModel {
  public:
    double eval_f(const Vec &x) const override {
        points.push_back(x);
        return SeparatedRestorationModel::eval_f(x);
    }
    Vec eval_ce(const Vec &x) const override {
        points.push_back(x);
        return SeparatedRestorationModel::eval_ce(x);
    }

    mutable std::vector<Vec> points;
};
} // namespace

TEST(SqpDriverRestorationProximity, TheProductionCentreIsTheRequestingIterateNotTheSeed) {
    WatchedSeparatedRestorationModel model;
    SqpOptions opts;
    opts.max_iter = 200;
    // THE FLOOR, RAISED: this is what makes the offered candidate O(1) from the
    // requesting iterate instead of a collapsed radius away from it.
    opts.tr_init = 8.0;
    opts.tr_min = 5.0;
    opts.restoration_proximity = kCentreZeta;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);

    // THE PREMISE THAT MAKES THE TWO CENTRES DISTINGUISHABLE AT ALL: the guard
    // FIRED, so the wrapper's construction point is NOT the requesting iterate,
    // and the phase ran once from it.
    ASSERT_EQ(1, seeded_rows(sol)) << "the fixture's premise: the guard took the offered trial";
    ASSERT_EQ(1, sol.counters.restoration_entries);
    ASSERT_GE(sol.counters.restoration_iters, 1);
    const SqpIterate *row = seeded_row(sol);
    ASSERT_NE(nullptr, row);
    EXPECT_EQ(StepVerdict::kReject, row->verdict)
        << "kReject on the REQUESTING row is the radius floor's own signature";
    EXPECT_EQ(SolveStatus::kInfeasible, sol.status);
    // AND THE CLAIM IS WITHHELD, which is this separation's own corollary and a
    // second reading of it: what the sub-solve's kOptimal certifies is
    // stationarity of `h + P`, and `||grad P||_inf = zeta * 2 = 0.4` here
    // against a `kkt_tol` of 1e-6, so the M2b gate refuses the certificate
    // while the status stays kInfeasible (solver_status.h's two shapes). A
    // fixture whose two centres genuinely separate CANNOT also certify, and
    // this line says so rather than leaving the reader to wonder.
    EXPECT_FALSE(sol.infeasibility_certified)
        << "at an O(1) proximity displacement the UNCORRECTED residual is zeta * 2, so the "
           "M2b gate refuses the claim -- the separation and the refusal are one fact";

    // AND `x_k` IS THE START POINT, because the request is at major 0 -- pinned
    // against the S5 record's entry half rather than recovered by search.
    const SeparatedRestorationModel plain;
    const Vec x_k = plain.start_point();
    const NlpEval ev_k = eval_nlp(plain, x_k);
    ASSERT_NEAR(constraint_violation_l1(ev_k), sol.counters.restoration_h_in, 1e-12)
        << "the excursion record's entry half is h at the START point: the request is major 0's";
    ASSERT_NEAR(ev_k.f, sol.counters.restoration_f_in, 1e-12);

    // `s = J^T lambda - z` at the RETURNED point, from the driver's own reported
    // multipliers and bound price: the certificate's own vector, before any
    // proximity gradient is added back to it.
    Vec s = -sol.z;
    s += Eigen::MatrixXd(plain.eval_jac_e(sol.x)).transpose() * sol.lambda_e;
    const auto wrapper_residual = [&](const Vec &centre) {
        double r = 0.0;
        for (Index i = 0; i < sol.x.size(); ++i) {
            const double a = std::abs(centre(i));
            const double d = a > 1.0 ? 1.0 / a : 1.0;
            r = std::max(r, std::abs(s(i) + kCentreZeta * d * d * (sol.x(i) - centre(i))));
        }
        return r;
    };

    Index rivals = 0;
    double closest_rival = std::numeric_limits<double>::infinity();
    double nearest_candidate = std::numeric_limits<double>::infinity();
    for (const Vec &x_c : model.points) {
        const double dist = (x_c - x_k).lpNorm<Eigen::Infinity>();
        if (dist <= 0.0) {
            continue; // the requesting iterate itself is not a rival centre
        }
        const NlpEval ev = eval_nlp(plain, x_c);
        if (!(constraint_violation_l1(ev) < sol.counters.restoration_h_in)) {
            continue; // not a point the guard's own test would have started from
        }
        ++rivals;
        closest_rival = std::min(closest_rival, wrapper_residual(x_c));
        nearest_candidate = std::min(nearest_candidate, dist);
    }

    // THE SEPARATION PREMISE, ASSERTED. Fix round 2 SKIPPED a start that failed
    // this and moved on; the fixture is now built so that failing it is a
    // regression in the fixture, which must be re-derived and declared.
    ASSERT_GE(rivals, 1);
    ASSERT_GE(nearest_candidate, kCentreMinSeparation)
        << "the fixture's construction puts every admissible candidate O(1) from the requesting "
           "iterate; a nearer one means the phase or the floor moved";

    // THE VERDICT, two-sided.
    const double at_x_k = wrapper_residual(x_k);
    EXPECT_LE(at_x_k, opts.kkt_tol)
        << "the returned point IS stationary for the wrapper centred on the REQUESTING "
           "iterate -- which is what a kOptimal sub-solve on that wrapper means, and is the "
           "half a regression to `*x_start` would break";
    EXPECT_GT(closest_rival, 10.0 * opts.kkt_tol)
        << "and it is NOT stationary for a wrapper centred on any candidate the guard could "
           "have started from -- `*x_start` among them";
    // AND THE MARGIN IS THE ASSERTION, not an observation left in a comment: a
    // fixture that drifts to where the two centres barely differ fails HERE.
    EXPECT_GE(closest_rival, kCentreWrongCentreMargin * 10.0 * opts.kkt_tol)
        << "the wrong-centre side must clear its bar by " << kCentreWrongCentreMargin << "x";
    ::testing::Test::RecordProperty("centre",
                                    fmt::format("zeta={:g} at_x_k={:.4e} closest_rival={:.4e} "
                                                "nearest_candidate={:.4e} rivals={} kkt_tol={:g}",
                                                kCentreZeta, at_x_k, closest_rival,
                                                nearest_candidate, rivals, opts.kkt_tol));
}

// =========================================================================
// THE CLAMPED START (M7 W2 ladder, L2). An entry outside the box (E2-D3:
// YORKNET's CUTEst x0, 40 components) made the sub-solve's first QP
// box-infeasible. The wrapper's start is the entry CLAMPED into the box, with
// the rows re-read there; an in-box entry is the wrapper that shipped.
// =========================================================================
namespace {
// min 0  s.t.  x0 + x1 - 1 = 0,  x0 - x1 - 3 <= 0,  0 <= x <= 5.
class BoxedLineModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &) const override { return 0.0; }
    Vec eval_grad(const Vec &) const override { return Vec::Zero(2); }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, x(0) + x(1) - 1.0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(0) - x(1) - 3.0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = -1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Zero(2);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 5.0);
        return u;
    }
    Vec start_point() const override { return Vec::Ones(2); }
};
} // namespace

TEST(RestorationModelClampedStart, AnOutOfBoxEntryStartsClampedWithItsRowsReadThere) {
    BoxedLineModel model;
    Vec x_entry(2);
    x_entry << -2.0, 6.0; // both outside [0, 5]; the rows read 3 here, 4 at (0, 5)
    const NlpEval ev = eval_nlp(model, x_entry);
    ASSERT_DOUBLE_EQ(constraint_violation_l1(ev), 3.0);
    const RestorationModel r(model, x_entry, ev);

    EXPECT_EQ(r.start_clamps(), 2);
    const Vec y = r.start_point();
    ASSERT_EQ(y.size(), 5); // (x0, x1, sp, sm, si)
    EXPECT_EQ(y(0), 0.0);
    EXPECT_EQ(y(1), 5.0);
    EXPECT_EQ(y(2), 0.0); // sp
    EXPECT_EQ(y(3), 4.0); // sm: cE(0, 5) = 4, sigma 1
    EXPECT_EQ(y(4), 0.0); // si: cI(0, 5) = -8
    // The start is inside the wrapper's box and satisfies its rows.
    for (Index i = 0; i < y.size(); ++i) {
        EXPECT_GE(y(i), r.lower()(i));
        EXPECT_LE(y(i), r.upper()(i));
    }
    EXPECT_DOUBLE_EQ(r.eval_ce(y)(0), 0.0);
    EXPECT_LE(r.eval_ci(y)(0), 0.0);
    // Its objective is h at the CLAMPED point, not at the entry.
    EXPECT_DOUBLE_EQ(r.eval_f(y), 4.0);
    // At zeta > 0 the centre is clamped too: the proximity gradient vanishes there.
    const RestorationModel rp(model, x_entry, ev, x_entry, 2.0);
    EXPECT_EQ(rp.start_clamps(), 2);
    const Vec g = rp.eval_grad(rp.start_point());
    EXPECT_EQ(g(0), 0.0);
    EXPECT_EQ(g(1), 0.0);
}

// A model that cannot be evaluated AT its own bound keeps the entry: the sub-solve
// throws on a non-finite start, and the entry's rows are finite.
TEST(RestorationModelClampedStart, ANonFiniteClampedPointKeepsTheEntry) {
    class SingularAtTheBoundModel : public BoxedLineModel {
      public:
        Vec eval_ce(const Vec &x) const override {
            return Vec::Constant(1, x(0) == 0.0 ? std::numeric_limits<double>::quiet_NaN()
                                                : x(0) + x(1) - 1.0);
        }
    };
    SingularAtTheBoundModel model;
    Vec x_entry(2);
    x_entry << -2.0, 6.0;
    const NlpEval ev = eval_nlp(model, x_entry);
    ASSERT_TRUE(ev.all_finite);
    const RestorationModel r(model, x_entry, ev);

    EXPECT_EQ(r.start_clamps(), 0);
    EXPECT_EQ(r.original_x(r.start_point()), x_entry);
    EXPECT_TRUE(r.start_point().allFinite());
    EXPECT_EQ(r.start_point()(3), 3.0); // sm from the ENTRY's cE = 3
}

TEST(RestorationModelClampedStart, AnInBoxEntryIsTheWrapperThatShipped) {
    BoxedLineModel model;
    Vec x_entry(2);
    x_entry << 1.0, 2.0;
    const NlpEval ev = eval_nlp(model, x_entry);
    const RestorationModel r(model, x_entry, ev);

    EXPECT_EQ(r.start_clamps(), 0);
    // The pre-L2 formula, byte for byte: x_entry, then the slacks from `ev`.
    Vec expect(5);
    expect << x_entry(0), x_entry(1), std::max(0.0, -ev.ce(0)), std::max(0.0, ev.ce(0)),
        std::max(0.0, ev.ci(0));
    EXPECT_EQ(r.start_point(), expect);
    // On a bound is not outside it.
    Vec on_bound(2);
    on_bound << 0.0, 5.0;
    EXPECT_EQ(RestorationModel(model, on_bound, eval_nlp(model, on_bound)).start_clamps(), 0);
}

HVEN_SUPPRESS_DEPRECATED_END
