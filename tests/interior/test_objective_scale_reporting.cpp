// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The objective scale is internal to the solve, and these are the two
// boundaries that keep it there.
//
// The solver minimizes obj_scale * f, so its Lagrangian is
// L = s*f + lambda_e^T cE + lambda_i^T cI - z and every multiplier it carries
// is the scaled problem's -- as is every objective value it evaluates. The
// caller's problem is the unscaled one, and nlp_model.h's stationarity
// convention is stated at s = 1, so the scale is divided out of what the solve
// REPORTS and multiplied into what a caller SEEDS. The tests below hold both
// directions: nothing a caller reads or writes moves when the scale does.
//
// The minimizer itself is scale-invariant for a positive scale, so the primals
// are the control: they were already the same at every scale and stay so.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <string>

#include <Eigen/Core>

#include "hven/detail/drivers/result_access.h"
#include "hven/detail/model/problem_access.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/model/nlp.h"

#include "declared_route.h" // NOLINT(build/include_subdir)

using hven::IpmSolver;
using hven::Nlp;
using hven::detail::ConstEigenRef;
using hven_interior_tests::solve_declared;
using hven_interior_tests::transcribe;

// min 0.5*|x|^2 subject to sum(x) == 3, with a two-sided box on every
// variable. The optimum is x_i = 0.75 with f = 1.125, and the equality
// multiplier is -0.75 under the caller's convention -- all three known by hand,
// so a scale leaking into a report is a difference from a NUMBER rather than a
// difference between two runs.
struct ObjScaleBoxedProblem : Nlp, hven_interior_tests::MultiplierSeedHook {
    hven::NlpDimensions dimensions() const override {
        return {num_vars(), num_cons(), num_jac_nonzeros(), num_hess_nonzeros()};
    }

    static constexpr int kN = 4;

    int num_vars() const { return kN; }
    int num_cons() const { return 1; }
    int num_jac_nonzeros() const { return kN; }
    int num_hess_nonzeros() const { return kN; }

    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        xl.setConstant(-2.0);
        xu.setConstant(2.0);
        gl.setConstant(3.0);
        gu.setConstant(3.0);
    }
    double eval_f(hven::ConstVecRef x) const override {
        double f = 0.0;
        f = 0.5 * x.squaredNorm();
        return f;
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override { g = x; }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override { g[0] = x.sum(); }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        for (int i = 0; i < kN; i++) {
            r[i] = 0;
            c[i] = i;
        }
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        for (int i = 0; i < kN; i++) {
            r[i] = i;
            c[i] = i;
        }
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef v) const override { v.setConstant(1.0); }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v.setConstant(obj_factor);
    }
    /// The seed the installation test relies on: a multiplier on the CALLER's
    /// convention, handed over through the problem's own seed hook. Since M6 W5
    /// T8.9 the CALLER consults it (declared_route.h's
    /// starting_multiplier_seed, over NlpProblemModel::split_user_multipliers)
    /// and hands the result to the payload overload of solve(); a hook that
    /// returns false yields no payload at all.
    bool starting_multipliers(hven::VecRef lambda) const override {
        lambda.setConstant(kSeed);
        return true;
    }

    static constexpr double kSeed = -0.75;

    std::string name() const override { return "ObjScaleBoxed"; }
};

// A variable pinned below by its own bound, so the bound multiplier at the
// solution is a real active one (1.0 on the caller's scale) rather than a
// vanishing residual.
struct ObjScaleActiveBoundProblem : Nlp {
    hven::NlpDimensions dimensions() const override {
        return {num_vars(), num_cons(), num_jac_nonzeros(), num_hess_nonzeros()};
    }

    static constexpr int kN = 2;
    static constexpr double kInf = std::numeric_limits<double>::infinity();

    int num_vars() const { return kN; }
    int num_cons() const { return 1; }
    int num_jac_nonzeros() const { return kN; }
    int num_hess_nonzeros() const { return kN; }

    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        xl << 1.0, -kInf;
        xu << kInf, kInf;
        gl << 0.5;
        gu << kInf;
    }
    double eval_f(hven::ConstVecRef x) const override {
        double f = 0.0;
        f = 0.5 * x.squaredNorm();
        return f;
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override { g = x; }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override { g[0] = x[0] + x[1]; }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        for (int i = 0; i < kN; i++) {
            r[i] = 0;
            c[i] = i;
        }
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        for (int i = 0; i < kN; i++) {
            r[i] = i;
            c[i] = i;
        }
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef v) const override { v.setConstant(1.0); }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "ObjScaleActiveBound"; }
};

namespace {

constexpr double kObjScaleTol = 1e-5;

} // namespace

