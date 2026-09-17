# The SQP engine on the CUTEst TRAIN set — every failing cell by mechanism

**M7 W2 A-SQP. A READ, not a change: no source, test, script or CMake file in
this repository moves with this page.** Branch `m7`, HEAD at the start and at
every leg `24c8f7b6` (the W0 close), both trees clean — the stamp is
`docs/notes/data/2026-09-m7-w2-sqp-read/PROVENANCE.txt`.

The ledger registered this as **W2's first problem** at the H3 close
(`docs/notes/2026-09-m7-ledger.md`, the *W1 H3 CLOSED* entry: "the SQP engine's
behaviour on the CUTEst TRAIN set … is W2's first problem, the bridge cleared")
and the W0 CLOSED entry carries it as register item (2). It precedes A0 because
A0's signal list has to include whatever would have caught these failures, and
the last section of this page is that list.

**What the round already settled and this page does not re-open.** The bridge is
not the explanation — the derivative checker passes per cell on YORKNET and
STEENBRG at three objective scales, and on the three zero-iteration cells
(`hven-bench/runs/2026-09-m7-h3-cutest/logs/cutest-deriv-inbox.log`,
`…/cutest-deriv-nan-cells.log`). The models are not the explanation — the
interior-point lane solves 15 of the same 23 and SNOPT, an SQP, solves 15
(`runs/2026-09-m7-h4-external/four-lane-table.md`). Nothing here is tuned and
nothing here is a proposal to tune: every fix below is a change to a rule, with
the proof it owes.

## 0. How this page was measured

Two kinds of evidence, and every number is one or the other.

**RETAINED ROWS.** The H3 CUTEst run's `run-A` (`hven-bench/runs/2026-09-m7-h3-cutest/run-A/results.csv`
and `counters-hven-sqp.csv`, harness `348932f`, hven pin `3904a477`, 46 cells ×2
runs, `46 / 46 / 0`), the four-lane table
(`runs/2026-09-m7-h4-external/four-lane-table.md`), and the W2 A4-0 sweep's
`qp_mode` arms, read where they stand today under that task's own scratch
(`hven-bench/.scratch/w2a4/runs/hven-sqp/{default,sqp-ssn,sqp-ipm}/cutest-run-A/`).
That sweep is another implementer's live work; nothing here writes to it and
nothing here is a claim about its conclusions, which are its own to draw.

**RE-RUNS AT A MAJOR-ITERATION CAP.** The instrument is the harness's own
already-built cell binary, run from a private root holding a COPY of the decoded
CUTEst instances, at `--budget-iters k` for k = 1, 2, 3, … . `SolveBudget::max_iterations`
can only TIGHTEN the engine's own option, so a capped run is a PREFIX of the
uncapped one and the row it prints is the solve's state after k units of budget.
That turns a single final row into a trajectory without a line of new code.
The raw logs are `docs/notes/data/2026-09-m7-w2-sqp-read/raw/<PROBLEM>-default-b<k>.log`.

**The instrument is pinned against the artifact it reads.** Four cells were
re-run at `--budget-iters 0` — the uncapped configuration H3 ran — and compared
column by column against H3's retained `run-A` rows: **ACOPP57, READING6,
DALLASM and STEENBRB, 37 columns each, 148 of 148 identical**, statuses,
counters and the four declared residuals alike
(`raw/{ACOPP57,READING6,DALLASM,STEENBRB}-default-b0.log`). The binary in the
harness's build tree today is NEWER than the one that produced H3's rows; that
comparison is what makes it usable against them, and it is also the closing
check the co-run below owes.

**CONCURRENCY — declared per leg, per `docs/measurement.md` §3.1, and the part of
it that was wrong is declared first.** Every leg on this page ran while the W2
A4-0 sweep was working the same box. The wrapper took `flock /tmp/box-build.lock`
where it could and, after a 20 s wait, **proceeded without it** — and that
fallback was not this task's to make: the sweep holds the lock for whole legs of
its own, its cells run under a 300 s WALL ceiling, and a wall-ceilinged row is
exactly the kind §7 says "must run solo, or co-run only with their full budget
honored". **Of the 43 legs here — 38 a completed cell, 3 a cell that hit this
task's own cap, 2 wrapper fold-proofs — 4 held the lock and 39 did not**
(`LOCK=held` / `LOCK=corun`, recorded in each leg's own raw log; the four are
`ACOPP57-default-b1`, `READING6-default-b0`, `STEENBRC-default-b12` and
`YORKNET-default-b14`). The window, first leg start to last leg end, is
**2026-09-17T10:01:08−04:00 to 2026-09-17T10:49:38−04:00**, and it is recorded
here because the settler is having the sweep lane re-run whatever overlapped it.
No process of this task's was ever signalled by anything but its own `timeout`,
and no foreign process was signalled at all.

**What that costs THIS page, stated rather than waved at.** The terms on this
task's own processes were `taskset -c 7`, `MKL_NUM_THREADS=1`,
`OMP_NUM_THREADS=1`; the foreign lane's placement was not controlled, so this is
not the "one process per pinned PHYSICAL core" discipline §3 defines and nothing
here is offered as one. What is read is counters, statuses and the four declared
residuals — scheduling-invariant at one MKL thread — and the 148-of-148 identity
above is the evidence that they are, taken across both `LOCK=held` and
`LOCK=corun` legs. **NO WALL-CLOCK NUMBER FROM ANY LEG ON THIS PAGE IS QUOTED,
ANYWHERE.** The one place a duration appears at all is the statement that three
legs did not RETURN inside a 300 s cap of their own; under contention that is a
candidate and never a measurement, and it is marked as one at each of the three
cells that use it.

**EVERY `file:line` ON THIS PAGE IS AT HEAD `24c8f7b6`; THE BINARY IS AT PIN
`3904a477` (fix1).** 48 commits and a 383-line delta in `src/drivers/sqp_solver.cpp`
separate them (W0 T1–T7), so a reader diffing the pin finds `map_status` at
`:6021`, not `:6268`. **Every mechanism this page cites is present and unchanged
between the two**, checked rather than assumed, and the check is in the review
(`.superpowers/m7-w2-a-sqp-read-review-fable.md` item 1) and re-run here:
`map_status` and `qp_failure_is_retryable` are BYTE-IDENTICAL pin to HEAD
(`git show <pin>:…` against `sed -n` at HEAD, both bodies diff-empty); the
restoration RESUME test is at pin `:4397` and the retry gate at pin `:5507`, the
same statements; `src/qp/qp_engine.cpp`, `include/hven/qp/qp_types.h` and
`src/model/nlp_problem_model.cpp` are unchanged in their entirety, so
`latch_still_holds` is at `:1816` and `schur_cap = 128` at `:139` at BOTH
commits; `include/hven/detail/qp/qp_engine.h`'s whole pin→HEAD delta is four
lines, two W0 T7 curvature-source COMMENT blocks, leaving `kQpMaxIterFloor = 500`
and `kQpMaxIterCoeff = 5` at `:385-386` at both; the box note is at pin `:2390`;
and `IpmSolver::push_initial_point_interior`'s body (`ipm_solver.cpp:491-543`)
and `NlpProblemModel::start_point()`'s (`:462-468`) are byte-identical at both,
although their files are not.

**What could not be measured, and why.** The engine's per-major evidence exists
— `SqpResult::history` carries `qp_status`, `qp_minor_iters`, `qp_factorizations`,
`verdict`, `tr_radius`, `violation_l1` and `elastic_applied` per row
(`include/hven/drivers/sqp_solver_types.h:778-900`) and `JsonLinesTraceSink`
writes the same rows as `sqp.major` lines (`docs/trace-schema-v0.md` §4.3) — but
**the harness's hven lane exposes neither**. `lanes/hven/hven_bench_cell.cpp`
records a hand-enumerated subset of `SqpCounters` and never calls
`attach_trace`; the runner's `--solver-log` reaches the EXTERNAL lanes only
(`runners/run_serial.py:394-396`), and the hven lanes run at
`common.print_level = 10`, the silent level. Attaching a sink would be a change
to another implementer's tree and was not made. Everything below is therefore
read from counters and from capped prefixes, and where that leaves two
mechanisms open the page says which one the evidence supports and what would
separate them.

---

## 1. All 23 TRAIN cells under the SQP default

`hven-sqp`, `options_label = default` (the engine's own options plus
`common.threads = 1` and `common.print_level = 10`), `qp_mode = kWalk`,
`budget_iters = 100` in force, 300 s wall ceiling. Counters are H3's retained
`run-A`; the **mechanism** column is this page's reading and every one of them is
cited in §2. `f*` is the best-supported objective any lane reached
(`four-lane-table.md`).

