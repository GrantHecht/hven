# Chunk 3 comment-sweep review — `include/hven/detail/qp` + `include/hven/qp`

Repo `/home/ghecht/Projects/hven-sweep`, branch `sweep/base`, commit **5befdf1** over **15e2789**.
Read-only review. No builds, no edits, no subagents.

**VERDICT: CHANGES REQUIRED** — 4 blocking, 8 non-blocking items.

Nothing technically false about the *algorithm* was found: every condensation the
report flagged as uncertain (qp_engine.h reuse condition (e), the HotState
two-mechanism summary, qp_types.h's `max_iter` contract and static_assert message)
checks out against the code. What blocks is text integrity: the sweep introduced a
broken sentence, six dangling cross-references to a section it renamed, two
malformed Doxygen blocks and several formatting regressions. All are cheap to fix.

---

## Findings

### Blocking

**B1 — SEVERITY HIGH. `ssn_engine.h:1490` — reflow corrupted the sentence; unclosed paren.**

Now reads:

```
// **HOW FAR x LIES OUTSIDE THE TRUST REGION, AND IT CAN BE FAR** (see the
// max_j max(0, x_j - up_eff_j, lo_eff_j - x_j) over the
// variables whose effective bound came from the RADIUS -- 0.0 when no
```

The label-strip replaced `(review fix round 1, I3).` with `(see the` — an opening
parenthesis with no close, and the definition sentence now has no verb. Pre-image
was `... AND IT CAN BE FAR** (review fix round 1, I3). max_j max(0, ...) over the
variables ...`. Correct text: drop the parenthetical entirely — `**HOW FAR x LIES
OUTSIDE THE TRUST REGION, AND IT CAN BE FAR.** max_j max(0, x_j - up_eff_j,
lo_eff_j - x_j) over the variables whose effective bound came from the RADIUS ...`

**B2 — SEVERITY MEDIUM-HIGH. `qp_engine.h` — six dangling references to a section the sweep renamed.**

The header-contract note at `qp_engine.h:1357` was retitled
`PHASE-3 DESIGN FRICTION -- RESOLVED (Phase 3 Task 2)` → `PER-SOLVE RADIUS
VARIATION`. Six sites still send the reader to the old title, which no longer
exists anywhere in the tree:

- `qp_engine.h:264` — "(see assemble_kkt_core, and the PHASE-3 DESIGN FRICTION note"
- `qp_engine.h:280` — "PHASE-3 DESIGN FRICTION note) is byte-identical to the pair the"
- `qp_engine.h:2267` — "contract's PHASE-3 DESIGN FRICTION note -- so this is byte-identical"
- `qp_engine.h:2589` — "PHASE-3 DESIGN FRICTION note). Shared with run() so refine_on_face()"
- `qp_engine.h:2911` — "SolveOverrides and the header contract's PHASE-3 DESIGN FRICTION"
- `qp_engine.h:5193` — "PHASE-3 DESIGN FRICTION note above this class explains why tr_radius/"

Either restore the old heading or retarget all six to `PER-SOLVE RADIUS VARIATION`
(preferred — the new name is the better one).

**B3 — SEVERITY MEDIUM. `ssn_engine.h:1099-1102` — malformed Doxygen: `///` spliced into `//` blocks.**

Two member docstrings on `SsnStart` were half-converted:

- `SsnStart::z` (`:1091-1101`): the substantive paragraph ("SIGNED bound
  multiplier, qp_problem.h's QpSolution::z convention …") stays `//` while the
  *secondary* remark got `///`. Doxygen therefore documents `z` with "Added beyond
  the bare method's own field list…" and drops the convention paragraph.
- `SsnStart::slacks` (`:1102-1109`): first line is `///`, the remaining seven lines
  are `//`. The doc comment terminates at line 2; the rest is invisible.

The rules require one form per block: either promote the whole block to `///` (with
`@brief`) or leave the whole block `//`. This is the only place in the chunk where
the two are mixed — qp_problem.h / working_set.h / qp_types.h are clean.

**B4 — SEVERITY MEDIUM. `eqp_solve.h:75` — a shipped header now points at the sweep's own report.**

```
// one has none. (A flag-gated iterated loop previously stood here and never
// fired across the full A/B corpus; see the report for the measurement.)
```

"the report" is `.sweep/chunk-3-report.md`, a sweep workspace artifact. A reader of
hven has no such document. This is also the class of sentence the rules delete
outright (history of a removed flag). Correct action: drop the parenthetical, and
carry the measurement in the report's rationale list only (it already is, under
"eqp_solve.h:69-76, 93-95").

### Non-blocking

**N1 — SEVERITY MEDIUM. `eqp_solve.h:59-60` — the deleted definition of `reg` is load-bearing, not history.**

Pre-image: `K0 = K - diag(reg) with reg(k) = delta for Hessian rows and -mu for
constraint rows`. Post: `K0 = K - diag(reg)`. Every subsequent line of that
derivation (`r = rhs - K*y + diag(reg)*y`, and the stopping-rule paragraph's
`||diag(reg)*y||inf`) uses `reg` with no definition left in the header. The deleted
clause is exactly true of the code (`eqp_solve.h:204-206`:
`reg.head(n_free).setConstant(opts.primal_delta);
reg.segment(n_free, me + n_working).setConstant(-opts.dual_mu);`) and is a formula
term, not narration. Restore the seven words.

**N2 — SEVERITY MEDIUM. `ssn_engine.h:52-56` — the fix is attributed to the wrong mechanism.**

```
// (An earlier implementation ran the convergence test before every
// factorization and returned kOptimal at the saddle when SEEDED there -- the
// defect the inertia gate closed.)
```

Section 7b (`ssn_engine.h:457-484`) is explicit that the inertia gate is step 8 of
the ordinary attempt and was already present; what closed this defect is 7b's
*certifying exit* — "the convergence test now opens a VERIFICATION ATTEMPT instead
of exiting". Pre-image said "the defect review fix round 1 closed", which named no
mechanism at all, so the rewrite added a claim rather than condensing one. Suggest
"— the defect section 7b's certifying exit closed."

**N3 — SEVERITY LOW-MEDIUM. Five comment lines now exceed the 100-column limit; the pre-image had none.**

`git show 15e2789` has zero >100-col lines in either file; 5befdf1 has five, all in
re-flowed regions:

- `qp_engine.h:2322` (106 cols) — "…read off the possibly-shared object. The two are equal at every ordinary call"
- `qp_engine.h:3069` (128 cols) — "…PLUS (e) below. `ws` here is exactly the seed working set condition (b) is"
- `qp_engine.h:4744` (113 cols) — "RESERVE BEFORE THE SCHUR MUTATION, in all three add loops. The ledger and the border stack are one"
- `ssn_engine.h:360` (131 cols) — "NOTHING HERE -- switch it off deliberately rather than discover that. delta and mu perturb ONLY the Jacobian -- for_each_entry's"
- `ssn_engine.h:607` (118 cols) — "form 2ab / (a + b + rho) whenever a + b > 0. The two are algebraically identical -- multiply by (a+b+rho)/(a+b+rho)"

**N4 — SEVERITY LOW. `qp_engine.h:2419` — mis-indented comment line.**

```
    // caller may use it unconditionally.
    //
                  // **PUBLIC-API PRECONDITION**:
    // the trust-region gate below assumes its window is centred at
```

18 spaces of indent, mid-block. Also leaves the continuation line starting
lower-case mid-sentence.

**N5 — SEVERITY LOW. `qp_types.h:86` — carried-forward wrong identifier.**

"see qp_engine.h's `unbounded_artifact_threshold`, written in terms of this field."
No such symbol exists; it is `detail::unbounded_artifact_scale`
(`qp_engine.h:1643`, `kUnboundedArtifactFactor / opts.primal_delta`). The claim
about the *value* (`~|g|/primal_delta`) is correct. Pre-existing (the pre-image said
the same), but the sentence was rewritten in this commit and the name was not fixed.

**N6 — SEVERITY LOW. `qp_types.h:79-93` — validation scope broadened.**

Post: "Validated at solve start (QpEngine::solve throws std::invalid_argument
otherwise)". The code (`qp_engine.h:2712-2726`) validates `opts.primal_delta` /
`opts.dual_mu` only when the corresponding `SolveOverrides` field is at its negative
sentinel — a stored NaN behind a supplied override does not throw. The pre-image
carried that qualifier explicitly. Not false in the common case; worth one clause
("…on every call that resolves to this field").

