// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_engine.h — the primal active-set loop for the QP
//
//     min   g^T x + 1/2 x^T H x
//     s.t.  Ae x  = be,   Ai x <= bi,   l <= x <= u
//
// H is not required to be positive semidefinite; sections 4b/4c add what an
// INDEFINITE H needs on top of the ordinary convex loop. Both are inert (and
// free) on a convex H.
//
// The numerics live below this file: the working set (working_set.h), the
// regularized bound-eliminated KKT assembly (kkt_assembly.h), the sparse
// factorization (hven::linear::SymmetricFactor via kkt_calls.h's KktFactor),
// and the equality-QP solve with iterative refinement (eqp_solve.h). This
// file is only the loop that walks between working sets.
//
// --- Loop contract ---
//
// 0. CROSSED BOUNDS. lower(i) > upper(i) + feas_tol is an empty box: verdict
//    kInfeasible immediately, x = the clamped start point, multipliers zero.
//    Checked here rather than in QpProblem::validate() because a caller may
//    legitimately hand this engine an empty box (a trust-region box
//    intersected with the real bounds) as a normal runtime outcome.
//
// 1. START POINT. Cold: x = clamp(0, l, u). Warm: clamp(seed.x, l, u) plus
//    seed.bound_state/seed.ineq_active as the initial working set. l(i) ==
//    u(i) is kFixed and never leaves the working set. A variable merely
//    sitting at a bound is NOT pinned at start -- the ratio test pins it the
//    first time it actually blocks.
//
//    General inequalities violated at the start enter via a SHIFTED-
//    CONSTRAINT HOMOTOPY rather than a phase-1 LP: shift(j) =
//    max(0, Ai_j x - bi_j), and every row with shift(j) > 0 joins the working
//    set. The EQP always solves against the TRUE rhs bi, so a shifted row's
//    shift decays with the step. Shifts are recomputed from x every step (not
//    propagated) and clamped to zero below a scale-aware tolerance
//    (refresh_shifts). A working row whose shift is still positive is EXEMPT
//    from step 2's drop rule: it is being driven to feasibility, so its
//    multiplier's sign says nothing yet.
//
// 2. EQP CANDIDATE + DROP RULE. Each iteration solves the EQP on the current
//    working set, giving x* and multipliers lambda_e/lambda_w. If p = x*-x is
//    negligible, x is a KKT point of the working set and the multipliers
//    decide: a working inequality needs lambda_i >= -opt_tol; a variable at
//    its lower bound needs z >= -opt_tol (upper: z <= opt_tol), where
//    z = (Hx + g + Ae^T lambda_e + Ai^T lambda_i)(i) (eqp_solve.h); kFixed
//    variables are unconstrained. No violation => kOptimal; otherwise the
//    MOST NEGATIVE multiplier leaves (Dantzig rule), ties broken by largest
//    angle (violation scaled by the constraint gradient's 2-norm) then lowest
//    index.
//
//    TR-PINNED STATIONARITY CAVEAT (section 6). z is priced and consulted
//    internally the same way at every index, TR-pinned or not, but the z
//    REPORTED in QpSolution is forced to 0 at a TR-pinned index, so there the
//    reported quantities do not satisfy stationarity. A kFree report at a
//    TR-pinned index means only "unconstrained by any REAL bound"; read
//    tr_active, not z or bound_state, for TR constraint status.
//
// 3. RATIO TEST. If p is not negligible, step along it: alpha = min(1,
//    min_j ratio_j), stopping at the first non-working inequality or bound
//    that blocks; that constraint joins the working set. A ratio landing at 1
//    within kEngineStepTieTol counts as blocking too.
//
//    KNOWN LABELING DIVERGENCE. That tie-break only fires for a constraint
//    the step travels toward -- a variable already sitting on its bound with
//    p(i) == 0 is never pinned (the ratio test only considers |p(i)| >
//    kEngineDenomTol), so it is reported kFree/z==0 where a dense oracle
//    reports kAtLower/kAtUpper with a zero multiplier. x, the objective and
//    the duals agree; only the active-set LABEL differs. A caller needing
//    activity by geometry rather than working-set membership must test the
//    residual itself.
//
// 4. WORKING-SET UPDATE. Under QpOptions::ws_algebra == kRefactorize, every
//    working-set change is followed by a fresh assemble_kkt() +
//    factorize_checked() (solve_eqp does both).
//
//    BORDER MODE (kSchurBorder, the DEFAULT) leaves the loop unchanged and
//    swaps only the linear algebra: one K0 (assemble_kkt_full, spanning all n
//    variables) is factorized from the seed working set, and every later
//    working-set change becomes a GMSW border over that fixed factorization
//    (border_ops.h/schur_complement.h) rather than a refactorization. K0 is
//    rebuilt -- clearing the border stack -- when
//    SchurComplement::needs_refactorization() trips or K0's own factorization
//    needed a perturbed pivot; iterations where a rebuild cannot help fall
//    back to the elimination path. See border_candidate, rebuild_k0,
//    sync_borders and latch_still_holds for the mechanism.
//
//    The two modes are OBSERVATIONALLY EQUIVALENT for a CONVEX H (same
//    status, active set, x, multipliers), with the refactorize path as the
//    oracle. They are NOT equivalent for an indefinite H: the bound-
//    eliminated K and the full-variable K0 can have different inertia, so the
//    two modes can legitimately reach different working sets and statuses.
//
//    COUNTER SEMANTICS (relied on downstream, e.g. warm-start assertions).
//    QpCounters::minor_iters increments exactly ONCE per major iteration, so
//    a run stopping at opts.max_iter reports minor_iters == max_iter. Under
//    kRefactorize, factorizations counts solve_eqp calls (one per major
//    iteration, except the empty-reduced-system short-circuit -- every
//    variable pinned, no equalities, no working rows -- which touches no
//    factorization) and schur_updates stays 0. Under kSchurBorder,
//    factorizations counts K0 factorizations plus elimination-path fallbacks
//    (a single iteration can spend two), and schur_updates counts individual
//    add_border/drop_border calls INCLUDING re-adds after a rebuild, but not
//    the rebuild's own wholesale clear. 4c's ride costs one minor_iter like
//    any other step. 4b's repair costs one EXTRA minor_iter (the kWrong
//    iteration is counted, then retried) plus, per pin/release it probes,
//    schur_updates under kSchurBorder or one factorization under
//    kRefactorize; neither is reachable on a convex H.
//    border_refine_steps/eqp_refine_steps accumulate at the same two EQP call
//    sites: every solve_bordered_eqp call adds its total kept steps (>= 1),
//    every solve_eqp call adds its extra kept steps (always 0) -- see
//    core/solver_counters.h.
//
//    HOT-START REUSE (border mode only). border_ is an ENGINE-INSTANCE
//    member, not a per-solve local, so a warm re-solve on the same QpEngine
//    can skip K0's assembly/factorization when FIVE conditions all hold at
//    the seed working set, checked once before the loop's first iteration:
//      (a)/(c) H/Ae/Ai's structural pattern AND values are byte-identical to
//          the previous trustworthy solve (detail::structural_hash /
//          detail::values_hash). K0's values depend on H/Ae/Ai and the
//          EFFECTIVE (primal_delta, dual_mu) this solve resolved to -- never
//          on g/be/bi.
//      (d) that effective (primal_delta, dual_mu) pair is identical to the
//          previous trustworthy solve's. tr_radius is NOT part of the key at
//          all -- bounds (real or TR-derived) never enter K0.
//      (b) the seed working set (start_center()/ingest_seed_working_set()
//          plus the pre-loop refresh_shifts()) equals the EXIT working set of
//          that same previous solve.
//      (e) border_'s factor's own live (session_id, epoch) identity equals
//          the pair THIS engine last saw as trustworthy, AND the factor's
//          numerics are usable (inertia().state == kObserved).
//
//    (b) IS ONLY APPROXIMATE, AND THAT IS SAFE: refresh_shifts() can add a
//    row to ws after border_candidate()'s last sync_borders() call, so
//    border_exit_active_ineq_ can understate the true exit ws. What makes
//    reuse safe is that sync_borders() is an UNCONDITIONAL, FULL
//    reconciliation of ws against border_'s ledger, run on EVERY iteration of
//    EVERY solve -- the reuse fast path skips rebuild_k0's assembly and
//    factorization, never sync_borders(). That first post-reuse
//    sync_borders() call may not be skipped on the theory that a matching
//    seed ws leaves nothing to reconcile.
//
//    All five conditions are NECESSARY but NOT SUFFICIENT for
//    `factorizations == 0`: border_candidate's own checks (a carried-over
//    perturbed-pivot count, or a border stack already past
//    needs_refactorization()) can still force a rebuild. Callers must not
//    assert `counters.factorizations == 0` unconditionally on a warm
//    re-solve; assert it only alongside control of these conditions.
//
//    INVALIDATION POLICY. border_'s cache is committed (border_valid_ set)
//    ONLY on a clean kOptimal exit, and is pessimistically cleared at the top
//    of every solve() call before anything else runs -- including before
//    qp.validate(). kMaxIter/kInfeasible/kNumericalError exits and any
//    exception thrown mid-solve therefore all leave the cache invalidated;
//    the next solve() reassembles and refactorizes from scratch.
//
//    THREAD SAFETY. QpEngine is NOT thread-safe for concurrent solve() calls
//    on the same instance: border_ and the reuse-fingerprint members are
//    mutable state shared across calls despite solve() being const from the
//    caller's view. "One QpEngine per thread" is not the whole rule either:
//    two DIFFERENT QpEngine instances, on any threads, can share one
//    BorderState object (Pardiso pt_ array included) if one adopts a hot
//    handle the other produced. SEQUENTIAL hand-off (produce a handle, then
//    feed it to a different engine after that call returns) is safe -- the
//    session/epoch identity detects a producer that mutated the object again
//    and degrades to kWarm. CONCURRENT use of a shared BorderState is
//    UNDEFINED: that identity is unsynchronized state, not atomic. A caller
//    sharing a hot handle across threads must ensure no two engines holding a
//    copy of it ever call solve() concurrently.
//
// 4b. INERTIA GATE AND TEMPORARY-VERTEX START REPAIR (indefinite H).
//
//    Every EQP solve above is a MINIMIZATION over the current working set
//    only for a convex H. For an indefinite H the regularized KKT system is
//    still nonsingular but its answer can be a saddle or a maximizer the loop
//    would certify kOptimal without noticing. The signature is the KKT
//    matrix's INERTIA: where the reduced Hessian is positive definite on the
//    null space of the working constraints, [H+delta*I A^T; A -mu*I] has
//    inertia exactly (#variables, #constraint rows, 0) -- unconditional for a
//    convex H, which is why this gate is a no-op there. The per-path
//    expectations are derived at eliminated_candidate and
//    border_inertia_verdict; the gate must read the BORDERED system's
//    inertia, never K0's numbers against a fixed expectation.
//
//    PERTURBED PIVOTS ARE NOT A PASS AND NOT A REPAIR TRIGGER. Pardiso's
//    (n_pos, n_neg) is trustworthy IFF perturbed_pivots == 0: on an exactly
//    singular matrix it fabricates a pivot sign and reports an inertia
//    indistinguishable from the nonsingular case, without raising error -4.
//    detail::InertiaVerdict has THREE verdicts:
//      kOk      trustworthy and matching -- proceed.
//      kSuspect inertia UNKNOWN -- never a pass, and never evidence that a
//               repair is needed. Handled by step 4's refactorization
//               machinery and, at a would-be-kOptimal exit, by the
//               suspect-stall gate below.
//      kWrong   trustworthy and DISAGREEING -- the working set is
//               second-order inconsistent; at solve start this triggers the
//               temporary-vertex repair (repair_temporary_vertex), which pins
//               free variables one at a time until the gate returns kOk and
//               unwinds entirely if it cannot reach kOk.
//
//    SECOND-ORDER CERTIFICATION (step 5's classification). When the loop
//    reaches a point it can neither improve nor drop from, and the gate's
//    verdict for the system just solved is a TRUSTED kWrong, the point is
//    reported kNumericalError rather than kOptimal (multipliers cleared).
//    Only kWrong triggers this; kSuspect does not.
//
//    ZERO-MULTIPLIER PROBE. The gate tests the null space of the FULL active
//    labeling, so negative curvature excluded only by a WEAKLY ACTIVE
//    constraint stays hidden. probe_zero_multiplier_drops runs at the
//    would-be-kOptimal exit and tentatively drops each such member,
//    most-recently-added first, re-running the gate; a kWrong makes the drop
//    real and RESUMES the loop. ONE-AT-A-TIME IS THE KNOWN REMAINING
//    APPROXIMATION: a critical cone that opens only when TWO OR MORE weakly
//    active constraints drop simultaneously is not covered.
//
//    SUSPECT-STALL GATE. When the verdict for the system the FINAL iterate
//    was solved from is kSuspect, kOptimal may be certified only after an
//    explicit free-block stationarity check on the QP model
//    (detail::free_block_stationarity, which is NaN-aware and applies a
//    SCALED rather than absolute opt_tol). It runs AFTER the zero-multiplier
//    probe, independently of it. On failure primal_delta is escalated one
//    decade (detail::kSuspectDeltaFactor), the border state is discarded, and
//    the iteration resumes; the ladder is bounded
//    (detail::kMaxSuspectEscalations rungs, QpCounters::suspect_escalations)
//    and reports kNumericalError when exhausted. NEVER kOptimal off a stalled
//    suspect loop, and never a hang. CAVEAT (known, unmeasured): r is built
//    from prices computed at the top of the iteration, but refresh_shifts()
//    between there and here can add rows with lambda_i == 0, which can
//    inflate the residual on a point that is in fact stationary -- a spurious
//    escalation bounded by the ladder, not a wrong answer.
//
//    POST-PROBE RESTART. A probe-driven drop leaves 4c's ride nothing to arm
//    off (its direction is identically zero), so the temporary-vertex repair
//    is spent ONCE per solve at the certification branch's trusted-kWrong arm
//    instead; see that branch for the trigger, budget and failure rule.
//
// 4c. NEGATIVE-CURVATURE RIDES AFTER A DROP (indefinite H).
//
//    Section 4b makes an indefinite START safe; this makes an indefinite DROP
//    safe. By Cauchy interlacing (adding a working row can only raise the
//    reduced Hessian's smallest eigenvalue), a drop is the ONLY way negative
//    curvature can reappear once a working set is second-order consistent.
//
//    For a KKT point of working set W with positive definite reduced Hessian
//    B = Z'HZ, releasing constraint c (lambda_c < 0) leaves W' with
//    null(A_W') = null(A_W) + span{d}, a_c.d = -1, w = Z'Hd, gamma = d'Hd,
//    and Schur complement sigma = gamma - w' B^-1 w -- by interlacing the
//    only eigenvalue of the new reduced Hessian that can be negative. The
//    next EQP's step is p = (-lambda_c/sigma) * q, q = d - Z B^-1 w, with
//    q'Hq = sigma and a_c.q = -1, so p'Hp = (lambda_c/sigma)^2 * sigma has
//    the SIGN of sigma: the curvature of the ordinary EQP step already IS the
//    test, with no extra solve. When sigma < 0, p points back into the
//    just-released constraint, the ratio test answers alpha = 0, and without
//    this section the loop cycles between W and W' until max_iter.
//
//    THE CHECK. On the iteration after a drop, and only there, the Rayleigh
//    quotient p'Hp/p'p is compared against curvature_tol =
//    kCurvatureTolFactor * hessian_scale(qp). Above it, the ordinary EQP step
//    is taken. At or below it, the loop RIDES: the admissible sign of p
//    (ride_sign), the ratio test with its unit cap DISABLED, and a step to
//    the nearest blocking constraint or bound. A negligible p is excluded
//    before the check. NO BLOCKER => kNumericalError, multipliers cleared,
//    reported immediately from inside the loop; the SQP driver's trust-region
//    bounds make that branch unreachable in SQP use.
//
//    ARMING IS ONE-SHOT: the drop record is consumed on the very next
//    iteration whether the ride fires, declines, or is never reached. A
//    DECLINED ride leaves the curvature un-retested until some later drop;
//    section 4b's certification branch is the fallback, not equivalent
//    coverage. THIS SECTION IS VACUOUS for a probe-driven drop; 4b's
//    POST-PROBE RESTART intercepts that case. 4c's no-blocker branch and 4b's
//    certification branch are MUTUALLY EXCLUSIVE by construction and report
//    the same status and semantics.
//
//    For STRICTLY CONVEX H the ride branch is UNREACHABLE (the Rayleigh
//    quotient is at least lambda_min(H) > curvature_tol whenever H's 2-norm
//    condition number is below ~1e12) and the engine is bit-for-bit
//    unchanged. For PSD-SINGULAR H (including H == 0, an LP) the branch IS
//    reachable and behavior is NOT identical to the capped path: the ride
//    takes the uncapped ratio while the ordinary path clamps at alpha = 1.
//    That is intended on the PSD-singular path.
//
//    NO ANTI-CYCLING RULE IS NEEDED HERE: a ride's objective decrease is
//    strict whenever alpha > 0, so the working set it lands in cannot be
//    revisited at the same objective. alpha == 0 is still possible at a
//    degenerate vertex, covered by the Degeneracy note below.
//
// 5. TERMINATION. The loop reaches a KKT point of its working set with
//    nothing left to drop, and classifies it:
//      - kInfeasible if any inequality or equality row carries a STRUCTURAL
//        violation (violation_is_structural). Both blocks are checked -- the
//        shift machinery only watches inequalities.
//      - kNumericalError if the point is feasible but some free component
//        unbounded on the side it grew toward exceeds
//        detail::unbounded_artifact_scale() (is_runaway) -- the answer is the
//        regularization talking, not an optimum.
//      - kNumericalError if the inertia gate's verdict for the system just
//        solved is a TRUSTED kWrong (unreachable on convex H). ONE ESCAPE:
//        4b's POST-PROBE RESTART, if unspent.
//      - otherwise 4b's ZERO-MULTIPLIER PROBE gets a veto: a trusted kWrong
//        there makes the drop real and RESUMES the loop without assigning a
//        status on this pass -- the only branch here that does not end the
//        solve.
//      - kOptimal otherwise.
//    kMaxIter once eff_max_iter major iterations have been spent -- an
//    explicit opts.max_iter, or, at its default sentinel, the size-derived
//    cap (detail::effective_qp_max_iter). A FOURTH kNumericalError exit
//    bypasses this classification entirely: 4c's ride finding no blocker
//    stops the loop mid-iteration.
//
//    ORDERING IS LOAD-BEARING: the drop rule is consulted BEFORE
//    infeasibility may be declared, because a bound pinned by the ratio test
//    commonly blocks a shift from closing, and releasing it is what lets the
//    homotopy finish.
//
//    ON kInfeasible/kNumericalError the returned x is the FINAL ITERATE the
//    loop stopped at (not the least-violating point seen; no argmin is kept),
//    and multipliers are CLEARED on both -- an inconsistent or runaway system
//    prices them at O(1/dual_mu), which are regularization artifacts, not
//    prices.
//
//    TRUSTWORTHY RANGE, both directions, both known and accepted:
//    (i) FALSE kInfeasible on a feasible but ill-scaled row whose refined
//        residual still clears kStructuralResidualFrac. Bites once |lambda|
//        exceeds roughly 1e6*row_scale, where the regularized answer is
//        itself percent-level wrong.
//    (ii) FALSE kOptimal where a genuine contradiction's gap hides beneath
//        kStructuralResidualFrac*dual_mu*|lambda|. Tightening the fraction to
//        catch it re-breaks (i). The SQP driver is the second detection
//        layer; a caller using the QP engine alone should scale its rows or
//        shrink dual_mu.
//    (iii) FALSE kNumericalError where a free variable's TRUE optimum lies
//        beyond unbounded_artifact_scale() =
//        kUnboundedArtifactFactor/primal_delta. A caller expecting very large
//        optima should tune primal_delta rather than treat this
//        kNumericalError as load-bearing.
//
// 6. TRUST-REGION SOFT BOUNDS -- an l-infinity trust region around the
//    current SQP iterate, expressed the same way every other bound is.
//
//    EFFECTIVE BOUNDS, computed ONCE about the clamped seed primal x0
//    (start_center()'s cold clamp(0,l,u) or warm clamp(seed.x,l,u)) -- BEFORE
//    the seed's bound-state hints are materialized onto x0 (step 1b; see
//    WINDOW-CONSISTENCY RULE below):
//        lo_eff(i) = max(lower(i), x0(i) - Delta),
//        up_eff(i) = min(upper(i), x0(i) + Delta),   Delta = opts.tr_radius.
//    Every subsequent bound read in this file sees lo_eff/up_eff, never
//    lower/upper directly -- via a SHADOWED QpProblem reference: `qp` inside
//    run() names either the caller's own problem unchanged (Delta == +inf) or
//    a local copy with lower/upper replaced, so every function taking
//    `const QpProblem &qp` is already TR-aware.
//
//    CROSSED EFFECTIVE BOUNDS CANNOT HAPPEN: step 0 already rejected a
//    crossed real box, so lower(i) <= x0(i) <= upper(i) gives lo_eff(i) <=
//    x0(i) <= up_eff(i) always (asserted).
//
//    THE kFixed FLIP. lo_eff(i) == up_eff(i) can only happen at Delta == 0
//    for a variable not already genuinely kFixed. Every such variable is
//    flipped to BoundState::kFixed before the loop runs, with tr_active set;
//    an already-kFixed real bound is left alone (tr_active stays false).
//
//    tr_active (size n, parallel to bound_state) IS A SEPARATE ACTIVITY SET,
//    NOT A NEW BoundState: true at i iff the ratio test, the temporary-vertex
//    repair, or the zero-radius flip pinned i at the TR side of its effective
//    bound rather than the real one (a coincidental tie is attributed to the
//    real bound). Applied once at the end of run(), on the FINAL working set
//    only:
//      (a) a TR-pinned variable's bound_state reports kFree, never
//          kAtLower/kAtUpper/kFixed;
//      (b) its z entry is forced to 0 -- the ratio test and drop_worst still
//          see and act on the real priced multiplier while the loop runs, but
//          that number is never exposed;
//      (c) none of this changes what the loop DID, only how the final ws/z
//          are reported.
//
//    WINDOW-CONSISTENCY RULE. A seeded bound-state hint is applied against
//    the window, and a hint whose bound falls outside [lo_eff(i), up_eff(i)]
//    is DROPPED -- index i arrives kFree and the loop re-derives its activity
//    from the effective bounds. Clamping such a hint to the window edge
//    instead would break bound_state's meaning (kAtLower/kAtUpper means
//    sitting on the REAL bound); tr_active is the documented channel for a
//    TR-tight edge. A hint INSIDE the window is honoured unchanged.
//
//    WARM-START SEED INGESTION IGNORES tr_active BY CONSTRUCTION:
//    ingest_seed_working_set() reads only seed.bound_state/seed.ineq_active.
//    Combined with rule (a), a variable TR-pinned on the solve that produced
//    `seed` already arrives as bound_state == kFree and is left untouched --
//    the TR pin is not carried into the new solve's working set.
//
//    HOT-START REUSE INTERACTION (border mode). K0's structural/values hashes
//    never depend on lower/upper, so a bound change (real or TR-driven) can
//    NEVER poison K0 reuse on fingerprint grounds: a pin's border column is
//    e_i (BorderOps::pin_variable), independent of the bound value, and only
//    the border's RHS entry carries that value, rebuilt from the CURRENT `qp`
//    on every solve_bordered_eqp call. What a bound change CAN do is change
//    ws.bound_state() at the seed, which reuse condition (b) already treats
//    as an ordinary working-set change.
//
//    PER-SOLVE RADIUS VARIATION. tr_radius lives on QpOptions, which is
//    per-instance const, but every solve() overload also takes a
//    `const SolveOverrides &` (qp_types.h) resolved ONCE at the top of run()
//    into effective tr_radius/primal_delta/dual_mu (a sentinel field resolves
//    to the corresponding opts_ value); every read site below consults those
//    effective values, not opts_ directly. A shrink-radius retry loop
//    therefore shares one QpEngine across every retry radius and keeps
//    hot-start reuse wherever the ordinary eligibility conditions allow.
//
//    UNBOUNDED-ARTIFACT GUARD AND REPAIR SYNERGY. is_runaway() and
//    repair_temporary_vertex() both read bounds through the same shadowed
//    `qp`, so with a finite tr_radius every variable has a finite effective
//    bound on both sides: is_runaway() can never fire for a TR-bounded
//    variable, and repair_temporary_vertex()'s "no finite bound to pin"
//    failure mode cannot occur.
//
//    BIT-IDENTICAL OFF PATH. opts.tr_radius defaults to +inf; at that value
//    no QpProblem copy and no effective-bounds vector is ever materialized,
//    and the shadowed `qp` aliases the caller's problem directly (the "zero
//    new work" claim is scoped to exactly those two allocations).
//    QpSolution::tr_active is still allocated unconditionally on every solve
//    (all false) -- part of QpSolution's contract.
//
// 6b. THE EXPORT INVARIANT ON z: A FREE VARIABLE CARRIES NO BOUND PRICE.
//    For every index i, on EVERY status this engine can return:
//        bound_state[i] == kFree  =>  z(i) == 0.0
//    Enforced at the point of export in run(). `refine_on_face` -- the
//    engine's other public QpSolution producer -- satisfies the same
//    invariant by a DIFFERENT route (price()'s own postcondition plus its own
//    TR-exclusion pass); a third producer must re-derive the invariant rather
//    than assume it is inherited.
//
//    ONE-WAY GUARANTEE: a PINNED index's z is the price the last price() call
//    computed, which on a kMaxIter exit may be one working set stale -- a
//    caller needing fresh prices needs a converged solve. `kFixed`
//    (lower == upper) is silently outside this invariant.
//
// --- Degeneracy ---
//
// Linearly dependent working sets (e.g. duplicated inequality rows) do not
// break the loop: the delta/mu regularization in assemble_kkt keeps the KKT
// matrix factorizable, and the dependent rows simply split the multiplier
// between them. No anti-cycling rule (Bland/least-index) is implemented;
// a degenerate stall is bounded by max_iter.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <hven/core/ledger.h>
#include <hven/core/pattern_hash.h>
#include <hven/detail/kkt/border_ops.h>
#include <hven/detail/kkt/bordered_eqp.h>
#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/kkt/schur_complement.h>
#include <hven/detail/qp/eqp_solve.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/working_set.h>
#include <hven/linear/symmetric_factor.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

