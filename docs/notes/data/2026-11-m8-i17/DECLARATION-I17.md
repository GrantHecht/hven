# M8 I17 -- weak columns under `column_scaling = kAlways` (hven issue 17): four fixes (DECLARATION)

Committed before the first gate leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by
`DECLARATION-I17-r2.md` (then -r3, ...), with this file and every output kept.

## 0. What this is

A declared change (CLAUDE.md section 7) answering GitHub issue 17 and the triage `.superpowers/i17/triage-2026-11-19.md` (owner-approved; API additions
before the M8 API lock). The default-configuration solve is unchanged: every new option defaults to the shipped behaviour or, for the one default that moves
(`column_scaling_factor_max`), moves it under `kAlways` only.

1. **Ladder diagnostics.** The exhausted elastic ladder no longer overwrites the main QP's status in the `sqp.major` row (`qp_status` stays the main QP's;
   `SqpIterationRow::ladder` carries `entry` -- verified_infeasible / working_set_termination / refused_certificate / ipqp_fallback --, `main_status`,
   `rung_status`, `rho`, `rungs`, `closed`, `reduced`, `promises_f`, `slack_l1`, `exhausted`, also on a usable ladder). The `elastic_exhausted` message and the console
   line state the same ("elastic ladder exhausted after 5 rungs (entered: verified infeasible in trust region; ...)"). Restoration-phase QPs are recorded in an attached
   `Ledger` (sub-driver prefix `<prefix>_resto<n>`): three lines (`sub.attach_ledger`).
2. **`SqpOptions::caller_step_limit`** (default 0 = off). `|dx_j| <= r (1 + |x_j|)` in CALLER units, divided by the column factor d_j into the engine's units and folded into the
   subproblem box (`qp.lower`/`qp.upper`) right after each subproblem build, so walk, ssn and ipqp, the SOC re-solve and the elastic ladder (built from that QP) all read it.
   Not applied inside the restoration sub-solve. A bound the iterate already stands beyond is kept.
3. **`SqpOptions::column_scaling_factor_max`** (default 1024 = 2^10), the clamp on the `kAlways` column factors alone; `scaling_factor_max` stays for rows and objective and
   still clamps `kAuto`'s column factors (the new option is not read under `kAuto`). The default is data-chosen (section 3). `docs/guides/snopt-like-configuration.md`
   says `kAlways` is not SNOPT's "Scale option 2", recommends physical caller scales with `kAuto` or `kOff`, and that console step/radius columns are in scaled units
   when column scaling fires.
4. **Failed-QP dump:** `SqpAdvancedOptions::qp_dump_dir` + `qp_dump_policy` (`kOff` default / `kFailed` / `kAll`, `SqpQpDumpPolicy`). The walk engine writes, after each qualifying solve, the
   existing "D4QP 1" text (`tests/qp/fixtures/e2-d4/FORMAT.txt`; loader `tests/qp/support/qp_dump.h`) as `qp<seq>_d<depth>_m<major>_<site>.txt` (site main, soc, retry, rung<k>); the
   tag is in the file name, the format is unchanged. A directory or file that cannot be created/written throws `std::runtime_error` (library code never prints). New TU
   `src/qp/qp_dump_writer.cpp` (kept out of the PCH: once per dump, opt-in; the source-count tripwire moves 59 -> 60).

## 1. The code and the binaries

