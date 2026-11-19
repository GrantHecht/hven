# M8 WA-1 r2 -- the footprint step capped at the rows' structural margin (DECLARATION, successor of DECLARATION-WA1.md)

Committed before the first r2 leg; never edited afterwards. `DECLARATION-WA1.md` and every run-1 output are kept unchanged. A failed or
void r2 leg is answered by `DECLARATION-WA1-r3.md`.

## 0. Why a successor

Run 1 (`RESULTS.md`) passed every leg on base `66cc44ccc`. The branch was then rebased onto `origin/m8` `bc8ed0e12` (RS-4; `REBASE.md`:
suites and replay identical). CI run **44480073900** on the rebased head failed on **windows-clang-release**: `QpW1AscentGuard.
BritgasElasticQp1{Border,Refactorize}` counted `eq_drift_violations` = 9 (the test asserts 0 on every build) on an off-reference route
with 261 ascent rides (Linux: 4). The same code had passed Windows in run 44459486250; recent `m8` and `rs-*` CI runs show no such
failure, so the failure is read as this change's. Mechanism: the footprint branch rides -p UNCAPPED, and -p carries the regularized
step's row footprint alpha * mu * lambda; on a long ride that crosses the walk's own drift invariant (THE EQUALITY DRIFT, 10x
row_tolerance). Nothing on Linux reaches it (TRAIN and replay: no drift counter moved).

## 1. The change (code commit `df3c523d6fb50f865629244d06b884c5186c10e4`)

