// tests/sqp/test_parametric_families.cpp — Phase-4 Task 8: the synthetic
// parametric families of tests/sqp/support/parametric_families.h, checked in the
// two ways that make them usable as a test bed for Tasks 9/10 and Phase 5/6.
//
// F1-F3 ARE TASK 8's; F6 WAS ADDED IN TASK 11's FIX ROUND 1, held to the same
// two gates because its analytic multipliers are the REFERENCE
// tests/test_mesh_transfer.cpp measures the whole costate transfer against.
// (F4/F5 are exercised by tests/test_predictor.cpp, which is what they were
// built for.)
//
// PER FAMILY, IN ORDER:
//   1. THE TRANSCRIPTION GATE. Every family's analytic gradient, Jacobians and
//      Lagrangian Hessian are central-differenced against its own f/cE/cI
//      (derivative_check.h), at several x AND several p -- a family whose
//      derivatives are wrong makes every downstream measurement meaningless,
//      so nothing else about it is asserted until this passes.
//   2. THE PATH GATE. A COLD SOLVE at five parameter values per family is
//      compared against the closed-form path derived in the families header:
//      f within 1e-8 relative, x to 1e-7, the multipliers to 1e-6, and the
//      ACTIVE SET exactly. This is the claim Tasks 9/10 actually consume
//      ("x_star(p) is where the solver lands"), and it is the one a mutation
//      of x_star has to break -- verified by mutation during Task 8.
//
// ACTIVITY IS MEASURED GEOMETRICALLY -- a constraint is active at the returned
// point iff it holds with equality there, to a tolerance. That is the only
// notion that is well defined for F1, whose linear row is active with a ZERO
// multiplier on its whole middle branch (parametric_families.h's DEGENERACY
// note): at a zero multiplier, whether a solver reports the row in its exit
// working set is not determined by the KKT conditions, so asserting
// WarmStart::ineq_active there would be asserting a solver-internal choice as
// if it were mathematics. Where the analytic multiplier is nonzero the two
// notions coincide and the assertion is the same either way.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>

#include "support/derivative_check.h"
#include "support/nlp_kkt_check.h"
#include "support/parametric_families.h"

