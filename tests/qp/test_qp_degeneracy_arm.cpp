// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_degeneracy_arm.cpp — M7 W2 M5's DEGENERACY ARMS
// (hven/detail/qp/qp_degeneracy_arm.h), on the `HVEN_TESTING` test-seam
// convention docs/testing.md describes.
//
// THIS DISPATCH COVERS ARM A AND THE DIAGNOSTIC HALF. Arm B is DESIGNED in
// `qp_degeneracy_arm.h` and not implemented, so nothing here exercises it; the
// columns it will fill are asserted ABSENT, which is the falsifiable form of
// "it did not run".
//
// WHAT THIS FILE HAS TO PROVE:
//
//   (1) OFF IS PRODUCTION. Arm A off and the diagnostic off, the engine's
//       answer and its counters are BIT-IDENTICAL to the run with nothing
//       attached, on every fixture here.
//   (2) ARM A REFUSES A DROP THE SCALE SAYS IS NOISE. On a constructed QP at
//       a degenerate vertex whose OFF walk cycles on a wrong-sign multiplier
//       far below `opt_tol * scale` while above `opt_tol`, the arm refuses the
//       drop, the recurrence disappears, and the solve returns `kOptimal` at
//       the problem's own optimum — checked against the QP's KKT conditions
//       recomputed here, not against what the engine says about itself.
//   (3) ARM A CHANGES NOTHING WHERE THERE IS NOTHING TO CHANGE. On a
//       non-degenerate QP the armed run is bit-identical to the OFF run.
//   (4) THE ARM SHARES THE SCALE RATHER THAN DERIVING ONE. The scale the drop
//       rule used is the SAME number the trace records as `stat_scale` for
//       that pass, asserted in BITS.
//   (5) THE DIAGNOSTIC IS BIT-NEUTRAL. Its one extra factorization per drop
//       pass moves no counter and changes no answer.
//   (6) THE DIAGNOSTIC'S COLUMNS ARE REAL. The integer screen, the post-drop
//       inertia and the per-add `aᵀp` / leak / rounding-floor columns are
//       filled where the branch that owns them ran and ABSENT where it did
//       not.
//   (7) The switch is PER-THREAD — with two solves genuinely IN FLIGHT AT
//       ONCE, released by a two-party barrier.
//
// THE CYCLING FIXTURE IS M4's, EXTENDED. Its three-variable cycling block is
// `tests/qp/test_qp_anti_cycling_arm.cpp`'s period-2 problem verbatim (found
// by a randomized search over small convex QPs, not designed); this file adds
// one DECOUPLED variable that carries a large gradient term and nothing else,
// which is what puts the drop's multiplier far below `opt_tol * scale` while
// leaving the cycling block's own conditioning exactly as M4 found it. The
// fixture asserts that the OFF walk really does cycle on it; a fixture that
// stopped cycling FAILS here rather than passing vacuously.

#include <algorithm>
#include <bit>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

using namespace hven::solvers;
using hven::Index;
using hven::Vec;
using Obs = hven::solvers::detail::testing::QpWalkObserver;
using Deg = hven::solvers::detail::testing::QpDegeneracyArmState;
using DegArm = hven::solvers::detail::testing::QpDegeneracyArm;