namespace detail {

// Bounds at or beyond this magnitude are treated as absent (matches the
// dense oracle's convention).
constexpr double kEngineInfBound = 1e20;

// THE SIZE-DERIVED QP ITERATION CAP. QpOptions::max_iter (qp_types.h)
// defaults to the sentinel 0, meaning "derive the cap from this subproblem's
// size"; a positive value is an explicit absolute cap and wins outright.
//
//     base  =  n + mi + #bounded          (qp_cap_base)
//     cap   =  max(kQpMaxIterFloor, kQpMaxIterCoeff * base)
//
// #bounded COUNTS *REAL* BOUNDS, NOT EFFECTIVE ONES -- load-bearing: the SQP
// driver passes a FINITE trust-region radius on every subproblem, so counting
// the EFFECTIVE box would make #bounded == n unconditionally. qp_cap_base
// reads the CALLER'S OWN qp.lower/qp.upper, before section 6's window.
constexpr Index kQpMaxIterFloor = 500;
constexpr Index kQpMaxIterCoeff = 5;

// n + mi + #bounded, over the CALLER'S problem (real bounds, pre-trust-region
// -- see the note above).
inline Index qp_cap_base(const QpProblem &qp) {
    const Index n = qp.n();
    Index bounded = 0;
    for (Index i = 0; i < n; ++i) {
        if (qp.lower(i) > -kEngineInfBound || qp.upper(i) < kEngineInfBound) {
            ++bounded;
        }
    }
    return n + qp.mi() + bounded;
}

inline Index derived_qp_max_iter(Index base) {
    return std::max(kQpMaxIterFloor, kQpMaxIterCoeff * base);
}

// The cap this solve actually runs at: an explicitly set (positive)
// QpOptions::max_iter wins outright; the sentinel (<= 0, the default) derives
// from size.
inline Index effective_qp_max_iter(const QpProblem &qp, Index requested) {
    return requested > 0 ? requested : derived_qp_max_iter(qp_cap_base(qp));
}
// Direction components below this are not considered to move a constraint.
constexpr double kEngineDenomTol = 1e-12;
// A ratio within this of 1 counts as blocking at the full step; see step 3.
constexpr double kEngineStepTieTol = 1e-9;
// Relative window inside which two drop candidates' violations are a tie.
constexpr double kEngineDropTieTol = 1e-12;
// Hard cap (relative to the row's scale) on how much constraint violation the
// regularization allowance in row_tolerance() may absorb. On an INCONSISTENT
// row the regularized solve satisfies Ai x - dual_mu*lambda = bi by
// construction, so the residual IS dual_mu*|lambda| exactly and lambda grows
// without bound as the contradiction sharpens; uncapped, the allowance would
// always cover the violation and kInfeasible would be unreachable.
constexpr double kInfeasibilityAbsorbTol = 1e-6;
// Runaway threshold for the unbounded-artifact guard, as a fraction of
// 1/primal_delta. When a QP is unbounded below in some direction, the only
// thing stopping the regularized solve is the primal_delta ridge:
// stationarity degenerates to delta*x = -g and the iterate runs off to
// ~|g|/primal_delta -- which is why the threshold is relative to
// 1/primal_delta rather than an absolute size. 0.1/primal_delta is 1e7 at the
// defaults, an order of magnitude beneath the observed ~2e8 artifact while
// still admitting genuinely large physical-scale solutions.
constexpr double kUnboundedArtifactFactor = 0.1;
inline double unbounded_artifact_scale(const QpOptions &opts) {
    return kUnboundedArtifactFactor / opts.primal_delta;
}
// Multiple of a row's tolerance a violation must clear before it is even
// considered as evidence of infeasibility (condition (a); see
// violation_is_structural).
constexpr double kInfeasibilityMarginFactor = 10.0;
// Fraction of the regularization footprint dual_mu*|lambda| that a violation
// must reach before it counts as STRUCTURAL rather than solver noise
// (condition (b); see violation_is_structural).
constexpr double kStructuralResidualFrac = 0.1;

// --- Inertia gate (see the header contract's section 4b) ---

// What one factorization's reported inertia says about the system that was
// factorized. Three verdicts, not two: "wrong" and "unknowable" demand
// different responses.
enum class InertiaVerdict {
    kOk,      // trustworthy AND equal to the expectation
    kSuspect, // untrustworthy: perturbed pivots, or counts that do not sum to n
    kWrong,   // trustworthy AND different from the expectation
};

// Gate one factorization's InertiaEvidence against an expected (positive,
// negative) eigenvalue count. `expected_pos + expected_neg` must be the
// factorized matrix's dimension -- an expectation of zero zero-eigenvalues is
// part of what is being asserted. Call sites pass `kkt.factor.inertia()`.
//
// Rule order: non-kObserved evidence -> kSuspect; present-and-nonzero
// perturbed pivots -> kSuspect (a perturbed factorization reports an inertia
// that looks exactly like a genuine one, and pardiso raises no error for that
// matrix; absent evidence, as on Accelerate, does not trigger the rule); a
// short sum -> kSuspect (a zero eigenvalue means the factorization did not see
// the matrix this expectation describes); exact match -> kOk, else kWrong.
inline InertiaVerdict inertia_verdict(const hven::linear::InertiaEvidence &e, Index expected_pos,
                                      Index expected_neg) {
    if (e.state != hven::linear::InertiaEvidence::State::kObserved) {
        return InertiaVerdict::kSuspect;
    }
    if (e.perturbed_pivots.has_value() && *e.perturbed_pivots != 0) {
        return InertiaVerdict::kSuspect;
    }
    const Index pos = static_cast<Index>(e.n_pos);
    const Index neg = static_cast<Index>(e.n_neg);
    if (pos + neg != expected_pos + expected_neg) {
        return InertiaVerdict::kSuspect;
    }
    return (pos == expected_pos && neg == expected_neg) ? InertiaVerdict::kOk
                                                        : InertiaVerdict::kWrong;
}

// --- Suspect-stall escalation ladder (section 4b's SUSPECT-STALL GATE) ---
//
// What a would-be-kOptimal exit off a kSuspect factorization does when the
// free-block stationarity check fails: multiply primal_delta by
// kSuspectDeltaFactor and resume, at most kMaxSuspectEscalations times, then
// report kNumericalError. ONE DECADE PER RUNG, matching the driver's elastic
// penalty ladder (kElasticRhoFactor in globalization/sqp/elastic.h); no exact
// Hessian/primal_delta cancellation survives a 10x change in delta. THREE
// RUNGS, i.e. delta 1e-8 -> 1e-5 at the defaults. Per-SOLVE.
constexpr double kSuspectDeltaFactor = 10.0;
constexpr Index kMaxSuspectEscalations = 3;

// Section 4b's SUSPECT-STALL GATE, invariant half (ii). The QP MODEL's
// stationarity residual r = H x + g + Ae^T lambda_e + Ai^T lambda_i,
// restricted to the FREE variables (a pinned variable's r(i) IS the bound
// multiplier z(i), already judged by the drop rule's sign test, while a free
// variable's z(i) is zero by construction, so r(i) there is the whole
// first-order condition). Returns the inf-norm over the free block and
// reports, through `scale`, the largest term that went into r.
//
// THE THRESHOLD IS SCALED, unlike this file's other (absolute) opt_tol tests:
// this one reads a GRADIENT RESIDUAL, which scales with the objective, so an
// absolute test would refuse every suspect exit on a large-objective problem.
// scale >= 1 always, so it errs toward false REFUSALS (a bounded, recoverable
// kNumericalError) rather than a false certificate.
//
// NaN IS A FAILURE, NOT A PASS: the accumulation below is NaN-STICKY, and the
// VERDICT is taken by free_block_is_stationary. Deliberately NOT a
// general-purpose KKT checker: primal feasibility and multiplier signs are
// judged by the classification branch that calls it.
inline double free_block_stationarity(const QpProblem &qp, const Vec &x, const WorkingSet &ws,
                                      const Vec &lambda_e, const Vec &lambda_i, double &scale) {
    const Vec hx = qp.H.selfadjointView<Eigen::Upper>() * x;
    Vec r = hx + qp.g;
    scale = std::max({1.0, hx.lpNorm<Eigen::Infinity>(), qp.g.lpNorm<Eigen::Infinity>()});
    if (qp.me() > 0) {
        const Vec ae_term = qp.Ae.transpose() * lambda_e;
        r += ae_term;
        scale = std::max(scale, ae_term.lpNorm<Eigen::Infinity>());
    }
    if (qp.mi() > 0) {
        const Vec ai_term = qp.Ai.transpose() * lambda_i;
        r += ai_term;
        scale = std::max(scale, ai_term.lpNorm<Eigen::Infinity>());
    }
    double worst = 0.0;
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
            continue;
        }
        const double ri = std::abs(r(i));
        // NaN IS STICKY, and it takes an explicit test to make it so.
        // BOTH one-liners are wrong here, in opposite directions:
        //   std::max(worst, ri)  DROPS a NaN ri and returns `worst`;
        //   !(ri <= worst)       lets a NaN worst be OVERWRITTEN by the next
        //                        finite component.
        // Returning immediately is the only form under which a single NaN
        // anywhere in the free block decides the verdict.
        if (std::isnan(ri)) {
            return ri;
        }
        if (ri > worst) {
            worst = ri;
        }
    }
    return worst;
}

