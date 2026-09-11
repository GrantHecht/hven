# W5 T8.9r — the group-1 runtime reading (v2, fix round 1)

`102f729 → e51a7e0` (the three SQP arms **and now the interior leg's base arm**)
and `b9848bf → e51a7e0` (the interior leg's 41-key arm), solo. Read
`PROVENANCE.txt` first — the arms, the hashes, the hardware, the solo protocol,
and what this leg does **not** measure — and then §0 below, which is the record
of what fix round 1 changed.

Every table is regenerated from `raw/` and `perf/` by
`python3 comparator.py --root . --out -`; the tool's own output is reproduced
verbatim at §9. Nothing here is hand-transcribed.

**This is a measurement, not a disposition.** §11.1.1 puts the disposition with
the settler and the owner, and **three** of the findings below need one.

---

## 0. Revision record — what fix round 1 changed, and why

This file is **v2**. It was rewritten in place on 2026-09-11 after astra's
review (`FIX-ROUND`, SIGNOFF `W5-T8-9R-REVIEW-ASTRA`: nine Important items and
a set of Minors) and the settler's seven rulings R1–R7. **Every wall number in
v2 comes from a re-measurement taken after those rulings were fixed**; v1's raw
captures are retained, unedited, under `raw/round1-superseded/` with a README
saying why, and its logs under `logs/round1-superseded/`.

| section | what changed | why |
|---|---|---|
| §1 | every number re-measured; the interior leg is now three arms | astra I1, I2; R2, R4 |
| §1, §2, §5 | the veto is classified on the BANDED reading, and the rule was fixed before the runs (`predeclaration-R1-R3.txt`, `logs/F0-predeclaration.log`) | astra I5 (e); R1 |
| §2 | "leg 1 — the only wall-clock leg" now names the banded interior F7 leg too | Minor |
| §4 | leg 2's floor is the MAXIMUM per-(mode, trace) same-arm disagreement, not a median across combinations | astra I5 (b) |
| §5 | the interior leg's first-row exclusion is POSITIONAL and pre-declared, both corpus figures print, and the primary is the one WITHOUT the warm-up row | astra I5 (a); R3 |
| §5 | the interior leg gains its 102f729 arm — the leg EXISTS there, 33 rows, 19 columns — so the top-level IPM across group 1 IS measured | astra I2; R4 |
| §5 | the per-cell interior instruction verdict (I4), differenced against the unconditional row set so it is like-for-like across arms | astra I4 |
| §5 | the HS movement is quoted against the reported 1.71–2.18 %, not "inside 1.4 %" | Minor |
| §6 | REWRITTEN: the `--dump-qp` probe is superseded; §10's eleven-arm attribution says what the increase is, and the source says where it runs | astra I8; R5 |
| §7 | "T7 was comment-only" replaced by the library-hash argument; "+327 lines" → "327 changed lines" | Minor |
| §8 | the attached-callback and attached-sink costs are stated UNMEASURED; the `jet.h` condition is stated as settled here, not only in PROVENANCE | R7; astra §5 |
| §8.1 | NEW: the solo proof, by CPU time on the pinned core | astra I1; R2 |
| §10 | unchanged, as landed at `e096f7f` | — |

**What did NOT change: the shape of the finding.** Leg 1's wall is FLAT and its
instructions are UP. The veto trigger fired in v1 and it fires in v2. What the
fix round changed is whether those numbers are entitled to be quoted, and what
the increase can be attributed to.

---

## 1. The short version

| leg | mode | corpus ratio | cells outside 0.99–1.01 | band | banded veto cells | instruction verdict |
|---|---|---|---|---|---|---|
| **leg 1** (U0, 27 cells, wall) | ipm | **1.00062** (14.8524 → 14.8617 s) | **0/27** | **FLAT** | 0 | **instructions UP +0.032…+0.046 %** |
| leg 1 | ssn | **0.99924** (11.0994 → 11.0909 s) | **0/27** | **FLAT** | 0 | **instructions UP +0.065…+0.087 %** |
| leg 1 | walk | **1.00014** (6.0245 → 6.0253 s) | **0/27** | **FLAT** | 0 | **instructions UP +0.092…+0.132 %** |
| **leg 2** (27 HS, `--repeat`) | ipm off / sink | 0.9913 / 0.9861 | 21/27, 26/27 | **MOVED (faster)** | 2 / 2 | no verdict at 1e-4 — §4 |
| leg 2 | ssn off / sink | 0.9822 / 0.9754 | 17/27, 23/27 | **MOVED (faster)** | 0 / 0 | no verdict at 1e-4 |
| leg 2 | walk off / sink | 0.9931 / 0.9761 | 8/27, 24/27 | **UNRESOLVED / MOVED** | 0 / 0 | no verdict at 1e-4 |
| **interior, `102f729` → head** (29 F7 rows, R3-primary) | — | **1.0249** (10.1030 → 10.3545 s) | **29/29** | **MOVED** | **28** | no verdict — §5 |
| **interior, `b9848bf` → head** (31 F7 rows, R3-primary) | — | **0.9948** (10.4830 → 10.4284 s) | 7/31 | **UNRESOLVED** | 1 | no verdict — §5 |

**Four things follow, and they are independent of one another.**

**(a) THE SQP CORPUS LEG'S WALL IS FLAT IN ALL THREE MODES.** 0/27 cells outside
0.99–1.01 in every mode, every corpus figure inside ±0.08 %, an order of
magnitude inside the ±0.5 % bar, and **zero banded veto cells** (R1). The strict
same-sign counts — 3, 3, 2 of 27 — are what a fair coin gives (~3.4 expected);
they are reported, and they are not the trigger.

**(b) AND INSTRUCTIONS ARE UP ANYWAY — WORK-MOVED, THE VETO.** On every one of
the nine leg-1 perf cells the head arm executes more instructions than the base,
by 0.03 % (ipm) to 0.13 % (walk) — 3× to 13× outside §11.1's 1e-4 identity band,
and 32× to 132× outside this leg's own measured noise floor (§4). §11.1.1's
OVERLAP 1 is explicit that a FLAT timing does not excuse this: *"does
demonstrated added work trigger the veto even when the timing meets FLAT?
ANSWER: YES. There is NO automatic KEEP."* **The veto trigger has fired.** §6
charges the whole of it to T8.4's per-call declared diagnostics and shows, from
the call chain, that they run inside the timed window.

**(c) AND THE TOP-LEVEL IPM IS 2.5 % SLOWER ACROSS GROUP 1, REPRODUCIBLY, DOING
IDENTICAL WORK. THIS IS NEW AT FIX1 AND IT IS THE LARGEST FINDING IN THE
DIRECTORY.** The interior leg exists at `102f729` (astra I2; round 1 said it did
not, and that was false), so group 1's effect on the top-level solver is now
measured rather than declared unmeasurable. On the 29 banded F7 rows the corpus
goes **10.1030 → 10.3545 s, +2.49 %**; **every one of the 29 rows is outside
0.99–1.01** and **28 of them are slower in all three alternating rounds** — the
banded reading R1 fixes, on 28 cells, not one. And the work is the same: across
33 common rows × 12 common counter columns × 3 rounds the **only** column that
differs is `status`, and it differs because T8.2 renamed the vocabulary
(`CONVERGED` → `optimal`). Same iterations, same factorizations, same solves,
same analyses, same residuals to the last printed digit — 2.5 % more wall.
**The regression is NOT in T8.9**: `b9848bf` → head is −0.52 % (UNRESOLVED,
1 banded veto cell), so it sits inside T8.1–T8.8.

**(d) THE INTERIOR LEG'S INSTRUCTION VERDICT REMAINS UNAVAILABLE, AND THE REASON
IS NOW MEASURED RATHER THAN ASSERTED.** I4's per-cell `perf stat` ran — 11 cells
× 3 arms × 3 rounds — and the differencing instrument built to make it
like-for-like is **unsound**: it produces negative differenced counts (§5). The
numbers are retained as data; no verdict is issued; the gap is registered.

---

## 2. Leg 1 — the wall, in the T6 close reading's shape

Recipe: the U0 27-cell corpus, `--engine walk|ssn|ipm`, 3× alternating A/B, solo,
`taskset -c 2` with the driving shell pinned off that core and its SMT sibling,
`MKL_NUM_THREADS=OMP_NUM_THREADS=1`, the lock held for each whole round-batch.
Per-cell median of the three runs; corpus = the sum of the per-cell medians.

| mode | corpus | per-cell envelope | outside 0.99–1.01 | slowest cell | fastest cell |
|---|---|---|---|---|---|
| ipm | **1.00062** (14.8524 → 14.8617 s) | 0.9929–1.0083 | **0/27** | `f7_n1000_bound_activity` 1.0083 | `f7_n1000_bound_neutral` 0.9929 |
| ssn | **0.99924** (11.0994 → 11.0909 s) | 0.9906–1.0057 | **0/27** | `f7_n10000_bound_warm` 1.0057 | `f7_n2000_bound_corrupted` 0.9906 |
| walk | **1.00014** (6.0245 → 6.0253 s) | 0.9932–1.0043 | **0/27** | `f7_n5000_bound_corrupted` 1.0043 | `f7_n1000_bound_physics` 0.9932 |

**NO CELL IS OUTSIDE 0.99–1.01 IN ANY MODE.** The full per-cell tables, with the
three paired per-round ratios beside every median, are in §9.

**AND THE COUNTERS ARE BYTE-IDENTICAL.** The comparator recomputes it from
`raw/`, so the reading does not take it on trust from another task's replay:
**27 cells × 75 columns × 3 rounds × 3 modes, `wall_s` excluded, ZERO
differences** — same factorizations, same QP minors, same escapes, same KKT
residuals to the last printed digit. That is what makes (b) a statement about
how the work is executed rather than about how much of it there is.

**Leg 1 is not the only banded leg.** The interior F7 rows carry the band too,
under the same leg-1 rules (§5); what leg 1 is, per §11.3, is the leg whose
cells are seconds-scale and whose ABSOLUTE wall is therefore quotable. §11.3 (ii)
reads leg 2's wall only as the paired A/B ratio.

### The veto check on leg 1 (R1)

| mode | banded: outside 0.99–1.01 AND slower in all three | strict, informational |
|---|---|---|
| ipm | **0** | 3 — `f7_n1000_bound_activity`, `f7_n10000_bound_warm`, `f7_n20000_bound_warm` |
| ssn | **0** | 3 — `f7_n5000_bound_corrupted`, `f7_n10000_bound_warm`, `f7_n800_path_warm` |
| walk | **0** | 2 — `f7_n2000_bound_corrupted`, `f7_n5000_bound_corrupted` |

R1, fixed before the runs (`predeclaration-R1-R3.txt`): the classification is the
banded count. **The wall side of the veto is clean on leg 1. The instruction side
is not, and the interior leg's is not.**

---

## 3. Leg 1 — pass A and pass B, and the instruction finding

| mode | cell | instructions | branches | cycles | L1-icache misses | verdict |
|---|---|---|---|---|---|---|
| ipm | n1000 | **1.00046** | 1.00050 | 0.99689 | 0.754 | WORK-MOVED |
| ipm | n5000 | **1.00032** | 1.00018 | 0.99823 | 0.785 | WORK-MOVED |
| ipm | n20000 | **1.00043** | 1.00046 | 1.00011 | 0.781 | WORK-MOVED |
| ssn | n1000 | **1.00087** | 1.00111 | 0.99967 | 0.732 | WORK-MOVED |
| ssn | n5000 | **1.00065** | 1.00068 | 1.00009 | 0.698 | WORK-MOVED |
| ssn | n20000 | **1.00071** | 1.00091 | 1.00149 | 0.685 | WORK-MOVED |
| walk | n1000 | **1.00132** | 1.00144 | 0.99906 | 0.899 | WORK-MOVED |
| walk | n5000 | **1.00127** | 1.00140 | 0.99864 | 0.917 | WORK-MOVED |
| walk | n20000 | **1.00092** | 1.00065 | 0.99728 | 0.912 | WORK-MOVED |

Branches move with instructions, by the same fraction, in every cell — so this is
executed work, not a mis-attributed counter. Cycles stay inside ±0.32 % and
L1-icache misses fall by 8–31 %: the extra instructions are cheap ones the front
end absorbs, which is why the wall does not see them. **That explains why the
timing is FLAT. It is not a defence against the veto, which §11.1 states on
instructions.** Pass B changes no classification; the full tables are in §9.

### The absolute deltas — the increase SCALES with the problem

| mode | cell | base instructions | head instructions | delta | ratio |
|---|---|---|---|---|---|
| ipm | n1000 | 749 122 644 | 749 466 678 | +344 034 | 1.00046 |
| ipm | n5000 | 3 834 878 023 | 3 836 104 607 | +1 226 584 | 1.00032 |
| ipm | n20000 | 15 958 318 794 | 15 965 123 025 | +6 804 231 | 1.00043 |
| ssn | n1000 | 488 479 834 | 488 903 314 | +423 480 | 1.00087 |
| ssn | n5000 | 2 629 037 411 | 2 630 743 395 | +1 705 984 | 1.00065 |
| ssn | n20000 | 11 674 701 748 | 11 683 014 698 | +8 312 950 | 1.00071 |
| walk | n1000 | 261 301 248 | 261 644 867 | +343 619 | 1.00132 |
| walk | n5000 | 1 333 971 634 | 1 335 665 712 | +1 694 078 | 1.00127 |
| walk | n20000 | 5 640 392 312 | 5 645 600 279 | +5 207 967 | 1.00092 |

The delta is not a constant per process: it grows roughly twentyfold as the
problem does. Whatever it is, it is charged per unit of work, not once at
startup. §6 says what it is.

---

## 4. The instrument's own floor — and which legs may speak about instructions

`instructions` and `cycles` appear in BOTH passes precisely so the two can be
tied to each other (§11.2). That tie is also a control: **when the same arm's two
passes of the SAME binary disagree by more than 1e-4, the identity band sits
below that population's noise floor and no instruction verdict at 1e-4 is
available from it.**

| population | worst same-arm pass A / pass B disagreement | verdict at 1e-4? |
|---|---|---|
| **leg 1** (nine cells, both arms, scored per cell) | **0.0010 %** | **YES** |
| leg 2 (six mode × trace combinations, the MAXIMUM) | **0.2452 %** | **NO** |
| interior, whole process | **0.2030 %** | **NO** |

**THE LEG-2 FLOOR IS THE MAXIMUM PER-(mode, trace) DISAGREEMENT, NOT A MEDIAN
ACROSS COMBINATIONS** (astra I5 (b)). Round 1 printed 0.0793 % by taking medians
across different modes, traces and repeat counts — an average of instruments
measuring different workloads, which is not a noise floor of anything. The
per-combination figures are `ipm/off` 0.158309 %, `ipm/sink` 0.158621 %,
`ssn/off` 0.153612 %, `ssn/sink` 0.152999 %, `walk/off` 0.011700 %, `walk/sink`
**0.245198 %**; the floor is the last of these. **`walk/off`'s 0.0117 % is not
promoted on its own** — the HS population is bimodal per process (T6.d finding
F1), and one combination landing in the same cluster on both passes is a
coincidence of clustering, not a sound instrument.

Leg 1's instrument reproduces to 1e-5 — ten times finer than the band it is
asked about — and the head-vs-base signal is 32× to 132× larger than that floor.
**Leg 1's instruction finding is real.** Leg 2's and the interior leg's
whole-process counts are inside their own noise; the comparator labels them
`NO VERDICT AT 1e-4` and prints the ratio it would otherwise have classified.
**The gate now applies to pass B as well as pass A** (astra I5 (c)): a population
that cannot carry a verdict on one pass cannot carry one on the other, and the
interior leg's whole-process counts carry none on either pass for a second and
stronger reason — the arms' processes do not run the same rows.

---

## 5. Leg 2 and the interior leg — what they do and do not say

### Leg 2 (the 27 HS problems)

All six mode × trace combinations moved in the **FASTER** direction, from
−0.69 % (`walk`/off) to −2.46 % (`ssn`/sink), with 8–26 of 27 cells outside
0.99–1.01. Five read MOVED; `walk`/off reads UNRESOLVED.

Two caveats, both written down before this leg ran:

* §11.3 (ii) records that three runs of the SAME binary disagree by up to
  **1.4 %** at the walk corpus level on this leg, because at 0.3–3 ms the
  between-run variation is a per-process constant — address layout, allocator
  state, page placement — that `--repeat` cannot average away at any N.
  **FOUR of the six combinations moved by MORE than that 1.4 %** (`ipm`/sink
  1.39 %, `ssn`/off 1.78 %, `ssn`/sink 2.46 %, `walk`/sink 2.39 %), so the
  movement is not simply inside the declared limit and round 1's sentences
  saying it was are withdrawn. What the limit does establish is that **leg 2's
  absolute wall figure cannot resolve the 0.5 % effect the veto turns on**, and
  §11.3 (ii) reads its wall only as the paired A/B ratio, never as a magnitude.
* What is *not* inside that caveat is the SIGN. All six moved the same way.
  Six same-direction readings is more than one instrument's noise, and it is
  recorded as such: on the HS scale the head is plausibly genuinely faster. It
  is not quoted as a magnitude, and the disposition of a FASTER MOVED band is
  §11.1.1's ("by the current wording, this includes the faster side").

Leg 2's verdict was to rest on instructions and branches (§11.3 (ii)). It cannot:
§4 shows this leg's own noise floor is **0.2452 %**, twenty-four times the band.
**So leg 2 returns no verdict in either currency.** Both mapper call sites were
exercised — the `trace=sink` arm carries a real sink and is reported separately
from the null-sink arm, never averaged with it.

The per-cell veto check on leg 2 (R1, banded): `ipm`/off and `ipm`/sink each
carry **2** — `hs12` and `hs15`; the other four combinations carry none.

**And the calibration's exceedances are not hidden.** N was adopted on the worst
per-cell `median_se_pct` of the calibrating run (ipm 0.354782 %, walk 0.419009 %,
ssn 0.390189 %); later runs of the same cells at the same N do exceed 0.5 % —
round 1 saw 1.027244 % and 1.739174 % on individual HS rows. That is the same
per-process bimodality, and **N cannot be raised to fix it**. `calibration/README.md`
records it in full.

### The interior leg — THREE arms (R4)

**Round 1's premise was false and is withdrawn.** It said "the interior leg DOES
NOT EXIST at 102f729". It does: `git merge-base --is-ancestor 149f29b 102f729`
returns 0, the base arm's extraction carries the `--engine interior` dispatch,
and run at fix1 it writes **33 rows in the 19-column schema**. The leg therefore
has three arms, and the one that matters most is the one round 1 declared
impossible.

| arm | commit | rows | schema | what it measures |
|---|---|---|---|---|
| `arm102` | `102f729` | 33 | 19 columns | **the top-level IPM across the WHOLE of group 1** |
| `armb98` | `b9848bf` | 41 | 31 columns | T8.9's harness rewrite alone (round 1's only arm) |
| `head` | `e51a7e0` | 43 | 31 columns | — |

**(i) `102f729` → head: MOVED, +2.49 %, 29 of 29 rows outside the band, 28
reproducible slowdowns.**

corpus **10.1030 → 10.3545 s, ratio 1.0249**, on the 29 F7 rows the R3 rule
leaves banded. Per-row ratios run 1.0112 to 1.0500; the only row inside
0.99–1.01 is none of them. Twenty-eight are slower in all three alternating
rounds — the banded reading R1 fixes. This is not one cell and it is not noise:
it is the whole population moving one way by 2–5 %.

**AND THE WORK IS IDENTICAL.** Across **33 common rows × 12 common counter
columns × 3 rounds** the only column that differs is `status`, and it differs
because T8.2 replaced `ConvergenceFlags` with `SolveStatus`: every one of the 33
reads `CONVERGED` at `102f729` and `optimal` at the head. `iter_num`, `obj_val`,
`kkt_inf`, `barr_inf`, `econ_inf`, `icon_inf`, `factorizations`, `solves`,
`analyses`, `soc_steps` and `watchdog_activations` are **identical**. Same
iterations, same factorizations, same residuals — 2.5 % more wall.

**And the timed span is the same span.** At `102f729` the leg's clock brackets
`ipm.optimize(x0)` with `transcribe()` above it
(`arm-base/bench/ipm_corpus_leg.cpp:160–164`); at the head it brackets
`ipm.solve(program, x0)` with the transcription above it
(`bench/ipm_corpus_leg.cpp:652–655`). Each arm runs its own harness source
(A7 (i)) and both executables' sha256 are in `PROVENANCE.txt`.

**(ii) `b9848bf` → head: UNRESOLVED, −0.52 %, 7 of 31 rows outside, 1 banded veto
cell** (`f7_n1000_bound_physics/RelaxBounds`, 1.0237). **So the regression is not
T8.9's.** It sits in T8.1–T8.8, and this artifact does not resolve further than
that: it has no arm between them.

**(iii) R3 — the first row, excluded by the pre-declared rule.**
`f7_n1000_bound_neutral/MakeParameter` is the first row every `--engine interior`
process writes and is excluded from the band **by position**, decided before the
round's first sample existed (`predeclaration-R1-R3.txt`, hashed in
`logs/F0-predeclaration.log` at 15:22:25 UTC, before the first
`FOREGROUND_START` at 15:33:07 UTC). Both corpus figures print:

| arm | PRIMARY (without the warm-up row) | with it |
|---|---|---|
| `102f729` → head | **1.0249** (+2.490 %), 29 rows, 29 outside, **MOVED** | 0.9759 (−2.409 %), 30 rows, 30 outside, MOVED |
| `b9848bf` → head | **0.9948** (−0.520 %), 31 rows, 7 outside, **UNRESOLVED** | 0.9743 (−2.566 %), 32 rows, 8 outside, MOVED |

The excluded row's own three paired ratios are 0.8306 / 0.4847 / 0.4792 against
`102f729` and 0.6999 / 0.6771 / 0.7021 against `b9848bf`; the `102f729` arm's own
three runs of it span **1.656×**. It is the process's warm-up and it dominates
whichever way it is counted — which is exactly why the rule was fixed in advance
rather than after looking.

**(iv) NO INSTRUCTION VERDICT, and at fix1 the reason is measured.**

The whole-process counts carry none: the arms' processes do not run the same
rows (33 / 41 / 43), and `perf stat` counts the process. Both passes are
suppressed (astra I5 (c)).

I4's per-cell measurement ran — **11 cells × 3 arms × 3 rounds**, each its own
`--engine interior --cells <id>` process, pass A, R2 discipline. The dispatch's
premise that this is like-for-like because the `parts2` rows belong to
`f7_n1000_bound_neutral` only is **FALSE, verified**: the two `parts2` rows run
in EVERY head process whichever cell is requested (6 / 14 / 16 rows for
`--cells f7_n1000_bound_physics` at the three arms). The instrument built to fix
that — difference each cell process against a `--cells hs071_x1_fixed` process,
which is exactly the unconditional set — is **unsound, and the comparator says so
and refuses the verdict**: nine of the differenced quantities come out NEGATIVE,
a cell process with fewer instructions than the row set it contains. The cause is
that the unconditional set is the process's FIRST solve when run alone and pays
MKL's first call and the allocator's first growth, which inside a cell process a
large F7 solve has already paid. The deeper cause is worse: the head's 13-row
unconditional set costs FEWER instructions (5 164 860 443) than `b9848bf`'s
11-row set (5 425 387 936), so the term being subtracted is not the same quantity
across the arms.

**A7 (ii)'s per-row instructions-only reading for the interior leg is NOT
AVAILABLE from this harness, by either route.** The measurements are retained in
`raw/interior_cells/` and `perf/interior_cells/` as data; the gap is REGISTERED
for W6 with the rest of A7. `hs071_x1_fixed` and the two `infeas2` rows are a
special case of the same thing — they ARE the unconditional set, so there is
nothing to difference them against.

---
## 6. Attribution — where leg 1's extra instructions are, and whether they are inside the solve

**This section was rewritten at fix1.** Round 1 answered the question with a
`--dump-qp` probe and concluded that "the entire increase is inside the solve".
astra's I8 is right that the probe cannot carry that: it does not execute the
post-solve KKT gate, the result processing, or the same neutral-path driver
construction, so what it bounds is narrower than what was claimed from it. The
probe's captures are retained under `raw/round1-superseded/perf/attribution/`
and are cited nowhere in this reading. Settler ruling R5 replaces it with two
things that do carry the question: §10's eleven-arm attribution for WHAT the
increase is, and the SOURCE for WHERE it runs.

**What it is: T8.4's per-call declared diagnostics, and nothing else.** §10
re-measured §3's nine cells at eleven arms — the base and every group-1 code
head in order. Every task's step is inside ±2e-5 except T8.4's, and T8.4's step
IS the whole increase on all nine cells (the steps sum to the end-to-end delta
to better than 1e-9). It is **one fixed `O(n)` cost per call** — ≈+341 K
instructions at n = 1000, ≈1.688 M at n = 5000, ≈6.73 M at n = 20000, linear in
`n` to better than 2 % and the same on all three QP tiers to better than 0.5 %.
§3's "+0.03 %…+0.13 %" is that one quantity divided by three different solve
totals, largest as a fraction on `walk` because `walk` is the cheapest solve.

**Where it runs — read off the source, not inferred from a probe.** The
mechanism T8.4's design §2.3 and §2.7 (7) declare is the shared declared
diagnostics computed once per call over the declared NLP
(`compute_declared_diagnostics` / `compute_declared_diagnostics_from_grad_lag`)
plus the declared-space vectors the new `SolveResult` carries by value. Both
engines' call sites, and both harnesses' clocks:

