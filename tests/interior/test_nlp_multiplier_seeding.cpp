// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The opt-in constraint-multiplier seed and apply_staged_multipliers -- the
// path NLPProblem::starting_multipliers() feeds through, which since M6 W5
// T8.9 is the caller's own: declared_route.h's starting_multiplier_seed over
// NlpProblemModel::split_user_multipliers, handed to the payload overload.
//
// M6 W5 T8.5 REPLACED THE ENTRY, not the behaviour. InteriorPointSolver::
// set_initial_multipliers()/clear_initial_multipliers() are gone; a seed is the
// MULTIPLIERS-ONLY FORM of the shared payload -- a WarmStartData with an EMPTY
// `primal_` -- handed to `solve(model, x0, warm, budget)` as an argument. The
// install site, the [kSeededIqMultFloor, kSeededMultInitMax] clamps and the
// objective-scale handling are unchanged, and `x0` is still the start. Three
// tests about the STAGING STATE (its consumption on throw, its clear-first rule
// on a declining problem, and "the unseeded path does not consult it") are
// retired in place, each with the argument that the state they guarded no
// longer exists.
//
// Problem structs here are deliberately distinct
// from (though structurally similar to) the ones in test_ipm_solver_entry.cpp: the
// unity build merges test TUs, so file-scope names must not collide.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>

#include "hven/drivers/interior_point_solver.h"
#include "hven/warmstart/warm_start_data.h"

#include "declared_route.h" // NOLINT(build/include_subdir)

namespace {
constexpr double kSeedSolverInf = std::numeric_limits<double>::infinity();
} // namespace

using hven::ConstEigenRef;
using hven::solvers::NLPProblem;

// The canonical Ipopt HS071 example: n=4, one lower-bounded product row, one
// equality sphere row, dense Jacobian and Hessian. Same problem as
// test_ipm_solver_entry.cpp's Hs071Problem, duplicated under a distinct name to
// avoid a unity-build symbol collision.
struct SeedHs071Problem : NLPProblem {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0;
        xu << 5.0, 5.0, 5.0, 5.0;
        gl << 25.0, 40.0;
        gu << kSeedSolverInf, 40.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]);
        g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0;
        g[3] = x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[1] * x[2] * x[3];
        g[1] = x[0] * x[0] + x[1] * x[1] + x[2] * x[2] + x[3] * x[3];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = x[1] * x[2] * x[3];
        v[1] = x[0] * x[2] * x[3];
        v[2] = x[0] * x[1] * x[3];
        v[3] = x[0] * x[1] * x[2];
        v[4] = 2.0 * x[0];
        v[5] = 2.0 * x[1];
        v[6] = 2.0 * x[2];
        v[7] = 2.0 * x[3];
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        const double x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
        v[0] = obj_factor * 2 * x3 + lambda[1] * 2;
        v[1] = obj_factor * x3 + lambda[0] * x2 * x3;
        v[2] = lambda[1] * 2;
        v[3] = obj_factor * x3 + lambda[0] * x1 * x3;
        v[4] = lambda[0] * x0 * x3;
        v[5] = lambda[1] * 2;
        v[6] = obj_factor * (2 * x0 + x1 + x2) + lambda[0] * x1 * x2;
        v[7] = obj_factor * x0 + lambda[0] * x0 * x2;
        v[8] = obj_factor * x0 + lambda[0] * x0 * x1;
        v[9] = lambda[1] * 2;
    }
    std::string name() const override { return "SeedHs071Problem"; }
};

// SeedHs071Problem plus a starting_multipliers() override seeding a modest,
// merely-plausible-sign guess (not the exact solution multiplier -- unlike
// EqualityMultiplierHasIpoptSign-style tests, this is only checking that a
// seed which is in the right ballpark does not perturb the converged
// optimum).
struct SeededSeedHs071Problem : SeedHs071Problem {
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        lambda[0] = -1.0; // row 0: active lower bound -> negative Ipopt sign
        lambda[1] = 0.5;  // row 1: equality, sign unconstrained
        return true;
    }
    std::string name() const override { return "SeededSeedHs071Problem"; }
};