// The suspect-stall gate's DECISION, factored out of run() so the comparison
// and its test cannot drift apart. `residual`/`scale` are
// free_block_stationarity's two outputs.
//
// WRITTEN AS `<=`, NOT `!(>)`: every comparison against NaN is false, so
// `residual <= tol` is FALSE for a NaN residual and the caller's `!stationary`
// routes it to the escalation ladder. `residual > tol` would instead certify
// kOptimal on a NaN.
inline bool free_block_is_stationary(double residual, double scale, const QpOptions &opts) {
    return residual <= opts.opt_tol * scale;
}

// --- Negative-curvature ride (see the header contract's section 4c) ---

// Relative threshold below which a direction's curvature counts as NOT
// positive. Applied to the RAYLEIGH QUOTIENT p'Hp / p'p, which lives on H's
// own eigenvalue scale -- so the threshold must be relative to that scale
// too, or it would classify H and 1000*H differently.
constexpr double kCurvatureTolFactor = 1e-12;

// Relative tolerance on "the ride direction lies in the null space of the
// working constraints" (ride_stays_in_working_set), stated relative to
// ||a_j|| * ||p||.
//
// 1e-8 rather than something near machine epsilon: the EQP is solved through a
// dual_mu-regularized KKT system, so even a consistent working row carries an
// irreducible dual_mu*|lambda| residual, while the cases this must REJECT miss
// by orders of magnitude more. This does NOT bound the residual at the LANDING
// point -- the test is direction-relative and the ride travels alpha along p,
// so the drift admitted is amplified by ~alpha; row_tolerance, not this
// constant, governs a residual-driven kInfeasible after a ride.
constexpr double kRideNullspaceTol = 1e-8;