| what | where | inside the timed bracket? |
|---|---|---|
| the SQP's timed bracket | `bench/corpus_cells.h:1647–1649` — `t0`, `solve_target()`, `t1`; `record_kkt_check` is at `:1651`, AFTER `t1` | — |
| SQP, per major | `src/drivers/sqp_driver.cpp:5123` `st.mb.diag = stash_declared_diagnostics(...)`, in `SqpDriver::run_major` (`:4642`), reached from `solve_impl_body` (`:5786`) from `SqpDriver::solve` (`:2463`) | **YES** |
| SQP, restoration | `src/drivers/sqp_driver.cpp:4485`, same call chain | **YES** |
| SQP, every exit | `src/drivers/sqp_driver.cpp:5920, 5958, 5991, 6014` in `solve_impl_body` | **YES** |
| SQP, the caller-scale four | `src/drivers/sqp_driver.cpp:6418` `compute_declared_diagnostics(...)` in `SqpDriver::finish` (`:6264`), called from `solve_impl_body`'s returns at `:5911, 5935, 5951, 5969, 6001` | **YES** |
| SQP, the callback's per-event four | `src/drivers/sqp_driver.cpp:4732`, inside `run_major`'s **callback-attached** branch (`:4736`'s `else if` is the no-callback arm, and `:4738` says a solve with no callback copies nothing) | inside the bracket, but **NOT EXECUTED** by these legs — no callback is attached |
| the interior leg's timed bracket | `bench/ipm_corpus_leg.cpp:652–655` — `t0`, `ipm.solve(...)`, `wall_s`; the transcription runs above, before `t0` | — |
| IPM, per solve | `src/drivers/interior_point_solver.cpp:6039` `declared_diagnostics_from_reduced_grad_lag(...)` in `run_phase_sequence` (`:5041`), called from `InteriorPointSolver::solve` (`:6104`, `:6158`) | **YES** |
| IPM, the callback's | `src/drivers/interior_point_solver.cpp:2358`, inside `fire_iteration_event` (`:2249`) | inside the bracket, but **NOT EXECUTED** — the leg attaches no callback (`HVEN_LEG_COUNT_CALLBACK` is unset, `bench/ipm_corpus_leg.cpp:638`) |

So the answer is **yes, it executes inside `wall_s`'s bracket**, on both engines,
by the call chain rather than by inference; and the one site that would NOT run
in these legs is the callback-attached one, which is exactly the cost this
artifact does not measure (§8, R7).

Combined with the byte-identical counters of §2 — same factorizations, same
minors, same iterations — the finding is: **the head arm performs the same
solve, in the same number of steps, executing one extra `O(n)` diagnostic
quantity per call inside the timed window, and the front end absorbs it so the
wall does not move.** Whether that is a cost worth paying is §11.1.1's question
and the owner's.

## 7. The composed cumulative chain

§11.1's cumulative bar does not compose itself: two in-band readings can compose
out of band, so the arithmetic is stated. There is no third arm and none is
implied — this is multiplication of two measured ratios.

| mode | T6 close 50f616a → 1997159 | T8.9r 102f729 → e51a7e0 | composed | inside ±0.5 %? |
|---|---|---|---|---|
| ipm | 1.0023 | 1.000625 | **1.00293** (+0.293 %) | **yes** |
| ssn | 1.0015 | 0.999236 | **1.00074** (+0.074 %) | **yes** |
| walk | 0.9993 | 1.000137 | **0.99944** (−0.056 %) | **yes** |

**And the chain has no unmeasured code gap.** T7 sits between `1997159` and
`102f729`. The argument is not that T7 was comment-only — it was not; it moved a
dead-friend token and T8.1 landed interleaved with it. The argument is the
LIBRARY HASH: `arm-base`'s `libhven.a` sha256 begins `735eea1d9d51ef93`, the
sha16 the T6 close record gives for the post-(d) revert arm's library. **The
library at `102f729` IS the library at the T6 close head**, so the two measured
spans meet end to end whatever happened to comments in between.

**The composed chain is an SQP-corpus chain.** The interior leg's `102f729` arm
is NOT in it: it measures the top-level IPM, which the T6 close did not measure,
so there is nothing to compose it with. Its +2.49 % stands on its own and is
reported as its own finding (§1 (c)).

---
## 8. What this reading claims, and what it does not

**Claims, with the numbers above:**

* Leg 1 is FLAT in all three modes, 0/27 cells outside 0.99–1.01, on
  re-measurement under R2's CPU-time solo proof.
* Leg 1's counters are byte-identical between the arms across 75 columns, 27
  cells, three rounds, three modes.
* Leg 1's instructions are UP, reproducibly and far above that leg's measured
  noise floor. **This is WORK-MOVED under §11.1 and the veto trigger has
  fired.** §10 charges the whole of it to T8.4's per-call declared diagnostics,
  and §6 shows by call chain that they run inside the timed bracket.
* **The top-level IPM is 2.49 % slower at the group-1 head than at `102f729` on
  the 29 banded F7 rows, with 29 of 29 rows outside 0.99–1.01 and 28 slower in
  all three rounds, while every common counter column is identical.** That band
  is MOVED and those 28 cells are reproducible slowdowns on R1's banded
  reading. It is not in T8.9: `b9848bf` → head reads −0.52 %.
* The interior leg has a base arm at `102f729` as well as at `b9848bf`, so the
  top-level IPM's runtime across the WHOLE of group 1 is measured on the 33
  base rows — the limitation round 1 declared was false (R4).
* Leg 2 moved in the FASTER direction in all six combinations. Four of the six
  moved by more than §11.3 (ii)'s 1.4 % same-binary limit, so the magnitude is
  NOT inside that limit — but the limit is still what says the magnitude cannot
  be resolved, so it is not quoted as a measurement either way.

**Does not claim:**

* Any disposition. §11.1.1 sends "demonstrated instructions UP" to the owner
  with the numbers; §11.5 owns the outcome. This directory stops at the numbers.
* **THE COST OF AN ATTACHED CALLBACK OR AN ATTACHED TRACE SINK ON THE INTERIOR
  LEG. UNMEASURED (settler ruling R7.)** A7 (iii) named a callback lever for
  the interior leg; the harness has none. `bench/bench_corpus.cpp` carries
  `--repeat`, `--internal-run-one` and `--internal-out`, and no callback option;
  `bench/ipm_corpus_leg.cpp:638` installs a counting callback only when
  `HVEN_LEG_COUNT_CALLBACK` is set in the environment, and that lever counts
  events — it is not an A/B pair and was not used here. T8.6's callback and
  T8.7's sink therefore have NO measured attached-cost number in this artifact,
  and one is not inferred from the detached numbers: §6's table shows two call
  sites (`sqp_driver.cpp:4732`, `interior_point_solver.cpp:2358`) that run ONLY
  with something attached and that these legs execute zero times. The settler
  has REGISTERED the measurement for W6. No harness lever was added for it here
  — adding one would be a bench source change, which this leg may not make.
* Any wall reading from leg 2 as a magnitude, and **any instruction verdict for
  the interior leg at all** — neither from its whole-process counts (unmatched
  row sets) nor from I4's per-cell measurement, whose differencing instrument is
  unsound and is refused by the comparator rather than published (§5 (iv)).
  A7 (ii) is UNSATISFIED for the interior leg and the gap is registered for W6.
* **Any cause for the +2.49 %.** It is located to T8.1–T8.8 and no further; this
  artifact has no arm between those heads and no instruction evidence that could
  attribute it. Attributing it is the obvious next measurement and it is the
  settler's and the owner's to commission.
* Any Apple/Accelerate or Windows value, and any Intel pass-B value. All
  **UNOBSERVED**.
* **That the box was solo in R2's per-process sense.** It was not, and no run
  on this machine can be: a Wayland compositor, a browser and this agent's own
  daemon are resident and accrue CPU continuously. What IS proved, per timed
  run, is that the PINNED CORE and its SMT sibling ran no foreign task — see
  `logs/IDLE-PROOF.md` and §8.1.

---

### 8.1 The solo proof — by CPU time, on the pinned core

**Round 1's solo proof was `pgrep`, and astra's I1 refused it.** A command name
cannot say whether a process ran; the checks were taken before rounds rather
than between alternations; and `box_pgrep` printed matches and continued — it
implemented no pause. Worse, the audit was incomplete on its own terms: the
pattern `cmake|ninja|ctest|hven_|codex|clang` matches neither `kwin_wayland`
nor `firefox` nor this agent's own daemon, and all three were running the whole
time. Settler ruling R2 replaced it. The full per-batch table is
`logs/IDLE-PROOF.md`, computed by `scripts/idle_proof.py` from the batch logs.

**Test 1 — R2 as written, and it is NOT met.** Every foreign process (everything
but this agent's own process tree and the kernel threads) is snapshotted at the
start and end of every timed batch and between every A/B alternation, with each
pid's `utime+stime` in clock ticks beside the verbatim `ps` row. R2's bar is
every foreign pid under 0.5 % of the window's wall with no `R` state seen.
**On this box that bar is unreachable and re-running cannot reach it**: the
desktop's resident processes — the Wayland compositor, the browser, the agent's
own daemon — accrue CPU continuously across 16 logical CPUs. Every occurrence
paused the batch and was recorded; nothing was ever signalled. The numbers are
reported, not worked around.

**Test 2 — the pinned core, which is what the recipe actually reserves.**
`/proc/stat` accounts every jiffy per logical CPU. Around **each timed run** —
not around the batch, so none of this agent's own analysis falls inside the
bracket — the leg reads `cpu2` and `cpu10` and the run's own `user+sys` from
`/usr/bin/time`:

* **foreign TASK time on `cpu2`** = `cpu2`'s `user+nice+system+steal+guest`
  minus the run's own `user+sys`;
* **SMT contention** = `cpu10`'s task time (`cpu10` is `cpu2`'s thread sibling).

`irq`/`softirq` are reported separately and are not counted as foreign: they are
the kernel servicing the machine on that core — timer ticks, the measured
process's own page-fault path — not another runnable thing, and they are in
every measurement this protocol has taken, T6's included.

**And the driving shell is pinned OFF `cpu2` and `cpu10`** (`taskset -cp` with
mask `0,1,3-9,11-15` on the leg script's own pid). Round 1 pinned the solve and
left the harness — the shell, `ps`, `awk`, `perf`'s setup, the timestamps —
free to land on the measurement core, so "cpu2 ran nothing but the solve" was
not true even of the leg's own scaffold.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's
shell runs niced, so the solve's user time lands in `/proc/stat`'s `nice`
bucket; every foreign user task on this box — the compositor, the browser, the
daemons — is UN-niced and lands in `user`. **The `user` delta on the pinned core
is therefore foreign user-task time with nothing of the measurement in it and
nothing subtracted.**

**THE RESULT: 30 of 30 timed batches PINNED-CLEAN, and `scripts/idle_proof.py`
exits 0.**

* **Foreign un-niced user time on `cpu2` is EXACTLY ZERO on 28 of the 30
  batches** — including all nine leg-1 wall batches, all twelve leg-2 batches,
  all three leg-1 perf batches and the interior wall batch, which is every
  batch that asserts a wall number.
* The worst any batch shows is **0.440 s / 0.4019 %** (`interior-perfA`), under
  the 0.5 % bar; the worst SMT-sibling figure is **0.280 s / 0.2558 %** on the
  same batch.
* `system` beyond the run's own `sys` (0.00–1.38 s per batch) and
  `irq`+`softirq` (0.06–2.48 s) are reported separately and are not counted as
  foreign: they scale with the NUMBER OF PROCESSES a batch launches, not with
  its wall, which is what kernel-side process creation, PMU programming and CSV
  writeback look like.

**FIVE BATCHES WERE RE-RUN, as R2 requires, and the re-runs are the retained
data.** On the first pass `leg1-walk-r2` failed on the SMT sibling (5.82 s of
foreign un-niced user time on `cpu10`, 1.39 %), `interior-perfB` and
`interior-cells-r1/r2/r3` failed on `cpu2`. All five were re-run under the same
recipe; `interior-cells-r1` and `-r2` needed a second re-run. **Nothing was ever
signalled.** The superseded first-pass logs are not retained separately — each
re-run overwrote its batch's log, raw and perf files, which is what "the batch
is re-run" means.

**Test 1's numbers, reported because R2 asks for them.** Worst foreign CPU-time
delta across any batch window: **1.420 s, 2.9418 % of the window** — pid 50122,
`kwin_wayland`, the desktop compositor. Twelve pauses were taken across five
batches in which a foreign process was seen in state `R`. None of it landed on
the pinned core.


---

## 9. The comparator's output, verbatim

Everything above is derived from what follows; what follows is derived from
`raw/` and `perf/` and nothing else. Regenerate with
`python3 comparator.py --root . --out -` (sha256 in `comparator.py.sha256`).

# W5 T8.9r comparator output (fix round 1)

Regenerate: `python3 comparator.py --root . --out -`

R1 (the banded veto reading) and R3 (the interior first-row rule) are PRE-DECLARED in this file's docstring and in `PROVENANCE.txt`; `logs/F0-predeclaration.log` records that they were fixed before the first timed run of this round.

## Leg 1 -- the U0 27-cell corpus (102f729 -> e51a7e0)

### leg 1 / ipm

corpus base 14.8524 s -> head 14.8617 s, **ratio 1.0006** (+0.062 %); cells compared 27; outside 0.99-1.01: **0**; band **FLAT**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 0 **; strict, informational, 3 ['f7_n10000_bound_warm', 'f7_n1000_bound_activity', 'f7_n20000_bound_warm']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `f7_n10000_bound_activity` | 0.677230 | 0.676187 | 0.9985 | 0.9983 1.0066 0.9955 |
| `f7_n10000_bound_corrupted` | 0.771408 | 0.772228 | 1.0011 | 0.9988 1.0045 1.0000 |
| `f7_n10000_bound_neutral` | 0.830061 | 0.829918 | 0.9998 | 0.9996 1.0031 0.9973 |
| `f7_n10000_bound_physics` | 0.677521 | 0.677520 | 1.0000 | 0.9969 1.0016 0.9992 |
| `f7_n10000_bound_warm` | 0.770478 | 0.774307 | 1.0050 | 1.0050 1.0062 1.0002 |
| `f7_n1000_bound_activity` | 0.064256 | 0.064791 | 1.0083 | 1.0002 1.0083 1.0134 |
| `f7_n1000_bound_corrupted` | 0.069044 | 0.068792 | 0.9964 | 0.9720 0.9984 0.9965 |
| `f7_n1000_bound_neutral` | 0.081621 | 0.081043 | 0.9929 | 0.9916 1.0018 0.9929 |
| `f7_n1000_bound_physics` | 0.065360 | 0.065137 | 0.9966 | 1.0043 0.9965 0.9984 |
| `f7_n1000_bound_warm` | 0.068570 | 0.068559 | 0.9998 | 0.9993 1.0013 0.9850 |
| `f7_n1000_path_warm` | 0.267771 | 0.267955 | 1.0007 | 1.0007 0.9978 1.0031 |
| `f7_n20000_bound_activity` | 1.413533 | 1.413451 | 0.9999 | 1.0017 0.9983 1.0019 |
| `f7_n20000_bound_corrupted` | 1.615827 | 1.615828 | 1.0000 | 0.9983 1.0028 0.9992 |
| `f7_n20000_bound_neutral` | 1.732945 | 1.730225 | 0.9984 | 1.0016 1.0026 0.9976 |
| `f7_n20000_bound_physics` | 1.411426 | 1.411360 | 1.0000 | 0.9998 1.0053 0.9995 |
| `f7_n20000_bound_warm` | 1.616200 | 1.627830 | 1.0072 | 1.0158 1.0048 1.0036 |
| `f7_n2000_bound_activity` | 0.131101 | 0.131126 | 1.0002 | 0.9926 1.0067 1.0002 |
| `f7_n2000_bound_corrupted` | 0.140191 | 0.140143 | 0.9997 | 0.9985 1.0006 0.9958 |
| `f7_n2000_bound_neutral` | 0.162214 | 0.162061 | 0.9991 | 0.9996 1.0005 0.9944 |
| `f7_n2000_bound_physics` | 0.131132 | 0.130920 | 0.9984 | 0.9970 0.9968 1.0001 |
| `f7_n2000_bound_warm` | 0.139883 | 0.140017 | 1.0010 | 1.0014 1.0035 0.9961 |
| `f7_n5000_bound_activity` | 0.329362 | 0.328655 | 0.9979 | 0.9793 0.9979 0.9977 |
| `f7_n5000_bound_corrupted` | 0.374401 | 0.373296 | 0.9970 | 0.9975 1.0033 0.9970 |
| `f7_n5000_bound_neutral` | 0.405251 | 0.404813 | 0.9989 | 1.0133 0.9986 1.0001 |
| `f7_n5000_bound_physics` | 0.328671 | 0.329233 | 1.0017 | 0.9970 1.0065 0.9997 |
| `f7_n5000_bound_warm` | 0.354655 | 0.353749 | 0.9974 | 0.9917 1.0015 0.9974 |
| `f7_n800_path_warm` | 0.222303 | 0.222544 | 1.0011 | 1.0051 1.0011 0.9993 |

pass A, cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 75899 | 57250 | 0.75429 |
| `branch-misses:u` | 550079 | 534476 | 0.97163 |
| `branches:u` | 112844047 | 112899985 | 1.00050 |
| `cycles:u` | 242141477 | 241387885 | 0.99689 |
| `instructions:u` | 749122644 | 749466678 | 1.00046 |

pass B (Zen 3 front end), cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 242612286 | 241660217 | 0.99608 |
| `de_dis_uop_queue_empty_di0:u` | 9323027 | 8996860 | 0.96501 |
| `ic_fetch_stall.ic_stall_any:u` | 102715497 | 101794224 | 0.99103 |
| `instructions:u` | 749117947 | 749470766 | 1.00047 |
| `op_cache_hit_miss.op_cache_hit:u` | 106768207 | 107498631 | 1.00684 |
| `op_cache_hit_miss.op_cache_miss:u` | 11366723 | 10959509 | 0.96417 |
base arm: front-end-bound (dq-empty/cycles) = 0.0384; op-cache miss share = 0.0962
head arm: front-end-bound (dq-empty/cycles) = 0.0372; op-cache miss share = 0.0925

pass A, cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 1179259 | 920622 | 0.78068 |
| `branch-misses:u` | 8379334 | 8275128 | 0.98756 |
| `branches:u` | 2405655225 | 2406773117 | 1.00046 |
| `cycles:u` | 5255497812 | 5256059911 | 1.00011 |
| `instructions:u` | 15958318794 | 15965123025 | 1.00043 |

pass B (Zen 3 front end), cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 5257571745 | 5260249587 | 1.00051 |
| `de_dis_uop_queue_empty_di0:u` | 133691979 | 142374460 | 1.06494 |
| `ic_fetch_stall.ic_stall_any:u` | 2384335094 | 2379952264 | 0.99816 |
| `instructions:u` | 15958323295 | 15965123049 | 1.00043 |
| `op_cache_hit_miss.op_cache_hit:u` | 2260499982 | 2278973325 | 1.00817 |
| `op_cache_hit_miss.op_cache_miss:u` | 214636205 | 206778704 | 0.96339 |
base arm: front-end-bound (dq-empty/cycles) = 0.0254; op-cache miss share = 0.0867
head arm: front-end-bound (dq-empty/cycles) = 0.0271; op-cache miss share = 0.0832

pass A, cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 327141 | 256748 | 0.78482 |
| `branch-misses:u` | 2099246 | 2067730 | 0.98499 |
| `branches:u` | 577331394 | 577434976 | 1.00018 |
| `cycles:u` | 1223077363 | 1220909564 | 0.99823 |
| `instructions:u` | 3834878023 | 3836104607 | 1.00032 |

pass B (Zen 3 front end), cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 1222272900 | 1222312233 | 1.00003 |
| `de_dis_uop_queue_empty_di0:u` | 34015060 | 34811428 | 1.02341 |
| `ic_fetch_stall.ic_stall_any:u` | 528537913 | 528177460 | 0.99932 |
| `instructions:u` | 3834879144 | 3836109110 | 1.00032 |
| `op_cache_hit_miss.op_cache_hit:u` | 540722088 | 544736901 | 1.00742 |
| `op_cache_hit_miss.op_cache_miss:u` | 54366704 | 52281224 | 0.96164 |
base arm: front-end-bound (dq-empty/cycles) = 0.0278; op-cache miss share = 0.0914
head arm: front-end-bound (dq-empty/cycles) = 0.0285; op-cache miss share = 0.0876

### leg 1 / ssn

corpus base 11.0994 s -> head 11.0909 s, **ratio 0.9992** (-0.076 %); cells compared 27; outside 0.99-1.01: **0**; band **FLAT**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 0 **; strict, informational, 3 ['f7_n10000_bound_warm', 'f7_n5000_bound_corrupted', 'f7_n800_path_warm']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `f7_n10000_bound_activity` | 0.705763 | 0.703535 | 0.9968 | 0.9961 1.0007 0.9925 |
| `f7_n10000_bound_corrupted` | 0.325487 | 0.325534 | 1.0001 | 1.0001 1.0038 0.9947 |
| `f7_n10000_bound_neutral` | 0.704532 | 0.702506 | 0.9971 | 0.9988 0.9988 0.9916 |
| `f7_n10000_bound_physics` | 0.703953 | 0.705143 | 1.0017 | 1.0017 1.0080 0.9939 |
| `f7_n10000_bound_warm` | 0.324798 | 0.326639 | 1.0057 | 1.0078 1.0031 1.0074 |
| `f7_n1000_bound_activity` | 0.062441 | 0.062003 | 0.9930 | 0.9982 0.9902 0.9934 |
| `f7_n1000_bound_corrupted` | 0.029270 | 0.029228 | 0.9985 | 1.0039 0.9985 0.9989 |
| `f7_n1000_bound_neutral` | 0.063094 | 0.062849 | 0.9961 | 0.9961 1.0031 0.9940 |
| `f7_n1000_bound_physics` | 0.061987 | 0.061873 | 0.9982 | 0.9991 0.9977 0.9996 |
| `f7_n1000_bound_warm` | 0.029193 | 0.029141 | 0.9982 | 0.9993 0.9986 0.9982 |
| `f7_n1000_path_warm` | 0.223949 | 0.224730 | 1.0035 | 1.0049 1.0062 0.9954 |
| `f7_n20000_bound_activity` | 1.503629 | 1.503849 | 1.0001 | 1.0008 0.9984 0.9946 |
| `f7_n20000_bound_corrupted` | 0.700199 | 0.700346 | 1.0002 | 1.0020 0.9980 0.9993 |
| `f7_n20000_bound_neutral` | 1.505732 | 1.499431 | 0.9958 | 0.9935 0.9993 0.9907 |
| `f7_n20000_bound_physics` | 1.503766 | 1.501787 | 0.9987 | 0.9996 1.0004 0.9940 |
| `f7_n20000_bound_warm` | 0.697853 | 0.700526 | 1.0038 | 1.0034 1.0049 0.9989 |
| `f7_n2000_bound_activity` | 0.128600 | 0.128170 | 0.9967 | 0.9981 1.0001 0.9935 |
| `f7_n2000_bound_corrupted` | 0.061395 | 0.060815 | 0.9906 | 0.9887 0.9888 0.9992 |
| `f7_n2000_bound_neutral` | 0.128338 | 0.128895 | 1.0043 | 0.9990 1.0046 1.0044 |
| `f7_n2000_bound_physics` | 0.127758 | 0.127858 | 1.0008 | 0.9967 1.0001 1.0017 |
| `f7_n2000_bound_warm` | 0.060778 | 0.060639 | 0.9977 | 0.9934 1.0023 0.9974 |
| `f7_n5000_bound_activity` | 0.332731 | 0.332356 | 0.9989 | 0.9907 0.9997 1.0007 |
| `f7_n5000_bound_corrupted` | 0.154192 | 0.154359 | 1.0011 | 1.0001 1.0058 1.0004 |
| `f7_n5000_bound_neutral` | 0.332164 | 0.332452 | 1.0009 | 0.9973 1.0021 1.0003 |
| `f7_n5000_bound_physics` | 0.333819 | 0.331983 | 0.9945 | 0.9976 0.9931 0.9961 |
| `f7_n5000_bound_warm` | 0.152963 | 0.153016 | 1.0003 | 1.0049 1.0002 0.9987 |
| `f7_n800_path_warm` | 0.141006 | 0.141248 | 1.0017 | 1.0178 1.0019 1.0007 |

pass A, cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 39579 | 28987 | 0.73238 |
| `branch-misses:u` | 607020 | 583985 | 0.96205 |
| `branches:u` | 77311604 | 77397043 | 1.00111 |
| `cycles:u` | 178782706 | 178724532 | 0.99967 |
| `instructions:u` | 488479834 | 488903314 | 1.00087 |

pass B (Zen 3 front end), cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 178500126 | 178687695 | 1.00105 |
| `de_dis_uop_queue_empty_di0:u` | 7569270 | 7629983 | 1.00802 |
| `ic_fetch_stall.ic_stall_any:u` | 82681845 | 82617021 | 0.99922 |
| `instructions:u` | 488480134 | 488897489 | 1.00085 |
| `op_cache_hit_miss.op_cache_hit:u` | 77325528 | 77563339 | 1.00308 |
| `op_cache_hit_miss.op_cache_miss:u` | 3905666 | 3941209 | 1.00910 |
base arm: front-end-bound (dq-empty/cycles) = 0.0424; op-cache miss share = 0.0481
head arm: front-end-bound (dq-empty/cycles) = 0.0427; op-cache miss share = 0.0484

pass A, cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 831467 | 569664 | 0.68513 |
| `branch-misses:u` | 9681184 | 9491530 | 0.98041 |
| `branches:u` | 1834993085 | 1836666840 | 1.00091 |
| `cycles:u` | 4314645666 | 4321063387 | 1.00149 |
| `instructions:u` | 11674701748 | 11683014698 | 1.00071 |

pass B (Zen 3 front end), cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 4314978504 | 4315722027 | 1.00017 |
| `de_dis_uop_queue_empty_di0:u` | 117455290 | 116684949 | 0.99344 |
| `ic_fetch_stall.ic_stall_any:u` | 2154952574 | 2155950280 | 1.00046 |
| `instructions:u` | 11674701519 | 11683019527 | 1.00071 |
| `op_cache_hit_miss.op_cache_hit:u` | 1806264726 | 1817000100 | 1.00594 |
| `op_cache_hit_miss.op_cache_miss:u` | 71960562 | 72526458 | 1.00786 |
base arm: front-end-bound (dq-empty/cycles) = 0.0272; op-cache miss share = 0.0383
head arm: front-end-bound (dq-empty/cycles) = 0.0270; op-cache miss share = 0.0384

pass A, cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 200932 | 140252 | 0.69801 |
| `branch-misses:u` | 2324121 | 2264303 | 0.97426 |
| `branches:u` | 413931437 | 414211747 | 1.00068 |
| `cycles:u` | 948803168 | 948888055 | 1.00009 |
| `instructions:u` | 2629037411 | 2630743395 | 1.00065 |

pass B (Zen 3 front end), cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 948856177 | 950979841 | 1.00224 |
| `de_dis_uop_queue_empty_di0:u` | 29354648 | 29270454 | 0.99713 |
| `ic_fetch_stall.ic_stall_any:u` | 456769769 | 457420678 | 1.00143 |
| `instructions:u` | 2629037407 | 2630738428 | 1.00065 |
| `op_cache_hit_miss.op_cache_hit:u` | 408320126 | 409250376 | 1.00228 |
| `op_cache_hit_miss.op_cache_miss:u` | 18038321 | 18169420 | 1.00727 |
base arm: front-end-bound (dq-empty/cycles) = 0.0309; op-cache miss share = 0.0423
head arm: front-end-bound (dq-empty/cycles) = 0.0308; op-cache miss share = 0.0425

### leg 1 / walk

corpus base 6.0245 s -> head 6.0253 s, **ratio 1.0001** (+0.014 %); cells compared 27; outside 0.99-1.01: **0**; band **FLAT**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 0 **; strict, informational, 2 ['f7_n2000_bound_corrupted', 'f7_n5000_bound_corrupted']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `f7_n10000_bound_activity` | 0.312034 | 0.311617 | 0.9987 | 0.9969 0.9959 1.0017 |
| `f7_n10000_bound_corrupted` | 0.294470 | 0.293819 | 0.9978 | 0.9968 1.0034 0.9997 |
| `f7_n10000_bound_neutral` | 0.311176 | 0.311744 | 1.0018 | 1.0044 1.0037 0.9929 |
| `f7_n10000_bound_physics` | 0.311945 | 0.310772 | 0.9962 | 0.9975 1.0022 0.9949 |
| `f7_n10000_bound_warm` | 0.294087 | 0.294042 | 0.9998 | 1.0008 1.0061 0.9991 |
| `f7_n1000_bound_activity` | 0.030893 | 0.030848 | 0.9985 | 1.0001 1.0032 0.9964 |
| `f7_n1000_bound_corrupted` | 0.027439 | 0.027431 | 0.9997 | 1.0065 1.0022 0.9982 |
| `f7_n1000_bound_neutral` | 0.031489 | 0.031433 | 0.9982 | 1.0022 0.9986 0.9952 |
| `f7_n1000_bound_physics` | 0.030945 | 0.030735 | 0.9932 | 0.8877 0.9985 0.9932 |
| `f7_n1000_bound_warm` | 0.027057 | 0.027060 | 1.0001 | 0.9988 1.0066 1.0001 |
| `f7_n1000_path_warm` | 0.105107 | 0.104948 | 0.9985 | 1.0001 1.0055 0.9971 |
| `f7_n20000_bound_activity` | 0.646089 | 0.646297 | 1.0003 | 1.0033 1.0004 0.9978 |
| `f7_n20000_bound_corrupted` | 0.612187 | 0.613787 | 1.0026 | 1.0054 0.9961 1.0033 |
| `f7_n20000_bound_neutral` | 0.646449 | 0.645325 | 0.9983 | 0.9990 0.9983 1.0025 |
| `f7_n20000_bound_physics` | 0.645156 | 0.645778 | 1.0010 | 1.0019 1.0188 0.9965 |
| `f7_n20000_bound_warm` | 0.610274 | 0.611516 | 1.0020 | 1.0020 0.9956 1.0048 |
| `f7_n2000_bound_activity` | 0.061224 | 0.061165 | 0.9990 | 0.9990 0.9990 1.0008 |
| `f7_n2000_bound_corrupted` | 0.056251 | 0.056387 | 1.0024 | 1.0038 1.0003 1.0025 |
| `f7_n2000_bound_neutral` | 0.061453 | 0.061361 | 0.9985 | 0.9962 1.0100 0.9949 |
| `f7_n2000_bound_physics` | 0.061136 | 0.061221 | 1.0014 | 0.9962 1.0014 0.9992 |
| `f7_n2000_bound_warm` | 0.055789 | 0.055703 | 0.9985 | 0.9995 0.9980 0.9822 |
| `f7_n5000_bound_activity` | 0.151819 | 0.151669 | 0.9990 | 0.9948 0.9942 1.0008 |
| `f7_n5000_bound_corrupted` | 0.141917 | 0.142532 | 1.0043 | 1.0029 1.0079 1.0017 |
| `f7_n5000_bound_neutral` | 0.151678 | 0.151525 | 0.9990 | 0.9953 1.0028 1.0016 |
| `f7_n5000_bound_physics` | 0.151460 | 0.151248 | 0.9986 | 0.9938 1.0017 1.0008 |
| `f7_n5000_bound_warm` | 0.141337 | 0.141653 | 1.0022 | 0.9964 1.0025 1.0104 |
| `f7_n800_path_warm` | 0.053672 | 0.053733 | 1.0011 | 1.0014 0.9996 1.0032 |

pass A, cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 16955 | 15245 | 0.89914 |
| `branch-misses:u` | 305697 | 301806 | 0.98727 |
| `branches:u` | 39749528 | 39806863 | 1.00144 |
| `cycles:u` | 87656503 | 87574543 | 0.99906 |
| `instructions:u` | 261301248 | 261644867 | 1.00132 |

pass B (Zen 3 front end), cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 87631284 | 87469356 | 0.99815 |
| `de_dis_uop_queue_empty_di0:u` | 3923503 | 3776283 | 0.96248 |
| `ic_fetch_stall.ic_stall_any:u` | 39688179 | 39251115 | 0.98899 |
| `instructions:u` | 261301243 | 261644860 | 1.00132 |
| `op_cache_hit_miss.op_cache_hit:u` | 40153582 | 40303819 | 1.00374 |
| `op_cache_hit_miss.op_cache_miss:u` | 1494939 | 1559596 | 1.04325 |
base arm: front-end-bound (dq-empty/cycles) = 0.0448; op-cache miss share = 0.0359
head arm: front-end-bound (dq-empty/cycles) = 0.0432; op-cache miss share = 0.0373

pass A, cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 222987 | 203254 | 0.91151 |
| `branch-misses:u` | 4962619 | 4876724 | 0.98269 |
| `branches:u` | 857541749 | 858097331 | 1.00065 |
| `cycles:u` | 1873162320 | 1868074830 | 0.99728 |
| `instructions:u` | 5640392312 | 5645600279 | 1.00092 |

pass B (Zen 3 front end), cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 1867238209 | 1869998248 | 1.00148 |
| `de_dis_uop_queue_empty_di0:u` | 45794954 | 47113501 | 1.02879 |
| `ic_fetch_stall.ic_stall_any:u` | 874466656 | 871689036 | 0.99682 |
| `instructions:u` | 5640391548 | 5645594599 | 1.00092 |
| `op_cache_hit_miss.op_cache_hit:u` | 861378988 | 860712144 | 0.99923 |
| `op_cache_hit_miss.op_cache_miss:u` | 24073297 | 25667297 | 1.06621 |
base arm: front-end-bound (dq-empty/cycles) = 0.0245; op-cache miss share = 0.0272
head arm: front-end-bound (dq-empty/cycles) = 0.0252; op-cache miss share = 0.0290

pass A, cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 59010 | 54108 | 0.91693 |
| `branch-misses:u` | 1240004 | 1236478 | 0.99716 |
| `branches:u` | 202368147 | 202650607 | 1.00140 |
| `cycles:u` | 436537834 | 435943427 | 0.99864 |
| `instructions:u` | 1333971634 | 1335665712 | 1.00127 |

pass B (Zen 3 front end), cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 436975923 | 437233221 | 1.00059 |
| `de_dis_uop_queue_empty_di0:u` | 12475113 | 12518659 | 1.00349 |
| `ic_fetch_stall.ic_stall_any:u` | 200285406 | 199530996 | 0.99623 |
| `instructions:u` | 1333977267 | 1335664954 | 1.00127 |
| `op_cache_hit_miss.op_cache_hit:u` | 203645279 | 203634364 | 0.99995 |
| `op_cache_hit_miss.op_cache_miss:u` | 6107760 | 6643089 | 1.08765 |
base arm: front-end-bound (dq-empty/cycles) = 0.0285; op-cache miss share = 0.0291
head arm: front-end-bound (dq-empty/cycles) = 0.0286; op-cache miss share = 0.0316

## Leg 2 -- the 27 Hock-Schittkowski problems, --repeat N

**Leg 2's noise floor (I5 (b)).** The same binary measured twice, per (mode, trace) population; the floor this leg is gated on is the MAXIMUM, not a median across combinations:

| mode / trace | worst same-arm passA/passB disagreement |
|---|---|
| ipm / off | 0.158309 % |
| ipm / sink | 0.158621 % |
| ssn / off | 0.153612 % |
| ssn / sink | 0.152999 % |
| walk / off | 0.011700 % |
| walk / sink | 0.245198 % |

**Leg 2 floor = 0.245198 %** (the maximum above); the 1e-4 identity band is 0.01 %.

### leg 2 / ipm / trace=off

corpus base 0.0373 s -> head 0.0370 s, **ratio 0.9913** (-0.870 %); cells compared 27; outside 0.99-1.01: **21**; band **MOVED**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 2 ['12', '15']**; strict, informational, 2 ['12', '15']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.002333 | 0.002291 | 0.9823 | 0.9846 0.9794 0.9917 |
| `10` | 0.004632 | 0.004574 | 0.9875 | 0.9875 0.9748 0.9953 |
| `11` | 0.001221 | 0.001192 | 0.9761 | 0.9761 0.9674 0.9874 |
| `12` | 0.000761 | 0.000855 | 1.1227 | 1.1171 1.1212 1.1360 |
| `14` | 0.000571 | 0.000560 | 0.9810 | 0.9810 0.9663 0.9944 |
| `15` | 0.003108 | 0.003259 | 1.0484 | 1.0465 1.0446 1.0591 |
| `22` | 0.000749 | 0.000709 | 0.9461 | 0.9561 0.9378 0.9504 |
| `24` | 0.000589 | 0.000574 | 0.9753 | 0.9753 0.9723 0.9817 |
| `25` | 0.000027 | 0.000027 | 1.0049 | 1.0169 0.9981 1.0072 |
| `26` | 0.001704 | 0.001679 | 0.9850 | 0.9824 0.9850 0.9978 |
| `27` | 0.001909 | 0.001877 | 0.9830 | 0.9830 0.9749 0.9944 |
| `28` | 0.000465 | 0.000456 | 0.9800 | 0.9798 0.9740 0.9935 |
| `3` | 0.000600 | 0.000593 | 0.9875 | 0.9953 0.9780 0.9841 |
| `30` | 0.001253 | 0.001223 | 0.9759 | 0.9723 0.9749 0.9931 |
| `33` | 0.001906 | 0.001888 | 0.9909 | 0.9899 0.9875 0.9996 |
| `35` | 0.000155 | 0.000153 | 0.9847 | 0.9860 0.9769 0.9915 |
| `38` | 0.006246 | 0.006093 | 0.9755 | 0.9754 0.9717 0.9880 |
| `39` | 0.001204 | 0.001183 | 0.9829 | 0.9754 0.9781 0.9968 |
| `40` | 0.000491 | 0.000483 | 0.9852 | 0.9852 0.9713 0.9948 |
| `43` | 0.001358 | 0.001348 | 0.9923 | 0.9934 0.9787 0.9980 |
| `45` | 0.001123 | 0.001120 | 0.9975 | 0.9975 0.9827 1.0064 |
| `5` | 0.000372 | 0.000366 | 0.9829 | 0.9925 0.9684 0.9829 |
| `6` | 0.000934 | 0.000929 | 0.9952 | 0.9998 0.9853 0.9923 |
| `7` | 0.001090 | 0.001082 | 0.9924 | 0.9989 0.9835 0.9930 |
| `76` | 0.000534 | 0.000525 | 0.9843 | 0.9850 0.9723 0.9851 |
| `77` | 0.001468 | 0.001448 | 0.9862 | 0.9862 0.9776 0.9953 |
| `79` | 0.000511 | 0.000503 | 0.9843 | 0.9843 0.9811 0.9933 |

perf: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1))

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 706931911 | 782015933 | 1.10621 |
| `branch-misses:u` | 370149731 | 339002053 | 0.91585 |
| `branches:u` | 45235419965 | 45172633903 | 0.99861 |
| `cycles:u` | 126374530385 | 124594126388 | 0.98591 |
| `instructions:u` | 247046537687 | 246930307513 | 0.99953 |

