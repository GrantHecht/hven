#pragma once

// warm_start.h — WarmStart, the value object EVERY SqpDriver::solve() call
// emits (SqpSolution::warm_start, sqp_types.h) describing what a solve
// learned that a SUBSEQUENT solve of a nearby problem could reuse: the
// primal/dual point, which inequalities and bounds were active there, the
// QP engine's own working set at exit, and the globalization/regularization
// state (funnel width, trust-region radius, effective primal_delta/dual_mu).
//
// PHASE-4 TASK 2 SCOPE: the value object and its population, nothing more.
// NO FACTORIZATION HANDLE LIVED HERE -- Task 4 adds the hot-start machinery
// that consumes `structure_hash` to decide whether a cached K0 factorization
// is still valid; through Task 2 this struct was deliberately copyable
// POD-ish (Vec/vector/WorkingSet members, no pointers, no owned resources)
// so it could be stored, compared and fed back into a later solve with no
// lifetime contract.
//
// PHASE-4 TASK 4 ADDS `hot` (below), the ONE exception to that POD-ish
// shape: a std::shared_ptr<const HotState>, opaque to this header on
// purpose -- HotState is only FORWARD-declared here and fully defined in
// qp_engine.h, next to the engine-instance reuse machinery
// (QpEngine::border_/border_valid_ and friends) it is a frozen snapshot of.
// WarmStart stays COPYABLE (a shared_ptr copies cheaply, sharing ownership
// rather than duplicating anything Pardiso-owned -- see HotState's own
// OWNERSHIP note in qp_engine.h for the full argument and the one caller
// invariant it asks for), but it is no longer lifetime-free in the way the
// rest of this struct is: `hot`, when non-null, keeps a KktSystem (and its
// live Pardiso handle) alive for as long as any copy of this WarmStart
// does. SAME-PROCESS ONLY: nothing here serializes `hot` (a shared_ptr and
// a Pardiso handle have no on-disk representation), and none of this
// struct's own (de)serialization -- there is none -- is expected to grow
// any. A default-constructed WarmStart (valid == false) leaves `hot` null,
// exactly like every other field's "cold" default.
//
// SIGN CONVENTIONS, unchanged from qp_problem.h/sqp_types.h: lambda_i >= 0
// (cI(x) <= 0's own convention), and z follows the stationarity identity
//     grad f + Ae^T lambda_e + Ai^T lambda_i - z = 0,
// z >= 0 at an active lower bound, z <= 0 at an active upper bound, 0 when
// free -- see sqp_types.h's SqpSolution note for how the DRIVER's own z
// differs from the QP engine's (model-implied, never TR-zeroed).
//
// valid == false (the default-constructed state) means COLD: nothing in the
// object should be trusted or fed back, which is exactly what a caller gets
// by default-constructing one itself rather than receiving it from a solve.
// Every SqpDriver::solve() call sets valid = true on EVERY exit path,
// INCLUDING A FAILED ONE (sqp_driver.h audits every return site) -- a failed
// solve's last-known point, multipliers and activity are still safe evidence
// for a caller retrying nearby, which is the whole contract Task 3 depends
// on: a `WarmStart` this task produces must never carry a value a later
// solve cannot safely feed back, even when the solve that produced it did
// not converge.
//
// EXACTLY ONE EXIT IS EXCLUDED, and stating it sharpens the rest rather than
// weakening it (Phase-5 Task 0, fix round 1). A solve that could not EVALUATE
// its own start point -- `kNumericalError` before any subproblem was built --
// returns `valid = false`. The clause above rests on the point being one the
// MODEL CAN EVALUATE; there it is not, and there alone. This is not
// squeamishness about a failed solve: because the 3-arg `solve()` overload
// takes its `x` FROM a warm object once the object resolves kWarm, a
// hand-off from that exit would pin the next solve back onto the unevaluable
// point and silently DISCARD the corrected `x0` a caller retried with. A cold
// object is the only honest answer, and it is what a retry needs. The
// solve's own `SqpSolution::x`/`lambda_*` still report where it stood, and
// this object's fields are still populated for inspection -- what `valid`
// withholds is permission to FEED IT BACK.
//
// THE SECOND CONSEQUENCE, for a caller assembling its own sweep: the two
// functions that REFUSE a cold object outright now refuse that exit's
// hand-off too. `predictor.h`'s predict() and `mesh_transfer.h`'s transfer()
// both throw std::invalid_argument on `valid == false` (their own T6
// validation, unchanged and pre-dating this exception -- "a cold WarmStart
// carries nothing that may be trusted or fed forward"), where before they
// would have accepted the object and worked from a point the model cannot
// evaluate. That is the correct reading -- a cold object is not a prediction
// base -- and `continuation.h`'s own sweep never meets it, because it
// advances `warm_cur` only on a kOptimal solve and this exit reports
// kNumericalError.
//
// PHASE-5 TASK 7b NARROWS ONE INGEST GUARANTEE, and it is the only place this
// object's duals are not taken verbatim. **`lambda_i` IS NOT INGESTED
// UNCONDITIONALLY ANY MORE**: sqp_driver.h clears any `lambda_i(j)` whose row
// is NOT GEOMETRICALLY ACTIVE at the ingested `x` -- `cI_j(x) < -feas_tol`,
// the same distance test the reduced stationarity measure already applies to
// bounds -- before this solve's first convergence test reads it. `x`,
// `lambda_e`, `z`, the activity vectors, the working set and every
// globalization/regularization field are unaffected.
//
// WHY THE CONTRACT HAD TO CHANGE (finding B-1,
// docs/notes/2026-07-31-nonconvex-sweep-adjudications.md §1): this object is
// defined at the top of this note as what a solve learned that a SUBSEQUENT
// SOLVE OF A NEARBY PROBLEM could reuse, and cross-parameter ingest is
// therefore its stated purpose rather than an abuse of it. But a price
// attached to a row that has since gone SLACK is not "learned" -- it is
// contradicted by the new problem's own geometry, and carrying it verbatim let
// a warm solve certify a non-KKT point in zero majors. The clear is what makes
// the ingested triple satisfy complementarity by construction; the full
// argument, and why this option was chosen over three others, is in
// sqp_driver.h's THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY note.
//
// WHO NOTICES. A caller that hands back an object a SOLVE produced generally
// does not: an active-set QP prices an inactive row at exactly zero already,
// so the clear only bites when the ROW ITSELF moved. A caller that ASSEMBLES
// an object -- `from_interior_point` below, or a hand-built WarmStart -- does:
// interior-point duals are strictly positive on every row, so every row that
// the destination model reports strictly slack will have its price dropped.
// That is the intended reading in both cases, and on the crossover path it IS
// an improvement -- an interior-point dual on a row that the destination model
// reports strictly slack is exactly the stale price the clear exists to drop.
//
// **PHASE-6 TASK 5 RESOLVED THE QUALIFIER THIS PARAGRAPH USED TO CARRY.** It
// used to end "...WOULD BE an improvement ONCE A NON-HASH-GATED INGEST LEVEL
// EXISTS (Phase 6). TODAY, THOUGH, IT IS NOT REACHED", because
// `from_interior_point` carries `structure_hash == 0` unconditionally and the
// driver's ingest resolved every such object to `kCold` before any dual it
// carried was consulted. THAT LEVEL NOW EXISTS: `StartLevel::kSeeded` (above)
// takes a hash-less object's duals, so a crossover object REACHES THE CLEAR on
// an ordinary `solve()` call and the clause above describes something that
// happens, not something that would. The B-1 clear runs FIRST at that level and
// the `lambda_i >= 0` clamp SECOND -- see sqp_driver.h's THE SEEDED DUAL CLAMP
// for why that order, and not the reverse, is the normative one.
//
// **AND THE `lambda_i >= 0` CONVENTION ABOVE IS NOW LOAD-BEARING AT INGEST,
// not merely descriptive.** Read the SIGN CONVENTIONS paragraph at the top of
// this note as a PRECONDITION on anything fed to `SqpDriver::solve`'s 3-arg
// overload. The driver gates stationarity and feasibility and, after the clear,
// gets complementarity by construction -- but **dual feasibility is gated
// NOWHERE**: `evaluate_kkt` folds a sign-consistency residual in for BOUNDS
// only, and a general inequality row enters the Lagrangian gradient
// unconditionally with no sign test. A hand-assembled object carrying a
// NEGATIVE `lambda_i(j)` is therefore out of contract and, AT kWarm AND kHot,
// is still not defended against, and the clear can make its consequence visible
// where it previously was not: measured, an ingest at `x = 0` carrying
// `lambda_i = (-1, +0.5)` on `min x s.t. x <= 0, -x - 1 <= 0` certifies
// `kOptimal` in zero majors at `f = 0` where the truth is `f = -1`, because the
// cleared `+0.5` had been the only thing breaking stationarity (Task-7b review,
// probe P2). The same false certificate is reachable without the clear from
// `lambda_i = (-1, 0)`, so this is a pre-existing ungated condition rather than
// a new one -- see `sqp_driver.h`'s THE INGESTED MULTIPLIERS ARE MADE
// COMPLEMENTARY for the full itemization.
//
// **AT kSeeded IT IS NOW GATED, and that closes O-B1-4 on the one route that
// can actually produce it** (Phase-6 Task 5). The Phase-6 candidate those notes
// carried -- an ingest-time `lambda_i >= 0` validation -- landed as
// sqp_driver.h's THE SEEDED DUAL CLAMP, scoped to the seeded level: a negative
// price within `kSeededDualClampTol` of zero is CLAMPED to zero (and counted in
// `SqpCounters::seeded_clamped`), a larger one DEGRADES THE WHOLE OBJECT to
// kCold. Run the P2 probe above through the seeded level and it degrades: -1 on
// a geometrically active row is not a seed. The clamp is deliberately NOT
// extended to kWarm/kHot -- those levels are hash-gated, every producer that
// can clear a hash gate is non-negative or bounded by 1e-9 relative
// (predictor.h's kDualSignTol), and widening the change there would move
// pinned trajectories for no reachable defect.
//
// **THE CROSSOVER PATH BELOW IS A LIVE PRODUCER OF EXACTLY THAT INPUT, and an
// earlier version of this paragraph claimed the opposite** (Task-7b review
// round 2, N-1). `from_interior_point` **copies the caller's `lambda_i`
// verbatim**; its `dual_tol` sign filter governs only WORKING-SET MEMBERSHIP --
// a row failing `lambda_i(j) > dual_tol` is left out of
// `ineq_active`/`qp_working_set`, and its PRICE goes into the object unchanged.
// `WarmStart.CrossoverWrongSignDualLeavesRowFree` in tests/test_warm_start.cpp
// builds one deliberately: `lambda_i = -1e-8` against a slack of `-1e-9`, i.e.
// a negative price on a row that is GEOMETRICALLY ACTIVE -- which is the one
// configuration the driver's clear does not touch, since the clear only zeroes
// rows that are STRICTLY SLACK. An interior-point solve handed over before full
// convergence is therefore a shipped route to a sign-violating ingest, and the
// magnitude is bounded by whatever residual sign noise that solver commits
// (1e-8 in the fixture -- orders below `kkt_tol`, which is why the conclusion
// stands and the repair's verdict is unchanged). **THAT ROUTE IS NOW LIVE AND
// NOW DEFENDED** (Phase-6 Task 5): the same object resolves kSeeded rather than
// kCold, so it reaches the ingest -- and the seeded `lambda_i >= 0` clamp zeroes
// that -1e-8 (it is TWO orders inside `kSeededDualClampTol` = 1e-6; an earlier
// version of this line said four, final fix wave W9 -- sqp_driver.h's own
// derivation says two) and counts it.
//
// The other producers are clean, for reasons worth keeping distinct:
// `mesh_transfer.h`'s output resolves kSeeded but never above it (no hash);
// `predictor.h`'s ratio test admits a negative `lambda_i` only within
// `kDualSignTol * max(1, |lambda|)` = 1e-9 relative; and every `SqpDriver` exit
// is non-negative, though not uniformly because it is QP-priced -- an exit that
// solved a subproblem carries QP duals, a ZERO-MAJOR exit re-emits the ingested
// duals **as cleared by the driver's own ingest block**, and a restoration exit
// carries the sub-solve's subgradient selectors.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <tycho_sqp/types.h>
#include <tycho_sqp/working_set.h>

