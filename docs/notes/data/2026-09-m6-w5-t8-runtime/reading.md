# W5 T8.9r — the group-1 runtime reading

`102f729 → e51a7e0` (the three SQP arms) and `b9848bf → e51a7e0` (the interior
leg), solo. Read `PROVENANCE.txt` first: it carries the arms, the hashes, the
hardware, the solo protocol and — as importantly — what this leg does **not**
measure.

Every table below is regenerated from `raw/` and `perf/` by
`python3 comparator.py --root . --out -`; the tool's own output is reproduced
verbatim at the end of this file. Nothing here is hand-transcribed.

**This is a measurement, not a disposition.** §11.1.1 puts the disposition with
the settler and the owner, and two of the three findings below need one.

---

## 1. The short version

| leg | mode | corpus ratio | cells outside 0.99–1.01 | band | instruction verdict |
|---|---|---|---|---|---|
| **leg 1** (U0, 27 cells, wall) | ipm | **0.99992** (14.8418 → 14.8406 s) | 0/27 | **FLAT** | **instructions UP +0.032…+0.046 %** |
| leg 1 | ssn | **0.99990** (11.0930 → 11.0918 s) | 0/27 | **FLAT** | **instructions UP +0.065…+0.087 %** |
| leg 1 | walk | **1.00093** (6.0222 → 6.0278 s) | 0/27 | **FLAT** | **instructions UP +0.092…+0.133 %** |
| **leg 2** (27 HS, `--repeat`) | ipm off / sink | 0.9917 / 0.9912 | 23/27, 24/27 | **MOVED (faster)** | no verdict at 1e-4 — see §4 |
| leg 2 | ssn off / sink | 0.9782 / 0.9816 | 20/27, 17/27 | **MOVED (faster)** | no verdict at 1e-4 |
| leg 2 | walk off / sink | 0.9798 / 0.9829 | 24/27, 16/27 | **MOVED (faster)** | no verdict at 1e-4 |
| **interior** (F7 rows) | — | **1.0002** | 6/31 | **UNRESOLVED** | no verdict at 1e-4 — see §5 |

Three things follow, and they are independent of one another.

**(a) THE WALL IS FLAT ON THE WALL-CLOCK LEG.** Leg 1 — the only leg §11.3
leaves as the wall-clock leg — is FLAT in all three modes, 0/27 cells outside
0.99–1.01 in every mode, every corpus figure inside ±0.1 %, an order of
magnitude inside the ±0.5 % bar. That is the strongest wall reading any W5 leg
has produced.

**(b) AND INSTRUCTIONS ARE UP ANYWAY — WORK-MOVED, THE VETO.** On every one of
the nine leg-1 perf cells the head arm executes more instructions than the base,
by 0.03 % (ipm) to 0.13 % (walk) — 3× to 13× outside §11.1's 1e-4 identity band,
and 16× to 66× outside this leg's own measured noise floor (§4). §11.1.1's
OVERLAP 1 is explicit that a FLAT timing does not excuse this: *"does
demonstrated added work trigger the veto even when the timing meets FLAT?
ANSWER: YES. There is NO automatic KEEP."* **The veto trigger has fired, and the
numbers go to the owner.** §6 is the attribution the owner will want.

**(c) THE INTERIOR LEG IS UNRESOLVED, AND TWO OF ITS CELLS ARE A CANDIDATE.**
Corpus 1.0002 — flat — but 6 of 31 banded rows sit outside 0.99–1.01 with mixed
signs, and two of them are slower in all three alternating rounds. Its
instruction comparison cannot classify them (§5), so under §11.1 the leg is
UNRESOLVED pending re-measurement, not LAYOUT-MOVED.

---

## 2. Leg 1 — the wall, in the T6 close reading's shape

Recipe: the U0 27-cell corpus, `--engine walk|ssn|ipm`, 3× alternating
A/B/A/B/A/B, solo, `taskset -c 2`, `MKL_NUM_THREADS=OMP_NUM_THREADS=1`, the lock
held for the whole sequence. Per-cell median of the three runs; corpus = the sum
of the per-cell medians.

| mode | corpus | per-cell envelope | outside 0.99–1.01 | slowest cell | fastest cell |
|---|---|---|---|---|---|
| ipm | **0.99992** (14.8418 → 14.8406 s) | 0.9922–1.0048 | **0/27** | `f7_n1000_bound_activity` 1.0048 | `f7_n1000_bound_physics` 0.9922 |
| ssn | **0.99990** (11.0930 → 11.0918 s) | 0.9952–1.0073 | **0/27** | `f7_n5000_bound_corrupted` 1.0073 | `f7_n1000_bound_neutral` 0.9952 |
| walk | **1.00093** (6.0222 → 6.0278 s) | 0.9918–1.0098 | **0/27** | `f7_n2000_bound_corrupted` 1.0098 | `f7_n5000_bound_neutral` 0.9918 |

**NO CELL IS OUTSIDE 0.99–1.01 IN ANY MODE.** The full per-cell tables, with the
three paired per-round ratios beside every median, are in §7.

**AND THE COUNTERS ARE BYTE-IDENTICAL.** Independently of the T8 identity
replays, this leg's own CSVs were compared column by column between the arms —
and the comparator recomputes it from `raw/`, so the reading does not take it on
trust from another task: **27 cells × 75 columns × 3 rounds × 3 modes, `wall_s`
excluded, ZERO differences.** Same factorizations, same QP minors, same escapes, same KKT
residuals to the last printed digit. The two arms do the same work in the same
number of steps — which is exactly what makes (b) a statement about how that
work is executed rather than about how much of it there is.

### The veto check on leg 1

§11.1's sentence — *"a reproducible slowdown — three solo runs, same sign — on
ANY cell"* — admits two readings, and the comparator prints both rather than
choosing for the reader:

| mode | strict: cells slower in all three rounds | banded: also outside 0.99–1.01 |
|---|---|---|
| ipm | 4 — `f7_n1000_bound_activity`, `f7_n20000_bound_corrupted`, `f7_n20000_bound_warm`, `f7_n800_path_warm` | **0** |
| ssn | 3 — `f7_n1000_bound_activity`, `f7_n1000_path_warm`, `f7_n10000_bound_warm` | **0** |
| walk | 7 — `f7_n1000_bound_warm`, `f7_n2000_bound_corrupted`, `f7_n5000_bound_corrupted`, `f7_n5000_bound_warm`, `f7_n20000_bound_corrupted`, `f7_n20000_bound_physics`, `f7_n20000_bound_warm` | **0** |

The strict reading fires on sampling alone: with 27 cells and a fair coin per
round, ~3.4 cells are expected same-sign by chance. **On the banded reading —
outside the FLAT band AND slower in all three rounds — leg 1 has zero cells in
any mode.** The wall side of the veto is clean. The instruction side is not.

---

## 3. Leg 1 — pass A and pass B, and the instruction finding

Pass A: `instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u`.
Pass B (Zen 3): `instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u`.
Three designated cells per mode, each a single process via `--internal-run-one`,
3× alternating, same discipline as the wall.

| mode | cell | instructions | branches | cycles | L1-icache misses | verdict |
|---|---|---|---|---|---|---|
| ipm | n1000 | **1.00046** | 1.00051 | 0.99678 | 0.757 | WORK-MOVED |
| ipm | n5000 | **1.00032** | 1.00019 | 0.99938 | 0.842 | WORK-MOVED |
| ipm | n20000 | **1.00043** | 1.00046 | 1.00026 | 0.698 | WORK-MOVED |
| ssn | n1000 | **1.00087** | 1.00110 | 1.00010 | 0.721 | WORK-MOVED |
| ssn | n5000 | **1.00065** | 1.00068 | 0.99835 | 0.698 | WORK-MOVED |
| ssn | n20000 | **1.00071** | 1.00091 | 1.00207 | 0.686 | WORK-MOVED |
| walk | n1000 | **1.00131** | 1.00144 | 1.00086 | 0.907 | WORK-MOVED |
| walk | n5000 | **1.00127** | 1.00138 | 1.00158 | 0.931 | WORK-MOVED |
| walk | n20000 | **1.00092** | 1.00065 | 1.00156 | 0.937 | WORK-MOVED |

Branches move with instructions, by the same fraction, in every cell — so this
is executed work, not a mis-attributed counter. Cycles stay inside ±0.35 % and
L1-icache misses fall by 6–31 %: the extra instructions are cheap ones that the
front end absorbs, which is why the wall does not see them. **That is an
explanation of why the timing is FLAT. It is not a defence against the veto,
which §11.1 states on instructions.**

Pass B agrees and adds nothing that changes the classification: the front-end
share (`de_dis_uop_queue_empty_di0/cycles`) and the op-cache miss share move by
fractions of a point in both directions across the nine cells. The full tables
are in §7.

### The absolute deltas — the increase SCALES with the problem

| mode | cell | base instructions | head instructions | delta | ratio |
|---|---|---|---|---|---|
| ipm | n1000 | 749 104 475 | 749 451 809 | +347 334 | 1.00046 |
| ipm | n5000 | 3 834 858 737 | 3 836 095 428 | +1 236 691 | 1.00032 |
| ipm | n20000 | 15 958 310 647 | 15 965 108 235 | +6 797 588 | 1.00043 |
| ssn | n1000 | 488 466 427 | 488 889 622 | +423 195 | 1.00087 |
| ssn | n5000 | 2 629 019 851 | 2 630 729 726 | +1 709 875 | 1.00065 |
| ssn | n20000 | 11 674 687 728 | 11 683 005 904 | +8 318 176 | 1.00071 |
| walk | n1000 | 261 282 683 | 261 625 450 | +342 767 | 1.00131 |
| walk | n5000 | 1 333 963 362 | 1 335 651 610 | +1 688 248 | 1.00127 |
| walk | n20000 | 5 640 377 762 | 5 645 586 494 | +5 208 732 | 1.00092 |

The delta is not a constant per process: it grows roughly twentyfold as the
problem does. Whatever it is, it is charged per unit of work, not once at
startup. §6 says where it is.

---

## 4. The instrument's own floor — and why only leg 1 may speak about instructions

`instructions` and `cycles` appear in BOTH passes precisely so the two can be
tied to each other (§11.2). That tie is also a control: **when the same arm's
two passes of the SAME binary disagree by more than 1e-4, the identity band sits
below that leg's noise floor and no instruction verdict at 1e-4 is available
from it.** This reproduces, and extends, T6.d's finding F1 — *"`instructions:u`
is BIMODAL per process on HS (0.16–0.22 % between clusters; the identity band
sits at the noise floor; pair cluster-wise)"*.