**One correction to the pin table before the rows.** The four-lane README's
budget table records the `hven-sqp` iteration ceiling as "**500** (`SqpOptions`)"
(`runs/2026-09-m7-h4-external/README.md`, *The budgets were not the same*). It is
**100**: `SqpOptions::max_iter` is 100 (`include/hven/drivers/sqp_solver_types.h:381`)
against `IpmOptions::max_iters`' 500 (`include/hven/drivers/ipm_solver_types.h:106`),
the lane's own code says so and computes the minimum
(`hven-bench/lanes/hven/hven_bench_cell.cpp`'s `CellReport::budget_iters` note),
and every `hven-sqp` row's `budget_iters` column reads `100`
(`runs/2026-09-m7-h3-cutest/run-A/results.csv`). The rows are right and the
README's summary of them is not. It matters here twice: the SQP lane ran under a
ceiling **five times tighter than the IPM lane's** and **thirty times tighter
than IPOPT's**, and that ceiling is shared with the restoration phase — "the
bounded quantity is `major_iters + restoration_iters`"
(`include/hven/drivers/sqp_solver_types.h:250-253`).

| cell | `n` / `m` | outcome | majors / minors / facts | mechanism | ipm · ipopt · snopt | what the contrast says |
|---|---|---|---|---|---|---|
| `ACOPP118` | 344 / 608 | **error** `numerical_error` | 5 / 10 497 / 11 233 | **M1.** Majors 1–3 accepted, `f` 128 439 → 128 700 → **129 648.674** and stationarity 4.4e3 → 675 → **25.1**; majors 4 and 5 each spend **exactly 4 710** minors — the subproblem's own cap, twice — and the second ends the solve | `optimal` 129 660.686 · `optimal` 129 660.686 · `optimal` 129 660.686 | every other lane converges, and so was this one: the engine gave up 0.01 % from `f*` because a subproblem ran out of minors |
| `ACOPP57` | 128 / 274 | **error** `numerical_error` | 5 / 3 658 / 114 | **M1**, the same shape: `f` 41 063 → 41 661 → **41 737.5431**, stationarity 4.8e3 → 139 → **0.639**, then **1 795** minors twice | `optimal` 41 737.787 · `optimal` 41 737.787 · `optimal` 41 737.787 | the cleanest case on the set: three quadratic-looking majors, then the cap. `n = schur_cap` here, so M3 never engages and 3 658 minors cost only 114 factorizations |
| `ACOPR118` | 344 / 726 | **dnf_wall** | — | **M1 + M3's row-pattern variant.** One major alone costs **10 548 factorizations** and **3 115 symbolic analyses** — the K0 pattern rebuilt three thousand times in one subproblem — for **5 300** minors, which is `5 × 1 060`: the first subproblem is a CAPPED one (M1's shape, `rejected_steps = 1`, `steps_accepted = 0`, no elastic entry) carrying M3's price. 6 of its 5 300 minors were bordered. It ends with the step rejected at `f` 131 322, ‖c_e‖ 1.297 (`raw/ACOPR118-default-b1.log`) | `optimal` (acceptable) 129 660.687 · `optimal` 129 660.687 · `optimal` 129 660.687 | the IPM lane reaches the same answer in 696 factorizations TOTAL. This is the widest linear-algebra gap on the set |
| `ACOPR57` | 128 / 331 | **error** `numerical_error` | 13 / 9 714 / 4 159 | **M1**, with M3's row-change variant behind it: 8 accepted steps reach `f` 41 737.785 (`f*` to 6 digits) at stationarity 1.02, and 428 symbolic analyses say the pattern was rebuilt all through | `dnf_budget` 41 737.787 · `optimal` 41 737.787 · `optimal` 41 737.787 | the IPM lane does not finish either (500 iters); `sqp-ssn` DOES solve this cell, in 25 majors and 1 907 factorizations |
| `BIGBANK` | 2 230 / 1 112 | **dnf_wall** | — | **M3, accumulated.** Major 1 completes and its step is ACCEPTED — 1 039 factorizations for 1 038 minors, `f` −6 559.6, ‖c_e‖ 6.4e-13 (`raw/BIGBANK-default-b1.log`). At ~1 000 factorizations of a 2 230-variable KKT per major and up to 100 majors, the ceiling is the aggregate, not one subproblem | `optimal` −4 205 693 · `dnf_budget` · `dnf_budget` | the IPM lane solves it in **48** factorizations. IPOPT and SNOPT both fail it, so this is a hard problem — but not for hven's other engine |
| `BRITGAS` | 450 / 360 | **error** `numerical_error` | 2 / 9 000 / 9 265 | **M1 at the FIRST iterate.** Major 1's subproblem spends **4 500** minors = `5 × 900` and fails; major 2 spends **4 500** again; the solve ends having never moved from `x0` (`f` 0, ‖c_e‖ 2). 40 of 9 000 minors bordered — M3 at its most extreme | `optimal` −9.74e-08 · `dnf_budget` · `optimal` 0 | SNOPT solves it in 12 majors and 793 minors. hven's SQP engine never took a step |
| `DALLASM` | 196 / 151 | **error** `numerical_error` | 3 / 6 663 / 4 917 | **M1 + M2.** `steps_accepted = 0`: the returned `f` = 6.197e+07 is the start point's to five significant figures, not a blow-up (`raw/DALLASM-default-b{1,2,0}.log` — `f` moves only 61 971 711 → 61 974 566 → 61 974 881 across three majors, a relative move of 5e-5, all of it restoration's). One elastic activation and 2 restoration iterations | `optimal` −48 198.19 · `optimal` −48 198.19 · `optimal` −48 198.19 | three lanes agree on −48 198; the SQP lane never left the neighbourhood of `x0` |
| `MSS2` | 756 / 703 | **dnf_wall** | — | **M3.** Major 1 alone did not return inside a 300 s cap of this page's own, under a co-run (`raw/MSS2-default-b1.log`, `LEG_RC=124`) — a candidate, not a measurement, but it bounds where the ceiling goes: the FIRST subproblem | `dnf_budget` −6.93 · `error` · `error` | no lane answers this problem. It is the one wall cell whose difficulty is corroborated on every lane |
| `MSS3` | 2 070 / 1 981 | **dnf_wall** | — | **M3**, same reading as `MSS2` (`raw/MSS3-default-b1.log`, `LEG_RC=124`) | `dnf_wall` · `dnf_wall` · `dnf_budget` | three of four lanes DNF. Not an SQP-specific finding |
| `NET3` | 464 / 521 | **error** `numerical_error` | 0 / 0 / 0 | **M4.** CUTEst's declared `x0` is outside its own box on 41 of 464 components and `f` is NaN there; the driver's NON-FINITE ITERATE exit fires before the first subproblem | `dnf_budget` 5 859 392 · `error` · `error` | no lane solves NET3. The SQP lane is the only one that never starts |
| `READING6` | 102 / 50 | **error** `numerical_error` | 3 / 1 783 / 3 313 | **M1.** One step accepted at major 1, to a point with `f = 0`, stationarity **0** and ‖c_e‖ 2.036 — a stationary point of the objective that is 2 units infeasible. Majors 2 and 3 each spend **exactly 770** minors and fail | `optimal` −144.660 · `optimal` −144.660 · `optimal` −144.660 | three lanes agree on −144.66. The SQP lane walks into an objective-stationary infeasible point and cannot leave it |
| `READING8` | 2 002 / 1 000 | **dnf_wall** | — | **M3.** Major 1 alone did not return in 300 s under a co-run (`raw/READING8-default-b1.log`, `LEG_RC=124`) | `dnf_budget` −1 924.16 · `optimal` −2 179.74 · `optimal` −2 290.45 | both external lanes answer; SNOPT in 19 majors |
| `SAWPATH` | 583 / 774 | **dnf_wall** | — | **M3.** ONE unit of budget — one major, with one elastic ladder entry — costs **14 031 factorizations** for 7 337 minors and ends at `f` 1 483.19, ‖c_e‖ 0, with no step accepted (`raw/SAWPATH-default-b1.log`). The ceiling is spent in the earliest subproblems | `optimal` (acceptable) 750.459 · `optimal` 723.168 · `optimal` 750.459 | the IPM lane answers in **105** factorizations total; SNOPT in 8 majors and 21 minors |
| `SMBANK` | 117 / 64 | **optimal** | 29 / 334 / 29 | 29 majors, 29 accepted steps, **no rejection, no elastic, no restoration**. Every minor bordered (`border_refine_steps == qp_minor_iters`), one factorization per major | `optimal` (acceptable) −7 129 292 · `optimal` −7 129 292 · `optimal` −7 129 292 | what the engine looks like when nothing goes wrong: `n < schur_cap`, no subproblem near its cap |
| `STEENBRB` | 468 / 108 | **error** `numerical_error` | 3 / 11 277 / 16 582 | **M2.** At the last major before the phase, `f` = 4 547.15 at ‖c_e‖ = **1 999**; 11 restoration iterations later the returned point is `f` = **4.09e+12** at ‖c_e‖ = 2.3e-13 (`raw/STEENBRB-default-b{1,2,0}.log`). `steps_accepted = 0` throughout | `optimal` 9 075.855 · `optimal` 9 075.855 · `optimal` 9 075.855 | three lanes agree to ten digits on 9 075.855. Restoration found a feasible point nine orders up the objective and the engine adopted it unjudged |
| `STEENBRC` | 540 / 126 | **error** `numerical_error` | 4 / 24 524 / 32 137 | **M2**, the same shape and worse: `f` 4 610.23 at ‖c_e‖ **9 999.5** → **1.83e+14** at 9.1e-13, 16 restoration iterations. Major 1's subproblem spends **5 400** minors = `5 × (540 + 0 + 540)`, the derived cap on a problem whose every variable is bounded and whose every row is an equality | `optimal` 28 589.34 · `optimal` 28 519.52 · `dnf_budget` 27 540.22 | the two answering lanes are within 0.25 % of each other; the SQP lane is out by ten orders |
| `STEENBRD` | 468 / 108 | **error** `numerical_error` | 0 / 0 / 0 | **M4**, 2 of 468 components out of box | `dnf_budget` 9 555.66 · `error` · **`optimal` 9 030.08** | SNOPT solves it from the same declaration, because it projects. This is M4's strongest case |
| `STEENBRE` | 540 / 126 | **error** `numerical_error` | 0 / 0 / 0 | **M4**, 2 of 540 components out of box | `optimal` (acceptable) 28 744.36 · `error` · **`optimal` 27 458.82** | as `STEENBRD`; SNOPT's is the better of the two answers |
| `STEENBRG` | 540 / 126 | **error** `numerical_error` | 4 / 24 629 / 32 291 | **M2**, byte-identical in outcome to `STEENBRC`: `f` 4 609.92 at ‖c_e‖ 9 999.5 → **1.83e+14** at 9.1e-13, 16 restoration iterations, and the same 5 400-minor first subproblem | `optimal` (acceptable) 27 722.96 · `optimal` 74 465.58 · `dnf_budget` 27 484.75 | the lanes disagree about `STEENBRG`'s answer (an open W2 question of H4's), but all three are at 2.7e4–7.4e4, not 1.8e14 |
| `TAX213322` | 1 512 / 570 781 | **dnf_wall** | — | **M3, accumulated.** Major 1 completes, its step is accepted, 2 142 factorizations for 758 minors, `f` −6 747.92 at ‖c_e‖ 0 (`raw/TAX213322-default-b1.log`). The ceiling is many such majors, not one | `dnf_wall` · `dnf_wall` · `dnf_wall` | every lane DNFs. The largest instance the set admits; not an SQP finding |
| `TAX53322` | 360 / 32 221 | **dnf_wall** | — | **M3, accumulated.** Major 1 completes cheaply — 414 factorizations, 182 minors, step accepted, `f` −1 607.55 at ‖c_e‖ 0 (`raw/TAX53322-default-b1.log`), already lower than the `f` the IPM lane reaches in 500 iterations (−1 601.51), though SNOPT's −2 142.00 is lower still | `dnf_budget` −1 601.51 · `dnf_budget` · `error` | **the one wall cell where the SQP engine looks competitive.** One major reaches a better objective than the IPM lane's 500; it simply never got to stop |
| `YORKNET` | 312 / 256 | **infeasible** (UNCERTIFIED) | 1 / 50 241 / 52 180 | **M5 = M2 + M3.** One major, 18 restoration iterations, **19 elastic ladder entries**, 52 180 factorizations, and `sqp_infeasibility_certified = 0` — "no claim is made about the model". Returned at ‖c_e‖ 4 099, ‖c_i‖ 131 | `optimal` 14 239.97 · `optimal` 14 239.97 · `Optimal` 14 617.54 (not a KKT point) | two lanes agree to eight digits at 14 239.97, so the problem is feasible. The worst-labelled row on the set |
| `ZAMB2-8` | 138 / 48 | **optimal** | 11 / 6 666 / 8 803 | Converges — but pays **8 803 factorizations** and **11 restoration iterations** for 11 majors. M3's cost without M1's or M2's failure | `optimal` −0.152936 · `optimal` −0.152936 · `optimal` −0.152936 | all four lanes agree. A success that still costs 220× the IPM lane's 40 factorizations |

