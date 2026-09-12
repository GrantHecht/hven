// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// sqp_warm_start.h -- SqpWarmStart, the SQP engine's OWN native warm-start
// value object, and the public home of what was
// `detail/warmstart/warm_start.h::SqpWarmStart` up to M6 W5 T8.5.
//
// TWO IDENTITIES, ONE PROTOCOL (design 2.4). The shared, cross-engine
// currency is `warmstart/warm_start_data.h::WarmStartData` -- a declared-space
// PAYLOAD both engines accept through
// `solve(model, x0, const WarmStartData &, SolveBudget)`, whose identity stamp
// REFUSES a foreign problem in every mode. This header carries the OTHER
// identity: the SQP's own native object, which is not a payload, carries no
// declaration stamp, is never serialized, and reaches the engine only through
// the LABELLED SQP-ONLY entry
// `SqpResult SqpSolver::solve(NlpModelAssembly &, const Vec &x0,
//                             const SqpWarmStart &, SolveBudget)`
// (and its `NlpModel &` twin). It is the exact counterpart of the IPM-only
// KKT hook: one engine-only extension per engine, labelled here and in the
// migration guide. Without it kWarm and kHot are unreachable -- a payload
// carries `structure_hash == 0` and so caps at kSeeded, and the hot handle is
// process-local and never travels in a payload.
//
// VALIDATION ON THIS ROUTE IS UNCHANGED BY T8.5 and is DEGRADE-ONLY: nothing
// here is ever "refused for identity", because nothing here claims one.
// `valid == false`, a size disagreement, or a non-finite ingested vector
// resolves kCold; a structure-hash mismatch resolves kSeeded; a match
// resolves kWarm; a match plus a live hot handle resolves kHot subject to the
// backend's own reuse checks.
//
// `SqpWarmStart` remains a spelling of this type -- `detail/warmstart/
// warm_start.h` keeps `using SqpWarmStart = SqpWarmStart;` until T8.10 rewrites
// the call sites -- so every existing producer and consumer is unaffected.
//
// WHAT THE OBJECT IS (carried verbatim from its old home):
//
// SqpWarmStart is the value object EVERY SqpSolver::solve() call
// emits (SqpResult::warm_start, sqp_solver_types.h) describing what a solve
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
// SqpWarmStart stays COPYABLE (a shared_ptr copies cheaply, sharing ownership),
// but it is no longer lifetime-free in the way the rest of this struct is:
// `hot`, when non-null, keeps a sparse KKT factor (and its live backend
// session) alive for as long as any copy of this SqpWarmStart does.
// SAME-PROCESS ONLY: nothing here serializes `hot`, and none of this
// struct's own (de)serialization -- there is none -- is expected to grow
// any. A default-constructed SqpWarmStart (valid == false) leaves `hot` null,
// exactly like every other field's "cold" default.
//
// SIGN CONVENTIONS: lambda_i >= 0 (cI(x) <= 0's own convention), and z
// follows the stationarity identity
//     grad f + Ae^T lambda_e + Ai^T lambda_i - z = 0,
// z >= 0 at an active lower bound, z <= 0 at an active upper bound, 0 when
// free -- see sqp_solver_types.h's SqpResult note for how the DRIVER's own z
// differs from the QP engine's (model-implied, never TR-zeroed).
//
// valid == false (the default-constructed state) means COLD: nothing in the
// object should be trusted or fed back. Every SqpSolver::solve() call sets
// valid = true on EVERY exit path, INCLUDING A FAILED ONE -- a failed
// solve's last-known point, multipliers and activity are still safe evidence
// for a caller retrying nearby: a SqpWarmStart must never carry a value a later
// solve cannot safely feed back, even when the solve that produced it did
// not converge.
//
// EXACTLY ONE EXIT IS EXCLUDED: a solve that could not EVALUATE its own
// start point (`kNumericalError` before any subproblem was built) returns
// `valid = false`. Because the 3-arg `solve()` overload takes its `x` FROM a
// warm object once the object resolves kWarm, a hand-off from that exit
// would pin the next solve back onto the unevaluable point and silently
// DISCARD the corrected `x0` a caller retried with. A cold object is the
// only honest answer. The solve's own `SqpResult::x`/`lambda_*` still
// report where it stood, and this object's fields are still populated for
// inspection -- what `valid` withholds is permission to FEED IT BACK.
//
// CONSEQUENCE: `predictor.h`'s predict() and `mesh_transfer.h`'s transfer()
// both throw std::invalid_argument on `valid == false` -- a cold SqpWarmStart
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
// complementarity by construction (sqp_solver.h's THE INGESTED MULTIPLIERS
// ARE MADE COMPLEMENTARY note carries the full argument).
//
// WHO NOTICES. A caller that hands back an object a SOLVE produced
// generally does not: an active-set QP prices an inactive row at exactly
// zero already, so the clear only bites when the ROW ITSELF moved. A caller
// that ASSEMBLES an object -- `from_interior_point` below, or a hand-built
// SqpWarmStart -- does: interior-point duals are strictly positive on every
// row, so every row the destination model reports strictly slack will have
// its price dropped, which on the crossover path is exactly the stale price
// the clear exists to remove.
//
// THE SIGN PRECONDITION IS LOAD-BEARING AT INGEST. Read the SIGN CONVENTIONS
// above as a PRECONDITION on anything fed to `SqpSolver::solve`'s 3-arg
// overload: the driver gates stationarity and feasibility and gets
// complementarity by construction after the clear, but DUAL FEASIBILITY IS
// GATED NOWHERE at kWarm/kHot -- `evaluate_kkt` folds a sign-consistency
// residual in for BOUNDS only. A hand-assembled object carrying a NEGATIVE
// `lambda_i(j)` is out of contract and undefended there.
//
// AT StartLevel::kSeeded IT IS GATED (sqp_solver.h's THE SEEDED DUAL CLAMP):
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
// `SqpSolver` exit is non-negative, though not uniformly QP-priced -- a
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
// SqpWarmStart can hold a std::shared_ptr<const HotState> without this header
// depending on qp_engine.h (or on Eigen's sparse types / MKL Pardiso, which
// qp_engine.h ultimately pulls in) -- a shared_ptr to an incomplete type is
// fine to store, copy and destroy; only CONSTRUCTING or DEREFERENCING one
// needs the full definition, and nothing in this header does either.
struct HotState;

