# M6 W5 T8.9r-attrib2 — the interior leg's +2.49 % localised per group-1 task

**Leg date 2026-09-11, BASE `7b704fc`, branch `m6`. No library, test or bench source was
changed; nothing was built. The eleven arm binaries are the T8.9r-attrib leg's, re-verified
by sha256 (`arms.txt`): 11/11 corpus executables and 11/11 `libhven.a` matched, 0 mismatch.**

`reading.md` §5 found the top-level interior-point leg **+2.49 % slower** from `102f729` to
`e51a7e0` — 29 of 29 banded F7 rows outside 0.99–1.01, 28 reproducible slowdowns, and **the
twelve common counter columns IDENTICAL**. §5 could not say which of T8.1–T8.8 carried it,
having no arm between them. This directory puts an arm at every group-1 head and asks.

---

## 1. The answer, in four sentences

**T8.4 is the DOMINANT step, and nothing else is close — bounded to exactly that (corrected
at fix2; astra's fix1 review, item 7).** On all eleven scored rows T8.4's wall step is above
1.01 (1.0147 to 1.0425, median **1.0300**); on those same rows **no other task has a single
step above 1.01 except one** (T8.7 on one row, 1.0111). T8.4's median share of the whole
cumulative in log space is **+1.03**, and it is at or above 0.80 on 9 of the 11 rows, while
every other task's median share is between −0.20 and +0.14 and reaches 0.80 on **zero** rows.

**What that median share does NOT license is a statement about the aggregate.** On the
eleven-row corpus T8.4's own step is **1.0236838** and the remaining ten tasks' aggregate
factor is **≈1.0013197** — small, but POSITIVE, not the "other nine net slightly negative"
this file first wrote. So: one task carries the great majority of the step, and no other
carries more than about a tenth of a percent of it. "All of it is T8.4" is withdrawn.

**The cumulative, and what it does and does not reproduce.** Over the eleven scored rows this
leg reads **1.0250** against §5's **1.0249** — a different round set of the SAME DAY (fix1's
timed logs end 17:16:56 UTC and this leg's begin after them; "a different day" was wrong) and
a different cell set. **It is NOT a like-population reproduction**: §5's 1.0249 is its full
29-row population, and §5's own ratio over THESE eleven rows is **1.0238**.

**On the instruction side this leg returns NO VERDICT, and the reason is measured.** T8.4's
pair is ROW-ADDING (19 → 21 rows), which by itself forbids a verdict; and independently of
that, the *floor* of the whole-process instruction count on this leg — read off the control
arm whose `libhven.a` is byte-identical to the base's — is **+1.27 %**, larger than every
like-for-like step this leg measured and larger than T8.4's own +1.01 %. Pass B is worse:
the control moves further than the pair under test on five of the six Zen 3 events.

**AND THE LAYOUT READING §6 DERIVED IS WITHDRAWN AT FIX2.** §6 estimated T8.4's
whole-process instruction step from the two rows T8.4 added and concluded LAYOUT-MOVED; the
estimate never measured those rows' instructions, read a central residual as an upper bound,
and established no branch or cycle/miss identity. **Under §11.1 the classification is
UNRESOLVED.** `reading.md` §14 later measured the instruction question directly, in a
single-row process, and returned **NOT WORK-MOVED without reaching the identity band
either** — the same UNRESOLVED, with an instrument instead of a derivation. The disposition
is not this directory's; §11.1 and the owner have it, and the owner ruled KEEP on
2026-09-11 (`reading.md` §15).

---

## 2. What was measured, and why these twelve rows

`--engine interior --cells f7_n1000_bound_physics,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n20000_bound_neutral`
— four dual-binding F7 cells × three fixed-variable treatments = **twelve base rows** across
three problem sizes and both treatments §5's band covers. The variant rows (`cap1`,
`stalled`, `resto_infeasible`, `solve_optimize`, `warm_*`, `parts2`) run unconditionally at
the arms that have them and are recorded but **not scored**.

