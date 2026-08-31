// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_engine.h -- the interior-point (IP-PMM) QP tier: declarations only.
//
// WHAT THIS TIER IS. A third QP kernel beside the working-set walk
// (detail/qp/qp_engine.h) and the semismooth Newton engine
// (detail/qp/ssn_engine.h): a primal-dual barrier method with Mehrotra
// predictor-corrector steps on the section 3.1 system, a monotone
// (rho, delta) proximal regularization schedule, and the Wachter-Biegler
// inertia-correction ladder on top of it. Specified in
// docs/notes/2026-08-m6-w1-ipqp-spec.md; the task decomposition is
// docs/notes/2026-08-m6-w1-implementation-plan.md.
//
// WHAT IT IS NOT. It is NOT a branch inside InteriorPointSolver: none of that
// engine's Settings/SolveResult/alg_impl, none of detail/globalization/'s
// acceptance strategies, funnels, watchdogs, recovery chains or restoration.
// Globalization here is fraction-to-boundary and nothing else (spec 3.1 item
// 3). It is a sibling of SsnEngine.
//
// REACHABILITY. As of M6 W1 task 4 this engine is reachable from TESTS ONLY.
// QpMode::kIpm is still refused at validate_sqp_options (task 1's temporary
// refusal), the routing chain is task 6's, and the certification/escape census
// is task 5's -- see THE TASK-4 BOUNDARY below for exactly which parts of the
// specification this file implements today.
//
// TU PLACEMENT (CLAUDE.md section 5). This header carries declarations, enums
// and `inline constexpr` constants ONLY. The iteration, the ladder, the
// residual contract and the equilibration live in src/qp/ipqp_engine.cpp:
// they are orchestration, and orchestration lives in a .cpp regardless of how
// hot the surrounding loop is. The per-element hot loops the iteration runs
// are detail/qp/ipqp_math.h's kernels, which stay header-inline for exactly
// the opposite reason -- see that file's own TU-placement note.
//
// -------------------------------------------------------------------------
// THE TASK-4 BOUNDARY -- what this file does and does not do yet
// -------------------------------------------------------------------------
//
// IMPLEMENTED HERE (task 4): the clamp-centred box and its domain gate
// (IpqpBox / IpqpBounds), the cold start (spec 5.6), the Mehrotra
// predictor-corrector iteration (3.1), the gated (rho, delta) schedule with
// its monotone floor (3.2), the inertia gate and the W-B ladder (2.2), Ruiz
// equilibration with unscale-on-export (4.3), the relative-KKT stopping rule
// (3.4), the required final unregularized inertia read (2.2 item 4), the
// budget/factorization caps, and the ratio face classification (2.3 item 2).
//
// NOT HERE YET, and deliberately so:
//   * THE ESCAPE CENSUS. `IpqpCounters::ipqp_escapes` and its five-way census
//     are TASK 5's. This engine sets `IpqpResult::escape_reason` -- correctly
//     classified per plan section 7 note (h) -- and leaves the six census
//     counters at 0, which keeps the sum-to-`ipqp_escapes` invariant true
//     (0 == 0) rather than half-populated.
//   * THE EARLY-STALL TEST (6.2) and INFEASIBLE-SUSPECT (6.3). Task 5.
//     IpqpEscape carries their enumerators so the vocabulary is fixed once;
//     nothing in this task returns them.
//   * THE WARM RESTART (section 5's repair, mu clamp and warm-kill). TASK 7.
//     `solve()` takes an `IpqpSeed *` so the signature does not move under
//     task 7, and REFUSES a non-null one with std::invalid_argument rather
//     than silently ignoring it -- a primal-only near-solution consumed
//     without section 5.2's repair is the "worse than neutral" hazard the
//     specification names, so accepting one unrepaired would be the wrong
//     kind of quiet.
//   * THE ROUTING CHAIN (2.3 items 3-5), tier-3 hand-off, and the K = 3
//     retirement ladder (6.1). Task 6. This engine reports the face
//     classification the chain reads; it never routes.

#include <cstdint>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/core/ledger.h>
#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/core/types.h>
#include <hven/detail/interior/kkt_factorization.h>
#include <hven/detail/qp/ipqp_kkt_layout.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/drivers/sqp_types.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