namespace hven::solvers {
namespace {

using test_support::AnalyticActiveSet;
using test_support::assert_gradient;
using test_support::assert_hessian;
using test_support::assert_jacobians;
using test_support::F1BoxQp;
using test_support::F2CircleNlp;
using test_support::F3SpringChain;
using test_support::F6PathBoundQuadrature;

constexpr double kDerivTol = 1e-6;    // derivative_check.h's mixed abs/rel tol
constexpr double kFRelTol = 1e-8;     // the brief's objective tolerance
constexpr double kXTol = 1e-7;        // primal tolerance against x_star
constexpr double kMultTol = 1e-6;     // multiplier tolerance against the path
constexpr double kActivityTol = 1e-8; // geometric activity tolerance

// A solve tight enough that the 1e-8 relative objective claim is a statement
// about the FAMILY rather than about the default stopping tolerance.
SqpOptions tight_options() {
    SqpOptions opts;
    opts.kkt_tol = 1e-9;
    opts.feas_tol = 1e-9;
    opts.max_iter = 60;
    return opts;
}

// Which constraints hold with equality at `x`, in AnalyticActiveSet's (and
// therefore WarmStart's) encoding. See this file's ACTIVITY note.
AnalyticActiveSet geometric_active_set(const NlpModel &model, const Vec &x, double tol) {
    AnalyticActiveSet a;
    a.bound_active.assign(static_cast<std::size_t>(model.n()), 0);
    const Vec &lo = model.lower();
    const Vec &up = model.upper();
    for (Index i = 0; i < model.n(); ++i) {
        const auto k = static_cast<std::size_t>(i);
        if (std::isfinite(lo(i)) && std::abs(x(i) - lo(i)) <= tol) {
            a.bound_active[k] = -1;
        } else if (std::isfinite(up(i)) && std::abs(up(i) - x(i)) <= tol) {
            a.bound_active[k] = +1;
        }
    }
    a.ineq_active.assign(static_cast<std::size_t>(model.mi()), 0);
    if (model.mi() > 0) {
        const Vec ci = model.eval_ci(x);
        for (Index j = 0; j < model.mi(); ++j) {
            a.ineq_active[static_cast<std::size_t>(j)] =
                static_cast<std::uint8_t>(std::abs(ci(j)) <= tol ? 1 : 0);
        }
    }
    return a;
}

std::string to_text(const AnalyticActiveSet &a) {
    std::string s = "bounds=[";
    for (std::int8_t b : a.bound_active) {
        s += fmt::format("{} ", static_cast<int>(b)); // int8_t would print as a char
    }
    s += "] ineq=[";
    for (std::uint8_t b : a.ineq_active) {
        s += fmt::format("{} ", static_cast<int>(b));
    }
    return s + "]";
}

// The (row, col) list of a sparse matrix's stored entries -- its PATTERN,
// independent of the values. Used to pin nlp_model.h's STRUCTURAL PATTERN
// INVARIANCE precondition (constant in x) and ParametricNlpModel's extension
// of it (constant in p as well).
template <typename SpMat> std::vector<std::pair<int, int>> pattern_of(const SpMat &m) {
    std::vector<std::pair<int, int>> out;
    for (int k = 0; k < m.outerSize(); ++k) {
        for (typename SpMat::InnerIterator it(m, k); it; ++it) {
            out.emplace_back(static_cast<int>(it.row()), static_cast<int>(it.col()));
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Every pattern a model can emit, at one (x, p). Compared across x and p.
struct ModelPattern {
    std::vector<std::pair<int, int>> hess, jac_e, jac_i;
    bool operator==(const ModelPattern &o) const {
        return hess == o.hess && jac_e == o.jac_e && jac_i == o.jac_i;
    }
};

ModelPattern patterns_at(const NlpModel &model, const Vec &x) {
    ModelPattern p;
    p.hess = pattern_of(model.eval_hess(x, 1.0, Vec::Ones(model.me()), Vec::Ones(model.mi())));
    p.jac_e = pattern_of(model.eval_jac_e(x));
    p.jac_i = pattern_of(model.eval_jac_i(x));
    return p;
}

// ---- THE TRANSCRIPTION GATE, once, for any family ----
void check_derivatives_at(const NlpModel &model, const Vec &x, const Vec &lambda_e,
                          const Vec &lambda_i) {
    EXPECT_TRUE(assert_gradient(model, x, kDerivTol));
    EXPECT_TRUE(assert_jacobians(model, x, kDerivTol));
    EXPECT_TRUE(assert_hessian(model, x, lambda_e, lambda_i, kDerivTol));
}

// ---- THE PATH GATE, once, for any family ----
//
// `x_star`, `f_star`, `active` and the two multiplier vectors are the family's
// own analytic claims at this p; everything here is the comparison against a
// cold solve, plus the KKT self-check that keeps a wrong-but-self-consistent
// pair (x_star, solver) from agreeing with itself.
void check_path_at(const NlpModel &model, double p, const Vec &x_star, double f_star,
                   const Vec &lambda_e_star, const Vec &lambda_i_star, const Vec &z_star,
                   const AnalyticActiveSet &active) {
    SCOPED_TRACE(::testing::Message() << "p = " << p);
    const SqpOptions opts = tight_options();
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    EXPECT_LE(std::abs(sol.f - f_star), kFRelTol * std::max(1.0, std::abs(f_star)))
        << "f = " << fmt::format("{:.17g}", sol.f)
        << " vs f_star = " << fmt::format("{:.17g}", f_star);
    EXPECT_LE((sol.x - x_star).lpNorm<Eigen::Infinity>(), kXTol)
        << "x = " << sol.x.transpose() << " vs x_star = " << x_star.transpose();

    if (model.me() > 0) {
        EXPECT_LE((sol.lambda_e - lambda_e_star).lpNorm<Eigen::Infinity>(), kMultTol)
            << "lambda_e = " << sol.lambda_e.transpose() << " vs analytic "
            << lambda_e_star.transpose();
    }
    if (model.mi() > 0) {
        EXPECT_LE((sol.lambda_i - lambda_i_star).lpNorm<Eigen::Infinity>(), kMultTol)
            << "lambda_i = " << sol.lambda_i.transpose() << " vs analytic "
            << lambda_i_star.transpose();
    }
    EXPECT_LE((sol.z - z_star).lpNorm<Eigen::Infinity>(), kMultTol)
        << "z = " << sol.z.transpose() << " vs analytic " << z_star.transpose();

    const AnalyticActiveSet observed = geometric_active_set(model, sol.x, kActivityTol);
    EXPECT_EQ(observed.bound_active, active.bound_active)
        << "observed " << to_text(observed) << " vs analytic " << to_text(active);
    EXPECT_EQ(observed.ineq_active, active.ineq_active)
        << "observed " << to_text(observed) << " vs analytic " << to_text(active);

    // The returned quadruple is a KKT point of the MODEL (recomputed from the
    // model, not from the driver's own residual) -- so a family whose x_star
    // agreed with a mis-solved point would still have to explain itself here.
    const test_support::NlpKktResidual chk =
        test_support::self_check_kkt(model, sol, opts.feas_tol);
    EXPECT_LT(chk.stationarity, 1e-7);
    EXPECT_LT(chk.primal, 1e-8);
    EXPECT_LT(chk.dual_sign, 1e-9);
    EXPECT_LT(chk.complementarity, 1e-8);
}

// =====================================================================
// F1 -- parametric box QP.
// =====================================================================

// Sampled away from p = 0, 0.2, 0.8, 1: the four p at which F1 is degenerate
// (the two bound thresholds and the two endpoints where x_star touches a
// LOWER bound with a zero multiplier). Two on branch L, one on M, two on U.
const std::vector<double> &f1_samples() {
    static const std::vector<double> s{0.05, 0.15, 0.5, 0.85, 0.95};
    return s;
}

TEST(ParametricF1, DerivativesMatchFiniteDifferences) {
    for (double p : {0.05, 0.5, 0.95}) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        F1BoxQp model(p);
        for (const Vec &x :
             {Vec((Vec(2) << 0.4, 0.4).finished()), Vec((Vec(2) << 0.7, 0.1).finished()),
              Vec((Vec(2) << 0.0, 0.8).finished())}) {
            SCOPED_TRACE(::testing::Message() << "x = " << x.transpose());
            // A nonzero lambda_i is deliberate even though F1's analytic
            // multiplier is 0: eval_hess must be checked for the CONSTRAINT
            // term it is contracted to add (here: none, cI being linear), and
            // at lambda_i = 0 that term is untested.
            check_derivatives_at(model, x, Vec(0), Vec::Constant(1, 0.3));
        }
    }
}

TEST(ParametricF1, AnalyticPathMatchesColdSolve) {
    for (double p : f1_samples()) {
        F1BoxQp model(p);
        check_path_at(model, p, F1BoxQp::x_star(p), F1BoxQp::f_star(p), F1BoxQp::lambda_e_star(p),
                      F1BoxQp::lambda_i_star(p), F1BoxQp::z_star(p), F1BoxQp::active_set(p));
    }
}

TEST(ParametricF1, ThresholdsMatchTheirDefiningGeometry) {
    // kPLow is the literal 0.2 and kSumLimit - kBoxUpper is one ulp below it;
    // the header says so, and this is what keeps the two from drifting apart.
    EXPECT_NEAR(F1BoxQp::kPLow, F1BoxQp::kSumLimit - F1BoxQp::kBoxUpper, 1e-15);
    EXPECT_DOUBLE_EQ(F1BoxQp::kPHigh, F1BoxQp::kBoxUpper);

    // p_low is exactly where x2* = 1 - p reaches the ceiling, p_high exactly
    // where x1* = p does -- checked by straddling each threshold rather than
    // by restating the formula.
    const double eps = 1e-6;
    EXPECT_EQ(F1BoxQp::active_set(F1BoxQp::kPLow - eps).bound_active[1], +1);
    EXPECT_EQ(F1BoxQp::active_set(F1BoxQp::kPLow + eps).bound_active[1], 0);
    EXPECT_EQ(F1BoxQp::active_set(F1BoxQp::kPHigh - eps).bound_active[0], 0);
    EXPECT_EQ(F1BoxQp::active_set(F1BoxQp::kPHigh + eps).bound_active[0], +1);
    // ... and the bound multiplier crosses zero there, transversally.
    EXPECT_NEAR(F1BoxQp::z_star(F1BoxQp::kPLow - eps)(1), -eps, 1e-12);
    EXPECT_NEAR(F1BoxQp::z_star(F1BoxQp::kPHigh + eps)(0), -eps, 1e-12);

    // The branches agree at the thresholds (continuity of f*, checked in the
    // header's derivation and pinned here).
    EXPECT_NEAR(F1BoxQp::f_star(F1BoxQp::kPLow), -0.34, 1e-15);
    EXPECT_NEAR(F1BoxQp::f_star(F1BoxQp::kPHigh), -0.34, 1e-15);
}

TEST(ParametricF1, LinearRowIsWeaklyActiveOnTheMiddleBranch) {
    // The DEGENERACY the families header derives: c(p) sums to 1 identically,
    // so the row is active with multiplier zero across all of branch M. Pinned
    // because Tasks 9/10 must not assume strict complementarity here.
    for (double p : {0.25, 0.5, 0.75}) {
        F1BoxQp model(p);
        const Vec x = F1BoxQp::x_star(p);
        EXPECT_NEAR(model.eval_ci(x)(0), 0.0, 1e-15) << "row should be geometrically active";
        EXPECT_DOUBLE_EQ(F1BoxQp::lambda_i_star(p)(0), 0.0) << "with a zero multiplier";
        EXPECT_EQ(F1BoxQp::active_set(p).bound_active, (std::vector<std::int8_t>{0, 0}));
    }
    // ... and strictly inactive off it.
    for (double p : {0.05, 0.95}) {
        F1BoxQp model(p);
        EXPECT_LT(model.eval_ci(F1BoxQp::x_star(p))(0), -0.1);
    }
}

// =====================================================================
// F2 -- projection onto the unit disk.
// =====================================================================

// Three below p* = 0.7861513778 (0.75 deliberately close to it) and two above.
const std::vector<double> &f2_samples() {
    static const std::vector<double> s{0.0, 0.3, 0.75, 0.9, 1.2};
    return s;
}

TEST(ParametricF2, DerivativesMatchFiniteDifferences) {
    for (double p : {0.0, 0.5, 1.2}) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        F2CircleNlp model(p);
        for (const Vec &x : {Vec((Vec(2) << 0.5, 0.5).finished()),
                             Vec((Vec(2) << -0.3, 0.9).finished()), Vec(Vec::Zero(2))}) {
            SCOPED_TRACE(::testing::Message() << "x = " << x.transpose());
            // lambda_i = 0.7 is what exercises the 2*lambda*I constraint term
            // of eval_hess; at x = 0 it also pins that eval_jac_i still emits
            // both (numerically zero) entries. assert_jacobians itself only
            // checks matrix DIMENSIONS, not nnz, so it would not catch a
            // dropped structural zero here -- that guard is
            // ParametricModelContract.PatternsAreIndependentOfXAndP below,
            // which directly compares each model's ModelPattern across x/p.
            check_derivatives_at(model, x, Vec(0), Vec::Constant(1, 0.7));
        }
    }
}

TEST(ParametricF2, AnalyticPathMatchesColdSolve) {
    for (double p : f2_samples()) {
        F2CircleNlp model(p);
        check_path_at(model, p, F2CircleNlp::x_star(p), F2CircleNlp::f_star(p),
                      F2CircleNlp::lambda_e_star(p), F2CircleNlp::lambda_i_star(p),
                      F2CircleNlp::z_star(p), F2CircleNlp::active_set(p));
    }
}

TEST(ParametricF2, ActivationThresholdSolvesItsDefiningEquation) {
    const double t = F2CircleNlp::t_activation();
    const double p = F2CircleNlp::p_activation();
    // t* is the positive root of t^2 + t - 1 = 0 ...
    EXPECT_NEAR(t * t + t - 1.0, 0.0, 5e-16); // ~2 ulp of 1.0
    EXPECT_NEAR(t, 0.61803398874989484820, 1e-15);
    // ... and p* = sqrt(t*) is the positive root of p^2 + p^4 = 1.
    EXPECT_NEAR(p * p + p * p * p * p, 1.0, 1e-15);
    EXPECT_NEAR(p, 0.78615137775742328606, 1e-15);
    // The constraint is inactive strictly below it and active strictly above.
    EXPECT_FALSE(F2CircleNlp::constraint_active(p - 1e-9));
    EXPECT_TRUE(F2CircleNlp::constraint_active(p + 1e-9));
    // The multiplier leaves zero transversally (derivative sqrt(1+p^2) +
    // p^2/sqrt(1+p^2) ~ 1.7 at p*), so activity is a clean event.
    const double dl = F2CircleNlp::lambda_i_star(p + 1e-6)(0) / 1e-6;
    EXPECT_NEAR(dl, std::sqrt(1.0 + p * p) + p * p / std::sqrt(1.0 + p * p), 1e-4);
    EXPECT_DOUBLE_EQ(F2CircleNlp::lambda_i_star(p - 1e-6)(0), 0.0);
}

TEST(ParametricF2, PathIsTheProjectionOntoTheDisk) {
    // x_star = a/||a|| on the active branch, stated independently of the
    // closed form the header derives (and of the code's own sign handling),
    // including for p < 0 where only the implementation, not the derivation,
    // covers the family.
    for (double p : {-1.5, -1.0, 0.9, 1.2, 2.0}) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        const Vec a = F2CircleNlp::a_of(p);
        ASSERT_TRUE(F2CircleNlp::constraint_active(p));
        const Vec x = F2CircleNlp::x_star(p);
        EXPECT_NEAR(x.norm(), 1.0, 1e-15);
        EXPECT_LE((x - a / a.norm()).lpNorm<Eigen::Infinity>(), 1e-15);
        // Tolerances are RELATIVE: at p = 2, f* ~ 12 and one ulp there is
        // already 1.8e-15, so an absolute 1e-15 would be an equality test.
        EXPECT_NEAR(F2CircleNlp::f_star(p), (x - a).squaredNorm(),
                    1e-14 * std::max(1.0, F2CircleNlp::f_star(p)));
        EXPECT_NEAR(F2CircleNlp::lambda_i_star(p)(0), a.norm() - 1.0,
                    1e-14 * std::max(1.0, a.norm()));
    }
}

// =====================================================================
// F3 -- spring chain.
// =====================================================================

const std::vector<double> &f3_samples() {
    // Default threshold p_act = 0.5: three on the free branch, two clamped.
    static const std::vector<double> s{0.1, 0.3, 0.45, 0.7, 1.0};
    return s;
}

TEST(ParametricF3, DerivativesMatchFiniteDifferences) {
    F3SpringChain model(12);
    for (double p : {0.1, 0.5, 1.3}) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        model.set_parameters(Vec::Constant(1, p));
        Vec x1 = Vec::Zero(12);
        for (Index i = 0; i < 12; ++i) {
            x1(i) = 0.3 * static_cast<double>(i);
        }
        Vec x2 = Vec::Zero(12);
        for (Index i = 0; i < 12; ++i) { // a deliberately non-uniform chain
            x2(i) = std::sin(0.7 * static_cast<double>(i)) + 0.2 * static_cast<double>(i);
        }
        for (const Vec &x : {Vec(Vec::Zero(12)), x1, x2}) {
            SCOPED_TRACE(::testing::Message() << "x = " << x.transpose());
            check_derivatives_at(model, x, Vec::Constant(1, -0.4), Vec(0));
        }
    }
}

TEST(ParametricF3, AnalyticPathMatchesColdSolve) {
    F3SpringChain model(50);
    ASSERT_DOUBLE_EQ(model.p_activation(), 0.5);
    for (double p : f3_samples()) {
        model.set_parameters(Vec::Constant(1, p));
        check_path_at(model, p, model.x_star(p), model.f_star(p), model.lambda_e_star(p),
                      model.lambda_i_star(p), model.z_star(p), model.active_set(p));
    }
}

TEST(ParametricF3, ClampedBranchActivatesExactlyTheLastNode) {
    F3SpringChain model(20, 0.5);
    // Free branch: nothing at the ceiling, and the chain ends strictly below.
    for (double p : {0.1, 0.49}) {
        const Vec x = model.x_star(p);
        EXPECT_LT(x(model.n() - 1), model.u() - 1e-12);
        EXPECT_EQ(model.active_front_index(p), model.n()); // "none"
        EXPECT_EQ(model.active_set(p).bound_active, std::vector<std::int8_t>(20, 0));
        EXPECT_DOUBLE_EQ(model.f_star(p), 0.0);
    }
    // Clamped branch: the LAST node only -- never an interior front (the
    // families header derives why the naive front is not KKT-consistent).
    for (double p : {0.51, 1.0, 3.0}) {
        const Vec x = model.x_star(p);
        EXPECT_NEAR(x(model.n() - 1), model.u(), 1e-14);
        EXPECT_EQ(model.active_front_index(p), model.n() - 1);
        std::vector<std::int8_t> expected(20, 0);
        expected.back() = +1;
        EXPECT_EQ(model.active_set(p).bound_active, expected);
        EXPECT_GT(model.f_star(p), 0.0);
        // Every other node is strictly interior.
        for (Index i = 0; i + 1 < model.n(); ++i) {
            EXPECT_LT(x(i), model.u() - 1e-12) << "node " << i;
        }
    }
}

TEST(ParametricF3, NaiveClampedRampIsStrictlyWorseThanTheAnalyticPath) {
    // Pins the families header's correction of the plan's "growing front"
    // expectation: x_i = min((i-1)r, u) is not optimal, with the two worked
    // numbers the header cites.
    F3SpringChain model(6, 1.0 / 5.0); // u = 1.0 at n = 6
    ASSERT_DOUBLE_EQ(model.u(), 1.0);
    for (auto [p, naive_f, opt_f] : {std::tuple{0.35, 0.12375, 0.05625}, {0.5, 0.375, 0.225}}) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        model.set_parameters(Vec::Constant(1, p));
        Vec naive(6);
        for (Index i = 0; i < 6; ++i) {
            naive(i) = std::min(static_cast<double>(i) * p, model.u());
        }
        EXPECT_NEAR(model.eval_f(naive), naive_f, 1e-12);
        EXPECT_NEAR(model.f_star(p), opt_f, 1e-12);
        EXPECT_GT(model.eval_f(naive), model.f_star(p));
        EXPECT_NEAR(model.eval_f(model.x_star(p)), model.f_star(p), 1e-14);
    }
}

