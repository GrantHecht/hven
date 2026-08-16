#pragma once

// start_level.h -- StartLevel, the warm-start ingest level a solve resolves to,
// its display-string helper, and the per-level histogram a Ledger reports over
// whole-driver solves.
//
// M3 PHASE-C S2 MOVED ALL THREE HERE UNCHANGED: the enum and its `to_string`
// from `hven/detail/warmstart/warm_start.h`, the histogram from
// `hven/core/ledger.h`. The reason is layering, not taste -- `core/ledger.h`
// and `core/solver_counters.h` both consume `StartLevel`, and a `core/` header
// may not reach up into `detail/warmstart/` for it (CLAUDE.md section 2's tier
// order). `warm_start.h` includes this header, so every call site that reached
// `StartLevel` through it still does.

#include <hven/core/types.h>

namespace hven::solvers {

// `Index` BELOW IS `hven::Index`, reached by ordinary enclosing-namespace
// lookup out of `hven::solvers` -- there is no SQP-side index alias any more.
// This header used to re-declare one (`using Index = Eigen::Index;`), because
// M3 phase-C S1 had found `hven::Index` and `Eigen::Index` to be DISTINCT types
// on Apple arm64 and the SQP layer needed Eigen's; `qp/qp_types.h` carried the
// normative block and the pins, and this was a copy of the alias rather than an
// include of that header, since `core/` may not depend upward on `qp/`. Phase-C
// S2c redefined `hven::Index` onto `Eigen::Index`, which made both the alias
// and the copy redundant; `qp_types.h` keeps the historical record and the
// identity pin. The upward-dependency argument is untouched: this header
// includes `core/types.h`, a `core/` sibling.

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

// PHASE-6 TASK 5. How many of a ledger's whole-driver solves resolved at each
// StartLevel -- the aggregate the kSeeded level made worth having, since a
// crossover or mesh-refinement loop's headline question is "how many of my
// hand-offs were actually ingested, and at what level".
//
// ONE FIELD PER ENUMERATOR, NOT A MAP OR AN ARRAY INDEXED BY THE ENUM. The
// enum's integer values are load-bearing for ORDERING (warm_start.h's
// StartLevel note: kSeeded had to be inserted BETWEEN kCold and kWarm, which
// renumbered kWarm and kHot), so anything that indexes by them would silently
// re-bind its columns the next time a level is inserted. Named fields cannot.
//
// `total()` is the number of SqpSolveRecords the histogram was built from, so a
// caller can assert the four columns account for every solve rather than
// assuming they do.
struct StartLevelHistogram {
    Index cold = 0;
    Index seeded = 0;
    Index warm = 0;
    Index hot = 0;

    Index total() const { return cold + seeded + warm + hot; }
};

} // namespace hven::solvers
