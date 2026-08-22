# M4 Task 5 — the SQP engine's Level 2 consumption: design

2026-08-21, the SQP lane, under its Task 5 hold (grant at ledger
`07d5ee1`). Design custody is this lane's; this note is the design of
record for the task, read by the settler against settled rulings and by
the drafting lane as boundary reviewer. One section (§4) asks a
contract question that BLOCKS implementation start; everything else is
decided here.

## 1. The consumption inventory (what actually touches the model today)

The driver consumes its model at six moments, each with an exact
model-call bill the battery's pinned counters and the documented
call-count invariants rest on:

| # | Moment | Model calls today |
| --- | --- | --- |
| E1 | Full first-order (`eval_nlp`) — cold start, hot rebuilds | `eval_f, eval_grad[, eval_ce, eval_jac_e][, eval_ci, eval_jac_i]` = 2 + 2·[me>0] + 2·[mi>0] |
| E2 | Values-only (`eval_nlp_values`) — trial points, ingest probe | `eval_values` (default: f + cE + cI value calls, no derivative) |
| E3 | Accepted-trial derivative refresh | `eval_grad[, eval_jac_e][, eval_jac_i]` — values deliberately NOT re-evaluated |
| E4 | Subproblem Hessian (`build_subproblem`) — per accepted iterate | `eval_hess(x, obj_scale, λe, λi)` alone, plus `lower()/upper()` reads |
| E5 | Zero-major ingest probe | E2 + `eval_jac_e + eval_jac_i` + E4 (via a probe subproblem), pattern-hashed engine-side |
| E6 | Metadata | `n/me/mi`, `start_point`, `lower/upper` |

Two facts shape everything below. First, the counters the 30-row
battery pins are DRIVER-attributed (the driver increments where it
calls), so the preservation bar is: the adapter issues the same
aggregate operations where the driver issued model calls, 1:1, and
whatever the provider does internally is its own bill. Second, every
downstream float path (KKT residual, funnel, `structural_hash`) reads
`NlpEval`'s row-major matrices and `QpProblem`'s blocks; if those
objects are bit-identical, every pin and census column downstream is
too.

## 2. Architecture: one seam, the driver logic untouched

A consumer-side binding — working name `AggregateEvalSeam`, final name
at implementation — sits between the driver and an `NlpAggregate &`:

- **Entry points.** `SqpDriver` gains `solve(NlpAggregate &, ...)`
  forms; the existing `solve(const NlpModel &, ...)` forms remain as
  conveniences that wrap the model in an `NlpModelAggregate` and ride
  the same path — Level 1 rides the bridge, one consumption path,
  exactly acceptance criterion 1's shape. The restoration phase's
  internal `NlpModel` wrapper rides its own bridge instance the same
  way.
- **The claim pass and the arena (the RA-2 engine-side binding).** At
  lay time the seam hands the provider claim cursors and answers each
  claimed `(row, col)` with its offset in ONE consumer-owned value
  arena, laid in exactly the CSR order of the row-major `H`, `Ae`,
  `Ai` the engine builds — [H | Ae | Ai] as three contiguous segments,
  upper-triangle H enforced at claim time by the same convention
  `QpProblem::validate` pins. The location table therefore IS the
  walk-order-to-CSR permutation, resolved once per structure; the
  per-call scatter is `values[table.location(slot)] += v` with no
  branch and no reorder. Storage state is resolved at claim time
  (`KktStorage`), per the settled design.