// One solve's exit state, in the shape a later solve's warm-start machinery
// consumes. See this header's own note above for the sign conventions and
// the valid/cold contract.
struct SqpWarmStart {
    // Primal point, eq/ineq duals, bound duals. NOTE the one ingest-side
    // qualification on `lambda_i` -- a price on a row that is strictly slack
    // at the ingested `x` is CLEARED rather than used (this header's ingest
    // note; sqp_solver.h's THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY).
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
    // WorkingSet(0, 0) purely so SqpWarmStart itself stays default-constructible
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
    // The trust-region radius the solve exits at (sqp_solver.h's `delta`).
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
    // 0 = NO MODEL WAS SEEN. sqp_solver.h's ingest treats it exactly like a
    // mismatch, never like a match -- so an object carrying it can never
    // reach kWarm or kHot; since StartLevel::kSeeded exists it may still
    // have its duals and activity hint ingested when dimensionally
    // consistent and finite (see StartLevel's note for what kSeeded takes
    // and refuses). Who can carry 0:
    //
    //   - NO `valid` SqpSolver::solve() EXIT except one: a solve that
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
    // is always safe to feed forward (sqp_solver.h's level resolution
    // degrades to kWarm silently). Populated on every SqpSolver::solve()
    // exit from QpEngine::hot_state() -- non-null whenever that engine's own
    // last solve() call ended kOptimal under border-mode reuse, regardless
    // of this SqpWarmStart's own `valid`/overall-status story (the same
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
    // sqp_solver.h reads this block only at StartLevel::kWarm or above --
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
    // Every exit of SqpSolver::solve() sets this true EXCEPT ONE -- a solve
    // that could not EVALUATE its own start point reports false, because
    // feeding that point back would override the corrected x0 a caller
    // retries with (EXACTLY ONE EXIT IS EXCLUDED, above).
    bool valid = false;
};

} // namespace hven::solvers
