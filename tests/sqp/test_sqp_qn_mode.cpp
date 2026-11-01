// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_sqp_qn_mode.cpp -- M7 QN stage 3: the SQP driver under
// SqpOptions::hessian_mode = kLbfgsDamped (design .superpowers/m7-qn-design.md sections 2-3;
// rulings C30, C32, C33).
//
// What is pinned: the options' validation and dump; HS-scale NLPs converge with NO eval_hess
// call from the driver (the model is wrapped and counted: the only call is the bridge's
// lay-time pattern walk, one per laid model); every accepted step offers exactly one pair (the
// SOC-promoted step included); the update-on-reject ablation offers more; the restoration
// phase runs its own instance and the main window resets at its resume; the warm-start probe
// makes no Hessian call and its hash matches the QN solve's own; the consecutive-skip reset and
// its zero-step rate limit; every QP mode; the scaled solve; and the bridge's own contracts
// (lift_variant is a fresh lift byte for byte, predicted_decrease reads B).

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/globalization/sqp/lbfgs_damped.h>
#include <hven/detail/globalization/sqp/mixed_lift.h>
#include <hven/detail/globalization/sqp/qn_lift_bridge.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>

#include "support/hs_problems.h"

#include "hven/core/compiler.h"

HVEN_SUPPRESS_DEPRECATED_BEGIN

namespace hven::solvers {
namespace {

using test_support::make_hs;

constexpr double kInfBound = 1e20;

SqpOptions qn_options() {
    SqpOptions o;
    o.hessian_mode = SqpHessianMode::kLbfgsDamped;
    o.max_iter = 300;
    // The L-BFGS window and its lift, which these stage-3 tests pin; the shipped full-memory
    // default is the `shipped` row of the variant battery below.
    o.qn_full_memory_max = 0;
    return o;
}

// Forwards everything and counts the Hessian calls the MODEL sees.
class HessCountingModel : public NlpModel {
  public:
    explicit HessCountingModel(std::unique_ptr<NlpModel> inner) : inner_(std::move(inner)) {}
    explicit HessCountingModel(NlpModel *borrowed) : borrowed_(borrowed) {}

    mutable Index n_hess = 0;

    Index n() const override { return in().n(); }
    Index me() const override { return in().me(); }
    Index mi() const override { return in().mi(); }
    double eval_f(const Vec &x) const override { return in().eval_f(x); }
    Vec eval_grad(const Vec &x) const override { return in().eval_grad(x); }
    Vec eval_ce(const Vec &x) const override { return in().eval_ce(x); }
    Vec eval_ci(const Vec &x) const override { return in().eval_ci(x); }
    SpMatRM eval_hess(const Vec &x, double s, const Vec &le, const Vec &li) const override {
        ++n_hess;
        return in().eval_hess(x, s, le, li);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return in().eval_jac_e(x);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return in().eval_jac_i(x);
    }
    const Vec &lower() const override { return in().lower(); }
    const Vec &upper() const override { return in().upper(); }
    Vec start_point() const override { return in().start_point(); }

  private:
    const NlpModel &in() const { return borrowed_ != nullptr ? *borrowed_ : *inner_; }
    std::unique_ptr<NlpModel> inner_;
    NlpModel *borrowed_ = nullptr;
};

Index qn_offers(const SqpCounters &c) {
    return c.qn_updates + c.qn_skipped_nonfinite + c.qn_skipped_zero_step + c.qn_skipped_curvature +
           c.qn_skipped_conditioning;
}

// min x0 + x1 s.t. x0^2 + x1^2 = 1, x0 + x1 = 2: infeasible (test_sqp_restoration.cpp's
// fixture 1). The certified infeasible point is (1/sqrt 2, 1/sqrt 2).
class InfeasibleCircleLineModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(0) + x(1); }
    Vec eval_grad(const Vec &) const override { return Vec::Ones(2); }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << x(0) * x(0) + x(1) * x(1) - 1.0, x(0) + x(1) - 2.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &lambda_e, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_e(0);
        h.insert(1, 1) = 2.0 * lambda_e(0);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(2, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.insert(1, 0) = 1.0;
        j.insert(1, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 2.0); }
};

