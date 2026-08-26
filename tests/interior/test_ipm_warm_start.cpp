// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// InteriorPointSolver::export_warm_start / stage_warm_start -- the engine's
// half of the M5 warm-start currency. What is pinned here: the no-completed-
// solve refusal, the declared-space mapping in both directions, the stamp
// captured AS OF the exporting solve, the staging-time and solve-entry stamp
// checks, one-shot consumption, and R5 determinism.
//
// The W2 section at the bottom adds the "hven.ipm.polish.v1" extension: the
// export that produces it, the staging that parses it (and refuses a corrupt
// one loudly, naming the tag), the bound-dual seed it delivers, and the
// crossover bridge it feeds.
//
// Problem structs are named with a Warm* prefix: the unity build merges test
// TUs, so file-scope names must not collide with the other suites here.

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include <fmt/format.h>

#include "hven/drivers/interior_point_solver.h"
#include "hven/model/nlp_solver.h"
#include "hven/warmstart/ipm_polish_extension.h"
#include "hven/warmstart/warm_start_data.h"

using hven::ConstEigenRef;
using hven::solvers::declaration_key;
using hven::solvers::NLPProblem;
using hven::solvers::NLPSolver;
using hven::solvers::WarmStartData;

namespace {
constexpr double kWarmInf = std::numeric_limits<double>::infinity();
} // namespace

// min 0.5*(x0^2 + x1^2) s.t. x0 + x1 = 2. Optimum (1, 1), one equality row, no
// inequality rows and NO finite variable bounds -- which is what makes it the
// right instrument for the start-point pins: with no bound set the solve's
// interior push is a no-op, so the first iterate's primal block is exactly the
// vector handed in.
struct WarmEqOnlyProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kWarmInf, -kWarmInf;
        xu << kWarmInf, kWarmInf;
        gl << 2.0;
        gu << 2.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * (x[0] * x[0] + x[1] * x[1]);
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0];
        g[1] = x[1];
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
        v[0] = obj_factor;
        v[1] = obj_factor;
    }
    std::string name() const override { return "WarmEqOnlyProblem"; }
};

// WarmEqOnlyProblem widened to three variables, so a solver re-bound onto it
// carries a different declared structure (and therefore a different stamp).
struct WarmWiderProblem : NLPProblem {
    int num_vars() const override { return 3; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 3; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(-kWarmInf);
        xu.setConstant(kWarmInf);
        gl << 2.0;
        gu << 2.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * x.squaredNorm();
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g = x;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x.sum();
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0;
        c << 0, 1, 2;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 2;
        c << 0, 1, 2;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(1.0);
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "WarmWiderProblem"; }
};

// A MakeParameter-treatment problem: x2 is declared bound-FIXED at 0.25, so
// the default treatment eliminates it from the solved system; x0 carries a
// two-sided box whose LOWER bound is active at the optimum, so the solve has a
// nonzero bound multiplier to report in the reduced space; x1 is free.
//
// min 0.5*((x0+1)^2 + x1^2 + x2^2) s.t. x1 + x2 = 1, x0 in [0, 5], x2 == 0.25.
// Optimum: x2 = 0.25 (held), x1 = 0.75, x0 = 0 (at its lower bound, z0 = 1).
struct WarmFixedVarProblem : NLPProblem {
    int num_vars() const override { return 3; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 0.0, -kWarmInf, 0.25;
        xu << 5.0, kWarmInf, 0.25;
        gl << 1.0;
        gu << 1.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * ((x[0] + 1.0) * (x[0] + 1.0) + x[1] * x[1] + x[2] * x[2]);
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + 1.0;
        g[1] = x[1];
        g[2] = x[2];
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[1] + x[2];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 1, 2;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 2;
        c << 0, 1, 2;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
        v[1] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "WarmFixedVarProblem"; }
};

namespace {

Eigen::VectorXd warm_eq_start() {
    Eigen::VectorXd x0(2);
    x0 << -3.0, 4.0;
    return x0;
}

// InteriorPointSolver's own entry points return the primal vector; the verdict
// is on the result. Wrapped so the pins below read as flag comparisons.
hven::ConvergenceFlags warm_optimize(hven::solvers::InteriorPointSolver &opt,
                                     const Eigen::VectorXd &x0) {
    opt.optimize(x0);
    return opt.result().converge_flag_;
}

// THE FIRST-ITERATE PROBE. The early callback is handed the live KKT vector
// before the first step of the first phase is computed, so its primal block at
// iteration 0 is the point the solve actually started from -- which is what
// every start-state pin below has to observe. Captured once per solve.
struct FirstIterateProbe {
    Eigen::VectorXd primal_;
    bool seen_ = false;

    void arm(hven::solvers::InteriorPointSolver &opt, int reduced_primal_vars) {
        this->primal_.resize(0);
        this->seen_ = false;
        opt.set_early_callback(
            [this, reduced_primal_vars](int iter, double, Eigen::Ref<Eigen::VectorXd> xsl, double,
                                        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
                                        Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
                if (iter == 0 && !this->seen_) {
                    this->primal_ = xsl.head(reduced_primal_vars);
                    this->seen_ = true;
                }
                return 0;
            });
    }
};

// Bit-level equality, which is what the "%a-level" pins mean: a warm start
// that reproduces a point reproduces its bits, not a neighbourhood of it.
void expect_bit_identical(const Eigen::VectorXd &a, const Eigen::VectorXd &b, const char *what) {
    ASSERT_EQ(a.size(), b.size()) << what;
    for (Eigen::Index i = 0; i < a.size(); i++) {
        EXPECT_EQ(std::bit_cast<std::uint64_t>(a[i]), std::bit_cast<std::uint64_t>(b[i]))
            << what << ", entry " << i;
    }
}

} // namespace

// --- The no-completed-solve refusal ---

TEST(IpmWarmStart, AFreshSolverCannotExport) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    EXPECT_FALSE(solver.optimizer_->solve_completed_);
    EXPECT_THROW(solver.optimizer_->export_warm_start(), std::logic_error);
}

// A solve that THREW is not a completed solve. The throw is produced by the
// existing staged-seed size validation, which fires at solve entry -- after
// the variable-treatment reconfiguration and before any phase runs.
TEST(IpmWarmStart, ASolveThatThrewIsNotACompletedSolve) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    Eigen::VectorXd bad_eq(3);
    bad_eq << 1.0, 2.0, 3.0;
    Eigen::VectorXd bad_iq(1);
    bad_iq << 1.0;
    solver.optimizer_->set_initial_multipliers(bad_eq, bad_iq);

    EXPECT_THROW(warm_optimize(*solver.optimizer_, warm_eq_start()), std::invalid_argument);

    EXPECT_FALSE(solver.optimizer_->solve_completed_);
    EXPECT_THROW(solver.optimizer_->export_warm_start(), std::logic_error);
}

// --- Export: declared space and the stamp ---

