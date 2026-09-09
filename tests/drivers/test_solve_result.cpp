// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The result core: the shared declared diagnostics, the snapshot export, the
// interior-point phase sequence and the shared budget.
//
// THE DIAGNOSTICS PINS BELOW ARE INDEPENDENT COMPUTATIONS. Nothing here asks an
// engine what it measured; the model quantities are written out by hand at a
// point whose KKT multipliers are known, and the expected answers follow from
// the definitions in drivers/solve_result.h rather than from any engine's own
// convergence test. That is the whole point of the pin: if the shared
// definition ever drifts toward one engine's internal measure, these fail.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <hven/drivers/interior_point_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_model_aggregate.h>
#include <hven/model/nlp_solver.h>

#include "sqp/support/hs_problems.h"
#include "support/hs071_problem.h"

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::compute_declared_diagnostics;
using hven::solvers::DeclaredDiagnostics;

namespace {

/// An empty m x n sparse block, for a problem class the fixture does not use.
SpMatRM empty_block(Index rows, Index cols) { return SpMatRM(rows, cols); }

/// A dense row vector as a one-row sparse Jacobian block.
SpMatRM row_block(const std::vector<double> &row) {
    const Index n = static_cast<Index>(row.size());
    SpMatRM m(1, n);
    std::vector<Eigen::Triplet<double>> t;
    for (Index j = 0; j < n; ++j) {
        t.emplace_back(0, j, row[static_cast<std::size_t>(j)]);
    }
    m.setFromTriplets(t.begin(), t.end());
    m.makeCompressed();
    return m;
}

Vec vec_of(const std::vector<double> &v) {
    Vec out(static_cast<Index>(v.size()));
    for (std::size_t i = 0; i < v.size(); ++i) {
        out(static_cast<Index>(i)) = v[i];
    }
    return out;
}

constexpr double kInf = std::numeric_limits<double>::infinity();

} // namespace

// ===========================================================================
// (1) The definition closes on a genuine KKT point.
// ===========================================================================

// HS071 -- min x1*x4*(x1+x2+x3) + x3, subject to
//   x1*x2*x3*x4 >= 25, x1^2+x2^2+x3^2+x4^2 = 40, 1 <= xi <= 5.
//
// In hven's declared convention the inequality is written ci <= 0, so
//   ce(x) = x1^2+x2^2+x3^2+x4^2 - 40,   ci(x) = 25 - x1*x2*x3*x4,
// and the Lagrangian is L = f + lambda_e*ce + lambda_i*ci - z.
//
// THE POINT AND ITS PRICES BELOW WERE COMPUTED OUTSIDE THIS TREE, by Newton's
// method on the five KKT equations (stationarity in x2, x3, x4; the equality
// row; the active inequality row) with x1 pinned at its lower bound -- not by
// running either hven engine on the problem. x1 sits ON its lower bound, which
// is what makes z(0) non-zero and gives the canonical bound product something
// to price.
TEST(DeclaredDiagnostics, ClosesOnAKktPointOfHs071) {
    const double x1 = 1.0;
    const double x2 = 4.7429996372644174;
    const double x3 = 3.8211499841848742;
    const double x4 = 1.3794082931726723;
    const double lambda_e = 0.16146856677050586;
    const double lambda_i = 0.55229366012072689;

    const Vec x = vec_of({x1, x2, x3, x4});
    // grad f, by hand: d/dx1 = x4*(2*x1+x2+x3), d/dx2 = x1*x4,
    //                  d/dx3 = x1*x4 + 1,       d/dx4 = x1*(x1+x2+x3).
    const Vec grad =
        vec_of({x4 * (2.0 * x1 + x2 + x3), x1 * x4, x1 * x4 + 1.0, x1 * (x1 + x2 + x3)});
    // Je = grad(sum xi^2) = 2x; Ji = grad(25 - prod xi) = -(the cofactors).
    const SpMatRM Je = row_block({2.0 * x1, 2.0 * x2, 2.0 * x3, 2.0 * x4});
    const SpMatRM Ji =
        row_block({-(x2 * x3 * x4), -(x1 * x3 * x4), -(x1 * x2 * x4), -(x1 * x2 * x3)});
    const Vec ce = vec_of({x1 * x1 + x2 * x2 + x3 * x3 + x4 * x4 - 40.0});
    const Vec ci = vec_of({25.0 - x1 * x2 * x3 * x4});

    // z is the bound price: zero on the three interior coordinates, and on the
    // coordinate at its lower bound it is exactly what stationarity leaves --
    // grad L there. Written out rather than read back from the function under
    // test.
    const double grad_lag_1 = grad(0) + lambda_e * 2.0 * x1 + lambda_i * (-(x2 * x3 * x4));
    const Vec z = vec_of({grad_lag_1, 0.0, 0.0, 0.0});
    EXPECT_NEAR(grad_lag_1, 1.0878712286669412, 1e-12);

    const DeclaredDiagnostics d = compute_declared_diagnostics(
        x, vec_of({lambda_e}), vec_of({lambda_i}), z, grad, Je, Ji, ce, ci,
        vec_of({1.0, 1.0, 1.0, 1.0}), vec_of({5.0, 5.0, 5.0, 5.0}), {});

    EXPECT_LT(d.stationarity, 1e-9);
    EXPECT_LT(d.feasibility_e, 1e-9);
    // The inequality is ACTIVE here, so its residual is 0 rather than negative;
    // the positive part is 0 either way and no bound is violated.
    EXPECT_LT(d.feasibility_i, 1e-9);
    // Both complementarity families vanish at this point for DIFFERENT reasons:
    // lambda_i * ci because ci is 0 at an active row, and the lower-bound
    // product because x1 - l1 is 0 at a bound the price is paid at. A
    // definition that dropped either family would still read 0 here, which is
    // why the two tests below pin the families themselves.
    EXPECT_LT(d.complementarity, 1e-9);
}

// ===========================================================================
// (2) The canonical two-sided bound split.
// ===========================================================================

