# M8 AP-5 G3 -- DECLARATION-r3 (supersedes DECLARATION-r2.md; committed before any r3 leg; never edited afterwards)

## Why a successor
r1 and r2 (code heads 8e8f2e652, eebe2179d over origin/m8 f1d8a8d19) met every criterion (r2: VERDICTS-r2.txt, CRITERIA_MET_ALL). An
independent review of the G3 diff then raised riders, and origin/m8 moved to ec84cffad (a tests-only CI portability fix). The riders change
code, tests and docs, so r2's evidence does not cover the new head: the whole chain is re-run on the rebased branch. r1/r2 evidence is untouched.

## The change (code head 07ef800e7cb6e66af6bc7114defde83b745cfc69 = ce53939e5 + one sqp test; g3 rebased onto origin/m8 ec84cffad)
Code (src/include) is identical at ce53939e5 and 07ef800e7; the second commit adds one TEST.
- M1 (must): the invariant `returned_best_iterate => best_iterate.has_value()` on both engines; stated once in include/hven/drivers/solve_result.h.
  IPM: publish_phase_best now publishes when the tracker is armed, a best is recorded, and (the best was returned OR the exit is neither kOptimal
  nor kAcceptable) -- so a kAcceptable that substituted publishes. returned_best_iterate = return_best_applied && best_recorded: a phase that
  left before the tracker recorded anything (a work limit at its first row) substituted nothing and no longer claims a best (the dbg ctest of the
  first dev build caught the segfault the unguarded form had there; pre-existing: such an exit printed "returned the best iterate").
- S1: the "recorded" sentinel (`BestCriteriaVal != 1.0e10`) is replaced by an explicit flag set inside track_best_iterate when a finite-criterion
  iterate is stored; a non-finite stored criterion yields to the next finite one. Reaching it: a criterion exactly 1e10 is reachable (test); a NaN
  at the forced i==0 record is NOT reachable (the top-of-loop screen aborts on a non-finite objective, gradient, KKT or rhs before tracking), so
  that half is unpinned by any test -- mutation A below.
- S2: SQP attach_best_iterate applies sweep_negative_face_prices to best_iterate's inequality multipliers (disclosure counters discarded, the
  primary's stay the primary's). IPM non-objective phase: the docs/migration-0.2.md claim is corrected to the exact rule (best_iterate.f is NaN where
  nothing measured the objective, while the primary f is the objective at the returned point); no code change there.
- S3: tests on scaled/reduced solves, compared to the matching event's snapshot() field by field including lambda and z (SQP row+column scaling;
  IPM objective_scale=7 with a fixed variable, on HS071's interleaved rows).
- S4: the dead `want_vectors` parameter of declared_point removed.
- S5: IPM tracker armed-ness latched at solve entry (best_tracking_armed_, set after the entry's pending-callback application). Found while
  testing: the SQP engine did NOT latch either (its arming read iteration_callback_ at every row); it now latches in SolveState::best_armed.
- S6: stale "Appended last..." notes in ipm_solver_impl.h removed; IterationSnapshot::elapsed_seconds and IterationEvent::elapsed_seconds default
  to NaN like the other unmeasured scalars.
Other: Hs071Problem lost `final` (the fixed-variable fixture derives from it). Deviations D-a .. D-g of DECLARATION.md stand.

## Expected movers (stated before any run)
NONE in P3 (replay, ARMED) and P4 (interior, three comparisons) and no counter anywhere. Reasoning: an unarmed solve does not enter the tracker and
publishes nothing (best_tracking_armed false => publish_phase_best(false)); the latches read the same truth as the live reads they replace for any
solve whose callback never uninstalls itself; the SQP sweep touches only best_iterate's row copy; the 1e10 flag differs from the sentinel only on a
criterion exactly 1e10; returned_best_iterate (not a replay column) differs only for a phase leaving before its first tracker record. If a cell
moves, it is not explained by these and the leg FAILS (successor -r4).

## Tests (P1 count)
"100% tests passed, 0 tests failed out of 3712", Release and Debug (r2: 3705): +7 TESTs, from the diff over eebe2179d
(`git diff eebe2179d HEAD -- tests | grep '^[+-]TEST'`): in tests/drivers/test_best_iterate.cpp AnAcceptableReturnBestSolvePublishesTheBestItSubstituted (M1),
AnIterateScoringTheOldSentinelValueIsRecorded (S1), SqpBestIterateInequalityMultipliersAreSignCorrect (S2, the invariant), SqpScaledBestIterateEqualsItsEventsSnapshot
and IpmReducedObjectiveScaledBestIterateEqualsItsEventsSnapshot (S3), ACallbackThatUninstallsItselfStillLeavesTheTrackedBest (S5); in tests/sqp/test_scale_smoke.cpp
F7ColdScaleSmoke.TheBestIteratesFacePricesAreSignRepaired (S2: N=1050, kIpqp, kStopReturnBest at iteration 2..4 -- the stop at 3 holds a -2.9e-6 price before the repair).
The existing BestIterate tests' `solve` helper now asserts the invariant on every result. No test removed or renamed. (3711 was the count at
ce53939e5, before the sqp test.)

