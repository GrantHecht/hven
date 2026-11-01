# RULE — M7 W2 M4b: EXPAND ALONE, ON THE TEN

**This file is committed BEFORE the first leg of this task runs, and is not
edited after that leg has run.** It is a new rule for a new task. M4's
`RULE.md` is not edited by this task and not superseded by it; where this rule
inherits from M4's it says so by section number and the inherited text is read
out of M4's file, never retyped here.

*Predecessor: `docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/` — its
`RULE.md`, its `README.md`, its `verdict.txt`, and the rows, logs and gate
transcripts they cite.*

---

## §0 What this asks, and what it is not

**The question.** Does arm **E** — Gill & Murray's EXPAND, at M4 §2.1's
parameters, behind M4's `HVEN_TESTING`-only switch — change the outcome on the
ten CUTEst cells `hven-sqp` fails at DEFAULT options?

**What it is not.** It is not a proposal to ship anything. M4 §10 governs this
task unchanged and is repeated in §9 below: a SUPPORTED result ranks a
production design for the settler and the owner; it is not a decision, and
nothing under `src/` or `include/` is edited as a consequence of it.

**Why this task exists at all.** M4 ran E, L and L+ against a bit-equality
neutrality bar on three cells `hven-sqp` already solves, and §8 of that rule
stopped all three arms there. Phase 3 never ran, so **E was never tried on the
ten.** That is the gap this task closes, and closing it requires one change to
the bar, declared in §5 below and argued there rather than assumed.

---

## §1 The arms

### 1.1 E ONLY. L and L+ are EXCLUDED BY NAME, and here is the reason

Arm `E` (`--arm E`) is the only arm this task runs. `L` and `--lex-full-pricing
on` (`L+`) are excluded **by name, before anything is run**, on M4's evidence:

* **L and L+ each BREAK a cell `hven-sqp` already solves.** `ZAMB2-8` goes
  `optimal` → `numerical_error` under `L` and `optimal` → `infeasible` under
  `L+`; `READING6` goes `optimal` → `numerical_error` under `L+`. Observed in
  `../2026-09-m7-w2-m4-anticycling-arm/logs/gate-neutral-L.txt` and
  `gate-neutral-L+.txt`, tabulated in that task's `tables/arm.md` §2.
* Those three cells are **degenerate-but-solved** — M4's README records
  `READING6` at 875/3833 degenerate minors and `ZAMB2-8` at 2300/6666, from the
  OFF controls' own per-solve tables — which is exactly the population an
  anti-cycling rule exists to serve. An arm that damages that population is not
  a production candidate, and running it over the ten would be measuring a
  candidate that has already been disqualified.

**Neither is re-tested here, and no result of this task bears on them.** A
future task that wants to revisit either writes its own rule.

### 1.2 E's parameters are M4 §2.1's, unchanged and untuned

`--arm E` with the probe's declared defaults, exactly the values M4's neutrality
legs ran and exactly the values M4 §2.1 derived from the paper. They are
readable in every M4 row as the `arm_*` option keys:

    arm_delta_master     = 1.0000000000000001e-09
    arm_delta_0_factor   = 0.5
    arm_delta_max_factor = 0.98999999999999999
    arm_expand_k         = 10000
    arm_negligible_pivot = 9.9999999999999998e-13
    arm_reset_nontrivial = 3.7000000000000001e-11
    arm_tau              = 4.9000000000000006e-14

**No parameter is tuned against a result in this task.** A leg that suggests a
different value produces a FINDING, not a re-run.

---

## §2 The cells — M4 §1.1's TEN, and nothing else

    ACOPP57  DALLASM  STEENBRB  ACOPP118  ACOPR57
    STEENBRC STEENBRG YORKNET   SAWPATH   BRITGAS

These are M3c's pass-2 cells at DEFAULT options. Every one of them has an OFF
control that **already passed M4's identity gate** (§4 below), so all ten are
carried into the arm.

**NOT run here, and named so the omission is visible:** `ACOPR118`, `BIGBANK`
and the five cells censored at M4 §9's 900 s outer stop. M4 §9's conditional
extensions were and remain conditional; this task does not adopt them. **HOLDOUT
is never run.**

---

## §3 The instrument — THE SAME BINARY, PINNED, NEVER REBUILT

