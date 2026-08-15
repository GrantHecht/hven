// tests/test_scale_problems.cpp — Phase-5 Task 1: the F7 collocation-chain
// scale generator of tests/sqp/support/scale_problems.h, held to the two gates
// tests/test_parametric_families.cpp holds F1-F6 to, plus the two Phase-5
// additions that are the point of this family.
//
//   1. THE TRANSCRIPTION GATE (small N). Every analytic derivative is
//      central-differenced against the model's own f/cE/cI
//      (support/derivative_check.h), at several x, several p and three
//      (ns, nc) shapes -- including shapes where ns is not a multiple of nc,
//      which is where the mod-nc control coupling C can go wrong.
//   2. THE MANUFACTURED-SOLUTION GATE. First ALGEBRAICALLY: at N = 10 and
//      N = 100 the quadruple (x*, lambda_e*, lambda_i*, z*) is checked against
//      every KKT condition using the MODEL's own Jacobians -- feasibility to
//      the last bit, stationarity, sign conditions, complementarity -- and
//      eval_f(x_star(p)) is checked against the independently derived closed
//      form f_star(p) ((F7-F) in the header, which never touches the source
//      terms g that eval_f is built from). Then by COLD SOLVE at the same two
//      sizes: f within 1e-8 relative, x to 1e-7, and the per-node active set
//      EXACTLY.
//   3. THE SCALE GATES, at n = 10^4 (N = 10^3) and n = 10^5 (N = 10^4), with
//      the runtime budgets stated and the measured values recorded below.
//   4. THE PARAMETRIC CONTRACT: patterns independent of x, of p, and -- the
//      Phase-5 Task-0 clause -- of obj_scale and of the multiplier VALUES.
//
// WHY THE BIG GATES DO NOT USE assert_hessian. derivative_check.h's Hessian
// checker densifies eval_hess into an n x n Eigen::MatrixXd, which is 8e8
// bytes at n = 10^4 and 8e10 at n = 10^5 -- not a slow test, an impossible
// one. The substitute is the Phase-4 Task-8 precedent
// (ParametricF3.ScaleReadinessAtTenThousand in
// tests/test_parametric_families.cpp): the exact sparsity STRUCTURE, entry by
// entry against the closed-form counts, plus a DIRECTIONAL H*v check against a
// central difference of the Lagrangian gradient along several v. The same
// substitution is made for the gradient and the Jacobians at n = 10^5, where
// the coordinate-wise checkers are quadratic in n.
//
// ACTIVITY IS MEASURED GEOMETRICALLY -- a constraint is active at the returned
// point iff it holds with equality there, to a tolerance -- for the reason
// tests/test_parametric_families.cpp states. F7 is strictly complementary at
// every sampled (N, p) (junction_margin() below is asserted to stay well above
// zero), so the geometric and working-set notions coincide here.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Dense>
#include <Eigen/SparseCore>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>

#include "support/derivative_check.h"
#include "support/nlp_kkt_check.h"
#include "support/scale_problems.h"