// The classic Maratos fixture (test_sqp_solver.cpp): min 2(x0^2 + x1^2 - 1) - x0 on the unit
// circle, from (cos 0.1, sin 0.1); x* = (1, 0), f* = -1.
class MaratosModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override {
        return 2.0 * (x(0) * x(0) + x(1) * x(1) - 1.0) - x(0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 4.0 * x(0) - 1.0, 4.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(0) * x(0) + x(1) * x(1) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double s, const Vec &le, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = s * 4.0 + le(0) * 2.0;
        h.insert(1, 1) = s * 4.0 + le(0) * 2.0;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << std::cos(0.1), std::sin(0.1);
        return x;
    }
};

// ===========================================================================
// Options
// ===========================================================================

TEST(SqpQnOptions, DefaultsAreTheExactMode) {
    const SqpOptions o;
    EXPECT_EQ(o.hessian_mode, SqpHessianMode::kExact);
    EXPECT_EQ(o.qn_memory, 10);
    EXPECT_FALSE(o.qn_update_on_reject);
    EXPECT_FALSE(o.qn_nonlinear_only);
    EXPECT_EQ(o.qn_full_memory_max, 400);
}

TEST(SqpQnOptions, ValidationRefusesTheIllFormedSettings) {
    SqpOptions o = qn_options();
    EXPECT_NO_THROW(validate(o));
    o.hessian_mode = SqpHessianMode::kSqpHessianModeCount;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o = qn_options();
    o.qn_memory = 0;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.qn_memory = -3;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o = qn_options();
    o.qn_full_memory_max = -1;
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.qn_full_memory_max = 0; // never
    EXPECT_NO_THROW(validate(o));
    o = qn_options();
    o.restoration_proximity = 1e-3; // the exact zeta D^2 split is not shipped under QN
    EXPECT_THROW(validate(o), std::invalid_argument);
    o.hessian_mode = SqpHessianMode::kExact; // ... and is fine under the exact mode
    EXPECT_NO_THROW(validate(o));
    EXPECT_THROW(SqpSolver(SqpOptions{qn_options()}).set_options([] {
        SqpOptions b = qn_options();
        b.qn_memory = 0;
        return b;
    }()),
                 std::invalid_argument);
}

TEST(SqpQnOptions, DumpNamesTheFiveFields) {
    SqpOptions o = qn_options();
    o.qn_memory = 7;
    o.qn_update_on_reject = true;
    o.qn_nonlinear_only = true;
    o.qn_full_memory_max = 250;
    std::ostringstream os;
    dump_options(o, os);
    const std::string text = os.str();
    EXPECT_NE(text.find("\nhessian_mode = kLbfgsDamped\n"), std::string::npos) << text;
    EXPECT_NE(text.find("\nqn_memory = 7\n"), std::string::npos) << text;
    EXPECT_NE(text.find("\nqn_update_on_reject = true\n"), std::string::npos) << text;
    EXPECT_NE(text.find("\nqn_nonlinear_only = true\n"), std::string::npos) << text;
    EXPECT_NE(text.find("\nqn_full_memory_max = 250\n"), std::string::npos) << text;
}

// ===========================================================================
// HS-scale convergence with no driver eval_hess
// ===========================================================================

struct HsCase {
    int number;
    const char *why;
};

// One with active bounds at the optimum (HS3: x2 >= 0), equality-constrained (HS6, HS28, HS39),
// nonconvex (HS6's curved equality, HS39's, HS45's product objective with every upper bound
// active), and inequality rows with bounds (HS76).
const HsCase kHsCases[] = {{3, "active bound"},       {6, "curved equality"},
                           {28, "linear equalities"}, {39, "nonconvex equalities"},
                           {45, "nonconvex, bounds"}, {76, "inequalities + bounds"}};

class SqpQnHs : public ::testing::TestWithParam<HsCase> {};

