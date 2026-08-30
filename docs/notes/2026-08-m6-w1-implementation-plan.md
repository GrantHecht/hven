# W1 implementation plan v3 — the IPQP tier (IP-PMM), with W3 folded in

Date: 2026-08-30 (v3, final round; supersedes same-day v1/v2 after Codex
reviews `.superpowers/w1-plan-codex-r1.md` / `w1-plan-codex-r2.md` and the
settler rulings — §8 lists the deltas). Branch: m6 (head 40aa1f5). Spec of record:
`docs/notes/2026-08-m6-w1-ipqp-spec.md` (v2, APPROVED AS AMENDED, owner
2026-08-27); ruling record: `docs/notes/2026-08-m6-ledger.md` "W1 DESIGN RULED"
entry (ledger :250-290). Both bind; the spec wins any disagreement, except
where a §7 dated amendment note below records a settler-ruled change. Line
references verified at head 40aa1f5.

## §0. What W1 delivers, and what it does not

Delivers (spec §1):
- A third QP kernel `class IpqpEngine` behind `QpMode::kIpm`
  (`include/hven/drivers/sqp_types.h:37`): IP-PMM (papers-only per CLAUDE.md
  §6), Mehrotra predictor-corrector, bounds condensed into the (1,1) diagonal
  (spec §3.1).
- The ruled routing (spec §2.3): barrier → ~1e-8 → ratio-rule face threshold
  with an UNCERTAIN class → `QpEngine::refine_on_face`
  (`include/hven/detail/qp/qp_engine.h:985`) → SSN warm-grade → cold walk LAST;
  a converged IPQP iterate never re-enters the one-row walk.
- Indefinite-H per the Q5 ruling (spec §2.2): inertia gate first (right → exact
  Newton), inertia-demanded Wächter–Biegler `rho` on a current-iterate anchor
  (NOT proximal-point), monotone-per-solve ladder, REQUIRED final unregularized
  inertia read (+1 factorization), wrong read → certificate downgraded via
  `IpqpEscape::kIndefinite`.
- Symbolic-once linear algebra: `IpqpKktLayout` value scatter (the SSN
  `sync_matrix`/`value_pos_` template, `src/qp/ssn_engine.cpp:1156`); one
  `compute()` per structure epoch, one verified factorization per tier entry,
  `kAssumeAnalyzed` after (§7 note a); Ruiz inside the tier, duals unscaled on
  export, every pin on unscaled quantities (spec §4.3).
- W3, folded in (spec §5): engine-internal `IpqpSeed` cross-major restart with
  a PRESERVED driver ingest path (polish zL/zU/mu reach the tier unflattened —
  ruling 4); the §5.2 repair; the §5.3 `mu_0` clamp; §5.4 payload grades; the
  §5.5 WARM-KILL rule (clamped budget, §7 note c).
- `IpqpCounters` (spec §7 + the §7 dated counter amendments); machine-trace
  schema v0 event points; the W2 hook `certified_feasibility_fallback(...)` →
  cold walk (spec §6.4).
- One piece of shared-interface work, additive and replay-inert: the
  `KktFactorization` evidence accessor (ruling 1, T3.a). No other file outside
  the tier's own set changes behavior.

Does NOT deliver:
- No default flip, no automatic switching — `qp_mode` default stays `kWalk`
  (spec §1/§9; `sqp_types.h:576`). Every task lands inert.
- No line search/funnel/watchdog/restoration in the tier (spec §3.1.3, §3.5);
  no `QpStatus::kInfeasible` from the tier (spec §6.3;
  `include/hven/drivers/sqp_driver.h:1701`).
- No new warm currency (Q9), no `QpSolution` extension (Q8), no crash-basis
  transfer (`crash_basis_seed`, `sqp_driver.cpp:595`; spec §2.3.5).
- No W2 body (registered §6.4); no per-piece convexification (M7;
  `include/hven/detail/drivers/aggregate_eval_seam.h:81`); no Gondzio–Grothey
  correctors, LOQO oracle, or least-squares x0 (all M7).
- No move of `KktFactorization`/`max_step_to_boundary` out of
  `detail/interior/` (W5, Q1). WITHDRAWN from the reuse ledger (§7 note d):
  `BoundSet` — NLP-produced, reduced-space, relaxed-bound, fixed variables
  eliminated (`bound_set.h:19-35`), the wrong shape for the tier's full-space
  box → IPQP owns `IpqpBounds` (T4.b); and the QP residual-scale helpers —
  private members (`qp_engine.cpp:980-:1032`) or WorkingSet-bound
  (`free_block_stationarity`, `qp_engine.h:622`) → T4.d owns a new
  implementation (ruling 5).

Global evidence rule (EVERY task): default-OFF inertness proven by the 27-cell
U0-corpus counter replay (`bench/baselines/2026-08-16-u0-corpus`), both arms
(walk vs frozen pin; ssn vs pre-task rebuild), 0 differences on 36 non-wall
columns, SOLO under the W0.2-close §7 declaration (pinned core, SMT sibling
measured idle, MKL/OMP threads 1, exact-string); suite at its current 1930/0 or
better. "Replay" below means this. Wall is informational except A13/A5's
declared serial cells.

## §1. Task list

Order is landing order; §8's one-implementer rule makes tasks sequential.
**T1 — Mode and options surface (S).** Dep: none.
- Goal: `QpMode::kIpm`; `struct IpqpOptions` per the spec §9 table as amended
  by §7 note c (`ipqp_warm_iter_budget` doc carries the clamp);
  `SqpOptions::ipqp`, forwarded like the SSN levers.
