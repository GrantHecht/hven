# M8 zero-step -- DECLARATION (a declared event: the SQP driver's zero-step test changes)

Committed before the first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by
`DECLARATION-r2.md` (then -r3, ...), with this file and every output kept.

## 0. The change

Code head **`237edda083ff72c3ae6fdd415812539288f2f9c6`** (branch `zero-step`, one commit on arm A, `origin/m8` at
**`10ec3df92818ac2121523287bb3d439e42d5d5e7`**, the AP-5 G3 r4 evidence commit; `origin/m8` has since moved and arm A stays this base).

1. `qp_no_motion_tol(feasibility_tol, x_inf) = feasibility_tol * max(1, x_inf)` (include/hven/detail/qp/qp_problem.h) is the one
   definition of no motion. The walk's `step_tol` (src/qp/qp_engine.cpp, the zero-motion test) reads it; the value is unchanged.
2. The driver's zero-step short-circuit (`zero_step`, src/drivers/sqp_solver.cpp) reads it too:
   `!tr_binding && step_norm <= qp_no_motion_tol(opts_.qp.feasibility_tol, step_norm)`, replacing `kZeroStepScale (1e-12) * max(1, |x|)`.
   For a step under the tolerance this is `qp.feasibility_tol` (default 1e-9) absolute. The walk's iterate is the step in the driver's
   QPs, which is why the scale argument is the step's own norm. SSN and IPQP have no no-motion test of their own; their answers are
   accurate to the same `qp.feasibility_tol`, so one definition serves all three modes (docs/migration-0.2.md, "Zero-step short-circuit").