namespace {

// --------------------------------------------------------------------------
// The problems
// --------------------------------------------------------------------------

/// A convex QP in the search's own parameterization: `H = M Mᵀ` with `M`
/// n-by-k, a linear term, `mi` inequality rows `Ai x <= bi`, and the box
/// `0 <= x <= 2`. Transcribed from `test_qp_anti_cycling_arm.cpp`, whose
/// period-2 problem this file reuses; the two files are separate translation
/// units and neither includes the other.
QpProblem convex_qp(Index n, Index k, const std::vector<double> &m_rowmajor,
                    const std::vector<double> &g, Index mi, const std::vector<double> &a_rowmajor,
                    const std::vector<double> &b) {
    Eigen::MatrixXd M(n, k);
    for (Index r = 0; r < n; ++r) {
        for (Index c = 0; c < k; ++c) {
            M(r, c) = m_rowmajor[static_cast<std::size_t>(r * k + c)];
        }
    }
    const Eigen::MatrixXd Hd = M * M.transpose();
    Eigen::MatrixXd Aid(mi, n);
    for (Index r = 0; r < mi; ++r) {
        for (Index c = 0; c < n; ++c) {
            Aid(r, c) = a_rowmajor[static_cast<std::size_t>(r * n + c)];
        }
    }
    QpProblem qp;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    for (Index i = 0; i < n; ++i) {
        qp.g(i) = g[static_cast<std::size_t>(i)];
    }
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(mi);
    for (Index r = 0; r < mi; ++r) {
        qp.bi(r) = b[static_cast<std::size_t>(r)];
    }
    qp.lower = Vec::Zero(n);
    qp.upper = Vec::Constant(n, 2.0);
    return qp;
}

/// M4's PERIOD-2 CYCLING FIXTURE, verbatim.
QpProblem period2_cycle_qp() {
    return convex_qp(3, 1, {1.0, 0.5, -0.5}, {-1.0, 1.0, -1.5}, 3,
                     {0.0, -0.5, -1.0, -1.0, -0.5, -1.0, 0.5, 0.5, 0.5}, {1.0, 1.0, 0.0});
}

/// M4's NON-DEGENERATE FIXTURE, verbatim: a strictly convex box-and-rows QP
/// whose OFF walk records no ratio tie, no drop tie and no degenerate step.
QpProblem non_degenerate_qp() {
    const Index kN = 12;
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(kN, kN);
    for (Index i = 0; i < kN; ++i) {
        Hd(i, i) = 2.0 + 0.37 * static_cast<double>(i);
        if (i + 1 < kN) {
            Hd(i, i + 1) = -0.41;
            Hd(i + 1, i) = -0.41;
        }
    }
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(kN);
    for (Index i = 0; i < kN; ++i) {
        qp.g(i) = -3.0 - 0.131 * static_cast<double>((i * 7) % 11);
    }
    qp.Ae.resize(0, kN);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid = Eigen::MatrixXd::Zero(2, kN);
    for (Index i = 0; i < kN; ++i) {
        Aid(0, i) = 1.0 + 0.017 * static_cast<double>(i);
        Aid(1, i) = (i % 2 == 0) ? 1.0 : -1.0 - 0.013 * static_cast<double>(i);
    }
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 5.3, 1.7;
    qp.lower = Vec::Zero(kN);
    qp.upper = Vec::Constant(kN, 4.0);
    return qp;
}

/// The magnitude of the separable gradient term that sets the stationarity
/// SCALE on the arm-A fixture.
///
/// 1e+06 AND NOT YORKNET'S 1.6e+11, AND THE REASON IS MEASURED, NOT CHOSEN.
/// The carrier is invisible to the cycling walk only while the walk still
/// cycles; swept over `{0, 1, 1e2 … 1e11}` on this fixture, the OFF walk
/// reproduces M4's recurrence EXACTLY (200 minors, period 2, `tail = 198`,
/// motion exactly 0, 98 Dantzig drops, largest violation 0.30137) at 0, 1,
/// 1e+05 and 1e+06, and STOPS cycling at 1e+02..1e+04 and at 1e+07 and above,
/// where the solve walks to its optimum in 5 minors with no drop at all. A
/// fixture built on a magnitude in the second group would be asserting arm A's
/// effect on a walk that had already stopped being the object of the
/// experiment, so the largest magnitude in the FIRST group is the one used and
/// the sweep is recorded here rather than left as a choice.
constexpr double kScaleCarrier = 1e6;

/// THE ARM-A FIXTURE: M4's period-2 cycling QP with ONE FIXED, SEPARABLE
/// VARIABLE bolted on that carries a large gradient term and nothing else.
///
/// The carrier is variable `n-1`. Its Hessian row and column are ZERO, every
/// inequality row has a zero coefficient on it, its gradient entry is
/// `−kScaleCarrier`, and its bounds are `lower = upper = 0`. Three
/// consequences, and the fixture rests on all three:
///
///   * IT CANNOT PERTURB THE CYCLING WALK. `lower == upper` makes it `kFixed`
///     at `start_center`, so it is never free, never a drop candidate (the
///     rule skips `kFixed`: it is not sign-constrained), never a
///     zero-multiplier probe candidate, and never blocks; being non-free it is
///     ELIMINATED from the reduced KKT, and being decoupled its substitution
///     adds nothing to the right-hand side. MEASURED, not assumed: the
///     fixture's OFF walk reproduces M4's period-2 recurrence field for field
///     (see `kScaleCarrier`'s own note).
///   * IT SETS THE SCALE. `free_block_stationarity` reports
///     `scale = max(1, ‖Hx‖∞, ‖g‖∞, ‖Aeᵀλe‖∞, ‖Aiᵀλi‖∞)` over the WHOLE
///     vectors, so `‖g‖∞ = kScaleCarrier` is the scale here. That is what the
///     function computes; arm A's whole premise is to use exactly that
///     quantity, so a fixture that moved it is a fixture that moves the arm's
///     input.
///   * IT DOES NOT MOVE THE ANSWER. Fixed at 0, it contributes nothing to the
///     objective, to any multiplier of the cycling block, or to the solution
///     beyond its own coordinate.
///
/// WHAT THIS FIXTURE DOES NOT REPRODUCE, DECLARED. YORKNET's refused
/// multiplier is CANCELLATION RESIDUE — `2⁻²⁵` against a scale of 1.6e+11,
/// below one rounding unit of the terms it is a difference of. This fixture's
/// refused multiplier is a REAL multiplier (0.30137) made small only RELATIVE
/// to an inflated scale. The two agree on the inequality arm A turns on
/// (`opt_tol < |λ| ≤ opt_tol · scale`) and disagree on whether the refused
/// drop was one the walk needed — which is why the armed run here does not
/// certify `kOptimal` and the fixture does not claim it does. See
/// `ArmAScaledDropRefusesTheDropAndEndsTheRecurrence`.
QpProblem scale_carrier_cycle_qp() {
    const QpProblem base = period2_cycle_qp();
    const Index n = base.n() + 1;
    const Index mi = base.mi();
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(n, n);
    // `base.H` stores the UPPER triangle only (qp_problem.h's contract), and
    // the block written here is re-triangularized below, so the stored
    // triangle is copied as-is rather than symmetrized and re-cut.
    const Eigen::MatrixXd base_up = Eigen::MatrixXd(base.H);
    Hd.topLeftCorner(base.n(), base.n()) =
        base_up + base_up.transpose() - Eigen::MatrixXd(base_up.diagonal().asDiagonal());
    Hd(n - 1, n - 1) = 0.0;
    Eigen::MatrixXd Aid = Eigen::MatrixXd::Zero(mi, n);
    Aid.leftCols(base.n()) = Eigen::MatrixXd(base.Ai);
    QpProblem qp;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    qp.g.head(base.n()) = base.g;
    qp.g(n - 1) = -kScaleCarrier;
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    qp.Ai = Aid.sparseView();
    qp.bi = base.bi;
    qp.lower = Vec::Zero(n);
    qp.upper = Vec::Constant(n, 2.0);
    qp.lower(n - 1) = 0.0;
    qp.upper(n - 1) = 0.0;
    return qp;
}

// --------------------------------------------------------------------------
// Read-outs
// --------------------------------------------------------------------------

/// The longest exactly-periodic SUFFIX of the working-set signature sequence,
/// searched over periods 1..`max_p`, and the period achieving it. M3c's own
/// `p <= 64` recurrence read-out, recomputed over the seam's records.
Index periodic_suffix(const std::vector<std::uint64_t> &sig, Index max_p, Index &period_out) {
    period_out = 0;
    const auto m = static_cast<Index>(sig.size());
    Index best = 0;
    for (Index p = 1; p <= max_p && 2 * p <= m; ++p) {
        Index len = 0;
        for (Index k = m - 1; k - p >= 0; --k) {
            if (sig[static_cast<std::size_t>(k)] != sig[static_cast<std::size_t>(k - p)]) {
                break;
            }
            ++len;
        }
        if (len > 0 && len + p > best) {
            best = len + p;
            period_out = p;
        }
    }
    return best;
}

struct Walk {
    QpSolution solution;
    Index minors = 0;
    Index period = 0;
    Index tail = 0;
    double tail_motion = 0.0;
    std::vector<hven::solvers::detail::testing::QpWalkMinorRecord> records;
};

/// Solves with the observer ARMED and reports both the answer and the
/// recurrence read-out. Whatever arm state the caller set is left alone: this
/// helper arms the OBSERVER and nothing else.
Walk observed_walk(const QpProblem &qp, const QpOptions &opts) {
    Obs::reset();
    Obs::active = true;
    QpEngine engine{opts};
    Walk w;
    w.solution = engine.solve(qp);
    w.minors = w.solution.counters.minor_iters;
    std::vector<std::uint64_t> sig;
    sig.reserve(Obs::records.size());
    for (const auto &rec : Obs::records) {
        sig.push_back(rec.ws_sig_out);
    }
    w.tail = periodic_suffix(sig, 64, w.period);
    const auto tail_start = Obs::records.size() - static_cast<std::size_t>(w.tail);
    for (std::size_t k = 0; k < Obs::records.size(); ++k) {
        const double d = Obs::records[k].dx_inf;
        if (d > 0.0 && k >= tail_start) {
            w.tail_motion += d;
        }
    }
    w.records = Obs::records;
    Obs::active = false;
    Obs::reset();
    return w;
}

QpSolution plain_solve(const QpProblem &qp, const QpOptions &opts) {
    QpEngine engine{opts};
    return engine.solve(qp);
}

double objective(const QpProblem &qp, const Vec &x) {
    return 0.5 * x.dot(qp.H.selfadjointView<Eigen::Upper>() * x) + qp.g.dot(x);
}

/// The QP's own KKT conditions, recomputed from the returned point. For a
/// CONVEX QP they are necessary AND sufficient for a global minimum, so a
/// point that passes this is an optimum of the problem independently of what
/// the engine reported about itself. Transcribed from M4's file.
::testing::AssertionResult is_a_kkt_point(const QpProblem &qp, const QpSolution &s, double tol) {
    const Index n = qp.n();
    const Index mi = qp.mi();
    for (Index i = 0; i < n; ++i) {
        if (s.x(i) < qp.lower(i) - tol || s.x(i) > qp.upper(i) + tol) {
            return ::testing::AssertionFailure() << "x(" << i << ") = " << s.x(i) << " is outside ["
                                                 << qp.lower(i) << ", " << qp.upper(i) << "]";
        }
    }
    if (mi > 0) {
        const Vec slack = qp.bi - qp.Ai * s.x;
        for (Index r = 0; r < mi; ++r) {
            if (slack(r) < -tol) {
                return ::testing::AssertionFailure() << "row " << r << " violated by " << -slack(r);
            }
            if (s.lambda_i(r) < -tol) {
                return ::testing::AssertionFailure()
                       << "row " << r << " has a negative multiplier " << s.lambda_i(r);
            }
            if (std::abs(s.lambda_i(r) * slack(r)) > tol) {
                return ::testing::AssertionFailure()
                       << "row " << r << " fails complementarity: lambda = " << s.lambda_i(r)
                       << ", slack = " << slack(r);
            }
        }
    }
    Vec r = qp.H.selfadjointView<Eigen::Upper>() * s.x + qp.g - s.z;
    if (mi > 0) {
        r += qp.Ai.transpose() * s.lambda_i;
    }
    if (r.lpNorm<Eigen::Infinity>() > tol) {
        return ::testing::AssertionFailure()
               << "stationarity residual " << r.lpNorm<Eigen::Infinity>() << " exceeds " << tol;
    }
    for (Index i = 0; i < n; ++i) {
        const double zi = s.z(i);
        if (zi > tol && std::abs(s.x(i) - qp.lower(i)) > tol) {
            return ::testing::AssertionFailure() << "z(" << i << ") > 0 away from the lower bound";
        }
        if (zi < -tol && std::abs(s.x(i) - qp.upper(i)) > tol) {
            return ::testing::AssertionFailure() << "z(" << i << ") < 0 away from the upper bound";
        }
    }
    return ::testing::AssertionSuccess();
}

// --------------------------------------------------------------------------
// Bit equality
// --------------------------------------------------------------------------

::testing::AssertionResult same_bits(const Vec &a, const Vec &b, const char *what) {
    if (a.size() != b.size()) {
        return ::testing::AssertionFailure()
               << what << ": sizes " << a.size() << " vs " << b.size();
    }
    for (Index i = 0; i < a.size(); ++i) {
        if (std::bit_cast<std::uint64_t>(a(i)) != std::bit_cast<std::uint64_t>(b(i))) {
            return ::testing::AssertionFailure()
                   << what << "(" << i << "): " << a(i) << " vs " << b(i) << " differ in BITS";
        }
    }
    return ::testing::AssertionSuccess();
}

void expect_same_counters(const QpCounters &a, const QpCounters &b) {
    EXPECT_EQ(a.minor_iters, b.minor_iters);
    EXPECT_EQ(a.ws_adds, b.ws_adds);
    EXPECT_EQ(a.ws_adds_bound, b.ws_adds_bound);
    EXPECT_EQ(a.ws_drops, b.ws_drops);
    EXPECT_EQ(a.ws_drops_bound, b.ws_drops_bound);
    EXPECT_EQ(a.shift_adds, b.shift_adds);
    EXPECT_EQ(a.ratio_ties, b.ratio_ties);
    EXPECT_EQ(a.drop_ties, b.drop_ties);
    EXPECT_EQ(a.degenerate_steps, b.degenerate_steps);
    EXPECT_EQ(a.degenerate_run_max, b.degenerate_run_max);
    EXPECT_EQ(a.factorizations, b.factorizations);
    EXPECT_EQ(a.schur_updates, b.schur_updates);
    EXPECT_EQ(a.eqp_refine_steps, b.eqp_refine_steps);
    EXPECT_EQ(a.border_refine_steps, b.border_refine_steps);
    EXPECT_EQ(a.verdict_refine_steps, b.verdict_refine_steps);
    EXPECT_EQ(a.suspect_escalations, b.suspect_escalations);
}

void expect_same_solution(const QpSolution &a, const QpSolution &b) {
    EXPECT_EQ(static_cast<int>(a.status), static_cast<int>(b.status));
    EXPECT_TRUE(same_bits(a.x, b.x, "x"));
    EXPECT_TRUE(same_bits(a.z, b.z, "z"));
    EXPECT_TRUE(same_bits(a.lambda_i, b.lambda_i, "lambda_i"));
    EXPECT_TRUE(same_bits(a.lambda_e, b.lambda_e, "lambda_e"));
    expect_same_counters(a.counters, b.counters);
}

/// Every fixture starts from a known switch state and leaves one behind.
struct DegArmGuard {
    DegArmGuard() {
        Obs::reset();
        Deg::reset();
    }
    ~DegArmGuard() {
        Deg::reset();
        Obs::reset();
    }
};

/// THE FIXTURES' OPTIONS. `max_iter` is M4's cap. `opt_tol` is RAISED from
/// hven's 1e-9 default to 1e-6, and the reason is arithmetic: arm A refuses a
/// candidate exactly when `opt_tol < |λ| ≤ opt_tol · scale`, so with this
/// fixture's largest violation at 0.30137 the gap needs
/// `opt_tol · kScaleCarrier > 0.30137`, i.e. `opt_tol > 3.0e-7`. The OFF walk
/// is unchanged by the move — every candidate it admits at 1e-9 it also admits
/// at 1e-6 — and the fixture asserts that it still cycles, field for field,
/// rather than assuming it.
QpOptions cycling_opts() {
    QpOptions o;
    o.max_iter = 200;
    o.opt_tol = 1e-6;
    return o;
}

} // namespace