TEST(NLPMultiplierSeedingTest, SeededSolveMatchesUnseededSolution) {
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;

    const auto unseeded_route =
        hven_interior_tests::transcribe(std::make_shared<SeedHs071Problem>());
    hven::solvers::InteriorPointSolver unseeded;
    hven::solvers::IpmResult unseeded_result;
    {
        auto o = unseeded.options();
        o.common.print_level = 10;
        unseeded.set_options(std::move(o));
    }
    unseeded_result = hven_interior_tests::solve_declared(unseeded, unseeded_route, x0);
    ASSERT_EQ(unseeded_result.status, hven::solvers::SolveStatus::kOptimal);

    const auto seeded_route =
        hven_interior_tests::transcribe(std::make_shared<SeededSeedHs071Problem>());
    hven::solvers::InteriorPointSolver seeded;
    hven::solvers::IpmResult seeded_result;
    {
        auto o = seeded.options();
        o.common.print_level = 10;
        seeded.set_options(std::move(o));
    }
    seeded_result = hven_interior_tests::solve_declared(seeded, seeded_route, x0);
    ASSERT_EQ(seeded_result.status, hven::solvers::SolveStatus::kOptimal);

    EXPECT_LT((seeded_result.x - unseeded_result.x).lpNorm<Eigen::Infinity>(), 1e-6);
}

// f = x0^2 + x1^2 subject to x0 + x1 = 2 -- optimum (1, 1), a single equality
// row and no inequality rows. Used below to probe the seed payload directly
// (rather than through starting_multipliers()) with deliberately wrong-sized
// vectors.
struct SeedEqOnlyProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSeedSolverInf, -kSeedSolverInf;
        xu << kSeedSolverInf, kSeedSolverInf;
        gl << 2.0;
        gu << 2.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[0] + x[1] * x[1];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * x[0];
        g[1] = 2.0 * x[1];
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + x[1];
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
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
        v[1] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor;
        v[1] = 2.0 * obj_factor;
    }
    std::string name() const override { return "SeedEqOnlyProblem"; }
};

// THE MULTIPLIERS-ONLY SEED, built here (M6 W5 T8.5).
// InteriorPointSolver::set_initial_multipliers() is gone; a seed is now a
// WarmStartData whose `primal_` is EMPTY, carrying the two row blocks and the
// program's declaration stamp, handed to `solve(model, x0, warm, budget)` as an
// argument. `x0` is still the start -- which is what the old two-call shape
// meant -- and the install site, the clamps and the objective-scale handling
// are the same ones the staged seed fed.
namespace {
hven::solvers::WarmStartData seed_payload(const hven::solvers::NonLinearProgram &program,
                                          const Eigen::VectorXd &eq_mults,
                                          const Eigen::VectorXd &iq_mults) {
    hven::solvers::WarmStartData seed;
    seed.eq_lmults_ = eq_mults;
    seed.iq_lmults_ = iq_mults;
    seed.structure_key_ = hven::solvers::declaration_key(program.declaration());
    return seed;
}
} // namespace

TEST(NLPMultiplierSeedingTest, SeedSizeMismatchThrowsAndTheNextSolveIsUnaffected) {
    const auto solver_route =
        hven_interior_tests::transcribe(std::make_shared<SeedEqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }

    // SeedEqOnlyProblem has 1 equality row and 0 inequality rows; stage sizes
    // that match neither.
    Eigen::VectorXd bad_eq(2);
    bad_eq << -2.0, 0.0;
    Eigen::VectorXd bad_iq(1);
    bad_iq << 1.0;
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    // Probing the payload overload with a HAND-BUILT seed, not
    // solve_declared(): SeedEqOnlyProblem's starting_multipliers() returns
    // false, so the problem's own route would build no seed at all -- that is
    // the correct behavior for a problem that never asked to be seeded, but it
    // means this size-mismatch probe has to bypass it. The throw fires from
    // validate_staged_multipliers, right after variable-treatment
    // reconfiguration and before the entry init_impl/factorization.
    EXPECT_THROW((void)solver.solve(*solver_route.program, x0,
                                    seed_payload(*solver_route.program, bad_eq, bad_iq)),
                 std::invalid_argument);

    // NOTHING SURVIVES THE THROW (M6 W5 T8.5): the refused seed was that call's
    // own argument, so a second, seedless solve converges normally by
    // construction rather than by a disarm-on-throw discipline. The check is
    // kept because the PROPERTY is what mattered, not the mechanism.
    const hven::solvers::IpmResult r2 = solver.solve(*solver_route.program, x0);
    const Eigen::VectorXd &x = r2.x;
    ASSERT_EQ(r2.status, hven::solvers::SolveStatus::kOptimal);
    Eigen::VectorXd expect(2);
    expect << 1.0, 1.0;
    EXPECT_LT((x - expect).lpNorm<Eigen::Infinity>(), 1e-6);
}