| leg | worst same-arm pass A / pass B disagreement | verdict at 1e-4? |
|---|---|---|
| **leg 1** (all nine cells, both arms) | **0.002 %** (worst 1.00002) | **YES** |
| leg 2 (six mode × trace combinations) | **0.217 %** | **NO** |
| interior | **0.557 %** | **NO** |

Leg 1's instrument reproduces to 2e-5 — five times finer than the band it is
being asked about — and the head-vs-base signal is 16× to 66× larger than that
floor.
**Leg 1's instruction finding is real.** Leg 2's and the interior leg's are
inside their own noise and are reported without classification; the comparator
labels them `NO VERDICT AT 1e-4` and prints the ratio it would otherwise have
classified.

One leg-2 combination (ssn / trace=sink) happens to land in the same bimodal
cluster on both passes. That is a coincidence of clustering on a population
known to be bimodal, not a sound instrument, so the floor is applied at the LEG
level and that combination is not promoted on its own. The comparator says so in
its own comment.

---

## 5. Leg 2 and the interior leg — what they do and do not say

### Leg 2 (the 27 HS problems)

All six mode × trace combinations read **MOVED in the FASTER direction**, from
−0.8 % (ipm) to −2.2 % (ssn), with 16–24 of 27 cells outside 0.99–1.01.

Two caveats, both of which the protocol wrote down before this leg ran:

* §11.3 (ii) records that three runs of the SAME binary disagree by up to
  **1.4 %** at the walk corpus level on this leg, because at 0.3–3 ms the
  between-run variation is a per-process constant — address layout, allocator
  state, page placement — that `--repeat` cannot average away at any N. Most of
  the movement above is inside that. **Leg 2's absolute wall figure cannot
  resolve the 0.5 % effect the veto turns on, and §11.3 (ii) says its wall is
  informational and read only as the paired A/B ratio.**
* What is *not* inside that caveat is the SIGN. All six combinations moved the
  same way. Six same-direction readings is more than one instrument's noise, and
  it is recorded as such: on the HS scale the head is plausibly genuinely
  faster. It is not quoted as a magnitude.

Leg 2's verdict was supposed to rest on instructions and branches (§11.3 (ii)).
It cannot, here: §4 shows this leg's own noise floor is 0.217 %, above the band.
**So leg 2 returns no verdict in either currency.** Both mapper call sites were
exercised — the trace=sink arm carries a real sink and is reported separately
from the null-sink arm, never averaged with it.

The per-cell veto check on leg 2: 3 cells (`hs12`, `hs15`, `hs25`) are slower in
all three rounds on the ipm arms, 2 of them also outside the band; ssn/off has
one (`hs25`); the other three combinations have none.

### The interior leg (b9848bf → e51a7e0)

Base statement, in full, is in `PROVENANCE.txt`. In short: the leg does not
exist at 102f729, so its base is the T8.9 close's predecessor b9848bf, its 41
rows are the join against the head's 43, and what it measures is T8.9's harness
rewrite and nothing else. The two `parts2` rows are head-only and carry no band.

**The band: corpus 1.0002, and UNRESOLVED.** Six of the 31 banded rows sit
outside 0.99–1.01:

| row | ratio | paired per round | direction |
|---|---|---|---|
| `f7_n1000_bound_physics/RelaxBounds` | 1.0219 | 1.0168 1.0253 1.0223 | slower, all three |
| `f7_n10000_bound_physics/MakeParameter` | 1.0129 | 1.0139 1.0119 1.0143 | slower, all three |
| `f7_n20000_bound_physics/MakeConstraint` | 1.0109 | 1.0088 1.0118 0.9984 | slower, two of three |
| `f7_n5000_bound_physics/MakeParameter` | 0.9806 | 0.9800 0.9837 0.9823 | faster, all three |
| `f7_n5000_bound_physics/MakeConstraint` | 0.9853 | 0.9830 0.9864 0.9861 | faster, all three |
| `f7_n5000_bound_physics/RelaxBounds` | 0.9892 | 0.9898 0.9888 0.9877 | faster, all three |

Mixed signs within one taxonomy (`_physics`), at one size class going one way
and at another going the other — the shape of placement, not of work. But the
instruction measurement that would settle that is unavailable here (§4 and
below), so under §11.1 the correct label is **UNRESOLVED pending
re-measurement**, and two cells — the two slower-in-all-three ones outside the
band — are a **veto candidate on the banded reading**, not a veto.

**AND ONE ROW IS EXCLUDED FROM THE BAND, DECLARED.**
`f7_n1000_bound_neutral/MakeParameter` is the FIRST row the process writes, and
it pays the one-time cost of everything under it — MKL's first call, the
allocator's first growth, the page faults for the working set. The BASE arm's
own three runs of that row are **0.998575, 0.330953, 0.316802 s — a factor of
3.2 on one binary** — while its sibling row two lines below is stable to 0.3 %.
A quantity whose own arm disagrees with itself threefold cannot carry a 1 %
band. It is reported here in full and left out of the corpus figure; nothing
else is excluded. **With it included the interior corpus reads 1.0153 (+1.53 %)
and that single row supplies essentially all of the movement** — which is why
leaving it in would have been the misleading choice, not the conservative one.
Both figures are printed by the comparator.

**AND THE INTERIOR INSTRUCTION COMPARISON IS NOT LIKE-FOR-LIKE.** The head arm's
leg runs 43 rows and the base arm's 41; no flag suppresses the two `parts2`
rows. `perf stat` counts the process, so the +0.82 % instruction ratio charges
the head with two solves the base never ran. Those rows are **0.71 %** of the
head arm's measured wall — the same order as the instruction difference. The
interior leg therefore carries **no instruction verdict**, and its raw counters
are printed for the record only.

---

## 6. Attribution — where leg 1's extra instructions are

The veto turns on instructions, so it matters whether the extra work is inside
the solve the wall band measures or in the process around it. `perf stat` counts
the whole process; `wall_s` brackets `solve_target()` alone.

`--dump-qp` on a `kNeutral` cell runs program start, model construction, the
cell's first QP build and the dump write, and **solves nothing**. Same
discipline: solo, pinned, threads 1, 3× alternating.

| probe cell | base instructions | head instructions | delta | ratio |
|---|---|---|---|---|
| `f7_n1000_bound_neutral` | 81 388 482 | 81 388 895 | **+413** | 1.00001 |
| `f7_n20000_bound_neutral` | 1 591 880 187 | 1 591 880 599 | **+412** | 1.00000 |

**+413 and +412 instructions — a fixed constant, independent of n, and five
orders of magnitude below the leg-1 deltas of 0.35 M–8.3 M.** Startup, model
construction and QP assembly are instruction-identical between the arms. The
entire size-scaling instruction increase is therefore inside the solve (or the
post-solve KKT gate, which this probe does not run and which also scales with
n — the one residual this probe does not separate, and it is named rather than
assumed away).

Combined with the byte-identical counters of §2 — same factorizations, same
minors, same iterations — the finding is precise: **the head arm performs the
same solve, in the same number of steps, executing about 0.03–0.13 % more
instructions per unit of work, and the front end absorbs the difference so the
wall does not move.**

---

## 7. The composed cumulative chain

§11.1's cumulative bar does not compose itself: two in-band readings can compose
out of band, so the arithmetic is stated. There is no third arm and none is
implied — this is multiplication of two measured ratios.

| mode | T6 close 50f616a → 1997159 | T8.9r 102f729 → e51a7e0 | composed | inside ±0.5 %? |
|---|---|---|---|---|
| ipm | 1.0023 | 0.999920 | **1.00222** (+0.222 %) | **yes** |
| ssn | 1.0015 | 0.999899 | **1.00140** (+0.140 %) | **yes** |
| walk | 0.9993 | 1.000927 | **1.00023** (+0.023 %) | **yes** |

**And the chain has no unmeasured code gap.** T7 sits between 1997159 and
102f729 and was comment-only; `arm-base`'s `libhven.a` sha256 begins
`735eea1d9d51ef93`, the sha16 the T6 close record gives for the post-(d) revert
arm's library. The library at 102f729 IS the library at the T6 close head, so
the two measured spans meet end to end.

The interior leg does not enter this chain: it has no arm before b9848bf, and
the top-level IPM's runtime across T8.1–T8.8 is not measured by anything here.

---

## 8. What this reading claims, and what it does not

**Claims, with the numbers above:**

* Leg 1 is FLAT in all three modes, 0/27 cells outside band, composed cumulative
  chain inside ±0.5 % in all three modes.
* Leg 1's counters are byte-identical between the arms across 75 columns, 27
  cells, three rounds, three modes.
* Leg 1's instructions are UP, reproducibly and far above that leg's measured
  noise floor, and the increase lies inside the solve. **This is WORK-MOVED
  under §11.1 and the veto trigger has fired.**
* Leg 2 moved faster in all six combinations; its magnitude is inside its own
  declared resolution limit and is not quoted as a measurement.
* The interior leg is UNRESOLVED, with two banded veto candidates and a declared
  first-row exclusion.

**Does not claim:**

* Any disposition. §11.1.1 sends "demonstrated instructions UP" to the owner
  with the numbers; §11.5 owns the outcome. This directory stops at the numbers.
* Anything about the top-level IPM's runtime across T8.1–T8.8. No base arm
  exists; it is not measured here.
* Any instruction verdict from leg 2 or from the interior leg.
* Any Apple/Accelerate or Windows value, and any Intel pass-B value. All
  **UNOBSERVED**.
* Any wall number from a co-run. There was none.

---

## 9. The comparator's output, verbatim

Everything above is derived from what follows; what follows is derived from
`raw/` and `perf/` and nothing else.

# W5 T8.9r comparator output

Regenerate: `python3 comparator.py --root . --out -`

## Leg 1 -- the U0 27-cell corpus (102f729 -> e51a7e0)

### leg 1 / ipm

corpus base 14.8418 s -> head 14.8406 s, **ratio 0.9999** (-0.008 %); cells compared 27; outside 0.99-1.01: **0**; band **FLAT**

