// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_qp_lift_oracle.cpp -- the mixed-lift adapter (mixed_lift.h) against the DENSE-B
// ORACLE (M7 QN stage 2; rulings C30 BLOCKING-1/BLOCKING-2, AMEND-6/8/9). No driver is involved.
//
// The oracle: the same random convex QP solved (a) LIFTED -- MixedLift of the quasi-Newton B from
// a few LbfgsDamped updates, through the engine, un-lifted -- and (b) with the dense B placed
// directly in QpProblem::H. Both go through the same engine at the same radius. Engines: the
// walk (both ws_algebra modes), the interior tier and SSN.
//
// THE TOLERANCE (derived, not tuned). The two arms solve the same QP up to what the lifted
// formulation's regularization and the engines' stopping tolerances perturb:
//   * the walk regularizes each equality row with -mu I (QpOptions::dual_mu). For a lifted row
//     kappa u' - Up^T p = mu*lambda with stationarity lambda = -theta u this gives
//     u = Up^T p / (1 + mu theta): the operative curvature on that mode is theta/(1 + mu theta),
//     i.e. B is perturbed by Delta B = -mu theta^2 Up Up^T, ||Delta B||_2 <= mu theta_max^2;
//   * the primal term (primal_delta/2)||x - x_k||^2 is centred at the iterate: O(delta) on p;
//   * every engine stops at a tolerance eps (the walk's opt_tol, the others' ~1e-8).
// Solving the perturbed QP changes p by at most ||Delta||/lambda_min(B) (the reduced Hessian is
// bounded below by B's own smallest eigenvalue), so
//     tol_p = C ((mu theta_max^2 + delta) max(1, ||p||_inf) + eps (1 + lambda_max(B))) /
//     lambda_min(B)
// with C = 50 absorbing constants and the engine-specific stop rules. The multipliers solve
// M^T y = -(g + B p) on the active rows M (equalities, active inequalities, active bounds and
// TR pins), so
//     tol_y = C sqrt(n) (lambda_max tol_p + mu theta_max^2 max(1, ||p||_inf)) / sigma_min(M)
// with sigma_min(M) read from the oracle's own active set.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/globalization/sqp/elastic.h>
#include <hven/detail/globalization/sqp/lbfgs_damped.h>
#include <hven/detail/globalization/sqp/mixed_lift.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>

