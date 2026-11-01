# M7 W2 M3a — the restoration REQUEST probe — THE PRE-REGISTERED DECISION RULE

Committed BEFORE any probe leg ran. Nothing below is revisable by what the legs
say; a rule that turned out to be the wrong question is reported as the wrong
question and re-registered in a named successor round, never edited here.

Task: `.superpowers/m7-w2-m3a-brief.md`. Tree `/home/ghecht/Projects/hven`,
branch `m7`. Task A (the instrument) landed at `8e6aacdf` / `67b81138`; its gates
are in `gates/`. Background: `../2026-09-m7-w2-m2b-proximity/README.md` findings 1–2,
and `docs/notes/2026-09-m7-w2-m2-restoration-design.md`'s "Outcome (M2b)".

---

## 1. The hypothesis, and the one thing this task measures

On STEENBRB and YORKNET the restoration phase is requested in **major 0**: the
first QP returns `QpStatus::kInfeasible` inside the l∞ window
`min(st.delta, opts_.qp.tr_radius)` with `st.delta` starting at
`SqpOptions::tr_init = 1.0` (`src/drivers/sqp_solver.cpp` ~:5940–5947;
`sqp_solver_types.h:388`). `SqpOptions::qp.tr_radius` is `+inf` by default
(`include/hven/qp/qp_types.h:238`), so the window IS `tr_init` on the shipped
path, and a `tr_init` of `+inf` removes it entirely.

This task measures **whether the REQUEST is an artefact of that window**. It is
a measurement, not a change: **no default is changed by this task under any
outcome.** The proximity term stays OFF and EXPERIMENTAL; stage 3 uses it as an
interaction arm only. The settler rules on what, if anything, follows.

The hypothesis is registered as a HYPOTHESIS. M2b's close already says the
initial trust radius is "M3a's hypothesis, not a root cause", and this rule does
not promote it.

---

## 2. What the probe can and cannot see — STATED BEFORE THE LEGS

### 2.1 The first QP's own status is on the public surface, with ONE hole

`SqpResult::history` is a public `std::vector<SqpIterate>`, and
`SqpIterate::qp_status` is a public `QpStatus` (`sqp_solver_types.h:1059`,
meaningful iff `qp_solved`). So **`history[0].qp_status` is reachable** and the
probe reports it.

**The hole, and it is live on both stage-1 cells.** On a major where the elastic
tier supplied the answer, `sqp_solver.cpp` overwrites the row's QP fields with
the ELASTIC solve's (`mj.row.qp_status = report.qp_status`), and
`SqpIterate::elastic_applied`'s own contract says so: "the qp_* fields describe
the final elastic solve, not the kInfeasible one that triggered it". M2b's screen
rows show `elastic_activations = 1` on STEENBRB and `19` on YORKNET, so this is
not a hypothetical.

The probe therefore reports three things rather than one, and a reader never has
to infer which it is holding:

| key | what it is |
|---|---|
| `first_qp_status` | `history[0].qp_status` verbatim, or `n/a` if `!qp_solved` or the history is empty |
| `first_qp_elastic_applied` | `history[0].elastic_applied` |
| `first_qp_plain_status` | the DERIVED answer to "what did the plain QP say", resolved by the rule below |

`first_qp_plain_status` resolves as:

- `elastic_applied == false` → **`first_qp_status`**, which then IS the plain
  solve's own status.
- `elastic_applied == true` AND `elastic_from_ipqp_escape == 0` →
  **`infeasible (inferred)`**. At this lane's `default` options
  `SqpOptions::qp_mode == QpMode::kWalk`, no interior-point tier runs, so no
  `fallback_report` can exist, and the kInfeasible branch is "the ONLY consumer
  of QpStatus::kInfeasible in the driver" (`sqp_solver.cpp`'s own comment at the
  elastic tier). `elastic_applied` then implies the plain QP returned
  kInfeasible. The precondition is CHECKED, not assumed: the probe prints
  `elastic_from_ipqp_escape` and the inference is only drawn when it is 0.
- otherwise → **`unavailable (elastic_applied with a tier escape)`**. The public
  surface does not carry the plain solve's status in that shape, and this rule
  will not invent it.

The word `(inferred)` is part of the printed value, so no table can quote an
inference as a reading.

### 2.2 "Requested in major 0" is read from the S5 record and the ladder

`request0 = yes` iff **both**: `restoration_entries >= 1`, and the probe's
`x_k_source` is `start_point (the phase was entered in major 0)` — the M2b
probe's own rule, unchanged, for "there was no depth-0 event before the first
depth-1 one, so the requesting iterate is the start point".

