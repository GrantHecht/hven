# M8 WA-1 -- final RESULTS (the shipped range: code `879d52bd4` on `origin/m8` `c71c0a7d9`)

Round history (every declaration and output kept): run 1 (`DECLARATION-WA1.md`, base 66cc44ccc) ALL_LEGS_PASSED, then failed Windows
CI after the RS-4 rebase; r2 (`DECLARATION-WA1-r2.md`) withdrawn without a leg (its code failed Windows CI identically); r3
(`DECLARATION-WA1-r3.md`, base c71c0a7d9, the multiplier-free row budget) passed P1-P4 and stopped at P5 on a stale mutation anchor;
r4 (`DECLARATION-WA1-r4.md`, same code and binaries, anchor re-pointed) passed P5 and P6.

| leg | round | result |
|---|---|---|
| P1 ctest | r3 | Release and Debug: 100% tests passed, 0 failed out of 3916 (`r3/suites/`) |
| P2 replay x3 | r3 | 18/18 comparisons 0 differences vs arm A and B dev rows (walk, ssn, ipm) |
| P3 TRAIN 238 x D/K | r3 | 476 rows, 0 missing/capped, 8 movers (better 0, worse 0, neutral 8) == declared, every row == B dev row: I4_PASS |
| P4 runtime (walk, serial, solo) | r3 | counter identity 81/81, THROTTLE 0, filtered B/A = 1.000002 |
| P5 mutations | r4 | control clean; M1 (2 killers: the qp26 fixture tests), M2 (1), M3 (1), M4 (4), M5 (2), M8 (1) KILLED; M6, M7 recorded (0 killers, as declared): MUTATIONS_AS_DECLARED |
| P6 install smoke | r4 | smoke and export contract rc 0, GCC default and clang++ |

Movers (TRAIN, all NEUTRAL, no status change): CRESC100.K, HS101.D, HS101.K, MESH.D, MSS1.D, MSS1.K, SPANHYD.D, SPANHYD.K. Replay: none.
Cost: TRAIN qp minors +2.5%, factorizations +1.5% (bound +3%); the solved movers HS101.D/K +1/+2 minors (bound +1%).

CI on the final code: run 44515953087 green on all four lanes; Windows repeat: `CI-final.md`.