// A signed z = zL - zU cannot recover two separate prices at a two-sided bound
// where both are positive. The shared diagnostic therefore prices the LOWER
// side with max(z,0) and the UPPER side with max(-z,0), and this pins that
// choice against the alternative it is not.
TEST(DeclaredDiagnostics, TwoSidedBoundUsesCanonicalSplit) {
    // One variable, strictly inside [0, 1], with a signed price that came from
    // zL = 2 and zU = 3 -- both strictly positive, which is the case the split
    // exists for.
    const Vec x = vec_of({0.5});
    const Vec z = vec_of({2.0 - 3.0});
    const Vec grad = vec_of({z(0)}); // stationary at that price: grad - z = 0
    const Vec empty(0);

    const DeclaredDiagnostics d =
        compute_declared_diagnostics(x, empty, empty, z, grad, empty_block(0, 1), empty_block(0, 1),
                                     empty, empty, vec_of({0.0}), vec_of({1.0}), {});

    EXPECT_NEAR(d.stationarity, 0.0, 1e-15);
    // z is negative, so the LOWER product max(z,0)*(x-l) is 0 and the UPPER
    // product max(-z,0)*(u-x) = 1 * 0.5 carries the whole measure.
    EXPECT_NEAR(d.complementarity, 0.5, 1e-15);
    // What it is NOT: the true two-price measure max(zL*(x-l), zU*(u-x)) would
    // read max(2*0.5, 3*0.5) = 1.5 here. The canonical split cannot see zL and
    // zU separately and does not pretend to -- an engine that holds both keeps
    // its own measure on its own result.
    EXPECT_NE(d.complementarity, 1.5);
}

// ===========================================================================
// (3) An excluded coordinate is not measured.
// ===========================================================================

// Under the interior-point engine's MakeParameter treatment an eliminated
// variable has no row in the reduced problem at all: the reduced gradient
// reports 0 there, and a 0 that means "no row" must not enter an inf-norm
// beside 0s that mean "stationary". The exclusion list is how the engine says
// so.
TEST(DeclaredDiagnostics, ExcludedCoordinateIsNotMeasured) {
    // Coordinate 0 carries a large residual; coordinate 1 a tiny one. No
    // constraints, no finite bounds, so stationarity is the whole measurement.
    const Vec x = vec_of({3.0, 7.0});
    const Vec grad = vec_of({100.0, 1e-9});
    const Vec z = vec_of({0.0, 0.0});
    const Vec empty(0);
    const Vec lower = vec_of({-kInf, -kInf});
    const Vec upper = vec_of({kInf, kInf});

    const DeclaredDiagnostics measured =
        compute_declared_diagnostics(x, empty, empty, z, grad, empty_block(0, 2), empty_block(0, 2),
                                     empty, empty, lower, upper, {});
    EXPECT_NEAR(measured.stationarity, 100.0, 1e-15);

    const DeclaredDiagnostics excluded =
        compute_declared_diagnostics(x, empty, empty, z, grad, empty_block(0, 2), empty_block(0, 2),
                                     empty, empty, lower, upper, {0});
    EXPECT_NEAR(excluded.stationarity, 1e-9, 1e-18);

    // An infinite bound prices nothing: 0 * inf is not a measurement, and the
    // complementarity here has no finite bound and no inequality row to read.
    EXPECT_NEAR(excluded.complementarity, 0.0, 1e-15);

    // Excluding EVERY coordinate leaves nothing measured, which is NaN and not
    // zero -- the same discipline the rest of the result core keeps.
    const DeclaredDiagnostics none =
        compute_declared_diagnostics(x, empty, empty, z, grad, empty_block(0, 2), empty_block(0, 2),
                                     empty, empty, lower, upper, {0, 1});
    EXPECT_TRUE(std::isnan(none.stationarity));
    // The other three were still measured: exclusion is about the stationarity
    // coordinates alone.
    EXPECT_FALSE(std::isnan(none.feasibility_e));
    EXPECT_FALSE(std::isnan(none.feasibility_i));
    EXPECT_FALSE(std::isnan(none.complementarity));
}

// ===========================================================================
// (4) The interior-point engine's half of the result core: phases, the budget,
//     the model-taking pattern query and the export snapshot.
// ===========================================================================
//
// These need a live solve, which is why they sit after the definition pins
// above rather than beside them: what they check is that the ENGINE fills the
// shared shape correctly, not what the shape means.

