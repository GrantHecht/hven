// tests/test_b1_gate.cpp — PHASE-5 TASK 7b: THE REGRESSION CORPUS FOR THE B-1
// REPAIR (sqp_driver.h's THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY).
//
// =====================================================================
// THE DEFECT, AND WHY THIS FILE EXISTS AT ALL
//
// A warm-started solve could report kOptimal, IN ZERO MAJORS, at a point that
// is not a KKT point of the problem it was asked to solve. The convergence
// test gates `stationarity <= kkt_tol && feasibility <= feas_tol` and RECORDS
// complementarity without gating it, justified by an identity about THE
// SUBPROBLEM'S OWN complementarity -- and that test runs at the TOP of the
// major loop, on multipliers ingested from a solve of a DIFFERENT problem,
// before this solve has any subproblem for the identity to be about. When a
// previously ACTIVE inequality goes STRICTLY SLACK at the new parameter while
// its stale multiplier still zeroes the Lagrangian gradient, both gated
// quantities pass and the solve certifies the OLD answer having done nothing.
//
// Full analysis: docs/notes/2026-07-31-nonconvex-sweep-adjudications.md §1.
// The repair: sqp_driver.h clears any ingested lambda_i whose row is not
// GEOMETRICALLY ACTIVE at the ingested x (cI_j(x) >= -feas_tol), the same
// distance test and the same tolerance evaluate_kkt already applies to bounds.
//
// =====================================================================
// THE EXPOSURE CRITERION -- WHAT THIS CORPUS HAD TO CONTAIN, AND WHY
//
// > A warm ingest is exposed exactly when moving p moves NO quantity the
// > convergence test GATES -- neither stationarity nor feasibility at the
// > ingested point -- while it does move one the test only RECORDS
// > (general-inequality complementarity).
//
// **THAT "NO" IS A DISJUNCTION, ROW-WISE OVER EVERY CHANNEL p ENTERS
// THROUGH**, and it is the trap a corpus designer falls into. ONE COUPLED
// GATE RESCUES A MODEL WHOSE INEQUALITY ROW IS OTHERWISE EXPOSED. F4 of
// tests/sqp/support/parametric_families.h is exactly that near-miss: its cI row IS
// B-1-shaped in isolation (relaxing, stale positive price), and it survives
// only because the same scalar a(p) simultaneously moves an equality row
// (feasibility fires at |Delta a|) and a lower bound (the released bound gives
// a stationarity residual of exactly a_old). Those two rescues are
// independently sufficient AS MECHANISM, but both trip on |Delta a| > feas_tol
// -- so they are NOT redundant within that band, and a fixture whose immunity
// rests on either would be pinning a margin rather than a mechanism. A family
// like F4 passes a repair and a non-repair ALIKE and therefore demonstrates
// nothing. Hence B1MinimalRelease below, whose whole point is that a RELAXING
// INEQUALITY IS THE SOLE THING p TOUCHES -- asserted, in
// TheMinimalModelMovesNoGatedQuantityWithP, rather than claimed.
//
// CONVEXITY IS IRRELEVANT and me-vs-mi is not the criterion either. Two of the
// three reproducers below are convex.
//
// =====================================================================
// THE SEVEN ELEMENTS THIS FILE AND ITS SIBLING CARRY
//
//   1. The three broken HS sweep specs, flipped to sound
//                                       -- tests/test_hs_sweeps.cpp (sibling).
//   2. F6PathBoundQuadrature, a SHIPPED convex family, both directions -- the
//      criterion demonstrating itself                        -- (5)/(6) below.
//   3. The Task-7 review's convex n = 3 sphere class, reconstructed and cited
//                                                          -- (3)/(4) below.
//   4. A TRANSFER-PRODUCED WarmStart into the B-1 exit -- ESTABLISHED VACUOUS
//      and pinned as such                                       -- (7) below.
//   5. Equality-only controls: bit-identity across the repair    -- (8) below.
//   6. LEGITIMATE zero-major exits (Phase-5 Task 0's O-1 feature) still
//      certify, INCLUDING with a live inequality price          -- (9) below.
//   7. A minimal problem where a RELAXING INEQUALITY is the SOLE thing p
//      touches                                            -- (1)/(2)/(10) below.
//
// =====================================================================
// WHAT THE REPAIR DOES **NOT** COVER, AND WHY NOTHING HERE PINS IT
//
// The clear restores three of the four KKT conditions at the ingested point --
// stationarity and feasibility are GATED by the convergence test,
// complementarity is established BY CONSTRUCTION. **DUAL FEASIBILITY
// (lambda_i >= 0) IS NEITHER RESTORED NOR GATED ANYWHERE**: evaluate_kkt folds
// a sign-consistency residual into the reduced stationarity measure for BOUNDS
// only, and a general inequality row enters grad_lag unconditionally with no
// sign test. It is an INGEST PRECONDITION -- warm_start.h's SIGN CONVENTIONS
// paragraph -- and every test below assumes a WarmStart that honours it.
//
// AND THAT PRECONDITION HAS A LIVE IN-REPO PRODUCER, not only hand-assembly:
// warm_start.h's from_interior_point COPIES THE CALLER'S lambda_i VERBATIM (its
// dual_tol filter governs working-set MEMBERSHIP, not the price), so an
// interior-point crossover taken before full convergence can carry a negative
// price on a GEOMETRICALLY ACTIVE row -- which is exactly the configuration the
// clear leaves alone, since the clear only zeroes STRICTLY SLACK rows.
// tests/test_warm_start.cpp's WarmStart.CrossoverWrongSignDualLeavesRowFree
// ships a fixture of that shape (lambda_i = -1e-8 against a slack of -1e-9).
// The magnitude is bounded by the producer's sign violation and is orders below
// kkt_tol, which is why the repair's verdict is unchanged.
//
// An object that violates the convention is out of contract and is not defended
// against: measured, an ingest at x = 0 carrying lambda_i = (-1, +0.5) on
// `min x s.t. x <= 0, -x - 1 <= 0` certifies kOptimal in ZERO majors at f = 0
// where the truth is f = -1, BECAUSE the clear removed the +0.5 that had been
// the only thing breaking stationarity (Task-7b review, probe P2). **That is a
// PRE-EXISTING ungated condition with UNCHANGED reachability** -- the identical
// false certificate is reachable without the clear from lambda_i = (-1, 0) --
// so it is deliberately NOT fixtured here: a regression corpus for B-1 pinning
// a different, older hole would misattribute it. It is carried as O-B1-4 in
// docs/notes/2026-07-31-nonconvex-sweep-adjudications.md §7.5, whose fix (an
// ingest-time lambda_i >= 0 validation) belongs to a hardening task rather than
// to this repair.
//
// =====================================================================
// BACKEND. B-1 is an arithmetic-free control-flow property -- a stale
// multiplier zeroing a gradient that does not depend on the factorization --
// so every assertion in this file is expected to hold on Apple Accelerate
// unchanged. Nothing here is pinned on a trajectory-sensitive count.

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/warmstart/continuation.h>
#include <hven/detail/warmstart/mesh_transfer.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

#include "support/hs_problems.h"
#include "support/nlp_kkt_check.h"
#include "support/parametric_families.h"

namespace hven::solvers {
namespace {

using test_support::F6PathBoundQuadrature;
using test_support::make_hs;
using test_support::self_check_kkt;
using test_support::trapezoid_weights;
using test_support::uniform_nodes;

constexpr double kInf = 1e20;
constexpr double kTol = 1e-6;

// The options every B-1 probe shares. adaptive_mu is OFF for the reason the
// warm-start battery gives: the schedule drives dual_mu off the KKT residual,
// which differs between two consecutive solves, and that makes two arms differ
// in more than the one thing under test.
SqpOptions probe_options(StartLevel level = StartLevel::kWarm) {
    SqpOptions opts;
    opts.kkt_tol = kTol;
    opts.feas_tol = kTol;
    opts.max_iter = 60;
    opts.adaptive_mu = false;
    opts.start_level = level;
    return opts;
}

// =====================================================================
// THE MINIMAL MODEL -- corpus element 7.
//
//     min  f(x) = 1/2 (x - 2)^2        s.t.  cI(x) = x - p <= 0,   n = 1
//
// no equalities, NO FINITE BOUNDS, and an objective that does not mention p.
// So p enters through EXACTLY ONE channel, a single inequality row, as a
// constant shift -- the one row of the criterion table that is exposed, with
// nothing else moving to rescue it. This is the shape F4 only LOOKS like.
//
// THE ANALYTIC PATH, which makes every assertion below an oracle rather than a
// cross-arm comparison:
//     x*(p) = min(2, p),   lambda*(p) = max(0, 2 - p),   f*(p) = 1/2 (x* - 2)^2.
// For p < 2 the row is active and its price is strictly positive; for p >= 2
// the unconstrained minimizer is feasible and the price is zero.
//
// WHY THE STALE PRICE IS EXACTLY CANCELLING. At the p_old solution
// x = p_old < 2, grad f = p_old - 2 and Ji = 1, so lambda = 2 - p_old zeroes
// gL identically. Raise p: the row goes slack (cI = p_old - p_new < 0) and
// NEITHER gated quantity moves -- gL is unchanged because neither grad f nor
// Ji depends on p, and the point is strictly feasible. Only complementarity
// moves, from 0 to (2 - p_old)(p_new - p_old).
class B1MinimalRelease : public ParametricNlpModel {
  public:
    explicit B1MinimalRelease(double p0) : p_(p0) {}

    Index n() const override { return 1; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return 0.5 * (x(0) - 2.0) * (x(0) - 2.0); }
    Vec eval_grad(const Vec &x) const override { return Vec::Constant(1, x(0) - 2.0); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(0) - p_); }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(1, 1);
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
    Vec start_point() const override { return Vec::Constant(1, 0.0); }

    Index parameter_dim() const override { return 1; }
    Vec parameters() const override { return Vec::Constant(1, p_); }
    void set_parameters(const Vec &p) override { p_ = p(0); }

    static double x_star(double p) { return std::min(2.0, p); }
    static double f_star(double p) {
        const double d = x_star(p) - 2.0;
        return 0.5 * d * d;
    }
    static double lambda_star(double p) { return std::max(0.0, 2.0 - p); }

  private:
    double p_;
    Vec lower_ = Vec::Constant(1, -kInf);
    Vec upper_ = Vec::Constant(1, kInf);
};

// =====================================================================
// THE TASK-7 REVIEW'S OWN REPRODUCTION -- corpus element 3, reconstructed from
// task-7-review.md's B-1 Verification §2 and cited to it. Deliberately CONVEX,
// n = 3, a sphere rather than an ellipse, written by the reviewer on a problem
// the Task-7 implementer never saw:
//
//     min  -x1 - x2 - x3     s.t.  ||x||^2 - 1 - p <= 0,   -5 <= x <= 5
//     x*(p) = (1,1,1) sqrt((1+p)/3),  f*(p) = -sqrt(3(1+p)),
//     lambda*(p) = sqrt(3) / (2 sqrt(1+p)).
//
// The reviewer measured: warm from p = 0 to p = 0.5 returns kOptimal in ZERO
// majors with start_level_used = kWarm, stationarity 4.9e-09, primal 0 and
// COMPLEMENTARITY 4.33e-01, at an objective 18.35 % from truth; and a real
// run_continuation sweep p: 0 -> 1 froze the objective at -1.73205081 against
// a truth of -2.44948974 (29.3 % error) while the step controller GREW dp,
// because the false solves cost 0 majors and so beat target_majors.
//
// The BOUNDS ARE NOT A RESCUE HERE and that is why they are kept: x*(p) is
// interior to [-5, 5]^3 over the whole probed range, so no bound is ever
// active and the geometric bound handling has nothing to fire on.
class B1ReviewSphere : public ParametricNlpModel {
  public:
    explicit B1ReviewSphere(double p0) : p_(p0) {}

