// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_anti_cycling_arm.cpp — M7 W2 M4's TWO ANTI-CYCLING ARMS
// (hven/detail/qp/qp_anti_cycling_arm.h), on the `HVEN_TESTING` test-seam
// convention docs/testing.md describes.
//
// WHAT THIS FILE HAS TO PROVE, in the order the round needs it:
//
//   (1) OFF IS PRODUCTION. The switch defaults to `kOff`; with it off the
//       engine's answer and its counters are BIT-IDENTICAL to the run with
//       the observer unattached, on every fixture here including the two
//       cycling ones. This is the in-binary half of the round's neutrality
//       claim; the other half -- that the production LIBRARY's objects are
//       byte-identical -- is the gates' P-SYM and section check, which no
//       test can make.
//   (2) THE ARMS DO WHAT THEY ARE FOR. On a CONSTRUCTED convex QP whose walk
//       cycles at period 2 with exactly zero motion, and on another that
//       cycles at period 4, each arm makes the recurrence DISAPPEAR and the
//       solve return `kOptimal` at the problem's own optimum -- verified two
//       independent ways: by the QP's KKT conditions recomputed here from the
//       returned point, and against a brute-force enumeration of every active
//       set the problem has.
//   (3) AN ARM CHANGES NOTHING WHERE THERE IS NOTHING TO CHANGE. On a
//       fixture whose OFF walk records NO ratio tie, NO drop tie and NO
//       degenerate step, both arms reproduce the OFF run bit for bit.
//   (4) EXPAND's RESET path (§4.3) is reachable and is exercised, rather than
//       being a branch the fixtures describe and never enter -- including its
//       SEQUENCING (the K-th iteration runs at the expanded tolerance and the
//       reset follows it) and step (1)'s nontrivial re-projection count.
//   (5) The switch is PER-THREAD, like the observer's -- both across a
//       finished thread and between two solves IN FLIGHT AT ONCE.
//   (6) ARM E's `alpha_min` FLOOR is honoured on BOTH of the walk's add paths,
//       the ordinary one and the negative-curvature RIDE's.
//   (7) §4.5's negligible-pivot tolerance is READ: the default is hven's own
//       engine threshold and a non-default value changes what can block.
//
// THE CYCLING FIXTURES WERE FOUND, NOT DESIGNED. Both were located by a
// randomized search over small convex QPs (H = M Mᵀ, so positive SEMIdefinite
// and, for k < n, singular; box 0 <= x <= 2; half-integer data; every
// right-hand side >= 0, so x = 0 is feasible and the problem is solvable) for
// a walk whose working-set signature has an exactly periodic suffix with
// Σ|Δx|∞ EXACTLY zero -- the same recurrence M7 W2 M3c's census measured on
// the ten failing CUTEst cells. The search is not committed; what is committed
// is the two problems it found, in full, together with the assertions that the
// OFF walk really does cycle on them. A fixture that stopped cycling FAILS
// here rather than passing vacuously.

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::Index;
using hven::Vec;
using Obs = hven::solvers::detail::testing::QpWalkObserver;
using Ac = hven::solvers::detail::testing::QpAntiCycleArmState;
using Arm = hven::solvers::detail::testing::QpAntiCycleArm;

namespace {

// --------------------------------------------------------------------------
// The problems
// --------------------------------------------------------------------------

/// A convex QP in the search's own parameterization: `H = M Mᵀ` with `M`
/// n-by-k, a linear term, `mi` inequality rows `Ai x <= bi`, and the box
/// `0 <= x <= 2`.
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

/// THE PERIOD-2 FIXTURE. Three variables, three rows, `H` of rank ONE (so the
/// objective is flat in two directions, which is what makes a degenerate
/// vertex easy to sit on). Cold start. Its OFF walk spends every minor it is
/// given in an exactly periodic drop/re-add pair whose iterate does not move.
QpProblem period2_cycle_qp() {
    return convex_qp(3, 1, {1.0, 0.5, -0.5}, {-1.0, 1.0, -1.5}, 3,
                     {0.0, -0.5, -1.0, -1.0, -0.5, -1.0, 0.5, 0.5, 0.5}, {1.0, 1.0, 0.0});
}

/// THE PERIOD-4 FIXTURE. Three variables, four rows, `H` of rank two. Cold
/// start. Its OFF walk cycles over FOUR working sets rather than two, the
/// other period M3c's census observed.
QpProblem period4_cycle_qp() {
    return convex_qp(3, 2, {-0.5, -1.0, -0.5, 0.5, -1.5, 0.0}, {2.0, -2.5, 1.5}, 4,
                     {1.0, 0.0, 1.0, 0.0, 0.5, -1.0, -0.5, -0.5, 0.5, 0.5, 0.5, 0.5},
                     {1.0, 1.0, 1.0, 1.0});
}

/// THE NON-DEGENERATE FIXTURE. A strictly convex box-and-rows QP whose OFF
/// walk records no ratio tie, no drop tie and no degenerate step -- asserted
/// in the fixture that uses it, so the bit-identity claim there cannot pass on
/// a problem that silently became degenerate.
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

// --------------------------------------------------------------------------
// Read-outs
// --------------------------------------------------------------------------

/// The longest exactly-periodic SUFFIX of the working-set signature sequence,
/// searched over periods 1..`max_p`, and the period achieving it. This is
/// M3c's own `p <= 64` recurrence read-out, recomputed here over the seam's
/// records.
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
    double total_motion = 0.0;
};

/// Solves with the observer ARMED and reports both the answer and the
/// recurrence read-out. The observer is read-only -- its own fixture proves
/// that -- and nothing here uses it to decide anything the engine did.
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
    const auto tail_start = sig.size() - static_cast<std::size_t>(w.tail);
    for (std::size_t k = 0; k < Obs::records.size(); ++k) {
        const double d = Obs::records[k].dx_inf;
        if (d > 0.0) {
            w.total_motion += d;
            if (k >= tail_start) {
                w.tail_motion += d;
            }
        }
    }
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

/// AN INDEPENDENT OPTIMUM, by brute force. Every combination of a bound state
/// per variable (free / at lower / at upper) and a subset of active rows is
/// turned into the equality-constrained stationarity system and solved with a
/// complete-orthogonal decomposition, which handles the singular `H` these
/// fixtures have. Feasible stationary points are kept and the smallest
/// objective among them is returned.
///
/// Why it is a legitimate oracle: `H = M Mᵀ` is positive semidefinite, so the
/// QP is CONVEX; its feasible set is a compact polyhedron, so an optimum
/// exists; and at an optimum some active set is one of the sets enumerated
/// here. `QpEngine` touches none of this code.
double brute_force_optimum(const QpProblem &qp) {
    const Index n = qp.n();
    const Index mi = qp.mi();
    // `qp.H` stores the UPPER triangle only (the contract in qp_problem.h), so
    // the dense symmetric matrix is rebuilt here rather than asked of a
    // selfadjointView, whose dense conversion Eigen does not offer for a
    // sparse view.
    const Eigen::MatrixXd Up = Eigen::MatrixXd(qp.H);
    const Eigen::MatrixXd Hd = Up + Up.transpose() - Eigen::MatrixXd(Up.diagonal().asDiagonal());
    const Eigen::MatrixXd Aid = Eigen::MatrixXd(qp.Ai);
    double best = std::numeric_limits<double>::infinity();
    const auto pow3 = static_cast<long long>(std::lround(std::pow(3.0, static_cast<double>(n))));
    for (long long mask_rows = 0; mask_rows < (1LL << mi); ++mask_rows) {
        std::vector<Index> rows;
        for (Index r = 0; r < mi; ++r) {
            if ((mask_rows >> r) & 1LL) {
                rows.push_back(r);
            }
        }
        const auto nr = static_cast<Index>(rows.size());
        const Index dim = n + nr;
        for (long long code = 0; code < pow3; ++code) {
            std::vector<int> state(static_cast<std::size_t>(n), 0);
            long long c = code;
            for (Index i = 0; i < n; ++i) {
                state[static_cast<std::size_t>(i)] = static_cast<int>(c % 3);
                c /= 3;
            }
            Eigen::MatrixXd K = Eigen::MatrixXd::Zero(dim, dim);
            Eigen::VectorXd rhs = Eigen::VectorXd::Zero(dim);
            K.topLeftCorner(n, n) = Hd;
            rhs.head(n) = -qp.g;
            for (Index t = 0; t < nr; ++t) {
                K.block(0, n + t, n, 1) = Aid.row(rows[static_cast<std::size_t>(t)]).transpose();
                K.block(n + t, 0, 1, n) = Aid.row(rows[static_cast<std::size_t>(t)]);
                rhs(n + t) = qp.bi(rows[static_cast<std::size_t>(t)]);
            }
            for (Index i = 0; i < n; ++i) {
                if (state[static_cast<std::size_t>(i)] == 0) {
                    continue;
                }
                // Replace stationarity row i by `x_i = bound`; that bound's
                // own multiplier is the row's residual and is not needed for
                // the objective comparison this function makes.
                const double v =
                    state[static_cast<std::size_t>(i)] == 1 ? qp.lower(i) : qp.upper(i);
                K.row(i).setZero();
                K(i, i) = 1.0;
                rhs(i) = v;
            }
            const Eigen::VectorXd sol = K.completeOrthogonalDecomposition().solve(rhs);
            if (!sol.allFinite() || (K * sol - rhs).lpNorm<Eigen::Infinity>() > 1e-9) {
                continue;
            }
            const Vec x = sol.head(n);
            bool feasible = true;
            for (Index i = 0; i < n && feasible; ++i) {
                feasible = x(i) >= qp.lower(i) - 1e-9 && x(i) <= qp.upper(i) + 1e-9;
            }
            if (feasible && mi > 0) {
                feasible = (qp.bi - Aid * x).minCoeff() >= -1e-9;
            }
            if (feasible) {
                best = std::min(best, objective(qp, x));
            }
        }
    }
    return best;
}

/// The QP's own KKT conditions, recomputed from the returned point. For a
/// CONVEX QP they are necessary AND sufficient for a global minimum, so a
/// point that passes this is an optimum of the problem independently of what
/// the engine reported about itself.
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
    // hven's own stationarity, in hven's own convention: `price()` sets
    // `z(i) = (Hx + g + Aeᵀλe + Aiᵀλi)(i)` for every PINNED i and zero for
    // every free one, so the residual is that expression MINUS z.
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
    // AND THE WHOLE OBJECT, so a field this list does not name cannot move
    // unnoticed -- MINUS THE BYTES NO FIELD OWNS.
    //
    // WHY THE HOLE IS CUT (FIX ROUND 1). `QpCounters` carries one `bool`,
    // `k0_reused`, among `Index` fields, so the compiler pads after it; and a
    // `QpSolution` is DEFAULT-initialized, so whatever was on the stack
    // survives into the bytes no member owns. Padding is not a counter. Two
    // solutions produced the same way usually carry the same garbage there and
    // the same-thread fixtures never noticed; the CROSS-THREAD fixture at the
    // end of this file notices every time, with every counter equal. The two
    // ranges are derived from the struct with `offsetof` rather than typed, so
    // a field added later falls inside one of them and is still covered.
    constexpr std::size_t kOwnedThrough = offsetof(QpCounters, k0_reused) + sizeof(bool);
    constexpr std::size_t kOwnedFrom = offsetof(QpCounters, symbolic_analyses);
    static_assert(kOwnedFrom >= kOwnedThrough,
                  "the two owned ranges must not overlap: k0_reused is followed by "
                  "symbolic_analyses in QpCounters, and the bytes between them are padding");
    const auto *pa = reinterpret_cast<const unsigned char *>(&a);
    const auto *pb = reinterpret_cast<const unsigned char *>(&b);
    EXPECT_EQ(std::memcmp(pa, pb, kOwnedThrough), 0)
        << "two QpCounters differ somewhere the named fields above do not cover";
    EXPECT_EQ(std::memcmp(pa + kOwnedFrom, pb + kOwnedFrom, sizeof(QpCounters) - kOwnedFrom), 0)
        << "two QpCounters differ somewhere the named fields above do not cover";
}

void expect_same_answer(const QpSolution &a, const QpSolution &b) {
    EXPECT_EQ(static_cast<int>(a.status), static_cast<int>(b.status));
    EXPECT_TRUE(same_bits(a.x, b.x, "x"));
    EXPECT_TRUE(same_bits(a.lambda_e, b.lambda_e, "lambda_e"));
    EXPECT_TRUE(same_bits(a.lambda_i, b.lambda_i, "lambda_i"));
    EXPECT_TRUE(same_bits(a.z, b.z, "z"));
    EXPECT_EQ(a.bound_state, b.bound_state);
    EXPECT_EQ(a.ineq_active, b.ineq_active);
    EXPECT_EQ(a.tr_active, b.tr_active);
    expect_same_counters(a.counters, b.counters);
}

/// Restores BOTH per-thread switches on the way out, whatever a fixture did,
/// so one failing expectation cannot leave an arm armed for the next test in
/// the binary.
struct ArmGuard {
    ArmGuard() = default;
    ArmGuard(const ArmGuard &) = delete;
    ArmGuard &operator=(const ArmGuard &) = delete;
    ~ArmGuard() {
        Ac::reset();
        Obs::active = false;
        Obs::reset();
    }
};

QpOptions capped(Index cap) {
    QpOptions opts;
    opts.max_iter = cap;
    // The premise "the old walk cycles" belongs to the pre-E2 walk: with the
    // independent working set (M7 W2 E2) these QPs END (the E2 fixtures at the
    // end of test_qp_anti_cycling_arm.cpp).
    opts.independent_working_set = false;
    return opts;
}

constexpr Index kCycleCap = 200;

/// THE RE-PREMISED READ-OUT (M7 W2 E4 fix round 2, ruling E4-19).
///
/// WHAT THIS HELPER USED TO ASSERT, and why it cannot any more. It was
/// `expect_the_off_walk_cycles`: the OFF walk on `qp` spends its whole budget,
/// its working-set signature has an exactly periodic zero-motion suffix of the
/// expected period, and that suffix covers most of the walk. That premise held
/// from M4 until M7 W2 E4. **The E4 walk does not cycle on either of these two
/// fixtures.** Both now read `kOptimal` in FOUR minors, with the recurrence
/// gone at its source: the recurrence was driven by a NOISE-LEVEL DROP -- a
/// multiplier below `opt_tol * scale` that the pre-E4 walk believed and dropped,
/// re-adding the same member on the next pass -- and E4's proximal EQP prices
/// that multiplier differently (the primal regularization's own term is no
/// longer folded into it, NC2 fix1), so the drop is never made.
///
/// WHAT IT ASSERTS NOW: what the OFF walk actually reads, by name, so the change
/// is pinned rather than merely tolerated, and so a REGRESSION back into the
/// recurrence fails here. `expected_period` is retired as an argument: the
/// recurrence it named does not exist.
///
/// NO CELL IN THIS TREE STILL CYCLES. Ruling E4-19(b) keeps the old premise on
/// any cell that does; there is none, in this file or in
/// test_qp_degeneracy_arm.cpp, and that is a finding rather than a reason to
/// delete the arms -- they stay in the tree, test-only, and the fixtures below
/// now assert that they are INERT where they used to assert an escape.
Walk expect_the_off_walk_reads(const QpProblem &qp, QpStatus status, Index minors) {
    Ac::reset();
    const Walk off = observed_walk(qp, capped(kCycleCap));
    EXPECT_EQ(static_cast<int>(off.solution.status), static_cast<int>(status))
        << "the OFF walk's status moved: E4 fix2 measured " << static_cast<int>(status);
    EXPECT_EQ(off.minors, minors) << "the OFF walk's minor count moved";
    EXPECT_LT(off.minors * 5, kCycleCap)
        << "the OFF walk is back to spending its budget: the recurrence has returned";
    // NOT asserted: that the walk MOVED. The escape read-out this replaces could
    // require it, because it was contrasting against a stuck walk; MEASURED at E4
    // fix round 2, `period2_cycle_qp`'s optimum IS its start vertex, so the whole
    // walk is a zero-motion working-set correction and `total_motion` is exactly
    // 0. Asserting motion here would assert a property of the fixture's start
    // point, not of the walk.
    //
    // AND IT IS THE PROBLEM'S OWN OPTIMUM, the same two independent checks the
    // armed read-out uses -- because "it terminates" is worth nothing without it.
    EXPECT_TRUE(is_a_kkt_point(qp, off.solution, 1e-7));
    EXPECT_NEAR(objective(qp, off.solution.x), brute_force_optimum(qp), 1e-7);
    return off;
}

