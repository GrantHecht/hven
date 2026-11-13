// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 DN-3: DensePolicy::kAuto at the engines. Each engine supplies its own threshold (SQP 105,
// IPM 167) and each analyzed pattern selects its arm by its own order: a pattern at or below
// the threshold is the dense arm exactly as kAlways takes it, a pattern above it the platform
// session exactly as kNever takes it. The dense_factorizations counters (SqpCounters,
// IpmResult) count the dense arm's factorizations, and the walk's hot-handle key separates the
// three policies.
//
// The fixture is one equality-constrained convex quadratic of order n with m rows and no
// bounds, so every KKT system is [H A'; A 0] of order n + m on both engines: 120 sits between
// the two thresholds (the SQP's patterns sparse, the IPM's dense), 200 above both.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include <Eigen/Core>

#include <hven/detail/kkt/dense_selection.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>

#include "support/hs071_problem.h"

namespace {

using hven::Index;
using hven::linear::DensePolicy;

// min sum_i (x_i - c_i)^2 + 0.5 sum_i x_i x_{i+1}  s.t.  for each row r: the sum of x over the
// r-th of m interleaved blocks = r + 1. H = 2I + 0.5 (super + sub diagonal) is positive definite
// and A has full row rank, so the KKT system is nonsingular at every iterate.
class EqualityQuadratic final : public hven::Nlp {
  public:
    EqualityQuadratic(Index n, Index m) : n_(n), m_(m) {}
    hven::NlpDimensions dimensions() const override { return {n_, m_, n_, 2 * n_ - 1}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl.setConstant(-inf);
        xu.setConstant(inf);
        for (Index r = 0; r < m_; ++r) {
            gl[r] = gu[r] = static_cast<double>(r + 1);
        }
    }
    double eval_f(hven::ConstVecRef x) const override {
        double f = 0.0;
        for (Index i = 0; i < n_; ++i) {
            f += (x[i] - c(i)) * (x[i] - c(i));
        }
        for (Index i = 0; i + 1 < n_; ++i) {
            f += 0.5 * x[i] * x[i + 1];
        }
        return f;
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        for (Index i = 0; i < n_; ++i) {
            g[i] = 2.0 * (x[i] - c(i));
            if (i > 0) {
                g[i] += 0.5 * x[i - 1];
            }
            if (i + 1 < n_) {
                g[i] += 0.5 * x[i + 1];
            }
        }
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g.setZero();
        for (Index i = 0; i < n_; ++i) {
            g[i % m_] += x[i];
        }
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef col) const override {
        for (Index i = 0; i < n_; ++i) {
            r[i] = i % m_;
            col[i] = i;
        }
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef col) const override {
        for (Index i = 0; i < n_; ++i) {
            r[i] = i;
            col[i] = i;
        }
        for (Index i = 0; i + 1 < n_; ++i) {
            r[n_ + i] = i + 1;
            col[n_ + i] = i;
        }
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef v) const override { v.setOnes(); }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v.head(n_).setConstant(2.0 * obj_factor);
        v.tail(n_ - 1).setConstant(0.5 * obj_factor);
    }
    std::string name() const override { return "EqualityQuadratic"; }

  private:
    static double c(Index i) { return std::sin(static_cast<double>(i + 1)); }
    Index n_;
    Index m_;
};

hven::IpmResult ipm_solve(DensePolicy policy, std::shared_ptr<hven::Nlp> nlp,
                          const Eigen::VectorXd &x0) {
    hven::IpmOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = policy;
    hven::IpmSolver solver(o);
    hven::Problem problem(std::move(nlp));
    return solver.solve(problem, x0);
}

hven::SqpResult sqp_solve(DensePolicy policy, std::shared_ptr<hven::Nlp> nlp,
                          const Eigen::VectorXd &x0) {
    hven::SqpOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = policy;
    hven::SqpSolver solver(o);
    hven::Problem problem(std::move(nlp));
    return solver.solve(problem, x0);
}

