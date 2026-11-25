# M8 PF-4 -- the Schur border batch's one multi-RHS K0 solve, and the boundary analysis tally (DECLARATION-PF-4)

Committed before the first gate leg; never edited afterwards. A failed or void leg is answered by `DECLARATION-PF-4-r1.md`,
with this file and every output kept. Criteria are never changed after data.

## 0. What this is

Two items from PF-3's ranked opportunities (`../2026-11-m8-pf3/RESULTS-PF-3.md` section 3).

1. **Opportunity 1, built: batch the Schur border stack's K0 solves.** `SchurComplement`'s batch (`begin_batch`/`end_batch`,
   the walk's `sync_borders`) already deferred C's factorization; it now also defers the K0 solves: each `add_border` inside
   a batch only records `(v, d)`, and `end_batch` lands the batch with ONE multi-right-hand-side K0 solve
   (`detail::solve_mat`, `src/kkt/kkt_factor.cpp`, through `SymmetricFactor::solve(ConstMatRef, MatRef)` -- the matrix
   overload both backends already had, implemented in the Apache-2.0 adapters; nothing added to the `hven::linear`
   interface, no session file touched), then each border's row and column of C in add order, then one factorization.
   A batch of one border lands through the per-add solve (a one-column multi-RHS solve measured no saving). A throwing
   batch solve falls back to single solves up to the first add whose own solve throws; the per-add stack and throw are kept
   (`schur_complement.h` BATCHING; fixtures `Schur.BatchLandsWithOneSolveOnEitherArm`,
   `Schur.BatchWhoseK0SolveThrowsLeavesThePerAddStack`).
   **This is a DECLARED NUMERICS EVENT, not a neutral change.** Step 1 (section 2) measured that Pardiso's multi-RHS solve
   does not reproduce k single solves bit for bit (sparse-arm columns differ by up to 1e-3 relative on the profile cells;
   the dense arm's `dsytrs` at the last ulp), so trajectories move. The movers are declared in section 3 with their
   directions; whether the event ships is the settler's decision.
2. **Opportunity 3, probed; the counter built, symbolic reuse NOT built.** `side.symbolic_analyses` counts what its
   `QpCounters` doc says it counts at the call site -- the border/rebuild path's (and the verdict refinement's fresh factor's)
   analysis decisions -- and not the elimination path's EQP factors (`solve_eqp`'s `factorize_checked`), which is where the
   50-296 backend analyses callgrind saw come from (section 2). The first sentence of that doc ("backend SYMBOLIC-ANALYSIS
   calls this solve() call paid for") over-claimed; it is corrected. New, at the boundary: `hven::linear::AnalysisTally`
   (`symmetric_factor.h`, `src/linear/analysis_tally.cpp`), recorded by both platform adapters after each completed
   platform analysis, and `SqpCounters::backend_symbolic_analyses` / `backend_symbolic_repeats` (appended). Reuse was not
   built: the achievable (same-factor) repeat rate is high on two of fourteen profile cells only, and a reuse that swaps a
   retained session back in carries identity and stale-numerics hazards that need their own design round (section 7).
3. **A schema event** rides it: the two counters append two keys to the `sqp.solve.end` counters object (trace v1 goldens
   and the two counters-object goldens re-derived) and two `side.*` lines to every hven-sqp lane row.

## 1. The code and the binaries

- **Code commit `ea71502aa7789f27931e63d1ab6d10fa02b20e8b`** on `origin/m8` `aca3c0afdcd872ad5e90867c2ab1c049dfb717d0` (arm A): three commits, `d0c228d85`
  (the batch, `solve_mat`, the tally, the counters, tests, docs), `204c4444a` (the batch fixture asserts `dim()` inside a
  batch), `ea71502aa` (a batch of one lands through the per-add solve). Code paths
  `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg HEAD must equal it
  (`legs/orchestrate.sh`).
- Trees `.scratch/build-pf4/rel` (Release) and `.scratch/build-pf4/dbg` (Debug) of this worktree; arm A's corpus tree
  `.scratch/build-pf4/armA` from `git archive aca3c0afd` (+ the pinned `dep/` submodule trees). clang 22.1.8,
  `-DHVEN_FP_MODE=SAFER_FAST -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON` (`legs/build.sh`).
- Corpus binaries (frozen, `.scratch/bin/{A,B}/hven_sqp_corpus`): A `20a9fa884605451e10bc81c6f64d5f4a12d27b56fa377eda3a9e62143570cb2c`, B `2b7d173b6959e0372ac9a4c5da51410f049b06aa94e6a4dddce20d202708da18`.
- Lane binaries (frozen, `.scratch/bin/{A,B}/hven_bench_cell`; `legs/lane_build.sh`): a PRIVATE clone `.scratch/hb` of the
  local hven-bench at main `e03cbad` (PF-3's harness) + `legs/hb_patch_never.py` (WA-1's `never-<base>` label) + the pin,
  never pushed. A (pin `aca3c0afd`) `684379f7a1831ae8529f9da6d338baa9332787b66f206720e61c54c2892f2916`; B (pin `ea71502aa778`) `28377676aeef8ffef9b93be3501bce9fa341793843d7a786ac5ad02d72b3bea1`. The clone's decoded CUTEst problems: the 97
  small-dense decodes from `hven-bench-dn1`, the 35 nlp-35 decodes from `hven-bench-pf3` (scratch copies of one decode).

Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, `MKL_NUM_THREADS=1`. Apple/Accelerate and Windows: CI only.

## 2. Disclosures: what ran before this file (none is a leg)

**Step 1, batch size** (`step1/callgrind/`, `tools/pf4_callgrind.py`, `tools/step1_callgrind.sh`; callgrind on PF-3's lane
binary, pin `48e1eeae`, which equals `aca3c0afd`'s library): add_border calls per `sync_borders` call that added at least
one border -- ACOPP57 8.2 (338 / 41; 94 syncs), SMBANK 1.3 (321 / 253; 712 syncs), DALLASM 13.3 (7 529 / 566; 1 254
syncs). Every add_border is called from `sync_borders`.

**Step 1, the microbenchmark** (`step1/probe/`, `step1/summary.txt`, `tools/step1_probe.sh`, `tools/step1_summary.py`): a
scratch probe lane (branch `pf4-probe` `026373ee6`, never pushed) measured, inside every real `sync_borders` batch of the
profile cells, ONE multi-RHS solve of the batch's columns against the k single solves `add_border` took (perf_event
`instructions:u` around each; statuses, iterations and factorizations equal PF-3's on every cell). k >= 2 instructions,
multi/single, and the saving as a share of the cell's PF-3 instruction total:

| cell | batches | mean k | multi/single | saving (% of cell) | columns bit-identical |
|---|---:|---:|---:|---:|---:|
| ACOPP118 | 1 322 | 47.2 | 0.427 | 4.37e+10 (21.2 %) | 0 of 61 728 |
| ACOPP57 | 39 | 7.8 | 0.541 | 7.34e+07 (9.5 %) | 0 of 296 |
| BIGBANK | 23 | 508.0 | 0.429 | 3.27e+10 (16.7 %) | 230 of 11 684 |
| CRESC100 | 16 314 | 46.8 | 0.719 | 5.86e+10 (15.5 %) | 683 687 of 754 969 |
| CRESC50 | 12 547 | 34.7 | 0.574 | 2.36e+10 (25.8 %) | 385 934 of 429 433 |
| DALLASM | 524 | 12.6 | 0.435 | 2.04e+09 (17.3 %) | 0 of 6 192 |
| DNIEPER | 162 | 41.0 | 0.356 | 6.35e+08 (46.9 %) | 2 270 of 6 625 |
| MESH | 4 689 | 25.3 | 0.409 | 1.36e+10 (40.4 %) | 41 942 of 116 107 |
| MSS1 | 3 124 | 10.7 | 0.484 | 1.02e+10 (10.3 %) | 1 383 of 30 530 |
| READING6 | 392 | 36.8 | 0.478 | 3.11e+09 (35.6 %) | 0 of 14 265 |
| SMBANK | 254 | 1.3 | 0.625 | 9.12e+06 (0.7 %) | 0 of 79 |
| SPANHYD | 16 654 | 72.6 | 0.420 | 2.22e+11 (51.1 %) | 0 of 1 209 785 |
| STEENBRC | 538 | 93.5 | 0.547 | 4.05e+10 (11.8 %) | 668 of 49 905 |
| YORKNET | 885 | 34.2 | 0.404 | 2.67e+10 (13.1 %) | 7 of 29 504 |

A one-column multi-RHS solve costs what a single solve costs (CRESC100: 8 331 k = 1 batches, 2.980e9 vs 2.976e9
instructions, every column identical), hence the k = 1 rule. Go: the saving is 10-51 % of the cell on every
add_border-dominated cell. Accelerate's multi-RHS path exists (the same overload); its rounding against single solves is
UNOBSERVED (the `Schur.BatchLandsWithOneSolveOnEitherArm` bit comparison runs on the macOS CI lane, on a 10 x 10 fixture).

**Opportunity 3** (`step1/probe/`, `step1/probe2/`, `step1/summary-probe2.txt`; probe 2 = branch `pf4-probe2` `057f46358`,
never pushed): sparse `SymmetricFactor::analyze` calls, repeats of a pattern hash seen earlier in the process, and (probe
2) repeats on the SAME factor instance:

| cell | analyses | repeats (process) | repeats (same factor) |
|---|---:|---:|---:|
| ACOPP118 | 8950 | 7520 (84 %) | 7491 |
| ACOPP57 | 42 | 11 (26 %) | -- |
| BIGBANK | 2347 | 8 (0 %) | -- |
| CRESC100 | 17092 | 8399 (49 %) | 2842 |
| CRESC50 | 3969 | 1459 (37 %) | 1421 |
| DALLASM | 282 | 9 (3 %) | -- |
| DNIEPER | 3 | 1 (33 %) | -- |
| MESH | 132 | 73 (55 %) | 73 |
| MSS1 | 9371 | 1039 (11 %) | 349 |
| READING6 | 116 | 16 (14 %) | -- |
| SMBANK | 3 | 0 (0 %) | -- |
| SPANHYD | 596 | 337 (57 %) | 27 |
| STEENBRC | 34889 | 3031 (9 %) | 1137 |
| YORKNET | 14520 | 8219 (57 %) | 7697 |

The repeats a per-factor reuse could take concentrate on ACOPP118 (84 % of its analyses) and YORKNET (53 %); BIGBANK, the
cell where analysis is 24 % of the instructions, repeats nothing.

**Dev runs** (raw under `.scratch/dev/`, the summaries kept under `dev/`):
- `d0c228d85` + the anti-cycling re-pin: ctest Release "100% tests passed, 0 tests failed out of 3937" after one
  failure on the first build (`QpE2WorkingSet.AStructuralDependencyDecidesFirstOnTheSteenbrbElasticQp`, section 3).
- `ea71502aa`: ctest Release and Debug "100% tests passed, 0 tests failed out of 3937";
  `scripts/check_accelerate_syntax_linux.sh` PASS (partial, its own scope).
- Replay at `ea71502aa` vs arm A (`dev/replay/`): walk 3 differences, ssn 0, ipm 0 -- the three are residual-class columns
  of one cell, `f7_n800_path_warm` (`kkt_residual` 4.212493723e-10 -> 4.212493600e-10, `kkt_stationarity` the same,
  `kkt_complementarity` 1.295699806e-15 -> 1.295703950e-15); no counter or status moves.
- TRAIN 238 x D/K at `ea71502aa` vs arm A (`dev/train_check.txt`, `dev/train_instr.txt`): 476 rows, 0 missing or capped; 33 movers, all hven-sqp (1 BETTER: CRESC100.K infeasible -> optimal; 32 NEUTRAL by status rank, 0 WORSE); summed over the 238 hven-sqp rows iterations D 2261 -> 2446, K 1852 -> 2084, qp minors D 106747 -> 106517, K 99668 -> 99910, factorizations D 121420 -> 111426, K 107432 -> 103622, optimal rows D 105 -> 105, K 104 -> 105. Instructions (perf stat, `instructions:u`): hven-sqp B/A 0.5912 (D 0.5302, K 0.6544), hven-ipm 1.0000; on the 205 hven-sqp rows whose counters did not move B/A 0.82. The rows at `ea71502aa` equal the `d0c228d85` dev rows (0 differences: the k = 1 rule changes no number).
- nlp-35 (`dev/nlp_check.txt`, `dev/nlp_instr.txt`): 15 rows; 14 move (statuses all unchanged; NEUTRAL by status rank); the A and B rows of two separate dev runs are identical (both arms reproduce across processes). Instructions: total B/A 0.9990 -- 13 cells save 0.4-35 % (BIGBANK 0.834, READING6 0.649, STEENBRC 0.882, YORKNET 0.871), and two cost more: ACOPP118 2.036 (qp minors 10361 -> 33922, iterations 8 -> 14) and STEENBRB 1.052 (iterations 67 -> 107).
- Mutation pre-flight (`dev/mut/`): control clean; B1-B5, B7, T2-T6 KILLED; the first B6 (the land step's
  `factor_pending_`) SURVIVED -- equivalent, the record step already owes the factorization -- and was re-declared as the
  record step's (`dev/mut-b6/`: control clean; B6 KILLED by Schur.BatchWhoseK0SolveThrowsLeavesThePerAddStack); R1, R2 recorded with no killer.
- The criteria tools self-tested on the dev rows: `train_check.py` (dev B against itself with `movers-train.txt`: I4_PASS;
  with the list cut to four rows: I4_FAIL), `nlp_check.py` (dev against itself: NLP_AS_DECLARED; with an empty list:
  NLP_FAIL; against the `d0c228d85` dev run: NLP_AS_DECLARED), `runtime/analyze.py --preflight` on WA-1's fixture: PASS.
- CI on `ea71502aa`, dispatched before this file: run 44712385076 (windows-clang-release, macos-clang-release and linux-clang-release green; linux-clang-coverage still running when this was written -- recorded in RESULTS, not a leg).
- `origin/m8` advanced after the base (RS-6b, `85b8307ce..44313bae4`, tests and Windows only); not rebased this round.

## 3. The expected movers (declared)

**Test suite** (each re-pinned in its test, with the reason):

| test | was | now |
|---|---|---|
| `QpE2WorkingSet.AStructuralDependencyDecidesFirstOnTheSteenbrbElasticQp` (Release, border algebra) | `dep_structural_first >= 1` asserted | recorded (`structural_first_*_since_pf4`): the numerical test no longer admits pin 407 under the batch's rounding, so the premise is lost; kOptimal, KKT battery and zero-motion counters still asserted; the rule stays pinned where it fires (the STEENBRB pass-58 fixture) |
| `TraceV1Fixtures.SqpCompleteStream`, `JsonLinesTraceSink.*` golden counters objects, `kSqpCountersFieldCount` 106 -> 108, `test_callback` / `test_threads` counter tables | -- | two keys appended (schema event) |

Plus the new fixtures: `Schur.BatchLandsWithOneSolveOnEitherArm`, `Schur.BatchWhoseK0SolveThrowsLeavesThePerAddStack`, the
dim() assertion inside `Schur.BatchedAddsLeaveTheStatePerAddWouldHave`, `AnalysisTally.*` (6), `SqpBackendAnalysisTally.*` (2).

**Replay (27 cells x walk / ssn / ipm)**: `movers-replay.txt` -- walk 3 (the three residual-class columns of
`f7_n800_path_warm` above), ssn 0, ipm 0.

**Runtime leg (corpus, walk)**: `movers-runtime.txt` -- `f7_n800_path_warm`'s residual-class columns may differ A vs B.

**TRAIN (238 x D/K)**: `movers-train.txt`, every row vs arm A's dev row:

| row | direction | status A -> B | iterations | qp minors | objective A -> B |
|---|---|---|---|---|---|
| sd-hven-sqp-ACOPP30.D | NEUTRAL | optimal | 11 -> 7 | 615 -> 482 | 576.89242601367209 -> 576.8924260137228 |
| sd-hven-sqp-ACOPP30.K | NEUTRAL | optimal | 11 -> 7 | 615 -> 482 | 576.89242601367209 -> 576.8924260137228 |
| sd-hven-sqp-BT7.K | NEUTRAL | optimal | 97 | 1903 | 360.37976717416603 |
| sd-hven-sqp-CRESC100.D | NEUTRAL | optimal | 40 -> 211 | 23791 -> 26853 | 0.56760270787356015 -> 0.56760271636285275 |
| sd-hven-sqp-CRESC100.K | BETTER | infeasible -> optimal | 25 -> 38 | 18339 -> 15898 | 4.618293723662861e-09 -> 0.56760270972592863 |
| sd-hven-sqp-CRESC4.K | NEUTRAL | optimal | 48 -> 38 | 843 -> 708 | 0.87189754866739555 -> 0.87189755146824099 |
| sd-hven-sqp-CRESC50.D | NEUTRAL | max_iter | 240 -> 259 | 13673 -> 12524 | 0.59346805137766978 -> 0.59341348563251017 |
| sd-hven-sqp-CRESC50.K | NEUTRAL | numerical_error -> max_iter | 138 -> 279 | 11206 -> 14831 | 0.59596698581944807 -> 0.59482354659056114 |
| sd-hven-sqp-DECONVC.K | NEUTRAL | optimal | 126 | 3265 -> 3266 | 2.0788223401773871e-11 -> 2.0788220966684235e-11 |
| sd-hven-sqp-DEMBO7.K | NEUTRAL | optimal | 15 -> 17 | 1340 -> 1368 | 174.78699436474761 -> 174.78699436628204 |
| sd-hven-sqp-HS101.K | NEUTRAL | optimal | 26 -> 28 | 1032 -> 768 | 1809.7647657963535 -> 1809.7647657900543 |
| sd-hven-sqp-HS102.K | NEUTRAL | optimal | 23 | 275 | 911.88057153777993 -> 911.88057153778118 |
| sd-hven-sqp-HS103.K | NEUTRAL | optimal | 21 | 201 | 543.6679585036353 -> 543.66795850371875 |
| sd-hven-sqp-HS104.K | NEUTRAL | optimal | 11 | 59 -> 61 | 3.951163440064283 |
| sd-hven-sqp-HS107.K | NEUTRAL | optimal | 5 | 14 | 5055.0118033914569 -> 5055.011803391455 |
| sd-hven-sqp-HS108.K | NEUTRAL | optimal | 11 | 133 | -0.67498144504677182 |
| sd-hven-sqp-HS117.K | NEUTRAL | optimal | 8 | 80 | 32.348678965723884 -> 32.348678965723877 |
| sd-hven-sqp-HS83.K | NEUTRAL | optimal | 12 | 205 | -30665.538671783321 |
| sd-hven-sqp-HS99EXP.K | NEUTRAL | max_iter | 47 -> 50 | 9404 -> 9478 | -42935256.249999806 -> -42266901.690001324 |
| sd-hven-sqp-LAKES.D | NEUTRAL | numerical_error | 11 | 6388 | 728495951628.13489 |
| sd-hven-sqp-LAKES.K | NEUTRAL | numerical_error | 11 | 6388 | 728495951628.13489 |
| sd-hven-sqp-LAUNCH.K | NEUTRAL | optimal | 10 | 67 | 9.0049050687378216 |
| sd-hven-sqp-MATRIX2.K | NEUTRAL | optimal | 11 | 26 | 2.3841857910156208e-07 -> 2.3841857910156176e-07 |
| sd-hven-sqp-MESH.D | NEUTRAL | max_iter | 273 | 4011 -> 4065 | -8.1453339138274874 |
| sd-hven-sqp-MESH.K | NEUTRAL | infeasible -> numerical_error | 5 -> 93 | 776 -> 1127 | -0.24525331631627351 -> -56.715693901853726 |
| sd-hven-sqp-MSS1.D | NEUTRAL | numerical_error | 11 -> 10 | 9525 -> 4856 | -1.6021852668144545 -> -8.0838825476377352 |
| sd-hven-sqp-MSS1.K | NEUTRAL | numerical_error | 31 | 11116 -> 11519 | -15.12934489281842 -> -15.004146919469283 |
| sd-hven-sqp-NET1.D | NEUTRAL | optimal | 13 | 787 -> 853 | 941194.33167399233 |
| sd-hven-sqp-NET1.K | NEUTRAL | optimal | 13 | 846 -> 784 | 941194.33167399233 |
| sd-hven-sqp-OPTPRLOC.K | NEUTRAL | optimal | 40 | 1567 -> 1561 | -16.419773828930577 |
| sd-hven-sqp-SPANHYD.D | NEUTRAL | numerical_error | 32 | 18208 -> 20747 | 21686964497.356411 -> 21683781263.755222 |
| sd-hven-sqp-SPANHYD.K | NEUTRAL | numerical_error | 23 -> 22 | 20306 -> 19105 | 21850262140.24538 -> 21889522943.996796 |
| sd-hven-sqp-WATER.K | NEUTRAL | optimal | 48 -> 46 | 1602 | 10549.379466956936 -> 10549.379466956942 |

**nlp-35 (15 hven-sqp cells, `default`)**: `movers-nlp.txt`:

| row | direction | status | iterations | qp minors | objective A -> B | instr B/A |
|---|---|---|---|---|---|---|
| nlp-hven-sqp-ACOPP118.D | NEUTRAL | optimal | 8 -> 14 | 10361 -> 33922 | 129660.68619604065 -> 129660.68619604249 | 2.0359 |
| nlp-hven-sqp-ACOPP57.D | NEUTRAL | optimal | 5 | 85 | 41737.786849846176 -> 41737.786849846256 | 0.9246 |
| nlp-hven-sqp-ACOPR57.D | NEUTRAL | optimal | 16 | 2296 -> 2268 | 41737.786035029698 -> 41737.786035029734 | 0.9814 |
| nlp-hven-sqp-DALLASM.D | NEUTRAL | optimal | 18 | 759 -> 787 | -48198.188192052978 | 0.9047 |
| nlp-hven-sqp-READING6.D | NEUTRAL | optimal | 8 | 520 -> 521 | -144.65970057278301 | 0.6492 |
| nlp-hven-sqp-SAWPATH.D | NEUTRAL | optimal | 9 | 334 -> 330 | 750.45894675975114 -> 750.45894675677539 | 0.9924 |
| nlp-hven-sqp-SMBANK.D | NEUTRAL | optimal | 29 | 357 | -7129292 | 0.9959 |
| nlp-hven-sqp-STEENBRB.D | NEUTRAL | optimal | 67 -> 107 | 19099 -> 19356 | 9086.1898281220529 -> 9203.7988198051571 | 1.0519 |
| nlp-hven-sqp-STEENBRC.D | NEUTRAL | optimal | 88 | 27973 | 27504.936820531504 | 0.8816 |
| nlp-hven-sqp-STEENBRD.D | NEUTRAL | optimal | 85 -> 107 | 21336 -> 19836 | 9030.0818667716558 -> 9074.1942666909217 | 0.8863 |
| nlp-hven-sqp-STEENBRE.D | NEUTRAL | optimal | 80 | 14948 | 27459.163311770888 | 0.8329 |
| nlp-hven-sqp-STEENBRG.D | NEUTRAL | optimal | 72 | 23432 | 27420.929665048159 | 0.8422 |
| nlp-hven-sqp-YORKNET.D | NEUTRAL | numerical_error | 6 | 13391 -> 13409 | 53368.040358179773 | 0.8706 |
| nlp-hven-sqp-ZAMB2-8.D | NEUTRAL | optimal | 8 | 551 -> 546 | -0.15293592839679387 -> -0.15293592839679393 | 0.8871 |

## 4. The legs and their criteria (fixed here, before data); outputs under `gate/`

| leg | instrument (`legs/`) | criterion |
|---|---|---|
| P1 | `ctest.sh` rel, dbg | each log reads "100% tests passed, 0 tests failed out of 3937" |
| P2 | `replay.sh` x3 | each run and mode: vs arm A's dev rows (`.scratch/dev/replay/A-k1-*`) exactly the declared difference count (walk 3, ssn 0, ipm 0); vs arm B's dev rows 0 differences (`scripts/compare_replay.py`, exact) |
| P3 | `train_enum.sh` (MODE stat) + `train_check.py` + `instr_compare.py` | `I4_PASS`: every one of the 476 rows equals its dev B row, the set of rows differing from arm A's dev rows is exactly `movers-train.txt`, no row missing or capped, no WORSE mover; `TOTAL hven-ipm` B/A within [0.995, 1.005] and `TOTAL hven-sqp` B/A <= 1.005 (`gate/train/instr.txt`, against arm A's dev .stat files) |
| P4a | `../runtime/run_walk.sh` | the corpus runtime gate, walk mode, serial and solo, 3 rounds (A->B, B->A, A->B): THROTTLE 0; the stop condition of `runtime/analyze.py` (A vs B equal but the declared residual-class columns of `movers-runtime.txt`; each arm's three rounds equal in every column); filtered B/A <= 1.005 |
| P4b | `nlp_instr.sh` + `nlp_check.py` | `NLP_AS_DECLARED`: arm A and arm B rows equal their dev rows, the A-vs-B movers are exactly `movers-nlp.txt`, none WORSE; the instruction totals are reported (`gate/nlp/instr.txt`), with no threshold: the trajectories move (section 3) |
| P5 | `mutations.sh` | control clean; B1-B7, T2-T6 each KILLED (`MUTATIONS_AS_DECLARED`); R1, R2 recorded; the tree restored |
| P6 | `install_smoke.sh` | install smoke and export contract rc 0 on the default (GCC) and clang++ front ends |

Mutations (`legs/mutate.py`): B1 the batch solve never used; B2 the batch solve's throw swallowed; B3 the batch landed in
reverse order; B4 every border landed with column 0; B5 the old evidence readable inside a batch; B6 a recorded add owes no
factorization; B7 `dim()` blind to recorded adds; T2 the MKL adapter records no analysis; T3 repeats never counted; T4 the
ordering left out of the key; T5 only the innermost tally sees an analysis; T6 the count never reaches the SQP result.
Recorded, not must-kill: R1 `solve_mat`'s scaled-retake unscaling dropped, R2 its matched-retake arm solving on the platform
factor (no fixture lands a border batch on retake numerics; `QpPlatformRetake.*` is run for them).

Every mover of every leg is reported with its direction in `RESULTS-PF-4.md`. The instruction comparison is the asserted
perf evidence (CLAUDE.md section 7); no wall-clock is asserted, and none is quoted.

Protocols. Every leg: `pgrep -x hven_bench_cell` empty first, inside one `setsid -w flock /tmp/box-build.lock`
acquisition, `set -o pipefail`, FOREGROUND_START/END markers, `MKL_NUM_THREADS=1`, `TMPDIR` in the worktree, binary
sha256 checked before the first cell. P1: `ctest -j4` under `setsid -w`. P2, P3, P4a, P4b: serial, one process at a time
(P3 and P4b through `cell.sh`, THREADS=1, core 2, CAP 300 s).

## 5. Order, STOP and ON FAIL

A pre-gate build (not a leg): `ninja` on `rel` and `dbg` at the code commit (relinks after the mutation pre-flight; no source
differs), then:

```
setsid -w flock /tmp/box-build.lock bash docs/notes/data/2026-11-m8-pf4/legs/orchestrate.sh ea71502aa7789f27931e63d1ab6d10fa02b20e8b 20a9fa884605451e10bc81c6f64d5f4a12d27b56fa377eda3a9e62143570cb2c 2b7d173b6959e0372ac9a4c5da51410f049b06aa94e6a4dddce20d202708da18 684379f7a1831ae8529f9da6d338baa9332787b66f206720e61c54c2892f2916 28377676aeef8ffef9b93be3501bce9fa341793843d7a786ac5ad02d72b3bea1 3937 > docs/notes/data/2026-11-m8-pf4/gate/orchestrate.out 2>&1
```

Legs in order: P1 (rel, dbg), P2, P3, P4a, P4b, P5, P6. The chain stops at the first failed leg: commit the evidence,
report; no re-run, no criterion change after data; `DECLARATION-PF-4-r1.md` after a fix, with this record kept.

## 6. NOT RUN

HOLDOUT (never). The census (about 6 h; directional at best for a numerics event). Any wall-clock leg (PF-3's solo wall leg
is still pending; wall times printed by cell.sh are informational, co-run-free but not quoted). The iparm dump: no `iparm`
value, no session file and no MKL integration is touched (the multi-RHS path passes `nrhs` to the same phase-33 call).
Apple/Accelerate and Windows: CI only; every Accelerate reading of the batch's rounding is UNOBSERVED.

## 7. Registered (for the settler's ruling)

- **The numerics event itself**: ship, or keep the batch per-add. Section 3's movers carry the costs: on TRAIN, CRESC100.D (optimal, iterations 40 -> 211), CRESC50.K (numerical_error -> max_iter, iterations 138 -> 279), MESH.K (a 5-iteration kInfeasible -> kNumericalError at 93 iterations, objective -0.245 -> -56.7: a failure exchanged for a failure, the old one a definite claim), MSS1.D/SPANHYD.D/.K (failures that stay failures); on nlp-35, ACOPP118 (x2.04 instructions, minors x3.3), and STEENBRB and STEENBRD reaching kOptimal at a HIGHER local objective (9086.19 -> 9203.80, 9030.08 -> 9074.19). Against them: CRESC100.K becomes optimal, TRAIN hven-sqp instructions -41 %, nlp-35 -0.1 % in total (-11 to -35 % on 12 of 15 cells).
- **Symbolic reuse**: a per-`KktFactor` cache of retained analyzed factors keyed on the pattern hash would take ACOPP118's
  and YORKNET's same-factor repeats (84 % / 53 % of their analyses; analysis is about 20 % of those cells). Hazards a design
  round must answer: a retained session swapped back in carries its OLD numerics and (session, epoch) until it is
  refactorized, so a failed refactorization must not leave it solvable or let an `EliminatedFace` / hot-start record match
  it; `DenseTallyScope` reads the factor's own counters across the factorization; retained Pardiso memory; bit identity of
  a refactorization into a retained session against a fresh analysis is unproven; Accelerate parity.
- `side.symbolic_analyses` keeps its border/rebuild meaning; consumers wanting every analysis read
  `backend_symbolic_analyses` (hven-bench picks it up through `dump_counters`; the trace reader's v1 fixtures move).
- Rebase onto `origin/m8` (RS-6b) before merge.
