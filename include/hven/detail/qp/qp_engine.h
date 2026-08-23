// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_engine.h — the primal active-set loop for the QP
//
//     min   g^T x + 1/2 x^T H x
//     s.t.  Ae x  = be,   Ai x <= bi,   l <= x <= u
//
// H is not required to be positive semidefinite; sections 4b/4c below add
// what an INDEFINITE H needs on top of the ordinary convex loop -- 4b keeps a
// second-order-inconsistent start from certifying optimal, 4c keeps a drop
// that exposes negative curvature from solving an unbounded EQP. Both are
// provably inert (and free) on a convex H.
//
// The numerics live below this file: the working set (working_set.h), the
// regularized bound-eliminated KKT assembly (kkt_assembly.h), the sparse
// factorization (hven::linear::SymmetricFactor via kkt_calls.h's KktFactor),
// and the equality-QP solve with iterative refinement (eqp_solve.h). This
// file is only the loop that walks between working sets.
//
// --- Loop contract ---
//
// 0. CROSSED BOUNDS. Every i with lower(i) > upper(i) + feas_tol is an empty
//    box regardless of H/g/Ae/Ai. Checked here (not in QpProblem::validate())
//    because a caller may legitimately hand this engine an empty box (e.g. a
//    trust-region box intersected with the real bounds) as a normal runtime
//    outcome. Verdict: kInfeasible immediately, x = the clamped start point,
//    multipliers zero.
//
// 1. START POINT. Cold start: x = clamp(0, l, u). Warm start: clamp(seed.x,
//    l, u) plus seed.bound_state/seed.ineq_active as the initial working set.
//    l(i) == u(i) is marked kFixed and never leaves the working set. A
//    variable merely sitting at a bound is NOT pinned at start -- the ratio
//    test pins it the first time it actually blocks, which keeps the initial
//    reduced KKT system well posed even when many bounds are touched at once.
//
//    General inequalities violated at the start enter via a SHIFTED-
//    CONSTRAINT HOMOTOPY rather than a phase-1 LP:
//        shift(j) = max(0, Ai_j x - bi_j)
//    x is feasible for bi + shift; every row with shift(j) > 0 joins the
//    working set. The EQP always solves against the TRUE rhs bi, so a
//    shifted row is pulled from bi_j + shift(j) toward bi_j and its shift
//    decays with the step. Shifts are recomputed from x every step (not
//    propagated) and clamped to zero below a scale-aware tolerance (see
//    refresh_shifts). A working row whose shift is still positive is exempt
//    from the drop rule in step 2: it is being driven to feasibility, so its
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
//    TR-PINNED STATIONARITY CAVEAT (section 6). z above is priced and
//    consulted internally the same way at every index, TR-pinned or not, but
//    the z REPORTED in QpSolution is forced to 0 at a TR-pinned index, so
//    there the reported quantities do not satisfy stationarity -- the
//    unexposed TR dual absorbed that residual. A kFree report at a TR-pinned
//    index therefore only means "unconstrained by any REAL bound"; a caller
//    must read tr_active, not z or bound_state, for TR constraint status.
//
// 3. RATIO TEST. If p is not negligible, step along it: alpha = min(1,
//    min_j ratio_j), stopping at the first non-working inequality or bound
//    that blocks; that constraint joins the working set. A ratio landing at 1
//    within kStepTieTol counts as blocking too (a full step landing exactly
//    on a constraint activates it), matching the dense oracle's tie-break
//    toward MORE active constraints at a degenerate vertex.
//
//    KNOWN LABELING DIVERGENCE. This tie-break only fires for a constraint
//    the step travels toward -- a variable already sitting on its bound with
//    p(i) == 0 is never pinned (the ratio test only considers |p(i)| >
//    kEngineDenomTol), so it is reported kFree/z==0 where the dense oracle
//    reports kAtLower/kAtUpper with a zero multiplier too. x, the objective
//    and the duals agree; only the active-set LABEL differs. Documented
//    rather than fixed -- pinning zero-step variables would grow the working
//    set every iteration for no numerical gain. A caller needing activity by
//    geometry rather than working-set membership must test the residual
//    itself.
//
// 4. WORKING-SET UPDATE / COUNTERS. Under QpOptions::ws_algebra ==
//    kRefactorize, every working-set change is followed by a fresh
//    assemble_kkt() + factorize_checked() (solve_eqp does both), counted in
//    QpCounters::factorizations.
//
//    BORDER MODE (kSchurBorder, the DEFAULT) keeps the loop above unchanged
//    and swaps only the linear algebra: one K0 (assemble_kkt_full, spanning
//    all n variables) is assembled and factorized from the seed working set,
//    and every later working-set change becomes a GMSW border over that fixed
//    factorization (border_ops.h/schur_complement.h) instead of a
//    refactorization -- pin -> pin_variable border, free -> drop that border,
//    row activated -> add_ineq_row border (or drop its delete border if K0
//    already owns the row), row deactivated -> delete_k0_row border (or drop
//    its add_ineq_row border). Each such op increments
//    QpCounters::schur_updates; the EQP solves through solve_bordered_eqp.
//    K0 is re-assembled/re-factorized -- clearing the border stack, counted
//    in factorizations -- when SchurComplement::needs_refactorization() trips
//    (past schur_cap, past schur_cond_max, a singular factor) or K0's own
//    factorization needed a perturbed pivot.
//
//    A PIN CAN NEVER BE FOLDED BACK INTO K0 (assemble_kkt_full eliminates
//    nothing), so once the live pin count alone passes schur_cap,
//    needs_refactorization() is permanently true and a rebuild is a no-op
//    that just re-adds every pin -- quadratic in n on a box-shaped QP (the
//    canonical trust-region case). Those iterations, and any where a rebuild
//    already fired without clearing the flag or the bordered solve throws on
//    a singular factor, fall back to the ELIMINATION path (solve_eqp on the
//    current working set, where pins are eliminable by construction) and
//    cost a factorization like any other.
//
//    THE LATCH. When the cause is that pins-only dead end specifically, the
//    engine stops maintaining the border stack at all rather than syncing it
//    for a solve that will not read it (every add_border costs a K0 solve
//    plus an O(dim^2*n0 + dim^3) dense rebuild of C, and syncing anyway was
//    the dominant remaining cost on the box shape). It releases on exactly
//    one condition -- the pin count falling back to schur_cap or below -- at
//    which point one rebuild_k0 restarts normal border operation; the stale
//    stack is discarded, not repaired. A change in the working ROWS does NOT
//    release it (see latch_still_holds).
//
//    The two modes are held OBSERVATIONALLY EQUIVALENT for a CONVEX H (same
//    status, active set, x, multipliers) by
//    QpEngineBorder.BorderModeMatchesRefactorizeMode, which runs this file's
//    fixture battery through both; the refactorize path is the oracle. They
//    are NOT equivalent for an indefinite H -- the bound-eliminated K and the
//    full-variable K0 can have different inertia, so the two modes can
//    legitimately reach different working sets/statuses; indefinite-H
//    correctness is validated against the local-minimizer oracle instead.
//
//    COUNTER SEMANTICS (relied on downstream, e.g. warm-start assertions).
//    QpCounters::minor_iters increments exactly ONCE per major iteration (one
//    EQP solve plus a drop or a ratio-test step), so a run stopping at
//    opts.max_iter reports minor_iters == max_iter. Under kRefactorize,
//    factorizations counts solve_eqp calls (one per major iteration, except
//    the empty-reduced-system short-circuit -- every variable pinned, no
//    equalities, no working rows -- which touches no factorization);
//    schur_updates stays 0. Under kSchurBorder, factorizations counts K0
//    factorizations plus elimination-path fallbacks -- normally one per solve
//    that trips nothing, but a single iteration can spend two (a K0 rebuild
//    found not to have helped, then the fallback solve_eqp); schur_updates
//    counts individual add_border/drop_border calls INCLUDING re-adds after a
//    rebuild (work actually done), but not the rebuild's own wholesale clear.
//    4c's ride costs one minor_iter like any other step and is otherwise
//    invisible to these counters. 4b's repair costs one EXTRA minor_iter (the
//    kWrong iteration is counted, then retried) plus, per pin/release it
//    probes, schur_updates under kSchurBorder or one factorization under
//    kRefactorize. Neither is reachable on a convex H (the inertia gate
//    cannot fire kWrong there). border_refine_steps/
//    eqp_refine_steps accumulate at the same two EQP call sites: every
//    solve_bordered_eqp call adds its total kept steps (>= 1), every
//    solve_eqp call adds its extra kept steps (always 0 -- no iterated loop)
//    -- see core/solver_counters.h. The empty-system short-circuit has no
//    border-mode counterpart and needs none -- K0 spans all n variables
//    unconditionally, so "every variable pinned" is just n pin borders over
//    an ordinary factorization.
//
//    HOT-START REUSE (border mode only). border_ is an ENGINE-INSTANCE
//    member, not a per-solve local, so a warm re-solve on the same QpEngine
//    can skip K0's assembly/factorization when FIVE conditions all hold at
//    the seed working set, checked once before the loop's first iteration:
//      (a)/(c) H/Ae/Ai's structural pattern AND values are byte-identical to
//          the previous trustworthy solve (detail::structural_hash /
//          detail::values_hash -- two separate FNV-1a fingerprints, because a
//          structural change can leave the value stream byte-identical, e.g.
//          two rows [1,0],[-1,0] vs one row [1,-1] both hash to values
//          [1,-1] under values_hash alone). K0's values depend on H/Ae/Ai
//          and the EFFECTIVE (primal_delta, dual_mu) this solve resolved to
//          -- never on g/be/bi -- so a warm re-solve that only perturbs g/b
//          is exactly the case this exists for.
//      (d) that effective (primal_delta, dual_mu) pair is identical to the
//          previous trustworthy solve's (border_effective_delta_/
//          border_effective_mu_). Necessary because (a)/(c) hash only
//          H/Ae/Ai: a changed pair changes every value assemble_kkt_core
//          writes onto K0's regularized diagonal without touching an H/Ae/Ai
//          byte. tr_radius is NOT part of this or of the key at all -- bounds
//          (real or TR-derived) never enter K0 (see the HOT-START REUSE
//          INTERACTION note in section 6).
//      (b) the seed working set (immediately after start_center()/
//          ingest_seed_working_set() plus the pre-loop refresh_shifts())
//          equals the EXIT working set of that same previous solve.
//      (e) border_'s factor's own live (session_id, epoch) identity --
//          advanced by every successful factorize() inside rebuild_k0(), the
//          sole site that reassigns or refactorizes K0 -- equals the pair
//          THIS engine last saw as trustworthy, AND the factor's numerics
//          are usable (inertia().state == kObserved). (a)-(d) describe only
//          the PROBLEM being solved and cannot tell a shared HotState handle
//          (fed to a second QpEngine while the producing engine keeps
//          mutating it) apart from an untouched object with the identical
//          fingerprint; (e) can, by comparing the object's own identity.
//          This is DEFENSE-IN-DEPTH, not the mechanism that makes
//          cross-engine sharing safe -- that is DETACH (see HotState's own
//          ownership note below QpEngine): because a shared object can only
//          be mutated by an engine whose own (a)-(d) already passed, every
//          K0 ever written into a shared BorderState already matches the
//          handle's fingerprint, so a stale-identity reuse (e) alone would
//          block is not, on the evidence measured so far, a wrong-answer
//          case. (e) is kept anyway as a cheap independent check that
//          degrades a questionable reuse to kWarm.
//
//    (b) IS ONLY APPROXIMATE, AND THAT IS SAFE. refresh_shifts() can add a
//    row to ws strictly AFTER border_candidate()'s last sync_borders() call
//    on the final iteration and before drop_worst() is consulted, so
//    border_exit_active_ineq_ can understate the true exit ws. What actually
//    makes reuse safe is that sync_borders() is an UNCONDITIONAL, FULL
//    reconciliation of ws against border_'s ledger, run on EVERY iteration of
//    EVERY solve regardless of whether rebuild_k0 was skipped -- the reuse
//    fast path only ever skips rebuild_k0's assembly/factorization, never
//    sync_borders(). So even when the seed understates ws, the first
//    post-reuse sync_borders() call adds whatever border is missing (paying
//    schur_updates, not factorizations) before border_solve_or_fall_back() is
//    allowed to solve anything.
//    DO NOT skip that first sync_borders() call on the theory that a
//    matching seed ws means nothing is left to reconcile -- that would turn
//    this into a silent stale-reuse bug. (Latched exits: covered the same
//    way -- border_candidate's own latch handling re-detects "nothing
//    changed" and falls back to elimination on the next solve.)
//
//    All five conditions are NECESSARY but NOT SUFFICIENT for
//    `factorizations == 0`: border_candidate's own pre-existing checks (a
//    carried-over perturbed-pivot count, or a border stack already past
//    needs_refactorization()) can still force a rebuild regardless. Callers
//    must not assert `counters.factorizations == 0` unconditionally on a warm
//    re-solve; they should assert it only alongside control of the same
//    conditions this note documents.
//
//    INVALIDATION POLICY. border_'s cache is committed (border_valid_ set)
//    ONLY on a clean kOptimal exit, and is pessimistically cleared at the
//    top of every solve() call before anything else runs -- including before
//    qp.validate(). kMaxIter/kInfeasible/kNumericalError exits and any
//    exception thrown mid-solve therefore all leave the cache invalidated;
//    the next solve() reassembles and refactorizes from scratch. Always safe
//    to fall back to a full rebuild; the fast path exists only to skip work
//    that is provably redundant.
//
//    THREAD SAFETY. QpEngine is NOT thread-safe for concurrent solve() calls
//    on the same instance: border_ and the reuse-fingerprint members are
//    mutable state shared across calls despite solve() being const from the
//    caller's view. As of the hot-start handle (WarmStart::hot/HotState),
//    "one QpEngine per thread" is not the whole rule either: two DIFFERENT
//    QpEngine instances, on any threads, can share one BorderState object
//    (Pardiso pt_ array included) if one adopts a hot handle the other
//    produced -- a std::shared_ptr, not a lock. SEQUENTIAL hand-off (produce
//    a handle, then feed it to a different engine after that call returns)
//    is safe: the session/epoch identity detects a producer that mutated the
//    object again before the handle was consumed and degrades to kWarm.
//    CONCURRENT use of a shared BorderState -- two engines calling solve() at
//    the same time while both hold the same handle's shared_ptr -- is
//    UNDEFINED: the session/epoch identity is unsynchronized state, not
//    atomic, and provides no ordering, fence or exclusion -- the ordinary
//    meaning of a data race. A caller sharing a hot handle across threads
//    must ensure no two engines holding a copy of it ever call solve()
//    concurrently.
//
// 4b. INERTIA GATE AND TEMPORARY-VERTEX START REPAIR (indefinite H).
//
//    Every EQP solve above is a MINIMIZATION over the current working set
//    only for a convex H. For an indefinite H the regularized KKT system is
//    still nonsingular but its "answer" can be a saddle or a maximizer that
//    the loop would certify kOptimal without noticing (H = diag(1,-1), g = 0
//    on [-1,1]^2: the first EQP returns (0,0), objective 0, when the true
//    minimizers sit at (0,+/-1), objective -0.5).
//
//    THE SIGNATURE is the KKT matrix's INERTIA. For a system whose reduced
//    Hessian is positive definite on the null space of its working
//    constraints, the regularized KKT matrix [H+delta*I A^T; A -mu*I] has
//    inertia exactly (#variables, #constraint rows, 0) -- unconditional for a
//    convex H (H+delta*I positive definite makes the matrix quasi-definite
//    regardless of A's rank), which is why this gate is a no-op on every
//    convex fixture in this file's battery. Expected inertia per path:
//      - ELIMINATION (solve_eqp's bound-eliminated K): (n_free, me+n_working, 0).
//      - BORDER: pardiso reports K0's inertia alone; the whole bordered
//        matrix's expectation is (n+extra_pos, me+n_w0+extra_neg, 0), where
//        each border contributes one negative eigenvalue except a
//        kRowDelete border, which pairs with the K0 row it kills into a
//        [[-mu,1];[1,0]] block contributing one positive and one negative --
//        so extra_pos = #kRowDelete, extra_neg = dim()-#kRowDelete. The
//        actual bordered inertia is pardiso's K0 inertia plus the Schur
//        complement C's own inertia (Haynsworth); C's negative count is
//        SchurComplement::expected_neg_eigs_delta(), consulted only after
//        needs_refactorization() (it throws on a singular C).
//      Pinning a variable in border mode changes only C's factorization, not
//      K0's -- the gate must read the BORDERED system's inertia, never K0's
//      numbers against a fixed expectation, or it would be blind to every
//      pin the repair below adds.
//
//    PERTURBED PIVOTS ARE NOT A PASS AND NOT A REPAIR TRIGGER. Pardiso's
//    (n_pos, n_neg) is trustworthy IFF perturbed_pivots == 0: on an exactly singular
//    matrix it fabricates a pivot sign and reports an inertia
//    indistinguishable from the nonsingular case, without raising error -4.
//    The gate (detail::InertiaVerdict) has THREE verdicts:
//      kOk      trustworthy and matching -- proceed.
//      kSuspect non-kObserved or perturbed evidence, counts that don't sum
//               to the matrix dimension, or (border path) C past
//               needs_refactorization(). Inertia UNKNOWN -- never a pass, and
//               never treated as evidence a repair is needed (that would act
//               on a fabricated sign as ground truth). Already handled by
//               step 4's refactorization machinery (border_candidate
//               rebuilds K0 whenever evidence is untrusted).
//      kWrong   trustworthy and DISAGREEING -- the working set is
//               second-order inconsistent; at solve start this triggers the
//               repair below.
//
//    TEMPORARY-VERTEX START REPAIR. On kWrong at the FIRST major iteration
//    only (a later kWrong mid-solve is section 4c's ride to handle, with one
//    exception -- the POST-PROBE RESTART below), repair_temporary_vertex
//    walks the free variables in order of MOST NEGATIVE H diagonal and pins
//    them one at a time until the gate returns kOk. Each pin goes through the
//    ordinary bound machinery (ws.bound_state()), so the repaired state is an
//    ordinary working set. Pin target: the bound the variable is ALREADY
//    sitting on (within feas_tol), else the finite bound that lowers the
//    objective more along e_i (df(t) = grad_i*t + H_ii*t^2/2, t = bound-x(i);
//    ties to the lower bound). A variable with no finite bound on either side
//    cannot be pinned and is skipped.
//
//    THE REPAIR IS ALL-OR-NOTHING: it succeeds only if it reaches kOk, and
//    one unpinnable free variable carrying the negative curvature is enough
//    to unwind the whole repair (not confined to "no bounds at all" -- even
//    one unbounded variable among otherwise boxed ones can block it), which
//    is why the verdict is also consulted at classification time below.
//
//    SECOND-ORDER CERTIFICATION (step 5's classification). When the loop
//    reaches a point it can neither improve nor drop from, and the gate's
//    verdict for the system just solved is a TRUSTED kWrong, the point is
//    reported kNumericalError rather than kOptimal (multipliers cleared) --
//    stamping kOptimal would certify second-order optimality the engine has
//    positive evidence against. Only kWrong triggers this; kSuspect does not,
//    for the same reason it does not trigger the repair.
//
//    ZERO-MULTIPLIER PROBE. The inertia gate tests the null space of the
//    FULL active labeling, including a member whose multiplier is
//    (numerically) zero -- a weakly active constraint. If the missing
//    negative curvature lives only in the direction that constraint alone
//    excludes, the full-labeling reduced Hessian still reads positive
//    (semi)definite and a strict non-minimizer gets certified kOptimal.
//    probe_zero_multiplier_drops runs at the would-be-kOptimal exit, after
//    the infeasibility/runaway/full-labeling inertia checks pass: every
//    working-set member whose multiplier is within opt_tol of zero is
//    tentatively dropped, most-recently-added first, and the gate is re-run
//    on the reduced labeling -- kWrong makes the drop REAL (as if drop_worst
//    had released it, arming section 4c) and RESUMES the loop; kOk or
//    kSuspect restores it and moves to the next candidate. All candidates
//    exhausted without a kWrong => certify kOptimal. ANTI-CYCLING: a
//    constraint whose tentative drop was made real is never probed again for
//    the rest of that solve (ProbeState's per-solve exemption set -- at most
//    n+mi drops per solve). kSuspect during a probe is treated as kOk (the
//    same fabricated-pivot policy as above), which can let a weakly active
//    constraint whose reduced system is unreadable certify a point that is
//    stationary but not verified second-order optimal -- the narrower gap the
//    suspect-stall gate below closes for the non-stationary case.
//
//    SUSPECT-STALL GATE. A persistent kSuspect that certifies a genuinely
//    stationary point is common and correct (in border mode a pin can leave
//    K0 permanently perturbed while every bordered solve is exact), so
//    escalating on the verdict alone would trade correct answers for coarser
//    ones. Instead: when the verdict for the system the FINAL iterate was
//    solved from is kSuspect, kOptimal may be certified only after an
//    explicit free-block stationarity check on the QP model,
//    r = Hx + g + Ae^T lambda_e + Ai^T lambda_i restricted to the FREE
//    variables, against opt_tol SCALED by max(1, ||Hx||inf, ||g||inf,
//    ||Ae^T le||inf, ||Ai^T li||inf) -- a departure from this file's other
//    (absolute) opt_tol tests, argued at free_block_stationarity's own
//    comment; also NaN-aware (a NaN residual never yields kOptimal). It runs
//    AFTER the zero-multiplier probe, independently of it. On failure:
//    primal_delta is escalated one decade (detail::kSuspectDeltaFactor), the
//    border state is discarded (K0 rebuilds at the new regularization -- an
//    exact h_ii == -primal_delta cannot survive a decade bump), and the
//    iteration resumes; the ladder is bounded
//    (detail::kMaxSuspectEscalations rungs, QpCounters::suspect_escalations)
//    and reports kNumericalError when exhausted. NEVER kOptimal off a stalled
//    suspect loop, and never a hang -- the ladder is finite and max_iter
//    still bounds the loop. CAVEAT (known, unmeasured): r is built from
//    prices computed at the top of the iteration, but refresh_shifts()
//    between there and here can add rows with lambda_i == 0, which can
//    inflate the residual on a point that is in fact stationary -- a
//    spurious escalation bounded by the ladder, not a wrong answer.
//
//    ONE-AT-A-TIME IS THE KNOWN REMAINING APPROXIMATION: a critical cone that
//    opens only when TWO OR MORE weakly active constraints drop
//    simultaneously is not covered (probing combinations is exponential).
//    What checks the residual is the local-minimizer oracle battery
//    (QpEngineIndefinite.EngineLandsOnALocalMinimizer,
//    RandomizedIndefiniteBatteryLandsOnLocalMinimizers), which verifies every
//    kOptimal answer independently rather than against this file's own
//    reasoning.
//
//    POST-PROBE RESTART. A probe-driven drop's next-iteration step p is
//    identically ZERO (section 4c's p = (-lambda_c/sigma)*q vanishes when
//    lambda_c is zero, which a probe-dropped constraint's multiplier is by
//    construction), so the ride in 4c cannot arm off it and the loop lands
//    back at a trusted-kWrong classification instead of the true minimizer.
//    The fix reuses the temporary-vertex repair at exactly one place:
//      TRIGGER  the certification branch's trusted-kWrong arm, reached AFTER
//               a probe-driven drop earlier in this solve
//               (`probe_drop_made`), evaluated before kNumericalError would
//               be reported and after the structural-violation/runaway
//               checks.
//      BUDGET   once per solve (`post_probe_restart_spent`) -- the repair is
//               a heuristic with no termination argument for repeating it. A
//               solve needing a second restart reports kNumericalError with
//               the one block it did repair repaired.
//      FAILURE  if repair_temporary_vertex declines (it restores ws/x
//               itself), the original kNumericalError stands.
//    `probe_drop_made` is a PER-SOLVE sticky bit, not per-drop -- so a later
//    drop_worst drop in the same solve whose ride declines may also consume
//    the unspent restart, wider than the narrowest intended rule, but no
//    shipped fixture distinguishes the two (see the branch's own arming
//    note).
//
//    RELEASE POLICY (temporary-vertex repair). Pins are released one at a
//    time, in reverse pin order, each release KEPT only if the gate still
//    returns kOk without it (the same criterion as "the release exposed
//    negative curvature") -- otherwise the pin is restored. A pin kept
//    unnecessarily is not specially marked (no Kind::kTempVertex in the
//    border ledger -- the repair pins at BOUNDS, so ws.bound_state() already
//    represents it exactly, and the ledger is a derived view sync_borders
//    reconciles every iteration); it is superseded through the ordinary
//    route instead -- the next iteration prices it, a wrong-signed multiplier
//    sends it to step 2's drop rule, which arms section 4c. A spurious repair
//    is therefore self-correcting: its cost is extra iterations (or
//    kMaxIter), never a wrong answer, which is what makes triggering on a
//    single inertia reading acceptable. Repair moves x to a genuine vertex
//    deliberately -- pinning at the current interior value would leave a
//    saddle certified optimal, which is the bug this section fixes; the
//    resulting bound violations on general rows are cleaned up by the
//    existing refresh_shifts() homotopy afterward, same as any other start
//    point.
//
//    COST ON CONVEX PROBLEMS IS ZERO for the gate itself (it reads counters
//    off a factorization the loop runs anyway, and a convex K0/K can never
//    read kWrong). The zero-multiplier probe DOES charge a convex solve with
//    a DEGENERATE optimum -- one probe (one factorization in kRefactorize
//    mode, a handful of schur_updates in border mode) per zero-multiplier
//    candidate at each would-be-kOptimal exit; probes per solve are bounded
//    by O((n+mi)^2) in the worst case (each real drop can re-arm the gate, up
//    to n+mi times, each probing up to n+mi candidates), but the convex case
//    reaches the gate exactly once since no probe there ever reads kWrong.
//
// 4c. NEGATIVE-CURVATURE RIDES AFTER A DROP (indefinite H).
//
//    Section 4b makes an indefinite START safe; this makes an indefinite
//    DROP safe. By 4b's Cauchy-interlacing argument (adding a working row can
//    only raise the reduced Hessian's smallest eigenvalue), a drop is the
//    ONLY way negative curvature can reappear once a working set is
//    second-order consistent.
//
//    Let x be a KKT point of working set W with positive definite reduced
//    Hessian B = Z'HZ, and let the drop rule release constraint c
//    (lambda_c < 0), leaving W' with null(A_W') = null(A_W) + span{d},
//    a_c.d = -1, w = Z'Hd, gamma = d'Hd, and
//        sigma = gamma - w' B^-1 w
//    the Schur complement -- by interlacing the only eigenvalue of the new
//    reduced Hessian that can be negative. The next EQP's step works out to
//        p = (-lambda_c / sigma) * q,   q = d - Z B^-1 w,
//    with q'Hq = sigma and a_c.q = -1. So p'Hp = (lambda_c/sigma)^2 * sigma
//    has the SIGN of sigma: the curvature of the ordinary EQP step already IS
//    the test for whether the post-drop reduced Hessian stays positive
//    definite, with no extra solve. When sigma < 0, -lambda_c/sigma is
//    negative, so p points back into the just-released constraint (whose
//    slack is exactly zero); the ratio test answers alpha = 0, the
//    constraint is immediately re-added, and without this section the loop
//    cycles between W and W' until max_iter.
//
//    THE CHECK. On the iteration after a drop, and only there, the Rayleigh
//    quotient p'Hp/p'p is compared against curvature_tol =
//    kCurvatureTolFactor * hessian_scale(qp). Above it, the ordinary EQP step
//    is taken. At or below it, the QP is unbounded below along +/-p within
//    the current working set, so the loop RIDES instead: take the admissible
//    sign of p (ride_sign requires both descent and moving off the just-freed
//    constraint), run the ratio test with its unit cap DISABLED, and step to
//    the nearest blocking constraint or bound, which joins the working set
//    through the same machinery any ratio-test blocker does. A negligible p
//    is excluded before the check (already classified a KKT point by the
//    loop's own step_tol test -- the Rayleigh quotient of rounding noise is
//    meaningless and was measured to turn correct kOptimal answers into
//    spurious "unbounded" reports).
//
//    NO BLOCKER => kNumericalError, multipliers cleared, reported immediately
//    from inside the loop -- nothing stops the ride, so the QP is genuinely
//    unbounded below. The SQP driver's trust-region bounds make this branch
//    unreachable in SQP use (every variable is boxed there).
//
//    ARMING IS ONE-SHOT: the drop record is consumed on the very next
//    iteration whether the ride fires, declines, or is never reached. A
//    DECLINED ride leaves the curvature un-retested until some later drop;
//    section 4b's certification branch (refuses to certify a trusted-kWrong
//    final point) is the fallback, not equivalent coverage. THIS SECTION IS
//    VACUOUS for a probe-driven drop specifically: the probe drops
//    zero-multiplier constraints, and p = (-lambda_c/sigma)*q is then
//    identically 0, so the ride is never even offered a direction -- that
//    case falls through to section 4b's certification branch, whose
//    POST-PROBE RESTART intercepts it instead (see there).
//
//    WHICH MECHANISM FIRES: this section's no-blocker branch (mid-loop,
//    needs a preceding drop) and section 4b's certification branch
//    (classification time, needs nothing left to drop) are MUTUALLY
//    EXCLUSIVE by construction and report the same status/semantics, so
//    which one fired is not observable in the result, only in the iteration
//    count.
//
//    COST ON CONVEX PROBLEMS: one sparse mat-vec (Hp) on post-drop
//    iterations, nothing otherwise. For STRICTLY CONVEX (positive definite)
//    H the Rayleigh quotient is at least lambda_min(H) > curvature_tol
//    whenever H's 2-norm condition number is below ~1e12, so the branch is
//    UNREACHABLE and the engine is bit-for-bit unchanged (verified across a
//    508-fixture convex battery, both ws_algebra modes, cold and warm);
//    worse conditioning than that is indistinguishable from PSD-singular and
//    falls under the next case, which is intended. For PSD-SINGULAR H
//    (including H == 0, an LP) the branch IS reachable and the ride does
//    run. Behavior is NOT identical to the capped path there: the ride takes
//    the uncapped ratio while the ordinary path clamps at alpha = 1, so they
//    agree only while the blocker is nearer than the regularized step (the
//    common case). Where they differ the difference favors the ride -- on
//    randomized H == 0 LPs it recovers kOptimal from kMaxIter/kNumericalError
//    on wide boxes where the capped path runs into the unbounded-artifact
//    guard before reaching the true vertex, and no kOptimal answer was lost
//    or worsened. This is intended behavior on the PSD-singular path.
//
//    NO ANTI-CYCLING RULE IS NEEDED HERE: a ride's objective decrease is
//    strict whenever alpha > 0 (ride_sign requires non-ascending curvature
//    and strictly descending slope), so the working set it lands in cannot
//    be revisited at the same objective. alpha == 0 is still possible at a
//    degenerate vertex, covered by the file's standing Degeneracy note
//    (max_iter bound, no Bland/least-index rule).
//
// 5. TERMINATION. The loop reaches a KKT point of its working set with
//    nothing left to drop, and classifies it:
//      - kInfeasible if any inequality or equality row carries a STRUCTURAL
//        violation (violation_is_structural: clears the row's tolerance by a
//        margin AND reaches an appreciable fraction of the regularization
//        footprint dual_mu*|lambda|). Both blocks are checked -- the shift
//        machinery only watches inequalities, so an inconsistent equality
//        block or one unreachable inside the box is otherwise invisible.
//      - kNumericalError if the point is feasible but some free component
//        unbounded on the side it grew toward exceeds
//        detail::unbounded_artifact_scale() (is_runaway) -- the answer is the
//        regularization talking, not an optimum.
//      - kNumericalError if the inertia gate's verdict for the system just
//        solved is a TRUSTED kWrong (unreachable on convex H; see 4b's
//        SECOND-ORDER CERTIFICATION). ONE ESCAPE: if a probe-driven drop
//        happened earlier in this solve and the single POST-PROBE RESTART is
//        unspent, one temporary-vertex repair is attempted and, on success,
//        the loop RESUMES from the repaired vertex instead (see 4b).
//      - otherwise 4b's ZERO-MULTIPLIER PROBE gets a veto: a weakly active
//        working-set member is tentatively dropped and the gate re-run; a
//        trusted kWrong there makes the drop real and RESUMES the loop
//        without assigning a status on this pass -- the only branch here
//        that does not end the solve.
//      - kOptimal otherwise.
//    kMaxIter once eff_max_iter major iterations have been spent -- an
//    explicit opts.max_iter, or, at its default sentinel, the SIZE-DERIVED
//    QP ITERATION CAP (see section 4's block of that name). A FOURTH
//    kNumericalError exit bypasses this classification entirely: section
//    4c's ride, finding no blocker for a direction of negative curvature,
//    stops the loop mid-iteration (same semantics; cannot coincide with the
//    certification branch -- see 4c's WHICH MECHANISM FIRES note).
//
//    ORDERING IS LOAD-BEARING: the drop rule is consulted BEFORE
//    infeasibility may be declared, because a bound pinned by the ratio test
//    commonly blocks a shift from closing, and releasing it is what lets the
//    homotopy finish (QpEngine.DropRuleRunsBeforeInfeasibilityIsDeclared).
//
//    ON kInfeasible/kNumericalError the returned x is the FINAL ITERATE the
//    loop stopped at (not the least-violating point seen; no argmin is
//    kept), and multipliers are CLEARED on both -- an inconsistent or
//    runaway system prices them at O(1/dual_mu) (~1e8 at the defaults),
//    which are regularization artifacts, not prices.
//
//    TRUSTWORTHY RANGE. Infeasibility detection separates a structural
//    residual from solver noise by comparing a violation against
//    dual_mu*|lambda| via threshold kStructuralResidualFrac, which can be
//    crossed from either side:
//    (i) FALSE kInfeasible -- a feasible but ill-scaled row whose refined
//        residual still clears the fraction. Bites once |lambda| exceeds
//        roughly 1e6*row_scale (a row scaled by 1e-4 drives |lambda| to
//        5e7); at that point the regularized answer is itself
//        percent-level wrong, so kInfeasible is the honest verdict.
//        (QpEngine.IllScaledFeasibleRowsAreNotDeclaredInfeasible, down to a
//        row scale of 3e-4.)
//    (ii) FALSE kOptimal -- a genuine contradiction whose gap is smaller
//        than kStructuralResidualFrac*dual_mu*|lambda| hides beneath the
//        footprint. |lambda| need not come from the contradiction at all --
//        on an ill-scaled active row the OBJECTIVE inflates it
//        (|lambda| ~ 1/(2a^2) for a row scaled by a), so the footprint can
//        grow out from under a fixed gap; this range does not overlap (i)'s.
//        (QpEngine.ObjectiveInflatedMultiplierHidesASmallContradiction, a
//        known, accepted limitation -- tightening kStructuralResidualFrac to
//        catch it re-breaks (i). The SQP driver is the second detection
//        layer for this case; a caller using the QP engine alone should
//        scale its rows or shrink dual_mu.)
//
//    RUNAWAY-GUARD RANGE has the symmetric false positive: a genuinely free
//    variable whose TRUE optimum lies beyond unbounded_artifact_scale() is
//    indistinguishable from a regularization artifact and is reported
//    kNumericalError (e.g. at the defaults, primal_delta = 1e-8, threshold
//    1e7, a legitimate optimum of 2e7 is misreported). The threshold is
//    kUnboundedArtifactFactor/primal_delta, so it scales directly with
//    1/primal_delta: tightening primal_delta pushes the boundary out (worse
//    conditioning, larger admitted optima); loosening pulls it in. A caller
//    expecting free variables to legitimately settle at very large values
//    should tune primal_delta rather than treat this kNumericalError as
//    unconditionally load-bearing.
//
// 6. TRUST-REGION SOFT BOUNDS -- the interface the SQP driver needs: an
//    l-infinity trust region around the current SQP iterate, expressed the
//    same way every other bound is.
//
//    EFFECTIVE BOUNDS, computed ONCE about the clamped seed primal x0
//    (start_center()'s cold clamp(0,l,u) or warm clamp(seed.x,l,u)) --
//    BEFORE the seed's bound-state hints are materialized onto x0 (step 1b,
//    ingest_seed_working_set, which runs after this block; see
//    WINDOW-CONSISTENCY RULE below):
//        lo_eff(i) = max(lower(i), x0(i) - Delta),
//        up_eff(i) = min(upper(i), x0(i) + Delta),   Delta = opts.tr_radius.
//    Every subsequent bound read in this file (ratio test, is_runaway,
//    repair_temporary_vertex's pin choice, the direct bound-snap, and
//    through eqp_candidate the pinned-variable rhs in solve_eqp and
//    solve_bordered_eqp) sees lo_eff/up_eff, never lower/upper directly -- via
//    a SHADOWED QpProblem reference: `qp` inside run() names either the
//    caller's own problem unchanged (Delta == +inf) or a local copy with
//    lower/upper replaced (Delta finite), so every function taking
//    `const QpProblem &qp` is already TR-aware.
//
//    CROSSED EFFECTIVE BOUNDS CANNOT HAPPEN: step 0 already rejected a
//    crossed real box, so lower(i) <= x0(i) <= upper(i) gives lo_eff(i) <=
//    x0(i) <= up_eff(i) always (asserted, not merely hoped for).
//
//    THE kFixed FLIP. lo_eff(i) == up_eff(i) can only happen at Delta == 0
//    for a variable not already genuinely kFixed -- a zero-radius solve pins
//    x0 exactly (a legitimate "no-move evaluation"). Every such variable is
//    flipped to BoundState::kFixed before the loop runs, with tr_active set;
//    an already-kFixed real bound is left alone (tr_active stays false).
//
//    tr_active (size n, parallel to bound_state) IS A SEPARATE ACTIVITY SET,
//    NOT A NEW BoundState: true at i iff the ratio test, the temporary-vertex
//    repair, or the zero-radius flip pinned i at the TR side of its
//    effective bound rather than the real one (a coincidental tie is
//    attributed to the real bound). Applied once at the end of run(), on the
//    FINAL working set only:
//      (a) a TR-pinned variable's bound_state reports kFree, never
//          kAtLower/kAtUpper/kFixed;
//      (b) its z entry is forced to 0 -- the ratio test and drop_worst still
//          see and act on the real priced multiplier while the loop runs (a
//          TR bound participates exactly like a real one), but that number
//          is never exposed (TR duals are not prices a driver should read);
//      (c) none of this changes what the loop DID, only how the final
//          ws/z are reported -- applied after the loop, not by skipping
//          pins/pricing for TR-tight bounds during it.
//
//    WINDOW-CONSISTENCY RULE. The window is computed about the clamped seed
//    PRIMAL; a seeded bound-state hint is then applied against that window,
//    and a hint whose bound falls outside [lo_eff(i), up_eff(i)] is DROPPED
//    -- index i arrives kFree and the loop re-derives its activity from the
//    effective bounds. Applying hints FIRST let a hint at a far bound become
//    the window's own center (measured: H=I, g=(-100,-100) over [0,6]^2,
//    seeded kAtUpper with seed.x zeroed, returns (6,6) at Delta = 5, 2.5 AND
//    1.25 alike -- zeroing seed.x cannot defend against this, since the zero
//    is exactly what the hint overwrote). Clamping an out-of-window hint to
//    the window edge instead of dropping it would also break bound_state's
//    meaning (kAtLower/kAtUpper means sitting on the REAL bound); tr_active
//    is the documented channel for a TR-tight edge instead. A hint INSIDE the
//    window is honoured unchanged -- the case every ordinary warm chain
//    relies on, since the window is centered on the seed's own carried x.
//
//    WARM-START SEED INGESTION IGNORES tr_active BY CONSTRUCTION:
//    ingest_seed_working_set() reads only seed.bound_state/seed.ineq_active.
//    Combined with rule (a), a variable TR-pinned on the solve that produced
//    `seed` already arrives as bound_state == kFree and is left untouched --
//    the TR pin is not carried into the new solve's working set, matching
//    the "TR bounds excluded from working-set carryover" requirement; the
//    new solve's effective bounds are recomputed about its own x0 regardless.
//
//    HOT-START REUSE INTERACTION (border mode). K0's structural/values
//    hashes never depend on lower/upper, so a bound change (real or
//    TR-driven) can NEVER poison K0 reuse on fingerprint grounds: a pin's
//    border column is e_i (BorderOps::pin_variable), independent of the
//    bound value; only the border's RHS entry (pinned_value(qp, state, i))
//    carries the bound value, rebuilt from the CURRENT `qp` on every
//    solve_bordered_eqp call, never cached (verified with 4000 randomized
//    warm re-solves perturbing raw lower/upper between calls on the same
//    engine, zero divergence from cold solves). What a bound change CAN do is
//    change ws.bound_state() at the seed (e.g. the kFixed flip firing under a
//    new radius where it didn't before), which reuse condition (b) -- seed
//    ws vs previous exit ws -- already treats as an ordinary working-set
//    change, correctly forcing a rebuild.
//
//    PER-SOLVE RADIUS VARIATION. tr_radius lives on QpOptions, which is
//    per-instance const, but every solve() overload also takes a
//    `const SolveOverrides &` (qp_types.h) resolved ONCE at the top of run()
//    into effective tr_radius/primal_delta/dual_mu (a sentinel field
//    resolves to the corresponding opts_ value); every read site below this
//    point consults those effective values, not opts_ directly. A
//    SHRINK-RADIUS RETRY LOOP therefore shares one QpEngine across every
//    retry radius and keeps hot-start reuse across the loop wherever the
//    ordinary eligibility conditions allow (tr_radius itself never joins the
//    reuse key -- see HOT-START REUSE INTERACTION above). Reuse-key
//    extension: the effective (primal_delta, dual_mu) pair now JOINS the
//    reuse key as condition (d) above, since it enters K0's values exactly
//    as H/Ae/Ai do; tr_radius is NOT part of it, since it never reaches K0.
//
//    UNBOUNDED-ARTIFACT GUARD AND REPAIR SYNERGY. is_runaway() and
//    repair_temporary_vertex() (4b) both read bounds through the same
//    shadowed `qp`, so with a finite tr_radius every variable has a finite
//    effective bound on both sides: is_runaway() can never fire for a
//    TR-bounded variable, and repair_temporary_vertex()'s "no finite bound
//    to pin" failure mode cannot occur -- every variable becomes pinnable.
//    The SQP driver always calls through a finite trust region, so
//    kNumericalError-from-unbounded is unreachable in SQP use.
//
//    BIT-IDENTICAL OFF PATH. opts.tr_radius defaults to +inf; at that value
//    no QpProblem copy and no effective-bounds vector is ever materialized,
//    and the shadowed `qp` aliases the caller's problem directly (the
//    "zero new work" claim is scoped to exactly those two allocations).
//    QpSolution::tr_active is still allocated unconditionally on every solve
//    (all false) -- part of QpSolution's contract, not something the off
//    path skips. Every pre-existing test's counters/status/iterate are
//    unchanged bit-for-bit (TrOffIsBitIdenticalDefault).
//
// 6b. THE EXPORT INVARIANT ON z: A FREE VARIABLE CARRIES NO BOUND PRICE.
//    For every index i, on EVERY status this engine can return:
//        bound_state[i] == kFree  =>  z(i) == 0.0
//    Enforced at the point of export in run() (see the enforcement site at
//    the bottom of run()) because price() imposes this against the working
//    set live at PRICING time, and a kMaxIter exit leaves the loop after
//    later mutations without re-pricing. `refine_on_face` -- the engine's
//    other public QpSolution producer -- satisfies the same invariant by a
//    DIFFERENT route (price()'s own postcondition plus its own TR-exclusion
//    pass), not by calling into this loop; a third producer must re-derive
//    the invariant rather than assume it is inherited.
//
//    ONE-WAY GUARANTEE: a PINNED index's z is the price the last price() call
//    computed, which on a kMaxIter exit may be one working set stale -- a
//    caller needing fresh prices needs a converged solve. `kFixed`
//    (lower == upper) is silently outside this invariant (stated over
//    bound_state == kFree; drop_worst notes a fixed variable is "not
//    sign-constrained", so it never enters either side).
//
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
// defaults to the sentinel 0, meaning "derive the cap from this
// subproblem's size"; a positive value is an explicit absolute cap and wins
// outright. These three helpers are the whole of the derivation, factored
// out of run() so they can be tested without solving anything.
//
//     base  =  n + mi + #bounded          (qp_cap_base)
//     cap   =  max(kQpMaxIterFloor, kQpMaxIterCoeff * base)
//
// The identification walk's demand scales with the number of working-set
// events it can touch -- `mi` plus the number of variables with a finite
// bound -- with `n` carried alongside since the EQP's own dimension bounds
// how many independent directions the walk can traverse before repeating.
//
// #bounded COUNTS *REAL* BOUNDS, NOT EFFECTIVE ONES -- load-bearing, not
// cosmetic: the SQP driver passes a FINITE trust-region radius on every
// subproblem, so counting the EFFECTIVE box would make #bounded == n
// unconditionally regardless of the problem's actual geometry. qp_cap_base
// therefore reads the CALLER'S OWN qp.lower/qp.upper, before section 6's
// window is applied.
//
// kQpMaxIterCoeff = 5 is a measured, two-sided calibration: large enough to
// clear the largest observed per-subproblem demand with real headroom,
// because the two failure directions are asymmetric -- too small LOSES a
// solve that would have converged (the driver exits kNumericalError nowhere
// near x*), while too large only costs TIME on a subproblem that was never
// going to converge. kQpMaxIterFloor keeps small problems (where `base` is
// tiny but demand is not proportionally tiny) from being capped below their
// working demand.
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
// from size. Every existing tiny-cap battery and fixture sets max_iter
// explicitly and therefore keeps working unchanged, including the ones that
// deliberately cap at 1 or 2 to force a kMaxIter exit.
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
// regularization allowance in row_tolerance() may absorb.
//
// The allowance exists because a CONSISTENT working row carries an
// irreducible dual_mu*|lambda| residual (see row_tolerance). But on an
// INCONSISTENT row that same identity is self-fulfilling: the regularized
// solve satisfies Ai x - dual_mu*lambda = bi by construction, so the residual
// IS dual_mu*|lambda| exactly, and lambda grows without bound as the
// contradiction sharpens. Uncapped, the allowance would therefore always
// cover the violation and kInfeasible would be unreachable. Capping it well
// below any violation worth reporting keeps the allowance doing its job for
// merely badly scaled rows while leaving genuine contradictions detectable.
constexpr double kInfeasibilityAbsorbTol = 1e-6;
// Runaway threshold for the unbounded-artifact guard, as a fraction of
// 1/primal_delta.
//
// When a QP is unbounded below in some direction, the only thing stopping the
// regularized solve is the primal_delta ridge: stationarity degenerates to
// delta*x = -g, so the iterate runs off to ~|g|/primal_delta. That -- not any
// comparison of delta*||x||^2 against a fixed constant -- is the signature
// being detected, and it is why the threshold is expressed relative to
// 1/primal_delta rather than as an absolute size.
//
// 0.1/primal_delta is 1e7 at the defaults. The observed artifact on the
// PSD-singular probe reaches ~2e8, so this keeps an order of magnitude of
// clearance beneath it while still admitting genuinely large physical-scale
// solutions (1e5, 1e6) that an absolute cutoff would libel.
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
// factorized. Three verdicts, not two: "the inertia is wrong" and "the
// inertia is unknowable" demand different responses, and collapsing them
// would mean acting on a pardiso-fabricated pivot sign as if it were ground
// truth.
enum class InertiaVerdict {
    kOk,      // trustworthy AND equal to the expectation
    kSuspect, // untrustworthy: perturbed pivots, or counts that do not sum to n
    kWrong,   // trustworthy AND different from the expectation
};