namespace hven::solvers {
namespace {

using detail::LbfgsDamped;
using detail::MixedLift;
using detail::MixedLiftData;
using Eigen::MatrixXd;
using Eigen::VectorXd;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kEngineEps = 1e-8; // the interior tier's and SSN's stopping scale

enum class Mode { kWalkBorder, kWalkRefactorize, kIpm, kSsn };

const char *name_of(Mode m) {
    switch (m) {
    case Mode::kWalkBorder:
        return "walk/border";
    case Mode::kWalkRefactorize:
        return "walk/refactorize";
    case Mode::kIpm:
        return "ipm";
    case Mode::kSsn:
        return "ssn";
    }
    return "?";
}

// ------------------------------------------------------------------------------------------
// Problem generation
// ------------------------------------------------------------------------------------------

MatrixXd random_symmetric(std::mt19937 &g, int n, double lo, double hi) {
    std::normal_distribution<double> nd;
    MatrixXd A(n, n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            A(i, j) = nd(g);
        }
    }
    const MatrixXd Q = Eigen::HouseholderQR<MatrixXd>(A).householderQ();
    VectorXd d(n);
    for (int i = 0; i < n; ++i) {
        d(i) = lo + (hi - lo) * (static_cast<double>(i) / (n - 1));
    }
    return Q * d.asDiagonal() * Q.transpose();
}

VectorXd random_vec(std::mt19937 &g, int n, double scale = 1.0) {
    std::normal_distribution<double> nd;
    VectorXd v(n);
    for (int i = 0; i < n; ++i) {
        v(i) = scale * nd(g);
    }
    return v;
}

SpMatRM sparse_of(const MatrixXd &M) {
    std::vector<Eigen::Triplet<double>> t;
    for (int i = 0; i < M.rows(); ++i) {
        for (int j = 0; j < M.cols(); ++j) {
            t.emplace_back(i, j, M(i, j));
        }
    }
    SpMatRM out(M.rows(), M.cols());
    out.setFromTriplets(t.begin(), t.end());
    out.makeCompressed();
    return out;
}

// A feasible-at-p_ref QP with bounds, equalities and inequalities and no Hessian (H = 0 until
// the arm under test fills it in). exclude_zero moves some variables' boxes off the origin so
// that p_ref = clamp(0) is NONzero (exercises the shift).
QpProblem random_qp(std::mt19937 &g, int n, int me, int mi, bool exclude_zero) {
    std::uniform_real_distribution<double> u(0.0, 1.0);
    QpProblem qp;
    qp.H = SpMatRM(n, n);
    qp.g = random_vec(g, n, 2.0);
    qp.lower = VectorXd::Constant(n, -kInf);
    qp.upper = VectorXd::Constant(n, kInf);
    for (int i = 0; i < n; ++i) {
        const double r = u(g);
        if (r < 0.15) {
            continue; // free
        }
        qp.lower(i) = -(0.5 + 1.5 * u(g));
        qp.upper(i) = 0.5 + 1.5 * u(g);
        if (exclude_zero && i % 3 == 0) {
            qp.lower(i) = 0.3 + 0.3 * u(g);
            qp.upper(i) = 2.0 + u(g);
        }
    }
    const VectorXd p_ref = qp.lower.cwiseMax(VectorXd::Zero(n)).cwiseMin(qp.upper);
    const MatrixXd Ae = random_vec(g, me * n).reshaped(me, n);
    const MatrixXd Ai = random_vec(g, mi * n).reshaped(mi, n);
    qp.Ae = sparse_of(Ae);
    qp.Ai = sparse_of(Ai);
    qp.be = Ae * p_ref;
    qp.bi = Ai * p_ref;
    for (int j = 0; j < mi; ++j) {
        qp.bi(j) += 0.1 + 0.4 * u(g);
    }
    return qp;
}

struct Quasi {
    MatrixXd B;
    MixedLiftData d;
    double lambda_min = 0.0, lambda_max = 0.0, theta_max = 0.0;
};

// B from a few random damped updates against an indefinite Hessian; retried until the eigen
// form carries BOTH signs of theta.
Quasi random_quasi_newton(std::mt19937 &g, int n) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        const MatrixXd Ht = random_symmetric(g, n, -2.0, 3.0);
        LbfgsDamped q(n, 1.0, {.memory = 4});
        for (int k = 0; k < 6; ++k) {
            const VectorXd s = random_vec(g, n, 0.5);
            q.update(s, Ht * s, 1.0);
        }
        const MixedLiftData &d = q.mixed_lift();
        if (d.u_plus.cols() == 0 || d.v.cols() == 0) {
            continue;
        }
        Quasi out;
        out.d = d;
        out.B.resize(n, n);
        for (int j = 0; j < n; ++j) {
            VectorXd e = VectorXd::Zero(n);
            e(j) = 1.0;
            out.B.col(j) = q.apply(e);
        }
        out.B = 0.5 * (out.B + out.B.transpose());
        out.lambda_min = q.min_eigenvalue();
        out.lambda_max = q.max_eigenvalue();
        out.theta_max = d.theta_plus.maxCoeff();
        return out;
    }
    ADD_FAILURE() << "no quasi-Newton B with both signs of theta found";
    return {};
}

// B placed directly in H (upper triangle), zero-padded to n_total (slack columns).
QpProblem with_dense_h(QpProblem qp, const MatrixXd &B) {
    const int n_total = static_cast<int>(qp.n());
    std::vector<Eigen::Triplet<double>> t;
    for (int i = 0; i < B.rows(); ++i) {
        for (int j = i; j < B.cols(); ++j) {
            t.emplace_back(i, j, B(i, j));
        }
    }
    qp.H = SpMatRM(n_total, n_total);
    qp.H.setFromTriplets(t.begin(), t.end());
    qp.H.makeCompressed();
    return qp;
}

// ------------------------------------------------------------------------------------------
// Solving
// ------------------------------------------------------------------------------------------

struct Out {
    QpStatus status = QpStatus::kNumericalError;
    Vec x, lambda_e, z;
    std::vector<bool> tr_active, ineq_active;
    std::vector<BoundState> bound_state;
    QpCounters walk;
    // engine-specific "the unmodified system was PD" evidence
    bool clean_inertia = true;
};

