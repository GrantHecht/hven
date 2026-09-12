# The single-row interior process, and the instruction verdict on `9cebbbe`

M6 W5 T8.9r-attrib5, 2026-09-11, same box.

`reading.md` §5 (iv) and §11 both stopped at the same wall. The interior leg
returns **no instruction verdict**, because a `--engine interior` process writes
33 / 41 / 43 rows depending on the arm and `perf stat` counts the process, and
because the count is dominated by the first row's warm-up — the row the wall
reading excludes by a pre-declared positional rule and a counter cannot. §11
measured that floor at **+1.27 %** on a byte-identical control pair, larger than
every like-for-like step under test, and said outright that a single-row interior
process would fix it and *"is not reachable without a source change"*.

The source change is `d5931e8`. This directory is what it measured.

---

## 1. The answer in one line

**`9cebbbe`'s solve does NOT execute more instructions.** With MKL's first-call
clock calibration set aside inside each profile, the culprit and the parent run
the same n20000 row to within **0.3–0.6 %**, in the marginally *fewer* direction,
against a byte-identical control pair separated by 0.02–0.14 % — and the three
rounds' values **interleave between the two arms**. This is **NOT WORK-MOVED**.
It does not reach §11.1's LAYOUT-MOVED band either (identical within 1e-4), so no
§11.1 label is claimed from it.

