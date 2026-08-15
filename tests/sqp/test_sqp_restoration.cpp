// tests/test_sqp_restoration.cpp — Task 9: the restoration phase and the
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
// assertions are looser than the observation, per test_sqp_driver.cpp's
// standing convention. The two fixtures' OPTIMA and CERTIFICATES, by contrast,
// are hand-derived in the comments and asserted tightly -- they are properties
// of the problems, not of the run.

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>

#include "support/derivative_check.h"
#include "support/hs_problems.h"

using namespace hven::solvers;
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
// THE CONSTRAINT AND THE START ARE test_sqp_driver.cpp's
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
// FIXTURE 3 -- test_sqp_driver.cpp's InconsistentBoundedModel, copied here
// because this file uses it for a DIFFERENT purpose: its restoration problem
// is the smallest case in the suite where the FEASIBILITY problem's start
// point is ALREADY its solution, which is the normal state of affairs when
// restoration is entered at an infeasible stationary point and is what
// sqp_driver.h's kZeroStepScale short-circuit exists for.
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
// only sqp_types.h's documented claim about what a certified kInfeasible exit
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

SubgradientCertificate check_certificate(const NlpModel &model, const SqpSolution &sol,
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
        // COEFFICIENT is sigma, so the product is the violation itself.
        EXPECT_DOUBLE_EQ(r.eval_f(r.start_point()), constraint_violation_l1(ev));
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
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);

        EXPECT_EQ(sol.status, SqpStatus::kInfeasible);
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
// (sqp_driver.h's MULTIPLIERS ARE NOT CARRIED OUT OF AN OPEN RELAXATION); this
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
// optimal POINT. types.h's WorkingSetLinearAlgebra note carries that caveat
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
        opts.max_iter = 2;
        opts.qp.ws_algebra = algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);
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
    // BOTH ITERATES SIT ON THE SAME OPTIMAL FACE x0 + x1 = 2 ...
    EXPECT_NEAR(x_border(0) + x_border(1), 2.0, 1e-12);
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
        opts.max_iter = 200;
        opts.qp.ws_algebra = algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);
        EXPECT_EQ(sol.status, SqpStatus::kInfeasible);
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

        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);

        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
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
// (test_sqp_driver.cpp), which is the sibling claim for the tier below it.
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
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);
        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
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
// is test_sqp_driver.cpp's SecondOrderInfeasibleModel geometry with the
// ORIGINAL unbounded-along-the-manifold objective -- i.e. this file's
// StalledValleyModel with the +5 x1^2 term removed -- which is precisely the
// case the cap exists for: restoration recovers a feasible point, the
// optimality phase walks back out to a second stall, and a driver without a cap
// would alternate. See sqp_driver.h's ONE RESTORATION PER SOLVE paragraph for
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
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    // The first request is serviced and the solve genuinely resumes...
    EXPECT_EQ(log.resumes.size(), 1u);
    EXPECT_GE(sol.counters.restoration_iters, 1);
    EXPECT_GT(sol.counters.steps_accepted, 1) << "the resumed loop made real progress";
    // ...and the second is reported rather than serviced.
    EXPECT_EQ(sol.status, SqpStatus::kInfeasible);
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
    // leaves kReject on its last row (sqp_driver.h's RESTORATION PHASE note
    // has the corrected decision table; SqpDriverRadius.
    // FloorRaisesTheRestorationRequest exercises that route). So neither
    // field tells the three kInfeasible outcomes apart -- that is exactly why
    // the flag exists (sqp_types.h's SqpSolution::infeasibility_certified).
    EXPECT_GE(sol.counters.restoration_iters, 1);
    EXPECT_LT(sol.counters.major_iters, opts.max_iter)
        << "the cap must END the solve, not let it grind to the budget";
    ::testing::Test::RecordProperty("capped", fmt::format("majors={} restoration={} f={:.10g}",
                                                          sol.counters.major_iters,
                                                          sol.counters.restoration_iters, sol.f));
}

