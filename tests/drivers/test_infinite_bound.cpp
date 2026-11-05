// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-2 B7: the absent-bound threshold, CommonOptions::infinite_bound.
//
//   * a bound at or beyond the threshold is EXACTLY absent: a problem declared with
//     +/-1e20 (and beyond) solves bit-identically to the same problem declared with
//     +/-infinity, on both engines and every SQP QP mode, Nlp and pieces routes;
//   * the edges: exactly at the threshold is absent, the largest double below it is
//     a real bound; under kInfinity every finite value is real;
//   * a user-changed threshold, and a Problem re-laid when the threshold changes;
//   * the refusals by name (an equality or a fixed variable at infinity, a lower
//     bound read as +infinity, a pre-laid program or bridge under another
//     threshold);
//   * the count in Problem::statistics() and the solve-begin trace lines;
//   * the QP tier's one cutoff (QpProblem::infinite_bound).

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/drivers/result_access.h"
#include "hven/detail/model/make_nlp_program.h"
#include "hven/detail/model/nlp_model_assembly.h"
#include "hven/detail/model/nlp_problem_model.h"
#include "hven/detail/model/non_linear_program.h"
#include "hven/detail/model/problem_access.h"
#include "hven/detail/qp/ipqp_engine.h"
#include "hven/detail/qp/qp_problem.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/trace_writer.h"
#include "hven/model/nlp.h"
#include "hven/model/pieces.h"
#include "hven/model/problem.h"

namespace {

using namespace hven::solvers;
using hven::ConstVecRef;
using hven::Index;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::MatRef;
using hven::Vec;
using hven::VecRef;
using hven::solvers::detail::ProblemAccess;

constexpr double kThr = 1e20; // CommonOptions{}.infinite_bound

/// min (x0-3)^2 + (x1+4)^2 + (x2-1)^2  s.t.  row 0: g0 = x0 + x1,  row 1: g1 = x2.
/// The reference box: x0 >= 0, x1 free, x2 <= 0.5 (active at the optimum), row 0
/// >= -10 (inactive), row 1 free. Every bound is a member, so a test can state the
/// same problem with large finite values in place of the infinities.
struct Shifted : Nlp {
    Vec xl{{0.0, -kInfinity, -kInfinity}};
    Vec xu{{kInfinity, kInfinity, 0.5}};
    Vec gl{{-10.0, -kInfinity}};
    Vec gu{{kInfinity, kInfinity}};

    NlpDimensions dimensions() const override {
        return {.n = 3, .m = 2, .jacobian_nonzeros = 3, .hessian_nonzeros = 3};
    }
    void bounds(VecRef x_lower, VecRef x_upper, VecRef g_lower, VecRef g_upper) const override {
        x_lower = xl;
        x_upper = xu;
        g_lower = gl;
        g_upper = gu;
    }
    double eval_f(ConstVecRef x) const override {
        return (x[0] - 3.0) * (x[0] - 3.0) + (x[1] + 4.0) * (x[1] + 4.0) +
               (x[2] - 1.0) * (x[2] - 1.0);
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << 2.0 * (x[0] - 3.0), 2.0 * (x[1] + 4.0), 2.0 * (x[2] - 1.0);
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x[0] + x[1], x[2]; }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0, 1;
        c << 0, 1, 2;
    }
    void eval_jacobian(ConstVecRef, VecRef v) const override { v << 1.0, 1.0, 1.0; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 2;
        c << 0, 1, 2;
    }
    void eval_hessian(ConstVecRef, double obj_factor, ConstVecRef, VecRef v) const override {
        v << 2.0 * obj_factor, 2.0 * obj_factor, 2.0 * obj_factor;
    }
    std::string name() const override { return "Shifted"; }
};

/// The reference problem with every absent side written as a finite value at or
/// beyond the default threshold: seven of them (x0 upper, x1 both, x2 lower,
/// row 0 upper, row 1 both).
std::shared_ptr<Shifted> large_finite() {
    auto p = std::make_shared<Shifted>();
    p->xu[0] = kThr; // exactly at the threshold
    p->xl[1] = -kThr;
    p->xu[1] = 2.0 * kThr; // beyond it
    p->xl[2] = -1e25;
    p->gu[0] = kThr;
    p->gl[1] = -kThr;
    p->gu[1] = std::numeric_limits<double>::max();
    return p;
}

