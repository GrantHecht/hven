#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/sqp/eqp_solve.h>
#include <hven/detail/sqp/kkt_assembly.h>
#include <hven/detail/sqp/kkt_system.h>
#include <hven/detail/sqp/qp_engine.h>
#include <hven/detail/sqp/schur_complement.h>

#include "support/border_test_utils.h"

using namespace hven::solvers;

namespace {

// Repeated verbatim from tests/test_qp_engine.cpp's simple_box_qp().
QpProblem simple_box_qp() {
    // min 1/2(x0^2 + x1^2) - x0 - 2 x1  s.t. x0 + x1 <= 1, 0 <= x <= 10.
    // Optimum (0, 1) is a DEGENERATE vertex: the inequality is active with
    // lambda = 1 and the bound x0 >= 0 is active with a zero multiplier.
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

// K = [H Aᵀ; A 0] with H = diag(2,3), A = [1 1]. Saddle point: inertia
// (2,1,0). (Repeated verbatim from tests/test_kkt_system.cpp's make_kkt3().)
SpMatU make_kkt3() {
    Eigen::MatrixXd D(3, 3);
    D << 2, 0, 1, 0, 3, 1, 1, 1, 0;
    SpMatU K = D.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    K.makeCompressed();
    return K;
}

// --- Equivalence-battery fixtures ---------------------------------------
//
// Repeated VERBATIM (modulo the simple_box_qp above, reused as-is) from
// tests/test_qp_engine.cpp's battery, so BorderModeMatchesRefactorizeMode
// exercises the border path against the exact same problems the refactorize
// path is pinned on -- including the infeasible, crossed-bounds, degenerate
// and start-infeasible ones, whose regularization-artifact regimes are
// precisely where a border/refactorize divergence would hide. This file
// deliberately does not include another test translation unit.

QpProblem equality_only_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 2.0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem all_bounds_active_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -5.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 1.0);
    return qp;
}

QpProblem degenerate_qp() {
    QpProblem qp = simple_box_qp();
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 1, 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(2, 1.0);
    return qp;
}

QpProblem start_infeasible_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -0.5, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << -1, -1, 1, -1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -2.0, 0.3;
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 5.0);
    return qp;
}

QpProblem crossed_bounds_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec(2);
    qp.lower << 2.0, -5.0;
    qp.upper = Vec(2);
    qp.upper << 1.0, 5.0;
    return qp;
}

QpProblem kfixed_probe_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -3.0, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec(2);
    qp.lower << 1.0, -1e20;
    qp.upper = Vec(2);
    qp.upper << 1.0, 1e20;
    return qp;
}

QpProblem badly_scaled_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 0.01, 0.01;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -1.0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem infeasible_bounds_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << -1, 0, 1, 0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -2.0, 1.0;
    qp.lower = Vec::Constant(2, -5.0);
    qp.upper = Vec::Constant(2, 5.0);
    return qp;
}

QpProblem infeasible_with_equality_qp() {
    QpProblem qp = infeasible_bounds_qp();
    Eigen::MatrixXd Aed(1, 2);
    Aed << 0, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 0.0);
    return qp;
}

QpProblem inconsistent_equalities_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(2, 2);
    Aed << 1, 1, 1, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec(2);
    qp.be << 1.0, 2.0;
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -5.0);
    qp.upper = Vec::Constant(2, 5.0);
    return qp;
}

QpProblem equality_outside_box_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, 0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 10.0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 1.0);
    return qp;
}

QpProblem large_solution_free_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1e5, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e7);
    qp.upper = Vec::Constant(2, 1e7);
    return qp;
}

QpProblem large_bound_pinned_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1e6, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec(2);
    qp.lower << 0.0, -1e5;
    qp.upper = Vec::Constant(2, 1e5);
    return qp;
}

QpProblem large_equality_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, 0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 1e5);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem large_solution_in_box_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -2e4, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 3e4);
    return qp;
}

QpProblem ill_scaled_ineq_qp(double a) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << a, a;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -1.0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem ill_scaled_eq_qp(double a) {
    QpProblem qp = ill_scaled_ineq_qp(a);
    Eigen::MatrixXd Aed(1, 2);
    Aed << a, a;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, -1.0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    return qp;
}

QpProblem objective_inflated_lambda_qp(double a, double gap) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << a, a;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 1.0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << a, a;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0 - gap);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem runaway_with_active_row_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 0, 0, 0, 1;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1.0, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 0, -1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -1.0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem psd_singular_unbounded_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 0, 0, 0, 1;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1.0, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem drop_before_infeasible_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -3.0, 3.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << -1, -1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -2.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec(2);
    qp.upper << 1.0, 5.0;
    return qp;
}

QpProblem random_strictly_convex(int n, int mi, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(-1.0, 1.0);

    Eigen::MatrixXd M(n, n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            M(i, j) = unit(rng);
        }
    }
    const Eigen::MatrixXd Hd = M.transpose() * M + Eigen::MatrixXd::Identity(n, n);

    QpProblem qp;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    for (int i = 0; i < n; ++i) {
        qp.g(i) = 2.0 * unit(rng);
    }
    qp.Ae.resize(0, n);
    qp.be = Vec(0);

    Eigen::MatrixXd Aid(mi, n);
    for (int r = 0; r < mi; ++r) {
        for (int j = 0; j < n; ++j) {
            Aid(r, j) = unit(rng);
        }
    }
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(mi);
    for (int r = 0; r < mi; ++r) {
        qp.bi(r) = 0.25 + std::abs(unit(rng));
    }
    qp.lower = Vec::Constant(n, -1.5);
    qp.upper = Vec::Constant(n, 1.5);
    return qp;
}