TEST(IpmWarmStart, ExportIsStampedAndDeclaredWidthOnAnEliminatingProblem) {
    NLPSolver solver(std::make_shared<WarmFixedVarProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    Eigen::VectorXd x0(3);
    x0 << 2.0, -1.0, 0.25;
    ASSERT_EQ(warm_optimize(*solver.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);

    // The treatment really did eliminate the fixed variable, so the reduced
    // space this export maps out of is genuinely narrower than the declared one.
    ASSERT_TRUE(solver.nlp_->is_reduced());
    ASSERT_EQ(solver.nlp_->reduced_primal_vars(), 2);

    const WarmStartData warm = solver.optimizer_->export_warm_start();

    EXPECT_TRUE(warm.structure_key_ == declaration_key(solver.nlp_->declaration()));
    EXPECT_EQ(warm.primal_.size(), 3);
    EXPECT_EQ(warm.bound_lmults_.size(), 3);
    EXPECT_EQ(warm.eq_lmults_.size(), 1);
    EXPECT_EQ(warm.iq_lmults_.size(), 0);
    // ONE extension: this problem HAS finite variable bounds, so the export
    // carries the polish hand-off beside the core.
    ASSERT_EQ(warm.extensions_.size(), 1u);
    EXPECT_EQ(warm.extensions_[0].tag_, std::string(hven::solvers::kIpmPolishTag));

    // The eliminated coordinate carries the value the treatment holds it at --
    // exactly, not a zero. A payload that zeroed it would not restart the same
    // point.
    EXPECT_EQ(warm.primal_[2], 0.25);
    EXPECT_NEAR(warm.primal_[0], 0.0, 1e-6);
    EXPECT_NEAR(warm.primal_[1], 0.75, 1e-6);

    // The bound block: the reduced solve reports two entries, and they land in
    // the declared coordinates the reduced->full map names. The eliminated
    // coordinate has no row in the reduced problem and so no multiplier; it
    // exports as an exact zero.
    EXPECT_EQ(warm.bound_lmults_[2], 0.0);
    EXPECT_GT(warm.bound_lmults_[0], 0.5) << "x0 sits at an active lower bound";
    EXPECT_NEAR(warm.bound_lmults_[1], 0.0, 1e-6) << "x1 is free";

    // THE EXTENSION MAPS OUT OF THE REDUCED SPACE THE SAME WAY. x0 carries a
    // TWO-SIDED box [0, 5], so its signed z above cannot say what the two sides
    // are priced at while the pair can. The eliminated coordinate has nothing
    // scattered to it: an exact zero on both sides, like the core block.
    const hven::solvers::IpmPolishData polish =
        hven::solvers::deserialize_ipm_polish(hven::solvers::find_ipm_polish(warm)->payload_);
    ASSERT_EQ(polish.z_lower_.size(), 3);
    ASSERT_EQ(polish.z_upper_.size(), 3);
    EXPECT_EQ(polish.iq_values_.size(), 0);
    EXPECT_EQ(polish.z_lower_[2], 0.0);
    EXPECT_EQ(polish.z_upper_[2], 0.0);
    EXPECT_GT(polish.z_lower_[0], 0.5) << "x0's lower side is the priced one";
    EXPECT_GT(polish.z_upper_[0], 0.0) << "and its upper side still carries a barrier price";
    EXPECT_GT(polish.mu_, 0.0);
}

// The stamp is the one the SOLVE ran under, not the one standing at export: a
// re-lay in between must not restamp blocks it never saw.
TEST(IpmWarmStart, TheStampIsCapturedAtSolveCompletionNotAtExport) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const auto solved_key = declaration_key(solver.nlp_->declaration());

    // A genuine DECLARATION change, which is the only thing the stamp is
    // defined to notice: a previously infinite side of a variable becomes
    // finite, which moves the bound conjunct.
    solver.nlp_->set_variable_bound(0, -10.0, 10.0);
    solver.nlp_->make_nlp(2, 1, 0);
    const auto relaid_key = declaration_key(solver.nlp_->declaration());
    ASSERT_FALSE(relaid_key == solved_key) << "the re-declaration must have moved the stamp";

    const WarmStartData warm = solver.optimizer_->export_warm_start();
    EXPECT_TRUE(warm.structure_key_ == solved_key);
    EXPECT_FALSE(warm.structure_key_ == relaid_key);
}

// --- Staging refusals: sizes and finiteness, and deliberately NOT the stamp ---

TEST(IpmWarmStart, StagingRefusesAMisSizedBlockNamingItAndBothCounts) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    WarmStartData warm = solver.optimizer_->export_warm_start();
    warm.eq_lmults_ = Eigen::VectorXd::Zero(4);

    try {
        solver.optimizer_->stage_warm_start(warm);
        FAIL() << "a mis-sized block must refuse";
    } catch (const std::invalid_argument &e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("eq_lmults_"), std::string::npos) << msg;
        EXPECT_NE(msg.find("4"), std::string::npos) << msg;
        EXPECT_NE(msg.find("1"), std::string::npos) << msg;
    }
    EXPECT_FALSE(solver.optimizer_->warm_staged_);
}

TEST(IpmWarmStart, StagingRefusesANonFiniteBlock) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    WarmStartData warm = solver.optimizer_->export_warm_start();
    warm.primal_[0] = std::numeric_limits<double>::quiet_NaN();

    EXPECT_THROW(solver.optimizer_->stage_warm_start(warm), std::invalid_argument);
    EXPECT_FALSE(solver.optimizer_->warm_staged_);
}

// A staging CALL clears what was staged before it, whether it is accepted or
// refused. The refused case is the one that matters: a consumer that stages
// P1, later stages a bad P2, logs the refusal and solves anyway must
// cold-start, not silently warm-start off the stale P1 -- P2 was refused on a
// SIZE complaint, so no stamp check downstream could catch it.
TEST(IpmWarmStart, ARefusedStagingClearsTheValueStagedBeforeIt) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const WarmStartData good = solver.optimizer_->export_warm_start();

    solver.optimizer_->stage_warm_start(good);
    ASSERT_TRUE(solver.optimizer_->warm_staged_);

    WarmStartData bad = good;
    bad.iq_lmults_ = Eigen::VectorXd::Zero(3); // the problem declares none
    EXPECT_THROW(solver.optimizer_->stage_warm_start(bad), std::invalid_argument);

    EXPECT_FALSE(solver.optimizer_->warm_staged_)
        << "the refused call must have cleared the value staged before it";

    // And the next solve is genuinely cold: it starts from the caller's guess,
    // not from the payload that was staged before the refusal.
    FirstIterateProbe probe;
    probe.arm(*solver.optimizer_, solver.nlp_->reduced_primal_vars());
    Eigen::VectorXd cold(2);
    cold << 12.0, -7.0;
    ASSERT_EQ(warm_optimize(*solver.optimizer_, cold), hven::ConvergenceFlags::CONVERGED);
    solver.optimizer_->disable_early_callback();

    ASSERT_TRUE(probe.seen_);
    expect_bit_identical(probe.primal_, cold, "the solve after a refused staging is cold");
}

// The seed half of the same rule.
TEST(IpmWarmStart, ARefusedStagingAlsoClearsAStagedMultiplierSeed) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    WarmStartData bad = solver.optimizer_->export_warm_start();
    bad.primal_ = Eigen::VectorXd::Zero(5);

    Eigen::VectorXd seed_eq(1);
    seed_eq << 42.0;
    solver.optimizer_->set_initial_multipliers(seed_eq, Eigen::VectorXd());
    ASSERT_TRUE(solver.optimizer_->mults_staged_);

    EXPECT_THROW(solver.optimizer_->stage_warm_start(bad), std::invalid_argument);
    EXPECT_FALSE(solver.optimizer_->mults_staged_);
    EXPECT_FALSE(solver.optimizer_->warm_staged_);
}

// --- The stamp check, which fires at solve entry and only there ---

TEST(IpmWarmStart, ARelayBetweenStagingAndSolvingRefusesAtSolveEntry) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const WarmStartData warm = solver.optimizer_->export_warm_start();

    solver.optimizer_->stage_warm_start(warm);
    ASSERT_TRUE(solver.optimizer_->warm_staged_);

    solver.nlp_->set_variable_bound(0, -10.0, 10.0);
    solver.nlp_->make_nlp(2, 1, 0);
    const auto live_key = declaration_key(solver.nlp_->declaration());

    try {
        warm_optimize(*solver.optimizer_, warm_eq_start());
        FAIL() << "a solve whose live stamp no longer matches the staged payload must refuse";
    } catch (const std::invalid_argument &e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find(fmt::format("{:#x}", warm.structure_key_.digest())), std::string::npos)
            << msg;
        EXPECT_NE(msg.find(fmt::format("{:#x}", live_key.digest())), std::string::npos) << msg;
    }
    // ONE-SHOT holds on the refusal path too: the value was this call's, and
    // it must not stay armed for an unrelated later one.
    EXPECT_FALSE(solver.optimizer_->warm_staged_);
}

// A re-bind is not a re-lay, and the staged value must survive it -- so long
// as the newly bound program carries the same declared structure. Here it does
// not, and the same refusal fires.
TEST(IpmWarmStart, ARebindToADifferentStructureRefusesAtSolveEntry) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();
    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const WarmStartData warm = solver.optimizer_->export_warm_start();
    solver.optimizer_->stage_warm_start(warm);

    NLPSolver wider(std::make_shared<WarmWiderProblem>());
    wider.optimizer_->set_print_level(10);
    wider.transcribe();
    solver.optimizer_->set_nlp(wider.nlp_);

    ASSERT_TRUE(solver.optimizer_->warm_staged_) << "a re-bind must not discard a staged value";
    EXPECT_THROW(warm_optimize(*solver.optimizer_, Eigen::VectorXd::Zero(3)),
                 std::invalid_argument);
    EXPECT_FALSE(solver.optimizer_->warm_staged_);
}

