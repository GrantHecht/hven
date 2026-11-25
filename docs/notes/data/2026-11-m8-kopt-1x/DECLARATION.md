# M8 kopt-1x -- the QP walk's kOptimal row contract (DECLARATION)

Committed before the first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by
`DECLARATION-r2.md`, with this file and every output kept.

## 0. What this is

A declared re-pin event that changes the QP walk's certification (CLAUDE.md section 7). Ruling (settler): `kOptimal` promises every
equality row and working inequality row within 1x of `row_tolerance(row_scale, lambda)` (the `dual_mu * |lambda|` footprint
included); the 10x `kInfeasibilityMarginFactor` stays only as the `kInfeasible` evidence margin. Cause: the KKT-point test reused the
classifier's 10x predicate, so a row 1x-10x off was certified where it stood and `polish_footprint_open` (complementarity or
stationarity only) never opened (YORKNET qp5, Intel AVX-512 CI, row 246 at 6.88x).

The change (`src/qp/qp_engine.cpp`, `docs/migration-0.2.md` has the rationale):

1. `polish_footprint_open` third disjunct: any equality or working row past `row_tolerance` (`worst_row_ratio > 1`); a row-triggered
   entry floors the acceptance test's stationarity comparison (relative factor 4) at `kTerminalPolishStatFactor * opt_tol`.
2. A stationary pass with nothing to drop and a row past 1x takes the ordinary step before certifying ("the gray hold", one
   consecutive hold at most), at the terminal site. **Deviation from the ruling's wording, measured:** the ruling asks that
   `stationary_proper` itself require rows within 1x. Implemented that way (per-pass), the stagnation reading (which is defined
   as `!stationary_proper`) fired on gray points and then moving the 1x into the per-pass test changed mid-walk drop decisions:
   ACOPP118 qp1 (border) ran to the iteration cap (4710 minors, f 17306 against 1172.358, status kIterationLimit where the base is
   kOptimal) and ACOPP118 qp3 took 2000 minors (400 at the base). The terminal-site hold keeps the per-pass test and the
   stagnation reading at the classifier's 10x and applies the 1x only where the walk would certify; those two fixtures and the
   rest of the walk suites then pass unchanged (section 2).
