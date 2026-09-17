// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_start_point_projection.cpp -- M7 W2 M4: the out-of-box start
// point, `SqpOptions::project_start_point`, and `SqpCounters::start_point_clamps`.
//
// WHAT THE FIXTURE REPRODUCES. CUTEst declares three problems (NET3, STEENBRD,
// STEENBRE) whose own `x0` lies outside their own declared box, and whose
// objective is not defined there: hven's SQP lane returns `numerical_error` with
// `f = nan` in 0 majors and 0 factorizations on all three
// (docs/notes/2026-09-m7-w2-sqp-cutest-read.md section M4). `OutOfBoxSqrtModel`
// below is the same shape at n = 2 -- a square-root objective that is NaN below
// its declared lower bound -- so the OFF pin here IS that behaviour, on a model
// a unit test can carry.
//
// THE OFF STATE IS NOT A REFUSAL. At `project_start_point == false` the driver
// takes `x0` VERBATIM: it does not throw, it evaluates where the caller pointed
// and REPORTS `kNumericalError`, which is why every consumer -- a benchmark
// table included -- reads it as "the solver failed".
//
// Names carry a `StartPoint` prefix: this suite's TUs share a link unit.

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/core/solver_counters.h>
#include <hven/drivers/solve_status.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/model/nlp_model.h>

namespace hven::solvers {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

Vec vec2(double a, double b) {
    Vec v(2);
    v << a, b;
    return v;
}

/// @brief f(x) = -sqrt(x0) - sqrt(x1) over a declared box, no rows.
///
/// CONVEX (each term's second derivative is +x^(-3/2)/4 > 0) and strictly
/// decreasing, so the minimizer is the box's upper corner. NaN at any NEGATIVE
/// coordinate, which is the whole point: a start point below a non-negative
/// lower bound cannot be evaluated, exactly as the three CUTEst cells cannot.
class OutOfBoxSqrtModel : public NlpModel {
  public:
    OutOfBoxSqrtModel(Vec lower, Vec upper) : lower_(std::move(lower)), upper_(std::move(upper)) {}

    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return -std::sqrt(x(0)) - std::sqrt(x(1)); }
    Vec eval_grad(const Vec &x) const override {
        return vec2(-0.5 / std::sqrt(x(0)), -0.5 / std::sqrt(x(1)));
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 0.25 * std::pow(x(0), -1.5);
        h.insert(1, 1) = obj_scale * 0.25 * std::pow(x(1), -1.5);
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, 2); }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    /// 1 projected onto the declared box, not the ORIGIN: `NlpModelAssembly`
    /// walks the sparsity patterns at this point (src/model/nlp_model_assembly
    /// .cpp), and this model's Hessian is +inf at a zero coordinate. In the box
    /// by construction either way, so nothing here is a second source of
    /// out-of-box points.
    Vec start_point() const override {
        return vec2(std::min(std::max(1.0, lower_(0)), upper_(0)),
                    std::min(std::max(1.0, lower_(1)), upper_(1)));
    }

  private:
    Vec lower_, upper_;
};

/// The shipped box for most cases below: 1 <= x <= 5, minimizer (5, 5).
OutOfBoxSqrtModel unit_box_model() { return OutOfBoxSqrtModel(vec2(1.0, 1.0), vec2(5.0, 5.0)); }

SqpOptions quiet_options(bool project) {
    SqpOptions o;
    o.common.print_level = 10;
    o.project_start_point = project;
    return o;
}

/// Every counter, through the tables, so a field added without a thought here
/// is still compared.
void expect_same_counters(const SqpCounters &a, const SqpCounters &b, const char *tag) {
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.f, b.f) << tag << " " #f;
    HVEN_SQP_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.ssn.f, b.ssn.f) << tag << " ssn." #f;
    HVEN_SSN_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.ipqp.f, b.ipqp.f) << tag << " ipqp." #f;
    HVEN_IPQP_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
}

} // namespace

// ---------------------------------------------------------------------------
// OFF -- the pin on what this engine does today
// ---------------------------------------------------------------------------