TEST(ParametricF3, RestFrontIndexIsTheNaiveRampCrossing) {
    // rest_front_index is provided BECAUSE it is not the active set: it is the
    // first node the rest-length ramp would put above the ceiling. Pinned
    // against its definition (smallest 0-based i with i*p > u) by scanning.
    F3SpringChain model(10, 0.5); // u = 4.5
    ASSERT_DOUBLE_EQ(model.u(), 4.5);
    for (double p : {0.0, 0.1, 0.3, 0.5, 0.9, 1.5, 5.0}) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        Index expected = model.n();
        for (Index i = 0; i < model.n(); ++i) {
            if (static_cast<double>(i) * p > model.u()) {
                expected = i;
                break;
            }
        }
        EXPECT_EQ(model.rest_front_index(p), expected);
    }
}

// n = 10^4 SCALE READINESS (Phase 5). RUNTIME BUDGET: this test is the whole
// file's cost centre -- MEASURED at 0.69 s (Release) and 12.4 s (Debug) on the
// development machine, against 0.70 s / 12.6 s for all 18 tests in this file,
// i.e. it IS the file. It is trimmed to that by two decisions, both stated
// here rather than left to be rediscovered:
//
//   (a) THE CHECK RUNS AT ONE POINT, not the two or three the small-n gates
//       use. assert_gradient costs 2n objective evaluations of O(n) each, so
//       it is quadratic in n: ~2e8 flops at n = 1e4 per point, and that single
//       call is essentially the whole 12.4 s Debug figure above. One point is
//       enough here because the SAME code path is already checked at three
//       points and three parameter values at n = 12; what n = 1e4 adds is
//       size (and the chance for an index-type or allocation mistake to show
//       up), not a new branch.
//   (b) assert_hessian IS REPLACED BY A DIRECTIONAL EQUIVALENT. That checker
//       densifies eval_hess into an n x n Eigen::MatrixXd (derivative_check.h,
//       detail::symmetrize_upper), which is 1e8 doubles = 800 MB at this size
//       -- not a slow test, an impossible one. The substitute below checks
//       H*v against a central difference of the Lagrangian gradient along v
//       for three directions v, which is the same claim contracted per
//       direction, plus the exact tridiagonal structure entry by entry.
TEST(ParametricF3, ScaleReadinessAtTenThousand) {
    constexpr Index kN = 10000;
    F3SpringChain model(kN, 0.5, 0.3);
    ASSERT_EQ(model.n(), kN);
    ASSERT_DOUBLE_EQ(model.p_activation(), 0.5);
    ASSERT_DOUBLE_EQ(model.u(), 0.5 * static_cast<double>(kN - 1));

    Vec x(kN);
    for (Index i = 0; i < kN; ++i) {
        x(i) = 0.2 * static_cast<double>(i) + 0.05 * std::sin(0.3 * static_cast<double>(i));
    }
    const Vec lambda_e = Vec::Constant(1, -0.4);

    EXPECT_TRUE(assert_gradient(model, x, kDerivTol));
    EXPECT_TRUE(assert_jacobians(model, x, kDerivTol));

    // The Hessian, structurally: the exact path Laplacian's upper triangle,
    // 2n - 1 stored entries, values 1/2/-1 as derived.
    const SpMatRM H = model.eval_hess(x, 1.0, lambda_e, Vec(0));
    ASSERT_EQ(H.rows(), kN);
    ASSERT_EQ(H.nonZeros(), 2 * kN - 1);
    for (Index j = 0; j < kN; ++j) {
        const double diag = (j == 0 || j == kN - 1) ? 1.0 : 2.0;
        ASSERT_DOUBLE_EQ(H.coeff(j, j), diag) << "diagonal " << j;
        if (j + 1 < kN) {
            ASSERT_DOUBLE_EQ(H.coeff(j, j + 1), -1.0) << "superdiagonal " << j;
        }
    }

    // The Hessian, numerically: H*v vs a central difference of grad L along v.
    // (H is stored upper-triangle-only, so the symmetric product is
    // H*v + H^T*v - diag(H)*v.)
    const Eigen::SparseMatrix<double, Eigen::RowMajor> Je = model.eval_jac_e(x);
    // The -> Vec is load-bearing: without it the lambda returns an Eigen
    // EXPRESSION holding references to the temporaries built inside it, which
    // dangle the moment it returns (a segfault at this size, silently wrong
    // numbers at a small one).
    auto grad_lagrangian = [&](const Vec &at) -> Vec {
        return model.eval_grad(at) + Je.transpose() * lambda_e;
    };
    const double h = 1e-6;
    for (int dir = 0; dir < 3; ++dir) {
        Vec v(kN);
        for (Index i = 0; i < kN; ++i) {
            v(i) = std::sin(0.11 * static_cast<double>(i) + static_cast<double>(dir));
        }
        const Vec analytic = H * v + H.transpose() * v - H.diagonal().cwiseProduct(v);
        const Vec fd =
            (grad_lagrangian(Vec(x + h * v)) - grad_lagrangian(Vec(x - h * v))) / (2.0 * h);
        EXPECT_LE((analytic - fd).lpNorm<Eigen::Infinity>(),
                  1e-6 * std::max(1.0, analytic.lpNorm<Eigen::Infinity>()))
            << "direction " << dir;
    }

    // The analytic path still holds at this size, on both branches (no solve:
    // a 10^4-variable QP solve is Phase 5's business, not this gate's).
    for (double p : {0.3, 0.9}) {
        model.set_parameters(Vec::Constant(1, p)); // eval_f reads the MODEL's p
        const Vec xs = model.x_star(p);
        const double s = std::min(p, model.p_activation());
        EXPECT_NEAR(xs(kN - 1), static_cast<double>(kN - 1) * s, 1e-9);
        EXPECT_NEAR(model.eval_f(xs), model.f_star(p), 1e-9 * std::max(1.0, model.f_star(p)));
    }
}

