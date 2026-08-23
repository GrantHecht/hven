// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// warm_start.h — WarmStart, the value object EVERY SqpDriver::solve() call
// emits (SqpSolution::warm_start, sqp_types.h) describing what a solve
// learned that a SUBSEQUENT solve of a nearby problem could reuse: the
// primal/dual point, which inequalities and bounds were active there, the
// QP engine's own working set at exit, and the globalization/regularization
// state (funnel width, trust-region radius, effective primal_delta/dual_mu).
//
// SHAPE. Everything but `hot` below is plain copyable data (Vec/vector/
// WorkingSet members, no pointers, no owned resources). `hot` is a
// std::shared_ptr<const HotState>, opaque to this header on purpose --
// HotState is only FORWARD-declared here and fully defined in qp_engine.h,
// next to the engine-instance reuse machinery it is a frozen snapshot of.
// WarmStart stays COPYABLE (a shared_ptr copies cheaply, sharing ownership),
// but it is no longer lifetime-free in the way the rest of this struct is:
// `hot`, when non-null, keeps a sparse KKT factor (and its live backend
// session) alive for as long as any copy of this WarmStart does.
// SAME-PROCESS ONLY: nothing here serializes `hot`, and none of this
// struct's own (de)serialization -- there is none -- is expected to grow
// any. A default-constructed WarmStart (valid == false) leaves `hot` null,
// exactly like every other field's "cold" default.
//
// SIGN CONVENTIONS: lambda_i >= 0 (cI(x) <= 0's own convention), and z
// follows the stationarity identity
//     grad f + Ae^T lambda_e + Ai^T lambda_i - z = 0,
// z >= 0 at an active lower bound, z <= 0 at an active upper bound, 0 when
// free -- see sqp_types.h's SqpSolution note for how the DRIVER's own z
// differs from the QP engine's (model-implied, never TR-zeroed).
//
// valid == false (the default-constructed state) means COLD: nothing in the
// object should be trusted or fed back. Every SqpDriver::solve() call sets
// valid = true on EVERY exit path, INCLUDING A FAILED ONE -- a failed
// solve's last-known point, multipliers and activity are still safe evidence
// for a caller retrying nearby: a WarmStart must never carry a value a later
// solve cannot safely feed back, even when the solve that produced it did
// not converge.
//
// EXACTLY ONE EXIT IS EXCLUDED: a solve that could not EVALUATE its own
// start point (`kNumericalError` before any subproblem was built) returns
// `valid = false`. Because the 3-arg `solve()` overload takes its `x` FROM a
// warm object once the object resolves kWarm, a hand-off from that exit
// would pin the next solve back onto the unevaluable point and silently
// DISCARD the corrected `x0` a caller retried with. A cold object is the
// only honest answer. The solve's own `SqpSolution::x`/`lambda_*` still
// report where it stood, and this object's fields are still populated for
// inspection -- what `valid` withholds is permission to FEED IT BACK.
//
// CONSEQUENCE: `predictor.h`'s predict() and `mesh_transfer.h`'s transfer()
// both throw std::invalid_argument on `valid == false` -- a cold WarmStart
// carries nothing that may be trusted or fed forward, and is not a
// prediction base.
//
// THE ONE INGEST-SIDE QUALIFICATION ON `lambda_i`: the driver clears any
// `lambda_i(j)` whose row is NOT GEOMETRICALLY ACTIVE at the ingested `x`
// (`cI_j(x) < -feas_tol`, the same distance test the reduced stationarity
// measure applies to bounds) before this solve's first convergence test
// reads it. `x`, `lambda_e`, `z`, the activity vectors, the working set and
// every globalization/regularization field are unaffected. A price attached
// to a row that has gone SLACK is contradicted by the new problem's own
// geometry, and carrying it verbatim let a warm solve certify a non-KKT
// point in zero majors; the clear makes the ingested triple satisfy
// complementarity by construction (sqp_driver.h's THE INGESTED MULTIPLIERS
// ARE MADE COMPLEMENTARY note carries the full argument).
//
// WHO NOTICES. A caller that hands back an object a SOLVE produced
// generally does not: an active-set QP prices an inactive row at exactly
// zero already, so the clear only bites when the ROW ITSELF moved. A caller
// that ASSEMBLES an object -- `from_interior_point` below, or a hand-built
// WarmStart -- does: interior-point duals are strictly positive on every
// row, so every row the destination model reports strictly slack will have
// its price dropped, which on the crossover path is exactly the stale price
// the clear exists to remove.
//
// THE SIGN PRECONDITION IS LOAD-BEARING AT INGEST. Read the SIGN CONVENTIONS
// above as a PRECONDITION on anything fed to `SqpDriver::solve`'s 3-arg
// overload: the driver gates stationarity and feasibility and gets
// complementarity by construction after the clear, but DUAL FEASIBILITY IS
// GATED NOWHERE at kWarm/kHot -- `evaluate_kkt` folds a sign-consistency
// residual in for BOUNDS only. A hand-assembled object carrying a NEGATIVE
// `lambda_i(j)` is out of contract and undefended there.
//
// AT StartLevel::kSeeded IT IS GATED (sqp_driver.h's THE SEEDED DUAL CLAMP):
// a negative price within `kSeededDualClampTol` of zero is CLAMPED to zero
// (counted in `SqpCounters::seeded_clamped`); a larger one DEGRADES THE WHOLE
// OBJECT to kCold. The clamp is deliberately NOT extended to kWarm/kHot --
// those levels are hash-gated, and every producer that can clear a hash gate
// is non-negative or negative only within `kDualSignTol * max(1, |lambda|)`
// = 1e-9 relative (predictor.h). Producer inventory: mesh_transfer.h's
// output resolves kSeeded but never above it (no hash); `predictor.h`'s
// ratio test admits a negative `lambda_i` only within that 1e-9-relative
// band and clamps its emitted prices to be non-negative outright;
// `from_interior_point` COPIES the caller's `lambda_i` verbatim (its
// `dual_tol` sign filter governs only working-set membership), so it is a
// shipped route to a small wrong-sign ingest -- defended today by the
// seeded clamp, which zeroes such values and counts them; and every
// `SqpDriver` exit is non-negative, though not uniformly QP-priced -- a
// ZERO-MAJOR exit re-emits the ingested duals as cleared by the driver's own
// ingest block, and a restoration exit carries the sub-solve's subgradient
// selectors.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <hven/core/start_level.h>
#include <hven/detail/qp/working_set.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

