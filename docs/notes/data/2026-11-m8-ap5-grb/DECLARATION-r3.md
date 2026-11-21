# M8 AP-5 G-RB -- DECLARATION-r3 (committed before any r3 leg; never edited afterwards)

DECLARATION.md (7184ee8af) and DECLARATION-r2.md (3f1f9ecd0) stand except for the items below. Why r3: the r2 chain ran every leg to its end and the mutation verdict was FAIL. r2's
ctest rel/dbg, replay (0 differences x3, armed 0 x3), interior, install smoke and the other nine must-kill mutations passed (evidence kept under r2/); two faults, both mine:
- n1 (the IPM non-finite abort substitutes under kOnLimit) SURVIVED: no test ran that door under kOnLimit (the abort tests used kOnAnyNonOptimal only; my fixture,
  PoisonedHs071, ends on a different door -- a poisoned Hessian ends at the factorization door, not the evaluation door n1 mutates). Fix: a test on the evaluation-failure
  fixture the existing abort tests use: `RiderReturnBest.ANonFiniteAbortUnderOnLimitReturnsTheFailedPointAndNoClaim` (tests/drivers/test_evaluation_failure.cpp).
- t1 was reported as an INSTRUMENT FAULT: its description in legs/mutations.sh held a comma, which broke the CSV row. The description is fixed (no comma); the mutation itself was
  applied and built (mutate.py matched once). Its kill (the limit-exit test) is visible in r2/mutation/mutation-table.csv's raw row; r3 re-runs it.
No criterion moved; no code changed except the added test.

## What changed
Code head 8e6e07310d51e5e22f7f3944f3ba4d900bcbc758 (2d49ca3a9 + the test commit 8e6e07310). `git diff 2d49ca3a9 8e6e07310 -- src include bench examples cmake CMakeLists.txt scripts` is empty
(tests only), so the library and the corpus tool are source-identical to r2's; the full chain is re-run anyway, from the top, so no leg's evidence is carried across heads.

## Arms and binaries (re-observed)
Arm A unchanged: origin/m8 0cfcec52f, sha256 33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0.
Arm B = build-grb/rel/bench/hven_sqp_corpus at 8e6e073..., frozen by legs/arm_b.sh, sha256 599b8c181816a86d56ac80bd2366ea359911dac2369da87c8808da5236c05c5d (the bench stamp
embeds the head, so it differs from r2's). Release and Debug trees rebuilt at this head (pre-declaration, not evidence).

## Everything else
As DECLARATION.md / DECLARATION-r2.md: P1, P3, P4, P5, M with the must-kill list s1 s2 l1 l2 o1 o2 o3 n1 n2 p1 t1 (r1 reported), no runtime gate, no TSan, no goldens, HOLDOUT not run.
Order: legs/chain.sh 8e6e07310d51e5e22f7f3944f3ba4d900bcbc758 599b8c181816a86d56ac80bd2366ea359911dac2369da87c8808da5236c05c5d 33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0.