Vec start() { return Vec{{1.0, 1.0, 0.0}}; }

bool same_bits(const Vec &a, const Vec &b) {
    return a.size() == b.size() &&
           (a.size() == 0 || std::memcmp(a.data(), b.data(),
                                         static_cast<std::size_t>(a.size()) * sizeof(double)) == 0);
}

void expect_same_solve(const SolveResult &a, const SolveResult &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.iterations, b.iterations);
    EXPECT_TRUE(same_bits(a.x, b.x)) << a.x.transpose() << " vs " << b.x.transpose();
    EXPECT_TRUE(same_bits(a.z, b.z));
    EXPECT_TRUE(same_bits(a.lambda, b.lambda));
    EXPECT_TRUE(same_bits(a.g, b.g)); // the free row is NaN in both, bit for bit
    EXPECT_EQ(std::memcmp(&a.f, &b.f, sizeof(double)), 0);
    EXPECT_EQ(a.work.objective_evals, b.work.objective_evals);
    EXPECT_EQ(a.work.jacobian_evals, b.work.jacobian_evals);
    EXPECT_EQ(a.work.factorizations, b.work.factorizations);
}

IpmSolver quiet_ipm(double infinite_bound = kThr) {
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.common.infinite_bound = infinite_bound;
    return IpmSolver(o);
}

SqpSolver quiet_sqp(QpMode mode = QpMode::kWalk, double infinite_bound = kThr) {
    SqpOptions o;
    o.qp_mode = mode;
    o.common.infinite_bound = infinite_bound;
    return SqpSolver(o);
}

template <class F> void expect_refusal(F &&f, const std::vector<std::string> &needles) {
    try {
        f();
        ADD_FAILURE() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string what = e.what();
        for (const std::string &needle : needles) {
            EXPECT_NE(what.find(needle), std::string::npos) << "'" << needle << "' in: " << what;
        }
    }
}

// ---------------------------------------------------------------------------
// Absent means absent: bit-identical to the infinities.
// ---------------------------------------------------------------------------

TEST(InfiniteBound, AnNlpAtTheThresholdSolvesBitIdenticallyToItsInfinitiesOnTheIpm) {
    Problem inf_problem(std::make_shared<Shifted>());
    Problem big_problem(large_finite());
    IpmSolver a = quiet_ipm();
    IpmSolver b = quiet_ipm();
    const IpmResult ra = a.solve(inf_problem, start());
    const IpmResult rb = b.solve(big_problem, start());
    ASSERT_EQ(ra.status, SolveStatus::kOptimal);
    EXPECT_NEAR(ra.x[2], 0.5, 1e-6);
    expect_same_solve(ra, rb);
    EXPECT_TRUE(std::isnan(rb.g[1])); // row 1 is free: never evaluated
}

TEST(InfiniteBound, AnNlpAtTheThresholdSolvesBitIdenticallyToItsInfinitiesInEverySqpMode) {
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
        SCOPED_TRACE(static_cast<int>(mode));
        Problem inf_problem(std::make_shared<Shifted>());
        Problem big_problem(large_finite());
        SqpSolver a = quiet_sqp(mode);
        SqpSolver b = quiet_sqp(mode);
        const SqpResult ra = a.solve(inf_problem, start());
        const SqpResult rb = b.solve(big_problem, start());
        ASSERT_EQ(ra.status, SolveStatus::kOptimal);
        EXPECT_NEAR(ra.x[2], 0.5, 1e-6);
        expect_same_solve(ra, rb);
        EXPECT_EQ(ra.counters.qp_minor_iters, rb.counters.qp_minor_iters);
        EXPECT_EQ(ra.counters.factorizations, rb.counters.factorizations);
    }
}