// KLV ALGORITHM 2'S ||d|| = 0 SHORT-CIRCUIT (sqp_driver.h's kZeroStepScale).
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
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(feasibility, feasibility.start_point());

    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
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
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    EXPECT_EQ(sol.status, SqpStatus::kInfeasible);
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
TEST(SqpDriverRadius, FloorRaisesTheRestorationRequest) {
    struct Run {
        double tr_min;
        Index majors = 0;
        double last_radius = 0.0;
    };
    std::vector<Run> runs{{1e-10}, {1e-4}};

    for (Run &run : runs) {
        SCOPED_TRACE(fmt::format("tr_min {:g}", run.tr_min));
        InfeasibleCircleLineModel model(1.0, 0.0);
        SqpOptions opts;
        opts.tr_min = run.tr_min;
        FunnelLog log;
        record_funnel_into(opts, &log);
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);
        run.majors = sol.counters.major_iters;
        ASSERT_FALSE(sol.history.empty());
        run.last_radius = sol.history.back().tr_radius;

        // (1) THE FUNNEL DID NOT ASK. Its signature never fired anywhere in
        // the solve, so every kRestore on this path came from the driver.
        EXPECT_EQ(std::count(log.verdict.begin(), log.verdict.end(), StepVerdict::kRestore), 0)
            << "if the signature fires here this test is measuring the wrong trigger";
        ASSERT_FALSE(log.verdict.empty());
        EXPECT_EQ(log.verdict.back(), StepVerdict::kReject);
        EXPECT_TRUE(log.resumes.empty()) << "this fixture is infeasible; there is nothing to "
                                            "resume to";

        // (2) THE FLOOR HELD, AND WAS REACHED. Every radius is >= tr_min, and
        // the last one is within one halving of it -- which is exactly the
        // condition the driver escalates on (the NEXT shrink would cross).
        for (const SqpIterate &h : sol.history) {
            EXPECT_GE(h.tr_radius, opts.tr_min) << "trial " << h.trial;
        }
        EXPECT_LT(run.last_radius * 0.5, opts.tr_min)
            << "the request must be raised AT the floor, not somewhere above it";

        // And the phase ran, and produced the same verdict and the same
        // certificate as the elastic tier's route does (InfeasibleNlpCertifies).
        EXPECT_EQ(sol.status, SqpStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified);
        EXPECT_GE(sol.counters.restoration_iters, 1);
        const SubgradientCertificate c = check_certificate(model, sol, opts.feas_tol);
        EXPECT_NEAR(c.h, 2.0 - std::sqrt(2.0), 1e-6);
        EXPECT_LE(c.stationarity, opts.kkt_tol);
        EXPECT_LE(c.selector, 1e-9);
    }

    // (3) tr_min IS THE KNOB. Observed: 60 majors at 1e-10, 40 at 1e-4 -- the
    // difference is exactly the log2(1e6)/1 = 20 halvings that the higher
    // floor does not spend.
    EXPECT_LT(runs[1].majors, runs[0].majors);
    EXPECT_GT(runs[1].last_radius, runs[0].last_radius);
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
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);

        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
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
        EXPECT_THROW(SqpDriver{opts}, std::invalid_argument);
    }
    {
        SqpOptions opts;
        opts.tr_min = -1.0;
        EXPECT_THROW(SqpDriver{opts}, std::invalid_argument);
    }
    {
        SqpOptions opts;
        opts.tr_min = std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(SqpDriver{opts}, std::invalid_argument);
    }
    {
        SqpOptions opts; // tr_init = 1.0
        opts.tr_min = 2.0;
        EXPECT_THROW(SqpDriver{opts}, std::invalid_argument);
    }
    {
        SqpOptions opts; // a +inf tr_init still has to clear tr_max
        opts.tr_init = std::numeric_limits<double>::infinity();
        opts.tr_max = 1.0;
        opts.tr_min = 2.0;
        EXPECT_THROW(SqpDriver{opts}, std::invalid_argument);
    }
    { // and the default triple is admissible
        EXPECT_NO_THROW(SqpDriver{SqpOptions{}});
    }
}