Out solve_mode(Mode m, const QpProblem &qp, double radius, bool known_convex,
               const Vec *start_x = nullptr, const QpSolution *seed = nullptr) {
    QpOptions o;
    o.hessian_known_convex = known_convex;
    SolveOverrides ov;
    ov.tr_radius = radius;
    Out out;
    switch (m) {
    case Mode::kWalkBorder:
    case Mode::kWalkRefactorize: {
        o.ws_algebra = m == Mode::kWalkBorder ? WorkingSetLinearAlgebra::kSchurBorder
                                              : WorkingSetLinearAlgebra::kRefactorize;
        QpEngine eng(o);
        const QpSolution s = seed != nullptr ? eng.solve(qp, *seed, ov) : eng.solve(qp, ov);
        out.status = s.status;
        out.x = s.x;
        out.lambda_e = s.lambda_e;
        out.z = s.z;
        out.tr_active = s.tr_active;
        out.ineq_active = s.ineq_active;
        out.bound_state = s.bound_state;
        out.walk = s.counters;
        const QpCounters &c = s.counters;
        out.clean_inertia = c.suspect_escalations == 0 && c.saddle_exits == 0 &&
                            c.saddle_exit_misses == 0 && c.saddle_direction_solves == 0 &&
                            c.convex_certificates == 0 && c.dep_inertia_rereads == 0 &&
                            c.dep_inertia_reread_flips == 0 && c.dep_repair_restarts == 0 &&
                            c.dep_repair_budget_stops == 0;
        break;
    }
    case Mode::kIpm: {
        IpqpEngine eng(o);
        const IpqpResult r = eng.solve(qp, nullptr, IpqpOptions{}, ov);
        out.status = r.status;
        out.x = r.x;
        out.lambda_e = r.lambda_e;
        out.z = r.z;
        out.tr_active = r.tr_active;
        out.ineq_active = r.ineq_active;
        out.bound_state = r.bound_state;
        out.clean_inertia = !r.certificate_downgraded && !r.inertia_evidence_failed &&
                            r.counters.ipqp_inertia_retries == 0 && r.rho_mod == 0.0;
        break;
    }
    case Mode::kSsn: {
        SsnEngine eng(o);
        SsnStart start;
        if (start_x != nullptr) {
            start.x = *start_x;
            start.box_center = *start_x;
        }
        SsnResult r;
        SsnOptions sopts;
        sopts.hard_budget =
            400; // SSN is Newton on a nonsmooth F: a tight radius needs many steps, the default 25
                 // is for the driver, which routes a kBudget escape to the walk
        eng.solve(qp, start, sopts, ov, &r);
        out.status = r.status;
        out.x = r.x;
        out.lambda_e = r.lambda_e;
        out.z = r.z;
        out.tr_active = r.tr_active;
        out.ineq_active = r.ineq_active;
        out.bound_state = r.bound_state;
        out.clean_inertia = r.escape_reason == SsnEscape::kNone;
        break;
    }
    }
    return out;
}

Vec clamp0(const QpProblem &qp) {
    return qp.lower.cwiseMax(VectorXd::Zero(qp.n())).cwiseMin(qp.upper);
}

struct Pair {
    Out lifted; // un-lifted
    Out lifted_raw;
    Out dense;
};

// Solves the unlifted `qp` both ways. `n_struct` leading variables carry B.
Pair run_pair(Mode m, const QpProblem &qp, const Quasi &q, Index n_struct, double radius,
              const QpSolution *seed_unlifted = nullptr) {
    Pair p;
    const MixedLift L(qp, n_struct, q.d);
    const QpProblem dense_qp = with_dense_h(qp, q.B);
    const Vec p_ref = clamp0(qp);
    const Vec lifted_ref = L.lift_primal(p_ref);
    // the lifted arm: the production configuration, hessian_known_convex asserted
    QpSolution lseed;
    if (seed_unlifted != nullptr) {
        lseed = L.lift_seed(*seed_unlifted);
    }
    p.lifted_raw = solve_mode(m, L.problem(), radius, true, &lifted_ref,
                              seed_unlifted != nullptr ? &lseed : nullptr);
    p.lifted = p.lifted_raw;
    if (p.lifted_raw.x.size() == L.n_lifted()) {
        p.lifted.x = L.unlift_primal(p.lifted_raw.x);
        p.lifted.z = p.lifted_raw.z.head(L.n_orig());
        p.lifted.lambda_e = L.unlift_equality_multipliers(p.lifted_raw.lambda_e);
        p.lifted.tr_active.assign(p.lifted_raw.tr_active.begin(),
                                  p.lifted_raw.tr_active.begin() + L.n_orig());
        p.lifted.bound_state.assign(p.lifted_raw.bound_state.begin(),
                                    p.lifted_raw.bound_state.begin() + L.n_orig());
    }
    p.dense = solve_mode(m, dense_qp, radius, false, &p_ref, seed_unlifted);
    return p;
}