// --- Application, survival, one-shot ---

TEST(IpmWarmStart, AStagedStartSurvivesAnIdenticalStampRelayAndIsTheSolveStart) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const WarmStartData warm = solver.optimizer_->export_warm_start();
    const auto key_before = declaration_key(solver.nlp_->declaration());

    solver.optimizer_->stage_warm_start(warm);

    // A genuine re-lay that leaves the DECLARED PROBLEM where it was: the
    // renegotiation adopts the count already in force, and a partition count is
    // layout policy the stamp is defined to ignore either way.
    solver.nlp_->negotiate_partition_count(1);
    ASSERT_TRUE(declaration_key(solver.nlp_->declaration()) == key_before);

    FirstIterateProbe probe;
    probe.arm(*solver.optimizer_, solver.nlp_->reduced_primal_vars());

    // A start point deliberately far from the staged one, so "the solve started
    // from the staged value" is not something the caller's own guess could
    // have produced.
    Eigen::VectorXd cold(2);
    cold << 12.0, -7.0;
    ASSERT_EQ(warm_optimize(*solver.optimizer_, cold), hven::ConvergenceFlags::CONVERGED);

    ASSERT_TRUE(probe.seen_);
    expect_bit_identical(probe.primal_, warm.primal_, "the staged primal is the solve's start");
    EXPECT_FALSE(solver.optimizer_->warm_staged_) << "the solve consumed the staged value";
}

TEST(IpmWarmStart, StagedDataIsOneShotAndTheNextSolveIsCold) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const WarmStartData warm = solver.optimizer_->export_warm_start();

    solver.optimizer_->stage_warm_start(warm);

    FirstIterateProbe probe;
    probe.arm(*solver.optimizer_, solver.nlp_->reduced_primal_vars());
    Eigen::VectorXd cold(2);
    cold << 12.0, -7.0;
    ASSERT_EQ(warm_optimize(*solver.optimizer_, cold), hven::ConvergenceFlags::CONVERGED);
    expect_bit_identical(probe.primal_, warm.primal_, "first solve is warm");

    // Second solve, nothing re-staged: the caller's own guess is the start.
    probe.arm(*solver.optimizer_, solver.nlp_->reduced_primal_vars());
    ASSERT_EQ(warm_optimize(*solver.optimizer_, cold), hven::ConvergenceFlags::CONVERGED);
    ASSERT_TRUE(probe.seen_);
    expect_bit_identical(probe.primal_, cold, "second solve is cold");
}

