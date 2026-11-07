// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-2 B2: one problem type. The pins docs/notes/2026-11-m8-ap1-api-review.md
// section 13 names for group B2:
//   * one Problem on both engines, and IPM alternation counted;
//   * the EX-1 in-place parameter sweep (friction #23) correct on both engines;
//   * an unchanged-bounds re-solve counter-identical to the internal route's re-solve;
//   * a value-only bound change adopted in place: no re-analysis, the fresh-Problem
//     solution, and a prior warm start kept (both engines);
//   * a changed fixed set re-laid and counted, and a prior WarmStart refused as an
//     identity mismatch;
//   * a dimension change, a structure change and a row-kind flip refused by name;
//   * the per-row lambda/g map on every row kind, an active range row on both sides,
//     and a pieces problem's [c_E | c_I] row space;
//   * report() makes no callback;
//   * has_hessian() == false refused by name where the Hessian is read;
//   * the census, and a moved-from Problem.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <Eigen/Core>

#include "drivers/support/hs071_problem.h"
#include "hven/detail/drivers/result_access.h"
#include "hven/detail/model/make_nlp_program.h"
#include "hven/detail/model/nlp_problem_model.h"
#include "hven/detail/model/non_linear_program.h"
#include "hven/detail/model/problem_access.h"
#include "hven/detail/warmstart/sqp_warm_access.h"
#include "hven/detail/warmstart/warm_start_access.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/model/assembly_declaration.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"
#include "hven/warmstart/ipm_polish_extension.h"

namespace {
using hven::detail::SqpWarmAccess;
} // namespace

namespace {

using namespace hven;
using namespace hven::detail;
using hven::ConstVecRef;
using hven::Index;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::Vec;
using hven::VecRef;

IpmSolver
quiet_ipm(IpmFixedVariableTreatment treatment = IpmFixedVariableTreatment::kMakeParameter) {
    IpmOptions options;
    options.common.verbosity = hven::Verbosity::kSilent;
    options.fixed_variable_treatment = treatment;
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

// g with an unmeasured (NaN) row: equal iff the sizes match and every entry is NaN in
// both or bit-identical.
bool bitwise_equal_nan_aware(const Vec &a, const Vec &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (Index i = 0; i < a.size(); ++i) {
        if (std::isnan(a[i]) && std::isnan(b[i])) {
            continue;
        }
        if (std::memcmp(&a[i], &b[i], sizeof(double)) != 0) {
            return false;
        }
    }
    return true;
}

// The KKT factor's counters run from the solver's last transcription of a program,
// so a solve on a program already transcribed reports them cumulatively. This turns
// each result into its own call's share, per solver; @p new_program says the solve
// transcribed a program it had not seen (a new Problem, or a re-laid one).
class FactorCalls {
  public:
    std::pair<Index, Index> of(const IpmResult &r, bool new_program) {
        if (new_program) {
            factorize_ = solve_ = 0;
        }
        const std::pair<Index, Index> calls{r.kkt_factor_counters.factorize_count - factorize_,
                                            r.kkt_factor_counters.solve_count - solve_};
        factorize_ = r.kkt_factor_counters.factorize_count;
        solve_ = r.kkt_factor_counters.solve_count;
        return calls;
    }

  private:
    Index factorize_ = 0;
    Index solve_ = 0;
};

// Everything but the analysis: a value-only bound change keeps the analysis a fresh
// Problem has to lay. @p a_calls and @p b_calls are each solve's own factor calls.
void expect_same_ipm_solution(const IpmResult &a, std::pair<Index, Index> a_calls,
                              const IpmResult &b, std::pair<Index, Index> b_calls) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.iterations, b.iterations);
    EXPECT_EQ(a_calls, b_calls);
    EXPECT_TRUE(bitwise_equal(a.x, b.x));
    EXPECT_EQ(std::memcmp(&a.f, &b.f, sizeof(double)), 0);
    EXPECT_TRUE(bitwise_equal(a.lambda, b.lambda));
    EXPECT_TRUE(bitwise_equal(a.z, b.z));
    EXPECT_TRUE(bitwise_equal_nan_aware(a.g, b.g));
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

// Status, counters, x and f: all that two ROUTES (the internal model and the public
// Problem, whose result is in the problem's own row space) share.
void expect_same_sqp_core(const SqpResult &a, const SqpResult &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.iterations, b.iterations);
    EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
    EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
    EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
    EXPECT_EQ(a.counters.eval_hess_calls, b.counters.eval_hess_calls);
    EXPECT_TRUE(bitwise_equal(a.x, b.x));
    EXPECT_EQ(std::memcmp(&a.f, &b.f, sizeof(double)), 0);
}

