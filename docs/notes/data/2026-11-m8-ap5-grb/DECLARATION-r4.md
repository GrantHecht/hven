# M8 AP-5 G-RB rider r4 -- DECLARATION (remove `IpmOptions::best_iterate_criterion`; the shared best ordering is the only one; committed before any leg; never edited afterwards -- a failure is DECLARATION-r5.md)

## Premise
Owner ruling (Grant, 2026-11-17): the shared best ordering (feasibility first, banded at `common.feasibility_tol`, then the objective, strictly better only) is ratified for both engines, and the
old IPM ranking criterion is REMOVED entirely -- not kept as an option. This r4 is the proof addendum for the branch after the rebase and the rider; G-RB's r1-r3 and G-S's declarations stay frozen
as the record of the heads they proved (their heads no longer exist after the rebase: their evidence is the pre-rebase record, this addendum re-proves the new head).

Branch g-rb rebased (no conflicts) onto origin/m8 d7be9a4e3747ce58e99abceb23908fcebc005582 (that commit changed only tests/qp/test_qp_rank_repair.cpp, tests/drivers/test_trace_v1_fixtures.cpp,
.gitattributes). Rider code head (the new head under test) 683897c48e28250a1ecee7a89dd2c662a243b7be `feat(m8-ap5-grb-r4)`, on top of G-RB's feat/test commits and G-S's feat commit b456f4a44 (rebased shas)
and the G-S evidence commit 98052f2fe. G-RB code/test commits and G-S code are unchanged in content; only their shas moved with the rebase.

## What the rider removes
`IpmOptions::best_iterate_criterion`, `enum class IpmBestIterateCriterion {kEquality, kInequality, kKkt, kObjective, kFeasibilityFirst}` (include/hven/drivers/ipm_enums.h), its validation
(`enum_in_range`), its dump line and `enum_name`, the criterion switch in `IpmSolverImpl::track_best_iterate` (now only `detail::best_improves`), the doc comments that named it, every test use
(8 sites; the set-to-kObjective/kEquality lines are deleted, no replacement option exists), and the doc/CATCH-UP rows (docs/migration-0.2.md section G-RB gets an old -> new row: removed, the shared
ordering is always used). hven-bench: `grep best_iterate_criterion` over its sources (outside runs/): no use; only recorded options-dump artifacts under runs/ name it (historical).

## Declared movers
- Options dump: the IPM dump loses one line (`best_iterate_criterion = ...`): 67 -> 66 lines. SQP dump unchanged. Pins re-derived (declared here): `kIpmOptionsFields` 15 -> 14, `kIpmDumpLines` 67 -> 66,
  `sizeof(IpmOptions)` 480 -> 472 (Linux x86-64; observed by the compile, the value is the only axis that noticed). The `IpmDumpHeadIsVerbatim` head is unaffected (the removed line was not in it).
- Behaviour: none on any path that does not track a best. The IPM best-tracker already defaulted to the shared ordering since G-RB (kFeasibilityFirst was the default); a consumer that left the option
  alone sees no change from G-RB's head; one that set a single-measure value loses it (a compile error, not silent).
- Tests re-derived: BestIterate.AnIterateScoringTheOldSentinelValueIsRecorded, Limits.AWorkLimitExitFollowsTheSharedReturnBestRule (no override now), IpmSolverEntry best-iterate rising-objective pin,
  SqpStopPollSeams (IPM arm of the stop-poll fixture), ReturnBestForcedExits default test (the criterion assertion removed), Callback.IpmTerminalEventDescribesTheReturnedPointUnderReturnBest
  (its fixture needed an early best that the single-measure override used to manufacture; under the shared ordering it moves to HS071 with `common.feasibility_tol = 30` and the acceptable equality and
  inequality tolerances = 30, a band that ties every iterate so the objective decides -- the substitution is asserted to fire, so the pin stays non-vacuous; the now-unused RisingObjectiveProblem fixture is deleted).

## Runtime gate
NOT REQUIRED: the unarmed path is unchanged. The tracker comparison runs only when the tracker is armed (`best_tracking_armed()`); the removed switch sat inside that armed-only function, and the
default (all-off) solve executes the same per-iteration code as arm A. Dump/validation changes are once per solve.

