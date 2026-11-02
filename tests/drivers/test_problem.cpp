// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-2 B2: one problem type. The pins docs/notes/2026-11-m8-ap1-api-review.md
// section 13 names for group B2:
//   * one Problem on both engines, and IPM alternation counted;
//   * the EX-1 in-place parameter sweep (friction #23) correct on both engines;
//   * an unchanged-bounds re-solve counter-identical to the internal route's re-solve;
//   * a changed fixed set re-laid and counted, and a prior WarmStartData refused as an
//     identity mismatch (a value-only bound change keeps it);
//   * a dimension change, a structure change and a row-kind flip refused by name;
//   * the per-row lambda/g map on every row kind;
//   * has_hessian() == false refused by name where the Hessian is read;
//   * the census, and a moved-from Problem.

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include <Eigen/Core>

#include "drivers/support/hs071_problem.h"
#include "hven/detail/drivers/result_access.h"
#include "hven/detail/model/make_nlp_program.h"
#include "hven/detail/model/nlp_problem_model.h"
#include "hven/detail/model/non_linear_program.h"
#include "hven/detail/model/problem_access.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

namespace {

using namespace hven::solvers;
using hven::ConstVecRef;
using hven::Index;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::Vec;
using hven::VecRef;

IpmSolver quiet_ipm() {
    IpmOptions options;
    options.common.print_level = 10;
    return IpmSolver(options);
}

SqpSolver quiet_sqp(SqpHessianMode mode = SqpHessianMode::kExact) {
    SqpOptions options;
    options.hessian_mode = mode;
    return SqpSolver(options);
}

Vec hs071_start() {
    Vec x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

std::shared_ptr<Nlp> hs071() { return std::make_shared<hven_drivers_tests::Hs071Problem>(); }

bool bitwise_equal(const Vec &a, const Vec &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (Index i = 0; i < a.size(); ++i) {
        if (std::memcmp(&a[i], &b[i], sizeof(double)) != 0) {
            return false;
        }
    }
    return true;
}

void expect_same_ipm(const IpmResult &a, const IpmResult &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.iterations, b.iterations);
    EXPECT_EQ(a.kkt_analyses_this_call, b.kkt_analyses_this_call);
    EXPECT_EQ(a.kkt_factor_counters.factorize_count, b.kkt_factor_counters.factorize_count);
    EXPECT_EQ(a.kkt_factor_counters.solve_count, b.kkt_factor_counters.solve_count);
    EXPECT_TRUE(bitwise_equal(a.x, b.x));
    EXPECT_EQ(std::memcmp(&a.f, &b.f, sizeof(double)), 0);
}

void expect_same_sqp(const SqpResult &a, const SqpResult &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.iterations, b.iterations);
    EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
    EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
    EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
    EXPECT_EQ(a.counters.eval_hess_calls, b.counters.eval_hess_calls);
    EXPECT_TRUE(bitwise_equal(a.x, b.x));
    EXPECT_EQ(std::memcmp(&a.f, &b.f, sizeof(double)), 0);
}

// ---------------------------------------------------------------------------
// One Problem, both engines; IPM alternation
// ---------------------------------------------------------------------------

TEST(Problem, OneProblemIsSolvedByBothEngines) {
    Problem problem(hs071());
    IpmSolver ipm = quiet_ipm();
    SqpSolver sqp = quiet_sqp();

    const IpmResult a = ipm.solve(problem, hs071_start());
    const SqpResult b = sqp.solve(problem, hs071_start());
    ASSERT_EQ(a.status, SolveStatus::kOptimal);
    ASSERT_EQ(b.status, SolveStatus::kOptimal);
    EXPECT_NEAR(a.f, 17.0140172, 1e-6);
    EXPECT_NEAR(b.f, 17.0140172, 1e-6);
    ASSERT_EQ(a.lambda.size(), 2);
    ASSERT_EQ(b.lambda.size(), 2);
    EXPECT_NEAR(a.lambda[0], b.lambda[0], 1e-5);
    EXPECT_NEAR(a.lambda[1], b.lambda[1], 1e-5);
    ASSERT_EQ(a.g.size(), 2);
    ASSERT_EQ(b.g.size(), 2);

    // An SQP solve between two IPM solves does not re-lay the interior-point
    // transcription: the second IPM solve reuses its analysis.
    const IpmResult c = ipm.solve(problem, hs071_start());
    EXPECT_EQ(a.kkt_analyses_this_call, 1);
    EXPECT_EQ(c.kkt_analyses_this_call, 0);
    EXPECT_TRUE(ipm.kkt_pattern_is_analyzed(problem));
}