**The three summary counts this table supports.** Engine-attributable failures:
**9** `numerical_error` cells that reached a subproblem (M1, with M2 behind three
of them) — the H3 addendum's own count, reached here per cell rather than by
subtraction. **3** cells the engine never started (M4). **8** wall cells, of
which **5** (`ACOPR118`, `MSS2`, `MSS3`, `READING8`, `SAWPATH`) spend the ceiling
inside the first subproblem and **3** (`BIGBANK`, `TAX53322`, `TAX213322`) spend
it across majors that were individually healthy. **1** uncertified `kInfeasible`
(M5). **2** optimal.

## 2. The mechanisms, ranked by the cells a fix would move

Five mechanisms account for all 21 non-optimal cells. They are ranked by how
many cells the fix would move, not by how deep the defect is; where two
mechanisms are consistent with the evidence, both are named and the measurement
that would separate them is named with them.

---

### M1 — A subproblem that ran out of its own minor budget is reported as a NUMERICAL ERROR, and two of them in a row end the solve

**Cells: 9** — ACOPP118, ACOPP57, ACOPR57, BRITGAS, DALLASM, READING6,
STEENBRB, STEENBRC, STEENBRG. Every `numerical_error` cell that reached a
subproblem at all exits through this one gate. A tenth cell shows the same shape
without reaching the exit: `ACOPR118`'s first subproblem is capped too (5 300
minors = `5 × 1 060`, `rejected_steps = 1`), but the 300 s ceiling fell before a
second failure could — §1's row and §M3.

**The mechanism, in two halves.**

*(a) The label.* `SqpSolver::map_status` (`src/drivers/sqp_solver.cpp:6268-6277`)
has one arm for three QP verdicts:

```
    case QpStatus::kOptimal:
    case QpStatus::kMaxIter:
    case QpStatus::kNumericalError:
        break;
    }
    return SolveStatus::kNumericalError;
```

`QpStatus::kMaxIter` means the subproblem hit the minor cap `QpOptions::max_iter`
resolves to — and the option's own header says so in as many words: "A capped
subproblem remains an honest failure — `QpStatus::kMaxIter` with `minor_iters ==
the effective cap`; raising the cap never converts a wrong answer into a right
one" (`include/hven/qp/qp_types.h:131-137`). The driver then reports it under the
one status hven reserves for "the solve produced no answer it stands behind" and
the harness buckets it as `error` (`lanes/hven/hven_bench_cell.cpp`'s
`normalise_outcome`). **A budget exhaustion and a factorization failure are
indistinguishable in every column a consumer can see.**

*(b) The failure budget is exactly one.* The retry gate is
`st.qp_failures_in_a_row == 0 && qp_failure_is_retryable(...)`
(`src/drivers/sqp_solver.cpp:5754`): the FIRST failure at an iterate shrinks the
radius, reseeds from the failed solve's own working set and continues; the
counter is cleared only by a `kOptimal` subproblem (`:5806`). So a second
consecutive failure — at a smaller radius, which is a DIFFERENT subproblem — ends
the whole solve through `kFinishQpFailure` (`:5804`).

**The evidence, per cell, from the capped ladders.** The fatal majors consume the
*identical* minor count twice, and that count is a multiple of the cap's own
coefficient (`kQpMaxIterCoeff = 5`, `include/hven/detail/qp/qp_engine.h:386`):

| cell | minors in the first fatal major | in the second | the pair |
|---|---:|---:|---|
| `ACOPP57` | 1 795 | 1 795 | `5 × 359` |
| `ACOPP118` | 4 710 | 4 710 | `5 × 942` |
| `BRITGAS` | 4 500 | 4 500 | `5 × 900` |
| `READING6` | 770 | 770 | `5 × 154` |

(`raw/ACOPP57-default-b{1,2,3,4,0}.log`, `raw/ACOPP118-default-b{1,2,3,4}.log`
against the retained `run-A` totals; `raw/BRITGAS-default-b1.log`;
`raw/READING6-default-b{1,2,0}.log`.) A subproblem that failed on its linear
algebra would not spend the same count twice. A subproblem stopped by a
size-derived cap spends exactly that count every time.

**And the iterate the engine threw away was nearly the answer.** The capped
ladders give the objective and the declared residuals at every major:

| cell | major 1 | major 2 | major 3 | at the exit | the answer the other lanes found |
|---|---|---|---|---|---|
| `ACOPP57` | `f = 41063.4`, stat 4.8e3 | `41660.9`, 139 | `41737.5431`, **0.639** | unchanged from major 3 | `41737.7868` (ipm, ipopt, snopt) |
| `ACOPP118` | `128439.2`, 4.4e3 | `128699.6`, 675 | `129648.674`, **25.1** | unchanged from major 3 | `129660.686` (ipm, ipopt, snopt) |

Three majors of clean quadratic-looking progress, then two capped subproblems,
then `numerical_error` — with the engine standing on a point whose declared
equality residual is 1.1e-4 (`ACOPP57`) and 6.5e-3 (`ACOPP118`).

**Corroboration from the A4-0 sweep, which is the sharpest part of this.** Change
nothing but the inner QP kernel and both cells converge: under `qp_mode = kSsn`,
`ACOPP118` is `optimal` at `129660.6859` in 5 majors and 2 030 factorizations,
and `ACOPR57` is `optimal` at `41737.7850` in 25 majors
(`hven-bench/.scratch/w2a4/runs/hven-sqp/sqp-ssn/cutest-run-A/results.csv`).
The outer iteration was never the problem on those cells; the subproblem's
ability to finish inside its budget was.

**The proposed fix, and its owner.** `src/drivers/sqp_solver.cpp` — the driver.
Three separable pieces, in increasing order of risk:

1. **Report the cap as itself.** `SolveStatus` has no arm for "the subproblem ran
   out of minors", and inventing one is an enum change with a migration cost.
   The cheap, additive form is a counter (M-A0-1 below) plus the history row's
   existing `qp_status`, which already carries `QpStatus::kMaxIter`
   (`include/hven/drivers/sqp_solver_types.h:849`) — the information exists and no consumer surface
   publishes it.
