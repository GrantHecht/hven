# M8 AP-5 G6 -- DECLARATION-r2 (successor to DECLARATION.md; committed before any r2 leg; never edited afterwards -- a failure is r3)

## Relation to r1
r1 (DECLARATION.md, evidence 77f3e10c7, VERDICTS.txt) failed ONE criterion: mutation leg M. c3, c4, c11, c12 survived (no test drove the second-order correction or the watchdog
counters through a real solve, nor read the whole-solve restoration counters) and p1 was an instrument fault (the mutation text did not compile). Every other r1 leg met its criterion
(P0, P1 3769 x2, P3 0 x3 + ARMED 0 x3, P4 0/0/0 + multi-phase-restoration count 0, P5, goldens-rename pairs 4 failures 0). r1's files stand untouched; r2 writes *-r2.

## Change over r1 (tests and an instrument only)
Code head 9cd52b284 on r1 code head 0423a7562:
- tests/drivers/test_ipm_counters.cpp: a Maratos fixture; `SecondOrderCorrectionsAreCountedAsSocStepsAndNotAsWatchdogArms` (soc_steps > 0 with max_second_order_corrections = 4 from the feasible start (cos .5, sin .5);
  watchdog_activations 0; recovery_depth_histogram[0] > 0; soc_steps 0 with SOC off) and `TheWatchdogArmsAreCountedAsWatchdogActivationsAndNotAsSocSteps` (funnel, SOC off, watchdog on, start 2 (cos 1.6, sin 1.6): watchdog_activations > 0, soc_steps 0; watchdog off: 0). The start points were
  found with a scratch sweep before this declaration (disclosed below).
- tests/interior/test_ipm_stop_reason.cpp: in `IpmStopReason.TheProximalArmLeavesRestorationWhereTheL1ArmDeclaresInfeasibility` the whole-solve restoration counters equal the one phase's values and iters > 0.
- legs/mutate.py: p1's replacement text fixed (`[[maybe_unused]] auto mut_unused_ =`). legs/chain-r2.sh, legs/mutations.sh (MUT_IDS, RAW_DIR env).
src, include, bench, examples, cmake, scripts: BYTE-IDENTICAL to r1's code head (proved by `git diff --quiet 0423a7562 9cd52b284 -- src include bench examples CMakeLists.txt cmake scripts`, re-run by chain-r2.sh at its start; the diff stat of tests is saved as carry-premise-r2.txt).

## Carried legs (not re-run) -- justification
P3 (replay + ARMED), P4 (interior), P5 (install smoke + export contract) and the goldens-rename check measure the library, the corpus binary and the install tree, none of which the r2 diff touches (tests/ only; the corpus binary builds from bench/ and src/, identical;
the goldens check diffs tests/ against 7c70be63a and the r2 tests add no quoted `last_kkt_info`/`kkt_factor_status` line). Their r1 results stand for code head 9cd52b284 on the identity proof above. Runtime gate P2: not applicable, as r1 declared.

## Re-run legs and criteria (flock /tmp/box-build.lock, `pgrep -a hven_bench_cell` first, setsid -w, foreground, MKL_NUM_THREADS=1, ctest -j4)
P1  ctest Release (build-g6/rel) and Debug (build-g6/dbg), rebuilt at the code head: 100% passed, 3771 tests (3769 + the two new TESTs).
M   mutations on the code head's Release scratch copy (legs/mutations.sh with MUT_IDS="c3 c4 c11 c12 p1"; the control first, which must fail nothing). MUST BE KILLED by a failing test: c3, c4, c11, c12, p1. The other r1 table rows stand (their killers
    are unchanged and only tests were added); c1b and c13 remain REPORTED and may survive.
Order: legs/chain-r2.sh 9cd52b284 0423a7562. Evidence: suites-r2/, mutation-r2/, VERDICTS-r2.txt, MANIFEST-r2.txt (raw dumps under the ignored .scratch/g6r2-*). ON FAIL: stop, commit, report; r3.

## hven-bench
Unchanged by r2 (branch g6-catchup, local, not pushed).

## Pre-declaration runs (NOT evidence, disclosed)
A scratch exploration test (deleted, never committed) swept SOC/watchdog options on HS071 and Maratos to find the starts above; the new tests and the extended stop-reason test were run alone in the Release tree (pass); Release and Debug trees rebuilt at the code head. No r2 leg has run.