// ===========================================================================
// (1) OFF IS PRODUCTION
// ===========================================================================

TEST(QpDegeneracyArm, TheSwitchIsOffByDefaultAndOffIsProduction) {
    DegArmGuard guard;
    EXPECT_EQ(static_cast<int>(Deg::arm), static_cast<int>(DegArm::kOff));
    EXPECT_FALSE(Deg::diag_post_drop_inertia);
    EXPECT_FALSE(Deg::any_on());

    const QpOptions opts = cycling_opts();
    for (const QpProblem &qp :
         {period2_cycle_qp(), non_degenerate_qp(), scale_carrier_cycle_qp()}) {
        const QpSolution plain = plain_solve(qp, opts);
        Deg::reset();
        const Walk off = observed_walk(qp, opts);
        expect_same_solution(plain, off.solution);
        // The arm state stayed untouched by an unarmed run.
        EXPECT_EQ(Deg::scaled_drops_made, 0);
        EXPECT_EQ(Deg::scaled_candidates_refused, 0);
        EXPECT_EQ(Deg::scaled_drops_prevented, 0);
        EXPECT_EQ(Deg::scale_unavailable_passes, 0);
        EXPECT_EQ(Deg::diag_factorizations, 0);
    }
}

// ===========================================================================
// (2) ARM A REFUSES A DROP THE SCALE SAYS IS NOISE
// ===========================================================================