## Binaries and trees (git-ignored build-g3/)
Arm A  build-g3/armAbin/hven_sqp_corpus  (origin/m8 ec84cffaded18c3b571cb56895dab0e2b19d531a, git archive, Release, legs/build.sh's flags)
       sha256 91f4ee0066b990a2ebf74f38b73fbfb8aa7e7c7251b811eed63107c1fe39a0d9
Arm B3 build-g3/armBbin/hven_sqp_corpus  copied by runtime-r3/run_all.sh from build-g3/rel/bench/hven_sqp_corpus at the code head
       sha256 8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78
Run order as in DECLARATION-r2.md, with the -r3 instruments: runtime-r3/run_all.sh <armB3> <armA>, runtime-r3/run_attached.sh <armB3> <armA>, then
legs-r3/chain.sh 07ef800e7cb6e66af6bc7114defde83b745cfc69 <armB3>, each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, wall-clock
legs serial and alone. Evidence: runtime-r3/, suites-r3/, replay-r3/, interior-r3/, install-smoke-r3/, tsan-r3/, mutation-r3/ here; raw dumps under the ignored
.scratch/ with MANIFEST-r3.txt (sha256, size, path of every dropped file).

## Instrument changes (declared before data)
legs-r3/ and runtime-r3/ are copies of legs-r2/ and runtime-r2/ with every output directory suffixed -r3 and the mutation scratch trees at
build-g3/mut-src-r3, mut-r3 (raw dumps .scratch/g3-mutation-r3). mutate.py: a, e and l are RETARGETED to the surviving text (the arming latch; the
publish condition; the SQP arming at solve entry); w, x, y, z, A, B are new. mutations.sh: default id list a..v w x y z A B; the control and y also run
hven_sqp_tests F7ColdScaleSmoke.TheBestIterates* (alongside r's ColumnScalingDriver.*). Nothing else moves. P3 still compares against the COMMITTED G2 r3 rows
(arm A moved by a tests-only commit; the rows stand).

## Proofs and pass criteria (r2's carry over except where stated)
P0 arm A / arm B3 sha256 above. P1 3712 x2. P2a, P2b, P3 (vs the committed rows, and ARMED), P4 (unmoved, promoted, armed), P5 (default and clang), P8 as in
DECLARATION.md, on arm B3 and the r3 head; P8's test count will read 91 (85 + the six new BestIterate tests). Expected movers: none (above).
M  the mutation table, thirty-one ids: the control fails nothing. MUST be killed: a b c d e f g h i j k l m n o p q r s u v w x y z B. At least one of t, u must
   be killed. t and A may survive and are reported. New: w ipm best not published on a substituted kAcceptable exit (M1); x ipm recorded flag keeps the 1e10 sentinel
   (S1); y sqp best_iterate inequality prices not sign-swept (S2); z ipm tracker armed-ness read live, not latched (S5); B sqp tracker armed-ness read live, not latched
   (S5, SQP); A ipm non-finite incumbent never yields to a finite criterion (S1's other half -- unreachable, may survive, reported as unpinned). e is now "ipm best
   published on an optimal exit".
ON FAIL  stop, commit the evidence, report; a successor -rN keeps these criteria unchanged.

## Pre-declaration runs (NOT evidence, disclosed)
- Dev trees build-g3/rel and build-g3/dbg built at ce53939e5 and ctest -j4 run: 3711/3711 Release and Debug, after the Limits.ALimitExitIgnoresReturnBest segfault the first
  form of M1 caused (fixed before the commit). Targeted BestIterate.*/Callback.* runs before that; failing first forms of three new tests were corrected (a sign
  convention misread in the S2/S3 tests; the SQP block of the S5 test, which then showed the SQP did not latch).
- legs-r3/mutations.sh with MUT_IDS="a e l w x y z A B" on a scratch copy of ce53939e5: control clean; a e l w x z B each killed; y SURVIVED (no driver-suite fixture
  reaches a wrong-signed iterate price: a 480-solve probe of HS071 across qp modes/Hessians/scaling found none) and A SURVIVED. y was then pinned by the sqp scale-smoke
  test above (the 07ef800e7 commit): a scratch run of mutant y on it fails exactly that test; the unmutated head passes it (Release 2.2 s, Debug 28.5 s).
- Arm A prebuilt (legs-r3/prebuild.sh) and arm B3's sha taken from the dev tree. No ctest leg, replay, interior, runtime, install-smoke, TSan or full mutation
  leg of r3 has run.
