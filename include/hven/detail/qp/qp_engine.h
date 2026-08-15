#pragma once

// qp_engine.h — the primal active-set loop for the QP
//
//     min   g^T x + 1/2 x^T H x
//     s.t.  Ae x  = be,   Ai x <= bi,   l <= x <= u
//
// H is not required to be positive semidefinite. Sections 4b and 4c below are
// what an INDEFINITE H needs on top of the ordinary convex loop: 4b keeps a
// second-order-inconsistent START from being certified optimal, 4c keeps a
// DROP that exposes negative curvature from solving an unbounded EQP. On a
// convex H both are provably inert and neither costs a factorization.
//
// Everything numerical lives in the components below this file: the working
// set (working_set.h), the regularized bound-eliminated KKT assembly
// (kkt_assembly.h), the sparse factorization (hven::linear::SymmetricFactor
// through kkt_calls.h's KktFactor helper) and the
// equality-QP solve with iterative refinement (eqp_solve.h). What follows is
// only the loop that walks between working sets.
//
// --- Loop contract ---
//
// 0. CROSSED BOUNDS. Before anything else runs, every i is checked for
//    lower(i) > upper(i) + feas_tol -- a box that is empty regardless of
//    H/g/Ae/Ai, since no x can sit in [lower(i), upper(i)] at all. This is
//    detected HERE, in the engine, rather than thrown by QpProblem::validate():
//    a caller assembling a child QP at runtime -- Phase 3's trust-region box
//    intersected with the original variable bounds is the motivating case --
//    can legitimately hand this engine an empty box, and that is a normal
//    runtime outcome for the loop to report, not a modeling error to reject
//    before the loop is even entered. The verdict is kInfeasible immediately,
//    with x set to the clamped start point (step 1's clamp naturally
//    collapses to upper(i) when lower(i) > upper(i)) and all multipliers
//    zero, exactly as for any other kInfeasible exit.
//
// 1. START POINT. Cold start uses x = clamp(0, l, u); warm start uses
//    clamp(seed.x, l, u) plus seed.bound_state / seed.ineq_active as the
//    initial working set. Variables with l(i) == u(i) are marked kFixed and
//    never leave the working set. Variables merely SITTING at a bound are
//    NOT pinned at start: the ratio test pins them the first time they
//    actually block, which keeps the initial reduced KKT system well posed
//    even when many bounds are touched at once.
//
//    General inequalities violated at the start point enter via a
//    SHIFTED-CONSTRAINT HOMOTOPY rather than a separate phase-1 LP. Define
//        shift(j) = max(0, Ai_j x - bi_j)
//    so x is feasible for the shifted rhs bi + shift, and put every row with
//    shift(j) > 0 into the working set. The EQP is always solved against the
//    TRUE rhs bi, so a shifted working row is pulled from bi_j + shift(j)
//    toward bi_j and its shift decays by exactly the step fraction taken.
//    The shifts are recomputed from x after every step (drift-correction
//    style) rather than propagated, so rounding cannot let them go stale, and
//    they are clamped to zero below a SCALE-AWARE tolerance (see
//    refresh_shifts: an absolute feas_tol is not achievable on a badly scaled
//    row, nor below the regularization's own dual_mu*|lambda| footprint). A
//    working row whose shift is still positive is exempt from the drop rule
//    in step 2 — it is being driven to feasibility, so the sign of its
//    multiplier says nothing yet.
//
// 2. EQP CANDIDATE + DROP RULE. Each iteration calls solve_eqp() on the
//    current working set, giving the candidate x* and the multipliers
//    lambda_e / lambda_w. If the step p = x* - x is negligible, the point is
//    a KKT point of the current working set and the multipliers decide:
//      - a working inequality row needs lambda_i >= -opt_tol;
//      - a variable at its lower bound needs z >= -opt_tol, at its upper
//        bound z <= opt_tol, where z is priced from the stationarity residual
//        per eqp_solve.h:  z = (Hx + g + Ae^T lambda_e + Ai^T lambda_i)(i);
//      - kFixed variables are not sign-constrained.
//    No violation => kOptimal. Otherwise the MOST NEGATIVE multiplier leaves
//    the working set (Dantzig rule); exact ties are broken by the largest
//    "angle", i.e. the largest violation scaled by the constraint gradient's
//    2-norm, then by lowest index for determinism.
//
//    STATIONARITY CAVEAT AT A TR-PINNED INDEX (section 6). The z above is
//    priced and consulted internally exactly as for a real bound, for every
//    index -- TR-pinned or not. But the z REPORTED in QpSolution is forced to
//    0 at a TR-pinned index (section 6), so at such an index the reported
//    quantities do not satisfy stationarity: grad(f) + Ae^T lambda_e +
//    Ai^T lambda_i - z_reported != 0 in general, because the unexposed TR
//    dual is exactly what absorbed that residual internally. A kFree entry
//    at a TR-pinned index is therefore NOT a witness that the point is
//    stationary in that coordinate treated as free -- it is a witness that
//    the coordinate is unconstrained by any REAL bound. A caller must read
//    tr_active, not z or bound_state, to know whether a coordinate is
//    TR-constrained.
//
// 3. RATIO TEST. If p is not negligible, step along it and stop at the first
//    non-working inequality or bound that blocks:
//    alpha = min(1, min_j ratio_j). The blocking constraint joins the working
//    set. A constraint whose ratio lands at 1 to within kStepTieTol counts as
//    blocking too, so a full step that happens to land exactly on a
//    constraint labels it active — this reproduces the dense oracle's
//    tie-break toward MORE active constraints at a degenerate vertex (e.g.
//    simple_box_qp, whose optimum sits on a bound carrying a zero
//    multiplier).
//
//    KNOWN LABELING DIVERGENCE. The tie-break above only fires for a
//    constraint the step actually travels toward. A variable that is ALREADY
//    sitting exactly on its bound and has p(i) == 0 is never pinned, because
//    it never blocks anything: the ratio test only considers directions with
//    |p(i)| > kEngineDenomTol. Such a variable is reported kFree with z == 0,
//    where the dense oracle — which tie-breaks toward more active
//    constraints — reports it kAtLower/kAtUpper, also with a zero multiplier.
//    Probe: min 1/2||x - (1,0)||^2 with x0 <= 1, x1 >= 0 solves to (1, 0) and
//    gives engine [kAtUpper, kFree] against oracle [kAtUpper, kAtLower]. The
//    two agree on x0 (the step travels to that bound, so the step-3 tie-break
//    pins it) and differ only on x1, which starts at its lower bound and has
//    p(1) == 0 on every iteration, so nothing ever blocks there. Both label a
//    genuine KKT point and both carry a zero multiplier, so x, the objective
//    and the duals all agree; only the ACTIVE-SET LABEL differs. Documented
//    rather than "fixed" because pinning zero-step variables would enlarge
//    the working set on every iteration for no numerical gain. Callers that
//    need activity by geometry rather than by working-set membership should
//    test the residual against their own tolerance.
//
// 4. WORKING-SET UPDATE / COUNTERS. Under
//    QpOptions::ws_algebra == kRefactorize (selectable, and the equivalence
//    oracle every border-mode battery is checked against -- see below), every
//    working-set change is followed by a fresh assemble_kkt() +
//    factorize_checked() on the next iteration (solve_eqp does both),
//    counted in QpCounters::factorizations.
//
//    BORDER MODE (ws_algebra == kSchurBorder, the DEFAULT) keeps the loop
//    above letter-for-letter and swaps only the linear algebra underneath it. One
//    K0 (assemble_kkt_full, spanning ALL n variables) is assembled and
//    factorized from the SEED working set -- cold or warm, the difference is
//    only which working set seeds it -- and every subsequent working-set
//    change becomes a GMSW border over that fixed factorization
//    (border_ops.h / schur_complement.h) rather than a refactorization:
//      - a variable pinned at a bound     -> pin_variable border
//        (variables already pinned at seed time included: assemble_kkt_full
//        eliminates nothing, so their pins are borders added immediately
//        after the factorization, not part of K0)
//      - a variable freed                 -> drop that pin border
//      - an inequality row activated      -> add_ineq_row border, unless K0
//        already owns the row, in which case its delete border (if any) is
//        dropped instead
//      - a working row deactivated        -> delete_k0_row border if K0 owns
//        the row, otherwise drop its add_ineq_row border
//    Each such border operation increments QpCounters::schur_updates, and the
//    EQP is solved through solve_bordered_eqp (bordered_eqp.h). K0 is
//    re-assembled and re-factorized -- clearing the border stack, and counted
//    in QpCounters::factorizations -- when the Schur complement can no longer
//    be trusted: SchurComplement::needs_refactorization() (past schur_cap,
//    past schur_cond_max, a numerically or exactly singular factor) or a K0
//    factorization Pardiso had to perturb pivots in.
//
//    NOT EVERY UNTRUSTWORTHY BORDER STACK IS FIXABLE BY REBUILDING, so
//    factorizations does NOT count K0 factorizations only. A rebuild folds
//    the working ROWS back into K0, but a PIN can never be folded in --
//    assemble_kkt_full eliminates nothing, so a pinned variable is a border
//    against every possible K0. Once the live pin count alone passes
//    schur_cap, needs_refactorization() is therefore permanently true and
//    rebuilding is a no-op that merely re-adds every pin: quadratic in n on a
//    box-shaped QP (the canonical trust-region case), measured three orders
//    of magnitude slower than refactorize mode. Those iterations -- along
//    with any where a rebuild has already been spent, or where the rebuild
//    did not clear the flag, or where the bordered solve throws on a singular
//    factor -- are served from the ELIMINATION path (solve_eqp on the current
//    working set), where pins are eliminable by construction. That costs a
//    factorization, counted like any other.
//
//    When the cause is that pins-only dead end specifically, the engine
//    LATCHES: it stops maintaining the border stack altogether rather than
//    keeping it in sync for a solve that will not read it. Syncing anyway is
//    not free bookkeeping -- every add_border costs a K0 solve plus an
//    O(dim^2 * n0 + dim^3) dense rebuild of C, and letting dim() run past
//    schur_cap that way was the dominant remaining cost on the box shape. The
//    latch releases on EXACTLY ONE condition -- the pin count falling back to
//    schur_cap or below, i.e. bordering becoming possible again -- at which
//    point one rebuild_k0 from the current working set restarts normal border
//    operation; the stale stack is discarded rather than repaired, since it
//    was never kept current. A change in the working ROWS does NOT release it:
//    that condition existed through Phase 4 and was measured (Phase-5 Task 4)
//    to thrash the latch once per row change on any working set whose rows
//    move, for rebuilds that are futile by construction. See
//    latch_still_holds for the measurement and for why the pin-count rule
//    cannot oscillate.
//
//    The two modes are held to be OBSERVATIONALLY EQUIVALENT -- same status,
//    same active set, same x and multipliers -- by
//    QpEngineBorder.BorderModeMatchesRefactorizeMode, which runs this file's
//    entire fixture battery through both. The refactorize path is the oracle;
//    a divergence is a border-path bug.
//
//    THAT CLAIM IS SCOPED TO CONVEX H, which is what this whole file's
//    battery covers. The two formulations are NOT equivalent for an
//    indefinite Hessian: the bound-eliminated K and the full-variable K0
//    generally have different inertia (elimination removes rows that carry
//    the negative curvature), so the two modes can legitimately reach
//    different working sets and report different statuses -- measured at
//    88 status mismatches in 4000 adversarial indefinite-H cases. That is a
//    property of the mathematics, not a bug in either path, and it is why
//    indefinite-H correctness is validated against the LOCAL-MINIMIZER
//    oracle (Task 8) rather than by cross-mode equality.
//
//    COUNTER SEMANTICS (relied on downstream, e.g. Task 10's warm-start
//    assertions): QpCounters::minor_iters is incremented exactly ONCE per
//    major iteration of the loop below — one EQP solve plus either a drop or
//    a ratio-test step — so it counts major iterations, and a run that stops
//    at opts.max_iter reports minor_iters == max_iter.
//    Under kRefactorize, QpCounters::factorizations counts solve_eqp calls,
//    which is one per major iteration EXCEPT when the reduced system is empty
//    (every variable pinned, no equalities, no working rows), a case
//    short-circuited without touching Pardiso; QpCounters::schur_updates
//    stays 0. Under kSchurBorder, factorizations counts K0 factorizations
//    PLUS the elimination-path fallbacks described above -- one in total for
//    a solve that trips nothing, but note a SINGLE iteration can spend two
//    (a K0 rebuild that is then found not to have helped, followed by the
//    fallback solve_eqp), so it is not bounded by one per iteration.
//    schur_updates counts individual SchurComplement
//    add_border/drop_border calls -- INCLUDING the re-adds that follow a
//    rebuild, since those are work actually done. A rebuild's wholesale
//    clearing of the stack is NOT counted, so the counter is deliberately
//    asymmetric: it measures border work performed, not net stack size.
//    THE TEMPORARY-VERTEX REPAIR (section 4b) shows up in all three
//    counters, and only ever on an indefinite H: the iteration whose gate
//    came back kWrong is counted like any other and then RETRIED from the
//    repaired vertex, so a repaired solve spends one extra minor_iter; and
//    each pin/release the repair probes re-runs this same dispatch, which
//    under kSchurBorder costs schur_updates only (K0 is untouched by a pin)
//    but under kRefactorize costs one factorization per probe. On a convex H
//    the gate cannot come back kWrong, so none of that is reachable and every
//    counter is bit-identical to what it was before the gate existed.
//    SECTION 4c's RIDE is likewise invisible to all three counters: it neither
//    factorizes nor borders anything of its own (it reuses the EQP result the
//    iteration already computed and the ratio test the iteration would have
//    run anyway), and it consumes exactly the one major iteration it fires on,
//    so a ride costs one minor_iter like any other step. What it changes is
//    how MANY iterations a solve spends -- an indefinite solve that used to
//    cycle to max_iter now terminates -- never the accounting per iteration.
//
//    THE TWO REFINEMENT COUNTERS (QpCounters::border_refine_steps /
//    eqp_refine_steps) are accumulated at
//    the same two EQP call sites as factorizations: every solve_bordered_eqp
//    call adds its TOTAL kept steps (>= 1) to border_refine_steps, every
//    solve_eqp call adds its EXTRA kept steps (identically 0 -- the eliminated
//    path has no iterated loop) to eqp_refine_steps -- see types.h for why the
//    two baselines differ and why the zero one is still reported. A
//    border-mode solve that falls back to the elimination path therefore
//    contributes to both, exactly as it already does to factorizations.
//
//    Note the empty-system short-circuit has no border-mode counterpart and
//    needs none -- K0 spans all n variables
//    unconditionally, so "every variable pinned" is simply n pin borders over
//    an ordinary factorization.
//
//    HOT-START REUSE (border mode only). border_ (the BorderState below) is
//    an ENGINE-INSTANCE member, not a per-solve local -- it persists across
//    solve() calls on the same QpEngine, and a warm re-solve can therefore
//    skip K0's assembly and factorization when FIVE conditions all hold at
//    the SEED working set (i.e. checked once, before the loop below runs its
//    first iteration):
//      (a)/(c) H/Ae/Ai's structural pattern AND their VALUES are exactly
//          what they were on the previous solve() call that left border_ in
//          a trustworthy state (detail::structural_hash /
//          detail::values_hash, both FNV-1a fingerprints of the raw problem
//          matrices -- NOT of an assembled K0, so this check itself never
//          assembles or factorizes anything). K0's VALUES depend on H, Ae,
//          Ai and the EFFECTIVE primal_delta/dual_mu this solve resolved
//          (see assemble_kkt_core, and the PHASE-3 DESIGN FRICTION note
//          below for how that pair is resolved) -- never on g, be, or bi,
//          so a warm re-solve that only perturbs g/b is exactly the case
//          this exists for.
//
//          These are TWO SEPARATE hashes, not one: values_hash alone is not
//          enough to detect a structural change, because it hashes only the
//          value bytes, and two structurally different matrices can produce
//          byte-identical value streams -- e.g. mi=2 with rows [1,0],[-1,0]
//          (values [1,-1], one nonzero per row) versus mi=1 with the single
//          row [1,-1] (values [1,-1], both nonzero) hash identically under
//          values_hash despite `mi` itself differing. structural_hash (which
//          mixes rows/nnz/index arrays, not values) is what catches that;
//          see QpWarmStart.StructureChangeAtConstantValueBytesForcesRefactorization.
//      (d) the EFFECTIVE (primal_delta, dual_mu) pair this solve resolved to
//          (SolveOverrides, resolved once at the top of run() -- see the
//          PHASE-3 DESIGN FRICTION note) is byte-identical to the pair the
//          previous trustworthy solve resolved to (border_effective_delta_ /
//          border_effective_mu_ below, committed alongside the two hashes).
//          This is a NECESSARY fourth condition precisely because (a)/(c)
//          above hash only H/Ae/Ai: before per-solve overrides existed,
//          primal_delta/dual_mu were fixed for an engine's whole lifetime
//          (opts_ being const), so they could never differ between two
//          solve() calls on one instance and needed no entry in the key at
//          all. A changed pair changes every value assemble_kkt_core writes
//          onto K0's regularized diagonal without changing a single H/Ae/Ai
//          byte, so omitting it now would silently reuse a K0 built for the
//          WRONG regularization. tr_radius is deliberately not part of this
//          condition, or of the key at all -- see the HOT-START REUSE
//          INTERACTION note below for why bounds (real or TR-derived) never
//          enter K0 in the first place.
//      (b) the seed working set -- ws immediately after start_center() /
//          ingest_seed_working_set() plus
//          the pre-loop refresh_shifts() call, i.e. exactly what the first
//          border_candidate() call will see -- equals the EXIT working set
//          of that same previous solve.
//      (e) FIX ROUND 1 (Phase-4 Task 4; M3 phase B retargeted it onto the
//          factor's identity). border_'s factor's own live (session_id,
//          epoch) pair -- the session id moved by analyze() and the epoch
//          advanced by every successful factorize() inside rebuild_k0(),
//          the sole site that reassigns K0 or refactorizes it, BY WHICHEVER
//          ENGINE INSTANCE calls it -- equals the pair THIS engine last saw
//          as trustworthy (border_kkt_session_id_/border_kkt_epoch_ below,
//          committed alongside the other fingerprints and adopted alongside
//          them from a HotState handle), AND the factor's current numerics
//          are USABLE (`inertia().state == kObserved` -- false precisely
//          when the last factorize failed or none happened, the SS7.2
//          conjunct that covers what generation's bump-before-mutate used
//          to). Conditions (a)-(d) alone describe
//          the PROBLEM being solved and are necessarily silent about WHICH
//          OBJECT border_ currently names: a WarmStart's `hot` handle can be
//          fed to a second QpEngine instance (see HotState below) while the
//          producing engine keeps the object alive and keeps mutating it,
//          and (a)-(d) cannot tell that apart from an untouched object with
//          the identical fingerprint. (e) is what can, by comparing against
//          the object's OWN identity rather than a value copied out of it.
//          READ HOTSTATE'S OWN OWNERSHIP NOTE (below QpEngine, where the
//          adjudication actually lives) BEFORE OVER-CREDITING THIS
//          CONDITION, though: it is DEFENSE-IN-DEPTH, not the mechanism that
//          makes cross-engine sharing safe. That mechanism is DETACH (this
//          section, below): because a shared object can only ever be
//          mutated by an engine whose OWN (a)-(d) already passed, every K0
//          ever written into a shared BorderState already has the VALUES
//          the handle's fingerprint describes, and sync_borders()'s
//          unconditional reconciliation (the note just above) absorbs any
//          difference in which rows happen to be folded into K0 versus
//          carried as borders. So a stale-identity reuse that (e) alone
//          would have blocked is not, on the evidence measured so far, a
//          wrong-answer case -- see HotState's OWNERSHIP note for the full
//          adjudication. (e) still earns its keep as an independent, cheap
//          check that degrades a
//          questionable reuse to kWarm rather than asserting it is fine on
//          an argument this file's own tests do not exercise every corner
//          of; it is kept for that reason, not because reuse would
//          otherwise be unsound.
//
//    WHY THIS IS SAFE EVEN THOUGH (b) IS ONLY APPROXIMATE. It is EXPECTED,
//    but NOT relied on for correctness, that at a genuine kOptimal exit the
//    border stack border_ holds already matches the exit ws exactly: the
//    last sync_borders() call before the loop breaks is usually the one made
//    for that same, final ws. That is not a guarantee, though -- there is a
//    real threshold hole. refresh_shifts() adds a row to ws whenever its
//    violation clears row_tolerance(row_scale, lambda=0), roughly
//    s > feas_tol*row_scale; for a violation landing in (1x, 10x] of that
//    threshold, the row is added to ws but is not yet large enough for
//    anything downstream to object to. That add can happen on the FINAL
//    iteration, strictly AFTER border_candidate()'s last sync_borders() call
//    for that iteration and BEFORE drop_worst() is consulted -- so a
//    genuinely new row can enter ws and the point still be classified
//    kOptimal (drop_worst simply finds nothing TO drop) without border_ ever
//    having synced against it. border_exit_active_ineq_ (captured after the
//    loop, from the final ws) can therefore name a row border_'s actual
//    ledger does not yet carry.
//
//    What actually makes reuse safe despite that hole is NOT an invariant
//    about border_'s contents at exit -- it is that sync_borders() is an
//    UNCONDITIONAL, FULL reconciliation of ws against border_'s ledger, run
//    every time border_candidate() executes, on EVERY iteration of EVERY
//    solve, whether or not the K0 rebuild above it was skipped. The reuse
//    fast path only ever skips rebuild_k0 (assembly + factorization); it
//    NEVER skips sync_borders(). So even when border_exit_active_ineq_
//    understates what ws.active_ineq() turns out to be on the next warm
//    solve's first iteration, that iteration's sync_borders() call adds
//    whatever border is missing (paying schur_updates, not factorizations)
//    before border_solve_or_fall_back() is allowed to solve anything.
//    Condition (a)/(b)/(c)/(d)/(e) reuse eligibility is therefore a claim
//    about K0 alone -- its values are unchanged, so it need not be rebuilt --
//    never a claim that the border STACK is already fully in sync;
//    sync_borders() re-verifies that fresh on every call regardless.
//
//    WARNING: do not "optimize" the reuse fast path further by skipping the
//    first post-reuse sync_borders() call on the theory that a matching seed
//    ws means there is nothing left to reconcile -- the threshold hole above
//    is a standing counterexample, and skipping that reconciliation would
//    turn it into a silent stale-reuse bug: a border-mode solve proceeding
//    against a K0/border stack that does not actually represent its own
//    working set.
//
//    (Latched exits are covered by the same argument: a previous exit left
//    LATCHED -- see the LATCHES note above -- has a border stack
//    deliberately not kept in sync with ws at all, but border_candidate's
//    own latch handling re-detects "nothing changed" and falls back to the
//    elimination path on the next solve, so reuse eligibility still gates
//    safely there too, just without the K0 win.)
//
//    All five conditions are NECESSARY, but even together they are NOT
//    SUFFICIENT for `factorizations == 0` -- they only make skipping
//    rebuild_k0 correct when border_candidate's OWN, pre-existing checks
//    also agree there is nothing to redo. A carried-over perturbed-pivot
//    (or, on Accelerate, natively-observed zero-eigenvalue) count in
//    border_.kkt.factor.inertia() from the previous solve's last
//    factorization, or a border stack already past needs_refactorization()
//    (e.g. over schur_cap), still forces a rebuild regardless of
//    (a)/(b)/(c)/(d)/(e).
//    So reuse eligibility should be read as "K0 need not be rebuilt on
//    fingerprint/working-set grounds alone" -- a necessary precondition for
//    the zero-factorization case, not a guarantee of it. Callers (e.g.
//    Tasks 9/10's warm-start assertions) must not assert
//    `counters.factorizations == 0` unconditionally on a warm re-solve; they
//    should assert it only alongside control of the same conditions this
//    note documents.
//
//    INVALIDATION POLICY. border_'s cache is committed (border_valid_ set)
//    ONLY on a clean kOptimal exit, and is pessimistically cleared at the
//    very top of every solve() call before anything else runs -- including
//    before qp.validate(). Consequences:
//      - kMaxIter, kInfeasible, and kNumericalError exits all leave the cache
//        invalidated. border_ itself may still hold a perfectly good K0
//        factorization (the working set just did not certify as a KKT
//        point), but the NEXT solve() call cannot tell that from a working
//        set that genuinely was never resolved, so it is not trusted either
//        way; the next call reassembles and refactorizes from scratch.
//      - ANY exception thrown by this solve() call -- qp.validate(), a
//        Pardiso failure, anything -- leaves the cache invalidated for
//        exactly the same reason: the pessimistic clear happens before the
//        first opportunity to throw, so there is no window in which a
//        thrown solve leaves border_valid_ true.
//    This is deliberately conservative: it is always SAFE to fall back to a
//    full rebuild, and the fast path exists purely to skip work that is
//    provably redundant, never to skip work whose validity is in question.
//
//    THREAD SAFETY. QpEngine is NOT thread-safe for concurrent solve() calls
//    on the same instance: border_ and the reuse-fingerprint members beside
//    it are mutable state shared across calls despite solve() being const
//    from the caller's point of view. Concurrent solve() calls on one
//    QpEngine race on that state.
//
//    "USE A SEPARATE QpEngine PER THREAD" IS NO LONGER THE WHOLE RULE, as of
//    Phase-4 Task 4's hot-start handle (WarmStart::hot / HotState, this
//    file). Two DIFFERENT QpEngine instances -- even on two different
//    threads, even never sharing so much as a variable of their caller's --
//    can end up sharing the SAME BorderState object (Pardiso pt_ array
//    included) if one adopts a hot handle the other produced. That sharing
//    is a std::shared_ptr, not a lock: nothing here synchronizes access to
//    the object it names. THE CORRECTED RULE:
//      - SEQUENTIAL hand-off across engines/threads (produce a handle,
//        THEN, after that call has returned, feed it to a solve() on a
//        different engine, on the same thread or a different one, with no
//        overlap in time) is exactly what this mechanism is FOR, and is
//        safe: the factor's session/epoch identity (HotState's own note
//        has the full argument) detects a producer that mutated the shared
//        object again before its handle was consumed, and degrades silently
//        to kWarm rather than computing a wrong answer.
//      - CONCURRENT use of a shared BorderState -- two engines calling
//        solve() AT THE SAME TIME, on any thread, while both hold a copy of
//        the same handle's shared_ptr -- is UNDEFINED. The session/epoch
//        identity is ordinary unsynchronized state, not atomic; it detects
//        a STALE handle precisely because engine calls are assumed to
//        happen one at a time, in some order. It provides no ordering, no
//        memory fence and no exclusion, so concurrent calls race on the
//        identity itself, on every other BorderState member, and on the
//        backend session underneath `kkt` -- the ordinary meaning of a data race,
//        with all its usual consequences, not merely a wrong QP answer.
//        "Use a separate QpEngine per thread" remains necessary advice; it
//        is no longer SUFFICIENT once a hot handle can be shared out of an
//        engine that advice was supposed to keep isolated -- a caller
//        introducing hot-start hand-off across threads must additionally
//        ensure no two engines holding a copy of the same handle ever call
//        solve() concurrently.
//
// 4b. INERTIA GATE AND TEMPORARY-VERTEX START REPAIR (indefinite H).
//
//    Everything above assumes only that each EQP solve is a MINIMIZATION over
//    the current working set. That is automatic for a convex H and false in
//    general: with an indefinite H the regularized KKT system is still
//    nonsingular and still hands back an "answer", but that answer can be a
//    saddle point or a maximizer, and the loop above would certify it
//    kOptimal without noticing. H = diag(1, -1), g = 0 on [-1,1]^2 is the
//    smallest case -- the loop's first EQP returns x = (0,0), the step is
//    negligible, nothing may be dropped, and (0,0) is reported optimal at
//    objective 0 when the true minimizers sit at (0, +/-1) with objective
//    -0.5.
//
//    The signature that distinguishes the two is the KKT matrix's INERTIA.
//    For a system whose reduced Hessian is positive definite on the null
//    space of its working constraints, the regularized KKT matrix
//    [H+delta*I  A^T; A  -mu*I] has inertia exactly (#variables,
//    #constraint rows, 0) -- for a convex H that holds unconditionally
//    (H+delta*I is positive definite, so the matrix is quasi-definite,
//    whatever A's rank), which is why this gate is a NO-OP on every convex
//    fixture in this file's battery.
//
//    THE EXPECTATION, per path:
//      - ELIMINATION path (solve_eqp's bound-eliminated K):
//            (n_free, me + n_working, 0).
//      - BORDER path: pardiso reports the inertia of K0 ALONE (spanning
//        [n | me | n_w0]), but the system actually being solved is K0
//        bordered by the live stack, so the expectation is stated for the
//        whole bordered matrix:
//            (n + extra_pos, me + n_w0 + extra_neg, 0)
//        where the STRUCTURAL border contribution is one negative eigenvalue
//        per border (a pin or an added row is one more constraint), except
//        that a kRowDelete border pairs with the K0 row it kills into a
//        2x2 [[-mu, 1], [1, 0]] block contributing one POSITIVE and one
//        negative -- so extra_pos = #kRowDelete and extra_neg = dim() -
//        #kRowDelete. The bordered matrix's ACTUAL inertia is pardiso's K0
//        inertia plus the Schur complement C's own inertia (Haynsworth), and
//        C's negative count is what SchurComplement::expected_neg_eigs_delta()
//        reports -- consulted ONLY after needs_refactorization() has been
//        checked, since it THROWS on a singular C. Equivalently (and this is
//        how border_inertia_verdict is coded) C's contribution is backed out
//        of both sides and pardiso's K0 numbers are compared against the
//        remainder.
//
//    Note what this means for the repair below: pinning a variable in border
//    mode does NOT change K0's factorization, only C's -- which is exactly
//    why the gate must be evaluated on the BORDERED system's inertia rather
//    than on pardiso's K0 numbers against a fixed expectation. A gate on K0
//    alone would be blind to every pin the repair adds.
//
//    PERTURBED PIVOTS ARE NOT A PASS AND NOT A REPAIR TRIGGER. Per
//    docs/notes/2026-07-27-pardiso-inertia-findings.md, pardiso's
//    (n_pos, n_neg) evidence pair may be trusted IF AND ONLY IF
//    its perturbed_pivots count is zero: on an exactly singular matrix it silently
//    fabricates a pivot sign and reports an inertia INDISTINGUISHABLE from
//    the nonsingular case (the findings-doc probe reports (2,1) either way),
//    and it does NOT raise error -4, so exception handling is not a
//    singularity detector. The gate therefore has THREE verdicts, not two
//    (detail::InertiaVerdict):
//      kOk       trustworthy and matching -- proceed;
//      kSuspect  non-kObserved or perturbed evidence, or reported counts
//                that do not
//                even sum to the matrix dimension, or (border path) C is
//                past needs_refactorization() so its inertia cannot be read.
//                The inertia is UNKNOWN. It is never treated as a pass; it is
//                also never treated as evidence that a repair is needed,
//                because acting on it would be acting on a fabricated sign as
//                ground truth -- precisely what the findings doc forbids.
//                Suspect factorizations are already handled by the
//                refactorization machinery in step 4 (border_candidate
//                rebuilds K0 whenever the evidence is untrusted), which is
//                the findings doc's prescribed response.
//      kWrong    trustworthy and DISAGREEING with the expectation -- the
//                working set is second-order inconsistent, and at solve start
//                that triggers the repair below.
//
//    TEMPORARY-VERTEX START REPAIR. On kWrong at the FIRST major iteration
//    (a wrong inertia later in the run is a negative-curvature event mid-solve,
//    which Task 7's ride handles -- with ONE exception, the POST-PROBE RESTART
//    below, which is the dead end the ride is structurally unable to serve),
//    repair_temporary_vertex
//    walks the free variables in order of MOST NEGATIVE H diagonal -- that is
//    where the negative curvature lives -- and pins them one at a time until
//    the gate returns kOk, i.e. until the working set is second-order
//    consistent. Each pin goes in through the ordinary bound machinery
//    (ws.bound_state(), which border mode turns into a BorderOps::pin_variable
//    border via sync_borders, and elimination mode substitutes out), so the
//    repaired state is an ordinary working set that every other part of this
//    file already understands.
//
//    WHICH BOUND a variable is pinned at: the one it is ALREADY sitting on if
//    it sits on one (within feas_tol -- "pin at the current value", the
//    warm-start case), and otherwise the finite bound that lowers the
//    objective more, measured exactly along e_i: df(t) = grad_i * t +
//    H_ii * t^2 / 2 for t = bound - x(i). Ties go to the lower bound for
//    determinism. A variable with no finite bound on either side cannot be
//    pinned at all and is skipped -- there is no vertex coordinate to pin it
//    to.
//
//    THE REPAIR IS ALL-OR-NOTHING, and that matters more than it sounds: it
//    succeeds only if it reaches kOk, and a working set containing ANY
//    unpinnable free variable whose direction carries the negative curvature
//    cannot get there, so the whole repair unwinds. This is not confined to
//    the "no bounds at all" scalar case -- one unbounded variable among
//    otherwise fully boxed ones is enough (measured on a randomized
//    indefinite battery: with one of three variables unbounded, 117 of 241
//    solves reached a second-order-INVALID point; with two of three, 210 of
//    241). Which is why the verdict is also consulted at CLASSIFICATION time
//    below, not only here.
//
//    SECOND-ORDER CERTIFICATION (step 5's classification). kOptimal is a
//    claim about a KKT point, and on an indefinite H a KKT point can be a
//    saddle or a maximizer. So when the loop reaches a point it can neither
//    improve nor drop anything from, and the gate's verdict for the system it
//    just solved is a TRUSTED kWrong, the point is reported
//    kNumericalError rather than kOptimal (multipliers cleared, per that
//    exit's existing semantics). The engine has, in that situation, a
//    trustworthy measurement saying the reduced Hessian is not positive
//    semidefinite there and no remaining move it is willing to make; stamping
//    kOptimal on it would be certifying second-order optimality it has
//    positive evidence against. Only kWrong triggers this -- kSuspect does
//    not, for the same reason it does not trigger the repair. Measured effect
//    on the battery above: second-order-invalid kOptimal exits 117 -> 0 and
//    210 -> 0, with no false positives on the fully boxed battery (where the
//    repair always succeeds and this branch never fires).
//
//    WEAKLY ACTIVE CONSTRAINTS HIDE THE CRITICAL CONE FROM THE GATE, AND THE
//    ZERO-MULTIPLIER PROBE IS WHAT ANSWERS THAT. The gate above tests the null
//    space of the FULL active labeling -- every bound and row the working set
//    currently carries, including one whose multiplier is (numerically) zero.
//    A weakly active constraint is exactly that: on its boundary but
//    contributing nothing to the first-order picture, so the true critical cone
//    at the point is the null space of the working set WITHOUT it, strictly
//    larger than what the gate examines. When the missing negative curvature
//    lives only in the direction that constraint alone excludes, the
//    full-labeling reduced Hessian still reads positive (semi)definite, the
//    gate returns kOk, and -- before Phase 3's Task 1 -- kOptimal was certified
//    at a point that is a STRICT non-minimizer.
//
//    THE PROBE (probe_zero_multiplier_drops), run at the would-be-kOptimal
//    exit and only there, after the infeasibility, runaway and full-labeling
//    inertia checks have all passed. Every working-set member whose multiplier
//    (lambda_w for a row, z for a bound pin) is within opts.opt_tol of zero is
//    a candidate. Each is tentatively dropped -- ONE AT A TIME, most recently
//    added first -- and the gate is re-run on the reduced labeling:
//      kWrong    the hidden negative curvature is real and trustworthily
//                measured. The drop is made REAL, exactly as if the drop rule
//                had released it (including the DropRecord that arms section
//                4c), and the loop RESUMES instead of certifying.
//      kOk       that constraint hides nothing; restore it and try the next.
//      kSuspect  see below. Restore it and try the next.
//    All candidates exhausted without a kWrong => certify kOptimal.
//
//    ADDITION ORDER comes from ProbeState, which re-derives it by rescanning ws
//    once per major iteration rather than by instrumenting the six places that
//    mutate the working set (see there). Most-recently-added-first is a
//    heuristic, not a correctness property: every candidate is tried before the
//    probe gives up, so the order only decides which of several exposable
//    directions is taken first.
//
//    ANTI-CYCLING. A constraint sitting at multiplier zero cannot be told
//    apart, by that value alone, from one this probe just dropped and the
//    ride's blocker immediately re-added -- so without a memory the same drop
//    could be probed and taken forever. ProbeState therefore carries a
//    per-SOLVE exemption set: a constraint whose tentative drop was made real
//    is never probed again for the rest of that solve. Each constraint enters
//    the set at most once, so a solve makes at most n + mi probe-driven drops.
//    The set is per-solve, not per-engine: a warm re-solve starts clean.
//
//    kSUSPECT DURING A PROBE IS TREATED AS kOk, i.e. as permission to certify,
//    and that is the one place this fix stays deliberately silent. The reduced
//    inertia is UNKNOWN there (perturbed pivots, or a border stack past
//    needs_refactorization), and per
//    docs/notes/2026-07-27-pardiso-inertia-findings.md a fabricated pivot sign
//    is never ground truth -- the same policy that keeps kSuspect from
//    triggering the repair or the certification branch above. So a weakly
//    active constraint whose reduced system is unreadable can still carry a
//    false certificate out. It is not silently swallowed in the sense that
//    matters: the loop's own refactorization machinery rebuilds on perturbed
//    pivots (step 4), and -- since Task 3b -- the SUSPECT-STALL GATE below
//    refuses to certify a point that a suspect factorization left
//    non-stationary at all, whether the suspicion came from the full labeling
//    or from a probe's reduced one. What remains uncovered is narrower than it
//    was: a weakly active constraint whose reduced system is unreadable but
//    whose point IS stationary still certifies, which is a second-order claim
//    made on unreadable evidence, not a first-order one made against readable
//    evidence.
//
//    THE SUSPECT-STALL GATE (Task 3b) IS WHAT CLOSES THE HOLE THE PARAGRAPH
//    ABOVE LEAVES OPEN, and it is the reason that paragraph's "deliberately
//    silent" is no longer the last word. Audit finding D9
//    (docs/notes/2026-07-29-accelerate-audit-results.md) showed the failure
//    the refactorization machinery was assumed to cover: rebuilding K0 "while
//    the evidence reports perturbed pivots" can make ZERO PROGRESS -- the rebuild
//    re-derives the same exactly-singular K0 -- and the loop then reaches a
//    negligible-step exit and certifies kOptimal at a point that is not a KKT
//    point at all. So a persistent kSuspect is a signal, but it was a signal
//    nothing acted on at the certification gate.
//
//      (ii) THE INVARIANT. When the verdict for the system the FINAL iterate
//           was solved from is kSuspect, kOptimal may be certified only after
//           an explicit free-block stationarity check on the QP MODEL:
//           r = H x + g + Ae^T lambda_e + Ai^T lambda_i, restricted to the
//           FREE variables (z is identically zero there by construction, so
//           the working set enters exactly through which coordinates are
//           examined), against opt_tol SCALED by the largest term in r --
//           max(1, ||Hx||inf, ||g||inf, ||Ae^T le||inf, ||Ai^T li||inf).
//           That scaling is a DEPARTURE from this file's other opt_tol tests,
//           which are absolute (the drop rule, the zero-multiplier probe's
//           candidate test); feas_tol is the tolerance this engine scales,
//           opt_tol is not. It is deliberate and it is argued rather than
//           assumed -- see free_block_stationarity's own comment for why a
//           gradient residual needs a relative test where a multiplier does
//           not, and why the looser direction is the safe one here. The test
//           is also NaN-aware in both halves (accumulation and comparison):
//           a NaN residual routes to escalation/refusal, never to kOptimal.
//           It runs AFTER the zero-multiplier probe, not instead of it: the
//           probe answers "is the labeling hiding curvature", this answers
//           "did the linear algebra that produced this point actually solve
//           anything", and a point can fail either independently.
//
//           CAVEAT, KNOWN AND UNMEASURED (reviewer finding, Task 3b fix round
//           1): r is built from the multipliers `price` computed at the TOP of
//           this iteration, but the refresh_shifts() call between there and
//           here can ADD inequality rows to the working set. A row added that
//           way carries lambda_i == 0, so its Ai^T lambda_i contribution is
//           absent from r, which can INFLATE the residual on an exit that is
//           in fact stationary under the current labeling -- i.e. a spurious
//           escalation, bounded by the ladder and ending in a recoverable
//           kNumericalError rather than a wrong answer. This is the same
//           family as the ORDERING CAVEAT on `verdict` documented below, and
//           the fix is the same shape (re-price against the current working
//           set before the check). NOT taken here: no fixture in the suite
//           exhibits it, the gate is only reachable on a kSuspect exit at all,
//           and adding a re-price on that path unmeasured would trade a known
//           narrow risk for an unknown one. Measure first.
//
//      (i)  THE POLICY. A failed check means the suspect loop has stalled, so
//           the response is to change the system rather than to keep
//           re-solving it: primal_delta is escalated one decade
//           (detail::kSuspectDeltaFactor) and the iteration resumes with the
//           border state discarded, so K0 is rebuilt at the new
//           regularization -- an exact cancellation h_ii == -primal_delta
//           cannot survive a decade bump. The ladder is bounded
//           (detail::kMaxSuspectEscalations rungs, counted in
//           QpCounters::suspect_escalations); when it is exhausted the solve
//           reports kNumericalError. NEVER kOptimal off a stalled suspect
//           loop, and never a hang -- the ladder is finite and max_iter still
//           bounds the loop.
//
//    WHY THE GATE IS A GATE AND THE ESCALATION IS NOT. A persistent kSuspect
//    that certifies a genuinely stationary point is COMMON and correct: in
//    border mode K0 spans all n variables, so a singular direction that the
//    border stack (a pin) constrains leaves K0 perturbed forever while every
//    bordered solve is exact. The MKL arm of
//    QpEngineIndefinite.InertiaGateRefusesPerturbedFactorization is exactly
//    that case. Escalating there would trade a correct answer for a coarser
//    one, which is why the stationarity check -- not the kSuspect verdict, and
//    not "the working set did not change" -- is what arms the escalation.
//
//    MEASURED, BOTH BACKENDS' COSTUMES OF D9. On Accelerate the singular
//    direction's solve is subspace-clean, the iterate never moves, and the
//    loop certifies at the start point. On MKL the perturbed pivot emits
//    garbage-but-nonzero values, which USUALLY kick the iterate onto a bound
//    and rescue it by accident (that is the MKL arm above) -- but the rescue
//    is only an accident: with primal_delta = 1e-10, h11 = -1e-10 and a box
//    wide enough that the garbage is neither clipped by the ratio test nor
//    caught by is_runaway, MKL certifies kOptimal at x1 = 4.04e8 with a
//    free-block reduced gradient of 1.04. That fixture
//    (SuspectStallIsNotCertifiedOptimal) is the MKL-reachable pin on this
//    gate; before it, both backends had the same defect in different clothes.
//
//    ONE AT A TIME IS THE KNOWN REMAINING APPROXIMATION. A critical cone that
//    opens only when TWO OR MORE weakly active constraints are dropped
//    SIMULTANEOUSLY is not covered: each single drop can read kOk while the
//    pair reads kWrong. Probing combinations is exponential in the number of
//    weakly active constraints and is not done. What checks the residual is the
//    local-minimizer oracle battery
//    (QpEngineIndefinite.EngineLandsOnALocalMinimizer over eight hand-built
//    fixtures, plus RandomizedIndefiniteBatteryLandsOnLocalMinimizers), which
//    verifies every kOptimal answer against an independently enumerated set of
//    local minimizers rather than against this machinery's own reasoning.
//    Measured across Task 1: both batteries unchanged (the randomized one at
//    0 of 50 blocks non-kOptimal and 0 invalid kOptimal exits, before and
//    after).
//
//    WHY THE PROBE ALONE REMOVED THE FALSE CERTIFICATE BUT NOT THE WRONG
//    POINT. Section 4c's ride cannot be armed off a zero-multiplier drop: its
//    arming direction is the post-drop EQP step p, and 4c's own formula
//    p = (-lambda_c / sigma) * q makes p identically ZERO when lambda_c is
//    zero -- which is the defining property of the constraints this probe
//    drops. So the iteration after a probe-driven drop typically sees a
//    negligible p, takes the loop's KKT-point branch, and lands back in this
//    classification with the REDUCED set's trusted kWrong. Measured on the
//    pinned fixture: |p|_inf = 4.4e-16 (border) / 6.7e-16 (refactorize)
//    against a step_tol of 2e-9. Through Task 1 the two pinned cases -- the
//    weakly active BOUND (WeaklyActiveBoundIsNotCertifiedOptimal) and the same
//    geometry through a general ROW (WeaklyActiveGeneralRowIsNotCertifiedEither)
//    -- therefore went from kOptimal at the saddle to kNumericalError at the
//    saddle: an honest refusal at a point 1.5 units from the answer.
//
//    THE POST-PROBE RESTART (Task 6b) IS WHAT COLLECTS IT, and it is a
//    ONE-SHOT lift of the iter == 0 gate rather than new machinery. Task 1's
//    review measured that feeding that kNumericalError result straight back in
//    as a WARM SEED recovers the minimizer -- both fixtures, both algebra
//    modes, kOptimal at (0, -2, 0.5), objective -0.375 -- and identified the
//    mechanism as the temporary-vertex START repair, which fires on the fresh
//    solve but never on the iteration a probe's drop resumes into. So the
//    engine now spends that repair itself, at exactly one place:
//
//      TRIGGER   the certification branch's TRUSTED kWrong arm, i.e. a KKT
//                point of the post-drop working set that the loop can neither
//                improve nor drop anything from, reached AFTER a probe-driven
//                drop was made in this solve (`probe_drop_made`). Evaluated
//                BEFORE the branch reports kNumericalError, and after the
//                structural-violation and runaway checks -- both of which
//                describe conditions no repair can fix.
//      BUDGET    ONCE per solve (`post_probe_restart_spent`). The repair is a
//                heuristic, not a decision procedure: it pins until the gate
//                reads kOk, and there is no argument that repeating it
//                terminates. The exemption set bounds probe-driven DROPS, not
//                repairs, so the budget has to be stated separately. A solve
//                that needs a second restart reports kNumericalError with the
//                one block it did repair repaired -- see
//                QpEngineIndefinite.PostProbeRestartIsOneShotPerSolve, two
//                independent copies of the pinned fixture, which pins exactly
//                that split outcome.
//      FAILURE   if repair_temporary_vertex declines (it is ALL-OR-NOTHING and
//                restores ws/x itself), the original kNumericalError stands.
//                The budget is spent only on a repair that succeeded.
//
//    MEASURED (Task 6b): both pinned fixtures now reach kOptimal at
//    (0, -2, 0.5), objective -0.375, in both modes, with lambda_e = 0.5,
//    lambda_i = 1.5 and z_1 = 1.5 at x1's lower bound. Cost on the bound
//    fixture: minor_iters 7 -> 12 (border) / 6 -> 11 (refactorize), which is
//    the price of continuing a solve that used to stop.
//
//    THE EXPLICIT NEGATIVE-CURVATURE ROUTE remains the second choice and
//    remains undone: it needs a constructed direction (the EQP step is
//    structurally unusable here) AND a relaxation of ride_sign's
//    strict-descent rule, since the slope along such a direction is exactly
//    zero. It is what would be needed for the case this restart cannot serve
//    -- the second and later dead ends of one solve.
//
//    COST. Zero when no working multiplier is near zero at the exit: the
//    candidate list is built from multipliers the loop already priced and an
//    empty list returns before any linear algebra (pinned, counter-identical to
//    the pre-fix engine, by
//    QpEngineIndefinite.ZeroMultiplierProbeIsANoOpOnStrictComplementarity).
//    A DEGENERATE exit does pay, INCLUDING ON A CONVEX H, where the probe can
//    only ever read kOk (H + delta*I is positive definite on ALL of R^n, so it
//    is positive definite on the enlarged null space too, whatever the drop):
//    one probe -- one factorization in kRefactorize mode, a handful of
//    schur_updates in border mode -- per zero-multiplier candidate, at each
//    would-be-kOptimal exit the solve reaches.
//
//    COUNT THE TWO BOUNDS SEPARATELY, because they are NOT the same bound.
//    Probe-driven DROPS are bounded by n + mi, since the exemption set admits
//    each constraint at most once. PROBES are not: a real drop RESUMES the
//    loop, so the classification gate can be reached again, and the next visit
//    re-probes every zero-multiplier candidate that is not exempt. With at most
//    n + mi gate visits that can each probe at most n + mi candidates, probes
//    per solve are O((n + mi)^2) in the worst case. The convex case is the
//    benign one -- no probe there ever reads kWrong, so no drop is made, so the
//    gate is reached exactly ONCE and the cost really is one probe per
//    candidate. The quadratic term needs an indefinite H that keeps exposing
//    fresh curvature, and in exchange each of those probes buys a real drop.
//    GateIsANoOpOnAConvexFixture pins the convex case on a degenerate convex
//    vertex (one candidate, one probe: 3 -> 4 factorizations in kRefactorize,
//    2 -> 3 schur_updates in border, unchanged answer and iteration count).
//    This is the one place the section-wide "cost on convex problems is zero"
//    claim below does NOT extend to: it is a claim about the GATE, which still
//    holds.
//
//    THE CERTIFICATION BRANCH is also where a genuinely unbounded indefinite
//    QP lands today: with no finite bound to ride to, the loop stops at a
//    stationary point of an indefinite system and that branch reports
//    kNumericalError -- the same verdict Task 7's ride is specified to produce
//    when it finds no blocking constraint.
//
//    WHY LATER WORKING-SET GROWTH CANNOT INVALIDATE THE REPAIR. After the
//    repair, refresh_shifts() may add general rows to the working set (the
//    move to a vertex can violate them), and the ratio test adds more as the
//    loop proceeds. Neither can destroy the second-order consistency the
//    repair established: adding a working row SHRINKS the null space of the
//    working constraints, and by Cauchy interlacing the smallest eigenvalue
//    of a symmetric form restricted to a subspace is at least its smallest
//    eigenvalue on the enclosing space -- so a reduced Hessian that is
//    positive definite stays positive definite under any further restriction.
//    Only a DROP can reintroduce negative curvature, which is exactly the
//    event Task 7's ride is about (and, until then, the classification branch
//    above refuses to certify).
//
//    THAT MOVES x, and deliberately so: "temporary VERTEX" means a point with
//    enough bounds active to make the reduced Hessian positive definite, and
//    an interior point is not one. Pinning at the current interior value
//    instead (the literal reading of "pin at current values") would leave the
//    saddle above sitting AT the saddle -- x1 held at 0 -- and report it
//    optimal, which is the bug this section exists to fix. Moving to a bound
//    can violate general inequality rows; the caller re-runs refresh_shifts()
//    afterward and the existing homotopy drives them back down, exactly as it
//    does for any other start point.
//
//    RELEASE POLICY. Pins are then released one at a time, in reverse pin
//    order, and each release is KEPT only if the gate still returns kOk
//    without it; otherwise the pin is restored. That is the same criterion as
//    "the release exposed negative curvature", since wrong inertia on the
//    released working set is exactly what a non-descent/unbounded EQP
//    direction means.
//
//    The re-pin is deliberately RETAINED as a conservative pre-filter rather
//    than replaced in place by section 4c's ride, and the reason is where the
//    ride can be made sound. A ride needs three things the release pass does
//    not have: the identity of the constraint just freed (to fix the step's
//    SIGN), a current Aix/shift state for the ratio test, and a place to put
//    the blocker it lands on. All three exist only in the main loop. So a pin
//    the repair kept unnecessarily is superseded through the ORDINARY route
//    instead: it is an ordinary bound pin, the next iteration prices it, a
//    wrong-signed multiplier sends it to step 2's drop rule, and the drop is
//    what arms section 4c. The re-pin therefore costs at most iterations, and
//    never suppresses a ride that would otherwise have happened.
//
//    NO NEW LEDGER KIND. The plan sketched marking the repair's pins
//    distinctly in the border ledger (a Kind::kTempVertex). That is
//    deliberately NOT done: because the repair pins at BOUNDS rather than at
//    arbitrary interior values, its pins are exactly representable in
//    ws.bound_state(), and the border ledger is a DERIVED view of ws that
//    sync_borders reconciles from scratch on every iteration (and rebuild_k0
//    clears outright). A kTempVertex marking would therefore be write-only
//    state, erased by the next rebuild, and would additionally need its own
//    rhs rule in solve_bordered_eqp for a pin value that -- pinned at a bound
//    -- is already the one pinned_value() supplies. The repair tracks its own
//    pins in a local vector for the duration of the release pass instead,
//    which is the only place that information is needed.
//
//    A SPURIOUS REPAIR IS SELF-CORRECTING, which is what makes triggering on
//    a single inertia reading acceptable. Because the repair's pins are
//    ordinary bound pins, the very next iteration prices them like any other:
//    a pin the problem did not actually need carries a wrong-signed
//    multiplier and step 2's drop rule releases it. The cost of a wrong
//    repair is therefore extra iterations, or at worst kMaxIter if those
//    iterations cycle -- never a wrong answer. That is relevant
//    because a convex-but-viciously-conditioned K0 could in principle report
//    a wrong inertia with zero perturbed pivots, which no gate can
//    distinguish from a genuinely indefinite one.
//
//    COST OF THE GATE ON CONVEX PROBLEMS IS ZERO. The gate reads counters off
//    a factorization the loop was going to perform anyway (there is no probe
//    factorization: iteration 0 solves its EQP first and the verdict is a
//    by-product), and a convex K0/K can never come back kWrong. Every
//    fixture in the convex battery therefore reports the same statuses,
//    working sets and factorization counts as before this section existed.
//    This is a claim about the GATE only -- the ZERO-MULTIPLIER PROBE above
//    does charge a convex solve whose optimum is DEGENERATE; see its COST
//    note for the (bounded, answer-preserving) amount.
//
// 4c. NEGATIVE-CURVATURE RIDES AFTER A DROP (indefinite H).
//
//    Section 4b makes an indefinite START safe. This makes an indefinite DROP
//    safe, and by section 4b's Cauchy-interlacing note a drop is the ONLY
//    remaining way negative curvature can appear: adding a working row shrinks
//    the null space of the working constraints and can only raise the reduced
//    Hessian's smallest eigenvalue, so no add, no homotopy row and no ratio-
//    test blocker can undo a second-order-consistent working set. Releasing
//    one can.
//
//    WHAT GOES WRONG WITHOUT THIS. Let x be a KKT point of working set W with
//    a positive definite reduced Hessian, and let the drop rule release
//    constraint c (lambda_c < 0), leaving W'. Write null(A_W') =
//    null(A_W) + span{d} with a_c . d = -1, B = Z' H Z (the old, positive
//    definite reduced Hessian), w = Z' H d, gamma = d' H d, and
//        sigma = gamma - w' B^-1 w
//    the Schur complement -- which by interlacing is the ONLY eigenvalue of
//    the new reduced Hessian that can be negative. The next EQP's step works
//    out to
//        p = (-lambda_c / sigma) * q,   q = d - Z B^-1 w,
//    where q'Hq = sigma exactly and a_c . q = -1. Two consequences, and they
//    are the whole of this section:
//      - p'Hp = (lambda_c/sigma)^2 * sigma has the SIGN OF sigma, so the
//        curvature of the ordinary EQP step is exactly the test for whether
//        the post-drop reduced Hessian is still positive definite. No separate
//        null-space direction has to be constructed, and no second solve or
//        factorization is needed: the direction the loop already computed IS
//        the direction of negative curvature when there is one.
//      - when sigma < 0 the scalar -lambda_c/sigma is NEGATIVE, so p is a
//        negative multiple of q: it points back INTO the constraint just
//        released, whose slack is exactly zero. The ratio test answers that
//        with alpha = 0, the constraint is immediately re-added, and the loop
//        cycles between W and W' until max_iter. That cycle -- not a wrong
//        answer -- is the pre-ride failure mode, and it is the one the tests
//        in test_qp_engine_indefinite.cpp measured.
//
//    THE CHECK. On the iteration after a drop, and only there, the RAYLEIGH
//    QUOTIENT p'Hp / p'p is compared against
//        curvature_tol = kCurvatureTolFactor * hessian_scale(qp)
//    (see those two for why the threshold is relative to H's eigenvalue scale
//    and how that scale is measured). Above it, nothing changes: the ordinary
//    EQP step is taken. At or below it, the QP is unbounded below along one of
//    +/-p within the current working set, so the step to the EQP's "solution"
//    is meaningless and the loop RIDES instead.
//
//    A NEGLIGIBLE p is excluded before the check runs -- the loop's own
//    step_tol test has already classified that as a KKT point -- because the
//    Rayleigh quotient of a rounding-noise direction says nothing, and acting
//    on it was measured to turn correct kOptimal answers into spurious
//    "unbounded" reports. Such a point is section 4b's certification branch to
//    judge, not this one's.
//
//    THE ARMING IS ONE-SHOT, AND THAT IS A COVERAGE LIMIT. The drop record is
//    consumed on the very next iteration whether the ride fires, declines, or
//    is never reached -- so a DECLINED ride leaves the negative curvature in
//    place and UN-RETESTED: the ordinary step runs, and if it lands on a
//    working set that still carries that curvature, nothing re-arms this
//    section until some later drop. What covers that case is section 4b's
//    certification branch, which refuses to stamp kOptimal on a trusted-kWrong
//    final point. That is a fallback, not equivalent coverage: it declines to
//    certify rather than finding the minimizer, and it is itself silent when
//    the verdict comes back kSuspect. Re-arming on the declining iteration was
//    not done because a decline means the direction is not usable AS a ride,
//    so re-offering the same direction next iteration would either loop or
//    need a second, differently-constructed direction -- which is exactly the
//    explicit negative-curvature-direction machinery this section is designed
//    to avoid needing.
//
//    THERE ARE NOW TWO PRODUCERS OF A DropRecord, AND THIS SECTION IS VACUOUS
//    FOR THE SECOND. drop_worst is the original one, and everything above is
//    written about it. Section 4b's ZERO-MULTIPLIER PROBE is the other: when a
//    tentative drop exposes negative curvature it fills in the same record and
//    resumes the loop, deliberately so, to hand the step to this section. It
//    does not work, and cannot, for a structural reason worth stating HERE
//    rather than only at 4b: the probe drops constraints whose multiplier is
//    (numerically) ZERO, and this section's own step formula
//        p = (-lambda_c / sigma) * q
//    then gives p == 0 identically. So the arming direction is nothing, the
//    loop's negligible-p test fires first, and the ride is never even offered
//    the direction. A probe-driven drop therefore falls through to section 4b's
//    certification branch on the following iteration -- which is the SAME
//    fallback the declined-ride case above lands in, but NO LONGER with the
//    same limitation: section 4b's POST-PROBE RESTART (Task 6b) intercepts that
//    branch and spends one temporary-vertex repair there, recovering the
//    minimizer on both pinned fixtures. Arming THIS section off a
//    zero-multiplier drop would still need an explicitly constructed direction
//    AND a relaxation of ride_sign's strict-descent rule, since the slope along
//    such a direction is exactly zero -- which is why the restart, not surgery
//    here, is what landed. See section 4b's POST-PROBE RESTART note for the
//    trigger, the one-shot budget and the measurements behind both.
//
//    NOTE FOR THE DECLINED-RIDE CASE, and the arming conjunct is WEAKER THAN
//    "never". The restart is armed on `probe_drop_made`, which is a PER-SOLVE
//    sticky bit, not a per-drop one. So a drop_worst drop whose ride declined
//    lands in the certification branch's original kNumericalError *unless a
//    probe drop occurred earlier in the same solve*, in which case an unspent
//    restart may be spent on it. The intent is the narrower rule -- the restart
//    answers the specific dead end this section is structurally unable to
//    serve, and widening it would spend the budget on cases the ride is the
//    right tool for -- but the implementation does not enforce it, and that is
//    stated here rather than left as an inference from the variable's name.
//    Making it per-drop (clearing the bit when the restart fires, or when a
//    drop_worst drop intervenes) is a strictly-narrowing change no shipped
//    fixture distinguishes; see the arming note at the branch itself for the
//    measurement.
//
//    THE RIDE. Take the admissible sign of p (ride_sign: DESCENT and moving
//    off the just-freed constraint, both required -- see there for why either
//    alone is unsound), run the existing ratio test with its unit cap
//    DISABLED, and step to the nearest blocking constraint or bound, which
//    joins the working set through the same machinery any ratio-test blocker
//    does (border mode turns it into a border on the next sync_borders). The
//    loop then continues normally. The unit cap must be off because a ride has
//    no landing point of its own: the objective decreases along the entire
//    ray, so the only thing that may stop it is a constraint.
//
//    NO BLOCKER => kNumericalError, multipliers cleared, reported immediately
//    from inside the loop rather than by running out of iterations. Nothing
//    stops the ride, so the QP is genuinely unbounded below. Phase 3's
//    trust-region bounds make this branch unreachable in SQP use: every
//    variable is boxed there, so some bound always blocks.
//
//    WHICH MECHANISM FIRES, AND THEY AGREE. Two places now report a
//    second-order failure as kNumericalError, and they are MUTUALLY EXCLUSIVE
//    by construction:
//      - this section's no-blocker branch needs a preceding DROP, and fires
//        mid-loop on the iteration the curvature was measured;
//      - section 4b's certification branch needs a point where NOTHING may be
//        dropped, and fires at classification time.
//    A solve cannot satisfy both conditions on the same iteration. They return
//    the same status with the same semantics (final iterate reported,
//    multipliers cleared), so which one fires is not observable in the result
//    -- only in the iteration count, since the ride's branch stops promptly
//    where the pre-ride engine spent max_iter. The genuinely unbounded scalar
//    QP (H = diag(-1), no bounds) is section 4b's case: its working set is
//    empty throughout, so there is never a drop to arm this section.
//
//    COST ON CONVEX PROBLEMS, AND WHERE BEHAVIOR DOES AND DOES NOT CHANGE.
//    The cost is one sparse mat-vec (H . p) on post-drop iterations and
//    nothing on any other iteration, on every problem. The BEHAVIOR splits in
//    two, and the split is exactly whether H is definite:
//
//      - STRICTLY CONVEX H (positive DEFINITE). The Rayleigh quotient is at
//        least lambda_min(H), so the ride branch is UNREACHABLE and the engine
//        is bit-for-bit what it was: verified as byte-identical statuses,
//        iterates, active sets, multipliers and all three counters across a
//        convex battery of 508 fixtures x both ws_algebra modes, cold and
//        warm.
//
//        UNREACHABILITY IS A CONDITION-NUMBER CLAIM, not merely a sign claim.
//        The test is against curvature_tol = kCurvatureTolFactor *
//        hessian_scale(qp), so what is actually required is
//            lambda_min(H) > 1e-12 * max|H_ij|,
//        i.e. a 2-norm condition number below ~1e12 (up to the factor of n
//        between max|H_ij| and ||H||_2). A positive definite H worse
//        conditioned than that is INDISTINGUISHABLE here from a PSD-singular
//        one and falls under the next bullet -- which is the intended
//        behavior, since at that conditioning the direction's curvature is
//        below what the regularized solve can resolve anyway.
//
//      - PSD-SINGULAR H, INCLUDING H == 0 (an LP). The curvature along p can
//        be exactly zero, so the branch is REACHABLE and the ride does run --
//        measured at 844 entries / 411 rides over 800 randomized H == 0 solves.
//        BEHAVIOR THERE IS NOT IDENTICAL, and it is important not to claim it
//        is. The ride takes the UNCAPPED ratio while the ordinary path clamps
//        at alpha = 1, so the two agree only while the blocker is nearer than
//        the regularized step -- which is the common case (the H == 0 EQP
//        solution sits ~|g|/primal_delta, i.e. ~1e8, along the same ray, so
//        any modest box blocks first) but not a theorem. Measured on
//        randomized H == 0 LPs, 1600 blocks each:
//            box +/-1e1:    1 block differs
//            box +/-1e9:  490 blocks differ
//        The difference is in the ride's favour where it is decidable: 47 of
//        those became kOptimal from kMaxIter or kNumericalError -- with a box
//        wide enough that the regularized step cannot cross it, the capped
//        path runs away into the unbounded-artifact guard while the ride walks
//        to the true vertex (pinned by
//        QpEngineIndefinite.RideOnAWideBoxLpReachesTheTrueMinimizer). Across
//        both box sizes NO kOptimal answer was lost and none got a worse
//        objective. This is intended behavior on the PSD-singular path, not an
//        accident to be tolerated.
//
//    ANTI-CYCLING IS NOT CLAIMED, AND ride_sign IS WHY IT IS NOT NEEDED HERE.
//    A ride steps along a direction that is simultaneously non-ascending in
//    curvature and strictly descending in slope, so its objective decrease is
//    strict whenever alpha > 0, and the working set it lands in cannot be
//    revisited at the same objective. alpha == 0 is still possible at a
//    degenerate vertex, and the file's standing Degeneracy note applies
//    unchanged: no Bland/least-index rule is implemented and a degenerate
//    stall is bounded by max_iter.
//
// 5. TERMINATION. The loop reaches a KKT point of its working set with
//    nothing left it may drop, and that final point is then classified:
//      - kInfeasible if any inequality or equality row carries a STRUCTURAL
//        violation (violation_is_structural: it clears the row's tolerance by
//        a margin AND reaches an appreciable fraction of the regularization
//        footprint dual_mu*|lambda|). Both blocks are checked: the shift
//        machinery only watches inequalities, so an inconsistent equality
//        block (x0+x1=1 with x0+x1=2) or an equality unreachable inside the
//        box (x0=10 with 0<=x0<=1) is invisible to it.
//      - kNumericalError if the point is feasible but some FREE component
//        that is unbounded on the side it grew toward exceeds
//        detail::unbounded_artifact_scale() (see is_runaway) -- the answer is
//        the regularization talking rather than an optimum.
//      - kNumericalError also if the inertia gate's verdict for the system
//        just solved is a TRUSTED kWrong: the point is a KKT point of a
//        second-order-inconsistent system (a saddle or a maximizer), which
//        must not be certified kOptimal. Unreachable on a convex H. See
//        section 4b's SECOND-ORDER CERTIFICATION note. ONE ESCAPE precedes
//        that verdict: if a probe-driven drop was made earlier in THIS solve
//        and the solve's single POST-PROBE RESTART is unspent, one
//        temporary-vertex repair is attempted and, on success, the loop
//        RESUMES from the repaired vertex instead of stopping. See section
//        4b's POST-PROBE RESTART note.
//      - otherwise, and last, section 4b's ZERO-MULTIPLIER PROBE gets a veto:
//        a working-set member whose multiplier is within opt_tol of zero is
//        weakly active, so the gate above tested a SMALLER cone than the real
//        critical one. Each such member is tentatively dropped and the gate
//        re-run on the reduced labeling; a trusted kWrong there means the drop
//        is real, the loop RESUMES rather than terminating here, and no status
//        is assigned on this pass. This is the only branch in this list that
//        does not end the solve.
//      - kOptimal otherwise.
//    kMaxIter once opts.max_iter major iterations have been spent.
//
//    A FOURTH kNumericalError EXIT does not pass through this classification
//    at all: section 4c's ride, finding nothing that blocks a direction of
//    negative curvature, stops the loop mid-iteration. It carries the same
//    semantics as the two above (final iterate, multipliers cleared) and
//    cannot coincide with the certification branch -- see section 4c's WHICH
//    MECHANISM FIRES note.
//
//    ORDERING IS LOAD-BEARING: the drop rule is consulted BEFORE
//    infeasibility may be declared, because a bound pinned by the ratio test
//    is exactly what commonly blocks a shift from closing, and releasing it
//    is what lets the homotopy finish. Declaring infeasibility first turns
//    feasible problems into kInfeasible (pinned by
//    QpEngine.DropRuleRunsBeforeInfeasibilityIsDeclared).
//
//    ON kInfeasible AND kNumericalError the returned x is the FINAL ITERATE
//    the loop stopped at, whose residual is the caller's evidence of what
//    could not be satisfied. (It is not tracked as the least-violating point
//    seen; no argmin is kept.) The multipliers are deliberately CLEARED on
//    both paths: an inconsistent or runaway system prices them at
//    O(1/dual_mu) (~1e8 at the defaults) and those numbers are regularization
//    artifacts, not prices, so they must not leak out looking meaningful.
//
//    TRUSTWORTHY RANGE. Infeasibility detection rests on separating a
//    structural residual from solver noise by comparing a violation against
//    the regularization footprint dual_mu*|lambda|. That comparison has a
//    threshold, kStructuralResidualFrac, and a threshold can be crossed from
//    EITHER side. Both failure directions are real and neither is hypothetical:
//
//    (i) FALSE kInfeasible — a feasible but ill-scaled row whose refined
//        residual still clears the fraction. Bites once |lambda| exceeds
//        roughly 1e6 * row_scale; e.g. a row scaled by 1e-4 drives
//        |lambda| to 5e7, and there the regularized answer is itself
//        percent-level wrong, so reporting kInfeasible is the honest verdict
//        rather than certifying a point the engine cannot stand behind.
//        Pinned by QpEngine.IllScaledFeasibleRowsAreNotDeclaredInfeasible
//        (which holds the range down to a row scale of 3e-4).
//
//    (ii) FALSE kOptimal — a GENUINE contradiction whose gap is smaller than
//        kStructuralResidualFrac * dual_mu * |lambda| hides beneath the
//        footprint and is certified optimal. Crucially |lambda| here need not
//        come from the contradiction at all: on an ill-scaled active row the
//        OBJECTIVE inflates it (|lambda| ~ 1/(2a^2) for a row scaled by a),
//        so the footprint can grow out from under a fixed gap. Worked case:
//        a(x0+x1) = 1 against a(x0+x1) <= 1 - 3e-4 is caught at a = 1e-2 and
//        a = 3e-3 but returns kOptimal at a = 1e-3, where |lambda| ~ 3e5
//        lifts the footprint above the 1.4e-4 violation. Note this sits
//        INSIDE the |lambda| <~ 1e6 * row_scale range that direction (i) is
//        reliable in — the two directions do not share a boundary. Pinned as
//        a known limitation by
//        QpEngine.ObjectiveInflatedMultiplierHidesASmallContradiction.
//
//    Direction (ii) is an accepted trade-off, not an oversight: tightening
//    kStructuralResidualFrac to catch it re-breaks direction (i) on rows the
//    engine must support. Per the project spec the SQP driver is the second
//    detection layer and is where this case is restored; a caller relying on
//    the QP engine ALONE to certify feasibility should scale its rows or
//    shrink dual_mu.
//
//    RUNAWAY-GUARD RANGE. The unbounded-artifact guard (kUnboundedArtifactFactor
//    / is_runaway, used for the kNumericalError verdict above) has the
//    symmetric false-positive direction: a variable that is genuinely free
//    (no finite bound restrains it) whose TRUE optimum simply lies beyond
//    unbounded_artifact_scale() is indistinguishable from a regularization
//    artifact and is reported kNumericalError, multipliers cleared exactly as
//    on any other kNumericalError exit. E.g. at the defaults (primal_delta =
//    1e-8, so the threshold is 1e7) a legitimate optimum of x* = 2e7 on an
//    unbounded variable clears the threshold and is misreported. Because the
//    threshold is kUnboundedArtifactFactor / primal_delta, it scales directly
//    with 1/primal_delta: tightening primal_delta (accepting a worse
//    conditioned KKT system) pushes the boundary out and admits larger
//    legitimate optima before they are mistaken for artifacts; loosening
//    primal_delta pulls the boundary in. A caller that expects free variables
//    to legitimately settle at very large values should tune primal_delta
//    rather than treat kNumericalError here as unconditionally load-bearing.
//
// 6. TRUST-REGION SOFT BOUNDS (Task 9) -- the interface Phase 3's SQP driver
//    needs from this engine: an l-infinity trust region around the current
//    SQP iterate, expressed the same way every other bound is.
//
//    EFFECTIVE BOUNDS, COMPUTED ONCE, ABOUT THE CLAMPED SEED PRIMAL. The
//    center is start_center()'s x0 -- cold clamp(0, l, u), or warm
//    clamp(seed.x, l, u) -- and NOTHING ELSE. In particular it is computed
//    BEFORE the seed's bound-state hints are materialized onto x0 (that is
//    step 1b, ingest_seed_working_set, which runs after this block); see the
//    WINDOW-CONSISTENCY RULE below for why that ordering is a correctness
//    requirement and not a preference. Before anything else runs, this engine
//    computes
//        lo_eff(i) = max(lower(i), x0(i) - Delta),
//        up_eff(i) = min(upper(i), x0(i) + Delta),
//    Delta = opts.tr_radius, and every subsequent read of a bound in this
//    file -- the ratio test, is_runaway, repair_temporary_vertex's pin
//    choice, the direct bound-snap after a ratio-test block, and (through
//    eqp_candidate) the pinned-variable rhs in both solve_eqp
//    (kkt_assembly.h/eqp_solve.h) and solve_bordered_eqp (bordered_eqp.h) --
//    sees lo_eff/up_eff, never lower/upper directly. This is implemented as
//    a SHADOWED QpProblem reference: `qp` inside run(), from just after
//    start_center() onward, names either the caller's own problem unchanged
//    (Delta == +inf) or a local copy with lower/upper replaced (Delta finite);
//    every function below that takes `const QpProblem &qp` is therefore
//    already TR-aware with no further change, because it was already reading
//    bounds through whatever `qp` its caller happened to pass.
//
//    CROSSED EFFECTIVE BOUNDS CANNOT HAPPEN. start_center() clamps
//    lower(i) <= x0(i) <= upper(i) (step 0 has already rejected a crossed
//    REAL box), so lower(i) <= x0(i) - Delta <= x0(i) and x0(i) <=
//    x0(i) + Delta... no -- more simply: lower(i) <= x0(i) and x0(i) <=
//    upper(i) give lo_eff(i) = max(lower(i), x0(i)-Delta) <= x0(i) and
//    up_eff(i) = min(upper(i), x0(i)+Delta) >= x0(i), so lo_eff(i) <= x0(i)
//    <= up_eff(i) always. This is asserted, not merely hoped for.
//
//    THE kFixed FLIP. lo_eff(i) == up_eff(i) can only happen at Delta == 0
//    (a variable that is already genuinely kFixed, lower(i) == upper(i), is
//    excluded -- see below): a zero-radius solve pins x0 exactly, which is
//    the "no-move evaluation" a driver can legitimately ask for. Every such
//    variable is flipped to BoundState::kFixed right there (before the loop
//    runs), with tr_active set -- kFixed is the correct label because it is
//    never released by drop_worst, matching the "do not move this" intent,
//    exactly the same convention a genuinely fixed real bound already gets.
//    A variable that was ALREADY kFixed from a real lower(i) == upper(i) is
//    left alone (tr_active stays false for it): the flip only fires for a
//    variable the TR radius itself pinned.
//
//    TR-TIGHT BOUNDS ARE A SEPARATE ACTIVITY SET, NOT A NEW BoundState.
//    QpSolution::tr_active (size n, parallel to bound_state) is true at index
//    i iff the ratio test, the temporary-vertex repair, or the zero-radius
//    flip above pinned variable i at the TR side of its effective bound
//    rather than the real one (lo_eff(i)/up_eff(i) compared against
//    lower(i)/upper(i) directly; a coincidental tie -- the TR term happening
//    to equal the real bound -- is attributed to the REAL bound, not TR,
//    i.e. tr_active is false there). Three reporting consequences, applied
//    once at the very end of run(), on the FINAL working set only (a bound
//    pinned mid-loop can still be released and re-pinned on the other side
//    before the loop ends, so only the final state is meaningful):
//      (a) a TR-pinned variable's bound_state is reported kFree, never
//          kAtLower/kAtUpper/kFixed -- bound_state stays a REAL-bound-only
//          view, and a caller checks tr_active for the TR case;
//      (b) its z entry is forced to 0 -- the ratio test and drop_worst still
//          see and act on the TR pin's REAL (nonzero) priced multiplier while
//          the loop runs (a TR bound participates in the ratio test and the
//          drop rule exactly like a real one, per the interface contract),
//          but that internal number is never exposed: TR duals are not
//          prices a Phase-3 driver should read;
//      (c) nothing above changes what the loop DID -- only how the already-
//          final ws/z are reported. In particular this is why the exclusion
//          is applied after the loop, not by skipping pins/pricing for
//          TR-tight bounds during it.
//
//    THE WINDOW-CONSISTENCY RULE (fix round 1). The window above is computed
//    about the clamped seed PRIMAL; a seeded bound-state HINT is then applied
//    against that window, and a hint whose bound falls outside
//    [lo_eff(i), up_eff(i)] is DROPPED -- index i arrives kFree and the loop
//    re-derives its activity from the effective bounds like any other
//    variable. Both halves are necessary:
//      - Applying hints FIRST (what this engine used to do) let a hint at a
//        far bound become the window's own center, so the solve returned a
//        point up to |bound - intended center| away from the caller's x0 at
//        a radius of Delta. Measured on H = I, g = (-100,-100) over [0,6]^2,
//        seeded kAtUpper with seed.x zeroed: (6, 6) returned at Delta = 5,
//        2.5 AND 1.25 alike. A CALLER CANNOT DEFEND AGAINST THIS by zeroing
//        seed.x -- the zero is precisely what the hint overwrote -- so the
//        fix has to be here. Driver-level symptom (SqpDriver's shrink-retry
//        loop, whose seed comes from a solve that pinned a real bound): the
//        same step returned bit-identically at every radius while the driver
//        halved Delta to 1.7e-17, i.e. a permanent stall.
//      - Dropping an out-of-window hint rather than clamping it to the window
//        edge keeps bound_state honest: kAtLower/kAtUpper means "sitting on
//        the REAL bound", and a variable clamped to a TR-tight edge is not.
//        The loop's own ratio test pins it there and reports it through
//        tr_active, which is the documented channel for exactly that.
//    A hint INSIDE the window is honoured unchanged, which is the case every
//    ordinary warm chain relies on: when the seed's own x is carried rather
//    than zeroed, the window is centered on that x, so a bound the previous
//    solve was sitting at is at distance 0 from the center and always fits.
//
//    WARM-START SEED INGESTION IGNORES tr_active BY CONSTRUCTION, NOT BY AN
//    EXPLICIT CHECK. ingest_seed_working_set() reads seed.bound_state
//    and seed.ineq_active only -- it has no reason to read seed.tr_active,
//    and does not. Combined with reporting rule (a) above, a variable that
//    was TR-pinned on the solve that produced `seed` already arrives as
//    bound_state == kFree, which ingest_seed_working_set()'s "only pin what
//    the seed says is pinned" logic leaves untouched (kFree means "not seeded
//    pinned"): the TR pin is not carried into the new solve's working set at
//    all, which is exactly the "TR bounds excluded from working-set
//    carryover" the interface requires -- the new solve's radius may differ,
//    and effective bounds are recomputed about the NEW solve's own x0 (the
//    seed's x, clamped by the REAL bounds -- old TR bounds leave no trace
//    beyond that x).
//
//    HOT-START REUSE INTERACTION (border mode). K0's structural/values
//    hashes never depend on lower/upper (see detail::structural_hash /
//    detail::values_hash), so a bound change between two solves on the same
//    QpEngine can NEVER poison K0 reuse on fingerprint grounds -- a pin's
//    border column is BorderOps::pin_variable(i, ...), i.e. e_i, which does
//    not depend on the bound value at all; only the border's RHS entry
//    (pinned_value(qp, state, i)) carries the bound value, and that RHS is
//    rebuilt from the CURRENT `qp` (so the current effective bounds) on
//    every solve_bordered_eqp call -- never cached across solves. What Task
//    5's review actually verified (tr_radius did not exist yet, and
//    QpOptions is fixed per QpEngine instance -- see the PHASE-3 DESIGN
//    FRICTION note just below -- so it could not have been "perturbed
//    between calls on one instance" as an earlier draft of this note
//    claimed): 4000 randomized warm re-solves that perturb the RAW
//    lower/upper bounds between two solve() calls on the same engine
//    instance, zero divergence from the equivalent cold solves, by exactly
//    the e_i/rebuilt-rhs mechanism above. tr_radius changing (lo_eff/up_eff
//    ultimately deriving from a different Delta) reduces to that same case:
//    a TR radius only ever tightens `lower`/`upper` into `lo_eff`/`up_eff`,
//    it does not change how a pin's border column or rhs are built, so
//    nothing about that verified mechanism depends on the bound coming from
//    a real bound or a TR one. What a bound change (real or TR-driven) CAN
//    do is change ws.bound_state() at the seed (the kFixed flip above
//    changes it outright when a new radius drives a variable to
//    lo_eff == up_eff where the old one did not, or vice versa), which the
//    existing reuse-eligibility check (condition (b), comparing the seed ws
//    against the previous exit ws) already treats as a working-set change
//    like any other -- correctly forcing a rebuild rather than reusing a
//    border stack built for a different pin set. That is expected behavior,
//    not a gap, and is exercised on a single engine instance by
//    tests/test_qp_engine_tr.cpp's SameInstanceWarmSolveWithShiftedTrWindow.
//
//    PHASE-3 DESIGN FRICTION -- RESOLVED (Phase 3 Task 2). tr_radius still
//    LIVES ON QpOptions, which is still PER-INSTANCE CONST (`explicit
//    QpEngine(const QpOptions &opts) : opts_(opts), ...`, never reassigned):
//    that field itself is exactly what it was and cannot vary across solve()
//    calls on one instance. What changed is that it no longer has to -- every
//    solve() overload now also takes a `const SolveOverrides &` (types.h),
//    resolved ONCE at the very top of run() into effective tr_radius/
//    primal_delta/dual_mu values (a field at its sentinel resolves to the
//    corresponding opts_ value, so a default-constructed SolveOverrides
//    reproduces opts_ exactly -- see types.h's SolveOverrides for the
//    sentinel convention). Those effective values, not opts_'s own fields,
//    are what every read site in this file below actually consults from that
//    point on (the two-argument/three-argument overloads without a
//    SolveOverrides forward a default-constructed one, which is why they are
//    byte-identical to the pre-Task-2 engine -- see
//    tests/test_qp_engine_tr.cpp/test_qp_warm_start.cpp's forwarding guard).
//    A Phase-3 SHRINK-RADIUS RETRY LOOP (a rejected SQP step re-solved at a
//    smaller Delta) therefore now shares ONE QpEngine instance across every
//    retry radius, and keeps that engine's hot-start K0/border reuse across
//    the loop wherever the ordinary reuse-eligibility conditions otherwise
//    allow it (tr_radius itself never has to join the reuse key -- see the
//    HOT-START REUSE note's condition (d) and the HOT-START REUSE INTERACTION
//    note above for why bounds never enter K0 regardless of where they came
//    from). tests/test_qp_engine_tr.cpp's ShrinkRadiusRetryReusesHotStart is
//    the guard for exactly this scenario, including its own hand-derived
//    geometry note on why a CHAINED pair of solves (each seeded from the
//    other's own returned QpSolution) cannot ALSO both carry the SAME pin at
//    exit and still reuse: rule (a) above reports a TR-pinned index kFree, so
//    chaining from the engine's own output can never reproduce that pin as a
//    seed hint. That is narrower than "TR pins and reuse are mutually
//    exclusive" -- they are not (a HAND-BUILT seed hint can still force a
//    bound pin, the loop can release it and land on a genuinely different
//    TR-attributed pin before exit, and reuse-eligibility only ever compares
//    the PRE-LOOP seed ws against the PRIOR exit, never against what the
//    CURRENT solve's own loop later discovers) -- see the test's own comment
//    for a verified counter-example. It is specifically the CHAINED-from-
//    output construction that cannot reproduce a TR-attributed exit pin as a
//    seed, not something this task changes or could change.
//
//    THE REUSE-KEY EXTENSION THIS REQUIRED. Before this task, primal_delta/
//    dual_mu could not differ between two solve() calls on one instance
//    (opts_ being const), so K0's reuse-eligibility check never needed to
//    compare them -- H/Ae/Ai's structural/value hashes were the whole
//    story. With per-solve overrides, the EFFECTIVE (primal_delta, dual_mu)
//    pair now can differ solve-to-solve on one instance, and that pair
//    enters K0's VALUES exactly as much as H/Ae/Ai do (assemble_kkt_core's
//    regularized diagonal), so it now JOINS the reuse key as condition (d)
//    (see the HOT-START REUSE note above): border_effective_delta_ /
//    border_effective_mu_ are committed alongside the two hashes on a clean
//    kOptimal exit, and a warm re-solve whose resolved pair differs from
//    what is stored there forces a K0 rebuild exactly like a changed H
//    would. tests/test_qp_warm_start.cpp's
//    DeltaMuOverrideForcesRefactorization is the guard, with a recorded
//    mutation-check (temporarily dropping the pair from the key) in
//    task-2-report.md. tr_radius is NOT part of this extension and does NOT
//    join the key: unlike primal_delta/dual_mu it never reaches K0 at all,
//    only ever tightening bounds into lo_eff/up_eff, which are rebuilt fresh
//    into every border's rhs on every solve regardless (see the HOT-START
//    REUSE INTERACTION note above).
//
//    UNBOUNDED-ARTIFACT GUARD AND REPAIR SYNERGY. is_runaway() and
//    repair_temporary_vertex() (section 4b) both read bounds through the
//    same shadowed `qp`, so with a finite tr_radius every variable has a
//    finite effective bound on both sides. Two consequences noted in the
//    header contract's own sections above, restated here because Task 9 is
//    what makes them reachable in the first place: is_runaway() can never
//    fire for a TR-bounded variable (its effective bound is always within
//    Delta of x0, far inside unbounded_artifact_scale() for any reasonable
//    Delta), and repair_temporary_vertex()'s "no finite bound to pin" failure
//    mode (section 4b's ALL-OR-NOTHING note) cannot occur at all -- every
//    variable becomes pinnable. This is the fix for the unpinnable-repair
//    hole those sections describe (measured there at up to 210/241 solves
//    going second-order-invalid with two of three variables unbounded):
//    Phase 3 always calls through a finite trust region, so
//    kNumericalError-from-unbounded is unreachable in SQP use, exactly as
//    predicted.
//
//    BIT-IDENTICAL OFF PATH. opts.tr_radius defaults to +inf. At that value
//    std::isfinite(opts.tr_radius) is false, no QpProblem copy and no
//    effective-bounds Vec (lo_eff/up_eff) is ever materialized, and the
//    shadowed `qp` reference aliases the caller's own problem directly --
//    the "zero new work" claim is scoped to exactly those two allocations.
//    QpSolution::tr_active itself (a size-n vector<bool>, all false) is
//    still allocated unconditionally on every solve, same as bound_state and
//    ineq_active always are -- it is part of QpSolution's contract, not
//    something the off path skips. With that scoping, every one of the 109
//    pre-Task-9 tests, and every counter/status/iterate they check, is
//    unchanged bit-for-bit. tests/test_qp_engine_tr.cpp's
//    TrOffIsBitIdenticalDefault is the guard for this claim.
//
// 6b. THE EXPORT INVARIANT ON z: A FREE VARIABLE CARRIES NO BOUND PRICE
//    (Phase-7 Task 6b, docket D0). For every index i, the returned pair
//    satisfies
//        bound_state[i] == kFree  =>  z(i) == 0.0
//    on EVERY status this engine can return -- kOptimal, kMaxIter,
//    kInfeasible and kNumericalError alike. Section 2's labeling note already
//    stated one instance of it ("reported kFree with z == 0") and
//    nlp_kkt_check.h's `dual_sign` already checks the NLP-level analogue; it
//    is enforced here at the point of export because `price()` imposes it
//    against the working set live at PRICING time, and a `kMaxIter` exit
//    leaves the loop after later mutations without re-pricing. See the
//    enforcement site at the bottom of run() for the mechanism, the direction
//    argument, and why it is a no-op on the other three statuses. **THE
//    ENFORCEMENT SITE ITSELF LIVES IN `run()` ONLY** (final branch review
//    WAVE #14, T6b m4): `refine_on_face` -- the engine's other public
//    `QpSolution` producer -- satisfies this same invariant by a DIFFERENT
//    route (`price()`'s own postcondition plus its own TR-exclusion pass,
//    verified), not by calling into this loop; a reader adding a third
//    producer must re-derive the invariant there rather than assume it is
//    inherited.
//
//    THIS IS A ONE-WAY GUARANTEE, deliberately: a PINNED index's z is the
//    price the last `price()` call computed, which on a kMaxIter exit may be
//    one working set stale. Repairing THAT would need a factorization at the
//    exit point, which a capped solve has by definition run out of budget
//    for. A caller that needs fresh prices needs a converged solve.
//
//    `kFixed` (lower == upper) IS SILENTLY OUTSIDE THIS INVARIANT (final
//    branch review WAVE #14, T6b m6): it is stated over `bound_state ==
//    kFree`, and a fixed variable can neither be freed nor trip the
//    absent-bound check (`drop_worst` notes explicitly that it is "not
//    sign-constrained"), so it never enters either side of the implication.
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