// The H-scale kCurvatureTolFactor multiplies: the largest magnitude of any
// stored entry of H, floored at 1. max|H_ij| brackets ||H||_2 within a factor
// of n, costs one pass over the stored nonzeros instead of an eigensolve, and
// is exactly homogeneous in H, so scaling the objective leaves every ride
// decision unchanged.
//
// FLOORED AT 1 so a pure-LP H (entirely zero) still yields a usable (plain
// absolute, 1e-12) tolerance rather than 0: there the curvature is exactly 0,
// the ride branch is taken, and that is CORRECT.
inline double hessian_scale(const QpProblem &qp) {
    double s = 0.0;
    for (Index i = 0; i < qp.n(); ++i) {
        for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
            s = std::max(s, std::abs(it.value()));
        }
    }
    return std::max(s, 1.0);
}

// --- Hot-start reuse fingerprints (see the header contract's HOT-START
// REUSE note) ---
//
// Both hashes are computed directly from the QpProblem's raw H/Ae/Ai
// matrices, never from an assembled K0, so the reuse check runs without
// paying for assembly. Neither ever touches g, be, or bi.

// FNV-1a mixing step (same FNV-1a family hven::pattern_hash uses). Feeds
// only `values_hash` below: the STRUCTURAL fingerprint is hven's combined
// pattern key and no longer mixes raw bytes.
inline void fnv1a_mix(std::uint64_t &h, const void *data, std::size_t len) {
    constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i < len; ++i) {
        h ^= bytes[i];
        h *= kFnvPrime;
    }
}

// Mixes rows/cols/nnz as a separator BEFORE the value bytes: two matrices of
// different SHAPE can produce byte-identical value streams (mi=2 rows
// [1,0],[-1,0] vs mi=1 row [1,-1]). structural_hash is the primary guard
// against that class and is always checked alongside this one.
//
// A caller-supplied QpProblem matrix is not guaranteed to be COMPRESSED, so
// this hasher pays for a compressed copy only when the input needs one.
inline void mix_values(std::uint64_t &h, const SpMatRM &m_in) {
    SpMatRM tmp;
    const SpMatRM *mp = &m_in;
    if (!m_in.isCompressed()) {
        tmp = m_in;
        tmp.makeCompressed();
        mp = &tmp;
    }
    const SpMatRM &m = *mp;
    const Index rows = m.rows();
    const Index cols = m.cols();
    const Index nnz = m.nonZeros();
    fnv1a_mix(h, &rows, sizeof(rows));
    fnv1a_mix(h, &cols, sizeof(cols));
    fnv1a_mix(h, &nnz, sizeof(nnz));
    fnv1a_mix(h, m.valuePtr(), sizeof(double) * static_cast<std::size_t>(nnz));
}

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;