namespace hven::solvers {
namespace {

using test_support::AnalyticActiveSet;
using test_support::assert_gradient;
using test_support::assert_hessian;
using test_support::assert_jacobians;
using test_support::F7CollocationChain;
using test_support::peak_rss_mib;

constexpr double kDerivTol = 1e-6;    // derivative_check.h's mixed abs/rel tol
constexpr double kFRelTol = 1e-8;     // the brief's objective tolerance
constexpr double kXTol = 1e-7;        // primal tolerance against x_star
constexpr double kMultTol = 1e-6;     // multiplier tolerance against the path
constexpr double kActivityTol = 1e-8; // geometric activity tolerance

// The four p the path gate samples, all inside the design range (0, R = 1) and
// all with junction_margin() >= 0.19 h at BOTH N = 10 and N = 100 (pinned in
// JunctionGeometryIsAnalytic), so no assertion below is decided by a node
// sitting on a junction. 0.45 is BELOW p_activation() = 0.5: no path row is
// active there, which is the empty-active-set branch.
const std::vector<double> &path_samples() {
    static const std::vector<double> s{0.45, 0.55, 0.68, 0.85};
    return s;
}

// A solve tight enough that the 1e-8 relative objective claim is a statement
// about the FAMILY rather than about the default stopping tolerance.
//
// THE QP MINOR-ITERATION CAP IS RAISED, AND THAT IS A MEASUREMENT, NOT A
// WORKAROUND. QpOptions::max_iter defaults to 500. At N = 100 (mi = 100 path
// rows plus 2*N*nc = 400 finite bounds) the FIRST real major -- the one that
// walks from the cold start onto the active window -- needs 761 minor
// iterations at p = 0.68 and 954 at p = 0.85, because an active-set method
// admits roughly one constraint per minor and the window holds 68 and 88 rows
// respectively, on top of the bound activity it passes through on the way. At
// the default cap those QPs come back unsolved, the step is rejected, and the
// solve exits kNumericalError with an iterate that is nowhere near x* -- which
// is what this file measured before the cap was raised. Every SUBSEQUENT major
// costs 2 minors, so the cost is entirely in that one step. Raising the cap is
// how a fixture asks the engine the question it was built to ask; whether the
// DEFAULT should move is a Phase-5 policy question for the scale study
// (Task 3), not something a test-support file decides. ANSWERED, in part, by
// that study: docs/notes/2026-07-30-scale-study-cold.md section 5 measures the
// default failing from n ~ 250 upward and recommends a driver-level SIZE-
// SCALING rule rather than a new constant -- but recommends only, since a
// shipped-default change needs its own task and human agreement. The default
// is therefore still 500 and this fixture still raises it.
//
// EVERY COUNTER IN THIS COMMENT AND IN THE TWO CARRIES BELOW WAS MEASURED ON
// clang, MKL PARDISO (Linux), this development machine -- the phase's standing
// rule for counter statements. Nothing here is asserted; the numbers exist so
// Task 3 can budget from them rather than rediscover them.
//
// CARRY 1 FOR TASK 3 -- MINORS TRACK ADMITTED ROWS ROUGHLY 1:1, so the cap has
// to be budgeted from the problem size, not guessed. First-major minors at
// p = 0.85: N = 30 -> 289, N = 50 -> 629, N = 70 -> 1069, N = 100 -> 954
// (review round 1). Those counts are at the ns = 3, nc = 2 shape, where
// n = N*(ns + nc) = 5N, so Task 3's n = 10^4 pin is N = 2000 with ~1790 active
// path rows plus bound activity -- an earlier version of this sentence said
// N = 10^3, which is n = 5000 at that shape, not 10^4. Expect on the order of
// 10^4 minors there -- 20x the engine default and 2x this fixture's own 5000.
//
// DISCHARGED by docs/notes/2026-07-30-scale-study-cold.md section 4.2, which
// measured it across N = 30..1500: on every size whose QP subproblems converge
// on their own, the whole-solve minors/n ratio sits in [1.7, 5.0], so the
// estimate above was the right shape. BUDGET FROM THAT RANGE ONLY WITH THAT
// CAVEAT ATTACHED: N = 800 is the one measured exception, where the first
// subproblem does not converge and instead consumes whatever cap it is given
// (510165 minors at n = 4000, a ratio of 128, on a cap of 500000), so a cap
// chosen from the ratio alone can be exceeded arbitrarily. That section is also
// where the cap recommendation (C ~ 3 times n + mi + #bounded) comes from.
//
// CARRY 2 FOR TASK 3 -- A WALL-TIME CLIFF JUST ABOVE THIS FIXTURE'S LARGEST
// SOLVE. Same probe, Release, p = 0.85, these options verbatim:
// N = 100 -> 2.62 s; N = 200 -> DID NOT FINISH in 300 s at qp.max_iter = 5000,
// nor in 13 min at 50000 (both killed). That is a >100x wall-time jump for a
// 2x size step, against ~4.4x per doubling from N = 50 to N = 100.
//
// *** DIAGNOSED AND CLOSED BY PHASE-5 TASK 3. ***
// docs/notes/2026-07-30-scale-study-cold.md sections 2.1-2.4 is the record; the
// verdict, so a reader of THIS file does not have to open that one to learn
// whether the question is still live:
//
//   The cause is a QpOptions::schur_cap-TRIGGERED K0-REBUILD STORM -- a pure
//   linear-algebra cost explosion. Once the live Schur-border count exceeds the
//   default schur_cap = 128, nearly every minor iteration pays a K0 rebuild
//   plus a full re-border, and because a rebuild folds the working rows back
//   into K0 its sparsity pattern changes and Pardiso must re-analyze.
//
//   WHAT CROSSES THE CAP IS THE PIN COUNT (the Theta(N) pinned CONTROLS), not
//   the active path rows -- Phase-5 Task 4 measured the stack's composition at
//   every breach and found it 100% pins, and FIXED the resulting latch thrash
//   in qp_engine.h. See docs/notes/2026-07-31-schur-cap-policy.md sections 1-2;
//   with that fix N = 150 costs 4.8 s instead of 87 s (the single-threaded
//   figure -- §2.3's cross-check, not §2.2's 97 s 16-thread one) and N = 200
//   costs 31.6 s instead of not finishing in 13 minutes, on the SAME minor
//   counts.
//
//   Of the three hypotheses this comment listed as "still live", TWO ARE
//   REFUTED. The discriminating experiment is a one-knob A/B on schur_cap at
//   N = 150: 128 vs 512 gives the IDENTICAL minor count (1276 whole-solve,
//   1270 in the one big subproblem) against 853 vs 4 factorizations, 41005 vs
//   847 Schur updates and 274 vs 3 symbolic analyses. Identical iterations
//   rules out minor-iteration blow-up; kOptimal in 4 majors at |x - x*|inf ~
//   8e-10, under a cap a cycle would have exhausted, rules out cycling.
//
//   EVERY PAIR IN THAT SENTENCE IS NOW A WHOLE-SOLVE COUNT, which is what
//   SqpCounters reports and therefore what a reader of this file can check
//   directly. The symbolic-analysis pair was "272 vs 1" here through Task 3;
//   those are the ONE BIG SUBPROBLEM's counts, from that task's --qp-csv
//   per-subproblem table, and the other three subproblems contribute the
//   remaining 2 in each column. Corrected to one consistent level by Phase-5
//   Task 4, which re-measured the whole-solve pair on the base engine
//   (docs/notes/2026-07-31-schur-cap-policy.md section 2); the factorization
//   and Schur-update pairs were already whole-solve and are unchanged.
//
//   NO SOLVER-CORRUPTING DEFECT WAS FOUND, so the stop-and-report discipline
//   this comment invoked did not fire. N = 200 is not a wall at all once the
//   linear algebra is right: at ws_algebra = kRefactorize it solves in 9.7 s,
//   with the same 4690 minors.
//
// One item from that study is still OPEN and is recorded in the note's section
// 6: at N = 800 a single QP subproblem consumes whatever cap it is given
// without converging (the solve still returns kOptimal, in 279 s at a cap of
// 20000 and 4944 s at 500000). Unreachable at the shipped default cap.
// MARKED NOTE, PHASE-6 TASK 4 (M6): `opts.qp.max_iter = 5000` below is now an
// EXPLICIT cap in the precedence sense (types.h's QpOptions::max_iter) rather
// than merely a raised one -- it wins outright over the size-derived default,
// so every figure this file publishes is measured at exactly the cap it always
// was and none of them moved under M6.
SqpOptions tight_options() {
    SqpOptions opts;
    opts.kkt_tol = 1e-9;
    opts.feas_tol = 1e-9;
    opts.max_iter = 60;
    opts.qp.max_iter = 5000;
    return opts;
}

// Which constraints hold with equality at `x`, in AnalyticActiveSet's (and
// therefore WarmStart's) encoding. Mirrors the helper of the same name in
// tests/test_parametric_families.cpp, which is file-local there.
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
    const Vec ci = model.eval_ci(x);
    for (Index j = 0; j < model.mi(); ++j) {
        a.ineq_active[static_cast<std::size_t>(j)] =
            static_cast<std::uint8_t>(std::abs(ci(j)) <= tol ? 1 : 0);
    }
    return a;
}

// The (row, col) list of a sparse matrix's stored entries -- its PATTERN,
// independent of the values (test_parametric_families.cpp's pattern_of).
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

struct ModelPattern {
    std::vector<std::pair<int, int>> hess, jac_e, jac_i;
    bool operator==(const ModelPattern &o) const {
        return hess == o.hess && jac_e == o.jac_e && jac_i == o.jac_i;
    }
};

// The Phase-5 Task-0 clause: the probe varies obj_scale and the multiplier
// VALUES as well as x, because two callers (the warm-start ingest probe and
// the restoration model) build at zero multipliers / zero obj_scale.
ModelPattern patterns_at(const NlpModel &model, const Vec &x, double obj_scale, double mult) {
    ModelPattern p;
    p.hess = pattern_of(model.eval_hess(x, obj_scale, Vec::Constant(model.me(), mult),
                                        Vec::Constant(model.mi(), mult)));
    p.jac_e = pattern_of(model.eval_jac_e(x));
    p.jac_i = pattern_of(model.eval_jac_i(x));
    return p;
}

// The symmetric product H*v for an upper-triangle-only H (F3's gate).
Vec symmetric_product(const SpMatRM &H, const Vec &v) {
    return H * v + H.transpose() * v - H.diagonal().cwiseProduct(v);
}

// A reproducible O(1) direction of length n.
Vec probe_direction(Index n, int seed) {
    Vec v(n);
    for (Index i = 0; i < n; ++i) {
        v(i) = std::sin(0.11 * static_cast<double>(i) + static_cast<double>(seed));
    }
    return v;
}

// peak_rss_mib() -- PHASE-5 TASK 2 moved this from a local helper here into
// tests/sqp/support/scale_problems.h so bench/bench_scale.cpp can share the same
// /proc/self/status parser rather than duplicating it; see that header's own
// note for the -1 sentinel contract (unchanged) and this file's THE PEAK-RSS
// PRINT IS INFORMATIONAL note below for how the one call site here still
// reads it.

// =====================================================================
// 1. THE TRANSCRIPTION GATE.
// =====================================================================

TEST(ScaleF7, DerivativesMatchFiniteDifferences) {
    // Three shapes: ns a multiple of nc, ns NOT a multiple of nc (where the
    // mod-nc coupling C has unequal residue classes, and m_max = ceil(ns/nc)
    // is the thing gamma is derived from), and the scalar corner ns = nc = 1.
    struct Shape {
        Index nodes, ns, nc;
    };
    for (const Shape sh : {Shape{10, 3, 2}, Shape{6, 4, 3}, Shape{5, 1, 1}}) {
        for (double p : {0.55, 0.68, 0.85}) {
            SCOPED_TRACE(::testing::Message()
                         << "N=" << sh.nodes << " ns=" << sh.ns << " nc=" << sh.nc << " p=" << p);
            F7CollocationChain model(sh.nodes, sh.ns, sh.nc, p);
            const Index n = model.n();
            // Three points: the cold start, the manufactured optimum, and an
            // off-path point that is on neither.
            const std::vector<Vec> points{model.start_point(), model.x_star(p),
                                          probe_direction(n, 1) * 0.4};
            for (const Vec &x : points) {
                SCOPED_TRACE(::testing::Message() << "||x|| = " << x.norm());
                // Deliberately NONZERO multipliers, including on rows whose
                // analytic multiplier is zero: eval_hess must be checked for
                // the constraint term it is contracted to add (here the path
                // rows' identity block), and at lambda_i = 0 that term is
                // untested. F1's reasoning, verbatim.
                const Vec lambda_e = probe_direction(model.me(), 2) * 0.25;
                const Vec lambda_i = Vec::Constant(model.mi(), 0.3);
                EXPECT_TRUE(assert_gradient(model, x, kDerivTol));
                EXPECT_TRUE(assert_jacobians(model, x, kDerivTol));
                EXPECT_TRUE(assert_hessian(model, x, lambda_e, lambda_i, kDerivTol));
            }
        }
    }
}

// =====================================================================
// 2a. THE MANUFACTURED SOLUTION, ALGEBRAICALLY.
// =====================================================================

// Every KKT condition of the header's (V2)-(V5), recomputed from the MODEL at
// the analytic quadruple. Nothing here involves the driver: if this fails, the
// family is wrong, and no solve result would mean anything.
void check_manufactured_kkt(const F7CollocationChain &model, double p) {
    SCOPED_TRACE(::testing::Message() << "N = " << model.node_count() << ", p = " << p);
    const Vec x = model.x_star(p);
    const Vec le = model.lambda_e_star(p);
    const Vec li = model.lambda_i_star(p);
    const Vec z = model.z_star(p);
    ASSERT_EQ(x.size(), model.n());
    ASSERT_EQ(le.size(), model.me());
    ASSERT_EQ(li.size(), model.mi());

    // (V2) primal feasibility. The equality rows are EXACT -- the forcing
    // (F7-D) is defined as the residual they would otherwise leave -- so the
    // bound here is roundoff on O(1) data, not a discretization error.
    EXPECT_LE(model.eval_ce(x).lpNorm<Eigen::Infinity>(), 1e-14);
    const Vec ci = model.eval_ci(x);
    EXPECT_LE(ci.maxCoeff(), 1e-14);
    for (Index k = 0; k < model.mi(); ++k) {
        if (model.row_active(k, p)) {
            // Clamped: ||y*_k|| == R exactly, so the row holds with equality.
            EXPECT_NEAR(ci(k), 0.0, 1e-14) << "node " << k << " should be ON the path bound";
            EXPECT_GT(li(k), 0.0) << "node " << k << " should carry a strictly positive price";
        } else {
            EXPECT_LT(ci(k), -1e-12) << "node " << k << " should be strictly inside";
            EXPECT_DOUBLE_EQ(li(k), 0.0) << "node " << k;
        }
    }
    // The box is inactive at the optimum by construction (CHOICE 2).
    for (Index i = 0; i < model.n(); ++i) {
        if (std::isfinite(model.lower()(i))) {
            EXPECT_GT(x(i) - model.lower()(i), 0.4) << "control " << i << " near its lower bound";
            EXPECT_GT(model.upper()(i) - x(i), 0.4) << "control " << i << " near its upper bound";
        }
    }

    // (V5) stationarity, from the model's own Jacobians.
    const Vec stat = model.eval_grad(x) + Eigen::VectorXd(model.eval_jac_e(x).transpose() * le) +
                     Eigen::VectorXd(model.eval_jac_i(x).transpose() * li) - z;
    EXPECT_LE(stat.lpNorm<Eigen::Infinity>(), 1e-14) << "stationarity residual";

    // (V3)/(V4) dual feasibility and complementarity.
    EXPECT_GE(li.minCoeff(), 0.0);
    EXPECT_LE(li.cwiseProduct(ci).lpNorm<Eigen::Infinity>(), 1e-15);
    EXPECT_EQ(z.lpNorm<Eigen::Infinity>(), 0.0);

    // (F7-F) against eval_f -- two independent expressions for the same
    // number; see this file's banner.
    const double f_at_star = model.eval_f(x);
    EXPECT_LE(std::abs(f_at_star - model.f_star(p)), 1e-12 * std::max(1.0, std::abs(f_at_star)))
        << fmt::format("eval_f = {:.17g} vs f_star = {:.17g}", f_at_star, model.f_star(p));
}

TEST(ScaleF7, ManufacturedSolutionSatisfiesKktExactly) {
    for (Index nodes : {10, 100}) {
        for (double p : path_samples()) {
            F7CollocationChain model(nodes, 3, 2, p);
            check_manufactured_kkt(model, p);
        }
    }
    // And at a second shape, so nothing above depends on ns = 3, nc = 2.
    F7CollocationChain wide(40, 5, 3, 0.7);
    check_manufactured_kkt(wide, 0.7);
}

// =====================================================================
// 2b. THE MANUFACTURED SOLUTION, AGAINST A COLD SOLVE.
// =====================================================================

void check_path_at(F7CollocationChain &model, double p) {
    SCOPED_TRACE(::testing::Message() << "N = " << model.node_count() << ", p = " << p);
    model.set_parameters(Vec::Constant(1, p));
    const SqpOptions opts = tight_options();
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    const double f_star = model.f_star(p);
    EXPECT_LE(std::abs(sol.f - f_star), kFRelTol * std::max(1.0, std::abs(f_star)))
        << fmt::format("f = {:.17g} vs f_star = {:.17g}", sol.f, f_star);
    const Vec x_star = model.x_star(p);
    EXPECT_LE((sol.x - x_star).lpNorm<Eigen::Infinity>(), kXTol);
    EXPECT_LE((sol.lambda_i - model.lambda_i_star(p)).lpNorm<Eigen::Infinity>(), kMultTol);
    EXPECT_LE((sol.z - model.z_star(p)).lpNorm<Eigen::Infinity>(), kMultTol);

    // The ACTIVE SET, exactly, per node -- both the path rows and the (empty)
    // set of active bounds.
    const AnalyticActiveSet analytic = model.active_set(p);
    const AnalyticActiveSet observed = geometric_active_set(model, sol.x, kActivityTol);
    EXPECT_EQ(observed.ineq_active, analytic.ineq_active);
    EXPECT_EQ(observed.bound_active, analytic.bound_active);

    // The returned quadruple is a KKT point OF THE MODEL, recomputed from the
    // model rather than from the driver's own residual.
    const test_support::NlpKktResidual chk =
        test_support::self_check_kkt(model, sol, opts.feas_tol);
    EXPECT_LT(chk.stationarity, 1e-7);
    EXPECT_LT(chk.primal, 1e-8);
    EXPECT_LT(chk.dual_sign, 1e-9);
    EXPECT_LT(chk.complementarity, 1e-8);
}

TEST(ScaleF7, AnalyticPathMatchesColdSolveAtTenNodes) {
    F7CollocationChain model(10, 3, 2);
    for (double p : path_samples()) {
        check_path_at(model, p);
    }
}

// THE COST OF A COLD SOLVE IS ENTIRELY IN THE ACTIVE WINDOW, AND IT IS STEEP.
// Measured (review round 1's standalone probe, clang, MKL Pardiso, this
// development machine, with tight_options() verbatim), per parameter value:
//
//     N = 100, p = 0.85 (88 of 100 rows active)  84.8 s Debug / 2.62 s Release
//     N = 100, p = 0.45 (window empty)            0.02 s Debug / 0.01 s Release
//     N =  70, p = 0.85                          54.1 s Debug
//     N =  50, p = 0.85                          17.1 s Debug / 0.59 s Release
//     N =  30, p = 0.85                           1.9 s Debug
//
// As the gtests actually run -- which adds set_parameters, the KKT self-check
// and the active-set comparison to the probe's solve -- the two that survive
// below measure ~18 s Debug / 0.6 s Release (N = 50) and 0.02 s / 0.01 s
// (N = 100, empty window), and ScaleF7Slow's N = 100 wide-window solve measures
// ~80 s Debug (77-81 s observed across 4 runs, clang, this machine) / 2.6 s
// Release. EVERY WALL-TIME FIGURE IN THIS FILE IS A RANGE OR A LEADING-DIGIT
// APPROXIMATION ON PURPOSE: they are single- or few-sample measurements on one
// machine, and quoting them to the tenth of a second would claim a
// repeatability they do not have. What the decisions below rest on is the
// DELTA between configurations, which is an order of magnitude larger than the
// run-to-run spread.
//
// -- i.e. the empty-window solve is FREE (1 major, 2 minors: the problem is a
// QP once no path row can activate), and everything else is the one big QP
// tight_options() describes. The earlier figures in this comment ("1.2 s
// Release / 43 s Debug ... 0.5 s / 15 s") were wrong in both columns and are
// replaced by the measurements above.
//
// SO THE SIZES ARE SPLIT THREE WAYS, and every split is a runtime decision
// rather than a coverage one -- all four assertions are computed from
// model.x_star/f_star/active_set at whatever N, and every downsized solve
// reaches the SAME verdict (|x - x*|inf ~ 8e-10, |df|/|f| ~ 3.5e-11 at
// N = 30/50/70/100), so size buys a bigger QP and no new assertion outcome:
//
//   * the ACTIVE-WINDOW verification runs at N = 50 (below), where the junction
//     margin at p = 0.85 is 0.233 h -- still above this file's 0.19 floor, so
//     strict complementarity is as clean there as at N = 100;
//   * the EMPTY-WINDOW verification stays at N = 100 (below), because it is
//     free;
//   * the N = 100 ACTIVE-WINDOW solve is kept in full, in ScaleF7Slow at the
//     end of this file, which tests/CMakeLists.txt excludes from the
//     per-commit ctest run.
TEST(ScaleF7, AnalyticPathMatchesColdSolveAtFiftyNodes) {
    F7CollocationChain model(50, 3, 2);
    ASSERT_GT(model.junction_margin(0.85), 0.19); // the 0.233 h cited above
    check_path_at(model, 0.85);
}

TEST(ScaleF7, AnalyticPathMatchesColdSolveAtHundredNodes) {
    F7CollocationChain model(100, 3, 2);
    check_path_at(model, 0.45); // the empty-window branch, 1 major and 2 minors
}

// =====================================================================
// 2c. THE WIDE-WINDOW WALK -- WHAT THE MINORS ARE SPENT ON.
//
// PHASE-6 TASK 3. docs/notes/2026-08-03-identification-stall-study.md is the
// study; this is its regression net, at the two smallest sizes that exhibit
// the mechanism (N = 30 costs 0.08 s Release, N = 50 costs 0.43 s -- the same
// two sizes AnalyticPathMatchesColdSolveAt{Ten,Fifty}Nodes already run per
// commit, so nothing here changes the suite's cost profile materially).
//
// WHAT IS BEING PINNED, AND WHY IT IS THE MECHANISM RATHER THAN A NUMBER.
// Phase 5 closed with the F7 wide-window minor-stall open and its mechanism
// unknown, and the phase-6 design spec's standing hypothesis for it was
// "degenerate working-set churn". The eleven QpCounters fields Task 3 added
// (types.h, observation-only) separate that compound hypothesis into its
// independent halves, and the corpus answers them differently:
//
//   CHURN: YES, AND IT IS A ROW PHENOMENON. Fix round 1 added the
//                      constraint-CLASS split, because the merged counters
//                      could not tell "each row re-acquired several times"
//                      from "many DISTINCT bounds touched once each" -- the
//                      brief's own bound-flip alternative, which fitted every
//                      merged number equally well. Measured apart: an
//                      inequality row is admitted 3.11x (N = 30) / 3.94x
//                      (N = 50) per DISTINCT row touched, while a variable
//                      bound is admitted 1.00x at both -- pinned once,
//                      released once, never revisited. Assertion (4) below is
//                      that split, and it is the assertion that would catch a
//                      change turning row churn into bound churn or back.
//   TIES: NO.          drop_ties and ratio_ties are EXACTLY ZERO -- no drop
//                      decision was settled inside the rule's relative
//                      1e-12 tie window with more than one candidate, and no
//                      ratio test ever had two constraints at exactly the
//                      minimum ratio. Fix round 1 again: the original test
//                      inferred this from the degeneracy zero, which measures
//                      STEP LENGTH and says nothing about tie multiplicity.
//                      Assertion (5) below.
//
//   DEGENERACY: NO.    degenerate_steps is EXACTLY ZERO. Every blocking add
//                      this walk makes moves the iterate by more than the
//                      loop's own step tolerance. qp_engine.h implements no
//                      anti-cycling rule at all and says so (its section on
//                      degeneracy: "a degenerate stall is bounded by
//                      max_iter"); this pins the measured fact that the
//                      omission costs nothing on this family, which is what
//                      rules an anti-degeneracy remedy OUT of the study's
//                      mitigation menu.
//
// THE CONSERVATION IDENTITY is asserted rather than the raw counts alone,
// because it is what makes the two counts mean what the note says they mean:
//
//     shift_adds + ws_adds - ws_drops == |final working set|
//
// with the right-hand side taken from the FAMILY's analytic active set, not
// from the solve. F7 activates no variable bound at these p (checked by
// check_path_at's own bound_active assertion), so every add and every drop on
// this fixture is an inequality row and the identity is exact. A future change
// that made the engine drop a row it never counted, or count a probe drop it
// then restored, breaks this line and nothing else in the suite would.
//
// THE COST LAW, minors ~= 1.5 * (ws_adds + ws_drops), is asserted as a band
// rather than an equality: it is the study's central quantitative claim (each
// working-set event costs a fixed ~3 minors per drop/add PAIR, so the only
// reducible quantity is the EVENT COUNT, not the per-event price), and it held
// to within 3% across N = 30 ... 1000 -- nine healthy sizes spanning 26 to 886
// active rows, at p = 0.85 and p = 0.90. (Each of those rows was measured in ONE
// algebra mode; Sec. 2 of the note -- minor counts bit-identical between border
// and refactorize on 22 two-sided cells -- is what licenses reading them
// together.) The band is deliberately loose
// enough that ordinary trajectory noise does not trip it and tight enough that a mechanism change
// does.
//
// Observed values below are LLVM/clang++ Release AND Debug on MKL Pardiso,
// this development machine, at tight_options() exactly. They are not
// backend-independent and no Accelerate arm exists for them -- a Mac
// re-verification pass should expect to re-derive the eight pinned counts (the
// identity, the zero, and the cost-law band should all hold regardless, since
// none of them depends on which pivot the walk happened to pick).
// =====================================================================

// The Task-3 walk counters this test reads, summed over every QP subproblem of
// one solve (five from the original round, six from its fix round).
// SqpCounters carries none of them (sqp_types.h), so an attached Ledger is the
// only route -- the same route tests/test_scale_smoke.cpp uses for
// schur_updates.
struct WalkCounts {
    Index minors = 0;
    Index adds = 0;
    Index drops = 0;
    Index shift_adds = 0;
    Index degenerate = 0;
    // FIX ROUND 1: the constraint-CLASS split and the distinct-touch counts,
    // which are what turn "the walk re-discovers its rows" from an inference
    // into a measurement -- see this section's banner.
    Index adds_bound = 0;
    Index drops_bound = 0;
    Index distinct_ineq = 0;
    Index distinct_bound = 0;
    Index drop_ties = 0;
    Index ratio_ties = 0;
};

WalkCounts walk_counts(const Ledger &ledger) {
    WalkCounts w;
    for (const SolveRecord &rec : ledger.records()) {
        w.minors += rec.counters.minor_iters;
        w.adds += rec.counters.ws_adds;
        w.drops += rec.counters.ws_drops;
        w.shift_adds += rec.counters.shift_adds;
        w.degenerate += rec.counters.degenerate_steps;
        w.adds_bound += rec.counters.ws_adds_bound;
        w.drops_bound += rec.counters.ws_drops_bound;
        w.distinct_ineq += rec.counters.distinct_ineq_added;
        w.distinct_bound += rec.counters.distinct_bound_added;
        w.drop_ties += rec.counters.drop_ties;
        w.ratio_ties += rec.counters.ratio_ties;
    }
    return w;
}

void check_wide_window_walk(Index nodes, double p, const WalkCounts &want) {
    SCOPED_TRACE(::testing::Message() << "N = " << nodes << ", p = " << p);
    F7CollocationChain model(nodes, 3, 2);
    model.set_parameters(Vec::Constant(1, p));

    SqpDriver driver(tight_options());
    Ledger ledger;
    driver.attach_ledger(&ledger, "walk");
    const SqpSolution sol = driver.solve(model);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);

