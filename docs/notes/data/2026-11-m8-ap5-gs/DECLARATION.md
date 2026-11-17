# M8 AP-5 G-S -- DECLARATION (the interior-point restoration-locally-infeasible exit is kInfeasible; a declared status event; committed before any leg; never edited afterwards -- a failure is DECLARATION-r2.md)

## Change
Code head 9f333b756d99faa027dfe887347e45e2e4ad2a96 (`feat(m8-ap5-gs)`) on the G-RB branch head (g-rb; G-RB code head 8e6e07310d51e5e22f7f3944f3ba4d900bcbc758, its commits all land before this one, so
G-S reverts alone). Design: docs/notes/2026-11-m8-ap5-interface-design.md section 5 row G-S, Q6 (owner answer: yes, as its own declared status event), section 2.5 O1. Migration
section G-S; consumer list CATCH-UP.txt.
- `resolve_ipm_phase_status`: IpmPhaseStop::kRestorationLocallyInfeasible resolves kInfeasible (was kStalled); kStageStalled stays kStalled. `infeasibility_certified` stays false (the engine
  claims nothing about the model): the exit is **kInfeasible, uncertified**. The message for that exit reads `locally infeasible (uncertified: restoration reached a locally infeasible point)`;
  the phase report's status, the `ipm.phase.exit` event's status and the console exit line follow the resolved status. `stop_reason` is unchanged.
- **kInfeasible exits never substitute the best iterate**, certified or not (G-RB's rule, `detail::return_best_applies`, reads the RESOLVED status). So the relabelled exit no longer substitutes
  under return-best, where it did as a stall; a return-best-on cell is the only thing this touches beyond the status word.
- The reporting order is unchanged (kStalled 6 < kInfeasible 7 < kDiverging 8): `run_phase_sequence` continues past the exit exactly as before (its short-circuit is >= kDiverging).

## THE AFFECTED CORPUS CELLS (counted before the first leg, from the committed pins and the G-RB r3 captures; both counts are re-measured by the legs)
- Interior leg (`hven_sqp_corpus --engine interior`, 43 rows; promoted baseline bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv): **2 rows**, exactly those whose stop_reason is
  restoration_locally_infeasible (and, in this baseline, the only `stalled` rows):
    1. infeas2_spike/MakeParameter/stalled              status stalled -> infeasible; phases `kSolve:stalled:123` -> `kSolve:infeasible:123`
    2. infeas2_stationary/MakeParameter/resto_infeasible status stalled -> infeasible; phases `kOptimize:stalled:22` -> `kOptimize:infeasible:22`
  The other 41 interior rows (39 optimal, 2 max_iter with stop_reason none) are unmoved.
- Replay corpus (27 cells x walk, ssn, ipm): **0 cells**. The walk and ssn engines are SQP. All 27 IPM cells are `Optimal` (G-RB r3's grb-ipm-B.csv, and G7's before it); none reaches restoration.
- Points are unchanged: every column except `status` and the status word inside `phases` is bit-identical on those two rows (the re-derivation below is the proof, EXACT otherwise).
- Declared column map (legs/status_map.py, fail-closed): a row with stop_reason restoration_locally_infeasible and status `stalled`: status -> `infeasible`, the `:stalled:` token of `phases` ->
  `:infeasible:`. It produced the new baseline bench/baselines/2026-11-m8-ap5-gs-ipm-leg/interior_baseline.csv (G1's stays FROZEN and unedited; the new file's banner says so;
  tests/sqp/CMakeLists.txt reads the new one). Difference from its predecessor: 2 rows x 2 columns, verified with a diff of the non-comment lines before this declaration.

## Declared movers (tests, goldens)
Re-derived by this event (each pinned the stall label on the restoration-locally-infeasible door): IpmStopReason.{ARestorationLocalInfeasibilityIsRecordedAsSuch,
TheLocallyInfeasibleDoorFiresItsRowOnBothStreams, TheDoorsMarkerFollowsItsOwnIterLineOnALiveSolve, TheLocallyInfeasibleDoorStillPrintsNoSolutionFoundAndCarriesItsMessage (status key `infeasible`),
TheProximalArmLeavesRestorationWhereTheL1ArmDeclaresInfeasibility}; IpmTrace.TheLocallyInfeasibleDoorWritesRestorationExitedWithSourceLocallyInfeasible;
SolveStatus.IpmPhaseResolutionSplitsTheCapExitByStopReason (the resolution table; the reachability loop now says kInfeasible exactly for cap-verdict + restoration door);
SolveResult.AnActiveRestorationExitEmptiesTheResidualBlocks (+ not certified); HumanTraceGoldens.TheRestorationFixture (tests/drivers/golden/human_trace/ipm_restoration_iterations.txt, one `exit` line);
tests/sqp/test_corpus_cells.cpp's expected interior exit rows (status `infeasible`). New: ReturnBestForcedExits.TheIpmRestorationLocallyInfeasibleExitIsInfeasibleAndNeverSubstitutes (all three
return-best values: kInfeasible, uncertified, message, no substitution, best recorded before restoration). The full Release suite at this head ran clean before this declaration (pre-declaration, below).

