# M8 I17 r2 -- the review rider (rulings 1-8) on origin/m8 afc9744bf (DECLARATION)

Committed before the first r2 gate leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by
`DECLARATION-I17-r3.md`, with this file and every output kept. `DECLARATION-I17.md` and its round (`gate/`, `RESULTS.md`, `orchestrate.out`) are kept as the r1 record.

## 0. What this is

The successor round of `DECLARATION-I17.md` for the Fable review of the r1 code (SHIP AFTER FIXES) and the settler's rulings 1-8
(`.superpowers/i17/I17-review-fable-2026-11-20.md`). The branch was rebased onto `origin/m8` `afc9744bf` (WA-1 r7 and the AP-3 examples, which now run under
ctest) without conflicts; the r1 commits are kept as rebased. The default-configuration solve is still unchanged except for the ledger (section 3).

1. **Ruling 1 (I1).** On the certified fallback's route the row's `ladder` block is built from rung A's report whenever it owns the answer (a usable rung A
   included, where it was null); its `main_status` is the interior-point tier's status at the escape (`ires.status`), never the kInfeasible the fallback
   synthesizes on exhaustion.
2. **Ruling 2 (I2).** A bound side the caller step limit set (not the declared box) is recorded per variable when the limit is folded in; a QP answer pinned
   there (the major's, the SOC re-solve's) is exported as a trust-region pin is: `kFree`, `z = 0`, `tr_active` set, so `tr_binding` is true for the strategy
   and the radius rule, and neither the activity census, the next major's seed, nor the warm currency reads a limit pin as bound activity.