// Two solves through the SAME route: the multipliers, bound multipliers and row values
// are bitwise equal too (g NaN-aware for an unmeasured row).
void expect_same_sqp(const SqpResult &a, const SqpResult &b) {
    expect_same_sqp_core(a, b);
    EXPECT_TRUE(bitwise_equal(a.lambda, b.lambda));
    EXPECT_TRUE(bitwise_equal(a.z, b.z));
    EXPECT_TRUE(bitwise_equal_nan_aware(a.g, b.g));
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
        expect_same_sqp_core(i1, p1);
        expect_same_sqp_core(i2, p2);
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
    bool first = true;
    FactorCalls ipm_calls, rebuilt_calls;
    for (const double p : {0.2, 0.4, 0.6, 0.8, 1.0}) {
        disc->p = p;
        const IpmResult a = ipm.solve(problem, x0);
        const SqpResult b = sqp.solve(problem, x0);
        EXPECT_EQ(a.status, SolveStatus::kOptimal) << "p=" << p;
        EXPECT_EQ(b.status, SolveStatus::kOptimal) << "p=" << p;
        EXPECT_NEAR(a.f, disc_objective(p), 1e-6) << "p=" << p;
        EXPECT_NEAR(b.f, disc_objective(p), 1e-6) << "p=" << p;
        // p moves the row bound's value only, so only the first solve analyses.
        EXPECT_EQ(a.kkt_analyses_this_call, first ? 1 : 0) << "p=" << p;
        const bool first_p = first;
        first = false;

        auto fresh_disc = std::make_shared<Disc>();
        fresh_disc->p = p;
        Problem fresh(fresh_disc);
        const IpmResult rebuilt = rebuilt_ipm.solve(fresh, x0);
        EXPECT_EQ(rebuilt.kkt_analyses_this_call, 1) << "p=" << p;
        expect_same_ipm_solution(rebuilt, rebuilt_calls.of(rebuilt, true), a,
                                 ipm_calls.of(a, first_p));
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
    const WarmStart payload = free_solve.warm_start;
    ASSERT_TRUE(!payload.empty());
    EXPECT_EQ(problem.statistics().boxed_variables, 3);
    EXPECT_EQ(problem.statistics().fixed_variables, 0);

    // A value-only change (x1 still boxed, both bounds finite): the payload's
    // declaration still matches (the value-only pins themselves are below).
    box->x1_upper = 5.0;
    const IpmResult moved = ipm.solve(problem, x0, payload);
    EXPECT_EQ(moved.status, SolveStatus::kOptimal);
    EXPECT_EQ(moved.kkt_analyses_this_call, 0);
    EXPECT_TRUE(ipm.kkt_pattern_is_analyzed(problem));

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
    EXPECT_THROW((void)ipm.solve(problem, x0, payload), std::invalid_argument);
    SqpSolver sqp = quiet_sqp();
    EXPECT_THROW((void)sqp.solve(problem, x0, payload), std::invalid_argument);

    // Unchanged again: no re-lay.
    EXPECT_EQ(ipm.solve(problem, x0).kkt_analyses_this_call, 0);
}

// Pins the IPM declared-space seam contract: under MakeConstraint the engine strips its
// internal fixing rows, so g and lambda are me-long (the user rows alone) and g matches eval_g.
TEST(Problem, ReportsTheUserRowSpaceUnderMakeConstraintWithAFixedVariable) {
    const auto box = std::make_shared<Box3>();
    box->x1_lower = box->x1_upper = 0.25;
    Problem problem(box);
    IpmSolver ipm = quiet_ipm(IpmFixedVariableTreatment::kMakeConstraint);
    const IpmResult r = ipm.solve(problem, Vec::Constant(3, 0.5));
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_EQ(r.g.size(), 1);
    ASSERT_EQ(r.lambda.size(), 1);
    Vec g(1);
    box->eval_g(r.x, g);
    EXPECT_NEAR(r.g[0], g[0], 1e-12 * std::max(1.0, std::abs(g[0])));
}

// The seed of an export carries the key the export carries, also when a fixed
// variable is held by an internal fixing row.
TEST(Problem, SeedKeyMatchesTheExportKeyUnderMakeConstraintWithAFixedVariable) {
    const auto box = std::make_shared<Box3>();
    box->x1_lower = box->x1_upper = 0.25;
    Problem problem(box);
    IpmSolver ipm = quiet_ipm(IpmFixedVariableTreatment::kMakeConstraint);
    const IpmResult r = ipm.solve(problem, Vec::Constant(3, 0.5));
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_FALSE(r.warm_start.empty());
    const WarmStart seed = WarmStart::seed(problem, r.x, r.lambda, r.z);
    EXPECT_TRUE(seed.key() == r.warm_start.key());
}

// A value-only change -- every variable keeps which sides are finite and whether
// it is fixed -- is adopted in place: no re-analysis, and the solution a fresh
// Problem at the new bounds returns, BITWISE. Bitwise because nothing about the
// arithmetic differs: the same KKT pattern, the same fill-reducing ordering (a
// deterministic function of the pattern), the same bound values entering the same
// barrier terms; the only thing skipped is re-deriving that ordering.
TEST(Problem, AValueOnlyBoundChangeIsAdoptedInPlaceWithoutReanalysis) {
    const Vec x0 = Vec::Constant(3, 0.5);
    for (const IpmFixedVariableTreatment treatment :
         {IpmFixedVariableTreatment::kMakeParameter, IpmFixedVariableTreatment::kMakeConstraint,
          IpmFixedVariableTreatment::kRelaxBounds}) {
        SCOPED_TRACE(static_cast<int>(treatment));
        const auto box = std::make_shared<Box3>();
        Problem problem(box);
        IpmSolver ipm = quiet_ipm(treatment);
        FactorCalls calls;
        const IpmResult first = ipm.solve(problem, x0);
        ASSERT_EQ(first.kkt_analyses_this_call, 1);
        (void)calls.of(first, true);

        const auto expect_reference = [&](const IpmResult &r, double lower, double upper) {
            auto fresh_box = std::make_shared<Box3>();
            fresh_box->x1_lower = lower;
            fresh_box->x1_upper = upper;
            Problem fresh(fresh_box);
            IpmSolver fresh_ipm = quiet_ipm(treatment);
            FactorCalls fresh_calls;
            const IpmResult ref = fresh_ipm.solve(fresh, x0);
            expect_same_ipm_solution(r, calls.of(r, false), ref, fresh_calls.of(ref, true));
        };

        // Boxed to boxed: x1's upper bound becomes active.
        box->x1_lower = -2.0;
        box->x1_upper = 0.5;
        const IpmResult moved = ipm.solve(problem, x0);
        ASSERT_EQ(moved.status, SolveStatus::kOptimal);
        EXPECT_EQ(moved.kkt_analyses_this_call, 0);
        EXPECT_TRUE(ipm.kkt_pattern_is_analyzed(problem));
        EXPECT_NEAR(moved.x[1], 0.5, 1e-6);
        expect_reference(moved, -2.0, 0.5);

        // Boxed to fixed is a structure change: re-laid and counted.
        box->x1_lower = box->x1_upper = 0.25;
        const IpmResult relaid = ipm.solve(problem, x0);
        EXPECT_GE(relaid.kkt_analyses_this_call, 1);
        (void)calls.of(relaid, true);

        // Fixed to fixed at a new value. MakeParameter pins the eliminated value and
        // RelaxBounds moves a relaxed pair, both in place; MakeConstraint holds the
        // value in its fixing row, which is re-installed (counted) -- the stated
        // exception.
        box->x1_lower = box->x1_upper = 0.75;
        const IpmResult refixed = ipm.solve(problem, x0);
        ASSERT_EQ(refixed.status, SolveStatus::kOptimal);
        EXPECT_EQ(refixed.kkt_analyses_this_call,
                  treatment == IpmFixedVariableTreatment::kMakeConstraint ? 1 : 0);
        expect_reference(refixed, 0.75, 0.75);
        if (treatment == IpmFixedVariableTreatment::kMakeParameter) {
            EXPECT_EQ(refixed.x[1], 0.75);
        }
        EXPECT_EQ(problem.statistics().fixed_variables, 1);
    }
}

// The SQP side of a value-only change: the model adopts the values in place, a cold
// solve is the fresh Problem's bitwise, the shared payload taken before is accepted,
// and the engine-native warm start resolves above kCold.
TEST(Problem, AValueOnlyBoundChangeKeepsTheSqpWarmStarts) {
    const auto box = std::make_shared<Box3>();
    Problem problem(box);
    SqpSolver sqp = quiet_sqp();
    const Vec x0 = Vec::Constant(3, 0.5);
    const SqpResult first = sqp.solve(problem, x0);
    ASSERT_EQ(first.status, SolveStatus::kOptimal);
    const WarmStart payload = first.warm_start;
    ASSERT_TRUE(!payload.empty());
    const hven::detail::SqpWarmState native = SqpWarmAccess::state(first.sqp_warm_start);
    ASSERT_TRUE(native.valid);

    box->x1_upper = 0.5; // the unconstrained x1 = 2/3 is cut off: the bound is active
    const SqpResult cold = sqp.solve(problem, x0);
    ASSERT_EQ(cold.status, SolveStatus::kOptimal);
    EXPECT_NEAR(cold.x[1], 0.5, 1e-8);
    {
        // The reference solver carries the same history: one solve at the old bounds.
        SqpSolver fresh_sqp = quiet_sqp();
        Problem old_bounds(std::make_shared<Box3>());
        ASSERT_EQ(fresh_sqp.solve(old_bounds, x0).status, SolveStatus::kOptimal);
        auto fresh_box = std::make_shared<Box3>();
        fresh_box->x1_upper = 0.5;
        Problem fresh(fresh_box);
        expect_same_sqp(cold, fresh_sqp.solve(fresh, x0));
    }

    const SqpResult from_payload = sqp.solve(problem, x0, payload);
    EXPECT_EQ(from_payload.status, SolveStatus::kOptimal);
    EXPECT_NEAR(from_payload.x[1], 0.5, 1e-8);

    const SqpResult from_native = sqp.solve(problem, x0, SqpWarmAccess::wrap(native));
    EXPECT_EQ(from_native.status, SolveStatus::kOptimal);
    EXPECT_GT(from_native.counters.start_level_used, StartLevel::kCold);
    EXPECT_NEAR(from_native.x[1], 0.5, 1e-8);
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
    // g is the engine's measurement mapped through each row's shift, to two
    // roundings (the engine's shift out and this map's shift back); row 4 is free, so no engine
    // measures it: NaN, never evaluated.
    ASSERT_EQ(r.g.size(), 5);
    Vec g(5);
    nlp.eval_g(r.x, g);
    for (Index k = 0; k < 4; ++k) {
        EXPECT_NEAR(r.g[k], g[k], 1e-12 * (std::abs(g[k]) + 10.0)) << "row " << k;
    }
    EXPECT_TRUE(std::isnan(r.g[4]));
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

// Five rows, every kind but free, two of them range rows active on opposite sides,
// over x in R^3 (all free):
//   min (x0-3)^2 + (x1-3)^2 + (x2-3)^2
//   row 0  x2           = 1        equality
//   row 1  x0 + x1     in [-1, 2]  range, active at its upper bound
//   row 2  x0 - x1     in [1, 5]   range, active at its lower bound
//   row 3  x0          <= 10       upper-bounded, inactive
//   row 4  x1          >= -10      lower-bounded, inactive
// x* = (1.5, 0.5, 1); grad f + J^T lambda = 0 gives lambda* = (4, 4, -1, 0, 0).
struct ActiveRanges final : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 3, .m = 5, .jacobian_nonzeros = 7, .hessian_nonzeros = 3};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << 1.0, -1.0, 1.0, -kInfinity, -10.0;
        gu << 1.0, 2.0, 5.0, 10.0, kInfinity;
    }
    double eval_f(ConstVecRef x) const override { return (x.array() - 3.0).square().sum(); }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g = 2.0 * (x.array() - 3.0).matrix();
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        g << x[2], x[0] + x[1], x[0] - x[1], x[0], x[1];
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 1, 2, 2, 3, 4;
        c << 2, 0, 1, 0, 1, 0, 1;
    }
    void eval_jacobian(ConstVecRef, VecRef v) const override { v << 1, 1, 1, 1, -1, 1, 1; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 2;
        c << 0, 1, 2;
    }
    void eval_hessian(ConstVecRef, double obj_factor, ConstVecRef, VecRef v) const override {
        v.setConstant(2.0 * obj_factor);
    }
};