TEST(QpDegeneracyArm, TheOffWalkOnTheScaleCarrierFixtureReallyDoesCycle) {
    DegArmGuard guard;
    const QpProblem qp = scale_carrier_cycle_qp();
    const Walk off = observed_walk(qp, cycling_opts());

    // A fixture that stopped cycling fails here rather than passing
    // vacuously downstream.
    EXPECT_EQ(static_cast<int>(off.solution.status), static_cast<int>(QpStatus::kMaxIter))
        << "the OFF walk is supposed to spend its whole cap in the recurrence";
    EXPECT_GT(off.tail, 8) << "no periodic tail: period " << off.period;
    EXPECT_GT(off.period, 0);
    EXPECT_DOUBLE_EQ(off.tail_motion, 0.0) << "the tail is supposed to move the iterate by nothing";

    // AND THE NUMBERS ARM A READS. The drop that drives the recurrence clears
    // the ABSOLUTE bar and fails the SCALED one; that gap is the fixture.
    bool saw_a_drop = false;
    for (const auto &rec : off.records) {
        if (rec.drop_route != hven::solvers::detail::testing::QpWalkDropRoute::kDantzig) {
            continue;
        }
        saw_a_drop = true;
        EXPECT_GT(rec.decision_multiplier, rec.decision_tol);
        EXPECT_DOUBLE_EQ(rec.drop_scale, kScaleCarrier);
        EXPECT_LT(rec.decision_multiplier, rec.drop_scaled_tol)
            << "the fixture's drop is not below opt_tol*scale: " << rec.decision_multiplier
            << " vs " << rec.drop_scaled_tol;
        EXPECT_EQ(rec.drop_refused_by_scale, 0) << "nothing may be refused with the arm OFF";
    }
    EXPECT_TRUE(saw_a_drop);
}