namespace tycho::sqp {

// Opaque Task-4 hot-start handle; fully defined in qp_engine.h, right next
// to QpEngine's own border-mode reuse state. Forward-declared here only so
// WarmStart can hold a std::shared_ptr<const HotState> without this header
// depending on qp_engine.h (or on Eigen's sparse types / MKL Pardiso, which
// qp_engine.h ultimately pulls in) -- a shared_ptr to an incomplete type is
// fine to store, copy and destroy; only CONSTRUCTING or DEREFERENCING one
// needs the full definition, and nothing in this header does either.
struct HotState;

// How much of a previous solve's state a caller intends to feed into the
// next one: kCold ignores it entirely (an ordinary cold solve), kSeeded
// takes the VALUES (x, the duals, the activity hint) without any claim
// about where they came from, kWarm additionally trusts the object's
// provenance -- a structural-hash MATCH against this model -- and with it
// the globalization/regularization state, and kHot additionally offers
// `hot` (above) for the engine to reuse a cached factorization keyed on
// that same `structure_hash`. kHot is Task 4's: sqp_driver.h's level
// resolution now produces and reads it (see solve_impl's WARM-START INGEST
// note there), gated the same way kWarm always was -- a caller does
// nothing different to opt in beyond having received a `hot` handle from a
// prior solve; SqpOptions::start_level still CAPS the result, so a caller
// that never wants kHot pinned can clamp it there.
//
// THE ORDER IS LOAD-BEARING AND THE ENUM IS COMPARED BY IT. sqp_driver.h's
// ceiling test compares `static_cast<int>`, and the suite's own
// `EXPECT_GE(step.level, kWarm)`-style reads (tests/test_continuation.cpp)
// compare the enumerators directly, so kSeeded had to be inserted BETWEEN
// kCold and kWarm rather than appended: it is strictly less than kWarm in
// everything it asks the driver to believe.
//
// PHASE-6 TASK 5 ADDS kSeeded -- "TRUSTS VALUES, NOT PROVENANCE". It is the
// ingest level the two hash-less producers in this project have been waiting
// for since Phase 4: `from_interior_point` below and mesh_transfer.h's
// MeshTransfer both emit `structure_hash == 0` BY CONSTRUCTION (neither has a
// model of the DESTINATION solve to hash), and through Phase 5 that sentinel
// resolved every such object to kCold, so nothing but `x` -- passed explicitly
// as the caller's own x0 -- could reach a solve. The gap was written up twice,
// independently, as this header's THE CROSSOVER'S VALUE TODAY and
// mesh_transfer.h's THE PHASE-6 INGEST GAP; kSeeded is the driver-side change
// both of those notes named ("option (i)", equivalently the battery note's
// repair C) and both are now RESOLVED rather than deferred.
//
// WHAT kSeeded TAKES, exactly, and it is the whole list:
//   - `x`, `lambda_e`, `lambda_i` -- fed into the FIRST convergence test, so a
//     seeded object that genuinely IS a KKT point of the posed problem can be
//     certified in zero majors, under the post-B-1 rules below;
//   - the ACTIVITY HINT (`qp_working_set`) -- seeded into the first QP
//     subproblem's working set as a GUESS, through the same seed path every
//     other major uses, so the engine's own WINDOW-CONSISTENCY RULE may drop
//     any part of it that does not fit the first trust-region window.
//     **AT THIS LEVEL AN EMPTY HINT IS NO HINT** -- an all-free working set
//     means "no activity was attributed" and not "nothing is active"
//     (`ineq_active`/`bound_active`'s own field notes below say so, and the
//     two producers above can both emit exactly that), so a hint-less seeded
//     object leaves the first subproblem unseeded and thereby RE-ARMS
//     SqpOptions::crash_basis if it is on. A hint-CARRYING one suppresses the
//     crash basis exactly as a kWarm object does. That reading is a Phase-6
//     Task 5 ruling; sqp_driver.h's seed-building block carries the full
//     argument and states that kCold/kWarm/kHot behaviour is unchanged by it.
//
// WHAT IT NEVER TAKES, and each exclusion has the same one-line reason -- the
// object cannot justify it without provenance:
//   - THE FACTORIZATION (`hot`). The hash exists precisely to protect
//     factorization reuse; an object that cannot produce a hash may never
//     reach kHot, by construction (kSeeded < kWarm < kHot).
//   - THE FUNNEL WIDTH and THE TRUST-REGION RADIUS. Both are measured in the
//     units of a DIFFERENT problem's own violation measure and step scale
//     (mesh_transfer.h's section 4 makes exactly this argument for
//     funnel_width and declines to transfer it; kSeeded extends the same
//     refusal to tr_radius, which MeshTransfer does carry). The destination
//     solve seeds its funnel from Eq. (9) at its own first measured h0 and
//     starts at SqpOptions::tr_init, exactly as a cold solve does.
//   - THE KUNGURTSEV-DIEHL FULL-STEP WINDOW (SqpOptions::warm_full_step). The
//     full-step-first rule's justification is local convergence of a warm
//     start ON THE SAME PROBLEM; a seeded object makes no such claim.
//
// THE INGEST GATE IS DIMENSIONS AND FINITENESS, NOTHING ELSE. `valid`, then
// `x`/`lambda_e`/`lambda_i`/`qp_working_set` sized against (n, me, mi), then
// every one of those three vectors finite. NO HASH IS REQUIRED and none is
// computed -- a solve whose ceiling is kSeeded pays no resolution probe at
// all. An object failing the gate resolves kCold exactly as before.
//
// AND ONE THING kSeeded ADDS THAT NO OTHER LEVEL HAS: `lambda_i >= 0` IS
// ENFORCED AT INGEST (sqp_driver.h's THE SEEDED DUAL CLAMP). That is not
// decoration -- it is the reason this level is safe to open. Every other
// route into the ingest is hash-gated, and the producers that can clear a
// hash gate are non-negative by construction; kSeeded admits objects a
// FOREIGN solver or a caller's own hand assembled, which is exactly the
// input class this header's SIGN CONVENTIONS paragraph states as a
// PRECONDITION and which sqp_driver.h's THE INGESTED MULTIPLIERS ARE MADE
// COMPLEMENTARY note itemizes as ungated (O-B1-4). At the seeded level it is
// no longer ungated: a small negative price is CLAMPED to zero and counted,
// and a large one degrades the whole object to kCold.
enum class StartLevel { kCold, kSeeded, kWarm, kHot };

// StartLevel -> a short display string. PHASE-4 TASK 7: the cold-vs-warm
// ledger instrumentation and sqp_driver.h's iteration-table printer both need
// to render `SqpCounters::start_level_used` (sqp_types.h) for a human, so the
// switch lives here once rather than being hand-copied at each call site --
// the same rationale sqp_types.h's to_string(SqpStatus) states for itself.
inline const char *to_string(StartLevel level) {
    switch (level) {
    case StartLevel::kCold:
        return "Cold";
    case StartLevel::kSeeded:
        return "Seeded";
    case StartLevel::kWarm:
        return "Warm";
    case StartLevel::kHot:
        return "Hot";
    }
    return "Unknown";
}

// One solve's exit state, in the shape a later solve's warm-start machinery
// consumes. See this header's own note above for the sign conventions and
// the valid/cold contract.
struct WarmStart {
    // Primal point, eq/ineq duals, bound duals. NOTE the one ingest-side
    // qualification on `lambda_i` -- a price on a row that is strictly slack at
    // the ingested `x` is CLEARED rather than used (this header's PHASE-5 TASK
    // 7b paragraph; sqp_driver.h's THE INGESTED MULTIPLIERS ARE MADE
    // COMPLEMENTARY). Emission is unaffected: what a solve writes here is still
    // exactly the duals it exited with.
    Vec x, lambda_e, lambda_i, z;