veto check -- cells slower in ALL alternating rounds: strict reading 4 ['f7_n1000_bound_activity', 'f7_n20000_bound_corrupted', 'f7_n20000_bound_warm', 'f7_n800_path_warm']; banded reading (also outside 0.99-1.01) 0 

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `f7_n10000_bound_activity` | 0.677407 | 0.675712 | 0.9975 | 0.9991 0.9983 0.9975 |
| `f7_n10000_bound_corrupted` | 0.769952 | 0.770004 | 1.0001 | 0.9984 1.0149 1.0004 |
| `f7_n10000_bound_neutral` | 0.833237 | 0.829718 | 0.9958 | 0.9985 0.9958 0.9957 |
| `f7_n10000_bound_physics` | 0.676953 | 0.675871 | 0.9984 | 0.9995 0.9984 0.9986 |
| `f7_n10000_bound_warm` | 0.770296 | 0.769825 | 0.9994 | 0.9993 1.0015 1.0003 |
| `f7_n1000_bound_activity` | 0.064134 | 0.064439 | 1.0048 | 1.0019 1.0048 1.0031 |
| `f7_n1000_bound_corrupted` | 0.068799 | 0.068609 | 0.9972 | 0.9964 0.9968 1.0015 |
| `f7_n1000_bound_neutral` | 0.081143 | 0.080538 | 0.9925 | 0.9880 0.9958 0.9933 |
| `f7_n1000_bound_physics` | 0.065542 | 0.065031 | 0.9922 | 0.9876 0.9927 0.9998 |
| `f7_n1000_bound_warm` | 0.068549 | 0.068602 | 1.0008 | 1.0012 0.9982 1.0012 |
| `f7_n1000_path_warm` | 0.267507 | 0.267731 | 1.0008 | 1.0041 0.9976 1.0012 |
| `f7_n20000_bound_activity` | 1.412377 | 1.413215 | 1.0006 | 0.9989 1.0006 1.0020 |
| `f7_n20000_bound_corrupted` | 1.613839 | 1.614746 | 1.0006 | 1.0016 1.0012 1.0006 |
| `f7_n20000_bound_neutral` | 1.729833 | 1.733913 | 1.0024 | 1.0038 0.9952 1.0024 |
| `f7_n20000_bound_physics` | 1.412928 | 1.413174 | 1.0002 | 1.0006 0.9978 1.0006 |
| `f7_n20000_bound_warm` | 1.612748 | 1.616515 | 1.0023 | 1.0036 1.0021 1.0017 |
| `f7_n2000_bound_activity` | 0.130761 | 0.130549 | 0.9984 | 0.9986 0.9962 1.0011 |
| `f7_n2000_bound_corrupted` | 0.139900 | 0.139729 | 0.9988 | 0.9933 0.9989 1.0004 |
| `f7_n2000_bound_neutral` | 0.162210 | 0.161421 | 0.9951 | 0.9951 0.9664 0.9972 |
| `f7_n2000_bound_physics` | 0.130891 | 0.130668 | 0.9983 | 0.9941 0.9983 0.9990 |
| `f7_n2000_bound_warm` | 0.139875 | 0.139736 | 0.9990 | 0.9934 0.9998 1.0002 |
| `f7_n5000_bound_activity` | 0.328636 | 0.327475 | 0.9965 | 0.9978 0.9965 0.9973 |
| `f7_n5000_bound_corrupted` | 0.373884 | 0.374173 | 1.0008 | 0.9975 1.0010 1.0008 |
| `f7_n5000_bound_neutral` | 0.405597 | 0.404338 | 0.9969 | 0.9924 1.0006 0.9969 |
| `f7_n5000_bound_physics` | 0.328565 | 0.328334 | 0.9993 | 0.9982 0.9983 0.9999 |
| `f7_n5000_bound_warm` | 0.353944 | 0.353928 | 1.0000 | 0.9979 1.0004 1.0017 |
| `f7_n800_path_warm` | 0.222308 | 0.222635 | 1.0015 | 1.0014 1.0023 1.0015 |

pass A, cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 74229 | 56222 | 0.75741 |
| `branch-misses:u` | 546745 | 539467 | 0.98669 |
| `branches:u` | 112838765 | 112896091 | 1.00051 |
| `cycles:u` | 242036696 | 241257007 | 0.99678 |
| `instructions:u` | 749104475 | 749451809 | 1.00046 |

pass B (Zen 3 front end), cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 241954984 | 241054472 | 0.99628 |
| `de_dis_uop_queue_empty_di0:u` | 8824739 | 8965372 | 1.01594 |
| `ic_fetch_stall.ic_stall_any:u` | 102540081 | 101689837 | 0.99171 |
| `instructions:u` | 749110069 | 749457045 | 1.00046 |
| `op_cache_hit_miss.op_cache_hit:u` | 106648233 | 107394853 | 1.00700 |
| `op_cache_hit_miss.op_cache_miss:u` | 11293265 | 10947494 | 0.96938 |
base arm: front-end-bound (dq-empty/cycles) = 0.0365; op-cache miss share = 0.0958
head arm: front-end-bound (dq-empty/cycles) = 0.0372; op-cache miss share = 0.0925

pass A, cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 1347820 | 940755 | 0.69798 |
| `branch-misses:u` | 8368682 | 8273673 | 0.98865 |
| `branches:u` | 2405653783 | 2406769283 | 1.00046 |
| `cycles:u` | 5256755178 | 5258115417 | 1.00026 |
| `instructions:u` | 15958310647 | 15965108235 | 1.00043 |

pass B (Zen 3 front end), cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 5256072486 | 5248881938 | 0.99863 |
| `de_dis_uop_queue_empty_di0:u` | 133330492 | 141982945 | 1.06489 |
| `ic_fetch_stall.ic_stall_any:u` | 2381855820 | 2373036664 | 0.99630 |
| `instructions:u` | 15958310649 | 15965108334 | 1.00043 |
| `op_cache_hit_miss.op_cache_hit:u` | 2261873960 | 2277354416 | 1.00684 |
| `op_cache_hit_miss.op_cache_miss:u` | 214974768 | 207050261 | 0.96314 |
base arm: front-end-bound (dq-empty/cycles) = 0.0254; op-cache miss share = 0.0868
head arm: front-end-bound (dq-empty/cycles) = 0.0271; op-cache miss share = 0.0833

pass A, cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 319107 | 268560 | 0.84160 |
| `branch-misses:u` | 2093392 | 2051243 | 0.97987 |
| `branches:u` | 577325776 | 577433214 | 1.00019 |
| `cycles:u` | 1224322909 | 1223564419 | 0.99938 |
| `instructions:u` | 3834858737 | 3836095428 | 1.00032 |

pass B (Zen 3 front end), cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 1224261437 | 1219411337 | 0.99604 |
| `de_dis_uop_queue_empty_di0:u` | 34412804 | 34970581 | 1.01621 |
| `ic_fetch_stall.ic_stall_any:u` | 531045333 | 525553579 | 0.98966 |
| `instructions:u` | 3834864367 | 3836096487 | 1.00032 |
| `op_cache_hit_miss.op_cache_hit:u` | 540135720 | 544827456 | 1.00869 |
| `op_cache_hit_miss.op_cache_miss:u` | 54373801 | 52332335 | 0.96245 |
base arm: front-end-bound (dq-empty/cycles) = 0.0281; op-cache miss share = 0.0915
head arm: front-end-bound (dq-empty/cycles) = 0.0287; op-cache miss share = 0.0876

### leg 1 / ssn

corpus base 11.0930 s -> head 11.0918 s, **ratio 0.9999** (-0.010 %); cells compared 27; outside 0.99-1.01: **0**; band **FLAT**

veto check -- cells slower in ALL alternating rounds: strict reading 3 ['f7_n10000_bound_warm', 'f7_n1000_bound_activity', 'f7_n1000_path_warm']; banded reading (also outside 0.99-1.01) 0 

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `f7_n10000_bound_activity` | 0.704880 | 0.703407 | 0.9979 | 0.9964 0.9953 0.9994 |
| `f7_n10000_bound_corrupted` | 0.324928 | 0.325223 | 1.0009 | 0.9996 0.9979 1.0036 |
| `f7_n10000_bound_neutral` | 0.703621 | 0.702434 | 0.9983 | 0.9944 1.0037 0.9973 |
| `f7_n10000_bound_physics` | 0.703894 | 0.702718 | 0.9983 | 0.9961 0.9983 0.9967 |
| `f7_n10000_bound_warm` | 0.324060 | 0.325499 | 1.0044 | 1.0025 1.0068 1.0044 |
| `f7_n1000_bound_activity` | 0.062178 | 0.062271 | 1.0015 | 1.0009 1.0045 1.0015 |
| `f7_n1000_bound_corrupted` | 0.029231 | 0.029218 | 0.9996 | 0.9994 1.0084 0.9988 |
| `f7_n1000_bound_neutral` | 0.062666 | 0.062363 | 0.9952 | 0.9976 1.0099 0.9920 |
| `f7_n1000_bound_physics` | 0.061887 | 0.061871 | 0.9997 | 1.0006 1.0030 0.9997 |
| `f7_n1000_bound_warm` | 0.029090 | 0.029118 | 1.0010 | 0.9982 1.0010 1.0058 |
| `f7_n1000_path_warm` | 0.223861 | 0.224339 | 1.0021 | 1.0027 1.0023 1.0019 |
| `f7_n20000_bound_activity` | 1.503526 | 1.504916 | 1.0009 | 0.9956 1.0008 1.0009 |
| `f7_n20000_bound_corrupted` | 0.700498 | 0.701057 | 1.0008 | 0.9979 1.0057 1.0031 |
| `f7_n20000_bound_neutral` | 1.504459 | 1.499849 | 0.9969 | 0.9942 0.9965 0.9995 |
| `f7_n20000_bound_physics` | 1.503984 | 1.503420 | 0.9996 | 0.9973 0.9996 0.9976 |
| `f7_n20000_bound_warm` | 0.699087 | 0.700560 | 1.0021 | 0.9995 1.0034 1.0009 |
| `f7_n2000_bound_activity` | 0.127927 | 0.128022 | 1.0007 | 0.9963 1.0080 0.9983 |
| `f7_n2000_bound_corrupted` | 0.060741 | 0.060801 | 1.0010 | 1.0016 1.0029 0.9987 |
| `f7_n2000_bound_neutral` | 0.128466 | 0.128232 | 0.9982 | 1.0002 1.0015 0.9967 |
| `f7_n2000_bound_physics` | 0.128008 | 0.128013 | 1.0000 | 1.0046 0.9961 1.0015 |
| `f7_n2000_bound_warm` | 0.060730 | 0.060669 | 0.9990 | 0.9969 0.9669 0.9990 |
| `f7_n5000_bound_activity` | 0.333561 | 0.332663 | 0.9973 | 0.9890 1.0051 0.9972 |
| `f7_n5000_bound_corrupted` | 0.153068 | 0.154187 | 1.0073 | 0.9844 1.0108 1.0073 |
| `f7_n5000_bound_neutral` | 0.331898 | 0.334118 | 1.0067 | 0.9963 1.0147 1.0065 |
| `f7_n5000_bound_physics` | 0.332250 | 0.332282 | 1.0001 | 0.9967 1.0034 1.0000 |
| `f7_n5000_bound_warm` | 0.153273 | 0.153127 | 0.9991 | 0.9985 1.0010 0.9943 |
| `f7_n800_path_warm` | 0.141187 | 0.141466 | 1.0020 | 0.9991 1.0041 1.0021 |

