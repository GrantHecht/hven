// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/drivers/test_callback.cpp -- ONE PER-ITERATION CALLBACK, BOTH ENGINES
// (M6 W5 T8.6).
//
// `set_iteration_callback(IterationCallback)` / `clear_iteration_callback()`
// exist on the interior-point solver and on the SQP driver, over ONE event type
// in DECLARED space and CALLER units, and `CallbackAction::kStop` is honoured on
// both -- which is what makes `SolveStatus::kInterrupted` reachable at all
// (design section 2.3: "neither reports kInterrupted today" was true until this
// task). That is a statement about the SHARED surface, so it is pinned here
// rather than in either engine's suite.
//
// WHAT EACH ENGINE PROMISES, and what the nine tests below hold it to:
//
//   SQP -- one event per HISTORY ROW, fired at the single site that emits them,
//   in the caller's units even on a scaled solve (design section 2.7 behaviour
//   change: design section 2.5's "multipliers and z have the scaling map
//   applied" -- the SQP's caller-unit rule is not one of section 2.7's numbered
//   items); `depth` counts restoration nesting and a stop taken inside a
//   restoration sub-solve LATCHES on the parent; a stop is honoured before the
//   next subproblem is built, so `history.size() == major_iters + 1` survives
//   it; converged beats stop.
//
//   INTERIOR-POINT -- one event per `ipm.iter` row, fired at the TOP of the
//   iteration that row belongs to, so the event describes the COMMITTED point
//   of the previous step with the diagnostics of THAT point (behaviour change
//   (3); (2) of that list is kInterrupted on both engines -- corrected at fix1);
//   a stop is honoured pre-factorization and ends the PHASE SEQUENCE.
//
//   BOTH -- an exception from the callback propagates out of solve(), the
//   abandoned solve writes no end event and (on the SQP, which is the engine
//   that has a ledger today) no ledger record of its own, and the solver is
//   reusable.

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include <hven/core/ledger.h>
#include <hven/detail/model/nlp_adapter.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/drivers/trace_writer.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_model_assembly.h>
#include <hven/model/nlp_problem_model.h>
#include <hven/model/nlp_triplet_model.h>

#include "support/hs071_problem.h"

namespace {

using hven::Index;
using hven::solvers::CallbackAction;
using hven::solvers::IterationEvent;
using hven::solvers::Ledger;
using hven::solvers::NlpModel;
using hven::solvers::NlpProblemModel;
using hven::solvers::SolveStatus;
using hven::solvers::SqpOptions;
using hven::solvers::SqpResult;
using hven::solvers::SqpSolver;
using Vec = Eigen::VectorXd;
using SpMatRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;

constexpr double kInfBound = std::numeric_limits<double>::infinity();

// ---------------------------------------------------------------------------
// THE RECORDER. An IterationEvent's four vector views are borrowed and valid
// for the call only, so an oracle that wants them afterwards COPIES them --
// which is exactly what this does, and what a caller would have to do.
// ---------------------------------------------------------------------------
struct Seen {
    Index iteration = 0;
    std::optional<Index> phase;
    std::optional<Index> depth;
    double f = 0.0;
    double stationarity = 0.0;
    double feasibility_e = 0.0;
    double feasibility_i = 0.0;
    double complementarity = 0.0;
    double step_norm = 0.0;
    std::optional<double> radius;
    std::optional<double> mu;
    Vec x, lambda_e, lambda_i, z;
    double elapsed_seconds = 0.0;
};

Seen copy_of(const IterationEvent &e) {
    Seen s;
    s.iteration = e.iteration;
    s.phase = e.phase;
    s.depth = e.depth;
    s.f = e.f;
    s.stationarity = e.stationarity;
    s.feasibility_e = e.feasibility_e;
    s.feasibility_i = e.feasibility_i;
    s.complementarity = e.complementarity;
    s.step_norm = e.step_norm;
    s.radius = e.radius;
    s.mu = e.mu;
    s.x = e.x;
    s.lambda_e = e.lambda_e;
    s.lambda_i = e.lambda_i;
    s.z = e.z;
    s.elapsed_seconds = e.elapsed_seconds;
    return s;
}

/// @brief Records every event and never stops.
struct Recorder {
    std::vector<Seen> seen;
    hven::solvers::IterationCallback hook() {
        return [this](const IterationEvent &e) {
            this->seen.push_back(copy_of(e));
            return CallbackAction::kContinue;
        };
    }
};

Vec hs071_start() {
    Vec x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

SqpOptions quiet_sqp() {
    SqpOptions o;
    o.common.print_level = 10;
    return o;
}

hven::solvers::IpmOptions quiet_ipm(const hven::solvers::IpmSolver &solver) {
    hven::solvers::IpmOptions o = solver.options();
    o.common.print_level = 10;
    return o;
}

// ---------------------------------------------------------------------------
// FIXTURE: THE FUNNEL-STALLING VALLEY, which is the tree's documented route
// into the restoration phase and back out of it.
//
//     min  -10 x1 + 5 x1^2 - 0.01 x0 + 0.0005 x0^2
//     s.t. cE(x) = x1 + 1000 x0^2 = 0,      x_start = (0, 0.5)
//
// VERBATIM from tests/sqp/test_sqp_restoration.cpp's StalledValleyModel, whose
// own comment carries the hand-derived stall schedule, the restoration entry
// and the optimum. It is copied rather than shared because that model is a
// local of a .cpp in the SQP suite; nothing about it is edited here, and the
// only property this file uses is that a solve of it ENTERS RESTORATION and
// then RESUMES -- which is what gives depth-1 events to stop at.
// ---------------------------------------------------------------------------
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
        g << -0.01 + 0.001 * x(0), -10.0 + 10.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(1) + 1000.0 * x(0) * x(0);
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_factor, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_factor * 0.001 + 2000.0 * lambda_e(0);
        h.insert(1, 1) = obj_factor * 10.0;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 2000.0 * x(0);
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
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

// ---------------------------------------------------------------------------
// FIXTURE: AN INFEASIBLE NLP, whose restoration phase runs to a certificate
// rather than resuming.
//
//     min  x0 + x1   s.t.  x0^2 + x1^2 = 1,  x0 + x1 = 2      x_start = (2, 2)
//
// VERBATIM from tests/sqp/test_sqp_restoration.cpp's InfeasibleCircleLineModel
// (see that file for the derivation of the elastic-then-restoration
// trajectory), copied for the same reason StalledValleyModel is. What this file
// uses it for is the OTHER restoration route: the sub-solve here exits at a
// still-infeasible point, so a stop taken inside it arrives at the parent as
// `rs.status == kInterrupted` through enter_restoration's status switch --
// where the resuming fixture above exercises only the forwarder's latch.
// ---------------------------------------------------------------------------
class InfeasibleCircleLineModel : public NlpModel {
  public:
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
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(2, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.insert(1, 0) = 1.0;
        j.insert(1, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
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
        x << 2.0, 2.0;
        return x;
    }
};

// ---------------------------------------------------------------------------
// FIXTURE: A MODEL THE CALLER'S OWN START POINT CANNOT BE EVALUATED AT.
//
// x0 itself is finite -- the driver validates that -- but f, its gradient and
// the constraint row are all NaN there, so the driver's very first KKT
// measurement is non-finite and it takes the ZERO-MAJOR exit: one history row,
// no subproblem, kNumericalError.
// ---------------------------------------------------------------------------
class NonFiniteStartModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    static double nan() { return std::numeric_limits<double>::quiet_NaN(); }

    double eval_f(const Vec &) const override { return nan(); }
    Vec eval_grad(const Vec &) const override { return Vec::Constant(2, nan()); }
    Vec eval_ce(const Vec &) const override { return Vec::Constant(1, nan()); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 1.0;
        h.insert(1, 1) = 1.0;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = nan();
        j.insert(0, 1) = nan();
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 1.0); }
};

// An SQP-side view of HS071, kept alive with its bridge.
struct Hs071View {
    std::shared_ptr<hven::solvers::NlpTripletModel> problem;
    std::shared_ptr<NlpProblemModel> model;
    std::shared_ptr<hven::solvers::NlpModelAssembly> bridge;

    Hs071View()
        : problem(std::make_shared<hven_drivers_tests::Hs071Problem>()),
          model(std::make_shared<NlpProblemModel>(problem)),
          bridge(std::make_shared<hven::solvers::NlpModelAssembly>(model)) {}
};

// A FIXED-VARIABLE PROBLEM, for the MakeConstraint arm of the interior-point
// twin pin (M6 W5 T8.6 fix1, the SQP lane's M3): under that treatment the fixed
// coordinate is NOT eliminated, it becomes an internal fixing ROW, and the
// declared bound price there is -lambda_fix. That fold, and the `excluded` set
// the reduced gradient needs, are the two pieces of the declared-space seam a
// plain HS071 solve never exercises.
//
//   min (x0-1)^2 + (x1-1)^2,   x1 fixed at 3 by equal bounds.
//
// Shaped after tests/interior/test_ipm_solver_entry.cpp's FixedVarProblem, which the
// treatment pins already use; copied rather than shared because that fixture is
// a local of a .cpp in another suite.
struct FixedVarProblem final : hven::solvers::NlpTripletModel {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 0; }
    int num_jac_nonzeros() const override { return 0; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Vec> xl, Eigen::Ref<Vec> xu, Eigen::Ref<Vec>,
                Eigen::Ref<Vec>) const override {
        xl << -kInfBound, 3.0;
        xu << kInfBound, 3.0;
    }
    void eval_f(hven::ConstEigenRef<Vec> x, double &f) const override {
        const double a = x[0] - 1.0, b = x[1] - 1.0;
        f = a * a + b * b;
    }
    void eval_grad_f(hven::ConstEigenRef<Vec> x, Eigen::Ref<Vec> g) const override {
        g[0] = 2.0 * (x[0] - 1.0);
        g[1] = 2.0 * (x[1] - 1.0);
    }
    void eval_g(hven::ConstEigenRef<Vec>, Eigen::Ref<Vec>) const override {}
    void jac_structure(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>) const override {}
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(hven::ConstEigenRef<Vec>, Eigen::Ref<Vec>) const override {}
    void eval_hess(hven::ConstEigenRef<Vec>, double obj_factor, hven::ConstEigenRef<Vec>,
                   Eigen::Ref<Vec> v) const override {
        v[0] = 2.0 * obj_factor;
        v[1] = 2.0 * obj_factor;
    }
    std::string name() const override { return "FixedVarProblem"; }
};

