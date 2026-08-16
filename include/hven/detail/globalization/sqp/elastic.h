#pragma once

// elastic.h -- the elastic (l1 exact-penalty) tier's subproblem construction,
// carved verbatim out of drivers/sqp_driver.h (phase-C S3, restructure only).
// The comments below still speak from that header's point of view: the
// ELASTIC TIER and WARM SEEDING notes and qp_failure_is_retryable remain in
// drivers/sqp_driver.h, which includes this file at the exact point the
// carved code stood; kZeroStepScale ("further down this file") is now
// detail/globalization/sqp/trust_region.h's.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include <hven/core/solver_status.h>
#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>

namespace hven::solvers {

// =============================================================================
// THE ELASTIC TIER (Task 8). See this header's ELASTIC TIER note for the
// algorithm; everything from here to qp_failure_is_retryable is the
// CONSTRUCTION, factored out as free functions so it can be tested away from
// the driver's loop and away from any engine (the precedent set by
// qp_failure_is_retryable and build_soc_subproblem).
// =============================================================================

// The penalty ladder. rho starts at kElasticRhoInit and is multiplied by
// kElasticRhoFactor until the relaxation closes or rho reaches
// kElasticRhoMax -- SIX escalations at these values, so at most SEVEN solves
// per activation.
//
// WHY A LADDER AT ALL, and why these endpoints. The elastic subproblem is an
// l1 EXACT PENALTY reformulation, and the exact-penalty threshold is the
// subproblem's OWN multiplier norm: for rho > ||lambda*||inf of the
// unrelaxed QP, paying the penalty is strictly worse than satisfying the row,
// so the elastic solution has s == 0 and IS the unrelaxed solution (pinned by
// SqpDriverElastic.ElasticReformulationRecoversAFeasibleQpsSolution, which
// straddles the threshold on a hand-derived fixture). rho_init = 1e2 is well
// above the multipliers of a well-scaled subproblem; rho_max = 1e8 is where
// the penalty term starts to dominate the double-precision arithmetic of the
// H block (the engine's own regularization pair sits at 1e-8, so 1e8 is the
// reciprocal scale at which the two meet). Neither is a paper constant; they
// are the brief's, recorded here so a re-derivation has one place to edit.
inline constexpr double kElasticRhoInit = 1e2;
inline constexpr double kElasticRhoMax = 1e8;
inline constexpr double kElasticRhoFactor = 10.0;

// THE STALL EARLY-EXIT's own tolerance (Phase-5 Task 10; see this file's
// ELASTIC TIER note, THE STALL EARLY-EXIT paragraph, for the derivation and
// the SqpOptions::elastic_ladder_early_exit lever this gates). Two
// consecutive rungs' augmented solutions are the SAME rung, not merely
// close, when they differ by less than this fraction of the previous rung's
// own scale -- a NUMERICAL-ZERO threshold, exactly kZeroStepScale's own
// (further down this file), and for the identical reason: at 1e-12 relative,
// what "differs" is rounding, not a rung that changed anything.
inline constexpr double kElasticStallScale = 1e-12;

// ElasticQp::eq_slack/ineq_slack entry for a row that was NOT relaxed.
inline constexpr Index kNoSlack = -1;

// The elastic reformulation of ONE subproblem: the augmented QpProblem plus
// the bookkeeping needed to read a solution of it back in the original
// variables.
//
// The augmented problem has n + ns variables, the SAME me and mi (slacks add
// COLUMNS, never rows), and the trust-region window folded into the original
// block's box -- see build_elastic_subproblem for all of it.
struct ElasticQp {
    QpProblem qp; // the augmented problem, n_orig + ns variables
    Index n_orig = 0;
    Index ns = 0; // number of slack columns == number of RELAXED rows
    // Per row: the slack's COLUMN INDEX in qp (>= n_orig), or kNoSlack.
    std::vector<Index> eq_slack;   // size me
    std::vector<Index> ineq_slack; // size mi
    // The l1 violation of the RELAXED rows at p_ref, i.e. the slack vector of
    // the feasible-by-construction witness point (p_ref, residuals). It is
    // the reference the driver's usability test compares against: an elastic
    // solution whose slacks sum to this made no progress on the linearized
    // violation at all.
    double violation_l1 = 0.0;
    // clamp(0, lower, upper) -- the point the residuals above were measured
    // at, and the engine's own solve center for a seed whose x is zeroed
    // (qp_engine.h's start_center). It is 0 whenever the iterate is inside
    // its bounds, which every driver-produced iterate is.
    Vec p_ref;
    // COLUMN SCALE of each slack, sigma_j = max(1, |residual_j|), size ns.
    // The slack VARIABLE is the row's violation measured in units of
    // sigma_j -- see build_elastic_subproblem's FINITE, SCALED SLACKS
    // paragraph for why the variable cannot be the violation itself.
    Vec slack_scale;