pass B: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 126519520602 | 124554439614 | 0.98447 |
| `de_dis_uop_queue_empty_di0:u` | 28872319439 | 27983936699 | 0.96923 |
| `ic_fetch_stall.ic_stall_any:u` | 53420840273 | 52254460970 | 0.97817 |
| `instructions:u` | 247046392527 | 247321839491 | 1.00111 |
| `op_cache_hit_miss.op_cache_hit:u` | 22502591067 | 22713690477 | 1.00938 |
| `op_cache_hit_miss.op_cache_miss:u` | 24647576724 | 24171839346 | 0.98070 |

### leg 2 / ipm / trace=sink

corpus base 0.0374 s -> head 0.0369 s, **ratio 0.9861** (-1.393 %); cells compared 27; outside 0.99-1.01: **26**; band **MOVED**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 2 ['12', '15']**; strict, informational, 2 ['12', '15']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.002353 | 0.002299 | 0.9769 | 0.9736 0.9879 0.9880 |
| `10` | 0.004646 | 0.004549 | 0.9792 | 0.9733 0.9836 0.9867 |
| `11` | 0.001219 | 0.001187 | 0.9733 | 0.9615 0.9733 0.9845 |
| `12` | 0.000760 | 0.000857 | 1.1273 | 1.1180 1.1273 1.1421 |
| `14` | 0.000572 | 0.000557 | 0.9735 | 0.9698 0.9735 0.9830 |
| `15` | 0.003123 | 0.003257 | 1.0430 | 1.0345 1.0489 1.0521 |
| `22` | 0.000748 | 0.000705 | 0.9427 | 0.9342 0.9494 0.9493 |
| `24` | 0.000593 | 0.000573 | 0.9663 | 0.9639 0.9755 0.9706 |
| `25` | 0.000027 | 0.000027 | 1.0090 | 1.0087 0.9778 1.0090 |
| `26` | 0.001714 | 0.001682 | 0.9812 | 0.9751 0.9881 0.9874 |
| `27` | 0.001898 | 0.001863 | 0.9819 | 0.9745 0.9831 0.9893 |
| `28` | 0.000464 | 0.000456 | 0.9836 | 0.9691 0.9879 0.9875 |
| `3` | 0.000597 | 0.000590 | 0.9887 | 0.9744 0.9931 0.9976 |
| `30` | 0.001254 | 0.001221 | 0.9733 | 0.9729 0.9759 0.9777 |
| `33` | 0.001925 | 0.001882 | 0.9776 | 0.9713 0.9969 0.9873 |
| `35` | 0.000155 | 0.000153 | 0.9863 | 0.9716 0.9863 0.9995 |
| `38` | 0.006252 | 0.006076 | 0.9718 | 0.9669 0.9778 0.9787 |
| `39` | 0.001208 | 0.001181 | 0.9775 | 0.9768 0.9791 0.9784 |
| `40` | 0.000494 | 0.000482 | 0.9760 | 0.9760 0.9871 0.9745 |
| `43` | 0.001363 | 0.001339 | 0.9820 | 0.9788 0.9885 0.9848 |
| `45` | 0.001128 | 0.001117 | 0.9899 | 0.9893 1.0003 0.9944 |
| `5` | 0.000371 | 0.000363 | 0.9804 | 0.9673 0.9832 0.9938 |
| `6` | 0.000935 | 0.000924 | 0.9877 | 0.9789 0.9932 0.9980 |
| `7` | 0.001090 | 0.001077 | 0.9882 | 0.9763 0.9914 0.9985 |
| `76` | 0.000531 | 0.000521 | 0.9797 | 0.9797 0.9798 0.9834 |
| `77` | 0.001480 | 0.001451 | 0.9805 | 0.9768 0.9920 0.9809 |
| `79` | 0.000513 | 0.000504 | 0.9826 | 0.9784 0.9869 0.9761 |