TEST(IpmPhases, TrailingSolveIsConditionalOnThePrecedingOptimize) {
    // {kOptimize, kSolve} -- what the old optimize_solve() entry ran. The
    // trailing solve is conditional on the phase BEFORE it, so on a problem the
    // optimize phase converges it never runs at all.
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    o.phases = {hven::solvers::IpmPhase::kOptimize, hven::solvers::IpmPhase::kSolve};
    hven::solvers::InteriorPointSolver solver(o);

    hven_drivers_tests::Hs071Problem problem;
    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();

    const hven::solvers::IpmResult converged =
        solver.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    ASSERT_EQ(converged.status, hven::solvers::SolveStatus::kOptimal);
    ASSERT_EQ(converged.phases.size(), 2u);
    EXPECT_EQ(converged.phases[0].phase, hven::solvers::IpmPhase::kOptimize);
    EXPECT_TRUE(converged.phases[0].ran);
    EXPECT_EQ(converged.phases[0].status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(converged.phases[1].phase, hven::solvers::IpmPhase::kSolve);
    EXPECT_FALSE(converged.phases[1].ran) << "a kSolve after a converged kOptimize must not run";
    EXPECT_EQ(converged.phases[1].iterations, 0);
    // The call's own account is the phases that RAN: the sum, and the last
    // ran phase's verdict.
    EXPECT_EQ(converged.iterations, converged.phases[0].iterations);

    // The same sequence with the optimize phase capped so it cannot converge:
    // now the trailing solve DOES run, off exactly the same rule.
    hven::solvers::IpmOptions capped = o;
    capped.max_iters = 1;
    hven::solvers::InteriorPointSolver tight(capped);
    const hven::solvers::IpmResult exhausted =
        tight.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    ASSERT_EQ(exhausted.phases.size(), 2u);
    EXPECT_TRUE(exhausted.phases[0].ran);
    EXPECT_NE(exhausted.phases[0].status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_TRUE(exhausted.phases[1].ran) << "a kSolve after a NON-converged kOptimize runs";
    EXPECT_EQ(exhausted.iterations,
              exhausted.phases[0].iterations + exhausted.phases[1].iterations);
}

// THE PER-PHASE VERDICT, which is the whole of the defect design §2.3
// registered for this task. Before T8.4 the stop reason was phase-scoped and
// the verdict was per CALL, so a later phase that left without assigning one
// reported the EARLIER phase's answer. The engine resets the verdict at every
// phase start now, so the two cannot disagree.
TEST(IpmPhases, EachPhaseCarriesItsOwnVerdict) {
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    // {kSolve, kOptimize} -- the old solve_optimize(). The second phase is
    // UNCONDITIONAL, so both always run, and a cap tight enough to stop the
    // optimize phase leaves the feasibility phase's own verdict standing beside
    // it rather than overwriting it.
    o.phases = {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize};
    o.max_iters = 10;
    hven::solvers::InteriorPointSolver solver(o);

    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    const hven::solvers::IpmResult r = solver.solve(*model.nlp_, x0);

    ASSERT_EQ(r.phases.size(), 2u);
    ASSERT_TRUE(r.phases[0].ran);
    ASSERT_TRUE(r.phases[1].ran);
    // The call reports the LAST RAN phase's status, and the first phase's own
    // verdict is still readable beside it.
    EXPECT_EQ(r.status, r.phases[1].status);
    EXPECT_EQ(r.phases[1].status, hven::solvers::SolveStatus::kMaxIter);
    EXPECT_EQ(r.phases[1].stop_reason, hven::solvers::IpmStopReason::kIterationCap);
    // The feasibility phase ended on its own verdict, not on the cap.
    EXPECT_LT(r.phases[0].iterations, o.max_iters);
    EXPECT_EQ(r.phases[0].stop_reason, hven::solvers::IpmStopReason::kNone);
    // Both phases are timed separately, and neither timing is the call's.
    EXPECT_GE(r.phases[0].phase_seconds, 0.0);
    EXPECT_GE(r.phases[1].phase_seconds, 0.0);
    EXPECT_GE(r.wall_seconds, 0.0);
}

TEST(IpmPhases, EmptySequenceIsRefused) {
    hven::solvers::IpmOptions o;
    o.phases.clear();
    EXPECT_THROW(hven::solvers::validate(o), std::invalid_argument);
    // And the refusal reaches a caller through every door that validates.
    EXPECT_THROW(hven::solvers::InteriorPointSolver{o}, std::invalid_argument);
}

TEST(SolveBudget, IpmMaxIterationsCapsEachPhase) {
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    o.max_iters = 200;
    hven::solvers::InteriorPointSolver solver(o);

    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();

    const hven::solvers::IpmResult capped =
        solver.solve(*model.nlp_, hven_drivers_tests::hs071_start(),
                     hven::solvers::SolveBudget{/*minor_budget=*/0, /*max_iterations=*/2});
    EXPECT_EQ(capped.status, hven::solvers::SolveStatus::kMaxIter);
    EXPECT_LE(capped.iterations, 2);
    ASSERT_EQ(capped.phases.size(), 1u);
    EXPECT_EQ(capped.phases[0].stop_reason, hven::solvers::IpmStopReason::kIterationCap);

    // TIGHTEN ONLY: a budget ABOVE the engine's own limit does not raise it.
    hven::solvers::IpmOptions tight = o;
    tight.max_iters = 2;
    hven::solvers::InteriorPointSolver small(tight);
    const hven::solvers::IpmResult still_capped = small.solve(
        *model.nlp_, hven_drivers_tests::hs071_start(), hven::solvers::SolveBudget{0, 10000});
    EXPECT_EQ(still_capped.status, hven::solvers::SolveStatus::kMaxIter);
    EXPECT_LE(still_capped.iterations, 2);

    // AND THE DEFAULT BUDGET IS THE IDENTITY, which is what makes the whole
    // feature trajectory-neutral: the same solve with no budget named converges.
    const hven::solvers::IpmResult unbudgeted =
        solver.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    EXPECT_EQ(unbudgeted.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_GT(unbudgeted.iterations, 2);
}

TEST(IpmIntrospection, PatternQueryTakesTheModel) {
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    hven::solvers::InteriorPointSolver solver(o);

    auto first = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    first.transcribe();
    auto second = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    second.transcribe();

    // Nothing analysed yet: no program answers true.
    EXPECT_FALSE(solver.kkt_pattern_is_analyzed(*first.nlp_));
    EXPECT_FALSE(solver.kkt_pattern_is_analyzed(*second.nlp_));

    const hven::solvers::IpmResult r = solver.solve(*first.nlp_, hven_drivers_tests::hs071_start());
    ASSERT_EQ(r.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_TRUE(solver.kkt_pattern_is_analyzed(*first.nlp_));

    // A SECOND PROGRAM OF THE SAME DECLARED STRUCTURE answers FALSE. Its
    // structure key and its structure epoch both match the analysed one -- every
    // epoch counter starts at 0 -- so a token made of those two alone would say
    // true here, and a solve would then scatter through location tables that
    // are all -1. This is the case that makes the third conjunct load-bearing.
    EXPECT_FALSE(solver.kkt_pattern_is_analyzed(*second.nlp_))
        << "a different program of identical structure is not the analysed program";

    // Solving the second re-analyses, and the first stops being the answer.
    const hven::solvers::IpmResult r2 =
        solver.solve(*second.nlp_, hven_drivers_tests::hs071_start());
    ASSERT_EQ(r2.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_TRUE(solver.kkt_pattern_is_analyzed(*second.nlp_));
    EXPECT_FALSE(solver.kkt_pattern_is_analyzed(*first.nlp_));
    EXPECT_EQ(r2.kkt_analyses_this_call, 1);
    EXPECT_GT(r2.kkt_analyses_total, r.kkt_analyses_total);

    // A REPEAT solve of the program already analysed re-analyses nothing.
    const hven::solvers::IpmResult r3 =
        solver.solve(*second.nlp_, hven_drivers_tests::hs071_start());
    EXPECT_EQ(r3.kkt_analyses_this_call, 0);
    EXPECT_EQ(r3.kkt_analyses_total, r2.kkt_analyses_total);
}

TEST(SolveResult, ExportIsASnapshotIndependentOfLaterEdits) {
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    hven::solvers::InteriorPointSolver solver(o);
    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();

    hven::solvers::IpmResult r = solver.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    ASSERT_EQ(r.status, hven::solvers::SolveStatus::kOptimal);

    const auto before = r.export_warm_start();
    ASSERT_TRUE(before.has_value());
    ASSERT_EQ(before->primal_.size(), 4);
    const Eigen::VectorXd captured = before->primal_;

    // Scribble on every public field. The snapshot was taken at solve exit and
    // is not a view of these.
    r.x.setZero();
    r.lambda_e.setZero();
    r.lambda_i.setZero();
    r.z.setZero();
    r.f = 0.0;
    r.status = hven::solvers::SolveStatus::kNumericalError;

    const auto after = r.export_warm_start();
    ASSERT_TRUE(after.has_value());
    ASSERT_EQ(after->primal_.size(), captured.size());
    for (Index i = 0; i < captured.size(); ++i) {
        EXPECT_EQ(after->primal_[i], captured[i]);
    }
    EXPECT_GT(after->primal_.lpNorm<Eigen::Infinity>(), 0.0)
        << "the export still carries the solution, not the zeroed field";
}

// The four shared diagnostics on a live interior-point solve, checked against
// an INDEPENDENT computation over the same returned point. This is the pin that
// ties §(1)'s definition to what the engine actually reports.
TEST(SolveResult, TheIpmFillsTheSharedDiagnosticsAtItsReturnedPoint) {
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    hven::solvers::InteriorPointSolver solver(o);
    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();

    const hven::solvers::IpmResult r = solver.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    ASSERT_EQ(r.status, hven::solvers::SolveStatus::kOptimal);

    // Declared shapes, in the caller's space.
    ASSERT_EQ(r.x.size(), 4);
    ASSERT_EQ(r.z.size(), 4);
    ASSERT_EQ(r.lambda_e.size(), 1);
    ASSERT_EQ(r.lambda_i.size(), 1);
    ASSERT_EQ(r.ce.size(), 1);
    ASSERT_EQ(r.ci.size(), 1);

    // Every diagnostic MEASURED (not NaN), and every one small at a converged
    // KKT point of HS071.
    EXPECT_FALSE(std::isnan(r.stationarity));
    EXPECT_FALSE(std::isnan(r.feasibility_e));
    EXPECT_FALSE(std::isnan(r.feasibility_i));
    EXPECT_FALSE(std::isnan(r.complementarity));
    EXPECT_LT(r.stationarity, 1e-6);
    EXPECT_LT(r.feasibility_e, 1e-6);
    EXPECT_LT(r.feasibility_i, 1e-6);
    EXPECT_LT(r.complementarity, 1e-6);

    // INDEPENDENTLY: the same four from the shared function, over the model
    // quantities written out by hand at the returned point -- so a wrong
    // reduced->declared mapping in the engine's own fill would show here.
    const double x1 = r.x[0], x2 = r.x[1], x3 = r.x[2], x4 = r.x[3];
    const Vec grad =
        vec_of({x4 * (2.0 * x1 + x2 + x3), x1 * x4, x1 * x4 + 1.0, x1 * (x1 + x2 + x3)});
    const SpMatRM Je = row_block({2.0 * x1, 2.0 * x2, 2.0 * x3, 2.0 * x4});
    const SpMatRM Ji =
        row_block({-(x2 * x3 * x4), -(x1 * x3 * x4), -(x1 * x2 * x4), -(x1 * x2 * x3)});
    const DeclaredDiagnostics d = compute_declared_diagnostics(
        r.x, r.lambda_e, r.lambda_i, r.z, grad, Je, Ji, r.ce, r.ci, vec_of({1.0, 1.0, 1.0, 1.0}),
        vec_of({5.0, 5.0, 5.0, 5.0}), {});
    EXPECT_NEAR(d.stationarity, r.stationarity, 1e-9);
    EXPECT_NEAR(d.feasibility_e, r.feasibility_e, 1e-12);
    EXPECT_NEAR(d.feasibility_i, r.feasibility_i, 1e-12);
    EXPECT_NEAR(d.complementarity, r.complementarity, 1e-9);
}

// ===========================================================================
// (5) The SQP engine's half of the result core.
// ===========================================================================

namespace {

/// A model whose evaluation is NaN everywhere: the SQP's non-finite-start exit.
///
/// The point of the fixture is that NOTHING can be measured at the returned
/// point, so the result has to say so rather than fill zeros.
class NonFiniteModel final : public hven::solvers::NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &) const override { return std::numeric_limits<double>::quiet_NaN(); }
    Vec eval_grad(const Vec &) const override {
        return Vec::Constant(2, std::numeric_limits<double>::quiet_NaN());
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, 2); }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInf);
        return u;
    }
    Vec start_point() const override { return Vec::Zero(2); }
};

hven::solvers::SqpOptions quiet_sqp() {
    hven::solvers::SqpOptions o;
    o.common.print_level = 10;
    return o;
}

} // namespace