- Base = arm A = `origin/m8` `64e384e2a4bf1f80a9dc2d72ee2b0bdf41911ed4` (detached worktree `../i17-base`, `build/`). Arm B = this worktree (branch `i17-fixes`), `build/` and `build-debug/`.
- **Code commit `5ee73653effaa719c102851c3b2633f9284182ad`.** `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg HEAD must equal it
  (`legs/orchestrate.sh`); commits after it add only evidence, runners and docs. All three build trees must be up to date (`ninja` on each, last line "no work to do"), checked by the orchestrator.
- clang 22.1.8 (`/usr/bin/clang++`), `-DHVEN_FP_MODE=SAFER_FAST`, bench + examples ON (`legs/build.sh`), MKL 2026.2, AMD Ryzen 7 5800X3D, Linux x86-64. Apple/Accelerate and Windows: by CI only.
- Frozen binaries (sha256, checked by the orchestrator before the first leg):
  corpus A `bd3d322803b1d98e90e6b622b39901da19a48991cdf1caf2a9673ee28e8c71c9`, corpus B `8391680bcddc3cc44de3f216800f76cedd26fe126d65ce8695fb2a80e3ddee96`;
  lane A `5cb4b4a2e575b894fb6d71044cec166c98ca9482b78bfa4fa159bb0a1272809f` (pin = arm A), lane B `48992df9e1893e14528b4279aedf5508de2e55cbf7b7647a9ffc6a8340de8d56`
  (pin `a780eb39b899643b1c6ca00fd7bdaa6f98680872`, whose code paths equal the code commit). Lanes: a PRIVATE clone `.scratch/hb` (git-ignored, never pushed) of hven-bench `main` `6d1a8a0` + `legs/hb_patch_i17.py`
  (label prefixes `never-`, `always-`, `cap<K>-`; arm A without the cap labels) + the RS include fix, one pin per arm (`legs/train_setup.sh`).
- CI is a further check (section 6).

## 2. Disclosures: what ran before this file (none is a leg)

- Dev runs at the code commit, outputs under `dev/`: Release and Debug `ctest`: "100% tests passed, 0 tests failed out of 3925" (`dev/ctest-summary-dev.txt`); replay A vs B, 27 cells x walk/ssn/ipm: **0 differences** in
  all three (`dev/replay-cmp-*.txt`); TRAIN 476 rows A vs B: **40 movers, all LEDGER class** (section 3; `dev/train_check-dev.txt`), 0 missing or capped, no row differs in any other key;
  the Always measurement (`dev/always_gate-dev.txt`, `dev/always_table-dev.txt`, `dev/auto_table-dev.txt`); mutation leg (`dev/mutation/`): control clean, all 17 killed.
- The first dev ctest after the first code commit had four failures that are the declared re-pins of section 3, and one trailing-whitespace failure of the options dump (an empty `qp_dump_dir` printed `name = `; now `unset`).
- The first draft of item 3 applied the new cap to `kAuto` too; the dev TRAIN showed no Auto row moves at any candidate, but the change then also altered which problems `kAuto` gates for unmeasured inputs, so the cap was restricted to `kAlways`
  (the owner's rule: Always-path behaviour only). The measurement below is of the final code.
- Instruments are modelled on `2026-11-m8-rs/` and `2026-11-m8-wa1/` (copied and re-pointed; each derives its paths from its own location or arguments and pre-flights fail-closed): `legs/{env,build,ctest,replay,train_setup,train_enum,cell,install_smoke,mutations,orchestrate}.sh`,
  `legs/{train_check,always_table,always_gate,mutate_i17,hb_patch_i17}.py`. They were self-tested on the dev data above (the dev movers lists ARE the declared lists in section 3: measured per cell, not carried).
- `build.build.log` / `build.configure.log` beside the build trees are untracked build logs and are not committed.

## 3. The expected movers (declared)

**Behaviour under the default configuration: none.** Replay: 0 differences. TRAIN: no row differs in any key but the ledger-derived harness columns below.

**TRAIN, 238 cells x {D = `default`, K = `never-default`} (`movers-train.txt`): 40 rows, all LEDGER class.** The lane sums the attached Ledger's records (`side.harness_qp_ledger_*`, `side.harness_qpsum_*`); the restoration phase's QPs are now ledgered, so a row
whose solve entered restoration (all 40 have `side.restoration_iters > 0` in arm A) reads more records. Status, iterations, objective, every other counter and every residual are equal. A row whose differences reach any key outside `side.harness_qp*` is an OTHER mover and
fails the leg whether listed or not. The `.optdump` of every hven-sqp row gains exactly the four lines of the new options at their defaults (`caller_step_limit`, `column_scaling_factor_max`, `advanced.qp_dump_dir`, `advanced.qp_dump_policy`), all else byte-equal; an ipm row's optdump is byte-equal.

**Always-forced TRAIN (hven-sqp, 119 cells; `movers-always.txt`): 17 rows, LEDGER class only** (label `always-default`, base clamp 1e12 -> 1024): every row equal to its arm-A row except the same ledger-derived columns.

**Test pins re-derived** (each in its test, commit `9719ca8f5` and the first code commit): `tests/solver/test_options_dump.cpp` (field arity SqpOptions 16 -> 18, SqpAdvancedOptions 27 -> 29, dump lines 86 -> 90, sizeof 608 -> 664 and 328 -> 368);
the trace schema: `tests/trace/golden/trace_v1/sqp_complete.jsonl` (three `sqp` blocks gain `"ladder":null`), `tests/trace/test_json_lines_trace_sink.cpp` literals (+ `"ladder":null`; arity pins `SqpIterationRow` 24 -> 25, `MessageTraceEvent` 14 -> 20), human goldens
`sqp_elastic_detail.txt` and `sqp_restoration_iterations.txt` (the `elastic_exhausted` line extended; on the exhausted row the `step` column reads `-` and the codes gain `n`, because the row's QP status is now the main QP's, not optimal); two tests whose exhausted-row assertion moved from "the final rung's status" to "the main QP's status, the rung's in `ladder`":
`SqpSolverGlobalization.SubproblemInfeasibilityEndsTheSolveThroughTheElasticTier`, `SqpSolverCertifiedFallback.T7TheEscapeBranchTernaryCONSUMESAnAttachedReportAtTheDriver`; three `ColumnScalingDriver` tests whose reference rule now reads the driver's `kAlways` clamp, and
`ScaledConvergenceGuard.ACallerUnitGradientAColumnFactorHidIsRefused`, whose premise (a factor of 2^-26) now sets `column_scaling_factor_max = 1e12` explicitly.
`docs/trace-schema-v1.md` records the key additions; `CHANGELOG.md`, `docs/guides/snopt-like-configuration.md` carry the docs.

## 4. The legs and their criteria (fixed here, before data)

All under `flock /tmp/box-build.lock`, serial, one process at a time; `pgrep -a hven_bench_cell` empty first; `MKL_NUM_THREADS=1`; wall-clock is not asserted (CLAUDE.md section 7); HOLDOUT is never run. `legs/orchestrate.sh <code> <corpus A> <corpus B> <lane A> <lane B> 3925` runs them in order and stops at the first failure.

| leg | instrument | criterion |
|---|---|---|
| P0 | orchestrate.sh | code paths of HEAD == the code commit and clean; base worktree's code paths == `64e384e2a`; the four binary sha256 match; the three build trees are up to date |
| P1 | `ctest.sh` Release, Debug (arm B) | each log reads "100% tests passed, 0 tests failed out of 3925" |
| P2 | `replay.sh` arm A, arm B | 27 cells x walk, ssn, ipm; `scripts/compare_replay.py` A vs B reads "differences: 0" in each of the three, exact |
| P3 | `train_enum.sh` x2 + `train_check.py` | 238 cells x D, K on each arm; `I17_TRAIN_PASS`: no missing or capped row, no OTHER mover, the LEDGER movers are exactly `movers-train.txt` |
| P4 | `train_enum.sh` (LANE_FILTER=hven-sqp) x2 + `always_gate.py` | arm A `always-default`; arm B `always-default`, `default`, and for k in {6, 8, 10, 13, 16, 20} `cap<k>-always-default`, `cap<k>-default`. `I17_ALWAYS_PASS`: C1 the ledger movers are exactly `movers-always.txt`, none OTHER; C2 Auto at every cap equals Auto at the default on every `.out` key (the clamp is not read under kAuto); C3 the smallest cap whose Always rows equal the adopted default's is 2^10 and none above it moves; C4 (recorded) the rows each smaller cap moves, with direction |
| P5 | `mutations.sh` | control clean; every mutation of section 5 fails at least one pinning test (`MUTATIONS_AS_DECLARED`); the tree is restored and rebuilt |
| P6 | `install_smoke.sh` | install smoke and export contract rc 0 on the default and the clang++ front ends |
| CI | `gh workflow run ci.yml --ref i17-fixes` if the push did not trigger it | all four lanes green on the pushed head |

The adoption rule for item 3 (stated before the gate's data, as it was before the dev data): the default is the smallest candidate power of two at which no TRAIN row under `kAlways` differs from the unclamped (1e12) rows; on the dev data that is 2^10 (2^8 moves HS99EXP 26 -> 27 iterations, neutral; 2^6 moves six rows, DEMBO7 optimal -> numerical_error is WORSE).
TRAIN is indifferent to every cap from 2^10 up (every Always row is byte-equal to the unclamped row, so the clamp binds on no TRAIN solve), so 2^10 is the tightest protective value the data permits; the issue's case needed about 2^15.

## 5. Mutations (`legs/mutate_i17.py`; each anchor occurs exactly once, checked before the first rebuild)

Must-kill (killed iff at least one pinning test of `hven_sqp_tests` (the filtered ladder, step-limit, cap, dump, scaling suites) or `hven_trace_tests` (all) fails):

| id | what | killed by (dev) |
|---|---|---|
| L1 | exhausted row's `qp_status` = the final rung's again | Globalization row test, TraceMessagesSqp exhaustion test, two human goldens |
| L2 | entry route of a verified-infeasible main QP misclassified | Globalization row test (+ goldens) |
| L3 | `rho_final` reports `rho_0` | `SqpSolverW2Fixtures.TheLadderReportCarriesTheFinalRungsRho` (+ goldens) |
| L4 | `exhausted` flag always false | Globalization, CertifiedFallback T7, TraceMessagesSqp |
| L5 | console route text altered | `ConsoleMessages.EachTemplateRendersItsOwnLine` (+ goldens) |
| L6 | JSON `ladder.main_status` writes the rung status | `JsonLinesTraceSink.ARowsLadderBlockIsAnObjectAndNullWithoutOne` |
| L7 | restoration sub-driver not attached to the ledger | `QpDump.RestorationQpsAreRecordedInTheAttachedLedger` |
| S1 | step limit never applied | `CallerStepLimit.*` (three) |
| S2 | not divided by the column factor | `CallerStepLimit` always-scaling and box tests |
| S3 | caller `|x|` read from the scaled iterate | `CallerStepLimit` box test |
| S4 | the declared bounds dropped from the box | `CallerStepLimit` box test |
| C1 | the `kAlways` clamp reads `scaling_factor_max` | `ColumnScalingFactorMax` + three `ColumnScalingDriver` tests |
| C2 | `kAuto` reads `column_scaling_factor_max` | `ColumnScalingFactorMax`, `ScaledConvergenceGuard.ABoxViolation...` |
| D1 | `kFailed` writes every solve | `QpDump.OffWritesNothingAndFailedWritesOnlyFailedSolves` |
| D2 | dump writes a negated `g` | `QpDump.EveryWalkSolveRoundTripsThroughTheLoaderToItsStatusAndObjective` |
| D3 | a ladder rung is tagged `main` | `QpDump.OffWritesNothing...` (reference-route pin; on the reference build) |
| D4 | the engine never writes | the four `QpDump`/`CallerStepLimit` box tests |

## 6. Order, STOP and ON FAIL

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-i17/legs/orchestrate.sh 5ee73653effaa719c102851c3b2633f9284182ad \
  bd3d322803b1d98e90e6b622b39901da19a48991cdf1caf2a9673ee28e8c71c9 8391680bcddc3cc44de3f216800f76cedd26fe126d65ce8695fb2a80e3ddee96 \
  5cb4b4a2e575b894fb6d71044cec166c98ca9482b78bfa4fa159bb0a1272809f 48992df9e1893e14528b4279aedf5508de2e55cbf7b7647a9ffc6a8340de8d56 3925 \
  > docs/notes/data/2026-11-m8-i17/orchestrate.out 2>&1   # leg outputs under gate/
```

A failed leg stops the chain; the fix lands, the declaration is re-derived as `DECLARATION-I17-r2.md` (the code commit, binaries, movers lists as measured on the new head), and the chain is re-run in full under the successor's names. The branch is pushed (never `m8`) after the chain passes; CI on the pushed head
must show all four lanes green, and a red lane is a successor round, not a footnote. Nothing in this directory is edited after the declaration commit except by adding the gate outputs.
