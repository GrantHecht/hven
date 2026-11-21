# M8 AP-5 G2 -- DECLARATION-r2 (supersedes DECLARATION.md; committed before any leg; never edited afterwards)

## Why a successor
DECLARATION.md named code head 99cc2d2ac on origin/m8 286fb5398. Two things happened before any evidence existed:
1. runtime/run_all.sh aborted at its pre-flight (rc 7, STOP_MAP_NOT_FOUND): the G1 column-map file was never copied into
   legs/. No timed leg, replay, interior or ctest leg produced evidence under DECLARATION.md (the chain that followed in the
   same script was stopped by me, by exact pid, on a message that origin/m8 had moved; its partial output was deleted).
2. origin/m8 moved to 7bb635605 (dense-gate fix on top of the walk-cap fix); the coordinator directed a rebase onto it.
Both change the head and the arm A binary, so every sha below is new. Nothing in DECLARATION.md's criteria changes.

## Instrument fix (declared before data)
runtime/analyze_r11.py: the G1 column-map step (arm A's interior CSV through legs/stop_reason_map.py) is REMOVED. G2 changes no
column and arm A (7bb635605) already carries G1's stop-reason spelling, so the map is the identity; applying G1's map
(old IpmStopReason vocabulary) to a post-G1 CSV would fail closed on 'callback_stop'. Interior counter identity is judged
arm A vs arm B directly. No other instrument change. (legs/interior.sh already had no map.)

## Unchanged from DECLARATION.md
Spec, the change, deviations D-a .. D-f, proofs P0-P8 and their pass criteria, ON FAIL rule. Read DECLARATION.md for them;
only the numbers below differ.

## Code head and base
Code head fe39616ddab6738a8640ad7a1906d9e32ed5db95 (the three G2 code commits + two test-pin commits, rebased), on origin/m8 7bb635605. Source/include/tests/
bench/examples/cmake/scripts at the chain's guard are identical to fe39616ddab6738a8640ad7a1906d9e32ed5db95. Arm A = origin/m8 7bb635605 (the walk-cap and dense-gate
fixes are the base's and must not appear as G2 differences). The g2 diff to the base touches no file under src/linear,
src/kkt or src/qp and nothing the dense-gate commit changed (src/drivers/sqp_solver.cpp + ipm_solver.cpp + headers + bench
+ tests only).

## Pre-declaration runs (NOT evidence, disclosed)
Dev Release and Debug trees (build-g2/rel, build-g2/dbg), every target, on this head: ctest 100% of 3683 in both
(3671 of origin/m8 + the 12 G2 TESTs; ctest -N lists 3685, 2 Disabled). The 12-mutation table of DECLARATION.md was
re-run on this head with the identical result: a, b, c, e, f, g, h, j, l killed; d, i, k survive (recorded there).
The arm A tree was built (legs/prebuild.sh 7bb635605) before this file was committed; no replay, interior, runtime or
chain leg ran.

## Tests (P1 count)
ctest as it counts them: "100% tests passed, 0 tests failed out of 3683", Release and Debug.

## Binaries
Arm A  build-g2/armAbin/hven_sqp_corpus  sha256 d80ce1bfd2512b7e830d693eefae6b9d4da48c757add091b5e55bab5cfe31334
Arm B  build-g2/armBbin/hven_sqp_corpus  frozen (cp -p) from build-g2/rel/bench/hven_sqp_corpus
       sha256 bd63871d04868a8d3fe89e439060f19faedf406c05f056749e19c3d00848fc7b

## Runs
runtime/run_all.sh <armB> <armA> FIRST and alone, then legs/chain.sh fe39616ddab6738a8640ad7a1906d9e32ed5db95 <armB>, each under flock, pgrep first.
Evidence written to runtime/{interior,ipm,ssn,walk}, suites/, replay/, interior/, install-smoke/, tsan/ in this directory.