// ---------------------------------------------------------------------
// PHASE-6 TASK 4 (M6) -- THE SIZE-DERIVED QP ITERATION CAP.
//
// QpOptions::max_iter (types.h) now defaults to the sentinel 0, meaning
// "derive the cap from this subproblem's size"; a positive value is an
// explicit absolute cap and wins outright. These three helpers are the whole
// of the derivation, factored out of run() so they can be tested (and quoted
// by a bench) without solving anything.
//
//     base  =  n + mi + #bounded          (qp_cap_base)
//     cap   =  max(kQpMaxIterFloor, kQpMaxIterCoeff * base)
//
// WHY THAT BASE. The identification walk's cost law is
// `minors ~ 1.50 * (ws_adds + ws_drops)`
// (docs/notes/2026-08-03-identification-stall-study.md Sec. 3), and every
// working-set event is either an inequality ROW admission/release or a
// variable BOUND pin/release. So the demand scales with the number of
// constraints the walk can touch -- `mi` plus the number of variables with a
// finite bound -- and `n` is carried alongside them because the EQP's own
// dimension bounds how many independent directions the walk can traverse
// before it must repeat. This is the shape
// docs/notes/2026-07-30-scale-study-cold.md Sec. 5 recommended, verbatim.
//
// #bounded COUNTS *REAL* BOUNDS, NOT EFFECTIVE ONES, and the distinction is
// load-bearing rather than cosmetic. The SQP driver passes a FINITE
// trust-region radius on every subproblem (SolveOverrides::tr_radius), so the
// effective box lo_eff/up_eff is finite in EVERY coordinate on essentially
// every driver-issued solve -- counting those would make #bounded == n
// unconditionally and turn `base` into `2n + mi` regardless of the problem's
// actual geometry, which is not a size the demand tracks. qp_cap_base
// therefore reads the CALLER'S OWN qp.lower/qp.upper, before Section 6's
// window is applied.
//
// THE CALIBRATION OF kQpMaxIterCoeff is measured, two-sided, and recorded in
// docs/notes/2026-08-03-crash-basis.md Sec. 4 -- read it before changing this
// number. Summary of what the number has to clear: on the F7 collocation
// family (`base = 8N`) the largest PER-SUBPROBLEM demand observed across
// twelve healthy cells from N = 30 to N = 1500 is 2.856 * base (N = 200,
// p = 0.90: one subproblem wanting 4569 minors at base = 1600); the median is
// 1.45 * base. 5 clears the observed maximum by 1.75x. NOTE THAT THIS RETIRES
// docs/notes/2026-07-30-scale-study-cold.md Sec. 5's own C ~ 3 proposal: that
// figure came from whole-SOLVE minor totals and never measured the N = 200 /
// p = 0.90 cell, where C = 3 would clear the demand by 1.05x -- a coincidence
// rather than headroom. The coefficient is the smallest integer that
// clears the observed maximum with real headroom, because the two sides of
// the error are NOT symmetric: too small LOSES a solve that would have
// converged (a capped subproblem is refused, the step is rejected, and the
// driver exits kNumericalError at a point nowhere near x*), while too large
// only costs TIME on a subproblem that was never going to converge (the SQP
// absorbs a cut-short subproblem and still returns kOptimal --
// docs/notes/2026-07-30-scale-study-cold.md Sec. 4.2 measured the same
// answer at 20 000 and at 500 000, 18x apart in wall).
//
// THE FLOOR is the historical fixed default, kept as a floor rather than
// discarded: `base` is tiny on a small problem (4 on a 2-variable, 1-row,
// 1-bounded Hock-Schittkowski model) while the demand there is not
// proportionally tiny (HS10 spends 218 minors over 14 majors at n = 2), so a
// pure multiple of `base` would cap small problems FAR below their demand and
// break solves that work today. Every cap-related pin in this suite that does
// NOT move under M6 is a solve sitting under this floor.
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
// from size. THE PRECEDENCE IS THE OVERRIDABILITY REQUIREMENT
// docs/notes/2026-07-30-scale-study-cold.md Sec. 5 made a condition of this
// change -- every existing tiny-cap battery and fixture sets max_iter
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
// truth (docs/notes/2026-07-27-pardiso-inertia-findings.md).
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
// Rule order (docs/retarget-design-sqp.md SS4.1):
//   1. non-kObserved evidence -> kSuspect. The explicit route for "no
//      factorization has produced counts" (pre-factorization, failed
//      factorization, a failed backend query): the dissolved seam read a
//      constructor-zeroed triple there and landed on kSuspect through the
//      short-sum rule; the same verdict now takes the honest channel.
//   2. perturbed pivots (present and nonzero) -> kSuspect. Per the findings
//      doc, a factorization pardiso had to perturb reports an inertia that
//      looks exactly like a genuine one (the probe's exactly-singular matrix
//      reports the same (2, 1) its nonsingular neighbors do), so the
//      reported counts carry no information at all in that case -- including
//      when they happen to MATCH the expectation, which is the
//      silent-failure direction this guards. Pardiso raises no error for
//      that matrix either, so there is nothing to catch. Absent evidence
//      (Accelerate has no such counter) does not trigger this rule --
//      nothing is fabricated from absence.
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
// (kElasticRhoFactor in sqp_driver.h) so the project has one escalation
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
// restricted to the FREE variables -- the same r QpEngine::price builds, read on
// the complementary set of coordinates. On a pinned variable r(i) IS the
// bound multiplier z(i) and says nothing about stationarity (that is what
// the drop rule's sign test judges); on a free variable z(i) is zero by
// construction, so r(i) is the whole first-order condition there, and a
// nonzero one is a witness that the point is not a KKT point of ANY
// labeling.
//
// Returns the inf-norm over the free block and reports, through `scale`,
// the largest term that went into r -- the denominator the caller's
// opt_tol is applied to.
//
// THE THRESHOLD IS SCALED, AND THAT DIVERGES FROM THIS FILE'S OTHER
// opt_tol TESTS, which are absolute (drop_worst's `viol > opts_.opt_tol`
// and the zero-multiplier probe's candidate test both compare a raw
// multiplier against opt_tol). The relative form here is deliberate and
// is NOT a claim that the file already works this way -- feas_tol is what
// this engine scales; opt_tol is not:
//
//   - SCALE-INVARIANCE AT A SUSPECT EXIT. Every other opt_tol test in this
//     file reads a MULTIPLIER, whose size the caller controls through the
//     constraint scaling they chose. This one reads a GRADIENT RESIDUAL,
//     which scales with the objective: multiplying f by 1e6 multiplies r
//     by 1e6 and would turn an absolute test into "refuse every suspect
//     exit on a large-objective problem". The quantity being judged is
//     "did the algebra solve anything", which is a relative question.
//   - IT ERRS LOOSER, WHICH IS THE SAFE DIRECTION HERE. scale >= 1 always,
//     so this threshold is never tighter than the absolute opt_tol the
//     drop rule already applied before certifying. The gate can therefore
//     only refuse points the drop rule was happy with by a wide margin --
//     false REFUSALS are damped, and the measured margin on the defect
//     this exists for is 1e9 (residual 1.04 against a threshold of 1e-9).
//     A false refusal costs a bounded ladder and a recoverable
//     kNumericalError; a false certificate is unrecoverable.
//
// NaN IS A FAILURE, NOT A PASS. std::max(worst, NaN) returns `worst`, so a
// naive accumulation would make a NaN residual component VANISH and the gate
// would then certify kOptimal on a factorization that produced NaN -- the
// exact Phase-3 defect class this gate exists to close, arrived at from the
// other side. The accumulation below is therefore NaN-STICKY (see its own
// comment: the obvious negated-comparison fix is ALSO wrong, in the opposite
// direction), and the VERDICT is taken by free_block_is_stationary below
// rather than by a bare comparison at the call site -- so both halves of the
// NaN behaviour sit in this file's `detail` block where a test can reach
// them, instead of one half being reachable only end-to-end.
//
// NOT a general-purpose KKT checker: it deliberately ignores primal
// feasibility and multiplier signs, both of which the classification
// branch has already judged by the time this is called (see run()).
//
// IT LIVES IN `detail` RATHER THAN ON QpEngine for the same reason
// inertia_verdict above does: it is one of the two primitives certification
// turns on, and a gate primitive should be testable AT THE CALL LEVEL rather
// than only through whatever end-to-end fixture happens to reach it. The
// NaN arm in particular has no reachable end-to-end fixture -- a
// factorization bad enough to emit NaN poisons the primal step long before
// the certification branch -- so the call-level test is the only test there
// is (QpEngineIndefinite.FreeBlockStationarityTreatsNanAsAFailure).
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