TEST_P(SqpQnHs, ConvergesWithoutAHessianCall) {
    const HsCase hc = GetParam();
    test_support::HsProblem p = make_hs(hc.number);
    HessCountingModel model(p.model.get());
    SqpSolver driver(qn_options());
    const SqpResult sol = driver.solve(model);
    SCOPED_TRACE(fmt::format("HS{} ({}): status {} majors {} f {:.12g} f* {:.12g}", hc.number,
                             hc.why, to_string(sol.status), sol.counters.major_iters, sol.f,
                             p.f_star));
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_NEAR(sol.f, p.f_star, 1e-5 * std::max(1.0, std::abs(p.f_star)));
    EXPECT_EQ(sol.counters.eval_hess_calls, 0);
    // The model's only Hessian call is the bridge's lay-time pattern walk.
    EXPECT_EQ(model.n_hess, 1);
    EXPECT_EQ(qn_offers(sol.counters), sol.counters.steps_accepted)
        << "accepted-only: one offer per accepted step";
    EXPECT_GE(sol.counters.qn_pairs_peak, sol.counters.qn_updates > 0 ? 1 : 0);
    EXPECT_LE(sol.counters.qn_pairs_peak, 10);
    EXPECT_EQ(sol.counters.qn_kwrong_seen, 0) << "a PD B never reads a wrong inertia";
    EXPECT_EQ(sol.counters.qn_nonpd_seen, 0) << "B reads positive definite at every lift";
    ::testing::Test::RecordProperty("qn",
                                    fmt::format("HS{} majors={} updates={} damped={}", hc.number,
                                                sol.counters.major_iters, sol.counters.qn_updates,
                                                sol.counters.qn_damped));
}

INSTANTIATE_TEST_SUITE_P(Battery, SqpQnHs, ::testing::ValuesIn(kHsCases),
                         [](const ::testing::TestParamInfo<HsCase> &i) {
                             return fmt::format("HS{}", i.param.number);
                         });

// THE MODEL VARIANTS (M7 QN quality): the nonlinear-variables-only restriction, the full-memory
// BFGS, and both, on the same battery. The stock HS6 model declares a structural zero at
// (x2, x2), so the restriction is a no-op on this battery (the live case is
// Hs6LinearX2Model below). Full memory carries no lift (rank 0) and keeps every pair.
struct QnVariant {
    const char *name;
    bool nonlinear_only;
    Index full_memory_max;
};
const QnVariant kQnVariants[] = {
    {"nl", true, 0}, {"full", false, 1000}, {"nl_full", true, 1000}, {"shipped", false, 400}};

class SqpQnVariantHs : public ::testing::TestWithParam<std::tuple<HsCase, QnVariant>> {};

TEST_P(SqpQnVariantHs, ConvergesWithoutAHessianCall) {
    const auto [hc, v] = GetParam();
    test_support::HsProblem p = make_hs(hc.number);
    HessCountingModel model(p.model.get());
    SqpOptions o = qn_options();
    o.qn_nonlinear_only = v.nonlinear_only;
    o.qn_full_memory_max = v.full_memory_max;
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);
    SCOPED_TRACE(fmt::format("HS{} {}: status {} majors {} f {:.12g} f* {:.12g}", hc.number, v.name,
                             to_string(sol.status), sol.counters.major_iters, sol.f, p.f_star));
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_NEAR(sol.f, p.f_star, 1e-5 * std::max(1.0, std::abs(p.f_star)));
    EXPECT_EQ(sol.counters.eval_hess_calls, 0);
    EXPECT_EQ(model.n_hess, 1);
    EXPECT_EQ(sol.counters.qn_kwrong_seen, 0);
    EXPECT_EQ(sol.counters.qn_nonpd_seen, 0);
    EXPECT_EQ(qn_offers(sol.counters), sol.counters.steps_accepted);
    if (v.full_memory_max > 0) {
        EXPECT_EQ(sol.counters.qn_lift_rank_peak, 0) << "a full-memory B rides in H, unlifted";
        EXPECT_EQ(sol.counters.qn_pairs_peak, sol.counters.qn_updates) << "no window";
    }
}

INSTANTIATE_TEST_SUITE_P(Battery, SqpQnVariantHs,
                         ::testing::Combine(::testing::ValuesIn(kHsCases),
                                            ::testing::ValuesIn(kQnVariants)),
                         [](const auto &i) {
                             return fmt::format("HS{}_{}", std::get<0>(i.param).number,
                                                std::get<1>(i.param).name);
                         });