// random_strictly_convex with the inequality rhs pushed NEGATIVE, so the cold
// start x = clamp(0, l, u) = 0 violates most rows and the shifted-constraint
// homotopy is live from the first iteration. That combination -- rows joining
// the working set mid-run and then being dropped again once their shifts
// close -- is the only one that reaches the "a row added AS A BORDER is
// deactivated" transition (dropping its add_ineq_row border); the strictly
// feasible cold starts above only ever GROW their working set, and the
// over-full warm start below deactivates rows K0 itself owns instead.
QpProblem random_tight(int n, int mi, unsigned seed) {
    QpProblem qp = random_strictly_convex(n, mi, seed);
    std::mt19937 rng(seed + 9973u);
    std::uniform_real_distribution<double> unit(-1.0, 1.0);
    for (int r = 0; r < mi; ++r) {
        qp.bi(r) = -0.3 + std::abs(unit(rng));
    }
    return qp;
}

// Every fixture the refactorize path is pinned on, in one list.
std::vector<std::pair<std::string, QpProblem>> equivalence_battery() {
    std::vector<std::pair<std::string, QpProblem>> cases;
    cases.emplace_back("simple_box_qp", simple_box_qp());
    cases.emplace_back("equality_only_qp", equality_only_qp());
    cases.emplace_back("all_bounds_active_qp", all_bounds_active_qp());
    cases.emplace_back("degenerate_qp", degenerate_qp());
    cases.emplace_back("start_infeasible_qp", start_infeasible_qp());
    cases.emplace_back("crossed_bounds_qp", crossed_bounds_qp());
    cases.emplace_back("kfixed_probe_qp", kfixed_probe_qp());
    cases.emplace_back("badly_scaled_qp", badly_scaled_qp());
    cases.emplace_back("infeasible_bounds_qp", infeasible_bounds_qp());
    cases.emplace_back("infeasible_with_equality_qp", infeasible_with_equality_qp());
    cases.emplace_back("inconsistent_equalities_qp", inconsistent_equalities_qp());
    cases.emplace_back("equality_outside_box_qp", equality_outside_box_qp());
    cases.emplace_back("large_solution_free_qp", large_solution_free_qp());
    cases.emplace_back("large_bound_pinned_qp", large_bound_pinned_qp());
    cases.emplace_back("large_equality_qp", large_equality_qp());
    cases.emplace_back("large_solution_in_box_qp", large_solution_in_box_qp());
    cases.emplace_back("runaway_with_active_row_qp", runaway_with_active_row_qp());
    cases.emplace_back("psd_singular_unbounded_qp", psd_singular_unbounded_qp());
    cases.emplace_back("drop_before_infeasible_qp", drop_before_infeasible_qp());
    for (double a : {1e-2, 3e-3, 1e-3, 3e-4}) {
        cases.emplace_back("ill_scaled_ineq_qp(" + std::to_string(a) + ")", ill_scaled_ineq_qp(a));
        cases.emplace_back("ill_scaled_eq_qp(" + std::to_string(a) + ")", ill_scaled_eq_qp(a));
        cases.emplace_back("objective_inflated_lambda_qp(" + std::to_string(a) + ", 3e-4)",
                           objective_inflated_lambda_qp(a, 3e-4));
    }
    cases.emplace_back("random_strictly_convex(4,2,7)", random_strictly_convex(4, 2, 7));
    cases.emplace_back("random_strictly_convex(5,3,11)", random_strictly_convex(5, 3, 11));
    for (unsigned seed = 0; seed < 50; ++seed) {
        cases.emplace_back("random_strictly_convex(5,3," + std::to_string(seed) + ")",
                           random_strictly_convex(5, 3, seed));
        cases.emplace_back("random_tight(5,4," + std::to_string(seed) + ")",
                           random_tight(5, 4, seed));
    }
    return cases;
}