TEST(Problem, IpmAlternationOnOneProblemReanalysesAndIsCounted) {
    Problem problem(hs071());
    IpmSolver a = quiet_ipm();
    IpmSolver b = quiet_ipm();
    EXPECT_FALSE(a.kkt_pattern_is_analyzed(problem));
    EXPECT_EQ(a.solve(problem, hs071_start()).kkt_analyses_this_call, 1);
    EXPECT_EQ(b.solve(problem, hs071_start()).kkt_analyses_this_call, 1);
    // b laid the analysis last, so a's next solve re-analyses ...
    EXPECT_FALSE(a.kkt_pattern_is_analyzed(problem));
    EXPECT_EQ(a.solve(problem, hs071_start()).kkt_analyses_this_call, 1);
    // ... and a re-solve by the same solver does not.
    EXPECT_EQ(a.solve(problem, hs071_start()).kkt_analyses_this_call, 0);
}

// ---------------------------------------------------------------------------
// The public route against the internal route
// ---------------------------------------------------------------------------

TEST(Problem, UnchangedBoundsResolveIsCounterIdenticalToTheInternalRoute) {
    const std::shared_ptr<Nlp> nlp = hs071();

    // IPM: the internal route is make_nlp_program, solved twice on one solver.
    {
        const auto program = make_nlp_program(nlp);
        IpmSolver internal = quiet_ipm();
        const IpmResult i1 = internal.solve(detail::problem_ref(*program), hs071_start());
        const IpmResult i2 = internal.solve(detail::problem_ref(*program), hs071_start());

        Problem problem(nlp);
        IpmSolver pub = quiet_ipm();
        const IpmResult p1 = pub.solve(problem, hs071_start());
        const IpmResult p2 = pub.solve(problem, hs071_start());
        expect_same_ipm(i1, p1);
        expect_same_ipm(i2, p2);
        EXPECT_EQ(p2.kkt_analyses_this_call, 0);
    }
    // SQP: the internal route is NlpProblemModel, solved twice on one solver.
    {
        NlpProblemModel model(nlp);
        SqpSolver internal = quiet_sqp();
        const SqpResult i1 = internal.solve(detail::problem_ref(model), hs071_start());
        const SqpResult i2 = internal.solve(detail::problem_ref(model), hs071_start());

        Problem problem(nlp);
        SqpSolver pub = quiet_sqp();
        const SqpResult p1 = pub.solve(problem, hs071_start());
        const SqpResult p2 = pub.solve(problem, hs071_start());
        expect_same_sqp(i1, p1);
        expect_same_sqp(i2, p2);
    }
}

// ---------------------------------------------------------------------------
// The EX-1 in-place sweep (friction #23)
// ---------------------------------------------------------------------------

// min (x0 - 2p)^2 + (x1 - p)^2  s.t.  |x|^2 <= 1 + p/2; the row bound moves with p.
struct Disc final : Nlp {
    double p = 0.0;
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 1, .jacobian_nonzeros = 2, .hessian_nonzeros = 2};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << -kInfinity;
        gu << 1.0 + 0.5 * p;
    }
    double eval_f(ConstVecRef x) const override {
        return std::pow(x[0] - 2 * p, 2) + std::pow(x[1] - p, 2);
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << 2 * (x[0] - 2 * p), 2 * (x[1] - p);
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x.squaredNorm(); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v = 2.0 * x; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_hessian(ConstVecRef, double obj_factor, ConstVecRef lambda, VecRef v) const override {
        v.setConstant(2.0 * obj_factor + 2.0 * lambda[0]);
    }
};

double disc_objective(double p) {
    const double gap = p * std::sqrt(5.0) - std::sqrt(1.0 + 0.5 * p);
    return gap > 0 ? gap * gap : 0.0;
}

