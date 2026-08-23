// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// elastic.h -- the elastic (l1 exact-penalty) tier's subproblem construction.
// The ELASTIC TIER, WARM SEEDING and REPORTED BOUND MULTIPLIER notes cited
// below live in drivers/sqp_driver.h, which includes this file at the point
// the construction stood; kZeroStepScale lives in
// detail/globalization/sqp/trust_region.h. Rule of thumb: any "this file"/
// "this header" reference that does not resolve here resolves in
// drivers/sqp_driver.h. The bodies of ElasticQp's two member functions and of
// build_elastic_subproblem / elastic_seed / elastic_project are in
// src/globalization/sqp/soc_elastic_restoration.cpp (with soc.h's and
// restoration.h's). ONE definition stays inline HERE: set_elastic_penalty --
// its own note says why.

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

// The penalty ladder: rho starts at kElasticRhoInit and is multiplied by
// kElasticRhoFactor until the relaxation closes or rho reaches kElasticRhoMax
// -- six escalations at these values, so at most SEVEN solves per activation.
//
// WHY A LADDER AT ALL, and why these endpoints. The elastic subproblem is an
// l1 EXACT PENALTY reformulation, and the exact-penalty threshold is the
// subproblem's OWN multiplier norm: for rho > ||lambda*||inf of the unrelaxed
// QP, paying the penalty is strictly worse than satisfying the row, so the
// elastic solution has s == 0 and IS the unrelaxed solution (pinned by a suite
// fixture that straddles the threshold). rho_init = 1e2 sits well above the
// multipliers of a well-scaled subproblem; rho_max = 1e8 is where the penalty
// term starts to dominate the double-precision arithmetic of the H block (the
// engine's own regularization pair sits at 1e-8, so 1e8 is the reciprocal scale
// at which the two meet). Neither is a paper constant; they are recorded here
// so a re-derivation has one place to edit.
inline constexpr double kElasticRhoInit = 1e2;
inline constexpr double kElasticRhoMax = 1e8;
inline constexpr double kElasticRhoFactor = 10.0;

/// The stall early-exit tolerance: two consecutive rungs' augmented solutions
/// count as THE SAME rung, not merely close, when they differ by less than this
/// fraction of the previous rung's own scale -- a NUMERICAL-ZERO threshold,
/// exactly kZeroStepScale's own and for the identical reason: at 1e-12
/// relative, what "differs" is rounding, not a rung that changed anything.
inline constexpr double kElasticStallScale = 1e-12;

/// ElasticQp::eq_slack/ineq_slack entry for a row that was NOT relaxed.
inline constexpr Index kNoSlack = -1;

/// @brief The elastic reformulation of ONE subproblem: the augmented QpProblem
/// plus the bookkeeping needed to read a solution back in the original
/// variables.
///
/// The augmented problem has n + ns variables, the SAME me and mi (slacks add
/// COLUMNS, never rows), and the trust-region window folded into the original
/// block's box -- see build_elastic_subproblem.
struct ElasticQp {
    QpProblem qp; ///< The augmented problem, n_orig + ns variables.
    Index n_orig = 0;
    Index ns = 0; ///< Number of slack columns == number of RELAXED rows.
    /// Per row: the slack's COLUMN INDEX in qp (>= n_orig), or kNoSlack.
    std::vector<Index> eq_slack;   ///< Size me.
    std::vector<Index> ineq_slack; ///< Size mi.
    /// The l1 violation of the RELAXED rows at p_ref — the slack vector of the
    /// feasible-by-construction witness point (p_ref, residuals). The reference
    /// the driver's usability test compares against: an elastic solution whose
    /// slacks sum to this made no progress on the linearized violation at all.
    double violation_l1 = 0.0;
    /// clamp(0, lower, upper) -- the point the residuals were measured at, and
    /// the engine's own solve center for a seed whose x is zeroed. Zero whenever
    /// the iterate is inside its bounds, which every driver-produced iterate is.
    Vec p_ref;
    /// COLUMN SCALE of each slack, sigma_j = max(1, |residual_j|), size ns. The
    /// slack VARIABLE is the row's violation measured in units of sigma_j (see
    /// build_elastic_subproblem for why the variable cannot be the violation
    /// itself).
    Vec slack_scale;

