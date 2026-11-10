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

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <Eigen/Core>
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

// test_dense_arm_engines.cpp's NearDuplicateRowsProblem: the second row is 1.76 x the first, so
// the dense arm takes a numerically singular KKT with the expected inertia and the guard fires.
struct NearDuplicateRows final : hven::Nlp {
    static constexpr double kScale = 1.76;
    hven::NlpDimensions dimensions() const override { return {2, 2, 4, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
        gl << 1.0, kScale;
        gu << 1.0, kScale;
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
        g[1] = kScale * x[0] + (kScale * 0.3) * x[1];
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
        v << 1.0, 0.3, kScale, kScale * 0.3;
    }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v << 2.0 * obj_factor, 2.0 * obj_factor;
    }
    std::string name() const override { return "NearDuplicateRows"; }
};

hven::IpmResult solve_dense() {
    hven::IpmOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = DensePolicy::kAlways;
    hven::IpmSolver solver(o);
    hven::Problem problem(std::make_shared<NearDuplicateRows>());
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
    const hven::IpmResult clean = solve_dense();
    ASSERT_TRUE(success(clean.status));
    ASSERT_GT(clean.dense_residual_ratio_triggers, 0);
    const hven::Index attempts = clean.kkt_factor_counters.dense_factorize_count;

    // Every dense attempt from `pass` on fails (dense read singular, platform refuses). The
    // smallest `pass` whose run still counts a trigger is the one whose first failed attempt is
    // the guard's re-run: below it the guard never sees a dense factorization to test.
    bool found = false;
    for (int pass = 0; pass <= static_cast<int>(attempts) && !found; ++pass) {
        hven::IpmResult r;
        bool threw = false;
        {
            const Armed armed(pass);
            try {
                r = solve_dense();
            } catch (const std::logic_error &) {
                // A failure injected before the guard can reach a solve the engine
                // does not guard (outside this test's scope); not the guard's route.
                threw = true;
            }
        }
        if (threw || r.dense_residual_ratio_triggers == 0) {
            continue;
        }
        found = true;
        SCOPED_TRACE(pass);
        EXPECT_GE(r.dense_residual_ratio_triggers, 1);
        EXPECT_EQ(r.kkt_factor_counters.dense_factorize_count, pass);
        EXPECT_GT(r.kkt_factor_counters.dense_fallback_count, 0);
        EXPECT_FALSE(success(r.status));
        EXPECT_TRUE(r.x.allFinite());
    }
    EXPECT_TRUE(found);
}