TEST(SolveResult, UnmeasuredExitsReportNaNAndEmptyVectors) {
    NonFiniteModel model;
    hven::solvers::SqpDriver driver(quiet_sqp());
    const hven::solvers::SqpResult r = driver.solve(model, model.start_point());

    EXPECT_EQ(r.status, hven::solvers::SolveStatus::kNumericalError);
    // NaN means UNMEASURED. A 0.0 in any of these would read as a converged
    // residual at a point the model could not even be evaluated at.
    EXPECT_TRUE(std::isnan(r.stationarity));
    EXPECT_TRUE(std::isnan(r.feasibility_e));
    EXPECT_TRUE(std::isnan(r.feasibility_i));
    EXPECT_TRUE(std::isnan(r.complementarity));
    // EMPTY, not zero-filled: the problem declares no rows here, and on a
    // problem that did they would still be empty, because nothing was measured.
    EXPECT_EQ(r.ce.size(), 0);
    EXPECT_EQ(r.ci.size(), 0);
    // The engine's own four say the same thing under their own names.
    EXPECT_TRUE(std::isnan(r.sqp_stationarity));
    EXPECT_TRUE(std::isnan(r.sqp_feasibility));
    EXPECT_TRUE(std::isnan(r.sqp_complementarity));

    // AND NO FRESH EVALUATION WAS TAKEN TO FIND THAT OUT. One evaluation, the
    // one at x0 that failed. This is the whole reason the diagnostics are
    // computed from a stash: an exit that measured nothing must not pay for a
    // measurement, and no path may move the evaluation bill.
    EXPECT_EQ(r.counters.evals_full, 1);

    // The export is still present -- this engine's is never nullopt, even here:
    // the returned point is the caller's own finite x0 with zero multipliers.
    EXPECT_TRUE(r.export_warm_start().has_value());
}