/// THE RE-PREMISED ARM READ-OUT (M7 W2 E4 fix round 2, ruling E4-19(a)).
///
/// WHAT IT USED TO ASSERT: that the ARMED walk ESCAPED a recurrence the OFF
/// walk was stuck in -- `kOptimal`, no zero-motion periodic suffix worth the
/// name, the problem's own optimum, and the arm's counters showing it changed a
/// decision. With the recurrence gone (see `expect_the_off_walk_reads`) there is
/// nothing to escape, and an escape assertion on a walk that already terminates
/// asserts nothing at all.
///
/// WHAT IT ASSERTS NOW: **the arm is INERT.** The armed walk reads the SAME
/// status and the SAME minor count as the unarmed one, and returns the problem's
/// own optimum by the same two independent checks. That is real regression
/// coverage and it is the coverage the arm still needs: an arm that is supposed
/// to change a decision only at a degenerate tie must not perturb a walk that
/// never reaches one. What is NOT asserted any more is that the arm's own
/// switch counters moved -- they are reported for the record instead, because
/// "the arm did something" is exactly what is no longer true here.
void expect_the_arm_is_inert(const QpProblem &qp, Arm arm, const Walk &off) {
    Ac::reset();
    Ac::arm = arm;
    const Walk on = observed_walk(qp, capped(kCycleCap));
    const Index min_steps = Ac::min_steps;
    const Index pivot_switches = Ac::pivot_switches;
    const Index lex_drop_switches = Ac::lex_drop_switches;
    const Index lex_ratio_switches = Ac::lex_ratio_switches;
    Ac::reset();

    // INERTNESS, and it is asserted as an EQUALITY against the unarmed walk
    // rather than as a bound: the armed walk must reach the same answer in the
    // same number of passes. That is what "the arm does not change a walk that
    // does not cycle" means, and it is a stronger statement than the escape
    // assertion it replaces was on a walk that already terminates.
    EXPECT_EQ(static_cast<int>(on.solution.status), static_cast<int>(off.solution.status))
        << "the arm changed the status of a walk that does not cycle";
    EXPECT_EQ(on.minors, off.minors) << "the arm changed the minor count of a walk that does not "
                                        "cycle";
    // MOTION: the same to a relative 1e-9, NOT bit-identical, and the difference is
    // the arm itself. MEASURED at E4 fix round 2: arm E takes its expanded-tolerance
    // FLOOR STEP on both fixtures (min_steps 2, one pivot switch), which moves the
    // iterate by 2.4e-13 where the unarmed walk moves nothing on the period-2 cell,
    // and by 1.9999999999999998 against 2 on the period-4 one. So "inert" here is
    // the statement ruling E4-19(a) asks for -- the same STATUS and the same MINOR
    // COUNT -- and not bit-identity, which the arm's own floor step rules out.
    EXPECT_NEAR(on.total_motion, off.total_motion, 1e-9 * std::max(1.0, off.total_motion))
        << "the arm moved the iterate a materially different distance";

    // AND THE ANSWER IS THE PROBLEM'S OWN OPTIMUM, two independent ways -- kept
    // verbatim from the escape read-out, because this half never depended on the
    // recurrence.
    EXPECT_TRUE(is_a_kkt_point(qp, on.solution, 1e-7));
    EXPECT_NEAR(objective(qp, on.solution.x), brute_force_optimum(qp), 1e-7);

    // THE ARM'S OWN SWITCH COUNTERS: REPORTED, NOT ASSERTED. The escape read-out
    // required them nonzero ("the arm actually did something"); on a walk with no
    // degenerate tie to re-break there is nothing for them to count, and pinning
    // a zero here would pin the absence of the very mechanism the arm exists for
    // rather than the arm's inertness, which is what the equalities above say.
    // They are printed so a future round reading this file sees the numbers.
    std::printf("[ e4fix2 ] arm %s inert: off %d/%ld == on %d/%ld ; switches min=%ld pivot=%ld "
                "lex_drop=%ld lex_ratio=%ld\n",
                arm == Arm::kExpand ? "E" : "L", static_cast<int>(off.solution.status),
                static_cast<long>(off.minors), static_cast<int>(on.solution.status),
                static_cast<long>(on.minors), static_cast<long>(min_steps),
                static_cast<long>(pivot_switches), static_cast<long>(lex_drop_switches),
                static_cast<long>(lex_ratio_switches));
}

} // namespace

// ===========================================================================
// (1) OFF IS PRODUCTION
// ===========================================================================

TEST(QpAntiCyclingArm, TheSwitchIsOffByDefaultAndItsParametersAreThePaperDerivedOnes) {
    ArmGuard guard;
    Ac::reset();
    EXPECT_EQ(static_cast<int>(Ac::arm), static_cast<int>(Arm::kOff));
    EXPECT_FALSE(Ac::any_on());
    EXPECT_FALSE(Ac::expand_on());
    EXPECT_FALSE(Ac::lex_on());
    // EXPAND §4.2's relations among the four parameters, checked so a later
    // edit to one cannot silently break the others.
    EXPECT_DOUBLE_EQ(Ac::params.delta_0(), 0.5 * Ac::params.delta_master);
    EXPECT_DOUBLE_EQ(Ac::params.delta_max(), 0.99 * Ac::params.delta_master);
    EXPECT_EQ(Ac::params.expand_k, 10000);
    EXPECT_DOUBLE_EQ(Ac::params.tau(), (Ac::params.delta_max() - Ac::params.delta_0()) /
                                           static_cast<double>(Ac::params.expand_k));
    EXPECT_GT(Ac::params.tau(), 0.0);
    // ARM L's default is the BRIEFED tie-break form, not the full-pricing
    // Bland-STYLE analogue.
    EXPECT_FALSE(Ac::params.lex_full_pricing);
    // AND §4.5's `t` IS hven's OWN ENGINE THRESHOLD, at the default (FIX
    // ROUND 1, review Important 5). The arm READS this field now, so the
    // default has to BE production's threshold rather than merely resemble it:
    // `src/qp/qp_engine.cpp` ties the two literals together with a
    // `static_assert`, and this is the same fact asserted from outside, where
    // a reader of the fixtures can see it.
    EXPECT_DOUBLE_EQ(Ac::params.negligible_pivot, hven::solvers::detail::kEngineDenomTol);
    EXPECT_DOUBLE_EQ(Ac::params.reset_nontrivial, 3.7e-11);
    // And `begin_minor()` is inert unless ARM E is armed.
    EXPECT_FALSE(Ac::begin_minor());
    EXPECT_EQ(Ac::expand_iter, 0);
    EXPECT_EQ(Ac::delta, 0.0);
}

TEST(QpAntiCyclingArm, AnUnarmedSwitchWithTheObserverAttachedIsTheUnattachedRunBitForBit) {
    ArmGuard guard;
    struct Cse {
        const char *name;
        QpProblem (*build)();
        Index cap;
    };
    const Cse cases[] = {{"period2", &period2_cycle_qp, kCycleCap},
                         {"period4", &period4_cycle_qp, kCycleCap},
                         {"non_degenerate", &non_degenerate_qp, 0}};
    for (const Cse &c : cases) {
        SCOPED_TRACE(c.name);
        const QpProblem qp = c.build();
        const QpOptions opts = capped(c.cap);

        Ac::reset();
        Obs::reset();
        const QpSolution without = plain_solve(qp, opts);
        EXPECT_TRUE(Obs::records.empty());

        Ac::reset();
        Obs::reset();
        Obs::active = true;
        const QpSolution with = plain_solve(qp, opts);
        EXPECT_FALSE(Obs::records.empty()) << "an ACTIVE observer recorded nothing";
        Obs::active = false;
        Obs::reset();

        expect_same_answer(without, with);
        // And the arms recorded nothing, because nothing was armed.
        EXPECT_EQ(Ac::min_steps, 0);
        EXPECT_EQ(Ac::pivot_switches, 0);
        EXPECT_EQ(Ac::lex_drop_switches, 0);
        EXPECT_EQ(Ac::lex_ratio_switches, 0);
        EXPECT_EQ(Ac::unpinned_adds, 0);
        EXPECT_EQ(Ac::resets, 0);
    }
}

// ===========================================================================
// (2) THE ARMS DO WHAT THEY ARE FOR
// ===========================================================================

// THE RENAME EVENT (M7 W2 E4 fix round 2, ruling E4-19(c)). These six tests are
// the same six experiments with their premise re-derived; each one's name used to
// say "cycles" or "escapes", which is now false, so each was RENAMED to say what
// it asserts. Old -> new:
//
//   TheOffWalkCyclesAtPeriodTwoWithExactlyZeroMotion
//     -> TheOffWalkOnThePeriodTwoFixtureTerminatesAtItsOptimum
//   TheOffWalkCyclesAtPeriodFourWithExactlyZeroMotion
//     -> TheOffWalkOnThePeriodFourFixtureTerminatesAtItsOptimum
//   ArmEEscapesThePeriodTwoCycleAndReturnsTheOptimum
//     -> ArmEIsInertOnThePeriodTwoFixtureThatNoLongerCycles
//   ArmLEscapesThePeriodTwoCycleAndReturnsTheOptimum
//     -> ArmLIsInertOnThePeriodTwoFixtureThatNoLongerCycles
//   ArmEEscapesThePeriodFourCycleAndReturnsTheOptimum
//     -> ArmEIsInertOnThePeriodFourFixtureThatNoLongerCycles
//   ArmLEscapesThePeriodFourCycleAndReturnsTheOptimum
//     -> ArmLIsInertOnThePeriodFourFixtureThatNoLongerCycles
//
// The names are the whole of the by-name gate's view of this file, so the event
// is declared as six REMOVED and six ADDED.

TEST(QpAntiCyclingArm, TheOffWalkOnThePeriodTwoFixtureTerminatesAtItsOptimum) {
    ArmGuard guard;
    // MEASURED at M7 W2 E4 fix round 2: kOptimal in 4 minors, where M4 through
    // E3 read kMaxIter at the 200-minor cap with a period-2 zero-motion suffix.
    expect_the_off_walk_reads(period2_cycle_qp(), QpStatus::kOptimal, 4);
}

TEST(QpAntiCyclingArm, TheOffWalkOnThePeriodFourFixtureTerminatesAtItsOptimum) {
    ArmGuard guard;
    // MEASURED at M7 W2 E4 fix round 2: kOptimal in 4 minors, where M4 through
    // E3 read kMaxIter at the cap with a period-4 zero-motion suffix.
    expect_the_off_walk_reads(period4_cycle_qp(), QpStatus::kOptimal, 4);
}

TEST(QpAntiCyclingArm, ArmEIsInertOnThePeriodTwoFixtureThatNoLongerCycles) {
    ArmGuard guard;
    const QpProblem qp = period2_cycle_qp();
    const Walk off = expect_the_off_walk_reads(qp, QpStatus::kOptimal, 4);
    expect_the_arm_is_inert(qp, Arm::kExpand, off);
}

TEST(QpAntiCyclingArm, ArmLIsInertOnThePeriodTwoFixtureThatNoLongerCycles) {
    ArmGuard guard;
    const QpProblem qp = period2_cycle_qp();
    const Walk off = expect_the_off_walk_reads(qp, QpStatus::kOptimal, 4);
    expect_the_arm_is_inert(qp, Arm::kLexicographic, off);
}

TEST(QpAntiCyclingArm, ArmEIsInertOnThePeriodFourFixtureThatNoLongerCycles) {
    ArmGuard guard;
    const QpProblem qp = period4_cycle_qp();
    const Walk off = expect_the_off_walk_reads(qp, QpStatus::kOptimal, 4);
    expect_the_arm_is_inert(qp, Arm::kExpand, off);
}

TEST(QpAntiCyclingArm, ArmLIsInertOnThePeriodFourFixtureThatNoLongerCycles) {
    ArmGuard guard;
    const QpProblem qp = period4_cycle_qp();
    const Walk off = expect_the_off_walk_reads(qp, QpStatus::kOptimal, 4);
    expect_the_arm_is_inert(qp, Arm::kLexicographic, off);
}

// ===========================================================================
// (3) AN ARM CHANGES NOTHING WHERE THERE IS NOTHING TO CHANGE
// ===========================================================================

TEST(QpAntiCyclingArm, NeitherArmChangesAWalkWithNoTieAndNoZeroRatio) {
    ArmGuard guard;
    const QpProblem qp = non_degenerate_qp();
    const QpOptions opts;

    Ac::reset();
    const QpSolution off = plain_solve(qp, opts);
    ASSERT_EQ(static_cast<int>(off.status), static_cast<int>(QpStatus::kOptimal));
    // THE FIXTURE'S OWN NON-DEGENERACY, asserted rather than assumed: with any
    // of these non-zero the bit-identity below would be a claim about a
    // degenerate walk, which is the opposite of what it is for.
    ASSERT_EQ(off.counters.ratio_ties, 0);
    ASSERT_EQ(off.counters.drop_ties, 0);
    ASSERT_EQ(off.counters.degenerate_steps, 0);
    ASSERT_GT(off.counters.minor_iters, 1) << "a one-pass walk would prove nothing";
    ASSERT_GT(off.counters.ws_adds + off.counters.ws_drops, 0)
        << "a walk that never touched its working set would prove nothing";

    for (Arm arm : {Arm::kExpand, Arm::kLexicographic}) {
        SCOPED_TRACE(arm == Arm::kExpand ? "ARM E" : "ARM L");
        Ac::reset();
        Ac::arm = arm;
        const QpSolution on = plain_solve(qp, opts);
        const Index touched = Ac::min_steps + Ac::pivot_switches + Ac::lex_drop_switches +
                              Ac::lex_ratio_switches + Ac::unpinned_adds;
        Ac::reset();
        expect_same_answer(off, on);
        EXPECT_EQ(touched, 0) << "the arm reports having changed a decision on a walk whose "
                                 "answer it did not change";
    }
}

// ===========================================================================
// (4) EXPAND'S RESET PATH IS REACHED
// ===========================================================================

