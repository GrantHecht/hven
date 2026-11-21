# M8 AP-5 G3 -- DECLARATION-r2 (supersedes DECLARATION.md; committed before any r2 leg; never edited afterwards)

## Why a successor
The r1 chain (DECLARATION.md, code head 8e8f2e652; evidence committed at ab0c4feab, VERDICTS.txt) met P0, P1 (3703 Release and Debug),
P2a, P2b, P3 (0 differences x3 and ARMED 0 x3), P4, P5 and P8, and did NOT meet the mutation criterion: mutation e (ipm best kept on an
optimal exit) survived and was on the must-be-killed list. Diagnosis: the clear it removes, at the end of run_phase_sequence, is dead
code -- alg_impl's exit publishes a best only for a phase whose ExitCode is neither kOptimal nor kAcceptable, and a phase's final status
is never optimal when its ExitCode is not. j (token-only step norm: the r1 test compared two callback-less arms), s (SQP kFinishCurrent
arm) and t (one of the two SQP restoration-return arms) also survived; j, s and t were permitted to. r1's evidence is untouched.
The fix changes code and tests, so r1's evidence does not cover the new head; the whole chain is re-run.

## The change (code head eebe2179df2011b39847993ceab9dbddd048e93a over 8e8f2e65240c597b0ec9b8c998d6e61cd784013e, on origin/m8 f1d8a8d19)
- src/drivers/ipm_solver.cpp: the dead final clear removed (one `if` at a once-per-solve site; nothing in an iteration loop moved).
- tests/drivers/test_best_iterate.cpp: the token-only test compares the tracker's best, step norm included, with a callback-armed run
  and with the option's, and asserts the IPM step norm is positive once a step reached the best (kills j).
- tests/drivers/test_callback.cpp: two TESTs -- SqpStopReturnBestOnARowThatIsNotAnAcceptedStepReturnsTheBest (the ordinary
  terminal exit; kills s) and SqpStopReturnBestOnTheCertifyingRestorationReturnsTheBestAndNoCertificate (the circle fixture; the
  other restoration-return exit).
- include/hven/drivers/solve_result.h: best_iterate's comment says it is also absent when the tracker recorded nothing.
Source/include/tests/bench/examples/cmake/scripts at the chain's guard are identical to eebe2179d. The deviations D-a .. D-g of
DECLARATION.md stand unchanged.

## Instrument changes (declared before data)
legs-r2/ and runtime-r2/ are copies of legs/ and runtime/ with every output directory suffixed -r2 (suites-r2, replay-r2, interior-r2,
install-smoke-r2, tsan-r2, mutation-r2, runtime-r2/*), the mutation scratch tree moved to build-g3/mut-src-r2 and build-g3/mut-r2, raw
mutation dumps to .scratch/g3-mutation-r2, and `MUT_IDS` (an optional subset; unset means every id) added to mutations.sh. mutate.py:
mutation e is RETARGETED to the surviving code's own guard -- the tail condition `ExitCode != kOptimal && ExitCode != kAcceptable` in
alg_impl's publish call is dropped -- since the text r1's e removed no longer exists. Nothing else in mutate.py moves.

## Tests (P1 count)
ctest as it counts them: "100% tests passed, 0 tests failed out of 3705", Release and Debug (r1: 3703). +2 verified from the diff over
8e8f2e652 (`git diff 8e8f2e652 HEAD -- tests | grep '^[+-]TEST'`): the two Callback TESTs above. No test modified-and-renamed or removed.

## Pre-declaration runs (NOT evidence, disclosed)
- Targeted gtest runs of BestIterate.* and Callback.SqpStopReturnBest* on the Release dev tree at the code head: all pass (16 tests).
  Before them, the new s-test's first form (an HS071 fixture) found no non-accepted row, and the step-norm assertion was first written
  for both engines (the SQP's best is the last point, whose own step is 0); both were corrected before the commit.
- legs-r2/mutations.sh with MUT_IDS="e j s t" on a scratch copy of eebe2179d (control, then the four): the control fails nothing, and e, j, s and t are each KILLED (e by
  BestIterate.AnOptimalSolveCarriesNoBestIterate and two more, j by BestIterate.ATokenAloneArmsTheTracker, s by
  Callback.SqpStopReturnBestOnARowThatIsNotAnAcceptedStepReturnsTheBest, t by the certifying-restoration TEST). The scratch
  tree build-g3/mut-src-r2 / mut-r2 it built is the one the chain's own mutation step reuses (each mutated file is restored from the
  head after every mutant).
- Both dev trees (build-g3/rel, build-g3/dbg) rebuilt at the code head by legs-r2/build.sh; arm B2 frozen from build-g3/rel/bench.
- No ctest leg, replay, interior, runtime, install-smoke or TSan leg of r2 has run.

## Binaries and trees (git-ignored build-g3/)
Arm A  build-g3/armAbin/hven_sqp_corpus  (origin/m8 f1d8a8d19, unchanged from r1)
       sha256 77c3e021d694160a61d4b8810c5fe3bbe6ea489e20188c94116da7bf7787bae5
Arm B2 build-g3/armBbin/hven_sqp_corpus  frozen (cp -p) from build-g3/rel/bench/hven_sqp_corpus at eebe2179d (r1's arm B kept as armBbin-r1)
       sha256 0c346f0047bce71b0812f594055626d1691f2527d030c2a019788a8e9b8697be
Run order, as in DECLARATION.md: runtime-r2/run_all.sh <armB2> <armA>, runtime-r2/run_attached.sh <armB2> <armA>, then
legs-r2/chain.sh eebe2179df2011b39847993ceab9dbddd048e93a <armB2>, each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty
first, wall-clock legs serial and alone. Evidence is written to runtime-r2/, suites-r2/, replay-r2/, interior-r2/, install-smoke-r2/,
tsan-r2/, mutation-r2/ here; raw per-run dumps under the ignored .scratch/ with MANIFEST-r2.txt.

## Proofs and pass criteria (r1's carry over except where this section says otherwise)
P0 arm A / arm B2 sha256 above.
P1 ctest Release and Debug: "100% tests passed, 0 tests failed out of 3705".
P2a, P2b, P3, P4, P5, P8: as in DECLARATION.md, on arm B2 and the r2 head (P8's filter unchanged; its test count will read 85).
   P3's comparison is still against the COMMITTED G2 r3 rows. P2 is re-run in full although the diff touches no iteration loop, so that
   the evidence is on the final head.
M  the mutation table of DECLARATION.md (a..v, the same twenty-two, e as retargeted above): the control fails nothing. MUST be killed:
   a b c d e f g h i j k l m n o p q r s u v. At least one of t, u must be killed (u already is, in r1); t may survive and is reported.
   (r1's must-kill set plus j, s and the retargeted e.)
ON FAIL  stop, commit the evidence, report; a successor -rN keeps these criteria unchanged.
