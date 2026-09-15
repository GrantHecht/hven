# W5 T8.9r-attrib3 — Step A: which commit inside T8.4 carries the +2.5…4 %

`reading.md` §11 charged the interior leg's whole **+2.49 %** to T8.4 and could
not say more: its arms sat at group-1 task heads, and T8.4 is seven commits.
This leg put an arm at **every T8.4 library commit** and measured the per-row
wall at each — five rounds, arm order rotated per round, `--engine interior` on
the same four dual-binding F7 cells × three treatments, solo. `arms.txt` records
the six arms, their build recipe, their hashes and the calibration;
`bisect_t84.out` is `bisect_t84.py`'s own output over `raw/wall/`, reproduced
below in the parts that carry the finding. Nothing here is hand-transcribed.

**THIS ASSERTS WALL CLOCK** (CLAUDE.md §7) under the T8.9r fix1 R2 discipline:
one solve at a time, `taskset -c 2`, `MKL_NUM_THREADS=1`, the box lock held for
the whole batch, the driving shell pinned off the measurement core and its SMT
sibling. **RE-AUDITED AT FIX2 UNDER R2'** (the pinned-core rule, foreign `nice`
counted) **AND AGAIN AT FIX3 UNDER ITS EVIDENCE ACCOUNTING**: **`wall-r1` fails
the FRACTION** — 0.1290 % on the pinned core, **0.6706 %** on its SMT sibling,
against the 0.5 % bar — and **`wall-r2` and `wall-r4` are UNPROVEN-EVIDENCE**, a
foreign `R` at a named snapshot with no re-snapshot after it, which leaves TWO of
the five fully proven. The table below is a median of five rounds of which three
are flagged; a median of five
does not turn on one member, and the carrier's step is 36.5× the byte-identical
control's, so the finding survives the loss of any single round — but it is not a
five-clean-round median and is not offered as one. The governing table is
`../../../logs/IDLE-PROOF.md`; `logs/IDLE-PROOF.md` here is the superseded
`user`-only proof.

The scoring rules — the R3 positional exclusion, the scored set, the statistic,
and **the rule for naming a carrier** — were written down and hashed at
**18:50:35 UTC**, before the first wall batch's `FOREGROUND_START` at 18:51:1x
(`predeclaration.txt`, `logs/B0-predeclaration.log`).

---

## 1. It is `9cebbbe`, and nothing else is within a factor of ten

Per-row wall step, median of five rounds. **Bold** = above 1.01. The first row
every process writes is excluded by the pre-declared positional rule and is the
same row at every arm (`f7_n1000_bound_physics/MakeParameter`); eleven rows are
scored.

| row | 8ae1618 | **9cebbbe** | 9ce9bb2 | fix1 | 3c8e43b | cumul |
|---|---|---|---|---|---|---|
| `f7_n1000_bound_physics/MakeConstraint` | 0.9980 | **1.0200** | 1.0043 | 0.9959 | 1.0012 | 1.0194 |
| `f7_n1000_bound_physics/RelaxBounds` | 1.0000 | **1.0238** | 1.0050 | 0.9924 | 1.0008 | 1.0219 |
| `f7_n5000_bound_physics/MakeParameter` | 0.9995 | **1.0410** | 1.0056 | 0.9945 | 1.0005 | 1.0411 |
| `f7_n5000_bound_physics/MakeConstraint` | 0.9992 | **1.0328** | 1.0002 | 1.0074 | 1.0015 | 1.0414 |
| `f7_n5000_bound_physics/RelaxBounds` | 0.9995 | **1.0330** | 1.0019 | 1.0038 | 1.0015 | 1.0399 |
| `f7_n10000_bound_neutral/MakeParameter` | 0.9980 | **1.0413** | 1.0060 | 0.9950 | 1.0000 | 1.0403 |
| `f7_n10000_bound_neutral/MakeConstraint` | 1.0013 | **1.0305** | 1.0009 | 0.9982 | 1.0003 | 1.0312 |
| `f7_n10000_bound_neutral/RelaxBounds` | 1.0007 | **1.0281** | 1.0028 | 0.9878 | 1.0040 | 1.0232 |
| `f7_n20000_bound_neutral/MakeParameter` | 0.9991 | **1.0287** | 1.0028 | 0.9974 | 1.0011 | 1.0290 |
| `f7_n20000_bound_neutral/MakeConstraint` | 0.9998 | **1.0332** | 1.0001 | 0.9872 | 1.0009 | 1.0208 |
| `f7_n20000_bound_neutral/RelaxBounds` | 0.9994 | **1.0324** | 1.0019 | 0.9906 | 1.0007 | 1.0246 |
| **median step** | 0.9995 | **1.0324** | 1.0028 | 0.9950 | 1.0009 | — |
| **rows above 1.01, of 11** | 0 | **11** | 0 | 0 | 0 | — |
| **median share of the cumulative** | −0.01 | **+1.02** | +0.10 | −0.14 | +0.04 | — |
| **rows at or above 0.80 share** | 0 | **10** | 0 | 0 | 0 | — |
| `sizeof(InteriorPointSolver)` | 2056 | **2440** | 2440 | 2472 | 2472 | — |
| `interior_point_solver.cpp.o` .text | 285 330 | **295 042** | 295 042 | 296 410 | 296 410 | — |
| executable identical to parent's? | **YES** | no | no | no | **YES** | — |

