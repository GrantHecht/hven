# M7 W2 M3c — RULE-P2.md: PASS 2, the per-minor trace at DEFAULT options

**Committed BEFORE any pass-2 leg ran, the mapping legs and the identity legs
included.** Nothing below is written after a pass-2 number was seen. `RULE.md`
(pass 1) is unchanged and still governs everything it covers; this file governs
pass 2 only, and where the two touch, `RULE.md` §8 is the clause this one
implements.

---

## §0 The question, and what an answer may not be

**THE QUESTION.** Pass 1 found that eight of the twelve identity-passing cells
end with the QP failure budget spent on subproblems that hit their ITERATION
CAP, and that every capped ledger record in the census has the same outward
shape — `degenerate_steps ≈ minors/2`, `ws_adds ≈ ws_drops ≈ minors/2`. M3b
traced a tail at `tr_init = +inf` and found a **zero-motion period-2 drop/re-add
cycle**, in a configuration hven does not ship.

> **ARE THE CAPPED QPs AT DEFAULT OPTIONS THE SAME ZERO-MOTION PERIOD-2 CYCLE,
> OR GENUINE LONG WALKS, OR SOMETHING ELSE?**

**WHAT AN ANSWER MAY NOT BE.**

* **No file under `src/` or `include/` is changed**, no default is changed, no
  remedy is selected. **If pass 2 needs an observer field that does not exist,
  this task STOPS and reports** (`RULE.md` §8). None was added: see §2.
* **M3b's two classifiers are run as DESCRIPTIVE READ-OUTS ONLY, never as
  verdicts,** and their withdrawn arms are excluded before they are read (§6).
* **The counter shape above is a HYPOTHESIS, not a premise.** The read-outs of
  §5 are defined so that "a genuine long walk" and "something else" are as
  reportable as "the cycle", and §5.1 fixes what each would look like BEFORE any
  trace is read. A rule that can only return one answer has not measured
  anything.
* **Wall-clock is informational only** (CLAUDE.md §7). The budgets in §4 are
  STOPS.
* **HOLDOUT is never run.** `/home/ghecht/Projects/hven-bench` is READ-ONLY and
  its CMake is never entered. M3a's and M3b's artifacts are never edited; what
  this task needs from them is COPIED here. The SNOPT source firewall is
  absolute.

---

## §1 The instrument

`wrappers/m3c_p2_probe.cpp` + `wrappers/probe-p2-CMakeLists.txt`, COPIED at HEAD
from M3b fix round 1's `fix1_probe.cpp` and `probe-fix1-CMakeLists.txt` — the
fix2 seam, single-read inertia (M3b C15). The CMake recompiles the `hven`
target's own `SOURCES` (42 files, printed by the configure) a second time with
`HVEN_TESTING` defined target-wide and **does not link `hven::hven`**; that
property is CHECKED by `wrappers/prepare_p2.sh` with M3b fix1's REPAIRED check
(it locates the link description under either generator and REFUSES when it
finds none, so an absent file can never read as an absent link), and the check's
own non-vacuity is exercised on a constructed link line that DOES name
`libhven.a`. The transcript is `logs/prepare-p2.txt`.

**Four differences from M3b's probe, and they are the whole of the adaptation:**

1. `--capture-solve K`, default `0`, `-1` = every `QpEngine::run` on the thread.
2. **Default options are STRUCTURAL**: with `--tr-init` absent the probe does not
   ASSIGN `options.tr_init` at all, and prints `tr_init_touched=0`. Every
   pass-2 leg omits `--tr-init`.
3. `--stop-after-first-qp` defaults to `off`: pass 2 runs each solve to its own
   termination.
4. A `SOLVE_SUMMARY_BEGIN … SOLVE_SUMMARY_END` block (§2).

