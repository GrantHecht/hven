// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The primal active-set loop for the QP
///        min g^T x + 1/2 x^T H x  s.t.  Ae x = be, Ai x <= bi, l <= x <= u.
///
/// H need not be positive semidefinite; sections 4b and 4c add what an
/// INDEFINITE H needs on top of the ordinary convex loop, and both are inert on
/// a convex one. The numerics live below this file -- the working set, the
/// regularized bound-eliminated KKT assembly, the sparse factorization and the
/// equality-QP solve with iterative refinement. This file is the loop that walks
/// between working sets.
/// @see docs/notes/2026-09-header-prose-archive.md §qp_engine.h
//
// --- Loop contract ---
//
// 0. CROSSED BOUNDS. lower(i) > upper(i) + feas_tol is an empty box: verdict
//    kInfeasible immediately, x = the clamped start point, multipliers zero.
//    Checked here rather than in QpProblem::validate() because a caller may
//    legitimately hand this engine an empty box as a normal runtime outcome.
//
// 1. START POINT. Cold: x = clamp(0, l, u). Warm: clamp(seed.x, l, u) plus
//    seed.bound_state/seed.ineq_active as the initial working set. l(i) == u(i)
//    is kFixed and never leaves. A variable merely sitting at a bound is NOT
//    pinned at start -- the ratio test pins it the first time it blocks.
//
//    General inequalities violated at the start enter via a SHIFTED-CONSTRAINT
//    HOMOTOPY rather than a phase-1 LP: shift(j) = max(0, Ai_j x - bi_j), and
//    every row with shift(j) > 0 joins the working set. The EQP always solves
//    against the TRUE rhs, so a shifted row's shift decays with the step. Shifts
//    are recomputed from x every step and clamped to zero below a scale-aware
//    tolerance. A working row whose shift is still positive is EXEMPT from step
//    2's drop rule.
//
// 2. EQP CANDIDATE + DROP RULE. Each iteration solves the EQP on the current
//    working set. If p = x* - x is negligible, x is a KKT point of the working
//    set and the multipliers decide: a working inequality needs
//    lambda_i >= -opt_tol, a variable at its lower bound needs z >= -opt_tol
//    (upper: z <= opt_tol), and kFixed variables are unconstrained. No violation
//    gives kOptimal; otherwise the MOST NEGATIVE multiplier leaves (Dantzig),
//    ties broken by largest angle then lowest index.
//
//    TR-pinned stationarity caveat (section 6). z is priced and consulted
//    internally the same way at every index, but the z REPORTED in QpSolution is
//    forced to 0 at a TR-pinned index, so there the reported quantities do not
//    satisfy stationarity. Read tr_active, not z or bound_state, for TR status.
//
// 3. RATIO TEST. Step along p: alpha = min(1, min_j ratio_j), stopping at the
//    first non-working inequality or bound that blocks; that constraint joins
//    the working set. A ratio landing at 1 within kEngineStepTieTol blocks too.
//
//    Known labeling divergence. A variable already sitting on its bound with
//    p(i) == 0 is never pinned, so it is reported kFree/z == 0 where a dense
//    oracle reports kAtLower/kAtUpper with a zero multiplier. x, the objective
//    and the duals agree; only the LABEL differs. A caller needing activity by
//    geometry must test the residual itself.
//
// 4. Working-set update. Under ws_algebra == kRefactorize every working-set
//    change is followed by a fresh assemble_kkt() + factorize_checked().
//
//    BORDER MODE (kSchurBorder, the DEFAULT) leaves the loop unchanged and swaps
//    only the linear algebra: one full-variable K0 is factorized from the seed
//    working set, and every later change becomes a GMSW border over that fixed
//    factorization. K0 is rebuilt -- clearing the border stack -- when
//    SchurComplement::needs_refactorization() trips or K0's own factorization
//    needed a perturbed pivot; iterations where a rebuild cannot help fall back
//    to the elimination path.
//
//    THE BORDERED ANSWER'S CERTIFICATE (independent_working_set; M7 W2 BPS;
//    bordered_eqp.h states it), from an iterate that meets its equality rows: a
//    pass whose bordered refinement stops above its own target takes the
//    eliminated twin for that pass (border_uncertified_fallbacks); an answer
//    whose step is undecided at the zero-motion scale is refined past the
//    footprint while the refinement contracts. With the option off nothing
//    changes.
//
//    The two modes are OBSERVATIONALLY EQUIVALENT for a CONVEX H, with the
//    refactorize path as the oracle. They are NOT equivalent for an indefinite
//    H: the bound-eliminated K and the full-variable K0 can have different
//    inertia, so the two modes can reach different working sets and statuses.
//
//    COUNTER SEMANTICS (relied on downstream). minor_iters increments exactly
//    ONCE per major iteration. Under kRefactorize, factorizations counts
//    solve_eqp calls (one per major iteration, except the empty-reduced-system
//    short-circuit) and schur_updates stays 0. Under kSchurBorder,
//    factorizations counts K0 factorizations plus elimination-path fallbacks,
//    and schur_updates counts individual add_border/drop_border calls including
//    re-adds after a rebuild, but not the rebuild's own clear. ON EITHER MODE
//    the verdict-site face refinement's ELIMINATED twin charges one
//    factorization and one symbolic_analyses when its working-set guard MISSES;
//    a guard HIT charges neither, and a solve that never dead-ends charges
//    neither. 4c's ride costs one minor_iter like any other step; 4b's repair
//    costs one EXTRA minor_iter plus, per pin or release it probes, one
//    schur_update or one factorization.
//
//    Hot-start reuse (border mode only). border_ is an ENGINE-INSTANCE member,
//    so a warm re-solve on the same QpEngine can skip K0's assembly and
//    factorization when FIVE conditions all hold at the seed working set,
//    checked once before the loop's first iteration:
//      (a)/(c) H/Ae/Ai's structural pattern AND values are byte-identical to the
//          previous trustworthy solve. K0's values depend on H/Ae/Ai and the
//          EFFECTIVE (primal_delta, dual_mu) -- never on g/be/bi.
//      (d) that effective pair is identical to the previous trustworthy solve's.
//          tr_radius is NOT part of the key: bounds never enter K0.
//      (b) the seed working set equals the EXIT working set of that same solve.
//      (e) border_'s factor's own live (session_id, epoch) identity equals the
//          pair THIS engine last saw as trustworthy, AND the factor's numerics
//          are usable (inertia().state == kObserved).
//
//    (b) IS ONLY APPROXIMATE, AND THAT IS SAFE: refresh_shifts() can add a row
//    after the last sync_borders() call, so the recorded exit set can understate
//    the true one. What makes reuse safe is that sync_borders() is an
//    UNCONDITIONAL, FULL reconciliation run on EVERY iteration of EVERY solve --
//    the fast path skips rebuild_k0's assembly and factorization, never
//    sync_borders().
//
//    All five conditions are NECESSARY but NOT SUFFICIENT for
//    `factorizations == 0`: border_candidate's own checks can still force a
//    rebuild. Callers must not assert that count unconditionally on a warm
//    re-solve.
//
//    INVALIDATION POLICY. border_'s cache is committed ONLY on a clean kOptimal
//    exit, and is pessimistically cleared at the top of every solve() call
//    before anything else runs, qp.validate() included. Every other exit, and
//    any exception thrown mid-solve, leaves the cache invalidated.
//
//    THREAD SAFETY. QpEngine is NOT thread-safe for concurrent solve() calls on
//    one instance. "One QpEngine per thread" is not the whole rule either: two
//    DIFFERENT engines can share one BorderState if one adopts a hot handle the
//    other produced. SEQUENTIAL hand-off is safe -- the session/epoch identity
//    detects a producer that mutated the object again and degrades to kWarm.
//    CONCURRENT use of a shared BorderState is UNDEFINED.
//
// 4b. Inertia gate and temporary-vertex start repair (indefinite H).
//
//    For an indefinite H the regularized KKT system is still nonsingular but its
//    answer can be a saddle or a maximizer the loop would certify kOptimal. The
//    signature is the KKT matrix's INERTIA: where the reduced Hessian is
//    positive definite on the null space of the working constraints,
//    [H+delta*I A^T; A -mu*I] has inertia exactly (#variables, #constraint rows,
//    0) -- unconditional for a convex H, which is why the gate is a no-op there.
//    The gate must read the BORDERED system's inertia, never K0's numbers
//    against a fixed expectation.
//
//    Perturbed pivots are not A Pass and not A REPAIR TRIGGER: on an exactly
//    singular matrix the backend fabricates a pivot sign and reports an inertia
//    indistinguishable from the nonsingular case. detail::InertiaVerdict has
//    three verdicts: kOk (trustworthy and matching), kSuspect (inertia UNKNOWN
//    -- never a pass, and never evidence that a repair is needed), and kWrong
//    (trustworthy and DISAGREEING -- the working set is second-order
//    inconsistent, which at solve start triggers repair_temporary_vertex).
//
//    Second-order certification (step 5's classification). When the loop reaches
//    a point it can neither improve nor drop from and the verdict for the system
//    just solved is a TRUSTED kWrong, the point is reported kNumericalError
//    rather than kOptimal, multipliers cleared. Only kWrong triggers this.
//
//    ZERO-MULTIPLIER PROBE. The gate tests the null space of the FULL active
//    labeling, so negative curvature excluded only by a WEAKLY ACTIVE constraint
//    stays hidden. probe_zero_multiplier_drops runs at the would-be-kOptimal
//    exit and tentatively drops each such member, most-recently-added first,
//    re-running the gate; a kWrong makes the drop real and RESUMES the loop.
//    One-at-a-time is the KNOWN REMAINING approximation.
//
//    Suspect-stall gate. When the verdict for the system the FINAL iterate was
//    solved from is kSuspect, kOptimal may be certified only after an explicit
//    free-block stationarity check on the QP model, which is NaN-aware and
//    applies a SCALED rather than absolute opt_tol. It runs AFTER the
//    zero-multiplier probe and independently of it. On failure primal_delta is
//    escalated one decade, the border state is discarded and the iteration
//    resumes; the ladder is bounded and reports kNumericalError when exhausted.
//    Never kOptimal off a stalled suspect loop, and never a hang. CAVEAT (known,
//    unmeasured): refresh_shifts() between the pricing and the check can add
//    rows with lambda_i == 0 and inflate the residual on a point that is in fact
//    stationary -- a spurious escalation bounded by the ladder, not a wrong
//    answer.
//
//    POST-PROBE RESTART. A probe-driven drop leaves 4c's ride nothing to arm
//    off, so the temporary-vertex repair is spent ONCE per solve at the
//    certification branch's trusted-kWrong arm instead.
//
// 4c. Negative-curvature rides after a drop (indefinite H).
//
//    Section 4b makes an indefinite START safe; this makes an indefinite DROP
//    safe. By Cauchy interlacing a drop is the ONLY way negative curvature can
//    reappear once a working set is second-order consistent, and the curvature
//    of the ordinary EQP step already IS the test, with no extra solve: with
//    sigma the Schur complement of the released row, p'Hp has the SIGN of sigma.
//    When sigma < 0 the step points back into the just-released constraint, the
//    ratio test answers alpha = 0, and without this section the loop cycles.
//
//    THE CHECK. On the iteration after a drop, and only there, the Rayleigh
//    quotient p'Hp/p'p is compared against
//    curvature_tol = kCurvatureTolFactor * hessian_scale(qp). Above it the
//    ordinary EQP step is taken; at or below it the loop RIDES -- the admissible
//    sign of p, the ratio test with its unit cap DISABLED, and a step to the
//    nearest blocking constraint. A negligible p is excluded first. NO BLOCKER
//    gives kNumericalError with multipliers cleared, reported immediately from
//    inside the loop.
//
//    ARMING IS ONE-SHOT: the drop record is consumed on the very next iteration
//    whether the ride fires, is refused, or is never reached -- except where THE
//    DECLINED RIDE below re-arms it. THIS SECTION IS VACUOUS for a probe-driven
//    drop, which 4b's Post-PROBE restart intercepts. 4c's no-blocker branch and
//    4b's certification branch are mutually exclusive by construction and report
//    the same status.
//
//    THE DECLINED RIDE. A drop onto a negative-curvature face never falls
//    through to a step that re-enters the released constraint. With
//    independent_working_set, per refusal:
//    (iii) The ride's first blocker tests DEPENDENT on the pass's factor: it
//        takes the add site's rule (Degeneracy) along the ride direction,
//        uncapped -- the exchange (ExchangePick), else the certificate, the
//        pass-over (the next blocker takes the same rule) or the end. After an
//        exchange the ride is RE-ARMED and re-taken on the exchanged set (same
//        null space, fresh factor); an independent blocker reached by pass-over
//        is a ride step. Finite: a ride step that moves strictly decreases f;
//        at one point an exchange releases only a member that did not enter
//        there (the lock) and marks the blocker entered, so at most |W| per
//        point, inside the zero-motion cap.
//    (ii) ride_sign's veto (flat slope, or descent against leaving): the drop
//        is UNDONE -- the released constraint re-enters at this point,
//        drop-exempt under the zero-motion re-entry rule -- never the ordinary
//        step. At a stationary point of an independent set g'p = z_s * (leaving
//        rate of p) EXACTLY, so descent and leaving agree there; they disagree
//        only by the point's own first-order residual along p, which is what
//        THE FLAT FACE below reads the veto as. ride_sign_vetoes counts every
//        firing.
//    (i) The null-space veto (p off the retained set's null space): the
//        ordinary step only when p leaves the released constraint
//        (dropped_directional(p) < -tol, ride_sign's tolerance); else undone
//        as in (ii). No reduced-curvature re-read is made.
//    Counted: dep_ride_blockers, ride_exchanges, ride_declines_fallthrough_blocked,
//    ride_sign_vetoes.
//    With the option off, (i) and (ii) fall through to the ordinary step and
//    (iii) cannot arise.
//
//    THE FLAT FACE (independent_working_set). On a face with NO curvature the
//    post-drop quotient is 0 and the gate fires (hessian_scale floors at 1, so
//    curvature_tol is a positive floor and not 0): the direction is a RAY and
//    the ride along it is the problem's own answer. What stops a ride there is
//    ride_sign, so ride_sign carries the rule.
//    At a stationary point of an INDEPENDENT retained set the slope along p is
//    also readable from the drop's own multiplier,
//        s = violation * dropped_directional(p),
//    which IS g'p in exact arithmetic on every member kind: the Dantzig
//    violation is -lambda_s on a row, -z_s at a LOWER bound and +z_s at an
//    UPPER one, dropped_directional carries the matching sign, and every
//    retained row is orthogonal to p, so that one product is the whole of g'p.
//    The two differ by rho'p, rho being the point's own first-order residual
//    (the regularized EQP's, as its one unregularized refinement step leaves
//    it), and on a flat face rho'p can be the LARGER term and of the opposite
//    sign.
//    RULE: the MEASURED g'p is the exact directional derivative of the
//    objective, so it decides the sign WHENEVER IT RESOLVES above ride_sign's
//    slope tolerance; s decides only below that resolution, where the measured
//    slope says nothing (ride_predicted_signs counts those). DESCENT INVARIANT:
//    no ride ever ascends a slope the arithmetic can read --
//    sign * g'p <= kEngineDenomTol * ||g|| * ||p|| on every ride taken, by
//    construction, hence f does not rise by more than alpha times that.
//    The predicted sign IS the leaving sign by construction (violation > 0 at
//    every drop, so s < 0 iff p leaves the released constraint), so where s
//    decides, (ii)'s feasibility half cannot veto it.
//    A drop undone on ride_sign's veto is marked kPtNoise -- it does not count
//    toward zero-motion exhaustion at this point -- but ONLY when it is a NOISE
//    drop: |s| at or below the resolution of the slope it was meant to predict,
//    which is the larger of the dot product's own floor and |g'p - s|, the
//    residual the two slopes disclose (ride_drop_is_noise; no extra solve).
//    Testing |s| against an ABSOLUTE floor instead would read a resolving
//    multiplier on a p that barely leaves the constraint as noise; testing it
//    against the residual along the SAME p does not. pt_clear retires the mark
//    as soon as the iterate moves.
//    Note that a sign veto and a noise drop coincide under this rule: a veto
//    needs the two slopes to disagree or neither to resolve, and a disagreement
//    forces |s| < |g'p - s| by that same identity.
//
//    THE FLOOR IS NOT A COINCIDENCE-FREE ZONE. "s decides only below the
//    measured slope's resolution" is inert on the faces this rule exists for --
//    where rho'p dominates, the measured slope is far ABOVE the tolerance, not
//    below it. It is not impossible: g'p = s + rho'p cancelling to within
//    kEngineDenomTol * ||g|| * ||p|| is a coincidence of measure ~tol/|s|, not
//    something double precision forbids. It was simply measured never to happen
//    on the corpus -- see
//    docs/notes/data/2026-09-m7-w2-nc2-flat-face/REPORT.md.
//
//    THE EQUALITY DRIFT (independent_working_set). A regularized EQP answer can
//    miss its OWN equality rows, and a ratio-test step toward such an answer
//    leaves the iterate off rows it had already met. The iterate's invariant -- no equality
//    row carries a STRUCTURAL violation, the terminal classifier's own test
//    (violation_is_structural, row by row, on this pass's multipliers) -- is
//    therefore CHECKED at every pass once the walk has met the rows (a start
//    point need not), and a miss is COUNTED (eq_drift_violations). Stated in the
//    classifier's currency and not one digit tighter: a residual section 5 would
//    not call a violation is not a drift either. NOTHING re-projects the
//    iterate: the repair belongs where the miss is made, in the regularized
//    solve, and the walk-side forms that were implemented and measured are in
//    docs/notes/data/2026-09-m7-w2-nc2-flat-face/REPORT.md.
//
//    For STRICTLY CONVEX H the ride branch is UNREACHABLE and the engine is
//    bit-for-bit unchanged. For PSD-SINGULAR H it IS reachable and behaviour is
//    NOT identical to the capped path: the ride takes the uncapped ratio while
//    the ordinary path clamps at alpha = 1. That is intended.
//
//    FINITENESS. A ride that MOVES decreases f strictly (the descent invariant
//    above with alpha > 0), so it needs no anti-cycling rule. A ride whose
//    uncapped ratio test answers a step within step_tol moves NOTHING and is
//    the ZERO-MOTION EVENT it looks like: the point memory is not cleared and
//    the ride's blocker is admitted through it -- the ordinary add path's own
//    two rules -- so the release/re-entry cap 2(n + mi) bounds a
//    drop -> zero-length ride -> drop cycle.
//
//    THE SADDLE EXIT (independent_working_set). At the certification branch's
//    trusted-kWrong arm (4b; after R-C''s re-read and the post-probe restart) the
//    point is a stationary point of an INDEFINITE face, however the face was
//    reached -- an add as well as a drop. Before the kNumericalError exit the
//    face is searched for a direction of negative curvature: ONE factorization
//    of the current face at dual_mu = 0 (the bound-eliminated K R-C' reads), and
//    detail::face_negative_curvature on it -- Lanczos on the inverse reduced
//    Hessian through that factor (papers: Forsgren, Gill & Murray 1991; Gill,
//    Murray, Saunders & Wright 1991). A direction is used only when VERIFIED:
//    d'Hd <= -curvature_tol * d'd on H itself, and d in the working set's null
//    space (ride_stays_in_working_set). SIGN: the descent sign when g'd resolves;
//    on a flat slope (the stationary point's own case, where both signs descend)
//    the sign with the larger decrease to its first blocker, a tie preferring the
//    sign that leaves the constraint the previous pass released -- and a ride
//    that would re-enter that constraint at zero length is not a direction (it
//    undoes the drop: the zero-multiplier probe's, whose point would then be
//    re-certified); the same for a zero-length re-entry of ANY probe-exempt member
//    or any member the point memory holds released at this point (two such rides
//    would otherwise re-certify the saddle through a degenerate vertex). Then it
//    is ridden as 4c's ride is: the uncapped ratio test to the first blocker; an independent
//    blocker joins; a dependent one takes the add site's rule along d (THE
//    DECLINED RIDE (iii)) on that same mu-free factor, and nothing is re-armed;
//    no blocker is kNumericalError (unbounded along d, multipliers cleared). No
//    verified direction: the kNumericalError exit stands. NOTHING IS CERTIFIED
//    HERE -- every later exit is the ordinary walk's, its inertia gate included.
//    DESCENT: f(x + alpha d) - f(x) = alpha g'd + alpha^2 d'Hd / 2 with
//    d'Hd < 0 and g'd at most its own resolution (kEngineDenomTol ||g|| ||d||):
//    no ride ascends a slope the arithmetic can read, and the curvature term
//    makes it a decrease for every alpha above 2 g'd / |d'Hd|, a rounding-sized
//    threshold. BOUNDED: one search per dead end (one factorization, at most
//    kSaddleLanczosSteps operator applications); a ride that moves decreases f
//    strictly; a zero-length one is a zero-motion event (the point memory kept,
//    its blocker admitted through it), inside the zero-motion cap; all of it
//    inside the minor cap. Counted: saddle_exits, saddle_exit_misses,
//    saddle_direction_solves. With the option off the exit is unchanged.
//
//    THE CONVEX CERTIFICATE (independent_working_set; M7 W2 NC4). Taken at the
//    same arm, after R-C''s re-read and after the post-probe restart (an armed
//    restart is spent first, as before; a failed one falls through to this), in
//    place of the saddle search:
//    when every stored entry of H is exactly 0 off the diagonal and >= 0 on it
//    (detail::hessian_is_nonneg_diagonal; an LP is H == 0) or the caller asserts
//    it (QpOptions::hessian_known_convex, default false), H is PSD on all of
//    R^n, so every K the walk factorizes, [H_F + delta I, A'; A, -mu I], is
//    quasi-definite and its TRUE inertia IS the expectation: a trusted kWrong
//    read is a misread (a flat face's delta-sized null-space pivots signed by
//    rounding), not a saddle -- and the saddle exit could only miss there (no
//    d'Hd < 0 exists, in exact or floating-point arithmetic). If the free block
//    is stationary in the suspect-stall gate's own opt_tol currency, the read is
//    replaced by kSuspect and the point goes to the would-be-kOptimal branch --
//    the ordinary walk's tests, unchanged: the zero-motion exhaustion test, the
//    suspect-stall gate (which the precondition passes, so the escalation
//    ladder is never entered from here), the terminal polish, the certificate's
//    bound half. Not stationary: the kNumericalError exit stands, as before.
//    The zero-multiplier PROBE is skipped on this path: on a convex QP no
//    weakly active constraint can hide negative curvature, and its inertia
//    reads are the same misreads. SOUNDNESS: for a convex QP a first-order
//    point is a global minimizer; nothing is certified on the exemption of a
//    probe or of the point memory alone -- the exhaustion test still reads
//    every exempted multiplier. A face with no verified negative curvature that
//    is NOT provably PSD this way stays on the saddle exit's path (a miss is
//    kNumericalError), and a true saddle never qualifies (a nonnegative
//    diagonal H has no negative curvature anywhere). The only outcomes it can
//    change are the ones that were kNumericalError at this exit. Counted:
//    convex_certificates. With the option off nothing changes.
//
//    THE ASCENT GUARD (independent_working_set; M7 W2 W1). Interlacing makes a drop
//    the only way INTO an indefinite face, but nothing makes a ride's add the way
//    OUT: on the reduced space of W - s + r the Hessian is A + (gamma/beta^2) c c'
//    (A the PD reduced Hessian of W, gamma = p'Hp < 0, beta = a_r'p, c = Z_W'a_r), which
//    can stay indefinite. The next pass follows no drop, the curvature gate is
//    unarmed, and the EQP step heads for the face's stationary point -- a saddle.
//    With p'(H + delta I)p = -(Hx + g)'p on the face, a slope s > 0 gives
//    f(x + alpha p) - f(x) = s (alpha - alpha^2/2) - delta alpha^2 |p|^2 / 2, positive
//    at every alpha in (0, 1] once s > delta |p|^2: an ascent (BRITGAS: s = 0.181,
//    delta |p|^2 = 8e-8), and a dependent blocker then exchanges
//    without changing the face, so it repeats (BRITGAS's elastic qp1 cycled so to
//    its minor cap). RULE: on any pass about to take the ordinary step (no ride
//    pending), when s > kEngineDenomTol ||Hx + g|| ||p||, p'Hp <= -curvature_tol p'p
//    (VERIFIED negative curvature, the saddle exit's test), p holds the working set
//    (ride_stays_in_working_set), x meets every equality and working row within
//    row_tolerance (iterate_meets_working_face: no row-restoring part, which a
//    phase-1 step may legitimately pay for in f), and the capped step along p or the
//    ride along -p would move x (a pass where both are zero-length is a degenerate
//    pivot, not an ascent, and keeps the ordinary add: measured, guard rides at such
//    vertices only re-ordered the pivots and sent a Debug walk to a blocker
//    termination), the walk rides -p instead -- a
//    direction of negative curvature taken with its descent sign to the nearest
//    blocking constraint (Gill, Murray, Saunders & Wright 1991; Fletcher, Practical
//    Methods of Optimization, ch. 10), never the indefinite face's stationary point.
//    It is ridden as the saddle exit's d: the uncapped ratio test; an independent
//    blocker joins; a dependent one takes the add site's rule along -p and nothing
//    is re-armed (not counted in dep_ride_blockers / ride_exchanges, which stay 4c's
//    and the exit's); no blocker is kNumericalError. DESCENT: g'(-p) = -s < 0 and
//    p'Hp < 0, so f falls strictly for every alpha > 0 -- a ride that moves cannot
//    cycle, a zero-length one is a zero-motion event inside the zero-motion cap.
//    Counted: ascent_rides. With the option off nothing changes.
//
// 5. TERMINATION. The loop reaches a KKT point of its working set with nothing
//    left to drop, and classifies it.
//
//    HOW A PASS IS RECOGNISED AS A KKT POINT OF ITS WORKING SET (M7 W2 E4,
//    ruling E4-2) -- the free-block STATIONARITY RESIDUAL, in the engine's own
//    opt_tol currency, AND the rows:
//        free_block_stationarity(x, price's multipliers) <= opt_tol * its scale
//      AND no row or bound carries a STRUCTURAL violation
//    which are the same two tests this classification itself decides kOptimal
//    on, asked one branch earlier so the loop can recognise the point it is
//    standing on. The zero-motion reading ||p||inf <= step_tol is kept, ahead of
//    it, as the observation it always was -- the EQP answer IS this iterate, so
//    snapping onto it is free -- and it alone may no longer certify convergence.
//    WHY: since E4 the EQP is centred at the iterate (eqp_solve.h), so p is the
//    STEP and on a flat face p = -(1/delta) P_Z (H x + g); delta*||p||, not
//    ||p||, is the reduced gradient's size. A reduced gradient of 1e-17 makes a
//    step of 2.8e-09 against a step_tol of 1e-09, and 300 consecutive full steps
//    were measured around a point whose rows were met to 5e-14 and whose
//    stationarity residual was 1.6e-15 -- a converged point the old test could
//    not see (M7 W2 E3 §3e). The stationary branch does NOT snap x onto the EQP
//    point: nothing has run a ratio test on a step of that size.
//
//    THE RUNAWAY GUARD IS ASKED ON EVERY MOVING PASS, not only here (E4-3), and
//    in the ITERATE's currency: `is_runaway`, this section's own predicate,
//    unchanged, at the head of the ordinary path. That is what the proximal
//    centring makes necessary -- the ridge's answer is now re-proposed from every
//    point it reaches instead of vanishing at its fixed point, so the cap would
//    be burned rather than the artifact reported. A SECOND guard in the STEP's
//    currency was built, MEASURED and REMOVED: at the scale the three runaway
//    fixtures need it fired on 10 HS cells that are not runaways and cost HS10
//    and HS27, so the iterate and the ridge scale are the only currencies the
//    guard is asked in (eqp_solve.h says the same, and says why ||p|| cannot
//    carry it).
//
//    A pass whose EQP answer is REFUSED -- by the EQP-site residual check
//    (eqp_solve.h; E4-4) or by the step's ROW-DRIFT CHECK
//    (eqp_answer_drifts_off_met_rows) -- or that is not finite takes 4b's
//    SUSPECT-STALL ESCALATION instead of a step: one decade of primal_delta per
//    rung, the border state discarded, at most kMaxSuspectEscalations rungs.
//
//    THE RULE, E4-4 AS AMENDED (ruling E4-27; the other sites point here): no
//    step is taken toward a refused answer WHILE THE LADDER HAS RUNGS; an
//    EXHAUSTED ladder is ADVISORY for BOTH refusals -- the walk steps and this
//    section's TERMINAL SITE decides, because exhaustion says the residual is
//    IRREDUCIBLE (what an inconsistent row set looks like) rather than
//    untrustworthy, and the certificate is a claim about the point the walk stops
//    at and never about the refused pass -- and only a NON-FINITE answer is
//    TERMINAL (kNumericalError at the finite iterate the walk stands on).
//    MEASURED both ways: the terminal form cost an inconsistent pair its
//    certified kInfeasible, and classifying AT the refused pass produced 16 false
//    census certificates. `eqp_residual_refusals` and `eqp_row_drift_refusals`
//    carry the reason.
//
//    The classification:
//      - kInfeasible if any inequality or equality row carries a STRUCTURAL
//        violation. Both blocks are checked; the shift machinery only watches
//        inequalities.
//      - kNumericalError if the point is feasible but some free component
//        unbounded on the side it grew toward exceeds
//        detail::unbounded_artifact_scale() -- the answer is the regularization
//        talking, not an optimum.
//      - kNumericalError if the inertia gate's verdict for the system just
//        solved is a TRUSTED kWrong. ONE ESCAPE: 4b's Post-PROBE restart.
//      - otherwise the Zero-multiplier PROBE gets a veto: a trusted kWrong there
//        makes the drop real and RESUMES the loop without assigning a status.
//      - kOptimal otherwise, but only if x meets its own box: a bound, read as
//        the row e_i under the rows' structural test, violated gives kInfeasible.
//    kMaxIter once eff_max_iter major iterations have been spent. A fourth
//    kNumericalError exit bypasses this classification: 4c's ride, no blocker.
//
//    ORDERING IS LOAD-BEARING: the drop rule is consulted BEFORE infeasibility
//    may be declared, because a bound pinned by the ratio test commonly blocks a
//    shift from closing.
//
//    ON kInfeasible/kNumericalError the returned x is the FINAL ITERATE the loop
//    stopped at -- no argmin is kept -- and multipliers are CLEARED on both.
//
//    TRUSTWORTHY RANGE, both directions, known and accepted:
//    (i) FALSE kInfeasible on a feasible row whose residual at the classified
//        point still clears kStructuralResidualFrac. COVERED, on both paths, by
//        The verdict-site face refinement below. STILL UNCOVERED: any (i) case
//        whose face the refinement cannot close inside its budget.
//    (ii) FALSE kOptimal where a genuine contradiction's gap hides beneath
//        kStructuralResidualFrac*dual_mu*|lambda|. Tightening the fraction to
//        catch it re-breaks (i). The SQP driver is the second detection layer;
//        a caller using the engine alone should scale its rows or shrink
//        dual_mu. On the independent working set a contradiction that rides a
//        DEPENDENT row is read by the dependency test instead and ends
//        kInfeasible on a Farkas certificate (E2 fix6's grey re-read closes
//        objective_inflated_lambda_qp(1e-3, 3e-4)); (ii) stands for the rest.
//    (iii) FALSE kNumericalError where a free variable's TRUE optimum lies
//        beyond unbounded_artifact_scale(). Tune primal_delta rather than treat
//        this kNumericalError as load-bearing.
//
//    The verdict-site face refinement runs between the two: once the
//    classification has said kInfeasible, the closed working face is refined
//    further -- against the same unregularized system the candidate solve
//    targets, but to a target stated in ROW units. Which twin runs IS DECIDED BY
//    PROVENANCE, NOT BY ws_algebra: EqpResult::refine_steps is >= 1 on every
//    bordered candidate and identically 0 on every eliminated one, so it is an
//    exact witness of the path that produced the candidate. The refined point is
//    adopted only if the face CLOSES, and it is then both what the classifier
//    re-reads and what a kInfeasible exit returns. An ADOPTING dead end re-enters
//    the ordinary would-be-kOptimal path and may continue the walk.
//
// 6. Trust-region soft bounds -- an l-infinity trust region around the current
//    SQP iterate, expressed the same way every other bound is.
//
//    EFFECTIVE BOUNDS, computed ONCE about the clamped seed primal x0, BEFORE
//    the seed's bound-state hints are materialized:
//        lo_eff(i) = max(lower(i), x0(i) - Delta),
//        up_eff(i) = min(upper(i), x0(i) + Delta),   Delta = opts.tr_radius.
//    Every subsequent bound read sees lo_eff/up_eff via a SHADOWED QpProblem
//    reference, so every function taking `const QpProblem &qp` is TR-aware.
//
//    Crossed effective bounds cannot happen: step 0 already rejected a crossed
//    real box.
//
//    THE kFixed FLIP. lo_eff(i) == up_eff(i) can only happen at Delta == 0 for a
//    variable not already genuinely kFixed. Every such variable is flipped to
//    kFixed before the loop runs, with tr_active set; an already-kFixed real
//    bound is left alone.
//
//    tr_active (size n, parallel to bound_state) IS A Separate activity set, not
//    a new BoundState: true at i iff the ratio test, the temporary-vertex repair
//    or the zero-radius flip pinned i at the TR side of its effective bound
//    rather than the real one (a coincidental tie is attributed to the real
//    bound). Applied once at the end of run(), on the FINAL working set only:
//      (a) a TR-pinned variable's bound_state reports kFree;
//      (b) its z entry is forced to 0 -- the loop still sees and acts on the
//          real priced multiplier, but that number is never exposed;
//      (c) none of this changes what the loop DID, only how the final ws/z are
//          reported.
//
//    Window-consistency rule. A seeded bound-state hint is applied against the
//    window, and a hint whose bound falls outside [lo_eff(i), up_eff(i)] is
//    DROPPED -- index i arrives kFree and the loop re-derives its activity.
//    Clamping such a hint to the window edge instead would break bound_state's
//    meaning; tr_active is the documented channel for a TR-tight edge. A hint
//    INSIDE the window is honoured unchanged.
//
//    Warm-start seed ingestion ignores tr_active BY CONSTRUCTION, so a variable
//    TR-pinned on the solve that produced `seed` already arrives kFree and the
//    TR pin is not carried into the new solve's working set.
//
//    Hot-start reuse interaction (border mode). K0's hashes never depend on
//    lower/upper, so a bound change can NEVER poison K0 reuse on fingerprint
//    grounds. What it CAN do is change ws.bound_state() at the seed, which reuse
//    condition (b) already treats as an ordinary working-set change.
//
//    Per-solve radius variation. tr_radius lives on QpOptions, which is
//    per-instance const, but every solve() overload also takes a
//    `const SolveOverrides &` resolved ONCE at the top of run(); every read site
//    consults those effective values. A shrink-radius retry loop therefore
//    shares one QpEngine across every retry radius and keeps hot-start reuse
//    wherever the ordinary conditions allow.
//
//    Unbounded-artifact guard and repair synergy. is_runaway() and
//    repair_temporary_vertex() both read bounds through the same shadowed `qp`,
//    so with a finite tr_radius is_runaway() can never fire for a TR-bounded
//    variable and the repair's "no finite bound to pin" failure cannot occur.
//
//    Bit-identical off path. At the +inf default no QpProblem copy and no
//    effective-bounds vector is materialized, and the shadowed `qp` aliases the
//    caller's problem directly. QpSolution::tr_active is still allocated
//    unconditionally on every solve, all false.
//
// 6b. The export invariant on z: a free variable carries no bound price.
//    For every index i, on EVERY status this engine can return:
//        bound_state[i] == kFree  =>  z(i) == 0.0
//    Enforced at the point of export in run(). `refine_on_face` satisfies the
//    same invariant by a DIFFERENT route; a third producer must re-derive it
//    rather than assume it is inherited.
//
//    ONE-WAY GUARANTEE: a PINNED index's z is the price the last price() call
//    computed, which on a kMaxIter exit may be one working set stale. `kFixed`
//    is silently outside this invariant.
//
// --- Degeneracy ---
//
// With QpOptions::independent_working_set (default) the working set is kept
// linearly independent (qp/ws_independence.h): an entry detector runs on the
// first factorization and after untested growth or an over-count (Debug
// asserts that an over-count is always found), and each ratio-test blocker is
// tested against the pass's factor (a ride's too: 4c, THE DECLINED RIDE). A
// dependent blocker is EXCHANGED for a member (ExchangePick; Gill & Wong 2015,
// Result 3.4(b)); with no admissible
// member: a verified EXACT Farkas certificate (multipliers refined mu-free,
// residual within the rounding floor kCertExactKappa) ends kInfeasible
// (QpSolution::farkas), a verified TOLERANCED one (residual above the floor,
// not a proof for this QP) ends kNumericalError with the certificate kept, else the
// blocker is passed over when its predicted violation stays within
// feas_tol * row_scale, else the solve ends kNumericalError at the current
// iterate. Before any exchange, on an ordinary step, THE IMPLIED BLOCKER (M7
// W2 QC-8; QpEngine::blocker_implied_by_face): a blocker PROVED dependent
// (rho <= kDepTol, no shift-driven row involved) whose face value satisfies it
// (g + rho ||a|| ||x_face,free|| <= feas_tol * row_scale) cannot be crossed by
// the exact step, so its computed rate is the step's leak off the members; it
// is passed over while its predicted violation stays within the members'
// leak allowance (dep_implied_passovers), and otherwise falls back to the
// exchange it set aside, the unlock, then the end -- never a new end.
// The zero-motion re-entry rule (WalkSeen's point memory) makes a
// constraint released and re-entered at one point drop-exempt until the
// iterate moves; an exempt multiplier above opt_tol * stat_scale at the end
// is kNumericalError (zero_motion_exhausted). The integer screen can call an
// independent candidate dependent when the MANDATORY rows are dependent
// (counted: dep_screen_under_mandatory_dependence; the safe direction). A
// blocker the test leaves UNDECIDABLE (screen-negative, a stalled refinement)
// is decided exactly by a structural
// dependency when one exists (dep_structural_exact); else, on a border-stack
// pass, it takes the border retreat below; else it is admitted and the next
// pass's detector tests the grown set (dep_deferred_adds). E2 FIX7 (settler
// E4-37(b)): a test that did not RUN is not one of those. With no factor for
// this pass's set nothing is computed -- rho is the NaN default and solves is
// 0 -- so the verdict is the integer screen's alone: positive it is an exact
// proof of dependence, negative the blocker is an ORDINARY ADD, the pre-E2
// admission (S17), counted dep_unrun_tests. It is never a deferred add: that
// path charges the next pass to a re-check and marks the member
// known-dependent if the re-check removes it, on the strength of a rho that
// was never computed. A pass can reach the add site with no factor after
// repair_temporary_vertex FAILS -- see E2 FIX8 below, which stops the walk
// running such a pass at all. A stalled test with
// rho >= kDepUndecidable is admitted as independent (dep_stall_admits). A deferred add that
// re-check removes is known-dependent at that point and is not re-admitted
// there on an undecided test (dep_known_dependent_refusals). THE BORDER
// RETREAT (once per solve, dep_border_retreats): when the rule would end the
// solve, or a blocker's test is undecidable, on a border-stack pass, the pass
// is redone on the bound-eliminated path. When K0
// is near-singular on the pinned directions its Schur complement carries O(1)
// rounding, the border step leaves its face, and the dependency coefficients
// are inaccurate (rho = 1.4e-3 and 0.34 measured where the exact value is 0):
// finding E2-F1, a limit of the border path's accuracy, not of the rule.
// E2 FIX3 (the E2-D4 rules): (R-A) an exact structural dependency decides a
// candidate before any numerical "independent" at every growth site
// (dep_structural_first; ws_independence.h); (R-C') a kWrong
// inertia at the start repair or the terminal check is re-read mu-free: kOk overturns it,
// kSuspect only with a second eliminated read at dual_mu reading kOk (dep_inertia_rereads /
// dep_inertia_reread_flips; reread_wrong_inertia). E2 FIX5 (R-B): a zero-motion
// blocker with no admissible member, where the rule would otherwise end the solve,
// unlocks the members locked only by having entered at this point, min-ratio with
// lowest-index ties (dep_unlocks; ExchangePick, where the finiteness argument is).
// E2 FIX6 (R-E, the grey re-read): a candidate test that CONVERGED to "independent" with
// kDepTol < rho < kDepUndecidable is re-read at every growth site (test_candidate, after R-A)
// by running the refinement to its end on the same factor (certificate_coefficients: no
// factorization, at most kDepRefineSteps steps); rho' plus its rounding floor
// (kDepRereadKappa) <= kDepTol makes it DEPENDENT with the re-read coefficients, else the
// "independent" stands (dep_grey_rereads / dep_grey_reread_dependent). Stall, undecidable,
// screen and R-A verdicts are never touched. THEOREM (the safe direction): for every v,
// rho(v) = ||(a - A_W^T v)_free|| / ||a|| >= dist(a_free, span A_W,free) / ||a||, since A_W^T v
// is a point of that span; so a dependent verdict is CERTIFIED at the contract's own kDepTol
// and R-E can never call dependent a candidate farther than kDepTol from the span. Why the
// converged grey reading is not a distance: the test stops on the KKT residual, which sees the
// mu-solve's A_W u = mu v, while rho reads (H + delta) u -- amplified by ||H|| and by a
// rank-deficient A_W, which the residual does not see. The stall class is left alone: it is
// truly independent on the measured corpus (R-D retired, settler S34).
// E2 FIX8, THE BOUNDED REPAIR (two rules; settler S40).
// (i) THE INTEGER SCREEN MAY NOT SKIP A REPAIR PIN. me + |W| + 1 > n proves the FAMILY
// {A_W, e_i} dependent; it proves e_i in span(A_W) -- which is what "a dependent pin leaves
// null(A_W), and so the inertia, unchanged" needs -- only when A_W is itself independent,
// which a rank-deficient equality Jacobian denies (M5's Q1). At the add site the screen's
// reading is the SAFE direction; in the repair it is the unsafe one, because a skipped pin
// that would have cut the negative curvature costs the solve. So a screen-positive candidate
// is decided by the EXACT structural dependency alone and is otherwise TRIED
// (dep_repair_screen_pins), under a MEASURED BUDGET on its pins, which bounds its probe
// factorizations at twice that (repair_pin_budget below, with the distribution the bound
// comes from); a repair that stops on the budget fails, counted dep_repair_budget_stops. Measured
// on ACOPR118's majors 17-20: the screen fired after 70 pins and skipped the remaining 245, where
// pin 75 reads kOk and both algebras then certify at the objective the option-off walk reaches.
// (ii) F7-1, A FAILED REPAIR DOES NOT LEAVE A PASS RUNNING ON WHAT IT LEFT BEHIND. The
// repair restores ws and x but not `face` and the border stack, which describe the last set
// it PROBED; the walk used to fall through into the same pass and run every dependency test
// at its add site with no factor that holds its working set -- which is how fix7's un-run
// test was reached. When the repair took away a factor view the pass HELD (dep_factor_view
// usable before the repair, unusable after), that pass is now SPENT (dep_repair_restarts),
// and the next one re-establishes the factor by the ordinary route: a fresh factorization on
// the eliminated path, sync_borders on the border path. Nothing is lost, because a failed
// repair moved neither ws nor x; a repair that never probed, or a pass that had no view to
// lose, runs on as before. GATED on
// independent_working_set: what a stale `face` costs is a dependency test with nothing to
// test against, and only this walk takes one, so the option-off walk stays byte-identical --
// which its role as the declared CONTROL arm of every E2 re-take requires.
// THE STANDSTILL RETREAT (E2 fix9; standstill_retreats). On a border-stack pass of the
// independent walk whose inertia reads kWrong while the iterate carries a structural
// equality violation, a zero-motion pass -- or the second of two consecutive full steps (no
// blocker, alpha >= 1, no ride) -- takes THE BORDER RETREAT and arms the re-check. Such a
// walk cannot reach a KKT point of its face: the rows it stands off are the face's own, and
// kWrong on the border path is that factor's failure signature (exactly rank-deficient
// faces measured with a positive-definite reduced Hessian). A kOk standstill is left alone;
// any non-qualifying pass resets the full-step run. Once per solve, like every retreat.
// E2 FIX4: a terminal point that would
// read kInfeasible by its structural violation is first re-solved mu-free on its face and
// adopted when it satisfies every row and bound (terminal_mu_refines /
// terminal_mu_refine_rescues; refine_terminal_mu_free). With
// the option off, the delta/mu regularization keeps a dependent KKT
// factorizable and the dependent rows split the multiplier; a degenerate
// stall is bounded by max_iter.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
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
// The walk's READ-ONLY test seam (docs/testing.md; CLAUDE.md §8). It compiles
// to NOTHING without HVEN_TESTING, and it is included HERE rather than in
// src/qp/qp_engine.cpp so that that TU gains no line and its one `assert`'s
// __LINE__ immediate cannot move -- see qp_walk_trace.h's own note.
#include <hven/detail/qp/qp_walk_trace.h>
#include <hven/detail/qp/working_set.h>
#include <hven/detail/qp/ws_independence.h>
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
// The Farkas certificate's rounding floor (settler S25'): its residual is EXACT
// iff <= kCertExactTol = kCertExactKappa * (n + m) * eps * ||y||_1 * max_k ||a_k||
// (QpFarkasCertificate::exact_tol). Standard model, fl(a op b) = (a op b)(1 + d),
// |d| <= eps: box_part = s c - Ae^T y_e - Ai^T y_i sums at most m + 1 products
// per entry, so its computed error is <= (m + 1) eps ||y||_1 max_k ||a_k||; y is
// the mu-free-refined solution of the (n + m)-dimensional KKT system, whose
// backward error is O((n + m) eps) relative to the same product. (n + m) covers
// both; kappa = 10 is the safety factor. ||y||_1 counts the blocker's own unit
// coefficient, max_k runs over c and the rows with y_k != 0, m = me + |W|.
constexpr double kCertExactKappa = 10.0;
// Fraction of the regularization footprint dual_mu*|lambda| that a violation
// must reach before it counts as STRUCTURAL rather than solver noise
// (condition (b); see violation_is_structural).
constexpr double kStructuralResidualFrac = 0.1;
// Hard floor, relative to the row's own scale, beneath the verdict-site face
// refinement's per-row target (see QpEngine::face_row_target). A residual
// flattens out at ~1e-16 of the row's scale and then oscillates in the last
// bit, so this stops the loop rather than spending Schur solves on rounding
// noise. It is a backstop, not the rule: at any sane feas_tol the classifier's
// own tolerance is orders above it and is what actually ends the loop.
constexpr double kVerdictRefineRelFloor = 1e-14;
// THE TERMINAL MU-FREE REFINEMENT (E2 fix4; refine_terminal_mu_free): the
// iterative-refinement steps taken on the dual_mu = 0 bound-eliminated factor
// BEYOND solve_eqp's own mandatory one, each kept only while the unregularized
// KKT residual strictly falls. Two: the mu-free solve is already accurate to
// O(primal_delta) and the steps only polish that; a face they cannot close is
// left to the existing classification.
constexpr Index kTerminalMuRefineSteps = 2;