namespace detail {

/// @brief The FIRST rung of the inertia ladder, as an absolute floor: Ipopt's
/// `delta_w_0` (Wachter-Biegler 2006, Algorithm IC).
///
/// The ladder multiplies, so without a floor a ladder starting from a decayed
/// schedule value (`rho` reaches `kProxRegFloor = 1e-10` on a healthy solve)
/// would spend six rungs -- six numeric factorizations -- at magnitudes far
/// too small to change any inertia before reaching a value that can. The
/// first rung is therefore `max(rho * kIpqpRhoGrowth, kIpqpRhoLadderInit)`.
inline constexpr double kIpqpRhoLadderInit = 1.0e-4;

/// @brief Ladder growth per rung, TWO DECADES -- `detail::kSsnProxGrowth`'s
/// value, adopted for the reason that ladder's own banner gives: at a 1e6
/// ceiling two decades per rung gives a ladder short enough that exhausting
/// it is bounded work, and one decade per rung doubles the factorization bill
/// to buy resolution no fixture has ever needed. Applied to `rho` on a wrong
/// inertia and to `delta` on a perturbed-pivot report.
inline constexpr double kIpqpRhoGrowth = 100.0;

/// @brief The interior push's two Ipopt constants (`bound_push` /
/// `bound_frac`): the cold start's `x_0` is moved to
/// `max(x_0, l + min(kIpqpBoundPushAbs * max(1, |l|), kIpqpBoundPushRel * (u - l)))`
/// and symmetrically at the upper side.
///
/// Both sides move by at most `kIpqpBoundPushRel` of the box width, so the
/// two pushes cannot cross even on a very narrow box -- which is what makes
/// "the tier keeps a strict interior by construction" a statement about the
/// code and not a hope. A ZERO-WIDTH box cannot reach here at all: it is
/// refused by the domain gate (IpqpBounds) before the start point is built.
inline constexpr double kIpqpBoundPushAbs = 1.0e-2;
inline constexpr double kIpqpBoundPushRel = 1.0e-2;

/// @brief The cold start's slack floor (spec 5.6: `s_0 = max(bi - Ai x_0, 1)`).
inline constexpr double kIpqpSlackInit = 1.0;

/// @brief The (rho, delta) schedule's DECREASE GATE (spec 3.2's
/// bounded-decrease condition), as a CONTRACTION FACTOR: the regularized
/// problem's RELATIVE residual must have fallen to this multiple of its value
/// at the last estimate advance before the schedule may fall again and
/// `(zeta, lambda_est)` may move.
///
/// A RATIO, AND THAT IS LOAD-BEARING RATHER THAN STYLISTIC. The first version
/// of this gate compared the regularized residual against the current `mu`
/// -- two ABSOLUTE quantities -- and the comparison is then a statement about
/// the problem's units, not about its progress: on a QP whose gradient is of
/// order 1e6 the gate can never fire, `rho` stays at `ipqp_rho_init`, and the
/// tier converges to the PROXIMALLY BIASED point `argmin f(x) + rho/2 |x -
/// zeta|^2` while reporting healthy relative residuals in the well-scaled
/// coordinates. Measured, on a two-variable fixture with six decades between
/// its blocks: the tier returned x2 = 0.004988 where the answer is 2, which
/// is `4e-2 / (2e-2 + 8)` exactly -- the bias, solved for. The contraction
/// form has no units and cannot fail that way.
///
/// NOT A PAPER CONSTANT, and said so rather than dressed up as one.
/// Cipolla-Gondzio state the condition abstractly (the PMM subproblem is
/// solved to an accuracy tied to the current penalty); the FACTOR is an
/// implementation choice. 0.5 -- "the regularized residual has halved" -- is
/// slack enough that a healthy Newton solve, which contracts far faster than
/// that, advances the schedule on nearly every iteration, and tight enough
/// that a stalling one stops advancing it. What matters for CORRECTNESS is
/// only that the decrease is GATED at all: an ungated geometric decay
/// (`prox_reg_decay`'s) is what spec 3.2 rules out.
inline constexpr double kIpqpRegGateContract = 0.5;

/// @brief The factorization budget's sentinel multiple (IpqpOptions'
/// `ipqp_max_factorizations <= 0`): one iteration costs one factorization
/// plus ladder rungs, and three per iteration is the ladder headroom the
/// budget grants before it calls a solve pathological.
inline constexpr Index kIpqpFactorizationsPerIter = 3;

/// @brief Ruiz equilibration (spec 4.3): sweep cap and stopping tolerance.
///
/// Ruiz's own convergence result is linear with a small constant, so the
/// practical sweep count is single digits; OSQP and Clarabel both ship 10.
/// The tolerance stops early once every row/column norm is within
/// `kIpqpRuizTol` of 1, which on a well-scaled KKT matrix happens in two or
/// three sweeps.
inline constexpr Index kIpqpRuizSweeps = 10;
inline constexpr double kIpqpRuizTol = 1.0e-3;

} // namespace detail

