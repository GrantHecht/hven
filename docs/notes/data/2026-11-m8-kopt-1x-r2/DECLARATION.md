# M8 kopt-1x r2 -- the QP walk's kOptimal row contract, second round (DECLARATION)

Committed before the first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by
`DECLARATION-r2.md`, with this file and every output kept.

## 0. What this is

The first round (`../2026-11-m8-kopt-1x/`) promised `kOptimal` => every equality row and working inequality row within 1x of
`row_tolerance`. On an Intel AVX-512 CI runner (MKL branch 14) YORKNET qp5 (`RDV_YORKNET_qp5.txt`) still certified row 246 at 6.88x
(residual 2.082e-7, row scale 30.26, lambda 0.198474, 1177 minors, 2 rank repairs, `optimal_row_gray_exits` = 1). This round repairs the repair
paths (settler ruling C387: no hard `kNumericalError` gate until a census finds the gray zone empty; the counter stays the record).
It is a declared re-pin event (CLAUDE.md section 7): the polish acceptance, the hold bookkeeping and the certifying site change; so do
a set of pins whose first-round values came from a defect found here (section 2).

The change (`src/qp/qp_engine.cpp`, `include/hven/detail/qp/qp_engine.h`; rationale in `docs/migration-0.2.md`):

1. **(a) Polish acceptance reads the row contract.** `adopt_polished_candidate` also requires the candidate's `worst_row_ratio` (its own
   multipliers) to be within `max(1, the incumbent's ratio)` (the incumbent's own multipliers); a row-triggered entry prefers a stage-1 candidate within 1x
   and falls back to stage 1 as it was if stage 2 adopts nothing.
2. **(b) The gray hold is spent only by an ordinary step.** A rank-repair or border-retreat `continue` leaves it unspent.
3. **(c) A bounded return from the certifying site.** After polish adoption, refusal or sign/bound revert, the 1x ratio is re-read with the
   final multipliers; past 1x the walk goes back for an ordinary step (the next KKT point holds), at most `kGrayReturnBudget` = 16 times per
   solve and only while the previous return's step added a working-set member (the first return is always taken); then it certifies and
   counts `optimal_row_gray_exits`.
4. **The row contract reads a working inequality row one-sided** (violation only), as `worst_structural_violation` and `terminal_kkt` do. The
   first round's `worst_row_ratio` read it two-sided, so the slack of a zero-multiplier working row counted as a violation.
5. **(d) the 1x verdict-site refinement target: BUILT, MEASURED, NOT ENABLED** (deviation from the ruling, with evidence in section 2): the
   verdict-site refinement keeps the classifier's 10x target and trigger; a refined point that certifies past 1x is returned to the walk by (c).

## 1. The code and the binaries

- **Code commit `b64fd4cfd9b5fd90ea29d199e1e62a1f37ebfaba`** (branch `kopt-1x-r2`; base = arm A = `origin/m8` `d7be9a4e3747ce58e99abceb23908fcebc005582`).
  `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg HEAD must equal it (checked by `legs/orchestrate.sh`).
  Commits after it add only evidence and runners.
- Trees: `build-k/rel`, `build-k/dbg` (arm B), `../kopt-1x-r2-base/build-k/rel` (arm A, detached worktree at the base), clang 22.1.8,
  `-DHVEN_FP_MODE=SAFER_FAST`, `HVEN_BUILD_BENCH=ON HVEN_BUILD_EXAMPLES=ON` (`legs/build.sh`).
- Corpus binaries (P2, P4), frozen in `build-k/bin/{A,B}`: A `33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0`, B
  `0a10b101cdba839f63195f2eb8640bb3df68eb5d3eab84e83152afc2f56d5baf`.
- Lane binaries (P3), frozen: A `339605d056be9c77906f31e678b0b53e2795da89c1d0882cad9b648db69d7dc2`, B
  `030c19a411ce9f1e2516f51b6b3839e5ed5299ba340247c1a91df2328e314087`, built by `legs/train_setup.sh` in a PRIVATE clone (`.scratch/hb`,
  git-ignored, never pushed) of the local hven-bench clone at `main` `6d1a8a07804964be524bbc9d5181f54228f7d400` plus `legs/hb_patch_never.py`
  (the `never-<base>` label prefix = `<base>` with `linear.dense_factorization = kNever`; the first round's lane commit `adcb7d2` no longer
  compiles against this hven's API), with only the pin moved to the arm's hven sha (A: the base; B: the code commit). Both arms use the same lane
  source; `legs/cell.sh` is the first round's, re-pointed.

Linux x86-64, AMD Ryzen 7 5800X3D, MKL 2026.2, `MKL_NUM_THREADS=1`. Apple/Accelerate: UNOBSERVED here. The Intel acceptance reading is the CI
section of `RESULTS.md`.

## 2. Disclosures: what ran before this file (none is a leg)

- `dep/eigen`, `dep/fmt` copied from the main checkout (submodule update refused); both worktrees.
- The reproducers were built first and shown RED on the base engine (`dev/red-base-reproducers.txt`): the multiplier-repricing synthetic
  (`QpRowContractPolish.ARepricedCandidateIsNotAdoptedPastOneFloorUnit`: the candidate is adopted at 6.88 floor units) and, on the real fixture,
  the certificate seam presenting the walk's own certified point with row 246 moved to 6.88 floor units on the first read
  (`QpRdvRankRepair.YorknetQp5CertifiedGrayRowIsReturnedToTheWalk*`: kOptimal, ratio 6.880, one gray exit; residual 2.082e-7, row scale 30.26,
  lambda 0.198474 -- the Intel numbers). The post-repair seam the task asked for (perturb row 246 before the successful-repair `continue`, hold flag
  preserved) is built (`QpRepairSeam`, registered in `docs/testing.md`) but is GREEN on the base engine: the 373 passes after the repair absorb the
  perturbation, so it is kept as a regression pin (`YorknetQp5PostRepairGrayRow*`) and, with a hold forced at the firing, pins that a repair does
  not spend the hold (`YorknetQp5RepairDoesNotSpendTheHold*`).
- **A defect in the first round's instrument, found here.** `worst_row_ratio` read a working inequality row two-sided. On the TRAIN lane the first
  round's counter read 68 gray exits in 7 cells (ALLINITC.K 50, LEWISPOL.K 8, CRESC100.D 3, CRESC4.D 3, CRESC4.K 2, CRESC50.K 1, OPTPRLOC.D 1);
  a debug print at those exits showed working inequality rows with multiplier ~1e-20 sitting 75 to 416 inside their row (ratio up to 1.16e8)
  -- primal feasible points counted as gray, and the first round's hold fired on them. Its HS-battery census of 4 (HS15 warm) and its pins
  (HS10 warm 28/42 -> 26/40, total minors Release 3355 -> 3359 and Debug 3346 -> 3355, HS15 failed-step arm 34/99/39/7 -> 33/95/38/6) came from that
  read; read one-sided they return to the values before the first round (verified by the tests below, not asserted here).
- **Path evidence (Codex's ranking).** (1) multiplier repricing at the polish: REAL (synthetic RED at the base). (2) the hold consumed by a repair
  `continue`: REAL by construction (the hold was copied to `prev` at loop entry); not reachable on the box. (3) polish refusing both candidates
  and leaving the bad incumbent: REAL on the real fixture -- with row 246 at 6.88x both stage candidates were infeasible by 2.4e-3 (the unclipped
  step crosses an inequality), so the polish could not repair it and only returning to the walk did (8 returns, each a degenerate add, then
  ratio 6.7e-8). (4) zero-motion bypassing the hold: covered by (c)'s re-read, no separate evidence. (5) verdict-site refinement targeting 10x: see
  the next item. (6) a revert restoring a bad incumbent: covered by (c)'s re-read, no separate evidence.
- **(d) built and not enabled.** A 1x target at a gray row (`worst == 0`) fired on ACOPP30 once and moved that solve's whole trajectory: qp minors
  615 -> 2494 (the TRAIN lane, same 11 majors; with the trigger off the cell equals arm A exactly). A 1x-first target at a structural dead end moved
  the classifier's refined point (a T7 pin: 9 -> 10 steps, residual 2.943550e-4 -> 2.981e-4). Neither bought a measured row-contract gain, so the
  refinement is exactly the base's.
- **Budget and progress rule.** With a bound of 2 the YORKNET seam point stays gray; it needs 8 returns, so the bound is 16 (the seam leg of
  `YorknetQp5CertifiedGrayRowIsReturnedToTheWalk*`). Without the progress rule the TRAIN lane paid for unrepairable gray exits (LEWISPOL.K 67 -> 323
  minors, ALLINITC.K 316 -> 1292); with it 83 and 377.
- Dev runs (outputs under `dev/`, raw cells in `.scratch/`): suites Release and Debug "100% tests passed, 0 tests failed out of 3857"
  (`dev/dev-ctest.txt`); replay once per mode, A and B, 27 cells x 75 columns: **0 differences** in all three (`dev/replay-compare-*.txt`); TRAIN
  once per arm (the final B lane): 476 rows, 0 missing or capped, **10 movers, all NEUTRAL** (`dev/train_check-dev.txt`, section 3).
- Intermediate code states built and run through the dev lane (in the private clone, never pushed): a temporary debug-print commit on
  `kopt-1x-r2` was reset before the code commit; the CI dispatches 44096979507 and 44097860913 ran earlier code states and were cancelled.
- No interior leg (the interior-point engine reaches neither the walk nor the SQP driver), no install smoke, no TSan: nothing here touches an
  install surface or a concurrent path; no new counter.

## 3. The expected movers (declared)

**Test suite** (arm B vs arm A): +7 new tests (`QpRowContractPolish.ARepricedCandidateIsNotAdoptedPastOneFloorUnit`, `QpRdvRankRepair.Yorknet
QpPostRepairGrayRow{Border,Refactorize}`, `...CertifiedGrayRowIsReturnedToTheWalk{...}`, `...RepairDoesNotSpendTheHold{...}`);
`QpCertificateSign.AnOptimalExitPastOneFloorUnitIsCountedAGrayExit` re-derived (a gray point is presented twice: the walk returns once);
the YORKNET off-reference arm asserts ratio <= 1 again; **the first round's HS-battery pins are withdrawn to the values before it**: HS10 warm
Release 28/42, total minors Release 3355 and Debug 3346, HS15 failed-step arm Release 34/99/39/7, the gray census 0 in both configs (was Release 4).

**Replay (27 cells x walk / ssn / ipm):** none.

**TRAIN (238 cells x D/K), `legs/movers-train.txt`:** 10 rows, all NEUTRAL by the status-rank rule (no solved cell becomes unsolved):
- reversions of the first round's effect (B equals the cell's value before the first round): CRESC100.D (iterations 45 -> 40, minors 13586 -> 23791),
  CRESC4.D (26 -> 65, 402 -> 1240), CRESC4.K (26 -> 48, 407 -> 843), OPTPRLOC.D and .K (42 -> 40), HS84.D and .K (54 -> 53 minors), CRESC50.K
  (max_iter -> numerical_error, both unsolved; the status label moves back). These are cells where the first round's hold fired on a
  feasible working row (the defect above); the cost direction is mixed and the cells that got slower (CRESC) are the ones that had been sped
  up by a hold that should not have fired.
- ALLINITC.K (minors 316 -> 377) and LEWISPOL.K (67 -> 83): returns from the certifying site on gray exits the walk cannot repair (status
  numerical_error and infeasible, unchanged).
Every other row, both arms, equals arm A's.

**Gray-exit census (`optimal_row_gray_exits`, TRAIN lane):** arm A 68 exits in 7 cells; arm B 58 in 2 cells (ALLINITC.K 50, LEWISPOL.K 8, both
cells whose final status is not optimal). HS battery (Release): A 4 (HS15 warm), B 0; Debug 0 both.

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument (under `legs/`) | criterion |
|---|---|---|
| P1 | `ctest.sh` rel, dbg -> `suites/`; P1b: arm A's own `HsSweepCorpus.Corpus` (its pin asserts the Release census of 4) -> `suites/baseA-hs-census.log` | each log reads "100% tests passed, 0 tests failed out of 3857"; P1b reads PASSED |
| P2 | `replay.sh` -> `replay/`, 3 repetitions x walk / ssn / ipm | each B run vs arm A's dev row (`dev/A-k1-*.csv`) and vs B's dev row, `scripts/compare_replay.py` byte-exact, 0 differences |
| P3 | `train_enum.sh` + `train_check.py` -> `train/`; P3b `gray_census.py` | `I4_PASS`: every row equals B's dev row; the set of rows differing from arm A's dev row is exactly `movers-train.txt`; none missing or capped; any WORSE mover fails. P3b is a report (A 68 in 7 cells, B 58 in 2 cells expected; a B count above A's total, or a cell with more exits than at A, is a failure) |
| P4 | `runtime/run_walk.sh` (ap5-g3's runtime gate, walk mode) | serial and solo under the lock; THROTTLE 0; counter identity on every (round, cell); B/A instructions:u within [0.995, 1.005] (analyze_r11.py) |
| P5 | `mutations.sh` -> `mutation/` | control clean; **Ma** (polish acceptance does not read the candidate's row ratio), **Mb** (a rank-repair `continue` spends the hold), **Mc** (no return from the certifying site), **Mg** (returns without the progress rule) each KILLED by at least one test; **Me1** (bound removed, progress rule alone), **Me2** (bound doubled), **Me3** (bound and progress rule removed: returns until the minor cap) each TERMINATE (no test run hits its `timeout`), killers recorded; tree restored |

Any status worse than arm A on any leg fails the leg unless listed in section 3 (none is). Protocols: every leg `pgrep -a hven_bench_cell` empty
first, under `flock /tmp/box-build.lock` (the orchestrator holds it for the chain), `set -o pipefail`, `setsid -w`, FOREGROUND markers,
`MKL_NUM_THREADS=1`, `TMPDIR=build-k/tmp`, `ctest -j4` under `setsid`; P2, P3, P4 serial, one process at a time (P3: 476 cell processes
through `cell.sh`, THREADS=1, core 2, CAP 300 s). P2 and P3 assert counters and statuses only; the wall-clock of no leg is quoted. The other agent
on the box (G-RB) shares the lock; waiting on it is expected.

## 5. Order, STOP and ON FAIL

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-kopt-1x-r2/legs/orchestrate.sh b64fd4cfd9b5fd90ea29d199e1e62a1f37ebfaba 33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0 0a10b101cdba839f63195f2eb8640bb3df68eb5d3eab84e83152afc2f56d5baf 339605d056be9c77906f31e678b0b53e2795da89c1d0882cad9b648db69d7dc2 030c19a411ce9f1e2516f51b6b3839e5ed5299ba340247c1a91df2328e314087 > docs/notes/data/2026-11-m8-kopt-1x-r2/orchestrate.out 2>&1
```

The chain stops at the first failed leg: commit the evidence, report; no re-run, no criterion change after data; a successor
`DECLARATION-r2.md` after a fix.

## 6. NOT RUN

HOLDOUT (never). The 57-cell census, interior, install smoke, TSan (section 2). Any wall-clock measurement. The iparm leg (no Pardiso
configuration moves). The Intel acceptance reading is taken on CI (`RESULTS.md`), not on this box.
