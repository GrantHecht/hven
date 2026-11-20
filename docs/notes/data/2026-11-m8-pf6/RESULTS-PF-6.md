# RESULTS-PF-6 -- the MKL threading layer

Declaration: `DECLARATION-PF-6.md` (committed `7fcc9cf51` before any leg). Library code commit `2688aa5b0` (CMake/docs/CI only). A test-only
commit follows (section 2). Box: two-lane `box-lane`, co-run protocol as declared. Toolchain clang 22.1.8, MKL oneMKL 2026.2, AMD Ryzen 7 5800X3D, 2026-11-20.

## 1. C1 -- the default is unchanged (configure-time proof)

`configure/`: m8 `331b31837` vs pf6 default arm. Link line of `hven_sqp_corpus` and the compile command of `symmetric_factor_mkl.cpp`
byte-identical after path normalization (`link-base.txt = link-int.txt`, `compile-base.txt = compile-int.txt`). Sequential vs default differs only in:
`libmkl_sequential.a` for `libmkl_intel_thread.a`, no `libiomp5.so`, no compiler lib dir in the rpath, no `-fopenmp=libiomp5` compile flag.
The sequential `hven_bench_cell` has no libiomp5 in NEEDED (`readelf -d`).

## 2. C2 -- ctest (Release, 3954 tests, -j4)

| arm | result |
|---|---|
| intel (default) | 100% passed, 0 failed (`out/ctest/int.log`; re-run after the test-only change `int-r2.log`: same) |
| sequential, first run (`out/ctest/seq.log`) | 10 failed: every one a test whose PREMISE is that MKL honours a thread-count override (ThreadScope x4, DenseArmThreads x3, DenseSymmetricFactor / SymmetricFactor per-instance override, Threads.TheCallersThreadSettingSurvives...) |
| sequential after the test-only change (`out/ctest/seq-r2.log`) | 100% passed, 0 failed; those 10 SKIP with a stated reason (`tests/support/mkl_threading.h`, `HVEN_TEST_MKL_SEQUENTIAL`) |

The ten failures are the documented consequence of the design (mkl_sequential accepts and ignores `mkl_set_num_threads_local`), not a defect in the layer. They were
not hidden: the skip is the declared handling ("classified, trivially a test assumption"). No test assertion was loosened; nothing else changed in a test.

## 3. C3 -- replay (27 cells x walk/ssn/ipm, MKL_NUM_THREADS=1), intel vs sequential

`out/replay/cmp-int-seq-{walk,ssn,ipm}.txt`: **cells 27, columns 75, differences 0 in all three**, exact (scripts/compare_replay.py, no tolerance). Binaries: build-int / build-seq
`hven_sqp_corpus` (sha256 in `out/legs-logs/replay-*.out`). The default-vs-m8 identity is carried by C1 (identical compile and link lines) rather than a base replay.
Sequential changes no counter, status, or residual on the corpus.

## 4. C4 -- instructions:u, PF-3 TRAIN `full` cells (78), both layers, 2 reps (`out/stat/`, `out/stat-table.md`)

Lane binaries: intel `843671be...`, sequential `5c722cb4...` (full sha in `out/stat/run.log`), hven-bench e03cbad private worktrees, one lane, width 4, pinned physical cpus 4-7, MKL/OMP threads 1.
Asserted and held: status/iterations/factorizations equal across layers and reps in all 78 cells.

| set | lane | cells | intel | sequential | seq/intel |
|---|---|---:|---:|---:|---:|
| HS TRAIN | hven-ipm | 22 | 150,341,455 | 99,825,795 | 0.6640 |
| HS TRAIN | hven-sqp | 22 | 193,061,598 | 142,755,576 | 0.7394 |
| HS TRAIN | both | 44 | 343,403,053 | 242,581,371 | 0.7064 |
| nlp-35 TRAIN | hven-ipm | 19 | 508,039,447,817 | 500,782,538,436 | 0.9857 |
| nlp-35 TRAIN | hven-sqp | 15 | 2,021,071,486,291 | 1,939,708,939,831 | 0.9597 |
| nlp-35 TRAIN | both | 34 | 2,529,110,934,108 | 2,440,491,478,267 | 0.9650 |
| all | | 78 | 2,529,454,337,160 | 2,440,734,059,634 | **0.96493 (-3.51%)** |

Sequential is lower in 78 of 78 cells; per-cell ratio median 0.740, min 0.653, max 0.986; median saving 2.42M instructions per cell (the smallest HS ipm cells: -2.29M, i.e. the
per-process OpenMP bring-up PF-3 measured). The suite saving (-3.5%) matches PF-3's libiomp5-leaf 3.0% plus the fixed bring-up. Deadline-class cells (12) were not run.

**Amendment to the declaration (disclosed, not silent).** The declaration asserted that per-layer totals "reproduce" between the two repetitions. They do not exactly: `intel`
totals vary by up to 1.6e-3 relative between reps (libiomp5 spin counts), `sequential` by up to 2.5e-5. PF-3 had already measured instruction spread up to 1.2e-2 (`2026-11-m8-pf3/out/determinism.txt`), so the
bound applied by `tools/pf6_report.py` is that PF-3 figure; all 78 cells are inside it (0 flagged). Reported ratios use the mean of the two reps. A finding in its own right:
sequential's instruction counts are roughly 60x more reproducible than intel's.

## 5. Findings and caveats

- Mixed-layer link conflict. In the hven-bench build, the IPOPT lane (`libipopt.so` brings its own `libmkl_intel_thread.so`) fails to link against a sequential hven
  (`undefined reference to __kmpc_*`). The hven lane (`hven_bench_cell`) links and runs. Consequence: a process that links hven sequential together with another MKL-threaded
  component must resolve the layer in one place; for the benchmark, the hven lane can be sequential, the IPOPT lane stays on its own build.
- `threads > 1` is ignored under sequential (design). No wall-clock was measured (none asserted); parallel-Pardiso speedup on large problems is the capability given up.
- Windows is covered by construction in FindMKL/hvenConfig (same branches, `libiomp5md.lib` skipped) but unobserved here; the Windows CI lane runs the default.
- CI (default lanes) plus the new Linux sequential install-smoke step: see CI note below.
