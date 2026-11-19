# M8 WA-1 -- the walk's ascent on a regularization footprint (DECLARATION)

Committed before the first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is
answered by `DECLARATION-WA1-r2.md` (then -r3, ...), with this file and every output kept.

## 0. What this is

A declared walk change (CLAUDE.md section 7). The sparse-equilibration diagnosis (`../2026-11-m8-sparse-equil-v2/diag/qp26-cycle.txt`,
branch sparse-equil-v2) found YORKNET restoration qp26 cycling with period 5 to its minor cap under the WIP matched factorization,
including objective-increasing ordinary steps. The mechanism (`diag/mechanism.txt`, raw data `diag/MANIFEST-scratch.txt`):

- Minor 44: slope (Hx+g)'p = +50.35, p'Hp = -86.56 (verified), iterate on its rows -- every precondition of the ascent guard (4c)
  but one: p is not in the working set's null space. It leaves equality rows 191 and 221 by 4.5e-4 / 5.2e-5 (relative 2.1e-6 /
  2.5e-7 vs kRideNullspaceTol 1e-8): the dual_mu footprint A_W p = mu lambda_W of a nearly dependent working set. The guard
  declined; the ordinary step rose f by 13.6 and left the iterate 1.93e-4 off row 191.
- The face's inertia (271/257, 0 perturbed pivots) is true: minor 43's negative-curvature ride added a bound to an indefinite face
  and the face stayed indefinite (the interlacing note under THE ASCENT GUARD); p'Hp = -86.6 confirms it.
- Minor 46's rise (+1.95e-4, ride null-space-vetoed, ordinary step) restores minor 44's drift on row 191: a consequence.
- The mu-free step is no remedy: the face factors at dual_mu = 0 with residual ratios 1e26..1e45.
- Reproduced on origin/m8 WITHOUT matching from the cycle's entry state (fixture below): kIterationLimit at 8240 minors, 1644
  ascents of up to 13.6 -- walk-intrinsic.

The change (`src/qp/walk/walk_engine.cpp`, `include/hven/detail/qp/walk/walk_engine.h` section 4c THE REGULARIZATION FOOTPRINT,
`walk_trace.h`): for a p that holds its pins but not its rows, from an iterate on them, with slope > 0 and verified p'Hp < 0, the
step is read in the regularized face's penalty merit M = f + |A_W x - b_W|^2 / (2 mu) (the regularized KKT is its Newton system).
When M's slope along p rises too, the walk goes along -p -- f falls for every alpha -- ridden to the blocker when the blocker comes
no later than M's minimiser along -p (or M is concave along p), else moved to that minimiser (`detail::footprint_decision`).
Basis: Nocedal & Wright ch. 16 (descent in active-set QP), Gill & Wong 2015 (regularized active-set QP and its merit), Gill,
Murray, Saunders & Wright 1991 (inertia-controlling methods: no Newton step toward a non-convex face's stationary point; a descent
direction to a blocker). With dual_mu = 0, or a p in the null space, nothing changes (the existing guard's path is byte-for-byte
the same decision). No backstop is added.

## 1. The code and the binaries

- **Code commit `0d674bdcbc7affd9b7e6e24449413209f8e42746`** (branch `wa-1`; base = arm A = `origin/m8` `66cc44ccc9ce4b62efe7b6e1a1de3d67628a2c71`).
  `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg HEAD must equal it (`legs/orchestrate.sh`).
  Commits after it add only evidence and runners.
- Trees: `build-wa/rel`, `build-wa/dbg` (arm B, this worktree), `../wa1-base/build-wa/rel` (arm A, detached worktree at the base),
  clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST`, `HVEN_BUILD_BENCH=ON HVEN_BUILD_EXAMPLES=ON` (`legs/build.sh`), built by `legs/dev.sh`.
- Corpus binaries (P2, P4), frozen in `build-wa/bin/{A,B}`: A `51fd5c0bb644a26f4becf9d51cd2c897b5e5bd0b8ef2d65871a6629b409721dd`,
  B `e8e3d9548a7d49def22083640c48c31f280173ea58ba70cfe23ccdc57c80bf44`.
