# M7 W2 M4 — the out-of-box start point, measured on the CUTEst TRAIN 23

**THE MEASUREMENT SAYS NO.** The toggle ships **OFF**, which is the default
commit `e65187ac` already carries, and this page is the evidence for that
decision rather than for the fix the A-SQP read expected. §8 has the rule and
the arithmetic; §7 has the finding that decides it.

## 1. What is measured, and why it needed a binary of its own

`SqpOptions::project_start_point` (M7 W2 M4) clamps the caller's `x0` onto the
model's declared variable box before the first evaluation, and counts the
components that were **strictly outside** it in
`SqpCounters::start_point_clamps`. The mechanism it addresses is the A-SQP
read's §M4 (`docs/notes/2026-09-m7-w2-sqp-cutest-read.md`): CUTEst declares
three problems in this set — **NET3, STEENBRD, STEENBRE** — whose own `x0` lies
outside their own declared bounds and whose objective is not defined there, so
hven's SQP lane returns `numerical_error` with `f = nan` in 0 majors.

The exit criterion is the M7 brief's: a measured win over the toggle's own OFF
state on the **TRAIN** split. The holdout is not run.

## 2. The two arms, and how they differ

The harness's cell binary (`lanes/hven/hven_bench_cell.cpp`) builds its options
from a small label table and has **no way to set a field its pin does not know
about**. The harness pin is `3904a477` (M7 W0 T5's close) and this task's field
is newer. The harness tree was therefore **not modified**; instead each arm is
an hven install whose *shipped default* for the one field differs, and the
harness's own unmodified cell source is configured **out of tree** against it:

| arm | `SqpOptions::project_start_point` default | install prefix | cell binary |
|---|---|---|---|
| **OFF** | `false` — today's behaviour, `x0` taken verbatim | `.scratch/m7w2m4/install-off` | `.scratch/m7w2m4/bench-off/lanes/hven/hven_bench_cell` |
| **ON** | `true` — `x0` clamped onto `[l, u]` | `.scratch/m7w2m4/install-on` | `.scratch/m7w2m4/bench-on/lanes/hven/hven_bench_cell` |

`wrappers/build_arm.sh` is the script that produces one arm. The two hven source
states differ by **one literal** and by nothing else; the sha256 of each install's
`include/hven/drivers/sqp_solver_types.h` is recorded in §4.

**THE PIN STAMP IN THESE BINARIES IS FALSE AND IS NEVER QUOTED.** The harness's
`cmake/hven_pin.cmake` sets `HVEN_PIN_SHA` unconditionally, so a caller can point
`CMAKE_PREFIX_PATH` at another prefix (the top-level `CMakeLists.txt` guards that
one with `if(NOT CMAKE_PREFIX_PATH)`) but cannot move the compile definition the
lane binary stamps into its rows. Every row these binaries emit therefore carries
`hven_pin_sha=3904a477…`, which describes an hven **they were not linked
against**. The true identity of the hven under test is in §4. Re-pinning the
harness is the settler's own declared event and is not done here.

`-DEIGEN_MAX_ALIGN_BYTES=32` is passed to every harness TU consuming the
installed package (it is also PUBLIC on the `hven` target, so the value is
carried twice and agrees).

## 3. The protocol these rows were taken under

- **Serial, one cell, one process**, `MKL_NUM_THREADS=1` and `OMP_NUM_THREADS=1`
  in every cell process. Not a co-run: this is CLAUDE.md §7's timing mode, and
  the arm holds the shared box lock (`/tmp/box-build.lock`) for its whole
  duration, at the same granularity another lane's sweep uses on this box.
- **No timeout and no fallback on the lock.** A wrapper that gave up on a lock
  timeout and ran anyway contaminated a sweep on this box on 2026-09-17; this
  one queues.
- **A foreign-cell guard inside the lock.** Before each cell, `pgrep -x
  hven_bench_cell` is resolved through `/proc/<pid>/exe`; any hit that is not
  this arm's own binary aborts the arm, because it would mean something is
  running outside the lock. The match is by process NAME, not command line —
  the runner's own text contains the string (the declared self-match).
- **The wall ceiling is 300 s per cell**, the value `sets/cutest-nlp.json`'s
  `run_terms.budget_wall_s` pins for this set, applied to the CELL and not to
  the lock wait. `--budget-iters 0`, so the engine's own `max_iter = 100` is the
  iteration cap, exactly as the H3 run had it.
- **Wall is informational and is never asserted** (CLAUDE.md §7). The asserted
  columns are the status, the outcome and every counter the lane reports.
  `dnf_wall` is the one wall-DEPENDENT outcome on the page; it is a DNF
  CONDITION and never a measurement, and a cell that changes only between
  `dnf_wall` and something else is reported as CENSORED rather than as a moved
  counter.

## 4. Provenance

`PROVENANCE.txt` beside this file, verbatim from `wrappers/provenance.sh`:
AMD Ryzen 7 5800X3D, Linux 7.2.4, clang 22.1.8, CMake 4.3.0, hven
**`003b5d89`** on `m7` (the M4 test commit; the one "modified path" it reports is
this artifact directory, untracked at the time, and no version string ending
`-dirty` is quoted anywhere on this page), harness `3968f3fb` READ ONLY.

The two arms' installed headers and the two cell binaries have **different
sha256s** and the installed default reads `false` on one and `true` on the
other — recorded in `PROVENANCE.txt`, so the arms are not a claim but a check.

## 5. The outcome table, OFF vs ON

`tables.txt` carries it in full, generated by `wrappers/make_tables.py`, which
also applies the decision rule itself. The shape of it:

| | OFF | ON |
|---|---|---|
| `optimal` | **2** (SMBANK, ZAMB2-8) | **1** (SMBANK) |
| `infeasible` | 1 (YORKNET) | 0 |
| `error` (`numerical_error`) | 12 | 15 |
| `dnf_wall` (censored at 300 s) | 8 | 7 |
| **cells whose asserted columns MOVED** | — | **7 of 23** |

**Both arms were run twice and both repeats agree EXACTLY** — 0 field
differences on the OFF pair and 0 on the ON pair (`compare-OFF-repeat.txt`,
`compare-ON-repeat.txt`), and the OFF-vs-ON difference is the SAME 133 fields in
both repeats (`compare-OFF-vs-ON-r1.txt`, `-r2.txt`). Nothing here is noise.

## 6. The three target cells

All three now START — the projection does exactly what it was written to do —
and **none of them converges**:

| cell | OFF | ON |
|---|---|---|
| **NET3** | `numerical_error`, `f = nan`, **0** majors, **0** factorizations, 0.005 s | `numerical_error`, `f = 218 961.90`, 1 major, **37 093** factorizations, 76.8 s |
| **STEENBRD** | `numerical_error`, `f = nan`, **0** / **0**, 0.005 s | `numerical_error`, `f = 2.855e12`, 3 majors, **16 869** factorizations, 60.0 s |
| **STEENBRE** | `numerical_error`, `f = nan`, **0** / **0**, 0.005 s | `numerical_error`, `f = 1.819e14`, 4 majors, **31 017** factorizations, 146.2 s |

The `f` at ON on all three is the value the H3 addendum predicted for a clamped
start (NET3's 218 961.90 is that log's number to the digit), so the projection
is arithmetically right. **What it is not is a win.** The A-SQP read's
expectation was "STEENBRD and STEENBRE are the plausible wins, because SNOPT
solves both from the same declaration"; on this engine, past iteration 0, both
fail for a reason M4 does not address — the same reason STEENBRB, STEENBRC and
STEENBRG already fail from in-box starts. **The out-of-box start was not the
only thing wrong with those cells, and the read could not have known that,
because the engine never got past the first evaluation to find out.**

## 7. Every other cell — and the finding that decides this page

**FOUR cells outside the three targets moved, and one of them was a converged
solve.** Three moved in the counter currency; the fourth, SAWPATH, moved only
across the wall ceiling and is reported apart, per §3.

| cell | OFF | ON | what changed |
|---|---|---|---|
| **ZAMB2-8** | **`optimal`**, f = −0.152 935 927 | **`error`** (`numerical_error`), f = −0.082 078 613 | 8 803 → 7 825 factorizations, 11 → 2 majors. **A CONVERGED CELL IS LOST.** |
| **YORKNET** | `infeasible` (uncertified), f = 110 670.30 | **`error`**, f = 110 481.70 | 52 180 → 22 165 factorizations, 19 → 1 elastic activations, 18 → 2 restoration iterations |
| **ACOPP57** | `error`, f = 41 737.54 | `error`, f = 41 063.26 | 114 → **3 726** factorizations, 5 → 3 majors — a 33× cost increase for a worse KKT residual (0.64 → 4 819.87) |
| **SAWPATH** | `dnf_wall` (censored at 300 s) | `error`, f = 1 470.55 | it now FINISHES inside the ceiling, with an error. **CENSORED-class and NOT a counter fact**: `dnf_wall` is a wall-DEPENDENT outcome and this page's own rule (§3) reports a transition into or out of it apart from a moved counter. It is counted in the "other cells moved" clause, which it fails on its own, and NOT in the "new errors" clause, whose two members are YORKNET and ZAMB2-8 |

**THE CHECK THE BRIEF ASKS FOR — is any MOVED cell an IN-BOX cell? NO, and that
is provable rather than observed.** `SqpSolver::solve_impl` substitutes the
projected vector **only when the count is non-zero**; at zero clamps the callee
receives the caller's own `x0` object, so the two arms execute identical code on
identical inputs and cannot differ by a single byte. A cell can therefore move
**only if its declared `x0` has at least one strictly out-of-box component**.
The 16 unmoved cells are the empirical control on that determinism — they
include `STEENBRC` at 32 137 factorizations and `DALLASM` at 53 s, so the
binaries agree to the last digit wherever the clamp does not fire.

**So the finding is not a defect in the clamp. It is that the premise was
incomplete: AT LEAST SEVEN of the 23 TRAIN cells declare an out-of-box `x0`, not
three.**

**AT LEAST, and the bound is one-sided for a reason worth stating.** Seven is the
number of cells whose asserted columns MOVED, and a cell moves only if it was
clamped — but the converse does not hold. Seven other cells are `dnf_wall` at
BOTH arms; they emit no asserted columns at all, so a clamp on one of them would
be invisible here. And the harness at its pin prints no clamp column
(`run_sqp`'s `side.*` keys are hand-enumerated and predate the counter), so
`start_point_clamps` — which would answer this directly, for every cell, without
a solve — was not readable on these rows. **Seven is a floor on the true count,
not the count**, and the settler's re-pin is what turns it into one.
The read found three because it was looking for cells whose objective is NaN
there; a start point can be outside its box and perfectly evaluable, and on four
such cells the declared point — outside the box though it is — is a BETTER place
to start than its projection. On ZAMB2-8 it is the difference between a solve
and a failure.

## 8. The decision

The rule, from the task brief: **ON iff ≥ the same optimal count, no new errors,
and the other cells unmoved.** All three clauses fail, and not marginally:

| clause | measured | verdict |
|---|---|---|
| ≥ the same optimal count | OFF **2** → ON **1** (ZAMB2-8 lost) | **FAILS** |
| no new errors | **2** new in the counter currency: YORKNET, ZAMB2-8 — **plus SAWPATH in the CENSORED class** | **FAILS** |
| the other cells unmoved | **3** non-target cells moved in counters (ACOPP57, YORKNET, ZAMB2-8), plus SAWPATH in the censored class | **FAILS** |

### **DEFAULT = OFF (`project_start_point = false`).**

That is the value commit `e65187ac` already ships, so **no header line moves and
no re-derivation is owed**: the U0 27-cell three-arm replay was run at this
default at HEAD `003b5d89` and came back **27 cells / 75 columns / 0 differences
on all three arms** (`gates/gates.txt`).

**What is NOT claimed.** This is not evidence that projecting a start point is
wrong — the projection is arithmetically correct, it is what three other solvers
do, and `NlpProblemModel::start_point()` already says hven agrees a start point
belongs in the box. It is evidence that **on this engine, on this set, at this
default, the projection costs more than it buys**, and that the honest form of
the fix is a narrower one: something that projects only where the declared point
cannot be evaluated, or a per-cell decision, neither of which this task was
scoped to design. The toggle stays, off, so the next round can ask that question
against a lever that already exists and a measurement that already has both arms.

## 9. Files here

| file | what it is |
|---|---|
| `cells-<arm>-r<N>.csv` | one row per TRAIN cell, the lane binary's own key=value output |
| `raw/<arm>-r<N>/<PROBLEM>.txt` | that cell's verbatim stdout, its invocation and its wall |
| `tables.txt` | the generated outcome table, the target rows, the moved/censored split and the verdict |
| `compare-*.txt` | the four column-for-column comparisons (both repeats, both arms) |
| `PROVENANCE.txt` | toolchain, host, commit, the two arms' header and binary sha256s |
| `gates/` | the Release and Debug suite lines, the long test alone, and the replay/smoke/export transcript |
| `replay/head-{walk,ssn,ipm}.csv` | the U0 27-cell replay this HEAD produced |
| `wrappers/run_locked.sh` | the box-lock wrapper (markers inside the lock, status propagated, no timeout, no fallback) |
| `wrappers/run_cells.py` | the serial runner, its foreign-cell guard and the per-cell ceiling |
| `wrappers/build_arm.sh` | one arm: the hven install and the out-of-tree harness cell binary |
| `wrappers/compare_cells.py` | the comparator |
| `wrappers/make_tables.py` | the table generator, which applies the decision rule |
| `wrappers/provenance.sh` | the stamp |