/// @brief Why a tier solve STOPPED without a standing certificate.
///
/// SsnEscape's vocabulary and SsnEscape's warning, restated because they bind
/// this tier identically: **NONE OF THESE CERTIFIES ANYTHING**, and the
/// driver **branches on `escape_reason`, never on `status`** -- `kInfeasible`
/// is a certificate word the walk issues and this tier never does (spec 6.3:
/// IP-PMM has no homogeneous self-dual embedding and therefore no
/// infeasibility certificate).
///
/// The indefinite/numerical boundary is plan section 7 note (h), which is a
/// settler ruling and not a judgement call at the call site:
///   * `kIndefinite` -- an inertia reading WAS taken (an
///     `InertiaEvidence::State` was actually observed) and DISAGREED with the
///     required signature: at the section 2.2 item 4 final certification
///     factorization, or when the monotone ladder reached `ipqp_reg_max` with
///     the reading still wrong. Saddle-suspect; section 2.3 item 4 routes it
///     to the SSN warm grade.
///   * `kNumerical` -- everything else that is not budget, stall or
///     infeasible-suspect: a factorization failure, an UNREADABLE inertia (no
///     evidence state observed at all), a non-finite iterate, residual or
///     step.
enum class IpqpEscape {
    kNone = 0,
    kBudget = 1,
    kStall = 2,
    kIndefinite = 3,
    kNumerical = 4,
    kInfeasibleSuspect = 5,
};

/// @brief The section 2.3 ratio rule's three-way verdict on one inequality row
/// or one variable-bound side.
///
/// A RATIO rule, not an absolute threshold, and three-way rather than binary
/// on purpose: a pair whose BOTH members are tiny carries no information about
/// which side of the face it is on, and forcing it either way is exactly the
/// thresholding-boundary artifact the E1 study recorded (one row at relative
/// slack 1.33e-8 against a 1e-8 absolute rule, whose dual was unambiguous).
/// `kUncertain` is counted (`IpqpCounters::ipqp_face_uncertain`) and handed to
/// the exact tier-3 refinement rather than asserted.
enum class IpqpFace {
    kInactive = 0,
    kActive = 1,
    kUncertain = 2,
};

/// @brief THE IMMUTABLE CLAMP-CENTRED BOX (T4.a; spec 2.1, plan ruling 2).
///
/// Computed ONCE at solve entry from the effective trust-region radius and
/// never rebuilt inside a solve:
///
///     c(i)      = clamp(0, lower(i), upper(i))
///     lo_eff(i) = max(lower(i), c(i) - Delta)
///     up_eff(i) = min(upper(i), c(i) + Delta)
///
/// THE CENTRE RULE IS `refine_on_face`'s OWN, and that is the reason it is
/// this rule rather than the plain origin. `QpEngine::refine_on_face`
/// (qp_engine.cpp's gate, qp_engine.h's public-API precondition) gates its
/// refined point against a window "centred on the clamped origin",
/// `c = min(max(0, lo), up)`, and takes no centre parameter. `QpProblem`
/// permits zero to lie OUTSIDE the box (qp_problem.h::validate checks sizes
/// and the upper-triangle convention, nothing else). A tier that centred its
/// own box on the plain origin would therefore hand tier 3 a point gated
/// against a DIFFERENT window than the one the tier itself solved in --
/// silently, and only on the problems where zero is outside the box, which is
/// the worst possible failure profile. Sharing the rule removes the
/// disagreement by construction.
///
/// THE CONTRACT, CHOSEN AND STATED: `IpqpEngine::solve()` takes NO centre
/// parameter. The clamp rule IS the documented contract. A warm iterate
/// (`IpqpSeed::x`, task 7) lives INSIDE this box; it never redefines the
/// centre and never becomes the refinement centre. A trust-region
/// shrink-retry rebuilds the box at the new radius under the same rule and
/// re-clamps the iterate into it.
struct IpqpBox {
    Vec centre;          ///< n. `clamp(0, lower, upper)`.
    Vec lo_eff;          ///< n. `max(lower, centre - Delta)`.
    Vec up_eff;          ///< n. `min(upper, centre + Delta)`.
    double radius = 0.0; ///< The EFFECTIVE Delta this box was built at.