template <class Result> void check_active_ranges(const Result &r, const ActiveRanges &nlp) {
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_EQ(r.lambda.size(), 5);
    ASSERT_EQ(r.g.size(), 5);
    EXPECT_NEAR(r.x[0], 1.5, 1e-7);
    EXPECT_NEAR(r.x[1], 0.5, 1e-7);
    EXPECT_NEAR(r.x[2], 1.0, 1e-7);
    // The range rows' signed multipliers: positive at the upper bound, negative at
    // the lower, each the composition of its two one-sided engine rows.
    EXPECT_NEAR(r.lambda[0], 4.0, 1e-6);
    EXPECT_NEAR(r.lambda[1], 4.0, 1e-6);
    EXPECT_NEAR(r.lambda[2], -1.0, 1e-6);
    EXPECT_NEAR(r.lambda[3], 0.0, 1e-6);
    EXPECT_NEAR(r.lambda[4], 0.0, 1e-6);
    // Stationarity in Ipopt's sign: grad f + J^T lambda = 0 (no variable bounds).
    Eigen::MatrixXd J = Eigen::MatrixXd::Zero(5, 3);
    J.row(0) << 0, 0, 1;
    J.row(1) << 1, 1, 0;
    J.row(2) << 1, -1, 0;
    J.row(3) << 1, 0, 0;
    J.row(4) << 0, 1, 0;
    Vec grad(3);
    nlp.eval_grad_f(r.x, grad);
    const Vec stationarity = grad + J.transpose() * r.lambda;
    EXPECT_LT(stationarity.lpNorm<Eigen::Infinity>(), 1e-6);
    // g is the engine's measurement mapped through each row's shift: g(x) to two
    // roundings (the engine's shift out and this map's shift back).
    Vec g(5);
    nlp.eval_g(r.x, g);
    for (Index k = 0; k < 5; ++k) {
        EXPECT_NEAR(r.g[k], g[k], 1e-12 * std::max(1.0, std::abs(g[k]) + 10.0)) << "row " << k;
    }
}