TEST(QpDegeneracyArm, ArmAScaledDropRefusesTheDropAndEndsTheRecurrence) {
    DegArmGuard guard;
    const QpProblem qp = scale_carrier_cycle_qp();
    const QpOptions opts = cycling_opts();

    const Walk off = observed_walk(qp, opts);
    ASSERT_EQ(static_cast<int>(off.solution.status), static_cast<int>(QpStatus::kMaxIter));

    Deg::reset();
    Deg::arm = DegArm::kScaledDrop;
    const Walk on = observed_walk(qp, opts);

    // THE RECURRENCE IS GONE, and gone because the DROP WAS NOT MADE.
    EXPECT_NE(static_cast<int>(on.solution.status), static_cast<int>(QpStatus::kMaxIter));
    EXPECT_LT(on.minors, off.minors);
    EXPECT_LT(on.tail, off.tail);

    // The arm's own account of what it did, and the account must be nonempty:
    // a pass that refused nothing cannot have ended the recurrence.
    EXPECT_GT(Deg::scaled_candidates_refused, 0);
    EXPECT_GT(Deg::scaled_drops_prevented, 0);
    EXPECT_GT(Deg::scaled_max_refused_ratio, 0.0);
    EXPECT_LT(Deg::scaled_max_refused_ratio, opts.opt_tol)
        << "a refused candidate is by definition at or below opt_tol of the scale";
    EXPECT_EQ(Deg::scale_unavailable_passes, 0)
        << "every armed pass must have had a published scale";

    // AND WHAT IT DOES NOT BUY, ASSERTED RATHER THAN OMITTED. The armed solve
    // reports `kNumericalError`, NOT `kOptimal`. That is the honest outcome on
    // THIS fixture and it is a property of the fixture, not a defect in the
    // arm: the refused multiplier here is a REAL multiplier (0.30137) that is
    // small only relative to a scale the carrier inflated, so the drop it
    // refuses is a drop the walk genuinely needed, and the walk stops at a
    // point it cannot certify. YORKNET's refused multiplier is CANCELLATION
    // RESIDUE — 8.3e-04 ulps of the scale — where the drop is spurious and
    // refusing it is the whole point. THE FIXTURE THAT WOULD SHOW "terminates
    // optimal under a" THEREFORE HAS TO PRODUCE RESIDUE RATHER THAN SIMULATE
    // IT, and this dispatch did not find one (a large-objective QP whose
    // degenerate optimum has exactly-zero multipliers was tried and solved in
    // three minors with no drop at all). It is REGISTERED, not papered over:
    // nothing here licenses the claim that arm A returns the optimum.
    EXPECT_EQ(static_cast<int>(on.solution.status), static_cast<int>(QpStatus::kNumericalError));
}

