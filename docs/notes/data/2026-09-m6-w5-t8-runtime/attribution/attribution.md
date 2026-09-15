# M6 W5 T8.9r-attrib — leg 1's instruction increase, attributed per group-1 task

**What this is.** `reading.md` §3 records that on every leg-1 perf cell the group-1 head executes
+0.03 % (ipm) … +0.13 % (walk) more instructions than 102f729, inside the solve, scaling with `n`,
with byte-identical counters; §11.1.1 sends that to the owner **with the attribution recorded**. This
directory is that attribution: the same nine cells, the same pin, measured at **eleven arms** — the
base and every group-1 code head in order — so the increase can be charged to the task that added it.

**It is a measurement, not a disposition.** It asserts no wall-clock number of any kind and none was
taken; `cycles` is recorded for the record only (CLAUDE.md §7). Apple/Accelerate and Windows:
UNOBSERVED.

---

## 1. The answer in one line

**All of it is T8.4, and T8.4 declared it.** On every one of the nine cells the single work step is
the base→T8.4 step, and it is the whole end-to-end delta. **SEVEN other steps fall outside this
leg's ±2e-5 floor** (T8.3 on five cells, T8.1 and T8.9 on `walk`/n1000, spanning −2.69e-4 to
+1.57e-4) and every one of them is a code-layout cluster transition, not work, by the branch-density
fingerprint of §3 — 2.32–2.48 against executed work's 1.05–1.11, with nothing in between. Fix1's
summary here said "two visible movements"; it is seven, and §3's table says so (astra's fix1 review,
item 8). The mechanism T8.4's design and ledger declare — the shared declared diagnostics computed once
per call over the declared NLP (`compute_declared_diagnostics`), and the declared-space vectors the
new `SolveResult` carries by value — executes per call and is `O(n)`, which is exactly the shape the
measurement has. **The UNDECLARED list is EMPTY.**

The absolute step is the SAME on all three QP tiers and linear in `n`:

| task | ipm n1000 | ssn n1000 | walk n1000 | ipm n5000 | ipm n20000 |
|---|---|---|---|---|---|
| T8.4 step, instructions | +342 533 | +341 797 | +340 870 | +1 693 182 | +6 731 982 |

≈341 k at n = 1000, ≈1.688 M at n = 5000 (4.95×), ≈6.73 M at n = 20000 (19.7×) — linear in `n` to
better than 2 %, and mode-independent in absolute terms to better than 0.5 %. **That is why §3's
fractions differ by mode**: it is one fixed `O(n)` cost divided by three different totals. walk is
the cheapest solve, so the same work reads +0.13 % there and +0.03 % on ipm. §3's "+0.03 %…+0.13 %"
is one number, not nine.

---

## 2. How it was measured, and the one thing that had to be controlled for

Eleven `git archive` arms, built ONE AT A TIME from an empty build directory at one absolute path on
the artifact's own recipe; `arms.txt` has the shas, the two submodule pins, both binaries' sha256 per
arm and the measurement pin. Five rounds per (arm, mode, cell) — 495 `perf stat` runs, all in
`raw/perf/`, every leg log in `raw/logs/`, every script in `raw/`.

**The build-recipe calibration is exact.** All three libraries `PROVENANCE.txt` retained reproduce
here BYTE-IDENTICALLY — `735eea1d…` (102f729), `d236166e…` (b9848bf), `60bfe03f…` (e51a7e0). The
`hven_sqp_corpus` executables do not and cannot: they embed the build tree's absolute paths, and this
leg builds at a different path.

**And one instrument fact had to be controlled for before any of this could be read.**
`instructions:u` on these cells is **bimodal in the byte footprint of the measured process's argv and
environment**. `raw/logs/A12-layout-probe.log` is the control: ONE binary, ONE cell, ONE pin, varying
**nothing but the length of its `--internal-out` path**, lands in one of two clusters ~1.2e-4 apart —
on `ipm`/`n1000`, 749 195 808 at one padding and 749 119 854 at another. That is the same phenomenon
T6.d's finding F1 recorded on HS and `reading.md` §4 quotes; what this leg adds is the **controlling
variable**.