// The suspect-stall gate's DECISION, factored out of run() so that the
// comparison and its test cannot drift apart. `residual`/`scale` are
// free_block_stationarity's two outputs.
//
// WRITTEN AS `<=`, NOT `!(>)`, AND THAT IS THE WHOLE POINT: every comparison
// against NaN is false, so `residual <= tol` is FALSE for a NaN residual and
// the caller's `!stationary` therefore routes it to the escalation ladder.
// Had the caller instead asked `residual > tol` directly, a NaN would have
// answered "not a violation" and certified kOptimal. Both readings are one
// character apart and only one is safe, which is why the decision is a named
// function with its own test (QpEngineIndefinite.
// FreeBlockStationarityTreatsNanAsAFailure) rather than an inline expression
// only an end-to-end fixture could exercise -- and no end-to-end fixture can,
// since a NaN primal step never reaches the certification branch at all.
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
// WHY THIS SCALE. The quantity being classified is a Rayleigh quotient, which
// lies in [lambda_min(H), lambda_max(H)], so the honest scale is
// max|lambda| = ||H||_2. max|H_ij| brackets that within a factor of n
// (max|H_ij| <= ||H||_2 <= n * max|H_ij|), costs one pass over the stored
// nonzeros instead of an eigensolve, and -- the property that actually matters
// -- is exactly homogeneous in H, so scaling the objective by a constant
// leaves every ride decision unchanged. The factor-of-n slack is irrelevant at
// 1e-12: it moves the threshold by far less than the gap between the two
// populations being separated (a curvature that is genuinely positive sits at
// O(lambda_min) and one that is genuinely negative at O(-|lambda|), neither
// anywhere near 1e-12 * ||H|| on a conditioned problem).
//
// FLOORED AT 1 so an H that is entirely zero -- a pure LP -- still yields a
// usable (then plain absolute, 1e-12) tolerance rather than 0. There the
// curvature is exactly 0, the ride branch is taken, and that is CORRECT: after
// a drop an LP's freed edge direction has no curvature and its objective
// decreases linearly until a constraint blocks, which is precisely the step
// the ride makes. It is also the step the pre-ride code already made, by a
// different route (the delta-regularized EQP solution sits ~|g|/delta away
// along the same direction and the capped ratio test clips it to the same
// blocker), so the floor changes no LP answer.
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