    /// The slack block of an augmented primal vector, in the SCALED variable;
    /// empty when ns == 0. Callers judging feasibility want slack_violations()
    /// instead -- this is the raw variable.
    Vec slacks(const Vec &x_aug) const;

    /// The ACTUAL linearized violation each relaxed row was left with,
    /// sigma_j * s_j -- the quantity in the same units as violation_l1 and
    /// SqpOptions::feas_tol, and therefore the one every test in the driver is
    /// written against.
    Vec slack_violations(const Vec &x_aug) const;
};

/// @brief Builds the elastic reformulation of `qp` at penalty rho.
///
/// WHICH ROWS GET SLACKS: exactly those VIOLATED at p_ref = clamp(0, lower,
/// upper), measured with tolerance `tol`. That choice makes the augmented
/// problem FEASIBLE BY CONSTRUCTION (the witness point (p_ref, residuals)
/// satisfies every relaxed row) and is also minimal: a row satisfied at p_ref
/// is satisfied by the witness, so relaxing it could only add variables the
/// witness never uses.
///
///   INEQUALITY row j (a^T p <= b), violated iff a^T p_ref - b > tol:
///       ONE slack, coefficient -1:   a^T p - s <= b,  s >= 0.
///   EQUALITY row k (a^T p = b), violated iff |b - a^T p_ref| > tol:
///       ONE SIGNED slack, coefficient sigma = sign(b - a^T p_ref):
///                                    a^T p + sigma*s = b,  s >= 0.
///
/// WHY ONE SIGNED SLACK RATHER THAN THE TEXTBOOK s+/s- PAIR. The pair costs two
/// columns per equality and buys violation in EITHER direction; the signed
/// slack costs one and buys violation in the direction the row is ALREADY
/// violated in. Everything this tier needs lives on that side: the witness is
/// feasible so the elastic QP cannot be infeasible; and the relaxed row is a
/// SUPERSET of the original equality containing p_ref, so no step the unrelaxed
/// subproblem could have taken is lost. What IS given up is the freedom to
/// overshoot the target and pay for it -- a real restriction, and the reason
/// the pair remains the named upgrade if a fixture ever shows a step being
/// blocked by it. Nothing in this file's suite does.
///
/// H IS EXTENDED WITH ZEROS -- the slack block is purely LINEAR (penalty
/// entries rho*sigma_j against s >= 0, so the objective term IS
/// rho*||violation||_1): no new nonzeros at all. That trivially satisfies
/// QpProblem::validate's upper-triangle rule, and leaves the augmented Hessian
/// only POSITIVE SEMIdefinite in the slack directions -- safe here for two
/// independent reasons: the engine regularizes the H block (primal_delta, 1e-8
/// by default), and the objective is strictly INCREASING in every slack
/// (coefficient rho*sigma_j > 0) against s >= 0, so no slack direction is a
/// direction of decrease and the zero curvature cannot produce an unbounded ray.
///
/// THE TRUST REGION IS FOLDED INTO THE BOX, not passed as SolveOverrides::
/// tr_radius -- a correctness requirement, not style: applying the radius to
/// EVERY variable index would cap every slack at Delta too, and the slack a
/// violated row needs is the size of the VIOLATION, which has nothing to do
/// with the radius; capping it would hand back an infeasible elastic QP,
/// defeating the tier. So the window is computed here (about p_ref, since the
/// seed's primal is zeroed), applied to the ORIGINAL block only, and the solve
/// runs with the +inf sentinel. `tr_radius` may be +inf, in which case the box
/// is the caller's unchanged.
ElasticQp build_elastic_subproblem(const QpProblem &qp, double tr_radius, double rho, double tol);