pass A, cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 39472 | 28478 | 0.72147 |
| `branch-misses:u` | 601490 | 575095 | 0.95612 |
| `branches:u` | 77308157 | 77393520 | 1.00110 |
| `cycles:u` | 178805455 | 178823739 | 1.00010 |
| `instructions:u` | 488466427 | 488889622 | 1.00087 |

pass B (Zen 3 front end), cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 178673346 | 178223624 | 0.99748 |
| `de_dis_uop_queue_empty_di0:u` | 7588338 | 7508658 | 0.98950 |
| `ic_fetch_stall.ic_stall_any:u` | 82622537 | 82561825 | 0.99927 |
| `instructions:u` | 488466160 | 488889565 | 1.00087 |
| `op_cache_hit_miss.op_cache_hit:u` | 77391759 | 77284210 | 0.99861 |
| `op_cache_hit_miss.op_cache_miss:u` | 3940687 | 3952778 | 1.00307 |
base arm: front-end-bound (dq-empty/cycles) = 0.0425; op-cache miss share = 0.0485
head arm: front-end-bound (dq-empty/cycles) = 0.0421; op-cache miss share = 0.0487

pass A, cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 803291 | 551429 | 0.68646 |
| `branch-misses:u` | 9556923 | 9372498 | 0.98070 |
| `branches:u` | 1834989473 | 1836665187 | 1.00091 |
| `cycles:u` | 4309616648 | 4318546785 | 1.00207 |
| `instructions:u` | 11674687728 | 11683005904 | 1.00071 |

pass B (Zen 3 front end), cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 4322594857 | 4319876228 | 0.99937 |
| `de_dis_uop_queue_empty_di0:u` | 118427765 | 116415368 | 0.98301 |
| `ic_fetch_stall.ic_stall_any:u` | 2160873055 | 2161073512 | 1.00009 |
| `instructions:u` | 11674687773 | 11683006049 | 1.00071 |
| `op_cache_hit_miss.op_cache_hit:u` | 1810731226 | 1811978908 | 1.00069 |
| `op_cache_hit_miss.op_cache_miss:u` | 73144703 | 72759492 | 0.99473 |
base arm: front-end-bound (dq-empty/cycles) = 0.0274; op-cache miss share = 0.0388
head arm: front-end-bound (dq-empty/cycles) = 0.0269; op-cache miss share = 0.0386

pass A, cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 196101 | 136844 | 0.69782 |
| `branch-misses:u` | 2298992 | 2255882 | 0.98125 |
| `branches:u` | 413926450 | 414208237 | 1.00068 |
| `cycles:u` | 950102606 | 948530293 | 0.99835 |
| `instructions:u` | 2629019851 | 2630729726 | 1.00065 |

pass B (Zen 3 front end), cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 947734670 | 948351925 | 1.00065 |
| `de_dis_uop_queue_empty_di0:u` | 29018022 | 28806433 | 0.99271 |
| `ic_fetch_stall.ic_stall_any:u` | 455636222 | 456265064 | 1.00138 |
| `instructions:u` | 2629019484 | 2630729693 | 1.00065 |
| `op_cache_hit_miss.op_cache_hit:u` | 407767684 | 408502712 | 1.00180 |
| `op_cache_hit_miss.op_cache_miss:u` | 18150329 | 18224064 | 1.00406 |
base arm: front-end-bound (dq-empty/cycles) = 0.0306; op-cache miss share = 0.0426
head arm: front-end-bound (dq-empty/cycles) = 0.0304; op-cache miss share = 0.0427

### leg 1 / walk

corpus base 6.0222 s -> head 6.0278 s, **ratio 1.0009** (+0.093 %); cells compared 27; outside 0.99-1.01: **0**; band **FLAT**

veto check -- cells slower in ALL alternating rounds: strict reading 7 ['f7_n1000_bound_warm', 'f7_n20000_bound_corrupted', 'f7_n20000_bound_physics', 'f7_n20000_bound_warm', 'f7_n2000_bound_corrupted', 'f7_n5000_bound_corrupted', 'f7_n5000_bound_warm']; banded reading (also outside 0.99-1.01) 0 

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `f7_n10000_bound_activity` | 0.312062 | 0.311863 | 0.9994 | 1.0005 0.9975 0.9965 |
| `f7_n10000_bound_corrupted` | 0.294587 | 0.294500 | 0.9997 | 1.0015 0.9977 0.9997 |
| `f7_n10000_bound_neutral` | 0.311576 | 0.311401 | 0.9994 | 1.0041 0.9981 0.9994 |
| `f7_n10000_bound_physics` | 0.311616 | 0.311799 | 1.0006 | 1.0064 1.0006 0.9950 |
| `f7_n10000_bound_warm` | 0.294018 | 0.294518 | 1.0017 | 1.0038 0.9990 1.0037 |
| `f7_n1000_bound_activity` | 0.030776 | 0.030698 | 0.9975 | 0.9951 1.0037 0.9977 |
| `f7_n1000_bound_corrupted` | 0.027200 | 0.027245 | 1.0017 | 1.0009 0.9988 1.0035 |
| `f7_n1000_bound_neutral` | 0.031194 | 0.031096 | 0.9969 | 0.9936 0.9996 1.0006 |
| `f7_n1000_bound_physics` | 0.030797 | 0.030786 | 0.9997 | 1.0001 1.0016 0.9968 |
| `f7_n1000_bound_warm` | 0.027005 | 0.027057 | 1.0019 | 1.0019 1.0009 1.0029 |
| `f7_n1000_path_warm` | 0.105443 | 0.105147 | 0.9972 | 1.0016 0.9972 0.9889 |
| `f7_n20000_bound_activity` | 0.645168 | 0.645220 | 1.0001 | 0.9929 1.0010 1.0010 |
| `f7_n20000_bound_corrupted` | 0.610360 | 0.611967 | 1.0026 | 1.0005 1.0037 1.0030 |
| `f7_n20000_bound_neutral` | 0.645584 | 0.644858 | 0.9989 | 1.0001 0.9989 0.9988 |
| `f7_n20000_bound_physics` | 0.644921 | 0.647790 | 1.0044 | 1.0044 1.0030 1.0037 |
| `f7_n20000_bound_warm` | 0.609916 | 0.611786 | 1.0031 | 1.0022 1.0044 1.0033 |
| `f7_n2000_bound_activity` | 0.061135 | 0.061333 | 1.0032 | 1.0036 1.0025 0.9990 |
| `f7_n2000_bound_corrupted` | 0.056104 | 0.056656 | 1.0098 | 1.0098 1.0060 1.0044 |
| `f7_n2000_bound_neutral` | 0.061348 | 0.061123 | 0.9963 | 1.0011 0.9953 0.9951 |
| `f7_n2000_bound_physics` | 0.061176 | 0.061137 | 0.9994 | 1.0005 0.9966 1.0000 |
| `f7_n2000_bound_warm` | 0.055841 | 0.055898 | 1.0010 | 0.9947 1.0067 1.0030 |
| `f7_n5000_bound_activity` | 0.151437 | 0.151787 | 1.0023 | 1.0083 0.9974 1.0048 |
| `f7_n5000_bound_corrupted` | 0.142426 | 0.142882 | 1.0032 | 1.0092 1.0005 1.0022 |
| `f7_n5000_bound_neutral` | 0.153059 | 0.151798 | 0.9918 | 0.9576 0.9991 0.9920 |
| `f7_n5000_bound_physics` | 0.151823 | 0.151384 | 0.9971 | 0.9615 1.0009 0.9971 |
| `f7_n5000_bound_warm` | 0.141913 | 0.142253 | 1.0024 | 1.0048 1.0062 1.0001 |
| `f7_n800_path_warm` | 0.053709 | 0.053793 | 1.0016 | 1.0019 1.0001 0.9994 |

pass A, cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 16869 | 15301 | 0.90705 |
| `branch-misses:u` | 304670 | 301474 | 0.98951 |
| `branches:u` | 39744127 | 39801199 | 1.00144 |
| `cycles:u` | 87465899 | 87541065 | 1.00086 |
| `instructions:u` | 261282683 | 261625450 | 1.00131 |

pass B (Zen 3 front end), cell `f7_n1000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 87322993 | 87279878 | 0.99951 |
| `de_dis_uop_queue_empty_di0:u` | 3741957 | 3715567 | 0.99295 |
| `ic_fetch_stall.ic_stall_any:u` | 39569690 | 39242103 | 0.99172 |
| `instructions:u` | 261282861 | 261631125 | 1.00133 |
| `op_cache_hit_miss.op_cache_hit:u` | 40209549 | 40253050 | 1.00108 |
| `op_cache_hit_miss.op_cache_miss:u` | 1466602 | 1539927 | 1.05000 |
base arm: front-end-bound (dq-empty/cycles) = 0.0429; op-cache miss share = 0.0352
head arm: front-end-bound (dq-empty/cycles) = 0.0426; op-cache miss share = 0.0368

pass A, cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 221131 | 207121 | 0.93664 |
| `branch-misses:u` | 4893409 | 4839068 | 0.98890 |
| `branches:u` | 857537972 | 858093808 | 1.00065 |
| `cycles:u` | 1865961852 | 1868867214 | 1.00156 |
| `instructions:u` | 5640377762 | 5645586494 | 1.00092 |

pass B (Zen 3 front end), cell `f7_n20000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 1869556842 | 1869502137 | 0.99997 |
| `de_dis_uop_queue_empty_di0:u` | 46438682 | 46366468 | 0.99844 |
| `ic_fetch_stall.ic_stall_any:u` | 874551668 | 871354386 | 0.99634 |
| `instructions:u` | 5640377781 | 5645580813 | 1.00092 |
| `op_cache_hit_miss.op_cache_hit:u` | 863395979 | 861414269 | 0.99770 |
| `op_cache_hit_miss.op_cache_miss:u` | 23921066 | 25598276 | 1.07011 |
base arm: front-end-bound (dq-empty/cycles) = 0.0248; op-cache miss share = 0.0270
head arm: front-end-bound (dq-empty/cycles) = 0.0248; op-cache miss share = 0.0289

pass A, cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 59081 | 54989 | 0.93074 |
| `branch-misses:u` | 1240285 | 1231867 | 0.99321 |
| `branches:u` | 202366687 | 202646959 | 1.00138 |
| `cycles:u` | 436551433 | 437239326 | 1.00158 |
| `instructions:u` | 1333963362 | 1335651610 | 1.00127 |