TEST(ObjectiveScaleReporting, TheReportedObjectiveAndMultipliersDoNotMoveWithTheScale) {
    for (double scale : {1.0, 2.0, 10.0, 0.125}) {
        const auto route = transcribe(std::make_shared<ObjScaleBoxedProblem>());
        IpmSolver solver;
        {
            auto o = solver.options();
            o.common.verbosity = hven::Verbosity::kSilent;
            o.objective_scale = scale;
            solver.set_options(std::move(o));
        }

        const Eigen::VectorXd x0 = Eigen::VectorXd::Constant(ObjScaleBoxedProblem::kN, 0.6);
        const hven::IpmResult result = solve_declared(solver, route, x0);
        ASSERT_EQ(result.status, hven::SolveStatus::kOptimal) << "scale " << scale;

        EXPECT_NEAR(result.f, 1.125, kObjScaleTol) << "scale " << scale;
        ASSERT_EQ(hven::detail::engine_rows(result).lambda_e.size(), 1);
        EXPECT_NEAR(hven::detail::engine_rows(result).lambda_e[0], -0.75, kObjScaleTol)
            << "scale " << scale;

        // The control: the minimizer never depended on the scale, and still
        // does not.
        for (int i = 0; i < ObjScaleBoxedProblem::kN; i++) {
            EXPECT_NEAR(result.x[i], 0.75, kObjScaleTol) << "scale " << scale;
        }
    }
}

TEST(ObjectiveScaleReporting, AnActiveBoundMultiplierDoesNotMoveWithTheScale) {
    for (double scale : {1.0, 2.0, 10.0}) {
        const auto route = transcribe(std::make_shared<ObjScaleActiveBoundProblem>());
        IpmSolver solver;
        {
            auto o = solver.options();
            o.common.verbosity = hven::Verbosity::kSilent;
            o.objective_scale = scale;
            solver.set_options(std::move(o));
        }

        const Eigen::VectorXd x0 = Eigen::VectorXd::Constant(ObjScaleActiveBoundProblem::kN, 0.6);
        const hven::IpmResult result = solve_declared(solver, route, x0);
        ASSERT_EQ(result.status, hven::SolveStatus::kOptimal) << "scale " << scale;

        ASSERT_EQ(result.z.size(), ObjScaleActiveBoundProblem::kN);
        // x0 sits on its lower bound, and the bound multiplier that holds it
        // there balances the objective gradient: 1.0 on the caller's scale.
        EXPECT_NEAR(result.z[0], 1.0, kObjScaleTol) << "scale " << scale;
        EXPECT_NEAR(result.x[0], 1.0, kObjScaleTol) << "scale " << scale;
    }
}

namespace {

/// Runs one seeded solve and reports the equality multiplier as it stood in
/// the iterate at the first callback -- the last moment before any algorithmic
/// step has moved it, and therefore the installed seed itself.
double obj_scale_installed_seed(double scale) {
    const auto route = transcribe(std::make_shared<ObjScaleBoxedProblem>());
    IpmSolver solver;
    {
        auto o = solver.options();
        o.common.verbosity = hven::Verbosity::kSilent;
        o.objective_scale = scale;
        solver.set_options(std::move(o));
    }

    double installed = 0.0;
    bool seen = false;
    const int primal_vars = ObjScaleBoxedProblem::kN;
    solver.set_kkt_hook([&](int iteration, double, hven::detail::ConstEigenRef<Eigen::VectorXd> xsl,
                            double, hven::detail::ConstEigenRef<Eigen::VectorXd>,
                            hven::detail::ConstEigenRef<Eigen::VectorXd>,
                            Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
        if (iteration == 0 && !seen) {
            // No slack variables on this problem, so the equality
            // multiplier block follows the primals directly.
            installed = xsl[primal_vars];
            seen = true;
        }
        return 0;
    });

    EXPECT_EQ(solve_declared(solver, route, Eigen::VectorXd::Constant(primal_vars, 0.6)).status,
              hven::SolveStatus::kOptimal)
        << "scale " << scale;
    EXPECT_TRUE(seen) << "the early callback never ran, so nothing was observed";
    return installed;
}

} // namespace

// The other direction of the same boundary: a seed arrives on the caller's
// convention and is installed on the solver's, which is the caller's
// multiplied by the scale.
TEST(ObjectiveScaleReporting, ASeededMultiplierIsInstalledOnTheSolversScale) {
    const double unit = obj_scale_installed_seed(1.0);

    // The unit-scale case is the identity, so what lands is the seed itself.
    EXPECT_DOUBLE_EQ(unit, ObjScaleBoxedProblem::kSeed);

    // And every other scale multiplies it, exactly.
    EXPECT_DOUBLE_EQ(obj_scale_installed_seed(4.0), ObjScaleBoxedProblem::kSeed * 4.0);
    EXPECT_DOUBLE_EQ(obj_scale_installed_seed(0.25), ObjScaleBoxedProblem::kSeed * 0.25);
}