bool success(hven::SolveStatus s) {
    return s == hven::SolveStatus::kOptimal || s == hven::SolveStatus::kAcceptable;
}

// The same solve, bit for bit, on the observables both arms share.
void expect_same_ipm(const hven::IpmResult &a, const hven::IpmResult &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.iterations, b.iterations);
    EXPECT_EQ(a.f, b.f);
    EXPECT_EQ(a.x, b.x);
    EXPECT_EQ(a.work.factorizations, b.work.factorizations);
    EXPECT_EQ(a.dense_factorizations, b.dense_factorizations);
    EXPECT_EQ(a.dense_residual_ratio_triggers, b.dense_residual_ratio_triggers);
    EXPECT_EQ(a.kkt_factor_counters.factorize_count, b.kkt_factor_counters.factorize_count);
    EXPECT_EQ(a.kkt_factor_counters.dense_factorize_count,
              b.kkt_factor_counters.dense_factorize_count);
}

void expect_same_sqp(const hven::SqpResult &a, const hven::SqpResult &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.iterations, b.iterations);
    EXPECT_EQ(a.f, b.f);
    EXPECT_EQ(a.x, b.x);
    EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
    EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
    EXPECT_EQ(a.counters.dense_factorizations, b.counters.dense_factorizations);
    EXPECT_EQ(a.counters.dense_flagged_factorizations, b.counters.dense_flagged_factorizations);
}

Eigen::VectorXd zeros(Index n) { return Eigen::VectorXd::Zero(n); }

} // namespace

TEST(DenseAuto, EachEngineHasItsOwnThreshold) {
    using hven::detail::dense_selection_threshold;
    EXPECT_EQ(hven::detail::kSqpDenseSelectionThreshold, 105);
    EXPECT_EQ(hven::detail::kIpmDenseSelectionThreshold, 167);
    EXPECT_EQ(dense_selection_threshold(DensePolicy::kAuto, 7), 7);
    EXPECT_FALSE(dense_selection_threshold(DensePolicy::kNever, 7).has_value());
    EXPECT_FALSE(dense_selection_threshold(DensePolicy::kAlways, 7).has_value());
    // The SQP's single options factory carries its threshold under kAuto only.
    EXPECT_EQ(hven::detail::sqp_kkt_options(0, DensePolicy::kAuto).max_dense_dimension,
              hven::detail::kSqpDenseSelectionThreshold);
    EXPECT_FALSE(
        hven::detail::sqp_kkt_options(0, DensePolicy::kAlways).max_dense_dimension.has_value());
    EXPECT_FALSE(
        hven::detail::sqp_kkt_options(0, DensePolicy::kNever).max_dense_dimension.has_value());
}

// A KKT factor configured under kAuto selects per analyzed pattern, by that pattern's own order.
TEST(DenseAuto, AKktFactorSelectsEachPatternByItsOwnOrder) {
    const auto spd = [](Index n) {
        Eigen::MatrixXd D = 4.0 * Eigen::MatrixXd::Identity(n, n);
        for (Index i = 0; i + 1 < n; ++i) {
            D(i, i + 1) = 1.0;
        }
        hven::SpMatRM K = D.sparseView();
        K.makeCompressed();
        return K;
    };
    const Index t = hven::detail::kSqpDenseSelectionThreshold;
    hven::detail::KktFactor k(0, DensePolicy::kAuto);
    Index dense = 0;
    Index flagged = 0;
    Index refactors = 0;
    Index retakes = 0;
    {
        const hven::detail::DenseTallyScope tally(k, dense, flagged, refactors, retakes);
        const auto at = hven::detail::factorize_checked(k, spd(t), hven::detail::DenseShiftSite{t});
        EXPECT_EQ(at.arm, hven::linear::FactorArm::kDense);
    }
    EXPECT_EQ(dense, 1);
    {
        const hven::detail::DenseTallyScope tally(k, dense, flagged, refactors, retakes);
        const auto above =
            hven::detail::factorize_checked(k, spd(t + 1), hven::detail::DenseShiftSite{t + 1});
        EXPECT_EQ(above.arm, hven::linear::FactorArm::kSparse);
    }
    EXPECT_EQ(dense, 1);
    EXPECT_EQ(flagged, 0);
    EXPECT_EQ(retakes, 0);
}