    // The slack block of an augmented primal vector, in the SCALED variable;
    // empty when ns == 0. Callers judging feasibility want
    // slack_violations() instead -- this is the raw variable.
    Vec slacks(const Vec &x_aug) const {
        return x_aug.size() == qp.n() ? Vec(x_aug.tail(ns)) : Vec::Zero(ns);
    }

    // The ACTUAL linearized violation each relaxed row was left with,
    // sigma_j * s_j -- the quantity in the same units as violation_l1 and as
    // SqpOptions::feas_tol, and therefore the one every test in the driver
    // (the escalation ladder's "materially nonzero", the usability
    // comparison) is written against.
    Vec slack_violations(const Vec &x_aug) const { return slacks(x_aug).cwiseProduct(slack_scale); }
};

// Builds the elastic reformulation of `qp` at penalty rho.
//
// WHICH ROWS GET SLACKS: exactly those VIOLATED at p_ref = clamp(0, lower,
// upper), measured with tolerance `tol`. That choice is what makes the
// augmented problem FEASIBLE BY CONSTRUCTION -- see the header note -- and it
// is also the minimal one: a row satisfied at p_ref is satisfied by the
// witness point, so relaxing it could only add variables the witness never
// uses.
//
//   INEQUALITY row j (a^T p <= b), violated iff a^T p_ref - b > tol:
//       ONE slack, coefficient -1:   a^T p - s <= b,  s >= 0.
//   EQUALITY row k (a^T p = b), violated iff |b - a^T p_ref| > tol:
//       ONE SIGNED slack, coefficient sigma = sign(b - a^T p_ref):
//                                    a^T p + sigma*s = b,  s >= 0.
//
// WHY ONE SIGNED SLACK RATHER THAN THE TEXTBOOK s+ / s- PAIR. The pair costs
// two columns per equality and buys the ability to violate the row in EITHER
// direction; the signed slack costs one and buys the ability to violate it in
// the direction the row is ALREADY violated in. Everything this tier needs
// lives on that side: the witness point (p_ref, |residual|) is feasible, so
// the elastic QP cannot be infeasible; and the relaxed row a^T p <= b (for
// sigma = +1) is a SUPERSET of the original equality containing p_ref, so no
// step the unrelaxed subproblem could have taken is lost. What IS given up is
// the freedom to overshoot the target and pay for it -- a step that would
// drive a^T p past b. That is a real restriction, it is the reason the pair
// is the textbook choice, and the pair remains the named upgrade if a fixture
// ever shows a step being blocked by it. Nothing in this file's suite does.
//
// H IS EXTENDED WITH ZEROS -- the slack block is purely LINEAR (the penalty
// entries are rho*sigma_j against s >= 0, so the objective term IS
// rho * ||violation||_1). Concretely the augmented H is the original's
// entries in an (n+ns)-square frame with no new nonzeros at all, which (i)
// trivially satisfies QpProblem::validate's upper-triangle rule (no entry
// moved, so none can land below the diagonal) and (ii) leaves the augmented
// Hessian only POSITIVE SEMIdefinite in the slack directions. The second is
// safe here for two independent reasons: the engine regularizes the H block
// by opts.primal_delta (1e-8 by default), and the objective is strictly
// INCREASING in every slack (coefficient rho*sigma_j > 0) against s >= 0, so
// no slack direction is a direction of decrease and the zero curvature there
// cannot produce an unbounded ray.
//
// (An earlier version of this paragraph added "-- the augmented QP is bounded
// below on a bounded box, which the folded window guarantees". Dropped in fix
// round 1: the window guarantees no such thing at tr_radius == +inf, where
// the box is the caller's own and may be unbounded. The slack argument above
// does not need it, and boundedness in p is the ORIGINAL subproblem's
// business, unchanged by this construction.)
//
// THE TRUST REGION IS FOLDED INTO THE BOX, not passed as SolveOverrides::
// tr_radius, and that is a correctness requirement rather than a style
// choice: qp_engine.h's section 6 applies the radius to EVERY variable
// index, so a radius of Delta would also cap every slack at Delta (their
// window is [max(0, 0-Delta), min(inf, 0+Delta)]) -- and the slack a violated
// row needs is the size of the VIOLATION, which has nothing to do with the
// radius. Capping it would hand back an infeasible elastic QP, defeating the
// tier. So the window is computed here, exactly as section 6 computes it
// (about p_ref, since the seed's primal is zeroed), applied to the ORIGINAL
// block only, and the solve is made with the +inf sentinel.
// `tr_radius` may be +inf, in which case the box is the caller's unchanged.
inline ElasticQp build_elastic_subproblem(const QpProblem &qp, double tr_radius, double rho,
                                          double tol) {
    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();

    ElasticQp e;
    e.n_orig = n;
    e.p_ref = Vec::Zero(n);

    Vec lo(n), up(n);
    const bool tr = std::isfinite(tr_radius);
    for (Index i = 0; i < n; ++i) {
        e.p_ref(i) = std::min(std::max(0.0, qp.lower(i)), qp.upper(i));
        lo(i) = tr ? std::max(qp.lower(i), e.p_ref(i) - tr_radius) : qp.lower(i);
        up(i) = tr ? std::min(qp.upper(i), e.p_ref(i) + tr_radius) : qp.upper(i);
    }

    // Which rows are violated at p_ref, and by how much.
    const Vec eq_res = me > 0 ? Vec(qp.be - qp.Ae * e.p_ref) : Vec(0);
    const Vec in_res = mi > 0 ? Vec(qp.Ai * e.p_ref - qp.bi) : Vec(0);
    e.eq_slack.assign(static_cast<std::size_t>(me), kNoSlack);
    e.ineq_slack.assign(static_cast<std::size_t>(mi), kNoSlack);
    std::vector<double> scales;
    Index ns = 0;
    for (Index k = 0; k < me; ++k) {
        if (std::abs(eq_res(k)) > tol) {
            e.eq_slack[static_cast<std::size_t>(k)] = n + ns++;
            e.violation_l1 += std::abs(eq_res(k));
            scales.push_back(std::max(1.0, std::abs(eq_res(k))));
        }
    }
    for (Index j = 0; j < mi; ++j) {
        if (in_res(j) > tol) {
            e.ineq_slack[static_cast<std::size_t>(j)] = n + ns++;
            e.violation_l1 += in_res(j);
            scales.push_back(std::max(1.0, in_res(j)));
        }
    }
    e.ns = ns;
    e.slack_scale = Eigen::Map<const Vec>(scales.data(), ns);
    const Index n2 = n + ns;

    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<std::size_t>(qp.H.nonZeros()));
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
            t.emplace_back(it.row(), it.col(), it.value());
        }
    }
    e.qp.H = SpMatRM(n2, n2);
    e.qp.H.setFromTriplets(t.begin(), t.end());
    e.qp.H.makeCompressed();

    e.qp.g = Vec::Zero(n2);
    e.qp.g.head(n) = qp.g;
    e.qp.g.tail(ns) = rho * e.slack_scale; // rho * (the VIOLATION), see set_elastic_penalty

    t.clear();
    t.reserve(static_cast<std::size_t>(qp.Ae.nonZeros()) + static_cast<std::size_t>(me));
    for (Index k = 0; k < me; ++k) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(qp.Ae, k); it; ++it) {
            t.emplace_back(it.row(), it.col(), it.value());
        }
        const Index col = e.eq_slack[static_cast<std::size_t>(k)];
        if (col != kNoSlack) {
            const double sigma = e.slack_scale(col - n);
            t.emplace_back(k, col, eq_res(k) > 0.0 ? sigma : -sigma);
        }
    }
    e.qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(me, n2);
    e.qp.Ae.setFromTriplets(t.begin(), t.end());
    e.qp.Ae.makeCompressed();
    e.qp.be = qp.be;

    t.clear();
    t.reserve(static_cast<std::size_t>(qp.Ai.nonZeros()) + static_cast<std::size_t>(mi));
    for (Index j = 0; j < mi; ++j) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(qp.Ai, j); it; ++it) {
            t.emplace_back(it.row(), it.col(), it.value());
        }
        const Index col = e.ineq_slack[static_cast<std::size_t>(j)];
        if (col != kNoSlack) {
            t.emplace_back(j, col, -e.slack_scale(col - n));
        }
    }
    e.qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(mi, n2);
    e.qp.Ai.setFromTriplets(t.begin(), t.end());
    e.qp.Ai.makeCompressed();
    e.qp.bi = qp.bi;

    e.qp.lower = Vec::Zero(n2);
    e.qp.upper = Vec::Zero(n2);
    e.qp.lower.head(n) = lo;
    e.qp.upper.head(n) = up;
    // THE SLACK CEILING (fix round 1): violation_l1 IN ACTUAL UNITS, i.e.
    // violation_l1 / sigma_j in the scaled variable, rather than +inf. It
    // says the relaxation may not leave the linearization MORE violated in
    // total than it already is at p_ref, and it cannot cut off the witness
    // point, whose actual violation |r_j| is at most sum_k |r_k| =
    // violation_l1 by construction. A slack that saturates it is reported
    // kAtUpper, which qp_engine.h's is_runaway skips outright ("pinned at a
    // bound: it did not run away").
    for (Index k = 0; k < ns; ++k) {
        e.qp.upper(n + k) = e.violation_l1 / e.slack_scale(k);
    }
    return e;
}