// A problem with no constraint rows at all, used to show that a solver reused
// across shapes reports the shape it just solved.
struct ObjScaleUnconstrainedProblem : Nlp {
    hven::NlpDimensions dimensions() const override {
        return {num_vars(), num_cons(), num_jac_nonzeros(), num_hess_nonzeros()};
    }

    static constexpr int kN = 3;

    int num_vars() const { return kN; }
    int num_cons() const { return 0; }
    int num_jac_nonzeros() const { return 0; }
    int num_hess_nonzeros() const { return kN; }

    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef, hven::VecRef) const override {
        xl.setConstant(-2.0);
        xu.setConstant(2.0);
    }
    double eval_f(hven::ConstVecRef x) const override {
        double f = 0.0;
        f = 0.5 * x.squaredNorm();
        return f;
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override { g = x; }
    void eval_g(hven::ConstVecRef, hven::VecRef) const override {}
    void jacobian_structure(hven::IndexVecRef, hven::IndexVecRef) const override {}
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        for (int i = 0; i < kN; i++) {
            r[i] = i;
            c[i] = i;
        }
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef) const override {}
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "ObjScaleUnconstrained"; }
};

// A scale multiplies the objective; it does not turn minimization into
// maximization. A negative factor would do the latter while leaving the
// multiplier cones the solve reports against exactly where they were, so a
// sign-constrained dual would come back with a sign its own convention rules
// out.
//
// ONE door since M6 W5 T8.3: validate(), which set_options() runs over the whole
// value. The second door this test used to name -- the entry check catching a
// scale written PAST the setter through the mutable settings() reference -- has
// nothing left to catch: there is no mutable reference and no per-field setter,
// so an invalid scale cannot reach opts_ at all. run_phase_sequence() still
// validates at entry, as defense in depth for a future write path.
TEST(ObjectiveScaleReporting, ANegativeScaleIsRefusedByValidate) {
    const auto route = transcribe(std::make_shared<ObjScaleBoxedProblem>());
    IpmSolver solver;
    {
        auto o = solver.options();
        o.common.verbosity = hven::Verbosity::kSilent;
        solver.set_options(std::move(o));
    }

    const auto with_scale = [&](double scale) {
        auto o = solver.options();
        o.objective_scale = scale;
        return o;
    };
    EXPECT_THROW(hven::validate(with_scale(-1.0)), std::invalid_argument);
    EXPECT_THROW(hven::validate(with_scale(0.0)), std::invalid_argument);
    EXPECT_THROW(hven::validate(with_scale(-1e-12)), std::invalid_argument);
    EXPECT_NO_THROW(solver.set_options(with_scale(2.0)));

    // The refusal names the value, so the reader is not left to guess which
    // setting was rejected.
    try {
        {
            auto o = solver.options();
            o.objective_scale = -3.5;
            solver.set_options(std::move(o));
        }
        FAIL() << "a negative scale must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("objective_scale"), std::string::npos) << message;
        EXPECT_NE(message.find("-3.5"), std::string::npos) << message;
    }

    // The refusal left the previous value standing, so the solver is still
    // usable and still running at the scale it accepted.
    EXPECT_EQ(solver.options().objective_scale, 2.0);
    const Eigen::VectorXd x0 = Eigen::VectorXd::Constant(ObjScaleBoxedProblem::kN, 0.6);
    EXPECT_EQ(solve_declared(solver, route, x0).status, hven::SolveStatus::kOptimal);
}

