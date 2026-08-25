# M4 Task 5 — the model-surface scorer leg (W-gate leg): harvest report

**Run:** the Task 5 model-surface scorer **demonstration** (settler-mandated),
`--score-model-surface` ON over the 57-cell walk corpus.
**NOT a census of record** — the milestone's close census remains the Task 9
instrument. The census columns ride along and are **co-asserted** against the
frozen U0 baseline as supporting evidence at zero marginal cost.

**Outcome in one line:** the run was **owner-terminated during tier 3**
(10 baseline-DNF cells, UNOBSERVED); the co-assertion over the **47 completed
cells is 0 mismatches on all 13 asserted columns** (and, as unrequired context,
byte-identical on all 36 non-`wall_s` columns); the pre-registered W-leg
criterion — **zero verdict flips across all KKT-gated cells — PASSES**, 0 flips
on 43/43 gated cells, maximum relative residual disagreement **1.0e-19**.

Provenance, including the owner's termination record and the executable
identity of the run as launched, is in `provenance_header.txt` (also embedded,
comment-escaped, at the head of `walk_census_task5wgate.csv`).

---

## 1. Integrity

| check | result |
|---|---|
| completed row files (`rows/`) | **47** |
| scorer sidecars (`scorer_rows/`) | **47** |
| rows ↔ sidecars pair exactly by cell id | **YES** — set difference empty in both directions; every sidecar's own `cell_id` field equals its filename stem |
| `failed/` | **empty** |
| `.part` files left in the artifact set | **none** |
| row schema | 37 columns, **identical across all 47 rows and identical to the U0 baseline header** |
| each row file | exactly one data line, `cell_id` = filename stem |
| `census.log` — tier 1 | 44 cells, started 07:57:23Z, **complete 08:33:37Z** |
| `census.log` — tier 2 | 3 cells, started 08:33:37Z, **complete 11:15:46Z** |
| `census.log` — tier 3 | 10 cells, started 11:15:46Z, **no completion line** (terminated) |
| `census.log` error lines | **none.** The only lines matching `error` are four `NumericalError` entries in the printed *baseline* tier table — a baseline status value, not a run diagnostic |
| `DONE` | **absent** — the runner was killed before its post-pass, which is exactly what the termination record says |

`locks/` holds 52 empty claim directories and `logs/` 52 files: the 47 completed
cells plus the 5 tier-3 cells that were in flight when the process group was
signalled. Claims are per-invocation only (the runner clears them at start), so
they carry no evidence and are not committed.

### 1.1 The 10 absent cells, and the termination rationale re-verified independently

Derived by set difference (baseline roster minus completed rows), then read
against `bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv`'s `status`
column — **not** taken from the census log's tier table:

| absent cell | baseline status |
|---|---|
| `f7_n5000_path_corrupted` | `dnf_budget` |
| `f7_n10000_path_activity` | `dnf_budget` |
| `f7_n10000_path_corrupted` | `dnf_setup` |
| `f7_n10000_path_neutral` | `dnf_budget` |
| `f7_n10000_path_warm` | `dnf_setup` |
| `f7_n20000_path_activity` | `dnf_budget` |
| `f7_n20000_path_corrupted` | `dnf_setup` |
| `f7_n20000_path_neutral` | `dnf_budget` |
| `f7_n20000_path_physics` | `dnf_budget` |
| `f7_n20000_path_warm` | `dnf_setup` |

**All 10 are DNF-class in the baseline.** Under W1 a DNF row is never KKT-gated,
so the demonstration's pre-registered surface — every KKT-gated cell — was
already whole when the owner stopped the run. The independent confirmation of
that claim is the count: the baseline carries **43** `Optimal` cells across all
57, and **all 43 are among the 47 completed**. The 10 absent cells are recorded
**UNOBSERVED — absent, not flipped**, and nothing in this artifact asserts
anything about them.

---

## 2. Shim provenance (adjudicated requirement)

The run was launched through a shim, because the standing runner builds its
worker command lines itself and has no pass-through for extra flags, and
`--score-model-surface-out` is a per-**process** path that six concurrent
tier-1 workers would clobber. The shim is therefore part of the invocation and
its identity is now recorded beside the standing runner's, in
`provenance_header.txt`:

```
# census.runner_path:     scripts/run_walk_census.sh
# census.runner_sha256:   dba90583ff963ddda24d21a97f31a86b865de7448b4a2b922214b2211f0ed4ce
# census.runner_state:    UNMODIFIED (the standing runner of record)
# census.shim_path:       docs/notes/data/2026-08-21-m4-task5-wgate-leg/corpus_with_scorer.sh
# census.shim_sha256:     4aab361e91b4538d0b54e1a56d9d20f36ee1881978ef7a59e4a24be0a41bfbea
# census.wrapper_path:    docs/notes/data/2026-08-21-m4-task5-wgate-leg/run_wgate_leg.sh
# census.wrapper_sha256:  3b528058d1e1edf3389e7c1fb5a8f1ef90ac53704049c03fb1bf9558efa6c0c6
# census.comparator_path: .superpowers/sdd/2026-08-14-hven-m3-plan-revB/preserved-census/census_compare.py
# census.comparator_sha256: 74e4112db603e5f0ad64b7aa40fad433af4ad7958ec84d1e4defa5f4ca435acd
```

The runner's own sha256 is recorded to make "UNMODIFIED" checkable rather than
asserted. The binary's compiled-in git-describe stamp is `d9cc35e` (clean
configure at that commit); the launch record was committed one commit later at
`8503f39`, a docs-only child of `d9cc35e`. Both name the same run.

---

## 3. Ordering: the scorer runs strictly AFTER the recorded diagnostics

### 3.1 The code-order argument

`bench/corpus_cells.h`, `timed_row` (the single funnel every corpus solve goes
through) executes, in this order:

1. `solve_target()` — the solve, inside the timed window;
2. `row_from_solution(...)` — counters, `status`, `kkt_residual`, `wall_s`;
3. `record_kkt_check(model, sol, kFeasTol, row)` — **this is where every
   RECORDED float is computed**: `kkt_stationarity`, `kkt_primal`,
   `kkt_dual_sign`, `kkt_complementarity`, `dual_scale`, `x_scale`,
   `neg_ineq_duals`, all from `self_check_kkt`;
4. **only then**, and only when the flag is on,
   `record_model_surface_check(model, sol, row)`.

So the recorded values are computed **one call earlier** than the scorer and are
already resident in `row` before `record_model_surface_check` is entered.

`record_model_surface_check` writes **exactly five fields** —
`row.ms_stationarity`, `row.ms_complementarity`, `row.ms_primal`,
`row.ms_dual_scale`, `row.ms_x_scale` — and no others. It cannot reach a
recorded column: those are different members, already written, and never
re-assigned on this path. It scores the **same point** (`sol.x`, `sol.lambda_e`,
`sol.lambda_i`, `sol.z`) through a non-owning `NlpModelAggregate` bridge and
`bench/model_surface_kkt.h`'s engine-independent scorer, so the two blocks are
two independent readings of one point, not two points.

Two further separations reinforce this. (a) The `ms_*` fields are **not** part
of the committed 31/37-column artifact contract: they are never threaded through
`write_outcome`/`read_outcomes_csv`, so the main corpus CSV's schema and values
are the same whether or not the hook ever ran; they cross the fork/exec boundary
in their own small sidecar (`write_model_surface_sidecar` /
`read_model_surface_sidecar`), written by the child **after** `write_outcome`.
(b) When the flag is off the branch is not taken at all — no extra evaluation,
no extra allocation.

`write_model_surface_census` then re-derives **both** verdicts through the same
`kkt_gate_verdict` — once over the row as recorded, once over a copy whose three
W2 residuals and two W3 divisors are swapped for the scorer's readings. The
recorded row is copied, never mutated.

### 3.2 The empirical check

Per-cell logs carry the ordering directly. Every one of the 47 logs prints

```
wrote 1 row(s) to .../rows/<cell>.csv.part
wrote the model-surface census to .../scorer_rows/<cell>.wgate.csv
```

in that order — **47/47, zero anomalies** (line-number comparison, not eyeball).

### 3.3 The strongest evidence: the recorded floats are byte-identical to a pre-scorer baseline

