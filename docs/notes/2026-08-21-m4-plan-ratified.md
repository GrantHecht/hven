# M4 Plan — the two-level model contract

**Status:** RATIFIED 2026-08-21 — the text of record. Both plan gates
passed: the SQP lane's (reviewer-side `91c4ec1`, SIGNOFF
M4-PLAN-GATE-SQP-FINAL; its two required amendments A1/A2 and riders
folded, each marked) and the hven lane's (this repo,
`docs/notes/2026-08-21-m4-plan-gate-review-hven.md`, SIGNOFF
M4-PLAN-GATE-HVEN-FINAL, with the gate-passed addendum at `6fb4e5e`;
its four required amendments RA-1..4, three judgment rulings a/b/c, and
six minor folds MF-1..6 folded, each marked). Committed here per §7:
this repo's `docs/notes/` is the M4 sentinel carrier, and this file
supersedes the drafter-side working copy. M4 verdicts and rulings land
beside it; the M4 execution ledger is `docs/notes/2026-08-m4-ledger.md`.

**Drafted:** 2026-08-21, tycho controller lane, per the owner's lane-ownership
ruling (tycho drafts, hven reviews as a gate; implementation split by
lane, cross-review mirroring M3 with the center of gravity flipped).

**Basis:** rev B §7 + §10 [AGREED]; the SQP lane's requirements note
(reviewer-side `beff317`, SIGNOFF M4-SQP-REQUIREMENTS-FINAL — cited below
as R0–R5.6); the hven lane's eleven-item constraint slate (2026-08-21
channel message, banked in the tycho controller's requirements file);
the code survey of hven main `72baafd` and tycho main `e19f9b87`.

---

## 1. Goal and base state

**Goal:** both engines consume one partitioned evaluation contract
(Level 2); the native model (`NlpModel`, Level 1) bridges onto it as a
single serial piece; the Ipopt-shaped `NLPProblem` survives as a Level-1
convenience adapter over the native model; tycho's VF transcription
becomes a Level 2 provider; the conversion-equivalence suite
(tycho-owned) closes the milestone. Gate per rev B §10: NLP interface
suites + cross-checks — same optimum from the VF path, the native
model, and the adapter.

**Base state (verified 2026-08-21):**