**And a second finding the brief did not ask for — WITHDRAWN AND REPLACED AT FIX
ROUND 3 (settler ruling R12; astra's fix2 review, item 4).** This paragraph said
*"the +2.5 % step itself does not appear in a single-row process at all"*, on
`perf.csv`'s WHOLE-PROCESS `elapsed_s` (0.995–1.009). **That is withdrawn.** Read
on `wall_s` — the leg's own SOLVE BRACKET, and the column §5/§11/§12/§13 sum —
the same retained rounds give **corpus 1.0110**, five of the eleven rows above
1.01 and each of those slower in **all five paired rounds**, against a control of
0.9999; passes B and C reproduce the corpus at 1.0108 and 1.0111. **The step is
SMALLER in a single-row process (~1.1 %), not absent.** The whole process is
nevertheless unchanged (+0.04 %), because the culprit's out-of-bracket time falls
by 14.1 % as its bracket rises: a BOUNDARY MOVE, measured in §7 below and in
`wall_bracket.out`. Cycles and calibration-excluded instructions remain flat.

---

## 2. The arms, and the one STOP condition

`arms.txt`. Both arms are `git archive` extractions with the lever's edits
applied by `scripts/adapt_arm.py` (the committed diff does not apply to trees
that old), the two adapted patches **byte-identical to each other**, built
Release / TESTS=OFF from an emptied directory at one absolute path. The STOP
condition the brief set — each arm's `libhven.a` must equal the earlier legs' —
**passed on both**: `6f0dc91e2432…` reproduces `../../attribution/arms.txt`'s a04
and `../arms.txt`'s b01, `25027b509a02…` reproduces `../arms.txt`'s b03. The
control is 510a4bb's executable `cmp`-equal at a second path.

Every child's argv byte length is identical across the three arms for the same
row (the three arm directories are the same length); every run prints its own
`ARGV_LEN` and the batch logs carry them.

---

## 3. What the whole-process counts say, and why they are not the verdict

Pass A, `instructions:u`, medians of five rounds, eleven scored rows
(`classify.out` has all of it, and branches and cycles beside it):

| row | parent e9 | culprit e9 | control e9 | c/p | x/p |
|---|---|---|---|---|---|
| `f7_n1000_bound_physics/MakeConstraint` | 4.7528 | 4.0673 | 4.7498 | **0.85576** | 0.99935 |
| `f7_n1000_bound_physics/RelaxBounds` | 4.7543 | 4.0686 | 4.7517 | **0.85577** | 0.99946 |
| `f7_n5000_bound_physics/MakeParameter` | 6.0329 | 5.3862 | 6.0367 | **0.89280** | 1.00062 |
| `f7_n5000_bound_physics/MakeConstraint` | 5.3205 | 4.7795 | 5.3221 | **0.89831** | 1.00030 |
| `f7_n5000_bound_physics/RelaxBounds` | 5.3226 | 4.7593 | 5.3245 | **0.89418** | 1.00036 |
| `f7_n10000_bound_neutral/MakeParameter` | 8.7929 | 8.2743 | 8.7938 | **0.94103** | 1.00011 |
| `f7_n10000_bound_neutral/MakeConstraint` | 7.0281 | 6.7903 | 7.0341 | **0.96617** | 1.00086 |
| `f7_n10000_bound_neutral/RelaxBounds` | 6.9885 | 6.7899 | 7.0095 | **0.97159** | 1.00301 |
| `f7_n20000_bound_neutral/MakeParameter` | 12.2102 | 12.0365 | 12.2109 | **0.98577** | 1.00006 |
| `f7_n20000_bound_neutral/MakeConstraint` | 14.1421 | 13.6474 | 14.1397 | **0.96502** | 0.99983 |
| `f7_n20000_bound_neutral/RelaxBounds` | 14.1426 | 13.6620 | 14.1426 | **0.96602** | 1.00000 |

The instrument's own floor — the byte-identical control against the parent — is
**0.30 % at worst and 0.05 % typically**, against §11's whole-process floor of
**+1.27 %**: the single-row process is four to twenty-five times tighter, which is the
thing the lever was built for and it delivered it. Cycles, on the same rows, read
**0.996–1.008** against a control of 0.998–1.003: flat.

**Eleven of eleven rows go DOWN, by 5× to 48× that floor.** Read at face value
that is an emphatic "not WORK-MOVED". It is not read at face value, because
**most of each of these numbers is not the row**.

### What the per-process constant is, named

`perf record -g` on a single-row `hs071_x1_fixed` process — four variables, dense,
**0.00018 s** of solve inside a warm leg — puts **89.5 %** of that process in two
symbols: `difftime` (63.6 %) and `mkl_serv_get_clocks_frequency` (25.9 %), both
reached through `___pthread_once` from **`hven::solvers::ensure_solver_initialized()`**.
That is MKL's `dsecnd()` first-call clock calibration, which hven calls
deliberately and once per process:

* `src/drivers/solver_init.cpp:27` — `dsecnd();` inside `std::call_once`
* `src/drivers/interior_point_solver.cpp:798` — the member that forwards to it
* `src/drivers/interior_point_solver.cpp:5535` — the call site, inside the solve

It is a **wall-timed busy-wait of about 0.95 s**. The leg proper pays it **once for
43 rows** — its committed artifact shows the first row at 0.602 s and the next
`n1000` rows at 0.033–0.042 s, and `hs071_x1_fixed` at 0.00018 s. **The single-row
instrument pays it once per row.** In the sampled profiles it is **16.7–27.2 % of
an `n20000` process and 82.4–84.0 % of an `n1000` one** (`record_analyze.out`'s
`cal %` column, per round; "17 %" and "96 %" were quoted here at first and were
neither profile's reading — corrected at fix round 3). Its instruction count is a
measure of how fast that loop's own code runs — an alignment-sensitive property
of MKL's binary — not of anything the solver does. And because it is WALL-TIMED
and of fixed duration, it enters both arms' `wall_s` equally: it DAMPS the wall
ratios below rather than creating one.

**This is also, finally, what §5 (iv) and §11 were calling "the first row's
warm-up".** It has a name, a file and a line now.

### The differencing repair, and its refusal

Pass S measures the constant directly: a single-row `hs071_x1_fixed` process at
each arm. Medians 4.612 e9 (parent), 3.899 e9 (culprit), 4.614 e9 (control) — the
control reproducing the parent to 0.05 %, as it must.

Subtracting it row by row would invert the whole table, reading **+2.3 % to
+24.3 %** — WORK-MOVED. **That difference is refused, and the reason is measured
rather than feared**: the constant is not constant. Pass S's own five rounds
spread **29.9 %** at the parent (3.241 to 4.621 e9); the sampled profiles put the
calibration at **2.9 e9** inside an `n20000` process at the same arm whose `hs071`
process reads 4.6 e9; and one `f7_n1000_bound_physics/MakeParameter` sample read
0.9 e9 where its siblings read 4.2. A term that moves by 5× is not a term to
subtract. `reading.md` §5 (iv) refused a differencing instrument for a related
reason; this leg refuses this one, on its own evidence.

---

## 4. The verdict, per symbol

`record_analyze.out`. `perf record -e instructions:u -c 2000000`, three rounds per
arm, calibration set aside **inside each profile** — so nothing is assumed about
its size:

| row | parent e9 | culprit e9 | control e9 | c/p | x/p |
|---|---|---|---|---|---|
| `f7_n20000_bound_neutral/MakeParameter` | 11.3220 | 11.2600 | 11.3065 | **0.99452** | 0.99863 |
| `f7_n20000_bound_neutral/MakeConstraint` | 11.2270 | 11.1928 | 11.2247 | **0.99696** | 0.99980 |
| `f7_n1000_bound_physics/MakeConstraint` | 0.6781 | 0.5718 | 0.6539 | 0.84322 | 0.96433 |

The `n1000` row is **reported and not scored**: 83 % of that profile is
calibration, its non-calibration part draws ~340 samples, and the control itself
reads 0.964 — the sampling noise is larger than anything being asked about.

On the two `n20000` rows the per-round values **interleave**: parent
11.2606 / 11.3220 / 11.3525, culprit 11.2052 / 11.2600 / 11.2742, control
11.2879 / 11.3065 / 11.3066. **The culprit's solve executes the same instructions
as the parent's, to within 0.3–0.6 %, in the marginally fewer direction.**

`perf diff --sort symbol` (`perf-diff/`) says the same thing the other way, **with
its maximum corrected at fix round 3** (astra's fix2 review, item 4): the largest
SHARE GAIN by any named solver symbol is **+0.51 percentage points** —
`mkl_pds_lp64_blkl_ll_real.extracted` on the `MakeConstraint` row, where this
paragraph first read +0.34 off the `MakeParameter` row — and the rest move by a
few tenths of a point either way. **A share change is not a bound on that
symbol's instruction increase** and is not read as one: it says the symbol takes
a larger slice of a profile whose non-calibration total is flat, which is what
the calibration shrinking does to every other slice. The byte-identical CONTROL
pair moves shares by up to **0.95 points** on the same rows
(`perf-diff/*-control-vs-parent.txt`), which is the scale to read this column
against. **No function got materially more expensive.**

---

## 5. Pass B and pass C

Pass B, on the eleven scored rows (medians of five; control in brackets):

* `de_dis_uop_queue_empty_di0` — the front end failing to deliver — is **UP
  13.9 % to 26.0 %** at the culprit on every row (control 0.99–1.00).
* `ic_fetch_stall.ic_stall_any` is **UP 2.5 % to 8.0 %** (control 0.998–1.001).
* `op_cache_hit_miss.op_cache_miss` is **DOWN 4.4–5.7 %** and `op_cache_hit`
  **DOWN 1.1–6.0 %** — both tracking the shorter instruction stream.

So the culprit's process issues fewer instructions, in the same cycles, with a
front end that is starved more often. **On the whole-process scale that is a
front-end/layout signature, not a work one** — but it is a signature of the
calibration loop as much as of the solve, and this leg does not separate them per
event, so it is reported and not leant on.

Pass C, same rows:

* `L1-dcache-load-misses` **+0.4 % to +1.09 %** at the culprit (the maximum is
  1.0946 on `f7_n10000_bound_neutral/RelaxBounds`; "+0.9 %" was the second-largest
  — corrected at fix round 3), control ±0.5 %.
* `dTLB-load-misses` within ±1.3 %, control within ±1.8 % — **no verdict**.
* `page-faults` **flat to within half a per mille on every row** — the scored
  ratios span **0.995080–1.000041**, which is four figures and not the five this
  line first claimed (corrected at fix round 3) — on a within-arm spread under 6
  counts in 10 000. The single-row
  process's fault count is a per-process constant, and the culprit does not move
  it: §12's **+53 149 faults** and §13's **+41 627** are a property of the
  **43-row leg process**, not of one row.
* `LLC-load-misses` reads **`<not supported>` on this PMU** and is reported
  **ABSENT**, never zero-filled.

---

## 6. The twelfth row, and the batch-position effect

§11 scored eleven rows: four cells × three treatments minus
`f7_n1000_bound_physics/MakeParameter`, which its pre-declared positional rule
excluded as the process's warm-up. **This leg runs all twelve** — every row is its
own process here, so no row is any other row's warm-up — and the twelfth is
reported beside the eleven rather than folded in, because **it is the one row
whose counts are not stable**: 0.914 / 4.203 / 4.199 / 3.835 / 4.188 e9 at the
parent, a 79 % spread, where every other row spreads under 1.7 %.

The cause is the calibration again, and it is positional: the **first process of
each batch** pays a cheaper calibration (0.9–2.9 e9 rather than 4.2–4.6 e9), and
the arm order rotates with the round, so that first process is always
`f7_n1000_bound_physics/MakeParameter` at whichever arm leads. The effect is
therefore confined to the twelfth row and lands on a different arm each round —
noise in one unscored row, not bias in the eleven. It is disclosed rather than
smoothed.

---

## 7. The solve bracket, and the boundary move (fix round 3)

`wall_bracket.py` / `wall_bracket.out`, from the 585 retained row CSVs and
`perf.csv`. **Neither column here is asserted** (CLAUDE.md §7): this leg carries
no R2' evidence at all, and both wall populations are informational.

`wall_s` is the leg's own SOLVE BRACKET — `bench/ipm_corpus_leg.cpp:389-392`, the
same three lines at both arms — and it is the column `reading.md` §5, §11, §12
and §13 sum into their corpus figures. `elapsed_s` is the whole process.

| pass A, medians of five | parent | culprit | control | **c/p** | x/p | c>p |
|---|---:|---:|---:|---:|---:|---:|
| `f7_n1000_bound_physics/MakeConstraint` | 0.957310 | 0.958355 | 0.957168 | 1.001093 | 0.999852 | 4/5 |
| `f7_n1000_bound_physics/RelaxBounds` | 0.957358 | 0.958267 | 0.957144 | 1.000950 | 0.999777 | 3/5 |
| `f7_n5000_bound_physics/MakeParameter` | 1.070456 | 1.076753 | 1.069078 | 1.005882 | 0.998712 | 4/5 |
| `f7_n5000_bound_physics/MakeConstraint` | 0.919128 | 0.927189 | 0.918988 | 1.008770 | 0.999848 | **5/5** |
| `f7_n5000_bound_physics/RelaxBounds` | 0.919426 | 0.921316 | 0.919348 | 1.002055 | 0.999915 | 4/5 |
| `f7_n10000_bound_neutral/MakeParameter` | 1.245887 | 1.258229 | 1.245891 | 1.009906 | 1.000003 | **5/5** |
| `f7_n10000_bound_neutral/MakeConstraint` | 0.878224 | 0.890860 | 0.878389 | **1.014388** | 1.000189 | **5/5** |
| `f7_n10000_bound_neutral/RelaxBounds` | 0.870673 | 0.890178 | 0.873140 | **1.022402** | 1.002833 | **5/5** |
| `f7_n20000_bound_neutral/MakeParameter` | 1.390526 | 1.418436 | 1.388294 | **1.020071** | 0.998394 | **5/5** |
| `f7_n20000_bound_neutral/MakeConstraint` | 1.790536 | 1.814459 | 1.791744 | **1.013361** | 1.000674 | **5/5** |
| `f7_n20000_bound_neutral/RelaxBounds` | 1.793515 | 1.819266 | 1.792350 | **1.014358** | 0.999351 | **5/5** |
| **CORPUS** | **12.793039** | **12.933306** | **12.791534** | **1.010964** | **0.999882** | — |

Passes B and C — independent round sets of the same eleven rows — give corpus
**1.010819** and **1.011110**. The twelfth, unscored row reads 0.9987 / 1.0012 /
0.9973 and is excluded by §11's positional rule as everywhere else.

**AND THE TWO WINDOWS DISAGREE FOR A MEASURED REASON.** Their within-arm spreads
are the same size, so this is not resolution. Over the eleven rows, in seconds:

| totals, medians of five | process `elapsed_s` | bracket `wall_s` | outside the bracket |
|---|---:|---:|---:|
| parent `510a4bb` | 13.75595 | 12.79304 | 0.96132 |
| culprit `9cebbbe` | 13.76119 | 12.93331 | 0.82593 |
| control | 13.75220 | 12.79153 | 0.96098 |
| **culprit / parent** | **1.000381** | **1.010964** | **0.859162** |
| control / parent | 0.999727 | 0.999882 | 0.999644 |
| culprit − parent (s) | **+0.0052** | **+0.1403** | **−0.1354** |

The bracket gains 0.140 s, the time outside it loses 0.135 s, the whole process
is unchanged, and the control moves none of the three. **That is a BOUNDARY
MOVE** — and it is what `9cebbbe` describes: the model is *borrowed for the
call*, so binding and keying it happen inside `solve()` rather than before it.
The out-of-bracket term scales with n like a model build (0.012 s at n1000,
0.176 s at n20000) and shrinks 10–15 % at every row. **Stated as measured and no
further**: the compensation is near-exact but not proven exact, and nothing here
says how much of the 43-row LEG process's +2.5 % is the same move — no leg batch
recorded its per-row out-of-bracket time, so that question cannot be answered
from any retained artifact. It is consistent with §4's instruction verdict rather
than against it: a boundary move relocates work without adding any.

---

## 8. What this leg does NOT claim

* **No wall claim.** Both wall populations — `classify.out`'s process elapsed and
  §7's solve bracket — are informational (CLAUDE.md §7), and this leg carries no
  R2' evidence at all. Nothing here is quoted as a timing. What §7 is offered
  against is the sentence it replaces, which rested on the same unproven batches
  and on the wider of the two windows.
* **No disposition.** §11.1 and the owner have it.
* **No claim that §11/§12/§13 are wrong.** They measured the leg's process; this
  measured a single-row one. What this adds is that the extra time is **not extra
  instructions**, and that the single-row process does not reproduce the step —
  which is a constraint on any mechanism, not a refutation of the step.
* **No claim about a cell not measured here.** Four F7 cells, three treatments,
  one subtrahend cell.
* **Apple/Accelerate and Windows: UNOBSERVED.**

---

## 9. Files

```
single-row/
  classification.md     this reading
  arms.txt              the three arms, both hashes each, the recipe, the STOP check
  adapted-lever.patch   the lever as applied to both arms (byte-identical between them)
  perf.csv              3 510 records: pass, arm, cell, treatment, round, event, value
  classify.py(.sha256)  the analysis; prints every whole-process table from perf.csv alone
  classify.out          its saved output
  record_analyze.py(.sha256)  the per-symbol analysis, calibration set aside
  record_analyze.out    its saved output -- THE VERDICT
  perf-diff/            perf diff --sort symbol, culprit-vs-parent and control-vs-parent
  wall_bracket.py(.sha256)    the SOLVE-BRACKET reading (fix3), from raw/csv/ and perf.csv
  wall_bracket.out            its saved output
  perf-report/report/   the 27 sampled profiles' `perf report --stdio` text (fix3),
                        which is what record_analyze.py now reads, so the calibration
                        exclusion reproduces from this directory alone
  perf-report/data/     the raw perf.data those reports came from, retained beside them,
                        with the hs071 call-graph profile that names the two symbols
  raw/perf{A,B,C,S}/    every perf stat output, 585 files
  raw/csv/              every row the measured processes wrote, 585 files
  logs/                 every batch and build log: each carries its pgrep capture taken
                        SEPARATELY outside the lock and again INSIDE it, its
                        FOREGROUND_START/END pair, its ARGV_LEN lines and its
                        WRAPPER_EXIT; G0-fold-proof.log is the worst-fold proof
  scripts/              every script the legs ran, and the commit-1 gate scripts
```
