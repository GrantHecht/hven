// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// BOTH ENGINES ON THE DENSE ARM (M8 DN-1, design §13 T12 and T14): kAlways solves as kNever
// does on HS071 and on a problem whose KKT matrix is exactly singular (two identical equality
// rows, so the dense arm falls back to the platform session); both validate()s accept kNever and
// kAlways and still refuse kAuto, an engaged threshold, and (IPM) CNR with the dense arm; the
// options dump carries the field.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <Eigen/Core>

#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>

#include "support/hs071_problem.h"

namespace {

using hven::linear::DensePolicy;

// min (x0 - 1)^2 + (x1 - 2)^2  s.t.  x0 + x1 = 1,  2 x0 + 2 x1 = 2: the two rows are
// identical up to scale, so every KKT matrix [H A'; A 0] is exactly singular. x* = (0, 1),
// f* = 2.
struct DuplicateRowsProblem final : hven::Nlp {
    hven::NlpDimensions dimensions() const override { return {2, 2, 4, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
        gl << 1.0, 2.0;
        gu << 1.0, 2.0;
    }
    double eval_f(hven::ConstVecRef x) const override {
        return (x[0] - 1.0) * (x[0] - 1.0) + (x[1] - 2.0) * (x[1] - 2.0);
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = 2.0 * (x[0] - 1.0);
        g[1] = 2.0 * (x[1] - 2.0);
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = x[0] + x[1];
        g[1] = 2.0 * x[0] + 2.0 * x[1];
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
        v << 1.0, 1.0, 2.0, 2.0;
    }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v << 2.0 * obj_factor, 2.0 * obj_factor;
    }
    std::string name() const override { return "DuplicateRowsProblem"; }
};

bool success(hven::SolveStatus s) {
    return s == hven::SolveStatus::kOptimal || s == hven::SolveStatus::kAcceptable;
}

template <class Problem> void expect_ipm_arms_agree(const Eigen::VectorXd &x0) {
    hven::IpmResult r[2];
    int k = 0;
    for (const DensePolicy policy : {DensePolicy::kNever, DensePolicy::kAlways}) {
        hven::IpmOptions o;
        o.common.verbosity = hven::Verbosity::kSilent;
        o.linear.dense_factorization = policy;
        hven::IpmSolver solver(o);
        hven::Problem problem(std::make_shared<Problem>());
        r[k++] = solver.solve(problem, x0);
    }
    EXPECT_EQ(r[1].status, r[0].status);
    if (success(r[0].status)) {
        EXPECT_NEAR(r[1].f, r[0].f, 1e-8 * std::max(1.0, std::abs(r[0].f)));
    }
    EXPECT_EQ(r[0].kkt_factor_counters.dense_factorize_count, 0);
    EXPECT_EQ(r[0].kkt_factor_counters.dense_fallback_count, 0);
    EXPECT_EQ(r[1].kkt_factor_counters.factorize_count,
              r[1].kkt_factor_counters.dense_factorize_count +
                  r[1].kkt_factor_counters.dense_fallback_count);
    EXPECT_GT(r[1].kkt_factor_counters.dense_factorize_count +
                  r[1].kkt_factor_counters.dense_fallback_count,
              0);
}

template <class Problem> void expect_sqp_arms_agree(const Eigen::VectorXd &x0) {
    hven::SqpResult r[2];
    int k = 0;
    for (const DensePolicy policy : {DensePolicy::kNever, DensePolicy::kAlways}) {
        hven::SqpOptions o;
        o.common.verbosity = hven::Verbosity::kSilent;
        o.linear.dense_factorization = policy;
        hven::SqpSolver solver(o);
        hven::Problem problem(std::make_shared<Problem>());
        r[k++] = solver.solve(problem, x0);
    }
    EXPECT_EQ(r[1].status, r[0].status);
    if (success(r[0].status)) {
        EXPECT_NEAR(r[1].f, r[0].f, 1e-8 * std::max(1.0, std::abs(r[0].f)));
    }
}

TEST(DenseArmEngines, IpmSolvesHs071AsTheSparseArmDoes) {
    expect_ipm_arms_agree<hven_drivers_tests::Hs071Problem>(hven_drivers_tests::hs071_start());
}

TEST(DenseArmEngines, SqpSolvesHs071AsTheSparseArmDoes) {
    expect_sqp_arms_agree<hven_drivers_tests::Hs071Problem>(hven_drivers_tests::hs071_start());
}

TEST(DenseArmEngines, IpmSolvesASingularKktProblemAsTheSparseArmDoes) {
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    expect_ipm_arms_agree<DuplicateRowsProblem>(x0);
}

TEST(DenseArmEngines, SqpSolvesASingularKktProblemAsTheSparseArmDoes) {
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    expect_sqp_arms_agree<DuplicateRowsProblem>(x0);
}

template <class Options> std::string refusal(const Options &o) {
    try {
        hven::validate(o);
    } catch (const std::invalid_argument &e) {
        return e.what();
    }
    return {};
}

template <class Options> void validate_policies() {
    Options o;
    EXPECT_TRUE(refusal(o).empty());
    o.linear.dense_factorization = DensePolicy::kAlways;
    EXPECT_TRUE(refusal(o).empty()) << refusal(o);
    o.linear.dense_factorization = DensePolicy::kAuto;
    EXPECT_NE(refusal(o).find("linear.dense_factorization"), std::string::npos);
    Options t;
    t.linear.max_dense_dimension = 100;
    EXPECT_NE(refusal(t).find("linear.max_dense_dimension"), std::string::npos);
    t.linear.dense_factorization = DensePolicy::kAlways;
    EXPECT_NE(refusal(t).find("linear.max_dense_dimension"), std::string::npos);
}

TEST(DenseArmEngines, IpmValidateAcceptsNeverAndAlwaysOnly) {
    validate_policies<hven::IpmOptions>();
    hven::IpmOptions cnr;
    cnr.linear.conditional_reproducibility = true;
    EXPECT_TRUE(refusal(cnr).empty());
    cnr.linear.dense_factorization = DensePolicy::kAlways;
    EXPECT_NE(refusal(cnr).find("conditional_reproducibility"), std::string::npos);
}

TEST(DenseArmEngines, SqpValidateAcceptsNeverAndAlwaysOnly) {
    validate_policies<hven::SqpOptions>();
}

TEST(DenseArmEngines, TheOptionsDumpCarriesThePolicy) {
    hven::IpmOptions i;
    i.linear.dense_factorization = DensePolicy::kAlways;
    hven::SqpOptions q;
    q.linear.dense_factorization = DensePolicy::kAlways;
    std::ostringstream si;
    std::ostringstream sq;
    hven::dump_options(i, si);
    hven::dump_options(q, sq);
    EXPECT_NE(si.str().find("linear.dense_factorization = kAlways"), std::string::npos) << si.str();
    EXPECT_NE(sq.str().find("linear.dense_factorization = kAlways"), std::string::npos) << sq.str();
}

} // namespace
