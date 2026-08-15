// tests/test_scale_smoke.cpp — Task 10: moderate-scale smoke test.
//
// Every other test in this suite runs at toy sizes (n <= ~10 in the dense-
// oracle-checked batteries, n <= ~30 in the largest randomized ones). This
// file is the phase's first evidence beyond that: a single block-tridiagonal
// convex QP at n ~ 3000, solved cold and warm, in both ws_algebra modes, with
// SELF-CHECKED KKT residuals standing in for the dense oracle (which is
// exponential in mi + bounded-variable count and cannot run at this size at
// all -- see tests/sqp/support/dense_oracle.h).
//
// This is deliberately also a SPIKE (per the task brief): the assertions
// below are chosen to be ROBUST to small cross-machine/library-version
// iteration-count wobble (a genuinely nondeterministic MKL Pardiso thread
// count can, in principle, perturb which floating-point operations round
// which way and shift a ratio-test tie by one iteration), while the exact
// numbers actually observed on this machine are recorded in comments and via
// RecordProperty/Ledger for the companion note,
// docs/notes/2026-07-28-border-mode-scale-smoke.md, which is the place the
// spike's real findings live.
//
// =====================================================================
// PHASE-5 TASK 3 ADDITION -- THE F7 COLD-START SCALE SMOKES.
//
// Everything above this line is Phase-3's QP-LEVEL spike (a hand-built banded
// QP handed straight to a QpEngine). What follows is Phase-5's DRIVER-LEVEL
// one: whole SqpDriver cold solves of tests/sqp/support/scale_problems.h's F7
// collocation family, whose optimum is known in closed form at every size, so
// these can pin CORRECTNESS at scale and not merely self-consistency.
//
// THE SIZE OF THESE TESTS IS SET BY TASK 3'S MEASURED DIAGNOSIS, NOT BY THE
// PLAN'S ORIGINAL GUESS -- see docs/notes/2026-07-30-scale-study-cold.md for
// the full study. The one-line version: F7's cold-solve cost splits cleanly in
// two along the family's own activation threshold p_activation() = R/2, and
// the two halves scale nothing alike.
//
//   * EMPTY WINDOW (p < R/2, no path row active). The solve is one major and
//     two minors at ANY size, so its cost is pure assembly + one sparse
//     factorization, and it scales essentially LINEARLY: measured cold, clang,
//     MKL Pardiso (MKL_NUM_THREADS=1), this development machine, at p = 0.45 --
//     n = 10^4 -> 0.07 s / 24 MiB, 10^5 -> 0.6 s / 138 MiB, 10^6 -> 7 s /
//     1.3 GiB, every one kOptimal. n = 10^6 IS REACHED. That is the arm the
//     two size-carrying tests below live on.
//   * WIDE WINDOW (p in (R/2, R), ~0.9N path rows active). Cost is dominated
//     by identifying a Theta(n)-sized active set, and it is the regime the
//     N = 200 wall-time cliff recorded in tests/test_scale_problems.cpp's
//     CARRY 2 lives in. n = 10^4 WAS NOT REACHED THERE as a single cold solve
//     in either configuration the study tried (kNumericalError after 819 s at
//     qp.max_iter = 20000; no finish in 3000 s uncapped); the largest that
//     terminated is n = 7500 in 434 s. So no test here solves a wide window
//     above the sizes test_scale_problems.cpp already covers.
//
// THE CLIFF'S MECHANISM IS PINNED ANYWAY, at a size that costs a few seconds
// in Debug, by SchurCapExhaustionBuysFactorizationsNotIterations below: the
// cliff is a QpOptions::schur_cap-triggered K0-rebuild storm, which reproduces
// at ANY N once schur_cap is set below the live border count (which is the
// PIN count -- see SchurCapExhaustionBuysFactorizationsNotIterations' own
// header for the Phase-5 Task 4 correction). Pinning the
// mechanism rather than the size is what keeps this a per-commit test.

#include <algorithm>
#include <chrono>
#include <random>
#include <string>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/sqp/qp_engine.h>
#include <hven/detail/sqp/sqp_driver.h>
#include <hven/drivers/sqp_types.h>

#include "support/nlp_kkt_check.h"
#include "support/scale_problems.h"

using namespace hven::solvers;