    // Per-inequality exit activity: ineq_active[j] == 1 iff row j was in the
    // best-known QP solution's working set (QpSolution::ineq_active[j]), 0
    // otherwise. Size mi(); ALL-ZERO (no activity attributed) when no
    // subproblem's activity could be attributed to the returned point
    // (sqp_driver.h's POPULATION note on each exit site covers exactly when
    // that is) -- make_warm_start unconditionally assigns a sized,
    // zero-filled vector even then, so this is never .empty() on a
    // solve-emitted object; a caller must read all-zero, not size 0, as "no
    // activity".
    std::vector<std::uint8_t> ineq_active;

    // Per-variable exit bound activity: -1 at an active LOWER bound, 0 FREE,
    // +1 at an active UPPER bound. A kFixed variable (types.h's BoundState,
    // lower(i) == upper(i)) is reported as +1: it sits at both bounds at
    // once and is not sign-constrained (qp_engine.h's own note on kFixed),
    // so either label is equally valid and +1 is the arbitrary-but-consistent
    // choice made here. Size n(); ALL-ZERO (no activity attributed) under the
    // same condition as ineq_active above, and for the same reason never
    // .empty() on a solve-emitted object.
    std::vector<std::int8_t> bound_active;

    // The QP engine's own working set at exit (working_set.h), carrying
    // exactly the same information as ineq_active/bound_active above in the
    // form Task 4's hot-start seeding actually consumes
    // (WorkingSet::bound_state()/active_ineq()). Default-initialized to
    // WorkingSet(0, 0) purely so WarmStart itself stays default-constructible
    // (WorkingSet has no default constructor); a populated object always
    // resizes it to the model's own (n, mi) first.
    WorkingSet qp_working_set = WorkingSet(0, 0);