TEST(StartPointProjection, OffTakesAnOutOfBoxStartVerbatimAndExitsNumericalErrorAtIterationZero) {
    // THE THREE CUTEst CELLS' ROW, reproduced: `numerical_error`, `f = nan`,
    // 0 majors, 0 factorizations. It is a RETURNED STATUS and not a throw --
    // that distinction is the whole reason M4 exists.
    OutOfBoxSqrtModel model = unit_box_model();
    SqpSolver driver(quiet_options(/*project=*/false));
    SqpResult out;
    ASSERT_NO_THROW(out = driver.solve(model, vec2(-1.0, -1.0)));

    EXPECT_EQ(out.status, SolveStatus::kNumericalError);
    EXPECT_TRUE(std::isnan(out.f));
    EXPECT_EQ(out.counters.major_iters, 0);
    EXPECT_EQ(out.counters.factorizations, 0);
    // The solve stood where the caller pointed, and says so.
    EXPECT_EQ(out.x, vec2(-1.0, -1.0));
    // Nothing was clamped, because nothing clamps at OFF.
    EXPECT_EQ(out.counters.start_point_clamps, 0);
    // The cold hand-off the exit's own comment promises, so a caller retrying
    // from a corrected x0 is honoured.
    EXPECT_FALSE(out.warm_start.valid);
}

TEST(StartPointProjection, OffIsUnchangedOnAnInBoxStart) {
    OutOfBoxSqrtModel model = unit_box_model();
    SqpSolver driver(quiet_options(/*project=*/false));
    const SqpResult out = driver.solve(model, vec2(2.0, 3.0));

    EXPECT_EQ(out.status, SolveStatus::kOptimal);
    EXPECT_NEAR(out.f, -2.0 * std::sqrt(5.0), 1e-9);
    EXPECT_EQ(out.counters.start_point_clamps, 0);
}

// ---------------------------------------------------------------------------
// ON -- the projection, and the count
// ---------------------------------------------------------------------------

TEST(StartPointProjection, OnSolvesTheOutOfBoxStartAndCountsBothComponents) {
    OutOfBoxSqrtModel model = unit_box_model();
    SqpSolver driver(quiet_options(/*project=*/true));
    const SqpResult out = driver.solve(model, vec2(-1.0, -1.0));

    EXPECT_EQ(out.status, SolveStatus::kOptimal);
    EXPECT_NEAR(out.f, -2.0 * std::sqrt(5.0), 1e-9);
    EXPECT_NEAR(out.x(0), 5.0, 1e-7);
    EXPECT_NEAR(out.x(1), 5.0, 1e-7);
    // EXACT: two components lay strictly outside, both below their lower bound.
    EXPECT_EQ(out.counters.start_point_clamps, 2);
}

TEST(StartPointProjection, OnCountsOnlyTheComponentsStrictlyOutsideTheBox) {
    OutOfBoxSqrtModel model = unit_box_model();
    SqpSolver driver(quiet_options(/*project=*/true));

    // One below its lower bound, one already inside.
    EXPECT_EQ(driver.solve(model, vec2(-1.0, 3.0)).counters.start_point_clamps, 1);
    // One below, one ABOVE: the count is a component count and covers both sides.
    EXPECT_EQ(driver.solve(model, vec2(-1.0, 9.0)).counters.start_point_clamps, 2);
    // ON A BOUND is not OUTSIDE it: the predicate is strict.
    EXPECT_EQ(driver.solve(model, vec2(1.0, 5.0)).counters.start_point_clamps, 0);
}

TEST(StartPointProjection, OnLeavesAnInBoxStartByteIdenticalToOff) {
    // THE CLAUSE-3 PROOF at unit scale: a clamp count of 0 must change nothing.
    // Not "nearly nothing" -- the same status, the same objective bit for bit,
    // the same iterate, and every counter through the tables.
    OutOfBoxSqrtModel model = unit_box_model();
    const Vec x0 = vec2(2.0, 3.0);

    SqpSolver off(quiet_options(/*project=*/false));
    const SqpResult a = off.solve(model, x0);
    SqpSolver on(quiet_options(/*project=*/true));
    const SqpResult b = on.solve(model, x0);

    ASSERT_EQ(a.status, SolveStatus::kOptimal);
    EXPECT_EQ(b.status, a.status);
    EXPECT_EQ(b.f, a.f);
    EXPECT_EQ(b.x, a.x);
    EXPECT_EQ(b.kkt_residual, a.kkt_residual);
    EXPECT_EQ(b.iterations, a.iterations);
    EXPECT_EQ(b.history.size(), a.history.size());
    EXPECT_EQ(b.counters.start_point_clamps, 0);
    expect_same_counters(a.counters, b.counters, "in-box ON vs OFF");
}