// THE TERMINAL POLISH's refinement budget (M7 W2 E4 fix1, ruling E4-11;
// QpEngine::polish_terminal_point, and polish_eqp_unregularized which takes
// them). Steps against the FULLY unregularized residual -- dual mu and the
// primal delta both backed out -- on the polish's own mu-free factor, each kept
// only while that residual strictly falls. TWO, for the same reason
// kTerminalMuRefineSteps is two: the mu-free solve of the final face is already
// accurate to O(primal_delta * ||x* - x_k||) and each step contracts that by
// another factor of order primal_delta / lambda_min(Z'HZ), so two steps take a
// 1e-08-sized bias past 1e-16 wherever they converge at all, and a face they
// cannot close in two they will not close in ten (the guard stops them at the
// first).
constexpr Index kTerminalPolishSteps = 2;

// THE TERMINAL POLISH's ENTRY TEST (polish_footprint_open), as a multiple of
// feas_tol: the worst COMPLEMENTARITY PRODUCT at a would-be kOptimal point --
// |slack|/row_scale times |multiplier|, over the working rows and the pinned
// bounds (detail::terminal_kkt) -- that must be reached before the polish's
// stage-2 factorization is spent.
//
// THE PRODUCT IS THE RIGHT QUANTITY because it is what a CONSUMER reads. The
// regularized row identity leaves a working row slack by dual_mu*|lambda|, and a
// driver's own KKT check prices that slack by lambda again, so what it sees is
// dual_mu*lambda^2 -- the B-1 term. A footprint invisible in that product is
// invisible, full stop.
//
// A HUNDRED, i.e. 1e-07 at the QpOptions defaults, which is ONE DECADE beneath
// the 1e-06 kkt_tol both drivers default to (SqpOptions, IpmOptions): a product
// below it cannot move any consumer's complementarity check even after the
// driver's own scaling, and one above it can. MEASURED, on the 27-problem HS
// battery at kSchurBorder, and the calibration is why the factor is not 1:
//
//   factor    factorizations   stage-2 solves   stage-2 adoptions
//   1         432              145              58
//   100       288              1                1
//
// -- at a factor of 1 the gate fired at nearly every certified exit for a 40 %
// adoption rate and cost 145 factorizations against a pre-E4 baseline of 320,
// and EVERY fixture it fixed is green at 100 as well. (288 is BELOW that 320
// baseline, because the EQP-site polish removes the extra pass per face the
// proximal answer's O(primal_delta) bias used to cost.)
//
// A THIRD DISJUNCT WAS MEASURED AND REMOVED: the DUAL FOOTPRINT
// (mu_footprint_open: a working row whose dual_mu*|lambda| exceeds its own
// tolerance) is this product's square root and fires two decades wider for
// nothing a consumer sees -- at a factor of 1 with that disjunct in place the HS
// battery spent 145 stage-2 solves for 58 adoptions.
constexpr double kTerminalPolishComplFactor = 100.0;