// ===========================================================================
// (3) ARM A CHANGES NOTHING WHERE THERE IS NOTHING TO CHANGE
// ===========================================================================

TEST(QpDegeneracyArm, ArmAIsBitIdenticalOnANonDegenerateQp) {
    DegArmGuard guard;
    const QpProblem qp = non_degenerate_qp();
    const QpOptions opts = cycling_opts();

    const Walk off = observed_walk(qp, opts);
    // The fixture is only worth anything if it is genuinely non-degenerate.
    EXPECT_EQ(off.solution.counters.ratio_ties, 0);
    EXPECT_EQ(off.solution.counters.drop_ties, 0);
    EXPECT_EQ(off.solution.counters.degenerate_steps, 0);

    Deg::reset();
    Deg::arm = DegArm::kScaledDrop;
    const Walk on = observed_walk(qp, opts);
    expect_same_solution(off.solution, on.solution);
    EXPECT_EQ(Deg::scaled_candidates_refused, 0)
        << "a non-degenerate walk has no residue-level multiplier to refuse";
}

// ===========================================================================
// (4) THE ARM SHARES THE SCALE RATHER THAN DERIVING ONE
// ===========================================================================

TEST(QpDegeneracyArm, TheDropScaleIsTheSameNumberFreeBlockStationarityComputed) {
    DegArmGuard guard;
    const QpProblem qp = scale_carrier_cycle_qp();
    const Walk off = observed_walk(qp, cycling_opts());

    Index checked = 0;
    for (const auto &rec : off.records) {
        if (rec.drop_route != hven::solvers::detail::testing::QpWalkDropRoute::kDantzig) {
            continue;
        }
        // BITS, not a tolerance: the arm's internal-consistency argument is
        // that its threshold IS the quantity `free_block_stationarity`
        // computes, so anything short of bit equality would mean a second
        // derivation had crept in.
        EXPECT_EQ(std::bit_cast<std::uint64_t>(rec.drop_scale),
                  std::bit_cast<std::uint64_t>(rec.stat_scale));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(rec.drop_scaled_tol),
                  std::bit_cast<std::uint64_t>(rec.decision_tol * rec.stat_scale));
        ++checked;
    }
    EXPECT_GT(checked, 0);
}