bool bit_equal(const Vec &a, const Vec &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (Eigen::Index i = 0; i < a.size(); ++i) {
        if (std::bit_cast<std::uint64_t>(a[i]) != std::bit_cast<std::uint64_t>(b[i])) {
            return false;
        }
    }
    return true;
}

// A PROBLEM WHOSE OBJECTIVE RISES along the solve: min 0.5*|x|^2 subject to
// sum(x) == 3 on a [-2, 2] box, started at the origin. f(x0) = 0 and
// f(x*) = 1.125, so under BestCriteriaModes::kObj the best-scoring iterate is an
// EARLY one -- which is what a `return_best` substitution needs in order to
// substitute anything at all.
//
// VERBATIM from tests/interior/test_ipm_solver_entry.cpp's
// BestIterateRisingObjectiveProblem, copied for the reason the two SQP fixtures
// above are: it is a local of a .cpp in another suite.
struct RisingObjectiveProblem final : hven::solvers::NlpTripletModel {
    static constexpr int kN = 4;

    int num_vars() const override { return kN; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return kN; }
    int num_hess_nonzeros() const override { return kN; }

    void bounds(Eigen::Ref<Vec> xl, Eigen::Ref<Vec> xu, Eigen::Ref<Vec> gl,
                Eigen::Ref<Vec> gu) const override {
        xl.setConstant(-2.0);
        xu.setConstant(2.0);
        gl.setConstant(3.0);
        gu.setConstant(3.0);
    }
    void eval_f(hven::ConstEigenRef<Vec> x, double &f) const override { f = 0.5 * x.squaredNorm(); }
    void eval_grad_f(hven::ConstEigenRef<Vec> x, Eigen::Ref<Vec> g) const override { g = x; }
    void eval_g(hven::ConstEigenRef<Vec> x, Eigen::Ref<Vec> g) const override { g[0] = x.sum(); }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int i = 0; i < kN; i++) {
            r[i] = 0;
            c[i] = i;
        }
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int i = 0; i < kN; i++) {
            r[i] = i;
            c[i] = i;
        }
    }
    void eval_jac(hven::ConstEigenRef<Vec>, Eigen::Ref<Vec> v) const override {
        v.setConstant(1.0);
    }
    void eval_hess(hven::ConstEigenRef<Vec>, double obj_factor, hven::ConstEigenRef<Vec>,
                   Eigen::Ref<Vec> v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "RisingObjectiveProblem"; }
};

// Bitwise equality that treats NaN as a VALUE: "unmeasured" is one of the
// answers the four diagnostics give, and two unmeasured answers agree.
bool same_scalar(double a, double b) {
    if (std::isnan(a) && std::isnan(b)) {
        return true;
    }
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

} // namespace

// ===========================================================================
// (1) THE SQP'S EVENTS ARE ITS HISTORY ROWS, IN THE CALLER'S UNITS
// ===========================================================================

// enable_scaling ON, deliberately: an event built from the engine's own vectors
// without the engine->caller map would agree with the history on an UNSCALED
// solve and disagree on this one, so only this arm proves the caller-unit rule.
// The test asserts the scaling really engaged before it reads anything.
TEST(Callback, SqpEventRowsEqualHistoryInCallerUnits) {
    Hs071View view;
    SqpOptions opts = quiet_sqp();
    opts.enable_scaling = true;

    SqpSolver driver(opts);
    Recorder rec;
    driver.set_iteration_callback(rec.hook());
    const SqpResult sol = driver.solve(*view.bridge, hs071_start());

    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    ASSERT_TRUE(sol.scaling.active) << "fixture premise: the scaling must engage, or the "
                                       "engine->caller map below is the identity and this "
                                       "test proves nothing about the caller-unit rule";
    ASSERT_NE(sol.scaling.obj, 1.0) << "fixture premise: a non-unit objective factor";

    // ONE EVENT PER ROW, IN ORDER.
    ASSERT_EQ(rec.seen.size(), sol.history.size());
    ASSERT_GT(rec.seen.size(), 1u) << "non-vacuous: the solve really iterates";
    for (std::size_t k = 0; k < rec.seen.size(); ++k) {
        SCOPED_TRACE("row " + std::to_string(k));
        const Seen &e = rec.seen[k];
        EXPECT_EQ(e.iteration, static_cast<Index>(k));
        EXPECT_FALSE(e.phase.has_value()) << "phase is the interior-point engine's field";
        ASSERT_TRUE(e.depth.has_value());
        EXPECT_EQ(*e.depth, 0) << "a top-level major is nesting depth 0";
        // The row's own caller-unit columns, bit for bit: the event is built
        // from the SAME `exported` row the push and the trace line carry.
        EXPECT_EQ(std::bit_cast<std::uint64_t>(e.f),
                  std::bit_cast<std::uint64_t>(sol.history[k].f));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(e.step_norm),
                  std::bit_cast<std::uint64_t>(sol.history[k].step_norm));
        ASSERT_TRUE(e.radius.has_value());
        EXPECT_EQ(std::bit_cast<std::uint64_t>(*e.radius),
                  std::bit_cast<std::uint64_t>(sol.history[k].tr_radius));
        EXPECT_FALSE(e.mu.has_value()) << "mu is the interior-point engine's field";
        EXPECT_EQ(e.x.size(), sol.x.size());
        // EVERY EVENT'S PRICES, ITS BOUND BLOCK AND ITS FOUR DIAGNOSTICS, not
        // only the terminal event's (M6 W5 T8.6 fix1, astra item 6). The
        // comparison is INDEPENDENT: the CALLER's own model is evaluated at the
        // event's own `x`, and the four shared diagnostics are recomputed from
        // that evaluation and the event's own three price blocks through the
        // public compute_declared_diagnostics -- the same definition, from the
        // outside. A block still carrying a factor of the engine's would fail
        // it, which is the whole of the caller-unit rule checked per row rather
        // than per solve. The tolerance is the round-off of a scaled solve's
        // own arithmetic (the engine divides mapped quantities where the
        // recomputation multiplies raw ones); nothing here is bitwise, and the
        // UNSCALED control below is what pins the bitwise claim.
        ASSERT_EQ(e.lambda_e.size(), sol.lambda_e.size());
        ASSERT_EQ(e.lambda_i.size(), sol.lambda_i.size());
        ASSERT_EQ(e.z.size(), sol.z.size());
        ASSERT_TRUE(e.x.allFinite());
        ASSERT_TRUE(e.lambda_e.allFinite());
        ASSERT_TRUE(e.lambda_i.allFinite());
        ASSERT_TRUE(e.z.allFinite());
        {
            const NlpModel &m = *view.model;
            const hven::solvers::DeclaredDiagnostics d =
                hven::solvers::compute_declared_diagnostics(
                    e.x, e.lambda_e, e.lambda_i, e.z, m.eval_grad(e.x), m.eval_jac_e(e.x),
                    m.eval_jac_i(e.x), m.eval_ce(e.x), m.eval_ci(e.x), m.lower(), m.upper(), {});
            EXPECT_NEAR(e.f, m.eval_f(e.x), 1e-9 * (1.0 + std::abs(e.f)))
                << "the event's objective is the caller's objective at the event's point";
            EXPECT_NEAR(e.stationarity, d.stationarity, 1e-7 * (1.0 + d.stationarity));
            EXPECT_NEAR(e.feasibility_e, d.feasibility_e, 1e-9 * (1.0 + d.feasibility_e));
            EXPECT_NEAR(e.feasibility_i, d.feasibility_i, 1e-9 * (1.0 + d.feasibility_i));
            EXPECT_NEAR(e.complementarity, d.complementarity, 1e-7 * (1.0 + d.complementarity));
        }
    }

    // AND THE VECTORS ARE THE CALLER'S TOO. The terminal row is pushed at the
    // exit conjunction, from the very (x, lambda) `finish` then exports, so the
    // point matches bit for bit and the prices match to the sign sweep and the
    // re-measurement `finish` takes on a scaled solve.
    const Seen &last = rec.seen.back();
    EXPECT_TRUE(bit_equal(last.x, sol.x)) << "the terminal event's point IS the returned point";
    ASSERT_EQ(last.lambda_e.size(), sol.lambda_e.size());
    for (Eigen::Index i = 0; i < last.lambda_e.size(); ++i) {
        EXPECT_NEAR(last.lambda_e[i], sol.lambda_e[i], 1e-9 * (1.0 + std::abs(sol.lambda_e[i])));
    }
    ASSERT_EQ(last.lambda_i.size(), sol.lambda_i.size());
    for (Eigen::Index i = 0; i < last.lambda_i.size(); ++i) {
        EXPECT_NEAR(last.lambda_i[i], sol.lambda_i[i], 1e-9 * (1.0 + std::abs(sol.lambda_i[i])));
    }

    // FALSIFIABILITY: the engine-unit multipliers this solve ran at are NOT the
    // caller-unit ones, so a map that had been skipped would be caught. The
    // objective factor is what separates them.
    EXPECT_NE(sol.scaling.obj, 1.0);
}

// THE UNSCALED CONTROL, and the stronger half of the same claim: with no map to
// apply, the terminal event's four SHARED diagnostics are the very numbers the
// result reports -- one arithmetic, computed once, not two that happen to agree.
TEST(Callback, SqpTerminalEventDiagnosticsAreTheResultsOwn) {
    Hs071View view;
    SqpSolver driver(quiet_sqp());
    Recorder rec;
    driver.set_iteration_callback(rec.hook());
    const SqpResult sol = driver.solve(*view.bridge, hs071_start());

    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    ASSERT_FALSE(sol.scaling.active) << "this arm is the UNSCALED control";
    ASSERT_FALSE(rec.seen.empty());
    const Seen &last = rec.seen.back();
    EXPECT_EQ(std::bit_cast<std::uint64_t>(last.stationarity),
              std::bit_cast<std::uint64_t>(sol.stationarity));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(last.feasibility_e),
              std::bit_cast<std::uint64_t>(sol.feasibility_e));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(last.feasibility_i),
              std::bit_cast<std::uint64_t>(sol.feasibility_i));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(last.complementarity),
              std::bit_cast<std::uint64_t>(sol.complementarity));
}

