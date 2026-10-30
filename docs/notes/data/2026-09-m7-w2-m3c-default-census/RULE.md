# M7 W2 M3c — RULE.md: the DEFAULT-PATH failure census, pre-registered

**Committed BEFORE any leg of this task ran, the identity legs included.**
Nothing below is written after a number was seen. Where a quantity is not
reachable from this task's instrument it is declared ABSENT here, in advance,
rather than substituted for.

---

## §0 Standing: what this task may and may not do

This is a **MEASUREMENT** task at the SQP lane's `default` options.

* **No file under `src/` or `include/` is changed.** No default is changed. No
  remedy is selected, designed or implemented.
* **No outcome of this rule licenses a behaviour change.** Every "engine change
  ⇒ cells it plausibly converts" grouping the deliverable carries is a
  RECOMMENDATION TO THE SETTLER and binds nothing.
* **The taxonomy's outputs are RULE OUTPUTS, not causes.** M3b's Corrections C9
  and C10 are this task's standing warning: a label produced by a predicate
  says which predicate fired, never why the solve died. Wherever this rule's
  class names sound causal (`QP_CAP_BUDGET`, `RESTORATION_EXCURSION`), the
  census must print the underlying fields beside the label and must mark
  OBSERVED and INFERRED separately.
* **Wall-clock is informational only** (CLAUDE.md §7). No conclusion here rests
  on a timing. The two budgets in §3 are wall-clock *stops*, not measurements:
  they decide whether a row exists, and they are reported as such.
* **HOLDOUT is never run.** Every cell below is CUTEst **TRAIN**.
* The SNOPT **source firewall is absolute**. The SNOPT lane's committed RESULT
  ROWS in the harness are read (they are data in a committed CSV); nothing else
  about SNOPT is read, and no SNOPT source, header or decompilation is touched.
* `/home/ghecht/Projects/hven-bench` is **READ-ONLY** and its CMake is never
  entered. The artifacts of M3a and M3b are **never edited**; what this task
  needs from them is COPIED into this directory.

---

## §1 The cells

**The cell list is every CUTEst TRAIN cell whose `hven-sqp @aeebdccc` row is not
`optimal`**, taken from the four-lane table
`/home/ghecht/Projects/hven-bench/runs/2026-09-m7-w2-h-repin/four-lane-table.md`
and from the machine-readable rows behind it
(`run-A/results.csv`, `run-A/cutest/counters-hven-sqp.csv`, same directory).
The set is **20 of the 23** TRAIN cells; the three excluded are the lane's three
`optimal` cells `READING6`, `SMBANK`, `ZAMB2-8`.

The 20, in the run order of §3:

**Group E — the eleven `error` cells** (harness status `numerical_error`):

`ACOPP118`, `ACOPP57`, `ACOPR57`, `BRITGAS`, `DALLASM`, `NET3`, `STEENBRB`,
`STEENBRC`, `STEENBRD`, `STEENBRE`, `STEENBRG`

**Group I — the one `infeasible` cell:** `YORKNET`

**Group W — the eight `dnf_wall` cells** (the harness's OWN wall stop, not an
hven status; the committed row carries `status = null` and no counters):

`ACOPR118`, `BIGBANK`, `MSS2`, `MSS3`, `READING8`, `SAWPATH`, `TAX213322`,
`TAX53322`

Group I is run with group E and not with group W because its harness row is a
**finished** solve (a terminal `infeasible` at 1 major), so it is identity-
comparable and it is cheap; `dnf_wall` is the only group whose harness row has
nothing to compare against.

### 1.1 The pin, and the gap between the pin and HEAD

The harness pin is **`aeebdccc`**; this task runs at hven HEAD **`b2bbaed8`**,
which is **81 commits later**. The four intervening pieces of work that touch
the SQP lane at all are M2 (the S5 restoration record — counters only), M2b (the
restoration proximity term, landed at a declared OFF), M3a (the
`restoration_sub_status` counter field and its `RestorationSubStatus` enum) and
M3b (a `HVEN_TESTING`-gated walk seam, absent from the production library).
**None of them is intended to change default behaviour**, which is what §4's
identity gate exists to check rather than to assume.

### 1.2 The reference values, and where they come from