TEST(StartPointProjection, OnLandsAFixedVariableExactlyOnItsValueAndLeavesInfiniteBoundsAlone) {
    // FIXED (l == u) and INFINITE bounds, read off the point the solve actually
    // stands on: the iteration callback's `x` at major 0 IS the projected start
    // point, which is the only place a clamp's TARGET is observable.
    //
    // x0 is fixed at 2 and x1 has no lower bound; the start point is off the
    // fixed value and above x1's upper bound, so exactly two components are
    // strictly outside.
    OutOfBoxSqrtModel model(vec2(2.0, -kInf), vec2(2.0, 5.0));
    SqpSolver driver(quiet_options(/*project=*/true));
    Vec first;
    driver.set_iteration_callback([&first](const IterationEvent &e) {
        if (first.size() == 0 && e.depth.value_or(0) == 0) {
            first = e.x;
        }
        return CallbackAction::kContinue;
    });
    const SqpResult out = driver.solve(model, vec2(4.0, 9.0));

    EXPECT_EQ(out.counters.start_point_clamps, 2);
    ASSERT_EQ(first.size(), 2);
    // EXACTLY its value, not near it.
    EXPECT_EQ(first(0), 2.0);
    EXPECT_EQ(first(1), 5.0);
}

TEST(StartPointProjection, OnMovesNothingWhereTheBoundIsInfinite) {
    // -inf below, +inf above: every finite point is inside, so the projection
    // is a no-op however far out the caller starts. The model is evaluated at a
    // positive point, so nothing here is NaN.
    OutOfBoxSqrtModel model(vec2(-kInf, -kInf), vec2(kInf, kInf));
    // The objective is unbounded below on this box, so the solve is capped:
    // this pin is about the START POINT, not about where the iteration goes.
    SqpOptions opts = quiet_options(/*project=*/true);
    opts.max_iter = 2;
    SqpSolver driver(opts);
    Vec first;
    driver.set_iteration_callback([&first](const IterationEvent &e) {
        if (first.size() == 0 && e.depth.value_or(0) == 0) {
            first = e.x;
        }
        return CallbackAction::kContinue;
    });
    const SqpResult out = driver.solve(model, vec2(1e6, 1e6));

    EXPECT_EQ(out.counters.start_point_clamps, 0);
    ASSERT_EQ(first.size(), 2);
    EXPECT_EQ(first(0), 1e6);
    EXPECT_EQ(first(1), 1e6);
}

TEST(StartPointProjection, OnStillRefusesANonFiniteStartPoint) {
    // THE REFUSAL IS UNWEAKENED, and it must be: a NaN has no projection, and
    // clamping one would certify kOptimal in zero majors. Both non-finite
    // spellings, and the size refusal beside them, are still throws.
    OutOfBoxSqrtModel model = unit_box_model();
    SqpSolver driver(quiet_options(/*project=*/true));

    EXPECT_THROW(driver.solve(model, vec2(std::numeric_limits<double>::quiet_NaN(), 3.0)),
                 std::invalid_argument);
    EXPECT_THROW(driver.solve(model, vec2(kInf, 3.0)), std::invalid_argument);
    EXPECT_THROW(driver.solve(model, Vec::Constant(3, 2.0)), std::invalid_argument);
}

TEST(StartPointProjection, TheOptionsValidatorAcceptsBothStatesAndTheDumpCarriesTheField) {
    // A BOOL HAS NO REFUSABLE VALUE, which is the reason validate() carries a
    // stated line rather than a predicate for this field -- pinned here so the
    // claim is checked rather than asserted in a comment.
    SqpOptions o;
    o.project_start_point = false;
    EXPECT_NO_THROW(validate(o));
    o.project_start_point = true;
    EXPECT_NO_THROW(validate(o));

    // The dump's own line, in declaration position; the LINE COUNT and the
    // field count are pinned by tests/drivers/test_options_dump.cpp.
    std::ostringstream os;
    dump_options(o, os);
    EXPECT_NE(os.str().find("project_start_point = true\n"), std::string::npos) << os.str();
}

} // namespace hven::solvers