// ===========================================================================
// (2) kStop ON THE SQP
// ===========================================================================

TEST(Callback, SqpStopReturnsInterruptedAtTheCurrentPoint) {
    // THE STOP INDEX. The callback stops on the event of history row 1; the
    // latch is then read at the NEXT major's exit conjunction, which sits above
    // build_subproblem -- so that major pushes its own row and solves nothing.
    constexpr Index kStopAtRow = 1;

    for (const bool budget_mode : {false, true}) {
        SCOPED_TRACE(budget_mode ? "budget_mode" : "ordinary");
        Hs071View view;
        SqpOptions opts = quiet_sqp();
        opts.budget_mode = budget_mode;

        SqpSolver driver(opts);
        std::vector<Seen> seen;
        driver.set_iteration_callback([&](const IterationEvent &e) {
            seen.push_back(copy_of(e));
            return e.iteration == kStopAtRow ? CallbackAction::kStop : CallbackAction::kContinue;
        });
        const SqpResult sol = driver.solve(*view.bridge, hs071_start());

        // THE VERDICT. Under budget_mode too: an interrupt takes the ORDINARY
        // exit at the current point, never the budget-best substitution --
        // handing back a different iterate than the one the caller stopped on
        // would make the event a lie.
        EXPECT_EQ(sol.status, SolveStatus::kInterrupted);
        EXPECT_NE(sol.status, SolveStatus::kBudgetExhausted);

        // THE SHAPE. One more row than majors, exactly as a capped solve: the
        // interrupted major pushed its row and built no subproblem.
        EXPECT_EQ(sol.history.size(), static_cast<std::size_t>(kStopAtRow) + 2u);
        EXPECT_EQ(sol.counters.major_iters, kStopAtRow + 1);
        EXPECT_EQ(static_cast<Index>(sol.history.size()), sol.counters.major_iters + 1);

        // ONE EVENT PER ROW STILL, the stop's own row included, and the LAST
        // event describes the point that is returned.
        ASSERT_EQ(seen.size(), sol.history.size());
        EXPECT_TRUE(bit_equal(seen.back().x, sol.x));

        // NON-VACUOUS: an uninterrupted solve of this problem takes strictly
        // more majors, so the stop really stopped something.
        SqpSolver full(opts);
        const SqpResult ref = full.solve(*view.bridge, hs071_start());
        EXPECT_EQ(ref.status, SolveStatus::kOptimal);
        EXPECT_GT(ref.counters.major_iters, sol.counters.major_iters);
    }
}

TEST(Callback, SqpConvergedBeatsStop) {
    Hs071View view;
    const SqpOptions opts = quiet_sqp();

    // The converged solve's terminal row index, taken first so the stop below
    // can be aimed at exactly that row and at no other.
    SqpSolver pilot(opts);
    const SqpResult reference = pilot.solve(*view.bridge, hs071_start());
    ASSERT_EQ(reference.status, SolveStatus::kOptimal);
    ASSERT_GT(reference.history.size(), 1u);
    const Index terminal_row = static_cast<Index>(reference.history.size()) - 1;

    SqpSolver driver(opts);
    driver.set_iteration_callback([&](const IterationEvent &e) {
        return e.iteration == terminal_row ? CallbackAction::kStop : CallbackAction::kContinue;
    });
    const SqpResult sol = driver.solve(*view.bridge, hs071_start());

    // CONVERGED BEATS STOP: the row the callback stopped on is the row the
    // convergence test had already passed, and the verdict was settled before
    // the callback ever saw it. The stop is a no-op there.
    EXPECT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_EQ(sol.history.size(), reference.history.size());
    EXPECT_EQ(sol.counters.major_iters, reference.counters.major_iters);
    EXPECT_TRUE(bit_equal(sol.x, reference.x))
        << "a stop that cannot demote the verdict must not move the answer either";
}

TEST(Callback, SqpStopInsideRestorationLatchesAndReachesTheParent) {
    StalledValleyModel model;
    SqpOptions opts = quiet_sqp();
    opts.tr_init = 8.0; // the fixture's own radius schedule
    opts.max_iter = 60;

    // The reference solve: restoration runs, the solve resumes and converges.
    SqpSolver reference_driver(opts);
    const SqpResult reference = reference_driver.solve(model);
    ASSERT_EQ(reference.status, SolveStatus::kOptimal);
    ASSERT_GE(reference.counters.restoration_iters, 1)
        << "fixture premise: the solve must actually enter restoration";

    SqpSolver driver(opts);
    std::vector<Seen> seen;
    bool stopped = false;
    std::size_t stop_index = 0;
    driver.set_iteration_callback([&](const IterationEvent &e) {
        seen.push_back(copy_of(e));
        if (!stopped && e.depth.value_or(0) == 1) {
            stopped = true;
            stop_index = seen.size() - 1;
            return CallbackAction::kStop;
        }
        return CallbackAction::kContinue;
    });
    const SqpResult sol = driver.solve(model);

    ASSERT_TRUE(stopped) << "no depth-1 event ever fired, so nothing inside restoration was seen";

    // (a) THE NESTING IS REPORTED. Depth-0 events came first -- the majors that
    //     stalled the funnel -- and the sub-solve's own rows arrive at depth 1
    //     through the forwarder the parent installs.
    EXPECT_EQ(seen[stop_index].depth.value_or(-1), 1);
    ASSERT_GT(stop_index, 0u);
    EXPECT_EQ(seen[0].depth.value_or(-1), 0);

    // (b) THE LATCH REACHES THE PARENT: the outer solve reports kInterrupted,
    //     not the kOptimal the same fixture reaches when nothing stops it.
    EXPECT_EQ(sol.status, SolveStatus::kInterrupted);

    // (c) THE SUB-SOLVE ITSELF IS UNMOVED HERE, and that is the point of using
    //     the RESUMING fixture for this arm: its restoration phase converges on
    //     the very next row, so converged beats stop INSIDE the sub-solve too
    //     and the phase returns its own kOptimal at a feasible point. The stop
    //     reaches the parent through the FORWARDER's latch and through nothing
    //     else -- route (b). Route (a), where the sub-solve's own kInterrupted
    //     arrives at enter_restoration's status switch, is the circle fixture
    //     below.
    EXPECT_EQ(sol.counters.restoration_iters, reference.counters.restoration_iters);

    // (d) AND NO FURTHER TOP-LEVEL MAJOR: the parent's own subproblem count
    //     stops where the restoration request left it.
    EXPECT_LT(sol.counters.major_iters, reference.counters.major_iters);
    EXPECT_EQ(static_cast<Index>(sol.history.size()), sol.counters.major_iters + 1);
}

TEST(Callback, SqpStopInsideRestorationArrivesAsTheSubSolvesOwnVerdict) {
    InfeasibleCircleLineModel model;
    const SqpOptions opts = quiet_sqp();

    // The reference: restoration runs to a CERTIFICATE of local infeasibility.
    SqpSolver reference_driver(opts);
    const SqpResult reference = reference_driver.solve(model);
    ASSERT_EQ(reference.status, SolveStatus::kInfeasible);
    ASSERT_TRUE(reference.infeasibility_certified)
        << "fixture premise: the restoration phase must reach the certificate, or the pin "
           "below cannot show a stop withholding it";
    ASSERT_GE(reference.counters.restoration_iters, 2);

    SqpSolver driver(opts);
    bool stopped = false;
    driver.set_iteration_callback([&](const IterationEvent &e) {
        if (!stopped && e.depth.value_or(0) == 1) {
            stopped = true;
            return CallbackAction::kStop;
        }
        return CallbackAction::kContinue;
    });
    const SqpResult sol = driver.solve(model);
    ASSERT_TRUE(stopped);

    // THE SUB-SOLVE'S OWN VERDICT REACHES THE PARENT. Its point is still
    // infeasible, so the phase takes the "not feasible enough" exit and hands
    // back kInterrupted -- the case enter_restoration's status switch gained in
    // M6 W5 T8.6. Without that case the switch (which has no `default`) would
    // fall through and the parent would report whatever the previous
    // restoration left in st.resto.status.
    EXPECT_EQ(sol.status, SolveStatus::kInterrupted);
    EXPECT_LT(sol.counters.restoration_iters, reference.counters.restoration_iters);

    // AND NO CERTIFICATE IS CLAIMED. Only a restoration phase that ran to its
    // OWN kOptimal certifies; a phase the caller stopped has proved nothing
    // about the model, and must not be allowed to say otherwise.
    EXPECT_FALSE(sol.infeasibility_certified);
    EXPECT_TRUE(reference.infeasibility_certified);
}

// ===========================================================================
// (2b) THE EVENT'S POINT IS THE ROW'S OWN POINT, ACROSS A RESTORATION
// ===========================================================================

