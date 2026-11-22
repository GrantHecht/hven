# DECLARATION-RF-1-C-r1 -- gate C, successor for leg C1

Written and committed before its legs; never edited afterwards. `DECLARATION-RF-1-C.md` is kept unchanged.

**Why.** Run 1 (`gate-C/`, code `c721dc32d`): C1 dbg failed one test, `SqpSolverQpFailure.RepeatedQpMaxIterStillPropagates`.
The test pins its status in three build arms (`USE_ACCELERATE_SPARSE`, `NDEBUG`, Debug); C's pin move updated the Release arm
only. The Debug arm reads kStalled -- the same minor-cap relabel section 1 of the declaration predicts. Fix `cfe994701` (test
only): the Debug and the Accelerate arms move to kStalled with the Release arm. The Accelerate arm's value is UNOBSERVED
until the macOS CI lane runs it. Add that row to section 2's table of pinned expectations.

**What stands.** The library is unchanged (`git diff c721dc32d cfe994701 -- src include` is empty), so run 1's C2 (TRAIN), C3
(replay), C4 (mutations) and C5 (install smoke) evidence stands as recorded, and so do their binaries.

**Re-run (evidence `gate-C-r1/`).** C1 at `cfe994701`: `ctest.sh` rel and dbg, both "100% tests passed, 0 tests failed".
Then CI: one dispatch of the four-lane workflow at the pushed `rf1` head, green on all four lanes.