// f = x0^2 subject to x0 >= 1 -- optimum x0 = 1, a single active lower-bound
// inequality row and no equality rows.
struct SeedLowerBoundProblem : NLPProblem {
    int num_vars() const override { return 1; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 1; }
    int num_hess_nonzeros() const override { return 1; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSeedSolverInf;
        xu << kSeedSolverInf;
        gl << 1.0;
        gu << kSeedSolverInf;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[0] * x[0]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * x[0];
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor;
    }
    std::string name() const override { return "SeedLowerBoundProblem"; }
};

// starting_multipliers() returns a positive value for the active lower-bound
// row, which is the WRONG Ipopt sign there (see
// LowerBoundedRowActiveWithNegativeIpoptMultiplier in test_ipm_solver_entry.cpp --
// the correct sign is negative). apply_starting_multipliers's
// LowerBounded-row mapping negates it (iqm = -lam), so this deliberately
// produces a negative seed on the InteriorPointSolver-internal inequality multiplier,
// which apply_staged_multipliers must clamp to kSeededIqMultFloor rather than
// installing it verbatim.
struct SeededSeedLowerBoundProblem : SeedLowerBoundProblem {
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        lambda[0] = 100.0;
        return true;
    }
    std::string name() const override { return "SeededSeedLowerBoundProblem"; }
};