// THE ENTRY TEST's SECOND DISJUNCT (polish_footprint_open): the multiple of
// opt_tol a terminal point's free-block STATIONARITY residual, relative to its own
// scale, must reach before stage 2 is spent.
//
// THIS IS E4-11's U0 TARGET, and it is the one quantity the EQP-site polish
// cannot reach on its own. The KKT-point test of section 5 stops as soon as that
// residual is within opt_tol * scale, which is correct and is where it should
// stop; the walk it replaced kept stepping until ||p|| fell under step_tol and on
// the F7 bound family that was four decades further (U0's `kkt_stationarity`
// column: 9.19e-14 at the M6-close pin against 9.20e-10 at the tolerance). One
// mu-free re-solve of the final face reaches 1e-16 there, so the polish buys back
// the accuracy for ONE factorization instead of the extra passes.
//
// 1e-3, i.e. THREE DECADES of the point's own tolerance left to gain. Below it
// there is nothing worth a solve: a face already at 1e-6 of its tolerance is at
// rounding or is flat, and a flat face's residual is one no re-solve removes (the
// polish's strict-decrease guard rejects it at both sites). The COST of this
// disjunct lands in `terminal_polish_solves` and NOT in `factorizations` -- see
// that counter's note, which is also why this disjunct is affordable at all.
constexpr double kTerminalPolishStatFactor = 1e-3;