// R3: the masked carrier is pinned at the driver. The fixture is HS6 with a Hessian that declares
// only the (x1, x1) entry (x2 enters linearly, so the pattern is exact; the stock HS6 model
// declares a structural zero at (x2, x2), which puts x2 in the pattern and leaves the
// restriction a no-op there). qn_nonlinear_only then builds MaskedQnModel over x1 alone
// (carried dimension 1); the default carries both. kExact reports 0.
class Hs6LinearX2Model : public HessCountingModel {
  public:
    explicit Hs6LinearX2Model(NlpModel *hs6) : HessCountingModel(hs6) {}
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 2.0 + lambda_e(0) * -20.0;
        h.makeCompressed();
        return h;
    }
};

TEST(SqpQnVariants, TheNonlinearOnlyRestrictionBuildsTheMaskedCarrierAtTheExpectedDimension) {
    auto run = [](bool nonlinear_only, Index full_memory_max) {
        test_support::HsProblem p = make_hs(6);
        Hs6LinearX2Model model(p.model.get());
        SqpOptions o = qn_options();
        o.qn_nonlinear_only = nonlinear_only;
        o.qn_full_memory_max = full_memory_max;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(model);
        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_NEAR(sol.f, p.f_star, 1e-5);
        return sol.counters.qn_dimension;
    };
    EXPECT_EQ(run(false, 400), 2) << "default: every structural variable";
    EXPECT_EQ(run(true, 400), 1) << "nonlinear only, dense carrier: x1";
    EXPECT_EQ(run(true, 0), 1) << "nonlinear only, L-BFGS window inside the mask: x1";
    test_support::HsProblem p = make_hs(6);
    Hs6LinearX2Model model(p.model.get());
    SqpSolver exact{SqpOptions{}};
    EXPECT_EQ(exact.solve(model).counters.qn_dimension, 0) << "kExact";
}

// Under kExact the counter is exactly the driver's share of the model's calls.
TEST(SqpQnExact, TheCounterIsTheDriversShareOfTheModelsCalls) {
    for (const int number : {6, 39, 76}) {
        SCOPED_TRACE(number);
        test_support::HsProblem p = make_hs(number);
        HessCountingModel model(p.model.get());
        SqpSolver driver(SqpOptions{});
        const SqpResult sol = driver.solve(model);
        ASSERT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_GT(sol.counters.eval_hess_calls, 0);
        EXPECT_EQ(model.n_hess, 1 + sol.counters.eval_hess_calls) << "1 = the lay-time walk";
        EXPECT_EQ(qn_offers(sol.counters), 0);
        EXPECT_EQ(sol.counters.qn_resets_consecutive_skips + sol.counters.qn_resets_radius_floor +
                      sol.counters.qn_resets_restoration_resume + sol.counters.qn_pairs_peak +
                      sol.counters.qn_lift_rank_peak + sol.counters.qn_kwrong_seen +
                      sol.counters.qn_nonpd_seen,
                  0);
    }
}

// Every QP kernel runs the lifted subproblems.
TEST(SqpQnModes, EveryQpModeConvergesWithoutAHessianCall) {
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
        for (const int number : {6, 45, 76}) {
            SCOPED_TRACE(fmt::format("mode {} HS{}", static_cast<int>(mode), number));
            test_support::HsProblem p = make_hs(number);
            HessCountingModel model(p.model.get());
            SqpOptions o = qn_options();
            o.qp_mode = mode;
            SqpSolver driver(o);
            const SqpResult sol = driver.solve(model);
            ASSERT_EQ(sol.status, SolveStatus::kOptimal);
            EXPECT_NEAR(sol.f, p.f_star, 1e-5 * std::max(1.0, std::abs(p.f_star)));
            EXPECT_EQ(sol.counters.eval_hess_calls, 0);
            EXPECT_EQ(model.n_hess, 1);
            // THE TIER ACTUALLY RAN on the lifted subproblems (review LOW-4): a mode that
            // silently fell back to the walk would pass every line above.
            if (mode == QpMode::kSsn) {
                EXPECT_GT(sol.counters.ssn.ssn_iters, 0);
            } else if (mode == QpMode::kIpm) {
                EXPECT_GT(sol.counters.ipqp.ipqp_iters, 0);
            }
        }
    }
}

// min x0 + x1 s.t. x0^2 + x1^2 = 1 from (0.1, 0): the first linearization asks for a step of
// 4.95 along x0 inside a radius of 1, so the first QP is infeasible and the ELASTIC tier opens
// (review LOW-4). x* = -(1, 1)/sqrt 2, f* = -sqrt 2.
class CircleFromInsideModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(0) + x(1); }
    Vec eval_grad(const Vec &) const override { return Vec::Ones(2); }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(0) * x(0) + x(1) * x(1) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &le, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * le(0);
        h.insert(1, 1) = 2.0 * le(0);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 0.1, 0.0;
        return x;
    }
};

