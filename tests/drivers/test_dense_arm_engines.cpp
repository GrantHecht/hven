// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// BOTH ENGINES ON THE DENSE ARM (M8 DN-1, design §13 T12 and T14): kAlways solves as kNever
// does on HS071 and on a problem whose KKT matrix is exactly singular (two identical equality
// rows: the IPM's ladder answers the dense arm's singular report, M8 DN-3b); both validate()s
// accept every policy (kAuto since M8 DN-3) and refuse an engaged threshold and (IPM) CNR with
// the dense arm; the options dump carries the field. M8 DN-1b: the IPM's residual-ratio guard on a
// numerically singular dense factor, and its absence under kNever.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/QR>

#include <hven/detail/drivers/sqp_solver_impl.h>
#include <hven/detail/qp/ssn_engine.h>
#include <hven/drivers/ipm_iterate.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/drivers/trace.h>
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

// min (x0 - 1)^2 + (x1 - 2)^2  s.t.  x0 + 0.3 x1 = 1,  c x0 + (c * 0.3) x1 = c, c = 1.76: the
// second row is the first scaled, up to the rounding of c * 0.3, so every KKT matrix is
// numerically singular -- and for this c the dense arm's Bunch-Kaufman factorization of both
// the multiplier system [I A'; A 0] and the step system [2I A'; A 0] reads a pivot ~1e-33 of
// the expected sign: inertia correct, no fallback, solve residual ratio ~0.4 (the ACOPP30
// failure class, M8 DN-1b).
struct NearDuplicateRowsProblem final : hven::Nlp {
    static constexpr double kScale = 1.76;
    // Moves the second row's bound: a nonzero offset makes the rows inconsistent.
    explicit NearDuplicateRowsProblem(double offset = 0.0) : offset_(offset) {}
    hven::NlpDimensions dimensions() const override { return {2, 2, 4, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
        gl << 1.0, kScale + offset_;
        gu << 1.0, kScale + offset_;
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
    std::string name() const override { return "NearDuplicateRowsProblem"; }

  private:
    double offset_;
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
    EXPECT_EQ(r[0].kkt_factor_counters.dense_singular_count, 0);
    // The dense arm took or reported every factorization; none went to the platform session.
    EXPECT_EQ(r[1].kkt_factor_counters.factorize_count,
              r[1].kkt_factor_counters.dense_factorize_count +
                  r[1].kkt_factor_counters.dense_singular_count);
    EXPECT_GT(r[1].kkt_factor_counters.dense_factorize_count, 0);
    EXPECT_EQ(r[1].kkt_factor_counters.dense_fallback_count, 0);
    EXPECT_EQ(r[1].kkt_factor_counters.dense_retake_count, 0);
    EXPECT_EQ(r[1].kkt_factor_counters.dense_shift_refactor_count, 0);
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
    EXPECT_TRUE(refusal(o).empty()) << refusal(o);
    o.linear.dense_factorization = static_cast<DensePolicy>(3);
    EXPECT_NE(refusal(o).find("linear.dense_factorization"), std::string::npos);
    Options t;
    t.linear.max_dense_dimension = 100;
    EXPECT_NE(refusal(t).find("linear.max_dense_dimension"), std::string::npos);
    t.linear.dense_factorization = DensePolicy::kAlways;
    EXPECT_NE(refusal(t).find("linear.max_dense_dimension"), std::string::npos);
    t.linear.dense_factorization = DensePolicy::kAuto;
    EXPECT_NE(refusal(t).find("linear.max_dense_dimension"), std::string::npos);
}

TEST(DenseArmEngines, IpmValidateAcceptsEveryPolicyAndNoThreshold) {
    validate_policies<hven::IpmOptions>();
    hven::IpmOptions cnr;
    cnr.linear.conditional_reproducibility = true;
    // M8 DN-3 flip: the default is kAuto, so CNR alone is refused; with kNever it is accepted.
    EXPECT_NE(refusal(cnr).find("conditional_reproducibility"), std::string::npos);
    cnr.linear.dense_factorization = DensePolicy::kNever;
    EXPECT_TRUE(refusal(cnr).empty()) << refusal(cnr);
    cnr.linear.dense_factorization = DensePolicy::kAlways;
    EXPECT_NE(refusal(cnr).find("conditional_reproducibility"), std::string::npos);
    cnr.linear.dense_factorization = DensePolicy::kAuto;
    EXPECT_NE(refusal(cnr).find("conditional_reproducibility"), std::string::npos);
}

TEST(DenseArmEngines, SqpValidateAcceptsEveryPolicyAndNoThreshold) {
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

namespace {

hven::IpmResult solve_ipm(DensePolicy policy, std::shared_ptr<hven::Nlp> nlp,
                          const Eigen::VectorXd &x0,
                          hven::IpmInertiaMode mode = hven::IpmInertiaMode::kClassic) {
    hven::IpmOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = policy;
    o.advanced.inertia_mode = mode;
    hven::IpmSolver solver(o);
    hven::Problem problem(std::move(nlp));
    return solver.solve(problem, x0);
}

} // namespace

// M8 DN-1b: a numerically singular KKT the dense arm factorizes without a fallback and with
// the expected inertia is caught by the solve's residual ratio and answered as singular; the
// solve then ends where the sparse arm's does.
TEST(DenseArmEngines, IpmResidualGuardRecoversANumericallySingularDenseFactor) {
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    const hven::IpmResult sparse =
        solve_ipm(DensePolicy::kNever, std::make_shared<NearDuplicateRowsProblem>(), x0);
    const hven::IpmResult dense =
        solve_ipm(DensePolicy::kAlways, std::make_shared<NearDuplicateRowsProblem>(), x0);
    ASSERT_TRUE(success(sparse.status));
    EXPECT_EQ(dense.status, sparse.status);
    EXPECT_NEAR(dense.f, sparse.f, 1e-8 * std::max(1.0, std::abs(sparse.f)));
    EXPECT_GT(dense.kkt_factor_counters.dense_factorize_count, 0);
    EXPECT_EQ(dense.kkt_factor_counters.dense_fallback_count, 0);
    EXPECT_GT(dense.dense_residual_ratio_triggers, 0);
}

// The same in proximal mode: the strip also removes the base shift rho_k.
TEST(DenseArmEngines, IpmResidualGuardRecoversANumericallySingularDenseFactorProximal) {
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    constexpr auto kProx = hven::IpmInertiaMode::kProximalRegularization;
    const hven::IpmResult sparse =
        solve_ipm(DensePolicy::kNever, std::make_shared<NearDuplicateRowsProblem>(), x0, kProx);
    const hven::IpmResult dense =
        solve_ipm(DensePolicy::kAlways, std::make_shared<NearDuplicateRowsProblem>(), x0, kProx);
    ASSERT_TRUE(success(sparse.status));
    EXPECT_EQ(dense.status, sparse.status);
    EXPECT_NEAR(dense.f, sparse.f, 1e-8 * std::max(1.0, std::abs(sparse.f)));
    EXPECT_GT(dense.kkt_factor_counters.dense_factorize_count, 0);
    EXPECT_EQ(dense.kkt_factor_counters.dense_fallback_count, 0);
    EXPECT_GT(dense.dense_residual_ratio_triggers, 0);
}

// kNever: the guard is never entered. The inconsistent variant (second row's bound off by
// 1e-2) leaves Pardiso solves whose residual ratio exceeds the threshold, so an ungated guard
// would fire on it; the other two are a numerically singular and an exactly singular KKT.
TEST(DenseArmEngines, IpmResidualGuardIsNeverEnteredOnSparseFactorizations) {
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    EXPECT_EQ(solve_ipm(DensePolicy::kNever, std::make_shared<NearDuplicateRowsProblem>(1e-2), x0)
                  .dense_residual_ratio_triggers,
              0);
    EXPECT_EQ(solve_ipm(DensePolicy::kNever, std::make_shared<NearDuplicateRowsProblem>(), x0)
                  .dense_residual_ratio_triggers,
              0);
    EXPECT_EQ(solve_ipm(DensePolicy::kNever, std::make_shared<DuplicateRowsProblem>(), x0)
                  .dense_residual_ratio_triggers,
              0);
}

namespace {

// Every iterate row of a solve, through the trace.
class RowSink final : public hven::TraceSink {
  public:
    explicit RowSink(std::vector<hven::IpmIterate> &rows) : rows_(rows) {}
    void on_ipm_iter(const hven::IpmIterTraceEvent &event) override {
        rows_.push_back(event.iterate);
    }

  private:
    std::vector<hven::IpmIterate> &rows_;
};

hven::IpmResult solve_ipm_rows(DensePolicy policy, std::shared_ptr<hven::Nlp> nlp,
                               const Eigen::VectorXd &x0, std::vector<hven::IpmIterate> &rows) {
    hven::IpmOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = policy;
    hven::IpmSolver solver(o);
    RowSink sink(rows);
    solver.attach_trace(&sink);
    hven::Problem problem(std::move(nlp));
    return solver.solve(problem, x0);
}

// min 0.5 (x0 - 1)^2 + 0.5 e (x1 - 2)^2, e = 1e-17, unconstrained: the KKT matrix is H itself,
// diagonal and positive definite with condition 1e17, so the dense arm's detector flags it
// (rcond < 2^-53) although every solve on it is exact to rounding. Ill-conditioning of this kind
// is what barrier scaling produces on every late IPM iterate.
struct IllConditionedDiagonalProblem final : hven::Nlp {
    static constexpr double kE = 1e-17;
    hven::NlpDimensions dimensions() const override { return {2, 0, 0, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef, hven::VecRef) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
    }
    double eval_f(hven::ConstVecRef x) const override {
        return 0.5 * (x[0] - 1.0) * (x[0] - 1.0) + 0.5 * kE * (x[1] - 2.0) * (x[1] - 2.0);
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = x[0] - 1.0;
        g[1] = kE * (x[1] - 2.0);
    }
    void eval_g(hven::ConstVecRef, hven::VecRef) const override {}
    void jacobian_structure(hven::IndexVecRef, hven::IndexVecRef) const override {}
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef) const override {}
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v << obj_factor, obj_factor * kE;
    }
    std::string name() const override { return "IllConditionedDiagonalProblem"; }
};

} // namespace

// M8 DN-3b: an exactly singular dense KKT is reported, not retaken on the platform session; the
// ladder reads it singular, engages delta_c with its first delta_w rung, and refactors on the
// dense arm, which then takes the factorization.
TEST(DenseArmEngines, IpmLadderAnswersAnExactlySingularDenseKktOnTheDenseArm) {
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    std::vector<hven::IpmIterate> rows;
    const hven::IpmResult sparse =
        solve_ipm(DensePolicy::kNever, std::make_shared<DuplicateRowsProblem>(), x0);
    const hven::IpmResult dense =
        solve_ipm_rows(DensePolicy::kAlways, std::make_shared<DuplicateRowsProblem>(), x0, rows);
    ASSERT_TRUE(success(sparse.status));
    EXPECT_EQ(dense.status, sparse.status);
    EXPECT_NEAR(dense.f, sparse.f, 1e-8 * std::max(1.0, std::abs(sparse.f)));
    const auto &c = dense.kkt_factor_counters;
    EXPECT_GT(c.dense_singular_count, 0);
    EXPECT_EQ(c.dense_fallback_count, 0);
    EXPECT_EQ(c.dense_retake_count, 0);
    EXPECT_EQ(c.factorize_count, c.dense_factorize_count + c.dense_singular_count);
    ASSERT_FALSE(rows.empty());
    // The first iteration's base factorization is singular: one rung of the ladder answers it.
    EXPECT_GE(rows.front().h_facs, 1);
    EXPECT_GT(rows.front().h_pert, 0.0);
}

// M8 DN-3b: a flagged dense factorization whose inertia is the expected one is trusted; the step
// solve's residual ratio (the DN-1b guard), not the flag, decides whether the system is treated
// as singular. Here every solve is exact, so no rung of the ladder runs.
TEST(DenseArmEngines, IpmTrustsAFlaggedDenseFactorWhoseSolveIsAccurate) {
    Eigen::VectorXd x0(2);
    x0 << 3.0, -1.0;
    std::vector<hven::IpmIterate> rows;
    const hven::IpmResult sparse =
        solve_ipm(DensePolicy::kNever, std::make_shared<IllConditionedDiagonalProblem>(), x0);
    const hven::IpmResult dense = solve_ipm_rows(
        DensePolicy::kAlways, std::make_shared<IllConditionedDiagonalProblem>(), x0, rows);
    ASSERT_TRUE(success(sparse.status));
    EXPECT_EQ(dense.status, sparse.status);
    EXPECT_NEAR(dense.f, sparse.f, 1e-8 * std::max(1.0, std::abs(sparse.f)));
    EXPECT_GT(dense.kkt_factor_counters.dense_flagged_count, 0);
    EXPECT_EQ(dense.kkt_factor_counters.dense_singular_count, 0);
    EXPECT_EQ(dense.kkt_factor_counters.dense_fallback_count, 0);
    EXPECT_EQ(dense.dense_residual_ratio_triggers, 0);
    ASSERT_FALSE(rows.empty());
    for (const hven::IpmIterate &row : rows) {
        EXPECT_EQ(row.h_facs, 0) << "iteration " << row.iter;
    }
}

namespace {

// M8 DN-1c: min 0.5 x'Hx - sum(x) on [-1, 1]^3 with H = [[1, 2^20, 0], [2^20, 2^40, 0],
// [0, 0, 1]] (PSD, singular): the walk's K0 carries H + delta*I, singular to working precision
// (rcond ~ 1e-20) although the pivot factorization takes it with nonzero pivots.
struct NearSingularHessianProblem final : hven::Nlp {
    static double big() { return std::ldexp(1.0, 20); }
    hven::NlpDimensions dimensions() const override { return {3, 0, 0, 4}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef, hven::VecRef) const override {
        xl.setConstant(-1.0);
        xu.setConstant(1.0);
    }
    double eval_f(hven::ConstVecRef x) const override {
        const double a = x[0] + big() * x[1];
        return 0.5 * a * a + 0.5 * x[2] * x[2] - x.sum();
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        const double a = x[0] + big() * x[1];
        g[0] = a - 1.0;
        g[1] = big() * a - 1.0;
        g[2] = x[2] - 1.0;
    }
    void eval_g(hven::ConstVecRef, hven::VecRef) const override {}
    void jacobian_structure(hven::IndexVecRef, hven::IndexVecRef) const override {}
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1, 1, 2;
        c << 0, 0, 1, 2;
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef) const override {}
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v << obj_factor, obj_factor * big(), obj_factor * big() * big(), obj_factor;
    }
    std::string name() const override { return "NearSingularHessianProblem"; }
};