// Opaque hot-start handle; fully defined in qp_engine.h, right next to
// QpEngine's own border-mode reuse state. Forward-declared here only so
// WarmStart can hold a std::shared_ptr<const HotState> without this header
// depending on qp_engine.h (or on Eigen's sparse types / MKL Pardiso, which
// qp_engine.h ultimately pulls in) -- a shared_ptr to an incomplete type is
// fine to store, copy and destroy; only CONSTRUCTING or DEREFERENCING one
// needs the full definition, and nothing in this header does either.
struct HotState;

// One solve's exit state, in the shape a later solve's warm-start machinery
// consumes. See this header's own note above for the sign conventions and
// the valid/cold contract.
struct WarmStart {
    // Primal point, eq/ineq duals, bound duals. NOTE the one ingest-side
    // qualification on `lambda_i` -- a price on a row that is strictly slack
    // at the ingested `x` is CLEARED rather than used (this header's ingest
    // note; sqp_driver.h's THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY).
    // Emission is unaffected: what a solve writes here is still exactly the
    // duals it exited with.
    Vec x, lambda_e, lambda_i, z;

    // Per-inequality exit activity: ineq_active[j] == 1 iff row j was in the
    // best-known QP solution's working set (QpSolution::ineq_active[j]), 0
    // otherwise. Size mi(); ALL-ZERO (no activity attributed) when no
    // subproblem's activity could be attributed to the returned point --
    // make_warm_start unconditionally assigns a sized, zero-filled vector
    // even then, so this is never .empty() on a solve-emitted object; a
    // caller must read all-zero, not size 0, as "no activity".
    std::vector<std::uint8_t> ineq_active;

    // Per-variable exit bound activity: -1 at an active LOWER bound, 0 FREE,
    // +1 at an active UPPER bound. A kFixed variable (qp_types.h's
    // BoundState, lower(i) == upper(i)) is reported as +1: it sits at both
    // bounds at once and is not sign-constrained, so either label is equally
    // valid and +1 is the arbitrary-but-consistent choice made here.
    // Size n(); ALL-ZERO under the same condition as ineq_active above, and
    // for the same reason never .empty() on a solve-emitted object.
    std::vector<std::int8_t> bound_active;