TEST(Problem, InPlaceParameterSweepIsCorrectOnBothEngines) {
    const auto disc = std::make_shared<Disc>();
    Problem problem(disc);
    IpmSolver ipm = quiet_ipm();
    SqpSolver sqp = quiet_sqp();
    // The rebuild-per-parameter route, on solvers with the same history: the only
    // difference between the two sweeps is one Problem against a fresh one per p.
    IpmSolver rebuilt_ipm = quiet_ipm();
    SqpSolver rebuilt_sqp = quiet_sqp();
    const Vec x0 = Vec::Zero(2);
    for (const double p : {0.2, 0.4, 0.6, 0.8, 1.0}) {
        disc->p = p;
        const IpmResult a = ipm.solve(problem, x0);
        const SqpResult b = sqp.solve(problem, x0);
        EXPECT_EQ(a.status, SolveStatus::kOptimal) << "p=" << p;
        EXPECT_EQ(b.status, SolveStatus::kOptimal) << "p=" << p;
        EXPECT_NEAR(a.f, disc_objective(p), 1e-6) << "p=" << p;
        EXPECT_NEAR(b.f, disc_objective(p), 1e-6) << "p=" << p;

        auto fresh_disc = std::make_shared<Disc>();
        fresh_disc->p = p;
        Problem fresh(fresh_disc);
        expect_same_ipm(rebuilt_ipm.solve(fresh, x0), a);
        expect_same_sqp(rebuilt_sqp.solve(fresh, x0), b);
    }
}

// ---------------------------------------------------------------------------
// Bound changes: re-lay, count, warm-start identity
// ---------------------------------------------------------------------------

// min sum (x_i - 1)^2 + x0*x1  s.t.  x0 + x1 + x2 >= 1; x1's bounds are a member.
struct Box3 final : Nlp {
    double x1_lower = -10.0;
    double x1_upper = 10.0;
    NlpDimensions dimensions() const override {
        return {.n = 3, .m = 1, .jacobian_nonzeros = 3, .hessian_nonzeros = 4};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl << -10.0, x1_lower, -10.0;
        xu << 10.0, x1_upper, 10.0;
        gl << 1.0;
        gu << kInfinity;
    }
    double eval_f(ConstVecRef x) const override {
        return (x.array() - 1.0).square().sum() + x[0] * x[1];
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g = 2.0 * (x.array() - 1.0).matrix();
        g[0] += x[1];
        g[1] += x[0];
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x.sum(); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0, 0;
        c << 0, 1, 2;
    }
    void eval_jacobian(ConstVecRef, VecRef v) const override { v.setOnes(); }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 1, 2;
        c << 0, 0, 1, 2;
    }
    void eval_hessian(ConstVecRef, double obj_factor, ConstVecRef, VecRef v) const override {
        v << 2.0 * obj_factor, obj_factor, 2.0 * obj_factor, 2.0 * obj_factor;
    }
};

TEST(Problem, AChangedFixedSetIsReLaidCountedAndRefusesAPriorWarmStart) {
    const auto box = std::make_shared<Box3>();
    Problem problem(box);
    IpmSolver ipm = quiet_ipm();
    const Vec x0 = Vec::Constant(3, 0.5);

    const IpmResult free_solve = ipm.solve(problem, x0);
    ASSERT_EQ(free_solve.status, SolveStatus::kOptimal);
    const std::optional<WarmStartData> payload = free_solve.export_warm_start();
    ASSERT_TRUE(payload.has_value());
    EXPECT_EQ(problem.statistics().boxed_variables, 3);
    EXPECT_EQ(problem.statistics().fixed_variables, 0);

    // A value-only change (x1 still boxed, both bounds finite): re-laid and
    // counted, and the payload's declaration still matches.
    box->x1_upper = 5.0;
    const IpmResult moved = ipm.solve(problem, x0, *payload);
    EXPECT_EQ(moved.status, SolveStatus::kOptimal);
    EXPECT_EQ(moved.kkt_analyses_this_call, 1);
    EXPECT_FALSE(ipm.kkt_pattern_is_analyzed(Problem(hs071())));

    // x1 fixed: the elimination is re-laid and counted, exactly as a fresh Problem
    // at these bounds counts it ...
    box->x1_lower = box->x1_upper = 0.25;
    const IpmResult fixed = ipm.solve(problem, x0);
    ASSERT_EQ(fixed.status, SolveStatus::kOptimal);
    {
        auto fresh_box = std::make_shared<Box3>();
        fresh_box->x1_lower = fresh_box->x1_upper = 0.25;
        Problem fresh(fresh_box);
        IpmSolver fresh_ipm = quiet_ipm();
        const IpmResult reference = fresh_ipm.solve(fresh, x0);
        EXPECT_GE(fixed.kkt_analyses_this_call, 1);
        EXPECT_EQ(fixed.kkt_analyses_this_call, reference.kkt_analyses_this_call);
        EXPECT_TRUE(bitwise_equal(fixed.x, reference.x));
    }
    EXPECT_EQ(fixed.x[1], 0.25);
    EXPECT_EQ(problem.statistics().fixed_variables, 1);
    // ... and the payload taken before it is an identity mismatch.
    EXPECT_THROW((void)ipm.solve(problem, x0, *payload), std::invalid_argument);
    SqpSolver sqp = quiet_sqp();
    EXPECT_THROW((void)sqp.solve(problem, x0, *payload), std::invalid_argument);

    // Unchanged again: no re-lay.
    EXPECT_EQ(ipm.solve(problem, x0).kkt_analyses_this_call, 0);
}

