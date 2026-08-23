# Two-level model contract, stage 2 — wall-leg record (M4, hven side)

BASE `a062fcc` (stage-2 parent) vs HEAD `1939a3d` (the stage-2 tip as first
measured; the chain now closes at `b1b2c4d` -- see the fold section below).
Instrument: the standing IPM wall leg — whole solves of a
dense-Jacobian NLP through NLPSolver, 9 reps x 15 inner solves, base/head
alternated per rep, serial arm pinned under `MKL_NUM_THREADS=1` and threaded
arm, n in {60, 120, 240}. `aggregate.py` prints median and minimum estimators
with the base rep-spread; deltas are read off the MINIMUM estimator. Binaries
built by `build_ipm_time.sh` against each side's `libhven.a`; provenance and
image checksums in `provenance.txt`.

Standing policy: one-sided +3.0% band at serial n=240; positive mechanism
evidence auto-closes a trip; hot-path symbol growth investigates at any size.

| arm | n | median | minimum | base rep-spread |
|---|---|---|---|---|
| serial | 60 | -0.77% | -0.69% | 0.55% |
| serial | 120 | -1.76% | -1.76% | 2.60% |
| **serial** | **240** | **-2.20%** | **-2.69%** | **0.54%** |
| threaded | 60 | -1.23% | -1.20% | 31.21% |
| threaded | 120 | -1.50% | -1.44% | 32.62% |
| threaded | 240 | -2.55% | -2.67% | 0.49% |

**ASSERTED.** The asserted cell is serial n=240 at -2.20% / -2.69% — inside the
band, and on the fast side of it. The box was quiet at the gate (loadavg 0.49,
no other compile or bench running; the leg waits for loadavg[1] < 0.6 while
holding the box lock) and the n=240 base rep-spread is 0.54%, well under the
~1.3% above which a run is informational rather than asserted. The two smallest
threaded cells carry their usual 31-33% rep-spread and are not read.

**Identity.** All 54 base/head pairs bit-identical: `flag=0` throughout, and one
`xnorm2` per size on both sides — 1.3750963021836506 (n=60),
2.7501926043856093 (n=120), 5.5003852082409646 (n=240).

**Mechanism (`nm`).** Of 287 filtered symbols in the two probe images, exactly
ONE changed size: `NonLinearProgram::assemble_impl`, 0x3249 -> 0x3259, +16
bytes. That is the refused-shape-name argument added to the terminal refusal
arm, which is cold and unreachable for every supported request. No other symbol
moved, so nothing on the hot path grew and the -2.2% is machine-state and code
layout rather than any change in work done.

**What this leg cannot see.** The interior-point probe image links no
`AggregateEvalSeam` symbol at all — the interior path never reaches the seam —
so the seam work in this stage (the stale-bundle refusal, the max-location
scan, the block disjointness check, the docstring corrections) is invisible
here by construction. The SQP-side bench is the arm that observes it; its
checks are per lay and per major, not per minor.

Files: `leg_per_rep.csv` (raw per-rep rows: arm, rep, side, n, inner, median_s,
min_s, flag, xnorm2), `leg_aggregate.txt`, `provenance.txt`, `nm_base_a062fcc.txt`
/ `nm_head_1939a3d.txt` (symbol-size dumps filtered to the seam, provider,
aggregate and driver symbols), and the three harness scripts as run.

## SQP-side leg — the arm that CAN observe the seam

The instrument above links no seam symbol, so a second leg was run on the arm
that does: `hven_sqp_bench`, built at both sides, three standing cells, 7 reps,
base/head alternated per rep, each run pinned to one core under
`MKL_NUM_THREADS=1`, with the same box-quiet gate (started at loadavg 0.52).
This is where A3's per-major bundle check and A4/A5's per-lay checks would show.

| cell | median | minimum | base rep-spread |
|---|---|---|---|
| F3 n=1000 cold | +0.32% | +0.11% | 1.73% |
| F3 n=1000 warm | +0.77% | +0.38% | 0.96% |
| F7 n=200 cold | −0.23% | −0.34% | 0.81% |

Every reading is at or inside its own cell's base rep-spread, and the sizes match
the standing precedent for this instrument (the Task 6 leg read +0.13% / +0.08% /
+0.17% on the same three cells). This is the expected shape for adding fixed work
per lay and per major to a path whose cost is per minor: A3's check runs once per
`build_subproblem`, A4's and A5's once per `lay()`, and none of them is per
element.

Raw rows in `sqp_leg_per_rep.txt` (rep, side, cell, wall seconds); harness in
`run_sqp_leg.sh`.

## Cross-lane fold — SQP-side leg re-run at the fold HEAD

The fold changed the seam TU (the claim-block scan became disjointness-only),
so the SQP-side leg was re-run. BASE `5cd8ae9`, **HEAD `b1b2c4d`**, same
protocol as above, box quiet at the gate (loadavg 0.49).

| cell | median | minimum | base rep-spread |
|---|---|---|---|
| F3 n=1000 cold | −0.11% | −0.32% | 0.97% |
| F3 n=1000 warm | +0.19% | +0.00% | 0.96% |
| F7 n=200 cold | +0.26% | +0.17% | 0.31% |

Every reading is inside its own cell's base rep-spread.

**`nm`, and what it caught.** The first fold head (`15147eb`) ordered the three
blocks with `std::stable_partition` + `std::stable_sort`. The symbol diff showed
that costing **eight new out-of-line merge-sort and stable-partition
instantiations** in the seam TU, with `lay()` growing `0x1007 → 0x13ff` — about
a kilobyte of machinery to put three pairs in order by one integer. That is code
growth, not layout, so it was not accepted on the wall reading: `b1b2c4d`
replaces the library sort with an insertion sort written out over the same fixed
array. At the recorded HEAD the seam symbol set is **identical to base, 22 for
22, with no new instantiations**, and the only delta is `lay()` itself,
`0x1007 → 0x11e7` (+480 bytes) — the non-negativity pass, the widened coverage
sum, the length check ahead of the narrowing, the hand sort and the fuller
refusal messages, all in a per-lay routine.

Provenance for this leg (hardware, toolchain, date, binary checksums, box state)
is in `provenance_sqp.txt`; the IPM leg's is in `provenance.txt`. Raw rows in
`sqp_leg_fold_per_rep.txt`, aggregate in `sqp_leg_fold_aggregate.txt`, symbol
dumps in `nm_fold_base_5cd8ae9.txt` / `nm_fold_head_b1b2c4d.txt`.