double sigma_min_of_active(const QpProblem &qp, const Out &o) {
    const Index n = qp.n();
    std::vector<VectorXd> rows;
    for (Index k = 0; k < qp.me(); ++k) {
        rows.push_back(VectorXd(qp.Ae.row(k)));
    }
    for (Index j = 0; j < qp.mi(); ++j) {
        if (j < static_cast<Index>(o.ineq_active.size()) && o.ineq_active[static_cast<size_t>(j)]) {
            rows.push_back(VectorXd(qp.Ai.row(j)));
        }
    }
    for (Index i = 0; i < n; ++i) {
        const auto si = static_cast<size_t>(i);
        const bool pinned = (si < o.bound_state.size() && o.bound_state[si] != BoundState::kFree) ||
                            (si < o.tr_active.size() && o.tr_active[si]);
        if (pinned) {
            VectorXd e = VectorXd::Zero(n);
            e(i) = 1.0;
            rows.push_back(e);
        }
    }
    if (rows.empty()) {
        return 1.0;
    }
    MatrixXd M(static_cast<Index>(rows.size()), n);
    for (size_t r = 0; r < rows.size(); ++r) {
        M.row(static_cast<Index>(r)) = rows[r].transpose();
    }
    const Eigen::BDCSVD<MatrixXd> svd(M);
    const VectorXd s = svd.singularValues();
    double smin = s(s.size() - 1);
    for (Index k = s.size() - 1; k >= 0; --k) {
        if (s(k) > 1e-9 * s(0)) {
            smin = s(k);
            break;
        }
    }
    return smin;
}

struct Worst {
    double p = 0.0, y = 0.0; // observed / tolerance ratios
};
Worst g_worst; // reported at the end of the suite (informational)

void compare(const std::string &tag, const QpProblem &qp, const Quasi &q, const Pair &pr,
             const QpOptions &opts, Mode mode) {
    ASSERT_EQ(static_cast<int>(pr.dense.status), static_cast<int>(QpStatus::kOptimal)) << tag;
    ASSERT_EQ(static_cast<int>(pr.lifted.status), static_cast<int>(pr.dense.status)) << tag;
    const double mu = opts.dual_mu, delta = opts.primal_delta;
    const double eps = mode == Mode::kIpm || mode == Mode::kSsn ? kEngineEps : opts.opt_tol;
    const double pnorm = std::max(1.0, pr.dense.x.lpNorm<Eigen::Infinity>());
    const double mu_theta2 = mu * q.theta_max * q.theta_max;
    const double tol_p =
        50.0 * ((mu_theta2 + delta) * pnorm + eps * (1.0 + q.lambda_max)) / q.lambda_min;
    const double sig = sigma_min_of_active(qp, pr.dense);
    const double tol_y = 50.0 * std::sqrt(static_cast<double>(qp.n())) *
                         (q.lambda_max * tol_p + mu_theta2 * pnorm) / sig;

    const double dp = (pr.lifted.x - pr.dense.x).lpNorm<Eigen::Infinity>();
    EXPECT_LE(dp, tol_p) << tag << " p";
    const double dz = (pr.lifted.z - pr.dense.z).lpNorm<Eigen::Infinity>();
    EXPECT_LE(dz, tol_y) << tag << " z";
    ASSERT_EQ(pr.lifted.lambda_e.size(), pr.dense.lambda_e.size()) << tag;
    const double dl = pr.dense.lambda_e.size() == 0
                          ? 0.0
                          : (pr.lifted.lambda_e - pr.dense.lambda_e).lpNorm<Eigen::Infinity>();
    EXPECT_LE(dl, tol_y) << tag << " lambda_e";
    g_worst.p = std::max(g_worst.p, dp / tol_p);
    g_worst.y = std::max({g_worst.y, dz / tol_y, dl / tol_y});
}

// BLOCKING-1: the window never binds an auxiliary, on every engine, at every radius.
void expect_aux_free(const std::string &tag, const MixedLift &L, const Out &raw, double radius,
                     const Vec &lifted_ref) {
    ASSERT_EQ(static_cast<Index>(raw.tr_active.size()), L.n_lifted()) << tag;
    for (Index i = L.n_orig(); i < L.n_lifted(); ++i) {
        EXPECT_FALSE(raw.tr_active[static_cast<size_t>(i)]) << tag << " aux " << i;
        if (std::isfinite(radius)) {
            // stronger than the flag: strictly inside half the window (the scale's own margin)
            EXPECT_LE(std::abs(raw.x(i) - lifted_ref(i)), 0.5 * radius + 1e-6)
                << tag << " aux " << i << " strays beyond Delta/2";
        }
    }
}

const double kRadii[] = {kInf, 5.0, 1.0, 0.3, 0.05};
const Mode kModes[] = {Mode::kWalkBorder, Mode::kWalkRefactorize, Mode::kIpm, Mode::kSsn};

} // namespace

// ------------------------------------------------------------------------------------------
// The oracle
// ------------------------------------------------------------------------------------------