// Condition (a): does K0's SPARSITY (never mind its values) still match what
// it was built from? K0's structure is fully determined by n, me, mi, and
// H/Ae/Ai's own nonzero patterns, so hashing the three raw patterns is
// equivalent to hashing an assembled K0's pattern -- without assembling one.
//
// The digest is hven's combined pattern key (feed_pattern,
// docs/pattern-hash.md), not a raw-byte FNV over the index arrays: it does
// not depend on host byte order or on SpMatRM::StorageIndex's width. No
// consumer compares it against anything but another call of this same
// function in the same process; 0 stays meaningful only as
// WarmStart::structure_hash's "no claim made" sentinel.
inline std::uint64_t structural_hash(const QpProblem &qp) {
    return combined_pattern_hash(qp.H, qp.Ae, qp.Ai);
}

// Condition (c): have H/Ae/Ai's VALUES changed?
inline std::uint64_t values_hash(const QpProblem &qp) {
    std::uint64_t h = kFnvOffsetBasis;
    mix_values(h, qp.H);
    mix_values(h, qp.Ae);
    mix_values(h, qp.Ai);
    return h;
}

} // namespace detail

// Border mode's entire persistent state (unused under kRefactorize): the one
// full-variable K0 the loop keeps factorized, the working rows built INTO it,
// and the live border stack that carries it from that working set to the
// current one. An empty `schur` means "K0 has not been built yet".
//
// An ENGINE-INSTANCE member (border_ below), not a per-solve local -- that is
// what lets a warm re-solve on the same QpEngine reuse K0's factorization
// outright. See the header contract's HOT-START REUSE and INVALIDATION POLICY.
//
// K0 gets its OWN KktFactor, separate from the elimination path's, so a
// fallback to solve_eqp does not destroy K0's factorization and silently
// invalidate the border stack's cached K0^-1 v columns.
//
// Copy and move are DELETED: `schur` holds a reference to the `kkt` member, so
// a moved BorderState would leave the SchurComplement pointing at the corpse.
// QpEngine therefore holds this through a std::shared_ptr -- see HotState's
// OWNERSHIP note.
//
// `latched` marks the pins-only dead end (border_candidate): bordering is
// abandoned for the current working-set SHAPE, the elimination path serves
// every iteration, and the border stack is deliberately NOT kept in sync.
struct BorderState {
    BorderState() = default;
    BorderState(const BorderState &) = delete;
    BorderState &operator=(const BorderState &) = delete;
    BorderState(BorderState &&) = delete;
    BorderState &operator=(BorderState &&) = delete;

    KktAssembly k0;
    std::vector<Index> k0_rows;
    std::vector<BorderLedgerEntry> ledger; // in SchurComplement::add_border order
    detail::KktFactor kkt;                 // configured by sqp_kkt_options()
    std::optional<SchurComplement> schur;
    bool latched = false;

    // Stale-handle detection lives in the backend factor's own identity:
    // `analyze()` moves `kkt.factor.session_id()` and every successful
    // `factorize()` advances `kkt.factor.epoch()`, so rebuild_k0() stamps
    // nothing. See QpEngine::run()'s reuse condition (e).
};

// HOT-START LEVEL. The opaque handle behind warm_start.h's WarmStart::hot --
// forward-declared there and DEFINED here. A frozen copy of the
// fingerprint/exit-state members QpEngine::run() tracks per instance, plus
// shared ownership of the BorderState those fingerprints describe (the K0
// symbolic analysis and last numeric factorization, Pardiso pt handle
// included).
//
// OWNERSHIP. BorderState wraps a move-only SymmetricFactor by value and is
// itself non-copyable and non-movable. `border_` is a
// std::shared_ptr<BorderState>; HotState::border is a COPY of that same
// shared_ptr. The backend session is released exactly once, when the LAST
// shared_ptr is destroyed, so an engine adopting a hot handle can
// factorize/solve against it safely even if the producing engine is gone.
//
// LIFETIME SAFETY ALONE DOES NOT GUARANTEE THE CONTENTS STILL MATCH A
// HOLDER'S FROZEN FINGERPRINT: a producer that solves again after emitting a
// handle can mutate the SAME shared BorderState. TWO MECHANISMS close this:
//   - DETACH (`border_ = std::make_shared<BorderState>()` at a refused-reuse
//     site whose `border_.use_count() > 1`; a sole-owned object is wiped in
//     place instead, to keep its KktFactor's symbolic analysis reusable) is
//     the LOAD-BEARING fix: the only engine that can ever write into a SHARED
//     BorderState is one whose own conditions (a)-(e) already passed for it,
//     so every K0 written into a shared object already carries the values any
//     handle's fingerprint describes, and sync_borders()'s unconditional
//     reconciliation absorbs the rest.
//   - The factor's IDENTITY pair (session_id/epoch) plus this engine's own
//     committed copy is DEFENSE-IN-DEPTH: reuse condition (e) compares a
//     HotState's frozen pair against a LIVE read off the shared object on
//     every solve() call, and its usable-numerics conjunct closes the
//     failed-rebuild case (no epoch advance). A mismatch degrades to kWarm
//     silently, never a throw.
//
// CONCURRENCY, NOT SEQUENCING, IS WHAT REMAINS UNSAFE -- see the header
// contract's THREAD SAFETY note. SAME-PROCESS ONLY (per warm_start.h):
// HotState is never serialized.
//
// AFTER AN ELASTIC/SOC RE-SOLVE: an ELASTIC re-solve builds an AUGMENTED
// (original-plus-slack) K0, so a handle emitted right after it silently
// forfeits kHot for the next major (safe, degraded to kWarm); an SOC re-solve
// shifts only be/bi, so a post-SOC handle remains an ordinary (a)-(e)-gated
// reuse candidate.
struct HotState {
    std::shared_ptr<BorderState> border;
    std::uint64_t structural_hash = 0;
    std::uint64_t values_hash = 0;
    double effective_delta = 0.0;
    double effective_mu = 0.0;
    std::vector<BoundState> exit_bound_state;
    std::vector<Index> exit_active_ineq;
    // The COMMITTED (session_id, epoch) identity of `border`'s factor at
    // emission -- hot_state() emits this engine's own last-committed pair,
    // never a live re-read off the possibly-shared object. Together with
    // `structural_hash` these form hven's (pattern_hash, session_id, epoch)
    // naming triple. Reuse condition (e) -- see QpEngine::run().
    std::uint64_t kkt_session_id = 0;
    std::uint64_t kkt_epoch = 0;
};

class QpEngine {
  public:
    explicit QpEngine(const QpOptions &opts)
        : opts_(opts), border_(std::make_shared<BorderState>()) {}

    // Attach a ledger for instrumentation (nullptr = off, default off).
    // Emits one SolveRecord per solve() call with the given label prefix
    // and a per-solve counter appended. If solve() throws (e.g., from
    // qp.validate() or KKT factorization failure), no record is emitted
    // and solve_counter_ is not incremented, so the next successful solve
    // gets the same label it would have received if the failed solve had not
    // been attempted.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

    // Cold start from clamp(0, l, u) with an empty working set. Forwards a
    // default-constructed SolveOverrides, which resolves to every opts_
    // value unchanged -- see qp_types.h's SolveOverrides and the header
    // contract's PER-SOLVE RADIUS VARIATION note.
    QpSolution solve(const QpProblem &qp) const;

    // Warm start from `seed`'s point and working set. Same
    // default-overrides forwarding as above.
    QpSolution solve(const QpProblem &qp, const QpSolution &seed) const;

    // Cold start with a per-solve override of tr_radius/primal_delta/
    // dual_mu (see qp_types.h's SolveOverrides). Resolved once, at the top of
    // run(), into the effective values every read site below actually
    // consults.
    QpSolution solve(const QpProblem &qp, const SolveOverrides &overrides) const;

    // Warm start with a per-solve override, combining both of the above.
    QpSolution solve(const QpProblem &qp, const QpSolution &seed,
                     const SolveOverrides &overrides) const;

    // Warm start with a per-solve override AND a hot handle
    // (WarmStart::hot) from a PRIOR solve -- typically on a DIFFERENT
    // QpEngine instance -- offered for THIS engine to adopt as its own
    // border-mode cache if it does not already have a valid one (see run()'s
    // ADOPT AN EXTERNAL HOT HANDLE step). `hot` may be null (falls back to
    // the 3-arg overload exactly) or stale/foreign: the engine's own
    // reuse-eligibility conditions (a)-(e) are the ONLY gate on whether
    // adopting it skips a factorization, and a mismatch silently costs the
    // ordinary rebuild.
    QpSolution solve(const QpProblem &qp, const QpSolution &seed, const SolveOverrides &overrides,
                     const std::shared_ptr<const HotState> &hot) const;

    // A shared, opaque snapshot of this engine's CURRENTLY valid border-mode
    // cache -- what warm_start.h's make_warm_start attaches to
    // WarmStart::hot on every exit. Returns nullptr whenever border_valid_ is
    // false: no solve() on this instance has yet ended kOptimal, or
    // ws_algebra == kRefactorize.
    //
    // Emits this engine's own COMMITTED (session_id, epoch) pair, NEVER a LIVE
    // read off the possibly-shared object: the live pair would mint a
    // SELF-CONSISTENT FORGED HANDLE whose fingerprints describe this engine's
    // last problem but whose identity matches the object's current,
    // foreign-mutated state, so a later adopter's condition (e) would pass on
    // an object this engine never certified.
    std::shared_ptr<const HotState> hot_state() const;