// M6 W5 T8.6 fix1 -- the SQP lane's I1, measured on both restoration routes.
//
// Four of the ten push sites sit AFTER an `enter_restoration` call, and that
// call REPLACES the driver's point before it returns (`st.x = x_r` on both
// routes, and `st.ev` too on the resumed one). An event that read `st.x`,
// `st.ev.ce` and `st.ev.ci` LIVE at the push therefore reported the RESTORED
// point's coordinates and constraint residuals beside the stalled row's `f`,
// `step_norm`, `radius`, `stationarity` and prices -- one event describing two
// points, and exactly on the row a caller most wants to understand.
//
// THE FALSIFIER IS THE MODEL ITSELF. On an UNSCALED solve the row's `f` IS
// `eval_f` at the point the row was measured at, and its `feasibility_e` IS
// the inf-norm of `eval_ce` there, so an event whose `x` belongs to a different
// point disagrees with its own numbers. Both fixtures are checked on every
// depth-0 event: the RESUMING one (the restoration returns kResumed and the
// solve carries on) and the EXITING one (the restoration returns at a
// still-infeasible point). Before the fix each failed on exactly one row --
// its restoration-requesting row -- and passed on every other.
template <class Model>
void expect_every_depth0_event_is_its_own_point(Model &model, const SqpOptions &opts) {
    SqpSolver driver(opts);
    std::vector<Seen> seen;
    driver.set_iteration_callback([&](const IterationEvent &e) {
        seen.push_back(copy_of(e));
        return CallbackAction::kContinue;
    });
    const SqpResult sol = driver.solve(model);
    ASSERT_FALSE(sol.scaling.active) << "this pin is stated on an UNSCALED solve, where the "
                                        "row's f IS eval_f at the row's point";

    std::size_t depth0 = 0;
    std::size_t depth1 = 0;
    for (std::size_t k = 0; k < seen.size(); ++k) {
        const Seen &e = seen[k];
        if (e.depth.value_or(0) != 0) {
            ++depth1;
            continue;
        }
        ++depth0;
        SCOPED_TRACE("depth-0 event " + std::to_string(k) + " (iteration " +
                     std::to_string(e.iteration) + ")");
        if (std::isnan(e.f)) {
            continue; // the non-finite-start row measures nothing
        }
        EXPECT_EQ(std::bit_cast<std::uint64_t>(e.f),
                  std::bit_cast<std::uint64_t>(model.eval_f(e.x)))
            << "the event's objective is not the caller's objective at the event's point";
        const Vec ce = model.eval_ce(e.x);
        if (ce.size() > 0 && !std::isnan(e.feasibility_e)) {
            EXPECT_EQ(std::bit_cast<std::uint64_t>(e.feasibility_e),
                      std::bit_cast<std::uint64_t>(ce.lpNorm<Eigen::Infinity>()))
                << "the event's equality residual is not the residual at the event's point";
        }
    }
    EXPECT_EQ(depth0, sol.history.size()) << "one depth-0 event per history row";
    EXPECT_GT(depth1, 0u) << "fixture premise: the solve must actually enter restoration, or "
                             "the post-restoration push sites are never reached";
}

TEST(Callback, SqpEventXIsTheRowsOwnPointAcrossRestoration) {
    {
        SCOPED_TRACE("the RESUMING route");
        StalledValleyModel model;
        SqpOptions opts = quiet_sqp();
        opts.tr_init = 8.0;
        opts.max_iter = 60;
        expect_every_depth0_event_is_its_own_point(model, opts);
    }
    {
        SCOPED_TRACE("the EXITING route");
        InfeasibleCircleLineModel model;
        expect_every_depth0_event_is_its_own_point(model, quiet_sqp());
    }
}

// AND NOTHING IS SNAPSHOTTED WITHOUT A CALLBACK. The snapshot above is taken
// under `if (iteration_callback_)`, and `push_history` refuses -- by throwing
// std::logic_error, beside its two other identity guards -- to push a row whose
// snapshot vectors are non-empty on a solve with no callback installed. So the
// claim "a solve without a callback copies nothing" is checked on EVERY
// no-callback push in the whole suite; this test is its named exerciser, on the
// fixture that reaches all four post-restoration push sites.
TEST(Callback, ASolveWithNoCallbackSnapshotsNothing) {
    StalledValleyModel model;
    SqpOptions opts = quiet_sqp();
    opts.tr_init = 8.0;
    opts.max_iter = 60;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(model);
    EXPECT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_GE(sol.counters.restoration_iters, 1);
}

// R1's SIBLING (M6 W5 T8.6 fix1). `SqpConvergedBeatsStop` pins the top of the
// precedence order; this pins the bottom of it. A kStop returned on the PUBLIC
// call's terminal row is a NO-OP whatever that row's verdict is, because the
// exit and the verdict were both decided before the callback was shown the row:
// the latch is read as it stood BEFORE this row's own event fired. On a
// NON-CONVERGED cap terminal row that means the solve still reports kMaxIter,
// not kInterrupted. Declared behaviour -- design section 2.5's "read only on a
// CONTINUING dispatch" -- and not an omission.
TEST(Callback, SqpStopOnANonConvergedCapTerminalRowIsANoOp) {
    Hs071View view;
    SqpOptions opts = quiet_sqp();
    opts.max_iter = 3; // well short of this fixture's convergence
    SqpSolver driver(opts);
    std::vector<Seen> seen;
    driver.set_iteration_callback([&](const IterationEvent &e) {
        seen.push_back(copy_of(e));
        // Stop on the LAST row this capped solve will push -- the one the cap
        // itself makes terminal.
        return e.iteration == opts.max_iter ? CallbackAction::kStop : CallbackAction::kContinue;
    });
    const SqpResult sol = driver.solve(*view.bridge, hs071_start());

    ASSERT_EQ(static_cast<Index>(seen.size()), static_cast<Index>(opts.max_iter) + 1)
        << "fixture premise: the cap really is what ends this solve, and the stop was aimed "
           "at its terminal row";
    EXPECT_EQ(sol.status, SolveStatus::kMaxIter);
    EXPECT_NE(sol.status, SolveStatus::kInterrupted);
    EXPECT_EQ(sol.counters.major_iters, opts.max_iter);

    // NON-VACUOUS: the same stop aimed one row EARLIER does interrupt, so the
    // no-op above is about WHICH row, not about the stop being ignored.
    SqpSolver earlier(opts);
    earlier.set_iteration_callback([&](const IterationEvent &e) {
        return e.iteration == opts.max_iter - 1 ? CallbackAction::kStop : CallbackAction::kContinue;
    });
    const SqpResult interrupted = earlier.solve(*view.bridge, hs071_start());
    EXPECT_EQ(interrupted.status, SolveStatus::kInterrupted);
}

// RULING R4 (M6 W5 T8.6 fix1), the arm astra's review named: a stop returned on
// the restoration sub-solve's own CONVERGED terminal row. The sub-solve's
// verdict stays kOptimal -- converged beats stop inside it too -- so the
// certificate is EARNED and kept, while the PARENT, whose latch the forwarder
// set, exits kInterrupted at the point it holds now that restoration has
// returned. Two facts about two solves, and neither is allowed to overwrite the
// other: `infeasibility_certified` is a fact about the SUB-SOLVE, orthogonal to
// the parent's status.
TEST(Callback, SqpStopOnTheRestorationsConvergedRowInterruptsAndKeepsTheCertificate) {
    InfeasibleCircleLineModel model;
    const SqpOptions opts = quiet_sqp();

    // The reference, so the sub-solve's own terminal depth-1 row is known.
    SqpSolver reference_driver(opts);
    std::vector<Seen> ref_seen;
    reference_driver.set_iteration_callback([&](const IterationEvent &e) {
        ref_seen.push_back(copy_of(e));
        return CallbackAction::kContinue;
    });
    const SqpResult reference = reference_driver.solve(model);
    ASSERT_EQ(reference.status, SolveStatus::kInfeasible);
    ASSERT_TRUE(reference.infeasibility_certified)
        << "fixture premise: the restoration phase reaches its own kOptimal certificate";

    // The LAST depth-1 event of the reference run is the sub-solve's converged
    // terminal row; aim the stop at exactly that one.
    Index last_depth1 = -1;
    for (const Seen &e : ref_seen) {
        if (e.depth.value_or(0) == 1) {
            last_depth1 = e.iteration;
        }
    }
    ASSERT_GE(last_depth1, 0);

    SqpSolver driver(opts);
    bool stopped = false;
    driver.set_iteration_callback([&](const IterationEvent &e) {
        if (e.depth.value_or(0) == 1 && e.iteration == last_depth1) {
            stopped = true;
            return CallbackAction::kStop;
        }
        return CallbackAction::kContinue;
    });
    const SqpResult sol = driver.solve(model);
    ASSERT_TRUE(stopped);

    // THE PARENT IS INTERRUPTED, on the restoration-return arm itself: the
    // sub-solve came back kOptimal (its own converged verdict), and before fix1
    // the parent reported that phase's kInfeasible as if nothing had stopped
    // it.
    EXPECT_EQ(sol.status, SolveStatus::kInterrupted);
    // AND THE CERTIFICATE IS KEPT, because the sub-solve really did converge at
    // an infeasible point. A real proof is not withheld for a stop taken after
    // it was earned.
    EXPECT_TRUE(sol.infeasibility_certified);
    EXPECT_EQ(sol.counters.restoration_iters, reference.counters.restoration_iters);
}