// FNV-1a mixing step, identical constants to the dissolved seam's
// hash_pattern (and the same FNV-1a family hven::pattern_hash uses).
inline void fnv1a_mix(std::uint64_t &h, const void *data, std::size_t len) {
    constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i < len; ++i) {
        h ^= bytes[i];
        h *= kFnvPrime;
    }
}

// A caller-supplied QpProblem matrix is not guaranteed to already be in
// COMPRESSED form (unlike the KKT matrix K, which this engine assembles itself
// via setFromTriplets + makeCompressed), so both hashers below only pay for
// a compressed COPY when the input actually needs one; the common case
// (already compressed, e.g. anything built via .sparseView() or this
// engine's own assemble_kkt_full) binds `m` straight to the caller's matrix.
inline void mix_pattern(std::uint64_t &h, const SpMatRM &m_in) {
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
    fnv1a_mix(h, m.outerIndexPtr(),
              sizeof(SpMatRM::StorageIndex) * static_cast<std::size_t>(rows + 1));
    fnv1a_mix(h, m.innerIndexPtr(), sizeof(SpMatRM::StorageIndex) * static_cast<std::size_t>(nnz));
}

// Mixes rows/cols/nnz as a separator BEFORE the value bytes themselves, not
// just the bytes alone. This hardens against a real collision class: two
// matrices with different SHAPE can produce byte-identical value streams,
// e.g. mi=2 with rows [1,0],[-1,0] (values [1,-1], one nonzero per row)
// versus mi=1 with the single row [1,-1] (values [1,-1], both nonzero) --
// same bytes, different mi. structural_hash (mix_pattern, above) is the
// primary guard against that class and IS checked alongside this one in
// every reuse decision (see the header contract's HOT-START REUSE note and
// QpWarmStart.StructureChangeAtConstantValueBytesForcesRefactorization), but
// mixing the shape in here too means values_hash alone is no longer
// collision-prone across a shape change, cheaply, in case it is ever
// consulted on its own.
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
// H/Ae/Ai's own nonzero patterns (delta/mu diagonals are always present
// regardless), so hashing the three raw patterns is equivalent to hashing an
// assembled K0's pattern -- without ever assembling one.
inline std::uint64_t structural_hash(const QpProblem &qp) {
    std::uint64_t h = kFnvOffsetBasis;
    mix_pattern(h, qp.H);
    mix_pattern(h, qp.Ae);
    mix_pattern(h, qp.Ai);
    return h;
}