    // The QP engine's own working set at exit (working_set.h), carrying
    // exactly the same information as ineq_active/bound_active above in the
    // form the hot-start seeding actually consumes
    // (WorkingSet::bound_state()/active_ineq()). Default-initialized to
    // WorkingSet(0, 0) purely so WarmStart itself stays default-constructible
    // (WorkingSet has no default constructor); a populated object always
    // resizes it to the model's own (n, mi) first.
    WorkingSet qp_working_set = WorkingSet(0, 0);

    // globalization.h's FunnelStrategy::width() at exit. -1 = unset: either
    // the funnel was never reset (an exit before the first measurable
    // iterate) or the driver's strategy is a caller-supplied one that is not
    // a FunnelStrategy and so has no width this header can read. Note this
    // is NOT the same condition structure_hash's 0 marks -- a solve that
    // builds no subproblem still emits a real hash, while its funnel width
    // may genuinely never have existed.
    double funnel_width = -1.0;
    // The trust-region radius the solve exits at (sqp_driver.h's `delta`).
    // Unlike funnel_width this is always known once a solve has started, so
    // -1 here means only "never populated" (a default-constructed object).
    double tr_radius = -1.0;

    // The EFFECTIVE primal_delta/dual_mu the last subproblem was solved with
    // (qp_types.h's SolveOverrides resolution, not its sentinel) -- -1 is
    // this struct's own "never populated" default and is otherwise never a
    // valid regularization value (both are physically positive constants).
    double primal_delta = -1.0, dual_mu = -1.0;

    // FNV-1a fingerprint over H/Ae/Ai's SPARSITY PATTERNS ONLY, never their
    // values -- qp_engine.h's detail::structural_hash, reused as-is. Two
    // solves of the SAME model produce the same hash; two structurally
    // different models are not expected to collide. This is the warm-vs-hot
    // discriminator: a cached factorization is only trusted when a new
    // solve's structure_hash matches the one it was built from.
    //
    // 0 = NO MODEL WAS SEEN. sqp_driver.h's ingest treats it exactly like a
    // mismatch, never like a match -- so an object carrying it can never
    // reach kWarm or kHot; since StartLevel::kSeeded exists it may still
    // have its duals and activity hint ingested when dimensionally
    // consistent and finite (see StartLevel's note for what kSeeded takes
    // and refuses). Who can carry 0:
    //
    //   - NO `valid` SqpDriver::solve() EXIT except one: a solve that
    //     converges at its start point or runs out of budget at zero majors
    //     still has THE MODEL in hand, and make_warm_start probes the model
    //     at the exit point and hashes that pattern. The one exit that
    //     writes 0 is the unevaluable start point, which also writes
    //     `valid = false` -- there the object is cold outright, so the hash
    //     is never consulted.
    //   - mesh_transfer.h's MeshTransfer AND from_interior_point emit 0,
    //     unconditionally and BY DESIGN: a transferred object's hash is
    //     genuinely UNKNOWN -- the destination model is a DIFFERENT model,
    //     and a crossover was never produced by a model this library can
    //     hash at all. Neither may claim a hash it cannot justify, and
    //     neither has to: the seeded level that consumes their output does
    //     not ask for one.
    std::uint64_t structure_hash = 0;

    // The hot-start handle -- see this header's SHAPE note for what it is
    // and is not, and qp_engine.h's HotState/BorderState for the full
    // ownership argument. nullptr means exactly what it does everywhere else
    // in this project: no cached factorization is available to offer, which
    // is always safe to feed forward (sqp_driver.h's level resolution
    // degrades to kWarm silently). Populated on every SqpDriver::solve()
    // exit from QpEngine::hot_state() -- non-null whenever that engine's own
    // last solve() call ended kOptimal under border-mode reuse, regardless
    // of this WarmStart's own `valid`/overall-status story (the same
    // "last-known-good evidence survives a failed solve" contract `valid`
    // documents above applies here too).
    std::shared_ptr<const HotState> hot;