The U0 baseline (2026-08-16) was produced by a binary that **had no scorer at
all**. Across the 47 co-asserted cells, **all seven recorded KKT columns**
(`kkt_verdict`, `kkt_stationarity`, `kkt_primal`, `kkt_dual_sign`,
`kkt_complementarity`, `dual_scale`, `x_scale`) — indeed all 36 non-`wall_s`
columns — are **byte-identical** to that baseline (§4.2). Turning the scorer on
moved no recorded value anywhere. This is measurement, not inference from code
order.

---

## 4. The baseline co-assertion (47 of 57)

### 4.1 How the compare was produced

The runner was killed **before** its own merge/compare post-pass, so both steps
were done by hand at harvest time. The corpus binary was **not** re-invoked (the
build token is held elsewhere), so the merge is **textual**:
`walk_census_task5wgate.csv` = comment-escaped provenance header, then the
shared 37-column header once, then the 47 completed rows in `cell_id` order.
**No value is recomputed by the merge.** This differs from the runner's own
merge only in that the runner would have re-emitted the rows through
`--from-csv --score-gates`; the asserted columns are carried verbatim either
way, and `score_gates.txt` is consequently **absent** from this artifact.

Comparator: the preserved `census_compare.py`, run twice.

**Run A — against the full 57-cell baseline of record** (`compare.txt`):

```
MISSING CELL f7_n5000_path_corrupted
MISSING CELL f7_n10000_path_neutral
MISSING CELL f7_n10000_path_corrupted
MISSING CELL f7_n10000_path_activity
MISSING CELL f7_n10000_path_warm
MISSING CELL f7_n20000_path_neutral
MISSING CELL f7_n20000_path_physics
MISSING CELL f7_n20000_path_corrupted
MISSING CELL f7_n20000_path_activity
MISSING CELL f7_n20000_path_warm
57 baseline cells, 47 fresh cells, 0 mismatches
```
exit **1**.

**Run B — against the 47-cell restriction of the same baseline**
(`baseline_47cell_restriction.csv`, a pure row filter of the baseline of record;
no value altered), in `compare_47.txt`:

```
47 baseline cells, 47 fresh cells, 0 mismatches
```
exit **0**.

**Reading both, per the runner's own documented classification.** Run A's exit 1
is driven entirely by the ten `MISSING CELL` lines — a **roster** finding, which
the runner classifies as "evidence incomplete", and which here is not a defect
but the owner's deliberate truncation, already on the record. Its value verdict
is the same as Run B's: **`0 mismatches`**. Run B removes the roster question and
isolates the value co-assertion, which is the claim this artifact makes.

Note on the runner's exit-code rule, since it is easy to misquote: it is the
**runner** (`scripts/run_walk_census.sh`) that turns a value disagreement into
exit 0 plus a `WARN` and a `serial_confirm_list.txt`; the **comparator itself**
exits 1 on any disagreement or roster problem. Nothing here needed that rule —
there is no disagreement to serial-confirm, and `serial_confirm_list.txt` is
consequently absent (the runner never reached the step that writes it).

### 4.2 Per-column outcome, all 47 co-asserted cells

The 13 asserted columns, each verified independently of the comparator:

| # | column | outcome |
|---|---|---|
| 1 | `cell_id` | IDENTICAL |
| 2 | `family` | IDENTICAL |
| 3 | `n_nodes` | IDENTICAL |
| 4 | `window` | IDENTICAL |
| 5 | `taxonomy` | IDENTICAL |
| 6 | `degenerate` | IDENTICAL |
| 7 | `status` | IDENTICAL |
| 8 | `factorizations` | IDENTICAL |
| 9 | `qp_minors` | IDENTICAL |
| 10 | `escapes` | IDENTICAL |
| 11 | `qp_subproblems` | IDENTICAL |
| 12 | `qp_fact_per_qp` | IDENTICAL |
| 13 | `kkt_residual` | IDENTICAL |

**No counter column differs on any completed cell. There is no trigger-clause
event to report.**

