#pragma once

#include <limits>
#include <type_traits>

#include <Eigen/Dense>
#include <Eigen/SparseCore>

#include "hven/core/solver_counters.h"
#include "hven/core/solver_status.h"
#include "hven/core/types.h"

namespace hven::solvers {

// Type alias. This header used to define `Vec` and `SpMatU` as well --
// SQP-side spellings of types `hven` itself owns. Phase-C S1 made them aliases
// OF the core types; phase-C S2b finished the job and deleted them, moving
// every call site onto `hven::Vec` and `hven::SpMatRM` so the SQP layer and the
// rest of the library speak one vocabulary. `Index` is the one alias that
// stays, and the reason it stays is load-bearing; read the note before
// changing it.
//
// `Index` IS NOT RECONCILED, and the reason is a measured fact, not caution.
// M3 phase-C S1 attempted `using Index = hven::Index;` on the plan's stated
// premise that `std::int64_t` and `std::ptrdiff_t` are the same type on LP64
// Linux and on both Apple targets. THE PREMISE IS FALSE ON APPLE: on macOS
// arm64, `std::int64_t` is `long long` while `std::ptrdiff_t` (and therefore
// `Eigen::Index`) is `long`. Two 64-bit signed types, same width, same
// signedness, DISTINCT TYPES -- so the reconcile would silently change what
// `hven::solvers::Index` denotes on Apple and could re-resolve an overload
// there, which is the exact hazard the plan named. Observed at the CI
// macos-clang-release lane, which failed the identity pin below with
// `std::is_same_v<long long, long>` (run 31902660573, commit 58dac2a). The
// pin did its job: it turned an assumption into a compile error on the one
// target where it is wrong.
//
// So `Index` stays `Eigen::Index` here. What IS pinned is the interoperation
// invariant the boundary actually needs -- `hven::Index` and this `Index` are
// the same width and signedness, so values cross the linear-algebra boundary
// without truncation or sign change on every supported target, whether or not
// the two are the same type.
using Index = Eigen::Index;

// The interoperation pin (phase-C S1; required by the O7 ruling, not optional).
//
// The pins live HERE, on the consumer side, and deliberately not in
// core/types.h: `hven::Index` is documented there as a fixed-width type
// independent of Eigen's platform-defined one, and hven does not promise the
// two coincide -- on Apple they demonstrably do not. This header is where the
// two vocabularies meet, so this is where the relationship is checked.
static_assert(sizeof(Index) == sizeof(hven::Index) &&
                  std::is_signed_v<Index> == std::is_signed_v<hven::Index>,
              "hven::solvers::Index (Eigen::Index / std::ptrdiff_t) and hven::Index "
              "(std::int64_t) must stay the same width and signedness: they are NOT the "
              "same type on Apple targets (long vs long long), and every value that "
              "crosses between the SQP layer and hven's linear-algebra boundary relies on "
              "the conversion being exact.");

// The 32-bit-index pin (phase-C S1; re-aimed at the library's own alias in S2b,
// when the SQP-side spelling it used to name was deleted). It guards the same
// handoff it always did: the sparse matrices this layer builds and hands to the
// backends are `hven::SpMatRM`, and their storage index must stay `int`.
static_assert(std::is_same_v<hven::SpMatRM::StorageIndex, int>,
              "hven::SpMatRM's storage index must stay 32-bit: the sparse backends are built "
              "against int indices and hven hands these arrays over with no reindexing pass "
              "(hven/core/types.h; hven/detail/linear/pardiso_session.h).");

// Bound state enumeration
enum class BoundState {
    kFree = 0,
    kAtLower = 1,
    kAtUpper = 2,
    kFixed = 3,
};

// Selects which linear-algebra path the QP engine uses to solve each
// working-set's KKT system: kRefactorize re-derives and refactorizes a
// bound-eliminated K (kkt_assembly.h's assemble_kkt) from scratch every
// working-set change; kSchurBorder holds one persistent full-variable K0
// (assemble_kkt_full) and represents working-set changes as Schur-complement
// borders (schur_complement.h) instead of re-factorizing. kSchurBorder is the
// default -- qp_engine.h's QpEngineBorder.BorderModeMatchesRefactorizeMode
// (and the tiny-schur_cap rebuild/recovery batteries alongside it) hold the
// two paths observationally equivalent over the whole fixture set for convex
// H, and border mode is cheaper on the box-shaped (all-bounds-active)
// trust-region case that motivated it. kRefactorize remains selectable and
// stays the equivalence oracle: every border-mode battery is checked against
// it, not the other way around.
//
// "CONVEX H" IS NOT ENOUGH FOR THE TWO MODES TO RETURN THE SAME POINT, and
// the gap is worth naming because a real fixture walked into it (Phase-5
// Task 4, docs/notes/2026-07-31-schur-cap-policy.md section 4). Where H is
// convex but NOT STRICTLY convex the subproblem's optimal set is a FACE, and
// the two paths -- a bordered full-variable K0 versus a bound-eliminated K --
// legitimately return different, equally optimal points of it. Measured on
// tests/test_sqp_restoration.cpp's InfeasibleCircleLineModel, whose Lagrangian
// Hessian is 2*lambda_e(0)*I and is therefore IDENTICALLY ZERO on every major
// whose elastic relaxation is still open (an open relaxation carries no
// multipliers out): both modes land on the same optimal face (x0 + x1 = 2,
// same objective) at different points -- exactly, up to the primal_delta the
// engine adds, which makes the solved system formally strictly convex and the
// "face" a delta-flat set whose selected point is undetermined at O(delta)
// rather than an exact LP face -- and the DRIVER above them then follows
// different trajectories from there -- visible as a status difference only
// under a budget too small for both to finish. So read the equivalence claim
// as "the same optimal VALUE, and the same point wherever that point is
// unique"; it is not a promise about which point of a degenerate optimal face
// comes back. This is a scoping correction to the sentence above, not a
// retraction: every battery that checks the two modes against each other
// solves a strictly convex H and is unaffected.
enum class WorkingSetLinearAlgebra {
    kRefactorize,
    kSchurBorder,
};

// QP solver options
struct QpOptions {
    double primal_delta = 1e-8;
    double dual_mu = 1e-8;
    double feas_tol = 1e-9;
    double opt_tol = 1e-9;
    // MINOR-ITERATION CAP FOR ONE QP SOLVE. Two meanings, by sign:
    //
    //     > 0   an EXPLICIT, ABSOLUTE cap. Wins outright, at any size.
    //    <= 0   THE SENTINEL, AND THE SHIPPED DEFAULT: derive the cap from
    //           this subproblem's own size, as
    //               max(kQpMaxIterFloor, kQpMaxIterCoeff * (n + mi + #bounded))
    //           -- qp_engine.h's detail::effective_qp_max_iter, which carries
    //           the derivation, the calibration and the reasoning in full.
    //
    // PHASE-6 TASK 4 (M6) CHANGED THIS DEFAULT FROM A FIXED 500, on a ruling,
    // and the argument is a LOWER-bound argument rather than a tuning
    // preference. The minors ONE subproblem needs grows with the problem's
    // constraint count (docs/notes/2026-08-03-identification-stall-study.md
    // Sec. 3's cost law), so ANY fixed constant is eventually below the
    // demand: 500 is already below it at n = 250 on the F7 collocation family
    // (docs/notes/2026-07-30-scale-study-cold.md Sec. 5), and 6 of the 15
    // instances of the F7 cold grid were recorded as SQP-engine FAILURES that
    // are nothing but this cap sitting under a demand of 850-7341 minors
    // (docs/notes/2026-08-03-identification-stall-study.md Sec. 5, which
    // recovers three of them outright by raising it and nothing else).
    //
    // THE PRECEDENCE RULE IS THE ESCAPE HATCH, and it is what makes this a
    // safe default change: a caller who sets this field to any positive value
    // gets exactly that value, so every fixture that deliberately caps at 1
    // or 2 to force a kMaxIter exit, and every battery that pins a
    // cap-signature counter, keeps its old behaviour by construction. Only
    // solves that (a) leave this at its default AND (b) actually reach the
    // cap can change at all -- a healthy solve never touches it, which is why
    // the pins that move under M6 are the cap-signature ones and no others.
    //
    // A CAPPED SUBPROBLEM REMAINS AN HONEST FAILURE, unchanged: it reports
    // QpStatus::kMaxIter with minor_iters == the effective cap, and the
    // driver above it routes that as it always has. Raising the cap never
    // converts a wrong answer into a right one; it only stops refusing
    // subproblems that were going to finish.
    Index max_iter = 0;
    Index schur_cap = 128;
    double schur_cond_max = 1e9;
    WorkingSetLinearAlgebra ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;