3. `kZeroStepScale` stays 1e-12 and now means the L-BFGS pair-skip scale only; its comment is restated tersely.
4. Seam (`HVEN_TESTING`, the sixth seam, docs/testing.md): `SqpLadderSeam::perturb_restoration_walk_step` adds a noise step to the
   restoration sub-driver's walk answers that the walk reads as no motion. Test `SqpLadderSeam.ZeroStepANoiseStep...` in
   `hven_sqp_seam_tests`: certified kInfeasible on both dense arms x walk/ssn/ipm. The T7F1 and T3 DenseArm twins
   (tests/sqp/test_sqp_solver.cpp) are pinned on every build again (commit 133c56732's record-not-pin hunks reverted).

Why: macOS/Accelerate CI runs 43719577164, 43720140103, 43720765090: the dense arm's restoration sub-solve returns a 1.07e-10 step
where MKL returns 0; the walk reads it as no motion, the driver judged it, the funnel rejected it 34 times, kNumericalError uncertified.

Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, `-DHVEN_FP_MODE=SAFER_FAST`, `MKL_NUM_THREADS=1`. Apple/Accelerate:
verified by the CI lane (section 6), never by a leg here.

## 1. Arms and binaries (frozen by `legs/prebuild.sh`, `sl/lane.sh`; outputs in `dev/`)

- Arm A = `10ec3df92` (git archive export, Release, `build-zs/armA`). Arm B = the code head (`build-zs/rel` Release, `build-zs/dbg` Debug,
  `HVEN_BUILD_BENCH=ON HVEN_BUILD_EXAMPLES=ON`).
- Corpus binaries: arm A `ea688cd49d938fca2c4e525c761a63febbfe45406b9eb19ef8963d42b55e77dd`, arm B
  `9fdf7534e2160a2708e58c10b0984151a5387db8a0689836d501f1d4a75404af`.
- Lane binaries (a local clone of hven-bench-dn1 at `.scratch/hb`, never pushed; branches `zs-lane-A` / `zs-lane-B` from `dense-gate-legs-r2`
  with ONLY the pin moved): A `893c5167803db46e907bab28228a6a601117aca9603771dae6cccd15d6d05d48` (pin `10ec3df92`), B
  `5f326a05bd8e21d9eb797ca1a214062a40c3bc229168d6a500384d08e514d3c2` (pin `237edda08`).

## 2. Disclosures: what ran before this file (none is a leg)

- `dep/eigen`, `dep/fmt`: copies of the main checkout's submodule work trees at the pinned commits (submodule clone refused: file transport).
- The diagnosis and reproduction: probe tests (since deleted) scanned perturbations of the restoration sub-driver's walk answer with the
  pre-fix threshold. A perturbation of the first component alone, or of x only, is judged and ACCEPTED by the funnel (no failure); the
  failing shape is a pair of slack columns moved in opposite directions by 1e-10 (net-zero predicted decrease), 10 of 83 shapes, each
  with 34 sub-driver majors and kNumericalError uncertified -- the macOS signature. The seam uses one such shape (+v on column 2, -v on
  column 4). `dev/seam-without-fix.out` (threshold reverted: 6 of 6 policy x mode configurations fail) and `dev/seam-with-fix.out`
  (28 of 28 `SqpLadderSeam.*` pass) are committed with the code.
- Dev Release run of the code head: "100% tests passed, 0 tests failed out of 3730" (`dev/ctest-rel-dev.tail.txt`). No Debug ctest, replay,
  interior or mutation-chain run before this file.
- **The dev TRAIN run** (`dev/enumA.out`, `dev/enumB.out`, `dev/tally-dev.txt`): `sl/enum.sh` on both frozen lane binaries, all 238 cells x
  {default, never-default}, serial, before this file: **0 movers of 476 rows** (HS battery: 44 cells x 2 labels, 0 movers). The I4 criterion
  binds the leg to these rows (I4R).
- `origin/m8` moved after the worktree was cut (other tasks landed); this work stays on its cut base so that A/B isolates the change.

## 3. Expected movers

**None.** Tests: one TEST added (`git diff 10ec3df92 HEAD -- tests | grep '^[+-]TEST'` = that one line); the two twins' assertions
tighten on non-reference builds only (on the reference build their pins are unchanged and read the same). TRAIN: 0 (dev run). Replay and
interior legs: 0 differences expected. A mover found by any leg is a failure of that leg, never re-cut into a pass; a successor
declaration then classifies it (improved / equal-class / worse) with its mechanism. Permitted-worse: none.

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument | criterion |
|---|---|---|
| F1 | `legs/ctest.sh` rel, dbg -> `legs-out/suites/` | each log reads "100% tests passed, 0 tests failed out of 3730" (arm A's count + 1 TEST; Release dev reading) |
| I1 | `legs/replay.sh` -> `legs-out/replay/` | the 27 replay cells x walk / ssn / ipm / interior, arm A vs arm B, `scripts/compare_replay.py` byte-exact: 0 differences in all 4 |
| I4 | `sl/enum.sh` (A, B) + `sl/tally.py` -> `legs-out/tally.txt` | `I4_PASS`: 476 rows per arm present and uncapped; 0 movers (declared set empty: none undeclared); 0 WORSE; every row equals the dev run's (I4R); the HS battery (the roster's 22 hs x 2 engines x 2 labels) reported on its own. Equality: every .out key except `options_label` / `hven_pin_sha` plus the exit code, integers and strings byte-equal, residual-class values byte-equal or within hven-bench `docs/corun.md`'s gate (rel 1e-12, abs 1e-13); .optdump byte-equal |
| M | `legs/mutate.sh` -> `legs-out/mutation/` | control (code head's `hven_sqp_seam_tests`) passes `SqpLadderSeam.ZeroStep*`; the mutant (the driver threshold reverted to `kZeroStepScale * max(1, |x|)`, built from an export) fails it: `MUTATION_KILLED` |

Protocols. Every leg: `pgrep -a hven_bench_cell` empty first, under `flock /tmp/box-build.lock`, `set -o pipefail`, `setsid -w`,
FOREGROUND_START/END markers, `MKL_NUM_THREADS=1`, `TMPDIR=build-zs/tmp`, `GIT_CEILING_DIRECTORIES=$TMPDIR`, binary sha256 checked before the
first cell. F1: `ctest -j4`. I1, I4: serial, one process at a time (I4: 952 cell processes through `sl/cell.sh`, THREADS=1, core 2, CAP 300 s).
These are counter/status legs; no leg asserts wall-clock. `legs/orchestrate.sh` checks that `src include tests bench examples
CMakeLists.txt cmake scripts` equal the code head and are clean before the first leg.

**The serial runtime gate is NOT run**: the hot path does not change measurably -- the driver's zero-step test is a threshold compare once
per major (one `max` and one multiply replaced by the same), and the walk's `step_tol` is the identical expression through an inlined helper.
Counters, not wall-clock, are the asserted currency here (CLAUDE.md section 7).

## 5. STOP and ON FAIL

The orchestrator stops at the first failed leg: commit the evidence, report; no re-run, no criterion change after data; a successor
`DECLARATION-r2.md` after a fix, with this record kept.

## 6. Mac verification (after the legs; not a leg)

Branch `zero-step` is pushed to origin (never `m8`); `gh workflow run` on it one run at a time, `gh run watch <id> --exit-status`. The macOS
lane must pass with the twins pinned. Run ids are reported in the results file.

## 7. NOT RUN

HOLDOUT (never). The 6 h census (not asked; the walk's `step_tol` value is unchanged, the driver change is read through the TRAIN lane and
the replay). The serial runtime gate (section 4). Install smoke (no installed-interface change; `qp_problem.h` gains an `<algorithm>`
include and an inline function in a detail header).

## 8. Order

```
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/zero-step/docs/notes/data/2026-11-m8-zero-step/legs/orchestrate.sh 237edda083ff72c3ae6fdd415812539288f2f9c6 ea688cd49d938fca2c4e525c761a63febbfe45406b9eb19ef8963d42b55e77dd 9fdf7534e2160a2708e58c10b0984151a5387db8a0689836d501f1d4a75404af 893c5167803db46e907bab28228a6a601117aca9603771dae6cccd15d6d05d48 5f326a05bd8e21d9eb797ca1a214062a40c3bc229168d6a500384d08e514d3c2 > /home/ghecht/Projects/hven/.claude/worktrees/zero-step/docs/notes/data/2026-11-m8-zero-step/legs-out/orchestrate.out 2>&1
```
