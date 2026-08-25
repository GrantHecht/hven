// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// start_level.h -- StartLevel, the warm-start ingest level a solve resolves to,
// its display-string helper, and the per-level histogram a Ledger reports over
// whole-driver solves.

#include <hven/core/types.h>

namespace hven::solvers {

// `Index` below is `hven::Index`, reached by ordinary enclosing-namespace
// lookup out of `hven::solvers`; there is no SQP-side index alias.

/// How much of a previous solve's state a caller intends to feed into the
/// next one. THE ORDER IS LOAD-BEARING AND THE ENUM IS COMPARED BY IT:
/// sqp_driver.h's ceiling test compares `static_cast<int>` and tests read the
/// enumerators directly (`EXPECT_GE(step.level, kWarm)`-style), so inserting
/// a new level means placing it between its neighbours, not appending.
///
/// Per level, each strictly more trusting than the last:
/// - kCold ignores the previous state entirely (an ordinary cold solve).
/// - kSeeded TRUSTS VALUES, NOT PROVENANCE. Takes exactly:
///     * `x`, `lambda_e`, `lambda_i` -- fed into the FIRST convergence test,
///       so a seeded object that genuinely IS a KKT point of the posed
///       problem can be certified in zero majors;
///     * the ACTIVITY HINT (`qp_working_set`) -- seeded into the first QP
///       subproblem's working set as a GUESS, through the same seed path
///       every other major uses, so the engine's own WINDOW-CONSISTENCY RULE
///       may drop any part that does not fit the first trust-region window.
///       AT THIS LEVEL AN EMPTY HINT IS NO HINT: an all-free working set
///       means "no activity was attributed", not "nothing is active"
///       (ineq_active/bound_active's field notes say so), so a hint-less
///       seeded object leaves the first subproblem unseeded and thereby
///       RE-ARMS SqpOptions::crash_basis if it is on; a hint-CARRYING one
///       suppresses the crash basis exactly as a kWarm object does.
///   The two hash-less producers -- from_interior_point below and
///   mesh_transfer.h's MeshTransfer -- emit `structure_hash == 0` BY
///   CONSTRUCTION (neither has a model of the DESTINATION solve to hash),
///   and kSeeded is the ingest level built for exactly such objects.
///   WHAT IT NEVER TAKES, each exclusion because the object cannot justify
///   it without provenance:
///     * THE FACTORIZATION (`hot`): the hash exists precisely to protect
///       factorization reuse, so an object with no hash can never reach kHot
///       (kSeeded < kWarm < kHot).
///     * THE FUNNEL WIDTH and THE TRUST-REGION RADIUS: both are measured in
///       the units of a DIFFERENT problem's violation measure and step scale
///       (mesh_transfer.h section 4 declines to transfer funnel_width;
///       kSeeded extends the refusal to tr_radius, which MeshTransfer does
///       carry). The destination solve seeds its funnel from Eq. (9) at its
///       own first measured h0 and starts at SqpOptions::tr_init, exactly as
///       a cold solve does.
///     * THE KUNGURTSEV-DIEHL FULL-STEP WINDOW (SqpOptions::warm_full_step):
///       the full-step-first rule presumes local convergence of a warm start
///       ON THE SAME PROBLEM; a seeded object makes no such claim.
///   AND ONE THING NO OTHER LEVEL HAS: `lambda_i >= 0` IS ENFORCED AT INGEST
///   (sqp_driver.h's THE SEEDED DUAL CLAMP). Every other route into the
///   ingest is hash-gated and the producers that clear a hash gate are
///   non-negative by construction; kSeeded admits objects a FOREIGN solver
///   or a caller's own hand assembled -- the input class warm_start.h's SIGN
///   CONVENTIONS paragraph states as a PRECONDITION and sqp_driver.h's THE
///   INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY note itemizes as ungated.
///   At the seeded level a small negative price is CLAMPED to zero and
///   counted; a large one degrades the whole object to kCold.
///   The ingest gate is DIMENSIONS AND FINITENESS, NOTHING ELSE: `valid`,
///   then x/lambda_e/lambda_i/qp_working_set sized against (n, me, mi), then
///   those vectors finite. NO HASH IS REQUIRED and none is computed -- a
///   solve whose ceiling is kSeeded pays no resolution probe at all. An
///   object failing the gate resolves kCold.
/// - kWarm additionally trusts the object's provenance -- a structural-hash
///   MATCH against this model -- and with it the
///   globalization/regularization state.
/// - kHot additionally offers `hot` for the engine to reuse a cached
///   factorization keyed on that same `structure_hash`, gated the same way
///   kWarm always was: a caller opts in only by having received a `hot`
///   handle from a prior solve; SqpOptions::start_level still CAPS the
///   result, so a caller that never wants kHot pinned can clamp it there.
enum class StartLevel { kCold, kSeeded, kWarm, kHot };

/// @brief Maps StartLevel to a short display string; defined in a library TU.
const char *to_string(StartLevel level);

/// How many of a ledger's whole-driver solves resolved at each StartLevel --
/// the aggregate for a crossover or mesh-refinement loop's headline question:
/// how many of its hand-offs were actually ingested, and at what level.
///
/// ONE FIELD PER ENUMERATOR, NOT A MAP OR AN ARRAY INDEXED BY THE ENUM: the
/// integer values are load-bearing for ORDERING (kSeeded sits BETWEEN kCold
/// and kWarm, and inserting a level renumbers everything after it), so
/// anything indexed by them would silently re-bind its columns on such an
/// insertion; named fields cannot.
struct StartLevelHistogram {
    /// @brief Solves that resolved to kCold.
    Index cold = 0;
    /// @brief Solves that resolved to kSeeded.
    Index seeded = 0;
    /// @brief Solves that resolved to kWarm.
    Index warm = 0;
    /// @brief Solves that resolved to kHot.
    Index hot = 0;

    /// The number of SqpSolveRecords the histogram was built from: the four
    /// columns account for every solve iff this equals their sum.
    Index total() const { return cold + seeded + warm + hot; }
};

} // namespace hven::solvers
