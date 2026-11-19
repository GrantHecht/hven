# M8 WA-1 -- RESULTS of the rework (r5/r6): code `0811f4cf6` on `origin/m8` `b02e88eb4` (RS-6a)

Supersedes `RESULTS-final.md` (the r1-r4 range, withdrawn after the Fable review). Declarations: `DECLARATION-WA1-r5.md`,
`DECLARATION-WA1-r6.md`; chains `r5/orchestrate.out` (P1, P2; stopped at P3) and `r6/orchestrate.out` (P3-P6, ALL_LEGS_PASSED).

## Gate

| leg | round | result |
|---|---|---|
| P1 ctest | r5 | Release and Debug: 100% tests passed, 0 failed out of 3922 (`r5/suites/*.tail.txt`) |
| P2 replay x3 | r5 | 18/18 comparisons 0 differences vs arm A's and B's r5 dev rows (walk, ssn, ipm); no replay mover |
| P3 TRAIN 238 x D/K | r5 | STOPPED: SPANHYD.K's B row differed from its dev row (bimodal across processes; solo 5/5 = the leg row, `r6/spanhyd-solo.txt`) |
| P3 TRAIN 238 x D/K | r6 | 476 rows, 0 missing/capped, 9 movers == declared (1 BETTER, 8 NEUTRAL, 0 WORSE), every row == r5's leg row: I4_PASS |
| P4 runtime (walk, serial, solo) | r6 | counter identity 81/81, 0 throttled, filtered B/A = 1.000183 |
| P5 mutations | r6 | control clean; N1-N16 all KILLED (`r6/mutation/*.killers.txt`): MUTATIONS_AS_DECLARED |
| P6 install smoke | r6 | smoke and export contract rc 0, GCC default and clang++ |

CI on the code (head 028daa536): run **44579321329** green on all four lanes (linux release, linux coverage, macOS, Windows).
The previous code head's run 44567275588 failed macOS on `QpW1AscentGuard` (a ride whose decrease sat below f's rounding floor;
the tally now reads past the floor).

## Movers

- TRAIN vs arm A: **CRESC100.K BETTER** (infeasible -> optimal); NEUTRAL: HS101.D (qp minors 698 -> 692), HS101.K (1039 -> 1032),
  HS56.D (551 -> 552), MESH.D (4011 -> 2804), MSS1.D (9003 -> 15627), MSS1.K (9521 -> 9648), SPANHYD.D, SPANHYD.K (numerical_error
  on both arms). Totals: qp minors +0.16%, factorizations +0.75% (bound +3%).
- Replay: none. Tests: qp25's Release pin re-derived -1398.63412864 -> -5.79957813652 (declared).

## C1 (measured on the r1-r4 code)

TRAIN: 1472 footprint moves; 722 of 724 move chains on an unchanged working set end in an ascending ordinary step
(`r5/c1-train.txt`). The move only postponed the ascent; it is removed.