`detail::footprint_decision` takes an eighth argument, the ROW BUDGET along -p: the least alpha at which a working row's residual
reaches `kInfeasibilityMarginFactor * row_tolerance` (computed in `QpEngine::footprint_merit` with the iterate's multipliers). The step
stops at the nearest of the blocker (ridden), the merit's minimiser and the row budget (both moves; nothing joins). The ordinary step's
own 1x budget was built and measured first and REJECTED: on `QpYrkRefusedExchange.YorknetRestorationQp25*` it certified a worse local
minimum (f = -5.7996 against the pinned -1398.6341). The qp26 fixture now also asserts `eq_drift_violations == 0`; a unit test
(`DecisionStopsAtTheRowBudget`) pins the budget's stop. Docs: `walk_engine.h` section 4c (THE REGULARIZATION FOOTPRINT) and the member
comments say the budget and why.

- Code paths `src include tests bench examples bindings CMakeLists.txt cmake scripts` at the leg HEAD must equal `df3c523d6`
  (`legs/orchestrate_r2.sh`). Commits after it add only evidence and runners.
- Arm A = `origin/m8` `bc8ed0e124a8e349e9bbe107f2017cf25eda3f95` (worktree `../wa1-base`, `build-wa/rel`). Arm B = this worktree,
  `build-wa/rel` and `build-wa/dbg` (`legs/dev.sh`, flags as run 1). Run-1 binaries moved to `build-wa/bin-r1/`.
- Corpus binaries (frozen `build-wa/bin/{A,B}`): A `f1b22f0f70bc371f56a87afbd2337a312eab93c2ccfb56e02503654047e55cb9`,
  B `39a36bf205fec0e54744646e10dc9be3b775bd7a24470e409df6d7e4f4284e47`.
- Lane binaries: A `4ec11d6a5789bb97a5e46a6fceb68a994d753691ab5326a7e1659406cbc6a330`, B
  `b31d8a7721453ebd7be0646f779ef4576cd7fb3e5305686c499e5102f8a38d56` (private clone `.scratch/hb`, branches `wa1r2-A` / `wa1r2-B`, the
  same lane recipe as run 1: hven-bench main `6d1a8a0` + never-label patch + RS include fix + pin).

## 2. Disclosures (none is a leg)

- Instrument changes since run 1, all in `legs/` and `runtime/`: `dev.sh` (DEVROOT and the round tag), `train_setup.sh` (the round tag
  in the clone branch name), `mutate.py` (M4's anchor follows the code; **M8** added: the row budget never stops the step),
  `mutations.sh` (M8 in the must-kill set), `runtime/run_walk.sh` (an output directory argument; run 1's default unchanged), and the new
  `legs/orchestrate_r2.sh` (run 1's orchestrator with every output under `r2/`, r2 dev rows and movers). Run 1 used the earlier
  versions; its outputs are untouched.
- Dev at the r2 code commit (`r2/dev/`, raw rows `.scratch/r2/`): replay A vs B 0 differences in walk, ssn and ipm; TRAIN 476 rows, 0
  missing or capped, 9 movers all NEUTRAL -- the same 9 rows as run 1, and B's rows equal run 1's B rows except HS101.K
  (`border_refine_steps` 1482 -> 1483; status, iterations, objective and minors equal). Arm A's rows equal run 1's arm A rows (RS-4 is
  counter-neutral). TRAIN totals B vs A: qp minors 209225 -> 211784 (+1.2%), factorizations 249131 -> 253973 (+1.9%), iterations
  13570 -> 13573.
- Targeted seam run of the r2 code (`QpWa1AscentFootprint.*`, `QpW1AscentGuard.*`, `QpYrkRefusedExchange.*`): all pass; qp26 fixture
  kOptimal in 38 minors, f = -1458.316029, eq_drift 0; qp25 f = -1398.63412864 (85 minors, as run 1). No full r2 dev ctest was run.

## 3. Expected movers (declared)

Test suite: run 1's +6 tests plus `DecisionStopsAtTheRowBudget` (3892 = arm A's 3885 + 7). Replay: none (`r2/movers-replay.txt`).
TRAIN: `r2/movers-train.txt`, the 9 NEUTRAL rows of `DECLARATION-WA1.md` section 3 (CRESC100.K, HS101.D/K, HS56.D, MESH.D, MSS1.D/K,
SPANHYD.D/K). Cost bound unchanged: +2.5% on TRAIN qp minors and factorizations, no status change; runtime band as below.

## 4. The legs and their criteria (fixed here, before data)

As `DECLARATION-WA1.md` section 4, with outputs under `r2/`, references the r2 dev rows (`.scratch/r2/dev-replay`,
`.scratch/r2/dev-train-{A,B}`), and:

| leg | criterion |
|---|---|
| P1 | each log reads "100% tests passed, 0 tests failed out of 3892" |
| P2 | 3 x walk/ssn/ipm, each B run vs arm A's r2 dev rows and vs B's r2 dev rows: 0 differences |
| P3 | `I4_PASS` against `r2/movers-train.txt`; no WORSE mover |
| P4 | `runtime/run_walk.sh` into `r2/runtime/`: THROTTLE 0; counter identity; filtered B/A in [0.995, 1.005] |
| P5 | control clean; M1, M2, M3, M4, M5, **M8** each KILLED; M6, M7 recorded; tree restored |
| P6 | install smoke and export contract rc 0, both front ends |

Ship rule, protocols and STOP as `DECLARATION-WA1.md`.

## 5. Order

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-wa1/legs/orchestrate_r2.sh df3c523d6fb50f865629244d06b884c5186c10e4 f1b22f0f70bc371f56a87afbd2337a312eab93c2ccfb56e02503654047e55cb9 39a36bf205fec0e54744646e10dc9be3b775bd7a24470e409df6d7e4f4284e47 4ec11d6a5789bb97a5e46a6fceb68a994d753691ab5326a7e1659406cbc6a330 b31d8a7721453ebd7be0646f779ef4576cd7fb3e5305686c499e5102f8a38d56 3892 > docs/notes/data/2026-11-m8-wa1/r2/orchestrate.out 2>&1
```

## 6. CI

After the legs: push `wa-1`, `gh workflow run ci.yml --ref wa-1`, all four lanes; Windows run at least twice (its BRITGAS route flips
run to run off the reference build). NOT RUN as `DECLARATION-WA1.md` section 6.