/// EXPAND §4.3's SEQUENCING, asserted on the state machine itself (FIX ROUND
/// 1, review Important 1). The paper runs K iterations at the EXPANDING
/// tolerances delta_1 .. delta_K and resets AFTER the K-th; the first
/// implementation signalled the reset from inside the same call that produced
/// delta_K, so the K-th minor ran at delta_0. This fixture is written against
/// the state machine rather than against a walk because that is where the
/// sequence is decidable exactly: what delta the K-th iteration RAN at is not
/// recoverable from a solve's counters.
TEST(QpAntiCyclingArm, ArmEsKthIterationRunsAtTheExpandedToleranceAndTheResetFollowsIt) {
    ArmGuard guard;
    Ac::reset();
    Ac::arm = Arm::kExpand;
    const Index kK = 3; // small, so the boundary is reachable by hand
    Ac::params.expand_k = kK;
    const double tau = Ac::params.tau();
    Ac::begin_solve();
    EXPECT_EQ(Ac::expand_iter, 0);
    EXPECT_DOUBLE_EQ(Ac::delta, Ac::params.delta_0());

    // THE K ITERATIONS OF THE SEQUENCE. Each expands, none asks for a reset,
    // and the K-th ends at the LARGEST tolerance of the sequence.
    double want = Ac::params.delta_0();
    for (Index k = 1; k <= kK; ++k) {
        SCOPED_TRACE(k);
        EXPECT_FALSE(Ac::begin_minor()) << "the reset was signalled at iteration " << k << " of "
                                        << kK << ", before that iteration ran";
        want += tau;
        EXPECT_EQ(Ac::expand_iter, k);
        EXPECT_DOUBLE_EQ(Ac::delta, want);
        EXPECT_EQ(Ac::resets, 0);
    }
    EXPECT_DOUBLE_EQ(Ac::delta, Ac::params.delta_0() + 3.0 * tau)
        << "iteration K did not run at delta_K";

    // AND THE RESET IS DUE AT ITERATION K+1, before its ratio test -- and the
    // call that signals it must NOT also expand, or the sequence would have
    // run K+1 tolerances.
    EXPECT_TRUE(Ac::begin_minor()) << "the sequence ran past its length K without a reset";
    EXPECT_EQ(Ac::expand_iter, kK) << "the signalling call expanded the sequence as well";
    EXPECT_DOUBLE_EQ(Ac::delta, want);

    // §4.3 step (3): a NEW sequence begins here, and this minor is its FIRST,
    // so it runs at delta_1 = delta_0 + tau.
    Ac::note_reset();
    EXPECT_EQ(Ac::resets, 1);
    EXPECT_EQ(Ac::expand_iter, 1);
    want = Ac::params.delta_0() + tau;
    EXPECT_DOUBLE_EQ(Ac::delta, want);

    // AND THE NEW SEQUENCE IS K LONG TOO: K-1 more expansions, then the next
    // reset. A reset that cost the sequence an iteration would show up here.
    for (Index k = 2; k <= kK; ++k) {
        SCOPED_TRACE(k);
        EXPECT_FALSE(Ac::begin_minor());
        want += tau;
        EXPECT_EQ(Ac::expand_iter, k);
        EXPECT_DOUBLE_EQ(Ac::delta, want);
    }
    EXPECT_TRUE(Ac::begin_minor());
    EXPECT_EQ(Ac::resets, 1) << "note_reset() is the caller's, not begin_minor()'s";
}

/// §4.3 step (1) COUNTED A NONTRIVIAL RE-PROJECTION, which is the part of the
/// reset the previous fixture's name used to claim and no fixture asserted
/// (FIX ROUND 1, review Important 1). A reset re-projects every PINNED
/// variable lying within delta_f of its bound exactly onto it, and counts the
/// adjustments larger than eps^(2/3) as "nontrivial". Under ARM E such a
/// variable exists by construction: an `alpha_min` FLOOR step deliberately
/// leaves its blocker off its bound by up to tau, and with a short sequence
/// the next reset arrives before the walk has erased that displacement.
TEST(QpAntiCyclingArm, ArmEsResetCountsANontrivialReProjection) {
    ArmGuard guard;
    const QpProblem qp = period4_cycle_qp();
    Ac::reset();
    Ac::arm = Arm::kExpand;
    // K = 1: every minor after the first opens with a reset, so a floor step
    // taken at minor m is seen by the reset at minor m+1. tau is then
    // 0.49 * delta_master = 4.9e-10, which is both ABOVE `reset_nontrivial`
    // (3.7e-11, so the displacement counts) and BELOW `delta_master` (1e-9, so
    // the re-projection window sees it at all).
    Ac::params.expand_k = 1;
    const Walk on = observed_walk(qp, capped(kCycleCap));
    const Index resets = Ac::resets;
    const Index nontrivial = Ac::reset_nontrivial_moves;
    const Index min_steps = Ac::min_steps;
    const Index unpinned = Ac::unpinned_adds;
    Ac::reset();

    EXPECT_GT(min_steps, 0) << "no floor step was taken, so nothing was left off a bound";
    EXPECT_GT(unpinned, 0) << "no add was left unpinned, so no displacement survived the add";
    EXPECT_GT(resets, 0) << "EXPAND section 4.3's reset never ran";
    EXPECT_GT(nontrivial, 0)
        << "every re-projection at a reset moved a variable by at most eps^(2/3): section "
           "4.3 step (1)'s nontrivial count is a branch no fixture entered";
    EXPECT_GT(on.minors, 0);
}