    Index n() const override { return 3; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return -x.sum(); }
    Vec eval_grad(const Vec &) const override { return Vec::Constant(3, -1.0); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        return Vec::Constant(1, x.squaredNorm() - 1.0 - p_);
    }

    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &li) const override {
        SpMatRM h(3, 3);
        for (int i = 0; i < 3; ++i) {
            h.insert(i, i) = 2.0 * li(0); // f is linear; only the row contributes
        }
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 3);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 3);
        for (int i = 0; i < 3; ++i) {
            j.insert(0, i) = 2.0 * x(i);
        }
        j.makeCompressed();
        return j;
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Constant(3, 0.1); }

    Index parameter_dim() const override { return 1; }
    Vec parameters() const override { return Vec::Constant(1, p_); }
    void set_parameters(const Vec &p) override { p_ = p(0); }

    static double f_star(double p) { return -std::sqrt(3.0 * (1.0 + p)); }

  private:
    double p_;
    Vec lower_ = Vec::Constant(3, -5.0);
    Vec upper_ = Vec::Constant(3, 5.0);
};

// A two-solve warm chain: solve cold at p_from, then warm-solve at p_to from
// that solve's own hand-off. This is the ONE pattern every reproduction below
// uses, so "warm" means the same thing everywhere in this file.
struct WarmLink {
    SqpSolution first, second;
};

WarmLink warm_link(ParametricNlpModel &model, double p_from, double p_to, const SqpOptions &opts) {
    WarmLink out;
    model.set_parameters(Vec::Constant(1, p_from));
    SqpDriver cold(probe_options(StartLevel::kCold));
    out.first = cold.solve(model, model.start_point());
    model.set_parameters(Vec::Constant(1, p_to));
    SqpDriver warm(opts);
    out.second = warm.solve(model, out.first.x, out.first.warm_start);
    return out;
}

// =====================================================================
// PHASE-6 TASK 5: THE SEEDED ARMS.
//
// StartLevel::kSeeded (warm_start.h) ingests x, the duals and the activity hint
// from an object with NO USABLE HASH -- a mesh transfer, an interior-point
// crossover, or anything a caller assembled. That is a SECOND ROUTE INTO THE
// EXACT EXIT THIS FILE EXISTS FOR, and a repair that carried only to the kWarm
// route would leave the defect alive on it. Nothing about B-1 is
// level-specific -- sqp_driver.h's clear is gated on `warm_ingest`, which is
// true from kSeeded up -- but "nothing about it is level-specific" is an
// argument, and this file's standing discipline is to pin mechanisms rather
// than argue them.
//
// THE ARM IS THE SAME CHAIN WITH THE HASH ERASED. `seeded_link` is
// `warm_link` with one line added: the hand-off's structure_hash is set to 0
// before it is fed back, which is exactly what mesh_transfer.h and
// from_interior_point emit by construction. Everything else -- the models, the
// parameter steps, the options, the analytic oracles -- is identical, so a
// difference between the two arms can only be the level.
WarmLink seeded_link(ParametricNlpModel &model, double p_from, double p_to,
                     const SqpOptions &opts) {
    WarmLink out;
    model.set_parameters(Vec::Constant(1, p_from));
    SqpDriver cold(probe_options(StartLevel::kCold));
    out.first = cold.solve(model, model.start_point());
    WarmStart hashless = out.first.warm_start;
    hashless.structure_hash = 0; // the mesh-transfer / crossover sentinel
    model.set_parameters(Vec::Constant(1, p_to));
    SqpDriver seeded(opts);
    out.second = seeded.solve(model, out.first.x, hashless);
    return out;
}

// =====================================================================
// (1) THE PRECONDITION OF CORPUS ELEMENT 7, ASSERTED RATHER THAN CLAIMED.
//
// The criterion is a DISJUNCTION, so a fixture is only worth anything if NO
// gated quantity moves with p. For B1MinimalRelease that is checkable exactly:
// grad f, the Jacobian and the bounds are p-independent, there are no equality
// rows, and cI moves by exactly -(p_new - p_old). This test is what entitles
// (2) to attribute its result to the inequality row and nothing else.
// =====================================================================
TEST(B1Gate, TheMinimalModelMovesNoGatedQuantityWithP) {
    B1MinimalRelease model(1.0);
    const Vec x = Vec::Constant(1, 0.37);

    const Vec g0 = model.eval_grad(x);
    const Vec ci0 = model.eval_ci(x);
    const Vec lo0 = model.lower(), up0 = model.upper();
    const auto ji0 = model.eval_jac_i(x);

    ASSERT_EQ(0, model.me()) << "no equality row exists to move";
    ASSERT_EQ(1, model.mi());
    EXPECT_FALSE(std::isfinite(lo0(0)) && std::abs(lo0(0)) < kInf)
        << "no finite bound exists for the geometric bound handling to fire on";

    model.set_parameters(Vec::Constant(1, 1.9));
    // STATIONARITY's inputs: grad f and Ji. Neither moves.
    EXPECT_EQ(g0, model.eval_grad(x)) << "grad f must not depend on p";
    EXPECT_EQ(0.0, Eigen::MatrixXd(ji0 - model.eval_jac_i(x)).cwiseAbs().maxCoeff())
        << "Ji must not depend on p";
    // FEASIBILITY's inputs: cE (none), the bounds, and cI. Only cI moves, and
    // it moves in the RELAXING direction.
    EXPECT_EQ(lo0, model.lower());
    EXPECT_EQ(up0, model.upper());
    EXPECT_NEAR(ci0(0) - 0.9, model.eval_ci(x)(0), 1e-15)
        << "cI is the sole channel, and it relaxes";
}

// =====================================================================
// (2) CORPUS ELEMENT 7 ITSELF -- the minimal B-1 exit, repaired.
//
// PRE-REPAIR (measured on the same fixture at BASE commit 0fbdca6): the second
// solve returned kOptimal in ZERO majors at x = 1, f = 0.5, with stationarity
// 0, primal 0 and COMPLEMENTARITY 0.5 -- against a truth of x = 1.5, f = 0.125,
// i.e. a 300 % relative objective error reported as success.
// =====================================================================
TEST(B1Gate, MinimalReleaseNoLongerCertifiesTheOldPoint) {
    B1MinimalRelease model(0.0);
    const WarmLink link = warm_link(model, 1.0, 1.5, probe_options());

    // The p = 1 solve is the one whose price goes stale: the row is active and
    // its multiplier is strictly positive and exactly cancelling.
    ASSERT_EQ(SqpStatus::kOptimal, link.first.status);
    EXPECT_NEAR(1.0, link.first.x(0), 1e-8);
    ASSERT_EQ(1, link.first.lambda_i.size());
    EXPECT_NEAR(B1MinimalRelease::lambda_star(1.0), link.first.lambda_i(0), 1e-8)
        << "lambda = 2 - p = 1 zeroes gL at x = 1, for EVERY p";

    // The warm solve is a real ingest -- not a silently-cold one.
    EXPECT_EQ(StartLevel::kWarm, link.second.counters.start_level_used);
    EXPECT_EQ(SqpStatus::kOptimal, link.second.status);
    // THE REGRESSION: it no longer certifies the old point in zero majors.
    EXPECT_GT(link.second.counters.major_iters, 0)
        << "the released row's stale price is cleared, stationarity reads "
           "|grad f| = 0.5 against kkt_tol, and the solve has work to do";
    EXPECT_NEAR(B1MinimalRelease::x_star(1.5), link.second.x(0), 1e-7);
    EXPECT_NEAR(B1MinimalRelease::f_star(1.5), link.second.f, 1e-7);

    // AND THE CERTIFICATE IS COMPLETE: complementarity is the term B-1
    // destroyed, and the self-check recomputes it from the model rather than
    // reading the driver's own number.
    const test_support::NlpKktResidual r = self_check_kkt(model, link.second, kTol);
    EXPECT_LT(r.stationarity, 1e-6);
    EXPECT_LT(r.primal, 1e-6);
    EXPECT_LT(r.dual_sign, 1e-9);
    EXPECT_LT(r.complementarity, 1e-6) << "pre-repair this read 0.5";
}

// The other half of element 7: a release LARGE enough that the row stays slack
// at the answer. The returned quadruple must then carry lambda_i == 0 EXACTLY
// on that row -- which is the repair's stronger claim (sqp_driver.h's point
// (2)): the ingested multipliers are not merely refused, they are corrected to
// the ones the KKT system actually asks for.
TEST(B1Gate, AFullyReleasedRowExitsWithAnExactlyZeroPrice) {
    B1MinimalRelease model(0.0);
    const WarmLink link = warm_link(model, 1.0, 3.0, probe_options());

    ASSERT_EQ(SqpStatus::kOptimal, link.second.status);
    EXPECT_NEAR(2.0, link.second.x(0), 1e-7) << "the unconstrained minimizer is feasible at p = 3";
    ASSERT_EQ(1, link.second.lambda_i.size());
    EXPECT_EQ(0.0, link.second.lambda_i(0))
        << "a strictly slack row prices at exactly zero, not merely at roundoff";
    const test_support::NlpKktResidual r = self_check_kkt(model, link.second, kTol);
    EXPECT_LT(r.complementarity, 1e-12);
}

// =====================================================================
// (3) CORPUS ELEMENT 3 -- the Task-7 review's convex n = 3 sphere.
//
// THE REVIEWER'S NO-STEP-SIZE-AVOIDS-IT FINDING IS WHAT THE THREE dp VALUES
// PIN: pre-repair, dp = 0.5 / 0.05 / 0.001 all returned 0 majors with
// complementarity 4.33e-1 / 4.33e-2 / 8.66e-4 respectively -- every one of
// them orders of magnitude above kkt_tol = 1e-6, so "take smaller continuation
// steps" was never a mitigation. Post-repair every one of them must land on
// the analytic path instead.
// =====================================================================
TEST(B1Gate, ReviewSphereAgreesWithItsAnalyticPathAtEveryStepSize) {
    for (double dp : {0.5, 0.05, 0.001}) {
        SCOPED_TRACE(fmt::format("dp = {}", dp));
        B1ReviewSphere model(0.0);
        const WarmLink link = warm_link(model, 0.0, dp, probe_options());

        ASSERT_EQ(SqpStatus::kOptimal, link.first.status);
        EXPECT_NEAR(B1ReviewSphere::f_star(0.0), link.first.f, 1e-6);

        EXPECT_EQ(StartLevel::kWarm, link.second.counters.start_level_used);
        ASSERT_EQ(SqpStatus::kOptimal, link.second.status);
        EXPECT_NEAR(B1ReviewSphere::f_star(dp), link.second.f, 1e-6)
            << "pre-repair the objective stayed frozen at f*(0) = -sqrt(3)";
        const test_support::NlpKktResidual r = self_check_kkt(model, link.second, kTol);
        EXPECT_LT(r.complementarity, 1e-6)
            << "pre-repair: 4.33e-1 at dp = 0.5, 8.66e-4 at dp = 0.001";
    }
}

