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

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <fmt/format.h>
#include <gtest/gtest.h>

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
        // never mistaken for the route. Of the injected runs, one whose failure no guard absorbs
        // either ends before any dense solve (no trigger) or reaches the step solve without
        // numerics (std::logic_error, outside this test's scope); one that still counts a trigger
        // and returns had its failure absorbed by the step guard's re-run.
        for (int pass = 0; pass < static_cast<int>(attempts); ++pass) {
            hven::IpmResult r;
            bool threw = false;
            {
                const Armed armed(pass);
                try {
                    r = solve_dense(scale);
                } catch (const std::logic_error &) {
                    threw = true;
                }
            }
            if (threw || r.dense_residual_ratio_triggers == 0) {
                continue;
            }
            found = true;
            SCOPED_TRACE(fmt::format("scale {:.2f}, pass {}", scale, pass));
            EXPECT_GE(r.dense_residual_ratio_triggers, 1);
            EXPECT_EQ(r.kkt_factor_counters.dense_factorize_count, pass);
            EXPECT_GT(r.kkt_factor_counters.dense_fallback_count, 0);
            EXPECT_FALSE(success(r.status));
            EXPECT_TRUE(r.x.allFinite());
            break;
        }
        if (found) {
            break;
        }
        survey += " -- no injected run reached the step guard's re-run";
    }
    EXPECT_TRUE(found) << "no member of the family reached the guard's failed re-run:" << survey;
}