Two consequences, both stated rather than assumed away:

* **`reading.md`'s leg-1 finding is unaffected.** Its two arms' invocations were length-matched by
  construction — `arm-base` / `arm-head`, `A-base-` / `A-head-`, equal byte for byte — so both arms
  sat in the same cluster and the cluster term cancelled. Its 2e-5 floor is a *within-layout* floor,
  and it is correct as such.
* **This leg locks the layout.** Every arm's measured child has a byte-identical argv but for the two
  digits of its arm number (243 bytes on all eleven), from a shell at a fixed cwd, `OLDPWD` and
  nesting depth, with an identical 4374-byte environment (one md5 on all eleven, logged per arm). The
  `_argv_pad_12` component of the output path is the twelve bytes that make this leg's footprint
  reproduce the artifact's, so the two are comparable cell by cell.

---

## 3. Work or layout: the branch-density fingerprint

The layout probe also supplies the **discriminator**, which is what lets a ±1e-4 movement be read
rather than merely reported. A layout cluster transition moves branches by ≈2.4× the instruction
fraction; executed work moves them by ≈1.1× — this program's own average branch density. Measured:

| | instruction fraction | branch fraction | ratio |
|---|---|---|---|
| layout probe, ipm/n1000, padding only | −1.01e-4 | −2.48e-4 | **2.46** |
| every T8.4 step (nine cells) | +4.2e-4 … +1.31e-3 | +4.6e-4 … +1.43e-3 | **1.05 – 1.11** |
| every other step outside the band (**seven**) | ±2.0e-5 … ±2.7e-4 | ±5.0e-5 … ±6.5e-4 | **2.32 – 2.48** |

`attribute.py` prints this ratio for every step outside the band and labels it. **No step came back
UNCLASSIFIED.** The separation is clean: 1.05–1.11 on one side, 2.32–2.48 on the other, nothing
between.

This also refines `reading.md` §3's argument. §3 reads "branches move with instructions, by the same
fraction, in every cell — so this is executed work". The proportionality is the right test and its
conclusion there is right — the ratio on that comparison is 1.05–1.11. But proportionality *alone*
does not distinguish work from a layout cluster, because a cluster moves branches too; it is the
*value* of the ratio that separates them, and §3's cells sit at the work value.

---

## 4. The tables — per-task steps, as a fraction of the base arm

`attribute.py` prints these from `table.csv`, with the per-round values, the spreads and the
verdicts. Reproduced here as fractions of the base arm (102f729); **bold** = outside the ±2e-5 band.

### ipm

| task | n1000 | n5000 | n20000 | verdict |
|---|---|---|---|---|
| T8.1 | +0.000000 | +0.000000 | +0.000000 | flat (control arm — library byte-identical to base) |
| T8.2 | +0.000000 | +0.000001 | +0.000000 | flat |
| T8.3 | +0.000001 | **−0.000125** | −0.000000 | LAYOUT (b/i 2.45) |
| T8.4 | **+0.000457** | **+0.000442** | **+0.000422** | **WORK** (b/i 1.10–1.11) |
| T8.5 | +0.000002 | −0.000000 | +0.000000 | flat |
| T8.6 | +0.000001 | +0.000000 | −0.000000 | flat |
| T8.7 | −0.000007 | −0.000001 | −0.000000 | flat |
| T8.7b | −0.000004 | +0.000000 | −0.000000 | flat |
| T8.8 | +0.000014 | +0.000006 | +0.000004 | flat (positive on every ipm/ssn cell — §5) |
| T8.9 | +0.000000 | +0.000000 | −0.000000 | flat |
| **cumulative** | **+0.000465** | **+0.000323** | **+0.000426** | §3's 1.00046 / 1.00032 / 1.00043 |

### ssn