TEST(NLPMultiplierSeedingTest, NegativeIqSeedIsClamped) {
    const auto solver_route =
        hven_interior_tests::transcribe(std::make_shared<SeededSeedLowerBoundProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(1);
    x0 << 3.0;
    result = hven_interior_tests::solve_declared(solver, solver_route, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_NEAR(result.x[0], 1.0, 1e-5);
}

// A problem that declines to seed produces no payload at all, so the solve
// the problem's own route runs is the cold overload. Retitled from
// `UnseededPathDoesNotConsultStaging` (M6 W5 T8.5): there is no staging left to
// consult, and what is pinned is that the unseeded path still converges.
TEST(NLPMultiplierSeedingTest, TheUnseededPathBuildsNoPayload) {
    const auto solver_route =
        hven_interior_tests::transcribe(std::make_shared<SeedEqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);

    result = hven_interior_tests::solve_declared(solver, solver_route, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    // The result is the proof: a payload was neither built nor applied.
    EXPECT_EQ(result.payload_ignored, 0);
    EXPECT_EQ(result.polish_ignored, 0);
}

// -----------------------------------------------------------------------------
// Fix round 1: (1) MakeConstraint + seeding, (2) staleness/staging
// hygiene, (3) NaN rejection + magnitude cap, (4) solve-first phase
// sequencing (SOE ignores the seed; it must reach the first OPT/OPTNO phase).
// -----------------------------------------------------------------------------

// SeededPhaseEntryEqOnlyProblem's seed reaching the OPT phase's very first
// iteration (before any Newton step) is observed through InteriorPointSolver's early
// (per-iteration) callback: XSL's KKTVector layout is documented as
// [primals | slacks | eq_lmults | iq_lmults] (kkt_vector.h). For
// SeedEqOnlyProblem specifically -- 2 unbounded primal vars (so reduced ==
// full, no fixed-variable elimination), 0 inequality rows (no slacks), 1
// equality row -- that puts the single equality multiplier at XSL[2].
struct SeededPhaseEntryEqOnlyProblem : SeedEqOnlyProblem {
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        // Deliberately NOT the true KKT multiplier (-2.0, see the EqOnly-style
        // tests elsewhere in this series): SeedEqOnlyProblem is exactly
        // quadratic/linear, so its SOE phase alone already lands exactly on
        // the true optimum with the true multiplier, and init_impl's own
        // fresh KKT solve at the inter-phase re-init would independently
        // re-derive that same -2.0 regardless of any seed. Seeding a value
        // that is otherwise impossible for InteriorPointSolver to produce here is what
        // makes "the callback observed the seed" distinguishable from "the
        // callback observed InteriorPointSolver's own correct answer by coincidence".
        lambda[0] = 7.0;
        return true;
    }
    std::string name() const override { return "SeededPhaseEntryEqOnlyProblem"; }
};

TEST(NLPMultiplierSeedingTest, SeededSolveOptimizeReachesOptPhase) {
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);

    // solve_optimize() runs SOE then OPT. The callback fires once per
    // iteration of EVERY phase and `i` resets to 0 at the start of each
    // phase's own iteration loop, so capturing (and overwriting) on every
    // i==0 leaves the LAST captured value as the OPT phase's entry value --
    // the one this test cares about -- regardless of how many iterations SOE
    // itself takes.
    double unseeded_opt_entry_eq_mult = std::numeric_limits<double>::quiet_NaN();
    {
        const auto solver_route =
            hven_interior_tests::transcribe(std::make_shared<SeedEqOnlyProblem>());
        hven::solvers::InteriorPointSolver solver;
        hven::solvers::IpmResult result;
        {
            auto o = solver.options();
            o.common.print_level = 10;
            solver.set_options(std::move(o));
        }
        solver.set_kkt_hook([&](int i, double, hven::ConstEigenRef<Eigen::VectorXd> XSL, double,
                                hven::ConstEigenRef<Eigen::VectorXd>,
                                hven::ConstEigenRef<Eigen::VectorXd>,
                                Eigen::SparseMatrix<double, Eigen::RowMajor> &) -> int {
            if (i == 0) {
                unseeded_opt_entry_eq_mult = XSL[2];
            }
            return 0;
        });
        {
            auto o = solver.options();
            o.phases = {hven::solvers::IpmPhase::kSolve,
                        hven::solvers::IpmPhase::kOptimize}; // what solve_optimize() ran
            solver.set_options(std::move(o));
        }
        result = hven_interior_tests::solve_declared(solver, solver_route, x0);
        ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    }
    ASSERT_FALSE(std::isnan(unseeded_opt_entry_eq_mult));

    double seeded_opt_entry_eq_mult = std::numeric_limits<double>::quiet_NaN();
    {
        const auto solver_route =
            hven_interior_tests::transcribe(std::make_shared<SeededPhaseEntryEqOnlyProblem>());
        hven::solvers::InteriorPointSolver solver;
        hven::solvers::IpmResult result;
        {
            auto o = solver.options();
            o.common.print_level = 10;
            solver.set_options(std::move(o));
        }
        solver.set_kkt_hook([&](int i, double, hven::ConstEigenRef<Eigen::VectorXd> XSL, double,
                                hven::ConstEigenRef<Eigen::VectorXd>,
                                hven::ConstEigenRef<Eigen::VectorXd>,
                                Eigen::SparseMatrix<double, Eigen::RowMajor> &) -> int {
            if (i == 0) {
                seeded_opt_entry_eq_mult = XSL[2];
            }
            return 0;
        });
        {
            auto o = solver.options();
            o.phases = {hven::solvers::IpmPhase::kSolve,
                        hven::solvers::IpmPhase::kOptimize}; // what solve_optimize() ran
            solver.set_options(std::move(o));
        }
        result = hven_interior_tests::solve_declared(solver, solver_route, x0);
        ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    }

    EXPECT_DOUBLE_EQ(unseeded_opt_entry_eq_mult, -2.0); // InteriorPointSolver's own correct answer
    EXPECT_DOUBLE_EQ(seeded_opt_entry_eq_mult, 7.0);    // the seed, verbatim
    EXPECT_NE(seeded_opt_entry_eq_mult, unseeded_opt_entry_eq_mult);
}

// Same objective/constraint as SeedEqOnlyProblem (x0^2 + x1^2 s.t. x0+x1=2),
// but x1 is FIXED (xl==xu==1.0). Under the MakeConstraint fixed-variable
// treatment InteriorPointSolver keeps x1 as a solver variable and adds one internal
// equality row x1-1=0 on top of the problem's own single row -- growing
// equal_cons_ to 2 while user_equal_cons_ (what starting_multipliers()/
// the declared route sees) stays at 1. That mismatch is exactly what finding 1's fix
// targets: a seed sized to the 1 user row must still be accepted, and the
// internal row zero-padded, not rejected as a size mismatch against 2.
struct SeedFixedVarEqProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSeedSolverInf, 1.0;
        xu << kSeedSolverInf, 1.0;
        gl << 2.0;
        gu << 2.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[0] + x[1] * x[1];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * x[0];
        g[1] = 2.0 * x[1];
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + x[1];
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
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
        v[1] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor;
        v[1] = 2.0 * obj_factor;
    }
    std::string name() const override { return "SeedFixedVarEqProblem"; }
};

struct SeededSeedFixedVarEqProblem : SeedFixedVarEqProblem {
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        lambda[0] = -2.0;
        return true;
    }
    std::string name() const override { return "SeededSeedFixedVarEqProblem"; }
};