### 3.1 The pin

    /home/ghecht/Projects/hven/.scratch/m7w2m4/arm/build/m4_arm_probe
    sha256 c1be17d2d47e7930440450d72d233da56a8fc374bceb68062baea924fe50a720

**The chain verifies this sha256 before its first leg and REFUSES if it differs.**
It is the same value `../2026-09-m7-w2-m4-anticycling-arm/logs/chain.log`
printed at `CHAIN_START 2026-09-22T07:28:06-04:00`, and the same value
`../2026-09-m7-w2-m4-anticycling-arm/logs/binary-m4.txt` records.

### 3.2 There is NO rebuild and NO fresh smoke, and that is a choice with a reason

M4 built this binary as a REPLAY of M3c's build (M4 §3.2), verified it, smoked
it and gated it. Rebuilding it here could only produce a different binary or the
same one; the first would break the comparison this task depends on, and the
second would prove nothing that is not already proved. So the evidence is
**cited, by path, rather than regenerated**:

| what | where |
|---|---|
| the build replay's residual-free harvest transcript | `../2026-09-m7-w2-m4-anticycling-arm/logs/harvest-diff-m4.txt` |
| the SMOKE — `--arm off`, ACOPP57, under the lock — reproducing M3c's committed pass-1 row on all 58 compared fields with ZERO near-ulp acceptances | `../2026-09-m7-w2-m4-anticycling-arm/logs/gate-smoke.txt`, row in `rows-smoke/SMOKE-off-ACOPP57.txt` |
| the arm switch reachable from outside `src/`/`include/` (`arm_compiled_in=1`) | `../2026-09-m7-w2-m4-anticycling-arm/logs/armcheck.txt` |
| the OFF-control gate, 10 / 10 | `../2026-09-m7-w2-m4-anticycling-arm/logs/gate-off.txt` |
| Task A's gate record, including the proof that the production library is byte-for-byte unmoved by the arm | `../2026-09-m7-w2-m4-anticycling-arm/gates/README.md` |

**If the sha256 does not match at chain start, this task stops and reports it.**
It does not rebuild.

---

## §4 The OFF controls are REUSED, BY COPY, AND NEVER RE-RUN

M4's chain ran all ten OFF controls and gated them against M3c's COMMITTED
pass-1 rows: **REPRODUCES 10 / 10, DIFFERS 0, CENSORED 0, REFUSED 0, MISSING 0,
zero near-ulp acceptances**
(`../2026-09-m7-w2-m4-anticycling-arm/logs/gate-off.txt`).

Re-running them could only (a) reproduce, proving nothing new, or (b) differ,
which would be a finding about the machine and not about EXPAND — and would
leave this task with two OFF rows and no rule for choosing between them. So:

* the ten rows are **COPIED** from
  `../2026-09-m7-w2-m4-anticycling-arm/rows/off-<CELL>.txt` into this task's
  `rows-off/`, byte for byte;
* their **sha256 manifest** is `rows-off/SHA256-off-rows.txt`, written at the
  copy and checked by the chain before its first leg;
* **no OFF leg runs in this task, at all.** A chain that would run one is
  malformed.

Wall-clock from M4's OFF legs is not carried and not compared: it was taken in a
different run on a different day and CLAUDE.md §7 makes it informational in any
case.

---

## §5 The neutrality bar — REPLACED for E, and the replacement is declared HERE

### 5.1 Why M4's bar cannot stand for this arm

M4 §5.5 required an arm to reproduce a cell's own `off` leg **bit for bit** on
three cells `hven-sqp` already solves. E failed it on all three. M4's close-out
established, from the traces, **why**: EXPAND does not only decide ties. It
admits constraints against a slowly growing feasibility bound and takes a
largest-pivot second pass, so it changes which constraint enters even where
nothing is tied. On `SMBANK` — the one neutrality cell that is tie-free on every
printed scalar — E's first differing per-minor record is minor 6 of solve 0, in
`alpha`, `dx_inf` and `raw_ratio`: the ratio test's own output, on a step with
no tie. *(Observed: `../2026-09-m7-w2-m4-anticycling-arm/tables/readouts-m4.md`,
"The walk difference the ROW gate cannot see".)*

A bit-equality bar is therefore **unmeetable by an arm of this shape, for
reasons that have nothing to do with whether the arm is any good.** Keeping it
would stop this task before it started, for the second time, on the same
structural fact.

