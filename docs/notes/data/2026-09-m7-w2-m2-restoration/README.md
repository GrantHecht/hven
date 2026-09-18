# M7 W2 M2 — the restoration excursion ladder, the S5 record, and the read's control

This artifact is the evidence behind
`docs/notes/2026-09-m7-w2-m2-restoration-design.md`. It answers one question with
numbers: **what does the SQP restoration phase actually do to the caller's
objective and to the caller's violation, iteration by iteration, on the cells the
A-SQP read named** — and it carries the control that shows the read's own
STEENBRC ladder reproduced at the read's own pin.

Nothing here changes behaviour. The only code this task landed is the S5 counter
record (`9a08a715`), which writes five fields and moves no trajectory; the
replay's 27/75/0 on all three engines (`gates/replay/comparison.txt`) is that
statement's proof.

The stamp — toolchain, host, shas, run window, box protocol — is
`PROVENANCE.txt`, and it is the authority on provenance; this file is the
reader's guide to what the numbers mean and what may be said with them.

---

## 1. What was measured, and on which binary

Two pins, deliberately different, and every number below belongs to exactly one
of them.

| pin | engine | instrument | what it produced |
|---|---|---|---|
| **the ladder** | hven `9a08a715` (the M1-shipped default, `qp_failure_budget = 3`, with S5) | `m2_probe`, this task's own instrument | `ladder/`, `ladder-off/`, `tables/`, `neutrality.txt` |
| **the control** | hven `3904a477` (the A-SQP read's pin, `qp_failure_budget = 1`) | the harness's own `hven_bench_cell` at harness sha `348932f` | `control/`, `control.txt` |

sha256, from `PROVENANCE.txt`:

```
libhven.a  (the ladder's, = the gates' own build's)
    d2e41c32170ce293bfebf45e6445e75e60c319881f9ae0535b2296c03b181c8b
m2_probe   3ba8570662506b131981e3a37703c6db5b27f443eb04cf1bba2d5d82841e637f
libhven.a  (the control's, the harness's pinned prefix for 3904a477)
    893b72b18f734cd0f92d4fceeb2def13627dd7e7b885e8db9b52336df3e0574f
hven_bench_cell_read
    71c5e057333c3ae7e99836a62639ae798057f32dd1ac4eff5ff988458a39467e
```

**The library the ladder ran on and the library the gates certified are the same
bytes.** That is stated in `PROVENANCE.txt` and it is what lets a reader treat
`gates/` as covering the ladder's engine rather than a neighbouring build of it.

A second, superseded probe binary appears in the chain transcripts
(`46c9a519…`); its `x_k` rule left `dx_inf` unmeasurable on the two cells that
enter restoration in major 0, every leg was re-taken with the binary above, and
**no row in this artifact comes from it**.

## 2. The instrument, and why it is not the harness's cell binary

**The harness's `hven_bench_cell` cannot produce three of the five columns this
task needs.** A `--budget-iters k` leg reports the main model's `f` and `h` at
each `k` and a hand-enumerated subset of `SqpCounters` (which has no S5 fields);
it attaches no callback, so a capped run's row carries no trust radius and no
`‖x − x_k‖_∞`. The budget ladder is therefore kept as the **control** — the thing
that proves this task reproduces the read — and the ladder proper is taken with
an instrument built for it.

`m2_probe` (`wrappers/m2_probe.cpp`, `wrappers/probe-CMakeLists.txt`) runs one
solve of one cell at the SQP lane's `default` options with a per-iteration
callback attached, and prints a `key=value` block (status, `f`, the four declared
diagnostics, the counters that matter including the five S5 fields, and
`requesting_h`/`requesting_f`) followed by a `LADDER_BEGIN … LADDER_END` CSV block
with one row per iteration event:
`depth,event_iteration,f_event,radius,step_norm,f_main,h_main,dx_inf`.

Four properties of that instrument are load-bearing, and a reader who skips them
will misread the tables.

* **`depth == 1` rows are the restoration sub-solve's own majors.** On those rows
  `f_event` is the **feasibility wrapper's** objective, not the caller's; the
  callback space is the wrapper's augmented `(x, sp, sm, si)` vector
  (`include/hven/drivers/solve_result.h`), and `radius` is the sub-solve's own
  trust radius.
* **`f_main`, `h_main` and `dx_inf` are the PROBE's own recomputation**, from the
  caller's model at `x.head(n)` (`RestorationModel::original_x` is `head(nx_)`),
  not engine output. `dx_inf` is `‖x − x_k‖_∞` against the requesting iterate —
  the last `depth == 0` event before the first `depth == 1` one, or the start
  point when the phase is entered in major 0. Each cell's `x_k_source=` line in
  `ladder/<CELL>.txt` says which. The engine's own numbers in this artifact are
  the S5 record and the counters block, and nothing else.
* **Model evaluations happen after the solve, never inside the callback.** CUTEst
  is process-global and holds exactly one decoded problem, so the callback copies
  each iterate (the four vector views are borrowed and valid for the call only)
  and the `eval_f`/`eval_ce`/`eval_ci` pass runs once the solver has returned.
* **These binaries carry NO harness pin stamp, by construction.**
  `HVEN_BENCH_PIN_SHA` is the harness CMake's compile definition, and the
  harness's CMake is never entered: it refuses a `find_package(hven)` resolving
  outside its pinned prefix, and this probe deliberately links an hven *ahead* of
  that pin (the S5 fields do not exist at it). The probe compiles three harness
  sources read-only — `lanes/cutest/cutest_model.cpp`,
  `lanes/cutest/cutest_source.cpp`, `lanes/hven/nlp_model_triplet_view.cpp`, at
  harness sha `eb26ead38b6877fd65ea502c620063e243d4e5bd` — and links the
  installed hven itself. **Every row in `ladder/` and `ladder-off/` is M2's
  ladder and is never to be cited as a harness row.**

**One stale comment in a committed wrapper, named rather than edited.**
`wrappers/chain2b.sh`'s header says *"The harness is configured READ-ONLY
(`add_subdirectory` from the probe's own CMakeLists)"*. That line survives from
the first attempt, which the settler's ruling replaced; the file that actually
runs — `wrappers/probe-CMakeLists.txt` — compiles three named harness sources and
calls no `add_subdirectory` at all, and `chain2b.sh`'s own body configures only
`$S/probe`. The wrappers are committed verbatim as evidence, so the comment is
corrected here rather than rewritten there. `probe-CMakeLists.txt` and
`PROVENANCE.txt` are authoritative on this point.

`EIGEN_MAX_ALIGN_BYTES=32` is set at directory level in the probe's own CMake, so
the three harness sources get it too: a mixed alignment across TUs sharing Eigen
types is an ABI break, not a warning. `PROVENANCE.txt` carries the exact configure
line for reproduction from this tree.

## 3. The three-part neutrality proof, per cell

The instrument must be proved not to have changed what it measured. A single
comparison against H3's `run-A` would be confounded: `run-A` was taken at hven
`3904a477`, i.e. `qp_failure_budget = 1`, and M1 shipped 3 — which moves exactly
the cells that fail a subproblem. So the proof is in three parts, and each cell
carries the part that is sound for it. Machine-written, full text in
`neutrality.txt`.

| cell | (i) callback ON vs OFF, same binary | (ii) vs H3 `run-A` | (iii) vs M1's committed budget arm |
|---|---|---|---|
| STEENBRB | 33 columns, **0 differing** | — | 16/16 identical, 0 absent |
| STEENBRC | 33 columns, **0 differing** | — | 16/16 identical, 0 absent |
| STEENBRG | 33 columns, **0 differing** | — | 16/16 identical, 0 absent |
| ZAMB2-8 | 33 columns, **0 differing** | 13/13 identical, 0 absent | — |
| YORKNET | 33 columns, **0 differing** | — | 16/16 identical, 0 absent |
| BRITGAS | 33 columns, **0 differing** | — | 16/16 identical, 0 absent |
| READING6 | 33 columns, **0 differing** | — | 16/16 identical, 0 absent |

* **(i)** tests the instrument against *itself* rather than appealing to
  `CallbackAction::kContinue`'s contract: the same binary, the same cell, once
  with the callback attached and once without, every column the probe prints
  compared. `ladder-off/<CELL>.txt` is the other arm and is committed beside the
  first.
* **(ii)** is ZAMB2-8 only, against
  `hven-bench/runs/2026-09-m7-h3-cutest/run-A/` and its
  `counters-hven-sqp.csv`: it is the one cell of the seven that M1's default
  change left bit-identical, so it is the only one for which `run-A` is a valid
  baseline at all.
* **(iii)** is every other cell, against
  `docs/notes/data/2026-09-m7-w2-m1-qp-cap/cells-budget-r1.csv` — the 23 CUTEst
  cells at precisely the shipped default this ladder is built at, already in the
  tree.

## 4. The control at `3904a477`, and the one number that is declared rather than reconciled

`control.txt` compares six budget-ladder legs at the read's own pin against the
read's published STEENBRC table, and then compares the uncapped leg against H3's
`run-A` row at full precision:

* **24 read-table cells compared, 0 differing.** The read printed rounded values,
  so a cell matches when the control's full value rounds to the read's at the
  read's own significant figures; the full value is printed beside it every time.
* **16 full-precision columns against `run-A`, 16 identical** — status, outcome,
  objective, factorizations, iterations and the whole `side.*` block, including
  `side.restoration_iters = 16` and `side.sqp_feasibility =
  9.0949470177292824e-13`.

So the read is reproduced, on the read's engine, with the read's binary pair.

**What is NOT a discrepancy and must be declared rather than reconciled: M1 moved
this cell's cost.** At the read's pin STEENBRC costs **32 137** factorizations
(`control.txt`, `run-A`); at the M1-shipped default the same cell costs **42 946**
(`ladder/STEENBRC.txt`, and M1's own table). Same cell, two engines. Every number
quoted from this artifact says which engine produced it, and the design page does
the same.

## 5. The concurrency declaration

Every leg of this task — every build, every solver run, every gate — ran inside
`flock /tmp/box-build.lock` with **no timeout and no fallback**, with
`FOREGROUND_START`/`FOREGROUND_END` markers printed *inside* the lock and
`set -o pipefail` in every wrapper. Solver legs ran `MKL_NUM_THREADS=1`,
`OMP_NUM_THREADS=1`, `taskset -c 2`. **One cell per process, serial, alone**: a
`pgrep -x hven_bench_cell` guard ran inside the lock before every solver leg and
would have aborted the chain had a foreign cell binary been alive. It never
fired, in any chain.

The harness lane worked the same box throughout — first its A4 sweep, then its
1800 s re-pin tier — taking the lock for a whole cell at a time. **The two lanes
interleaved leg by leg**, which is why the long chains here are segmented into
several lock acquisitions rather than one long hold: a single hour-long hold
would have starved that lane. The segments that produced committed rows, from the
chain transcripts:

| segment | window (informational) | what it produced |
|---|---|---|
| `chain1b` | 10:41:41 → 10:43:59 | the Release build/suite leg of the S5 commit |
| `chain2b` | 11:54:47 → 11:55:43 | install + probe build + the ZAMB2-8 smoke pair |
| `chain3` | 12:50:27 → 13:16:16 | the first ladder run |
| `chain4A` | 13:48:00 → 14:13:53 | the ladder re-take (all fourteen legs, one binary) |
| `chain4bB` | 14:52:17 → 14:52:27 | the control's build segment |
| `chain4bC` | 15:09:07 → 15:15:42 | the six control legs |
| `gates.sh` G1–G6 | 15:45:42 → 18:58:54, six separate acquisitions | `gates/` |

`chain1`, `chain2` and `chain4`'s segments B/C are **superseded attempts** that
produced no committed row (`PROVENANCE.txt` names them); `chain4b` is the control
chain that replaced the last of them.

Because this is a pinned box with another lane live on it, **no wall-clock number
in this artifact is a measurement** (CLAUDE.md §7), including the per-leg walls in
the chain transcripts and the windows above. The 900 s per-leg cap was a safety
stop, not a DNF condition: **no leg hit it**, all fourteen ladder legs and all six
control legs returned rc 0, and nothing here is censored. Had a leg been
censored it would be reported as censored and never as a row — BRITGAS was the
candidate, since at the M1-shipped default it sits near the harness's 300 s cell
ceiling.

## 6. The S5 record, per cell

The engine's own reading at exit, machine-written to `tables/summary.md`:

| cell | entries | h in → out | f in → out | restoration iters | outcome |
|---|---:|---|---|---:|---|
| STEENBRB | 1 | 19 800 → 1.6303669053986523e-12 | 2 858.7861353626763 → 4 090 860 686 442.3218 | 11 | `numerical_error` |
| STEENBRC | 1 | 41 800 → 4.8902304088678286e-12 | 4 528.3061353626763 → 183 453 796 271 334.12 | 16 | `numerical_error` |
| STEENBRG | 1 | 41 800 → 4.8902304088678286e-12 | 4 527.9899075966596 → 183 453 796 271 333.81 | 16 | `numerical_error` |
| ZAMB2-8 | 1 | 24.643618798749973 → 1.209356876794021e-11 | 0.018068535257036302 → −0.069042214516419051 | 11 | `optimal` |
| YORKNET | 1 | 48 491.383947016526 → 48 491.383947016526 | 110 670.29950761795 → 110 670.29950761795 | 18 | `infeasible` |
| BRITGAS | 1 | 115.03999999999996 → 110.10740647336283 | 0 → 0.075494336118951622 | 7 | `numerical_error` |
| READING6 | **0** | — | — | 0 | `optimal` |

Reading rules that go with that table:

* **The absence reading is `restoration_entries == 0`, not a sentinel.** `0.0` is a
  legal observed violation and a legal observed objective, so none of the four
  absolutes carries an absence predicate. READING6's four zeros mean *no phase
  ran*, not *a phase ran and moved nothing* — that second case is YORKNET, where
  the four absolutes are equal and `entries` is 1.
* **`restoration_h_out <= feas_tol` iff that entry resumed.** STEENBRB, STEENBRC,
  STEENBRG and ZAMB2-8 took the RESUME arm; YORKNET and BRITGAS took the
  not-feasible-enough arm. No second field is needed to tell them apart.
* **The entry half is the requesting iterate's, in ENGINE units.** `enable_scaling`
  is `false` by default and off on every cell here, so engine units are caller
  units throughout this artifact and the record is directly comparable with the
  harness's own `f` column.
* **`requesting_h` and `restoration_h_in` differ in the last few digits on some
  cells** (ZAMB2-8: `…749976` against `…749973`). That is one l1 sum taken in two
  different orders by two different programs, not a disagreement, and neither
  value is to be "fixed".
* **Nothing folds from the sub-solve.** Nested restoration is refused, so the
  sub-solve's own `restoration_entries` is structurally 0 and its four absolutes
  are untouched defaults; the driver deliberately does not fold them.

The per-cell ladders are `tables/<CELL>.md`, one row per iteration event, and the
design page argues from them.

## 7. The gates

`gates/gates.txt` is the full transcript index and carries the caveats. In
summary, at `9a08a715`:

| gate | result |
|---|---|
| G1 Debug suite (the > 600 s test excluded) | 2662 / 2662, rc 0 |
| G2 `Continuation.ProbeBudgetBoundsAFailingProposal`, alone | passed, rc 0 |
| G3 Release suite | 2663 / 2663, rc 0 |
| G4 U0 27-cell three-arm replay vs the M6 close pin | **27 / 75 / 0** on all three engines |
| G5 P-SYM, Release and Debug, classified vs `aeebdccc` | exit 1 as expected; **0 UNEXPLAINED in both**, and the two classifications byte-identical |
| G6 install smoke, clang-format, `git diff --check` | rc 0 / 0 replacements / clean |
| G6b export contract (`linux 32`, this task's own install prefix) | **rc 0** — platform macro OK, `EIGEN_MAX_ALIGN_BYTES` exported 32 = computed 32, `INTERFACE_COMPILE_OPTIONS` absent, include dirs exactly once (`gates/export-contract.txt`) |

G6b was recorded separately because the first invocation in `gates.sh` passed the
script no arguments and it refused with its usage message — a defect in the
wrapper, not in the tree. It was re-run alone under the lock against this task's
own install prefix, queued from 19:00 with no timeout and no fallback behind the
other lane's 1800 s cell, and landed at 19:29:01. `gates/gates.txt` carries the
appended CLOSED record — the original table is not rewritten — and
`gates/export-contract.txt` is the verbatim transcript.

## 8. Files, and the citation rules

```
README.md                  this file
PROVENANCE.txt             the §7 stamp: toolchain, host, shas, window, protocol
ladder/<CELL>.txt          the probe, callback ON, verbatim stdout (7 cells)
ladder-off/<CELL>.txt      the same cell, callback OFF — proof (i)'s other arm
tables/<CELL>.md           the cell's restoration ladder, machine-written
tables/summary.md          the S5 record for all seven cells
neutrality.txt             proofs (i), (ii), (iii), per cell per column
control/STEENBRC-b<k>.txt  the six budget-ladder control legs at the read's pin
control.txt                the control vs the read's table, and vs run-A at full precision
gates/                     the gate transcripts, both P-SYM classifications, the replay CSVs
wrappers/                  every source and script that produced the above
```

Four rules, all easy to get wrong:

1. **The probe's rows are never harness rows** — no `HVEN_BENCH_PIN_SHA`, by
   construction. Cite them as M2's ladder.
2. **The ladder's caller-model `f` and `h` columns are the probe's own
   recomputation**, not engine output. The engine's numbers here are the S5 record
   and the counters block.
3. **Say which engine every number came from**: the ladder is the M1-shipped
   default, the control is the read's pin, and STEENBRC's factorization count
   differs between them because M1 moved it.
4. **No wall-clock number here is a measurement.**

---

## 9. How this file itself landed, since a reader will check

This README was written by the round that wrote the design page, after the
measurements were complete. **Its first version is committed inside `7e0c6691`**
— the retiring measurement implementer's commit closing the export-contract gate
— because it was staged in the shared index when that commit was made, so it sits
under a message that does not describe it. Nothing was rewritten to tidy that up.
The later edits are `01d24747`, and `97b02ef9` removes a duplicated G6b record
that the same shared-index confusion produced in `gates/gates.txt`, with its
reason written into that file. The design page is `ebc882d9`.
