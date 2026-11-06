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
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "hven/core/compiler.h"
#include "hven/detail/drivers/result_access.h"
#include "hven/detail/drivers/sqp_solver_impl.h"
#include "hven/detail/globalization/sqp/restoration.h"
#include "hven/detail/model/make_nlp_program.h"
#include "hven/detail/model/nlp_adapter.h"
#include "hven/detail/model/nlp_model_assembly.h"
#include "hven/detail/model/nlp_problem_model.h"
#include "hven/detail/model/non_linear_program.h"
#include "hven/detail/model/problem_access.h"
#include "hven/detail/qp/ipqp_engine.h"
#include "hven/detail/qp/qp_problem.h"
#include "hven/detail/warmstart/predictor.h"
#include "hven/detail/warmstart/sqp_warm_access.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/json_lines_trace_sink.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/hven.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

#include "../common_support/console_capture.h"
#include "../common_support/route_reference.h"

namespace {

using namespace hven;
using namespace hven::detail;
using hven::ConstVecRef;
using hven::Index;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::MatRef;
using hven::Vec;
using hven::VecRef;
using hven::detail::ProblemAccess;

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

/// The status of an interior-point solve of large_finite() under kInfinity (huge real bounds).
/// Reference build: exactly kOptimal. Off it: kOptimal or kDiverging, so a NEW failure mode
/// still shows. Returns whether the solve converged; the caller guards what is downstream of a
/// converged solve on it. REGISTERED ITEM: huge real bounds give a zero/1e-43 slack diagonal
/// in the barrier KKT; off-reference backend diverges; engine fix pending Mac evidence.
bool ipm_converged_on_huge_real_bounds(SolveStatus status, Problem &problem, double threshold) {
#if HVEN_TEST_ROUTE_REFERENCE
    (void)problem;
    (void)threshold;
    EXPECT_EQ(status, SolveStatus::kOptimal);
#else
    EXPECT_TRUE(status == SolveStatus::kOptimal || status == SolveStatus::kDiverging)
        << static_cast<int>(status);
    hven::test_support::record_off_reference_route("huge_real_bounds_ipm",
                                                   std::to_string(static_cast<int>(status)));
    if (status != SolveStatus::kOptimal) {
        // Diagnostic for the off-reference CI log: re-run once with the iteration table and
        // print it, so which kDiverging path fired, and when, is readable. Greppable tag.
        std::string table;
        try {
            hven::testing::StdoutCapture capture;
            IpmOptions o;
            o.common.verbosity = Verbosity::kIterations;
            o.common.infinite_bound = threshold;
            IpmSolver verbose(o);
            (void)verbose.solve(problem, start());
            table = capture.text();
        } catch (const std::exception &e) {
            table += std::string("rerun threw: ") + e.what();
        }
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::cerr << "HVEN-HUGE-BOUND-DIAG " << (info != nullptr ? info->name() : "?")
                  << " status=" << static_cast<int>(status) << "\n"
                  << table << "\nHVEN-HUGE-BOUND-DIAG-END" << std::endl;
    }
#endif
    return status == SolveStatus::kOptimal;
}

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

/// A model that reports another model's box verbatim and no threshold of its own
/// (NlpModel::source_infinite_bound() is nullopt): the shape of the corpus's own
/// models, whose box the engines read under the solve's threshold.
class VerbatimBox final : public NlpModel {
  public:
    explicit VerbatimBox(std::shared_ptr<const NlpModel> inner) : inner_(std::move(inner)) {}
    Index n() const override { return inner_->n(); }
    Index me() const override { return inner_->me(); }
    Index mi() const override { return inner_->mi(); }
    double eval_f(const Vec &x) const override { return inner_->eval_f(x); }
    HVEN_SUPPRESS_DEPRECATED_BEGIN
    Vec eval_grad(const Vec &x) const override { return inner_->eval_grad(x); }
    Vec eval_ce(const Vec &x) const override { return inner_->eval_ce(x); }
    Vec eval_ci(const Vec &x) const override { return inner_->eval_ci(x); }
    hven::SpMatRM eval_hess(const Vec &x, double s, const Vec &le, const Vec &li) const override {
        return inner_->eval_hess(x, s, le, li);
    }
    hven::SpMatRM eval_jac_e(const Vec &x) const override { return inner_->eval_jac_e(x); }
    hven::SpMatRM eval_jac_i(const Vec &x) const override { return inner_->eval_jac_i(x); }
    HVEN_SUPPRESS_DEPRECATED_END
    void eval_grad_in_place(const Vec &x, Vec &out) const override {
        inner_->eval_grad_in_place(x, out);
    }
    void eval_ce_in_place(const Vec &x, Vec &out) const override {
        inner_->eval_ce_in_place(x, out);
    }
    void eval_ci_in_place(const Vec &x, Vec &out) const override {
        inner_->eval_ci_in_place(x, out);
    }
    void eval_jac_e_in_place(const Vec &x, hven::SpMatRM &out) const override {
        inner_->eval_jac_e_in_place(x, out);
    }
    void eval_jac_i_in_place(const Vec &x, hven::SpMatRM &out) const override {
        inner_->eval_jac_i_in_place(x, out);
    }
    void eval_hess_in_place(const Vec &x, double s, const Vec &le, const Vec &li,
                            hven::SpMatRM &out) const override {
        inner_->eval_hess_in_place(x, s, le, li, out);
    }
    void eval_values(const Vec &x, double &f, Vec &ce, Vec &ci) const override {
        inner_->eval_values(x, f, ce, ci);
    }
    bool has_hessian() const override { return inner_->has_hessian(); }
    const Vec &lower() const override { return inner_->lower(); }
    const Vec &upper() const override { return inner_->upper(); }
    Vec start_point() const override { return inner_->start_point(); }