- Lane binaries (P3), frozen: A `000c90854b3c84b81ddd7ea2b96886165e154fbf821961057ee3f716799b26ba`, B
  `3413a1f2e0d91f066d42ef726ba2772ae3415b92cfbdc78dac4eb0b51f63f6b7`, built by `legs/train_setup.sh` in a PRIVATE clone (`.scratch/hb`,
  git-ignored, never pushed) of `/home/ghecht/Projects/hven-bench` at `main` `6d1a8a07804964be524bbc9d5181f54228f7d400` plus
  `legs/hb_patch_never.py` (the `never-<base>` label), the RS include fix (`lanes/external/declared_point.h`:
  `detail/drivers/declared_diagnostics.h` -> `detail/solver/`), `sets/cutest` copied from the source clone, and only the pin moved.

Linux x86-64, AMD Ryzen 7 5800X3D, MKL 2026.2, `MKL_NUM_THREADS=1`. Apple/Accelerate and Windows: by CI only (section 6).

## 2. Disclosures: what ran before this file (none is a leg)

- The reproduction tree `.claude/worktrees/wa1-repro` (detached `a5798ad32` + seq2's `devdbg-v3.patch`, copied unmodified, + WA1
  dev prints and state dumps), its traces, and the fixture generator: `diag/MANIFEST-scratch.txt`.
- Two rejected designs, built and measured on the fixture: (a) a mu-free re-solve of the face (refused: residual 1e26..1e45);
  (b) a merit ride only when M is concave along p: the cycle broke but a later pass (verdict kOk, slope +0.10, M convex along p)
  ascended f by 0.10 and drifted 8e-6 off its rows, and the walk ended kNumericalError at 894 minors. The declared rule adds the
  move to M's minimiser; the fixture then certifies kOptimal in 38 minors, f = -1458.316029 (`diag/mechanism.txt`).
- Dev runs at the code commit (outputs under `dev/`, raw rows in `.scratch/`): Release ctest "100% tests passed, 0 tests failed out
  of 3891" (`dev/dev-ctest-rel.txt`; arm A + 6 new tests); replay once per arm and mode, 27 cells x 75 columns: **0 differences** in
  walk, ssn and ipm (`dev/compare-*.txt`); TRAIN once per arm: 476 rows, 0 missing or capped, **9 movers, all NEUTRAL**
  (`dev/train_check-dev.txt`).
- `origin/m8` moved from `ca3588735` to `66cc44ccc` (the Windows CI fix) during the work; the branch was cut again on it before the
  code commit.

## 3. The expected movers (declared)

**Test suite:** +6 tests in `tests/qp/test_qp_ascent_footprint.cpp` (two fixture tests `QpWa1AscentFootprint.YorknetQp26MatchedFace{Border,
Refactorize}` on the new fixture `tests/qp/fixtures/e2-d4/YRK_YORKNET_qp26_m42.txt`; four unit tests of `detail::footprint_decision`).
No existing test changes. (`QpYrkRefusedExchange.YorknetRestorationQp25*` prints 85 minors where arm A printed 83; minors are printed, not
asserted.)

**Replay (27 cells x walk / ssn / ipm):** none (`legs/movers-replay.txt`).

**TRAIN (238 cells x D/K), `legs/movers-train.txt`:** 9 rows, all NEUTRAL (no status changes):

| cell | status (A = B) | what moved |
|---|---|---|
| CRESC100.K | infeasible, 25 it | qp minors 18339 -> 18340, ascent rides 1 -> 2 |
| HS101.D | optimal, 27 it, same f | qp minors 698 -> 699, ascent rides 0 -> 1 |
| HS101.K | optimal, 28 it, same f | qp minors 806 -> 808, ascent rides 0 -> 2 |
| HS56.D | optimal, 39 it, same f | qp minors 551 -> 552, ascent rides 0 -> 1 |
| MESH.D | max_iter, 273 it, same f | qp minors 4011 -> 2804, factorizations 7177 -> 5391, qp maxiter exits 1 -> 0 |
| MSS1.D | numerical_error | 11 -> 33 it, qp minors 9003 -> 14517 |
| MSS1.K | numerical_error | 31 -> 12 it, qp minors 9521 -> 8955 |
| SPANHYD.D | numerical_error, 22 it | qp minors 23158 -> 20675 |
| SPANHYD.K | numerical_error, 23 it | qp minors 20309 -> 21605 |