pass B (Zen 3 front end), cell `f7_n5000_bound_neutral`: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 436149190 | 435156499 | 0.99772 |
| `de_dis_uop_queue_empty_di0:u` | 12269166 | 12265739 | 0.99972 |
| `ic_fetch_stall.ic_stall_any:u` | 199227462 | 196964365 | 0.98864 |
| `instructions:u` | 1333964103 | 1335651049 | 1.00126 |
| `op_cache_hit_miss.op_cache_hit:u` | 204180163 | 204112190 | 0.99967 |
| `op_cache_hit_miss.op_cache_miss:u` | 6080830 | 6539482 | 1.07543 |
base arm: front-end-bound (dq-empty/cycles) = 0.0281; op-cache miss share = 0.0289
head arm: front-end-bound (dq-empty/cycles) = 0.0282; op-cache miss share = 0.0310

## Leg 2 -- the 27 Hock-Schittkowski problems, --repeat N

### leg 2 / ipm / trace=off

corpus base 0.0373 s -> head 0.0370 s, **ratio 0.9917** (-0.834 %); cells compared 27; outside 0.99-1.01: **23**; band **MOVED**

veto check -- cells slower in ALL alternating rounds: strict reading 3 ['12', '15', '25']; banded reading (also outside 0.99-1.01) 2 ['12', '15']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.002334 | 0.002304 | 0.9869 | 0.9839 0.9929 0.9794 |
| `10` | 0.004641 | 0.004575 | 0.9857 | 0.9846 0.9907 0.9816 |
| `11` | 0.001223 | 0.001191 | 0.9738 | 0.9707 0.9888 0.9740 |
| `12` | 0.000762 | 0.000860 | 1.1285 | 1.1354 1.1393 1.1260 |
| `14` | 0.000567 | 0.000560 | 0.9864 | 0.9704 0.9915 0.9864 |
| `15` | 0.003116 | 0.003255 | 1.0446 | 1.0484 1.0542 1.0446 |
| `22` | 0.000739 | 0.000705 | 0.9538 | 0.9512 0.9551 0.9504 |
| `24` | 0.000586 | 0.000575 | 0.9819 | 0.9786 0.9848 0.9789 |
| `25` | 0.000027 | 0.000027 | 1.0094 | 1.0109 1.0068 1.0188 |
| `26` | 0.001711 | 0.001684 | 0.9843 | 0.9843 0.9946 0.9778 |
| `27` | 0.001905 | 0.001875 | 0.9842 | 0.9849 0.9888 0.9772 |
| `28` | 0.000464 | 0.000456 | 0.9835 | 0.9846 0.9893 0.9767 |
| `3` | 0.000602 | 0.000592 | 0.9831 | 0.9767 1.0052 0.9831 |
| `30` | 0.001251 | 0.001226 | 0.9800 | 0.9760 0.9870 0.9797 |
| `33` | 0.001906 | 0.001888 | 0.9905 | 0.9891 1.0043 0.9897 |
| `35` | 0.000155 | 0.000152 | 0.9817 | 0.9817 0.9858 0.9739 |
| `38` | 0.006245 | 0.006106 | 0.9777 | 0.9784 0.9817 0.9737 |
| `39` | 0.001199 | 0.001190 | 0.9917 | 0.9777 1.0000 0.9863 |
| `40` | 0.000489 | 0.000483 | 0.9872 | 0.9898 0.9872 0.9778 |
| `43` | 0.001356 | 0.001341 | 0.9894 | 0.9836 0.9914 0.9894 |
| `45` | 0.001121 | 0.001120 | 0.9986 | 0.9958 1.0097 0.9972 |
| `5` | 0.000374 | 0.000366 | 0.9789 | 0.9664 0.9973 0.9798 |
| `6` | 0.000937 | 0.000927 | 0.9888 | 0.9796 1.0037 0.9888 |
| `7` | 0.001093 | 0.001081 | 0.9892 | 0.9741 1.0030 0.9892 |
| `76` | 0.000531 | 0.000523 | 0.9845 | 0.9768 1.0018 0.9824 |
| `77` | 0.001476 | 0.001448 | 0.9811 | 0.9784 0.9858 0.9807 |
| `79` | 0.000511 | 0.000503 | 0.9849 | 0.9831 0.9849 0.9847 |

perf: NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 709604209 | 782741912 | 1.10307 |
| `branch-misses:u` | 367362540 | 343015293 | 0.93372 |
| `branches:u` | 45235390379 | 45291761791 | 1.00125 |
| `cycles:u` | 126577853874 | 124672011482 | 0.98494 |
| `instructions:u` | 247046485578 | 247321537000 | 1.00111 |

pass B: INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 126815853080 | 124863042499 | 0.98460 |
| `de_dis_uop_queue_empty_di0:u` | 29517085728 | 28881100368 | 0.97845 |
| `ic_fetch_stall.ic_stall_any:u` | 53606988642 | 52471225319 | 0.97881 |
| `instructions:u` | 247046493255 | 246929524711 | 0.99953 |
| `op_cache_hit_miss.op_cache_hit:u` | 22520214231 | 22658531922 | 1.00614 |
| `op_cache_hit_miss.op_cache_miss:u` | 24731094357 | 24232692274 | 0.97985 |

### leg 2 / ipm / trace=sink

corpus base 0.0374 s -> head 0.0371 s, **ratio 0.9912** (-0.884 %); cells compared 27; outside 0.99-1.01: **24**; band **MOVED**

veto check -- cells slower in ALL alternating rounds: strict reading 3 ['12', '15', '25']; banded reading (also outside 0.99-1.01) 2 ['12', '15']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.002343 | 0.002310 | 0.9857 | 0.9877 1.0055 0.9819 |
| `10` | 0.004657 | 0.004586 | 0.9848 | 0.9891 0.9836 0.9778 |
| `11` | 0.001228 | 0.001193 | 0.9713 | 0.9709 0.9957 0.9703 |
| `12` | 0.000764 | 0.000859 | 1.1241 | 1.1168 1.1453 1.1241 |
| `14` | 0.000572 | 0.000560 | 0.9783 | 0.9810 0.9994 0.9683 |
| `15` | 0.003126 | 0.003275 | 1.0477 | 1.0476 1.0688 1.0465 |
| `22` | 0.000751 | 0.000707 | 0.9414 | 0.9387 0.9608 0.9460 |
| `24` | 0.000590 | 0.000577 | 0.9782 | 0.9705 0.9830 0.9782 |
| `25` | 0.000027 | 0.000027 | 1.0094 | 1.0083 1.0143 1.0094 |
| `26` | 0.001708 | 0.001689 | 0.9886 | 0.9894 0.9950 0.9819 |
| `27` | 0.001904 | 0.001874 | 0.9842 | 0.9854 0.9842 0.9793 |
| `28` | 0.000465 | 0.000458 | 0.9841 | 0.9734 0.9850 0.9841 |
| `3` | 0.000601 | 0.000593 | 0.9865 | 0.9957 1.0028 0.9845 |
| `30` | 0.001252 | 0.001230 | 0.9823 | 0.9773 0.9935 0.9738 |
| `33` | 0.001908 | 0.001891 | 0.9912 | 0.9875 0.9922 0.9912 |
| `35` | 0.000156 | 0.000153 | 0.9807 | 0.9782 0.9807 0.9839 |
| `38` | 0.006263 | 0.006125 | 0.9780 | 0.9768 0.9830 0.9727 |
| `39` | 0.001209 | 0.001191 | 0.9847 | 0.9813 0.9851 0.9795 |
| `40` | 0.000493 | 0.000484 | 0.9808 | 0.9802 0.9797 0.9823 |
| `43` | 0.001365 | 0.001347 | 0.9871 | 0.9857 0.9893 0.9869 |
| `45` | 0.001128 | 0.001123 | 0.9960 | 0.9946 0.9944 0.9960 |
| `5` | 0.000374 | 0.000366 | 0.9794 | 0.9828 0.9760 0.9820 |
| `6` | 0.000937 | 0.000927 | 0.9890 | 0.9920 0.9870 0.9860 |
| `7` | 0.001095 | 0.001081 | 0.9875 | 0.9895 0.9871 0.9872 |
| `76` | 0.000536 | 0.000527 | 0.9839 | 0.9750 1.0006 0.9839 |
| `77` | 0.001475 | 0.001453 | 0.9849 | 0.9849 0.9985 0.9824 |
| `79` | 0.000512 | 0.000504 | 0.9834 | 0.9877 0.9827 0.9800 |

perf: NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 714763328 | 792329520 | 1.10852 |
| `branch-misses:u` | 365955915 | 348310704 | 0.95178 |
| `branches:u` | 45134854861 | 45191301891 | 1.00125 |
| `cycles:u` | 126740361026 | 124916120505 | 0.98561 |
| `instructions:u` | 246877690892 | 247153393777 | 1.00112 |

pass B: INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 127627330554 | 124736385563 | 0.97735 |
| `de_dis_uop_queue_empty_di0:u` | 29428719969 | 28306590690 | 0.96187 |
| `ic_fetch_stall.ic_stall_any:u` | 53956294237 | 52508755318 | 0.97317 |
| `instructions:u` | 247269978796 | 247153382896 | 0.99953 |
| `op_cache_hit_miss.op_cache_hit:u` | 22590863048 | 22643005066 | 1.00231 |
| `op_cache_hit_miss.op_cache_miss:u` | 24660768199 | 24202394325 | 0.98141 |

### leg 2 / ssn / trace=off

corpus base 0.0304 s -> head 0.0297 s, **ratio 0.9782** (-2.183 %); cells compared 27; outside 0.99-1.01: **20**; band **MOVED**