**N7 — SEVERITY LOW. `qp_types.h:48-56` — the equivalence claim lost its `convex H` qualifier.**

Pre-image: "hold the two paths observationally equivalent over the whole fixture set
**for convex H**". Post drops the qualifier, then the following paragraph explains
convexity is not even sufficient for point-equality. The callout repairs the reading,
but the headline sentence is now stated unconditionally. Also deleted without
replacement: the oracle test's name (`BorderModeMatchesRefactorizeMode`) and the
fixture the scoping correction was measured on
(`tests/test_sqp_restoration.cpp`'s `InfeasibleCircleLineModel`) — those are
evidence pointers into the live test tree, not history.

**N8 — SEVERITY LOW. Doc-citation policy applied inconsistently.**

The sweep deleted citations to `docs/notes/2026-07-29-eqp-refinement-ab.md`,
`…-accelerate-audit-results.md`, `…-identification-stall-study.md`,
`…-scale-study-cold.md`, `docs/retarget-design-sqp.md SS7.2` — correctly, since none
of those files exists in this repo (checked `docs/notes/`, which starts 2026-08-14;
`docs/retarget-design-sqp.md` *does* exist). But the same dead citations survive
elsewhere and the labels they anchored were kept:

- `qp_engine.h:653` / `:722` / `:3415` — "Audit finding D9" with its source removed
  (now unresolvable).
- `qp_engine.h:3124` / `:5251` — "SS7.2" kept while the `docs/retarget-design-sqp.md`
  pointer was stripped, even though that file *does* exist.
- `qp_engine.h:1602` — `docs/notes/2026-07-30-scale-study-cold.md Sec. 5` retained,
  while the same citation was deleted from `qp_types.h`'s `max_iter` doc.
- `ssn_engine.h:57` — `docs/notes/2026-08-07-ssn-safeguards.md` retained; that file
  is not in this repo.

---

## Claims verified TRUE against the code (the report's "written under uncertainty" list)

- **Reuse condition (e)** (`qp_engine.h:300-311`) matches `qp_engine.h:3135-3145`
  exactly: `factor.session_id() == prev_border_kkt_session_id && factor.epoch() ==
  prev_border_kkt_epoch && factor.inertia().state == kObserved`. The dropped clause
  ("the SS7.2 conjunct that covers what generation's bump-before-mutate used to")
  was pure provenance; the technical content survives verbatim.
- **HotState ownership, two-mechanism summary** (`qp_engine.h:2124-2185`): detach as
  load-bearing / identity pair as defense-in-depth is what the code does, and the
  condensation of the "DETACH ONLY WHEN ACTUALLY SHARED" paragraph correctly inverts
  the deleted round-1/round-2 narration into a direct statement matching
  `qp_engine.h:3196-3203` (`use_count() > 1` → `make_shared`, else wipe in place).
  *Caveat (pre-existing, untouched):* the header at `:2144` still says detach happens
  "at every `!reuse_eligible` site", which the `use_count()` guard makes false. Worth
  correcting while that block is open.
- **`qp_types.h` static_assert message** — accurate: the assert tests
  `is_same_v<Eigen::Index, hven::Index>` and the message describes exactly that plus
  the Apple `long long` / `long` split, which is true on macOS arm64.
- **`max_iter`'s three claims** — all three hold against
  `detail::effective_qp_max_iter` (`qp_engine.h:1605-1607`): positive wins outright
  (`requested > 0 ? requested : …`); sentinel derives
  `max(kQpMaxIterFloor, kQpMaxIterCoeff * (n + mi + #bounded))`
  (`qp_cap_base` + `derived_qp_max_iter`, `:1584-1597`); the honest-failure statement
  is unchanged from the pre-image.
- **`ssn_engine.h` section 7b Accelerate corollary** — re-flow only; no clause lost.
- **`qp_problem.h` `validate()` docstring** — `@throws` is on the throwing
  declaration, and "dimensional consistency of every block … and the upper-triangle
  convention for H" is precisely the set of checks in the body (H shape, H lower-tri
  entry, Ae cols, be size, Ai cols, bi size, lower size, upper size). It does *not*
  claim a `lower <= upper` check, which the function indeed does not make.