TEST(SolveResult, TheSqpFillsTheSharedDiagnosticsAtItsReturnedPoint) {
    const auto p = hven::solvers::test_support::make_hs(7);
    hven::solvers::SqpDriver driver(quiet_sqp());
    const hven::solvers::SqpResult r = driver.solve(*p.model, p.model->start_point());
    ASSERT_EQ(r.status, hven::solvers::SolveStatus::kOptimal);

    // Measured, small, and reported in the declared shapes.
    EXPECT_FALSE(std::isnan(r.stationarity));
    EXPECT_LT(r.stationarity, 1e-6);
    EXPECT_LT(r.feasibility_e, 1e-6);
    EXPECT_LT(r.feasibility_i, 1e-6);
    EXPECT_FALSE(std::isnan(r.complementarity));
    EXPECT_EQ(r.ce.size(), p.model->me());
    EXPECT_EQ(r.ci.size(), p.model->mi());
    EXPECT_EQ(r.x.size(), p.model->n());
    EXPECT_EQ(r.z.size(), p.model->n());

    // INDEPENDENTLY, from the shared function over the model's own quantities
    // at the returned point. This is what ties the engine's fill to §(1)'s
    // definition; it evaluates the model itself, which a SOLVE may not do.
    const hven::solvers::NlpEval ev = hven::solvers::eval_nlp(*p.model, r.x);
    const DeclaredDiagnostics d =
        compute_declared_diagnostics(r.x, r.lambda_e, r.lambda_i, r.z, ev.grad, ev.Je, ev.Ji, ev.ce,
                                     ev.ci, p.model->lower(), p.model->upper(), {});
    EXPECT_NEAR(d.stationarity, r.stationarity, 1e-9);
    EXPECT_NEAR(d.feasibility_e, r.feasibility_e, 1e-12);
    EXPECT_NEAR(d.feasibility_i, r.feasibility_i, 1e-12);
    EXPECT_NEAR(d.complementarity, r.complementarity, 1e-9);

    // THE TWO CLOCKS ARE DIFFERENT MEASUREMENTS, and both are present: the
    // base's runs from the public entry, the engine's around solve_impl alone,
    // so the first is never the smaller.
    EXPECT_GE(r.wall_seconds, r.solve_impl_seconds);
    // The shared iteration count is the TOP-LEVEL major count.
    EXPECT_EQ(r.iterations, r.counters.major_iters);

    // And the export snapshot is a snapshot: editing the public fields does not
    // reach it.
    hven::solvers::SqpResult edited = r;
    const auto before = edited.export_warm_start();
    ASSERT_TRUE(before.has_value());
    edited.x.setZero();
    const auto after = edited.export_warm_start();
    ASSERT_TRUE(after.has_value());
    EXPECT_GT(after->primal_.template lpNorm<Eigen::Infinity>(), 0.0);
}

namespace {

/// min 0.5||x||^2 s.t. x0 = 5, over the box [0,1]^2 -- an equality the box
/// blocks, so the elastic tier exhausts and the RESTORATION phase runs and
/// certifies at x = (1, 0). Copied from tests/sqp/test_sqp_restoration.cpp's
/// fixture of the same name, because this directory links no SQP fixtures.
class BoxBlockedEqualityModel final : public hven::solvers::NlpModel {
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
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1.0;
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
    Vec start_point() const override { return Vec::Constant(2, 0.5); }
};

} // namespace