`request0 = no` iff `restoration_entries == 0` (no phase ran at all), or
`x_k_source` is the `depth0_event` rule (a phase ran, but not from major 0).

Both stage-1 cells have `request0 = yes` at the control by M2b's committed screen
rows, which is the fact under test.

### 2.3 The new field

`SqpCounters::restoration_sub_status` (Task A) is the feasibility sub-solve's own
terminal status at the LAST entry. It has no "never entered" sentinel: the
record's absence rule is `restoration_entries == 0`, and the probe prints the
field only alongside `restoration_entries` so it is never read as a fact about a
phase that did not run. This closes M2b finding 1's inference gap — until M3a,
"restoration returned and the solve then said X" could not be separated from
"restoration itself said X".

### 2.4 Wall-clock

Informational everywhere, never asserted, per CLAUDE.md §7. Counters and statuses
are the currency. The one place wall-clock touches a VERDICT is the 300 s solver
deadline of §6, and §6 says exactly what a leg that hits it may and may not be
read as.

---

## 3. The stages, the cells, and the tr_init values

ζ (`SqpOptions::restoration_proximity`) is **0** — the shipped default — in
stages 1 and 2. Every leg otherwise runs this lane's `default` label:
hven's own defaults plus `common.threads = 1` and `common.print_level = 10`,
exactly as `lanes/hven/hven_bench_cell.cpp` sets them and exactly as M2b's probe
set them.

### Stage 1 (this round)

Cells: **STEENBRB**, **YORKNET**.
`tr_init` ∈ **{1 (control), 10, 1e2, 1e3, 1e4, +inf}**.
12 legs, in this order: the two controls first, then the five raised radii on
STEENBRB, then the five on YORKNET.

The two control legs are the **identity leg** of §4. If either fails §4, the
chain ABORTS at that point and no raised-radius leg is run. A chain that aborts
this way has produced no ladder and reports none.

### Stage 2 (a later round; run only if stage 1 produces a qualifying radius)

The radii that qualify under §5 on stage 1, applied to all seven M2b ladder
cells: **STEENBRB, STEENBRC, STEENBRG, ZAMB2-8, YORKNET, BRITGAS, READING6**,
still at ζ = 0.

Every cell's control in stage 2 is **that cell's own `tr_init = 1`, ζ = 0 leg run
in THIS task's chain**, not a number lifted from M2b — for STEENBRB and YORKNET
those are stage 1's control legs, and the other five are run in stage 2. Each
such control is held to §4 against M2b's committed screen row for that cell. A
cell whose control leg is CENSORED or TRUNCATED (§6) has no control, and is
**UNDETERMINED at every radius**: it yields neither a qualification nor a harm
finding.

**Pre-registered cost risk, named now so it is not a surprise later.** M2b's
screen legs ran 276.0 s on BRITGAS and 155 s on STEENBRC/STEENBRG
(`../2026-09-m7-w2-m2b-proximity/logs/chainB.log`, informational). BRITGAS sits
close to the 300 s solver deadline before any radius is raised, and its M2b
candidate arms ran 370–433 s. BRITGAS is therefore the cell most likely to be
TRUNCATED in stage 2. That is a reported outcome, not a reason to widen the
budget mid-task.

### Stage 3 (a later round; run only if §5's stage-3 trigger fires)

Proximity **member 1**, ζ = h_k/(n·ρ) with ρ = 1e3 and (n, h_k) taken verbatim
from M2's committed ladder exactly as M2b's `summarise_m2b.py` took them, at the
stage-2 radii, on the cells that still request restoration in major 0 there.
Interaction arm only.

---

## 4. The control-identity requirement

**The control leg is the gate on the whole instrument.** `tr_init = 1` IS the
shipped default and ζ = 0 IS the shipped default, so a control leg is M2b's
screen leg re-run on a library that has gained one counters field and a probe
that has gained three printed keys and one argument. It must reproduce M2b's
committed screen row.

Compared: **every key present in BOTH this probe's `PROBE_BEGIN` block and M2b's
committed `rows/screen/<CELL>.txt`** — the shared-key set — and **every ladder
column present in both headers**, row for row, as STRINGS (the probe prints every
float at `%.17g`, so string equality is value equality).

Excluded, **by name and only by name**:

```
tr_init                      this task's new argument, printed as a key
first_qp_solved              this task's new key
first_qp_status              this task's new key
first_qp_elastic_applied     this task's new key
first_qp_plain_status        this task's new key
first_qp_verdict             this task's new key
first_qp_tr_radius           this task's new key
first_qp_tr_binding          this task's new key
first_qp_restoration_seed_used  this task's new key
history_rows                 this task's new key
elastic_from_ipqp_escape     this task's new key (the inference's precondition)
restoration_sub_status       Task A's new field
deadline_seconds             this task's new key
deadline_fired               this task's new key
```