    /// The first index (ascending) whose effective width is NOT STRICTLY
    /// POSITIVE (`lo_eff(i) >= up_eff(i)`), or -1 when every width is
    /// positive. See IpqpBounds for what a non-negative value here means.
    ///
    /// `>=` rather than `==` deliberately, so a CROSSED declared box
    /// (`lower(i) > upper(i)`, which `QpProblem::validate` does not reject --
    /// it checks sizes and the upper-triangle convention only) lands here too
    /// and DECLINES rather than throwing. A crossed box is a legitimate,
    /// infeasible QP, not a caller error, and the walk answers it exactly
    /// (`QpStatus::kInfeasible`) where a barrier method has no interior to
    /// start from at all.
    Index zero_width_index = -1;
};

/// @brief THE TIER'S EFFECTIVE BOUNDS AND ITS DOMAIN GATE (T4.b; plan rulings
/// 7 and r3.2). Replaces the WITHDRAWN verbatim reuse of `BoundSet`.
///
/// `BoundSet` (detail/interior/bound_set.h) is the NLP engine's:
/// reduced-space indices, RELAXED bound values, fixed variables already
/// eliminated, damping indicators materialized by the NLP's own classifier.
/// A `QpProblem` has none of that -- dense n-vectors with a +/-1e20 absent
/// sentinel, nothing eliminated -- so the reuse was withdrawn (plan section 7
/// note d) in favour of this.
///
/// REPRESENTATION, SETTLED HERE (task 3's carried item). DENSE, not index
/// lists. The ipqp_math.h kernels every iteration runs are written against
/// dense `(x, l, u, zl, zu)` with presence decided by `ipqp_has_lower` /
/// `ipqp_has_upper`, so dense IS the shape the hot path wants and no overload
/// beside those kernels is needed. Index lists would be a SECOND answer to
/// "which bounds are present", derivable from the first and able to disagree
/// with it after any edit; the counts below carry everything the orchestration
/// actually needs (the complementarity denominator and the inertia
/// bookkeeping) without that risk.
///
/// THE ZERO-WIDTH RULE, CHOSEN AND STATED. A subproblem containing ANY
/// `lo_eff(i) == up_eff(i)` pair is OUT OF THIS TIER'S DOMAIN. The build
/// reports it (`zero_width_index`), the dispatch DECLINES pre-solve and routes
/// to the walk, and `IpqpCounters::ipqp_declined_pinned` records it. **A
/// DECLINE IS NOT AN ESCAPE**: the tier never ran, so it never counts toward
/// `ipqp_escapes` or the K = 3 retirement threshold.
///
/// Why declining covers every case, rather than relaxing the pair by an
/// epsilon the way the v2 draft proposed: under the clamp centre a zero-width
/// pair can arise ONLY at a declaration-level `lower(i) == upper(i)` or at
/// `Delta == 0`. That is the tree's own statement, at qp_engine.h's section 6
/// note -- "lo_eff(i) == up_eff(i) can only happen at Delta == 0 for a
/// variable not already genuinely kFixed". Both cases are exactly what the
/// walk solves best (it pins the variable and eliminates it), an
/// epsilon-relaxed pin would no longer be an EXACT pin, and the tier keeps a
/// strict interior by construction instead of by an epsilon. The v2 rule and
/// its `ipqp_fixed_bounds_relaxed` counter are deleted entirely (plan section
/// 7 note e): no equal-bound pair survives to the barrier, so there is nothing
/// to relax.
struct IpqpBounds {
    Vec lower; ///< n. `IpqpBox::lo_eff`. `<= -kIpqpInfBound` means ABSENT.
    Vec upper; ///< n. `IpqpBox::up_eff`. `>= kIpqpInfBound` means ABSENT.