    // globalization.h's FunnelStrategy::width() at exit. -1 = unset: either
    // the funnel was never reset (an exit before the first measurable
    // iterate) or the driver's strategy is a caller-supplied one that is not
    // a FunnelStrategy and so has no width this header can read. Note this is
    // NOT the same condition structure_hash's 0 marks -- a solve that builds
    // no subproblem still emits a real hash (see that field), while its
    // funnel width may genuinely never have existed.
    double funnel_width = -1.0;
    // The trust-region radius the solve exits at (sqp_driver.h's `delta`).
    // Unlike funnel_width this is always known once a solve has started, so
    // -1 here means only "never populated" (a default-constructed object).
    double tr_radius = -1.0;

    // The EFFECTIVE primal_delta/dual_mu the last subproblem was solved with
    // (types.h's SolveOverrides resolution, not its sentinel) -- -1 is this
    // struct's own "never populated" default and is otherwise never a valid
    // regularization value (both are physically positive constants).
    double primal_delta = -1.0, dual_mu = -1.0;

    // FNV-1a fingerprint over H/Ae/Ai's SPARSITY PATTERNS ONLY, never their
    // values -- qp_engine.h's detail::structural_hash, reused as-is (it
    // already hashes pattern only) rather than duplicated here. Two solves
    // of the SAME model produce the same hash; two structurally different
    // models (different n/me/mi, or the same shape with a different
    // sparsity pattern) are not expected to collide. This is the warm-vs-hot
    // discriminator Task 4 reads: a cached factorization is only trusted
    // when a new solve's structure_hash matches the one it was built from.
    //
    // 0 = NO MODEL WAS SEEN. sqp_driver.h's ingest treats it exactly like a
    // mismatch, never like a match -- so an object carrying it can never reach
    // kWarm or kHot. IT IS NO LONGER "CONTRIBUTES NOTHING BUT ITS `x`", and
    // that sentence stood here through Phase 5: since Phase-6 Task 5 such an
    // object resolves `StartLevel::kSeeded` when it is dimensionally
    // compatible and finite, so its DUALS AND ACTIVITY HINT are ingested too
    // (see StartLevel's own note for the exact list, and for the three things
    // kSeeded still refuses -- the factorization this field exists to protect
    // among them). PHASE-5 TASK 0 NARROWED WHO CAN CARRY IT, and the
    // distinction is worth stating precisely because the two cases used to be
    // conflated (the battery note's O-1):
    //
    //   - NO `valid` SqpDriver::solve() EXIT PRODUCES 0. Not even one that
    //     built no subproblem at all: a solve that converges at its start
    //     point or runs out of budget at zero majors still has THE MODEL in
    //     hand, and structural_hash reads only that model's H/Ae/Ai sparsity
    //     PATTERN -- so sqp_driver.h's make_warm_start probes the model at
    //     the exit point and hashes that (its own THE ZERO-MAJOR PROBE
    //     note). Through Phase 4 these exits wrote 0 and their hand-offs were
    //     silently unusable. The one driver exit that still writes 0 is the
    //     unevaluable start point, which also writes `valid = false` (see
    //     that field's note) -- there the object is cold outright, so the
    //     hash is never consulted.
    //   - mesh_transfer.h's MeshTransfer AND from_interior_point (below) DO
    //     emit 0, unconditionally and BY DESIGN, and that is not the same
    //     condition repaired above. A transferred object's hash is genuinely
    //     UNKNOWN rather than merely uncomputed -- the destination model is a
    //     DIFFERENT model (a refined mesh, a different transcription), and a
    //     crossover was never produced by a model this library can hash at
    //     all. Neither may claim a hash it cannot justify, so both keep the
    //     sentinel and both document the ingest consequence themselves
    //     (mesh_transfer.h's PHASE-6 INGEST GAP; this header's own note on
    //     the crossover's value today). Closing THAT gap needed an ingest
    //     level that trusts duals without a hash match -- **which PHASE-6
    //     TASK 5 BUILT (`StartLevel::kSeeded`), so the gap is CLOSED and
    //     both those notes are resolved rather than deferred.** The sentinel
    //     itself is unchanged and stays exactly as truthful: neither producer
    //     may claim a hash it cannot justify, and neither now has to, because
    //     the level that consumes their output does not ask for one.
    std::uint64_t structure_hash = 0;

    // PHASE-4 TASK 4: the hot-start handle -- see this header's own note
    // above (PHASE-4 TASK 4 ADDS `hot`) for what it is and is not, and
    // qp_engine.h's HotState/BorderState for the full ownership argument.
    // nullptr means exactly what it does everywhere else in this project:
    // no cached factorization is available to offer, which is always safe
    // to feed forward (sqp_driver.h's level resolution degrades to kWarm
    // silently). Populated on every SqpDriver::solve() exit from
    // QpEngine::hot_state() -- non-null whenever that engine's own last
    // solve() call ended kOptimal under border-mode reuse, regardless of
    // this WarmStart's own `valid`/overall-status story (the same "last-
    // known-good evidence survives a failed solve" contract `valid`
    // documents above applies here too).
    std::shared_ptr<const HotState> hot;