// =====================================================================
// (4) THE REVIEWER'S OTHER FINDING -- A REAL run_continuation SWEEP DOES NOT
// ESCAPE IT, AND THE DEFECT ACCELERATED ITS OWN SWEEP: the adaptive controller
// GREW dp (0.05 -> 0.1 -> 0.2 -> 0.4) precisely because the false solves cost
// 0 majors and therefore beat target_majors, so the sweep reported
// reached_p1 = true with a 29.3 % objective error at p = 1. The identical
// sweep at start_level = kCold had worst relative error 6.2e-10.
//
// This is the end-to-end statement, on the shipped path, with the shipped
// adaptive controller -- deliberately NOT the pinned-grid configuration the HS
// corpus uses, because the controller's reaction to a zero-major solve is half
// of what made the defect dangerous.
// =====================================================================
TEST(B1Gate, ReviewSphereContinuationSweepTracksTheTruePath) {
    for (StartLevel level : {StartLevel::kWarm, StartLevel::kCold}) {
        SCOPED_TRACE(level == StartLevel::kWarm ? "warm" : "cold");
        B1ReviewSphere model(0.0);
        SqpDriver driver(probe_options(level));
        ContinuationOptions copts;
        copts.dp_init = 0.05;
        copts.use_predictor = false;
        const ContinuationResult res =
            run_continuation(model, Vec::Constant(1, 0.0), Vec::Constant(1, 1.0), driver, copts);
        EXPECT_TRUE(res.reached_p1);
        double worst_rel = 0.0;
        for (const ContinuationStep &st : res.steps) {
            ASSERT_EQ(SqpStatus::kOptimal, st.status) << "at p = " << st.p(0);
            model.set_parameters(st.p);
            const double truth = B1ReviewSphere::f_star(st.p(0));
            worst_rel = std::max(worst_rel, std::abs(model.eval_f(st.x) - truth) /
                                                std::max(1.0, std::abs(truth)));
        }
        EXPECT_LT(worst_rel, 1e-6)
            << "pre-repair the WARM arm read 0.293 here while the cold arm read 6.2e-10";
    }
}

// =====================================================================
// (5) CORPUS ELEMENT 2 -- F6PathBoundQuadrature, a SHIPPED convex family, on a
// RELAXING step. F6 is min sum_i w_i (cosh(x_i - a(t_i)) - 1) s.t. x_i - p <= 0
// with me = 0, mi = N: p is a pure constant shift of cI, grad f and Ji = I are
// p-independent, and there are no finite bounds. It is B-1's shape exactly, it
// is convex, and it shipped in Phase 4 -- which is the whole reason the
// criterion is about where p enters and not about convexity.
//
// The reviewer measured, pre-repair, at N = 9 with kkt_tol = feas_tol = 1e-8:
//   p 0.30 -> 0.40 | warm kOptimal 0 majors f=0.1040078 COMP=9.5e-03 | gap 3.4 %
//   p 0.30 -> 0.60 | warm kOptimal 0 majors f=0.1040078 COMP=2.8e-02 | gap 7.9 %
//   p 0.30 -> 1.20 | warm kOptimal 0 majors f=0.1040078 COMP=8.5e-02 | gap 10.4 %
//   p 0.50 -> 0.60 | warm kOptimal 0 majors f=0.0441316 COMP=6.5e-03 | gap 1.9 %
// Each is checked below against F6's own ANALYTIC f_star, not against a cold
// arm, so nothing here rests on a second solve being right.
// =====================================================================
TEST(B1Gate, F6RelaxingStepsNoLongerFreezeTheObjective) {
    const Vec nodes = uniform_nodes(9);
    const Vec weights = trapezoid_weights(nodes);
    const std::vector<std::pair<double, double>> steps = {
        {0.30, 0.40}, {0.30, 0.60}, {0.30, 1.20}, {0.50, 0.60}};
    for (const auto &[p_from, p_to] : steps) {
        SCOPED_TRACE(fmt::format("F6 p {} -> {}", p_from, p_to));
        F6PathBoundQuadrature model(nodes, weights, p_from);
        SqpOptions opts = probe_options();
        opts.kkt_tol = 1e-8;
        opts.feas_tol = 1e-8;
        SqpOptions cold_opts = opts;
        cold_opts.start_level = StartLevel::kCold;

        SqpDriver cold(cold_opts);
        const SqpSolution first = cold.solve(model, model.start_point());
        ASSERT_EQ(SqpStatus::kOptimal, first.status);
        model.set_parameters(Vec::Constant(1, p_to));
        SqpDriver warm(opts);
        const SqpSolution second = warm.solve(model, first.x, first.warm_start);

        EXPECT_EQ(StartLevel::kWarm, second.counters.start_level_used);
        ASSERT_EQ(SqpStatus::kOptimal, second.status);
        EXPECT_NEAR(model.f_star(p_to), second.f, 1e-7)
            << "pre-repair the objective froze at f*(p_from)";
        const test_support::NlpKktResidual r = self_check_kkt(model, second, 1e-8);
        EXPECT_LT(r.complementarity, 1e-7) << "pre-repair this read 6.5e-3 to 8.5e-2";
    }
}

// =====================================================================
// (6) THE CRITERION DEMONSTRATING ITSELF -- the SAME family, the SAME ingest,
// p moving the OTHER way. A TIGHTENING step moves a GATED quantity
// (feasibility: the old x now violates the lowered bound), so the pre-repair
// driver already caught it and solved correctly. The repair must not disturb
// that, and this is the control that says so: the reviewer measured
// p 0.50 -> 0.20 at 2 majors, f = 0.1476129, complementarity 0, gap 0.
// =====================================================================
TEST(B1Gate, F6TighteningStepWasNeverExposedAndIsUnchanged) {
    const Vec nodes = uniform_nodes(9);
    const Vec weights = trapezoid_weights(nodes);
    F6PathBoundQuadrature model(nodes, weights, 0.50);
    SqpOptions opts = probe_options();
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    SqpOptions cold_opts = opts;
    cold_opts.start_level = StartLevel::kCold;

    SqpDriver cold(cold_opts);
    const SqpSolution first = cold.solve(model, model.start_point());
    ASSERT_EQ(SqpStatus::kOptimal, first.status);
    model.set_parameters(Vec::Constant(1, 0.20));
    SqpDriver warm(opts);
    const SqpSolution second = warm.solve(model, first.x, first.warm_start);

    ASSERT_EQ(SqpStatus::kOptimal, second.status);
    EXPECT_EQ(StartLevel::kWarm, second.counters.start_level_used);
    // THE REVIEWER'S PRE-REPAIR READING, UNCHANGED BY THE REPAIR. The gate
    // cannot bind on a tightening step: every row that moves becomes MORE
    // active, never less, so no ingested price is cleared.
    EXPECT_EQ(2, second.counters.major_iters)
        << "measured pre-repair AND post-repair: a tightening step was always caught";
    EXPECT_NEAR(0.1476129, second.f, 1e-6);
    EXPECT_NEAR(model.f_star(0.20), second.f, 1e-7);
}

// =====================================================================
// (7) CORPUS ELEMENT 4 -- A TRANSFER-PRODUCED WarmStart INTO THE B-1 EXIT.
//
// Carried from the Task-7 note's §5.1 as UNCHECKED BY BOTH PARTIES, with the
// instruction to establish which of two truths holds and pin it either way.
//
// THROUGH PHASE 5 IT WAS VACUOUS, BY DESIGN, and this test was the proof:
// mesh_transfer.h §4 sets structure_hash == 0 unconditionally (a transferred
// object's hash is UNKNOWN -- the destination is a different model -- as
// opposed to merely uncomputed, which is what Phase-5 Task 0 repaired), and
// sqp_driver.h's ingest required `warm.structure_hash != 0` before it would
// resolve above kCold, so a transferred object could not reach ANY ingest path,
// B-1's included. The test recorded its own trigger: "WHAT THIS TEST WOULD
// CATCH: a future change that makes transferred objects ingestible (Phase-6's
// ratification of the ingest gap) WITHOUT ALSO CARRYING THE B-1 REPAIR TO THAT
// ROUTE. It fails loudly rather than silently widening."
//
// **PHASE-6 TASK 5 IS THAT CHANGE, AND THIS TEST IS NOW THE POSITIVE FORM OF
// THE SAME GUARANTEE.** A transferred object resolves StartLevel::kSeeded, so
// its multipliers DO reach the ingest -- and the assertion the old version made
// about the route being unreachable is replaced by the assertion it was
// standing guard for all along: THE B-1 CLEAR RUNS ON IT. The configuration is
// unchanged and is deliberately the exposed one -- the same relaxing F6 step
// (5) reproduces the defect on -- so if the clear did NOT carry to the seeded
// route, this test would report the pre-repair symptom exactly: kOptimal in
// zero majors at the objective of the COARSE mesh's own p = 0.30 answer.
//
// F6 IS THE RIGHT FIXTURE FOR THIS and that is not a coincidence: it is
// mesh_transfer.h's own fixture AND it is a B-1 reproducer (5), so if any
// transfer could reach the exit, this one would -- and now one does.
// =====================================================================
TEST(B1Gate, TransferProducedWarmStartIsSeededAndTheB1ClearCarriesToIt) {
    Mesh coarse, fine;
    coarse.nodes = uniform_nodes(9);
    coarse.weights = trapezoid_weights(coarse.nodes);
    fine.nodes = uniform_nodes(17);
    fine.weights = trapezoid_weights(fine.nodes);

    F6PathBoundQuadrature coarse_model(coarse.nodes, coarse.weights, 0.30);
    SqpDriver cold(probe_options(StartLevel::kCold));
    const SqpSolution coarse_sol = cold.solve(coarse_model, coarse_model.start_point());
    ASSERT_EQ(SqpStatus::kOptimal, coarse_sol.status);
    ASSERT_NE(0u, coarse_sol.warm_start.structure_hash) << "the SOURCE object is hash-valid";
    ASSERT_GT(coarse_sol.lambda_i.maxCoeff(), 0.0) << "and carries live inequality prices";

    const MeshTransfer transfer;
    const WarmStart moved = transfer.transfer(coarse_sol.warm_start, coarse, fine);
    EXPECT_TRUE(moved.valid);
    EXPECT_EQ(0u, moved.structure_hash)
        << "mesh_transfer.h section 4: UNKNOWN, not uncomputed -- the sentinel is deliberate, and "
           "kSeeded does not ask for a hash";
    ASSERT_GT(moved.lambda_i.maxCoeff(), 0.0)
        << "the transferred object carries live prices into the ingest -- otherwise the clear "
           "below would have nothing to bite on";

    // The consequence, on the destination mesh, at a RELAXED p -- i.e. into the
    // exact configuration (5) shows is exposed when the ingest resolves.
    F6PathBoundQuadrature fine_model(fine.nodes, fine.weights, 1.20);
    SqpDriver warm(probe_options(StartLevel::kWarm));
    const SqpSolution sol = warm.solve(fine_model, moved.x, moved);
    EXPECT_EQ(StartLevel::kSeeded, sol.counters.start_level_used)
        << "THE PIN, INVERTED BY PHASE-6 TASK 5: a transferred object now REACHES the ingest, at "
           "the level that takes values without provenance";
    EXPECT_EQ(1, sol.counters.n_seeded);
    EXPECT_EQ(0, sol.counters.seeded_clamped)
        << "a transfer of a driver-produced object carries no negative price";
    EXPECT_EQ(SqpStatus::kOptimal, sol.status);
    // THE B-1 GUARANTEE ON THE NEWLY REACHABLE ROUTE. p relaxed 0.30 -> 1.20,
    // so every path row went strictly slack and every transferred price is
    // stale; without the clear the stale prices zero the Lagrangian gradient at
    // the transferred x and the solve certifies the COARSE answer for free.
    //
    // **THIS ASSERTION IS A GUARD, NOT A MUTATION-SENSITIVE PIN, AND THE
    // DIFFERENCE IS RECORDED RATHER THAN LEFT FOR A LATER READER TO DISCOVER.**
    // Measured: gating sqp_driver.h's B-1 clear on `warm_state_ingest` instead
    // of `warm_ingest` -- i.e. skipping it on the seeded route entirely -- does
    // NOT fail this test. The transferred point's interpolation error across
    // F6's two activity junctions is first-order (tests/test_mesh_transfer.cpp
    // (a')), so its Lagrangian gradient at the fine mesh is outside kkt_tol
    // whether or not the stale prices survive, and a major is owed either way.
    // The mutation-sensitive B-1 pins on this route are
    // B1Gate.SeededIngestCarriesTheB1Repair and
    // tests/test_warm_start.cpp's SeededB1ClearRunsBeforeTheSignClampAnd-
    // TheOrderIsObservable, both of which that mutation kills. What THIS test
    // is for is the ROUTE -- that a transferred object reaches the ingest at
    // all, and at which level -- which is the thing its pre-Task-5 form pinned
    // the negative of.
    EXPECT_GT(sol.counters.major_iters, 0)
        << "the released rows' stale prices are cleared and the transferred point is not a KKT "
           "point of the fine problem, so it cannot be certified in zero majors";
    EXPECT_NEAR(fine_model.f_star(1.20), sol.f, 1e-6);
    const test_support::NlpKktResidual r = self_check_kkt(fine_model, sol, kTol);
    EXPECT_LT(r.complementarity, 1e-6);
}