hven::SqpResult solve_sqp(DensePolicy policy, std::shared_ptr<hven::Nlp> nlp,
                          const Eigen::VectorXd &x0) {
    hven::SqpOptions o;
    o.common.verbosity = hven::Verbosity::kSilent;
    o.linear.dense_factorization = policy;
    hven::SqpSolver solver(o);
    hven::Problem problem(std::move(nlp));
    return solver.solve(problem, x0);
}

} // namespace

// The SQP's dense factors report (M8 DN-1d): the walk's singular-to-working-precision K0 is
// flagged and answered with the equilibrated shift on the dense arm -- every flag answered, no
// platform retake -- counted in SqpCounters; the solve ends where the sparse arm's does, and
// kNever counts nothing.
TEST(DenseArmEngines, SqpShiftsASingularKktOnTheDenseArm) {
    const Eigen::VectorXd x0 = Eigen::VectorXd::Zero(3);
    const hven::SqpResult sparse =
        solve_sqp(DensePolicy::kNever, std::make_shared<NearSingularHessianProblem>(), x0);
    const hven::SqpResult dense =
        solve_sqp(DensePolicy::kAlways, std::make_shared<NearSingularHessianProblem>(), x0);
    ASSERT_TRUE(success(sparse.status));
    EXPECT_EQ(dense.status, sparse.status);
    EXPECT_NEAR(dense.f, sparse.f, 1e-8 * std::max(1.0, std::abs(sparse.f)));
    EXPECT_EQ(sparse.counters.dense_platform_retakes, 0);
    EXPECT_EQ(sparse.counters.dense_flagged_factorizations, 0);
    EXPECT_EQ(sparse.counters.dense_singular_refactors, 0);
    EXPECT_EQ(dense.counters.dense_platform_retakes, 0);
    EXPECT_GT(dense.counters.dense_flagged_factorizations, 0);
    EXPECT_GT(dense.counters.dense_singular_refactors, 0);
    EXPECT_EQ(dense.counters.dense_singular_refactors, dense.counters.dense_flagged_factorizations);
    EXPECT_EQ(sparse.counters.dense_shifted_solve_refusals, 0);
}