| task | n1000 | n5000 | n20000 | verdict |
|---|---|---|---|---|
| T8.1 | +0.000001 | +0.000000 | −0.000000 | flat |
| T8.2 | −0.000000 | +0.000000 | +0.000000 | flat |
| T8.3 | **+0.000157** | +0.000000 | **+0.000130** | LAYOUT (b/i 2.32, 2.35) |
| T8.4 | **+0.000700** | **+0.000641** | **+0.000576** | **WORK** (b/i 1.05–1.06) |
| T8.5 | +0.000002 | +0.000000 | +0.000000 | flat |
| T8.6 | −0.000009 | +0.000000 | −0.000000 | flat |
| T8.7 | +0.000011 | −0.000000 | +0.000000 | flat |
| T8.7b | +0.000000 | +0.000000 | −0.000000 | flat |
| T8.8 | +0.000007 | +0.000007 | +0.000006 | flat |
| T8.9 | +0.000001 | +0.000000 | +0.000000 | flat |
| **cumulative** | **+0.000867** | **+0.000649** | **+0.000713** | §3's 1.00087 / 1.00065 / 1.00071 |

### walk

| task | n1000 | n5000 | n20000 | verdict |
|---|---|---|---|---|
| T8.1 | **+0.000020** | −0.000000 | +0.000000 | LAYOUT (b/i 2.46) — and the library is byte-identical to base |
| T8.2 | −0.000019 | −0.000004 | −0.000000 | flat |
| T8.3 | **+0.000024** | +0.000005 | **−0.000269** | LAYOUT (b/i 2.32, 2.42) |
| T8.4 | **+0.001305** | **+0.001264** | **+0.001192** | **WORK** (b/i 1.09) |
| T8.5 | +0.000002 | −0.000004 | +0.000000 | flat |
| T8.6 | +0.000006 | +0.000005 | +0.000000 | flat |
| T8.7 | −0.000004 | −0.000000 | −0.000000 | flat |
| T8.7b | +0.000003 | +0.000000 | −0.000000 | flat |
| T8.8 | −0.000003 | +0.000001 | −0.000000 | flat |
| T8.9 | **−0.000021** | −0.000000 | +0.000000 | LAYOUT (b/i 2.48) |
| **cumulative** | **+0.001314** | **+0.001265** | **+0.000923** | §3's 1.00131 / 1.00127 / 1.00092 |

**Closure.** The ten steps sum to the measured end-to-end delta on every cell, residual < 1e-9 —
arithmetic, since both are differences of the same medians, and printed so a reader can see nothing
was dropped.

**The band is below the spread on three cells.** `walk`/`n1000` (worst five-round spread 1.33e-4),
`walk`/`n5000` (5.76e-5) and `ipm`/`n1000` (2.76e-5) have a round-to-round spread larger than the
±2e-5 band, so a ±2e-5 flag on those cells is not resolvable and is not read as one. The three flags
that fall there — walk/n1000's T8.1, T8.3 and T8.9 — are all at the layout ratio, and **T8.1's
library is byte-identical to the base's**, so its +2.0e-5 is a direct measurement of that cell's
floor and nothing else it could be. The only step resolvable on every cell is T8.4's.

---

## 5. Each task with a step, and the mechanism its design declared

**T8.4 — the one work step, on all nine cells. DECLARED.**
Design §2.3 (`docs/notes/2026-09-m6-w5-t8-design.md`): `SolveResult` is a value carrying `x`,
`lambda_e`, `lambda_i`, `z`, `ce`, `ci` in **declared space and caller units**, and the four shared
diagnostics are "a reporting ADDITION computed by one function in `solve_result.cpp`" from
`(x, lambda_e, lambda_i, z, f, ce, ci, grad, Je, Ji)` at the returned point — inf-norms over the
declared coordinates, plus the canonical two-sided bound products. §2.7 names it as behaviour change
**(7) "the shared diagnostics added to every result"**. The ledger's T8.4 close line names the
function, `compute_declared_diagnostics`, "(canonical two-sided bound products, excluded coordinates
skipped, **one loop after fix1**)", and the `IpmResult` additions including the two internal
fixing-row vectors. Every one of those executes per public call and is linear in the declared
dimension — which is what was measured: `O(n)`, once per call, identical on all three QP tiers.
Nothing here is a per-iteration cost; the measurement excludes that by its own `n`-linearity and by
its indifference to the QP tier.