veto check -- cells slower in ALL alternating rounds: strict reading 1 ['25']; banded reading (also outside 0.99-1.01) 1 ['25']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.001677 | 0.001637 | 0.9764 | 0.9726 0.9843 0.9685 |
| `10` | 0.006558 | 0.006440 | 0.9819 | 0.9806 1.0000 0.9812 |
| `11` | 0.002027 | 0.001872 | 0.9233 | 0.9221 0.9390 0.9217 |
| `12` | 0.000410 | 0.000399 | 0.9731 | 0.9712 0.9846 0.9625 |
| `14` | 0.000356 | 0.000351 | 0.9857 | 0.9854 0.9921 0.9747 |
| `15` | 0.001188 | 0.001146 | 0.9645 | 0.9628 0.9777 0.9625 |
| `22` | 0.000521 | 0.000491 | 0.9434 | 0.9434 0.9522 0.9324 |
| `24` | 0.000647 | 0.000614 | 0.9499 | 0.9488 0.9609 0.9499 |
| `25` | 0.000027 | 0.000027 | 1.0106 | 1.0106 1.0106 1.0071 |
| `26` | 0.001112 | 0.001085 | 0.9753 | 0.9753 0.9876 0.9703 |
| `27` | 0.001521 | 0.001491 | 0.9803 | 0.9790 0.9905 0.9803 |
| `28` | 0.000270 | 0.000270 | 0.9974 | 0.9974 1.0022 0.9836 |
| `3` | 0.000320 | 0.000313 | 0.9780 | 0.9800 0.9784 0.9738 |
| `30` | 0.000699 | 0.000684 | 0.9783 | 0.9775 0.9880 0.9756 |
| `33` | 0.000533 | 0.000527 | 0.9891 | 0.9888 1.0003 0.9876 |
| `35` | 0.000131 | 0.000131 | 0.9982 | 0.9982 1.0011 0.9939 |
| `38` | 0.005131 | 0.005053 | 0.9848 | 0.9848 0.9954 0.9806 |
| `39` | 0.001122 | 0.001112 | 0.9910 | 0.9910 1.0028 0.9836 |
| `40` | 0.000318 | 0.000313 | 0.9837 | 0.9832 0.9887 0.9833 |
| `43` | 0.000889 | 0.000884 | 0.9939 | 0.9939 1.0058 0.9762 |
| `45` | 0.000781 | 0.000772 | 0.9893 | 0.9872 0.9985 0.9823 |
| `5` | 0.000298 | 0.000291 | 0.9778 | 0.9778 0.9905 0.9668 |
| `6` | 0.001061 | 0.001059 | 0.9976 | 0.9932 1.0087 0.9938 |
| `7` | 0.001106 | 0.001105 | 0.9984 | 0.9961 1.0095 0.9942 |
| `76` | 0.000275 | 0.000273 | 0.9923 | 0.9923 0.9990 0.9844 |
| `77` | 0.001034 | 0.001018 | 0.9846 | 0.9833 0.9967 0.9811 |
| `79` | 0.000339 | 0.000332 | 0.9809 | 0.9798 0.9914 0.9787 |

perf: NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 929855983 | 1108268327 | 1.19187 |
| `branch-misses:u` | 594860147 | 543282895 | 0.91330 |
| `branches:u` | 76148774488 | 76211238558 | 1.00082 |
| `cycles:u` | 205139475497 | 201208435722 | 0.98084 |
| `instructions:u` | 420656001456 | 421231142644 | 1.00137 |

pass B: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 206173859103 | 202186896774 | 0.98066 |
| `de_dis_uop_queue_empty_di0:u` | 44973145867 | 43283125425 | 0.96242 |
| `ic_fetch_stall.ic_stall_any:u` | 85980326798 | 83932590962 | 0.97618 |
| `instructions:u` | 420655361626 | 421876613054 | 1.00290 |
| `op_cache_hit_miss.op_cache_hit:u` | 42048161384 | 42318370111 | 1.00643 |
| `op_cache_hit_miss.op_cache_miss:u` | 37595293308 | 36751339345 | 0.97755 |

### leg 2 / ssn / trace=sink

corpus base 0.0304 s -> head 0.0299 s, **ratio 0.9816** (-1.841 %); cells compared 27; outside 0.99-1.01: **17**; band **MOVED**

veto check -- cells slower in ALL alternating rounds: strict reading 0 ; banded reading (also outside 0.99-1.01) 0 

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.001682 | 0.001642 | 0.9763 | 0.9852 0.9750 0.9777 |
| `10` | 0.006554 | 0.006481 | 0.9890 | 0.9868 0.9857 0.9923 |
| `11` | 0.002029 | 0.001883 | 0.9282 | 0.9256 0.9299 0.9303 |
| `12` | 0.000409 | 0.000402 | 0.9819 | 0.9819 0.9855 0.9739 |
| `14` | 0.000360 | 0.000352 | 0.9782 | 0.9920 0.9782 0.9705 |
| `15` | 0.001188 | 0.001156 | 0.9731 | 0.9739 0.9807 0.9699 |
| `22` | 0.000523 | 0.000494 | 0.9447 | 0.9441 0.9481 0.9451 |
| `24` | 0.000648 | 0.000619 | 0.9543 | 0.9544 0.9568 0.9535 |
| `25` | 0.000027 | 0.000027 | 1.0015 | 1.0019 1.0227 0.9933 |
| `26` | 0.001119 | 0.001096 | 0.9789 | 0.9763 0.9829 0.9764 |
| `27` | 0.001525 | 0.001498 | 0.9824 | 0.9803 0.9846 0.9855 |
| `28` | 0.000273 | 0.000270 | 0.9903 | 0.9903 0.9855 0.9946 |
| `3` | 0.000322 | 0.000316 | 0.9799 | 0.9783 0.9853 0.9777 |
| `30` | 0.000701 | 0.000686 | 0.9785 | 0.9691 0.9801 0.9753 |
| `33` | 0.000535 | 0.000531 | 0.9917 | 0.9887 0.9910 0.9945 |
| `35` | 0.000132 | 0.000131 | 0.9910 | 0.9977 0.9911 0.9884 |
| `38` | 0.005146 | 0.005082 | 0.9876 | 0.9852 0.9865 0.9878 |
| `39` | 0.001126 | 0.001118 | 0.9930 | 0.9931 0.9957 0.9890 |
| `40` | 0.000320 | 0.000315 | 0.9831 | 0.9834 0.9847 0.9803 |
| `43` | 0.000892 | 0.000888 | 0.9960 | 0.9931 0.9974 0.9980 |
| `45` | 0.000781 | 0.000778 | 0.9956 | 0.9927 0.9942 0.9961 |
| `5` | 0.000297 | 0.000293 | 0.9852 | 0.9881 0.9839 0.9767 |
| `6` | 0.001062 | 0.001060 | 0.9973 | 0.9930 0.9937 0.9982 |
| `7` | 0.001109 | 0.001105 | 0.9964 | 0.9948 0.9940 1.0012 |
| `76` | 0.000276 | 0.000273 | 0.9900 | 0.9877 0.9903 0.9949 |
| `77` | 0.001038 | 0.001026 | 0.9894 | 0.9909 0.9827 0.9897 |
| `79` | 0.000341 | 0.000334 | 0.9793 | 0.9835 0.9784 0.9791 |

perf: NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 930168139 | 1112647931 | 1.19618 |
| `branch-misses:u` | 600430361 | 553502730 | 0.92184 |
| `branches:u` | 76153897976 | 76412922262 | 1.00340 |
| `cycles:u` | 205163227613 | 202484725859 | 0.98694 |
| `instructions:u` | 420693068673 | 421914594237 | 1.00290 |

pass B: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 205789696226 | 202884190202 | 0.98588 |
| `de_dis_uop_queue_empty_di0:u` | 44579717059 | 43665379892 | 0.97949 |
| `ic_fetch_stall.ic_stall_any:u` | 85504779422 | 84081593209 | 0.98336 |
| `instructions:u` | 420693315401 | 421914642205 | 1.00290 |
| `op_cache_hit_miss.op_cache_hit:u` | 42117422266 | 42256919148 | 1.00331 |
| `op_cache_hit_miss.op_cache_miss:u` | 37548101125 | 36868848962 | 0.98191 |

### leg 2 / walk / trace=off

corpus base 0.0229 s -> head 0.0224 s, **ratio 0.9798** (-2.016 %); cells compared 27; outside 0.99-1.01: **24**; band **MOVED**

veto check -- cells slower in ALL alternating rounds: strict reading 0 ; banded reading (also outside 0.99-1.01) 0 

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.001290 | 0.001256 | 0.9738 | 0.9783 0.9697 0.9890 |
| `10` | 0.005499 | 0.005410 | 0.9839 | 0.9889 0.9774 0.9954 |
| `11` | 0.001359 | 0.001332 | 0.9797 | 0.9820 0.9797 0.9869 |
| `12` | 0.000407 | 0.000394 | 0.9662 | 0.9703 0.9619 0.9794 |
| `14` | 0.000257 | 0.000249 | 0.9689 | 0.9746 0.9642 0.9804 |
| `15` | 0.001011 | 0.000994 | 0.9837 | 0.9909 0.9792 0.9913 |
| `22` | 0.000256 | 0.000251 | 0.9806 | 0.9806 0.9835 0.9833 |
| `24` | 0.000443 | 0.000428 | 0.9657 | 0.9693 0.9651 0.9759 |
| `25` | 0.000027 | 0.000027 | 1.0049 | 1.0038 0.9952 1.0083 |
| `26` | 0.000907 | 0.000877 | 0.9675 | 0.9704 0.9635 0.9754 |
| `27` | 0.001344 | 0.001310 | 0.9750 | 0.9781 0.9750 0.9800 |
| `28` | 0.000225 | 0.000220 | 0.9776 | 0.9776 0.9696 0.9826 |
| `3` | 0.000223 | 0.000218 | 0.9777 | 0.9783 0.9738 0.9881 |
| `30` | 0.000641 | 0.000628 | 0.9806 | 0.9821 0.9768 0.9870 |
| `33` | 0.000677 | 0.000664 | 0.9815 | 0.9809 0.9798 0.9900 |
| `35` | 0.000092 | 0.000091 | 0.9915 | 0.9953 0.9887 0.9966 |
| `38` | 0.003353 | 0.003280 | 0.9784 | 0.9821 0.9752 0.9866 |
| `39` | 0.000689 | 0.000680 | 0.9862 | 0.9909 0.9775 0.9895 |
| `40` | 0.000234 | 0.000230 | 0.9833 | 0.9893 0.9778 0.9885 |
| `43` | 0.000653 | 0.000644 | 0.9855 | 0.9898 0.9797 0.9925 |
| `45` | 0.000523 | 0.000516 | 0.9852 | 0.9893 0.9772 0.9933 |
| `5` | 0.000186 | 0.000182 | 0.9794 | 0.9811 0.9745 0.9897 |
| `6` | 0.000590 | 0.000574 | 0.9745 | 0.9781 0.9731 0.9858 |
| `7` | 0.000748 | 0.000731 | 0.9767 | 0.9852 0.9755 0.9845 |
| `76` | 0.000277 | 0.000275 | 0.9908 | 0.9937 0.9812 0.9994 |
| `77` | 0.000722 | 0.000713 | 0.9874 | 0.9874 0.9770 0.9940 |
| `79` | 0.000242 | 0.000239 | 0.9868 | 0.9899 0.9815 0.9884 |