// Condition (c): have H/Ae/Ai's VALUES changed? Named `values_hash` per the
// task brief.
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
// PHASE-4 TASK 4: hoisted from a nested class member to a free struct in
// this namespace, and QpEngine's own `border_` member (below) now holds it
// through a std::shared_ptr rather than by value. That is the ONLY change
// this task makes to BorderState itself -- every field, every deleted
// copy/move, and the reference `schur` holds into `kkt` are exactly as
// they were. The reason is HotState below: a BorderState must live at a
// FIXED heap address for `schur`'s reference into `kkt` to stay valid (this
// is already why copy/move were deleted), and a std::shared_ptr is what
// lets a SECOND QpEngine instance -- one that never ran the solve() call
// that built this BorderState at all -- share that same fixed address
// without this engine's own destructor ever double-freeing the Pardiso
// handle `kkt` owns. See HotState's own OWNERSHIP note for the full
// argument.
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

    // M3 PHASE B (docs/retarget-design-sqp.md SS7): the FIX ROUND 1
    // `generation` counter this struct used to carry is DELETED. Its job --
    // letting a SECOND engine instance that adopted this object via a
    // HotState handle detect that the object has moved on since the handle
    // was taken, even though every problem-shaped fingerprint still matches
    // -- is carried by the backend factor's own identity: `analyze()` moves
    // `kkt.factor.session_id()` and every successful `factorize()` advances
    // `kkt.factor.epoch()`, so rebuild_k0() no longer stamps anything (the
    // factorize IS the stamp). See QpEngine::reuse_eligible's condition (e)
    // and HotState's kkt_session_id/kkt_epoch pair; the failed-rebuild case
    // generation's bump-before-mutate used to cover is closed by the
    // usable-numerics conjunct there (SS7.2).
};

