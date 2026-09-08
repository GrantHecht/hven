// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The LIVE pins on InteriorPointSolver::last_stop_reason(): the two abnormal
// NOTCONVERGED doors and the stall-beats-cap tie. The mapping onto SolveStatus
// is pinned in tests/drivers/test_solve_status.cpp; the iteration-cap pin is in
// test_nlp_solver.cpp, on HS071.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include <Eigen/Core>

#include <hven/drivers/interior_point_solver.h>
#include <hven/drivers/solve_status.h>
#include <hven/model/nlp_problem.h>
#include <hven/model/nlp_solver.h>

namespace stop_reason_test {
namespace {

using hven::solvers::NLPSolver;
using hven::solvers::RestorationModes;

constexpr double kStopReasonInf = std::numeric_limits<double>::infinity();

// c(x) = 1 + eps*x0 + x0^10 = 0 has no real root, so the feasibility phase can
// never succeed; the near-flat Jacobian at the start throws the unlinesearched
// Newton step out to |x0| ~ 1/eps and the pure power then walks it back by a
// tenth per iteration. That long walk back is the SUSTAINED WORSENING the stall
// detector certifies -- a violation a full window of iterations above the
// phase's own best (detail/globalization/feasibility_stall.h).
struct PowerSpikeProblem final : hven::solvers::NLPProblem {
    static constexpr double kEps = 1.0e-4;
    static constexpr int kPower = 10;

    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kStopReasonInf, -kStopReasonInf;
        xu << kStopReasonInf, kStopReasonInf;
        gl << 0.0;
        gu << 0.0;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[1]; }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd>,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 0.0, 1.0;
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 1.0 + kEps * x[0] + std::pow(x[0], kPower);
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
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v << kEps + kPower * std::pow(x[0], kPower - 1), 0.0;
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd> x, double,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << lambda[0] * kPower * (kPower - 1) * std::pow(x[0], kPower - 2), 0.0;
    }
    std::string name() const override { return "PowerSpikeProblem"; }
};

// c(x) = x0^2 + x1^2 + 1 = 0 has no real solution and its violation has a
// STRICT stationary point at the origin: the shape a restoration phase converges
// to and then declares locally infeasible.
struct LocallyInfeasibleProblem final : hven::solvers::NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kStopReasonInf, -kStopReasonInf;
        xu << kStopReasonInf, kStopReasonInf;
        gl << 0.0;
        gu << 0.0;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] + x[1];
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd>,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 1.0, 1.0;
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[0] + x[1] * x[1] + 1.0;
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
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * x[0], 2.0 * x[1];
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd>, double,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * lambda[0], 2.0 * lambda[0];
    }
    std::string name() const override { return "LocallyInfeasibleProblem"; }
};

Eigen::VectorXd two_var_start(double a, double b) {
    Eigen::VectorXd x0(2);
    x0 << a, b;
    return x0;
}

// The stall lever set. The stall branch lives in the feasibility phase, so the
// entry is solve(); restoration must be constructed and its entry budget must
// run out with the stage no better off than where its one episode left it. The
// loosened tolerances keep that episode's exit the near-feasible one rather than
// a local-infeasibility declaration, and the divergence thresholds are lifted
// past the spike this fixture is built around.
NLPSolver make_stall_solver(int max_iters) {
    NLPSolver solver(std::make_shared<PowerSpikeProblem>());
    solver.optimizer_->set_print_level(10);
    solver.optimizer_->set_qp_threads(1);
    solver.optimizer_->set_max_iters(max_iters);
    solver.optimizer_->set_tols(1.0e-8, 0.02, 0.02, 1.0e-8);
    solver.optimizer_->set_acc_tols(1.0e-6, 0.2, 0.2, 1.0e-6);
    solver.optimizer_->set_div_tols(1.0e300, 1.0e300, 1.0e300, 1.0e300);
    solver.optimizer_->settings().restoration_mode_ = RestorationModes::l1_nested;
    solver.optimizer_->set_max_feas_rest(1);
    return solver;
}

