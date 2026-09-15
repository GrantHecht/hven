# W6 T6 — the attached-callback / sink cost leg at HEAD

`b0a7ffaa`, attached vs unattached, one binary, solo. Read `PROVENANCE.txt`
first — the arms, the levers, the hardware, the serial terms, the idle proof and
what this leg does **not** measure. Every table below is regenerated from `raw/`
and `perf/` by `python3 comparator.py --root . --out -`; the tool's own output is
reproduced verbatim in `comparator.out`. **Nothing here is hand-transcribed.**

> **FIX ROUND 1 — 2026-09-14 (the sol review of T6, every item accepted by the
> settler).** Six corrections are folded into this file, each marked `(fix1)`
> where it lands, and no measured number moved:
> **(1)** the counter check's population was 52 comparisons / 1468
> cell-comparisons because `comparator.py` derived it from the files it found
> and never visited the second interior batch; the comparator now walks an EXACT
> MANIFEST, a missing file is a refusal, and the captured population is **55 /
> 1597 / 0 differences** (§5).
> **(2)** the SQP carrier was attributed to `IterationEvent` construction; at
> source `SqpSolver::fire_iteration_event` computes nothing and the event
> borrows views, so the O(n) attached-only work is the guarded snapshot /
> mapping / declared-diagnostics preparation (§4). The numbers, the O(n)
> reading, the event-density explanation and the M7 disposition are unchanged.
> **(3)** the idle proof omitted `14-ipmleg2-perf.log`: it is regenerated over
> all **34** batches, and the three wall batches that fail R2' are marked
> **NON-QUOTABLE** where their numbers appear (§7).
> **(4)** the ~96 % path-cell share is WALK-ONLY (§2).
> **(5)** the HS level structure is MEASURED; its cause is INFERRED (§6).
> **(6)** two wordings: the batch-2 warm-up is untimed, not unrecorded, and the
> batch-1 identity check pairs SAME-ROUND files (§3).
> Nothing under `raw/` or `perf/` was touched by any of it.

**This is a MEASUREMENT and a FINDING, not a gate.** T6 compares one HEAD with
itself, so there is no commit to keep or revert and §11.1's veto machinery — a
commit-pair rule — does not apply. What the band does here is tell a reader
whether an attached observer costs more than the project's own ±0.5 % bar.

---

## 1. The short version

| leg | mode | arm | corpus instruction ratio | per-cell median | per-cell max | cells outside 0.99–1.01 | band |
|---|---|---|---|---|---|---|---|
| **SQP corpus** (U0, 27 cells) | walk | `a10` callback | **1.00014** (+0.014 %) | 1.00219 (+0.219 %) | 1.00253 | 0/27 | **FLAT** |
| SQP corpus | walk | `a01` sink | **1.00002** (+0.003 %) | 1.00056 (+0.056 %) | 1.00060 | 0/27 | **FLAT** |
| SQP corpus | walk | `a11` both | **1.00016** (+0.016 %) | 1.00274 (+0.274 %) | 1.00311 | 0/27 | **FLAT** |
| SQP corpus | ssn | `a10` callback | **1.00117** (+0.117 %) | 1.00134 (+0.134 %) | 1.00177 | 0/27 | **FLAT** |
| SQP corpus | ssn | `a01` sink | **1.00029** (+0.029 %) | 1.00030 (+0.030 %) | 1.00040 | 0/27 | **FLAT** |
| SQP corpus | ssn | `a11` both | **1.00146** (+0.146 %) | 1.00171 (+0.171 %) | 1.00216 | 0/27 | **FLAT** |
| SQP corpus | ipm | `a10` callback | **1.00074** (+0.074 %) | 1.00078 (+0.078 %) | 1.00119 | 0/27 | **FLAT** |
| SQP corpus | ipm | `a01` sink | **1.00022** (+0.022 %) | 1.00024 (+0.024 %) | 1.00031 | 0/27 | **FLAT** |
| SQP corpus | ipm | `a11` both | **1.00096** (+0.096 %) | 1.00103 (+0.103 %) | 1.00151 | 0/27 | **FLAT** |
| **interior leg** (43 rows) | — | `on` callback, batch 1 | **1.01459** (+1.46 %) | — (single-process leg) | — | — | **MOVED** |
| interior leg | — | `on` callback, batch 2 | **1.01708** (+1.71 %) | — | — | — | **MOVED** |
| **HS leg** (27 cells, `--repeat 200`) | walk | `sink` | 0.99789 median-of-three; **1.000047 level-matched (+0.005 %)** | — | — | — | **below the floor — the registration CONFIRMED** |
| HS leg | ssn | `sink` | 1.00002 median-of-three; **1.000014 level-matched (+0.001 %)** | — | — | — | **below the floor — CONFIRMED** |
| HS leg | ipm | `sink` | 1.00288 median-of-three; **1.001294 level-matched (+0.129 %)** | — | — | — | **below the floor — CONFIRMED** |