// PHASE-4 TASK 4: HOT-START LEVEL. The opaque handle behind
// warm_start.h's WarmStart::hot -- forward-declared there
// (`struct HotState;`) and DEFINED here, where the machinery it snapshots
// actually lives. A HotState is a frozen copy of the fingerprint/exit-state
// members QpEngine::run() already tracks per instance (see the HOT-START
// REUSE note above QpEngine and border_valid_'s own declaration below),
// PLUS shared ownership of the BorderState those fingerprints describe --
// the K0 symbolic analysis and last numeric factorization, Pardiso pt
// handle included.
//
// OWNERSHIP AND THE PARDISO-HANDLE SAFETY ARGUMENT (the design this task's
// brief asked to be stated explicitly). hven::linear::SymmetricFactor (the
// factor inside KktFactor) is move-only with a non-throwing destructor that
// releases its backend session (Pardiso pt handle included) exactly once;
// BorderState wraps one BY VALUE and is
// itself both non-copyable AND non-movable (its own `schur` holds a
// reference into its own `kkt`, so moving the struct would leave that
// reference dangling -- see BorderState's note above). Two designs were
// available for handing a BorderState's factorization to a DIFFERENT
// QpEngine instance: (i) transfer sole ownership, so exactly one engine
// instance ever again touches that pt handle, or (ii) share ownership via a
// reference-counted pointer, so the handle is released exactly once
// regardless of how many engines have looked at it, whenever the LAST
// reference drops. THIS FILE TAKES (ii): `border_` (QpEngine's own member,
// below) is a std::shared_ptr<BorderState>, HotState::border is a COPY of
// that same shared_ptr (not a raw pointer, not a reference), and
// hot_state() below hands out that copy rather than anything that could
// outlive the BorderState it names. Consequences:
//   - NO DOUBLE FREE: a BorderState's destructor (and through it, the
//     factor's single backend-session release) runs exactly once, when
//     the LAST std::shared_ptr referencing it -- whichever engine or
//     WarmStart/HotState chain happens to hold it -- is destroyed. It is
//     irrelevant how many engines have copied the pointer meanwhile.
//   - NO USE-AFTER-RELEASE: as long as ANY QpEngine, WarmStart, or HotState
//     holds a copy of the shared_ptr, the BorderState (and its Pardiso pt_
//     array) is guaranteed alive; a QpEngine that adopts a hot handle (see
//     run()'s ADOPT AN EXTERNAL HOT HANDLE step) can therefore call
//     factorize()/solve() against it with no lifetime hazard whatsoever,
//     even though the ORIGINAL engine that built it may since have been
//     destroyed.
// WHAT SHARED LIFETIME ALONE DOES NOT MAKE SAFE, WHAT FIX ROUND 1 FOUND, AND
// WHAT FIX ROUND 2's RE-REVIEW ADJUDICATED ABOUT WHICH PART OF THE FIX
// ACTUALLY CLOSES IT. Shared ownership answers LIFETIME (no dangling, no
// double free) but, ON ITS OWN, says nothing about whether the shared
// object's CONTENTS still mean what a holder's frozen fingerprint claims
// they mean. Fix Round 1's review demonstrated exactly that gap, WITHIN the
// sequential, one-handle-at-a-time usage this note used to call sufficient:
// producer P emits a handle, THEN SOLVES AGAIN on its own engine (an
// ordinary, unremarkable thing to do with a `WarmStart` that is a plain
// copyable value the caller is invited to store), which -- under the
// PRE-FIX code -- wiped the SAME shared BorderState's schur/latched/
// k0_rows/ledger fields IN PLACE and then rebuild_k0()'d it for P's own next
// problem; consumer C then adopted the ORIGINAL handle and found every one
// of the four PRE-EXISTING fingerprint fields still matching ITS OWN
// problem (they describe P's FIRST problem, which is what C also happens to
// be solving) -- so C certified kHot, reported qp_factorizations == 0, and
// silently computed its answer against P's SECOND problem's K0. No throw,
// no degradation: the frozen snapshot and the shared object had quietly
// gone separate ways, and nothing compared them.
//
// TWO INDEPENDENT MECHANISMS WENT IN TO CLOSE THIS, and FIX ROUND 2's
// re-review is explicit about which one actually does the work:
//   - DETACH (`border_ = std::make_shared<BorderState>()` at every
//     `!reuse_eligible` site, rather than wiping the existing object's
//     fields in place) is THE LOAD-BEARING FIX. Once a refused reuse
//     allocates a FRESH object instead of mutating the shared one, the ONLY
//     engine that can ever write into a shared BorderState is one whose OWN
//     conditions (a)-(e) already passed for it -- which means every K0 ever
//     written into a shared object already carries the VALUES (H/Ae/Ai,
//     effective primal_delta/dual_mu) any handle's fingerprint describes,
//     and the one remaining difference (which rows happen to be folded into
//     K0 versus carried as live borders) is exactly what sync_borders()
//     reconciles UNCONDITIONALLY on every call regardless (the HOT-START
//     REUSE note's own "WHY THIS IS SAFE EVEN THOUGH (b) IS ONLY
//     APPROXIMATE" argument, undisturbed by any of this). No fixture built
//     for this fix round -- including one deliberately constructed so a
//     producer's own top-level conditions (a)-(d) keep passing across
//     several of its own further solves, forcing genuine schur_cap-driven
//     mid-solve rebuilds on the shared object without ever detaching --
//     has produced a wrong ANSWER once detach is in place; every regression
//     it catches is a factorization-count / start_level_used assertion, the
//     answer itself matching a clean control exactly.
//   - The factor's IDENTITY pair -- `kkt.factor.session_id()` / `epoch()`,
//     M3 phase B's replacement for the deleted `BorderState::generation` --
//     plus this engine's own committed copies (below) are DEFENSE-IN-DEPTH,
//     not the mechanism that makes the sharing safe. Every rebuild_k0()
//     call advances the epoch on the object itself (factorize() IS the
//     stamp), whoever makes it, and a pattern-change rebuild moves the
//     session id too (analyze() forks a fresh session); HotState freezes
//     the pair it saw AT EMISSION; QpEngine::run()'s reuse_eligible check
//     (its condition (e)) compares that FROZEN pair against LIVE
//     `session_id()`/`epoch()` reads off the shared object on EVERY solve()
//     call -- not only at adoption -- which is what would catch the
//     SYMMETRIC ordering too (C adopts, THEN P solves again, THEN C solves
//     again): C's own four problem-shaped fields never changed, but the
//     object did, and the identity read live off the object is what
//     notices. (The one case the old counter caught that the epoch does not
//     -- a FAILED rebuild, which bumped generation before the mutation that
//     threw but does NOT advance an epoch -- is closed by condition (e)'s
//     usable-numerics conjunct, docs/retarget-design-sqp.md SS7.2: a failed
//     factorize leaves `inertia().state != kObserved`, so reuse is refused
//     on that ground instead.) A mismatch degrades to kWarm exactly like
//     any other reuse_eligible failure -- silent, never a throw -- and
//     forces a rebuild in a case that, by the argument above, was already
//     value-consistent and so was never going to compute a wrong answer
//     either way. It earns its place as a cheap, independent check that
//     does not rely on the sync_borders argument holding in every corner
//     this file's own tests have not tried to reach, not because reuse
//     would otherwise be unsound.
//
// WHAT REMAINS GENUINELY UNSAFE: CONCURRENCY, not sequencing. The
// session/epoch identity is ordinary, unsynchronized state read and
// written with no atomics and no locking -- it detects a STALE handle
// exactly because engine calls happen one at a time, in some order, on one
// thread (or on threads that never overlap in time). It detects nothing,
// and prevents nothing, if two QpEngine instances that share a
// BorderState call solve() CONCURRENTLY: that is a data race on the
// factor's identity state, on `border_->kkt`'s backend session, and on
// every other BorderState member, full stop, regardless of what any
// fingerprint says. See the THREAD SAFETY note below QpEngine's
// declaration -- it is the canonical location for this rule and states it
// in those terms; this paragraph exists so a reader who lands here first
// is not left thinking shared ownership plus an identity stamp is a
// concurrency story, because it is not one.
//
// SAME-PROCESS ONLY, per warm_start.h: HotState is never serialized, and
// nothing here attempts to make it so -- a std::shared_ptr and a live
// Pardiso pt_ array have no meaningful on-disk representation.
//
// WHICH K0 THE HANDLE DESCRIBES WHEN THE PRODUCER'S LAST SUBPROBLEM WAS
// ELASTIC/SOC. sqp_driver.h's elastic re-solve and SOC re-solve both call
// `engine_.solve()` again on the SAME engine_ instance the ordinary majors
// use. The two cases differ: the ELASTIC re-solve builds an AUGMENTED
// (original-plus-slack) variable space, so a handle emitted right after it
// describes that augmented-space K0, not the original problem's -- SAFE (a
// later solve's probe is hashed against the unaugmented problem and cannot
// match an augmented-space fingerprint), but the handle SILENTLY FORFEITS
// kHot for the very next major (degrading to kWarm rather than throwing or
// being flagged). The SOC re-solve, by contrast, shifts only the
// right-hand sides (be/bi -- see build_soc_subproblem): H/Ae/Ai patterns
// AND values are identical and K0 does not depend on the rhs, so a
// post-SOC handle carries the SAME fingerprints and remains an ordinary
// (a)-(e)-gated reuse candidate -- no forfeiture. Both behaviors were
// previously undocumented.
struct HotState {
    std::shared_ptr<BorderState> border;
    std::uint64_t structural_hash = 0;
    std::uint64_t values_hash = 0;
    double effective_delta = 0.0;
    double effective_mu = 0.0;
    std::vector<BoundState> exit_bound_state;
    std::vector<Index> exit_active_ineq;
    // M3 PHASE B (docs/retarget-design-sqp.md SS7): the COMMITTED
    // (session_id, epoch) identity of `border`'s factor at emission --
    // hot_state() emits this engine's own last-committed pair, never a live
    // re-read off the possibly-shared object (see hot_state()'s FIX ROUND 2
    // note). Replaces FIX ROUND 1's `generation` stamp: analyze() moving
    // the session id covers the pattern-rebuild case, factorize()
    // advancing the epoch covers every numeric rebuild, and the two
    // together are exactly the object-identity half of hven's
    // (pattern_hash, session_id, epoch) naming triple (the pattern member
    // is carried by `structural_hash`, condition (a)). Condition (e) --
    // see QpEngine::run()'s reuse_eligible and the OWNERSHIP note above.
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
    // value unchanged -- see types.h's SolveOverrides and the header
    // contract's PHASE-3 DESIGN FRICTION note -- so this is byte-identical
    // to the pre-Task-2 engine.
    QpSolution solve(const QpProblem &qp) const {
        return run(qp, nullptr, false, SolveOverrides{});
    }

    // Warm start from `seed`'s point and working set (Task 10 tunes this;
    // the loop itself is identical). Same default-overrides forwarding as
    // above.
    QpSolution solve(const QpProblem &qp, const QpSolution &seed) const {
        return run(qp, &seed, true, SolveOverrides{});
    }

    // Cold start with a per-solve override of tr_radius/primal_delta/
    // dual_mu (see types.h's SolveOverrides). Resolved once, at the top of
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

    // PHASE-4 TASK 4: warm start with a per-solve override AND a Task-4 hot
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