TEST(QpAntiCyclingArm, ArmEsResetSequenceIsReachableAndRestartsTheTolerance) {
    ArmGuard guard;
    const QpProblem qp = period4_cycle_qp();
    Ac::reset();
    Ac::arm = Arm::kExpand;
    // EXPAND §4.2's K is the LENGTH of an expanding sequence. The paper
    // recommends 10000, which no fixture this small reaches; shortening it is
    // what makes §4.3's reset -- the branch that re-projects the iterate onto
    // its working bounds and restarts the tolerance -- an EXERCISED path
    // rather than a described one.
    Ac::params.expand_k = 2;
    const Walk on = observed_walk(qp, capped(kCycleCap));
    const Index resets = Ac::resets;
    const double delta_at_end = Ac::delta;
    const double delta_max = Ac::params.delta_max();
    Ac::reset();

    EXPECT_GT(resets, 0) << "EXPAND §4.3's reset never ran";
    EXPECT_LE(delta_at_end, delta_max) << "delta was never brought back below its maximum";
    EXPECT_EQ(static_cast<int>(on.solution.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_TRUE(is_a_kkt_point(qp, on.solution, 1e-7));
}

// ===========================================================================
// (5) THE SWITCH IS PER-THREAD
// ===========================================================================

TEST(QpAntiCyclingArm, ArmingOnOneThreadIsInvisibleOnAnother) {
    ArmGuard guard;
    const QpProblem qp = period2_cycle_qp();
    const QpOptions opts = capped(kCycleCap);

    Ac::reset();
    const QpSolution sequential_off = plain_solve(qp, opts);

    QpSolution armed_other_thread;
    std::thread other([&] {
        Ac::reset();
        Ac::arm = Arm::kExpand;
        armed_other_thread = plain_solve(qp, opts);
        Ac::reset();
    });
    other.join();
    Ac::reset();
    const QpSolution unarmed_this_thread = plain_solve(qp, opts);

    EXPECT_EQ(static_cast<int>(armed_other_thread.status), static_cast<int>(QpStatus::kOptimal))
        << "the other thread's arm did not take";
    expect_same_answer(sequential_off, unarmed_this_thread);
}

// ===========================================================================
// (6) THE RIDE'S ADD PATH HONOURS THE FLOOR TOO
// ===========================================================================

namespace {

using Action = hven::solvers::detail::testing::QpWalkAction;
using Block = hven::solvers::detail::testing::QpWalkBlock;

/// A QP in the engine's own terms: a dense symmetric `H` given in full (the
/// INDEFINITE Hessians below are not `M Mᵀ` and cannot be written through
/// `convex_qp`), a linear term, `mi` rows `Ai x <= bi`, and the box
/// `0 <= x <= 2`.
QpProblem dense_qp(Index n, const std::vector<double> &h_rowmajor, const std::vector<double> &g,
                   Index mi, const std::vector<double> &a_rowmajor, const std::vector<double> &b) {
    Eigen::MatrixXd Hd(n, n);
    for (Index r = 0; r < n; ++r) {
        for (Index c = 0; c < n; ++c) {
            Hd(r, c) = h_rowmajor[static_cast<std::size_t>(r * n + c)];
        }
    }
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

/// THE RIDE FIXTURE, FOUND THE SAME WAY THE CYCLING ONES WERE. An INDEFINITE
/// three-variable QP over three rows and the box `0 <= x <= 2`, located by a
/// randomized search for a walk that (a) enters section 4c's negative-curvature
/// RIDE and steps, (b) has the ride block on a BOUND, and (c) adds no bound
/// anywhere else — so the one unpinned add the fixture measures can only be the
/// ride's. The search is not committed; the problem it found is.
QpProblem ride_floor_qp() {
    return dense_qp(3, {0.5, 2.0, -1.0, 2.0, 2.0, 1.5, -1.0, 1.5, 0.5}, {-1.0, 0.0, 2.0}, 3,
                    {1.5, -0.5, 1.5, 0.0, 2.0, -0.5, 1.0, 1.5, 1.5}, {1.0, 0.5, 1.0});
}

} // namespace

/// ARM E's `alpha_min` FLOOR IS HONOURED ON THE RIDE'S ADD PATH (FIX ROUND 1,
/// review Important 2). The negative-curvature ride reaches its blocking
/// constraint through the SAME `ratio_test` the ordinary add path uses, so
/// armed with E it can be handed a floor step there too; the first
/// implementation then re-pinned that blocker exactly, undoing the positive
/// move the floor exists to create. Both paths now ask ONE predicate
/// (`QpAntiCycleArmState::expand_skips_exact_repin()`), and this fixture is
/// what would notice them diverging again: before the fix it measured
/// `arm_unpinned_adds = 0` against `arm_min_steps = 1`.
TEST(QpAntiCyclingArm, ArmEsFloorStepOnTheNegativeCurvatureRideIsNotRePinned) {
    ArmGuard guard;
    const QpProblem qp = ride_floor_qp();
    Ac::reset();
    Ac::arm = Arm::kExpand;
    // A CONSTRUCTED tau, and it is constructed deliberately. At the default
    // parameters tau is ~4.9e-14 and only a degenerate (exactly-zero) blocking
    // step is below it, which is not a situation this small walk reaches on its
    // ride. `delta_master = 1e-2` with `expand_k = 1` makes tau 4.9e-3, so the
    // ride's own blocking step is below the floor and Case 2 of §4.1 is the
    // case that runs. Nothing about the SHARED-PREDICATE claim depends on the
    // size of tau; what depends on it is reaching the branch at all.
    Ac::params.delta_master = 1e-2;
    Ac::params.expand_k = 1;

    Obs::reset();
    Obs::active = true;
    QpEngine engine{capped(60)};
    const QpSolution sol = engine.solve(qp);
    Index ride_stepped = 0;
    Index ride_bound_adds = 0;
    Index ordinary_bound_adds = 0;
    for (const auto &r : Obs::records) {
        if (r.action == Action::kRideStepped) {
            ++ride_stepped;
        }
        if (r.block != Block::kBound) {
            continue;
        }
        if (r.action == Action::kRideStepped) {
            ++ride_bound_adds;
        } else if (r.action == Action::kAdd) {
            ++ordinary_bound_adds;
        }
    }
    const Index min_steps = Ac::min_steps;
    const Index unpinned_adds = Ac::unpinned_adds;
    Obs::active = false;
    Obs::reset();
    Ac::reset();

    // NON-VACUITY FIRST. Every one of these would let the claim below pass on
    // a walk that never rode, never blocked on a bound there, or never took a
    // floor step at all.
    ASSERT_EQ(static_cast<int>(sol.status), static_cast<int>(QpStatus::kOptimal));
    ASSERT_EQ(ride_stepped, 1) << "section 4c's ride never stepped: this fixture is not about "
                                  "the ride any more";
    ASSERT_EQ(ride_bound_adds, 1) << "the ride did not block on a BOUND, so no re-pin was at "
                                     "stake on its add path";
    ASSERT_EQ(ordinary_bound_adds, 0)
        << "the walk added a bound OUTSIDE the ride, so an unpinned add below could be that "
           "one rather than the ride's";
    ASSERT_EQ(min_steps, 1) << "the ride's step was not the alpha_min FLOOR";

    // THE CLAIM. The walk's ONE bound add is the ride's, the walk's ONE floor
    // step is that add's, and the blocker was LEFT OFF its bound -- the same
    // treatment §4.1 Cases 2/3 get on the ordinary path.
    EXPECT_EQ(unpinned_adds, 1)
        << "the ride re-pinned its blocking bound exactly after a floor step, which undoes the "
           "positive move alpha_min exists to create";
    EXPECT_EQ(unpinned_adds, min_steps) << "a floor step was taken that no add left unpinned";
}

// ===========================================================================
// (7) §4.5's `t` IS READ
// ===========================================================================

/// `negligible_pivot` DECIDES ELIGIBILITY (FIX ROUND 1, review Important 5).
/// The field used to be printed by the probe as a realized parameter and never
/// read; the arms always used `detail::kEngineDenomTol`. It is read now, and
/// the two halves of that are asserted here: at the DEFAULT the arm's
/// candidate set is production's (the default IS that constant, asserted in the
/// defaults fixture above and by a `static_assert` in `src/qp/qp_engine.cpp`),
/// and at a NON-default value the eligible set really changes.
TEST(QpAntiCyclingArm, ArmEsNegligiblePivotParameterDecidesWhichConstraintsCanBlock) {
    ArmGuard guard;
    const QpProblem qp = period2_cycle_qp();
    const QpOptions opts = capped(kCycleCap);

    // AT THE DEFAULT: constraints block, and the walk adds them.
    Ac::reset();
    Ac::arm = Arm::kExpand;
    const QpSolution at_default = plain_solve(qp, opts);
    Ac::reset();
    ASSERT_GT(at_default.counters.ws_adds, 0)
        << "the default-parameter walk adds nothing, so raising the threshold could not "
           "take anything away";

    // AT A THRESHOLD ABOVE EVERY FINITE PIVOT: §4.5 calls every component of
    // `p` negligible, no constraint is eligible to block, and the walk adds
    // NOTHING. The value is 1e300 and not something modest BY MEASUREMENT:
    // this fixture's `H` has rank one, so the regularized EQP on a singular
    // face returns directions whose components run past 1e6, and a threshold
    // chosen to look reasonable leaves candidates eligible. A threshold no
    // finite double pivot can exceed makes the claim about the PARAMETER
    // rather than about this problem's conditioning.
    Ac::reset();
    Ac::arm = Arm::kExpand;
    Ac::params.negligible_pivot = 1e300;
    const QpSolution at_huge = plain_solve(qp, opts);
    const Index min_steps = Ac::min_steps;
    const Index pivot_switches = Ac::pivot_switches;
    Ac::reset();

    EXPECT_EQ(at_huge.counters.ws_adds, 0)
        << "a constraint blocked although every pivot is below the negligible-pivot threshold: "
           "the arm is not reading its own parameter";
    EXPECT_EQ(min_steps, 0) << "a floor step was taken with no blocking constraint";
    EXPECT_EQ(pivot_switches, 0) << "the second pass selected a constraint it had ruled out";
}

// ===========================================================================
// (8) TWO SOLVES IN FLIGHT AT ONCE, ONE ARMED AND ONE NOT
// ===========================================================================

namespace {

/// A TWO-PARTY START GATE, hand-rolled from a mutex and a condition variable
/// exactly as `test_qp_walk_seam.cpp`'s is and for the same reason: without it
/// the two workers are merely CREATED close together and a valid scheduler may
/// run them one after the other, which is what
/// `ArmingOnOneThreadIsInvisibleOnAnother` above already proves.
class ArmStartGate {
  public:
    void arrive_and_wait() {
        std::unique_lock<std::mutex> lk(m_);
        if (++arrived_ == 2) {
            open_ = true;
            cv_.notify_all();
            return;
        }
        cv_.wait(lk, [this] { return open_; });
    }

  private:
    std::mutex m_;
    std::condition_variable cv_;
    int arrived_ = 0;
    bool open_ = false;
};

} // namespace

/// THE SWITCH IS PER-THREAD WHILE TWO SOLVES OVERLAP (FIX ROUND 1, review
/// Minor 1). The sequential fixture above joins the armed worker before the
/// unarmed solve starts, so it proves that arming does not PERSIST across a
/// finished thread — not that an armed solve cannot reach into an unarmed one
/// running beside it. Here both threads build their problem and construct
/// their engine, wait at the gate, and are released together, so what overlaps
/// is `QpEngine::run` itself. The unarmed thread's answer must be its solo
/// unarmed answer, bit for bit, and the armed thread's arm must have taken.
///
/// WHAT IT CANNOT PROMISE, said plainly: the defect it is written against is a
/// data race on process-global state, which is undefined behaviour, so no
/// assertion can promise a symptom from one. The gate makes overlap the
/// overwhelmingly likely case rather than an accident of scheduling; a
/// ThreadSanitizer verdict would make it a certainty and this tree has no such
/// preset (`docs/testing.md` registers adding one).
TEST(QpAntiCyclingArm, AnArmedSolveDoesNotReachIntoAnUnarmedOneRunningBesideIt) {
    ArmGuard guard;
    // TWO DIFFERENT PROBLEMS, ONE PER THREAD, and the choice is not cosmetic.
    // The ARMED thread takes the period-2 cycling QP, because an arm must have
    // something to do for "the arm took" to mean anything. The UNARMED thread
    // takes the NON-DEGENERATE QP, because the claim on that side is
    // BIT-IDENTITY against a solo run and a degenerate rank-one walk is not a
    // sound subject for one across threads: MKL's kernels are address-
    // sensitive (CLAUDE.md §7), two heaps give two alignments, and a walk
    // sitting on a degenerate vertex turns a last-digit difference into a
    // different pivot. MEASURED, not assumed -- the cycling QP run this way
    // differs from its solo self in `QpCounters`, at a DIFFERENT byte on
    // different runs, with no arm involved on that thread at all.
    const QpOptions opts = capped(kCycleCap);
    const QpOptions quiet_opts;

    // THE SOLO UNARMED ANSWER, taken first and on this thread.
    Ac::reset();
    const QpSolution solo_unarmed = plain_solve(non_degenerate_qp(), quiet_opts);

    ArmStartGate gate;
    QpSolution armed_out;
    QpSolution unarmed_out;
    Index armed_switches = 0;
    Index unarmed_switches = 0;
    bool armed_saw_an_arm_at_entry = false;
    bool unarmed_saw_an_arm_at_entry = false;

    std::thread armed([&] {
        armed_saw_an_arm_at_entry = Ac::any_on();
        Ac::reset();
        Ac::arm = Arm::kExpand;
        const QpProblem mine = period2_cycle_qp();
        QpEngine engine{opts};
        // EVERYTHING EXPENSIVE THAT IS NOT THE SOLVE IS ABOVE THIS LINE.
        gate.arrive_and_wait();
        armed_out = engine.solve(mine);
        armed_switches = Ac::min_steps + Ac::pivot_switches;
        Ac::reset();
    });
    std::thread unarmed([&] {
        unarmed_saw_an_arm_at_entry = Ac::any_on();
        Ac::reset();
        const QpProblem mine = non_degenerate_qp();
        QpEngine engine{quiet_opts};
        gate.arrive_and_wait();
        unarmed_out = engine.solve(mine);
        unarmed_switches = Ac::min_steps + Ac::pivot_switches + Ac::lex_drop_switches +
                           Ac::lex_ratio_switches + Ac::unpinned_adds + Ac::resets;
        Ac::reset();
    });
    armed.join();
    unarmed.join();
    Ac::reset();

    // Neither thread saw the other's switch on the way in.
    EXPECT_FALSE(armed_saw_an_arm_at_entry);
    EXPECT_FALSE(unarmed_saw_an_arm_at_entry);
    // THE ARM TOOK on the thread that armed it -- otherwise the unarmed
    // thread's bit-identity below would be a statement about two unarmed runs.
    EXPECT_EQ(static_cast<int>(armed_out.status), static_cast<int>(QpStatus::kOptimal))
        << "the armed thread's arm did not take";
    EXPECT_GT(armed_switches, 0) << "the armed thread's arm changed no decision";
    // AND THE UNARMED THREAD IS ITS SOLO SELF, bit for bit, with every arm
    // counter on it at zero.
    expect_same_answer(solo_unarmed, unarmed_out);
    EXPECT_EQ(unarmed_switches, 0)
        << "the unarmed thread recorded arm activity while an armed solve ran beside it";
}

// ===========================================================================
// (E2) THE INDEPENDENT WORKING SET (M7 W2 E2): the exchange at the add site,
// the no-admissible-member rule, and the border retreat. Gill & Wong 2015,
// Results 3.3-3.5.
// ===========================================================================

namespace {

QpOptions e2_opts(WorkingSetLinearAlgebra alg, Index cap = kCycleCap) {
    QpOptions o;
    o.max_iter = cap;
    o.independent_working_set = true;
    o.ws_algebra = alg;
    return o;
}

/// `H = diag(h)`, rows `Ai x <= bi` (row-major), the box `[lo, hi]^n`.
QpProblem e2_diag_qp(const std::vector<double> &h, const std::vector<double> &g,
                     const std::vector<double> &a_rowmajor, const std::vector<double> &b, double lo,
                     double hi) {
    const auto n = static_cast<Index>(h.size());
    const auto mi = static_cast<Index>(b.size());
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(n, n);
    qp.g = Vec(n);
    for (Index i = 0; i < n; ++i) {
        Hd(i, i) = h[static_cast<std::size_t>(i)];
        qp.g(i) = g[static_cast<std::size_t>(i)];
    }
    qp.H = Hd.sparseView();
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(mi, n);
    qp.bi = Vec(mi);
    for (Index r = 0; r < mi; ++r) {
        for (Index c = 0; c < n; ++c) {
            Aid(r, c) = a_rowmajor[static_cast<std::size_t>(r * n + c)];
        }
        qp.bi(r) = b[static_cast<std::size_t>(r)];
    }
    qp.Ai = Aid.sparseView();
    qp.lower = Vec::Constant(n, lo);
    qp.upper = Vec::Constant(n, hi);
    return qp;
}

constexpr std::array<WorkingSetLinearAlgebra, 2> kE2Algebras = {
    WorkingSetLinearAlgebra::kRefactorize, WorkingSetLinearAlgebra::kSchurBorder};

const char *e2_name(WorkingSetLinearAlgebra alg) {
    return alg == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize";
}

/// THE RECURRENCE ENDS: kOptimal at the brute-force optimum, promptly, with
/// no cap and no termination. The border path retreats once on both shapes
/// (E2-F1: its Schur complement is swamped on these K0s); refactorize never.
void expect_e2_ends(const QpProblem &qp) {
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const Walk on = observed_walk(qp, e2_opts(alg));
        const QpCounters &c = on.solution.counters;
        EXPECT_EQ(static_cast<int>(on.solution.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_TRUE(is_a_kkt_point(qp, on.solution, 1e-7));
        EXPECT_NEAR(objective(qp, on.solution.x), brute_force_optimum(qp), 1e-7);
        EXPECT_LT(on.minors * 5, kCycleCap);
        // The lock's purpose (E2 fix5 R-B leaves it intact): the recurrence ends
        // in a handful of minors, and no locked member is ever unlocked.
        EXPECT_LE(on.minors, 5);
        EXPECT_EQ(c.dep_unlocks, 0);
        EXPECT_LE(on.tail, 8);
        EXPECT_EQ(c.zero_motion_cap_hits, 0);
        EXPECT_EQ(c.zero_motion_exhausted, 0);
        EXPECT_EQ(c.dep_blocker_terminations, 0);
        EXPECT_EQ(c.dep_certificate_rejections, 0);
        // RE-DERIVED AT M7 W2 E4 FIX ROUND 2 (a declared counter move). The border
        // path used to retreat ONCE here, and finding E2-F1 explained why: on these
        // K0s the border pass's Schur complement is swamped, the EQP step leaves its
        // face, the dependency coefficients are inaccurate and the pass is redone on
        // the bound-eliminated path. E4's UNREGULARIZED POLISH removes the cause --
        // it refines each bordered answer against the face's own exact system, so
        // the step does not leave its face and the dependency coefficients are the
        // eliminated path's -- and the retreat is not needed on either shape. 0 on
        // both algebras now, and the retreat's own coverage lives in
        // `TheBorderRetreatsExactlyOnceAndEndsOptimal`, re-premised the same way.
        EXPECT_EQ(c.dep_border_retreats, 0)
            << "the border retreat is back: the E2-F1 swamping the polish removes has returned";
    }
}

} // namespace

TEST(QpE2WorkingSet, ThePeriodTwoQpEndsAtItsOptimumInBothAlgebras) {
    ArmGuard guard;
    expect_e2_ends(period2_cycle_qp());
}

TEST(QpE2WorkingSet, ThePeriodFourQpEndsAtItsOptimumInBothAlgebras) {
    ArmGuard guard;
    expect_e2_ends(period4_cycle_qp());
}

TEST(QpE2WorkingSet, ADependentBlockerIsExchangedForTheMemberItsCoefficientsName) {
    // min 1/2|x|^2 - 3 x0 + 3 x1  s.t.  x0 + x1 >= 2,  [0,1] x [0,5]. x1's lower
    // bound joins at alpha = 0 beside the shifted row; x0's upper bound blocks at
    // alpha = 1/2, dependent on {row, x1} (c = e0 = -row + x1's -e1 pin, so
    // v = (-1, +1)): the exchange releases x1's pin on that same step, and the
    // walk ends at the optimum (1, 1).
    const QpProblem qp = e2_diag_qp({1.0, 1.0}, {-3.0, 3.0}, {-1.0, -1.0}, {-2.0}, 0.0, 5.0);
    QpProblem boxed = qp;
    boxed.upper(0) = 1.0;
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = plain_solve(boxed, e2_opts(alg, 40));
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_NEAR(s.x(0), 1.0, 1e-9);
        EXPECT_NEAR(s.x(1), 1.0, 1e-9);
        EXPECT_EQ(s.counters.ws_exchanges, 1);
        EXPECT_EQ(s.counters.ws_exchanges_bound, 1);
        EXPECT_EQ(s.counters.ws_exchanges_shifted, 0);
        EXPECT_EQ(s.bound_state[0], BoundState::kAtUpper);
        EXPECT_EQ(s.bound_state[1], BoundState::kFree);
        EXPECT_TRUE(is_a_kkt_point(boxed, s, 1e-7));
    }
}

TEST(QpE2WorkingSet, NoAdmissibleMemberOnAnInfeasibleQpReturnsAVerifiedCertificate) {
    // x0 + x1 >= 3 and x0 + x1 <= 1 in [0,2]^2: genuinely infeasible. The
    // second row blocks dependent on the shifted first with a NEGATIVE
    // coefficient, so no member is admissible and the certificate is built.
    const QpProblem qp =
        e2_diag_qp({1.0, 1.0}, {0.0, 0.0}, {-1.0, -1.0, 1.0, 1.0}, {-3.0, 1.0}, 0.0, 2.0);
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = plain_solve(qp, e2_opts(alg, 40));
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
        EXPECT_EQ(s.counters.dep_infeasible_certificates, 1);
        EXPECT_EQ(s.counters.dep_certificate_rejections, 0);
        const QpFarkasCertificate &f = s.farkas;
        ASSERT_TRUE(f.present);
        // Re-derived here from the problem data, not read off the engine:
        // box_part = s c - Ai^T y_i with y_i <= 0, and the implied bound.
        Vec c = Vec::Zero(qp.n());
        if (f.blocker_is_ineq) {
            c = qp.Ai.row(f.blocker).transpose();
        } else {
            c(f.blocker) = 1.0;
        }
        const Vec box_part = f.sign * c - qp.Ai.transpose() * f.y_i;
        EXPECT_LE((box_part - f.box_part).lpNorm<Eigen::Infinity>(), 1e-12);
        EXPECT_LE(f.y_i.maxCoeff(), 0.0);
        double implied = f.y_i.dot(qp.bi);
        for (Index i = 0; i < qp.n(); ++i) {
            implied += box_part(i) * (box_part(i) > 0.0 ? qp.lower(i) : qp.upper(i));
        }
        EXPECT_NEAR(implied, f.implied, 1e-12);
        EXPECT_GT(implied - f.limit, f.margin);
        EXPECT_NEAR(f.implied, 3.0, 1e-6); // coefficients from the dual_mu-regularized solve
        EXPECT_NEAR(f.limit, 1.0, 1e-12);
        EXPECT_EQ(f.residual, 0.0);
    }
}

TEST(QpE2WorkingSet, NoAdmissibleMemberAndNoPassOverEndsInsideTheBox) {
    // x0+x1+x2 <= 1 and its double 2x0+2x1+2x2 <= 2, [0,2]^3, at dual_mu =
    // 1e-2: the double blocks dependent with no admissible member, no
    // certificate, and a pass-over that would violate it. The walk ends
    // kNumericalError where it stands -- inside the box, rows satisfied,
    // never a step past the blocker. (At the default dual_mu the same QP
    // ends kOptimal at (3/4, 0, 1/4).)
    const QpProblem qp = e2_diag_qp({1.0, 1.0, 1.0}, {-2.0, -0.5, -1.5},
                                    {1.0, 1.0, 1.0, 2.0, 2.0, 2.0}, {1.0, 2.0}, 0.0, 2.0);
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        QpOptions o = e2_opts(alg, 40);
        o.dual_mu = 1e-2;
        const QpSolution s = plain_solve(qp, o);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(s.counters.dep_blocker_terminations, 1);
        EXPECT_EQ(s.counters.dep_infeasible_certificates, 0);
        EXPECT_FALSE(s.farkas.present);
        EXPECT_EQ(s.counters.dep_border_retreats,
                  alg == WorkingSetLinearAlgebra::kSchurBorder ? 1 : 0);
        for (Index i = 0; i < qp.n(); ++i) {
            EXPECT_GE(s.x(i), qp.lower(i));
            EXPECT_LE(s.x(i), qp.upper(i));
        }
        const Vec Aix = qp.Ai * s.x;
        for (Index r = 0; r < qp.mi(); ++r) {
            EXPECT_LE(Aix(r) - qp.bi(r), 1e-12);
        }
        // THE DEAD END'S POINT, RE-DERIVED at M7 W2 E4 fix round 2 (a declared
        // mover): (0.5, 0.125, 0.375) -> (0.6, 0, 0.4), to 1.2e-13 on each
        // component in both algebras. E4 reaches this dead end by a different route
        // -- every EQP answer on the way is the proximal one, polished against its
        // own face -- so the iterate the blocker terminates ON is a different point
        // of the same feasible region. EVERY CLAIM THE FIXTURE MAKES IS ASSERTED
        // ABOVE AND UNCHANGED: kNumericalError, one blocker termination, no
        // certificate, inside the box, every row satisfied, never a step past the
        // blocker. What moves is the point, and it is pinned here so the route's
        // own stability is still a test rather than an assumption.
        EXPECT_NEAR(s.x(0), 0.6, 1e-12);
        EXPECT_NEAR(s.x(1), 0.0, 1e-12);
        EXPECT_NEAR(s.x(2), 0.4, 1e-12);
    }
}

TEST(QpE2WorkingSet, TheBorderNoLongerNeedsItsRetreatAndStillEndsOptimal) {
    // RENAMED AND RE-PREMISED at M7 W2 E4 fix round 2 (ruling E4-19(c); old name
    // `TheBorderRetreatsExactlyOnceAndEndsOptimal`).
    //
    // THE DECLARED DEVIATION (settler S11) and finding E2-F1, which this fixture
    // held from E2 fix1 until E4: on the period-two QP the border pass's K0 is
    // near-singular on the pinned directions, its Schur complement carries O(1)
    // rounding, the EQP step leaves its face and the dependency coefficients are
    // inaccurate; the rule would end the solve there, so the pass was redone ONCE
    // on the bound-eliminated path and the walk ended at the optimum.
    //
    // E4's UNREGULARIZED POLISH REMOVES THE CAUSE. Every bordered answer is now
    // refined against the face's OWN exact KKT system on the factor the solve
    // already paid for (fix round 1, ruling E4-11), which is exactly the accuracy
    // the retreat was buying by re-solving; the step stays on its face, the
    // coefficients are the eliminated path's, and the retreat never fires.
    // MEASURED: `dep_border_retreats` 1 -> 0 and no `kEliminatedRetreat` record.
    // What this fixture asserts now is the ABSENCE together with the same optimal
    // outcome, so a returning retreat -- i.e. a returning E2-F1 -- fails here.
    ArmGuard guard;
    const QpProblem qp = period2_cycle_qp();
    Obs::reset();
    Obs::active = true;
    const QpSolution s = plain_solve(qp, e2_opts(WorkingSetLinearAlgebra::kSchurBorder));
    Index retreat_records = 0;
    for (const auto &r : Obs::records) {
        retreat_records +=
            r.action == hven::solvers::detail::testing::QpWalkAction::kEliminatedRetreat ? 1 : 0;
    }
    Obs::active = false;
    Obs::reset();
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_EQ(s.counters.dep_border_retreats, 0)
        << "the border retreat fired: E2-F1's swamping is back on this K0";
    EXPECT_EQ(retreat_records, 0);
    EXPECT_EQ(s.counters.dep_blocker_terminations, 0);
    EXPECT_NEAR(objective(qp, s.x), brute_force_optimum(qp), 1e-7);
}

// ===========================================================================
// (E2 FIX ROUND 1) D1-D3 of the E2-D1 diagnosis
// (docs/notes/data/2026-09-m7-w2-e2-d1-diagnosis/REPORT.md).
// ===========================================================================

namespace {

/// `H = diag(h)`, equalities `Ae x = be` and rows `Ai x <= bi` (row-major), the
/// box `[lo, hi]` per variable.
QpProblem e2_eq_qp(const std::vector<double> &h, const std::vector<double> &g,
                   const std::vector<double> &ae, const std::vector<double> &be,
                   const std::vector<double> &lo, const std::vector<double> &hi) {
    const auto n = static_cast<Index>(h.size());
    const auto me = static_cast<Index>(be.size());
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(n, n);
    qp.g = Vec(n);
    qp.lower = Vec(n);
    qp.upper = Vec(n);
    for (Index i = 0; i < n; ++i) {
        const auto si = static_cast<std::size_t>(i);
        Hd(i, i) = h[si];
        qp.g(i) = g[si];
        qp.lower(i) = lo[si];
        qp.upper(i) = hi[si];
    }
    qp.H = Hd.sparseView();
    Eigen::MatrixXd Aed(me, n);
    qp.be = Vec(me);
    for (Index r = 0; r < me; ++r) {
        for (Index c = 0; c < n; ++c) {
            Aed(r, c) = ae[static_cast<std::size_t>(r * n + c)];
        }
        qp.be(r) = be[static_cast<std::size_t>(r)];
    }
    qp.Ae = Aed.sparseView();
    qp.Ai.resize(0, n);
    qp.bi = Vec(0);
    return qp;
}

/// The largest first-order violation at `s`, equalities included: primal
/// feasibility, stationarity in hven's convention (`z` on the pins), dual
/// signs and complementarity.
double e2_kkt_residual(const QpProblem &qp, const QpSolution &s) {
    double v = 0.0;
    for (Index i = 0; i < qp.n(); ++i) {
        v = std::max({v, qp.lower(i) - s.x(i), s.x(i) - qp.upper(i)});
        if (s.z(i) > 0.0) {
            v = std::max(v, s.z(i) * std::abs(s.x(i) - qp.lower(i)));
        } else if (s.z(i) < 0.0) {
            v = std::max(v, -s.z(i) * std::abs(s.x(i) - qp.upper(i)));
        }
    }
    Vec r = qp.H.selfadjointView<Eigen::Upper>() * s.x + qp.g - s.z;
    if (qp.me() > 0) {
        v = std::max(v, (qp.Ae * s.x - qp.be).lpNorm<Eigen::Infinity>());
        r += qp.Ae.transpose() * s.lambda_e;
    }
    if (qp.mi() > 0) {
        const Vec slack = qp.bi - qp.Ai * s.x;
        for (Index j = 0; j < qp.mi(); ++j) {
            v = std::max({v, -slack(j), -s.lambda_i(j), std::abs(s.lambda_i(j) * slack(j))});
        }
        r += qp.Ai.transpose() * s.lambda_i;
    }
    return std::max(v, r.lpNorm<Eigen::Infinity>());
}

/// The certificate re-derived from the problem data, independently of the
/// engine: box_part = sign*c - Ae^T y_e - Ai^T y_i (y_i <= 0), and the implied
/// bound y_e^T be + y_i^T bi + sum_i box_part_i * (l_i or u_i) must clear the
/// blocker's limit by the stored margin.
void expect_certificate_rederives(const QpProblem &qp, const QpFarkasCertificate &f) {
    ASSERT_TRUE(f.present);
    Vec c = Vec::Zero(qp.n());
    if (f.blocker_is_ineq) {
        c = qp.Ai.row(f.blocker).transpose();
    } else {
        c(f.blocker) = 1.0;
    }
    Vec box_part = f.sign * c;
    double implied = 0.0;
    if (qp.me() > 0) {
        box_part -= qp.Ae.transpose() * f.y_e;
        implied += f.y_e.dot(qp.be);
    }
    if (qp.mi() > 0) {
        EXPECT_LE(f.y_i.maxCoeff(), 0.0);
        box_part -= qp.Ai.transpose() * f.y_i;
        implied += f.y_i.dot(qp.bi);
    }
    EXPECT_LE((box_part - f.box_part).lpNorm<Eigen::Infinity>(), 1e-12);
    for (Index i = 0; i < qp.n(); ++i) {
        if (box_part(i) != 0.0) {
            implied += box_part(i) * (box_part(i) > 0.0 ? qp.lower(i) : qp.upper(i));
        }
    }
    EXPECT_NEAR(implied, f.implied, 1e-12);
    EXPECT_GT(implied - f.limit, f.margin);
    EXPECT_EQ(f.residual, 0.0);
}

} // namespace

TEST(QpE2WorkingSet, ALockedPositiveMemberStillGetsTheVerifiedCertificate) {
    // D1 (E2-D1 REPORT, READING6's pass 2). -x0 + x1 + x2 = -3 with x2 FIXED
    // at 0 forces x1 - x0 = -3, outside [-1, 1]^2: infeasible. From the seed
    // (-1, -1) the step leaves both lower bounds at alpha = 0; x0's lower pin
    // enters first (independent of the equality) and is LOCKED at this point.
    // x1's lower pin then blocks at zero motion, dependent on {row, x0, x2}
    // with -e1 = -a + (+1)(-e0) + e2: a POSITIVE coefficient on the locked x0,
    // so no member is admissible and cond1 is false. The base (6d123e75) never
    // tried the certificate there: N3, kNumericalError (border: a retreat, then
    // N3). The box-based certificate does not need cond1: kInfeasible.
    const QpProblem qp = e2_eq_qp({1.0, 1.0, 1.0}, {1.5, 4.5, 0.0}, {-1.0, 1.0, 1.0}, {-3.0},
                                  {-1.0, -1.0, 0.0}, {1.0, 1.0, 0.0});
    QpSolution seed;
    seed.x = Vec(3);
    seed.x << -1.0, -1.0, 0.0;
    seed.bound_state = {BoundState::kFree, BoundState::kFree, BoundState::kFixed};
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpEngine engine{e2_opts(alg, 40)};
        const QpSolution s = engine.solve(qp, seed);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
        EXPECT_EQ(s.counters.dep_infeasible_certificates, 1);
        EXPECT_EQ(s.counters.dep_blocker_terminations, 0);
        EXPECT_EQ(s.counters.dep_certificate_rejections, 0);
        EXPECT_EQ(s.counters.ws_exchanges, 0);
        EXPECT_EQ(s.counters.dep_border_retreats, 0);
        EXPECT_EQ(s.counters.minor_iters, 2);
        // No step past the blocker: the walk stands on the seed.
        EXPECT_EQ(s.x(0), -1.0);
        EXPECT_EQ(s.x(1), -1.0);
        const QpFarkasCertificate &f = s.farkas;
        expect_certificate_rederives(qp, f);
        EXPECT_FALSE(f.blocker_is_ineq);
        EXPECT_EQ(f.blocker, 1);
        EXPECT_EQ(f.sign, -1.0);
        EXPECT_NEAR(f.implied, 2.0, 1e-6); // y_e = -1: 3 + (-1)*u0 on the box
        EXPECT_NEAR(f.limit, 1.0, 1e-12);  // -l1
    }
}

