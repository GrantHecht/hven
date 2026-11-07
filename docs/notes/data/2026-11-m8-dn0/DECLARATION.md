# M8 DN-0 -- the small-dense measurement round (DECLARATION)

This file is the declaration. It is committed, with every instrument under `tools/`, BEFORE the
first measuring leg, and it is never edited afterwards. A failed leg gets a successor
`DECLARATION-r1.md` (then `-r2`, ...); the failed record is kept beside it. Results are written
to separate files (`REPORT.md`, `results/`, `PROVENANCE.txt`).

Authority: `docs/notes/2026-11-m8-brief.md`, section 2, "DN: small dense problems" (DN-0 and the
pre-registered G-DENSE rule). DN-0 is a measurement of the current code. It writes no dense arm,
changes no engine behaviour, and adds no compiled file to hven.

## 1. Pin and binaries

- **hven** `58dc14bc54a4d4225c6903ff73e3e9722e23ae87` (origin/m8 head; library objects identical to
  the PF-1 head `e55cecde`, the commits after it being comment/test/docs only). Built by the
  harness's `scripts/build_hven_pin.sh` (Release, clang++, `HVEN_LINK_TIME_OPT=OFF`, tests and
  bench off) into `.hven/install-58dc14bc54a4/`. `libhven.a` sha256 `2cc652f7...54fa`.
  The DN-0 hven commits (this directory) add no compiled file.
- **hven-bench** worktree `/home/ghecht/Projects/hven-bench-sd`, branch `m8-dn0` off main
  `7e7596e`, at `6ad47d7` = an EVIDENCE-ONLY pin move (`cmake/hven_pin.cmake` bdd797e3 -> 58dc14bc),
  reverted in the commit after the run and never landed on main (the PF-1 TRAIN precedent,
  `runs/2026-11-m8-pf1-train/README.txt`). Lane sources unchanged.
- Binaries (paths relative to the harness root; `tools/expected.sha256`; `tools/preflight.sh`
  refuses any leg if one differs, the harness HEAD is not `6ad47d7`, or its tree is dirty):

  | binary | role | sha256 |
  |---|---|---|
  | `build/lanes/hven/hven_bench_cell` | MEASURED lane binary (configure.sh, unchanged) | `6fd1cdce...d5a5` |
  | `.scratch/dn0/build-dump/lanes/hven/hven_bench_cell` | DUMP build: same objects + `-Wl,--wrap=pardiso` + `tools/pardiso_wrap.c` | `e25afa3d...da30` |
  | `.scratch/dn0/build-tools/dn0_replay` | replay, measuring | `80a21da9...00af` |
  | `.scratch/dn0/build-tools/dn0_replay_verify` | replay, iparm-identity check (`--wrap=pardiso`) | `1ad55842...eded` |

  All 36 lane object files of the dump build are byte-identical to the measured build's
  (checked with `cmp`); the dump binary differs only at link. The measured binary carries no
  `__wrap_pardiso` (checked by `tools/build_instruments.sh`).

## 2. Population and terms

- **HS TRAIN (22)**: the 21 TRAIN members of `sets/hs-27.json` plus `hs071` (h1-bootstrap, TRAIN
  by the same split policy). **Small-dense CUTEst TRAIN (97)**: the TRAIN members of
  `sets/cutest-small-dense.json` (cutest-small-dense v1). Both hven lanes: **238 cells**.
- `tools/roster.py` re-checks every name against `schema/split.py` (split-v1) and every CUTEst
  name against `schema/holdout_guard.py`; any HOLDOUT name aborts before a cell runs.
  **HOLDOUT is never run.**
- Each cell is the runner's hven-lane argv unchanged (`runners/run_serial.py`, `run_cell`):
  `--lane L --problem P --options default --budget-iters 0 --options-dump F --assert-reference`,
  cwd = harness root, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, shipped defaults (engine iteration
  caps), wall cap **300 s** for every cell (the cutest-nlp-35 ceiling; the small-dense pin carries
  none). A capped cell is DNF (`dnf_wall`).
- Serial: one cell process at a time, alone under `/tmp/box-build.lock`, pinned with
  `taskset -c 2` (perf runs inside the pin, so only the lane process is counted).

## 3. Instruments (`tools/`)

- **S -- totals** (`legs.sh S1/S2/S3`, `cell.sh stat`): `perf stat -x, -e
  instructions:u,cycles:u,branches:u`, one process per cell. One untimed, unrecorded warm-up
  process first (`docs/measurement.md` §4). Round 1 over all 238 cells; rounds 2 and 3 over the
  cells not capped in round 1. Cell total = the median of its rounds. The round-1 CELL row is the
  cell's status row.