---

## (c) Residual labels at 5befdf1 — every hit with line

### `eqp_solve.h` (2)
- `:47` — "signals that row should be DROPPED from the working set (Task 9)."
- `:49` — "Task 9 must price them from the stationarity residual at a non-free"

### `qp_engine.h` (29)
- `:30` — "a caller assembling a child QP at runtime -- Phase 3's trust-region box"
- `:264` — "(see assemble_kkt_core, and the PHASE-3 DESIGN FRICTION note"  *(also B2)*
- `:280` — "PHASE-3 DESIGN FRICTION note) is byte-identical to the pair the"  *(also B2)*
- `:653` — "is no longer the last word. Audit finding D9 showed the failure"
- `:722` — "MEASURED, BOTH BACKENDS' COSTUMES OF D9. On Accelerate the singular"
- `:1034` — "stops the ride, so the QP is genuinely unbounded below. Phase 3's"
- `:1426` — "Phase 3 always calls through a finite trust region, so"
- `:2011` — "task brief."
- `:2051` — "PHASE-4 TASK 4: hoisted from a nested class member to a free struct in"
- `:2267` — "contract's PHASE-3 DESIGN FRICTION note -- so this is byte-identical"  *(also B2)*
- `:2268` — "to the pre-Task-2 engine."
- `:2309` — "PHASE-4 TASK 4: a shared, opaque snapshot of this engine's CURRENTLY"
- `:2589` — "PHASE-3 DESIGN FRICTION note). Shared with run() so refine_on_face()"  *(also B2)*
- `:2835` — "PHASE-4 TASK 4: ADOPT AN EXTERNAL HOT HANDLE. `hot` (WarmStart::"
- `:2911` — "SolveOverrides and the header contract's PHASE-3 DESIGN FRICTION"  *(also B2)*
- `:2977` — "is what makes that path bit-identical to the pre-Task-9 engine:"
- `:2983` — "TODO(Phase 5 perf pass): this deep-copies H/Ae/Ai/g/be/bi every"
- `:3124` — "The USABLE-NUMERICS conjunct (SS7.2, the ordered-pin closure):"
- `:3415` — "fiction (audit finding D9)."
- `:3434` — "branch above uses (FIX ROUND 1 Q3 + FIX"
- `:3765` — "C1 defect described above."
- `:4574` — "PHASE-5 TASK 4 REMOVED A SECOND RELEASE CONDITION, and the measurement"
- `:4768` — "ledger.reserve(ledger.size() + 1); // FX-8; see the pin loop above"
- `:4778` — "ledger.reserve(ledger.size() + 1); // FX-8; see the pin loop above"
- `:5111` — "PHASE-6 TASK 3 FIX ROUND 1: `counters` is written and never read here --"
- `:5193` — "PHASE-3 DESIGN FRICTION note above this class explains why tr_radius/"  *(also B2)*
- `:5219` — "PHASE-4 TASK 4: held through a std::shared_ptr, not by value, so that"
- `:5242` — "Condition (e), FIX ROUND 1 retargeted onto the factor's identity (M3"
- `:5251` — "SS7.2 usable-numerics conjunct, which covers the failed-rebuild case"