// THE WALK'S HOT-HANDLE KEY: kNever's is options_fingerprint's and kAlways' is DN-1's, both
// unmoved; kAuto's differs from both.
TEST(DenseAuto, TheWalkKeySeparatesThePolicies) {
    using hven::detail::QpEngine;
    const hven::QpOptions o;
    const std::uint64_t never = QpEngine::engine_fingerprint(o, 0, DensePolicy::kNever);
    const std::uint64_t always = QpEngine::engine_fingerprint(o, 0, DensePolicy::kAlways);
    const std::uint64_t automatic = QpEngine::engine_fingerprint(o, 0, DensePolicy::kAuto);
    EXPECT_EQ(never, hven::options_fingerprint(o, 0));
    EXPECT_EQ(always, never ^ (0x9E3779B97F4A7C15ULL * 2ULL));
    EXPECT_NE(automatic, never);
    EXPECT_NE(automatic, always);
    EXPECT_NE(automatic, never ^ (0x9E3779B97F4A7C15ULL * 3ULL)); // the threshold is in it
}

// ---------------------------------------------------------------------------------------------
// The IPM: one KKT pattern per solve, so a solve is the dense arm's or the platform's.
// ---------------------------------------------------------------------------------------------

TEST(DenseAuto, IpmBelowItsThresholdIsTheDenseArm) {
    // n_kkt = 120: above the SQP's threshold, at or below the IPM's.
    const hven::IpmResult always =
        ipm_solve(DensePolicy::kAlways, std::make_shared<EqualityQuadratic>(110, 10), zeros(110));
    const hven::IpmResult automatic =
        ipm_solve(DensePolicy::kAuto, std::make_shared<EqualityQuadratic>(110, 10), zeros(110));
    ASSERT_TRUE(success(always.status));
    expect_same_ipm(automatic, always);
    EXPECT_GT(automatic.dense_factorizations, 0);
    EXPECT_EQ(automatic.dense_factorizations, automatic.work.factorizations);
}

TEST(DenseAuto, IpmAboveItsThresholdIsThePlatformSession) {
    const hven::IpmResult never =
        ipm_solve(DensePolicy::kNever, std::make_shared<EqualityQuadratic>(190, 10), zeros(190));
    const hven::IpmResult automatic =
        ipm_solve(DensePolicy::kAuto, std::make_shared<EqualityQuadratic>(190, 10), zeros(190));
    ASSERT_TRUE(success(never.status));
    expect_same_ipm(automatic, never);
    EXPECT_EQ(automatic.dense_factorizations, 0);
    EXPECT_EQ(automatic.kkt_factor_counters.dense_factorize_count, 0);
    EXPECT_GT(automatic.work.factorizations, 0);
}

TEST(DenseAuto, IpmCountsTheDenseArmsFactorizations) {
    const Eigen::VectorXd x0 = hven_drivers_tests::hs071_start();
    const hven::IpmResult never =
        ipm_solve(DensePolicy::kNever, std::make_shared<hven_drivers_tests::Hs071Problem>(), x0);
    const hven::IpmResult always =
        ipm_solve(DensePolicy::kAlways, std::make_shared<hven_drivers_tests::Hs071Problem>(), x0);
    const hven::IpmResult automatic =
        ipm_solve(DensePolicy::kAuto, std::make_shared<hven_drivers_tests::Hs071Problem>(), x0);
    EXPECT_EQ(never.dense_factorizations, 0);
    EXPECT_GT(always.dense_factorizations, 0);
    EXPECT_EQ(always.dense_factorizations, always.work.factorizations);
    expect_same_ipm(automatic, always);
}