// =====================================================================
// F6 -- discretized integral penalty with a path bound (Phase-4 Task 11).
//
// SAME TWO GATES AS F1-F3, for the same reason: F6's analytic claims are
// consumed by tests/test_mesh_transfer.cpp, which uses lambda_i_star as the
// reference its whole costate measurement is taken against, and by whatever
// Phase 5/6 builds on it next. The rest of the family's analytic surface
// (x_star, f_star, z_star, lambda_e_star, active_set, the junctions and the
// activation threshold) is exercised here so that ~50 lines of derivation in a
// SHARED support header are verified rather than merely asserted.
//
// F6 IS MESH-CARRYING, so unlike F1-F3 its "analytic path" is a function of
// the mesh it was constructed with. The gates below therefore fix ONE mesh per
// test and sweep p over it, which is the same sweep F3 does over a fixed chain
// length.
// =====================================================================

// The mesh both gates below use: 17 uniform nodes with composite-trapezoid
// weights. Fine enough that no sampled p puts a node within 1e-3 of an
// activity junction (checked in JunctionsBoundTheActiveSubInterval), so the
// GEOMETRIC active-set comparison in check_path_at is never decided by a
// near-tie.
test_support::F6PathBoundQuadrature make_f6(Index count = 17, double p0 = 0.6) {
    const Vec nodes = test_support::uniform_nodes(count);
    return test_support::F6PathBoundQuadrature(nodes, test_support::trapezoid_weights(nodes), p0);
}