TEST(QpE2WorkingSet, TheUndecidableBorderPingPongRetreatsOnceAndEndsOptimal) {
    // D3(a) (E2-D1 REPORT, STEENBRB/C). x1's lower pin blocks at zero motion,
    // exactly dependent on the equality -x1 + x4 = 1 with x4 pinned; on the
    // border path its test stalls at rho = 1.2e-6 (UNDECIDABLE). The base
    // admitted it as a deferred add, the next pass's detector removed it, it
    // re-blocked at alpha = 0: nine times, to the zero-motion cap
    // (kNumericalError). An undecidable border test now takes the border
    // retreat and the eliminated path decides it (dependent, exchanged): both
    // algebras end at the same KKT point.
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(5, 5);
    Hd(0, 1) = -0.25;
    Hd(0, 3) = 0.5;
    Hd(0, 4) = -0.5;
    Hd(1, 1) = 0.25;
    Hd(1, 3) = -0.5;
    Hd(1, 4) = 0.25;
    Hd(3, 3) = 1.0;
    Hd(3, 4) = -0.5;
    qp.H = Hd.sparseView(); // upper triangle
    qp.g = Vec(5);
    qp.g << -0.5, -1.0, 0.0, 1.5, -1.5;
    Eigen::MatrixXd Ae = Eigen::MatrixXd::Zero(1, 5);
    Ae(0, 1) = -1.0;
    Ae(0, 4) = 1.0;
    qp.Ae = Ae.sparseView();
    qp.be = Vec::Constant(1, 1.0);
    Eigen::MatrixXd Ai(3, 5);
    Ai << -1, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, 1, 0, -1, 0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec(3);
    qp.bi << 1.5, 1.0, 0.0;
    qp.lower = Vec::Zero(5);
    qp.upper = Vec::Ones(5);
    qp.upper(0) = 0.0; // x0 fixed
    Vec x_ref;
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = plain_solve(qp, e2_opts(alg, 200));
        const QpCounters &c = s.counters;
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_LE(e2_kkt_residual(qp, s), 1e-9);
        EXPECT_EQ(c.zero_motion_cap_hits, 0);
        EXPECT_EQ(c.dep_deferred_adds, 0);
        EXPECT_EQ(c.dep_recheck_removals, 0);
        EXPECT_EQ(c.dep_known_dependent_refusals, 0);
        EXPECT_EQ(c.ws_exchanges, 1);
        EXPECT_LE(c.minor_iters, 8);
        if (alg == WorkingSetLinearAlgebra::kSchurBorder) {
            EXPECT_EQ(c.dep_undecidable, 1);
            EXPECT_EQ(c.dep_border_retreats, 1);
        } else {
            EXPECT_EQ(c.dep_undecidable, 0);
            EXPECT_EQ(c.dep_border_retreats, 0);
        }
        if (x_ref.size() == 0) {
            x_ref = s.x;
        } else {
            EXPECT_LE((s.x - x_ref).lpNorm<Eigen::Infinity>(), 1e-12);
        }
    }
    EXPECT_NEAR(x_ref(4), 1.0, 1e-12);
}

TEST(QpE2WorkingSet, AFullyPinnedRowDecidesAnUndecidablePinExactly) {
    // D3(c) (E2-D1 REPORT, READING6-shaped). x0 - x2 = 0.5 with x0 FIXED at 0
    // forces x2 = -0.5, below its lower bound 0: infeasible. H is indefinite;
    // on the border path the first pass holds no usable factor, so x2's lower
    // pin's test DOES NOT RUN -- and it completes the equality row on pinned
    // columns, an EXACT dependency read off the row (-e2 = a - e0). The base
    // deferred it, re-checked, retreated and ended N3 (6 minors); now the
    // certificate verifies on the first pass. The refactorize path's test
    // decides it numerically and reaches the same certificate.
    //
    // COUNTER PINS RE-DERIVED, M7 W2 E2 FIX ROUND 7 (a declared mover;
    // settler E4-37(b)): on the BORDER arm `dep_undecidable` 1 -> 0 and
    // `dep_structural_exact` 1 -> 0, replaced by `dep_structural_first` 0 -> 1.
    // `dep_unrun_tests` stays 0 in BOTH arms: it is counted after R-A, and R-A did
    // decide this one.
    // An un-run test is no longer a verdict of dependence, so the pin reaches
    // R-A INSIDE test_candidate -- which applies the same exact row reading one
    // step earlier -- instead of the add site's own D3(c) branch. EVERY
    // CORRECTNESS STATEMENT OF THIS FIXTURE IS UNCHANGED and is asserted
    // unchanged below: kInfeasible, one verified certificate, blocker 2,
    // y_e(0) = 1, implied 0.5, limit 0, ONE minor, no deferral, no retreat.
    // Only which counter records the exact decision moved.
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(4, 4);
    Hd(0, 0) = 0.75;
    Hd(0, 2) = -1.0;
    Hd(0, 3) = 0.5;
    Hd(2, 2) = 1.0;
    Hd(2, 3) = -1.0;
    qp.H = Hd.sparseView(); // upper triangle
    qp.g = Vec(4);
    qp.g << -0.5, -0.5, 0.0, -1.5;
    Eigen::MatrixXd Ae = Eigen::MatrixXd::Zero(1, 4);
    Ae(0, 0) = 1.0;
    Ae(0, 2) = -1.0;
    qp.Ae = Ae.sparseView();
    qp.be = Vec::Constant(1, 0.5);
    Eigen::MatrixXd Ai(2, 4);
    Ai << 0, 0, -1, 1, 1, 1, 0, 0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec(2);
    qp.bi << -0.0, 0.5;
    qp.lower = Vec::Zero(4);
    qp.upper = Vec::Ones(4);
    qp.upper(0) = 0.0; // x0 fixed
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = plain_solve(qp, e2_opts(alg, 40));
        const QpCounters &c = s.counters;
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
        EXPECT_EQ(c.dep_infeasible_certificates, 1);
        EXPECT_EQ(c.dep_blocker_terminations, 0);
        EXPECT_EQ(c.dep_deferred_adds, 0);
        EXPECT_EQ(c.dep_border_retreats, 0);
        EXPECT_EQ(c.zero_motion_cap_hits, 0);
        const QpFarkasCertificate &f = s.farkas;
        expect_certificate_rederives(qp, f);
        EXPECT_FALSE(f.blocker_is_ineq);
        EXPECT_EQ(f.blocker, 2);
        EXPECT_EQ(c.dep_undecidable, 0);
        EXPECT_EQ(c.dep_structural_exact, 0);
        // NOT dep_unrun_tests: that counter is read AFTER R-A, and R-A decided this
        // candidate EXACTLY off the row, so the admission rests on no uncomputed rho
        // (QpEngine::test_candidate). It is the count of verdicts the screen alone
        // carries, not of passes that held no factor.
        EXPECT_EQ(c.dep_unrun_tests, 0);
        if (alg == WorkingSetLinearAlgebra::kSchurBorder) {
            EXPECT_EQ(c.dep_structural_first, 1);
            // RE-DERIVED AGAIN, M7 W2 E2 FIX ROUND 8 (F7-1): 1 -> 2 minors. The first pass's
            // repair pins and undoes itself, and that pass is now SPENT rather than run on
            // the `face`/border state its probes left behind (dep_repair_restarts 1); the
            // certificate is reached on the pass after it. Every other statement here --
            // kInfeasible, ONE verified certificate, the blocker, y_e, implied, limit -- is
            // unchanged, which is the point: the walk takes one more pass to say the same
            // thing, on a factor it can actually test against.
            EXPECT_EQ(c.dep_repair_restarts, 1);
            EXPECT_EQ(c.minor_iters, 2);
            // Read off the row, exactly: -e2 = a - e0, so y_e = 1 and the
            // implied bound is be - u0 = 0.5 against -l2 = 0.
            EXPECT_EQ(f.y_e(0), 1.0);
            EXPECT_EQ(f.implied, 0.5);
            EXPECT_EQ(f.limit, 0.0);
        } else {
            EXPECT_EQ(c.dep_structural_first, 0);
        }
    }
}