perf: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 703605738 | 780630703 | 1.10947 |
| `branch-misses:u` | 360494078 | 347003965 | 0.96258 |
| `branches:u` | 45134863694 | 45191303920 | 1.00125 |
| `cycles:u` | 126782297518 | 124551438056 | 0.98240 |
| `instructions:u` | 246877716760 | 247153434708 | 1.00112 |

pass B: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 127446368910 | 124970460402 | 0.98057 |
| `de_dis_uop_queue_empty_di0:u` | 29370166374 | 28159364785 | 0.95877 |
| `ic_fetch_stall.ic_stall_any:u` | 53952935716 | 52269843210 | 0.96880 |
| `instructions:u` | 247269938321 | 247545370414 | 1.00111 |
| `op_cache_hit_miss.op_cache_hit:u` | 22572642605 | 22771514828 | 1.00881 |
| `op_cache_hit_miss.op_cache_miss:u` | 24812379430 | 24295223149 | 0.97916 |

### leg 2 / ssn / trace=off

corpus base 0.0304 s -> head 0.0299 s, **ratio 0.9822** (-1.779 %); cells compared 27; outside 0.99-1.01: **17**; band **MOVED**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 0 **; strict, informational, 1 ['25']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.001682 | 0.001645 | 0.9781 | 0.9883 0.9728 0.9700 |
| `10` | 0.006559 | 0.006487 | 0.9890 | 0.9984 0.9847 0.9868 |
| `11` | 0.002029 | 0.001883 | 0.9281 | 0.9364 0.9257 0.9264 |
| `12` | 0.000410 | 0.000401 | 0.9792 | 0.9866 0.9794 0.9770 |
| `14` | 0.000359 | 0.000353 | 0.9815 | 0.9856 0.9772 0.9815 |
| `15` | 0.001188 | 0.001155 | 0.9719 | 0.9764 0.9671 0.9709 |
| `22` | 0.000521 | 0.000494 | 0.9487 | 0.9558 0.9480 0.9455 |
| `24` | 0.000649 | 0.000619 | 0.9541 | 0.9603 0.9509 0.9530 |
| `25` | 0.000027 | 0.000027 | 1.0098 | 1.0083 1.0098 1.0153 |
| `26` | 0.001118 | 0.001093 | 0.9782 | 0.9989 0.9743 0.9739 |
| `27` | 0.001524 | 0.001498 | 0.9827 | 0.9959 0.9817 0.9827 |
| `28` | 0.000272 | 0.000270 | 0.9920 | 1.0039 0.9884 0.9920 |
| `3` | 0.000321 | 0.000314 | 0.9781 | 0.9991 0.9708 0.9781 |
| `30` | 0.000701 | 0.000688 | 0.9821 | 0.9895 0.9718 0.9767 |
| `33` | 0.000533 | 0.000530 | 0.9943 | 1.0035 0.9896 0.9870 |
| `35` | 0.000132 | 0.000131 | 0.9920 | 0.9936 0.9995 0.9904 |
| `38` | 0.005144 | 0.005083 | 0.9882 | 0.9983 0.9841 0.9843 |
| `39` | 0.001123 | 0.001116 | 0.9940 | 1.0026 0.9926 0.9869 |
| `40` | 0.000318 | 0.000313 | 0.9846 | 0.9907 0.9849 0.9747 |
| `43` | 0.000892 | 0.000886 | 0.9930 | 1.0064 0.9920 0.9916 |
| `45` | 0.000783 | 0.000776 | 0.9913 | 1.0032 0.9885 0.9913 |
| `5` | 0.000297 | 0.000292 | 0.9810 | 1.0064 0.9790 0.9691 |
| `6` | 0.001061 | 0.001062 | 1.0013 | 1.0037 0.9900 0.9928 |
| `7` | 0.001106 | 0.001107 | 1.0010 | 1.0046 0.9911 0.9941 |
| `76` | 0.000277 | 0.000274 | 0.9897 | 1.0072 0.9834 0.9897 |
| `77` | 0.001034 | 0.001026 | 0.9925 | 0.9974 0.9887 0.9895 |
| `79` | 0.000339 | 0.000333 | 0.9835 | 0.9928 0.9849 0.9774 |

perf: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 922815428 | 1101187291 | 1.19329 |
| `branch-misses:u` | 599184260 | 560992513 | 0.93626 |
| `branches:u` | 76148743517 | 76407743396 | 1.00340 |
| `cycles:u` | 205540080362 | 202457868212 | 0.98500 |
| `instructions:u` | 420655887578 | 421876944130 | 1.00290 |

pass B: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 205765487168 | 202653983726 | 0.98488 |
| `de_dis_uop_queue_empty_di0:u` | 44883629951 | 43723151368 | 0.97414 |
| `ic_fetch_stall.ic_stall_any:u` | 85649199597 | 84108151052 | 0.98201 |
| `instructions:u` | 420010698648 | 421876775857 | 1.00444 |
| `op_cache_hit_miss.op_cache_hit:u` | 42185453495 | 42405759159 | 1.00522 |
| `op_cache_hit_miss.op_cache_miss:u` | 37536777444 | 36820163002 | 0.98091 |

### leg 2 / ssn / trace=sink

corpus base 0.0305 s -> head 0.0297 s, **ratio 0.9754** (-2.456 %); cells compared 27; outside 0.99-1.01: **23**; band **MOVED**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 0 **; strict, informational, 1 ['25']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.001689 | 0.001641 | 0.9716 | 0.9716 0.9773 0.9651 |
| `10` | 0.006572 | 0.006445 | 0.9807 | 0.9807 0.9929 0.9788 |
| `11` | 0.002036 | 0.001874 | 0.9209 | 0.9197 0.9316 0.9209 |
| `12` | 0.000412 | 0.000401 | 0.9727 | 0.9742 0.9781 0.9727 |
| `14` | 0.000360 | 0.000352 | 0.9777 | 0.9698 0.9905 0.9741 |
| `15` | 0.001190 | 0.001148 | 0.9653 | 0.9642 0.9738 0.9637 |
| `22` | 0.000521 | 0.000493 | 0.9467 | 0.9467 0.9532 0.9422 |
| `24` | 0.000649 | 0.000616 | 0.9494 | 0.9494 0.9561 0.9450 |
| `25` | 0.000027 | 0.000027 | 1.0098 | 1.0079 1.0139 1.0101 |
| `26` | 0.001118 | 0.001089 | 0.9739 | 0.9739 0.9861 0.9650 |
| `27` | 0.001522 | 0.001489 | 0.9785 | 0.9785 0.9898 0.9713 |
| `28` | 0.000273 | 0.000269 | 0.9827 | 0.9798 0.9912 0.9838 |
| `3` | 0.000322 | 0.000315 | 0.9772 | 0.9733 0.9838 0.9659 |
| `30` | 0.000704 | 0.000680 | 0.9652 | 0.9632 0.9773 0.9652 |
| `33` | 0.000536 | 0.000528 | 0.9863 | 0.9861 0.9974 0.9855 |
| `35` | 0.000132 | 0.000131 | 0.9930 | 0.9940 0.9953 0.9913 |
| `38` | 0.005151 | 0.005059 | 0.9821 | 0.9821 0.9906 0.9786 |
| `39` | 0.001128 | 0.001115 | 0.9885 | 0.9850 0.9924 0.9871 |
| `40` | 0.000320 | 0.000313 | 0.9756 | 0.9756 0.9764 0.9671 |
| `43` | 0.000896 | 0.000884 | 0.9868 | 0.9845 0.9977 0.9868 |
| `45` | 0.000783 | 0.000773 | 0.9877 | 0.9871 0.9979 0.9856 |
| `5` | 0.000301 | 0.000294 | 0.9792 | 0.9690 0.9923 0.9720 |
| `6` | 0.001067 | 0.001058 | 0.9916 | 0.9860 1.0011 0.9916 |
| `7` | 0.001113 | 0.001106 | 0.9941 | 0.9865 1.0049 0.9941 |
| `76` | 0.000277 | 0.000274 | 0.9873 | 0.9695 0.9962 0.9873 |
| `77` | 0.001039 | 0.001020 | 0.9826 | 0.9826 0.9918 0.9785 |
| `79` | 0.000340 | 0.000333 | 0.9787 | 0.9684 0.9793 0.9852 |

perf: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 928492530 | 1096761915 | 1.18123 |
| `branch-misses:u` | 607137601 | 544161270 | 0.89627 |
| `branches:u` | 75957517855 | 76216538730 | 1.00341 |
| `cycles:u` | 205988582717 | 201470242317 | 0.97807 |
| `instructions:u` | 420047893460 | 421269399170 | 1.00291 |

pass B: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 205336581312 | 203106248583 | 0.98914 |
| `de_dis_uop_queue_empty_di0:u` | 44625309894 | 43426878308 | 0.97314 |
| `ic_fetch_stall.ic_stall_any:u` | 85341290499 | 84135900786 | 0.98588 |
| `instructions:u` | 420048270761 | 421914926774 | 1.00444 |
| `op_cache_hit_miss.op_cache_hit:u` | 41851822100 | 42343074274 | 1.01174 |
| `op_cache_hit_miss.op_cache_miss:u` | 37537386556 | 36912684622 | 0.98336 |

### leg 2 / walk / trace=off

corpus base 0.0227 s -> head 0.0226 s, **ratio 0.9931** (-0.686 %); cells compared 27; outside 0.99-1.01: **8**; band **UNRESOLVED**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 0 **; strict, informational, 1 ['35']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.001280 | 0.001268 | 0.9904 | 0.9872 0.9878 0.9904 |
| `10` | 0.005478 | 0.005451 | 0.9951 | 0.9956 0.9862 0.9972 |
| `11` | 0.001347 | 0.001344 | 0.9982 | 0.9988 0.9893 0.9991 |
| `12` | 0.000404 | 0.000391 | 0.9683 | 0.9581 0.9683 0.9812 |
| `14` | 0.000256 | 0.000251 | 0.9830 | 0.9840 0.9769 0.9830 |
| `15` | 0.001003 | 0.001001 | 0.9975 | 0.9967 0.9912 1.0018 |
| `22` | 0.000253 | 0.000253 | 1.0018 | 1.0031 0.9951 1.0018 |
| `24` | 0.000440 | 0.000432 | 0.9818 | 0.9820 0.9673 0.9859 |
| `25` | 0.000027 | 0.000027 | 1.0094 | 1.0102 1.0102 0.9609 |
| `26` | 0.000897 | 0.000882 | 0.9839 | 0.9894 0.9850 0.9750 |
| `27` | 0.001336 | 0.001319 | 0.9872 | 0.9943 0.9778 0.9872 |
| `28` | 0.000226 | 0.000222 | 0.9835 | 0.9898 0.9866 0.9830 |
| `3` | 0.000222 | 0.000220 | 0.9913 | 0.9862 0.9849 0.9913 |
| `30` | 0.000636 | 0.000633 | 0.9946 | 0.9941 0.9895 0.9964 |
| `33` | 0.000673 | 0.000670 | 0.9956 | 0.9959 0.9889 0.9981 |
| `35` | 0.000091 | 0.000091 | 1.0029 | 1.0088 1.0004 1.0058 |
| `38` | 0.003327 | 0.003303 | 0.9929 | 1.0001 0.9871 0.9910 |
| `39` | 0.000684 | 0.000682 | 0.9971 | 0.9994 0.9912 0.9972 |
| `40` | 0.000234 | 0.000230 | 0.9841 | 0.9963 0.9945 0.9821 |
| `43` | 0.000648 | 0.000647 | 0.9985 | 1.0008 0.9910 1.0015 |
| `45` | 0.000521 | 0.000518 | 0.9956 | 0.9957 0.9863 0.9984 |
| `5` | 0.000184 | 0.000182 | 0.9893 | 0.9850 0.9861 0.9893 |
| `6` | 0.000586 | 0.000582 | 0.9937 | 0.9897 0.9907 0.9937 |
| `7` | 0.000740 | 0.000738 | 0.9966 | 0.9875 0.9934 0.9966 |
| `76` | 0.000275 | 0.000276 | 1.0018 | 0.9974 0.9976 1.0047 |
| `77` | 0.000718 | 0.000716 | 0.9970 | 1.0082 0.9928 0.9970 |
| `79` | 0.000241 | 0.000240 | 0.9941 | 1.0076 0.9950 0.9891 |

perf: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 403329363 | 486946254 | 1.20732 |
| `branch-misses:u` | 162768671 | 159489667 | 0.97985 |
| `branches:u` | 30523461684 | 30660459958 | 1.00449 |
| `cycles:u` | 77002998708 | 76501798696 | 0.99349 |
| `instructions:u` | 167530241085 | 168052397121 | 1.00312 |

pass B: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 77073978997 | 76569890856 | 0.99346 |
| `de_dis_uop_queue_empty_di0:u` | 15135511702 | 14988272872 | 0.99027 |
| `ic_fetch_stall.ic_stall_any:u` | 30029850548 | 29933347169 | 0.99679 |
| `instructions:u` | 167530258399 | 168072062225 | 1.00323 |
| `op_cache_hit_miss.op_cache_hit:u` | 15194069095 | 15203742488 | 1.00064 |
| `op_cache_hit_miss.op_cache_miss:u` | 15861529604 | 15815861768 | 0.99712 |

### leg 2 / walk / trace=sink

corpus base 0.0230 s -> head 0.0225 s, **ratio 0.9761** (-2.390 %); cells compared 27; outside 0.99-1.01: **24**; band **MOVED**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 0 **; strict, informational, 1 ['25']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.001287 | 0.001265 | 0.9828 | 0.9914 0.9853 0.9828 |
| `10` | 0.005507 | 0.005423 | 0.9847 | 0.9910 0.9889 0.9847 |
| `11` | 0.001408 | 0.001339 | 0.9510 | 0.9519 0.9578 0.9508 |
| `12` | 0.000414 | 0.000386 | 0.9329 | 0.9419 0.9407 0.9287 |
| `14` | 0.000257 | 0.000251 | 0.9763 | 0.9781 0.9830 0.9660 |
| `15` | 0.001051 | 0.000995 | 0.9468 | 0.9503 0.9555 0.9447 |
| `22` | 0.000271 | 0.000252 | 0.9305 | 0.9326 0.9351 0.9290 |
| `24` | 0.000468 | 0.000430 | 0.9205 | 0.9247 0.9287 0.9202 |
| `25` | 0.000027 | 0.000027 | 1.0079 | 1.0086 1.0048 1.0079 |
| `26` | 0.000902 | 0.000879 | 0.9752 | 0.9845 0.9813 0.9698 |
| `27` | 0.001343 | 0.001313 | 0.9775 | 0.9876 0.9813 0.9757 |
| `28` | 0.000225 | 0.000221 | 0.9824 | 0.9816 0.9858 0.9760 |
| `3` | 0.000223 | 0.000219 | 0.9829 | 0.9913 0.9835 0.9829 |
| `30` | 0.000642 | 0.000631 | 0.9831 | 0.9883 0.9866 0.9769 |
| `33` | 0.000679 | 0.000666 | 0.9807 | 0.9799 0.9848 0.9796 |
| `35` | 0.000092 | 0.000091 | 0.9961 | 1.0025 1.0012 0.9941 |
| `38` | 0.003355 | 0.003281 | 0.9780 | 0.9904 0.9835 0.9777 |
| `39` | 0.000690 | 0.000678 | 0.9838 | 0.9896 0.9916 0.9819 |
| `40` | 0.000234 | 0.000231 | 0.9863 | 0.9863 0.9937 0.9770 |
| `43` | 0.000652 | 0.000644 | 0.9876 | 0.9904 0.9921 0.9836 |
| `45` | 0.000524 | 0.000515 | 0.9833 | 0.9872 0.9880 0.9818 |
| `5` | 0.000186 | 0.000182 | 0.9834 | 0.9894 0.9849 0.9834 |
| `6` | 0.000589 | 0.000580 | 0.9853 | 0.9793 0.9849 0.9853 |
| `7` | 0.000747 | 0.000734 | 0.9826 | 0.9848 0.9834 0.9826 |
| `76` | 0.000278 | 0.000275 | 0.9894 | 0.9873 0.9957 0.9817 |
| `77` | 0.000726 | 0.000716 | 0.9862 | 0.9930 0.9906 0.9856 |
| `79` | 0.000242 | 0.000240 | 0.9944 | 1.0017 0.9944 0.9857 |

perf: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1))

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 396750813 | 488495698 | 1.23124 |
| `branch-misses:u` | 169834496 | 154520800 | 0.90983 |
| `branches:u` | 30626755840 | 30552625280 | 0.99758 |
| `cycles:u` | 77763742587 | 76210875045 | 0.98003 |
| `instructions:u` | 167866546913 | 167708110355 | 0.99906 |

pass B: NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 77525262209 | 76618960979 | 0.98831 |
| `de_dis_uop_queue_empty_di0:u` | 15332536263 | 15265289251 | 0.99561 |
| `ic_fetch_stall.ic_stall_any:u` | 30302911635 | 30192552517 | 0.99636 |
| `instructions:u` | 167927025244 | 168120338106 | 1.00115 |
| `op_cache_hit_miss.op_cache_hit:u` | 15216478894 | 15156242366 | 0.99604 |
| `op_cache_hit_miss.op_cache_miss:u` | 15903428754 | 15778168758 | 0.99212 |