    // l-infinity trust-region radius Delta, applied about the SOLVE'S OWN
    // start point x0 (there is no separate configurable center -- see
    // qp_engine.h's "Section 6" note): effective bounds
    // lo_eff = max(lower, x0 - Delta), up_eff = min(upper, x0 + Delta) are
    // computed once, at the top of solve(), and used in place of `lower`/
    // `upper` for the rest of that solve. Default +inf disables the feature
    // (lo_eff == lower, up_eff == upper exactly, and the engine takes a path
    // that materializes nothing new -- see qp_engine.h for the bit-identical
    // claim this default is load-bearing for).
    //
    // PER-INSTANCE, CONST: like every other QpOptions field, this is fixed
    // for the lifetime of the owning QpEngine (see its constructor) and
    // cannot vary across solve() calls on one instance THROUGH THIS FIELD.
    // A caller that needs a DIFFERENT radius per call -- e.g. a Phase-3
    // shrink-radius retry loop re-solving a rejected SQP step at a smaller
    // Delta -- uses SolveOverrides::tr_radius (below) instead: it is
    // resolved once per solve() call and never touches this const field, so
    // one QpEngine instance now serves an entire retry loop and keeps its
    // hot-start K0/border reuse across it (see qp_engine.h's PHASE-3 DESIGN
    // FRICTION note, updated for this resolved state, and its HOT-START
    // REUSE note for why tr_radius -- unlike primal_delta/dual_mu below --
    // never has to join the reuse key: bounds never enter K0).
    double tr_radius = std::numeric_limits<double>::infinity();
};

// Per-solve overrides for the three QpOptions fields a Phase-3 SQP driver
// needs to vary call-to-call on ONE QpEngine instance: the trust-region
// radius (shrink-retry loops) and the primal/dual regularization (adaptive
// regularization). Passed to QpEngine::solve()'s override-taking overloads;
// the plain overloads forward a default-constructed SolveOverrides, which
// resolves to every field's opts_ value -- see qp_engine.h's per-field
// resolution rule -- so they are byte-identical to solving with no override
// support at all.
//
// SENTINEL CONVENTION: a field at its default means "no override -- use the
// owning QpEngine's opts_ value for this solve", exactly like QpOptions::
// tr_radius's own default already means "no radius" when read directly.
// tr_radius's sentinel is +inf (same value QpOptions::tr_radius defaults to,
// so there is nothing to encode specially); primal_delta/dual_mu's sentinel
// is any negative value (both are physically positive regularization
// constants, so negative is otherwise meaningless). A caller that needs
// tr_radius genuinely disabled on an engine whose opts_.tr_radius is finite
// can construct that engine's opts_ with tr_radius = +inf in the first
// place -- SolveOverrides has no way to request "+inf, overriding a finite
// opts_ default" because it cannot be told apart from "no override", the same
// single-sentinel tradeoff QpOptions::tr_radius itself already makes.
//
// REUSE-KEY CONSEQUENCE (see qp_engine.h's HOT-START REUSE note): the
// EFFECTIVE (primal_delta, dual_mu) pair this resolves to enters K0's values
// (assemble_kkt_core bakes both onto the regularized diagonal), so it joins
// the hot-start reuse key -- a warm re-solve whose effective pair differs
// from the previous solve's forces a K0 rebuild. The effective tr_radius
// does NOT join that key: it only ever tightens bounds into lo_eff/up_eff,
// and bounds never enter K0 (see qp_engine.h for the full argument).
//
// PRECONDITION, VALIDATED AT SOLVE START (QpEngine::solve throws
// std::invalid_argument otherwise, before anything else in that call can run
// -- see run()'s VALIDATE FIRST block): tr_radius must be either the +inf
// sentinel above or a value >= 0 -- NEVER a negative, non-sentinel Delta and
// NEVER NaN. A negative Delta would silently cross lo_eff/up_eff (section 6's
// CROSSED EFFECTIVE BOUNDS CANNOT HAPPEN proof assumes Delta >= 0), which the
// engine can only catch with an `assert` that a Release (NDEBUG) build
// compiles out entirely -- so this is checked explicitly rather than left to
// that assert, and it is checked because tr_radius is now per-solve DRIVER
// INPUT (a Task-6 shrink-retry loop's own arithmetic), not a value only ever
// set once, by hand, in opts_. primal_delta/dual_mu keep the negative-means-
// sentinel convention above unchanged -- a negative value is valid and
// meaningful for them -- but NaN is rejected for both, for the same reason:
// it is not a value either field's resolution or any downstream arithmetic
// can absorb.
struct SolveOverrides {
    double tr_radius = std::numeric_limits<double>::infinity(); // +inf = use opts_.tr_radius
    double primal_delta = -1.0;                                 // <0 = use opts_.primal_delta
    double dual_mu = -1.0;                                      // <0 = use opts_.dual_mu
};

} // namespace hven::solvers
