# M8 AP-5 G3 -- DECLARATION-r4 (supersedes DECLARATION-r3.md; committed before any r4 leg; never edited afterwards)

## Why a successor
r3 (DECLARATION-r3.md, code head 07ef800e7 over origin/m8 ec84cffad; VERDICTS-r3.txt, evidence committed at 216497525) met P0, P1 (3712 Release and
Debug), P2a, P2b, P3 (0 differences x3, ARMED 0 x3), P4 (0 x3), P5 and the mutation criterion (control clean; every must-kill id killed, incl. the new w x y z B;
A survived as declared), and did NOT meet P8: the TSan-instrumented drivers binary exited 1 with 0 TSan reports. Cause (reproduced in the tsan tree, 1 test
failing, value printed): the new test BestIterate.IpmReducedObjectiveScaledBestIterateEqualsItsEventsSnapshot compared best_iterate.mu with the matching event's
mu BIT FOR BIT; both are `mu / scale` evaluated at two sites, and the sanitizer build rounds them one ulp apart (8.0175348548365961e-05 vs
8.0175348548365948e-05, objective_scale 7). A test flaw, not a counter, runtime or semantic fault: the two values are the same barrier parameter.

## The change (code head 062fadba2171df5294714b902e82488a972bfd39 over 07ef800e7cb6e66af6bc7114defde83b745cfc69)
One test file: tests/drivers/test_best_iterate.cpp -- the mu comparison inside expect_snapshot_equals is EXPECT_NEAR with relative tolerance 1e-14 (the other fields stay
bit-equal; the same test passes bit-equal on f, x, lambda, z in the sanitizer build). src/, include/, bench/, examples/, cmake/, scripts/ and CMakeLists.txt are
byte-identical to 07ef800e7 (and so is the code of arm B). Everything else of DECLARATION-r3.md -- the riders M1/S1..S6, the expected movers (NONE), the
deviations, the P-set and the mutation table (thirty-one ids; must-kill a b c d e f g h i j k l m n o p q r s u v w x y z B, one of t/u, t and A reportable) -- stands unchanged.

## Tests (P1 count)
"100% tests passed, 0 tests failed out of 3712", Release and Debug: no TEST added or removed since r3 (`git diff 07ef800e7 HEAD -- tests | grep '^[+-]TEST'` is empty).
P8's count reads 91.

## Binaries and trees (git-ignored build-g3/)
Arm A  build-g3/armAbin/hven_sqp_corpus, unchanged from r3 (origin/m8 ec84cffaded18c3b571cb56895dab0e2b19d531a)
       sha256 91f4ee0066b990a2ebf74f38b73fbfb8aa7e7c7251b811eed63107c1fe39a0d9
Arm B4 = arm B3: runtime-r4/run_all.sh copies build-g3/rel/bench/hven_sqp_corpus, which no source change since r3 has touched (ninja: no work to do)
       sha256 8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78
Run order: runtime-r4/run_all.sh <armB4> <armA>, runtime-r4/run_attached.sh <armB4> <armA>, then legs-r4/chain.sh 062fadba2171df5294714b902e82488a972bfd39 <armB4>, each under
`flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, wall-clock legs serial and alone. Evidence: runtime-r4/, suites-r4/, replay-r4/, interior-r4/,
install-smoke-r4/, tsan-r4/, mutation-r4/ here; raw dumps under the ignored .scratch/ with MANIFEST-r4.txt (sha256, size, path).

## Instrument changes (declared before data)
legs-r4/ and runtime-r4/ are copies of legs-r3/ and runtime-r3/ with every output directory suffixed -r4 (scratch trees build-g3/mut-src-r4, mut-r4; raw .scratch/g3-mutation-r4).
The G2 r3 replay-row path in legs-r4/replay.sh (../../2026-11-m8-ap5-g2/replay-r3/) is kept as is: the suffix rewrite touched it and it was restored. mutate.py and the
mutation list are r3's.

## Proofs and pass criteria
As DECLARATION-r3.md, unchanged: P0 (shas above), P1 3712 x2, P2a, P2b, P3 (vs the committed G2 r3 rows, and ARMED), P4, P5 (default, clang), P8 (91 tests, exit 0, 0 reports),
M (as r3). Expected movers: none.
ON FAIL  stop, commit the evidence, report; a successor -rN keeps these criteria unchanged.

## Pre-declaration runs (NOT evidence, disclosed)
- In the tsan tree and the Release dev tree: BestIterate.* (19 tests) all pass at the new test (tsan: the previously failing test OK; rel: 19/19). The Debug dev tree was rebuilt
  at the head (build-g3/b4.sh); no ctest of it was run before the chain.
- Nothing else of r4 has run: no ctest leg, runtime, replay, interior, install-smoke, TSan or mutation leg.