- **P -- buckets** (`legs.sh P`, `cell.sh record`, `attribute.py`, `bucket_map.tsv`): `perf record
  -m 1M -e instructions:u -c PERIOD --call-graph dwarf,16384 --user-callchains` (the box's
  `perf_event_paranoid = 2` requires `--user-callchains`). PERIOD base `= max(1000003, T/8000)` (odd),
  `T` = the S median; repeats `R = min(40, ceil(4000 * base / T))`, repeat `k` at period
  `base*(1000+7k)/1000` (odd) so repeats sample different phases; a capped cell gets one 60 s
  repeat (informational: never a median cell, see §5). Samples are weighted by their period.
  `perf script --no-inline` text is attributed by the frozen `bucket_map.tsv` with the rule in
  its header: frames leaf to root, the first frame matching any row decides; externals are
  transparent; no-match chains fall back on the leaf (ld-linux -> B_proc, MKL/LAPACK/OpenMP
  runtime -> B_fac, CUTEst/libpb/libgfortran -> B_eval, else B_eng = unmatched). **B_proc**
  (process start/exit and harness-only work) is reported and excluded from the total the shares
  read. Unmatched, fallback and lost-sample counts are reported per cell.
- **D -- KKT dumps** (`legs.sh D`, `cell.sh dump`, `pardiso_wrap.c`): the DUMP build, one process
  per cell. The link-time wrap of MKL's public `pardiso` records every call (`index.tsv`: phase,
  session handle, n, nnz, nrhs, error, inertia) and writes the matrix, input iparm and returned
  inertia of the FIRST 50 numeric factorizations (phases 12/13/22/23) of the cell. A boundary
  instrument at MKL's public ABI (CLAUDE.md §6): no hven source, no libhven.a object, and no
  measured binary changes. Every SymmetricFactor consumer is captured (KKT, equality-basis Gram,
  independence screens); the options profile of each matrix is recorded. Cap 300 s (60 s for a
  cell capped in S1).
- **R -- dense replay** (`legs.sh R`, `dn0_replay.cpp`, built against the installed hven by
  `find_package`): each dumped matrix, in isolation, in one process per cell:
  - sparse arm: `SymmetricFactor(opts)` construct + `analyze` + `factorize` + `solve` (one RHS of
    ones), destruction excluded; `opts` rebuilt from the captured iparm;
  - dense arm: `DenseSymmetricFactor` construct + `try_factorize` + `solve` on the densified upper
    triangle; densification counted separately and NOT in the arm (reported).
  User-space instructions by `perf_event_open` around each arm; three repetitions with arm order
  S,D / D,S / S,D, per-arm median; one untimed warm-up of both arms on the cell's first matrix.
  `dn0_replay_verify` first runs every sparse arm once and checks that its phase-22 input iparm
  equals the captured one on the input fields (0,1,3,4,5,7,9,10,11,12,20,23,24,26,27,30,33,34,
  35,36,55,59).

## 4. Definitions (read by `tools/rule.py`, the rule's only implementation)

- Buckets (brief): B_sym, B_fac, B_kkt, B_eval, B_setup, B_eng. `total` = their sum (sampled
  instructions of the cell excluding B_proc). Share = bucket / total.