// =====================================================================
// (8) CORPUS ELEMENT 5 -- THE EQUALITY-ONLY CONTROLS, BIT-IDENTICAL ACROSS THE
// REPAIR.
//
// The immunity argument is rigorous and is the strongest one on the page: an
// equality-only KKT system has NO COMPLEMENTARITY CONDITION AT ALL, so
// stationarity + feasibility IS the complete KKT test there. The repair
// touches lambda_i and nothing else, and on a model with mi() == 0 the clear
// loop does not execute at all -- so these solves must be BYTE-FOR-BYTE what
// Phase 4 produced.
//
// THE VALUES BELOW WERE MEASURED AT BASE COMMIT 0fbdca6 (pre-repair) AND
// RE-MEASURED AFTER IT, IDENTICAL IN BOTH. That is what makes them a control
// rather than a fresh pin: EXPECT_DOUBLE_EQ on the objective and exact
// equality on every count, against numbers taken before the change existed.
//
// HS7 is the equality-only problem Phase-5 Task 0's own O-1 chain test uses
// (tests/test_warm_start.cpp), HS26/HS40/HS77 are the nonconvex sweep corpus's
// equality-only third, and hs_problems.h gives all four no finite bounds -- so
// their KKT system genuinely is {cE = 0, grad f + Je^T lambda_e = 0} with no
// third condition for any gate to touch.
// =====================================================================
struct EqualityControl {
    int hs_number;
    Index cold_majors;
    Index warm_majors;
    double f; // printed at 17 significant digits, i.e. the exact binary64
    double x0;
};

TEST(B1Gate, EqualityOnlyWarmSolvesAreBitIdenticalAcrossTheRepair) {
    // {HS number, cold majors, warm majors, the objective, x(0)}. MEASURED
    // TWICE -- once with the repaired header and once with HEAD's (0fbdca6)
    // sqp_driver.h shadowing it, same binary otherwise -- and every field
    // below came back with the IDENTICAL BIT PATTERN both times (checked as
    // %a: HS7 -0x1.bb67ae8a20f7dp+0, HS26 0x1.4130f91e14ceep-39,
    // HS40 -0x1.000000000dfap-2, HS77 0x1.ee9a3dafc1692p-3).
#ifdef USE_ACCELERATE_SPARSE
    // Origin divergence entry D16. The note that carried it
    // (docs/notes/2026-07-31-accelerate-second-pass-results.md) did NOT
    // migrate into hven, and the register that succeeds it here
    // (docs/notes/2026-08-14-accelerate-divergence-register.md, "Why this
    // file exists at this path") has no row for D16 -- so the observation
    // below is quoted in full in this comment, and this comment is the
    // citable record for it in this repository. What was observed: an
    // on-Apple A/B (pre-repair 0fbdca6 sqp_driver.h shadowing the repaired
    // one, same protocol the origin documents for the MKL measurement)
    // reproduced every field below bit-identically ACROSS THE REPAIR, so the
    // control's PURPOSE -- the repair moves nothing on equality-only models
    // -- holds on this backend exactly as on MKL. Only its ENCODING (MKL
    // binary64 trajectory values) does not transfer: three of the eight
    // pinned fields fork from the MKL trajectory. HS7 f, HS40 f/x0, and HS77
    // x0 are bit-identical to MKL and keep the MKL constants; HS7 x0, HS26 f,
    // and HS26 x0 take the Accelerate-observed constants below (distances
    // 4.2e-22, 2.4e-21, 1.6e-13 respectively -- HS7 x0 and HS26 f are both
    // near-zero readings, where ulp distance is enormous but absolute
    // distance is tiny, per the origin's caution (i), quoted here because the
    // origin itself is not readable from this tree).
    //
    // HS77 f is the origin's flagged TRAP: it passes on Accelerate against the
    // MKL constant, but only through EXPECT_DOUBLE_EQ's 4-ulp gate, at
    // EXACTLY 4 ulps -- zero margin, one further ulp of drift anywhere in
    // this dependency chain flips it to a failure with no warning. Rather
    // than leave that latent, this arm takes the Accelerate-observed value
    // for HS77 f too, so the Accelerate branch is a 0-ulp match like every
    // other passing cell instead of a 4-ulp one.
    //
    // U0 (2026-08-16): THE TRAP SPRANG, exactly where the paragraph above
    // said it would. Under the unified flag set's Apple form
    // (-ffast-math -mcpu=apple-m1; docs/notes/2026-08-16-m3-u0-design.md §2)
    // HS77 f re-observed on the macOS lane as 0.24150512879002839 rather than
    // 0.24150512879002822 -- one further ulp of drift in this dependency
    // chain, which is all the zero-margin 4-ulp gate had left. RE-OBSERVED,
    // NOT COMPUTED (CLAUDE.md §6 forbids fabricating an Apple value): the
    // figure is bit-identical in TWO macOS lane runs, CI run 31985550447
    // attempts 1 and 2 on commit 305e5a1, which is this project's two-run
    // bar. Two notes for the record. (i) The new Apple value is the SAME
    // value MKL Release re-derived under the same flag change (see the NDEBUG
    // arm below) -- the two backends' HS77 encodings, which used to differ,
    // now agree. (ii) The control's PURPOSE is untouched: `first.f ==
    // second.f` across the repair held on both lane runs; only the encoding
    // this arm pins moved. Nothing else in this table moved on Apple.
    const std::vector<EqualityControl> controls = {
        {7, 9, 0, -1.7320508086422415, 3.4794942110708625e-10},
        {26, 17, 0, 2.282201461221889e-12, 0.99938512958678594},
        {40, 4, 0, -0.2500000000031779, 0.7937005259836708},
        {77, 12, 0, 0.24150512879002839, 1.1661721897049999},
    };
#elif defined(NDEBUG)
    // U0 DECLARED RE-DERIVATION (phase-C flag unification, 2026-08-16, see
    // docs/notes/2026-08-16-m3-u0-design.md): the MKL binary64 trajectory
    // constants below are the RELEASE unified-flags encoding of this
    // control. Three of the eight moved in their last bits when tests/sqp
    // adopted COMPILE_FLAGS (-ffast-math -march=native, Release-only
    // generator expressions): HS7 x0 (3.4794942110750977e-10 ->
    // 3.4794942110708625e-10 -- coincidentally the Accelerate arm's own
    // value, both being near-zero readings), HS26 f (2.282201458793485e-12
    // -> 2.2822014587976076e-12), and HS77 f (0.24150512879002811 ->
    // 0.24150512879002839). The control's PURPOSE -- the repair moves
    // nothing on equality-only models -- held unchanged: every counter,
    // status, and the in-process first.f == second.f identity passed across
    // the flag change; only the trajectory ENCODING moved, exactly as it did
    // between backends (the Accelerate arm above). The pre-U0 constants stay
    // asserted in Debug (the #else arm below), whose arithmetic the
    // unification did not move -- a config-split pin per §6(c)1 of the
    // phase-C plan, tolerances not widened. Two fresh reproductions agreed
    // on every constant before this landed.
    const std::vector<EqualityControl> controls = {
        {7, 9, 0, -1.7320508086422415, 3.4794942110708625e-10},
        {26, 17, 0, 2.2822014587976076e-12, 0.99938512958694969},
        {40, 4, 0, -0.2500000000031779, 0.7937005259836708},
        {77, 12, 0, 0.24150512879002839, 1.1661721897049999},
    };
#else
    // Debug: the pre-U0 constants, unchanged -- Debug carries no
    // -ffast-math/-march (those ride Release-only genexes), so its
    // trajectory is the one these were originally derived on. See the
    // NDEBUG arm's U0 note.
    const std::vector<EqualityControl> controls = {
        {7, 9, 0, -1.7320508086422415, 3.4794942110750977e-10},
        {26, 17, 0, 2.282201458793485e-12, 0.99938512958694969},
        {40, 4, 0, -0.2500000000031779, 0.7937005259836708},
        {77, 12, 0, 0.24150512879002811, 1.1661721897049999},
    };
#endif
    for (const EqualityControl &c : controls) {
        SCOPED_TRACE(fmt::format("HS{}", c.hs_number));
        const auto p = make_hs(c.hs_number);
        ASSERT_EQ(0, p.model->mi()) << "this control is equality-only by construction";
        ASSERT_GT(p.model->me(), 0);

        SqpDriver cold(probe_options(StartLevel::kCold));
        const SqpSolution first = cold.solve(*p.model, p.model->start_point());
        ASSERT_EQ(SqpStatus::kOptimal, first.status);
        EXPECT_EQ(c.cold_majors, first.counters.major_iters);

        SqpDriver warm(probe_options(StartLevel::kWarm));
        const SqpSolution second = warm.solve(*p.model, first.x, first.warm_start);
        EXPECT_EQ(StartLevel::kWarm, second.counters.start_level_used);
        EXPECT_EQ(SqpStatus::kOptimal, second.status);
        EXPECT_EQ(c.warm_majors, second.counters.major_iters);
        EXPECT_DOUBLE_EQ(c.f, second.f) << "bit-identical to the pre-repair reading";
        EXPECT_DOUBLE_EQ(c.x0, second.x(0)) << "bit-identical to the pre-repair reading";
        EXPECT_DOUBLE_EQ(first.f, second.f)
            << "re-solving AT the answer returns the answer, exactly";
    }
}