TEST(SqpQnModes, AnElasticActivatingSolveConvergesWithoutAHessianCall) {
    CircleFromInsideModel inner;
    HessCountingModel model(&inner);
    SqpSolver driver(qn_options());
    const SqpResult sol = driver.solve(model);
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_TRUE(sol.history.front().elastic_applied);
    EXPECT_NEAR(sol.f, -std::sqrt(2.0), 1e-6);
    EXPECT_EQ(sol.counters.eval_hess_calls, 0);
    EXPECT_EQ(sol.counters.qn_kwrong_seen, 0);
    EXPECT_EQ(sol.counters.qn_nonpd_seen, 0);
}

TEST(SqpQnModes, BothWorkingSetAlgebrasAndTheScaledSolveConverge) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        for (const bool scaled : {false, true}) {
            SCOPED_TRACE(fmt::format("algebra {} scaled {}", static_cast<int>(algebra), scaled));
            test_support::HsProblem p = make_hs(39);
            SqpOptions o = qn_options();
            o.qp.ws_algebra = algebra;
            o.enable_scaling = scaled;
            SqpSolver driver(o);
            const SqpResult sol = driver.solve(*p.model);
            ASSERT_EQ(sol.status, SolveStatus::kOptimal);
            EXPECT_NEAR(sol.f, p.f_star, 1e-5 * std::max(1.0, std::abs(p.f_star)));
            EXPECT_EQ(sol.counters.eval_hess_calls, 0);
        }
    }
}

// ===========================================================================
// The pair rule
// ===========================================================================

// THE SOC PAIR IS THE ACCEPTED STEP: on the Maratos fixture the rescue is promoted, and the
// accepted-only rule still offers exactly one pair per accepted step -- the SOC-corrected one.
TEST(SqpQnPairs, TheSocPromotedStepIsTheOfferedPair) {
    MaratosModel model;
    SqpOptions o = qn_options();
    o.kkt_tol = 1e-8;
    o.feas_tol = 1e-8;
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);
    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_NEAR(sol.f, -1.0, 1e-7);
    EXPECT_GE(sol.counters.soc_applied, 1) << "the fixture must promote a correction under QN";
    EXPECT_EQ(qn_offers(sol.counters), sol.counters.steps_accepted);
    EXPECT_EQ(sol.counters.eval_hess_calls, 0);
}

// THE ABLATION offers the rejected trials too, at one derivative query each.
TEST(SqpQnPairs, UpdateOnRejectOffersTheRejectedTrials) {
    MaratosModel model;
    SqpOptions o = qn_options();
    o.kkt_tol = 1e-8;
    o.feas_tol = 1e-8;
    o.enable_soc = false; // rejections stay rejections
    const SqpResult base = SqpSolver(o).solve(model);
    o.qn_update_on_reject = true;
    const SqpResult abl = SqpSolver(o).solve(model);
    ASSERT_EQ(base.status, SolveStatus::kOptimal);
    ASSERT_EQ(abl.status, SolveStatus::kOptimal);
    EXPECT_EQ(qn_offers(base.counters), base.counters.steps_accepted);
    ASSERT_GT(abl.counters.rejected_steps, 0) << "the fixture must reject under QN";
    EXPECT_EQ(qn_offers(abl.counters), abl.counters.steps_accepted + abl.counters.rejected_steps);
    EXPECT_EQ(abl.counters.eval_hess_calls, 0);
}