TEST(SolveBudget, SqpRestorationIsBudgetedFromTheEffectiveCap) {
    // The effective cap governs the exit conjunction, the restoration REFUSAL
    // and the restoration sub-driver's budget alike -- three reads, and a sweep
    // that reached only some of them would let a capped solve enter restoration
    // it has no budget for.
    //
    // ON A RESTORATION FIXTURE (M6 W5 T8.4 fix1). T8.4 ran this on HS7, which
    // never restores, so the two reads a partial sweep would have missed were
    // never exercised and the test could not tell a swept implementation from
    // an unswept one. This model's equality is blocked by its box, so the
    // elastic tier exhausts and the phase RUNS -- asserted first, because
    // everything below is vacuous without it.
    BoxBlockedEqualityModel model;
    hven::solvers::SqpOptions o = quiet_sqp();
    o.max_iter = 200;
    hven::solvers::SqpDriver driver(o);

    const hven::solvers::SqpResult free_run = driver.solve(model, model.start_point());
    ASSERT_EQ(free_run.status, hven::solvers::SolveStatus::kInfeasible);
    ASSERT_GT(free_run.counters.restoration_iters, 0)
        << "the restoration phase must RUN, or this test measures nothing";
    const Index unbudgeted_total =
        free_run.counters.major_iters + free_run.counters.restoration_iters;
    ASSERT_GT(unbudgeted_total, 2);

    // THE SUB-BUDGET READ: a cap the solve reaches only by spending majors on
    // both phases. The sub-driver is allocated from the EFFECTIVE cap minus
    // what the outer loop has already spent, so the two together stay inside it.
    const hven::solvers::SqpResult capped =
        driver.solve(model, model.start_point(),
                     hven::solvers::SolveBudget{/*minor_budget=*/0, /*max_iterations=*/2});
    EXPECT_LE(capped.counters.major_iters + capped.counters.restoration_iters, 2)
        << "the cap bounds the majors AND the restoration majors together";

    // THE REFUSAL READ: at a cap of ONE the outer loop's first major has
    // already spent the whole budget, so the request is REFUSED rather than
    // sub-budgeted -- no restoration major is spent at all.
    const hven::solvers::SqpResult refused =
        driver.solve(model, model.start_point(), hven::solvers::SolveBudget{0, 1});
    EXPECT_LE(refused.counters.major_iters, 1);
    EXPECT_EQ(refused.counters.restoration_iters, 0)
        << "a spent budget refuses the phase rather than entering it";

    // TIGHTEN ONLY: a budget above the engine's own limit does not raise it,
    // and the ENGINE's own max_iter reaches the same three reads.
    hven::solvers::SqpOptions tight = quiet_sqp();
    tight.max_iter = 2;
    hven::solvers::SqpDriver small(tight);
    const hven::solvers::SqpResult still =
        small.solve(model, model.start_point(), hven::solvers::SolveBudget{0, 100000});
    EXPECT_LE(still.counters.major_iters + still.counters.restoration_iters, 2);

    // THE DEFAULT BUDGET IS THE IDENTITY, which is what makes this
    // trajectory-neutral on every existing path: the unbudgeted solve above
    // spent more than any cap here allowed and reached its own exit.
    EXPECT_GT(unbudgeted_total, 2);
}

// ===========================================================================
// (6) EVALUATION PROVENANCE (M6 W5 T8.4 fix1).
//
// The four shared diagnostics are a statement about the RETURNED point of the
// DECLARED problem. Three things can be true of the evaluation an engine has
// in hand at its exit, and none of them can be read off the captured vector's
// size: it may carry no declared objective gradient (a feasibility phase, or a
// restoration seam substituting its own objective), it may belong to an
// iterate a restoration return has since moved away from, and its constraint
// rows may be a restoration subproblem's condensed residuals rather than the
// declared ones. Each pin below is one of those.
// ===========================================================================

namespace {

/// c(x) = x0^2 + x1^2 + 1 = 0 has no real solution, and the violation has a
/// STRICT stationary point at the origin -- the shape a restoration phase
/// converges to and then declares locally infeasible. A copy, for the reason
/// the bench leg's own copy states: a test in this directory links no interior
/// fixtures.
struct LocallyInfeasibleProblem final : hven::solvers::NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kInf, -kInf;
        xu << kInf, kInf;
        gl << 0.0;
        gu << 0.0;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] + x[1];
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd>,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 1.0, 1.0;
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[0] + x[1] * x[1] + 1.0;
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * x[0], 2.0 * x[1];
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd>, double,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * lambda[0], 2.0 * lambda[0];
    }
    std::string name() const override { return "LocallyInfeasibleProblem"; }
};

Eigen::VectorXd two_var_start(double a, double b) {
    Eigen::VectorXd x(2);
    x << a, b;
    return x;
}

} // namespace

TEST(SolveResult, AFeasibilityOnlyLastPhaseReportsStationarityUnmeasured) {
    // THE SETTLER'S S1 / the lane's Critical, pinned on both sides.
    //
    // A `{kSolve}` call runs the feasibility phase alone. That phase evaluates
    // through eval_soe at objective scale 0.0 and then zeroes both primal
    // blocks, so the right-hand side it leaves behind carries NO declared
    // objective gradient at all -- and the T8.4 result assembly nevertheless
    // divided that vector by the objective scale and reported it as a measured
    // declared stationarity. On the bench leg's `infeas2_spike` cell that
    // printed 0.000000000e+00 for a problem whose objective is f = x[1] and
    // whose honest declared stationarity is at least 1.
    //
    // The honest REPORT is NaN: no declared-objective evaluation of the
    // returned point exists, and manufacturing one would move evals_full on
    // every solve.
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    o.phases = {hven::solvers::IpmPhase::kSolve};
    hven::solvers::InteriorPointSolver solver(o);
    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();

    const hven::solvers::IpmResult r = solver.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    ASSERT_EQ(r.status, hven::solvers::SolveStatus::kOptimal)
        << "the feasibility phase does find a feasible point of HS071";
    ASSERT_EQ(r.phases.size(), 1u);
    EXPECT_EQ(r.phases[0].phase, hven::solvers::IpmPhase::kSolve);

    EXPECT_TRUE(std::isnan(r.stationarity))
        << "a feasibility phase carries no objective gradient, so the declared "
           "stationarity of its exit is UNMEASURED, never 0";
    // THE OTHER THREE ARE MEASURED AND KEPT. They read ce, ci, x, the box,
    // lambda_i and z -- every one of which a feasibility phase does evaluate --
    // so NaN-ing them would throw away real measurements.
    EXPECT_FALSE(std::isnan(r.feasibility_e));
    EXPECT_FALSE(std::isnan(r.feasibility_i));
    EXPECT_FALSE(std::isnan(r.complementarity));
    EXPECT_LT(r.feasibility_e, 1e-6);
    // And `f` is measured: the exit assembles the true objective at the
    // returned primals on every non-OPT exit.
    EXPECT_FALSE(std::isnan(r.f));
    EXPECT_EQ(r.ce.size(), 1);
    EXPECT_EQ(r.ci.size(), 1);
}