    // THE PROXIMAL SEQUENCE, CARRIED ACROSS SOLVES.
    //
    // The semismooth-Newton kernel (ssn_engine.h) escalates a PROXIMAL term
    // when a solve gets into trouble -- a wrong inertia verdict, an
    // exhausted line search, or crossing `SsnOptions::soft_budget`. The
    // escalation is a LADDER (detail::kSsnProxInit -> * kSsnProxGrowth ->
    // ... -> kSsnProxMax), and every rung it climbs costs one factorization
    // that produced no step. A CONTINUATION caller re-solves a nearby
    // problem over and over; without a carry, every one of those solves
    // re-climbs the same ladder from scratch. These fields are the carry.
    //
    // WHAT IS ACTUALLY READ TODAY IS `prox_sigma`, AND ONLY IT: the shipped
    // proximal term anchors at the CURRENT ITERATE rather than at a lagging
    // centre (ssn_engine.h's SsnStart::prox_center_x), which needs only the
    // LEVEL, consumed directly by `SsnOptions::prox_sigma_init`. The two
    // CENTRE vectors are carried anyway so that a future revival of a
    // lagging centre finds the hand-off object already the right shape;
    // they are DELIBERATELY NOT populated on a solve whose ladder never
    // armed, so the shipped default configuration never pays for them.
    //
    // THE GATE IS THE SIGMA, NOT THE CENTRE: `has_prox_center` CAN be true
    // with both centre vectors empty -- the driver never populates either,
    // because the sole ingest consumer reads `prox_sigma` alone. A future
    // reader reviving a lagging centre must NOT assume `has_prox_center`
    // implies a populated centre.
    //
    // HASH-GATED ON INGEST, like every other state field here:
    // sqp_driver.h reads this block only at StartLevel::kWarm or above --
    // the hash-confirmed-provenance level, the same gate `funnel_width`,
    // `tr_radius`, `primal_delta` and `dual_mu` sit behind. A `kSeeded`
    // object may not claim a proximal history: its provenance is
    // unconfirmed, and a sigma from a DIFFERENT model would damp the first
    // subproblem of this one for no reason.
    //
    // ON EMISSION, `prox_sigma` is the LARGEST sigma any SSN subproblem of
    // the emitting solve finished at -- "the proximal level this solve found
    // it needed" -- not the last one's, which would report 0 whenever the
    // final major happened to be benign. ON INGEST it seeds the FIRST SSN
    // subproblem of the receiving solve only; carrying it into every major
    // would damp subproblems no measurement says need damping ("the
    // proximal term is a REPAIR, not a policy" -- SsnOptions::
    // prox_sigma_init's ruling). 0.0 is the honest "the ladder never armed"
    // default.
    Vec prox_center_x;      // n, or empty
    Vec prox_center_lambda; // me + mi, or empty
    double prox_sigma = 0.0;
    // Gates all three fields above, but a true value does NOT imply the two
    // centre vectors are populated (see THE GATE IS THE SIGMA, NOT THE
    // CENTRE, above) -- only that `prox_sigma` is meaningful. False on every
    // object emitted under the shipped default.
    bool has_prox_center = false;

    // false (default construction) == cold: see this header's own note.
    // Every exit of SqpDriver::solve() sets this true EXCEPT ONE -- a solve
    // that could not EVALUATE its own start point reports false, because
    // feeding that point back would override the corrected x0 a caller
    // retries with (EXACTLY ONE EXIT IS EXCLUDED, above).
    bool valid = false;
};