// THE SSN's NEWTON STEP through shifted numerics is refined against K and gated on rho: on a
// nearly inconsistent duplicated row at mu = 0 (every row met to 5e-10, rho ~ 5e-7) the step is
// refused and the engine answers it with its proximal ladder -- sigma also regularizes the dual
// block, so the next factorization is no longer singular -- never by stepping on it.
TEST(DenseArmEngines, TheSsnRefusesAShiftedNewtonStepThatMissesK) {
    hven::detail::QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(3, 3);
    qp.H = Hd.sparseView();
    qp.g = Eigen::VectorXd::Zero(3);
    qp.g(1) = -1e-3;
    Eigen::MatrixXd Ae(2, 3);
    Ae << 1.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    qp.Ae = Ae.sparseView();
    qp.be = Eigen::VectorXd(2);
    qp.be << 1e-3, 1e-3 + 1e-9;
    qp.Ai.resize(0, 3);
    qp.bi = Eigen::VectorXd(0);
    qp.lower = Eigen::VectorXd::Constant(3, -10.0);
    qp.upper = Eigen::VectorXd::Constant(3, 10.0);
    hven::QpOptions opts;
    opts.dual_regularization = 0.0;
    for (const DensePolicy policy : {DensePolicy::kNever, DensePolicy::kAlways}) {
        SCOPED_TRACE(policy == DensePolicy::kAlways ? "dense" : "sparse");
        hven::detail::SsnEngine engine(opts, 0, policy);
        hven::detail::SsnResult res;
        engine.solve(qp, hven::detail::SsnStart{}, hven::detail::SsnOptions{}, &res);
        if (policy == DensePolicy::kNever) {
            EXPECT_EQ(res.dense_shifted_solve_refusals, 0);
            EXPECT_EQ(res.dense_flagged_factorizations, 0);
        } else {
            EXPECT_GE(res.dense_flagged_factorizations, 1);
            EXPECT_EQ(res.dense_platform_retakes, 0);
            EXPECT_GE(res.dense_shifted_solve_refusals, 1);
            EXPECT_GE(res.counters.ssn_prox_updates, 1);
        }
    }
}