Every number this task compares against is READ BY A SCRIPT from the committed
harness CSVs at run time. The values below are quoted **for the reader only**;
no gate consumes this transcription (M3b's gate lesson: compare against the
bytes of the file, never against a rule's copy of them).

| cell | harness status | majors | minors | factorizations | objective |
|---|---|---:|---:|---:|---|
| ACOPP118 | numerical_error | 7 | 19917 | 20501 | 129648.67425337316 |
| ACOPP57 | numerical_error | 7 | 7248 | 128 | 41737.543107411046 |
| ACOPR57 | numerical_error | 15 | 13874 | 4848 | 41737.784920103695 |
| BRITGAS | numerical_error | 4 | 92309 | 93749 | 0.075494336118951622 |
| DALLASM | numerical_error | 5 | 10583 | 4919 | 61974881.482906975 |
| NET3 | numerical_error | 0 | 0 | 0 | nan |
| STEENBRB | numerical_error | 5 | 20637 | 25973 | 4090860686442.3218 |
| STEENBRC | numerical_error | 6 | 35324 | 42946 | 183453796271334.12 |
| STEENBRD | numerical_error | 0 | 0 | 0 | nan |
| STEENBRE | numerical_error | 0 | 0 | 0 | nan |
| STEENBRG | numerical_error | 6 | 35429 | 43096 | 183453796271333.81 |
| YORKNET | infeasible | 1 | 50241 | 52180 | 110670.29950761795 |
| the 8 group-W cells | `null` (dnf_wall) | — | — | — | — |

**A correction to the brief's own reading, recorded here because the census's
"how close was it" column depends on it.** The four-lane table's fifth column is
headed `facts / minors` and carries **factorizations** for the two hven lanes
and **minors** (suffixed ` m`) for the SNOPT lane. So `ACOPP57`'s `128` is 128
FACTORIZATIONS against SNOPT's 128 MINORS — not, as the brief's "7 majors and
128 minors" has it, a minor count. `ACOPP57`'s minors at the pin are **7 248**.
The same correction applies wherever that column is read for an hven lane.

### 1.3 The CONSENSUS objective, defined before it is used

The census reports how close a failing cell died to the answer. "The answer" is
defined here, in advance, and is **ABSENT** where the definition does not
produce one:

Let the three reference lanes be `hven-ipm @aeebdccc` (counted only when its
outcome is `optimal` or `acceptable`), `ipopt` (only `Solve_Succeeded` or
`Solved_To_Acceptable_Level`) and `snopt` (only `Optimal`), read from the
committed four-lane rows. **A consensus exists iff at least two counted lanes
agree to 6 significant digits, or at least two counted lanes both report
`|f| <= 1e-6`** (in which case the consensus is `0`; this arm exists for the
feasibility-type cells whose optimal objective is zero, e.g. `BRITGAS`). The
consensus value is that agreed value. Otherwise the cell's consensus is
**ABSENT** and its closeness column reads `ABSENT (no consensus)` — never a
distance to an arbitrarily chosen lane.

---

## §2 The instrument

### 2.1 What is copied, and from where

* **`wrappers/m3c_probe.cpp`** — M3a's `wrappers/m3a_probe.cpp` (public
  `SqpResult` surface + public `Ledger`), copied at HEAD `b2bbaed8` and extended.
  **Every extension is a new ARGUMENT or a new PRINTED KEY**; not one line of
  the solve path differs from M3a's, which is what makes §4's identity check an
  identity check. The extensions, named here so a reader can diff the two files
  and find nothing else:
  1. `--ladder on|off` (default `on`). At `off` the per-iteration callback does
     **not copy the iterate** and the post-solve model evaluations of the ladder
     block are not performed. The callback is still attached and still carries
     §3's deadline, so the SOLVE is unchanged; what changes is the probe's own
     post-processing. Every census leg runs `--ladder off` — the ladder's
     columns are M2's instrument and this task reads none of them, and on a
     300 s `dnf_wall` cell the per-event model evaluations would cost more than
     the solve.
  2. `--history-tail K` (default 12) and the **`HISTORY_BEGIN … HISTORY_END`
     block**: the last `K` rows of `SqpResult::history`, ALL 29 public
     `SqpIterate` fields per row, in declaration order, with the row's absolute
     index. This is §5's and §6's evidence.
  3. The **`COUNTERS_BEGIN … COUNTERS_END` block**: ALL 49 public `SqpCounters`
     fields, in declaration order. Not a chosen subset — a subset is what makes
     a later question unanswerable.
  4. The **`OPTIONS_BEGIN … OPTIONS_END` block**: the values of the option
     fields §5's predicates read (`tr_init`, `tr_min`, `tr_max`, `feas_tol`,
     `kkt_tol`, `max_iter`, `qp_failure_budget`, `qp_cap_retry`,
     `qp.max_iter`, `restoration_proximity`, `qp_mode`, `project_start_point`),
     read off the `SqpOptions` object this process actually constructed. **A
     predicate that reads a threshold reads it from the leg's own row**, never
     from this rule's copy of a default.
  5. `restoration_sub_status` prints through `to_string(RestorationSubStatus)`
     at HEAD, so a solve that never restored prints `not_run` (M3a fix1 R1) —
     where M3a's own committed rows print `optimal`. The M3a rows are NOT
     re-derived; this is a difference between two instruments' libraries and is
     recorded, not repaired.