- hven `main` = `72baafd` (M3 merged, PR #5 squash). Preservation tag
  `m3-branch-final` at old tip `658e9f4` pushed (Mac checkout points
  `bed76b2`/`fb45a00` are ancestors).
- tycho `main` = `e19f9b87`; `dep/hven` gitlink at `eb5c1f4`
  (checkout locally at `9ac4a03`) — both pre-M3; the bump to `72baafd`
  is Task 0 below. *(Corrected per plan-gate review A2, measured via
  `git ls-tree`.)*
- `model/` today holds Level 1 already: `NlpModel`/`ParametricNlpModel`
  (`include/hven/model/nlp_model.h` — split form, structure-fixed
  sparse returns, upper-triangle Lagrangian Hessian with `obj_scale`,
  pinned multiplier sign convention, opt-in `eval_values`, pattern
  invariance binding in x, args, and p), plus the Ipopt-shaped
  `NLPProblem`/`NLPSolver` and their adapter core.
- The partitioned evaluation engine is the orchestration half of
  `drivers/non_linear_program.h/.cpp` (piece lists, partition analysis,
  claim spaces/location tables, KKT clash locks, partition-fanned eval
  entry points), plus the fixed-variable-treatment and bound-staging
  subsystems that ride with it.
- `set_num_partitions(int, int)` — the conflated two-arg form — lives
  on `drivers/optimization_problem_base.h:83` (second arg forwards to
  `optimizer_->set_qp_threads`); callers exist in hven
  (`src/model/nlp_solver.cpp`) and across tycho's OC layer and Python
  bindings.
- The engine-side composite re-key is ALREADY LANDED (M3 H-series,
  merged): `combined_pattern_hash(H, Ae, Ai)` + bound-row conjunct,
  value-pinned. M4 must not re-open it (R0, R1.1).
- `HotState` EXISTS on merged main: defined at
  `include/hven/detail/qp/qp_engine.h:2247`, forward-declared at
  `detail/warmstart/warm_start.h:206`, riding the WarmStart currency
  as `shared_ptr<const HotState>` — the engine-owned hot handle,
  consumed at warm/hot ingest today and pinned by the 30-row
  warm-start battery. What does NOT exist yet is the model-level
  structure epoch it should record. `pattern_hash` is consumed only at
  factorization and assembled-QP scope. The model-level structural key
  and structure epoch are NEW surface. *(Corrected per plan-gate
  review A1.)*

## 2. Contract architecture (how the existing surface maps in)

Nothing already in `model/` is displaced. The mapping the hven lane
required the plan to state explicitly:

- **`NlpModel` IS Level 1.** The rev B §7 "standard model interface"
  clause list matches the landed header point for point (split form,
  structure-fixed returns, upper-triangle `obj_scale` Hessian, single
  signed `z` currency, values-only fast path, `ParametricNlpModel`).
  M4 adds NO new obligations to `NlpModel` implementers and does not
  re-word `eval_values` (the U0 ruling's wording is carried verbatim —
  hven constraint 2). What M4 adds AROUND it: the serial-piece bridge
  that makes an `NlpModel` consumable through Level 2.
- **`NLPProblem` becomes a Level-1 convenience adapter over
  `NlpModel`** (rev B §7: monolithic g / triplets / lower triangle /
  (z_L, z_U) → split / structure-fixed / upper triangle / single z).
  Today it reaches the engine through its own `NLPAdapterCore` pieces;
  after M4 it converts to the native model and rides the same bridge,
  so both engines implement exactly one consumption path and the
  adapter's conversion is testable in isolation.
- **Level 2 is the extracted partitioned evaluation contract**,
  provider-side, in `model/`: provider declares pieces + partition
  count; solver publishes where values land (claim spaces / location
  tables) and requests evaluations; the provider's aggregate fills
  through the tables using its own threads (ownership principle
  [RULED]: all provider parallelism is provider-owned;
  solver-internal threading stays solver-internal).

Consumption topology after M4:

```
tycho VF transcription ──────────────┐  (Level 2 provider, own threads)
NlpModel ── serial-piece bridge ─────┤  (Level 1 native)
NLPProblem ── adapter → NlpModel ────┘  (Level 1 convenience)
                                     │
                     Level 2 contract (model/)
                     claim spaces · location tables · structural
                     key + epoch · identity probe
                                     │
                ┌────────────────────┴──────────────────┐
        InteriorPointSolver                        SQP engine
        (consumes Level 2)                   (consumes Level 2)
```

## 3. New contract surface (the design content the review gates)

### 3.1 Model-level structural key and structure epoch

- The model structural key = assembled-pattern identity + **partition
  count** (rev B §7) + the bound-structure conjunct at model scope. It
  is distinct from and never folded into the engine digest, which stays
  a pure function of assembled matrices behind its value pin (R1.1;
  H1-RULING-FINAL literal is a stop-and-escalate constraint).
- A **monotone structure epoch**, readable at the consumption seam,
  bumps on pattern change AND partition-count renegotiation — before
  any post-renegotiation evaluation is observable (R1.2, R1.3).
  Renegotiation without an assembled-pattern change bumps it too
  (R3.3). Engines never infer structural events from digest drift.
- A **cheap identity probe** (structure epoch + values digest or
  equivalent) evaluable without a factorization and without a full
  derivative evaluation, keeping the probe-hash-on-every-
  subproblem-free-exit clause implementable (R1.4).
- **Global (var, row) identities are partition-invariant** (R1.5):
  partitions are an evaluation concern, never an identity concern; a
  renegotiation must not renumber the global space (M5's warm-currency
  remap across repartition depends on this — R4.5 derivability
  preserved, remap itself out of scope).

### 3.2 Claim spaces and location tables

- Split form end-to-end: H, Ae, Ai, and explicit bounds are distinct
  claims; splitting is done by the bridge, never by an engine (R2.1).
- Deterministic layout: for a fixed model structural key, claim-space
  layout and assembly order are identical across runs and
  evaluation-thread counts; layout freedom is fixed by the contract,
  never by scheduling (R2.2 — byte-stable pins and census sit on
  this).
- Direct scatter into engine storage in both storage states
  (upper-triangle and full), no intermediate copy on the per-minor hot
  path (R2.3). **Scope (per hven gate RA-2):** R2.3 binds NATIVE
  Level 2 providers' fill path. The Level-1 bridge intrinsically holds
  one model-owned intermediate (`NlpModel` evaluators return by value,
  and §2 forbids new implementer obligations), so for the bridge this
  is a DECLARED property mirroring R2.4's declared-degradation shape.
  Ownership: the scatter capability is DEFINED at Task 1; the
  engine-side binding is BUILT at Task 5 under that task's design
  custody; the SQP lane's verification rider applies to what Task 5
  built. This facility and RA-4's candidate-point surface are two
  faces of one design element — designed together at Task 1.
- Value claims separable from derivative claims, so `eval_values`
  survives at Level 2; the Level 1 bridge may degrade to full
  evaluation but must DECLARE it via a capability flag (R2.4).

### 3.3 Threading and the deconflation

- Evaluation-thread settings move to the provider API (rev B §7).
- Engine-side QP threading stays engine-owned; the contract never
  routes it through the model (R5.2). Partition count and linear-solver
  threads remain separate, separately-owned knobs; the FX-4
  authoritative-copy semantics on the linear surface are not disturbed
  (hven constraint 7).
- The two-arg `set_num_partitions(n, qp_threads)` is removed; tycho's
  callers and Python bindings migrate to the split knobs (Task 6).
  Public tycho API change — flagged for the owner per tycho's standing
  review list; break-API-freely applies (no grace shims).

### 3.4 Pinned conventions and non-foreclosure

- Multiplier sign/scale conventions at the seam are stated IN THE
  CONTRACT TEXT, not engine-defined (R4.2): `NlpModel`'s pinned
  stationarity convention is the seam convention; the `NLPProblem`
  adapter's (z_L, z_U) → single-z mapping and any IPM-internal
  convention conversions are documented at the adapter/engine boundary
  and asserted by the equivalence suite.
- The WarmStart currency stays neutral-core + engine-owned opaque
  extensions; Level 2 never requires currency contents in model terms
  (R4.1). Problem identity (model key incl. partition count + engine
  digest) is exportable (R4.3). `kSeeded` reachable from a foreign
  hand-off — nothing assumes warm data originated in-stack (R4.4).
  The existing `HotState` handle and its warm/hot ingest semantics are
  preserved; wiring MODEL-EPOCH recording into that handle is an M4
  design option under the SQP lane's custody (Task 5), not an
  obligation — M4's obligation is only that the epoch surface makes
  staleness detectable so a handle can record its mint epoch and
  downgrade (at best `kSeeded`, clear-then-clamp normative) instead of
  silently reusing (R3.1, R3.2).
- The model-level KKT gate (W1–W5) is implementable at the model
  surface with no engine cooperation: Level 2 exposes the evaluations
  needed to score a KKT residual at an arbitrary candidate point
  (R5.1).

### 3.5 Boundary-validation ownership (R5.3, hven constraint 6)

- **hven owns**: validation of provider/callback returns at the model
  boundary (dim-checks naming the callback in the fmt-formatted
  throw), options validated symmetrically on stored and override
  paths. If the QP options surface is touched, carries C-6/C-7 ride
  the same design.
- **tycho owns**: nanobind-boundary validation (T5 rule — Eigen
  asserts never the only guard) for every Python-reachable path the
  rebase touches, and its own VF-provider invariants before handing
  aggregates to Level 2.

## 4. Task breakdown

Owner key: **[T]** tycho lane, **[S]** SQP lane, **[H]** hven lane.
Every hven-repo task lands on an `m4` feature branch (PR to main at
gate, mirroring M3); every tycho-repo task lands on a tycho feature
branch (never direct to main). Proof-ladder class is stated per task
(hven constraint 5); the census runs at STRATEGIC points only (owner ruling, 2026-08-21, relayed via the drafting lane): the Phase 1 census of record is the Task 2b run; Tasks 3–8 carry no census legs and prove through the ladder (suite + bit-identical counters/traces + the standing bench compare with its wall bands); the Task 9 close gate carries the one remaining census; outside these, a census requires a trigger — a counter moved, a trace diverged, an unexplained band trip — never routine cadence.

**Phase 0 — pre-flight**

- **Task 0 [T]** (tycho repo): bump `dep/hven` to `72baafd`; run
  tycho's full pre-merge sequence against it (ctest incl.
  skipped-count check, 34 Python examples, C++ brachistochrone,
  bench compare); tycho PR. Proof: suite + bench counters
  (behavior-preserving consumption bump; any deltas declared).
  Everything else depends on this.