- `rho` (the rule's) = sum over the cell's replayed matrices of dense-arm instructions / sum of
  sparse-arm instructions (brief: "the sum of dense instructions over the sum of sparse
  instructions across that cell's replayed matrices"). Reported beside it, never read by the rule:
  `rho_incl_densify` (densification added to the dense arm) and `rho_engine_weighted` (sparse
  analyze counted only where the engine analyzed, each solve weighted by the RHS columns the engine
  solved on that factorization, from `index.tsv`).
- Negative-eigenvalue agreement = the share of the cell's replayed matrices whose dense
  Bunch-Kaufman negative count (`block_evidence()->neg_eigs`) equals the replay's sparse
  `InertiaEvidence::negative`. A matrix where either arm yields no count (backend error, exactly
  singular, query failed) counts as NOT agreeing. Reported beside it: engine-recorded (in-solve)
  inertia vs dense, and replay fidelity (replay sparse vs engine-recorded).
- Median cell, per engine (IPM = `hven-ipm`, SQP = `hven-sqp`) x population (HS TRAIN,
  small-dense TRAIN): cells sorted ascending by total instructions (the S median), a cell capped
  in S1 ranked above every finished cell (ties by name); the median is the cell at rank
  `ceil(N/2)` (22 -> 11, 97 -> 49). A median cell with no replayed matrix has no rho and fails the
  DN-1 condition.

## 5. The pre-registered rule (brief, verbatim table; scope as declared here)

| fires | condition | then |
|---|---|---|
| **DN-1** (dense arm behind the factor layer) | `(B_sym + B_fac) / total >= 0.40` **and** `rho <= 0.5` on that cell, **and** negative-eigenvalue counts agree on >= 99 % of replayed matrices | build DN-1 |
| **DN-2** (dense KKT assembly) | `B_kkt / total >= 0.25`, measured after DN-1 if DN-1 was built | build DN-2 |
| **no dense build** | neither fires | record which bucket dominates. If it is B_setup or B_eval, that bucket is registered as the small-problem lever with its numbers, and no dense code is written in M8 |

- Read on the four median cells. DN-1 fires for an engine if its condition holds on BOTH of that
  engine's median cells, and for the milestone if it fires for either engine; the per-engine
  outcome is recorded either way. The agreement clause is read over the median cell's own
  replayed matrices (the row's "on that cell").
- DN-2 is read with the same scope (an engine's two median cells; milestone if either engine).
  If DN-1 fires, this DN-2 reading is provisional: the brief re-measures it after DN-1 is built.
- "Dominates" = the largest share on each median cell; reported per median cell, plus the
  population-wide picture.
- **Primary reading** = the definitions above: the rule outcome.
- **Secondary reading, pre-registered, informational**: the same mechanics with the process-once
  solver initialization removed from B_setup and from the total (samples whose chain contains
  `hven::detail::ensure_solver_initialized`), median cells ranked by the resulting per-run
  steady-state instructions. If the two readings give different G-DENSE outcomes, both are
  reported; the primary is the rule outcome.

## 6. What the instrument check before this declaration saw (disclosure)

The instruments were exercised before this commit on four cells OUTSIDE the DN-0 population
(cutest-nlp-35 TRAIN: SMBANK and ACOPP57 on both lanes, read from the main harness's decoded
directory). No DN-0 population cell was run before this declaration. That check fixed three
instrument parameters and exposed one property of the code:

1. paranoid = 2 needs `--user-callchains`; 64 KB DWARF stacks at a 63-226 k period lost samples
   (sampled sums 2.6x short of `perf stat`), so the period floor is 1000003 and the stack copy
   16 KB with repeats; at 8-64 KB the bucket shares moved by <= 0.01.
2. `pardisoinit` leaves iparm[33] = -1 (CNR off), which the replay maps to 0; afterwards 142/142
   checked matrices matched on every input field.
3. The IPM's process-once `hven::detail::ensure_solver_initialized()` calls MKL `dsecnd()`, whose
   first call calibrates the clock by spinning (`mkl_serv_get_clocks_frequency` / `difftime`):
   ~1-3e9 user instructions, varying run to run with wall time (ACOPP57 IPM: 2.70e9 / 4.09e9 /
   4.10e9 process instructions in three runs; SQP: 768.6e6 +- 1e4). This is why the secondary
   reading is registered, and why IPM totals carry a wall-clock-dependent term (median of three).

## 7. Failure and successor

A leg FAILS (successor declaration, failed record kept) if: the roster or pre-flight refuses; a
roster cell has no `.result` for its leg; a S1 `.stat` lacks an instructions count; a cell has no
attribution JSON after P; or any replayed matrix fails the iparm identity check. Lost samples are
reported per cell and are not a failure. A cell's solver outcome (error, infeasible, capped) is a
measurement, never a failure.

## 8. Carried caveats (rows are kept, flagged in `cells.csv` `caveat`)

- CUTEst derivative-check failures: checker stencil leaves the box (ALLINITC, FLETCHER, HS30,
  HS31, S365, S365MOD); undiagnosed mismatches (DEMBO7, HS67, HS70, HS85).
- hven-sqp LOOTSMA: infeasible at 2 iterations, a registered false-infeasibility candidate;
  recorded, not fixed here.
- Platform: every figure is MKL Pardiso on Linux x86-64. Apple/Accelerate (and the
  `lapacke_shim.h` dense route there) is **UNOBSERVED**.
- Wall-clock is informational and never quoted as a measurement.

## 9. Outputs

`results/cells.csv` (bucket table per cell, totals, rho variants, agreement, caveats),
`results/rule.json` and `results/rule.txt` (the four median cells per reading, every condition,
the outcome), per-cell raw summaries under `raw/` (stat files, attribution JSON, replay and verify
CSVs, dump `index.tsv`), `REPORT.md`, `PROVENANCE.txt`. perf data, perf script text and the
binary matrix dumps stay in the harness's git-ignored `.scratch/dn0/run/` and are not committed
(they are re-derivable from the pinned binaries).
