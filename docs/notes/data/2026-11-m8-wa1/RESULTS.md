# M8 WA-1 -- RESULTS (DECLARATION-WA1.md, run 1)

`orchestrate.out`: **ALL_LEGS_PASSED** (2026-11-19 00:07 - 01:19), code commit `0d674bdcb`, arm A `66cc44ccc`, one run, no successor.

| leg | criterion | result |
|---|---|---|
| P1 rel | 100% of 3891 | `suites/ctest-rel.log`: 100% tests passed, 0 failed out of 3891 |
| P1 dbg | 100% of 3891 | `suites/ctest-dbg.log`: 100% tests passed, 0 failed out of 3891 |
| P2 replay x3 | vs A dev rows and B dev rows, 0 differences | 18/18 comparisons `differences: 0` (walk, ssn, ipm x 3), REPLAY_AS_DECLARED |
| P3 TRAIN 238 x D/K | I4_PASS, no WORSE | 476 rows, 0 missing/capped, 9 movers (better 0, worse 0, neutral 9) == declared, every row == B dev row: I4_PASS |
| P4 runtime (walk, serial, solo) | THROTTLE 0, counter identity, B/A in [0.995, 1.005] | 81 (round, cell) pairs, 0 differences; filtered B/A = 0.999998 (rounds 1.000000, 0.999998, 1.000001) |
| P5 mutations | control clean; M1-M5 killed | control 0 failures; M1 killed by both fixture tests; M2 by the merit-rise unit test; M3 and M5 by the move unit test; M4 by the blocker unit test and both fixture tests; M6, M7 recorded, 0 killers (as declared); tree restored: MUTATIONS_AS_DECLARED |
| P6 install smoke | rc 0, both front ends | smoke and export contract rc 0 (GCC default and clang++) |

Movers: exactly section 3's -- no replay mover; TRAIN 9 NEUTRAL rows (CRESC100.K, HS101.D/K, HS56.D, MESH.D, MSS1.D/K, SPANHYD.D/K);
no status changed, no certified cell lost; TRAIN cost +1.2% qp minors, +1.9% factorizations (dev), inside the declared +2.5%.

CI: run 44459486250 green on all four lanes (`CI.md`).

Raw per-cell dumps (TRAIN cells, runtime symbol tables) are not committed: `MANIFEST-dropped.txt`.