**WALL**, five rounds, one batch per round, arm order rotated by (round−1) so no arm keeps a
fixed position. This asserts wall clock (CLAUDE.md §7) and ran under the fix1 R2 discipline;
§7 below and `IDLE-PROOF.md` carry the evidence.

**INSTRUCTIONS**, pass A on the *same invocation* at every arm — three rounds as `A-*` and
five more as the diff leg's `P-*` — and pass B (the artifact's Zen 3 six) on the localised
pair plus the control pair, three rounds.

### The R3 rule, and why the cell order is part of the pin

`--engine interior` writes its base rows in the order the cells were **requested**, three
treatments per cell, `MakeParameter` first. The cell order above therefore makes
**`f7_n1000_bound_physics/MakeParameter` the first row every process writes, at every one of
the eleven arms** — verified from the row keys of all 55 wall CSVs — so fix1's positional
warm-up rule excludes *the same row* everywhere. Eleven rows are scored.

That the excluded row is a warm-up and not data is visible in its own line of §4's table:
its T8.1 step reads **2.8937** and its T8.5 step **0.7024**, against the ±0.6 % every other
row shows for those two tasks. It is the process paying for MKL's first call and the
allocator's first growth, and it is excluded by position, decided before this leg ran
(`../predeclaration-R1-R3.txt`, fix1's rule, adopted here unchanged).

### The like-for-like map, read off the row counts

| pair | task | rows | verdict permitted on whole-process instructions |
|---|---|---|---|
| a01→a02 | T8.1 | 15 → 15 | **LIKE-FOR-LIKE** (and a control — see below) |
| a02→a03 | T8.2 | 15 → 19 | ROW-ADDING (+4) — **NO VERDICT** |
| a03→a04 | T8.3 | 19 → 19 | **LIKE-FOR-LIKE** |
| a04→a05 | **T8.4** | 19 → 21 | ROW-ADDING (+2) — **NO VERDICT** |
| a05→a06 | T8.5 | 21 → 23 | ROW-ADDING (+2) — **NO VERDICT** |
| a06→a07 | T8.6 | 23 → 23 | **LIKE-FOR-LIKE** |
| a07→a08 | T8.7 | 23 → 23 | **LIKE-FOR-LIKE** |
| a08→a09 | T8.7b | 23 → 23 | **LIKE-FOR-LIKE** |
| a09→a10 | T8.8 | 23 → 23 | **LIKE-FOR-LIKE** |
| a10→a11 | T8.9 | 23 → 25 | ROW-ADDING (+2) — **NO VERDICT** |

The row counts are read off this leg's own CSVs, not assumed. The brief predicted
T8.2→T8.3 and T8.5→T8.6→T8.7→T8.7b→T8.8; measurement adds a01→a02 to that list and
confirms the rest. **The per-row WALL is like-for-like at all eleven arms regardless** —
that is the currency the localisation rests on.

---

## 3. The per-row wall step per task

Median of five rounds. **Bold** = above 1.01.

| row | T8.1 | T8.2 | T8.3 | **T8.4** | T8.5 | T8.6 | T8.7 | T8.7b | T8.8 | T8.9 | cumul |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `f7_n1000_bound_physics/MakeParameter` *(R3 — EXCLUDED)* | *2.8937* | *0.9605* | *0.9809* | *1.0116* | *0.7024* | *1.1276* | *0.9338* | *0.9936* | *0.8865* | *0.8996* | *1.6162* |
| `f7_n1000_bound_physics/MakeConstraint` | 1.0079 | 0.9937 | 0.9967 | **1.0331** | 0.9938 | 0.9990 | 1.0020 | 1.0002 | 1.0027 | 0.9913 | 1.0199 |
| `f7_n1000_bound_physics/RelaxBounds` | 1.0066 | 0.9972 | 0.9979 | **1.0255** | 0.9932 | 1.0038 | 1.0009 | 1.0001 | 1.0043 | 0.9920 | 1.0214 |
| `f7_n5000_bound_physics/MakeParameter` | 1.0028 | 0.9982 | 0.9994 | **1.0425** | 0.9963 | 1.0041 | 0.9963 | 1.0003 | 1.0041 | 0.9957 | 1.0394 |
| `f7_n5000_bound_physics/MakeConstraint` | 1.0024 | 0.9996 | 0.9985 | **1.0329** | 0.9976 | 1.0041 | 0.9966 | 0.9997 | 1.0033 | 1.0058 | 1.0406 |
| `f7_n5000_bound_physics/RelaxBounds` | 1.0045 | 0.9972 | 0.9992 | **1.0343** | 0.9923 | 1.0072 | 1.0007 | 0.9976 | 1.0041 | 1.0031 | 1.0403 |
| `f7_n10000_bound_neutral/MakeParameter` | 1.0008 | 0.9972 | 1.0007 | **1.0147** | 1.0076 | 0.9925 | **1.0111** | 0.9868 | 1.0025 | 1.0085 | 1.0222 |
| `f7_n10000_bound_neutral/MakeConstraint` | 1.0037 | 0.9987 | 0.9999 | **1.0301** | 0.9917 | 1.0068 | 0.9917 | 0.9976 | 1.0034 | 1.0057 | 1.0292 |
| `f7_n10000_bound_neutral/RelaxBounds` | 1.0000 | 0.9997 | 1.0000 | **1.0300** | 0.9845 | 1.0053 | 0.9993 | 1.0024 | 1.0040 | 0.9934 | 1.0181 |
| `f7_n20000_bound_neutral/MakeParameter` | 1.0031 | 0.9993 | 1.0015 | **1.0255** | 0.9964 | 1.0037 | 1.0019 | 0.9975 | 1.0038 | 0.9939 | 1.0266 |
| `f7_n20000_bound_neutral/MakeConstraint` | 1.0011 | 0.9998 | 1.0020 | **1.0178** | 0.9941 | 1.0078 | 0.9991 | 1.0049 | 1.0043 | 0.9972 | 1.0282 |
| `f7_n20000_bound_neutral/RelaxBounds` | 1.0005 | 1.0017 | 0.9983 | **1.0194** | 0.9982 | 1.0004 | 1.0063 | 1.0027 | 1.0044 | 0.9843 | 1.0160 |
| **rows above 1.01 (of 11 scored)** | 0 | 0 | 0 | **11** | 0 | 0 | 1 | 0 | 0 | 0 | — |
| **median share of the cumulative** | +0.07 | −0.05 | −0.02 | **+1.03** | −0.20 | +0.14 | +0.02 | +0.00 | +0.14 | −0.11 | — |

The share is `ln(step)/ln(cumulative)`; the ten sum to 1 on every row by construction. T8.4
is at or above 0.80 on 9 of the 11 rows; every other task on **none**.

### The cumulative check against §5

| | this leg | §5 |
|---|---|---|
| corpus ratio | **1.0250** (11 scored rows, median of 5) | **1.0249** (29 banded F7 rows, 3 rounds) |

The per-row columns disagree by up to 2.9 percentage points and that is reported, not
smoothed: two independent legs, no shared round and no shared process, five medians here
against three there, a per-process layout term in both. What reproduces is the quantity the
owner asked about — the corpus ratio — and the shape: every scored row moves the same way in
both legs, and both legs put the move in one place.

---

## 4. The instruction side — and the floor that suppresses it

Whole-process counts, median of five rounds:

| task | instructions | branches | b/i | like-for-like? |
|---|---|---|---|---|
| T8.1 *(control)* | **+0.012712** | +0.025284 | 1.99 | yes |
| T8.2 | +0.004009 | +0.004655 | 1.16 | ROW-ADDING |
| T8.3 | +0.002032 | +0.003988 | 1.96 | yes |
| **T8.4** | +0.010084 | +0.005811 | 0.58 | ROW-ADDING |
| T8.5 | −0.004896 | −0.009674 | 1.98 | ROW-ADDING |
| T8.6 | +0.003019 | +0.005954 | 1.97 | yes |
| T8.7 | −0.003063 | −0.006002 | 1.96 | yes |
| T8.7b | +0.000349 | +0.000672 | 1.93 | yes |
| T8.8 | +0.002188 | +0.004324 | 1.98 | yes |
| T8.9 | +0.012768 | +0.010149 | 0.79 | ROW-ADDING |

**THE FLOOR IS MEASURED, NOT ASSUMED.** Arm 02's `libhven.a` is byte-identical to arm 01's,
so its *library* work is exactly zero; whatever its step reads is this instrument's floor.
It reads **+1.27 %**. **Every like-for-like step in the table is smaller than that in
magnitude** (the largest is T8.7's −0.31 %), and so is T8.4's +1.01 %. On this leg, in this
currency, nothing is resolvable.

Its `b/i` of 1.99 is also the answer to what the floor *is*: attribution.md §3's fingerprint
reads a code-layout cluster transition at ≈2.4 and executed work at ≈1.1, and every
like-for-like pair here reads **1.93–1.99** — the layout family — while the three row-adding
pairs, whose step is dominated by real added rows, read 0.58–1.16.

**Why this leg's floor is a hundred times the SQP leg's 2e-5.** The whole-process count is
dominated by rows the wall reading excludes. The R3 warm-up row's wall spans a factor of ~30
between its worst and best appearance, and the round-to-round spread of the whole-process
instruction count tracks it: 1.8e-2 at a01 and 5.3e-2 at a04 against 5e-4 to 1.2e-3 at the
later arms. `perf stat` counts the process; it cannot exclude a row.

**And a single-row interior process is not available without a source change.**
`--internal-run-one`, which is how the SQP legs reach one cell in one process, rejects
`interior`: `run_cell` throws for any engine but `walk|ssn|ipm`
(`bench/corpus_cells.h:1843`). This brief forbids a source change, so the gap §5 (iv)
registered for W6 stands, with this leg's floor measurement added to it.

### Pass B

| event | T8.1 *(control)* | **T8.4** |
|---|---|---|
| `instructions:u` | 1.01252 | 1.01112 |
| `cycles:u` | 1.03569 | 1.02098 |
| `de_dis_uop_queue_empty_di0:u` | 1.22696 | 1.15363 |
| `op_cache_hit_miss.op_cache_miss:u` | 1.01420 | 0.99629 |
| `op_cache_hit_miss.op_cache_hit:u` | 1.04153 | 1.00569 |
| `ic_fetch_stall.ic_stall_any:u` | 1.02843 | 1.03586 |
| IPC | 0.97762 | 0.99035 |

**Read it on the control first.** The pair with exactly zero library work moves further than
the pair under test on five of the six events and further on IPC. The one event where T8.4
moves further — `ic_fetch_stall.ic_stall_any`, 1.0359 against 1.0284 — sits far inside the
floor the rest of the row establishes. **Pass B separates nothing here.**

---

## 5. The differencing instrument — built, run, and REJECTED

The wall localises to a row-adding pair, so an instrument that could difference the added
rows away was worth building, and this leg built it: two processes per arm per round,
`Q = --cells <the first cell only>` and `P = --cells <the four>`, so that **P − Q is exactly
the nine large base rows** with every unconditional row, every variant row *and the R3
warm-up row* in both. That would be like-for-like even across a row-adding pair — and it is
**not** fix1's I4: I4's subtrahend was a `--cells hs071_x1_fixed` process whose first solve
was a different, tiny row paying costs the minuend's first solve had already paid, and nine
of its differenced quantities came out negative. Here both processes' first solve is the
same row in the same position.

The construction worked: **P − Q is 9 rows at every one of the eleven arms.** The instrument
still fails, and the data says why: the Q process's own instruction count scatters by
**15–22 % round to round**, while the P process at the later arms is reproducible to under
0.2 %. The rows Q contains are in P too — so **those rows do not cost the same inside a
process that goes on to run nine large solves as they do in a process that does not**, and
the cancellation the instrument depends on is not exact. The control arm's differenced step
lands far from 1, which is the whole verdict.

**It is reported, not used.** The raw data is retained in `raw/diff/` and
`attribute_interior.py` §8 prints it in full. What this leg *does* use from that leg is the
`P` process on its own, as the five-round whole-process sample of §4 — the same invocation
the wall leg times, with `perf` attached.

---

## 6. The added-row accounting — what is available in place of a verdict

A row-adding pair gets no verdict. What can still be asked of it is arithmetic: **does the
step equal the rows the later arm added?** The added rows' own wall is in the CSV; at the
earlier arm's own whole-process instruction rate it gives an estimate of their cost.

**The assumption is stated: this charges the added rows the process's *average* instructions
per second.** The arm-to-arm spread of that rate is 3.3 % (9.548–9.860 e9/s), and the
residual below is bounded with the extreme rates, not the central one.

| pair | added rows | their wall | estimated | MEASURED step | residual on the SHARED rows |
|---|---|---|---|---|---|
| T8.2 | 4 (`cap1` ×2, `stalled`, `resto_infeasible`) | 0.016907 s | 0.164 e9 | 0.228 e9 | +0.061 to +0.066 e9 (+1.1e-3) |
| **T8.4** | 2 (`solve_optimize` ×2) | 0.057578 s | 0.563 e9 | **0.576 e9** | **+0.008 to +0.026 e9 (+2.3e-4)** |
| T8.9 | 2 (`parts2` ×2) | 0.077305 s | 0.744 e9 | 0.735 e9 | −0.027 to −0.003 e9 (−1.5e-4) |

**THE CONCLUSION THIS TABLE WAS USED FOR IS WITHDRAWN AT FIX2 (astra's fix1 review,
item 7).** The arithmetic is retained — it is retained evidence — and the reading taken from
it is not. Three things are wrong with it:

* **The added rows' instructions are never measured.** The "estimated" column multiplies
  those rows' round-1 WALL by the whole process's instructions divided by the summed row
  wall. Whole-process instructions include work outside every row's timing bracket, and the
  3.3 % spread of aggregate rates does not bound any individual row's rate.
* **2.3e-4 is the CENTRAL residual fraction, not an upper bound.** The "+0.008 to +0.026 e9"
  beside it is the extreme-rate spread; the fraction was then read as though it were the
  worst case. On its own it already exceeds §11.1's 1e-4 identity tolerance.
* **No branch identity and no cycle/miss accounting was established**, both of which §11.1's
  LAYOUT-MOVED band requires beside the instruction identity. And a 3 % wall increase does
  not imply 3 % more instructions to begin with.

So the earlier sentence — "that is the signature §11.1 calls LAYOUT-MOVED" — **is withdrawn.
Under §11.1 the classification is UNRESOLVED**: neither identity-banded nor instructions-up.

**What still stands from the table:** the nineteen rows both arms run take **2–4 % more
wall**, reproducibly, across five rounds, and §5 established their **twelve counter columns
are identical** end to end — same iterations, same factorizations, same solves, same
residuals. The direct instruction measurement is refused here twice over — by the row-adding
rule and by the control arm's floor — and `reading.md` §14 later took it with a single-row
process, where the verdict is **NOT WORK-MOVED** and §11.1's identity band is **not reached
either**: the same UNRESOLVED, arrived at with an instrument rather than a derivation.

### The mechanism, and what it is not

**It is not the declared diagnostics.** T8.4's own new per-call work is
`compute_declared_diagnostics`, which the interior driver calls **once per solve** at
`src/drivers/interior_point_solver.cpp:2215`. attribution.md §5 measured that same commit's
cost on the SQP legs at ≈342 500 instructions at n = 1000, linear in the declared dimension
— about 6.7 M at n = 20000, against the ≈10.6 e9 instructions an `f7_n20000` row executes.
**That is 0.06 % of a row whose wall moved 2.0 %.** The cost T8.4 declared is real, is
where it said it would be, and is two orders of magnitude too small to be this.

**AND THE PLACEMENT MECHANISM BELOW IS REFUTED IN BOTH HALVES BY `t84/` (§12 of
`reading.md`), which is retained here as the reading this file published.** `8ae1618` is the
commit that adds `drivers/solve_result.cpp` to `src/CMakeLists.txt`; `ar t` puts the new
member at position 13 of 43 and **the linked executable is byte-identical across it** — a
linker does not pull an unreferenced archive member in. And inserting 4 096 / 9 712 / 16 384
bytes of unreachable `.text` at the head of the IPM's own TU at the parent moves the scored
corpus by +0.05 %, +0.19 % and +0.01 %, reproducing 1.5 %, 6.4 % and 0.3 % of the step. The
paragraph as published follows.

**What T8.4 did that moves code addresses** is in the same commit and is large. Against
`510a4bb` it rewrote `src/drivers/interior_point_solver.cpp` (1101 lines changed — the TU
that carries the IPM iteration loop) and **inserted a new translation unit**,
`drivers/solve_result.cpp`, into `src/CMakeLists.txt:76`, taking the library's source count
from 42 to 43. A new object in the archive and a rewritten hot TU relocate every object that
follows them, which is what a layout cluster transition is. The corroborating front-end
counters exist (cycles +1.71 % against instructions +1.01 %, IPC −0.69 %) but are **not quoted
as proof**: §4 shows the control arm moves those same counters further.

---

## 7. The solo evidence, per batch

**Sixteen timed batches, RE-AUDITED AT FIX2 UNDER R2' — the pinned-core rule with foreign `nice`
time counted. FOURTEEN ARE PROVEN, and the two that are not are both COUNTER batches**: `diff-r1`
at 0.6828 % on the pinned core and `perfB-r2` at 0.7559 % on its sibling (the second is astra's own
worked example — 0.11 s user + 0.09 s nice on `cpu10`, which the superseded `user`-only accounting
reported as 0.1120 % and passed). **The five WALL batches — the only ones asserting wall clock —
are all PROVEN**, core 0.0561–0.0837 % and sibling 0.3084–0.3363 %. The governing table is
`../logs/IDLE-PROOF.md`; this directory's `IDLE-PROOF.md` is the superseded `user`-only proof.

* **The five WALL batches, which are the only ones that assert wall clock, read foreign
  un-niced user time on the pinned core `cpu2` of EXACTLY ZERO — 0.0000 % — every one.** The
  SMT sibling `cpu10` reads 0.070–0.130 s (0.098–0.184 %), all under the 0.5 % bar.
* The eleven instruction batches read 0.000–0.340 s on `cpu2` (worst 0.414 %) and
  0.010–0.230 s on `cpu10` (worst 0.282 %).
* `system` beyond each run's own `sys` (0.03–0.69 s) and `irq`+`softirq` (0.12–0.40 s) are
  reported separately and not counted as foreign, on fix1's reasoning.

**Five batches were re-run, as R2 requires, and the re-runs are the retained data.**
On the first pass `wall-r1` failed on the SMT sibling (0.860 s, 1.2120 %); `diff-r4` failed
on `cpu2` (1.1045 %), `diff-r5` on `cpu10` (1.4463 %), and `perfB-r2` / `perfB-r3` on `cpu2`
(1.0166 % / 1.0886 %). All five were re-run under the same recipe and all five came back
clean. Each re-run overwrote its batch's log, CSVs and perf files, which is what "the batch
is re-run" means here; the superseded first pass is not retained separately.

**Test 1 (R2 as written) is NOT met, for fix1's reason, and the numbers are reported.**
Worst foreign CPU-time delta across any batch window: **3.20 %** (`diff-r1`), the desktop's
resident processes — 194–201 foreign pids in every snapshot. Five pauses were taken across
five batches in which a foreign process was seen in state `R`, each followed by the recorded
give-up that fix1 documents: on this box no amount of waiting reaches "no `R` in any
snapshot". **Nothing was ever signalled.** The pinned-core test above is what proves these
windows.

| batch | window (UTC) | rc |
|---|---|---|
| fold-proof fail / pass | 17:34:39 | **5** / **0** |
| `wall-r2` … `wall-r5` | 17:50:02 → 17:55:41 | 0 |
| `wall-r1` *(re-run)* | 17:56:08 → 17:57:26 | 0 |
| `perfA-r1` … `perfA-r3` | 17:57:40 → 18:01:44 | 0 |
| `diff-r1` … `diff-r3` | 18:06:00 → 18:10:59 | 0 |
| `perfB-r1` | 18:17:38 → 18:18:07 | 0 |
| `diff-r4`, `diff-r5` *(re-runs)* | 18:22:37 → 18:25:40 | 0 |
| `perfB-r2`, `perfB-r3` *(re-runs)* | 18:25:44 → 18:26:48 | 0 |

Every batch took `flock /tmp/box-build.lock` for its whole length, with the `pgrep` audit
taken **separately, outside the lock** and pasted verbatim into the log above a second
in-lock `pgrep`, then `FOREGROUND_START` / `FOREGROUND_END` inside. Every script sets
`-o pipefail`, folds every status worst-first, and ends `WRAPPER_EXIT=$rc; exit $rc`.

**The fold is proved, once, against a stub.** `raw/scripts/fold_proof.sh` sources the *same*
`common.sh` the timed legs source. In `fail` mode it stubs three failures — rc 2 through the
legs' own `timed_run` wrapper, rc 5 in the first stage of a pipe whose tail returns 0, and a
*later* rc 3 — with successful steps between them, and exits **5**: the worst, not the last,
and not cleared by a later success. In `pass` mode the identical path exits **0**. Both logs
are retained.

`common.sh` here is fix1's `../scripts/common.sh` with the two scratch paths repointed and
the `BOX_PAUSE_GIVEUP` message shortened, and **nothing else** —
`raw/scripts/common-vs-fix1.diff` is that diff, retained, so the claim that this leg
reproduces fix1's R2 discipline is checkable rather than asserted.

---

## 8. What this directory does not claim

* **It asserts no disposition.** Whether a reproducible 2.5 % wall band with identical
  counters and no resolvable instruction change is a regression the owner must act on is
  §11.1's question and the owner's.
* **It gives no instruction verdict on T8.4**, and §6's accounting is a derivation from an
  estimate whose assumption is stated, not a measurement.
* **It does not re-open §5.** Its corpus ratio agrees (1.0250 / 1.0249); its per-row columns
  differ by up to 2.9 points and it says so.
* **It measured nothing on Apple or Windows.** APPLE / ACCELERATE: **UNOBSERVED.**
  WINDOWS: **UNOBSERVED.**

## 9. Files

| file | what |
|---|---|
| `arms.txt` | the eleven arms, their re-verification, the measurement pin, the argv lock |
| `attribute_interior.py` (+ `.sha256`) | the whole analysis; emits `wall.csv` and `perf.csv` |
| `attribute_interior.out` | its output, saved |
| `wall.csv` | arm, task, sha, round, row index, row, `wall_s` — 1145 rows (every row of every wall process, scored or not) |
| `perf.csv` | leg, arm, task, sha, round, event, value |
| `IDLE-PROOF.md` | `../scripts/idle_proof.py` over all sixteen timed batches |
| `raw/wall/` | 55 CSVs + 55 stdouts (five rounds × eleven arms) |
| `raw/perfA/`, `raw/perfB/` | perf outputs, CSVs, stdouts |
| `raw/diff/` | the rejected instrument's P and Q processes, retained as data |
| `raw/logs/` | every batch log with its R2 brackets, and every separate `pgrep` |
| `raw/scripts/` | every script this leg ran, plus `common-vs-fix1.diff` |