// THE INTERIOR-POINT CROSSOVER -- `from_interior_point`.
//
// Builds a WarmStart from an INTERIOR-POINT-STYLE primal-dual iterate: the
// Knitro crossover pattern (an IP method runs to near-KKT, then hands off to
// an active-set method for the final polish). Unlike mesh_transfer.h's
// MeshTransfer (which maps a WarmStart born from THIS driver's own solve onto
// a different mesh), this function's INPUT never went through this driver at
// all: there is no QpEngine working set, no funnel/trust-region history and,
// critically, no MODEL this function could hash -- so `structure_hash` is
// unconditionally 0 and `hot` is unconditionally null.
//
// SIGN CONVENTION -- READ BEFORE WIRING A CALLER. This project's own
// convention (this header's SIGN CONVENTIONS note above) is cI(x) <= 0,
// lambda_i >= 0 at an active row, and a SINGLE signed bound dual `z` (>= 0 at
// an active LOWER bound, <= 0 at an active UPPER bound). Interior-point
// codes conventionally report bound multipliers SEPARATELY, both
// non-negative (z_lower >= 0 for the constraint lower <= x, z_upper >= 0 for
// x <= upper), and a general-inequality SLACK s_i >= 0 with cI_i(x) + s_i = 0
// -- i.e. s_i = -cI_i(x). THIS FUNCTION TAKES `slack_i` AS THE cI(x) VALUES
// THEMSELVES (this project's own convention, matching every other slot in
// this header), NOT the IP solver's own non-negative s_i: a caller wiring an
// IPM- or Knitro-style s_i through must negate it first
// (slack_i = -s_i). Getting this backwards would flip every activity
// verdict below without producing any error. The two bound duals are
// converted to this project's single `z` internally as `z = z_lower -
// z_upper` -- a caller never has to perform that conversion itself.
//
// ACTIVITY INFERENCE (the Knitro-crossover pattern). An inequality row is
// judged ACTIVE iff BOTH
//   lambda_i(j) > dual_tol                       -- the multiplier is
//                                                   genuinely positive
//   lambda_i(j) >= kIpActivityFactor
//                  * max(mu_hat, ||lambda_i||inf * activity_rel_tol)
//                                                -- and the price is LIVE at
//                                                   the hand-off's OWN dual
//                                                   scale
// where `mu_hat` is the hand-off's own barrier level, estimated from the data
// it carries as the mean of lambda_i(j)*|slack_i(j)| over the PRICED rows
// (lambda_i(j) > 0), and `||lambda_i||inf` is the largest price among those
// same rows. A bound is judged active at its lower/upper side by the exact
// same test with lambda_i replaced by z_lower/z_upper and the row's residual
// replaced by the GAP (x(i) - lower(i), or upper(i) - x(i)); each side
// estimates its own `mu_hat` and its own dual norm.
//
// WHY A DUAL-SIDE TEST: the slack-side rule it replaced ("residual small
// relative to its own multiplier", with an absolute max(1, .) floor) degrades
// to "empty" on mesh-refinement sequences, where collocation multipliers are
// O(h) while an IP method's residual mu/lambda GROWS as prices shrink --
// measured in docs/notes/2026-08-06-activity-tol-repair.md. The dual-side
// rule recovers the same fixtures' active sets across all mesh sizes. The
// slack is not ignored -- it is read AGGREGATELY as `mu_hat`, which also
// closes the extreme-dual trap: a hand-off pricing a row far above its own
// residual raises the threshold against itself instead of certifying the row
// ACTIVE.
//
// AMBIGUOUS rows -- a price at or below the noise floor, the degenerate case
// an IP method's numerical noise produces close to convergence -- are left
// FREE rather than guessed either way: the destination QP re-derives the
// correct working set from a real linearization, while a
// wrongly-forced-active row can only cost that QP work backing off a face
// the solution is not on. The `lambda_i(j) > dual_tol` conjunct also leaves
// a WRONG-SIGN dual free rather than certified active.
//
// THREE DELIBERATE LIMITS, stated so a reader does not have to find them by
// measurement:
//   * `dual_tol` (1e-6) is absolute. It answers "is this multiplier zero?"
//     against the driver's stationarity noise floor; a relative version
//     would certify degenerate tiny-price/tiny-slack rows. Past the point
//     where the rule's own relative term falls below it, `dual_tol` is the
//     sole false-active guard next to a quantity that scales -- so a caller
//     crossing over at a fine mesh should hand over at a barrier level mu no
//     looser than dual_tol * min|cI| over the strictly slack rows, or treat
//     the hint as slightly wrong (measured false-active fractions stay near
//     1 % even then).
//   * A FINE ENOUGH MESH MAKES THE WEAKLY-ACTIVE AND NOISE POPULATIONS
//     OVERLAP, and no dual-side rule survives that; the rule takes the
//     conservative side (missed rows, never invented ones).
//   * A LOOSE hand-off (mu large against its own dual scale) is a regime
//     where identification is not available from the hand-off at all, and
//     can be actively wrong rather than merely empty.
//
// A kFixed variable (lower(i) == upper(i)) is reported +1, the same
// arbitrary-but-consistent choice WarmStart::bound_active's own note makes
// for a solve-derived object.
//
// NO FUNNEL/TRUST-REGION/REGULARIZATION STATE: funnel_width, tr_radius,
// primal_delta and dual_mu all stay at their "never populated" -1 sentinels
// -- an IP method's own trajectory carries none of these in a form this
// project's globalization understands, so the destination solve uses
// SqpOptions' plain defaults for all four rather than being handed a
// borrowed value with no justified scale.
//
// THE CROSSOVER'S VALUE: `structure_hash == 0` means the object resolves
// StartLevel::kSeeded on an ordinary 3-arg solve() -- never kWarm or kHot,
// and never authorized to reuse a factorization of a matrix nobody here has
// seen (that was always the point of the sentinel). What kSeeded changes
// concretely: `lambda_e`, `lambda_i` and the activity hint REACH the solve
// (the 3-arg call is no longer equivalent to the 2-arg one);
// `crossover.x` is taken FROM THE OBJECT, not from the caller's `x0`
// argument; the ingest-time clear applies, on this path a genuine
// improvement (interior-point duals are strictly positive on every row); and
// the seeded `lambda_i >= 0` clamp applies, the one defence this producer
// specifically needs (see this header's producer-inventory note).
//
// The activity rule's safety factor `nu`, NOT an option: it is one decade of
// margin over the larger of the two scales the rule compares against (the
// same "an order above the producer's own tolerance" step sqp_driver.h's
// kSeededDualClampTol derivation takes), fixed here rather than exposed
// because a caller has no data with which to choose it that the rule has not
// already read for itself. Its product with the default `activity_rel_tol`
// is 1e-3.
inline constexpr double kIpActivityFactor = 10.0;