TEST(Problem, ActiveRangeRowsSatisfyStationarityOnBothEngines) {
    const auto nlp = std::make_shared<ActiveRanges>();
    Problem problem(nlp);
    EXPECT_EQ(problem.statistics().range_rows, 2);
    const Vec x0 = Vec::Zero(3);
    IpmSolver ipm = quiet_ipm();
    check_active_ranges(ipm.solve(problem, x0), *nlp);
    SqpSolver sqp = quiet_sqp();
    check_active_ranges(sqp.solve(problem, x0), *nlp);
}

// A pieces problem with rows reports the declaration's own row space,
// [c_E | c_I] with c_I(x) <= 0. HS071's transcription laid as pieces: row 1
// (sum of squares = 40) is c_E = g1 - 40, row 0 (product >= 25) is c_I = 25 - g0.
TEST(Problem, APiecesProblemReportsTheDeclarationsRowSpace) {
    const auto program = make_nlp_program(hs071());
    Problem pieces(AssemblyDeclaration(program->declaration()));
    EXPECT_EQ(pieces.num_constraints(), 2);
    EXPECT_EQ(pieces.statistics().equality_rows, 1);
    EXPECT_EQ(pieces.statistics().upper_bounded_rows, 1);

    IpmSolver ipm = quiet_ipm();
    const IpmResult r = ipm.solve(pieces, hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    const auto &rows = detail::engine_rows(r);
    ASSERT_EQ(r.lambda.size(), 2);
    ASSERT_EQ(r.g.size(), 2);
    EXPECT_EQ(r.lambda[0], rows.lambda_e[0]);
    EXPECT_EQ(r.lambda[1], rows.lambda_i[0]);
    EXPECT_EQ(r.g[0], rows.ce[0]);
    EXPECT_EQ(r.g[1], rows.ci[0]);

    Problem nlp_problem(hs071());
    IpmSolver nlp_ipm = quiet_ipm();
    const IpmResult s = nlp_ipm.solve(nlp_problem, hs071_start());
    ASSERT_EQ(s.status, SolveStatus::kOptimal);
    EXPECT_NEAR(r.lambda[0], s.lambda[1], 1e-8);
    EXPECT_NEAR(r.lambda[1], -s.lambda[0], 1e-8);
    EXPECT_NEAR(r.g[0], s.g[1] - 40.0, 1e-8);
    EXPECT_NEAR(r.g[1], 25.0 - s.g[0], 1e-8);
}

// ---------------------------------------------------------------------------
// report() makes no callback
// ---------------------------------------------------------------------------

struct CountingHs071 final : Nlp {
    hven_drivers_tests::Hs071Problem inner;
    mutable Index f = 0, grad = 0, g = 0, jac = 0, hess = 0;
    NlpDimensions dimensions() const override { return inner.dimensions(); }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        inner.bounds(xl, xu, gl, gu);
    }
    double eval_f(ConstVecRef x) const override {
        ++f;
        return inner.eval_f(x);
    }
    void eval_grad_f(ConstVecRef x, VecRef out) const override {
        ++grad;
        inner.eval_grad_f(x, out);
    }
    void eval_g(ConstVecRef x, VecRef out) const override {
        ++g;
        inner.eval_g(x, out);
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        inner.jacobian_structure(r, c);
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override {
        ++jac;
        inner.eval_jacobian(x, v);
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        inner.hessian_structure(r, c);
    }
    void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                      VecRef v) const override {
        ++hess;
        inner.eval_hessian(x, obj_factor, lambda, v);
    }
    std::array<Index, 5> counts() const { return {f, grad, g, jac, hess}; }
};