// Gate one factorization's InertiaEvidence against an expected (positive,
// negative) eigenvalue count. `expected_pos + expected_neg` must be the
// factorized matrix's dimension -- an expectation of zero zero-eigenvalues
// is part of what is being asserted. Call sites pass
// `kkt.factor.inertia()`, the evidence of the most recent factorization.
//
// Rule order:
//   1. non-kObserved evidence -> kSuspect: no factorization has produced
//      counts (pre-factorization, failed factorization, a failed backend
//      query).
//   2. perturbed pivots (present and nonzero) -> kSuspect. A factorization
//      pardiso had to perturb reports an inertia that looks exactly like a
//      genuine one -- including when it happens to MATCH the expectation,
//      the silent-failure direction this guards -- and pardiso raises no
//      error for that matrix either, so there is nothing else to catch it.
//      Absent evidence (Accelerate has no such counter) does not trigger
//      this rule -- nothing is fabricated from absence.
//   3. short sum -> kSuspect. The two sign counts account for every
//      eigenvalue except the zero class, so a short sum means the
//      factorization did not see the matrix this expectation describes (a
//      zero eigenvalue -- derived on MKL, measured natively on Accelerate).
//   4. exact match -> kOk, else kWrong.
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
// kSuspectDeltaFactor and resume, at most kMaxSuspectEscalations times.
//
// ONE DECADE PER RUNG, mirroring the driver's elastic penalty ladder
// (kElasticRhoFactor in globalization/sqp/elastic.h) so the project has one escalation
// convention rather than two. A decade is also the smallest bump that is
// certainly enough for the shape this exists for: the stall needs a Hessian
// diagonal to cancel primal_delta to within the backend's zero tolerance
// (~1e-20 relative), and no such cancellation survives a 10x change in delta.
//
// THREE RUNGS, i.e. delta 1e-8 -> 1e-5 at the defaults. The first rung is what
// the measured failure needs; the other two are for a system whose singularity
// is structural rather than an exact cancellation, where a coarser
// regularization may still be the difference between a solvable and an
// unsolvable K. Past three the regularization is coarse enough that a
// "solution" would be a delta-artifact rather than an answer, and reporting
// kNumericalError is the honest outcome. The ladder is per-SOLVE: a warm
// re-solve starts back at the caller's own primal_delta.
constexpr double kSuspectDeltaFactor = 10.0;
constexpr Index kMaxSuspectEscalations = 3;

