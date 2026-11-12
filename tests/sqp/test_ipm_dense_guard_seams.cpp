// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// THE IPM DENSE GUARD'S FAILED RE-RUN (DN-1b review F8, M8 DN-1d). On a numerically singular
// dense factorization the guard strips the ladder's shifts and re-runs the factorization; when
// that re-run leaves no numerics the step it was for must not be taken. No real backend fails
// there (the dense arm or Pardiso always produce numerics), so the re-run is failed through the
// dense-attempt and platform-factorize injectors (hven/detail/linear/fault_injection.h).
//
// THE BRANCH IS DEFENSIVE: factor_impl already reports `exhausted` when its last factorization
// failed (every rung then reads a wrong inertia), so this test pins the route -- the guard fired,
// its re-run failed, the solve ends unsuccessfully with a finite point -- rather than a
// difference the branch alone makes.
//
// THE FIXTURE IS A FAMILY, NOT ONE PROBLEM. Whether the step guard fires at all is decided by
// rounding: the step KKT [[2I, J^T], [J, 0]] does not depend on x, J's second row is `scale` x
// its first (consistent targets, so J is rank deficient to rounding), and dsytrf factors it either
// with an exactly zero pivot (the platform session takes it; no guard), with a pivot block of
// rounding-noise size and the expected inertia (the solve's residual ratio is O(1); the guard
// fires), or with the expected inertia and an accurate solve (only the multiplier estimate's own
// guard fires). Which of these a given scale lands in is the platform LAPACK's rounding: at
// scale 1.76 MKL and OpenBLAS factor the step KKT with a 2x2 pivot block of determinant ~1e-33
// (the guard fires); on macOS CI, Accelerate did not reach the step guard at 1.76 at all. So the
// test searches a fixed family of scales, 1.76 first, for a member whose step guard fires, and
// pins the route on the first one.
//
// A FACTORIZATION THAT LEFT NO NUMERICS IS A STATUS (IpmNoNumericsSeam). The same two injectors,
// armed from a later pass, make every factorization from that pass on leave no numerics -- the
// dense attempt reads exactly singular and drops its numerics, and the platform session, which
// never factorized on a kAlways solve, refuses (the one scenario FactorizeFaultInjector is faithful
// for). That is what a backend refusing every rung of the inertia-correction ladder looks like, and
// the solve must end kNumericalError at the iterate it had committed, with no step taken and no
// exception: at the step solve, at the PROBE predictor's solve (which runs first), and at the dense
// guard's failed re-run (the pass search above, whose other passes now return that status too).

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "../drivers/support/hs071_problem.h"
#include <hven/detail/linear/fault_injection.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>

namespace {

using hven::linear::DensePolicy;
using hven::linear::detail::testing::DenseFactorizeFaultInjector;
using hven::linear::detail::testing::FactorizeFaultInjector;

// test_dense_arm_engines.cpp's NearDuplicateRowsProblem with its scale as a parameter: the second
// row is `scale` x the first.
class NearDuplicateRows final : public hven::Nlp {
  public:
    explicit NearDuplicateRows(double scale) : scale_(scale) {}
    hven::NlpDimensions dimensions() const override { return {2, 2, 4, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
        gl << 1.0, scale_;
        gu << 1.0, scale_;
    }
    double eval_f(hven::ConstVecRef x) const override {
        return (x[0] - 1.0) * (x[0] - 1.0) + (x[1] - 2.0) * (x[1] - 2.0);
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = 2.0 * (x[0] - 1.0);
        g[1] = 2.0 * (x[1] - 2.0);
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = x[0] + 0.3 * x[1];
        g[1] = scale_ * x[0] + (scale_ * 0.3) * x[1];
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0, 1, 1;
        c << 0, 1, 0, 1;
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef v) const override {
        v << 1.0, 0.3, scale_, scale_ * 0.3;
    }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v << 2.0 * obj_factor, 2.0 * obj_factor;
    }
    std::string name() const override { return "NearDuplicateRows"; }