    Index num_lower = 0; ///< Variables with a finite effective lower bound.
    Index num_upper = 0; ///< Variables with a finite effective upper bound.

    /// `IpqpBox::zero_width_index` carried through: the first index whose
    /// effective width is not strictly positive, or -1.
    Index zero_width_index = -1;

    /// True iff this subproblem is inside the tier's domain, i.e. no zero-width
    /// pair. `false` is a DECLINE, not a failure -- see the zero-width rule
    /// above.
    ///
    /// Defined in the .cpp, not here: it is a predicate consulted ONCE per
    /// solve at the domain gate, not a per-element hot loop, so CLAUDE.md
    /// section 5's inlining criterion does not apply to it and this header's
    /// declarations-and-inline-constexpr-only budget does.
    bool in_domain() const;
};

/// @brief The tier's own iterate, carried across majors by task 7's
/// subproblem-level warm restart (spec 5.1 flow (b)).
///
/// Declared in full here so `solve()`'s signature is final at task 4 and does
/// not move under task 7. **Task 4 REFUSES a non-null seed** -- see the
/// TASK-4 BOUNDARY note at the top of this file for why refusing beats
/// silently ignoring.
///
/// Deliberately NOT routed through `QpSolution`, which has no slack and no
/// barrier block and whose shape is published: the tier carries its state the
/// way `QpEngine` carries `BorderState`/`HotState`, as engine-internal
/// currency.
struct IpqpSeed {
    Vec x;            ///< n, INSIDE the box (never the box's centre -- see IpqpBox).
    Vec s;            ///< mi, > 0.
    Vec lambda_e;     ///< me.
    Vec lambda_i;     ///< mi, > 0.
    Vec zl;           ///< n, >= 0 (0 where the lower bound is absent).
    Vec zu;           ///< n, >= 0 (0 where the upper bound is absent).
    double mu = 0.0;  ///< The seed's own barrier parameter (spec 5.3's clamp input).
    Vec zeta;         ///< n, the proximal primal estimate.
    Vec lambda_est_e; ///< me, the proximal dual estimate.
    Vec lambda_est_i; ///< mi, the proximal dual estimate.
};

/// @brief The relative KKT residual the tier converges on (T4.d, plan ruling 5).
///
/// A NEW implementation, owned by this tier. The reuse ledger's "QP
/// residual-scale helpers" row was WITHDRAWN (plan section 7 note d): the
/// walk's helpers are `QpEngine` PRIVATE MEMBERS bound to a `WorkingSet`
/// (qp_engine.cpp), and this tier has no working set and cannot reach them.
/// The DISCIPLINE is nonetheless the walk's, from
/// `detail::free_block_stationarity` (qp_engine.h): every residual is divided
/// by a `max(1, ...)` fold over the largest terms that went into it, so the
/// test is scale-free upward and never LOOSER than an absolute one.
///
/// Full-KKT and WorkingSet-free, and taken on the UNREGULARIZED problem: the
/// proximal terms `rho (x - zeta)` and `delta (y - lambda_est)` are a solver
/// device, so a point is judged against the caller's QP, not against the
/// regularized one the last factorization described.
///
/// Convergence-agreement pins against the walk are the cross-check that the
/// two implementations judge alike.
struct IpqpResiduals {
    double stationarity = 0.0;    ///< ||Hx + g + Ae'ye + Ai'yi - zl + zu||inf / scale.
    double primal_eq = 0.0;       ///< ||Ae x - be||inf / scale.
    double primal_iq = 0.0;       ///< ||Ai x + s - bi||inf / scale.
    double complementarity = 0.0; ///< max pair |s.y|, |(x-l).zl|, |(u-x).zu| / scale.

    /// The largest of the four -- the single number the stopping rule and the
    /// stall window read.
    double worst() const;
};

/// @brief One tier solve's outcome.
///
/// **BRANCH ON `escape_reason`, NEVER ON `status`** -- SsnResult's rule,
/// binding here for the same reason: `status` alone would let a driver promote
/// a suspicion to a certificate.
struct IpqpResult {
    QpStatus status = QpStatus::kOptimal;
    IpqpEscape escape_reason = IpqpEscape::kNone;
    IpqpCounters counters;