TEST(SolveResult, AnObjectiveBearingLastPhaseReportsAllFour) {
    // The other side of the same rule: a `{kSolve, kOptimize}` call ends in an
    // objective-bearing phase, so all four are measured. The feasibility phase
    // having run FIRST does not taint them -- the provenance is a fact about
    // the LAST evaluation, not about the sequence.
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    o.phases = {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize};
    hven::solvers::InteriorPointSolver solver(o);
    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();

    const hven::solvers::IpmResult r = solver.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    ASSERT_EQ(r.status, hven::solvers::SolveStatus::kOptimal);
    ASSERT_EQ(r.phases.size(), 2u);
    EXPECT_TRUE(r.phases[0].ran);
    EXPECT_TRUE(r.phases[1].ran);

    EXPECT_FALSE(std::isnan(r.stationarity));
    EXPECT_FALSE(std::isnan(r.feasibility_e));
    EXPECT_FALSE(std::isnan(r.feasibility_i));
    EXPECT_FALSE(std::isnan(r.complementarity));
    EXPECT_LT(r.stationarity, 1e-6);
}

TEST(SolveResult, AnActiveRestorationExitEmptiesTheResidualBlocks) {
    // CLAUSE (c). Nested restoration replaces the constraint rows of the
    // right-hand side with the CONDENSED residuals of its own subproblem
    // (eval_nlp's nested arm), so an exit taken while it was active carries
    // rows that are not the declared ones: this fixture's declared equality is
    // x0^2 + x1^2 + 1, which is >= 1 EVERYWHERE, while the copied row is a
    // tiny condensed number that reads as a nearly feasible point. T8.4
    // reported the four diagnostics as NaN here and then copied those rows
    // anyway. Empty is the contract's word for unmeasured; a zero-length
    // vector cannot be misread the way a copied one can.
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    o.common.threads = 1;
    o.max_iters = 200;
    o.restoration_mode = hven::solvers::RestorationModes::l1_nested;
    o.max_feas_rest = 1;
    hven::solvers::InteriorPointSolver solver(o);
    auto model = hven::solvers::NLPSolver(std::make_shared<LocallyInfeasibleProblem>());
    model.transcribe();

    const hven::solvers::IpmResult r = solver.solve(*model.nlp_, two_var_start(1.0, 1.0));
    ASSERT_EQ(r.status, hven::solvers::SolveStatus::kStalled);
    ASSERT_EQ(r.phases.back().stop_reason,
              hven::solvers::IpmStopReason::kRestorationLocallyInfeasible);

    EXPECT_TRUE(std::isnan(r.stationarity));
    EXPECT_TRUE(std::isnan(r.feasibility_e));
    EXPECT_TRUE(std::isnan(r.feasibility_i));
    EXPECT_TRUE(std::isnan(r.complementarity));
    EXPECT_EQ(r.ce.size(), 0) << "a condensed restoration residual is not the declared one";
    EXPECT_EQ(r.ci.size(), 0);
}

TEST(SolveResult, ReturnBestReportsTheBestIterateSObjective) {
    // THE INHERITED MISMATCH, fixed here because IpmResult is T8.4's. The
    // return_best_ substitution replaces XSL and RHS with the BEST iterate's
    // pair, so x, the multipliers and the residuals all describe that point --
    // while `f` was taken from iters.back(), the LAST one. On a solve whose
    // last iterate is not its best, the objective did not belong to the point
    // being returned.
    // ON A FIXTURE WHOSE ITERATES GET WORSE. HS071 descends monotonically, so
    // its last iterate IS its best at every cap and the substitution never
    // fires there. This equality has no real root and the feasibility measure
    // the default criterion (ECONS) scores runs away from its own best early
    // iterate, which is exactly the case return_best exists for.
    LocallyInfeasibleProblem problem;
    auto model = hven::solvers::NLPSolver(std::make_shared<LocallyInfeasibleProblem>());
    model.transcribe();

    // THE CAP IS SEARCHED FOR, NOT ASSUMED. What makes the fixture
    // discriminating is that the substitution actually FIRES -- that the best
    // iterate is not the last one at the cap -- and which caps have that
    // property is a fact about the trajectory, not something a test may assert
    // by construction. The search asserts that at least one exists; the pin
    // below then runs at it.
    hven::solvers::IpmResult best;
    hven::solvers::IpmResult last;
    int chosen_cap = -1;
    for (int cap = 2; cap <= 30 && chosen_cap < 0; ++cap) {
        hven::solvers::IpmOptions probe;
        probe.common.print_level = 10;
        probe.common.threads = 1;
        probe.max_iters = cap;
        hven::solvers::IpmOptions with_best = probe;
        with_best.return_best = true;

        hven::solvers::InteriorPointSolver best_solver(with_best);
        hven::solvers::InteriorPointSolver last_solver(probe);
        const hven::solvers::IpmResult b = best_solver.solve(*model.nlp_, two_var_start(1.0, 1.0));
        const hven::solvers::IpmResult l = last_solver.solve(*model.nlp_, two_var_start(1.0, 1.0));
        if (b.status != hven::solvers::SolveStatus::kOptimal && b.x.allFinite() &&
            (b.x - l.x).lpNorm<Eigen::Infinity>() > 1e-12) {
            best = b;
            last = l;
            chosen_cap = cap;
        }
    }
    ASSERT_GE(chosen_cap, 0)
        << "no cap in [2, 30] made return_best substitute a different point -- "
           "the pin below would not discriminate";
    ::testing::Test::RecordProperty("cap", chosen_cap);

    // The objective REPORTED is the objective OF THE POINT RETURNED, evaluated
    // here from the model itself.
    double f_at_returned = 0.0;
    problem.eval_f(best.x, f_at_returned);
    EXPECT_NEAR(best.f, f_at_returned, 1e-9 * std::max(1.0, std::abs(f_at_returned)));

    // And the unsubstituted solve is unchanged -- the fix touches one branch.
    double f_at_last = 0.0;
    problem.eval_f(last.x, f_at_last);
    EXPECT_NEAR(last.f, f_at_last, 1e-9 * std::max(1.0, std::abs(f_at_last)));
}