**Time-valued fields are dropped BY NAME.** The list is:

```
wall_seconds        (not printed by either probe, listed so the rule is total)
elapsed             (captured per row by both probes, printed by neither)
elapsed_seconds     (the IterationEvent field behind `elapsed`)
```

No time-valued key is printed by either probe today; the list is stated in full
anyway so that a future key of that kind is excluded by this rule rather than by
a later judgement call.

**The bar is byte equality, and the near-ulp fallback is declared in advance.**
M2b's own off-identity check achieved byte equality on all 37 shared keys and
every ladder column across processes and days
(`../2026-09-m7-w2-m2b-proximity/off-identity.txt`), so byte equality is the
expectation here. CLAUDE.md §7 exempts residual-class columns from cross-process
byte determinism, so:

- **counters, statuses, integer- and enum-valued keys, and `x_k_source`**:
  byte equality, no tolerance, ever.
- **residual-class float keys and columns** — `f`, `stationarity`,
  `feasibility_e`, `feasibility_i`, `complementarity`, `restoration_h_in`,
  `restoration_h_out`, `restoration_f_in`, `restoration_f_out`, `requesting_h`,
  `requesting_f`, and the ladder's `f_event`, `radius`, `step_norm`, `f_main`,
  `h_main`, `dx_inf`, `prox_rel`: byte equality is still the expectation, and a
  byte difference within `|a - b| <= 1e-12 * max(1, |a|, |b|)` is accepted as
  near-ulp cross-process noise. **Every such acceptance is printed, with both
  values and the relative gap**, so a reader sees exactly what was tolerated. A
  difference outside that gate is a FAILURE.

**A control-identity failure ABORTS the chain.** No raised-radius leg runs, the
ladder is not produced, and the failure is reported as the finding. It would mean
the instrument is not measuring what M2b measured, and no radius row taken with
it could be believed.

---

## 5. The decision rule

### 5.1 The status ordering, fixed here

`rank` over `to_string(SolveStatus)`'s spellings, for this task only:

| rank | status | reading |
|---:|---|---|
| 4 | `optimal` | the solver answered the problem |
| 3 | `infeasible` **with** `infeasibility_certified = 1` | a verdict ABOUT THE MODEL |
| 2 | `max_iter`, `budget_exhausted` | a budget outcome; no verdict either way |
| 1 | `infeasible` **with** `infeasibility_certified = 0` | "could not make progress" — a statement about THIS solve |
| 0 | `numerical_error` | the solve broke |
| — | `interrupted` | NOT RANKED; see §6 |

`acceptable`, `stalled` and `diverging` are unreachable on the SQP engine
(`core/solver_status.h`) and are not in the table; a leg reporting one is a STOP
and is reported as such.

Both stage-1 cells sit LOW at the control: STEENBRB is `numerical_error` (rank 0)
and YORKNET is `infeasible, certified=0` (rank 1). There is room above them,
which is why the test below is worth running.

### 5.2 "The request is a window artefact"

For a cell `c` at a raised radius `r`, **`WINDOW_ARTEFACT(c, r)`** iff ALL THREE:

- **(A1)** `request0 = no` at `(c, r)` — the major-0 request vanished, by §2.2;
- **(A2)** `rank(c, r) >= rank(c, control)` — the terminal status improves or
  holds;
- **(A3)** `max(feasibility_e, feasibility_i) <= feas_tol` at the returned point,
  with `feas_tol = 1e-6` (the shipped default, `sqp_solver_types.h:375`) — the
  point it holds at is feasible.

All three, at one radius, on one cell. Anything less is reported as what it is
and is not called a window artefact.

**The task's finding** is `WINDOW_ARTEFACT(c, r)` for each of the 2 cells × 5
raised radii, reported as a table, plus the plain statement of which radii (if
any) satisfy it on BOTH stage-1 cells. Only a radius satisfying it on both goes
to stage 2.

### 5.3 "Harm"

Evaluated at every `(c, r)` against that cell's own control, and reported whether
or not `WINDOW_ARTEFACT` holds — the point of the harm test is the cells the
radius was NOT supposed to help.

**`HARM(c, r)`** iff EITHER:

- **(H1)** status regression: `rank(c, r) < rank(c, control)`; or
- **(H2)** `factorizations(c, r) > 1.10 * factorizations(c, control)`.

`factorizations` is a counter, so H2 is an assertion on the currency, not on
wall-clock. The 1.10 factor is the brief's, fixed here before any leg.