TEST(QpE2WorkingSet, ATolerancedCertificateOnAnUnboundedCoordinateEndsNumericalErrorInsideTheBox) {
    // E2 fix2, settler S25. Rows -x0 <= -1 and x0 + e x1 <= 0, e = 2^-21
    // (exact), x0 in [-10, 10], x1 FREE (no finite bound on either side).
    // From the seed (1, -2^22) with row 0 active the step runs along +e1 and
    // row 1 blocks at alpha = 1/2, dependent on row 0 to rho = e/||c|| <
    // kDepTol with a NEGATIVE coefficient: nothing admissible. Its certificate
    // (y_1 = -1 on row 0) leaves box_part = (0, e): the e sits on x1, which is
    // unbounded, so verify_farkas drops it (residual = e) and verifies. That is
    // a proof for the PERTURBED QP (e = 0: x0 >= 1 and x0 <= 0), NOT for this
    // one, which is feasible -- (1, -2^21) satisfies both rows exactly. The walk
    // ends kNumericalError where it stands, the certificate returned.
    const double e = std::ldexp(1.0, -21);
    const double inf = std::numeric_limits<double>::infinity();
    QpProblem qp =
        e2_diag_qp({1.0, 1.0}, {0.0, 0.0}, {-1.0, 0.0, 1.0, e}, {-1.0, 0.0}, -10.0, 10.0);
    qp.lower(1) = -inf;
    qp.upper(1) = inf;
    QpSolution seed;
    seed.x = Vec(2);
    seed.x << 1.0, -std::ldexp(1.0, 22);
    seed.bound_state = {BoundState::kFree, BoundState::kFree};
    seed.ineq_active = {true, false};

    // The data, independently of the engine: the original QP is FEASIBLE.
    Vec xf(2);
    xf << 1.0, -std::ldexp(1.0, 21);
    const Vec axf = qp.Ai * xf;
    ASSERT_LE(axf(0), qp.bi(0));
    ASSERT_LE(axf(1), qp.bi(1));

    // min over the box of b^T x, with every entry of b included (no drop).
    const auto box_min = [](const QpProblem &p, const Vec &b) {
        double m = 0.0;
        for (Index i = 0; i < p.n(); ++i) {
            if (b(i) != 0.0) {
                m += b(i) * (b(i) > 0.0 ? p.lower(i) : p.upper(i));
            }
        }
        return m;
    };
    // The certificate's inequality s c^T x >= y^T bi + min_box(box_part), for
    // the problem `p`, re-derived from p's own rows.
    const auto implied_for = [&](const QpProblem &p, const QpFarkasCertificate &f) {
        Vec c = p.Ai.row(f.blocker).transpose();
        const Vec box_part = f.sign * c - p.Ai.transpose() * f.y_i;
        return f.y_i.dot(p.bi) + box_min(p, box_part);
    };

    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpEngine engine{e2_opts(alg, 40)};
        const QpSolution s = engine.solve(qp, seed);
        const QpCounters &c = s.counters;
        // (c) the verdict: kNumericalError, inside the box, rows satisfied.
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(c.dep_certificate_toleranced, 1);
        EXPECT_EQ(c.dep_infeasible_certificates, 0);
        EXPECT_EQ(c.dep_blocker_terminations, 0);
        EXPECT_EQ(c.dep_certificate_rejections, 0);
        EXPECT_EQ(c.dep_border_retreats, alg == WorkingSetLinearAlgebra::kSchurBorder ? 1 : 0);
        for (Index i = 0; i < qp.n(); ++i) {
            EXPECT_GE(s.x(i), qp.lower(i));
            EXPECT_LE(s.x(i), qp.upper(i));
        }
        EXPECT_LE((qp.Ai * s.x - qp.bi).maxCoeff(), 0.0);
        EXPECT_EQ(s.x(0), 1.0);
        EXPECT_EQ(s.x(1), -std::ldexp(1.0, 22)) << "no step past the blocker";

        // (b) the toleranced certificate, returned with its residual.
        const QpFarkasCertificate &f = s.farkas;
        ASSERT_TRUE(f.present);
        EXPECT_TRUE(f.blocker_is_ineq);
        EXPECT_EQ(f.blocker, 1);
        EXPECT_EQ(f.residual, e);
        EXPECT_GT(f.residual, f.exact_tol) << "above the rounding floor (S25')";
        // Refined mu-free (S25'): -1 to within the floor, not O(mu).
        EXPECT_LE(std::abs(f.y_i(0) + 1.0), f.exact_tol);
        EXPECT_EQ(f.y_i(1), 0.0);
        EXPECT_EQ(f.box_part(1), e);

        // (a) the ORIGINAL QP: the full box_part keeps e on the unbounded x1,
        // so the implied bound is -inf -- no certificate.
        EXPECT_EQ(implied_for(qp, f), -inf);
        // The COEFFICIENT-PERTURBED QP (the dropped e zeroed): the same
        // multipliers prove it infeasible, box_part zero on x1.
        QpProblem perturbed = qp;
        perturbed.Ai.coeffRef(1, 1) = 0.0;
        perturbed.Ai.prune(0.0);
        const double implied_p = implied_for(perturbed, f);
        EXPECT_TRUE(std::isfinite(implied_p));
        EXPECT_NEAR(implied_p, 1.0, 1e-6);
        EXPECT_GT(implied_p - perturbed.bi(1), f.margin);
        EXPECT_NEAR(implied_p, f.implied, 1e-12);
    }
}

TEST(QpE2WorkingSet, AnExactlyDependentBlockerOnFreeVariablesIsCertifiedInfeasibleAfterRefinement) {
    // E2 fix2, settler S25'. x in R^3, every coordinate FREE (no finite bound).
    // Equalities x0 + x2 = 1 and x1 + x2 = 1; the row x0 + x1 + 2 x2 <= 1 is
    // EXACTLY their sum (a two-row dependency, so the single-row exact
    // coefficient cannot apply: the mu-free refinement alone decides), and the
    // sum forces 2 > 1 -- infeasible. The dual_mu-regularized solve leaves the
    // multipliers O(mu) off (measured 8.2e-9 on the free coordinates, E2 README
    // fix-round-2): without the refinement, every such entry would sit on an
    // unbounded coordinate and the certificate would be toleranced
    // (kNumericalError). Refined, the residual is within the rounding floor:
    // kInfeasible, y_e = (1, 1) to the floor.
    const double inf = std::numeric_limits<double>::infinity();
    QpProblem qp = e2_diag_qp({1.0, 1.0, 1.0}, {0.0, 0.0, 0.0}, {1.0, 1.0, 2.0}, {1.0}, -inf, inf);
    Eigen::MatrixXd Aed(2, 3);
    Aed << 1.0, 0.0, 1.0, 0.0, 1.0, 1.0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(2, 1.0);

    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpEngine engine{e2_opts(alg, 40)};
        const QpSolution s = engine.solve(qp);
        const QpCounters &c = s.counters;
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
        EXPECT_EQ(c.dep_infeasible_certificates, 1);
        EXPECT_EQ(c.dep_certificate_toleranced, 0);
        const QpFarkasCertificate &f = s.farkas;
        ASSERT_TRUE(f.present);
        EXPECT_TRUE(f.blocker_is_ineq);
        EXPECT_EQ(f.blocker, 0);
        EXPECT_GT(f.exact_tol, 0.0);
        EXPECT_LE(f.residual, f.exact_tol);
        ASSERT_EQ(f.y_e.size(), 2);
        EXPECT_LE(std::abs(f.y_e(0) - 1.0), f.exact_tol);
        EXPECT_LE(std::abs(f.y_e(1) - 1.0), f.exact_tol);
        EXPECT_LE(f.box_part.lpNorm<Eigen::Infinity>(), f.exact_tol);
        // The certificate's inequality, re-derived: c^T x >= y_e^T be = 2 > 1.
        EXPECT_NEAR(f.implied, 2.0, 1e-12);
        EXPECT_GT(f.implied - f.limit, f.margin);
    }
}

// ===========================================================================
// (E2 fix3) THE E2-D4 RULES ON THE DUMPED QPs THAT MOTIVATED THEM. Each QP is
// a walk solve dumped from a TRAIN cell (tests/qp/fixtures/e2-d4/, format in
// FORMAT.txt there; the 32-dump corpus and its oracle are in
// docs/notes/data/2026-09-m7-w2-e2-d4-feasible-qps/). The dump's own `result`
// line is the in-cell outcome at the pre-rule walk, asserted as the premise.
// Both algebras run; where a rule fires on one algebra only, that is said.
// ===========================================================================

namespace {

using hven::test_support::QpDump;

QpDump e2d4_dump(const char *name) {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + name);
}

QpSolution e2d4_solve(const QpDump &d, WorkingSetLinearAlgebra alg) {
    QpOptions o = d.opts;
    o.ws_algebra = alg;
    return hven::test_support::solve_qp_dump(d, o);
}

/// kOptimal with the KKT conditions recomputed from the QP (qp_dump.h): the walk's
/// row-scaled feasibility leaves primal residuals of a few 1e-7 on these rows (the
/// E2-OFF walk lands on the same points), so primal <= 1e-5, dual <= 1e-6.
void expect_e2d4_kkt(const QpDump &d, const QpSolution &s) {
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    EXPECT_LE(k.eq, 1e-5);
    EXPECT_LE(k.ineq, 1e-5);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-6);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-6);
}

} // namespace

TEST(QpE2WorkingSet, AStructuralDependencyDecidesFirstOnTheSteenbrbElasticQp) {
    // STEENBRB's elastic QP at major 0 (n 492, me 96; nonconvex H). In the cell the
    // numerical test admitted pin 407 as independent (rho 5.7e10, a diverged
    // refinement) although it completes equality row 63 on pinned columns; the set
    // leaked and the solve ended in zero-motion exhaustion. With R-A the structural
    // dependency decides first. Only the border path takes that admission (the
    // refactorize walk reaches kOptimal with or without R-A).
    const QpDump d = e2d4_dump("STEENBRB_def-qp0.txt");
    EXPECT_NE(d.result.find("status=3"), std::string::npos);
    EXPECT_NE(d.result.find("zmexh=1"), std::string::npos);
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = e2d4_solve(d, alg);
        expect_e2d4_kkt(d, s);
        EXPECT_EQ(s.counters.zero_motion_exhausted, 0);
        EXPECT_EQ(s.counters.zero_motion_cap_hits, 0);
#ifdef NDEBUG
        // Release only: Debug's arithmetic never makes that admission on this QP
        // (measured dep_structural_first 0 there, kOptimal all the same).
        //
        // UNCHANGED AT M7 W2 E4 FIX ROUND 2, and the round measured it twice. An
        // intermediate version of the step's row-drift check (fix round 2, without
        // its kEqpRowDriftFloor) escalated the suspect ladder on this dump and the
        // admission stopped happening -- `dep_structural_first` 0, a non-vacuity
        // premise lost. The floor, which is there for its own reasons (see the
        // constant's note), restores it: the premise holds at the shipped engine.
        if (alg == WorkingSetLinearAlgebra::kSchurBorder) {
            EXPECT_GE(s.counters.dep_structural_first, 1);
        }
#endif
    }
}

TEST(QpE2WorkingSet, TheLockedVertexUnlocksOnTheAcopr57PrimalQps) {
    // ACOPR57's primary QPs at trust radii 1 and 0.5 (n 128, me 114, mi 274): E2-D4
    // class A, the locked degenerate vertex. The shift-seeded over-count is trimmed
    // one member per pass; the trimmed pins re-block at alpha = 0 against members
    // that all entered at this point, the lock leaves no admissible member, and
    // without R-B the solve ends N3 (kNumericalError, dep_blocker_terminations) on
    // both algebras. With R-B (E2 fix5, the locked-vertex unlock; ExchangePick) the
    // locked members become admissible under the min-ratio rule and the walk reaches
    // the optimum. f* is Clarabel's on the dumped QP (E2-D4 oracle), and the E2-OFF
    // walk returns the same objective. The entry detector stalls on the shift-seeded
    // over-count here (dep_untested_growth, trimmed by count); the S3 Debug assert on
    // it was downgraded to that counter (settler S27), so this runs in both
    // configurations.
    //
    // RE-PREMISED AT M7 W2 E4 FIX ROUND 2, AND THE RE-PREMISE IS A STRONGER CLAIM.
    // The cross-arm pin `|f - f_off| <= 1e-7 * |f_off|` said "the two arms agree",
    // which was worth something while the OFF arm was the better-measured of the
    // two. It is not any more: on qp0 the E2-ON arm now lands on CLARABEL'S OWN
    // OPTIMUM to 2.3e-12 relative (432.674974173 against f* = 432.674974172, with
    // the equality residual 6.14e-07 -> 1.20e-14), while the E2-OFF arm returns
    // 432.660942518 -- 3.2e-05 relative off the oracle, inside its own declared
    // 1e-4 and no longer inside 1e-7 of the production answer. The E2-off path is
    // the harness CONTROL lane and is declared MOVING this round (E4-23), so the
    // authority the two arms are measured against is the ORACLE, not each other.
    // What replaces the cross-arm equality: each arm within its declared tolerance
    // of f*, AND the ON arm no further from f* than the OFF arm -- which is the
    // claim that actually protects production, and which the equality never made.
    // `on_rel_tol` is MEASURED per cell, two to three decades above what the arm
    // reads, and it is two to four decades tighter than the pin it replaces.
    struct Case {
        const char *file;
        double f_star;
        double rel_tol;
        double on_rel_tol;
    };
    for (const Case &c : {Case{"ACOPR57_def-qp0.txt", 432.674974172, 1e-4, 1e-9},
                          Case{"ACOPR57_def-qp1.txt", 501.805584074, 1e-5, 1e-7}}) {
        SCOPED_TRACE(c.file);
        const QpDump d = e2d4_dump(c.file);
        EXPECT_NE(d.result.find("status=3"), std::string::npos);
        EXPECT_NE(d.result.find("blk_term=1"), std::string::npos);
        QpOptions off = d.opts;
        off.independent_working_set = false;
        const QpSolution s_off = hven::test_support::solve_qp_dump(d, off);
        ASSERT_EQ(static_cast<int>(s_off.status), static_cast<int>(QpStatus::kOptimal));
        const double f_off = hven::test_support::qp_dump_objective(d, s_off.x);
        // BOTH ARMS ARE HELD TO `on_rel_tol` NOW, not just the production one: the
        // E2-off control reaches the oracle to 2.2e-12 (qp0) and 7.2e-10 (qp1)
        // relative, so the declared `rel_tol` of 1e-4 / 1e-5 it used to be measured
        // against is kept only as the looser of the two bounds. MEASURED at E4 fix
        // round 2, all four arms: qp0 on 2.2e-12 / off 2.2e-12, qp1 on 1.0e-09 /
        // off 7.2e-10 relative.
        EXPECT_LE(std::abs(f_off - c.f_star), c.on_rel_tol * std::abs(c.f_star));
        EXPECT_LE(std::abs(f_off - c.f_star), c.rel_tol * std::abs(c.f_star));
        for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
            SCOPED_TRACE(e2_name(alg));
            const QpSolution s = e2d4_solve(d, alg);
            expect_e2d4_kkt(d, s);
            const double f = hven::test_support::qp_dump_objective(d, s.x);
            EXPECT_LE(std::abs(f - c.f_star), c.on_rel_tol * std::abs(c.f_star))
                << "the production arm left the oracle's optimum";
            EXPECT_GE(s.counters.dep_unlocks, 1);
            EXPECT_EQ(s.counters.dep_blocker_terminations, 0);
            EXPECT_EQ(s.counters.zero_motion_cap_hits, 0);
        }
    }
}