perf: NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1))

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 398679344 | 495076222 | 1.24179 |
| `branch-misses:u` | 168837595 | 150816982 | 0.89327 |
| `branches:u` | 30633982188 | 30553830268 | 0.99738 |
| `cycles:u` | 77568258707 | 75909893215 | 0.97862 |
| `instructions:u` | 167893361469 | 167708990254 | 0.99890 |

pass B: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 77163707251 | 76107247059 | 0.98631 |
| `de_dis_uop_queue_empty_di0:u` | 15135461612 | 14822033302 | 0.97929 |
| `ic_fetch_stall.ic_stall_any:u` | 30175327861 | 29606271814 | 0.98114 |
| `instructions:u` | 167530254212 | 168072046032 | 1.00323 |
| `op_cache_hit_miss.op_cache_hit:u` | 15156131602 | 15177657024 | 1.00142 |
| `op_cache_hit_miss.op_cache_miss:u` | 15845082168 | 15773749207 | 0.99550 |

### leg 2 / walk / trace=sink

corpus base 0.0230 s -> head 0.0226 s, **ratio 0.9829** (-1.712 %); cells compared 27; outside 0.99-1.01: **16**; band **MOVED**

veto check -- cells slower in ALL alternating rounds: strict reading 0 ; banded reading (also outside 0.99-1.01) 0 

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `1` | 0.001283 | 0.001270 | 0.9899 | 0.9899 0.9916 0.9877 |
| `10` | 0.005498 | 0.005447 | 0.9907 | 0.9965 0.9906 0.9898 |
| `11` | 0.001404 | 0.001345 | 0.9580 | 0.9547 0.9580 0.9559 |
| `12` | 0.000413 | 0.000389 | 0.9403 | 0.9423 0.9401 0.9383 |
| `14` | 0.000257 | 0.000252 | 0.9793 | 0.9780 0.9793 0.9782 |
| `15` | 0.001050 | 0.001001 | 0.9531 | 0.9566 0.9531 0.9523 |
| `22` | 0.000272 | 0.000254 | 0.9337 | 0.9316 0.9354 0.9334 |
| `24` | 0.000467 | 0.000433 | 0.9281 | 0.9307 0.9281 0.9276 |
| `25` | 0.000027 | 0.000027 | 1.0086 | 1.0075 1.0173 0.9878 |
| `26` | 0.000900 | 0.000882 | 0.9804 | 0.9854 0.9814 0.9786 |
| `27` | 0.001339 | 0.001321 | 0.9864 | 0.9910 0.9864 0.9862 |
| `28` | 0.000226 | 0.000222 | 0.9844 | 0.9877 0.9811 0.9838 |
| `3` | 0.000223 | 0.000220 | 0.9855 | 0.9864 0.9875 0.9853 |
| `30` | 0.000641 | 0.000632 | 0.9863 | 0.9932 0.9886 0.9850 |
| `33` | 0.000675 | 0.000668 | 0.9895 | 0.9923 0.9884 0.9895 |
| `35` | 0.000092 | 0.000092 | 0.9966 | 1.0039 0.9932 0.9966 |
| `38` | 0.003337 | 0.003294 | 0.9871 | 0.9859 0.9875 0.9854 |
| `39` | 0.000687 | 0.000681 | 0.9915 | 0.9923 0.9934 0.9893 |
| `40` | 0.000233 | 0.000231 | 0.9906 | 0.9923 0.9853 0.9906 |
| `43` | 0.000652 | 0.000647 | 0.9923 | 0.9926 0.9934 0.9911 |
| `45` | 0.000523 | 0.000518 | 0.9900 | 0.9912 0.9907 0.9897 |
| `5` | 0.000185 | 0.000183 | 0.9881 | 0.9871 0.9910 0.9881 |
| `6` | 0.000583 | 0.000582 | 0.9980 | 0.9987 0.9953 0.9980 |
| `7` | 0.000744 | 0.000738 | 0.9918 | 0.9952 0.9918 0.9881 |
| `76` | 0.000280 | 0.000276 | 0.9860 | 0.9976 0.9842 0.9863 |
| `77` | 0.000723 | 0.000718 | 0.9933 | 0.9944 0.9928 0.9911 |
| `79` | 0.000241 | 0.000240 | 0.9947 | 0.9990 0.9885 0.9936 |

perf: NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO)

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 405894192 | 491109479 | 1.20994 |
| `branch-misses:u` | 171180482 | 160417947 | 0.93713 |
| `branches:u` | 30516241473 | 30663157279 | 1.00481 |
| `cycles:u` | 77559752273 | 76454170329 | 0.98575 |
| `instructions:u` | 167503453734 | 168071290524 | 1.00339 |

pass B: INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 77669540413 | 76282518914 | 0.98214 |
| `de_dis_uop_queue_empty_di0:u` | 15580867046 | 15052836447 | 0.96611 |
| `ic_fetch_stall.ic_stall_any:u` | 30440170804 | 29948704434 | 0.98385 |
| `instructions:u` | 167866562347 | 167708080467 | 0.99906 |
| `op_cache_hit_miss.op_cache_hit:u` | 15304735712 | 15203225075 | 0.99337 |
| `op_cache_hit_miss.op_cache_miss:u` | 15939644161 | 15766255911 | 0.98912 |

## The interior leg (b9848bf -> e51a7e0)

### interior / F7 rows (banded, leg-1 rules)

Keys only in the HEAD arm (head-only, informational, no band): `f7_n1000_bound_neutral/MakeConstraint/parts2`, `f7_n1000_bound_neutral/MakeParameter/parts2`

corpus base 10.8763 s -> head 11.0430 s, **ratio 1.0153** (+1.532 %); cells compared 32; outside 0.99-1.01: **7**; band **MOVED**

veto check -- cells slower in ALL alternating rounds: strict reading 7 ['f7_n10000_bound_neutral/RelaxBounds', 'f7_n10000_bound_physics/MakeParameter', 'f7_n1000_bound_physics/RelaxBounds', 'f7_n20000_bound_physics/RelaxBounds', 'f7_n2000_bound_neutral/RelaxBounds', 'f7_n2000_bound_physics/MakeConstraint', 'f7_n2000_bound_physics/RelaxBounds']; banded reading (also outside 0.99-1.01) 2 ['f7_n10000_bound_physics/MakeParameter', 'f7_n1000_bound_physics/RelaxBounds']

| cell | base median (s) | head median (s) | ratio | paired per round |
|---|---|---|---|---|
| `f7_n10000_bound_neutral/MakeConstraint` | 0.543957 | 0.540786 | 0.9942 | 0.9859 0.9950 0.9952 |
| `f7_n10000_bound_neutral/MakeParameter` | 0.560447 | 0.559469 | 0.9983 | 0.9937 0.9977 0.9984 |
| `f7_n10000_bound_neutral/RelaxBounds` | 0.537535 | 0.541407 | 1.0072 | 1.0085 1.0069 1.0077 |
| `f7_n10000_bound_physics/MakeConstraint` | 0.372800 | 0.369296 | 0.9906 | 0.9863 0.9907 0.9992 |
| `f7_n10000_bound_physics/MakeParameter` | 0.369970 | 0.374725 | 1.0129 | 1.0139 1.0119 1.0143 |
| `f7_n10000_bound_physics/RelaxBounds` | 0.371940 | 0.371653 | 0.9992 | 0.9963 0.9995 1.0031 |
| `f7_n1000_bound_neutral/MakeConstraint` | 0.041589 | 0.041588 | 1.0000 | 1.0009 1.0006 0.9993 |
| `f7_n1000_bound_neutral/MakeParameter` | 0.330953 | 0.495463 | 1.4971 | 0.4962 1.5310 1.5248 |
| `f7_n1000_bound_neutral/MakeParameter/cap1` | 0.017018 | 0.017030 | 1.0007 | 0.9934 1.0101 1.0007 |
| `f7_n1000_bound_neutral/MakeParameter/solve_optimize` | 0.058694 | 0.058270 | 0.9928 | 0.9977 0.9928 0.9897 |
| `f7_n1000_bound_neutral/RelaxBounds` | 0.041459 | 0.041382 | 0.9982 | 0.9948 1.0006 0.9988 |
| `f7_n1000_bound_physics/MakeConstraint` | 0.033128 | 0.033073 | 0.9984 | 0.9966 0.9984 0.9977 |
| `f7_n1000_bound_physics/MakeParameter` | 0.033145 | 0.033146 | 1.0000 | 0.9995 1.0035 0.9993 |
| `f7_n1000_bound_physics/RelaxBounds` | 0.033078 | 0.033804 | 1.0219 | 1.0168 1.0253 1.0223 |
| `f7_n20000_bound_neutral/MakeConstraint` | 1.124330 | 1.124356 | 1.0000 | 1.0000 1.0042 0.9969 |
| `f7_n20000_bound_neutral/MakeParameter` | 1.169279 | 1.167518 | 0.9985 | 0.9960 0.9985 0.9999 |
| `f7_n20000_bound_neutral/RelaxBounds` | 1.124255 | 1.124827 | 1.0005 | 1.0005 1.0050 0.9993 |
| `f7_n20000_bound_physics/MakeConstraint` | 0.774364 | 0.782818 | 1.0109 | 1.0088 1.0118 0.9984 |
| `f7_n20000_bound_physics/MakeParameter` | 0.781860 | 0.782832 | 1.0012 | 1.0012 1.0049 0.9960 |
| `f7_n20000_bound_physics/RelaxBounds` | 0.773236 | 0.780408 | 1.0093 | 1.0124 1.0093 1.0094 |
| `f7_n2000_bound_neutral/MakeConstraint` | 0.095320 | 0.094853 | 0.9951 | 0.9969 0.9956 0.9946 |
| `f7_n2000_bound_neutral/MakeParameter` | 0.098033 | 0.097377 | 0.9933 | 0.9919 0.9927 0.9963 |
| `f7_n2000_bound_neutral/RelaxBounds` | 0.095125 | 0.095328 | 1.0021 | 1.0021 1.0013 1.0033 |
| `f7_n2000_bound_physics/MakeConstraint` | 0.069619 | 0.069884 | 1.0038 | 1.0058 1.0022 1.0052 |
| `f7_n2000_bound_physics/MakeParameter` | 0.069648 | 0.069900 | 1.0036 | 1.0039 0.9985 1.0038 |
| `f7_n2000_bound_physics/RelaxBounds` | 0.069520 | 0.070022 | 1.0072 | 1.0075 1.0095 1.0037 |
| `f7_n5000_bound_neutral/MakeConstraint` | 0.244206 | 0.242035 | 0.9911 | 0.9899 0.9901 0.9915 |
| `f7_n5000_bound_neutral/MakeParameter` | 0.251128 | 0.249557 | 0.9937 | 0.9948 0.9937 0.9945 |
| `f7_n5000_bound_neutral/RelaxBounds` | 0.245202 | 0.242877 | 0.9905 | 0.9888 0.9898 0.9905 |
| `f7_n5000_bound_physics/MakeConstraint` | 0.182187 | 0.179502 | 0.9853 | 0.9830 0.9864 0.9861 |
| `f7_n5000_bound_physics/MakeParameter` | 0.182380 | 0.178839 | 0.9806 | 0.9800 0.9837 0.9823 |
| `f7_n5000_bound_physics/RelaxBounds` | 0.180932 | 0.178977 | 0.9892 | 0.9898 0.9888 0.9877 |