    const WalkCounts got = walk_counts(ledger);
    const auto trace = [&] {
        return fmt::format("minors={} adds={} drops={} shift_adds={} degenerate={} adds_bound={} "
                           "drops_bound={} distinct_ineq={} distinct_bound={} drop_ties={} "
                           "ratio_ties={}",
                           got.minors, got.adds, got.drops, got.shift_adds, got.degenerate,
                           got.adds_bound, got.drops_bound, got.distinct_ineq, got.distinct_bound,
                           got.drop_ties, got.ratio_ties);
    };

    // (1) THE MECHANISM. Churn is real; degeneracy is not.
    EXPECT_EQ(got.degenerate, 0) << trace();
    EXPECT_GT(got.drops, 0) << trace();

    // (2) THE CONSERVATION IDENTITY, against the FAMILY's own active set.
    const AnalyticActiveSet analytic = model.active_set(p);
    const auto active_rows =
        static_cast<Index>(std::count(analytic.ineq_active.begin(), analytic.ineq_active.end(), 1));
    EXPECT_EQ(got.shift_adds + got.adds - got.drops, active_rows) << trace();

    // (3) THE COST LAW: ~1.5 minors per working-set event, i.e. ~3 per
    // drop/add pair. Held to within 3% over N = 30 ... 1000; the band is +/-10%.
    const double events = static_cast<double>(got.adds + got.drops);
    ASSERT_GT(events, 0.0) << trace();
    const double minors_per_event = static_cast<double>(got.minors) / events;
    EXPECT_GT(minors_per_event, 1.35) << trace();
    EXPECT_LT(minors_per_event, 1.65) << trace();