## The interior leg -- THREE arms

The leg exists at 102f729 (astra I2: `149f29b` is an ancestor of `102f729`), with 33 rows and the 19-column schema. It therefore gets a base arm at the SQP legs' own base for those 33 keys, and keeps b9848bf as the 41-key arm. What is measured at 102f729 -> e51a7e0 IS the top-level IPM's runtime across the whole of group 1 on those 33 rows.

### interior / 102f729 -> e51a7e0 (33 base keys) / F7 rows (banded, leg-1 rules)

Keys only in the HEAD arm (head-only, informational, no band): `f7_n1000_bound_neutral/MakeConstraint/parts2`, `f7_n1000_bound_neutral/MakeParameter/cap1`, `f7_n1000_bound_neutral/MakeParameter/parts2`, `f7_n1000_bound_neutral/MakeParameter/solve_optimize`, `hs071_x1_fixed/MakeParameter/cap1`, `hs071_x1_fixed/MakeParameter/solve_optimize`, `hs071_x1_fixed/MakeParameter/warm_multiplier_seed`, `hs071_x1_fixed/MakeParameter/warm_payload`, `infeas2_spike/MakeParameter/stalled`, `infeas2_stationary/MakeParameter/resto_infeasible`

corpus base 11.1107 s -> head 10.8430 s, **ratio 0.9759** (-2.409 %); cells compared 30; outside 0.99-1.01: **30**; band **MOVED**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 28 ['f7_n10000_bound_neutral/MakeConstraint', 'f7_n10000_bound_neutral/MakeParameter', 'f7_n10000_bound_physics/MakeConstraint', 'f7_n10000_bound_physics/MakeParameter', 'f7_n10000_bound_physics/RelaxBounds', 'f7_n1000_bound_neutral/MakeConstraint', 'f7_n1000_bound_neutral/RelaxBounds', 'f7_n1000_bound_physics/MakeConstraint', 'f7_n1000_bound_physics/MakeParameter', 'f7_n1000_bound_physics/RelaxBounds', 'f7_n20000_bound_neutral/MakeConstraint', 'f7_n20000_bound_neutral/MakeParameter', 'f7_n20000_bound_neutral/RelaxBounds', 'f7_n20000_bound_physics/MakeConstraint', 'f7_n20000_bound_physics/MakeParameter', 'f7_n20000_bound_physics/RelaxBounds', 'f7_n2000_bound_neutral/MakeConstraint', 'f7_n2000_bound_neutral/MakeParameter', 'f7_n2000_bound_neutral/RelaxBounds', 'f7_n2000_bound_physics/MakeConstraint', 'f7_n2000_bound_physics/MakeParameter', 'f7_n2000_bound_physics/RelaxBounds', 'f7_n5000_bound_neutral/MakeConstraint', 'f7_n5000_bound_neutral/MakeParameter', 'f7_n5000_bound_neutral/RelaxBounds', 'f7_n5000_bound_physics/MakeConstraint', 'f7_n5000_bound_physics/MakeParameter', 'f7_n5000_bound_physics/RelaxBounds']**; strict, informational, 28 ['f7_n10000_bound_neutral/MakeConstraint', 'f7_n10000_bound_neutral/MakeParameter', 'f7_n10000_bound_physics/MakeConstraint', 'f7_n10000_bound_physics/MakeParameter', 'f7_n10000_bound_physics/RelaxBounds', 'f7_n1000_bound_neutral/MakeConstraint', 'f7_n1000_bound_neutral/RelaxBounds', 'f7_n1000_bound_physics/MakeConstraint', 'f7_n1000_bound_physics/MakeParameter', 'f7_n1000_bound_physics/RelaxBounds', 'f7_n20000_bound_neutral/MakeConstraint', 'f7_n20000_bound_neutral/MakeParameter', 'f7_n20000_bound_neutral/RelaxBounds', 'f7_n20000_bound_physics/MakeConstraint', 'f7_n20000_bound_physics/MakeParameter', 'f7_n20000_bound_physics/RelaxBounds', 'f7_n2000_bound_neutral/MakeConstraint', 'f7_n2000_bound_neutral/MakeParameter', 'f7_n2000_bound_neutral/RelaxBounds', 'f7_n2000_bound_physics/MakeConstraint', 'f7_n2000_bound_physics/MakeParameter', 'f7_n2000_bound_physics/RelaxBounds', 'f7_n5000_bound_neutral/MakeConstraint', 'f7_n5000_bound_neutral/MakeParameter', 'f7_n5000_bound_neutral/RelaxBounds', 'f7_n5000_bound_physics/MakeConstraint', 'f7_n5000_bound_physics/MakeParameter', 'f7_n5000_bound_physics/RelaxBounds']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `f7_n10000_bound_neutral/MakeConstraint` | 0.522490 | 0.535106 | 1.0241 | 1.0255 1.0247 1.0159 |
| `f7_n10000_bound_neutral/MakeParameter` | 0.534967 | 0.557573 | 1.0423 | 1.0616 1.0327 1.0423 |
| `f7_n10000_bound_neutral/RelaxBounds` | 0.524365 | 0.533073 | 1.0166 | 1.0379 1.0182 0.9874 |
| `f7_n10000_bound_physics/MakeConstraint` | 0.359548 | 0.369068 | 1.0265 | 1.0160 1.0287 1.0277 |
| `f7_n10000_bound_physics/MakeParameter` | 0.358677 | 0.366945 | 1.0231 | 1.0282 1.0151 1.0246 |
| `f7_n10000_bound_physics/RelaxBounds` | 0.357815 | 0.367796 | 1.0279 | 1.0275 1.0351 1.0270 |
| `f7_n1000_bound_neutral/MakeConstraint` | 0.040156 | 0.041063 | 1.0226 | 1.0215 1.0226 1.0251 |
| `f7_n1000_bound_neutral/MakeParameter` | 1.007717 | 0.488490 | 0.4847 | 0.8306 0.4847 0.4792 |
| `f7_n1000_bound_neutral/RelaxBounds` | 0.040066 | 0.040763 | 1.0174 | 1.0106 1.0176 1.0191 |
| `f7_n1000_bound_physics/MakeConstraint` | 0.031875 | 0.032625 | 1.0235 | 1.0245 1.0219 1.0246 |
| `f7_n1000_bound_physics/MakeParameter` | 0.031876 | 0.032621 | 1.0234 | 1.0250 1.0210 1.0252 |
| `f7_n1000_bound_physics/RelaxBounds` | 0.031824 | 0.033414 | 1.0500 | 1.0587 1.0447 1.0507 |
| `f7_n20000_bound_neutral/MakeConstraint` | 1.096184 | 1.115335 | 1.0175 | 1.0198 1.0231 1.0167 |
| `f7_n20000_bound_neutral/MakeParameter` | 1.128355 | 1.157094 | 1.0255 | 1.0300 1.0258 1.0236 |
| `f7_n20000_bound_neutral/RelaxBounds` | 1.093752 | 1.117852 | 1.0220 | 1.0210 1.0220 1.0302 |
| `f7_n20000_bound_physics/MakeConstraint` | 0.745633 | 0.766034 | 1.0274 | 1.0253 1.0274 1.0267 |
| `f7_n20000_bound_physics/MakeParameter` | 0.747248 | 0.765812 | 1.0248 | 1.0224 1.0262 1.0248 |
| `f7_n20000_bound_physics/RelaxBounds` | 0.747737 | 0.766940 | 1.0257 | 1.0186 1.0257 1.0294 |
| `f7_n2000_bound_neutral/MakeConstraint` | 0.091204 | 0.093714 | 1.0275 | 1.0307 1.0238 1.0277 |
| `f7_n2000_bound_neutral/MakeParameter` | 0.093843 | 0.096196 | 1.0251 | 1.0278 1.0251 1.0265 |
| `f7_n2000_bound_neutral/RelaxBounds` | 0.091964 | 0.092995 | 1.0112 | 1.0096 1.0112 1.0108 |
| `f7_n2000_bound_physics/MakeConstraint` | 0.067141 | 0.069159 | 1.0301 | 1.0127 1.0301 1.0356 |
| `f7_n2000_bound_physics/MakeParameter` | 0.066866 | 0.069341 | 1.0370 | 1.0393 1.0378 1.0317 |
| `f7_n2000_bound_physics/RelaxBounds` | 0.066777 | 0.069182 | 1.0360 | 1.0368 1.0342 1.0339 |
| `f7_n5000_bound_neutral/MakeConstraint` | 0.234757 | 0.241432 | 1.0284 | 1.0264 1.0239 1.0291 |
| `f7_n5000_bound_neutral/MakeParameter` | 0.240784 | 0.247101 | 1.0262 | 1.0266 1.0255 1.0256 |
| `f7_n5000_bound_neutral/RelaxBounds` | 0.235503 | 0.242212 | 1.0285 | 1.0261 1.0318 1.0297 |
| `f7_n5000_bound_physics/MakeConstraint` | 0.173193 | 0.177075 | 1.0224 | 1.0236 1.0225 1.0223 |
| `f7_n5000_bound_physics/MakeParameter` | 0.173816 | 0.180025 | 1.0357 | 1.0363 1.0357 1.0354 |
| `f7_n5000_bound_physics/RelaxBounds` | 0.174583 | 0.176998 | 1.0138 | 1.0171 1.0132 1.0158 |

#### R3 -- the process's FIRST row, excluded from the band by the pre-declared rule: `f7_n1000_bound_neutral/MakeParameter`

base runs 0.611087, 1.007717, 1.011716; head runs 0.507581, 0.488490, 0.484845; paired ratios 0.8306 0.4847 0.4792; median ratio **0.4847**. The base arm's own three runs span 1.656x.

corpus WITHOUT it (**PRIMARY**, 29 rows): base 10.1030 -> head 10.3545, ratio **1.0249** (+2.490 %); outside 0.99-1.01: **29**; band **MOVED**

corpus WITH it (30 rows, reported for completeness): base 11.1107 -> head 10.8430, ratio **0.9759** (-2.409 %); outside 0.99-1.01: **30**; band **MOVED**


**The millisecond-scale rows of this arm -- NO BAND (A7 (ii)).**

| row | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `hs071_x1_fixed/MakeConstraint` | 0.000137 | 0.000132 | 0.9652 | 0.9652 0.9837 0.9962 |
| `hs071_x1_fixed/MakeParameter` | 0.000179 | 0.000185 | 1.0302 | 1.0302 1.0554 1.0503 |
| `hs071_x1_fixed/RelaxBounds` | 0.000134 | 0.000138 | 1.0280 | 1.0358 1.0316 0.9381 |

**Counter identity across the two schemas (R4).** 33 common rows x 12 common counter columns x 3 rounds, `wall_s` excluded: `status` (33 rows)


### interior / b9848bf -> e51a7e0 (41 keys) / F7 rows (banded, leg-1 rules)

Keys only in the HEAD arm (head-only, informational, no band): `f7_n1000_bound_neutral/MakeConstraint/parts2`, `f7_n1000_bound_neutral/MakeParameter/parts2`

corpus base 11.2045 s -> head 10.9169 s, **ratio 0.9743** (-2.566 %); cells compared 32; outside 0.99-1.01: **8**; band **MOVED**

veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is informational) -- cells slower in ALL alternating rounds: **banded (also outside 0.99-1.01) 1 ['f7_n1000_bound_physics/RelaxBounds']**; strict, informational, 2 ['f7_n1000_bound_physics/RelaxBounds', 'f7_n2000_bound_neutral/MakeConstraint']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `f7_n10000_bound_neutral/MakeConstraint` | 0.539716 | 0.535106 | 0.9915 | 0.9910 0.9915 0.9866 |
| `f7_n10000_bound_neutral/MakeParameter` | 0.557155 | 0.557573 | 1.0007 | 1.0156 0.9879 1.0011 |
| `f7_n10000_bound_neutral/RelaxBounds` | 0.541245 | 0.533073 | 0.9849 | 1.0103 0.9836 0.9835 |
| `f7_n10000_bound_physics/MakeConstraint` | 0.373892 | 0.369068 | 0.9871 | 0.9858 0.9889 0.9871 |
| `f7_n10000_bound_physics/MakeParameter` | 0.372381 | 0.366945 | 0.9854 | 0.9847 0.9850 0.9869 |
| `f7_n10000_bound_physics/RelaxBounds` | 0.372182 | 0.367796 | 0.9882 | 0.9783 0.9962 0.9882 |
| `f7_n1000_bound_neutral/MakeConstraint` | 0.041090 | 0.041063 | 0.9993 | 0.9964 1.0000 1.0014 |
| `f7_n1000_bound_neutral/MakeParameter` | 0.721486 | 0.488490 | 0.6771 | 0.6999 0.6771 0.7021 |
| `f7_n1000_bound_neutral/MakeParameter/cap1` | 0.016816 | 0.016709 | 0.9937 | 0.9976 0.9876 0.9937 |
| `f7_n1000_bound_neutral/MakeParameter/solve_optimize` | 0.057414 | 0.057184 | 0.9960 | 0.9966 0.9943 0.9975 |
| `f7_n1000_bound_neutral/RelaxBounds` | 0.040878 | 0.040763 | 0.9972 | 0.9938 1.0006 0.9972 |
| `f7_n1000_bound_physics/MakeConstraint` | 0.032589 | 0.032625 | 1.0011 | 0.9965 0.9995 1.0015 |
| `f7_n1000_bound_physics/MakeParameter` | 0.032823 | 0.032621 | 0.9938 | 0.9954 0.9978 0.9918 |
| `f7_n1000_bound_physics/RelaxBounds` | 0.032642 | 0.033414 | 1.0237 | 1.0304 1.0201 1.0237 |
| `f7_n20000_bound_neutral/MakeConstraint` | 1.125829 | 1.115335 | 0.9907 | 0.9941 0.9907 0.9888 |
| `f7_n20000_bound_neutral/MakeParameter` | 1.162700 | 1.157094 | 0.9952 | 0.9971 0.9947 0.9947 |
| `f7_n20000_bound_neutral/RelaxBounds` | 1.113962 | 1.117852 | 1.0035 | 1.0000 1.0046 1.0047 |
| `f7_n20000_bound_physics/MakeConstraint` | 0.765394 | 0.766034 | 1.0008 | 0.9980 0.9883 1.0010 |
| `f7_n20000_bound_physics/MakeParameter` | 0.768054 | 0.765812 | 0.9971 | 1.0023 0.9966 0.9945 |
| `f7_n20000_bound_physics/RelaxBounds` | 0.771872 | 0.766940 | 0.9936 | 0.9902 0.9917 0.9963 |
| `f7_n2000_bound_neutral/MakeConstraint` | 0.093543 | 0.093714 | 1.0018 | 1.0042 1.0012 1.0019 |
| `f7_n2000_bound_neutral/MakeParameter` | 0.097103 | 0.096196 | 0.9907 | 0.9944 0.9896 0.9930 |
| `f7_n2000_bound_neutral/RelaxBounds` | 0.094696 | 0.092995 | 0.9820 | 0.9825 0.9722 0.9817 |
| `f7_n2000_bound_physics/MakeConstraint` | 0.069430 | 0.069159 | 0.9961 | 0.9949 0.9961 0.9959 |
| `f7_n2000_bound_physics/MakeParameter` | 0.069743 | 0.069341 | 0.9942 | 0.9964 0.9950 0.9931 |
| `f7_n2000_bound_physics/RelaxBounds` | 0.068940 | 0.069182 | 1.0035 | 1.0035 1.0047 0.9987 |
| `f7_n5000_bound_neutral/MakeConstraint` | 0.242183 | 0.241432 | 0.9969 | 0.9913 1.0015 0.9969 |
| `f7_n5000_bound_neutral/MakeParameter` | 0.247908 | 0.247101 | 0.9967 | 0.9971 0.9969 0.9899 |
| `f7_n5000_bound_neutral/RelaxBounds` | 0.243510 | 0.242212 | 0.9947 | 0.9943 0.9840 0.9947 |
| `f7_n5000_bound_physics/MakeConstraint` | 0.178808 | 0.177075 | 0.9903 | 0.9903 0.9899 0.9906 |
| `f7_n5000_bound_physics/MakeParameter` | 0.179528 | 0.180025 | 1.0028 | 1.0026 1.0028 0.9995 |
| `f7_n5000_bound_physics/RelaxBounds` | 0.178967 | 0.176998 | 0.9890 | 0.9890 0.9846 0.9932 |

#### R3 -- the process's FIRST row, excluded from the band by the pre-declared rule: `f7_n1000_bound_neutral/MakeParameter`

base runs 0.725169, 0.721486, 0.690592; head runs 0.507581, 0.488490, 0.484845; paired ratios 0.6999 0.6771 0.7021; median ratio **0.6771**. The base arm's own three runs span 1.050x.

corpus WITHOUT it (**PRIMARY**, 31 rows): base 10.4830 -> head 10.4284, ratio **0.9948** (-0.520 %); outside 0.99-1.01: **7**; band **UNRESOLVED**

corpus WITH it (32 rows, reported for completeness): base 11.2045 -> head 10.9169, ratio **0.9743** (-2.566 %); outside 0.99-1.01: **8**; band **MOVED**


**The millisecond-scale rows of this arm -- NO BAND (A7 (ii)).**