// THE TERMINAL POLISH'S ACCEPTANCE SLACK (M7 W2 E4 fix round 2), on each of the
// three KKT residuals: a candidate may leave one of them up to this factor WORSE,
// and must leave one of them better BY this factor, before it is adopted.
//
// WHY IT IS NOT 1 (an exact `<=`, which is what fix round 1 shipped). On
// `ACOPR57_def`/qp0 the mu-free candidate improves the worst row residual from
// 6.14e-07 to 1.20e-14 -- SEVEN DECADES -- and lands its stationarity at 1.80e-14
// against the incumbent's 1.64e-14, which is one ulp of that quantity in the wrong
// direction. Under an exact test that single ulp decides the adoption, and WHICH
// SIDE of it the two numbers fall on is not a property of the algorithm: MEASURED,
// the production library adopts and the HVEN_TESTING recompile of the same TU
// (tests/CMakeLists.txt's walk-seam target) does not, and the exported objective
// then differs by 3.2e-05 relative between two builds of one source. A factor
// makes the decision depend on the decades and not on the ulp.
//
// 4, and it is a ROUNDING factor rather than a tuning knob: it is two bits, far
// below the decade a polish that is worth its factorization moves, and far above
// the last-digit noise two builds of the same expression can differ by. The
// "strictly better" half is strengthened with it in the same motion -- a candidate
// must now improve a residual by a FACTOR OF 4, where before any improvement
// however small was enough -- so the slack does not widen adoption: it moves the
// decision from a tie-break to a margin on both sides.
//
// AND IT APPLIES TO THE TWO DUAL RESIDUALS ONLY. The worst ROW/BOUND violation is
// compared exactly, because feasibility is the one thing the polish may never
// trade: MEASURED, with the factor on the row half as well a candidate up to 4x
// LESS feasible than the incumbent is adoptable, and on an elastic relaxation whose
// incumbent rows sit at 1e-3 that is enough for the exported point to read a
// STRUCTURAL violation -- the T7 feasible census went to 16 open relaxations
// certified infeasible against its own claim of 0.
constexpr double kTerminalPolishWorseFactor = 4.0;

// THE STEP'S ROW-DRIFT FLOOR (M7 W2 E4 fix round 2), relative to a row's own
// scale: below it a drift off a closed working row is not refused.
//
// WHY THERE IS A FLOOR AT ALL. The refusal routes into the suspect-stall ladder,
// which is GLOBAL by design -- it raises `primal_delta` for the whole remaining
// solve and discards the border state -- so a refusal is not a cheap observation
// and a check that fires on ordinary accuracy changes every route downstream of
// it. MEASURED without a floor: three driver fixtures lose their premises to a
// solve that ran at 1e-05 primal_delta after three rungs, one of them
// (`WarmStart.FullStepModeInheritsElasticAndRestoration`) reading
// `suspect_escalations` 3 against its pinned 0 and never reaching the restoration
// row it exists to test.
//
// 1e-3 OF THE ROW'S SCALE. The pathology is a drift of ORDER ONE relative to the
// scale -- `dual_mu * |lambda|` with `|lambda| ~ 1/primal_delta`, which is 1e-08 /
// 1e-08 = O(1) at the defaults, and 0.5 on the fixture that found it
// (`indefinite_equality_qp`, reduced Hessian exactly 0 on the face). Three decades
// beneath that and two above `face_row_target`, so the check still sees anything
// the regularization's own term can produce and says nothing about a solve that is
// merely a few decades short of exact.
constexpr double kEqpRowDriftFloor = 1e-3;

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