// =====================================================================
// (9) CORPUS ELEMENT 6 -- LEGITIMATE ZERO-MAJOR EXITS MUST SURVIVE.
//
// Phase-5 Task 0 repaired the zero-major hand-off (O-1), and repair menu option
// 4 ("require major_iters >= 1") would have forfeited it outright. The gate
// sits directly next to that machinery, so the feature is guarded here on the
// case that actually stresses it: a warm ingest carrying a STRICTLY POSITIVE
// inequality price on a row that is STILL ACTIVE. The gate must leave that
// price alone and the solve must still certify in zero majors.
//
// tests/test_warm_start.cpp's ZeroMajorSolveEmitsTheSameStructureHashAsABuilt-
// Subproblem is the chain half of the same guarantee (on HS7, equality-only).
// This is the inequality half, which nothing else in the suite covered.
// =====================================================================
TEST(B1Gate, AZeroMajorWarmSolveWithALiveInequalityPriceStillCertifies) {
    B1MinimalRelease model(1.0);
    SqpDriver cold(probe_options(StartLevel::kCold));
    const SqpSolution first = cold.solve(model, model.start_point());
    ASSERT_EQ(SqpStatus::kOptimal, first.status);
    ASSERT_NEAR(1.0, first.lambda_i(0), 1e-8) << "a live, strictly positive price";
    ASSERT_NEAR(0.0, model.eval_ci(first.x)(0), 1e-9) << "on a row sitting ON its boundary";

    // The SAME p: the ingested point is an exact KKT point of the very problem
    // being posed, so the certificate is genuine and must be issued for free.
    SqpDriver warm(probe_options(StartLevel::kWarm));
    const SqpSolution second = warm.solve(model, first.x, first.warm_start);
    EXPECT_EQ(StartLevel::kWarm, second.counters.start_level_used);
    EXPECT_EQ(SqpStatus::kOptimal, second.status);
    EXPECT_EQ(0, second.counters.major_iters)
        << "THE GUARD: the gate must not destroy the zero-major hand-off it sits beside";
    EXPECT_DOUBLE_EQ(first.lambda_i(0), second.lambda_i(0))
        << "an ACTIVE row's price is carried through untouched";

    // And a TIGHTENING move is still caught by feasibility, as it always was.
    model.set_parameters(Vec::Constant(1, 0.5));
    SqpDriver tighten(probe_options(StartLevel::kWarm));
    const SqpSolution third = tighten.solve(model, first.x, first.warm_start);
    EXPECT_GT(third.counters.major_iters, 0);
    EXPECT_NEAR(B1MinimalRelease::f_star(0.5), third.f, 1e-7);
}

// =====================================================================
// (10) THE GATE'S OWN BOUNDARY. The activity test is `cI_j(x) >= -feas_tol`,
// reusing feas_tol exactly as evaluate_kkt's at_lower/at_upper do -- no new
// knob, and the derivation is the one sqp_driver.h already gives for bounds (a
// row is "on" its boundary exactly when the primal feasibility measure could
// not tell it from being on the boundary).
//
// This pins BOTH SIDES of that boundary on one fixture, because a gate whose
// threshold is only tested from one side is a gate whose threshold is not
// tested. Both arms use the same ingested (x, lambda); only p differs, by an
// amount straddling feas_tol.
// =====================================================================
TEST(B1Gate, TheActivityBoundaryIsFeasTolOnBothSides) {
    B1MinimalRelease model(1.0);
    SqpDriver cold(probe_options(StartLevel::kCold));
    const SqpSolution first = cold.solve(model, model.start_point());
    ASSERT_EQ(SqpStatus::kOptimal, first.status);

    SqpOptions opts = probe_options();
    // INSIDE the band: the row is slack by 0.1 * feas_tol, so it still counts
    // as geometrically active, the price survives, and the point is certified
    // free -- which is correct, since it is a KKT point to within feas_tol.
    model.set_parameters(Vec::Constant(1, 1.0 + 0.1 * kTol));
    SqpDriver inside(opts);
    const SqpSolution s_in = inside.solve(model, first.x, first.warm_start);
    EXPECT_EQ(SqpStatus::kOptimal, s_in.status);
    EXPECT_EQ(0, s_in.counters.major_iters) << "within feas_tol of the boundary: still active";
    EXPECT_GT(s_in.lambda_i(0), 0.0);

    // OUTSIDE it, by 100x: the row is strictly slack, the price is cleared and
    // the solve has to work for its answer.
    model.set_parameters(Vec::Constant(1, 1.0 + 100.0 * kTol));
    SqpDriver outside(opts);
    const SqpSolution s_out = outside.solve(model, first.x, first.warm_start);
    EXPECT_EQ(SqpStatus::kOptimal, s_out.status);
    EXPECT_GT(s_out.counters.major_iters, 0) << "beyond feas_tol: the stale price is cleared";
    EXPECT_NEAR(B1MinimalRelease::x_star(1.0 + 100.0 * kTol), s_out.x(0), 1e-7);
}

// =====================================================================
// (11) PHASE-6 TASK 5 -- THE SEEDED ARM OF CORPUS ELEMENTS 7, 3 AND 2.
//
// The same three reproducers as (2), (3) and (5), fed through the kSeeded
// route instead of the kWarm one. Each is checked against its own ANALYTIC
// oracle, exactly as its kWarm twin is, so nothing here rests on the two arms
// agreeing with each other.
//
// WHAT WOULD FAIL HERE. Gating sqp_driver.h's B-1 clear on
// `warm_state_ingest` (kWarm and up) instead of `warm_ingest` (kSeeded and up)
// -- which is the single most natural way to get this wrong while adding a
// level below kWarm -- reproduces the pre-repair symptom on every arm below:
// kOptimal in ZERO majors at the objective of the p_from problem.
// =====================================================================
TEST(B1Gate, SeededIngestCarriesTheB1Repair) {
    // (a) The minimal release -- corpus element 7's own step, seeded.
    {
        B1MinimalRelease model(0.0);
        const WarmLink link = seeded_link(model, 1.0, 1.5, probe_options());
        ASSERT_EQ(SqpStatus::kOptimal, link.first.status);
        EXPECT_EQ(StartLevel::kSeeded, link.second.counters.start_level_used);
        EXPECT_EQ(1, link.second.counters.n_seeded);
        EXPECT_EQ(0, link.second.counters.seeded_clamped)
            << "a driver-produced hand-off is non-negative by construction";
        ASSERT_EQ(SqpStatus::kOptimal, link.second.status);
        EXPECT_GT(link.second.counters.major_iters, 0)
            << "THE SEEDED B-1 PIN: the released row's stale price is cleared on this route too";
        EXPECT_NEAR(B1MinimalRelease::x_star(1.5), link.second.x(0), 1e-7);
        EXPECT_NEAR(B1MinimalRelease::f_star(1.5), link.second.f, 1e-7)
            << "pre-repair this froze at f*(1.0) = 0.5";
        const test_support::NlpKktResidual r = self_check_kkt(model, link.second, kTol);
        EXPECT_LT(r.complementarity, 1e-6);
        EXPECT_LT(r.dual_sign, 1e-9);
    }

    // (b) The Task-7 review's convex n = 3 sphere, at all three step sizes --
    // the "no step size avoids it" finding, on the seeded route.
    for (double dp : {0.5, 0.05, 0.001}) {
        SCOPED_TRACE(fmt::format("sphere dp = {}", dp));
        B1ReviewSphere model(0.0);
        const WarmLink link = seeded_link(model, 0.0, dp, probe_options());
        ASSERT_EQ(SqpStatus::kOptimal, link.first.status);
        EXPECT_EQ(StartLevel::kSeeded, link.second.counters.start_level_used);
        ASSERT_EQ(SqpStatus::kOptimal, link.second.status);
        EXPECT_NEAR(B1ReviewSphere::f_star(dp), link.second.f, 1e-6)
            << "pre-repair the objective stayed frozen at f*(0) = -sqrt(3)";
        const test_support::NlpKktResidual r = self_check_kkt(model, link.second, kTol);
        EXPECT_LT(r.complementarity, 1e-6);
    }

    // (c) F6PathBoundQuadrature, the SHIPPED convex family, on its four
    // relaxing steps -- and this is the arm that matters most, because F6 is
    // also mesh_transfer.h's own fixture, i.e. the shape a real seeded object
    // arrives in.
    {
        const Vec nodes = uniform_nodes(9);
        const Vec weights = trapezoid_weights(nodes);
        const std::vector<std::pair<double, double>> steps = {
            {0.30, 0.40}, {0.30, 0.60}, {0.30, 1.20}, {0.50, 0.60}};
        for (const auto &[p_from, p_to] : steps) {
            SCOPED_TRACE(fmt::format("F6 seeded p {} -> {}", p_from, p_to));
            F6PathBoundQuadrature model(nodes, weights, p_from);
            SqpOptions opts = probe_options();
            opts.kkt_tol = 1e-8;
            opts.feas_tol = 1e-8;
            SqpOptions cold_opts = opts;
            cold_opts.start_level = StartLevel::kCold;

            SqpDriver cold(cold_opts);
            const SqpSolution first = cold.solve(model, model.start_point());
            ASSERT_EQ(SqpStatus::kOptimal, first.status);
            WarmStart hashless = first.warm_start;
            hashless.structure_hash = 0;
            model.set_parameters(Vec::Constant(1, p_to));
            SqpDriver seeded(opts);
            const SqpSolution second = seeded.solve(model, first.x, hashless);

            EXPECT_EQ(StartLevel::kSeeded, second.counters.start_level_used);
            ASSERT_EQ(SqpStatus::kOptimal, second.status);
            EXPECT_NEAR(model.f_star(p_to), second.f, 1e-7)
                << "pre-repair the objective froze at f*(p_from)";
            const test_support::NlpKktResidual r = self_check_kkt(model, second, 1e-8);
            EXPECT_LT(r.complementarity, 1e-7) << "pre-repair this read 6.5e-3 to 8.5e-2";
        }
    }
}

