// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The structure epoch as the interior engine's re-analysis signal, and as the
// gate on the per-factorization pattern guard.
//
// ONE MECHANISM, TWO CONSEQUENCES. The solver records the structure epoch its
// KKT sparsity analysis was laid against. When that epoch has moved by the
// next solve entry, the analysis is redone; while it has not, every numeric
// factorization may declare the assembly buffer's pattern to be the analyzed
// one instead of re-deriving that fact with an O(nnz) hash of the whole KKT
// matrix. The tests below cover both halves against the same signal.
//
// The defect the first half closes: a re-lay resets the program's KKT location
// table to -1 and drops its analyzed-destination capture, but leaves the
// fixed-variable treatment's own validity flag standing. The solver used to
// re-analyze only when the treatment call reported a rebuild, and that call
// short-circuits whenever treatment, relax factor and bounds revision are
// unchanged -- which is exactly the case after a partition renegotiation. The
// next solve then scattered through a location table of -1s.

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <Eigen/Core>

#include "hven/drivers/interior_point_solver.h"
#include "hven/model/nlp_solver.h"

using hven::ConstEigenRef;
using hven::solvers::NLPProblem;
using hven::solvers::NLPSolver;

// A small bound-bearing problem: minimize 0.5*|x|^2 subject to sum(x) == 3,
// with a two-sided box on every variable. Bounds are what put the solver on the
// path the defect corrupts -- the bound-fixed classification runs, the location
// table is consulted for the solver's own coefficient block, and the scatter
// reads it unchecked.
struct EpochGateBoxedProblem : NLPProblem {
    static constexpr int kN = 4;

    int num_vars() const override { return kN; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return kN; }
    int num_hess_nonzeros() const override { return kN; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(-2.0);
        xu.setConstant(2.0);
        gl.setConstant(3.0);
        gu.setConstant(3.0);
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
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(1.0);
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "EpochGateBoxed"; }
};

namespace {

Eigen::VectorXd epoch_gate_start_point() {
    Eigen::VectorXd x0(EpochGateBoxedProblem::kN);
    x0 << 0.4, 0.9, -0.3, 1.1;
    return x0;
}

// Every location the user pieces scatter through. A re-lay leaves all of them
// at -1; an analysis fills each one with an offset into the assembly buffer's
// value array.
bool epoch_gate_all_locations_unset(hven::solvers::NonLinearProgram &nlp) {
    const auto locations = nlp.get_kkt_locations();
    if (locations.size() == 0) {
        return false;
    }
    return (locations.array() == -1).all();
}

bool epoch_gate_no_location_unset(hven::solvers::NonLinearProgram &nlp) {
    const auto locations = nlp.get_kkt_locations();
    return locations.size() > 0 && (locations.array() >= 0).all();
}

} // namespace

// PIN 1 -- the defect. A partition renegotiation re-lays the structures while
// leaving treatment, relax factor and bounds revision exactly as they were, so
// the treatment call the old gate read reports no change. The re-analysis has
// to happen anyway, and this is the sequence that proves it does.
TEST(StructureEpochGating, APartitionRenegotiationBetweenSolvesForcesAFreshAnalysis) {
    NLPSolver solver(std::make_shared<EpochGateBoxedProblem>());
    solver.optimizer_->set_print_level(3);
    const Eigen::VectorXd x0 = epoch_gate_start_point();

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    const Eigen::VectorXd first_x = solver.return_x();
    const hven::Index analyses_after_first = solver.optimizer_->kkt_analysis_count();
    const hven::solvers::StructureEpoch epoch_after_first = solver.nlp_->structure_epoch();

    ASSERT_TRUE(epoch_gate_no_location_unset(*solver.nlp_))
        << "a solved program must carry a filled location table";

    // The renegotiation. It adopts the count already in force, so nothing about
    // the problem's size, bounds or treatment moves -- and it still re-lays.
    solver.nlp_->negotiate_partition_count(1);

    EXPECT_FALSE(solver.nlp_->structure_epoch() == epoch_after_first)
        << "a re-lay is a structural event and must move the epoch";
    EXPECT_TRUE(epoch_gate_all_locations_unset(*solver.nlp_))
        << "a re-lay resets every KKT location to -1; this is the state the next solve "
           "would otherwise scatter through";

    // THE OLD GATE'S OWN SIGNAL, read here so the mechanism is on the record:
    // the treatment call reports NO change across the renegotiation, which is
    // why reading it alone left the table above standing. The call is
    // idempotent, so asking costs the program nothing.
    EXPECT_FALSE(solver.nlp_->configure_variable_treatment(
        solver.optimizer_->settings().fixed_variable_treatment_,
        solver.optimizer_->settings().bound_relax_factor_))
        << "the treatment call cannot see a re-lay it did not perform";

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);

    EXPECT_GT(solver.optimizer_->kkt_analysis_count(), analyses_after_first)
        << "the moved epoch must have driven a fresh sparsity analysis";
    EXPECT_TRUE(epoch_gate_no_location_unset(*solver.nlp_))
        << "the fresh analysis must have refilled the location table";

    // And the solve that ran over the refilled table is the same solve.
    const Eigen::VectorXd second_x = solver.return_x();
    ASSERT_EQ(second_x.size(), first_x.size());
    for (int i = 0; i < second_x.size(); i++) {
        EXPECT_DOUBLE_EQ(second_x[i], first_x[i]);
    }
}