// R5: applying a warm payload is non-consuming and deterministic. Two solvers
// from cold, the same payload staged into each, bit-identical first iterates --
// and the payload itself is unchanged by staging.
TEST(IpmWarmStart, StagingTheSamePayloadTwiceFromColdGivesBitIdenticalFirstIterates) {
    NLPSolver source(std::make_shared<WarmEqOnlyProblem>());
    source.optimizer_->set_print_level(10);
    source.transcribe();
    ASSERT_EQ(warm_optimize(*source.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const WarmStartData warm = source.optimizer_->export_warm_start();

    Eigen::VectorXd cold(2);
    cold << 12.0, -7.0;

    Eigen::VectorXd first_start;
    Eigen::VectorXd second_start;
    for (Eigen::VectorXd *out : {&first_start, &second_start}) {
        NLPSolver fresh(std::make_shared<WarmEqOnlyProblem>());
        fresh.optimizer_->set_print_level(10);
        fresh.transcribe();
        ASSERT_TRUE(declaration_key(fresh.nlp_->declaration()) == warm.structure_key_)
            << "the same declaration must key the same from cold";

        fresh.optimizer_->stage_warm_start(warm);
        FirstIterateProbe probe;
        probe.arm(*fresh.optimizer_, fresh.nlp_->reduced_primal_vars());
        ASSERT_EQ(warm_optimize(*fresh.optimizer_, cold), hven::ConvergenceFlags::CONVERGED);
        ASSERT_TRUE(probe.seen_);
        *out = probe.primal_;
    }

    expect_bit_identical(first_start, second_start, "R5 determinism");

    // Non-consuming: the value staged twice is still the value that was
    // exported.
    EXPECT_TRUE(warm == source.optimizer_->export_warm_start());
}

// The round trip on the same problem: the exporting solve's terminal point is
// the warm solve's start, bit for bit.
TEST(IpmWarmStart, AnExportStageRoundTripStartsAtTheExportingSolvesTerminalPoint) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const Eigen::VectorXd terminal = solver.optimizer_->result().primals_;
    const WarmStartData warm = solver.optimizer_->export_warm_start();
    expect_bit_identical(warm.primal_, terminal, "the export carries the terminal point");

    solver.optimizer_->stage_warm_start(warm);
    FirstIterateProbe probe;
    probe.arm(*solver.optimizer_, solver.nlp_->reduced_primal_vars());
    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    ASSERT_TRUE(probe.seen_);
    expect_bit_identical(probe.primal_, terminal, "the warm solve starts at that point");
}

// The declared -> reduced mapping on application, on the eliminating problem:
// the eliminated coordinate's value is IGNORED, and a solve staged with a
// deliberately wrong value there starts from exactly the same reduced point as
// one staged with the held value.
TEST(IpmWarmStart, ValuesAtEliminatedVariablesAreIgnoredOnApplication) {
    NLPSolver solver(std::make_shared<WarmFixedVarProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    Eigen::VectorXd x0(3);
    x0 << 2.0, -1.0, 0.25;
    ASSERT_EQ(warm_optimize(*solver.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);
    const WarmStartData warm = solver.optimizer_->export_warm_start();
    ASSERT_EQ(warm.primal_[2], 0.25);

    WarmStartData poisoned = warm;
    poisoned.primal_[2] = -123.5; // the eliminated coordinate

    // Both staged into the SAME solver, whose program is already laid under
    // the treatment the payload was taken under -- see the pin below for why
    // that matters.
    Eigen::VectorXd starts[2];
    const WarmStartData *payloads[2] = {&warm, &poisoned};
    FirstIterateProbe probe;
    for (int k = 0; k < 2; k++) {
        solver.optimizer_->stage_warm_start(*payloads[k]);
        probe.arm(*solver.optimizer_, solver.nlp_->reduced_primal_vars());
        ASSERT_EQ(warm_optimize(*solver.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);
        ASSERT_TRUE(probe.seen_);
        starts[k] = probe.primal_;
    }
    solver.optimizer_->disable_early_callback();

    expect_bit_identical(starts[0], starts[1],
                         "the eliminated coordinate's staged value reaches nothing");
    // And the value the treatment holds is still the one the solve reports --
    // the poisoned entry was written nowhere.
    EXPECT_EQ(solver.optimizer_->result().primals_[2], 0.25);
}

// THE BRANCH head(user_equal_cons_) EXISTS FOR, and the only treatment that
// exercises it. Under MakeConstraint a bound-fixed variable becomes an
// INTERNAL equality row appended at the TAIL of the solver's equality row
// space, so the solve reports two multipliers where the declaration has one,
// and the currency carries the declared row only. Every other pin here runs the
// default MakeParameter path, where the two counts coincide and the truncation
// is a no-op copy.
TEST(IpmWarmStart, MakeConstraintExportDropsTheTreatmentsInternalFixingRow) {
    NLPSolver solver(std::make_shared<WarmFixedVarProblem>());
    solver.optimizer_->set_print_level(10);
    solver.optimizer_->set_fixed_variable_treatment(
        hven::solvers::FixedVariableTreatments::MakeConstraint);
    solver.transcribe();

    Eigen::VectorXd x0(3);
    x0 << 2.0, -1.0, 0.25;
    ASSERT_EQ(warm_optimize(*solver.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);

    // The treatment kept the variable in the solved system and paid for it with
    // a row: no reduction, one internal fixing row on top of the user's own.
    ASSERT_FALSE(solver.nlp_->is_reduced());
    ASSERT_EQ(solver.nlp_->internal_fixed_constraints(), 1);
    ASSERT_EQ(solver.nlp_->user_equal_cons_, 1);
    ASSERT_EQ(solver.optimizer_->result().eq_lmults_.size(), 2)
        << "the solve reports the user row AND the treatment's fixing row";

    const WarmStartData warm = solver.optimizer_->export_warm_start();

    EXPECT_EQ(warm.eq_lmults_.size(), 1) << "the currency carries the DECLARED rows only";
    // ...and it is the USER's row that survived, not the treatment's. Bitwise:
    // the export is a head(), so the value is the reported one unchanged.
    EXPECT_EQ(std::bit_cast<std::uint64_t>(warm.eq_lmults_[0]),
              std::bit_cast<std::uint64_t>(solver.optimizer_->result().eq_lmults_[0]));
    // A tail() would have carried this one instead, and the two differ.
    EXPECT_NE(std::bit_cast<std::uint64_t>(warm.eq_lmults_[0]),
              std::bit_cast<std::uint64_t>(solver.optimizer_->result().eq_lmults_[1]));

    // The rest of the payload is still declared-width, and the fixed variable
    // is at its held value -- here because the fixing ROW holds it, not because
    // it was eliminated and reinserted.
    EXPECT_EQ(warm.primal_.size(), 3);
    EXPECT_EQ(warm.bound_lmults_.size(), 3);
    EXPECT_NEAR(warm.primal_[2], 0.25, 1e-9);
}

// THE PRIMARY FLOW. The MakeParameter treatment RE-LAYS the program -- it
// renumbers the claim stream into the reduced column space -- so a program that
// has solved once has a different LAYOUT key from the same declaration freshly
// transcribed. The pins below say both halves: the layout key moves across the
// treatment, and the DECLARATION key does NOT, because a treatment's row and
// column rearrangement is exactly what the declaration key excludes.
//
// The stamp is checked at solve entry, not at staging: the value is held across
// an arbitrary number of set_nlp/re-declaration calls, so the only honest thing
// to compare it against is the problem THE CONSUMING CALL binds.
//
// Staged on a fresh engine with the same settings, the value goes in and the
// first solve consumes it WARM.
TEST(IpmWarmStart, AnEliminatingExportStagesIntoAFreshEngineWithTheSameSettings) {
    NLPSolver source(std::make_shared<WarmFixedVarProblem>());
    source.optimizer_->set_print_level(10);
    source.transcribe();
    const auto layout_key_before_any_solve = source.nlp_->model_structure_key();
    const auto stamp_before_any_solve = declaration_key(source.nlp_->declaration());

    Eigen::VectorXd x0(3);
    x0 << 2.0, -1.0, 0.25;
    ASSERT_EQ(warm_optimize(*source.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);
    ASSERT_TRUE(source.nlp_->is_reduced());
    // THE LAYOUT KEY MOVES ACROSS THE TREATMENT.
    EXPECT_FALSE(source.nlp_->model_structure_key() == layout_key_before_any_solve)
        << "the eliminating treatment re-lays, and a re-lay moves the LAYOUT key";
    // THE STAMP DOES NOT: same declaration, same declared problem, same key.
    EXPECT_TRUE(declaration_key(source.nlp_->declaration()) == stamp_before_any_solve)
        << "a fixed-variable treatment rearranges the solved system, not the "
           "declared problem, so it must not move the currency's stamp";

    const WarmStartData warm = source.optimizer_->export_warm_start();

    // A cold engine on the same declaration and the same settings.
    NLPSolver fresh(std::make_shared<WarmFixedVarProblem>());
    fresh.optimizer_->set_print_level(10);
    fresh.transcribe();
    ASSERT_TRUE(declaration_key(fresh.nlp_->declaration()) == warm.structure_key_)
        << "the fresh program has not been configured for any treatment, and the "
           "stamp does not care -- it is the same declared problem";
    ASSERT_EQ(fresh.optimizer_->settings().fixed_variable_treatment_,
              source.optimizer_->settings().fixed_variable_treatment_);

    EXPECT_NO_THROW(fresh.optimizer_->stage_warm_start(warm));
    ASSERT_TRUE(fresh.optimizer_->warm_staged_);

    // And the first solve consumes it warm: it starts at the staged point, not
    // at the guess handed in.
    FirstIterateProbe probe;
    probe.arm(*fresh.optimizer_, 2);
    Eigen::VectorXd cold(3);
    cold << 4.5, -9.0, 0.25;
    ASSERT_EQ(warm_optimize(*fresh.optimizer_, cold), hven::ConvergenceFlags::CONVERGED);
    fresh.optimizer_->disable_early_callback();

    ASSERT_TRUE(probe.seen_);
    EXPECT_FALSE(fresh.optimizer_->warm_staged_) << "the solve consumed the staged value";

    // The reduced space the treatment produced holds x0 and x1, in that order.
    // x1 is FREE, so the interior push leaves it alone and it is the staged
    // value bit for bit -- that is the warm start reaching the solve.
    ASSERT_EQ(probe.primal_.size(), 2);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(probe.primal_[1]),
              std::bit_cast<std::uint64_t>(warm.primal_[1]))
        << "the free coordinate must be the staged value exactly";

    // x0 carries a two-sided box and the staged value sits ON its lower bound,
    // so the interior push moves it -- as it moves any starting point, warm or
    // cold. What is pinned is which point it was pushed FROM: the staged one,
    // not the guess handed in.
    // The push lands at l_relaxed + min(bound_push_ * max(1, |l|),
    // bound_interval_push_ * (u - l)) = -1e-8 + 1e-3, so the threshold carries
    // real headroom over the arithmetic rather than sitting on it: what is
    // being distinguished is a start near the bound from the cold guess at 4.5.
    EXPECT_LT(probe.primal_[0], 2.0e-3) << "pushed off the staged point at the lower bound";
    EXPECT_GT(probe.primal_[0], 0.0) << "and pushed strictly inside";
    EXPECT_NE(probe.primal_[0], cold[0]) << "the caller's guess did not survive the staging";

    // And once staged and consumed, the key it ran under IS the payload's.
    EXPECT_TRUE(declaration_key(fresh.nlp_->declaration()) == warm.structure_key_);
}

// THE CROSS-TREATMENT PIN. One declaration, two fixed-variable treatments.
// MakeParameter ELIMINATES the fixed variable -- the solved system is one row
// and column narrower, and the claim stream is renumbered into the reduced
// column space. MakeConstraint keeps it and APPENDS an internal equality row --
// the solved system is one row and column wider, and the equality row space
// grows. Those are two different systems, and the M4 layout key says so,
// correctly, in both directions.
//
// THEY ARE THE SAME DECLARED PROBLEM, and the currency's stamp says THAT --
// which is what lets a value exported under one treatment be staged and applied
// under the other. It is safe for the reason the declared-space convention
// exists: the blocks are stated over the declared variables and rows, and
// application ignores whatever coordinates the receiving treatment holds.
TEST(IpmWarmStart, AnExportUnderOneTreatmentStagesAndAppliesUnderAnother) {
    NLPSolver source(std::make_shared<WarmFixedVarProblem>());
    source.optimizer_->set_print_level(10);
    source.optimizer_->settings().fixed_variable_treatment_ =
        hven::solvers::FixedVariableTreatments::MakeParameter;
    source.transcribe();

    Eigen::VectorXd x0(3);
    x0 << 2.0, -1.0, 0.25;
    ASSERT_EQ(warm_optimize(*source.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);
    ASSERT_TRUE(source.nlp_->is_reduced()) << "MakeParameter must actually have eliminated";
    const WarmStartData warm = source.optimizer_->export_warm_start();

    // The receiving engine: same declaration, DIFFERENT treatment.
    NLPSolver sink(std::make_shared<WarmFixedVarProblem>());
    sink.optimizer_->set_print_level(10);
    sink.optimizer_->settings().fixed_variable_treatment_ =
        hven::solvers::FixedVariableTreatments::MakeConstraint;
    sink.transcribe();

    EXPECT_NO_THROW(sink.optimizer_->stage_warm_start(warm));

    FirstIterateProbe probe;
    probe.arm(*sink.optimizer_, sink.nlp_->primal_vars_);
    Eigen::VectorXd cold(3);
    cold << -5.0, 9.0, 0.25;
    ASSERT_EQ(warm_optimize(*sink.optimizer_, cold), hven::ConvergenceFlags::CONVERGED);

    // ACCEPTED AND APPLIED, not silently dropped: the solve consumed the value
    // and started from the staged point rather than from the caller's guess.
    EXPECT_FALSE(sink.optimizer_->warm_staged_) << "the solve consumed the staged value";
    ASSERT_TRUE(probe.seen_);
    ASSERT_GE(probe.primal_.size(), 2);
    EXPECT_NE(probe.primal_[1], cold[1]) << "the caller's guess did not survive the staging";
    EXPECT_NEAR(probe.primal_[1], warm.primal_[1], 1e-6);

    // THE TWO KEYS, side by side, on the two engines that just exchanged a
    // value: the layout keys disagree (two genuinely different solved systems)
    // and the stamps agree (one declared problem).
    EXPECT_FALSE(source.nlp_->model_structure_key() == sink.nlp_->model_structure_key())
        << "eliminating and constraining lay different systems, and the LAYOUT "
           "key is supposed to notice";
    EXPECT_TRUE(declaration_key(sink.nlp_->declaration()) == warm.structure_key_)
        << "one declared problem must key the same under every treatment -- that "
           "is what makes the hand-off above legal rather than lucky";

    // And the solve really did run under the other treatment.
    EXPECT_EQ(sink.optimizer_->result().fixed_variable_treatment_,
              hven::solvers::FixedVariableTreatments::MakeConstraint);
}

// --- Precedence over a staged multiplier seed ---

TEST(IpmWarmStart, StagingAWarmStartClearsAStagedMultiplierSeed) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();
    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const WarmStartData warm = solver.optimizer_->export_warm_start();

    Eigen::VectorXd seed_eq(1);
    seed_eq << 42.0;
    solver.optimizer_->set_initial_multipliers(seed_eq, Eigen::VectorXd());
    ASSERT_TRUE(solver.optimizer_->mults_staged_);

    solver.optimizer_->stage_warm_start(warm);
    EXPECT_FALSE(solver.optimizer_->mults_staged_) << "the warm start replaces the seed";
    EXPECT_TRUE(solver.optimizer_->warm_staged_);
}

// A seed staged AFTER a warm start is discarded unapplied at solve entry: the
// warm start's own multiplier blocks are what the solve installs, so the two
// solves below -- one with the late seed, one without -- run identically.
TEST(IpmWarmStart, ASeedStagedAfterAWarmStartIsDiscardedAtSolveEntry) {
    NLPSolver source(std::make_shared<WarmEqOnlyProblem>());
    source.optimizer_->set_print_level(10);
    source.transcribe();
    ASSERT_EQ(warm_optimize(*source.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const WarmStartData warm = source.optimizer_->export_warm_start();

    Eigen::VectorXd seed_eq(1);
    seed_eq << 500.0;

    Eigen::VectorXd without;
    Eigen::VectorXd with;
    for (int k = 0; k < 2; k++) {
        NLPSolver fresh(std::make_shared<WarmEqOnlyProblem>());
        fresh.optimizer_->set_print_level(10);
        fresh.transcribe();
        fresh.optimizer_->stage_warm_start(warm);
        if (k == 1) {
            fresh.optimizer_->set_initial_multipliers(seed_eq, Eigen::VectorXd());
        }
        ASSERT_EQ(warm_optimize(*fresh.optimizer_, warm_eq_start()),
                  hven::ConvergenceFlags::CONVERGED);
        EXPECT_FALSE(fresh.optimizer_->mults_staged_);
        (k == 0 ? without : with) = fresh.optimizer_->result().eq_lmults_;
    }

    expect_bit_identical(with, without, "a seed staged after a warm start changes nothing");
}

// ===========================================================================
// The "hven.ipm.polish.v1" extension, produced here and consumed here.
//
// The currency's signed bound block does not invert into the (z_lower, z_upper)
// pair the barrier holds, so a core-only warm start on a BOUNDED problem takes
// a fresh init_mu_-and-distance bound-dual seed. The pair travels in the polish
// extension instead, and these pins fix both directions of it plus its bridge
// onto the SQP crossover.
//
// The extension's own byte form -- round trip, frozen layout, decode refusals,
// bridge mapping -- is pinned engine-free in
// tests/warmstart/test_ipm_polish_extension.cpp. What is pinned HERE is
// everything that needs a real solve to exist.
// ===========================================================================

// Two variables, each priced at ONE bound side at the solution, and one slack
// inequality row.
//
//   min 0.5*((x0-3)^2 + (x1+2)^2)   s.t.  x0 + x1 <= 5,  x0 <= 1,  x1 >= -1
//
// Optimum (1, -1): x0 at its UPPER bound (z0 = -2, i.e. z_upper = 2), x1 at
// its LOWER bound (z1 = +1, i.e. z_lower = 1), the row slack at 0 <= 5.
//
// ONE-SIDED ON PURPOSE. Every bounded variable here carries exactly one finite
// side, so the signed z the PUBLIC SolveResult reports inverts back into the
// pair exactly -- z_lower = max(z, 0), z_upper = max(-z, 0), with no rounding
// anywhere. That is what lets the equivalence pin below reconstruct the raw
// blocks a caller would have handed the crossover directly, out of the public
// result alone, and compare bit for bit against what the extension carried.
// The genuinely two-sided case (where that inversion does NOT exist, which is
// the whole reason this extension does) is covered by the hand-built bridge
// pins in the warmstart suite.
struct WarmBoundedProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kWarmInf, -1.0;
        xu << 1.0, kWarmInf;
        gl << -kWarmInf;
        gu << 5.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * ((x[0] - 3.0) * (x[0] - 3.0) + (x[1] + 2.0) * (x[1] + 2.0));
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] - 3.0;
        g[1] = x[1] + 2.0;
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
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "WarmBoundedProblem"; }
};

namespace {

Eigen::VectorXd warm_bounded_start() {
    Eigen::VectorXd x0(2);
    x0 << -4.0, 6.0;
    return x0;
}

// Solves WarmBoundedProblem from cold and hands back the solver (still holding
// its result) alongside the value it exported.
struct WarmBoundedSolve {
    hven::solvers::NLPSolver solver_{std::make_shared<WarmBoundedProblem>()};
    WarmStartData warm_;

    WarmBoundedSolve() {
        this->solver_.optimizer_->set_print_level(10);
        this->solver_.transcribe();
        EXPECT_EQ(warm_optimize(*this->solver_.optimizer_, warm_bounded_start()),
                  hven::ConvergenceFlags::CONVERGED);
        this->warm_ = this->solver_.optimizer_->export_warm_start();
    }
};

hven::solvers::IpmPolishData polish_of(const WarmStartData &data) {
    const hven::solvers::WarmExtension *extension = hven::solvers::find_ipm_polish(data);
    EXPECT_NE(extension, nullptr);
    return hven::solvers::deserialize_ipm_polish(extension->payload_);
}

WarmStartData without_extensions(WarmStartData data) {
    data.extensions_.clear();
    return data;
}

// THE FIRST-ITERATE DUAL PROBE, and the instrument every seeding pin below
// reads. The late callback is handed each completed IterateInfo; the FIRST one
// describes the iterate the solve started from, and its kkt_inf_ is the
// solver's own dual-infeasibility measure -- the residual that folds the bound
// multipliers in through the -z term (barrier_math.h's accumulate_bound_dual_
// terms). A seeded z that is the converged one makes that residual small at
// iteration 0; the fresh mu0/distance seed does not. Counters and values only;
// nothing here reads a clock.
struct FirstIterateDualProbe {
    double kkt_inf_ = -1.0;
    int iter_ = -1;
    bool seen_ = false;

    void arm(hven::solvers::InteriorPointSolver &opt) {
        this->kkt_inf_ = -1.0;
        this->iter_ = -1;
        this->seen_ = false;
        opt.set_late_callback([this](const hven::solvers::IterateInfo &info,
                                     ConstEigenRef<Eigen::VectorXd>,
                                     ConstEigenRef<Eigen::VectorXd>) {
            if (!this->seen_) {
                this->kkt_inf_ = info.kkt_inf_;
                this->iter_ = info.iter_;
                this->seen_ = true;
            }
            return 0;
        });
    }
};

// Stages `warm` into a fresh solver, solves, and reports the first iterate's
// dual infeasibility together with the iteration count the solve took.
struct WarmRun {
    double first_kkt_inf_ = -1.0;
    int iters_ = -1;
};

WarmRun run_warm(const WarmStartData &warm) {
    hven::solvers::NLPSolver fresh(std::make_shared<WarmBoundedProblem>());
    fresh.optimizer_->set_print_level(10);
    fresh.transcribe();
    FirstIterateDualProbe probe;
    probe.arm(*fresh.optimizer_);
    fresh.optimizer_->stage_warm_start(warm);
    EXPECT_EQ(warm_optimize(*fresh.optimizer_, warm_bounded_start()),
              hven::ConvergenceFlags::CONVERGED);
    EXPECT_TRUE(probe.seen_);
    return WarmRun{probe.kkt_inf_, fresh.optimizer_->result().iter_num_};
}

} // namespace

// --- The producer ---

TEST(IpmWarmStart, ExportCarriesThePolishTagOnABoundedProblem) {
    const WarmBoundedSolve solved;
    const auto &result = solved.solver_.optimizer_->result();

    ASSERT_EQ(solved.warm_.extensions_.size(), 1u);
    EXPECT_EQ(solved.warm_.extensions_[0].tag_, std::string(hven::solvers::kIpmPolishTag));

    const hven::solvers::IpmPolishData polish = polish_of(solved.warm_);
    ASSERT_EQ(polish.z_lower_.size(), 2);
    ASSERT_EQ(polish.z_upper_.size(), 2);
    ASSERT_EQ(polish.iq_values_.size(), 1);

    // The sides that exist carry the converged prices; the sides that do not
    // exist carry an exact zero, because nothing was ever scattered there.
    EXPECT_NEAR(polish.z_upper_[0], 2.0, 1e-6);
    EXPECT_NEAR(polish.z_lower_[1], 1.0, 1e-6);
    EXPECT_EQ(polish.z_lower_[0], 0.0);
    EXPECT_EQ(polish.z_upper_[1], 0.0);

    // The barrier level the solve ended at: positive, and at or above the
    // settings floor it is allowed to reach.
    EXPECT_GT(polish.mu_, 0.0);
    EXPECT_LT(polish.mu_, 1.0);

    // AND IT AGREES WITH THE CORE, BIT FOR BIT AT THE DEFAULT OBJECTIVE SCALE.
    // Every bounded variable here is one-sided, so the signed block IS one of
    // the two pair entries, negated for an upper side -- an exact operation.
    //
    // THE SCALE QUALIFIER IS REAL: the export divides each side of the pair by
    // solve_obj_scale_ INDIVIDUALLY, while the core's signed block is divided
    // AFTER the subtraction, in unscale_reported_outputs. At obj_scale == 1
    // both paths skip the division and the identity is exact; at obj_scale != 1
    // on a genuinely two-sided bound the two may differ by an ulp.
    ASSERT_EQ(solved.warm_.bound_lmults_.size(), 2);
    Eigen::VectorXd signed_from_pair = polish.z_lower_ - polish.z_upper_;
    expect_bit_identical(signed_from_pair, solved.warm_.bound_lmults_,
                         "z_lower - z_upper against the core's signed block");

    // The inequality values are the ones the solve reported, verbatim.
    expect_bit_identical(polish.iq_values_, result.iq_cons_,
                         "the extension's inequality values against result().iq_cons_");
}

TEST(IpmWarmStart, ExportCarriesNoExtensionWhenTheProblemHasNoFiniteBounds) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();
    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);

    const WarmStartData warm = solver.optimizer_->export_warm_start();
    // Not an empty payload under the tag -- no extension at all. There is no
    // (z_lower, z_upper) pair to carry, and claiming the capability with two
    // zero vectors would say nothing while looking like a hand-off.
    EXPECT_TRUE(warm.extensions_.empty());
    EXPECT_EQ(hven::solvers::find_ipm_polish(warm), nullptr);
}

// EVERY finite variable bound this problem has sits on a BOUND-FIXED variable
// (x0 is declared at [0.25, 0.25]; x1 is free on both sides), which makes the
// fixed-variable treatment a bound-set SWITCH here rather than a
// re-partitioning: under RelaxBounds x0 keeps a widened two-sided box and the
// bound set has members, while under MakeParameter x0 leaves the solver's
// variable space entirely and the bound set is EMPTY -- on the same program,
// with no re-declaration. That is what the pin below needs.
//
// min 0.5*((x0 - 1)^2 + (x1 + 2)^2)  s.t.  x0 + x1 <= 5.
struct WarmFixedOnlyBoundProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 0.25, -kWarmInf;
        xu << 0.25, kWarmInf;
        gl << -kWarmInf;
        gu << 5.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * ((x[0] - 1.0) * (x[0] - 1.0) + (x[1] + 2.0) * (x[1] + 2.0));
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] - 1.0;
        g[1] = x[1] + 2.0;
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
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "WarmFixedOnlyBoundProblem"; }
};