// Rewrites the penalty in place -- the SLACK BLOCK OF g AND NOTHING ELSE.
// The entry is rho * sigma_j, not rho: the slack VARIABLE is the row's
// violation in units of sigma_j (see build_elastic_subproblem), so this is
// what keeps the objective term equal to rho * (the ACTUAL violation), and
// with it the exact-penalty threshold as a statement about the subproblem's
// own multipliers at any constraint scale.
//
// WHAT THIS BUYS, STATED EXACTLY (fix round 1 corrected an earlier version of
// this comment that overclaimed): H/Ae/Ai stay byte-identical across the
// ladder, so the STRUCTURAL half of qp_engine.h's HOT-START REUSE key
// survives -- g is not part of that key. Necessary, and NOT sufficient:
// condition (b) of that note (the seed working set equals the immediately
// preceding solve's EXIT working set) is a property of how the caller SEEDS
// the next rung, not of this function. A caller that re-seeds every rung from
// the same stale solution pays a K0 rebuild per rung regardless -- which is
// what the driver's ladder did before fix round 1, and why it now chains the
// seed.
inline void set_elastic_penalty(ElasticQp &e, double rho) {
    e.qp.g.tail(e.ns) = rho * e.slack_scale;
}

// The warm seed for an elastic solve, mapped from the FAILED (kInfeasible)
// solve of the unrelaxed subproblem: the ORIGINAL variables' bound states
// carry over index-for-index (same variables, same box up to the folded
// window, so the labeling is meaningful), the general rows' activity carries
// over unchanged (same rows), and the SLACK block starts FRESH -- kFree, at
// x = 0 -- because there is no previous solve that had any opinion about it.
// The primal is zeroed exactly as everywhere else in this file: it is the
// engine's window CENTER, and in step variables that center must be p = 0
// (this header's WARM SEEDING note).
//
// A size mismatch degrades to "no hint" rather than throwing: the engine
// ignores a wrongly-sized seed block silently, and the seed is an
// optimization, never a correctness input.
inline QpSolution elastic_seed(const ElasticQp &e, const QpSolution &failed) {
    const Index n2 = e.qp.n();
    QpSolution s;
    s.status = QpStatus::kOptimal;
    s.x = Vec::Zero(n2);
    s.bound_state.assign(static_cast<std::size_t>(n2), BoundState::kFree);
    if (static_cast<Index>(failed.bound_state.size()) == e.n_orig) {
        for (Index i = 0; i < e.n_orig; ++i) {
            s.bound_state[static_cast<std::size_t>(i)] =
                failed.bound_state[static_cast<std::size_t>(i)];
        }
    }
    s.ineq_active = static_cast<Index>(failed.ineq_active.size()) == e.qp.mi()
                        ? failed.ineq_active
                        : std::vector<bool>(static_cast<std::size_t>(e.qp.mi()), false);
    return s;
}