// M1 (M6 W5 T8.6 fix1, the SQP lane's): a callback that CLEARS ITSELF from
// inside its own invocation, and then touches a capture.
//
// `clear_iteration_callback()` assigns nullptr to the std::function, which
// destroys the running callable's storage -- and every capture in it -- during
// its own invocation. The capture here is a std::string long enough to be
// heap-allocated, so a destroyed callable frees the buffer the very next line
// reads; the assertions below are the read. Under the Debug suite (Eigen
// asserts live, and the allocator's own poisoning where it has any) that is the
// falsifier available without ASan, which is not in this task's gate list.
TEST(Callback, ClearingTheCallbackFromInsideItIsSafe) {
    // ---- the SQP ----
    {
        Hs071View view;
        SqpSolver driver(quiet_sqp());
        std::string witness(256, 'w');
        int calls = 0;
        driver.set_iteration_callback([&driver, witness, &calls](const IterationEvent &) mutable {
            ++calls;
            driver.clear_iteration_callback();
            // THE POST-CLEAR READ. If the clear had taken effect immediately,
            // `witness` -- a member of this very callable -- would have been
            // destroyed one statement ago.
            EXPECT_EQ(witness.size(), 256u);
            EXPECT_EQ(witness[255], 'w');
            witness[0] = 'x';
            EXPECT_EQ(witness[0], 'x');
            return CallbackAction::kContinue;
        });
        const SqpResult sol = driver.solve(*view.bridge, hs071_start());
        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        // ONCE, AND THE CLEAR REALLY TOOK: the deferral is applied at the
        // statement after the invocation returns, so the second row fires
        // nothing.
        EXPECT_EQ(calls, 1);
        EXPECT_GT(sol.history.size(), 1u) << "non-vacuous: there was a second row to fire";
    }

    // ---- the interior-point engine ----
    {
        const auto program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver solver;
        solver.set_options(quiet_ipm(solver));
        std::string witness(256, 'w');
        int calls = 0;
        hven::solvers::IpmSolver *ipm = &solver;
        ipm->set_iteration_callback([ipm, witness, &calls](const IterationEvent &) mutable {
            ++calls;
            ipm->clear_iteration_callback();
            EXPECT_EQ(witness.size(), 256u);
            EXPECT_EQ(witness[255], 'w');
            witness[0] = 'x';
            EXPECT_EQ(witness[0], 'x');
            return CallbackAction::kContinue;
        });
        const hven::solvers::IpmResult r = ipm->solve(*program, hs071_start());
        EXPECT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_EQ(calls, 1);
        EXPECT_GT(r.iterations, 1);
    }

    // ---- the interior-point-only KKT hook, the same rule ----
    {
        const auto program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver solver;
        solver.set_options(quiet_ipm(solver));
        std::string witness(256, 'w');
        int calls = 0;
        hven::solvers::IpmSolver *ipm = &solver;
        ipm->set_kkt_hook(
            [ipm, witness, &calls](int, double, hven::ConstEigenRef<Vec>, double,
                                   hven::ConstEigenRef<Vec>, hven::ConstEigenRef<Vec>,
                                   Eigen::SparseMatrix<double, Eigen::RowMajor> &) mutable {
                ++calls;
                ipm->clear_kkt_hook();
                EXPECT_EQ(witness.size(), 256u);
                EXPECT_EQ(witness[255], 'w');
                witness[0] = 'x';
                EXPECT_EQ(witness[0], 'x');
                return 0;
            });
        const hven::solvers::IpmResult r = ipm->solve(*program, hs071_start());
        EXPECT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_EQ(calls, 1);
    }
}

// M6 W5 T8.7's RIDER (the SQP lane's M9, raised at the T8.6 fix1 re-check).
//
// THE RULE: a DIRECT set/clear -- one made while no callable is in flight --
// SUPERSEDES any pending deferral.
//
// THE DEFECT IT CLOSES. A callback that parks a deferral and then THROWS never
// reaches the apply point at the end of its own invocation, so the pending
// optional is still engaged when the exception leaves `solve()`. A caller who
// then installs a NEW callback directly used to have it silently replaced at
// the next solve entry by the stale deferred clear -- the next solve fired
// nothing, with no error and no way to see why.
//
// PINNED BOTH DIRECTIONS, on both engines and on the KKT hook: with a direct
// call after the throw the NEW callable fires; with no direct call the deferred
// clear still applies, which is the behaviour T8.6 fix1 installed and which
// must not be lost to the fix.
TEST(Callback, ADirectSetAfterAThrownDeferralSupersedesIt) {
    // ---- the SQP ----
    {
        Hs071View view;
        SqpSolver driver(quiet_sqp());
        driver.set_iteration_callback([&driver](const IterationEvent &) -> CallbackAction {
            driver.clear_iteration_callback();
            throw std::runtime_error("callback bailed after parking a deferred clear");
        });
        EXPECT_THROW(driver.solve(*view.bridge, hs071_start()), std::runtime_error);

        int fresh_calls = 0;
        driver.set_iteration_callback([&fresh_calls](const IterationEvent &) {
            ++fresh_calls;
            return CallbackAction::kContinue;
        });
        const SqpResult sol = driver.solve(*view.bridge, hs071_start());
        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_GT(fresh_calls, 0) << "the stale deferred clear replaced the new callback";
        EXPECT_EQ(static_cast<std::size_t>(fresh_calls), sol.history.size());
    }

    // ---- the SQP, THE OTHER DIRECTION: no direct call, so the deferral holds
    {
        Hs071View view;
        SqpSolver driver(quiet_sqp());
        int calls = 0;
        driver.set_iteration_callback([&driver, &calls](const IterationEvent &) -> CallbackAction {
            ++calls;
            driver.clear_iteration_callback();
            throw std::runtime_error("bail");
        });
        EXPECT_THROW(driver.solve(*view.bridge, hs071_start()), std::runtime_error);
        EXPECT_EQ(calls, 1);
        const SqpResult sol = driver.solve(*view.bridge, hs071_start());
        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_EQ(calls, 1) << "the deferred clear must still apply at the next entry";
    }

    // ---- the interior-point engine ----
    {
        const auto program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver solver;
        solver.set_options(quiet_ipm(solver));
        hven::solvers::IpmSolver *ipm = &solver;
        ipm->set_iteration_callback([ipm](const IterationEvent &) -> CallbackAction {
            ipm->clear_iteration_callback();
            throw std::runtime_error("bail");
        });
        EXPECT_THROW(ipm->solve(*program, hs071_start()), std::runtime_error);

        int fresh_calls = 0;
        ipm->set_iteration_callback([&fresh_calls](const IterationEvent &) {
            ++fresh_calls;
            return CallbackAction::kContinue;
        });
        const hven::solvers::IpmResult r = ipm->solve(*program, hs071_start());
        EXPECT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_GT(fresh_calls, 0) << "the stale deferred clear replaced the new callback";
    }

    // ---- the interior-point engine, the other direction ----
    {
        const auto program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver solver;
        solver.set_options(quiet_ipm(solver));
        hven::solvers::IpmSolver *ipm = &solver;
        int calls = 0;
        ipm->set_iteration_callback([ipm, &calls](const IterationEvent &) -> CallbackAction {
            ++calls;
            ipm->clear_iteration_callback();
            throw std::runtime_error("bail");
        });
        EXPECT_THROW(ipm->solve(*program, hs071_start()), std::runtime_error);
        EXPECT_EQ(calls, 1);
        const hven::solvers::IpmResult r = ipm->solve(*program, hs071_start());
        EXPECT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_EQ(calls, 1) << "the deferred clear must still apply at the next entry";
    }

    // ---- the interior-point-only KKT hook, both directions ----
    {
        const auto program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver solver;
        solver.set_options(quiet_ipm(solver));
        hven::solvers::IpmSolver *ipm = &solver;
        ipm->set_kkt_hook([ipm](int, double, hven::ConstEigenRef<Vec>, double,
                                hven::ConstEigenRef<Vec>, hven::ConstEigenRef<Vec>,
                                Eigen::SparseMatrix<double, Eigen::RowMajor> &) -> int {
            ipm->clear_kkt_hook();
            throw std::runtime_error("bail");
        });
        EXPECT_THROW(ipm->solve(*program, hs071_start()), std::runtime_error);

        int fresh_calls = 0;
        ipm->set_kkt_hook([&fresh_calls](int, double, hven::ConstEigenRef<Vec>, double,
                                         hven::ConstEigenRef<Vec>, hven::ConstEigenRef<Vec>,
                                         Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
            ++fresh_calls;
            return 0;
        });
        const hven::solvers::IpmResult r = ipm->solve(*program, hs071_start());
        EXPECT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_GT(fresh_calls, 0) << "the stale deferred clear replaced the new hook";
    }
    {
        const auto program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver solver;
        solver.set_options(quiet_ipm(solver));
        hven::solvers::IpmSolver *ipm = &solver;
        int calls = 0;
        ipm->set_kkt_hook([ipm, &calls](int, double, hven::ConstEigenRef<Vec>, double,
                                        hven::ConstEigenRef<Vec>, hven::ConstEigenRef<Vec>,
                                        Eigen::SparseMatrix<double, Eigen::RowMajor> &) -> int {
            ++calls;
            ipm->clear_kkt_hook();
            throw std::runtime_error("bail");
        });
        EXPECT_THROW(ipm->solve(*program, hs071_start()), std::runtime_error);
        EXPECT_EQ(calls, 1);
        const hven::solvers::IpmResult r = ipm->solve(*program, hs071_start());
        EXPECT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_EQ(calls, 1) << "the deferred clear must still apply at the next entry";
    }
}

TEST(Callback, ZeroMajorExitFiresOnceOrNever) {
    NonFiniteStartModel model;
    SqpSolver driver(quiet_sqp());
    std::vector<Seen> seen;
    driver.set_iteration_callback([&](const IterationEvent &e) {
        seen.push_back(copy_of(e));
        return CallbackAction::kContinue;
    });
    const SqpResult sol = driver.solve(model);

    // ONCE. The non-finite-start exit pushes exactly one history row and solves
    // no subproblem, and the callback fires for that row like any other -- the
    // rule is "once", not "never".
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
    EXPECT_EQ(sol.history.size(), 1u);
    EXPECT_EQ(sol.counters.major_iters, 0);
    ASSERT_EQ(seen.size(), 1u);

    // NaN IN ALL FOUR: nothing was measured at that point, and a zero would
    // read as a converged residual.
    EXPECT_TRUE(std::isnan(seen[0].stationarity));
    EXPECT_TRUE(std::isnan(seen[0].feasibility_e));
    EXPECT_TRUE(std::isnan(seen[0].feasibility_i));
    EXPECT_TRUE(std::isnan(seen[0].complementarity));
    EXPECT_EQ(seen[0].iteration, 0);
    EXPECT_EQ(seen[0].depth.value_or(-1), 0);
}

// ===========================================================================
// (3) THE INTERIOR-POINT ENGINE
// ===========================================================================