TEST(InfiniteBound, TheCensusAndTheDeclarationKeyAreTheInfinitiesOwn) {
    Problem inf_problem(std::make_shared<Shifted>());
    Problem big_problem(large_finite());
    const ProblemStatistics &si = inf_problem.statistics();
    const ProblemStatistics &sb = big_problem.statistics();
    EXPECT_EQ(si.absent_bounds, 0);
    EXPECT_EQ(sb.absent_bounds, 7);
    EXPECT_EQ(sb.free_variables, 1);
    EXPECT_EQ(sb.lower_bounded_variables, 1);
    EXPECT_EQ(sb.upper_bounded_variables, 1);
    EXPECT_EQ(sb.boxed_variables, 0);
    EXPECT_EQ(sb.lower_bounded_rows, 1);
    EXPECT_EQ(sb.free_rows, 1);
    EXPECT_EQ(si.free_variables, sb.free_variables);
    EXPECT_EQ(si.free_rows, sb.free_rows);
    // A WarmStart taken on one matches the other: the key digests the bound
    // structure as the engines read it.
    EXPECT_EQ(ProblemAccess::declaration_key(inf_problem),
              ProblemAccess::declaration_key(big_problem));
}

// ---------------------------------------------------------------------------
// The edges.
// ---------------------------------------------------------------------------

TEST(InfiniteBound, JustBelowTheThresholdIsARealBoundOnBothEngines) {
    auto p = std::make_shared<Shifted>();
    const double below = std::nextafter(kThr, 0.0);
    p->xu[0] = below; // real: x0 is boxed [0, below]
    p->gu[0] = below; // real: row 0 is a range
    Problem problem(p);
    EXPECT_EQ(problem.statistics().absent_bounds, 0);
    EXPECT_EQ(problem.statistics().boxed_variables, 1);
    EXPECT_EQ(problem.statistics().range_rows, 1);

    IpmSolver ipm = quiet_ipm();
    const IpmResult r = ipm.solve(problem, start());
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
    // The barrier holds a term for the real upper bound on x0.
    const BoundSet &bounds = ProblemAccess::built_ipm_program(problem)->variable_bound_set();
    bool has_x0_upper = false;
    for (Index k = 0; k < bounds.upper_idx_.size(); ++k) {
        has_x0_upper = has_x0_upper || bounds.upper_idx_[k] == 0;
    }
    EXPECT_TRUE(has_x0_upper);

    SqpSolver sqp = quiet_sqp();
    EXPECT_EQ(sqp.solve(problem, start()).status, SolveStatus::kOptimal);
}

TEST(InfiniteBound, AtTheThresholdTheBarrierHoldsNoTerm) {
    Problem problem(large_finite());
    IpmSolver ipm = quiet_ipm();
    ASSERT_EQ(ipm.solve(problem, start()).status, SolveStatus::kOptimal);
    const BoundSet &bounds = ProblemAccess::built_ipm_program(problem)->variable_bound_set();
    // Exactly the reference box's two real sides: x0 lower, x2 upper.
    EXPECT_EQ(bounds.lower_idx_.size(), 1);
    EXPECT_EQ(bounds.upper_idx_.size(), 1);
}