namespace {

// --- The generator -------------------------------------------------------
//
// banded_qp(n_blocks, block, seed) builds a block-tridiagonal convex QP of
// size n = n_blocks * block:
//
//   H:  block-tridiagonal. Each diagonal block is M_b^T M_b + I (SPD by
//       construction, same convention as random_strictly_convex in
//       test_qp_engine.cpp/test_qp_warm_start.cpp/test_ledger.cpp, but one
//       instance per block rather than one for the whole matrix). Each pair
//       of adjacent blocks is coupled by a SCALED IDENTITY (kCouplingScale =
//       0.1) rather than a second random block, which is what makes the
//       result block-TRIDIAGONAL (banded) rather than dense: coupling
//       through a diagonal matrix keeps every off-diagonal block's nonzero
//       pattern trivial (one entry per row) regardless of `block`.
//
//       SPD IS GUARANTEED ANALYTICALLY, not merely observed: this is a block
//       generalization of diagonal dominance. Each diagonal block's smallest
//       eigenvalue is >= 1 (M_b^T M_b is PSD, so M_b^T M_b + I has every
//       eigenvalue >= 1, independent of `block`). Each block-row touches at
//       most two neighbors, each coupled through a matrix of spectral norm
//       kCouplingScale = 0.1, so the total off-diagonal operator-norm mass
//       per block-row is at most 2*0.1 = 0.2 < 1 <= (diagonal block's
//       smallest eigenvalue) for EVERY block, independent of n_blocks or
//       block. That is exactly the hypothesis of the block/Gershgorin
//       generalization of diagonal dominance implying positive
//       definiteness, so H is SPD for any n_blocks/block/seed this function
//       is called with -- this is not sensitive to the random draw at all.
//
//   Bounds: symmetric box lower = -kBoxWidth, upper = +kBoxWidth on every
//       variable (kBoxWidth = 1.0). Traced at n_blocks=100, block=30, seed=42
//       (the n ~ 3000 fixture below): 49 of the 3000 variables end up pinned
//       at a bound at the optimum -- a moderate, nontrivial-but-not-total
//       active set, not the pathological "every variable pinned" shape
//       qp_engine.h's LATCH section describes (which would make
//       schur_cap-forced rebuilding permanently a no-op).
//
//   General inequalities: ~10% of variables (kIneqFrac = 0.1) each get ONE
//       single-entry row e_idx^T x <= bi(idx), idx stepping by
//       n/mi across the index range (so the selected rows are spread evenly
//       across every block rather than clustered in one). bi is drawn
//       uniform in [-kIneqTightness, kIneqTightness] (kIneqTightness = 0.5):
//       since the cold start clamps to x = 0 and each row's value at x = 0
//       is 0, a negative bi(idx) is VIOLATED at the start point (exercising
//       qp_engine.h's shifted-constraint homotopy, step 1), while a positive
//       one starts slack and may or may not bind later. Traced: 167 of the
//       300 rows are active at the optimum.
//
// Both counts above are OBSERVATIONS about this exact fixture (n_blocks=100,
// block=30, seed=42), recorded here as a sizing sanity check for future
// readers -- they are not asserted by this generator (a different seed would
// trace differently) and are re-derived from the actual returned solutions
// inside the test below rather than hardcoded.
QpProblem banded_qp(Index n_blocks, Index block, unsigned seed) {
    constexpr double kCouplingScale = 0.1;
    constexpr double kBoxWidth = 1.0;
    constexpr double kIneqFrac = 0.1;
    constexpr double kIneqTightness = 0.5;

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(-1.0, 1.0);

    const Index n = n_blocks * block;

    std::vector<Eigen::Triplet<double>> h_trips;
    h_trips.reserve(static_cast<std::size_t>(n_blocks * block * block + n_blocks * block));
    for (Index b = 0; b < n_blocks; ++b) {
        Eigen::MatrixXd M(block, block);
        for (Index i = 0; i < block; ++i) {
            for (Index j = 0; j < block; ++j) {
                M(i, j) = unit(rng);
            }
        }
        const Eigen::MatrixXd diag_block =
            M.transpose() * M + Eigen::MatrixXd::Identity(block, block);
        const Index base = b * block;
        for (Index i = 0; i < block; ++i) {
            for (Index j = i; j < block; ++j) {
                h_trips.emplace_back(base + i, base + j, diag_block(i, j));
            }
        }
        if (b + 1 < n_blocks) {
            // Coupling block is kCouplingScale * I between block b and
            // block b+1: a diagonal (hence trivially upper-triangle-storable
            // as one entry per row) coupling that keeps H's sparsity pattern
            // block-tridiagonal.
            const Index base2 = (b + 1) * block;
            for (Index i = 0; i < block; ++i) {
                h_trips.emplace_back(base + i, base2 + i, kCouplingScale);
            }
        }
    }

    QpProblem qp;
    qp.H = SpMatU(n, n);
    qp.H.setFromTriplets(h_trips.begin(), h_trips.end());
    qp.H.makeCompressed();

    qp.g = Vec(n);
    for (Index i = 0; i < n; ++i) {
        qp.g(i) = 2.0 * unit(rng);
    }

    qp.Ae.resize(0, n);
    qp.be = Vec(0);

    const Index mi = static_cast<Index>(kIneqFrac * static_cast<double>(n));
    const Index stride = n / std::max<Index>(mi, 1);
    std::vector<Eigen::Triplet<double>> ai_trips;
    ai_trips.reserve(static_cast<std::size_t>(mi));
    Vec bi(mi);
    for (Index k = 0; k < mi; ++k) {
        const Index idx = k * stride;
        ai_trips.emplace_back(k, idx, 1.0);
        bi(k) = kIneqTightness * unit(rng);
    }
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(mi, n);
    qp.Ai.setFromTriplets(ai_trips.begin(), ai_trips.end());
    qp.Ai.makeCompressed();
    qp.bi = bi;

    qp.lower = Vec::Constant(n, -kBoxWidth);
    qp.upper = Vec::Constant(n, kBoxWidth);
    return qp;
}

// --- Self-check (a): stationarity / primal feasibility / complementarity --
//
// No dense oracle exists at this size (enumerate_kkt_candidates is
// exponential in mi + bounded-variable count; 300 general rows alone put
// 2^mi far beyond tests/sqp/support/dense_oracle.h's own guard). Instead this
// verifies the returned QpSolution is a KKT point OF ITS OWN RIGHT, reading
// only qp_problem.h's stationarity convention (grad(f) + Ae^T lambda_e +
// Ai^T lambda_i - z = 0, lambda_i >= 0, z >= 0 at an active lower bound,
// z <= 0 at an active upper bound) directly off `sol`.
struct KktResidual {
    double stationarity = 0.0;    // inf-norm of grad(f) + Ae^T le + Ai^T li - z
    double primal = 0.0;          // inf-norm of the worst bound/row/equality violation
    double complementarity = 0.0; // inf-norm of lambda_i(j)*slack(j), z(i)*dist(i)
};

KktResidual self_check_kkt(const QpProblem &qp, const QpSolution &sol) {
    KktResidual r;

    Vec grad = qp.H.selfadjointView<Eigen::Upper>() * sol.x + qp.g;
    if (qp.me() > 0) {
        grad += qp.Ae.transpose() * sol.lambda_e;
    }
    Vec Aix = Vec::Zero(qp.mi());
    if (qp.mi() > 0) {
        Aix = qp.Ai * sol.x;
        grad += qp.Ai.transpose() * sol.lambda_i;
    }
    r.stationarity = (grad - sol.z).lpNorm<Eigen::Infinity>();

    for (Index i = 0; i < qp.n(); ++i) {
        r.primal = std::max(r.primal, std::max(0.0, qp.lower(i) - sol.x(i)));
        r.primal = std::max(r.primal, std::max(0.0, sol.x(i) - qp.upper(i)));
    }
    for (Index j = 0; j < qp.mi(); ++j) {
        r.primal = std::max(r.primal, std::max(0.0, Aix(j) - qp.bi(j)));
    }
    if (qp.me() > 0) {
        Vec eq_resid = qp.Ae * sol.x - qp.be;
        r.primal = std::max(r.primal, eq_resid.lpNorm<Eigen::Infinity>());
    }

    for (Index j = 0; j < qp.mi(); ++j) {
        r.complementarity =
            std::max(r.complementarity, std::abs(sol.lambda_i(j) * (Aix(j) - qp.bi(j))));
    }
    for (Index i = 0; i < qp.n(); ++i) {
        const double dist_lo = sol.x(i) - qp.lower(i);
        const double dist_up = qp.upper(i) - sol.x(i);
        const double dist = std::min(dist_lo, dist_up);
        r.complementarity = std::max(r.complementarity, std::abs(sol.z(i) * dist));
    }
    return r;
}

} // namespace