TEST(QpLiftOracle, LiftedSolutionMatchesTheDenseBOracleOnEveryEngineAndRadius) {
    const QpOptions defaults;
    int cells = 0;
    int ssn_oracle_unconverged = 0;
    for (unsigned seed = 1; seed <= 6; ++seed) {
        for (const bool exclude_zero : {false, true}) {
            std::mt19937 g(1000u * seed + (exclude_zero ? 7u : 0u));
            const int n = 10;
            const QpProblem qp = random_qp(g, n, 2, 4, exclude_zero);
            const Quasi q = random_quasi_newton(g, n);
            ASSERT_GT(q.d.u_plus.cols(), 0);
            ASSERT_GT(q.d.v.cols(), 0);
            const MixedLift L(qp, n, q.d);
            for (const Mode m : kModes) {
                for (const double radius : kRadii) {
                    const std::string tag =
                        std::string(name_of(m)) + " seed=" + std::to_string(seed) +
                        " r=" + std::to_string(radius) + (exclude_zero ? " x0" : "");
                    const Pair pr = run_pair(m, qp, q, n, radius);
                    if (m == Mode::kSsn && pr.dense.status != QpStatus::kOptimal) {
                        // SSN is Newton on a nonsmooth F and does not always converge at a
                        // tight radius (even at hard_budget 400). A cell whose ORACLE arm did
                        // not reach kOptimal compares nothing; it is counted, not hidden.
                        ++ssn_oracle_unconverged;
                        continue;
                    }
                    compare(tag, qp, q, pr, defaults, m);
                    expect_aux_free(tag, L, pr.lifted_raw, radius, L.lift_primal(clamp0(qp)));
                    // PD by construction: the inertia gate reads kOk, the repair and saddle
                    // machinery never runs (sections 4b/4c dormant).
                    EXPECT_TRUE(pr.lifted_raw.clean_inertia) << tag;
                    ++cells;
                }
            }
        }
    }
    std::printf("[ lift-oracle ] %d cells (+%d SSN cells skipped: the dense oracle itself did not "
                "converge); worst observed/tolerance ratios: p %.3g, multipliers "
                "%.3g\n",
                cells, ssn_oracle_unconverged, g_worst.p, g_worst.y);
    EXPECT_LE(ssn_oracle_unconverged, 6) << "SSN's own convergence is not what this test pins";
}

// NEGATIVE CONTROL: the tolerance discriminates. The same lifted solve against a dense B that is
// off by a relative 1e-3 (a B the lift did not carry) lands OUTSIDE tol_p on most cells.
TEST(QpLiftOracle, TheToleranceDiscriminatesAWrongB) {
    const QpOptions defaults;
    int outside = 0, cells = 0;
    for (unsigned seed = 1; seed <= 6; ++seed) {
        std::mt19937 g(1000u * seed);
        const int n = 10;
        const QpProblem qp = random_qp(g, n, 2, 4, false);
        const Quasi q = random_quasi_newton(g, n);
        const MixedLift L(qp, n, q.d);
        const Out lifted = solve_mode(Mode::kWalkBorder, L.problem(), kInf, true);
        const Out wrong = solve_mode(Mode::kWalkBorder, with_dense_h(qp, 1.001 * q.B), kInf, false);
        ASSERT_EQ(static_cast<int>(lifted.status), static_cast<int>(QpStatus::kOptimal));
        ASSERT_EQ(static_cast<int>(wrong.status), static_cast<int>(QpStatus::kOptimal));
        const double pnorm = std::max(1.0, wrong.x.lpNorm<Eigen::Infinity>());
        const double tol_p =
            50.0 *
            ((defaults.dual_mu * q.theta_max * q.theta_max + defaults.primal_delta) * pnorm +
             defaults.opt_tol * (1.0 + q.lambda_max)) /
            q.lambda_min;
        ++cells;
        outside += (L.unlift_primal(lifted.x) - wrong.x).lpNorm<Eigen::Infinity>() > tol_p ? 1 : 0;
    }
    EXPECT_GE(outside, cells - 1) << "the oracle must not pass a wrong B";
}

// kWalk: the flag is dormant on a PD lifted H, so asserting it changes nothing -- the lifted
// solve is bit-identical with and without it.
TEST(QpLiftOracle, TheConvexFlagIsDormantOnTheLiftedQp) {
    std::mt19937 g(77);
    const int n = 10;
    const QpProblem qp = random_qp(g, n, 2, 4, false);
    const Quasi q = random_quasi_newton(g, n);
    const MixedLift L(qp, n, q.d);
    for (const Mode m : {Mode::kWalkBorder, Mode::kWalkRefactorize}) {
        for (const double radius : {kInf, 0.3}) {
            const Out a = solve_mode(m, L.problem(), radius, true);
            const Out b = solve_mode(m, L.problem(), radius, false);
            ASSERT_EQ(static_cast<int>(a.status), static_cast<int>(QpStatus::kOptimal));
            EXPECT_EQ((a.x - b.x).lpNorm<Eigen::Infinity>(), 0.0);
            EXPECT_EQ((a.lambda_e - b.lambda_e).lpNorm<Eigen::Infinity>(), 0.0);
            EXPECT_EQ(a.walk.minor_iters, b.walk.minor_iters);
            EXPECT_EQ(a.walk.factorizations, b.walk.factorizations);
            EXPECT_EQ(a.walk.convex_certificates, 0);
        }
    }
}

