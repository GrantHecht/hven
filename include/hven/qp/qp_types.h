// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <limits>
#include <type_traits>

#include <Eigen/Dense>
#include <Eigen/SparseCore>

#include "hven/core/solver_counters.h"
#include "hven/core/solver_status.h"
#include "hven/core/types.h"

namespace hven::solvers {

// This header defines no type alias: code in hven::solvers that says `Index`
// resolves to hven::Index by ordinary enclosing-namespace lookup -- same
// spelling at every call site, one type, one vocabulary. The identity pin
// below is the compile-time proof that this is sound on every supported
// target rather than merely convenient: it fires if hven::Index is ever moved
// back off Eigen's index type, which is precisely the change that would
// re-split this layer's vocabulary on Apple (where std::int64_t is `long long`
// and std::ptrdiff_t is `long` -- distinct types of the same width, measured
// on CI) without touching a line of SQP code.
static_assert(std::is_same_v<Eigen::Index, hven::Index>,
              "hven::Index must BE Eigen::Index: the SQP layer spells its index type "
              "`Index` and relies on that resolving to ONE type on every target. On Apple "
              "arm64 std::int64_t (long long) and std::ptrdiff_t (long) are distinct types "
              "-- measured, CI run 31902660573 -- which is why this is pinned rather than "
              "assumed (see hven/core/types.h).");

// Guards the backend handoff: the sparse matrices this layer builds and hands
// to the backends are hven::SpMatRM, and their storage index must stay `int`.
static_assert(std::is_same_v<hven::SpMatRM::StorageIndex, int>,
              "hven::SpMatRM's storage index must stay 32-bit: the sparse backends are built "
              "against int indices and hven hands these arrays over with no reindexing pass "
              "(hven/core/types.h; hven/detail/linear/pardiso_session.h).");

/// @brief Per-variable bound state in the working set.
enum class BoundState {
    kFree = 0,
    kAtLower = 1,
    kAtUpper = 2,
    kFixed = 3,
};

/// @brief Selects which linear-algebra path the QP engine uses to solve each
/// working set's KKT system: kRefactorize re-derives and refactorizes a
/// bound-eliminated K (kkt_assembly.h's assemble_kkt) from scratch at every
/// working-set change; kSchurBorder (the default) holds one persistent
/// full-variable K0 and represents working-set changes as Schur-complement
/// borders (schur_complement.h). Border mode is cheaper on the box-shaped
/// trust-region case that motivated it, and the two paths are held
/// observationally equivalent over the whole fixture set by dedicated tests;
/// kRefactorize remains selectable and stays the equivalence oracle — every
/// border-mode battery is checked against IT, not the other way around.
///
/// Equivalence scoping callout: "convex H" alone is NOT enough for the two
/// modes to return the same POINT (a real fixture walked into this). Where H
/// is convex but NOT strictly convex the optimal set is a FACE, and a bordered
/// full-variable K0 versus a bound-eliminated K legitimately return different,
/// equally optimal points of it — up to the primal_delta the engine adds,
/// which makes the solved system formally strictly convex and the selected
/// point undetermined at O(delta) — after which the driver follows different
/// trajectories (visible as a status difference only under a too-small
/// budget). Read the equivalence claim as "the same optimal VALUE, and the
/// same point wherever that point is unique". Every cross-mode battery solves
/// a strictly convex H and is unaffected.
enum class WorkingSetLinearAlgebra {
    kRefactorize,
    kSchurBorder,
};

/// @brief QP solver options.
struct QpOptions {
    /// Primal regularization magnitude, baked onto the regularized diagonal of
    /// K0 by assemble_kkt_core: thickens the (1,1) block so the solved system
    /// is formally strictly convex. >= 0 and never NaN; 0 is a REAL SETTING
    /// ("no regularization on this block"), not a sentinel.
    ///
    /// Not free: its price is a footprint on what the solve can claim
    /// (~|g|/primal_delta bounds how far a free variable can run before it
    /// reads as an unbounded artifact — see qp_engine.h's
    /// unbounded_artifact_threshold, written in terms of this field).
    ///
    /// Validated at solve start (QpEngine::solve throws std::invalid_argument
    /// otherwise): a NaN burned the entire minor budget returning a NaN
    /// iterate; a negative produced spurious failures up to a confident
    /// kInfeasible on a feasible subproblem.
    double primal_delta = 1e-8;

    /// Dual regularization magnitude, baked onto the regularized (2,2) block so
    /// an over-determined working set stays solvable. Same domain, same
    /// validation, same 0-is-real caveat as primal_delta; its footprint is
    /// dual_mu*|lambda| on a row residual (qp_engine.h's row_tolerance).
    double dual_mu = 1e-8;
    double feas_tol = 1e-9;
    double opt_tol = 1e-9;

