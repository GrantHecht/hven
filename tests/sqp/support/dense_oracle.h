#pragma once

// tests/support/dense_oracle.h — test-support only, NOT part of the public
// library surface. Exhaustive active-set enumeration for small QPs; exact for
// n + mi + (number of bounded variables) <= ~14. This is the correctness
// reference the rest of the QP engine's tests are checked against, so it
// deliberately favors brute-force clarity over performance.
//
// TWO ENTRY POINTS, ONE ENUMERATION.
//
//   solve_dense_oracle(qp)          -> the global optimum of a STRICTLY CONVEX
//                                      QP (throws when nothing is accepted).
//   enumerate_local_minimizers(qp)  -> every LOCAL minimizer, for an
//                                      arbitrary (including indefinite) H.
//
// Both are thin visitors over detail::enumerate_kkt_candidates, which owns the
// enumeration, the KKT solve and the first-order acceptance tests. The split
// exists so the nonconvex path cannot drift from the convex one; the convex
// path's behavior is unchanged by its introduction.
//
// THE SHARED CORE. For every subset of inequality rows crossed with every
// per-variable choice of {free, at-lower, at-upper} (skipping a bound side
// that is +/-1e20 or beyond, i.e. effectively infinite), this builds the dense
// equality-KKT system for that active set and solves it exactly with
// Eigen::FullPivLU. A candidate is accepted iff:
//   - the resulting x is primal feasible for the FULL problem (all
//     inequalities, all bounds, and the equality constraints) to tol 1e-10,
//     and
//   - every inequality/bound multiplier has the correct sign, checked as
//     >= -1e-10 (see the sign-convention note on the bound rows below).
// Those are exactly the FIRST-ORDER (KKT) conditions. Nothing in the core
// looks at curvature.
//
// solve_dense_oracle returns, among accepted candidates, the one with lowest
// objective (for a strictly convex QP this is unique up to numerical noise, so
// "lowest objective" is really just a tie-breaker against duplicate candidates
// from degenerate active sets). It is NOT valid for an indefinite H: the
// lowest-objective KKT point of a nonconvex QP need not be the answer any
// descent method reaches, and a saddle can be accepted by first-order tests
// alone. Use enumerate_local_minimizers there.
//
// SECOND-ORDER FILTER (enumerate_local_minimizers only). See the function's
// own comment for the reduced-Hessian test, the grouping rule, and exactly
// where and in which direction the filter is inexact.
//
// Bound-row sign convention: a variable pinned at its lower bound is added
// to the augmented constraint matrix as the row -e_i (rhs -lower(i)); a
// variable pinned at its upper bound is added as +e_i (rhs upper(i)). With
// qp_problem.h's stationarity convention (grad(f) + Ae^T lambda_e +
// Ai^T lambda_i - z = 0), this choice makes the raw KKT multiplier for
// EITHER bound row equal to a quantity that must be >= -1e-10 to be
// feasible-dual, exactly like lambda_i. The actual bound multiplier
// reported in QpSolution::z is then that raw value at a lower bound, or its
// negation at an upper bound (z <= 0 there per the header's convention).

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <Eigen/Dense>
#include <fmt/format.h>

#include <tycho_sqp/qp_problem.h>
#include <tycho_sqp/types.h>