// n_blocks=100, block=30 -> n = 3000, mi = 300 (~10% of n). Traced wall time
// for the whole test (generator + 4 solves + self-checks) on this machine:
// well under 2s, comfortably inside the 30s budget -- no size-down was
// needed. See docs/notes/2026-07-28-border-mode-scale-smoke.md for the full
// counters/wall-clock table this test's RecordProperty calls feed.
TEST(QpScaleSmoke, ModerateScaleBorderModeMatchesRefactorizeAndReusesHotStart) {
    QpProblem qp = banded_qp(/*n_blocks=*/100, /*block=*/30, /*seed=*/42);
    ASSERT_EQ(qp.n(), 3000);
    ASSERT_EQ(qp.mi(), 300);

    Ledger ledger;

    // Border mode is QpOptions' default (kSchurBorder) -- deliberately left
    // at its DEFAULT schur_cap (128) rather than tuned up to dodge a rebuild,
    // so this fixture exercises the schur_cap-forced-rebuild path a real
    // caller would hit out of the box at this scale (see assertion (b)
    // below and the companion note for what that costs).
    QpEngine engine_border{QpOptions{}};
    engine_border.attach_ledger(&ledger, "border");

    QpOptions refactorize_opts;
    refactorize_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    QpEngine engine_refactorize{refactorize_opts};
    engine_refactorize.attach_ledger(&ledger, "refactorize");

    const auto t0 = std::chrono::steady_clock::now();
    const QpSolution cold_border = engine_border.solve(qp);
    const auto t1 = std::chrono::steady_clock::now();
    const QpSolution cold_refactorize = engine_refactorize.solve(qp);
    const auto t2 = std::chrono::steady_clock::now();

    ASSERT_EQ(cold_border.status, QpStatus::kOptimal);
    ASSERT_EQ(cold_refactorize.status, QpStatus::kOptimal);

    // Two independently-derived answers (bound-eliminated K vs a Schur
    // border over the full-variable K0) agreeing is the best correctness
    // evidence available at a size with no dense oracle -- qp_engine.h calls
    // the two modes "observationally equivalent" for convex H, and this is
    // that claim's first check at this scale.
    EXPECT_LT((cold_border.x - cold_refactorize.x).lpNorm<Eigen::Infinity>(), 1e-6);
    EXPECT_EQ(cold_border.bound_state, cold_refactorize.bound_state);
    EXPECT_EQ(cold_border.ineq_active, cold_refactorize.ineq_active);

    // --- (a) self-checked KKT residuals, no oracle -------------------------
    for (const auto &[label, sol] : {std::pair{"cold_border", &cold_border},
                                     std::pair{"cold_refactorize", &cold_refactorize}}) {
        const KktResidual kkt = self_check_kkt(qp, *sol);
        EXPECT_LT(kkt.stationarity, 1e-6) << label;
        EXPECT_LT(kkt.primal, 1e-6) << label;
        EXPECT_LT(kkt.complementarity, 1e-6) << label;
    }

    // --- (b) border mode's counters -----------------------------------
    //
    // factorizations <= 1 + (schur-cap-forced rebuilds). Traced on this
    // fixture: factorizations == 2 (the initial K0 build, plus exactly one
    // rebuild once the live border count crossed the default schur_cap =
    // 128 partway through the solve) -- confirmed to be schur_cap-caused
    // rather than a perturbed-pivot/singularity rebuild by re-running this
    // same fixture with schur_cap raised to 256 in a standalone probe
    // (outside this file, since QpOptions is fixed per QpEngine instance):
    // factorizations drops to exactly 1 there, which is only possible if
    // schur_cap alone was the trigger (a perturbed-pivot-forced rebuild
    // would fire regardless of the cap). See the companion note.
    //
    // Asserted as an inequality (not a hard pin on 2) so a different
    // MKL/Pardiso build that shifts a ratio-test tie by a handful of
    // iterations -- moving exactly when the border count crosses schur_cap
    // -- cannot spuriously fail this test; "at most one forced rebuild" is
    // the actual claim, and 2 is that bound's exact value here.
    EXPECT_GE(cold_border.counters.factorizations, 1); // at least the initial K0 build
    EXPECT_LE(cold_border.counters.factorizations, 2); // <= 1 initial + 1 forced rebuild

    // schur_updates "equals the working-set change count": exact equality
    // with an independently-reconstructed count is not obtainable from
    // outside the engine (a rebuild folds ROWS -- but never PINS -- back
    // into K0, so the post-rebuild schur_updates total is not simply "final
    // active count", and this file has no hook into the engine's
    // per-iteration ws mutations). What IS externally verifiable, and
    // asserted below as the consistency check the brief calls for: every
    // variable pinned at exit needed at least one add_border call that is
    // still live (a pin can NEVER be folded into K0 -- see qp_engine.h's
    // "NOT EVERY UNTRUSTWORTHY BORDER STACK IS FIXABLE" paragraph), so
    // schur_updates is at least the exit-time pinned-variable count; and it
    // is bounded above by a small multiple of minor_iters, since each major
    // iteration performs at most one working-set mutation (a drop or a
    // ratio-test add) plus whatever the one rebuild re-added. Traced:
    // schur_updates == 197 against minor_iters == 193 and 49 variables
    // pinned at exit -- both bounds hold with room to spare.
    const Index exit_pinned =
        std::count_if(cold_border.bound_state.begin(), cold_border.bound_state.end(),
                      [](BoundState s) { return s != BoundState::kFree; });
    EXPECT_GE(cold_border.counters.schur_updates, exit_pinned);
    EXPECT_LE(cold_border.counters.schur_updates, 2 * cold_border.counters.minor_iters);

    // --- (c) warm re-solve after a 1e-4 g perturbation ----------------------
    //
    // Same engine INSTANCEs as above (border_ state on engine_border, and
    // the reuse-fingerprint members beside it, persist across solve()
    // calls), so this exercises Task 5's hot-start K0 reuse for real.
    QpProblem qp_perturbed = qp;
    qp_perturbed.g.array() += 1e-4;

    const auto t3 = std::chrono::steady_clock::now();
    const QpSolution warm_border = engine_border.solve(qp_perturbed, cold_border);
    const auto t4 = std::chrono::steady_clock::now();
    const QpSolution warm_refactorize = engine_refactorize.solve(qp_perturbed, cold_refactorize);
    const auto t5 = std::chrono::steady_clock::now();

    ASSERT_EQ(warm_border.status, QpStatus::kOptimal);
    ASSERT_EQ(warm_refactorize.status, QpStatus::kOptimal);
    EXPECT_LT((warm_border.x - warm_refactorize.x).lpNorm<Eigen::Infinity>(), 1e-6);

    for (const auto &[label, sol] : {std::pair{"warm_border", &warm_border},
                                     std::pair{"warm_refactorize", &warm_refactorize}}) {
        const KktResidual kkt = self_check_kkt(qp_perturbed, *sol);
        EXPECT_LT(kkt.stationarity, 1e-6) << label;
        EXPECT_LT(kkt.primal, 1e-6) << label;
        EXPECT_LT(kkt.complementarity, 1e-6) << label;
    }

    // minor_iters <= 25% of the cold solve's.
    EXPECT_LE(static_cast<double>(warm_border.counters.minor_iters),
              0.25 * static_cast<double>(cold_border.counters.minor_iters));

    // factorizations == 0 -- per the qp_engine.h carry-forward, this is NOT
    // asserted unconditionally on every warm re-solve (a carried
    // needs_refactorization()/perturbed-pivot condition can still force a
    // rebuild). It is asserted here because this run CONFIRMS all three
    // hot-start reuse conditions held: only g moved (H/Ae/Ai are
    // byte-identical, so their structural/values hashes are unchanged), the
    // warm seed IS cold_border's own exit working set (condition (b): the
    // seed passed is cold_border itself), and the cold solve's own final K0
    // factorization carried no perturbed pivots. That last condition is
    // confirmed by this run, not derivable a priori from the fixture's
    // parameters -- whether a pivot gets perturbed is a property of the
    // actual factorization the solve performs, not something readable off
    // H/Ae/Ai in advance. Traced: warm_border.counters.{minor_iters,
    // factorizations} == {2, 0} against cold_border's 193/2.
    EXPECT_EQ(warm_border.counters.factorizations, 0);

    // --- (d) wall-clock ratio: recorded, NOT asserted (machines vary) ------
    const auto ms = [](std::chrono::steady_clock::time_point a,
                       std::chrono::steady_clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    const double cold_border_ms = ms(t0, t1);
    const double cold_refactorize_ms = ms(t1, t2);
    const double warm_border_ms = ms(t3, t4);
    const double warm_refactorize_ms = ms(t4, t5);

    RecordProperty("cold_border_ms", fmt::format("{:.2f}", cold_border_ms));
    RecordProperty("cold_refactorize_ms", fmt::format("{:.2f}", cold_refactorize_ms));
    RecordProperty("warm_border_ms", fmt::format("{:.2f}", warm_border_ms));
    RecordProperty("warm_refactorize_ms", fmt::format("{:.2f}", warm_refactorize_ms));
    RecordProperty("cold_border_over_refactorize_ratio",
                   fmt::format("{:.4f}", cold_border_ms / cold_refactorize_ms));
    RecordProperty("warm_border_over_refactorize_ratio",
                   fmt::format("{:.4f}", warm_border_ms / warm_refactorize_ms));
    RecordProperty("ledger_summary_table", ledger.summary_table());

    SUCCEED() << "\n"
              << ledger.summary_table() << "\nwall-clock (ms): cold_border=" << cold_border_ms
              << " cold_refactorize=" << cold_refactorize_ms << " warm_border=" << warm_border_ms
              << " warm_refactorize=" << warm_refactorize_ms
              << "\ncold border/refactorize ratio=" << (cold_border_ms / cold_refactorize_ms)
              << " warm border/refactorize ratio=" << (warm_border_ms / warm_refactorize_ms);
}

// =====================================================================
// PHASE-5 TASK 3: THE F7 COLD-START SCALE SMOKES. See this file's banner for
// why they are sized the way they are, and
// docs/notes/2026-07-30-scale-study-cold.md for the study they summarize.

namespace {

using hven::solvers::test_support::AnalyticActiveSet;
using hven::solvers::test_support::F7CollocationChain;

// The options every F7 smoke below solves with, EXCEPT for the one knob each
// test names. Deliberately the same tolerances as
// tests/test_scale_problems.cpp's tight_options() so a counter measured here
// is comparable with the ones recorded there; qp.max_iter is left at the
// LIBRARY DEFAULT here rather than that file's 5000, because every
// solve below is either an empty-window solve (2 minors -- the cap is not
// remotely in play) or an explicitly capped diagnostic, and the whole point of
// the note's max_iter policy section is that the default must not be silently
// worked around where it does not need to be.
//
// MARKED CORRECTION, PHASE-6 TASK 4 (M6): that library default is no longer
// the fixed 500 this comment used to name -- it is the SIZE-DERIVED sentinel
// (types.h's QpOptions::max_iter). Nothing below changes behaviour, and that
// is the point: every solve here is either 2 minors or explicitly capped, so
// the cap it would have been measured against never binds either way.
SqpOptions f7_smoke_options() {
    SqpOptions opts;
    opts.kkt_tol = 1e-9;
    opts.feas_tol = 1e-9;
    opts.max_iter = 60;
    return opts;
}

// The checks every F7 cold solve below makes against the family's MANUFACTURED
// optimum -- the thing a self-consistency check cannot do and the reason these
// tests use F7 rather than the banded QP above. Everything except `x_tol`
// matches tests/test_scale_problems.cpp's check_path_at; this asserts on the
// same quantities at sizes that file does not reach.
//
// WHY x_tol IS A PARAMETER AND THE OTHER TOLERANCES ARE NOT. The FORWARD error
// in x is the one quantity here that is size-dependent, and its growth is a
// property of the trapezoidal transcription, not of the solver: with h =
// 1/(N-1) the collocation KKT system's condition number grows like N^2, so a
// residual-based convergence test held at a fixed kkt_tol buys forward
// accuracy that degrades like N^2. MEASURED on this arm (p = 0.45, clang, MKL
// Pardiso, MKL_NUM_THREADS=1, this machine), |x - x*|inf against n:
//
//     n = 10^3 -> 2.3e-12    n = 10^4 -> 2.3e-10
//     n = 10^5 -> 2.3e-08    n = 10^6 -> 2.3e-06
//
// -- a clean 100x per DECADE of n, i.e. exactly the N^2 law, over three
// decades. The OBJECTIVE does not degrade with it (relative |f - f*| stays at
// 1e-15..6e-13 across the same span, because f is stationary at x*), nor does
// the KKT residual, which is why those two keep fixed tolerances below while
// this one is passed in per size. Each caller passes a tolerance with
// ~2 orders of headroom over the measured value at ITS size; a failure here
// therefore means a real change, not the conditioning that is already
// accounted for. This is a COST/CONDITIONING REALITY, not a defect -- see the
// note's per-decade analysis.
void check_against_manufactured_optimum(const F7CollocationChain &model, const SqpSolution &sol,
                                        double p, const SqpOptions &opts, double x_tol,
                                        const char *label) {
    ASSERT_EQ(sol.status, SqpStatus::kOptimal) << label;
    const double f_star = model.f_star(p);
    EXPECT_LE(std::abs(sol.f - f_star), 1e-7 * std::max(1.0, std::abs(f_star))) << label;
    EXPECT_LE((sol.x - model.x_star(p)).lpNorm<Eigen::Infinity>(), x_tol) << label;
    const hven::solvers::test_support::NlpKktResidual chk =
        hven::solvers::test_support::self_check_kkt(model, sol, opts.feas_tol);
    EXPECT_LT(chk.stationarity, 1e-7) << label;
    EXPECT_LT(chk.primal, 1e-8) << label;
    EXPECT_LT(chk.complementarity, 1e-8) << label;
}

} // namespace

// n = 10^4 (N = 2000 nodes, ns = 3, nc = 2, so n = N*(ns + nc) = 5N -- the
// mapping the whole study uses), cold, EMPTY WINDOW (p = 0.45 < R/2, the same
// empty-window sample tests/test_scale_problems.cpp's path_samples() uses).
//
// THIS IS THE BRIEF'S n = 10^4 PIN, ON THE ONLY ARM THAT REACHES IT. The
// original brief asked for an n = 10^4 F7 cold solve without distinguishing
// the two regimes; Task 3's measurements found that a WIDE-window cold solve
// was not reached at that size on this machine in either configuration tried
// (see the note), while an empty-window one costs 0.19 s Debug / 0.07 s
// Release. The honest test is
// therefore the empty-window solve at the requested size, with the wide-window
// boundary reported in the note rather than asserted here.
//
// THE BRIEF ALSO ASKED FOR A SEPARATE "Debug arm at n = 10^3", AND THIS TEST
// DELIBERATELY STANDS IN FOR IT -- a deviation, stated here rather than left
// silent. The brief's split assumed the n = 10^4 pin would be too slow for the
// per-commit Debug suite and would need a smaller Debug-only twin. It is not:
// on this arm the whole test costs 0.19 s Debug, so it is registered in the
// ordinary per-commit suite and runs in BOTH configurations. A second test at
// n = 10^3 would assert nothing this one does not -- the counters are
// size-independent here (1 major / 2 minors at every N measured), so the only
// thing it would add is a third size at which to observe the same numbers.
// The size-dependent quantity, the forward error, IS recorded at n = 10^3 in
// the note's section 4.1 table.
//
// WHAT IT ACTUALLY EXERCISES, so nobody over-reads a 2-minor solve: a 10^4-
// variable NLP with 6000 equality rows, 2000 inequality rows and 4000 bounded
// variables is assembled, its KKT matrix symbolically analysed and numerically
// factorized by Pardiso, one QP subproblem is solved to optimality, and the
// result is checked against the closed-form optimum. It does NOT exercise
// active-set identification (no path row is active at this p) -- that is the
// regime the cliff lives in, and it is deliberately not at this size.
//
// COUNTERS ARE PINNED EXACTLY, not bounded: unlike the banded-QP spike above
// (whose comment explains why its counters are inequalities), this solve's
// counts are structurally forced rather than tie-break-sensitive. The start
// point is feasible for every inequality and the problem is a convex QP with
// affine equalities once no path row can activate, so the driver takes exactly
// one major and its single QP takes exactly two minors, at ANY N. Measured on
// clang, MKL Pardiso, this machine, and identical at N = 2000/20000/200000.
TEST(F7ColdScaleSmoke, EmptyWindowColdSolveAtTenThousandVariables) {
    constexpr Index kNodes = 2000;
    constexpr double kP = 0.45;
    F7CollocationChain model(kNodes, 3, 2, kP, 1.0);
    ASSERT_EQ(model.n(), 10000);
    ASSERT_LT(kP, model.p_activation()); // the empty-window branch, by construction

    const SqpOptions opts = f7_smoke_options();
    SqpDriver driver(opts);
    Ledger ledger;
    driver.attach_ledger(&ledger, "f7_1e4_empty");

    const auto t0 = std::chrono::steady_clock::now();
    const SqpSolution sol = driver.solve(model, model.start_point());
    const auto t1 = std::chrono::steady_clock::now();

    ASSERT_NO_FATAL_FAILURE(
        check_against_manufactured_optimum(model, sol, kP, opts, /*x_tol=*/1e-8, "1e4_empty"));

    // No path row is active at the optimum -- the analytic statement the
    // regime split rests on, re-derived from the model rather than assumed.
    const AnalyticActiveSet analytic = model.active_set(kP);
    EXPECT_EQ(std::count(analytic.ineq_active.begin(), analytic.ineq_active.end(), 1), 0);

    EXPECT_EQ(sol.counters.major_iters, 1);
    EXPECT_EQ(sol.counters.qp_minor_iters, 2);
    EXPECT_EQ(sol.counters.factorizations, 1);
    EXPECT_EQ(sol.counters.steps_accepted, 1);
    EXPECT_EQ(sol.counters.rejected_steps, 0);
    EXPECT_EQ(sol.counters.elastic_activations, 0);
    EXPECT_EQ(sol.counters.restoration_iters, 0);

    RecordProperty(
        "wall_ms",
        fmt::format("{:.1f}", std::chrono::duration<double, std::milli>(t1 - t0).count()));
    RecordProperty("peak_rss_mib", fmt::format("{:.1f}", test_support::peak_rss_mib()));
    RecordProperty("x_forward_error_inf",
                   fmt::format("{:.3e}", (sol.x - model.x_star(kP)).lpNorm<Eigen::Infinity>()));
    RecordProperty("qp_ledger", ledger.summary_table());
}

// THE CLIFF'S MECHANISM, at a size the per-commit Debug suite can afford.
//
// The N = 200 wall-time cliff (tests/test_scale_problems.cpp's CARRY 2) is a
// QpOptions::schur_cap-triggered K0-REBUILD STORM, not a minor-iteration
// blow-up and not cycling: once the live border count exceeds schur_cap,
// qp_engine.h's border_solve_or_fall_back finds needs_refactorization() true on
// nearly every minor, and each one buys a K0 rebuild plus a full re-border of
// the surviving stack. The MEASURED signature at N = 150, p = 0.85, schur_cap
// 128 vs 512 (Release, clang, MKL Pardiso, this machine) is in the note; the
// load-bearing half of it is that the two runs took the IDENTICAL working-set
// path -- 1276 minors both times -- while factorizations went 853 vs 4,
// schur_updates 41005 vs 847, symbolic analyses 274 vs 3 and wall 87 s vs
// 9.6 s. Every one of those is a WHOLE-SOLVE count at MKL_NUM_THREADS=1; the
// note also publishes the same cell's 16-thread wall (97 s vs 10 s) and its
// per-subproblem symbolic pair (272 vs 1), which are different quantities and
// not disagreements -- see docs/notes/2026-07-30-scale-study-cold.md section 9.
// Cost, not answers.
//
// WHAT EXCEEDS THE CAP IS THE PIN COUNT, not the rows -- corrected by Phase-5
// Task 4 (docs/notes/2026-07-31-schur-cap-policy.md section 1), which measured
// the composition of the stack at every breach and found it 100% pins with
// zero row borders. It cannot be otherwise: a rebuild sets k0_rows =
// ws.active_ineq(), so the working rows are folded INTO K0 and the stack
// immediately after any rebuild is exactly the pins. On F7's wide window the
// pinned CONTROLS are Theta(N), which is what crosses 128 between N = 100
// (peak 126, no latch, 8 factorizations) and N = 125. That correction is what
// made the storm fixable: see LatchedBorderModeDoesNotThrashOnRowChanges in
// tests/test_qp_engine_border.cpp.
//
// This test reproduces exactly that signature at N = 30 (n = 150, ~1.9 s Debug
// per wide-window solve per test_scale_problems.cpp's own timings) by moving
// schur_cap DOWN to 8 instead of moving N up: the storm's trigger is
// "live borders > schur_cap", which is scale-free. It is the regression net
// for the diagnosis -- if a future change makes the small-cap run take a
// different number of minors, the "cost, not answers" claim is no longer true
// and the note's diagnosis needs revisiting.
//
// TRACED HERE (clang, MKL Pardiso, this machine; also emitted via
// RecordProperty so a CI reader does not have to trust this comment): 26 of
// the 30 path rows active at the optimum, minors 295 vs 295 -- EQUAL --
// and factorizations 239 vs 4, a 60x cost ratio for an identical answer.
// The whole test costs ~0.33 s Release / ~4.5 s Debug.
//
// PHASE-5 TASK 4 MOVED TWO OF THOSE TRACED NUMBERS and added a third
// assertion. The starved arm's cost used to be 333 factorizations, 2533
// border operations and 103 Pardiso symbolic analyses; it is now 239 / 95 /
// 10, because the latch no longer releases and re-takes on every working-ROW
// change (qp_engine.h's latch_still_holds, and
// docs/notes/2026-07-31-schur-cap-policy.md for the measurement). The minor
// counts, the answer and the ample arm are untouched -- which is exactly the
// "cost, not answers" claim this test exists to pin, now holding across an
// engine change as well as across a cap change.
//
// THE MINOR-COUNT EQUALITY IS AN EXACT PIN ON PURPOSE. It is not a
// tie-break-sensitive count being over-asserted: the claim is that two runs
// differing ONLY in a linear-algebra caching parameter visit the same working
// sets, which is qp_engine.h's own observational-equivalence contract. A
// one-iteration wobble here would be a real finding, not noise.
TEST(F7ColdScaleSmoke, SchurCapExhaustionBuysFactorizationsNotIterations) {
    constexpr Index kNodes = 30;
    constexpr double kP = 0.85;
    F7CollocationChain model(kNodes, 3, 2, kP, 1.0);
    ASSERT_EQ(model.n(), 150);
    ASSERT_GT(kP, model.p_activation()); // the wide-window branch

    // Enough of the 30 path rows are active for the live border stack to run
    // well past the starved cap below -- re-derived from the family, not
    // assumed, so a change to the analytic window cannot silently make this
    // test stop exercising the storm.
    const AnalyticActiveSet analytic = model.active_set(kP);
    const auto active_rows =
        std::count(analytic.ineq_active.begin(), analytic.ineq_active.end(), 1);
    ASSERT_GT(active_rows, 8 * 2) << "starved cap must be well inside the final active set";

    const auto solve_with_cap = [&](Index schur_cap) {
        SqpOptions opts = f7_smoke_options();
        opts.qp.max_iter = 5000; // the wide window needs it -- see the note
        opts.qp.schur_cap = schur_cap;
        SqpDriver driver(opts);
        return std::pair{driver.solve(model, model.start_point()), opts};
    };

    const auto [starved, starved_opts] = solve_with_cap(8);
    const auto [ample, ample_opts] = solve_with_cap(512);

    ASSERT_NO_FATAL_FAILURE(check_against_manufactured_optimum(model, starved, kP, starved_opts,
                                                               /*x_tol=*/1e-7, "schur_cap=8"));
    ASSERT_NO_FATAL_FAILURE(check_against_manufactured_optimum(model, ample, kP, ample_opts,
                                                               /*x_tol=*/1e-7, "schur_cap=512"));

    // (1) SAME ANSWER, to the last few bits.
    EXPECT_LT((starved.x - ample.x).lpNorm<Eigen::Infinity>(), 1e-9);

    // (2) SAME WORKING-SET PATH -- the claim that makes the cliff a cost
    // problem rather than an algorithmic one.
    EXPECT_EQ(starved.counters.major_iters, ample.counters.major_iters);
    EXPECT_EQ(starved.counters.qp_minor_iters, ample.counters.qp_minor_iters);

    // (3) THE STORM. Asserted as a RATIO rather than a pin on either count,
    // because the exact number of rebuilds depends on where in the minor
    // sequence the border count crosses the cap; the claim under test is that
    // starving the cap costs factorizations by an order of magnitude while
    // buying nothing. Measured at these settings: see RecordProperty below.
    EXPECT_GE(starved.counters.factorizations, 10 * ample.counters.factorizations);

    // (4) AND THE STORM IS BOUNDED, which is the Task-4 half of the same
    // claim. Every symbolic analysis in this solve is a K0 rebuild whose
    // sparsity pattern changed, so this counter is the direct measure of how
    // often border mode threw K0 away and started over. Starving the cap must
    // cost a HANDFUL of those, not one per minor: observed 10 here against the
    // ample arm's 3, and against 103 on the pre-Task-4 engine, which released
    // the pins-only latch on every working-row change. The bound is the
    // signature (a small constant, unrelated to the 295 minors), not the
    // observed value.
    //
    // THE ANTI-VACUITY GUARD BELOW IS NOT DECORATION, and it is why this pin
    // needs no #ifdef USE_ACCELERATE_SPARSE arm. An UPPER bound on a counter
    // fails OPEN: if some backend left symbolic_analyses structurally 0, the
    // bound would pass for the wrong reason and the coverage would vanish
    // without a signal. The lower bound on the AMPLE arm closes that hole on
    // every backend at once -- a solve that builds K0 at all must analyse a
    // pattern at least once, so a structurally-dead counter fails here loudly
    // instead of silently satisfying the bound above.
    //
    // THE COUNTER IS BACKEND-PORTABLE BY CONSTRUCTION, which is the reason a
    // guard suffices where a per-backend arm would otherwise be owed:
    // QpCounters::symbolic_analyses is bumped at the qp_engine.h CALL SITE, on
    // `!border.kkt.pattern_matches(border.k0.K)` (see its note in types.h), and
    // KktSystem::pattern_matches is implemented by BOTH shipped backends
    // (kkt_system.h and kkt_system_accelerate.h). So it counts K0 pattern
    // changes, not Pardiso phase-11 calls, and it cannot be structurally zero
    // on Accelerate. What is NOT verified on Accelerate is the VALUE: 10 and 3
    // are MKL-measured on this machine, and a different trajectory there would
    // move them. That is what the 2.5x headroom in the bound is for, and it is
    // audit item (g) of docs/notes/2026-07-28-accelerate-audit-checklist.md.
    EXPECT_GE(ample.counters.symbolic_analyses, 1)
        << "symbolic_analyses is dead on this backend, which would make the "
           "bound below vacuous rather than passing";
    EXPECT_LE(starved.counters.symbolic_analyses, 25)
        << "a starved schur_cap must not buy one K0 re-analysis per minor";

    RecordProperty("active_path_rows", fmt::format("{}", active_rows));
    RecordProperty(
        "minors_starved_vs_ample",
        fmt::format("{} vs {}", starved.counters.qp_minor_iters, ample.counters.qp_minor_iters));
    RecordProperty(
        "factorizations_starved_vs_ample",
        fmt::format("{} vs {}", starved.counters.factorizations, ample.counters.factorizations));
    RecordProperty("symbolic_analyses_starved_vs_ample",
                   fmt::format("{} vs {}", starved.counters.symbolic_analyses,
                               ample.counters.symbolic_analyses));
}

// n = 10^6 (N = 200000), cold, empty window -- THE STUDY'S CEILING DATUM,
// registered in the ScaleF7Slow suite so tests/CMakeLists.txt's TEST_FILTER
// keeps it out of the per-commit ctest run (see that file's note for the
// mechanism and the obligation that comes with it: anything in this suite must
// still be run in the phase-gate Debug sweep).
//
// PER THE BRIEF'S "Release-only arm for anything bigger" -- the repo's actual
// mechanism for that is this suite, not an NDEBUG guard, because a #ifdef'd
// test is invisible rather than merely deferred. Measured: 7 s / 1.3 GiB
// Release, 24 s / 1.3 GiB Debug -- affordable in the phase-gate Debug sweep the
// suite already owes, and it is the largest n this study reached cold at ANY p.
//
// x_tol = 1e-4 here against a MEASURED 2.3e-6 -- the two-orders-of-headroom
// rule check_against_manufactured_optimum's own note derives from the N^2
// conditioning law. Do not tighten it to the measured value: that would turn a
// documented property of the transcription into a machine-specific pin.
TEST(ScaleF7Slow, EmptyWindowColdSolveAtOneMillionVariables) {
    constexpr Index kNodes = 200000;
    constexpr double kP = 0.45;
    F7CollocationChain model(kNodes, 3, 2, kP, 1.0);
    ASSERT_EQ(model.n(), 1000000);

    ASSERT_LT(kP, model.p_activation()); // the empty-window branch, by construction

    const SqpOptions opts = f7_smoke_options();
    SqpDriver driver(opts);
    Ledger ledger;
    driver.attach_ledger(&ledger, "f7_1e6_empty");

    const auto t0 = std::chrono::steady_clock::now();
    const SqpSolution sol = driver.solve(model, model.start_point());
    const auto t1 = std::chrono::steady_clock::now();

    ASSERT_NO_FATAL_FAILURE(
        check_against_manufactured_optimum(model, sol, kP, opts, /*x_tol=*/1e-4, "1e6_empty"));

    // Re-derived from the family rather than assumed, exactly as in the n = 10^4
    // sibling: no path row is active at the optimum, which is the property the
    // structurally-forced counter pins below rest on.
    const AnalyticActiveSet analytic = model.active_set(kP);
    EXPECT_EQ(std::count(analytic.ineq_active.begin(), analytic.ineq_active.end(), 1), 0);

    // The same structurally-forced counters as the n = 10^4 pin above -- which
    // is the point: on this arm the iteration count is SIZE-INDEPENDENT, so
    // everything the decade costs is linear algebra.
    EXPECT_EQ(sol.counters.major_iters, 1);
    EXPECT_EQ(sol.counters.qp_minor_iters, 2);
    EXPECT_EQ(sol.counters.factorizations, 1);

    RecordProperty(
        "wall_ms",
        fmt::format("{:.1f}", std::chrono::duration<double, std::milli>(t1 - t0).count()));
    RecordProperty("peak_rss_mib", fmt::format("{:.1f}", test_support::peak_rss_mib()));
    RecordProperty("x_forward_error_inf",
                   fmt::format("{:.3e}", (sol.x - model.x_star(kP)).lpNorm<Eigen::Infinity>()));
    RecordProperty("qp_ledger", ledger.summary_table());
}