**Counter check across every attached/unattached pair: 0 differences.** **55
comparisons, 1597 cell-comparisons** (fix1 — the round-1 figure, 52 / 1468, was
the population the comparator happened to find rather than the one it owed; the
three `raw/ipmleg2` pairs are now in it), every asserted column byte-identical,
`--residual-gate` never needed and never used (§5).

Five things follow, and they are independent of one another.

**(a) THE SQP CORPUS IS FLAT WITH ANYTHING ATTACHED, in all three modes and on
both halves of the definition.** 0 of 27 cells outside 0.99–1.01 in every one of
the nine arms, every corpus figure inside ±0.15 %, and the worst single cell in
the whole leg is +0.31 %. The expected outcome, and it is the outcome.

**(b) THE TOP-LEVEL INTERIOR-POINT LEG IS NOT.** Attaching the same kind of
counting callback moves its instruction count **+1.46 % and +1.71 %** across two
independent batches — three to four times the ±0.5 % corpus bar and outside
0.99–1.01. **That is the finding this task registers for M7** (§3).

**(c) AND THE CARRIER IS NOT THE DISPATCH.** The brief names a
CALLBACK-DISPATCH class — one null check and one indirect call per iteration —
and asks whether the reading is consistent with it. **It is not.** Normalised by
the observer's own event count the delta is 1.8–4.0 **million** instructions per
event; normalised again by problem size it is a few hundred instructions per
node per event, and it does not fall. The cost is the **attached-only payload
preparation** each engine does behind its callback guard — the iterate snapshot,
the engine→caller mapping and the declared diagnostics — and it is O(n) (§4).
*(fix1: round 1 named `IterationEvent` construction. At source the SQP's event
computes nothing and borrows views; the correction is §4's.)*

**(d) SO THE TWO LEGS DIFFER BY EVENT DENSITY, not by engine quality.** A U0
corpus cell takes **two** major iterations however large it is, so it pays two
events against a whole solve; the interior leg's 43 rows take **452** iterations
between them at a tenth to a thousandth of the instructions. Events per 1e9
instructions: interior **4.28**, SQP/ipm 0.403, SQP/ssn 0.617, SQP/walk 0.047 —
a 7× to 90× spread, and the verdicts follow it.

**(e) THE HS SINK RE-READ CONFIRMS THE REGISTRATION AND EXPLAINS IT.** The
registration recorded 0.975–0.993 on the wall and called it "below its own
0.22 % instrument floor". Read on instructions, the floor turns out to be a
**bimodal per-process level structure of 0.15–0.22 %** that *both arms visit*,
and pairing the arms' levels resolves the sink at +0.005 %, +0.001 % and
+0.129 %. The registration's reading stands; it is now stated with a mechanism
(§6).

---

## 2. The SQP corpus leg — the per-cell tables

Full per-cell tables (all 27 cells × 4 arms × 3 modes, plus the other four
events at corpus level) are in `comparator.out` §"The SQP corpus leg". The shape
is the same in all three modes; walk is reproduced here.

Each cell is ONE PROCESS (`--internal-run-one <cell> --engine <mode>`), three
rounds, the four arms run back to back on each cell so an attached and an
unattached reading of the same cell are adjacent in time. The reported value is
the **median of the three rounds**, per cell per arm; the corpus row is the
**sum of per-cell medians**, never a mean of ratios.

### Why the corpus column and the per-cell column are an order of magnitude apart

**In walk mode** (fix1 — this explanation is WALK-ONLY; round 1 wrote it
unscoped) two cells — `f7_n1000_path_warm` (1.079e12 instructions) and
`f7_n800_path_warm` (6.66e11) — carry **95.9551 %** of the U0 set's instructions
between them (against 1.82e12 for all 27), and they take the same two major
iterations as every other cell. Their attached ratio is therefore ~1.00004 and
they pull the sum-of-medians corpus figure onto themselves.

**The same two cells carry only 6.8034 % in ssn and 8.7003 % in ipm**, which is
why the corpus and per-cell columns sit close together in those two modes and an
order of magnitude apart in walk. The gap is a property of the cell set *in a
given mode*, never of the observer.

**Both columns are reported and both are inside the band.** The corpus column is
the one the FLAT definition names. The per-cell median is the one that says what
an attached observer costs a typical cell, and it is 5–15× larger. A reader who
quoted only the corpus figure would understate the effect by that factor; a
reader who quoted only the per-cell median would be describing 4 % of the work.

### The rest of the instrument (walk, corpus level)

| event | a10/a00 | a01/a00 | a11/a00 |
|---|---|---|---|
| `instructions:u` | 1.00014 | 1.00002 | 1.00016 |
| `branches:u` | 1.00015 | 1.00003 | 1.00017 |
| `cycles:u` | 0.99847 | 0.99987 | 0.99917 |
| `branch-misses:u` | 0.99609 | 0.99892 | 0.99652 |
| `L1-icache-load-misses:u` | 1.00223 | 0.99962 | 1.00190 |

**Branches track instructions to a part in 1e5. CYCLES DO NOT — IN ANY MODE, IN
EITHER DIRECTION.** Here the attached arms retire +0.014 % more instructions in
**0.15 % fewer** cycles with 0.4 % fewer branch misses; in ssn the cycle ratios
are 1.00017 / 0.99962 / 1.00116, straddling 1; in ipm they are
1.00148 / 1.00171 / 1.00216, all **larger** than that mode's instruction ratios.
That inconsistency is the point: at a corpus scale of seconds the cycle count
carries everything the instruction count does not — frequency, memory stalls,
run-to-run placement — at a magnitude larger than a 0.01–0.15 % effect, so it
points three different ways on three runs of the same experiment. **It is why
this leg asserts instructions and nothing else, and the cycle column is reported
rather than read.** (Full five-event tables per mode in `comparator.out`.)

### The two levers compose

`a11` is `a10 × a01` to within a few parts in 1e5 in every mode (walk
1.00014 × 1.00002 = 1.00016; ssn 1.00117 × 1.00029 = 1.00146; ipm
1.00074 × 1.00022 = 1.00096). The callback path and the sink path do not
interact, which is what two independent guards on two independent members should
give.

---

## 3. The top-level interior-point leg — the finding

`--engine interior` (`bench/ipm_corpus_leg.cpp`), 43 rows, the whole leg in ONE
process — it forks nothing. The lever is the one that already existed,
`HVEN_LEG_COUNT_CALLBACK` (`bench/ipm_corpus_leg.cpp:638–645`), read and used,
not rewritten.

| batch | off (median) | on (median) | **instructions** | branches | cycles | process wall |
|---|---|---|---|---|---|---|
| 1 (as run) | 105 546 970 655 | 107 086 721 167 | **1.01459** | 1.02219 | 1.02701 | 1.02423 |
| 2 (behind an untimed warm-up) | 105 597 462 389 | 107 401 501 131 | **1.01708** | 1.02713 | 1.03518 | 1.03759 |

Per-round instruction counts, the alternation:

* batch 1 — off `[107 749 090 682, 105 477 426 901, 105 546 970 655]`,
  on `[107 041 161 266, 107 092 063 922, 107 086 721 167]`
* batch 2 — off `[105 810 880 755, 105 525 560 216, 105 597 462 389]`,
  on `[107 393 006 382, 107 401 501 131, 107 432 526 326]`

**BATCH 1's ROUND-1 UNATTACHED PROCESS IS AN OUTLIER AND IS SAID SO RATHER THAN
DROPPED.** It reads +2.1 % over its own rounds 2 and 3 — higher than any
attached round — while **all six processes of that batch wrote byte-identical
rows** (`scripts/compare_replay.py`, 43 cells × 30 columns, 0 differences, each
round's `off` against **that same round's** `on` — fix1: round 1 wrote "every
pair against `perf-off-r3.csv`", which is not what the comparator does). It is a first-process effect, the class the
ownership doc §11.3 already names, and the median of three discards it. Batch 2
exists because a reading should not rest on a median discarding an outlier:
one **untimed warm-up** process runs ahead of it so no round is the batch's
first. It is untimed, not unrecorded (fix1): its CSV and stdout are retained
(`raw/ipmleg2/warmup.csv`, `warmup.stdout`) and only its perf counters are
excluded, because it ran outside `BATCH_START` (`scripts/ipm_leg2.sh:22`). **Neither batch is discarded and both are stated.**

Batch 2 is the cleaner instrument and it is the larger number: each arm is
internally stable to **0.27 %** (off, worst round against the median) and
**0.04 %** (on), while the gap between the arms is **1.71 %** — six to forty
times either arm's own spread.

**VERDICT: MOVED.** Instructions up 1.46–1.71 %, outside 0.99–1.01 and 3–4× the
±0.5 % corpus bar. There is nothing to revert: the cost is what an attached
callback has always cost this engine, and this leg is the first time anything
measured it. **Registered for M7.**

The wall beside it, informational (§7): interior corpus wall
**10.7182 s → 11.0872 s, ratio 1.03443** — **NON-QUOTABLE (fix1)**: its batch
`ipmleg-wall` is UNPROVEN-EVIDENCE under R2' (one alternation snapshot missing),
so the figure may not be quoted or compared, and the verdict above rests on
instructions alone. Read only on this page, it is larger than the instruction
ratio and is not a contradiction — a wall figure at this scale carries the whole
process, including the per-row setup the leg's `wall_s` column does not
bracket.

### The declared omission

**This leg has no SINK arm and this task added none.** `IpmSolver` has an
`attach_trace` and six guarded emit sites (`src/drivers/ipm_solver.cpp:2451`,
`:3033`, `:3132`, `:3369`, `:3982`, `:4425`), but `bench/ipm_corpus_leg.cpp`
exposes no lever for them; adding one changes that leg's surface rather than the
flag surface T6 was scoped to, and the brief's instruction in that case is to
declare the omission and measure what exists. **The interior sink cost is
UNMEASURED and is registered for M7 beside the callback finding.**

---

## 4. What the attached cost actually is

The brief names the **CALLBACK-DISPATCH class**: one null check and one indirect
call per iteration. This is the test of it. A dispatch is O(1) per event, so if
the class were the carrier, the delta divided by the observer's own **event
count** would be a small constant, and dividing again by the **problem size**
would make the number fall.

| leg | mode | arm | instruction delta | events | instr / event | instr / (event × node) |
|---|---|---|---|---|---|---|
| SQP corpus | walk | `a10` callback | 2.485e8 | 86 | 2.89e6 | **454.8** |
| SQP corpus | walk | `a01` sink | 4.488e7 | 211 | 2.13e5 | **32.9** |
| SQP corpus | ssn | `a10` callback | 1.633e8 | 86 | 1.90e6 | **298.9** |
| SQP corpus | ssn | `a01` sink | 4.057e7 | 211 | 1.92e5 | **29.8** |
| SQP corpus | ipm | `a10` callback | 1.573e8 | 86 | 1.83e6 | **287.8** |
| SQP corpus | ipm | `a01` sink | 4.674e7 | 1189 | 3.93e4 | **7.2** |
| interior leg (batch 2) | — | `on` callback | 1.804e9 | 452 | 3.99e6 | **1129.9** |

**It does the opposite of what the dispatch class predicts.** Per event the
delta is **millions** of instructions; per event per collocation node it is a few
hundred, and it stays there across a 20× range of problem sizes. That is an
O(n) cost paid once per attached event.

**What is O(n) per event is the ATTACHED-ONLY PAYLOAD PREPARATION each engine
does behind its callback guard** — and it is not, on the SQP side, the
construction of the event aggregate. *(fix1. Round 1 attributed the whole cost
to "the `IterationEvent` the driver builds after the guard". That is right for
the interior engine and wrong for the SQP one, and the sol review of T6 caught
it. The measured numbers, the O(n) reading, the event-density explanation and
the M7 disposition are unaffected — only the name of the carrier changes.)*

* **The interior engine.** `IpmSolver::fire_iteration_event` returns at its null
  guard (`src/drivers/ipm_solver.cpp:2248`) and everything after it is
  attached-only: the KKT views, the expansion of the reduced primal space to the
  caller's `n`, the mapping out of engine units, the declared diagnostics and
  the event aggregate, with the invoke at `:2402`. The header says outright that
  the views are locals of that frame.
* **The SQP engine.** `SqpSolver::fire_iteration_event` **computes nothing** —
  the comment at `src/drivers/sqp_solver.cpp:4562` says so in those words, and
  its `IterationEvent` holds **borrowed views**, not deep copies
  (`include/hven/drivers/solve_result.h:228`). The O(n) work sits EARLIER, in
  `measure_iterate`, behind its own `if (iteration_callback_)` guard at
  `src/drivers/sqp_solver.cpp:4695`: the iterate snapshot (`event_x`,
  `event_lambda_e`, `event_lambda_i`, `event_z`), the engine→caller scaling map
  applied to it, and the declared diagnostics prepared beside it. Its own
  comment states the design: *"Copied only when a callback is installed, so a
  solve without one pays nothing at all."*

Nothing is prepared on the unattached path in either engine, which is why `a00`
pays literally nothing and why the guard is the right design. The measurement
cannot separate the copies from the diagnostics — it sees their sum — and does
not claim to.

**So the CALLBACK-DISPATCH class is real and is in there, and this leg cannot
see it.** A null check and an indirect call are tens of instructions against a
per-event cost of millions — three to five orders of magnitude below the
instrument. What an attached callback costs on these problems is **the payload
prepared for it**, and that scales with the problem, not with the iteration
count alone.

**The sink is 9–60× cheaper per event than the callback**, and the ratio tracks
what each event carries: a `TraceSink` event is a small POD, an `IterationEvent`
is a set of views over the whole iterate. The ipm mode's sink is cheapest of all
per event (7.2 instr/node/event) because it fires the most events (1189) and
most of them are IPQP iteration events, the smallest of the set.

---

## 5. The counter check — an attached observer changed no trajectory

This is the one thing that could have stopped the leg, and it is clean.

Every attached/unattached pair is compared by the committed comparator,
`scripts/compare_replay.py`, with **no `--residual-gate`**: exact string
equality on every column but `wall_s`, so counters, statuses **and residuals**
must all be byte-identical. The HS schema, which that tool does not know, is
compared by `comparator.py`'s own column walk, which excludes the wall columns,
the `trace` label and the `ev_*` per-site event counts — those exist only when a
sink is attached and **are** the evidence it fired — and compares every
remaining column exactly.

| population | pairs | what was compared | differences |
|---|---|---|---|
| SQP corpus wall CSVs | 9 | 27 cells × 75 columns, 3 modes × 3 attached arms | **0** |
| SQP single-cell perf CSVs | 27 | 27 cells × 75 columns, 3 modes × 3 rounds × 3 arms | **0** |
| interior leg, batch 1 | 4 | 43 rows × 30 columns, 1 wall + 3 perf rounds | **0** |
| **interior leg, batch 2** (fix1) | **3** | 43 rows × 30 columns, 3 perf rounds | **0** |
| HS leg | 12 | 27 cells × 17 columns, 3 modes × (1 wall + 3 perf rounds) | **0** |
| **total** | **55** | **1597 cell-comparisons** | **0** |

**THE POPULATION IS NOW AN EXACT MANIFEST, AND THAT IS THE FIX (fix1).** Round 1
reported 52 / 1468 and called it "every pair". It was not: `comparator.py`
derived the population from the `a00` files it FOUND and skipped any pair whose
files were absent, so the three `raw/ipmleg2` pairs — the second interior batch,
the one that CONFIRMS the finding — were never visited, and a deleted or
mistyped file would have shrunk the claim silently. `expected_manifest()` now
LISTS every comparison this leg owes, with the 27 cell ids spelled out; a
missing file is a REFUSAL, the comparator exits non-zero, and the run checks its
own totals against the 55 / 1597 the manifest demands. Falsified by renaming one
file (`raw/ipmleg2/perf-on-r2.csv`) in a scratch copy: 54 / 1554, one named
MANIFEST REFUSAL, exit 2.

The residual gate was never needed. Residuals came back byte-equal across
processes here — the same code, the same thread count, the same machine — so
CLAUDE.md §7's near-ulp allowance had nothing to relax. Full listing in
`comparator.out` §"The counter check".

**The observers were reached, and that is evidence, not assumption.** Each
attached run prints its own counts to stderr (never to the CSV — the schema does
not move for an instrument): `ATTACHED-CALLBACK … events=N` and
`ATTACHED-SINK … qp_mode_dispatch=… other=N`, retained per run in
`raw/sqpperf/<mode>/r<n>/<arm>-<cell>.stderr`. Every attached arm of every cell
recorded non-zero events. A walk cell fires no `qp.mode` site at all, which is
why the sink report carries an `other` count as well as the named ones.

---

## 6. The HS sink arm, re-read

The registration (`docs/notes/2026-08-m6-ledger.md:4455–4457`): *"the HS leg
MOVED FASTER in five of six combinations (0.975–0.993), informational, below its
own 0.22 % instrument floor."* That was a **wall** reading. This is the same
arm, the same lever, the same binary, read on **instructions** — the instrument
the wall reading was below — at `--repeat 200 --hs-warmup 1`, identical across
arms, three rounds alternated.

| mode | off (median) | sink (median) | median-of-three | **level-matched** |
|---|---|---|---|---|
| walk | 33 762 321 046 | 33 691 054 816 | 0.99789 | **1.000047 / 1.000048** |
| ssn | 42 209 083 906 | 42 209 799 511 | 1.00002 | **1.000014 / 1.000012** |
| ipm | 49 572 356 936 | 49 715 282 633 | 1.00288 | **1.001294 / 1.001295** |

**The median-of-three ratios are an artefact, and the per-round numbers show it
outright.** A three-round instruction sample of this leg is not scattered: it
lands on **two discrete levels**, 0.15–0.22 % apart, and **both arms visit both
of them**.

* walk — off `[33 689 414 335, 33 762 321 046, 33 762 326 000]`,
  sink `[33 763 952 681, 33 691 003 049, 33 691 054 816]`. Off visits the low
  level once and the high level twice, sink the other way round, so the two
  medians land on **different levels** and the ratio reports the level gap.
* ssn — off `[42 209 077 421, 42 209 083 906, 42 274 063 209]`,
  sink `[42 274 589 948, 42 209 685 256, 42 209 799 511]`. Same two levels.
* ipm — off `[49 572 356 936, 49 651 061 290, 49 572 334 090]`,
  sink `[49 715 282 633, 49 636 501 334, 49 715 346 124]`. Same structure.

Pairing the arms' sorted extremes — lowest with lowest, highest with highest —
cancels the level and leaves the arm. **The two pairings agreeing to 3e-7–2e-6
in all three modes is what says the levels really are shared** and the pairing
is the right one; a spurious pairing would not reproduce itself to the seventh
digit. (The interior leg's own pairing **disagrees**, 1.0148 vs 0.9939 and
1.0177 vs 1.0153, which is exactly right: its arms do **not** share levels
because the arm difference is real and larger than the level structure. The
comparator prints the disagreement and declines to read it.)

**VERDICT: the registration is CONFIRMED, and the level structure is MEASURED.**
The "0.22 % instrument floor" is a per-process bimodal level structure that no
arm changes. **The structure is measured; its CAUSE is INFERRED** (fix1): the
two levels, their separation and the fact that both arms visit both are read
directly off the retained per-round counts above, but address layout, allocator
state and page placement are the *candidate* mechanisms — the same per-process
constant §11.3(ii) identified on this leg at T6.d — and this leg measured none
of them separately. The level matching itself is a descriptive, post-hoc
estimator over three samples per arm; what makes it credible is that the two
independent pairings agree to 3e-7–2e-6, not that the cause is known. The sink's own
cost sits at **+0.005 % (walk), +0.001 % (ssn), +0.129 % (ipm)**, below that
floor in all three modes. **The reading does not move.** What moves is the
confidence: the earlier "MOVED FASTER in five of six" was the floor, not the
sink, and the sink never made anything faster.

The HS wall beside it, informational and **NON-QUOTABLE (fix1** — its batch
`hs-wall` is UNPROVEN-EVIDENCE under R2', five alternation snapshots missing**)**:
walk 1.00673, ssn 1.00413, ipm 0.98838 — still scattered on both sides of 1,
still below its own floor, still not a number to read.

---

## 7. The wall readings, beside the instructions (INFORMATIONAL)

CLAUDE.md §7: serial, solo, one solve at a time, `MKL_NUM_THREADS=1`, pinned to
cpu2 with its SMT sibling cpu10 left idle and **measured**. One reading per arm,
one arm per batch. The corpus figure is the sum of the per-cell `wall_s` column
— the leg's own solve bracket — exactly as T8.9r's leg 1 read it. **Nothing in
this leg asserts a wall; the asserted instrument is instructions.**

| leg | mode | a00 / off | a10 | a01 | a11 | idle gate |
|---|---|---|---|---|---|---|
| SQP corpus (27 cells) | walk | 6.0120 s | 1.00063 | 1.00181 | 1.00592 | PINNED-CLEAN |
| SQP corpus | ssn | 11.1258 s † | 0.99547 | 0.99829 | 0.99968 | **† NON-QUOTABLE** |
| SQP corpus | ipm | 14.8402 s | 1.00026 | 1.00002 | 1.00211 | PINNED-CLEAN |
| interior leg (43 rows) | — | 10.7182 s † | **1.03443** † (`on`) | — | — | **† NON-QUOTABLE** |
| HS leg (sum of per-cell medians) | walk | 0.022592 s † | 1.00673 † (`sink`) | — | — | **† NON-QUOTABLE** |
| HS leg | ssn | 0.030076 s † | 1.00413 † (`sink`) | — | — | **† NON-QUOTABLE** |
| HS leg | ipm | 0.037075 s † | 0.98838 † (`sink`) | — | — | **† NON-QUOTABLE** |

**† NON-QUOTABLE (fix1).** Three of this leg's fourteen wall batches do not meet
R2' (`docs/notes/data/2026-09-m6-w5-t8-runtime/PROVENANCE.txt:520–540`), so
**every wall number taken from them is marked NON-QUOTABLE and is not a
measurement**: it may not be quoted, carried into a comparison, or cited as
evidence for anything.

| batch | what fails R2' | the numbers it carries |
|---|---|---|
| `sqpwall-ssn-a00` | **UNPROVEN-FRACTION** — cpu10 0.7242 % (0.13 s of foreign task time across a 17.95 s window), over the 0.5 % bar on the SMT sibling; cpu2 0.0557 % is inside | the ssn corpus wall 11.1258 s and therefore all three ssn wall RATIOS |
| `ipmleg-wall` | **UNPROVEN-EVIDENCE** — 1 alternation snapshot missing (both fractions inside: cpu2 0.0000 %, cpu10 0.1265 %) | the interior wall 10.7182 s and the 1.03443 ratio |
| `hs-wall` | **UNPROVEN-EVIDENCE** — 5 alternation snapshots missing (cpu2 0.1096 %, cpu10 0.0548 %) | all three HS wall figures and ratios |

**They are marked, not re-run.** R2' as written says "a batch that cannot be
proven is re-run"
(`docs/notes/data/2026-09-m6-w5-t8-runtime/PROVENANCE.txt:533–538`). The settler
ruled at this fix round that marking them NON-QUOTABLE discharges it here, on
the brief's own terms: *nothing in this leg asserts a wall*, the asserted
instrument is instructions, and a number WITHDRAWN FROM QUOTATION cannot carry
an unproven idle condition into anything. A batch whose number is quoted must be
re-run; these numbers are not quoted.
The eleven remaining SQP wall batches are PINNED-CLEAN and their numbers stand
as informational. Re-taking any of the three is an M7 option and costs about
five minutes of solo box time, if a later task ever wants to quote one.

The SQP corpus walls scatter on both sides of 1 at the 0.1–0.6 % level, which is
what a one-reading-per-arm wall on a 6–15 s corpus gives; they neither confirm
nor contradict the instruction reading and are not read as doing either. The
interior leg's **+3.4 %** is the one wall figure larger than its own noise and it
agrees in direction and order with that leg's instruction finding — **but it
comes from a NON-QUOTABLE batch (†)**, so it is an observation on this page and
nothing more. The finding it agrees with rests on instructions, which are
scheduling-invariant and carry no such caveat.

**The idle proof** (`IDLE-PROOF.md`, per-pid appendix in `IDLE-PIDS.txt`) covers
**34 batches** — fix1: round 1's proof was generated before the second interior
batch existed and omitted `logs/14-ipmleg2-perf.log`, so it counted 33. The
regenerated proof is the same tool, unedited (sha256 `3394879d…`), over all 34
retained batch logs:

| class | batches | worst cpu2 / cpu10 fraction | verdicts |
|---|---:|---|---|
| wall batches (the only wall-quoting ones) | 14 | 0.1096 % / 0.7242 % | **11 PINNED-CLEAN**; the 3 marked † above are not |
| instruction batches, long (≥ 70 s timed wall) | 12 | 0.8110 % / 0.9747 % | all 12 UNPROVEN-EVIDENCE (snapshot cadence); 7 of them also UNPROVEN-FRACTION (0.59–0.81 % cpu2 on the nine-cell-group batches, 0.97 % cpu10 on `hs-perf`); the other 5 read 0.020–0.042 % on cpu2 |
| instruction batches, short (walk groups, 2.8–10.8 s) | 8 | 6.4748 % / 0.4634 % | all 8 UNPROVEN-EVIDENCE and UNPROVEN-FRACTION (0.73–6.47 % cpu2) — a resolution artefact, below |
| **total** | **34** | — | 11 PINNED-CLEAN, 16 UNPROVEN-FRACTION, 22 UNPROVEN-EVIDENCE (a batch can be both) |

`14-ipmleg2-perf.log` itself: 6 timed runs, 71.44 s, cpu2 **0.0420 %**, cpu10
**0.0700 %**, 3 alternation snapshots missing — UNPROVEN-EVIDENCE, both
fractions comfortably inside the bar. It asserts no wall.

**The short groups' 5–6.5 % is a MEASUREMENT-RESOLUTION artefact and the
arithmetic is now complete (fix1).** Round 1 wrote "one 10 ms tick is 0.36 % of
a 2.8 s batch", which is true and does not by itself explain a 5–6 % reading:
0.15–0.18 s is fifteen to eighteen ticks, not one. The mechanism is CUMULATIVE
ROUNDING. Each of the batch's **36** timed runs has its own user time read from
`/usr/bin/time`'s `%U` (`scripts/common.sh:171`), which prints CENTISECONDS, and
the tool subtracts the sum of those 36 rounded values from a `/proc/stat` delta
that was not rounded the same way. Up to 0.01 s of rounding per process across
36 processes is up to 0.36 s of apparent foreign time; the observed residual is
0.15–0.18 s, inside that. **The batches stay UNPROVEN-FRACTION** — the
explanation is not a proof, and the record keeps the verdict the tool gave. The
same arithmetic sizes the long batches' own residuals: a 27-cell group runs 108
timed processes, up to 1.08 s of accumulated centisecond rounding, and the
`sqpperf-*-gall` batches read 0.59–0.81 % of a 73–94 s window — 0.43–0.59 s,
inside it. The long walk groups, which run 16–36 processes across 800 s, read
0.020–0.032 %: the same absolute rounding spread over a window 80–290× longer.

PROVENANCE.txt states the rest, including the instruction batches' own evidence
gaps and why none of them reaches a scheduling-invariant instruction count.

---

## 8. What this leg does not measure

* **The interior leg's sink arm** — declared omission, §3, registered for M7.
* **Anything on Apple or Windows** — UNOBSERVED, and nothing here is
  interpolated to them.
* **A commit pair** — there is none, by construction.
* **An observer that does work.** Both attached observers here are the cheapest
  forms that can be written, on purpose: the question is what *attaching* costs,
  and an observer that did real work would measure itself. A real consumer's
  callback pays everything below **plus** its own body.

## 9. The disposition

**Nothing to revert, one thing to register.**

1. **The SQP corpus is FLAT with anything attached** — the expected outcome,
   recorded, no action.
2. **The top-level interior-point leg is MOVED at +1.46 %/+1.71 %** — outside
   the ±0.5 % bar. **Registered for M7**, with the carrier already identified:
   the per-event attached-only payload preparation (snapshot, engine→caller
   mapping, declared diagnostics), O(n), done behind the callback guard — §4,
   as corrected in fix1.
   The obvious shape of a remedy — building the event lazily, or letting a
   callback declare which views it wants — is an M7 design question and this
   task deliberately proposes none and tunes nothing.
3. **The interior leg's sink cost is UNMEASURED** — declared omission, also for
   M7.
4. **The HS registration is CONFIRMED, not moved**, and its "0.22 % instrument
   floor" now has a measured mechanism.
5. **The two levers land** (`--callback`, `--trace`, default off, stamped,
   refused where they cannot apply), so the next reader can re-take any of this
   without writing an instrument first.