TEST(NLPMultiplierSeedingTest, SeededSolveWithMakeConstraintFixedVarConverges) {
    const auto solver_route =
        hven_interior_tests::transcribe(std::make_shared<SeededSeedFixedVarEqProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        o.fixed_variable_treatment = hven::solvers::FixedVariableTreatments::MakeConstraint;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    result = hven_interior_tests::solve_declared(solver, solver_route, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    Eigen::VectorXd x = result.x;
    EXPECT_NEAR(x[0], 1.0, 1e-6);
    EXPECT_NEAR(x[1], 1.0, 1e-6);
}

// `DecliningProblemClearsStaleStaging` IS RETIRED (M6 W5 T8.5). It armed a
// poisoned seed directly on the solver, ran a solve through the wrapper for a
// problem that declines to seed, and asserted the poison never reached the
// engine -- a pin on `apply_starting_multipliers`'s early-return CLEAR. There
// is nothing left to arm: the seed is an argument built per call, and a problem
// that returns false from starting_multipliers() produces no payload for the
// solve to be handed. The hazard is unconstructible, and the test below is what
// remains to say about a declining problem.
TEST(NLPMultiplierSeedingTest, ADecliningProblemSolvesColdThroughTheWrapper) {
    const auto solver_route =
        hven_interior_tests::transcribe(std::make_shared<SeedEqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }

    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    result = hven_interior_tests::solve_declared(solver, solver_route, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(result.payload_ignored, 0);

    Eigen::VectorXd x = result.x;
    Eigen::VectorXd expect(2);
    expect << 1.0, 1.0;
    EXPECT_LT((x - expect).lpNorm<Eigen::Infinity>(), 1e-6);
}

TEST(NLPMultiplierSeedingTest, NaNSeedThrows) {
    const auto solver_route =
        hven_interior_tests::transcribe(std::make_shared<SeedEqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }

    Eigen::VectorXd eq(1);
    eq << std::numeric_limits<double>::quiet_NaN();
    Eigen::VectorXd iq(0);

    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    // Direct payload call, not solve_declared() -- see the note in
    // SeedSizeMismatchThrowsAndTheNextSolveIsUnaffected above: SeedEqOnlyProblem
    // declines to seed, so the problem's own route would build no payload at
    // all. Probing InteriorPointSolver's own validation this way is also the
    // point: it must reject a non-finite seed even from a caller that bypasses
    // starting_multiplier_seed's allFinite() guard entirely.
    //
    // THE REFUSAL MOVED UP with the payload route (M6 W5 T8.5): a non-finite
    // block is now refused at the HAND-OVER, in this frame, rather than at
    // validate_staged_multipliers inside the solve. Same exception type, same
    // call, one frame earlier.
    EXPECT_THROW(
        (void)solver.solve(*solver_route.program, x0, seed_payload(*solver_route.program, eq, iq)),
        std::invalid_argument);
}

// A seed of 1e12 into the single equality row -- SeedEqOnlyProblem is
// exactly quadratic/linear, so a plain "does it still converge" check on the
// final solution is vacuous: the first Newton step lands exactly on the
// solution regardless of the seeded multiplier, clamp deleted or not. Making
// this assertive means observing the CAPPED value directly, the same way
// SeededSolveOptimizeReachesOptPhase does.
struct SeededOversizedEqOnlyProblem : SeedEqOnlyProblem {
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        lambda[0] = 1.0e12;
        return true;
    }
    std::string name() const override { return "SeededOversizedEqOnlyProblem"; }
};

TEST(NLPMultiplierSeedingTest, OversizedSeedIsCapped) {
    const auto solver_route =
        hven_interior_tests::transcribe(std::make_shared<SeededOversizedEqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }

    double captured_eq_mult = std::numeric_limits<double>::quiet_NaN();
    solver.set_kkt_hook([&](int i, double, hven::ConstEigenRef<Eigen::VectorXd> XSL, double,
                            hven::ConstEigenRef<Eigen::VectorXd>,
                            hven::ConstEigenRef<Eigen::VectorXd>,
                            Eigen::SparseMatrix<double, Eigen::RowMajor> &) -> int {
        if (i == 0) {
            captured_eq_mult = XSL[2]; // see SeededSolveOptimizeReachesOptPhase for the layout
        }
        return 0;
    });

    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    result = hven_interior_tests::solve_declared(solver, solver_route, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_DOUBLE_EQ(captured_eq_mult, 1.0e6); // kSeededMultInitMax, not the raw 1e12 seed
}

// Same idea on the inequality side: SeedLowerBoundProblem (1 primal var, 1
// slack for the single active lower-bound row, 0 eq rows) puts the iq
// multiplier at XSL[2] (primal(1)+slack(1)+eq(0)). starting_multipliers()'s
// LowerBounded-row mapping negates lam (iqm = -lam), so seeding lam=-1e12
// arrives at InteriorPointSolver as a +1e12 iq seed, which the upper clamp (not the floor
// -- that only bites negative seeds) must cap.
struct SeededOversizedLowerBoundProblem : SeedLowerBoundProblem {
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        lambda[0] = -1.0e12;
        return true;
    }
    std::string name() const override { return "SeededOversizedLowerBoundProblem"; }
};

TEST(NLPMultiplierSeedingTest, OversizedIqSeedIsCapped) {
    const auto solver_route =
        hven_interior_tests::transcribe(std::make_shared<SeededOversizedLowerBoundProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }

    double captured_iq_mult = std::numeric_limits<double>::quiet_NaN();
    solver.set_kkt_hook([&](int i, double, hven::ConstEigenRef<Eigen::VectorXd> XSL, double,
                            hven::ConstEigenRef<Eigen::VectorXd>,
                            hven::ConstEigenRef<Eigen::VectorXd>,
                            Eigen::SparseMatrix<double, Eigen::RowMajor> &) -> int {
        if (i == 0) {
            captured_iq_mult = XSL[2];
        }
        return 0;
    });

    Eigen::VectorXd x0(1);
    x0 << 3.0;
    result = hven_interior_tests::solve_declared(solver, solver_route, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_DOUBLE_EQ(captured_iq_mult, 1.0e6);
}

// The solver's multiplier vector is longer than the problem's own row count
// whenever the fixed-variable treatment turns a fixed variable into an
// equality row: the engine appends its row after the transcribed ones. Two
// places depend on reading only the leading block -- the host, which hands the
// model its own rows when the Hessian owner runs, and compose_user_multipliers,
// which reports in the problem's declared row space. A solve that converges
// exercises the first; the assertions below exercise the second.
TEST(NLPMultiplierSeedingTest, FixedVariableConstraintRowStaysOutOfTheReportedMultipliers) {
    const auto solver_route =
        hven_interior_tests::transcribe(std::make_shared<SeedFixedVarEqProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        o.fixed_variable_treatment = hven::solvers::FixedVariableTreatments::MakeConstraint;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    result = hven_interior_tests::solve_declared(solver, solver_route, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

    Eigen::VectorXd x = result.x;
    EXPECT_NEAR(x[0], 1.0, 1e-6);
    EXPECT_NEAR(x[1], 1.0, 1e-6);

    // The engine really did add a row, and the row is REPORTED SEPARATELY
    // (M6 W5 T8.4): the base's equality block is the DECLARED one exactly, and
    // the treatment's own fixing row lands in internal_fixed_lambda_e. Before
    // T8.4 the block handed to this class was the engine's, one row longer than
    // the problem's, and this test asserted that length; the split is what
    // makes the report below correct by construction rather than by a trim.
    ASSERT_EQ(solver_route.model->me(), 1);
    EXPECT_EQ(result.lambda_e.size(), solver_route.model->me());
    ASSERT_EQ(result.internal_fixed_lambda_e.size(), 1)
        << "MakeConstraint must report the fixing row it added";
    ASSERT_EQ(result.internal_fixed_ce.size(), 1);
    EXPECT_NEAR(result.internal_fixed_ce[0], 0.0, 1e-9)
        << "the fixing row x1 - 1 = 0 is satisfied at the solution";

    // ...and the report is in the problem's own row space, one entry, carrying
    // the multiplier of the problem's own row and not the engine's.
    Eigen::VectorXd lambda =
        solver_route.model->compose_user_multipliers(result.lambda_e, result.lambda_i);
    ASSERT_EQ(lambda.size(), 1);
    // min x0^2 + x1^2 s.t. x0 + x1 = 2 with x1 fixed at 1: stationarity in the
    // free variable gives 2*x0 + lambda = 0 at x0 = 1.
    EXPECT_NEAR(lambda[0], -2.0, 1e-6);
    EXPECT_NEAR(lambda[0], result.lambda_e[0], 1e-12);
}