- Files: `include/hven/drivers/sqp_types.h` (enum :37; struct beside the
  `Ssn*Rule` enums :68-98; member near `qp_mode` :576);
  `src/drivers/sqp_options.cpp` (`validate_sqp_options` :74 gains the
  predicates, each the negation of the acceptance condition so NaN rejects —
  that TU's stated rule).
- Interfaces: `enum class QpMode { kWalk, kSsn, kIpm };` `struct IpqpOptions
  {...};` `IpqpOptions SqpOptions::ipqp;` plus a TEMPORARY
  `validate_sqp_options` throw on `kIpm` ("not yet dispatchable"), removed in
  T6 — no half-wired state ever ships.
- Tests: one pin per predicate class. Cells: A7 groundwork. Evidence: replay.
**T2 — Counters surface (S).** Dep: T1.
- Goal: `struct IpqpCounters` — the spec §7 table plus the §7 dated amendments
  (stall-reason counters DROPPED, note b; `ipqp_declined_pinned` note e); each
  doc comment states what is counted AND excluded; `Index` zero-init; DNF
  sentinel `1e6`; `SqpCounters::ipqp` beside `ssn`; `accumulate_ipqp_counters`
  mirroring `accumulate_ssn_counters` (`src/drivers/sqp_driver.cpp:719`),
  min/max-folding alpha-min/shift-max (cf. `ssn_sign_sweep_max`,
  `include/hven/core/solver_counters.h:467`).
- Files: `include/hven/core/solver_counters.h` (models: `QpCounters` :44,
  `SsnCounters` :290, census discipline :522-527, `SqpCounters` :713);
  `src/drivers/sqp_driver.cpp` (accumulate fn only).
- Tests: zero-init; the escape-census sum invariant as a reusable helper
  (the five escape counters sum to `ipqp_escapes`; the stall-reason counters
  are DROPPED — §7 note b, escape-count only); fold pins. Cells: A7
  groundwork. Evidence: replay (dead code until T6).
**T3 — Evidence accessor + `IpqpKktLayout` + `ipqp_math.h` (M).** Dep: T1.
- T3.a (ruling 1) — the additive, evidence-preserving accessor:
  `KktFactorization` exposes only cached `peigs()/neigs()/ppivs()` ints
  (`include/hven/detail/interior/kkt_factorization.h:129-139`) and its
  `record()` collapses an absent perturbed-pivot count to 0
  (`src/interior/kkt_factorization.cpp:139`, `value_or(0)`), losing
  `InertiaEvidence::state`, `n_zero`, `zero_is_derived`, and pivot optionality
  (`include/hven/linear/symmetric_factor.h:103`). Add `const
  hven::linear::InertiaEvidence &inertia_evidence() const` — the last
  `FactorizeOutcome::inertia` stored verbatim beside the cached ints, which
  stay untouched (NLP reads unchanged → replay-inert). The tier reads ONLY this
  accessor, so the `value_or(0)` collapse is unreachable from its path —
  pinned: on MKL `state == kObserved`, counts match `peigs()/neigs()`,
  `zero_is_derived == true`; a seam-injected absent pivot count surfaces as
  `nullopt`, never 0 (consumed in T5).
- T3.b — `IpqpKktLayout` (spec §4.2): from `(H, Ae, Ai, n, me, mi)` compute the
  §3.1 pattern once (dim `n + 2*mi + me`, `KKTVector` block order,
  `include/hven/detail/interior/kkt_vector.h:33-40`), materialize into
  `KktFactorization::matrix()` (`kkt_factorization.h:54`), return
  per-source-nonzero offsets + diagonal slot bases (each iteration an O(nnz)
  scatter, no allocation, no `setFromTriplets`); structure key + entry-count
  collision guard (the `sync_matrix` template). Plus `detail/qp/ipqp_math.h`:
  the §3.5 specialized QP-shaped kernels, each naming the `barrier_math.h`
  kernel it mirrors (:76-:221); the complementarity reduction deliberately NOT
  shared (ULP-load-bearing `.sum()` order).
- Files: `include/hven/detail/interior/kkt_factorization.h` +
  `src/interior/kkt_factorization.cpp` (T3.a, additive only); new
  `include/hven/detail/qp/ipqp_kkt_layout.h` + `src/qp/ipqp_kkt_layout.cpp`
  (Q-S1), new `include/hven/detail/qp/ipqp_math.h` (header-inline per-element
  hot loops; Q-S2); `src/CMakeLists.txt` :188-198.
- Tests: T3.a pins above; scattered values byte-identical to a
  `setFromTriplets` reference (fixtures incl. empty me/mi, shared-column
  patterns); diagonal slots land at the offsets the scatter plan records,
  cross-checked against the factorization's observed inertia moving when a
  recorded diag slot is perturbed (fixed wording, ruling 9); Amendment E
  re-entrancy pin — predictor and corrector RHS against ONE `factorize()`
  through `KktFactorization::solve` (`const`, :127), both residuals checked;
  mutation non-vacuity (W0.3 precedent).
- Cells: A9 groundwork. Evidence: replay (T3.a touches shared interior/ code —
  the replay is the proof the addition is inert).
- SCOPE VALVE (Q11, ruled): on value-write overrun, ship W1 on full
  `setFromTriplets` per iteration; layout lands as a separately-verified,
  bit-identity-proven follow-on.
**T4 — Core engine: cold solve + ladder (L).** Dep: T1-T3.
- T4.a — the immutable clamp-centred box (rulings 2 + r3.2): `struct
  IpqpBox` — centre `c(i) = clamp(0, lower(i), upper(i))`, `lo_eff = max(l,
  c - Delta)`, `up_eff = min(u, c + Delta)`, computed ONCE at solve entry
  from `QpOptions::tr_radius` (`include/hven/qp/qp_types.h:77`;
  `SolveOverrides` :184; driver precedent `sqp_driver.cpp:2689`). The centre
  RULE IS `refine_on_face`'s own — its gate window is "centred on the clamped
  origin" (`qp_engine.cpp:174-186`, `c = min(max(0, lo), up)`;
  `qp_engine.h:979-985`) — cited as the reason: `QpProblem` permits zero
  outside the box (`qp_problem.h:89` checks sizes only), and a plain-origin
  centre would gate a different window than the refinement's, silently.
  CONTRACT, chosen and stated: `solve()` takes NO centre parameter — the
  clamp rule is the documented contract; the warm iterate `IpqpSeed::x` lives
  INSIDE the box, never redefines the centre, never becomes the refinement
  centre. A TR shrink-retry rebuilds `IpqpBox` at the new radius, same
  centre rule, re-clamping the iterate.