// Section 4b's SUSPECT-STALL GATE, invariant half (ii). The QP MODEL's
// stationarity residual r = H x + g + Ae^T lambda_e + Ai^T lambda_i,
// restricted to the FREE variables (the same r QpEngine::price builds, read
// on the complementary set of coordinates: a pinned variable's r(i) IS the
// bound multiplier z(i), which the drop rule's sign test already judges,
// while a free variable's z(i) is zero by construction, so r(i) there is
// the whole first-order condition, and a nonzero one witnesses that the
// point is not a KKT point of ANY labeling). Returns the inf-norm over the
// free block and reports, through `scale`, the largest term that went into
// r -- the denominator the caller's opt_tol is applied to.
//
// THE THRESHOLD IS SCALED, unlike this file's other (absolute) opt_tol
// tests: this one reads a GRADIENT RESIDUAL, which scales with the
// objective (multiplying f by 1e6 multiplies r by 1e6), so an absolute test
// would refuse every suspect exit on a large-objective problem. scale >= 1
// always, so the threshold is never tighter than the drop rule's own
// absolute opt_tol -- errs toward false REFUSALS (a bounded, recoverable
// kNumericalError) rather than a false certificate (unrecoverable).
//
// NaN IS A FAILURE, NOT A PASS. std::max(worst, NaN) returns `worst`, so a
// naive accumulation would make a NaN residual component VANISH and
// certify kOptimal on a factorization that produced NaN -- the defect
// class this gate exists to close. The accumulation below is therefore
// NaN-STICKY (the negated-comparison alternative is also wrong, in the
// opposite direction -- see its own comment), and the VERDICT is taken by
// free_block_is_stationary below rather than by a bare comparison at the
// call site.
//
// Deliberately NOT a general-purpose KKT checker: it ignores primal
// feasibility and multiplier signs, both already judged by the
// classification branch that calls it.
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
        // NaN IS STICKY, and it takes an explicit test to make it so. BOTH
        // one-liners are wrong here, in opposite directions, and each was
        // measured wrong by the test rather than reasoned about:
        //   std::max(worst, ri)  DROPS a NaN ri and returns `worst`;
        //   !(ri <= worst)       lets a NaN worst be OVERWRITTEN by the next
        //                        finite component (every comparison against
        //                        NaN is false, so the negation always fires).
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

// The suspect-stall gate's DECISION, factored out of run() so the
// comparison and its test cannot drift apart. `residual`/`scale` are
// free_block_stationarity's two outputs.
//
// WRITTEN AS `<=`, NOT `!(>)`: every comparison against NaN is false, so
// `residual <= tol` is FALSE for a NaN residual and the caller's
// `!stationary` routes it to the escalation ladder. `residual > tol`
// directly would instead answer "not a violation" for a NaN and certify
// kOptimal -- one character apart, and only one reading is safe.
inline bool free_block_is_stationary(double residual, double scale, const QpOptions &opts) {
    return residual <= opts.opt_tol * scale;
}

// --- Negative-curvature ride (see the header contract's section 4c) ---

// Relative threshold below which a direction's curvature counts as NOT
// positive. It is applied to the RAYLEIGH QUOTIENT p'Hp / p'p, which is the
// value of H's quadratic form restricted to span{p} and therefore lives on H's
// own EIGENVALUE scale -- so the threshold has to be relative to that scale
// too, or it would classify H and 1000*H (the same QP with a rescaled
// objective) differently.
constexpr double kCurvatureTolFactor = 1e-12;

// Relative tolerance on "the ride direction lies in the null space of the
// working constraints" (ride_stays_in_working_set). Stated relative to
// ||a_j|| * ||p||, the scale of the inner product being tested.
//
// 1e-8 rather than something near machine epsilon: the EQP is solved through a
// dual_mu-regularized KKT system, so even a perfectly consistent working row
// carries an irreducible dual_mu*|lambda| residual (the same footprint
// row_tolerance() exists for), and iterative refinement shrinks but does not
// erase it. The cases this must REJECT miss by orders of magnitude more than
// that -- an over-determined working set drives |lambda| to ~1/dual_mu and the
// residual to O(1) -- so the ACCEPT/REJECT decision is not delicate.
//
// What this tolerance does NOT bound is the residual at the LANDING point.
// The test is direction-relative (|a_j . p| vs ||a_j|| * ||p||), and the ride
// then travels alpha along p, so the drift it admits is amplified by ~alpha.
// That is a real, if secondary, effect -- see
// tests/test_qp_engine_indefinite.cpp:1742-1748 (repro seed 777 case 1613)
// for the measured case. It is NOT, however, the cause of the
// residual-driven kInfeasible discussed there: that one arises at modest
// alpha and ||p||, from the regularized EQP for the post-ride working set
// rather than from drift admitted here, so tightening this constant would
// not address it -- the classifier's row_tolerance is the lever that
// actually governs it.
constexpr double kRideNullspaceTol = 1e-8;

// The H-scale kCurvatureTolFactor multiplies: the largest magnitude of any
// stored entry of H, floored at 1.
//
// max|H_ij| brackets ||H||_2 (the honest scale for a Rayleigh quotient,
// which lies in [lambda_min(H), lambda_max(H)]) within a factor of n, costs
// one pass over the stored nonzeros instead of an eigensolve, and is
// exactly homogeneous in H, so scaling the objective by a constant leaves
// every ride decision unchanged.
//
// FLOORED AT 1 so a pure-LP H (entirely zero) still yields a usable (plain
// absolute, 1e-12) tolerance rather than 0: there the curvature is exactly
// 0, the ride branch is taken, and that is CORRECT -- after a drop an LP's
// freed edge direction has no curvature and its objective decreases
// linearly until a constraint blocks, exactly the step the ride makes (and
// the step the pre-ride code already made, by a different route).
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
// matrices, never from an assembled K0 -- that is what lets the reuse check
// run without paying for assembly even when it turns out reuse is not
// possible. Neither ever touches g, be, or bi: K0's assembly (assemble_kkt_
// core) never reads them, so a solve that only perturbs g/b cannot change
// either fingerprint.

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

// Mixes rows/cols/nnz as a separator BEFORE the value bytes, to guard
// against a real collision class where two matrices with different SHAPE
// produce byte-identical value streams (e.g. mi=2 rows [1,0],[-1,0] vs mi=1
// row [1,-1] -- same value bytes, different mi). structural_hash is the
// primary guard against that class and is always checked alongside this one
// in a reuse decision (see the header contract's HOT-START REUSE note), but
// mixing the shape in here too keeps values_hash collision-resistant across
// a shape change.
//
// A caller-supplied QpProblem matrix is not guaranteed to already be
// COMPRESSED (unlike the KKT matrix K, which this engine assembles itself),
// so this hasher only pays for a compressed COPY when the input actually
// needs one.
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
// The digest is hven's combined pattern key (feed_pattern, docs/pattern-hash.md
// -- one Fnv1a threaded across the three matrices), not a raw-byte FNV over
// the index arrays: it does not depend on host byte order or on
// SpMatRM::StorageIndex's width, and hashes an uncompressed caller-supplied
// matrix IN PLACE. No consumer compares this digest against anything but
// another computation of this same function in the same process; 0 stays
// meaningful only as WarmStart::structure_hash's "no claim made" sentinel.
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

// Border mode's entire persistent state (unused under kRefactorize): the
// one full-variable K0 the loop keeps factorized, the working rows built
// INTO it, and the live border stack that carries it from that working
// set to the current one. An empty `schur` means "K0 has not been built
// yet".
//
// This struct is an ENGINE-INSTANCE member (border_ below), not a
// per-solve local -- that is what lets a warm re-solve on the same
// QpEngine reuse K0's factorization outright. See the header contract's
// HOT-START REUSE note for the conditions under which that reuse is
// taken, and the INVALIDATION POLICY for how a non-kOptimal exit or a
// thrown exception is kept from leaving this in a state a later solve()
// would wrongly trust.
//
// K0 gets its OWN KktFactor, separate from the one the elimination path
// uses. That is what lets an iteration fall back to solve_eqp (see
// border_candidate) without destroying K0's factorization: solve_eqp
// factorizes a different, bound-eliminated matrix, and if the two shared
// a factor every fallback would silently invalidate the border stack's
// cached K0^-1 v columns. Copy and move are DELETED rather than merely
// discouraged: `schur` holds a reference to the `kkt` member, so a moved
// BorderState would leave the SchurComplement pointing at the corpse.
//
// `latched` marks the pins-only dead end (see border_candidate): bordering
// has been abandoned for the current working-set SHAPE, the elimination
// path is serving every iteration, and the border stack is therefore
// deliberately NOT kept in sync -- it is stale until the latch releases
// and rebuild_k0 rebuilds it from scratch.
//
// QpEngine's own `border_` member (below) holds this struct through a
// std::shared_ptr rather than by value: a BorderState must live at a FIXED
// heap address for `schur`'s reference into `kkt` to stay valid (why
// copy/move are deleted below), and a std::shared_ptr is what lets a SECOND
// QpEngine instance -- one that never ran the solve() call that built this
// BorderState -- share that same fixed address without either engine's
// destructor double-freeing the Pardiso handle `kkt` owns. See HotState's
// own OWNERSHIP note for the full argument.
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
    // nothing (the factorize IS the stamp). See QpEngine::reuse_eligible's
    // condition (e) and HotState's kkt_session_id/kkt_epoch pair; the
    // usable-numerics conjunct there closes the failed-rebuild case.
};