Beyond the asserted set, and unrequired: columns 15–37 (`kkt_verdict` …
`esc_gate_refused`, i.e. the recorded KKT block, the SSN counters and the escape
census) are **also byte-identical** on all 47 cells. `wall_s` (column 14) is
excluded from the assertion by contract and is not a timing measurement here;
for context only, its median relative delta against the baseline is 0.080, max
0.129 (`f7_n2000_bound_neutral`) — expected under tiered co-running.

### 4.3 The 10 unobserved cells

Recorded **UNOBSERVED** — absent from the artifact, not flipped, not inferred,
not carried over from the baseline. Any statement about their counters must come
from the Task 9 close census.

---

## 5. The W-leg verdict

**Pre-registered criterion** (`docs/notes/2026-08-21-m4-task5-design.md` §5,
settler rider, declared 2026-08-21 **before** the leg ran): the leg **PASSES iff
zero cells flip any W1–W3 verdict** between the recorded residuals and the
scorer's — on every KKT-gated cell, `kkt_gate_verdict` computed over the
scorer's three residuals (same thresholds, same scales) must equal the verdict
computed over the recorded ones. One flip anywhere fails the leg. The maximum
relative disagreement is **reported as data, never converted into the acceptance
test**.

### 5.1 Which cells are gated

Derived from the rows' own `status` column, not assumed: W1 gates every row that
claims `kOptimal`. Over the 47 completed cells the fresh statuses are **43
`Optimal` + 4 `NumericalError`**, and the rows' own `kkt_verdict` column reads
**43 `ok` + 4 `unchecked`** — the two derivations name the **same 43 cells**.
Cross-check against the baseline: the U0 baseline carries 43 `Optimal` cells
across all 57, and **all 43 are among the 47 completed**. So **all 43 KKT-gated
cells are present**; the demonstration's pre-registered surface is whole
notwithstanding the truncation.

The four `NumericalError` cells (`f7_n2000_path_corrupted`,
`f7_n2000_path_neutral`, `f7_n5000_path_neutral`, `f7_n5000_path_warm`) produced
an answer, so they carry scorer rows; both their verdicts are `unchecked` by W1
and their `verdict_equal` is trivially 1. They are reported, not counted toward
the criterion.

### 5.2 Verdict

**Verdict flips: 0 — across all 43 KKT-gated cells (and, incidentally, across
all 47 scored cells).** `verdict_equal == 1` on every row of `wgate_scorer.csv`.

> ### W-LEG: **PASS**

### 5.3 Maximum relative disagreement, reported as data

`|scorer − recorded| / max(1, recorded)`, computed from the sidecar's printed
values (`%.9e`, i.e. 10 significant digits — the disagreement floor this
artifact can resolve is therefore ~1e-10 relative on a unit-scale residual, and
the figures below are at or below that floor):

| residual | max over the 43 gated cells | at cell | recorded | scorer |
|---|---|---|---|---|
| stationarity | **9.999998e-20** | `f7_n1000_path_physics` | 4.067571900e-10 | 4.067571899e-10 |
| complementarity | **0.000000e+00** | (all cells) | — | — |
| primal | **0.000000e+00** | (all cells) | — | — |

The same three maxima hold over all 47 scored cells (the four ungated cells add
no disagreement). **46 of 47 cells agree on all three residuals to every printed
digit**; the single cell with any printed difference is
`f7_n1000_path_physics`, and only in stationarity, at a last-printed-digit
difference of 1e-19 relative.

Against the gate's own threshold (`kKktGateRel = 1e-6`, and on this corpus both
W3 divisors read exactly 1.0, so the rule reduces to an absolute 1e-6), the
observed disagreement is **13 orders of magnitude below the decision boundary**.
The design note's expectation — "at the e-16 scale: the scorer's stationarity
sum and the driver's disagree in float order" — is confirmed in direction and
is, on this corpus, an order of magnitude tighter than anticipated. This is
Task 9's evidence for any decision about switching the recorded columns; it is
**not** part of this leg's acceptance test.

### 5.4 Per-cell table

`rel stat` / `rel comp` / `rel prim` are `|scorer − recorded| / max(1, recorded)`.