    // =========================================================================
    // PHASE-7 TASK 5: THE PROXIMAL SEQUENCE, CARRIED ACROSS SOLVES
    // =========================================================================
    //
    // The semismooth-Newton kernel (ssn_engine.h) escalates a PROXIMAL term
    // when a solve gets into trouble -- a wrong inertia verdict, an exhausted
    // line search, or crossing `SsnOptions::soft_budget`. The escalation is a
    // LADDER (detail::kSsnProxInit -> * kSsnProxGrowth -> ... -> kSsnProxMax,
    // seven rungs), and every rung it climbs costs one factorization that
    // produced no step. A CONTINUATION caller re-solves a nearby problem over
    // and over; without a carry, every one of those solves re-climbs the same
    // ladder from scratch. These fields are the carry.
    //
    // **WHAT IS ACTUALLY READ TODAY IS `prox_sigma`, AND ONLY IT.** That is a
    // consequence of a Task-4 RULING (its deviation D5, restated at
    // ssn_engine.h's `SsnStart::prox_center_x`): the shipped proximal term
    // anchors at the CURRENT ITERATE rather than at a lagging centre, which is
    // what keeps the iteration modified-Newton on the EXACT residual and its
    // fixed points exact, unregularized KKT points. An anchor at the current
    // iterate needs no centre -- it needs only the LEVEL, which is
    // `prox_sigma`, and which `SsnOptions::prox_sigma_init` consumes directly.
    //
    // The two CENTRE vectors are carried anyway, for exactly the reason
    // ssn_engine.h carries `SsnStart::prox_center_x`/`prox_center_lambda` and
    // `SsnStart::slacks`: if Task 6 (or tycho) revives a lagging centre, the
    // hand-off object is already the right shape, and widening a serialized
    // interface later is the cost this avoids. They are DELIBERATELY NOT
    // populated on a solve whose ladder never armed (see the gate below), so
    // the shipped default configuration never pays for them at all.
    //
    // THE COST GATE, and it is why these three fields are free in practice:
    // `has_prox_center` is set ONLY when an SSN subproblem of the emitting
    // solve actually finished at a nonzero sigma. Under the shipped default
    // (`SqpOptions::qp_mode == QpMode::kWalk`) no SSN subproblem is ever
    // solved, so `has_prox_center` is false on every object this project
    // emits today and the two vectors stay empty -- an important property at
    // scale, where an n-vector and an (me+mi)-vector are not free (Phase 5's
    // n = 1e6 / 1285 MiB ceiling).
    //
    // **THE GATE IS THE SIGMA, NOT THE CENTRE** (sqp_driver.h:4277's own note;
    // T5 re-review NF-3, final branch review WAVE #11). `has_prox_center` CAN
    // be true with `prox_center_x`/`prox_center_lambda` still both empty --
    // the driver never populates either vector, only `prox_sigma` -- because
    // the sole ingest consumer (`sqp_driver.h`'s `ssn_prox_carry` read) uses
    // `prox_sigma` alone and never touches the centre fields. A future reader
    // reviving a lagging centre (the comment above) must NOT assume
    // `has_prox_center` implies a populated centre.
    //
    // HASH-GATED ON INGEST, like every other state field here. sqp_driver.h
    // reads this block only at `warm_state_ingest` (StartLevel::kWarm or
    // above), which is the hash-confirmed-provenance level -- the same gate
    // `funnel_width`, `tr_radius`, `primal_delta` and `dual_mu` already sit
    // behind. A `kSeeded` object may not claim a proximal history: its
    // provenance is unconfirmed, and a sigma from a DIFFERENT model would
    // damp the first subproblem of this one for no reason.
    //
    // WHAT `prox_sigma` MEANS ON EMISSION: the LARGEST sigma any SSN
    // subproblem of the emitting solve finished at -- "the proximal level
    // this solve found it needed" -- not the last one's, which would report 0
    // whenever the final major happened to be benign and would lose the very
    // fact the carry exists to transmit. -1 is not used; 0.0 is the honest
    // "the ladder never armed" reading and is also `SsnOptions`' own default.
    //
    // WHAT IT MEANS ON INGEST: the FIRST SSN subproblem of the receiving
    // solve starts its ladder there; every later major starts at 0 again.
    // That scope is deliberate. The first subproblem of a continuation step
    // is the one most like the previous step's last, so it is the one the
    // carry is evidence about; carrying it into every major would damp
    // subproblems no measurement says need damping, which is precisely the
    // trade `SsnOptions::prox_sigma_init`'s own ruling ("the proximal term is
    // a REPAIR, not a policy") refuses.
    Vec prox_center_x;      // n, or empty
    Vec prox_center_lambda; // me + mi, or empty
    double prox_sigma = 0.0;
    // Gates all three fields above, but a true value does NOT imply the two
    // centre vectors are populated (see THE GATE IS THE SIGMA, NOT THE
    // CENTRE, above) -- only that `prox_sigma` is meaningful. False on every
    // object this project emits under the shipped default -- see THE COST
    // GATE.
    bool has_prox_center = false;