// THE CONSECUTIVE-SKIP RESET and its zero-step rate limit, on the policy itself.
TEST(SqpQnPairs, FiveNonZeroStepSkipsResetAndZeroStepSkipsDoNotCount) {
    detail::LbfgsDamped qn(3, 1.0);
    Index streak = 0;
    const Vec s = (Vec(3) << 1.0, -0.5, 0.25).finished();
    const Vec y = (Vec(3) << 2.0, -0.4, 0.5).finished();
    ASSERT_EQ(detail::offer_pair_with_skip_reset(qn, streak, s, y, 1.0),
              detail::UpdateOutcome::kUpdated);
    ASSERT_EQ(qn.pairs(), 1);
    Vec nan_y = y;
    nan_y(1) = std::numeric_limits<double>::quiet_NaN();
    const Vec zero_s = Vec::Constant(3, 1e-30);
    for (int k = 0; k < 4; ++k) {
        EXPECT_EQ(detail::offer_pair_with_skip_reset(qn, streak, s, nan_y, 1.0),
                  detail::UpdateOutcome::kSkippedNonFinite);
        EXPECT_EQ(detail::offer_pair_with_skip_reset(qn, streak, zero_s, y, 1.0),
                  detail::UpdateOutcome::kSkippedZeroStep);
    }
    EXPECT_EQ(streak, 4) << "zero-step skips neither advance nor clear the streak";
    EXPECT_EQ(qn.pairs(), 1);
    const auto reset_index = static_cast<std::size_t>(detail::ResetReason::kConsecutiveSkips);
    EXPECT_EQ(qn.counters().resets[reset_index], 0);
    (void)detail::offer_pair_with_skip_reset(qn, streak, s, nan_y, 1.0);
    EXPECT_EQ(qn.counters().resets[reset_index], 1);
    EXPECT_EQ(qn.pairs(), 0);
    EXPECT_EQ(streak, 0);
    // An accepted pair clears a partial streak.
    (void)detail::offer_pair_with_skip_reset(qn, streak, s, nan_y, 1.0);
    EXPECT_EQ(streak, 1);
    (void)detail::offer_pair_with_skip_reset(qn, streak, s, y, 1.0);
    EXPECT_EQ(streak, 0);
}

// ===========================================================================
// Restoration
// ===========================================================================

TEST(SqpQnRestoration, TheSubSolveRunsItsOwnInstanceWithoutAHessianCall) {
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");
        InfeasibleCircleLineModel inner;
        HessCountingModel model(&inner);
        SqpOptions o = qn_options();
        o.qp.ws_algebra = algebra;
        // The phase on the solve's OWN curvature source (M7 W2 RST's kModel); the
        // regularized curvature is the next test's.
        o.restoration_curvature = SqpRestorationCurvature::kModel;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(model);
        ASSERT_GE(sol.counters.restoration_entries, 1);
        EXPECT_GE(sol.counters.restoration_iters, 1);
        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_EQ(sol.counters.eval_hess_calls, 0);
        // One lay walk for the caller's model, one for each restoration wrapper's.
        EXPECT_EQ(model.n_hess, 1 + sol.counters.restoration_entries);
        EXPECT_GE(sol.counters.qn_updates, 1) << "the sub-solve's own pairs fold in";
        const double t = 1.0 / std::sqrt(2.0);
        EXPECT_NEAR(sol.x(0), t, 1e-5);
        EXPECT_NEAR(sol.x(1), t, 1e-5);
    }
}

// The exact mode's restoration counts its sub-solve's Hessian calls too.
TEST(SqpQnRestoration, TheExactModeFoldsTheSubSolvesHessianCalls) {
    InfeasibleCircleLineModel inner;
    HessCountingModel model(&inner);
    SqpOptions o;
    o.max_iter = 200;
    o.restoration_curvature = SqpRestorationCurvature::kModel;
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);
    ASSERT_GE(sol.counters.restoration_entries, 1);
    EXPECT_EQ(model.n_hess, 1 + sol.counters.restoration_entries + sol.counters.eval_hess_calls);
}