2. **Let the radius shrink do its job.** A capped subproblem at radius Δ and the
   same subproblem at Δ/2 are different problems; the gate stops after one.
   A failure budget of `k` consecutive routed failures (with the floor→restoration
   path unchanged) is one line and a constant.
3. **Give a capped subproblem more minors rather than refusing it.** The cap's
   own header calls the explicit-`max_iter` precedence "the escape hatch that
   makes the default safe" (`qp_types.h:133-137`). A driver-side re-solve at a
   raised cap, once per iterate, is the same escape hatch used from inside.

**What a fix must prove.** The U0 27-cell three-arm replay against the M6 close
HEAD's committed CSVs (`docs/notes/data/2026-09-m6-close-gate/replay/head-{walk,ssn,ipm}.csv`
through `scripts/compare_replay.py`) — and here, unlike a TU move, it is a GATE
rather than a formality: (2) and (3) change trajectories, so the expected result
is a DECLARED re-derivation of those three CSVs, not 27/75/0. The SQP corpus
suite (`tests/sqp/test_corpus_cells.cpp`) and the golden trace bytes
(`tests/sqp/test_trace_writer.cpp`) move with them. The TRAIN measurement is the
CUTEst 23 at `qp_mode = kWalk` re-run A/B, and the bar is ACOPP57 and ACOPP118
reaching `optimal` without SMBANK or ZAMB2-8 leaving it.

**The signal A0 needed.** A counter that separates the two failures, and one that
says the cap was reached: `QpStatus::kMaxIter` exits and `kNumericalError` exits
counted apart, plus the peak of `qp_failures_in_a_row`. None exists.
`SqpCounters::rejected_steps` conflates them with an ordinary strategy rejection
BY DESIGN — "a strategy-rejected trial, and a subproblem that failed but returned
a usable iterate. Tell them apart by the row's `qp_status`"
(`include/hven/core/solver_counters.h:993-995`) — and the row is not in any
artifact this round produced.

---

### M2 — The restoration phase adopts its point on FEASIBILITY ALONE, with no test on the objective

**Cells: 3** — STEENBRB, STEENBRC, STEENBRG. These are the "objective 4e12 →
1.6e15" cells, and the framing they were registered under needs correcting: the
globalization did not accept a blow-up. It never judged the point at all.

**The mechanism.** When restoration's sub-solve returns a point whose constraint
violation is inside `feas_tol`, the driver takes it, unconditionally:

```
    if (h_r <= opts_.feas_tol) {
        // RESUME. The order here is the header note's order.
        st.x = x_r;
        st.ev = std::move(ev_r);
```

(`src/drivers/sqp_solver.cpp:4577-4580`.) There is no merit test, no funnel test
and no comparison of `ev_r.f` against `st.ev.f` on this path — by construction,
since the feasibility sub-problem has no objective block of the main problem's.
The default strategy is `FunnelStrategy` (`:2519`), and it is `resume_from_restoration(h_r)`'d
at `:4602` — re-based on the new point rather than consulted about it.

**The evidence: the restoration phase's whole trajectory, measured.** Capping the
budget mid-phase reports the state after `k` units of it — the budget is shared,
"the bounded quantity is `major_iters + restoration_iters`"
(`include/hven/drivers/sqp_solver_types.h:250-253`) — so a ladder over `k` walks
the phase. On `STEENBRC`:

| `--budget-iters` | majors | restoration iters | `f` | ‖c_e‖ | log |
|---:|---:|---:|---:|---:|---|
| 2 | 2 | 0 | **4 528.31** | **10 000** | `raw/STEENBRC-default-b2.log` |
| 3 | 2 | 1 | 4 610.23 | 9 999.5 | `raw/STEENBRC-default-b3.log` |
| 6 | 2 | 4 | 2.0102e+06 | 9 992.5 | `raw/STEENBRC-default-b6.log` |
| 12 | 2 | 10 | 2.1982e+11 | 9 488.5 | `raw/STEENBRC-default-b12.log` |
| 17 | 2 | 15 | 7.0385e+13 | 1 808.5 | `raw/STEENBRC-default-b17.log` |
| — (uncapped) | **4** | 16 | **1.8345e+14** | **9.09e-13** | `run-A/results.csv` |

**The objective climbs eleven orders of magnitude, monotonically, while the
violation falls from 10 000 to 1e-13.** That is not an anomaly at the end of the
phase; it is the phase.

The same shape on its two siblings, from the majors either side:

| cell | at the last major before the phase | at the exit | restoration iters |
|---|---|---|---:|
| `STEENBRB` | `f = 4 547.15`, ‖c_e‖ = **1 999** | `f = 4.0909e+12`, ‖c_e‖ = **2.27e-13** | 11 |
| `STEENBRG` | `f = 4 609.92`, ‖c_e‖ = **9 999.5** | `f = 1.8345e+14`, ‖c_e‖ = **9.09e-13** | 16 |