// run_phase_sequence's conditional clears bounds_ on its else branch: a solve
// whose bound set came up empty must not be left pointing at the PREVIOUS
// solve's set, or the export's `if (this->bounds_)` gate claims the polish
// hand-off on a problem with no variable-bound barrier terms at all -- an
// all-zero (z_lower, z_upper) pair a consumer would stage and seed from.
//
// Both solves run on ONE InteriorPointSolver instance holding ONE program;
// only Settings::fixed_variable_treatment_ moves between them.
TEST(IpmWarmStart, ARelayThatEmptiesTheBoundSetLeavesNoBoundStoryOnTheSameInstance) {
    NLPSolver solver(std::make_shared<WarmFixedOnlyBoundProblem>());
    solver.optimizer_->set_print_level(10);
    // The widest relaxation the contract allows (1e-2), so x0's relaxed box is
    // [0.24, 0.26] -- room enough for the barrier to sit inside, rather than
    // the default 1e-8's 5e-9-wide slit.
    solver.optimizer_->set_bound_relax_factor(hven::solvers::kMaxBoundRelaxFactor);
    solver.optimizer_->set_fixed_variable_treatment(
        hven::solvers::FixedVariableTreatments::RelaxBounds);
    solver.transcribe();

    Eigen::VectorXd x0(2);
    x0 << 0.25, 1.0;

    // --- Leg 1: bounded. The bound set has members and the export says so.
    ASSERT_EQ(warm_optimize(*solver.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);
    ASSERT_TRUE(solver.nlp_->variable_bound_set().any())
        << "RelaxBounds must leave x0 as a two-sided bounded variable";
    ASSERT_EQ(solver.optimizer_->result().bound_lmults_.size(), 2);

    const WarmStartData bounded = solver.optimizer_->export_warm_start();
    ASSERT_EQ(bounded.extensions_.size(), 1u);
    EXPECT_EQ(bounded.extensions_[0].tag_, std::string(hven::solvers::kIpmPolishTag));

    // --- The re-lay: same instance, same program, no set_nlp(). The treatment
    // eliminates the only bound-carrying variable, so the bound set empties.
    solver.optimizer_->set_fixed_variable_treatment(
        hven::solvers::FixedVariableTreatments::MakeParameter);
    ASSERT_EQ(warm_optimize(*solver.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);
    ASSERT_TRUE(solver.nlp_->is_reduced());
    ASSERT_FALSE(solver.nlp_->variable_bound_set().any())
        << "MakeParameter eliminates x0, and nothing else here has a finite bound";

    // --- Leg 2: the bound story is ABSENT, in all three places it is told.
    //
    // (a) The result's own signed block is EMPTY, which is exactly what the
    //     field documents for a problem with no finite variable bounds -- not
    //     a stale two-entry vector from leg 1, and not a zero-filled one.
    EXPECT_EQ(solver.optimizer_->result().bound_lmults_.size(), 0);

    const WarmStartData unbounded = solver.optimizer_->export_warm_start();

    // (b) NO extension -- not an extension carrying zeros. Same rule
    //     ExportCarriesNoExtensionWhenTheProblemHasNoFiniteBounds pins for a
    //     problem that never had bounds; a problem that just lost them is not
    //     a different case.
    EXPECT_TRUE(unbounded.extensions_.empty());
    EXPECT_EQ(hven::solvers::find_ipm_polish(unbounded), nullptr);

    // (c) The currency's core block is still DECLARED-WIDTH -- the currency's
    //     lengths are the declared ones unconditionally -- and every entry is
    //     an exact zero, because there was no reported block to scatter.
    ASSERT_EQ(unbounded.bound_lmults_.size(), 2);
    EXPECT_EQ(unbounded.bound_lmults_[0], 0.0);
    EXPECT_EQ(unbounded.bound_lmults_[1], 0.0);
}

TEST(IpmWarmStart, ThePolishPayloadSurvivesTheCurrencysOwnRoundTripVerbatim) {
    const WarmBoundedSolve solved;
    const WarmStartData decoded =
        hven::solvers::deserialize(hven::solvers::serialize(solved.warm_));
    EXPECT_EQ(decoded, solved.warm_);
    ASSERT_EQ(decoded.extensions_.size(), 1u);
    EXPECT_EQ(decoded.extensions_[0].payload_, solved.warm_.extensions_[0].payload_);
    EXPECT_EQ(polish_of(decoded), polish_of(solved.warm_));
}

// --- Staging: the known tag is parsed, a foreign one is not ---

TEST(IpmWarmStart, StagingRefusesAMalformedPayloadUnderTheKnownTagNamingIt) {
    const WarmBoundedSolve solved;
    WarmStartData corrupt = solved.warm_;
    corrupt.extensions_[0].payload_[4] = std::byte{0xFF}; // break the magic

    NLPSolver fresh(std::make_shared<WarmBoundedProblem>());
    fresh.optimizer_->set_print_level(10);
    fresh.transcribe();
    try {
        fresh.optimizer_->stage_warm_start(corrupt);
        ADD_FAILURE() << "expected a refusal";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("stage_warm_start"), std::string::npos) << message;
        EXPECT_NE(message.find(std::string(hven::solvers::kIpmPolishTag)), std::string::npos)
            << message;
        EXPECT_NE(message.find("payload magic"), std::string::npos) << message;
    }
    // Refused, then GONE -- the staging discipline the core blocks already
    // follow applies to the extension's refusal too.
    EXPECT_FALSE(fresh.optimizer_->warm_staged_);
}

// Every PROPER prefix of the payload refuses at staging, and names the offset
// it ran out at. The codec suite covers the decoder; this pin adds that the
// ENGINE routes every one of those refusals out of the staging call rather
// than letting a short payload through to a solve.
TEST(IpmWarmStart, EveryTruncationOfThePolishPayloadRefusesAtStagingNamingTheOffset) {
    const WarmBoundedSolve solved;
    const std::vector<std::byte> &full = solved.warm_.extensions_[0].payload_;
    ASSERT_FALSE(full.empty());

    NLPSolver fresh(std::make_shared<WarmBoundedProblem>());
    fresh.optimizer_->set_print_level(10);
    fresh.transcribe();

    for (std::size_t prefix = 0; prefix < full.size(); prefix++) {
        WarmStartData truncated = solved.warm_;
        truncated.extensions_[0].payload_.assign(full.begin(),
                                                 full.begin() + static_cast<long>(prefix));
        try {
            fresh.optimizer_->stage_warm_start(truncated);
            ADD_FAILURE() << "expected a refusal at prefix " << prefix;
        } catch (const std::invalid_argument &error) {
            const std::string message = error.what();
            EXPECT_NE(message.find(std::string(hven::solvers::kIpmPolishTag)), std::string::npos)
                << "prefix " << prefix << ": " << message;
            EXPECT_NE(message.find("byte offset"), std::string::npos)
                << "prefix " << prefix << ": " << message;
        }
    }
}

TEST(IpmWarmStart, StagingRefusesAPolishBlockThatIsNotAtTheDeclaredWidth) {
    const WarmBoundedSolve solved;
    hven::solvers::IpmPolishData polish = polish_of(solved.warm_);
    polish.z_upper_ = Eigen::VectorXd::Zero(3);

    WarmStartData wide = solved.warm_;
    wide.extensions_[0].payload_ = hven::solvers::serialize_ipm_polish(polish);

    NLPSolver fresh(std::make_shared<WarmBoundedProblem>());
    fresh.optimizer_->set_print_level(10);
    fresh.transcribe();
    try {
        fresh.optimizer_->stage_warm_start(wide);
        ADD_FAILURE() << "expected a refusal";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("upper-bound multiplier block"), std::string::npos) << message;
        EXPECT_NE(message.find("holds 3"), std::string::npos) << message;
        EXPECT_NE(message.find("has 2"), std::string::npos) << message;
    }
}