  private:
    double scale_;
};

// 1.76 first (the scale the Linux backends reach the route on), then every other hundredth from
// 0.50 to 4.99. Of those 450 scales, MKL and OpenBLAS factor 336 step KKTs with an exactly zero
// pivot and 65 with a rounding-noise pivot block (1.76's class).
std::vector<double> scale_family() {
    std::vector<double> scales{1.76};
    for (int k = 50; k < 500; ++k) {
        if (k != 176) {
            scales.push_back(k / 100.0);
        }
    }
    return scales;
}

hven::IpmResult solve_dense(double scale) {
    hven::IpmOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = DensePolicy::kAlways;
    hven::IpmSolver solver(o);
    hven::Problem problem(std::make_shared<NearDuplicateRows>(scale));
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    return solver.solve(problem, x0);
}

struct Armed {
    explicit Armed(int pass) {
        DenseFactorizeFaultInjector::pass = pass;
        DenseFactorizeFaultInjector::active = true;
        FactorizeFaultInjector::injected_backend_code = -4;
        FactorizeFaultInjector::active = true;
    }
    ~Armed() {
        DenseFactorizeFaultInjector::active = false;
        FactorizeFaultInjector::active = false;
    }
    Armed(const Armed &) = delete;
    Armed &operator=(const Armed &) = delete;
};

bool success(hven::SolveStatus s) {
    return s == hven::SolveStatus::kOptimal || s == hven::SolveStatus::kAcceptable;
}

// HS071 on the dense arm, recording the x of every iteration event: the last is the iterate the
// final iteration started from, which a solve that takes no step there returns unchanged.
struct RecordedSolve {
    hven::IpmResult result;
    std::vector<Eigen::VectorXd> event_x;
};

RecordedSolve solve_hs071_dense(hven::IpmBarrierMode barrier_mode) {
    hven::IpmOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = DensePolicy::kAlways;
    o.advanced.optimize_barrier_mode = barrier_mode;
    o.advanced.solve_barrier_mode = barrier_mode;
    hven::IpmSolver solver(o);
    RecordedSolve out;
    solver.set_iteration_callback([&out](const hven::IterationEvent &e) {
        out.event_x.emplace_back(e.x);
        return hven::CallbackAction::kContinue;
    });
    hven::Problem problem(std::make_shared<hven_drivers_tests::Hs071Problem>());
    out.result = solver.solve(problem, hven_drivers_tests::hs071_start());
    return out;
}

// The contract every no-numerics exit keeps: a status, the ladder's words, the last factorization's
// failure recorded, and the iterate the failing iteration started from returned unmoved.
void expect_ladder_exit(const RecordedSolve &run, hven::Index pass) {
    const hven::IpmResult &r = run.result;
    EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
    EXPECT_NE(r.message.find("inertia-correction ladder at iteration"), std::string::npos)
        << r.message;
    EXPECT_NE(r.message.find("ended on a failed KKT factorization"), std::string::npos)
        << r.message;
    EXPECT_NE(r.last_kkt_info, Eigen::Success);
    EXPECT_EQ(r.kkt_factor_counters.dense_factorize_count, pass);
    EXPECT_GT(r.kkt_factor_counters.dense_fallback_count, 0);
    ASSERT_FALSE(run.event_x.empty());
    const Eigen::VectorXd &start = run.event_x.back();
    ASSERT_EQ(r.x.size(), start.size());
    EXPECT_TRUE(r.x.allFinite());
    EXPECT_LE((r.x - start).lpNorm<Eigen::Infinity>(),
              1e-12 * (1.0 + start.lpNorm<Eigen::Infinity>()))
        << "a step was taken from the iterate the failing iteration started from";
}

} // namespace