const std::vector<double> &f6_samples() {
    // Four with the bound active on a sub-interval (widening as p falls) and
    // one ABOVE the activation threshold kPActivation = 1, where nothing is
    // active anywhere and every multiplier vanishes. p = 1 itself is excluded:
    // it is the degenerate point where the node at t = 0.5 sits exactly on the
    // bound with a zero multiplier.
    static const std::vector<double> s{0.2, 0.45, 0.6, 0.9, 1.2};
    return s;
}

TEST(ParametricF6, DerivativesMatchFiniteDifferences) {
    auto model = make_f6(7);
    for (double p : {0.2, 0.6, 1.2}) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        model.set_parameters(Vec::Constant(1, p));
        // Three profiles: flat at the start point, the analytic solution
        // itself (where half the nodes sit ON the bound), and a deliberately
        // wiggly one straddling it in both directions.
        Vec wiggly(model.n());
        for (Index i = 0; i < model.n(); ++i) {
            wiggly(i) = 0.9 * std::sin(2.3 * static_cast<double>(i)) - 0.2;
        }
        for (const Vec &x : {Vec(Vec::Zero(model.n())), model.x_star(p), wiggly}) {
            SCOPED_TRACE(::testing::Message() << "x = " << x.transpose());
            // lambda_i is deliberately NONZERO even though cI is affine and so
            // contributes nothing to hess L: that is the claim being checked.
            check_derivatives_at(model, x, Vec(0), Vec::Constant(model.mi(), 0.37));
        }
    }
}

