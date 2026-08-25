// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// InteriorPointSolver::export_warm_start / stage_warm_start -- the engine's
// half of the M5 warm-start currency. What is pinned here: the no-completed-
// solve refusal, the declared-space mapping in both directions, the stamp
// captured AS OF the exporting solve, the staging-time and solve-entry stamp
// checks, one-shot consumption, and R5 determinism.
//
// Problem structs are named with a Warm* prefix: the unity build merges test
// TUs, so file-scope names must not collide with the other suites here.

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include <fmt/format.h>

#include "hven/drivers/interior_point_solver.h"
#include "hven/model/nlp_solver.h"
#include "hven/warmstart/warm_start_data.h"

using hven::ConstEigenRef;
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

    EXPECT_TRUE(warm.structure_key_ == solver.nlp_->model_structure_key());
    EXPECT_EQ(warm.primal_.size(), 3);
    EXPECT_EQ(warm.bound_lmults_.size(), 3);
    EXPECT_EQ(warm.eq_lmults_.size(), 1);
    EXPECT_EQ(warm.iq_lmults_.size(), 0);
    EXPECT_TRUE(warm.extensions_.empty());

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
}

// The stamp is the one the SOLVE ran under, not the one standing at export: a
// re-lay in between must not restamp blocks it never saw.
TEST(IpmWarmStart, TheStampIsCapturedAtSolveCompletionNotAtExport) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const auto solved_key = solver.nlp_->model_structure_key();

    // A genuine re-lay that moves the key: a previously infinite side of a
    // variable becomes finite, which is one of the two changes the bound
    // conjunct is defined to notice.
    solver.nlp_->set_variable_bound(0, -10.0, 10.0);
    solver.nlp_->make_nlp(2, 1, 0);
    const auto relaid_key = solver.nlp_->model_structure_key();
    ASSERT_FALSE(relaid_key == solved_key) << "the re-lay must have moved the structural key";

    const WarmStartData warm = solver.optimizer_->export_warm_start();
    EXPECT_TRUE(warm.structure_key_ == solved_key);
    EXPECT_FALSE(warm.structure_key_ == relaid_key);
}

// --- Staging refusals ---

TEST(IpmWarmStart, StagingRefusesAStampFromABeforeStructure) {
    NLPSolver solver(std::make_shared<WarmEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    solver.transcribe();

    ASSERT_EQ(warm_optimize(*solver.optimizer_, warm_eq_start()),
              hven::ConvergenceFlags::CONVERGED);
    const WarmStartData warm = solver.optimizer_->export_warm_start();

    solver.nlp_->set_variable_bound(0, -10.0, 10.0);
    solver.nlp_->make_nlp(2, 1, 0);
    const auto live_key = solver.nlp_->model_structure_key();
    ASSERT_FALSE(warm.structure_key_ == live_key);

    try {
        solver.optimizer_->stage_warm_start(warm);
        FAIL() << "staging a payload from a different structure must refuse";
    } catch (const std::invalid_argument &e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find(fmt::format("{:#x}", warm.structure_key_.digest())), std::string::npos)
            << msg;
        EXPECT_NE(msg.find(fmt::format("{:#x}", live_key.digest())), std::string::npos) << msg;
    }
    EXPECT_FALSE(solver.optimizer_->warm_staged_);
}

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

// --- The solve-entry re-check ---

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
    const auto live_key = solver.nlp_->model_structure_key();

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
    const auto key_before = solver.nlp_->model_structure_key();

    solver.optimizer_->stage_warm_start(warm);

    // A genuine re-lay that leaves every conjunct of the key where it was: the
    // renegotiation adopts the count already in force.
    solver.nlp_->negotiate_partition_count(1);
    ASSERT_TRUE(solver.nlp_->model_structure_key() == key_before);

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
        ASSERT_TRUE(fresh.nlp_->model_structure_key() == warm.structure_key_)
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

// RECORDED BEHAVIOUR, not an endorsement of it. The MakeParameter treatment
// RE-LAYS the program (it renumbers the claim stream into the reduced column
// space), so a program that has solved once carries a different structural key
// from the same declaration freshly transcribed and not yet configured for any
// treatment. The stamp is defined to carry the treatment conjunct, so the
// refusal is consistent -- but the practical consequence is that a payload
// exported from an eliminating solve does not stage into a cold engine until
// that engine's program has been laid under the same treatment. Pinned so the
// fact is visible rather than discovered downstream.
TEST(IpmWarmStart, AnEliminatingExportDoesNotStageIntoAnUnconfiguredProgram) {
    NLPSolver solved(std::make_shared<WarmFixedVarProblem>());
    solved.optimizer_->set_print_level(10);
    solved.transcribe();
    const auto key_before_any_solve = solved.nlp_->model_structure_key();

    Eigen::VectorXd x0(3);
    x0 << 2.0, -1.0, 0.25;
    ASSERT_EQ(warm_optimize(*solved.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);
    ASSERT_TRUE(solved.nlp_->is_reduced());
    EXPECT_FALSE(solved.nlp_->model_structure_key() == key_before_any_solve)
        << "the eliminating treatment re-lays, and a re-lay moves the key";

    const WarmStartData warm = solved.optimizer_->export_warm_start();

    NLPSolver fresh(std::make_shared<WarmFixedVarProblem>());
    fresh.optimizer_->set_print_level(10);
    fresh.transcribe();
    EXPECT_THROW(fresh.optimizer_->stage_warm_start(warm), std::invalid_argument);

    // One solve is all it takes to put the fresh engine's program in the state
    // the payload was taken under, and the staging then stands.
    ASSERT_EQ(warm_optimize(*fresh.optimizer_, x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_NO_THROW(fresh.optimizer_->stage_warm_start(warm));
    EXPECT_TRUE(fresh.optimizer_->warm_staged_);
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
