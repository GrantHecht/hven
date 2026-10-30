# WX-R1 pre-landing TRAIN check -- DECLARATION (written and committed before the first cell runs; never edited afterwards)

Settler ruling C153 requires a TRAIN check of the WX-R1 convergence-gate fix before it lands (review MEDIUM-1: an absolute
`kkt_tol` on the complementarity product `lambda_j * c_j` might refuse genuine optima at TRAIN scale, where multipliers are
~1e3, and cause zero-step spins to `kMaxIter`).

## Pin

- hven commit measured: `ed41225391f9d611211e68504515636340cb5ab9` (branch `wx-r1`, the riders commit). Its library code is
  identical to the fix commit `ae151b87` (the riders commit changed comment lines only: `git diff -w cd43ec35 -- src include`).
- Harness: hven-bench `b5423f6` (RETAKE19's close), run from a SCRATCH CLONE under this worktree's `.scratch/train-bench`
  with only `cmake/hven_pin.cmake` changed to the pin above (a local scratch commit). Nothing is committed or pushed to hven-bench.
- The comparison baseline is RETAKE19's committed run `runs/2026-10-m7-w2-retake19/` at hven `6a2a58c7` (m7 head with the
  same toolchain, MKL 2026.2.0-881). The pin moves from `6a2a58c7` to this commit: the difference is the m7 work since
  `6a2a58c7` plus the WX-R1 fix. Any moved row is therefore attributed to "the fix" only where the run itself shows a
  complementarity refusal; otherwise the cause is reported as unattributed.

## What runs