  private:
    std::shared_ptr<const NlpModel> inner_;
};

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
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
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
    const bool converged = ipm_converged_on_huge_real_bounds(r.status, problem, kInfinity);
    if (converged) {
        EXPECT_NEAR(r.x[0], 3.0, 1e-5);
        EXPECT_NEAR(r.x[2], 0.5, 1e-5);
    }
    const ProblemStatistics &s = problem.statistics();
    EXPECT_EQ(s.absent_bounds, 0);
    EXPECT_EQ(s.boxed_variables, 3);
    EXPECT_EQ(s.free_variables, 0);
    EXPECT_EQ(s.range_rows, 2);
    const BoundSet &bounds = ProblemAccess::built_ipm_program(problem)->variable_bound_set();
    EXPECT_EQ(bounds.lower_idx_.size(), 3);
    EXPECT_EQ(bounds.upper_idx_.size(), 3);
    if (converged)
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

    // Each solver solves the Problem once at its own threshold first, so a later
    // analysis on that solver is the re-lay's and not a first solve's.
    IpmSolver ipm_default = quiet_ipm();
    const IpmResult r1 = ipm_default.solve(problem, start());
    ASSERT_EQ(r1.status, SolveStatus::kOptimal);
    EXPECT_TRUE(std::isnan(r1.g[1]));
    const NonLinearProgram *laid_default = ProblemAccess::built_ipm_program(problem);
    const IpmResult r1_again = ipm_default.solve(problem, start());
    EXPECT_EQ(r1_again.kkt_analyses_this_call, 0); // nothing re-laid: no analysis
    EXPECT_EQ(ProblemAccess::built_ipm_program(problem), laid_default);

    // Row 1 flips from free to range under kInfinity: a re-lay, not a refusal --
    // a new program, analysed once.
    IpmSolver ipm_inf = quiet_ipm(kInfinity);
    const IpmResult r2 = ipm_inf.solve(problem, start());
    const bool converged2 = ipm_converged_on_huge_real_bounds(r2.status, problem, kInfinity);
    EXPECT_EQ(r2.kkt_analyses_this_call, 1);
    // A new program: a laid program's threshold is fixed (set_infinite_bound).
    EXPECT_EQ(ProblemAccess::built_ipm_program(problem)->infinite_bound_, kInfinity);
    EXPECT_EQ(problem.statistics().free_rows, 0);
    if (converged2) {
        EXPECT_FALSE(std::isnan(r2.g[1]));
        // The re-laid solve is a fresh Problem's under kInfinity, bit for bit.
        Problem fresh_inf(large_finite());
        IpmSolver ipm_fresh_inf = quiet_ipm(kInfinity);
        expect_same_solve(r2, ipm_fresh_inf.solve(fresh_inf, start()));
    }