// Solves `qp` both ways and asserts the two agree on everything the caller
// can observe. On the kInfeasible/kNumericalError paths the multipliers are
// deliberately cleared by BOTH modes (they are 1/dual_mu regularization
// artifacts either way), and the returned x is whatever iterate the loop
// stopped at -- pure artifact territory where the two linear-algebra paths
// have no reason to agree numerically -- so only the status and the
// cleared-multiplier contract are compared there.
void expect_border_matches_refactorize(const QpProblem &qp) {
    QpOptions ref_opts;
    ref_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    QpOptions bor_opts;
    bor_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;

    const auto ref = QpEngine{ref_opts}.solve(qp);
    const auto bor = QpEngine{bor_opts}.solve(qp);

    ASSERT_EQ(bor.status, ref.status);
    if (ref.status == QpStatus::kInfeasible || ref.status == QpStatus::kNumericalError) {
        EXPECT_TRUE(bor.lambda_e.isZero());
        EXPECT_TRUE(bor.lambda_i.isZero());
        EXPECT_TRUE(bor.z.isZero());
        return;
    }
    // Scale-aware on purpose: the battery deliberately includes fixtures whose
    // legitimate answers are large (x ~ 1e5 on the runaway-guard probes,
    // |lambda| ~ 5e6 on the ill-scaled ladder), where a bare absolute 1e-8 /
    // 1e-6 would demand agreement below double precision's own relative
    // resolution. The tolerance is the brief's absolute one whenever the
    // quantity is O(1), and degrades to the same figure RELATIVE to the
    // refactorize answer's magnitude above that.
    auto scaled = [](const Vec &a, const Vec &b, double tol) {
        return (a - b).lpNorm<Eigen::Infinity>() < tol * std::max(1.0, b.lpNorm<Eigen::Infinity>());
    };
    EXPECT_TRUE(scaled(bor.x, ref.x, 1e-8));
    EXPECT_EQ(bor.ineq_active, ref.ineq_active);
    EXPECT_EQ(bor.bound_state, ref.bound_state);
    EXPECT_TRUE(scaled(bor.lambda_e, ref.lambda_e, 1e-6));
    EXPECT_TRUE(scaled(bor.lambda_i, ref.lambda_i, 1e-6));
    EXPECT_TRUE(scaled(bor.z, ref.z, 1e-6));
}

// n=3: x0,x1 share a Hessian block ([[1,1],[1,1]]) that is exactly singular
// along (1,-1) before regularization -- only opts.primal_delta lifts that
// direction's eigenvalue off zero -- while x2 sits in its own independent
// Hessian block (block-diagonal H), decoupled from x0/x1 entirely. g pushes
// x0 and x1 hard toward their shared upper bound (both end up pinned there);
// x2 has its own small linear term and stays free throughout. Used by
// SingularBorderTriggersRebuild below to put TWO pin borders (e0, e1) live
// over the SAME near-null K0 direction simultaneously, and x2's presence is
// what keeps the fully-pinned-x0/x1 state from ever hitting the
// empty-reduced-system short-circuit (eliminated_candidate's fallback must
// actually call solve_eqp, so factorizations counts the recovery).
QpProblem near_singular_border_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(3, 3);
    // clang-format off
    Hd << 1, 1, 0,
          1, 1, 0,
          0, 0, 1;
    // clang-format on
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << -10.0, -10.0, -0.5;
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec(3);
    qp.lower << 0.0, 0.0, -1e6;
    qp.upper = Vec(3);
    qp.upper << 1.0, 1.0, 1e6;
    return qp;
}

} // namespace

// Proves pin-by-border (GMSW form: K0 spans ALL variables, unmodified; a
// variable is pinned at its bound by bordering K0 with a unit column and
// -mu diagonal, with the bound VALUE entering through the border's rhs
// entry) agrees with pin-by-elimination (assemble_kkt's existing path,
// which removes the pinned variable from K entirely and moves its
// contribution into rhs_shift).
TEST(QpEngineBorder, BorderPinEquivalence) {
    QpProblem qp = simple_box_qp();
    QpOptions opts;

    // --- Border-pin path: assemble_kkt_full with an EMPTY working set (no
    // bound eliminated, no inequality row active), then pin variable 0 at
    // its lower bound via a border. ---
    WorkingSet ws_empty(qp.n(), qp.mi());
    KktAssembly full = assemble_kkt_full(qp, ws_empty, opts);

    ASSERT_EQ(full.K.rows(), qp.n()); // me == 0, n_working == 0 for ws_empty
    ASSERT_EQ(full.free_of_full.size(), static_cast<std::size_t>(qp.n()));
    for (Index i = 0; i < qp.n(); ++i) {
        EXPECT_EQ(full.free_of_full[static_cast<std::size_t>(i)], i); // identity map
    }
    EXPECT_DOUBLE_EQ(full.rhs_shift.norm(), 0.0); // all-zero in full mode

    KktSystem kkt0(opts);
    kkt0.factorize(full.K);
    SchurComplement schur(kkt0, opts);

    Vec e0 = Vec::Zero(full.K.rows());
    e0(0) = 1.0;
    schur.add_border(e0, -opts.dual_mu);

    // Full-mode rhs = [-g (all n) | be | bw] with the border rhs appended;
    // the border's rhs entry carries the bound VALUE (lower(0) here), not a
    // residual -- that is what makes the (near-)singular -mu row pin x(0) to
    // that value rather than to 0.
    Vec rhs(full.K.rows() + schur.dim());
    rhs.head(qp.n()) = -qp.g;
    rhs(qp.n()) = qp.lower(0);

    Vec sol = schur.solve(rhs);

    // One step of iterative refinement against the UNREGULARIZED bordered
    // system, mirroring solve_eqp's refinement (eqp_solve.h): the
    // regularized system solved above has +primal_delta on K0's Hessian
    // diagonal and the border's own -dual_mu diagonal; both are subtracted
    // back out of the residual before re-solving with the SAME (already-
    // factorized) regularized system. solve_eqp applies this same step, so
    // without it the two paths agree only to O(primal_delta) ~ 1e-8, not the
    // 1e-9 this test asserts. Shared with BorderOps.PinVariableMatchesEliminated
    // (test_border_ops.cpp) via tests/support/border_test_utils.h.
    std::vector<Vec> border_v{e0};
    std::vector<double> border_d{-opts.dual_mu};
    sol = test_support::refine_bordered_solve(full.K, qp.n(), border_v, border_d, schur, rhs, sol,
                                              opts);
    Vec x_border = sol.head(qp.n());

    // --- Elimination path: the Phase-1 assemble_kkt/solve_eqp path, with
    // variable 0 marked kAtLower in the working set. ---
    WorkingSet ws_pin(qp.n(), qp.mi());
    ws_pin.bound_state()[0] = BoundState::kAtLower;
    KktSystem kkt1(opts);
    EqpResult eqp = solve_eqp(qp, ws_pin, kkt1, opts);

    ASSERT_EQ(x_border.size(), eqp.x.size());
    EXPECT_LT((x_border - eqp.x).norm(), 1e-9);
}