    // (4) RE-DISCOVERY IS A ROW PHENOMENON AND NOT A BOUND ONE -- the fix
    // round's central measurement, asserted as a shape rather than as two
    // magic numbers. Row admissions come by two routes (the homotopy and the
    // ratio test) and bound admissions by one; dividing each by the number of
    // DISTINCT constraints of that class ever touched gives the mean number of
    // times one constraint was admitted.
    ASSERT_GT(got.distinct_ineq, 0) << trace();
    ASSERT_GT(got.distinct_bound, 0) << trace();
    const double row_admits = static_cast<double>(got.adds - got.adds_bound + got.shift_adds);
    const double row_repeat = row_admits / static_cast<double>(got.distinct_ineq);
    const double bound_repeat =
        static_cast<double>(got.adds_bound) / static_cast<double>(got.distinct_bound);
    EXPECT_GT(row_repeat, 2.0) << trace();    // rows: re-discovered, ~3.1-3.9x
    EXPECT_LT(bound_repeat, 1.25) << trace(); // bounds: touched once, never repeated

    // (5) TIES ARE NOT THE MECHANISM, measured rather than argued. Neither the
    // drop rule's relative tie window nor the ratio test's exact-minimum
    // multiplicity is ever entered with more than one candidate on this
    // family, which is what rules a tie-break change off the study's
    // mitigation menu.
    EXPECT_EQ(got.drop_ties, 0) << trace();
    EXPECT_EQ(got.ratio_ties, 0) << trace();

    // (6) THE OBSERVED VALUES, which is what turns (1)-(5) from a shape claim
    // into a regression net -- see this section's banner for the backend
    // provenance and for why they are expected to move on Accelerate.
    EXPECT_EQ(got.minors, want.minors) << trace();
    EXPECT_EQ(got.adds, want.adds) << trace();
    EXPECT_EQ(got.drops, want.drops) << trace();
    EXPECT_EQ(got.shift_adds, want.shift_adds) << trace();
    EXPECT_EQ(got.adds_bound, want.adds_bound) << trace();
    EXPECT_EQ(got.drops_bound, want.drops_bound) << trace();
    EXPECT_EQ(got.distinct_ineq, want.distinct_ineq) << trace();
    EXPECT_EQ(got.distinct_bound, want.distinct_bound) << trace();
}

TEST(ScaleF7, WideWindowWalkIsChurnNotDegeneracy) {
    // N = 30: |W*| = 26. 87 row admissions over 28 distinct rows = 3.11x;
    // 36 bound pins over 36 distinct bounds = 1.00x. 295 minors, 1.54/event.
    check_wide_window_walk(30, 0.85, WalkCounts{295, 95, 97, 28, 0, 36, 36, 28, 36, 0, 0});
    // N = 50: |W*| = 44. 185 row admissions over 47 distinct rows = 3.94x;
    // 69 bound pins over 69 distinct bounds = 1.00x. 635 minors, 1.52/event.
    check_wide_window_walk(50, 0.85, WalkCounts{635, 207, 210, 47, 0, 69, 69, 47, 69, 0, 0});
}