// THE SSN's NEWTON STEP through UNSHIFTED dense numerics is checked per solve: H is
// Q diag(1, 0.5, 0.25, 0.1, 0.01, 1e-13) Q' (no rows, no bounds, no regularization), the detector
// passes its factorization, and the first step, along the small eigenvector, misses rho even
// after refinement -- refused and answered with the proximal ladder, as a refused shifted step
// is. The sparse arm is not checked here; the count reads 0 under kNever.
TEST(DenseArmEngines, TheSsnRefusesAnUnshiftedDenseNewtonStepThatMissesK) {
    constexpr Eigen::Index n = 6;
    Eigen::MatrixXd S(n, n);
    for (Eigen::Index i = 0; i < n; ++i) {
        for (Eigen::Index j = 0; j < n; ++j) {
            S(i, j) = std::sin(static_cast<double>(1 + 7 * i + 13 * j));
        }
    }
    const Eigen::HouseholderQR<Eigen::MatrixXd> qr(S);
    const Eigen::MatrixXd Q = qr.householderQ() * Eigen::MatrixXd::Identity(n, n);
    Eigen::VectorXd ev(n);
    ev << 1.0, 0.5, 0.25, 0.1, 0.01, 1e-13;
    Eigen::MatrixXd Hd = Q * ev.asDiagonal() * Q.transpose();
    Hd = 0.5 * (Hd + Hd.transpose()).eval();
    hven::detail::QpProblem qp;
    qp.H = Eigen::MatrixXd(Hd.triangularView<Eigen::Upper>()).sparseView();
    qp.g = 1e-3 * Q.col(n - 1);
    qp.Ae.resize(0, n);
    qp.be = Eigen::VectorXd(0);
    qp.Ai.resize(0, n);
    qp.bi = Eigen::VectorXd(0);
    const double inf = std::numeric_limits<double>::infinity();
    qp.lower = Eigen::VectorXd::Constant(n, -inf);
    qp.upper = Eigen::VectorXd::Constant(n, inf);
    hven::QpOptions opts;
    opts.primal_regularization = 0.0;
    opts.dual_regularization = 0.0;
    for (const DensePolicy policy : {DensePolicy::kNever, DensePolicy::kAlways}) {
        SCOPED_TRACE(policy == DensePolicy::kAlways ? "dense" : "sparse");
        hven::detail::SsnEngine engine(opts, 0, policy);
        hven::detail::SsnResult res;
        engine.solve(qp, hven::detail::SsnStart{}, hven::detail::SsnOptions{}, &res);
        const auto refusals = res.dense_solve_residual_refusals;
        if (policy == DensePolicy::kNever) {
            EXPECT_EQ(refusals, 0);
        } else {
            EXPECT_EQ(res.dense_flagged_factorizations, 0); // the premise: unflagged numerics
            EXPECT_EQ(res.dense_shifted_solve_refusals, 0);
            EXPECT_GE(refusals, 1);
            EXPECT_GE(res.counters.ssn_prox_updates, refusals);
            // Carried to the QP solution's counters and the SQP fold as the shifted count is.
            EXPECT_EQ(
                hven::detail::ssn_result_to_qp_solution(res).counters.dense_solve_residual_refusals,
                refusals);
            hven::SqpCounters total;
            hven::detail::charge_ssn_subproblem_cost(total, res);
            EXPECT_EQ(total.dense_solve_residual_refusals, refusals);
        }
    }
}