3. New counter `optimal_row_gray_exits` (QpCounters, SqpCounters, trace key after `dense_factorizations`).
4. Callers of `worst_structural_violation` / `violation_is_structural` (all KEEP 10x, none moved to 1x): the per-pass
   `stationary_proper` (KKT-point recognition; the 1x is the terminal hold's), the terminal classification and its two re-reads after
   the verdict-site and mu-free refinements (classifier), the equality-drift check (`eq_rows_violated`, a classifier-currency
   invariant), the certificate's bound half (classifier), and `mu_footprint_open`'s read of the same rows. The acceptance sites that
   moved to 1x: the terminal hold, `polish_footprint_open`'s third disjunct, the counter.

## 1. The code and the binaries

- **Code commit `006ccc49663611dc27440989860d1a1f468e4090`** (branch `kopt-1x`, two commits over `origin/m8` `10ec3df92818ac2121523287bb3d439e42d5d5e7` = arm A):
  `86314dcd3` (the change) and `006ccc496` (test only: the HS battery's gray census). `src include tests bench examples bindings
  CMakeLists.txt cmake scripts` of the leg HEAD must equal it (checked by `legs/orchestrate.sh`). Arm B's `libhven.a` is
  byte-identical across the two commits (sha256 `31a5192058dd...`, the pin installs of both).
- Trees `build-k/rel`, `build-k/dbg` (arm B), `../kopt-1x-base/build-k/rel` (arm A, detached worktree at the base), clang 22.1.8,
  `-DHVEN_FP_MODE=SAFER_FAST`, `HVEN_BUILD_BENCH=ON HVEN_BUILD_EXAMPLES=ON` (`legs/build.sh`).
- Corpus binaries (P2, P4), frozen in `build-k/bin/{A,B}`: A `8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78`, B
  `111a4bbbc1e3eef2dd770fa4e0f741e08887ac2ea492d651b58b3484bc595c90`.
- Lane binaries (P3), frozen: A `893c5167803db46e907bab28228a6a601117aca9603771dae6cccd15d6d05d48`, B
  `39e08bce607540430a0b882dcb92d1c438799cf72d964fc606e4617f216bf348`, built by `legs/train_setup.sh` in a PRIVATE clone of the local
  hven-bench clone (`.scratch/hb`, git-ignored, never pushed) from lane commit `dn3-flip-legs` `adcb7d2` (DN-3's patches g, h, i, j;
  the `default` / `never-default` labels) with only the pin moved to the arm's hven sha (`legs/cell.sh` is DN-3's `sl/cell.sh` with
  its clone path changed).

Linux x86-64, AMD Ryzen 7 5800X3D, MKL 2026.2, `MKL_NUM_THREADS=1`. Apple/Accelerate: UNOBSERVED here; Windows/macOS/Linux CI rows are in
the results file.

## 2. Disclosures: what ran before this file (none is a leg)

- `dep/eigen`, `dep/fmt` copied from the main checkout's trees (submodule update refused); both worktrees.
- Arm B's corpus binary and arm A's: arm A's was built with `GIT_CEILING_DIRECTORIES` set in the build environment, so its bench stamp reads
  `unknown`; no leg asserts the stamp for A. Arm B's stamp: `check_bench_stamp: OK (006ccc496636)`.
- Dev runs on the frozen binaries (outputs under `dev/`, raw cells in `.scratch/`, summarised): replay once per mode, A and B, walk / ssn /
  ipm, 27 cells x 75 columns: **0 differences** in all three (`dev/replay-compare-*.txt`). TRAIN once per arm, 238 cells x {default, never-default}:
  arm A 0 missing; arm B 0 missing or capped; **14 movers, all NEUTRAL** (section 3).
- Dev suites: Release and Debug "100% tests passed, 0 tests failed out of 3734" on `006ccc49663611dc27440989860d1a1f468e4090`. Before the terminal-site hold (the per-pass
  variant, section 0 item 2) three tests failed in Release (ACOPP118 qp3 retreat fixture, the F7 release-readmission fixture whose
  re-admission rung moved 313 -> 322, HsSweepCorpus); the first two pass unchanged under the final design.
- The first dev TRAIN B run was voided (the decoded CUTEst sources were missing from the private clone: 238 rows without a status);
  the sources were copied in and both arms re-run.
- No 57-cell census was taken (about 6 h serial-tiered; the walk-cap record's census is the reference for a later one), no interior leg
  (the interior-point engine reaches neither the walk nor the SQP driver), no install smoke, no TSan: nothing here touches an install
  surface or a concurrent path beyond one trailing counter.

## 3. The expected movers (declared)

**Test suite** (arm B vs arm A, 3731 -> 3736 registered, 3734 non-disabled in B): +5 new tests (`QpRowContract.*`,
`QpRowContractPolish.*` x3, `QpCertificateSign.AnOptimalExitPastOneFloorUnitIsCountedAGrayExit`); re-derived: golden trace lines and
counter-aggregate pins for the one new key; `QpRankRepair` eq contract 10x -> 1x; `HsSweepCorpus.Corpus`: HS10 warm 28 / 42 -> 26 / 40 in
Release (Debug unchanged at 26 / 40), total minors Release 3355 -> 3359, Debug 3346 -> 3355, every status unchanged; the new gray census
(Release 4 on HS15's warm arm, one in each of its four lever cells; Debug 0).

**Replay (27 cells x walk / ssn / ipm):** none.

**TRAIN (238 cells x D/K), `legs/movers-train.txt`:** 14 rows, all NEUTRAL by the status-rank rule (no solved cell becomes unsolved):
ACOPP30 D,K (optimal both; iterations 7 -> 11); CRESC100 D (optimal both; qp minors down); CRESC4 D,K (optimal both; iterations 65 -> 26,
48 -> 26); **CRESC50 K (numerical_error -> max_iter: both unsolved; called out, the status label moves)**; HS84 D,K (optimal;
+1 qp minor, terminal_polish_adopted 1 -> 0); HS92 D,K (infeasible both; +1 qp minor); HS99EXP K (max_iter both; +1 minor); MESH D (max_iter
both; +2 minors); OPTPRLOC D,K (optimal both; iterations 40 -> 42). Every other row, both arms, equals arm A's.

**HS battery gray-zone census:** Release 4 exits (HS15 warm), Debug 0, pinned in `HsSweepCorpus.Corpus`.

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument (under `legs/`) | criterion |
|---|---|---|
| P1 | `ctest.sh` rel, dbg -> `suites/` | each log reads "100% tests passed, 0 tests failed out of 3734" |
| P2 | `replay.sh` -> `replay/`, 3 repetitions x walk / ssn / ipm | each B run vs arm A's dev row (`dev/A-k1-*.csv`) and vs B's dev row, `scripts/compare_replay.py` byte-exact, 0 differences |
| P3 | `train_enum.sh` + `train_check.py` -> `train/` | `I4_PASS`: every row equals B's dev row; the set of rows differing from arm A's dev row is exactly `movers-train.txt`; none missing or capped; any WORSE mover (reference solved, fresh not) fails |
| P4 | `../runtime/run_walk.sh` (ap5-g3's runtime gate, walk mode) | serial and solo under the lock; THROTTLE 0; counter identity on every (round, cell); B/A instructions:u within [0.995, 1.005] (analyze_r11.py) |
| P5 | `mutations.sh` -> `mutation/` | control (none) clean; M1 (no polish row trigger), M2 (gray hold off: stationary pass certifies at 10x), M3 (no floor on the stationarity comparison), M4 (counter not incremented) each KILLED by at least one of `QpRowContract.*`, `QpRowContractPolish.*`, `QpCertificateSign.AnOptimalExitPastOneFloorUnitIsCountedAGrayExit`; tree restored |

Any status worse than arm A on any leg fails the leg unless listed in section 3 (none is). Protocols: every leg `pgrep -a hven_bench_cell` empty
first, under `flock /tmp/box-build.lock` (the orchestrator holds it for the chain), `set -o pipefail`, `setsid -w`, FOREGROUND markers,
`MKL_NUM_THREADS=1`, `TMPDIR=build-k/tmp`, `ctest -j4` under `setsid`; P2, P3, P4 serial, one process at a time (P3: 476 cell processes
through `cell.sh`, THREADS=1, core 2, CAP 300 s). P2 and P3 assert counters and statuses only; the wall-clock of no leg is quoted.

## 5. Order, STOP and ON FAIL

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-kopt-1x/legs/orchestrate.sh 006ccc49663611dc27440989860d1a1f468e4090 8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78 111a4bbbc1e3eef2dd770fa4e0f741e08887ac2ea492d651b58b3484bc595c90 893c5167803db46e907bab28228a6a601117aca9603771dae6cccd15d6d05d48 39e08bce607540430a0b882dcb92d1c438799cf72d964fc606e4617f216bf348 > docs/notes/data/2026-11-m8-kopt-1x/orchestrate.out 2>&1
```

The chain stops at the first failed leg: commit the evidence, report; no re-run, no criterion change after data; a successor
`DECLARATION-r2.md` after a fix.

## 6. NOT RUN

HOLDOUT (never). The 57-cell census, interior, install smoke, TSan (section 2). Any wall-clock measurement. The iparm leg (no Pardiso
configuration moves).