**Phase 1 — contract definition (hven repo)**

- **Task 1 [T]**: Level 2 contract surface in `model/` — the provider
  declaration (pieces + partition count), claim-space/location-table
  types as public contract, structural key + structure epoch + identity
  probe (§3.1), value/derivative claim separation (§3.2), provider
  thread API (§3.3), contract-text pins (§3.4), model-boundary
  validation (§3.5). Naming is origin-neutral (hven constraint 3);
  concrete names proposed in the task design note, which is reviewed
  and settled by the hven lane BEFORE the first Task 1 implementation
  commit — a named checkpoint (hven gate ruling c). Per hven gate
  RA-4: Level 2 exposes candidate-point aggregate evaluation into
  caller-owned split storage (equivalently: admits an independent
  consumer), and the model-surface W1–W5 scorer consumes THAT surface,
  never the bridge's underlying model — designed together with RA-2's
  scatter capability (one design element). New tests: contract-surface
  unit tests incl. the epoch ordering guarantee (bump observable
  before any post-renegotiation evaluation), the failure-restore path
  (a rejected reconfiguration that re-lays structures is a structural
  event and bumps the epoch under the same ordering guarantee — the
  in-tree `bounds_revision_` pair's failure handling is precedent;
  MF-4), the R1.5 identity-invariance pin, and the R2.2
  layout-determinism pin (same model structural key ⇒ identical
  claim-space layout and assembly order across runs and across
  evaluation-thread counts — the owning test of acceptance criterion
  4; hven gate RA-3).