- Labels: `default` (= `hven-sqp`, exact Hessian) and `sqp-qn` (= `hven-sqp-qn`, `hessian_mode = kLbfgsDamped`).
- Cells: every TRAIN cell of the `cutest-nlp-35 v1` set (23 problems, roster derived from `sets/cutest-nlp.json`).
- Budget: the lane's 300-iteration `max_iter` (restoration included), the set's 300 s wall ceiling; `MKL_NUM_THREADS=1`, one cell
  per lock hold under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` as its own prior command, no `timeout` wrapper.
- 46 runs (23 x 2), driven by the harness's own runner through `scripts/w2_a4_sweep.sh`, as RETAKE19's chain did. HOLDOUT is never run.
- The options dumps are expected UNCHANGED from RETAKE19's (the fix touched no dumped option); each row's `options_digest` is
  compared by the comparator to the digest of RETAKE19's declared dump (default
  `385ad28e01b9c50b93171cb86880fdc3f264b34f4ea6f0fc0705108978b11906`, sqp-qn
  `c1eb1e260373a8f7551e0a20496bcbc0985e35a65f1a6e8c8adfa912d03c1d2f`); a mismatch is stated in the results, not hidden.
- Not run: RETAKE19's pre-chain / pre-flight scripts (they assert a declaration README of that round). The comparator
  (`scripts/w2_retake19_compare.py`) is run unmodified with `HVEN_RETAKE19_RUN` pointing at RETAKE19's run directory, so its
  measured rows and declared dumps are RETAKE19's own.

## Predictions (each cell's prediction is its RETAKE19 row: status, majors, factorizations)

Source: `runs/2026-10-m7-w2-retake19/run-A-<label>/cutest/{results.csv,counters-hven-sqp.csv}`. `dnf_wall` rows are the cells
RETAKE19 ended at the 300 s ceiling (status `null`, no row). Expected optimal counts: `default` 16 of 23, `sqp-qn` 8 of 23,
union 17 against SNOPT's 15 of 23.

#### label `default`

| cell | status | majors | factorizations | objective |
|---|---|---|---|---|
| ACOPP118 | optimal | 8 | 11372 | 129660.68619604065 |
| ACOPP57 | optimal | 5 | 82 | 41737.786849846176 |
| ACOPR118 | optimal | 25 | 14802 | 129660.68370269192 |
| ACOPR57 | optimal | 16 | 1618 | 41737.786035029698 |
| BIGBANK | optimal | 23 | 3219 | -4205693.2995377351 |
| BRITGAS | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| DALLASM | optimal | 18 | 413 | -48198.188192052978 |
| MSS2 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| MSS3 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| NET3 | optimal | 23 | 30702 | 5859393.7023180835 |
| READING6 | optimal | 8 | 567 | -144.65970057278301 |
| READING8 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| SAWPATH | optimal | 9 | 61 | 750.45894675975114 |
| SMBANK | optimal | 29 | 29 | -7129292 |
| STEENBRB | optimal | 136 | 33003 | 9075.8553755338289 |
| STEENBRC | optimal | 102 | 50801 | 27504.936820482559 |
| STEENBRD | optimal | 85 | 30401 | 9030.0818667716558 |
| STEENBRE | optimal | 80 | 23417 | 27459.163311770888 |
| STEENBRG | optimal | 78 | 31030 | 27420.929665317915 |
| TAX213322 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| TAX53322 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| YORKNET | numerical_error | 40 | 12863 | 14706.403707417367 |
| ZAMB2-8 | optimal | 8 | 144 | -0.15293592839679399 |

#### label `sqp-qn`

| cell | status | majors | factorizations | objective |
|---|---|---|---|---|
| ACOPP118 | optimal | 119 | 106 | 129660.686196043 |
| ACOPP57 | optimal | 34 | 29 | 41737.786850017314 |
| ACOPR118 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| ACOPR57 | optimal | 264 | 450 | 41737.786035180863 |
| BIGBANK | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| BRITGAS | optimal | 16 | 3990 | 0 |
| DALLASM | optimal | 142 | 549 | -48198.188192052992 |
| MSS2 | numerical_error | 10 | 6183 | -2.234774541233735 |
| MSS3 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| NET3 | max_iter | 7 | 13555 | 6409199.3858827297 |
| READING6 | optimal | 50 | 61 | -144.65970057278562 |
| READING8 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| SAWPATH | numerical_error | 16 | 4457 | 5198.122401693613 |
| SMBANK | optimal | 194 | 216 | -7129292 |
| STEENBRB | numerical_error | 64 | 18244 | 46510529.562034786 |
| STEENBRC | numerical_error | 66 | 58184 | 3164122027.6027775 |
| STEENBRD | max_iter | 284 | 22189 | 9265.8148155042254 |
| STEENBRE | max_iter | 277 | 32972 | 28608.165058138973 |
| STEENBRG | numerical_error | 64 | 46235 | 3164345527.5795503 |
| TAX213322 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| TAX53322 | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| YORKNET | dnf_wall (capped at the 300 s ceiling; no row) | - | - | - |
| ZAMB2-8 | optimal | 34 | 165 | -0.15293592764282651 |

## STOP rule

The landing STOPS and goes to the settler if either:

1. any RETAKE19-optimal cell (of either label) is not `optimal` in this run; or
2. any cell newly ends `max_iter` (relative to its RETAKE19 row) with a ZERO-STEP signature, defined from the run's own
   columns as: the returned point has `sqp_stationarity <= 1e-6` and `sqp_feasibility <= 1e-6` while
   `sqp_complementarity > 1e-6` (the third conjunct alone is refusing), or the harness zero-motion counters
   (`harness_qpsum_zero_motion_exhausted`, `harness_qpsum_zero_motion_cap_hits`) are nonzero where RETAKE19's were zero.

Anything else that differs (status, majors, factorizations) is reported as a moved row with its cause read from the run, not a stop.

## READ rule for wall-clock-dependent cells (as in RETAKE19)

A status `dnf_wall` is wall-clock dependent and not scheduling-invariant (CLAUDE.md s7). Cells RETAKE19 ended at the ceiling
that finish solo above it (`default` BRITGAS 377.3 s; `sqp-qn` ACOPR118 415.5 s, BIGBANK 484.7 s, YORKNET 380.3 s) are predicted
`dnf_wall` and a READ if they end at the wall. A cell predicted `dnf_wall` that finishes, or a finished cell that ends at the
wall, is a READ finding, not a stop, unless it is also a RETAKE19-optimal cell turned non-optimal (rule 1). A deviation observed
here that rests on a wall is re-run ALONE under the serial rule before it is called one. Counters and statuses of finished rows
are exact; residual-class columns are compared under the comparator's near-ulp gate. Wall-clock numbers are informational.