### `ssn_engine.h` (21 plan/review labels)
- `:7` — "ssn_engine.h -- THE SEMISMOOTH-NEWTON QP KERNEL (Phase 7, Tasks 3 and 4)"
- `:17` — "WHY IT EXISTS. Phase 5/6 established that the walk's cost at scale is in the"
- `:136` — "solve, which is a thing a Task-5 caller sizing a budget needs to know."
- `:416` — "**FOUR OF THESE STEPS HAVE AN OPT-IN ALTERNATIVE AS OF PHASE-7 TASK 6b PHASE"
- `:717` — "shipped surface and are NOT renamed here -- that is a Task-5-interface"
- `:853` — "PHASE-7 TASK 6b PHASE B -- THE RESEARCH LEVERS' OWN CONSTANTS"
- `:1060` — "QpStatus::kNumericalError. A Task-5 driver that switched on status alone"
- `:1185` — "12 is the brief's registered value, kept rather than re-derived: every"
- `:1238` — "PHASE-7 TASK 6b PHASE B -- THE FOUR RESEARCH LEVERS (R5/R1/R2/R4)"
- `:1368` — "PHASE-7 TASK 6b PHASE B, R5. True iff this solve reached a certifying"
- `:1379` — "PHASE-7 TASK 6b PHASE B -- THE TWO LEVER INSTRUMENTS."
- `:1558` — "out rather than printed (tycho rule T6 -- a diagnostic this file does not"
- `:1797` — "--- convergence, and THE SECOND-ORDER VERIFICATION (C1) ----"
- `:1826` — "both true and useless, and it is the value a Task-5 caller would"
- `:1924` — "rather than discarded (tycho rule T6). Under the"
- `:2305` — "the single most load-bearing line in the safeguard set (Fable"
- `:2747` — "exists to serve, and pushing every Task-5 caller into sanitising a hint"
- `:2780` — "downstream estimate, and a Task-5 ingest that hands one model's z to a"
- `:2863` — "in the Task-3 report rather than left to look like an untested line."
- `:3545` — "correct-hint payoff, which is what gate G1 is scored on, would be gone."
- `:3605` — "The pattern cache (M5): the structure key k_ was built for -- both"