Two defects found while building it are recorded here rather than left in the
transcript: M3b's `--stop-after-first-qp` parser only ever assigned `false`
(harmless at its own default of `true`, silent breakage at this copy's `false`),
and the probe resolves its CUTEst problem directory RELATIVE to the cwd, so every
leg runs from the bench tree. Both were caught by `prepare_p2.sh`'s refusal
folds before a leg ran.

---

## §2 The observer-solve-index ↔ ledger-record MAPPING, and why no field was added

`Ledger` receives one `SolveRecord` per solve of the driver's **MAIN WALK ENGINE**
only: the restoration sub-driver and the four lazily-built engines (ipqp,
elastic-ipqp, ssn, cap-retry) are never handed it. `QpWalkObserver` counts
**every** `QpEngine::run` on the thread. The two indices are not the same index.

**`QpWalkMinorRecord::solve` already carries each record's own solve index**
(`include/hven/detail/qp/qp_walk_trace.h`), and `pass_factorizations` is a
per-pass count. So ONE run at `--capture-solve -1` recovers, for every solve the
process performed, its minor count and its factorization count — **without any
new observer field.** The probe's `SOLVE_SUMMARY` block is exactly that fold and
nothing else: per solve, `minors`, `minor_first`, `minor_last`,
`minor_contiguous`, summed `factorizations` / `schur_updates` / the three refine
counts, `ws_size_in` at its first record, `ws_size_out` at its last, and the
`degenerate` / `ride` / `post_drop` / drop / add counts.

**THE MAPPING RULE, fixed here.** For a cell's pass-2 leg and its pass-1 ledger
table, observer solve `k` is identified with ledger record `r` iff

  `summary[k].minors == ledger[r].minor_iters`  **and**
  `summary[k].factorizations == ledger[r].factorizations`,

and the identified `k`s are **strictly increasing in `r`** (the main engine's
solves occur in order). The mapping is ACCEPTED only when it is a **unique**
such assignment over the whole table; if more than one assignment satisfies both
conditions, the mapping for that cell is `AMBIGUOUS` and every trace selected
through it is reported as provenance-`UNCERTAIN` rather than as a main-engine
solve. **A mapping is never assumed from an index correspondence.**

**PROVENANCE.** A summary row that no ledger record claims is a solve of the
restoration sub-driver or of a lazy engine, and is labelled
`NOT-LEDGERED (restoration sub-driver or a lazy engine)`. The census's pass-1
rows already say which cells entered restoration and which activated the elastic
tier, so the label is reported with those two counts beside it and is never
sharpened beyond what the numbers support.

**PRE-REGISTERED CAVEAT (M3b C7).** "One record per minor iteration" holds for a
**non-truncated** capture whose record allocations succeeded. The probe prints
`summary_truncated` and `summary_allocation_failed`; a leg with either set is
**ABORTED for that cell** and its traces are not read. `minor_contiguous` is
printed per solve for the same reason, and a `0` there aborts the cell too.

**ABSENT, declared in advance.** The per-solve `warm`, `n`, `me`, `mi`,
`max_iter`, `incoming_ws_signature` and `exit_status` fields are per-CAPTURE and
are overwritten at every solve, so at `--capture-solve -1` only the LAST solve's
values survive. They are therefore **not used by the mapping** and are not
reported per solve. Where a selected solve's own header is wanted, §3's second
stage re-runs that cell at `--capture-solve k` for the selected `k` alone.

---

## §3 Which solves are traced, decided in advance

**STAGE 1 — one leg per cell at `--capture-solve -1`.** Cells: the eight
`QP_CAP_BUDGET` cells of the census (`ACOPP118`, `ACOPP57`, `ACOPR57`, `BRITGAS`,
`DALLASM`, `STEENBRB`, `STEENBRC`, `STEENBRG`), plus `YORKNET`
(`RESTORATION_EXIT(infeasible)`) and `SAWPATH` (`WALL_TRUNCATED`, whose
restoration sub-status is `numerical_error`). **Ten cells.** The three
`START_POINT_NONFINITE` cells build no subproblem and are not traced; the five
CENSORED cells have no pass-1 row and are never traced (`RULE.md` §8); `BIGBANK`
has no failing QP of any kind and `ACOPR118`'s failing solves are reachable only
under the wall.

**STAGE 2 — the selected solves, from stage 1's own summary.** Selected
mechanically, by this rule, with no judgement at selection time:

* For each of the eight cap-budget cells: the **FIRST** and the **LAST**
  main-engine solve whose mapped ledger record has `status ∈ {max_iter,
  numerical_error}`. At most two per cell.
* For each cell whose pass-1 restoration record reports a sub-status that is not
  `optimal` (`BRITGAS`, `YORKNET`, `SAWPATH`): additionally, the **FIRST
  NOT-LEDGERED solve** whose summary row shows `minors == its own max_iter cap`
  — i.e. a capped solve the ledger never saw — with its provenance stated as
  §2 requires. If no such row exists, the cell reports
  `NO REACHABLE SUB-DRIVER CAPPED SOLVE` and nothing is invented.
* `SAWPATH`'s and `YORKNET`'s restoration sub-solve is traced only if it is
  reachable inside §4's deadline; a leg the deadline stops reports what it
  observed and is marked TRUNCATED.

**WHAT IS KEPT.** Stage 1 writes the FULL flat trace of every solve to the
scratch tree; **only the SELECTED solves' per-minor CSVs are committed**, plus
every cell's `SOLVE_SUMMARY`. The trace volume is large — `BRITGAS` alone is
about 92 000 minors — and a committed artifact that no one can read is not
evidence. This is declared here, in advance, and the scratch path of the full
trace is printed in each cell's transcript so a reader knows what was dropped
and where it was.

---

## §4 The legs

DEFAULT options throughout, `--tr-init` never passed. One solver process at a
time, serial, `MKL_NUM_THREADS=1`, `OMP_NUM_THREADS=1`, `taskset -c 2`, the box
lock (`flock /tmp/box-build.lock`, **no timeout, no fallback**) held per leg and
released between legs, a `pgrep` for a foreign cell binary as its own command
before the lock AND inside it, `set -o pipefail` everywhere. The whole of pass 2
is ONE DETACHED CHAIN (`setsid`) with its pid in
`.superpowers/m7-w2-m3c-STATE.md`.

* **The SOLVER deadline is 600 s**, enforced by the probe's iteration callback
  exactly as in pass 1. It is the value `RULE.md` §3.1 derived for groups E and I
  and the value pass 1 actually ran those cells at, so a pass-2 leg that
  reproduces a pass-1 row is reproducing it under the same stop. `SAWPATH` is a
  group-W cell and pass 1 ran it at 300 s; **its pass-2 leg runs at 300 s too**,
  because §5's identity gate compares against the row pass 1 produced and a
  different stop would compare two different solves.
* **The OUTER stop is `timeout 900`** per leg. A leg it kills, or a leg exiting
  non-zero for any other reason, is **CENSORED: no summary, no trace, no row,
  and nothing filled in later.**
* The observer's `max_records` is set to **2 000 000**, above every cell's
  pass-1 `qp_minor_iters`; `summary_truncated` proves per leg that it did not
  bite.

---

## §5 The IDENTITY GATE — what the seam must not have moved

### 5.1 The whole-run gate (every traced cell)

The pass-2 leg's own public row must reproduce **the committed pass-1 row for the
same cell**, read from the bytes of `rows/<CELL>.txt`, on:

`status`, `major_iters`, `qp_minor_iters`, `factorizations`, `steps_accepted`,
`rejected_steps`, `elastic_activations`, `restoration_entries`,
`restoration_sub_status`, `qp_maxiter_exits`, `qp_numerical_exits`,
`qp_failures_routed`, `qp_failures_in_a_row_peak`, `deadline_fired` — **byte
equality, no tolerance** — and `f`, `stationarity`, `feasibility_e`,
`feasibility_i` at CLAUDE.md §7's near-ulp gate, `|a − b| ≤ 1e-12·max(1,|a|,|b|)`,
with `nan` matched as a BYTE fact and never numerically.

**This is the seam's neutrality proof and it is stronger than M3b's**: the
pass-1 row was produced by a DIFFERENT BINARY, built from an INSTALLED library
that has no observer in it at all (`prepare_p2.sh` prints
`QpWalkObserver symbols in libhven.a: 0`). A cell that differs is a **STOP for
that cell**: its traces are not read, and the difference is reported with both
values and the fields.

### 5.2 The per-solve gate (every traced solve that the mapping identifies)

For a traced main-engine solve `k ↔ r`: `summary[k].minors ==
ledger[r].minor_iters` and `summary[k].factorizations ==
ledger[r].factorizations`, against the **pass-1** ledger table — which is the
same pair §2's mapping matched on, now checked against the OTHER run's bytes.

### 5.3 What is checked for a solve with NO ledger record

A sub-driver or lazy-engine solve has no ledger record and **§5.2 cannot be
applied to it; this rule does not pretend otherwise.** What is checked instead is
a CONSERVATION identity over the whole leg, which covers exactly those solves:

  `Σ over ALL summary rows of minors == the leg's own qp_minor_iters counter`
  `Σ over ALL summary rows of factorizations == the leg's own factorizations counter`

and each is reported as its two numbers, not as a bool. If a sum disagrees, the
DIFFERENCE is reported and every NOT-LEDGERED trace on that cell is marked
`UNRECONCILED`. **The conjecture that these two sums must agree is itself a
pre-registered CLAIM about the counters, not an established fact**: if they
disagree on every cell, that is a finding about what `qp_minor_iters` counts, and
it is reported as one rather than treated as a gate failure.

---

## §6 What is reported per traced solve — read-outs, not verdicts

M3b's `classify_walk_m3b.py` and `classify_cause_fix1.py` are run for their
DESCRIPTIVE output only. **Their withdrawn arms are excluded before they are
read**, per M3b's own Corrections:

* **`P_imm`'s null is INVALID** (C6: it assumed an independent/random Dantzig
  pick and used `n − me` as the candidate count; the pick is deterministic and
  the candidate set is the sign-violating subset). `IMMEDIATE_READD` is not
  reported.