TEST(QpE2WorkingSet, AMuFreeInertiaReReadCertifiesTheSingularPsdFaceOfAcopp57) {
    // ACOPP57's primary QP at major 3 (n 128, me 114, mi 160). The walk's final face
    // IS the optimal face, but Z'HZ there is positive SEMIdefinite (lambda_min
    // +3.0e-12, next 50.4): the mu-regularized inertia reads kWrong and the saddle
    // exit ended the solve in 2 minors. The mu-free re-read reads kSuspect, the
    // suspect-stall gate certifies stationarity, kOptimal. f* = Clarabel's; the
    // walk's point is 1.7e-4 from x* and carries a 4e-7 equality residual on rows
    // with curvature ~7e5, which is the -1.1e-3 in f (both algebras alike). The
    // border path takes the kWrong; the refactorize walk reads kOk already.
    const QpDump d = e2d4_dump("ACOPP57_def-qp0.txt");
    EXPECT_NE(d.result.find("status=3 minors=2 "), std::string::npos);
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = e2d4_solve(d, alg);
        expect_e2d4_kkt(d, s);
        EXPECT_LE(std::abs(hven::test_support::qp_dump_objective(d, s.x) - 0.243742606211),
                  1e-2 * 0.243742606211);
        EXPECT_LE(s.counters.minor_iters, 12);
        EXPECT_EQ(s.counters.dep_inertia_reread_flips, s.counters.dep_inertia_rereads);
        if (alg == WorkingSetLinearAlgebra::kSchurBorder) {
            EXPECT_GE(s.counters.dep_inertia_reread_flips, 1);
        }
    }
}

TEST(QpE2WorkingSet, AMuFreeSuspectReadDoesNotCertifyASaddle) {
    // min 1/2 (x1^2 + x2^2 - x3^2 + 0.75 x0^2) - x0 x2 + 0.5 x0 x3 - x2 x3 on
    // [-1, 1]^3 with x0 FIXED at 0 and x0 - x2 = 0. At x = 0 the face is {x1, x3}
    // with Z'HZ = diag(1, -1): a saddle, stationary, genuinely indefinite; the
    // minimizers are x3 = +-1, f = -1/2 (the option-off walk finds one). The
    // regularized start verdict is kWrong; the mu-free eliminated factor reads
    // kSuspect (a perturbed zero pivot), which R-C as first ruled let replace the
    // kWrong -- no start repair, the suspect-stall gate saw a stationary point,
    // and the walk returned kOptimal AT THE SADDLE (f = 0) in both algebras. A
    // kSuspect mu-free read must not replace a kWrong on its own.
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(4, 4);
    Hd(0, 0) = 0.75;
    Hd(0, 2) = -1.0;
    Hd(0, 3) = 0.5;
    Hd(1, 1) = 1.0;
    Hd(2, 2) = 1.0;
    Hd(2, 3) = -1.0;
    Hd(3, 3) = -1.0;
    qp.H = Hd.sparseView(); // upper triangle
    qp.g = Vec::Zero(4);
    Eigen::MatrixXd Ae = Eigen::MatrixXd::Zero(1, 4);
    Ae(0, 0) = 1.0;
    Ae(0, 2) = -1.0;
    qp.Ae = Ae.sparseView();
    qp.be = Vec::Zero(1);
    qp.Ai.resize(0, 4);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(4, -1.0);
    qp.upper = Vec::Constant(4, 1.0);
    qp.lower(0) = 0.0;
    qp.upper(0) = 0.0;
    for (const bool e2 : {false, true}) {
        for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
            SCOPED_TRACE(e2 ? "on" : "off");
            SCOPED_TRACE(e2_name(alg));
            QpOptions o = e2_opts(alg, 40);
            o.independent_working_set = e2;
            const QpSolution s = plain_solve(qp, o);
            EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
            EXPECT_NEAR(objective(qp, s.x), -0.5, 1e-12);
            EXPECT_NEAR(std::abs(s.x(3)), 1.0, 1e-12);
            EXPECT_EQ(s.counters.dep_inertia_reread_flips, 0);
            EXPECT_EQ(s.counters.dep_inertia_rereads, e2 ? 1 : 0);
        }
    }
}

// ===========================================================================
// (E2 fix6) R-E, THE GREY RE-READ, ON THE DUMPED QPs (settler S33). A candidate
// test that converged to "independent" with kDepTol < rho < kDepUndecidable is
// re-read with the refinement run to its end on the same factor; rho' plus its
// rounding floor <= kDepTol is a certified dependent (QpEngine::test_candidate;
// the theorem is in qp_engine.h, Degeneracy). The F1 design note's dense check
// (the relative distance of the candidate from the working rows' span, QR on the
// free columns) is the truth these fixtures are read against.
// ===========================================================================

TEST(QpE2WorkingSet, AGreyReReadSolvesTheAcopr57RestorationQp) {
    // ACOPR57's restoration QP (n 630, me 114, mi 274; nonconvex H, phase-1
    // feasible). In the cell and until E2 fix6 the walk admitted grey
    // dependents (dense distance ~1e-12 and below, rho 1e-6..1e-4 from a
    // converged mu-solve), the set leaked and the solve ended N3
    // (kNumericalError, 26 minors). R-C' could not certify it (its mu-free read
    // is kSuspect; the gap registered at fix round 3). With R-E the grey
    // dependents are read dependent (34 of 36 re-reads flip; the two kept
    // re-read above kDepTol) and the walk reaches the strict local minimum
    // f = -18.3950535 (Z'HZ min 0.73; the value R-C as first ruled reached) on
    // both algebras, with the KKT conditions recomputed from the QP.
    const QpDump d = e2d4_dump("ACOPR57_def-qp4.txt");
    EXPECT_NE(d.result.find("status=3 minors=27 "), std::string::npos);
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = e2d4_solve(d, alg);
        expect_e2d4_kkt(d, s);
        EXPECT_NEAR(hven::test_support::qp_dump_objective(d, s.x), -18.3950535, 1e-6);
        EXPECT_GE(s.counters.dep_grey_reread_dependent, 1);
        EXPECT_EQ(s.counters.dep_blocker_terminations, 0);
    }
}

TEST(QpE2WorkingSet, AConvergedGreyPinOnDallasmIsReReadDependent) {
    // DALLASM's primary QP at trust radius 1 (n 196, me 150; the network rows are
    // rank-deficient). Its first grey admission until E2 fix6 was pin 98 at pass 9
    // (border): rho 3.4e-6, "converged" at ZERO refinement steps (the mu-solve's
    // residual 1e-8 is under the target 1e-7), dense distance ~7e-16; the re-read
    // reads rho' 1e-17 (Release, border). Every grey re-read on this QP flips, and
    // every flip was dense-checked dependent (the F1 design note's corpus run). The walk still ends
    // short of the optimum (the degenerate vertex, 112 active bounds and 46 dof: E2-D4 class A, not
    // this rule), so the status is not pinned; a kOptimal must pass the recomputed KKT check.
    const QpDump d = e2d4_dump("DALLASM_def-qp0.txt");
    EXPECT_NE(d.result.find("status=3 minors=64 "), std::string::npos);
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = e2d4_solve(d, alg);
        if (s.status == QpStatus::kOptimal) {
            expect_e2d4_kkt(d, s); // a kOptimal here must be a true one
        }
        EXPECT_GE(s.counters.dep_grey_rereads, 1);
        EXPECT_EQ(s.counters.dep_grey_reread_dependent, s.counters.dep_grey_rereads);
    }
}

TEST(QpE2WorkingSet, AGreyPinOnYorknetThatIsIndependentStaysIndependent) {
#ifndef NDEBUG
    // A DECLARED DEBUG SKIP (M7 W2 E4 fix round 2, ruling E4-22), with its cost.
    //
    // THIS TEST COSTS 2681.90 s IN DEBUG -- nine times the next slowest test in the
    // Debug suite and four times M6's 600 s per-fixture budget. It is 37.35 s in
    // Release, where it runs and asserts everything below.
    //
    // THE COST IS NOT THIS ROUND'S, and that is measurable: its Release time moved
    // 35.94 s -> 37.35 s across M7 W2 E4, so at this tree's ~72x Debug/Release factor
    // on this dump it was already ~2590 s before the round. What IS this round's is
    // that the number is KNOWN: fix round 1 is the first round to have run the whole
    // Debug suite, and it found this.
    //
    // WHY A SKIP AND NOT A SPLIT: the test solves ONE dump (YORKNET_def-qp1) on BOTH
    // algebras and reads a grey-band re-read that happens on the ELIMINATED path, so
    // splitting it per algebra halves the cost at best and leaves a ~1300 s Debug
    // case. REGISTERED: split the dump itself, or give this cell a smaller one.
    //
    // WHAT THE SKIP COSTS, stated: the grey-band re-read's Debug arithmetic is not
    // observed. The rule it checks (R-E: a grey reading re-read on the same factor
    // stays independent when its refined rho does not fall under kDepTol) has its
    // synthetic fixtures earlier in this file, and they run in both configurations.
    GTEST_SKIP() << "Debug costs 2681.90 s on this dump (Release 37.35 s), four times M6's 600 s "
                    "per-fixture budget and nine times the next slowest Debug test; asserted in "
                    "Release, and R-E's synthetic fixtures cover the rule in both configurations";
#endif
    // YORKNET's restoration QP (272 free columns and 256 working rows at the event). Its one grey
    // reading is pin 36 at pass 219 (eliminated path): rho 7.669e-4 converged,
    // and the dense distance IS 7.669e-4 -- an independent candidate that E2-D4
    // had inferred dependent. The re-read converges to the same rho' (>= the
    // distance for every coefficient vector), so it stays independent: R-E does
    // not flip it, and the walk is the fix-5 walk (596 minors in Release,
    // kNumericalError -- not this rule's class).
    const QpDump d = e2d4_dump("YORKNET_def-qp1.txt");
    EXPECT_NE(d.result.find("status=3 minors=594 "), std::string::npos);
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = e2d4_solve(d, alg);
        EXPECT_EQ(s.counters.dep_grey_rereads, 1);
        EXPECT_EQ(s.counters.dep_grey_reread_dependent, 0);
    }
}

// ===========================================================================
// (M7 W2 NC1) THE DECLINED RIDE (qp_engine.h section 4c). A drop onto a
// negative-curvature face never falls through to a step that re-enters the
// released constraint. On STEENBRB's post-restoration QPs (R1 dumps
// Cs_STEENBRB_qp3-qp6, n 468, me 96, radius 0.1 / 0.05 / 0.025 / 0.0125) the
// walk drops pin 312 from its upper bound (multiplier 4.99e-3) onto a face with
// curvature -1.27e-6; the ride's first blocker is a pin at zero motion (pin 128
// on qp3, 362 on qp4-qp6) with rate -2.5e-12 along a direction of norm 3.9e3,
// and it tests DEPENDENT (rho 1.6e-12: e_i = e_j + an equality row). Before
// NC1 the ride declined, the ordinary capped step re-entered pin 312 at
// alpha = 0, and the solve ended in zero-motion exhaustion. With NC1 the
// dependent ride blocker takes the add site's exchange and the ride is re-taken
// on the exchanged set: kOptimal at the option-off walk's objective.
// ===========================================================================

namespace {

// `expect_exchange` RE-PREMISED at M7 W2 E4 fix round 2 (ruling E4-19(b): the old
// premise stays where the pathology remains, and it remains on qp4-qp6). NC1's
// mechanism needs a DEPENDENT ride blocker at zero motion, which needs the rows'
// dual_mu footprint to put one there; E4's unregularized polish backs that
// footprint out of every EQP answer, and on qp3 the blocker is gone -- the walk
// reaches the same optimum with `dep_ride_blockers` and `ride_exchanges` at 0.
// qp4, qp5 and qp6 still reach it and still assert the exchange.
void expect_the_declined_ride_resolves(const char *file, double f_off, Index off_minors,
                                       bool expect_exchange = true) {
    const QpDump d = e2d4_dump(file);
    // The in-cell E2-on outcome (the pre-NC1 walk): stuck, zero-motion exhaustion.
    EXPECT_NE(d.result.find("status=3"), std::string::npos);
    EXPECT_NE(d.result.find("zmexh=1"), std::string::npos);
#ifdef NDEBUG
    // The option-off walk is untouched (N3): kOptimal at the R1 replay's minors
    // (Release only: Debug's arithmetic takes a different, equally optimal walk).
    {
        QpOptions off = d.opts;
        off.independent_working_set = false;
        const QpSolution s_off = hven::test_support::solve_qp_dump(d, off);
        ASSERT_EQ(static_cast<int>(s_off.status), static_cast<int>(QpStatus::kOptimal));
        // `off_minors` RE-DERIVED, M7 W2 E4 fix round 1 (declared mover), per cell at
        // the four call sites below: 596 -> 586, 192 -> 197, 191 -> 181, 184 -> 180.
        // The OPTION-OFF walk is reached by every EQP solve too (E4-5), so the
        // UNREGULARIZED POLISH (eqp_solve.h) moves it: the objective pin and the
        // kOptimal status -- what N3 is about -- hold unchanged at every one.
        EXPECT_EQ(s_off.counters.minor_iters, off_minors);
        EXPECT_LE(std::abs(hven::test_support::qp_dump_objective(d, s_off.x) - f_off),
                  1e-9 * std::abs(f_off));
        EXPECT_EQ(s_off.counters.dep_ride_blockers, 0);
        EXPECT_EQ(s_off.counters.ride_declines_fallthrough_blocked, 0);
    }
#else
    (void)off_minors;
#endif
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = e2d4_solve(d, alg);
        expect_e2d4_kkt(d, s);
        EXPECT_LE(std::abs(hven::test_support::qp_dump_objective(d, s.x) - f_off),
                  1e-7 * std::abs(f_off));
        EXPECT_EQ(s.counters.zero_motion_exhausted, 0);
        EXPECT_EQ(s.counters.zero_motion_cap_hits, 0);
        if (expect_exchange) {
            EXPECT_GE(s.counters.dep_ride_blockers, 1);
            EXPECT_GE(s.counters.ride_exchanges, 1);
        } else {
            // qp3 ONLY, and it is ALGEBRA-DEPENDENT at E4 fix round 2: the border
            // walk still meets 3 dependent ride blockers and exchanges all 3, the
            // refactorize walk meets none. Both reach the same optimum with the same
            // KKT bounds, so what is asserted on this cell is NC1's RULE rather than
            // a count -- every dependent blocker the walk meets is exchanged, and
            // none is left to decline -- which holds on both algebras and is the
            // statement the mechanism is for. The counts themselves are measured
            // above and recorded here; qp4, qp5 and qp6 still carry the >= 1 premise.
            EXPECT_EQ(s.counters.ride_exchanges, s.counters.dep_ride_blockers)
                << "a dependent ride blocker was met and not exchanged";
        }
        EXPECT_EQ(s.counters.ride_declines_fallthrough_blocked, 0);
        EXPECT_EQ(s.counters.ride_sign_vetoes, 0);
    }
}

} // namespace

TEST(QpE2WorkingSet, TheDeclinedRideResolvesWithoutAnExchangeOnSteenbrbRestorationQp3) {
    // RENAMED at E4 fix round 2 (old: ADeclinedRideIsExchangedOnSteenbrbRestorationQp3):
    // this cell no longer needs the exchange. Same optimum, same KKT bounds,
    // `dep_ride_blockers` 0 -- see the helper's note.
    expect_the_declined_ride_resolves("Cs_STEENBRB_qp3.txt", -11.5370634375, 586, false);
}