- **Task 2 [T]**: extraction — the partitioned evaluation engine moves
  from `drivers/` into `model/` per migrate-then-restructure (hven
  constraint 4): **(2a)** file move with NO CONTENT EDITS —
  include-path/CMake retargeting only, enumerated in the commit
  message (proof: P-SYM per-symbol instruction identity with its
  licensed noise class; any difference outside that class is a FAILED
  MOVE to be redone, never a proof-class downgrade — hven gate ruling
  a). The M4 P-SYM runs DECLARE their object set before compare,
  extended beyond the standing SQP-centric set to cover the moved
  code's consumers (interior-side test objects added, or their
  exclusion justified in the task report). 2a moves a TU in the PCH
  membership table: the membership bar (faster AND byte-identical,
  measured never predicted) applies — re-measure or explicitly leave
  membership unchanged (MF-2, also binding at 2c); **(2b)** carve the
  solver-facing vs provider-facing surfaces onto the Task 1 contract,
  fixed-variable treatment and bound staging riding with the provider
  side (proof: suite + bench counters per the layout-band standard —
  counters bit-identical unconditional; +0.5 % pooled / +1.5 %
  sub-second one-sided wall bands (MF-1) — census leg); **(2c)** any
  renames/TU rationalization as a separate, separately-verified step
  or explicitly deferred. The
  KKT-clash lock table, partition analysis, and location-table builds
  keep their existing behavior — this phase moves and re-labels, it
  does not redesign.

**Phase 2 — consumption rebases**

- **Task 3 [T]** (hven repo): IPM consumes Level 2 — retarget
  `InteriorPointSolver`'s `NonLinearProgram` coupling onto the Task 1/2
  contract. Target proof: byte-identity where the retarget is
  mechanical; suite + counters + the 17-problem bit-identity corpus
  where not. IPARM surface untouched (else IPARM-SURFACE labeling +
  validation evidence). The IPM's renegotiation posture, stated: it
  holds no cross-structural-event hot state — on an epoch bump it
  re-runs structure analysis and solves cold, and its factorization
  reuse is already keyed at factorization scope by the linear layer's
  pattern hash and dies with the pattern; M4 preserves exactly that.
- **Task 4 [T]** (hven repo): the `NlpModel` serial-piece bridge — one
  serial Level 2 piece per rev B §7, bridge does the splitting (R2.1),
  declares its `eval_values` capability honestly (R2.4). The existing
  `NLPAdapterCore` piece machinery is the reference implementation to
  subsume. Proof: suite + counters; the `NlpModel` rebase onto the
  bridge rides the declared ladder (R5.4).