    /// True iff the domain gate declined this subproblem pre-solve
    /// (IpqpBounds' zero-width rule). `counters.ipqp_declined_pinned` is 1 and
    /// every other counter is at its default.
    ///
    /// EXACTLY ONE OTHER FIELD IS MEANINGFUL ON THAT PATH, and naming it
    /// matters because the routing chain reads it: `box` IS populated -- the
    /// decline is DERIVED from it, so it has to be -- and it carries
    /// `zero_width_index`, which is the index that caused the decline. No
    /// iterate, no face, no residual: the tier never ran.
    bool declined_pinned = false;

    /// True iff the certificate was DOWNGRADED (spec 2.2 item 4): the final
    /// unregularized inertia read disagreed, or no evidence state could be
    /// observed at all. A downgraded solve never reports `kOptimal`.
    bool certificate_downgraded = false;

    // --- the point ---------------------------------------------------------

    Vec x;        ///< n.
    Vec lambda_e; ///< me.
    Vec lambda_i; ///< mi, >= 0.

    /// SIGNED bound multiplier, `QpSolution::z`'s convention (>= 0 at an
    /// active lower bound, <= 0 at an active upper one), recombined as
    /// `zl - zu` and FORCED TO 0 at a trust-region-pinned index -- exactly
    /// QpSolution's contract, including its stationarity caveat: at such an
    /// index the reported quantities do NOT satisfy stationarity, because the
    /// multiplier that balanced the row was the TR dual and TR duals are
    /// internal.
    Vec z;

    // --- the tier's own blocks (task 7's seed reads these) ------------------

    Vec s;              ///< mi, > 0. `bi - Ai x` up to the primal residual.
    Vec zl;             ///< n, >= 0. RAW, not TR-swept.
    Vec zu;             ///< n, >= 0. RAW, not TR-swept.
    double mu = 0.0;    ///< The measured complementarity at the returned point.
    double rho = 0.0;   ///< The (rho, delta) schedule's final primal value.
    double delta = 0.0; ///< ... and its final dual value.

    // --- the face blocks (spec 2.3 item 2; the routing chain's input) -------

    std::vector<IpqpFace> ineq_face;  ///< mi.
    std::vector<IpqpFace> lower_face; ///< n. kInactive at an ABSENT lower bound.
    std::vector<IpqpFace> upper_face; ///< n. kInactive at an ABSENT upper bound.

    /// mi / n / n, the two shapes `QpSolution` publishes an active set in.
    /// Derived from the three face vectors above: a row is active iff its
    /// verdict is `kActive` (an UNCERTAIN row reads inactive here and is
    /// counted in `ipqp_face_uncertain` -- never forced either way).
    std::vector<bool> ineq_active;
    std::vector<BoundState> bound_state;

    /// n, `QpSolution::tr_active`'s contract verbatim: true at i iff the
    /// variable is held by a TR-TIGHT effective bound rather than a real one.
    /// Such a variable reports `kFree` in `bound_state` and 0 in `z`.
    std::vector<bool> tr_active;

    // --- evidence ----------------------------------------------------------

    IpqpResiduals residuals;

    /// The box this solve ran in -- the answer to "which window was this point
    /// gated against", which tier 3's hand-off needs and which is exactly the
    /// thing IpqpBox exists to keep from being re-derived differently.
    IpqpBox box;
};

/// @brief The IP-PMM interior-point QP tier.
///
/// STATEFUL ACROSS SOLVES, like `QpEngine`'s border cache and `SsnEngine`'s
/// pattern cache: the KKT factorization, its symbolic analysis and the
/// `IpqpKktLayout` scatter plan persist on the instance, so a sequence of
/// subproblems sharing a sparsity pattern pays ONE symbolic analysis (spec
/// 4.1's hoisting rule). `IpqpOptions::ipqp_hoist_symbolic == false` turns
/// that off and forces a fresh symbolic pass every entry.
///
/// ONE LAYOUT SERVES ONE BUFFER for its whole life (IpqpKktLayout's own
/// contract): the layout on this instance is only ever handed
/// `kkt_.matrix()`, and nothing else may be.
class IpqpEngine {
  public:
    explicit IpqpEngine(const QpOptions &opts);

    IpqpEngine(const IpqpEngine &) = delete;
    IpqpEngine &operator=(const IpqpEngine &) = delete;