**It is replaced, not lowered.** What replaces it asserts nothing about
equality and cannot be passed by accident; it is a DESCRIPTION that the report
must carry, whatever it says.

### 5.2 The replacement: a DESCRIPTIVE neutrality report on the same three cells

On `READING6`, `SMBANK` and `ZAMB2-8`, using **M4's already-committed rows** —
`../2026-09-m7-w2-m4-anticycling-arm/rows/{off,E}-<CELL>.txt`, no leg re-run —
the report prints, per cell:

1. **terminal status under OFF and under E**, side by side;
2. **`f` under E against M3c's three-lane consensus** for that cell, by M4 §7.6's
   consensus rule unchanged, machine-read from M3c's committed
   `../2026-09-m7-w2-m3c-default-census/tables/census.md`, never recomputed;
   and `|f_E − f_off| / max(1, |f_off|)` beside it;
3. **`max(feasibility_e, feasibility_i)` against that leg's own printed
   `opt_feas_tol`**;
4. **the minors ratio E / off**, printed to four places.

**These three cells receive no verdict.** The report states what happened on
them and moves on. They are context for reading the ten, not a gate in front of
them.

### 5.3 THE DECLARED CAUTION, written here BEFORE the ten are run

M4 measured E's minors ratio against OFF on those three cells:

| cell | minors off | minors E | ratio E / off |
|---|---|---|---|
| `READING6` | 3833 | 3119 | **0.8137** |
| `SMBANK` | 334 | 328 | **0.9820** |
| `ZAMB2-8` | 6666 | 9947 | **1.4922** |

**E can lengthen a walk as readily as shorten one.** This is recorded now, as a
declared expectation, so that a long leg on one of the ten is read as a known
behaviour of the arm and not as a surprise, and so that a short leg is not read
as the arm's general property either.

M4's post-hoc read-outs locate the +49 % precisely and that reading is carried
with the caution: on `ZAMB2-8`, minors inside RECURRENT solves went 4620 → 7920
(+3300) while minors outside them went 2046 → 2027 (−19), against a total move
of +3281 — **E did not break that cell's recurrence; it made the capped solves
longer.** On `READING6` it removed one of three recurrences and the minor saving
is exactly that one solve. *(Observed:
`../2026-09-m7-w2-m4-anticycling-arm/tables/readouts-m4.md`, POST-HOC and
labelled so there.)*

### 5.4 Identity of the compared rows

Every E leg of this task is compared against **that cell's copied OFF row**
(§4), field by field, and the comparison is printed in full in `tables/arm.md`
whatever the verdict. The near-ulp constant is M4 §5.2's, unchanged:
`|a − b| ≤ 1e-12 · max(1, |a|, |b|)`, with residual class decided by value text
exactly as M4 §5.2 decides it. **This is a DESCRIPTION of the difference, not a
gate**: on the ten, a difference is the point of the experiment.

---

## §6 The per-solve predicates — INHERITED BY REFERENCE, VERBATIM

`RECURRENT_TAIL`, `RECURRENCE_GONE`, the `0.10` coverage bar, the `8p` bar and
the `Σ‖Δx‖∞ > 0` motion conjunct are **M4 `RULE.md` §§6.2–6.5, read out of that
file and applied unchanged.** They are not restated here, so that no drift
between two copies is possible. M4 §6.4's DECLARED DEFECT of the coverage-only
form travels with them and is repeated in this task's README.

M4 §6.6's per-solve read-outs are likewise inherited verbatim, including its
instruction that `arm_lex_drop_switches` across the cells is the direct test of
Task A's vacuity finding — which on this task reads as a **control**: arm E must
leave it at 0 on every leg, since E touches no drop rule. A non-zero value on an
E leg is a FINDING about the instrument and is reported as one.

The machinery is `wrappers/readouts_m4.py` and `wrappers/readouts_driver_m4.py`,
**copied unmodified** from M4's `wrappers/` with their three helper modules. The
driver's own docstring records that it is POST-HOC with respect to M4's three
neutrality cells; on this task's ten cells §6 is **pre-registered**, by this
file, before any leg.

---

## §7 The per-cell verdicts — INHERITED BY REFERENCE