* **`RULE-G`'s residual arms are WITHDRAWN** (C9: `active_res_inf` is measured at
  the pass's STARTING `x`, its normalization is not a backward error, the
  equations carry `−dual_mu·I`, and the level holds throughout the walk). The
  `1e-6` active-row bar is withdrawn with them. `NUMERICAL_DIRECTION` is not
  reported as a cause.
* `RIDE_VETOED` and `NUMERICAL_DIRECTION` are LABELS, not causes (C9, C10).

**THE READ-OUTS THAT MATTER, fixed here before any trace is read:**

1. **Tail class and period** — `classify_walk_m3b.py`'s output, with the tail's
   defining working-set signature cycle.
2. **Tail start minor, and the tail's SHARE of the solve's passes.**
3. **The re-admitted constraint's identity and side**, and its **admission
   share** over the tail.
4. **Σ‖Δx‖∞ over the tail** — the zero-motion quantity. Reported as the sum and
   as the count of tail passes with `dx_inf == 0` exactly.
5. **Inertia excess at minor 0 and in the tail** — `(inertia_pos, inertia_neg)`
   against `(inertia_expected_pos, inertia_expected_neg)`, with M3b C11's
   qualification carried: on the border path the printed triple is the retained
   sparse `K0` factor's and the later "excess" is an ALGEBRAIC verdict, not a
   fresh per-working-set factorization.
