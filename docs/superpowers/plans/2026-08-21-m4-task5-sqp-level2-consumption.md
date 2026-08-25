# M4 Task 5 — SQP Level 2 Consumption Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The SQP driver consumes Level 2 (`NlpAggregate`) through the
`NlpModelAggregate` bridge, byte-identically to today's direct `NlpModel`
consumption, per the design of record `docs/notes/2026-08-21-m4-task5-design.md`
(read §1–§7 before any task; it is the binding spec) and the settled
shape-amendment ruling (M4 ledger, 2026-08-21 entries).

**Architecture:** A consumer-side seam (`AggregateEvalSeam`) between the driver
and `NlpAggregate`: a claim pass maps the bridge's claim stream onto one
consumer-owned CSR-ordered [H | Ae | Ai] value arena (the location table IS the
walk-order→CSR permutation), per-major contiguous segment copies feed the
driver's existing `NlpEval`/`QpProblem` objects bit-identically, and the
per-minor QP path is untouched. Three new SQP-owned `EvalRequest` shapes
(9–11) reproduce the driver's exact model-call bills.

**Tech Stack:** C++20, Eigen (vendored), fmt, gtest/ctest, CMake+Ninja,
clang++. Build dir: `.scratch/task-5/build` (Release), `.scratch/task-5/build-debug` (Debug).

## Global Constraints

- Branch `m4` of /home/ghecht/Projects/hven ONLY. Commit locally; NEVER push
  (the controller pushes; holder-only rule). Never touch `main`.
- Configure: `cmake -B .scratch/task-5/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang` (Debug analogously in `.scratch/task-5/build-debug`). Before ANY build: `pgrep -f 'ninja|clang' ` and wait if another lane is building (one build on the box at a time).
- `MKL_NUM_THREADS=1` on every test/bench execution.
- Byte-identity is the product: the 30-row `kPins` warm-start battery, every
  counter, every recorded float column must be preserved. Any test that
  compares counters or floats must pass UNCHANGED unless this plan's task
  text explicitly licenses the change.
- Comment style: structured Doxygen (`@brief` etc.), terse declarative
  content; rationale lives in the design note/ledger, pointed to, not pasted.
- No PM labels (Task-N, R-numbers, "M4") in shipped code or committed test
  names; they may appear in commit messages and docs/notes only.
- Naming: `PascalCase` types, `snake_case` functions, `snake_case_` members,
  `kPascalCase` constants. Errors: `throw std::invalid_argument(fmt::format(...))`,
  never a print, never an unthrown exception. No `exit()`.
- Commit prefixes `feat:`/`fix:`/`test:`/`docs:`; every commit message states
  the test-registration delta (e.g. "+4 registered") — the count chain is audited.
- New third-party dependencies: forbidden.
- SNOPT source firewall: never read `~/Software/snopt7`.
- Never fabricate Apple/Accelerate values; Apple-conditional code compiles
  but its observed values stay UNOBSERVED.

---

### Task 1: Contract amendment completion (shapes 9–11, support refusal, tests)

**Files:**
- Modify: `include/hven/model/candidate_point.h` (MOSTLY DONE — verify)
- Modify: `include/hven/model/nlp_aggregate.h` (DONE — verify)
- Modify: `src/model/non_linear_program.cpp` (comment done; refuse arm NOT done)
- Modify: `tests/model/test_contract_flags.cpp`
- Modify: `tests/model/test_aggregate_contract.cpp` and/or
  `tests/model/test_nlp_model_aggregate.cpp` (whichever holds the engine/bridge
  assemble fixtures — read both first)

**Context:** The working tree already carries uncommitted edits from the
controller (the designer): candidate_point.h has the five-rules block, the
union-of-families mapping-table header with per-provider support statement,
rows 9–11, the re-scoped kObjectiveValue bullet, three new constants
(`kRequestLagrangianHessian` = ObjectiveHessian|ConstraintAdjointHessian,
`kRequestGradientAndJacobians` = ObjectiveGradient|ConstraintJacobian,
`kRequestConstraintJacobiansOnly` = ConstraintJacobian), the extended
`is_legal_request`, and the extended `validate_eval_request` message;
nlp_aggregate.h has the two re-worded passages; non_linear_program.cpp has
the amended dispatch comment. Verify these against the design note §4 and the
mapping-table text, fix anything malformed, then complete what remains:

- [ ] **Step 1: The engine refuse arm.** In `src/model/non_linear_program.cpp`
  `assemble_impl`, the dispatch currently ends `} else { this->full_kkt_pass(...); ... }`.
  Change to `} else if (request == kRequestFullKkt) { <the same body> } else { <terminal refuse> }`.
  The refuse throws `std::invalid_argument` via `fmt::format`, naming (a) the
  request mask in hex, (b) this provider ("the partitioned evaluation engine
  serves the eight shapes it grew; see the mapping table's per-provider
  support statement in model/candidate_point.h"), and (c) the legal-but-
  unsupported distinction. Match the entry-validation throw style already in
  the file.
- [ ] **Step 2: Fix the three colliding tests in `tests/model/test_contract_flags.cpp`.**
  (a) `kMappedRequestSets` gains the three new sets — IF the array is used by
  tests that assume interior-family invariants, split into
  `kInteriorOwnedRequestSets` (8) and `kSqpOwnedRequestSets` (3) with a
  combined view. (b) `ExactlyTheEightMappedSetsAreLegal` → rename
  `ExactlyTheMappedSetsAreLegal`, expected legal count over all 2^7
  combinations becomes 11. (c) `EveryShapeThatNamesADerivativeAlsoNamesItsValues`
  re-scopes to the interior-owned sets, with a companion assertion that each
  SQP-owned set does NOT name the values its family deliberately omits
  (shape 9 names neither value kind; shape 10 names no constraint values and
  no objective value; shape 11 names no values at all). (d)
  `AnUnmappedCombinationIsRefusedByName` currently uses bare
  `kConstraintJacobian` as its unmapped example — that is now legal shape 11;
  replace the example with `EvalRequest::kObjectiveHessian` alone (still
  unmapped) and keep the message assertions.
- [ ] **Step 3: New shape tests in test_contract_flags.cpp.** For each of
  shapes 9/10/11: legality (`is_legal_request` true, `validate_eval_request`
  no-throw); multiplier consumption (`request_consumes_multipliers` true for
  shape 9, false for 10 and 11); distinctness from all prior sets.
- [ ] **Step 4: Bidirectional support pins.** Engine side: an existing
  `NonLinearProgram` assemble fixture sends shape 9 (and one of 10/11) —
  EXPECT `std::invalid_argument` whose message contains "support" and the
  mask; ALSO verify shape 8 still runs (the explicit arm did not break the
  fallback body). Bridge side: an `NlpModelAggregate` fixture sends shapes
  9/10/11 — served without throw, with correct outputs (shape 9: KKT values
  match `eval_hess` composed Lagrangian; shape 10: gradient arena + Jacobian
  claims filled; shape 11: Jacobian claims filled, gradient arena untouched).
- [ ] **Step 5: Per-shape evaluator-set pins (falsification style).** Using
  the existing call-counting model fixture (find it in tests/model/support/ —
  the bridge tests count model calls; reuse that fixture), pin the exact
  evaluator set per new shape: shape 9 → exactly one `eval_hess`, zero of
  every other evaluator; shape 10 → one `eval_grad` + one `eval_jac_e` (me>0)
  + one `eval_jac_i` (mi>0), nothing else; shape 11 → the Jacobian pair only.
  Falsification: the pins must be shown capable of failing — write one
  EXPECT that would fail if shape 9 also called `eval_f` (i.e., assert the
  count is EXACTLY zero, not at-most).
- [ ] **Step 6: Build + run affected test targets** (Release):
  `ctest --test-dir .scratch/task-5/build -R 'ContractFlags|AggregateContract|NlpModelAggregate|EvalRequest' --output-on-failure` (adjust -R to the actual
  registered names). Then run the FULL model-tests directory
  (`ctest --test-dir .scratch/task-5/build -R 'model' ...` or the suite label
  used by tests/model/CMakeLists.txt).
- [ ] **Step 7: Commit** with prefix `feat(model):`, message naming the
  amendment ruling and the registration delta.

**Interfaces produced:** shapes 9–11 usable by later tasks; the engine
refuses them; the bridge serves them.

---

### Task 2: AggregateEvalSeam — the consumer-side binding

**Files:**
- Create: `include/hven/detail/drivers/aggregate_eval_seam.h`
- Create: `src/drivers/aggregate_eval_seam.cpp` (+ add to src/CMakeLists.txt
  next to sqp_driver.cpp)
- Create: `tests/sqp/test_aggregate_eval_seam.cpp` (+ register in
  tests/sqp/CMakeLists.txt)

**Requirements (design note §2–§3 binding):**

Class `hven::solvers::AggregateEvalSeam`, constructed from
`NlpAggregate &` plus `NlpModelAggregate`'s claim-stream accessors. NOTE: the
claim-stream surface (`kkt_claim_rows()/kkt_claim_cols()`,
`hessian_claims()/equality_jacobian_claims()/inequality_jacobian_claims()`,
`objective_gradient_claim_rows()`) lives on `NlpModelAggregate`, not on the
`NlpAggregate` base — the seam therefore takes `NlpModelAggregate &` in this
milestone (the driver consumes Level 2 THROUGH THE BRIDGE, per the ratified
plan; a claim-stream-bearing base interface is a later-milestone question and
NOT yours to add).

- [ ] **Step 1: Lay.** From `declaration()` capture n/me/mi and materialized
  lower/upper bound vectors. From the claim stream build: (a) sorted-CSR
  row-major patterns for H (upper triangle, assembled rows/cols < n), Ae
  (assembled row − n), Ai (assembled row − n − me); (b) ONE owned value
  arena `Vec arena_` sized total KKT claims, laid [H-CSR | Ae-CSR | Ai-CSR];
  (c) the location vector: claim slot → arena offset of that (row,col)'s CSR
  position (this is the walk-order→CSR permutation); (d) a
  `KktLocationTable`-conforming object over it (read
  `include/hven/model/claim_space.h` for how one is constructed; no clash
  locks needed — the bridge is one serial piece; use the uncontested form);
  (e) the objective-gradient arena (size n) with its `RhsLocationTable`
  (identity mapping; read claim_space.h for the dropped-row sentinel rules);
  (f) record `structure_epoch()` as `epoch_at_lay_`.
- [ ] **Step 2: Evaluation methods**, each first re-reading
  `structure_epoch()` and re-laying on mismatch (then the caller-visible
  outputs carry the new structure; a re-lay invalidates cached patterns):
  - `NlpEval eval_nlp(const Vec &x, const Vec &lambda_e, const Vec &lambda_i)`:
    `evaluate_candidate_values` into an `NlpEval`'s f/ce/ci (allocate exact
    sizes; empty blocks for me/mi == 0) + one `assemble` with
    `kRequestGradientAndJacobians` (multiplier blocks empty in the
    CandidatePoint — the shape consumes none) writing the gradient arena +
    KKT view over the arena; zero the named arenas first (assemble
    ACCUMULATES — the contract's own discipline); then copy: gradient arena →
    `ev.grad`; Ae/Ai CSR segments → `ev.Je`/`ev.Ji` value arrays (patterns
    pre-built at lay, `makeCompressed` already-compressed). `all_finite`
    computed exactly as today's `eval_nlp` does (read
    include/hven/drivers/sqp_driver.h:2195 first and reproduce its checks —
    including the S-1 return-size validations, which the aggregate entry
    layer already performs; do not double-throw, but keep the finiteness
    semantics identical).
  - `NlpEval eval_nlp_values(const Vec &x)`: `evaluate_candidate_values`
    only; identical to today's eval_nlp_values semantics (sqp_driver.h:2270).
  - `void refresh_derivatives(NlpEval &ev, const Vec &x)`: shape 10 assemble;
    fills ev.grad/ev.Je/ev.Ji (the accepted-trial moment).
  - `void jacobians_only(NlpEval &ev, const Vec &x)`: shape 11 (the probe
    moment).
  - `QpProblem build_subproblem(const NlpEval &ev, const Vec &x, const Vec &lambda_e, const Vec &lambda_i, double obj_scale)`:
    shape 9 assemble with multipliers + obj_scale in the CandidatePoint (H
    segment of the arena, zeroed first) → H CSR values copied into `qp.H`
    (pattern pre-built); the rest of the QpProblem exactly as
    sqp_driver.h:2556 builds it (g/be/bi from ev, lower/upper from the
    seam's materialized bounds minus x).
  - Accessors: `n()/me()/mi()/lower()/upper()`, `epoch()` (last-read),
    `aggregate()` (the bridge ref).
- [ ] **Step 3: Unit tests** (fixture: a small NlpModel with me>0, mi>0, a
  non-trivial Hessian pattern — reuse an existing tests/sqp support model,
  e.g. an HS problem from the suite's support headers):
  (a) BIT-IDENTITY: `seam.eval_nlp(x, λe, λi)` vs the free
  `eval_nlp(model, x)` — f, grad, ce, ci exactly equal (`==`, not near), Je/Ji
  identical structure (rows/cols/outer arrays) AND bit-equal values;
  same for eval_nlp_values and for build_subproblem (H bit-equal,
  g/be/bi/lower/upper bit-equal).
  (b) PERMUTATION FALSIFICATION: corrupt one location entry in a copy of the
  seam's table (test hook or friend fixture — a `for-testing` mutation
  method is acceptable in the test support layer, not in shipped API) and
  show the bit-identity check FAILS — the pin can fail, so its green means
  something.
  (c) EPOCH RE-LAY: call `negotiate_partition_count(1)` on the bridge
  (bumps the epoch by contract), then a seam evaluation — assert it re-laid
  (epoch() advanced; outputs still bit-identical to direct eval).
  (d) ZEROED-ARENA DISCIPLINE: two consecutive eval_nlp calls at different
  points give each point's exact values (no accumulation leak).
- [ ] **Step 4: Build + run the new test target both configs.**
- [ ] **Step 5: Commit** `feat(sqp):` with registration delta.

**Interfaces produced (Task 3 consumes):** the exact method signatures above.

---

### Task 3: Driver rewiring — one consumption path

**Files:**
- Modify: `include/hven/drivers/sqp_driver.h`, `src/drivers/sqp_driver.cpp`
- Modify: `include/hven/model/nlp_model_aggregate.h`,
  `src/model/nlp_model_aggregate.cpp` (ONE additive change, Step 1)
- Test: existing suites are the test (byte-identity); plus one new entry test

- [ ] **Step 1: Const-model bridge construction.** Add an
  `explicit NlpModelAggregate(std::shared_ptr<const NlpModel> model)` overload
  (internal storage becomes `std::shared_ptr<const NlpModel>`; every bridge
  call is const — verify by compilation; the existing non-const ctor
  converts). This is an additive amendment to the bridge surface — note it
  in the commit message as boundary-flagged.
- [ ] **Step 2: Driver entries.** `SqpDriver::solve(NlpModelAggregate &, const Vec &x0[, const WarmStart &...])`
  becomes the primary path: constructs an `AggregateEvalSeam` and runs
  `solve_impl` against it. The existing `solve(const NlpModel &...)`
  overloads wrap: build a non-owning `std::shared_ptr<const NlpModel>`
  (aliasing constructor, no deleter concerns — document lifetime: the bridge
  lives only for the solve call), construct the bridge, delegate. The
  zero-arg-x0 overload reads `model.start_point()` BEFORE wrapping, as today.
- [ ] **Step 3: solve_impl + helpers rewire.** Change
  `solve_impl(const NlpModel &model, ...)` to take the seam; replace every
  `model.` use inside solve_impl, make_warm_start, and the ingest/probe block
  with the seam equivalent: `model.n()/me()/mi()` → seam accessors;
  `eval_nlp(model, x)` → `seam.eval_nlp(x, <current duals>)`;
  `eval_nlp_values(model, x)` → `seam.eval_nlp_values(x)`; the accepted-trial
  `eval_grad/eval_jac_*` block → `seam.refresh_derivatives(ev, x)`; the probe
  block's `model.eval_jac_e/i` → `seam.jacobians_only(probe_ev, x0)`;
  `build_subproblem(model, ev, ...)` → `seam.build_subproblem(ev, ...)`;
  `model.lower()/upper()` → seam bounds. COUNTER ATTRIBUTION UNCHANGED: every
  `++out.counters.*` stays exactly where it is. The restoration wrapper
  (sqp_driver.h ~:1666) wraps its restoration NlpModel in its own
  bridge+seam at restoration entry, same pattern.
- [ ] **Step 4: MF-5 hygiene.** After rewiring, `src/drivers/sqp_driver.cpp`
  and the driver header must not call `model.eval_*` anywhere (the
  include-graph/symbol sweep at the M4 gate checks this); `eval_nlp`/
  `eval_nlp_values`/`build_subproblem` free functions over NlpModel REMAIN in
  sqp_driver.h (tests and the seam's own bit-identity tests use them) but
  the driver's solve path must not touch them — grep and verify, state the
  result in the report.
- [ ] **Step 5: New test — entry equivalence.** `solve(bridge&)` vs
  `solve(model)` on two battery families (F1, F3n50; cold + warm arms):
  identical SqpSolution status, x (bit-equal), counters (all twelve),
  and WarmStart emission fields.
- [ ] **Step 6: FULL SUITE both configs** (`ctest` Release AND Debug, whole
  tree). EXPECTED: everything green with ZERO pin edits. If ANY
  counter/float pin moves, STOP — do not adjust any pin; report BLOCKED with
  the failing test and diff of observed vs pinned; the controller
  adjudicates (a moved pin means the seam broke bit-identity somewhere).
- [ ] **Step 7: Battery byte-identity evidence:**
  `MKL_NUM_THREADS=1 ctest --test-dir .scratch/task-5/build -R 'WarmStartBattery|ScaleF7Slow' --output-on-failure`
  plus `bench_scale --self-check` from the same build; paste the pass lines
  into the report.
- [ ] **Step 8: Commit(s)** `feat(sqp):` — may be split (bridge ctor /
  entries / rewire) with per-commit registration deltas.

---

### Task 4: W1–W5 model-surface scorer

**Files:**
- Create: `bench/model_surface_kkt.h` (header-only scorer, engine-independent)
- Modify: `bench/corpus_cells.h` or `bench/bench_corpus.cpp` (the hook that
  can emit scorer-vs-recorded columns when a flag is set)
- Create: `tests/model/test_model_surface_kkt.cpp`

- [ ] **Step 1: The scorer.** `model_surface_kkt_residuals(NlpAggregate &, const Vec &x, const Vec &lambda_e, const Vec &lambda_i, const Vec &z)`
  → `{stationarity, complementarity, primal}` (the same three the corpus
  gate reads), computed ONLY from `evaluate_candidate_first_order` outputs +
  `declaration()` bounds: stationarity = ‖grad_f + adjoint_grad − z‖∞ over
  NON-declared-fixed coordinates (the settled exclusion rule: skip variables
  whose materialized bounds have lower == upper — computable from
  declaration data alone); complementarity and primal residuals mirroring
  the driver's evaluate_kkt definitions (read them in sqp_driver.h first;
  same norms, same scale conventions so the comparison is apples-to-apples).
  It must not include any engine header (engine-independent by construction
  — the compile is the proof; keep includes to model/ + Eigen + core).
- [ ] **Step 2: Unit tests**: on a fixture with a known KKT point, residuals
  ≈ 0 (near-ulp); on a fixture with one declared-fixed variable whose
  gradient row is garbage, the exclusion keeps stationarity clean; on a
  perturbed point, residuals move (falsification).
- [ ] **Step 3: The census hook.** Behind an opt-in flag (env var or CLI,
  matching the harness's existing flag style), the corpus harness computes
  scorer residuals per cell beside the recorded ones and emits
  `wgate_scorer.csv` (cell id, three recorded, three scorer, per-cell
  verdict-equal boolean). DEFAULT OFF; zero behavior change when off (the
  flag must not touch any default code path — guard the entire hook).
- [ ] **Step 4: Build, tests, commit** `feat(bench):` + registration delta.

---

### Task 5: Proof battery, W-leg run, wall leg, close

- [ ] **Step 1: Full suites both configs**, final counts recorded (registered
  and executed, SNOPT-enabled), reconciled against the count chain from
  Tasks 1–4 commit messages.
- [ ] **Step 2: The combined census + W-leg run** (SERIALIZED, machine
  otherwise idle, `MKL_NUM_THREADS=1`, no other builds — this run takes
  hours; launch in background, monitor): the standing 57-cell census with
  the Task 4 hook ON, compared against the standing baselines
  (`bench/baselines/2026-08-16-u0-corpus/` + the gate-C census artifact per
  the runner's wired compare). REQUIRED: all asserted columns byte-identical
  AND the pre-registered W-leg criterion (ZERO verdict flips across all
  KKT-gated cells). Artifacts → `docs/notes/data/2026-08-21-m4-task5-wgate-leg/`
  with provenance header (commit, stamp, thread discipline).
- [ ] **Step 3: The wall leg** (the refuse arm made the engine TU
  solve-reachable): run the preserved instrument per
  `docs/notes/data/2026-08-21-m4-task2b-wall/wall_leg.sh` protocol with base
  = the grant commit `07d5ee1`, both-estimator + spread reporting. EXPECTED:
  inside the standing band (a terminal refuse arm after exact-equality
  tests should be wall-invisible); report the numbers either way.
- [ ] **Step 4: Ledger + evidence fold** (the controller writes the ledger
  close; the implementer's report must carry: final counts, census compare
  output, W-leg verdict line + max-disagreement table, wall-leg numbers,
  battery/self-check pass lines).

---

## Self-review notes

- Task 1's Step 4 engine fixture and Step 5 counting fixture: the exact
  fixture names live in tests/model/ — the implementer reads the two test
  files first and reuses; if no counting fixture exists, build a minimal
  CountingModel : NlpModel in tests/model/support/.
- Task 2 Step 1(d): if `KktLocationTable` cannot be constructed by a
  consumer (constructor private to the engine), the seam holds its own
  `std::vector<int>` locations + a thin adapter satisfying the
  `KktScatterView.locations_` type — read claim_space.h and do whichever the
  contract supports; if NEITHER works, report NEEDS_CONTEXT naming the
  blocker rather than modifying claim_space.h.
- Task 3 Step 3 is the risk center: the probe block (sqp_driver.cpp:285–425)
  has exact counter increments (`++out.counters.evals_values`) and
  short-circuit ordering that must not move.