### `ssn_engine.h` — the `R1/R2/R4/R5` lever vocabulary (36 lines, separate decision)

`R1`/`R2`/`R4`/`R5` are used as *shipped identifiers* for four `SsnOptions` fields
(`sigma_rule`, `hint_rule`+`watchdog_q`, `infeasibility_rule`, `defer_certification`)
and appear in section headers, field docs, member names' comments, and inline
markers: `:422, :426, :429, :431, :861, :891, :901, :1250, :1286, :1314, :1331,
:1387, :1391, :1638, :1664, :1743, :1753, :1764, :1793, :1838, :1888, :1923, :1947,
:1961, :2015, :2114, :2320, :2375, :2489, :2626, :3301, :3329, :3629, :3636`
(plus `:1238`, `:1368` above). These are plan labels that hardened into a file
vocabulary — stripping them means renaming the concepts, not deleting a parenthetical.
Flagging for a controller ruling rather than as a finding.

### `qp_problem.h`, `working_set.h`, `qp_types.h` — zero label hits. Clean.

---

## (d) Doxygen form on rewritten declarations

| File | Verdict |
|---|---|
| `qp_types.h` | Clean. `///` blocks on `BoundState`, `WorkingSetLinearAlgebra`, `QpOptions`, `SolveOverrides`; `///<` trailers on the three `SolveOverrides` members; one `@brief` per block; no promised-but-absent tags. |
| `qp_problem.h` | Clean. `@brief`/`@throws` on `validate()` (throws-tag on the throwing declaration ✓); `///<` trailers on all members; the H-triangle callout correctly left as an in-body `//` comment, not a docstring. |
| `working_set.h` | Clean. `@brief`/`@throws` on `add_ineq`/`drop_ineq`, `@p row` used correctly; `///` one-liners on `active_ineq()`/`num_free()`. |
| `eqp_solve.h` | Acceptable. `EqpResult::refine_steps` uses a `///` block with no `@brief` — permitted for a member one-liner, but it is five lines of prose; consider `@brief` + remainder. |
| `qp_engine.h` | No `///` introduced; all rewritten text stayed `//`. Consistent, and defensible for a header whose declarations are all inside one class. |
| `ssn_engine.h` | **Defective — see B3.** Two half-converted blocks (`:1099`, `:1102`). No other `///` was introduced. |

