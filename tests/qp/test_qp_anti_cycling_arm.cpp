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
//       being a branch the fixtures describe and never enter.
//   (5) The switch is PER-THREAD, like the observer's.
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
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

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
    // unnoticed.
    EXPECT_EQ(std::memcmp(&a, &b, sizeof(QpCounters)), 0)
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
    const Index lex_switches = Ac::lex_drop_switches + Ac::lex_ratio_switches;
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
        EXPECT_GT(lex_switches, 0);
    }
}

} // namespace

// ===========================================================================
// (1) OFF IS PRODUCTION
// ===========================================================================

TEST(QpAntiCyclingArm, TheSwitchIsOffByDefaultAndItsParametersAreThePapersOwn) {
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
    // ARM L's default is the BRIEFED tie-break form, not Bland's full rule.
    EXPECT_FALSE(Ac::params.lex_full_pricing);
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

TEST(QpAntiCyclingArm, ArmEsResetSequenceIsReachableAndReProjectsTheIterate) {
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
