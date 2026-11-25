# DECLARATION-PF-6 -- the MKL threading layer (`HVEN_MKL_THREADING = intel | sequential`)

Committed before the first measurement leg. HOLDOUT is never run.

## 1. Pin and change

- Branch pf6, code commit `2688aa5b08896ae74fec8590bc09244fc6e3d4e6`, on m8 `331b31837`. Change: CMake only (`CMakeLists.txt`, `cmake/FindMKL.cmake`,
  `cmake/hvenConfig.cmake.in`), `docs/build.md`, and one CI step. No source under `src/` or `include/`, no `iparm`, no `pardiso_session.*`.
- Default stays `intel`. Redistribution: unchanged by default. Under `sequential` the OpenMP runtime (libiomp5 / libiomp5md.dll) is no longer linked or
  needed; nothing is added to what is redistributed.
- Design: `sequential` links `mkl_sequential`, drops `-fopenmp`, and does not search/link/rpath libiomp5. `threads > 1` is IGNORED (not refused):
  see docs/build.md "MKL threading layer" for the argument (one options struct stays valid on both builds; matches the existing best-effort-absent
  contract and Apple's behaviour; `mkl_set_num_threads_local` is a no-op under mkl_sequential). The installed `hvenConfig.cmake` replays the choice.

## 2. Claims and what refutes them

- C1 (default is unchanged). `configure/` compares the m8 base arm and the pf6 default arm: the link line of `hven_sqp_corpus` and the compile command of
  `symmetric_factor_mkl.cpp` are byte-identical after path normalization (already observed at configure time; `configure/link-*.txt`, `compile-*.txt`).
  Sequential differs from default in exactly: the three MKL archives (`mkl_sequential` for `mkl_intel_thread`, no libiomp5), the rpath (no compiler lib dir)
  and the absence of `-fopenmp=libiomp5`.
- C2 (ctest). Release ctest, `-j$BOX_LANE_JOBS`, passes 100% on both layers. A failure on `sequential` is reported and classified (genuine
  layer dependence vs test defect); it is not fixed in this round unless trivially a test assumption, and is then declared.
- C3 (replay). 27-cell corpus replay, walk/ssn/ipm, `MKL_NUM_THREADS=1`, binary of each layer (`scripts/compare_replay.py`, instruments from
  `2026-11-m8-i17/legs/replay.sh`): 0 differences, exact, in all three modes between the layers. Anything else is reported as a finding (a
  layer-dependent numeric path, e.g. iparm[1]=3 under sequential).
- C4 (instructions). `perf stat -e instructions:u` per cell on the PF-3 TRAIN rosters (`2026-11-m8-pf3/cells.csv`, the 78 `full`-mode cells: HS TRAIN 22 x
  {ipm,sqp} + nlp-35 TRAIN, minus the 12 deadline-class cells, which are not asserted and not run). Both layers, 2 repetitions each. Asserted: status /
  iterations / factorizations equal between layers and between repetitions; per-layer instruction totals reproduce between the two repetitions
  (otherwise the cell is flagged non-deterministic and excluded from sums). Reported: per-cell and suite B/A for `sequential` over `intel`, and the
  smallest cells (per-process bring-up). Expected from PF-3: sequential lower, by about the libiomp5 3.0% plus ~2M instructions per cell.
- Wall-clock is informational and is not claimed.

## 3. Instruments

- Lane builds: `legs/lane_install.sh` builds pf6 HEAD with the pin build's flags (Release, clang++, tests/bench OFF, LTO OFF) plus the layer, installs into a
  private hven-bench worktree (`/home/ghecht/Projects/hven-bench-pf6-<layer>` at main e03cbad; its `cmake/hven_pin.cmake` carries a local, uncommitted edit
  naming the pf6 commit), then builds `hven_bench_cell` there. hven-bench main is not touched. Binaries' sha256 recorded in the leg logs.
- `tools/pf6_stat.py` (derived from PF-3's pf3_cell.py/pf3_corun.py pass A; argv identical).
- ctest/replay: `legs/{build,ctest,replay}.sh` (from I17), executables built Release, flags `-DHVEN_FP_MODE=SAFER_FAST`, bench ON.

## 4. Co-run protocol (CLAUDE.md section 7, as amended by the two-lane box rule)

The instruction leg asserts only deterministic columns (instruction totals, counters, statuses) at `MKL_NUM_THREADS=1` / `OMP_NUM_THREADS=1`, so it co-runs:
one `box-lane` lane (4 physical CPUs, `$BOX_LANE_PHYS`), width 4, one process per pinned PHYSICAL cpu, SMT siblings idle, `taskset`-pinned, job order fixed
before the run (longest-first by the PF-3 baseline wall, layer-interleaved per cell). Replay and ctest run inside a lane with the same thread pins. No leg here
asserts wall-clock, so none is run `--exclusive`.

## 5. Recommendation scope

The recommendation on flipping the default for standalone/benchmark builds is made from C2-C4 and is not acted on in this round.