TEST(Callback, IpmContinuingEventObservesTheCommittedPoint) {
    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    hven::solvers::IpmSolver solver;
    solver.set_options(quiet_ipm(solver));

    Recorder rec;
    solver.set_iteration_callback(rec.hook());
    const hven::solvers::IpmResult r = solver.solve(*program, hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_GT(rec.seen.size(), 2u) << "non-vacuous: the solve really iterates";

    // THE FIRST EVENT IS THE START POINT, which nothing stepped to.
    EXPECT_EQ(rec.seen[0].step_norm, 0.0);
    ASSERT_EQ(rec.seen[0].x.size(), hs071_start().size());
    // THE POINT IT DESCRIBES IS THE SOLVE'S OWN START, which is x0 after the
    // interior initialization has pushed it off its bounds -- a displacement of
    // the order of that push (5e-3 on this fixture's widest coordinate) and not
    // a step, which is why `step_norm` above is exactly 0 while these are NEAR.
    EXPECT_LT((rec.seen[0].x - hs071_start()).lpNorm<Eigen::Infinity>(), 1e-2)
        << "the first event describes the point the solve started at";
    EXPECT_GT((rec.seen[0].x - hs071_start()).lpNorm<Eigen::Infinity>(), 0.0)
        << "and it is the PUSHED start, not the caller's raw x0 -- if this fires the "
           "initialization stopped moving the point and the bound above is vacuous";

    // EVERY LATER EVENT DESCRIBES THE COMMITTED POINT OF THE STEP BEFORE IT, so
    // the distance between consecutive events' points IS that event's
    // step_norm. This is the whole of behaviour change (3) (design section 2.7;
    // (2) of that list is kInterrupted on both engines -- corrected at fix1): the old late
    // callback fired below the factorization of the iterate it described, and
    // the diagnostics it carried were of the point before the commit.
    for (std::size_t k = 1; k < rec.seen.size(); ++k) {
        SCOPED_TRACE("event " + std::to_string(k));
        const double moved = (rec.seen[k].x - rec.seen[k - 1].x).lpNorm<Eigen::Infinity>();
        EXPECT_NEAR(moved, rec.seen[k].step_norm, 1e-9 * (1.0 + rec.seen[k].step_norm));
    }

    // AND THE LAST EVENT DESCRIBES THE RETURNED POINT: every in-loop exit
    // breaks above the commit, so the terminal event's iterate is the result's.
    ASSERT_EQ(rec.seen.back().x.size(), r.x.size());
    for (Eigen::Index i = 0; i < r.x.size(); ++i) {
        EXPECT_EQ(std::bit_cast<std::uint64_t>(rec.seen.back().x[i]),
                  std::bit_cast<std::uint64_t>(r.x[i]));
    }
}

namespace {
// A sink that counts `ipm.iter` lines and nothing else.
class IterCountingSink : public hven::solvers::TraceSink {
  public:
    Index rows = 0;
    void on_ipm_iter(const hven::solvers::IpmIterTraceEvent &) override { ++rows; }
    void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const hven::solvers::IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const hven::solvers::IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const hven::solvers::IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const hven::solvers::IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const hven::solvers::IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const hven::solvers::QpModeTraceEvent &) override {}
    void on_fallback_verdict(const hven::solvers::SqpFallbackVerdictTraceEvent &) override {}
};
} // namespace

TEST(Callback, IpmTerminalRowStillFires) {
    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    hven::solvers::IpmSolver solver;
    solver.set_options(quiet_ipm(solver));

    IterCountingSink sink;
    solver.attach_trace(&sink);
    Recorder rec;
    solver.set_iteration_callback(rec.hook());
    const hven::solvers::IpmResult r = solver.solve(*program, hs071_start());

    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_GT(sink.rows, 1);
    // EXACTLY the trace's own row count -- the terminal row's event included,
    // which is the claim: moving the continuing dispatch to the top of the next
    // iteration loses no event and invents none.
    EXPECT_EQ(static_cast<Index>(rec.seen.size()), sink.rows);
    EXPECT_EQ(static_cast<Index>(rec.seen.size()), r.iterations);
}

TEST(Callback, IpmStopEndsThePhaseSequence) {
    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    hven::solvers::IpmSolver solver;
    {
        hven::solvers::IpmOptions o = quiet_ipm(solver);
        o.phases = {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize};
        solver.set_options(std::move(o));
    }

    std::vector<Seen> seen;
    solver.set_iteration_callback([&](const IterationEvent &e) {
        seen.push_back(copy_of(e));
        // The second event of phase 0: the first describes the start point, so
        // stopping there would prove nothing about a committed iterate.
        return seen.size() == 2 ? CallbackAction::kStop : CallbackAction::kContinue;
    });
    const hven::solvers::IpmResult r = solver.solve(*program, hs071_start());

    EXPECT_EQ(r.status, SolveStatus::kInterrupted);
    EXPECT_EQ(solver.last_stop_reason(), hven::solvers::IpmStopReason::kInterrupted);
    ASSERT_EQ(r.phases.size(), 2u);
    EXPECT_TRUE(r.phases[0].ran);
    EXPECT_EQ(r.phases[0].status, SolveStatus::kInterrupted);
    EXPECT_EQ(r.phases[0].stop_reason, hven::solvers::IpmStopReason::kInterrupted);
    EXPECT_FALSE(r.phases[1].ran) << "a stop ends the phase SEQUENCE, not only the phase";

    // NO WORK WAS SPENT ON THE STOPPED ITERATION. The stop is honoured
    // pre-factorization, so the factorization count stands where the callback
    // observed it -- the whole reason the interrupt is not folded into the
    // terminal conjunction.
    EXPECT_EQ(seen.size(), 2u) << "the solve stopped at the event that returned kStop";
    // AND THE COUNTER SAYS SO (M6 W5 T8.6 fix1, astra item 3 -- the comment
    // above claimed this proof and the test did not assert it). A second solve
    // of the same fixture, stopped one event LATER, must have spent exactly one
    // more factorization: that difference is what makes "the stopped iteration
    // was never factorized" a measurement rather than a description.
    const auto later_program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    hven::solvers::IpmSolver later;
    {
        hven::solvers::IpmOptions o = quiet_ipm(later);
        o.phases = {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize};
        later.set_options(std::move(o));
    }
    std::size_t later_events = 0;
    later.set_iteration_callback([&](const IterationEvent &) {
        ++later_events;
        return later_events == 3 ? CallbackAction::kStop : CallbackAction::kContinue;
    });
    const hven::solvers::IpmResult r3 = later.solve(*later_program, hs071_start());
    ASSERT_EQ(r3.status, SolveStatus::kInterrupted);
    ASSERT_EQ(later_events, 3u);
    EXPECT_EQ(r3.kkt_factor_counters.factorize_count, r.kkt_factor_counters.factorize_count + 1)
        << "one more observed iterate costs exactly one more factorization, so the stop "
           "really is honoured above the factorization and not below it";

    // AND THE CALLER'S OWN PHASE COUNT: every event carries the phase index it
    // belongs to, and both of these are phase 0's.
    for (const Seen &e : seen) {
        ASSERT_TRUE(e.phase.has_value());
        EXPECT_EQ(*e.phase, 0);
        EXPECT_FALSE(e.depth.has_value()) << "depth is the SQP's field";
        EXPECT_TRUE(e.mu.has_value()) << "mu is this engine's";
    }
}

// ===========================================================================
// (3b) THE INTERIOR-POINT EVENT AND THE RESULT ARE ONE ARITHMETIC
// ===========================================================================

// M6 W5 T8.6 fix1 -- astra item 2's twin pin and the SQP lane's M3.
//
// The declared-space conversion the event builder applies and the one
// `run_phase_sequence`'s result seam applies are now ONE function, called
// twice. This is what holds them together from the outside: on a CONVERGED
// solve the terminal event describes the point the phase returns, so its four
// diagnostics and its `x`/`z` must be the RESULT's own, bit for bit.
//
// TWO ARMS, because the seam has two pieces a plain solve never reaches: the
// MakeConstraint arm has an internal fixing row, so the `-lambda_fix` fold into
// `z` and the `excluded` set the eliminated-coordinate rule builds are both
// exercised there.
namespace {
void expect_terminal_event_is_the_ipm_result(hven::solvers::IpmSolver &solver,
                                             hven::solvers::NonLinearProgram &program,
                                             const Vec &x0,
                                             hven::solvers::IpmResult *out = nullptr) {
    Recorder rec;
    solver.set_iteration_callback(rec.hook());
    const hven::solvers::IpmResult r = solver.solve(program, x0);
    if (out != nullptr) {
        *out = r;
    }
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_FALSE(rec.seen.empty());
    const Seen &last = rec.seen.back();

    EXPECT_TRUE(bit_equal(last.x, r.x)) << "the terminal event's point IS the returned point";
    EXPECT_TRUE(bit_equal(last.z, r.z)) << "and its bound prices are the returned ones";
    EXPECT_TRUE(bit_equal(last.lambda_e, r.lambda_e));
    EXPECT_TRUE(bit_equal(last.lambda_i, r.lambda_i));
    EXPECT_TRUE(same_scalar(last.stationarity, r.stationarity));
    EXPECT_TRUE(same_scalar(last.feasibility_e, r.feasibility_e));
    EXPECT_TRUE(same_scalar(last.feasibility_i, r.feasibility_i));
    EXPECT_TRUE(same_scalar(last.complementarity, r.complementarity));
    // The objective too: on a converged OPT-phase exit the result reports
    // `iters.back().prim_obj_ / scale`, which is what the event reports.
    EXPECT_TRUE(same_scalar(last.f, r.f));
}
} // namespace

TEST(Callback, IpmTerminalEventDiagnosticsAreTheResultsOwn) {
    {
        SCOPED_TRACE("HS071");
        const auto program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver solver;
        solver.set_options(quiet_ipm(solver));
        expect_terminal_event_is_the_ipm_result(solver, *program, hs071_start());
    }
    {
        SCOPED_TRACE("the fixed-variable MakeConstraint arm");
        const auto program = hven::solvers::make_nlp_program(std::make_shared<FixedVarProblem>());
        hven::solvers::IpmSolver solver;
        {
            hven::solvers::IpmOptions o = quiet_ipm(solver);
            o.fixed_variable_treatment = hven::solvers::FixedVariableTreatments::MakeConstraint;
            solver.set_options(std::move(o));
        }
        Vec x0(2);
        x0 << 0.0, 3.0;
        hven::solvers::IpmResult r;
        expect_terminal_event_is_the_ipm_result(solver, *program, x0, &r);
        // FIXTURE PREMISE: the treatment really installed its internal fixing
        // row, so the -lambda_fix fold and the excluded set were exercised.
        EXPECT_EQ(r.fixed_variable_treatment,
                  hven::solvers::FixedVariableTreatments::MakeConstraint);
        EXPECT_EQ(r.internal_fixed_lambda_e.size(), 1);
    }
}