TEST(IpmDenseGuardSeam, AFailedGuardReRunEndsTheSolveWithoutItsStep) {
    // What each member did, reported if none reaches the route: the next platform's observations.
    std::string survey;
    bool found = false;
    for (const double scale : scale_family()) {
        const hven::IpmResult clean = solve_dense(scale);
        const hven::Index attempts = clean.kkt_factor_counters.dense_factorize_count;
        survey += fmt::format("\n  scale {:.2f}: clean {} triggers {} dense {} fallbacks {}", scale,
                              success(clean.status) ? "ok" : "FAILED",
                              clean.dense_residual_ratio_triggers, attempts,
                              clean.kkt_factor_counters.dense_fallback_count);
        // A member qualifies when its clean solve succeeds with every factorization on the dense
        // arm (so dense attempts and dense_factorize_count count the same thing) and some guard
        // fired.
        if (!success(clean.status) || clean.kkt_factor_counters.dense_fallback_count != 0 ||
            clean.dense_residual_ratio_triggers == 0) {
            continue;
        }

        // Every dense attempt from `pass` on fails (dense read singular, platform refuses), and
        // `pass < attempts`, so the injector always fires: a run identical to the clean one is
        // never mistaken for the route. EVERY injected run returns a status, never an exception:
        // one whose failure no guard absorbs ends at the initial factorization or at an
        // iteration's inertia-correction ladder (kNumericalError, its message naming which); the
        // route is the run whose message names the step guard's failed re-run.
        for (int pass = 0; pass < static_cast<int>(attempts); ++pass) {
            SCOPED_TRACE(fmt::format("scale {:.2f}, pass {}", scale, pass));
            hven::IpmResult r;
            {
                const Armed armed(pass);
                ASSERT_NO_THROW(r = solve_dense(scale));
            }
            EXPECT_EQ(r.status, hven::SolveStatus::kNumericalError) << r.message;
            EXPECT_TRUE(r.x.allFinite());
            if (r.message.find("dense step guard's refactorization") == std::string::npos) {
                continue;
            }
            found = true;
            // The member and pass the route was pinned on (Linux MKL and OpenBLAS: 1.76, 3).
            ::testing::Test::RecordProperty("route",
                                            fmt::format("scale {:.2f} pass {}", scale, pass));
            EXPECT_GE(r.dense_residual_ratio_triggers, 1);
            EXPECT_EQ(r.kkt_factor_counters.dense_factorize_count, pass);
            EXPECT_GT(r.kkt_factor_counters.dense_fallback_count, 0);
            EXPECT_NE(r.last_kkt_info, Eigen::Success);
            break;
        }
        if (found) {
            break;
        }
        survey += " -- no injected run reached the step guard's re-run";
    }
    EXPECT_TRUE(found) << "no member of the family reached the guard's failed re-run:" << survey;
}

// The step solve's site: from the first iteration's ladder on, every factorization leaves no
// numerics (pass 1: only the initial factorization succeeds), so the ladder ends on a failed one
// and the step solve has nothing to solve on.
TEST(IpmNoNumericsSeam, ALadderEndingWithoutNumericsIsAStatusAtTheFirstIteration) {
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(hven::IpmBarrierMode::kLoqo));
    }
    expect_ladder_exit(run, 1);
    EXPECT_NE(run.result.message.find("at iteration 0 "), std::string::npos) << run.result.message;
}

// The same site mid-solve: only the clean solve's last factorization fails, so the solve has taken
// steps, and ends at the iterate its last iteration started from instead of throwing.
TEST(IpmNoNumericsSeam, ALadderEndingWithoutNumericsIsAStatusMidSolve) {
    const RecordedSolve clean = solve_hs071_dense(hven::IpmBarrierMode::kLoqo);
    ASSERT_EQ(clean.result.status, hven::SolveStatus::kOptimal) << clean.result.message;
    ASSERT_EQ(clean.result.kkt_factor_counters.dense_fallback_count, 0);
    const hven::Index attempts = clean.result.kkt_factor_counters.dense_factorize_count;
    ASSERT_GT(clean.event_x.size(), 2U) << "the fixture must take steps before the failure";
    RecordedSolve run;
    {
        const Armed armed(static_cast<int>(attempts - 1));
        ASSERT_NO_THROW(run = solve_hs071_dense(hven::IpmBarrierMode::kLoqo));
    }
    expect_ladder_exit(run, attempts - 1);
    EXPECT_GT(run.event_x.size(), 2U);
}

// The PROBE predictor's site: under the PROBE barrier mode the governor solves on the iteration's
// factorization before the step solve does, so it must not run on one that left no numerics.
TEST(IpmNoNumericsSeam, ALadderEndingWithoutNumericsIsAStatusBeforeTheProbePredictor) {
    const RecordedSolve clean = solve_hs071_dense(hven::IpmBarrierMode::kProbe);
    ASSERT_TRUE(success(clean.result.status)) << clean.result.message;
    RecordedSolve run;
    {
        const Armed armed(1);
        ASSERT_NO_THROW(run = solve_hs071_dense(hven::IpmBarrierMode::kProbe));
    }
    expect_ladder_exit(run, 1);
}