(`raw/STEENBRB-default-b{1,2,0}.log`, `raw/STEENBRG-default-b{1,2,3}.log`,
against `run-A`'s rows and `counters-hven-sqp.csv`.) `steps_accepted` is **0** on
all three: not one step on the caller's own problem was ever taken.

**And the ladder settles which restoration arm fired, which the final row alone
could not.** At `--budget-iters 17` the cell stands at **2** majors and 15
restoration iterations; the uncapped row stands at **4** majors and 16. So the
phase ended on its RESUME arm — `h_r` fell inside `feas_tol` at iteration 16, the
driver took the point (`:4577-4580`), and the main loop then ran **two more
majors at `f = 1.8e14`**, which failed consecutively and ended the solve through
M1's gate (`rejected_steps` 1 → 2 across the same pair of rows). The causal chain
is complete and it is M2 followed by M1: restoration resumes at a point eleven
orders up the objective, and the two subproblems built there cannot be solved.

**The returned objective is therefore restoration's, carried forward.** On the
RESUME arm the restored point BECOMES the iterate — `st.x = x_r; st.ev = ev_r` —
so the `f` the exit reports two majors later is the one restoration handed over.
(On the OTHER arm the same is true by a different route, and the result contract
spells it out there: "ON A Terminal restoration exit, `SqpResult::x` is NOT the
point the last history row describes … `x`/`f`/`lambda_*` are the RESTORED
point", `include/hven/core/solver_counters.h:981-984`.)

**What the ladder rules out.** The other reachable route to this exit — the
"NOT FEASIBLE ENOUGH" arm at `:4615` with the sub-solve's own
`SolveStatus::kNumericalError` (`:4700-4702`) — requires `h_r > feas_tol` at the
end of the phase. The uncapped row's ‖c_e‖ of 9.09e-13 is six orders inside
`feas_tol = 1e-6`, and the two extra majors the ladder exposes are exactly what
the RESUME arm produces and what the terminal arm cannot. That reading is
measured, not inferred from the status.

**The proposed fix, and its owner.** `src/drivers/sqp_solver.cpp`, the
restoration boundary. The question is a design one and belongs to the settler:
should a restoration RESUME be judged? Three shapes, with the argument for each:

- **Judge it.** Hand `(f_old, f_new, h_old, h_new)` to the strategy exactly as a
  trial step is judged, and refuse a restored point that buys feasibility at an
  unbounded objective cost. Against: a funnel that refuses the only feasible
  point it has leaves the solve with nowhere to go, and the restoration was
  entered precisely because the iterate was stuck.
- **Bound the excursion.** Accept the restored point but cap the objective
  increase relative to the requesting iterate, and re-enter restoration from a
  closer target otherwise. Against: the cap is a tuning constant, which this
  repository has been careful not to accumulate.
- **Return the best point, not the last.** hven already has this: `budget_mode`
  returns "the best iterate by the funnel's own ordering rather than the last one
  reached" (`include/hven/core/solver_status.h:70-77`), and it is OFF by default
  and reported only on a max-iter exhaustion. Extending the best-iterate return
  to the QP-failure exit would have `STEENBRB` report `f = 4547` at ‖c_e‖ = 1999
  instead of `4.09e12` at 2e-13 — **which is not obviously better**, and saying so
  is the point: neither point is an answer, and a benchmark that scored one over
  the other would be scoring the reporting rule.

**What a fix must prove.** `tests/sqp/test_sqp_restoration.cpp` is the suite that
pins this boundary, including the certificate's own re-derivation. The three
replay CSVs are at risk on any of the three shapes and a declared re-derivation
should be assumed. The TRAIN bar: STEENBRB to `optimal` at `9075.855` (where the
IPM, IPOPT and SNOPT all land), with ZAMB2-8 — which takes 11 restoration
iterations and still converges (`counters-hven-sqp.csv`) — unmoved.

**The signal A0 needed.** A per-restoration record of `(h_in, h_out, f_in,
f_out)`. `SqpCounters::restoration_iters` is a count and says nothing about the
excursion; the history has no row for the phase at all, by contract
("restoration produces no history rows",
`include/hven/drivers/sqp_solver_types.h:353`). An objective ratio
of 1e11 across a phase that produces no row is invisible to any heuristic that
reads the currency.

---

### M3 — Past 128 pinned variables the working-set walk pays a FULL FACTORIZATION per minor

**Cells: at least 15** — every cell whose factorization count is enormous
(YORKNET 52 180, STEENBRG 32 291, STEENBRC 32 137, STEENBRB 16 582,
ACOPP118 11 233, BRITGAS 9 265, ZAMB2-8 8 803, DALLASM 4 917, ACOPR57 4 159,
READING6 3 313) **and** all 8 `dnf_wall` cells, whose ceiling this is what
spends. It is the cost mechanism rather than the exit mechanism: it does not by
itself fail a cell, it makes every other failure expensive and it is what turns
a hard problem into a wall row.

**The mechanism.** The QP engine's border-mode linear algebra keeps one
factorized `K0` and carries working-set changes on a Schur border stack. Bordering
is unavailable whenever the pin count exceeds `QpOptions::schur_cap`, whose
default is **128** (`include/hven/qp/qp_types.h:139`):

```
bool QpEngine::latch_still_holds(const WorkingSet &ws) const {
    const Index pinned = ws.n() - ws.num_free();
    return pinned > opts_.schur_cap;
}
```

(`src/qp/qp_engine.cpp:1816-1819`.) The header states the consequence without
hedging: "Pins can never be folded into K0 … so immediately after ANY rebuild_k0
the live stack is exactly the pins — while `pinned > schur_cap`, every possible
K0 is born past `needs_refactorization()` and **bordering is unavailable at any
price**" (`include/hven/detail/qp/qp_engine.h:1400-1410`). And the SQP driver
passes a FINITE trust-region radius on every subproblem
(`include/hven/detail/qp/qp_engine.h:380-383`, which says so explicitly), so the
effective box is `[max(l, x−Δ), min(u, x+Δ)]` and a variable held by the radius
is pinned like any other.

**The evidence.** Read `border_refine_steps` against `qp_minor_iters`, and
`factorizations` against `qp_minor_iters`, over the cells with rows
(`hven-bench/runs/2026-09-m7-h3-cutest/run-A/counters-hven-sqp.csv`), sorted by
`n` (`hven-bench/sets/cutest-nlp.json`):

| cell | `n` | minors | border refine | factorizations | facts / minor |
|---|---:|---:|---:|---:|---:|
| `READING6` | 102 | 1 783 | 131 | 3 313 | 1.86 |
| `SMBANK` | 117 | 334 | **334** | **29** | **0.09** |
| `ACOPP57` | 128 | 3 658 | **3 658** | **114** | **0.03** |
| `ACOPR57` | 128 | 9 714 | 8 369 | 4 159 | 0.43 |
| `ZAMB2-8` | 138 | 6 666 | 1 290 | 8 803 | 1.32 |
| `DALLASM` | 196 | 6 663 | 4 284 | 4 917 | 0.74 |
| `YORKNET` | 312 | 50 241 | 140 | 52 180 | 1.04 |
| `ACOPP118` | 344 | 10 497 | 602 | 11 233 | 1.07 |
| `BRITGAS` | 450 | 9 000 | 40 | 9 265 | 1.03 |
| `STEENBRB` | 468 | 11 277 | 972 | 16 582 | 1.47 |
| `STEENBRC` | 540 | 24 524 | 761 | 32 137 | 1.31 |
| `STEENBRG` | 540 | 24 629 | 845 | 32 291 | 1.31 |

**The two cells that pay almost nothing for their minors are the two where every
minor was bordered** — SMBANK (`n = 117`) and ACOPP57 (`n = 128`), both with
`border_refine_steps == qp_minor_iters` and a factorization count one to two
orders of magnitude below the minor count. **All eight cells with `n > 128` read
0.74 factorizations per minor or worse, and seven of the eight read at least
1.03.** ACOPP57 is the sharpest case in the set: `n = 128` IS `schur_cap`, so
`pinned > schur_cap` is UNREACHABLE on it whatever the radius does, and it pays
114 factorizations for 3 658 minors while `ACOPP118` — the same power-flow family
at `n = 344` — pays 11 233 for 10 497.

The capped ladders show the same thing within a single cell. On `ACOPP118`,
majors 2 and 3 cost **one factorization each** (1 964 → 1 965 → 1 966) for 107
and 20 minors, while major 1 costs 1 964 and major 4 costs 4 633
(`raw/ACOPP118-default-b{1,2,3,4}.log`). Bordering is not always unavailable on a
large cell — it is unavailable when the radius is small enough to pin more than
128 variables, which is precisely when the driver is in trouble.

**The two cells that do not fit the reading, named rather than dropped.**
`READING6` (`n = 102`) reads 1.86 factorizations per minor where its size says it
should read like SMBANK's 0.09, and `ACOPR57` (`n = 128`) reads 0.43 where
`ACOPP57` at the identical `n` reads 0.03. Each has a candidate explanation and
neither is settled here. `READING6` fires the elastic ladder
(`elastic_activations = 1`), whose subproblem carries two slack columns per
elastic row on top of `n` — so the SUBPROBLEM's variable count, which is what the
latch reads, exceeds the NLP's `n = 102`. `ACOPR57` pays **428** symbolic
analyses against every other cell's 1–6, which is the OTHER trigger (below) and
not the latch at all.

**The second refactorization trigger is measurable too, and the ACOPR family is
where it shows.** `symbolic_analyses` counts the times `rebuild_k0` found the
backend's SYMBOLIC analysis needed — i.e. the `K0` PATTERN changed, which happens
when the working ROW set changes rather than when a variable is pinned
(`include/hven/core/solver_counters.h:75-83`, `src/qp/qp_engine.cpp:1825-1830`).
Every cell on this set reads 1–6 **except** `ACOPR57` at **428** and `ACOPR118`,
whose FIRST MAJOR ALONE reads **3 115** against 10 548 factorizations and 5 300
minors (`raw/ACOPR118-default-b1.log`). Those two are the `ACOP*R*` variants —
the same power-flow models as `ACOPP57`/`ACOPP118` with more rows (331 against
274; 726 against 608) — and they are the two cells where a third of the
refactorizations also paid a fresh symbolic analysis. So the set carries both
triggers, and it separates them: the pin latch on the STEENBR and BRITGAS cells
(symbolic analyses 1–4), the row-pattern rebuild on the ACOPR cells.

**What would separate the latch reading from its alternatives.** `QpCounters::schur_updates`
is the count of border updates actually applied, and it is the direct reading of
"was bordering available?" — and `SqpCounters` **deliberately does not aggregate
it**: "qp_minor_iters and factorizations are plain SUMS of the corresponding
QpCounters fields over every subproblem solved … `schur_updates` is deliberately
NOT aggregated" (`include/hven/core/solver_counters.h:986-988`). With that one
sum in the currency, this section would be a measurement instead of a
correlation with a named mechanism. The alternative it would rule out is the
other refactorization trigger — a change in the working ROWS that `K0` spans
(`src/qp/qp_engine.cpp:1807-1814`'s `rebuild_would_be_noop`) — which would
produce the same aggregate without the latch ever engaging.

**The proposed fix, and its owner.** The QP layer (`src/qp/qp_engine.cpp`,
`include/hven/qp/qp_types.h`) and, jointly, the driver's radius policy. Three
candidates, none of them obviously right:

- **`schur_cap` is a fixed 128 on a problem of any size.** Making it
  size-derived, on `QpOptions::max_iter`'s own sentinel discipline
  (`qp_types.h:117-137`), is the smallest change that addresses the reading
  directly. Against: the cap exists because a border stack of dimension `k`
  costs `O(k²)` to carry and `O(k³)` to refresh; raising it trades
  factorizations for dense work, and the trade has not been measured here.
- **The trust region pins variables the problem does not.** `qp_cap_base`
  already distinguishes "REAL bounds, pre-trust-region" from effective ones
  (`include/hven/detail/qp/qp_engine.h:380-389`) for the minor cap; the latch
  does not make that distinction, and whether it should is a question for
  whoever owns the border stack.
- **Neither, and the answer is the tier.** W0 T3 measured the walk's elastic-rung
  factorizations growing linearly in `n` (184 → 2 281) against the IPQP tier's
  flat 73–75 and kept `kWalk` as the default on the strength of the tier's false
  declines (ledger, *W0 T3 CLOSED*). This set is where that trade-off is being
  paid, and the A4-0 sweep's `sqp-ssn` arm already shows the effect: `ACOPP118`
  at 2 030 factorizations instead of 11 233, `SMBANK` at 271 instead of 29 — a
  win on one and a loss on the other.

**What a fix must prove.** The QP engine's own suites
(`tests/sqp/test_qp_engine*.cpp`, `test_qp_engine_border.cpp`, `test_schur.cpp`,
`test_border_ops.cpp`) and the three replay CSVs, with a declared re-derivation expected rather than 27/75/0 — a
`schur_cap` change moves trajectories. It also moves the engine's own reuse key,
which covers all nine `QpOptions` fields by construction and has a field-coverage
test requiring the hash to move when any one of them does
(`include/hven/qp/qp_types.h:182-187`), and it moves the options DUMP and
therefore `options_digest`, which every retained bench row pins — so it is an
`hven-bench` re-derivation of every hven row on both lanes, declared under §7. The TRAIN measurement is the
factorization column on the 15 cells above, and the bar is that no cell's
`factorizations` grows.

**The signal A0 needed.** `schur_updates` aggregated onto `SqpCounters`, and a
count of the latch engaging. Today a heuristic watching a solve cannot tell a
subproblem that is bordering cheaply from one that is refactorizing every minor
until it reads the wall clock, which §7 forbids it to assert on.

---

### M4 — The start point is taken verbatim, and CUTEst declares three that lie outside their own box

**Cells: 3** — NET3, STEENBRD, STEENBRE. All three: 0 majors, 0 factorizations,
`f = nan`, `numerical_error`
(`hven-bench/runs/2026-09-m7-h3-cutest/run-A/results.csv`).

**The mechanism, and it is not a defect in either engine.** CUTEst declares a
start point outside its own declared bounds — 41 of NET3's 464 components, 2 each
on STEENBRD and STEENBRE — and `f` is NaN there; clamped into the box by 1e-12 it
is finite at 218 961.90, 2 512.75 and 3 979.07
(`hven-bench/runs/2026-09-m7-h3-cutest/logs/cutest-deriv-nan-cells.log`, the
addendum). hven's SQP entry validates `x0`'s SIZE and its FINITENESS and stops
there — "The box is checked at the model boundary, not here"
(`src/drivers/sqp_solver.cpp:2490-2514`) — so the first evaluation happens at the
declared point, the KKT measurement is non-finite, and the driver takes its
NON-FINITE ITERATE exit with the multipliers cleared
(`:5077-5095`). That exit's own comment states the contract it is keeping: "Only
an `x0` the model cannot evaluate … reaches here", and the warm-start object comes
back COLD "so that a caller retrying from a CORRECTED `x0` gets its own point
honoured".

**The interior-point lane never sees it** because it pushes every bounded
coordinate strictly inside before the first evaluation —
`x[i] = max(x[i], l + p)` with `p = bound_push · max(1,|l|)`, capped for a
two-sided variable at `bound_interval_push · (u − l)`
(`src/drivers/ipm_solver.cpp:491-543`, `IpmOptions::bound_push = 1e-3` and
`bound_interval_push = 1e-2`, `include/hven/drivers/ipm_solver_types.h:240-254`).

**The other two lanes: the premise, at the strength of its evidence (fix1).** For
IPOPT it is DOCUMENTED and hven's own header names the mechanism: the two options
above are transcriptions of "Ipopt's `bound_push`" and "Ipopt's `bound_frac`,
same default" (`ipm_solver_types.h:242-254`), so IPOPT's initialisation moves a
bounded coordinate strictly inside by construction — and IPOPT's lane adapter
says it of itself in the round's own artifact ("IPOPT 'should never see those
points' because it pushes the start point strictly inside the bounds", quoted in
`hven-bench/runs/2026-09-m7-h4-external/README.md`). **For SNOPT the evidence
here is EMPIRICAL and is stated as such:** no SNOPT documentation is cited on
this page and no SNOPT source exists on this box, so what is claimed is only what
was OBSERVED on this set — `STEENBRD/snopt` **`Optimal` at 9 030.08** and
`STEENBRE/snopt` **`Optimal` at 27 458.82**, from the same declaration that gives
hven's SQP lane a NaN at iteration 0 (`four-lane-table.md`). A solver that
evaluated the declared `x0` verbatim could not have produced those rows.
(`NET3/snopt` is `PointCannotBeImproved`, so the set does not claim SNOPT
*solves* an out-of-box start, only that it gets past one.) That observation is
sufficient for the recommendation below, which rests on hven's own contract and
not on what any other solver does.

**PROJECT OR REFUSE — the argument, both ways.**

*For refusing, which is what hven does today.* A start point outside the declared
box is a caller error, and hven's boundary discipline is to refuse caller errors
rather than repair them: `x0` with a NaN throws `std::invalid_argument`
(`:2502-2506`) rather than being cleaned up. Silently moving a caller's point
means the solve did not start where the caller said, and a caller who set `x0`
deliberately just outside a bound — a continuation step, a warm start from a
relaxed problem — would not be told. The `kNumericalError` exit is *designed* for
the retry: it returns a cold warm-start object precisely so the corrected
retry is honoured.

*For projecting, which is what all three other solvers do.* The declared box is
part of the problem; a point outside it is not a point of the problem at all, and
every quantity evaluated there is outside the model's own domain. hven's model
contract ALREADY carries the projection as the right answer:
`NlpProblemModel::start_point()` is "The origin projected onto the declared
variable bounds … a bounds-respecting default"
(`include/hven/model/nlp_problem_model.h:232-237`,
`src/model/nlp_problem_model.cpp:462-468`). So hven agrees that a start point
belongs in the box; it simply does not apply that to the `x0` a caller passes.
And the current behaviour is not "refuse" in the boundary sense at all — it does
not throw, it returns a STATUS, which is the one shape that reads as "the solver
failed" to every consumer including a benchmark table.

*What this page recommends.* **Project, and say so in the row.** Clamp `x0` into
the declared box at the solve entry, count the clamped components in a counter,
and leave the `kNumericalError` exit in place for a point that is in-box and
still un-evaluable — which is what its own comment says it is for. That keeps the
refusal for the case it was written for, gives a caller who wanted to know a
counter to read, and costs a caller whose `x0` is already in the box exactly
nothing. The decision is the settler's; it is a contract change and belongs in
`docs/` as well as in code.

**What a fix must prove.** It changes no in-box solve, so the three replay CSVs
and the corpus suite must come back UNMOVED — that is the whole gate, and it is
a strong one: a clamp that moved a single pinned cell would be clamping something
it should not. Plus a new fixture: an out-of-box `x0` on both engines, with the
clamp counter read. The TRAIN measurement is the three cells producing a solve at
all; STEENBRD and STEENBRE are the plausible wins (SNOPT solves both from the
same declaration), NET3 is not (no lane solves it).

**The signal A0 needed.** A count of start-point components clamped, or — if the
refusal is kept — a status that says "the start point was outside the declared
box", which is a different fact from "the model could not be evaluated" and is
the one a heuristic could act on. Today the three cells are indistinguishable in
the currency from a genuine numerical failure: 0 majors, 0 factorizations,
`f = nan`, `numerical_error`.

---

### M5 — YORKNET: an uncertified `kInfeasible` on a feasible problem, from a restoration whose every iteration re-entered the elastic ladder

**Cells: 1** — YORKNET.

**What the row says.** `infeasible` at `f = 110 670.3` with declared residuals
‖c_e‖ = 4 099.28 and ‖c_i‖ = 131.08 — a wildly infeasible point — after **1**
major iteration, 18 restoration iterations, **19 elastic activations**, 50 241
minors and **52 180 factorizations**
(`run-A/results.csv`, `counters-hven-sqp.csv`). The interior-point lane returns
`optimal` at `14 239.97` on the same declared model through the same bridge in
134 iterations, and IPOPT agrees on every digit the table prints (`four-lane-table.md`). The
problem is feasible.

**What it does NOT say, and this correction is already on the record.**
`sqp_infeasibility_certified = 0` (`logs/yorknet-certified.log`, and the field is
in the side file from H3 fix1 onward), and hven's own contract is that false
means "NO CLAIM IS MADE ABOUT THE MODEL"
(`include/hven/core/solver_status.h:43-68`). **The engine did not issue a false
certificate.** It failed to make progress and reported the shape that says so.
The H3 README withdrew the stronger "false `kInfeasible`" reading itself.

**The mechanism the counters support.** `elastic_activations` counts entries to
the ladder, incremented once per `run_elastic_ladder` call
(`src/drivers/sqp_solver.cpp:1167`), and the restoration sub-solve's own
activations are folded into the parent's (`:4466`). 19 activations against 18
restoration iterations and 1 major is one ladder entry per restoration iteration
plus one for the outer major: **the feasibility sub-problem was itself hard at
every point restoration visited**, and each visit paid a full elastic climb.

**And the phase made no progress whatsoever — this is measured, not inferred.**
The same budget ladder as §M2's, on YORKNET:

| `--budget-iters` | majors | restoration iters | elastic entries | factorizations | `f` | ‖c_e‖ | stationarity |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 3 | 1 | 2 | 3 | 11 452 | 110 670.29950761795 | 4 099.28292 | 1 959.2999458312988 |
| 8 | 1 | 7 | 8 | 22 003 | 110 670.29950761795 | 4 099.28292 | 1 959.2999458312988 |
| 14 | 1 | 13 | 14 | 34 276 | 110 670.29950761795 | 4 099.28292 | 1 959.2999458312988 |
| — (uncapped) | 1 | 18 | 19 | **52 180** | 110 670.29950761795 | 4 099.28292 | 1 959.2999458312988 |

(`raw/YORKNET-default-b{3,8,14}.log` against `run-A`.) **The objective and all
four declared residuals are identical to the last digit at 2, 7, 13 and 18
restoration iterations**, while the factorization count climbs by roughly 2 200
per iteration and `symbolic_analyses` by **exactly 2** per iteration — 6 → 16 →
28 → 38 against restoration iterations 2 → 7 → 13 → 18 — the K0 pattern being
rebuilt twice for every elastic entry. Sixteen further iterations and forty
thousand further factorizations moved nothing.

(A caveat the page owes itself: "identical to the last digit" is an OBSERVATION
across four processes, not a guarantee. CLAUDE.md §7 exempts residual-class
columns from cross-process byte equality, so a last-digit drift on one of them
would have been legal — and would not have touched the reading, which is that
the objective and the violation did not move at all.) Where `STEENBRC`'s restoration went somewhere bad,
YORKNET's went nowhere at all — and the two failures are opposite, which is why
one fix will not serve both.

M3 supplies the price: `n = 312`, so the latch is reachable, and
`border_refine_steps` is 140 against 50 241 minors — **0.3 % of minors bordered,
the lowest ratio on the set.**

**How this exit is reached is not a choice between arms — it is one arm, and the
open question is WHICH REQUEST SOURCE fed it (corrected at fix1).** The page's
first version offered a two-arm dichotomy; the code has one.

*The route is forced.* The parent's switch on the sub-solve's status
(`:4668-4712`) has exactly ONE uncertified `kInfeasible` arm, at `:4680-4686`
(`rs.status == kInfeasible`). Inside the sub-solve that status can arise on this
path only from the REFUSED REQUEST exit (`:4188-4198`): a restoration solve
raising a restoration request of its own, with `allow_restoration_` false. It
cannot arrive from a subproblem — `map_status`'s
`QpStatus::kInfeasible → SolveStatus::kInfeasible` arm (`:6270-6271`) is
UNREACHABLE, because the elastic requester is "the ONLY consumer of
`QpStatus::kInfeasible` in the driver" (`:5628-5632`), and
`include/hven/core/solver_status.h:40-41` says the same from the other side: "no
subproblem status propagates into kInfeasible at all". So YORKNET's row means
exactly this: **the feasibility sub-problem asked for a restoration of its own,
and nothing is allowed to service one.**

*What is open is which of hven's THREE REQUEST SOURCES raised it.*
`solver_status.h:60-66` names them — the funnel's own restoration signature
(`:6012`), the elastic tier's exhaustion (`:5687`, fed by the ladder's
synthesized `kInfeasible` + `SqpFallbackVerdict::kExhausted` at `:1396-1401` and
the `StepVerdict::kRestore` the row takes at `:5661-5665`), and the radius floor
(`:5765` on a routed subproblem failure, `:5983` on a judged rejection).

*And the counters already exclude one of the three.* Both radius-floor sites
increment `rejected_steps` BEFORE requesting (`:5756-5757`, `:5972-5973`), and
YORKNET's parent row reads `rejected_steps = 0` — the sub-solve's own verdict
counters are deliberately not folded into the parent (`:4445-4453`), so that zero
is the parent's own. **The radius floor did not raise the outer request.** The
funnel's signature and the ladder's exhaustion remain, and they are the two that
BOTH leave `StepVerdict::kRestore`, so `solver_status.h:60-66`'s verdict test
separates the floor from the pair and not the pair from each other.

*What would separate them,* and it is S7 at trace depth 1: the sub-solve's last
history row — its `verdict` for the floor, and `elastic_applied` /
`qp_status` on the same row to split exhaustion from signature
(`include/hven/drivers/sqp_solver_types.h:778-900`; the ladder path sets
`elastic_applied = true` at `:5657`). Nothing in any artifact this round produced
carries a sub-solve history row.

**The proposed fix, and its owner.** The driver's restoration and elastic paths,
jointly with M2 and M3 — YORKNET is those two mechanisms compounding, not a sixth
thing. It is named separately because its OUTCOME is the worst one on the set: a
status that a consumer who does not read `infeasibility_certified` will read as a
claim about the model. The narrow, independently valuable change is a
REPORTING one: hven's own contract already says the status alone cannot be read,
and the harness needed a defect report and a re-run to learn that. A status
enumerator that distinguishes "infeasibility certified" from "could not progress"
— or a documented rule that no consumer may bucket `kInfeasible` without reading
the flag — closes it.

**What a fix must prove.** `tests/sqp/test_sqp_restoration.cpp`'s certificate
re-derivation, unchanged; the replay CSVs; and, on TRAIN, YORKNET producing
either `optimal` at 14 239.97 or an honest non-answer that is not spelled
`infeasible`.

**The signal A0 needed.** The elastic ladder's own counters —
`elastic_escalations`, `elastic_rho0_ceiling_hits`, `elastic_floor_retries` — all
exist on `SqpCounters` (`include/hven/core/solver_counters.h:1059, 1114, 1124`)
and **none of them is in any retained artifact**, because the harness's side file
is hand-enumerated and hven publishes no counters dump. 19 ladder entries in one
solve is already a loud signal; how far each climbed is louder, and nothing
recorded it.

---

## 3. The proposed W2 order — a recommendation for the settler, not a decision

Ordered by evidence-per-unit-risk rather than by cells: the first three change
nothing about a solve that is already working, and the last two change
trajectories and cost a declared re-derivation of the pinned corpora.

| # | the change | cells it could move | risk to the pinned corpora |
|---|---|---|---|
| **1** | **The signals** (§5): the counters and the history/trace route that would have let this page be a measurement rather than a reading | 0 directly | **none** — additive counters and an additive publication surface; the replay CSVs' schema is a comparator's, and a new counter that no pinned column reads moves nothing |
| **1b** | **A `tr_init` arm on the A4-0 options vehicle** (added at fix1, the reviewer's item 5) — NO CODE, an options label on the TRAIN 23 | 0 directly, but it is the probe M3's design needs | **none** — it is a label, exactly like `sqp-ssn` and `sqp-ipm`; it changes no default and no pinned row |
| **2** | **M4, the out-of-box start point** | 3 (NET3, STEENBRD, STEENBRE; realistically 2) | **none if it is right** — an in-box `x0` is untouched, so the three replay CSVs and the corpus suite coming back UNMOVED *is* the gate |
| **3** | **M1, the label and the failure budget** (pieces 1 and 2) | 3–5 (ACOPP57 and ACOPP118 are at stationarity 0.64 and 25 when the engine gives up; ACOPR57, BRITGAS, READING6 behind them) | **high** — the failure budget changes trajectories on any cell that ever fails a subproblem. A declared re-derivation of `head-{walk,ssn,ipm}.csv` should be budgeted, not hoped against |
| **4** | **M2, the restoration resume** | 3 (STEENBRB, STEENBRC, STEENBRG) | **high**, and it is a DESIGN question before it is a change — §M2 gives three shapes and argues against all three; it wants the settler's ruling first |
| **5** | **M3, the border-stack latch** | the 8 wall cells' cost, and every cell's factorization column | **highest, and the widest blast radius** — it moves the QP engine, the options digest of every retained hven row on both lanes, and both engines' trajectories. It is also the one whose fix is least clear: three candidates, none measured |

**Two remarks on the order.**

*Why the signals come first even though they move no cell.* Every mechanism
above left at least one question this page could not answer — which uncertified
restoration arm fired on YORKNET, whether the latch or a row-pattern rebuild
drove `ACOPR57`'s 428 symbolic analyses and `ACOPR118`'s 3 115, how far each of
YORKNET's 19 ladder entries climbed. Each of those is one counter or one published row away. Doing (3), (4)
or (5) without them means re-deriving the pinned corpora to test a fix whose
effect still cannot be read.

*Why M3 comes last even though it touches the most cells.* Its own §7 cost is
the largest on the list, and the A4-0 sweep is currently measuring the two
alternatives to `kWalk` on this exact set. Whatever that sweep concludes is an
input to M3 and not to any of the other four.

*Why (1b) is on the list at all, and why it is free.* `SqpOptions::tr_init` is
**1.0** — an ABSOLUTE initial radius (`include/hven/drivers/sqp_solver_types.h:388`),
on a set the engine runs unscaled. It bears on three of the five mechanisms at
once, and on the one this page could not settle:

- **M3.** An l-infinity box of ±1 pins every variable whose natural scale exceeds
  1, and a pinned variable is what takes the border stack past `schur_cap = 128`.
  §M3's second candidate — "the trust region pins variables the problem does not"
  — is exactly this, and a `tr_init` arm is the measurement that decides between
  it and "the cap is simply fixed at 128" **without designing either fix**.
- **M1.** The subproblems that run to their minor cap do so at Δ = 1 on cells
  whose violation at the start point is 2 (BRITGAS) to 10 000 (STEENBRC): the
  first subproblem cannot reach a feasible point inside its own box, which is a
  plausible reason for the walk to exhaust a size-derived budget.
- **M2.** The restoration phase restarts at
  `max(tr_min, kRestoreRadiusFactor · tr_init)` (`src/drivers/sqp_solver.cpp:6288-6291`),
  so the excursion §M2 measures is taken at a radius `tr_init` sets.

It is a one-field label on a lane that already runs labels, its rows are
counters, and it says something before any of (3), (4) or (5) is designed. That
is why it is in the zero-risk tier rather than behind M3.

---

## 4. What is NOT the mechanism, with the evidence

**The inner QP kernel is not the mechanism.** Under the default
`qp_mode = kWalk` the SQP lane solves 2 of 23; under `kIpm` it solves the same 2;
under `kSsn` it solves 4, and the two it adds (`ACOPP118`, `ACOPR57`) are the two
§M1 shows were converging when their subproblem ran out of minors
(`hven-bench/.scratch/w2a4/runs/hven-sqp/{default,sqp-ipm,sqp-ssn}/cutest-run-A/results.csv`).
Every structural failure survives the change of kernel intact: NET3, STEENBRD and
STEENBRE are 0-iteration `nan` exits in all three arms; all 8 `dnf_wall` cells
are `dnf_wall` in all three; YORKNET is `infeasible` at the identical
`f = 110 670.2995076` in all three, with 52 180 / 52 205 / 52 215 factorizations;
DALLASM returns the identical `f = 61 974 881.4829069` in all three.
**The gap is upstream of the QP** — which is what the W0 CLOSED entry's register
item (2) already said from the sweep's first read, and this is the per-cell form
of it.

*One thing the sweep did add, and it belongs here rather than in a mechanism
section:* `kSsn` turns four `numerical_error` cells (BRITGAS, STEENBRB, STEENBRC,
STEENBRG) into rows with a **`null` status and an `error` outcome** — the cell
process produced no row at all, where `kWalk` produced one. That is a harness-
visible hard failure under a non-default mode and it is the A4-0 sweep's finding
to report, not this page's; it is named here only so that "4 of 23 under `kSsn`"
is not read as strictly better than "2 of 23".

**NLP scaling is not what separates the two hven lanes.** The SQP lane runs with
`enable_scaling = false` (`run-A/options/*-hven-sqp.txt`), and it is fair to ask
whether STEENBR's ‖c_e(x0)‖ of 2 000 to 10 000 is a scaling problem. But **the
interior-point engine has no NLP-scaling option at all** — `IpmOptions` exposes
no gradient- or constraint-scaling rule, only `obj_scale`, which the dump reads
as `1` (`include/hven/drivers/ipm_solver_types.h:336`,
`run-A/options/*-hven-ipm.txt`) — and it solves 15 of 23 including all three
STEENBR cells the SQP lane blows up on. **Whatever the SQP engine needs on this
set, the IPM lane demonstrates that NLP scaling is not necessary for it.**
`enable_scaling = true` remains a lever worth measuring; it is not the
explanation. *(Corrected at fix1: the first version read the IPM dump's
`qp_scaling = 0` as "no NLP scaling". It is not an NLP setting at all — it is
the MKL Pardiso MPS matrix-scaling flag, `iparm[10]`, a linear-solver option
(`ipm_solver_types.h:312-316`). The conclusion is unchanged and now rests on the
absence of the option rather than on the wrong one.)*

**The tier's §6.3 infeasibility-suspicion detector is not firing here.**
`suspect_escalations` is **0** on every one of the 15 cells with rows
(`counters-hven-sqp.csv`), which is the reading its own header calls for — "Zero
is the expected reading and a nonzero one is a finding"
(`include/hven/core/solver_counters.h:1143-1148`). The W2 register's item (1) —
the suspicion detector false-firing on consistent subproblems — has two
constructed vehicles and **no TRAIN cell** behind it on this set.

**The bridge is not the mechanism, and the models are not.** Settled at H3 and
its addendum, restated in §0.

**And the globalization did not accept a blow-up.** `steps_accepted` is **0** on
STEENBRB, STEENBRC, STEENBRG, BRITGAS, DALLASM and YORKNET
(`counters-hven-sqp.csv`): on the six worst cells on the set, the funnel never
accepted a single step on the caller's own problem. The objectives of 4e12 and
1.8e14 came from the restoration phase, which is not judged by the acceptance
test at all (§M2). **This corrects the reading the task was dispatched under.**

---

## 5. The signals A0 must surface

Each entry names what could not be read this round, what would have read it, and
where it lives. They are additive — no existing counter changes meaning.

| # | the signal | why this round needed it | where |
|---|---|---|---|
| **S1** | **`dump_counters()`, the twin of `dump_options()`** | the harness's side file is HAND-ENUMERATED from the public structs "because hven has no counters dump" (`hven-bench/runs/…/counters-hven-sqp.csv`'s own header), so 20 of `SqpCounters`' 38 direct fields — including every elastic-ladder field — reached no artifact this round | `src/drivers/sqp_options.cpp` has the pattern; W0 T5(b) is the precedent and the X-macro tables (`HVEN_SQP_COUNTERS_FIELDS`) already generate the trace's `counters` object |
| **S2** | **QP-exit counters: `kMaxIter` exits and `kNumericalError` exits, counted apart** | §M1: a capped subproblem and a failed factorization are one number in every consumer column, and the distinction is the difference between "give it more minors" and "the linear algebra broke" | `SqpCounters`, beside `rejected_steps`, whose own doc says the distinction lives on the history row |
| **S3** | **`qp_failures_in_a_row`'s peak, and the count of routed failures** | §M1(b): the solve ends on the second consecutive failure and no counter records that this is what happened | `SqpCounters` |
| **S4** | **`schur_updates` aggregated** | §M3: the one direct reading of "was bordering available?", deliberately not aggregated today (`include/hven/core/solver_counters.h:986-988`) | `SqpCounters`, one `+=` beside `factorizations` |
| **S5** | **A restoration excursion record: `(h_in, h_out, f_in, f_out)` per entry** | §M2: an objective moving by 1e11 across a phase that by contract produces no history row is invisible | `SqpCounters` (max-folded ratios) or a `sqp.restoration` trace event |
| **S6** | **A start-point clamp count** | §M4: three cells are indistinguishable in the currency from a genuine numerical failure | `SqpCounters`, and the IPM's interior push should carry the twin |
| **S7** | **The history, or a trace, published by the bench lane** | every "what would separate them" in §2 is answered by one `sqp.major` line; `SqpResult::history` already carries `qp_status`, `qp_minor_iters`, `qp_factorizations`, `verdict`, `tr_radius`, `violation_l1` and `elastic_applied` (`include/hven/drivers/sqp_solver_types.h:778-900`) | **harness-side** (`hven-bench/lanes/hven/hven_bench_cell.cpp`), not hven — hven's surface is already sufficient and `JsonLinesTraceSink` gained `flush()` at W0 T5(c) |

**The A0 reading these support.** Four of the five mechanisms would have been
caught in-flight by a heuristic reading S2/S3 (a subproblem at its cap),
S4 (bordering unavailable), S5 (a restoration excursion) and S6 (a clamped start
point). None of the four is readable today, and the one signal that IS loud in
the current currency — YORKNET's 52 180 factorizations in one major — is loud
only in hindsight, because nothing declares what a factorization count *should*
be for a problem of a given size.

---

## 6. Found at source while reading, and not fixed here

This is a design page; none of these is repaired by it. Each is reported with the
file and line so the settler can place it.

1. **`docs/trace-schema-v0.md:699-701` is stale at HEAD.** Reader rule 5 reads
   "There is no flush at destruction. The stream is BORROWED: the sink never
   opens, closes or flushes it." M7 W0 T5(c) changed exactly that: the sink "DOES
   flush it, on request (`flush()`) and once more at destruction"
   (`include/hven/drivers/trace_writer.h:37-41`). The header moved and its
   schema page did not. Nothing on this page depends on the difference; a reader
   of the schema deciding whether to trust a truncated artifact does.
2. **`hven-bench/runs/2026-09-m7-h4-external/README.md`'s budget table records
   the `hven-sqp` iteration ceiling as 500.** It is 100 — see §1's note, which
   gives the three places that say so, including every row's own
   `budget_iters` column. The correction moves the round's "the hven lanes ran
   under a ceiling six times tighter than IPOPT's" from conservative to *more*
   conservative on one of the two lanes: thirty times, not six. **Harness-side,
   and this is the A4-0 lane's file, not this task's to edit — and it is now
   CLOSED there** (`hven-bench` `3968f3f`, 2026-09-17, an APPEND-ONLY correction
   record under a new "CORRECTIONS (append-only)" heading; the README's original
   text is not edited, on the same discipline `notices/` is held to).
3. **`SolveStatus` has no way to say "the subproblem ran out of minors".**
   `src/drivers/sqp_solver.cpp:6268-6277`. §M1(a). The fact exists on the history
   row's `qp_status` and reaches no consumer surface.
4. **`SqpCounters` does not aggregate `schur_updates`, by an explicit decision**
   (`include/hven/core/solver_counters.h:986-988`), and the driver-level currency
   consequently cannot distinguish a bordered minor from a refactorized one.
   §M3, §S4.
5. **The bench's hven lane publishes neither `SqpResult::history` nor a trace**,
   so every per-major question this page asked had to be answered by re-running
   the whole solve at a tighter budget. **Harness-side** — hven's surface is
   already sufficient. §S7.

---

## 7. What is here

```
docs/notes/2026-09-m7-w2-sqp-cutest-read.md          this page
docs/notes/data/2026-09-m7-w2-sqp-read/
  PROVENANCE.txt                                    the §7 stamp, the co-run
                                                    declaration and its window
  raw/<PROBLEM>-default-b<k>.log                     38 completed cells + 3 that
                                                    hit this task's own cap;
                                                    each carries its own
                                                    LOCK= line, its pre-leg
                                                    pgrep of the foreign lane,
                                                    and the cell's whole row
  raw/foldproof-false.log                           the leg wrapper's fold
                                                    proof: a stubbed binary
                                                    propagates (LEG_RC=1)
  raw/NOSUCHPROB-default-b1.log                     the batch driver's fold
                                                    proof: an unknown problem
                                                    propagates (rc 2)
  identity-check.txt                                the four b0 legs against
                                                    H3 run-A, 148 / 148
  scripts/run_cell.sh                               one leg
  scripts/batch.sh                                  several in sequence
  scripts/summarise.py                              folds raw/ into one table
```

Everything else this page cites is somebody else's retained artifact and is cited
by path: `hven-bench/runs/2026-09-m7-h3-cutest/`,
`hven-bench/runs/2026-09-m7-h4-external/`, and — read-only, as another
implementer's live work — `hven-bench/.scratch/w2a4/runs/hven-sqp/`.