    // PHASE-4 TASK 4: a shared, opaque snapshot of this engine's CURRENTLY
    // valid border-mode cache -- what warm_start.h's make_warm_start (called
    // from sqp_driver.h) attaches to WarmStart::hot on every exit. Returns
    // nullptr whenever border_valid_ is false: no solve() on this instance
    // has yet ended kOptimal (a fresh engine, an engine whose only solves so
    // far failed, or ws_algebra == kRefactorize, under which border_valid_
    // is never set at all -- see the header contract's HOT-START REUSE note,
    // "border mode only"). See HotState's own OWNERSHIP note, immediately
    // above BorderState, for what sharing the returned pointer does and does
    // not make safe.
    //
    // FIX ROUND 2 (re-review finding 2b), re-derived for M3 phase B's
    // session/epoch identity: emits `border_kkt_session_id_`/
    // `border_kkt_epoch_` (this engine's own COMMITTED pair, from its own
    // last successful run()), NEVER a LIVE `border_->kkt.factor` read off
    // the possibly-shared object. The two are equal at every ordinary call
    // site -- this engine's last commit captured the live pair and nothing
    // between then and now can have changed either -- EXCEPT when a
    // DIFFERENT engine sharing `border_` has since rebuilt it (exactly the
    // sharing this handle exists to support). Emitting the live pair there
    // would mint a SELF-CONSISTENT FORGED HANDLE: a snapshot whose
    // fingerprint fields describe this engine's own last problem (correct)
    // but whose identity matches the object's CURRENT, foreign-mutated
    // state (wrong) -- so a later adopter's condition (e) would pass on an
    // object this engine itself never certified as representing that
    // identity. Demonstrated at this API directly (re-review's R3 probe,
    // retargeted in tests/sqp/test_qp_warm_start.cpp): P emits h1 (epoch
    // E); a second engine C adopts h1 against a value-consistent problem
    // (its own conditions (a)-(d) pass, so it is not detached -- see
    // HotState's OWNERSHIP note for why that is the only way a shared
    // object is ever mutated) and needs one mid-solve rebuild for reasons
    // unrelated to any hash, advancing the SHARED object's epoch; P --
    // which never solved again -- then calls hot_state() a SECOND time
    // and, reading the live pair, would emit h2 claiming the advanced
    // epoch for a fingerprint that has been sitting at E the whole time.
    // Emitting the committed pair (still E) instead means h2 == h1 in
    // every field that matters, and a later adopter's condition (e) is
    // judged against what P actually last certified, not against whatever
    // the object happens to read right now.
    std::shared_ptr<const HotState> hot_state() const {
        if (!border_valid_) {
            return nullptr;
        }
        return std::make_shared<const HotState>(
            HotState{border_, border_structural_hash_, border_values_hash_, border_effective_delta_,
                     border_effective_mu_, border_exit_bound_state_, border_exit_active_ineq_,
                     border_kkt_session_id_, border_kkt_epoch_});
    }