6. **Curvature quotient vs tolerance** — `p_hp / p_sq` against `curvature_tol`,
   taken apart as M3b C10 requires.
7. **Ride outcome counts**, granular.
8. **Candidate-set recurrence** — `drop_candidates`, `drop_tied`, and how often
   the tail's candidate set repeats.
9. **Warm vs cold, and the incoming warm signature** — and the question the
   dispatch names: **does a later capped solve START in the previous capped
   solve's cycle state?** Reported as the comparison of the later solve's first
   recorded `ws_sig_in` against the earlier solve's tail signatures, as an
   EQUALITY or a NON-EQUALITY of 64-bit values, with M3b C7's qualification that
   the SplitMix64 signature's collision figure is a uniform-hash heuristic and
   not a bound.

### 6.1 The three answers, distinguished IN ADVANCE

So that the rule can return something other than its hypothesis:

* **`CYCLE`** — a tail in which the recorded working-set signature sequence is
  periodic AND `dx_inf == 0` on every tail pass. This is M3b's shape.
* **`LONG WALK`** — no periodic tail, or a tail with non-zero motion:
  `Σ‖Δx‖∞ > 0` over the tail and the working set does not repeat. A solve that is
  simply larger than its cap.
* **`SOMETHING ELSE`** — any trace that is neither, reported with the numbers
  that made it neither and with an explicit statement that this rule did not
  anticipate it. **This arm is a finding about the rule**, exactly as
  `RULE.md` §5.2 #9's `OTHER` is.

The three are decided per TRACED SOLVE, not per cell, and a cell whose first and
last capped solves land in different classes is reported that way rather than
summarized into one.

---

## §7 The discipline this rule holds itself to

Carried from `RULE.md` §9, unchanged, and each of them has already cost this
week something:

1. **Every bar is derived from the fields' semantics**, with its null written
   down. §6.1's `CYCLE` conjunct is an EXACT zero, not a threshold, for exactly
   that reason — M3b C4 narrowed "the same direction" to "every RECORDED field
   repeats", and this rule claims no more than the records carry.
2. **Formal outputs and causal claims are kept apart** (§0, §6).
3. **Any gate or shape declaration is derived by diffing THIS task's own
   base..head**, never inherited from a previous round's measured numbers
   (M3b C8), and **every path handed to a sibling script is checked to exist**
   before it is used — `prepare_p2.sh` does that check and refuses.
4. **A checker that has never been seen to fail is not a checker.** The gate of
   §5 and the mapping of §2 carry fold proofs on constructed inputs, committed
   before they read a leg.

## §8 What no outcome here licenses

Nothing in pass 2 licenses a behaviour change, a default change, or a line of
code. Whatever the traces say, the remedy is a separate task with its own
pre-registered rule and its own gates, and the census's own recommendation
grouping stays a recommendation. Algorithmic references are open-literature
**papers only**; GPL/LGPL-licensed solvers' source is never read (CLAUDE.md §6).