// ---------------------------------------------------------------------------
// Refusals by name
// ---------------------------------------------------------------------------

struct Mutable final : Nlp {
    Index n = 2;
    bool swap_jacobian = false;
    double g_upper = 1.0;
    NlpDimensions dimensions() const override {
        return {.n = n, .m = 1, .jacobian_nonzeros = 2, .hessian_nonzeros = 2};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << -kInfinity;
        gu << g_upper;
    }
    double eval_f(ConstVecRef x) const override { return (x.array() - 2.0).square().sum(); }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g = 2.0 * (x.array() - 2.0).matrix();
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x.head(2).squaredNorm(); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        if (swap_jacobian) {
            c << 1, 0;
        } else {
            c << 0, 1;
        }
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v = 2.0 * x.head(2); }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_hessian(ConstVecRef, double obj_factor, ConstVecRef lambda, VecRef v) const override {
        v.setConstant(2.0 * obj_factor + 2.0 * lambda[0]);
    }
    std::string name() const override { return "mutable"; }
};

template <class Fn> std::string refusal(Fn &&fn) {
    try {
        fn();
    } catch (const std::invalid_argument &e) {
        return e.what();
    }
    return {};
}

TEST(Problem, DimensionStructureAndRowKindChangesAreRefusedByName) {
    const auto nlp = std::make_shared<Mutable>();
    Problem problem(nlp);
    IpmSolver ipm = quiet_ipm();
    SqpSolver sqp = quiet_sqp();
    const Vec x0 = Vec::Zero(2);
    ASSERT_EQ(ipm.solve(problem, x0).status, SolveStatus::kOptimal);

    nlp->swap_jacobian = true;
    for (const std::string &m : {refusal([&] { (void)ipm.solve(problem, x0); }),
                                 refusal([&] { (void)sqp.solve(problem, x0); })}) {
        EXPECT_NE(m.find("'mutable'"), std::string::npos) << m;
        EXPECT_NE(m.find("Jacobian structure changed"), std::string::npos) << m;
    }
    nlp->swap_jacobian = false;

    nlp->n = 3;
    for (const std::string &m : {refusal([&] { (void)ipm.solve(problem, x0); }),
                                 refusal([&] { (void)sqp.solve(problem, x0); })}) {
        EXPECT_NE(m.find("dimensions changed"), std::string::npos) << m;
    }
    nlp->n = 2;

    // Upper-bounded -> free: a row-kind flip.
    nlp->g_upper = kInfinity;
    for (const std::string &m : {refusal([&] { (void)ipm.solve(problem, x0); }),
                                 refusal([&] { (void)sqp.solve(problem, x0); })}) {
        EXPECT_NE(m.find("row 0 changed kind"), std::string::npos) << m;
        EXPECT_NE(m.find("from upper-bounded to free"), std::string::npos) << m;
    }

    // A refused solve committed nothing: restored, the problem solves as before
    // and reuses its analysis.
    nlp->g_upper = 1.0;
    const IpmResult again = ipm.solve(problem, x0);
    EXPECT_EQ(again.status, SolveStatus::kOptimal);
    EXPECT_EQ(again.kkt_analyses_this_call, 0);
    EXPECT_EQ(sqp.solve(problem, x0).status, SolveStatus::kOptimal);
}

// ---------------------------------------------------------------------------
// lambda and g in the problem's row space, every row kind
// ---------------------------------------------------------------------------