// Reads an augmented solution back in the ORIGINAL variables, so that every
// consumer downstream of the elastic tier (the funnel judgment, the warm
// seed, the history row, the accepted step) sees exactly the shape a plain
// subproblem would have produced.
//
// THE STEP is the original block of the augmented primal; the slacks are
// dropped (they are not part of any NLP variable).
//
// THE RADIUS BIT IS RE-DERIVED HERE, because the elastic solve was given its
// window as REAL bounds and so reports tr_active all-false and bound_state
// kAtLower/kAtUpper at a TR-truncated bound. This function applies
// qp_engine.h's section-6 reporting exclusions itself -- an index pinned at a
// bound that is strictly tighter than the CALLER's own bound is TR-pinned, so
// it is reported kFree with z == 0 and tr_active set -- which keeps
// SqpIterate::tr_binding, the radius growth rule and the warm seed's
// real-bound-only view identical across the two paths.
//
// carry_multipliers IS FALSE WHENEVER THE RELAXATION STAYED OPEN, and that is
// not a nicety: at an elastic solution with s_j > 0 the row's own multiplier
// is FORCED to the penalty (stationarity in s_j reads rho - sigma*lambda_j -
// z_j = 0 with z_j = 0 when s_j is off its bound, so |lambda_j| = rho), and
// the contamination spreads to rows that merely SHARE A VARIABLE with a
// relaxed one -- measured on
// SqpDriverElastic.InconsistentLinearizationRecovers' fixture, where the row
// with s == 0 prices at rho as well. Those numbers are penalty parameters
// wearing the shape of prices; feeding them to eval_hess would build the next
// iterate's Hessian out of rho (there, H = diag(2 - 4*rho, 2) at rho = 1e8).
// They are zeroed instead, exactly as qp_engine.h zeroes the multipliers on
// its own kInfeasible/kNumericalError exits and for the identical reason.
// When the relaxation CLOSED (every slack at 0) there is no contamination to
// worry about: the elastic solution then satisfies every original row, so it
// solves the UNRELAXED subproblem and its multipliers are that subproblem's.
inline QpSolution elastic_project(const ElasticQp &e, const QpProblem &qp, const QpSolution &aug,
                                  bool carry_multipliers) {
    const Index n = e.n_orig;
    QpSolution out;
    out.status = aug.status;
    out.counters = aug.counters;
    out.x = aug.x.size() >= n ? Vec(aug.x.head(n)) : Vec::Zero(n);
    // z IS QUARANTINED WITH THE MULTIPLIERS (fix round 1), not carried
    // independently: a bound price at index i is the stationarity residual
    // there, so it is contaminated by exactly the same rho the row
    // multipliers are (see carry_multipliers below). SqpDriver itself never
    // reads QpSolution::z -- it reports the MODEL-implied bound multiplier
    // instead (this header's REPORTED BOUND MULTIPLIER note) -- so this
    // changes nothing today; it removes a trap for the next reader.
    out.z = carry_multipliers && aug.z.size() >= n ? Vec(aug.z.head(n)) : Vec::Zero(n);
    out.lambda_e =
        carry_multipliers && aug.lambda_e.size() == qp.me() ? aug.lambda_e : Vec::Zero(qp.me());
    out.lambda_i =
        carry_multipliers && aug.lambda_i.size() == qp.mi() ? aug.lambda_i : Vec::Zero(qp.mi());
    out.ineq_active = static_cast<Index>(aug.ineq_active.size()) == qp.mi()
                          ? aug.ineq_active
                          : std::vector<bool>(static_cast<std::size_t>(qp.mi()), false);
    out.bound_state.assign(static_cast<std::size_t>(n), BoundState::kFree);
    out.tr_active.assign(static_cast<std::size_t>(n), false);
    if (static_cast<Index>(aug.bound_state.size()) < n) {
        return out;
    }
    for (Index i = 0; i < n; ++i) {
        const auto si = static_cast<std::size_t>(i);
        const BoundState st = aug.bound_state[si];
        const bool tr = (st == BoundState::kAtLower && e.qp.lower(i) > qp.lower(i)) ||
                        (st == BoundState::kAtUpper && e.qp.upper(i) < qp.upper(i));
        out.tr_active[si] = tr;
        out.bound_state[si] = tr ? BoundState::kFree : st;
        if (tr) {
            out.z(i) = 0.0;
        }
    }
    return out;
}

} // namespace hven::solvers
