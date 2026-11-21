# M8 AP-5 G-RB -- DECLARATION-r2 (committed before any r2 leg; never edited afterwards)

DECLARATION.md (7184ee8af) stands in everything it says except the items below. Why r2: the r1 chain stopped at its first leg (ctest-rel, r1/chain.out,
r1/ctest-rel-summary.txt): `Limits.ALimitExitIgnoresReturnBest` failed. The cause is mine: after adding the IPM work/time limit door substitution (DECLARATION B, "IPM work/time
limit door") I ran the targeted tests but not the full suite again, so I declared a head with one existing test still pinning the pre-G-RB behaviour. No leg after ctest-rel ran in r1;
no criterion moved.

## What changed
Code head 2d49ca3a99d92a48e6c362e5fa155cacb6034f18 (89614ba63 + the test commit 2d49ca3a9): `Limits.ALimitExitIgnoresReturnBest` is re-derived as
`Limits.AWorkLimitExitFollowsTheSharedReturnBestRule` (tests/drivers/test_limits.cpp), a declared re-derivation. The old test pinned that an IPM work-limit exit returns the iterate the
solve stopped on under return_best. That pin sat beside a mixed result (the old code set returned_best_iterate and read the best iterate's residual row while x and f were the last
point's -- the forced-exit test found it). The unified rule (design 2.5 O1: kOnLimit covers kIterationLimit/kWorkLimit/kTimeLimit; the IPM's old bool is kOnAnyNonOptimal) says a
limit exit substitutes, so the new test pins: kNever returns the stopped-on point with returned_best_iterate false; kOnLimit and kOnAnyNonOptimal return the best iterate in full
(x and f equal best_iterate's) whenever a best was recorded, with a premise that the substitution moves some work-limit exit. This is a behaviour change for IPM return-best users at a
work or time limit (return-best on only; no corpus cell has it on). The migration doc already lists it (section G-RB, fourth bullet).
Full Release ctest at this head, run before this declaration as a pre-declaration run (NOT evidence): 100% passed.

## Arms and binaries (re-observed)
Arm A unchanged: origin/m8 0cfcec52f, build-grb/armAbin/hven_sqp_corpus sha256 33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0.
Arm B = build-grb/rel/bench/hven_sqp_corpus at 2d49ca3a99d92a48e6c362e5fa155cacb6034f18, frozen by legs/arm_b.sh, sha256
b198ea0cacf1b892d40a80e723483bae8325fcedc5a0c465d0c4d4a0d49087aa. Debug tree rebuilt at this head.

## Everything else
Proofs P1, P3, P4, P5, M, the must-kill list (s1 s2 l1 l2 o1 o2 o3 n1 n2 p1 t1; r1 reported), the runtime-gate statement, Order, hven-bench note: as DECLARATION.md, with the head and arm B
above. Order: legs/chain.sh 2d49ca3a99d92a48e6c362e5fa155cacb6034f18 b198ea0cacf1b892d40a80e723483bae8325fcedc5a0c465d0c4d4a0d49087aa 33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0.
Evidence of r1 is kept under r1/ (chain.out, ctest-rel-summary.txt).