// Five rows, one of each kind, over x in R^3:
//   row 0  x0 + x1 + x2      = 1      equality
//   row 1  x0 - x1          <= -0.5   upper-bounded (active)
//   row 2  x2^2             >= 1      lower-bounded (active)
//   row 3  x0 + 2 x2      in [-1, 3]  range
//   row 4  x1 * x2           free
struct EveryRowKind final : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 3, .m = 5, .jacobian_nonzeros = 10, .hessian_nonzeros = 5};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-5.0);
        xu.setConstant(5.0);
        gl << 1.0, -kInfinity, 1.0, -1.0, -kInfinity;
        gu << 1.0, -0.5, kInfinity, 3.0, kInfinity;
    }
    double eval_f(ConstVecRef x) const override {
        return x[0] * x[0] + 2.0 * x[1] * x[1] + 0.5 * x[2] * x[2];
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override { g << 2 * x[0], 4 * x[1], x[2]; }
    void eval_g(ConstVecRef x, VecRef g) const override {
        g << x.sum(), x[0] - x[1], x[2] * x[2], x[0] + 2 * x[2], x[1] * x[2];
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0, 0, 1, 1, 2, 3, 3, 4, 4;
        c << 0, 1, 2, 0, 1, 2, 0, 2, 1, 2;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override {
        v << 1, 1, 1, 1, -1, 2 * x[2], 1, 2, x[2], x[1];
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 2, 2, 2;
        c << 0, 1, 2, 1, 2;
    }
    void eval_hessian(ConstVecRef, double obj_factor, ConstVecRef lambda, VecRef v) const override {
        // (2,2) is declared twice; duplicates are summed.
        v << 2 * obj_factor, 4 * obj_factor, obj_factor, lambda[4], 2 * lambda[2];
    }
};

// The full Jacobian, dense, for the stationarity check (row 4 is d(x1 x2)).
Eigen::MatrixXd every_row_kind_jacobian(const Vec &x) {
    Eigen::MatrixXd J = Eigen::MatrixXd::Zero(5, 3);
    J.row(0) << 1, 1, 1;
    J.row(1) << 1, -1, 0;
    J.row(2) << 0, 0, 2 * x[2];
    J.row(3) << 1, 0, 2;
    J.row(4) << 0, x[2], x[1];
    return J;
}

template <class Result>
void check_row_space(const Problem &problem, const Result &r, const EveryRowKind &nlp, double tol) {
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_EQ(r.lambda.size(), 5);
    ASSERT_EQ(r.g.size(), 5);
    // g is g(x), exactly.
    Vec g(5);
    nlp.eval_g(r.x, g);
    EXPECT_TRUE(bitwise_equal(r.g, g));
    // A free row carries no multiplier.
    EXPECT_EQ(r.lambda[4], 0.0);
    // Ipopt's sign: grad f + J^T lambda - z = 0.
    Vec grad(3);
    nlp.eval_grad_f(r.x, grad);
    const Vec residual = grad + every_row_kind_jacobian(r.x).transpose() * r.lambda - r.z;
    EXPECT_LT(residual.lpNorm<Eigen::Infinity>(), tol);
    // Active upper bound: lambda >= 0; active lower bound: lambda <= 0.
    EXPECT_GT(r.lambda[1], 0.0);
    EXPECT_LT(r.lambda[2], 0.0);
    // The map round-trips: lambda is the composition of the engine's own rows, and
    // splitting it and composing again returns it.
    const NlpProblemModel *model = detail::ProblemAccess::nlp_model(problem);
    ASSERT_NE(model, nullptr);
    const auto &rows = detail::engine_rows(r);
    EXPECT_TRUE(
        bitwise_equal(r.lambda, model->compose_user_multipliers(rows.lambda_e, rows.lambda_i)));
    Vec le, li;
    model->split_user_multipliers(r.lambda, le, li);
    EXPECT_TRUE(bitwise_equal(model->compose_user_multipliers(le, li), r.lambda));
}

TEST(Problem, LambdaAndGAreInTheProblemsRowSpaceOnEveryRowKind) {
    const auto nlp = std::make_shared<EveryRowKind>();
    Problem problem(nlp);
    const ProblemStatistics &stats = problem.statistics();
    EXPECT_EQ(stats.constraints, 5);
    EXPECT_EQ(stats.equality_rows, 1);
    EXPECT_EQ(stats.upper_bounded_rows, 1);
    EXPECT_EQ(stats.lower_bounded_rows, 1);
    EXPECT_EQ(stats.range_rows, 1);
    EXPECT_EQ(stats.free_rows, 1);
    EXPECT_EQ(stats.boxed_variables, 3);

    const Vec x0{{0.0, 0.3, 1.2}};
    IpmSolver ipm = quiet_ipm();
    check_row_space(problem, ipm.solve(problem, x0), *nlp, 1e-6);
    SqpSolver sqp = quiet_sqp();
    check_row_space(problem, sqp.solve(problem, x0), *nlp, 1e-6);
}

// ---------------------------------------------------------------------------
// An optional Hessian
// ---------------------------------------------------------------------------

struct NoHessian final : Nlp {
    NlpDimensions dimensions() const override { return {.n = 2}; }
    void bounds(VecRef xl, VecRef xu, VecRef, VecRef) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
    }
    double eval_f(ConstVecRef x) const override { return (x.array() - 3.0).square().sum(); }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g = 2.0 * (x.array() - 3.0).matrix();
    }
    bool has_hessian() const override { return false; }
    std::string name() const override { return "first-order"; }
};