A radius that qualifies under §5.2 on both stage-1 cells but shows `HARM` on
either of them is reported as **qualified-with-harm** and is carried to stage 2
only with that label attached; the settler rules on it, not this rule.

### 5.4 The stage-3 trigger

Stage 3 runs iff, at the stage-2 radii, **at least one of the seven cells still
has `request0 = yes`**. Its arm is §3's member 1. Its own read-out is the same
`WINDOW_ARTEFACT` / `HARM` pair, computed against the SAME cell's ζ = 0 control
at the SAME radius — never against the `tr_init = 1` control, because that would
confound two changes in one comparison.

### 5.5 The failure path

If **no radius qualifies** under §5.2 — on either stage-1 cell, or on both but
never simultaneously — the task **reports that**, with the full 2 × 5 table and
the counters behind it, and **stops**. The hypothesis is then recorded as not
supported at these radii on these cells. It is not re-tested at other radii, not
widened to other cells, and not rescued by relaxing (A1)–(A3): a rule relaxed
after seeing the data is not a rule. The settler rules on what follows.

**No default is changed by this task under any outcome** — not `tr_init`, not
`restoration_proximity`, not `qp.tr_radius`, not `feas_tol`. This task lands
measurements and a report. That sentence is the rule, not a caveat on it.

---

## 6. Budgets, censoring, and truncation

**Per leg: a 300 s SOLVER deadline inside a 900 s OUTER stop.**

- The **solver deadline** is enforced by the probe itself, in the iteration
  callback: once an `IterationEvent` reports `elapsed_seconds > 300`, the
  callback returns `CallbackAction::kStop` and the solve exits `interrupted` at
  the point it was standing on. hven has no wall-clock option, so this is the
  only place it can live. The probe prints `deadline_seconds=300` and
  `deadline_fired=0|1` on every leg.
- The **outer stop** is `timeout 900` around the process. It exists because the
  callback fires only BETWEEN iterations: a solve that spends 900 s inside one
  subproblem emits no event and the deadline can never fire.

**TRUNCATED** — `deadline_fired = 1`. The leg's counters ARE reported (they are
exact, and the leg ran alone under the lock), but the row is marked TRUNCATED and
is **UNDETERMINED** for both §5.2 and §5.3: `interrupted` is unranked, it is
never a status regression, and it is never an improvement. A TRUNCATED leg can
neither qualify a radius nor condemn one.

**CENSORED** — the 900 s outer stop fired (rc 124), or the leg exited non-zero
for any other reason. **No row.** Never filled, never interpolated, never
replaced by a re-run at a different budget, never averaged with a neighbour. The
leg is listed with its cell, its radius and its rc, and that is the whole record.
A CENSORED leg is UNDETERMINED in exactly the sense above.

**A control leg that is TRUNCATED or CENSORED is a missing control**, and §3
already says what that costs: that cell is UNDETERMINED at every radius. For
stage 1 specifically, a truncated or censored control ABORTS the chain, exactly
as a §4 identity failure does. (M2b's screen legs ran 60.0 s on STEENBRB and
78.6 s on YORKNET — informational — so neither control is expected anywhere near
the deadline, and the identity check of §4 is itself the proof that the deadline
mechanism did not perturb the control.)

**Measurement conditions, declared.** One leg per process, serial, ALONE under
`flock /tmp/box-build.lock` with no timeout and no fallback, released per leg;
`MKL_NUM_THREADS=1`, `OMP_NUM_THREADS=1`; pinned to one physical core with
`taskset`; a `pgrep` check for a foreign cell binary as a separate prior command,
with a `PGREP_EMPTY` marker inside the lock. These are the serial terms of
CLAUDE.md §7, not the co-run terms: nothing here co-runs. HOLDOUT is never run.

---

## 7. What gets committed, and in what order

1. This file — **before any leg**. Done at this commit.
2. The probe, its CMake project and its chain wrapper, under `wrappers/`.
3. The raw leg output, one file per leg, under `rows/stage<k>/`, unedited.
4. A machine-written fold: the identity report, the ladder tables, and the
   §5 verdict — **no number in the artifact transcribed by hand** (M2b's
   `summarise_m2b.py` precedent).
5. `.superpowers/m7-w2-m3a-report.md`.

Every artifact carries its provenance: the hven HEAD the probe was built at, the
`sha256` of the installed `libhven.a`, the `sha256` of the probe binary, the
harness tree's sha for the three bridge sources compiled read-only out of it, the
toolchain, and the date. Nothing built here carries `HVEN_BENCH_PIN_SHA` — the
harness CMake is never entered — so **no row from this probe is ever cited as a
harness row**.