// Phase-1 deferred debt item: KktSystem must remain usable after a move
// construction (the border-mode engine, from this task onward, holds a
// long-lived KktSystem that Task 4 will move around), and the moved-from
// object must destruct without double-freeing Pardiso's internal memory.
TEST(QpEngineBorder, KktSystemMoveKeepsFactorizationUsable) {
    QpOptions opts;
    SpMatU K = make_kkt3();

    KktSystem source(opts);
    source.factorize(K);

    KktSystem dest(std::move(source));

    Vec rhs(3);
    rhs << 1.0, 2.0, 0.5;
    Vec x = dest.solve(rhs);
    Eigen::MatrixXd Kd = Eigen::MatrixXd(K).selfadjointView<Eigen::Upper>();
    EXPECT_LT((Kd * x - rhs).norm(), 1e-10);
    EXPECT_EQ(dest.num_pos_eigs(), 2);
    EXPECT_EQ(dest.num_neg_eigs(), 1);

    // `source` is moved-from; its destructor runs at end of scope. If move
    // construction left it holding a stale Pardiso handle (rather than the
    // nulled-out state KktSystem's move constructor is supposed to leave
    // behind), that destructor would double-release `dest`'s Pardiso memory.
}

// The equivalence oracle for the whole border path: the refactorize path is
// byte-for-byte the Phase-1 loop, so anything the border path decides
// differently -- a different active set, a different multiplier, a different
// status -- is a border-path bug by definition.
TEST(QpEngineBorder, BorderModeMatchesRefactorizeMode) {
    for (const auto &[name, qp] : equivalence_battery()) {
        SCOPED_TRACE(name);
        expect_border_matches_refactorize(qp);
    }
}

// Warm starts seed the INITIAL K0 with a different working set, which is the
// whole point of the design: the same equivalence must hold there.
TEST(QpEngineBorder, BorderModeMatchesRefactorizeModeWarmStarted) {
    // ref_opts is explicit rather than QpOptions{} precisely BECAUSE the
    // default is kSchurBorder: this test's whole point is comparing against
    // the refactorize oracle, so leaving it implicit would silently turn into
    // a vacuous border-vs-border comparison.
    QpOptions ref_opts;
    ref_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;

    const QpProblem qp = random_strictly_convex(5, 3, 3);
    const auto cold = QpEngine{ref_opts}.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);

    QpOptions bor_opts;
    bor_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    const auto ref_warm = QpEngine{ref_opts}.solve(qp, cold);
    const auto bor_warm = QpEngine{bor_opts}.solve(qp, cold);

    ASSERT_EQ(bor_warm.status, ref_warm.status);
    EXPECT_LT((bor_warm.x - ref_warm.x).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_EQ(bor_warm.ineq_active, ref_warm.ineq_active);
    EXPECT_EQ(bor_warm.bound_state, ref_warm.bound_state);
    EXPECT_EQ(bor_warm.counters.factorizations, 1); // one K0, seeded from the warm ws
}

