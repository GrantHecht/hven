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

/// Asserts that the OFF walk on `qp` really is the recurrence this round is
/// about: it spends its whole budget, its working-set signature has an exactly
/// periodic suffix of the expected period covering most of the walk, and the
/// iterate does not move at all over that suffix.
Walk expect_the_off_walk_cycles(const QpProblem &qp, Index expected_period) {
    Ac::reset();
    const Walk off = observed_walk(qp, capped(kCycleCap));
    EXPECT_EQ(static_cast<int>(off.solution.status), static_cast<int>(QpStatus::kMaxIter))
        << "the OFF walk did not spend its budget: this fixture no longer cycles";
    EXPECT_EQ(off.minors, kCycleCap);
    EXPECT_EQ(off.period, expected_period);
    EXPECT_GE(off.tail * 2, off.minors) << "the periodic suffix covers less than half the walk";
    EXPECT_EQ(off.tail_motion, 0.0) << "the suffix is not a ZERO-MOTION recurrence";
    // NOT asserted: that the WHOLE walk moved nothing. M3c's cells move
    // while they approach the degenerate vertex and stop moving once they are
    // on it; the zero-motion claim is about the TAIL, and that is what is
    // asserted above.
    return off;
}

/// Asserts that an ARMED walk escaped: it returns `kOptimal`, the recurrence
/// read-out no longer finds a zero-motion periodic suffix worth the name, the
/// point it returns is an optimum of the problem by two independent checks,
/// and the arm's own counters say it actually changed a decision.
void expect_the_armed_walk_escapes(const QpProblem &qp, Arm arm, const Walk &off) {
    Ac::reset();
    Ac::arm = arm;
    const Walk on = observed_walk(qp, capped(kCycleCap));
    const Index min_steps = Ac::min_steps;
    const Index pivot_switches = Ac::pivot_switches;
    const Index lex_drop_switches = Ac::lex_drop_switches;
    const Index lex_ratio_switches = Ac::lex_ratio_switches;
    Ac::reset();

    // THE PREDICATE, and the one thing it must not be read as. A converged
    // walk ALWAYS ends with a run of passes carrying the same working-set
    // signature -- the terminal pass, and whatever refinement precedes it --
    // so "a periodic suffix exists" is true of every successful solve and
    // cannot by itself mean "cycling". What separates the recurrence M3c
    // measured from ordinary termination is that the recurrence never ends:
    // it runs until the iteration cap. So RECURRENCE_GONE is asserted here as
    // the conjunction of TERMINATION well inside the cap, a walk that MOVED,
    // and a periodic suffix strictly shorter than the OFF walk's and short in
    // absolute terms.
    EXPECT_EQ(static_cast<int>(on.solution.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_LT(on.minors * 5, kCycleCap) << "the armed walk did not terminate promptly";
    EXPECT_GT(on.total_motion, 0.0) << "the armed walk still never moved the iterate";
    EXPECT_LT(on.tail, off.tail) << "the periodic suffix is no shorter than the OFF walk's";
    EXPECT_LE(on.tail, 8) << "the armed walk still ends in a long repetition";

    // AND THE ANSWER IS THE PROBLEM'S OWN OPTIMUM, two independent ways.
    EXPECT_TRUE(is_a_kkt_point(qp, on.solution, 1e-7));
    EXPECT_NEAR(objective(qp, on.solution.x), brute_force_optimum(qp), 1e-7);

    // THE ARM ACTUALLY DID SOMETHING -- otherwise "the armed walk escaped"
    // would be a statement about the walk and not about the arm.
    if (arm == Arm::kExpand) {
        EXPECT_GT(min_steps + pivot_switches, 0);
    } else {
        // AND WHICH SIDE DID IT, ASSERTED RATHER THAN SUMMED (FIX ROUND 1,
        // review Minor 2). The round's claim about ARM L on these two
        // trajectories is not "the arm switched something" but "the DROP side
        // switched NOTHING and the escape is entirely the RATIO side". A sum
        // cannot say that, and the sum is what the first version asserted.
        // NOTE THE SCOPE: `lex_drop_switches == 0` is a fact about THESE TWO
        // FIXTURE TRAJECTORIES, not about hven's drop rule in general -- on
        // field cells L records 13 drop switches on READING6 and 31 on
        // ZAMB2-8 (this round's `tables/arm.md`).
        EXPECT_EQ(lex_drop_switches, 0)
            << "the lexicographic DROP rule moved a choice on this fixture; the round's "
               "read-out of these two trajectories says it does not";
        EXPECT_GT(lex_ratio_switches, 0)
            << "the lexicographic RATIO re-tie moved no choice, so the escape cannot be "
               "credited to the arm";
    }
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

TEST(QpAntiCyclingArm, TheOffWalkCyclesAtPeriodTwoWithExactlyZeroMotion) {
    ArmGuard guard;
    expect_the_off_walk_cycles(period2_cycle_qp(), 2);
}

TEST(QpAntiCyclingArm, TheOffWalkCyclesAtPeriodFourWithExactlyZeroMotion) {
    ArmGuard guard;
    expect_the_off_walk_cycles(period4_cycle_qp(), 4);
}

TEST(QpAntiCyclingArm, ArmEEscapesThePeriodTwoCycleAndReturnsTheOptimum) {
    ArmGuard guard;
    const QpProblem qp = period2_cycle_qp();
    const Walk off = expect_the_off_walk_cycles(qp, 2);
    expect_the_armed_walk_escapes(qp, Arm::kExpand, off);
}

TEST(QpAntiCyclingArm, ArmLEscapesThePeriodTwoCycleAndReturnsTheOptimum) {
    ArmGuard guard;
    const QpProblem qp = period2_cycle_qp();
    const Walk off = expect_the_off_walk_cycles(qp, 2);
    expect_the_armed_walk_escapes(qp, Arm::kLexicographic, off);
}

TEST(QpAntiCyclingArm, ArmEEscapesThePeriodFourCycleAndReturnsTheOptimum) {
    ArmGuard guard;
    const QpProblem qp = period4_cycle_qp();
    const Walk off = expect_the_off_walk_cycles(qp, 4);
    expect_the_armed_walk_escapes(qp, Arm::kExpand, off);
}

TEST(QpAntiCyclingArm, ArmLEscapesThePeriodFourCycleAndReturnsTheOptimum) {
    ArmGuard guard;
    const QpProblem qp = period4_cycle_qp();
    const Walk off = expect_the_off_walk_cycles(qp, 4);
    expect_the_armed_walk_escapes(qp, Arm::kLexicographic, off);
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
        EXPECT_EQ(c.dep_border_retreats, alg == WorkingSetLinearAlgebra::kSchurBorder ? 1 : 0);
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
        EXPECT_NEAR(s.x(0), 0.5, 1e-12);
        EXPECT_NEAR(s.x(1), 0.125, 1e-12);
        EXPECT_NEAR(s.x(2), 0.375, 1e-12);
    }
}

TEST(QpE2WorkingSet, TheBorderRetreatsExactlyOnceAndEndsOptimal) {
    // THE DECLARED DEVIATION (settler S11), and finding E2-F1: on the
    // period-two QP the border pass's K0 is near-singular on the pinned
    // directions, its Schur complement carries O(1) rounding, the EQP step
    // leaves its face and the dependency coefficients are inaccurate; the
    // rule would end the solve there, so the pass is redone ONCE on the
    // bound-eliminated path and the walk ends at the optimum.
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
    EXPECT_EQ(s.counters.dep_border_retreats, 1);
    EXPECT_EQ(retreat_records, 1);
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
    // pin tests UNDECIDABLE -- and it completes the equality row on pinned
    // columns, an EXACT dependency read off the row (-e2 = a - e0). The base
    // deferred it, re-checked, retreated and ended N3 (6 minors); now the
    // certificate verifies on the first pass. The refactorize path's test
    // decides it numerically and reaches the same certificate.
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
        if (alg == WorkingSetLinearAlgebra::kSchurBorder) {
            EXPECT_EQ(c.dep_undecidable, 1);
            EXPECT_EQ(c.dep_structural_exact, 1);
            EXPECT_EQ(c.minor_iters, 1);
            // Read off the row, exactly: -e2 = a - e0, so y_e = 1 and the
            // implied bound is be - u0 = 0.5 against -l2 = 0.
            EXPECT_EQ(f.y_e(0), 1.0);
            EXPECT_EQ(f.implied, 0.5);
            EXPECT_EQ(f.limit, 0.0);
        } else {
            EXPECT_EQ(c.dep_undecidable, 0);
            EXPECT_EQ(c.dep_structural_exact, 0);
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
    struct Case {
        const char *file;
        double f_star;
        double rel_tol;
    };
    for (const Case &c : {Case{"ACOPR57_def-qp0.txt", 432.674974172, 1e-4},
                          Case{"ACOPR57_def-qp1.txt", 501.805584074, 1e-5}}) {
        SCOPED_TRACE(c.file);
        const QpDump d = e2d4_dump(c.file);
        EXPECT_NE(d.result.find("status=3"), std::string::npos);
        EXPECT_NE(d.result.find("blk_term=1"), std::string::npos);
        QpOptions off = d.opts;
        off.independent_working_set = false;
        const QpSolution s_off = hven::test_support::solve_qp_dump(d, off);
        ASSERT_EQ(static_cast<int>(s_off.status), static_cast<int>(QpStatus::kOptimal));
        const double f_off = hven::test_support::qp_dump_objective(d, s_off.x);
        EXPECT_LE(std::abs(f_off - c.f_star), c.rel_tol * std::abs(c.f_star));
        for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
            SCOPED_TRACE(e2_name(alg));
            const QpSolution s = e2d4_solve(d, alg);
            expect_e2d4_kkt(d, s);
            const double f = hven::test_support::qp_dump_objective(d, s.x);
            EXPECT_LE(std::abs(f - c.f_star), c.rel_tol * std::abs(c.f_star));
            EXPECT_LE(std::abs(f - f_off), 1e-7 * std::abs(f_off));
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

void expect_the_declined_ride_resolves(const char *file, double f_off, Index off_minors) {
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
        EXPECT_GE(s.counters.dep_ride_blockers, 1);
        EXPECT_GE(s.counters.ride_exchanges, 1);
        EXPECT_EQ(s.counters.ride_declines_fallthrough_blocked, 0);
        EXPECT_EQ(s.counters.ride_sign_vetoes, 0);
    }
}

} // namespace

TEST(QpE2WorkingSet, ADeclinedRideIsExchangedOnSteenbrbRestorationQp3) {
    expect_the_declined_ride_resolves("Cs_STEENBRB_qp3.txt", -11.5370634375, 596);
}

TEST(QpE2WorkingSet, ADeclinedRideIsExchangedOnSteenbrbRestorationQp4) {
    expect_the_declined_ride_resolves("Cs_STEENBRB_qp4.txt", -5.76904694719, 192);
}

TEST(QpE2WorkingSet, ADeclinedRideIsExchangedOnSteenbrbRestorationQp5) {
    expect_the_declined_ride_resolves("Cs_STEENBRB_qp5.txt", -2.88465228068, 191);
}

TEST(QpE2WorkingSet, ADeclinedRideIsExchangedOnSteenbrbRestorationQp6) {
    expect_the_declined_ride_resolves("Cs_STEENBRB_qp6.txt", -1.44235834209, 184);
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

TEST(QpE2WorkingSet, ADependentRideBlockerIsExchangedAndTheRideRetaken) {
    // dual_mu = 1e-5: one refinement step leaves an O(mu^2 * dlambda) footprint on
    // the rows, a rate far above the ratio test's 1e-12 and inside the ride's
    // null-space tolerance. The ride's blocker is row 2 or row 0, each dependent
    // on the working rows; each is EXCHANGED for the member with the smallest
    // ratio, the ride re-armed and re-taken on the exchanged set (same null
    // space), until an independent blocker -- x2's lower bound -- ends it.
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        QpOptions o = e2_opts(alg, 40);
        o.dual_mu = 1e-5;
        const RideWalk w = declined_ride_walk(o);
        const QpSolution &s = w.solution;
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_NEAR(objective(declined_ride_qp(), s.x), -35.0, 1e-7);
        EXPECT_NEAR(s.x(2), -10.0, 1e-12);
        EXPECT_GE(s.counters.dep_ride_blockers, 1);
        EXPECT_EQ(s.counters.ride_exchanges, s.counters.dep_ride_blockers);
        EXPECT_EQ(s.counters.ride_declines_fallthrough_blocked, 0);
        EXPECT_EQ(s.counters.ride_sign_vetoes, 0);
        EXPECT_EQ(s.counters.zero_motion_cap_hits, 0);
        // The seam: every dependent-blocker pass released x2 and is followed by a
        // re-armed post-drop pass on the same release; the last ride steps.
        Index dependent = 0;
        bool stepped = false;
        for (std::size_t k = 0; k < w.records.size(); ++k) {
            const auto &r = w.records[k];
            if (r.ride_outcome == RideRead::kDependentBlocker) {
                ++dependent;
                EXPECT_EQ(r.released_index, 2);
                EXPECT_NE(r.ride_sign_chosen, 0);
                EXPECT_EQ(r.action, hven::solvers::detail::testing::QpWalkAction::kAdd);
                EXPECT_FALSE(r.block == hven::solvers::detail::testing::QpWalkBlock::kBound &&
                             r.block_index == 2)
                    << "the released bound never re-enters";
                ASSERT_LT(k + 1, w.records.size());
                EXPECT_TRUE(w.records[k + 1].post_drop);
                EXPECT_EQ(w.records[k + 1].released_index, 2);
            }
            if (r.ride_outcome == RideRead::kStepped && r.released_index == 2) {
                stepped = true;
            }
        }
        EXPECT_EQ(dependent, s.counters.dep_ride_blockers);
        EXPECT_TRUE(stepped);
    }
    // The option-off walk (no dependency test): the ride admits the dependent row
    // and still reaches the same optimum.
    QpOptions off = e2_opts(WorkingSetLinearAlgebra::kRefactorize, 40);
    off.dual_mu = 1e-5;
    off.independent_working_set = false;
    const RideWalk w_off = declined_ride_walk(off);
    EXPECT_EQ(static_cast<int>(w_off.solution.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_NEAR(objective(declined_ride_qp(), w_off.solution.x), -35.0, 1e-7);
    EXPECT_EQ(w_off.solution.counters.dep_ride_blockers, 0);
}

TEST(QpE2WorkingSet, ANullSpaceVetoedRideNeverStepsBackIntoTheReleasedBound) {
    // The same QP at dual_mu = 1e-4: the rows' footprint after the refinement
    // step exceeds the ride's null-space tolerance, so the ride is VETOED (i).
    // The EQP step re-enters x2's upper bound, so the drop is undone (x2
    // re-enters drop-exempt) and no step is taken. The option-off walk falls
    // through instead: its ordinary step re-adds x2 at alpha = 0. Both walks then
    // cycle WITH motion through the other faces to the iteration cap (a class
    // registered with NC1: the null-space veto does not recompute the direction);
    // only the rule is asserted here.
    for (const WorkingSetLinearAlgebra alg : kE2Algebras) {
        SCOPED_TRACE(e2_name(alg));
        QpOptions o = e2_opts(alg, 40);
        o.dual_mu = 1e-4;
        const RideWalk w = declined_ride_walk(o);
        EXPECT_GE(w.solution.counters.ride_declines_fallthrough_blocked, 1);
        EXPECT_EQ(w.solution.counters.ride_sign_vetoes, 0);
        Index undone = 0;
        for (const auto &r : w.records) {
            if (!r.post_drop || r.ride_outcome != RideRead::kNullspaceVeto) {
                continue;
            }
            if (r.action == hven::solvers::detail::testing::QpWalkAction::kRideUndone) {
                ++undone;
                EXPECT_EQ(r.block_index, r.released_index);
                EXPECT_GE(r.released_directional, -1e-9) << "undone only when p does not leave";
            } else {
                // The ordinary step is taken only along a p that leaves.
                EXPECT_LT(r.released_directional, 0.0);
            }
        }
        EXPECT_EQ(undone, w.solution.counters.ride_declines_fallthrough_blocked);
    }
    QpOptions off = e2_opts(WorkingSetLinearAlgebra::kRefactorize, 40);
    off.dual_mu = 1e-4;
    off.independent_working_set = false;
    const RideWalk w_off = declined_ride_walk(off);
    bool fell_through = false;
    for (const auto &r : w_off.records) {
        if (r.post_drop && r.ride_outcome == RideRead::kNullspaceVeto &&
            r.action == hven::solvers::detail::testing::QpWalkAction::kAdd &&
            r.block_index == r.released_index && r.alpha == 0.0) {
            fell_through = true;
        }
    }
    EXPECT_TRUE(fell_through) << "the option-off contrast: the saddle step re-enters at alpha 0";
    EXPECT_EQ(w_off.solution.counters.ride_declines_fallthrough_blocked, 0);
}