    // false (default construction) == cold: see this header's own note.
    // Every exit of SqpDriver::solve() sets this true EXCEPT ONE -- a solve
    // that could not EVALUATE its own start point (kNumericalError before any
    // subproblem was built) reports false, because feeding that point back
    // would override the corrected x0 a caller retries with. That exception,
    // and why it is the only one, is stated in full in this header's own note
    // above (EXACTLY ONE EXIT IS EXCLUDED); it is repeated here because this
    // is the note sqp_types.h's SqpSolution::warm_start and sqp_driver.h's
    // make_warm_start both send the reader to.
    bool valid = false;
};

// =============================================================================
// PHASE-4 TASK 12: the interior-point crossover -- `from_interior_point`.
// =============================================================================
//
// Builds a WarmStart from an INTERIOR-POINT-STYLE primal-dual iterate: the
// Knitro crossover pattern (an IP method runs to near-KKT, then hands off to
// an active-set method for the final polish) -- in tycho this is exactly how
// a PSIOPT solve would seed this SQP driver's own refinement. Unlike
// mesh_transfer.h's MeshTransfer (which maps a WarmStart born from THIS
// driver's own solve onto a different mesh), this function's INPUT never
// went through this driver at all: there is no QpEngine working set, no
// funnel/trust-region history and, critically, no MODEL this function could
// hash -- so, exactly like MeshTransfer's own output (see that header's
// section 4), `structure_hash` is unconditionally 0 and `hot` is
// unconditionally null.
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
// this header), NOT the IP solver's own non-negative s_i: a caller wiring a
// PSIOPT- or Knitro-style s_i through must negate it first
// (slack_i = -s_i). Getting this backwards would flip every activity
// verdict below without producing any error, so it is stated here in the
// starkest terms this header has: PASS cI(x) VALUES, NEVER THE IP SLACK
// s_i UNNEGATED. The two bound duals are converted to this project's single
// `z` internally as `z = z_lower - z_upper` (>= 0 exactly when only the
// lower bound's multiplier is live, matching this project's own sign at an
// active lower bound; <= 0 the mirror case) -- a caller never has to perform
// that conversion itself.
//
// ACTIVITY INFERENCE (the Knitro-crossover pattern) -- **REWRITTEN IN PHASE-7
// TASK 0; the rule it replaced, and why it was wrong, are kept below because
// the replacement is only defensible against them.** An inequality row is
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
// estimates its own `mu_hat` and its own dual norm, since the two are
// different dual populations.
//
// WHY A DUAL-SIDE TEST, AND WHERE THE SLACK WENT. The rule this replaced was
//     |slack_i(j)| < activity_tol * max(1, |lambda_i(j)|)
// -- "residual small relative to its own multiplier" -- and its `max(1, .)`
// floor makes the test ABSOLUTE for every multiplier below O(1). A
// COLLOCATION multiplier is O(h) by construction (tests/support's F7 carries
// lambda_i* = h * nu(t_k, p)), while the residual an IP method stops at is
// mu/lambda, which GROWS as the price shrinks -- so on a mesh-refinement
// sequence the two move APART and the test's verdict degrades from
// "correct" to "empty" with nothing about the hand-off getting worse. It is
// measured, on F7 at p = 0.9 with mu = 1e-9, in
// docs/notes/2026-08-06-activity-tol-repair.md: 84 of 92 rows recovered at
// N = 100, 250 of 370 at N = 400, **0 of 1486** at N = 1600, and 0 of 92 at
// N = 100 once mu is the bridge's own 1e-8 default. The repaired rule
// recovers 92/92, 370/370 and 1484/1486 on the same three meshes.
//
// The slack is not ignored -- it is read AGGREGATELY rather than per row, as
// `mu_hat`, and that is the strictly stronger reading: on any hand-off worth
// the name lambda_i(j)*|slack_i(j)| is the SAME number (mu) on every row, so
// a per-row residual test carries no information a per-row dual test does not
// already carry, while the aggregate DOES carry the one thing the old rule
// could not see -- how converged the caller's iterate is. THE EXTREME-DUAL
// TRAP THE OLD RULE DOCUMENTED IS CLOSED BY THAT SAME TERM: a hand-off
// pricing a row at 1e6 against a residual of ~1 used to be certified ACTIVE
// (activity_tol * 1e6 == 1 at the old 1e-6 default); it now raises its own
// bar, since mu_hat is then ~1e6 and the threshold ~1e7, and the row is left
// FREE.
//
// AMBIGUOUS rows -- a price at or below the noise floor, the degenerate case
// an IP method's own numerical noise produces close to convergence -- are
// left FREE rather than guessed either way: the destination QP re-derives the
// correct working set from a real linearization, while a wrongly-forced-active
// row can only ever cost that QP work backing off a face the solution is not
// on (the same argument mesh_transfer.h's section 3 makes for its own
// unanimity rule). The `lambda_i(j) > dual_tol` conjunct also covers a
// WRONG-SIGN dual an IP method can hand off close to convergence (a small
// NEGATIVE lambda_i/z on a row that IS geometrically active): it fails for any
// non-positive value, so such a row is left free rather than certified active
// on a dual that is not even the right sign -- tests/test_warm_start.cpp's
// CrossoverWrongSignDualLeavesRowFree pins exactly this.
//
// THE PRECONDITION THE OLD FORMULA INHERITED IS DISCHARGED, NOT MOVED. It
// used to read: "the max(1, |lambda_i(j)|) scaling assumes an O(1)-SCALED
// DUAL, so callers should only feed this function points that are already
// near-complementary". The repaired rule asks for no dual scale at all --
// every quantity it compares against is derived from the hand-off's own data
// -- which is what makes it usable at collocation scale without a caller
// deriving a tolerance per mesh. THREE LIMITS REMAIN, all deliberate and all
// stated so a reader does not have to find them by measurement:
//   * `dual_tol` (1e-6) is still absolute. It stays that way because it
//     answers "is this multiplier zero?" against the driver's own stationarity
//     noise floor -- the same argument sqp_driver.h's kSeededDualClampTol
//     makes -- and because a relative version would certify the degenerate
//     1e-9-price/1e-9-slack row that CrossoverDegenerateRowLeftFree pins FREE.
//     It does overtake the rule's own threshold once the prices are small
//     enough -- on F7 the two cross around N ~ 800, and at N = 1600 dual_tol
//     (1e-6) sits above the rule's 5.00e-7 -- but LOWERING IT THERE IS A LOSS,
//     not a repair: the live rows a reader would blame it for (3.63e-7) are
//     below BOTH floors, so two decades of headroom recovers none of them and
//     admits the two barrier-noise rows (8.50e-7) that were sitting in the
//     band between the floors. See the overlap bullet below for why.
//
//     PAST THAT CROSSING `dual_tol` IS THE **SOLE** FALSE-ACTIVE GUARD, AND IT
//     IS AN ABSOLUTE FLOOR NEXT TO A QUANTITY THAT SCALES -- the same shape as
//     the defect this rule repaired, on the other side of the ledger. A slack
//     row's barrier-noise price is mu/|cI_j|, so the guard holds iff
//         mu <= max(kIpActivityFactor * ||lambda_i||inf * activity_rel_tol,
//                   dual_tol) * min |cI_j| over the strictly slack rows,
//     which past N ~ 800 is just mu <= dual_tol * min|cI| -- 1.2e-9 on F7 at
//     N = 1600. So mu = 1e-9 (just) holds and mu = 1e-8, ONE DECADE AWAY AND
//     THE LEVEL THE SHIPPED PSIOPT BRIDGE HANDS OVER AT, does not: measured
//     false actives are 12 / 24 / 46 at N = 1600 / 3200 / 6400 against
//     0 / 2 / 4 at mu = 1e-9, with recall identical at both levels
//     (CrossoverFalseActiveGuardDegradesAtTheOperativeBarrierLevel). The
//     degradation is MILD -- the false-active fraction of the hint stays near
//     0.8 % -- and the rule is deliberately not redesigned to chase it: the
//     honest fix is a dual_tol that scales, every relative form of which
//     certifies the degenerate row above, so it is a ruling on evidence a
//     corpus is about to produce rather than an implementation detail. A
//     caller crossing over at a fine mesh should hand over at mu no looser
//     than dual_tol * min|cI|, or treat the hint as ~1 % wrong.
//   * A FINE ENOUGH MESH MAKES THE TWO POPULATIONS OVERLAP, and no dual-side
//     rule survives that. On F7 at N = 1600 with mu = 1e-9 the weakest
//     genuinely-active price is 3.63e-7 while the loudest barrier noise on a
//     strictly slack row is 8.50e-7 -- so recovering the last 2 of 1486 rows
//     costs at least 2 invented ones, whatever the threshold. The rule takes
//     the conservative side (2 missed, 0 invented), which is the same
//     direction the AMBIGUOUS paragraph above argues for. The condition is
//     mu >~ activity_rel_tol * kIpActivityFactor * ||lambda_i||inf * min|cI|
//     on the slack rows, and a caller that needs the last rows must hand over
//     a tighter mu rather than a looser tolerance.
//   * A LOOSE hand-off (mu large against its own dual scale, i.e.
//     kIpActivityFactor * mu_hat > ||lambda_i||inf * activity_rel_tol) is the
//     regime where the inference stops being trustworthy, and on a fine mesh
//     it can be WRONG rather than merely empty: at mu = 1e-4 on F7 N = 1600,
//     where every live price is below 5.1e-4, the rule infers 114 rows and all
//     114 are false. That is measured and carried in the note's section 5; the
//     honest reading is that identification is not available from a hand-off
//     that loose, and a caller crossing over at mu >> ||lambda||inf * 1e-3
//     should expect a hint it would have been no worse off without.
//
// A kFixed variable (lower(i) == upper(i), types.h's BoundState) is reported
// +1, the same arbitrary-but-consistent choice WarmStart::bound_active's own
// note makes for a solve-derived object.
//
// NO FUNNEL/TRUST-REGION/REGULARIZATION STATE: funnel_width, tr_radius,
// primal_delta and dual_mu all stay at their "never populated" -1 sentinels
// (this struct's own defaults) -- an IP method's own trajectory carries none
// of these in a form this project's globalization understands (its trust
// region and adaptive regularization are THIS driver's own bookkeeping, not
// a generic IP concept), so the destination solve uses SqpOptions' plain
// defaults for all four rather than being handed a borrowed value with no
// justified scale.
//
// THE CROSSOVER'S VALUE -- RESOLVED IN PHASE-6 TASK 5, AND THE HISTORY IS KEPT
// BECAUSE IT IS THE WHOLE ARGUMENT FOR THE LEVEL THAT REPLACED IT.
//
// THROUGH PHASE 5 this note read "THE CROSSOVER'S VALUE TODAY" and said:
// exactly like mesh_transfer.h's own PHASE-6 INGEST GAP note,
// `structure_hash == 0` means sqp_driver.h's WARM-START INGEST rule (its
// `warm_dims_plausible` check) resolves EVERY solve fed this object to
// StartLevel::kCold, regardless of how well-formed lambda_e/lambda_i/z/the
// activity vectors are; so `driver.solve(model, crossover.x, crossover)` was
// BYTE-IDENTICAL to the two-argument `driver.solve(model, crossover.x)`, and
// the duals and activity guess this function computes -- real and correctly
// signed -- were consumable only by a caller that read them off the returned
// object. The note then deferred the repair to "a future, non-hash-gated
// ingest level ... the same Phase-6 design question mesh_transfer.h's own note
// raises".
//
// THAT LEVEL IS `StartLevel::kSeeded` AND THE DEFERRAL IS DISCHARGED. A
// crossover object is `valid`, is dimensionally consistent with the model a
// caller is crossing over INTO (it was built from that model's own n/me/mi), and
// carries finite values, so it resolves kSeeded on an ordinary 3-argument
// `solve()`. WHAT CHANGES, CONCRETELY:
//   - `lambda_e`, `lambda_i` and the activity hint now REACH the solve. The
//     3-arg call is no longer byte-identical to the 2-arg one, and
//     tests/test_warm_start.cpp's CrossoverRecoversExactSolveHs14 -- which
//     pinned that identity as the honest statement of what the crossover bought
//     -- now pins the OPPOSITE: the seeded call resolves kSeeded and its
//     counters are measured against the x-only call rather than equal to it.
//   - `crossover.x` is taken FROM THE OBJECT, not from the caller's `x0`
//     argument, exactly as at kWarm. A caller that wants its own x0 honoured
//     must still pass `WarmStart{}`.
//   - The B-1 clear (this header's PHASE-5 TASK 7b paragraph) applies, which on
//     this path is a genuine improvement: interior-point duals are strictly
//     positive on EVERY row, and the ones the destination model reports strictly
//     slack are exactly the stale prices the clear exists to drop.
//   - The seeded `lambda_i >= 0` clamp applies, which on this path is the one
//     defence this producer specifically needs -- see the SIGN CONVENTION note
//     above, and `CrossoverWrongSignDualLeavesRowFree`'s -1e-8 fixture.
// WHAT DOES NOT CHANGE: `structure_hash` is still 0 and `hot` still null, so a
// crossover can never reach kWarm or kHot and can never authorize the reuse of a
// factorization of a matrix nobody here has seen. That was always the point of
// the sentinel and kSeeded does not weaken it.
//
// =============================================================================
// PHASE-7 TASK 0: the activity rule's own constants.
// =============================================================================
//
// The safety factor `nu` of the activity rule above, NOT an option: it is one
// decade of margin over the larger of the two scales the rule compares
// against, which is the same "an order above the producer's own tolerance"
// step sqp_driver.h's kSeededDualClampTol derivation takes (see
// docs/notes/2026-08-04-kseeded-ingest.md section 2), and it is fixed here
// rather than exposed because a caller has no data with which to choose it
// that the rule has not already read for itself. Its product with the default
// `activity_rel_tol` is 1e-3 -- exactly the relative floor
// tests/test_warm_start_battery.cpp derived by hand for this family before
// the repair, which is why that fixture reads the same 92 rows with the
// override deleted.
inline constexpr double kIpActivityFactor = 10.0;

struct IpCrossoverOptions {
    // eps_rel: how far below the LARGEST price a live price may sit, as a
    // fraction. 1e-4 against kIpActivityFactor = 10 puts the floor three
    // decades below ||lambda_i||inf, which on F7 at N = 100 sits 17x under the
    // weakest genuinely-active price (1.41e-4) and 115x over the strongest
    // barrier-noise price on an inactive row (7.0e-8) -- the widest separation
    // available on that family, measured in the repair note's section 4.
    //
    // **THIS FIELD REPLACED `activity_tol`, WHICH WAS ABSOLUTE (default 1e-6)
    // AND MEANT SOMETHING ELSE** -- it multiplied the SLACK, not the dual. The
    // rename is deliberate rather than a silent redefinition: a caller that
    // tuned the old knob per mesh (the only way to use it at collocation
    // scale) gets a compile error and reads this note, instead of a
    // same-named knob quietly changing what it scales.
    double activity_rel_tol = 1e-4;
    // Absolute "is this multiplier zero?" floor -- see the note above on why
    // this one stays absolute and when a caller should lower it.
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
// population (nothing priced) yields 0, which leaves the caller's `dual_tol`
// conjunct as the only gate -- correct, since a population with no positive
// price carries no scale to be relative to.
//
// PRICED ENTRIES ONLY, on both statistics. A zero or negative price is not a
// statement about the barrier level (it is the absence of one), and averaging
// it in would drag `mu_hat` toward zero in proportion to how many rows the
// hand-off left unpriced -- making the threshold depend on the model's SIZE
// rather than on its hand-off's quality. The `> 0.0` form also skips a NaN
// PRICE, which matters because from_interior_point does not check finiteness
// (only the driver's kSeeded gate does, and only on x/lambda_e/lambda_i). A
// NaN RESIDUAL against a real price does reach the mean and makes the
// threshold NaN, whereupon every `>=` below is false and the hint comes out
// EMPTY -- the safe direction, and the one this header takes on every other
// malformed input.
//
// THE RESIDUAL IS A TEMPLATE PARAMETER so a caller can pass an Eigen
// EXPRESSION (`x - lower`) rather than a materialized vector: this function is
// reachable at n = 1e6 through the crossover cells of a scale corpus, and two
// n-sized temporaries per call is a cost with nothing to show for it. Nothing
// else about the signature depends on it.
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
// Dimensions: n = x.size(), me = lambda_e.size(), mi = lambda_i.size().
// slack_i must match mi; z_lower, z_upper, lower and upper must each match
// n. Throws std::invalid_argument (T6, offending sizes in the message) on
// any mismatch.
inline WarmStart from_interior_point(const Vec &x, const Vec &lambda_e, const Vec &lambda_i,
                                     const Vec &slack_i, const Vec &z_lower, const Vec &z_upper,
                                     const Vec &lower, const Vec &upper,
                                     const IpCrossoverOptions &opts = {}) {
    const Index n = x.size();
    const Index mi = lambda_i.size();
    if (slack_i.size() != mi) {
        throw std::invalid_argument(fmt::format(
            "from_interior_point: slack_i has size {}, expected {} (lambda_i's own size -- one "
            "cI value per inequality row)",
            slack_i.size(), mi));
    }
    if (z_lower.size() != n) {
        throw std::invalid_argument(
            fmt::format("from_interior_point: z_lower has size {}, expected {} (x's own size)",
                        z_lower.size(), n));
    }
    if (z_upper.size() != n) {
        throw std::invalid_argument(
            fmt::format("from_interior_point: z_upper has size {}, expected {} (x's own size)",
                        z_upper.size(), n));
    }
    if (lower.size() != n) {
        throw std::invalid_argument(fmt::format(
            "from_interior_point: lower has size {}, expected {} (x's own size)", lower.size(), n));
    }
    if (upper.size() != n) {
        throw std::invalid_argument(fmt::format(
            "from_interior_point: upper has size {}, expected {} (x's own size)", upper.size(), n));
    }

    WarmStart out;
    out.x = x;
    out.lambda_e = lambda_e;
    out.lambda_i = lambda_i;
    // This project's single signed convention (this header's SIGN
    // CONVENTIONS note): z >= 0 at an active lower bound (z_lower live,
    // z_upper == 0), z <= 0 at an active upper bound (mirrored).
    out.z = z_lower - z_upper;

    out.ineq_active.assign(static_cast<std::size_t>(mi), 0);
    out.qp_working_set = WorkingSet(n, mi);
    // ONE threshold for the whole row population, computed BEFORE the loop:
    // the rule is relative to the hand-off's own dual scale, which is a
    // property of the vector, not of a row (this header's ACTIVITY INFERENCE
    // note).
    const double row_threshold =
        detail::ip_activity_threshold(lambda_i, slack_i, opts.activity_rel_tol);
    for (Index j = 0; j < mi; ++j) {
        const bool active = lambda_i(j) > opts.dual_tol && lambda_i(j) >= row_threshold;
        if (active) {
            out.ineq_active[static_cast<std::size_t>(j)] = 1;
            out.qp_working_set.add_ineq(j);
        }
    }

    out.bound_active.assign(static_cast<std::size_t>(n), 0);
    std::vector<BoundState> &states = out.qp_working_set.bound_state();
    // The two bound sides are SEPARATE dual populations (a variable can be
    // priced at one side only), so each sets its own scale. A caller that
    // prices an UNBOUNDED side -- z_lower(i) > 0 against lower(i) = -1e20 --
    // poisons its side's mu_hat with a ~1e20 product and drives that side's
    // whole verdict to FREE; that is the safe direction this header always
    // takes on a malformed hand-off, and such an input is a contradiction the
    // caller must not produce in the first place (nlp_model.h's +/-1e20
    // convention marks a side nothing can be active at).
    const double lower_threshold =
        detail::ip_activity_threshold(z_lower, x - lower, opts.activity_rel_tol);
    const double upper_threshold =
        detail::ip_activity_threshold(z_upper, upper - x, opts.activity_rel_tol);
    for (Index i = 0; i < n; ++i) {
        if (lower(i) == upper(i)) {
            // kFixed: sits at both bounds at once, not sign-constrained --
            // the same arbitrary-but-consistent +1 WarmStart::bound_active's
            // own note picks for a solve-derived object.
            out.bound_active[static_cast<std::size_t>(i)] = 1;
            states[static_cast<std::size_t>(i)] = BoundState::kFixed;
            continue;
        }
        const bool active_lower = z_lower(i) > opts.dual_tol && z_lower(i) >= lower_threshold;
        const bool active_upper = z_upper(i) > opts.dual_tol && z_upper(i) >= upper_threshold;
        if (active_lower && !active_upper) {
            out.bound_active[static_cast<std::size_t>(i)] = -1;
            states[static_cast<std::size_t>(i)] = BoundState::kAtLower;
        } else if (active_upper && !active_lower) {
            out.bound_active[static_cast<std::size_t>(i)] = 1;
            states[static_cast<std::size_t>(i)] = BoundState::kAtUpper;
        } else {
            // FREE: both the AMBIGUOUS (neither fires) and the CONTRADICTORY
            // (both fire) cases resolve the same safe way. The second is a
            // hand-off pricing BOTH sides of a variable that is not fixed --
            // impossible on a converged central path, where one side's price
            // is always at the barrier noise floor, and a statement the
            // inference declines to arbitrate when it does arrive. (Phase-7
            // Task 0 note: the pre-repair rule made this unreachable "under
            // any sane tolerance" by ALSO requiring both gaps to be small,
            // which cannot hold for lower(i) != upper(i). The repaired rule
            // reads duals only, so the case is reachable and is spelled out
            // rather than assumed away.)
            out.bound_active[static_cast<std::size_t>(i)] = 0;
            states[static_cast<std::size_t>(i)] = BoundState::kFree;
        }
    }

    out.funnel_width = -1.0;
    out.tr_radius = -1.0;
    out.primal_delta = -1.0;
    out.dual_mu = -1.0;
    // BY DESIGN, and NOT the condition Phase-5 Task 0 repaired in the driver:
    // there is no model here to hash in the first place, so the hash is
    // UNKNOWN rather than merely uncomputed. See WarmStart::structure_hash.
    out.structure_hash = 0;
    out.hot = nullptr; // never hot: no factorization exists to offer.
    out.valid = true;
    return out;
}

} // namespace tycho::sqp