// The duplicate-tag refusal, THROUGH THE ENGINE. find_ipm_polish's own unit
// test covers the refusal; this adds that staging routes it out with the entry
// prefix every other refusal from this entry carries.
TEST(IpmWarmStart, StagingRefusesThePolishTagCarriedTwiceNamingTheEntry) {
    const WarmBoundedSolve solved;
    WarmStartData twice = solved.warm_;
    twice.extensions_.push_back(solved.warm_.extensions_[0]);
    ASSERT_EQ(twice.extensions_.size(), 2u);

    NLPSolver fresh(std::make_shared<WarmBoundedProblem>());
    fresh.optimizer_->set_print_level(10);
    fresh.transcribe();
    try {
        fresh.optimizer_->stage_warm_start(twice);
        ADD_FAILURE() << "expected a refusal";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_EQ(message.rfind("InteriorPointSolver::stage_warm_start:", 0), 0u) << message;
        EXPECT_NE(message.find("more than once"), std::string::npos) << message;
        EXPECT_NE(message.find(std::string(hven::solvers::kIpmPolishTag)), std::string::npos)
            << message;
    }
    EXPECT_FALSE(fresh.optimizer_->warm_staged_);
}

TEST(IpmWarmStart, AForeignExtensionTagIsIgnoredAtStagingAndAtSolve) {
    const WarmBoundedSolve solved;
    WarmStartData foreign = solved.warm_;
    // Junk under a tag this engine does not know -- a capability downgrade,
    // not corruption, so it is skipped in silence (R3).
    foreign.extensions_.push_back(
        hven::solvers::WarmExtension{"some.other.producer", {std::byte{0xDE}, std::byte{0xAD}}});

    NLPSolver fresh(std::make_shared<WarmBoundedProblem>());
    fresh.optimizer_->set_print_level(10);
    fresh.transcribe();
    EXPECT_NO_THROW(fresh.optimizer_->stage_warm_start(foreign));
    EXPECT_EQ(warm_optimize(*fresh.optimizer_, warm_bounded_start()),
              hven::ConvergenceFlags::CONVERGED);
    EXPECT_FALSE(fresh.optimizer_->warm_staged_);
}