// THE EVENT'S OBJECTIVE OBEYS THE RESULT'S OWN PROVENANCE RULE (M6 W5 T8.6
// fix1, astra item 1). `IterateInfo::prim_obj_` is NOT the caller's objective on
// a phase that never evaluates one: the FEASIBILITY phase (kSolve) leaves the
// field at its initialised zero, and a zero objective reads as a measured one.
// The result reports the true objective there only because it EVALUATES for it
// at the exit; an event evaluates nothing, so what it can honestly report is
// NaN -- solve_result.h's "unmeasured is NaN, never zero", applied to `f`.
TEST(Callback, IpmEventObjectiveIsNaNOnANonObjectiveBearingPhase) {
    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    hven::solvers::IpmSolver solver;
    {
        hven::solvers::IpmOptions o = quiet_ipm(solver);
        o.phases = {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize};
        solver.set_options(std::move(o));
    }
    Recorder rec;
    solver.set_iteration_callback(rec.hook());
    const hven::solvers::IpmResult r = solver.solve(*program, hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_EQ(r.phases.size(), 2u);
    ASSERT_TRUE(r.phases[0].ran);
    ASSERT_TRUE(r.phases[1].ran);

    std::size_t phase0 = 0;
    std::size_t phase1 = 0;
    for (const Seen &e : rec.seen) {
        ASSERT_TRUE(e.phase.has_value());
        if (*e.phase == 0) {
            ++phase0;
            EXPECT_TRUE(std::isnan(e.f))
                << "phase 0 is the FEASIBILITY phase: it evaluates no declared objective, so "
                   "the event cannot report one";
            EXPECT_TRUE(std::isnan(e.stationarity))
                << "and stationarity alone of the four is unmeasured there, as the result's "
                   "own provenance rule says";
        } else {
            ++phase1;
            EXPECT_FALSE(std::isnan(e.f)) << "the optimality phase does evaluate the objective";
        }
    }
    EXPECT_GT(phase0, 0u);
    EXPECT_GT(phase1, 0u);
    // AND THE RESULT STILL REPORTS A NUMBER, because it evaluates for one: the
    // event's NaN is a statement about what an event may claim without
    // evaluating, not about the solve.
    EXPECT_FALSE(std::isnan(r.f));
}

// THE TERMINAL EVENT DESCRIBES THE POINT THE PHASE RETURNS, `return_best`
// INCLUDED (M6 W5 T8.6 fix1, astra item 3).
//
// The convergence-check early exit fires its terminal event; until fix1 it
// fired ABOVE the `XSL = BestXSL` substitution, so on an acceptable or
// diverging exit with `return_best` on the caller was shown a point the solve
// then replaced. The dispatch is below the substitution now.
//
// THE FIXTURE IS SEARCHED FOR, NOT ASSUMED, exactly as
// SolveResult.ReturnBestReportsTheBestIterateSObjective searches for its cap:
// which settings make (i) the exit leave through the convergence-check site and
// (ii) the substitution actually fire is a fact about a trajectory, not
// something a test may assert by construction. The search states both premises;
// the pin then runs at the configuration it found.
TEST(Callback, IpmTerminalEventDescribesTheReturnedPointUnderReturnBest) {
    // The converge-check early exit is told apart from the bottom-of-loop one
    // by the record it hands out: it is reached BEFORE this iterate is
    // factorized, so the row carries IterateInfo's fresh per-iteration defaults
    // for everything a factorization would have written. That is
    // test_ipm_trace.cpp's own `looks_like_the_early_exit_site` discriminator,
    // read here off the record rather than off the serialized line.
    struct LastRowSink : public hven::solvers::TraceSink {
        hven::solvers::IterateInfo last;
        Index rows = 0;
        void on_ipm_iter(const hven::solvers::IpmIterTraceEvent &e) override {
            last = e.iterate;
            ++rows;
        }
        void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &) override {}
        void on_ipqp_reg(const hven::solvers::IpqpTraceRegEvent &) override {}
        void on_ipqp_restart(const hven::solvers::IpqpTraceRestartEvent &) override {}
        void on_ipqp_route(const hven::solvers::IpqpTraceRouteEvent &) override {}
        void on_ipqp_certify(const hven::solvers::IpqpTraceCertifyEvent &) override {}
        void on_ipqp_escape(const hven::solvers::IpqpTraceEscapeEvent &) override {}
        void on_qp_mode(const hven::solvers::QpModeTraceEvent &) override {}
        void on_fallback_verdict(const hven::solvers::SqpFallbackVerdictTraceEvent &) override {}
    };
    const auto looks_like_the_early_exit_site = [](const hven::solvers::IterateInfo &it) {
        return it.barr_obj_ == 0.0 && it.merit_val_ == 0.0 && it.ls_iters_ == 0 &&
               it.alpha_p_ == 1.0 && it.alpha_d_ == 1.0 && it.alpha_t_ == 1.0;
    };

    // THE SEARCH. `acc_*` loosened (with `max_acc_iters` at 1) is what makes
    // converge_check answer kAcceptable at the TOP of an iteration, which is
    // the convergence-check early exit; `BestCriteriaModes::kObj` on a fixture
    // whose objective RISES is what makes the best iterate an early one, so the
    // substitution has something to substitute.
    bool ran = false;
    for (const double acc : {1.0e-1, 1.0e-2, 1.0e-3}) {
        if (ran) {
            break;
        }
        const auto with_program =
            hven::solvers::make_nlp_program(std::make_shared<RisingObjectiveProblem>());
        hven::solvers::IpmSolver with;
        const auto without_program =
            hven::solvers::make_nlp_program(std::make_shared<RisingObjectiveProblem>());
        hven::solvers::IpmSolver without;
        for (hven::solvers::IpmSolver *s : {&with, &without}) {
            hven::solvers::IpmOptions o = quiet_ipm(*s);
            o.max_acc_iters = 1;
            o.acc_kkt_tol = acc;
            o.acc_econ_tol = acc;
            o.acc_icon_tol = acc;
            o.acc_bar_tol = acc;
            s->set_options(std::move(o));
        }
        {
            hven::solvers::IpmOptions o = with.options();
            o.return_best = true;
            o.best_criteria = hven::solvers::IpmSolver::BestCriteriaModes::kObj;
            with.set_options(std::move(o));
        }

        LastRowSink sink;
        with.attach_trace(&sink);
        Recorder rec;
        with.set_iteration_callback(rec.hook());
        const Vec x0 = Vec::Zero(RisingObjectiveProblem::kN);
        const hven::solvers::IpmResult b = with.solve(*with_program, x0);
        Recorder plain;
        without.set_iteration_callback(plain.hook());
        const hven::solvers::IpmResult l = without.solve(*without_program, x0);

        const bool early_exit = sink.rows > 0 && looks_like_the_early_exit_site(sink.last);
        const bool substituted = b.x.size() == l.x.size() && b.x.allFinite() &&
                                 (b.x - l.x).lpNorm<Eigen::Infinity>() > 0.0;
        if (!early_exit || !substituted || b.status == SolveStatus::kOptimal) {
            continue;
        }
        ran = true;
        ::testing::Test::RecordProperty("acc_tol", acc);
        ::testing::Test::RecordProperty("exit_status", static_cast<int>(b.status));

        ASSERT_FALSE(rec.seen.empty());
        const Seen &last = rec.seen.back();
        // THE PIN: the terminal event's point IS the returned point, and its
        // diagnostics are the result's -- NaN-aware, because the substituted
        // pair carries its own provenance and may be unmeasured.
        EXPECT_TRUE(bit_equal(last.x, b.x));
        EXPECT_TRUE(same_scalar(last.stationarity, b.stationarity));
        EXPECT_TRUE(same_scalar(last.feasibility_e, b.feasibility_e));
        EXPECT_TRUE(same_scalar(last.feasibility_i, b.feasibility_i));
        EXPECT_TRUE(same_scalar(last.complementarity, b.complementarity));
        // AND NO EVENT WAS GAINED OR LOST: moving the dispatch below the
        // substitution changes WHAT the terminal event says, never how many
        // there are.
        EXPECT_EQ(rec.seen.size(), plain.seen.size());
        EXPECT_EQ(static_cast<Index>(rec.seen.size()), sink.rows);
    }
    ASSERT_TRUE(ran) << "no acceptable tolerance in the searched set produced a "
                        "convergence-check exit whose return_best substitution fired -- the "
                        "pin above would not discriminate";
}

// ===========================================================================
// (4) A THROWING CALLBACK, ON BOTH ENGINES
// ===========================================================================