TEST(ParametricF6, AnalyticPathMatchesColdSolve) {
    auto model = make_f6();
    for (double p : f6_samples()) {
        model.set_parameters(Vec::Constant(1, p));
        check_path_at(model, p, model.x_star(p), model.f_star(p), model.lambda_e_star(p),
                      model.lambda_i_star(p), model.z_star(p), model.active_set(p));
    }
}

// The junction formulae and the activation threshold, checked against the
// DEFINING geometry rather than restated: sin(pi t) > p on exactly
// (junction_left, junction_right), and nothing is active at all above
// kPActivation.
TEST(ParametricF6, JunctionsBoundTheActiveSubInterval) {
    using test_support::F6PathBoundQuadrature;
    EXPECT_DOUBLE_EQ(F6PathBoundQuadrature::kPActivation, 1.0);

    for (double p : {0.2, 0.45, 0.6, 0.9}) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        const double lo = F6PathBoundQuadrature::junction_left(p);
        const double hi = F6PathBoundQuadrature::junction_right(p);
        EXPECT_LT(lo, hi);
        // a(t) == p exactly at both junctions, and the interval is symmetric
        // about t = 1/2 (sin(pi t)'s own symmetry, which is what
        // junction_right's 1 - junction_left encodes).
        EXPECT_NEAR(F6PathBoundQuadrature::a_of(lo), p, 1e-15);
        EXPECT_NEAR(F6PathBoundQuadrature::a_of(hi), p, 1e-15);
        EXPECT_NEAR(lo + hi, 1.0, 1e-15);
        // Strictly inside -> active (nu > 0); strictly outside -> nu == 0.
        EXPECT_GT(F6PathBoundQuadrature::nu_of(0.5 * (lo + hi), p), 0.0);
        EXPECT_EQ(F6PathBoundQuadrature::nu_of(0.5 * lo, p), 0.0);
        EXPECT_EQ(F6PathBoundQuadrature::nu_of(0.5 * (hi + 1.0), p), 0.0);

        // No sampled p puts a mesh node within 1e-3 of a junction -- the
        // precondition the path gate's exact active-set comparison rests on.
        const auto model = make_f6();
        for (Index i = 0; i < model.n(); ++i) {
            EXPECT_GT(std::abs(model.nodes()(i) - lo), 1e-3);
            EXPECT_GT(std::abs(model.nodes()(i) - hi), 1e-3);
        }
    }

    // Above the threshold the sup of a is below the bound, so nu vanishes
    // identically and active_set() is empty of active rows.
    const auto model = make_f6(17, 1.2);
    const AnalyticActiveSet s = model.active_set(1.2);
    for (std::uint8_t flag : s.ineq_active) {
        EXPECT_EQ(flag, 0);
    }
    EXPECT_EQ(model.lambda_i_star(1.2).cwiseAbs().maxCoeff(), 0.0);
    // x_star is then the unconstrained target profile a(t_i) exactly.
    for (Index i = 0; i < model.n(); ++i) {
        EXPECT_DOUBLE_EQ(model.x_star(1.2)(i),
                         test_support::F6PathBoundQuadrature::a_of(model.nodes()(i)));
    }
}