---

## INVENTORY — `qp_engine.h` @ 5befdf1

5257 lines total, **3603 comment lines** (68.5%), 175 comment blocks.
Classes are a *partition* (each block gets one primary class); "label-bearing" is an
**overlay** counted on top, = lines living in a block that carries ≥1 plan/review label.
Ranges are approximate paragraph boundaries inside the 1477-line header contract
(`:6-1482`) and whole blocks elsewhere.

| Class | Lines | % of comments |
|---|---:|---:|
| Derivation (argument chain) | **1754** | 48.7% |
| Contract / invariant statement | **1803** | 50.0% |
| History / narration | **46** | 1.3% |
| *(overlay)* label-bearing | *683* | *19.0%* |
| *(overlay)* history-marker phrasing (`used to` / `no longer` / `formerly` / `Measured:`) | *707* | *19.6%* |

Contract total includes 243 lines in the 90-odd sub-6-line descriptor blocks, which
are almost all legitimate class-2 descriptors and need no pass.

### Largest derivation blocks (condensation targets — 1754 lines total)

| Range | Lines | What |
|---|---:|---|
| `358-403` | 46 | UNCONDITIONAL FULL RECONCILIATION (sync_borders argument) |
| `926-960` | 35 | §4c "WHAT GOES WRONG WITHOUT THIS" |
| `1053-1097` | 45 | §4c cost/unreachability/behaviour-not-identical |
| `1163-1198`, `1199-1215` | 53 | TRUSTWORTHY RANGE + RUNAWAY-GUARD RANGE |
| `1518-1578` | 61 | size-derived cap: the whole calibration derivation |
| `1736-1795` | 60 | suspect-stall gate, invariant half (ii) |
| `2086-2224` | 139 | HotState + OWNERSHIP / PARDISO-handle safety argument |
| `2586-2681` | 96 | `resolve_effective_options` VALIDATE-FIRST + the failure-mode evidence table |
| `3151-3195` | 45 | DETACH vs wipe-in-place |
| `3612-3661` | 50 | export invariant "WHAT WAS WRONG" |
| `4561-4612` | 52 | pins-only dead-end release condition |
| `4964-5025` | 62 | ride-sign selection |
| others (≈50 blocks) | ≈1010 | §1 homotopy, §3 labeling divergence, §4b probe/anti-cycling/restart, §4c arming, §6 crossed-bounds proof, hash/mixing rationale, walk helpers |

### Largest contract/invariant blocks (1803 lines — mostly keep)

| Range | Lines | What |
|---|---:|---|
| `251-337` | 87 | HOT-START REUSE conditions (a)-(e) — the file's central contract |
| `423-461` | 39 | THREAD SAFETY |
| `761-796` | 36 | POST-PROBE RESTART spec (TRIGGER / BUDGET / …) |
| `1107-1147` | 41 | §5 TERMINATION |
| `2356-2426` | 71 | tier-3 `refine_on_face` contract |
| `1442-1482` | 41 | §6b export invariant + one-way guarantee + kFixed |
| `2022-2063` | 42 | `BorderState` field semantics |
| `1263-1314` | 52 | TR-tight activity set + window-consistency rule |
| others | ≈1394 | section headers, per-constant descriptors, counter semantics, small in-body descriptors (243) |

### History / narration (46 lines — delete candidates)

