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

// The defect. A partition renegotiation re-lays the structures while
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

// The healthy path. Nothing structural happened between the two
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

// The same signal on the factorization side. While the epoch stands,
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

// The skip is not unconditional, and the condition is the callback. The early
// callback holds the assembly buffer by mutable reference, so an edit made
// there re-patterns the buffer without re-laying the model -- the one route by
// which the epoch can stand over a pattern that is no longer the analyzed one.
// A call that hands the matrix out therefore re-derives the pattern at every
// factorization, for the whole call, and a call that does not keeps the skip.
TEST(StructureEpochGating, ASolveThatHandsOutTheKktMatrixVerifiesThePatternThroughout) {
    NLPSolver with_callback(std::make_shared<EpochGateBoxedProblem>());
    with_callback.optimizer_->set_print_level(3);
    int callback_calls = 0;
    with_callback.optimizer_->set_early_callback(
        [&](int, double, hven::EigenRef<Eigen::VectorXd>, double, hven::EigenRef<Eigen::VectorXd>,
            hven::EigenRef<Eigen::VectorXd>, Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
            ++callback_calls;
            return 0;
        });

    ASSERT_EQ(with_callback.optimize(epoch_gate_start_point()), hven::ConvergenceFlags::CONVERGED);
    ASSERT_GT(callback_calls, 0) << "the callback never ran, so nothing was handed out";

    const auto &guarded = with_callback.optimizer_->kkt_factor_counters();
    EXPECT_GT(guarded.pattern_verify_count, 0)
        << "a call that hands the matrix out must re-derive the pattern it factorizes";
    EXPECT_TRUE(with_callback.optimizer_->kkt_pattern_is_analyzed())
        << "the epoch itself never moved -- what changed is whether it is taken as the answer";

    // The same problem with no callback installed keeps the skip, which is
    // what makes the line the callback and not something the problem did.
    NLPSolver without_callback(std::make_shared<EpochGateBoxedProblem>());
    without_callback.optimizer_->set_print_level(3);
    ASSERT_EQ(without_callback.optimize(epoch_gate_start_point()),
              hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(without_callback.optimizer_->kkt_factor_counters().pattern_verify_count, 0);

    // The guard reads the matrix and decides whether to throw; it feeds
    // nothing into the factorization, so the two calls agree exactly.
    const Eigen::VectorXd guarded_x = with_callback.return_x();
    const Eigen::VectorXd skipped_x = without_callback.return_x();
    ASSERT_EQ(guarded_x.size(), skipped_x.size());
    for (int i = 0; i < guarded_x.size(); i++) {
        EXPECT_DOUBLE_EQ(guarded_x[i], skipped_x[i]);
    }
}

// Disarming the callback part-way does not hand the rest of the call back the
// skip: by the time it is disabled the matrix has already been out, so the
// decision is taken once at entry and held. The next call, with nothing
// installed, is the one that skips again.
TEST(StructureEpochGating, TheVerdictOnTheGuardIsTakenOnceAtEntryAndHeldForTheCall) {
    NLPSolver solver(std::make_shared<EpochGateBoxedProblem>());
    solver.optimizer_->set_print_level(3);
    solver.optimizer_->set_early_callback(
        [&](int iteration, double, hven::EigenRef<Eigen::VectorXd>, double,
            hven::EigenRef<Eigen::VectorXd>, hven::EigenRef<Eigen::VectorXd>,
            Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
            if (iteration == 0) {
                solver.optimizer_->disable_early_callback();
            }
            return 0;
        });

    ASSERT_EQ(solver.optimize(epoch_gate_start_point()), hven::ConvergenceFlags::CONVERGED);
    const auto counters_after_disarming = solver.optimizer_->kkt_factor_counters();
    EXPECT_GT(counters_after_disarming.pattern_verify_count, 0)
        << "the call that handed the matrix out verifies to its end";

    ASSERT_EQ(solver.optimize(epoch_gate_start_point()), hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(solver.optimizer_->kkt_factor_counters().pattern_verify_count,
              counters_after_disarming.pattern_verify_count)
        << "the next call has nothing installed, so it takes the skip again";
}

// The verdict is not only taken at entry -- it also responds to a hand-out
// that happens mid-call. Nothing in the public API forbids installing an
// early callback from inside the late callback, and doing so is the door a
// call with no callback armed at entry can still reach the matrix through:
// this call starts with no early callback installed at all (so the entry
// verdict is false, same as the callback-free baseline), then arms one from
// inside the late callback partway through. The early callback's first
// hand-out -- and every factorization after it -- must still verify, exactly
// as it would have if the callback had been armed from the start.
TEST(StructureEpochGating, AnEarlyCallbackArmedFromInsideTheLateCallbackVerifiesFromThatHandOutOn) {
    NLPSolver solver(std::make_shared<EpochGateBoxedProblem>());
    solver.optimizer_->set_print_level(3);

    bool armed = false;
    int early_callback_calls = 0;
    solver.optimizer_->set_late_callback([&](const hven::solvers::IterateInfo &,
                                             hven::ConstEigenRef<Eigen::VectorXd>,
                                             hven::ConstEigenRef<Eigen::VectorXd>) {
        if (!armed) {
            armed = true;
            solver.optimizer_->set_early_callback(
                [&](int, double, hven::EigenRef<Eigen::VectorXd>, double,
                    hven::EigenRef<Eigen::VectorXd>, hven::EigenRef<Eigen::VectorXd>,
                    Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
                    ++early_callback_calls;
                    return 0;
                });
        }
        return 0;
    });

    ASSERT_EQ(solver.optimize(epoch_gate_start_point()), hven::ConvergenceFlags::CONVERGED);
    ASSERT_TRUE(armed) << "the late callback never ran, so the early one was never armed";
    ASSERT_GT(early_callback_calls, 0)
        << "the mid-call-armed early callback never ran, so nothing was handed out";

    EXPECT_GT(solver.optimizer_->kkt_factor_counters().pattern_verify_count, 0)
        << "a hand-out armed mid-call, not just one armed at entry, must still force every "
           "factorization from that hand-out on to re-derive the pattern rather than assume it "
           "-- an early callback armed from inside the late callback used to run under the "
           "entry verdict (false) instead, which is the gap this closes";

    // The late callback alone, never arming an early one, never hands the
    // matrix out and keeps the skip -- isolating that the late callback's
    // mere presence is not what forces verification.
    NLPSolver late_only(std::make_shared<EpochGateBoxedProblem>());
    late_only.optimizer_->set_print_level(3);
    late_only.optimizer_->set_late_callback([](const hven::solvers::IterateInfo &,
                                               hven::ConstEigenRef<Eigen::VectorXd>,
                                               hven::ConstEigenRef<Eigen::VectorXd>) { return 0; });
    ASSERT_EQ(late_only.optimize(epoch_gate_start_point()), hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(late_only.optimizer_->kkt_factor_counters().pattern_verify_count, 0)
        << "a late callback that never arms an early one never hands the matrix out, and must "
           "keep the skip throughout";
}