**T8.3 — no work step; a code-layout cluster transition. Not UNDECLARED, because it is not work.**
Four cells move by −2.7e-4 … +1.6e-4 with **mixed sign across cells of the same mode** and a branch
ratio of 2.32–2.45, the layout probe's fingerprint to two figures. T8.3 rewrote 26 source files
(options as values), so the binary's code layout moved; the cells that moved are the ones whose
cluster changed with it. Signs that differ between `n5000` and `n20000` of the same mode are the
shape of placement, not of work — `reading.md` §11.2's own LAYOUT-vs-WORK rule, here with the
fingerprint to apply it. The per-call mechanism T8.3 did declare — the options fingerprint on hot
reuse (design §2.7 item (8)) — costs nothing measurable on these cells.

**T8.8 — below the band, but positive on every ipm and ssn cell.** +1.4e-5, +6e-6, +4e-6 (ipm) and
+7e-6, +7e-6, +6e-6 (ssn); on walk, nothing. That is the sign and the shape `MklThreadScope` around
every backend call would have (design §2.6, v3.6 at the T8.8 close: applied around every backend
call, 0 = untouched, undone after each call) — a fixed cost per backend call, so it is largest on the
cell with the most calls per unit of work and absent on the tier that makes fewest. **It is below
this leg's band and is reported as an observation, not a step.** DECLARED either way.

**T8.6 and T8.7 / T8.7b — the per-iteration mechanisms the brief expected to find, and did not.**
T8.6's callback dispatch per row, T8.7's ledger/trace composition and event counting, T8.7b's IPM
message events: all flat on all nine cells, at or below 1e-5. The corpus attaches no callback and no
sink, so what these tasks added is a branch not taken per row, and it costs nothing measurable. They
are declared mechanisms that do not execute here; the reading records that they are cheap **when
absent**, and says nothing about their cost when attached — no cell here attaches one.

**T8.1, T8.2, T8.5, T8.9 — flat on every cell.** T8.1's library is byte-identical to the base's.

### The UNDECLARED list

**EMPTY.** No task added an instruction step this leg can resolve that its design or ledger close
line does not declare.

---

## 6. Calibration against the artifact, in full

**CORRECTED AT FIX2 (settler ruling R9; astra's fix1 review item 5), and the correction is the
REFERENCE, not the arithmetic.** `attribute.py` hardcoded ROUND 1's nine count pairs while its own
output line said "reading.md section 3" — and round 1's captures were superseded by the fix1
re-measurement, so this section was calibrating against a reading the artifact no longer publishes.
The hardcode is now **reading.md v2 §3's** counts. Both results are recorded below, because
changing the reference changes the numbers and a reader is entitled to see which reference each one
belongs to. **`attribute.py`'s tables, steps and closure are untouched by this** — the calibration
block is the only part of its output that moved.

**Against `reading.md` v2 §3 — THE CURRENT REFERENCE.**

* *Ratio form, the quantity being attributed and layout-free:* **worst deviation 5.48e-06** across
  all nine cells, roughly four times finer than the artifact's own 2e-5 leg-1 floor. (Round 1's
  sentence "every cell agrees to six decimals" was **false against either reference** and is
  withdrawn: `ipm`/`n1000` reads 1.000465 here against 1.000459 there. What is true is the bound.)
* *Absolute form:* **16 of the 18 end-arm counts inside 2e-5; worst 3.26e-05.** The **two** outside
  are `walk`/`n1000` — base **−3.13e-05**, head **−3.26e-05** — the smallest cell, and both
  NEGATIVE.