// --- The consumer: the pair reaches the barrier's bound multipliers ---

TEST(IpmWarmStart, ThePolishPairSeedsTheBoundDualsAndACoreOnlyValueDoesNot) {
    const WarmBoundedSolve solved;

    const WarmRun core_only = run_warm(without_extensions(solved.warm_));
    const WarmRun with_polish = run_warm(solved.warm_);

    // THE INSTRUMENT (see FirstIterateDualProbe): the solver's own dual
    // infeasibility at the FIRST iterate, which is where a bound-multiplier
    // seed either is or is not the converged one. Both runs start from the
    // same primal point -- the same payload's primal_ block, pushed into the
    // interior identically -- so the ONLY thing that differs between them is
    // the bound-dual seed the extension carries.
    EXPECT_GT(core_only.first_kkt_inf_, 0.0);
    EXPECT_LT(with_polish.first_kkt_inf_, core_only.first_kkt_inf_);
    // And it is not merely smaller: seeded with the converged pair, the first
    // iterate's dual residual is at the scale of the bound push rather than at
    // the scale of the prices themselves (which are 2.0 and 1.0 here).
    EXPECT_LT(with_polish.first_kkt_inf_, 0.5);

    // A MARGIN, not an exact count: the counter is assertable (CLAUDE.md
    // section 7), but its exact value is a property of this box's arithmetic.
    // What the seed promises is that it never costs iterations.
    EXPECT_LE(with_polish.iters_, core_only.iters_);
}

TEST(IpmWarmStart, ACoreOnlyWarmStartOnABoundedProblemStillConverges) {
    const WarmBoundedSolve solved;
    // The no-regression half of the pin above: stripping the extension leaves
    // point and constraint multipliers restarted and bound multipliers seeded
    // fresh, and that path still solves.
    const WarmRun core_only = run_warm(without_extensions(solved.warm_));
    EXPECT_GT(core_only.iters_, 0);
}

// --- The bridge, against a real solve ---