// THE REGULARIZED CURVATURE (M7 W2 RST) REPLACES THE SUB-SOLVE'S CURVATURE SOURCE IN
// BOTH MODES: the sub-solve takes the exact-Hessian path, so its Hessian calls are
// counted, and every one of them is answered by the wrapper's own model -- the caller's
// eval_hess sees the main loop's calls and its lay walk, and nothing of the phase's.
TEST(SqpQnRestoration, TheRegularizedCurvatureNeverCallsTheCallersHessianInThePhase) {
    for (const bool qn : {true, false}) {
        SCOPED_TRACE(qn ? "kLbfgsDamped" : "kExact");
        InfeasibleCircleLineModel inner;
        HessCountingModel model(&inner);
        SqpOptions o = qn ? qn_options() : SqpOptions{};
        o.max_iter = 300;
        o.restoration_curvature = SqpRestorationCurvature::kRegularized;
        o.restoration_max_entries = 1;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(model);
        ASSERT_GE(sol.counters.restoration_entries, 1);
        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified);
        EXPECT_GE(sol.counters.eval_hess_calls, 1) << "the phase's Hessian calls are counted";
        if (qn) {
            EXPECT_EQ(model.n_hess, 1) << "the caller's lay walk only";
        } else {
            EXPECT_LT(model.n_hess,
                      1 + sol.counters.restoration_entries + sol.counters.eval_hess_calls)
                << "the phase's calls did not reach the caller's model";
        }
        const double t = 1.0 / std::sqrt(2.0);
        EXPECT_NEAR(sol.x(0), t, 1e-5);
        EXPECT_NEAR(sol.x(1), t, 1e-5);
    }
}

// ===========================================================================
// The warm-start probe
// ===========================================================================

TEST(SqpQnWarmStart, TheProbeMakesNoHessianCallAndMatchesTheQnStructure) {
    test_support::HsProblem p = make_hs(76);
    HessCountingModel model(p.model.get());
    SqpSolver driver(qn_options());
    const SqpResult first = driver.solve(model);
    ASSERT_EQ(first.status, SolveStatus::kOptimal);
    ASSERT_TRUE(first.warm_start.valid);
    ASSERT_NE(first.warm_start.structure_hash, 0u);
    const Index calls_before = model.n_hess;
    const SqpResult second = driver.solve(model, model.start_point(), first.warm_start);
    ASSERT_EQ(second.status, SolveStatus::kOptimal);
    EXPECT_EQ(model.n_hess - calls_before, 1) << "only the lay walk: the probe made no call";
    EXPECT_EQ(second.counters.eval_hess_calls, 0);
    EXPECT_GE(static_cast<int>(second.counters.start_level_used),
              static_cast<int>(StartLevel::kWarm))
        << "the probe's hash is the QN solve's own structure";
    // An exact-mode warm start describes a different structure: degraded, never thrown.
    SqpSolver exact(SqpOptions{});
    const SqpResult ex = exact.solve(*p.model);
    ASSERT_EQ(ex.status, SolveStatus::kOptimal);
    const SqpResult cross = driver.solve(*p.model, p.model->start_point(), ex.warm_start);
    EXPECT_EQ(cross.status, SolveStatus::kOptimal);
    EXPECT_EQ(cross.counters.start_level_used, StartLevel::kSeeded);
}

// ===========================================================================
// The bridge
// ===========================================================================

detail::LbfgsDamped trained_qn(int n, std::mt19937 &g) {
    detail::LbfgsDamped qn(n, 1.5);
    std::normal_distribution<double> nd;
    Eigen::MatrixXd A = Eigen::MatrixXd::Random(n, n);
    const Eigen::MatrixXd Hs = A * A.transpose() + Eigen::MatrixXd::Identity(n, n);
    for (int k = 0; k < 6; ++k) {
        Vec s(n);
        for (int i = 0; i < n; ++i) {
            s(i) = nd(g);
        }
        (void)qn.update(s, Hs * s, 1.0);
    }
    return qn;
}

QpProblem small_qp(int n) {
    QpProblem qp;
    qp.H = SpMatRM(n, n);
    qp.g = Vec::LinSpaced(n, -1.0, 1.0);
    qp.lower = Vec::Constant(n, -2.0);
    qp.upper = Vec::Constant(n, 2.0);
    qp.lower(0) = 0.25; // p_ref != 0 on one coordinate
    qp.upper(0) = 1.0;
    std::vector<Eigen::Triplet<double>> t;
    for (int j = 0; j < n; ++j) {
        t.emplace_back(0, j, 1.0);
    }
    qp.Ae = SpMatRM(1, n);
    qp.Ae.setFromTriplets(t.begin(), t.end());
    qp.Ae.makeCompressed();
    qp.be = Vec::Constant(1, 0.5);
    t.clear();
    t.emplace_back(0, 1, 1.0);
    t.emplace_back(0, 2, -1.0);
    qp.Ai = SpMatRM(1, n);
    qp.Ai.setFromTriplets(t.begin(), t.end());
    qp.Ai.makeCompressed();
    qp.bi = Vec::Constant(1, 0.7);
    return qp;
}