`9cebbbe`'s median share of the cumulative is **+1.02** and it is at or above
0.80 on **10 of 11** scored rows — the eleventh, `f7_n5000_bound_physics/MakeConstraint`,
prints 0.80 and falls just under the bar; the pre-declared rule names it the carrier.
The other four commits' medians are −0.14 to +0.10 and none reaches 0.80 on any
row. The cumulative over the eleven scored rows, `510a4bb → 3c8e43b`, is
**1.0283** against §11's 1.0250 and §5's 1.0249 — a third day's rounds, a third
instrument, the same answer.

## 2. The floor, measured twice, on binaries that are the same bytes

`b01 → b02` and `b05 → b06` each produced a **byte-identical** `hven_sqp_corpus`
(`cmp` exit 0, `logs/B3-identity.log`), so their true step is exactly zero on
every row. They read:

| control pair | min | max | median | median \|ln step\| |
|---|---|---|---|---|
| `510a4bb → 8ae1618` | 0.9980 | 1.0013 | 0.9995 | 0.00075 |
| `5124aa1 → 3c8e43b` | 1.0000 | 1.0040 | 1.0009 | 0.00087 |

**`9cebbbe`'s median \|ln step\| is 0.03189 — 36.5× the worse of the two
floors.** Each arm's own round-to-round spread on its scored-row total is
0.6–1.4 %, and every arm's five round totals are printed in `bisect_t84.out`.

## 3. Two things this bisect settles about §11's stated mechanism

**(i) Adding an unreferenced object to the archive relocated nothing.** `8ae1618`
is where `src/drivers/solve_result.cpp` enters `src/CMakeLists.txt` and the
library's source count goes 42 → 43; `ar t` shows the new member inserted at
position 13 of 43. The linked executable is **byte-identical** to `510a4bb`'s,
because nothing references the new object at that commit and a linker does not
pull an unreferenced archive member in. §11's sentence "**a new object in the
archive** and a rewritten hot TU relocate everything that follows them" is, in
its first half, refuted here by measurement.

**(ii) Perturbing the same TU again, later, cost nothing.** T8.4 fix1 (`5124aa1`)
changed 302 lines of `src/drivers/interior_point_solver.cpp` and 86 of its
header, grew the solver object by another 32 bytes and that object file by
another 1 368, and read a median step of **0.9950** — *faster*, and inside the
floor's neighbourhood. `9ce9bb2` rewrote 280 lines of `sqp_driver.cpp` and moved
the status vocabulary down a tier, leaving the IPM's own object **byte-identical**
(`sha256` equal at `9cebbbe` and `9ce9bb2`), and read 1.0028. So this box is not
generically sensitive to being perturbed: two later, comparable perturbations of
the same files cost 0.3 % and −0.5 % where `9cebbbe` costs 3.2 %.

`mechanism.md` takes it from here: what `9cebbbe` does that those do not, and
what four experiments say about it.

APPLE / ACCELERATE: UNOBSERVED.  WINDOWS: UNOBSERVED.