// THE CONVEX CERTIFICATE's structural test (M7 W2 NC4; section 4c). True iff every
// STORED entry of H is exactly 0.0 off the diagonal and >= 0 on it: an exact proof,
// with no tolerance and no factorization, that H is positive semidefinite on all of
// R^n -- an LP is the case H == 0. A NaN entry fails. O(nnz(H)).
inline bool hessian_is_nonneg_diagonal(const SpMatRM &H) {
    for (Index r = 0; r < H.outerSize(); ++r) {
        for (SpMatRM::InnerIterator it(H, r); it; ++it) {
            if (it.row() == it.col() ? !(it.value() >= 0.0) : it.value() != 0.0) {
                return false;
            }
        }
    }
    return true;
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

// --- The temporary-vertex repair's BUDGET (E2 fix8, settler S40) -----------
//
// `repair_temporary_vertex` pins free variables one at a time, probing the inertia after
// each, until the gate reads kOk or the candidates run out. Until E2 fix8 the INTEGER
// SCREEN (me + |W| + 1 > n) skipped every candidate once it fired, which bounded the loop
// by accident and at the wrong place: the screen proves the FAMILY {A_W, e_i} dependent,
// not e_i in span(A_W), and those differ exactly when A_W is rank-deficient -- which a
// rank-deficient equality Jacobian makes routine. Removing that accident needs a REAL
// bound, and this is it:
//   pin budget = max(kRepairPinFloor, ceil(n / kRepairPinDivisor))
// It bounds the PROBE FACTORIZATIONS as well, by construction: the pin loop probes once per
// pin and the release pass at most once per pin, so a repair probes at most twice its pin
// budget, and never more than 2n.
//
// MEASURED, two populations. (a) 838 repairs replayed from dumps -- the 51-dump E2-D4 corpus
// on both algebras, the whole HS sweep, and ACOPR118's majors 17-20 on both algebras: 832
// succeed, and the pins a SUCCESSFUL repair tries are p50 1, p90 1, p95 2, p99 134, max 161;
// the largest fraction of n any of them used is 75/344 = 0.218, then 161/824 = 0.195. (b) The
// repairs IN-CELL on the hven-sqp lane's `default` ACOPR118 and ACOPP118 cells, with the
// repair unbounded (scratch instrumentation): 20 repairs, all succeed, the largest 118 pins at
// n = 344 = 0.343 -- each cell's first, cold subproblem -- the next 99. Pins scale with n, so
// the bound is a fraction of n: 1/2 clears the largest measured fraction by 1.46x (an
// earlier 1/3, derived from (a) alone, cut that 118-pin repair in both cells and moved both
// trajectories). Below n = 16 the floor governs; there the largest measured is 4 pins at
// n = 5, which the floor of 8 clears by 2x (n = 2's one pin by 8x). NOTHING MEASURED IS CUT:
// the budget exists to bound the pathology the screen used to bound by accident (an unbounded
// repair can spend n probe factorizations, and n more releasing, on one attempt). A repair
// that stops on it is counted QpCounters::dep_repair_budget_stops and FAILS, with every
// consequence a failed start repair has -- on a truly indefinite face the walk has no way
// off it and ends kNumericalError, which is what the screen produced whenever it aborted a
// repair. THE COST WHEN IT BINDS is therefore real, and pinned
// (QpE2Fix8Budget in tests/qp/test_qp_unrun_dependency_test.cpp): a fraction of n cannot see
// the INERTIA DEFICIT, so a QP needing more pins than the budget -- H = -I on n = 12 needs
// all twelve, its budget is 8 -- is cut where the unbounded repair would have succeeded. None of
// the measured repairs is; a deficit-aware budget is registered. THE BUDGET IS
// independent_working_set ONLY: the option-off walk has neither screen nor dependency test, so it
// never had the accidental bound this replaces, and bounding it now would restrict the declared
// CONTROL arm of every E2 re-take. That its repair is unbounded is registered.
constexpr Index kRepairPinFloor = 8;
constexpr Index kRepairPinDivisor = 2;
inline Index repair_pin_budget(Index n) {
    const Index fraction = (n + kRepairPinDivisor - 1) / kRepairPinDivisor; // ceil(n / 2)
    return fraction > kRepairPinFloor ? fraction : kRepairPinFloor;
}

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

// THE TERMINAL POLISH's three KKT residuals (M7 W2 E4 fix1, ruling E4-11), all
// three RELATIVE to their own scales so that "worse" is a comparison between
// two points of the same face and not between two units:
//
//  - `stationarity`: free_block_stationarity divided by the scale that function
//    reports, i.e. the quantity the KKT-point test and the suspect-stall gate
//    both read, in units of opt_tol.
//  - `rows`: the worst row (equality, inequality) or bound violation divided by
//    that row's own scale -- worst_structural_violation's row_scale and
//    refine_terminal_mu_free's box rule, in units of feas_tol.
//  - `complementarity`: the worst |slack| * |multiplier| product over the WORKING
//    rows and the PINNED bounds, the slack relative to the row's scale. It is
//    the quantity the regularization's footprint shows up in and the one the B-1
//    family reads at the NLP level: a regularized row identity leaves a working
//    row slack by dual_mu*|lambda|, so this product is dual_mu*lambda^2 there
//    and the driver's own complementarity check sees it.
struct TerminalKkt {
    double stationarity = 0.0;
    double rows = 0.0;
    double complementarity = 0.0;

    bool all_finite() const {
        return std::isfinite(stationarity) && std::isfinite(rows) && std::isfinite(complementarity);
    }
};

inline TerminalKkt terminal_kkt(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                const Vec &lambda_e, const Vec &lambda_i, const Vec &z) {
    TerminalKkt out;
    double stat_scale = 1.0;
    const double stat = free_block_stationarity(qp, x, ws, lambda_e, lambda_i, stat_scale);
    out.stationarity = stat / std::max(stat_scale, 1e-300);

    const double xmag = x.lpNorm<Eigen::Infinity>();
    if (qp.me() > 0) {
        const Vec resid = qp.Ae * x - qp.be;
        for (Index j = 0; j < qp.me(); ++j) {
            const double row_scale =
                std::max({1.0, std::abs(qp.be(j)), qp.Ae.row(j).cwiseAbs().sum() * xmag});
            const double rel = std::abs(resid(j)) / row_scale;
            out.rows = std::max(out.rows, rel);
            out.complementarity = std::max(out.complementarity, rel * std::abs(lambda_e(j)));
        }
    }
    if (qp.mi() > 0) {
        const Vec resid = qp.Ai * x - qp.bi;
        for (Index j = 0; j < qp.mi(); ++j) {
            const double row_scale =
                std::max({1.0, std::abs(qp.bi(j)), qp.Ai.row(j).cwiseAbs().sum() * xmag});
            out.rows = std::max(out.rows, std::max(0.0, resid(j)) / row_scale);
            // In the working set? The list is index-ordered, and this is the same
            // binary search QpEngine::in_working runs on the same vector (a private
            // static this free function cannot name).
            const auto &aw = ws.active_ineq();
            if (std::binary_search(aw.begin(), aw.end(), j)) {
                out.complementarity = std::max(out.complementarity, std::abs(resid(j)) / row_scale *
                                                                        std::abs(lambda_i(j)));
            }
        }
    }
    for (Index i = 0; i < qp.n(); ++i) {
        const double below = qp.lower(i) - x(i);
        const double above = x(i) - qp.upper(i);
        const double bound = below >= above ? qp.lower(i) : qp.upper(i);
        const double row_scale = std::max({1.0, std::abs(bound), xmag});
        out.rows = std::max(out.rows, std::max(below, above) / row_scale);
        const BoundState st = ws.bound_state()[static_cast<std::size_t>(i)];
        if (st != BoundState::kFree) {
            const double at = st == BoundState::kAtUpper ? qp.upper(i) : qp.lower(i);
            out.complementarity =
                std::max(out.complementarity, std::abs(x(i) - at) / row_scale * std::abs(z(i)));
        }
    }
    return out;
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
// SqpWarmStart::structure_hash's "no claim made" sentinel.
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
    /// @param threads The owning engine's thread count, handed to K0's factor
    ///                at construction (M6 W5 T8.8). 0 -- the default, and what
    ///                every construction site passed before that task -- leaves
    ///                the backend's own default alone, so a default-argument
    ///                construction is bit-for-bit the old one.
    explicit BorderState(int threads = 0) : kkt(threads) {}

    BorderState(const BorderState &) = delete;
    BorderState &operator=(const BorderState &) = delete;
    BorderState(BorderState &&) = delete;
    BorderState &operator=(BorderState &&) = delete;

    KktAssembly k0;
    std::vector<Index> k0_rows;
    std::vector<BorderLedgerEntry> ledger; // in SchurComplement::add_border order
    detail::KktFactor kkt;                 // configured by sqp_kkt_options(threads)
    std::optional<SchurComplement> schur;
    bool latched = false;

    // Stale-handle detection lives in the backend factor's own identity:
    // `analyze()` moves `kkt.factor.session_id()` and every successful
    // `factorize()` advances `kkt.factor.epoch()`, so rebuild_k0() stamps
    // nothing. See QpEngine::run()'s reuse condition (e).
};

// The face the elimination path'S `kkt` CURRENTLY HOLDS. Captured where
// solve_eqp factorizes, read at the verdict site so the face refinement can
// REUSE that factorization instead of buying its own.
//
// The key follows the factor, and both halves are necessary: WHICH SYSTEM was
// factorized (the elimination partition and the working rows, both recorded
// WHOLE rather than as counts, since refresh_shifts adds rows and drop_worst
// removes them without changing n) and WHICH FACTORIZATION `kkt` is holding now
// (the factor's own (session_id, epoch), which it advances inside its own
// factorize path, so every writer is covered by construction rather than by an
// enumerated list).
//
// `factorized` is the entry condition, not part of the signature: it is true
// only on an iteration whose candidate came from solve_eqp on THIS `kkt`. A
// bordered candidate leaves it false, and so does solve_eqp's
// empty-reduced-system short-circuit.
struct EliminatedFace {
    bool factorized = false;
    std::uint64_t session_id = 0;
    std::uint64_t epoch = 0;
    std::vector<BoundState> bound_state;
    std::vector<Index> rows;

    // Assignment reuses the vectors' capacity, so steady state allocates
    // nothing.
    void capture(const WorkingSet &ws, const detail::KktFactor &kkt) {
        bound_state = ws.bound_state();
        rows = ws.active_ineq();
        session_id = kkt.factor.session_id();
        epoch = kkt.factor.epoch();
        factorized = true;
    }

    bool holds(const WorkingSet &ws, const detail::KktFactor &kkt) const {
        return factorized && session_id == kkt.factor.session_id() && epoch == kkt.factor.epoch() &&
               bound_state == ws.bound_state() && rows == ws.active_ineq();
    }
};

// Hot-start level. The opaque handle behind warm_start.h's SqpWarmStart::hot,
// forward-declared there and DEFINED here: a frozen copy of the
// fingerprint/exit-state members QpEngine::run() tracks per instance, plus
// shared ownership of the BorderState those fingerprints describe.
//
// OWNERSHIP. BorderState wraps a move-only SymmetricFactor by value and is
// itself non-copyable and non-movable; `border_` is a shared_ptr and
// HotState::border is a COPY of it, so the backend session is released exactly
// once and an engine adopting a handle can factorize against it even if the
// producing engine is gone.
//
// Lifetime safety alone does not guarantee the contents still match A HOLDER'S
// FROZEN FINGERPRINT. Two mechanisms close that:
//   - DETACH at a refused-reuse site whose `border_` is shared (a sole-owned
//     object is wiped in place instead, keeping its symbolic analysis reusable)
//     is the LOAD-BEARING fix: the only engine that can write into a SHARED
//     BorderState is one whose own conditions (a)-(e) already passed for it.
//   - The factor's IDENTITY pair plus this engine's own committed copy is
//     DEFENSE-IN-DEPTH: condition (e) compares a frozen pair against a LIVE read
//     on every solve() call, and its usable-numerics conjunct closes the
//     failed-rebuild case. A mismatch degrades to kWarm silently, never a throw.
//
// Concurrency, not sequencing, is what remains unsafe. Same-process only:
// HotState is never serialized.
//
// AFTER AN ELASTIC/SOC RE-SOLVE: an ELASTIC re-solve builds an AUGMENTED K0, so
// a handle emitted right after it silently forfeits kHot for the next major; an
// SOC re-solve shifts only be/bi, so a post-SOC handle remains an ordinary
// (a)-(e)-gated candidate.
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

    // The OPTIONS FINGERPRINT of the engine that emitted this handle:
    // options_fingerprint(opts_, threads) over every field of its QpOptions plus
    // the thread count in force (qp_types.h). DISTINCT FROM kkt_session_id
    // above, which names the FACTOR SESSION; this one names the SETTINGS the K0
    // was built under, which conditions (a)-(e) never look at.
    //
    // run() adopts a handle only when this equals the adopting engine's own
    // fingerprint. A rebuilt engine with changed QP or thread options therefore
    // refuses the handle and resolves kWarm by construction -- the values and
    // the working set still come from `seed`, which is independent of `hot`. An
    // identical-options rebuild adopts: it is the same factor object built under
    // the same settings, and refusing it would buy nothing and cost a
    // factorization. A FRESH engine with the same options adopts, which is what
    // every cross-engine kHot pin in the tree relies on (M6 W5 T8.3).
    std::uint64_t engine_options_hash = 0;
};

class QpEngine {
  public:
    // `threads` is the thread count in force for this engine's factor paths --
    // SqpSolver passes SqpOptions::common.threads. Since M6 W5 T8.8 this engine
    // both CARRIES it (it is folded into the options fingerprint a hot handle is
    // keyed on -- qp_types.h's options_fingerprint) and APPLIES it: K0's factor
    // takes it here, and run()/refine_on_face()/
    // refine_eliminated_face_for_verdict()'s temporary and fallback factors take
    // it at their own construction. Defaulted so every existing construction
    // site keeps compiling and keeps hashing the 0 the SQP lane has always
    // passed -- and 0 means "leave the backend's own default alone", so a
    // defaulted engine is bit-for-bit the pre-T8.8 one.
    explicit QpEngine(const QpOptions &opts, int threads = 0)
        : opts_(opts), threads_(threads), options_hash_(options_fingerprint(opts, threads)),
          border_(std::make_shared<BorderState>(threads)) {}

    // THE LIVE K0 FACTOR'S OWN COUNT, read through to the backend session --
    // NOT the carried `threads_` this engine was built with (M6 W5 T8.8). The
    // two agree by construction on an engine solving through its own border,
    // and tests/drivers/test_threads.cpp asserts they do; they are nonetheless
    // different facts, and this accessor exists to make the FACTOR's answer
    // observable at the boundary. After a hot handle is adopted, `border_`
    // is the PRODUCING engine's BorderState -- adoption requires an equal
    // options fingerprint, which folds `threads`, so the answer still agrees.
    int num_threads() const noexcept { return border_->kkt.factor.num_threads(); }

    // The thread count this engine was BUILT with -- the value hashed into
    // options_hash_. Distinct from num_threads() above; see it.
    int carried_num_threads() const noexcept { return threads_; }

    // Attach a ledger for instrumentation (nullptr = off, default off).
    // Emits one SolveRecord per solve() call with the given label prefix
    // and a per-solve counter appended. If solve() throws (e.g., from
    // qp.validate() or KKT factorization failure), no record is emitted
    // and solve_counter_ is not incremented, so the next successful solve
    // gets the same label it would have received if the failed solve had not
    // been attempted.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

    // The per-solve record counter this engine has reached, and the way to hand
    // it to a REPLACEMENT engine. Exists for exactly one caller: SqpSolver::
    // set_options(), whose transactional rebuild throws this engine away. The
    // labels are a per-driver sequence, not a per-engine one, so a rebuild that
    // let the counter restart would put a second `<prefix>_qp_0` in a ledger
    // that already holds one. Not otherwise part of the engine's surface.
    Index solve_counter() const noexcept { return solve_counter_; }
    void adopt_solve_counter(Index n) noexcept { solve_counter_ = n; }

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
    // run(), into the effective values every read site in qp_engine.cpp actually
    // consults.
    QpSolution solve(const QpProblem &qp, const SolveOverrides &overrides) const;

    // Warm start with a per-solve override, combining both of the above.
    QpSolution solve(const QpProblem &qp, const QpSolution &seed,
                     const SolveOverrides &overrides) const;