// PIN 2 -- the healthy path. Nothing structural happened between the two
// solves, so no analysis is redone and the second solve reproduces the first
// bit for bit.
TEST(StructureEpochGating, ASecondSolveAgainstUnmovedStructuresRunsNoFreshAnalysis) {
    NLPSolver solver(std::make_shared<EpochGateBoxedProblem>());
    solver.optimizer_->set_print_level(3);
    const Eigen::VectorXd x0 = epoch_gate_start_point();

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    const Eigen::VectorXd first_x = solver.return_x();
    const double first_obj = solver.optimizer_->result().obj_val_;
    const Eigen::VectorXd first_eq = solver.optimizer_->result().eq_lmults_;
    const hven::Index analyses_after_first = solver.optimizer_->kkt_analysis_count();
    const hven::solvers::StructureEpoch epoch_after_first = solver.nlp_->structure_epoch();

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);

    EXPECT_TRUE(solver.nlp_->structure_epoch() == epoch_after_first)
        << "a solve is not a structural event";
    EXPECT_EQ(solver.optimizer_->kkt_analysis_count(), analyses_after_first)
        << "an unmoved epoch must not trigger a re-analysis";

    const Eigen::VectorXd second_x = solver.return_x();
    ASSERT_EQ(second_x.size(), first_x.size());
    for (int i = 0; i < second_x.size(); i++) {
        EXPECT_DOUBLE_EQ(second_x[i], first_x[i]);
    }
    EXPECT_DOUBLE_EQ(solver.optimizer_->result().obj_val_, first_obj);
    ASSERT_EQ(solver.optimizer_->result().eq_lmults_.size(), first_eq.size());
    for (int i = 0; i < first_eq.size(); i++) {
        EXPECT_DOUBLE_EQ(solver.optimizer_->result().eq_lmults_[i], first_eq[i]);
    }
}

// PIN 3 -- the same signal on the factorization side. While the epoch stands,
// every numeric factorization declares the pattern rather than re-hashing the
// whole KKT matrix to rediscover it, and the linear layer's own counters are
// what show that the guard was skipped rather than merely believed skipped.
TEST(StructureEpochGating, AWholeSolveRunsNoFullKktPatternHash) {
    NLPSolver solver(std::make_shared<EpochGateBoxedProblem>());
    solver.optimizer_->set_print_level(3);

    ASSERT_EQ(solver.optimize(epoch_gate_start_point()), hven::ConvergenceFlags::CONVERGED);

    const auto &counters = solver.optimizer_->kkt_factor_counters();
    EXPECT_GT(counters.factorize_count, 0) << "the solve must have factorized something";
    EXPECT_EQ(counters.analyze_count, 1) << "one backend symbolic per analysis, and one analysis";
    EXPECT_EQ(counters.pattern_verify_count, 0)
        << "every factorization ran under an epoch the analysis was laid against, so none of "
           "them had a pattern to re-derive";
}

// The gate is a gate, not a removal: between a re-lay and the analysis that
// answers it, the solver does not vouch for the buffer's pattern, and a
// factorization taken there would verify rather than declare. The solve entry
// closes that span by re-analyzing.
TEST(StructureEpochGating, TheEpochStopsVouchingForThePatternAcrossARelay) {
    NLPSolver solver(std::make_shared<EpochGateBoxedProblem>());
    solver.optimizer_->set_print_level(3);
    const Eigen::VectorXd x0 = epoch_gate_start_point();

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    ASSERT_EQ(solver.optimizer_->kkt_factor_counters().pattern_verify_count, 0);

    // Nothing re-analyzes here, so the solver is left holding an analysis whose
    // epoch has moved out from under it -- the state the gate has to notice.
    solver.nlp_->negotiate_partition_count(1);
    EXPECT_FALSE(solver.optimizer_->kkt_pattern_is_analyzed())
        << "a moved epoch must leave the buffer's pattern unvouched-for";

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_TRUE(solver.optimizer_->kkt_pattern_is_analyzed())
        << "the solve-entry re-analysis re-establishes the epoch";
}