**Against round 1's counts — THE SUPERSEDED REFERENCE this file used to quote.** Ratio form: worst
**1.87e-06**. Absolute form: **12 of 18 inside 2e-5, worst 4.16e-05**, with **six** outside —
`ipm`/`n1000` (+2.05e-5 base, +2.15e-5 head), `ssn`/`n1000` (+2.17e-5, +2.26e-5), `walk`/`n1000`
(+3.97e-5, +4.16e-5). Round 1's own sentence here said "13 of 18 and not on 5"; **that count was
wrong on its own reference as well as being taken against the wrong one**, and astra's item 5 is
what found it.

**What the failures are, on either reference.** Every deviation is confined to the three smallest
cells, and on each reference base and head of the same cell move TOGETHER — common-mode to within
about 2e-6 against round 1, and both `walk`/`n1000` deviations negative and within 1.3e-6 of each
other against v2 — which is why the ratio form is barely touched by a term that visibly moves the
absolute counts. This is the residual of the process-layout term of §2, which no rebuild in a
different session can null out: the environment of the measuring shell is part of the measured
process's footprint, and it is not the same shell. **The brief asks the end arms to reproduce the
artifact's counts to 2e-5; against the current reading they do so on 16 of 18 and not on 2, and this
file says so rather than choosing a statistic that hides it.** What stands in its place is stronger
and exact: all three of the artifact's retained `libhven.a` reproduce byte-identically, and the
end-to-end ratios reproduce to 5.5e-6.

---

## 7. What this directory does not claim

* **No wall-clock claim.** None was measured, none is quoted, and `cycles` is informational.
* **No disposition.** §11.5 and the owner decide what follows from "the increase is T8.4's declared
  `O(n)` per-call reporting addition".
* **Nothing about a cell not measured here.** Three F7 `bound_neutral` cells, three QP tiers. Leg 2's
  HS problems and the interior leg are outside this and carried no instruction verdict anyway
  (`reading.md` §4).
* **Nothing about the callback, sink or ledger cost when one is ATTACHED.** No cell here attaches one.
* **Apple/Accelerate and Windows: UNOBSERVED.**

---

## 8. Files

```
attribution/
  attribution.md      this reading
  arms.txt            the eleven arms, both binaries' sha256, the pins, the recipe
  table.csv           495 rows: arm, task, sha, mode, cell, round, instructions,
                      branches, cycles, branch_misses, l1_icache_load_misses
  attribute.py        the analysis; prints every table above from table.csv alone
  attribute.py.sha256
  raw/perf/aNN/       every perf stat output, 45 per arm
  raw/logs/           every leg log: A0-fold-proof-{pass,fail}, A01..A11 per arm,
                      A12-layout-probe; each carries its pgrep capture taken
                      SEPARATELY outside the lock and again INSIDE it, its
                      FOREGROUND_START/END pair and its WRAPPER_EXIT
  raw/*.sh            common.sh, arm_leg.sh, run_arm.sh, fold_proof.sh,
                      layout_probe.sh -- the scripts as run
```

Every script sets `pipefail`, worst-folds every status into `LEG_RC` and prints `WRAPPER_EXIT`; the
fold is proven both ways in `raw/logs/A0-fold-proof-{pass,fail}.log` — the pass run ends 0, the fail
run ends 5 through a rc-2 build stub, a rc-5 PIPED stub folded from `PIPESTATUS[0]`, a later rc 3 and
a later rc 0 that do not lower it. `WRAPPER_EXIT=0` on all eleven arms and on the probe.

**The co-run terms.** This leg asserts instructions and branches only, which are scheduling-invariant
at `MKL_NUM_THREADS=1` (CLAUDE.md §7), so it was permitted to co-run and the box lock was taken per
batch rather than held across the window; the `pgrep` captures in every log show what was on the box.
No wall-clock number from this leg is quotable and none is quoted. No foreign process was signalled
or killed.