// HOT-START LEVEL. The opaque handle behind warm_start.h's WarmStart::hot --
// forward-declared there (`struct HotState;`) and DEFINED here, where the
// machinery it snapshots lives. A HotState is a frozen copy of the
// fingerprint/exit-state members QpEngine::run() already tracks per
// instance (see the HOT-START REUSE note above QpEngine), plus shared
// ownership of the BorderState those fingerprints describe -- the K0
// symbolic analysis and last numeric factorization, Pardiso pt handle
// included.
//
// OWNERSHIP. hven::linear::SymmetricFactor is move-only with a
// non-throwing destructor that releases its backend session exactly once;
// BorderState wraps one BY VALUE and is itself non-copyable and
// non-movable (its `schur` holds a reference into its own `kkt`). `border_`
// (QpEngine's own member) is a std::shared_ptr<BorderState>; HotState::border
// is a COPY of that same shared_ptr, never a raw pointer. So the factor's
// backend session is released exactly once, when the LAST shared_ptr
// referencing the BorderState is destroyed, regardless of how many engines
// have copied the pointer meanwhile, and any QpEngine/WarmStart/HotState
// holding a copy keeps the object (and its Pardiso pt_ array) alive for as
// long as it needs it -- an engine adopting a hot handle can factorize/
// solve against it safely even if the producing engine has since been
// destroyed.
//
// LIFETIME SAFETY ALONE DOES NOT GUARANTEE THE CONTENTS STILL MATCH A
// HOLDER'S FROZEN FINGERPRINT: a producer that solves again on its own
// engine after emitting a handle can mutate the SAME shared BorderState a
// consumer is about to adopt. TWO MECHANISMS close this:
//   - DETACH (`border_ = std::make_shared<BorderState>()` at a refused-reuse
//     site whose `border_.use_count() > 1` -- i.e. actually shared -- rather
//     than mutating the existing object in place; a sole-owned object is
//     still wiped in place, to keep its KktFactor's symbolic analysis
//     reusable) is the LOAD-BEARING fix: once a refused, shared reuse
//     allocates a fresh object, the only engine that can ever write into a shared
//     BorderState is one whose own conditions (a)-(e) already passed for
//     it, so every K0 ever written into a shared object already carries the
//     values any handle's fingerprint describes, and sync_borders()'s
//     unconditional reconciliation (see the HOT-START REUSE note) absorbs
//     the remaining difference in which rows are folded into K0 versus
//     carried as borders.
//   - The factor's IDENTITY pair (`kkt.factor.session_id()`/`epoch()`,
//     advanced by every rebuild_k0()) plus this engine's own committed copy
//     is DEFENSE-IN-DEPTH, not the safety mechanism itself: reuse_eligible's
//     condition (e) compares a HotState's frozen pair against a LIVE read
//     off the shared object on every solve() call, which also catches the
//     symmetric ordering (consumer adopts, producer solves again, consumer
//     solves again). A failed rebuild (which bumps generation but not the
//     epoch) is separately closed by condition (e)'s usable-numerics
//     conjunct (`inertia().state != kObserved`). A mismatch degrades to
//     kWarm silently, never a throw.
//
// CONCURRENCY, NOT SEQUENCING, IS WHAT REMAINS UNSAFE: the session/epoch
// identity is ordinary unsynchronized state -- it detects a stale handle
// because engine calls happen one at a time, and detects nothing if two
// QpEngine instances sharing a BorderState call solve() CONCURRENTLY (a
// plain data race on the factor's identity state, `border_->kkt`'s backend
// session, and every other member). See the THREAD SAFETY note below
// QpEngine's declaration for the canonical statement of this rule.
//
// SAME-PROCESS ONLY (per warm_start.h): HotState is never serialized -- a
// std::shared_ptr and a live Pardiso pt_ array have no on-disk
// representation.
//
// WHICH K0 A HANDLE DESCRIBES AFTER AN ELASTIC/SOC RE-SOLVE. sqp_driver.h's
// elastic and SOC re-solves both call engine_.solve() again on the same
// engine instance the ordinary majors use. An ELASTIC re-solve builds an
// AUGMENTED (original-plus-slack) K0, so a handle emitted right after it
// SILENTLY FORFEITS kHot for the next major (a later probe hashes against
// the unaugmented problem and cannot match) -- safe, just degraded to
// kWarm. An SOC re-solve shifts only be/bi (build_soc_subproblem); H/Ae/Ai
// and K0 are unchanged, so a post-SOC handle keeps its fingerprints and
// remains an ordinary (a)-(e)-gated reuse candidate.
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
    // never a live re-read off the possibly-shared object. analyze() moving
    // the session id covers the pattern-rebuild case; factorize() advancing
    // the epoch covers every numeric rebuild; together they are the
    // object-identity half of hven's (pattern_hash, session_id, epoch)
    // naming triple (the pattern member is carried by `structural_hash`,
    // condition (a)). Condition (e) -- see QpEngine::run()'s
    // reuse_eligible and the OWNERSHIP note above.
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
    void attach_ledger(Ledger *ledger, std::string label_prefix) {
        ledger_ = ledger;
        label_prefix_ = std::move(label_prefix);
        solve_counter_ = 0;
    }

    // Cold start from clamp(0, l, u) with an empty working set. Forwards a
    // default-constructed SolveOverrides, which resolves to every opts_
    // value unchanged -- see qp_types.h's SolveOverrides and the header
    // contract's PER-SOLVE RADIUS VARIATION note.
    QpSolution solve(const QpProblem &qp) const {
        return run(qp, nullptr, false, SolveOverrides{});
    }

    // Warm start from `seed`'s point and working set. Same
    // default-overrides forwarding as above.
    QpSolution solve(const QpProblem &qp, const QpSolution &seed) const {
        return run(qp, &seed, true, SolveOverrides{});
    }

    // Cold start with a per-solve override of tr_radius/primal_delta/
    // dual_mu (see qp_types.h's SolveOverrides). Resolved once, at the top of
    // run(), into the effective values every read site below actually
    // consults.
    QpSolution solve(const QpProblem &qp, const SolveOverrides &overrides) const {
        return run(qp, nullptr, false, overrides);
    }

    // Warm start with a per-solve override, combining both of the above.
    QpSolution solve(const QpProblem &qp, const QpSolution &seed,
                     const SolveOverrides &overrides) const {
        return run(qp, &seed, true, overrides);
    }

    // Warm start with a per-solve override AND a hot
    // handle (WarmStart::hot) from a PRIOR solve -- typically on a DIFFERENT
    // QpEngine instance (a fresh driver's engine_, per sqp_driver.h's own
    // START LEVEL RESOLUTION), offered for THIS engine to adopt as its own
    // border-mode cache if it does not already have a valid one -- see
    // run()'s ADOPT AN EXTERNAL HOT HANDLE step for exactly when adoption
    // happens. `hot` may be null (falls back to the 3-arg overload's
    // behaviour exactly) or stale/foreign (the engine's own reuse-eligibility
    // conditions (a)-(e) are the ONLY gate on whether adopting it actually
    // skips a factorization -- see qp_engine.h's HOT-START REUSE note; a
    // mismatch silently costs the ordinary rebuild instead of misbehaving).
    QpSolution solve(const QpProblem &qp, const QpSolution &seed, const SolveOverrides &overrides,
                     const std::shared_ptr<const HotState> &hot) const {
        return run(qp, &seed, true, overrides, hot);
    }

    // A shared, opaque snapshot of this engine's CURRENTLY valid
    // border-mode cache -- what warm_start.h's make_warm_start (called
    // from sqp_driver.h) attaches to WarmStart::hot on every exit. Returns
    // nullptr whenever border_valid_ is false: no solve() on this instance
    // has yet ended kOptimal (a fresh engine, an engine whose only solves so
    // far failed, or ws_algebra == kRefactorize, under which border_valid_
    // is never set at all -- see the header contract's HOT-START REUSE note,
    // "border mode only"). See HotState's own OWNERSHIP note, immediately
    // above BorderState, for what sharing the returned pointer does and does
    // not make safe.
    //
    // Emits `border_kkt_session_id_`/`border_kkt_epoch_` (this engine's own
    // COMMITTED pair, from its own last successful run()), NEVER a LIVE
    // `border_->kkt.factor` read off the possibly-shared object: the two
    // usually agree, but a DIFFERENT engine sharing `border_` may have
    // rebuilt it since (exactly the sharing this handle exists to support),
    // and emitting the live pair then would mint a SELF-CONSISTENT FORGED
    // HANDLE -- a snapshot whose fingerprint fields describe this engine's
    // own last problem (correct) but whose identity matches the object's
    // CURRENT, foreign-mutated state (wrong), so a later adopter's
    // condition (e) would pass on an object this engine never certified as
    // representing that identity.
    std::shared_ptr<const HotState> hot_state() const {
        if (!border_valid_) {
            return nullptr;
        }
        return std::make_shared<const HotState>(
            HotState{border_, border_structural_hash_, border_values_hash_, border_effective_delta_,
                     border_effective_mu_, border_exit_bound_state_, border_exit_active_ineq_,
                     border_kkt_session_id_, border_kkt_epoch_});
    }

    // TIER 3: EXACT REFINEMENT ON AN EXTERNALLY IDENTIFIED FACE.
    //
    // ONE exact equality-constrained solve on the face `face` names, plus this
    // engine's ordinary iterative-refinement step -- i.e. `solve_eqp`, the same
    // function the walk's own per-minor `eqp_candidate` calls, reached here
    // WITHOUT a walk. It is "stable-face refinement": a kernel that
    // IDENTIFIES an active set to its own tolerance (today: the
    // semismooth-Newton tier, ssn_engine.h) hands that set here and gets back
    // the point the set determines EXACTLY, rather than the point its own
    // residual tolerance was willing to stop at.
    //
    // WHY THIS IS THE PRINCIPLED REPAIR AND NOT A POLISH STEP. An FB kernel
    // stopping at |phi| <= fb_tol certifies only `min(s, lambda) = O(fb_tol)`,
    // so its per-row complementarity is bounded by `fb_tol * ||lambda||inf`,
    // which does not vanish with the step -- whereas an ACTIVE-SET solve's
    // complementarity is an EXACT identity (a row outside the working set is
    // absent from the KKT system, so its price is zero to machine precision,
    // and a row inside it is driven to `Ai_j p = bi_j`). Re-solving the
    // identified face here restores that exact identity by construction.
    //
    // WHAT IS AND IS NOT GATED HERE.
    //   GATED: the refined point must be a legal answer to the SUBPROBLEM --
    //     finite, inside the real box, and inside the trust region. Those are
    //     the same three properties `run()` guarantees its own caller, and the
    //     funnel's ratio test presumes them. An EQP solve does not know about
    //     the constraints that are OFF the face, so it can walk out of the box
    //     if the face was wrong; when it does, this function REFUSES and the
    //     caller keeps the certificate it already had. Also gated: every
    //     inequality row NOT on the face must still be satisfied at the refined
    //     point, to the row-scaled feasibility tolerance -- the same statement
    //     that the identified face was the right one.
    //   NOT GATED: the sign of the refined multipliers. The face is the
    //     CALLER's; this function re-solves it exactly and does not re-judge
    //     it, exactly as `eqp_candidate` does not (the walk's drop rule, a
    //     separate step, is what judges prices). The driver's own model-level
    //     KKT test is unchanged and is where a bad face shows up.
    //
    // THE RANK PRE-SCREEN, before anything is factorized: a face with more
    // equality rows (model equalities + working inequalities) than free
    // variables cannot be a regular face, and handing its singular K to the
    // backend would trade a usable answer for a thrown Pardiso error. Refused
    // instead. Numerical (as opposed to structural) singularity is caught one
    // step later by the SAME `detail::inertia_verdict` gate the walk applies to
    // its own EQP candidate.
    //
    // COST AND STATE. `out.counters` reports ONLY what THIS call paid (at most
    // one factorization) -- never the caller's own, which the caller still
    // owns. NOTHING PERSISTENT IS TOUCHED: no `border_`, no hash, no
    // `border_valid_`, no ledger record, no `solve_counter_`. This function is
    // invisible to the hot-start machinery and to every ledger consumer, which
    // is also why it cannot perturb a walk-mode solve in any way.
    //
    // Returns true iff the refinement was ACCEPTED. `out` is written either
    // way: on refusal it is `face` verbatim (with this call's own cost), so a
    // caller may use it unconditionally.
    //
    // PUBLIC-API PRECONDITION: the trust-region gate below assumes its window
    // is centred at `clamp(0, l, u)` -- true today only because every seeding
    // site zeroes `seed.x` before it reaches this function, so
    // `start_center()`'s own centre agrees with the clamped origin. This
    // function takes no centre parameter and does NOT validate that
    // assumption itself. A caller that seeds from a non-zeroed point gets a
    // window gated about the wrong centre, silently.
    bool refine_on_face(const QpProblem &qp_in, const QpSolution &face,
                        const SolveOverrides &overrides, QpSolution &out) const {
        out = face;
        out.counters = QpCounters{};

        qp_in.validate();
        const Index n = qp_in.n();
        const Index me = qp_in.me();
        const Index mi = qp_in.mi();

        // A face this function cannot read is a face it must not guess at.
        if (face.x.size() != n || static_cast<Index>(face.bound_state.size()) != n ||
            static_cast<Index>(face.ineq_active.size()) != mi || !face.x.allFinite()) {
            return false;
        }

        const QpOptions eff_opts = resolve_effective_options(opts_, overrides);

        // --- The face, transcribed into a WorkingSet ----------------------
        //
        // TRUST-REGION PINS ARE PART OF THE FACE HERE, and that is the one
        // place this transcription differs from `ingest_seed_working_set`'s.
        // That function deliberately IGNORES `tr_active`, because a radius
        // artefact from one major must never be re-asserted as a genuine bound
        // in the next. This function is not looking at a different major: it is
        // re-solving THE SAME subproblem the caller just solved, whose box
        // really does include the radius, so a variable the caller reports
        // TR-pinned is a pinned variable OF THIS FACE. It is pinned at the
        // value the caller stopped at -- which IS the trust-region bound, the
        // caller having been solved under the same radius -- by a local copy of
        // the problem with that variable fixed.
        WorkingSet ws(n, mi);
        const bool tr_enabled = std::isfinite(eff_opts.tr_radius);
        // A `tr_active` flag means nothing without a radius in force -- with
        // the +inf sentinel there is no trust region for a pin to belong to --
        // so the flags are read only when this solve HAS one.
        const bool have_tr = tr_enabled && static_cast<Index>(face.tr_active.size()) == n;
        bool any_tr_pin = false;
        for (Index i = 0; i < n; ++i) {
            const auto si = static_cast<std::size_t>(i);
            if (qp_in.lower(i) == qp_in.upper(i)) {
                ws.bound_state()[si] = BoundState::kFixed;
                continue;
            }
            if (have_tr && face.tr_active[si]) {
                any_tr_pin = true;
                continue; // pinned below, against the local copy
            }
            const BoundState st = face.bound_state[si];
            if (st == BoundState::kAtLower && qp_in.lower(i) > -detail::kEngineInfBound) {
                ws.bound_state()[si] = BoundState::kAtLower;
            } else if (st == BoundState::kAtUpper && qp_in.upper(i) < detail::kEngineInfBound) {
                ws.bound_state()[si] = BoundState::kAtUpper;
            }
        }
        QpProblem tr_problem;
        if (any_tr_pin) {
            tr_problem = qp_in;
            for (Index i = 0; i < n; ++i) {
                const auto si = static_cast<std::size_t>(i);
                if (face.tr_active[si]) {
                    tr_problem.lower(i) = face.x(i);
                    tr_problem.upper(i) = face.x(i);
                    ws.bound_state()[si] = BoundState::kFixed;
                }
            }
        }
        const QpProblem &qp = any_tr_pin ? tr_problem : qp_in;
        for (Index r = 0; r < mi; ++r) {
            if (face.ineq_active[static_cast<std::size_t>(r)]) {
                ws.add_ineq(r);
            }
        }

        const Index n_working = static_cast<Index>(ws.active_ineq().size());
        if (ws.num_free() == 0 && me == 0 && n_working == 0) {
            // Every variable pinned and nothing to solve: the face already
            // DETERMINES x and there is no refinement to make (the same
            // empty-system case `eliminated_candidate` short-circuits, and it
            // costs no factorization there either).
            return false;
        }
        if (me + n_working > ws.num_free()) {
            return false; // THE RANK PRE-SCREEN -- see the note above
        }

        // --- One exact solve on that face ---------------------------------
        detail::KktFactor kkt;
        ++out.counters.factorizations;
        const EqpResult eqp = solve_eqp(qp, ws, kkt, eff_opts);
        out.counters.eqp_refine_steps += eqp.refine_steps; // identically 0; see eqp_solve.h
        if (detail::inertia_verdict(kkt.factor.inertia(), ws.num_free(), me + n_working) !=
            detail::InertiaVerdict::kOk) {
            return false;
        }
        if (eqp.x.size() != n || !eqp.x.allFinite()) {
            return false;
        }

        // --- The gate: is this a legal answer to the SUBPROBLEM? ----------
        //
        // The window is the one `run()` would have built for this same
        // subproblem: centred on the clamped origin (the driver zeroes a seed's
        // primal at every seeding site, so the centre is `clamp(0, l, u)`) and
        // intersected with the real box. Compared to `feas_tol`, the same
        // tolerance every other feasibility statement in this engine is made
        // against, so no new tolerance is introduced.
        for (Index i = 0; i < n; ++i) {
            const double lo = qp_in.lower(i);
            const double up = qp_in.upper(i);
            double lo_eff = lo;
            double up_eff = up;
            if (tr_enabled) {
                const double c = std::min(std::max(0.0, lo), up);
                lo_eff = std::max(lo, c - eff_opts.tr_radius);
                up_eff = std::min(up, c + eff_opts.tr_radius);
            }
            if (eqp.x(i) < lo_eff - eff_opts.feas_tol || eqp.x(i) > up_eff + eff_opts.feas_tol) {
                return false;
            }
        }
        if (mi > 0) {
            const Vec Aix = qp.Ai * eqp.x;
            for (Index r = 0; r < mi; ++r) {
                if (face.ineq_active[static_cast<std::size_t>(r)]) {
                    continue; // on the face: driven to equality by the solve itself
                }
                const double row_scale = std::max(1.0, qp.Ai.row(r).cwiseAbs().sum());
                if (Aix(r) - qp.bi(r) > eff_opts.feas_tol * row_scale) {
                    return false;
                }
            }
        }

        // --- Accepted. Price it and report it in the caller's own shape ---
        Vec lambda_e = Vec::Zero(me);
        Vec lambda_i = Vec::Zero(mi);
        Vec z = Vec::Zero(n);
        price(qp, ws, eqp, lambda_e, lambda_i, z);
        if (have_tr) {
            // Section 6's reporting exclusion, applied here for the same
            // reason `run()` applies it: a trust-region pin is not a bound of
            // the caller's problem, so it may not leave a price behind.
            for (Index i = 0; i < n; ++i) {
                if (face.tr_active[static_cast<std::size_t>(i)]) {
                    z(i) = 0.0;
                }
            }
        }
        out.x = eqp.x;
        out.lambda_e = std::move(lambda_e);
        out.lambda_i = std::move(lambda_i);
        out.z = std::move(z);
        // bound_state/ineq_active/tr_active are the caller's OWN exports,
        // carried verbatim: this solve did not change the face, it solved it.
        return true;
    }

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
    ///     is NaN or negative. A negative, non-sentinel Delta would silently
    ///     cross lo_eff/up_eff (section 6's proof assumes Delta >= 0; the
    ///     assert guarding that is compiled out under NDEBUG). Also thrown
    ///     if overrides.primal_delta or overrides.dual_mu is NaN; or, at
    ///     either field's negative sentinel, if the corresponding stored
    ///     opts field is NaN or negative. The domain enforced throughout is
    ///     NOT NaN and >= 0 -- on the override directly for the NaN checks,
    ///     and on whichever of override/stored the sentinel rule selects for
    ///     the negative checks.
    static QpOptions resolve_effective_options(const QpOptions &opts,
                                               const SolveOverrides &overrides) {
        if (std::isnan(overrides.tr_radius) || overrides.tr_radius < 0.0) {
            throw std::invalid_argument(fmt::format(
                "QpEngine::solve: SolveOverrides.tr_radius must be >= 0, or the +inf sentinel "
                "for \"use opts_.tr_radius\" (qp_types.h) -- got {}",
                overrides.tr_radius));
        }
        if (!std::isfinite(overrides.tr_radius) &&
            (std::isnan(opts.tr_radius) || opts.tr_radius < 0.0)) {
            throw std::invalid_argument(fmt::format(
                "QpEngine::solve: QpOptions.tr_radius must be >= 0, or the +inf value that "
                "disables the trust region (qp_types.h) -- got {}",
                opts.tr_radius));
        }
        if (std::isnan(overrides.primal_delta)) {
            throw std::invalid_argument(
                "QpEngine::solve: SolveOverrides.primal_delta must not be NaN (negative is the "
                "documented sentinel for \"use opts_.primal_delta\" -- see qp_types.h)");
        }
        if (std::isnan(overrides.dual_mu)) {
            throw std::invalid_argument(
                "QpEngine::solve: SolveOverrides.dual_mu must not be NaN (negative is the "
                "documented sentinel for \"use opts_.dual_mu\" -- see qp_types.h)");
        }
        // The guard on each of these two is the EXACT NEGATION of the
        // corresponding resolution ternary's condition below, so "checked" and
        // "selected" cannot drift apart if a ternary is ever rewritten. (NaN
        // overrides are already rejected above, so `!(x >= 0.0)` is here just
        // the sentinel test `x < 0.0` spelled in the ternary's own terms.)
        if (!(overrides.primal_delta >= 0.0) &&
            (std::isnan(opts.primal_delta) || opts.primal_delta < 0.0)) {
            throw std::invalid_argument(fmt::format(
                "QpEngine::solve: QpOptions.primal_delta must be >= 0 (0 means no primal "
                "regularization; negative is SolveOverrides' sentinel, never a stored value "
                "-- qp_types.h) -- got {}",
                opts.primal_delta));
        }
        if (!(overrides.dual_mu >= 0.0) && (std::isnan(opts.dual_mu) || opts.dual_mu < 0.0)) {
            throw std::invalid_argument(fmt::format(
                "QpEngine::solve: QpOptions.dual_mu must be >= 0 (0 means no dual "
                "regularization; negative is SolveOverrides' sentinel, never a stored value "
                "-- qp_types.h) -- got {}",
                opts.dual_mu));
        }
        QpOptions eff = opts;
        eff.tr_radius = std::isfinite(overrides.tr_radius) ? overrides.tr_radius : opts.tr_radius;
        eff.primal_delta =
            overrides.primal_delta >= 0.0 ? overrides.primal_delta : opts.primal_delta;
        eff.dual_mu = overrides.dual_mu >= 0.0 ? overrides.dual_mu : opts.dual_mu;
        return eff;
    }

    // What the ratio test found blocking, or kNone.
    enum class BlockKind { kNone, kIneq, kBound };

    // What the drop rule last released. Section 4c's ride needs it to choose
    // the ride's SIGN, and `active` is what ARMS the curvature check: it is
    // true for exactly the one iteration that follows a drop, which is the
    // only iteration on which negative curvature can newly appear (section
    // 4b's Cauchy-interlacing note). `from` is meaningful for bound drops
    // only.
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
    // sites. WorkingSet stores active_ineq_ SORTED and bound_state_ by index,
    // so neither carries insertion order, and `ws` is mutated from six
    // different places (the seed's own primal, refresh_shifts' homotopy adds, the
    // temporary-vertex repair's pins, the ratio test's blocker, the ride's
    // blocker, the drop rule). refresh() is called once per major iteration
    // and stamps a monotone sequence number on every member that was not there
    // last time, which costs one O(n + mi) pass and cannot go stale when a
    // seventh mutation site appears. Entries that leave lose their number, so
    // a constraint dropped and later re-added is correctly the NEWEST member.
    // Members that appear together in one iteration are numbered in index
    // order, which is arbitrary but deterministic.
    //
    // A bound that merely switches SIDE (kAtLower <-> kAtUpper) keeps its
    // number: it never left the working set.
    //
    // THE EXEMPTION SETS are what bound the probe. A constraint whose
    // tentative drop was made real is exempt from probing for the rest of the
    // solve, so the ride's blocker re-adding that same constraint cannot lead
    // to the identical drop being probed and taken again. Each constraint
    // enters at most once, so the total number of probe-driven DROPS in a
    // solve is at most n + mi. (That is not a bound on the number of PROBES,
    // which is quadratic in the worst case -- see section 4b's COST note.)
    // Keyed by constraint identity, which for a bound means the VARIABLE and
    // not the side it was pinned at; see probe_zero_multiplier_drops for why
    // that conservative choice is the right one.
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
                   const std::shared_ptr<const HotState> &hot = nullptr) const {
        // ADOPT AN EXTERNAL HOT HANDLE. `hot` (WarmStart::hot, opaque -- see
        // this file's HotState) is a snapshot possibly produced by a
        // DIFFERENT QpEngine instance's hot_state(); see that method and the
        // OWNERSHIP note above HotState/BorderState for what sharing it does
        // and does not make safe. Adopted ONLY when this engine's OWN
        // instance-level cache is not already trustworthy (border_valid_
        // false): an engine with valid state of its own always prefers it,
        // since that state already describes what THIS instance has
        // actually been solving. Adoption makes this engine's border_/hash/
        // exit-state members look exactly like the handle's snapshot; the
        // pessimistic-invalidation snapshot immediately below reads them
        // right back out, so the reuse-eligibility check below (conditions
        // (a)-(e)) is the only thing that decides whether adopting it
        // actually skips a factorization on THIS call. ws_algebra ==
        // kRefactorize never adopts: the border-mode cache does not exist in
        // that mode, and hot_state() never emits a non-null handle from it.
        //
        // The identity pair (`hot->kkt_session_id`/`hot->kkt_epoch`) is also
        // copied into the committed members here, alongside the four
        // problem-shaped fields: it is what lets the reuse gate's condition
        // (e) tell whether the OBJECT `border_` now points at still carries
        // the numerics this identity describes, not merely whether the
        // PROBLEM still looks the same -- see HotState's OWNERSHIP note for
        // why the problem-shaped fields alone cannot make that distinction.
        if (!border_valid_ && hot != nullptr && hot->border != nullptr &&
            opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder) {
            border_ = hot->border;
            border_structural_hash_ = hot->structural_hash;
            border_values_hash_ = hot->values_hash;
            border_effective_delta_ = hot->effective_delta;
            border_effective_mu_ = hot->effective_mu;
            border_exit_bound_state_ = hot->exit_bound_state;
            border_exit_active_ineq_ = hot->exit_active_ineq;
            border_kkt_session_id_ = hot->kkt_session_id;
            border_kkt_epoch_ = hot->kkt_epoch;
            border_valid_ = true;
        }

        // HOT-START REUSE bookkeeping (border mode only -- see the header
        // contract). Snapshot whatever the previous solve() call left
        // behind, then pessimistically invalidate the persisted cache for
        // the ENTIRE duration of this call: if anything below throws --
        // qp.validate(), a Pardiso failure, anything -- border_valid_ is
        // already false, so a failed or thrown solve can never leave a
        // poisoned cache that a later solve() mistakes for reusable state.
        // It is re-armed only at the very bottom of this function, and only
        // on a clean kOptimal exit.
        const bool prev_border_valid = border_valid_;
        const std::uint64_t prev_border_structural_hash = border_structural_hash_;
        const std::uint64_t prev_border_values_hash = border_values_hash_;
        const double prev_border_effective_delta = border_effective_delta_;
        const double prev_border_effective_mu = border_effective_mu_;
        // This engine's own last-trusted (session_id, epoch) pair for
        // whatever object
        // `border_` currently names (possibly just adopted above).
        // Compared, at the reuse gate below, against that object's OWN live
        // identity -- not against each other's frozen copies -- so a
        // mutation any OTHER holder made to the shared object in the
        // meantime is detected even though every problem-shaped fingerprint
        // above still matches.
        const std::uint64_t prev_border_kkt_session_id = border_kkt_session_id_;
        const std::uint64_t prev_border_kkt_epoch = border_kkt_epoch_;
        border_valid_ = false;

        // Resolve this solve's SolveOverrides against opts_ ONCE, into the
        // effective values every opts_.tr_radius/primal_delta/dual_mu read
        // site below actually consults from here on (see qp_types.h's
        // SolveOverrides and the header contract's PER-SOLVE RADIUS
        // VARIATION note). `eff_opts` is opts_ verbatim except for these
        // three fields, so passing it anywhere opts_ used to be passed as a
        // whole QpOptions is safe: every other field (feas_tol, opt_tol,
        // max_iter, schur_cap, schur_cond_max, ws_algebra) is untouched.
        //
        // VALIDATE FIRST. tr_radius must be the +inf sentinel or >= 0: a
        // negative, non-sentinel Delta silently crosses lo_eff/up_eff --
        // section 6's CROSSED EFFECTIVE BOUNDS CANNOT HAPPEN proof assumes
        // Delta >= 0, an assumption that was closed while tr_radius lived
        // only on the per-instance-const opts_ but is now a LIVE hazard,
        // since Delta is per-solve driver input (a shrink loop can pass one
        // straight through from its own arithmetic). The `assert`
        // guarding that proof below is compiled OUT under NDEBUG, so without
        // this check a negative override degrades silently into a crossed
        // effective box in a Release build rather than failing loudly:
        // probed at tr_radius = -0.5 on a [-2,2]^2 box, which returns
        // kOptimal at (-0.5, 0) with no diagnostic at all in Release (Debug's
        // assert does catch it). primal_delta/dual_mu keep their documented
        // negative-means-sentinel convention (qp_types.h) unchanged -- only NaN
        // is rejected for them, since NaN is not a value either field's
        // resolution ternary (or any downstream arithmetic) can absorb. The
        // SAME domain is enforced on opts_.tr_radius whenever the +inf sentinel
        // selects it -- which includes every call through the PLAIN solve()
        // overloads -- since the resolved value is what section 6 below reads
        // either way (see resolve_effective_options). Shared by run() and
        // refine_on_face() so both resolve per-solve overrides through the
        // SAME code rather than copies that could drift.
        QpOptions eff_opts = resolve_effective_options(opts_, overrides);

        qp_in.validate();
        const Index n = qp_in.n();
        const Index me = qp_in.me();
        const Index mi = qp_in.mi();

        // Step 0: an empty box (lower(i) > upper(i) beyond feas_tol) is
        // detected here rather than in QpProblem::validate() -- see the
        // header contract's CROSSED BOUNDS note for why. Checked before the
        // working set / start point are even built; `crossed_bounds` gates
        // the main loop below, and the shared finalization tail (which
        // already zeroes the multipliers on any kInfeasible exit) handles
        // the rest. Always against the caller's REAL bounds: a trust region
        // cannot manufacture a crossed bound that was not already there (see
        // section 6's CROSSED EFFECTIVE BOUNDS CANNOT HAPPEN note), and
        // start_center()'s own clamp needs a real box to clamp into regardless
        // of tr_radius.
        bool crossed_bounds = false;
        for (Index i = 0; i < n; ++i) {
            if (qp_in.lower(i) > qp_in.upper(i) + eff_opts.feas_tol) {
                crossed_bounds = true;
                break;
            }
        }

        WorkingSet ws(n, mi);
        // Step 1a ONLY: the clamped seed primal. The seed's WORKING SET is
        // ingested further down, after section 6 has computed the window --
        // see start_center/ingest_seed_working_set and section 6's
        // WINDOW-CONSISTENCY RULE.
        Vec x = start_center(qp_in, seed, ws);

        // --- Section 6: trust-region effective bounds (see header contract) ---
        //
        // tr_active is materialized unconditionally (QpSolution's contract:
        // size n, always) but everything ELSE here -- the two Vecs and the
        // QpProblem copy -- is skipped entirely when tr_radius is +inf, which
        // is what makes that path bit-identical to a trust-region-free
        // solve: `qp` below aliases `qp_in` directly, with nothing new
        // allocated.
        std::vector<bool> tr_active(static_cast<std::size_t>(n), false);
        QpProblem tr_problem;
        const bool tr_enabled = std::isfinite(eff_opts.tr_radius) && !crossed_bounds;
        if (tr_enabled) {
            // TODO(perf): this deep-copies H/Ae/Ai/g/be/bi every solve purely
            // to get a place to put lo_eff/up_eff -- none of those other
            // fields ever differ from qp_in's. Cheaper fix directions if
            // this ever shows up in a profile: thread lo_eff/up_eff as
            // separate parameters into the handful of functions that read
            // bounds instead of shadowing a whole QpProblem, or give the
            // engine a persistent scratch QpProblem (an instance member,
            // resized/refilled in place rather than reassigned) that only
            // the bounds are ever written into.
            tr_problem = qp_in;
            Vec lo_eff(n);
            Vec up_eff(n);
            for (Index i = 0; i < n; ++i) {
                lo_eff(i) = std::max(qp_in.lower(i), x(i) - eff_opts.tr_radius);
                up_eff(i) = std::min(qp_in.upper(i), x(i) + eff_opts.tr_radius);
                // start_center() clamped x(i) into [qp_in.lower(i),
                // qp_in.upper(i)] just above, so lo_eff(i) <= x(i) <=
                // up_eff(i) always -- crossed effective bounds cannot happen
                // by construction (see section 6).
                assert(lo_eff(i) <= up_eff(i));
                if (lo_eff(i) == up_eff(i) &&
                    ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFixed) {
                    // Delta == 0 (or a start point already pinned exactly
                    // Delta from one side with the other just as tight):
                    // pins x(i) here as a temporary vertex of one, exactly
                    // as a genuinely fixed real bound would, and TR-caused,
                    // so tr_active is set. A variable already kFixed from a
                    // real lower(i) == upper(i) is left untouched above.
                    ws.bound_state()[static_cast<std::size_t>(i)] = BoundState::kFixed;
                    x(i) = lo_eff(i);
                    tr_active[static_cast<std::size_t>(i)] = true;
                }
            }
            tr_problem.lower = std::move(lo_eff);
            tr_problem.upper = std::move(up_eff);
        }
        // Shadows qp_in for the rest of this function. Every qp.lower(i)/
        // qp.upper(i) read below -- directly here, or indirectly through
        // eqp_candidate into kkt_assembly.h/eqp_solve.h/bordered_eqp.h --
        // sees the effective bounds, which is the entire implementation of
        // "TR bounds participate in the ratio test and pins exactly like
        // real bounds" (see section 6).
        const QpProblem &qp = tr_enabled ? tr_problem : qp_in;

        // Step 1b: NOW ingest the seed's working set, against the effective
        // bounds just computed. Deliberately after section 6, so a seeded pin
        // can never move the center the window was built around; a pin that
        // does not fit inside that window is dropped (section 6's
        // WINDOW-CONSISTENCY RULE). With tr_enabled == false, `qp` IS qp_in
        // and every real bound trivially lies inside its own box, so this is
        // bit-identical to ingesting during step 1.
        ingest_seed_working_set(qp_in, qp, seed, ws, x);

        // Row magnitudes of Ai/Ae, used to scale the feasibility tolerances.
        Vec ai_row_norm1 = Vec::Zero(mi);
        for (Index j = 0; j < mi; ++j) {
            ai_row_norm1(j) = qp.Ai.row(j).cwiseAbs().sum();
        }
        Vec ae_row_norm1 = Vec::Zero(me);
        for (Index j = 0; j < me; ++j) {
            ae_row_norm1(j) = qp.Ae.row(j).cwiseAbs().sum();
        }

        Vec lambda_e = Vec::Zero(me);
        Vec lambda_i = Vec::Zero(mi);
        Vec z = Vec::Zero(n);

        Vec shift = Vec::Zero(mi);
        Vec Aix = Vec::Zero(mi);

        // `counters` is declared BEFORE the seeding refresh_shifts() below
        // for one reason: that call is itself a source of
        // counters.shift_adds -- the
        // seed's own homotopy admission -- and QpCounters documents that the
        // pre-loop call is included. Nothing else reads it this early.
        detail::KktFactor kkt;
        QpCounters counters;
        WalkSeen seen{std::vector<std::uint8_t>(static_cast<std::size_t>(mi), 0),
                      std::vector<std::uint8_t>(static_cast<std::size_t>(n), 0)};

        refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);

        // Decide whether border_ (the engine-instance persistent K0/border
        // state) may be trusted for THIS solve, per the header contract's
        // HOT-START REUSE conditions (a)/(b)/(c)/(d) PLUS (e) below. `ws`
        // here is exactly the seed working set condition (b) is stated
        // over: start_center() plus ingest_seed_working_set() plus
        // the refresh_shifts() call just above -- so a hint the
        // WINDOW-CONSISTENCY RULE dropped is absent here too, and reuse
        // eligibility is judged on the working set this solve will actually
        // start from, before the loop below runs its first iteration. When
        // any condition fails, border_ is DETACHED onto a brand-new
        // BorderState of this engine's own -- as if this were the engine's
        // first solve ever -- rather than the previous object being patched
        // in place, so the ordinary rebuild/sync machinery below runs
        // unmodified AND so a refused adoption never mutates an object some
        // other holder may still be relying on; see the
        // detach comment just below for why patch-in-place was the bug.
        //
        // current_structural_hash/current_values_hash are computed at most
        // ONCE per solve() call and reused verbatim by the exit-commit block
        // near the end of this function -- each hash walks all of H/Ae/Ai,
        // so recomputing them there would silently double the sparse-matrix
        // traffic this feature exists to avoid.
        std::uint64_t current_structural_hash = 0;
        std::uint64_t current_values_hash = 0;
        if (opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder) {
            current_structural_hash = detail::structural_hash(qp);
            current_values_hash = detail::values_hash(qp);
            // Condition (d): the EFFECTIVE (primal_delta, dual_mu) pair this
            // solve resolved to must match what the previous trustworthy
            // solve resolved to -- see the header contract's HOT-START REUSE
            // note.
            //
            // Condition (e), the factor-identity conjunct: `border_`'s
            // factor's own live (session_id, epoch) must equal what THIS
            // engine last saw as trustworthy for whatever object `border_`
            // currently names -- conditions (a)-(d) alone describe the
            // PROBLEM, never WHICH OBJECT `border_` names, and only a live
            // read of the object's own identity on every solve() call (not
            // only at adoption) catches a different engine having mutated a
            // shared object since. See HotState's OWNERSHIP note for the
            // full adjudication of what this conjunct does and does not
            // make safe on its own.
            //
            // The USABLE-NUMERICS conjunct: an epoch does not advance on a
            // FAILED factorize, so a stale (session_id, epoch) pair can
            // still MATCH an object whose numerics are invalidated.
            // `inertia().state == kObserved` closes that gap (false
            // precisely when the last factorize failed or none happened).
            const bool reuse_eligible =
                prev_border_valid && current_structural_hash == prev_border_structural_hash &&
                current_values_hash == prev_border_values_hash &&
                eff_opts.primal_delta == prev_border_effective_delta &&
                eff_opts.dual_mu == prev_border_effective_mu &&
                ws.bound_state() == border_exit_bound_state_ &&
                ws.active_ineq() == border_exit_active_ineq_ &&
                border_->kkt.factor.session_id() == prev_border_kkt_session_id &&
                border_->kkt.factor.epoch() == prev_border_kkt_epoch &&
                border_->kkt.factor.inertia().state ==
                    hven::linear::InertiaEvidence::State::kObserved;
            // The direct "did the gate pass" signal -- see
            // QpCounters::k0_reused's own note for why a caller must read THIS
            // rather than infer reuse from `factorizations == 0`.
            counters.k0_reused = reuse_eligible;
            if (!reuse_eligible) {
                // DETACH rather than wipe in place: `border_` may be the
                // SAME shared object another engine still holds (via its own
                // copy of a HotState); clearing schur/latched/k0_rows/ledger
                // on it still leaves `k0`/`kkt` stale until the next
                // rebuild_k0() call, which then refactorizes THAT SHARED
                // OBJECT for THIS engine's problem, silently destroying
                // whatever the other holder was relying on.
                //
                // DETACH ONLY WHEN ACTUALLY SHARED: unconditional detach is
                // not free even in the never-shared case, since a fresh
                // BorderState pays a full symbolic re-analysis on every
                // value-changing major (factorize_checked() only skips it on
                // the SAME KktFactor object), material at scale where the
                // symbolic analysis is the whole reason the pattern cache
                // exists.
                //
                // `border_.use_count()` is exactly the discriminator the
                // correctness argument needs (see HotState's OWNERSHIP
                // note): a shared object may only ever be safely mutated by
                // an engine whose OWN gate just passed for it (never true in
                // this branch), so `use_count() > 1` means some OTHER holder
                // relies on this object unchanged and it must be detached;
                // `use_count() == 1` means this engine is the ONLY owner, so
                // the SAME object's border-stack fields are wiped in place
                // instead, letting the next rebuild_k0() reuse this
                // KktFactor's still-live symbolic analysis when the sparsity
                // pattern has not changed. `use_count()` is reliable here
                // only under this file's documented sequential-use contract
                // (THREAD SAFETY, above).
                if (border_.use_count() > 1) {
                    border_ = std::make_shared<BorderState>();
                } else {
                    border_->schur.reset();
                    border_->latched = false;
                    border_->k0_rows.clear();
                    border_->ledger.clear();
                }
            }
        }

        // Section 4c: the curvature threshold, computed once (hessian_scale
        // walks H's stored nonzeros) and the record of what the previous
        // iteration's drop rule released, which arms the check.
        const double curvature_tol = detail::kCurvatureTolFactor * detail::hessian_scale(qp);
        DropRecord last_drop;

        // Section 4b's zero-multiplier probe: working-set addition order, and
        // the per-solve exemption set that bounds it. Both are per-SOLVE, not
        // per-engine -- a warm re-solve starts with a clean exemption set.
        ProbeState probe(n, mi);

        // Section 4b's POST-PROBE RESTART, whose whole content is these two
        // bits: `probe_drop_made` ARMS it (only a probe-driven drop can leave
        // the loop in the dead end it answers) and `post_probe_restart_spent`
        // CONSUMES it, once per solve. Per-solve for the same reason the
        // exemption set is: a warm re-solve is a new caller decision and gets
        // its own budget.
        bool probe_drop_made = false;
        bool post_probe_restart_spent = false;

        // Observation only (see QpCounters::degenerate_run_max): the length of
        // the degenerate-step run in progress. A run is broken by any minor on
        // which THE ITERATE MOVED, which is the question the counter is asked
        // -- not by any minor that merely differs in kind. Concretely it is
        // reset in exactly three places, and the omissions are deliberate:
        //   - an ordinary step that was not degenerate (the else below), which
        //     covers both a full step to the EQP point and a blocked step that
        //     moved more than step_tol;
        //   - a taken RIDE, which steps along a negative-curvature direction;
        //   - a START REPAIR, which pins variables onto bounds and so moves x.
        // NOT reset by a drop iteration or a zero-multiplier probe: both snap x
        // onto an EQP point already within step_tol of where it was, so the
        // iterate did not move and a run that spans them is still one stall.
        Index degenerate_run = 0;

        // The cap this solve runs at: an explicitly set
        // QpOptions::max_iter, or -- at its default sentinel -- one derived
        // from the subproblem's own size. See detail::effective_qp_max_iter
        // for the derivation, the calibration and the precedence rule.
        // Resolved off `qp_in` (the caller's problem, real bounds) rather
        // than `qp` (the trust-region-narrowed one), deliberately.
        const Index eff_max_iter = detail::effective_qp_max_iter(qp_in, opts_.max_iter);

        QpStatus status = crossed_bounds ? QpStatus::kInfeasible : QpStatus::kMaxIter;
        for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {
            ++counters.minor_iters;

            // Stamp addition numbers on whatever joined the working set since
            // the last iteration, BEFORE anything this iteration changes it,
            // so the record reflects the order the loop actually built the
            // working set in (section 4b's zero-multiplier probe reads it).
            probe.refresh(ws);

            detail::InertiaVerdict verdict = detail::InertiaVerdict::kOk;
            const EqpResult eqp = eqp_candidate(qp, ws, kkt, *border_, counters, verdict, eff_opts);

            // INERTIA GATE / TEMPORARY-VERTEX START REPAIR (section 4b). Only
            // at the start: a trustworthy wrong inertia here means the seed
            // working set is second-order inconsistent (an indefinite H with
            // too few active constraints), and the EQP result just computed
            // is a saddle/maximizer rather than a minimizer, so it is
            // discarded and the iteration is retried from the repaired
            // vertex. A kSuspect verdict is deliberately NOT a repair
            // trigger, and a wrong inertia LATER in the
            // run is a mid-solve negative-curvature event, which section
            // 4c's ride handles.
            if (iter == 0 && verdict == detail::InertiaVerdict::kWrong &&
                repair_temporary_vertex(qp, ws, x, kkt, *border_, counters, eff_opts)) {
                degenerate_run = 0; // observation only -- x moved onto bounds
                // Pinning moved x onto bounds, which can violate general
                // rows; the homotopy picks them up exactly as at a start
                // point.
                refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen,
                               eff_opts);
                continue;
            }

            price(qp, ws, eqp, lambda_e, lambda_i, z);

            const Vec p = eqp.x - x;

            // Whatever the previous iteration's drop rule released. Consumed
            // here whether or not the ride below fires, so it can never arm an
            // iteration that did not follow a drop.
            const DropRecord dropped = last_drop;
            last_drop = DropRecord{};

            const double step_tol = eff_opts.feas_tol * std::max(1.0, x.lpNorm<Eigen::Infinity>());
            if (p.lpNorm<Eigen::Infinity>() <= step_tol) {
                // A KKT point of the current working set: snap onto it, then
                // let the multipliers decide.
                x = eqp.x;
                refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen,
                               eff_opts);
                if (!drop_worst(qp, shift, lambda_i, z, ws, last_drop, counters)) {
                    // Nothing left to leave the working set, so this point is
                    // final. Classify it: any constraint row (either block)
                    // carrying a STRUCTURAL violation means no reachable point
                    // satisfies the constraints; failing that, a free
                    // component that ran away along an unbounded direction is
                    // regularization noise rather than an optimum; failing
                    // that, a TRUSTED wrong inertia means the point is a KKT
                    // point of a system whose reduced Hessian is not positive
                    // semidefinite -- a saddle or a maximizer, not an optimum
                    // (see section 4b's SECOND-ORDER CERTIFICATION note).
                    //
                    // ORDERING CAVEAT ON `verdict`, pre-existing and unchanged
                    // by the restart below: it was computed by eqp_candidate at
                    // the TOP of this iteration, i.e. against the working set
                    // BEFORE the refresh_shifts above -- which can ADD rows the
                    // homotopy is now driving. So a kWrong read here describes
                    // the labeling the EQP was solved on, not necessarily the
                    // one being classified. That is conservative in the
                    // direction that matters (adding rows only shrinks the null
                    // space, so a kOk read cannot become kWrong), and the probe
                    // below re-runs the gate itself on the CURRENT set, but a
                    // kWrong read can be one iteration stale. Noted here
                    // because the restart now acts on it.
                    if (worst_structural_violation(qp, x, Aix, ai_row_norm1, lambda_i, ae_row_norm1,
                                                   lambda_e, eff_opts) > 0.0) {
                        status = QpStatus::kInfeasible;
                    } else if (is_runaway(qp, x, ws, eff_opts)) {
                        status = QpStatus::kNumericalError;
                    } else if (verdict == detail::InertiaVerdict::kWrong) {
                        // THE POST-PROBE RESTART (section 4b). Reaching here
                        // after a probe-driven drop means the drop exposed
                        // real negative curvature but its multiplier was
                        // zero, so section 4c's arming direction p is
                        // identically zero and the ride is never offered
                        // anything. The temporary-vertex repair (normally
                        // gated on iter == 0) is spent ONE time here instead,
                        // after every cheaper way out (structural
                        // infeasibility, runaway, the drop rule) has been
                        // exhausted. If it fails, or the budget is gone, the
                        // certification branch's original verdict stands.
                        //
                        // `probe_drop_made` is scoping, not a measured
                        // necessity: every OTHER shipped fixture reaching
                        // this branch does so with an UNPINNABLE free
                        // variable, where the repair declines anyway. It
                        // gates the restart to the case section 4c is
                        // structurally unable to serve.
                        //
                        // STICKY PER SOLVE, NOT PER DROP: once a probe drop
                        // has been made anywhere in this solve, a LATER dead
                        // end from a drop_worst drop whose ride declined can
                        // also spend the unspent restart -- wider than the
                        // narrowest intent, but sound either way, since an
                        // unspent repair either reaches a second-order-
                        // consistent working set or unwinds itself (worst
                        // case: one wasted factorization).
                        if (probe_drop_made && !post_probe_restart_spent &&
                            repair_temporary_vertex(qp, ws, x, kkt, *border_, counters, eff_opts)) {
                            post_probe_restart_spent = true;
                            // Pinning moved x onto bounds, exactly as at the
                            // start-of-solve repair: general rows may now be
                            // violated and the homotopy picks them up.
                            refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters,
                                           seen, eff_opts);
                            continue;
                        }
                        status = QpStatus::kNumericalError;
                    } else {
                        // Everything above passed, so this is a would-be
                        // kOptimal exit -- the last place a WEAKLY ACTIVE
                        // constraint can still be hiding the critical cone
                        // from the gate. Probe the zero-multiplier members of
                        // the working set before certifying (section 4b's
                        // ZERO-MULTIPLIER PROBE). refresh_shifts above may have
                        // just added rows, so the addition record is brought up
                        // to date first.
                        probe.refresh(ws);
                        if (probe_zero_multiplier_drops(qp, shift, lambda_i, z, ws, kkt, *border_,
                                                        counters, probe, last_drop, eff_opts)) {
                            // Negative curvature the full labeling hid: the
                            // drop is real, and `last_drop` arms section 4c's
                            // ride to take the step on the next iteration.
                            // It also arms the POST-PROBE RESTART above, which
                            // is what actually answers this case -- 4c's ride
                            // is vacuous for a zero-multiplier drop.
                            probe_drop_made = true;
                            continue;
                        }
                        // SECTION 4b's SUSPECT-STALL GATE. `verdict` describes
                        // the system this iterate was actually solved from
                        // (eqp_candidate, top of the iteration), so a kSuspect
                        // here means the numbers that produced this point came
                        // out of a factorization with a fabricated or dropped
                        // pivot. Certify only after checking the QP model's own
                        // first-order condition on the free block; a failure
                        // means the rebuild loop stalled and the answer is
                        // fiction.
                        double stat_scale = 1.0;
                        const double free_stat =
                            verdict == detail::InertiaVerdict::kSuspect
                                ? detail::free_block_stationarity(qp, x, ws, lambda_e, lambda_i,
                                                                  stat_scale)
                                : 0.0;
                        // Asks "is it stationary" and NEGATES, rather than
                        // asking "is it a violation": see
                        // free_block_is_stationary for why the two are not
                        // interchangeable when the residual is NaN.
                        if (!detail::free_block_is_stationary(free_stat, stat_scale, eff_opts)) {
                            if (counters.suspect_escalations < detail::kMaxSuspectEscalations) {
                                // Half (i): change the system rather than
                                // re-solve it. Discarding the border state is
                                // what makes the new primal_delta reach K0 --
                                // rebuild_k0 rebuilds from eff_opts, and this
                                // is the SAME use_count()-guarded
                                // DETACH-or-wipe the hot-start ineligibility
                                // branch above uses: `border_` may
                                // still be the shared object a HotState
                                // handle handed to another engine names, in
                                // which case a fresh BorderState is allocated
                                // here too rather than the old one being
                                // wiped and rebuilt in place -- see that
                                // branch's own comment for the full argument,
                                // including why the sole-owner case wipes in
                                // place instead to keep this KktFactor's
                                // backend symbolic analysis alive. A
                                // detached object starts with a fresh
                                // factor identity, same as there; a
                                // wiped-in-place object keeps its session
                                // and epoch sequence.
                                //
                                // THE RESET IS NOT DISTINGUISHED BY ANY SHIPPED
                                // FIXTURE, and that is recorded rather than
                                // implied: deleting it leaves the whole suite
                                // green (measured), because on every
                                // fixture that reaches here the suspect signal
                                // is a perturbed pivot, which border_candidate
                                // ALREADY rebuilds on. It is kept because
                                // kSuspect has two other sources -- counts
                                // that do not sum to n, and a border stack
                                // past needs_refactorization -- and on neither
                                // of those does that rebuild trigger fire.
                                // Without it the escalated delta would never
                                // reach K0 there and the ladder would burn all
                                // three rungs re-solving an unchanged system,
                                // which is precisely the no-progress loop this
                                // exists to escape.
                                ++counters.suspect_escalations;
                                eff_opts.primal_delta *= detail::kSuspectDeltaFactor;
                                if (border_.use_count() > 1) {
                                    border_ = std::make_shared<BorderState>();
                                } else {
                                    border_->schur.reset();
                                    border_->latched = false;
                                    border_->k0_rows.clear();
                                    border_->ledger.clear();
                                }
                                continue;
                            }
                            // Ladder exhausted. Same verdict and semantics as
                            // the runaway/saddle exits: report the iterate,
                            // clear the multipliers, and let
                            // counters.suspect_escalations carry the reason.
                            status = QpStatus::kNumericalError;
                        } else {
                            status = QpStatus::kOptimal;
                        }
                    }
                    break;
                }
                continue;
            }

            // NEGATIVE-CURVATURE RIDE (section 4c). Armed only on the
            // iteration that follows a drop -- the only place negative
            // curvature can newly appear -- and only for a step the loop
            // considers real: on a NEGLIGIBLE p (handled above) the direction
            // is rounding noise, its Rayleigh quotient means nothing, and the
            // point is a KKT point of the post-drop working set that section
            // 4b's certification branch is the right judge of.
            if (dropped.active) {
                const double curvature =
                    p.dot(qp.H.selfadjointView<Eigen::Upper>() * p) / p.squaredNorm();
                if (curvature <= curvature_tol) {
                    // The EQP just solved is unbounded below along +/-p within
                    // this working set, so its "solution" is a saddle and the
                    // step to it means nothing. Ride the direction instead --
                    // unless the ride declines it, in which case fall through
                    // to the ordinary step this engine took before the ride
                    // existed. p.squaredNorm() is nonzero here: the negligible
                    // case returned above.
                    const RideOutcome outcome =
                        ride_negative_curvature(qp, dropped, counters, seen, p, Aix, ws, x);
                    if (outcome == RideOutcome::kUnbounded) {
                        // Nothing blocks: genuinely unbounded. Same verdict and
                        // same semantics (final iterate reported, multipliers
                        // cleared below) as section 4b's certification branch.
                        status = QpStatus::kNumericalError;
                        break;
                    }
                    if (outcome == RideOutcome::kStepped) {
                        degenerate_run = 0; // observation only -- the ride moved x
                        refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters,
                                       seen, eff_opts);
                        continue;
                    }
                }
            }

            BlockKind kind = BlockKind::kNone;
            Index block_idx = -1;
            BoundState block_state = BoundState::kFree;
            const double alpha = ratio_test(qp, ws, x, Aix, p, kind, block_idx, block_state,
                                            /*cap_at_unit=*/true, counters);

            // Observation only (see QpCounters' own note): a blocking add
            // whose step moved the iterate by no more than the SAME step_tol
            // the KKT-point test above used is a degenerate pivot. Read
            // before x is overwritten, since ||p||inf is the pre-step
            // direction's magnitude.
            if (kind != BlockKind::kNone && alpha * p.lpNorm<Eigen::Infinity>() <= step_tol) {
                ++counters.degenerate_steps;
                ++degenerate_run;
                counters.degenerate_run_max = std::max(counters.degenerate_run_max, degenerate_run);
            } else {
                degenerate_run = 0;
            }
            if (kind != BlockKind::kNone) {
                ++counters.ws_adds;
                if (kind == BlockKind::kBound) {
                    ++counters.ws_adds_bound;
                    mark_bound_seen(seen, counters, block_idx);
                } else {
                    mark_ineq_seen(seen, counters, block_idx);
                }
            }

            x = (alpha >= 1.0) ? eqp.x : Vec(x + alpha * p);
            if (kind == BlockKind::kIneq) {
                ws.add_ineq(block_idx);
            } else if (kind == BlockKind::kBound) {
                ws.bound_state()[static_cast<std::size_t>(block_idx)] = block_state;
                x(block_idx) = (block_state == BoundState::kAtUpper) ? qp.upper(block_idx)
                                                                     : qp.lower(block_idx);
            }
            refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);
        }

        // Commit the hot-start reuse cache iff this solve reached a genuine
        // KKT point -- see the header contract's INVALIDATION POLICY.
        // kMaxIter/kInfeasible/kNumericalError all leave border_valid_ false
        // (it was cleared at the top of this function and is armed only
        // here): border_ may still hold a perfectly good K0 factorization,
        // but the working set that produced it was never certified, so the
        // NEXT solve() call must not trust it either. Reuses
        // current_structural_hash/current_values_hash computed above rather
        // than re-hashing H/Ae/Ai a second time.
        if (opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder &&
            status == QpStatus::kOptimal) {
            border_structural_hash_ = current_structural_hash;
            border_values_hash_ = current_values_hash;
            // Condition (d): commit the EFFECTIVE pair this solve resolved
            // to, not opts_'s own primal_delta/dual_mu -- see the header
            // contract's HOT-START REUSE note and this function's top.
            border_effective_delta_ = eff_opts.primal_delta;
            border_effective_mu_ = eff_opts.dual_mu;
            border_exit_bound_state_ = ws.bound_state();
            border_exit_active_ineq_ = ws.active_ineq();
            // Condition (e): commit `border_`'s factor's OWN live
            // (session_id, epoch) identity as
            // of THIS solve's exit -- whatever it ended at, whether that is
            // unchanged (reuse was granted, nothing rebuilt) or freshly
            // advanced (a rebuild_k0 ran during this solve, on this object
            // or a freshly detached one). The NEXT solve's reuse gate
            // compares this snapshot against a live re-read of the same
            // identity, which is what notices if some OTHER holder has
            // since rebuilt the object out from under it.
            border_kkt_session_id_ = border_->kkt.factor.session_id();
            border_kkt_epoch_ = border_->kkt.factor.epoch();
            border_valid_ = true;
        }

        // On the infeasible and runaway paths the multipliers are pure
        // regularization artifacts of size ~1/dual_mu (1e8 at the defaults)
        // that mean nothing. Report the final iterate -- the caller's evidence
        // of what could not be satisfied -- but do not let those numbers leak
        // out as if they were prices.
        if (status == QpStatus::kInfeasible || status == QpStatus::kNumericalError) {
            lambda_e.setZero();
            lambda_i.setZero();
            z.setZero();
        }

        // THE EXPORT INVARIANT: A FREE VARIABLE CARRIES NO BOUND PRICE.
        // `price()` is the sole producer of `z` and already imposes this
        // rule (z(i) written only where bound_state[i] != kFree), but it
        // runs at the TOP of a minor iteration while the working set can
        // still be mutated afterward (drop_worst, the zero-multiplier
        // probe, the ratio test). On a converging exit the last mutation is
        // always followed by another `price()`, so the pair stays
        // consistent; on a `kMaxIter` exit it is not -- the export can carry
        // a price computed against a working set that no longer exists.
        // This loop re-imposes `price()`'s own rule at the point of export
        // rather than re-labelling `bound_state` to match `z`:
        // `bound_state` is read far more widely (the hot-start reuse
        // ledger, the driver's activity export, WarmStart ingest), and
        // clearing z is the direction that ADDS no information -- it cannot
        // invent activity.
        //
        // UNCONDITIONAL rather than `if (status == kMaxIter)`: the invariant
        // is a property of the EXPORT, not of a status. On the other three
        // statuses it finds nothing to do -- kInfeasible/kNumericalError
        // zero `z` outright two lines above, and between the last `price()`
        // call and the ONLY assignment of kOptimal, the sole writes to
        // ws.bound_state() are refresh_shifts (adds rows, writes no bound
        // state) and probe_zero_multiplier_drops, which on the `false`
        // return that is the only route to kOptimal has already restored
        // every bound it tentatively released.
        //
        // Runs BEFORE section 6's TR exclusion below, which only ever
        // clears MORE of `z`, so the two compose in either order.
        for (Index i = 0; i < n; ++i) {
            if (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree) {
                z(i) = 0.0;
            }
        }

        // Section 6's reporting exclusions, against the FINAL working set
        // only (see the header contract): finalize tr_active for whichever
        // side (kAtLower/kAtUpper) a bound ended up pinned at -- the
        // zero-radius kFixed flip above already set tr_active for its own
        // indices, and needs no revisiting -- then report bound_state kFree
        // and z == 0 for every TR-pinned index. border_exit_bound_state_
        // just above deliberately reads ws.bound_state() BEFORE this
        // rewrite: the hot-start reuse ledger must see the real, internal
        // pin state, not this caller-facing view.
        std::vector<BoundState> reported_bound_state = ws.bound_state();
        if (tr_enabled) {
            for (Index i = 0; i < n; ++i) {
                const auto si = static_cast<std::size_t>(i);
                const BoundState st = reported_bound_state[si];
                if (st == BoundState::kAtLower) {
                    tr_active[si] = tr_problem.lower(i) > qp_in.lower(i);
                } else if (st == BoundState::kAtUpper) {
                    tr_active[si] = tr_problem.upper(i) < qp_in.upper(i);
                }
                if (tr_active[si]) {
                    reported_bound_state[si] = BoundState::kFree;
                    z(si) = 0.0;
                }
            }
        }

        QpSolution out;
        out.status = status;
        out.x = std::move(x);
        out.lambda_e = std::move(lambda_e);
        out.lambda_i = std::move(lambda_i);
        out.z = std::move(z);
        out.bound_state = std::move(reported_bound_state);
        out.tr_active = std::move(tr_active);
        out.ineq_active.assign(static_cast<std::size_t>(mi), false);
        for (Index row : ws.active_ineq()) {
            out.ineq_active[static_cast<std::size_t>(row)] = true;
        }
        out.counters = counters;

        // Emit a record to the ledger if attached.
        if (ledger_ != nullptr) {
            SolveRecord rec;
            rec.label = label_prefix_ + "_" + std::to_string(solve_counter_);
            rec.warm = warm;
            rec.status = status;
            rec.counters = counters;
            ledger_->record(rec);
            ++solve_counter_;
        }

        return out;
    }

    // Step 1a: the START CENTER -- the clamped seed primal, plus kFixed
    // detection. NO seeded bound-state hint is materialized here; that is
    // step 1b (ingest_seed_working_set), and the SPLIT IS LOAD-BEARING.
    //
    // WHY THE SPLIT EXISTS. Section 6 computes the
    // trust-region window about this x. When the two steps were one function,
    // a seeded kAtLower/kAtUpper hint overwrote x with its BOUND before the
    // window was computed, so the window came out centered on that bound
    // rather than on the point the caller meant -- and the returned step then
    // violated the radius by up to the distance to that bound. Measured on
    // H = I, g = (-100,-100) over [0,6]^2 seeded kAtUpper with x zeroed: the
    // solve returned (6, 6) at every one of Delta = 5, 2.5, 1.25. A caller
    // cannot defend against this by zeroing seed.x, because the zero is
    // exactly what the hint overwrites.
    static Vec start_center(const QpProblem &qp, const QpSolution *seed, WorkingSet &ws) {
        const Index n = qp.n();
        const bool seed_x = seed != nullptr && seed->x.size() == n;

        Vec x(n);
        for (Index i = 0; i < n; ++i) {
            const double lo = qp.lower(i);
            const double up = qp.upper(i);
            x(i) = std::min(std::max(seed_x ? seed->x(i) : 0.0, lo), up);
            if (lo == up) {
                ws.bound_state()[static_cast<std::size_t>(i)] = BoundState::kFixed;
                x(i) = lo;
            }
        }
        return x;
    }

    // Step 1b: ingest the seed's WORKING SET -- bound-state hints and active
    // inequality rows -- against the EFFECTIVE bounds, i.e. after section 6
    // has computed the trust-region window about start_center's x.
    //
    // THE WINDOW-CONSISTENCY RULE. A seeded bound hint is honoured only if
    // the bound it names lies INSIDE this solve's effective window
    // [qp_eff.lower(i), qp_eff.upper(i)]. A hint outside the window is
    // DROPPED: index i simply arrives kFree and the loop re-derives its
    // activity from the effective bounds like any other variable, which costs
    // at most the working-set steps needed to rediscover a pin that is
    // reachable. Honouring it instead is not an option -- it would place x
    // outside the very window this solve was asked to respect, which is the
    // defect described above.
    //
    // A hint INSIDE the window is honoured EXACTLY as before, and that is the
    // common case a warm chain depends on: when the seed's own x is carried
    // (not zeroed), the window is centered on that x, so a bound the previous
    // solve was sitting at is at distance 0 from the center and is always
    // inside. ShrinkRadiusRetryWithALiveRealBoundPinChainsAndReuses is the
    // regression guard for that path, and the whole warm-start battery is
    // the guard for the Delta == +inf path, where the effective bounds ARE
    // the real bounds and this function is bit-identical to the pre-split
    // code.
    //
    // The pin materializes the REAL bound value (qp_real), not the effective
    // one -- which are the same number whenever the test above passes, since
    // a real bound inside the window cannot have been truncated by the TR.
    static void ingest_seed_working_set(const QpProblem &qp_real, const QpProblem &qp_eff,
                                        const QpSolution *seed, WorkingSet &ws, Vec &x) {
        const Index n = qp_real.n();
        const Index mi = qp_real.mi();
        const bool seed_bounds =
            seed != nullptr && static_cast<Index>(seed->bound_state.size()) == n;
        const bool seed_rows =
            seed != nullptr && static_cast<Index>(seed->ineq_active.size()) == mi;

        if (seed_bounds) {
            for (Index i = 0; i < n; ++i) {
                const auto si = static_cast<std::size_t>(i);
                // Already pinned by something this function may not override:
                // a genuinely fixed real bound, or section 6's zero-width
                // window flip.
                if (ws.bound_state()[si] == BoundState::kFixed) {
                    continue;
                }
                const bool at_lower = seed->bound_state[si] == BoundState::kAtLower;
                const bool at_upper = seed->bound_state[si] == BoundState::kAtUpper;
                if (!at_lower && !at_upper) {
                    continue;
                }
                const double bound = at_lower ? qp_real.lower(i) : qp_real.upper(i);
                if (at_lower ? !(bound > -detail::kEngineInfBound)
                             : !(bound < detail::kEngineInfBound)) {
                    continue; // no real bound on that side to pin at
                }
                if (bound < qp_eff.lower(i) || bound > qp_eff.upper(i)) {
                    continue; // WINDOW-CONSISTENCY RULE: outside the window, dropped
                }
                ws.bound_state()[si] = at_lower ? BoundState::kAtLower : BoundState::kAtUpper;
                x(i) = bound;
            }
        }
        if (seed_rows) {
            for (Index r = 0; r < mi; ++r) {
                if (seed->ineq_active[static_cast<std::size_t>(r)]) {
                    ws.add_ineq(r);
                }
            }
        }
    }

    // Recompute Ai*x and the homotopy shifts from the CURRENT x, clamping
    // negligible violations to zero, and make sure every still-shifted row is
    // in the working set (that is what drives its shift down).
    //
    // The clamp is scale-aware on purpose, for two independent reasons; a
    // bare absolute feas_tol makes a converged solve look like a live
    // homotopy shift and so reports kInfeasible on a feasible problem.
    //
    //  - Ai_j . x is a sum of terms of size ~ ||Ai_j||_1 * ||x||_inf that
    //    cancel down to ~0 on an active row, so its achievable residual grows
    //    with that magnitude.
    //  - More fundamentally, assemble_kkt puts -dual_mu on each constraint
    //    row's diagonal, so the regularized system solves Ai_j x - mu*lam_j =
    //    bi_j: a working row's residual has an irreducible footprint of
    //    dual_mu * |lam_j| (iterative refinement shrinks it but cannot erase
    //    it). Rows outside the working set carry lam_j == 0 and so get no
    //    such allowance. The engine therefore cannot certify feasibility
    //    tighter than its own regularization, and says so here rather than
    //    mistaking the footprint for an infeasibility. The allowance is
    //    CAPPED (kInfeasibilityAbsorbTol) so it cannot absorb a genuine
    //    contradiction -- see row_tolerance().
    // `opts` is the EFFECTIVE options this solve resolved (see run()'s
    // `eff_opts`) -- feas_tol/dual_mu below must be the resolved values, not
    // opts_'s own, since dual_mu can vary per solve via SolveOverrides.
    // OBSERVATION ONLY. Which constraints this solve has EVER admitted, so
    // QpCounters::distinct_{ineq,bound}_added can
    // report re-discovery directly instead of leaving it to a pigeonhole
    // argument that does not close. Two byte vectors, sized
    // mi and n, written once per FIRST admission of each constraint and read
    // by nothing.
    struct WalkSeen {
        std::vector<std::uint8_t> ineq;
        std::vector<std::uint8_t> bound;
    };

    static void mark_ineq_seen(WalkSeen &seen, QpCounters &counters, Index j) {
        auto &flag = seen.ineq[static_cast<std::size_t>(j)];
        if (flag == 0) {
            flag = 1;
            ++counters.distinct_ineq_added;
        }
    }

    static void mark_bound_seen(WalkSeen &seen, QpCounters &counters, Index i) {
        auto &flag = seen.bound[static_cast<std::size_t>(i)];
        if (flag == 0) {
            flag = 1;
            ++counters.distinct_bound_added;
        }
    }

    void refresh_shifts(const QpProblem &qp, const Vec &x, const Vec &ai_row_norm1,
                        const Vec &lambda_i, WorkingSet &ws, Vec &Aix, Vec &shift,
                        QpCounters &counters, WalkSeen &seen, const QpOptions &opts) const {
        const Index mi = qp.mi();
        if (mi == 0) {
            return;
        }
        Aix = qp.Ai * x;
        const double xmag = x.lpNorm<Eigen::Infinity>();
        for (Index j = 0; j < mi; ++j) {
            const double row_scale = std::max({1.0, std::abs(qp.bi(j)), ai_row_norm1(j) * xmag});
            const double s = Aix(j) - qp.bi(j);
            shift(j) = (s > row_tolerance(row_scale, lambda_i(j), opts)) ? s : 0.0;
            if (shift(j) > 0.0 && !in_working(ws, j)) {
                ws.add_ineq(j);
                ++counters.shift_adds; // observation only -- see QpCounters
                mark_ineq_seen(seen, counters, j);
            }
        }
    }

    // Feasibility tolerance for one constraint row (equality or inequality),
    // combining the two effects described above refresh_shifts: the row's own
    // magnitude, and the regularization's irreducible dual_mu*|lambda|
    // residual footprint -- the latter capped at kInfeasibilityAbsorbTol *
    // row_scale so a contradiction cannot fund its own tolerance. `opts` is
    // the effective options this solve resolved (see run()'s `eff_opts`).
    double row_tolerance(double row_scale, double lambda, const QpOptions &opts) const {
        return std::max(
            opts.feas_tol * row_scale,
            std::min(opts.dual_mu * std::abs(lambda), detail::kInfeasibilityAbsorbTol * row_scale));
    }

    // Is a single row's violation genuine evidence of infeasibility, rather
    // than the regularized solve's own noise? Both conditions must hold.
    //
    //  (a) The violation clears the row's tolerance by a real margin.
    //
    //  (b) The violation is STRUCTURAL: it reaches an appreciable fraction of
    //      the regularization footprint dual_mu*|lambda|. This is the sharp
    //      discriminator, and it is scale free. On an INCONSISTENT row the
    //      identity Ai x - dual_mu*lambda = bi is exact and irreducible, so
    //      the residual sits AT the footprint (ratio ~ 1) and no amount of
    //      iterative refinement moves it -- the system, not the solve, is at
    //      fault. On a merely ill-scaled but CONSISTENT row the same footprint
    //      is just a first-order error that refinement then knocks down by
    //      orders of magnitude, leaving ratio << 1. Measured on the ill-scaled
    //      ladder (row scaled by a, lambda ~ 1/(2a^2)): ratio is 0.005 at
    //      a = 1e-3 and 0.05 at a = 3e-4, against ~1.0 for every genuinely
    //      contradictory system.
    //
    // Testing the ratio rather than |lambda| against a fixed scale matters:
    // a contradiction with a small gap produces a correspondingly small
    // lambda (~gap/dual_mu) and would slip under any absolute multiplier
    // threshold, while its ratio stays pinned at ~1.
    // `opts` is the effective options this solve resolved (see run()'s
    // `eff_opts`).
    bool violation_is_structural(double v, double row_scale, double lambda,
                                 const QpOptions &opts) const {
        if (v <= detail::kInfeasibilityMarginFactor * row_tolerance(row_scale, lambda, opts)) {
            return false; // (a) within tolerance, nothing to explain
        }
        return v >= detail::kStructuralResidualFrac * opts.dual_mu * std::abs(lambda);
    }

    // Largest violation across BOTH constraint blocks that qualifies as
    // structural, or 0 if every violation is explainable as solver noise.
    // The shift machinery only watches inequalities, so without the equality
    // half an inconsistent equality block (or one that cannot be reached
    // inside the box) would be reported kOptimal. `opts` is the effective
    // options this solve resolved (see run()'s `eff_opts`).
    double worst_structural_violation(const QpProblem &qp, const Vec &x, const Vec &Aix,
                                      const Vec &ai_row_norm1, const Vec &lambda_i,
                                      const Vec &ae_row_norm1, const Vec &lambda_e,
                                      const QpOptions &opts) const {
        const double xmag = x.lpNorm<Eigen::Infinity>();
        double worst = 0.0;
        for (Index j = 0; j < qp.mi(); ++j) {
            const double v = Aix(j) - qp.bi(j);
            const double row_scale = std::max({1.0, std::abs(qp.bi(j)), ai_row_norm1(j) * xmag});
            if (v > 0.0 && violation_is_structural(v, row_scale, lambda_i(j), opts)) {
                worst = std::max(worst, v);
            }
        }
        if (qp.me() > 0) {
            const Vec resid = qp.Ae * x - qp.be;
            for (Index j = 0; j < qp.me(); ++j) {
                const double v = std::abs(resid(j));
                const double row_scale =
                    std::max({1.0, std::abs(qp.be(j)), ae_row_norm1(j) * xmag});
                if (violation_is_structural(v, row_scale, lambda_e(j), opts)) {
                    worst = std::max(worst, v);
                }
            }
        }
        return worst;
    }

    // Is ||x|| large in a way that only an unbounded direction explains?
    // Bound-relative on purpose: a variable boxed by finite bounds CANNOT run
    // away, and one pinned at a bound demonstrably has not, so only free
    // components that grew toward a non-constraining bound are eligible. A raw
    // ||x||_inf test libels every legitimately large-but-bounded solution.
    //
    // "Non-constraining" is measured against the runaway threshold itself
    // rather than against kEngineInfBound: a hand-written "effectively
    // infinite" bound like 1e18 is far below 1e20 but still cannot restrain an
    // artifact sitting at ~1/primal_delta, since a bound the solve could
    // actually reach would have pinned the variable there instead. `opts` is
    // the effective options this solve resolved (see run()'s `eff_opts`) --
    // primal_delta can vary per solve via SolveOverrides.
    bool is_runaway(const QpProblem &qp, const Vec &x, const WorkingSet &ws,
                    const QpOptions &opts) const {
        const double limit = detail::unbounded_artifact_scale(opts);
        for (Index i = 0; i < qp.n(); ++i) {
            if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
                continue; // pinned at a bound: it did not run away
            }
            const bool grew_up = x(i) > 0.0 && qp.upper(i) > limit;
            const bool grew_down = x(i) < 0.0 && qp.lower(i) < -limit;
            if ((grew_up || grew_down) && std::abs(x(i)) > limit) {
                return true;
            }
        }
        return false;
    }

    static bool in_working(const WorkingSet &ws, Index row) {
        const auto &aw = ws.active_ineq();
        return std::binary_search(aw.begin(), aw.end(), row);
    }

    // Step 2/4: dispatch on ws_algebra. eliminated_candidate() below is the
    // kRefactorize path; border mode falls back to it whenever bordering is
    // not available or not worth it for an iteration.
    //
    // `verdict` reports the inertia gate's reading of whichever system was
    // ACTUALLY solved (see the header contract's section 4b) -- the bordered
    // K0 when the border path served the iteration, the bound-eliminated K
    // when the elimination path did, on either mode. It is an out-parameter
    // rather than a member so the verdict can never outlive the
    // factorization it describes.
    // `opts` is the effective options this solve resolved (see run()'s
    // `eff_opts`); threaded down to every assembly/solve call below it that
    // reads primal_delta/dual_mu.
    EqpResult eqp_candidate(const QpProblem &qp, const WorkingSet &ws, detail::KktFactor &kkt,
                            BorderState &border, QpCounters &counters,
                            detail::InertiaVerdict &verdict, const QpOptions &opts) const {
        if (opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder) {
            return border_candidate(qp, ws, kkt, border, counters, verdict, opts);
        }
        return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
    }

    // One solve_eqp against a freshly assembled+factorized, bound-ELIMINATED
    // KKT system. Short-circuits the empty system (every variable pinned, no
    // equalities, no working rows), which Pardiso cannot be handed -- note
    // that short-circuit is also why a border-mode fallback on an
    // all-variables-pinned working set costs no factorization at all. `opts`
    // is the effective options this solve resolved (see run()'s `eff_opts`).
    EqpResult eliminated_candidate(const QpProblem &qp, const WorkingSet &ws,
                                   detail::KktFactor &kkt, QpCounters &counters,
                                   detail::InertiaVerdict &verdict, const QpOptions &opts) const {
        const bool empty_system = ws.num_free() == 0 && qp.me() == 0 && ws.active_ineq().empty();
        if (empty_system) {
            // Nothing was factorized, and a 0x0 system trivially has the
            // expected (0, 0, 0) inertia: every variable is pinned, so there
            // is no free direction for negative curvature to live in.
            verdict = detail::InertiaVerdict::kOk;
            Vec xf(qp.n());
            for (Index i = 0; i < qp.n(); ++i) {
                xf(i) = (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kAtUpper)
                            ? qp.upper(i)
                            : qp.lower(i);
            }
            return EqpResult{std::move(xf), Vec::Zero(0), Vec::Zero(0)};
        }
        ++counters.factorizations;
        EqpResult res = solve_eqp(qp, ws, kkt, opts);
        // EXTRA steps only, i.e. identically 0 (solver_counters.h): the eliminated path
        // takes its one mandatory step and has no iterated loop.
        counters.eqp_refine_steps += res.refine_steps;
        // solve_eqp leaves `kkt` holding the factorization of the
        // bound-ELIMINATED K, whose expected inertia is (n_free, me +
        // n_working, 0) -- the reduced form of the gate in section 4b.
        verdict = detail::inertia_verdict(kkt.factor.inertia(), ws.num_free(),
                                          qp.me() + static_cast<Index>(ws.active_ineq().size()));
        return res;
    }

    // Border-mode counterpart of eqp_candidate (see the header contract's
    // step 4): bring K0's border stack in line with `ws`, rebuild K0 if that
    // stack can no longer be trusted, then solve through it. The empty-system
    // short-circuit above has no counterpart here and needs none -- K0 spans
    // all n variables whatever the working set does. `opts` is the effective
    // options this solve resolved (see run()'s `eff_opts`).
    EqpResult border_candidate(const QpProblem &qp, const WorkingSet &ws, detail::KktFactor &kkt,
                               BorderState &border, QpCounters &counters,
                               detail::InertiaVerdict &verdict, const QpOptions &opts) const {
        // LATCHED: bordering has been abandoned for this working-set shape
        // (see border_solve_or_fall_back). Do not sync -- the whole point is
        // that every border added here would be discarded unused, and adding
        // them anyway is what drove dim() past schur_cap and made the wasted
        // work the dominant cost.
        if (border.latched) {
            if (latch_still_holds(ws)) {
                return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
            }
            // The pin count fell back to schur_cap or below, so bordering is
            // possible again. The stale stack cannot be incrementally repaired
            // (it was never updated, and the working ROWS may have moved
            // arbitrarily far during the latch -- see latch_still_holds), so
            // resume with a clean K0 built from the CURRENT working set.
            border.latched = false;
            rebuild_k0(qp, ws, border, counters, opts);
            sync_borders(qp, ws, border, counters, opts);
            return border_solve_or_fall_back(qp, ws, kkt, border, counters, verdict,
                                             /*rebuilt=*/true, opts);
        }

        // A K0 factorization the backend had to perturb pivots in is
        // untrustworthy whatever the border stack does, so that check comes
        // BEFORE the sync: re-adding borders onto a factorization already
        // known to be discarded is pure waste. This deliberately reads the
        // PREVIOUS solve's factorization evidence; the short-circuit
        // guarantees a successful factorization exists (`schur.has_value()`
        // implies rebuild_k0 completed). Rebuilds on non-kObserved evidence
        // (defensive, unreachable today), on a present-and-nonzero
        // perturbed-pivot count (MKL), or on a NATIVELY-OBSERVED nonzero
        // zero class (Accelerate's zero-pivot trust signal; inert on MKL,
        // where the zero class is derived rather than observed).
        bool rebuilt = false;
        bool k0_untrusted = false;
        if (border.schur.has_value()) {
            const hven::linear::InertiaEvidence e = border.kkt.factor.inertia();
            k0_untrusted = e.state != hven::linear::InertiaEvidence::State::kObserved ||
                           (e.perturbed_pivots.has_value() && *e.perturbed_pivots != 0) ||
                           (!e.zero_is_derived && e.n_zero != 0);
        }
        if (!border.schur.has_value() || k0_untrusted) {
            rebuild_k0(qp, ws, border, counters, opts); // the seed working set's K0
            rebuilt = true;
        }
        sync_borders(qp, ws, border, counters, opts);
        return border_solve_or_fall_back(qp, ws, kkt, border, counters, verdict, rebuilt, opts);
    }

    // Shared tail of both entry paths above: decide whether the border stack
    // is usable, spend at most one rebuild trying to make it so, and solve --
    // falling back to the elimination path (and latching where appropriate)
    // when it is not. `opts` is the effective options this solve resolved
    // (see run()'s `eff_opts`).
    EqpResult border_solve_or_fall_back(const QpProblem &qp, const WorkingSet &ws,
                                        detail::KktFactor &kkt, BorderState &border,
                                        QpCounters &counters, detail::InertiaVerdict &verdict,
                                        bool rebuilt, const QpOptions &opts) const {
        if (border.schur->needs_refactorization()) {
            // Rebuilding re-derives K0 from the CURRENT working set, folding
            // the working ROWS that triggered it back into K0. It cannot do
            // the same for PINS: assemble_kkt_full eliminates nothing, so a
            // pinned variable is a border no matter which working set K0 is
            // built from, and rebuilding when the live borders are all pins
            // over K0's own rows reproduces the identical state. Left
            // unguarded that is a permanent trigger -- every iteration past
            // schur_cap pins would clear the ledger and re-add every pin,
            // which is quadratic in n on a box-shaped QP and was measured
            // three orders of magnitude slower than refactorize mode.
            //
            // So: when a rebuild would be a no-op, LATCH -- serve this and
            // subsequent iterations from the elimination path, where pins ARE
            // eliminable, and stop maintaining a border stack nothing reads.
            if (rebuild_would_be_noop(ws, border)) {
                border.latched = true;
                return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
            }
            // A rebuild that has already been spent this iteration, or one
            // that did not clear the flag, also falls back -- but only latches
            // if the state it left behind is the pins-only dead end.
            if (rebuilt) {
                return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
            }
            rebuild_k0(qp, ws, border, counters, opts);
            sync_borders(qp, ws, border, counters, opts);
            if (border.schur->needs_refactorization()) {
                border.latched = rebuild_would_be_noop(ws, border);
                return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
            }
        }

        // SchurComplement::solve throws std::runtime_error on an exactly
        // singular C. The checks above are meant to have ruled that out, but
        // "meant to" is not a contract: QpEngine::solve owes a QpStatus for
        // every solvable input and must not leak a linear-algebra exception,
        // so a singular factor that slips through degrades to the elimination
        // path like any other untrustworthy border stack.
        //
        // That must NOT swallow every std::runtime_error, though: the KKT
        // factor throws the same type for a failed backend factorization
        // (factorize_checked) and for a solve before any successful
        // factorize, and those are genuine faults that
        // kRefactorize mode reports and border mode must not hide. The
        // discriminator is the state the throw leaves behind -- a singular C
        // sets SchurComplement's own singular_ flag, so needs_refactorization()
        // is necessarily true afterward. If it is not, this was some other
        // failure and it is rethrown untouched.
        try {
            // The gate reads the system that is about to be solved: K0's
            // pardiso inertia plus the live border stack's own contribution.
            // Evaluated BEFORE the solve so a throw below cannot leave a
            // verdict describing a system that was never solved.
            verdict = border_inertia_verdict(qp, border);
            EqpResult res = solve_bordered_eqp(qp, border.k0, border.k0_rows, *border.schur,
                                               border.ledger, ws, opts);
            // TOTAL steps, mandatory first one included (solver_counters.h).
            counters.border_refine_steps += res.refine_steps;
            return res;
        } catch (const std::runtime_error &) {
            if (!border.schur->needs_refactorization()) {
                throw;
            }
            return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
        }
    }

    // The inertia gate for the BORDERED system K0 + live border stack (see
    // the header contract's section 4b for the derivation of both sides).
    // `border.schur` must have a value.
    detail::InertiaVerdict border_inertia_verdict(const QpProblem &qp,
                                                  const BorderState &border) const {
        const SchurComplement &schur = *border.schur;
        if (schur.needs_refactorization()) {
            // C's inertia is either unreadable (an exactly singular factor
            // makes expected_neg_eigs_delta() THROW) or not worth reading
            // (past the cap / condition limit). Either way the bordered
            // system's inertia is unknown, not wrong.
            return detail::InertiaVerdict::kSuspect;
        }
        const Index dim = schur.dim();
        const Index neg_c = schur.expected_neg_eigs_delta();
        if (neg_c < 0 || neg_c > dim) {
            return detail::InertiaVerdict::kSuspect;
        }

        // STRUCTURAL expectation for the whole bordered matrix: K0 supplies
        // (n, me + n_w0) and each border supplies one negative eigenvalue,
        // except a kRowDelete, which pairs with the K0 row it kills into an
        // indefinite 2x2 and so supplies one positive and one negative.
        Index deletes = 0;
        for (const BorderLedgerEntry &e : border.ledger) {
            if (e.kind == BorderLedgerEntry::Kind::kRowDelete) {
                ++deletes;
            }
        }
        const Index n_w0 = static_cast<Index>(border.k0_rows.size());
        const Index expected_pos = qp.n() + deletes;
        const Index expected_neg = qp.me() + n_w0 + dim - deletes;

        // ACTUAL inertia = K0's (backend evidence) + C's (Haynsworth).
        // Rather than adding C's contribution to the backend's counts, back
        // it out of the expectation and let the shared helper compare K0's
        // own numbers -- same test, and it keeps the perturbed-pivot policy
        // in one place.
        const Index extra_neg = neg_c;
        const Index extra_pos = dim - neg_c;
        return detail::inertia_verdict(border.kkt.factor.inertia(), expected_pos - extra_pos,
                                       expected_neg - extra_neg);
    }

    // Re-run the current iteration's linear algebra for `ws` purely to read
    // the gate off it. Deliberately routed through eqp_candidate rather than
    // a bespoke factorize-and-peek: the verdict must describe the system the
    // LOOP would solve for this working set, including every rebuild, latch
    // and fallback decision, and duplicating that dispatch here is how the
    // two would drift apart. The EqpResult itself is discarded -- in border
    // mode a probe costs a few K0 solves and a dense C rebuild (schur_updates,
    // no factorization); in refactorize mode it costs one factorization,
    // counted like any other.
    // `opts` is the effective options this solve resolved (see run()'s
    // `eff_opts`).
    detail::InertiaVerdict probe_inertia(const QpProblem &qp, const WorkingSet &ws,
                                         detail::KktFactor &kkt, BorderState &border,
                                         QpCounters &counters, const QpOptions &opts) const {
        detail::InertiaVerdict verdict = detail::InertiaVerdict::kOk;
        (void)eqp_candidate(qp, ws, kkt, border, counters, verdict, opts);
        return verdict;
    }

    // Section 4b's ZERO-MULTIPLIER PROBE, run at the would-be-kOptimal exit.
    // Walks every WEAKLY active working-set member (multiplier numerically
    // zero), most recently added first, tentatively drops each and re-runs
    // the inertia gate on the reduced labeling:
    //   kWrong   the hidden negative curvature is real. The drop is made
    //            REAL (`dropped` filled in exactly as drop_worst fills it,
    //            arming section 4c's ride on the next iteration) and this
    //            returns true, so the caller RESUMES the loop instead of
    //            certifying.
    //   kOk      that constraint hides nothing. Restore it and try the next.
    //   kSuspect treated exactly as kOk -- the reduced inertia is UNKNOWN, and
    //            a fabricated pivot sign is never ground truth (see section
    //            4b for the gap this leaves and what closes it).
    //
    // Returns false if every candidate came back kOk/kSuspect (or there were
    // none at all), leaving `ws` exactly as it found it and the caller free to
    // certify.
    //
    // COST WHEN NOTHING IS WEAKLY ACTIVE IS ZERO: the candidate list is built
    // from multipliers the loop already priced, and an empty list returns
    // before any linear algebra runs (always the case under strict
    // complementarity, every convex fixture in this file's battery). `opts`
    // is the effective options this solve resolved (see run()'s `eff_opts`),
    // threaded through to probe_inertia -> eqp_candidate, which needs the
    // effective primal_delta/dual_mu.
    bool probe_zero_multiplier_drops(const QpProblem &qp, const Vec &shift, const Vec &lambda_i,
                                     const Vec &z, WorkingSet &ws, detail::KktFactor &kkt,
                                     BorderState &border, QpCounters &counters, ProbeState &probe,
                                     DropRecord &dropped, const QpOptions &opts) const {
        struct Candidate {
            bool is_ineq = false;
            Index idx = -1;
            Index seq = -1;
        };
        std::vector<Candidate> cands;
        for (const Index row : ws.active_ineq()) {
            const auto sr = static_cast<std::size_t>(row);
            // A row the homotopy is still driving to feasibility is not
            // WEAKLY active -- x does not sit on it at all, so it excludes no
            // direction of the critical cone and its multiplier's size says
            // nothing (the same reason drop_worst skips it). Dropping one
            // would also just be undone: refresh_shifts re-adds every shifted
            // row on the next iteration.
            if (shift(row) > 0.0) {
                continue;
            }
            if (probe.ineq_exempt[sr] || std::abs(lambda_i(row)) > opts.opt_tol) {
                continue;
            }
            cands.push_back({true, row, probe.ineq_seq[sr]});
        }
        for (Index i = 0; i < qp.n(); ++i) {
            const auto si = static_cast<std::size_t>(i);
            const BoundState st = ws.bound_state()[si];
            // kFixed is lower == upper: there is no feasible direction off it
            // to expose, and the drop rule does not release it either.
            if (st != BoundState::kAtLower && st != BoundState::kAtUpper) {
                continue;
            }
            if (probe.bound_exempt[si] || std::abs(z(i)) > opts.opt_tol) {
                continue;
            }
            cands.push_back({false, i, probe.bound_seq[si]});
        }
        if (cands.empty()) {
            return false;
        }
        // Most recently added first: the newest member of the working set is
        // the one the loop has done the least to justify, and trying it first
        // keeps the common case (one weakly active constraint, just added) to
        // a single probe.
        std::stable_sort(cands.begin(), cands.end(),
                         [](const Candidate &a, const Candidate &b) { return a.seq > b.seq; });

        for (const Candidate &c : cands) {
            const auto si = static_cast<std::size_t>(c.idx);
            const BoundState held = c.is_ineq ? BoundState::kFree : ws.bound_state()[si];
            if (c.is_ineq) {
                ws.drop_ineq(c.idx);
            } else {
                ws.bound_state()[si] = BoundState::kFree;
            }

            const detail::InertiaVerdict verdict =
                probe_inertia(qp, ws, kkt, border, counters, opts);
            if (verdict == detail::InertiaVerdict::kWrong) {
                dropped = DropRecord{};
                dropped.active = true;
                dropped.is_ineq = c.is_ineq;
                dropped.idx = c.idx;
                dropped.from = held;
                // Observation only (see QpCounters): counted HERE, on the
                // branch that KEEPS the drop, never on the restore path
                // below -- a probed-and-restored constraint leaves the
                // working set unchanged and counting it would break the
                // net identity QpCounters states.
                ++counters.ws_drops;
                if (!c.is_ineq) {
                    ++counters.ws_drops_bound;
                }
                // The exemption is keyed by CONSTRAINT IDENTITY -- an Ai row
                // index, or a VARIABLE index for a bound -- never by which
                // SIDE a bound was pinned at. So a variable dropped from
                // kAtLower and later re-pinned at kAtUpper inherits this
                // exemption and is not probed again, even though that is
                // arguably a different constraint (-x_i <= -l_i versus
                // x_i <= u_i). Deliberately conservative: the two sides share
                // the one direction e_i that a drop frees, so a probe of the
                // second side would re-examine the same enlarged null space
                // the first one already ruled on -- and if the answer HAS
                // changed because x moved, the exemption costs at most a
                // missed drop, never a wrong one. Erring the other way would
                // put an alternating lower/upper pin back inside the cycle
                // this set exists to break.
                if (c.is_ineq) {
                    probe.ineq_exempt[si] = true;
                } else {
                    probe.bound_exempt[si] = true;
                }
                return true;
            }
            // kOk or kSuspect: put back the CONSTRAINT. Border-mode state is
            // deliberately left as the probe left it, and there are TWO pieces
            // of it, not one:
            //   - border.ledger (and the Schur stack it indexes), which now
            //     describes the REDUCED working set;
            //   - border.latched, which the probe's own eqp_candidate call may
            //     have set or cleared, and which a rebuild_k0 inside the probe
            //     may have re-seeded.
            // Neither is restored, and neither needs to be, for the same
            // reason: both are DERIVED views that border_candidate reconciles
            // against `ws` unconditionally on its next call -- sync_borders
            // re-adds whatever this restore just put back, and latch_still_holds
            // re-decides the latch from the current working set's own pin count
            // rather than from history. This is exactly the contract
            // repair_temporary_vertex already relies on when it unwinds. The
            // only observable consequence is a few extra schur_updates, and
            // only on a solve that had a zero-multiplier candidate to probe.
            if (c.is_ineq) {
                ws.add_ineq(c.idx);
            } else {
                ws.bound_state()[si] = held;
            }
        }
        return false;
    }

    // H's diagonal, gathered once (qp.H stores the upper triangle, so the
    // diagonal entries are the ones with col == row).
    static Vec hessian_diagonal(const QpProblem &qp) {
        Vec d = Vec::Zero(qp.n());
        for (Index i = 0; i < qp.n(); ++i) {
            for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
                if (it.col() == i) {
                    d(i) = it.value();
                }
            }
        }
        return d;
    }

    // Pin variable `i` at a bound, moving x(i) there, per the WHICH BOUND
    // rule in the header contract's section 4b. Returns false (leaving ws and
    // x untouched) if neither bound is finite.
    bool pin_at_best_bound(const QpProblem &qp, WorkingSet &ws, Vec &x, const Vec &grad, Index i,
                           double hii) const {
        const double lo = qp.lower(i);
        const double up = qp.upper(i);
        const bool lo_ok = lo > -detail::kEngineInfBound;
        const bool up_ok = up < detail::kEngineInfBound;
        if (!lo_ok && !up_ok) {
            return false;
        }
        const auto si = static_cast<std::size_t>(i);
        const double tol = opts_.feas_tol * std::max(1.0, std::abs(x(i)));
        // Already sitting on a bound: pin it exactly where it is, so a warm
        // start's own vertex is never disturbed by the repair.
        if (lo_ok && std::abs(x(i) - lo) <= tol) {
            ws.bound_state()[si] = BoundState::kAtLower;
            x(i) = lo;
            return true;
        }
        if (up_ok && std::abs(x(i) - up) <= tol) {
            ws.bound_state()[si] = BoundState::kAtUpper;
            x(i) = up;
            return true;
        }
        // Otherwise take the bound that lowers the objective more along e_i.
        auto delta_f = [&](double bound) {
            const double t = bound - x(i);
            return grad(i) * t + 0.5 * hii * t * t;
        };
        const bool take_lower =
            lo_ok && (!up_ok || delta_f(lo) <= delta_f(up)); // ties -> lower, deterministically
        ws.bound_state()[si] = take_lower ? BoundState::kAtLower : BoundState::kAtUpper;
        x(i) = take_lower ? lo : up;
        return true;
    }

    // Temporary-vertex start repair (header contract, section 4b). Mutates
    // `ws` and `x` and returns true iff it reached a second-order consistent
    // working set; on failure both are restored to what they were.
    // `opts` is the effective options this solve resolved (see run()'s
    // `eff_opts`), threaded through to probe_inertia -> eqp_candidate.
    bool repair_temporary_vertex(const QpProblem &qp, WorkingSet &ws, Vec &x,
                                 detail::KktFactor &kkt, BorderState &border, QpCounters &counters,
                                 const QpOptions &opts) const {
        const Index n = qp.n();
        const Vec hdiag = hessian_diagonal(qp);

        // Most negative H diagonal first -- that is where the negative
        // curvature is concentrated -- with index order breaking ties, so the
        // repair is deterministic. stable_sort over an already index-ordered
        // list gives that tie-break for free.
        std::vector<Index> order;
        for (Index i = 0; i < n; ++i) {
            if (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree) {
                order.push_back(i);
            }
        }
        std::stable_sort(order.begin(), order.end(),
                         [&](Index a, Index b) { return hdiag(a) < hdiag(b); });

        const std::vector<BoundState> saved_state = ws.bound_state();
        const Vec saved_x = x;

        std::vector<Index> pinned;
        detail::InertiaVerdict verdict = detail::InertiaVerdict::kWrong;
        for (const Index i : order) {
            Vec grad = qp.H.selfadjointView<Eigen::Upper>() * x + qp.g;
            if (!pin_at_best_bound(qp, ws, x, grad, i, hdiag(i))) {
                continue; // unbounded both ways: nothing to pin against
            }
            pinned.push_back(i);
            verdict = probe_inertia(qp, ws, kkt, border, counters, opts);
            if (verdict != detail::InertiaVerdict::kWrong) {
                break; // repaired, or no longer verifiable
            }
        }
        if (verdict != detail::InertiaVerdict::kOk) {
            // Either we ran out of variables to pin, or the gate stopped
            // being readable partway through (kSuspect). Neither leaves us
            // able to certify the repaired working set, so undo it entirely
            // and let the loop proceed exactly as it would have without this
            // repair. The border stack is left alone: sync_borders reconciles
            // it against ws unconditionally on the next iteration.
            ws.bound_state() = saved_state;
            x = saved_x;
            return false;
        }

        // Release pass: a pin added before the one that actually fixed the
        // inertia may not have been needed. Reverse order (newest first) so
        // the pin most likely to be load-bearing is tried last.
        for (auto it = pinned.rbegin(); it != pinned.rend(); ++it) {
            const auto si = static_cast<std::size_t>(*it);
            const BoundState held = ws.bound_state()[si];
            ws.bound_state()[si] = BoundState::kFree;
            if (probe_inertia(qp, ws, kkt, border, counters, opts) != detail::InertiaVerdict::kOk) {
                ws.bound_state()[si] = held; // negative curvature is still there: keep the pin
            }
        }
        return true;
    }

    // Would re-deriving K0 from `ws` reproduce exactly the state we are in?
    // It would iff K0 already spans the current working ROWS (so the rebuild
    // assembles the same matrix) and every live border is a pin (so the
    // re-sync re-adds the same borders). Rebuilding then buys nothing and
    // costs a factorization.
    static bool rebuild_would_be_noop(const WorkingSet &ws, const BorderState &border) {
        if (ws.active_ineq() != border.k0_rows) {
            return false;
        }
        return std::all_of(
            border.ledger.begin(), border.ledger.end(),
            [](const BorderLedgerEntry &e) { return e.kind == BorderLedgerEntry::Kind::kVarPin; });
    }

    // Is the pins-only dead end still the situation we are in? EXACTLY ONE
    // question decides it: can a border stack carry the current pin count at
    // all? Pins can never be folded into K0 (assemble_kkt_full eliminates
    // nothing), so IMMEDIATELY AFTER ANY rebuild_k0 the live stack is exactly
    // the pins -- which means that while `pinned > schur_cap`, every possible
    // K0 is born already past needs_refactorization() and bordering is not
    // available at any price. The latch holds until that stops being true.
    //
    // Testing the pin COUNT rather than "fewer pins than at latch time" is
    // what keeps this from oscillating: releasing pins one at a time above the
    // cap changes nothing, and the single release that crosses the cap costs
    // exactly one rebuild.
    //
    // The latch releases on the PIN COUNT alone, never on a change to the
    // working ROWS: a working set whose rows change every iteration (an
    // active-set identification sweep, e.g. an SQP subproblem on a
    // path-constrained collocation NLP) would otherwise thrash the latch
    // once per row change, and each release is far more expensive than one
    // refactorize-mode iteration -- a full assemble_kkt_full + Pardiso
    // factorization with fresh symbolic analysis, plus a sync_borders that
    // re-adds EVERY pin -- and buys nothing: the state it resumes into still
    // has dim() == pinned > schur_cap, so needs_refactorization() is true
    // again before anything is solved and the latch is retaken on the same
    // iteration.
    //
    // Correctness of holding through a row change: nothing reads K0 or the
    // border stack while latched (the elimination path builds its own reduced
    // K), and the release below rebuilds K0 from the CURRENT working set, so a
    // stack that went stale during a long latch is discarded rather than
    // trusted -- exactly as it always was.
    bool latch_still_holds(const WorkingSet &ws) const {
        const Index pinned = ws.n() - ws.num_free();
        return pinned > opts_.schur_cap;
    }

    // Assemble + factorize a fresh full-variable K0 for the CURRENT working
    // set and restart the border stack empty. Counted in
    // QpCounters::factorizations. `opts` is the effective options this solve
    // resolved (see run()'s `eff_opts`) -- K0's regularized diagonal is built
    // from opts.primal_delta/opts.dual_mu, which is exactly what reuse-key
    // condition (d) tracks.
    //
    // THIS IS THE SOLE SITE that reassigns `border.k0` or factorizes
    // `border.kkt`. It stamps nothing itself: `factorize_checked()` advancing
    // the factor's epoch (and `analyze()` moving its session id at a pattern
    // change) IS the stamp, for EVERY caller, engine-own or shared.
    // sync_borders() below only ever adds/drops BORDERS around whatever K0
    // this call last built; it never touches `k0`/`kkt`.
    //
    // COMMIT-LAST: nothing this function publishes onto `border` is
    // published until the factorization has SUCCEEDED. Assigning
    // `k0`/`k0_rows` before the factorization completes would publish a NEW
    // K0 against the OLD factorization, ledger and Schur cache if the
    // SYMBOLIC analysis then threw (SymmetricFactor::analyze()'s strong
    // guarantee leaves session id/epoch/inertia untouched on a failed
    // analysis, so the usable-numerics reuse conjunct would see a perfectly
    // healthy factor and grant reuse) -- a second holder of this shared
    // BorderState would then solve the bordered system against a factor of a
    // matrix that is no longer `border.k0`, silently wrong rather than
    // detectably wrong. Assembling into locals and moving them in afterward
    // costs nothing: Eigen's sparse move-assignment is a swap.
    void rebuild_k0(const QpProblem &qp, const WorkingSet &ws, BorderState &border,
                    QpCounters &counters, const QpOptions &opts) const {
        KktAssembly k0 = assemble_kkt_full(qp, ws, opts);
        std::vector<Index> k0_rows = ws.active_ineq();
        // The analysis decision (QpCounters::symbolic_analyses' own note):
        // decided BEFORE the factorize, which is the only way to know whether
        // THIS call is about to pay the backend's symbolic analysis --
        // factorize_checked() acts on the decision but does not report it
        // back. The decision is TAKEN here and HANDED to factorize_checked(),
        // rather than taken twice, so one factorization costs one pattern hash
        // at this layer and not two (kkt_calls.h's AnalysisDecision).
        const detail::AnalysisDecision analysis = detail::analysis_decision(border.kkt, k0.K);
        if (analysis.needed) {
            ++counters.symbolic_analyses;
        }
        detail::factorize_checked(border.kkt, k0.K, analysis);
        ++counters.factorizations;

        // Commit, against a factorization that already succeeded. Same
        // statements in the same order as before, with the same effect: both
        // members that used to be assigned at the TOP of this function are
        // assigned here instead, from locals built up there, so the two
        // allocations they need are still paid before the factorize and only
        // the (non-throwing) hand-over moved. The clear/emplace pair below
        // always ran here.
        border.k0 = std::move(k0);
        border.k0_rows = std::move(k0_rows);
        border.ledger.clear();
        // Constructed AFTER the factorization: add_border caches K0^-1 v
        // against whatever `border.kkt` currently holds. SchurComplement only
        // ever reads schur_cap/schur_cond_max from this -- both unaffected by
        // SolveOverrides -- so passing `opts` here rather than opts_ changes
        // nothing observable; it is done anyway so this function has one
        // single, consistent options value.
        border.schur.emplace(border.kkt, opts);
    }

    // Apply the delta between the working set `border`'s ledger currently
    // represents and `ws`, one BorderOps construction per change. Drops run
    // first, in REVERSE ledger order, so the add_border-order indices
    // SchurComplement::drop_border expects stay valid as entries are removed.
    //
    // Note a pin border's ledger entry names only the VARIABLE, so a bound
    // that switches side (kAtLower <-> kAtUpper) needs no border work at all:
    // the pinned value is read from ws.bound_state() when the rhs is built
    // (solve_bordered_eqp), not baked into the border column.
    // `opts` is the effective options this solve resolved (see run()'s
    // `eff_opts`) -- the -dual_mu diagonal entries below must be the
    // resolved value, not opts_'s own.
    void sync_borders(const QpProblem &qp, const WorkingSet &ws, BorderState &border,
                      QpCounters &counters, const QpOptions &opts) const {
        const Index n = qp.n();
        const Index me = qp.me();
        const Index n0 = border.k0.K.rows();
        const std::vector<BoundState> &bs = ws.bound_state();
        std::vector<BorderLedgerEntry> &ledger = border.ledger;

        for (Index b = static_cast<Index>(ledger.size()) - 1; b >= 0; --b) {
            const BorderLedgerEntry &e = ledger[static_cast<std::size_t>(b)];
            bool live = true;
            switch (e.kind) {
            case BorderLedgerEntry::Kind::kVarPin:
                live = bs[static_cast<std::size_t>(e.target)] != BoundState::kFree;
                break;
            case BorderLedgerEntry::Kind::kRowDelete:
                // Re-activating a row K0 owns is the exact INVERSE of
                // deactivating it: drop the delete border and K0's own row
                // (with its own dual) is live again -- no add_ineq_row
                // duplicate of a row K0 already carries.
                live = !in_working(ws, border.k0_rows[static_cast<std::size_t>(e.target - me)]);
                break;
            case BorderLedgerEntry::Kind::kIneqRow:
                live = in_working(ws, e.target);
                break;
            }
            if (!live) {
                border.schur->drop_border(b);
                ledger.erase(ledger.begin() + static_cast<std::ptrdiff_t>(b));
                ++counters.schur_updates;
            }
        }

        for (Index i = 0; i < n; ++i) {
            if (bs[static_cast<std::size_t>(i)] == BoundState::kFree ||
                has_border(ledger, BorderLedgerEntry::Kind::kVarPin, i)) {
                continue;
            }
            // RESERVE BEFORE THE SCHUR MUTATION, in all three add loops. The
            // ledger and the border stack are one structure kept in two
            // places, matched by add-order POSITION:
            // the drop loop above walks the ledger and drops the border at
            // the same index. So an add_border() that succeeds followed by a
            // ledger.push_back() that throws on its growth allocation leaves
            // a border the ledger cannot name -- undroppable forever, and
            // shifting every later drop by one, which removes the WRONG
            // border and silently solves the wrong bordered system. A trivial
            // entry has no other throw in it than that allocation, so
            // reserving here puts the only throw AHEAD of the border stack
            // mutation, where a throw costs nothing. (The add_border side of
            // the same invariant is closed in schur_complement.h: a throwing
            // add leaves the stack exactly as it was.)
            ledger.reserve(ledger.size() + 1);
            border.schur->add_border(BorderOps::pin_variable(i, n0), -opts.dual_mu);
            ledger.push_back({BorderLedgerEntry::Kind::kVarPin, i});
            ++counters.schur_updates;
        }
        for (std::size_t p = 0; p < border.k0_rows.size(); ++p) {
            const Index k = me + static_cast<Index>(p); // K0 constraint-row index
            if (in_working(ws, border.k0_rows[p]) ||
                has_border(ledger, BorderLedgerEntry::Kind::kRowDelete, k)) {
                continue;
            }
            ledger.reserve(ledger.size() + 1); // see the pin loop above
            border.schur->add_border(BorderOps::delete_k0_row(k, me, n, n0), 0.0);
            ledger.push_back({BorderLedgerEntry::Kind::kRowDelete, k});
            ++counters.schur_updates;
        }
        for (const Index row : ws.active_ineq()) {
            if (std::binary_search(border.k0_rows.begin(), border.k0_rows.end(), row) ||
                has_border(ledger, BorderLedgerEntry::Kind::kIneqRow, row)) {
                continue;
            }
            ledger.reserve(ledger.size() + 1); // see the pin loop above
            border.schur->add_border(BorderOps::add_ineq_row(qp, row, n0), -opts.dual_mu);
            ledger.push_back({BorderLedgerEntry::Kind::kIneqRow, row});
            ++counters.schur_updates;
        }
    }

    static bool has_border(const std::vector<BorderLedgerEntry> &ledger,
                           BorderLedgerEntry::Kind kind, Index target) {
        return std::any_of(ledger.begin(), ledger.end(), [&](const BorderLedgerEntry &e) {
            return e.kind == kind && e.target == target;
        });
    }

    // Scatter the EQP multipliers into full-length vectors and price the
    // bound multipliers from the stationarity residual (eqp_solve.h).
    static void price(const QpProblem &qp, const WorkingSet &ws, const EqpResult &eqp,
                      Vec &lambda_e, Vec &lambda_i, Vec &z) {
        lambda_e = eqp.lambda_e;
        lambda_i.setZero();
        const auto &aw = ws.active_ineq();
        for (std::size_t k = 0; k < aw.size(); ++k) {
            lambda_i(aw[k]) = eqp.lambda_w(static_cast<Index>(k));
        }

        Vec r = qp.H.selfadjointView<Eigen::Upper>() * eqp.x + qp.g;
        if (qp.me() > 0) {
            r += qp.Ae.transpose() * lambda_e;
        }
        if (qp.mi() > 0) {
            r += qp.Ai.transpose() * lambda_i;
        }
        z.setZero();
        for (Index i = 0; i < qp.n(); ++i) {
            if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
                z(i) = r(i);
            }
        }
    }

    // Step 2's drop rule. Returns true (and mutates `ws`) iff something left
    // the working set; false means the point is optimal. `dropped` records
    // WHAT left, which section 4c's ride needs to choose its sign; it is left
    // untouched when nothing is dropped.
    bool drop_worst(const QpProblem &qp, const Vec &shift, const Vec &lambda_i, const Vec &z,
                    WorkingSet &ws, DropRecord &dropped, QpCounters &counters) const {
        struct Candidate {
            bool is_ineq = false;
            Index idx = -1;
            double violation = 0.0;
            double angle = 0.0; // violation / ||constraint gradient||
        };

        std::vector<Candidate> cands;
        for (Index row : ws.active_ineq()) {
            if (shift(row) > 0.0) {
                continue; // still being driven to feasibility; sign says nothing
            }
            const double viol = -lambda_i(row);
            if (viol <= opts_.opt_tol) {
                continue;
            }
            const double nrm = qp.Ai.row(row).norm();
            cands.push_back({true, row, viol, viol / std::max(nrm, detail::kEngineDenomTol)});
        }
        for (Index i = 0; i < qp.n(); ++i) {
            const BoundState st = ws.bound_state()[static_cast<std::size_t>(i)];
            double viol = 0.0;
            if (st == BoundState::kAtLower) {
                viol = -z(i);
            } else if (st == BoundState::kAtUpper) {
                viol = z(i);
            } else {
                continue; // kFree has no multiplier, kFixed is not sign-constrained
            }
            if (viol > opts_.opt_tol) {
                cands.push_back({false, i, viol, viol}); // ||e_i|| == 1
            }
        }
        if (cands.empty()) {
            return false;
        }

        // Dantzig: most negative multiplier; exact ties go to the largest
        // angle, then to the first candidate (inequalities before bounds,
        // both in ascending index order).
        double best_viol = 0.0;
        for (const auto &c : cands) {
            best_viol = std::max(best_viol, c.violation);
        }
        const double tie_window = best_viol * detail::kEngineDropTieTol;
        const Candidate *pick = nullptr;
        Index in_window = 0; // observation only -- see QpCounters::drop_ties
        for (const auto &c : cands) {
            if (c.violation < best_viol - tie_window) {
                continue;
            }
            ++in_window;
            if (pick == nullptr || c.angle > pick->angle) {
                pick = &c;
            }
        }
        if (in_window > 1) {
            ++counters.drop_ties; // observation only -- see QpCounters
        }

        dropped = DropRecord{};
        dropped.active = true;
        dropped.is_ineq = pick->is_ineq;
        dropped.idx = pick->idx;
        ++counters.ws_drops; // observation only -- see QpCounters
        if (!pick->is_ineq) {
            ++counters.ws_drops_bound;
        }
        if (pick->is_ineq) {
            ws.drop_ineq(pick->idx);
        } else {
            const auto si = static_cast<std::size_t>(pick->idx);
            dropped.from = ws.bound_state()[si];
            ws.bound_state()[si] = BoundState::kFree;
        }
        return true;
    }

    // Directional derivative of the constraint `dropped` names, along `d`.
    // Stated so that a NEGATIVE value means "d moves into that constraint's
    // feasible side", uniformly for a general row (a_j . x <= b_j) and for a
    // bound pin (x_i <= u_i at an upper bound, -x_i <= -l_i at a lower one).
    double dropped_directional(const QpProblem &qp, const DropRecord &dropped, const Vec &d) const {
        if (dropped.is_ineq) {
            return qp.Ai.row(dropped.idx).dot(d);
        }
        const double di = d(dropped.idx);
        return (dropped.from == BoundState::kAtUpper) ? di : -di;
    }

    // Does the ride's ray stay inside the current working set -- i.e. does `p`
    // lie in the null space of the working constraints (equalities, working
    // inequality rows, pinned variables)?
    //
    // THIS IS A PRECONDITION OF THE RIDE, not a nicety. The uncapped ratio
    // test SKIPS working rows, on the standing assumption that a step along an
    // EQP direction preserves them. That assumption is what makes the ordinary
    // step safe -- it is capped at alpha = 1, which lands exactly on the EQP's
    // own point, where every working row holds by construction. A ride has no
    // such cap, so if p has any component out of that null space the ride will
    // sail straight through a working row that nothing is watching. Two ways
    // that happens in practice:
    //   - the HOMOTOPY: a working row with a positive shift is deliberately
    //     not satisfied at x, and the EQP pulls toward the true rhs, so
    //     A_j p != 0 by design;
    //   - a DEGENERATE working set (more active constraints than dimensions),
    //     where the regularized solve satisfies the rows only in a
    //     least-squares sense and x sits off them by O(dual_mu * |lambda|),
    //     which is O(1) once |lambda| reaches its ~1/dual_mu artifact size.
    // Measured on a randomized indefinite battery with an unbounded variable:
    // without this test, a ride down a direction that violated a working row
    // reported a bounded QP as unbounded (kNumericalError) where the engine
    // had previously returned a valid local minimizer.
    //
    // Failing it DECLINES the ride, i.e. falls back to the ordinary capped
    // step, which handles both cases correctly (it stops at alpha = 1, on the
    // working rows, and the homotopy closes the shift there).
    bool ride_stays_in_working_set(const QpProblem &qp, const WorkingSet &ws, const Vec &p) const {
        const double pn = p.norm();
        for (Index j = 0; j < qp.me(); ++j) {
            if (std::abs(qp.Ae.row(j).dot(p)) >
                detail::kRideNullspaceTol * qp.Ae.row(j).norm() * pn) {
                return false;
            }
        }
        for (const Index row : ws.active_ineq()) {
            if (std::abs(qp.Ai.row(row).dot(p)) >
                detail::kRideNullspaceTol * qp.Ai.row(row).norm() * pn) {
                return false;
            }
        }
        for (Index i = 0; i < qp.n(); ++i) {
            if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree &&
                std::abs(p(i)) > detail::kRideNullspaceTol * pn) {
                return false; // ||e_i|| == 1
            }
        }
        return true;
    }

    // Which sign of `p` the ride takes, or 0 to DECLINE the ride entirely.
    //
    // Two conditions, and section 4c requires BOTH:
    //
    //   DESCENT      the objective's directional derivative along the chosen
    //                sign is STRICTLY negative -- past a tolerance relative to
    //                ||g_x|| * ||p||, so "flat to within rounding" does not
    //                qualify. Combined with non-positive curvature this means
    //                the objective decreases monotonically along the whole
    //                ray, so the step to the blocker strictly improves and the
    //                working set cannot be revisited at the same objective.
    //   FEASIBILITY  the chosen sign moves OFF the constraint the drop rule
    //                just released, into its feasible side. Its slack is
    //                exactly zero, so the other sign is answered by the ratio
    //                test with alpha = 0 and an immediate re-add -- the
    //                pre-ride cycle this section exists to break.
    //
    // At a KKT point of the PRE-drop working set the two provably agree: with
    // g_x = -A_w' lambda there, the derivative along a direction q with
    // a_c . q = -1 and A_w' q = 0 is exactly lambda_c, which the drop rule made
    // negative. They CAN disagree on a DEGENERATE working set (more active
    // constraints than dimensions, which the header's Degeneracy note already
    // allows): the multipliers merely split among dependent rows, so
    // "lambda_c < 0" stops implying that moving off c improves anything.
    //
    // A disagreement DECLINES: the iteration falls through to the ordinary
    // EQP step, i.e. to exactly the behavior this engine had before the ride
    // existed. Declining is never worse than the baseline, where riding on a
    // wrong sign demonstrably is.
    //
    // A FLAT DIRECTION ALSO DECLINES, and this is load-bearing rather than
    // fastidious: zero curvature plus zero slope is not an ascent, but it is
    // not PROGRESS either, and riding it is strictly worse than not -- the
    // step buys no objective decrease at all while still pinning a blocker
    // into the working set, which can turn a legitimate small row residual
    // into a spurious kInfeasible at step 5's classifier. It would also
    // contradict section 4c's own anti-cycling argument, which rests on the
    // decrease being STRICT. Declining a flat direction can cost decisiveness
    // on some problems (a solve that would otherwise have terminated does
    // not); it is chosen anyway because riding one can cost correctness.
    //
    // Both tests use tolerances relative to the product of the norms involved,
    // since both quantities are inner products whose achievable accuracy
    // scales that way.
    int ride_sign(const QpProblem &qp, const DropRecord &dropped, const Vec &p,
                  const Vec &x) const {
        const Vec gx = qp.H.selfadjointView<Eigen::Upper>() * x + qp.g;
        const double slope = gx.dot(p);
        const double slope_tol = detail::kEngineDenomTol * gx.norm() * p.norm();
        const double leaves = -dropped_directional(qp, dropped, p);
        const double leaves_tol =
            detail::kEngineDenomTol * dropped_gradient_norm(qp, dropped) * p.norm();

        // DESCENT picks the sign, and only strict descent qualifies -- a flat
        // direction has no descending sign and declines.
        int sign = 0;
        if (slope < -slope_tol) {
            sign = 1;
        } else if (slope > slope_tol) {
            sign = -1;
        }
        // FEASIBILITY vetoes it: the descending sign must not step back into
        // the constraint the drop rule just released.
        if (sign == 0 || static_cast<double>(sign) * leaves < -leaves_tol) {
            return 0; // no admissible sign: decline
        }
        return sign;
    }

    // 2-norm of the dropped constraint's gradient, for ride_sign's tolerance.
    static double dropped_gradient_norm(const QpProblem &qp, const DropRecord &dropped) {
        return dropped.is_ineq ? qp.Ai.row(dropped.idx).norm() : 1.0; // ||e_i|| == 1
    }

    // Section 4c's ride. `p` is the post-drop EQP step, already measured to
    // have non-positive curvature, so the QP is unbounded below along one of
    // +/-p inside the current working set. Steps to the nearest blocker along
    // the admissible sign and puts that blocker into `ws`. `ws` and `x` are
    // left untouched on kUnbounded and kDeclined.
    RideOutcome ride_negative_curvature(const QpProblem &qp, const DropRecord &dropped,
                                        QpCounters &counters, WalkSeen &seen, const Vec &p,
                                        const Vec &Aix, WorkingSet &ws, Vec &x) const {
        if (!ride_stays_in_working_set(qp, ws, p)) {
            return RideOutcome::kDeclined;
        }
        const int sign = ride_sign(qp, dropped, p, x);
        if (sign == 0) {
            return RideOutcome::kDeclined;
        }
        const Vec dir = (sign < 0) ? Vec(-p) : p;

        BlockKind kind = BlockKind::kNone;
        Index block_idx = -1;
        BoundState block_state = BoundState::kFree;
        const double alpha = ratio_test(qp, ws, x, Aix, dir, kind, block_idx, block_state,
                                        /*cap_at_unit=*/false, counters);
        if (kind == BlockKind::kNone) {
            return RideOutcome::kUnbounded;
        }

        x += alpha * dir;
        ++counters.ws_adds; // observation only -- see QpCounters
        if (kind == BlockKind::kBound) {
            ++counters.ws_adds_bound;
            mark_bound_seen(seen, counters, block_idx);
        } else {
            mark_ineq_seen(seen, counters, block_idx);
        }
        if (kind == BlockKind::kIneq) {
            ws.add_ineq(block_idx);
        } else {
            ws.bound_state()[static_cast<std::size_t>(block_idx)] = block_state;
            x(block_idx) =
                (block_state == BoundState::kAtUpper) ? qp.upper(block_idx) : qp.lower(block_idx);
        }
        return RideOutcome::kStepped;
    }

    // Step 3. Reports the blocking constraint and the step to it.
    //
    // `cap_at_unit` is what distinguishes an ordinary EQP step from a ride
    // (section 4c). With it set -- every call from the main loop's step path --
    // alpha is clamped to [0, 1], because 1 lands on the EQP's own solution and
    // there is no reason to go past a minimizer; a constraint sitting beyond
    // that is not blocking anything and is reported kNone. A ride has no such
    // landing point (the objective decreases without bound along the
    // direction), so it clears the flag and takes the raw ratio; alpha is then
    // finite exactly when a blocker was found.
    //
    // `tied` counts, EXACTLY, how many candidates share the final minimum
    // ratio: resetting it on every strict decrease of `best` and incrementing
    // it on every exact equality leaves it equal to the true multiplicity of
    // the final minimum, with no second pass and no tolerance (a candidate
    // equal to the final best but seen before the last strict decrease is
    // impossible -- it would have set `best` to that value earlier, making
    // the later decrease non-strict). SELECTION IS UNTOUCHED by this --
    // `best`, `kind`, `block_idx` and `block_state` are assigned by exactly
    // the same comparisons as before.
    static double ratio_test(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                             const Vec &Aix, const Vec &p, BlockKind &kind, Index &block_idx,
                             BoundState &block_state, bool cap_at_unit, QpCounters &counters) {
        double best = std::numeric_limits<double>::infinity();
        kind = BlockKind::kNone;
        Index tied = 0;

        if (qp.mi() > 0) {
            const Vec Aip = qp.Ai * p;
            for (Index j = 0; j < qp.mi(); ++j) {
                if (in_working(ws, j) || Aip(j) <= detail::kEngineDenomTol) {
                    continue;
                }
                // No shift term is needed here: refresh_shifts() puts every
                // row carrying a positive shift INTO the working set, and
                // working rows are skipped above, so a row reaching this line
                // always has shift == 0. Were that invariant ever broken the
                // omission is the safe direction -- slack clamps to 0, the
                // row blocks at alpha = 0 and is added, which restores it.
                const double slack = std::max(0.0, qp.bi(j) - Aix(j));
                const double ratio = slack / Aip(j);
                if (ratio < best) {
                    best = ratio;
                    kind = BlockKind::kIneq;
                    block_idx = j;
                    tied = 1; // observation only -- see this function's note
                } else if (ratio == best) {
                    ++tied; // observation only
                }
            }
        }
        for (Index i = 0; i < qp.n(); ++i) {
            if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
                continue;
            }
            double ratio = std::numeric_limits<double>::infinity();
            BoundState hit = BoundState::kFree;
            if (p(i) > detail::kEngineDenomTol && qp.upper(i) < detail::kEngineInfBound) {
                ratio = std::max(0.0, qp.upper(i) - x(i)) / p(i);
                hit = BoundState::kAtUpper;
            } else if (p(i) < -detail::kEngineDenomTol && qp.lower(i) > -detail::kEngineInfBound) {
                ratio = std::min(0.0, qp.lower(i) - x(i)) / p(i);
                hit = BoundState::kAtLower;
            }
            if (ratio < best) {
                best = ratio;
                kind = BlockKind::kBound;
                block_idx = i;
                block_state = hit;
                tied = 1; // observation only
            } else if (hit != BoundState::kFree && ratio == best) {
                ++tied; // observation only
            }
        }
        if (kind != BlockKind::kNone && tied > 1) {
            ++counters.ratio_ties; // observation only -- see QpCounters
        }

        if (!cap_at_unit) {
            return best; // infinite exactly when kind == kNone
        }
        if (kind != BlockKind::kNone && best > 1.0 + detail::kEngineStepTieTol) {
            kind = BlockKind::kNone; // the full step is interior: nothing joins
        }
        return std::min(1.0, best);
    }

    // const: nothing may mutate opts_ after construction; every field it
    // holds is this engine instance's DEFAULT for that field, resolved
    // against a solve()'s own SolveOverrides at the top of run() (see
    // `eff_opts` there) rather than read directly by most of the file below --
    // see the header contract's PER-SOLVE RADIUS VARIATION note.
    // border_effective_delta_/border_effective_mu_ below are what let the
    // HOT-START REUSE fingerprints track a solve's EFFECTIVE (primal_delta,
    // dual_mu) pair (condition (d)) even though opts_ itself stays const.
    const QpOptions opts_;
    Ledger *ledger_ = nullptr;
    std::string label_prefix_;
    mutable Index solve_counter_ = 0;

    // HOT-START REUSE state (border mode only; see the header contract's
    // HOT-START REUSE note). border_ persists across solve() calls -- it is
    // constructed once, in this instance's constructor, not per-solve. The
    // rest track the fingerprint of the problem and the exit working set
    // border_ was last left representing, and whether that snapshot is
    // still trustworthy (border_valid_, cleared pessimistically at the top
    // of every run() and only re-armed on a clean kOptimal exit -- see
    // INVALIDATION POLICY). Mutable because solve() is logically const (a
    // pure function of qp and seed) even though the engine instance caches
    // state between calls.
    //
    // Held through a std::shared_ptr, not by value, so that
    // hot_state() can hand a COPY of this same pointer to a different
    // QpEngine instance (see HotState's OWNERSHIP note, above BorderState's
    // definition) without ever moving or copying the BorderState object
    // itself -- both of which BorderState's own copy/move-deleted design
    // already forbids, for the `schur`-references-`kkt` reason documented
    // there. Never null: constructed once above and only ever REASSIGNED
    // (never reset to null) by run()'s ADOPT AN EXTERNAL HOT HANDLE step.
    mutable std::shared_ptr<BorderState> border_;
    mutable bool border_valid_ = false;
    mutable std::uint64_t border_structural_hash_ = 0;
    mutable std::uint64_t border_values_hash_ = 0;
    // Condition (d) of the HOT-START REUSE note: the EFFECTIVE (primal_delta,
    // dual_mu) pair the last trustworthy solve resolved to (SolveOverrides
    // resolved against opts_ -- see `eff_opts` in run()). Committed alongside
    // the two hashes above, on the same clean-kOptimal-exit schedule, and
    // compared against the NEXT solve's own resolved pair before reuse is
    // granted -- see reuse_eligible in run(). tr_radius has no counterpart
    // here: it never joins this key at all (see the same note).
    mutable double border_effective_delta_ = 0.0;
    mutable double border_effective_mu_ = 0.0;
    mutable std::vector<BoundState> border_exit_bound_state_;
    mutable std::vector<Index> border_exit_active_ineq_;
    // Condition (e) (HotState's OWNERSHIP note has the full argument): this
    // engine's own last-trusted (session_id, epoch) pair of whatever object
    // `border_` currently names. Compared against that object's LIVE
    // `kkt.factor.session_id()`/`epoch()` -- not against another frozen
    // copy -- at the top of every run() call, which is what detects a
    // DIFFERENT engine (reached via a HotState hand-off) having rebuilt the
    // shared object in the meantime, even when every other fingerprint
    // above still matches this solve's own problem. Joined there by the
    // usable-numerics conjunct, which covers the failed-rebuild case no
    // epoch advance records.
    mutable std::uint64_t border_kkt_session_id_ = 0;
    mutable std::uint64_t border_kkt_epoch_ = 0;
};

} // namespace hven::solvers
