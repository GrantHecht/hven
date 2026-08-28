// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_sqp_driver.cpp — the SQP driver: Task 4's skeleton and Task 6's
// trust-region major loop.
//
// Seven batteries:
//
//   SqpDriverContract.*     -- the pieces the later tasks build on, pinned
//                              directly: build_subproblem's linearization,
//                              evaluate_kkt's reduced stationarity measure
//                              (including its NON-FINITE handling and the
//                              activity tolerance), the trust region's
//                              CENTER, the per-major model-evaluation
//                              budget, option/start-point validation, the
//                              two history shapes, and the zero-subproblem
//                              path.
//   SqpDriverEquality.*     -- (a) quadratic convergence. HS7 from inside its
//                              region of attraction converges in 7 majors
//                              with the last-three KKT residuals contracting
//                              quadratically; HS6/HS7 from their PUBLISHED
//                              start points converge too, less tidily.
//   SqpDriverBounds.*       -- (b) HS1/HS3/HS5 to their cited f*, each with
//                              an in-test KKT self-check against the MODEL's
//                              gradients at the returned point (the NLP
//                              analogue of test_scale_smoke.cpp's
//                              self_check_kkt, which does the same for a QP).
//   SqpDriverWarmStart.*    -- (c) warm QP seeding: late majors cost <= 2
//                              minor iterations, and the SAME subproblems
//                              solved cold cost strictly more.
//   SqpDriverGlobalization.* -- WAS "what full-step SQP does not do" (Task
//                              5's motivation evidence). Task 6 turned each
//                              of those measurements into its successor: the
//                              funnel's actual guarantees on the same
//                              fixtures, the HS5 cycle now converging, and
//                              the kInfeasible propagation branch re-pinned
//                              on a genuinely inconsistent NLP.
//   SqpDriverTrustRegion.*  -- Task 6 proper: the predicted-decrease formula,
//                              rejection + hot-started retry, radius growth
//                              and its ceiling, the rejection count the
//                              funnel's restoration gate reads, and strategy
//                              pluggability.
//   SqpDriverQpFailure.*    -- Task 6b: a subproblem that FAILED but returned
//                              a usable iterate is a rejected step, not an
//                              abort -- the retryability predicate itself,
//                              the shrink-retry on kNumericalError and on
//                              kMaxIter, the one-shot bound that still
//                              propagates a repeated failure, and the mix of
//                              routed vs judged evidence the funnel's
//                              restoration gate ends up reading.
//   SqpDriverSoc.*          -- Task 7: the second-order correction. The
//                              classic Maratos fixture defeated (fewer majors
//                              WITH the correction than without, on the exact
//                              same problem -- the delta IS the test), the
//                              gate that keeps it from firing when h did not
//                              increase, and the routed-QP-failure path that
//                              never reaches it at all.
//   SqpDriverElastic.*      -- Task 8: the elastic tier. The augmented
//                              subproblem's construction unit-tested away
//                              from any engine (which rows are relaxed, both
//                              slack signs, the zero H block, the folded
//                              window), the exact-penalty recovery of a
//                              FEASIBLE QP's own solution (the structural
//                              coverage of Phase 2's false-kInfeasible
//                              carry-forward), a constructed feasible NLP
//                              with an inconsistent linearization at its
//                              start point converging anyway, the tier's
//                              idleness on every clean fixture, and the
//                              bounded rho ladder ending in the restoration
//                              signal.
//   SqpDriverAdaptiveMu.*   -- Task 10: the KKT-residual-driven dual_mu
//                              schedule. A badly-scaled equality row (ported
//                              from test_eqp_solve.cpp's refinement fixture)
//                              recovering 1e-9-relative accuracy where a
//                              fixed engine-default mu plateaus at a much
//                              looser one; the recorded schedule's own
//                              monotonicity/quantization plus the
//                              hot-start reuse the quantization is FOR
//                              (a run of cross-major reuses at one held
//                              decade); and the lever's idleness when
//                              disabled, byte-compared against the schedule
//                              never having existed.
//   SqpDriverDiagnostics.*  -- Task 12: the formalized per-major history
//                              contract (the length invariant, and every
//                              field the brief names -- KKT residual, h, f,
//                              Delta, verdict, QP counters -- populated on
//                              every row), the fmt-based iteration printer,
//                              and the Ledger's SqpSolveRecord (one aggregate
//                              record per driver solve, with the QP-level
//                              SolveRecord entries from that same driver's
//                              internal engine still landing in the same
//                              Ledger when both are attached).
//
// EVERY ITERATION COUNT IN THIS FILE IS AN OBSERVATION ON THIS MACHINE, and
// the assertions are deliberately looser than the observation (the observed
// value is stated in a comment and/or RecordProperty). The trajectories here
// are unglobalized Newton sequences and some of them are genuinely erratic;
// asserting an exact major count would be pinning floating-point luck. Where
// a fixture IS sensitive to the trust-region radius, the sensitivity is
// measured and stated rather than hidden behind a lucky default.

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>

#include "support/hs_problems.h"
#include "support/hs_sweeps.h"
#include "support/nlp_kkt_check.h"

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::test_support::HsProblem;
using hven::solvers::test_support::make_hs;
// TASK 11 MOVED THESE TWO to tests/sqp/support/nlp_kkt_check.h so that the
// Hock-Schittkowski battery can run the same check on 27 problems rather than
// keep a second copy of it. The definitions are unchanged; only their home is.
using hven::solvers::test_support::NlpKktResidual;
using hven::solvers::test_support::self_check_kkt;

namespace {

// Records a solve's per-major residual/cost table onto the gtest XML, so a
// failing run on another machine reports the trajectory that produced it
// rather than only the assertion that tripped.
void record_history(const SqpSolution &sol, const std::string &tag) {
    for (std::size_t k = 0; k < sol.history.size(); ++k) {
        const SqpIterate &h = sol.history[k];
        const char *verdict = "-";
        if (h.qp_solved && h.qp_status == QpStatus::kOptimal) {
            switch (h.verdict) {
            case StepVerdict::kAcceptF:
                verdict = "acceptF";
                break;
            case StepVerdict::kAcceptH:
                verdict = "acceptH";
                break;
            case StepVerdict::kReject:
                verdict = "reject";
                break;
            case StepVerdict::kRestore:
                verdict = "restore";
                break;
            }
        }
        ::testing::Test::RecordProperty(
            fmt::format("{}_it{}", tag, k),
            fmt::format("f={:.10g} stat={:.6g} feas={:.6g} kkt={:.6g} h={:.6g} step={:.6g} "
                        "delta={:.6g} minor={} fact={} tr={} verdict={}",
                        h.f, h.stationarity, h.feasibility, h.kkt_residual, h.violation_l1,
                        h.step_norm, h.tr_radius, h.qp_minor_iters, h.qp_factorizations,
                        h.tr_binding ? 1 : 0, verdict));
    }
}

// Replays a driver solve's major sequence, solving EVERY subproblem twice --
// once on a fresh engine with no seed, once on a persistent engine warm-
// seeded exactly as SqpDriver seeds it (active set only, x zeroed). Reports
// the cold and warm minor-iteration counts per major, plus the displacement
// the warm path actually applied (which is what SqpIterate::step_norm is
// checked against). The warm path is what drives the iterates forward, so
// the replay follows the driver's own trajectory.
//
// TASK 6: THE RADII COME FROM THE DRIVER'S OWN HISTORY (`radii`), because the
// radius is no longer constant across a solve. The replay still takes every
// step it computes, so it reproduces a driver trajectory only on a fixture
// where NOTHING WAS REJECTED -- callers must assert
// counters.rejected_steps == 0 before comparing, and the tests below do.
struct ColdWarmMinors {
    std::vector<Index> cold, warm;
    std::vector<double> warm_step_norm; // ||x_{k+1} - x_k||inf actually applied
};

std::vector<double> radii_of(const SqpSolution &sol) {
    std::vector<double> radii;
    for (const SqpIterate &h : sol.history) {
        radii.push_back(h.tr_radius);
    }
    return radii;
}

ColdWarmMinors replay_cold_vs_warm(const NlpModel &model, const SqpOptions &opts, Index majors,
                                   const std::vector<double> &radii) {
    ColdWarmMinors out;
    QpEngine cold_engine(opts.qp);
    QpEngine warm_engine(opts.qp);
    SolveOverrides overrides; // tr_radius is set per major, from `radii`, below

    Vec x = model.start_point();
    Vec le = Vec::Zero(model.me());
    Vec li = Vec::Zero(model.mi());
    QpSolution seed;
    bool have_seed = false;

    for (Index k = 0; k < majors; ++k) {
        overrides.tr_radius = k < static_cast<Index>(radii.size())
                                  ? radii[static_cast<std::size_t>(k)]
                                  : opts.tr_init;
        const QpProblem qp = build_subproblem(model, x, le, li, 1.0);
        const QpSolution c = cold_engine.solve(qp, overrides);
        QpSolution w =
            have_seed ? warm_engine.solve(qp, seed, overrides) : warm_engine.solve(qp, overrides);
        out.cold.push_back(c.counters.minor_iters);
        out.warm.push_back(w.counters.minor_iters);
        if (w.status != QpStatus::kOptimal) {
            out.warm_step_norm.push_back(0.0);
            break;
        }
        const Vec x_before = x;
        x += w.x;
        out.warm_step_norm.push_back((x - x_before).lpNorm<Eigen::Infinity>());
        le = w.lambda_e;
        li = w.lambda_i;
        seed = std::move(w);
        seed.x.setZero(); // exactly SqpDriver's seeding policy
        have_seed = true;
    }
    return out;
}

constexpr double kInfBound = 1e20;

// Counts every virtual model call, forwarding to a real HS model. Used to
// pin the per-major evaluation budget (see CallCountPerMajorIsBounded).
class CountingModel : public NlpModel {
  public:
    explicit CountingModel(std::unique_ptr<NlpModel> inner) : inner_(std::move(inner)) {}

    mutable Index n_f = 0, n_grad = 0, n_ce = 0, n_ci = 0, n_hess = 0, n_jac_e = 0, n_jac_i = 0;

    Index n() const override { return inner_->n(); }
    Index me() const override { return inner_->me(); }
    Index mi() const override { return inner_->mi(); }

    double eval_f(const Vec &x) const override {
        ++n_f;
        return inner_->eval_f(x);
    }
    Vec eval_grad(const Vec &x) const override {
        ++n_grad;
        return inner_->eval_grad(x);
    }
    Vec eval_ce(const Vec &x) const override {
        ++n_ce;
        return inner_->eval_ce(x);
    }
    Vec eval_ci(const Vec &x) const override {
        ++n_ci;
        return inner_->eval_ci(x);
    }
    SpMatRM eval_hess(const Vec &x, double s, const Vec &le, const Vec &li) const override {
        ++n_hess;
        return inner_->eval_hess(x, s, le, li);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        ++n_jac_e;
        return inner_->eval_jac_e(x);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        ++n_jac_i;
        return inner_->eval_jac_i(x);
    }
    const Vec &lower() const override { return inner_->lower(); }
    const Vec &upper() const override { return inner_->upper(); }
    Vec start_point() const override { return inner_->start_point(); }

  private:
    std::unique_ptr<NlpModel> inner_;
};

// M3 FINAL REVIEW, S-1. A decorator that forwards everything to a real model
// except ONE callback, whose return it deliberately mis-SIZES -- the shape a
// consumer-written model produces by getting me()/mi()/n() wrong in one place
// and right everywhere else. Every arm is a size error and nothing else: the
// values are the base model's own, so what the eval boundary is being asked to
// notice is the dimension and not a NaN or a wild number.
//
// EMPTY IS THE INTERESTING CASE, and it is why `kCi` returns a 0-sized vector
// rather than a merely-short one: the finiteness screen these bundles carry is
// allFinite(), which is VACUOUSLY TRUE on an empty vector, so an empty cI on a
// model with mi() == 1 passes every check that existed before S-1 and is then
// indexed at ev.ci(0) by evaluate_kkt.
class MisSizingModel : public NlpModel {
  public:
    // kNone is the control arm: the decorator forwards everything untouched,
    // which is what makes "the other arms throw" a statement about the arms
    // rather than about the decorator.
    enum class Which { kNone, kGrad, kCe, kJacE, kCi, kJacI, kLower, kUpper, kValuesCe, kValuesCi };

    MisSizingModel(std::unique_ptr<NlpModel> inner, Which which)
        : inner_(std::move(inner)), which_(which) {
        short_box_ = Vec::Zero(inner_->n() > 0 ? inner_->n() - 1 : 0);
    }

    Index n() const override { return inner_->n(); }
    Index me() const override { return inner_->me(); }
    Index mi() const override { return inner_->mi(); }

    double eval_f(const Vec &x) const override { return inner_->eval_f(x); }
    Vec eval_grad(const Vec &x) const override {
        const Vec g = inner_->eval_grad(x);
        return which_ == Which::kGrad ? Vec(g.head(g.size() - 1)) : g;
    }
    Vec eval_ce(const Vec &x) const override {
        const Vec c = inner_->eval_ce(x);
        return which_ == Which::kCe ? Vec(0) : c;
    }
    Vec eval_ci(const Vec &x) const override {
        const Vec c = inner_->eval_ci(x);
        return which_ == Which::kCi ? Vec(0) : c;
    }
    SpMatRM eval_hess(const Vec &x, double s, const Vec &le, const Vec &li) const override {
        return inner_->eval_hess(x, s, le, li);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> J = inner_->eval_jac_e(x);
        if (which_ == Which::kJacE) {
            J.conservativeResize(J.rows() + 1, J.cols()); // one row too many
        }
        return J;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> J = inner_->eval_jac_i(x);
        if (which_ == Which::kJacI) {
            J.conservativeResize(J.rows(), J.cols() + 1); // one column too many
        }
        return J;
    }
    void eval_values(const Vec &x, double &f, Vec &cE, Vec &cI) const override {
        inner_->eval_values(x, f, cE, cI);
        if (which_ == Which::kValuesCe) {
            cE = Vec::Zero(inner_->me() + 1);
        }
        if (which_ == Which::kValuesCi) {
            cI = Vec::Zero(inner_->mi() + 1);
        }
    }
    const Vec &lower() const override {
        return which_ == Which::kLower ? short_box_ : inner_->lower();
    }
    const Vec &upper() const override {
        return which_ == Which::kUpper ? short_box_ : inner_->upper();
    }
    Vec start_point() const override { return inner_->start_point(); }

  private:
    std::unique_ptr<NlpModel> inner_;
    Which which_;
    Vec short_box_;
};

// Runs `fn` and returns the message of the std::invalid_argument it must
// throw, so a caller can assert on the TEXT and not merely on the type -- S-1's
// whole point is that the diagnostic names which callback misbehaved.
std::string message_of_throw(const std::function<void()> &fn) {
    try {
        fn();
    } catch (const std::invalid_argument &e) {
        return e.what();
    } catch (...) {
        return "<a non-invalid_argument exception>";
    }
    return "<no exception was thrown>";
}

} // namespace

// =========================================================================
// SqpDriverContract -- the reusable pieces, pinned directly.
// =========================================================================

// build_subproblem is what Tasks 5-9 rebuild their own steps from, so its
// block-by-block correspondence to nlp_model.h is a contract, not an
// implementation detail. HS76 is the fixture because it is the only shipped
// problem with general inequalities AND finite bounds, so every block is
// non-trivial at once.
TEST(SqpDriverContract, BuildSubproblemLinearizesTheModel) {
    const HsProblem p = make_hs(76);
    const NlpModel &m = *p.model;
    const Vec x = m.start_point(); // (0.5, 0.5, 0.5, 0.5)
    const Vec le = Vec::Zero(m.me());
    const Vec li = Vec::Zero(m.mi());

    const QpProblem qp = build_subproblem(m, x, le, li, 1.0);
    ASSERT_NO_THROW(qp.validate());

    EXPECT_EQ(qp.n(), 4);
    EXPECT_EQ(qp.me(), 0);
    EXPECT_EQ(qp.mi(), 3);

    // g == grad f(x).
    EXPECT_LT((qp.g - m.eval_grad(x)).lpNorm<Eigen::Infinity>(), 1e-15);

    // bi == -cI(x), Ai == Ji(x).
    EXPECT_LT((qp.bi + m.eval_ci(x)).lpNorm<Eigen::Infinity>(), 1e-15);
    EXPECT_LT((Eigen::MatrixXd(qp.Ai) - Eigen::MatrixXd(m.eval_jac_i(x))).lpNorm<Eigen::Infinity>(),
              1e-15);

    // Bounds are the box RELATIVE to x: l - x .. u - x. No trust region is
    // baked in -- the radius is a per-solve engine override.
    //
    // Compared ENTRYWISE FOR EXACT EQUALITY, not as a norm of a nested
    // difference (U0, unified flags). `u - x` is one subtraction on both
    // sides, so the doubles agree exactly -- but the old form
    // `(qp.upper - (m.upper() - x))` is a difference of differences, which
    // -ffast-math licenses clang to reassociate into `(qp.upper - m.upper())
    // + x`; on HS76's INFINITE upper bounds that rewrite computes the true
    // 0.5 instead of the rounded 0 (1e20 - 0.5 == 1e20 in binary64) and
    // failed the norm bound. Equality comparisons carry no arithmetic to
    // reassociate.
    for (Index i = 0; i < qp.n(); ++i) {
        EXPECT_EQ(qp.lower(i), m.lower()(i) - x(i)) << "lower, entry " << i;
        EXPECT_EQ(qp.upper(i), m.upper()(i) - x(i)) << "upper, entry " << i;
    }
    for (Index i = 0; i < qp.n(); ++i) {
        EXPECT_DOUBLE_EQ(qp.lower(i), -0.5); // l = 0, x = 0.5
        EXPECT_GT(qp.upper(i), 0.5 * kInfBound);
    }

    // H == the exact Lagrangian Hessian at (x, 1, le, li), upper triangle.
    const Eigen::MatrixXd h_built(qp.H);
    const Eigen::MatrixXd h_model(m.eval_hess(x, 1.0, le, li));
    EXPECT_LT((h_built - h_model).lpNorm<Eigen::Infinity>(), 1e-15);

    // Equality blocks are correctly shaped even when empty (me == 0): a
    // 0 x n Ae, not a default-constructed 0 x 0 one, or validate() would
    // reject it.
    EXPECT_EQ(qp.Ae.rows(), 0);
    EXPECT_EQ(qp.Ae.cols(), 4);
    EXPECT_EQ(qp.be.size(), 0);
}

// Cross-check the equality-block half on HS6, whose Je is x-dependent.
TEST(SqpDriverContract, BuildSubproblemLinearizesEqualityConstraints) {
    const HsProblem p = make_hs(6);
    const NlpModel &m = *p.model;
    const Vec x = m.start_point(); // (-1.2, 1)
    Vec le(1);
    le << 0.25; // a nonzero multiplier so the constraint Hessian term is live
    const Vec li = Vec::Zero(0);

    const QpProblem qp = build_subproblem(m, x, le, li, 1.0);
    ASSERT_NO_THROW(qp.validate());
    EXPECT_EQ(qp.me(), 1);

    // be == -cE(x): cE(-1.2, 1) = 10*(1 - 1.44) = -4.4, so be == 4.4.
    EXPECT_NEAR(qp.be(0), 4.4, 1e-13);
    // Je(x) = (-20 x1, 10) = (24, 10).
    const Eigen::MatrixXd je(qp.Ae);
    EXPECT_NEAR(je(0, 0), 24.0, 1e-13);
    EXPECT_NEAR(je(0, 1), 10.0, 1e-13);

    // H picks up lambda_e * hess(cE) = 0.25 * (-20) on the (0,0) entry.
    const Eigen::MatrixXd h(qp.H);
    EXPECT_NEAR(h(0, 0), 2.0 + 0.25 * -20.0, 1e-13);
}

// The stationarity measure documented in sqp_driver.h's CONVERGENCE TEST
// note. Three regimes, one per branch of the per-variable rule.
TEST(SqpDriverContract, KktMeasureUsesTheReducedGradientAtActiveBounds) {
    constexpr double kBoundTol = 1e-6;

    // (1) Every variable at a LOWER bound with the WRONG-signed multiplier.
    // HS76 at the origin: all four x >= 0 bounds active, grad f = (-1,-3,1,-1)
    // with zero multipliers, so z = grad f must be >= 0 and is not: the
    // measure is max(1, 3, 0, 1) = 3, NOT ||grad f||inf = 3 by coincidence of
    // the largest entry -- note the +1 entry (index 2) contributes 0, which
    // is the part a naive ||grad L||inf would get wrong.
    {
        const HsProblem p = make_hs(76);
        const Vec x = Vec::Zero(4);
        const SqpKkt k = evaluate_kkt(*p.model, x, Vec::Zero(0), Vec::Zero(3), kBoundTol);
        EXPECT_NEAR(k.stationarity, 3.0, 1e-12);
        EXPECT_NEAR(k.z(2), 1.0, 1e-12); // priced, sign-consistent, contributes 0
        EXPECT_NEAR(k.grad_lag(2), 1.0, 1e-12);
        // cI(0) = (-5, -4, 1.5): the third row is VIOLATED at the origin.
        EXPECT_NEAR(k.feasibility, 1.5, 1e-12);
    }

    // (2) Every variable at a LOWER bound with the RIGHT-signed multiplier:
    // HS3's solution (0, 0). grad f = (0, 1); x2 sits on its lower bound 0
    // and z2 = 1 >= 0, x1 is free with grad 0. Stationary, measure 0.
    {
        const HsProblem p = make_hs(3);
        const Vec x = Vec::Zero(2);
        const SqpKkt k = evaluate_kkt(*p.model, x, Vec::Zero(0), Vec::Zero(0), kBoundTol);
        EXPECT_LT(k.stationarity, 1e-14);
        EXPECT_NEAR(k.z(1), 1.0, 1e-12);
        EXPECT_NEAR(k.z(0), 0.0, 1e-14); // x1 is free (lower = -1e20)
        EXPECT_LT(k.feasibility, 1e-14);
    }

    // (3) At an UPPER bound the sign flips: HS5's box corner (4, 3), where
    // grad f = (cos 7 + 0.5, cos 7 + 0.5) = (1.2539.., 1.2539..). At an upper
    // bound z must be <= 0, so both entries are violations of the same size.
    {
        const HsProblem p = make_hs(5);
        Vec x(2);
        x << 4.0, 3.0;
        const SqpKkt k = evaluate_kkt(*p.model, x, Vec::Zero(0), Vec::Zero(0), kBoundTol);
        const double expected = std::cos(7.0) + 0.5;
        EXPECT_NEAR(k.stationarity, expected, 1e-12);
        EXPECT_NEAR(k.z(0), expected, 1e-12);
        EXPECT_NEAR(k.z(1), expected, 1e-12);
    }

    // (3b) THE ACTIVITY TOLERANCE IS LOAD-BEARING, and this is what pins the
    // choice of feas_tol for it. HS3 at (0, 1e-8) sits 1e-8 off its lower
    // bound -- inside feas_tol, so the variable is ACTIVE and its correctly
    // signed multiplier z2 = 1 > 0 certifies it: the measure is ~2e-13, i.e.
    // converged. With a naive ZERO tolerance the same point reads as a FREE
    // variable carrying |grad L| = 1, and the driver would keep stepping
    // forever at a point that is a solution to every tolerance it claims to
    // work to. (This is also why the activity tolerance must not be TIGHTER
    // than feas_tol: a point can be feasible-to-tolerance at a bound while
    // being nowhere near it in exact arithmetic.)
    {
        const HsProblem p = make_hs(3);
        Vec x(2);
        x << 0.0, 1e-8;
        const SqpKkt active =
            evaluate_kkt(*p.model, x, Vec::Zero(0), Vec::Zero(0), /*bound_tol=*/1e-6);
        const SqpKkt naive =
            evaluate_kkt(*p.model, x, Vec::Zero(0), Vec::Zero(0), /*bound_tol=*/0.0);
        EXPECT_LT(active.stationarity, 1e-9); // observed 2e-13
        EXPECT_NEAR(naive.stationarity, 1.0, 1e-6);
        EXPECT_GT(naive.stationarity, 1e6 * active.stationarity)
            << "the two tolerances must give qualitatively different verdicts, "
               "or this fixture is not pinning anything";
    }

    // (5) COMPLEMENTARITY is computed from the supplied lambda_i and cI, and
    // is pinned HERE rather than on a driver trajectory. On the only shipped
    // problem with general inequalities (HS76) every constraint is exactly
    // LINEAR, so the linearization reproduces cI exactly and the subproblem's
    // own complementarity lambda_i(j) * (Ji p + cI_j) == 0 lands as
    // lambda_i(j) * cI_j(x + p) == 0 at the very next iterate: the driver's
    // recorded complementarity is 0 or machine epsilon on EVERY reachable
    // HS76 trajectory (measured at tr_init = 0.1, 0.2, 0.25, 0.3, 0.5, 0.75,
    // 1, 1.5 and from three different start points -- max 7.1e-16 across all
    // of them). So there is no non-vacuous integration-level assertion to
    // make, and asserting "> 0" there would be pinning noise. A model with
    // genuinely NONLINEAR inequalities would show it; none is shipped yet.
    {
        const HsProblem p = make_hs(76);
        const Vec x = p.model->start_point();
        Vec li(3);
        li << 1.0, 0.0, 0.0; // a price on a row that is SLACK here
        const SqpKkt k = evaluate_kkt(*p.model, x, Vec::Zero(0), li, kBoundTol);
        // cI(x0) = (-2.5, -1.5, -1.0), so |lambda_i(0) * cI_0| = 2.5.
        EXPECT_NEAR(k.complementarity, 2.5, 1e-12);
    }

    // (4) FREE variable: the plain |grad L|. HS5 at its start point (0, 0),
    // interior to [-1.5,4] x [-3,3]: grad f = (-0.5, 3.5), measure 3.5.
    {
        const HsProblem p = make_hs(5);
        const Vec x = Vec::Zero(2);
        const SqpKkt k = evaluate_kkt(*p.model, x, Vec::Zero(0), Vec::Zero(0), kBoundTol);
        EXPECT_NEAR(k.stationarity, 3.5, 1e-12);
        EXPECT_LT(k.z.lpNorm<Eigen::Infinity>(), 1e-14);
    }
}

// The convergence test runs BEFORE any subproblem is built, so a solve
// started at a solution costs zero QPs.
TEST(SqpDriverContract, ConvergedStartSolvesNoSubproblems) {
    const HsProblem p = make_hs(5);
    Vec x_star(2);
    x_star << -0.5471975511965977, -1.547197551196598;

    SqpDriver driver{SqpOptions{}};
    const SqpSolution sol = driver.solve(*p.model, x_star);

    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_EQ(sol.counters.major_iters, 0);
    EXPECT_EQ(sol.counters.qp_minor_iters, 0);
    EXPECT_EQ(sol.counters.factorizations, 0);
    ASSERT_EQ(sol.history.size(), 1u);
    EXPECT_FALSE(sol.history[0].qp_solved);
    EXPECT_NEAR(sol.f, p.f_star, 1e-12);
}

namespace {

// A 1-D model whose objective, gradient and Hessian all go NaN once the
// iterate leaves |x| <= kFiniteRadius. Nothing about it is exotic: any model
// with a log, a sqrt or a division that the iteration can walk out of the
// domain of behaves this way, and full-step SQP with no globalization is
// exactly the setting in which that walk happens.
//
// The curvature is deliberately small against the slope, so the very first
// Newton step (-grad/hess = 1/0.01 = 100) overshoots kFiniteRadius by an
// order of magnitude: one legitimate major, then a NaN iterate. Both
// constants are also kept well inside qp_engine.h's unbounded-artifact scale
// (~1e7 at the default primal_delta) -- overshooting THAT instead makes the
// subproblem itself report kNumericalError, which exits through the
// QP-failure path and never exercises the non-finite-iterate branch this
// test is about.
class NanPastRadiusModel : public NlpModel {
  public:
    static constexpr double kFiniteRadius = 10.0;
    static constexpr double kCurvature = 0.01;

    Index n() const override { return 1; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    bool in_domain(const Vec &x) const { return std::abs(x(0)) <= kFiniteRadius; }

    double eval_f(const Vec &x) const override {
        if (!in_domain(x)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return 0.5 * kCurvature * x(0) * x(0) - x(0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(1);
        g(0) = in_domain(x) ? kCurvature * x(0) - 1.0 : std::numeric_limits<double>::quiet_NaN();
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(1, 1);
        h.insert(0, 0) =
            in_domain(x) ? obj_scale * kCurvature : std::numeric_limits<double>::quiet_NaN();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 1);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 1);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(1, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(1, kInfBound);
        return u;
    }
    Vec start_point() const override { return Vec::Zero(1); }
};

} // namespace

// A NaN ITERATE MUST NEVER BE CERTIFIED kOptimal, and before this test it
// was. Both residual measures are built by folding per-entry terms into a
// running maximum, and std::max(a, NaN) returns a -- so a NaN gradient or
// constraint value is SWALLOWED, both measures read 0.0, and the convergence
// gate fires on a point where nothing was actually measured. The driver
// returned kOptimal with f = nan after a single major.
//
// TASK 6 UPDATE, and it is a strict improvement rather than a relaxation.
// Under Task 4's full-step loop the driver MOVED onto the NaN point and then
// had nothing to do but report kNumericalError. With globalization the trial
// point is evaluated BEFORE it is adopted, and globalization.h rejects a
// non-finite trial outright ("nothing has been measured, so nothing can be
// accepted"), so the driver shrinks the radius and stays where it is. The
// property this test exists for -- a NaN point is never certified, and never
// silently becomes the answer -- is therefore asserted in its stronger form:
// the iteration never adopts one at all. NanPastRadiusModel has no minimizer
// inside its domain (the true minimum of 0.005 x^2 - x is at x = 100, outside
// |x| <= 10), so the honest outcome is running out of iterations while pinned
// against the domain edge.
//
// The stopped-AT-a-non-finite-iterate BRANCH stays reachable and is still
// covered below, by the one route the iteration cannot rescue: a START POINT
// the model cannot be evaluated at.
TEST(SqpDriverContract, NanIterateIsNeverAdoptedOrCertified) {
    NanPastRadiusModel model;
    SqpOptions opts;
    opts.tr_init = 1e3; // large enough that the first Newton step (100) is taken whole
    opts.max_iter = 10;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);
    record_history(sol, "nan_model");

    EXPECT_NE(sol.status, SqpStatus::kOptimal)
        << "a point the model cannot be evaluated at must never be certified";
    EXPECT_EQ(sol.status, SqpStatus::kMaxIter); // observed: pinned against the domain edge
    EXPECT_GE(sol.counters.rejected_steps, 1) << "the NaN trials must be REJECTED, not adopted";

    // No iterate the driver ever stood on was non-finite: every recorded row
    // carries a real objective and a real residual.
    ASSERT_FALSE(sol.history.empty());
    for (const SqpIterate &h : sol.history) {
        EXPECT_TRUE(std::isfinite(h.f)) << "trial " << h.trial;
        EXPECT_TRUE(std::isfinite(h.kkt_residual)) << "trial " << h.trial;
    }
    EXPECT_LT(std::abs(sol.x(0)), NanPastRadiusModel::kFiniteRadius)
        << "the returned point is inside the model's domain";
    EXPECT_TRUE(std::isfinite(sol.f));

    // The non-finite-ITERATE exit, still reachable: a start point inside the
    // model's NaN region. x0 itself is finite (a NaN x0 is a throw, not a
    // status -- see NonFiniteStartPointIsRejected), so this is the driver's
    // measurement of the MODEL failing, decided before any subproblem exists.
    Vec x0_outside(1);
    x0_outside << 50.0;
    const SqpSolution bad = driver.solve(model, x0_outside);
    EXPECT_EQ(bad.status, SqpStatus::kNumericalError);
    EXPECT_EQ(bad.counters.major_iters, 0);
    ASSERT_EQ(bad.history.size(), 1u);
    EXPECT_FALSE(bad.history[0].qp_solved) << "the solve stopped AT the bad iterate";
    EXPECT_TRUE(std::isnan(bad.history[0].kkt_residual))
        << "kkt_residual read " << bad.history[0].kkt_residual << " on a NaN iterate";
    // Multipliers are cleared: nothing priced at a NaN iterate is meaningful.
    EXPECT_LT(bad.z.lpNorm<Eigen::Infinity>(), 1e-300);

    // And the direct measurement, independent of the driver loop.
    Vec point(1);
    point << 100.0;
    const SqpKkt k = evaluate_kkt(model, point, Vec::Zero(0), Vec::Zero(0), 1e-6);
    EXPECT_FALSE(k.finite);
    EXPECT_FALSE(k.stationarity <= 1e-6) << "a non-finite measure must not satisfy the gate";
}

// PHASE-5 TASK 0, FIX ROUND 1 -- THE RETRY. What a caller DOES after the
// exit above is retry from a corrected x0, and the hand-off that exit emits
// must not sabotage that. It very nearly did: Task 0's first cut probed the
// model's structure on EVERY exit that built no subproblem, this one
// included, so its WarmStart came back `valid` with a matching hash. Fed
// back, it resolved kWarm -- and the 3-arg solve() takes its x FROM the warm
// object on a kWarm resolution, so the corrected x0 was DISCARDED and the
// retry re-ran at the very point the model could not evaluate, failing
// identically. A recovery turned into a repeat.
//
// So that one exit now emits a COLD object (valid = false, hash 0) and is not
// probed at all -- sqp_driver.h's make_warm_start, THE UNEVALUABLE EXIT.
// Pinned in the strongest available form: the retry is BIT-IDENTICAL to the
// 2-arg cold solve from the same corrected x0, which is what "the caller's
// own x0 was honoured" actually means. (The zero-major repair itself is
// untouched by this -- it is about solves that CONVERGED at a good point;
// tests/test_warm_start.cpp pins that half.)
TEST(SqpDriverContract, UnevaluableStartPointEmitsAColdHandOffSoARetryIsHonoured) {
    NanPastRadiusModel model;
    SqpOptions opts;
    opts.tr_init = 1e3;
    opts.max_iter = 10;
    SqpDriver driver(opts);

    Vec x0_outside(1);
    x0_outside << 50.0;
    const SqpSolution bad = driver.solve(model, x0_outside);
    ASSERT_EQ(bad.status, SqpStatus::kNumericalError);
    ASSERT_EQ(bad.counters.major_iters, 0) << "no subproblem was ever built";

    // THE HAND-OFF IS COLD, both halves. `valid` is the operative one -- the
    // ingest reads it first -- and the 0 hash is the same statement one level
    // down: nothing here was learned from a model evaluation that worked.
    EXPECT_FALSE(bad.warm_start.valid)
        << "an unevaluable point is not evidence a later solve may be fed";
    EXPECT_EQ(bad.warm_start.structure_hash, 0u);
    // The point itself is still REPORTED, for a caller inspecting the failure.
    ASSERT_EQ(bad.warm_start.x.size(), 1);
    EXPECT_DOUBLE_EQ(bad.warm_start.x(0), 50.0);

    // THE RETRY, from a corrected x0 well inside the model's domain.
    Vec x0_good(1);
    x0_good << 1.0;
    SqpDriver retry_driver(opts);
    const SqpSolution retry = retry_driver.solve(model, x0_good, bad.warm_start);
    SqpDriver cold_driver(opts);
    const SqpSolution cold = cold_driver.solve(model, x0_good);

    EXPECT_EQ(retry.counters.start_level_used, StartLevel::kCold)
        << "THE PIN: the failed solve's hand-off must not be ingested";
    EXPECT_NE(retry.status, SqpStatus::kNumericalError)
        << "the retry ran at the caller's x0, where the model IS evaluable";
    ASSERT_FALSE(retry.history.empty());
    EXPECT_TRUE(std::isfinite(retry.history.front().kkt_residual))
        << "the FIRST iterate measured was the corrected one, not the discarded 50";
    EXPECT_LT(std::abs(retry.x(0)), NanPastRadiusModel::kFiniteRadius);

    // BIT-FOR-BIT the same run as the 2-arg cold solve -- the same standard
    // WarmStart.StaleWarmIsSafe holds a foreign object to, and asserted at
    // that strength rather than described at it (fix round 2: the first cut
    // of this test claimed bit-identity while comparing `f` at 4 ULP and one
    // counter). `f` is EXPECT_EQ, not EXPECT_DOUBLE_EQ: these are two runs of
    // the SAME deterministic code path over the same inputs, so anything
    // short of exact equality would mean the ingest changed the arithmetic,
    // which is precisely what this test denies. Every counter that records
    // WORK is compared -- a run that reached the same point by a different
    // amount of it would not be the same run.
    EXPECT_EQ(retry.status, cold.status);
    EXPECT_EQ(retry.x, cold.x);
    EXPECT_EQ(retry.f, cold.f);
    EXPECT_EQ(retry.lambda_e, cold.lambda_e);
    EXPECT_EQ(retry.lambda_i, cold.lambda_i);
    EXPECT_EQ(retry.z, cold.z);
    EXPECT_EQ(retry.counters.major_iters, cold.counters.major_iters);
    EXPECT_EQ(retry.counters.qp_minor_iters, cold.counters.qp_minor_iters);
    EXPECT_EQ(retry.counters.factorizations, cold.counters.factorizations);
    EXPECT_EQ(retry.counters.steps_accepted, cold.counters.steps_accepted);
    EXPECT_EQ(retry.counters.rejected_steps, cold.counters.rejected_steps);
    EXPECT_EQ(retry.counters.start_level_used, cold.counters.start_level_used);
    ASSERT_EQ(retry.history.size(), cold.history.size());
    for (std::size_t i = 0; i < retry.history.size(); ++i) {
        EXPECT_EQ(retry.history[i].f, cold.history[i].f) << "history row " << i;
        EXPECT_EQ(retry.history[i].tr_radius, cold.history[i].tr_radius) << "history row " << i;
    }
}

// The same hole reachable without any iteration at all: a caller-supplied
// non-finite x0 was measured as stationary-and-feasible and certified
// kOptimal in zero majors. x0 is caller input, so this is a rejection (like
// the constructor's NaN tr_init), not a status.
TEST(SqpDriverContract, NonFiniteStartPointIsRejected) {
    NanPastRadiusModel model;
    SqpDriver driver{SqpOptions{}};

    Vec nan_x0(1);
    nan_x0 << std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(driver.solve(model, nan_x0), std::invalid_argument);

    Vec inf_x0(1);
    inf_x0 << std::numeric_limits<double>::infinity();
    EXPECT_THROW(driver.solve(model, inf_x0), std::invalid_argument);

    // The delegating overload routes through the same check, so a model whose
    // start_point() is non-finite is rejected too.
    const HsProblem p = make_hs(5);
    Vec good = p.model->start_point();
    EXPECT_NO_THROW(driver.solve(*p.model, good));
}

// REGRESSION, and the reason SqpDriver zeroes seed.x. qp_engine.h section 6
// centers the trust region on the SOLVE'S OWN start point, which on a warm
// solve is clamp(seed.x, ...). Handing the previous STEP through as the seed
// re-centers the box on p_prev, and the driver then takes steps of up to
// |p_prev| + Delta while believing the radius is Delta. Measured before the
// fix, HS6 from its published start at tr_init = 1.0: major 1's step had
// inf-norm 1.6 at a radius of 1.0, and the run ended kInfeasible two majors
// later. Every step must respect the radius.
//
// TASK 6 UPDATE. The radius is no longer constant, so the bound is now the
// PER-ROW radius (SqpIterate::tr_radius, the value actually handed to that
// subproblem) rather than tr_init -- against tr_init the test would now fail
// for the right reason (the radius legitimately GREW) and would go on failing
// for the wrong one. The centering claim itself is unchanged and is if
// anything better exercised: the REJECTION RETRY re-solves at the same
// iterate with a smaller radius and a seed taken from the rejected solve, so
// a driver that let seed.x carry the rejected step would re-center the
// shrunken box on exactly the step it just rejected. HS1 and HS5-at-2.0 both
// take that path here.
//
// FIX ROUND 1 [C1]: HS5 AT tr_init = 10 IS THE CASE THIS TEST WAS MISSING,
// and it failed. There the accepted/rejected steps run into the SUBPROBLEM'S
// OWN REAL bounds (the box corner of HS5's [-1.5,4] x [-3,3]), so the seed
// carries an honest kAtUpper/kAtLower hint at a bound FAR from p = 0 -- and
// the engine used to materialize that hint onto x BEFORE computing the
// trust-region window about x, re-centering the radius on the bound. Measured
// before the fix: every one of 60 trials returned the same step of inf-norm 6
// (bit-identical) while the radius was halved all the way down to 1.7e-17,
// and the solve ended kMaxIter at the wrong point. Zeroing seed.x cannot
// prevent this -- the zero is overwritten by the pin -- so the fix is
// engine-side (qp_engine.h's WINDOW-CONSISTENCY RULE) and this fixture is the
// driver-level guard on it.
TEST(SqpDriverContract, TrustRegionIsCenteredOnTheCurrentIterate) {
    struct Case {
        int number;
        double tr_init;
    };
    // HS5 appears twice on purpose: 2.0 is its rejecting radius, and 10.0 is
    // the C1 case (steps land on the subproblem's real bounds).
    for (const Case c : {Case{6, 1.0}, Case{3, 1.0}, Case{1, 1.0}, Case{5, 2.0}, Case{5, 10.0}}) {
        const HsProblem p = make_hs(c.number);
        SqpOptions opts;
        opts.tr_init = c.tr_init;
        opts.max_iter = 60;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);
        record_history(sol, fmt::format("hs{}_tr{:g}", c.number, c.tr_init));

        ASSERT_FALSE(sol.history.empty());
        for (const SqpIterate &h : sol.history) {
            // The radius is an l-infinity box about p = 0; a step may reach
            // the radius but never exceed it (rounding aside).
            EXPECT_LE(h.step_norm, h.tr_radius * (1.0 + 1e-9))
                << "HS" << c.number << " @ " << c.tr_init << " major " << h.trial;
            EXPECT_LE(h.tr_radius, opts.tr_max);
        }
        // A radius the steps do not respect is not merely untidy: it stalls
        // the solve, because shrinking it changes nothing. Every fixture here
        // must still SOLVE.
        EXPECT_EQ(sol.status, SqpStatus::kOptimal) << "HS" << c.number << " @ " << c.tr_init;
        EXPECT_NEAR(sol.f, p.f_star, 1e-6) << "HS" << c.number << " @ " << c.tr_init;
    }

    // THE SWEEP, which is how C1 was quantified: every shipped problem at
    // every radius from 0.25 to 30. The radius invariant is asserted and
    // NOTHING ELSE -- some of these configurations legitimately end kMaxIter
    // or kInfeasible, and this test is not the place to pin which. Measured
    // on THIS grid before the engine's window-consistency fix: 4 of these 48
    // configurations returned a step larger than the radius it was solved at
    // (the review's own sweep, over a different radius grid, counted 6 -- the
    // exact number is grid-dependent and the point is that it is not zero).
    // Now none does, and this loop is what keeps it that way.
    Index violations = 0;
    for (const int number : {1, 3, 5, 6, 7, 76}) {
        const HsProblem p = make_hs(number);
        for (const double tr : {0.25, 0.5, 1.0, 2.0, 3.0, 5.0, 10.0, 30.0}) {
            SqpOptions opts;
            opts.tr_init = tr;
            opts.max_iter = 60;
            SqpDriver driver(opts);
            const SqpSolution sol = driver.solve(*p.model);
            for (const SqpIterate &h : sol.history) {
                if (h.step_norm > h.tr_radius * (1.0 + 1e-9)) {
                    ++violations;
                    ADD_FAILURE() << "HS" << number << " @ tr_init " << tr << " trial " << h.trial
                                  << ": step " << h.step_norm << " at radius " << h.tr_radius;
                    break;
                }
            }
        }
    }
    EXPECT_EQ(violations, 0);
}

TEST(SqpDriverContract, RejectsUnusableOptionsAndStartPoints) {
    {
        SqpOptions o;
        o.kkt_tol = 0.0;
        EXPECT_THROW(SqpDriver{o}, std::invalid_argument);
    }
    {
        SqpOptions o;
        o.feas_tol = -1.0;
        EXPECT_THROW(SqpDriver{o}, std::invalid_argument);
    }
    {
        SqpOptions o;
        o.max_iter = -1;
        EXPECT_THROW(SqpDriver{o}, std::invalid_argument);
    }
    {
        SqpOptions o;
        o.tr_init = -1.0;
        EXPECT_THROW(SqpDriver{o}, std::invalid_argument);
    }
    {
        // tr_init == 0 pins every subproblem box to p = 0, so no iterate ever
        // moves and the solve burns max_iter majors to report kMaxIter.
        // Rejected rather than left as a silent stall.
        SqpOptions o;
        o.tr_init = 0.0;
        EXPECT_THROW(SqpDriver{o}, std::invalid_argument);
    }
    {
        SqpOptions o;
        o.tr_init = std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(SqpDriver{o}, std::invalid_argument);
    }
    {
        // +inf stays legal: it is SolveOverrides' sentinel for "no radius".
        // It is also EXEMPT from the tr_max >= tr_init check below -- see the
        // constructor's note on why an infinite radius is not the driver's to
        // manage and so has nothing to cap.
        SqpOptions o;
        o.tr_init = std::numeric_limits<double>::infinity();
        EXPECT_NO_THROW(SqpDriver{o});
    }
    {
        // tr_max IS READ from Task 6 on (it caps the growth rule), so a
        // ceiling below the starting radius is a contradiction rather than an
        // ignored field.
        SqpOptions o;
        o.tr_init = 2.0;
        o.tr_max = 1.0;
        EXPECT_THROW(SqpDriver{o}, std::invalid_argument);
    }
    {
        SqpOptions o;
        o.tr_max = std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(SqpDriver{o}, std::invalid_argument);
    }
    {
        // Equality is legal: it just means "never grow".
        SqpOptions o;
        o.tr_init = 2.0;
        o.tr_max = 2.0;
        EXPECT_NO_THROW(SqpDriver{o});
    }
    {
        const HsProblem p = make_hs(5);
        SqpDriver driver{SqpOptions{}};
        EXPECT_THROW(driver.solve(*p.model, Vec::Zero(3)), std::invalid_argument);
    }
}

// M3 FINAL REVIEW, S-1 (Critical). THE EVAL BOUNDARY CHECKS WHAT THE MODEL
// RETURNS, not just what the caller passed in. Before this, eval_nlp
// size-checked `x` -- the one quantity it did not obtain from the model -- and
// took all five callback returns on trust. A consumer model that mis-sizes one
// of them therefore reached an out-of-bounds read in Release rather than a
// diagnostic: the case reproduced by the kCi arm below (an EMPTY cI on a model
// declaring mi() == 1) slipped past allFinite(), which is vacuously true on an
// empty vector, and was then indexed by evaluate_kkt's `for j < model.mi()`.
//
// EVERY ARM ASSERTS THE MESSAGE, not just the throw. "size 0, expected 1" is
// not actionable on a model with six sized returns unless it says WHICH one,
// so the name of the offending callback is part of the contract.
//
// WHICH BOUNDARY EACH ARM PINS, RESTATED HERE, because "the eval boundary" is
// no longer one place. The driver's solve path evaluates through the Level 2
// aggregate and does not call the free functions at all any more:
//   (a) and (b) pin the FREE FUNCTIONS -- eval_nlp and eval_nlp_values, which
//       remain in drivers/sqp_driver.h as a supported direct-use surface for
//       tests and for callers measuring a single point. They are called
//       directly here, never through solve(), so they pin exactly that
//       retained surface and nothing about the loop.
//   (c) is THE LIVE-PATH ARM. It goes through driver.solve(), and the box is
//       the one mis-sized return that still reaches a driver-owned check
//       there: require_declared_box, at the model-taking entries in
//       src/drivers/sqp_driver.cpp, which runs before the model is wrapped in
//       a bridge so that this two-block message is the one that fires.
//   (d) is the control for all of the above.
// The solve path's rejection of the OTHER mis-sized returns has moved to the
// bridge's own entries (src/model/nlp_model_aggregate.cpp), which name each
// offending callback in their own messages -- the same contract as here, in a
// different voice, and covered by that file's own suite rather than restated
// through a solve() here.
TEST(SqpDriverContract, MisSizedCallbackReturnsAreRejectedByName) {
    using Which = MisSizingModel::Which;
    const Vec x7 = make_hs(7).model->start_point();
    const Vec x10 = make_hs(10).model->start_point();

    // (a) eval_nlp's five derivative returns. HS7 (n=2, me=1, mi=0) carries the
    // equality arms and HS10 (n=2, me=0, mi=1) the inequality ones, because
    // eval_nlp only CALLS the block callbacks whose dimension is nonzero.
    struct Arm {
        int hs;
        Which which;
        const char *names;
    };
    for (const Arm &arm : {Arm{7, Which::kGrad, "eval_grad"}, Arm{7, Which::kCe, "eval_ce"},
                           Arm{7, Which::kJacE, "eval_jac_e"}, Arm{10, Which::kCi, "eval_ci"},
                           Arm{10, Which::kJacI, "eval_jac_i"}}) {
        SCOPED_TRACE(arm.names);
        const MisSizingModel model(make_hs(arm.hs).model, arm.which);
        const Vec &x = arm.hs == 7 ? x7 : x10;
        const std::string msg = message_of_throw([&] { (void)eval_nlp(model, x); });
        EXPECT_NE(msg.find(arm.names), std::string::npos)
            << "the throw must name the callback that misbehaved; got: " << msg;
        EXPECT_NE(msg.find("eval_nlp"), std::string::npos) << msg;
    }

    // (b) eval_nlp_values' two out-parameters. One call produces both, so both
    // are checked unconditionally -- including on a block whose declared
    // dimension is zero, which is what the HS10 cE arm exercises.
    for (const Arm &arm : {Arm{10, Which::kValuesCe, "cE"}, Arm{10, Which::kValuesCi, "cI"}}) {
        SCOPED_TRACE(arm.names);
        const MisSizingModel model(make_hs(arm.hs).model, arm.which);
        const std::string msg = message_of_throw([&] { (void)eval_nlp_values(model, x10); });
        EXPECT_NE(msg.find("eval_values"), std::string::npos)
            << "the throw must name eval_values; got: " << msg;
        EXPECT_NE(msg.find(arm.names), std::string::npos) << "and which block: " << msg;
    }

    // (c) THE BOX, checked once at solve entry rather than at each of the
    // several sites that index it coordinate-wise.
    for (const Which which : {Which::kLower, Which::kUpper}) {
        SCOPED_TRACE(which == Which::kLower ? "lower" : "upper");
        const MisSizingModel model(make_hs(5).model, which);
        SqpDriver driver{SqpOptions{}};
        const std::string msg =
            message_of_throw([&] { (void)driver.solve(model, model.start_point()); });
        EXPECT_NE(msg.find("model.lower()"), std::string::npos) << msg;
        EXPECT_NE(msg.find("model.upper()"), std::string::npos) << msg;
    }

    // (d) THE CONTROL. The same decorator with no arm selected mis-sizes
    // nothing, so every path above is clean -- without this the assertions
    // could all be passing on a decorator that is simply broken.
    const MisSizingModel honest(make_hs(76).model, Which::kNone);
    EXPECT_NO_THROW((void)eval_nlp(honest, honest.start_point()));
    EXPECT_NO_THROW((void)eval_nlp_values(honest, honest.start_point()));
    SqpDriver clean{SqpOptions{}};
    EXPECT_NO_THROW((void)clean.solve(honest, honest.start_point()));
}

// M3 FINAL REVIEW, S-8 (round 2). THE THIRD DOOR INTO THE SAME BUNDLE.
// upgrade_to_full is the eval boundary S-1 did not close: it takes
// eval_grad/eval_jac_e/eval_jac_i exactly as eval_nlp does, and it is the one
// an ACCEPTED trial comes through -- a values-only or SOC-corrected point the
// funnel decided to keep is upgraded here, and the NlpEval that results is
// what the NEXT major linearizes from. So a mis-sized return here is not a
// diagnostic gap on a rare path; it is the same Release out-of-bounds read
// S-1 closed, one function along.
//
// EXERCISED DIRECTLY, not through a solve. upgrade_to_full is a free inline
// function with a documented precondition (its NlpEval must be
// eval_nlp_values' output at THIS x), which this test can satisfy exactly --
// and doing so pins the boundary itself rather than whichever solve happens to
// route through it today.
TEST(SqpDriverContract, MisSizedUpgradeReturnsAreRejectedByName) {
    using Which = MisSizingModel::Which;

    // HS7 (n=2, me=1, mi=0) for the gradient and equality-Jacobian arms, HS10
    // (n=2, me=0, mi=1) for the inequality-Jacobian arm -- upgrade_to_full,
    // like eval_nlp, only calls the block callbacks whose dimension is nonzero.
    struct Arm {
        int hs;
        Which which;
        const char *names;
    };
    for (const Arm &arm : {Arm{7, Which::kGrad, "eval_grad"}, Arm{7, Which::kJacE, "eval_jac_e"},
                           Arm{10, Which::kJacI, "eval_jac_i"}}) {
        SCOPED_TRACE(arm.names);
        const MisSizingModel model(make_hs(arm.hs).model, arm.which);
        const Vec x = model.start_point();

        // The documented precondition: a values-only bundle at this same x.
        // The decorator's eval_values is untouched on these arms, so this call
        // succeeds and the throw below can only come from the upgrade.
        NlpEval ev;
        ASSERT_NO_THROW(ev = eval_nlp_values(model, x));

        const std::string msg = message_of_throw([&] { upgrade_to_full(model, x, ev); });
        EXPECT_NE(msg.find(arm.names), std::string::npos)
            << "the throw must name the callback that misbehaved; got: " << msg;
        EXPECT_NE(msg.find("upgrade_to_full"), std::string::npos)
            << "and the boundary it was caught at; got: " << msg;
    }

    // THE CONTROL: an honest model upgrades cleanly, and the upgraded bundle is
    // the one upgrade_to_full's contract promises -- byte-identical to a fresh
    // eval_nlp at the same x. Without this the arms above could be passing on a
    // boundary that rejects everything.
    const MisSizingModel honest(make_hs(76).model, Which::kNone);
    const Vec x = honest.start_point();
    NlpEval upgraded = eval_nlp_values(honest, x);
    EXPECT_NO_THROW(upgrade_to_full(honest, x, upgraded));

    const NlpEval fresh = eval_nlp(honest, x);
    EXPECT_EQ(upgraded.grad, fresh.grad);
    EXPECT_EQ(upgraded.all_finite, fresh.all_finite);
    EXPECT_EQ(Eigen::MatrixXd(upgraded.Ji), Eigen::MatrixXd(fresh.Ji));
}

// MODEL EVALUATION IS THE COST THAT MATTERS on tycho's target workloads, so
// the per-major budget is a pinned contract and not an implementation
// detail. The convergence test and the subproblem read the SAME five
// derivative quantities at the same x; before NlpEval existed they each
// evaluated them independently, costing ~12 virtual calls per major where 7
// suffice. Every one of Tasks 5-9 calls build_subproblem, so a regression
// here would multiply across all of them.
//
// THE BUDGET: per HISTORY ROW exactly one eval_f, one eval_grad, one eval_ce
// + one eval_jac_e if me > 0, one eval_ci + one eval_jac_i if mi > 0. Per
// ACCEPTED major exactly one eval_hess -- the Hessian is the expensive one
// and the convergence test never needs it, so it is deliberately NOT in the
// bundle and is never evaluated at the final iterate. Equalities are exact: a
// "<=" here would pass on a double evaluation.
//
// TASK 6 RESTATES THE UNIT, NOT THE BUDGET. Globalization has to evaluate the
// TRIAL point before adopting it, so the count is now one evaluation per
// TRIAL (plus the start point) rather than per accepted iterate -- which is
// the same number, because there is exactly one history row per trial plus
// one for the point the solve stopped at. What DOES change is eval_hess: the
// subproblem is rebuilt only when the iterate moves, so a rejected trial pays
// no Hessian, and the count is majors MINUS rejected_steps. Written that way
// rather than as `== majors` so the assertion stays true (and stays
// meaningful) on a fixture that does reject; the three fixtures here happen
// to reject nothing, which is itself asserted.
//
// PHASE-5 TASK 0 ADDS ONE EXCEPTION THIS FIXTURE CANNOT SEE, named so the
// identity below is not read as more universal than it is: a solve that
// builds NO subproblem at all (converged at its start point, or a zero
// budget) pays one eval_hess in make_warm_start's zero-major probe, with
// steps_accepted == 0 (sqp_types.h's steps_accepted note). All three
// fixtures here converge through several majors, so none of them reaches it
// and no count below moves.
//
// THE DRIVER'S BRIDGE LAY ADDS ONE EXTRA CLAIM PASS, WHICH IS WHY THE THREE
// DERIVATIVE IDENTITIES CARRY AN EXPLICIT `+ 1` BELOW AND THE THREE VALUE ONES
// DO NOT. The driver's solve path consumes the Level 2 aggregate
// (detail/drivers/aggregate_eval_seam.h), and a model-taking solve() builds
// one bridge over the caller's model for the duration of the call. Building
// it is a CLAIM PASS: it walks eval_hess,
// eval_jac_e and eval_jac_i once each at the model's start point, because the
// claims must exist before any evaluation can scatter into them. That is a
// per-SOLVE constant, not a per-major one, and it is the whole of the change
// to this test -- the DRIVER's own economy (one evaluation per trial, one
// Hessian per accepted major) is exactly what it was, and every right-hand
// side below is still the same driver counter it always was. A caller who
// does not want to pay it per solve holds an NlpModelAggregate and uses
// SqpDriver's aggregate-taking entry, which builds no bridge of its own.
TEST(SqpDriverContract, CallCountPerMajorIsBounded) {
    for (const int number : {6, 76, 5}) {
        CountingModel model(make_hs(number).model);
        SqpOptions opts;
        opts.max_iter = 40;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal) << "HS" << number;

        const Index rows = static_cast<Index>(sol.history.size());
        const Index majors = sol.counters.major_iters;
        const Index accepted = sol.counters.steps_accepted;
        const std::string tag = fmt::format("HS{}", number);

        // THE BRIDGE LAY IS THE `lay` TERM BELOW, and it is a per-BRIDGE cost
        // rather than a per-major one: constructing an NlpModelAggregate walks
        // the model's three derivative patterns ONCE, at the model's own start
        // point -- one eval_hess unconditionally, one eval_jac_e iff me() > 0,
        // one eval_jac_i iff mi() > 0 (the claim pass, stated in the
        // constructor's own doc at include/hven/model/nlp_model_aggregate.h).
        // Each fixture here makes exactly ONE solve(model, ...) call and enters
        // no restoration, so exactly ONE bridge is built over it and the term
        // is counted once. The VALUE half of the economy is untouched, which is
        // why n_f/n_grad/n_ce/n_ci below carry no such term: a lay reads no
        // value callback at all.
        const Index lay = 1 + (model.me() > 0 ? 1 : 0) + (model.mi() > 0 ? 1 : 0);

        EXPECT_EQ(model.n_f, rows) << tag;
        EXPECT_EQ(model.n_grad, rows) << tag;
        // + 1 is the lay's own eval_hess; see `lay` above.
        EXPECT_EQ(model.n_hess, accepted + 1) << tag;
        EXPECT_EQ(model.n_ce, model.me() > 0 ? rows : 0) << tag;
        // + 1 is the lay's own eval_jac_e, paid because this model has rows.
        EXPECT_EQ(model.n_jac_e, model.me() > 0 ? rows + 1 : 0) << tag;
        EXPECT_EQ(model.n_ci, model.mi() > 0 ? rows : 0) << tag;
        // + 1 is the lay's own eval_jac_i, paid because this model has rows.
        EXPECT_EQ(model.n_jac_i, model.mi() > 0 ? rows + 1 : 0) << tag;

        // Stated as a total too, because that is the number a future reader
        // will actually compare against: 2 + 2*[me>0] + 2*[mi>0] per row, plus
        // one Hessian per ACCEPTED major, plus the one bridge lay.
        const Index per_row = 2 + (model.me() > 0 ? 2 : 0) + (model.mi() > 0 ? 2 : 0);
        const Index total = model.n_f + model.n_grad + model.n_ce + model.n_ci + model.n_jac_e +
                            model.n_jac_i + model.n_hess;
        EXPECT_EQ(total, per_row * rows + accepted + lay) << tag;
        ::testing::Test::RecordProperty(fmt::format("hs{}_model_calls", number),
                                        fmt::format("{} over {} rows / {} majors ({} rejected)",
                                                    total, rows, majors,
                                                    sol.counters.rejected_steps));
    }

    // The three fixtures above take every step they are offered, so `accepted`
    // and `majors` coincide there -- which is why the rejected-trial half of
    // the budget is pinned separately, on a fixture that does reject, by
    // SqpDriverTrustRegion.RejectedTrialsPayNoHessian.
}

// TASK 8 (the eval-economics carry): THE CEILING SHORT-CIRCUIT. A `warm`
// object that is `valid`, carries a nonzero structure_hash, and is
// dimensionally plausible against `model` would normally make solve_impl
// build a probe subproblem to test that hash -- UNLESS opts_.start_level is
// StartLevel::kCold, in which case resolved_level is capped back to kCold a
// few lines later NO MATTER WHAT the probe finds. Before this task the probe
// ran anyway (docs/notes/2026-07-30-phase-4-close-carries.md: "one wasted
// eval per warm-looking call"); this test pins that it no longer does, by
// comparing a start_level == kCold, 3-arg solve carrying a perfectly valid
// `warm` against a PLAIN 2-arg (always-cold) solve of a fresh copy of the
// same model from the same start point -- the two solves are FORCED to the
// same resolved_level (kCold) by construction, so their trajectories are
// bit-identical, and any difference in model call counts can only be the
// probe.
TEST(SqpDriverContract, ColdCeilingSkipsTheWarmResolutionProbeEntirely) {
    const HsProblem seed_problem = make_hs(6);
    SqpDriver seed_driver{SqpOptions{}};
    const SqpSolution seeded = seed_driver.solve(*seed_problem.model);
    ASSERT_EQ(seeded.status, SqpStatus::kOptimal);
    ASSERT_TRUE(seeded.warm_start.valid);
    ASSERT_NE(seeded.warm_start.structure_hash, 0u);

    CountingModel ceiling_model(make_hs(6).model);
    SqpOptions ceiling_opts;
    ceiling_opts.start_level = StartLevel::kCold;
    SqpDriver ceiling_driver(ceiling_opts);
    const SqpSolution ceiling_sol =
        ceiling_driver.solve(ceiling_model, ceiling_model.start_point(), seeded.warm_start);

    CountingModel plain_model(make_hs(6).model);
    SqpDriver plain_driver{SqpOptions{}};
    const SqpSolution plain_sol = plain_driver.solve(plain_model);

    ASSERT_EQ(ceiling_sol.status, SqpStatus::kOptimal);
    ASSERT_EQ(ceiling_sol.counters.start_level_used, StartLevel::kCold);
    ASSERT_EQ(plain_sol.status, SqpStatus::kOptimal);

    // THE PROBE NEVER RAN: every model call count and evals_values match the
    // plain cold solve exactly -- no extra eval_hess/eval_jac_e/eval_values
    // for a probe whose resolved_level the ceiling was always going to
    // discard.
    EXPECT_EQ(ceiling_model.n_hess, plain_model.n_hess);
    EXPECT_EQ(ceiling_model.n_jac_e, plain_model.n_jac_e);
    EXPECT_EQ(ceiling_model.n_jac_i, plain_model.n_jac_i);
    EXPECT_EQ(ceiling_model.n_f, plain_model.n_f);
    EXPECT_EQ(ceiling_model.n_grad, plain_model.n_grad);
    EXPECT_EQ(ceiling_sol.counters.evals_values, plain_sol.counters.evals_values);
    EXPECT_EQ(ceiling_sol.counters.evals_full, plain_sol.counters.evals_full);
}

// TASK 8: THE WARM-RESOLUTION PROBE ITSELF IS VALUES-ONLY, plus explicit
// Je/Ji -- never a gradient. Isolated the same way as the ceiling test just
// above (two solves forced to the SAME resolved_level, hence the same
// trajectory, so any call-count difference is the probe alone), except this
// `warm` is dimensionally plausible with a DELIBERATELY WRONG
// structure_hash: the dimension check still passes (opts_.start_level
// defaults to kWarm, above kCold), so the probe still runs and pays its cost,
// but the hash mismatch does not raise the level -- giving a bit-identical
// trajectory to the CONTROL arm to compare against, with the probe's own
// extra calls isolated on top.
//
// PHASE-6 TASK 5 CHANGED THE CONTROL ARM, NOT THE CLAIM. The claim is still
// "the probe costs exactly one eval_hess + one eval_jac_e + one eval_values
// and never an eval_grad", and it is still isolated by forcing two solves to
// the same resolved level. What moved is WHICH level that is: a hash mismatch
// on a dimensionally-compatible, finite object used to resolve kCold, so a
// plain 2-arg cold solve was the right control; it now resolves kSeeded
// (warm_start.h's StartLevel note), whose trajectory is NOT a cold one -- the
// object's x, duals and activity hint are all ingested. The control is
// therefore a second solve fed THE SAME OBJECT with the ceiling clamped to
// kSeeded, which by sqp_driver.h's generalized ceiling short-circuit skips the
// probe entirely while resolving to the identical level. Same ingest, same
// trajectory, one probe of difference -- a STRICTLY BETTER isolation than the
// old one, because the two arms now share the warm object as well as the
// level.
TEST(SqpDriverContract, WarmResolutionProbeIsValuesOnlyNeverFetchesAGradient) {
    const HsProblem seed_problem = make_hs(6);
    SqpDriver seed_driver{SqpOptions{}};
    const SqpSolution seeded = seed_driver.solve(*seed_problem.model);
    ASSERT_TRUE(seeded.warm_start.valid);

    WarmStart mismatched = seeded.warm_start;
    mismatched.structure_hash ^= 0xdeadbeefULL; // dimensionally fine, hash wrong

    CountingModel probed_model(make_hs(6).model);
    SqpDriver probed_driver{SqpOptions{}}; // default start_level == kWarm
    const SqpSolution probed_sol =
        probed_driver.solve(probed_model, probed_model.start_point(), mismatched);

    // THE CONTROL: same object, same resulting level, ceiling clamped so the
    // probe is never built.
    CountingModel plain_model(make_hs(6).model);
    SqpOptions plain_opts;
    plain_opts.start_level = StartLevel::kSeeded;
    SqpDriver plain_driver(plain_opts);
    const SqpSolution plain_sol =
        plain_driver.solve(plain_model, plain_model.start_point(), mismatched);

    ASSERT_EQ(probed_sol.status, SqpStatus::kOptimal);
    ASSERT_EQ(probed_sol.counters.start_level_used, StartLevel::kSeeded)
        << "the corrupted hash must actually mismatch (kWarm would mean it collided), and a "
           "dimensionally-compatible finite object must still be seeded";
    ASSERT_EQ(plain_sol.status, SqpStatus::kOptimal);
    ASSERT_EQ(plain_sol.counters.start_level_used, StartLevel::kSeeded)
        << "the control must reach the SAME level, or the trajectories are not comparable";
    ASSERT_EQ(probed_sol.counters.major_iters, plain_sol.counters.major_iters)
        << "and the same trajectory, so every call-count difference below is the probe alone";

    // Exactly one extra eval_hess/eval_jac_e (the probe's own H/Ae pattern
    // fetch) beyond the control's -- and NEVER an extra eval_grad, NEVER an
    // extra full-eval count: the probe's f/cE/cI come through eval_values,
    // counted as evals_values, not evals_full.
    EXPECT_EQ(probed_model.n_hess, plain_model.n_hess + 1);
    EXPECT_EQ(probed_model.n_jac_e, plain_model.n_jac_e + 1);
    EXPECT_EQ(probed_model.n_grad, plain_model.n_grad)
        << "the probe must never fetch a gradient -- only Je/Ji/H, whose PATTERN is hashed";
    EXPECT_EQ(probed_sol.counters.evals_values, plain_sol.counters.evals_values + 1);
    EXPECT_EQ(probed_sol.counters.evals_full, plain_sol.counters.evals_full);
}

// PHASE-6 TASK 5, THE CEILING SHORT-CIRCUIT'S NEW SETTING, pinned as its own
// statement rather than inferred from the test above: a driver capped at
// StartLevel::kSeeded pays NO resolution probe even when the object it is fed
// carries a PERFECTLY MATCHING hash -- the probe's only possible product is a
// level the ceiling is about to discard, which is the identical argument the
// kCold ceiling's short-circuit rests on (Task 8's own note in sqp_driver.h).
// Isolated against a kSeeded-capped solve fed a hash-BROKEN copy of the same
// object: both resolve kSeeded and run the same trajectory, so an equal call
// count is the probe having been skipped in both.
TEST(SqpDriverContract, SeededCeilingSkipsTheWarmResolutionProbeEntirely) {
    const HsProblem seed_problem = make_hs(6);
    SqpDriver seed_driver{SqpOptions{}};
    const SqpSolution seeded = seed_driver.solve(*seed_problem.model);
    ASSERT_TRUE(seeded.warm_start.valid);
    ASSERT_NE(seeded.warm_start.structure_hash, 0u);

    SqpOptions capped;
    capped.start_level = StartLevel::kSeeded;

    CountingModel matching_model(make_hs(6).model);
    SqpDriver matching_driver(capped);
    const SqpSolution matching_sol =
        matching_driver.solve(matching_model, matching_model.start_point(), seeded.warm_start);

    WarmStart broken = seeded.warm_start;
    broken.structure_hash ^= 0xdeadbeefULL;
    CountingModel broken_model(make_hs(6).model);
    SqpDriver broken_driver(capped);
    const SqpSolution broken_sol =
        broken_driver.solve(broken_model, broken_model.start_point(), broken);

    ASSERT_EQ(matching_sol.counters.start_level_used, StartLevel::kSeeded)
        << "the ceiling caps a would-be kWarm object at kSeeded";
    ASSERT_EQ(broken_sol.counters.start_level_used, StartLevel::kSeeded);

    EXPECT_EQ(matching_model.n_hess, broken_model.n_hess);
    EXPECT_EQ(matching_model.n_jac_e, broken_model.n_jac_e);
    EXPECT_EQ(matching_model.n_grad, broken_model.n_grad);
    EXPECT_EQ(matching_sol.counters.evals_values, broken_sol.counters.evals_values)
        << "THE PIN: a matching hash costs no probe under a kSeeded ceiling";
    EXPECT_EQ(matching_sol.counters.evals_full, broken_sol.counters.evals_full);
    EXPECT_EQ(matching_sol.counters.major_iters, broken_sol.counters.major_iters);
}

// =========================================================================
// SqpDriverEquality -- (a) quadratic convergence.
// =========================================================================

// THE BRIEF'S TEST (a). HS7 (min ln(1+x1^2) - x2 s.t. (1+x1^2)^2 + x2^2 = 4),
// started at (0.5, 1.5) -- inside the region of attraction of
// x* = (0, sqrt 3) -- converges in 7 majors, and the last three KKT
// residuals contract QUADRATICALLY.
//
// WHY (0.5, 1.5) RATHER THAN HS7'S PUBLISHED (2, 2). Not because the
// published start fails -- it converges too, and
// PublishedStartsAlsoConverge below pins that -- but because it is
// RADIUS-SENSITIVE in a way that would make a quadratic-convergence claim
// rest on a lucky default. Measured, tr_init sweep from the published start
// at kkt_tol = 1e-8:
//     3 -> 9 majors, 4 -> 10, 5 -> 11, 6 -> 12, 8 -> 10, 10 -> 8, 12 -> 8,
//     15 -> 8, 20 -> 15, 30 -> 16, 50 -> kInfeasible, 100 -> 23,
//     200 -> kInfeasible.
// Only tr_init in [10, 15] fits the brief's "<= 8 majors" at all, and both
// neighbours of that window behave qualitatively differently. From (0.5, 1.5)
// the same sweep is FLAT -- 7 majors at every one of tr_init = 2, 3, 5, 10,
// 30, 100, 1e3, 1e5 -- and no iterate's step ever reaches the radius
// (asserted below), so what is being measured really is unrestricted
// full-step Newton on the KKT system and not a trust-region artifact. That
// is the property the quadratic claim needs.
TEST(SqpDriverEquality, QuadraticConvergenceOnHs7) {
    const HsProblem p = make_hs(7);
    Vec x0(2);
    x0 << 0.5, 1.5;

    SqpOptions opts;
    opts.tr_init = 10.0; // provably inactive here; see the assertion below
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model, x0);
    record_history(sol, "hs7");

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_LE(sol.counters.major_iters, 8); // observed: 7
    EXPECT_EQ(sol.history.size(), static_cast<std::size_t>(sol.counters.major_iters) + 1u);
    EXPECT_FALSE(sol.history.back().qp_solved);

    // Landed on x* = (0, sqrt 3), f* = -sqrt 3.
    EXPECT_NEAR(sol.f, p.f_star, 1e-9);
    EXPECT_NEAR(sol.x(0), 0.0, 1e-6);
    EXPECT_NEAR(sol.x(1), std::sqrt(3.0), 1e-6);

    // FULL-STEP NEWTON, NOT A TRUST-REGION WALK: no major's step reached the
    // radius. Without this the contraction below could be an artifact of the
    // box rather than a property of the Newton iteration.
    for (const SqpIterate &h : sol.history) {
        EXPECT_FALSE(h.tr_binding) << "trial " << h.trial;
    }

    // --- the contraction test, on the last three recorded residuals -------
    //
    // Observed sequence (kkt_residual = max(stationarity, feasibility)):
    //   1, 10.85, 3.112, 1.337, 0.10316, 8.5769e-4, 1.1459e-7, 1.1102e-15.
    // Last three: 8.5769e-4 -> 1.1459e-7 -> 1.1102e-15.
    //   ratios       1.336e-4, 9.688e-9      (each far below 1, and shrinking
    //                                         by five orders -> SUPERLINEAR)
    //   r_{k+1}/r_k^2  0.156,    0.0846      (bounded -> QUADRATIC)
    ASSERT_GE(sol.history.size(), 3u);
    const std::size_t m = sol.history.size();
    const double r0 = sol.history[m - 3].kkt_residual;
    const double r1 = sol.history[m - 2].kkt_residual;
    const double r2 = sol.history[m - 1].kkt_residual;
    ASSERT_GT(r0, 0.0);
    ASSERT_GT(r1, 0.0);

    // Contraction.
    EXPECT_LT(r1, r0);
    EXPECT_LT(r2, r1);

    // Superlinear: the contraction FACTOR itself goes to zero.
    const double ratio_1 = r1 / r0;
    const double ratio_2 = r2 / r1;
    EXPECT_LT(ratio_1, 1.0);
    EXPECT_LT(ratio_2, ratio_1);

    // Quadratic: r_{k+1} <= C r_k^2. C = 1 against an observed max of 0.156
    // is ~6x of margin. kResidualFloor admits the last step landing at the
    // numerical floor (~1e-15 here), where the ratio is measuring rounding
    // rather than the iteration.
    constexpr double kQuadraticConstant = 1.0;
    constexpr double kResidualFloor = 1e-14;
    EXPECT_LE(r1, kQuadraticConstant * r0 * r0 + kResidualFloor);
    EXPECT_LE(r2, kQuadraticConstant * r1 * r1 + kResidualFloor);

    const NlpKktResidual chk = self_check_kkt(*p.model, sol, opts.feas_tol);
    EXPECT_LT(chk.stationarity, 1e-7);
    EXPECT_LT(chk.primal, 1e-7);
    EXPECT_LT(chk.dual_sign, 1e-9);
}

// The published start points DO converge; they are just not where a clean
// quadratic-convergence claim should be pinned (see the note above). HS6
// converges at the DEFAULT radius; HS7 needs a larger one -- at tr_init = 1.0
// it ends kInfeasible, which GlobalizationEvidence below measures.
TEST(SqpDriverEquality, PublishedStartsAlsoConverge) {
    { // HS6 from (-1.2, 1) at the default radius. Observed (Task 6, fix
      // round 1): 8 majors, 0 rejections, the radius growing 1 -> 2 on the
      // first step and holding there. Task 4 measured 6 at a fixed radius.
        const HsProblem p = make_hs(6);
        SqpOptions opts;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);
        record_history(sol, "hs6_pub");

        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_LE(sol.counters.major_iters, 15);
        EXPECT_NEAR(sol.f, p.f_star, 1e-10);
        EXPECT_NEAR(sol.x(0), 1.0, 1e-7);
        EXPECT_NEAR(sol.x(1), 1.0, 1e-7);
        // TASK 7, RE-MEASURED PER THE BRIEF'S RULE: HS6 has ZERO rejected
        // trials here (radius only grows), so SOC's gate (which is only
        // reached from a kReject) is never even consulted. soc_steps == 0,
        // unchanged from Task 6b.
        EXPECT_EQ(sol.counters.soc_steps, 0);
    }
    { // HS7 from (2, 2). Observed (Task 6, fix round 1): 12 majors at
      // tr_init = 10 -- against Task 4's 8 at the same FIXED radius. The
      // radius runs 10, 5, 10, 5, 2.5, ... : one early trial is rejected, the
      // next is strong enough to win the radius back, and the sequence
      // settles at 2.5. Slower here, but this is the same rule that makes
      // tr_init = 1.0 converge at all (Task 4: kInfeasible).
        const HsProblem p = make_hs(7);
        SqpOptions opts;
        opts.tr_init = 10.0;
        opts.kkt_tol = 1e-8;
        opts.feas_tol = 1e-8;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);
        record_history(sol, "hs7_pub");

        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_LE(sol.counters.major_iters, 20);
        EXPECT_NEAR(sol.f, p.f_star, 1e-7);
        // TASK 7, RE-MEASURED PER THE BRIEF'S RULE: this fixture DOES hit
        // the SOC gate 3 times, but NOT identically -- FIX ROUND 1
        // corrected an earlier version of this comment that implied all 3
        // reached a corrected point; measured (all values from this exact
        // 12-major run), only ONE does:
        //   trial with h_old = 25, raw h_new = 141: the SOC re-solve
        //     reaches kOptimal, but its OWN corrected point is WORSE
        //     (h = 260.34, not smaller) -- a violation far too large for a
        //     linearized-Jacobian correction to fix -- so judge() rejects
        //     it and the ORIGINAL kReject stands.
        //   two trials with h_old = 48.1174, raw h_new = 7160.33 and
        //     353.856: the SOC re-solve ITSELF returns kInfeasible -- no
        //     corrected point is ever computed, no eval_nlp is paid for
        //     either (sqp_driver.h's A FAILED SOC RE-SOLVE note).
        // All 3 attempts therefore fail to rescue anything, and the
        // trajectory is BIT-IDENTICAL to Task 6b's: same 12 majors, same
        // rejected_steps. soc_steps == 3 is the honest count of attempts
        // (mostly cheap failures, one full-but-wasted re-solve), not a
        // claim that SOC helped here -- it does not have to, on every
        // fixture, to be worth its cost (see sqp_driver.h's cost-regime
        // paragraph: this is the LARGE-residual regime, not the design one).
        EXPECT_EQ(sol.counters.soc_steps, 3);
    }
}

// =========================================================================
// SqpDriverBounds -- (b) HS1/HS3/HS5 to their cited f*.
// =========================================================================

namespace {

// One bound-constrained fixture: solve at the given options, check the
// objective against the CITED f*, the point against the cited x*, and run the
// in-test KKT self-check on the returned quadruple.
void expect_solves_to(int number, const Vec &x_star, double x_tol, double f_tol,
                      const SqpOptions &opts, Index major_budget, const std::string &tag) {
    const HsProblem p = make_hs(number);
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    record_history(sol, tag);
    ::testing::Test::RecordProperty(fmt::format("{}_majors", tag),
                                    static_cast<int>(sol.counters.major_iters));

    ASSERT_EQ(sol.status, SqpStatus::kOptimal) << "HS" << number;
    EXPECT_LE(sol.counters.major_iters, major_budget) << "HS" << number;
    EXPECT_NEAR(sol.f, p.f_star, f_tol) << "HS" << number << " (source: " << p.source << ")";
    EXPECT_LT((sol.x - x_star).lpNorm<Eigen::Infinity>(), x_tol) << "HS" << number;

    // Counters are aggregates over the subproblems actually solved.
    EXPECT_EQ(sol.history.size(), static_cast<std::size_t>(sol.counters.major_iters) + 1u);
    Index minor_sum = 0, fact_sum = 0;
    for (const SqpIterate &h : sol.history) {
        minor_sum += h.qp_minor_iters;
        fact_sum += h.qp_factorizations;
    }
    EXPECT_EQ(sol.counters.qp_minor_iters, minor_sum);
    EXPECT_EQ(sol.counters.factorizations, fact_sum);
    EXPECT_EQ(sol.counters.soc_steps, 0);         // Task 7 owns these
    EXPECT_EQ(sol.counters.restoration_iters, 0); // Tasks 8-9 own these

    const NlpKktResidual chk = self_check_kkt(*p.model, sol, opts.feas_tol);
    EXPECT_LT(chk.stationarity, 1e-6) << "HS" << number;
    EXPECT_LT(chk.primal, 1e-6) << "HS" << number;
    EXPECT_LT(chk.dual_sign, 1e-9) << "HS" << number;
    EXPECT_LT(chk.complementarity, 1e-6) << "HS" << number;
}

} // namespace

// HS1 IS A ROSENBROCK VARIANT AND THE BRIEF FLAGS IT AS THE RISK CASE, so
// state what was actually measured. Full-step SQP from the published (-2, 1)
// DID reach x* = (1, 1) -- at every trust-region radius probed
// (tr_init = 0.25, 0.5, 0.75, 1, 1.5, 2, 3, 5 -> 23, 16, 16, 12, 13, 10, 9, 9
// majors), so the outcome was not balanced on a knife edge. What it did NOT
// do was descend: the Task-4 objective at the default radius ran
//   909, 52.3, 6.84, 6.12, 4.79, 6.41, 2.91, 26.3, 0.968, 86.9, 3.4e-4,
//   1.1e-5, 5.1e-18
// -- three increases, one of them by a factor of 90, before the final
// quadratic collapse.
//
// TASK 6 RE-MEASURED: 29 majors (5 of them rejected trials) and ZERO
// objective increases -- the funnel's f-type branch is the whole of HS1's
// acceptance test, because h == 0 here. The count went UP, which is what
// buying monotone descent on Rosenbrock costs: the lucky 90x excursion the
// unglobalized run took was also a shortcut. FunnelIsMonotoneInFOnHs1 asserts
// the descent half; this test keeps asserting the outcome. The budget stays
// deliberately loose (40 against an observed 29): the claim worth pinning is
// "reaches the solution", not an exact count.
TEST(SqpDriverBounds, Hs1RosenbrockReachesTheSolution) {
    Vec x_star(2);
    x_star << 1.0, 1.0;
    SqpOptions opts;
    opts.max_iter = 60;
    expect_solves_to(1, x_star, 1e-5, 1e-8, opts, /*major_budget=*/40, "hs1");
}

// HS3: f = x2 + 1e-5 (x2 - x1)^2 with x2 >= 0. x* = (0, 0), f* = 0, and the
// bound at x2 is ACTIVE there carrying z2 = 1 -- so this is the fixture that
// exercises the reduced stationarity measure's active-bound branch end to
// end. Task 4 observed 10 majors at the default radius, the count tracking
// 1/tr_init exactly (40 at 0.25, 20 at 0.5, 10 at 1, 5 at 2, 2 at 5), because
// the Newton step wants p1 ~ -10 and a FIXED radius truncated it to tr_init
// every major.
//
// TASK 6 RE-MEASURED: 4 majors. The radius is no longer fixed -- HS3's
// objective is exactly quadratic, so every step is "very successful" by the
// growth rule's ratio test and the radius runs 1, 2, 4, 8 until the step is
// interior. SqpDriverTrustRegion.RadiusGrowsOnStrongSteps pins that sequence
// itself; here it is only the reason the count moved.
TEST(SqpDriverBounds, Hs3ReachesItsBoundConstrainedSolution) {
    Vec x_star(2);
    x_star << 0.0, 0.0;
    SqpOptions opts;
    opts.max_iter = 60;
    expect_solves_to(3, x_star, 1e-4, 1e-8, opts, /*major_budget=*/40, "hs3");

    // SqpIterate::tr_binding must actually be OBSERVED TRUE somewhere in the
    // suite, or the field is untested and hard-coding it to false would pass
    // everything. HS3 is where it is unmissable: the Newton step wants
    // p1 ~ -10 against a radius of 1, so the radius binds on 9 of this
    // solve's 11 rows (measured) and releases only as the iterate closes on
    // x*. TrustRegionIsCenteredOnTheCurrentIterate checks the complementary
    // half -- that a binding radius is never EXCEEDED.
    const HsProblem p = make_hs(3);
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    const Index binding = std::count_if(sol.history.begin(), sol.history.end(),
                                        [](const SqpIterate &h) { return h.tr_binding; });
    EXPECT_GT(binding, 0) << "tr_binding was never set on a fixture whose radius provably binds";
    EXPECT_LT(binding, static_cast<Index>(sol.history.size()))
        << "the radius must also RELEASE as the iterate converges";
    ::testing::Test::RecordProperty("hs3_tr_binding_rows",
                                    fmt::format("{}/{}", binding, sol.history.size()));
}

// HS5: an INTERIOR optimum inside the box [-1.5,4] x [-3,3]. Observed:
// 4 majors at the default radius, f* matched to 12 digits.
//
// THE RADIUS SENSITIVITY THAT USED TO BE HERE IS GONE, and the prose that
// described it was false in all three of its claims once Task 6 landed, so it
// is replaced rather than annotated. Task 4 measured: kOptimal at tr_init =
// 0.25 .. 1.5; a permanent 2-CYCLE ending kMaxIter at tr_init = 2 and 3; and
// convergence to a DIFFERENT stationary point (f = 1.228) at tr_init = 5.
// RE-MEASURED with globalization and radius management (fix round 1):
//     tr_init =  1 -> kOptimal, 4 majors, 0 rejected, radius held at 1
//     tr_init =  2 -> kOptimal, 6 majors, 1 rejected, radius down to 1
//     tr_init =  3 -> kOptimal, 6 majors, 1 rejected, radius down to 1.5
//     tr_init =  5 -> kOptimal, 7 majors, 2 rejected, radius down to 1.25
//     tr_init = 10 -> kOptimal, 8 majors, 3 rejected, radius down to 1.25
// every one of them at the TRUE f* = -1.913222954981. The wrong-stationary-
// point outcome at tr_init = 5 is gone too: the funnel rejects the step that
// used to walk there. So the fixture is no longer radius-sensitive in any of
// the ways that motivated the note -- the sensitivity was the FIXED radius,
// and the surviving pins for that are Hs5ConvergesFromATooLargeRadiusAfter-
// Shrinking (tr_init = 2) and TrustRegionIsCenteredOnTheCurrentIterate
// (tr_init = 10).
//
// SUCCESSOR NOTE (Task 5), DISCHARGED: the budget below is still calibrated
// against a measurement -- 4 majors, 0 rejections, radius held at 1.0. Note
// the radius is held by the RATIO conjunct here, not the tr_binding one: the
// first step does reach the radius but is not strong enough to earn a bigger
// one (RadiusHoldsWhenEitherGrowthConjunctFails pins the two conjuncts on
// fixtures where each is decisive on its own).
TEST(SqpDriverBounds, Hs5ReachesItsInteriorSolution) {
    Vec x_star(2);
    x_star << -0.5471975511965977, -1.547197551196598;
    SqpOptions opts;
    expect_solves_to(5, x_star, 1e-6, 1e-9, opts, /*major_budget=*/12, "hs5");
}

// Not in the brief's (b), but the only shipped problem with general
// inequalities: it pins that lambda_i, the ineq_active warm-start channel and
// the complementarity self-check all work. Observed: 2 majors, f* exact.
TEST(SqpDriverBounds, Hs76ReachesItsInequalityConstrainedSolution) {
    Vec x_star(4);
    x_star << 0.2727272727272727, 2.090909090909091, 0.0, 0.5454545454545454;
    SqpOptions opts;
    expect_solves_to(76, x_star, 1e-6, 1e-12, opts, /*major_budget=*/10, "hs76");
}

// =========================================================================
// SqpDriverWarmStart -- (c) warm QP seeding.
// =========================================================================

// THE BRIEF'S TEST (c): late majors cost <= 2 minor iterations. Asserted on
// the recorded history, and then corroborated by re-solving the SAME
// subproblems COLD -- because "2 minor iterations" is only evidence of warm
// seeding if the cold solve costs more, which is exactly what
// replay_cold_vs_warm measures.
//
// Observed on HS3 (10 majors at the default radius): every major after the
// first costs 2 warm against 3 cold. On HS76: 6 cold / 6 warm on major 0
// (the first solve IS cold), 6 cold / 3 warm on major 1.
//
// TASK 6: THE RADIUS IS DELIBERATELY PINNED (tr_max == tr_init) so that this
// fixture stays COUNTER-IDENTICAL to Task 4's measurement -- the numbers
// above are still the numbers, and what is being tested is warm SEEDING, not
// radius management. Left free, HS3's radius doubles every major (its
// objective is exactly quadratic, so every step is "very successful") and the
// solve finishes in 4 majors with barely a late major to speak of; that is
// SqpDriverTrustRegion.RadiusGrowsOnStrongSteps's subject, and it is asserted
// there. The one Task-6 property this test must still confirm is that the
// replay is comparable at all: replay_cold_vs_warm takes every step it
// computes, so it reproduces the driver's trajectory only if the driver
// rejected nothing.
TEST(SqpDriverWarmStart, WarmSeedingKeepsLateSubproblemsCheap) {
    const HsProblem p = make_hs(3);
    SqpOptions opts;
    opts.max_iter = 60;
    opts.tr_max = opts.tr_init; // no growth: Task 4's fixed-radius trajectory
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    record_history(sol, "hs3_warm");

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_EQ(sol.counters.rejected_steps, 0) << "the replay below cannot follow a rejected step";
    ASSERT_GE(sol.counters.major_iters, 4) << "fixture must have 'late' majors to speak of";

    // Every major AFTER the first (i.e. every warm-seeded one) is cheap.
    for (const SqpIterate &h : sol.history) {
        if (!h.qp_solved || h.trial == 0) {
            continue;
        }
        EXPECT_LE(h.qp_minor_iters, 2) << "trial " << h.trial;
    }

    // ... and cheaper than the identical subproblem solved cold.
    const ColdWarmMinors cw =
        replay_cold_vs_warm(*p.model, opts, sol.counters.major_iters, radii_of(sol));
    ASSERT_EQ(cw.cold.size(), cw.warm.size());
    ASSERT_GE(cw.warm.size(), 2u);
    // The replay must reproduce the driver's own warm costs -- if it does
    // not, the driver is seeding differently from what this test claims.
    for (std::size_t k = 0; k < cw.warm.size(); ++k) {
        EXPECT_EQ(cw.warm[k], sol.history[k].qp_minor_iters) << "major " << k;
    }
    // SqpIterate::step_norm must be the displacement ACTUALLY APPLIED to the
    // iterate, not merely some norm the driver happened to record: the replay
    // measures ||x_{k+1} - x_k||inf from its own iterates and they must agree
    // row for row. Task 5's acceptance test reads this field to decide
    // whether a step was worth taking, so a drift between "recorded" and
    // "applied" would be silent and load-bearing.
    ASSERT_EQ(cw.warm_step_norm.size(), cw.warm.size());
    for (std::size_t k = 0; k < cw.warm_step_norm.size(); ++k) {
        EXPECT_NEAR(sol.history[k].step_norm, cw.warm_step_norm[k],
                    1e-12 * (1.0 + cw.warm_step_norm[k]))
            << "major " << k;
    }

    Index strictly_cheaper = 0;
    for (std::size_t k = 1; k < cw.warm.size(); ++k) {
        EXPECT_LE(cw.warm[k], cw.cold[k]) << "major " << k;
        if (cw.warm[k] < cw.cold[k]) {
            ++strictly_cheaper;
        }
    }
    EXPECT_GT(strictly_cheaper, 0)
        << "warm seeding never beat a cold solve -- the seed is not being used";
    ::testing::Test::RecordProperty("hs3_strictly_cheaper_majors",
                                    static_cast<int>(strictly_cheaper));
}

// The same comparison where the active set is genuinely non-trivial: HS76
// has three general inequality rows and four bounds, so the warm seed
// carries an ineq_active labeling and not just bound states.
TEST(SqpDriverWarmStart, WarmSeedingCarriesTheGeneralRowActiveSet) {
    const HsProblem p = make_hs(76);
    SqpOptions opts;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_GE(sol.counters.major_iters, 2);
    ASSERT_EQ(sol.counters.rejected_steps, 0) << "the replay below cannot follow a rejected step";

    const ColdWarmMinors cw =
        replay_cold_vs_warm(*p.model, opts, sol.counters.major_iters, radii_of(sol));
    ASSERT_GE(cw.warm.size(), 2u);
    EXPECT_EQ(cw.cold[0], cw.warm[0]) << "the first solve is cold either way";
    EXPECT_LT(cw.warm[1], cw.cold[1]) << "observed: 3 warm against 6 cold";
    ::testing::Test::RecordProperty("hs76_major1",
                                    fmt::format("cold={} warm={}", cw.cold[1], cw.warm[1]));
}

// =========================================================================
// SqpDriverGlobalization -- what the funnel guarantees, on the exact
// fixtures where Task 4 measured that full-step SQP guaranteed nothing.
// =========================================================================

// THE SUCCESSOR TO FullStepIsNotMonotoneOnHs1, executed per that test's own
// instruction ("DELETE the two 'increases > 0' assertions and replace them
// with Task 5's actual monotonicity contract ... Keep the recorded trajectory
// properties either way").
//
// TASK 4 MEASURED, at the default radius: the objective ran 909, 52.3, 6.84,
// 6.12, 4.79, 6.41, 2.91, 26.3, 0.968, 86.9, 3.4e-4, ... -- three increases,
// one of them by a factor of 90. That is now 0 increases; the trajectory
// properties below record the replacement.
//
// WHAT THE FUNNEL ACTUALLY GUARANTEES, and it is NOT "f is monotone" in
// general -- getting this wrong would be asserting something the algorithm
// does not promise. KLV Algorithm 2 has two acceptance branches:
//   * f-TYPE (Eq. (10) holds, Eq. (11) tested): f_new <= f_old - sigma*pred_df
//     with pred_df >= delta*h^2 >= 0, so f is non-increasing on such a step;
//   * h-TYPE (Eq. (10) fails, Eq. (12) tested): the trial is judged on h
//     ALONE and f MAY RISE. All that is promised there is h_new <= beta*tau,
//     i.e. the trial stays strictly inside a funnel width that then TIGHTENS
//     (Eq. (13)) and never re-widens.
// So the honest contract has two halves, and they are asserted separately:
// f-decrease on f-type steps, and h bounded by the funnel width throughout.
//
// HS1 IS THE f-TYPE HALF IN PURE FORM. Its only constraint is a bound, and
// bounds are excluded from h by construction (globalization.h's h note), so
// h == 0 on every iterate, Eq. (10) reads pred_df >= 0, and EVERY accepted
// step is f-type. Hence f monotone, unconditionally.
TEST(SqpDriverGlobalization, FunnelIsMonotoneInFOnHs1) {
    const HsProblem p = make_hs(1);
    SqpOptions opts;
    opts.max_iter = 60;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    record_history(sol, "hs1_monotone");

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_GT(sol.counters.rejected_steps, 0)
        << "the fixture must actually exercise rejection, or the monotonicity below is free";

    Index f_increases = 0;
    Index kkt_increases = 0;
    Index h_type = 0;
    double worst_f_jump = 1.0;
    for (std::size_t k = 0; k < sol.history.size(); ++k) {
        const SqpIterate &h = sol.history[k];
        EXPECT_DOUBLE_EQ(h.violation_l1, 0.0) << "HS1 has no general constraints";
        if (h.qp_solved && h.qp_status == QpStatus::kOptimal &&
            h.verdict == StepVerdict::kAcceptH) {
            ++h_type;
        }
        if (k == 0) {
            continue;
        }
        if (sol.history[k].f > sol.history[k - 1].f) {
            ++f_increases;
            worst_f_jump = std::max(worst_f_jump, sol.history[k].f / sol.history[k - 1].f);
        }
        if (sol.history[k].kkt_residual > sol.history[k - 1].kkt_residual) {
            ++kkt_increases;
        }
    }

    // THE REPLACEMENT CONTRACT. Every row -- rejected trials included, since
    // a rejected trial leaves the iterate and therefore f exactly where they
    // were -- is <= its predecessor.
    EXPECT_EQ(f_increases, 0) << "the funnel accepts an f-type step only under KLV Eq. (11), "
                                 "which cannot admit an objective increase at h = 0";
    EXPECT_EQ(h_type, 0) << "at h == 0 the switching condition cannot fail, so no step here is "
                            "h-type -- if this fires, the pred_df SIGN is wrong (see carry 1)";
    // The KKT RESIDUAL is deliberately NOT asserted monotone: nothing in KLV
    // claims it, and it is not what the acceptance test reads. Recorded.
    ::testing::Test::RecordProperty("hs1_f_increases", static_cast<int>(f_increases));
    ::testing::Test::RecordProperty("hs1_kkt_increases", static_cast<int>(kkt_increases));
    ::testing::Test::RecordProperty("hs1_worst_f_jump", fmt::format("{:.4g}", worst_f_jump));
    ::testing::Test::RecordProperty("hs1_rejected", static_cast<int>(sol.counters.rejected_steps));
    ::testing::Test::RecordProperty("hs1_majors", static_cast<int>(sol.counters.major_iters));
}

// THE OTHER HALF OF THE CONTRACT, on the fixtures that HAVE an h to speak of:
// the constraint violation never leaves the funnel, and f falls on every
// f-type step. Asserted against the width the funnel was SEEDED with,
// tau_0 = max(tau_bar, kappa_bar * h_0) (KLV Eq. (9)), which bounds every
// later width because the update Eq. (13) is monotonically non-increasing --
// so `h <= tau_0` on every accepted iterate is implied by Eq. (8) alone and
// needs nothing exposed from inside the strategy.
TEST(SqpDriverGlobalization, FunnelKeepsIteratesInsideItsWidth) {
    for (const int number : {6, 7, 76}) {
        const HsProblem p = make_hs(number);
        SqpOptions opts;
        opts.max_iter = 60;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);
        record_history(sol, fmt::format("hs{}_funnel", number));
        ASSERT_EQ(sol.status, SqpStatus::kOptimal) << "HS" << number;

        ASSERT_FALSE(sol.history.empty());
        const double tau0 = std::max(kFunnelTauBar, kFunnelKappaBar * sol.history[0].violation_l1);
        Index f_type = 0, h_type = 0, f_rises_on_h_type = 0;
        for (std::size_t k = 0; k < sol.history.size(); ++k) {
            const SqpIterate &h = sol.history[k];
            EXPECT_LE(h.violation_l1, tau0) << "HS" << number << " trial " << h.trial;
            if (!h.qp_solved || h.qp_status != QpStatus::kOptimal || k + 1 >= sol.history.size()) {
                continue;
            }
            const double f_next = sol.history[k + 1].f;
            if (h.verdict == StepVerdict::kAcceptF) {
                ++f_type;
                // KLV Eq. (11) with sigma > 0 and pred_df >= delta*h^2 >= 0.
                EXPECT_LE(f_next, h.f + 1e-12 * (1.0 + std::abs(h.f)))
                    << "HS" << number << " trial " << h.trial << ": an f-type step must not "
                    << "increase the objective";
            } else if (h.verdict == StepVerdict::kAcceptH) {
                ++h_type;
                if (f_next > h.f) {
                    ++f_rises_on_h_type;
                }
            }
        }
        ::testing::Test::RecordProperty(fmt::format("hs{}_step_types", number),
                                        fmt::format("f={} h={} f_rose_on_h_type={} tau0={:g}",
                                                    f_type, h_type, f_rises_on_h_type, tau0));
    }
}

// With no radius management, a radius that is merely somewhat too large turns
// a 4-major solve into a permanent 2-CYCLE. HS5 at tr_init = 2.0 (against a
// default of 1.0) alternates between two corners of its box forever: the
// recorded residuals repeat with period 2 to the last bit. This is the
// trust-region-update argument for Task 5 / Task 9, and it is why
// Hs5ReachesItsInteriorSolution above states its radius window explicitly.
//
// SUCCESSOR INSTRUCTION (Task 5, confirmed by Task 9). A radius update rule
// is exactly what breaks this cycle: the first rejected step shrinks the
// radius into HS5's good window and the solve converges. So this test is
// EXPECTED to fail with kOptimal once Task 5 lands. When it does: rewrite it
// as the positive statement -- HS5 from tr_init = 2.0 now CONVERGES to
// f* = -1.913222954981, in some bounded number of majors, having shrunk the
// radius at least once (assert on whatever radius-history field Task 5/12
// exposes). Do not simply raise tr_init to keep the cycle alive; the cycle
// is not worth preserving once the mechanism that prevents it exists.
// THE SUCCESSOR, executed exactly as instructed: the positive statement,
// with the radius shrink asserted on SqpIterate::tr_radius (the field the
// instruction anticipated). tr_init is deliberately left at the cycling value
// of 2.0 rather than raised.
//
// TASK 4 MEASURED: kMaxIter after 20 majors, the residual sequence repeating
// with period 2 to the last bit, f nowhere near f*. The mechanism that breaks
// it is one rejection: the first trial from the box corner fails KLV Eq. (11),
// the radius halves to 1.0 -- HS5's good window, where Task 4 already
// converged in 4 majors -- and the solve proceeds.
TEST(SqpDriverGlobalization, Hs5ConvergesFromATooLargeRadiusAfterShrinking) {
    const HsProblem p = make_hs(5);
    SqpOptions opts;
    opts.tr_init = 2.0;
    opts.max_iter = 20;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    record_history(sol, "hs5_cycle");

    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_LT(sol.counters.major_iters, opts.max_iter) << "it stopped early, not on the budget";
    EXPECT_LE(sol.counters.major_iters, 10); // observed: 6
    EXPECT_NEAR(sol.f, p.f_star, 1e-9);
    EXPECT_NEAR(sol.x(0), -0.5471975511965977, 1e-6);
    EXPECT_NEAR(sol.x(1), -1.547197551196598, 1e-6);

    // THE MECHANISM, not just the outcome: at least one trial was rejected
    // and the radius actually came down below where it started.
    EXPECT_GE(sol.counters.rejected_steps, 1);
    double min_radius = std::numeric_limits<double>::infinity();
    for (const SqpIterate &h : sol.history) {
        min_radius = std::min(min_radius, h.tr_radius);
    }
    EXPECT_LT(min_radius, opts.tr_init) << "the radius never shrank, so this is not the fix";
    ::testing::Test::RecordProperty("hs5_min_radius", fmt::format("{:g}", min_radius));
    ::testing::Test::RecordProperty("hs5_rejected", static_cast<int>(sol.counters.rejected_steps));

    // The period-2 cycle is GONE: no two rows two apart repeat to the bit.
    Index repeats = 0;
    for (std::size_t k = 0; k + 2 < sol.history.size(); ++k) {
        if (sol.history[k].kkt_residual == sol.history[k + 2].kkt_residual) {
            ++repeats;
        }
    }
    EXPECT_EQ(repeats, 0);
}

// SUBPROBLEM FAILURE ROUTING, and the INTERIM caveat on it. A locally infeasible
// linearization ends the solve today; Tasks 6b/8/9 are what will instead
// shrink the radius, go elastic, or restore. HS7 from its published (2, 2) at
// the DEFAULT radius is a live instance: the radius is too small for the
// linearized equality to be satisfiable from major 2's iterate, the engine
// reports kInfeasible, and the driver propagates it verbatim. The same
// problem at tr_init = 10 converges (PublishedStartsAlsoConverge), which is
// precisely the point -- the failure is the fixed radius, not the problem.
//
// SUCCESSOR INSTRUCTION (Tasks 6b and 8). Neither of the two mechanisms that
// will handle this exists yet: Task 6b's shrink-retry (re-solve the SAME
// linearization at a smaller radius before giving up) and Task 8's elastic
// mode (relax the linearized constraints with a penalty when no radius makes
// them consistent). Once EITHER lands this test must change, and the change
// differs by which:
//   - Task 6b alone: the expected status becomes kOptimal for this fixture
//     (a smaller radius is exactly what makes HS7's linearization
//     satisfiable here -- tr_init = 1.0 fails while the SAME problem at
//     tr_init = 10 already converges today), and the assertion to keep is
//     that the retry was RECORDED, not that the driver silently succeeded.
//   - Task 8 alone: the status becomes kOptimal via an elastic major;
//     counters.restoration_iters -- asserted == 0 everywhere today -- becomes
//     the thing to assert nonzero.
// Either way, keep a fixture that still reaches the propagation branch (a
// genuinely inconsistent NLP, not merely a badly sized radius), because the
// branch itself must stay reachable and tested.
//
// ============================ TASK 6 UPDATE =============================
//
// EXECUTED, and by an unanticipated route: neither Task 6b nor Task 8, but
// Task 6's RADIUS GROWTH. HS7 from (2, 2) at tr_init = 1.0 now converges
// (kOptimal in 10 majors, observed) because the radius no longer stays at 1.0
// -- the early steps are strong, it doubles past the value at which major 2's
// linearization was unsatisfiable, and the run turns into the tr_init = 10
// run PublishedStartsAlsoConverge already recorded. The failure really was
// "the FIXED radius, not the problem", exactly as this test said; it just
// took the growth half of the radius rule rather than the shrink half.
//
// This is a DEVIATION FROM THE TASK-6 BRIEF'S EXPECTATION, which carried HS7
// -at-the-default-radius forward as a known-kInfeasible fixture for Task 8.
// It is recorded rather than reverted: nothing here special-cases HS7, and
// suppressing legitimate radius growth to preserve a failing fixture would be
// exactly the "do not simply raise tr_init to keep the cycle alive" mistake
// the sibling test warns about. Task 8 needs a fixture reaching restoration,
// and SqpDriverTrustRegion.RejectionCountReachesTheRestorationGate is a
// better one -- it is engineered to be radius-proof.
//
// THE PROPAGATION BRANCH ITSELF stays tested, per the instruction's closing
// requirement, on a GENUINELY inconsistent NLP: no radius makes
// InconsistentBoundedModel's linearization satisfiable, because the
// constraint asks for x0 = 5 inside a box that ends at 1.
TEST(SqpDriverGlobalization, Hs7AtTheDefaultRadiusNowConvergesByGrowingIt) {
    const HsProblem p = make_hs(7);
    SqpOptions opts; // tr_init = 1.0, the radius Task 4 could not survive here
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    record_history(sol, "hs7_default_radius");

    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_LE(sol.counters.major_iters, 20); // observed: 10
    EXPECT_NEAR(sol.f, p.f_star, 1e-7);

    double max_radius = 0.0;
    for (const SqpIterate &h : sol.history) {
        max_radius = std::max(max_radius, h.tr_radius);
    }
    EXPECT_GT(max_radius, opts.tr_init) << "the radius growth is the mechanism; without it this "
                                           "fixture is the kInfeasible propagation case";
    ::testing::Test::RecordProperty("hs7_max_radius", fmt::format("{:g}", max_radius));
}

namespace {

// A GENUINELY INCONSISTENT NLP, and inconsistent at every radius:
//
//     min 1/2 (x0^2 + x1^2)   s.t.  cE(x) = x0 - 5 = 0,   0 <= x <= 1.
//
// The linearized equality asks for p0 = 5 - x0 >= 4 while the subproblem's own
// box is 0 - x0 .. 1 - x0, i.e. |p0| <= 1 -- so no trust region, of any size,
// makes the QP feasible, and the engine's kInfeasible verdict is a fact about
// the model rather than about the radius. That is exactly the distinction the
// test it serves has to keep alive: shrinking, growing, restoring or going
// elastic will all still end here.
//
// TASK 8 CONFIRMED THE PREDICTION and made it precise. Going elastic DOES
// still end here -- but no longer at the first subproblem, and no longer with
// a raw QP status: the tier relaxes the row, takes one h-reducing step to
// x = (1, 0), finds that from THERE nothing helps at all, and exits through
// the funnel's kRestore route. SqpDriverElastic.RhoEscalationIsBoundedAnd-
// Signals has the whole hand-derived trajectory; the test below keeps its own
// job, which is the two HISTORY SHAPES.
class InconsistentBoundedModel : public NlpModel {
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

} // namespace

// TASK 8 RE-MEASURED AND RENAMED (was SubproblemInfeasibilityPropagates). A
// kInfeasible subproblem no longer propagates -- the elastic tier owns it --
// so the old name described behaviour that is gone. What SURVIVES, and is
// what this test was really guarding, is (i) that this model still ends the
// solve rather than being rescued by the new tier, and (ii) the two history
// SHAPES, which Task 6b consumes on exactly this path.
//
// The two row-level assertions that flipped are stated with their old values
// in the comments below, since the flip IS the task's evidence:
//   qp_status was kInfeasible (the failing solve's own status), and is now
//     kOptimal -- the row reports the FINAL ELASTIC solve, which succeeded;
//     the kInfeasible that triggered the tier is recorded by elastic_applied.
//   verdict was kReject (the no-step default), and is now kRestore -- the
//     elastic tier's exhaustion signal, deliberately routed through the SAME
//     interim exit the funnel's own kRestore uses (sqp_driver.h).
// The sentence the old assertion carried -- "must NOT be confused with the
// funnel's kRestore route, where qp_status is kOptimal" -- is therefore no
// longer a distinction the history can draw with these two fields alone, and
// elastic_applied is what draws it now.
TEST(SqpDriverGlobalization, SubproblemInfeasibilityEndsTheSolveThroughTheElasticTier) {
    InconsistentBoundedModel model;
    SqpOptions opts; // tr_init = 1.0
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);
    record_history(sol, "inconsistent");

    EXPECT_EQ(sol.status, SqpStatus::kInfeasible);
    ASSERT_FALSE(sol.history.empty());
    const SqpIterate &last = sol.history.back();
    EXPECT_TRUE(last.qp_solved)
        << "the terminating row must record the subproblem the solve stopped on -- which is now "
           "the ELASTIC re-solve, not a failing one";
    EXPECT_TRUE(last.elastic_applied) << "this is where the kInfeasible is recorded now";
    EXPECT_EQ(last.qp_status, QpStatus::kOptimal) << "the FINAL ELASTIC solve's status";
    EXPECT_EQ(last.verdict, StepVerdict::kRestore)
        << "the elastic tier was exhausted; that is the authoritative Algorithm-5 trigger and "
           "it exits through the same interim route as the funnel's own kRestore";
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_LT(sol.counters.major_iters, opts.max_iter) << "it stopped early, not on the budget";
    // x is the final iterate reached, not a cleared vector.
    EXPECT_EQ(sol.x.size(), model.n());
    EXPECT_TRUE(std::isfinite(sol.f));

    // THE HISTORY HAS A DIFFERENT SHAPE ON THIS PATH, and Task 6b consumes
    // the history on exactly this path, so the shape is pinned here rather
    // than left to be rediscovered. A solve that stops ON a subproblem
    // records NO qp_solved == false row, so history.size() == major_iters
    // (not major_iters + 1) and history[major_iters] is OUT OF BOUNDS.
    EXPECT_EQ(sol.history.size(), static_cast<std::size_t>(sol.counters.major_iters));
    EXPECT_EQ(std::count_if(sol.history.begin(), sol.history.end(),
                            [](const SqpIterate &h) { return !h.qp_solved; }),
              0)
        << "a solve that stopped on a subproblem must have no stopped-at-iterate row";
    for (const SqpIterate &h : sol.history) {
        EXPECT_TRUE(h.qp_solved) << "trial " << h.trial;
    }

    // The complementary shape, for contrast: a solve that stops AT an iterate
    // has exactly one such row and it is the last.
    const HsProblem q = make_hs(5);
    SqpDriver ok_driver{SqpOptions{}};
    const SqpSolution ok = ok_driver.solve(*q.model);
    ASSERT_EQ(ok.status, SqpStatus::kOptimal);
    EXPECT_EQ(ok.history.size(), static_cast<std::size_t>(ok.counters.major_iters) + 1u);
    EXPECT_EQ(std::count_if(ok.history.begin(), ok.history.end(),
                            [](const SqpIterate &h) { return !h.qp_solved; }),
              1);
    EXPECT_FALSE(ok.history.back().qp_solved);
}

// =========================================================================
// SqpDriverTrustRegion -- Task 6: the trust-region major loop.
//
// Task 4's loop took every step the subproblem returned at a FIXED radius.
// Task 6 wires Task 5's funnel in between: the trial point is evaluated, the
// strategy judges it, and the radius moves -- x2 up to tr_max on a strong
// accepted step whose radius was binding, /2 on a rejection, with the
// rejected trial RE-SOLVED at the same iterate from the same H/g (hot-started
// through SolveOverrides, which is what Task 2 built that API for).
//
// Every constant in these tests is hand-derived from the fixture below and
// stated in the comment that uses it; the iteration counts are observations.
// =========================================================================

namespace {

// THE REJECTION FIXTURE, hand-derivable end to end:
//
//     f(x) = x0^4/4 - x0 + 1/2 x1^2,   no constraints, no finite bounds,
//     x_start = (0.5, 0).
//
// x1 starts AT its own minimizer, so its step is 0 on every trial and the
// arithmetic below is exactly the 1-D arithmetic in x0. (The second variable
// is not decoration: it makes H a genuine 2x2 and makes the QP's box, the
// engine's bound_state and its tr_active vectors non-degenerate.)
//
// WHY THE FIRST FULL STEP IS REJECTED. At x0 = 0.5: g0 = x0^3 - 1 = -0.875,
// W00 = 3 x0^2 = 0.75, so the QP's unconstrained step is p0 = 0.875/0.75 =
// 7/6, INTERIOR to tr_init = 2 (this matters twice: the model is not
// truncated, and the solve exits with no pin at all, which is what lets the
// retry reuse the factorization). The model promises
//     pred_df = -(g^T p + 1/2 p^T W p) = 0.875*(7/6) - 0.375*(7/6)^2
//             = 1.0208333 - 0.5104167 = 0.5104167 > 0,
// but the quartic's true value at x0 + p = 5/3 is 1.929012 - 1.666667 =
// +0.262346 against f(0.5) = -0.484375: the objective RISES by 0.746721. h is
// identically 0 here, so KLV Eq. (10) makes every trial f-type
// (pred_df >= 0.999*0^2), and Eq. (11) then wants
// f_old - f_new >= 1e-4 * pred_df = 5.1e-5. It is -0.746721. REJECT.
//
// The shrink sequence, all four numbers hand-computed:
//   Delta = 2   -> p0 = 7/6 (interior), f_new = +0.262346, REJECT
//   Delta = 1   -> p0 = 1 (TR-pinned), pred_df = 0.5,     f_new = -0.234375,
//                  actual = -0.25,                         REJECT
//   Delta = 0.5 -> p0 = 0.5 (TR-pinned), pred_df = 0.34375, f_new = -0.75,
//                  actual = +0.265625 >= 1e-4*0.34375,      ACCEPT (f-type)
// and x0 + 0.5 = 1 is the EXACT minimizer of x0^4/4 - x0 (g = 1 - 1 = 0), so
// the next major converges with no step at all. The accepted trial's ratio
// actual/pred = 0.265625/0.34375 = 0.7727 clears kTrGrowThreshold = 0.75 and
// its radius was binding, so the radius doubles on the way out -- visible in
// the history even though the solve ends before spending it.
class FlatQuarticModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return 0.25 * std::pow(x(0), 4) - x(0) + 0.5 * x(1) * x(1);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << x(0) * x(0) * x(0) - 1.0, x(1);
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 3.0 * x(0) * x(0);
        h.insert(1, 1) = obj_scale * 1.0;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
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
        x << 0.5, 0.0;
        return x;
    }
};

// THE RESTORATION-SIGNATURE FIXTURE. Engineered so that FunnelStrategy's five
// conjuncts all hold on the FOURTH consecutive trial at the start point --
// i.e. so that the driver's `rejections_at_iterate` wiring is what decides the
// verdict, and a driver that forgot to supply it would silently get kReject
// forever (the documented fail-safe).
//
//     f(x) = -10 x1 - 0.01 x0 + 0.0005 (x0^2 + x1^2)
//     cE(x) = x1 + 1000 x0^2 = 0,     x_start = (0, 0.5)
//
// h_old = |cE| = 0.5, so tau_0 = max(100, 1.25*0.5) = 100 (Eq. (9)).
// Je = (2000 x0, 1) = (0, 1) at the start, so the LINEARIZED equality forces
// p1 = -0.5 exactly, on every trial, at every radius >= 0.5 -- and the true
// violation of the trial point is then
//     cE(x + p) = (0.5 + p1) + 1000 (0 + p0)^2 = 1000 p0^2,
// i.e. pure second-order error, invisible to the QP. The objective's -0.01 x0
// term with the tiny 0.001 curvature drives p0 to the radius (its
// unconstrained minimizer is p0 = 10), so h_new = 1000*Delta^2:
//     Delta = 4   -> h_new = 16000 > 100   REJECT (rejections_at_iterate = 0)
//     Delta = 2   -> h_new =  4000 > 100   REJECT (1)
//     Delta = 1   -> h_new =  1000 > 100   REJECT (2)
//     Delta = 0.5 -> h_new =   250 > 100   conjunct (e) now holds: RESTORE (3)
// and every trial has pred_df < 0 (conjunct (c)): the model pays
// -(-10)*(-0.5) = -5 for the constraint-forced move in x1 and recovers only
// 0.01*Delta from x0. h_new >= h_old is conjunct (d), trivially. Note the
// radius CANNOT be shrunk once more without the linearization going
// infeasible (p1 = -0.5 needs Delta >= 0.5) -- which is exactly KLV Lemma 5
// case 1, the paper's own account of what this signature is detecting.
class SecondOrderInfeasibleModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return -10.0 * x(1) - 0.01 * x(0) + 0.0005 * (x(0) * x(0) + x(1) * x(1));
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 0.001 * x(0) - 0.01, 0.001 * x(1) - 10.0;
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(1) + 1000.0 * x(0) * x(0);
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &le, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 0.001 + le(0) * 2000.0;
        h.insert(1, 1) = obj_scale * 0.001;
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

// Wraps the real FunnelStrategy and records every StepContext the driver
// hands it, plus the verdict it returned. This is how the tests below observe
// the fields that have no other externally visible trace -- pred_df and
// rejections_at_iterate -- without the driver having to publish them.
struct StepLog {
    double h0 = -1.0;
    Index resets = 0;
    Index resumes = 0; // resume_from_restoration calls (Task 9)
    std::vector<StepContext> ctx;
    std::vector<StepVerdict> verdict;
};

class RecordingStrategy final : public GlobalizationStrategy {
  public:
    explicit RecordingStrategy(StepLog *log) : log_(log) {}

    void reset(double h0) override {
        inner_.reset(h0);
        log_->h0 = h0;
        ++log_->resets;
    }
    // TASK 9: A DECORATOR MUST FORWARD THIS EXPLICITLY. The base class's
    // default is written in terms of reset(), so a wrapper that forwards only
    // reset() would have its INNER funnel re-initialized (Eq. (9), width back
    // to tau_bar) where the driver asked for a re-basing (Eq. (13)) -- and
    // would report a second reset() that the driver never made. That is what
    // this recorder did before the override existed, observed as h0 changing
    // to the restored point's h.
    void resume_from_restoration(double h) override {
        inner_.resume_from_restoration(h);
        ++log_->resumes;
    }
    StepVerdict judge(const StepContext &ctx) override {
        const StepVerdict v = inner_.judge(ctx);
        log_->ctx.push_back(ctx);
        log_->verdict.push_back(v);
        return v;
    }

  private:
    FunnelStrategy inner_;
    StepLog *log_;
};

// Installs a RecordingStrategy factory on `opts`, logging into `log`.
void record_steps_into(SqpOptions &opts, StepLog *log) {
    opts.make_strategy = [log]() -> std::unique_ptr<GlobalizationStrategy> {
        return std::make_unique<RecordingStrategy>(log);
    };
}

} // namespace

// CARRY 1 (ledger, Task-5 review): pred_df MUST be the QP MODEL decrease
//     delta_m_f(p) = -(g^T p + 1/2 p^T W p)
// computed from the SUBPROBLEM'S OWN g/H at the CURRENT iterate -- never an
// actual objective difference f(x) - f(x+p), and never the negation of the
// above. Both mistakes are silent: the negation makes pred_df <= 0 on every
// healthy step, so KLV Eq. (10) (pred_df >= delta*h^2) fails everywhere and
// EVERY step becomes h-type, judged only against the funnel width and never
// against the Armijo condition the convergence proof's case 2 rests on.
//
// Pinned twice: once as pure arithmetic on a hand-computed 2-variable QP, and
// once end-to-end on the FlatQuarticModel trajectory (where the model decrease
// and the actual objective difference have OPPOSITE SIGNS, so no
// implementation that confuses the two can pass both).
TEST(SqpDriverTrustRegion, PredictedDecreaseIsTheQpModelDecrease) {
    // H = [[2, 1], [1, 4]] (stored upper-triangular), g = (-1, 3), p = (2, -1).
    //   g^T p     = -2 - 3 = -5
    //   H p       = (2*2 + 1*(-1), 1*2 + 4*(-1)) = (3, -2)
    //   p^T H p   = 2*3 + (-1)*(-2) = 8
    //   pred_df   = -(-5 + 0.5*8) = -(-1) = +1
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(0, 1) = 1.0; // upper triangle only; the (1,0) mirror is implied
    qp.H.insert(1, 1) = 4.0;
    qp.H.makeCompressed();
    qp.g = Vec(2);
    qp.g << -1.0, 3.0;
    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.be = Vec(0);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -kInfBound);
    qp.upper = Vec::Constant(2, kInfBound);
    ASSERT_NO_THROW(qp.validate());

    Vec p(2);
    p << 2.0, -1.0;
    EXPECT_DOUBLE_EQ(predicted_decrease(qp, p), 1.0);

    // The OFF-DIAGONAL IS READ SYMMETRICALLY, and this is what pins it.
    // Reading H as the stored upper triangle alone -- [[2,1],[0,4]], the
    // mirror dropped -- gives
    //     H_upper p = (2*2 + 1*(-1), 0*2 + 4*(-1)) = (3, -4),
    //     p^T H_upper p = 2*3 + (-1)*(-4) = 10,
    //     pred_df = -(-5 + 5) = -0,
    // i.e. EXACTLY ZERO, not 8 and 1. The fixture is chosen so the two
    // readings differ, and the assertion is against 0 rather than against
    // some other nonzero value -- a pred_df of 0 is also the single worst
    // value to get wrong, since KLV Eq. (10) turns on its sign.
    EXPECT_NE(predicted_decrease(qp, p), 0.0);

    // A ZERO STEP predicts exactly zero decrease, on any model.
    EXPECT_DOUBLE_EQ(predicted_decrease(qp, Vec::Zero(2)), 0.0);

    // The sign convention, stated as its own assertion: a step ALONG the
    // gradient of a convex model predicts a decrease of the wrong sign.
    Vec uphill(2);
    uphill << -2.0, 1.0; // -p
    // g^T(-p) = +5, (-p)^T H (-p) = 8 -> pred_df = -(5 + 4) = -9 < 0.
    EXPECT_DOUBLE_EQ(predicted_decrease(qp, uphill), -9.0);
}

// THE BRIEF'S TEST (a). The first full step is rejected, the radius halves,
// and the SAME subproblem is re-solved at the same iterate -- hot-started, so
// the retry does not pay for a fresh factorization.
TEST(SqpDriverTrustRegion, RejectionShrinksAndRetriesHotly) {
    FlatQuarticModel model;
    SqpOptions opts;
    opts.tr_init = 2.0; // the first step (7/6) is INTERIOR to it, by design
    opts.max_iter = 20;
    StepLog log;
    record_steps_into(opts, &log);

    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);
    record_history(sol, "quartic");

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_GE(sol.counters.rejected_steps, 1);
    EXPECT_EQ(sol.counters.rejected_steps, 2); // hand-derived: Delta = 2 and 1
    EXPECT_EQ(sol.counters.major_iters, 3);    // two rejected trials + one accepted
    ASSERT_EQ(sol.history.size(), 4u);         // + the converged terminal row

    // The funnel is started exactly once per solve, at h(x0) = 0.
    EXPECT_EQ(log.resets, 1);
    EXPECT_DOUBLE_EQ(log.h0, 0.0);

    // --- the hand-derived trajectory, row by row -------------------------
    ASSERT_EQ(log.ctx.size(), 3u);
    EXPECT_EQ(log.verdict[0], StepVerdict::kReject);
    EXPECT_EQ(log.verdict[1], StepVerdict::kReject);
    EXPECT_EQ(log.verdict[2], StepVerdict::kAcceptF);

    // Trial 1 at Delta = 2: the model promises 0.5104167 and the objective
    // RISES to +0.262346. pred_df is the MODEL decrease, so it is positive
    // here while the actual difference is negative -- carry 1's whole point.
    EXPECT_NEAR(log.ctx[0].pred_df, 0.5104166666666667, 1e-12);
    EXPECT_NEAR(log.ctx[0].f_old, -0.484375, 1e-12);
    EXPECT_NEAR(log.ctx[0].f_new, 0.2623456790123457, 1e-12);
    EXPECT_LT(log.ctx[0].f_old - log.ctx[0].f_new, 0.0);
    EXPECT_GT(log.ctx[0].pred_df, 0.0);
    EXPECT_FALSE(log.ctx[0].tr_active); // 7/6 < 2

    // Trial 2 at Delta = 1: TR-pinned, pred_df = 0.875 - 0.375 = 0.5.
    EXPECT_NEAR(log.ctx[1].pred_df, 0.5, 1e-12);
    EXPECT_NEAR(log.ctx[1].f_new, -0.234375, 1e-12);
    EXPECT_TRUE(log.ctx[1].tr_active);

    // Trial 3 at Delta = 0.5: pred_df = 0.4375 - 0.09375 = 0.34375, and the
    // objective finally falls, to the EXACT minimizer x0 = 1 (f = -0.75).
    EXPECT_NEAR(log.ctx[2].pred_df, 0.34375, 1e-12);
    EXPECT_NEAR(log.ctx[2].f_new, -0.75, 1e-12);

    // The radius history, recorded per trial: 2 -> 1 -> 0.5, then doubled on
    // the way out because the accepted step's radius was binding and its
    // ratio actual/pred = 0.7727 cleared kTrGrowThreshold.
    EXPECT_DOUBLE_EQ(sol.history[0].tr_radius, 2.0);
    EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 1.0);
    EXPECT_DOUBLE_EQ(sol.history[2].tr_radius, 0.5);
    EXPECT_DOUBLE_EQ(sol.history[3].tr_radius, 1.0);

    // The rejected trials describe the SAME iterate: same f, same residual,
    // and no iterate row in between. This is what "re-solve the same iterate"
    // means, and it is also the TR-CENTERING discipline (the retry re-centers
    // nothing -- seed.x stays zero).
    EXPECT_DOUBLE_EQ(sol.history[0].f, sol.history[1].f);
    EXPECT_DOUBLE_EQ(sol.history[1].f, sol.history[2].f);
    EXPECT_DOUBLE_EQ(sol.history[0].kkt_residual, sol.history[2].kkt_residual);
    for (const SqpIterate &h : sol.history) {
        EXPECT_LE(h.step_norm, h.tr_radius * (1.0 + 1e-9)) << "trial " << h.trial;
    }

    // Landed on the exact minimizer.
    EXPECT_NEAR(sol.x(0), 1.0, 1e-12);
    EXPECT_NEAR(sol.x(1), 0.0, 1e-12);
    EXPECT_NEAR(sol.f, -0.75, 1e-12);

    // --- THE HOT-START OBSERVATION (Task 2's whole point) ----------------
    //
    // The retry re-solves the SAME QpProblem object (same H/g values, so the
    // values/structural hashes are unchanged) on the SAME engine with only
    // SolveOverrides::tr_radius different, seeded with the rejected solve's
    // own working set and x zeroed.
    //
    // ASSERTED AS THE OBSERVED VALUE, NOT AS == 0 UNCONDITIONALLY, per
    // qp_engine.h's HOT-START REUSE note: the four reuse conditions are
    // NECESSARY, not sufficient, and border_candidate's own checks (perturbed
    // pivots, schur_cap) can still force a rebuild. Here all three of the
    // conditions that ARE in this test's control hold on trial 2 -- (i) the
    // subproblem's H/g/Ae/Ai bytes are identical (it is literally the same
    // object, rebuilt only when the iterate moves), (ii) the effective
    // (primal_delta, dual_mu) pair is unchanged (no override is set for
    // either), and (iii) the previous solve exited with NO pin of any kind
    // (7/6 interior to Delta = 2, all bounds infinite), so the fresh seed's
    // all-kFree working set matches that exit exactly. Trial 3 is different
    // on (iii) ALONE and is recorded here rather than asserted at 0: its
    // predecessor exited TR-pinned at x0, and rule (a) reports a TR-pinned
    // index as kFree in bound_state, so the chained seed cannot reproduce
    // that pin and reuse-eligibility condition (b) fails by construction --
    // the same effect test_qp_engine_tr.cpp's ShrinkRadiusRetryReusesHotStart
    // documents at length.
    EXPECT_EQ(sol.history[0].qp_factorizations, 1); // cold: K0 built once
    EXPECT_EQ(sol.history[1].qp_factorizations, 0); // hot: K0 reused outright
    ::testing::Test::RecordProperty("quartic_factorizations",
                                    fmt::format("{} {} {}", sol.history[0].qp_factorizations,
                                                sol.history[1].qp_factorizations,
                                                sol.history[2].qp_factorizations));
}

// THE BRIEF'S TEST (b). Delta grows on an easy problem, and stops at tr_max.
//
// HS3 (f = x2 + 1e-5 (x2 - x1)^2 from (10, 1)) is the fixture because its
// objective is EXACTLY QUADRATIC: the QP model is not an approximation at
// all, so actual == predicted to the last bit and the ratio is 1 on every
// step -- an unambiguous "strong step". Its Newton step wants x1 to move by
// -10 against tr_init = 1, so the radius binds and the growth rule fires.
//
// Task 4 measured 10 majors here at a FIXED radius of 1 (the count tracked
// 1/tr_init exactly). With doubling the radius runs 1, 2, 4, 8 and the fourth
// step lands on x* interior to its own radius.
TEST(SqpDriverTrustRegion, RadiusGrowsOnStrongSteps) {
    const HsProblem p = make_hs(3);
    {
        SqpOptions opts;
        opts.tr_init = 1.0;
        opts.max_iter = 60;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);
        record_history(sol, "hs3_grow");

        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_EQ(sol.counters.rejected_steps, 0) << "no trial is rejected on an exact model";
        EXPECT_LE(sol.counters.major_iters, 6)
            << "observed 4, against Task 4's 10 at a fixed radius";

        double max_radius = 0.0;
        for (const SqpIterate &h : sol.history) {
            max_radius = std::max(max_radius, h.tr_radius);
            EXPECT_LE(h.step_norm, h.tr_radius * (1.0 + 1e-9)) << "trial " << h.trial;
        }
        EXPECT_GT(max_radius, opts.tr_init) << "the radius never grew";
        EXPECT_DOUBLE_EQ(sol.history[0].tr_radius, 1.0);
        EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 2.0);
        EXPECT_DOUBLE_EQ(sol.history[2].tr_radius, 4.0);
        ::testing::Test::RecordProperty("hs3_max_radius", fmt::format("{:g}", max_radius));
    }
    { // tr_max is a CEILING, not a suggestion.
        SqpOptions opts;
        opts.tr_init = 1.0;
        opts.tr_max = 2.0;
        opts.max_iter = 60;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        for (const SqpIterate &h : sol.history) {
            EXPECT_LE(h.tr_radius, opts.tr_max) << "trial " << h.trial;
        }
        EXPECT_GT(sol.counters.major_iters, 4) << "a tighter ceiling must cost majors";
    }
}

// CARRY 2 (ledger, Task-5 review): the driver MUST supply
// StepContext::rejections_at_iterate -- the count of CONSECUTIVE rejections
// already burned at the CURRENT iterate, reset to 0 whenever the iterate
// moves. FunnelStrategy's restoration signature (conjunct (e)) is gated on
// it, and a driver that leaves it at its default 0 gets no kRestore verdict
// ever: fail-safe, but the signature would be dead code.
//
// Asserted on both sides: the COUNT SEQUENCE the strategy actually sees, and
// the kRestore verdict that count unlocks on the fifth trial.
//
// ============================ TASK 9 UPDATE =============================
//
// THE COUPLING NOTE THIS TEST CARRIED HAS BEEN EXERCISED. kRestoreMinRejections
// was re-derived from the shrink factor and MOVED, 3 -> 4 (globalization.h has
// the derivation), so the trial at which conjunct (e) can first hold moved with
// it. THE FIXTURE'S RADIUS SCHEDULE IS RE-DERIVED ACCORDINGLY, tr_init 4 -> 8:
// at the old start the fifth trial would have run at Delta = 0.25, where
// h_new = 1000*0.25^2 = 62.5 is INSIDE the funnel (tau = 100) and the trial is
// accepted rather than restored -- i.e. keeping tr_init = 4 would have turned
// this into a test of a different thing. Starting one doubling higher keeps
// every trial's h_new above tau, so the schedule below is the old one with an
// extra row on the front, and each h_new is still 1000*Delta^2 exactly.
//
// The counts asserted here are written against the CONSTANT, not against the
// literal 4, so a future re-derivation fails on the fixture's arithmetic
// (which is hand-checked above) rather than silently passing.
TEST(SqpDriverTrustRegion, RejectionCountReachesTheRestorationGate) {
    SecondOrderInfeasibleModel model;
    SqpOptions opts;
    opts.tr_init = 8.0;
    opts.max_iter = 20;
    StepLog log;
    record_steps_into(opts, &log);

    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);
    record_history(sol, "restore");

    // tau_0 = max(100, 1.25 * 0.5) = 100, from h(x_start) = 0.5.
    EXPECT_DOUBLE_EQ(log.h0, 0.5);

    // THE WIRING: 0, 1, 2, ... -- one per consecutive rejection at this
    // iterate, up to and including the trial that reaches the gate.
    ASSERT_GE(log.ctx.size(), static_cast<std::size_t>(kRestoreMinRejections) + 1u);
    for (std::size_t k = 0; k <= static_cast<std::size_t>(kRestoreMinRejections); ++k) {
        EXPECT_EQ(log.ctx[k].rejections_at_iterate, static_cast<Index>(k)) << "trial " << k;
        EXPECT_GT(log.ctx[k].h_old, kFunnelFeasEps);   // conjunct (b)
        EXPECT_LE(log.ctx[k].pred_df, 0.0);            // conjunct (c)
        EXPECT_GE(log.ctx[k].h_new, log.ctx[k].h_old); // conjunct (d)
        // Hand-derived violation: 1000 * Delta^2 at Delta = 8, 4, 2, 1, 0.5.
        EXPECT_NEAR(log.ctx[k].h_new, 1000.0 * std::pow(8.0 / std::pow(2.0, k), 2.0), 1e-6)
            << "trial " << k;
        const StepVerdict want = k < static_cast<std::size_t>(kRestoreMinRejections)
                                     ? StepVerdict::kReject
                                     : StepVerdict::kRestore;
        EXPECT_EQ(log.verdict[k], want)
            << "trial " << k
            << ": conjunct (e) needs rejections_at_iterate >= " << kRestoreMinRejections
            << "; a driver that never supplies it can only ever reject";
    }

    // TASK 9 ROUTING: the verdict is recorded on the history row and the driver
    // now ACTS on it -- the restoration phase runs (this fixture's own recovery
    // and its second, capped request are tested in test_sqp_restoration.cpp;
    // what is pinned here is only that the gate handed the request over).
    ASSERT_GE(sol.history.size(), static_cast<std::size_t>(kRestoreMinRejections) + 1u);
    const SqpIterate &gate_row = sol.history[static_cast<std::size_t>(kRestoreMinRejections)];
    EXPECT_EQ(gate_row.verdict, StepVerdict::kRestore);
    EXPECT_TRUE(gate_row.qp_solved);
    EXPECT_EQ(gate_row.qp_status, QpStatus::kOptimal)
        << "the SUBPROBLEM was fine; it is the NLP step that had nowhere to go";
    EXPECT_GE(sol.counters.restoration_iters, 1) << "the phase must have run";
}

// The rejection counter RESETS when the iterate moves. Same fixture as (a),
// whose accepted third trial is followed by a converged major -- so the reset
// is observed by the count never exceeding 1 after an acceptance, and by the
// counter being 0 on the first trial of every new iterate.
TEST(SqpDriverTrustRegion, RejectionCountResetsWhenTheIterateMoves) {
    const HsProblem p = make_hs(1); // Rosenbrock: several rejection runs
    SqpOptions opts;
    opts.max_iter = 100;
    StepLog log;
    record_steps_into(opts, &log);

    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_FALSE(log.ctx.empty());

    Index expected = 0;
    Index runs = 0;
    for (std::size_t k = 0; k < log.ctx.size(); ++k) {
        EXPECT_EQ(log.ctx[k].rejections_at_iterate, expected) << "trial " << k;
        if (log.verdict[k] == StepVerdict::kReject) {
            if (expected == 0) {
                ++runs;
            }
            ++expected;
        } else {
            expected = 0; // the iterate moved
        }
    }
    EXPECT_GT(runs, 0) << "HS1 must actually reject something, or this pins nothing";
    ::testing::Test::RecordProperty("hs1_rejection_runs", static_cast<int>(runs));
}

// The strategy is PLUGGABLE (SqpOptions::make_strategy). Pinned with a
// strategy that accepts everything: the driver then reproduces Task 4's
// full-step behaviour exactly, including HS5's 2-cycle at tr_init = 2.0 --
// which is the cleanest available proof that the globalization, and not some
// incidental change to the loop, is what fixed that fixture.
TEST(SqpDriverTrustRegion, StrategyIsPluggable) {
    class AcceptEverything final : public GlobalizationStrategy {
      public:
        void reset(double) override {}
        StepVerdict judge(const StepContext &) override { return StepVerdict::kAcceptF; }
    };

    const HsProblem p = make_hs(5);
    SqpOptions opts;
    opts.tr_init = 2.0;
    opts.max_iter = 20;
    opts.make_strategy = []() -> std::unique_ptr<GlobalizationStrategy> {
        return std::make_unique<AcceptEverything>();
    };
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);

    EXPECT_EQ(sol.status, SqpStatus::kMaxIter) << "Task 4's cycle, reproduced by an accept-all "
                                                  "strategy";
    EXPECT_EQ(sol.counters.rejected_steps, 0);
    for (const SqpIterate &h : sol.history) {
        EXPECT_DOUBLE_EQ(h.tr_radius, opts.tr_init)
            << "nothing is ever rejected, so nothing shrinks";
    }

    // A factory that returns nullptr is a caller error, not a silent default.
    SqpOptions bad = opts;
    bad.make_strategy = []() -> std::unique_ptr<GlobalizationStrategy> { return nullptr; };
    SqpDriver bad_driver(bad);
    EXPECT_THROW(bad_driver.solve(*p.model), std::invalid_argument);
}

// THE REJECTED-TRIAL HALF of SqpDriverContract.CallCountPerMajorIsBounded's
// budget, which that test cannot pin because none of its fixtures rejects
// anything. A rejected trial re-solves the SAME subproblem, so it costs:
//   * ONE eval_nlp at the trial point (the price of looking before leaping --
//     wasted, gradients and all, and documented as such in sqp_driver.h's
//     MODEL EVALUATION note), and
//   * NO eval_hess at all.
// The second is the load-bearing one: rebuilding the subproblem on a
// rejection would re-evaluate the most expensive thing a model computes AND
// change H/g, which would in turn destroy the hot-start reuse the retry
// depends on (the values hash would move).
TEST(SqpDriverTrustRegion, RejectedTrialsPayNoHessian) {
    CountingModel model(std::make_unique<FlatQuarticModel>());
    SqpOptions opts;
    opts.tr_init = 2.0;
    opts.max_iter = 20;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_EQ(sol.counters.rejected_steps, 2);
    ASSERT_EQ(sol.counters.major_iters, 3);

    // One evaluation per history row: 3 trials + the converged iterate.
    // f/cE/cI are read at every one of them regardless of verdict (Task 8's
    // NlpModel::eval_values, on this default-impl model, calls eval_f itself
    // exactly once per row -- same count eval_nlp always paid).
    EXPECT_EQ(model.n_f, static_cast<Index>(sol.history.size()));
    EXPECT_EQ(model.n_f, 4);
    // TASK 8: n_grad is NOT 4 any more. globalization.h's judge() (this
    // fixture's own two rejections) reads f/h only, so a rejected trial's
    // gradient is never fetched -- only the loop-top evaluation and the ONE
    // accepted trial pay for it (sqp_driver.h's UPGRADE TO FULL sites), i.e.
    // 1 + steps_accepted, not one per row.
    EXPECT_EQ(model.n_grad, 1 + sol.counters.steps_accepted);
    EXPECT_EQ(model.n_grad, 2);
    // ONE Hessian for three subproblem solves: the two retries re-solved it.
    // The + 1 is the bridge's claim pass, not a fourth subproblem: this
    // fixture makes ONE solve(model, ...) call, which builds ONE
    // NlpModelAggregate over it, and laying one walks eval_hess once at the
    // start point (include/hven/model/nlp_model_aggregate.h's constructor
    // doc). FlatQuarticModel declares no rows of either kind, so no Jacobian
    // callback joins it. THE LOAD-BEARING HALF IS UNCHANGED: a rejection still
    // pays no Hessian, which is what `steps_accepted` on the right measures --
    // a subproblem rebuilt on rejection would make this read 3 + 1, not 1 + 1.
    EXPECT_EQ(model.n_hess, sol.counters.steps_accepted + 1);
    EXPECT_EQ(model.n_hess, 2);
    // The counter identity, on a shape where it DOES hold (stopped at an
    // iterate). SqpCounters documents why it is not general.
    EXPECT_EQ(sol.counters.steps_accepted, sol.counters.major_iters - sol.counters.rejected_steps);
}

// FIX ROUND 1 [I1]: THE GROWTH RULE HAS TWO CONJUNCTS AND BOTH ARE PINNED
// HERE. Before this test, deleting either `row.tr_binding` or
// `actual_df >= kTrGrowThreshold * ctx.pred_df` from the radius update left
// the whole suite green -- the rule was asserted only where it FIRES
// (RadiusGrowsOnStrongSteps), never where it must NOT.
TEST(SqpDriverTrustRegion, RadiusHoldsWhenEitherGrowthConjunctFails) {
    { // (a) THE RADIUS WAS NOT BINDING. FlatQuarticModel from (1, 0.5),
      // which is hand-derivable to the last bit: x0 = 1 is ALREADY the
      // minimizer of the quartic coordinate (g0 = 1 - 1 = 0, so p0 = 0), and
      // the x1 coordinate is exactly 1/2 x1^2, so the QP model of the step
      // p1 = -0.5 is EXACT:
      //     pred_df = -(0.5*(-0.5) + 0.5*1*0.25) = 0.25 - 0.125 = 0.125,
      //     f(1, 0.5) = -0.625, f(1, 0) = -0.75, actual = 0.125,
      //     rho = 1 exactly.
      // The step is interior to tr_init = 2 (0.5 < 2), so however perfect the
      // model was, nothing was learned about whether a LARGER radius would
      // have been usable -- and the radius must not move.
        FlatQuarticModel model;
        Vec x0(2);
        x0 << 1.0, 0.5;
        SqpOptions opts;
        opts.tr_init = 2.0;
        opts.max_iter = 20;
        StepLog log;
        record_steps_into(opts, &log);
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model, x0);
        record_history(sol, "quartic_interior");

        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        ASSERT_EQ(log.ctx.size(), 1u);
        EXPECT_EQ(log.verdict[0], StepVerdict::kAcceptF);
        EXPECT_FALSE(log.ctx[0].tr_active) << "the fixture must NOT bind the radius";
        EXPECT_DOUBLE_EQ(log.ctx[0].pred_df, 0.125);
        EXPECT_DOUBLE_EQ(log.ctx[0].f_old - log.ctx[0].f_new, 0.125);
        // rho == 1 clears the ratio threshold outright, so the ONLY thing
        // holding the radius here is the tr_binding conjunct.
        const double rho = (log.ctx[0].f_old - log.ctx[0].f_new) / log.ctx[0].pred_df;
        EXPECT_DOUBLE_EQ(rho, 1.0);
        EXPECT_GE(rho, kTrGrowThreshold);

        ASSERT_EQ(sol.history.size(), 2u);
        EXPECT_DOUBLE_EQ(sol.history[0].tr_radius, 2.0);
        EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 2.0)
            << "the radius grew on a step that never reached it";
    }
    { // (b) THE STEP WAS WEAK. FlatQuarticModel at tr_init = 0.75, hand-derived:
      //   p0 = 0.75 (the unconstrained model step is 7/6, so the radius BINDS),
      //   pred_df = 0.875*0.75 - 0.375*0.5625 = 0.65625 - 0.2109375 = 0.4453125,
      //   f(1.25) = 0.6103515625 - 1.25 = -0.6396484375,
      //   actual  = -0.484375 + 0.6396484375 = 0.1552734375,
      //   rho     = 0.1552734375 / 0.4453125 = 53/152 = 0.348684...
      // Eq. (11) accepts it (0.155 >= 1e-4 * 0.445), but rho is well inside
      // (0, 0.75): the model overpromised by a factor of three, so the radius
      // must be left exactly where it is.
        FlatQuarticModel model;
        SqpOptions opts;
        opts.tr_init = 0.75;
        opts.max_iter = 20;
        StepLog log;
        record_steps_into(opts, &log);
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);
        record_history(sol, "quartic_weak");

        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        ASSERT_GE(log.ctx.size(), 1u);
        EXPECT_EQ(log.verdict[0], StepVerdict::kAcceptF);
        EXPECT_TRUE(log.ctx[0].tr_active) << "the fixture must BIND the radius, or (a) is retested";
        EXPECT_NEAR(log.ctx[0].pred_df, 0.4453125, 1e-12);
        const double rho = (log.ctx[0].f_old - log.ctx[0].f_new) / log.ctx[0].pred_df;
        // 159/1024 divided by 456/1024 = 53/152, exactly.
        EXPECT_NEAR(rho, 53.0 / 152.0, 1e-15);
        EXPECT_GT(rho, 0.0);
        EXPECT_LT(rho, kTrGrowThreshold);

        ASSERT_GE(sol.history.size(), 2u);
        EXPECT_DOUBLE_EQ(sol.history[0].tr_radius, 0.75);
        EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 0.75)
            << "a weak step must not earn a larger radius even with the radius binding";
    }
}

// FIX ROUND 1 [I2]: THE REJECTED SUBPROBLEM'S MULTIPLIERS ARE DISCARDED.
// Leaking them (assigning lambda_e/lambda_i on the kReject path) left every
// test green, so the discard was asserted nowhere.
//
// WHY THE eval_hess RECORDER CANNOT SEE THIS. A rejection does not rebuild
// the subproblem -- that is the point of the hot-started retry, and
// RejectedTrialsPayNoHessian pins it -- so no eval_hess call happens during a
// rejection run at all, and the next one, after an acceptance, is handed the
// ACCEPTED step's multipliers either way. The leak is observable in exactly
// two places, and both are asserted below:
//   (1) the KKT MEASUREMENT of the unmoved iterate. Every history row of one
//       iterate reports grad_lag = grad f + Je^T lambda_e + Ji^T lambda_i at
//       the SAME x, so a leaked lambda changes the reported stationarity of a
//       point that did not move. On this fixture the QP prices its equality
//       row at lambda_e ~ 10, which very nearly annihilates grad f's second
//       entry: the leak would drop the recorded stationarity from 9.9995 to
//       ~0.01 between rows, a thousand-fold (999.95x) "improvement" at a
//       point where nothing happened.
//   (2) the RETURNED multipliers on an exit that never accepted a step, which
//       must still be the zeros the solve started with.
//
// TASK 9: THE BUDGET IS WHAT KEEPS THE SOLVE AT ONE ITERATE. Before the
// restoration phase existed, the kRestore verdict ENDED this solve and every
// row was therefore at the start point. It no longer does, so max_iter is set
// to exactly the number of trials the gate needs (kRestoreMinRejections + 1):
// the request is raised with no budget left to service it, the driver reports
// kMaxIter, and the multipliers are still the zeros this test is about. That
// also covers the no-budget arm of the phase, which no other test reaches.
// tr_init moves 4 -> 8 for the reason given at
// RejectionCountReachesTheRestorationGate.
TEST(SqpDriverTrustRegion, RejectedMultipliersAreDiscarded) {
    SecondOrderInfeasibleModel model;
    SqpOptions opts;
    opts.tr_init = 8.0;
    opts.max_iter = kRestoreMinRejections + 1;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    // Five trials at ONE iterate (4 rejections, then the restoration signature).
    EXPECT_EQ(sol.status, SqpStatus::kMaxIter);
    EXPECT_EQ(sol.counters.restoration_iters, 0) << "there was no budget to restore with";
    ASSERT_EQ(sol.history.size(), static_cast<std::size_t>(kRestoreMinRejections) + 1u);
    ASSERT_EQ(sol.counters.rejected_steps, kRestoreMinRejections);

    // (1) BIT-IDENTICAL measurement rows: the iterate never moved, and neither
    // did the multipliers it is measured against.
    for (std::size_t k = 1; k < sol.history.size(); ++k) {
        EXPECT_DOUBLE_EQ(sol.history[k].stationarity, sol.history[0].stationarity) << "row " << k;
        EXPECT_DOUBLE_EQ(sol.history[k].kkt_residual, sol.history[0].kkt_residual) << "row " << k;
        EXPECT_DOUBLE_EQ(sol.history[k].f, sol.history[0].f) << "row " << k;
        EXPECT_DOUBLE_EQ(sol.history[k].violation_l1, sol.history[0].violation_l1) << "row " << k;
    }
    // The measurement is only load-bearing if the discarded multipliers would
    // have CHANGED it: |grad f| at the start point is ~10 and the QP's own
    // equality price is ~10 with Je = (0, 1), so a leak collapses it.
    EXPECT_NEAR(sol.history[0].stationarity, 9.9995, 1e-9);

    // (2) Nothing was ever accepted, so the reported prices are still zero.
    ASSERT_EQ(sol.lambda_e.size(), 1);
    EXPECT_DOUBLE_EQ(sol.lambda_e(0), 0.0)
        << "a rejected subproblem's multipliers must never reach the caller";
}

// =========================================================================
// SqpDriverQpFailure -- Task 6b: a failed subproblem that still handed back
// a USABLE iterate is a REJECTED STEP, not an abort.
//
// Task 6 shipped the loop with a single rule for a non-kOptimal QpStatus:
// stop, and map the status out. That is right for
// kInfeasible -- there is no step and no radius at which one appears from a
// linearization that has no feasible point at all -- and wrong for the two
// statuses that come back WITH an iterate: kNumericalError (the engine
// refused to certify what it reached) and kMaxIter (the engine ran out of
// minor iterations). Both are properties OF THE SUBPROBLEM AT THIS RADIUS,
// and the driver already owns the instrument that changes the subproblem
// without moving the iterate: shrink Delta and re-solve.
//
// The fixture below is the Task-1 saddle geometry, REPLICATED, because the
// engine's own recovery (qp_engine.h section 4b's POST-PROBE RESTART) is
// one-shot per solve: k independent copies of the geometry need k restarts
// and get one, so every k >= 2 leaves the engine at a saddle it will not
// certify -- a kNumericalError with a finite, bound-feasible iterate,
// reproducibly, in both ws_algebra modes and both with and without that
// engine feature. See the fixture comment for the hand algebra.
// =========================================================================

namespace {

// k INDEPENDENT COPIES of test_qp_engine_indefinite.cpp's weakly-active-bound
// fixture, wrapped as an NLP. Per block b (variables 3b, 3b+1, 3b+2):
//
//     min  x0 x1 + 1/2 x2^2 - x2
//     s.t. x0 + x2 = 0.5,   x0 + x1 <= -2,   x in [-2, 2]^3
//
// The model is exactly quadratic with linear constraints, so at the start
// point x = 0 the SUBPROBLEM in the step variable p is, block by block,
//
//     H = [[0,1,0],[1,0,0],[0,0,1]],  g = grad f(0) = (0, 0, -1),
//     Ae p = -cE(0) = 0.5,  Ai p <= -cI(0) = -2,  -2 <= p <= 2,
//
// i.e. LITERALLY that QP -- provided the trust region does not clip the box,
// which needs Delta >= 2. That is the whole trick of this fixture: at
// Delta >= 2 the subproblem is the pinned engine fixture and fails; at
// Delta < 2 the window clips p2 away from the saddle's p2 = 2 and the same
// subproblem solves. So the shrink-retry is not merely "try again" here, it
// is the thing that changes the answer.
//
// The saddle, per block, is (-1.5, -0.5, 2) at objective 0.75 and the sole
// local minimizer is (0, -2, 0.5) at -0.375; the blocks are independent, so
// the k-block NLP's minimizer is that point repeated, at f* = -0.375 k.
class TwinSaddleModel : public NlpModel {
  public:
    explicit TwinSaddleModel(Index blocks)
        : blocks_(blocks), lo_(Vec::Constant(3 * blocks, -2.0)),
          up_(Vec::Constant(3 * blocks, 2.0)) {}

    Index n() const override { return 3 * blocks_; }
    Index me() const override { return blocks_; }
    Index mi() const override { return blocks_; }
    const Vec &lower() const override { return lo_; }
    const Vec &upper() const override { return up_; }
    Vec start_point() const override { return Vec::Zero(3 * blocks_); }

    // The sole local minimizer and its objective.
    Vec minimizer() const {
        Vec x(3 * blocks_);
        for (Index b = 0; b < blocks_; ++b) {
            x.segment(3 * b, 3) = Vec(Eigen::Vector3d(0.0, -2.0, 0.5));
        }
        return x;
    }
    double f_star() const { return -0.375 * static_cast<double>(blocks_); }

    double eval_f(const Vec &x) const override {
        double f = 0.0;
        for (Index b = 0; b < blocks_; ++b) {
            const Index o = 3 * b;
            f += x(o) * x(o + 1) + 0.5 * x(o + 2) * x(o + 2) - x(o + 2);
        }
        return f;
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(3 * blocks_);
        for (Index b = 0; b < blocks_; ++b) {
            const Index o = 3 * b;
            g(o) = x(o + 1);
            g(o + 1) = x(o);
            g(o + 2) = x(o + 2) - 1.0;
        }
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(blocks_);
        for (Index b = 0; b < blocks_; ++b) {
            c(b) = x(3 * b) + x(3 * b + 2) - 0.5;
        }
        return c;
    }
    Vec eval_ci(const Vec &x) const override {
        Vec c(blocks_);
        for (Index b = 0; b < blocks_; ++b) {
            c(b) = x(3 * b) + x(3 * b + 1) + 2.0;
        }
        return c;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::MatrixXd J = Eigen::MatrixXd::Zero(blocks_, 3 * blocks_);
        for (Index b = 0; b < blocks_; ++b) {
            J(b, 3 * b) = 1.0;
            J(b, 3 * b + 2) = 1.0;
        }
        return J.sparseView();
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::MatrixXd J = Eigen::MatrixXd::Zero(blocks_, 3 * blocks_);
        for (Index b = 0; b < blocks_; ++b) {
            J(b, 3 * b) = 1.0;
            J(b, 3 * b + 1) = 1.0;
        }
        return J.sparseView();
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        Eigen::MatrixXd H = Eigen::MatrixXd::Zero(3 * blocks_, 3 * blocks_);
        for (Index b = 0; b < blocks_; ++b) {
            H(3 * b, 3 * b + 1) = obj_scale;
            H(3 * b + 2, 3 * b + 2) = obj_scale;
        }
        return H.sparseView(); // upper triangle already
    }

  private:
    Index blocks_;
    Vec lo_, up_;
};

// THE DRIVER-LEVEL ARM OF THE SUSPECT-STALL FIXTURE (Task 3b fix round 1).
// A QUADRATIC NLP whose first subproblem IS
// test_qp_engine_indefinite.cpp's suspect_stall_qp(delta, 1e12), so the
// engine's escalation ladder is reached through the driver rather than
// through QpEngine::solve directly:
//
//     min  1/2 (x0^2 - delta*x1^2 + x2^2) - (x0 + x1 + x2)
//     s.t. x0 = 0,   x0, x2 in [-1, 1],   x1 in [-1e12, 1e12]
//
// The objective is quadratic and the constraint linear, so build_subproblem's
// linearization at the start point x = 0 reproduces the QP EXACTLY: H =
// diag(1, -delta, 1), g = (-1, -1, -1), Ae = (1, 0, 0), be = 0. Solved with
// QpOptions::primal_delta == delta, K0's (1, 1) entry is exactly zero and the
// seed factorization is suspect on either backend.
//
// tr_init must exceed x1's bound, or the trust region clips the singular
// direction and the ratio test rescues the solve exactly as the narrow box
// does at the QP level (SuspectLoopStillCertifiesAStationaryPoint) -- which
// is a correct outcome, but not the one this fixture is for.
class SuspectStallModel : public NlpModel {
  public:
    explicit SuspectStallModel(double delta, double x1_bound)
        : delta_(delta), lo_(Vec(3)), up_(Vec(3)) {
        lo_ << -1.0, -x1_bound, -1.0;
        up_ << 1.0, x1_bound, 1.0;
    }

    Index n() const override { return 3; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return 0.5 * (x(0) * x(0) - delta_ * x(1) * x(1) + x(2) * x(2)) - x.sum();
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(3);
        g << x(0) - 1.0, -delta_ * x(1) - 1.0, x(2) - 1.0;
        return g;
    }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, x(0)); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatRM eval_hess(const Vec &, double s, const Vec &, const Vec &) const override {
        // cE is linear, so the Lagrangian Hessian is s * hess(f) alone.
        Eigen::MatrixXd hd = Eigen::MatrixXd::Zero(3, 3);
        hd(0, 0) = s;
        hd(1, 1) = -s * delta_;
        hd(2, 2) = s;
        return SpMatRM(hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView());
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::MatrixXd jd(1, 3);
        jd << 1, 0, 0;
        return jd.sparseView();
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 3);
    }
    const Vec &lower() const override { return lo_; }
    const Vec &upper() const override { return up_; }
    Vec start_point() const override { return Vec::Zero(3); }

  private:
    double delta_;
    Vec lo_, up_;
};

} // namespace

// THE PREDICATE, unit-tested away from any engine. It is the whole of the
// routing decision, and every one of its arms is a different failure mode:
//
//   * kOptimal is not a failure at all and kInfeasible is not RETRYABLE --
//     the linearization has no feasible point, and shrinking the radius only
//     removes candidate points, so a smaller Delta cannot make one appear.
//     (That branch stays exactly what Task 6 shipped: propagate.)
//   * a NON-FINITE iterate means the linear algebra produced nothing to seed
//     from; the returned working set describes a point that does not exist.
//   * a BOUND-INFEASIBLE iterate is the same thing said geometrically: the
//     engine holds every iterate inside the box by construction, so one
//     outside it is evidence the answer is not the engine's own arithmetic.
//
// The two positive arms are the ones the loop uses: kNumericalError and
// kMaxIter with a finite iterate inside the box.
TEST(SqpDriverQpFailure, RetryabilityIsFinitenessAndTheBox) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1.0);
    qp.upper = Vec::Constant(2, 1.0);

    auto make = [](QpStatus st, double x0, double x1) {
        QpSolution qs;
        qs.status = st;
        qs.x = Vec(2);
        qs.x << x0, x1;
        return qs;
    };

    const double tol = 1e-8;
    // Positive arms.
    EXPECT_TRUE(qp_failure_is_retryable(qp, make(QpStatus::kNumericalError, 0.5, -0.5), tol));
    EXPECT_TRUE(qp_failure_is_retryable(qp, make(QpStatus::kMaxIter, 1.0, -1.0), tol));
    // Exactly ON a bound, and inside the tolerance band just past it.
    EXPECT_TRUE(qp_failure_is_retryable(qp, make(QpStatus::kMaxIter, 1.0 + 0.5 * tol, 0.0), tol));

    // Not a failure / not fixable by shrinking.
    EXPECT_FALSE(qp_failure_is_retryable(qp, make(QpStatus::kOptimal, 0.0, 0.0), tol));
    EXPECT_FALSE(qp_failure_is_retryable(qp, make(QpStatus::kInfeasible, 0.0, 0.0), tol));

    // Nothing usable came back.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(qp_failure_is_retryable(qp, make(QpStatus::kNumericalError, nan, 0.0), tol));
    EXPECT_FALSE(qp_failure_is_retryable(qp, make(QpStatus::kNumericalError, inf, 0.0), tol));
    EXPECT_FALSE(qp_failure_is_retryable(qp, make(QpStatus::kNumericalError, 1.1, 0.0), tol));
    EXPECT_FALSE(qp_failure_is_retryable(qp, make(QpStatus::kNumericalError, 0.0, -1.1), tol));

    // A size mismatch is not a usable iterate either.
    QpSolution empty;
    empty.status = QpStatus::kNumericalError;
    empty.x = Vec(0);
    EXPECT_FALSE(qp_failure_is_retryable(qp, empty, tol));
}

// THE ROUTING, end to end. Task 6 returned SqpStatus::kNumericalError here in
// one major without ever moving; the loop now shrinks and re-solves.
//
// HAND-DERIVED TRAJECTORY (2 blocks, tr_init = 3, both algebra modes):
//
//   trial 0, Delta = 3: the window max(-2, -3) .. min(2, 3) IS the box, so
//     the subproblem is two copies of the pinned engine fixture. The engine's
//     zero-multiplier probe refuses to certify the twin saddle and its
//     one-shot post-probe restart can only repair one of the two blocks, so
//     it returns kNumericalError at |p|inf = 2. Routed as a REJECTION:
//     Delta <- 1.5, rejections_at_iterate <- 1, nothing else moves.
//   trial 1, Delta = 1.5: the window now clips p2 at 1.5 < 2, the saddle is
//     outside it, and the subproblem solves. |p|inf = 1.5, accepted.
//   trial 2, Delta = 1.5: |p|inf = 0.5, accepted, landing exactly on the
//     minimizer.
//   trial 3: converged, no subproblem built.
//
// So: 3 subproblems, 1 rejected step, 2 accepted, f = -0.75 = 2 * (-0.375).
// Measured IDENTICAL with and without the engine's post-probe restart -- the
// restart changes which saddle the engine stops at, not that it stops.
TEST(SqpDriverQpFailure, QpNumericalErrorShrinksInsteadOfAborting) {
    TwinSaddleModel model(2);
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        SqpOptions opts;
        opts.tr_init = 3.0;
        opts.tr_max = 100.0;
        opts.max_iter = 40;
        opts.qp.ws_algebra = algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);

        // THE FIX: the solve recovers rather than aborting.
        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_NEAR(sol.f, model.f_star(), 1e-9);
        EXPECT_LT((sol.x - model.minimizer()).lpNorm<Eigen::Infinity>(), 1e-8);

        // THE ROUTE, pinned so a solve that reached the answer some other way
        // would not pass silently.
        ASSERT_EQ(sol.history.size(), 4u);
        EXPECT_TRUE(sol.history[0].qp_solved);
        EXPECT_EQ(sol.history[0].qp_status, QpStatus::kNumericalError);
        EXPECT_EQ(sol.history[0].verdict, StepVerdict::kReject)
            << "a routed QP failure is recorded as the rejection it is";
        EXPECT_DOUBLE_EQ(sol.history[0].tr_radius, 3.0);
        EXPECT_NEAR(sol.history[0].step_norm, 2.0, 1e-9); // the twin saddle

        // The radius halved and the iterate did NOT move: rows 0 and 1
        // measure the same point, bit for bit.
        EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 1.5);
        EXPECT_EQ(sol.history[1].qp_status, QpStatus::kOptimal);
        EXPECT_DOUBLE_EQ(sol.history[1].f, sol.history[0].f);
        EXPECT_DOUBLE_EQ(sol.history[1].stationarity, sol.history[0].stationarity);

        EXPECT_EQ(sol.counters.major_iters, 3);
        EXPECT_EQ(sol.counters.rejected_steps, 1);
        EXPECT_EQ(sol.counters.steps_accepted, 2);
    }
}

// AGGREGATION OF THE ENGINE'S SUSPECT-STALL LADDER (Task 3b fix round 1).
//
// QpCounters::suspect_escalations is the exhaustion diagnostic for the QP
// engine's suspect-stall gate: nonzero means a would-be-kOptimal subproblem
// exit off a SUSPECT factorization failed the free-block stationarity check
// and the engine escalated primal_delta rather than certifying a non-KKT
// point. A driver whose subproblem was refused for that reason has no other
// way to tell its caller why, so the counter must ROLL UP -- exactly like the
// two refinement counters beside it -- rather than dying at the QP boundary.
// Task 7 reads it off SqpSolveRecord, which embeds SqpCounters wholesale.
//
// The model's first subproblem IS the engine-level stall fixture (see
// SuspectStallModel), so this pins the whole path: engine spends a rung,
// engine refuses, driver routes the failure as a rejected step, and the rung
// arrives in SqpSolution::counters.
TEST(SqpDriverQpFailure, SuspectEscalationsAggregateToTheDriver) {
    constexpr double kDelta = 1e-10;
    SuspectStallModel model(kDelta, /*x1_bound=*/1e12);
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        SqpOptions opts;
        opts.tr_init = 1e13; // must exceed x1's bound -- see the model's note
        opts.tr_max = 1e13;
        opts.max_iter = 10;
        opts.qp.ws_algebra = algebra;
        opts.qp.primal_delta = kDelta;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);

        // The engine refused the first subproblem, and it refused it through
        // the gate rather than through some other exit.
        ASSERT_FALSE(sol.history.empty());
        EXPECT_EQ(sol.history[0].qp_status, QpStatus::kNumericalError);

        // THE ASSERTION THIS TEST EXISTS FOR: the rung is visible at driver
        // level. Without the aggregation this reads 0 while the QP-level
        // counter reads >= 1.
        EXPECT_GE(sol.counters.suspect_escalations, 1)
            << "the engine's exhaustion diagnostic never reached SqpCounters";

        // A control on the SAME model with the trust region tight enough to
        // clip the singular direction: the ratio test rescues the solve, the
        // gate passes, no rung is spent, and the aggregate stays 0. This is
        // what keeps the assertion above from passing on a counter that is
        // merely always nonzero.
        SqpOptions tight = opts;
        tight.tr_init = 1.0;
        tight.tr_max = 1.0;
        SqpDriver tight_driver(tight);
        const SqpSolution tight_sol = tight_driver.solve(model);
#ifdef USE_ACCELERATE_SPARSE
        // OBSERVED ON ACCELERATE (D9 re-measurement, merge bdf64da): 10
        // rungs, not 0 -- exactly one per subproblem solve (max_iter == 10).
        // The clipped trust region reproduces the narrow-box shape of
        // QpEngineIndefinite.SuspectLoopStillCertifiesAStationaryPoint at
        // EVERY major: the quadratic model re-derives the same
        // exactly-singular K0 at each linearization point, and Accelerate's
        // subspace-clean solves reach the gate instead of the MKL ratio-test
        // rescue, so each subproblem spends the one de-singularizing rung
        // before certifying kOptimal. Observed here (both modes, Release and
        // Debug): kMaxIter at x1 == 10 with every per-major qp_status
        // kOptimal -- the ratio-clipped +1-per-major trajectory the fixture
        // describes, with only the rung counter diverging from the MKL
        // expectation below. The exact one-rung-per-solve pin keeps
        // this control's discriminating power: a counter that were merely
        // always nonzero could not land on precisely 10.
        EXPECT_EQ(tight_sol.counters.suspect_escalations, 10)
            << "expected exactly one de-singularizing rung per subproblem solve";
#else
        EXPECT_EQ(tight_sol.counters.suspect_escalations, 0)
            << "a solve whose subproblems all certified cleanly must report no rungs";
#endif
    }
}

// THE OTHER HALF OF THE CONTRACT: the retry is ONE-SHOT, so a subproblem that
// fails again at the shrunken radius still propagates. Three blocks need
// three engine restarts and get one, and the box is [-2,2] on every variable,
// so BOTH Delta = 4 and Delta = 2 leave the window equal to the box and hand
// back the same unrepairable saddle. Without a propagation rule this fixture
// would shrink forever until max_iter.
TEST(SqpDriverQpFailure, RepeatedQpNumericalErrorStillPropagates) {
    TwinSaddleModel model(3);
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        SqpOptions opts;
        opts.tr_init = 4.0;
        opts.tr_max = 100.0;
        opts.max_iter = 40;
        opts.qp.ws_algebra = algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);

        EXPECT_EQ(sol.status, SqpStatus::kNumericalError);
        ASSERT_EQ(sol.history.size(), 2u);
        EXPECT_EQ(sol.history[0].qp_status, QpStatus::kNumericalError);
        EXPECT_EQ(sol.history[1].qp_status, QpStatus::kNumericalError);
        EXPECT_DOUBLE_EQ(sol.history[0].tr_radius, 4.0);
        EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 2.0);
        EXPECT_EQ(sol.counters.major_iters, 2);
        EXPECT_EQ(sol.counters.rejected_steps, 1);
        EXPECT_EQ(sol.counters.steps_accepted, 0);

        // The exit is AT the start point -- nothing was ever accepted.
        EXPECT_EQ(sol.x.lpNorm<Eigen::Infinity>(), 0.0);
        // This is the qp_solved == true kNumericalError shape (see
        // sqp_driver.h's SUBPROBLEM FAILURE ROUTING): the SUBPROBLEM failed, not the
        // iterate.
        EXPECT_TRUE(sol.history.back().qp_solved);
    }
}

// THE kMaxIter REVISIT (Task 6's ledger item). Task 6 mapped a QP kMaxIter
// straight to SqpStatus::kNumericalError on the grounds that "an unconverged
// subproblem's x is not a certified step, and this driver has no mechanism to
// take a partial one safely". The first clause is still true and the second
// is no longer: the shrink-retry does not TAKE the partial step, it discards
// it and asks a smaller subproblem. So kMaxIter now takes the same route, and
// the interim mapping survives only for the repeated case -- which is the
// pathological one it was written for.
//
// FIXTURE: HS6 with the ENGINE's minor-iteration budget capped at 3. Its
// second subproblem (at Delta = 2, after the first step grew the radius)
// needs a fourth minor iteration and comes back kMaxIter; retried at
// Delta = 1 it converges in 3, and the solve reaches f* = 0 exactly.
// Uncapped, HS6 converges in 8 majors with 0 rejections (Task 6's
// measurement); capped it takes 7 majors with 1 rejection -- the retry is
// visible in the counters, not hidden by them.
TEST(SqpDriverQpFailure, QpMaxIterTakesTheSameShrinkRetryPath) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        const HsProblem hs = make_hs(6);
        SqpOptions opts;
        opts.max_iter = 60;
        opts.qp.max_iter = 3;
        opts.qp.ws_algebra = algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*hs.model);

        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_NEAR(sol.f, hs.f_star, 1e-9);

        // Exactly one subproblem hit the cap, and it was ROUTED, not fatal.
        int capped_rows = 0;
        for (std::size_t k = 0; k < sol.history.size(); ++k) {
            if (!sol.history[k].qp_solved || sol.history[k].qp_status != QpStatus::kMaxIter) {
                continue;
            }
            ++capped_rows;
            ASSERT_LT(k + 1, sol.history.size()) << "a capped row must not be the last one";
            EXPECT_DOUBLE_EQ(sol.history[k + 1].tr_radius, 0.5 * sol.history[k].tr_radius);
            EXPECT_DOUBLE_EQ(sol.history[k + 1].f, sol.history[k].f) << "the iterate must not move";
        }
        EXPECT_EQ(capped_rows, 1);
        EXPECT_EQ(sol.counters.rejected_steps, 1);
    }
}

// ... AND kMaxIter STILL PROPAGATES WHEN THE RETRY FAILS TOO. HS76 is a
// convex QP whose FIRST subproblem needs more than 3 minor iterations at
// every radius the loop tries, so the retry hits the same cap and the interim
// kMaxIter -> kNumericalError mapping fires exactly where it was meant to.
TEST(SqpDriverQpFailure, RepeatedQpMaxIterStillPropagates) {
    const HsProblem hs = make_hs(76);
    SqpOptions opts;
    opts.max_iter = 60;
    opts.qp.max_iter = 3;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*hs.model);

// OBSERVED (results note D5): on Accelerate, HS76's subproblem at the halved
// radius finishes INSIDE the 3-minor-iteration cap, so the first retry
// succeeds (row 1, kOptimal at Delta = 0.5, step accepted -- f moves from
// -1.25 to -3.05859375 on row 2) and, per the reset rules the next test
// documents, a FRESH failure chain starts; the second chain (rows 2-3)
// repeats kMaxIter and the interim kMaxIter -> kNumericalError mapping fires
// there. Same terminal contract, longer chain: the fixture's "needs more than
// 3 minors at every radius" premise is Pardiso-specific.
#ifdef USE_ACCELERATE_SPARSE
    EXPECT_EQ(sol.status, SqpStatus::kNumericalError);
    ASSERT_EQ(sol.history.size(), 4u);
    EXPECT_EQ(sol.history[0].qp_status, QpStatus::kMaxIter);
    EXPECT_EQ(sol.history[1].qp_status, QpStatus::kOptimal);
    EXPECT_EQ(sol.history[2].qp_status, QpStatus::kMaxIter);
    EXPECT_EQ(sol.history[3].qp_status, QpStatus::kMaxIter);
    EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 0.5 * sol.history[0].tr_radius);
    EXPECT_DOUBLE_EQ(sol.history[3].tr_radius, 0.5 * sol.history[2].tr_radius);
    EXPECT_EQ(sol.counters.rejected_steps, 2);
#elif defined(NDEBUG)
    // U0 (unified flags, 2026-08-16, declared): under -ffast-math
    // -march=native the MKL Release chain takes the SAME longer shape the
    // Accelerate arm above documents -- the subproblem at the halved radius
    // finishes inside the 3-minor cap, so the first retry succeeds and a
    // fresh failure chain starts. The D5 observation's own diagnosis said it:
    // the fixture's "needs more than 3 minors at every radius" premise is a
    // codegen-level property, not a Pardiso guarantee, and the unified flags
    // moved MKL Release onto the other side of it. The terminal contract --
    // kMaxIter repeats exhaust the retry and map to kNumericalError -- is
    // UNCHANGED (the status pin did not move). Debug, whose arithmetic the
    // unification did not touch, still walks the original two-row chain (the
    // #else arm). Two fresh reproductions agreed.
    EXPECT_EQ(sol.status, SqpStatus::kNumericalError);
    ASSERT_EQ(sol.history.size(), 4u);
    EXPECT_EQ(sol.history[0].qp_status, QpStatus::kMaxIter);
    EXPECT_EQ(sol.history[1].qp_status, QpStatus::kOptimal);
    EXPECT_EQ(sol.history[2].qp_status, QpStatus::kMaxIter);
    EXPECT_EQ(sol.history[3].qp_status, QpStatus::kMaxIter);
    EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 0.5 * sol.history[0].tr_radius);
    EXPECT_DOUBLE_EQ(sol.history[3].tr_radius, 0.5 * sol.history[2].tr_radius);
    EXPECT_EQ(sol.counters.rejected_steps, 2);
#else
    EXPECT_EQ(sol.status, SqpStatus::kNumericalError);
    ASSERT_EQ(sol.history.size(), 2u);
    EXPECT_EQ(sol.history[0].qp_status, QpStatus::kMaxIter);
    EXPECT_EQ(sol.history[1].qp_status, QpStatus::kMaxIter);
    EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 0.5 * sol.history[0].tr_radius);
    EXPECT_EQ(sol.counters.rejected_steps, 1);
#endif
}

// THE ROUTED ROW IS EVIDENCE THE RESTORATION GATE READS, AND THE "ONE-SHOT"
// BOUND DOES NOT LIMIT HOW MUCH. This test exists because the header used to
// claim it did -- "at most one of the three can come from a QP failure, since
// the retry is one-shot" -- which is false, and falsified by the driver's own
// two reset rules:
//
//     qp_failures_in_a_row   resets on ANY subproblem that reaches kOptimal
//     rejections_at_iterate  resets only when a step is ACCEPTED
//
// so the one-shot bound is per FAILURE CHAIN, and a successful solve in
// between starts a fresh chain while the rejection count keeps climbing.
//
// FIXTURE: HS1 (Rosenbrock, box-constrained) from its published start at
// tr_init = 12 with the ENGINE's minor-iteration budget capped at 2. The
// observed trial sequence, identical in both algebra modes:
//
//   trial 0, Delta = 12   kOptimal, ACCEPTED  -> iterate moves; counts reset
//   trial 1, Delta = 12   kMaxIter, ROUTED    -> rejections_at_iterate = 1
//   trial 2, Delta =  6   kOptimal, kReject   -> rejections_at_iterate = 2,
//                                                and the failure CHAIN resets
//   trial 3, Delta =  3   kMaxIter, ROUTED    -> rejections_at_iterate = 3
//   trial 4, Delta =  1.5 kMaxIter            -> chain is 1, so it PROPAGATES
//
// At trial 3 the count reaches kRestoreMinRejections with TWO of its three
// entries contributed by routed QP failures that no strategy ever judged. Had
// trial 4's subproblem succeeded, judge() would have been called with
// ctx.rejections_at_iterate == 3 and conjunct (e) would have been satisfied on
// majority-QP-failure evidence.
//
// THAT IS DEFENSIBLE, AND THE POINT IS THAT IT IS DELIBERATE: conjunct (e)
// says "the radius has already been shrunk this many times at this iterate
// without the trial escaping the funnel", and a routed row satisfies that
// description exactly -- the radius was shrunk, the iterate did not move,
// nothing escaped. Nothing in KLV Lemma 5's hypothesis requires the shrink to
// have been caused by a JUDGED trial. What is not defensible is claiming the
// mix is bounded when it is not, so the mix is measured here instead.
// TASK 8 RE-MEASURED THIS TEST, AND IT DID NOT MOVE -- reported rather than
// assumed, because the elastic tier changed what a kInfeasible subproblem
// causes and this test is about the RESTORATION GATE. Every failing solve in
// the trajectory below returns kMaxIter (the opts.qp.max_iter = 2 cap), never
// kInfeasible, so the tier never activates: all five rows, both algebra
// modes, are bit-identical to Task 7's measurement.
//
// ============================ TASK 9 UPDATE =============================
//
// THE TRAJECTORY IS UNCHANGED AGAIN (re-measured: the same five rows, both
// modes) AND ITS CONCLUSION IS NOT. This test used to be called
// RoutedFailuresCanFillTheRestorationGate, and at kRestoreMinRejections = 3 it
// was: the count above reaches exactly 3 with two of its three entries routed.
// Task 9's re-derivation of that constant took it to 4 with THIS MEASUREMENT
// AS ONE OF ITS TWO ARGUMENTS (globalization.h, argument (II)): because the
// shrink-retry is one-shot per chain, routed rows cannot be ADJACENT, so at
// most ceil(r/2) of r consecutive rejections can be routed -- a MAJORITY only
// at odd r, and 4 is the smallest count that rules it out. So this fixture now
// stops ONE SHORT of the gate, and that is the derivation working as intended
// rather than coverage lost: what the test pins is unchanged (routed failures
// DO count toward the gate, and the mix is what it is), plus the new fact that
// the re-derived gate is not reachable from a 2-of-3 routed mix.
TEST(SqpDriverQpFailure, RoutedFailuresCountTowardTheRestorationGate) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        const HsProblem hs = make_hs(1);
        SqpOptions opts;
        opts.tr_init = 12.0;
        opts.tr_max = 100.0;
        opts.max_iter = 60;
        opts.qp.max_iter = 2;
        opts.qp.ws_algebra = algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*hs.model);

        ASSERT_EQ(sol.history.size(), 5u);
        // Trial 0 moved the iterate, so the rejection count restarts from it.
        EXPECT_EQ(sol.history[0].qp_status, QpStatus::kOptimal);
        EXPECT_NE(sol.history[0].verdict, StepVerdict::kReject);

        // Trials 1..3 are the three that fill the gate, all at ONE iterate
        // (same f, bit for bit) with the radius halving on every one.
        for (std::size_t k = 1; k <= 3; ++k) {
            SCOPED_TRACE("trial " + std::to_string(k));
            EXPECT_TRUE(sol.history[k].qp_solved);
            EXPECT_DOUBLE_EQ(sol.history[k].f, sol.history[1].f) << "the iterate must not move";
            EXPECT_EQ(sol.history[k].verdict, StepVerdict::kReject);
            EXPECT_DOUBLE_EQ(sol.history[k].tr_radius, 12.0 / std::pow(2.0, k - 1));
        }
        // TWO OF THE THREE ARE ROUTED FAILURES, and the middle one is a genuine
        // judged rejection -- which is exactly what resets the failure chain
        // and lets the third routed failure happen at all.
        EXPECT_EQ(sol.history[1].qp_status, QpStatus::kMaxIter);
        EXPECT_EQ(sol.history[2].qp_status, QpStatus::kOptimal);
        EXPECT_EQ(sol.history[3].qp_status, QpStatus::kMaxIter);

        // rejections_at_iterate is driver-internal, but at this iterate it is
        // exactly the rejections counted since trial 0's acceptance, i.e. the
        // whole of counters.rejected_steps. It reaches the gate's threshold.
        EXPECT_EQ(sol.counters.rejected_steps, 3);
        EXPECT_EQ(sol.counters.rejected_steps, kRestoreMinRejections - 1)
            << "conjunct (e) reads exactly this count. Two of its three entries were never "
               "judged by any strategy, and the re-derived gate (globalization.h, argument "
               "(II)) is one higher precisely so that a mix like this one cannot open it";
        // THE NON-ADJACENCY THE DERIVATION RESTS ON, pinned directly: no two
        // consecutive rows are both routed failures, because the second
        // failure of one chain propagates instead of shrinking.
        // (The last row is excluded: it is the SECOND failure of one chain,
        // which is adjacent to its predecessor by definition and is what ENDS
        // the solve rather than adding to the count.)
        for (std::size_t k = 1; k + 2 < sol.history.size(); ++k) {
            const bool routed_here = sol.history[k].qp_status != QpStatus::kOptimal;
            const bool routed_next = sol.history[k + 1].qp_status != QpStatus::kOptimal;
            EXPECT_FALSE(routed_here && routed_next)
                << "rows " << k << "," << k + 1 << " are adjacent routed failures, which the "
                << "one-shot chain must make impossible except on the terminating pair";
        }

        // Trial 4 is the SECOND consecutive failure of one chain, so it
        // propagates rather than shrinking a fourth time.
        EXPECT_EQ(sol.history[4].qp_status, QpStatus::kMaxIter);
        EXPECT_EQ(sol.status, SqpStatus::kNumericalError);

        // AND THE ELASTIC TIER NEVER ACTIVATED (fix round 1): every failure
        // above is kMaxIter, so this trajectory is bit-identical to Task 7's.
        // Pinned rather than asserted in prose, so that a future change which
        // routes some other status into the tier cannot silently rewrite what
        // this test is measuring.
        EXPECT_EQ(sol.counters.elastic_activations, 0);
    }
}

// =========================================================================
// SqpDriverSoc -- Task 7: the second-order correction.
// =========================================================================

// FIX ROUND 1, I1: build_soc_subproblem UNIT-TESTED DIRECTLY, away from any
// engine or NLP model -- exactly the reason qp_failure_is_retryable is a
// free function (Task 6b's own precedent). Before this test the inequality
// branch (sqp_driver.h:1270-1279 as of commit 13bb229) was exercised by
// ZERO tests: every SOC-firing fixture in this file has mi() == 0. Two
// mutations survived as a result -- flipping bi_soc's sign, and deleting the
// "only on ACTIVE rows" guard -- because nothing ever read soc_qp.bi. This
// test reads it directly, on a MIXED equality+inequality QP with one ACTIVE
// and one INACTIVE inequality row, and is exact (EXPECT_DOUBLE_EQ) rather
// than approximate specifically so a sign error or a dropped guard cannot
// pass by coincidence.
//
// HAND-DERIVED. n = 2, me = 1, mi = 2. Ae = [1 0], Ai = [[1 0], [0 1]] (so
// Ae*p and Ai*p read off p's own components directly). p = (0.3, 0.7).
// ev.ce = 0.5, ev.ci = (0.2, -5.0) (the CURRENT iterate); ev_trial.ce = 1.2,
// ev_trial.ci = (0.9, -4.5) (the REJECTED trial point x + p). Row 0 is
// ACTIVE, row 1 is NOT.
//
//   EQUALITY:   r_e  = ev_trial.ce - ev.ce - Ae.p = 1.2 - 0.5 - 0.3 = 0.4
//               be_soc = -ev.ce - r_e = -0.5 - 0.4 = -0.9
//   INEQUALITY, row 0 (active):
//               r_i0 = ev_trial.ci(0) - ev.ci(0) - Ai.p(0) = 0.9-0.2-0.3=0.4
//               bi_soc(0) = -ev.ci(0) - r_i0 = -0.2 - 0.4 = -0.6
//   INEQUALITY, row 1 (INACTIVE): bi_soc(1) MUST equal the ORIGINAL qp.bi(1)
//               = 7.5 exactly, UNCHANGED -- not recomputed. The formula
//               applied anyway (what the deleted-guard mutation does) would
//               give r_i1 = -4.5-(-5.0)-0.7 = -0.2, bi_soc(1) = 5.0+0.2=5.2,
//               a DIFFERENT number, which is exactly what makes this row a
//               mutation-killing assertion rather than a vacuous one.
TEST(SqpDriverSoc, BuildSocSubproblemShiftsRhsOnActiveRowsOnly) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 1.0;
    qp.H.insert(1, 1) = 1.0;
    qp.H.makeCompressed();
    qp.g = Vec(2);
    qp.g << -1.0, -1.0;

    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(1, 2);
    qp.Ae.insert(0, 0) = 1.0;
    qp.Ae.makeCompressed();
    qp.be = Vec(1);
    qp.be << -0.5; // the ORIGINAL rhs the rejected solve used; irrelevant to the shift

    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(2, 2);
    qp.Ai.insert(0, 0) = 1.0;
    qp.Ai.insert(1, 1) = 1.0;
    qp.Ai.makeCompressed();
    qp.bi = Vec(2);
    qp.bi << 1.0, 7.5; // row 1's value is the one that must SURVIVE unchanged

    qp.lower = Vec::Constant(2, -kInfBound);
    qp.upper = Vec::Constant(2, kInfBound);
    ASSERT_NO_THROW(qp.validate());

    NlpEval ev;
    ev.ce = Vec(1);
    ev.ce << 0.5;
    ev.ci = Vec(2);
    ev.ci << 0.2, -5.0;

    NlpEval ev_trial;
    ev_trial.ce = Vec(1);
    ev_trial.ce << 1.2;
    ev_trial.ci = Vec(2);
    ev_trial.ci << 0.9, -4.5;

    Vec p(2);
    p << 0.3, 0.7;
    std::vector<bool> ineq_active = {true, false};

    const QpProblem soc_qp = build_soc_subproblem(qp, ev, ev_trial, p, ineq_active);

    ASSERT_EQ(soc_qp.be.size(), 1);
    EXPECT_DOUBLE_EQ(soc_qp.be(0), -0.9);

    ASSERT_EQ(soc_qp.bi.size(), 2);
    EXPECT_DOUBLE_EQ(soc_qp.bi(0), -0.6);     // ACTIVE: shifted
    EXPECT_DOUBLE_EQ(soc_qp.bi(1), qp.bi(1)); // INACTIVE: UNCHANGED
    EXPECT_DOUBLE_EQ(soc_qp.bi(1), 7.5);      // ... and specifically not 5.2

    // ONLY be/bi are touched: H, g, Ae, Ai, lower, upper are the SAME
    // bytes -- this is what lets the re-solve reuse qp_engine.h's hot-start
    // K0 (see sqp_driver.h's WARM START paragraph).
    EXPECT_EQ(soc_qp.H.nonZeros(), qp.H.nonZeros());
    for (Index i = 0; i < 2; ++i) {
        EXPECT_DOUBLE_EQ(soc_qp.H.coeff(i, i), qp.H.coeff(i, i));
    }
    EXPECT_DOUBLE_EQ(soc_qp.g(0), qp.g(0));
    EXPECT_DOUBLE_EQ(soc_qp.g(1), qp.g(1));
    EXPECT_DOUBLE_EQ(soc_qp.lower(0), qp.lower(0));
    EXPECT_DOUBLE_EQ(soc_qp.upper(0), qp.upper(0));
}

// A pure-equality QP (mi == 0): the inequality branch must not be entered
// at all (it is guarded by qp.mi() > 0), and be_soc is the same formula in
// isolation.
TEST(SqpDriverSoc, BuildSocSubproblemHandlesEqualityOnlyQp) {
    QpProblem qp;
    qp.H = SpMatRM(1, 1);
    qp.H.insert(0, 0) = 2.0;
    qp.H.makeCompressed();
    qp.g = Vec(1);
    qp.g << 1.0;
    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(1, 1);
    qp.Ae.insert(0, 0) = 2.0;
    qp.Ae.makeCompressed();
    qp.be = Vec(1);
    qp.be << 3.0;
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 1);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(1, -kInfBound);
    qp.upper = Vec::Constant(1, kInfBound);
    ASSERT_NO_THROW(qp.validate());

    NlpEval ev;
    ev.ce = Vec(1);
    ev.ce << -1.5;
    NlpEval ev_trial;
    ev_trial.ce = Vec(1);
    ev_trial.ce << 4.0;

    Vec p(1);
    p << 2.0; // Ae*p = 4.0
    const QpProblem soc_qp = build_soc_subproblem(qp, ev, ev_trial, p, {});

    // r_e = 4.0 - (-1.5) - 4.0 = 1.5; be_soc = -(-1.5) - 1.5 = 0.0.
    EXPECT_DOUBLE_EQ(soc_qp.be(0), 0.0);
    EXPECT_EQ(soc_qp.bi.size(), 0);
}

namespace {

// THE CLASSIC MARATOS FIXTURE (e.g. Nocedal & Wright, "Numerical
// Optimization", the discussion of the Maratos effect):
//
//     min   2(x0^2 + x1^2 - 1) - x0     s.t.  x0^2 + x1^2 = 1
//
// The true minimizer is x* = (1, 0), f* = -1 (on the circle the first term
// is identically 0, so the problem reduces to minimizing -x0 on the unit
// circle). The classic illustration starts at x0 = (cos theta, sin theta)
// for small theta > 0, i.e. EXACTLY ON the constraint, close to x*.
class MaratosModel : public NlpModel {
  public:
    explicit MaratosModel(double theta) : theta_(theta) {}
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return 2.0 * (x(0) * x(0) + x(1) * x(1) - 1.0) - x(0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 4.0 * x(0) - 1.0, 4.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(0) * x(0) + x(1) * x(1) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &le, const Vec &) const override {
        // Hess f = 4*I; Hess cE = 2*I (cE is itself a quadratic form).
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 4.0 + le(0) * 2.0;
        h.insert(1, 1) = obj_scale * 4.0 + le(0) * 2.0;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
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
        x << std::cos(theta_), std::sin(theta_);
        return x;
    }

  private:
    double theta_;
};

} // namespace

// THE BRIEF'S TEST (a). theta = 0.1, x0 = (cos theta, sin theta), EXACTLY on
// the circle, so h(x0) = 0 exactly.
//
// TRIAL 0 IS EXACT CLOSED-FORM ALGEBRA, because lambda_e starts at 0 (the
// driver's own convention), so the FIRST subproblem's Hessian is 4*I -- the
// pure objective Hessian, no cE contribution -- and f is itself EXACTLY
// quadratic, so a QP model built from f's own exact gradient/Hessian
// reproduces f's true value along ANY step: pred_df == actual_df identically
// on this one trial, for every theta. Writing p = t*(-sin theta, cos theta)
// (the tangent direction; Je.p = 0 needs exactly this form) and minimizing
// the model along it gives t* = -sin(theta)/4, i.e.
//     p0 = (sin^2(theta)/4, -sin(theta)cos(theta)/4),   pred_df0 = sin^2(theta)/8.
// Since pred_df == actual_df here, Eq. (11)'s Armijo test
// (actual_df >= sigma*pred_df, sigma = 1e-4) holds for ANY positive
// pred_df, so trial 0 is ALWAYS accepted -- an algebraic fact, not luck.
// Landing point x1 = x0 + p0 has, again exactly,
//     cE(x1) = sin^2(theta)/16   and   lambda_e (returned) = (cos(theta) - 4)/2,
// both verified against this fixture at theta = 0.1: cE(x1) = 6.229194e-4,
// lambda_e = -1.50250 (both machine-exact against the driver's own trial-0
// output; see the assertions below).
//
// TRIAL 1 IS WHERE MARATOS HAPPENS. lambda_e = -1.5025 makes the Hessian of
// the LAGRANGIAN (not of f alone) H1 = diag(4 + 2*lambda_e) = diag(0.99500),
// i.e. NEARLY THE IDENTITY -- the textbook second-order-sufficient
// configuration in which the reduced Hessian along the tangent direction is
// positive and the QP step is a genuinely good, superlinear DIRECTION. Yet
// because the constraint is curved, the actual step overshoots it: at
// EXACTLY H = I on the circle (the theoretical limit theta -> 0 this trial
// approaches), the identical tangent-direction algebra gives, in closed
// form, p = (sin^2 theta, -sin(theta)cos(theta)), for which
//     cE(x + p) = sin^2(theta)   (h RISES, from ~0 to O(theta^2))
//     f(x + p) - f(x) = sin^2(theta)   (f RISES too -- BOTH signatures at once).
// Trial 1's actual H1 (0.99500, not exactly 1) and actual x1 (not exactly on
// the circle) make the numbers below MEASURED rather than closed-form, but
// the mechanism -- and its sign -- is exactly this one: h_new > h_old AND
// f_new > f_old on a step whose own model promised descent.
TEST(SqpDriverSoc, SocDefeatsMaratos) {
    constexpr double kTheta = 0.1;
    MaratosModel model(kTheta);

    // --- WITH SOC (the default) -------------------------------------------
    SqpOptions opts_soc;
    opts_soc.kkt_tol = 1e-8;
    opts_soc.feas_tol = 1e-8;
    opts_soc.enable_soc = true; // the default; stated for the reader
    StepLog log;
    record_steps_into(opts_soc, &log);
    SqpDriver driver_soc(opts_soc);
    const SqpSolution sol_soc = driver_soc.solve(model);
    record_history(sol_soc, "maratos_soc_on");

    // Trial 0: closed-form exact values, asserted to the derivation above.
    // Tolerance 1e-14, not 1e-15: these compare a value computed by the
    // real driver (accumulated through several floating-point operations
    // in a different order) against a value computed here from closed-form
    // sin/cos calls, and 1e-15 is tight enough to be platform/libm-
    // sensitive (a different but equally valid rounding of sin/cos, or a
    // differently-ordered summation, can legitimately land one ULP further
    // out at this magnitude).
    ASSERT_GE(log.ctx.size(), 2u);
    EXPECT_EQ(log.verdict[0], StepVerdict::kAcceptF);
    EXPECT_NEAR(log.ctx[0].pred_df, std::sin(kTheta) * std::sin(kTheta) / 8.0, 1e-14);
    EXPECT_NEAR(log.ctx[0].f_old - log.ctx[0].f_new, log.ctx[0].pred_df, 1e-14)
        << "trial 0's QP model is EXACT for this quadratic f: actual == predicted";
    EXPECT_NEAR(log.ctx[0].h_new, std::sin(kTheta) * std::sin(kTheta) / 16.0, 1e-14);

    // Trial 1: THE MARATOS SIGNATURE, verified against the real driver
    // (measured, per the derivation above): both f and h get WORSE on the
    // raw QP step, yet SOC rescues it.
    EXPECT_EQ(log.verdict[1], StepVerdict::kReject);
    EXPECT_GT(log.ctx[1].h_new, log.ctx[1].h_old)
        << "the Maratos signature: constraint violation must INCREASE";
    EXPECT_GT(log.ctx[1].f_new, log.ctx[1].f_old)
        << "the Maratos signature: the objective must ALSO increase";
    // Same portability note as trial 0's block above: 1e-14, not 1e-15.
    EXPECT_NEAR(log.ctx[1].h_old, 0.0006229194424611991, 1e-14);
    EXPECT_NEAR(log.ctx[1].h_new, 0.005678149281314715, 1e-9);
    EXPECT_NEAR(log.ctx[1].f_new, -0.9914787438395734, 1e-9);
    ::testing::Test::RecordProperty("maratos_trial1_h_old",
                                    fmt::format("{:.10g}", log.ctx[1].h_old));
    ::testing::Test::RecordProperty("maratos_trial1_h_new",
                                    fmt::format("{:.10g}", log.ctx[1].h_new));

    // The RESCUE: a second judge() call, on the corrected point, reads
    // ACCEPT with h collapsed by ~600x and f BETTER than the pre-Maratos
    // iterate (not merely better than the raw step).
    ASSERT_GE(log.ctx.size(), 3u);
    EXPECT_TRUE(log.verdict[2] == StepVerdict::kAcceptF || log.verdict[2] == StepVerdict::kAcceptH);
    EXPECT_LT(log.ctx[2].h_new, log.ctx[1].h_old)
        << "the corrected point beats even the PRE-Maratos iterate's violation";
    EXPECT_NEAR(log.ctx[2].h_new, 9.822740831166854e-06, 1e-12);
    EXPECT_NEAR(log.ctx[2].f_new, -0.9999851947563363, 1e-9);

    // FIX ROUND 1, M4: pred_df is NOT recomputed from the SOC step -- it is
    // the ORIGINAL, REJECTED QP's pred_df, reused UNCHANGED (sqp_driver.h's
    // SECOND-ORDER CORRECTION note). Pinned as an EXACT equality between the
    // two judge() calls' own StepContext, not merely inferred from the
    // outcome: recomputing it from qs_soc.x would give a DIFFERENT number
    // (a different step, generally against a different pred_df), so this
    // assertion is what actually distinguishes the two implementations.
    EXPECT_DOUBLE_EQ(log.ctx[2].pred_df, log.ctx[1].pred_df)
        << "pred_df must be the ORIGINAL QP's, not recomputed from the SOC step";

    ASSERT_EQ(sol_soc.status, SqpStatus::kOptimal);
    EXPECT_LE(sol_soc.counters.major_iters, 6); // observed: 4
    EXPECT_EQ(sol_soc.counters.rejected_steps, 0)
        << "every trial ends accepted -- the SOC rescue on trial 1 is what makes this true";
    EXPECT_EQ(sol_soc.counters.soc_steps, 1);
    EXPECT_NEAR(sol_soc.f, -1.0, 1e-8);
    EXPECT_NEAR(sol_soc.x(0), 1.0, 1e-6);
    EXPECT_NEAR(sol_soc.x(1), 0.0, 1e-6);

    // THE HOT-START OBSERVATION (mirrors
    // SqpDriverTrustRegion.RejectionShrinksAndRetriesHotly's own pin, for the
    // SAME three conditions -- see sqp_driver.h's WARM START paragraph): the
    // SOC re-solve's OWN factorization cost is recoverable as the difference
    // between the aggregate and the per-row sum (sqp_types.h's own note on
    // why those can now differ), and it is asserted as the OBSERVED value
    // (0), not merely claimed in a comment.
    Index row_factorizations = 0;
    for (const SqpIterate &h : sol_soc.history) {
        row_factorizations += h.qp_factorizations;
    }
    const Index soc_own_factorizations = sol_soc.counters.factorizations - row_factorizations;
    EXPECT_EQ(soc_own_factorizations, 0)
        << "the SOC re-solve's own K0 should be reused: H/Ae/Ai unchanged, "
           "(primal_delta, dual_mu) unchanged, seed == the rejected solve's own exit working set";

    // "UNIT STEPS ACCEPTED": the radius never had to move -- neither shrunk
    // (no un-rescued reject) nor grown (no step ever bound the radius).
    for (const SqpIterate &h : sol_soc.history) {
        EXPECT_DOUBLE_EQ(h.tr_radius, opts_soc.tr_init) << "trial " << h.trial;
    }

    // The accepted row that recorded the correction.
    bool soc_row_found = false;
    for (const SqpIterate &h : sol_soc.history) {
        if (h.soc_applied) {
            soc_row_found = true;
            EXPECT_NE(h.verdict, StepVerdict::kReject);
        }
    }
    EXPECT_TRUE(soc_row_found);

    // --- WITHOUT SOC: strictly more majors. THE DELTA IS THE TEST. --------
    SqpOptions opts_off = opts_soc;
    opts_off.enable_soc = false;
    opts_off.make_strategy = nullptr; // fresh, unlogged strategy for this solve
    SqpDriver driver_off(opts_off);
    const SqpSolution sol_off = driver_off.solve(model);
    record_history(sol_off, "maratos_soc_off");

    ASSERT_EQ(sol_off.status, SqpStatus::kOptimal);
    EXPECT_EQ(sol_off.counters.soc_steps, 0);
    EXPECT_GT(sol_off.counters.rejected_steps, 0)
        << "with SOC off, trial 1's Maratos reject is never rescued";
    EXPECT_NEAR(sol_off.f, -1.0, 1e-8);
    EXPECT_GT(sol_off.counters.major_iters, sol_soc.counters.major_iters)
        << "observed: " << sol_off.counters.major_iters << " (off) vs "
        << sol_soc.counters.major_iters << " (on)";
    ::testing::Test::RecordProperty("maratos_majors_soc_on",
                                    static_cast<int>(sol_soc.counters.major_iters));
    ::testing::Test::RecordProperty("maratos_majors_soc_off",
                                    static_cast<int>(sol_off.counters.major_iters));
}

namespace {

// A REJECTED TRIAL WHOSE VIOLATION DECREASES. Reuses FlatQuarticModel's own
// x0-dynamics UNCHANGED (see that class and
// SqpDriverTrustRegion.RejectionShrinksAndRetriesHotly for the hand-derived
// trajectory: Delta = 2 -> 1 -> 0.5, the first two REJECTED on Eq. (11), the
// third ACCEPTED) and adds ONE LINEAR equality purely in x1, decoupled from
// x0 (the added Hessian block is IDENTICALLY ZERO: cE is affine, so
// eval_hess is untouched and x0's algebra is bit-for-bit what
// RejectionShrinksAndRetriesHotly already pins).
//
// BECAUSE cE IS LINEAR, THE LINEARIZATION IS EXACT: Je is a CONSTANT row, so
// cE(x + p) = cE(x) + Je.p identically, with no second-order term at all.
// The equality row is solved EXACTLY by the engine on every trial (an
// equality is a hard constraint), so h_new = 0 EXACTLY whenever the box
// admits |p1| = |x1_start - 2|, which it does at every radius used here
// (0.5, 1, 2) for x1_start = 1.7 (|p1| = 0.3 <= 0.5, the smallest radius
// tried). So h_new = 0 < h_old = 0.3 on EVERY trial at this iterate,
// including the two REJECTED ones -- SOC's gate (h_new > h_old) must stay
// closed on both, regardless of why they were rejected.
class QuarticWithLinearEqualityModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    // x1's OWN objective term is centered at the constraint's OWN target
    // (2.0), not at 0: 0.5*(x1 - 2)^2, so moving x1 toward the target the
    // equality wants COSTS the model NOTHING extra (its own unconstrained
    // minimizer already sits there) -- x1's contribution to pred_df is then
    // POSITIVE, keeping trial 0's overall classification f-type (see the
    // derivation above the test) rather than accidentally flipping it
    // h-type through an unrelated, uncontrolled x1 cost.
    double eval_f(const Vec &x) const override {
        return 0.25 * std::pow(x(0), 4) - x(0) + 0.5 * (x(1) - 2.0) * (x(1) - 2.0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << x(0) * x(0) * x(0) - 1.0, x(1) - 2.0;
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(1) - 2.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        // cE(x) = x1 - 2 is AFFINE: its Hessian contribution is exactly
        // zero, so H is UNCHANGED from FlatQuarticModel's regardless of
        // lambda_e.
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 3.0 * x(0) * x(0);
        h.insert(1, 1) = obj_scale * 1.0;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
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
        x << 0.5, 1.7; // h0 = |1.7 - 2| = 0.3
        return x;
    }
};

} // namespace

// THE BRIEF'S TEST (b).
TEST(SqpDriverSoc, SocIsNotAttemptedWhenViolationDecreased) {
    QuarticWithLinearEqualityModel model;
    SqpOptions opts;
    opts.tr_init = 2.0; // matches RejectionShrinksAndRetriesHotly's x0 dynamics
    opts.max_iter = 20;
    opts.enable_soc = true; // explicit: the point is that it still does not fire
    StepLog log;
    record_steps_into(opts, &log);
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);
    record_history(sol, "quartic_linear_eq");

    ASSERT_GE(log.ctx.size(), 2u);
    // Trials 0 and 1 (Delta = 2, then 1) are REJECTED -- x0's Armijo failure,
    // unchanged from RejectionShrinksAndRetriesHotly -- but h DECREASES TO
    // EXACTLY 0 on both, per the derivation above.
    EXPECT_EQ(log.verdict[0], StepVerdict::kReject);
    EXPECT_DOUBLE_EQ(log.ctx[0].h_old, 0.3);
    EXPECT_DOUBLE_EQ(log.ctx[0].h_new, 0.0);
    EXPECT_LE(log.ctx[0].h_new, log.ctx[0].h_old);

    EXPECT_EQ(log.verdict[1], StepVerdict::kReject);
    EXPECT_DOUBLE_EQ(log.ctx[1].h_old, 0.3);
    EXPECT_DOUBLE_EQ(log.ctx[1].h_new, 0.0);
    EXPECT_LE(log.ctx[1].h_new, log.ctx[1].h_old);

    EXPECT_GE(sol.counters.rejected_steps, 2);
    EXPECT_EQ(sol.counters.soc_steps, 0)
        << "h_new <= h_old on every reject here -- SOC's gate (h_new > h_old) "
           "must stay closed regardless of why the trial was rejected";

    // The rest of the trajectory is unaffected: it still converges (x0's own
    // quartic story is untouched, and x1 is pinned to 2 from trial 0 on).
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), 1.0, 1e-9);
    EXPECT_NEAR(sol.x(1), 2.0, 1e-12);
}

// THE BRIEF'S TEST (c). A routed QP failure (Task 6b's SUBPROBLEM FAILURE
// ROUTING) never even reaches the code that could attempt SOC: that branch
// returns/continues before `verdict` is ever assigned from judge(), so the
// gate `verdict == StepVerdict::kReject` is structurally unreachable there,
// not merely false. Both routed statuses are exercised, reusing Task 6b's
// own fixtures verbatim with opts.enable_soc left ON (the default) to prove
// the skip is not an artifact of the option being off.
TEST(SqpDriverSoc, SocIsSkippedOnRoutedFailures) {
    { // kNumericalError, routed (TwinSaddleModel, exactly
      // QpNumericalErrorShrinksInsteadOfAborting's setup).
        TwinSaddleModel model(2);
        SqpOptions opts;
        opts.tr_init = 3.0;
        opts.tr_max = 100.0;
        opts.max_iter = 40;
        opts.enable_soc = true;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);

        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        ASSERT_EQ(sol.history.size(), 4u);
        EXPECT_EQ(sol.history[0].qp_status, QpStatus::kNumericalError);
        EXPECT_EQ(sol.history[0].verdict, StepVerdict::kReject);
        EXPECT_FALSE(sol.history[0].soc_applied);
        EXPECT_EQ(sol.counters.rejected_steps, 1);
        EXPECT_EQ(sol.counters.soc_steps, 0)
            << "a routed kNumericalError row never reaches judge(), so SOC's "
               "gate is unreachable, not merely closed";
    }
    { // kMaxIter, routed (HS6 with a tight QP iteration cap, exactly
      // QpMaxIterTakesTheSameShrinkRetryPath's setup).
        const HsProblem hs = make_hs(6);
        SqpOptions opts;
        opts.max_iter = 60;
        opts.qp.max_iter = 3;
        opts.enable_soc = true;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*hs.model);

        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        int capped_rows = 0;
        for (const SqpIterate &h : sol.history) {
            if (h.qp_solved && h.qp_status == QpStatus::kMaxIter) {
                ++capped_rows;
                EXPECT_EQ(h.verdict, StepVerdict::kReject);
                EXPECT_FALSE(h.soc_applied);
            }
        }
        EXPECT_EQ(capped_rows, 1);
        EXPECT_EQ(sol.counters.soc_steps, 0)
            << "a routed kMaxIter row never reaches judge() either";
    }
}

namespace {

// FIX ROUND 1, I1's INTEGRATION HALF: a real NLP, run through the real
// driver, in which SOC's INEQUALITY branch fires on a genuinely CURVED,
// ACTIVE row -- complementing BuildSocSubproblemShiftsRhsOnActiveRowsOnly's
// isolated formula test with an end-to-end demonstration.
//
// x0 REUSES FlatQuarticModel's OWN dynamics verbatim (0.25 x0^4 - x0,
// decoupled Hessian block, no coupling into any constraint row) -- the
// KNOWN, hand-derived trajectory from RejectionShrinksAndRetriesHotly:
// Delta = 2 -> p0 = 7/6 (interior), f RISES by 0.746721, REJECTED on
// Eq. (11); the QUARTIC's own nonlinearity does this, with no constraint
// or multiplier evolution involved.
//
// x1 IS THE CURVED, ACTIVE INEQUALITY: cI0(x) = x1^2 - 4 <= 0 (must stay in
// [-2, 2]), engaged from the INTERIOR (x1_start = 1.8) by a small linear
// objective pull (-0.1 x1, wanting x1 as large as possible) plus a tiny
// quadratic regularizer (0.0005 x1^2, keeping H well-posed at
// lambda_i = 0). cI1(x) = x0 - 10 <= 0 is pure decoration: x0 stays near
// 0.5-1.7 throughout, nowhere near 10.
//
// THE 1-D "LINEARIZATION OVERSHOOTS A CONVEX CURVE" MECHANISM (no 2-D
// tangent freedom needed, unlike the circle): starting STRICTLY INSIDE
// (x1_start = 1.8, cI0(x1_start) = -0.76 < 0) with an objective that wants
// x1 to increase, the QP pushes p1 out to EXACTLY where the LINEARIZED row
// binds, x1_lin = (x1_start^2 + 4) / (2 x1_start). Substituting shows the
// TRUE value there is cI0(x1_lin) = (x1_start^2 - 4)^2 / (4 x1_start^2) >= 0
// ALWAYS (a perfect square) -- the linearization overshoots the true curve
// by an amount that is a SECOND-ORDER, curvature-driven, ALWAYS-POSITIVE
// quantity, exactly SOC's target. At x1_start = 1.8: h_new (from cI0 alone)
// = (3.24-4)^2/(4*3.24) = 0.5776/12.96 = 0.044567901... -- HAND-VERIFIED
// against the driver's own StepContext below.
//
// h_old = 0 (x1_start is FEASIBLE, x0 has no constraint of its own), so
// this row alone contributes the WHOLE Maratos signature (h_new > h_old)
// on the SAME trial x0's quartic already rejects on Eq. (11) grounds --
// two INDEPENDENT defects landing on one trial, by construction.
//
// WHY THE OVERALL VERDICT STAYS REJECTED EVEN AFTER SOC: x0 and x1 are
// decoupled (neither's Jacobian/Hessian touches the other), so the SOC
// re-solve corrects x1's row EXACTLY (h collapses to 0 -- see the
// assertion) while leaving x0's OWN step untouched (no row of either
// active constraint has a nonzero x0 entry). x0's quartic overshoot is an
// OBJECTIVE nonlinearity, not a CONSTRAINT one, and SOC only ever repairs
// the latter (see sqp_driver.h's SECOND-ORDER CORRECTION note) -- so the
// corrected point's f is STILL worse than f_old by (approximately) the
// same amount x0 alone would cost, and Armijo still fails. This is the
// HONEST, EXPECTED outcome, not a defect: SOC's job here is to demonstrate
// it repairs EXACTLY the curvature it targets, not to rescue every
// rejection regardless of cause.
class CurvedActiveInequalityModel : public NlpModel {
  public:
    static constexpr double kPull = 0.1;
    static constexpr double kReg = 0.0005;
    static constexpr double kBound = 4.0; // x1 in [-2, 2]

    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 2; }
    double eval_f(const Vec &x) const override {
        return 0.25 * std::pow(x(0), 4) - x(0) - kPull * x(1) + 0.5 * kReg * x(1) * x(1);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << x(0) * x(0) * x(0) - 1.0, -kPull + kReg * x(1);
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(2);
        c << x(1) * x(1) - kBound, x(0) - 10.0;
        return c;
    }
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &li) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 3.0 * x(0) * x(0);
        h.insert(1, 1) = obj_scale * kReg + li(0) * 2.0;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(2, 2);
        j.insert(0, 1) = 2.0 * x(1);
        j.insert(1, 0) = 1.0;
        j.makeCompressed();
        return j;
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
        x << 0.5, 1.8;
        return x;
    }
};

} // namespace

TEST(SqpDriverSoc, SocFiresOnACurvedActiveInequalityAndImprovesH) {
    CurvedActiveInequalityModel model;
    SqpOptions opts;
    opts.tr_init = 2.0; // x0's own known trajectory needs this radius
    opts.max_iter = 20;
    opts.enable_soc = true;
    StepLog log;
    record_steps_into(opts, &log);
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);
    record_history(sol, "curved_ineq_soc");

    // Trial 0's RAW step: the Maratos signature, from x1's curved row alone
    // (h_old = 0, since x0 contributes nothing and x1 starts feasible).
    ASSERT_GE(log.ctx.size(), 2u);
    EXPECT_EQ(log.verdict[0], StepVerdict::kReject);
    EXPECT_DOUBLE_EQ(log.ctx[0].h_old, 0.0);
    EXPECT_NEAR(log.ctx[0].h_new, 0.044567901234567664, 1e-12);

    // SOC's rescue attempt: the very next judge() call is the CORRECTED
    // point, and its h has collapsed to (numerically) EXACTLY 0 -- cI0 is a
    // pure quadratic, so the frozen-Jacobian correction is EXACT for it,
    // just as build_subproblem's model is exact for a quadratic OBJECTIVE
    // (carry 1's own point, applied here to a constraint instead).
    EXPECT_LT(log.ctx[1].h_new, log.ctx[0].h_new)
        << "SOC's corrected point must be LESS infeasible than the raw rejected one";
    EXPECT_NEAR(log.ctx[1].h_new, 0.0, 1e-9);

    // THE HONEST LIMIT: the overall trial stays REJECTED. SOC repaired
    // EXACTLY the constraint curvature it targets (x1's row) and nothing
    // else; x0's own quartic overshoot is a SEPARATE, objective-side defect
    // that no constraint correction can fix. Documented, not a bug -- see
    // the class comment.
    EXPECT_EQ(log.verdict[1], StepVerdict::kReject);

    EXPECT_GE(sol.counters.soc_steps, 1);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), 1.0, 1e-6);
    EXPECT_NEAR(sol.x(1), 2.0, 1e-6);
}

// FIX ROUND 1, M5: the multipliers a SOC ACCEPTANCE carries out are the
// RE-SOLVE'S OWN (qs_soc.lambda_e/lambda_i), not the ORIGINAL (rejected)
// solve's -- sqp_driver.h's ACCEPT vs REJECT paragraph says so; this pins
// it against a concrete number rather than leaving it asserted only by
// construction.
//
// kkt_tol/feas_tol are loosened to 5e-4 (looser than trial 2's own
// kkt_residual of 0.0003771519593, measured) so the solve CONVERGES at the
// very next major's convergence check, immediately after the SOC rescue,
// with NO further subproblem solved -- i.e. `sol.lambda_e` is EXACTLY what
// the SOC re-solve returned, untouched by any later major. Observed:
// majors == 2 (trial 0 accept, trial 1 SOC-rescued accept, trial 2 is the
// converged terminal row -- no subproblem built there), lambda_e =
// -1.4999961431933102, matching the hand-derived lambda_e ~= -1.5 this
// file's own SocDefeatsMaratos comment predicts H1 needs to reshape to
// ~= I (4 + 2*(-1.5) = 1).
TEST(SqpDriverSoc, SocAcceptedMultipliersComeFromTheReSolve) {
    constexpr double kTheta = 0.1;
    MaratosModel model(kTheta);
    SqpOptions opts;
    opts.kkt_tol = 5e-4;
    opts.feas_tol = 5e-4;
    opts.enable_soc = true;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_EQ(sol.counters.major_iters, 2);
    ASSERT_EQ(sol.history.size(), 3u);
    EXPECT_TRUE(sol.history[1].soc_applied);
    EXPECT_FALSE(sol.history[2].qp_solved) << "the terminal row builds no subproblem";

    ASSERT_EQ(sol.lambda_e.size(), 1);
    EXPECT_NEAR(sol.lambda_e(0), -1.4999961431933102, 1e-9);
    EXPECT_NEAR(sol.f, -0.999985194756336, 1e-9);
}

// FIX ROUND 1, M2: extends SqpDriverContract.CallCountPerMajorIsBounded's
// own n_hess == accepted identity to a SOC-FIRING fixture. That identity
// would MISS a mutation that made the SOC path quietly rebuild the
// subproblem (an extra eval_hess), because none of that test's own three
// fixtures ever fires SOC. This one does (soc_steps == 1, verified), and
// n_hess == accepted holds here too: build_soc_subproblem never calls
// eval_hess (sqp_driver.h's SECOND-ORDER CORRECTION note). n_f/n_ce gain
// exactly ONE extra call beyond `rows` -- the ONE SOC re-solve that reached
// kOptimal paid one extra values-only fetch at the corrected point (MODEL
// EVALUATION's TASK 7 ADDS ONE MORE paragraph) -- so the original
// CallCountPerMajorIsBounded identity `total == per_row*rows + accepted`
// does NOT hold unmodified here; this test states the SOC-aware version
// instead of silently reusing the old one.
//
// TASK 8 (the eval-economics carry) CHANGES n_grad/n_jac_e's OWN IDENTITY,
// and this is the fixture that catches it: n_grad/n_jac_e are NOT rows + 1
// any more, because globalization.h's judge() reads f/h only, so a REJECTED
// trial's gradient/Jacobian is never fetched, whether or not SOC was
// attempted from it -- only the loop-top evaluation and each ACCEPTED trial
// (direct, or SOC-promoted, exactly this fixture's rescue) pay for one, via
// sqp_driver.h's UPGRADE TO FULL sites. The SOC re-solve's "+1" values-only
// fetch above IS one such trial here (the rescue's whole point is that it
// gets PROMOTED), so it is already counted inside `accepted`, not on top of
// it -- unlike n_f/n_ce, whose "+1" is unconditional (every values-only
// fetch reads f/cE regardless of verdict).
TEST(SqpDriverSoc, SocRescuePaysNoExtraHessian) {
    CountingModel model(std::make_unique<MaratosModel>(0.1));
    SqpOptions opts;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_EQ(sol.counters.soc_steps, 1);
    ASSERT_EQ(sol.counters.soc_applied, 1) << "this fixture's rescue must actually be promoted, "
                                              "or the n_grad/n_jac_e identity below proves nothing";

    const Index rows = static_cast<Index>(sol.history.size());
    const Index accepted = sol.counters.steps_accepted;

    // THE + 1 ON THE TWO DERIVATIVE COUNTS IS THE BRIDGE'S CLAIM PASS, and it
    // is emphatically NOT the thing this test guards. One solve(model, ...)
    // call builds one NlpModelAggregate, and laying one walks eval_hess and --
    // because MaratosModel declares equality rows -- eval_jac_e once each at
    // the start point (include/hven/model/nlp_model_aggregate.h's constructor
    // doc). What is still being asserted is that SOC adds NOTHING on top: a
    // SOC path that rebuilt the subproblem would read accepted + 2 here.
    EXPECT_EQ(model.n_hess, accepted + 1) << "SOC must add NO extra Hessian evaluation";
    EXPECT_EQ(model.n_f, rows + 1) << "the one successful SOC re-solve pays one extra values fetch";
    EXPECT_EQ(model.n_ce, rows + 1);
    EXPECT_EQ(model.n_grad, 1 + accepted);
    EXPECT_EQ(model.n_jac_e, 1 + accepted + 1);
    // NO LAY TERM ON THE INEQUALITY SIDE, and that is the claim pass's own
    // row-gating showing through rather than an omission: a model with mi() ==
    // 0 has its eval_jac_i skipped by the lay exactly as the solve skips it.
    EXPECT_EQ(model.n_ci, 0) << "MaratosModel has no inequalities";
    EXPECT_EQ(model.n_jac_i, 0);
}

// FIX ROUND 1, M3: WHY seed_soc.x.setZero() IS NOT OPTIONAL, isolated at the
// ENGINE level exactly the way qp_engine_tr.cpp's
// QpEngineTr.SeededOppositeBoundPinDoesNotRecenterTheWindow isolates the
// IDENTICAL bug pattern for the plain rejection-retry path (that test's own
// `seed.x.setZero(); // the SQP driver's TR-centering discipline` comment is
// this test's whole point, applied to the SOC re-solve instead).
//
// H = I, g = (-100, -100) (same shape as that test): the unconstrained
// direction is (100, 100), so a SMALL radius box is what actually decides
// the step -- exactly the regime a POST-rejection SOC re-solve is in (a
// small correction against a box the ORIGINAL, larger step already
// explored). Solved once with NO radius override to reach the real bound
// (6, 6) -- this stands in for "the rejected solve's own step, qs.x" -- then
// re-solved WARM at radius 1.0 two ways:
//   CORRECT (seed.x zeroed, i.e. what build_soc_subproblem's caller does):
//     the window is [-1, 1] about p = 0, so the step is the radius itself,
//     (1, 1).
//   MUTANT (seed.x = the far solve's own x, i.e. deleting
//     `seed_soc.x.setZero()`): the window RE-CENTERS on (6, 6), so the
//     warm solve reports (6, 6) again, bit-identical to the far solve, AT
//     EVERY RADIUS -- the SOC re-solve would silently ignore its own radius
//     override entirely, exactly the corruption Task 6's C1 fix eliminated
//     for the plain retry path.
TEST(SqpDriverSoc, SocSeedMustBeZeroedNotToRecenterTheWindow) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -100.0, -100.0;
    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.be = Vec(0);
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 6.0);
    ASSERT_NO_THROW(qp.validate());

    QpEngine engine{QpOptions{}};
    const QpSolution far = engine.solve(qp); // stands in for the rejected solve's qs
    ASSERT_EQ(far.status, QpStatus::kOptimal);
    EXPECT_NEAR(far.x(0), 6.0, 1e-9);
    EXPECT_NEAR(far.x(1), 6.0, 1e-9);

    SolveOverrides overrides;
    overrides.tr_radius = 1.0;

    // CORRECT: mirrors sqp_driver.h's `seed_soc = qs; seed_soc.x.setZero();`.
    QpSolution seed_correct = far;
    seed_correct.x.setZero();
    const QpSolution correct = engine.solve(qp, seed_correct, overrides);
    ASSERT_EQ(correct.status, QpStatus::kOptimal);
    EXPECT_NEAR(correct.x(0), 1.0, 1e-8);
    EXPECT_NEAR(correct.x(1), 1.0, 1e-8);

    // MUTANT: seed_soc.x left as the "rejected step" (far.x) -- the bug
    // `seed_soc.x.setZero()` exists to prevent.
    QpSolution seed_mutant = far; // seed_mutant.x == far.x, NOT zeroed
    const QpSolution mutant = engine.solve(qp, seed_mutant, overrides);
    ASSERT_EQ(mutant.status, QpStatus::kOptimal);
    EXPECT_NEAR(mutant.x(0), 6.0, 1e-8) << "the window silently re-centered on the unzeroed seed";
    EXPECT_NEAR(mutant.x(1), 6.0, 1e-8);

    // The two behave IDENTICALLY only because the mutant ignores the radius
    // override entirely -- the defining symptom.
    EXPECT_GT(std::abs(mutant.x(0) - correct.x(0)), 1.0);
}

// PHASE-4, TASK 1: THE SOC OUTCOME BREAKDOWN. The HS battery's kappa_soc
// adjudication (docs/notes/2026-07-29-hs-battery-results.md, item 2) measured
// SOC attempted exactly twice across all 27 HS problems -- both on HS77,
// neither applied -- but could not say WHY the two attempts were not applied
// (a failed re-solve vs. a corrected point the funnel still rejected),
// because soc_steps counts attempts only. soc_applied/soc_qp_infeasible/
// soc_rejected (sqp_types.h's SqpCounters) close that gap; this test pins the
// battery's own HS77 observation (2 attempts, 0 applied) through the new
// fields and checks the identity that must hold on every solve.
TEST(SqpDriverSoc, SocOutcomeCountersPartitionAttemptsOnHs77) {
    const HsProblem p = make_hs(77);
    SqpOptions opts;
    opts.max_iter = 60; // matches the HS battery's border_table() entry for HS77
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_EQ(sol.counters.soc_steps, 2) << "the battery's own pinned attempt count";
    EXPECT_EQ(sol.counters.soc_applied, 0) << "the battery's own pinned applied count";

    // THE IDENTITY: every attempt lands in exactly one of the three buckets.
    EXPECT_EQ(sol.counters.soc_steps, sol.counters.soc_applied + sol.counters.soc_qp_infeasible +
                                          sol.counters.soc_rejected);

    // AND THE FULL BREAKDOWN, not just the sum -- the WHY the battery's own
    // adjudication note said was unrecoverable. Recorded on the gtest XML so a
    // future re-run's numbers are visible without re-deriving them by hand.
    ::testing::Test::RecordProperty(
        "soc_hs77_breakdown",
        fmt::format("attempts={} applied={} qp_infeasible={} rejected={}", sol.counters.soc_steps,
                    sol.counters.soc_applied, sol.counters.soc_qp_infeasible,
                    sol.counters.soc_rejected));
}

// =========================================================================
// SqpDriverElastic -- Task 8: the elastic tier.
//
// A QP that returns kInfeasible no longer ends the solve. The SAME
// subproblem is reformulated with penalized slacks on its violated
// linearized rows and re-solved (sqp_driver.h's ELASTIC TIER note); the
// resulting step goes through the ordinary funnel judgment, and only an
// EXHAUSTED elastic tier -- rho at its ceiling with the relaxation still
// materially open and nothing to offer -- ends the solve, now through the
// funnel's own kRestore route rather than a raw QP status.
// =========================================================================

namespace {

// THE INCONSISTENT-LINEARIZATION FIXTURE (Task 8's test (a)), with the whole
// algebra in this comment. Task 6's HS7-from-the-published-start is NOT this
// fixture -- it converges by growing the radius (Hs7AtTheDefaultRadiusNow-
// ConvergesByGrowingIt) and never produces an inconsistent linearization at
// all -- so the pressure has to be constructed.
//
//     min  f(x) = (x0 - 2)^2 + x1^2
//     s.t. c1(x) =     x1 - x0^2 <= 0        (x1 <= x0^2)
//          c2(x) = 1 - x1 - x0^2 <= 0        (x1 >= 1 - x0^2)
//          -5 <= x <= 5,      x_start = (0, 0.5)
//
// (1) THE NLP IS FEASIBLE, and its solution is INTERIOR to both constraints.
//     Feasible set: 1 - x0^2 <= x1 <= x0^2, which is nonempty exactly when
//     x0^2 >= 1/2. EXHIBITED FEASIBLE POINT: x = (2, 0) -- c1 = 0 - 4 = -4 <= 0,
//     c2 = 1 - 0 - 4 = -3 <= 0, both STRICTLY satisfied -- and it is the
//     unconstrained minimizer of f, so it is the global solution with
//     f* = 0, lambda_i = (0, 0) and strict complementarity (no degeneracy,
//     no weakly active row).
//
// (2) THE LINEARIZATION AT x_start IS INCONSISTENT, by direct contradiction.
//     grad c1 = (-2 x0, 1) = (0, 1) and grad c2 = (-2 x0, -1) = (0, -1) at
//     x0 = 0 -- exactly ANTIPARALLEL, which is what Gordan/Farkas says is
//     needed for two linear inequalities in R^2 to conflict at all (two rows
//     with independent normals are always jointly satisfiable). With
//     c1 = 0.5 - 0 = 0.5 and c2 = 1 - 0.5 - 0 = 0.5, the subproblem's rows
//     (Ai p <= -cI) read
//              p1 <= -0.5      and     -p1 <= -0.5,   i.e.  p1 >= 0.5.
//     No p satisfies both, at ANY radius and inside ANY box.
//     FARKAS CERTIFICATE: y = (1, 1) >= 0 has y^T Ai = (0, 0) and
//     y^T bi = -1 < 0, so {p : Ai p <= bi} = {} -- the standard certificate
//     of an infeasible linear system.
//
// (3) WHAT THE ELASTIC TIER DOES WITH IT. Both rows are violated at p = 0,
//     so both get a slack: p1 - s1 <= -0.5, -p1 - s2 <= -0.5, s >= 0, and
//     ADDING the two shows s1 + s2 >= 1 IDENTICALLY -- no p in any box
//     reduces the LINEARIZED violation below 1, at any rho. That is exactly
//     why this fixture is the interesting one: the model's first-order view
//     is a dead end, and the elastic step is nevertheless the right thing to
//     take, because the TRUE constraint curvature (the x0^2 terms the
//     linearization drops) opens the feasible band as soon as x0 moves.
//     Hand-derived elastic solution at x_start with Delta = 1 (H = 2I since
//     lambda = 0, g = grad f = (-4, 1)):
//       p1: for p1 in [-0.5, 0.5] the penalty term is the CONSTANT rho*1, so
//           the objective is p1 + p1^2, minimized at p1 = -0.5 (and outside
//           that interval the (1 - rho) p1 slope points back into it), giving
//           s1 = 0, s2 = 1;
//       p0: -4 p0 + p0^2 is decreasing on [-1, 1], so p0 = 1, at the radius.
//     Step p = (1, -0.5) -> x_trial = (1, 0), where c1 = -1 and c2 = 0:
//     TRUE h drops from 1 to 0 in one step. pred_df = -(-4.5 + 1.25) = 3.25,
//     so Eq. (10) (3.25 >= 0.999 * h_old^2 = 0.999) classifies it f-TYPE and
//     Eq. (11) accepts it (actual_df = 4.25 - 1 = 3.25).
//     From (1, 0) the linearization is consistent (p = 0 satisfies both rows)
//     and the plain Newton step p = (1, 0) lands exactly on x* = (2, 0).
//
// THE CONSTRAINT SCALE IS A PARAMETER (fix round 1). Multiplying both cI rows
// by S changes neither the feasible set nor the solution -- it multiplies the
// multipliers by 1/S and the linearized RESIDUALS, hence the SLACKS the
// elastic tier needs, by S. That is the one axis along which the tier's own
// arithmetic has a ceiling, so the fixture carries the knob rather than a
// second near-copy of the model. See
// SqpDriverElastic.ElasticSurvivesALargeConstraintScale.
//
// REPRODUCED FROM tests/test_sqp_restoration.cpp (Phase-5 Task 10, for
// SqpDriverElastic.EarlyExitOnADegenerateRelaxationStillCertifiesCorrectly),
// rather than shared, to keep this file's own fixture set self-contained --
// same model, same constants, byte-for-byte. min x0 + x1 s.t.
// x0^2 + x1^2 - 1 = 0, x0 + x1 - 2 = 0 (provably infeasible: the circle's
// farthest point toward the line is still short of it). At (2, 2) -- and at
// (1, 1), which a step from (1, 0) reaches -- grad cE1 = (2x0, 2x0) is
// PARALLEL to grad cE2 = (1, 1), so the linearization is inconsistent there
// and the elastic tier is the first mechanism to fire; f and cE2 are linear,
// so the whole Lagrangian Hessian is 2*lambda_e(0)*I, which is IDENTICALLY
// ZERO whenever the elastic relaxation leaves lambda_e cleared (see
// sqp_driver.h's MULTIPLIERS ARE NOT CARRIED OUT OF AN OPEN RELAXATION) --
// the degenerate-LP mechanism docs/notes/2026-07-31-schur-cap-policy.md
// section 4 derives and this task's STALL EARLY-EXIT note relies on.
constexpr double kCircleLineInfBound = 1e20;
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
        static const Vec l = Vec::Constant(2, -kCircleLineInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kCircleLineInfBound);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << s0_, s1_;
        return x;
    }

  private:
    double s0_, s1_;
};

class InconsistentLinearizationModel : public NlpModel {
  public:
    explicit InconsistentLinearizationModel(double scale = 1.0) : scale_(scale) {}

    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 2; }

    double eval_f(const Vec &x) const override {
        const double a = x(0) - 2.0;
        return a * a + x(1) * x(1);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 2.0 * (x(0) - 2.0), 2.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(2);
        c << scale_ * (x(1) - x(0) * x(0)), scale_ * (1.0 - x(1) - x(0) * x(0));
        return c;
    }
    // grad^2 f = diag(2, 2); grad^2 c1 = grad^2 c2 = S * diag(-2, 0).
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &li) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * obj_scale - 2.0 * scale_ * (li(0) + li(1));
        h.insert(1, 1) = 2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(2, 2);
        j.insert(0, 0) = -2.0 * scale_ * x(0);
        j.insert(0, 1) = scale_;
        j.insert(1, 0) = -2.0 * scale_ * x(0);
        j.insert(1, 1) = -scale_;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -5.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 5.0);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 0.0, 0.5;
        return x;
    }

  private:
    double scale_;
};

} // namespace

// THE CONSTRUCTION ITSELF, unit-tested away from any engine and any NLP --
// the same reason qp_failure_is_retryable and build_soc_subproblem are free
// functions. Every number below is hand-derived from the QP in the comment.
//
//   n = 2, me = 2, mi = 2, H = diag(2, 4), g = (1, -1),
//   Ae = [[1 0], [0 1]], be = (3, -2);  Ai = [[1 0], [0 1]], bi = (-0.5, 2);
//   box [-1, 1]^2, radius 0.5.
//
// At p_ref = clamp(0, lower, upper) = 0:
//   equality row 0: residual be - Ae*0 = +3  -> RELAXED, coefficient sign +1
//   equality row 1: residual be - Ae*0 = -2  -> RELAXED, coefficient sign -1
//   inequality row 0: residual Ai*0 - bi = +0.5 > 0  -> RELAXED (violated)
//   inequality row 1: residual Ai*0 - bi = -2 <= 0   -> NOT relaxed
// so ns = 3, the slack columns are 2, 3 (the equalities) and 4 (inequality
// row 0), and violation_l1 = 3 + 2 + 0.5 = 5.5 -- which is exactly the
// VIOLATION vector of the FEASIBLE-BY-CONSTRUCTION point.
//
// THE COLUMNS ARE RESIDUAL-SCALED (fix round 1): sigma_j = max(1, |r_j|) =
// (3, 2, 1) here, the coefficient is +/-sigma_j rather than +/-1, the penalty
// entry is rho*sigma_j, and the slack VARIABLE is the row's violation in
// units of sigma_j -- so the witness sits at s = (1, 1, 0.5) and its
// violations are (3, 2, 0.5) again. sigma_j = max(1, .) never scales a column
// UP: a residual below 1 would shrink the column entry and inflate the
// variable, trading the ceiling this fixes for the opposite conditioning
// problem. See SqpDriverElastic.ElasticSurvivesALargeConstraintScale for the
// ceiling itself.
//
// THE TWO EQUALITY ROWS CARRY OPPOSITE-SIGNED RESIDUALS ON PURPOSE. A signed
// slack has to point at the side the row is ALREADY violated on, and with a
// single positive-residual row (which is all the driver-level fixtures in
// this file happen to produce) a hard-coded coefficient sign is
// observationally identical to sign(residual). Row 1 is what makes the sign
// load-bearing.
TEST(SqpDriverElastic, ElasticConstructionRelaxesOnlyViolatedRows) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 4.0;
    qp.H.makeCompressed();
    qp.g = Vec(2);
    qp.g << 1.0, -1.0;
    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(2, 2);
    qp.Ae.insert(0, 0) = 1.0;
    qp.Ae.insert(1, 1) = 1.0;
    qp.Ae.makeCompressed();
    qp.be = Vec(2);
    qp.be << 3.0, -2.0;
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(2, 2);
    qp.Ai.insert(0, 0) = 1.0;
    qp.Ai.insert(1, 1) = 1.0;
    qp.Ai.makeCompressed();
    qp.bi = Vec(2);
    qp.bi << -0.5, 2.0;
    qp.lower = Vec::Constant(2, -1.0);
    qp.upper = Vec::Constant(2, 1.0);

    ElasticQp e = build_elastic_subproblem(qp, /*tr_radius=*/0.5, /*rho=*/1e2, 1e-9);

    EXPECT_EQ(e.n_orig, 2);
    EXPECT_EQ(e.ns, 3);
    EXPECT_EQ(e.qp.n(), 5);
    EXPECT_EQ(e.qp.me(), 2) << "slacks add COLUMNS, never rows";
    EXPECT_EQ(e.qp.mi(), 2);
    EXPECT_EQ(e.eq_slack[0], 2);
    EXPECT_EQ(e.eq_slack[1], 3);
    EXPECT_EQ(e.ineq_slack[0], 4);
    EXPECT_EQ(e.ineq_slack[1], kNoSlack) << "row 1 is satisfied at p_ref and must stay hard";
    EXPECT_DOUBLE_EQ(e.violation_l1, 5.5);
    ASSERT_EQ(e.slack_scale.size(), 3);
    EXPECT_DOUBLE_EQ(e.slack_scale(0), 3.0);
    EXPECT_DOUBLE_EQ(e.slack_scale(1), 2.0);
    EXPECT_DOUBLE_EQ(e.slack_scale(2), 1.0) << "max(1, 0.5): columns are never scaled UP";

    // The augmented problem must pass the engine's own validator, INCLUDING
    // its upper-triangle rule: the slack block contributes no H entry at all
    // (the penalty is purely linear), so H is the original's entries in a
    // 5x5 frame and no lower-triangle entry can appear.
    EXPECT_NO_THROW(e.qp.validate());
    EXPECT_EQ(e.qp.H.rows(), 5);
    EXPECT_EQ(e.qp.H.cols(), 5);
    EXPECT_EQ(e.qp.H.nonZeros(), 2);
    const Eigen::MatrixXd Hd = Eigen::MatrixXd(e.qp.H);
    EXPECT_DOUBLE_EQ(Hd(0, 0), 2.0);
    EXPECT_DOUBLE_EQ(Hd(1, 1), 4.0);
    for (Index i = 2; i < 5; ++i) {
        for (Index j = 0; j < 5; ++j) {
            EXPECT_DOUBLE_EQ(Hd(i, j), 0.0) << "slack row " << i << ", col " << j;
            EXPECT_DOUBLE_EQ(Hd(j, i), 0.0) << "slack col " << i << ", row " << j;
        }
    }

    // g: the original gradient, then rho*sigma_j per slack -- which is what
    // makes the objective term rho * (the ACTUAL violation) at any scale.
    EXPECT_DOUBLE_EQ(e.qp.g(0), 1.0);
    EXPECT_DOUBLE_EQ(e.qp.g(1), -1.0);
    EXPECT_DOUBLE_EQ(e.qp.g(2), 1e2 * 3.0);
    EXPECT_DOUBLE_EQ(e.qp.g(3), 1e2 * 2.0);
    EXPECT_DOUBLE_EQ(e.qp.g(4), 1e2 * 1.0);
    set_elastic_penalty(e, 1e3);
    EXPECT_DOUBLE_EQ(e.qp.g(0), 1.0) << "escalation must touch the SLACK block only";
    EXPECT_DOUBLE_EQ(e.qp.g(1), -1.0);
    EXPECT_DOUBLE_EQ(e.qp.g(2), 1e3 * 3.0);
    EXPECT_DOUBLE_EQ(e.qp.g(3), 1e3 * 2.0);
    EXPECT_DOUBLE_EQ(e.qp.g(4), 1e3 * 1.0);

    // Coefficients: +sign(residual)*sigma on an equality's slack (so that
    // s = |r|/sigma absorbs the violation at p = 0), -sigma on an
    // inequality's.
    const Eigen::MatrixXd Ae_d = Eigen::MatrixXd(e.qp.Ae);
    EXPECT_DOUBLE_EQ(Ae_d(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(Ae_d(0, 2), 3.0) << "residual +3: the slack must ADD, scaled by sigma";
    EXPECT_DOUBLE_EQ(Ae_d(0, 3), 0.0);
    EXPECT_DOUBLE_EQ(Ae_d(0, 4), 0.0);
    EXPECT_DOUBLE_EQ(Ae_d(1, 1), 1.0);
    EXPECT_DOUBLE_EQ(Ae_d(1, 3), -2.0) << "residual -2: the slack must SUBTRACT";
    EXPECT_DOUBLE_EQ(Ae_d(1, 2), 0.0);
    const Eigen::MatrixXd Ai_d = Eigen::MatrixXd(e.qp.Ai);
    EXPECT_DOUBLE_EQ(Ai_d(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(Ai_d(0, 4), -1.0);
    EXPECT_DOUBLE_EQ(Ai_d(0, 2), 0.0);
    EXPECT_DOUBLE_EQ(Ai_d(1, 1), 1.0);
    EXPECT_DOUBLE_EQ(Ai_d(1, 2), 0.0) << "an unrelaxed row gets NO slack column";
    EXPECT_DOUBLE_EQ(Ai_d(1, 3), 0.0);
    EXPECT_DOUBLE_EQ(Ai_d(1, 4), 0.0);

    // The box: the trust-region window folded onto the ORIGINAL variables
    // (max(-1, 0 - 0.5) .. min(1, 0 + 0.5)) -- which is the whole reason the
    // radius is folded in by hand instead of passed as
    // SolveOverrides::tr_radius (which would cap the slacks at Delta too, see
    // sqp_driver.h's ELASTIC TIER note) -- and [0, violation_l1/sigma_j] on
    // the slacks, i.e. an ACTUAL violation of at most violation_l1 each.
    EXPECT_DOUBLE_EQ(e.qp.lower(0), -0.5);
    EXPECT_DOUBLE_EQ(e.qp.upper(0), 0.5);
    EXPECT_DOUBLE_EQ(e.qp.lower(1), -0.5);
    EXPECT_DOUBLE_EQ(e.qp.upper(1), 0.5);
    for (Index k = 2; k < 5; ++k) {
        EXPECT_DOUBLE_EQ(e.qp.lower(k), 0.0);
        EXPECT_DOUBLE_EQ(e.qp.upper(k), 5.5 / e.slack_scale(k - 2));
    }

    // FEASIBLE BY CONSTRUCTION, exhibited: (p, s) = (0, (1, 1, 0.5)) -- the
    // witness in the SCALED variables, whose violations are (3, 2, 0.5) --
    // satisfies every augmented row exactly. This is the property the whole
    // tier rests on: it is why a FALSE kInfeasible (Phase 2's ride-landing
    // class) is retried successfully rather than aborting.
    Vec witness(5);
    witness << 0.0, 0.0, 1.0, 1.0, 0.5;
    EXPECT_DOUBLE_EQ((e.qp.Ae * witness - e.qp.be).lpNorm<Eigen::Infinity>(), 0.0);
    EXPECT_LE((e.qp.Ai * witness - e.qp.bi).maxCoeff(), 0.0);
    for (Index i = 0; i < 5; ++i) {
        EXPECT_GE(witness(i), e.qp.lower(i));
        EXPECT_LE(witness(i), e.qp.upper(i));
    }
    EXPECT_LE(witness.tail(3).maxCoeff(), 1.0)
        << "the scaled witness is O(1) at ANY constraint scale -- the whole "
           "point of the residual scaling";
    EXPECT_DOUBLE_EQ(e.slack_violations(witness).lpNorm<1>(), e.violation_l1)
        << "violation_l1 IS the witness's slack vector, which is what the "
           "driver's usability test compares an elastic solution against";
}

// TEST (d), THE PHASE-2 CARRY-FORWARD, made executable at the level where the
// class is reachable. The rare FALSE kInfeasible from the QP engine (the
// ride-landing class Phase 2's ledger promised a second detection layer for)
// lands in this tier, and the tier's answer is a second solve of a problem
// that is FEASIBLE BY CONSTRUCTION -- so the retry cannot repeat the false
// verdict. What has to be shown is the second half: that the retry does not
// merely succeed but recovers the ORIGINAL subproblem's own solution.
//
// It does, by the EXACT-PENALTY property, and the fixture pins both sides of
// the threshold. QP: min 1/2 |p|^2 s.t. p0 + p1 = 1, -5 <= p <= 5.
//   Solution p* = (0.5, 0.5), lambda_e* = -0.5 (from p* + lambda_e*(1,1) = 0).
//   The equality's residual at p = 0 is +1, so it IS relaxed and the test is
//   not vacuous.
//   rho = 0.1 < |lambda_e*|: the relaxation is CHEAPER than the constraint,
//     and the elastic solution slides. With t = p0 + p1 = 1 - s, the objective
//     is t^2/4 + 0.1(1 - t), minimized at t = 0.2: p = (0.1, 0.1), s = 0.8.
//   rho = 100 > |lambda_e*|: paying the penalty is strictly worse than
//     satisfying the row, so s = 0 and p = p* EXACTLY.
// That threshold IS the rho escalation's justification: the ladder from 1e2
// to 1e8 is a search for a rho above the subproblem's own multipliers.
TEST(SqpDriverElastic, ElasticReformulationRecoversAFeasibleQpsSolution) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 1.0;
    qp.H.insert(1, 1) = 1.0;
    qp.H.makeCompressed();
    qp.g = Vec::Zero(2);
    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(1, 2);
    qp.Ae.insert(0, 0) = 1.0;
    qp.Ae.insert(0, 1) = 1.0;
    qp.Ae.makeCompressed();
    qp.be = Vec(1);
    qp.be << 1.0;
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -5.0);
    qp.upper = Vec::Constant(2, 5.0);

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        QpOptions qopts;
        qopts.ws_algebra = algebra;
        QpEngine engine(qopts);

        // What the ORIGINAL subproblem would have returned had it not lied.
        const QpSolution truth = engine.solve(qp);
        ASSERT_EQ(truth.status, QpStatus::kOptimal);
        ASSERT_NEAR(truth.x(0), 0.5, 1e-9);
        ASSERT_NEAR(truth.x(1), 0.5, 1e-9);

        ElasticQp e = build_elastic_subproblem(qp, std::numeric_limits<double>::infinity(),
                                               /*rho=*/0.1, 1e-9);
        ASSERT_EQ(e.ns, 1);
        const QpSolution cheap = engine.solve(e.qp);
        ASSERT_EQ(cheap.status, QpStatus::kOptimal);
        EXPECT_NEAR(cheap.x(0), 0.1, 1e-7);
        EXPECT_NEAR(cheap.x(1), 0.1, 1e-7);
        EXPECT_NEAR(cheap.x(2), 0.8, 1e-7) << "below the multiplier, the penalty is worth paying";

        set_elastic_penalty(e, 1e2);
        const QpSolution dear = engine.solve(e.qp);
        ASSERT_EQ(dear.status, QpStatus::kOptimal);
        EXPECT_NEAR(dear.x(2), 0.0, 1e-9) << "above the multiplier, the relaxation closes";
        EXPECT_NEAR(dear.x(0), truth.x(0), 1e-8);
        EXPECT_NEAR(dear.x(1), truth.x(1), 1e-8);

        // And the projection hands the driver back exactly the original
        // subproblem's own answer, in the original variables.
        const QpSolution proj = elastic_project(e, qp, dear, /*carry_multipliers=*/true);
        EXPECT_EQ(proj.x.size(), 2);
        EXPECT_EQ(static_cast<Index>(proj.bound_state.size()), 2);
        EXPECT_EQ(static_cast<Index>(proj.tr_active.size()), 2);
        EXPECT_NEAR(proj.x(0), 0.5, 1e-8);
        EXPECT_NEAR(proj.lambda_e(0), truth.lambda_e(0), 1e-7);
    }
}

// TEST (a). The constructed fixture above, end to end, at the DEFAULT radius.
//
// HAND-DERIVED TRAJECTORY (all three rows; see the fixture comment for the
// algebra):
//   trial 0, Delta = 1, x = (0, 0.5): the QP is INFEASIBLE (Farkas y = (1,1)).
//     Elastic: both rows relaxed, s1 + s2 >= 1 identically, so every rho in
//     the ladder leaves the relaxation open and all 6 escalations are spent.
//     The step p = (1, -0.5) is taken anyway -- it is what the tier is FOR --
//     and lands on x = (1, 0), where the TRUE violation is 0.
//   trial 1, x = (1, 0): the linearization is now consistent; the plain
//     Newton step p = (1, 0) lands on x* = (2, 0).
//   trial 2: KKT residual 0, kOptimal, no subproblem solved.
TEST(SqpDriverElastic, InconsistentLinearizationRecovers) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        InconsistentLinearizationModel model;
        SqpOptions opts; // tr_init = 1.0
        opts.qp.ws_algebra = algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);
        record_history(sol, "inconsistent_linearization");

        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 2.0, 1e-9);
        EXPECT_NEAR(sol.x(1), 0.0, 1e-9);
        EXPECT_NEAR(sol.f, 0.0, 1e-12);

        // THE PIN THE BRIEF ASKS FOR: this solve went through the elastic
        // tier. Without it, trial 0's kInfeasible ends the solve (that is
        // exactly Task 6b's behaviour) and none of the above is reached.
        EXPECT_GE(sol.counters.elastic_activations, 1);
        EXPECT_EQ(sol.counters.elastic_activations, 1);
        EXPECT_EQ(sol.counters.elastic_escalations, 6)
            << "1e2 -> 1e8 is six x10 steps, and this fixture's relaxation "
               "never closes, so the whole ladder is spent";
        EXPECT_EQ(sol.counters.major_iters, 2);
        EXPECT_EQ(sol.counters.steps_accepted, 2);
        EXPECT_EQ(sol.counters.rejected_steps, 0);

        ASSERT_EQ(sol.history.size(), 3u);
        EXPECT_TRUE(sol.history[0].elastic_applied);
        EXPECT_EQ(sol.history[0].qp_status, QpStatus::kOptimal)
            << "the row's qp_status is the FINAL ELASTIC solve's, not the "
               "kInfeasible that triggered the tier";
        EXPECT_EQ(sol.history[0].verdict, StepVerdict::kAcceptF);
        EXPECT_DOUBLE_EQ(sol.history[0].step_norm, 1.0)
            << "|(1, -0.5)|inf, the original block only";
        EXPECT_DOUBLE_EQ(sol.history[0].violation_l1, 1.0);

        // THE RADIUS BIT IS RE-DERIVED, not lost. The elastic solve gets the
        // window as REAL bounds, so the engine reports tr_active all-false and
        // elastic_project has to reconstruct it (sqp_driver.h). p0 = 1 sits on
        // the TR-truncated bound min(5, 0 + 1) = 1, strictly inside the real
        // bound 5, so this row's radius WAS binding -- and because the step is
        // also strong (actual/pred = 3.25/3.25 = 1 >= 0.75), the growth rule
        // fires and the next trial is solved at 2. Both halves would silently
        // disappear if tr_active came back all-false.
        EXPECT_TRUE(sol.history[0].tr_binding);
        EXPECT_DOUBLE_EQ(sol.history[0].tr_radius, 1.0);
        EXPECT_DOUBLE_EQ(sol.history[1].tr_radius, 2.0);

        // COST ACCOUNTING, the SOC convention ported: the ROW carries the
        // FINAL elastic solve's counts, while the AGGREGATE carries every
        // solve this trial paid for -- the kInfeasible original plus all seven
        // rungs of the ladder. Observed: 3 minor iterations on row 0 against
        // 29 for the whole solve.
        Index row_minors = 0;
        for (const SqpIterate &h : sol.history) {
            row_minors += h.qp_minor_iters;
        }
        EXPECT_EQ(sol.history[0].qp_minor_iters, 2);
        EXPECT_GT(sol.counters.qp_minor_iters, row_minors)
            << "the escalation re-solves' cost is folded into the aggregate only";

        // FIX ROUND 1, [Important 2]: THE LADDER KEEPS THE HOT START. Only
        // g's slack block changes between rungs, so qp_engine.h's HOT-START
        // REUSE conditions (a)/(c) (H/Ae/Ai values and structure) and (d)
        // (the effective regularization pair) hold across the whole ladder by
        // construction; condition (b) -- the seed working set equals the
        // IMMEDIATELY PRECEDING solve's exit working set -- is the one the
        // driver has to earn, and it does so by CHAINING the seed rung to
        // rung. Re-seeding every rung from the original kInfeasible solve
        // instead fails (b) from rung 2 on and pays a K0 rebuild each time.
        // MEASURED on this fixture in border mode: 9 factorizations before
        // the chain, 3 after -- i.e. the ladder's own share went 7 -> 1 (the
        // remaining 2 are the original solve's and trial 1's). The reuse is
        // NOT asserted unconditionally, per that header's warning that its
        // conditions are necessary and not sufficient; this is the observed
        // count with the mechanism spelled out.
        if (algebra == WorkingSetLinearAlgebra::kSchurBorder) {
            EXPECT_EQ(sol.counters.factorizations, 3)
                << "the seven-rung rho ladder must not rebuild K0 per rung";
        }
        ::testing::Test::RecordProperty(
            "inconsistent_lin_factorizations",
            fmt::format("{}:{}",
                        algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize",
                        sol.counters.factorizations));
        EXPECT_FALSE(sol.history[1].elastic_applied);
        EXPECT_DOUBLE_EQ(sol.history[1].violation_l1, 0.0)
            << "the elastic step landed exactly on the feasible set";
        EXPECT_FALSE(sol.history[2].qp_solved);

        // The returned quadruple is a genuine KKT certificate of the NLP.
        const NlpKktResidual r = self_check_kkt(model, sol, opts.feas_tol);
        EXPECT_LT(r.stationarity, 1e-8);
        EXPECT_LT(r.primal, 1e-9);
        EXPECT_LT(r.dual_sign, 1e-12);
        EXPECT_LT(r.complementarity, 1e-9);
    }
}

// FIX ROUND 1, [Important 1], AS CORRECTED IN ROUND 2: THE TIER'S ONE
// ARITHMETIC CEILING, and the COLUMN SCALING that keeps it above every scale
// the engine itself can serve.
//
// THE FAILURE MODE. qp_engine.h's is_runaway guard reports kNumericalError
// when a FREE variable exceeds detail::unbounded_artifact_scale (0.1 /
// primal_delta = 1e7 at the defaults) toward a bound that could not have
// restrained it -- "the answer is the regularization talking rather than an
// optimum". A raw slack is exactly such a variable: its natural size is the
// linearized VIOLATION, which scales with the constraint scale and has no
// relation to the regularization. With the +inf slack bounds this task first
// shipped, a subproblem whose violation exceeds ~1e7 therefore came back
// kNumericalError from the ELASTIC solve -- so the tier reported "exhausted"
// on a problem it had not even been allowed to try.
//
// MEASURED, on this fixture's own NLP with the two cI rows multiplied by S
// (which changes neither the feasible set nor the solution x* = (2, 0)):
//
//     S      before this fix                       after
//     1e6    kOptimal, 1 activation, 6 escalations  same
//     1e7    kOptimal, 1 activation, 6 escalations  same
//     1e8    kInfeasible, 1 activation, 0 escal.,   kOptimal, 1 activation,
//            elastic solve = kNumericalError        6 escalations
//     1e9    kInfeasible, as above                  kOptimal, as above
//
// THE FIX IS THE RESIDUAL SCALING of the slack columns (sqp_driver.h's
// FINITE, SCALED SLACKS note, and the THE COLUMNS ARE RESIDUAL-SCALED
// paragraph on the construction test above): the column and its penalty entry
// are multiplied by sigma_j = max(1, |r_j|), so the slack VARIABLE is the
// row's violation in units of sigma_j and the witness sits at s_j <= 1 at ANY
// constraint scale, while rho*sigma_j*s_j is still rho times the actual
// violation -- the exact-penalty threshold is unmoved.
//
// AND THE REASON IT WORKS IS CONDITIONING, which is worth stating because it
// says when this class recurs. On this fixture the two rows satisfy
// s1 + s2 = S identically, and on that face the penalty term is the constant
// rho*S, so the objective reduces to p1 + p1^2 and the optimum is the ENDPOINT
// p1 = -0.5, s = (0, S), at every scale. The unscaled row is (S, -1): its
// magnitudes differ by S, while the objective differential that decides where
// on the face to stop is O(1) (exactly 0.25). Past S ~ 1e3 that tie-break is
// lost in the linear solve and the returned point DRIFTS continuously off the
// endpoint toward the face's midpoint -- (1250, 8750) at 1e4, (49015, 50985)
// at 1e5, (5e7, 5e7) at 1e8 -- where the slacks are FREE and above the runaway
// limit. Scaling makes the row (S, -S/2), and the same sweep then returns the
// endpoint exactly at every S from 1e1 to 1e8.
//
// A FINITE CEILING ALONE DOES NOT FIX IT, and an earlier version of this
// comment said it did, on the strength of "the optimum is AT the bound so the
// slack is pinned". The optimum is indeed at the bound -- but the SOLVE does
// not return it at large S, which is the whole problem, so a bound-only
// construction still exits kNumericalError at S = 1e8 with both slacks free
// at 5e7 INSIDE a ceiling of 1e8. The ceiling is kept (it forbids increasing
// the total violation, and it pins a saturating slack out of the guard's
// reach) but it is not what makes this test pass.
TEST(SqpDriverElastic, ElasticSurvivesALargeConstraintScale) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        InconsistentLinearizationModel model(1e8);
        SqpOptions opts; // tr_init = 1.0, primal_delta = 1e-8 -> runaway limit 1e7
        opts.qp.ws_algebra = algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);
        record_history(sol, "scaled_1e8");

        EXPECT_EQ(sol.status, SqpStatus::kOptimal)
            << "the elastic solve must not be mistaken for an unbounded artifact";
        EXPECT_NEAR(sol.x(0), 2.0, 1e-7);
        EXPECT_NEAR(sol.x(1), 0.0, 1e-7);
        EXPECT_GE(sol.counters.elastic_activations, 1);
        ASSERT_FALSE(sol.history.empty());
        EXPECT_TRUE(sol.history[0].elastic_applied);
        EXPECT_EQ(sol.history[0].qp_status, QpStatus::kOptimal)
            << "before the finite slack bound this read kNumericalError";
        // The ladder was actually walked, which is what says the elastic solve
        // produced a usable answer at every rung rather than dying on rung 1.
        EXPECT_EQ(sol.counters.elastic_escalations, 6);
    }
}

// TEST (b). The tier must be INERT on every problem that does not need it --
// it is reached from a kInfeasible subproblem and from nothing else, so a
// clean solve must not pay a single elastic solve. Loops the Task-4 fixture
// set at the options each of those tests ships with.
TEST(SqpDriverElastic, ElasticIsIdleOnCleanProblems) {
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
        SqpDriver driver(c.opts);
        const SqpSolution sol = driver.solve(*p.model);
        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_EQ(sol.counters.elastic_activations, 0);
        EXPECT_EQ(sol.counters.elastic_escalations, 0);
        EXPECT_EQ(std::count_if(sol.history.begin(), sol.history.end(),
                                [](const SqpIterate &h) { return h.elastic_applied; }),
                  0);
    }
}

// TEST (c). A GENUINELY inconsistent NLP -- InconsistentBoundedModel, whose
// linearization is infeasible at every radius AND whose NLP is infeasible
// too (x0 = 5 is outside its own box) -- so no amount of elasticity can
// close it. What the tier must do is spend its bounded budget and then
// SIGNAL, rather than either giving up at once or looping forever.
//
// HAND-DERIVED (f = 1/2|x|^2, cE = x0 - 5, box [0,1]^2, x_start = (0.5, 0.5),
// Delta = 1; g = x and H = I on every major):
//   trial 0: box p in [-0.5, 0.5]^2, the row needs p0 = 4.5 -> kInfeasible.
//     Elastic (one signed slack, sigma = +1): p0 + s = 4.5. The objective in
//     p0 is (0.5 - rho) p0 + p0^2/2, so p0 runs to its upper bound 0.5 for
//     every rho >= 1: p = (0.5, -0.5), s = 4.0. Nonzero at rho_max -> all 6
//     escalations spent -- but the LINEARIZED violation DID fall, 4.5 -> 4.0,
//     so the tier has something to offer and the step is judged: h 4.5 -> 4.0
//     inside a funnel of width 100, h-type, ACCEPTED.
//   trial 1, x = (1, 0): the box is now p in [-1, 0] x [0, 1] and the row
//     needs p0 = 4 -> kInfeasible again. Elastic: p0 is pinned at its upper
//     bound 0 by the box, so p = 0 and s = 4.0 = the violation at p = 0.
//     NOTHING was gained: the relaxation is still open at rho_max, no step
//     reduces the linearized violation, and the model promises no objective
//     decrease either (pred_df = 0). That is the elastic tier EXHAUSTED --
//     the authoritative Algorithm-5 trigger -- and it exits through the
//     kRestore interim route.
TEST(SqpDriverElastic, RhoEscalationIsBoundedAndSignals) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        InconsistentBoundedModel model;
        SqpOptions opts; // tr_init = 1.0
        opts.qp.ws_algebra = algebra;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);
        record_history(sol, "rho_escalation");

        EXPECT_EQ(sol.status, SqpStatus::kInfeasible);
        EXPECT_EQ(sol.counters.elastic_activations, 2);
        EXPECT_EQ(sol.counters.elastic_escalations, 12) << "6 per activation, the whole ladder";
        EXPECT_EQ(sol.counters.major_iters, 2);
        EXPECT_EQ(sol.counters.steps_accepted, 1);

        ASSERT_EQ(sol.history.size(), 2u);
        EXPECT_TRUE(sol.history[0].elastic_applied);
        EXPECT_EQ(sol.history[0].verdict, StepVerdict::kAcceptH);
        EXPECT_DOUBLE_EQ(sol.history[0].violation_l1, 4.5);
        EXPECT_DOUBLE_EQ(sol.history[1].violation_l1, 4.0) << "the elastic step reduced h";

        // The terminating row: the elastic tier's own exhaustion signal,
        // routed through the SAME interim path Task 6 established for the
        // funnel's kRestore. Task 9 consumes exactly this.
        const SqpIterate &last = sol.history.back();
        EXPECT_TRUE(last.qp_solved);
        EXPECT_TRUE(last.elastic_applied);
        EXPECT_EQ(last.verdict, StepVerdict::kRestore);
        // TASK 9: the signal is now SERVICED rather than exited on. The tier's
        // own behaviour above is unchanged (same activations, same escalations,
        // same two rows); what follows the signal is the restoration phase,
        // which certifies this NLP infeasible at x = (1, 0) --
        // SqpDriverRestoration.ExhaustedElasticTierEntersRestoration is that
        // half of the claim, including the subgradient certificate.
        EXPECT_GE(sol.counters.restoration_iters, 1) << "the signal must now be serviced";
    }
}

// =========================================================================
// Phase-5 TASK 10: SqpOptions::elastic_ladder_early_exit, an OPT-IN lever
// (default false -- every test above runs unmodified and still spends the
// full six-rung ladder). See sqp_types.h's own note on the option, and
// sqp_driver.h's ELASTIC TIER note (THE STALL EARLY-EXIT paragraph), for why
// this defaults off rather than on: it is SAFE (escalation count drops,
// certified outcome unchanged to noise) on a fixture whose relaxed slacks
// are pinned at a REAL bound for the whole rho range, and NOT SAFE in
// general -- on a fixture whose elastic relaxation's augmented objective is
// EXACTLY CONSTANT on its whole feasible set (H === 0), every rung is
// equally optimal and a LATER one can still return a DIFFERENT arbitrary
// point of that set (an O(1) tie-break lost against rho's growing scale, not
// a later rung finding real progress an earlier one missed -- fix round 1,
// I-1). The two tests below pin the SAFE case; the third pins that the
// UNSAFE case still reaches a correct final answer despite its reshaped
// trajectory.
// =========================================================================

// THE SAFE CASE, PART 1: InconsistentLinearizationRecovers's own fixture
// (fix round 2, N-3: synced to the corrected, tolerance-based reading --
// see sqp_driver.h's STALL EARLY-EXIT note's WHAT THE EXIT ACTUALLY TESTS
// paragraph). Its hand-derivation (this file's own comment on that test)
// says the two antiparallel slack columns sit at the SAME corner "at every
// scale", but the ENGINE's returned point is not exactly there and drifts a
// little every rung (measured: |dx|inf 3.6e-13 at rho 1e2->1e3, growing to
// 3.6e-8 by rho 1e7->1e8, working set changing at rung 2). The lever is
// SAFE here not because rungs repeat exactly, but because the first
// escalation's drift already falls under kElasticStallScale's 1e-12
// tolerance and the whole ladder's accumulated drift stays four orders
// below feas_tol/kkt_tol regardless -- so turning the lever on cuts the
// escalation count with the certified outcome unchanged to that same noise
// floor, not "unchanged because nothing moved".
TEST(SqpDriverElastic, EarlyExitCutsTheLadderOnABoundPinnedFixture) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        InconsistentLinearizationModel model;
        SqpOptions opts; // tr_init = 1.0
        opts.qp.ws_algebra = algebra;
        opts.elastic_ladder_early_exit = true;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);

        // THE MUTATION-KILL PIN: 1, not the off-lever reading of 6 -- rung 2
        // (rho = 1e3) reproduces rung 1 (rho = 1e2) and the ladder stops
        // there. Deleting the early-exit check in sqp_driver.h's loop, or
        // wiring this option to be ignored, makes this read 6 and fail here.
        EXPECT_EQ(sol.counters.elastic_activations, 1);
        EXPECT_EQ(sol.counters.elastic_escalations, 1)
            << "rung 2 already reproduces rung 1 on this bound-pinned fixture";

        // THE CERTIFIED OUTCOME, unchanged from the lever-off reading
        // (InconsistentLinearizationRecovers, same fixture, same asserts).
        EXPECT_EQ(sol.status, SqpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 2.0, 1e-9);
        EXPECT_NEAR(sol.x(1), 0.0, 1e-9);
        EXPECT_NEAR(sol.f, 0.0, 1e-12);
        ASSERT_EQ(sol.history.size(), 3u);
        EXPECT_EQ(sol.history[0].verdict, StepVerdict::kAcceptF);
        // THE ONE THING THAT DOES MOVE (see sqp_types.h's own note): an
        // informational violation_l1 reading, by ~1e-13 -- MEASURED
        // 4.0012437807490642e-13 (border) / 2.0006218903745321e-13
        // (refactorize), because kElasticStallScale accepts rung 2 as "the
        // same" as rung 1 at 1e-12 relative rather than bit-for-bit, and
        // rho = 1e8's re-solve (the lever-off ladder's last rung) happens to
        // land a few ULPs nearer the exact corner than rho = 1e3's does.
        // Four to five orders below feas_tol/kkt_tol's default 1e-6 --
        // "the elastic step landed on the feasible set" either way.
        EXPECT_NEAR(sol.history[1].violation_l1, 0.0, 1e-9);

        const NlpKktResidual r = self_check_kkt(model, sol, opts.feas_tol);
        EXPECT_LT(r.stationarity, 1e-8);
        EXPECT_LT(r.primal, 1e-9);
        EXPECT_LT(r.dual_sign, 1e-12);
        EXPECT_LT(r.complementarity, 1e-9);
    }
}

// THE SAFE CASE, PART 2: RhoEscalationIsBoundedAndSignals's fixture, whose
// own hand-derivation says p0 runs to its upper bound "for every rho >= 1"
// on BOTH activations -- again a bound-pinned corner, stable from rung 1.
TEST(SqpDriverElastic, EarlyExitCutsBothActivationsOnASecondBoundPinnedFixture) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        InconsistentBoundedModel model;
        SqpOptions opts; // tr_init = 1.0
        opts.qp.ws_algebra = algebra;
        opts.elastic_ladder_early_exit = true;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model);

        EXPECT_EQ(sol.status, SqpStatus::kInfeasible);
        EXPECT_EQ(sol.counters.elastic_activations, 2);
        EXPECT_EQ(sol.counters.elastic_escalations, 2)
            << "1 per activation (the off-lever reading is 12, 6 per activation)";
        EXPECT_EQ(sol.counters.major_iters, 2);
        ASSERT_EQ(sol.history.size(), 2u);
        EXPECT_EQ(sol.history[0].verdict, StepVerdict::kAcceptH);
        EXPECT_DOUBLE_EQ(sol.history[0].violation_l1, 4.5);
        EXPECT_DOUBLE_EQ(sol.history[1].violation_l1, 4.0);
        EXPECT_EQ(sol.history.back().verdict, StepVerdict::kRestore);
        EXPECT_GE(sol.counters.restoration_iters, 1);
    }
}

// THE UNSAFE CASE, DOCUMENTED RATHER THAN HIDDEN: InfeasibleCircleLineModel
// (tests/test_sqp_restoration.cpp; reproduced here rather than shared, to
// keep this file's own TDD self-contained) has an elastic relaxation whose
// Lagrangian Hessian is IDENTICALLY ZERO (docs/notes/
// 2026-07-31-schur-cap-policy.md section 4), so its subproblem is a genuine
// LP over a FACE of alternative optima -- exactly the class
// sqp_driver.h's STALL EARLY-EXIT note says the lever is not safe for. This
// test is the evidence for that note's "still reaches a correct final
// answer" claim: turning the lever on reshapes the TRAJECTORY (fewer majors
// here, MEASURED, because the ladder's early stop hands the funnel a
// different point than the full ladder would have, which changes which
// mechanism -- the elastic tier's own signature vs. the funnel's radius
// grind -- eventually raises the restoration request) but the CERTIFIED
// outcome is the SAME infeasibility certificate either way.
TEST(SqpDriverElastic, EarlyExitOnADegenerateRelaxationStillCertifiesCorrectly) {
    InfeasibleCircleLineModel model(1.0, 0.0);
    SqpOptions opts_off;
    opts_off.tr_min = 1e-10;
    SqpOptions opts_on = opts_off;
    opts_on.elastic_ladder_early_exit = true;

    const SqpSolution off = SqpDriver(opts_off).solve(model);
    const SqpSolution on = SqpDriver(opts_on).solve(model);

    EXPECT_EQ(off.status, SqpStatus::kInfeasible);
    EXPECT_EQ(on.status, SqpStatus::kInfeasible);
    EXPECT_TRUE(off.infeasibility_certified);
    EXPECT_TRUE(on.infeasibility_certified);
    // THE SAME CERTIFICATE, to a tolerance set by opts.kkt_tol's default
    // (1e-6) -- both reach the circle's nearest point to the line,
    // independent of which mechanism (elastic exhaustion vs. the radius
    // floor) raised the restoration request. MEASURED: `on` lands on
    // sqrt(0.5) to ~1e-13 (a clean restoration-phase convergence); `off`
    // is off by ~5.3e-7 (it certifies via the radius floor at a looser
    // point) -- both correct to the driver's own tolerances, neither exact.
    EXPECT_NEAR(on.x(0), off.x(0), 1e-6);
    EXPECT_NEAR(on.x(1), off.x(1), 1e-6);
    EXPECT_NEAR(on.x(0), std::sqrt(0.5), 1e-6);
    EXPECT_NEAR(on.x(1), std::sqrt(0.5), 1e-6);

    // THE RESHAPED TRAJECTORY, MEASURED rather than merely claimed: the
    // early-exit run reaches restoration in far fewer majors, because it
    // stops trusting a rung the full ladder would still have escalated past.
    // This is the counter-example that keeps the lever off by default; see
    // this test's own banner and sqp_types.h's note for the reading on
    // tests/sqp/support/hs_sweeps.h's HS15, which shows the same effect on an
    // unrelated published problem.
    EXPECT_LT(on.counters.major_iters, off.counters.major_iters);
    ::testing::Test::RecordProperty(
        "degenerate_relaxation_majors",
        fmt::format("early_exit=off: {} majors; early_exit=on: {} majors", off.counters.major_iters,
                    on.counters.major_iters));
}

// =========================================================================
// SqpDriverAdaptiveMu -- Task 10: the KKT-residual-driven dual_mu schedule.
// See sqp_driver.h's ADAPTIVE DUAL REGULARIZATION note for the mechanism;
// these tests pin the caller-visible contract.
// =========================================================================

namespace {

// PORTED from tests/test_eqp_solve.cpp's
// FixedVariableWithEqualityRefinementImprovesAccuracy (Task 3's QP-level
// refinement fixture), as an NlpModel:
//
//     min 1/2||x||^2  s.t.  a*x0 + a*x1 = 2a  (i.e. x0+x1=2, scaled by a),
//     x0 >= lo,   x1 free.
//
// It is itself a QP (H = I always, the constraint Jacobian is the constant
// row (a, a) regardless of x) -- so build_subproblem's linearization is
// EXACT, not an approximation, at every iterate: every major solves the
// SAME underlying problem the engine would if handed it directly. That is
// what makes it useful here twice over: (i) it isolates the ENGINE's own
// regularization footprint (qp_engine.h's row_tolerance note) from any
// nonlinear-model effect, and (ii) since H/Ae are BYTE-IDENTICAL on every
// major (only g and the rhs shift change, and values_hash -- qp_engine.h's
// detail::values_hash -- does not read either), a run of ACCEPTED majors
// with an unchanged (primal_delta, dual_mu) pair is eligible for cross-major
// hot-start reuse, not merely same-iterate retry reuse -- see
// MuScheduleIsQuantizedAndMonotone.
//
// The small-a row is EXACTLY test_eqp_solve.cpp's amplification mechanism:
// scaling the row by a divides the equality multiplier's magnitude by a (the
// free variable's stationarity is x1 + a*lambda_e = 0), so the engine's
// dual_mu*|lambda_e| regularization footprint on that row is amplified by
// 1/a. Test_eqp_solve.cpp used a = 0.05 (amplification 20x); this file uses
// a = 1e-3 (amplification 1000x) because the DRIVER's accuracy floor is
// measured in x, one iterative-refinement step further removed from the raw
// footprint than that file's direct solve_eqp() call -- see
// AdaptiveMuRecoversTailAccuracy for the measured numbers at both mu
// policies.
class ScaledRowModel : public NlpModel {
  public:
    ScaledRowModel(double a, double lo, Vec x0) : a_(a), lo_(lo), x0_(x0) {}
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << a_ * x(0) + a_ * x(1) - 2.0 * a_;
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
        j.insert(0, 0) = a_;
        j.insert(0, 1) = a_;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        lower_ = Vec::Constant(2, -kInfBound);
        lower_(0) = lo_;
        return lower_;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override { return x0_; }

  private:
    double a_, lo_;
    Vec x0_;
    mutable Vec lower_;
};

// Is `mu` an exact power of ten (the schedule's quantization contract)?
// Compared via round-trip through log10/pow rather than a table, so it
// checks the ACTUAL formula rather than a copy of it.
bool is_exact_decade(double mu) {
    if (!(mu > 0.0)) {
        return false;
    }
    const double decade = std::pow(10.0, std::round(std::log10(mu)));
    return std::abs(mu - decade) <= 1e-9 * decade;
}

} // namespace

// THE BRIEF'S TEST. a = 1e-3, lo = 1.5, x0 = (0, 0): the equality-only
// optimum (1, 1) violates x0 >= 1.5, so x0 pins there and x1 = 0.5 (the
// SAME active-set shape as test_eqp_solve.cpp's fixture). x_star = (1.5,
// 0.5) exactly (unregularized).
//
// kkt_tol/feas_tol are tightened to 1e-10 (rather than left at the 1e-6
// default) so the loop keeps refining PAST the point a fixed engine-default
// mu can certify -- at the default 1e-6 both policies would converge on the
// very first major and never diverge, which is not evidence of anything.
//
// MEASURED (this machine): with the schedule disabled, dual_mu stays at the
// engine default (1e-8) forever and the solve never satisfies 1e-10 -- it
// reaches kMaxIter with the iterate PLATEAUED (kkt_residual bit-identical,
// 4.901e-08, from major 1 onward) at a relative x error of 3.100e-05. With
// the schedule enabled, mu drops from 1e-8 (major 0, forced -- see the
// header note's FIRST MAJOR paragraph) to the quantized decade 1e-11 (major
// 1, from clamp(1 * (4.901e-8)^1.5, 1e-12, 1e-8) = 3.43e-11, rounds to
// 1e-11), and the solve certifies kOptimal with relative x error 3.162e-11
// -- more than three orders inside the brief's 1e-9 bar, and 1e6x tighter
// than the fixed-mu plateau.
TEST(SqpDriverAdaptiveMu, AdaptiveMuRecoversTailAccuracy) {
    const double a = 1e-3;
    const double lo = 1.5;
    Vec x0 = Vec::Zero(2);
    Vec x_star(2);
    x_star << lo, 2.0 - lo;

    SqpOptions opts;
    opts.kkt_tol = 1e-10;
    opts.feas_tol = 1e-10;

    // --- (1) the schedule DISABLED: pins the fixed-mu ceiling -------------
    {
        ScaledRowModel model(a, lo, x0);
        SqpOptions fixed_opts = opts;
        fixed_opts.adaptive_mu = false;
        SqpDriver driver(fixed_opts);
        const SqpSolution sol = driver.solve(model, x0);

        EXPECT_EQ(sol.status, SqpStatus::kMaxIter)
            << "the regularization footprint at the engine's fixed default mu never clears "
               "1e-10 on this fixture -- that IS the ceiling";
        const double err = (sol.x - x_star).norm() / x_star.norm();
        // Observed 3.100e-05. Asserted much looser (still four orders above
        // the adaptive bar below) so the test does not pin floating-point
        // luck in the plateaued regime.
        EXPECT_GT(err, 1e-6) << "fixed-mu ceiling, relative error";

        ASSERT_FALSE(sol.history.empty());
        for (const SqpIterate &h : sol.history) {
            if (h.qp_solved) {
                EXPECT_DOUBLE_EQ(h.mu, fixed_opts.qp.dual_mu)
                    << "disabled: dual_mu never leaves the engine default -- trial " << h.trial;
            }
        }
    }

    // --- (2) the schedule ENABLED: recovers accuracy the brief asks for ---
    {
        ScaledRowModel model(a, lo, x0);
        SqpDriver driver(opts); // adaptive_mu defaults true
        const SqpSolution sol = driver.solve(model, x0);

        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        const double err = (sol.x - x_star).norm() / x_star.norm();
        EXPECT_LT(err, 1e-9) << "the brief's own bar; observed 3.162e-11";

        // "factorizations shows decade-quantized refactorizations only":
        // every recorded mu is an exact power of ten, and the schedule
        // genuinely moved (more than one distinct value) -- neither stuck at
        // mu_max nor drifting off the decade grid.
        std::vector<double> distinct_mu;
        for (const SqpIterate &h : sol.history) {
            if (!h.qp_solved) {
                continue;
            }
            EXPECT_TRUE(is_exact_decade(h.mu)) << "trial " << h.trial << " mu = " << h.mu;
            if (std::find(distinct_mu.begin(), distinct_mu.end(), h.mu) == distinct_mu.end()) {
                distinct_mu.push_back(h.mu);
            }
        }
        EXPECT_GE(distinct_mu.size(), 2u)
            << "the schedule must have actually shrunk mu at least once on this fixture, or the "
               "test is not exercising the mechanism at all";
        ::testing::Test::RecordProperty("adaptive_mu_scaled_row",
                                        fmt::format("majors={} err={:.3e} distinct_mu={}",
                                                    sol.counters.major_iters, err,
                                                    distinct_mu.size()));
    }
}

// THE BRIEF'S TEST (b), reshaped around what the underlying mechanism can
// actually show. HS7 (the brief's named fixture) is a poor vehicle for the
// SECOND half of this test -- its Hessian is x-DEPENDENT (Rosenbrock-shaped
// curvature), so qp_engine.h's detail::values_hash changes on every accepted
// major regardless of mu, and cross-major hot-start reuse is structurally
// impossible there. What IS demonstrable on HS7, and is checked first below,
// is the schedule's own monotonicity and quantization.
//
// For "late-iteration reuse still fires" (Task 2's carry: quantize so late
// iterations reuse), this test uses ScaledRowModel again -- its H/Ae are
// iterate-INDEPENDENT, which is exactly the shape the carry is worried
// about: WITHOUT quantization, a continuously-varying mu would force a
// refactorization on every accepted major even when nothing about the
// active set changed, because qp_engine.h's reuse key compares dual_mu by
// exact ==. Fixture: a = 1e-3, lo = 1.5, x0 = (-5, -5), tr_init = 0.01 (small
// on purpose, so the trust region grows across several majors before the
// unconstrained-in-the-box solution is reached, which is what stretches the
// mu-held-constant run long enough to observe).
TEST(SqpDriverAdaptiveMu, MuScheduleIsQuantizedAndMonotone) {
    // --- (1) monotonicity + quantization, on HS7 --------------------------
    {
        const HsProblem p = make_hs(7);
        Vec x0(2);
        x0 << 0.5, 1.5;
        SqpOptions opts;
        opts.tr_init = 10.0;
        opts.kkt_tol = 1e-8;
        opts.feas_tol = 1e-8;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model, x0);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);

        // NON-INCREASING (which subsumes "non-increasing after the first
        // change" -- mu starts at mu_max, so it has nowhere to go but down):
        // once the KKT residual driving it has contracted (it does,
        // quadratically, per QuadraticConvergenceOnHs7's own residual
        // sequence), mu must never grow back.
        double last_mu = -1.0;
        bool changed = false;
        for (const SqpIterate &h : sol.history) {
            if (!h.qp_solved) {
                continue;
            }
            EXPECT_TRUE(is_exact_decade(h.mu)) << "trial " << h.trial << " mu = " << h.mu;
            if (last_mu > 0.0) {
                EXPECT_LE(h.mu, last_mu) << "trial " << h.trial;
                changed = changed || h.mu != last_mu;
            }
            last_mu = h.mu;
        }
        EXPECT_TRUE(changed) << "the schedule must have moved at least once on HS7's own "
                                "quadratic tail (observed: 1e-8 -> 1e-10, see "
                                "QuadraticConvergenceOnHs7's residual sequence)";
    }

    // --- (2) late-iteration reuse still fires -----------------------------
    {
        const double a = 1e-3;
        const double lo = 1.5;
        Vec x0(2);
        x0 << -5.0, -5.0;
        ScaledRowModel model(a, lo, x0);
        SqpOptions opts;
        opts.tr_init = 0.01;
        opts.kkt_tol = 1e-10;
        opts.feas_tol = 1e-10;
        opts.max_iter = 40;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(model, x0);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);

        // MEASURED (this machine): trials 0-9 all hold dual_mu at the
        // engine-default decade (1e-8) while the trust region grows
        // 0.01 -> 2.56 across eight ACCEPTED majors (radius growth is the
        // mechanism, exactly RadiusGrowsOnStrongSteps' fixture shape) --
        // and trials 0-7 all report qp_factorizations == 0, i.e. K0 built
        // once (trial 0) and reused outright across seven subsequent
        // DIFFERENT iterates. Trial 10 is where the residual finally
        // clears the schedule's threshold and mu quantizes down to 1e-11 --
        // a genuine (primal_delta, dual_mu)-pair change, which correctly
        // costs a refactorization (qp_factorizations == 1 there).
        ASSERT_GE(sol.history.size(), 11u);
        for (const SqpIterate &h : sol.history) {
            if (h.qp_solved) {
                EXPECT_TRUE(is_exact_decade(h.mu)) << "trial " << h.trial;
            }
        }

        // ASSERTED AS THE OBSERVED VALUE, NOT AS == 0 UNCONDITIONALLY, per
        // qp_engine.h's HOT-START REUSE note and this file's own precedent
        // (RadiusGrowsOnStrongSteps' comment on
        // ShrinkRadiusRetryReusesHotStart): the reuse conditions this test
        // controls are (i) H/Ae are byte-identical every major (constant by
        // construction -- see ScaledRowModel's own doc comment), (ii) the
        // effective (primal_delta, dual_mu) pair is unchanged across trials
        // 0-7 (mu stays quantized at 1e-8 the whole run), and (iii) each
        // accepted major's growing radius does not introduce a bound pin
        // that the fresh seed's working set fails to match (x1 is free
        // throughout this stretch; x0 does not reach its lower bound until
        // later). All three hold on trials 1-7, so K0 built at trial 0 is
        // reused outright.
        for (Index t = 1; t <= 7; ++t) {
            EXPECT_EQ(sol.history[static_cast<std::size_t>(t)].qp_factorizations, 0)
                << "trial " << t << " -- see the three-condition comment above";
        }

        // The decade CHANGE (mu: 1e-8 -> 1e-11) is a real pair change and is
        // expected to cost a refactorization -- this is Task 2's "safe
        // direction" (refactorize more, never less) working as intended,
        // not a regression of the reuse this test just measured.
        bool saw_decade_drop = false;
        for (std::size_t t = 1; t < sol.history.size(); ++t) {
            const SqpIterate &prev = sol.history[t - 1];
            const SqpIterate &cur = sol.history[t];
            if (prev.qp_solved && cur.qp_solved && cur.mu != prev.mu) {
                saw_decade_drop = true;
                EXPECT_GE(cur.qp_factorizations, 1)
                    << "a genuine mu-decade change must force a refactorization, trial " << t;
            }
        }
        EXPECT_TRUE(saw_decade_drop) << "this fixture is only interesting if mu actually moved";
    }
}

// THE BRIEF'S TEST (c). adaptive_mu = false must reproduce the pre-Task-10
// engine behaviour byte for byte: SolveOverrides::dual_mu is never touched,
// so every row's mu is the engine's own QpOptions::dual_mu default, and the
// counters are whatever the driver would have produced before this task
// existed (there is no other lever left for this task to have moved).
TEST(SqpDriverAdaptiveMu, ScheduleIsIdleWhenDisabled) {
    const HsProblem p = make_hs(7);
    Vec x0(2);
    x0 << 0.5, 1.5;
    SqpOptions opts;
    opts.tr_init = 10.0;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    opts.adaptive_mu = false;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model, x0);

    // Byte-identical to QuadraticConvergenceOnHs7 (adaptive_mu defaults true
    // there but this fixture never needs mu to move at the default
    // kkt_tol/feas_tol of THAT test; here the tighter 1e-8 is what Task 9's
    // own HS7 test already used, so this is the same trajectory Task 9 left
    // behind, unmodified).
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_LE(sol.counters.major_iters, 8); // observed: 7, same as pre-Task-10

    ASSERT_FALSE(sol.history.empty());
    std::size_t solved_rows = 0;
    for (const SqpIterate &h : sol.history) {
        if (h.qp_solved) {
            ++solved_rows;
            EXPECT_EQ(h.mu, opts.qp.dual_mu)
                << "trial " << h.trial
                << " -- disabled means dual_mu NEVER leaves the engine "
                   "default, byte for byte";
        }
    }
    EXPECT_GT(solved_rows, 0u);
}

// --- Task 12: driver diagnostics -----------------------------------------
//
// The shared fixture below is deliberately IDENTICAL to
// SqpDriverEquality.QuadraticConvergenceOnHs7's (same start point, same
// options): that test already established it is small, clean, full-step
// Newton (no rejections, no SOC, no elastic activations, no restoration), so
// reusing it here means these three tests are about the SHAPE of the
// diagnostics -- the history contract, the printer, the ledger -- rather
// than about a new convergence claim.

namespace {
SqpSolution solve_hs7_diagnostics_fixture(SqpDriver &driver) {
    const HsProblem p = make_hs(7);
    Vec x0(2);
    x0 << 0.5, 1.5;
    return driver.solve(*p.model, x0);
}

SqpOptions hs7_diagnostics_opts() {
    SqpOptions opts;
    opts.tr_init = 10.0;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    return opts;
}
} // namespace

TEST(SqpDriverDiagnostics, HistoryRecordsTheFormalizedPerMajorContract) {
    SqpDriver driver(hs7_diagnostics_opts());
    const SqpSolution sol = solve_hs7_diagnostics_fixture(driver);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);

    // THE LENGTH INVARIANT (sqp_types.h's SqpCounters note, now formalized by
    // this task): stopped AT an iterate, so history.size() == major_iters + 1,
    // and the last row's qp_solved is false.
    EXPECT_EQ(sol.history.size(), static_cast<std::size_t>(sol.counters.major_iters) + 1u);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_FALSE(sol.history.back().qp_solved);

    // Every field the brief names -- KKT residual, h (violation_l1), f, Delta
    // (tr_radius), verdict, QP counters -- is populated with a sane value on
    // every row, and every row but the last has a solved subproblem behind it.
    for (std::size_t i = 0; i < sol.history.size(); ++i) {
        const SqpIterate &row = sol.history[i];
        EXPECT_TRUE(std::isfinite(row.f)) << "row " << i;
        EXPECT_TRUE(std::isfinite(row.kkt_residual)) << "row " << i;
        EXPECT_GE(row.kkt_residual, 0.0) << "row " << i;
        EXPECT_TRUE(std::isfinite(row.violation_l1)) << "row " << i;
        EXPECT_GE(row.violation_l1, 0.0) << "row " << i;
        EXPECT_GT(row.tr_radius, 0.0) << "row " << i;
        if (i + 1 < sol.history.size()) {
            EXPECT_TRUE(row.qp_solved) << "row " << i;
            EXPECT_GE(row.qp_minor_iters, 0) << "row " << i;
            EXPECT_GE(row.qp_factorizations, 0) << "row " << i;
        }
    }
}

TEST(SqpDriverDiagnostics, PrinterContainsPerIterationRowsAndFinalStatus) {
    SqpDriver driver(hs7_diagnostics_opts());
    const SqpSolution sol = solve_hs7_diagnostics_fixture(driver);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);

    const std::string table = format_iteration_table(sol);

    // One data row per history entry, plus a header line, a separator line,
    // the trailing blank-then-status lines, a trailing "Start Level" line
    // (Task 7) and a trailing "Scaling" line (M6 W0.2) -- counted by newlines
    // rather than by an exact total, so this does not pin the header's own
    // column widths.
    const std::size_t newline_count =
        static_cast<std::size_t>(std::count(table.begin(), table.end(), '\n'));
    EXPECT_EQ(newline_count, sol.history.size() + 6u);
    EXPECT_NE(table.find("Scaling: off"), std::string::npos)
        << "the trailer must say which scale the table is on, even when it is the caller's own\n"
        << table;

    EXPECT_NE(table.find("Trial"), std::string::npos) << table;
    EXPECT_NE(table.find("Verdict"), std::string::npos) << table;
    // This fixture is full-step Newton (see the shared-fixture note above):
    // every solved row is accepted, so the table must show at least one
    // acceptance verdict (AcceptF or AcceptH -- both contain "Accept").
    EXPECT_NE(table.find("Accept"), std::string::npos)
        << "expected at least one accepted-step verdict row\n"
        << table;

    // The final status.
    EXPECT_NE(table.find("Status: Optimal"), std::string::npos) << table;

    // PHASE-4 TASK 7. This fixture is a plain 2-arg (always-cold) solve, so
    // the trailing level line must read Cold, and the WD column exists but
    // is empty on every row -- nothing in this fixture engages the full-step
    // watchdog (see WatchdogRestoresOnDivergence, test_warm_start.cpp, for
    // that column's non-empty case).
    EXPECT_NE(table.find("WD"), std::string::npos) << table;
    EXPECT_NE(table.find("Start Level: Cold"), std::string::npos) << table;
}

TEST(SqpDriverDiagnostics, LedgerRecordsOneSqpSolveRecordPerDriverSolveAndQpRecordsStillWork) {
    SqpDriver driver(hs7_diagnostics_opts());

    Ledger ledger;
    driver.attach_ledger(&ledger, "hs7");

    const SqpSolution sol1 = solve_hs7_diagnostics_fixture(driver);
    ASSERT_EQ(sol1.status, SqpStatus::kOptimal);

    // Exactly one SqpSolveRecord for this one driver solve.
    ASSERT_EQ(ledger.sqp_records().size(), 1u);
    EXPECT_EQ(ledger.sqp_records()[0].label, "hs7_0");
    EXPECT_EQ(ledger.sqp_records()[0].status, sol1.status);
    EXPECT_EQ(ledger.sqp_records()[0].counters.major_iters, sol1.counters.major_iters);
    EXPECT_EQ(ledger.sqp_records()[0].counters.qp_minor_iters, sol1.counters.qp_minor_iters);
    EXPECT_EQ(ledger.sqp_records()[0].counters.factorizations, sol1.counters.factorizations);

    // QP-LEVEL RECORDS STILL WORK when both are attached: attach_ledger
    // forwards the same Ledger to this driver's own internal QpEngine, so it
    // also holds one SolveRecord per subproblem this solve built (this
    // fixture is full-step Newton -- see the shared-fixture note above -- so
    // that is exactly major_iters records, one per accepted major, no SOC/
    // elastic re-solves to inflate the count).
    ASSERT_FALSE(ledger.records().empty());
    EXPECT_EQ(ledger.records().size(), static_cast<std::size_t>(sol1.counters.major_iters));
    for (const auto &rec : ledger.records()) {
        EXPECT_NE(rec.label.find("hs7_qp_"), std::string::npos) << rec.label;
    }

    // A second solve() call on the SAME driver instance adds a SECOND
    // SqpSolveRecord -- "one per driver solve", not one total.
    const SqpSolution sol2 = solve_hs7_diagnostics_fixture(driver);
    ASSERT_EQ(sol2.status, SqpStatus::kOptimal);
    ASSERT_EQ(ledger.sqp_records().size(), 2u);
    EXPECT_EQ(ledger.sqp_records()[1].label, "hs7_1");
}

// =========================================================================
// SqpDriverEvalEconomics -- Task 8: the values/derivatives split, measured.
// =========================================================================
//
// THE FIXTURE: HS40 re-posed by tests/sqp/support/hs_sweeps.h's HsSweep (the
// same tilt/shift hs_sweep_spec(40) uses for the Phase-5 Task-7 corpus,
// spec.number/tilt/ce_shift/ci_shift), POISONED by posing it at p = 2 --
// well outside the corpus's own [p0, p1] = [0, 1] sweep range -- while
// solved COLD from the PUBLISHED HS40 start point, i.e. the point tuned for
// p = 0, with a bounded tr_init = 5 (the default trust region is
// unconstrained Newton, which this equality-only problem's Newton step
// satisfies in one shot with nothing to reject). HS40's own corpus note
// (hs_sweeps.h's THE SIX PROBLEMS) names it "the tightest equality fixture
// in the battery, where a linearization is misleading in exactly the way
// the Maratos effect needs" -- posing it this far from the point its start
// was tuned for, under a radius small enough to matter, is what turns that
// property into repeated funnel rejections (and, on several of them, an
// engaged second-order correction) before the solve finds x*(2).
TEST(SqpDriverEvalEconomics, RejectionHeavyHs40PoisonedFixtureCutsFullEvals) {
    using hven::solvers::test_support::hs_sweep_spec;
    using hven::solvers::test_support::HsSweep;

    const auto &spec = hs_sweep_spec(40);
    CountingModel model(std::make_unique<HsSweep>(spec.number, spec.tilt, spec.ce_shift,
                                                  spec.ci_shift, /*p_init=*/2.0));
    SqpOptions opts;
    opts.max_iter = 80;
    opts.tr_init = 5.0;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    // THE FIXTURE MUST ACTUALLY REJECT, or this whole battery measures
    // nothing -- this is the "rejection-heavy" precondition, pinned rather
    // than assumed. It also engages the SECOND-ORDER CORRECTION (Task 7),
    // so this one fixture exercises BOTH of THE REJECTED-TRIAL FUNNEL
    // EVALUATION's values-only paths (the plain trial and the SOC-corrected
    // one).
    ASSERT_GT(sol.counters.rejected_steps, 0);
    ASSERT_GT(sol.counters.soc_steps, 0);

    // BEFORE (Task 8 did not exist): every one of these queries -- the
    // rejected ones included -- was a full eval_nlp, so the pre-Task-8
    // full-eval count is exactly what evals_full + evals_values reads today
    // (SqpCounters::evals_full's own note states this reading). AFTER: only
    // evals_full of them still are.
    const Index before_full = sol.counters.evals_full + sol.counters.evals_values;
    const Index after_full = sol.counters.evals_full;

    // THE PIN. Observed on this fixture (clang++, Release and Debug both --
    // see this task's report for the byte-exact confirmation): 5 rejected
    // trials and 5 SOC attempts (1 promoted) over 22 history rows, cutting
    // the full-eval count from 23 (every query, including the ONE promoted
    // SOC re-solve's extra point) to 17 (the loop-top evaluation plus one
    // per ACCEPTED trial, direct or SOC-promoted).
    EXPECT_EQ(sol.counters.rejected_steps, 5);
    EXPECT_EQ(sol.counters.soc_steps, 5);
    EXPECT_EQ(sol.counters.steps_accepted, 16);
    EXPECT_EQ(static_cast<Index>(sol.history.size()), 22);
    EXPECT_EQ(before_full, 23);
    EXPECT_EQ(after_full, 17);
    EXPECT_EQ(sol.counters.evals_values, 6);

    // THE REDUCTION, stated as an inequality too, so the property being
    // demonstrated (fewer full evals than the pre-Task-8 baseline) survives
    // even if the exact trajectory above ever needs re-pinning for an
    // unrelated reason.
    EXPECT_LT(after_full, before_full);

    // CROSS-CHECK AGAINST THE MODEL'S OWN CALL COUNTS (CountingModel, not
    // just the driver's self-reported counters): eval_grad/eval_jac_e are
    // called strictly fewer times than eval_f/eval_ce (which eval_nlp alone
    // could never produce -- the two always match there), and both match
    // evals_full exactly (the loop-top evaluation plus one per
    // upgrade-to-full site, whether direct or SOC-promoted).
    //
    // THE ONE `+ 1` IS THE BRIDGE'S CLAIM PASS AND IT DOES NOT BLUNT THIS
    // GUARD. One solve(model, ...) call builds one NlpModelAggregate, whose lay
    // walks eval_jac_e once at the start point because HS40 declares equality
    // rows (include/hven/model/nlp_model_aggregate.h's constructor doc); it
    // walks no VALUE callback, which is why n_grad, n_f and n_ce carry no such
    // term and why the split's saving is still read off the same numbers. The
    // margin the strict inequality below measures narrows from 6 to 5 and stays
    // live in the direction that matters: under this file's own named mutation
    // -- routing the rejected-trial evaluation back through the full eval_nlp
    // -- every one of the 23 queries would fetch a Jacobian, so n_jac_e would
    // read 23 + 1 = 24 against an n_ce of 23 and `EXPECT_LT` would FAIL, as
    // would after_full (17 -> 23), evals_values (6 -> 0) and the equality just
    // below. The lay term makes that mutant MORE visible, not less.
    EXPECT_LT(model.n_grad, model.n_f);
    EXPECT_LT(model.n_jac_e, model.n_ce);
    EXPECT_EQ(model.n_grad, after_full);
    EXPECT_EQ(model.n_jac_e, after_full + 1);
    EXPECT_EQ(model.n_f, model.n_ce) << "HS40 has no inequalities; f/cE are read together always";
}

// STEP 3 OF THE BRIEF, IN COMMENT FORM (the mutation itself was run by hand,
// not left in the tree as dead code, then reverted): temporarily routing
// solve_impl's rejected-trial evaluation (sqp_driver.h's `NlpEval ev_trial =
// eval_nlp_values(model, x_trial);`) back through the full `eval_nlp` --
// i.e. reverting the ONE call to what it was pre-Task-8, with every
// surrounding counter/upgrade statement left alone.
//
// FIX ROUND 1 CORRECTION: the first pass of this note reported the kill
// list as scoped to THIS test's own four assertions, from a `--gtest_filter`
// run. That was wrong, and the project's own standing rule is exactly why:
// mutation runs are FULL-SUITE, never filtered to the test expected to move
// -- this single line touches EVERY trial evaluation the driver makes, not
// just this fixture's. A full, unfiltered `ctest` under the mutation kills
// FOUR TESTS, not four assertions in one:
//   SqpDriverContract.CallCountPerMajorIsBounded (HS6/HS76/HS5: n_grad/
//     n_jac_e/n_jac_i no longer equal `rows`, and the `total == per_row*rows
//     + accepted` identity breaks -- these fixtures never reject, so this is
//     the mutation's effect on the ACCEPT path alone, see below)
//   SqpDriverTrustRegion.RejectedTrialsPayNoHessian (n_grad reads 5, not 2)
//   SqpDriverSoc.SocRescuePaysNoExtraHessian (n_grad/n_jac_e read 9, not
//     1 + accepted = 5)
//   SqpDriverEvalEconomics.RejectionHeavyHs40PoisonedFixtureCutsFullEvals
//     (this test: n_grad/n_jac_e read 38, not 17 -- see below)
// 435/439 tests still pass (2 disabled, unaffected).
//
// WHY EVEN THE NON-REJECTING FIXTURES (CallCountPerMajorIsBounded) MOVE:
// `upgrade_to_full` was left in place at every accept site, so an ACCEPTED
// trial now pays for its gradient/Jacobian TWICE -- once from the
// (mutated, now-always-full) `eval_nlp_values(model, x_trial)` call, once
// again from the still-present `upgrade_to_full(model, x_trial, ev_trial)`
// right after it -- roughly doubling `n_grad`/`n_jac_e`/`n_jac_i` on every
// solve that accepts anything at all, rejecting or not. NOTE WHAT DOES NOT
// MOVE ON ANY OF THE FOUR: `sol.counters.evals_full/evals_values` (hence
// `before_full`/`after_full` below) are UNCHANGED by this mutation, because
// they are set from the control-flow OUTCOME (was this trial's fate an
// upgrade or not), not from which model method actually ran -- a caller who
// trusted only `SqpCounters` here would see NOTHING move, on any of the four
// killed tests, while the actual model-call pattern regressed everywhere.
// This is exactly why every assertion this file makes against
// `evals_full`/`evals_values` is paired with an independent
// `CountingModel`-based cross-check (`n_grad`/`n_jac_e`/`n_f`/`n_ce`), rather
// than trusting the driver's self-reported counters alone -- see
// `SqpCounters::evals_full`'s own doc (sqp_types.h) for the general
// statement of this caveat, added in the same fix round as this correction.
// What dies is specifically the CLAIM this task exists to make -- that a
// rejected trial no longer pays for a gradient it never uses -- confirmed
// now by a genuine full-suite kill list rather than a filtered one.
TEST(SqpDriverEvalEconomics, HsSweepPoisonedHs40MatchesTheBaseProblemAtPZero) {
    // Sanity precondition of the fixture above: p = 0 IS the published HS40
    // (hs_sweeps.h's THE CONSTRUCTION note), so a p = 0 solve should still
    // find the same optimum the unposed problem does -- if it did not,
    // "poisoned at p = 2" would be testing a broken re-posing rather than a
    // genuinely harder start.
    using hven::solvers::test_support::hs_sweep_spec;
    using hven::solvers::test_support::HsSweep;
    const auto &spec = hs_sweep_spec(40);
    HsSweep at_zero(spec.number, spec.tilt, spec.ce_shift, spec.ci_shift, /*p_init=*/0.0);
    SqpDriver driver{SqpOptions{}};
    const SqpSolution sol = driver.solve(at_zero);
    const HsProblem published = make_hs(40);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_NEAR(sol.f, published.f_star, 1e-6);
}

// =============================================================================
// PHASE-6 TASK 1 -- THE PROBE BUDGET, the driver seam continuation.h's retry
// economics is built on (this file's counterpart to sqp_driver.h's own PROBE
// BUDGET note on the 4-argument solve()). Three properties, one test each,
// because each one is separately load-bearing for the caller above:
// inertness at 0, the stop itself, and convergence winning over the budget.
// =============================================================================

// (1) INERTNESS. A budget of 0 -- what every 2-/3-argument call passes by
// omission -- must leave the solve BIT-IDENTICAL. Asserted against the
// 3-argument overload on the same problem from the same start, field by
// field, because "the default path did not change" is the entire basis on
// which this task claims byte-identity everywhere it did not arm a budget.
TEST(SqpDriverProbeBudget, ZeroBudgetIsBitIdenticalToNoBudget) {
    const HsProblem p = make_hs(1); // Rosenbrock: many majors, many trials
    Vec x0(2);
    x0 << -2.0, 1.0;
    SqpOptions opts;
    opts.max_iter = 60;

    SqpDriver plain(opts);
    const SqpSolution a = plain.solve(*p.model, x0, WarmStart{});
    SqpDriver budgeted(opts);
    const SqpSolution b = budgeted.solve(*p.model, x0, WarmStart{}, /*minor_budget=*/0);

    ASSERT_EQ(a.status, b.status);
    EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
    EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
    EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
    EXPECT_EQ(a.counters.steps_accepted, b.counters.steps_accepted);
    EXPECT_EQ(a.counters.rejected_steps, b.counters.rejected_steps);
    EXPECT_EQ(a.counters.evals_full, b.counters.evals_full);
    EXPECT_EQ(a.counters.probe_budget_stops, 0);
    EXPECT_EQ(b.counters.probe_budget_stops, 0);
    EXPECT_EQ(a.history.size(), b.history.size());
    EXPECT_EQ(a.x, b.x); // bit-for-bit
    EXPECT_EQ(a.f, b.f);
}

// (2) THE STOP. A budget of ONE minor on a problem that needs dozens ends the
// solve at the top of the second major -- kMaxIter, probe_budget_stops == 1,
// and (the property the controller pays for) far fewer majors than the
// unbudgeted solve of the same problem.
TEST(SqpDriverProbeBudget, ExhaustedBudgetStopsAtAnIterateAndSaysSo) {
    const HsProblem p = make_hs(1);
    Vec x0(2);
    x0 << -2.0, 1.0;
    SqpOptions opts;
    opts.max_iter = 60;

    SqpDriver plain(opts);
    const SqpSolution full = plain.solve(*p.model, x0);
    ASSERT_EQ(full.status, SqpStatus::kOptimal);
    ASSERT_GT(full.counters.major_iters, 3);

    SqpDriver budgeted(opts);
    const SqpSolution cut = budgeted.solve(*p.model, x0, WarmStart{}, /*minor_budget=*/1);
    EXPECT_EQ(cut.status, SqpStatus::kMaxIter)
        << "a probe-budget stop is an ordinary stopped-AT-an-iterate exit";
    EXPECT_EQ(cut.counters.probe_budget_stops, 1);
    EXPECT_LT(cut.counters.major_iters, full.counters.major_iters);
    // THE BUDGET IS NOT A HARD MINOR BOUND -- it is checked BETWEEN majors, so
    // the solve spends at least the budget and at most the budget plus the
    // minors of the ONE major that crossed it. **BOTH HALVES ARE NOW
    // ASSERTED; until the final fix wave (W12) only the lower one was, while
    // this comment claimed both** -- and a reader of the counter would
    // otherwise be entitled to expect <= 1.
    EXPECT_GE(cut.counters.qp_minor_iters, 1);
    // The upper half, read off the history rather than pinned as an observed
    // number: SqpIterate::qp_minor_iters (sqp_types.h) is the PER-MAJOR cost,
    // so the crossing major is the last row that solved a subproblem, and
    // every major before it summed to strictly less than the budget (that is
    // what "the budget had not yet been crossed" means).
    Index crossing_major_minors = 0;
    for (const SqpIterate &row : cut.history) {
        if (row.qp_solved) {
            crossing_major_minors = row.qp_minor_iters;
        }
    }
    ASSERT_GT(crossing_major_minors, 0) << "premise: some major did solve a subproblem";
    EXPECT_LE(cut.counters.qp_minor_iters, 1 + crossing_major_minors)
        << "the overshoot may never exceed the single major that crossed the budget";
    // The returned object is still a legal hand-off (warm_start.h), like
    // every other stopped-at-an-iterate exit's.
    EXPECT_TRUE(cut.warm_start.valid);
    EXPECT_TRUE(cut.x.allFinite());
}

// (3) CONVERGENCE WINS, and this fixture makes the ordering VISIBLE rather
// than vacuous: HS7 started at its own solution still solves ONE subproblem
// (the multipliers are not handed in, so the first measured iterate is not yet
// stationary), spends minors doing it, and therefore arrives at the next
// major's budget test already over a budget of 1 -- and is nevertheless
// reported kOptimal, with no abandonment counted. Abandonment may only ever
// cost work about to be spent, never an answer already found.
TEST(SqpDriverProbeBudget, ConvergenceBeatsAnExhaustedBudget) {
    const HsProblem p = make_hs(7);
    Vec x_star(2);
    x_star << 0.0, std::sqrt(3.0);
    SqpOptions opts;
    opts.kkt_tol = 1e-6;
    opts.feas_tol = 1e-6;

    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model, x_star, WarmStart{}, /*minor_budget=*/1);
    EXPECT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_EQ(sol.counters.probe_budget_stops, 0);
    EXPECT_NEAR(sol.f, p.f_star, 1e-9);
    // The budget WAS exhausted by the time the converged exit was taken --
    // without this the test would pass on a solve that simply never reached
    // the test at all.
    EXPECT_GE(sol.counters.qp_minor_iters, 1);
    EXPECT_EQ(sol.counters.major_iters, 1); // observed
}

// =====================================================================
// PHASE-6 TASK 4 -- THE CRASH BASIS (SqpOptions::crash_basis).
//
// The lever seeds a COLD solve's FIRST subproblem's working set from the
// activity geometry at x0. Its value therefore depends entirely on TWO
// properties of the start point, and this battery measures both directions
// rather than only the flattering one:
//
//   (i)  does x0 actually SIT on constraints? (if not, the seed is empty and
//        the lever is inert -- which is what both of this project's cold
//        corpora turn out to look like; see
//        docs/notes/2026-08-03-crash-basis.md);
//   (ii) are the constraints it sits on the ones active at x*? (if not, the
//        seed costs strictly MORE than no seed, because qp_engine.h's Dantzig
//        drop rule skips SHIFTED rows but not CRASH-SEEDED ones --
//        docs/notes/2026-08-03-identification-stall-study.md Sec. 7.8(a)).
//
// BoundaryStartModel below exists to make both measurable on one geometry,
// with the answer known in closed form:
//
//     min  1/2 ||x||^2 + c^T x     s.t.  -x_j <= 0  (j = 0..n-1)
//
// with c_j = +1 for the first (n - n_wrong) coordinates and c_j = -1 for the
// last n_wrong. Then x*_j = 0 with row j ACTIVE (lambda_j = 1) on the first
// block, and x*_j = +1 with row j strictly SLACK on the second. The start
// point is x0 = 0, which sits EXACTLY on every one of the n rows -- so the
// crash basis seeds all n of them, of which exactly (n - n_wrong) are right.
//
// A NOTE ON WHAT THIS GEOMETRY IS. At x0 every row has zero slack, so the
// unseeded walk's ratio tests all tie at ratio 0 and it admits rows one per
// minor at zero step length: this fixture's cold arm is DEGENERATE in
// qp_engine.h's own sense (QpCounters::degenerate_steps is nonzero on it).
// That is the geometry, not a defect, and it is the honest place to
// demonstrate a crash basis -- "the start point is on the boundary" is
// exactly the situation the mechanism is for. It is NOT representative of
// F7's or the Hock-Schittkowski corpus's cold starts, which is the whole
// finding of the accompanying note.
namespace {

class BoundaryStartModel : public NlpModel {
  public:
    BoundaryStartModel(Index n, Index n_wrong) : n_(n), n_wrong_(n_wrong) {}

    Index n() const override { return n_; }
    Index me() const override { return 0; }
    Index mi() const override { return n_; }

    double c(Index j) const { return j < n_ - n_wrong_ ? 1.0 : -1.0; }

    double eval_f(const Vec &x) const override {
        double acc = 0.5 * x.squaredNorm();
        for (Index j = 0; j < n_; ++j) {
            acc += c(j) * x(j);
        }
        return acc;
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g = x;
        for (Index j = 0; j < n_; ++j) {
            g(j) += c(j);
        }
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return -x; }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(n_, n_);
        for (Index i = 0; i < n_; ++i) {
            h.insert(i, i) = obj_scale;
        }
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, n_);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(n_, n_);
        for (Index i = 0; i < n_; ++i) {
            j.insert(i, i) = -1.0;
        }
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        lower_ = Vec::Constant(n_, -kInfBound);
        return lower_;
    }
    const Vec &upper() const override {
        upper_ = Vec::Constant(n_, kInfBound);
        return upper_;
    }
    Vec start_point() const override { return Vec::Zero(n_); }

    // The closed-form optimum: 0 on the first block, +1 on the second.
    Vec x_star() const {
        Vec x = Vec::Zero(n_);
        for (Index j = n_ - n_wrong_; j < n_; ++j) {
            x(j) = 1.0;
        }
        return x;
    }

  private:
    Index n_, n_wrong_;
    mutable Vec lower_, upper_;
};

} // namespace

// STEP 1's TEST. n = 64, every row active at x*: the crash basis identifies
// 64 of the 64 rows of the analytic active set in the first subproblem's
// seed -- the FULL fraction, which is what this geometry supports -- and the
// solve's minor count collapses accordingly.
//
// THE MUTATION TARGET. This is the test that fails if crash seeding is
// disabled (crash_seeded_rows drops to 0 and the minor count reverts to the
// cold walk's), which is the full-suite mutation the task's brief requires.
TEST(SqpDriverCrashBasis, SeedsTheWholeAnalyticActiveSetAtABoundaryStart) {
    constexpr Index kN = 64;
    BoundaryStartModel model(kN, /*n_wrong=*/0);

    SqpOptions cold;
    cold.max_iter = 30;
    SqpDriver plain(cold);
    const SqpSolution off = plain.solve(model);

    SqpOptions crash = cold;
    crash.crash_basis = true;
    SqpDriver seeded(crash);
    const SqpSolution on = seeded.solve(model);

    ASSERT_EQ(off.status, SqpStatus::kOptimal);
    ASSERT_EQ(on.status, SqpStatus::kOptimal);
    // Same answer, to the solver's own tolerance -- the lever must not move
    // WHERE the solve lands, only how much it costs to get there.
    EXPECT_LT((off.x - model.x_star()).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_LT((on.x - model.x_star()).lpNorm<Eigen::Infinity>(), 1e-8);

    // THE FRACTION THE BRIEF ASKS FOR: 64 of 64 analytic active rows seeded.
    EXPECT_EQ(off.counters.crash_seeded_rows, 0);
    EXPECT_EQ(off.counters.crash_seeded_bounds, 0);
    EXPECT_EQ(on.counters.crash_seeded_rows, kN);
    EXPECT_EQ(on.counters.crash_seeded_bounds, 0) << "this fixture has no finite bounds";

    // AND THE POINT OF SEEDING THEM: the walk does not have to find them one
    // ratio test at a time. OBSERVED (MKL Pardiso, clang++, this machine, and
    // the value the RecordProperty below actually carries): 65 minors off,
    // 1 on -- a 65x cut. (Fix round 1: this comment said "66 off, 2 on", which
    // was an estimate written before the fixture was run and never corrected
    // against the recorded property.)
    EXPECT_LT(on.counters.qp_minor_iters, off.counters.qp_minor_iters / 4)
        << "off=" << off.counters.qp_minor_iters << " on=" << on.counters.qp_minor_iters;
    RecordProperty("crash_boundary_minors", fmt::format("off={} on={}", off.counters.qp_minor_iters,
                                                        on.counters.qp_minor_iters));
}

// THE OTHER DIRECTION, MEASURED RATHER THAN ASSUMED. Half the rows the start
// point sits on are STRICTLY SLACK at x*, so half the seed is wrong. Task
// 3-P5's Sec. 7.8(a) predicts this costs strictly more than no seed at all
// (a wrongly seeded row can only leave through the drop rule), and it does.
// The row is reported, not excluded -- the task's own honest-outcome rule.
TEST(SqpDriverCrashBasis, AnOverGenerousSeedCostsMoreThanNoSeed) {
    constexpr Index kN = 64;
    BoundaryStartModel model(kN, /*n_wrong=*/kN / 2);

    SqpOptions cold;
    cold.max_iter = 30;
    SqpDriver plain(cold);
    const SqpSolution off = plain.solve(model);

    SqpOptions crash = cold;
    crash.crash_basis = true;
    SqpDriver seeded(crash);
    const SqpSolution on = seeded.solve(model);

    ASSERT_EQ(off.status, SqpStatus::kOptimal);
    ASSERT_EQ(on.status, SqpStatus::kOptimal);
    EXPECT_LT((off.x - model.x_star()).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_LT((on.x - model.x_star()).lpNorm<Eigen::Infinity>(), 1e-8);

    // All 64 rows are seeded; only 32 of them belong in the final set.
    EXPECT_EQ(on.counters.crash_seeded_rows, kN);
    // THE COST, ASSERTED IN THE DIRECTION THE MECHANISM PREDICTS. The seed
    // must be pruned back, and pruning is not free.
    EXPECT_GT(on.counters.qp_minor_iters, off.counters.qp_minor_iters)
        << "off=" << off.counters.qp_minor_iters << " on=" << on.counters.qp_minor_iters;
    RecordProperty(
        "crash_overgenerous_minors",
        fmt::format("off={} on={}", off.counters.qp_minor_iters, on.counters.qp_minor_iters));
}

// INGEST NON-INTERFERENCE. The seed is cold-only BY CONSTRUCTION (it is armed
// on `!warm_ingest`), so a WARM solve must be bit-identical with the lever on
// and off -- nothing about the Phase-5 Task 7b B-1 clear, the kWarm working-set
// ingest or the zero-major hand-off may move. Asserted on a real warm chain
// (solve, hand the warm object back, re-solve) rather than on a synthetic one.
TEST(SqpDriverCrashBasis, IsInertOnAWarmIngest) {
    const HsProblem p = make_hs(33); // one of the four HS starts that DO seed
    SqpOptions base;
    base.max_iter = 60;

    SqpDriver first(base);
    const SqpSolution seedrun = first.solve(*p.model);
    ASSERT_TRUE(seedrun.warm_start.valid);

    SqpDriver warm_off(base);
    const SqpSolution off = warm_off.solve(*p.model, p.model->start_point(), seedrun.warm_start);

    SqpOptions crash = base;
    crash.crash_basis = true;
    SqpDriver warm_on(crash);
    const SqpSolution on = warm_on.solve(*p.model, p.model->start_point(), seedrun.warm_start);

    ASSERT_NE(off.counters.start_level_used, StartLevel::kCold)
        << "the fixture must actually warm-resolve, or this asserts nothing";
    EXPECT_EQ(on.counters.start_level_used, off.counters.start_level_used);
    EXPECT_EQ(on.counters.crash_seeded_rows, 0);
    EXPECT_EQ(on.counters.crash_seeded_bounds, 0);
    EXPECT_EQ(on.counters.major_iters, off.counters.major_iters);
    EXPECT_EQ(on.counters.qp_minor_iters, off.counters.qp_minor_iters);
    EXPECT_EQ(on.counters.factorizations, off.counters.factorizations);
    EXPECT_EQ(on.x, off.x); // bit-for-bit
    EXPECT_EQ(on.f, off.f);
}

// THE SEED BUILDER ITSELF, away from any engine: the three predicates are the
// driver's own geometric-activity tests read off the SUBPROBLEM, and an
// infinite bound is never seeded (it falls out of the comparisons rather than
// needing a separate test -- crash_basis_seed's own note).
TEST(SqpDriverCrashBasis, SeedBuilderReadsTheSubproblemsOwnGeometry) {
    QpProblem qp;
    qp.H = SpMatRM(3, 3);
    qp.g = Vec::Zero(3);
    qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 3);
    qp.be = Vec(0);
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(3, 3);
    qp.bi = Vec(3);
    //           on the boundary   strictly slack   violated
    qp.bi << 0.0, 1.0, -0.5;
    qp.lower = Vec(3);
    qp.upper = Vec(3);
    //            at lower   free (both sides infinite)   at upper
    qp.lower << 0.0, -kInfBound, -2.0;
    qp.upper << 3.0, kInfBound, 0.0;

    QpSolution seed;
    Index rows = -1, bounds = -1;
    const bool any = crash_basis_seed(qp, /*feas_tol=*/1e-6, seed, rows, bounds);

    EXPECT_TRUE(any);
    EXPECT_EQ(rows, 2) << "the boundary row and the VIOLATED row, not the slack one";
    EXPECT_EQ(bounds, 2);
    EXPECT_TRUE(seed.ineq_active[0]);
    EXPECT_FALSE(seed.ineq_active[1]);
    EXPECT_TRUE(seed.ineq_active[2]);
    EXPECT_EQ(seed.bound_state[0], BoundState::kAtLower);
    EXPECT_EQ(seed.bound_state[1], BoundState::kFree);
    EXPECT_EQ(seed.bound_state[2], BoundState::kAtUpper);
    EXPECT_EQ(seed.x, Vec::Zero(3)) << "the seed is a claim about the working SET, never a point";

    // AND THE EMPTY CASE, which the driver relies on to keep the cold path's
    // call literally unchanged: nothing near anything seeds nothing.
    qp.bi << 1.0, 1.0, 1.0;
    qp.lower << -1.0, -kInfBound, -2.0;
    qp.upper << 3.0, kInfBound, 4.0;
    EXPECT_FALSE(crash_basis_seed(qp, 1e-6, seed, rows, bounds));
    EXPECT_EQ(rows, 0);
    EXPECT_EQ(bounds, 0);
}

// =========================================================================
// PHASE-6 TASK 5 x TASK 4: THE CRASH BASIS'S INTERACTION WITH THE kSeeded
// INGEST LEVEL -- the ruling sqp_driver.h records at its seed-building block.
//
// THE QUESTION. Through Phase 5 the crash basis was armed on `!warm_ingest`,
// i.e. "cold only", and that was unambiguous because every ingest built a
// working-set seed. kSeeded breaks the equivalence: a seeded object is an
// INGEST, but it may carry NO ACTIVITY HINT AT ALL -- warm_start.h states that
// an all-zero ineq_active/bound_active means "no activity was attributed", NOT
// "nothing is active", and the two producers kSeeded exists for
// (mesh_transfer.h, from_interior_point) can both emit exactly that (a transfer
// of a zero-major solve's hand-off; a crossover whose every row was ambiguous).
//
// THE RULING: AT kSeeded AN EMPTY HINT IS NO HINT. The first subproblem is left
// unseeded and SqpOptions::crash_basis, if enabled, may seed it -- a guess
// derived from the real first linearization being strictly better information
// than an empty working set asserted as fact. A seeded object that DOES carry a
// hint suppresses the crash basis exactly as a kWarm one does; the two seeds
// are mutually exclusive by construction and neither can displace the other.
// kCold/kWarm/kHot behaviour is byte-identical to Task 4's either way.
//
// THE FIXTURE is Task 4's own BoundaryStartModel at n = 64, whose every row is
// active at x* and whose start point sits ON all 64 of them -- the geometry
// that makes the crash basis's contribution unmistakable (64 seeded rows, a
// 65x minor cut) rather than a one-or-two-row nudge.
// =========================================================================
TEST(SqpDriverCrashBasis, SeededIngestWithNoActivityHintStillArmsTheCrashBasis) {
    constexpr Index kN = 64;
    BoundaryStartModel model(kN, /*n_wrong=*/0);

    SqpOptions crash;
    crash.max_iter = 30;
    crash.crash_basis = true;

    // A hash-less, dimensionally compatible object at the same start point,
    // carrying duals but an ALL-FREE working set: an ingest with nothing to say
    // about activity.
    WarmStart hintless;
    hintless.x = model.start_point();
    hintless.lambda_e = Vec(0);
    hintless.lambda_i = Vec::Zero(kN);
    hintless.z = Vec::Zero(kN);
    hintless.ineq_active.assign(static_cast<std::size_t>(kN), 0);
    hintless.bound_active.assign(static_cast<std::size_t>(kN), 0);
    hintless.qp_working_set = WorkingSet(kN, kN);
    hintless.structure_hash = 0;
    hintless.valid = true;

    SqpDriver seeded_driver(crash);
    const SqpSolution seeded = seeded_driver.solve(model, model.start_point(), hintless);
    ASSERT_EQ(seeded.counters.start_level_used, StartLevel::kSeeded);
    ASSERT_EQ(seeded.status, SqpStatus::kOptimal);
    EXPECT_EQ(seeded.counters.crash_seeded_rows, kN)
        << "THE RULING, PINNED: an all-free working set is NO HINT, so the crash basis is the "
           "only activity guess available and it fires -- all 64 rows, exactly as on a cold solve";
    EXPECT_EQ(seeded.counters.crash_seeded_bounds, 0) << "this fixture has no finite bounds";

    // AND THE OTHER HALF: a seeded object that DOES pin rows suppresses it.
    // Same model, same lever; only the hint differs.
    WarmStart hinted = hintless;
    for (Index j = 0; j < kN; ++j) {
        hinted.ineq_active[static_cast<std::size_t>(j)] = 1;
        hinted.qp_working_set.add_ineq(j);
    }
    SqpDriver hinted_driver(crash);
    const SqpSolution with_hint = hinted_driver.solve(model, model.start_point(), hinted);
    ASSERT_EQ(with_hint.counters.start_level_used, StartLevel::kSeeded);
    ASSERT_EQ(with_hint.status, SqpStatus::kOptimal);
    EXPECT_EQ(with_hint.counters.crash_seeded_rows, 0);
    EXPECT_EQ(with_hint.counters.crash_seeded_bounds, 0)
        << "THE PIN: a seeded object carrying a hint suppresses the crash basis, exactly as a "
           "kWarm one does";

    // Both land on the same answer, and both land there cheaply -- the ingested
    // hint and the crashed one describe the same active set here, which is why
    // this fixture can compare them at all.
    EXPECT_LT((seeded.x - model.x_star()).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_LT((with_hint.x - model.x_star()).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_EQ(seeded.counters.qp_minor_iters, with_hint.counters.qp_minor_iters)
        << "the crashed seed and the ingested hint agree on this geometry, so they cost the same";
}

// =====================================================================
// PHASE-7 TASK 5 -- THE SEMISMOOTH-NEWTON QP MODE
// =====================================================================
//
// SqpOptions::qp_mode selects which kernel solves each subproblem. Everything
// below is about kSsn; the SHIPPED DEFAULT is kWalk and is untouched, which the
// whole rest of this file (and every other suite) asserts by continuing to
// pass byte-identically.
//
// WHAT THESE ARMS PIN, IN ORDER OF AUTHORITY:
//
//   1. THE CERTIFICATES MUST MATCH. A problem the walk certifies must be
//      certified under kSsn at the same tolerance and at the same answer.
//      Trajectories MAY differ; certificates may not.
//   2. THE ROUTING. Every SSN escape hands the subproblem to the walk, once,
//      and the walk finishes it -- including through the elastic tier.
//   3. THE ACCOUNTING. SSN work is reported in SqpCounters::ssn and in
//      `factorizations`; it is NEVER folded into qp_minor_iters, which is the
//      currency every published figure in this repository is quoted in.
//
// ============ MKL-OBSERVED (clang++, Release). NOT YET RE-VERIFIED ON APPLE
// ACCELERATE. ============ Every count below is an observed value on this
// project's authoritative configuration. The counts are properties of a Newton
// trajectory and of an active-set walk, so a backend that rounds a
// factorization differently could land elsewhere near a tolerance boundary;
// the STATUS and ANSWER assertions are backend-independent, the COUNT
// assertions are the ones a Mac pass must re-derive. Flagged on
// docs/notes/2026-07-28-accelerate-audit-checklist.md.

namespace {

// An NLP whose subproblem is EXACTLY Task 4's `indefinite_qp` fixture
// (tests/sqp/support/ssn_fixtures.h), lifted to the driver:
//
//     min  1/2 x0^2 - x1^2 - c x0 + 1/4 x1     on [-1, 1]^2
//
// WHY THIS ONE AND NOT ANOTHER. It is the sharpest escape fixture Task 4
// found, and its sharpness is a THEOREM rather than an observation: `F`
// vanishes at BOTH (c, -1) -- the solution, objective -1.375 at c = 0.5,
// reached by riding negative curvature to the bound -- and at the interior
// stationary point (c, 1/8), objective -0.109, which is a SADDLE of the same
// KKT system. No residual-based test can separate them, so the SSN kernel's
// inertia gate refuses to certify and escapes instead
// (docs/notes/2026-08-07-ssn-safeguards.md section 3). That makes it the one
// fixture where "the escape routes to the walk and the walk gets the RIGHT
// answer" is a claim with teeth: a driver that consumed the escaped iterate
// would land on the saddle and report -0.109.
//
// `c` parameterizes it into a family so a continuation chain can be built
// (the proximal-carry arms below); c = 0.5 is Task 4's own instance.
class IndefiniteBoxModel : public NlpModel {
  public:
    explicit IndefiniteBoxModel(double c = 0.5) : c_(c) {}

    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return 0.5 * x(0) * x(0) - x(1) * x(1) - c_ * x(0) + 0.25 * x(1);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << x(0) - c_, -2.0 * x(1) + 0.25;
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = -2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(2); }

    // The analytic answer: ride the negative curvature in x1 to its bound.
    Vec x_star() const {
        Vec x(2);
        x << c_, -1.0;
        return x;
    }
    double f_star() const { return 0.5 * c_ * c_ - 1.0 - c_ * c_ - 0.25; }
    // The SADDLE the residual cannot tell from the solution -- what a driver
    // that consumed an escaped iterate would report instead.
    double f_saddle() const { return 0.5 * c_ * c_ - 1.0 / 64.0 - c_ * c_ + 0.25 / 8.0; }

  private:
    double c_ = 0.5;
    Vec lower_ = Vec::Constant(2, -1.0);
    Vec upper_ = Vec::Constant(2, 1.0);
};

SqpOptions ssn_mode_options(Index max_iter = 60) {
    SqpOptions opts;
    opts.max_iter = max_iter;
    opts.qp_mode = QpMode::kSsn;
    return opts;
}

} // namespace

// =====================================================================
// (a) THE CORPUS ARM. Every Hock-Schittkowski problem the walk certifies is
// certified under kSsn, at the same status and the same point.
//
// THE OBSERVED RESULT IS STRONGER THAN THE BRIEF ASKS FOR, and the test
// asserts the strong form because it is what happens and because a weakening
// would hide a regression: the two modes agree on the MAJOR COUNT of all 27
// problems, exactly. That is not luck. A convex QP has a unique solution, so
// on every subproblem both kernels return the SAME step -- the walk by walking
// to it, the SSN by Newton -- and the driver's trajectory (which reads only
// the step) is therefore identical. Where the SSN escapes, the walk re-solves
// and supplies its own answer, so the trajectory is identical there too. The
// modes differ in COST, not in path, and that is exactly the claim Phase 7
// wants to be able to make.
// =====================================================================
TEST(SqpDriverSsnMode, EveryHsProblemTheWalkCertifiesIsCertifiedUnderKSsn) {
    using hven::solvers::test_support::hs_numbers;
    using hven::solvers::test_support::make_hs;

    Index problems_with_an_escape = 0;
    Index problems_with_a_prox_climb = 0;
    Index problems_with_a_refinement = 0;
    Index problems_with_a_refusal = 0;
    for (int number : hs_numbers()) {
        SCOPED_TRACE(fmt::format("HS{}", number));

        auto walk_problem = make_hs(number);
        SqpOptions walk_opts;
        walk_opts.max_iter = 60;
        SqpDriver walk_driver(walk_opts);
        const SqpSolution walk = walk_driver.solve(*walk_problem.model);
        ASSERT_EQ(walk.status, SqpStatus::kOptimal) << "battery premise";

        auto ssn_problem = make_hs(number);
        SqpDriver ssn_driver(ssn_mode_options());
        const SqpSolution ssn = ssn_driver.solve(*ssn_problem.model);

        // 1. THE CERTIFICATE.
        EXPECT_EQ(ssn.status, SqpStatus::kOptimal)
            << "a problem the walk certifies must certify under kSsn too";
        // 2. THE CERTIFICATE IS REAL, verified against the MODEL rather than
        // against the driver's own residual -- the battery's own primary
        // guard (tests/test_hs_battery.cpp), applied at its own bars. This is
        // the assertion that actually carries the brief's "certifies at the
        // same tolerance"; a mode that certified at a non-KKT point would fail
        // here even if its status and objective matched.
        const test_support::NlpKktResidual r = self_check_kkt(*ssn_problem.model, ssn, 1e-6);
        EXPECT_LT(r.stationarity, 1e-6);
        EXPECT_LT(r.primal, 1e-6);
        EXPECT_LT(r.dual_sign, 1e-9);
        EXPECT_LT(r.complementarity, 1e-6);
        // 3. THE ANSWER, compared on the OBJECTIVE rather than on x. Both arms
        // are kOptimal at kkt_tol = 1e-6, and on a flat problem two points
        // that far apart in x can be that close in f: HS3 (f = x2 + 1e-5
        // (x2 - x1)^2, whose x1 is almost free near the optimum) separates the
        // two arms by 1.5e-03 in x while agreeing to 2.2e-11 in f. So x is not
        // the right comparand here; f and the self-check above are.
        EXPECT_LT(std::abs(ssn.f - walk.f), 1e-6 * std::max(1.0, std::abs(walk.f)));
        // 4. THE TRAJECTORY IDENTITY -- see this test's banner for why it
        // holds rather than merely happening to.
        EXPECT_EQ(ssn.counters.major_iters, walk.counters.major_iters)
            << "both kernels solve the same convex subproblem to the same unique step, so the "
               "driver's path cannot differ";

        // 4b. PHASE-7 TASK 6b (docket D6) -- THE ESCAPE-REASON CENSUS
        // PARTITIONS THE ESCAPE COUNT, asserted on all 27 problems rather than
        // on one fixture. This is the assertion that catches a NEW hand-off
        // route added without a census bucket, and specifically the one that
        // would catch the driver's own `ssn_escape_gate_refused` write going
        // missing on any problem that reaches it -- see this task's report for
        // why no shipped fixture reaches it today.
        EXPECT_EQ(ssn.counters.ssn.ssn_escape_budget + ssn.counters.ssn.ssn_escape_singular +
                      ssn.counters.ssn.ssn_escape_no_contraction +
                      ssn.counters.ssn.ssn_escape_infeasible_suspect +
                      ssn.counters.ssn.ssn_escape_indefinite +
                      ssn.counters.ssn.ssn_escape_gate_refused,
                  ssn.counters.ssn.ssn_escapes)
            << "the six census buckets must partition the hand-off count";

        // 5. THE ACCOUNTING RULE, asserted per problem: a subproblem the SSN
        // certified costs ZERO walk minors, so qp_minor_iters under kSsn can
        // only ever be the WALK's share -- never more than the pure-walk arm.
        EXPECT_LE(ssn.counters.qp_minor_iters, walk.counters.qp_minor_iters)
            << "SSN steps are never folded into the walk's currency";
        // THE FOUR WAYS THE WALK CAN RUN UNDER kSsn, and there are exactly
        // four: a hand-off, a second-order-correction re-solve, an elastic
        // rung, or the RESTORATION phase's own sub-solve. The last three are
        // walk-only BY DESIGN (sqp_driver.h's WHAT IS NOT ROUTED THROUGH SSN),
        // which is why they belong in this disjunction -- HS77 is the corpus
        // row that needs the SOC one: 0 escapes, and still 4 walk minors, every
        // one of them an SOC re-solve.
        //
        // FIX ROUND 1 ADDED RESTORATION, which the original enumeration missed
        // (opus review I-3). It happens to fire on no corpus row, so the branch
        // below passed anyway -- but as a general rule it was UNSOUND, and this
        // assertion is exactly the one a Task-6 reader will port to the scale
        // corpus, where restoration does fire.
        const bool the_walk_could_have_run =
            ssn.counters.ssn.ssn_escapes > 0 || ssn.counters.soc_steps > 0 ||
            ssn.counters.elastic_activations > 0 || ssn.counters.restoration_iters > 0;
        if (!the_walk_could_have_run) {
            EXPECT_EQ(ssn.counters.qp_minor_iters, 0)
                << "no hand-off, no SOC, no elastic rung and no restoration means the walk never "
                   "ran at all";
        }
        // 6. THE TIER-3 STABLE-FACE REFINEMENT (fix round 1). Every CERTIFYING
        // SSN exit is offered an exact solve on the face it identified, so the
        // two outcomes must account for every such exit; a solve that certified
        // nothing (HS25 converges in zero majors) offers none.
        if (ssn.counters.ssn.ssn_refinements > 0) {
            ++problems_with_a_refinement;
        }
        if (ssn.counters.ssn.ssn_refine_refused > 0) {
            ++problems_with_a_refusal;
        }
        EXPECT_LE(ssn.counters.ssn.ssn_refinements + ssn.counters.ssn.ssn_refine_refused,
                  ssn.counters.factorizations)
            << "each refinement attempt costs one factorization, folded like every other";
        if (ssn.counters.ssn.ssn_escapes > 0) {
            EXPECT_GT(ssn.counters.qp_minor_iters, 0)
                << "a hand-off means the walk ran and the minors it spent are counted";
            ++problems_with_an_escape;
        }
        if (ssn.counters.ssn.ssn_prox_updates > 0) {
            ++problems_with_a_prox_climb;
        }
        // 5. THE SSN COUNTERS ARE LIVE at all, on a solve that solved anything.
        if (walk.counters.major_iters > 0) {
            EXPECT_GT(ssn.counters.ssn.ssn_iters + ssn.counters.ssn.ssn_escapes, 0);
        }
    }

    // THE CORPUS IS A REAL EXERCISE OF BOTH PATHS, pinned so that a change
    // which quietly stopped escaping (or quietly stopped certifying) is caught
    // even though every per-problem assertion above still passes. OBSERVED:
    // 15 of 27 problems escape at least once, 15 climb the proximal ladder.
    EXPECT_GE(problems_with_an_escape, 10)
        << "the escape route must be exercised by the corpus, not merely reachable";
    EXPECT_LE(problems_with_an_escape, 20);
    EXPECT_GE(problems_with_a_prox_climb, 10);
    // THE REFINEMENT TIER IS EXERCISED IN BOTH POLARITIES BY THE CORPUS, which
    // is what keeps the gate in QpEngine::refine_on_face from being a branch
    // only the hand-built engine fixtures ever drive. OBSERVED: 24 of 27
    // problems take at least one refinement, 12 of 27 have at least one refused
    // (a refusal is not an error -- the caller keeps the certificate the SSN
    // tier already gave it; see SsnCounters).
    EXPECT_GE(problems_with_a_refinement, 18)
        << "the refinement must be exercised by the corpus, not merely reachable";
    EXPECT_GE(problems_with_a_refusal, 5)
        << "and so must its refusal -- a corpus that never refuses cannot detect a gate that "
           "stopped refusing";
}

// =====================================================================
// (a2) THE SECOND-ORDER CORRECTION IS REACHABLE UNDER kSsn, AND IS SEEDED
// FROM THE SSN's OWN SOLUTION -- the coverage assertion the original
// deviation D5 asserted the OPPOSITE of (opus review I-1). D5 said each
// walk-only rescue path "is hot-started off the immediately preceding WALK
// solve"; for the SOC that is factually wrong, because `seed_soc = qs` and
// `qs` may be the SSN's exported QpSolution. The BEHAVIOUR is sound -- the
// walk's seed ingest consumes exactly bound_state/ineq_active/duals, all of
// which the SSN exports under the shared conventions -- and this is GOOD news
// for Task 6's fairness, because it means there is no capability gap to
// disclose: SOC is available in both modes. It is asserted here rather than
// left to the corpus arm's disjunction, which only says the walk COULD have
// run.
// =====================================================================
TEST(SqpDriverSsnMode, TheSecondOrderCorrectionIsReachableUnderKSsnFromAnSsnSeed) {
    using hven::solvers::test_support::make_hs;

    auto walk_problem = make_hs(77);
    SqpOptions walk_opts;
    walk_opts.max_iter = 60;
    SqpDriver walk_driver(walk_opts);
    const SqpSolution walk = walk_driver.solve(*walk_problem.model);
    ASSERT_EQ(walk.status, SqpStatus::kOptimal);
    ASSERT_GT(walk.counters.soc_steps, 0) << "fixture premise: HS77 takes SOC re-solves";

    auto ssn_problem = make_hs(77);
    SqpDriver ssn_driver(ssn_mode_options());
    const SqpSolution ssn = ssn_driver.solve(*ssn_problem.model);
    ASSERT_EQ(ssn.status, SqpStatus::kOptimal);

    EXPECT_EQ(2, ssn.counters.soc_steps)
        << "OBSERVED. MKL, clang++, Release. SOC is REACHED under kSsn -- a mode in which the "
           "correction were unreachable would report 0 here";
    EXPECT_EQ(0, ssn.counters.ssn.ssn_escapes)
        << "AND THE SEED IS THE SSN's OWN: with no hand-off on this problem, every subproblem "
           "`qs` an SOC re-solve is seeded from came from ssn_result_to_qp_solution";
    EXPECT_GT(ssn.counters.qp_minor_iters, 0)
        << "the SOC re-solves are walk solves and their minors are counted as such";
}

// =====================================================================
// (d) THE ESCAPE ROUTING, on the fixture where getting it wrong is a WRONG
// ANSWER rather than a slow one. See IndefiniteBoxModel's banner.
// =====================================================================
TEST(SqpDriverSsnMode, AnEscapedSubproblemLandsInTheWalkAndStillCertifies) {
    IndefiniteBoxModel model;
    // THE FIXTURE'S OWN PREMISE, asserted rather than assumed: the walk finds
    // the solution by riding negative curvature, and the saddle is a
    // materially different objective -- so "which of the two did we land on"
    // is a question this test can actually answer.
    ASSERT_NEAR(model.f_star(), -1.375, 1e-12);
    ASSERT_NEAR(model.f_saddle(), -0.109375, 1e-12);

    SqpOptions walk_opts;
    SqpDriver walk_driver(walk_opts);
    const SqpSolution walk = walk_driver.solve(model);
    ASSERT_EQ(walk.status, SqpStatus::kOptimal);
    ASSERT_NEAR(walk.f, model.f_star(), 1e-12);

    SqpDriver ssn_driver(ssn_mode_options());
    const SqpSolution ssn = ssn_driver.solve(model);

    // THE ESCAPE HAPPENED. Without it the kernel would have to certify at one
    // of the two roots of F, and the inertia gate is what stops it certifying
    // the wrong one.
    EXPECT_EQ(ssn.counters.ssn.ssn_escapes, 1);
    EXPECT_GT(ssn.counters.ssn.ssn_iters, 0) << "it did real work before giving up";
    EXPECT_EQ(ssn.counters.ssn.ssn_prox_updates, 7)
        << "the full proximal ladder, 1e-6 -> 1e6 in seven rungs, climbed and exhausted";

    // THE WALK TOOK OVER AND FINISHED THE JOB -- and finished it at the SAME
    // cost as the pure-walk arm, which is what "one hand-off, then the walk
    // owns it" means in counters.
    EXPECT_EQ(ssn.counters.qp_minor_iters, walk.counters.qp_minor_iters);
    EXPECT_EQ(ssn.counters.major_iters, walk.counters.major_iters);
    EXPECT_EQ(ssn.status, SqpStatus::kOptimal);
    EXPECT_NEAR(ssn.f, model.f_star(), 1e-12);
    EXPECT_LT((ssn.x - model.x_star()).lpNorm<Eigen::Infinity>(), 1e-12);
    // AND THE NEGATIVE: it is NOT the saddle. This is the assertion the whole
    // fixture exists for -- an implementation that handed the funnel the
    // escaped iterate would land here.
    EXPECT_GT(std::abs(ssn.f - model.f_saddle()), 1.0);
}

// =====================================================================
// (e) AN INFEASIBLE LINEARIZATION UNDER kSsn REACHES THE ELASTIC LADDER.
//
// THE POINT OF THIS ARM IS THE MECHANISM, not just the destination. The SSN
// kernel cannot certify infeasibility -- `SsnEscape::kInfeasibleSuspect` is a
// diagnosis from behaviour and, since Task 4's fix round 1 traded recall for
// precision, an infeasible subproblem escapes `kNoContraction`/`kBudget` more
// often than it escapes `kInfeasibleSuspect` at all. So the driver does NOT
// route a suspicion to the elastic tier. It routes the ESCAPE to the walk; the
// walk returns its own `QpStatus::kInfeasible` CERTIFICATE; and the elastic
// tier fires on that, exactly as it always has. Same destination, better
// authority -- see sqp_driver.h's THE ROUTING RULE, justification (3).
// =====================================================================
TEST(SqpDriverElastic, InconsistentLinearizationRecoversUnderKSsn) {
    InconsistentLinearizationModel model;
    SqpOptions walk_opts;
    SqpDriver walk_driver(walk_opts);
    const SqpSolution walk = walk_driver.solve(model);
    ASSERT_EQ(walk.status, SqpStatus::kOptimal) << "fixture premise (the kWalk arm above)";

    SqpDriver ssn_driver(ssn_mode_options());
    const SqpSolution ssn = ssn_driver.solve(model);

    // THE SSN TIER ESCAPED on the infeasible subproblem and handed it over.
    EXPECT_EQ(ssn.counters.ssn.ssn_escapes, 1);
    // AND THE LADDER RAN, with the same shape the kWalk arm pins: one
    // activation, and the relaxation never closes, so all six escalations of
    // 1e2 -> 1e8 are spent.
    EXPECT_EQ(ssn.counters.elastic_activations, 1);
    EXPECT_EQ(ssn.counters.elastic_escalations, 6);
    EXPECT_EQ(ssn.counters.elastic_activations, walk.counters.elastic_activations);
    EXPECT_EQ(ssn.counters.elastic_escalations, walk.counters.elastic_escalations);

    EXPECT_EQ(ssn.status, SqpStatus::kOptimal);
    EXPECT_EQ(ssn.counters.major_iters, 2);
    EXPECT_NEAR(ssn.x(0), 2.0, 1e-7);
    EXPECT_NEAR(ssn.x(1), 0.0, 1e-7);
    EXPECT_NEAR(ssn.f, 0.0, 1e-12);
}

// =====================================================================
// THE USABILITY GATE, driven directly in both polarities on a hand-built
// SsnResult.
//
// WHY DIRECTLY. Two of the three conjuncts of `ssn_exit_is_a_usable_step` are
// reachable from an NLP fixture (the arms above drive them); the THIRD -- the
// trust-region bound -- provably is not, because ssn_engine.h's own derivation
// says a certifying exit's `tr_violation` is bounded by
// `kSsnComplementarityFactor * fb_tol`. A gate no fixture can drive is either
// dead code or an untested guard; making it a free predicate is what turns it
// into neither. See sqp_driver.h's kSsnTrViolationFactor note.
// =====================================================================
TEST(SqpDriverSsnMode, TheUsabilityGateRefusesEveryEscapeAndAnOutOfRegionCertificate) {
    const double fb_tol = 1e-6;
    SsnResult certifying;
    certifying.status = QpStatus::kOptimal;
    certifying.escape_reason = SsnEscape::kNone;
    certifying.x = Vec::Zero(2);
    certifying.tr_violation = 0.0;
    EXPECT_TRUE(ssn_exit_is_a_usable_step(certifying, fb_tol))
        << "the only exit whose x may be used as a step";

    // ALL FIVE ESCAPES, ENUMERATED. The list is written out rather than
    // looped over a range so that a SIXTH escape added to ssn_engine.h fails
    // to compile this test's switch-free enumeration only by being absent from
    // it -- which is the reminder a future task wants.
    for (const SsnEscape escape :
         {SsnEscape::kBudget, SsnEscape::kSingular, SsnEscape::kNoContraction,
          SsnEscape::kInfeasibleSuspect, SsnEscape::kIndefinite}) {
        SsnResult escaped = certifying;
        escaped.escape_reason = escape;
        EXPECT_FALSE(ssn_exit_is_a_usable_step(escaped, fb_tol))
            << "escape reason " << static_cast<int>(escape)
            << " must route to the walk, like every other";
    }

    // THE STATUS CONJUNCT, on its own: an exit that forgot to set an escape
    // but did not reach kOptimal is still refused.
    SsnResult no_escape_but_not_optimal = certifying;
    no_escape_but_not_optimal.status = QpStatus::kMaxIter;
    EXPECT_FALSE(ssn_exit_is_a_usable_step(no_escape_but_not_optimal, fb_tol));

    // THE TRUST-REGION CONJUNCT, both sides of its threshold. The bound is
    // 2 * kSsnComplementarityFactor = 3.414... times fb_tol.
    SsnResult inside = certifying;
    inside.tr_violation = kSsnTrViolationFactor * fb_tol * 0.999;
    EXPECT_TRUE(ssn_exit_is_a_usable_step(inside, fb_tol));
    SsnResult outside = certifying;
    outside.tr_violation = kSsnTrViolationFactor * fb_tol * 1.001;
    EXPECT_FALSE(ssn_exit_is_a_usable_step(outside, fb_tol))
        << "a step further outside the region than a certificate permits is not a step; "
           "ssn_engine.h measured an ESCAPED x at 133x-160x the radius";
    // AND THE SCALE-FREENESS OF THE BOUND: it is stated in units of fb_tol,
    // so the same violation that passes at a loose tolerance fails at a tight
    // one. This is what makes the 1e-10 finish safe.
    EXPECT_FALSE(ssn_exit_is_a_usable_step(inside, 1e-10));

    SsnResult non_finite = certifying;
    non_finite.x = Vec::Constant(2, std::numeric_limits<double>::quiet_NaN());
    EXPECT_FALSE(ssn_exit_is_a_usable_step(non_finite, fb_tol));
}

// =====================================================================
// THE 1e-10 FINISH IS REACHABLE FROM A kSsn-CERTIFIED SUBPROBLEM EXACTLY AS
// FROM A WALK-SOLVED ONE -- the phase's global constraint, asserted.
//
// THE MECHANISM, so the assertion is not a coincidence: SqpDriver::ssn_options
// derives the kernel's own `fb_tol` from `SqpOptions::kkt_tol` through
// ssn_engine.h's `ssn_fb_tol_from_kkt_tol`, so tightening the driver's
// tolerance tightens the subproblem kernel with it. A wiring that hard-coded
// `fb_tol` at its 1e-6 default would leave the SSN certifying subproblems four
// orders looser than the driver's own test demands, and the solve would stall
// (or, worse, certify) at the wrong residual.
// =====================================================================
TEST(SqpDriverSsnMode, TheTightFinishIsReachedUnderKSsnExactlyAsUnderTheWalk) {
    using hven::solvers::test_support::make_hs;
    using hven::solvers::test_support::self_check_kkt;

    for (int number : {7, 76, 40, 79}) {
        SCOPED_TRACE(fmt::format("HS{}", number));
        for (const auto mode : {QpMode::kWalk, QpMode::kSsn}) {
            SCOPED_TRACE(mode == QpMode::kWalk ? "walk" : "ssn");
            auto problem = make_hs(number);
            SqpOptions opts;
            opts.kkt_tol = 1e-10;
            opts.feas_tol = 1e-10;
            opts.max_iter = 60;
            opts.qp_mode = mode;
            SqpDriver driver(opts);
            const SqpSolution sol = driver.solve(*problem.model);

            ASSERT_EQ(sol.status, SqpStatus::kOptimal)
                << "the 1e-10 finish must be REACHABLE, not merely requested";
            // Verified against the MODEL, not against the driver's own
            // residual -- the battery's standing rule.
            const test_support::NlpKktResidual r = self_check_kkt(*problem.model, sol, 1e-10);
            EXPECT_LT(r.stationarity, 1e-8);
            EXPECT_LT(r.primal, 1e-9);
            EXPECT_LT(r.complementarity, 1e-8);
            EXPECT_NEAR(sol.f, problem.f_star, 1e-9 * std::max(1.0, std::abs(problem.f_star)));
        }
    }
}

// =====================================================================
// THE ADAPTIVE-mu SCHEDULE IS OFF UNDER kSsn, and ON under kWalk.
//
// Both halves are asserted on the SAME model with the SAME options object, so
// the only difference between the two rows is the mode. `SqpIterate::mu` is
// the schedule's own diagnostic (sqp_types.h) and reports the EFFECTIVE
// dual_mu each trial was solved at, which is exactly the quantity the ruling
// is about. See sqp_driver.h's WHY THE ADAPTIVE-mu SCHEDULE IS OFF UNDER kSsn.
// =====================================================================
TEST(SqpDriverSsnMode, TheAdaptiveMuScheduleIsDisabledUnderKSsn) {
    using hven::solvers::test_support::make_hs;

    // HS26 IS THE FIXTURE BECAUSE THE SCHEDULE ACTUALLY MOVES ON IT. Measured
    // over the whole corpus with the lever on, `SqpIterate::mu` leaves its
    // 1e-8 default on exactly three problems -- HS26, HS30 and HS77 -- because
    // kAdaptiveMuMax IS QpOptions::dual_mu's own default, so the schedule is
    // only VISIBLE once a major's residual is small enough to quantize below
    // it. A fixture where mu never moves would assert nothing in either arm.
    auto walk_problem = make_hs(26);
    SqpOptions opts;
    opts.max_iter = 60;
    opts.adaptive_mu = true;
    SqpDriver walk_driver(opts);
    const SqpSolution walk = walk_driver.solve(*walk_problem.model);
    ASSERT_GT(walk.history.size(), 1u);
    bool walk_mu_ever_differs = false;
    for (const SqpIterate &row : walk.history) {
        if (row.qp_solved && row.mu != opts.qp.dual_mu) {
            walk_mu_ever_differs = true;
        }
    }
    EXPECT_TRUE(walk_mu_ever_differs)
        << "fixture premise: with the lever on, the WALK's schedule really does move mu";

    auto ssn_problem = make_hs(26);
    SqpOptions ssn_opts = opts;
    ssn_opts.qp_mode = QpMode::kSsn;
    SqpDriver ssn_driver(ssn_opts);
    const SqpSolution ssn = ssn_driver.solve(*ssn_problem.model);
    ASSERT_EQ(ssn.status, SqpStatus::kOptimal);
    for (const SqpIterate &row : ssn.history) {
        if (row.qp_solved) {
            EXPECT_DOUBLE_EQ(row.mu, ssn_opts.qp.dual_mu)
                << "THE RULING, PINNED: under kSsn every trial is solved at the caller's own "
                   "dual_mu, walk fall-backs included, and SqpIterate::mu says so";
        }
    }
}

// =====================================================================
// (c) THE PROXIMAL CARRY. Round-trip, gate, and the honest measurement.
//
// **THE LEVER SHIPS OFF, AND THIS TEST IS WHY.** SqpOptions::ssn_prox_carry
// carries a solve's proximal level into the FIRST SSN subproblem of the next
// one. The mechanism works and this test proves it moves the counter it is
// named for. The SWEEP behind the default (sqp_types.h's own note carries the
// full table) says it should not be on: `ssn_prox_updates` falls on 2 of 13
// measured rows, RISES on 1, and is unchanged on 10, while `ssn_escapes` rises
// on 8 of 13 -- i.e. its dominant measured effect is to damp the Newton step
// hard enough that the SSN tier gives up and the WALK does the work. That buys
// factorizations by disabling the kernel Phase 7 exists to use, so it is not a
// default. Recorded here in the same shape Task 4 recorded its own null result
// (docs/notes/2026-08-07-ssn-safeguards.md section 5): measured, reported, not
// papered over.
// =====================================================================
TEST(SqpDriverSsnMode, TheProximalCarryRoundTripsAndIsGatedOffByDefault) {
    // ONE SOLVE that exhausts the ladder, so there is something to carry.
    IndefiniteBoxModel first;
    SqpDriver first_driver(ssn_mode_options());
    const SqpSolution exporter = first_driver.solve(first);
    ASSERT_EQ(exporter.status, SqpStatus::kOptimal);

    // THE EXPORT SIDE IS UNCONDITIONAL -- it does not need the lever, so a
    // caller can measure the carry before deciding to use it.
    EXPECT_TRUE(exporter.warm_start.has_prox_center);
    EXPECT_DOUBLE_EQ(exporter.warm_start.prox_sigma, 1e6)
        << "detail::kSsnProxMax -- this subproblem climbed the whole ladder";
    // FIX ROUND 1: THE CENTRE IS EMPTY HERE, AND THAT IS THE REPAIR. This
    // fixture's only ladder climb ends in an ESCAPE, and an escaped iterate
    // certifies nothing (ssn_engine.h measures them at 133x-160x the trust
    // region). Before the repair the centre WAS that iterate, sized 2 and
    // shipped under a field documented as "the point it was reached at". The
    // LEVEL still carries -- it is provenance-free -- so nothing the carry is
    // read for is lost. TheCarriedProximalCentreIsNeverAnEscapedIterate has the
    // full statement and the certifying counterpart.
    EXPECT_EQ(exporter.warm_start.prox_center_x.size(), 0)
        << "the max-sigma subproblem escaped, so there is no certified point to carry";
    EXPECT_EQ(exporter.warm_start.prox_center_lambda.size(), 0);

    // AND IT IS GATED THE OTHER WAY at the shipped default: a kWalk solve
    // exports nothing at all, so no existing hand-off in this project grows a
    // byte.
    IndefiniteBoxModel walk_model;
    SqpOptions walk_opts;
    SqpDriver walk_driver(walk_opts);
    const SqpSolution walk = walk_driver.solve(walk_model);
    EXPECT_FALSE(walk.warm_start.has_prox_center);
    EXPECT_DOUBLE_EQ(walk.warm_start.prox_sigma, 0.0);
    EXPECT_EQ(walk.warm_start.prox_center_x.size(), 0);
}

TEST(SqpDriverSsnMode, TheProximalCarryLeverMovesTheLadderAndIsOffByDefault) {
    using hven::solvers::test_support::make_hs;

    // HS27 IS THE CELL THE LEVER MOVES, and it is chosen because it moves the
    // counter it is NAMED for -- `ssn_prox_updates`, 7 -> 0 -- with the
    // trajectory's own shape held fixed (escapes 1 -> 1). That is what makes it
    // a lever test rather than a trajectory comparison.
    //
    // FIX ROUND 1 CORRECTED WHAT THIS TEST USED TO CLAIM, and the correction is
    // worth reading before the numbers below. It used to assert that
    // FACTORIZATIONS did not move either (26 -> 26), and offered that as the
    // isolation entitling the prox_updates reading to be attributed to the
    // carry. That equality was an ARTEFACT of a counter defect: an ESCAPED SSN
    // subproblem's factorizations reached no accumulation site at all, so the
    // dominant cost of the carry-OFF arm was invisible. Charged honestly, the
    // lever moves factorizations 56 -> 33 -- and that fall is NOT a win, it is
    // the mechanism this lever ships OFF for, now visible in the counter
    // instead of hidden by it: starting at a large sigma damps the Newton step,
    // the SSN tier stops contracting and hands the subproblem to the WALK, so
    // the carry buys factorizations by doing less of the work Phase 7 exists to
    // move onto that tier.
    auto exporter_problem = make_hs(27);
    SqpDriver exporter_driver(ssn_mode_options());
    const SqpSolution exporter = exporter_driver.solve(*exporter_problem.model);
    ASSERT_EQ(exporter.status, SqpStatus::kOptimal);
    ASSERT_TRUE(exporter.warm_start.has_prox_center);
    ASSERT_DOUBLE_EQ(exporter.warm_start.prox_sigma, 1e6);

    // THE RECEIVING SOLVE. Its warm object is the exporter's, with `x` reset
    // to the published start point so the second solve does REAL WORK rather
    // than converging at the point it was handed (a converged hand-off builds
    // no subproblem, so no ladder can arm and no lever can move).
    auto receiver_problem = make_hs(27);
    WarmStart handoff = exporter.warm_start;
    handoff.x = receiver_problem.model->start_point();

    Index prox_updates[2];
    Index escapes[2];
    Index factorizations[2];
    for (int lever = 0; lever < 2; ++lever) {
        SCOPED_TRACE(lever == 0 ? "carry off (the shipped default)" : "carry on");
        SqpOptions opts = ssn_mode_options();
        opts.ssn_prox_carry = lever == 1;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*receiver_problem.model,
                                             receiver_problem.model->start_point(), handoff, 0);
        ASSERT_EQ(sol.counters.start_level_used, StartLevel::kWarm)
            << "the carry is hash-gated: it is only read at kWarm and above";
        ASSERT_EQ(sol.status, SqpStatus::kOptimal) << "the lever must not cost the certificate";
        prox_updates[lever] = sol.counters.ssn.ssn_prox_updates;
        escapes[lever] = sol.counters.ssn.ssn_escapes;
        factorizations[lever] = sol.counters.factorizations;
    }

    // THE LEVER MOVES, AND IN THE DIRECTION IT IS NAMED FOR.
    EXPECT_EQ(prox_updates[0], 7) << "carry OFF: the ladder is re-climbed from scratch";
    EXPECT_EQ(prox_updates[1], 0)
        << "carry ON: the first subproblem starts at the level the exporting solve reached, so "
           "there is no ladder left to climb";
    EXPECT_LT(prox_updates[1], prox_updates[0]);
    // THE TRAJECTORY'S SHAPE IS HELD FIXED, which is what entitles the reading
    // above to be attributed to the carry rather than to a different path.
    EXPECT_EQ(escapes[0], escapes[1]);
    // AND THE COST MOVES, in the direction the default rests on. See this
    // test's banner for why a fall here is evidence AGAINST the lever.
    EXPECT_EQ(56, factorizations[0]) << "OBSERVED. MKL, clang++, Release. Carry OFF.";
    EXPECT_EQ(33, factorizations[1])
        << "OBSERVED. MKL, clang++, Release. Carry ON -- cheaper, by declining to use the "
           "kernel.";

    // THE DEFAULT IS OFF, asserted as a value rather than left to the header:
    // a solve that never touches the flag reproduces the carry-OFF row.
    SqpOptions shipped = ssn_mode_options();
    EXPECT_FALSE(shipped.ssn_prox_carry);
    SqpDriver shipped_driver(shipped);
    const SqpSolution shipped_sol = shipped_driver.solve(
        *receiver_problem.model, receiver_problem.model->start_point(), handoff, 0);
    EXPECT_EQ(shipped_sol.counters.ssn.ssn_prox_updates, prox_updates[0]);
}

// THE COUNTER-EXAMPLE, COMMITTED. The lever is not merely unproven, it is
// measurably WRONG on at least one row -- HS15, where turning it on nearly
// doubles the ladder climbs it was supposed to remove. This is the evidence
// the default rests on, so it is a test rather than a sentence in a note.
TEST(SqpDriverSsnMode, TheProximalCarryHasACommittedCounterExample) {
    using hven::solvers::test_support::make_hs;

    auto exporter_problem = make_hs(15);
    SqpDriver exporter_driver(ssn_mode_options());
    const SqpSolution exporter = exporter_driver.solve(*exporter_problem.model);
    ASSERT_TRUE(exporter.warm_start.has_prox_center);

    auto receiver_problem = make_hs(15);
    WarmStart handoff = exporter.warm_start;
    handoff.x = receiver_problem.model->start_point();

    Index prox_updates[2];
    for (int lever = 0; lever < 2; ++lever) {
        SqpOptions opts = ssn_mode_options();
        opts.ssn_prox_carry = lever == 1;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*receiver_problem.model,
                                             receiver_problem.model->start_point(), handoff, 0);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        prox_updates[lever] = sol.counters.ssn.ssn_prox_updates;
    }
    EXPECT_EQ(prox_updates[0], 7);
    EXPECT_EQ(prox_updates[1], 13);
    EXPECT_GT(prox_updates[1], prox_updates[0])
        << "THE COUNTER-EXAMPLE THE DEFAULT RESTS ON: on this row the carry COSTS ladder "
           "climbs rather than saving them, which is why ssn_prox_carry ships off";
}

// =====================================================================
// THE LEDGER'S SSN COLUMNS.
// =====================================================================
TEST(SqpDriverSsnMode, TheLedgerCarriesTheSsnColumnsAndTheyAgreeWithTheCounters) {
    Ledger ledger;
    IndefiniteBoxModel model;

    SqpDriver walk_driver{SqpOptions{}};
    walk_driver.attach_ledger(&ledger, "walk");
    const SqpSolution walk = walk_driver.solve(model);

    SqpDriver ssn_driver(ssn_mode_options());
    ssn_driver.attach_ledger(&ledger, "ssn");
    const SqpSolution ssn = ssn_driver.solve(model);

    ASSERT_EQ(ledger.sqp_records().size(), 2u);
    const SqpSolveRecord &walk_rec = ledger.sqp_records()[0];
    const SqpSolveRecord &ssn_rec = ledger.sqp_records()[1];

    // A kWalk ROW READS ZERO IN ALL THREE, which is what makes these columns
    // additive rather than a change to any existing ledger row.
    EXPECT_EQ(walk_rec.ssn_iters, 0);
    EXPECT_EQ(walk_rec.ssn_bulk_flips, 0);
    EXPECT_EQ(walk_rec.ssn_escapes, 0);
    EXPECT_EQ(walk_rec.counters.ssn.ssn_iters, 0);

    // A kSsn ROW CARRIES THE WORK, and the flat columns are exactly the nested
    // ones -- they are copied in the same statement list, so a drift between
    // them is a code change, not a measurement.
    EXPECT_EQ(ssn_rec.ssn_iters, ssn.counters.ssn.ssn_iters);
    EXPECT_EQ(ssn_rec.ssn_bulk_flips, ssn.counters.ssn.ssn_bulk_flips);
    EXPECT_EQ(ssn_rec.ssn_escapes, ssn.counters.ssn.ssn_escapes);
    EXPECT_EQ(ssn_rec.ssn_iters, ssn_rec.counters.ssn.ssn_iters);
    EXPECT_GT(ssn_rec.ssn_iters, 0);
    EXPECT_EQ(ssn_rec.ssn_escapes, 1);
    (void)walk;
}

// =====================================================================
// THE PEAK AGGREGATES BY MAX, THE OTHER FIVE BY SUM.
//
// Driven directly on the aggregation helper, because no NLP fixture can
// distinguish "max" from "sum" without also being a claim about which
// subproblem held the peak -- and that would pin a trajectory to test an
// accumulator.
// =====================================================================
TEST(SqpDriverSsnMode, CounterAggregationSumsFiveFieldsAndPeaksTheSixth) {
    SsnCounters total;
    SsnCounters a;
    a.ssn_iters = 3;
    a.ssn_bulk_flips = 2;
    a.ssn_backtracks = 5;
    a.ssn_prox_updates = 1;
    a.ssn_escapes = 1;
    a.ssn_uncertain_peak = 7;
    SsnCounters b;
    b.ssn_iters = 4;
    b.ssn_bulk_flips = 1;
    b.ssn_backtracks = 6;
    b.ssn_prox_updates = 2;
    b.ssn_escapes = 0;
    b.ssn_uncertain_peak = 4;

    accumulate_ssn_counters(total, a);
    accumulate_ssn_counters(total, b);

    EXPECT_EQ(total.ssn_iters, 7);
    EXPECT_EQ(total.ssn_bulk_flips, 3);
    EXPECT_EQ(total.ssn_backtracks, 11);
    EXPECT_EQ(total.ssn_prox_updates, 3);
    EXPECT_EQ(total.ssn_escapes, 1);
    EXPECT_EQ(total.ssn_uncertain_peak, 7)
        << "a PEAK aggregates by max: the sum of two peaks is not the peak of the union, and is "
           "not a total of anything either";
}

// =====================================================================
// PHASE-7 TASK 6b (docket D6) -- THE ESCAPE-REASON CENSUS PARTITIONS THE
// ESCAPE COUNT.
//
// The census is only worth having if it is EXHAUSTIVE and DISJOINT: six
// buckets that sum to `ssn_escapes`, on every solve, at every scale. That is
// the property `bench/bench_corpus.cpp`'s reader enforces on every artifact
// row it scores, and the property that makes a mislabel visible rather than
// inferred (the two G4 watch items -- the 0.55% false-`kInfeasible` rate and
// the kNoContraction/kBudget mislabel of genuine infeasibles).
//
// Asserted on a REAL escaping solve, not a synthetic counter struct, because
// the failure this guards against is a WRITE SITE that forgot to update the
// census -- which no accumulator test can see.
// =====================================================================
TEST(SqpDriverSsnMode, TheEscapeReasonCensusPartitionsTheEscapeCount) {
    const auto census_sum = [](const SsnCounters &c) {
        return c.ssn_escape_budget + c.ssn_escape_singular + c.ssn_escape_no_contraction +
               c.ssn_escape_infeasible_suspect + c.ssn_escape_indefinite +
               c.ssn_escape_gate_refused;
    };

    // (1) THE ESCAPING FIXTURE. IndefiniteBoxModel exhausts the proximal
    // ladder and hands off; the escape is real and is asserted by
    // AnEscapedSubproblemLandsInTheWalkAndStillCertifies above.
    {
        IndefiniteBoxModel model;
        SqpDriver driver(ssn_mode_options());
        const SqpSolution sol = driver.solve(model);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        ASSERT_EQ(sol.counters.ssn.ssn_escapes, 1) << "the fixture's own premise";
        EXPECT_EQ(census_sum(sol.counters.ssn), sol.counters.ssn.ssn_escapes)
            << "the six buckets must partition the total";
        // AND IT IS THE RIGHT BUCKET. The ladder ran 1e-6 -> 1e6 and
        // exhausted, which ssn_engine.h reports as kNoContraction -- so this
        // also pins that the census records the MECHANISM, not just a count.
        EXPECT_EQ(sol.counters.ssn.ssn_escape_no_contraction, 1);
        EXPECT_EQ(sol.counters.ssn.ssn_escape_gate_refused, 0)
            << "the kernel escaped on its own; the driver's usability gate was not the refuser";
    }

    // (2) A NON-ESCAPING SOLVE: every bucket is zero, and so is the total.
    // Guards the opposite error -- a census that counts something on a clean
    // run would make every corpus row look like a hand-off.
    {
        const HsProblem hs = make_hs(1);
        SqpDriver driver(ssn_mode_options());
        const SqpSolution sol = driver.solve(*hs.model);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        ASSERT_EQ(sol.counters.ssn.ssn_escapes, 0) << "the fixture's own premise";
        EXPECT_EQ(census_sum(sol.counters.ssn), 0);
    }

    // (3) THE SIXTH BUCKET HAS NO SsnEscape VALUE BEHIND IT, and is therefore
    // the one the engine cannot write. Driven on the aggregation helper, which
    // is the only driver-scale surface that folds it.
    {
        SsnCounters total;
        SsnCounters one;
        one.ssn_escapes = 1;
        one.ssn_escape_gate_refused = 1;
        accumulate_ssn_counters(total, one);
        accumulate_ssn_counters(total, one);
        EXPECT_EQ(total.ssn_escapes, 2);
        EXPECT_EQ(census_sum(total), 2);
        EXPECT_EQ(total.ssn_escape_gate_refused, 2);
    }
}

// =====================================================================
// THE SSN START IS BUILT FROM THE WALK'S OWN SEED, and drops exactly one
// thing: the primal.
// =====================================================================
TEST(SqpDriverSsnMode, TheSsnStartMirrorsTheWalkSeedButNeverItsPrimal) {
    QpSolution seed;
    seed.x = Vec::Constant(3, 9.0); // the remembered STEP -- must not be read
    seed.lambda_e = Vec::Constant(1, 2.5);
    seed.lambda_i = Vec::Constant(2, 0.75);
    seed.z = Vec::Constant(3, -1.5);
    seed.bound_state = {BoundState::kAtLower, BoundState::kFree, BoundState::kFixed};
    seed.ineq_active = {true, false};

    const SsnStart start = ssn_start_from_qp_seed(&seed);
    EXPECT_EQ(start.x.size(), 0)
        << "THE RULE: the subproblem is in STEP variables and the trust region centres on p = 0, "
           "so a remembered step is never read -- the same reason every walk seeding site zeroes "
           "seed.x";
    EXPECT_EQ(start.lambda_e, seed.lambda_e);
    EXPECT_EQ(start.lambda_i, seed.lambda_i);
    EXPECT_EQ(start.z, seed.z);
    EXPECT_EQ(start.activity_hint.ineq, seed.ineq_active);
    EXPECT_EQ(start.activity_hint.bounds, seed.bound_state);
    EXPECT_EQ(start.prox_center_x.size(), 0);

    // A NULL SEED IS THE COLD START, which ssn_engine.h reads as "zero of the
    // right size" in every block.
    const SsnStart cold = ssn_start_from_qp_seed(nullptr);
    EXPECT_TRUE(cold.activity_hint.empty());
    EXPECT_EQ(cold.x.size(), 0);
    EXPECT_EQ(cold.lambda_i.size(), 0);
}

// =====================================================================
// THE COUNTER ACCOUNTING RULE, on a fixture where the SSN certifies EVERY
// subproblem: SSN work goes to SqpCounters::ssn and to `factorizations`, and
// NEVER to qp_minor_iters. See ssn_result_to_qp_solution's own note for why
// folding them would corrupt every published figure in this repository.
// =====================================================================
TEST(SqpDriverSsnMode, SsnWorkIsNeverFoldedIntoTheWalksMinorCurrency) {
    using hven::solvers::test_support::make_hs;

    // HS14 certifies every subproblem under kSsn with no escape at all
    // (observed: 9 SSN iterations, 0 escapes, 0 backtracks).
    auto problem = make_hs(14);
    SqpDriver driver(ssn_mode_options());
    const SqpSolution sol = driver.solve(*problem.model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_EQ(sol.counters.ssn.ssn_escapes, 0) << "fixture premise: the walk never runs here";
    EXPECT_EQ(sol.counters.ssn.ssn_iters, 9);
    EXPECT_EQ(sol.counters.qp_minor_iters, 0)
        << "THE ACCOUNTING RULE: an SSN Newton step is not a walk minor and is never counted as "
           "one -- qp_minor_iters is the currency every published figure in this repository is "
           "quoted in";
    EXPECT_GT(sol.counters.factorizations, 0)
        << "a factorization IS the same physical quantity in both kernels and does carry across";
    for (const SqpIterate &row : sol.history) {
        if (row.qp_solved) {
            EXPECT_EQ(row.qp_minor_iters, 0);
            EXPECT_GT(row.qp_factorizations, 0) << "the per-major row carries the real cost too";
        }
    }
}

// =====================================================================
// THE PROXIMAL EXPORT ACCUMULATOR IS PER-SOLVE, NOT PER-DRIVER.
//
// It is a MEMBER (make_warm_start is static and solve_impl has a dozen exits,
// so the stamp happens once, in record_solve), which makes "cleared at the top
// of every solve_impl" a real obligation rather than a free property. One
// driver, two solves, and the second must not inherit the first's ladder.
//
// THIS IS THE MUTATION M24 FIXTURE. Without the reset the second solve below
// emits `has_prox_center = true` at sigma 1e6 — a proximal level belonging to a
// DIFFERENT MODEL — and a continuation caller chaining hand-offs off one driver
// would damp a subproblem on evidence from a problem it never solved.
// =====================================================================
TEST(SqpDriverSsnMode, TheProximalExportDoesNotLeakFromOneSolveToTheNext) {
    using hven::solvers::test_support::make_hs;

    SqpDriver driver(ssn_mode_options());

    // SOLVE 1: exhausts the ladder, so there is something to leak.
    IndefiniteBoxModel armed;
    const SqpSolution first = driver.solve(armed);
    ASSERT_EQ(first.status, SqpStatus::kOptimal);
    ASSERT_TRUE(first.warm_start.has_prox_center) << "fixture premise";
    ASSERT_DOUBLE_EQ(first.warm_start.prox_sigma, 1e6);

    // SOLVE 2, SAME DRIVER: HS14 certifies every subproblem under kSsn with no
    // escape and no ladder at all, so a proximal level on ITS hand-off can only
    // have come from solve 1.
    auto benign = make_hs(14);
    const SqpSolution second = driver.solve(*benign.model);
    ASSERT_EQ(second.status, SqpStatus::kOptimal);
    ASSERT_EQ(second.counters.ssn.ssn_prox_updates, 0) << "fixture premise: HS14 arms nothing";

    EXPECT_FALSE(second.warm_start.has_prox_center)
        << "THE PIN: the accumulator is cleared per solve, so a second solve on the same driver "
           "cannot emit the first solve's proximal level";
    EXPECT_DOUBLE_EQ(second.warm_start.prox_sigma, 0.0);
    EXPECT_EQ(second.warm_start.prox_center_x.size(), 0);
    EXPECT_EQ(second.warm_start.prox_center_lambda.size(), 0);
}

// =====================================================================
// THE SSN's BOUND ACTIVITY REACHES THE HAND-OFF.
//
// `ssn_result_to_qp_solution` copies `bound_state` across, and that export is
// what `seed` (the next major's hint, for BOTH kernels) and
// `make_warm_start`'s `bound_active` are built from. HS30 is the fixture
// because it satisfies three things at once: its optimum x* = (1, 0, 0) sits ON
// its own lower bound in x1, its solve certifies EVERY subproblem under kSsn
// (0 escapes, so the activity can only have come from the SSN kernel), and its
// walk arm is available as the reference the export must reproduce.
//
// THIS IS THE MUTATION M33 FIXTURE. With the export dropped the emitted
// hand-off reports every variable free, and a warm chain off it would re-derive
// an active set it had already paid to identify.
// =====================================================================
TEST(SqpDriverSsnMode, TheSsnBoundActivityReachesTheHandOff) {
    using hven::solvers::test_support::make_hs;

    auto walk_problem = make_hs(30);
    SqpOptions walk_opts;
    walk_opts.max_iter = 60;
    SqpDriver walk_driver(walk_opts);
    const SqpSolution walk = walk_driver.solve(*walk_problem.model);
    ASSERT_EQ(walk.status, SqpStatus::kOptimal);

    auto ssn_problem = make_hs(30);
    SqpDriver ssn_driver(ssn_mode_options());
    const SqpSolution ssn = ssn_driver.solve(*ssn_problem.model);
    ASSERT_EQ(ssn.status, SqpStatus::kOptimal);
    ASSERT_EQ(ssn.counters.ssn.ssn_escapes, 0)
        << "fixture premise: every subproblem is SSN-certified, so the activity below can only "
           "have come from the SSN export";

    // x1 IS ON ITS LOWER BOUND at the solution -- the fixture's whole premise,
    // asserted from the model rather than assumed.
    ASSERT_EQ(ssn_problem.model->lower()(0), 1.0);
    EXPECT_NEAR(ssn.x(0), 1.0, 1e-8);

    ASSERT_EQ(ssn.warm_start.bound_active.size(), 3u);
    EXPECT_EQ(ssn.warm_start.bound_active[0], -1)
        << "THE PIN: the SSN's bound_state export reaches make_warm_start, so the hand-off says "
           "x1 sits at its LOWER bound";
    // AND IT AGREES WITH THE WALK, variable for variable -- the export is the
    // same activity the other kernel identifies, not a second opinion.
    EXPECT_EQ(ssn.warm_start.bound_active, walk.warm_start.bound_active);
    // THE HAND-OFF IS ALSO USABLE: fed back, it ingests at kWarm and certifies.
    SqpDriver chained(ssn_mode_options());
    const SqpSolution again =
        chained.solve(*ssn_problem.model, ssn.x, ssn.warm_start, /*minor_budget=*/0);
    EXPECT_EQ(again.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(again.status, SqpStatus::kOptimal);
}

// =====================================================================
// FIX ROUND 1 (F-B). THE RESTORATION SUB-SOLVE STAYS ON THE WALK UNDER kSsn.
//
// THE DEFECT THIS PINS THE REPAIR OF. `SqpOptions ropts = opts_;` copied the
// caller's `qp_mode` into the restoration sub-driver along with everything
// else, so a kSsn solve that entered restoration ran the semismooth kernel
// through the whole phase -- on a wrapper model in (original + slack)
// variables with a subgradient-selector objective that no fixture in this
// repository has ever run that kernel against -- while `sqp_driver.h`'s own
// WHAT IS NOT ROUTED THROUGH SSN note said, and still says, that the phase
// stays on the walk UNCONDITIONALLY. Doc-versus-code, plus a telemetry hole:
// `rs.counters.factorizations` was folded and `rs.counters.ssn` was not, so
// restoration-phase SSN work appeared as cost with no work behind it.
//
// WHY THE COUNTERS ARE PINNED RATHER THAN THE MODE. There is no getter for a
// nested driver's options, so the reset is measured through what it CHANGES.
// With the reset (shipped): 131 SSN iterations, 128 factorizations. Without it
// (the mutant): 140 and 141. Both pinned exactly, so deleting the reset fails
// this test rather than passing quietly.
//
// (The factorization pin also covers a SECOND fix-round repair found while
// writing it: an ESCAPED SSN subproblem's own factorizations reached no
// accumulation site at all, so `SqpCounters::factorizations` under-reported and
// sqp_types.h's documented `ssn_iters <= factorizations` failed outright on
// this very fixture -- 131 against 128. They are charged now; see the
// hand-off's own note.)
// =====================================================================
TEST(SqpDriverSsnMode, RestorationStaysOnTheWalkUnderKSsn) {
    SecondOrderInfeasibleModel model;
    SqpOptions opts = ssn_mode_options(20);
    opts.tr_init = 8.0;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    // FIXTURE PREMISE, asserted rather than assumed: the phase really runs.
    ASSERT_GE(sol.counters.restoration_iters, 1)
        << "fixture premise: this model reaches the restoration gate";
    ASSERT_GT(sol.counters.ssn.ssn_iters, 0) << "and the MAIN loop really runs the SSN kernel";

    EXPECT_EQ(131, sol.counters.ssn.ssn_iters)
        << "OBSERVED. MKL, clang++, Release. THE PIN: this is the MAIN loop's SSN work and "
           "nothing else. A restoration sub-solve left on kSsn adds its own, reading 140.";
    EXPECT_EQ(234, sol.counters.factorizations)
        << "OBSERVED. MKL, clang++, Release. The other side of the same reset: the sub-solve's "
           "factorizations ARE folded, so a kernel change inside it moves this too (247 with "
           "the mode leaked in).";
    EXPECT_EQ(5, sol.counters.ssn.ssn_escapes) << "OBSERVED. MKL, clang++, Release.";
    EXPECT_EQ(5, sol.counters.ssn.ssn_refinements)
        << "OBSERVED. MKL, clang++, Release. Reads 6 with the mode leaked into restoration.";
    EXPECT_EQ(8, sol.counters.ssn.ssn_refine_refused) << "OBSERVED. MKL, clang++, Release.";
    EXPECT_LE(sol.counters.ssn.ssn_iters, sol.counters.factorizations)
        << "sqp_types.h's own invariant, which the unfolded nested counters used to break";
}

// =====================================================================
// FIX ROUND 1 (F-D). THE PROBE BUDGET SEES SSN WORK.
//
// THE DEFECT THIS PINS THE REPAIR OF. The budget trips on `qp_minor_iters`,
// and an SSN-solved subproblem contributes ZERO to it by design -- so under
// kSsn a budgeted probe could never stop: Phase-6 Task 1's failed-proposal
// detector, the mechanism Phase 6 named its own priority #1, was silently void
// in the one mode Phase 7 exists to measure. HS1 is the sharpest witness
// available: under kSsn it spends 129 factorizations and exactly ZERO walk
// minors on its way to an answer, so the old currency reads 0 forever.
//
// THE ASSERTION THAT MAKES THIS NON-VACUOUS is `qp_minor_iters == 0` beside
// the stop: the budget was met by work the walk currency cannot see, which is
// precisely the claim.
// =====================================================================
TEST(SqpDriverSsnMode, TheProbeBudgetChargesSsnWorkThatTheWalkCurrencyCannotSee) {
    using hven::solvers::test_support::make_hs;

    // (1) FIXTURE PREMISE: unbudgeted, HS1 under kSsn converges and spends no
    // walk minors at all.
    auto premise_problem = make_hs(1);
    SqpDriver premise_driver(ssn_mode_options());
    const SqpSolution premise = premise_driver.solve(*premise_problem.model);
    ASSERT_EQ(SqpStatus::kOptimal, premise.status);
    ASSERT_EQ(0, premise.counters.qp_minor_iters)
        << "fixture premise: every subproblem is SSN-certified, so the walk never runs";
    ASSERT_GT(premise.counters.factorizations, 20)
        << "and it is not cheap -- there is real work for a budget to see";

    // (2) THE BUDGET NOW STOPS IT. Before the repair this arm reported
    // kOptimal with probe_budget_stops == 0, because the test quantity never
    // left 0.
    auto problem = make_hs(1);
    SqpDriver driver(ssn_mode_options());
    const SqpSolution sol = driver.solve(*problem.model, problem.model->start_point(), WarmStart{},
                                         /*minor_budget=*/20);

    EXPECT_EQ(SqpStatus::kMaxIter, sol.status)
        << "THE PROBE BUDGET note's clause 4: a probe stop reports kMaxIter, not "
           "kBudgetExhausted";
    EXPECT_EQ(1, sol.counters.probe_budget_stops) << "and says so in the one field that tells "
                                                     "this exit from an ordinary max_iter one";
    EXPECT_EQ(0, sol.counters.qp_minor_iters)
        << "THE POINT: the walk currency is still 0. The budget was met entirely by SSN "
           "factorizations -- so a budget denominated in minors alone could not have stopped "
           "this solve at all.";
    EXPECT_EQ(27, sol.counters.factorizations)
        << "OBSERVED. MKL, clang++, Release. The budget is checked BETWEEN majors, so the "
           "crossing major's own work is bought (clause 2).";
    EXPECT_EQ(4, sol.counters.major_iters) << "OBSERVED. MKL, clang++, Release.";

    // (2b) A TIGHTER BUDGET STOPS EARLIER, AND THE REFINEMENT'S OWN
    // FACTORIZATION IS PART OF WHAT IT IS SPENDING. At minor_budget = 10 the
    // solve stops one major sooner than it would if only the SSN KERNEL's
    // factorizations were charged and the tier-3 refinement's were not (which
    // reads 3 majors / 19 factorizations).
    auto tight_problem = make_hs(1);
    SqpDriver tight_driver(ssn_mode_options());
    const SqpSolution tight = tight_driver.solve(
        *tight_problem.model, tight_problem.model->start_point(), WarmStart{}, /*minor_budget=*/10);
    EXPECT_EQ(1, tight.counters.probe_budget_stops);
    EXPECT_EQ(2, tight.counters.major_iters)
        << "OBSERVED. MKL, clang++, Release. Reads 3 if the refinement's factorization is not "
           "charged.";
    EXPECT_EQ(11, tight.counters.factorizations)
        << "OBSERVED. MKL, clang++, Release. Reads 19 in that case.";
    EXPECT_EQ(2, tight.counters.ssn.ssn_refinements);

    // (3) AND THE WALK SIDE IS UNMOVED. The same budget on the same problem at
    // the shipped default is the pre-existing behaviour, byte for byte -- the
    // charge is written only inside the kSsn branch.
    auto walk_problem = make_hs(1);
    SqpOptions walk_opts;
    walk_opts.max_iter = 60;
    SqpDriver walk_driver(walk_opts);
    const SqpSolution walk =
        walk_driver.solve(*walk_problem.model, walk_problem.model->start_point(), WarmStart{}, 20);
    EXPECT_EQ(1, walk.counters.probe_budget_stops);
    EXPECT_EQ(21, walk.counters.qp_minor_iters)
        << "OBSERVED. MKL, clang++, Release. kWalk still spends its budget in minors and "
           "crosses it by one.";
}

// =====================================================================
// FIX ROUND 1 (F-E). THE CARRIED PROXIMAL CENTRE IS NEVER AN ESCAPED ITERATE.
//
// THE DEFECT THIS PINS THE REPAIR OF (Fable review I3, the Task-7b stale-state
// class). The centre was stamped in the same statement as the sigma, BEFORE
// the usability gate -- and the subproblem that sets the max sigma is typically
// an EXHAUSTED LADDER, i.e. an ESCAPED solve, whose x ssn_engine.h measures at
// 133x-160x the trust region and whose export certifies nothing. So the
// WarmStart interface shipped, on every armed kSsn solve, a centre that was a
// diverged point by construction, under a field documented as "the point it was
// reached at". The centres are unread today (Task-4 deviation D5), which is
// exactly why this had to be repaired before a reader starts trusting them.
//
// BOTH POLARITIES ARE PINNED, because "always empty" would be a repair that
// broke the feature: (a) IndefiniteBoxModel, whose only ladder climb ends in an
// escape, carries the LEVEL and no centre; (b) HS7, whose ladder climbs on a
// subproblem that then CERTIFIES, carries both.
// =====================================================================
TEST(SqpDriverSsnMode, TheCarriedProximalCentreIsNeverAnEscapedIterate) {
    using hven::solvers::test_support::make_hs;

    // (a) THE ESCAPED CLIMB. The level carries; the centre does not.
    IndefiniteBoxModel escaped;
    SqpDriver escaped_driver(ssn_mode_options());
    const SqpSolution esc = escaped_driver.solve(escaped);
    ASSERT_EQ(SqpStatus::kOptimal, esc.status);
    ASSERT_EQ(1, esc.counters.ssn.ssn_escapes) << "fixture premise: the ladder is exhausted";
    EXPECT_DOUBLE_EQ(1e6, esc.warm_start.prox_sigma)
        << "THE LEVEL IS PROVENANCE-FREE and still carries -- an exhausted ladder is exactly "
           "the evidence the carry exists to transmit";
    EXPECT_EQ(0, esc.warm_start.prox_center_x.size())
        << "THE PIN: pre-repair this was the ESCAPED iterate, sized 2. A centre may only come "
           "from a subproblem the usability gate accepted.";
    EXPECT_EQ(0, esc.warm_start.prox_center_lambda.size());

    // (b) THE CERTIFYING CLIMB. Both carry, and the centre is a point the
    // trust region could actually have contained.
    auto p = make_hs(7);
    SqpDriver driver(ssn_mode_options());
    const SqpSolution sol = driver.solve(*p.model);
    ASSERT_EQ(SqpStatus::kOptimal, sol.status);
    ASSERT_TRUE(sol.warm_start.has_prox_center) << "fixture premise: HS7's ladder arms";
    EXPECT_EQ(p.model->n(), sol.warm_start.prox_center_x.size())
        << "a certifying subproblem DID set the high-water mark, so the centre is real";
    EXPECT_EQ(p.model->me() + p.model->mi(), sol.warm_start.prox_center_lambda.size());
    EXPECT_TRUE(sol.warm_start.prox_center_x.allFinite());
    // AND IT IS A STEP-SIZED QUANTITY, not a diverged one: the subproblem is in
    // STEP variables under a trust region, so a usable exit's x is bounded by
    // the radius to within the certifying bound. An escaped one is not, which
    // is the whole mechanism.
    EXPECT_LT(sol.warm_start.prox_center_x.lpNorm<Eigen::Infinity>(), 1.0)
        << "OBSERVED. MKL, clang++, Release. ssn_engine.h measures an ESCAPED iterate at "
           "133x-160x the radius; this is a certifying one.";

    // (c) THE TWO HIGH-WATER MARKS ARE GENUINELY SEPARATE, which is the part a
    // single-mark implementation would get wrong in the OTHER direction. HS33
    // is the row that distinguishes them: its solve-wide max sigma is set by a
    // subproblem the gate refused, and a LATER, LOWER-sigma subproblem
    // certifies. Keying the centre off the solve-wide mark (rather than off its
    // own) would refuse that centre and ship nothing at all here.
    auto split = make_hs(33);
    SqpDriver split_driver(ssn_mode_options());
    const SqpSolution split_sol = split_driver.solve(*split.model);
    ASSERT_EQ(SqpStatus::kOptimal, split_sol.status);
    EXPECT_DOUBLE_EQ(1e6, split_sol.warm_start.prox_sigma) << "the LEVEL comes from the ladder";
    EXPECT_EQ(split.model->n(), split_sol.warm_start.prox_center_x.size())
        << "OBSERVED. MKL, clang++, Release. THE PIN: reads 0 if the centre is keyed off the "
           "solve-wide sigma mark instead of its own.";
}

// FIX ROUND 1. THE CENTRE'S HIGH-WATER MARK IS PER SOLVE, like the level's.
//
// A per-driver accumulator that is not cleared between solves leaks: the SECOND
// solve on the same driver would have to BEAT the first solve's centre to stamp
// one at all, and would otherwise export a real level with an empty centre --
// silently, and only on the second and later solves of a reused driver, which
// is exactly the shape continuation drives. Two identical solves on ONE driver
// is the whole test; the second must be indistinguishable from the first.
TEST(SqpDriverSsnMode, TheCentresHighWaterMarkIsClearedBetweenSolves) {
    using hven::solvers::test_support::make_hs;

    auto first_problem = make_hs(7);
    auto second_problem = make_hs(7);
    SqpDriver driver(ssn_mode_options()); // ONE driver, two solves

    const SqpSolution first = driver.solve(*first_problem.model);
    ASSERT_EQ(SqpStatus::kOptimal, first.status);
    ASSERT_EQ(first_problem.model->n(), first.warm_start.prox_center_x.size())
        << "fixture premise: HS7's ladder arms on a subproblem that certifies";

    const SqpSolution second = driver.solve(*second_problem.model);
    ASSERT_EQ(SqpStatus::kOptimal, second.status);
    EXPECT_DOUBLE_EQ(first.warm_start.prox_sigma, second.warm_start.prox_sigma);
    EXPECT_EQ(first.warm_start.prox_center_x.size(), second.warm_start.prox_center_x.size())
        << "THE PIN: reads 0 on the second solve if the centre's mark is not cleared per "
           "solve, because that solve cannot beat the first's";
    EXPECT_EQ(first.warm_start.prox_center_x, second.warm_start.prox_center_x)
        << "and it is the SAME point, not merely a nonempty one";
}

// FIX ROUND 1. AN ESCAPED SUBPROBLEM'S COST IS CHARGED IN BOTH FIELDS.
//
// On a HAND-OFF the driver's `qs` becomes the WALK's solution, so the escaped
// SSN attempt's own cost reaches no other accumulation path -- it was simply
// LOST before fix round 1, which broke sqp_types.h's documented
// `ssn_iters <= factorizations` outright (131 against 128 on
// RestorationStaysOnTheWalkUnderKSsn). The rule is stated once, in a free
// function, so BOTH fields are assertable rather than only the one the
// driver-scale fixtures happen to exercise sharply.
TEST(SqpDriverSsnMode, TheEscapedSubproblemsCostIsChargedInBothFields) {
    SsnResult res;
    res.factorizations = 7;
    res.symbolic_analyses = 3;

    SqpCounters total;
    total.factorizations = 100;
    total.symbolic_analyses = 40;
    total.qp_minor_iters = 5;

    charge_ssn_subproblem_cost(total, res);

    EXPECT_EQ(107, total.factorizations);
    EXPECT_EQ(43, total.symbolic_analyses)
        << "THE FIELD THE DRIVER-SCALE FIXTURES DO NOT PIN SHARPLY: an SSN solve pays a "
           "phase-11 analysis whenever its KKT pattern changes, and an escaped one pays it "
           "just the same";
    EXPECT_EQ(5, total.qp_minor_iters)
        << "AND MINORS ARE NOT CHARGED, in either direction -- the two kernels do not share a "
           "minor (ssn_result_to_qp_solution's counter-mapping note)";
}

// =====================================================================
// FIX ROUND 1 (F-A, second half). fb_tol TRACKS THE TIGHTER OF THE DRIVER'S
// TWO TOLERANCES.
//
// THE DEFECT THIS PINS THE REPAIR OF (Fable review, minor M-a). `ssn_options`
// derived the kernel's FB threshold from `kkt_tol` alone. `feas_tol` and
// `kkt_tol` are independent SqpOptions fields, so a caller may set
// feas_tol < kkt_tol -- and then the SSN kept certifying subproblems whose own
// slack could be negative by ~1.71 * kkt_tol, i.e. outside the very feasibility
// bar the driver's convergence test applied to the resulting iterate. Not a
// wrong answer (the model-level gate holds) but a DNF/stall asymmetry against
// kWalk, whose engine reads feas_tol directly.
//
// Driven through the free function, which is where the rule lives, so this is
// a test of the RULE rather than of one driver configuration.
// =====================================================================
TEST(SqpDriverSsnMode, TheSsnFbToleranceTracksTheTighterOfTheDriversTwoTolerances) {
    // (1) THE DEFECT'S OWN CONFIGURATION: feas_tol tighter than kkt_tol.
    EXPECT_DOUBLE_EQ(ssn_fb_tol_from_kkt_tol(1e-10), ssn_fb_tol_for(1e-6, 1e-10))
        << "THE PIN: the tighter of the two wins. Reading kkt_tol alone returns the 1e-6 value "
           "here, which is the defect.";
    EXPECT_LT(ssn_fb_tol_for(1e-6, 1e-10), ssn_fb_tol_for(1e-6, 1e-6))
        << "and it is a MOVE, not a relabelling";

    // (2) THE OTHER DIRECTION IS UNCHANGED, which is what keeps every shipped
    // configuration byte-identical: at the defaults the two are equal, and a
    // LOOSER feas_tol may not loosen the stationarity bar.
    EXPECT_DOUBLE_EQ(ssn_fb_tol_from_kkt_tol(1e-6), ssn_fb_tol_for(1e-6, 1e-6));
    EXPECT_DOUBLE_EQ(ssn_fb_tol_from_kkt_tol(1e-6), ssn_fb_tol_for(1e-6, 1e-3));

    // (3) AND THE DRIVER REACHES 1e-10 THROUGH IT, on the same fixture the
    // tight-finish arm uses -- so the derivation change did not cost the
    // property that arm exists to protect.
    using hven::solvers::test_support::make_hs;
    auto p = make_hs(7);
    SqpOptions opts = ssn_mode_options();
    opts.kkt_tol = 1e-10;
    opts.feas_tol = 1e-10;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    EXPECT_EQ(SqpStatus::kOptimal, sol.status);
    const test_support::NlpKktResidual r = self_check_kkt(*p.model, sol, 1e-10);
    EXPECT_LT(r.stationarity, 1e-9);
    EXPECT_LT(r.primal, 1e-9);
}

// =====================================================================
// PHASE-7 TASK 6b PHASE B -- R5's DECIDING MEASUREMENT, AS A PIN
// =====================================================================
//
// The research pass (docs/notes/research/
// fable_fb_ssn_globalization_second_pass_claude.md, R5) names its own
// deciding measurement: "HS battery + certified-exit cells: identical
// certificate outcomes at -1 factorization; confirm no residue-set growth."
// This is that measurement, run over all 27 Hock-Schittkowski problems and
// asserted as an IDENTITY rather than as an inequality, because the saving is
// predictable to the unit:
//
//     factorizations(lever on) == factorizations(lever off) - ssn_refinements
//
// -- one per ACCEPTED tier-3 refinement and nothing else. Everything the
// certificate is about is asserted unchanged beside it: status, objective,
// majors, the refinement/refusal split (the "residue set"), the escape count
// and the whole escape-reason census.
//
// **WHY THE IDENTITY AND NOT A BOUND.** A bound would pass for a lever that
// saved a factorization by skipping a refinement, which is the one way this
// change could be cheap and wrong. Pinning the exact arithmetic makes the
// saving attributable: it is the deferred verification, once per accepted
// face solve, and nothing else moved to pay for it.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SqpDriverSsnMode, CertifyFromFaceCostsExactlyOneFactorizationPerAcceptedRefinement) {
    using hven::solvers::test_support::hs_numbers;
    using hven::solvers::test_support::make_hs;

    Index total_saved = 0;
    Index problems_that_saved = 0;
    for (int number : hs_numbers()) {
        SCOPED_TRACE(fmt::format("HS{}", number));

        auto base_problem = make_hs(number);
        SqpDriver base_driver(ssn_mode_options());
        const SqpSolution base = base_driver.solve(*base_problem.model);

        SqpOptions lever_opts = ssn_mode_options();
        lever_opts.ssn_certify_from_face = true;
        auto lever_problem = make_hs(number);
        SqpDriver lever_driver(lever_opts);
        const SqpSolution lever = lever_driver.solve(*lever_problem.model);

        // 1. THE CERTIFICATE OUTCOME IS IDENTICAL.
        EXPECT_EQ(lever.status, base.status);
        EXPECT_DOUBLE_EQ(lever.f, base.f);
        EXPECT_EQ(lever.counters.major_iters, base.counters.major_iters);
        EXPECT_EQ(lever.counters.qp_minor_iters, base.counters.qp_minor_iters);
        EXPECT_EQ(lever.counters.ssn.ssn_iters, base.counters.ssn.ssn_iters);

        // 2. THE REFUSAL SET DOES NOT GROW -- and in fact does not move.
        EXPECT_EQ(lever.counters.ssn.ssn_refinements, base.counters.ssn.ssn_refinements);
        EXPECT_EQ(lever.counters.ssn.ssn_refine_refused, base.counters.ssn.ssn_refine_refused);
        EXPECT_EQ(lever.counters.ssn.ssn_refine_factorizations,
                  base.counters.ssn.ssn_refine_factorizations);
        EXPECT_EQ(lever.counters.ssn.ssn_refine_neg_duals, base.counters.ssn.ssn_refine_neg_duals);

        // 3. NO ESCAPE IS CREATED OR DESTROYED, census included -- so no
        // certificate was traded for a hand-off.
        EXPECT_EQ(lever.counters.ssn.ssn_escapes, base.counters.ssn.ssn_escapes);
        EXPECT_EQ(lever.counters.ssn.ssn_escape_budget, base.counters.ssn.ssn_escape_budget);
        EXPECT_EQ(lever.counters.ssn.ssn_escape_singular, base.counters.ssn.ssn_escape_singular);
        EXPECT_EQ(lever.counters.ssn.ssn_escape_no_contraction,
                  base.counters.ssn.ssn_escape_no_contraction);
        EXPECT_EQ(lever.counters.ssn.ssn_escape_infeasible_suspect,
                  base.counters.ssn.ssn_escape_infeasible_suspect);
        EXPECT_EQ(lever.counters.ssn.ssn_escape_indefinite,
                  base.counters.ssn.ssn_escape_indefinite);
        EXPECT_EQ(lever.counters.ssn.ssn_escape_gate_refused,
                  base.counters.ssn.ssn_escape_gate_refused);

        // 4. THE SAVING, TO THE UNIT.
        EXPECT_EQ(lever.counters.factorizations,
                  base.counters.factorizations - base.counters.ssn.ssn_refinements)
            << "one factorization per accepted refinement, and nothing else";
        total_saved += base.counters.ssn.ssn_refinements;
        problems_that_saved += base.counters.ssn.ssn_refinements > 0 ? 1 : 0;
    }
    // The battery is a real population for this lever rather than a formality.
    EXPECT_GT(problems_that_saved, 20);
    EXPECT_GT(total_saved, 40);
}

// The lever is INERT AT kWalk in both directions -- there is no SSN subproblem
// for a certificate to be deferred on, so setting it changes nothing at all.
// This is the cheap detector the byte-identity discipline is enforced through.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
TEST(SqpDriverSsnMode, CertifyFromFaceIsInertAtTheWalkDefault) {
    using hven::solvers::test_support::make_hs;
    for (int number : {7, 15, 33, 76}) {
        SCOPED_TRACE(fmt::format("HS{}", number));
        auto a_problem = make_hs(number);
        SqpOptions a_opts;
        a_opts.max_iter = 60;
        SqpDriver a_driver(a_opts);
        const SqpSolution a = a_driver.solve(*a_problem.model);

        auto b_problem = make_hs(number);
        SqpOptions b_opts = a_opts;
        b_opts.ssn_certify_from_face = true;
        SqpDriver b_driver(b_opts);
        const SqpSolution b = b_driver.solve(*b_problem.model);

        EXPECT_EQ(b.status, a.status);
        EXPECT_DOUBLE_EQ(b.f, a.f);
        EXPECT_EQ(b.counters.major_iters, a.counters.major_iters);
        EXPECT_EQ(b.counters.qp_minor_iters, a.counters.qp_minor_iters);
        EXPECT_EQ(b.counters.factorizations, a.counters.factorizations);
        EXPECT_EQ(b.counters.ssn.ssn_escapes, a.counters.ssn.ssn_escapes);
    }
}

// =====================================================================
// PHASE-B REVIEW FIX ROUND -- F3: R5's REFUSED-AND-WITHDRAWN PATH PAYS A
// FACTORIZATION AND MUST BE SEEN TO PAY IT
//
// THE DEFECT. With `ssn_certify_from_face` on, the tier-3 face solve is
// hoisted ahead of the driver's usability gate. If it REFUSES and the deferred
// verification it falls back to then WITHDRAWS the certificate, `sres` becomes
// an escape, the gate turns false, and the hand-off branch runs -- where the
// hoisted face solve's own factorization used to reach no counter at all:
// not SqpCounters::factorizations, not `ssn_refine_factorizations`, not
// `ssn_refine_refused`, and not the probe budget. One sparse factorization,
// genuinely paid, invisible. It is the same vanishing-work class fix round 1
// closed at the hand-off itself (see sqp_driver.h's note there), and the
// review found it unexercised: zero withdrawals over the 27-problem HS battery
// and all 57 corpus cells, and none anywhere in this suite.
//
// THE FIXTURE, AND WHY IT IS BUILT THE WAY IT IS. Reaching the pair needs a
// subproblem on which (a) the SSN reaches a CERTIFYING exit -- so the
// certificate is deferred rather than escaped in-loop -- at a point whose full
// augmented block is NOT positive definite, so the deferred verdict withdraws,
// and (b) the face solve refuses there too. An INTERIOR SADDLE of an
// indefinite QP satisfies both at once (no row is active, so the face reduced
// Hessian IS the indefinite block the full verification reads), but the
// prox-damped iteration DIVERGES from a saddle along negative curvature, so
// the kernel can only be found sitting at one -- which is why
// SsnEngineLocal.DeferredCertificationWithdrawsTheSaddleCertificate must seed
// the engine there by hand and cannot be reached from a driver.
//
// The gap this fixture drives through is that the driver's convergence test
// and the kernel's FB test are the SAME quantity at a cold iterate but at
// DIFFERENT bars: the driver needs `stationarity <= kkt_tol` while the kernel
// certifies at `||F||inf <= fb_tol == kkt_tol` after taking a step. Placing the
// start point a hair OUTSIDE the driver's bar in the CONVEX coordinate only
// (`eps = kkt_tol * (1 + 1e-6)`, with the concave coordinate exactly
// stationary) gives a major iteration the driver must take, on a QP whose
// damped Newton step lands inside the kernel's bar immediately -- at the
// saddle, where both gates then refuse. Nothing here is a special mode: the
// model is an ordinary NlpModel and the only option set is the lever.
//
// WHAT IS PINNED. The whole cost of the lever on this path is ONE
// factorization, and it is pinned as an identity against the lever-off arm
// with every other column asserted unmoved -- so the test fails if the charge
// is dropped (the defect), if it is double-counted, or if it moves the answer.
// ============ MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE. ============
namespace {

// An NLP whose FIRST subproblem is an indefinite QP with an interior saddle a
// single damped Newton step away, and whose start point misses the driver's
// own KKT bar by one part in 1e6 -- see the banner above for why both halves
// are needed. `eps` is the miss.
//
//     min  1/2 x0^2 - x1^2 + eps * x0   on [-1, 1]^2,   x_start = (0, 0)
//
// hess = diag(1, -2) everywhere, so the subproblem is indefinite at every
// iterate and independent of the multipliers (there are no constraints to
// carry any). The ANSWER is unambiguous and is asserted: ride the negative
// curvature to |x1| = 1 for f = -1 - O(eps), never the saddle's f = O(eps^2).
class NearSaddleIndefiniteModel : public NlpModel {
  public:
    explicit NearSaddleIndefiniteModel(double eps) : eps_(eps) {}

    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return 0.5 * x(0) * x(0) - x(1) * x(1) + eps_ * x(0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << x(0) + eps_, -2.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = -2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(2); }

  private:
    double eps_;
    Vec lower_ = Vec::Constant(2, -1.0);
    Vec upper_ = Vec::Constant(2, 1.0);
};

} // namespace

TEST(SqpDriverSsnMode, ARefusedFaceRefinementIsChargedEvenWhenTheCertificateIsWithdrawn) {
    const SqpOptions base_opts = ssn_mode_options();
    NearSaddleIndefiniteModel model(base_opts.kkt_tol * (1.0 + 1e-6));

    SqpDriver base_driver(base_opts);
    const SqpSolution base = base_driver.solve(model);

    // THE FIXTURE'S OWN PREMISE, asserted rather than assumed. Without the
    // INDEFINITE escape there is no withdrawal to reach, and this test would
    // silently degrade into a second inertness pin.
    ASSERT_EQ(base.status, SqpStatus::kOptimal);
    ASSERT_EQ(base.counters.ssn.ssn_escape_indefinite, 1)
        << "the SSN must reach a CERTIFYING exit and be refused THERE -- an in-loop escape for "
           "any other reason never defers a certificate";
    ASSERT_EQ(base.counters.ssn.ssn_escapes, 1);
    ASSERT_EQ(base.counters.ssn.ssn_refine_refused, 0)
        << "the shipped arm never reaches tier 3 on this subproblem: the exit is an escape";
    ASSERT_EQ(base.counters.ssn.ssn_refinements, 0);
    // AND THE ANSWER IS THE MINIMIZER, NOT THE SADDLE -- the escape routed to
    // the walk and the walk rode the negative curvature.
    ASSERT_LT(base.f, -0.9) << "f(saddle) is O(eps^2); f(x*) is -1 - O(eps)";

    SqpOptions lever_opts = base_opts;
    lever_opts.ssn_certify_from_face = true;
    SqpDriver lever_driver(lever_opts);
    const SqpSolution lever = lever_driver.solve(model);

    // 1. THE TRAJECTORY IS THE SAME TRAJECTORY. The withdrawal reproduces the
    // shipped verdict, so the subproblem hands off exactly as it did.
    EXPECT_EQ(lever.status, base.status);
    EXPECT_DOUBLE_EQ(lever.f, base.f);
    EXPECT_EQ(lever.counters.major_iters, base.counters.major_iters);
    EXPECT_EQ(lever.counters.qp_minor_iters, base.counters.qp_minor_iters);
    EXPECT_EQ(lever.counters.ssn.ssn_iters, base.counters.ssn.ssn_iters);
    EXPECT_EQ(lever.counters.ssn.ssn_escapes, base.counters.ssn.ssn_escapes);
    EXPECT_EQ(lever.counters.ssn.ssn_escape_indefinite, base.counters.ssn.ssn_escape_indefinite);
    EXPECT_EQ(lever.counters.ssn.ssn_escape_budget, base.counters.ssn.ssn_escape_budget);
    EXPECT_EQ(lever.counters.ssn.ssn_escape_singular, base.counters.ssn.ssn_escape_singular);
    EXPECT_EQ(lever.counters.ssn.ssn_escape_gate_refused,
              base.counters.ssn.ssn_escape_gate_refused);
    EXPECT_EQ(lever.counters.ssn.ssn_refinements, 0) << "nothing was refined -- it was refused";

    // 2. THE PIN. The lever's whole cost on this path is the hoisted face
    // solve's ONE factorization (QpEngine::refine_on_face pays at most one and
    // reports only its own), and it is VISIBLE. Before the fix all four of
    // these read the base value: the work was paid and then vanished.
    EXPECT_EQ(lever.counters.factorizations, base.counters.factorizations + 1)
        << "THE DEFECT: a refused refinement followed by a withdrawn certificate charged nothing";
    EXPECT_EQ(lever.counters.ssn.ssn_refine_refused, base.counters.ssn.ssn_refine_refused + 1)
        << "the refusal happened and must be counted as one";
    EXPECT_EQ(lever.counters.ssn.ssn_refine_factorizations,
              base.counters.ssn.ssn_refine_factorizations + 1)
        << "the instrument's numerator must agree with the total it is read against";
    EXPECT_EQ(lever.counters.ssn.ssn_refine_factorizations,
              lever.counters.factorizations - base.counters.factorizations)
        << "and the two must agree with each other, which is what makes the charge attributable";
}

// The RULE the fix is written as, pinned directly -- so a field the rare
// dynamic above cannot move (eqp_refine_steps is identically 0 out of
// refine_on_face today, see eqp_solve.h) is still falsifiable, and so the
// probe-budget charge -- which no SqpSolution column exposes -- is asserted at
// all.
TEST(SqpDriverSsnMode, TheRefusedFaceRefinementChargeIsFiveFieldsAndNoMore) {
    QpSolution refined;
    refined.counters.factorizations = 3;
    refined.counters.eqp_refine_steps = 2;
    refined.counters.minor_iters = 7;       // NOT a walk minor -- must not travel
    refined.counters.symbolic_analyses = 5; // refine_on_face never reports one

    SqpCounters total;
    Index budget_charge = 10;
    charge_refused_face_refinement(total, refined, budget_charge);

    EXPECT_EQ(total.factorizations, 3);
    EXPECT_EQ(total.eqp_refine_steps, 2);
    EXPECT_EQ(total.ssn.ssn_refine_factorizations, 3);
    EXPECT_EQ(total.ssn.ssn_refine_refused, 1) << "one refusal, not one per factorization";
    EXPECT_EQ(budget_charge, 13) << "the probe budget sees the work the driver paid";

    // The three that must NOT move. `minor_iters` is the one that would
    // corrupt every published figure in this repository (see
    // ssn_result_to_qp_solution's counter-mapping note); `ssn_refinements` is
    // the field that would turn a refusal into an acceptance.
    EXPECT_EQ(total.qp_minor_iters, 0);
    EXPECT_EQ(total.symbolic_analyses, 0);
    EXPECT_EQ(total.ssn.ssn_refinements, 0);
}

// =====================================================================
// M6 W0.3 -- THE R6 SIGN SWEEP ON EXPORTED FACE PRICES.
//
// The defect (tycho_sqp record, accepted-with-disclosure as D2): a certifying
// SSN exit's inequality prices are not sign-constrained where they are
// produced, so a NEGATIVE price reaches SqpSolution::lambda_i and
// WarmStart::lambda_i -- and through the currency's `iq_lmults_` an
// interior-point inequality seed, where the barrier's own floor absorbs it as
// though it were near-zero noise. sweep_negative_face_prices clamps it at
// SqpDriver::finish, this driver's single export boundary.
//
// THE SEMANTICS ARE PINNED HERE, ON HAND-BUILT VECTORS, and the end-to-end
// reproduction lives in tests/sqp/test_scale_smoke.cpp (the family that
// actually produces the prices runs at N in the hundreds, above this file's
// weight class). Splitting them that way keeps the EXACT assertions -- no
// tolerance, NaN untouched, the peak fold -- on a fixture that cannot wobble.
// =====================================================================

TEST(SqpDriverSignSweep, EveryStrictlyNegativePriceIsClampedAndNothingElseMoves) {
    Vec lambda_i(6);
    // Deliberately spanning the historic disclosure's own scale (max 3.5e-6)
    // and three orders below it: the sweep has NO magnitude threshold, so a
    // price at 1e-14 is swept exactly as one at 3.5e-6 is.
    lambda_i << 2.5, 0.0, -3.5e-6, 1.0e-14 * -1.0, -0.75, 4.0e-9;
    SsnCounters counters;

    sweep_negative_face_prices(lambda_i, counters);

    EXPECT_EQ(counters.ssn_sign_swept, 3);
    EXPECT_DOUBLE_EQ(counters.ssn_sign_sweep_max, 0.75) << "the PEAK magnitude clamped";
    EXPECT_DOUBLE_EQ(lambda_i(0), 2.5) << "a positive price is untouched";
    EXPECT_DOUBLE_EQ(lambda_i(1), 0.0);
    EXPECT_DOUBLE_EQ(lambda_i(2), 0.0);
    EXPECT_DOUBLE_EQ(lambda_i(3), 0.0);
    EXPECT_DOUBLE_EQ(lambda_i(4), 0.0);
    EXPECT_DOUBLE_EQ(lambda_i(5), 4.0e-9) << "a tiny POSITIVE price is not swept";
    EXPECT_GE(lambda_i.minCoeff(), 0.0);
}

TEST(SqpDriverSignSweep, ExactZeroIsNotNegativeAndAnEmptyVectorIsNotAnError) {
    // The test is `< 0.0`, not `<= 0.0`. Most rows of a solved subproblem are
    // priced at EXACTLY zero (they are off the identified face), so a
    // counter testing `<=` would report dozens on every solve -- the same
    // strictness `ssn_refine_neg_duals` states, pinned the same way.
    Vec zeros = Vec::Zero(64);
    SsnCounters counters;
    sweep_negative_face_prices(zeros, counters);
    EXPECT_EQ(counters.ssn_sign_swept, 0);
    EXPECT_DOUBLE_EQ(counters.ssn_sign_sweep_max, 0.0);

    // NEGATIVE ZERO IS NOT A NEGATIVE PRICE. `-0.0 < 0.0` is false in IEEE-754,
    // so the sweep leaves it alone and does not count it -- which is the right
    // answer twice over: it is numerically zero (already dual feasible, nothing
    // to repair) and counting it would inflate `ssn_sign_swept` with rows that
    // were never wrong. Pinned because it is the one input where "is it
    // negative" and "does it carry a minus sign" disagree.
    Vec negative_zero(2);
    negative_zero << -0.0, 0.0;
    SsnCounters nz_counters;
    sweep_negative_face_prices(negative_zero, nz_counters);
    EXPECT_EQ(nz_counters.ssn_sign_swept, 0) << "-0.0 is not a negative price";
    EXPECT_DOUBLE_EQ(nz_counters.ssn_sign_sweep_max, 0.0);
    EXPECT_DOUBLE_EQ(negative_zero(0), 0.0) << "and it compares equal to zero either way";

    Vec empty;
    sweep_negative_face_prices(empty, counters);
    EXPECT_EQ(counters.ssn_sign_swept, 0) << "a problem with no inequality rows sweeps nothing";
}

TEST(SqpDriverSignSweep, ANonFinitePriceIsLeftForTheNonFiniteExitRatherThanQuietlyRepaired) {
    // `NaN < 0.0` is false, so a NaN is NOT swept -- the same discipline the
    // B-1 ingest clear states, and for the same reason: a NaN belongs to the
    // driver's non-finite exit, which reports it, not to a repair that would
    // silently turn it into a plausible zero. -inf IS negative and IS swept,
    // which is what keeps the rule "the sign test decides" rather than "finite
    // values only".
    Vec lambda_i(3);
    lambda_i << std::numeric_limits<double>::quiet_NaN(), -std::numeric_limits<double>::infinity(),
        1.0;
    SsnCounters counters;

    sweep_negative_face_prices(lambda_i, counters);

    EXPECT_TRUE(std::isnan(lambda_i(0))) << "the NaN survives to the non-finite exit";
    EXPECT_DOUBLE_EQ(lambda_i(1), 0.0);
    EXPECT_EQ(counters.ssn_sign_swept, 1);
    EXPECT_TRUE(std::isinf(counters.ssn_sign_sweep_max));
}

TEST(SqpDriverSignSweep, TheCountersAreCumulativeAcrossCallsAndThePeakIsAPeak) {
    // finish() calls this once per solve, and accumulate_ssn_counters folds
    // the pair the same way: the COUNT sums, the MAGNITUDE peaks. A sum of two
    // peaks would not be the peak of the union, and would not be a total of
    // anything either -- the argument ssn_uncertain_peak already carries.
    SsnCounters counters;
    Vec first(2);
    first << -1.0e-3, 0.5;
    sweep_negative_face_prices(first, counters);
    Vec second(2);
    second << -1.0e-7, -2.0e-4;
    sweep_negative_face_prices(second, counters);

    EXPECT_EQ(counters.ssn_sign_swept, 3);
    EXPECT_DOUBLE_EQ(counters.ssn_sign_sweep_max, 1.0e-3) << "the largest over BOTH calls";

    SsnCounters total;
    SsnCounters a;
    a.ssn_sign_swept = 4;
    a.ssn_sign_sweep_max = 2.0e-6;
    SsnCounters b;
    b.ssn_sign_swept = 3;
    b.ssn_sign_sweep_max = 9.0e-7;
    accumulate_ssn_counters(total, a);
    accumulate_ssn_counters(total, b);
    EXPECT_EQ(total.ssn_sign_swept, 7);
    EXPECT_DOUBLE_EQ(total.ssn_sign_sweep_max, 2.0e-6);
}

// THE CLEAN-SOLVE PIN (no false sweeping). The Hock-Schittkowski battery under
// kSsn is the population this project already certifies at 1e-9 dual sign
// (test 2 of EveryHsProblemTheWalkCertifiesIsCertifiedUnderKSsn above), so
// every one of its solves prices correctly and NOTHING may be swept. Without
// this, a sweep that clamped `<= 0.0`, or one placed where it saw a
// mid-iteration vector, would still pass every assertion in the sweep's own
// unit tests.
TEST(SqpDriverSignSweep, ACorrectlySignedSolveSweepsNothingUnderEitherKernel) {
    using hven::solvers::test_support::hs_numbers;
    using hven::solvers::test_support::make_hs;

    for (int number : hs_numbers()) {
        SCOPED_TRACE(fmt::format("HS{}", number));

        auto ssn_problem = make_hs(number);
        SqpDriver ssn_driver(ssn_mode_options());
        const SqpSolution ssn = ssn_driver.solve(*ssn_problem.model);
        EXPECT_EQ(ssn.counters.ssn.ssn_sign_swept, 0)
            << "nothing on this battery prices negative, so nothing may be repaired";
        EXPECT_DOUBLE_EQ(ssn.counters.ssn.ssn_sign_sweep_max, 0.0);
        if (ssn.lambda_i.size() > 0) {
            EXPECT_GE(ssn.lambda_i.minCoeff(), 0.0);
        }

        auto walk_problem = make_hs(number);
        SqpOptions walk_opts;
        walk_opts.max_iter = 60;
        SqpDriver walk_driver(walk_opts);
        const SqpSolution walk = walk_driver.solve(*walk_problem.model);
        EXPECT_EQ(walk.counters.ssn.ssn_sign_swept, 0)
            << "the sweep is unconditional in qp_mode but structurally inert on the walk arm: an "
               "active-set price is non-negative by the drop rule";
        EXPECT_DOUBLE_EQ(walk.counters.ssn.ssn_sign_sweep_max, 0.0);
    }
}

// --- The outcome values a consumer's per-solve record needs ---
//
// Two additions to SqpSolution: the terminal KKT measurement (the four scalar
// columns taken at the point the solve returns) and the solve's wall time.
// Their contract is on the fields in sqp_types.h.
//
// The wall-time pins assert only that the field was written -- finite,
// non-negative, and nonzero after a solve that ran majors. No value is
// asserted and no two durations are compared.
TEST(SqpDriverContract, TheSolutionCarriesTheTerminalKktMeasurementAndAWallTime) {
    const HsProblem p = make_hs(6);
    SqpOptions opts;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_GE(sol.counters.major_iters, 1);

    // The scalar is the pair's own maximum -- SqpKkt::residual(), carried
    // across unchanged. Bitwise, because no arithmetic happens in between.
    EXPECT_EQ(sol.kkt_residual, std::max(sol.stationarity, sol.feasibility));

    // The pair is the converged measurement: it satisfies both gates the
    // convergence test applied, at this solve's tolerances.
    EXPECT_LE(sol.stationarity, opts.kkt_tol);
    EXPECT_LE(sol.feasibility, opts.feas_tol);
    EXPECT_TRUE(std::isfinite(sol.complementarity));

    // On this exit the returned point is the last iterate measured, so the two
    // readings agree bit for bit. Not a general rule -- see SqpSolution's field
    // note in sqp_types.h.
    ASSERT_FALSE(sol.history.empty());
    const SqpIterate &last = sol.history.back();
    EXPECT_EQ(sol.stationarity, last.stationarity);
    EXPECT_EQ(sol.feasibility, last.feasibility);
    EXPECT_EQ(sol.complementarity, last.complementarity);
    EXPECT_EQ(sol.kkt_residual, last.kkt_residual);

    // The `> 0` is a liveness pin: a solve that ran majors took measurable
    // time, so a zero here means the field was never written.
    EXPECT_TRUE(std::isfinite(sol.wall_seconds));
    EXPECT_GE(sol.wall_seconds, 0.0);
    EXPECT_GT(sol.wall_seconds, 0.0) << "the field was not populated";
}

// The non-finite-iterate exit: nothing was measured at the returned point, so
// all four residuals are NaN rather than the 0.0 a running maximum over NaN
// entries would otherwise leave.
TEST(SqpDriverContract, TheTerminalKktMeasurementIsNaNAtAnUnevaluablePoint) {
    NanPastRadiusModel model;
    SqpOptions opts;
    opts.tr_init = 1e3;
    opts.max_iter = 10;
    SqpDriver driver(opts);

    Vec x0_outside(1);
    x0_outside << 50.0;
    const SqpSolution bad = driver.solve(model, x0_outside);

    ASSERT_EQ(bad.status, SqpStatus::kNumericalError);
    EXPECT_TRUE(std::isnan(bad.stationarity)) << "read " << bad.stationarity;
    EXPECT_TRUE(std::isnan(bad.feasibility)) << "read " << bad.feasibility;
    EXPECT_TRUE(std::isnan(bad.complementarity)) << "read " << bad.complementarity;
    EXPECT_TRUE(std::isnan(bad.kkt_residual)) << "read " << bad.kkt_residual;

    // The wall time is still populated: the solve ran, it just could not
    // measure the model.
    EXPECT_TRUE(std::isfinite(bad.wall_seconds));
    EXPECT_GE(bad.wall_seconds, 0.0);
}

// The certified-infeasible exit: the four residuals measure the NLP's own KKT
// conditions at the returned point, which is infeasible -- so `feasibility` is
// large by construction. They are not the subgradient certificate's residual;
// SqpDriverRestoration.InfeasibleNlpCertifies re-derives that from the model.
TEST(SqpDriverContract, TheTerminalKktMeasurementOnACertifiedInfeasibleExitMeasuresTheNlp) {
    InfeasibleCircleLineModel model;
    SqpOptions opts;
    opts.max_iter = 200;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(sol.status, SqpStatus::kInfeasible);
    ASSERT_TRUE(sol.infeasibility_certified);

    EXPECT_GT(sol.feasibility, opts.feas_tol)
        << "the returned point is infeasible; a small feasibility measure here would mean "
           "these columns describe the restoration problem instead of the NLP";
    EXPECT_EQ(sol.kkt_residual, std::max(sol.stationarity, sol.feasibility));
    EXPECT_TRUE(std::isfinite(sol.stationarity));
    EXPECT_TRUE(std::isfinite(sol.complementarity));
    EXPECT_GE(sol.wall_seconds, 0.0);
}