    /// Defined in the .cpp for the same reason `IpqpBounds::in_domain()` is:
    /// a const accessor is not a per-element hot loop, and this header carries
    /// declarations and `inline constexpr` only.
    const QpOptions &options() const;

    /// Attach a ledger for instrumentation (nullptr = off, default off).
    /// `QpEngine::attach_ledger`'s contract verbatim, including its
    /// failure-path rule: a `solve()` that THROWS emits no record and does not
    /// advance the per-solve counter, so the next successful solve gets the
    /// label it would have had if the throwing call had not been made.
    ///
    /// The emitted `SolveRecord` carries a QP-SHAPED PROJECTION of this tier's
    /// work (`minor_iters` <- `ipqp_iters`, `factorizations` <-
    /// `ipqp_factorizations`, `symbolic_analyses` <- `ipqp_symbolic_analyses`)
    /// so one ledger holds all three kernels' rows on one scale. The full
    /// `IpqpCounters` travel on `IpqpResult`, never through this projection.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

    /// @brief Solve `qp` by the interior-point tier.
    ///
    /// @param qp        the subproblem. Validated (`QpProblem::validate`).
    /// @param seed      TASK 7's warm restart. **Must be nullptr in task 4**;
    ///                  a non-null seed throws (see the TASK-4 BOUNDARY note).
    /// @param iopts     the tier's own settings (validated by
    ///                  `validate_sqp_options`; re-checked here at the API
    ///                  boundary, because this engine is reachable without a
    ///                  driver).
    /// @param overrides the walk's own per-solve override type
    ///                  (`qp_types.h`), unchanged: `tr_radius` disables at
    ///                  +inf, `primal_delta`/`dual_mu` are sentinel-negative.
    ///                  `tr_radius` is the ONE field this tier reads --
    ///                  `primal_delta`/`dual_mu` are the walk's regularized
    ///                  working-set device and have no meaning for a barrier
    ///                  method, which carries its own `(rho, delta)`; they are
    ///                  VALIDATED (so a malformed value is still refused) and
    ///                  then deliberately unused, which is stated here rather
    ///                  than left for a reader to infer from silence.
    ///
    /// @throws std::invalid_argument for CALLER errors -- a malformed
    ///         `QpProblem`, an out-of-range `IpqpOptions` or `SolveOverrides`
    ///         field, a non-null seed. A solve that cannot make progress
    ///         reports a status and an `IpqpEscape` and does NOT throw; that
    ///         separation is `SsnEngine::solve`'s and is kept exactly.
    IpqpResult solve(const QpProblem &qp, const IpqpSeed *seed, const IpqpOptions &iopts,
                     const SolveOverrides &overrides);

  private:
    struct Workspace;

    QpOptions opts_;

    KktFactorization kkt_;
    IpqpKktLayout layout_;

    /// True once this instance holds a symbolic analysis for the pattern the
    /// layout currently describes. Drives spec 4.1's compute()-vs-refactorize()
    /// decision and, with it, the section 7 note (a) verify-once discipline.
    bool analyzed_ = false;

    Ledger *ledger_ = nullptr;
    std::string label_prefix_;
    Index solve_counter_ = 0;
};

// ---------------------------------------------------------------------------
// Free functions -- the box and its domain gate, exposed so they are testable
// and re-derivable at the routing layer (task 6 declines a subproblem BEFORE
// entering the tier, and must build the same box to do it).
// ---------------------------------------------------------------------------

/// @brief Build the immutable clamp-centred box (IpqpBox's own contract).
/// @param qp     the subproblem, already validated.
/// @param radius the EFFECTIVE trust-region radius: `SolveOverrides::tr_radius`
///               resolved against `QpOptions::tr_radius`. +inf disables the
///               window exactly (`lo_eff == lower`, `up_eff == upper`).
/// @throws std::invalid_argument if `radius` is negative or NaN, or if a
///         declared bound is NaN. Checked here rather than left to Eigen's
///         asserts, which NDEBUG compiles out. A CROSSED declared box is NOT
///         a throw -- see `IpqpBox::zero_width_index`.
IpqpBox make_ipqp_box(const QpProblem &qp, double radius);

/// @brief Derive the tier's effective bounds and its domain verdict from a box.
IpqpBounds make_ipqp_bounds(const IpqpBox &box);

} // namespace hven::solvers