    // Warm start with a per-solve override AND a hot handle
    // (SqpWarmStart::hot) from a PRIOR solve -- typically on a DIFFERENT
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
    // SqpWarmStart::hot on every exit. Returns nullptr whenever border_valid_ is
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

    // Tier 3: exact refinement on an externally identified face.
    //
    // ONE exact equality-constrained solve on the face `face` names, plus this
    // engine's ordinary iterative-refinement step -- `solve_eqp`, reached
    // WITHOUT a walk. A kernel that IDENTIFIES an active set to its own
    // tolerance hands that set here and gets back the point the set determines
    // EXACTLY, where an FB kernel bounds its own complementarity only by
    // `fb_tol * ||lambda||inf`.
    //
    // GATED: the refined point must be a legal answer to the SUBPROBLEM --
    //   finite, inside the real box, inside the trust region, and satisfying
    //   every inequality row NOT on the face to the row-scaled feasibility
    //   tolerance. On failure this function REFUSES and the caller keeps the
    //   certificate it already had.
    // NOT GATED: the sign of the refined multipliers. The face is the CALLER's;
    //   this function re-solves it exactly and does not re-judge it.
    //
    // The rank pre-screen runs before anything is factorized: a face with more
    // equality rows than free variables cannot be a regular face and is refused
    // there. Numerical singularity is caught one step later by the inertia gate.
    //
    // Cost and state. `out.counters` reports only what this call paid (at most
    // one factorization). Nothing persistent is touched.
    //
    // Returns true iff the refinement was ACCEPTED. `out` is written either way:
    // on refusal it is `face` verbatim, with this call's own cost.
    //
    // Public-API precondition: the trust-region gate assumes its window is
    // centred at `clamp(0, l, u)`, which holds today only because every seeding
    // site zeroes `seed.x` first. This function takes no centre parameter and
    // does NOT validate that assumption.
    bool refine_on_face(const QpProblem &qp_in, const QpSolution &face,
                        const SolveOverrides &overrides, QpSolution &out) const;

  private:
    // R5's PIN TAKEN DIRECTLY: probe_inertia is private, and the test that
    // reads the face it leaves behind needs a name here. Declared only, defined
    // in the test -- assembly_eval_seam.h's convention, no shipped surface.
    friend struct QpEngineTestAccess;

