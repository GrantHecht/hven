# M8 WA-1 r5 -- the footprint rule reworked to the review's bar (DECLARATION, successor of DECLARATION-WA1-r4.md)

Committed before the first r5 leg; never edited afterwards. Every earlier declaration and output is kept. A failed or void r5 leg
is answered by `DECLARATION-WA1-r6.md`.

## 0. Why a successor

The Fable review of the r3/r4 range returned SHIP AFTER FIXES (C1, C2 critical; I1-I5; minors). The code is reworked; the r1-r4
rule (the merit-capped `-p` move) is withdrawn. Every measurement below is on the new range, rebased onto `origin/m8` after RS-6a
(one test executable per tests/ directory; the new test file is registered in `hven_qp_seam_tests`).

## 1. The change, the code, the binaries

- **Code commit `0811f4cf6d1e9d017927e55878e5006308b0665c`** on **`origin/m8` `b02e88eb43b6b3323cdb4ecb1e6e2a72dae63d98`** (arm A, worktree `../wa1-base`). Code paths
  `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg HEAD must equal it (`legs/orchestrate_r5.sh`).
- The rule (`walk_engine.h` 4c, THE REGULARIZATION FOOTPRINT; a stopgap for the registered P-1a round): for a refined EQP step p
  that rises with verified negative curvature from an iterate on its rows and fails the null-space test, with dual_mu > 0 and a
  verdict that is not kSuspect --
  * trusted kWrong: ride -p to its first blocker when the ride moves and the landing point keeps every working row within
    `kFootprintRowBudget = 0.5` of its multiplier-free floor `feasibility_tol * row_scale` (equalities two-sided, working
    inequalities on the infeasible side, scale at the landing point); else one dual_mu rung (shared `kMaxDualRegDecreases`); a
    rung whose face then reads kSuspect is reverted once and ends footprint rungs for the solve; with the rungs spent or ended the
    ordinary step stands (counted). Neither moving: a degenerate pivot (the ordinary add).
  * kOk: when the penalty merit f + |A_W x - b_W|^2 / (2 mu) rises along p beyond its rounding resolution, the ordinary step along
    `EqpResult::x_regularized` (the regularized answer before refinement and polish), verified to descend the merit; else the
    ordinary step along p (counted).
  * No `-p` move. The pins check is removed (p holds pins exactly by construction). Counters
    `QpCounters::ascent_footprint_{rides,regularized_steps,rungs,rung_reverts,declines}`; trace `QpWalkFootprint`,
    `QpWalkAction::kFootprintRung`.
- Corpus binaries (`build-wa/bin/{A,B}/hven_sqp_corpus`): A `48f8e5e246d0b5cf359d27488f73dc849c98b65e71c91f24e1916d2a031a2ee8`, B `f158012ca08b481ab6759c185144c97006c4d0e3d22b2c64155e1cd493b6eb53`.
- Lane binaries (`build-wa/bin/{A,B}/hven_bench_cell`; private clone `.scratch/hb`, branches `wa1r5-A` / `wa1r5-B`): A `5f89a12e505cd92aaf6f2d0ad7a9b377aaa8894573234a253cfe19adad9cbc51`,
  B `555d49dd78eb397aed0c25e00d301c6af39bfd17c3fd939347ff2fff1fed0f08`. Earlier binaries: `build-wa/bin-r3/` (r3, plus the measurement lanes M and N), `build-wa/bin-r5dev1/` (a superseded
  r5 dev round, before the rung revert).

## 2. Disclosures (none is a leg)

- **C1 measured on the r1-r4 code** (`r5/c1-train.txt`, `r5/c1_summary.py`; scratch branch `wa1-meas` 2f9ad0e67, env-gated
  prints, never shipped): TRAIN 1472 footprint moves in 9 cells; of 724 move chains on an unchanged working set, **722 end in an
  ascending ordinary step** (MSS1.D 371/371, MSS1.K 276/278, MESH.D 69/69 with chains up to 12 moves), 1 in a ride, 1 other. The
  qp26 fixture took no move on the current base (`r5/c1-qp26-fixture.txt`).
- **Design variants on TRAIN** (`r5/design-*.txt`; scratch `wa1-meas2` 0233d36df with a no-rung toggle): vs arm A, the rung variant
  +0.16% qp minors / +0.75% factorizations, CRESC100.K infeasible -> optimal; without the rung +4.79% / +2.82% (1214 counted
  declines vs 292); the r1-r4 code +2.54% / +1.53%. The rung was kept on that evidence. The rung revert (added after Debug qp25 read
  kNumericalError on rung-induced kSuspect faces) moves no TRAIN row.
- **Dev on the final range** (`r5/dev/`, raw rows `.scratch/r5/`): ctest Release and Debug "100% tests passed, 0 tests failed out
  of 3922"; replay A vs B 0 differences in walk, ssn, ipm; TRAIN 476 rows, 0 missing/capped, 9 movers (1 BETTER, 8 NEUTRAL, 0
  WORSE); totals qp minors 209264 -> 209591 (+0.16%), factorizations 249651 -> 251535 (+0.75%).
- **Mutation pre-flight** (`.scratch/r5/mut-pre3`, `mut-pre4`): control clean; N1-N16 each killed.
- **qp26 end to end** under the landed sparse-equil-v4 platform retake: `QpPlatformRetake.YorknetRestorationQp26EscalatesToTheMatchedRetake`
  kOptimal in 23 minors on both algebras (Release), one scaled and one matched retake; the footprint rule does not fire there. The
  pass-42 fixture: kOptimal, f = -1458.316029, 47 minors, one kWrong ride and one kOk regularized step, no ordinary step from an
  iterate on its rows rises.
- CI on the code commit: run 44579321329 dispatched before this file (windows and macos green when written).

## 3. Expected movers (declared)

- **Tests**: `tests/qp/test_qp_ascent_footprint.cpp` rewritten (14 tests: 3 helper, 9 synthetic, 2 qp26). `QpYrkRefusedExchange`
  qp25's Release pin `kQp25Release` **re-derived -1398.63412864 -> -5.79957813652** (a declared pin move: both values are the
  fixture's known certified points, the test already records them as route alternatives; the rule rides down where the walk had
  stepped up, and the walk certifies the other point -- kOptimal, battery and second-order face as before). `QpW1AscentGuard`'s
  ride tally reads "not down" past f's rounding floor (run 44567275588: macOS recorded one ride whose decrease sat below it).
- **Replay**: none (`r5/movers-replay.txt`).
- **TRAIN**: `r5/movers-train.txt` -- CRESC100.K BETTER; HS101.D, HS101.K, HS56.D, MESH.D, MSS1.D, MSS1.K, SPANHYD.D, SPANHYD.K
  NEUTRAL. **Cost bound**: TRAIN totals within +3% qp minors and +3% factorizations of arm A; no WORSE mover; every row optimal on
  both arms within +1% of its qp minors (dev: HS101.D -6, HS101.K -7, HS56.D +1).

## 4. The legs and their criteria (fixed here, before data), outputs under `r5/`

| leg | criterion |
|---|---|
| P1 | ctest Release and Debug: each log reads "100% tests passed, 0 tests failed out of 3922" |
| P2 | replay 3 x walk/ssn/ipm vs arm A's r5 dev rows (exactly the declared movers) and vs B's r5 dev rows (0 differences) |
| P3 | TRAIN 238 x D/K serial: `I4_PASS` against `r5/movers-train.txt`; no WORSE mover |
| P4 | `runtime/run_walk.sh` into `r5/runtime/` (walk, serial, solo): THROTTLE 0; counter identity; filtered B/A in [0.995, 1.005] |
| P5 | `legs/mutations_r5.sh`: control clean; N1-N16 each KILLED (MUTATIONS_AS_DECLARED); tree restored |
| P6 | `legs/install_smoke.sh`: install smoke and export contract rc 0, default (GCC) and clang++ front ends |

HOLDOUT is never run. The orchestrator stops at the first failed leg.

## 5. Order

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-wa1/legs/orchestrate_r5.sh 0811f4cf6d1e9d017927e55878e5006308b0665c 48f8e5e246d0b5cf359d27488f73dc849c98b65e71c91f24e1916d2a031a2ee8 f158012ca08b481ab6759c185144c97006c4d0e3d22b2c64155e1cd493b6eb53 5f89a12e505cd92aaf6f2d0ad7a9b377aaa8894573234a253cfe19adad9cbc51 555d49dd78eb397aed0c25e00d301c6af39bfd17c3fd939347ff2fff1fed0f08 3922 > docs/notes/data/2026-11-m8-wa1/r5/orchestrate.out 2>&1
```

## 6. CI

Push `wa-1`, `gh workflow run ci.yml --ref wa-1`, all four lanes green; recorded in `RESULTS-r5.md`. Not a leg.