TEST(IpmWarmStart, TheBridgeEqualsTheCrossoverHandedTheSolvesOwnRawBlocks) {
    const WarmBoundedSolve solved;
    const auto &result = solved.solver_.optimizer_->result();

    Eigen::VectorXd lower(2), upper(2);
    lower << -kWarmInf, -1.0;
    upper << 1.0, kWarmInf;

    const hven::solvers::WarmStart bridged =
        hven::solvers::to_sqp_warm_start(solved.warm_, lower, upper, solved.warm_.structure_key_);

    // THE RAW BLOCKS A CALLER WOULD HAVE HANDED OVER DIRECTLY, taken from the
    // PUBLIC SolveResult and nowhere else: the primal point, the two
    // multiplier blocks, the inequality values, and the pair recovered from
    // the signed block by the exact one-sided inversion this fixture makes
    // available (see WarmBoundedProblem's own note).
    const Eigen::VectorXd z = result.bound_lmults_;
    const Eigen::VectorXd z_lower = z.cwiseMax(0.0);
    const Eigen::VectorXd z_upper = (-z).cwiseMax(0.0);
    const hven::solvers::WarmStart direct =
        hven::solvers::from_interior_point(result.primals_, result.eq_lmults_, result.iq_lmults_,
                                           result.iq_cons_, z_lower, z_upper, lower, upper);

    // BIT-EXACT: from_interior_point is deterministic and both calls reach it
    // with the same doubles.
    expect_bit_identical(bridged.x, direct.x, "crossover x");
    expect_bit_identical(bridged.lambda_e, direct.lambda_e, "crossover lambda_e");
    expect_bit_identical(bridged.lambda_i, direct.lambda_i, "crossover lambda_i");
    expect_bit_identical(bridged.z, direct.z, "crossover z");
    EXPECT_EQ(bridged.ineq_active, direct.ineq_active);
    EXPECT_EQ(bridged.bound_active, direct.bound_active);
    EXPECT_EQ(bridged.qp_working_set.bound_state(), direct.qp_working_set.bound_state());
    EXPECT_EQ(bridged.qp_working_set.active_ineq(), direct.qp_working_set.active_ineq());
    EXPECT_EQ(bridged.structure_hash, direct.structure_hash);
    EXPECT_EQ(bridged.valid, direct.valid);
    EXPECT_EQ(bridged.hot, direct.hot);

    // And the crossover read the problem the way the solve did: x0 sits at its
    // UPPER bound, x1 at its LOWER one.
    ASSERT_EQ(bridged.bound_active.size(), 2u);
    EXPECT_EQ(bridged.bound_active[0], 1);
    EXPECT_EQ(bridged.bound_active[1], -1);
}

TEST(IpmWarmStart, TheBridgeRefusesTheStampOfADifferentStructure) {
    const WarmBoundedSolve solved;
    Eigen::VectorXd lower(2), upper(2);
    lower << -kWarmInf, -1.0;
    upper << 1.0, kWarmInf;

    hven::solvers::DeclarationKey other = solved.warm_.structure_key_;
    other.declaration_digest_ += 1;
    EXPECT_THROW(hven::solvers::to_sqp_warm_start(solved.warm_, lower, upper, other),
                 std::invalid_argument);
}

// --- R6: cold-vs-hot is never answer-observable ---
//
// THE ONLY HOT STATE THIS ENGINE CARRIES ACROSS SOLVES is the epoch-gated KKT
// sparsity analysis (tests/interior/test_structure_epoch_gating.cpp pins the
// gate itself). What is pinned here is the consequence of that reuse: a second
// solve that SKIPS the analysis must answer exactly what a fresh engine that
// PAID it answers, bit for bit. Epoch-keyed reuse is answer-neutral by
// construction -- same declared pattern, same symbolic -- so this is an
// EXACT-EQUALITY pin, not a tolerance one.
//
// NOTHING HERE READS A CLOCK. Wall time may differ freely between the two
// solves; it is asserted nowhere.

namespace {

// Every ANSWER a SolveResult reports, taken off a finished call.
struct WarmAnswer {
    hven::ConvergenceFlags flag_ = hven::ConvergenceFlags::NOTCONVERGED;
    int iters_ = -1;
    double obj_ = 0.0;
    double kkt_inf_ = 0.0;
    double barr_inf_ = 0.0;
    double econ_inf_ = 0.0;
    double icon_inf_ = 0.0;
    Eigen::VectorXd primals_, eq_lmults_, iq_lmults_, bound_lmults_, eq_cons_, iq_cons_;
};

WarmAnswer answer_of(const hven::solvers::InteriorPointSolver &opt) {
    const auto &r = opt.result();
    WarmAnswer a;
    a.flag_ = r.converge_flag_;
    a.iters_ = r.iter_num_;
    a.obj_ = r.obj_val_;
    a.kkt_inf_ = r.kkt_inf_;
    a.barr_inf_ = r.barr_inf_;
    a.econ_inf_ = r.econ_inf_;
    a.icon_inf_ = r.icon_inf_;
    a.primals_ = r.primals_;
    a.eq_lmults_ = r.eq_lmults_;
    a.iq_lmults_ = r.iq_lmults_;
    a.bound_lmults_ = r.bound_lmults_;
    a.eq_cons_ = r.eq_cons_;
    a.iq_cons_ = r.iq_cons_;
    return a;
}

void expect_same_answer(const WarmAnswer &hot, const WarmAnswer &cold) {
    EXPECT_EQ(hot.flag_, cold.flag_);
    EXPECT_EQ(hot.iters_, cold.iters_);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(hot.obj_), std::bit_cast<std::uint64_t>(cold.obj_));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(hot.kkt_inf_),
              std::bit_cast<std::uint64_t>(cold.kkt_inf_));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(hot.barr_inf_),
              std::bit_cast<std::uint64_t>(cold.barr_inf_));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(hot.econ_inf_),
              std::bit_cast<std::uint64_t>(cold.econ_inf_));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(hot.icon_inf_),
              std::bit_cast<std::uint64_t>(cold.icon_inf_));
    expect_bit_identical(hot.primals_, cold.primals_, "R6 primals");
    expect_bit_identical(hot.eq_lmults_, cold.eq_lmults_, "R6 eq_lmults");
    expect_bit_identical(hot.iq_lmults_, cold.iq_lmults_, "R6 iq_lmults");
    expect_bit_identical(hot.bound_lmults_, cold.bound_lmults_, "R6 bound_lmults");
    expect_bit_identical(hot.eq_cons_, cold.eq_cons_, "R6 eq_cons");
    expect_bit_identical(hot.iq_cons_, cold.iq_cons_, "R6 iq_cons");
}

} // namespace

TEST(IpmWarmStart, AnUnchangedEpochResolveAnswersExactlyWhatAFreshEngineAnswers) {
    NLPSolver reused(std::make_shared<WarmBoundedProblem>());
    reused.optimizer_->set_print_level(10);
    reused.transcribe();

    ASSERT_EQ(warm_optimize(*reused.optimizer_, warm_bounded_start()),
              hven::ConvergenceFlags::CONVERGED);
    const hven::Index analyses_after_first = reused.optimizer_->kkt_analysis_count();
    const hven::solvers::StructureEpoch epoch_after_first = reused.nlp_->structure_epoch();
    ASSERT_GT(analyses_after_first, 0) << "the first solve must have laid and analyzed a pattern";

    // THE HOT SOLVE. Same instance, same problem, same start point, nothing
    // staged -- so the only thing that differs from the first call is the
    // engine's own carried state.
    ASSERT_EQ(warm_optimize(*reused.optimizer_, warm_bounded_start()),
              hven::ConvergenceFlags::CONVERGED);

    // The reuse actually happened: the epoch did not move, and no second
    // analysis was paid. Asserted through the existing counter rather than
    // inferred from timing -- counters are the currency here.
    EXPECT_TRUE(reused.nlp_->structure_epoch() == epoch_after_first)
        << "nothing structural happened between the two solves";
    EXPECT_EQ(reused.optimizer_->kkt_analysis_count(), analyses_after_first)
        << "an unchanged epoch must not re-analyze; without this the pin below proves nothing";
    const WarmAnswer hot = answer_of(*reused.optimizer_);

    // THE COLD SOLVE, on a fresh engine over a fresh program: it pays the
    // analysis the second solve above skipped, and must land on the same bits.
    NLPSolver fresh(std::make_shared<WarmBoundedProblem>());
    fresh.optimizer_->set_print_level(10);
    fresh.transcribe();
    ASSERT_EQ(warm_optimize(*fresh.optimizer_, warm_bounded_start()),
              hven::ConvergenceFlags::CONVERGED);
    EXPECT_GT(fresh.optimizer_->kkt_analysis_count(), 0);

    expect_same_answer(hot, answer_of(*fresh.optimizer_));
}