    // TIER 3: EXACT REFINEMENT ON AN EXTERNALLY IDENTIFIED FACE.
    //
    // ONE exact equality-constrained solve on the face `face` names, plus this
    // engine's ordinary iterative-refinement step -- i.e. `solve_eqp`, the
    // same function the walk's per-minor `eqp_candidate` calls, reached here
    // WITHOUT a walk. A kernel that IDENTIFIES an active set to its own
    // tolerance (today: the semismooth-Newton tier, ssn_engine.h) hands that
    // set here and gets back the point the set determines EXACTLY -- an
    // ACTIVE-SET solve's complementarity is an EXACT identity, where an FB
    // kernel stopping at |phi| <= fb_tol bounds its own only by
    // `fb_tol * ||lambda||inf`.
    //
    // GATED: the refined point must be a legal answer to the SUBPROBLEM --
    //   finite, inside the real box, inside the trust region, and satisfying
    //   every inequality row NOT on the face to the row-scaled feasibility
    //   tolerance. On failure this function REFUSES and the caller keeps the
    //   certificate it already had.
    // NOT GATED: the sign of the refined multipliers. The face is the
    //   CALLER's; this function re-solves it exactly and does not re-judge it,
    //   exactly as `eqp_candidate` does not.
    //
    // THE RANK PRE-SCREEN, before anything is factorized: a face with more
    // equality rows (model equalities + working inequalities) than free
    // variables cannot be a regular face, and handing its singular K to the
    // backend would trade a usable answer for a thrown Pardiso error.
    // Numerical singularity is caught one step later by the same
    // `detail::inertia_verdict` gate the walk applies.
    //
    // COST AND STATE. `out.counters` reports ONLY what THIS call paid (at most
    // one factorization). NOTHING PERSISTENT IS TOUCHED: no `border_`, no
    // hash, no `border_valid_`, no ledger record, no `solve_counter_`.
    //
    // Returns true iff the refinement was ACCEPTED. `out` is written either
    // way: on refusal it is `face` verbatim (with this call's own cost), so a
    // caller may use it unconditionally.
    //
    // PUBLIC-API PRECONDITION: the trust-region gate below assumes its window
    // is centred at `clamp(0, l, u)` -- true today only because every seeding
    // site zeroes `seed.x` before it reaches this function. This function
    // takes no centre parameter and does NOT validate that assumption. A
    // caller that seeds from a non-zeroed point gets a window gated about the
    // wrong centre, silently.
    bool refine_on_face(const QpProblem &qp_in, const QpSolution &face,
                        const SolveOverrides &overrides, QpSolution &out) const;

  private:
    /// @brief Resolve one call's SolveOverrides against `opts` into the
    ///     effective tr_radius/primal_delta/dual_mu values every read site in
    ///     this file consults from that point on. Shared with run() so
    ///     refine_on_face() cannot resolve them differently.
    /// @param opts the engine's QpOptions; every other field (feas_tol,
    ///     opt_tol, max_iter, schur_cap, schur_cond_max, ws_algebra) is
    ///     carried through unchanged.
    /// @param overrides per-solve overrides (qp_types.h's SolveOverrides
    ///     sentinel convention: tr_radius disables at +inf; primal_delta/
    ///     dual_mu are overridden only by a value >= 0, and 0 is what
    ///     disables the regularization -- a negative override means "use
    ///     the stored field" instead).
    /// @return effective QpOptions, safe to pass anywhere a whole QpOptions
    ///     is expected.
    /// @throws std::invalid_argument if overrides.tr_radius is NaN or
    ///     negative; or, at the +inf sentinel, if the stored opts.tr_radius
    ///     is NaN or negative. Also thrown if overrides.primal_delta or
    ///     overrides.dual_mu is NaN; or, at either field's negative
    ///     sentinel, if the corresponding stored opts field is NaN or
    ///     negative. The domain enforced throughout is NOT NaN and >= 0.
    static QpOptions resolve_effective_options(const QpOptions &opts,
                                               const SolveOverrides &overrides);

    // What the ratio test found blocking, or kNone.
    enum class BlockKind { kNone, kIneq, kBound };

    // What the drop rule last released. Section 4c's ride needs it to choose
    // the ride's SIGN; `active` is what ARMS the curvature check -- true for
    // exactly the one iteration that follows a drop. `from` is meaningful for
    // bound drops only.
    struct DropRecord {
        bool active = false;
        bool is_ineq = false;
        Index idx = -1;
        BoundState from = BoundState::kFree;
    };

    // What one attempt at section 4c's ride did.
    enum class RideOutcome {
        kStepped,   // moved to a blocker, which joined the working set
        kUnbounded, // nothing blocks: the QP is unbounded along the ride
        kDeclined,  // the direction is not ridable: see ride_stays_in_working_set
                    // and ride_sign. The caller falls back to the ordinary step.
    };

    // Per-solve bookkeeping for section 4b's ZERO-MULTIPLIER PROBE: the order
    // in which the working set's current members joined it (the probe tries
    // the most recently added first), and the anti-cycling exemption set.
    //
    // ADDITION ORDER IS KEPT BY RESCAN, not by instrumenting the mutation
    // sites: WorkingSet stores active_ineq_ sorted and bound_state_ by index,
    // so neither carries insertion order, and `ws` is mutated from six places.
    // refresh() stamps a monotone sequence number on every member that was not
    // there last time, at one O(n + mi) pass per major iteration, and cannot
    // go stale when a seventh mutation site appears. Entries that leave lose
    // their number; members appearing together are numbered in index order; a
    // bound that merely switches SIDE keeps its number.
    //
    // THE EXEMPTION SETS bound the probe: a constraint whose tentative drop
    // was made real is exempt for the rest of the solve, so at most n + mi
    // probe-driven DROPS happen per solve (not a bound on the number of
    // PROBES). Keyed by constraint identity -- for a bound, the VARIABLE and
    // not the side it was pinned at; see probe_zero_multiplier_drops.
    struct ProbeState {
        std::vector<Index> bound_seq; // per variable, -1 == not in the working set
        std::vector<Index> ineq_seq;  // per Ai row,   -1 == not in the working set
        std::vector<bool> bound_exempt;
        std::vector<bool> ineq_exempt;
        Index next = 0;

        ProbeState(Index n, Index mi)
            : bound_seq(static_cast<std::size_t>(n), -1),
              ineq_seq(static_cast<std::size_t>(mi), -1),
              bound_exempt(static_cast<std::size_t>(n), false),
              ineq_exempt(static_cast<std::size_t>(mi), false) {}

        // Numbers bounds before rows within a single call. That tie-break is
        // arbitrary but deterministic, and it is only ever exercised when one
        // iteration adds several members at once (refresh_shifts can).
        void refresh(const WorkingSet &ws) {
            const std::vector<BoundState> &bs = ws.bound_state();
            for (std::size_t i = 0; i < bs.size(); ++i) {
                if (bs[i] == BoundState::kFree) {
                    bound_seq[i] = -1;
                } else if (bound_seq[i] < 0) {
                    bound_seq[i] = next++;
                }
            }
            // Linear merge against the SORTED active_ineq list: a row still in
            // it keeps whatever number it had, a row that left loses its.
            const std::vector<Index> &aw = ws.active_ineq();
            std::size_t k = 0;
            for (std::size_t j = 0; j < ineq_seq.size(); ++j) {
                const bool live = k < aw.size() && aw[k] == static_cast<Index>(j);
                if (!live) {
                    ineq_seq[j] = -1;
                    continue;
                }
                ++k;
                if (ineq_seq[j] < 0) {
                    ineq_seq[j] = next++;
                }
            }
        }
    };

    QpSolution run(const QpProblem &qp_in, const QpSolution *seed, bool warm,
                   const SolveOverrides &overrides,
                   const std::shared_ptr<const HotState> &hot = nullptr) const;

    // Step 1a: the START CENTER -- the clamped seed primal, plus kFixed
    // detection. NO seeded bound-state hint is materialized here; that is
    // step 1b (ingest_seed_working_set), and the SPLIT IS LOAD-BEARING:
    // section 6 computes the trust-region window about this x, so a seeded
    // kAtLower/kAtUpper hint applied first would overwrite x with its BOUND
    // and make that bound the window's own center, letting the returned step
    // violate the radius. A caller cannot defend against that by zeroing
    // seed.x, because the zero is exactly what the hint overwrites.
    static Vec start_center(const QpProblem &qp, const QpSolution *seed, WorkingSet &ws);

    // Step 1b: ingest the seed's WORKING SET -- bound-state hints and active
    // inequality rows -- against the EFFECTIVE bounds, i.e. after section 6
    // has computed the trust-region window about start_center's x.
    //
    // THE WINDOW-CONSISTENCY RULE. A seeded bound hint is honoured only if the
    // bound it names lies INSIDE [qp_eff.lower(i), qp_eff.upper(i)]. A hint
    // outside is DROPPED: index i arrives kFree and the loop re-derives its
    // activity like any other variable. Honouring it instead would place x
    // outside the very window this solve was asked to respect. A hint INSIDE
    // the window is honoured exactly as before -- the common warm-chain case,
    // since a carried seed.x centers the window on itself.
    //
    // The pin materializes the REAL bound value (qp_real), not the effective
    // one -- the same number whenever the test above passes.
    static void ingest_seed_working_set(const QpProblem &qp_real, const QpProblem &qp_eff,
                                        const QpSolution *seed, WorkingSet &ws, Vec &x);

    // Recompute Ai*x and the homotopy shifts from the CURRENT x, clamping
    // negligible violations to zero, and make sure every still-shifted row is
    // in the working set (that is what drives its shift down).
    //
    // The clamp is scale-aware on purpose: a bare absolute feas_tol makes a
    // converged solve look like a live homotopy shift and so reports
    // kInfeasible on a feasible problem. Ai_j . x is a sum of terms of size
    // ~ ||Ai_j||_1 * ||x||_inf cancelling to ~0 on an active row, and
    // assemble_kkt puts -dual_mu on each constraint row's diagonal, so a
    // working row's residual has an irreducible dual_mu * |lam_j| footprint
    // (rows outside the working set carry lam_j == 0 and get no allowance).
    // The allowance is CAPPED (kInfeasibilityAbsorbTol) so it cannot absorb a
    // genuine contradiction -- see row_tolerance().
    // `opts` is the EFFECTIVE options this solve resolved (run()'s
    // `eff_opts`), since dual_mu can vary per solve via SolveOverrides.