// Deliberately OVER-FULL warm start: every inequality row marked active and
// every variable pinned at its lower bound. This is the only route to the two
// border constructions the cold-start battery above never reaches --
// delete_k0_row (a row K0 itself owns being deactivated) and pin-dropping (a
// seeded bound being released) -- since a cold start seeds K0 with an empty
// or shift-driven working set that the loop then only ever GROWS. Both modes
// must walk back out of that bogus working set to the same optimum.
TEST(QpEngineBorder, BorderModeMatchesRefactorizeModeFromOverfullWarmStart) {
    // Explicit ref_opts: see BorderModeMatchesRefactorizeModeWarmStarted's
    // comment -- QpOptions{} is now kSchurBorder, so the oracle side of this
    // comparison must say so itself.
    QpOptions ref_opts;
    ref_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    QpOptions bor_opts;
    bor_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;

    for (unsigned seed = 0; seed < 20; ++seed) {
        SCOPED_TRACE(seed);
        const QpProblem qp = random_strictly_convex(5, 3, seed);

        QpSolution bogus;
        bogus.x = qp.lower;
        bogus.ineq_active.assign(static_cast<std::size_t>(qp.mi()), true);
        bogus.bound_state.assign(static_cast<std::size_t>(qp.n()), BoundState::kAtLower);

        const auto ref = QpEngine{ref_opts}.solve(qp, bogus);
        const auto bor = QpEngine{bor_opts}.solve(qp, bogus);

        ASSERT_EQ(bor.status, ref.status);
        ASSERT_EQ(ref.status, QpStatus::kOptimal);
        EXPECT_LT((bor.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-8);
        EXPECT_EQ(bor.ineq_active, ref.ineq_active);
        EXPECT_EQ(bor.bound_state, ref.bound_state);
        EXPECT_LT((bor.lambda_i - ref.lambda_i).lpNorm<Eigen::Infinity>(), 1e-6);
        EXPECT_LT((bor.z - ref.z).lpNorm<Eigen::Infinity>(), 1e-6);
    }
}

// n-dimensional all-bounds-active box QP: min 1/2||x - 5*1||^2 s.t.
// 0 <= x <= 1, whose optimum pins every single variable at its upper bound.
// This is the canonical Phase-3 trust-region shape (a box with no general
// rows), and it is the shape that exposes the pin/schur_cap interaction:
// pins can NEVER be folded into K0 (assemble_kkt_full eliminates nothing), so
// once the live pin count passes schur_cap there is no rebuild that reduces
// it.
QpProblem box_all_bounds_active(Index n) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(n, n).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(n, -5.0);
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    qp.Ai.resize(0, n);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(n);
    qp.upper = Vec::Constant(n, 1.0);
    return qp;
}

// PHASE-5 TASK 4. box_all_bounds_active above has pins and NOTHING ELSE, so
// once the latch is taken the working set never changes shape again and the
// latch's RELEASE rule is never exercised. This variant adds the missing
// ingredient -- working ROWS that move WHILE the pin count sits above
// schur_cap:
//
//     min 1/2||x - 5*1||^2   s.t.  -1 <= x <= 1,
//                                  -x_{2j} - x_{2j+1} <= -0.5,  j = 0..pairs-1
//
// The optimum is x = 1 with every variable pinned at its upper bound and NO
// row active (1 + 1 = 2 >= 0.5 strictly). But the COLD START is x = clamp(0)
// = 0, where every one of those rows is VIOLATED (0 < 0.5), so qp_engine.h's
// shifted-constraint homotopy (loop contract step 1) puts all `pairs` of them
// into the initial working set and then DROPS them one at a time as their
// shifts decay -- while the ratio test is simultaneously pinning variables at
// the upper bound. That is the configuration the removed row-change release
// condition thrashed on, and it is why the rows here must be inactive at the
// optimum rather than active: a row that is violated at the start and active
// at the end never moves after the first iteration (see the plain
// box_all_bounds_active case, which is exactly that degenerate variant).
QpProblem box_with_moving_rows(Index n, Index pairs) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(n, n).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(n, -5.0);
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    qp.lower = Vec::Constant(n, -1.0);
    qp.upper = Vec::Constant(n, 1.0);

    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<std::size_t>(2 * pairs));
    for (Index j = 0; j < pairs; ++j) {
        t.emplace_back(static_cast<int>(j), static_cast<int>(2 * j), -1.0);
        t.emplace_back(static_cast<int>(j), static_cast<int>(2 * j + 1), -1.0);
    }
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(pairs, n);
    qp.Ai.setFromTriplets(t.begin(), t.end());
    qp.Ai.makeCompressed();
    qp.bi = Vec::Constant(pairs, -0.5);
    return qp;
}

// simple_box_qp needs exactly two working-set changes after its (empty) seed
// working set: the ratio test adds inequality row 0, then pins x0 at its
// lower bound. In border mode both must be Schur updates over ONE K0
// factorization -- that single factorization count is what separates this
// path from the refactorize path, which spends one per major iteration.
TEST(QpEngineBorder, BorderModeCountsUpdates) {
    QpOptions opts;
    opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    const auto sol = QpEngine{opts}.solve(simple_box_qp());

    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_GE(sol.counters.schur_updates, 2);
    EXPECT_EQ(sol.counters.factorizations, 1);
}

// Rebuild coverage. The battery above never triggers one: every case runs to
// completion on a single K0 (factorizations == 1). Forcing schur_cap to 1
// makes needs_refactorization() fire as soon as a second border is live, so
// the rebuild path -- re-assembling K0 from the CURRENT working set, clearing
// the ledger and re-adding the pins -- is exercised on essentially every
// fixture, and must not move a single answer.
TEST(QpEngineBorder, RebuildUnderTinySchurCapMatchesRefactorize) {
    // Explicit ref_opts: see BorderModeMatchesRefactorizeModeWarmStarted's
    // comment -- QpOptions{} is now kSchurBorder, so the oracle side of this
    // comparison must say so itself.
    QpOptions ref_opts;
    ref_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    QpOptions bor_opts;
    bor_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    bor_opts.schur_cap = 1;

    Index cases_that_rebuilt = 0;
    for (const auto &[name, qp] : equivalence_battery()) {
        SCOPED_TRACE(name);
        const auto ref = QpEngine{ref_opts}.solve(qp);
        const auto bor = QpEngine{bor_opts}.solve(qp);

        ASSERT_EQ(bor.status, ref.status);
        if (ref.status == QpStatus::kInfeasible || ref.status == QpStatus::kNumericalError) {
            continue;
        }
        EXPECT_LT((bor.x - ref.x).lpNorm<Eigen::Infinity>(),
                  1e-8 * std::max(1.0, ref.x.lpNorm<Eigen::Infinity>()));
        EXPECT_EQ(bor.ineq_active, ref.ineq_active);
        EXPECT_EQ(bor.bound_state, ref.bound_state);
        EXPECT_LT((bor.lambda_i - ref.lambda_i).lpNorm<Eigen::Infinity>(),
                  1e-6 * std::max(1.0, ref.lambda_i.lpNorm<Eigen::Infinity>()));
        if (bor.counters.factorizations > 1) {
            ++cases_that_rebuilt;
        }
    }
    EXPECT_GT(cases_that_rebuilt, 0) << "schur_cap = 1 did not force a single rebuild";
}