- **Task 5 [S]** (hven repo): SQP engine consumes Level 2 through the
  bridge — their design custody; binds R1.2/R1.3 epoch consumption,
  R3.2's ingest clauses (the SQP lane's own gate checklist), and the
  W1–W5 gate re-pointed at the model surface. Existing warm/hot ingest
  behavior and `HotState` semantics — and their pins (the 30-row
  warm-start battery included) — are preserved through the rebase;
  wiring mint-epoch recording into the existing handle at M4 is a
  design option under this task's custody. The SQP lane owns verifying
  R2.3's no-copy scatter property at this task's design review (it has
  no test pin today). Sequenced after Task 4; tree-ownership handoff
  per §7.
- **Task 6 [T]** (hven + tycho repos): `NLPProblem` rebased as the
  Level-1 convenience adapter over `NlpModel`; two-arg
  `set_num_partitions` removed and callers migrated (hven's
  `nlp_solver.cpp`, tycho's OC layer + bindings + stubs regeneration —
  binding PRs carry the stubs gate). Proof: suite + counters; tycho
  public-API change flagged to the owner.

**Phase 3 — tycho rebase and equivalence**

- **Task 7 [T]** (tycho repo): VF transcription becomes a Level 2
  provider — `OptimizationProblem::transcribe()` (and the OC-layer
  callers) declare pieces + partition count through the contract
  instead of assembling `NonLinearProgram` internals; tycho's
  evaluation threading rides the provider API. Proof: tycho full gate
  suite + bench compare (regressions justified or fixed).
- **Task 8 [T]** (tycho repo, tycho-owned): the conversion-equivalence
  suite — same optimum AND same multipliers from the VF path, the
  native model, and the adapter; both directions; range-split dual
  recovery included; asserts the §3.4 pinned conventions. This is the
  SQP lane's stated acceptance bar (R5.5), so it is acceptance
  criteria here, not a review surprise.

**Phase 4 — M4 gate**

- **Task 9 [T+S+H]**: gate battery — hven suites green; tycho full
  pre-merge sequence green; the M4 close census (the
  milestone's second and final, per the owner's strategic-points
  ruling; the Task 2b artifact is the first), naming its baselines of
  record (the U0 corpus baseline and the gate-C census artifact; MF-6); bench
  counters vs both baselines (byte-stable where pinned; any
  intentional break declared and re-derived); conversion-equivalence
  suite green; the SQP lane's ingest-clause checklist walked; then the
  close ORDER (hven gate RA-1): hven pushes the `m4-branch-final`
  preservation tag at the m4 tip → hven PR merges to main →
  `dep/hven` re-points to the hven main merge commit on the tycho
  feature branch → the tycho PR merges. No fresh clone may ever see a
  gitlink unreachable from hven main. Merge authority on both closing
  PRs is the owner's. Preservation invariants per rev B §10 hold
  throughout.

## 5. Acceptance criteria (the gate, enumerated)

1. Both engines consume Level 2 and only Level 2 (one consumption path
   each); `NlpModel` unchanged for implementers; `NLPProblem` converts
   over the native model. Named check (MF-5): an include-graph /
   symbol-use sweep over both engines, recorded in the gate note — the
   criterion is inspectable, not asserted.
2. Same optimum AND same multipliers across VF path / native model /
   adapter, both directions, range-split dual recovery (R5.5).
3. Structural key includes partition count at model level; engine
   digest unmoved (the H1 value pin literal did not change); epoch
   semantics per §3.1 pinned by tests.