    // OBSERVATION ONLY. Which constraints this solve has EVER admitted, so
    // QpCounters::distinct_{ineq,bound}_added can report re-discovery
    // directly. Written once per FIRST admission and read by nothing.

    // OBSERVATION ONLY. Which constraints this solve has EVER admitted, so
    // QpCounters::distinct_{ineq,bound}_added can report re-discovery
    // directly. Two byte vectors, written once per FIRST admission of each
    // constraint and read by nothing.
    struct WalkSeen {
        std::vector<std::uint8_t> ineq;
        std::vector<std::uint8_t> bound;
    };

    static void mark_ineq_seen(WalkSeen &seen, QpCounters &counters, Index j);

    static void mark_bound_seen(WalkSeen &seen, QpCounters &counters, Index i);

    void refresh_shifts(const QpProblem &qp, const Vec &x, const Vec &ai_row_norm1,
                        const Vec &lambda_i, WorkingSet &ws, Vec &Aix, Vec &shift,
                        QpCounters &counters, WalkSeen &seen, const QpOptions &opts) const;

    // Feasibility tolerance for one constraint row (equality or inequality),
    // combining the two effects described above refresh_shifts: the row's own
    // magnitude, and the regularization's irreducible dual_mu*|lambda|
    // footprint -- the latter capped at kInfeasibilityAbsorbTol * row_scale
    // so a contradiction cannot fund its own tolerance.
    double row_tolerance(double row_scale, double lambda, const QpOptions &opts) const;

    // Is a single row's violation genuine evidence of infeasibility, rather
    // than the regularized solve's own noise? Both conditions must hold.
    //
    //  (a) The violation clears the row's tolerance by a real margin.
    //  (b) The violation is STRUCTURAL: it reaches an appreciable fraction of
    //      the regularization footprint dual_mu*|lambda|. On an INCONSISTENT
    //      row the identity Ai x - dual_mu*lambda = bi is exact and
    //      irreducible, so the residual sits AT the footprint (ratio ~ 1); on
    //      a merely ill-scaled but CONSISTENT row the same footprint is a
    //      first-order error refinement knocks down by orders of magnitude.
    //
    // Testing the RATIO rather than |lambda| against a fixed scale matters: a
    // contradiction with a small gap produces a correspondingly small lambda
    // and would slip under any absolute multiplier threshold.
    bool violation_is_structural(double v, double row_scale, double lambda,
                                 const QpOptions &opts) const;

    // Largest violation across BOTH constraint blocks that qualifies as
    // structural, or 0 if every violation is explainable as solver noise. The
    // shift machinery only watches inequalities, so without the equality half
    // an inconsistent equality block would be reported kOptimal.
    double worst_structural_violation(const QpProblem &qp, const Vec &x, const Vec &Aix,
                                      const Vec &ai_row_norm1, const Vec &lambda_i,
                                      const Vec &ae_row_norm1, const Vec &lambda_e,
                                      const QpOptions &opts) const;

    // Is ||x|| large in a way that only an unbounded direction explains?
    // Bound-relative on purpose: a variable boxed by finite bounds CANNOT run
    // away, and one pinned at a bound demonstrably has not, so only free
    // components that grew toward a non-constraining bound are eligible.
    //
    // "Non-constraining" is measured against the runaway threshold itself
    // rather than kEngineInfBound: a hand-written "effectively infinite"
    // bound like 1e18 still cannot restrain an artifact sitting at
    // ~1/primal_delta.
    bool is_runaway(const QpProblem &qp, const Vec &x, const WorkingSet &ws,
                    const QpOptions &opts) const;

    static bool in_working(const WorkingSet &ws, Index row);

    // Step 2/4: dispatch on ws_algebra. eliminated_candidate() below is the
    // kRefactorize path; border mode falls back to it whenever bordering is
    // not available or not worth it for an iteration.
    //
    // `verdict` reports the inertia gate's reading of whichever system was
    // ACTUALLY solved (section 4b) -- the bordered K0 or the bound-eliminated
    // K, on either mode. It is an out-parameter rather than a member so the
    // verdict can never outlive the factorization it describes.
    EqpResult eqp_candidate(const QpProblem &qp, const WorkingSet &ws, detail::KktFactor &kkt,
                            BorderState &border, QpCounters &counters,
                            detail::InertiaVerdict &verdict, const QpOptions &opts) const;

    // One solve_eqp against a freshly assembled+factorized, bound-ELIMINATED
    // KKT system. Short-circuits the empty system (every variable pinned, no
    // equalities, no working rows), which Pardiso cannot be handed -- which is
    // also why a border-mode fallback on an all-variables-pinned working set
    // costs no factorization at all.
    EqpResult eliminated_candidate(const QpProblem &qp, const WorkingSet &ws,
                                   detail::KktFactor &kkt, QpCounters &counters,
                                   detail::InertiaVerdict &verdict, const QpOptions &opts) const;

    // Border-mode counterpart of eqp_candidate (header contract, step 4):
    // bring K0's border stack in line with `ws`, rebuild K0 if that stack can
    // no longer be trusted, then solve through it. The empty-system
    // short-circuit above has no counterpart here and needs none -- K0 spans
    // all n variables whatever the working set does.
    EqpResult border_candidate(const QpProblem &qp, const WorkingSet &ws, detail::KktFactor &kkt,
                               BorderState &border, QpCounters &counters,
                               detail::InertiaVerdict &verdict, const QpOptions &opts) const;

    // Shared tail of both entry paths above: decide whether the border stack
    // is usable, spend at most one rebuild trying to make it so, and solve --
    // falling back to the elimination path (and latching where appropriate)
    // when it is not. `opts` is the effective options this solve resolved
    // (see run()'s `eff_opts`).
    EqpResult border_solve_or_fall_back(const QpProblem &qp, const WorkingSet &ws,
                                        detail::KktFactor &kkt, BorderState &border,
                                        QpCounters &counters, detail::InertiaVerdict &verdict,
                                        bool rebuilt, const QpOptions &opts) const;

    // The inertia gate for the BORDERED system K0 + live border stack (see
    // the header contract's section 4b for the derivation of both sides).
    // `border.schur` must have a value.
    detail::InertiaVerdict border_inertia_verdict(const QpProblem &qp,
                                                  const BorderState &border) const;

    // Re-run the current iteration's linear algebra for `ws` purely to read
    // the gate off it. Deliberately routed through eqp_candidate rather than
    // a bespoke factorize-and-peek: the verdict must describe the system the
    // LOOP would solve for this working set, including every rebuild, latch
    // and fallback decision. The EqpResult itself is discarded -- in border
    // mode a probe costs a few K0 solves and a dense C rebuild
    // (schur_updates, no factorization); in refactorize mode it costs one
    // factorization, counted like any other.
    detail::InertiaVerdict probe_inertia(const QpProblem &qp, const WorkingSet &ws,
                                         detail::KktFactor &kkt, BorderState &border,
                                         QpCounters &counters, const QpOptions &opts) const;

    // Section 4b's ZERO-MULTIPLIER PROBE, run at the would-be-kOptimal exit.
    // Walks every WEAKLY active working-set member (multiplier numerically
    // zero), most recently added first, tentatively drops each and re-runs the
    // inertia gate on the reduced labeling:
    //   kWrong   the hidden negative curvature is real. The drop is made REAL
    //            (`dropped` filled in exactly as drop_worst fills it) and this
    //            returns true, so the caller RESUMES the loop.
    //   kOk      that constraint hides nothing. Restore it and try the next.
    //   kSuspect treated exactly as kOk -- a fabricated pivot sign is never
    //            ground truth.
    // Returns false if every candidate came back kOk/kSuspect (or there were
    // none), leaving `ws` exactly as it found it. COST WHEN NOTHING IS WEAKLY
    // ACTIVE IS ZERO: an empty candidate list returns before any linear
    // algebra runs.
    bool probe_zero_multiplier_drops(const QpProblem &qp, const Vec &shift, const Vec &lambda_i,
                                     const Vec &z, WorkingSet &ws, detail::KktFactor &kkt,
                                     BorderState &border, QpCounters &counters, ProbeState &probe,
                                     DropRecord &dropped, const QpOptions &opts) const;

    // H's diagonal, gathered once (qp.H stores the upper triangle, so the
    // diagonal entries are the ones with col == row).
    static Vec hessian_diagonal(const QpProblem &qp);

    // Pin variable `i` at a bound, moving x(i) there, per the WHICH BOUND
    // rule in the header contract's section 4b. Returns false (leaving ws and
    // x untouched) if neither bound is finite.
    bool pin_at_best_bound(const QpProblem &qp, WorkingSet &ws, Vec &x, const Vec &grad, Index i,
                           double hii) const;

    // Temporary-vertex start repair (header contract, section 4b). Mutates
    // `ws` and `x` and returns true iff it reached a second-order consistent
    // working set; on failure both are restored to what they were.
    // `opts` is the effective options this solve resolved (see run()'s
    // `eff_opts`), threaded through to probe_inertia -> eqp_candidate.
    bool repair_temporary_vertex(const QpProblem &qp, WorkingSet &ws, Vec &x,
                                 detail::KktFactor &kkt, BorderState &border, QpCounters &counters,
                                 const QpOptions &opts) const;

    // Would re-deriving K0 from `ws` reproduce exactly the state we are in?
    // It would iff K0 already spans the current working ROWS (so the rebuild
    // assembles the same matrix) and every live border is a pin (so the
    // re-sync re-adds the same borders). Rebuilding then buys nothing and
    // costs a factorization.
    static bool rebuild_would_be_noop(const WorkingSet &ws, const BorderState &border);