| Range | Lines | What |
|---|---:|---|
| `722-733` | 12 | "MEASURED, BOTH BACKENDS' COSTUMES OF D9" |
| `882-894` | 13 | "NO NEW LEDGER KIND — the plan sketched …" |
| `1392-1412` | 21 | "THE REUSE-KEY EXTENSION THIS REQUIRED — Before this task …" |

The 46 is genuinely low; note the far larger **707-line history-marker overlay** —
history in this file is mostly one or two sentences embedded inside derivation
blocks ("that condition existed through …", "used to", "no longer"), not standalone
narration blocks. A condensation pass has to work sentence-level, not block-level.

---

## INVENTORY — `ssn_engine.h` @ 5befdf1

3649 lines total, **2231 comment lines** (61.1%), 144 comment blocks.
Same convention. Header contract is `:6-546` (541 lines).

| Class | Lines | % of comments |
|---|---:|---:|
| Derivation (argument chain) | **749** | 33.6% |
| Contract / invariant statement | **1340** | 60.1% |
| History / narration | **142** | 6.4% |
| *(overlay)* label-bearing | *422* | *18.9%* |
| *(overlay)* `R1`/`R2`/`R4`/`R5` lever vocabulary | *36 lines* | *1.6%* |
| *(overlay)* history-marker phrasing | *272* | *12.2%* |

Contract total includes 167 lines of sub-6-line descriptors.

### Largest derivation blocks (749 lines total)

| Range | Lines | What |
|---|---:|---|
| `75-88`, `89-138` | 64 | §1 THE RESIDUAL + BOUNDS ARE ROWS |
| `139-188` | 50 | §2 generalized Jacobian, why the assembled matrix is symmetric |
| `189-212` | 24 | D large-where-inactive |
| `241-274` | 34 | §4 activity hint = one PDAS step |
| `302-310` | 9 | derivation of the default tolerance |
| `495-522` | 28 | one-factorization cost / provably inert |
| `606-625`, `641-670` | 50 | cancellation-free `phi`, uncertain band |
| `708-750` | 43 | LM-regularized ladder + ceiling-slack derivation |
| `761-846` | 86 | infeasibility telemetry (three stall degrees of freedom) |
| `901-938` | 38 | R4 Farkas constants |
| `3328-3356` | 29 | R4 Farkas residual test derivation |
| `3487-3546` | 60 | branch selection / three-set partition / tie policy |
| others | ≈234 | proximal apply, dual projection, inertia gate, bound-row split |

### Largest contract/invariant blocks (1340 lines — mostly keep)

| Range | Lines | What |
|---|---:|---|
| `370-415` | 46 | §7 the safeguarded iteration, step-by-step |
| `311-358` | 48 | §6 per-solve seam / TR box / TR pins / soft radius |
| `213-240` | 28 | §3 fixed sparsity pattern (the load-bearing property) |
| `1020-1061` | 42 | `SsnEscape` reason semantics |
| `1490-1520` | 31 | `tr_violation` contract |
| `1250-1348` | 92 | the four lever field docs (R5/R1/R2/R4) |
| `1408-1487` | 79 | counters + implied active set + tr_active |
| `2845-2864` | 20 | FB-diagonal placeholder guarantee |
| others | ≈954 | option/field docs, section headers, in-body descriptors (167) |

### History / narration (142 lines — delete candidates)

| Range | Lines | What |
|---|---:|---|
| `17-26` | 10 | "WHY IT EXISTS. Phase 5/6 established …" |
| `53-56` | 4 | the earlier-implementation saddle defect *(see N2)* |
| `457-484` | 28 | §7b "the first implementation ran the convergence test …" |
| `523-546` | 24 | §8 REFERENCES (public-mathematics bibliography — arguably keep; not one of the four classes) |
| `2865-2940` | 76 | "THE REUSE — the first version rebuilt K from a triplet list on EVERY solve …" |

`2865-2940` is the single largest condensation win in this file: a 76-line
before/after performance narrative whose surviving contract is two sentences.
