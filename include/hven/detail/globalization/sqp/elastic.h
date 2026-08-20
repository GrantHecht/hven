#pragma once

// elastic.h -- the elastic (l1 exact-penalty) tier's subproblem construction,
// carved verbatim out of drivers/sqp_driver.h (phase-C S3, restructure only).
// The comments below still speak from that header's point of view: the
// ELASTIC TIER, WARM SEEDING and REPORTED BOUND MULTIPLIER notes and
// qp_failure_is_retryable remain in drivers/sqp_driver.h, which includes this
// file at the exact point the carved code stood; kZeroStepScale ("further down
// this file") is now detail/globalization/sqp/trust_region.h's. THAT IS THE
// RULE, NOT AN EXHAUSTIVE LIST: any reference of the form "this file"/"this
// header" that does not resolve here resolves in drivers/sqp_driver.h.
//
// WHERE THE DEFINITIONS LIVE (M3 phase-C T6): the bodies of ElasticQp's two
// member functions and of build_elastic_subproblem, elastic_seed and
// elastic_project are in src/globalization/sqp/soc_elastic_restoration.cpp,
// together with soc.h's and restoration.h's. That file's banner carries the
// measurement the carve rests on. ONE FUNCTION STAYED: set_elastic_penalty is
// still defined here, and its own note says why. This header keeps every
// declaration, every constant, ElasticQp's layout, and every word of the
// derivation.

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
    Vec slacks(const Vec &x_aug) const;

    // The ACTUAL linearized violation each relaxed row was left with,
    // sigma_j * s_j -- the quantity in the same units as violation_l1 and as
    // SqpOptions::feas_tol, and therefore the one every test in the driver
    // (the escalation ladder's "materially nonzero", the usability
    // comparison) is written against.
    Vec slack_violations(const Vec &x_aug) const;
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
ElasticQp build_elastic_subproblem(const QpProblem &qp, double tr_radius, double rho, double tol);

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
//
// THE ONE DEFINITION IN THIS FILE'S SCOPE THAT M3 PHASE-C T6 LEFT INLINE, and
// the reason is a measurement rather than a preference. At T6's base commit
// this function emitted NO symbol in any of the 68 objects of a clean Release
// build and had ZERO direct call sites anywhere -- which is what full inlining
// at every use looks like, and is the same test phase-C T4 applied to
// FunnelStrategy's four fully-inlined members before deciding they stay in
// globalization.h. It is also the one function in the tier whose stated reason
// for being carvable does not hold: the plan's premise for the elastic
// builders is that they ALLOCATE, so a call is already dominated. This one
// allocates nothing -- it is a single Eigen assignment into a block that
// already exists, on the ladder's own rung path. Out-of-lining it would turn a
// measured inline into a call and falsify the premise the rest of the carve
// rests on, so it is deliberately not done.
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
QpSolution elastic_seed(const ElasticQp &e, const QpSolution &failed);

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
QpSolution elastic_project(const ElasticQp &e, const QpProblem &qp, const QpSolution &aug,
                           bool carry_multipliers);

} // namespace hven::solvers