// =====================================================================
// (12) PHASE-6 TASK 5 -- **O-1's LEGITIMATE ZERO-MAJOR EXIT, EXTENDED TO
// SEEDED OBJECTS.** The seeded twin of (9), and the reason it must exist as a
// separate test rather than as an extra arm of (9): the seeded level is
// exactly where the pressure to over-tighten lands. Every defence added in this
// task (the B-1 clear carried down a level, the `lambda_i >= 0` clamp, the
// degrade-to-kCold) is a way of REFUSING an ingested certificate, and the
// cheapest way to make all three "safe" would be to refuse the zero-major exit
// outright -- which is repair menu option 4, the one Phase-5 Task 0's O-1
// feature exists to have avoided.
//
// SO THE GUARD IS THIS: a seeded object that genuinely IS a KKT point of the
// posed problem, carrying a STRICTLY POSITIVE price on a row that is STILL
// ACTIVE, must still be certified in ZERO MAJORS -- with its price carried
// through UNTOUCHED by both the clear and the clamp.
// =====================================================================
TEST(B1Gate, ASeededZeroMajorSolveWithALiveInequalityPriceStillCertifies) {
    B1MinimalRelease model(1.0);
    SqpDriver cold(probe_options(StartLevel::kCold));
    const SqpSolution first = cold.solve(model, model.start_point());
    ASSERT_EQ(SqpStatus::kOptimal, first.status);
    ASSERT_NEAR(1.0, first.lambda_i(0), 1e-8) << "a live, strictly positive price";
    ASSERT_NEAR(0.0, model.eval_ci(first.x)(0), 1e-9) << "on a row sitting ON its boundary";

    WarmStart hashless = first.warm_start;
    hashless.structure_hash = 0; // the mesh-transfer / crossover sentinel
    ASSERT_TRUE(hashless.valid);

    // The SAME p: the seeded point is an exact KKT point of the very problem
    // being posed, so the certificate is genuine and must be issued for free.
    SqpDriver seeded(probe_options(StartLevel::kWarm));
    const SqpSolution second = seeded.solve(model, first.x, hashless);
    EXPECT_EQ(StartLevel::kSeeded, second.counters.start_level_used);
    EXPECT_EQ(1, second.counters.n_seeded);
    EXPECT_EQ(SqpStatus::kOptimal, second.status);
    EXPECT_EQ(0, second.counters.major_iters)
        << "THE GUARD: neither the B-1 clear, the seeded sign clamp, nor the degradation may "
           "destroy the zero-major hand-off they all sit beside (Phase-5 Task 0's O-1)";
    EXPECT_EQ(0, second.counters.seeded_clamped) << "a positive price is not a sign violation";
    EXPECT_DOUBLE_EQ(first.lambda_i(0), second.lambda_i(0))
        << "an ACTIVE row's price is carried through untouched, at kSeeded exactly as at kWarm";
    EXPECT_DOUBLE_EQ(first.f, second.f);

    // And a TIGHTENING move is still caught by feasibility on this route too.
    model.set_parameters(Vec::Constant(1, 0.5));
    SqpDriver tighten(probe_options(StartLevel::kWarm));
    const SqpSolution third = tighten.solve(model, first.x, hashless);
    EXPECT_EQ(StartLevel::kSeeded, third.counters.start_level_used);
    EXPECT_GT(third.counters.major_iters, 0);
    EXPECT_NEAR(B1MinimalRelease::f_star(0.5), third.f, 1e-7);
}

// =====================================================================
// (13) PHASE-6 FINAL FIX WAVE (W1) -- **THE SCALED STALE PRICE**, i.e. the
// wrong-answer certificate the B-1 clear does NOT close, found by the
// cross-vendor adversarial review (2026-08-05) and fixed in the same wave.
//
// THE CLEAR'S GUARANTEE IS TOLERANCE-SCALED, AND THAT SCALE IS UNBOUNDED. After
// the clear every priced row is within feas_tol of its boundary, so
//
//     max_j |lambda_i(j) cI_j(x)| <= feas_tol * ||lambda_i||inf,
//
// which sqp_driver.h's own note is careful to call a DIFFERENT bound from the
// vanishing one it replaced. What neither note said until now is that the
// right-hand side has no upper limit: push ||lambda_i|| up and the same
// "complementary by construction" ingest carries an arbitrarily large
// complementarity residual -- and with it, since that residual is to first
// order the objective available by moving onto the row, an arbitrarily large
// OBJECTIVE ERROR under a kOptimal certificate.
//
// THE MODEL BELOW IS THAT STATEMENT MADE MINIMAL:
//
//     min  -S x + 1/2 x^2      s.t.  cI(x) = x - eps <= 0,   -1 <= x <= 1
//
// with 0 < eps <= feas_tol, so the seed x = 0 is INSIDE the activity band and
// the clear (correctly) leaves the row's price alone. Seeded with
// lambda_i = S, the point reads:
//     gL      = (-S + 0) + 1 * S = 0        EXACTLY -- stationarity passes;
//     cI(0)   = -eps <= 0                   -- feasibility passes;
//     comp    = |S * (-eps)| = S * eps      -- the only quantity that moves.
// The truth is x* = eps, f* = -S eps + eps^2/2, so the objective error under
// the certificate is S eps -- THE COMPLEMENTARITY RESIDUAL ITSELF, which is the
// cleanest possible statement of why the third residual had to be gated and of
// why the gate's tolerance belongs in the units of f.
//
// THE ARMS SPAN kkt_tol FROM BOTH SIDES, the same two-sided discipline
// TheActivityBoundaryIsFeasTolOnBothSides applies to the B-1 gate: the
// certificate is refused exactly when S * eps exceeds kkt_tol, and an ingest
// whose residual is INSIDE it is still certified for free -- the gate is a
// correctness bound, not a new reason to spend majors.
//
// ON THE SCALE THE ARMS USE, AND ON THE ONE THEY DO NOT. The review reported
// the instance at S = 1e12 (complementarity 5.0e5). The principal arm below
// runs at S = 1e6 -- complementarity 0.5, still 5.7 orders above
// kkt_tol -- because that is the largest scale at which THE POST-REFUSAL SOLVE
// ITSELF has an assertable answer: measured across S = 1e2 .. 1e12 on this
// model, S <= 1e9 converges in ONE major, and S = 1e12 runs out of majors
// (60/60, ending 1e-4 outside a row whose tolerance is 1e-6). THE S = 1e12 ARM
// IS KEPT ANYWAY, asserting only what W1 is responsible for -- that the
// certificate is not issued -- because the trade it makes is the point: a
// refused certificate followed by kMaxIter is a failure the CALLER CAN SEE,
// while the pre-W1 kOptimal at f = 0 is one no caller can detect at all. That
// this badly-scaled instance then defeats the iteration is a pre-existing
// scaling limit of the driver, not something the gate introduced, and it is
// reported as a carry rather than pinned as an outcome.
// =====================================================================
class B1ScaledStalePrice : public NlpModel {
  public:
    B1ScaledStalePrice(double scale, double eps) : s_(scale), eps_(eps) {}

    Index n() const override { return 1; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return -s_ * x(0) + 0.5 * x(0) * x(0); }
    Vec eval_grad(const Vec &x) const override { return Vec::Constant(1, -s_ + x(0)); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(0) - eps_); }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(1, 1);
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
    Vec start_point() const override { return Vec::Constant(1, 0.0); }

    // x* = eps (the row binds; the unconstrained minimizer is at x = S >> 1).
    double x_star() const { return eps_; }
    double f_star() const { return -s_ * eps_ + 0.5 * eps_ * eps_; }
    // The price that zeroes gL at the SEED x = 0, i.e. the stale value fed in.
    double stale_price() const { return s_; }
    double complementarity_at_seed() const { return s_ * eps_; }

  private:
    double s_, eps_;
    Vec lower_ = Vec::Constant(1, -1.0);
    Vec upper_ = Vec::Constant(1, 1.0);
};

// A hand-assembled hash-less object -- the shape kSeeded exists to admit, and
// the same helper tests/test_warm_start.cpp's seeded arms use. The activity
// hint is deliberately EMPTY: the B-1 clear and the W1 gate both read GEOMETRY,
// not the hint, so leaving it out keeps the fixture about the multiplier.
WarmStart scaled_seed(const Vec &x, double lambda_i0) {
    WarmStart w;
    w.x = x;
    w.lambda_e = Vec(0);
    w.lambda_i = Vec::Constant(1, lambda_i0);
    w.z = Vec::Zero(x.size());
    w.ineq_active.assign(1, 0);
    w.bound_active.assign(static_cast<std::size_t>(x.size()), 0);
    w.qp_working_set = WorkingSet(x.size(), 1);
    w.structure_hash = 0;
    w.valid = true;
    return w;
}

TEST(B1Gate, AScaledStalePriceIsRefusedAtIngest) {
    // Complementarity 0.5 against kkt_tol = 1e-6, and an objective error of the
    // same 0.5 under the pre-W1 certificate.
    B1ScaledStalePrice model(1e6, 5e-7);
    ASSERT_DOUBLE_EQ(0.5, model.complementarity_at_seed());

    SqpDriver driver(probe_options(StartLevel::kWarm));
    const SqpSolution sol =
        driver.solve(model, model.start_point(), scaled_seed(Vec::Zero(1), model.stale_price()));

    ASSERT_EQ(StartLevel::kSeeded, sol.counters.start_level_used)
        << "fixture premise: a hash-less object ingests at kSeeded";
    EXPECT_EQ(0, sol.counters.seeded_clamped)
        << "the price is POSITIVE -- the seeded sign clamp has nothing to say here, which is "
           "precisely why a second gate was needed";
    EXPECT_EQ(SqpStatus::kOptimal, sol.status);
    EXPECT_GT(sol.counters.major_iters, 0)
        << "THE PIN: pre-W1 this certified f = 0 in ZERO majors, with stationarity 0, "
           "feasibility 0 and complementarity 0.5 -- honouring feas_tol * ||lambda_i||inf "
           "and still 0.5 away from the truth";
    EXPECT_NEAR(model.x_star(), sol.x(0), 1e-8);
    EXPECT_NEAR(model.f_star(), sol.f, 1e-3) << "the truth is -0.5; the defect reported 0";
    // AND THE SCOPE RULING, MEASURED RATHER THAN ARGUED. The point this solve
    // converges to and certifies carries complementarity 1.0e-4 -- three and a
    // half orders better than the ingest's 0.5, and still a hundred times
    // kkt_tol, because the QP lands x a few 1e-10 outside a row priced at 1e6.
    // **SO A GATE APPLIED AT EVERY MAJOR WOULD REFUSE THIS SOLVE'S OWN CORRECT
    // ANSWER**, which is exactly why sqp_driver.h scopes the conjunct to the
    // ingested multipliers: once a subproblem has priced them, the
    // O(||lambda|| ||p||) argument is live and the residual is the step's, not
    // a stale set's.
    const double converged_complementarity = std::abs(sol.lambda_i(0) * model.eval_ci(sol.x)(0));
    EXPECT_LT(converged_complementarity, 1e-3);
    EXPECT_LT(converged_complementarity, 1e-3 * model.complementarity_at_seed());
    EXPECT_GT(converged_complementarity, 1e-6)
        << "the every-major scope this rules out would have refused this very point";
}

// THE REVIEW'S OWN SCALE, S = 1e12, asserting ONLY what W1 owns. See the
// fixture banner's ON THE SCALE THE ARMS USE paragraph: at this scale the
// refused solve does not then converge either, and the honest reading is that a
// visible failure replaced an invisible wrong answer.
TEST(B1Gate, TheReportedScaleIsRefusedEvenThoughTheSolveThenRunsOut) {
    B1ScaledStalePrice model(1e12, 5e-7);
    ASSERT_DOUBLE_EQ(5.0e5, model.complementarity_at_seed());

    SqpDriver driver(probe_options(StartLevel::kWarm));
    const SqpSolution sol =
        driver.solve(model, model.start_point(), scaled_seed(Vec::Zero(1), model.stale_price()));

    ASSERT_EQ(StartLevel::kSeeded, sol.counters.start_level_used);
    const bool false_certificate =
        sol.status == SqpStatus::kOptimal && sol.counters.major_iters == 0;
    EXPECT_FALSE(false_certificate)
        << "THE PIN: pre-W1 this returned kOptimal in ZERO majors at f = 0 against a truth of "
           "-5.0e5, carrying complementarity 5.0e5 -- exactly one half of the constructive "
           "bound feas_tol * ||lambda_i||inf = 1e6, i.e. a bound honoured and an answer still "
           "wrong";
}