## Arms and binaries
Arm A = origin/m8 d7be9a4e3 (`git archive` build, legs/prebuild.sh; build-gs/armA), build-gs/armAbin/hven_sqp_corpus sha256 33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0
(identical to G-RB's arm A binary, built from 0cfcec52f: the intervening origin/m8 commit touched tests and .gitattributes only).
Arm B = build-gs/rel/bench/hven_sqp_corpus at 683897c48, frozen by legs/arm_b.sh to build-gs/armBbin, sha256 a3343c1112d0b9741e87dc34134a04d3b355c6c5ea66e437794c698aa08c1a18.
Trees build-gs/rel and build-gs/dbg (clang++, SAFER_FAST, all targets), GIT_DIR/GIT_WORK_TREE at configure, GIT_CEILING_DIRECTORIES = worktree root, googletest from the main checkout. (The directory name
build-gs is kept from G-S; it is rebuilt at the new head.)

## Proofs and pass criteria (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first as its own command, setsid -w, foreground, MKL_NUM_THREADS=1, ctest -j4)
P1  full ctest Release and Debug at the code head: 100% passed.
P3  replay 27 cells x (walk, ssn, ipm): arm A vs arm B EXACT, "differences: 0" x3 (the mover set is EMPTY: return-best is off in every cell, and G-S's cells are interior-only); ARMED (--callback count +
    unfired token) vs unarmed arm B: 0 x3.
P4  interior (43 cells): arm A vs arm B: the differing cells are exactly G-S's two declared ones (infeas2_spike/MakeParameter/stalled, infeas2_stationary/MakeParameter/resto_infeasible), each in exactly
    `status` and `phases` by G-S's declared map (legs/interior_movers.py, MOVERS_VERDICT PASS); against the G-S baseline (bench/baselines/2026-11-m8-ap5-gs-ipm-leg/interior_baseline.csv, under its own
    invocation): EXACT, 0 differences; armed (B vs B armed): 0.
P5  install smoke, default and clang front ends, each followed by check_export_contract.sh: pass x2.
M   mutations (Release `git archive` copy of the code head; hven_sqp_seam_tests, hven_drivers_tests, hven_interior_tests in full, hven_sqp_tests filtered to Continuation.*:WarmStart.*:SqpDriverNonfiniteRollback.*:
    QpModes/EqualityBasisModes.*, each --gtest_fail_fast; control fails nothing; each id alone). MUST BE KILLED by a failing test:
      o1  the order compares f before the violation        o2  non-strict improvement        o3  no tolerance band
      x1  NEW: the removed criterion path reinstated in the IPM tracker (measure = equality residual alone, comparison = `<=` ties: the old default `kEquality`).
    "An IPM tracker that still honours a removed criterion path" is REPRESENTABLE only as x1 (there is no option left to honour, so a mutation cannot read one); x1 is the reinstated path, and kill = a test
    that distinguishes the shared ordering from the old default.
Not run: TSan, goldens, the runtime gate (above), HOLDOUT (never).

## Pre-declaration runs (NOT evidence, disclosed)
Dev Release builds of the rider and a full Release ctest (one failure, Callback.IpmTerminalEventDescribesTheReturnedPointUnderReturnBest: the fixture re-derived above, found by that run, plus the
options-dump compile pins found by the build); a one-off x1 dry-run on the dev tree (Release hven_drivers_tests, hven_interior_tests, hven_sqp_seam_tests): killed by Limits.AWorkLimitExitFollowsTheSharedReturnBestRule
and the re-derived callback test, the source restored from a saved copy (diff-clean); legs/mutate.py dry-applied to scratch copies (every id matches exactly once); bash -n / py_compile on every instrument; the
two arm binaries and both trees built. No ctest-under-the-chain, replay, interior, install or mutation leg has run.

## Order
r4/legs/chain.sh 683897c48e28250a1ecee7a89dd2c662a243b7be a3343c1112d0b9741e87dc34134a04d3b355c6c5ea66e437794c698aa08c1a18 33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0: ctest rel, dbg ->
replay -> interior -> install smoke -> mutations -> mutation verdict; STOPS at the first failed leg. Evidence under r4/ (suites/ replay/ interior/ install-smoke/ mutation/), VERDICTS-r4.txt, MANIFEST-r4.txt;
raw dumps under the ignored .scratch/r4-*. ON FAIL: stop, commit the evidence, report; DECLARATION-r5.md.