    // =====================================================================
    // TIER 3: EXACT REFINEMENT ON AN EXTERNALLY IDENTIFIED FACE
    // (Phase-7 Task 5, fix round 1)
    // =====================================================================
    //
    // ONE exact equality-constrained solve on the face `face` names, plus this
    // engine's ordinary iterative-refinement step -- i.e. `solve_eqp`, the same
    // function the walk's own per-minor `eqp_candidate` calls, reached here
    // WITHOUT a walk. It is the "stable-face refinement" of
    // docs/superpowers/specs/2026-08-05-phase-7-design.md's tier 3: a kernel
    // that IDENTIFIES an active set to its own tolerance (today: the
    // semismooth-Newton tier, ssn_engine.h) hands that set here and gets back
    // the point the set determines EXACTLY, rather than the point its own
    // residual tolerance was willing to stop at.
    //
    // WHY THIS IS THE PRINCIPLED REPAIR AND NOT A POLISH STEP. sqp_driver.h's
    // WHAT IS MEASURED BUT NOT GATED note declines to gate NLP complementarity
    // on the grounds that the SUBPROBLEM's own complementarity is an EXACT
    // identity -- a row outside the working set is simply absent from the KKT
    // system, so its price is zero to machine precision, and a row inside it is
    // driven to `Ai_j p = bi_j`. That identity is a property of an ACTIVE-SET
    // solve, not of a residual-tolerance solve: an FB kernel stopping at
    // |phi| <= fb_tol certifies only `min(s, lambda) = O(fb_tol)`, so its
    // per-row complementarity is bounded by `fb_tol * ||lambda||inf`, which
    // does not vanish with the step. Re-solving the identified face here
    // restores the identity by construction, because this solve is the
    // active-set solve the note is written about.
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
    // **PUBLIC-API PRECONDITION** (final branch review WAVE #16, T5 NF-6):
    // the trust-region gate below assumes its window is centred at
    // `clamp(0, l, u)` -- true today only because every seeding site zeroes
    // `seed.x` before it reaches this function, so `start_center()`'s own
    // centre agrees with the clamped origin. This function takes no centre
    // parameter and does NOT validate that assumption itself. A caller that
    // seeds from a non-zeroed point (a future non-driver caller, or tycho
    // post-migration) gets a window gated about the wrong centre, silently.
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
    // Resolve one call's SolveOverrides against an engine's QpOptions, into
    // the effective values every tr_radius/primal_delta/dual_mu read site
    // consults from there on (types.h's SolveOverrides; the header contract's
    // PHASE-3 DESIGN FRICTION note). Extracted from run() in Phase-7 Task 5's
    // fix round 1 so that refine_on_face() cannot resolve them differently.
    //
    // VALIDATE FIRST. tr_radius must be the +inf sentinel or >= 0: a negative,
    // non-sentinel Delta silently crosses lo_eff/up_eff -- section 6's CROSSED
    // EFFECTIVE BOUNDS CANNOT HAPPEN proof assumes Delta >= 0, an assumption
    // that was closed while tr_radius lived only on the per-instance-const
    // opts_ but is now a LIVE hazard, since Delta is per-solve driver input (a
    // shrink loop can pass one straight through from its own arithmetic). The
    // `assert` guarding that proof is compiled OUT under NDEBUG, so without
    // this check a negative override degrades silently into a crossed effective
    // box in a Release build rather than failing loudly: probed at
    // tr_radius = -0.5 on a [-2,2]^2 box, which returns kOptimal at (-0.5, 0)
    // with no diagnostic at all in Release (Debug's assert does catch it).
    // primal_delta/dual_mu keep their documented negative-means-sentinel
    // convention (types.h) unchanged -- only NaN is rejected for them, since
    // NaN is not a value either field's resolution ternary (or any downstream
    // arithmetic) can absorb.
    //
    // Every OTHER field of `opts` is carried through untouched (feas_tol,
    // opt_tol, max_iter, schur_cap, schur_cond_max, ws_algebra), which is what
    // makes the result safe to pass anywhere a whole QpOptions was passed.
    static QpOptions resolve_effective_options(const QpOptions &opts,
                                               const SolveOverrides &overrides) {
        if (std::isnan(overrides.tr_radius) || overrides.tr_radius < 0.0) {
            throw std::invalid_argument(fmt::format(
                "QpEngine::solve: SolveOverrides.tr_radius must be >= 0, or the +inf sentinel "
                "for \"use opts_.tr_radius\" (types.h) -- got {}",
                overrides.tr_radius));
        }
        if (std::isnan(overrides.primal_delta)) {
            throw std::invalid_argument(
                "QpEngine::solve: SolveOverrides.primal_delta must not be NaN (negative is the "
                "documented sentinel for \"use opts_.primal_delta\" -- see types.h)");
        }
        if (std::isnan(overrides.dual_mu)) {
            throw std::invalid_argument(
                "QpEngine::solve: SolveOverrides.dual_mu must not be NaN (negative is the "
                "documented sentinel for \"use opts_.dual_mu\" -- see types.h)");
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
        // PHASE-4 TASK 4: ADOPT AN EXTERNAL HOT HANDLE. `hot` (WarmStart::
        // hot, opaque -- see this file's HotState) is a snapshot possibly
        // produced by a DIFFERENT QpEngine instance's hot_state(); see that
        // method and the OWNERSHIP note above HotState/BorderState for what
        // sharing it does and does not make safe. Adopted ONLY when this
        // engine's OWN instance-level cache is not already trustworthy
        // (border_valid_ false): a fresh engine, or one whose last solve
        // did not end kOptimal. An engine that already has valid state of
        // its own always prefers it -- that state already describes what
        // THIS instance has actually been solving, which a caller-supplied
        // handle from elsewhere cannot improve on and could only clobber.
        // Adoption grants nothing beyond making this engine's border_/hash/
        // exit-state members look exactly like the handle's snapshot; the
        // pessimistic-invalidation snapshot immediately below
        // (prev_border_valid etc.) reads them right back out, so the
        // reuse-eligibility check below (conditions (a)-(e) -- (e), the
        // identity conjunct, is this fix round's own addition, retargeted
        // by M3 phase B onto session/epoch + usable numerics; (a)-(d)
        // predate it and are unchanged) is the only thing that decides
        // whether adopting it actually skips a factorization on THIS call --
        // this block adds no new invalidation logic of its own. ws_algebra ==
        // kRefactorize never adopts: the border-mode cache does not exist
        // in that mode, hot_state() never emits a non-null handle built
        // from it, and a handle from elsewhere is meaningless here.
        //
        // FIX ROUND 1 (retargeted onto the session/epoch identity, M3 phase
        // B) also copies `hot->kkt_session_id`/`hot->kkt_epoch` into the
        // committed members here, alongside the four pre-existing fields --
        // the pair is adopted exactly like they are, and is what lets the
        // reuse gate below (its condition (e)) tell whether the OBJECT
        // `border_` now points at still carries the numerics this identity
        // describes, not merely whether the PROBLEM still looks the same.
        // See HotState's OWNERSHIP note for why the problem-shaped fields
        // alone cannot make that distinction.
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
        // FIX ROUND 1 (retargeted, M3 phase B): this engine's own
        // last-trusted (session_id, epoch) pair for whatever object
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
        // site below actually consults from here on (see types.h's
        // SolveOverrides and the header contract's PHASE-3 DESIGN FRICTION
        // note). `eff_opts` is opts_ verbatim except for these three fields,
        // so passing it anywhere opts_ used to be passed as a whole
        // QpOptions is safe: every other field (feas_tol, opt_tol, max_iter,
        // schur_cap, schur_cond_max, ws_algebra) is untouched.
        //
        // VALIDATE FIRST. tr_radius must be the +inf sentinel or >= 0: a
        // negative, non-sentinel Delta silently crosses lo_eff/up_eff --
        // section 6's CROSSED EFFECTIVE BOUNDS CANNOT HAPPEN proof assumes
        // Delta >= 0, an assumption that was closed while tr_radius lived
        // only on the per-instance-const opts_ but is now a LIVE hazard,
        // since Delta is per-solve driver input (a Task-6 shrink loop can
        // pass one straight through from its own arithmetic). The `assert`
        // guarding that proof below is compiled OUT under NDEBUG, so without
        // this check a negative override degrades silently into a crossed
        // effective box in a Release build rather than failing loudly:
        // probed at tr_radius = -0.5 on a [-2,2]^2 box, which returns
        // kOptimal at (-0.5, 0) with no diagnostic at all in Release (Debug's
        // assert does catch it). primal_delta/dual_mu keep their documented
        // negative-means-sentinel convention (types.h) unchanged -- only NaN
        // is rejected for them, since NaN is not a value either field's
        // resolution ternary (or any downstream arithmetic) can absorb.
        // EXTRACTED, FIX ROUND 1 of Phase-7 Task 5, so that refine_on_face()
        // resolves the SAME per-solve overrides through the SAME code rather
        // than a second copy that could drift. The body below is the original
        // three checks and three ternaries verbatim, in the original order.
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
        // is what makes that path bit-identical to the pre-Task-9 engine:
        // `qp` below aliases `qp_in` directly, with nothing new allocated.
        std::vector<bool> tr_active(static_cast<std::size_t>(n), false);
        QpProblem tr_problem;
        const bool tr_enabled = std::isfinite(eff_opts.tr_radius) && !crossed_bounds;
        if (tr_enabled) {
            // TODO(Phase 5 perf pass): this deep-copies H/Ae/Ai/g/be/bi every
            // solve purely to get a place to put lo_eff/up_eff -- none of
            // those other fields ever differ from qp_in's. Cheaper fix
            // directions if this ever shows up in a profile: thread
            // lo_eff/up_eff as separate parameters into the handful of
            // functions that read bounds instead of shadowing a whole
            // QpProblem, or give the engine a persistent scratch QpProblem
            // (an instance member, resized/refilled in place rather than
            // reassigned) that only the bounds are ever written into. Not
            // done now: this task's bit-identical requirement binds only the
            // tr_radius == +inf path, which this copy is never on.
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
        // rather than after it (its position until Phase-6 Task 3) for one
        // reason: that call is itself a source of counters.shift_adds -- the
        // seed's own homotopy admission -- and QpCounters documents that the
        // pre-loop call is included. Nothing else reads it this early.
        detail::KktFactor kkt;
        QpCounters counters;
        WalkSeen seen{std::vector<std::uint8_t>(static_cast<std::size_t>(mi), 0),
                      std::vector<std::uint8_t>(static_cast<std::size_t>(n), 0)};

        refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);

        // Decide whether border_ (the engine-instance persistent K0/border
        // state) may be trusted for THIS solve, per the header contract's
        // HOT-START REUSE conditions (a)/(b)/(c)/(d) PLUS FIX ROUND 1's (e)
        // below. `ws` here is exactly the seed working set condition (b) is
        // stated over: start_center() plus ingest_seed_working_set() plus
        // the refresh_shifts() call just above -- so a hint the
        // WINDOW-CONSISTENCY RULE dropped is absent here too, and reuse
        // eligibility is judged on the working set this solve will actually
        // start from, before the loop below runs its first iteration. When
        // any condition fails, border_ is DETACHED onto a brand-new
        // BorderState of this engine's own -- as if this were the engine's
        // first solve ever -- rather than the previous object being patched
        // in place, so the ordinary rebuild/sync machinery below runs
        // unmodified AND (FIX ROUND 1) so a refused adoption never mutates
        // an object some other holder may still be relying on; see the
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
            // note. Before per-solve overrides existed this pair could never
            // change across calls on one instance (opts_ being const), so it
            // needed no entry here; now it can, and K0's values depend on it
            // exactly as they depend on H/Ae/Ai.
            //
            // Condition (e), FIX ROUND 1 retargeted onto the factor's own
            // identity (M3 phase B, docs/retarget-design-sqp.md SS7):
            // `border_`'s factor's OWN live (session_id, epoch) -- the
            // session id moved by analyze() at a pattern rebuild, the epoch
            // advanced by every successful factorize(), whichever engine
            // drives it -- must equal what THIS engine last saw as
            // trustworthy for whatever object `border_` currently names.
            // Conditions (a)-(d) alone describe the PROBLEM, never WHICH
            // OBJECT `border_` currently names, and a DIFFERENT engine
            // sharing this same BorderState (via a HotState hand-off) can
            // rebuild it for its own problem while every one of (a)-(d)
            // still matches -- the SYMMETRIC ordering (adopt, then the
            // OTHER holder mutates it, then solve again on THIS engine):
            // this engine's own four problem-shaped fields never changed,
            // but the object did, and only a live read of the object's own
            // identity, on EVERY solve() call rather than only at adoption,
            // sees that. FIX ROUND 2's adjudication (HotState's own
            // OWNERSHIP note): whether this conjunct alone ever prevents a
            // WRONG ANSWER, as opposed to an unnecessary-but-harmless
            // rebuild, is not demonstrated by this file's own fixtures --
            // DETACH (below) is what the evidence shows is load-bearing;
            // this conjunct is defense-in-depth kept for the same reason
            // any cheap, independent check earns its keep.
            //
            // The USABLE-NUMERICS conjunct (SS7.2, the ordered-pin closure):
            // an epoch does not advance on a FAILED factorize, so after
            // another holder's failed rebuild a stale (session_id, epoch)
            // pair can still MATCH an object whose numerics are
            // invalidated -- reuse would then throw at the first solve
            // where the old generation stamp detached and rebuilt
            // gracefully. `inertia().state == kObserved` is false precisely
            // when the last factorize failed or none happened (pinned at
            // hven: tests/linear/test_fault_injection.cpp's
            // FailedFactorizeEvidencePin), restoring detach-on-mismatch as
            // the failure mode.
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
            // FIX ROUND 1 (Q5): the direct "did the gate pass" signal --
            // see QpCounters::k0_reused's own note for why a caller must
            // read THIS rather than infer reuse from `factorizations == 0`.
            counters.k0_reused = reuse_eligible;
            if (!reuse_eligible) {
                // FIX ROUND 1 (Q3): DETACH rather than wipe in place --
                // `border_` may be the SAME shared object another engine
                // still holds (via its own copy of a HotState); clearing
                // schur/latched/k0_rows/ledger on it -- the pre-Task-4 code
                // -- still leaves `k0`/`kkt` stale until the next
                // rebuild_k0() call, which then refactorizes THAT SHARED
                // OBJECT for THIS engine's problem, silently destroying
                // whatever the other holder was relying on.
                //
                // FIX ROUND 2 (re-review 2c): DETACH ONLY WHEN ACTUALLY
                // SHARED. Round 1's report claimed detaching unconditionally
                // was "behaviourally identical to the old wipe for the
                // never-shared case, since nothing else could ever have
                // observed the difference there" -- MARKED AS FALSE by the
                // re-review, and measured: factorize_checked() (kkt_calls.h)
                // skips the backend's symbolic analysis only when
                // needs_analysis(K) is false on the SAME KktFactor object,
                // so a fresh BorderState -- a fresh KktFactor, analyzed ==
                // false -- pays a FULL symbolic re-analysis
                // on every value-changing major of an ORDINARY,
                // NEVER-SHARED solve, which is most majors of a real SQP run
                // (H changes almost every major). Measured before this
                // guard existed: HS38 48 phase-11 analyses per solve vs 1
                // pre-Task-4, HS26 17 vs 1, HS77 10 vs 1 -- invisible on
                // this project's toy KKT systems, squarely material at
                // tycho scale where the symbolic analysis is the whole
                // reason the pattern cache exists.
                //
                // `border_.use_count()` -- how many shared_ptr copies of
                // THIS object exist, this engine's own included -- is
                // exactly the discriminator the correctness argument above
                // (and HotState's OWNERSHIP note) already needs: a shared
                // object may only ever be safely mutated by an engine whose
                // OWN gate just passed for it (never true in this branch),
                // so `use_count() > 1` here means some OTHER holder is
                // relying on this object unchanged and it must be detached,
                // exactly as Round 1 shipped. `use_count() == 1` means this
                // engine is the ONLY owner -- nothing else can ever have
                // observed a difference here, the case Round 1's claim
                // should have been scoped to -- so the SAME object's
                // border-stack fields are wiped in place instead, letting
                // the next rebuild_k0() reuse this KktFactor's still-live
                // symbolic analysis when the sparsity pattern has not
                // changed. `use_count()` is reliable here under this
                // file's own documented sequential-use contract (THREAD
                // SAFETY, above); over-detaching if any doubt existed would
                // be the safe direction, but no such doubt exists under
                // that contract.
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

        // PHASE-6 TASK 4 (M6). The cap this solve runs at: an explicitly set
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
            // trigger (findings-doc policy) and a wrong inertia LATER in the
            // run is a mid-solve negative-curvature event, which Task 7's
            // ride handles.
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
                    // by Task 6b's restart: it was computed by eqp_candidate at
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
                        // after a probe-driven drop is the dead end Task 1
                        // measured and could not leave: the drop exposed real
                        // negative curvature, but its multiplier was zero, so
                        // section 4c's arming direction p is identically zero
                        // and the ride is never offered anything to ride. What
                        // DOES work -- measured in Task 1's review, by feeding
                        // this very exit back in as a warm seed -- is the
                        // temporary-vertex repair, which is gated on iter == 0
                        // and so cannot be reached from the iteration a probe's
                        // drop resumes into. This spends ONE such repair
                        // mid-solve, here and nowhere else, and only after
                        // every cheaper way out (structural infeasibility,
                        // runaway, the drop rule) has been exhausted. If it
                        // fails, or the budget is gone, the certification
                        // branch's original verdict stands.
                        //
                        // `probe_drop_made` IS SCOPING, NOT A MEASURED
                        // NECESSITY, and that is recorded rather than implied:
                        // deleting that conjunct leaves the whole suite green
                        // (measured, Task 6b), because every OTHER shipped
                        // fixture that reaches this branch does so with an
                        // UNPINNABLE free variable, where repair_temporary_vertex
                        // declines anyway. It is kept because the case this
                        // restart is justified by -- and measured on -- is the
                        // one section 4c is structurally unable to serve, and a
                        // budget of one should be spent there rather than on a
                        // declined ride, which has its own route.
                        //
                        // AND IT IS STICKY PER SOLVE, NOT PER DROP. Once a
                        // probe drop has been made anywhere in this solve, the
                        // bit stays set, so a LATER dead end reached from a
                        // drop_worst drop whose ride declined can spend the
                        // (still unspent) restart. That is weaker than the
                        // intent stated above, section 4c's cross-reference
                        // says so in the same words, and narrowing it -- clear
                        // the bit when the restart fires, or when a drop_worst
                        // drop intervenes -- is a change no shipped fixture
                        // distinguishes. Left as-is because the narrower rule
                        // buys nothing measurable and the wider one is still
                        // SOUND: an unspent repair either reaches a
                        // second-order-consistent working set or unwinds
                        // itself, so the worst case is one wasted
                        // factorization.
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
                        // fiction (audit finding D9).
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
                                // branch above uses (FIX ROUND 1 Q3 + FIX
                                // ROUND 2's shared-only guard): `border_` may
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
                                // green (measured, Task 3b), because on every
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
            // Condition (e), FIX ROUND 1 retargeted (M3 phase B): commit
            // `border_`'s factor's OWN live (session_id, epoch) identity as
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

        // THE EXPORT INVARIANT: A FREE VARIABLE CARRIES NO BOUND PRICE
        // (Phase-7 Task 6b, docket D0 option 3 -- Grant-ruled).
        //
        // WHAT WAS WRONG. `price()` is the sole producer of `z`, and it
        // already imposes exactly this rule -- z(i) is written only where
        // ws.bound_state()[i] != kFree -- but it runs at the TOP of a minor
        // iteration, and the working set is mutated AFTER it: the drop rule
        // frees a pin (drop_worst), the zero-multiplier probe frees one, the
        // ratio test adds one. On every exit the loop reaches by CONVERGING,
        // the last mutation is followed by another `price()`, so the pair is
        // consistent by construction. On a `kMaxIter` exit -- the loop simply
        // running out of minors -- it is not: the export carries a price
        // computed against a working set that no longer exists. The battery
        // caught exactly that (docs/notes/2026-08-08-ssn-gate-battery.md):
        // variable 1882 exported `z = +0.173` while reporting
        // `bound_state = kFree, tr_active = false`, which is the condition
        // `nlp_kkt_check.h` calls `dual_sign` and this header's own section-2
        // labeling note states as a fact ("reported kFree with z == 0").
        //
        // WHY THIS DIRECTION AND NOT THE OTHER. Reconciling the other way --
        // re-labelling `bound_state` to match whatever `z` happens to hold --
        // would FABRICATE a pin the walk did not end on, and `bound_state` is
        // read far more widely than `z` is (the hot-start reuse ledger, the
        // driver's activity export, WarmStart ingest). Restoring the priced
        // set to the working set is the direction that ADDS no information:
        // it re-imposes `price()`'s own rule at the point of export, costs
        // O(n) and no factorization, and cannot invent activity.
        //
        // WHY IT IS UNCONDITIONAL RATHER THAN `if (status == kMaxIter)`. The
        // invariant is a property of the EXPORT, not of a status; stating it
        // once here is what makes it checkable. On the other three statuses it
        // finds nothing to do, and that is an argument from the control flow
        // rather than from a measurement: kInfeasible/kNumericalError zero `z`
        // outright two lines above; and between the `price()` call at the top
        // of an iteration and the ONLY assignment of kOptimal, the sole writes
        // to ws.bound_state() are refresh_shifts (which adds inequality rows
        // and writes no bound state) and probe_zero_multiplier_drops, which on
        // the `false` return that is the only route to that assignment has
        // already restored every bound it tentatively released -- so no bound
        // is freed after the last pricing.
        //
        // WHAT THE BYTE-IDENTITY EVIDENCE DOES AND DOES NOT SAY. The full
        // suite in both configurations, the 57-cell walk census and
        // `bench_scale --self-check` are all unmoved by this edit -- but under
        // kWalk that shows the field is UNREAD (sqp_driver.h never reads
        // QpSolution::z), not that the clear never fires. The no-op claim
        // above rests on the control-flow argument; the byte-identity evidence
        // is what rules out a downstream consumer.
        //
        // It runs BEFORE section 6's TR exclusion below, which only ever
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
    // WHY THE SPLIT EXISTS (fix round 1, C1). Section 6 computes the
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
    // C1 defect described above.
    //
    // A hint INSIDE the window is honoured EXACTLY as before, and that is the
    // common case a warm chain depends on: when the seed's own x is carried
    // (not zeroed), the window is centered on that x, so a bound the previous
    // solve was sitting at is at distance 0 from the center and is always
    // inside. ShrinkRadiusRetryWithALiveRealBoundPinChainsAndReuses is the
    // regression guard for that path, and the whole Phase-2 warm-start
    // battery is the guard for the Delta == +inf path, where the effective
    // bounds ARE the real bounds and this function is bit-identical to the
    // pre-split code.
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
    // PHASE-6 TASK 3 FIX ROUND 1, OBSERVATION ONLY. Which constraints this
    // solve has EVER admitted, so QpCounters::distinct_{ineq,bound}_added can
    // report re-discovery directly instead of leaving it to a pigeonhole
    // argument that (the review found) does not close. Two byte vectors, sized
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
        // EXTRA steps only, i.e. identically 0 (types.h): the eliminated path
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
        // implies rebuild_k0 completed). Rewritten per
        // docs/retarget-design-sqp.md SS4.2 (approved): rebuild on
        // non-kObserved evidence (defensive, unreachable today), on a
        // present-and-nonzero perturbed-pivot count (the old MKL rule), or
        // on a NATIVELY-OBSERVED nonzero zero class -- the dissolved
        // Accelerate twin's zero-pivot trust signal, preserved through the
        // honest channel (inert on MKL, where the zero class is derived).
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
            // TOTAL steps, mandatory first one included (types.h).
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
    //
    // The inertia gate tests the null space of the FULL active labeling, so a
    // WEAKLY active constraint -- in the working set, multiplier (numerically)
    // zero -- hides whatever curvature lives in the direction it alone
    // excludes. The true critical cone there is the null space of the working
    // set WITHOUT it. This walks exactly those constraints, one at a time,
    // MOST RECENTLY ADDED FIRST, tentatively drops each and re-runs the gate on
    // the reduced labeling:
    //
    //   kWrong   the reduced reduced-Hessian is trustworthily NOT positive
    //            definite: the hidden negative curvature is real. The drop is
    //            made REAL (`dropped` is filled in exactly as drop_worst fills
    //            it, which arms section 4c's ride on the next iteration) and
    //            this returns true, so the caller RESUMES the loop instead of
    //            certifying.
    //   kOk      that constraint hides nothing. Restore it and try the next.
    //   kSuspect the reduced inertia is UNKNOWN (perturbed pivots, or a border
    //            stack past needs_refactorization). Treated exactly as kOk --
    //            restore and move on -- per the findings-doc policy that a
    //            fabricated pivot sign is never ground truth. It is the one
    //            place this fix stays silent; see section 4b.
    //
    // Returns false if every candidate came back kOk/kSuspect (or there were
    // none at all), leaving `ws` exactly as it found it and the caller free to
    // certify.
    //
    // ONE AT A TIME is an approximation, and a deliberate one: a cone opened
    // only by dropping TWO weakly active constraints simultaneously is not
    // covered. See section 4b for why, and for what checks it.
    //
    // COST WHEN NOTHING IS WEAKLY ACTIVE IS ZERO: the candidate list is built
    // from multipliers the loop already priced, and an empty list returns
    // before any linear algebra runs. Under strict complementarity -- every
    // convex fixture in this file's battery -- the list is always empty.
    // `opts` is the effective options this solve resolved (see run()'s
    // `eff_opts`). opt_tol itself is unaffected by SolveOverrides, but the
    // opt_tol reads below use `opts.opt_tol` rather than the `opts_` member
    // anyway -- purely for consistency (this function already has `opts` in
    // scope and reading the member instead would be a shadowing trap the day
    // opt_tol becomes overridable) -- and `opts` is threaded through to
    // probe_inertia -> eqp_candidate below regardless, which does need the
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
    // PHASE-5 TASK 4 REMOVED A SECOND RELEASE CONDITION, and the measurement
    // that removed it is the whole content of
    // docs/notes/2026-07-31-schur-cap-policy.md. This function used to release
    // the latch ALSO whenever `ws.active_ineq() != border.k0_rows` -- "the
    // working rows changed, so a rebuilt K0 would differ from the one the
    // latch was taken against and can absorb them" -- on the argument that
    // "in the worst case the latch releases and re-takes once per iteration,
    // which is the cost of refactorize mode -- never worse". THAT ARGUMENT IS
    // FALSE, and the worst case is the ORDINARY case on a collocation QP:
    //
    //   * A release is NOT the cost of one refactorize iteration. It costs a
    //     full-variable assemble_kkt_full + Pardiso factorization (whose
    //     sparsity pattern changed, so phase-11 symbolic analysis too) + a
    //     sync_borders that re-adds EVERY pin (one K0 solve and an O(dim^2)
    //     dense C rebuild apiece) + the elimination-path factorization the
    //     iteration needed anyway. Refactorize mode pays only the last, on a
    //     SMALLER bound-eliminated matrix.
    //   * The rebuild it buys is futile by construction: the state it resumes
    //     into has dim() == pinned > schur_cap, so needs_refactorization() is
    //     true again before anything is solved and the latch is retaken on the
    //     same iteration. Absorbing the new rows changes nothing, because the
    //     rows were never what breached the cap.
    //   * A working set whose rows move every iteration -- an active-set
    //     identification sweep, i.e. what an SQP subproblem on a path-
    //     constrained collocation NLP does for thousands of minors -- therefore
    //     thrashed the latch once per row change. MEASURED on F7 at N = 150,
    //     p = 0.85, schur_cap 128: 264 releases and 264 re-takes in a
    //     1276-minor solve, 853 factorizations, 41005 border operations, 274
    //     symbolic analyses, 87 s. With this release condition gone: the SAME
    //     1276 minors and the SAME answer to the last bit, ONE release, 590
    //     factorizations, 1622 border operations, 12 symbolic analyses, 4.8 s.
    //     At N = 200 the same change turns "did not finish in 13 minutes" into
    //     31 s.
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
    // from opts.primal_delta/opts.dual_mu, which is exactly what this task's
    // reuse-key extension (condition (d)) tracks.
    //
    // THIS IS THE SOLE SITE that reassigns `border.k0` or factorizes
    // `border.kkt` -- i.e. the sole site that changes what K0 actually IS.
    // M3 phase B: it no longer stamps anything -- the FIX ROUND 2
    // bump-before-mutate `++border.generation` that used to open this
    // function is deleted, because `factorize_checked()` advancing the
    // factor's epoch (and `analyze()` moving its session id at a pattern
    // change) IS the stamp, for EVERY caller, engine-own or shared.
    // sync_borders() below only ever adds/drops BORDERS around whatever K0
    // this call last built, it never touches `k0`/`kkt`, so it correctly
    // moves neither. The defensive role generation's early bump played on a
    // FAILED rebuild (a throw below leaving the object mutated but the
    // stamp already moved) is carried by condition (e)'s usable-numerics
    // conjunct instead: a failed factorize leaves `inertia().state !=
    // kObserved`, so every stale holder refuses reuse on that ground (see
    // run()'s reuse gate and docs/retarget-design-sqp.md SS7.2).
    void rebuild_k0(const QpProblem &qp, const WorkingSet &ws, BorderState &border,
                    QpCounters &counters, const QpOptions &opts) const {
        border.k0 = assemble_kkt_full(qp, ws, opts);
        border.k0_rows = ws.active_ineq();
        // FIX ROUND 2 (QpCounters::symbolic_analyses' own note): decided
        // BEFORE the factorize, which is the only way to know whether THIS
        // call is about to pay the backend's symbolic analysis --
        // factorize_checked() acts on the decision but does not report it
        // back. M3 phase C (B2): the decision is TAKEN here and HANDED to
        // factorize_checked(), rather than taken here and taken again in
        // there, so one factorization costs one pattern hash at this layer
        // and not two (kkt_calls.h's AnalysisDecision).
        const detail::AnalysisDecision analysis =
            detail::analysis_decision(border.kkt, border.k0.K);
        if (analysis.needed) {
            ++counters.symbolic_analyses;
        }
        detail::factorize_checked(border.kkt, border.k0.K, analysis);
        ++counters.factorizations;
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
            border.schur->add_border(BorderOps::delete_k0_row(k, me, n, n0), 0.0);
            ledger.push_back({BorderLedgerEntry::Kind::kRowDelete, k});
            ++counters.schur_updates;
        }
        for (const Index row : ws.active_ineq()) {
            if (std::binary_search(border.k0_rows.begin(), border.k0_rows.end(), row) ||
                has_border(ledger, BorderLedgerEntry::Kind::kIneqRow, row)) {
                continue;
            }
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
    //                qualify. This is what makes the ride progress: combined
    //                with non-positive curvature it says the objective
    //                decreases monotonically along the whole ray, so the step
    //                to the blocker strictly improves and the working set
    //                cannot be revisited at the same objective.
    //   FEASIBILITY  the chosen sign moves OFF the constraint the drop rule
    //                just released, into its feasible side. Its slack is
    //                exactly zero, so the other sign is answered by the ratio
    //                test with alpha = 0 and an immediate re-add -- the
    //                pre-ride cycle this section exists to break.
    //
    // At a KKT point of the PRE-drop working set the two provably agree: with
    // g_x = -A_w' lambda there, the derivative along a direction q with
    // a_c . q = -1 and A_w' q = 0 is exactly lambda_c, which the drop rule made
    // negative. They can nonetheless disagree in practice, and the case is not
    // hypothetical: on a DEGENERATE working set (more active constraints than
    // dimensions -- the ratio test and the ride can both add rows without a
    // rank test, and the header's Degeneracy note already allows it) the
    // multipliers merely split among dependent rows, so "lambda_c < 0" stops
    // implying that moving off c improves anything. Measured on a randomized
    // 3-variable indefinite battery, riding on the feasibility condition alone
    // there produces ASCENT steps that bounce a variable between its two
    // bounds until max_iter.
    //
    // So a disagreement DECLINES: the iteration falls through to the ordinary
    // EQP step, i.e. to exactly the behavior this engine had before the ride
    // existed. Declining is never worse than the baseline, where riding on a
    // wrong sign demonstrably is.
    //
    // A FLAT DIRECTION ALSO DECLINES, and this is load-bearing rather than
    // fastidious. An earlier revision let the freed constraint pick the sign
    // on its own when the slope was within tolerance of zero, on the theory
    // that "zero curvature plus zero slope is still not an ascent". It is not
    // an ascent, but it is not PROGRESS either, and riding it is strictly
    // worse than not: the step buys no objective decrease at all while still
    // pinning a blocker into the working set. Measured on a FEASIBLE,
    // exactly-PSD 5-variable convex QP (see
    // QpEngineIndefinite.RideDeclinesAFlatDirectionOnAPsdSingularQp) that
    // gratuitous pin left the next EQP with a ~1e-6 row residual (measured
    // 1.011e-06), which step 5's classifier read as a structural violation and
    // reported kInfeasible on a problem whose optimum the pre-ride engine
    // returned correctly. It also contradicted section 4c's own anti-cycling
    // argument, which rests on the decrease being STRICT. Requiring strict
    // descent removes the whole class: measured on that battery, the
    // kOptimal -> kInfeasible and kOptimal -> kMaxIter populations both go to
    // zero and every ride benefit is retained. That last clause is
    // BATTERY-SPECIFIC, not a general claim -- an independent battery traded
    // away two kMaxIter -> kOptimal decisiveness gains (same objective, so no
    // answer was worsened; the solve simply stopped terminating). Declining a
    // flat direction can cost decisiveness; it is chosen anyway because riding
    // one can cost correctness.
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
    // PHASE-6 TASK 3 FIX ROUND 1: `counters` is written and never read here --
    // `tied` counts, EXACTLY, how many candidates share the final minimum
    // ratio, and the count is exact for free because of how the scan runs. Any
    // candidate equal to the FINAL best that were seen before the last strict
    // decrease is impossible (it would have set `best` to that value earlier,
    // making the later decrease non-strict), so resetting `tied` on every
    // strict decrease and incrementing it on every exact equality leaves
    // `tied` equal to the true multiplicity of the final minimum. No second
    // pass, no tolerance, and SELECTION IS UNTOUCHED -- `best`, `kind`,
    // `block_idx` and `block_state` are assigned by exactly the comparisons
    // they were before.
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
    // `eff_opts` there) rather than read directly by most of the file below.
    // PHASE-3 DESIGN FRICTION note above this class explains why tr_radius/
    // primal_delta/dual_mu no longer need to vary this field to vary
    // per-solve. Before SolveOverrides existed, this same constness is what
    // licensed the HOT-START REUSE fingerprints omitting primal_delta/dual_mu
    // entirely (they could never change across calls on one instance, so K0's
    // dependence on them could never change out from under a cached
    // fingerprint); now that a solve's EFFECTIVE pair can vary call-to-call,
    // border_effective_delta_/border_effective_mu_ below are what closes that
    // gap (condition (d) in the HOT-START REUSE note) -- opts_ itself stays
    // const and untouched.
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
    // PHASE-4 TASK 4: held through a std::shared_ptr, not by value, so that
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
    // Condition (e), FIX ROUND 1 retargeted onto the factor's identity (M3
    // phase B; HotState's OWNERSHIP note has the full argument): this
    // engine's own last-trusted (session_id, epoch) pair of whatever object
    // `border_` currently names. Compared against that object's LIVE
    // `kkt.factor.session_id()`/`epoch()` -- not against another frozen
    // copy -- at the top of every run() call, which is what detects a
    // DIFFERENT engine (reached via a HotState hand-off) having rebuilt the
    // shared object in the meantime, even when every other fingerprint
    // above still matches this solve's own problem. Joined there by the
    // SS7.2 usable-numerics conjunct, which covers the failed-rebuild case
    // no epoch advance records.
    mutable std::uint64_t border_kkt_session_id_ = 0;
    mutable std::uint64_t border_kkt_epoch_ = 0;
};

} // namespace hven::solvers