| row | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `hs071_x1_fixed/MakeConstraint` | 0.000132 | 0.000132 | 1.0014 | 1.0014 0.9998 1.0164 |
| `hs071_x1_fixed/MakeParameter` | 0.000185 | 0.000185 | 0.9977 | 1.0142 1.0320 0.9369 |
| `hs071_x1_fixed/MakeParameter/cap1` | 0.000080 | 0.000081 | 1.0123 | 1.1210 1.0123 0.9961 |
| `hs071_x1_fixed/MakeParameter/solve_optimize` | 0.000189 | 0.000193 | 1.0225 | 1.0336 1.0268 0.9714 |
| `hs071_x1_fixed/MakeParameter/warm_multiplier_seed` | 0.000114 | 0.000114 | 0.9990 | 1.0422 1.0136 0.9869 |
| `hs071_x1_fixed/MakeParameter/warm_payload` | 0.000088 | 0.000081 | 0.9185 | 0.9143 1.0149 0.9573 |
| `hs071_x1_fixed/RelaxBounds` | 0.000135 | 0.000138 | 1.0224 | 1.0301 1.0262 0.9289 |
| `infeas2_spike/MakeParameter/stalled` | 0.000596 | 0.000615 | 1.0316 | 1.0396 1.0301 1.0223 |
| `infeas2_stationary/MakeParameter/resto_infeasible` | 0.000192 | 0.000188 | 0.9795 | 0.9820 1.0358 0.9653 |

### The interior leg's WHOLE-PROCESS instruction counts -- NO VERDICT


passA, 102f729 -> e51a7e0: NO VERDICT -- the two arms' processes do not run the same rows (102f729 -> e51a7e0), so `perf stat`, which counts the PROCESS, is not like-for-like (the ratios below are reported, not classified)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 10185550 | 6015374 | 0.59058 |
| `branch-misses:u` | 36760235 | 37061251 | 1.00819 |
| `branches:u` | 15929087394 | 15721887456 | 0.98699 |
| `cycles:u` | 36828992412 | 36072082494 | 0.97945 |
| `instructions:u` | 105689362822 | 105996243990 | 1.00290 |

passB, 102f729 -> e51a7e0: NO VERDICT -- the two arms' processes do not run the same rows (102f729 -> e51a7e0), so `perf stat`, which counts the PROCESS, is not like-for-like (the ratios below are reported, not classified)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 36936430426 | 36164674051 | 0.97911 |
| `de_dis_uop_queue_empty_di0:u` | 1789657392 | 1343823176 | 0.75088 |
| `ic_fetch_stall.ic_stall_any:u` | 17409837844 | 17312844443 | 0.99443 |
| `instructions:u` | 105865732752 | 106092471488 | 1.00214 |
| `op_cache_hit_miss.op_cache_hit:u` | 14769236256 | 14313506412 | 0.96914 |
| `op_cache_hit_miss.op_cache_miss:u` | 1552461298 | 1593890078 | 1.02669 |

passA, b9848bf -> e51a7e0: NO VERDICT -- the two arms' processes do not run the same rows (b9848bf -> e51a7e0), so `perf stat`, which counts the PROCESS, is not like-for-like (the ratios below are reported, not classified)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 12776335 | 6015374 | 0.47082 |
| `branch-misses:u` | 36416305 | 37061251 | 1.01771 |
| `branches:u` | 15957343428 | 15721887456 | 0.98524 |
| `cycles:u` | 36615624316 | 36072082494 | 0.98516 |
| `instructions:u` | 106370985568 | 105996243990 | 0.99648 |

passB, b9848bf -> e51a7e0: NO VERDICT -- the two arms' processes do not run the same rows (b9848bf -> e51a7e0), so `perf stat`, which counts the PROCESS, is not like-for-like (the ratios below are reported, not classified)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 36719626956 | 36164674051 | 0.98489 |
| `de_dis_uop_queue_empty_di0:u` | 1502449495 | 1343823176 | 0.89442 |
| `ic_fetch_stall.ic_stall_any:u` | 17169050194 | 17312844443 | 1.00838 |
| `instructions:u` | 106587667943 | 106092471488 | 0.99535 |
| `op_cache_hit_miss.op_cache_hit:u` | 14664356373 | 14313506412 | 0.97607 |
| `op_cache_hit_miss.op_cache_miss:u` | 1603958582 | 1593890078 | 0.99372 |

### I4 -- the per-cell interior instruction measurement, LIKE-FOR-LIKE

Every `--engine interior` process also runs an UNCONDITIONAL row set (the `hs071_x1_fixed` treatments, the `cap1`/`solve_optimize`/warm variants, the two `infeas2` rows) whichever cell is requested, and at the head TWO MORE (`parts2`) that no flag suppresses -- so a bare per-cell process is NOT like-for-like across arms. The instrument that is: run the cell, run `--cells hs071_x1_fixed` (which adds no cell of its own and is exactly that unconditional set), and DIFFERENCE the two on the same arm. The unconditional set -- the head's `parts2` rows included -- and process start-up cancel, and what is left is that cell's own three rows.

The unconditional set alone (`--cells hs071_x1_fixed`), median of three:

| arm | rows | instructions | branches |
|---|---|---|---|
| arm102 | 3 | 2396707545 | 695033409 |
| armb98 | 11 | 5425387936 | 1450674362 |
| head | 13 | 5164860443 | 1246968246 |

The per-cell processes, whole-process `instructions:u`, median of three:

| cell | arm102 | armb98 | head |
|---|---|---|---|
| `f7_n10000_bound_neutral` | 18729936114 | 18049040224 | 18415106833 |
| `f7_n10000_bound_physics` | 12920831007 | 15044287895 | 15028802834 |
| `f7_n1000_bound_neutral` | 2240101602 | 6034014962 | 5924741034 |
| `f7_n1000_bound_physics` | 1470747580 | 5884510318 | 5753928824 |
| `f7_n20000_bound_neutral` | 36587967071 | 35149038449 | 35451423466 |
| `f7_n20000_bound_physics` | 24462415422 | 24544535955 | 24837051477 |
| `f7_n2000_bound_neutral` | 3377461414 | 6785385185 | 6858584637 |
| `f7_n2000_bound_physics` | 5905195162 | 6363825334 | 6341417673 |
| `f7_n5000_bound_neutral` | 11406289801 | 8983674785 | 9374751905 |
| `f7_n5000_bound_physics` | 7201618092 | 7868321267 | 8108470486 |

| cell | arm | instructions (differenced) | branches (differenced) | ratio vs head |
|---|---|---|---|---|
| `f7_n10000_bound_neutral` | arm102 | +16333228569 | +2362755633 | 0.81124 |
| `f7_n10000_bound_neutral` | armb98 | +12623652288 | +1279949432 | 1.04964 |
| `f7_n10000_bound_physics` | arm102 | +10524123462 | +1509516403 | 0.93727 |
| `f7_n10000_bound_physics` | armb98 | +9618899959 | +1240889898 | 1.02548 |
| `f7_n1000_bound_neutral` | arm102 | -156605943 | -232754219 | -4.85218 |
| `f7_n1000_bound_neutral` | armb98 | +608627026 | -11133352 | 1.24852 |
| `f7_n1000_bound_physics` | arm102 | -925959965 | -414575678 | -0.63617 |
| `f7_n1000_bound_physics` | armb98 | +459122382 | -12997665 | 1.28303 |
| `f7_n20000_bound_neutral` | arm102 | +34191259526 | +5120287570 | 0.88580 |
| `f7_n20000_bound_neutral` | armb98 | +29723650513 | +3810323908 | 1.01894 |
| `f7_n20000_bound_physics` | arm102 | +22065707877 | +3267084981 | 0.89153 |
| `f7_n20000_bound_physics` | armb98 | +19119148019 | +2398742486 | 1.02893 |
| `f7_n2000_bound_neutral` | arm102 | +980753869 | -137119323 | 1.72696 |
| `f7_n2000_bound_neutral` | armb98 | +1359997249 | -28051699 | 1.24539 |
| `f7_n2000_bound_physics` | arm102 | +3508487617 | +722282290 | 0.33535 |
| `f7_n2000_bound_physics` | armb98 | +938437398 | -24894815 | 1.25374 |
| `f7_n5000_bound_neutral` | arm102 | +9009582256 | +1543365637 | 0.46727 |
| `f7_n5000_bound_neutral` | armb98 | +3558286849 | -41916990 | 1.18312 |
| `f7_n5000_bound_physics` | arm102 | +4804910547 | +634345540 | 0.61263 |
| `f7_n5000_bound_physics` | armb98 | +2442933331 | -54034203 | 1.20495 |

**THE DIFFERENCING IS UNSOUND AND CARRIES NO VERDICT.** 9 of the differenced quantities are NEGATIVE -- a cell process with fewer instructions or branches than the unconditional row set it contains, which cannot happen if the set cost the same in both processes. It does not: run alone it is the process's first solve and pays MKL's first call, the allocator's first growth and the working set's page faults, which inside a cell process a large F7 solve has already paid. The offending entries are `f7_n1000_bound_neutral/arm102`, `f7_n1000_bound_neutral/armb98`, `f7_n1000_bound_physics/arm102`, `f7_n1000_bound_physics/armb98`, `f7_n2000_bound_neutral/arm102`, `f7_n2000_bound_neutral/armb98`, `f7_n2000_bound_physics/armb98`, `f7_n5000_bound_neutral/armb98`, `f7_n5000_bound_physics/armb98`. **No per-cell interior instruction verdict is issued.**

AND THE UNCONDITIONAL SETS ARE NOT COMPARABLE EITHER, which is the deeper reason: the head's 13-row set costs FEWER instructions (5164860443) than b9848bf's 11-row set (5425387936), so even the term being subtracted is not the same quantity across the arms. **A7 (ii)'s per-row instructions-only reading for the interior leg is NOT AVAILABLE from this harness**, by either route; the gap is REGISTERED for W6 with the rest of A7, and the numbers above are retained as data, not as a verdict.

**`hs071_x1_fixed` and the two `infeas2` rows carry NO like-for-like instruction verdict** and none is printed: they ARE the unconditional set, so there is nothing to difference them against, and the set itself differs between the arms by the two head-only `parts2` rows. Registered as a harness gap, not resolved here.

## Counter identity, leg 1 (recomputed from raw/)

| mode | cells | columns compared | rounds | differing columns |
|---|---|---|---|---|
| ipm | 27 | 75 | 3 | **NONE -- byte-identical** |
| ssn | 27 | 75 | 3 | **NONE -- byte-identical** |
| walk | 27 | 75 | 3 | **NONE -- byte-identical** |

## Instrument checks

The same arm, measured twice -- pass A against pass B. A population whose own two passes disagree by more than 1e-4 cannot carry a 1e-4 instruction verdict.

| population | arm | pass A instructions | pass B instructions | A/B |
|---|---|---|---|---|
| leg 1 / ipm / f7_n1000_bound_neutral | base | 749122644 | 749117947 | 1.00001 |
| leg 1 / ipm / f7_n1000_bound_neutral | head | 749466678 | 749470766 | 0.99999 |
| leg 1 / ipm / f7_n20000_bound_neutral | base | 15958318794 | 15958323295 | 1.00000 |
| leg 1 / ipm / f7_n20000_bound_neutral | head | 15965123025 | 15965123049 | 1.00000 |
| leg 1 / ipm / f7_n5000_bound_neutral | base | 3834878023 | 3834879144 | 1.00000 |
| leg 1 / ipm / f7_n5000_bound_neutral | head | 3836104607 | 3836109110 | 1.00000 |
| leg 1 / ssn / f7_n1000_bound_neutral | base | 488479834 | 488480134 | 1.00000 |
| leg 1 / ssn / f7_n1000_bound_neutral | head | 488903314 | 488897489 | 1.00001 |
| leg 1 / ssn / f7_n20000_bound_neutral | base | 11674701748 | 11674701519 | 1.00000 |
| leg 1 / ssn / f7_n20000_bound_neutral | head | 11683014698 | 11683019527 | 1.00000 |
| leg 1 / ssn / f7_n5000_bound_neutral | base | 2629037411 | 2629037407 | 1.00000 |
| leg 1 / ssn / f7_n5000_bound_neutral | head | 2630743395 | 2630738428 | 1.00000 |
| leg 1 / walk / f7_n1000_bound_neutral | base | 261301248 | 261301243 | 1.00000 |
| leg 1 / walk / f7_n1000_bound_neutral | head | 261644867 | 261644860 | 1.00000 |
| leg 1 / walk / f7_n20000_bound_neutral | base | 5640392312 | 5640391548 | 1.00000 |
| leg 1 / walk / f7_n20000_bound_neutral | head | 5645600279 | 5645594599 | 1.00000 |
| leg 1 / walk / f7_n5000_bound_neutral | base | 1333971634 | 1333977267 | 1.00000 |
| leg 1 / walk / f7_n5000_bound_neutral | head | 1335665712 | 1335664954 | 1.00000 |
| leg 2 / ipm / off | base | 247046537687 | 247046392527 | 1.00000 |
| leg 2 / ipm / off | head | 246930307513 | 247321839491 | 0.99842 |
| leg 2 / ipm / sink | base | 246877716760 | 247269938321 | 0.99841 |
| leg 2 / ipm / sink | head | 247153434708 | 247545370414 | 0.99842 |
| leg 2 / ssn / off | base | 420655887578 | 420010698648 | 1.00154 |
| leg 2 / ssn / off | head | 421876944130 | 421876775857 | 1.00000 |
| leg 2 / ssn / sink | base | 420047893460 | 420048270761 | 1.00000 |
| leg 2 / ssn / sink | head | 421269399170 | 421914926774 | 0.99847 |
| leg 2 / walk / off | base | 167530241085 | 167530258399 | 1.00000 |
| leg 2 / walk / off | head | 168052397121 | 168072062225 | 0.99988 |
| leg 2 / walk / sink | base | 167866546913 | 167927025244 | 0.99964 |
| leg 2 / walk / sink | head | 167708110355 | 168120338106 | 0.99755 |
| interior (whole process) | arm102 | 105689362822 | 105865732752 | 0.99833 |
| interior (whole process) | armb98 | 106370985568 | 106587667943 | 0.99797 |
| interior (whole process) | head | 105996243990 | 106092471488 | 0.99909 |

## Summary

Bands are R1-classified: the veto column is the BANDED count.

| leg | corpus ratio | cells outside 0.99-1.01 | band | banded veto cells | perf verdict |
|---|---|---|---|---|---|
| interior/102f729 | 1.0249 | 29/29 | MOVED | 28 | n/a |
| interior/b9848bf | 0.9948 | 7/31 | UNRESOLVED | 1 | n/a |
| leg1/ipm | 1.0006 | 0/27 | FLAT | 0 | WORK-MOVED (instructions UP) -- THE VETO |
| leg1/ssn | 0.9992 | 0/27 | FLAT | 0 | WORK-MOVED (instructions UP) -- THE VETO |
| leg1/walk | 1.0001 | 0/27 | FLAT | 0 | WORK-MOVED (instructions UP) -- THE VETO |
| leg2/ipm/off | 0.9913 | 21/27 | MOVED | 2 | NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)) |
| leg2/ipm/sink | 0.9861 | 26/27 | MOVED | 2 | NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) |
| leg2/ssn/off | 0.9822 | 17/27 | MOVED | 0 | NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) |
| leg2/ssn/sink | 0.9754 | 23/27 | MOVED | 0 | NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) |
| leg2/walk/off | 0.9931 | 8/27 | UNRESOLVED | 0 | NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) |
| leg2/walk/sink | 0.9761 | 24/27 | MOVED | 0 | NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by 0.2452 %, above the identity band; the ratio below is reported, not classified (original label: INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)) |

## 10. Attribution per task (T8.9r-attrib, 2026-09-11)

A later leg, on the same box and the same day, re-measured §3's nine cells at **eleven arms** — the
base and every group-1 code head in order — so §3's increase could be charged to the task that added
it, which is what §11.1.1 sends to the owner. It is in `attribution/`
(`attribution.md`, `arms.txt`, `table.csv`, `attribute.py` + its sha256, and every perf output, leg
log and script under `raw/`). **This addendum asserts no wall-clock number and none was taken**;
`cycles` there is informational, as here.

**All of §3's increase is T8.4, and T8.4 declared it.** Per-task instruction steps as a fraction of
the base arm; **bold** = outside the ±2e-5 band the eleven-arm leg reads at.