    /// @brief Resolve one call's SolveOverrides against `opts` into the
    ///     effective tr_radius/primal_delta/dual_mu values every read site in
    ///     qp_engine.cpp consults from that point on. Shared with run() so
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
        // The drop's own Dantzig violation (> opt_tol): the predicted slope along
        // d is violation * dropped_directional(d) (4c, THE FLAT FACE).
        double violation = 0.0;
    };

    // What one attempt at section 4c's ride did.
    enum class RideOutcome {
        kStepped,          // moved to a blocker, which joined the working set
        kUnbounded,        // nothing blocks: the QP is unbounded along the ride
        kVetoed,           // the direction is not ridable: ride_stays_in_working_set
                           // or ride_sign refused it (section 4c, THE DECLINED RIDE)
        kDependentBlocker, // the ride's first blocker tested dependent on the
                           // pass's factor; nothing mutated, RideBlocker says
                           // what blocks (section 4c, THE DECLINED RIDE)
        kDeclined,         // HVEN_TESTING arms only: the caller falls back to the
                           // ordinary step
    };

    // The blocker section 4c's ride found. On kDependentBlocker the ride did NOT
    // admit it and the add site's rule decides it along `dir`, uncapped. On
    // kStepped the ride admitted it already, and `dir` is left empty: what the
    // caller needs there is the MOTION the ride made (4c, FINITENESS).
    struct RideBlocker {
        BlockKind kind = BlockKind::kNone;
        Index idx = -1;
        BoundState state = BoundState::kFree;
        double alpha = 0.0;
        // alpha * ||dir||inf -- the quantity the ordinary add path tests against
        // step_tol to decide whether the iterate moved.
        double dx_inf = 0.0;
        Vec dir;
        detail::DepTest test;
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
        // Rows the independence re-check removed as dependent shift adds;
        // refresh_shifts skips them until the next drop clears the set.
        std::vector<std::uint8_t> shift_refused;
        // THE ZERO-MOTION RE-ENTRY RULE's point memory (independent_working_set
        // only; empty otherwise), cleared whenever the iterate moves. A
        // constraint released here that re-enters here is drop-exempt until
        // the iterate moves; members that entered here are not exchanged out
        // by a step that does not move. `pt_events` counts releases and re-entries at this point,
        // capped at 2(n + mi). `pt_touched`: row j as j, bound i as -(i + 1).
        std::vector<std::uint8_t> pt_ineq;
        std::vector<std::uint8_t> pt_bound;
        std::vector<Index> pt_touched;
        Index pt_events = 0;
    };

    static constexpr std::uint8_t kPtReleased = 1;
    static constexpr std::uint8_t kPtEntered = 2;
    static constexpr std::uint8_t kPtExempt = 4;
    // A deferred add the re-check removed as dependent here (E2 fix1, D3(b)):
    // not re-admitted at this point on an undecided test.
    static constexpr std::uint8_t kPtKnownDependent = 8;
    // A drop undone on ride_sign's veto here: its multiplier is below the
    // resolution of the slope it was supposed to predict, so it does not count
    // as exhausted (4c, THE FLAT FACE).
    static constexpr std::uint8_t kPtNoise = 16;

    static std::uint8_t pt_flags(const WalkSeen &seen, bool is_ineq, Index idx);
    static void pt_mark(WalkSeen &seen, bool is_ineq, Index idx, std::uint8_t flag);
    static void pt_clear(WalkSeen &seen);
    // A release at this point (drop, probe drop, exchange, detector removal).
    static void pt_release(WalkSeen &seen, bool is_ineq, Index idx);
    // An admission at this point; a re-entry of a released constraint is made
    // drop-exempt and counted. Returns true on a re-entry.
    static bool pt_admit(WalkSeen &seen, QpCounters &counters, bool is_ineq, Index idx);

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

    // THE ASCENT GUARD's precondition (4c): every equality row and every working
    // inequality row met at x within row_tolerance -- the iterate stands ON its face, so
    // the EQP step has no row-restoring part to excuse an ascent.
    bool iterate_meets_working_face(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                    const Vec &Aix, const Vec &ai_row_norm1, const Vec &lambda_i,
                                    const Vec &ae_row_norm1, const Vec &lambda_e,
                                    const QpOptions &opts) const;

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

    // The residual the VERDICT-SITE FACE REFINEMENT drives one row to, in that
    // row's own units: the classifier's own threshold,
    // kInfeasibilityMarginFactor * row_tolerance, floored at
    // kVerdictRefineRelFloor * row_scale. Reaching it is exactly what makes
    // condition (a) stop calling the row's residual evidence, so the loop asks
    // for the least accuracy that settles the verdict and no more.
    double face_row_target(double row_scale, double lambda, const QpOptions &opts) const;

    // How far outside its target the worst row of the CLOSED WORKING FACE sits
    // -- max |residual| / face_row_target over every equality row and every
    // active inequality row, so 1.0 is the boundary and below it the face is
    // as accurate as the verdict needs. The acceptance test for a refined
    // point: a candidate is adopted only if it strictly lowers this.
    //
    // Rows OUTSIDE the working set are deliberately absent: they are not what
    // the refinement solves, and a genuine violation on one of them must reach
    // the classifier untouched.
    double working_face_measure(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                const Vec &Aix, const Vec &ai_row_norm1, const Vec &lambda_i,
                                const Vec &ae_row_norm1, const Vec &lambda_e,
                                const QpOptions &opts) const;

    // THE STEP'S ROW-DRIFT CHECK (E4 fix round 2): does the EQP answer leave a
    // working row the INCOMING iterate was already on?
    //
    // The classical active-set invariant is that a step along the EQP direction
    // keeps every working row met, because the unregularized system has
    // Ae p = 0 on those rows. The DUAL regularization breaks it by exactly its
    // own term: the regularized row block reads Ae p = -dual_mu * lambda, so the
    // step drifts off the row by dual_mu*|lambda|. That is harmless while lambda
    // is O(1) -- it is the footprint row_tolerance() already absorbs -- and it is
    // NOT harmless once the PROXIMAL step's multipliers are O(1/primal_delta),
    // which is what they are on a face whose reduced Hessian is flat: the drift
    // is then O(dual_mu/primal_delta), which is O(1) at the defaults.
    //
    // MEASURED (indefinite_equality_qp, 3 variables, one equality row, the
    // reduced Hessian on the face exactly 0, lambda_e ~ 5e+07): the answer
    // solved its own regularized system to 5.5e-09 -- so the E4-4 residual
    // check, which reads that system, had nothing to say -- and opened the
    // equality row by 0.5, after which the walk spent its whole iteration cap
    // taking full steps on an UNCHANGING working set with the row open,
    // reporting kMaxIter at a point that is no local minimizer. The base walk's
    // absolute centring prices that face's multipliers at O(primal_delta), so
    // it never saw this.
    //
    // WHAT IS CHECKED, and what makes it silent where it must be: the check
    // speaks ONLY for a CLOSED FACE -- the incoming iterate meets EVERY working
    // row within face_row_target (the classifier's own threshold, which already
    // absorbs an O(1) multiplier's footprint up to kInfeasibilityAbsorbTol) --
    // and it fires when the candidate leaves any of them. Two things follow.
    // On every restoration cell the incoming iterate does NOT meet its rows --
    // that is what makes it a restoration -- so the check says nothing there.
    // And on an INCONSISTENT row set no point meets them all, so the invariant
    // has nothing to defend and the qualifier keeps the check quiet while the
    // walk reaches its verdict. Equality rows and the working inequality rows
    // only, the same set working_face_measure reads, and with the CANDIDATE's
    // own multipliers pricing the target so a stale incumbent cannot widen it.
    //
    // The caller's response is the suspect-stall ladder, for the reason the
    // arithmetic gives: lambda scales as 1/primal_delta on such a face, so one
    // decade more primal_delta is one decade less drift. AND AN EXHAUSTED LADDER
    // IS ADVISORY, exactly as it is for the residual refusal -- section 5 states
    // the rule. MEASURED both ways -- terminal costs `ACOPR57_def`/qp0 its
    // base certificate on both algebras (opt/183 -> num/162, terminal
    // stationarity 1.24); advisory keeps it, still converges
    // `indefinite_equality_qp`, and converts `A0_YORKNET`/qp1 (num/329 ->
    // opt/717, a conversion fix round 1's polish step cap had given up).
    bool eqp_answer_drifts_off_met_rows(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                        const EqpResult &eqp, const Vec &Aix,
                                        const Vec &ai_row_norm1, const Vec &ae_row_norm1,
                                        const QpOptions &opts) const;

    // The verdict-site face refinement (section 5's dead-end classification).
    // Re-forms the live bordered system, refines it against a target expressed
    // in row units rather than the bordered loop's penalty-scaled footprint, and
    // overwrites `x` with the result -- returning true iff it did.
    //
    // On a bordered candidate only (EqpResult::refine_steps > 0), and only at a
    // dead end whose classification would otherwise be kInfeasible; the caller
    // gates on both. An iteration that reached the elimination path is the
    // eliminated twin's: the residue this one removes is the bordering residue.
    //
    // Closed, or nothing: a candidate is adopted only if the face reaches its
    // target and strictly improves on the walk's own point. A refinement that
    // spends its budget with the face still open is not adopted.
    bool refine_face_for_verdict(const QpProblem &qp, const WorkingSet &ws, Vec &x, const Vec &Aix,
                                 const Vec &ai_row_norm1, const Vec &lambda_i,
                                 const Vec &ae_row_norm1, const Vec &lambda_e, BorderState &border,
                                 QpCounters &counters, const QpOptions &opts) const;

    /// @brief `refine_face_for_verdict`'s eliminated twin: same entry
    ///        condition, same target, same closed-or-nothing adoption rule, same
    ///        counter -- reached whenever the candidate came off `solve_eqp`,
    ///        which is every kRefactorize iteration and every border-mode
    ///        iteration served by a fallback or by the latch.
    ///
    /// Cost and state. The twin reuses `kkt` and pays `solve_vec` calls alone
    /// whenever `face` holds -- the live working set matching the captured one
    /// and `kkt`'s factor still standing at the captured (session_id, epoch).
    /// A working-set change between the candidate solve and this call, and a
    /// re-factorization of `kkt` by any other path in between, each break that
    /// and each is caught. On a miss the twin assembles and factorizes its own
    /// KktFactor, charging one `factorizations` and one `symbolic_analyses`; a
    /// hit charges neither.
    ///
    /// `face` guards the working set and the factor, not the options: the
    /// effective (primal_delta, dual_mu) are fixed across one iteration, and a
    /// reorder that moved an options change ahead of the verdict site would
    /// break the reuse identity.
    ///
    /// Neither branch declines and neither swallows. On a miss the twin
    /// factorizes a system the walk never solved, and that system can be exactly
    /// singular at a legal setting (`dual_mu = 0`), which MKL Pardiso's default
    /// static pivoting perturbs and reports as success. A std::runtime_error out
    /// of `factorize_checked` here is therefore a genuine backend fault and
    /// propagates, as does the hit branch's `solve_vec`; the attempted
    /// factorization stays charged. Accelerate's behaviour on an exactly
    /// singular KKT is UNOBSERVED.
    ///
    /// A kInfeasible exit returns the refined point when it was adopted, so the
    /// elastic seed, the refusal return and the restoration trial read the
    /// refined point on either path; "closed" is the classifier's own tolerance
    /// and nothing tighter; duals are not refined.
    ///
    /// @param qp            The subproblem.
    /// @param ws            The live working set, which must still match the one
    ///                      `face` captured for the reuse path to be taken.
    /// @param x             The candidate, overwritten iff the refinement is
    ///                      adopted.
    /// @param Aix           `Ai * x` at the candidate.
    /// @param ai_row_norm1  Row 1-norms of `Ai`, for the row-unit target.
    /// @param lambda_i      The inequality multipliers at the candidate.
    /// @param ae_row_norm1  Row 1-norms of `Ae`, for the row-unit target.
    /// @param lambda_e      The equality multipliers at the candidate.
    /// @param kkt           The factor the candidate came off, reused when
    ///                      `face` holds.
    /// @param face          The captured elimination partition and working rows.
    /// @param counters      Charged one `factorizations` and one
    ///                      `symbolic_analyses` on a miss, and neither on a hit.
    /// @param opts          The effective options; `(primal_delta, dual_mu)` are
    ///                      fixed across one iteration.
    /// @return True iff the refined point was adopted.
    /// @throws std::runtime_error propagated from `factorize_checked` on a miss,
    ///         and from the hit branch's `solve_vec`; the attempted
    ///         factorization stays charged.
    bool refine_eliminated_face_for_verdict(const QpProblem &qp, const WorkingSet &ws, Vec &x,
                                            const Vec &Aix, const Vec &ai_row_norm1,
                                            const Vec &lambda_i, const Vec &ae_row_norm1,
                                            const Vec &lambda_e, const detail::KktFactor &kkt,
                                            const EliminatedFace &face, QpCounters &counters,
                                            const QpOptions &opts) const;

    // THE TERMINAL MU-FREE REFINEMENT (E2 fix4). At a terminal KKT point whose
    // structural classification would read kInfeasible, the point is the
    // dual_mu-REGULARIZED stationary point of the working face: each working
    // row's residual is dual_mu * lambda_j, and with |lambda| ~ 1e6 that is
    // O(1) -- a violation of the regularized system, not of the QP. The face's
    // EQP is re-solved with dual_mu = 0 on a local bound-eliminated factor
    // (reread_wrong_inertia's path; the pass's factor and border stack are
    // untouched), refined at most kTerminalMuRefineSteps further steps against
    // the unregularized residual, and the refined point is ADOPTED -- x and
    // the multipliers (lambda_e, lambda_i, z re-priced) -- only when it
    // satisfies EVERY row and every bound of `qp` within feas_tol * row_scale
    // (row_scale as worst_structural_violation's; the box as E4's). Otherwise
    // nothing changes and the existing classification stands.
    //
    // mu_footprint_open is the entry test: some working-face row (an equality
    // or an active inequality) carries dual_mu * |lambda_j| > feas_tol *
    // row_scale_j. Below it the regularization cannot explain a residual and
    // the refinement is skipped. The caller enters only at a would-be
    // kInfeasible (worst > 0) with independent_working_set on and dual_mu > 0,
    // so a terminal point that already satisfies the rows never reaches it.
    //
    // Counters: terminal_mu_refines (entries: one factorization each, charged
    // to `factorizations` too), terminal_mu_refine_rescues (adoptions).
    bool mu_footprint_open(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                           const Vec &ai_row_norm1, const Vec &lambda_i, const Vec &ae_row_norm1,
                           const Vec &lambda_e, const QpOptions &opts) const;
    bool refine_terminal_mu_free(const QpProblem &qp, const WorkingSet &ws, Vec &x, Vec &lambda_e,
                                 Vec &lambda_i, Vec &z, QpCounters &counters,
                                 const QpOptions &opts) const;

    // THE TERMINAL POLISH (M7 W2 E4 fix round 1, ruling E4-11; section 5 states
    // the rule and why the walk needs it). At a would-be kOptimal exit, the
    // final point is re-solved on its FINAL WORKING SET with the regularization
    // BACKED OUT -- dual_mu = 0 in the system, and the primal delta backed out
    // of the refinement target by kTerminalPolishSteps steps of
    // polish_eqp_unregularized, which is consistent precisely here because the
    // step is going to zero. One local factorization, charged to
    // `terminal_polish_solves` and NOT to `factorizations` (ruling E4-17;
    // solver_counters.h states the contract at both fields, and the asymmetry
    // with refine_terminal_mu_free, whose factorization IS in `factorizations`),
    // the pass's own factor and border stack untouched.
    //
    // ADOPTED only if the candidate is finite, keeps feasibility (no row and no
    // bound outside feas_tol * row_scale that the incumbent met, and nothing
    // worse than the incumbent), does not WORSEN any of the three KKT residuals
    // -- free-block stationarity, worst row/bound violation, worst
    // complementarity product -- and strictly IMPROVES at least one of them, and
    // if it PASSES THE WALK'S OWN KKT-POINT TEST (section 5) -- its relative
    // stationarity within opt_tol, so the exported point is one the walk itself
    // would certify. Otherwise x and every multiplier are left exactly as they
    // were and the exit is unchanged: the polish can make the answer better and
    // cannot make it worse.
    //
    // `polish_footprint_open` is the entry test, and it is a COST gate, not a
    // correctness one: it asks whether this point has anything a solve could gain
    // -- a complementarity product above kTerminalPolishComplFactor * feas_tol, or
    // a stationarity residual above kTerminalPolishStatFactor * opt_tol. Both
    // constants carry their own calibration; below both there is nothing worth a
    // factorization, and stage 1 is free either way.
    //
    // Counters: terminal_polish_solves (entries), terminal_polish_rejected
    // (candidates not adopted).
    bool polish_footprint_open(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                               const Vec &lambda_e, const Vec &lambda_i, const Vec &z,
                               const QpOptions &opts) const;
    // The acceptance test, shared by both stages: prices `cand`, measures
    // detail::terminal_kkt at it, and ADOPTS (x and all three multiplier blocks)
    // only if the candidate is finite, keeps feasibility, worsens none of the
    // three residuals and strictly improves one. Returns whether it adopted.
    bool adopt_polished_candidate(const QpProblem &qp, const WorkingSet &ws, const EqpResult &cand,
                                  const detail::TerminalKkt &before, const QpOptions &opts, Vec &x,
                                  Vec &lambda_e, Vec &lambda_i, Vec &z) const;
    bool polish_terminal_point(const QpProblem &qp, const WorkingSet &ws,
                               const EqpResult &pass_answer, Vec &x, Vec &lambda_e, Vec &lambda_i,
                               Vec &z, QpCounters &counters, const QpOptions &opts) const;

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
    //
    // `x_k` is THE PROXIMAL CENTRE the EQP is solved from -- the pass's own
    // iterate -- and it is threaded, explicitly, through every function on this
    // path down to the two solves (eqp_solve.h states the system). Explicitly,
    // because the centre is part of the system: a reader of any of these
    // signatures can see which point the answer is a step from. The probe and
    // inertia-read call sites pass the same iterate even though only the
    // FACTORIZATION's inertia is read there and the rhs cannot reach it.
    EqpResult eqp_candidate(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                            detail::KktFactor &kkt, EliminatedFace &face, BorderState &border,
                            QpCounters &counters, detail::InertiaVerdict &verdict,
                            const QpOptions &opts) const;

    // One solve_eqp against a freshly assembled+factorized, bound-ELIMINATED
    // KKT system. Short-circuits the empty system (every variable pinned, no
    // equalities, no working rows), which Pardiso cannot be handed -- which is
    // also why a border-mode fallback on an all-variables-pinned working set
    // costs no factorization at all.
    EqpResult eliminated_candidate(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                                   detail::KktFactor &kkt, EliminatedFace &face,
                                   QpCounters &counters, detail::InertiaVerdict &verdict,
                                   const QpOptions &opts) const;

    // THE MU-FREE INERTIA RE-READ (R-C', E2 fix3; E2-D4 class B; settler S26).
    // A kWrong verdict at the start repair or the terminal check is re-read on a
    // bound-eliminated factor of the CURRENT working set assembled with
    // dual_mu = 0, in a local KktFactor (the pass's factor and border stack are
    // untouched; each read is one extra factorization). THE RULE: a kOk mu-free
    // read overturns the kWrong. A kSuspect mu-free read overturns it ONLY when
    // a second eliminated read at the pass's own dual_mu reads kOk -- a kSuspect
    // alone (a perturbed zero pivot at mu = 0) cannot tell a PSD-singular face
    // from an indefinite one, so only the border path's K0 verdict is overturned
    // there; the suspect-stall gate then decides a would-be kOptimal as for any
    // kSuspect (correctness guard: AMuFreeSuspectReadDoesNotCertifyASaddle). Any
    // other outcome -- a kWrong mu-free read, a kSuspect with a non-kOk second
    // read, or a backend runtime_error -- leaves the kWrong standing. Known gap
    // (registered): a strict local minimum whose mu-free read is kSuspect and
    // whose dual_mu read is kWrong is not certified (ACOPR57 qp4); the honest fix
    // is the reduced Hessian's inertia on a small face, a later round.
    // independent_working_set only (the option-off walk is unchanged). Returns
    // true iff the verdict was replaced (dep_inertia_rereads counts the calls
    // that read, dep_inertia_reread_flips the replacements).
    bool reread_wrong_inertia(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                              QpCounters &counters, const QpOptions &opts,
                              detail::InertiaVerdict &verdict) const;

    // THE CONVEX CERTIFICATE (M7 W2 NC4; section 4c). At the certification
    // branch's trusted-kWrong arm, after R-C''s re-read has left the kWrong
    // standing and the post-probe restart has not been taken: a STRUCTURALLY
    // convex H (detail::hessian_is_nonneg_diagonal) at a point whose free block
    // is stationary in opt_tol currency (free_block_is_stationary) replaces the
    // kWrong by kSuspect -- the read is a misread, not evidence -- and counts it
    // (convex_certificates). independent_working_set only. Returns true iff the
    // verdict was replaced. QpOptions::hessian_known_convex stands in for the
    // structural test (a caller's assertion, never verified here).
    static bool convex_overrules_wrong_inertia(const QpProblem &qp, const WorkingSet &ws,
                                               const Vec &x, const Vec &lambda_e,
                                               const Vec &lambda_i, QpCounters &counters,
                                               const QpOptions &opts,
                                               detail::InertiaVerdict &verdict);

    // Border-mode counterpart of eqp_candidate (header contract, step 4):
    // bring K0's border stack in line with `ws`, rebuild K0 if that stack can
    // no longer be trusted, then solve through it. The empty-system
    // short-circuit above has no counterpart here and needs none -- K0 spans
    // all n variables whatever the working set does.
    EqpResult border_candidate(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                               detail::KktFactor &kkt, EliminatedFace &face, BorderState &border,
                               QpCounters &counters, detail::InertiaVerdict &verdict,
                               const QpOptions &opts) const;

    // Shared tail of both entry paths above: decide whether the border stack
    // is usable, spend at most one rebuild trying to make it so, and solve --
    // falling back to the elimination path (and latching where appropriate)
    // when it is not. `opts` is the effective options this solve resolved
    // (see run()'s `eff_opts`).
    EqpResult border_solve_or_fall_back(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                                        detail::KktFactor &kkt, EliminatedFace &face,
                                        BorderState &border, QpCounters &counters,
                                        detail::InertiaVerdict &verdict, bool rebuilt,
                                        const QpOptions &opts) const;

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
    //
    // `face` is the LOOP's key, not a throwaway: this call may re-factorize
    // `kkt`, and a key that did not follow it would keep asserting a
    // factorization the probe overwrote (EliminatedFace, fix 2).
    detail::InertiaVerdict probe_inertia(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                                         detail::KktFactor &kkt, EliminatedFace &face,
                                         BorderState &border, QpCounters &counters,
                                         const QpOptions &opts) const;

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
    // CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    // convex B, in which case this certifies B, not the NLP.
    bool probe_zero_multiplier_drops(const QpProblem &qp, const Vec &x_k, const Vec &shift,
                                     const Vec &lambda_i, const Vec &z, WorkingSet &ws,
                                     detail::KktFactor &kkt, EliminatedFace &face,
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
    // `eff_opts`), threaded through to probe_inertia -> eqp_candidate; `face`
    // is threaded for the same reason probe_inertia takes it.
    bool repair_temporary_vertex(const QpProblem &qp, WorkingSet &ws, Vec &x,
                                 detail::KktFactor &kkt, EliminatedFace &face, BorderState &border,
                                 QpCounters &counters, const QpOptions &opts) const;

    // Would re-deriving K0 from `ws` reproduce exactly the state we are in?
    // It would iff K0 already spans the current working ROWS (so the rebuild
    // assembles the same matrix) and every live border is a pin (so the
    // re-sync re-adds the same borders). Rebuilding then buys nothing and
    // costs a factorization.
    // The factor this pass holds for exactly `ws` (read-only), or none.
    static detail::DepFactorView dep_factor_view(const WorkingSet &ws, const detail::KktFactor &kkt,
                                                 const EliminatedFace &face,
                                                 const BorderState &border);

    // Does the live border stack represent exactly `ws`'s members?
    static bool border_holds(const WorkingSet &ws, const BorderState &border);

    // The dependency test on one candidate (a row, or a bound of variable
    // `idx`), counted.
    static detail::DepTest test_candidate(const QpProblem &qp, const WorkingSet &ws,
                                          const detail::DepFactorView &view, bool is_ineq,
                                          Index idx, QpCounters &counters, const QpOptions &opts);

    // Detector repair: drops one optional member of `det`'s support. At entry
    // (`seq` null): seed rows, then seed pins, then shifted rows, highest
    // index first. On a re-check: the newest member by `seq` (addition order),
    // highest index on ties. A removed shifted row is marked refused in
    // `refused`. False when the support is mandatory only.
    static bool remove_entry_dependency(WorkingSet &ws, const Vec &shift,
                                        const detail::EntryDetection &det, const ProbeState *seq,
                                        std::vector<std::uint8_t> &refused,
                                        detail::WsMember *removed = nullptr);

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
    // `pt` (independent_working_set): drop-exempt members are skipped, and the
    // largest violation among them is reported in `exempt_worst`.
    bool drop_worst(const QpProblem &qp, const Vec &shift, const Vec &lambda_i, const Vec &z,
                    WorkingSet &ws, DropRecord &dropped, QpCounters &counters,
                    const WalkSeen *pt = nullptr, double *exempt_worst = nullptr) const;

    // THE EXCHANGE at the ordinary add site, for a blocker the dependency test
    // found dependent (Gill & Wong 2015, Result 3.4(b)). Every sign-constrained
    // member k is oriented c_k^T x <= d_k with m_k >= 0 at optimality, and the
    // test's coefficients give c_r = sum_k v_k c_k + Ae^T v_e. Removable:
    // not an equality, a kFixed pin, or -- on a step that does not move (`moves`
    // false) -- a member that entered at this point. The lock applies ONLY on a
    // zero-motion step: the re-entry rule guards zero-motion cycles, and a
    // moving step changes the point and clears the point memory by
    // construction before the exchange mutates the set. Admissible: removable with
    // v_k ||c_k|| > kDepCoeffTol * max_l |v_l| ||c_l||. j = argmin m_k^+ / v_k
    // (ties: most negative m_k, then largest scaled v_k, then rows before pins,
    // ascending index); sigma = m_j^+ / v_j. New multipliers m - sigma v, and
    // sigma on r. A shifted row is admissible only when no other member is
    // (`j_shifted`, last resort): with v_j > 0 its value on the new face is
    // d_j - g / v_j <= d_j, so the face satisfies it; it is marked
    // shift-refused so the homotopy does not re-admit it. Shifted rows rank
    // LAST: one is never chosen while an unshifted admissible member exists
    // (Debug-asserted in select_exchange).
    //
    // THE LOCKED-VERTEX UNLOCK (R-B, E2 fix5; E2-D4 class A). When a
    // zero-motion step finds no admissible member and the rule would END the
    // solve there (no verified certificate, no pass-over within tolerance; on a
    // border-stack pass the border retreat comes first), the scan is repeated
    // (`unlock`) with every member locked ONLY by having entered at this point
    // -- flags kPtEntered without kPtReleased or kPtExempt, i.e. never
    // released here -- made removable, and j is chosen by the min-ratio rule
    // alone with ties to the lowest index (rows before pins, ascending; Bland
    // 1977), shifted rows still last. It can change only a solve that would
    // otherwise end dep_blocker_terminations (dep_unlocks counts it).
    // FINITENESS: the unlocked member is released (kPtReleased), so it can
    // never be unlocked again until the point memory clears (motion, a ride
    // step, or one of the two once-per-solve repairs): each member is unlocked
    // at most once per point, so at most n + mi unlocks per point, and each is
    // also a release event under the zero-motion cap 2(n + mi). No extra cap is
    // needed. Re-entered members (kPtExempt) stay locked: unlocking them too
    // broke ACOPR57 qp0-qp2 (E2-D4 `unlockall`).
    struct ExchangePick {
        bool found = false; // an admissible member exists
        bool cond1 = false; // every sign-constrained member's scaled v_k <= tol * max
        bool j_is_ineq = false;
        bool j_shifted = false; // the released member is a shift-driven row
        bool unlocked = false;  // j was admissible only under the unlock (R-B)
        // A shift-driven member row carries a nonzero coefficient (either sign): `g`
        // reads unshifted right-hand sides, so it does not describe the face.
        bool shifted_coeff = false;
        Index j = -1;
        double sigma = 0.0;
        double sign = 1.0; // the blocker's orientation: c_r = sign * (a_r or e_r)
        double g = 0.0;    // sum_k v_k d_k + v_e^T be - d_r
        Vec v_e;           // oriented, per equality row
        Vec v_w;           // oriented, per ws.active_ineq() entry
        Vec v_pin;         // oriented by each pin's side (kFixed as +e_i)
    };
    static ExchangePick select_exchange(const QpProblem &qp, const WorkingSet &ws, const Vec &shift,
                                        const WalkSeen &seen, const detail::DepTest &t,
                                        bool is_ineq, Index idx, BoundState side,
                                        const Vec &lambda_e, const Vec &lambda_i, const Vec &z,
                                        bool moves, bool unlock);

    // THE IMPLIED BLOCKER (M7 W2 QC-8), asked at the add site before the
    // exchange, on an ORDINARY step only (x_face = x + p, alpha <= 1). The
    // test proves c_r = sum_k v_k c_k + Ae^T v_e + r (verdict kDependent with
    // rho <= kDepTol, so ||r|| = rho ||a_r|| on free columns; structural:
    // r = 0). Every point meeting the members exactly gives c_r^T y = d_r + g
    // + r^T y (g as select_exchange's), and along the exact ordinary step every
    // member moves linearly onto its own constraint, so c_r^T (x + alpha p) =
    // (1 - alpha) c_r^T x + alpha (d_r + g + r^T x_face): the blocker's value
    // is a convex combination of its value at x and at the face. When
    // g + rho ||a_r|| ||x_face,free|| <= feas_tol * row_scale the face meets
    // it, so the EXACT step cannot carry it past max(its violation at x,
    // feas_tol * row_scale) -- it is not a blocker, and its computed rate along
    // p is the step's own leak off the members (A_W p = O(mu lambda)), which at
    // a tied vertex makes every member of a dependent cycle block in turn at
    // zero motion (DALLASM QN QP 6: an exchange keeps the span and so the step,
    // the released member blocks next, and the lock ends the solve). False for
    // a shift-driven blocker or a member row carrying ANY nonzero coefficient
    // while shifted (`g` reads unshifted right-hand sides), an unproved
    // dependency (the integer screen alone, a stalled or un-run test), and on a
    // ride, whose direction is not the step onto the face.
    //
    // THE LEAK BUDGET (QC-8 R1). The COMPUTED step leaks off each member by at
    // most what the member's own row tolerance absorbs, so the blocker's leak
    // is bounded by implied_leak_allowance = max(feas_tol * row_scale,
    // sum_k |v_k| row_tolerance(scale_k, lambda_k)) over the equality and
    // working inequality rows (pins are exact: 0). An implied blocker is passed
    // over only while its predicted violation at the end of the step,
    // max(0, c^T x - d + alpha c^T p), stays within that allowance -- N2's
    // check with the members' absorbency as the allowance, not an exemption
    // (QpCounters::dep_implied_passovers counts the pass-overs taken).
    //
    // A PREFERENCE, NOT A ROUTE (QC-8 R3). When a step with an implied
    // blocker passed over fails N2's check -- its own leak above the budget, or
    // any passed-over blocker's violation at the longer step -- the rule falls
    // back to what the walk would have done without it: the exchange pick the
    // sequence's first implied blocker set aside, taken at the step that
    // reached that blocker; else the locked-vertex unlock (R-B); only then the
    // end (N3). The implied pass-over never makes a new N3 end.
    static bool blocker_implied_by_face(const QpProblem &qp, const WorkingSet &ws, const Vec &shift,
                                        const Vec &x, const Vec &p, const detail::DepTest &t,
                                        const ExchangePick &pick, bool is_ineq, Index idx,
                                        double row_scale, const QpOptions &opts);
    double implied_leak_allowance(const QpProblem &qp, const WorkingSet &ws,
                                  const ExchangePick &pick, const Vec &x, const Vec &ai_row_norm1,
                                  const Vec &ae_row_norm1, const Vec &lambda_e, const Vec &lambda_i,
                                  double row_scale, const QpOptions &opts) const;

    // The no-admissible-member rule's certificate, VERIFIED, tried whenever no
    // member is admissible (E2 fix1: the box-based certificate does not need
    // cond1, and a locked positive-coefficient member leaves cond1 false on an
    // infeasible QP; a cond1 candidate that fails is still kRejected): builds the
    // certificate from the test `t` oriented by `pick.sign` (y_i = min(v, 0) on
    // member rows, the rest in the explicit box part) and true iff its implied
    // bound clears the blocker's limit by more than the structural margin at
    // `row_scale`. The multipliers are first REFINED mu-free on `view`
    // (detail::certificate_coefficients; its solves added to `solves`): t's come
    // from the dual_mu-regularized solve and are O(mu) off, which leaves an
    // O(mu) box_part on free coordinates of an exactly dependent blocker
    // (settler S25'). On a single-row dependency the row's exact coefficient
    // (c's least-squares fit on it over the free columns) replaces the refined
    // one when it lowers the residual: the refinement contracts by ~ mu ||H|| /
    // ||a||^2 per step, slowly on a short row. A test with rho == 0 (structural,
    // or no free column) is exact already and is not refined. Box_part entries <= kDepTol * ||c||
    // on coordinates unbounded on the needed side are dropped and their norm kept in
    // QpFarkasCertificate::residual. residual <= exact_tol (kCertExactKappa, the rounding floor):
    // EXACT, the caller ends kInfeasible. Above it: TOLERANCED, a proof only for the QP with the
    // dropped coefficients zeroed, NOT for this one (the dropped term is unbounded on its
    // coordinate): the caller ends kNumericalError at the current iterate (after the border retreat
    // on a border pass) and counts dep_certificate_toleranced (S25). Either is copied into
    // QpSolution::farkas.
    bool verify_farkas(const QpProblem &qp, const WorkingSet &ws, const detail::DepFactorView &view,
                       const detail::DepTest &t, const ExchangePick &pick, bool is_ineq, Index idx,
                       double row_scale, const QpOptions &opts, QpFarkasCertificate &cert,
                       Index &solves) const;

    // THE PASS-OVER SCAN: production's ratio scan with the passed-over
    // constraints excluded (the no-admissible rule's pass-over, N2), for ONE
    // pass-over sequence -- p, x and the working set do not change inside it.
    // The candidates (non-working rows with a.p > kEngineDenomTol, free
    // variables moving toward a finite bound, finite ratio) are scored and
    // sorted ONCE, by (ratio, rows before bounds, index): exactly the order in
    // which the full scan's strict `<` keeps the first minimum it meets, so
    // `next` returns the full scan's pick. Each pass-over is then O(1)
    // amortized instead of O(mi + n) -- M7 W2 C13 measured a degenerate vertex
    // of TAX53322 (mi = 32 221) passing over ~4 000-8 000 blockers per minor.
    class PassOverScan {
      public:
        PassOverScan(const QpProblem &qp, const WorkingSet &ws, const Vec &x, const Vec &Aix,
                     const Vec &p);
        // Excludes a constraint from every later `next`.
        void pass(bool is_ineq, Index idx);
        // The smallest unpassed ratio and its constraint, capped at the unit
        // step unless `cap_at_unit` is false (a ride's pass-over: the raw
        // ratio, infinite exactly when nothing blocks).
        double next(const Vec &p, BlockKind &kind, Index &block_idx, BoundState &block_state,
                    bool cap_at_unit);

      private:
        struct Candidate {
            double ratio;
            bool bound;
            Index idx;
        };
        bool passed(const Candidate &c) const {
            return (c.bound ? passed_bound_ : passed_ineq_)[static_cast<std::size_t>(c.idx)] != 0;
        }
        std::vector<Candidate> cands_;
        std::size_t cur_ = 0;
        std::vector<char> passed_ineq_;
        std::vector<char> passed_bound_;
    };

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
    // rows by O(dual_mu * |lambda|)). Failing this VETOES the ride (section
    // 4c, THE DECLINED RIDE, says what follows).
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
    // improves anything. A disagreement VETOES the ride (section 4c, THE
    // DECLINED RIDE).
    //
    // A FLAT DIRECTION ALSO VETOES, and that is load-bearing: it buys no
    // objective decrease while still pinning a blocker into the working set,
    // which can turn a legitimate small row residual into a spurious
    // kInfeasible at step 5's classifier, and it would contradict 4c's
    // anti-cycling argument, which rests on the decrease being STRICT.
    //
    // With independent_working_set the drop's own PREDICTED slope decides the
    // sign only BELOW the measured slope's resolution (4c, THE FLAT FACE);
    // `predicted` reports that it did.
    int ride_sign(const QpProblem &qp, const DropRecord &dropped, const Vec &p, const Vec &x,
                  bool *predicted = nullptr) const;

    // Whether a drop whose ride ride_sign vetoed is a NOISE drop: its own
    // predicted slope is at or below the resolution of the slope it was meant to
    // predict (4c, THE FLAT FACE). Reads g'p and s from `x` and `p`; no solve.
    bool ride_drop_is_noise(const QpProblem &qp, const DropRecord &dropped, const Vec &p,
                            const Vec &x) const;

    // 2-norm of the dropped constraint's gradient, for ride_sign's tolerance.
    static double dropped_gradient_norm(const QpProblem &qp, const DropRecord &dropped);

    // Section 4c's ride. `p` is the post-drop EQP step, already measured to
    // have non-positive curvature, so the QP is unbounded below along one of
    // +/-p inside the current working set. Steps to the nearest blocker along
    // the admissible sign and puts that blocker into `ws`. `ws` and `x` are
    // left untouched on every outcome but kStepped.
    // CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    // convex B, in which case this certifies B, not the NLP.
    // `dep` (independent_working_set): the pass's factor; a dependent ride
    // blocker is returned in `blocked` (kDependentBlocker), not admitted.
    RideOutcome ride_negative_curvature(const QpProblem &qp, const DropRecord &dropped,
                                        QpCounters &counters, WalkSeen &seen, const Vec &p,
                                        const Vec &Aix, WorkingSet &ws, Vec &x,
                                        const detail::DepFactorView *dep, const QpOptions &opts,
                                        RideBlocker &blocked) const;

    // The ride itself, along a direction already chosen: section 4c's ride after its
    // sign, and THE SADDLE EXIT's. Same outcomes and contract as ride_negative_curvature
    // past its two vetoes.
    RideOutcome ride_direction(QpCounters &counters, WalkSeen &seen, const QpProblem &qp,
                               const Vec &dir, const Vec &Aix, WorkingSet &ws, Vec &x,
                               const detail::DepFactorView *dep, const QpOptions &opts,
                               RideBlocker &blocked) const;

    // THE SADDLE EXIT's direction (section 4c): `ws`'s face factorized at dual_mu = 0 into
    // the caller's `kkt`/`face` (`view` names that factor on return, for the add site),
    // then detail::face_negative_curvature on it. Empty when no VERIFIED direction exists
    // (including a failed factorization).
    Vec saddle_direction(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                         detail::KktFactor &kkt, EliminatedFace &face, QpCounters &counters,
                         const QpOptions &opts, double curvature_tol,
                         detail::DepFactorView &view) const;

    // THE SADDLE EXIT's sign (section 4c), at `x`: the descent sign when g'd resolves; on a
    // flat slope the sign with the larger decrease to its first blocker, a tie preferring the
    // sign that leaves `released` (the previous pass's drop, if any). Empty when the chosen
    // ride would re-enter `released` at zero length (it would undo the drop, not leave the
    // saddle). The same holds for a zero-length re-entry of ANY probe-exempt member or
    // any member the point memory `seen` marks released at this point.
    Vec orient_saddle_direction(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                const Vec &Aix, Vec d, const DropRecord *released,
                                const ProbeState &probe, const WalkSeen &seen,
                                double step_tol) const;

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
    // than read directly by most of the engine.
    const QpOptions opts_;
    Ledger *ledger_ = nullptr;
    std::string label_prefix_;
    mutable Index solve_counter_ = 0;
    // The thread count in force, and the options fingerprint derived from it and
    // from opts_ at construction. Both are FROZEN for this engine's lifetime --
    // opts_ is, so the hash over it is too, and a changed option means a new
    // engine (SqpSolver::set_options rebuilds). Computed once here rather than
    // per solve because run()'s adoption gate reads it on every call.
    int threads_ = 0;
    std::uint64_t options_hash_ = 0;

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