// =====================================================================
// The activity geometry (F7-JCT) and its two regime ends.
// =====================================================================

TEST(ScaleF7, JunctionGeometryIsAnalytic) {
    F7CollocationChain model(10, 3, 2);
    const double R = model.radius();
    EXPECT_DOUBLE_EQ(model.p_activation(), 0.5 * R);
    EXPECT_DOUBLE_EQ(model.p_saturation(), R);

    for (double p : {0.55, 0.68, 0.85, 0.95}) {
        SCOPED_TRACE(::testing::Message() << "p = " << p);
        const double left = model.junction_left(p);
        const double right = model.junction_right(p);
        // The defining equation: psi(T(p), p) == R exactly at both junctions.
        EXPECT_NEAR(model.psi(left, p), R, 1e-14);
        EXPECT_NEAR(model.psi(right, p), R, 1e-14);
        EXPECT_LT(left, right);
        EXPECT_GT(left, 0.0); // interior, so node 0 is strictly inactive
        EXPECT_LT(right, 1.0);
        // The window widens with p: T is decreasing.
        EXPECT_LT(left, model.junction_left(p - 0.05));
    }
    // Below p_activation() the window is empty; the design range excludes
    // p >= p_saturation(), where node 0's path row collides with the boundary
    // condition (the header's ONE DEGENERACY).
    for (Index k = 0; k < model.node_count(); ++k) {
        EXPECT_FALSE(model.row_active(k, 0.45)) << "node " << k;
        EXPECT_TRUE(model.row_active(k, 1.05)) << "node " << k;
    }
    EXPECT_TRUE(model.row_active(0, model.p_saturation() + 1e-9));
    EXPECT_FALSE(model.row_active(0, model.p_saturation() - 1e-9));

    // active_set(p) agrees with the sub-interval, node by node, and no node
    // lands near a junction.
    //
    // DO NOT RETARGET THIS LOOP TO {10, 50} TO MATCH THE SOLVE SIZES. It looks
    // like a leftover from when the cold solves ran at N = 100, and it is not:
    // at N = 50, p = 0.55 the junction margin is 0.054 h and the > 0.19
    // assertion below FAILS (review round 1 checked this explicitly). The
    // margin is a property of where T(p) lands between two nodes and moves
    // erratically with N, which is exactly why junction_margin() exists and is
    // asserted rather than assumed. This loop costs about a millisecond -- there
    // is nothing to gain by moving it, and a fixture-invalidating failure to
    // lose.
    for (Index nodes : {10, 100}) {
        F7CollocationChain m(nodes, 3, 2);
        for (double p : {0.55, 0.68, 0.85}) {
            SCOPED_TRACE(::testing::Message() << "N = " << nodes << ", p = " << p);
            EXPECT_GT(m.junction_margin(p), 0.19) << "a node sits too close to a junction";
            const AnalyticActiveSet a = m.active_set(p);
            ASSERT_EQ(a.ineq_active.size(), static_cast<std::size_t>(nodes));
            for (Index k = 0; k < nodes; ++k) {
                const double t = m.node_time(k);
                const bool inside = (t > m.junction_left(p)) && (t < m.junction_right(p));
                EXPECT_EQ(a.ineq_active[static_cast<std::size_t>(k)] == 1, inside) << "node " << k;
            }
            // No bound is ever analytically active (CHOICE 2).
            EXPECT_EQ(std::count(a.bound_active.begin(), a.bound_active.end(), 0),
                      static_cast<std::ptrdiff_t>(a.bound_active.size()));
        }
    }
}

// =====================================================================
// The structure: banded, and exactly the closed-form counts.
// =====================================================================

// The three nnz counts of the header's STRUCTURE block, plus the bandedness
// claim (a defect row's columns lie inside a two-node window) and the Hessian's
// per-node block shape. Shared by the small and large structure gates.
void check_structure(const F7CollocationChain &model) {
    const Index N = model.node_count();
    const Index ns = model.state_dim();
    const Index nc = model.control_dim();
    const Index nv = model.vars_per_node();
    const Vec x = model.start_point();

    const SpMatRM H = model.eval_hess(x, 1.0, Vec::Zero(model.me()), Vec::Zero(model.mi()));
    ASSERT_EQ(H.rows(), model.n());
    EXPECT_EQ(H.nonZeros(), N * (2 * ns + nc));
    const auto Je = model.eval_jac_e(x);
    EXPECT_EQ(Je.nonZeros(), ns + (N - 1) * (6 * ns - 2));
    const auto Ji = model.eval_jac_i(x);
    EXPECT_EQ(Ji.nonZeros(), N * ns);

    // BANDEDNESS. Every stored entry of Je lies in the two-node window its
    // row belongs to; every entry of H and Ji lies in a single node's block.
    for (int r = 0; r < Je.outerSize(); ++r) {
        const Index row = static_cast<Index>(r);
        const Index first_node = (row < ns) ? 0 : (row - ns) / ns;
        const Index lo = first_node * nv;
        const Index hi = (row < ns) ? nv : (first_node + 2) * nv;
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(Je, r); it; ++it) {
            EXPECT_GE(static_cast<Index>(it.col()), lo) << "Je row " << row;
            EXPECT_LT(static_cast<Index>(it.col()), hi) << "Je row " << row;
        }
    }
    for (int r = 0; r < Ji.outerSize(); ++r) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(Ji, r); it; ++it) {
            EXPECT_EQ(static_cast<Index>(it.col()) / nv, static_cast<Index>(r));
        }
    }
    for (int c = 0; c < H.outerSize(); ++c) {
        for (SpMatRM::InnerIterator it(H, c); it; ++it) {
            EXPECT_EQ(static_cast<Index>(it.row()) / nv, static_cast<Index>(it.col()) / nv);
        }
    }
}

TEST(ScaleF7, StructureMatchesTheClosedFormCounts) {
    for (const auto shape :
         std::vector<std::array<Index, 3>>{{10, 3, 2}, {6, 4, 3}, {5, 1, 1}, {12, 7, 3}}) {
        SCOPED_TRACE(::testing::Message()
                     << "N=" << shape[0] << " ns=" << shape[1] << " nc=" << shape[2]);
        F7CollocationChain model(shape[0], shape[1], shape[2]);
        EXPECT_EQ(model.n(), shape[0] * (shape[1] + shape[2]));
        EXPECT_EQ(model.me(), shape[0] * shape[1]);
        EXPECT_EQ(model.mi(), shape[0]);
        check_structure(model);
    }

    // The Hessian's per-node block, entry by entry: state diagonal
    // obj_scale*h + lambda_i(k), state-control cross obj_scale*h*gamma at
    // column u_{i mod nc}, control diagonal obj_scale*h*rho. THIS is the
    // state-control coupling that makes hess L non-diagonal.
    F7CollocationChain model(10, 3, 2);
    const Vec li = Vec::LinSpaced(model.mi(), 0.1, 1.0);
    const SpMatRM H = model.eval_hess(model.start_point(), 2.0, Vec::Zero(model.me()), li);
    const double h = model.step();
    for (Index k = 0; k < model.node_count(); ++k) {
        const Index b = model.node_offset(k);
        for (Index i = 0; i < model.state_dim(); ++i) {
            EXPECT_DOUBLE_EQ(H.coeff(b + i, b + i), 2.0 * h + li(k)) << "node " << k;
            EXPECT_DOUBLE_EQ(H.coeff(b + i, b + model.state_dim() + (i % model.control_dim())),
                             2.0 * h * model.gamma())
                << "node " << k << " cross " << i;
        }
        for (Index j = 0; j < model.control_dim(); ++j) {
            const Index c = b + model.state_dim() + j;
            EXPECT_DOUBLE_EQ(H.coeff(c, c), 2.0 * h * F7CollocationChain::kRho);
        }
    }
}

// =====================================================================
// 3. THE SCALE GATES.
// =====================================================================