struct IpCrossoverOptions {
    // eps_rel: how far below the LARGEST price a live price may sit, as a
    // fraction. 1e-4 against kIpActivityFactor = 10 puts the floor three
    // decades below ||lambda_i||inf -- measured on F7 at N = 100 as the
    // widest separation available between genuinely-active and
    // barrier-noise prices on that family.
    double activity_rel_tol = 1e-4;
    // Absolute "is this multiplier zero?" floor -- see this header's note
    // above on why this one stays absolute.
    double dual_tol = 1e-6;
};

namespace detail {

// The activity threshold of the rule documented above, for ONE dual/residual
// population (the inequality rows, or one side of the variable bounds):
//
//     kIpActivityFactor * max(mu_hat, ||dual||inf * eps_rel)
//
// with `mu_hat` the mean of dual(j)*|residual(j)| over the PRICED entries
// (dual(j) > 0) and `||dual||inf` the largest price among them. An empty
// population yields 0, which leaves the caller's `dual_tol` conjunct as the
// only gate.
//
// PRICED ENTRIES ONLY, on both statistics: a zero or negative price is the
// absence of a barrier-level statement, and averaging it in would drag
// `mu_hat` toward zero in proportion to how many rows were left unpriced,
// making the threshold depend on model SIZE rather than hand-off quality.
// The `> 0.0` form also skips a NaN PRICE. A NaN RESIDUAL against a real
// price does reach the mean and makes the threshold NaN, whereupon every
// `>=` below is false and the hint comes out EMPTY -- the safe direction.
//
// THE RESIDUAL IS A TEMPLATE PARAMETER so a caller can pass an Eigen
// EXPRESSION (`x - lower`) rather than a materialized vector: this function
// is reachable at n = 1e6 through crossover cells of scale corpora, and two
// n-sized temporaries per call is a cost with nothing to show for it.
template <typename Residual>
inline double ip_activity_threshold(const Vec &dual, const Residual &residual, double eps_rel) {
    double complementarity = 0.0;
    double dual_inf = 0.0;
    Index priced = 0;
    for (Index j = 0; j < dual.size(); ++j) {
        if (!(dual(j) > 0.0)) {
            continue;
        }
        complementarity += dual(j) * std::abs(residual(j));
        dual_inf = std::max(dual_inf, dual(j));
        ++priced;
    }
    if (priced == 0) {
        return 0.0;
    }
    const double mu_hat = complementarity / static_cast<double>(priced);
    return kIpActivityFactor * std::max(mu_hat, dual_inf * eps_rel);
}

} // namespace detail

// Builds a WarmStart from an interior-point-style primal-dual point. See
// this header's own note immediately above for the sign convention `slack_i`
// must already be in (cI(x) VALUES, NOT an IP solver's own non-negative
// slack), the activity-inference rule, and what this object's fields do and
// do not carry.
//
/// @brief Dimensions: n = x.size(), me = lambda_e.size(), mi =
/// lambda_i.size(). slack_i must match mi; z_lower, z_upper, lower and upper
/// must each match n.
/// @throws std::invalid_argument On any size mismatch (offending sizes in
/// the message).
///
/// The body lives in src/warmstart/warm_start.cpp (runs at most once per
/// solve; never inlined at its call sites). detail::ip_activity_threshold
/// above STAYS HERE: it is a template on purpose, so this function can pass
/// it an Eigen expression rather than materialize two n-sized temporaries.
WarmStart from_interior_point(const Vec &x, const Vec &lambda_e, const Vec &lambda_i,
                              const Vec &slack_i, const Vec &z_lower, const Vec &z_upper,
                              const Vec &lower, const Vec &upper,
                              const IpCrossoverOptions &opts = {});

} // namespace hven::solvers