* **The RANK ORDER on terminal status is M4 `RULE.md` §7.2**, read out of that
  file: `optimal` 0, `acceptable` 1, `max_iter` 2, `stalled` 3, `diverging` 4,
  `budget_exhausted` 5, `infeasible` 6, `numerical_error` 7, and `interrupted`
  **NOT RANKED** — a deadline firing is a CENSORING, and such a cell is
  `TRUNCATED` for that arm and receives no verdict. Including M4 §7.2's own note
  that `infeasible` ranks low **on this cell set** and not as a general matter
  of the enum.
* **`CONVERTED` is M4 §7.3**, all three conditions, with `feas_tol` read from the
  leg's own printed `opt_feas_tol` and the consensus machine-read from M3c's
  committed `tables/census.md`.
* **`improved, unverified` is M4 §7.4**, in those exact words, never counted as a
  conversion.
* **`HARM` is M4 §7.5.** Its **second clause is `N/A` on these ten, exactly as in
  M4, because OFF solves none of them** — stated, not printed as a vacuous
  "0 harmed". Its first clause (`rank(arm) > rank(OFF)`) is live and applies.
* **The consensus rule is M4 §7.6**, which is M3c's, unchanged. **Only committed
  RESULT ROWS of the SNOPT lane are read. No SNOPT source, header or decompiled
  material is read by this task under any circumstance.**
* **The SNOPT cost read-out is M4 §7.7**, for CONVERTED cells only, with its
  standing correction that the four-lane table's fifth column is read PER LANE —
  factorizations for the two hven lanes, minors for SNOPT — and that hven's
  factorization count has no SNOPT counterpart.

The fields compared per cell are M4 §7.1's, and every one of them is printed for
every cell in `tables/arm.md` whatever the verdict.

---

## §8 The stopping rules — M4 §8's, MINUS the neutrality stop

| trigger | consequence |
|---|---|
| the pinned binary's sha256 does not match §3.1 | **everything stops** and the two values are printed. Nothing is rebuilt |
| a copied OFF row's sha256 does not match `rows-off/SHA256-off-rows.txt` | **everything stops** and the row is named |
| a leg is killed by `timeout 900`, or exits non-zero for any other reason | that cell is **CENSORED**. Its row is REMOVED. No row, no trace, nothing filled in later |
| the probe's own deadline fires (`deadline_fired=1`) | that cell is **TRUNCATED** (§7). It gets counters but no `CONVERTED`/`HARM` verdict |
| `arm_compiled_in` is not 1 on a leg | **everything stops** and it is reported. Nothing under `src/` or `include/` is edited |

**M4 §8's neutrality-stop row is deliberately absent** — §5 replaced the bar it
enforced, and a stop with no bar behind it would be a stop on nothing. Every
other row of M4 §8 is carried. **No stop is re-run to a better outcome.**

M4 §8's OFF-control row is also absent, and for a different reason: this task
runs no OFF leg (§4). The ten cells' OFF controls passed in M4 and are copied,
not re-gated.

---

## §9 What a result licenses — M4 §10, IN FORCE UNCHANGED

**Nothing ships.** Not a default change, not a production switch, not a line
under `src/` or `include/`, not in this task and not as a consequence of it.

A SUPPORTED result ranks the production design for the settler and the owner and
is an input to a future decision that has its own brief, design, review and
gates. An UNSUPPORTED result is worth the same and is reported with the same
care.

**What this cannot show**, to be stated in the README in these terms:

* that E generalizes beyond TRAIN. **HOLDOUT is never run.**
* that E is cycle-FREE on hven's walk. It is not proven so; known cycling
  examples exist even for EXPAND (Hall & McKinnon), and M4's read-outs already
  show E carrying FOUR recurrent solves on `ZAMB2-8` where OFF had three.
* that §1.2's parameters are the right ones. They are the paper's, taken once.
* that a converted cell converted for the reason the arm claims. §6.6's
  read-outs are evidence toward that and are labelled observed; anything beyond
  them is labelled inferred.
* that E is neutral anywhere. **This task has no neutrality bar**, by §5, and the
  descriptive report of §5.2 asserts no such thing.

---

## §10 The legs, the terms, and the order

### 10.1 The order

1. **The PINS** — the binary's sha256 against §3.1, the ten copied OFF rows
   against their manifest, the cell roster read out of the harness's own source.
   A failure here stops everything before any leg.
