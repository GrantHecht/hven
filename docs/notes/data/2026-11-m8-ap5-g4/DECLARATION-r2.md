# M8 AP-5 G4 -- DECLARATION-r2 (successor; committed before any r2 leg; never edited afterwards)

## Why a successor
The r1 chain under DECLARATION.md (evidence commit 3dfb91117) met every criterion but M: mutation e (the SSN poll ticks but
the loop continues) SURVIVED where it was declared must-kill. Cause: T4's SSN fire point was the last poll of its SSN solve
(the solve converged at the residual test right after it), so an engine that ignored the stop polled no more there and the
driver's own post-dispatch check produced the same result. The test was weak, not the code.

## The change (code head 1cc9001a347d8a700c9aa30d28bc7e6cf189a575 over 11a984f45, tests/ only)
StopPollSeam.T4ATokenStopInsideTheMainSubproblemReturnsXkOnEachQpMode now fires at the middle poll of the subproblem with the
most polls from major 1 on (premise asserted: at least three polls), which the unstopped kernel is guaranteed to follow with
another poll; `expect_no_poll_after_the_stop()` then sees any kernel that ignores the stop. Nothing under src/, include/,
bench/, examples/, cmake/ or scripts/ moved (`git diff --stat 11a984f45 1cc9001a3 -- src include bench` is empty). P1 count
unchanged: 3691 (no TEST added or removed).

## Everything re-runs
The corpus binary embeds the build's commit stamp (r1's arm B carries "f1d8a8d19767-dirty", built before the code commit), so
arm B is rebuilt and re-frozen at this head even though src/include/bench did not move: sha256
8e8918047ff063cf9bfd57d04fa71c89a5103199b3046613248a91e91d7e672c (build-g4/rel/bench/hven_sqp_corpus at 1cc9001a3). Arm A is
r1's file, unchanged (1cfd3715ecf26969362592edc50ea07645aa8bf2bbbd2dd5348f4cc592f9e8e9). P2 and IC are therefore re-run too,
not carried. Order unchanged: runtime/run_all.sh, ic/run_ic.sh, legs/chain.sh 1cc9001a347d8a700c9aa30d28bc7e6cf189a575 <arm B>.

## Criteria UNCHANGED from DECLARATION.md
P2 (runtime gate, all four modes) and IC (corpus B/A in [0.999, 1.001], cells in [0.995, 1.005]) as r1; P1 (3691, Release and Debug), P3 (replay vs G2 r3 + ARMED, 0 differences x3 each), P4 (interior unmoved/promoted/armed 0),
P5 (install smoke + export contract, both arms), P8 (TSan), M (control fails nothing; a, c, d, e, f, g, h, i, j, k, l, m, n,
o, r KILLED; b, p, q may survive). Outputs overwrite r1's summaries in runtime/, ic/, suites/, replay/, interior/, install-smoke/,
tsan/, mutation/; r1's are kept in git history at 3dfb91117. ON FAIL: stop, commit, report; a successor keeps these criteria.

## Pre-declaration runs (NOT evidence, disclosed)
The fixed test built and run on the Release seam binary (StopPollSeam.* 6/6; the first attempt picked major 0's
subproblem, whose cap comparison max_iterations = 0 is refused by SolveBudget, and was corrected before the commit). Release
and Debug trees rebuilt at 1cc9001a3 (every target) before this file was committed. No ctest, replay, interior, smoke, TSan
or mutation leg has run on r2.