- T4.b — `IpqpBounds` + the pinned-variable domain gate (rulings 7 + r3.2,
  replaces the withdrawn `BoundSet`): full-space effective-bound structure
  (lower/upper-bounded index lists + values from `IpqpBox`; `>= kSsnInfBound`
  = absent, `ssn_engine.h:422`). Zero-width rule, chosen and stated: a
  subproblem containing ANY `lo_eff(i) == up_eff(i)` pair is OUT OF THE
  TIER'S DOMAIN — the build reports it, the dispatch DECLINES pre-solve and
  routes to the walk (the routing chain's existing fallback), counted by
  `ipqp_declined_pinned` (§7 note e); a decline is not an escape and never
  counts toward K=3 (the tier never ran). Under the clamp centre, zero width
  arises ONLY at declaration-level `l == u` or `Delta == 0` — the tree's own
  statement (`qp_engine.h:361-364`: "lo_eff(i) == up_eff(i) can only happen
  at Delta == 0 for a variable not already genuinely kFixed") — so declining
  covers every case, exact TR pins stay exact (never widened), and the tier
  keeps a strict interior by construction. Consequently v2's ε-relax rule is
  DELETED ENTIRELY: no equal-bound pair can survive to the barrier, so there
  is nothing to relax (`ipqp_fixed_bounds_relaxed` dropped). Pins: a
  declaration-pinned QP and a `Delta == 0` retry both decline, the counter
  fires, the walk solves them exactly; a strict-interior QP leaves it 0.
- T4.c — the kernel: cold start (spec §5.6); Mehrotra predictor-corrector
  (§3.1) with `detail::max_step_to_boundary`
  (`include/hven/detail/interior/barrier_math.h:62`); the gated `(rho, delta)`
  schedule with monotone floor (§3.2 — reuse
  `dual_regularization`/`kProxRegFloor`,
  `include/hven/detail/globalization/inertia_regularization.h:61,:46`; gated
  decrease, NOT `prox_reg_decay`'s unconditional decay :72); inertia gate + W-B
  ladder (§2.2.1-3; precedent `InteriorPointSolver::factor_impl`,
  `interior_point_solver.cpp:1678`), inertia read through T3.a's accessor only;
  Ruiz with unscale-on-export (§4.3); budgets (`effective_qp_max_iter`
  sentinel, `qp_engine.h:516`; hard cap; factorization cap). Factorization
  discipline = §7 note a: one `refactorize(kVerify)` per tier entry (the
  one-time O(nnz) payment), `kAssumeAnalyzed` otherwise; `compute()` itself
  does not verify (`kkt_factorization.cpp:81-113`).
- T4.d — the residual contract (ruling 5): T4 OWNS a new QP-shaped relative-KKT
  residual implementation in `ipqp_math.h`/the engine TU — stationarity,
  primal/dual feasibility, complementarity, each scaled by a `max(1, ...)` fold
  in the discipline `detail::free_block_stationarity` models
  (`qp_engine.h:622`) but full-KKT and WorkingSet-free. No reaching into
  `QpEngine` private members (`qp_engine.cpp:980-:1032` are members), no shared
  refactor in W1; convergence-agreement pins against the walk are the
  cross-check that the two implementations judge alike.
- Backend `Options`: the tier's own, Accelerate-safe (Q6) —
  `weighted_matching`, `matrix_scaling`, `pivot_strategy`,
  `factorization_algorithm` (`symmetric_factor.h:375,:402,:444,:470`) all
  default, `cnr_threads=0`. All `iparm`-mapped fields at defaults → no
  IPARM-SURFACE label needed (else risk R3).
- Files: new `include/hven/detail/qp/ipqp_engine.h` (declarations, enums,
  `inline constexpr` ONLY) + new `src/qp/ipqp_engine.cpp`; `src/CMakeLists.txt`
  :188-198 + TU census comment (:376 region).
- Interfaces: `enum class IpqpEscape {...};` `struct IpqpBox;` `struct
  IpqpBounds;` `struct IpqpResult { QpStatus status; IpqpEscape escape_reason;
  IpqpCounters counters; /*face blocks, evidence*/ };` `struct IpqpSeed;`
  `IpqpResult IpqpEngine::solve(const QpProblem&, const IpqpSeed*, const
  IpqpOptions&, const SolveOverrides&);` (clamp-centre contract, T4.a);
  `attach_ledger` mirroring `qp_engine.h:895`.
- Tests: convex QPs agree with the walk at QP tolerances; convex-inertness pin
  (`ipqp_inertia_retries == 0`, `ipqp_rho_demanded_max == 0`); monotone-floor
  pin; cap-slack exhaustion pin (`kSsnProxMax`/`kSsnProxCapSlack`,
  `ssn_engine.h:576-577`); the verify-once counter pin (below, A9); cold
  determinism; T4.a/T4.b pins; Ruiz on/off, pins unscaled-only.
- Cells: A8 (cold half); A9 engine half — per tier entry
  `pattern_verify_count` advances by EXACTLY 1 and `analyze_count` by at most
  1, later factorizations leave both unchanged; mutation non-vacuity.
  Evidence: replay; suite.
**T5 — Certification + escape ladder + evidence-failure seams (L).**
Dep: T4.
- Goal: §2.2.4 — the REQUIRED final unregularized inertia read
  (`ipqp_final_inertia_read` 0/1/2), wrong → downgrade, never `kOptimal`; the
  perturbed-pivot policy (mirror `detail::inertia_verdict`,
  `qp_engine.h:558-589`) reading T3.a's full `InertiaEvidence` — `state`,
  `zero_is_derived`, and OPTIONAL pivots, so "absent count" and "zero count"
  are distinguishable on the tier path (ruling 1); `state != kObserved` →
  conservative-rho-floor step + whole-solve downgrade; §6.2 stall (W=5
  accepted-step window, all three conjuncts, reset on regularization change —
  constants `ssn_engine.h:641-644`), the escape carrying a §6.3-style
  evidence block with the three conjunct VALUES (μ ratio over the window,
  relative residual improvement, min α) — no reason partition (§7 note b);
  §6.3 infeasible-suspect two-conjunct mirror + exhaustion variant + evidence
  block + Farkas corroboration (arming only, never certifying).
- Files: `ipqp_engine.h/.cpp`; new `tests/sqp/test_ipqp_seams.cpp` under the
  `HVEN_TESTING` convention (CLAUDE.md §8): a standalone target recompiles the
  tier TUs (and the T3.a TU) with `HVEN_TESTING` to inject
  `kQueryFailed`/`kUnavailable`/absent-pivot/perturbed evidence — hook in the
  tier's own Apache-2.0 TU (§6 boundary preference), never in session files.
- Tests: A11 core — HS indefinite rows + the parametric `IndefiniteBoxModel`
  family (`tests/sqp/test_qp_engine_indefinite.cpp`): gate fires, ladder
  monotone, final read happens, wrong read → NOT kOptimal AND `kIndefinite`;
  `ipqp_require_final_inertia=false` → always-downgraded pin; injected
  `kQueryFailed`/`kUnavailable` → rho-floor + downgrade, counts never
  zero-filled; injected absent pivot count → `nullopt` observed (never 0);
  stall fixture + ONE evidence pin (the stall escape's evidence block carries
  all three conjunct values — replaces v2's three per-reason fixtures); a
  healthy solve never trips conjunct (iii) on one tiny-alpha step;
  infeasible-suspect fixture →
  `kInfeasibleSuspect` + evidence and NEVER `QpStatus::kInfeasible`
  (`solver_status.h:13`).
- Cells: A11 (engine half); A10 Linux-side branch coverage (Q12). Evidence:
  replay; `HVEN_TESTING` target proven cost-free per §6.
**T6 — Driver dispatch, routing chain, R6 third producer (L).** Dep: T4-T5.
- Goal: `const bool ssn_mode` (`sqp_driver.cpp:1210`) and `walk_owns_this_qp =
  !ssn_mode` (:2678) become `switch (opts_.qp_mode)` with the §2.3 routing
  chain; walk is the LAST branch. Seam functions one-for-one with the SSN set
  (`sqp_driver.cpp:653/:663/:688/:705`); factorizations charge the probe budget
  as `ssn_budget_charge` does (:711-716 — Q7); `accumulate_ipqp_counters` call
  site; lazy `SqpDriver::ipqp_engine()`/`ipqp_options()` mirroring :3448/:3455,
  member beside `ssn_engine_` (`sqp_driver.h:2873`). Routing: FIRST the
  T4.b domain gate — a zero-width effective pair declines to the walk
  pre-solve (`ipqp_declined_pinned`, not an escape, no K=3 charge); then
  converged → ratio-rule face threshold (kappa=1e-2, z above mu scale,
  UNCERTAIN never forced) → `refine_on_face` handed the tier's face WITH the
  clamp-centred window — `IpqpSeed::x` is never the centre (T4.a contract;
  `qp_engine.h:979-985`), asserted at the hand-off → refusal → SSN
  warm-grade via `split/recombine_bound_multipliers`
  (`ssn_engine.h:1425,:1433`) → genuine escape → COLD walk, iterate discarded.
  K=3 consecutive-escape retirement, success resets. W2 hook
  `certified_feasibility_fallback(qp, ev, seed)` as the escape branch's single
  entry, body = cold walk. SOC/elastic/restoration stay walk-unconditional
  (`ropts.qp_mode = QpMode::kWalk`, `sqp_driver.cpp:2343`). Cross-major
  hoisting gated on the ACTUAL epoch surface, `AggregateEvalSeam::epoch()`
  (`aggregate_eval_seam.h:113`) — reuse only while it is unchanged; kill
  switch `ipqp_hoist_symbolic`. R6: the tier is the THIRD producer of
  exported inequality face prices — extend the R6 measurement-point pin at
  `SqpDriver::finish()` to cover three (spec §9). Remove T1's kIpm throw.
- Files: `src/drivers/sqp_driver.cpp`; `include/hven/drivers/sqp_driver.h`
  (member + accessor decls); `src/qp/ipqp_engine.cpp` (face classifier); tests.
- Tests: A7 — at `kWalk` no `IpqpEngine` constructed, no analysis, every
  `ipqp_*` counter structurally zero (the lazy-`ssn_engine_` discipline);
  routing pins — declined-pinned→walk (counter fires, no retirement
  charge), converged→refine, refusal→SSN, escape→cold walk (seed discarded),
  K=3 + reset; end-to-end kIpm solves on small F7/HS cells agree
  with kWalk; `QpSolution::tr_active` semantics reproduced
  (`qp_problem.h:102`); the R6 guard-extension pin; A9 driver half — §7 note
  a asserted across majors (verify +1 per tier entry, analyses hoisted while
  `epoch()` holds, both reset on a bump), mutation non-vacuity.
- Cells: A7; A9 (driver half). Evidence: replay, BOTH arms — the plan's
  highest-risk replay (shared dispatch path); a diff reverts the task (R4).
  Plus an informational kIpm bench smoke.
**T7 — Warm restart: the W3 fold (L).** Dep: T6. Detail in §3.
- Goal (ruling 4): T7 OWNS the preserved-seed ingest path. Facts it routes
  around: staging throws on nonfinite blocks (`require_finite_core`,
  `sqp_driver.cpp:922`); solve-time ingest throws on wrong dimensions (:985)
  and stamp mismatch (:1007); the crossover flattens polish data into
  `WarmStart`, whose carrier holds only signed `z` (consumption from :1019;
  `include/hven/detail/warmstart/warm_start.h:132`). Under `kIpm` the
  MAIN-subproblem seed is built directly from the validated staged
  `WarmStartData` + `find_ipm_polish` payload (`ipm_polish_extension.h:97`)
  into `IpqpSeed` — zL/zU/mu never pass through the signed-z flattening;
  kWalk/kSsn ingest untouched (inertness). Mode-local degrade: in `kIpm`,
  stamp or dimension mismatch degrades COLD per spec §5.4 (visible via the
  `ipqp.restart` grade), never throws; the existing throws stand for the
  other modes and for nonfinite staged CORE data (a caller error in every
  mode). `mu_` finiteness (ruling r3.3): staging today finiteness-checks the
  core blocks and the three polish VECTORS but not `mu_`
  (`sqp_driver.cpp:105-119,:170-181`), and the decode round-trips NaN/inf
  bit-exactly (`ipm_polish_extension.cpp:169`; the header's layout note says
  so) — the tier's decode/ingest adds the check: a nonfinite `mu_` marks the
  extension MALFORMED → tier degrades cold, mode-local; the core staging
  throw is unchanged. One pin.
  Then: cross-major `IpqpSeed` carry; §5.2 repair; §5.3 clamp; §5.4 grades;
  §5.5 warm-kill (clamped budget, §7 note c); TR shrink-retry re-enters warm
  — never an escape, never a seed reset.
- Files: `ipqp_engine.h/.cpp`; `src/drivers/sqp_driver.cpp` (the kIpm seed
  branch beside the existing ingest, not inside it).
- Tests/cells: A12, A8 warm half, grade-table pins, mode-local degrade pins
  (kIpm degrades cold where kWalk throws — both asserted), the nonfinite-mu_
  malformed-extension pin — §3. Evidence: replay; suite.
**T8 — Ledger + trace event points (S).** Dep: T6-T7. Detail in §2.
- Goal: one `SolveRecord` per tier subproblem via the existing `attach_ledger`
  seam (`qp_engine.h:895`; driver analogue `sqp_driver.h:2240`), label prefix
  `"ipqp"`; the seven §7 event points as named private emit sites (no
  serializer — W4's).
- Tests: ledger-record pin per subproblem; event structs carry the facts the
  counters assert. Evidence: replay.
**T9 — In-tree acceptance battery (L).** Dep: T6-T7.
- Goal + cells:
- A1: two-junction block-placement cells at F7's own geometry. Real surface
  (ruling 9): junction locations are READ OFF a solved F7 bound-arc cell —
  the empty-window regime `p <= R/2`, `bench/corpus_cells.h:168-172`, cells
  built at :1245-1271 — via a walk solve; A1 builds the two-block placement
  cells at those indices. Exact recovery under the ratio rule, E1 Rule-A/B
  counts alongside, iterations in the gate.
- A2: a real mid-solve F7 subproblem via the dump seam
  (`tests/sqp/test_bench_dump.cpp` covers `bench/bench_cli.h`'s --dump-solution
  helpers; format sufficiency is Q-S5); pin tier iters/factorizations +
  agreement with the walk at QP tolerances.
- A3: cells with >= 1 active variable bound AND >= 1 active inequality row per
  margin class; both sets + bound-multiplier signs recovered.
- A5: the QP-level surrogate of the 2/8 dead population cells — a cold
  wide-window F7 subproblem at size, in budget (Q10: hven pins the surrogate;
  tycho_sqp pins the population cells; same BUILD ruling).
- A11 assembled: the HS battery leg (`tests/sqp/test_hs_battery.cpp`
  infrastructure) under kIpm on the indefinite rows.
- Files: new `tests/sqp/test_ipqp_engine.cpp` / `test_ipqp_routing.cpp` /
  `test_ipqp_acceptance.cpp`; `bench/` dump-driven fixtures.
- Evidence: replay; suite; every new pin demonstrated falsifiable against a
  deliberately broken build (the W0.2-close standard).
**T10 — A4 taxonomy, measured `mu` default, envelopes, close (M).** Dep: T9.
- Goal: (a) the hven solve arm for the 29 E1 cells — regenerated
  byte-identically from `docs/notes/data/2026-08-m6-e1-acquisition/generator/`
  (`e1_dump.h` container) + recorded seeds; PIQP never run, linked, or read —
  only its committed CSVs compared as oracle (CLAUDE.md §6). Gate (Amendment
  F): every cell converges, < 40 iters, exact active-set recovery, no monotone
  blow-up across the active-fraction sweep at BOTH sizes (nx = 2e4, 1e5),
  asserted not spot-checked; the brief's proxy caveat retires only when A4 is
  green. (b) Q4: sweep `ipqp_init_mu` over {1e-3,1e-2,1e-1,1}; adopt the winner
  as the measured default, sweep on the record (a §7 declared re-derivation if
  it moves the 0.1 placeholder). (c) A13: same-occasion PSIOPT-envelope re-read
  at nx=1e5 (the 30.15x gap) — §7 terms: serialized, solo, wall-asserting. (d)
  A14: one LTO-on/threads-on context leg, informational, never a gate; also
  re-examine the W0.1b scatter-inlining CARRY here. (e) A8 full: two
  independent A4 sweeps bit-identical on non-timing columns. (f) A6 final: full
  replay + suite. (g) A10: Mac legs of A2/A9 recorded UNOBSERVED (the O12
  session is the standing mechanism). (h) Evidence artifact under
  `docs/notes/data/2026-08-m6-w1-acceptance/` with provenance stamp + declared
  serial/co-run protocol; ledger close entry.
- Files: new `bench/ipqp_e1_arm.cpp` (or scratch arm — Q-S3); evidence dir;
  `docs/notes/2026-08-m6-ledger.md` append.
- Evidence: the artifact itself; replay; suite.

**Totals: 10 tasks — S: 3 (T1,T2,T8) · M: 2 (T3,T10) · L: 5
(T4,T5,T6,T7,T9).**

## §2. Counters/telemetry surface and the W4 hook

- `IpqpCounters` lands complete in T2 (spec §7 table + §7 amendments),
  populated T4-T7: the work, rho, warm, and routing groups; the five-way
  escape census with its sum invariant (stall-reason counters DROPPED, note
  b — the stall conjunct values ride the escape's evidence block);
  `ipqp_declined_pinned`; min/max-folded alpha/shift fields; DNF sentinel
  `1e6`. Counters are the asserted currency (§7).
- A9's counter relation is the AMENDED one of §7 note a.
- W4 hook: T8 places the seven event points (`ipqp.iter/.reg/.restart/
  .route/.certify/.escape`, `qp.mode`) as named private emit sites whose
  argument structs carry exactly the spec §7 schema fields (17-digit doubles).
  W1 ships structs + call sites + ledger integration; W4 ships the JSON-lines
  serializer against them — schema v0 is fixed by the spec. No W1 pin reads the
  trace.

## §3. The W3 tasks and their fixtures (inside T7)

- T7.1 Preserved ingest (ruling 4): the kIpm seed branch of T7's goal — full
  polish (zL/zU/mu) into `IpqpSeed` without the signed-z flattening
  (`warm_start.h:132`); mode-local cold degrade on stamp/dim mismatch.
- T7.2 `IpqpSeed` + cross-major carry: `(x, s, lambda_e, lambda_i, zL, zU, mu,
  zeta, lambda_est)` on the engine instance (Q8; `QpSolution`,
  `qp_problem.h:102`, stays untouched). `x` is an iterate inside T4.a's box,
  never a centre. TR shrink-retry re-enters with the previous attempt's state;
  never counts toward K=3.
- T7.3 Repair (§5.2): strict-positivity clamps (a polish payload legitimately
  carries exact zeros); SAY two-scalar `(delta_p, delta_d)` shift, O(n+mi), no
  factorization; re-center `zeta/lambda_est`. Counters: repairs + shift_max.
- T7.4 `mu_0` clamp (§5.3, Q3): never a setting; `mu_0 = clamp(max(mu_meas,
  kappa*mu_payload), ipqp_min_mu, ipqp_init_mu)`; `ipqp_mu_adopted`. The NLP
  IPM's refusal (`interior_point_solver.h:1607-1613`) stands untouched.
- T7.5 Grades (§5.4): full warm (core + `hven.ipm.polish.v1`,
  `ipm_polish_extension.h:30,:41`); base warm (signed-`z` split, eps from
  `mu_0`, a documented degradation — `WarmStartData::bound_lmults_` is signed,
  `warm_start_data.h:78-86`); stamp/dim mismatch → cold (mode-local, T7.1);
  foreign tags ignored (M5 R3).
- T7.6 WARM-KILL (§5.5): warm budget = `min(ipqp_warm_iter_budget, effective
  ipqp_max_iter)` (§7 note c); overrun → restart COLD exactly once +
  `ipqp_warm_restart_abandoned`; second overrun = ordinary budget escape. Trust
  thresholds relative to the QP tolerance.
- Fixtures: **A12** — an hven-shape perturbed-continuation cell mirroring tycho
  Task 13 (warm 24 vs cold 5, stale primal, alpha ~0.25): a parametric QP
  family (`tests/sqp/test_parametric_families.cpp` infrastructure), data
  perturbed, warm restart from the stale seed; assert the kill fires within the
  clamped budget, the counter increments, cold recovers the short path — the
  MECHANISM, not tycho's numbers. **zL/zU split** — full grade's first iterate
  matches the payload's split exactly on a two-sided-bound fixture; base grade
  converges, degradation documented. **Determinism (A8/M5 R5)** — same payload
  staged twice from cold → bit-identical first iterates. **Mode-local degrade**
  — a stamp-mismatched payload under kIpm runs cold (grade counter fires); the
  same payload under kWalk still throws.
- The ledger's "IPM re-entry mu reset" datum (ledger :469-480) rides W3 in
  problem shape only; NOT W1 scope — owner question Q-O1.

## §4. The W2 interface W1 leaves room for (interface only)

- `QpSolution certified_feasibility_fallback(const QpProblem &qp, const
  IpqpInfeasibilityEvidence &ev, const QpSolution *seed);` — landed in T6 as
  the escape branch's ONLY entry; W1 body forwards to the cold walk; W2 changes
  the body and no call site (spec §6.4).
- `struct IpqpInfeasibilityEvidence` (T5) is the W2 input of record: each §6.3
  signal with value, window, least-infeasible point, Farkas flag — shaped so
  the registered elastic l1-penalized QP (Amendment C) consumes it directly.
- The tier never emits `QpStatus::kInfeasible` (T5 pin): the elastic ladder
  stays the only `kInfeasible` consumer (`sqp_driver.h:1701`). No other W2
  work.

## §5. Risks and the specific mitigation task

- R1 — `IpqpKktLayout` value-write overrun → T3's ruled valve (Q11).
- R2 — A4 gate fails → T10 reports honestly; proxy caveat stays; escalate under
  the brief's M6a/M6b scope valve. No gate is weakened.
- R3 — iparm drift → T4 pins Options at backend defaults; any later non-default
  `iparm`-mapped field is its own IPARM-SURFACE-labelled commit with cited
  validation evidence (§6, Q6).
- R4 — T6's dispatch rewrite perturbs kWalk/kSsn → the both-arm U0 replay is
  the gate; a diff reverts the task; no "expected drift" (A6).
- R5 — R6-sweep third-producer gap at close → T6 carries the pin extension as
  an acceptance item, not a follow-up.
- R6 — scatter-loop inlining regression (W0.1b CARRY, ledger :104-115) → T10(d)
  re-examines on the tier's own bench.
- R7 — A12 fixture cannot reproduce tycho's pathology → T7 constructs the
  stale-primal mechanism directly; pins assert the mechanism.
- R8 — Accelerate divergence → T4 Accelerate-safe Options; T5 Linux-side branch
  coverage via `HVEN_TESTING` (Q12); T10 Mac legs UNOBSERVED, never fabricated
  (§6).
- R9 (new) — T3.a's shared-file edit regresses the NLP engine → additive member
  only, cached ints untouched, the replay is the gate; a diff reverts T3.a to
  its own commit for isolation.

## §6. Open questions

For the settler (Q-S4 of v1 is RESOLVED by ruling 8 — see §7 note c):
- Q-S1 (T3): `IpqpKktLayout` in its own `ipqp_kkt_layout.h/.cpp` pair (plan's
  assumption under §5's concrete-class rule) or folded into
  `ipqp_engine.h/.cpp` (spec §1 names only the engine pair)?
- Q-S2 (T3): confirm `ipqp_math.h` stays fully header-inline (per-element hot
  loops, §5's inlining criterion) — the "declarations only" header budget binds
  `ipqp_engine.h`, not `ipqp_math.h`.
- Q-S3 (T10): the A4 solve arm — in-tree `bench/` tool (plan's assumption) vs a
  scratch-workspace arm mirroring E1 with only the artifact committed?
- Q-S5 (T9/A2): is the `--dump-solution` format (`bench/bench_cli.h`, pinned by
  `tests/sqp/test_bench_dump.cpp`) sufficient to carry a mid-solve QP
  subproblem (H/Ae/Ai/g/bounds), or does A2 need an e1_dump.h-shaped
  QP-container dump added to the bench seam?

For the owner (expect 0-1):
- Q-O1: the registered "IPM re-entry mu reset" recovery (ledger 2026-08-28,
  "rides W3, parity is an owner call"): W1 treats it as OUT of scope —
  NLP-IPM-side, not tier-side. Confirm, or name the window.

## §7. Dated spec-amendment notes (settler-ruled, 2026-08-30)

Each is a declared, dated amendment to the spec of record, per §7's
declared-change discipline; the spec text itself is amended at W1 close.
- (a) A9 / spec §4.1 executable claim AMENDED (ruling 3): `compute()`
  factorizes without verification (`kkt_factorization.cpp:81-113`) and
  `kAssumeAnalyzed` calls leave `pattern_verify_count` unchanged
  (`symmetric_factor.h:670`), so `pattern_verify_count == analyze_count` per
  solve is unsatisfiable as written. Amended claim: the FIRST tier
  factorization per tier entry runs `kVerify` (+1, the one-time payment in the
  T4-precedent sense); all others `kAssumeAnalyzed`; analyses hoisted while
  `AggregateEvalSeam::epoch()` (`aggregate_eval_seam.h:113`) is unchanged. A9
  asserts exactly that.
- (b) Spec §6.2/§7 stall census AMENDED (ruling 6; RE-RULED TWICE, final
  2026-08-30 r3): fixed-priority attribution is degenerate (all three
  conjuncts hold at every escape) and last-arriving attribution is ill-posed
  (μ resets on a drop and α must hold on EVERY step of the window, so
  "arrival order" is not well-defined on a completed window — Codex r2
  finding 1). FINAL: the three `ipqp_stall_reason_*` counters are DROPPED
  from the spec §7 table. The census keeps the single `ipqp_escape_stall`
  (the ruling's `ipqp_stall_escapes`) inside the five-way escape census; the
  three conjunct VALUES at escape — μ ratio over the window, relative
  residual improvement, min α — travel in the §6.3-style evidence block
  attached to the stall escape: information without a fake partition. The
  census-sum pin is escape-count only; the three per-reason fixtures are
  replaced by ONE pin asserting the evidence block carries all three values.
- (c) Spec §9 table AMENDED (ruling 8, resolves v1 Q-S4): the effective
  warm-kill budget is `min(ipqp_warm_iter_budget, effective ipqp_max_iter)`;
  the field's doc row carries the clamp. A dated table amendment, not a silent
  extension of "verbatim".
- (d) Spec §3.5 reuse ledger AMENDED (rulings 1, 7, 5): `BoundSet` verbatim
  reuse WITHDRAWN (NLP-owned, reduced-space, relaxed-bound,
  fixed-variables-eliminated — `bound_set.h:19-35`) → `IpqpBounds` (T4.b,
  decline gate); `KktFactorization` row gains the additive `inertia_evidence()`
  accessor (T3.a); the "QP residual-scale helpers" row is WITHDRAWN (private
  members / WorkingSet-bound) → T4.d owns a new implementation.
- (e) Spec §7 counter table AMENDED (final 2026-08-30 r3): +
  `ipqp_declined_pinned` (T4.b's domain-gate declines); the v2-proposed
  `ipqp_fixed_bounds_relaxed` is dropped with the ε-relax rule; −
  `ipqp_stall_reason_mu/residual/alpha` per note b.
- (f) Spec §5.4/staging AMENDED (r3.3): the polish payload's `mu_` gains a
  finiteness check at the tier's decode/ingest — a nonfinite `mu_` marks the
  extension malformed → tier degrades cold, mode-local; the core staging
  throw (`require_finite_core`, `sqp_driver.cpp:105-119`) is unchanged.
  Today neither `validate_staged_polish` (:170-181, vectors only) nor the
  decode (`ipm_polish_extension.cpp:169`, bit-exact NaN/inf round-trip)
  checks it.

## §8. v2/v3 changes (Codex r1 + rulings 1-9; Codex r2 + r3 rulings 1-3)

1. (Critical 1) T3.a added: additive `KktFactorization::inertia_evidence()`
   preserving state/n_zero/zero_is_derived/pivot optionality; the `value_or(0)`
   collapse (`kkt_factorization.cpp:139`) unreachable from the tier's read
   path, pinned. T5 consumes it.
2. (Critical 2) T4.a added: immutable `IpqpBox` distinct from the warm
   iterate; documented no-centre-parameter contract on `solve()`;
   `IpqpSeed::x` never the refinement centre, reason cited
   (`qp_engine.h:979-985`); T6 hand-off asserts it. Centre finalized in r3
   (item 11).
3. (Major A9) A9 amended per §7 note a: verify exactly once per tier entry;
   epoch surface named (`AggregateEvalSeam::epoch()`).
4. (Major T7) T7 → L; owns the preserved-`IpqpSeed` ingest (polish zL/zU/mu
   never flattened through `WarmStart`'s signed z, `warm_start.h:132`);
   mode-local cold degrade on stamp/dim mismatch (existing throws at
   `sqp_driver.cpp:985/:1007` unchanged for kWalk/kSsn).
5. (Major T4) T5 → L; T4.d owns a new QP-shaped relative residual — no
   private-member reach (`qp_engine.cpp:980-:1032`), no shared refactor.
6. (Major stall census) Fixed-priority one-hot adopted, then last-arriving
   re-ruling; both found ill-posed. FINAL in r3 (item 10).
7. (Major BoundSet) Reuse withdrawn; T4.b `IpqpBounds`. The v2 ε-relax
   equal-bounds rule was superseded in r3 (item 11).
8. (Minor Q-S4) Resolved as §7 note c (dated table amendment); dropped from §6.
9. (Minor citations) T3 diag-slot wording fixed (accessors return cached
   counts, `kkt_factorization.h:131` — the pin now perturbs a recorded slot and
   reads the inertia move); A1 re-cited to the real surface (junctions read off
   a solved bound-arc cell, `corpus_cells.h:168-172`, :1245-1271 — no
   `f7_*_bound_*` cell-id surface exists). Sizes: T5 M→L, T7 M→L (ruling +
   Codex sizing finding). New risk R9.
10. (r3.1, Codex r2 finding 1) Stall census FINAL: the three
    `ipqp_stall_reason_*` counters DROPPED; single `ipqp_escape_stall` kept
    in the five-way escape census; the three conjunct values travel in the
    stall escape's evidence block (§7 note b as replaced); census-sum pin is
    escape-count only; the three per-reason fixtures replaced by one
    evidence-block pin (T2, T5, §2 updated).
11. (r3.2, Codex r2 finding 2) `IpqpBox` centre is `clamp(0, l, u)` — the
    refinement's own rule (`qp_engine.cpp:174-186`), since `QpProblem`
    permits zero outside the box; and zero-width pairs are OUT OF DOMAIN:
    the T4.b gate declines the subproblem to the walk pre-solve
    (`ipqp_declined_pinned`, no escape, no K=3 charge). Zero width arises
    only at declaration `l == u` or `Delta == 0` (`qp_engine.h:361-364`), so
    the decline covers every case and the v2 ε-relax rule is DELETED
    (`ipqp_fixed_bounds_relaxed` dropped; §7 notes d/e updated); exact TR
    pins stay exact.
12. (r3.3, Codex r2 finding 3) Polish `mu_` finiteness: checked at the
    tier's decode/ingest — nonfinite `mu_` = malformed extension → cold
    degrade, mode-local; core staging throw unchanged (§7 note f); one pin
    (T7).