// The count is this call's, not the solver's lifetime's.
TEST(DenseAuto, IpmCountsPerCall) {
    hven::IpmOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = DensePolicy::kAuto;
    hven::IpmSolver solver(o);
    hven::Problem problem(std::make_shared<hven_drivers_tests::Hs071Problem>());
    const hven::IpmResult first = solver.solve(problem, hven_drivers_tests::hs071_start());
    const hven::IpmResult second = solver.solve(problem, hven_drivers_tests::hs071_start());
    EXPECT_GT(first.dense_factorizations, 0);
    EXPECT_EQ(second.dense_factorizations, second.work.factorizations);
    EXPECT_EQ(first.dense_factorizations, first.work.factorizations);
}

// ---------------------------------------------------------------------------------------------
// The SQP: every factor selects by its own pattern.
// ---------------------------------------------------------------------------------------------

TEST(DenseAuto, SqpBelowItsThresholdIsTheDenseArm) {
    const Eigen::VectorXd x0 = hven_drivers_tests::hs071_start();
    const hven::SqpResult never =
        sqp_solve(DensePolicy::kNever, std::make_shared<hven_drivers_tests::Hs071Problem>(), x0);
    const hven::SqpResult always =
        sqp_solve(DensePolicy::kAlways, std::make_shared<hven_drivers_tests::Hs071Problem>(), x0);
    const hven::SqpResult automatic =
        sqp_solve(DensePolicy::kAuto, std::make_shared<hven_drivers_tests::Hs071Problem>(), x0);
    ASSERT_TRUE(success(always.status));
    EXPECT_EQ(never.counters.dense_factorizations, 0);
    EXPECT_GT(always.counters.dense_factorizations, 0);
    expect_same_sqp(automatic, always);
}

TEST(DenseAuto, SqpAboveItsThresholdIsThePlatformSession) {
    // Every KKT pattern of this solve has order 120 (no bounds: every face is the full one).
    const hven::SqpResult never =
        sqp_solve(DensePolicy::kNever, std::make_shared<EqualityQuadratic>(110, 10), zeros(110));
    const hven::SqpResult always =
        sqp_solve(DensePolicy::kAlways, std::make_shared<EqualityQuadratic>(110, 10), zeros(110));
    const hven::SqpResult automatic =
        sqp_solve(DensePolicy::kAuto, std::make_shared<EqualityQuadratic>(110, 10), zeros(110));
    ASSERT_TRUE(success(never.status));
    expect_same_sqp(automatic, never);
    EXPECT_EQ(automatic.counters.dense_factorizations, 0);
    EXPECT_GT(automatic.counters.factorizations, 0);
    EXPECT_GT(always.counters.dense_factorizations, 0); // the premise: kAlways takes them
}

// ---------------------------------------------------------------------------------------------
// The QP tiers the SQP builds, each with the policy and the SQP's threshold.
// ---------------------------------------------------------------------------------------------

namespace {

// min 0.5 x'x + g'x  s.t.  the sum of x over each of m interleaved blocks = 1, -10 <= x <= 10.
hven::detail::QpProblem block_qp(Index n, Index m) {
    hven::detail::QpProblem qp;
    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(n, n);
    qp.H = H.sparseView();
    qp.g = Eigen::VectorXd(n);
    for (Index i = 0; i < n; ++i) {
        qp.g(i) = 0.1 * std::sin(static_cast<double>(i + 1));
    }
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(m, n);
    for (Index i = 0; i < n; ++i) {
        A(i % m, i) = 1.0;
    }
    qp.Ae = A.sparseView();
    qp.be = Eigen::VectorXd::Ones(m);
    qp.Ai.resize(0, n);
    qp.bi = Eigen::VectorXd(0);
    qp.lower = Eigen::VectorXd::Constant(n, -10.0);
    qp.upper = Eigen::VectorXd::Constant(n, 10.0);
    return qp;
}

hven::QpOptions tier_opts() {
    hven::QpOptions o;
    o.trust_region_radius = std::numeric_limits<double>::infinity();
    return o;
}

} // namespace