// THE SAME DEFECT AT kWarm, because the gate is a property of the INGEST and
// not of the level. This arm carries a real structure hash (it comes off a
// genuine cold solve of the same model) and only its x and its price are
// replaced, so it resolves kWarm -- where neither the seeded dual clamp nor the
// degradation exists and the complementarity conjunct is the only defence.
TEST(B1Gate, TheScaledStalePriceIsRefusedAtWarmToo) {
    B1ScaledStalePrice model(1e6, 5e-7);
    SqpDriver cold(probe_options(StartLevel::kCold));
    const SqpSolution first = cold.solve(model, model.start_point());
    ASSERT_EQ(SqpStatus::kOptimal, first.status);
    ASSERT_TRUE(first.warm_start.valid);
    ASSERT_NE(0u, first.warm_start.structure_hash);

    WarmStart poisoned = first.warm_start;
    poisoned.x = Vec::Zero(1);
    poisoned.lambda_i = Vec::Constant(1, model.stale_price());

    SqpDriver warm(probe_options(StartLevel::kWarm));
    const SqpSolution sol = warm.solve(model, model.start_point(), poisoned);
    EXPECT_EQ(StartLevel::kWarm, sol.counters.start_level_used);
    EXPECT_GT(sol.counters.major_iters, 0) << "pre-W1: zero majors, f = 0";
    EXPECT_NEAR(model.f_star(), sol.f, 1e-3);
}

// THE GATE'S OWN BOUNDARY, BOTH SIDES. Only S changes between the arms; eps is
// fixed at a tenth of feas_tol so the row is geometrically active in both, and
// S * eps straddles kkt_tol. The INSIDE arm is the guard: a residual the gate
// admits must still buy its zero-major certificate, exactly as element (12)
// requires of the clear and the clamp.
TEST(B1Gate, TheComplementarityGateBoundaryIsKktTolOnBothSides) {
    constexpr double kEps = 1e-7;
    for (const bool inside : {true, false}) {
        SCOPED_TRACE(inside ? "S*eps = 5e-7, inside kkt_tol" : "S*eps = 2e-6, outside kkt_tol");
        const double scale = inside ? 5.0 : 20.0;
        B1ScaledStalePrice model(scale, kEps);
        ASSERT_NEAR(inside ? 5e-7 : 2e-6, model.complementarity_at_seed(), 1e-15);

        SqpDriver driver(probe_options(StartLevel::kWarm));
        const SqpSolution sol = driver.solve(model, model.start_point(),
                                             scaled_seed(Vec::Zero(1), model.stale_price()));
        ASSERT_EQ(StartLevel::kSeeded, sol.counters.start_level_used);
        EXPECT_EQ(SqpStatus::kOptimal, sol.status);
        if (inside) {
            EXPECT_EQ(0, sol.counters.major_iters)
                << "THE GUARD: a complementarity residual at or below kkt_tol is what the "
                   "certificate already tolerates on its other two residuals -- refusing it "
                   "would forfeit the zero-major hand-off for nothing";
            EXPECT_DOUBLE_EQ(0.0, sol.x(0)) << "certified where it stood";
            EXPECT_NEAR(model.f_star(), sol.f, 2.0 * model.complementarity_at_seed())
                << "and the objective error it admits IS the residual it admits";
        } else {
            EXPECT_GT(sol.counters.major_iters, 0);
            EXPECT_NEAR(model.x_star(), sol.x(0), 1e-12);
            EXPECT_NEAR(model.f_star(), sol.f, 1e-9);
        }
    }
}

// =====================================================================
// (14) PHASE-6 FINAL FIX WAVE, ROUND 2 (N2) -- **THE FULL-STEP WATCHDOG MUST
// RESTORE THE GATE ALONG WITH THE MULTIPLIERS.**
//
// THE HOLE THIS PINS, and it is a wrong-answer hole rather than a quality one.
// The W1 conjunct is armed by `duals_ingested`, which an accepted step clears.
// The Kungurtsev-Diehl full-step watchdog can then RESTORE the ingested
// (x, lambda_e, lambda_i) wholesale -- and if it does not restore the FLAG with
// them, the gate is disarmed on precisely the triple it exists to refuse, and
// the convergence test one screen below certifies it. sqp_driver.h carries
// `fs_best_duals_ingested` for exactly this; this fixture is what makes that
// carry a pinned property instead of an unexercised invariant.
//
// WHY THE RESTORE FIRES HERE, DETERMINISTICALLY, which is the whole trick:
//   * `SqpKkt::residual()` is `max(stationarity, feasibility)` and DELIBERATELY
//     EXCLUDES complementarity (the watchdog's own note says why).
//   * A W1-refused ingest has stationarity EXACTLY 0 and feasibility EXACTLY 0
//     -- that is what made its certificate false. So its residual() is 0, the
//     floor of the measure.
//   * The new-best test is a STRICT `residual < fs_best_residual`, so no later
//     iterate can ever displace it. `fs_majors_since_best` therefore increments
//     on every subsequent major and `kWarmFullStepWindow = 5` fires the restore
//     at major 5, with no tuning at all.
// The only thing the corpus was missing is a post-refusal solve LONGER than
// that window -- element (13)'s arms converge in about one major, under it.
//
// HOW THIS ONE IS MADE LONG, WITHOUT TOUCHING THE POISON. `tr_init == tr_max`
// pins the trust region at a fixed width for the whole solve (the growth rule
// is `min(delta * kTrGrowFactor, tr_max)`, so it cannot grow), and the model's
// row is placed `eps = 8 * tr` away from the seed. The post-refusal walk is
// then TR-limited at every step and needs ~8 majors to cross, which is
// comfortably past the window and does not depend on the growth threshold, the
// funnel, or any counter this project pins.
//
// WHAT EACH ARM ASSERTS. The solve must reach the TRUE answer, and the
// watchdog must be shown to have actually restored -- otherwise the fixture
// could pass for the trivial reason that the restore never happened and would
// stop pinning the carry the moment the window moved.
// =====================================================================
TEST(B1Gate, TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers) {
    constexpr double kScale = 1e6;
    constexpr double kTr = 1e-7;
    constexpr double kEps = 8.0 * kTr; // 8 TR-limited steps to cross
    B1ScaledStalePrice model(kScale, kEps);

    SqpOptions opts = probe_options(StartLevel::kWarm);
    // THE FIXED TRUST REGION -- see the banner. Equal init/max is legal
    // (SqpDriver validates `tr_max >= tr_init`) and is what makes the
    // post-refusal solve outlast kWarmFullStepWindow deterministically.
    opts.tr_init = kTr;
    opts.tr_max = kTr;
    ASSERT_TRUE(opts.warm_full_step) << "fixture premise: the KD window is armed by default";

    // A hash-CARRYING object, so this ingests at kWarm -- the level at which
    // the full-step window is armed at all (a seeded object never arms it).
    SqpDriver cold(opts);
    const SqpSolution first = cold.solve(model, model.start_point());
    ASSERT_EQ(SqpStatus::kOptimal, first.status);
    ASSERT_NE(0u, first.warm_start.structure_hash);

    WarmStart poisoned = first.warm_start;
    poisoned.x = Vec::Zero(1);
    poisoned.lambda_i = Vec::Constant(1, model.stale_price());

    SqpDriver warm(opts);
    const SqpSolution sol = warm.solve(model, model.start_point(), poisoned);

    ASSERT_EQ(StartLevel::kWarm, sol.counters.start_level_used);
    // THE MECHANISM, ASSERTED RATHER THAN ASSUMED: the watchdog really did
    // restore, so this fixture is exercising the carry and not passing by
    // accident. Without the restore there is nothing here to pin.
    const bool restored = std::any_of(sol.history.begin(), sol.history.end(),
                                      [](const SqpIterate &row) { return row.watchdog_restored; });
    EXPECT_TRUE(restored) << "fixture premise: kWarmFullStepWindow must fire on this solve; if "
                             "this fails the fixture has stopped testing what it claims";
    EXPECT_GT(sol.counters.major_iters, kWarmFullStepWindow)
        << "fixture premise: the post-refusal solve must outlast the window";

    // THE PIN. With the carry dropped, the restore re-installs the ingested
    // triple while `duals_ingested` is false (some accepted step cleared it),
    // the gate is disarmed on exactly that triple, and the very next
    // convergence test returns kOptimal at the NON-KKT point -- f = 0 against
    // a truth of -0.8, in fewer majors than the honest solve takes.
    EXPECT_EQ(SqpStatus::kOptimal, sol.status);
    // 1e-3, not tighter: the returned x sits ~1e-10 outside a row priced at
    // 1e6, so the objective carries the same feas_tol * ||lambda||-scale
    // residue element (13) measures and explains. The discriminator against
    // the defect is not this digit -- it is `|f| > 0.5` below, since the
    // wrong answer is f = 0 exactly.
    EXPECT_NEAR(model.f_star(), sol.f, 1e-3)
        << "THE PIN: dropping fs_best_duals_ingested reports f = 0 here";
    EXPECT_NEAR(model.x_star(), sol.x(0), 1e-9);
    EXPECT_GT(std::abs(sol.f), 0.5) << "the wrong answer this guards against is f = 0 exactly";
}

// =====================================================================
// PHASE-7 TASK 5: THE kSsn ARMS.
//
// SqpOptions::qp_mode selects which kernel solves each subproblem
// (sqp_driver.h's THE SEMISMOOTH-NEWTON TIER note). **NOTHING IN THIS FILE'S
// SUBJECT MATTER IS KERNEL-SPECIFIC** -- the B-1 geometric clear, the seeded
// dual clamp and the W1 complementarity gate all run in solve_impl BEFORE the
// first subproblem is built, and every one of them reads the model and the
// ingested triple rather than any QP solution. So the repairs SHOULD carry to
// kSsn untouched.
//
// "Should" is an argument, and this file's standing discipline (see the SEEDED
// ARMS banner above, which made the same argument about StartLevel::kSeeded and
// then pinned it anyway) is to pin mechanisms rather than argue them. The arms
// below are the three sharpest refusals in this file, re-run with the ONE line
// that differs -- the mode -- and asserting the SAME refusal, not merely a
// refusal:
//
//   * the minimal B-1 release (element 7): the released row's stale price is
//     cleared and the old point is no longer certified;
//   * the scaled stale price at S = 1e6: the poisoned seed is refused at
//     ingest, and the solve then converges to the truth;
//   * the review sphere (element 3): the reviewer's own reproduction.
//
// THEY ARE NOT A SECOND MEASUREMENT OF THE SAME NUMBER. A kSsn solve reaches
// the same answers by a different subproblem kernel and, on these fixtures,
// with real escapes in it -- so a repair that had quietly become dependent on
// something the WALK does would show up here and nowhere else in this file.
SqpOptions ssn_probe_options(StartLevel level = StartLevel::kWarm) {
    SqpOptions opts = probe_options(level);
    opts.qp_mode = QpMode::kSsn;
    return opts;
}