// The directional substitute for assert_hessian at a size where densifying is
// impossible: H*v against a central difference of the Lagrangian gradient
// along v (Phase-4 Task-8's precedent, cited in this file's banner).
void check_directional_derivatives(const F7CollocationChain &model, const Vec &x,
                                   const Vec &lambda_e, const Vec &lambda_i, int directions) {
    const SpMatRM H = model.eval_hess(x, 1.0, lambda_e, lambda_i);
    const auto Je = model.eval_jac_e(x);
    // The -> Vec is load-bearing: without it the lambda returns an Eigen
    // EXPRESSION holding references to temporaries built inside it, which
    // dangle the moment it returns (F3's gate makes the same note).
    auto grad_lagrangian = [&](const Vec &at) -> Vec {
        return model.eval_grad(at) + Je.transpose() * lambda_e +
               model.eval_jac_i(at).transpose() * lambda_i;
    };
    const double h = 1e-6;
    for (int dir = 0; dir < directions; ++dir) {
        SCOPED_TRACE(::testing::Message() << "direction " << dir);
        const Vec v = probe_direction(model.n(), dir);
        // grad f, directionally, against a central difference of f.
        const double fd_f = (model.eval_f(Vec(x + h * v)) - model.eval_f(Vec(x - h * v))) / (2 * h);
        const double an_f = model.eval_grad(x).dot(v);
        EXPECT_LE(std::abs(an_f - fd_f), 1e-6 * std::max(1.0, std::abs(an_f)));
        // Je*v and Ji*v against central differences of cE and cI.
        const Vec fd_ce = (model.eval_ce(Vec(x + h * v)) - model.eval_ce(Vec(x - h * v))) / (2 * h);
        const Vec an_ce = Je * v;
        EXPECT_LE((an_ce - fd_ce).lpNorm<Eigen::Infinity>(),
                  1e-6 * std::max(1.0, an_ce.lpNorm<Eigen::Infinity>()));
        const Vec fd_ci = (model.eval_ci(Vec(x + h * v)) - model.eval_ci(Vec(x - h * v))) / (2 * h);
        const Vec an_ci = model.eval_jac_i(x) * v;
        EXPECT_LE((an_ci - fd_ci).lpNorm<Eigen::Infinity>(),
                  1e-6 * std::max(1.0, an_ci.lpNorm<Eigen::Infinity>()));
        // H*v against a central difference of grad L.
        const Vec an_hv = symmetric_product(H, v);
        const Vec fd_hv =
            (grad_lagrangian(Vec(x + h * v)) - grad_lagrangian(Vec(x - h * v))) / (2 * h);
        EXPECT_LE((an_hv - fd_hv).lpNorm<Eigen::Infinity>(),
                  1e-6 * std::max(1.0, an_hv.lpNorm<Eigen::Infinity>()));
    }
}

// n = 10^4 (N = 10^3, ns = 7, nc = 3). RUNTIME BUDGET: Debug <= 20 s, the
// brief's figure and the F3 precedent's (ParametricF3.ScaleReadinessAtTen
// Thousand measured 12.4 s Debug). MEASURED here, clang, this development
// machine: 4.3 s Debug and 0.33 s Release, of which assert_gradient is 4.1 s
// and 0.30 s -- everything else in this test totals under 0.3 s. The measured
// value is also emitted at the end of the test and recorded as the `seconds`
// property, so a machine that differs reports itself rather than being
// silently slower.
//
// WHY THE COORDINATE-WISE GRADIENT CHECK RUNS AT ONE POINT ONLY: assert_gradient
// costs 2n objective evaluations of O(n) each, so it is quadratic in n --
// ~2e8 flops here. One point is enough at this size because the SAME code path
// is checked at three points, three p and three shapes at N <= 10 above; what
// n = 10^4 adds is size (and the chance for an index-type or allocation
// mistake to surface), not a new branch. assert_jacobians is quadratic in n
// TOO and with a much larger constant here (me and mi both grow with n, unlike
// F3's me = 1), so it is replaced by the exact directional identity below.
TEST(ScaleF7, ScaleReadinessAtTenThousandVariables) {
    const auto t0 = std::chrono::steady_clock::now();
    constexpr Index kNodes = 1000;
    F7CollocationChain model(kNodes, 7, 3, 0.68);
    ASSERT_EQ(model.n(), 10000);
    ASSERT_EQ(model.me(), 7000);
    ASSERT_EQ(model.mi(), 1000);

    check_structure(model);

    const Vec x = model.x_star(0.68) + 0.05 * probe_direction(model.n(), 3);
    const Vec lambda_e = 0.25 * probe_direction(model.me(), 4);
    const Vec lambda_i = Vec::Constant(model.mi(), 0.3);
    EXPECT_TRUE(assert_gradient(model, x, kDerivTol));
    check_directional_derivatives(model, x, lambda_e, lambda_i, 3);

    // The analytic path still holds at this size, on both branches of the
    // window (no solve: a 10^4-variable solve is Task 3's business -- and per
    // docs/notes/2026-07-30-scale-study-cold.md it is cheap in the EMPTY-window
    // regime, 0.06 s, and was not reached in the wide-window one).
    for (double p : {0.55, 0.85}) {
        F7CollocationChain m(kNodes, 7, 3, p);
        check_manufactured_kkt(m, p);
    }

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    RecordProperty("seconds", fmt::format("{:.3f}", seconds));
    fmt::print("[F7 gate] n = 1e4: {:.2f} s (budget: Debug <= 20 s)\n", seconds);
}

// n = 10^5 (N = 10^4, ns = 7, nc = 3). NO coordinate-wise checker runs here --
// both are quadratic in n, i.e. ~2e10 flops, and assert_hessian would need an
// 80 GB dense matrix. Structure + directional identities only, which are O(n)
// each. RUNTIME BUDGET: Debug <= 20 s (same figure); MEASURED 1.6 s Debug and
// 0.07 s Release -- far cheaper than the 10^4 gate precisely because nothing
// here is quadratic in n.
//
// THE PEAK-RSS PRINT IS INFORMATIONAL, per the brief -- printed and recorded,
// never asserted. Read it as an upper bound on this fixture rather than a
// measurement of it: VmHWM is the high-water mark of the WHOLE test binary,
// including everything that ran before. Measured 40.4 MiB Release / 42.5 MiB
// Debug when this file runs alone, against the ~3.5 MiB of live O(n) data the
// model itself holds at n = 10^5 (data_g_ + data_d_ + the two bound vectors,
// 4 vectors of ~10^5 doubles) plus the transient triplet arrays the three
// sparse assemblies build (the largest is Je's: nnz(Je) = ns + (N-1)(6 ns - 2)
// = 7 + 9999*40 = 399 967 triplets, i.e. ~6.1 MiB -- the count this file's own
// StructureMatchesTheClosedFormCounts pins).
TEST(ScaleF7, ScaleReadinessAtHundredThousandVariables) {
    const auto t0 = std::chrono::steady_clock::now();
    constexpr Index kNodes = 10000;
    F7CollocationChain model(kNodes, 7, 3, 0.68);
    ASSERT_EQ(model.n(), 100000);
    ASSERT_EQ(model.me(), 70000);
    ASSERT_EQ(model.mi(), 10000);

    check_structure(model);
    check_manufactured_kkt(model, 0.68);

    const Vec x = model.x_star(0.68) + 0.05 * probe_direction(model.n(), 5);
    check_directional_derivatives(model, x, 0.25 * probe_direction(model.me(), 6),
                                  Vec::Constant(model.mi(), 0.3), 2);

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double rss = peak_rss_mib();
    RecordProperty("seconds", fmt::format("{:.3f}", seconds));
    RecordProperty("peak_rss_mib", fmt::format("{:.1f}", rss));
    fmt::print("[F7 gate] n = 1e5: {:.2f} s, peak RSS {:.1f} MiB (informational)\n", seconds, rss);
}

// =====================================================================
// 4. THE PARAMETRIC CONTRACT (nlp_model.h), AND THE DESIGN-RANGE GUARD.
// =====================================================================

// THE ANALYTIC-PATH ACCESSORS REJECT p OUTSIDE (0, R) -- review round 1, Q-1.
// The family is only a manufactured solution on its design range, and outside
// it the failure is silent in both directions: at p >= R node 0's path row
// collides with the pinned initial condition (rank[Je; Ji_active] = 39 of 40 at
// N = 10, ns = 3, nc = 2, p = 1.05, measured), which is exactly the singular-K0
// geometry the Accelerate standing rule forbids a fixture to present; at p < 0
// the "solution" is not even FEASIBLE (max cI(x*) = +0.209 at p = -0.6,
// measured). Both used to return a confident answer.
//
// THE GUARD IS ON THE ACCESSORS, NOT ON set_parameters OR THE CONSTRUCTOR, and
// that distinction is the point: the MODEL is total in p -- eval_f/eval_ce/
// eval_hess are perfectly well defined at any p, and
// PatternsAreIndependentOfXPMultipliersAndObjScale below deliberately drives it
// to p = 1.4 to check that the sparsity patterns do not move. What is NOT total
// is the CLAIM "x_star(p) is the optimum".
TEST(ScaleF7Contract, AnalyticAccessorsRejectParametersOutsideTheDesignRange) {
    F7CollocationChain model(10, 3, 2);
    const double R = model.radius();

    for (double bad : {R, R + 1e-9, 1.05, 5.0, 0.0, -1e-9, -0.6}) {
        SCOPED_TRACE(::testing::Message() << "p = " << bad);
        EXPECT_THROW(model.x_star(bad), std::invalid_argument);
        EXPECT_THROW(model.f_star(bad), std::invalid_argument);
        EXPECT_THROW(model.lambda_e_star(bad), std::invalid_argument);
        EXPECT_THROW(model.lambda_i_star(bad), std::invalid_argument);
        EXPECT_THROW(model.z_star(bad), std::invalid_argument);
        EXPECT_THROW(model.active_set(bad), std::invalid_argument);
        EXPECT_THROW(model.junction_left(bad), std::invalid_argument);
        EXPECT_THROW(model.junction_right(bad), std::invalid_argument);
        EXPECT_THROW(model.junction_margin(bad), std::invalid_argument);
    }

    // The message names the family, the offending p AND the valid range
    // (project rule T6: the throw is the thing that reports).
    try {
        model.x_star(1.05);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("F7CollocationChain"), std::string::npos) << what;
        EXPECT_NE(what.find("1.05"), std::string::npos) << what;
    }

    // Everything strictly inside the range is accepted, including the empty
    // window below p_activation() and a p just under the saturation boundary.
    for (double good : {1e-6, 0.45, model.p_activation(), 0.68, R - 1e-9}) {
        SCOPED_TRACE(::testing::Message() << "p = " << good);
        EXPECT_NO_THROW(model.x_star(good));
        EXPECT_NO_THROW(model.f_star(good));
        EXPECT_NO_THROW(model.active_set(good));
        EXPECT_NO_THROW(model.junction_left(good));
    }

    // The MODEL stays total in p: set_parameters accepts what the accessors
    // reject, and every eval_* keeps working there.
    EXPECT_NO_THROW(model.set_parameters(Vec::Constant(1, 1.4)));
    const Vec x = model.start_point();
    EXPECT_TRUE(std::isfinite(model.eval_f(x)));
    EXPECT_TRUE(model.eval_ce(x).allFinite());
    EXPECT_EQ(model.eval_hess(x, 1.0, Vec::Zero(model.me()), Vec::Zero(model.mi())).nonZeros(),
              model.node_count() * (2 * model.state_dim() + model.control_dim()));
}