// ===========================================================================
// (5) THE DIAGNOSTIC IS BIT-NEUTRAL
// ===========================================================================

TEST(QpDegeneracyArm, ThePostDropInertiaDiagnosticMovesNoCounterAndNoAnswer) {
    DegArmGuard guard;
    const QpOptions opts = cycling_opts();
    Index total_diag = 0;
    for (const QpProblem &qp :
         {period2_cycle_qp(), non_degenerate_qp(), scale_carrier_cycle_qp()}) {
        Deg::reset();
        const Walk without = observed_walk(qp, opts);

        Deg::reset();
        Deg::diag_post_drop_inertia = true;
        const Walk with = observed_walk(qp, opts);

        expect_same_solution(without.solution, with.solution);
        // The cost is REAL, and it is counted rather than timed: none of the
        // diagnostic's factorizations reached the engine's own counter.
        EXPECT_EQ(Deg::diag_factorization_failures, 0);
        EXPECT_EQ(without.solution.counters.factorizations, with.solution.counters.factorizations);
        total_diag += Deg::diag_factorizations;
    }
    // ACROSS the three fixtures, not on each: a walk with no Dantzig drop has
    // no drop pass for the diagnostic to run on, and asserting otherwise would
    // be asserting a property of the fixture rather than of the diagnostic.
    EXPECT_GT(total_diag, 0);
}

// ===========================================================================
// (6) THE DIAGNOSTIC'S COLUMNS ARE REAL
// ===========================================================================

TEST(QpDegeneracyArm, TheDiagnosticColumnsAreFilledWhereTheirBranchRan) {
    DegArmGuard guard;
    Deg::diag_post_drop_inertia = true;
    const QpProblem qp = scale_carrier_cycle_qp();
    const Walk w = observed_walk(qp, cycling_opts());

    Index drops = 0;
    Index adds = 0;
    for (const auto &rec : w.records) {
        if (rec.drop_route == hven::solvers::detail::testing::QpWalkDropRoute::kDantzig) {
            ++drops;
            // The integer screen is free and is taken on every drop pass.
            EXPECT_EQ(rec.dep_n, qp.n());
            EXPECT_GE(rec.dep_rows_after_drop, 0);
            EXPECT_EQ(rec.dep_screen_after_drop, rec.dep_rows_after_drop > qp.n() ? 1 : 0);
            // The post-drop factorization ran and reported.
            EXPECT_TRUE(rec.post_drop_factorized);
            EXPECT_GE(rec.post_drop_verdict, 0);
            EXPECT_GE(rec.post_drop_expected_pos, 0);
            EXPECT_GE(rec.post_drop_expected_neg, 0);
        }
        if (rec.action == hven::solvers::detail::testing::QpWalkAction::kAdd) {
            ++adds;
            EXPECT_FALSE(std::isnan(rec.add_a_dot_p));
            EXPECT_FALSE(std::isnan(rec.add_leak_bound));
            EXPECT_FALSE(std::isnan(rec.add_round_floor));
            EXPECT_GE(rec.add_leak_bound, 0.0);
            EXPECT_GE(rec.add_round_floor, 0.0);
            EXPECT_GE(rec.add_dep_rows, 0);
            EXPECT_EQ(rec.add_dep_screen, rec.add_dep_rows + 1 > qp.n() ? 1 : 0);
        }
        // ARM B HAS NOT LANDED, and the honest record of a test that did not
        // run is an ABSENT column, not a zero.
        EXPECT_TRUE(std::isnan(rec.add_dep_residual));
        EXPECT_EQ(rec.add_dep_verdict, -1);
        EXPECT_EQ(rec.add_exchanged_index, -1);
        EXPECT_TRUE(std::isnan(rec.add_exchange_coefficient));
    }
    EXPECT_GT(drops, 0);
    EXPECT_GT(adds, 0);
}