namespace tycho::sqp {

namespace detail {

constexpr double kOracleInfBound = 1e20;
constexpr double kOracleFeasTol = 1e-10;
constexpr double kOracleSignTol = 1e-10;
constexpr std::uint64_t kOracleMaxCombinations = 1ull << 20;

// enumerate_local_minimizers only. kOracleCurvatureTol is the PSD threshold on
// the reduced Hessian's eigenvalues; kOracleDedupeTol is the infinity-norm
// radius within which two candidates count as the same POINT (reached through
// different active-set labelings) rather than two minimizers.
constexpr double kOracleCurvatureTol = 1e-10;
constexpr double kOracleDedupeTol = 1e-8;

// One accepted KKT point, as handed to a visitor by enumerate_kkt_candidates.
struct OracleCandidate {
    QpSolution solution;
    double objective = 0.0;
    // Active INEQUALITY rows plus pinned bounds. Equality rows are not counted
    // (they are active in every candidate, so including them would not order
    // anything).
    Index active_count = 0;
    // Gradients of the constraints this candidate treats as active, one per
    // row: the me equality rows first, then the active inequality rows, then
    // one +/-e_i row per pinned variable. (me + active_count) x n; the row
    // SIGNS follow the KKT system's own convention and are irrelevant to the
    // only consumer, which takes this matrix's null space.
    Eigen::MatrixXd active_jacobian;
};

using OracleVisitor = std::function<void(const OracleCandidate &)>;

// Visits every accepted KKT point of `qp`, in the enumeration order described
// at the top of this file. Throws on an invalid problem or one whose
// enumeration exceeds kOracleMaxCombinations.
inline void enumerate_kkt_candidates(const QpProblem &qp, const OracleVisitor &visit) {
    qp.validate();

    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();

    // Symmetrize H from its stored upper triangle into a dense matrix.
    const Eigen::MatrixXd Hd = Eigen::MatrixXd(qp.H).template selfadjointView<Eigen::Upper>();
    const Eigen::MatrixXd Aed = Eigen::MatrixXd(qp.Ae);
    const Eigen::MatrixXd Aid = Eigen::MatrixXd(qp.Ai);

    // Per-variable set of candidate bound states (always includes kFree).
    std::vector<std::vector<BoundState>> var_options(static_cast<std::size_t>(n));
    for (Index i = 0; i < n; ++i) {
        auto &opts = var_options[static_cast<std::size_t>(i)];
        opts.push_back(BoundState::kFree);
        if (qp.lower(i) > -detail::kOracleInfBound) {
            opts.push_back(BoundState::kAtLower);
        }
        if (qp.upper(i) < detail::kOracleInfBound) {
            opts.push_back(BoundState::kAtUpper);
        }
    }

    // Guard combinatorics before enumerating: 2^mi inequality subsets times
    // the product of per-variable option counts.
    if (mi > 20) {
        throw std::invalid_argument(
            fmt::format("enumerate_kkt_candidates: mi = {} makes 2^mi enumeration infeasible", mi));
    }
    const std::uint64_t ineq_combos = 1ull << mi;
    std::uint64_t var_combos = 1;
    for (const auto &opts : var_options) {
        var_combos *= static_cast<std::uint64_t>(opts.size());
        if (var_combos > detail::kOracleMaxCombinations) {
            throw std::invalid_argument(fmt::format(
                "enumerate_kkt_candidates: per-variable bound-state enumeration exceeds {} "
                "combinations",
                detail::kOracleMaxCombinations));
        }
    }
    const std::uint64_t total_combos = ineq_combos * var_combos;
    if (total_combos > detail::kOracleMaxCombinations) {
        throw std::invalid_argument(fmt::format(
            "enumerate_kkt_candidates: enumeration would require {} combinations (limit {})",
            total_combos, detail::kOracleMaxCombinations));
    }

    std::vector<BoundState> state(static_cast<std::size_t>(n), BoundState::kFree);

    // Evaluate one fully-specified active set: `state` gives each variable's
    // bound status, `ineq_mask` bit j says whether inequality row j is
    // active.
    auto try_candidate = [&](std::uint64_t ineq_mask) {
        std::vector<Index> active_rows;
        active_rows.reserve(static_cast<std::size_t>(mi));
        for (Index j = 0; j < mi; ++j) {
            if ((ineq_mask >> j) & 1ull) {
                active_rows.push_back(j);
            }
        }
        std::vector<Index> active_vars;
        active_vars.reserve(static_cast<std::size_t>(n));
        for (Index i = 0; i < n; ++i) {
            if (state[static_cast<std::size_t>(i)] != BoundState::kFree) {
                active_vars.push_back(i);
            }
        }

        const Index k = static_cast<Index>(active_rows.size() + active_vars.size());
        if (me + k > n) {
            return; // over-determined active set, per the brief's combinatorial guard
        }

        const Index rows_total = me + k;
        Eigen::MatrixXd C = Eigen::MatrixXd::Zero(rows_total, n);
        Eigen::VectorXd d = Eigen::VectorXd::Zero(rows_total);

        Index r = 0;
        if (me > 0) {
            C.topRows(me) = Aed;
            d.head(me) = qp.be;
            r = me;
        }
        for (Index row : active_rows) {
            C.row(r) = Aid.row(row);
            d(r) = qp.bi(row);
            ++r;
        }
        for (Index i : active_vars) {
            if (state[static_cast<std::size_t>(i)] == BoundState::kAtLower) {
                C(r, i) = -1.0;
                d(r) = -qp.lower(i);
            } else { // kAtUpper
                C(r, i) = 1.0;
                d(r) = qp.upper(i);
            }
            ++r;
        }

        const Index sys_size = n + rows_total;
        Eigen::MatrixXd K = Eigen::MatrixXd::Zero(sys_size, sys_size);
        K.topLeftCorner(n, n) = Hd;
        K.topRightCorner(n, rows_total) = C.transpose();
        K.bottomLeftCorner(rows_total, n) = C;

        Eigen::VectorXd rhs(sys_size);
        rhs.head(n) = -qp.g;
        rhs.tail(rows_total) = d;

        Eigen::FullPivLU<Eigen::MatrixXd> lu(K);
        if (!lu.isInvertible()) {
            return; // degenerate/rank-deficient active set: not a valid KKT point
        }
        const Eigen::VectorXd sol = lu.solve(rhs);
        const Eigen::VectorXd x = sol.head(n);
        const Eigen::VectorXd mult = sol.tail(rows_total);

        // Primal feasibility of the FULL problem, not just the active rows.
        if (me > 0 && (Aed * x - qp.be).lpNorm<Eigen::Infinity>() > detail::kOracleFeasTol) {
            return;
        }
        if (mi > 0) {
            const Eigen::VectorXd ineq_resid = Aid * x - qp.bi;
            for (Index j = 0; j < mi; ++j) {
                if (ineq_resid(j) > detail::kOracleFeasTol) {
                    return;
                }
            }
        }
        for (Index i = 0; i < n; ++i) {
            if (x(i) < qp.lower(i) - detail::kOracleFeasTol ||
                x(i) > qp.upper(i) + detail::kOracleFeasTol) {
                return;
            }
        }

        // Dual feasibility: every extracted multiplier must be >= -tol (see
        // the header comment for why this uniform check works for bound
        // rows at either side).
        // Multiplier layout in `mult` is [lambda_e (me) | active-ineq | bound],
        // matching the extraction below — the me offset is load-bearing.
        for (std::size_t idx = 0; idx < active_rows.size(); ++idx) {
            if (mult(me + static_cast<Index>(idx)) < -detail::kOracleSignTol) {
                return;
            }
        }
        for (std::size_t idx = 0; idx < active_vars.size(); ++idx) {
            const Index mult_idx = me + static_cast<Index>(active_rows.size() + idx);
            if (mult(mult_idx) < -detail::kOracleSignTol) {
                return;
            }
        }

        QpSolution candidate;
        candidate.status = QpStatus::kOptimal;
        candidate.x = x;
        candidate.lambda_e = (me > 0) ? mult.head(me) : Vec(0);
        candidate.lambda_i = Vec::Zero(mi);
        for (std::size_t idx = 0; idx < active_rows.size(); ++idx) {
            candidate.lambda_i(active_rows[idx]) = mult(me + static_cast<Index>(idx));
        }
        candidate.z = Vec::Zero(n);
        for (std::size_t idx = 0; idx < active_vars.size(); ++idx) {
            const Index i = active_vars[idx];
            const double raw = mult(me + static_cast<Index>(active_rows.size() + idx));
            candidate.z(i) =
                (state[static_cast<std::size_t>(i)] == BoundState::kAtLower) ? raw : -raw;
        }
        candidate.bound_state = state;
        candidate.ineq_active.assign(static_cast<std::size_t>(mi), false);
        for (Index row : active_rows) {
            candidate.ineq_active[static_cast<std::size_t>(row)] = true;
        }
        candidate.counters = QpCounters{};

        OracleCandidate out;
        out.solution = std::move(candidate);
        out.objective = qp.g.dot(x) + 0.5 * x.dot(Hd * x);
        out.active_count = k;
        out.active_jacobian = std::move(C);
        visit(out);
    };

    // Mixed-radix enumeration over each variable's bound-state options,
    // crossed with every inequality-row subset at each leaf. Runs exactly
    // once (over just the inequality subsets) when n == 0.
    std::vector<std::size_t> option_idx(static_cast<std::size_t>(n), 0);
    bool more = true;
    while (more) {
        for (Index i = 0; i < n; ++i) {
            state[static_cast<std::size_t>(i)] =
                var_options[static_cast<std::size_t>(i)][option_idx[static_cast<std::size_t>(i)]];
        }
        for (std::uint64_t mask = 0; mask < ineq_combos; ++mask) {
            try_candidate(mask);
        }

        // Advance the odometer; `more` becomes false once the last digit
        // rolls over (or immediately, when n == 0).
        Index i = n - 1;
        for (; i >= 0; --i) {
            const auto sz = static_cast<std::size_t>(i);
            ++option_idx[sz];
            if (option_idx[sz] < var_options[sz].size()) {
                break;
            }
            option_idx[sz] = 0;
        }
        if (i < 0) {
            more = false;
        }
    }
}

// Smallest eigenvalue of Z^T H Z, where Z is an ORTHONORMAL basis of the null
// space of `active` (the candidate's active-constraint gradients). Returns
// +infinity when that null space is trivial: with every direction pinned there
// is nothing for negative curvature to live in, so the point is vacuously
// second-order consistent.
//
// The basis is orthonormalized rather than used raw. By Sylvester's law of
// inertia any full-column-rank basis gives the same eigenvalue SIGNS, so the
// accept/reject decision would survive a skewed basis -- but the eigenvalue
// MAGNITUDES would be scaled by the basis' conditioning, and the test here is
// against a fixed absolute tolerance. Orthonormalizing keeps the tolerance
// meaning what it says.
inline double reduced_curvature(const Eigen::MatrixXd &Hd, const Eigen::MatrixXd &active) {
    const Index n = Hd.rows();
    Eigen::MatrixXd Z;
    if (active.rows() == 0) {
        Z = Eigen::MatrixXd::Identity(n, n);
    } else {
        const Eigen::FullPivLU<Eigen::MatrixXd> lu(active);
        if (lu.rank() >= n) {
            return std::numeric_limits<double>::infinity();
        }
        const Eigen::MatrixXd ker = lu.kernel();
        const Eigen::HouseholderQR<Eigen::MatrixXd> qr(ker);
        Z = qr.householderQ() * Eigen::MatrixXd::Identity(n, ker.cols());
    }
    if (Z.cols() == 0) { // n == 0; no direction to test
        return std::numeric_limits<double>::infinity();
    }
    const Eigen::MatrixXd Hred = Z.transpose() * Hd * Z;
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(Hred);
    return es.eigenvalues().minCoeff();
}

} // namespace detail

inline QpSolution solve_dense_oracle(const QpProblem &qp) {
    bool have_best = false;
    double best_obj = 0.0;
    Index best_active_count = 0;
    QpSolution best;

    detail::enumerate_kkt_candidates(qp, [&](const detail::OracleCandidate &cand) {
        if (have_best) {
            // For a strictly convex QP the optimal x (and objective) is
            // unique, so distinct accepted candidates should agree on obj up
            // to solver noise; treat near-ties as the same vertex reached
            // through a different (possibly incomplete) active-set labeling,
            // and prefer whichever labels the most constraints as active so
            // the reported active flags/multipliers are as complete as
            // possible.
            constexpr double kObjTieTol = 1e-9;
            if (cand.objective > best_obj + kObjTieTol) {
                return;
            }
            if (cand.objective > best_obj - kObjTieTol && cand.active_count <= best_active_count) {
                return;
            }
        }
        have_best = true;
        best_obj = cand.objective;
        best_active_count = cand.active_count;
        best = cand.solution;
    });

    if (!have_best) {
        throw std::runtime_error(
            "solve_dense_oracle: no feasible, correctly-signed KKT point found over the enumerated "
            "active sets (problem may be infeasible, unbounded, or exceed the oracle's size)");
    }

    return best;
}

// Every LOCAL MINIMIZER of `qp`, for an arbitrary (including indefinite) H --
// the ground truth an indefinite-H engine answer is checked against, since for
// a nonconvex QP there is no single "the" optimum to compare with and the two
// ws_algebra modes may legitimately walk to different minimizers.
//
// Returns an empty vector when there is none (unlike solve_dense_oracle, which
// throws); still throws on an invalid problem or one too large to enumerate.
//
// WHAT IS ACCEPTED. Every KKT point of the shared enumeration whose reduced
// Hessian Z^T H Z is PSD to detail::kOracleCurvatureTol, where Z spans the null
// space of the candidate's active constraint gradients (equality rows, active
// inequality rows, and one gradient row per pinned bound). That is the
// second-order NECESSARY condition, computed on the null space rather than on
// the critical cone.
//
// DEDUPING, and why it is not just cosmetic. One point x can be reached by
// several active-set labelings -- any constraint whose multiplier is exactly
// zero may be labeled active or inactive without moving x -- and those
// labelings have DIFFERENT null spaces, hence different reduced Hessians.
// Candidates within kOracleDedupeTol in the infinity norm are therefore grouped
// as one point, and the group is accepted only if EVERY labeling in it passes.
// Since dropping a constraint from a labeling can only ENLARGE the null space,
// that is equivalent to testing the largest null space the enumeration found
// for x -- the strictest of the available tests, and the one closest to the
// critical cone. The representative reported for the group is the labeling with
// the most active constraints, so the returned active flags and multipliers are
// as complete as possible (matching solve_dense_oracle's tie-break).
//
// WHERE THIS IS INEXACT, stated in the direction it actually errs. Write C for
// the true critical cone at x. Then
//     null(smallest labeling)  CONTAINS  C  CONTAINS  null(largest labeling),
// because the smallest labeling drops exactly the weakly active constraints,
// whose directions C admits only on one side. Second-order necessity is "H is
// PSD on C". This oracle tests the LEFT-hand space, so:
//   - It is STRICTER than necessity. It can in principle omit a genuine local
//     minimizer -- one whose enlarged null space carries negative curvature
//     along a direction d with NEITHER +d NOR -d in C. Stating the precondition
//     exactly, because it is not just "a weakly active constraint exists":
//     curvature is even in the sign of d, so ONE weakly active constraint is
//     never enough -- it leaves C containing the ray +d or the ray -d, and both
//     carry the same negative curvature, so rejecting is correct. What is
//     needed is either TWO weakly active constraints disagreeing in sign on d
//     (a_j . d < 0 and a_k . d > 0 cut C down to {0} on span{d}, and this
//     happens even with a ONE-dimensional null space), or a null space of
//     dimension >= 2 in which the negative-curvature cone misses C. No such
//     case occurs in this suite. Testing the RIGHT-hand space instead would be
//     too permissive: it
//     admits genuine saddles, and would have admitted the one pinned by
//     QpEngineIndefinite.WeaklyActiveBoundIsNotCertifiedOptimal.
//     Strict is the right side to err on for an oracle used to VALIDATE a
//     solver -- an over-permissive oracle silently weakens every battery built
//     on it, while an over-strict one fails loudly and gets investigated.
//   - PSD is second-order NECESSARY, not sufficient. A point with a SINGULAR
//     reduced Hessian may be non-strict: for a quadratic objective a zero
//     eigenvalue is a genuinely flat direction, so the point belongs to a
//     connected set of equally-valued points rather than being an isolated
//     minimizer. Such a point is still a valid landing place for a descent
//     method, which is what this oracle is used to judge.
//
// kFixed: the enumeration models lower(i) == upper(i) as an ordinary bound, so
// no returned bound_state is ever kFixed even where the engine reports it.
// Comparisons against engine output must not compare bound_state on such
// variables (Phase-1 deferred).
inline std::vector<QpSolution> enumerate_local_minimizers(const QpProblem &qp) {
    // Before touching qp.H: enumerate_kkt_candidates validates too, but only
    // after this function would already have symmetrized a possibly non-square
    // H -- which trips an Eigen assert rather than the documented
    // std::invalid_argument.
    qp.validate();
    const Eigen::MatrixXd Hd = Eigen::MatrixXd(qp.H).selfadjointView<Eigen::Upper>();

    struct Group {
        QpSolution representative;
        Index active_count = 0;
        // Smallest reduced-Hessian eigenvalue over every labeling of this
        // point -- i.e. the value from the labeling with the largest null
        // space, which is the strictest test available for it.
        double worst_curvature = 0.0;
    };
    std::vector<Group> groups;

    detail::enumerate_kkt_candidates(qp, [&](const detail::OracleCandidate &cand) {
        const double curvature = detail::reduced_curvature(Hd, cand.active_jacobian);
        for (auto &group : groups) {
            if ((group.representative.x - cand.solution.x).lpNorm<Eigen::Infinity>() <=
                detail::kOracleDedupeTol) {
                group.worst_curvature = std::min(group.worst_curvature, curvature);
                if (cand.active_count > group.active_count) {
                    group.representative = cand.solution;
                    group.active_count = cand.active_count;
                }
                return;
            }
        }
        groups.push_back(Group{cand.solution, cand.active_count, curvature});
    });

    std::vector<QpSolution> minimizers;
    for (auto &group : groups) {
        if (group.worst_curvature >= -detail::kOracleCurvatureTol) {
            minimizers.push_back(std::move(group.representative));
        }
    }
    return minimizers;
}

} // namespace tycho::sqp