TEST(ScaleF7Contract, PatternsAreIndependentOfXPMultipliersAndObjScale) {
    // nlp_model.h's STRUCTURAL PATTERN INVARIANCE, in all four arguments the
    // Phase-5 Task-0 clause names: x, p, obj_scale and the multiplier VALUES.
    // x = 0 is included on purpose -- it is where Ji's entries (which ARE the
    // state values) are all numerically zero, and obj_scale = 0 is what the
    // restoration model calls eval_hess with.
    F7CollocationChain model(9, 3, 2);
    const Index n = model.n();
    const std::vector<Vec> points{Vec::Zero(n), model.start_point(), model.x_star(0.68),
                                  -0.7 * probe_direction(n, 7)};
    const ModelPattern reference = patterns_at(model, points.front(), 1.0, 1.0);
    for (double p : {0.2, 0.5, 0.68, 0.95, 1.4}) {
        model.set_parameters(Vec::Constant(1, p));
        for (const Vec &x : points) {
            for (double obj_scale : {0.0, 1.0, 7.5}) {
                for (double mult : {0.0, 1.0, -2.0}) {
                    EXPECT_TRUE(patterns_at(model, x, obj_scale, mult) == reference) << fmt::format(
                        "pattern moved at p = {}, obj_scale = {}, mult = {}", p, obj_scale, mult);
                }
            }
        }
    }
    // And the pattern is the banner's counts.
    EXPECT_EQ(reference.hess.size(), static_cast<std::size_t>(9 * (2 * 3 + 2)));
    EXPECT_EQ(reference.jac_e.size(), static_cast<std::size_t>(3 + 8 * (6 * 3 - 2)));
    EXPECT_EQ(reference.jac_i.size(), static_cast<std::size_t>(9 * 3));
}

// TASK 8 (the eval-economics carry): F7's eval_values OVERRIDE must return
// EXACTLY what eval_f/eval_ce/eval_ci return, at several (N, p, x) triples --
// nlp_model.h's own invariant on every eval_values override ("bit-identical
// to calling eval_f(x)/eval_ce(x)/eval_ci(x) directly"), demonstrated on the
// one model in this tree whose override is not the default forwarding impl.
// Zero, x = 0 (F7's Ji-is-numerically-zero point, PatternsAreIndependentOf...
// above), the manufactured x*(p), and an off-solution probe point are all
// covered, at two node counts and two parameters, so a transcription slip in
// any one of the three merged loops (the node loop shared by f/cI, or the
// defect-row loop kept separate) has nowhere to hide.
TEST(ScaleF7Contract, EvalValuesMatchesEvalFCeCiBitForBit) {
    for (Index nodes : {6, 11}) {
        F7CollocationChain model(nodes, 3, 2);
        const Index n = model.n();
        for (double p : {0.4, 0.9}) {
            model.set_parameters(Vec::Constant(1, p));
            const std::vector<Vec> points{Vec::Zero(n), model.start_point(), model.x_star(p),
                                          model.x_star(p) + 0.05 * probe_direction(n, 5)};
            for (const Vec &x : points) {
                double f = 0.0;
                Vec cE, cI;
                model.eval_values(x, f, cE, cI);
                EXPECT_EQ(f, model.eval_f(x)) << fmt::format("nodes={} p={}", nodes, p);
                ASSERT_EQ(cE.size(), model.me());
                ASSERT_EQ(cI.size(), model.mi());
                EXPECT_TRUE(cE == model.eval_ce(x)) << fmt::format("nodes={} p={}", nodes, p);
                EXPECT_TRUE(cI == model.eval_ci(x)) << fmt::format("nodes={} p={}", nodes, p);
            }
        }
    }
}

TEST(ScaleF7Contract, ParameterAccessorsRoundTrip) {
    F7CollocationChain model(8, 3, 2, 0.62);
    EXPECT_EQ(model.parameter_dim(), 1);
    EXPECT_DOUBLE_EQ(model.parameters()(0), 0.62);
    EXPECT_DOUBLE_EQ(model.p(), 0.62);

    // set_parameters changes VALUES: at a fixed x both the objective and the
    // equality rows moved (p enters the boundary datum and the forcing).
    const Vec x = model.start_point();
    const double f_lo = model.eval_f(x);
    const Vec ce_lo = model.eval_ce(x);
    model.set_parameters(Vec::Constant(1, 0.9));
    EXPECT_DOUBLE_EQ(model.parameters()(0), 0.9);
    EXPECT_NE(model.eval_f(x), f_lo);
    EXPECT_GT((model.eval_ce(x) - ce_lo).lpNorm<Eigen::Infinity>(), 1e-6);
}

TEST(ScaleF7Contract, RejectsBadConstructionAndParameterSize) {
    // ParametricNlpModel precondition 2: a size mismatch throws, and the
    // message names the family (project rule T6).
    F7CollocationChain model(8, 3, 2);
    EXPECT_THROW(model.set_parameters(Vec::Zero(2)), std::invalid_argument);
    try {
        model.set_parameters(Vec::Zero(0));
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("F7CollocationChain"), std::string::npos) << e.what();
    }

    EXPECT_THROW(F7CollocationChain(2, 3, 2), std::invalid_argument);
    EXPECT_THROW(F7CollocationChain(0, 3, 2), std::invalid_argument);
    EXPECT_THROW(F7CollocationChain(10, 0, 2), std::invalid_argument);
    EXPECT_THROW(F7CollocationChain(10, 3, 0), std::invalid_argument);
    EXPECT_THROW(F7CollocationChain(10, 3, 2, 0.68, 0.0), std::invalid_argument);
    EXPECT_THROW(F7CollocationChain(10, 3, 2, 0.68, -1.0), std::invalid_argument);
    EXPECT_NO_THROW(F7CollocationChain(3, 1, 1));
}

// =====================================================================
// 5. THE SLOW SUITE -- NOT REGISTERED WITH ctest.
//
// tests/CMakeLists.txt passes TEST_FILTER "-ScaleF7Slow.*" to
// gtest_discover_tests, so nothing in this suite is registered as a CTest test
// and the per-commit `ctest --test-dir build[-debug]` run never executes it.
// It is still an ordinary gtest and is run directly:
//
//     ./tests/hven_sqp_tests --gtest_filter='ScaleF7Slow.*'
//
// WHEN IT MUST RUN: in the PHASE-GATE Debug sweep (CLAUDE.md's "the Debug suite
// must be green before each phase merge" applies to this file too -- the
// exclusion is from the per-commit cadence, not from the gate), and in any
// Release CI run, where it costs 2.6 s.
//
// WHY IT IS OUT OF THE PER-COMMIT RUN, DELTA FIRST BECAUSE THE DELTA IS WHAT
// THE DECISION RESTS ON: excluding the single solve below removes ~80 s of
// Debug time from every commit (the test measures 77-81 s across 4 runs; see
// AnalyticPathMatchesColdSolveAtFiftyNodes's comment for the whole size table).
// For scale, the totals either side of the split -- all clang, this development
// machine, and all few-sample, so read them as leading digits rather than
// stopwatch readings: the whole Debug suite was ~200 s before (200-201 s, 2
// runs) and is ~120 s after (117-128 s, 4 runs), with this file's own
// per-commit Debug contribution falling from ~94 s (1 run) to ~24 s (23-24 s,
// 2 runs). Release is ~10 s throughout. What it uniquely adds over the
// N = 50 twin that DOES run per commit is
// the DRIVER under Eigen's asserts on a 88-row active set rather than a 44-row
// one -- worth having, not worth 85 s per commit, and Task 3 measured that path
// properly in Release anyway. Eigen's asserts still see this family at N = 10^3
// and N = 10^4 every commit through the two scale gates.
// =====================================================================