## Runtime gate
NOT REQUIRED. The relabel is one switch arm in the per-phase status resolution (once per phase, at its exit) and a message string; no per-iteration code changed.

## Arms and binaries
Arm A = the G-RB code head 8e6e07310d51e5e22f7f3944f3ba4d900bcbc758, `git archive` build (legs/prebuild.sh; build-gs/armA), build-gs/armAbin/hven_sqp_corpus sha256
460104d7461e480dc8f7e8a4f03ec2a9532c9f9bdf71931d5a9552c538c81b17.
Arm B = build-gs/rel/bench/hven_sqp_corpus at 9f333b756d99faa027dfe887347e45e2e4ad2a96, frozen by legs/arm_b.sh to build-gs/armBbin, sha256
2128e7bc32c2d9b6667d409b34a86ac2867c9edf5f69fcb25f0e0513eca99d27.
Trees build-gs/rel, build-gs/dbg (all targets, clang++, SAFER_FAST), GIT_DIR/GIT_WORK_TREE at configure, GIT_CEILING_DIRECTORIES = the worktree root, googletest from the main checkout.

## Proofs and pass criteria (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first as its own command, setsid -w, foreground, MKL_NUM_THREADS=1, ctest -j4)
P1  ctest Release and Debug at the code head: 100% passed.
P3  replay 27 cells x (walk, ssn, ipm): arm A vs arm B, EXACT, "differences: 0" x3 (the declared mover set in the replay corpus is EMPTY); ARMED (--callback count + unfired token) vs unarmed arm B 0 x3.
P4  interior (43 cells): legs/interior_movers.py over arm A's and arm B's CSVs (every non-wall column): PASS only if the differing cells are exactly the two declared ones, each differing in exactly
    `status` and `phases` by the declared map (MOVERS_VERDICT PASS); promoted: the NEW baseline vs arm B under the baseline's own invocation, EXACT, 0 differences (this is the status column
    re-derivation: the declared map applied to the old baseline equals what the G-S binary writes); armed (B vs B armed) 0.
P5  install smoke, default and clang front ends, each followed by check_export_contract.sh: pass x2.
M   mutations (a Release `git archive` copy of the code head; hven_sqp_seam_tests, hven_drivers_tests, hven_interior_tests in full, hven_sqp_tests filtered to JsonLinesTraceSink.*, each --gtest_fail_fast;
    the control fails nothing; each id alone). MUST BE KILLED by a failing test:
      g1  the relabel is missed (the restoration door resolves kStalled)      g2  the relabelled exit claims a certificate (infeasibility_certified true)
      g3  the relabel is applied to the stage-stalled door too                g4  the message keeps the generic text
      g5  the IPM substitution reads the raw verdict (the relabelled exit substitutes again)
    (g2 and g3 are "the relabel applied where it must not be": a certificate it does not hold, a door it does not own.)
Not run: TSan, goldens beyond the one above, the runtime gate, HOLDOUT (never).

## Order
legs/chain.sh 9f333b756d99faa027dfe887347e45e2e4ad2a96 2128e7bc32c2d9b6667d409b34a86ac2867c9edf5f69fcb25f0e0513eca99d27 460104d7461e480dc8f7e8a4f03ec2a9532c9f9bdf71931d5a9552c538c81b17: ctest rel, dbg ->
replay -> interior (movers, promoted, armed) -> install smoke -> mutations -> mutation verdict; STOPS at the first failed leg. Evidence: suites/, replay/, interior/, install-smoke/, mutation/,
VERDICTS.txt; raw dumps under the ignored .scratch/gs-*, MANIFEST.txt committed. ON FAIL: stop, commit the evidence, report; DECLARATION-r2.md.

## hven-bench (outside this repository; not an hven proof)
No code change. The lane's `normalise_outcome` already maps kInfeasible to `infeasible` (kStalled to `error`), so an IPM-lane cell on this exit changes its outcome column from `error` to `infeasible`:
a column-value change for that lane, reported in VERDICTS (the comments near lanes/hven/hven_bench_cell.cpp:1521 describe the engines' infeasible reporting and may want a line).

## Pre-declaration runs (NOT evidence, disclosed)
A dev Release build and full ctest at the first G-S edit (9 failures: the tests re-derived above, found by that run), then the full Release ctest at this head (clean); the two arm binaries and both trees
built; legs/status_map.py run on the G1 baseline (2 rows) and on G-RB r3's interior capture, legs/interior_movers.py dry-run on that pair (PASS) and on an identical pair against one declared cell (FAIL, as it
must); legs/mutate.py dry-applied to a scratch copy (every id matches exactly once); bash -n / py_compile on every instrument. No replay, interior, install, ctest-at-head-under-the-chain or mutation leg has run.