TEST(B1Gate, MinimalReleaseNoLongerCertifiesTheOldPointUnderKSsn) {
    B1MinimalRelease model(0.0);
    const WarmLink link = warm_link(model, 1.0, 1.5, ssn_probe_options());

    ASSERT_EQ(SqpStatus::kOptimal, link.first.status);
    EXPECT_NEAR(1.0, link.first.x(0), 1e-8);

    EXPECT_EQ(StartLevel::kWarm, link.second.counters.start_level_used);
    EXPECT_EQ(SqpStatus::kOptimal, link.second.status);
    // THE REGRESSION, IDENTICALLY: still not zero majors, still the right
    // point, still a complete certificate.
    EXPECT_GT(link.second.counters.major_iters, 0)
        << "THE PIN, under kSsn: the B-1 clear runs before any subproblem is built, so it "
           "cannot depend on which kernel would have solved one";
    EXPECT_NEAR(B1MinimalRelease::x_star(1.5), link.second.x(0), 1e-7);
    EXPECT_NEAR(B1MinimalRelease::f_star(1.5), link.second.f, 1e-7);

    const test_support::NlpKktResidual r = self_check_kkt(model, link.second, kTol);
    EXPECT_LT(r.stationarity, 1e-6);
    EXPECT_LT(r.primal, 1e-6);
    EXPECT_LT(r.dual_sign, 1e-9);
    EXPECT_LT(r.complementarity, 1e-6) << "pre-repair this read 0.5";

    // AND THE TWO MODES AGREE ON THE REFUSAL ITSELF, not merely on the final
    // point: the same number of majors is spent recovering from it. If a
    // future change made the clear kernel-dependent this is the line that
    // moves first.
    const WarmLink walk = warm_link(model, 1.0, 1.5, probe_options());
    EXPECT_EQ(walk.second.counters.major_iters, link.second.counters.major_iters);
}

TEST(B1Gate, AFullyReleasedRowExitsWithAnExactlyZeroPriceUnderKSsn) {
    B1MinimalRelease model(0.0);
    const WarmLink link = warm_link(model, 1.0, 3.0, ssn_probe_options());

    ASSERT_EQ(SqpStatus::kOptimal, link.second.status);
    EXPECT_NEAR(2.0, link.second.x(0), 1e-7);
    ASSERT_EQ(1, link.second.lambda_i.size());
    EXPECT_EQ(0.0, link.second.lambda_i(0))
        << "a strictly slack row prices at exactly zero under either kernel, not merely at "
           "roundoff";
    const test_support::NlpKktResidual r = self_check_kkt(model, link.second, kTol);
    EXPECT_LT(r.complementarity, 1e-12);
}

// THE W1 REFUSAL CARRIES TO kSsn -- and this arm asserts ONLY that, because on
// this fixture (and only on this one, in this repository) kSsn's own
// certificate is materially weaker than the walk's. The next test pins that
// separately and in full; the split is deliberate, so that the refusal this
// file exists for is not entangled with a limitation of a different mechanism.
TEST(B1Gate, AScaledStalePriceIsRefusedAtIngestUnderKSsn) {
    B1ScaledStalePrice model(1e6, 5e-7);
    ASSERT_DOUBLE_EQ(0.5, model.complementarity_at_seed());

    SqpDriver driver(ssn_probe_options(StartLevel::kWarm));
    const SqpSolution sol =
        driver.solve(model, model.start_point(), scaled_seed(Vec::Zero(1), model.stale_price()));

    ASSERT_EQ(StartLevel::kSeeded, sol.counters.start_level_used)
        << "fixture premise: a hash-less object ingests at kSeeded, mode or no mode";
    EXPECT_EQ(0, sol.counters.seeded_clamped) << "the price is POSITIVE";
    EXPECT_GT(sol.counters.major_iters, 0)
        << "THE PIN, under kSsn: pre-W1 this certified f = 0 in ZERO majors at the SEED. The "
           "gate runs on the INGESTED triple, before any kernel is chosen, so it cannot depend "
           "on the mode -- and it does not.";
    EXPECT_GT(std::abs(sol.f), 1e-3)
        << "the pre-W1 wrong answer was f = 0 EXACTLY; whatever else this solve does, it does "
           "not report that";
}

// =====================================================================
// **THE REPAIR OF A NAMED LIMITATION OF kSsn, PINNED WHERE THE DEFECT USED TO
// BE.** Formerly `KSsnCertifiesAWeakerPointThanTheWalkOnABrutallyScaledRow`,
// which asserted the numbers below in their broken form. It is FLIPPED here
// rather than deleted, on the HS10/HS15/HS33 precedent and Phase-5 Task 7b's:
// a fixture that pinned a defect is the right place to pin its repair, because
// the two readings are then diffable against each other in one file.
//
// WHAT USED TO HAPPEN. On B1ScaledStalePrice(S = 1e6) the driver returned
// kOptimal under kSsn at x = -1.310745e-07, where the truth is x* = 5e-07.
// Measured at that point against the MODEL rather than the driver's own
// residual: stationarity 6.7e-07 (inside kkt_tol), primal 0, and
// **complementarity 6.311e-01**. The model-level KKT self-check REJECTED a
// point the driver certified. Under kWalk the same fixture landed within 1e-10
// of x* with complementarity 1.0e-04 (the kWalk arm above measures and explains
// that residue).
//
// THE MECHANISM, which was arithmetic rather than a bug. `fb_tol` is an
// ABSOLUTE tolerance on the Fischer-Burmeister residual, and
// phi(a, b) = a + b - sqrt(a^2 + b^2) is approximately `a` (the slack) when
// b >> a -- so |phi| <= fb_tol permits a slack of about fb_tol, and the per-row
// complementarity such an exit certifies is bounded by
//
//     fb_tol * ||lambda||inf,
//
// NOT by fb_tol. At ||lambda||inf = 1e6 that bound is 1.0 and the measured 0.63
// sat inside it. The certificate was intact; it simply certified min(s, lambda),
// which is a WEAKER quantity than the product the driver's convergence-test note
// assumes. That note (sqp_driver.h's WHAT IS MEASURED BUT NOT GATED) declines to
// gate NLP complementarity on the grounds that the SUBPROBLEM's own
// complementarity is an EXACT identity -- true of an active-set solve, false of
// a residual-tolerance one.
//
// **THE REPAIR, AND IT IS NOT A TOLERANCE.** Task 5's own brief named a
// deliverable that had never been implemented: the tier-3 STABLE-FACE
// REFINEMENT entry. Every certifying SSN exit now hands its identified face to
// QpEngine::refine_on_face for one EXACT equality-constrained solve, which
// restores the note's identity BY CONSTRUCTION -- rows off the face price at
// exactly zero, rows on it are driven to Ai_j p = bi_j. On this fixture the
// SSN's own partition declares the row active (lambda = 1e6 > s = 6.3e-7), so
// the face solve drives the row to equality and lands on x*.
//
// MEASURED, and it is the whole finding: complementarity 6.311e-01 ->
// 1.0000e-04, which is the WALK's own value on this fixture to five figures. No
// relative-fb_tol rule was needed, and none was added.
// =====================================================================
TEST(B1Gate, KSsnMatchesTheWalksComplementarityOnceTheFaceIsRefined) {
    B1ScaledStalePrice model(1e6, 5e-7);
    const WarmStart seed = scaled_seed(Vec::Zero(1), model.stale_price());

    SqpDriver walk_driver(probe_options(StartLevel::kWarm));
    const SqpSolution walk = walk_driver.solve(model, model.start_point(), seed);
    SqpDriver ssn_driver(ssn_probe_options(StartLevel::kWarm));
    const SqpSolution ssn = ssn_driver.solve(model, model.start_point(), seed);

    ASSERT_EQ(SqpStatus::kOptimal, walk.status);
    ASSERT_EQ(SqpStatus::kOptimal, ssn.status);

    const test_support::NlpKktResidual walk_r = self_check_kkt(model, walk, kTol);
    const test_support::NlpKktResidual ssn_r = self_check_kkt(model, ssn, kTol);

    // THE WALK'S CERTIFICATE SURVIVES THE MODEL-LEVEL CHECK, unchanged.
    EXPECT_NEAR(model.x_star(), walk.x(0), 1e-8);
    EXPECT_LT(walk_r.complementarity, 1e-3);

    // AND SO DOES kSsn's, NOW. The numbers are pinned so a REGRESSION is
    // visible as a failure rather than as silence -- exactly as the defect's
    // numbers were.
    EXPECT_NEAR(5.0010e-07, ssn.x(0), 1e-11)
        << "OBSERVED. MKL, clang++, Release. x* = 5e-07. The unrepaired arm reported "
           "-1.310745e-07 -- the wrong side of zero.";
    EXPECT_NEAR(1.0000e-04, ssn_r.complementarity, 1e-8)
        << "OBSERVED. MKL, clang++, Release. THE REPAIR: the unrepaired arm read 6.311e-01, "
           "which is 0.63 of the fb_tol * ||lambda||inf = 1 bound. See this test's banner.";
    EXPECT_LT(ssn_r.stationarity, 1e-6);
    EXPECT_LT(ssn_r.primal, 1e-9);
    EXPECT_EQ(0, ssn.counters.ssn.ssn_escapes)
        << "AND IT NEVER WAS AN ESCAPE THAT WENT WRONG: the SSN kernel CERTIFIED this "
           "subproblem, which is why the repair had to be at the certifying exit.";

    // THE MECHANISM IS ASSERTED, not merely the outcome: the refinement really
    // ran on this solve. A repair that had happened for some other reason would
    // leave this at zero.
    EXPECT_GT(ssn.counters.ssn.ssn_refinements, 0)
        << "the tier-3 stable-face refinement is what closed the gap";

    // AND THE TWO KERNELS NOW AGREE, stated as a ratio so it cannot be read as
    // a rounding coincidence. Pre-repair this ratio was > 1e3 IN THE OTHER
    // DIRECTION, and the old test asserted exactly that.
    EXPECT_LT(ssn_r.complementarity, 2.0 * walk_r.complementarity);
    EXPECT_GT(ssn_r.complementarity, 0.5 * walk_r.complementarity);
}

TEST(B1Gate, ReviewSphereIsRepairedUnderKSsnToo) {
    B1ReviewSphere model(0.0);
    const WarmLink link = warm_link(model, 0.0, 0.5, ssn_probe_options());

    ASSERT_EQ(SqpStatus::kOptimal, link.second.status);
    EXPECT_EQ(StartLevel::kWarm, link.second.counters.start_level_used);
    EXPECT_GT(link.second.counters.major_iters, 0)
        << "THE REVIEWER'S OWN REPRO, under kSsn: pre-repair this returned kOptimal in ZERO "
           "majors carrying complementarity 4.33e-01";
    EXPECT_NEAR(B1ReviewSphere::f_star(0.5), link.second.f, 1e-6)
        << "pre-repair the objective froze 18.35% from truth";
    const test_support::NlpKktResidual r = self_check_kkt(model, link.second, kTol);
    EXPECT_LT(r.stationarity, 1e-5);
    EXPECT_LT(r.primal, 1e-6);
    EXPECT_LT(r.complementarity, 1e-5);
}

} // namespace
} // namespace hven::solvers