4. Deterministic layout pinned across thread counts (R2.2 — owning
   test: Task 1's layout-determinism pin, per RA-3); `eval_values`
   path preserved at Level 2 with declared bridge capability (R2.4).
5. `set_num_partitions(n, qp_threads)` no longer exists; partition
   count and engine/linear threads separately owned (R5.2).
6. W1–W5 scoreable at the model surface engine-independently (R5.1).
7. All census legs, ladder proofs, and suite/bench invariants recorded;
   Mac-dependent values remain UNOBSERVED and are not fabricated.

## 6. Out of scope (named, so review can hold the line)

- WarmStart currency extraction, crossover wiring,
  `activity_rel_tol`/μ̂ consumption — M5. The existing `HotState`
  handle is preserved as-is; beyond the optional mint-epoch wiring
  (§3.4, SQP custody), all further HotState evolution is M5 (M4 only
  preserves derivability and staleness detectability).
- Repartition remap implementation — M5 (R4.5).
- the consumer project's frozen legacy-engine subtree deletion, the
  golden rig's old-seam arm tag conversion, Eigen/fmt
  transitivity — M6.
- Bindings/packaging under the hven name — M7.
- E2 globalization re-scope — after M5 (standing ruling).
- Mac legs — owner-scheduled O12 session (unchanged).

## 7. Process

- **Tree ownership (proposed convention, per hven constraint 11):** one
  mutating agent per repo at a time, tracked by an explicit token per
  repo. Default holders: hven repo → hven lane; tycho repo → tycho
  lane; the reviewer-side repo → archived read-only. A lane needing the other's
  tree requests it by SendMessage ("requesting tree: <repo>, task N")
  and holds it only between the grant message and its own explicit
  release ("releasing tree: <repo> @ <commit>"); the holder owns
  working tree, branches, builds, and CI-watching; non-holders read
  via `git show`/fetch only. For M4's Phase 1–2 hven work the tycho
  lane will hold the hven tree per-task with release at each task's
  commit+verdict; Task 5 hands it to the SQP lane the same way.
  Four amendments per the hven gate (ruling b): (1) EVERY grant and
  release is mirrored as one line in the M4 execution ledger
  regardless of channel health — the ledger lives at hven
  `docs/notes/2026-08-m4-ledger.md` on the m4 branch; if the message
  channel fails, full dated notes in the carrier are the escalated
  form (sentinel files are the carrier of record). (2) The OWNER
  preempts all tokens: any hold is void while the owner is working in that
  tree, and a lane receiving a grant checks `git status` before
  assuming a clean base. (3) A session restart VOIDS in-flight grants
  (peer identities change on restart) — re-request, never resume a
  pre-restart hold. (4) Only the current holder pushes to that repo's
  origin. Scratch builds under git-ignored `.scratch/<task>/`, swept
  at task close. Build parallelism and mutual ps-check rules unchanged
  (one build on the box at a time across all lanes).
- **`dep/hven` gitlink ownership across the milestone (hven gate
  RA-1):** Tasks 6–8 build tycho against Level 2, which exists only on
  hven's unmerged `m4` branch until Task 9 — so mid-milestone gitlink
  bumps to m4-branch commits ride tycho FEATURE branches only, never
  tycho main. Standing policy: hven pushes a preservation tag
  (`m4-branch-final`) at the m4 tip before the close merge (the M3
  squash lesson applied to M4's own pin); the Task 9 close order
  guarantees no fresh clone ever sees a gitlink unreachable from hven
  main.
- **Review lanes:** hven lane gate-reviews this plan and per-commit
  hven-surface changes; SQP lane plan-gates and holds the
  ingest-clause half plus R5.5 at the M4 gate; tycho-side risky diffs
  get the standing dual-lane treatment (Fable + Codex). Cross-lane
  verdicts land in hven `docs/notes/` with SIGNOFF sentinels.
- **Carrier:** hven `docs/notes/` is the M4 sentinel carrier; the
  ratified plan's hven-governing portion is committed in hven after
  gate review. Conversion of that portion to hven's naming/citation
  convention (the draft is saturated with identifiers hven's added
  lines may not carry) is OWNED by the tycho lane as drafter and
  verified by the hven lane at the commit inspection (MF-3).
  Tycho-only artifacts stay in the tycho repo.
- **Naming/citation:** no origin-project identifiers in added
  hven lines; no PM labels (Task-N/R-numbers) in shipped code —
  R-numbers live in plan/review text only; hven-committed text uses
  the M3 citation convention.

## 8. Global constraints (binding on every task)

The hven lane's eleven-item slate (2026-08-21) and the SQP lane's
R-series (reviewer-side `beff317`) bind in full; the load-bearing ones are
inlined above at their point of force. Additionally: tycho's standing
rules (conda env, -j6/no concurrent builds, never commit to tycho
main, notices/ untouched, stubs gate on binding PRs, exit-status
discipline, no fabricated Apple/Accelerate observations); hven's
CLAUDE.md §§1–6 (origin naming, migrate-then-restructure, boundary
validation, flag regime/LTO/ISA pins, boundary-preference testing +
HVEN_TESTING seam); eval_values wording carried, never re-strengthened;
census as gate/event instrument with the per-change proof ladder;
SNOPT firewall absolute.

---

*Review requested from: hven lane (hven-surface gate) and SQP lane
(plan gate + ingest-clause half). On both approvals the plan is
committed to hven `docs/` (hven-governing portion) and execution
begins at Task 0.*