bool same_bytes(const Vec &a, const Vec &b) {
    return a.size() == b.size() &&
           std::memcmp(a.data(), b.data(), static_cast<std::size_t>(a.size()) * sizeof(double)) ==
               0;
}

bool same_sparse(const SpMatRM &a, const SpMatRM &b) {
    return a.nonZeros() == b.nonZeros() && a.rows() == b.rows() && a.cols() == b.cols() &&
           std::memcmp(a.valuePtr(), b.valuePtr(),
                       static_cast<std::size_t>(a.nonZeros()) * sizeof(double)) == 0 &&
           std::equal(a.innerIndexPtr(), a.innerIndexPtr() + a.nonZeros(), b.innerIndexPtr());
}

TEST(SqpQnBridge, AVariantIsAFreshLiftByteForByte) {
    std::mt19937 g(7);
    const int n = 6;
    const detail::LbfgsDamped qn = trained_qn(n, g);
    ASSERT_GT(qn.pairs(), 0);
    const QpProblem qp = small_qp(n);
    const detail::MixedLift base(qp, n, qn.mixed_lift());
    QpProblem variant = qp;
    variant.g(2) += 0.375;
    variant.be(0) = -0.125;
    variant.bi(0) = 0.0625;
    const QpProblem a = base.lift_variant(variant);
    const QpProblem b = detail::MixedLift(variant, n, qn.mixed_lift()).problem();
    EXPECT_TRUE(same_bytes(a.g, b.g));
    EXPECT_TRUE(same_bytes(a.be, b.be));
    EXPECT_TRUE(same_bytes(a.bi, b.bi));
    EXPECT_TRUE(same_bytes(a.lower, b.lower));
    EXPECT_TRUE(same_bytes(a.upper, b.upper));
    EXPECT_TRUE(same_sparse(a.H, b.H));
    EXPECT_TRUE(same_sparse(a.Ae, b.Ae));
    EXPECT_TRUE(same_sparse(a.Ai, b.Ai));
    QpProblem wrong = qp;
    wrong.be = Vec::Zero(2);
    EXPECT_THROW((void)base.lift_variant(wrong), std::invalid_argument);
}

TEST(SqpQnBridge, TheCacheServesTheSameLiftAndPredictedDecreaseReadsB) {
    std::mt19937 g(11);
    const int n = 6;
    const detail::LbfgsDamped qn = trained_qn(n, g);
    detail::QnLiftBridge bridge(qn, n);
    const QpProblem qp = small_qp(n);
    const QpProblem first = bridge.lift_of(qp);
    const QpProblem again = bridge.lift_of(qp);
    EXPECT_TRUE(same_bytes(first.g, again.g));
    EXPECT_TRUE(same_sparse(first.H, again.H));
    const detail::QnLiftBridge::Tally t = bridge.take_tally();
    EXPECT_EQ(t.lift_rank_peak, qn.mixed_lift().u_plus.cols() + qn.mixed_lift().v.cols());
    // -(g^T p + 1/2 p^T B p) with the dense B.
    Eigen::MatrixXd B(n, n);
    for (int j = 0; j < n; ++j) {
        B.col(j) = qn.apply(Vec::Unit(n, j));
    }
    const Vec p = Vec::LinSpaced(n, 0.3, -0.2);
    EXPECT_NEAR(bridge.predicted_decrease(qp, p), -(qp.g.dot(p) + 0.5 * p.dot(B * p)), 1e-12);
    // A walk solve through the bridge returns the unlifted sizes.
    const QpEngine engine(QpOptions{});
    SolveOverrides ov;
    ov.tr_radius = 1.0;
    const QpSolution qs = bridge.solve(engine, qp, ov);
    ASSERT_EQ(qs.status, QpStatus::kOptimal);
    EXPECT_EQ(qs.x.size(), n);
    EXPECT_EQ(qs.z.size(), n);
    EXPECT_EQ(qs.lambda_e.size(), qp.me());
    EXPECT_EQ(qs.bound_state.size(), static_cast<std::size_t>(n));
    EXPECT_EQ(qs.tr_active.size(), static_cast<std::size_t>(n));
}

} // namespace
} // namespace hven::solvers

HVEN_SUPPRESS_DEPRECATED_END