    // Is the pins-only dead end still the situation we are in? EXACTLY ONE
    // question decides it: can a border stack carry the current pin count at
    // all? Pins can never be folded into K0 (assemble_kkt_full eliminates
    // nothing), so immediately after ANY rebuild_k0 the live stack is exactly
    // the pins -- while `pinned > schur_cap`, every possible K0 is born past
    // needs_refactorization() and bordering is unavailable at any price.
    //
    // Testing the pin COUNT rather than "fewer pins than at latch time" keeps
    // this from oscillating: the single release that crosses the cap costs
    // exactly one rebuild. The latch releases on the PIN COUNT alone, never on
    // a change to the working ROWS -- a working set whose rows change every
    // iteration would otherwise thrash the latch for nothing, since the
    // resumed state still has dim() > schur_cap. Holding through a row change
    // is correct because nothing reads K0 or the border stack while latched,
    // and the release rebuilds K0 from the CURRENT working set.
    bool latch_still_holds(const WorkingSet &ws) const;

    // Assemble + factorize a fresh full-variable K0 for the CURRENT working
    // set and restart the border stack empty. Counted in
    // QpCounters::factorizations. K0's regularized diagonal is built from
    // opts.primal_delta/opts.dual_mu, which is what reuse-key condition (d)
    // tracks.
    //
    // THIS IS THE SOLE SITE that reassigns `border.k0` or factorizes
    // `border.kkt`. It stamps nothing itself: `factorize_checked()` advancing
    // the factor's epoch (and `analyze()` moving its session id at a pattern
    // change) IS the stamp, for EVERY caller.
    //
    // COMMIT-LAST: nothing is published onto `border` until the factorization
    // has SUCCEEDED. Assigning `k0`/`k0_rows` first would publish a NEW K0
    // against the OLD factorization, ledger and Schur cache if the SYMBOLIC
    // analysis then threw -- analyze()'s strong guarantee leaves session
    // id/epoch/inertia untouched, so the usable-numerics reuse conjunct would
    // see a healthy factor and grant reuse, and a second holder would solve
    // against a factor of a matrix that is no longer `border.k0`.
    void rebuild_k0(const QpProblem &qp, const WorkingSet &ws, BorderState &border,
                    QpCounters &counters, const QpOptions &opts) const;

    // Apply the delta between the working set `border`'s ledger currently
    // represents and `ws`, one BorderOps construction per change. Drops run
    // first, in REVERSE ledger order, so the add_border-order indices
    // SchurComplement::drop_border expects stay valid as entries are removed.
    //
    // A pin border's ledger entry names only the VARIABLE, so a bound that
    // switches side needs no border work at all: the pinned value is read from
    // ws.bound_state() when the rhs is built (solve_bordered_eqp), not baked
    // into the border column.
    void sync_borders(const QpProblem &qp, const WorkingSet &ws, BorderState &border,
                      QpCounters &counters, const QpOptions &opts) const;

    static bool has_border(const std::vector<BorderLedgerEntry> &ledger,
                           BorderLedgerEntry::Kind kind, Index target);

    // Scatter the EQP multipliers into full-length vectors and price the
    // bound multipliers from the stationarity residual (eqp_solve.h).
    static void price(const QpProblem &qp, const WorkingSet &ws, const EqpResult &eqp,
                      Vec &lambda_e, Vec &lambda_i, Vec &z);

    // Step 2's drop rule. Returns true (and mutates `ws`) iff something left
    // the working set; false means the point is optimal. `dropped` records
    // WHAT left, which section 4c's ride needs to choose its sign; it is left
    // untouched when nothing is dropped.
    bool drop_worst(const QpProblem &qp, const Vec &shift, const Vec &lambda_i, const Vec &z,
                    WorkingSet &ws, DropRecord &dropped, QpCounters &counters) const;

    // Directional derivative of the constraint `dropped` names, along `d`.
    // Stated so that a NEGATIVE value means "d moves into that constraint's
    // feasible side", uniformly for a general row (a_j . x <= b_j) and for a
    // bound pin (x_i <= u_i at an upper bound, -x_i <= -l_i at a lower one).
    double dropped_directional(const QpProblem &qp, const DropRecord &dropped, const Vec &d) const;

    // Does the ride's ray stay inside the current working set -- i.e. does `p`
    // lie in the null space of the working constraints (equalities, working
    // inequality rows, pinned variables)?
    //
    // THIS IS A PRECONDITION OF THE RIDE, not a nicety. The uncapped ratio
    // test SKIPS working rows, on the standing assumption that a step along an
    // EQP direction preserves them -- an assumption the ordinary step earns by
    // being capped at alpha = 1, which lands exactly on the EQP's own point. A
    // ride has no such cap, so any component of p out of that null space sails
    // straight through a working row nothing is watching (the HOMOTOPY, where
    // A_j p != 0 by design, and a DEGENERATE working set, where x sits off the
    // rows by O(dual_mu * |lambda|)). Failing this DECLINES the ride, falling
    // back to the ordinary capped step, which handles both cases correctly.
    bool ride_stays_in_working_set(const QpProblem &qp, const WorkingSet &ws, const Vec &p) const;

    // Which sign of `p` the ride takes, or 0 to DECLINE the ride entirely.
    //
    // Two conditions, and section 4c requires BOTH:
    //   DESCENT      the objective's directional derivative along the chosen
    //                sign is STRICTLY negative, past a tolerance relative to
    //                ||g_x|| * ||p||. With non-positive curvature this makes
    //                the objective decrease monotonically along the whole ray,
    //                so the working set cannot be revisited at the same
    //                objective.
    //   FEASIBILITY  the chosen sign moves OFF the constraint the drop rule
    //                just released. Its slack is exactly zero, so the other
    //                sign is answered by the ratio test with alpha = 0 and an
    //                immediate re-add -- the cycle this section breaks.
    //
    // At a KKT point of the PRE-drop working set the two provably agree; they
    // CAN disagree on a DEGENERATE working set, where the multipliers split
    // among dependent rows and "lambda_c < 0" stops implying that moving off c
    // improves anything. A disagreement DECLINES, falling through to the
    // ordinary EQP step.
    //
    // A FLAT DIRECTION ALSO DECLINES, and that is load-bearing: it buys no
    // objective decrease while still pinning a blocker into the working set,
    // which can turn a legitimate small row residual into a spurious
    // kInfeasible at step 5's classifier, and it would contradict 4c's
    // anti-cycling argument, which rests on the decrease being STRICT.
    int ride_sign(const QpProblem &qp, const DropRecord &dropped, const Vec &p, const Vec &x) const;

    // 2-norm of the dropped constraint's gradient, for ride_sign's tolerance.
    static double dropped_gradient_norm(const QpProblem &qp, const DropRecord &dropped);

    // Section 4c's ride. `p` is the post-drop EQP step, already measured to
    // have non-positive curvature, so the QP is unbounded below along one of
    // +/-p inside the current working set. Steps to the nearest blocker along
    // the admissible sign and puts that blocker into `ws`. `ws` and `x` are
    // left untouched on kUnbounded and kDeclined.
    RideOutcome ride_negative_curvature(const QpProblem &qp, const DropRecord &dropped,
                                        QpCounters &counters, WalkSeen &seen, const Vec &p,
                                        const Vec &Aix, WorkingSet &ws, Vec &x) const;

    // Step 3. Reports the blocking constraint and the step to it.
    //
    // `cap_at_unit` distinguishes an ordinary EQP step from a ride (section
    // 4c). Set -- every call from the main loop's step path -- alpha is
    // clamped to [0, 1], because 1 lands on the EQP's own solution and a
    // constraint beyond that is not blocking anything (reported kNone). A ride
    // has no such landing point, so it clears the flag and takes the raw
    // ratio; alpha is then finite exactly when a blocker was found.
    //
    // `tied` counts, EXACTLY, how many candidates share the final minimum
    // ratio: resetting on every strict decrease of `best` and incrementing on
    // every exact equality leaves it equal to the true multiplicity, with no
    // second pass and no tolerance. SELECTION IS UNTOUCHED by it.
    static double ratio_test(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                             const Vec &Aix, const Vec &p, BlockKind &kind, Index &block_idx,
                             BoundState &block_state, bool cap_at_unit, QpCounters &counters);

    // const: nothing may mutate opts_ after construction; every field it holds
    // is this engine instance's DEFAULT for that field, resolved against a
    // solve()'s own SolveOverrides at the top of run() (`eff_opts`) rather
    // than read directly by most of the file below.
    const QpOptions opts_;
    Ledger *ledger_ = nullptr;
    std::string label_prefix_;
    mutable Index solve_counter_ = 0;

    // HOT-START REUSE state (border mode only; see the header contract).
    // border_ persists across solve() calls -- constructed once, in this
    // instance's constructor. The rest track the fingerprint of the problem
    // and the exit working set border_ was last left representing, and whether
    // that snapshot is still trustworthy (border_valid_). Mutable because
    // solve() is logically const even though the engine caches state between
    // calls. Held through a std::shared_ptr so hot_state() can hand a COPY of
    // this pointer to a different QpEngine instance (HotState's OWNERSHIP
    // note) without moving or copying the BorderState. Never null: only ever
    // REASSIGNED, by run()'s ADOPT step.
    mutable std::shared_ptr<BorderState> border_;
    mutable bool border_valid_ = false;
    mutable std::uint64_t border_structural_hash_ = 0;
    mutable std::uint64_t border_values_hash_ = 0;
    // Condition (d): the EFFECTIVE (primal_delta, dual_mu) pair the last
    // trustworthy solve resolved to. Committed alongside the two hashes above
    // on the same clean-kOptimal-exit schedule, and compared against the NEXT
    // solve's own resolved pair before reuse is granted. tr_radius has no
    // counterpart here: it never joins this key at all.
    mutable double border_effective_delta_ = 0.0;
    mutable double border_effective_mu_ = 0.0;
    mutable std::vector<BoundState> border_exit_bound_state_;
    mutable std::vector<Index> border_exit_active_ineq_;
    // Condition (e) (HotState's OWNERSHIP note has the full argument): this
    // engine's own last-trusted (session_id, epoch) pair of whatever object
    // `border_` currently names, compared against that object's LIVE identity
    // at the top of every run() call. That is what detects a DIFFERENT engine
    // having rebuilt the shared object, even when every other fingerprint
    // matches. Joined there by the usable-numerics conjunct.
    mutable std::uint64_t border_kkt_session_id_ = 0;
    mutable std::uint64_t border_kkt_epoch_ = 0;
};

} // namespace hven::solvers
