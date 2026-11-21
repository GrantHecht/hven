# DECLARATION-PF-2d -- three levers on the QP independence detector (L1 first-solve reuse, L2 re-read band, L3 first-solve decision), then the PF-2 re-measure

Written and committed before any gate or measurement leg; never edited afterwards (a failed or void leg is answered by a `-r1`
file, this one kept). Criteria are never changed after data. Evidence base for the levers: `../2026-11-m8-pf2b/SIZING-PF-2c.md`
(pf2c `de02a46a3`): measured counts on eight f7 cells, nothing implemented there. Base: pf2b `e8d2441a3` (code `ae2e6729f`).

## 1. What is built

All three act in `QpEngine::test_candidate` / `dependency_test_to` (`src/qp/walk/walk_engine.cpp`, `ws_independence.cpp`).

- **L1 (commit 1; bit-identical by construction).** The certificate re-read's first backend solve (same factor, same right-hand
  side) duplicates the test's own first solve. A converged "independent" test keeps its first solve (`DepTest::first_y`,
  `first_res`, `first_res_norm`, `has_first`); `certificate_coefficients(..., &t)` resumes from it instead of solving again. The
  logical count `DepTest::solves` (hence `dep_test_solves`) is UNCHANGED (the reused solve is still counted), so every
  `QpCounters` value is identical; only the backend tally's detector solves fall.
- **L2 (commit 2).** The re-read runs only when the test's converged rho lies in (kDepRereadBandLo = kDepTol = 1e-6,
  kDepRereadBandHi = 1e-4] (today: any converged independent with rho < kDepUndecidable = 1e-3). Constants are named in
  `ws_independence.h`, with the measured largest flipping rho 1.55e-5 (SIZING-PF-2c) cited in a one-line comment.
- **L3 (commit 3).** For a non-screen test (`target > 0`, `!screen`), the refinement loop is skipped when the first-solve rho1 >=
  kDepDecideFirstRho = 1e-2: the verdict is independent from the first solve (`stall_admit` = the first solve did not converge,
  as the stalled refinement would have set it; SIZING-PF-2c: 0 verdict changes at 1e-2, 2-14 per cell at 3e-3). The certificate
  mode (`target == 0`) and screen tests are untouched.
- Expected counter effects, declared: L2 and L3 change backend detector solves, `dep_test_solves`, `dep_grey_rereads`, and where
  the test refinement would have converged, `dep_stall_admits` (SIZING-PF-2c: 7-10 tests per tier-A cell). They are not expected to
  move statuses, factorizations or qp_minors on the 8 PF-2 cells; the TRAIN bar below is the test of that on the corpus.
- Tests added with each lever; the YORKNET grey-pin test (rho 7.669e-4, `dep_grey_rereads == 1`) pins the OLD band and is re-derived
  with L2 (its re-read no longer happens, `dep_grey_rereads == 0`, verdict unchanged) -- a declared pin move, Release only (its Debug
  arm is skipped by an existing ruling).

## 2. Arms and binaries (shas appended to `BINARIES.txt` before the first leg; frozen)

- **A** = pf2b `ae2e6729f` code (`e8d2441a3` tree): the pf2b gate's own arm B binaries (`corpus B`, `lane B` of
  `../2026-11-m8-pf2b/BINARIES.txt`, sha256 re-checked on copy).
- **L** = the L1 commit (`git archive` into `.scratch/armL`, rel and dbg builds); **F** = the final head (L1+L2+L3; worktree
  builds `rel`, `dbg`).
- clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON`, Linux x86-64, AMD Ryzen 7 5800X3D, MKL 2026.2,
  `MKL_NUM_THREADS=1`. TRAIN lanes: private hven-bench clone at `e03cbad` pinned to each arm (`legs/lane_build.sh`, `NEVER=1`), as PF-2b.
- Measurement binaries (section 4): `u0` = M6 `19f7674f6` (pf2b `.scratch/build-u0`), `prev` = pf2b's `measB` (ae2e6729f + PF-12 levers),
  `new` = `measD` = `git archive F` + `docs/notes/data/2026-11-m8-pf12/scripts/pf12-lever.patch` (scratch-only, never committed), `off` = the `new` binary with
  `HVEN_PF12_IWS=0`. Apple/Accelerate and Windows: CI only.

## 3. Gate (legs in `legs/`, evidence in `gate/`)

| leg | instrument | criterion |
|---|---|---|
| P1 | `ctest.sh` rel and dbg at L and at F (box-lane lane, `ctest -j4`) | each log reads "100% tests passed, 0 tests failed out of N"; N recorded in `BINARIES.txt` per head before the first leg (rel and dbg equal) |
| P2 | `replay.sh` x3 (27 cells x walk/ssn/ipm) of A, L, F, co-run (one process per pinned physical core of the lane, `MKL_NUM_THREADS=1`; counters, statuses, residuals only) | **L vs A: 0 differences, all nine runs** (`scripts/compare_replay.py` exact). **F vs A: 0 STATUS differences, all nine**; every other difference is listed per cell and column with its cause (reported, not asserted) |
| P3 | `train_enum.sh` x2 (238 x D/K, stat mode, CAP 300 s, co-run: A, L, F on three pinned physical cores of one lane, `MKL_NUM_THREADS=1`) + `train_check.py` + `train_movers.py` | **L vs A**: every row equal on every key except the twelve `side.backend_solve*` keys (movers 0, missing/capped 0; residual keys byte-equal or inside the calibrated gate); asserted `TOTAL hven-sqp` instructions L/A <= 1.000 (a fall). **F vs A**: **0 status movers** (the `status` key; the bar for L2/L3), missing/capped 0; every counter mover reported per row with the keys that moved and the cause; F/A instruction total reported |
| P4 | `mutations.sh` | control clean; every MUST mutation of `legs/mutate.py` killed (`MUTATIONS_AS_DECLARED`); tree restored |
| P5 | `install_smoke.sh` (default and clang front ends) | smoke and export contract rc 0 |
| P6 | the PF-2 re-measure (section 4) | determinism STOP: three rounds identical on status/factorizations/qp_minors and all twelve tally values per arm and cell; u0 rows equal the u0 baseline row; the result is reported against the 1.15 bar, not asserted |
| CI | one dispatch of the four-lane workflow at the pushed branch head `pf2d` (`gh run watch --interval 120`) | green on all four lanes |

Mutations (`legs/mutate.py`, each killed by a named test): L1: R1 reuse disabled, R2 reused residual norm zeroed, R3 the stored first
solve taken after refinement, R4 `has_first` never set. L2: B1 band top 1e-4 -> 1e-3, B2 top -> 1e-5, B3 band bottom 1e-6 -> 1e-5,
B4 `<=` at the top -> `<`, B5 `>` at the bottom -> `>=`, B6 the re-read ignores the band (old `< kDepUndecidable`), B7 the stored
first solve ignores the band. L3: T1 threshold 1e-2 -> 1.0, T2 -> 1e-3, T3 screen tests not excluded, T4 `>=` -> `>`, T5 skip also in
certificate mode, T6 skipped test carries `stall_admit = false`. Killers: `WsIndependenceCalibration.*`, `QpEngine*` in
`hven_qp_tests` (the PF-2d fixtures). A mutation that is equivalent by construction is recorded as such with the argument, not
silently dropped.

## 4. The PF-2 re-measure (RESULTS-PF-2b's instruments, cells and arms, re-taken)

Same eight cells (tier A: `f7_n1000_path_corrupted`, `f7_n1000_path_neutral`, `f7_n800_path_neutral`,
`f7_n825_path_neutral_control`, `f7_n750_path_neutral_control`; tier B context: `f7_n800_path_corrupted`, `f7_n2000_path_activity`,
`f7_n2000_path_physics`), same instrument (`perf stat -e instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u`, one
process per cell `--internal-run-one --engine walk`, untimed warm-up, three rounds, median), same co-run terms (four workers pinned to four
distinct physical cores of one box lane, SMT siblings idle, `MKL_NUM_THREADS=OMP_NUM_THREADS=1`; instructions and counters only, nothing quotable
as timing, never `--exclusive`). Arms `u0`, `prev`, `new`, `off` as section 2, rotating order across rounds; `HVEN_PF12_QP_FAILURE_BUDGET=1`
in every arm. Deliverables: new/U0 and prev/U0 and off/U0 instructions per qp minor per cell against the 1.15 bar (tier A, as PF-2b);
the per-bucket solve table, `prev` vs `new`. Path note carried: a ratio compares per-minor cost, so a cell whose minors differ between arms
is flagged.

## 5. Not run

HOLDOUT (never). The census. Any wall-clock claim. Accelerate and Windows readings. `f7_n10000_path_physics`. Remedies beyond L1-L3 (the
polish predictor of SIZING-PF-2c stays a settler decision).