**Cost bound (declared):** TRAIN totals over the 476 rows, B vs A (dev): qp minors 209225 -> 211784 (+1.2%), factorizations 249131 ->
253973 (+1.9%), SQP iterations 13570 -> 13573; the bound is +2.5% on qp minors and factorizations and no status change. Runtime: the
replay has no mover, so P4's counter identity holds and the B/A band applies.

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument (under `legs/`) | criterion |
|---|---|---|
| P1 | `ctest.sh` rel, dbg -> `suites/` | each log reads "100% tests passed, 0 tests failed out of 3891" |
| P2 | `replay.sh` -> `replay/`, 3 repetitions x walk / ssn / ipm | each B run vs arm A's dev rows (`.scratch/dev-replay/A-k1-*.csv`) and vs B's dev rows, `scripts/compare_replay.py` byte-exact: 0 differences |
| P3 | `train_enum.sh` + `train_check.py` -> `train/` | `I4_PASS`: every row equals B's dev row; the rows differing from arm A's dev rows are exactly `movers-train.txt`; none missing or capped; any WORSE mover fails |
| P4 | `../runtime/run_walk.sh` (kopt-1x r2's walk-mode runtime gate) | serial and solo under the lock; THROTTLE 0; counter identity on every (round, cell); B/A instructions:u within [0.995, 1.005] (`analyze_r11.py`) |
| P5 | `mutations.sh` + `mutate.py` -> `mutation/` | control clean; **M1** (footprint branch never entered), **M2** (merit-rise test removed), **M3** (no merit minimiser), **M4** (blocker never wins), **M5** (move along +p) each KILLED by at least one test; M6 (move applied with the wrong sign) and M7 (pins test removed) RECORDED, not criteria (M6: no fixture reaches the move path -- its decision is pinned by the unit tests; M7: equivalent, bounds are eliminated so an EQP step holds its pins); tree restored |
| P6 | `install_smoke.sh` -> `install-smoke/` | install smoke and export contract rc 0 on the default (GCC) and clang++ front ends |

The ship rule: no WORSE status on any leg, no lost certified cell, cost within section 3's bound. Protocols: every leg `pgrep -a
hven_bench_cell` empty first, under `flock /tmp/box-build.lock` (the orchestrator holds it for the chain), `set -o pipefail`, `setsid -w`,
FOREGROUND markers, `MKL_NUM_THREADS=1`, `TMPDIR=build-wa/tmp`, `ctest -j4` under `setsid`; P2, P3, P4 serial, one process at a time
(P3: 476 cell processes through `cell.sh`, THREADS=1, core 2, CAP 300 s; P4 alone on the box). P2 and P3 assert counters and statuses
only. Other agents share the lock; waiting on it is expected.

## 5. Order, STOP and ON FAIL

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-wa1/legs/orchestrate.sh 0d674bdcbc7affd9b7e6e24449413209f8e42746 51fd5c0bb644a26f4becf9d51cd2c897b5e5bd0b8ef2d65871a6629b409721dd e8e3d9548a7d49def22083640c48c31f280173ea58ba70cfe23ccdc57c80bf44 000c90854b3c84b81ddd7ea2b96886165e154fbf821961057ee3f716799b26ba 3413a1f2e0d91f066d42ef726ba2772ae3415b92cfbdc78dac4eb0b51f63f6b7 3891 > docs/notes/data/2026-11-m8-wa1/orchestrate.out 2>&1
```

The chain stops at the first failed leg: commit the evidence, report; no re-run, no criterion change after data; a successor
`DECLARATION-WA1-r2.md` after a fix.

## 6. CI and NOT RUN

After the legs: branch `wa-1` pushed (never `m8`), `gh workflow run ci.yml --ref wa-1`, all four lanes reported (Apple/Accelerate and
Windows readings of the new tests come from CI only; the fixture tests record, not pin, their route off the reference build).
NOT RUN: HOLDOUT (never); the 57-cell census (not asked; the replay and TRAIN read the change); interior (the interior-point engine does
not reach the walk); iparm (no Pardiso configuration moves); any quoted wall-clock.

## 7. Registered (not this round's)

- A pass on a kSuspect (perturbed-pivot) factor can take an in-face step with slope > 0 and p'Hp > 0 (the fixture's start: alpha 3.9e-8,
  f +2.4e-9): an inexact solve, outside this rule.
- The footprint rule's move path is reached by no fixture here (its decision is unit-tested); the TRAIN rows that fire the branch report
  `ascent_rides` only.