// Two pins. The public route's callback counts equal the internal route's (which
// has no row-space report), on both engines; and a report() made again after the
// solve moves no count.
TEST(Problem, ReportMakesNoCallback) {
    {
        const auto pub = std::make_shared<CountingHs071>();
        Problem problem(pub);
        IpmSolver ipm = quiet_ipm();
        IpmResult r = ipm.solve(problem, hs071_start());
        ASSERT_EQ(r.status, SolveStatus::kOptimal);
        ASSERT_EQ(r.g.size(), 2);

        const auto internal = std::make_shared<CountingHs071>();
        const auto program = make_nlp_program(internal);
        IpmSolver internal_ipm = quiet_ipm();
        ASSERT_EQ(internal_ipm.solve(detail::problem_ref(*program), hs071_start()).status,
                  SolveStatus::kOptimal);
        EXPECT_EQ(pub->counts(), internal->counts());

        const auto before = pub->counts();
        detail::ProblemAccess::report(problem, r);
        EXPECT_EQ(pub->counts(), before);
    }
    {
        const auto pub = std::make_shared<CountingHs071>();
        Problem problem(pub);
        SqpSolver sqp = quiet_sqp();
        SqpResult r = sqp.solve(problem, hs071_start());
        ASSERT_EQ(r.status, SolveStatus::kOptimal);
        ASSERT_EQ(r.g.size(), 2);
        EXPECT_EQ(pub->hess, r.counters.eval_hess_calls + 1); // + the bridge's one pattern walk

        const auto internal = std::make_shared<CountingHs071>();
        NlpProblemModel model(internal);
        SqpSolver internal_sqp = quiet_sqp();
        ASSERT_EQ(internal_sqp.solve(detail::problem_ref(model), hs071_start()).status,
                  SolveStatus::kOptimal);
        EXPECT_EQ(pub->counts(), internal->counts());

        const auto before = pub->counts();
        detail::ProblemAccess::report(problem, r);
        EXPECT_EQ(pub->counts(), before);
    }
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
    EXPECT_EQ(problem.num_partitions(), 1);
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

// ---------------------------------------------------------------------------
// M8 AP-2 B4 (S2): the warm start in the problem's row space
// ---------------------------------------------------------------------------

static_assert(!std::is_default_constructible_v<SqpWarmStart>,
              "SqpWarmStart declares no default constructor: a braced {} third argument must "
              "select the WarmStart overload");
static_assert(std::is_copy_constructible_v<SqpWarmStart> &&
                  std::is_nothrow_move_constructible_v<SqpWarmStart>,
              "SqpWarmStart is a copyable, movable handle");

using hven::detail::WarmStartAccess;

// A seed built from a result's own (x, lambda, z) is that result's warm start where
// the row map is injective, and always reads back as the same row-space lambda.
template <class Result>
void expect_seed_round_trip(const Problem &problem, const Result &r, bool every_row_injective) {
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_FALSE(r.warm_start.empty());
    const WarmStart seed = WarmStart::seed(problem, r.x, r.lambda, r.z);

    EXPECT_TRUE(seed.key() == r.warm_start.key()) << "seed() stamps the key the engine stamps";
    EXPECT_TRUE(bitwise_equal(seed.x(), r.warm_start.x()));
    EXPECT_TRUE(bitwise_equal(seed.z(), r.warm_start.z()));
    EXPECT_TRUE(seed.extensions().empty());
    // The row-space read-back is the result's lambda; the export's own read-back is too.
    EXPECT_TRUE(seed.lambda(problem) == r.lambda);
    EXPECT_TRUE(r.warm_start.lambda(problem) == r.lambda);

    const NlpProblemModel *model = detail::ProblemAccess::nlp_model(problem);
    ASSERT_NE(model, nullptr);
    const NlpRowClassification &rows = model->rows();
    const Vec &seed_eq = WarmStartAccess::eq(seed);
    const Vec &seed_iq = WarmStartAccess::iq(seed);
    const Vec &warm_eq = WarmStartAccess::eq(r.warm_start);
    const Vec &warm_iq = WarmStartAccess::iq(r.warm_start);
    ASSERT_EQ(seed_eq.size(), warm_eq.size());
    ASSERT_EQ(seed_iq.size(), warm_iq.size());
    for (std::size_t k = 0; k < rows.kinds.size(); ++k) {
        switch (rows.kinds[k]) {
        case NlpRowKind::kEquality:
            EXPECT_EQ(seed_eq[rows.eq_row[k]], warm_eq[rows.eq_row[k]]) << "row " << k;
            break;
        case NlpRowKind::kUpperBounded:
            EXPECT_EQ(seed_iq[rows.iq_upper_row[k]], warm_iq[rows.iq_upper_row[k]]) << "row " << k;
            break;
        case NlpRowKind::kLowerBounded:
            EXPECT_EQ(seed_iq[rows.iq_lower_row[k]], warm_iq[rows.iq_lower_row[k]]) << "row " << k;
            break;
        case NlpRowKind::kRange:
            // compose is not injective on a range row's two sides: an interior-point
            // pair with both sides live is recovered only as their difference.
            if (every_row_injective) {
                EXPECT_EQ(seed_iq[rows.iq_upper_row[k]], warm_iq[rows.iq_upper_row[k]])
                    << "row " << k;
                EXPECT_EQ(seed_iq[rows.iq_lower_row[k]], warm_iq[rows.iq_lower_row[k]])
                    << "row " << k;
            }
            break;
        case NlpRowKind::kFree:
            break;
        }
    }
}

TEST(WarmStartRowSpace, ASeedFromAResultRoundTripsOnEveryNlpRowKind) {
    {
        const auto nlp = std::make_shared<EveryRowKind>();
        Problem problem(nlp);
        const Vec x0{{0.0, 0.3, 1.2}};
        IpmSolver ipm = quiet_ipm();
        expect_seed_round_trip(problem, ipm.solve(problem, x0), /*every_row_injective=*/false);
        SqpSolver sqp = quiet_sqp();
        expect_seed_round_trip(problem, sqp.solve(problem, x0), /*every_row_injective=*/true);
    }
    {
        // Two range rows active on opposite sides.
        const auto nlp = std::make_shared<ActiveRanges>();
        Problem problem(nlp);
        const Vec x0 = Vec::Zero(3);
        IpmSolver ipm = quiet_ipm();
        expect_seed_round_trip(problem, ipm.solve(problem, x0), false);
        SqpSolver sqp = quiet_sqp();
        expect_seed_round_trip(problem, sqp.solve(problem, x0), true);
    }
}

TEST(WarmStartRowSpace, TheSeedKeyIsTheKeyBothEnginesStampOnExport) {
    const auto box = std::make_shared<Box3>();
    Problem problem(box);
    const Vec x0 = Vec::Constant(3, 0.5);
    IpmSolver ipm = quiet_ipm();
    SqpSolver sqp = quiet_sqp();
    const IpmResult a = ipm.solve(problem, x0);
    const SqpResult b = sqp.solve(problem, x0);
    ASSERT_EQ(a.status, SolveStatus::kOptimal);
    ASSERT_EQ(b.status, SolveStatus::kOptimal);
    const WarmStart seed = WarmStart::seed(problem, Vec(), a.lambda, Vec());
    EXPECT_TRUE(seed.key() == a.warm_start.key());
    EXPECT_TRUE(seed.key() == b.warm_start.key());
}

TEST(WarmStartRowSpace, ASeedOnAPiecesProblemIsTheEqualityRowsThenTheInequalityRows) {
    const auto program = make_nlp_program(hs071());
    Problem pieces(AssemblyDeclaration(program->declaration()));
    IpmSolver ipm = quiet_ipm();
    const IpmResult r = ipm.solve(pieces, hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_EQ(r.lambda.size(), 2);

    const WarmStart seed = WarmStart::seed(pieces, r.x, r.lambda, r.z);
    EXPECT_TRUE(seed.key() == r.warm_start.key());
    ASSERT_EQ(WarmStartAccess::eq(seed).size(), 1);
    ASSERT_EQ(WarmStartAccess::iq(seed).size(), 1);
    EXPECT_EQ(WarmStartAccess::eq(seed)[0], r.lambda[0]);
    EXPECT_EQ(WarmStartAccess::iq(seed)[0], r.lambda[1]);
    EXPECT_EQ(WarmStartAccess::eq(seed)[0], WarmStartAccess::eq(r.warm_start)[0]);
    EXPECT_EQ(WarmStartAccess::iq(seed)[0], WarmStartAccess::iq(r.warm_start)[0]);
    EXPECT_TRUE(seed.lambda(pieces) == r.lambda);

    // The IPM takes the value back.
    IpmSolver again = quiet_ipm();
    const IpmResult warm = again.solve(pieces, hs071_start(), seed);
    EXPECT_EQ(warm.status, SolveStatus::kOptimal);
    EXPECT_EQ(warm.payload_ignored, 0);
}

TEST(WarmStartRowSpace, AMultipliersOnlySeedReachesSeededOnTheSqpAndIsAcceptedByTheIpm) {
    Problem problem(hs071());
    SqpSolver producer = quiet_sqp();
    const SqpResult first = producer.solve(problem, hs071_start());
    ASSERT_EQ(first.status, SolveStatus::kOptimal);

    const WarmStart seed = WarmStart::seed(problem, Vec(), first.lambda, Vec());
    EXPECT_FALSE(seed.empty());
    EXPECT_EQ(seed.x().size(), 0);
    EXPECT_EQ(seed.z().size(), 0);

    SqpSolver sqp = quiet_sqp();
    const SqpResult seeded = sqp.solve(problem, hs071_start(), seed);
    EXPECT_EQ(seeded.status, SolveStatus::kOptimal);
    EXPECT_EQ(seeded.counters.start_level_used, StartLevel::kSeeded);

    IpmSolver ipm = quiet_ipm();
    const IpmResult accepted = ipm.solve(problem, hs071_start(), seed);
    EXPECT_EQ(accepted.status, SolveStatus::kOptimal);
    EXPECT_EQ(accepted.payload_ignored, 0);
    EXPECT_EQ(accepted.polish_ignored, 0);
}

TEST(WarmStartRowSpace, AnIdentityMismatchedSeedIsRefusedByBothEngines) {
    const auto box = std::make_shared<Box3>();
    Problem problem(box);
    const Vec x0 = Vec::Constant(3, 0.5);
    IpmSolver ipm = quiet_ipm();
    const IpmResult first = ipm.solve(problem, x0);
    ASSERT_EQ(first.status, SolveStatus::kOptimal);
    const WarmStart seed = WarmStart::seed(problem, first.x, first.lambda, first.z);
    const WarmStart multipliers_only = WarmStart::seed(problem, Vec(), first.lambda, Vec());

    // A fixed variable changes the declared bound structure: a different problem.
    box->x1_lower = box->x1_upper = 0.25;
    SqpSolver sqp = quiet_sqp();
    EXPECT_THROW((void)ipm.solve(problem, x0, seed), std::invalid_argument);
    EXPECT_THROW((void)ipm.solve(problem, x0, multipliers_only), std::invalid_argument);
    EXPECT_THROW((void)sqp.solve(problem, x0, seed), std::invalid_argument);
    EXPECT_THROW((void)sqp.solve(problem, x0, multipliers_only), std::invalid_argument);
    // ... and lambda() refuses to read it against the problem it no longer describes.
    EXPECT_THROW((void)seed.lambda(problem), std::invalid_argument);
}

TEST(WarmStartRowSpace, SeedAndLambdaRefuseWrongWidthsByName) {
    Problem problem(hs071());
    const Vec x = hs071_start();
    const Vec z = Vec::Zero(4);
    const Vec lambda = Vec::Zero(2);
    EXPECT_NO_THROW((void)WarmStart::seed(problem, x, lambda, z));
    EXPECT_THROW((void)WarmStart::seed(problem, x, Vec::Zero(3), z), std::invalid_argument);
    EXPECT_THROW((void)WarmStart::seed(problem, Vec::Zero(3), lambda, Vec::Zero(3)),
                 std::invalid_argument);
    EXPECT_THROW((void)WarmStart::seed(problem, x, lambda, Vec()), std::invalid_argument);
    EXPECT_THROW((void)WarmStart::seed(problem, Vec(), lambda, z), std::invalid_argument);
    // An empty value has no multiplier blocks at a problem that has rows.
    EXPECT_THROW((void)WarmStart().lambda(problem), std::invalid_argument);
}

TEST(WarmStartRowSpace, AnEmptyWarmStartIsTheColdSolveCounterForCounter) {
    {
        Problem a(hs071());
        Problem b(hs071());
        IpmSolver plain = quiet_ipm();
        IpmSolver empty = quiet_ipm();
        const IpmResult cold = plain.solve(a, hs071_start());
        const IpmResult with_empty = empty.solve(b, hs071_start(), WarmStart{});
        expect_same_ipm(cold, with_empty);
        EXPECT_EQ(cold.payload_ignored, with_empty.payload_ignored);
        EXPECT_EQ(cold.polish_ignored, with_empty.polish_ignored);
        EXPECT_EQ(with_empty.payload_ignored, 0);
        EXPECT_EQ(with_empty.polish_ignored, 0);
        EXPECT_EQ(cold.work.objective_evals, with_empty.work.objective_evals);
        EXPECT_EQ(cold.work.hessian_evals, with_empty.work.hessian_evals);
        EXPECT_TRUE(bitwise_equal(cold.lambda, with_empty.lambda));
    }
    {
        Problem a(hs071());
        Problem b(hs071());
        SqpSolver plain = quiet_sqp();
        SqpSolver empty = quiet_sqp();
        const SqpResult cold = plain.solve(a, hs071_start());
        const SqpResult with_empty = empty.solve(b, hs071_start(), WarmStart{});
        expect_same_sqp(cold, with_empty);
        EXPECT_EQ(cold.counters.start_level_used, with_empty.counters.start_level_used);
        EXPECT_EQ(cold.counters.polish_ignored, with_empty.counters.polish_ignored);
        EXPECT_EQ(cold.work.objective_evals, with_empty.work.objective_evals);
    }
}

// A braced `{}` third argument is the WarmStart overload: SqpWarmStart has no default
// constructor to compete with it, and a budget is spelled after it.
TEST(WarmStartRowSpace, ABracedEmptyThirdArgumentSelectsTheWarmStartOverload) {
    Problem a(hs071());
    Problem b(hs071());
    SqpSolver plain = quiet_sqp();
    SqpSolver braced = quiet_sqp();
    const SqpResult cold = plain.solve(a, hs071_start());
    const SqpResult via_braces = braced.solve(b, hs071_start(), {});
    expect_same_sqp(cold, via_braces);
    EXPECT_EQ(via_braces.counters.polish_ignored, 0);

    IpmSolver ipm = quiet_ipm();
    Problem c(hs071());
    const IpmResult ipm_braced = ipm.solve(c, hs071_start(), {});
    EXPECT_EQ(ipm_braced.status, SolveStatus::kOptimal);
    EXPECT_EQ(ipm_braced.payload_ignored, 0);

    // The native overload is still reachable by its type, and a result's native warm
    // start is a valid handle.
    SqpSolver native = quiet_sqp();
    Problem d(hs071());
    const SqpResult native_first = native.solve(d, hs071_start());
    ASSERT_TRUE(native_first.sqp_warm_start.valid());
    EXPECT_EQ(native_first.sqp_warm_start.x().size(), 4);
    const SqpResult native_second = native.solve(d, hs071_start(), native_first.sqp_warm_start);
    EXPECT_EQ(native_second.status, SolveStatus::kOptimal);
    EXPECT_GT(native_second.counters.start_level_used, StartLevel::kSeeded);
}

// The crossover from the currency, entered with the problem: the bounds and the
// declaration key come from the Problem.
TEST(WarmStartRowSpace, ToSqpWarmStartTakesTheBoundsAndTheKeyFromTheProblem) {
    Problem problem(hs071());
    IpmOptions rough;
    rough.common.verbosity = hven::Verbosity::kSilent;
    rough.common.optimality_tol = 1e-3;
    rough.common.feasibility_tol = 1e-3;
    IpmSolver ipm(rough);
    const IpmResult r = ipm.solve(problem, hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);

    const SqpWarmStart staged = to_sqp_warm_start(r.warm_start, problem);
    EXPECT_TRUE(staged.valid());
    EXPECT_EQ(staged.x().size(), 4);

    SqpSolver sqp = quiet_sqp();
    const SqpResult from_staged = sqp.solve(problem, r.x, staged);
    EXPECT_EQ(from_staged.status, SolveStatus::kOptimal);
    EXPECT_EQ(from_staged.counters.start_level_used, StartLevel::kSeeded);
    SqpSolver sqp_direct = quiet_sqp();
    const SqpResult direct = sqp_direct.solve(problem, r.x, r.warm_start);
    EXPECT_EQ(direct.counters.start_level_used, StartLevel::kSeeded);

    // A core-only value (no polish extension) cannot be bridged, and says so.
    EXPECT_THROW((void)to_sqp_warm_start(WarmStart::seed(problem, r.x, r.lambda, r.z), problem),
                 std::invalid_argument);
    // A value taken under another problem is refused by its stamp.
    Problem other(std::make_shared<Box3>());
    EXPECT_THROW((void)to_sqp_warm_start(r.warm_start, other), std::invalid_argument);
}

} // namespace