TEST(DenseAuto, TheWalkSelectsByTheSqpThreshold) {
    for (const Index n : {Index{20}, Index{110}}) {
        SCOPED_TRACE(n);
        const hven::detail::QpProblem qp = block_qp(n, 5);
        const hven::detail::QpEngine automatic(tier_opts(), 0, DensePolicy::kAuto);
        const hven::detail::QpEngine twin(tier_opts(), 0,
                                          n + 5 <= hven::detail::kSqpDenseSelectionThreshold
                                              ? DensePolicy::kAlways
                                              : DensePolicy::kNever);
        const hven::detail::QpSolution a = automatic.solve(qp);
        const hven::detail::QpSolution t = twin.solve(qp);
        EXPECT_EQ(a.status, t.status);
        EXPECT_EQ(a.x, t.x);
        EXPECT_EQ(a.counters.factorizations, t.counters.factorizations);
        EXPECT_EQ(a.counters.dense_factorizations, t.counters.dense_factorizations);
        if (n + 5 <= hven::detail::kSqpDenseSelectionThreshold) {
            EXPECT_GT(a.counters.dense_factorizations, 0);
        } else {
            EXPECT_EQ(a.counters.dense_factorizations, 0);
            EXPECT_GT(a.counters.factorizations, 0);
        }
    }
}

TEST(DenseAuto, TheSsnTierSelectsByTheSqpThreshold) {
    for (const Index n : {Index{20}, Index{110}}) {
        SCOPED_TRACE(n);
        const hven::detail::QpProblem qp = block_qp(n, 5);
        const bool below = n + 5 <= hven::detail::kSqpDenseSelectionThreshold;
        hven::detail::SsnEngine automatic(tier_opts(), 0, DensePolicy::kAuto);
        hven::detail::SsnEngine twin(tier_opts(), 0,
                                     below ? DensePolicy::kAlways : DensePolicy::kNever);
        hven::detail::SsnResult a;
        hven::detail::SsnResult t;
        automatic.solve(qp, hven::detail::SsnStart{}, hven::detail::SsnOptions{}, &a);
        twin.solve(qp, hven::detail::SsnStart{}, hven::detail::SsnOptions{}, &t);
        EXPECT_EQ(a.x, t.x);
        EXPECT_EQ(a.factorizations, t.factorizations);
        EXPECT_EQ(a.dense_factorizations, t.dense_factorizations);
        if (below) {
            EXPECT_GT(a.dense_factorizations, 0);
            EXPECT_EQ(a.dense_factorizations, a.factorizations);
        } else {
            EXPECT_EQ(a.dense_factorizations, 0);
        }
    }
}

TEST(DenseAuto, TheIpqpTierSelectsByTheSqpThreshold) {
    for (const Index n : {Index{20}, Index{110}}) {
        SCOPED_TRACE(n);
        const hven::detail::QpProblem qp = block_qp(n, 5);
        const bool below = n + 5 <= hven::detail::kSqpDenseSelectionThreshold;
        hven::detail::IpqpEngine automatic(tier_opts(), 0, DensePolicy::kAuto);
        hven::detail::IpqpEngine twin(tier_opts(), 0,
                                      below ? DensePolicy::kAlways : DensePolicy::kNever);
        const hven::detail::IpqpResult a =
            automatic.solve(qp, nullptr, hven::IpqpOptions{}, hven::QpSolveOverrides{});
        const hven::detail::IpqpResult t =
            twin.solve(qp, nullptr, hven::IpqpOptions{}, hven::QpSolveOverrides{});
        EXPECT_EQ(a.status, t.status);
        EXPECT_EQ(a.x, t.x);
        EXPECT_EQ(a.counters.ipqp_factorizations, t.counters.ipqp_factorizations);
        EXPECT_EQ(a.dense_factorizations, t.dense_factorizations);
        if (below) {
            EXPECT_GT(a.dense_factorizations, 0);
        } else {
            EXPECT_EQ(a.dense_factorizations, 0);
            EXPECT_GT(a.counters.ipqp_factorizations, 0);
        }
    }
}