// (F6-COSTATE) itself, as an identity rather than as a solve: the analytic
// multiplier vector is EXACTLY the weights times the mesh-free density. This
// is the claim mesh_transfer.h's whole unscale/rescale argument rests on, so
// it is pinned at the fixture rather than only observed downstream.
TEST(ParametricF6, MultipliersAreTheWeightsTimesTheDensity) {
    const auto model = make_f6();
    for (double p : f6_samples()) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        const Vec lam = model.lambda_i_star(p);
        for (Index i = 0; i < model.n(); ++i) {
            EXPECT_DOUBLE_EQ(lam(i),
                             model.weights()(i) *
                                 test_support::F6PathBoundQuadrature::nu_of(model.nodes()(i), p));
        }
        // z carries nothing (no finite bounds) and lambda_e is empty (me == 0)
        // -- both are part of the family's contract, not incidental.
        EXPECT_EQ(model.z_star(p).cwiseAbs().maxCoeff(), 0.0);
        EXPECT_EQ(model.lambda_e_star(p).size(), 0);
        EXPECT_EQ(model.me(), 0);
    }
}

TEST(ParametricF6, ParameterAccessorsAndPatternsHoldTheContract) {
    auto model = make_f6(6, 0.3);
    EXPECT_EQ(model.parameter_dim(), 1);
    EXPECT_DOUBLE_EQ(model.parameters()(0), 0.3);
    EXPECT_DOUBLE_EQ(model.p(), 0.3);
    model.set_parameters(Vec::Constant(1, 0.75));
    EXPECT_DOUBLE_EQ(model.parameters()(0), 0.75);
    EXPECT_DOUBLE_EQ(model.p(), 0.75);
    // set_parameters changes VALUES: p is the constraint level, so cI moved.
    EXPECT_DOUBLE_EQ(model.eval_ci(Vec::Zero(model.n()))(0), -0.75);

    // Precondition 2 (nlp_model.h): a size mismatch throws, naming the family.
    EXPECT_THROW(model.set_parameters(Vec::Zero(2)), std::invalid_argument);
    try {
        model.set_parameters(Vec::Zero(0));
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("F6PathBoundQuadrature"), std::string::npos)
            << e.what();
    }

    // STRUCTURAL PATTERN INVARIANCE in x AND in p. x = 0 with a large p is
    // included on purpose: hess = diag(w_i cosh(0)) is nowhere zero, but the
    // check is that no entry is ever DROPPED, which is what a diagonal built
    // from triplets could silently do if a value collapsed.
    const ModelPattern base = patterns_at(model, Vec::Zero(model.n()));
    for (double p : {0.1, 0.6, 1.5}) {
        model.set_parameters(Vec::Constant(1, p));
        for (const Vec &x : {Vec(Vec::Zero(model.n())), Vec(model.x_star(p)),
                             Vec(Vec::Constant(model.n(), -3.0))}) {
            EXPECT_TRUE(patterns_at(model, x) == base)
                << "pattern moved at p = " << p << ", x = " << x.transpose();
        }
    }
}

TEST(ParametricF6, RejectsDegenerateConstruction) {
    using test_support::F6PathBoundQuadrature;
    const Vec nodes = test_support::uniform_nodes(5);
    const Vec w = test_support::trapezoid_weights(nodes);

    // Fewer than two nodes: no interval.
    EXPECT_THROW(F6PathBoundQuadrature(Vec::Constant(1, 0.0), Vec::Constant(1, 1.0)),
                 std::invalid_argument);
    // Non-increasing nodes.
    Vec bad = nodes;
    bad(2) = bad(1);
    EXPECT_THROW(F6PathBoundQuadrature(bad, w), std::invalid_argument);
    bad(2) = bad(1) - 0.1;
    EXPECT_THROW(F6PathBoundQuadrature(bad, w), std::invalid_argument);
    // weights/nodes length mismatch.
    EXPECT_THROW(F6PathBoundQuadrature(nodes, Vec::Ones(4)), std::invalid_argument);
    // A non-positive weight -- it divides the costate unscaling, so zero is
    // just as fatal as negative (mesh_transfer.h says the same).
    Vec bad_w = w;
    bad_w(3) = 0.0;
    EXPECT_THROW(F6PathBoundQuadrature(nodes, bad_w), std::invalid_argument);
    bad_w(3) = -0.25;
    try {
        F6PathBoundQuadrature(nodes, bad_w);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        // T6: the message names the family, the index AND the value.
        const std::string msg = e.what();
        EXPECT_NE(msg.find("F6PathBoundQuadrature"), std::string::npos) << msg;
        EXPECT_NE(msg.find("weights[3]"), std::string::npos) << msg;
        EXPECT_NE(msg.find("-0.25"), std::string::npos) << msg;
    }
    EXPECT_NO_THROW(F6PathBoundQuadrature(nodes, w));

    // The two mesh helpers carry the same preconditions.
    EXPECT_THROW(test_support::uniform_nodes(1), std::invalid_argument);
    EXPECT_THROW(test_support::trapezoid_weights(Vec::Constant(1, 0.0)), std::invalid_argument);
    // Trapezoid weights sum to the interval length and halve at the ends.
    EXPECT_DOUBLE_EQ(w.sum(), 1.0);
    EXPECT_DOUBLE_EQ(w(0), 0.5 * w(1));
    EXPECT_DOUBLE_EQ(w(4), 0.5 * w(3));
}