// PERFORMANCE-SHAPE regression, not a timing test. On a box QP every variable
// is pinned by the end, and pins are borders that no rebuild can absorb --
// so past schur_cap, needs_refactorization() is permanently true. Rebuilding
// on that signal clears the ledger and re-adds every live pin, which makes
// the border bookkeeping QUADRATIC in n and was measured at 5.9x/59x/430x/
// 1423x slower than refactorize mode for n = 128/140/200/300.
//
// The fix serves those iterations from the elimination path instead (where
// pins ARE eliminable) and LATCHES -- it stops maintaining a border stack
// nothing will read -- so border operations stop accruing entirely once the
// dead end is reached.
//
// OBSERVED on this fixture (n = 64, schur_cap = 16), border operations:
//   2008  original (rebuild + re-add every pin, every iteration: quadratic)
//     64  round 1 (fallback, but still syncing: one add per pin)
//     17  round 2 (latched: adds stop at the one that trips the cap)
// End to end against refactorize mode on this shape:
//   n         |   64(cap 16)    128      200       300
//   original  |       --       5.9x     430x     1423x
//   round 1   |     0.80x      5.8x    12.5x      26.7x
//   round 2   |     0.35x      6.5x     5.2x       3.5x
// n = 128 is unchanged across all three because with schur_cap = 128 nothing
// ever trips (factorizations == 1): that column is the cost of pure
// bordering, dominated by the O(dim^3) dense rebuild inside
// SchurComplement::add_border -- a separate, already-documented optimization
// and not a rebuild cliff.
//
// The bound asserted below is the cliff SIGNATURE, not the observed value:
// one border operation per variable is already generous next to the observed
// 17, while the quadratic behavior overshoots it by 30x.
TEST(QpEngineBorder, PinsPastSchurCapDoNotRebuildQuadratically) {
    constexpr Index kN = 64;
    const QpProblem qp = box_all_bounds_active(kN);

    // Explicit ref_opts: see BorderModeMatchesRefactorizeModeWarmStarted's
    // comment -- QpOptions{} is now kSchurBorder, so the oracle side of this
    // comparison must say so itself.
    QpOptions ref_opts;
    ref_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    QpOptions bor_opts;
    bor_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    bor_opts.schur_cap = 16;

    const auto ref = QpEngine{ref_opts}.solve(qp);
    const auto bor = QpEngine{bor_opts}.solve(qp);

    ASSERT_EQ(ref.status, QpStatus::kOptimal);
    ASSERT_EQ(bor.status, QpStatus::kOptimal);
    EXPECT_LT((bor.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_EQ(bor.bound_state, ref.bound_state);

    EXPECT_LE(bor.counters.schur_updates, kN);
    // Weak by comparison -- it holds for the quadratic behavior too, since
    // that wasted its effort on borders rather than factorizations -- but it
    // is the invariant that says the fallback never costs MORE factorizations
    // than refactorize mode would have. The schur_updates bound above is the
    // one that actually detects the cliff.
    EXPECT_LE(bor.counters.factorizations, bor.counters.minor_iters + 1);
}

// PHASE-5 TASK 4 -- THE LATCH RELEASE RULE. A PERFORMANCE-SHAPE regression
// exactly like PinsPastSchurCapDoNotRebuildQuadratically above, and the
// regression net for the policy change ruled in
// docs/notes/2026-07-31-schur-cap-policy.md.
//
// THE CLAIM UNDER TEST. While the pin count exceeds schur_cap, bordering is
// unavailable at ANY price -- every possible K0 is born with dim() == pinned >
// schur_cap -- so a working-ROW change must not release the latch. It used to:
// latch_still_holds released whenever ws.active_ineq() != border.k0_rows, on a
// (measured false) claim that the resulting release/re-take cost no more than
// one refactorize-mode iteration. It costs a full-variable K0 assembly +
// factorization + Pardiso symbolic re-analysis + a re-add of every pin, and it
// buys a state that trips the cap again before anything is solved.
//
// box_with_moving_rows is the shape that exposes it: 64 pins against a cap of
// 16, with 12 rows dropping underneath. MEASURED on this fixture, clang/MKL,
// this machine, border mode with schur_cap = 16:
//
//     quantity                     row-change release   pin-count only
//                                    (through Phase 4)     (Task 4 on)
//     minor_iters                            78                 78
//     x, bound_state, ineq_active      identical          identical
//     schur_updates                         629                 17
//     factorizations                         73                 61
//     (refactorize-mode reference: 78 minors, 77 factorizations)
//
// The ASSERTED bound is the signature, not the observed value: the number of
// border operations must stay bounded by the pin count (one add per live pin,
// the same bound PinsPastSchurCapDoNotRebuildQuadratically asserts), which the
// thrash overshoots by 10x. The minor count is asserted EQUAL to refactorize
// mode's, which is what makes this a cost claim and not an answers claim. Run
// against the pre-Task-4 engine this test fails on exactly the schur_updates
// bound, 629 vs 64, and on nothing else.
TEST(QpEngineBorder, LatchedBorderModeDoesNotThrashOnRowChanges) {
    constexpr Index kN = 64;
    constexpr Index kPairs = 12;
    constexpr Index kCap = 16;
    const QpProblem qp = box_with_moving_rows(kN, kPairs);

    QpOptions ref_opts;
    ref_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    QpOptions bor_opts;
    bor_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    bor_opts.schur_cap = kCap;

    const auto ref = QpEngine{ref_opts}.solve(qp);
    const auto bor = QpEngine{bor_opts}.solve(qp);

    ASSERT_EQ(ref.status, QpStatus::kOptimal);
    ASSERT_EQ(bor.status, QpStatus::kOptimal);

    // The fixture is only a latch test if it is actually in the dead end --
    // re-derived from the returned solution rather than assumed.
    const Index pinned =
        static_cast<Index>(std::count_if(bor.bound_state.begin(), bor.bound_state.end(),
                                         [](BoundState s) { return s != BoundState::kFree; }));
    ASSERT_GT(pinned, kCap) << "fixture no longer reaches the pins-only dead end";
    // ... and only a row test if the rows genuinely MOVE: every one of them
    // is violated at the cold start x = clamp(0) (so the homotopy loads them
    // all into the initial working set) and inactive at the optimum (so every
    // one of them is dropped again while the latch is held). Both halves are
    // re-derived from the QP and the solution, not assumed.
    const Vec ai_at_start = qp.Ai * Vec::Zero(kN);
    for (Index j = 0; j < kPairs; ++j) {
        ASSERT_GT(ai_at_start(j), qp.bi(j)) << "row " << j << " is not violated at the cold start";
    }
    ASSERT_EQ(std::count(bor.ineq_active.begin(), bor.ineq_active.end(), true), 0)
        << "fixture no longer DROPS its rows, so none moves under the latch";

    // SAME ANSWER, against the equivalence oracle.
    EXPECT_LT((bor.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_EQ(bor.bound_state, ref.bound_state);
    EXPECT_EQ(bor.ineq_active, ref.ineq_active);

    // SAME WORKING-SET PATH -- cost, not answers.
    EXPECT_EQ(bor.counters.minor_iters, ref.counters.minor_iters);

    // THE THRASH. One border operation per live pin is already generous next
    // to the observed 5; the released-on-every-row-change behaviour spends
    // 629 here (the table above) because each release re-adds the whole pin
    // stack.
    EXPECT_LE(bor.counters.schur_updates, pinned);
    // And a latched solve must never pay MORE factorizations than refactorize
    // mode would have: every latched iteration IS a refactorize-mode
    // iteration, plus at most the handful of rebuilds before the latch took.
    EXPECT_LE(bor.counters.factorizations, ref.counters.factorizations);

    RecordProperty("pinned_at_exit", fmt::format("{}", pinned));
    RecordProperty("minors_border_vs_refactorize",
                   fmt::format("{} vs {}", bor.counters.minor_iters, ref.counters.minor_iters));
    RecordProperty("schur_updates", fmt::format("{}", bor.counters.schur_updates));
    RecordProperty(
        "factorizations_border_vs_refactorize",
        fmt::format("{} vs {}", bor.counters.factorizations, ref.counters.factorizations));
}

// Task 4, test (c): QpOptions{}.ws_algebra == kSchurBorder. Written FIRST
// (TDD) against the pre-flip default; the flip in types.h is what makes it
// pass.
TEST(QpEngineBorder, DefaultIsBorderMode) {
    EXPECT_EQ(QpOptions{}.ws_algebra, WorkingSetLinearAlgebra::kSchurBorder);
}

// Task 4, test (b): an ENGINE-level fixture that fires needs_refactorization()
// via nearly_singular() -- the guard Schur.NearlySingularOneByOneNeedsRefactorization
// (test_schur.cpp) already covers at the SchurComplement level, but which no
// shipped engine-driven battery had actually triggered through real borders
// (task-3-report.md's fix round 2, concern 2). An exactly singular C was
// deliberately NOT targeted: reviewers repeatedly failed to construct one
// through real border sequences (the -dual_mu diagonal on every pin/row
// border keeps C off exact zero even when its underlying K0 direction is
// itself singular -- see near_singular_border_qp's comment), so this goes
// after the NEARLY-singular path the brief names as the fallback target.
//
// --- How the fixture fires it ---
//
// near_singular_border_qp()'s x0/x1 block is exactly singular along (1,-1)
// before regularization; the only thing giving that direction a nonzero
// eigenvalue is opts.primal_delta. Cold-started, both x0 and x1 hit their
// shared upper bound and get pinned ONE AT A TIME (pin_variable borders e0,
// e1) over the SAME K0 -- so partway through the solve, C carries both pins
// simultaneously, live over a near-null K0 direction.
//
// Direct hand/probe algebra (SchurComplement, not just the 2x2 KKT
// sub-block) shows the resulting 2x2 C's eigenvalues are approximately
// {-(mu + 2a), -mu*2a/(mu+2a)} where a = (K0^-1)_{00} ~ 1/(2*primal_delta):
// the SMALLER eigenvalue collapses toward an O(1) constant (NOT toward mu,
// which was the first, wrong hand-derivation -- the (1+delta) vs 1 numerator
// asymmetry between K0^-1's diagonal and off-diagonal entries dominates it),
// while the larger one grows like 1/primal_delta. So shrinking primal_delta
// widens the ratio without bound. Empirically (LD_LIBRARY_PATH-linked probe
// against the shipped SchurComplement, not asserted here since it is
// implementation-internal): at the defaults (primal_delta = 1e-8) the ratio
// is ~5e-9 (needs_refactorization() stays false, matching
// BorderModeMatchesRefactorizeMode's factorizations == 1 on similar
// fixtures); measured crossing to needs_refactorization() firing sits within
// a factor of ~2 of that same 5e-9 (not the 1e-11-to-1e-12 pair an earlier
// pass at this comment claimed -- kept order-of-magnitude here rather than
// re-pinned to a second decimal, since the probe is not asserted). The
// shipped primal_delta = 1e-13 below has enormous margin past that
// crossing, not a hair-trigger value.
//
// opts.schur_cond_max is separately cranked to 1e30 so this fixture is
// attributable to nearly_singular() ALONE: with the crossing above, C's
// cond_estimate() is only ~1e11-1e12, comfortably under even schur_cond_max's
// DEFAULT (1e9) at smaller primal_delta values already, which would leave it
// ambiguous which guard fired. At 1e30 only nearly_singular() can trip
// needs_refactorization() here (dim() never exceeds 2, far under schur_cap).
//
// The trigger point is ALSO the pins-only latch (rebuild_would_be_noop is
// true: no inequality rows exist in this fixture at all, and both live
// borders are pins), so recovery here is via border_solve_or_fall_back's
// latch branch -- eliminated_candidate, not a K0 rebuild. x2 (the
// independent free block) is what keeps that fallback from hitting the
// empty-reduced-system short-circuit, so it actually spends a solve_eqp
// factorization every time the latch takes (and, per the latch's own
// documented oscillation -- pinned count 2 is never > schur_cap, so
// latch_still_holds() is false and the NEXT iteration releases the latch,
// rebuilds K0, immediately re-triggers, and re-latches -- one more
// factorization per iteration from there on), which is what pushes
// factorizations comfortably past 1.
TEST(QpEngineBorder, SingularBorderTriggersRebuild) {
    const QpProblem qp = near_singular_border_qp();

    QpOptions opts;
    opts.primal_delta = 1e-13;
    opts.schur_cond_max = 1e30; // isolate nearly_singular() from schur_cond_max

    QpOptions ref_opts = opts;
    ref_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    QpOptions bor_opts = opts;
    bor_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;

    const auto ref = QpEngine{ref_opts}.solve(qp);
    ASSERT_EQ(ref.status, QpStatus::kOptimal);

    QpSolution bor;
    EXPECT_NO_THROW(bor = QpEngine{bor_opts}.solve(qp)); // no throw escapes solve()

    ASSERT_EQ(bor.status, ref.status);
    EXPECT_LT((bor.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_EQ(bor.bound_state, ref.bound_state);
    EXPECT_GT(bor.counters.factorizations, 1); // the guard fired and recovered, more than once
}

// Deferred debt M5 (task-3-report.md): every border-mode test up to this
// point exercises QpOptions{} (mod ws_algebra alone). box_all_bounds_active(4)
// pins every variable, which -- combined with schur_cap = 2 below -- forces
// the pins-past-cap latch path (see PinsPastSchurCapDoNotRebuildQuadratically)
// to fire under FOUR simultaneously non-default options at once, checking
// that combination doesn't interact badly with the border machinery.
TEST(QpEngineBorder, BorderModeWithNonDefaultOptions) {
    const QpProblem qp = box_all_bounds_active(4);

    QpOptions opts;
    opts.max_iter = 7;
    opts.feas_tol = 1e-7;
    opts.schur_cap = 2;
    opts.schur_cond_max = 1e6;

    QpOptions ref_opts = opts;
    ref_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    QpOptions bor_opts = opts;
    bor_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;

    const auto ref = QpEngine{ref_opts}.solve(qp);
    const auto bor = QpEngine{bor_opts}.solve(qp);

    ASSERT_EQ(ref.status, QpStatus::kOptimal);
    ASSERT_EQ(bor.status, ref.status);
    EXPECT_LT((bor.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-7);
    EXPECT_EQ(bor.bound_state, ref.bound_state);
    EXPECT_LT((bor.z - ref.z).lpNorm<Eigen::Infinity>(), 1e-5);
}