// The restoration-locally-infeasible lever set, on the other fixture and the
// OPTIMIZE entry: the optimality phase's own switch enters restoration and the
// subproblem converges at a still-infeasible point. Default tolerances.
NLPSolver make_locally_infeasible_solver(int max_iters) {
    NLPSolver solver(std::make_shared<LocallyInfeasibleProblem>());
    solver.optimizer_->set_print_level(10);
    solver.optimizer_->set_qp_threads(1);
    solver.optimizer_->set_max_iters(max_iters);
    solver.optimizer_->settings().restoration_mode_ = RestorationModes::l1_nested;
    solver.optimizer_->set_max_feas_rest(1);
    return solver;
}

} // namespace
} // namespace stop_reason_test

TEST(IpmStopReason, NoSolveHasHappenedYet) {
    // kNone is not "we did not look": it is the engine saying no labelled door
    // was taken, so the verdict itself explains the exit.
    auto solver = stop_reason_test::make_locally_infeasible_solver(200);
    EXPECT_EQ(solver.optimizer_->last_stop_reason(), hven::solvers::IpmStopReason::kNone);
}

TEST(IpmStopReason, AStalledFeasibilityStageIsRecordedAsSuch) {
    auto solver = stop_reason_test::make_stall_solver(600);
    const hven::ConvergenceFlags flag = solver.solve(stop_reason_test::two_var_start(0.0, 0.0));
    EXPECT_EQ(flag, hven::ConvergenceFlags::NOTCONVERGED);
    EXPECT_EQ(solver.optimizer_->last_stop_reason(), hven::solvers::IpmStopReason::kStageStalled);
    // Well inside the iteration budget: this is not the cap wearing another label.
    EXPECT_LT(solver.optimizer_->result().iter_num_, 600);
    EXPECT_EQ(hven::solvers::to_solve_status(flag, solver.optimizer_->last_stop_reason()),
              hven::solvers::SolveStatus::kStalled);
}

TEST(IpmStopReason, ARestorationLocalInfeasibilityIsRecordedAsSuch) {
    auto solver = stop_reason_test::make_locally_infeasible_solver(200);
    const hven::ConvergenceFlags flag = solver.optimize(stop_reason_test::two_var_start(1.0, 1.0));
    EXPECT_EQ(flag, hven::ConvergenceFlags::NOTCONVERGED);
    EXPECT_EQ(solver.optimizer_->last_stop_reason(),
              hven::solvers::IpmStopReason::kRestorationLocallyInfeasible);
    EXPECT_LT(solver.optimizer_->result().iter_num_, 200);
    EXPECT_EQ(hven::solvers::to_solve_status(flag, solver.optimizer_->last_stop_reason()),
              hven::solvers::SolveStatus::kStalled);
}

TEST(IpmStopReason, AStallOnTheCapIterationKeepsTheStallLabel) {
    // The tie's cap is derived, not hard-coded: raise the cap one at a time from
    // the reported iteration count until the phase reaches the stall again. That
    // first cap is the one whose last loop iteration is BOTH the stall iteration
    // and the cap iteration, so both disjuncts of the terminal conjunction hold.
    auto unbounded = stop_reason_test::make_stall_solver(600);
    (void)unbounded.solve(stop_reason_test::two_var_start(0.0, 0.0));
    ASSERT_EQ(unbounded.optimizer_->last_stop_reason(),
              hven::solvers::IpmStopReason::kStageStalled);
    const int reported = unbounded.optimizer_->result().iter_num_;

    int tie_cap = -1;
    for (int cap = reported; cap <= reported + 20 && tie_cap < 0; ++cap) {
        auto solver = stop_reason_test::make_stall_solver(cap);
        (void)solver.solve(stop_reason_test::two_var_start(0.0, 0.0));
        const hven::solvers::IpmStopReason reason = solver.optimizer_->last_stop_reason();
        if (reason == hven::solvers::IpmStopReason::kStageStalled) {
            tie_cap = cap;
        } else {
            EXPECT_EQ(reason, hven::solvers::IpmStopReason::kIterationCap) << "cap " << cap;
        }
    }
    ASSERT_GT(tie_cap, 0) << "no cap in [" << reported << ", " << reported + 20
                          << "] made the stall iteration the cap iteration";
}