// Warm seeds: the lifted seed carries the matching auxiliaries, the lifted -> un-lifted ->
// re-lifted round trip is the identity on everything the engine reads, and a warm solve from a
// seed away from the origin agrees with the dense oracle seeded the same way (the window is
// centred on the clamped seed, so the auxiliary window is centred on the matching value).
TEST(QpLiftOracle, WarmSeedsRoundTripAndMatchTheOracle) {
    const QpOptions defaults;
    for (unsigned seed = 1; seed <= 5; ++seed) {
        std::mt19937 g(5000u + seed);
        const int n = 10;
        const QpProblem qp = random_qp(g, n, 2, 4, seed % 2 == 0);
        const Quasi q = random_quasi_newton(g, n);
        const MixedLift L(qp, n, q.d);

        // (1) round trip of a solved lifted point
        const Out cold = solve_mode(Mode::kWalkBorder, L.problem(), kInf, true);
        ASSERT_EQ(static_cast<int>(cold.status), static_cast<int>(QpStatus::kOptimal));
        QpSolution lifted_sol;
        lifted_sol.status = cold.status;
        lifted_sol.x = cold.x;
        lifted_sol.z = cold.z;
        lifted_sol.lambda_e = cold.lambda_e;
        lifted_sol.bound_state = cold.bound_state;
        lifted_sol.tr_active = cold.tr_active;
        lifted_sol.ineq_active = cold.ineq_active;
        const QpSolution un = L.unlift(lifted_sol);
        ASSERT_EQ(un.x.size(), n);
        EXPECT_EQ(un.lambda_e.size(), qp.me());
        EXPECT_EQ(static_cast<Index>(un.bound_state.size()), n);
        const QpSolution re = L.lift_seed(un);
        ASSERT_EQ(re.x.size(), L.n_lifted());
        EXPECT_LE((re.x - cold.x).lpNorm<Eigen::Infinity>(), 1e-7)
            << "auxiliaries consistent with p_seed";
        EXPECT_EQ(re.bound_state.size(), cold.bound_state.size());
        for (Index i = L.n_orig(); i < L.n_lifted(); ++i) {
            EXPECT_EQ(re.bound_state[static_cast<size_t>(i)], BoundState::kFree);
            EXPECT_FALSE(re.tr_active[static_cast<size_t>(i)]);
        }
        for (Index i = 0; i < L.n_orig(); ++i) {
            EXPECT_EQ(re.bound_state[static_cast<size_t>(i)],
                      un.bound_state[static_cast<size_t>(i)]);
        }
        EXPECT_EQ(re.lambda_e.size(), L.problem().me());

        // (2) re-solving from its own seed finds the same point
        const Out again = solve_mode(Mode::kWalkBorder, L.problem(), kInf, true, nullptr, &re);
        ASSERT_EQ(static_cast<int>(again.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_LE((again.x - cold.x).lpNorm<Eigen::Infinity>(), 1e-6);

        // (3) a seed away from the origin, tight radius: lifted-warm == dense-warm
        QpSolution away;
        away.x = clamp0(qp);
        for (Index i = 0; i < n; ++i) {
            away.x(i) = std::min(std::max(away.x(i) + 0.3 * std::sin(1.0 + i + seed), qp.lower(i)),
                                 qp.upper(i));
        }
        for (const double radius : {kInf, 1.0, 0.3}) {
            const Pair pr = run_pair(Mode::kWalkBorder, qp, q, n, radius, &away);
            compare("warm seed=" + std::to_string(seed) + " r=" + std::to_string(radius), qp, q, pr,
                    defaults, Mode::kWalkBorder);
            EXPECT_TRUE(pr.lifted_raw.clean_inertia);
            // the window is centred on the seed: aux strictly inside Delta/2 of the seed's aux
            const Vec seed_lifted = L.lift_primal(away.x);
            expect_aux_free("warm", L, pr.lifted_raw, radius, seed_lifted);
        }
    }
}

// An elastic-augmented input: slack columns (zero rows in Up/V, zero curvature) ride behind
// the structural block, and the lift is applied AFTER the augmentation.
TEST(QpLiftOracle, ElasticAugmentedInputMatchesTheOracle) {
    const QpOptions defaults;
    int tested = 0;
    for (unsigned seed = 1; seed <= 6; ++seed) {
        std::mt19937 g(9000u + seed);
        const int n = 10;
        QpProblem qp = random_qp(g, n, 2, 4, false);
        // violate rows at p_ref = 0 so the elastic builder adds slacks
        qp.be(0) += 0.7;
        qp.bi(1) = -0.4;
        qp.bi(3) = -0.2;
        const ElasticQp e = build_elastic_subproblem(qp, kInf, 1e2, 1e-9);
        ASSERT_GE(e.ns, 3);
        const Quasi q = random_quasi_newton(g, n);
        const MixedLift L(e.qp, n, q.d);
        ASSERT_EQ(L.n_orig(), n + e.ns);
        for (const Mode m : kModes) {
            const std::string tag =
                std::string("elastic ") + name_of(m) + " seed=" + std::to_string(seed);
            const Pair pr = run_pair(m, e.qp, q, n, kInf);
            compare(tag, e.qp, q, pr, defaults, m);
            EXPECT_TRUE(pr.lifted_raw.clean_inertia) << tag;
            ++tested;
        }
    }
    EXPECT_EQ(tested, 6 * 4);
}

// ------------------------------------------------------------------------------------------
// The adapter's own contract
// ------------------------------------------------------------------------------------------

TEST(QpLiftOracle, EliminatingTheAuxiliariesGivesExactlyTheDenseObjective) {
    std::mt19937 g(31);
    const int n = 9;
    for (const bool exclude_zero : {false, true}) {
        const QpProblem qp = random_qp(g, n, 2, 3, exclude_zero);
        const Quasi q = random_quasi_newton(g, n);
        const MixedLift L(qp, n, q.d);
        const Vec p_ref = clamp0(qp);
        const Vec c = q.d.u_plus.transpose() * p_ref;
        const Vec a = q.d.v.transpose() * p_ref;
        const double dropped =
            0.5 * (q.d.theta_plus.array() * c.array().square()).sum() + 0.5 * a.squaredNorm();
        const QpProblem &lq = L.problem();
        for (int trial = 0; trial < 5; ++trial) {
            const Vec p = p_ref + random_vec(g, n, 0.4);
            const Vec x = L.lift_primal(p); // clamps into the box
            const Vec pc = x.head(n);
            const double f_lift =
                lq.g.dot(x) + 0.5 * x.dot(lq.H.selfadjointView<Eigen::Upper>() * x);
            const double f_dense = qp.g.dot(pc) + 0.5 * pc.dot(q.B * pc);
            EXPECT_NEAR(f_lift + dropped, f_dense, 1e-11 * std::max(1.0, std::abs(f_dense)));
            // the lift rows hold at the consistent point
            const Vec res = lq.Ae * x - lq.be;
            EXPECT_LE(res.tail(L.num_plus()).lpNorm<Eigen::Infinity>(), 1e-12);
        }
    }
}

TEST(QpLiftOracle, TheLiftedHessianIsPositiveDefiniteWhenBIs) {
    std::mt19937 g(32);
    const int n = 11;
    for (int rep = 0; rep < 6; ++rep) {
        const QpProblem qp = random_qp(g, n, 1, 3, false);
        const Quasi q = random_quasi_newton(g, n);
        const MixedLift L(qp, n, q.d);
        const MatrixXd upper = MatrixXd(L.problem().H);
        const MatrixXd H = upper.selfadjointView<Eigen::Upper>();
        const double lmin = Eigen::SelfAdjointEigenSolver<MatrixXd>(H).eigenvalues()(0);
        EXPECT_GT(lmin, 0.0) << "rep " << rep;
        // upper triangle only
        for (Index i = 0; i < L.problem().H.outerSize(); ++i) {
            for (SpMatRM::InnerIterator it(L.problem().H, i); it; ++it) {
                EXPECT_LE(it.row(), it.col());
            }
        }
        EXPECT_NO_THROW(L.problem().validate());
    }
}

TEST(QpLiftOracle, ThePatternIsStableForAFixedRankPair) {
    const int n = 8;
    std::mt19937 g(33);
    const QpProblem qp = random_qp(g, n, 2, 3, false);
    // two MixedLiftData with the same (r+, r-) and different values; one with exact zeros in U
    const auto make = [&](unsigned s, bool sparse_columns) {
        std::mt19937 gg(s);
        MixedLiftData d;
        d.sigma = 1.0 + 0.1 * s;
        const MatrixXd Q =
            Eigen::HouseholderQR<MatrixXd>(MatrixXd(random_vec(gg, n * 4).reshaped(n, 4)))
                .householderQ() *
            MatrixXd::Identity(n, 4);
        d.u_plus = Q.leftCols(2);
        d.v = 0.2 * Q.rightCols(2);
        if (sparse_columns) {
            d.u_plus.col(0).tail(3).setZero();
            d.v.col(1).head(2).setZero();
        }
        d.theta_plus = VectorXd::Constant(2, 0.5 + s);
        d.kappa_plus =
            2.0 * d.u_plus.colwise().lpNorm<1>().transpose() + VectorXd::Constant(2, 0.1);
        d.kappa_v = 2.0 * d.v.colwise().lpNorm<1>().transpose() + VectorXd::Constant(2, 0.1);
        return d;
    };
    const MixedLift A(qp, n, make(1, false));
    const MixedLift B(qp, n, make(2, true));
    const auto same_pattern = [](const auto &X, const auto &Y) {
        if (X.nonZeros() != Y.nonZeros() || X.outerSize() != Y.outerSize()) {
            return false;
        }
        for (Index k = 0; k <= X.outerSize(); ++k) {
            if (X.outerIndexPtr()[k] != Y.outerIndexPtr()[k]) {
                return false;
            }
        }
        for (Index k = 0; k < X.nonZeros(); ++k) {
            if (X.innerIndexPtr()[k] != Y.innerIndexPtr()[k]) {
                return false;
            }
        }
        return true;
    };
    EXPECT_TRUE(same_pattern(A.problem().H, B.problem().H));
    EXPECT_TRUE(same_pattern(A.problem().Ae, B.problem().Ae));
    // dense over the structural block: nnz(Ae lift rows) = r+ (n + 1)
    EXPECT_EQ(A.problem().Ae.nonZeros(), qp.Ae.nonZeros() + 2 * (n + 1));
}

TEST(QpLiftOracle, ValidationRejectsBadInputsAndSizes) {
    std::mt19937 g(34);
    const int n = 6;
    const QpProblem qp = random_qp(g, n, 1, 2, false);
    const Quasi q = random_quasi_newton(g, n);
    EXPECT_NO_THROW(MixedLift(qp, n, q.d));
    {
        MixedLiftData d = q.d;
        d.kappa_plus(0) *= 0.4; // below 2||col||_1: the window could bind
        EXPECT_THROW(MixedLift(qp, n, d), std::invalid_argument);
    }
    {
        MixedLiftData d = q.d;
        d.kappa_v(0) *= 0.4;
        EXPECT_THROW(MixedLift(qp, n, d), std::invalid_argument);
    }
    {
        MixedLiftData d = q.d;
        d.theta_plus(0) = -1.0;
        EXPECT_THROW(MixedLift(qp, n, d), std::invalid_argument);
    }
    {
        MixedLiftData d = q.d;
        d.sigma = 0.0;
        EXPECT_THROW(MixedLift(qp, n, d), std::invalid_argument);
    }
    EXPECT_THROW(MixedLift(qp, n + 1, q.d), std::invalid_argument);
    EXPECT_THROW(MixedLift(qp, n - 1, q.d), std::invalid_argument); // rows != n_struct
    const MixedLift L(qp, n, q.d);
    EXPECT_THROW(L.lift_primal(Vec::Zero(n + 1)), std::invalid_argument);
    EXPECT_THROW(L.unlift_primal(Vec::Zero(n)), std::invalid_argument);
    EXPECT_THROW(L.unlift_equality_multipliers(Vec::Zero(1)), std::invalid_argument);
    QpSolution bad_seed;
    bad_seed.x = Vec::Zero(n + 2);
    EXPECT_THROW(L.lift_seed(bad_seed), std::invalid_argument);
    QpSolution empty_seed; // nothing sized: stays empty
    const QpSolution lifted_empty = L.lift_seed(empty_seed);
    EXPECT_EQ(lifted_empty.x.size(), 0);
    EXPECT_EQ(lifted_empty.bound_state.size(), 0u);
    QpSolution bad_solution;
    EXPECT_THROW(L.unlift(bad_solution), std::invalid_argument);
}

TEST(QpLiftOracle, AnEmptyLiftIsTheScalarHessian) {
    // r+ = r- = 0: B = sigma I, no auxiliaries, the lifted QP is the input with H = sigma I.
    std::mt19937 g(35);
    const int n = 7;
    const QpProblem qp = random_qp(g, n, 1, 2, false);
    MixedLiftData d;
    d.sigma = 2.5;
    d.u_plus = MatrixXd(n, 0);
    d.v = MatrixXd(n, 0);
    const MixedLift L(qp, n, d);
    EXPECT_EQ(L.n_lifted(), n);
    EXPECT_EQ(L.problem().me(), qp.me());
    const MatrixXd B = 2.5 * MatrixXd::Identity(n, n);
    const Out a = solve_mode(Mode::kWalkBorder, L.problem(), 0.5, true);
    const Out b = solve_mode(Mode::kWalkBorder, with_dense_h(qp, B), 0.5, false);
    ASSERT_EQ(static_cast<int>(a.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_LE((a.x - b.x).lpNorm<Eigen::Infinity>(), 1e-9);
}

} // namespace hven::solvers
