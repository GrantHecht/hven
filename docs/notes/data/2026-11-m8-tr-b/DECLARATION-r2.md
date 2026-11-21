# M8 TR-B -- DECLARATION-r2 (successor to DECLARATION.md, which stays frozen; committed before any r2 leg; never edited afterwards -- a failure is DECLARATION-r3.md)

## Why r2
r1 (code head cd92e65c3; arm A e43d9f8b5 corpus sha256 6d323ea60f7f9fb87edcdafdc83cfa663a610ee11e75cb200c3fe291fb7c7be1, arm B corpus sha256 071822cdb75a262e55c4208e6b98eacc244ac362b256fbdd5a074717b4a825c1) ran: A/A calibration PASS (interior 1.000000, ipm 1.000000, ssn 1.000000, walk 0.999997; band [0.995, 1.005] CONFIRMED), runtime B/A PASS (interior 0.999996... see runtime/), ctest Release and Debug 100%, replay x3 + armed, interior, install smoke + export contract all rc 0, and the mutation leg: control fails nothing, 12 of 13 must-kills KILLED, **s5 (the SQP drops restoration_exited on the resumed path) SURVIVED**: no test reached an SQP resumed restoration. The chain stopped at the verdict (r1/chain-r1.out, r1/mutation/).

## Change since r1 (tests and a leg script only; no src/, include/ or bench/ change)
- `tests/sqp/test_trace_messages_sqp.cpp` (new): TraceMessagesSqpResume scans the 27 HS problems at four start scales and requires every restoration entry to have an exit, every resumed exit a notice, and at least one resumed exit (premise); it kills s5.
- `legs/mutations.sh`: the SQP test filter gains TraceMessagesSqpResume.*; `legs/chain-r2.sh` runs the r2 legs only.
Code head r2 = `a31b441fe` (git diff --stat vs cd92e65c3 shows tests/sqp only; src, include and bench identical).

## What r2 runs (solo, serial under the box lock, the box protocol as in DECLARATION.md)
1. ctest Release and Debug (the suite changed: one test added): 100% passed.
2. The mutation leg on the r2 head and its verdict: control fails nothing; every id of i1 i2 i3 i4 s1 s2 s3 s4 s5 s6 a1 a2 g1 killed; MUTATION_VERDICT PASS.
Every r1 leg whose inputs did not move STANDS and is NOT re-run: runtime A/A and B/A (the arm B binary is the same bytes: src/include/bench unmoved), replay x3 + armed, interior, install smoke + export contract. The r1 arm B binary is kept at .scratch/build-trb/armBbin (sha256 071822cd..., the bytes the standing legs ran). A rebuilt Release corpus differs in bytes only by the bench stamp's head sha (the stamp is configured from the checkout's HEAD); the claim that the standing legs describe the r2 head rests on `git diff cd92e65c3 -- src include bench` being empty, which the chain checks.

## Not run, disclosed
No TSan leg. hven-bench not edited (TR-E). Windows/Accelerate UNOBSERVED. Pre-declaration runs (not evidence): the Release/Debug builds and the one new test (passes on Release).