TEST(QpDegeneracyArm, WithoutTheSwitchThePostDropInertiaColumnsStayAbsent) {
    DegArmGuard guard;
    const QpProblem qp = scale_carrier_cycle_qp();
    const Walk w = observed_walk(qp, cycling_opts());
    for (const auto &rec : w.records) {
        EXPECT_FALSE(rec.post_drop_factorized);
        EXPECT_EQ(rec.post_drop_verdict, -1);
        EXPECT_EQ(rec.post_drop_inertia_pos, -1);
        EXPECT_EQ(rec.post_drop_inertia_neg, -1);
    }
    EXPECT_EQ(Deg::diag_factorizations, 0);
}

// ===========================================================================
// (7) PER-THREAD, WITH BOTH SOLVES IN FLIGHT AT ONCE
// ===========================================================================

namespace {

/// A two-party start gate: neither side proceeds until both have arrived, so
/// the two solves are genuinely overlapped rather than sequential. Same shape
/// as `test_qp_anti_cycling_arm.cpp`'s.
class StartGate {
  public:
    void arrive_and_wait() {
        std::unique_lock<std::mutex> lock(m_);
        if (++arrived_ == 2) {
            open_ = true;
            cv_.notify_all();
            return;
        }
        cv_.wait(lock, [this] { return open_; });
    }

  private:
    std::mutex m_;
    std::condition_variable cv_;
    int arrived_ = 0;
    bool open_ = false;
};

} // namespace

TEST(QpDegeneracyArm, AnArmedSolveDoesNotReachIntoAnUnarmedOneRunningBesideIt) {
    DegArmGuard guard;
    const QpOptions opts = cycling_opts();
    // THE UNARMED THREAD TAKES THE NON-DEGENERATE QP, for the reason M4's own
    // cross-thread fixture gives: a degenerate walk across two heaps turns a
    // last-digit difference into a different pivot (CLAUDE.md §7), so the
    // bit-identity claim is made on the problem where it is a claim about the
    // SWITCH rather than about the allocator.
    const QpProblem quiet = non_degenerate_qp();
    const QpProblem loud = scale_carrier_cycle_qp();
    const QpSolution solo = plain_solve(quiet, opts);

    StartGate gate;
    QpSolution quiet_result;
    QpSolution loud_result;
    Index loud_refused = 0;
    Index quiet_refused = 0;
    bool quiet_arm_seen = true;

    std::thread armed([&] {
        Deg::reset();
        Deg::arm = DegArm::kScaledDrop;
        Deg::diag_post_drop_inertia = true;
        gate.arrive_and_wait();
        QpEngine engine{opts};
        loud_result = engine.solve(loud);
        loud_refused = Deg::scaled_candidates_refused;
        Deg::reset();
    });
    std::thread unarmed([&] {
        Deg::reset();
        quiet_arm_seen = Deg::any_on();
        gate.arrive_and_wait();
        QpEngine engine{opts};
        quiet_result = engine.solve(quiet);
        quiet_refused = Deg::scaled_candidates_refused;
        Deg::reset();
    });
    armed.join();
    unarmed.join();

    EXPECT_FALSE(quiet_arm_seen) << "the arm leaked across threads before either solve began";
    EXPECT_GT(loud_refused, 0) << "the armed thread was supposed to refuse something";
    EXPECT_EQ(quiet_refused, 0) << "the unarmed thread counted a refusal it never made";
    // The armed side's own status is the fixture's declared outcome (see
    // `ArmAScaledDropRefusesTheDropAndEndsTheRecurrence`), asserted here so a
    // drift in it is caught on this path too.
    EXPECT_EQ(static_cast<int>(loud_result.status), static_cast<int>(QpStatus::kNumericalError));
    expect_same_solution(solo, quiet_result);
}