TEST(Problem, AProblemWithoutAHessianIsRefusedByNameWhereTheHessianIsRead) {
    Problem problem(std::make_shared<NoHessian>());
    const Vec x0 = Vec::Zero(2);
    IpmSolver ipm = quiet_ipm();
    const std::string ipm_refusal = refusal([&] { (void)ipm.solve(problem, x0); });
    EXPECT_NE(ipm_refusal.find("has_hessian() == false"), std::string::npos) << ipm_refusal;
    EXPECT_NE(ipm_refusal.find("'first-order'"), std::string::npos) << ipm_refusal;
    SqpSolver exact = quiet_sqp(SqpHessianMode::kExact);
    const std::string sqp_refusal = refusal([&] { (void)exact.solve(problem, x0); });
    EXPECT_NE(sqp_refusal.find("SqpHessianMode::kExact"), std::string::npos) << sqp_refusal;

    SqpSolver qn = quiet_sqp(SqpHessianMode::kLbfgsDamped);
    const SqpResult r = qn.solve(problem, x0);
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_NEAR(r.x[0], 3.0, 1e-6);
    EXPECT_EQ(r.counters.eval_hess_calls, 0);
    EXPECT_EQ(r.lambda.size(), 0);
    EXPECT_EQ(r.g.size(), 0);
}

// ---------------------------------------------------------------------------
// Construction, census, moved-from
// ---------------------------------------------------------------------------

TEST(Problem, ConstructionValidatesAndCountsTheDeclaration) {
    EXPECT_THROW(Problem(std::shared_ptr<Nlp>()), std::invalid_argument);
    Problem problem(hs071());
    EXPECT_EQ(problem.num_variables(), 4);
    EXPECT_EQ(problem.num_constraints(), 2);
    EXPECT_EQ(problem.partitions(), 1);
    const ProblemStatistics &s = problem.statistics();
    EXPECT_EQ(s.variables, 4);
    EXPECT_EQ(s.boxed_variables, 4);
    EXPECT_EQ(s.equality_rows, 1);
    EXPECT_EQ(s.lower_bounded_rows, 1);
    EXPECT_EQ(s.free_variables + s.lower_bounded_variables + s.upper_bounded_variables +
                  s.fixed_variables,
              0);

    // A Hessian entry above the diagonal is refused at construction.
    struct Upper final : Nlp {
        NlpDimensions dimensions() const override { return {.n = 2, .hessian_nonzeros = 1}; }
        void bounds(VecRef xl, VecRef xu, VecRef, VecRef) const override {
            xl.setConstant(-kInfinity);
            xu.setConstant(kInfinity);
        }
        double eval_f(ConstVecRef) const override { return 0.0; }
        void eval_grad_f(ConstVecRef, VecRef g) const override { g.setZero(); }
        void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
            r << 0;
            c << 1;
        }
    };
    EXPECT_THROW(Problem(std::make_shared<Upper>()), std::invalid_argument);
}

TEST(Problem, AMovedFromProblemReportsEmptyAndRefusesASolve) {
    Problem source(hs071());
    Problem target(std::move(source));
    EXPECT_EQ(target.num_variables(), 4);
    // NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    EXPECT_EQ(source.num_variables(), 0);
    EXPECT_EQ(source.statistics().variables, 0);
    IpmSolver ipm = quiet_ipm();
    EXPECT_THROW((void)ipm.solve(source, hs071_start()), std::logic_error);
    SqpSolver sqp = quiet_sqp();
    EXPECT_THROW((void)sqp.solve(source, hs071_start()), std::logic_error);
    source = std::move(target);
    // NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    EXPECT_EQ(ipm.solve(source, hs071_start()).status, SolveStatus::kOptimal);
}

} // namespace