- **Per-major transfer.** After an assemble, the three arena segments
  are copied contiguously into the `QpProblem`/`NlpEval` matrices'
  value arrays (patterns constructed once per structure from the claim
  stream, identical in structure to today's conversions). These are
  per-MAJOR, bounded, contiguous copies on objects the driver already
  materializes today; the per-MINOR hot path — the QP engine's
  working-set iterations over the assembled matrices — is untouched by
  this task, which is precisely the scope RA-2 fixed and my lane
  accepted. §7 discharges the R2.3 rider on these terms.
- **Internal API preserved.** `eval_nlp`/`eval_nlp_values`/
  `build_subproblem` keep their shapes as seam methods producing
  bit-identical `NlpEval`/`QpProblem` objects: same sparsity
  structures (sorted CSR both before and after), same values (pure
  model outputs through the bridge's declared intermediate), same
  bound arithmetic (`declaration`'s materialized bounds replacing
  `model.lower()/upper()` — same vectors by construction). Driver
  logic, funnel/TR/SOC/elastic/restoration code, and every float
  expression stay byte-for-byte.

## 3. What maps cleanly today

- **E2 → `evaluate_candidate_values`** — assigns the same values the
  model returns; the bridge routes `eval_values`; `kValuesFastPath` is
  declared unconditionally (settled), so the probe economics carry.
- **E5's identity half** stays ENGINE-side: `structural_hash` over the
  rebuilt probe `QpProblem` is bit-identical because the patterns are,
  and the `WarmStart` currency is untouched — no M5 semantics move at
  M4 (R4.3's exportable identity gains the model key only when the
  currency itself is M5's to extract).
- **Epoch consumption (R1.2/R1.3/R3).** The seam records the epoch at
  lay and re-reads it (one atomic acquire) at each evaluation moment.
  On a mismatch: re-lay the claim tables, rebuild pattern-holding
  objects, and let the engine's existing pattern-keyed machinery see
  whatever structure results — the engine already treats a pattern
  change as a full structural event at factorization scope, and
  ingest-level provenance dies with it (at best `kSeeded`, per the
  ratified §3.4). No new engine state; the epoch is consumed at the
  seam, which is the only place claim-slot-indexed state lives.
- **Metadata** — from `declaration()`; `start_point` stays a Level-1
  convenience read taken before wrapping (the aggregate entry takes an
  explicit `x0`, as the contract has no start point and should not).

## 4. The contract question that blocks implementation: three missing shapes

E1, E3, E4, and E5's derivative fetches have NO legal `EvalRequest`
shape that reproduces their model-call bills. The eight named shapes
are the interior engine's own, bijectively (settled); consuming the
nearest supersets breaks pinned counters or evaluates work my engine
never asked for:

- E4 (`eval_hess` alone) has no Hessian-bearing shape without
  Jacobian: shape 8 adds 4–6 model calls per accepted iterate —
  counter-breaking, disqualified.
- E3 (`grad + Je + Ji`, values deliberately not re-evaluated) has no
  shape: 5 and 6 re-evaluate both residual blocks (+2 calls per
  accepted trial) and 6 adds the adjoint gradient besides.
- E5 fetches `Je + Ji` bare; shape 5 again re-evaluates residuals the
  probe already has.

**Proposed amendment — three SQP-owned shapes appended to the mapping
table, mirroring how the existing eight got there** (the table's own
text says the eight are "the evaluation shapes the partitioned engine
grew"; the second engine grows its three at its own consumption task,
and the union keeps the table's no-over-evaluation principle intact in
both directions):

- **Shape 9** `kRequestLagrangianHessian` = `kObjectiveHessian |
  kConstraintAdjointHessian` → exactly one `eval_hess(x, obj_scale,
  λ)`; consumes multipliers (validated full-length; the probe passes
  zeros, which are full-length by construction).
- **Shape 10** `kRequestGradientAndJacobians` = `kObjectiveGradient |
  kConstraintJacobian` → `eval_grad [+ eval_jac_e][+ eval_jac_i]`; no
  multipliers.
- **Shape 11** `kRequestConstraintJacobiansOnly` =
  `kConstraintJacobian` → `[eval_jac_e][+ eval_jac_i]`; no
  multipliers.

Then E1 = `evaluate_candidate_values` + shape 10 (six model calls, the
same six); E3 = shape 10; E4 = shape 9; E5 = values + shape 11 +
shape 9. Every driver-attributed counter is 1:1 with today.

The amendment's blast radius: three named constants, three disjuncts
in `is_legal_request`, three mapping-table rows, legality/masking/
determinism tests per new shape, and — pending one implementation-time
check — likely ZERO change in `NlpModelAggregate::assemble_impl` if
its dispatch is per-flag as its contract text implies. Both settled
invariants (masking never alters layout/digest/epoch; every legal
subset carries full determinism) extend to the new rows by the same
argument as the old ones. **This touches settled contract text
("exactly the eight named sets"), so it goes to the settler before I
write it, with the drafting lane at the boundary since the table is
their built surface.** Implementation of everything in §2–§3 that does
not depend on the new shapes can proceed meanwhile; the seam's
evaluation moments land last either way.

## 5. W1–W5 re-pointing without breaking the census

The corpus gate (W1–W5) scores a row's RECORDED residuals — the
driver's own diagnostics — against thresholds; the census asserts
those columns byte-identically. Re-pointing therefore lands as: a
**model-surface scorer** built on `evaluate_candidate_first_order` +
`declaration` bounds + the settled declared-fixed exclusion rule,
added to the harness as an INDEPENDENT verification leg and
demonstrated on the corpus — acceptance criterion 6 asks that W1–W5 be
SCOREABLE at the model surface engine-independently, and this makes it
so, with real output on all 57 cells. The RECORDED columns keep their
provenance (driver diagnostics, bit-identical floats), so the census
of record stands. Whether the recorded columns themselves ever switch
to the model-surface scorer is a declared re-derivation decision that
belongs at Task 9 with the owner and both lanes — deliberately NOT
inside this task's preservation envelope. (Expected at the e-16 scale:
the scorer's stationarity sum and the driver's disagree in float
order; the leg's report will quantify the agreement margin against the
1e-6 gate so Task 9 decides on evidence.)

## 6. The mint-epoch option: DEFERRED to M5, with the reason on record

Wiring the model epoch into the existing `HotState` at M4 would add a
field to an object the `WarmStart` currency carries — a currency
change with no consumer until crossover exists, reviewed properly when
M5 extracts the currency. The epoch is consumed at the seam (§3),
which owns every piece of claim-slot-indexed state this task creates;
the engine handle's staleness remains governed by the engine's own
structural/values hashes exactly as today. Option recorded, not
exercised.

## 7. Proof plan and the R2.3 rider

- **Suites both configs** (Release + Debug) green; count chain stated
  per commit in the ledger mirrors.
- **The 30-row battery, `ScaleF7Slow`, `bench_scale --self-check`,
  and the committed baseline re-scores**: byte-identical — the
  preservation argument is §1–§3's bit-identity of `NlpEval`/
  `QpProblem`, and these runs are its falsifiers.
- **Census leg** at the task's close (this phase's declared event
  ladder), against the standing baselines of record.
- **New tests**: seam claim-pass/permutation pins (including a
  deliberately-broken-permutation falsification), epoch re-lay
  behavior (bump between majors → re-lay + engine sees the event),
  shape legality/masking/determinism for §4's rows, the model-surface
  scorer leg, and an aggregate-entry battery arm proving
  `solve(NlpAggregate&)` ≡ `solve(model)` cell-for-cell on the pinned
  corpus.
- **R2.3 rider discharge** (mine to verify at this design review, per
  the ratified plan): the per-minor hot path is UNTOUCHED — no new
  copy, no new branch, no new indirection inside the QP engine's
  working-set iterations; the binding's per-major transfers are
  enumerated (three contiguous segment copies) on objects the driver
  materializes today, behind the bridge's declared intermediate. The
  wall-leg trigger rule is honored if any engine-linked shared TU is
  touched; my own engine's bench arms cover the SQP side.

## 8. Settler questions (blocking) and boundary-review invitations

1. **§4's three-shape amendment** — settled-text change, needs your
   ruling; drafting lane at the boundary.
2. **§5's posture** (recorded columns stay; scorer is an independent
   leg; Task 9 decides any switch) — confirm this reading of "W1–W5
   re-pointed" against the ratified text, since it is the one reading
   that preserves the census this task is sworn to.
3. §6's deferral is custody-mine per the ratified option wording; on
   the record here, no ruling needed unless someone reads it as an
   obligation.

SIGNOFF: M4-TASK5-DESIGN-SQP (settler + boundary review invited;
implementation of §4-dependent moments waits on question 1)