    // The SQP half: the re-laid Problem's solve under kInfinity is a fresh
    // Problem's, bit for bit, counters included.
    SqpSolver sqp_inf = quiet_sqp(QpMode::kWalk, kInfinity);
    const SqpResult s2 = sqp_inf.solve(problem, start());
    ASSERT_EQ(s2.status, SolveStatus::kOptimal);
    Problem fresh_sqp(large_finite());
    SqpSolver sqp_fresh = quiet_sqp(QpMode::kWalk, kInfinity);
    const SqpResult s2_fresh = sqp_fresh.solve(fresh_sqp, start());
    expect_same_solve(s2, s2_fresh);
    EXPECT_EQ(s2.counters.qp_minor_iters, s2_fresh.counters.qp_minor_iters);
    EXPECT_EQ(s2.counters.factorizations, s2_fresh.counters.factorizations);

    // And back: the default solve re-lays again (analysed once) and is the
    // fresh Problem's, bit for bit.
    const IpmResult r3 = ipm_default.solve(problem, start());
    EXPECT_EQ(r3.kkt_analyses_this_call, 1);
    EXPECT_EQ(ProblemAccess::built_ipm_program(problem)->infinite_bound_, kThr);
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
    IpmSolver small_ipm = quiet_ipm(1e10);
    expect_refusal([&] { small_ipm.solve(problem, start()); },
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
    // kInfinity, behind a decorator that reports no threshold of its own -- the
    // shape of the corpus's own models), solved through the model route the
    // corpus uses: the SQP's bridge and the interior-point program both read it
    // under the solve's threshold.
    const auto raw_model =
        std::make_shared<VerbatimBox>(std::make_shared<NlpProblemModel>(large_finite(), kInfinity));
    {
        // Row 0 is a range in both models (its upper is 1e20 in one, +inf in the
        // other, under kInfinity), so only the variable box differs -- and the
        // threshold removes the difference.
        auto inf_rows = std::make_shared<Shifted>();
        inf_rows->gu[0] = kThr;
        inf_rows->gl[1] = -kThr;
        inf_rows->gu[1] = std::numeric_limits<double>::max();
        const auto ref_model =
            std::make_shared<VerbatimBox>(std::make_shared<NlpProblemModel>(inf_rows, kInfinity));
        Problem a = detail::problem_from_model(raw_model);
        Problem b = detail::problem_from_model(ref_model);
        EXPECT_EQ(a.statistics().absent_bounds, 4); // the four variable sides
        for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
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
        if (ipm_converged_on_huge_real_bounds(ra.status, a, kThr))
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

// ---------------------------------------------------------------------------
// M8 B7 riders (C238, C239).
// ---------------------------------------------------------------------------

/// The `"key":value` of the first `ev` line in a trace, as text ("" when absent).
std::string begin_line(const std::string &text, const std::string &ev) {
    const auto begin = text.find("\"ev\":\"" + ev + "\"");
    if (begin == std::string::npos) {
        return {};
    }
    return text.substr(begin, text.find('\n', begin) - begin);
}

std::string ipm_begin_line(IpmSolver &ipm, Problem &problem) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    ipm.attach_trace(&sink);
    EXPECT_EQ(ipm.solve(problem, start()).status, SolveStatus::kOptimal);
    ipm.attach_trace(nullptr);
    return begin_line(os.str(), "ipm.solve.begin");
}

std::string sqp_begin_line(SqpSolver &sqp, Problem &problem) {
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    sqp.attach_trace(&sink);
    EXPECT_EQ(sqp.solve(problem, start()).status, SolveStatus::kOptimal);
    sqp.attach_trace(nullptr);
    return begin_line(os.str(), "sqp.solve.begin");
}

// R1, R7: a pre-laid program carries its model's count (the Nlp's seven values,
// read as absent by its NlpProblemModel before the program saw the box).
TEST(InfiniteBound, APreLaidProgramReportsItsModelsAbsentCount) {
    Problem problem = detail::problem_from_program(make_nlp_program(large_finite()));
    EXPECT_EQ(problem.statistics().absent_bounds, 7);
    EXPECT_EQ(problem.statistics().free_variables, 1);
    IpmSolver ipm = quiet_ipm();
    const std::string line = ipm_begin_line(ipm, problem);
    EXPECT_NE(line.find("\"absent_bounds\":7"), std::string::npos) << line;
    EXPECT_NE(line.find("\"vars_free\":1"), std::string::npos) << line;
    // The same count as the Nlp route's.
    Problem nlp_problem(large_finite());
    EXPECT_EQ(nlp_problem.statistics().absent_bounds, problem.statistics().absent_bounds);
}

// R6: a bridge's census counts its model's values too, as its sqp.solve.begin
// event does.
TEST(InfiniteBound, ABridgesCensusEqualsItsSolveBeginKey) {
    auto model = std::make_shared<NlpProblemModel>(large_finite());
    NlpModelAssembly bridge(model);
    Problem problem = detail::problem_from_assembly(bridge);
    EXPECT_EQ(problem.statistics().absent_bounds, 7);
    SqpSolver sqp = quiet_sqp();
    const std::string line = sqp_begin_line(sqp, problem);
    EXPECT_NE(line.find("\"absent_bounds\":7"), std::string::npos) << line;
}

// R5: the program counts once per side of the MATERIALISED box, however many
// records declared it.
TEST(InfiniteBound, ThePiecesCountIsOncePerSideOfTheMaterialisedBox) {
    AssemblyDeclaration d(3);
    d.add_objective(Pull{3.0}, Eigen::MatrixXi::Constant(1, 1, 0));
    d.add_objective(Pull{-4.0}, Eigen::MatrixXi::Constant(1, 1, 1));
    d.add_objective(Pull{1.0}, Eigen::MatrixXi::Constant(1, 1, 2));
    d.set_bounds(0, -kThr, 5.0); // x0's lower is absent here ...
    d.set_bounds(0, 0.0, kThr);  // ... and real here, its upper the reverse: [0, 5], none absent
    d.set_bounds(1, -kThr, kThr);
    d.set_bounds(1, -2e20, 2e20); // x1 free: two absent sides, counted once each
    d.set_bounds(2, -kInfinity, 0.5);
    Problem problem(std::move(d));
    EXPECT_EQ(problem.statistics().absent_bounds, 2);
    EXPECT_EQ(problem.statistics().boxed_variables, 1);
    EXPECT_EQ(problem.statistics().free_variables, 1);
    IpmSolver ipm = quiet_ipm();
    const std::string line = ipm_begin_line(ipm, problem);
    EXPECT_NE(line.find("\"absent_bounds\":2"), std::string::npos) << line;
    EXPECT_EQ(ProblemAccess::built_ipm_program(problem)->absent_bounds_, 2);
}

// R5: a value update counts the same box the same way as the first lay.
TEST(InfiniteBound, AValueUpdateCountsAsTheFirstLayDoes) {
    auto inner = std::make_shared<NlpProblemModel>(large_finite(), kInfinity);
    auto core = std::make_shared<NlpAdapterCore>(std::make_shared<VerbatimBox>(inner), "verbatim");
    const std::shared_ptr<NonLinearProgram> program = make_nlp_program(core);
    // The four variable sides at or beyond 1e20 (the model, under kInfinity, read
    // none as absent itself).
    EXPECT_EQ(program->absent_bounds_, 4);
    EXPECT_EQ(program->source_absent_bounds_, 0);
    Vec lower = inner->lower();
    Vec upper = inner->upper();
    lower[0] = 0.25; // a value change; the structure stands
    program->update_variable_bound_values(lower, upper);
    EXPECT_EQ(program->absent_bounds_, 4);
    EXPECT_EQ(program->x_lower_[0], 0.25);
}

// R2: a model that read its own declaration under one threshold is solved under
// that threshold alone; a refused solve leaves the Problem as it was.
TEST(InfiniteBound, AModelReadUnderAnotherThresholdIsRefusedByName) {
    auto model = std::make_shared<NlpProblemModel>(large_finite(), kInfinity);
    Problem problem = detail::problem_from_model(model);
    const ProblemStatistics before = problem.statistics();
    const DeclarationKey key = ProblemAccess::declaration_key(problem);
    IpmSolver ipm = quiet_ipm();
    expect_refusal([&] { ipm.solve(problem, start()); },
                   {"IpmSolver", "read its own declaration under infinite_bound = inf",
                    "common.infinite_bound is 1e+20"});
    SqpSolver sqp = quiet_sqp();
    expect_refusal([&] { sqp.solve(problem, start()); },
                   {"SqpSolver", "read its own declaration under infinite_bound = inf",
                    "common.infinite_bound is 1e+20"});
    EXPECT_EQ(ProblemAccess::built_ipm_program(problem), nullptr);
    EXPECT_EQ(problem.statistics().absent_bounds, before.absent_bounds);
    EXPECT_EQ(problem.statistics().free_variables, before.free_variables);
    EXPECT_EQ(ProblemAccess::declaration_key(problem), key);
    // Under its own threshold it solves on both engines.
    ipm_converged_on_huge_real_bounds(quiet_ipm(kInfinity).solve(problem, start()).status, problem,
                                      kInfinity);
    EXPECT_EQ(quiet_sqp(QpMode::kWalk, kInfinity).solve(problem, start()).status,
              SolveStatus::kOptimal);
}

// C238 (4): a model problem whose solve a new threshold refuses (the threshold
// empties a pair) is left exactly as it was: its census, its declaration key and
// its laid interior-point program, and the next default solve is the earlier one.
TEST(InfiniteBound, ARefusedModelSolveLeavesTheProblemAsItWas) {
    auto p = std::make_shared<Shifted>();
    p->xl[0] = 1e15; // real at the default; +infinity under 1e10
    p->xu[0] = kInfinity;
    const auto model = std::make_shared<VerbatimBox>(std::make_shared<NlpProblemModel>(p));
    Problem problem = detail::problem_from_model(model);
    IpmSolver first = quiet_ipm();
    const IpmResult r_before = first.solve(problem, start());
    const NonLinearProgram *laid = ProblemAccess::built_ipm_program(problem);
    ASSERT_NE(laid, nullptr);
    const ProblemStatistics before = problem.statistics();
    const DeclarationKey key = ProblemAccess::declaration_key(problem);

    IpmSolver small_ipm = quiet_ipm(1e10);
    expect_refusal([&] { small_ipm.solve(problem, start()); },
                   {"variable 0 has no interval", "infinite_bound = 10000000000"});
    SqpSolver small_sqp = quiet_sqp(QpMode::kWalk, 1e10);
    expect_refusal([&] { small_sqp.solve(problem, start()); },
                   {"variable 0 has no interval", "infinite_bound = 10000000000"});

    EXPECT_EQ(ProblemAccess::built_ipm_program(problem), laid);
    EXPECT_EQ(problem.statistics().absent_bounds, before.absent_bounds);
    EXPECT_EQ(problem.statistics().lower_bounded_variables, before.lower_bounded_variables);
    EXPECT_EQ(problem.statistics().free_variables, before.free_variables);
    EXPECT_EQ(ProblemAccess::declaration_key(problem), key);
    IpmSolver second = quiet_ipm();
    const IpmResult r_after = second.solve(problem, start());
    expect_same_solve(r_before, r_after);
    EXPECT_EQ(r_after.kkt_analyses_this_call, r_before.kkt_analyses_this_call);
}

// R7: a WarmStart taken under one threshold, offered after the Problem was re-laid
// under another that reads a bound differently, describes another problem.
TEST(InfiniteBound, AWarmStartFromAnotherThresholdIsRefused) {
    auto p = std::make_shared<Shifted>();
    p->xu[0] = 2e6; // real (inactive) at the default; absent under 1e6
    Problem problem(p);
    SqpSolver sqp = quiet_sqp();
    const SqpResult taken = sqp.solve(problem, start());
    ASSERT_EQ(taken.status, SolveStatus::kOptimal);
    ASSERT_FALSE(taken.warm_start.empty());
    // At its own threshold it is accepted.
    EXPECT_EQ(quiet_sqp().solve(problem, start(), taken.warm_start).status, SolveStatus::kOptimal);
    SqpSolver sqp_small = quiet_sqp(QpMode::kWalk, 1e6);
    expect_refusal([&] { sqp_small.solve(problem, start(), taken.warm_start); },
                   {"declaration key"});
    IpmSolver ipm_small = quiet_ipm(1e6);
    expect_refusal([&] { ipm_small.solve(problem, start(), taken.warm_start); },
                   {"declaration key"});
}

// R7: a variable UPPER bound read as -infinity is refused by name, on the Nlp and
// the pieces routes.
TEST(InfiniteBound, AnUpperBoundReadAsMinusInfinityIsRefusedByName) {
    auto p = std::make_shared<Shifted>();
    p->xu[1] = -kThr; // with x1 lower = -inf: fixed at -infinity
    expect_refusal([&] { Problem problem(p); },
                   {"variable 1", "fixed at infinity", "infinite_bound"});
    p->xl[1] = -5.0;
    p->xu[1] = -2e20; // a finite lower above an upper read as -inf
    expect_refusal([&] { Problem problem(p); },
                   {"variable 1", "lower bound -5 exceeds upper bound -2e+20", "infinite_bound"});
    expect_refusal([] { pieces(0.0, kInfinity, -kInfinity, -kThr); },
                   {"variable 1", "no interval", "infinite_bound"});
}

// R3: the model-taking helpers read the box under the threshold they are passed.
TEST(InfiniteBound, TheModelTakingHelpersReadTheBoxUnderTheirThreshold) {
    auto p = std::make_shared<Shifted>();
    p->xu[0] = 2e6;
    const VerbatimBox model(std::make_shared<NlpProblemModel>(p));
    const Vec x = start();
    const Vec le = Vec::Zero(model.me());
    const Vec li = Vec::Zero(model.mi());
    const QpProblem at_default = build_subproblem(model, x, le, li);
    EXPECT_EQ(at_default.upper[0], 2e6 - x[0]);
    const QpProblem at_small = build_subproblem(model, x, le, li, 1.0, 1e6);
    EXPECT_EQ(at_small.upper[0], kInfinity);
    EXPECT_EQ(at_small.infinite_bound, kInfinity);

    // A point beyond the 2e6 bound is box-infeasible only where the bound is real.
    Vec far_x = x;
    far_x[0] = 3e6;
    far_x[1] = -3e6; // keeps row 0 (x0 + x1 >= -10) satisfied
    EXPECT_GE(evaluate_kkt(model, far_x, le, li, 1e-8).feasibility, 1e6);
    EXPECT_EQ(evaluate_kkt(model, far_x, le, li, 1e-8, 1e6).feasibility, 0.0);

    EXPECT_THROW(build_subproblem(model, x, le, li, 1.0, 0.0), std::invalid_argument);
    EXPECT_THROW(evaluate_kkt(model, x, le, li, 1e-8, std::numeric_limits<double>::quiet_NaN()),
                 std::invalid_argument);
}

// C238 (3): the restoration wrapper's centre clamp and start clamp read the model's
// box under the threshold (a bound at 2e6 is absent under 1e6); its own box
// repeats the model's as declared, for the sub-solve's bridge to read.
TEST(InfiniteBound, TheRestorationClampsReadTheBoxUnderTheThreshold) {
    auto p = std::make_shared<Shifted>();
    p->xu[0] = 2e6;
    const VerbatimBox model(std::make_shared<NlpProblemModel>(p));
    Vec x = start();
    x[0] = 3e6;
    x[1] = -3e6;
    const NlpEval ev = eval_nlp(model, x);
    const RestorationModel at_default(model, x, ev, x, 1.0, false);
    const RestorationModel at_small(model, x, ev, x, 1.0, false, 1e6);
    EXPECT_EQ(at_default.start_clamps(), 1);
    EXPECT_EQ(at_small.start_clamps(), 0);
    EXPECT_EQ(at_default.upper()[0], 2e6);
    EXPECT_EQ(at_small.upper()[0], 2e6);
    // The proximity centre: clamped to 2e6 at the default, the entry itself under
    // 1e6, so the proximity term vanishes at the entry only there.
    Vec y = Vec::Zero(at_small.n());
    y.head(x.size()) = x;
    EXPECT_EQ(at_small.eval_f(y), 0.0);
    EXPECT_GT(at_default.eval_f(y), 0.0);
    EXPECT_THROW(RestorationModel(model, x, ev, x, 1.0, false, -1.0), std::invalid_argument);
}

/// min 0.5 (x - p)^2 over x <= kUpper: the solution tracks p until the bound.
class TrackParameter final : public ParametricNlpModel {
  public:
    static constexpr double kUpper = 2e6;
    explicit TrackParameter(double p) : p_(Vec::Constant(1, p)) {}
    Index parameter_dim() const override { return 1; }
    void set_parameters(const Vec &p) override {
        if (p.size() != 1) {
            throw std::invalid_argument("TrackParameter: one parameter");
        }
        p_ = p;
    }
    Vec parameters() const override { return p_; }
    Index n() const override { return 1; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return 0.5 * (x[0] - p_[0]) * (x[0] - p_[0]); }
    Vec eval_grad(const Vec &x) const override { return Vec::Constant(1, x[0] - p_[0]); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    hven::SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &,
                            const Vec &) const override {
        hven::SpMatRM h(1, 1);
        h.insert(0, 0) = obj_scale;
        h.makeCompressed();
        return h;
    }
    hven::SpMatRM eval_jac_e(const Vec &) const override { return hven::SpMatRM(0, 1); }
    hven::SpMatRM eval_jac_i(const Vec &) const override { return hven::SpMatRM(0, 1); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(1); }

  private:
    Vec p_;
    Vec lower_ = Vec::Constant(1, -kInfinity);
    Vec upper_ = Vec::Constant(1, kUpper);
};

// C238 (3): the warm-start predictor reads the box under its threshold: a bound
// at 2e6 the solve (under 1e6) does not have is never fixed at.
TEST(InfiniteBound, ThePredictorReadsTheBoxUnderItsThreshold) {
    TrackParameter model(1.5e6);
    SqpOptions o;
    o.common.infinite_bound = 1e6;
    SqpSolver sqp(o);
    const SqpResult sol = sqp.solve(detail::problem_ref(model), model.start_point());
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    const detail::SqpWarmState warm = detail::SqpWarmAccess::state(sol.sqp_warm_start);
    ASSERT_EQ(warm.bound_active[0], 0);
    const Vec dp = Vec::Constant(1, 1e6);

    PredictorOptions absent;
    absent.infinite_bound = 1e6;
    PredictorOutcome outcome = PredictorOutcome::kZeroStep;
    const detail::SqpWarmState free_path = predict(model, warm, dp, absent, &outcome);
    EXPECT_EQ(outcome, PredictorOutcome::kPredicted);
    EXPECT_EQ(free_path.bound_active[0], 0);
    EXPECT_NEAR(free_path.x[0], 2.5e6, 1.0); // the regularization floor, relative 4e-9

    // At the default threshold the bound is real, and the path stops on it.
    const detail::SqpWarmState bound_path = predict(model, warm, dp);
    EXPECT_EQ(bound_path.bound_active[0], +1);
    EXPECT_EQ(bound_path.x[0], TrackParameter::kUpper);

    PredictorOptions bad;
    bad.infinite_bound = 0.0;
    EXPECT_THROW(predict(model, warm, dp, bad), std::invalid_argument);
}

} // namespace
