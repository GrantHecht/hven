# M8 WA-1 r3 -- the multiplier-free row budget, on origin/m8 c71c0a7d9 (DECLARATION, successor of DECLARATION-WA1-r2.md)

Committed before the first r3 leg; never edited afterwards. `DECLARATION-WA1.md`, `DECLARATION-WA1-r2.md` and every earlier output are
kept unchanged. A failed or void r3 leg is answered by `DECLARATION-WA1-r4.md`.

## 0. Why a successor, and what is withdrawn

- **r2 is withdrawn without a leg.** Its chain was queued on the box lock when its code failed CI again: run **44498064793**
  (r2 head) failed windows-clang-release with exactly run 44480073900's numbers (BRITGAS qp1: 261 ascent rides, 9 drift violations).
  The queued chain was stopped by its owner before it acquired the lock; `r2/` holds only its dev evidence. The r2 range is kept on the
  local branch `wa-1-withdrawn-r2` (never pushed). The 10x budget at the iterate's multipliers never bound on that route.
- **Diagnosis on a throwaway branch** `wa-1-diag` (pushed, CI dispatched twice, then deleted; never merged): a test-only print in
  `QpW1AscentGuard` showed, on Windows, 40 footprint rides (first at minor 1437) and 221 in-face guard rides, 9 drift violations
  (run 44502512280). The multiplier-free budget below, on the same branch, made windows-clang-release green (run 44505376046).
- **The base moved twice more** after run 1: `bc8ed0e12` (RS-4) and now **`c71c0a7d98f41f53e0327ee1507ea16015b56e52`** (RS-5 layering and
  the sparse-equil-v4 platform retake, which touches `walk_engine.cpp`, `eqp_solve.h`, `kkt_factor.*`). The whole WA-1 range was
  rebased onto c71c0a7d9 with no textual conflict, and every r3 measurement below is on that final range.

## 1. The change and the code

The code of the range = run 1's footprint rule + the row budget of `detail::footprint_decision` (r2) with its tolerance now
**multiplier-free**: the alpha along -p at which a working row's residual reaches `kInfeasibilityMarginFactor * feasibility_tol *
row_scale` (`QpEngine::footprint_merit`; the multiplier arguments are gone). The reversed step changes the face, so the footprint
allowance `min(mu |lambda|, cap)` of this face's multipliers is not its to spend; THE EQUALITY DRIFT reads the next face's. Measured
alternatives: no budget (run 1/rebased: Windows 9 drift violations); 10x `row_tolerance` at the iterate's multipliers (r2: same);
the ordinary step's 1x budget at those multipliers (qp25 to a worse local minimum, f -5.80 vs -1398.63). Docs in `walk_engine.h` 4c and
the member comments.

- **Code commit `879d52bd43e13da863f402a47c0c78bba1f6e38a`** (HEAD of the rebased range at this file's parent). Code paths
  `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg HEAD must equal it (`legs/orchestrate_r3.sh`).
- Arm A = `origin/m8` `c71c0a7d98f41f53e0327ee1507ea16015b56e52` (worktree `../wa1-base`). Arm B = this worktree (`build-wa/rel`, `build-wa/dbg`).
  r2's binaries are in `build-wa/bin-r2/`, run 1's in `build-wa/bin-r1/`.
- Corpus binaries (`build-wa/bin/{A,B}`): A `0039d2bc165f955283b1a94e0a60eb69f04acba80b23a1783aa28a036a18ea0a`, B
  `9fb4b726ce6af4df290b1ba9cc86bc1ee2279529b0dc9b320927e4af260a31e9`.
- Lane binaries: A `8347f0604497ade8748578e1346b568063aeb4eb2c134fca92edaab32ae16457`, B
  `9ac2f75c8c24a8eb368e5ea7722b792f1822d8735b1d1fb8de1d0a6004bfdc55` (private clone `.scratch/hb`, branches `wa1r3-A` / `wa1r3-B`, run 1's recipe).

## 2. Disclosures (none is a leg)

- Dev on the final range (`r3/dev/`, raw rows `.scratch/r3/`): Release ctest "100% tests passed, 0 tests failed out of 3916" (arm A's 3909
  + 7); replay A vs B 0 differences in walk, ssn and ipm; TRAIN 476 rows, 0 missing or capped, **8 movers, all NEUTRAL** (run 1's 9 less
  HS56.D, which no longer moves on this base). TRAIN totals B vs A: qp minors 209264 -> 214576 (+2.5%), factorizations 249651 -> 253477
  (+1.5%), iterations 13518 -> 13524; the minors' excess is MSS1 (D +5014, K +1478; numerical_error on both arms).
- Local targeted run of the final code before the rebase (`QpWa1AscentFootprint.*`, `QpW1AscentGuard.*`, `QpYrkRefusedExchange.*`):
  all pass; qp26 fixture kOptimal, 38 minors, f = -1458.316029, eq_drift 0; qp25 f = -1398.63412864.
- New instrument: `legs/orchestrate_r3.sh` (orchestrate_r2.sh with outputs under `r3/` and the r3 dev rows); no other instrument changed
  since r2.
- RS-5's layering test and HeaderSelfContained ctest are in the 3916; no new header is added by WA-1.

## 3. Expected movers (declared)

Tests: +7 (`tests/qp/test_qp_ascent_footprint.cpp`), no existing test changed. Replay: none (`r3/movers-replay.txt`). TRAIN:
`r3/movers-train.txt`, 8 NEUTRAL rows (CRESC100.K, HS101.D/K, MESH.D, MSS1.D/K, SPANHYD.D/K). **Cost bound:** TRAIN totals within
+3% qp minors and +3% factorizations of arm A, no status change, and every row whose status is optimal on both arms within +1% of its
qp minors (dev: HS101.D +1, HS101.K +2); runtime band as below.

## 4. The legs and their criteria (fixed here, before data)

As `DECLARATION-WA1-r2.md` section 4, with outputs under `r3/`, references the r3 dev rows (`.scratch/r3/...`), and:

| leg | criterion |
|---|---|
| P1 | each log reads "100% tests passed, 0 tests failed out of 3916" |
| P2 | 3 x walk/ssn/ipm vs arm A's and B's r3 dev rows: 0 differences |
| P3 | `I4_PASS` against `r3/movers-train.txt`; no WORSE mover |
| P4 | `runtime/run_walk.sh` into `r3/runtime/`: THROTTLE 0; counter identity; filtered B/A in [0.995, 1.005] |
| P5 | control clean; M1, M2, M3, M4, M5, M8 each KILLED; M6, M7 recorded; tree restored |
| P6 | install smoke and export contract rc 0, both front ends |

## 5. Order

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-wa1/legs/orchestrate_r3.sh 879d52bd43e13da863f402a47c0c78bba1f6e38a 0039d2bc165f955283b1a94e0a60eb69f04acba80b23a1783aa28a036a18ea0a 9fb4b726ce6af4df290b1ba9cc86bc1ee2279529b0dc9b320927e4af260a31e9 8347f0604497ade8748578e1346b568063aeb4eb2c134fca92edaab32ae16457 9ac2f75c8c24a8eb368e5ea7722b792f1822d8735b1d1fb8de1d0a6004bfdc55 3916 > docs/notes/data/2026-11-m8-wa1/r3/orchestrate.out 2>&1
```

## 6. CI

After the legs: push `wa-1` (force: the remote holds the withdrawn r2 range), `gh workflow run ci.yml --ref wa-1`, all four lanes; the
Windows lane twice. NOT RUN as `DECLARATION-WA1.md` section 6.