perf: NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.5570 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) -- AND the row counts are NOT matched: the head arm's leg runs 43 rows, the base arm's 41

| event | base median | head median | ratio |
|---|---|---|---|
| `L1-icache-load-misses:u` | 12791627 | 6234659 | 0.48740 |
| `branch-misses:u` | 38618433 | 39072668 | 1.01176 |
| `branches:u` | 15535075849 | 15656615957 | 1.00782 |
| `cycles:u` | 35803854201 | 36183517375 | 1.01060 |
| `instructions:u` | 104916505445 | 105771421719 | 1.00815 |

pass B: WORK-MOVED (instructions UP) -- THE VETO

| event | base median | head median | ratio |
|---|---|---|---|
| `cycles:u` | 35928321609 | 36192085133 | 1.00734 |
| `de_dis_uop_queue_empty_di0:u` | 1338195528 | 1367487814 | 1.02189 |
| `ic_fetch_stall.ic_stall_any:u` | 16754995100 | 17268996551 | 1.03068 |
| `instructions:u` | 105504181544 | 106224165606 | 1.00682 |
| `op_cache_hit_miss.op_cache_hit:u` | 14360639141 | 14360544439 | 0.99999 |
| `op_cache_hit_miss.op_cache_miss:u` | 1602717492 | 1594134256 | 0.99464 |

#### The excluded first row -- `f7_n1000_bound_neutral/MakeParameter`

base runs 0.998575, 0.330953, 0.316802; head runs 0.495463, 0.506703, 0.483071. The base arm's own three runs span 3.152x. It is the FIRST solve of the process and carries the one-time initialisation of everything below it; the band is computed WITHOUT it and this row is reported here instead.

Corpus WITH that row: base 10.8763 -> head 11.0430, ratio **1.0153**.
Corpus WITHOUT it (31 rows): base 10.5454 -> head 10.5475, ratio **1.0002** (+0.020 %); cells outside 0.99-1.01: **6**; band **UNRESOLVED**.


#### The millisecond-scale interior rows -- NO BAND (A7 (ii))

HS071 and the infeasible rows run in milliseconds and `--repeat` is HS-only (`bench_corpus.cpp`), so these carry NO wall band; their ratios are recorded, and the leg's instruction verdict above is what speaks for them.

| row | base median (s) | head median (s) | ratio |
|---|---|---|---|
| `hs071_x1_fixed/MakeConstraint` | 0.000130 | 0.000134 | 1.0300 |
| `hs071_x1_fixed/MakeParameter` | 0.000180 | 0.000195 | 1.0854 |
| `hs071_x1_fixed/MakeParameter/cap1` | 0.000083 | 0.000083 | 1.0077 |
| `hs071_x1_fixed/MakeParameter/solve_optimize` | 0.000190 | 0.000197 | 1.0359 |
| `hs071_x1_fixed/MakeParameter/warm_multiplier_seed` | 0.000119 | 0.000115 | 0.9675 |
| `hs071_x1_fixed/MakeParameter/warm_payload` | 0.000081 | 0.000089 | 1.0919 |
| `hs071_x1_fixed/RelaxBounds` | 0.000137 | 0.000137 | 1.0056 |
| `infeas2_spike/MakeParameter/stalled` | 0.000595 | 0.000626 | 1.0533 |
| `infeas2_stationary/MakeParameter/resto_infeasible` | 0.000192 | 0.000284 | 1.4824 |

HEAD-ONLY rows (no base arm, informational): `f7_n1000_bound_neutral/MakeConstraint/parts2`, `f7_n1000_bound_neutral/MakeParameter/parts2`

## Counter identity, leg 1 (recomputed from raw/)

| mode | cells | columns compared | rounds | differing columns |
|---|---|---|---|---|
| ipm | 27 | 75 | 3 | **NONE -- byte-identical** |
| ssn | 27 | 75 | 3 | **NONE -- byte-identical** |
| walk | 27 | 75 | 3 | **NONE -- byte-identical** |

## Instrument checks

The same arm, measured twice -- pass A against pass B. A leg whose own two passes disagree by more than 1e-4 cannot carry a 1e-4 instruction verdict.

| leg | arm | pass A instructions | pass B instructions | A/B |
|---|---|---|---|---|
| leg 1 / ipm / f7_n1000_bound_neutral | base | 749104475 | 749110069 | 0.99999 |
| leg 1 / ipm / f7_n1000_bound_neutral | head | 749451809 | 749457045 | 0.99999 |
| leg 1 / ipm / f7_n20000_bound_neutral | base | 15958310647 | 15958310649 | 1.00000 |
| leg 1 / ipm / f7_n20000_bound_neutral | head | 15965108235 | 15965108334 | 1.00000 |
| leg 1 / ipm / f7_n5000_bound_neutral | base | 3834858737 | 3834864367 | 1.00000 |
| leg 1 / ipm / f7_n5000_bound_neutral | head | 3836095428 | 3836096487 | 1.00000 |
| leg 1 / ssn / f7_n1000_bound_neutral | base | 488466427 | 488466160 | 1.00000 |
| leg 1 / ssn / f7_n1000_bound_neutral | head | 488889622 | 488889565 | 1.00000 |
| leg 1 / ssn / f7_n20000_bound_neutral | base | 11674687728 | 11674687773 | 1.00000 |
| leg 1 / ssn / f7_n20000_bound_neutral | head | 11683005904 | 11683006049 | 1.00000 |
| leg 1 / ssn / f7_n5000_bound_neutral | base | 2629019851 | 2629019484 | 1.00000 |
| leg 1 / ssn / f7_n5000_bound_neutral | head | 2630729726 | 2630729693 | 1.00000 |
| leg 1 / walk / f7_n1000_bound_neutral | base | 261282683 | 261282861 | 1.00000 |
| leg 1 / walk / f7_n1000_bound_neutral | head | 261625450 | 261631125 | 0.99998 |
| leg 1 / walk / f7_n20000_bound_neutral | base | 5640377762 | 5640377781 | 1.00000 |
| leg 1 / walk / f7_n20000_bound_neutral | head | 5645586494 | 5645580813 | 1.00000 |
| leg 1 / walk / f7_n5000_bound_neutral | base | 1333963362 | 1333964103 | 1.00000 |
| leg 1 / walk / f7_n5000_bound_neutral | head | 1335651610 | 1335651049 | 1.00000 |
| leg 2 / ipm / off | base | 247046485578 | 247046493255 | 1.00000 |
| leg 2 / ipm / off | head | 247321537000 | 246929524711 | 1.00159 |
| leg 2 / ipm / sink | base | 246877690892 | 247269978796 | 0.99841 |
| leg 2 / ipm / sink | head | 247153393777 | 247153382896 | 1.00000 |
| leg 2 / ssn / off | base | 420656001456 | 420655361626 | 1.00000 |
| leg 2 / ssn / off | head | 421231142644 | 421876613054 | 0.99847 |
| leg 2 / ssn / sink | base | 420693068673 | 420693315401 | 1.00000 |
| leg 2 / ssn / sink | head | 421914594237 | 421914642205 | 1.00000 |
| leg 2 / walk / off | base | 167893361469 | 167530254212 | 1.00217 |
| leg 2 / walk / off | head | 167708990254 | 168072046032 | 0.99784 |
| leg 2 / walk / sink | base | 167503453734 | 167866562347 | 0.99784 |
| leg 2 / walk / sink | head | 168071290524 | 167708080467 | 1.00217 |
| interior | base | 104916505445 | 105504181544 | 0.99443 |
| interior | head | 105771421719 | 106224165606 | 0.99574 |

### The interior leg's instruction comparison is NOT like-for-like

The head arm's leg runs 43 rows, the base arm's 41 -- the two `parts2` rows exist only at the head and no flag suppresses them. They are **0.71 %** of the head arm's measured wall. `perf stat` counts the whole process, so the interior instruction ratio above charges the head with two solves the base never ran, and an increase of that order is those rows rather than the code. The interior leg therefore carries NO instruction verdict at 1e-4.

### Attribution probe -- where leg 1's extra instructions are

`--dump-qp` on a `kNeutral` cell runs program start, model construction, the cell's FIRST QP build and the dump write, and solves nothing. Its delta is the NON-SOLVE share of leg 1's delta.

| probe cell | base instructions | head instructions | delta | ratio |
|---|---|---|---|---|
| `f7_n1000_bound_neutral` | 81388482 | 81388895 | +413 | 1.00001 |
| `f7_n20000_bound_neutral` | 1591880187 | 1591880599 | +412 | 1.00000 |

## Summary

| leg | corpus ratio | cells outside 0.99-1.01 | band | perf verdict |
|---|---|---|---|---|
| interior | 1.0002 | 6/31 | UNRESOLVED | NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.5570 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) -- AND the row counts are NOT matched: the head arm's leg runs 43 rows, the base arm's 41 |
| leg1/ipm | 0.9999 | 0/27 | FLAT | WORK-MOVED (instructions UP) -- THE VETO |
| leg1/ssn | 0.9999 | 0/27 | FLAT | WORK-MOVED (instructions UP) -- THE VETO |
| leg1/walk | 1.0009 | 0/27 | FLAT | WORK-MOVED (instructions UP) -- THE VETO |
| leg2/ipm/off | 0.9917 | 23/27 | MOVED | NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) |
| leg2/ipm/sink | 0.9912 | 24/27 | MOVED | NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) |
| leg2/ssn/off | 0.9782 | 20/27 | MOVED | NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) |
| leg2/ssn/sink | 0.9816 | 17/27 | MOVED | NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) |
| leg2/walk/off | 0.9798 | 24/27 | MOVED | NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)) |
| leg2/walk/sink | 0.9829 | 16/27 | MOVED | NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary disagree by 0.0793 %, above the identity band; the ratio below is reported, not classified (original label: WORK-MOVED (instructions UP) -- THE VETO) |

---

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