/// @brief Rewrites the penalty in place -- the SLACK BLOCK OF g AND NOTHING
/// ELSE. The entry is rho * sigma_j, not rho: the slack VARIABLE is the row's
/// violation in units of sigma_j, so this keeps the objective term equal to
/// rho * (the ACTUAL violation), and with it the exact-penalty threshold as a
/// statement about the subproblem's own multipliers at any constraint scale.
///
/// WHAT THIS BUYS, STATED EXACTLY: H/Ae/Ai stay byte-identical across the
/// ladder, so the STRUCTURAL half of qp_engine.h's HOT-START REUSE key
/// survives -- g is not part of that key. Necessary, and NOT sufficient:
/// reuse condition (b) (the seed working set equals the preceding solve's EXIT
/// working set) is a property of how the caller SEEDS the next rung; a caller
/// re-seeding from a stale solution pays a K0 rebuild per rung regardless (which
/// is why the driver chains the seed).
///
/// Deliberately left INLINE (a measurement, not a preference): this function
/// emits NO symbol in a clean Release build and has ZERO direct call sites --
/// full inlining at every use -- and it allocates nothing (one Eigen assignment
/// into an existing block, on the ladder's rung path). Unlike the tier's other
/// builders it does not allocate, so out-of-lining it would turn a measured
/// inline into a call and falsify the premise the rest of the TU carve rests on.
inline void set_elastic_penalty(ElasticQp &e, double rho) {
    e.qp.g.tail(e.ns) = rho * e.slack_scale;
}

/// @brief The warm seed for an elastic solve, mapped from the FAILED
/// (kInfeasible) solve of the unrelaxed subproblem: the ORIGINAL variables'
/// bound states carry over index-for-index (same variables, same box up to the
/// folded window), the general rows' activity carries over unchanged (same
/// rows), and the SLACK block starts FRESH -- kFree, at x = 0 -- because no
/// previous solve had any opinion about it. The primal is zeroed exactly as
/// everywhere else in this file: it is the engine's window CENTER, and in step
/// variables that center must be p = 0.
///
/// A size mismatch degrades to "no hint" rather than throwing: the engine
/// ignores a wrongly-sized seed block silently, and the seed is an optimization,
/// never a correctness input.
QpSolution elastic_seed(const ElasticQp &e, const QpSolution &failed);

/// @brief Reads an augmented solution back in the ORIGINAL variables, so every
/// consumer downstream of the elastic tier (funnel judgment, warm seed, history
/// row, accepted step) sees exactly the shape a plain subproblem would have
/// produced.
///
/// THE STEP is the original block of the augmented primal; slacks are dropped.
///
/// THE RADIUS BITS ARE RE-DERIVED HERE, because the elastic solve was given its
/// window as REAL bounds and so reports tr_active all-false and bound_state
/// kAtLower/kAtUpper at a TR-truncated bound. This function applies the
/// engine's section-6 reporting exclusions itself -- an index pinned at a bound
/// strictly tighter than the CALLER's own bound is TR-pinned, reported kFree
/// with z == 0 and tr_active set -- keeping tr_binding, the radius growth rule,
/// and the warm seed's real-bound-only view identical across both paths.
///
/// carry_multipliers IS FALSE WHENEVER THE RELAXATION STAYED OPEN, and that is
/// not a nicety: at an elastic solution with s_j > 0 the row's own multiplier
/// is FORCED to the penalty (stationarity in s_j reads rho - sigma*lambda_j -
/// z_j = 0, so |lambda_j| = rho), and the contamination spreads to rows that
/// merely SHARE A VARIABLE with a relaxed one (measured on a suite fixture
/// where a row with s == 0 prices at rho as well). Those numbers are penalty
/// parameters wearing the shape of prices; feeding them to eval_hess would
/// build the next Hessian out of rho. They are zeroed instead, exactly as the
/// engine zeroes multipliers on its own kInfeasible/kNumericalError exits and
/// for the identical reason. When the relaxation CLOSED (every slack at 0)
/// there is no contamination: the elastic solution then satisfies every
/// original row, solves the UNRELAXED subproblem, and its multipliers are that
/// subproblem's.
QpSolution elastic_project(const ElasticQp &e, const QpProblem &qp, const QpSolution &aug,
                           bool carry_multipliers);

} // namespace hven::solvers