| cell_id | status | gated | rel stat | rel comp | rel prim | verdict_equal |
|---|---|---|---|---|---|---|
| `f7_n10000_bound_activity` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n10000_bound_corrupted` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n10000_bound_neutral` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n10000_bound_physics` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n10000_bound_warm` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n10000_path_physics` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_bound_activity` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_bound_corrupted` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_bound_neutral` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_bound_physics` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_bound_warm` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_path_activity` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_path_corrupted` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_path_neutral` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_path_physics` | Optimal | yes | **1.000e-19** | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n1000_path_warm` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n20000_bound_activity` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n20000_bound_corrupted` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n20000_bound_neutral` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n20000_bound_physics` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n20000_bound_warm` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_bound_activity` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_bound_corrupted` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_bound_neutral` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_bound_physics` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_bound_warm` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_path_activity` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_path_corrupted` | NumericalError | no | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_path_neutral` | NumericalError | no | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_path_physics` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n2000_path_warm` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n5000_bound_activity` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n5000_bound_corrupted` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n5000_bound_neutral` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n5000_bound_physics` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n5000_bound_warm` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n5000_path_activity` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n5000_path_neutral` | NumericalError | no | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n5000_path_physics` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n5000_path_warm` | NumericalError | no | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n750_path_neutral_control` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n800_path_activity` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n800_path_corrupted` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n800_path_neutral` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n800_path_physics` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n800_path_warm` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |
| `f7_n825_path_neutral_control` | Optimal | yes | 0.000e+00 | 0.000e+00 | 0.000e+00 | 1 |

*(10 further cells — the tier-3 DNF class — are UNOBSERVED and have no row.)*

---

## 6. HONEST SCOPE STATEMENT

**What the scorer delivers is not all of W1–W5, and this leg does not claim it
is.**

`bench/model_surface_kkt.h`'s `ModelSurfaceKktResiduals` carries exactly five
numbers: `stationarity_`, `complementarity_`, `primal_`, `dual_scale_`,
`x_scale_`. That is

* **W2's three gated residuals** — stationarity, complementarity, primal
  feasibility — computed at the model surface from
  `evaluate_candidate_first_order` output and `declaration()` bounds alone, with
  no engine state and no knowledge of a provider's treatment; and
* **W3's two divisors** — `max(1, ‖λ‖∞, ‖z‖∞)` and `max(1, ‖x‖∞)`.

It deliberately **omits `dual_sign`**. That omission mirrors the gate's own W2
split (dual sign is measured on every row and is *not* in the gate, pending a
threshold ruling Grant has not been asked for), and it means the scorer cannot
by itself reproduce the counterfactual `dual_sign_would_fail`.

Everything else in W1–W5 lives in the **gate**, in `bench/corpus_cells.h`, and
is not re-implemented by the scorer:

* **W1** — which rows are gated (`kkt_check_applies`: claimed `kOptimal` **and**
  carries a recorded check).
* **W3's threshold and comparison** — `kKktGateRel = 1e-6` and the four
  inequalities, in `kkt_gate_verdict`.
* **W4** — the calibration procedure (the rule fixed against the walk arm before
  any kSsn row was read). Procedural; nothing in a residual scorer can carry it.
* **W5** — a wrong-answer row charged exactly as a DNF, via
  `charged_as_worst_case` in `evaluate_gates`.

The leg's mechanism is precisely this joint reading: `write_model_surface_census`
feeds the scorer's five numbers into the **gate's own** `kkt_gate_verdict` and
compares the verdict against the same gate applied to the recorded numbers.

> **Therefore: the acceptance criterion "W1–W5 scoreable at the model surface"
> is met by the scorer and the gate JOINTLY, not by the scorer alone.** What
> this leg proves is that the *measurement* half of the gate (W2's residuals and
> W3's divisors) can be re-derived engine-independently at the model surface and
> reaches the same verdict on every gated cell it was demonstrated on. The
> row-selection, threshold, calibration and charging halves (W1, W3's constant,
> W4, W5) remain gate-side, unchanged and unduplicated — and `dual_sign` is not
> on the model-surface side at all.

Per §5 of the design note, the **recorded columns keep their provenance** — the
driver's own diagnostics, bit-identical floats — and the census of record
stands. Whether the recorded columns ever switch to the model-surface scorer is
a declared re-derivation decision for **Task 9**, deliberately outside this
task's preservation envelope. §5.3 above is the evidence for it, not a
recommendation.

---

## 7. Cited, not run: the suite's interior solve-trace assertions

Named so the close package can point inside the 1430-passing suite. **These were
not run by this harvest** — the build token is held elsewhere.

**The suite-side pin of interior-engine trace behaviour** is the golden rig's
interior-point trace set:

* **File:** `tests/golden_rig/traces_interior_point.cpp`
* **Tests:** `Arms/InteriorPointTrace.P1_IterateLoopLifecycle/<arm>`,
  `…P2_InertiaCorrectionLadderReplay/<arm>`,
  `…P3_SingularVerdictAndControl/<arm>`,
  `…P4_PerturbationEvidencePresenceIsBackendHonest/<arm>`,
  `…P5_InertiaBeforeFactorizationIsAnExplicitState/<arm>`,
  `…P6_RefinementStepEvidence/<arm>` — a `TEST_P` suite instantiated as
  `INSTANTIATE_TEST_SUITE_P(Arms, InteriorPointTrace, …)` over every configured
  seam arm.
* **Executable / registration:** `hven_golden_rig`, registered into ctest by
  `gtest_discover_tests` (`tests/golden_rig/CMakeLists.txt`); the traces are
  asserted against the pinned tables `tests/golden_rig/expected/P1.csv` …
  `P6.csv`.
* The SQP half is `tests/golden_rig/traces_sqp.cpp`
  (`INSTANTIATE_TEST_SUITE_P(Arms, SqpTrace, …)`, expected tables `T1`–`T8`).

**Correction on the wording of the request, stated plainly.** There is **no md5
assertion anywhere in `tests/`** — a case-insensitive grep for `md5` over
`tests/` returns nothing, and the golden rig pins traces by **value-for-value
comparison against the expected tables**, not by digest. The **md5 solve-trace
chain is evidence-side, not suite-side**: it is
`docs/notes/data/2026-08-21-m4-task3-legs/trace_leg.sh` (full deterministic IPM
print for HS071 + Rosenbrock, wall-clock lines dropped and ANSI stripped,
base-vs-head compared by `md5sum`), with its base capture at
`docs/notes/data/2026-08-21-m4-task3-legs/trace_base.txt`, and the chain of
recorded md5s is carried in `docs/notes/2026-08-m4-ledger.md` (see the entries
around the consumption-switch completion and the two/five-cell trace probes).
The close package should cite the golden-rig P-tests as the **suite** assertion
and the Task-3 trace leg as the **md5 chain**; conflating them would put a claim
in the suite's mouth that the suite does not make.

---

## 8. Artifact inventory

| file | what it is |
|---|---|
| `provenance_header.txt` | launch record + **owner termination record** + **harvest record with runner/shim/wrapper/comparator sha256** |
| `rows/` (47) | per-cell corpus rows, verbatim as written by the run |
| `scorer_rows/` (47) | per-cell model-surface scorer sidecars, verbatim |
| `walk_census_task5wgate.csv` | the 47 rows merged textually under the provenance header |
| `wgate_scorer.csv` | the 47 sidecars merged, header once |
| `baseline_47cell_restriction.csv` | the U0 baseline of record restricted to the 47 completed cells (row filter only, no value altered) |
| `compare.txt` | comparator, full 57-cell baseline vs 47 fresh — 10 `MISSING CELL`, **0 mismatches**, exit 1 |
| `compare_47.txt` | comparator, 47-cell restriction vs 47 fresh — **0 mismatches**, exit 0 |
| `census.log`, `run.log`, `logs/` | the run's own logs, including the per-cell ordering evidence of §3.2 |
| `corpus_with_scorer.sh`, `run_wgate_leg.sh` | the shim and the leg wrapper, as launched |
| `wgate_leg_report.md` | this report |

Absent by cause, all explained above: `DONE`, `score_gates.txt`,
`serial_confirm_list.txt` (runner killed before its post-pass); `failed/` empty
(no cell failed); `locks/` empty directories (per-invocation claims, no
evidence).