2. **ARM E over the ten**, in M4's own cell order:
   `ACOPP57 DALLASM STEENBRB ACOPP118 ACOPR57 STEENBRC STEENBRG YORKNET SAWPATH
   BRITGAS`.
3. Nothing else. There is no phase 3 and no extension.

### 10.2 The terms, IDENTICAL to M4 §4.2

Per leg, in one place so no leg can quietly differ from another:

* DEFAULT options — `--tr-init` is never passed, so the option field is never
  assigned;
* `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`;
* `taskset -c 2`;
* the ledger on, the observer armed over EVERY solve
  (`--capture-solve -1 --max-records 2000000`);
* a **600 s solver deadline, 300 s for `SAWPATH`** — the value M3c ran it at —
  inside an outer `timeout 900`;
* **ONE box-lock hold per leg**, `flock /tmp/box-build.lock` with **no timeout
  and no fallback**, released between legs;
* `pgrep` as its **own command BEFORE the lock**, and **again INSIDE it**, with
  `PGREP_EMPTY` and `FOREGROUND_START`/`FOREGROUND_END` printed inside the lock;
* `set -o pipefail`;
* a leg exiting non-zero is CENSORED and **its row is REMOVED**.

Worst case 10 legs × 900 s ≈ 2.5 h. M4's OFF legs on these same ten cells took
between 2 s (ACOPP57) and 5 m 51 s (SAWPATH), 19 m 53 s in total.

### 10.3 Wall-clock

Informational only, CLAUDE.md §7. No timing in this task is quoted as a
measurement. The deadlines are STOPS, not timings.

### 10.4 Outputs

Under `/home/ghecht/Projects/hven/.scratch/m7w2m4b/`: `rows/E-<CELL>.txt`,
`traces/E-<CELL>.csv`, `logs/chain.log`, `logs/E-<CELL>.err`. The artifact
directory takes the rows, the logs, the tables and trace SLICES with a sha256
manifest; the flat traces are not committed.

---

## §11 The process rules this rule inherits

Verbatim from M4 `RULE.md` §11 and from CLAUDE.md:

* **SNOPT source firewall, ABSOLUTE.** Only the SNOPT lane's committed RESULT
  ROWS are read.
* **Algorithmic references are open-literature PAPERS only.** No GPL/LGPL solver
  source is read or incorporated; `~/Software/piqp-src` is never read.
* **`hven-bench` is READ-ONLY** and its CMake is never entered.
* **`docs/notes/data/**` is FROZEN except this task's own directory.** M3a's,
  M3b's, M3c's and M4's artifacts are never edited; what this task needs from
  them is COPIED here.
* **`notices/` is untouched.** Nothing under `src/`, `include/` or `tests/` is
  touched.
* Box protocol: `pgrep` as a SEPARATE prior command; `flock /tmp/box-build.lock`
  with NO timeout and NO fallback; `PGREP_EMPTY` and
  `FOREGROUND_START`/`END` inside the lock; `set -o pipefail`; ONE solver at a
  time.
* Never signal a process this task did not start; kill only an exact pid it
  started.
* Foreground under 600 s, else DETACHED with the pid in the state note and an
  IMMEDIATE hand-back. **Nothing polls a detached chain.**
* Explicit pathspecs only — never `git add -A` or `git add .`; `git status`
  immediately before every commit; no `--amend`.
* **NO model attribution trailer of any kind on any commit.**
* No push.

---

## §12 The deliverables

* `tables/arm.md` — one row per cell, E against its copied OFF control, on M4
  §7.1's fields, with §6.6's read-outs and the §7 verdict.
* `tables/neutrality-descriptive.md` — §5.2's descriptive report on the three
  cells, from M4's committed rows, with no verdict attached.
* `verdict.txt` — §7's verdicts, applied, per cell.
* `README.md` — leading with the result in PLAIN LANGUAGE, observed vs inferred
  labelled on every claim, §5's replacement of the neutrality bar argued in
  full, §5.3's declared caution held against what actually happened, M4 §6.4's
  inherited defect repeated, and a "what this does not show" section carrying
  §9's items.
* `.superpowers/m7-w2-m4b-report.md`, ending exactly with the line
  `SIGNOFF: M7-W2-M4B-IMPLEMENTER-REPORT`.

**This file is not edited after its first leg runs.**