TEST(QpE2WorkingSet, ADeclinedRideIsExchangedOnSteenbrbRestorationQp4) {
    expect_the_declined_ride_resolves("Cs_STEENBRB_qp4.txt", -5.76904694719, 197);
}

TEST(QpE2WorkingSet, ADeclinedRideIsExchangedOnSteenbrbRestorationQp5) {
    expect_the_declined_ride_resolves("Cs_STEENBRB_qp5.txt", -2.88465228068, 181);
}

TEST(QpE2WorkingSet, ADeclinedRideIsExchangedOnSteenbrbRestorationQp6) {
    expect_the_declined_ride_resolves("Cs_STEENBRB_qp6.txt", -1.44235834209, 180);
}

namespace {

using RideRead = hven::solvers::detail::testing::QpWalkRideOutcome;

/// min 1/2 (x0^2 + x1^2) - 1/8 x2^2 + 1/2 x0 x2 - 0.4 x1 + 2.25 x2 subject to
/// x0 <= 0, x1 <= 0, x0 + x1 <= 0 (a DEPENDENT third row), x0, x1 in [-10, 10],
/// x2 in [-10, 1]. Seeded at (0, 0, 1) on {row 0, row 1, x2 at upper}: the
/// multipliers are row 0 -0.5, row 1 0.4, x2's 2 (wrong-signed, the largest), so
/// the walk drops x2 onto the face {x2} with curvature -1/4. The ride leaves x2's
/// upper bound downward; along it row 2 (= row 0 + row 1) carries the dual_mu
/// footprint of the rows' multiplier change as a positive rate and blocks,
/// DEPENDENT. The global minimum (x2 concave: an endpoint; then a convex QP in
/// x0, x1) is f = -35 at (0, 0, -10).
QpProblem declined_ride_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(3, 3);
    Hd(0, 0) = 1.0;
    Hd(1, 1) = 1.0;
    Hd(2, 2) = -0.25;
    Hd(0, 2) = 0.5;
    qp.H = Hd.sparseView(); // upper triangle
    qp.g = Vec(3);
    qp.g << 0.0, -0.4, 2.25;
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    Eigen::MatrixXd Ai(3, 3);
    Ai << 1, 0, 0, 0, 1, 0, 1, 1, 0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec::Zero(3);
    qp.lower = Vec::Constant(3, -10.0);
    qp.upper = Vec(3);
    qp.upper << 10.0, 10.0, 1.0;
    return qp;
}

QpSolution declined_ride_seed() {
    QpSolution seed;
    seed.x = Vec(3);
    seed.x << 0.0, 0.0, 1.0;
    seed.bound_state = {BoundState::kFree, BoundState::kFree, BoundState::kAtUpper};
    seed.ineq_active = {true, true, false};
    return seed;
}

struct RideWalk {
    QpSolution solution;
    std::vector<hven::solvers::detail::testing::QpWalkMinorRecord> records;
};

RideWalk declined_ride_walk(const QpOptions &o) {
    Obs::reset();
    Obs::active = true;
    RideWalk w;
    const QpEngine engine{o};
    w.solution = engine.solve(declined_ride_qp(), declined_ride_seed());
    w.records = Obs::records;
    Obs::active = false;
    Obs::reset();
    return w;
}

} // namespace

TEST(QpE2WorkingSet, TheRideNeedsNoExchangeOnceThePolishRemovesTheRowsFootprint) {
    // RENAMED AND RE-PREMISED at M7 W2 E4 fix round 2 (ruling E4-19; old name
    // `ADependentRideBlockerIsExchangedAndTheRideRetaken`).
    //
    // WHAT IT HELD, from NC1 until E4: at `dual_mu = 1e-5` one refinement step
    // leaves an O(mu^2 * dlambda) footprint on the rows -- a rate far above the
    // ratio test's 1e-12 and inside the ride's null-space tolerance -- so the
    // ride's blocker is a DEPENDENT row, exchanged for the smallest-ratio member,
    // the ride re-armed and re-taken on the exchanged set until x2's lower bound
    // ends it.
    //
    // WHY IT CANNOT HOLD IT: the footprint is MANUFACTURED by the fixture's
    // inflated `dual_mu`, and E4's UNREGULARIZED POLISH backs the dual
    // regularization out of every EQP answer (fix round 1, E4-11). There is no
    // footprint on the rows, so no dependent blocker at zero motion, so nothing to
    // exchange. MEASURED: `dep_ride_blockers` 0 and `ride_exchanges` 0 on both
    // algebras, with the SAME ANSWER -- kOptimal, objective -35, x2 at its lower
    // bound -- which is the half of this fixture that was ever about the outcome.
    //
    // NC1's MECHANISM KEEPS ITS LIVE COVERAGE on the field cells where the
    // dependency is structural rather than manufactured: `Cs_STEENBRB_qp4/5/6`
    // still assert `dep_ride_blockers >= 1` and `ride_exchanges >= 1` through
    // `expect_the_declined_ride_resolves`.
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        QpOptions o = e2_opts(alg, 40);
        o.dual_mu = 1e-5;
        const RideWalk w = declined_ride_walk(o);
        const QpSolution &s = w.solution;
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_NEAR(objective(declined_ride_qp(), s.x), -35.0, 1e-7);
        EXPECT_NEAR(s.x(2), -10.0, 1e-12);
        EXPECT_EQ(s.counters.dep_ride_blockers, 0)
            << "a dependent ride blocker is back: the rows' dual_mu footprint has returned";
        EXPECT_EQ(s.counters.ride_exchanges, 0);
        EXPECT_EQ(s.counters.ride_declines_fallthrough_blocked, 0);
        EXPECT_EQ(s.counters.ride_sign_vetoes, 0);
        EXPECT_EQ(s.counters.zero_motion_cap_hits, 0);
        // AND THE RELEASED BOUND NEVER RE-ENTERS, which never depended on the
        // exchange: no pass may add back the bound the drop released at zero motion.
        for (const auto &r : w.records) {
            EXPECT_FALSE(r.post_drop && r.released_index == 2 &&
                         r.block == hven::solvers::detail::testing::QpWalkBlock::kBound &&
                         r.block_index == 2 && r.alpha == 0.0)
                << "the released bound re-entered at alpha 0";
        }
    }
    // The option-off walk (no dependency test) reaches the same optimum, unchanged.
    QpOptions off = e2_opts(WorkingSetLinearAlgebra::kRefactorize, 40);
    off.dual_mu = 1e-5;
    off.independent_working_set = false;
    const RideWalk w_off = declined_ride_walk(off);
    EXPECT_EQ(static_cast<int>(w_off.solution.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_NEAR(objective(declined_ride_qp(), w_off.solution.x), -35.0, 1e-7);
    EXPECT_EQ(w_off.solution.counters.dep_ride_blockers, 0);
}

TEST(QpE2WorkingSet, TheNullSpaceVetoIsNotReachedOnceThePolishRemovesTheRowsFootprint) {
    // RENAMED AND RE-PREMISED at M7 W2 E4 fix round 2 (ruling E4-19; old name
    // `ANullSpaceVetoedRideNeverStepsBackIntoTheReleasedBound`).
    //
    // WHAT IT HELD: the same QP at `dual_mu = 1e-4`, where the rows' footprint after
    // the refinement step EXCEEDS the ride's null-space tolerance, so the ride is
    // VETOED (branch (i)); the EQP step re-enters x2's upper bound, the drop is
    // undone (x2 re-enters drop-exempt) and no step is taken, while the option-off
    // walk falls through and re-adds x2 at alpha = 0.
    //
    // WHY IT CANNOT HOLD IT: the footprint is what the fixture's inflated `dual_mu`
    // manufactures, and E4's UNREGULARIZED POLISH backs the dual regularization out
    // of the answer, so the rows are met and the veto's own condition is never true.
    // MEASURED: `ride_declines_fallthrough_blocked` 0 on BOTH algebras and 0 on the
    // option-off walk, with no `kNullspaceVeto` record at all.
    //
    // WHAT IS ASSERTED NOW: the absence, plus the two invariants that never
    // depended on the veto -- no sign veto, and the released bound never re-entering
    // at alpha 0 on ANY pass, which is the rule the fixture's name was about. A
    // returning veto fails here and the read-out above can be reinstated.
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        QpOptions o = e2_opts(alg, 40);
        o.dual_mu = 1e-4;
        const RideWalk w = declined_ride_walk(o);
        EXPECT_EQ(w.solution.counters.ride_declines_fallthrough_blocked, 0)
            << "the null-space veto fired: the rows' dual_mu footprint has returned";
        EXPECT_EQ(w.solution.counters.ride_sign_vetoes, 0);
        for (const auto &r : w.records) {
            EXPECT_NE(r.ride_outcome, RideRead::kNullspaceVeto);
            EXPECT_FALSE(r.post_drop && r.released_index == 2 &&
                         r.block == hven::solvers::detail::testing::QpWalkBlock::kBound &&
                         r.block_index == 2 && r.alpha == 0.0)
                << "the released bound re-entered at alpha 0";
        }
    }
    QpOptions off = e2_opts(WorkingSetLinearAlgebra::kRefactorize, 40);
    off.dual_mu = 1e-4;
    off.independent_working_set = false;
    const RideWalk w_off = declined_ride_walk(off);
    // The option-off CONTRAST is gone with the veto: there is no vetoed pass for the
    // ordinary step to fall through from, so the contrast reads 0 on both sides now.
    EXPECT_EQ(w_off.solution.counters.ride_declines_fallthrough_blocked, 0);
    for (const auto &r : w_off.records) {
        EXPECT_NE(r.ride_outcome, RideRead::kNullspaceVeto);
    }
}

// ===========================================================================
// M7 W2 E4 (ruling E4-7, added at fix round 2 per E4-14 / E4-22): the three E2-D4
// cells the round's own clauses name. They live here, beside the other E2-D4
// fixtures, because they use this file's `e2d4_dump` / `e2d4_solve` helpers.
// ===========================================================================

TEST(QpE4ProximalEqp, TheYorknetRestorationQpIsDriftFreeAndOptimal) {
#ifndef NDEBUG
    // A DECLARED DEBUG SKIP (M7 W2 E4 fix round 2, ruling E4-22's "every Debug test
    // < 600 s"), with its cost. MEASURED at this round's engine: **2414.10 s in
    // Debug** against 0.3 s in Release. The cell is A0_YORKNET's restoration QP, 689
    // minors and about 1020 factorizations of a 272-column system PER ALGEBRA, and
    // -O0 pays for every one of them; splitting per algebra leaves two ~1200 s cases,
    // so the split E4-14 prescribes does not reach the bound here either.
    //
    // WHAT THE SKIP COSTS, stated: the equality drift's ABSENCE is not observed under
    // Debug's arithmetic on this dump. The drift's own synthetic coverage is
    // `QpE4ProximalEqp.NoOrdinaryStepAscendsItsOwnObjectiveOn*` and
    // `QpNc2FlatFace`'s flat-ray LP, which run in both configurations, and the walk's
    // row-drift refusal has its own counter in every configuration.
    // REGISTERED: this cell wants a smaller dump, not a smaller test.
    GTEST_SKIP() << "Debug costs 2414.10 s on this dump (Release 0.3 s), four times M6's 600 s "
                    "per-fixture budget; asserted in Release, and the drift's synthetic coverage "
                    "runs in both configurations";
#endif
    // E4-7's A0_YORKNET cell, and the round's sharpest single conversion. At the base
    // this dump spent its whole 7680-minor cap with the equality rows open by
    // 2.50e-01 and `eq_drift_violations` at 7502 -- the walk stepping OFF its own
    // closed rows on pass after pass, which is the pathology E4-1's proximal pair and
    // E4-4's residual check were built for.
    //
    // MEASURED at this round's engine, both algebras: kOptimal in 689 minors with the
    // worst equality residual 8.86e-14 and the drift count ZERO. The drift count is
    // NC2's own counter and is only compiled into the scratch replay tool, so what is
    // asserted here is the residual it counts -- every equality row met to 1e-12 --
    // which is the same statement in the currency the shipped counters carry.
    const QpDump d = e2d4_dump("A0_YORKNET_qp3.txt");
    EXPECT_NE(d.result.find("status=1"), std::string::npos) << "the base spent its cap";
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = e2d4_solve(d, alg);
        const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_LE(k.eq, 1e-12) << "the equality drift is back";
        EXPECT_LE(k.box, 1e-9);
        EXPECT_LE(k.stationarity, 1e-6);
        EXPECT_EQ(s.counters.zero_motion_exhausted, 0);
        EXPECT_EQ(s.counters.zero_motion_cap_hits, 0);
    }
}

TEST(QpE4ProximalEqp, TheAcopp57PrimaryQpCostsTwoMinorsOnBothAlgebras) {
    // E4-12's cell, and the fixture of the border path's parity. The base read
    // kOptimal in 8 minors on the border path and 2 on the eliminated one, with that
    // face's bordered terminal stationarity 1.25e-07 against the eliminated path's
    // 1.02e-14 -- a nine-decade gap that E3's proximal arm turned into a 1795-minor
    // stall. E4's UNREGULARIZED POLISH refines the bordered answer against the face's
    // own exact system, so the two algebras agree: MEASURED kOptimal in 2 minors on
    // BOTH, with the terminal stationarity 9.7e-15.
    const QpDump d = e2d4_dump("ACOPP57_def-qp0.txt");
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = e2d4_solve(d, alg);
        const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(s.counters.minor_iters, 2) << "the border path lost its parity";
        // 1e-11 on the rows, MEASURED: border 1.37e-14, refactorize 1.97e-12 -- the
        // two algebras differ by two decades here and both are decades inside the
        // base's own 4.03e-07 on this dump, which is the move worth asserting.
        EXPECT_LE(k.eq, 1e-11);
        EXPECT_LE(k.stationarity, 1e-12) << "the bordered face's own residue is back";
    }
}

TEST(QpE4ProximalEqp, TheSteenbrbRestorationQp2ExitsFiniteAndSaysWhy) {
    // E4-4's cell, and the one clause of E4-7 that is NOT a conversion -- declared as
    // such at fix round 1 and unchanged here. At E3 this dump produced a NaN
    // objective: the factor fails at pass 0 with a residual of 9.3e+10 and the walk
    // stepped toward the answer anyway. E4-4's EQP-SITE RESIDUAL CHECK refuses that
    // answer, and what this fixture pins is the HONEST EXIT it buys.
    //
    // MEASURED, both algebras: kNumericalError in 547 minors -- NOT optimal, and that
    // is the declared reading -- with a FINITE objective (-990729858499, against the
    // base's -990591855583) and the equality rows met to 1.19e-15 where the base left
    // them at 6.55e-07. `eqp_residual_refusals` is nonzero, which is the counter that
    // says WHY the exit is what it is, and it is asserted rather than narrated.
    //
    // WHAT WOULD MAKE THIS CELL OPTIMAL, and why it is not taken: a polish step cap of
    // 1e-5 converts it (kOptimal in 554 minors, measured at fix round 2's cap search)
    // and loses `ACOPR57_def`/qp4's base certificate on both algebras in the same
    // motion. The cap stays at 1e-6 and this cell stays kNumericalError.
    const QpDump d = e2d4_dump("STEENBRB_def-qp2.txt");
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        const QpSolution s = e2d4_solve(d, alg);
        const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError))
            << "declared: this cell is not optimal at the shipped polish step cap";
        EXPECT_TRUE(s.x.allFinite()) << "no NaN may reach the caller";
        EXPECT_TRUE(std::isfinite(hven::test_support::qp_dump_objective(d, s.x)));
        EXPECT_LE(k.eq, 1e-12) << "and the rows it does report are met";
        EXPECT_GE(s.counters.eqp_residual_refusals, 1)
            << "the refusal is what makes the exit honest; without it this cell was a NaN";
        EXPECT_LT(s.counters.minor_iters, 1000) << "measured 547";
    }
}