// The reported constraint blocks describe the problem the call just solved,
// and nothing else. Without that, a solver reused across shapes would keep the
// earlier call's block standing and the scale seam would divide it a second
// time on every subsequent call.
TEST(ObjectiveScaleReporting, AnUnconstrainedCallReportsNoConstraintBlocks) {
    const auto route = transcribe(std::make_shared<ObjScaleBoxedProblem>());
    IpmSolver solver;
    {
        auto o = solver.options();
        o.common.verbosity = hven::Verbosity::kSilent;
        o.objective_scale = 2.0;
        solver.set_options(std::move(o));
    }

    const hven::IpmResult boxed =
        solve_declared(solver, route, Eigen::VectorXd::Constant(ObjScaleBoxedProblem::kN, 0.6));
    ASSERT_EQ(boxed.status, hven::SolveStatus::kOptimal);
    ASSERT_EQ(hven::detail::engine_rows(boxed).lambda_e.size(), 1);
    const double constrained_eq = hven::detail::engine_rows(boxed).lambda_e[0];
    EXPECT_NEAR(constrained_eq, -0.75, kObjScaleTol);

    // The SAME engine, at the same scale, pointed at a program with no
    // constraint rows at all.
    const auto unconstrained =
        hven::detail::make_nlp_program(std::make_shared<ObjScaleUnconstrainedProblem>());

    // THE SAME ENGINE, THE OTHER PROGRAM -- expressed by handing the other
    // program to solve() rather than by re-attaching (M6 W5 T8.4).
    const Eigen::VectorXd x0 = Eigen::VectorXd::Constant(ObjScaleUnconstrainedProblem::kN, 0.6);
    const hven::IpmResult first = solver.solve(hven::detail::problem_ref(*unconstrained), x0);
    ASSERT_EQ(first.status, hven::SolveStatus::kOptimal);

    EXPECT_EQ(hven::detail::engine_rows(first).lambda_e.size(), 0)
        << "there are no equality rows, so there is no equality multiplier block";
    EXPECT_EQ(hven::detail::engine_rows(first).lambda_i.size(), 0);
    EXPECT_EQ(hven::detail::engine_rows(first).ce.size(), 0);
    EXPECT_EQ(hven::detail::engine_rows(first).ci.size(), 0);

    // And a second call still reports nothing, rather than a block that has
    // been divided by the scale one more time.
    const hven::IpmResult second = solver.solve(hven::detail::problem_ref(*unconstrained), x0);
    ASSERT_EQ(second.status, hven::SolveStatus::kOptimal);
    EXPECT_EQ(hven::detail::engine_rows(second).lambda_e.size(), 0);
    EXPECT_NEAR(constrained_eq, -0.75, kObjScaleTol) << "the first call's value is not in doubt";
}

// One call runs at one scale. The setting is taken at entry and read from there
// by everything downstream, and since M6 W5 T8.3 a replacement attempted while
// the call is in flight is REFUSED outright (std::logic_error) rather than
// deferred to the next call -- either way this call cannot be split between two
// scales, which would report an objective and duals belonging to no problem.
TEST(ObjectiveScaleReporting, TheScaleACallRanAtIsTheScaleItsOutputsAreReportedOn) {
    const auto route = transcribe(std::make_shared<ObjScaleBoxedProblem>());
    IpmSolver solver;
    {
        auto o = solver.options();
        o.common.verbosity = hven::Verbosity::kSilent;
        o.objective_scale = 2.0;
        solver.set_options(std::move(o));
    }

    bool attempted = false;
    bool refused = false;
    solver.set_kkt_hook([&](int iteration, double, hven::detail::ConstEigenRef<Eigen::VectorXd>,
                            double, hven::detail::ConstEigenRef<Eigen::VectorXd>,
                            hven::detail::ConstEigenRef<Eigen::VectorXd>,
                            Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
        if (iteration == 0 && !attempted) {
            auto o = solver.options();
            o.objective_scale = 4.0;
            try {
                solver.set_options(std::move(o));
            } catch (const std::logic_error &) {
                refused = true;
            }
            attempted = true;
        }
        return 0;
    });

    hven::IpmResult result =
        solve_declared(solver, route, Eigen::VectorXd::Constant(ObjScaleBoxedProblem::kN, 0.6));
    ASSERT_EQ(result.status, hven::SolveStatus::kOptimal);
    ASSERT_TRUE(attempted) << "the callback never ran, so nothing was attempted under the call";
    EXPECT_TRUE(refused) << "a replacement under an in-flight solve must be a logic_error";

    // Reported on the entry scale of 2, which is what every phase evaluated
    // at -- and the refusal left that scale in force.
    EXPECT_NEAR(result.f, 1.125, kObjScaleTol);
    ASSERT_EQ(hven::detail::engine_rows(result).lambda_e.size(), 1);
    EXPECT_NEAR(hven::detail::engine_rows(result).lambda_e[0], -0.75, kObjScaleTol);
    EXPECT_EQ(solver.options().objective_scale, 2.0);

    // Between calls the replacement goes through, and the next call runs at 4
    // and reports the same caller-scale numbers because that is what the seam
    // is for.
    solver.clear_kkt_hook();
    {
        auto o = solver.options();
        o.objective_scale = 4.0;
        solver.set_options(std::move(o));
    }
    result =
        solve_declared(solver, route, Eigen::VectorXd::Constant(ObjScaleBoxedProblem::kN, 0.6));
    ASSERT_EQ(result.status, hven::SolveStatus::kOptimal);
    EXPECT_NEAR(result.f, 1.125, kObjScaleTol);
}