TEST(IpmIntrospection, AnalysisIdentityOutlivesTheAnalysingSolver) {
    // THE LIFETIME HOLE T8.4's fourth conjunct left open. That conjunct
    // compared a captured value-array ADDRESS, which answers "did I lay this
    // program's tables" only while no other solver can be handed the same
    // address. Here solver A analyses the program and DIES; solver B is
    // constructed afterwards and may well take A's storage. An address-based
    // token can coincide; a never-reused owner id cannot.
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;

    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();

    {
        hven::solvers::InteriorPointSolver a(o);
        const hven::solvers::IpmResult r = a.solve(*model.nlp_, hven_drivers_tests::hs071_start());
        ASSERT_EQ(r.status, hven::solvers::SolveStatus::kOptimal);
        ASSERT_TRUE(a.kkt_pattern_is_analyzed(*model.nlp_));
    }
    // A is gone. Its matrix is gone with it; the program's retained pointer to
    // it is a dangling one, which is exactly why nothing may take that program
    // as "already analysed" on the strength of an address.

    hven::solvers::InteriorPointSolver b(o);
    EXPECT_FALSE(b.kkt_pattern_is_analyzed(*model.nlp_))
        << "B did not lay this program's tables, whatever address it was given";

    const hven::solvers::IpmResult r2 = b.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    EXPECT_EQ(r2.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(r2.kkt_analyses_this_call, 1) << "B's first solve re-analyses";
    EXPECT_TRUE(b.kkt_pattern_is_analyzed(*model.nlp_));

    // And B's SECOND solve reuses, so the id is an identity token and not a
    // blanket refusal.
    const hven::solvers::IpmResult r3 = b.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    EXPECT_EQ(r3.kkt_analyses_this_call, 0);
}

TEST(SolveBudget, IpmClampsBeforeNarrowing) {
    // 2^32 is a perfectly valid Index budget and a WEAK constraint: it is far
    // above this engine's own limit, so the effective cap is that limit. T8.4
    // narrowed to `int` BEFORE taking the minimum, so 2^32 became 0, the loop
    // was skipped entirely and the exit reached an empty iterate history.
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    hven::solvers::InteriorPointSolver solver(o);
    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();

    const hven::solvers::IpmResult huge =
        solver.solve(*model.nlp_, hven_drivers_tests::hs071_start(),
                     hven::solvers::SolveBudget{/*minor_budget=*/0,
                                                /*max_iterations=*/Index{1} << 32});
    EXPECT_EQ(huge.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_GT(huge.iterations, 0);

    // The same solve with no budget named runs identically -- which is what
    // "a budget above the engine's limit is the identity" means.
    const hven::solvers::IpmResult none =
        solver.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    EXPECT_EQ(none.iterations, huge.iterations);
}

TEST(SolveResult, TheIpmClockRunsFromThePublicEntry) {
    // The shared clock is INFORMATIONAL (CLAUDE.md section 7) and is asserted
    // here only for the boundary it brackets: it starts at the public entry,
    // after the argument check, and stops at the return, so it is positive on
    // any solve that did work and is never smaller than the engine's own older
    // measurement, which stops before the final reporting.
    hven::solvers::IpmOptions o;
    o.common.print_level = 10;
    hven::solvers::InteriorPointSolver solver(o);
    auto model = hven::solvers::NLPSolver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    model.transcribe();

    const hven::solvers::IpmResult r = solver.solve(*model.nlp_, hven_drivers_tests::hs071_start());
    ASSERT_EQ(r.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_GT(r.wall_seconds, 0.0);
    EXPECT_GE(r.wall_seconds, r.total_time);

    // And the argument check is at that entry: a mis-sized start point is
    // refused by name.
    Eigen::VectorXd wrong(3);
    wrong.setOnes();
    EXPECT_THROW(solver.solve(*model.nlp_, wrong), std::invalid_argument);
}

TEST(SolveBudget, TheSqpTakesABudgetOnEveryPublicOverload) {
    // Design section 2.2 puts the budget on EVERY public overload. T8.4 left
    // the cold and bridge entries hardcoding SolveBudget{}, which meant a
    // STAGED warm start could not be budgeted at all: the warm-start overloads
    // refuse to run beside a staged value.
    const auto p = hven::solvers::test_support::make_hs(7);
    hven::solvers::SqpOptions o = quiet_sqp();
    o.max_iter = 200;

    hven::solvers::SqpDriver model_driver(o);
    const hven::solvers::SqpResult capped =
        model_driver.solve(*p.model, p.model->start_point(),
                           hven::solvers::SolveBudget{/*minor_budget=*/0, /*max_iterations=*/2});
    EXPECT_LE(capped.counters.major_iters, 2);
    EXPECT_EQ(capped.status, hven::solvers::SolveStatus::kMaxIter);

    // The bridge-taking form, and a PAYLOAD riding it -- the combination that
    // had no budgeted door at all before T8.4, and which T8.5 turned from a
    // staged value into an argument on this same overload family.
    hven::solvers::SqpDriver bridge_driver(o);
    // The borrow idiom this suite's bench neighbours use: a shared_ptr with an
    // EMPTY owner, so the bridge names a model it does not own.
    const std::shared_ptr<const hven::solvers::NlpModel> borrowed(std::shared_ptr<const void>(),
                                                                  p.model.get());
    hven::solvers::NlpModelAggregate bridge(borrowed);
    const hven::solvers::SqpResult warmup = bridge_driver.solve(bridge, p.model->start_point());
    ASSERT_EQ(warmup.status, hven::solvers::SolveStatus::kOptimal);
    const auto currency = warmup.export_warm_start();
    ASSERT_TRUE(currency.has_value());
    const hven::solvers::SqpResult payload_and_capped = bridge_driver.solve(
        bridge, p.model->start_point(), *currency, hven::solvers::SolveBudget{0, 1});
    EXPECT_LE(payload_and_capped.counters.major_iters, 1);

    // AND THE DEFAULT IS THE IDENTITY on both new doors.
    hven::solvers::SqpDriver free_driver(o);
    const hven::solvers::SqpResult free_run =
        free_driver.solve(*p.model, p.model->start_point(), hven::solvers::SolveBudget{});
    EXPECT_EQ(free_run.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_GT(free_run.counters.major_iters, 2);
    // The shared clock is present on the new overloads too, and is never the
    // smaller of the two measurements.
    EXPECT_GT(free_run.wall_seconds, 0.0);
    EXPECT_GE(free_run.wall_seconds, free_run.solve_impl_seconds);
}