TEST(Callback, ThrowingCallbackPropagatesAndTheSolverIsReusable) {
    // ---- the SQP ----
    {
        Hs071View view;
        Ledger ledger;
        SqpSolver driver(quiet_sqp());
        driver.attach_ledger(&ledger, "cb");

        std::ostringstream os;
        hven::solvers::JsonLinesTraceSink sink(os);
        driver.attach_trace(&sink);

        // THE SECOND ROW, so at least one subproblem has already been solved
        // and its QP-engine record already written when the throw happens.
        driver.set_iteration_callback([](const IterationEvent &e) -> CallbackAction {
            if (e.iteration == 1) {
                throw std::runtime_error("a callback that leaves the solve by throwing");
            }
            return CallbackAction::kContinue;
        });
        EXPECT_THROW((void)driver.solve(*view.bridge, hs071_start()), std::runtime_error);

        // NO RECORD OF THE ABANDONED SOLVE. The solve-begin/solve-end trace pair
        // and the ledger record are deliberately NON-RAII (W2's fix-round ruling
        // R1), so a throw out of the loop leaves a begin and no end.
        const std::string text = os.str();
        EXPECT_NE(text.find("\"sqp.solve.begin\""), std::string::npos);
        EXPECT_EQ(text.find("\"sqp.solve.end\""), std::string::npos);
        EXPECT_TRUE(ledger.sqp_records().empty());

        // BUT THE SUBPROBLEM RECORDS ALREADY EMITTED STAY, which is today's
        // behaviour for ANY throw out of the loop and is stated here so that a
        // later change does not "tidy" them away: they record work that was
        // really done.
        EXPECT_FALSE(ledger.records().empty())
            << "the QP engine's own records of the subproblems this solve DID run are not "
               "rolled back by the throw";
        const std::size_t qp_records_at_throw = ledger.records().size();

        // AND THE DRIVER IS REUSABLE: the in-flight guard cleared on the unwind
        // and the borrowed model went with the frame that borrowed it, so the
        // next solve is bit-for-bit a fresh driver's.
        //
        // EVERY DETERMINISTIC FIELD, NOT A SELECTION (M6 W5 T8.6 fix1, astra
        // item 5: the report claimed this comparison and the test made a
        // narrower one). The enumeration below is SqpResult's deterministic
        // surface -- status, the point, the three price blocks, the objective,
        // the four shared diagnostics, the four work counters and the history
        // length. Timing is excluded, and only timing: `wall_seconds` and
        // `solve_impl_seconds` are informational (CLAUDE.md section 7).
        driver.clear_iteration_callback();
        const SqpResult again = driver.solve(*view.bridge, hs071_start());
        SqpSolver fresh(quiet_sqp());
        const SqpResult baseline = fresh.solve(*view.bridge, hs071_start());
        EXPECT_EQ(again.status, SolveStatus::kOptimal);
        EXPECT_EQ(again.status, baseline.status);
        EXPECT_TRUE(bit_equal(again.x, baseline.x));
        EXPECT_TRUE(bit_equal(again.lambda_e, baseline.lambda_e));
        EXPECT_TRUE(bit_equal(again.lambda_i, baseline.lambda_i));
        EXPECT_TRUE(bit_equal(again.z, baseline.z));
        EXPECT_TRUE(same_scalar(again.f, baseline.f));
        EXPECT_TRUE(same_scalar(again.stationarity, baseline.stationarity));
        EXPECT_TRUE(same_scalar(again.feasibility_e, baseline.feasibility_e));
        EXPECT_TRUE(same_scalar(again.feasibility_i, baseline.feasibility_i));
        EXPECT_TRUE(same_scalar(again.complementarity, baseline.complementarity));
        EXPECT_EQ(again.counters.major_iters, baseline.counters.major_iters);
        EXPECT_EQ(again.counters.qp_minor_iters, baseline.counters.qp_minor_iters);
        EXPECT_EQ(again.counters.factorizations, baseline.counters.factorizations);
        EXPECT_EQ(again.counters.rejected_steps, baseline.counters.rejected_steps);
        EXPECT_EQ(again.counters.evals_full, baseline.counters.evals_full);
        EXPECT_EQ(again.history.size(), baseline.history.size());
        EXPECT_GT(ledger.records().size(), qp_records_at_throw)
            << "the reused driver's own subproblems are recorded too";
    }

    // ---- the interior-point engine ----
    {
        const auto program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver solver;
        solver.set_options(quiet_ipm(solver));

        std::ostringstream os;
        hven::solvers::JsonLinesTraceSink sink(os);
        solver.attach_trace(&sink);
        solver.set_iteration_callback([](const IterationEvent &e) -> CallbackAction {
            if (e.iteration == 1) {
                throw std::runtime_error("a callback that leaves the solve by throwing");
            }
            return CallbackAction::kContinue;
        });
        EXPECT_THROW((void)solver.solve(*program, hs071_start()), std::runtime_error);
        const std::string text = os.str();
        EXPECT_NE(text.find("\"ipm.solve.begin\""), std::string::npos);
        EXPECT_EQ(text.find("\"ipm.solve.end\""), std::string::npos);

        // AND THE REUSED SOLVER'S NEXT SOLVE IS A FRESH ONE'S, FIELD FOR FIELD
        // (M6 W5 T8.6 fix1, astra item 5: this arm asserted only kOptimal and
        // constructed no reference solver at all). The enumeration is T8.5
        // fix1's own, in test_warm_protocol.cpp's expect_same_reported_numbers
        // -- status, iterations, the point, the three price blocks, the two
        // constraint blocks, the objective, the four shared diagnostics and the
        // engine's own four residual columns.
        //
        // WHAT IT DEDUCTS, AND WHY, is exactly what that enumeration deducts:
        // the two PER-SOLVER quantities. `kkt_factor_counters` is a lifetime
        // total (the reused solver carries the abandoned solve's own
        // factorizations, and must), and `kkt_analyses_this_call` is 0 on a
        // solver that has already analysed and 1 on one that has not -- both are
        // facts about the OBJECT's history, which is precisely what "reused"
        // means. Timing is excluded on top of those, and only timing.
        solver.clear_iteration_callback();
        solver.attach_trace(nullptr);
        const hven::solvers::IpmResult again = solver.solve(*program, hs071_start());

        const auto fresh_program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver fresh;
        fresh.set_options(quiet_ipm(fresh));
        const hven::solvers::IpmResult baseline = fresh.solve(*fresh_program, hs071_start());

        EXPECT_EQ(again.status, SolveStatus::kOptimal);
        EXPECT_EQ(again.status, baseline.status);
        EXPECT_EQ(again.iterations, baseline.iterations);
        EXPECT_TRUE(bit_equal(again.x, baseline.x));
        EXPECT_TRUE(bit_equal(again.lambda_e, baseline.lambda_e));
        EXPECT_TRUE(bit_equal(again.lambda_i, baseline.lambda_i));
        EXPECT_TRUE(bit_equal(again.z, baseline.z));
        EXPECT_TRUE(bit_equal(again.ce, baseline.ce));
        EXPECT_TRUE(bit_equal(again.ci, baseline.ci));
        EXPECT_TRUE(same_scalar(again.f, baseline.f));
        EXPECT_TRUE(same_scalar(again.stationarity, baseline.stationarity));
        EXPECT_TRUE(same_scalar(again.feasibility_e, baseline.feasibility_e));
        EXPECT_TRUE(same_scalar(again.feasibility_i, baseline.feasibility_i));
        EXPECT_TRUE(same_scalar(again.complementarity, baseline.complementarity));
        EXPECT_TRUE(same_scalar(again.kkt_inf, baseline.kkt_inf));
        EXPECT_TRUE(same_scalar(again.barr_inf, baseline.barr_inf));
        EXPECT_TRUE(same_scalar(again.econ_inf, baseline.econ_inf));
        EXPECT_TRUE(same_scalar(again.icon_inf, baseline.icon_inf));
        EXPECT_EQ(again.soc_steps_taken, baseline.soc_steps_taken);
        EXPECT_EQ(again.watchdog_activations, baseline.watchdog_activations);
        EXPECT_EQ(again.fixed_variable_treatment, baseline.fixed_variable_treatment);
    }
}

// ===========================================================================
// (5) THE SURFACE ITSELF
// ===========================================================================

// A callback that only READS cannot move a solve. This is the pin the interior
// corpus leg's second run is the large-scale version of.
TEST(Callback, AReadOnlyCallbackChangesNothingOnEitherEngine) {
    // ---- the SQP ----
    {
        Hs071View view;
        SqpSolver with(quiet_sqp());
        Recorder rec;
        with.set_iteration_callback(rec.hook());
        const SqpResult attached = with.solve(*view.bridge, hs071_start());

        SqpSolver without(quiet_sqp());
        const SqpResult absent = without.solve(*view.bridge, hs071_start());

        EXPECT_EQ(attached.status, absent.status);
        EXPECT_TRUE(bit_equal(attached.x, absent.x));
        EXPECT_TRUE(bit_equal(attached.lambda_e, absent.lambda_e));
        EXPECT_TRUE(bit_equal(attached.lambda_i, absent.lambda_i));
        EXPECT_TRUE(bit_equal(attached.z, absent.z));
        EXPECT_EQ(attached.counters.major_iters, absent.counters.major_iters);
        EXPECT_EQ(attached.counters.qp_minor_iters, absent.counters.qp_minor_iters);
        EXPECT_EQ(attached.counters.factorizations, absent.counters.factorizations);
        EXPECT_EQ(attached.counters.evals_full, absent.counters.evals_full)
            << "an event evaluates nothing, so the evaluation bill cannot move";
        EXPECT_EQ(attached.counters.evals_values, absent.counters.evals_values);
    }

    // ---- the interior-point engine ----
    {
        const auto with_program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver with;
        with.set_options(quiet_ipm(with));
        Recorder rec;
        with.set_iteration_callback(rec.hook());
        const hven::solvers::IpmResult attached = with.solve(*with_program, hs071_start());

        const auto without_program =
            hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
        hven::solvers::IpmSolver without;
        without.set_options(quiet_ipm(without));
        const hven::solvers::IpmResult absent = without.solve(*without_program, hs071_start());

        EXPECT_EQ(attached.status, absent.status);
        EXPECT_EQ(attached.iterations, absent.iterations);
        EXPECT_TRUE(bit_equal(attached.x, absent.x));
        EXPECT_TRUE(bit_equal(attached.z, absent.z));
        EXPECT_EQ(attached.kkt_factor_counters.factorize_count,
                  absent.kkt_factor_counters.factorize_count);
        EXPECT_EQ(attached.kkt_analyses_this_call, absent.kkt_analyses_this_call);
    }
}

// clear_iteration_callback() really removes it, on both engines.
TEST(Callback, ClearingTheCallbackStopsTheEvents) {
    Hs071View view;
    SqpSolver driver(quiet_sqp());
    Recorder rec;
    driver.set_iteration_callback(rec.hook());
    (void)driver.solve(*view.bridge, hs071_start());
    const std::size_t first = rec.seen.size();
    ASSERT_GT(first, 0u);

    driver.clear_iteration_callback();
    (void)driver.solve(*view.bridge, hs071_start());
    EXPECT_EQ(rec.seen.size(), first);

    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    hven::solvers::IpmSolver solver;
    solver.set_options(quiet_ipm(solver));
    Recorder ipm_rec;
    solver.set_iteration_callback(ipm_rec.hook());
    (void)solver.solve(*program, hs071_start());
    const std::size_t ipm_first = ipm_rec.seen.size();
    ASSERT_GT(ipm_first, 0u);
    solver.clear_iteration_callback();
    (void)solver.solve(*program, hs071_start());
    EXPECT_EQ(ipm_rec.seen.size(), ipm_first);
}