TEST(ScaleF7Slow, AnalyticPathMatchesColdSolveAtHundredNodesWideWindow) {
    F7CollocationChain model(100, 3, 2);
    ASSERT_GT(model.junction_margin(0.85), 0.19);
    check_path_at(model, 0.85);
}

// PHASE-6 TASK 4 -- THE CRASH BASIS IS INERT ON F7, PINNED.
//
// FIX ROUND 1 (task-4 review, finding 1). The task shipped
// docs/notes/data/2026-08-03-crash-basis/f7_crash_arms.csv as its F7 evidence
// and no TEST, which left the lever's most load-bearing property -- that it
// changes nothing on this family -- with no regression net at all: widening
// the seeding threshold (the change the note's Sec. 1.1 and Sec. 5 argue
// hardest against, because an over-generous seed costs strictly MORE than no
// seed) would have altered F7 cold silently. This arm closes that.
//
// WHY N = 100 / p = 0.90 SPECIFICALLY. It is the exact cell the note's Sec. 2
// dissects per subproblem, and it is the friendliest wide-window cell that
// still exhibits the whole mechanism: 4 majors, 850 minors, of which the FIRST
// (and only crash-seedable) subproblem spends 2. So a seed that fires here
// would be firing on the one QP that cannot pay for it, which is precisely the
// finding worth protecting.
//
// WHAT IS ASSERTED, and both halves matter:
//   (a) crash_seeded_rows == 0 AND crash_seeded_bounds == 0 -- F7's start point
//       (a flat state profile at 30 % of the path radius) leaves every path row
//       slack by 0.455, which is 4.55e8 * feas_tol, and no control is near its
//       box. A threshold widened far enough to seed here fails this.
//   (b) COUNTER IDENTITY on/off, including x bit-for-bit -- so a seed that
//       somehow fired without changing the two counters would still be caught.
//
// OBSERVED VALUES (MKL Pardiso, clang++; raw in the CSV above): kOptimal,
// 4 majors, 850 minors, 6 factorizations, x_err 2.592006e-09 -- IDENTICAL IN
// RELEASE AND DEBUG, which is this file's own discipline for a counter pin.
// ACCELERATE STANDING RULE: these are MKL figures and no
// Apple observation exists for them yet -- the arm asserts the ON/OFF DELTA
// (which is backend-independent by construction, since both arms run the same
// backend) plus the two seed counts (which are pure driver arithmetic off the
// subproblem's own bi/lower/upper and cannot vary by backend); the absolute
// 850/6 are asserted too, and if THEY move on Accelerate the count belongs in
// the divergence register.
//
// WHY ScaleF7Slow AND NOT THE PER-COMMIT SUITE, with the Debug cost stated
// because it is the number a phase-gate budget actually needs: 3.6 s in
// Release for the pair, and 146.7 s in DEBUG (41x -- Eigen's runtime asserts
// on an 850-minor walk over 92 active rows). Both are informational, both are
// one otherwise-idle run of this machine. That Debug figure is the largest
// single arm in this suite and is the reason this test is here rather than in
// the per-commit run; it is NOT a reason to weaken it, since the property it
// guards (a widened seeding threshold silently changing F7 cold) has no other
// net anywhere.
TEST(ScaleF7Slow, CrashBasisIsInertOnAWideWindowColdSolve) {
    const double p = 0.90;
    const auto make_options = [] {
        SqpOptions opts;
        opts.kkt_tol = 1e-9;
        opts.feas_tol = 1e-9;
        opts.max_iter = 60;
        return opts; // qp.max_iter at the library default (M6's sentinel)
    };

    F7CollocationChain model_off(100, 3, 2, p, 1.0);
    SqpDriver driver_off(make_options());
    const SqpSolution off = driver_off.solve(model_off, model_off.start_point());

    SqpOptions on_opts = make_options();
    on_opts.crash_basis = true;
    F7CollocationChain model_on(100, 3, 2, p, 1.0);
    SqpDriver driver_on(on_opts);
    const SqpSolution on = driver_on.solve(model_on, model_on.start_point());

    ASSERT_EQ(off.status, SqpStatus::kOptimal);
    ASSERT_EQ(on.status, SqpStatus::kOptimal);

    // (a) THE LEVER FINDS NOTHING TO SEED -- the property a widened threshold
    // would break, and the reason the crash basis cannot help this family.
    EXPECT_EQ(off.counters.crash_seeded_rows, 0);
    EXPECT_EQ(off.counters.crash_seeded_bounds, 0);
    EXPECT_EQ(on.counters.crash_seeded_rows, 0)
        << "F7's start point leaves every path row slack by 0.455 = 4.55e8 * feas_tol; "
           "a seed here means the activity threshold has been widened past the driver's own "
           "definition of geometric activity (sqp_types.h's SqpOptions::crash_basis)";
    EXPECT_EQ(on.counters.crash_seeded_bounds, 0);

    // (b) AND THEREFORE NOTHING MOVES. Belt and braces: a seed that fired
    // without moving the counters above would still be caught here.
    EXPECT_EQ(on.counters.major_iters, off.counters.major_iters);
    EXPECT_EQ(on.counters.qp_minor_iters, off.counters.qp_minor_iters);
    EXPECT_EQ(on.counters.factorizations, off.counters.factorizations);
    EXPECT_EQ(on.counters.steps_accepted, off.counters.steps_accepted);
    EXPECT_EQ(on.counters.rejected_steps, off.counters.rejected_steps);
    EXPECT_EQ(on.x, off.x); // bit-for-bit
    EXPECT_EQ(on.f, off.f);

    // The observed absolutes, so a trajectory change anywhere in this cell
    // surfaces here rather than only as a delta that happens to stay zero.
    EXPECT_EQ(off.counters.major_iters, 4);
    EXPECT_EQ(off.counters.qp_minor_iters, 850);
    EXPECT_EQ(off.counters.factorizations, 6);
    EXPECT_LT((off.x - model_off.x_star(p)).lpNorm<Eigen::Infinity>(), 1e-8);
}

// PHASE-6 TASK 4 (M6) -- THE SIZE-DERIVED CAP, END TO END.
//
// N = 50, p = 0.85 is the SMALLEST wide-window cell whose one substantive
// subproblem demands more minors than the pre-M6 fixed default of 500: it
// wants 629 (docs/notes/2026-08-03-crash-basis.md Sec. 4). So the same solve,
// at the same tolerances, differs ONLY in QpOptions::max_iter and differs in
// OUTCOME -- which is the whole argument for the default change, made once,
// as a test rather than only as a table.
//
// WHY HERE AND NOT IN THE PER-COMMIT SUITE. It costs ~1 s in Release and
// several in Debug, and the POLICY it exercises is already pinned directly and
// cheaply by tests/test_qp_engine.cpp's QpEngineSizeDerivedCap battery (the
// base, the coefficient, the floor and the precedence rule, all without
// solving anything). This test adds the end-to-end consequence, which belongs
// with the family that produces it and at the cadence this suite already runs.
TEST(ScaleF7Slow, TheSizeDerivedCapRecoversASolveTheOldFixedDefaultLost) {
    const double p = 0.85;
    F7CollocationChain model(50, 3, 2, p, 1.0);

    SqpOptions opts;
    opts.kkt_tol = 1e-9;
    opts.feas_tol = 1e-9;
    opts.max_iter = 60;
    // qp.max_iter deliberately LEFT AT THE LIBRARY DEFAULT -- the sentinel.
    ASSERT_LE(opts.qp.max_iter, 0) << "the shipped default must BE the sentinel";

    SqpDriver derived(opts);
    const SqpSolution good = derived.solve(model, model.start_point());
    EXPECT_EQ(good.status, SqpStatus::kOptimal);
    EXPECT_LT((good.x - model.x_star(p)).lpNorm<Eigen::Infinity>(), 1e-8);
    // OBSERVED: 4 majors / 635 minors, against a derived cap of
    // max(500, 5 * (250 + 50 + 100)) = 2000.
    EXPECT_GT(good.counters.qp_minor_iters, 500)
        << "the fixture must actually exceed the old default, or this asserts nothing";

    // THE CONTROL: the same solve at the pre-M6 fixed cap, reached through the
    // escape hatch. It fails, at a point nowhere near x* -- exactly what
    // docs/notes/2026-07-30-scale-study-cold.md Sec. 5 recorded.
    SqpOptions old_default = opts;
    old_default.qp.max_iter = 500;
    SqpDriver fixed(old_default);
    const SqpSolution bad = fixed.solve(model, model.start_point());
    EXPECT_NE(bad.status, SqpStatus::kOptimal);
    EXPECT_GT((bad.x - model.x_star(p)).lpNorm<Eigen::Infinity>(), 1e-3);
}

} // namespace
} // namespace hven::solvers