    /// Minor-iteration cap for ONE QP solve. Two meanings, by sign:
    ///
    ///     > 0   an EXPLICIT, ABSOLUTE cap. Wins outright, at any size.
    ///    <= 0   THE SENTINEL, AND THE SHIPPED DEFAULT: derive the cap from
    ///           this subproblem's own size, as
    ///               max(kQpMaxIterFloor, kQpMaxIterCoeff * (n + mi + #bounded))
    ///           -- qp_engine.h's detail::effective_qp_max_iter, which carries
    ///           the derivation and calibration in full.
    ///
    /// Why a size-derived default: the minors one subproblem needs grows with
    /// the constraint count, so ANY fixed constant is eventually below demand
    /// (recorded on the F7 collocation family well below n = 1000). This is a
    /// lower-bound argument, not a tuning preference.
    ///
    /// The precedence rule above IS the escape hatch that makes the default
    /// safe: only solves that both leave the sentinel AND actually reach the
    /// cap can change; a healthy solve never touches it. A capped subproblem
    /// remains an honest failure — QpStatus::kMaxIter with minor_iters == the
    /// effective cap; raising the cap never converts a wrong answer into a
    /// right one, it only stops refusing subproblems that were going to
    /// finish.
    Index max_iter = 0;
    Index schur_cap = 128;
    double schur_cond_max = 1e9;
    WorkingSetLinearAlgebra ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;

    /// l-infinity trust-region radius Delta, applied about the SOLVE'S OWN
    /// start point x0 (there is no separate configurable center — see
    /// qp_engine.h's "Section 6"): effective bounds lo_eff = max(lower, x0 -
    /// Delta), up_eff = min(upper, x0 + Delta) are computed once at the top of
    /// solve() and used in place of lower/upper for the rest of that solve.
    /// Default +inf disables the feature exactly (lo_eff == lower, up_eff ==
    /// upper; the engine takes a path materializing nothing new — see
    /// qp_engine.h for the bit-identical claim this default is load-bearing
    /// for).
    ///
    /// PER-INSTANCE, CONST: fixed for the lifetime of the owning QpEngine and
    /// cannot vary across solve() calls THROUGH THIS FIELD. A caller needing a
    /// different radius per call (a shrink-retry loop) uses
    /// SolveOverrides::tr_radius instead, which never touches this field — so
    /// one QpEngine serves a whole retry loop keeping its hot-start K0/border
    /// reuse across it. Unlike primal_delta/dual_mu, tr_radius never joins the
    /// reuse key: bounds never enter K0.
    ///
    /// Validated at solve start on the same domain as the override: >= 0 or
    /// +inf — never negative (it would silently cross lo_eff/up_eff behind an
    /// assert a Release build compiles out), never NaN (read as "disabled", a
    /// meaning only +inf has).
    double tr_radius = std::numeric_limits<double>::infinity();
};

/// @brief Per-solve overrides for the three QpOptions fields a driver needs to
/// vary call-to-call on ONE QpEngine instance: the trust-region radius
/// (shrink-retry loops) and the primal/dual regularization (adaptive
/// regularization). Passed to QpEngine::solve()'s override-taking overloads;
/// the plain overloads forward a default-constructed SolveOverrides, which
/// resolves to every field's opts_ value — byte-identical to solving with no
/// override support at all.
///
/// SENTINEL CONVENTION: a field at its default means "no override — use the
/// owning QpEngine's opts_ value". tr_radius's sentinel is +inf;
/// primal_delta/dual_mu's sentinel is any negative value. Consequence: a
/// caller needing tr_radius genuinely disabled on an engine whose opts_
/// carries a finite radius must construct those opts_ with tr_radius = +inf —
/// there is no way to request "+inf, overriding finite opts_" through the
/// override, the same single-sentinel tradeoff QpOptions::tr_radius itself
/// makes.
///
/// REUSE-KEY CONSEQUENCE (see qp_engine.h's HOT-START REUSE note): the
/// EFFECTIVE (primal_delta, dual_mu) pair enters K0's values (assemble_kkt_core
/// bakes both onto the regularized diagonal), so it joins the hot-start reuse
/// key — a warm re-solve whose effective pair differs forces a K0 rebuild. The
/// effective tr_radius does NOT join that key: it only tightens bounds into
/// lo_eff/up_eff, and bounds never enter K0.
///
/// Validated at solve start (QpEngine::solve throws std::invalid_argument
/// otherwise, before anything else runs): tr_radius must be the +inf sentinel
/// or >= 0 — never a negative non-sentinel Delta, never NaN. A negative Delta
/// would silently cross lo_eff/up_eff behind a Release-compiled-out assert,
/// and this is checked explicitly because tr_radius is per-solve DRIVER INPUT.
/// primal_delta/dual_mu keep the negative-means-sentinel convention; NaN is
/// rejected for both.
struct SolveOverrides {
    double tr_radius = std::numeric_limits<double>::infinity(); ///< +inf = use opts_.tr_radius.
    double primal_delta = -1.0;                                 ///< <0 = use opts_.primal_delta.
    double dual_mu = -1.0;                                      ///< <0 = use opts_.dual_mu.
};

} // namespace hven::solvers