// =====================================================================
// The ParametricNlpModel contract itself (nlp_model.h).
// =====================================================================

TEST(ParametricModelContract, ParameterAccessorsRoundTrip) {
    F1BoxQp f1(0.3);
    F2CircleNlp f2(0.4);
    F3SpringChain f3(8, 0.5, 0.2);
    EXPECT_EQ(f1.parameter_dim(), 1);
    EXPECT_EQ(f2.parameter_dim(), 1);
    EXPECT_EQ(f3.parameter_dim(), 1);
    EXPECT_DOUBLE_EQ(f1.parameters()(0), 0.3);
    EXPECT_DOUBLE_EQ(f2.parameters()(0), 0.4);
    EXPECT_DOUBLE_EQ(f3.parameters()(0), 0.2);

    f1.set_parameters(Vec::Constant(1, 0.9));
    EXPECT_DOUBLE_EQ(f1.parameters()(0), 0.9);
    EXPECT_DOUBLE_EQ(f1.p(), 0.9);
    // set_parameters changes VALUES: the objective at a fixed x moved. x is
    // deliberately NOT (0.5, 0.5), where c(p)^T x = 0.5(p + 1 - p) = 0.5 makes
    // F1's objective p-INDEPENDENT and the check vacuous.
    const Vec x = (Vec(2) << 0.7, 0.2).finished();
    f1.set_parameters(Vec::Constant(1, 0.1));
    const double f_lo = f1.eval_f(x);
    f1.set_parameters(Vec::Constant(1, 0.9));
    EXPECT_NE(f1.eval_f(x), f_lo);
}

TEST(ParametricModelContract, SetParametersRejectsWrongSize) {
    // Precondition 2 in nlp_model.h: a size mismatch throws, and the message
    // names the family (project rule T6).
    F1BoxQp f1;
    F2CircleNlp f2;
    F3SpringChain f3(6);
    EXPECT_THROW(f1.set_parameters(Vec::Zero(2)), std::invalid_argument);
    EXPECT_THROW(f2.set_parameters(Vec::Zero(0)), std::invalid_argument);
    EXPECT_THROW(f3.set_parameters(Vec::Zero(3)), std::invalid_argument);
    try {
        f3.set_parameters(Vec::Zero(3));
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("F3SpringChain"), std::string::npos) << e.what();
    }
}

TEST(ParametricModelContract, PatternsAreIndependentOfXAndP) {
    // nlp_model.h's STRUCTURAL PATTERN INVARIANCE (x) plus
    // ParametricNlpModel's precondition 1 (p). The warm-start structure hash
    // keys a cached factorization on exactly these patterns, so a family that
    // dropped a numerically-zero entry at some (x, p) would silently forfeit
    // -- or worse, wrongly claim -- a hot start along a continuation sweep.
    // x = 0 is included on purpose: it is where F2's Jacobian [2x1, 2x2] is
    // entirely zero-valued.
    F1BoxQp f1;
    F2CircleNlp f2;
    F3SpringChain f3(9);
    const std::vector<Vec> pts2{Vec::Zero(2), Vec::Constant(2, 0.5),
                                (Vec(2) << -0.7, 0.3).finished()};
    for (auto *m : std::vector<ParametricNlpModel *>{&f1, &f2}) {
        const ModelPattern reference = patterns_at(*m, pts2.front());
        for (double p : {-0.5, 0.0, 0.25, 1.7}) {
            m->set_parameters(Vec::Constant(1, p));
            for (const Vec &x : pts2) {
                EXPECT_TRUE(patterns_at(*m, x) == reference)
                    << "pattern moved at p = " << p << ", x = " << x.transpose();
            }
        }
    }
    const ModelPattern reference3 = patterns_at(f3, Vec::Zero(9));
    for (double p : {0.0, 0.4, 2.5}) {
        f3.set_parameters(Vec::Constant(1, p));
        EXPECT_TRUE(patterns_at(f3, Vec::Zero(9)) == reference3);
        EXPECT_TRUE(patterns_at(f3, Vec::LinSpaced(9, 0.0, 3.0)) == reference3);
    }
    // The spring chain's pattern is the tridiagonal band's upper triangle.
    EXPECT_EQ(reference3.hess.size(), static_cast<std::size_t>(2 * 9 - 1));
    EXPECT_EQ(reference3.jac_e.size(), 1u);
    EXPECT_TRUE(reference3.jac_i.empty());
}

TEST(ParametricModelContract, SpringChainRejectsDegenerateConstruction) {
    EXPECT_THROW(F3SpringChain(1), std::invalid_argument);
    EXPECT_THROW(F3SpringChain(0), std::invalid_argument);
    EXPECT_THROW(F3SpringChain(-4), std::invalid_argument);
    EXPECT_THROW(F3SpringChain(10, 0.0), std::invalid_argument);
    EXPECT_THROW(F3SpringChain(10, -1.0), std::invalid_argument);
    EXPECT_NO_THROW(F3SpringChain(2, 1e-3));
}

} // namespace
} // namespace hven::solvers