3. **Ruling 4 (I4).** The limit applies in the restoration phase to the wrapper's x-block (`resto_base.n()` leading variables) in the parent's column units
   (the parent's column factors); the elastic slacks are not limited. Option doc and guide updated.
4. **Ruling 5 (I5).** Dump names `qp_p<pid>_s<solver>_<seq>_d<depth>_m<major>_<site>.txt` (pid; a process-unique per-`SqpSolver` id, inherited by its
   restoration sub-drivers); written to `<name>.tmp` and hard-linked into place, so an existing file of the final name throws `std::runtime_error` and is
   never replaced (exclusive create and atomic in one step).
5. **Ruling 6 (I6).** Restoration ledger prefixes `<prefix>_resto<entry>_<phase run>_<driver lifetime run>`.
6. **Ruling 7 (I7).** Route-specific assertions under `HVEN_TEST_ROUTE_REFERENCE` (the step-excess premise, the dump test's site names and replay equality, the
   rider's ladder/SOC/rung premises); portable assertions stay.
7. **Ruling 8 (minors).** One public enum `hven::QpDumpPolicy` (`qp/qp_options.h`; `SqpQpDumpPolicy` and `detail::QpDumpPolicy` removed); the dump format in
   `docs/qp-dump-format.md` (header pointer from `qp_dump_dir`), the `infinite_bound` line recorded (the test loader reads it), omitted tokens stated; the
   rung tag formatted only when dumping; the dump `site` reset after every walk solve and the fallback's walks tagged `fallback`; SOC stop-flag restore by
   RAII; `MessageTraceEvent` carries `std::optional<ElasticLadderRow> ladder` in place of r1's six string/bool slots (one name, `entry`; JSON key `ladder`
   written as the row's object, `null` on every other message, the message key policy; arity 20 -> 15 against r1, 14 -> 15 against the base); the console
   renders from the enums; the duplicated literal tables in `sqp_solver.cpp` removed; header comments as terse constraints (column cap, step limit, dump
   dir, mixed exhausted row, `main_status`); the SQP-only sentence (the interior-point solver has no trust region, column layer or walk QP); `TempDir`
   per process.

Ruling 3 (I3) is tests (section 3).

## 1. The code and the binaries

- Base = arm A = `origin/m8` `afc9744bf8050a87e678d0dfa77a0d8efeff40b5` (detached worktree `../i17-base`, `build/`). Arm B = this worktree (branch `i17-fixes`), `build/`, `build-debug/`.
- **Code commit `0e5eaa8aeda30fdab5f378eb22ca8c85839d5df4`.** `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg HEAD must equal it
  (`legs/orchestrate_r2.sh`); commits after it add only evidence, runners and docs. All three build trees up to date (checked).
- clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST`, bench + examples ON (`legs/build.sh`), MKL 2026.2, AMD Ryzen 7 5800X3D, Linux x86-64. Apple/Accelerate and Windows: CI only.
- Frozen binaries (sha256, checked before the first leg): corpus A `8690142108ae8bb0c00df64a4f65fd4696b96ca7a1765e2ed784b514b72ad24d`, corpus B `cbb53c108ed7abd55f986ed372b2bf7f20d2c7f777ba29323c130e5c90326b1b`; lane A `903a2cd5778e7bc833cae1bc192727651664d11a217955dfc7f7020c03b32bb8` (pin = arm A, `--no-cap`), lane B `d7b8f78c4aed7abcca2ecd804350d9cd94cdc7624065893a095622d6ed806a23` (pin = the code commit).
  Lanes: the private clone `.scratch/hb` (never pushed) at hven-bench `main` `6d1a8a0` + `legs/hb_patch_i17.py` + the RS include fix, branches `i17-r2-A` / `i17-r2-B` (`legs/train_setup.sh <wt> <arm> <sha> -r2`).
- Instruments: r1's `legs/{env,build,ctest,replay,train_setup,train_enum,cell,install_smoke}.sh` and `legs/{train_check,always_table,always_gate,hb_patch_i17}.py`
  unchanged (their docstrings name r1's base; the base is the arm-A argument / `../i17-base`, re-pointed). New for r2: `legs/orchestrate_r2.sh` (base `afc9744bf`, the r2 movers
  lists, `mutations_r2.sh`, round directory `gate-r2/`), `legs/mutations_r2.sh`, `legs/mutate_i17_r2.py` (section 5).

## 2. Disclosures: what ran before this file (none is a leg)

- Dev runs at the code commit (`dev-r2/`): Release and Debug ctest "100% tests passed, 0 tests failed out of 3954" (`dev-r2/ctest-summary-dev.txt`); replay A vs B, 27 cells x walk/ssn/ipm:
  **0 differences** in all three (`dev-r2/replay-cmp-*.txt`); TRAIN 476 rows: **40 LEDGER movers, 0 OTHER, 0 missing/capped** (`dev-r2/train_check-dev.txt`, `I17_TRAIN_PASS` against r1's list, which the
  measured set equals); Always 119 cells: **17 LEDGER movers**, C2/C3 as section 3 (`dev-r2/always_gate-dev.txt`, `I17_ALWAYS_PASS`); mutation pre-flight with `mutations_r2.sh`: control clean, **26 of 26 killed**
  (`dev-r2/mutation/`). The movers lists `movers-train-r2.txt` / `movers-always-r2.txt` are the measured sets (byte copies of r1's files because the sets are equal).
- CI on the code commit before this file: run 44682426808, four lanes green (pushed for early CI; the gate's CI criterion is a fresh run on the pushed leg head).
- The restoration limit, measured by the two restoration mutations on `CallerStepLimitSolve.RestorationStepsStayWithinTheLimitInCallerUnits` (`dev-r2/restoration-limit-excess.txt`): without the limit in
  restoration the largest caller-unit excess over the limit is 0.035 (kOff) and 6.35 (kAlways); with the limit applied in engine units without the parent's factors, 3.15 (kAlways); the code commit reads <= 0 in both.
  The synthetic issue-17 model (`Synth`, L = 4, kAlways, walk, r = 0.05) solved to the end: optimal in 32 majors, **0 restoration entries**, every visited point within the limit of the one before; the
  synthetic model does not reproduce the issue's exhaustion -> restoration sequence, which `InfeasibleBowl` (an infeasible row with column scales 1e4 : 1) exercises instead.
- Disclosure: after the dev mutation leg restored the tree, `ninja -C build-debug` was run once outside the box lock while checking the trees were current (it rebuilt the Debug tree's test
  executables, about 1 minute); nothing else ran outside the lock.
- `.scratch/bin` (r1's lane binaries) was renamed `.scratch/bin-r1`; the r2 lane binaries are at `.scratch/bin/{A,B}`.

## 3. The expected movers and pins (declared)

**Behaviour under the default configuration: none.** Replay: 0 differences. TRAIN: no row differs in any key but the ledger-derived harness columns.

**TRAIN, 238 cells x {D, K} (`movers-train-r2.txt`): 40 rows, all LEDGER class, measured on the dev run of the code commit against arm A `afc9744bf`
(`dev-r2/train_check-dev.txt`); the set is the same 40 rows as r1's `movers-train.txt` (the restoration-entered rows: the phase's QPs are ledgered). Ruling 6's label
change moves nothing: the lane sums the attached Ledger's records and never reads a label.** A row whose differences reach any key outside `side.harness_qp*` is an OTHER
mover and fails the leg. The `.optdump` of a hven-sqp row gains exactly the four I17 option lines at their defaults (spelling unchanged by ruling 8's enum move: `advanced.qp_dump_policy = kOff`), all else byte-equal; an ipm row's optdump is byte-equal.

**Always-forced TRAIN (hven-sqp, 119 cells; `movers-always-r2.txt`): 17 rows, LEDGER class only**, measured on the dev run (`dev-r2/always_gate-dev.txt`; the same 17 as r1). C2: Auto unmoved at every cap; C3: the smallest
cap with no moved Always row is 2^10 (2^8 moves HS99EXP 26 -> 27 NEUTRAL; 2^6 moves six rows, DEMBO7 WORSE), so the adopted default 1024 stands.

**ctest: 3954**, the measured count of the Release and Debug dev runs on the code commit (r1's 3925 was on the old base; the rebase brought the AP-3 examples under ctest).

**Test pins re-derived (code commit):**
- trace schema: `MessageTraceEvent` arity 14 -> 15 (r1's 20 withdrawn); every `message` line gains `"ladder":null` after `expected_n` (`kMessageGoldenLines`, 20 lines;
  `tests/trace/golden/trace_v1/ipm_complete.jsonl`'s one message line); r1's six-key message test replaced by `TheElasticExhaustionMessageCarriesTheRowsLadderBlockAndNullElsewhere`;
  the console template test builds the ladder from enums (rendered text unchanged); `TraceMessagesSqp.TheElasticExhaustion...` reads `m.ladder`.
- `SqpSolverCertifiedFallback.ARungAOwnedMajorIsAnElasticRowOnTheDriversOwnHistory` gains the ladder-row asserts (portable) and `main_status == kNumericalError` (reference route);
  `T7...` gains `main_status == kNumericalError` (inside its reference-route block).
- `tests/qp/support/qp_dump.h` reads the optional `infinite_bound` line.
- `docs/trace-schema-v1.md`, `CHANGELOG.md`, `docs/guides/snopt-like-configuration.md`, new `docs/qp-dump-format.md`.

**New tests (ruling 3 and the rider's behaviours), `tests/sqp/test_caller_step_limit_and_qp_dump.cpp`:** `CallerStepLimitSolve.` converging solve (HS071, every step within the limit, same optimum);
limit pin reported free and region-limited (tr_binding, 0 active sides, warm-start z = 0); limit making the main QP infeasible (elastic ladder carries it, converges); a variable beyond its box
by more than the limit keeps its declared box (lo > hi); a fixed variable; the equality-basis reduced path; SOC within the limit; every elastic rung's box carries the limit; restoration steps
within the limit in caller units under kOff and kAlways; the issue-17 model end to end. `QpDump.` two solvers on two threads; a rerun into the same directory keeps every earlier file; an existing
file of the next name is never replaced; the restoration-ledger test asserts label uniqueness across a second solve; the round trip checks the process token and the recorded `infinite_bound`.

## 4. The legs and their criteria (fixed here, before data)

All under `flock /tmp/box-build.lock`, serial, one process at a time; `pgrep -a hven_bench_cell` empty first; `MKL_NUM_THREADS=1`; wall-clock is not asserted (CLAUDE.md section 7); HOLDOUT is never run.
`legs/orchestrate_r2.sh <code> <corpus A> <corpus B> <lane A> <lane B> 3954` runs them in order and stops at the first failure; outputs under `gate-r2/`.

| leg | instrument | criterion |
|---|---|---|
| P0 | orchestrate_r2.sh | code paths of HEAD == the code commit and clean; base worktree's code paths == `afc9744bf`; the four binary sha256 match; the three build trees are up to date |
| P1 | `ctest.sh` Release, Debug (arm B) | each log reads "100% tests passed, 0 tests failed out of 3954" |
| P2 | `replay.sh` arm A, arm B | 27 cells x walk, ssn, ipm; `scripts/compare_replay.py` A vs B reads "differences: 0" in each of the three, exact |
| P3 | `train_enum.sh` x2 + `train_check.py` | 238 cells x D, K on each arm; `I17_TRAIN_PASS`: no missing or capped row, no OTHER mover, the LEDGER movers are exactly `movers-train-r2.txt` |
| P4 | `train_enum.sh` (LANE_FILTER=hven-sqp) x2 + `always_gate.py` | arm A `always-default`; arm B `always-default`, `default`, and for k in {6, 8, 10, 13, 16, 20} `cap<k>-always-default`, `cap<k>-default`. `I17_ALWAYS_PASS` (r1's C1-C4) with `movers-always-r2.txt` and the adopted 2^10 |
| P5 | `mutations_r2.sh` | control clean; every mutation of section 5 fails at least one pinning test (`MUTATIONS_AS_DECLARED`); the tree is restored and rebuilt |
| P6 | `install_smoke.sh` | install smoke and export contract rc 0 on the default and the clang++ front ends |
| CI | `gh workflow run ci.yml --ref i17-fixes` | all four lanes green on the pushed head |

## 5. Mutations

`legs/mutate_i17_r2.py` (each anchor occurs exactly once, checked before the first rebuild). Killed iff at least one pinning test of `hven_sqp_tests` (filter in `mutations_r2.sh`:
r1's suites plus `CallerStepLimitSolve.*`) or `hven_trace_tests` (all) fails. All must-kill.

| id | what | killed by (dev) |
|---|---|---|
| L1-L7, S1-S4, C1, C2, D1-D4 | r1's seventeen (DECLARATION-I17.md section 5), re-anchored on the r2 code: L4 on `make_ladder_row`'s `lr.exhausted = !usable`, L6 on the shared `key_opt_ladder`, S1 on `limit_subproblem_step`, D3 on `set_dump_site` | `dev-r2/mutation/<id>.killers.txt` |
| I1a | the usable fallback's ladder row not built | `SqpSolverCertifiedFallback.ARungAOwnedMajorIsAnElasticRowOnTheDriversOwnHistory` |
| I1b | the exhausted fallback's `main_status` = the synthesized kInfeasible | `SqpSolverCertifiedFallback.T7...` (reference route) |
| I2 | limit pins not released (no kFree / z = 0 / tr_active) | `CallerStepLimitSolve.ALimitPinIsReportedFreeAndTheStepRegionLimited` |
| I4a | restoration's x-block not limited | `CallerStepLimitSolve.RestorationStepsStayWithinTheLimitInCallerUnits` |
| I4b | restoration limited without the parent's column factors | the same test (its kAlways case) |
| I5a | rename over the final name in place of the exclusive hard link | `QpDump.AnExistingFileOfTheNextNameIsNeverReplaced` |
| I5b | one solver token for every driver | `QpDump.TwoSolversOnTwoThreadsWriteDistinctFilesIntoOneDirectory` |
| I5c | the process token dropped (0) | `QpDump.AnExistingFile...`, `QpDump.EveryWalkSolveRoundTrips...` |
| I6 | the lifetime run count dropped from the restoration ledger prefix | `QpDump.RestorationQpsAreRecordedInTheAttachedLedger` (second solve) |

## 6. Order, STOP and ON FAIL

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-i17/legs/orchestrate_r2.sh 0e5eaa8aeda30fdab5f378eb22ca8c85839d5df4 8690142108ae8bb0c00df64a4f65fd4696b96ca7a1765e2ed784b514b72ad24d cbb53c108ed7abd55f986ed372b2bf7f20d2c7f777ba29323c130e5c90326b1b 903a2cd5778e7bc833cae1bc192727651664d11a217955dfc7f7020c03b32bb8 d7b8f78c4aed7abcca2ecd804350d9cd94cdc7624065893a095622d6ed806a23 3954 \
  > docs/notes/data/2026-11-m8-i17/orchestrate-r2.out 2>&1   # leg outputs under gate-r2/
```

A failed leg stops the chain; the fix lands and the declaration is re-derived as `DECLARATION-I17-r3.md`. The branch is pushed (never `m8`) after the chain passes;
CI on the pushed head must show all four lanes green, and a red lane is a successor round. Nothing in this directory is edited after this commit except by adding
the r2 gate outputs and `RESULTS-r2.md`.