TEST(InfiniteBound, UnderKInfinityEveryFiniteValueIsARealBound) {
    Problem problem(large_finite());
    IpmSolver ipm = quiet_ipm(kInfinity);
    const IpmResult r = ipm.solve(problem, start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_NEAR(r.x[0], 3.0, 1e-5);
    EXPECT_NEAR(r.x[2], 0.5, 1e-5);
    const ProblemStatistics &s = problem.statistics();
    EXPECT_EQ(s.absent_bounds, 0);
    EXPECT_EQ(s.boxed_variables, 3);
    EXPECT_EQ(s.free_variables, 0);
    EXPECT_EQ(s.range_rows, 2);
    const BoundSet &bounds = ProblemAccess::built_ipm_program(problem)->variable_bound_set();
    EXPECT_EQ(bounds.lower_idx_.size(), 3);
    EXPECT_EQ(bounds.upper_idx_.size(), 3);
    EXPECT_FALSE(std::isnan(r.g[1])); // row 1 is a range now, and measured

    SqpSolver sqp = quiet_sqp(QpMode::kWalk, kInfinity);
    const SqpResult rs = sqp.solve(problem, start());
    ASSERT_EQ(rs.status, SolveStatus::kOptimal);
    EXPECT_NEAR(rs.x[2], 0.5, 1e-6);
}

TEST(InfiniteBound, AUserThresholdMovesTheCutoff) {
    auto p = std::make_shared<Shifted>();
    p->xu[0] = 2e3;    // absent under 1e3
    p->xl[1] = -999.0; // real under 1e3 (inactive)
    p->gu[0] = 1e3;    // absent: exactly at 1e3
    Problem problem(p);
    IpmSolver ipm = quiet_ipm(1e3);
    const IpmResult r = ipm.solve(problem, start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    const ProblemStatistics &s = problem.statistics();
    EXPECT_EQ(s.absent_bounds, 2);           // x0's upper and row 0's upper
    EXPECT_EQ(s.lower_bounded_variables, 2); // x0 (upper absent) and x1 (-999 is real)
    EXPECT_EQ(s.upper_bounded_variables, 1); // x2
    EXPECT_EQ(s.free_variables, 0);
    EXPECT_EQ(s.boxed_variables, 0);
    EXPECT_EQ(s.lower_bounded_rows, 1); // row 0
    EXPECT_EQ(s.range_rows, 0);
    const BoundSet &bounds = ProblemAccess::built_ipm_program(problem)->variable_bound_set();
    EXPECT_EQ(bounds.lower_idx_.size(), 2);
    EXPECT_EQ(bounds.upper_idx_.size(), 1);

    // Under the same threshold, the same problem with infinities: bit for bit.
    auto q = std::make_shared<Shifted>();
    q->xl[1] = -999.0;
    Problem reference(q);
    IpmSolver ipm_ref = quiet_ipm(1e3);
    expect_same_solve(r, ipm_ref.solve(reference, start()));
}

// ---------------------------------------------------------------------------
// A changed threshold re-lays the Problem.
// ---------------------------------------------------------------------------

TEST(InfiniteBound, AChangedThresholdReLaysTheProblemOnBothEngines) {
    Problem problem(large_finite());
    EXPECT_EQ(problem.statistics().free_rows, 1);

    IpmSolver ipm_default = quiet_ipm();
    const IpmResult r1 = ipm_default.solve(problem, start());
    ASSERT_EQ(r1.status, SolveStatus::kOptimal);
    EXPECT_TRUE(std::isnan(r1.g[1]));

    // Row 1 flips from free to range under kInfinity: a re-lay, not a refusal.
    IpmSolver ipm_inf = quiet_ipm(kInfinity);
    const IpmResult r2 = ipm_inf.solve(problem, start());
    ASSERT_EQ(r2.status, SolveStatus::kOptimal);
    EXPECT_GE(r2.kkt_analyses_this_call, 1);
    EXPECT_EQ(problem.statistics().free_rows, 0);
    EXPECT_FALSE(std::isnan(r2.g[1]));

    SqpSolver sqp_inf = quiet_sqp(QpMode::kWalk, kInfinity);
    EXPECT_EQ(sqp_inf.solve(problem, start()).status, SolveStatus::kOptimal);

    // And back: the default solve is the fresh Problem's, bit for bit.
    const IpmResult r3 = ipm_default.solve(problem, start());
    Problem fresh(large_finite());
    IpmSolver ipm_fresh = quiet_ipm();
    expect_same_solve(r3, ipm_fresh.solve(fresh, start()));
    EXPECT_EQ(problem.statistics().free_rows, 1);
}

// ---------------------------------------------------------------------------
// Refusals by name.
// ---------------------------------------------------------------------------

TEST(InfiniteBound, AnEqualityRowAtTheThresholdIsRefusedByName) {
    auto p = std::make_shared<Shifted>();
    p->gl[0] = kThr;
    p->gu[0] = kThr;
    expect_refusal([&] { Problem problem(p); },
                   {"constraint row 0", "1e+20", "infinite_bound", "equality at infinity"});
    p->gl[0] = -2e20;
    p->gu[0] = -kThr;
    expect_refusal([&] { Problem problem(p); }, {"constraint row 0", "equality at infinity"});
}

TEST(InfiniteBound, ALowerBoundReadAsPlusInfinityIsRefusedByName) {
    auto p = std::make_shared<Shifted>();
    p->xl[1] = kThr; // with x1 upper = +inf: fixed at +infinity
    expect_refusal([&] { Problem problem(p); },
                   {"variable 1", "infinite_bound", "fixed at infinity"});
    p->xu[1] = 5.0; // now lower (read +inf) exceeds a finite upper
    expect_refusal([&] { Problem problem(p); },
                   {"variable 1", "lower bound 1e+20 exceeds upper bound 5", "infinite_bound"});
    auto q = std::make_shared<Shifted>();
    q->gl[0] = 1e30; // a row lower bound read as +infinity, against an infinite upper
    expect_refusal([&] { Problem problem(q); }, {"constraint row 0", "equality at infinity"});
}

TEST(InfiniteBound, NanAndInvertedBoundsAreStillRefusedByName) {
    auto p = std::make_shared<Shifted>();
    p->xu[2] = std::numeric_limits<double>::quiet_NaN();
    expect_refusal([&] { Problem problem(p); }, {"variable 2", "NaN"});
    auto q = std::make_shared<Shifted>();
    q->gl[0] = 3.0;
    q->gu[0] = 2.0;
    expect_refusal([&] { Problem problem(q); }, {"constraint row 0", "exceeds"});
}

TEST(InfiniteBound, AThresholdThatEmptiesAPairRefusesTheSolveAndLeavesTheProblem) {
    auto p = std::make_shared<Shifted>();
    p->xl[0] = 1e15; // real at the default; +infinity under 1e10 (shortest form below)
    Problem problem(p);
    const ProblemStatistics before = problem.statistics();
    IpmSolver small = quiet_ipm(1e10);
    expect_refusal([&] { small.solve(problem, start()); },
                   {"variable 0", "infinite_bound = 10000000000", "fixed at infinity"});
    SqpSolver small_sqp = quiet_sqp(QpMode::kWalk, 1e10);
    expect_refusal([&] { small_sqp.solve(problem, start()); }, {"variable 0", "fixed at infinity"});
    // The Problem is as it was: its census, and a default solve that is a fresh
    // Problem's, bit for bit.
    EXPECT_EQ(problem.statistics().lower_bounded_variables, before.lower_bounded_variables);
    EXPECT_EQ(problem.statistics().absent_bounds, before.absent_bounds);
    IpmSolver ipm = quiet_ipm();
    const IpmResult r = ipm.solve(problem, start());
    Problem fresh(p);
    IpmSolver ipm_fresh = quiet_ipm();
    expect_same_solve(r, ipm_fresh.solve(fresh, start()));
}

TEST(InfiniteBound, APreLaidProgramUnderAnotherThresholdIsRefusedByName) {
    Problem problem = detail::problem_from_program(make_nlp_program(large_finite()));
    IpmSolver ipm = quiet_ipm(kInfinity);
    expect_refusal([&] { ipm.solve(problem, start()); },
                   {"program laid under infinite_bound = 1e+20", "common.infinite_bound is inf"});
    EXPECT_EQ(quiet_ipm().solve(problem, start()).status, SolveStatus::kOptimal);
}

TEST(InfiniteBound, ABridgeUnderAnotherThresholdIsRefusedByName) {
    auto model = std::make_shared<NlpProblemModel>(large_finite(), kInfinity);
    NlpModelAssembly bridge(model); // laid under the default
    Problem problem = detail::problem_from_assembly(bridge);
    SqpSolver sqp = quiet_sqp(QpMode::kWalk, kInfinity);
    expect_refusal(
        [&] { sqp.solve(problem, start()); },
        {"bridge was laid under infinite_bound = 1e+20", "common.infinite_bound is inf"});
}

TEST(InfiniteBound, TheInternalModelRouteReadsTheBoxOnBothEngines) {
    // A model whose box reports +/-1e20 verbatim (an NlpProblemModel laid under
    // kInfinity), solved through the model route the corpus uses: the SQP's bridge
    // and the interior-point program both read it under the solve's threshold.
    const auto raw_model = std::make_shared<NlpProblemModel>(large_finite(), kInfinity);
    {
        // Row 0 is a range in both models (its upper is 1e20 in one, +inf in the
        // other, under kInfinity), so only the variable box differs -- and the
        // threshold removes the difference.
        auto inf_rows = std::make_shared<Shifted>();
        inf_rows->gu[0] = kThr;
        inf_rows->gl[1] = -kThr;
        inf_rows->gu[1] = std::numeric_limits<double>::max();
        const auto ref_model = std::make_shared<NlpProblemModel>(inf_rows, kInfinity);
        Problem a = detail::problem_from_model(raw_model);
        Problem b = detail::problem_from_model(ref_model);
        EXPECT_EQ(a.statistics().absent_bounds, 4); // the four variable sides
        for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
            SCOPED_TRACE(static_cast<int>(mode));
            SqpSolver sa = quiet_sqp(mode);
            SqpSolver sb = quiet_sqp(mode);
            const SqpResult ra = sa.solve(a, start());
            ASSERT_EQ(ra.status, SolveStatus::kOptimal);
            expect_same_solve(ra, sb.solve(b, start()));
        }
        IpmSolver ia = quiet_ipm();
        IpmSolver ib = quiet_ipm();
        const IpmResult ra = ia.solve(a, start());
        ASSERT_EQ(ra.status, SolveStatus::kOptimal);
        expect_same_solve(ra, ib.solve(b, start()));
        EXPECT_EQ(ProblemAccess::built_ipm_program(a)->variable_bound_set().lower_idx_.size(), 1);
    }
}

// ---------------------------------------------------------------------------
// The pieces route.
// ---------------------------------------------------------------------------

struct Pull {
    double ref;
    static constexpr Index kInputs = 1;
    double value(ConstVecRef v) const { return (v[0] - ref) * (v[0] - ref); }
    void gradient(ConstVecRef v, VecRef g) const { g[0] = 2.0 * (v[0] - ref); }
    void hessian(ConstVecRef, MatRef H) const { H(0, 0) = 2.0; }
};

Problem pieces(double lo0, double up0, double lo1, double up1) {
    AssemblyDeclaration d(3);
    d.add_objective(Pull{3.0}, Eigen::MatrixXi::Constant(1, 1, 0));
    d.add_objective(Pull{-4.0}, Eigen::MatrixXi::Constant(1, 1, 1));
    d.add_objective(Pull{1.0}, Eigen::MatrixXi::Constant(1, 1, 2));
    d.set_bounds(0, lo0, up0);
    d.set_bounds(1, lo1, up1);
    d.set_bounds(2, -kInfinity, 0.5);
    return Problem(std::move(d));
}

TEST(InfiniteBound, APiecesProblemAtTheThresholdSolvesBitIdenticallyToItsInfinities) {
    Problem inf_problem = pieces(0.0, kInfinity, -kInfinity, kInfinity);
    Problem big_problem = pieces(0.0, kThr, -2e20, kThr);
    EXPECT_EQ(big_problem.statistics().absent_bounds, 3);
    EXPECT_EQ(big_problem.statistics().free_variables, 1);
    EXPECT_EQ(big_problem.statistics().lower_bounded_variables, 1);
    IpmSolver a = quiet_ipm();
    IpmSolver b = quiet_ipm();
    const IpmResult ra = a.solve(inf_problem, start());
    const IpmResult rb = b.solve(big_problem, start());
    ASSERT_EQ(ra.status, SolveStatus::kOptimal);
    expect_same_solve(ra, rb);
    EXPECT_EQ(ProblemAccess::declaration_key(inf_problem),
              ProblemAccess::declaration_key(big_problem));
}

TEST(InfiniteBound, APiecesProblemReLaysUnderAnotherThreshold) {
    Problem problem = pieces(0.0, kThr, -kThr, kThr);
    IpmSolver inf = quiet_ipm(kInfinity);
    const IpmResult r = inf.solve(problem, start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_NEAR(r.x[2], 0.5, 1e-6);
    EXPECT_EQ(problem.statistics().absent_bounds, 0);
    EXPECT_EQ(problem.statistics().boxed_variables, 2);
    EXPECT_EQ(ProblemAccess::built_ipm_program(problem)->variable_bound_set().upper_idx_.size(), 3);
    // And back to the default: the fresh Problem's solve, bit for bit.
    const IpmResult back = quiet_ipm().solve(problem, start());
    Problem fresh = pieces(0.0, kThr, -kThr, kThr);
    expect_same_solve(back, quiet_ipm().solve(fresh, start()));
    EXPECT_EQ(problem.statistics().absent_bounds, 3);
}

TEST(InfiniteBound, APiecesBoundReadAsPlusInfinityIsRefusedByName) {
    expect_refusal([] { pieces(kThr, kInfinity, -kInfinity, kInfinity); },
                   {"variable 0", "no interval", "infinite_bound"});
}

// ---------------------------------------------------------------------------
// The solve-begin trace lines count what the threshold made absent.
// ---------------------------------------------------------------------------

TEST(InfiniteBound, TheSolveBeginLinesCarryTheAbsentCount) {
    Problem problem(large_finite());
    {
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        SqpSolver sqp = quiet_sqp();
        sqp.attach_trace(&sink);
        ASSERT_EQ(sqp.solve(problem, start()).status, SolveStatus::kOptimal);
        const std::string text = os.str();
        const auto begin = text.find("\"ev\":\"sqp.solve.begin\"");
        ASSERT_NE(begin, std::string::npos);
        const std::string line = text.substr(begin, text.find('\n', begin) - begin);
        EXPECT_NE(line.find("\"vars_free\":1"), std::string::npos) << line;
        EXPECT_NE(line.find("\"absent_bounds\":7"), std::string::npos) << line;
    }
    {
        std::ostringstream os;
        JsonLinesTraceSink sink(os);
        IpmSolver ipm = quiet_ipm();
        ipm.attach_trace(&sink);
        ASSERT_EQ(ipm.solve(problem, start()).status, SolveStatus::kOptimal);
        const std::string text = os.str();
        const auto begin = text.find("\"ev\":\"ipm.solve.begin\"");
        ASSERT_NE(begin, std::string::npos);
        const std::string line = text.substr(begin, text.find('\n', begin) - begin);
        EXPECT_NE(line.find("\"vars_free\":1"), std::string::npos) << line;
        EXPECT_NE(line.find("\"absent_bounds\":7"), std::string::npos) << line;
    }
}

// ---------------------------------------------------------------------------
// The QP tier's one cutoff.
// ---------------------------------------------------------------------------

QpProblem box_qp(double lo, double up) {
    QpProblem qp;
    qp.H = hven::SpMatRM(1, 1);
    qp.H.insert(0, 0) = 1.0;
    qp.H.makeCompressed();
    qp.g = Vec::Zero(1);
    qp.Ae.resize(0, 1);
    qp.Ai.resize(0, 1);
    qp.be.resize(0);
    qp.bi.resize(0);
    qp.lower = Vec::Constant(1, lo);
    qp.upper = Vec::Constant(1, up);
    return qp;
}

TEST(InfiniteBound, TheQpTierReadsTheSubproblemsOwnCutoff) {
    QpProblem qp = box_qp(-kThr, kThr);
    EXPECT_EQ(qp.infinite_bound, kThr); // a hand-built problem keeps the default
    EXPECT_FALSE(qp.has_lower(0));
    EXPECT_FALSE(qp.has_upper(0));
    const IpqpBox absent = make_ipqp_box(qp, kInfinity);
    EXPECT_EQ(absent.lo_eff(0), -kInfinity);
    EXPECT_EQ(absent.up_eff(0), kInfinity);

    qp.infinite_bound = kInfinity; // the SQP's stamp: every finite value is real
    EXPECT_TRUE(qp.has_lower(0));
    EXPECT_TRUE(qp.has_upper(0));
    const IpqpBox real = make_ipqp_box(qp, kInfinity);
    EXPECT_EQ(real.lo_eff(0), -kThr);
    EXPECT_EQ(real.up_eff(0), kThr);

    qp.lower(0) = std::nextafter(-kThr, 0.0);
    qp.infinite_bound = kThr;
    EXPECT_TRUE(qp.has_lower(0)); // just inside the cutoff: real

    qp.infinite_bound = 0.0;
    EXPECT_THROW(qp.validate(), std::invalid_argument);
    qp.infinite_bound = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(qp.validate(), std::invalid_argument);
}

} // namespace