| task (code head) | ipm n1000/n5000/n20000 | ssn n1000/n5000/n20000 | walk n1000/n5000/n20000 |
|---|---|---|---|
| T8.1 `b3915ff` | +0.000000 / +0.000000 / +0.000000 | +0.000001 / +0.000000 / −0.000000 | **+0.000020** / −0.000000 / +0.000000 |
| T8.2 `b43580f` | +0.000000 / +0.000001 / +0.000000 | −0.000000 / +0.000000 / +0.000000 | −0.000019 / −0.000004 / −0.000000 |
| T8.3 `510a4bb` | +0.000001 / **−0.000125** / −0.000000 | **+0.000157** / +0.000000 / **+0.000130** | **+0.000024** / +0.000005 / **−0.000269** |
| **T8.4 `3c8e43b`** | **+0.000457 / +0.000442 / +0.000422** | **+0.000700 / +0.000641 / +0.000576** | **+0.001305 / +0.001264 / +0.001192** |
| T8.5 `8f95655` | +0.000002 / −0.000000 / +0.000000 | +0.000002 / +0.000000 / +0.000000 | +0.000002 / −0.000004 / +0.000000 |
| T8.6 `8cbaa39` | +0.000001 / +0.000000 / −0.000000 | −0.000009 / +0.000000 / −0.000000 | +0.000006 / +0.000005 / +0.000000 |
| T8.7 `56042be` | −0.000007 / −0.000001 / −0.000000 | +0.000011 / −0.000000 / +0.000000 | −0.000004 / −0.000000 / −0.000000 |
| T8.7b `ddac2cf` | −0.000004 / +0.000000 / −0.000000 | +0.000000 / +0.000000 / −0.000000 | +0.000003 / +0.000000 / −0.000000 |
| T8.8 `b9848bf` | +0.000014 / +0.000006 / +0.000004 | +0.000007 / +0.000007 / +0.000006 | −0.000003 / +0.000001 / −0.000000 |
| T8.9 `e51a7e0` | +0.000000 / +0.000000 / −0.000000 | +0.000001 / +0.000000 / +0.000000 | **−0.000021** / −0.000000 / +0.000000 |
| **cumulative** | **+0.000465 / +0.000323 / +0.000426** | **+0.000867 / +0.000649 / +0.000713** | **+0.001314 / +0.001265 / +0.000923** |

The steps sum to the end-to-end delta on every cell (residual < 1e-9), and the cumulative row
reproduces §3's ratios: 1.00046 / 1.00032 / 1.00043, 1.00087 / 1.00065 / 1.00071, 1.00131 / 1.00127 /
1.00092.

**The T8.4 step is one fixed `O(n)` cost per call, the same on all three QP tiers** — +342 533
(ipm), +341 797 (ssn), +340 870 (walk) instructions at n = 1000, ≈1.688 M at n = 5000, ≈6.73 M at
n = 20000; linear in `n` to better than 2 %, mode-independent to better than 0.5 %. **That is why §3's
fraction differs by mode**: one number divided by three different totals, largest as a fraction on
walk because walk is the cheapest solve. §3's "+0.03 %…+0.13 %" is one quantity, not nine. The
mechanism is the one T8.4's design §2.3 and §2.7 item (7) and its ledger close line declare — the
shared declared diagnostics computed once per call over the declared NLP
(`compute_declared_diagnostics`) and the declared-space vectors the new `SolveResult` carries by
value. It executes per call and is linear in the declared dimension, which is the shape measured.

**UNDECLARED list: EMPTY.** No task added an instruction step that leg can resolve which its design
or ledger close line does not declare. Two riders, both stated there in full:

* The non-T8.4 movements outside the band — T8.3 on four cells, and three ±2e-5 flags on walk/n1000
  — are **code-layout cluster transitions, not work**, identified by a branch-density fingerprint the
  leg measures rather than assumes: a layout cluster moves branches by ≈2.4× the instruction
  fraction, executed work by ≈1.1×. Every T8.4 step reads 1.05–1.11; every other step outside the
  band reads 2.32–2.48; nothing lands between. One of them is a control — T8.1's `libhven.a` is
  byte-identical to the base's, so its +2.0e-5 on walk/n1000 is that cell's floor and can be nothing
  else.
* **§4's floor is a WITHIN-layout floor, and this reading's leg 1 is unaffected by that.**
  `instructions:u` on these cells is bimodal in the byte footprint of the measured process's argv and
  environment: `attribution/raw/logs/A12-layout-probe.log` moves one binary between clusters ~1.2e-4
  apart on one cell by changing nothing but the length of an output path. Leg 1's two arms were
  length-matched by construction (`arm-base`/`arm-head`, `A-base-`/`A-head-`), so both sat in one
  cluster and the term cancelled — §3 stands. What the probe adds is the controlling variable, and
  the refinement that branch proportionality alone does not separate work from a layout cluster (a
  cluster moves branches too); it is the *value* of the ratio that does, and §3's cells sit at the
  work value.

**Calibration, declared in both forms.** All three `libhven.a` `PROVENANCE.txt` retained rebuild
BYTE-IDENTICALLY under that leg's recipe (`735eea1d…`, `d236166e…`, `60bfe03f…`), and its end-to-end
head/base ratios reproduce §3's on all nine cells to within **1.87e-06**. In ABSOLUTE counts it
reproduces §3's numbers to 2e-5 on 13 of the 18 end-arm cells and not on the other 5 — the three
smallest cells, worst +4.16e-05, every deviation positive and common-mode between base and head of
the same cell to within 2e-6, which is the residual of the process-layout term above and is why the
ratio form is untouched by it. Apple/Accelerate and Windows: UNOBSERVED, as everywhere here.

---
## 11. The interior leg's movement per task (T8.9r-attrib2, 2026-09-11)

§5's interior leg said the top-level IPM got **2.49 % slower across the whole of group 1**, on 29 of
29 banded rows, with the twelve common counter columns **identical** — and could not say which task
did it, having no arm between `102f729` and `b9848bf`. A later leg, same box, same day, put an arm at
**every one of the eleven group-1 heads** and measured the per-row **wall** at each: five rounds, arm
order rotated per round, `--engine interior` on four dual-binding F7 cells × three treatments. It is
in `attribution-interior/` (`attribution-interior.md`, `arms.txt`, `wall.csv`, `perf.csv`,
`attribute_interior.py` + its sha256 and saved output, `IDLE-PROOF.md`, and every CSV, perf output,
batch log and script under `raw/`). It built nothing: the eleven arm binaries are `attribution/`'s,
re-verified by sha256 before use, 11/11 and 11/11.

**IT IS T8.4, AND NOTHING ELSE IS CLOSE.** Per-row wall step, median of five rounds; **bold** = above
1.01. The first row every process writes is excluded by the pre-declared positional rule, and the cell
order was chosen so that row is the SAME row at every arm (`f7_n1000_bound_physics/MakeParameter`);
eleven rows are scored.

| row | T8.1 | T8.2 | T8.3 | **T8.4** | T8.5 | T8.6 | T8.7 | T8.7b | T8.8 | T8.9 | cumul |
|---|---|---|---|---|---|---|---|---|---|---|---|
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
| **rows above 1.01, of 11** | 0 | 0 | 0 | **11** | 0 | 0 | 1 | 0 | 0 | 0 | — |
| **median share of the cumulative** | +0.07 | −0.05 | −0.02 | **+1.03** | −0.20 | +0.14 | +0.02 | +0.00 | +0.14 | −0.11 | — |
| **whole-process instruction step** | *+0.0127 (FLOOR)* | +0.0040 ‡ | +0.0020 | *+0.0101* ‡ | −0.0049 ‡ | +0.0030 | −0.0031 | +0.0003 | +0.0022 | +0.0128 ‡ | — |

‡ = ROW-ADDING pair; **no instruction verdict** (T8.2, T8.4, T8.5 and T8.9 each add rows, so the two
arms' processes do not run the same work). The six unmarked columns are like-for-like, verified from
each arm's row count (15/15/19/19/21/23/23/23/23/23/25).

**THE CUMULATIVE REPRODUCES §5:** 1.0250 over the eleven scored rows against §5's **1.0249**, from a
different day's rounds and a different cell set. The per-row columns of the two legs differ by up to
2.9 points — five medians here against three there, a per-process layout term in both — and that is
reported rather than smoothed; what reproduces is the corpus ratio and the shape.

**THE LOCALISATION.** *All of the interior leg's +2.49 % is T8.4*: it is the only task whose step
exceeds 1.01 on any scored row, it does so on **all eleven**, its median share of the cumulative is
**+1.03** (the other nine net slightly negative), and it is at or above 0.80 on 9 of 11 rows while
every other task reaches 0.80 on none.

**AND THE INSTRUCTION CURRENCY RETURNS NO VERDICT ON THIS LEG, IN EITHER PASS.** T8.4's pair is
row-adding, which forbids one outright; and independently the floor of the whole-process count here,
read off the control arm whose `libhven.a` is **byte-identical** to the base's, is **+1.27 %** —
larger than every like-for-like step measured and larger than T8.4's own +1.01 %. Pass B is worse
still: the control pair moves further than the pair under test on five of the six Zen 3 front-end
events and further on IPC. The floor is a hundred times the SQP legs' 2e-5 because the whole-process
count is dominated by the warm-up row the wall reading excludes and `perf stat` cannot; a single-row
interior process would fix it and **is not reachable without a source change** —
`--internal-run-one` rejects `interior` (`bench/corpus_cells.h:1843`) — so §5 (iv)'s W6 registration
stands, with this floor measurement added to it.

**WHAT IS AVAILABLE IN PLACE OF A VERDICT, DECLARED AS THE DERIVATION IT IS.** T8.4's whole-process
instruction step (**0.576 e9**) is accounted for, to **2.3e-4 of the process**, by the two rows T8.4
*added* (0.563 e9, estimated from those rows' own wall at the earlier arm's own instruction rate;
residual +0.008 to +0.026 e9 across the arms' extreme rates). A WORK increase on the nineteen shared
rows matching T8.4's median wall step of +3.00 % would need about **1.716 e9** — 66 to 214 times the
residual. So the nineteen rows both arms run cost the same instructions, take 2–4 % more wall, and
§5 already established their twelve counter columns are identical end to end. **That is §11.1's
LAYOUT-MOVED signature, not WORK-MOVED — as a derivation from a stated estimate, not a measurement.**

**THE MECHANISM IS CODE PLACEMENT, AND IT IS NOT THE DECLARED DIAGNOSTICS.** T8.4's own new per-call
work, `compute_declared_diagnostics`, is called **once per solve** at
`src/drivers/interior_point_solver.cpp:2215`; §10's leg measured that same commit's cost at ≈342 500
instructions at n = 1000, linear in the declared dimension, so ≈6.7 M at n = 20000 against the
≈10.6 e9 an `f7_n20000` row executes — **0.06 % of a row whose wall moved 2.0 %**. What moves
addresses is the rest of the same commit: it rewrote `src/drivers/interior_point_solver.cpp` (1101
lines — the TU carrying the IPM iteration loop) and **inserted a new translation unit**,
`drivers/solve_result.cpp`, at `src/CMakeLists.txt:76`, taking the library's source count 42 → 43.
A new object in the archive and a rewritten hot TU relocate everything that follows them.

**This addendum ASSERTS WALL CLOCK, under the fix1 R2 discipline of §8.1; the solo evidence is in
`attribution-interior/` per batch** — sixteen timed batches, all **PINNED-CLEAN**, `scripts/idle_proof.py`
exits 0 over all of them, and **the five wall batches, the only ones asserting wall clock, read
foreign un-niced user time on the pinned core of EXACTLY ZERO, every one**. Five batches were re-run
after failing their window and the re-runs are the retained data; nothing was ever signalled.
**No disposition is offered — §11.1 and the owner have it.** Apple/Accelerate and Windows: UNOBSERVED.

---

## 12. Inside T8.4 (T8.9r-attrib3, 2026-09-11)

§11 charged the interior leg's **+2.49 %** to T8.4 — the only group-1 task whose
per-row wall step exceeded 1.01 on any scored row, and it exceeded it on all eleven —
and could go no further: its arms sat at task HEADS, and T8.4 is seven commits. A third
leg, same box, same day, put an arm at **every T8.4 library commit** and measured the
per-row wall at each: five rounds, arm order rotated per round, `--engine interior` on
the same four dual-binding F7 cells × three treatments, solo, the R3 positional warm-up
row excluded by a rule hashed before the first sample existed. It is in
`attribution-interior/t84/` (`arms.txt`, `steps.md`, `mechanism.md`, `layout.txt`,
`predeclaration.txt`, `wall.csv`, `experiments.csv`, `mechanism-tables.txt`, the three
analysis tools with their sha256s and saved output, three idle proofs, five experiment
patches, and every CSV, perf output, batch log and script under `raw/`, `perf/`,
`logs/` and `scripts/`). It built its own eleven arms; both end arms' `libhven.a`
reproduce `attribution/arms.txt` **byte for byte**.

**IT IS `9cebbbe`** — *IpmResult; `solve(model, x0, budget)` with phases; the model
borrowed per call; the five entries, the mutable accessors and `ConvergenceFlags`
removed*. Per-row wall step, median of five rounds, over the eleven scored rows:

| | 8ae1618 | **9cebbbe** | 9ce9bb2 | fix1 5124aa1 | 3c8e43b | cumul |
|---|---|---|---|---|---|---|
| median step | 0.9995 | **1.0324** | 1.0028 | 0.9950 | 1.0009 | **1.0283** |
| rows above 1.01, of 11 | 0 | **11** | 0 | 0 | 0 | — |
| median share of the cumulative | −0.01 | **+1.02** | +0.10 | −0.14 | +0.04 | — |

The cumulative **1.0283** reproduces §11's 1.0250 and §5's 1.0249 from a third round
set. **Two of the five steps are EXACTLY ZERO by construction** — `510a4bb → 8ae1618`
and `5124aa1 → 3c8e43b` each produced a **byte-identical `hven_sqp_corpus`** — and they
read 0.9995 and 1.0009, median |ln step| 0.00075 and 0.00087, against the carrier's
0.03189: **36.5× the floor**, on a control stronger than §11's (a byte-identical
binary, not merely a byte-identical library).

**AND THE MECHANISM §11 NAMED IS REFUTED IN BOTH HALVES.** §11 read the movement as code
placement — "a new object in the archive and a rewritten hot TU relocate everything that
follows them". (a) `8ae1618` is the commit that adds `drivers/solve_result.cpp` to
`src/CMakeLists.txt` and takes the source count 42 → 43; `ar t` shows the new member
inserted at position 13 of 43, and **the linked executable is byte-identical** — nothing
references it there, and a linker does not pull an unreferenced archive member in.
(b) Inserting 4 096, 9 712 or 16 384 bytes of unreachable `.text` at the head of
`src/drivers/interior_point_solver.cpp` at the parent — 9 712 being exactly the amount
the culprit grew that object by — moves the scored-row corpus by **+0.05 %, +0.19 % and
+0.01 %**, reproducing 1.5 %, 6.4 % and 0.3 % of the step. This box is not generically
placement-sensitive at this scale, and two later commits that rewrote the same files
(`9ce9bb2`, 280 lines of `sqp_driver.cpp`; fix1, 302 lines of the IPM's own TU) cost
+0.28 % and **−0.50 %**.

**THE MECHANISM, AS FAR AS IT IS PROVEN.** `9cebbbe`'s hot loop is a rename almost line
for line — 31 of 70 diff hunks survive filtering the commit's rename set, and none of
the survivors is inside the iteration; `eval_nlp` compiles to the byte. What the commit
does change is the result core's **lifetime**: where the parent handed the result out by
reference and kept its buffers across calls
(`include/hven/drivers/interior_point_solver.h:511`, `:360`), the culprit
default-constructs it at every entry and moves it out at every exit
(`src/drivers/interior_point_solver.cpp:4263` and `:5060`), so every buffer it owns is
freed and re-allocated inside each solve. The measured consequence is **+53 149 minor
page faults per process, +17.2 %, against a control-pair floor of 1 count in 308 106**
— the tightest instrument in this artifact — with **+0.090 s of kernel time** beside it,
counted in the wall leg's own condition with no `perf` attached. Telling glibc to stop
returning large blocks to the kernel, applied **identically to both arms**, removes
**99.7 % of the extra faults and 32.8 % of the step** (+2.999 % → +2.006 %).

**THAT IS A THIRD OF IT, AND THE REST IS NOT EXPLAINED.** Four experiments were run, each
a scratch build of a patched `git archive` extraction, each leaving all nineteen CSV
columns on all nineteen rows bit-identical to its unpatched base: restoring the culprit's
hot members to the parent's exact byte offsets recovers **16.8 %**; moving the parent's to
the culprit's reproduces **4.4 %**; the three code shifts reproduce **0.3–6.4 %**; the
allocator intervention removes **32.8 %**. The remainder sits in user time, spread across
MKL's own Pardiso kernels (66 % of the profile, unchanged source) and Eigen's assembly in
**unchanged proportion** — no function got slower relative to the others, and the
whole-process instruction and cycle counters return no verdict because the
byte-identical control pair moves them **+1.02 %** and **+1.81 %** where the pair under
test moves them −0.13 % and +0.71 %. §11's LAYOUT-MOVED derivation is therefore **not
refuted as a description** — the work is still identical and the cost is still not in any
named function — but its stated cause is, and a named per-call mechanism now carries a
third of it. **No committed source changed; the experiment patches are evidence, not a
fix.**

**This addendum ASSERTS WALL CLOCK, under the fix1 R2 discipline of §8.1.** Twenty-five timed
wall batches across five legs, **all PINNED-CLEAN**, `scripts/idle_proof.py` exits 0 over
each of the five; foreign un-niced user time on the pinned core reads **exactly zero on
twenty of the twenty-five** and never exceeds 0.050 s (0.1292 %) on the rest. Nine batches
failed their window on a first pass and were re-run; the re-runs are the retained data, and one
further experiment round set was discarded and re-run for breaking the argv lock by one
byte — it is retained, unedited, with its README. Nothing was ever signalled. **No
disposition is offered — §11.1 and the owner have it.** Apple/Accelerate and Windows:
UNOBSERVED.