* **`wrappers/probe-CMakeLists.txt`** — M3a's, verbatim but for the target name
  and the three sources' project name. It builds AGAINST AN INSTALLED hven and
  AROUND the harness: the harness's own CMake is never entered (its
  `find_package(hven)` guard refuses a prefix outside the pin, and this probe
  must link HEAD), the three bridge sources are compiled READ-ONLY out of the
  harness tree, and `EIGEN_MAX_ALIGN_BYTES=32` is set directory-wide (M7 H3
  fix1's alignment pin). **Nothing built here carries `HVEN_BENCH_PIN_SHA`**, so
  no row this probe prints is ever cited as a harness row.
* Pass 2 only (a SECOND chain, after the settler's wake): M3b fix1's
  `wrappers/fix1_probe.cpp` + `probe-fix1-CMakeLists.txt` — the `HVEN_TESTING`
  recompile of hven's sources, at HEAD, which is the **fix2 seam** (single-read
  inertia). Its classifiers `classify_walk_m3b.py` and `classify_cause_fix1.py`
  are run for their **DESCRIPTIVE read-outs only**: per M3b's Corrections C6,
  C9, C10 and the withdrawn `1e-6` active-row bar, `P_imm`'s null is INVALID,
  `RULE-G`'s residual arms are confounded on a post-drop pass and withdrawn in
  their derivation, and `RIDE_VETOED` / `NUMERICAL_DIRECTION` are labels rather
  than causes. **No verdict of this task rests on either classifier's output.**

### 2.2 The observer solve-index ↔ ledger-record mapping (pass 2)

The `Ledger` receives one `SolveRecord` per solve of the driver's **MAIN WALK
ENGINE** only: the restoration sub-driver and the four lazily-built engines
(ipqp, elastic-ipqp, ssn, cap-retry) are never handed it, so `records().size()`
is a **LOWER BOUND** on the QP solves the whole solve performed. M3b's walk
observer, by contrast, counts **every** `QpEngine::run` on the thread. The two
indices are therefore NOT the same index.

**The mapping is established empirically per cell before any pass-2 trace is
read**, by arming the observer's `capture_solve = k` over `k` and matching each
capture's terminal `QpCounters` against the ledger records, and the established
mapping is printed into the artifact. A QP failing inside the RESTORATION
sub-driver is invisible to the ledger, and per the brief it is exactly what this
census must not miss: whenever the restoration record reports a sub-solve that
ended `max_iter`, `infeasible` or `numerical_error`, the restoration phase's own
solves are traced too, and the trace's provenance (main walk vs sub-driver) is
stated on every traced solve.

### 2.3 ABSENT quantities, declared in advance

Not reachable from this task's pass-1 instrument, and therefore never inferred
from a neighbouring field:

* **The trust radius AT EXIT as a separate quantity.** `SqpResult` carries no
  terminal radius. What exists is `history.back().tr_radius`: the radius the
  LAST MAJOR WAS SOLVED AT, recorded before that major's own shrink. §6's
  `TR_AT_FLOOR` is stated over that quantity and over nothing else.
* **Which engine answered a given major** beyond `elastic_applied` and the
  ledger's own record labels.
* **`QpCounters::schur_updates` and the whole degeneracy set for solves the
  ledger does not see** (restoration sub-driver, lazy engines).
* **Per-minor anything** in pass 1. That is pass 2's instrument.
* **The reason a QP failure was judged unretryable.** `qp_failure_is_retryable`
  is not counted anywhere; §5's `QP_UNRETRYABLE` class is therefore stated as an
  INFERENCE from the budget arithmetic and is labelled as one.
* **Anything about Apple / Accelerate.** This is a Linux/MKL box: every
  Accelerate-valued quantity is `UNOBSERVED` (CLAUDE.md §6).

---

## §3 The legs

**One leg per cell, DEFAULT options throughout.** The lane's `default` label is
hven's own defaults plus `common.threads = 1` and `common.print_level = 10`
(`lanes/hven/hven_bench_cell.cpp::sqp_options`), which is exactly what the probe
constructs. `--ledger on` is passed on every census leg: the `Ledger` is a
PUBLIC READ-ONLY instrument, and §4's identity gate against the harness's
committed counters is simultaneously the check that attaching it moved nothing.

Terms, per leg: **one solver process at a time**, serial, `MKL_NUM_THREADS=1`,
`OMP_NUM_THREADS=1`, pinned to ONE physical core with `taskset -c 2`, and the
box lock — `flock /tmp/box-build.lock`, **no timeout, no fallback** — held per
leg and RELEASED between legs. A `pgrep` for a foreign cell binary runs as its
OWN command before the lock and again INSIDE the lock, printing `PGREP_EMPTY`
when it finds nothing. `set -o pipefail` in every script.

### 3.1 The two budgets are different things

* **The SOLVER deadline**, enforced inside the probe by the iteration callback:
  once an `IterationEvent` reports `elapsed_seconds` past it, the callback
  returns `kStop` and the solve exits `interrupted` where it stands. A leg it
  stops is **TRUNCATED**: its counters are reported, its terminal status is NOT
  ranked, and its chain-of-death fields are reported only as far as observed.
  `deadline_fired` is printed on every leg, so a truncated row can never be read
  as a finished one. The deadline fires only BETWEEN iterations.
  * **Group E and group I: 600 s.** The point of these legs is to reproduce a
    FINISHED harness row, and the pin's own slowest error cell (`BRITGAS`) spent
    275 s of a 300 s ceiling; a 300 s deadline would truncate it and destroy the
    identity check this task depends on. 600 s is twice that ceiling, declared
    here, and it is a STOP, not a measurement.
  * **Group W: 300 s**, as the brief directs — the same ceiling the harness
    applied when it recorded `dnf_wall`.
* **The OUTER stop**, `timeout 900` around every leg, for a solve that spends
  its whole budget inside one subproblem and emits no event the deadline can
  fire on. A leg it kills (rc 124) is **CENSORED**: **NO ROW, ever, not filled
  later, not reconstructed.** A leg exiting non-zero for any other reason is
  CENSORED the same way.

### 3.2 Order, and the one structural abort

Group E first (cheapest, and the group whose harness rows are richest), then
group I, then group W. The whole of pass 1 is ONE DETACHED CHAIN (`setsid`),
lock per leg, with its pid recorded in `.superpowers/m7-w2-m3c-STATE.md`.

**A per-cell identity difference is a FINDING, not an abort** (the brief's
ruling): the chain records it, continues, and the census then describes HEAD
with the difference named. **But an instrument that is wrong everywhere is not a
finding**, so one structural bar is pre-registered: if **every** identity-
comparable cell of group E and group I differs on the five core fields of §4.2,
the chain ABORTS before group W and no `dnf_wall` leg runs. The bar is "all of
them", because a genuine behaviour move between `aeebdccc` and HEAD would have
to be extraordinary to touch all twelve, while a mis-built or mis-linked binary
touches all twelve by construction.

---

## §4 The identity gate

### 4.1 What is compared against what

`wrappers/identity_m3c.py` compares **this task's leg row** for a cell against
**the harness's committed rows for the same cell at the pin**, read from the
bytes of
`runs/2026-09-m7-w2-h-repin/run-A/results.csv` and
`runs/2026-09-m7-w2-h-repin/run-A/cutest/counters-hven-sqp.csv`.
It is MACHINE-WRITTEN: no number it prints is transcribed by hand.

**Group W is NOT identity-comparable and the gate says so rather than passing.**
Those cells' committed rows carry `status = null` and no counters: there is
nothing to reproduce. Their gate result is `NOT COMPARABLE (dnf_wall: the pin's
row carries no counters)` — an explicit third value, never `PASS`.

### 4.2 The fields, and the tolerance class

**The five CORE fields the brief names** (a difference in any of them is what
the structural bar of §3.2 counts):

| probe key | harness column | class |
|---|---|---|
| `status` | `results.csv:status` | byte equality |
| `major_iters` | `counters:major_iters` | byte equality |
| `qp_minor_iters` | `counters:qp_minor_iters` | byte equality |
| `factorizations` | `counters:factorizations` | byte equality |
| `f` | `results.csv:objective` | residual class |

**The extended set**, compared and reported the same way, because they are free
and a difference in them is exactly the kind of drift this gate exists to catch:
`steps_accepted`, `rejected_steps`, `soc_steps`, `soc_applied`,
`elastic_activations`, `restoration_iters`, `eqp_refine_steps`,
`border_refine_steps`, `verdict_refine_steps`, `suspect_escalations`,
`symbolic_analyses`, `full_step_majors`, `watchdog_restores`, `evals_full`,
`evals_values` (byte equality), and `stationarity`, `feasibility_e`,
`feasibility_i`, `complementarity` (residual class).

**The residual class is the ONLY class a tolerance may be applied to**
(CLAUDE.md §7's near-ulp rule): `|a − b| <= 1e-12 * max(1, |a|, |b|)`. Counters,
statuses and integer-valued keys are byte equality, no tolerance, ever.
`nan` vs `nan` is accepted as a BYTE fact (the string matches) and never through
the numeric gate — `NaN == NaN` is false numerically, and three cells of group E
have `nan` objectives at the pin.

### 4.3 The structural refusals (M3a fix1's hardened template)

Before any value is compared, and any one of them is a STOP for that cell:

* **(a) SENTINELS** — the leg file's first non-empty line is `PROBE_BEGIN` and
  its last is `PROBE_END`, exactly one of each. A probe killed mid-write has no
  `PROBE_END`, and that must never read as agreement.
* **(b) THE PROBE'S KEY SET** is EXACTLY the pinned set the probe prints —
  cardinality AND names. A stripped or truncated row cannot pass by agreeing
  with itself.
* **(c) THE HARNESS CSV's HEADER** is EXACTLY the pinned 30-column
  `counters-hven-sqp.csv` header and the pinned 27-column `results.csv` header,
  in order; and the cell's row EXISTS in both. A missing row is a REFUSAL, never
  a skip.
* **(d) THE COMPARED-FIELD SET IS NON-EMPTY AND TOTAL** — every field named in
  §4.2 is present on both sides. A gate that compares zero fields reports
  `REFUSED`, never `PASS`.
* **(e) THE BLOCKS EXIST** — `COUNTERS_BEGIN/END`, `HISTORY_BEGIN/END`,
  `OPTIONS_BEGIN/END` and `LEDGER_BEGIN/END` each appear exactly once in the leg
  file, and the counters block carries all 49 field names.

Every refusal and every fold of the checker is exercised on constructed inputs
before the checker reads a leg (`wrappers/fold_proofs_m3c.sh`), and the fold
transcript is committed. A checker that has never been seen to fail is not a
checker.

### 4.4 What an identity difference means

A cell whose leg differs is reported in the census with: the differing fields,
both values, and **which commit in `aeebdccc..b2bbaed8` moved it** — established
by reading that commit's own diff, not guessed from its subject line. The
census then **describes HEAD**, because HEAD is what the engine work will be
done against. The pin's row is not re-derived and the harness is not re-run:
that would be a pin mutation and it is not this task's to make.

---

## §5 The TERMINATION TAXONOMY (axis 1)

### 5.0 Why there are two axes, and what that fixes

The brief's example list mixes two different questions — *what ended the solve*
(`QP_CAP_BUDGET`, `WALL`) and *what the solve had been doing before it ended*
(`RESTORATION_EXCURSION`, `TR_COLLAPSE`, `REJECTION_STALL`). Forcing both into
one mutually-exclusive list would make a cell like `STEENBRB` — which restores
to feasibility, lands at an objective twelve orders above the answer, and THEN
dies on subproblem failures — carry one label and lose the other, and the choice
between them would be a causal judgement dressed as a classification. So:

* **AXIS 1, the TERMINATOR** (this section): mutually exclusive, total, with a
  stated precedence. It answers "which exit path did the driver take", and
  every predicate is over fields §2.1's probe prints.
* **AXIS 2, the ANTECEDENTS** (§6): independent flags, each with its own
  predicate, each reported with the numbers that produced it.

The census's "chain of death in one line" is built from BOTH axes and is
labelled OBSERVED where it repeats fields and INFERRED where it reads code.

### 5.1 The fields the predicates read

All from the leg's own row: `status`; `deadline_fired`; `major_iters`;
`history_rows`; the LAST history row's `qp_solved`, `qp_status`,
`elastic_applied`, `verdict`, `tr_radius`; the counters
`qp_maxiter_exits`, `qp_numerical_exits`, `qp_failures_routed`,
`qp_failures_in_a_row_peak`, `qp_cap_retries`, `restoration_entries`,
`restoration_sub_status`, `restoration_h_in/h_out/f_in/f_out`,
`steps_accepted`, `rejected_steps`, `elastic_activations`; and the OPTIONS block's
`qp_failure_budget`, `tr_min`, `feas_tol`.

Two semantics that the predicates depend on, both read off the code and stated
here so no reader has to rediscover them:

* **`rejected_steps` counts two different events and exactly two.** There are
  precisely two increment sites in `src/drivers/sqp_solver.cpp`: the ROUTED QP
  FAILURE (which also increments `qp_failures_routed`) and the JUDGED REJECTION
  (which does not). Therefore
  **`judged_rejections := rejected_steps − qp_failures_routed`** is exact, not
  an estimate, and §6 uses it rather than `rejected_steps`.
* **The last history row's `qp_status` is the status of the solve that ANSWERED
  that major**, and on an `elastic_applied` row it is the ELASTIC solve's — the
  row's whole `qp_*` group is overwritten. It is therefore read beside
  `elastic_applied` and never without it (M3a's own rule for the first QP,
  applied to the last).

### 5.2 The classes, in PRECEDENCE order

The first class whose predicate holds is the cell's class. The precedence is
fixed here and is not re-ordered after a leg is read.

**1. `CENSORED` — not a class, an absence.** The outer `timeout 900` killed the
leg, or it exited non-zero. **NO ROW.** The census lists the cell as CENSORED
with no fields. It is first in precedence only to say that it never competes.

**2. `WALL_TRUNCATED`** — `deadline_fired == 1`.
The solve did not choose its exit; §3.1's deadline did. Reported with: the
status at the stop (`interrupted`), the counters as they stood, and the LAST
COMPLETED MAJOR's QP cost (`history.back().qp_minor_iters`,
`qp_factorizations`), which is the quantity that says whether the cell is
iteration-starved or cost-bound. It takes precedence over every solver-chosen
class because none of those exits happened.
*Expected to be the whole of group W and none of groups E and I; if a group-E
cell lands here, its identity gate has already failed and the census says so.*

**3. `START_POINT_NONFINITE`** — `status == numerical_error` **and**
`major_iters == 0` **and** `factorizations == 0` **and** `f` is `nan`.
INFERRED, and labelled as inferred: the driver has exactly one exit that
produces a terminal `numerical_error` before any major completes — the
non-finite-iterate exit, which fires when the model cannot be evaluated at `x0`
(or returns NaN there), clears the multipliers and returns. The predicate is
stated over the four OBSERVED quantities; the identification of the exit path is
the code reading.
*Expected: `NET3`, `STEENBRD`, `STEENBRE` — the three cells whose pin rows carry
0 majors, 0 factorizations and a `nan` objective.*

**4. `QP_CAP_BUDGET`** — `status == numerical_error` **and** the last history row
has `qp_solved == 1` and `qp_status == max_iter` **and**
`qp_failures_in_a_row_peak == qp_failure_budget`.
The subproblem ran out of MINORS, not arithmetic, and the driver's consecutive-
failure budget refused a further shrink-and-retry at that iterate. `== budget`
rather than `>= budget` because the counter is a max over admitted runs and the
gate admits a failure only while `in_a_row < budget`, so the peak cannot exceed
the budget: the equality IS the budget having been reached, and a value above it
would be a counter defect and is reported as one.

**5. `QP_NUMERICAL`** — `status == numerical_error` **and** the last history row
has `qp_solved == 1` and `qp_status == numerical_error`.
The answering subproblem's arithmetic failed. Reported with `qp_numerical_exits`
and with `qp_failures_in_a_row_peak`, which says whether the budget was also
exhausted (the two are not exclusive in mechanism; the precedence above gives
the label to the MAX-ITER arm only when the last QP was itself a cap exit).

**6. `QP_UNRETRYABLE`** — `status == numerical_error` **and** the last history
row has `qp_solved == 1` and `qp_status ∈ {max_iter, numerical_error}` **and**
`qp_failures_in_a_row_peak < qp_failure_budget`.
INFERRED, and labelled as inferred: the exit gate is
`in_a_row < budget && qp_failure_is_retryable(...)`, so an exit taken while the
budget still had room can only have been the retryability test refusing — the
failed solve handed back an iterate that was the wrong size, non-finite, or
outside the subproblem's own bounds. **The test is not counted anywhere** (§2.3),
so this class names an inference and never claims an observation.

**7. `RESTORATION_EXIT`** — `restoration_entries >= 1` **and** the terminal
status is one the restoration exit produces and the last history row is not a
QP-failure row (i.e. classes 4–6 did not fire). Sub-labelled by
`restoration_sub_status`, which is the sub-solve's OWN terminal status:
`RESTORATION_EXIT(infeasible)`, `(max_iter)`, `(numerical_error)`,
`(interrupted)`, `(optimal)`.
*Expected: `YORKNET`, whose pin row is a terminal `infeasible` at 1 major with a
sub-solve that M3a recorded returning `infeasible` without moving the point.*

**8. `MAJOR_BUDGET`** — `status == max_iter` **and**
`major_iters == OPTIONS:max_iter`. The solve ran out of MAJORS. No cell of the
list is expected here (no group-E row is `dnf_budget` at the pin); the class
exists because a total taxonomy needs it and because HEAD is not the pin.

**9. `OTHER`** — anything else, reported with every field of §5.1 printed and
with an explicit statement that the taxonomy did not cover it. **`OTHER` is a
finding about this rule**, not about the cell, and the census says so.

### 5.3 What the classes do NOT say

None of 4, 5, 6 says why the subproblem failed. `QP_CAP_BUDGET` says a walk
exhausted its minor cap three times in a row at one iterate; whether that walk
was cycling, saddle-stuck, ill-conditioned or simply large is **pass 2's
question and M3b's warning**: the two cells M3b traced at an unbounded radius
landed in DIFFERENT causes with the same outward counters.

---

## §6 The ANTECEDENT flags (axis 2)

Independent, non-exclusive, each reported with the numbers behind it. A flag is
`true`, `false`, or `N/A` where its inputs are absent (e.g. every restoration
flag on a cell with `restoration_entries == 0`).

**F1 `RESTORATION_EXCURSION`** — `restoration_entries >= 1` **and**
`restoration_sub_status == optimal` **and** `restoration_h_out <= feas_tol`
**and** `restoration_f_out − restoration_f_in >= max(1, |restoration_f_in|)`.
*Derivation of the f-bar.* At `restoration_proximity == 0` (the shipped default,
and the OPTIONS block proves it per leg) the feasibility sub-solve's model is
the violation alone: the parent's objective is not in it. The NULL for "the
excursion did not move the objective" is therefore `f_out == f_in`, and any
positive gap is an excursion in kind. The bar demands the gap be at least the
scale of the objective it started from — a doubling in relative terms — so that
rounding-scale and unit-scale drift cannot trip it. **The ratio
`(f_out − f_in) / max(1, |f_in|)` is REPORTED on every restored cell whether or
not the flag fires**, because the flag is a summary of that number and the
number is the evidence.

**F2 `RESTORATION_STUCK`** — `restoration_entries >= 1` **and**
`restoration_sub_status ∈ {infeasible, max_iter, numerical_error, interrupted}`
**and** `restoration_h_out >= 0.99 * restoration_h_in`.
*Derivation of the h-bar.* The NULL is EXACT equality: a sub-solve that returns
the point it was handed gives `h_out == h_in` bit for bit, which is what M3a
observed on `YORKNET`. The 0.99 band is the tolerance around that null for a
sub-solve that moved the violation by less than one per cent — below any
reduction the phase's own success test (`h <= feas_tol`) could be approaching.
The pair `(h_in, h_out)` is reported whether or not the flag fires.

**F3 `TR_AT_FLOOR`** — the LAST history row's `tr_radius < 2 * tr_min`.
*Derivation, and it invents nothing.* The driver's own floor test is
`shrink_hits_floor(delta) ⟺ delta * kTrShrinkFactor < tr_min` with
`kTrShrinkFactor = 0.5` (`detail/globalization/sqp/trust_region.h`), i.e.
`delta < 2 * tr_min`. The flag is that predicate, evaluated on the radius the
last major was solved at, with `tr_min` read from the leg's own OPTIONS block.
A cell with this flag was ONE shrink away from converting its next rejection
into a restoration request. The radius itself is reported always.

**F4 `REJECTION_STALL`** — `steps_accepted == 0` **and**
`judged_rejections >= 3` (§5.1's exact difference).
*Derivation of the 3.* It is the driver's own consecutive-failure budget, the
only "how many times may the same iterate be re-posed" constant the engine
carries, used here for the judged-rejection side of the same question rather
than a number chosen for this rule. `judged_rejections` and `steps_accepted` are
reported always.

**F5 `CAP_CHURN`** — `qp_maxiter_exits >= 1`. Reported with
`qp_maxiter_exits`, `qp_cap_retries` (identically 0 at the shipped
`qp_cap_retry = false`) and `qp_minor_iters`.

**F6 `ELASTIC_ENGAGED`** — `elastic_activations >= 1`, reported with
`elastic_escalations`, `elastic_rho0_ceiling_hits` and `elastic_floor_retries`.

**F7 `WARM_FAILURES`** — from the LEDGER's per-record table: whether the failing
records (`status ∈ {max_iter, numerical_error}`) were WARM or COLD. Reported as
the pair (warm-failing count, cold-failing count) over all ledger records, with
the index of the first and last failing record. This is the brief's "which
failing solves were warm vs cold" and it is a read-out, never a verdict; its
scope caveat is §2.2's (main walk engine only).

---

## §7 The descriptive read-outs (pass 1), pre-registered

Reported per cell; none is a verdict.

1. **Closeness — objective.** `f` at death against §1.3's consensus, as the
   absolute gap and as `|f − f*| / max(1, |f*|)`; `ABSENT (no consensus)` where
   §1.3 gives none; `n/a (f is nan)` on the three non-finite-start cells.
2. **Closeness — feasibility.** `max(feasibility_e, feasibility_i)` at death
   against the leg's own `feas_tol`, as the ratio to it.
3. **Cost against SNOPT, where SNOPT solved the cell.** hven's `major_iters` and
   `qp_minor_iters` against SNOPT's iterations and minors from the committed
   four-lane row. hven's factorizations have NO SNOPT counterpart and the column
   says so rather than comparing them to minors (§1.2's correction).
4. **The per-ledger-record `QpStatus` sequence**, with each record's
   `minor_iters`, `degenerate_steps`, `drop_ties`, `ratio_ties`, `ws_adds`,
   `ws_drops`, `factorizations` and warm flag — the whole table, with the
   print-cap state printed beside it.
5. **The history tail**, as §2.1's block prints it.
6. **The identity verdict** for the cell (PASS / DIFFERS+fields / NOT COMPARABLE).

---

## §8 Pass 2 — which solves are traced, decided in advance

Pass 2 runs the M3b walk seam at HEAD, at **DEFAULT options** (this is the whole
point of M3c: M3b's deepest result is at `tr_init = +inf`, a configuration hven
does not ship).

**Selection rule.** For each cell of groups E and I whose pass-1 row shows at
least one QP solve ending `max_iter` or `numerical_error`: trace the **FIRST**
and the **LAST** such solve, and **at most those two per cell**. Where the
restoration record reports a sub-solve that ended `max_iter`, `infeasible` or
`numerical_error`, additionally trace the restoration sub-driver's own first
failing solve, with its provenance stated (§2.2). Group W cells are traced only
if a pass-1 row for them exists and shows such a solve before the deadline; a
CENSORED cell is never traced.

**What is reported per trace** — read-outs, not verdicts: the M3b tail class
(`classify_walk_m3b.py`), the cause-field read-outs
(`classify_cause_fix1.py`), the inertia excess at minor 0, the ride outcomes,
and the candidate-set recurrence. Every one carries the withdrawals of M3b's
Corrections C6/C9/C10 beside it.

**If pass 2 needs an observer field that does not exist, this task STOPS and
reports** (the brief's instruction). No hven source change is expected and none
is made.

---

## §9 The pre-registration discipline this rule holds itself to

Three lessons, all of them errors made in this week's own rounds, written here
as constraints on everything above:

1. **Every bar is derived from the fields' semantics**, with its null and its
   pure-mechanism value written down (§6 F1–F4). **No bar assumes a random or
   independent pivot** — M3b's `P_imm` derivation did, and it was invalid
   (Corrections C6).
2. **Formal outputs and causal claims are kept apart** (§0, §5.0, §5.3).
3. **Any gate or shape declaration is derived by diffing THIS task's own
   `base..head`**, never inherited from a previous round's measured numbers, and
   **every path handed to a sibling script is checked to exist** before it is
   used (M3b Corrections C8).

## §10 What no outcome here licenses

No class, flag, read-out or grouping in this task licenses a behaviour change, a
default change or a line of code. The deliverable's "engine change ⇒ cells it
plausibly converts" grouping is a RECOMMENDATION to the settler, with OBSERVED
and INFERRED marked separately, and each candidate is a separate task with its
own pre-registered rule and its own gates. Algorithmic references are
open-literature **papers only**; GPL/LGPL-licensed solvers' source is never read
(CLAUDE.md §6).
