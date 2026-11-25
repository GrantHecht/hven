# M8 WA-1 -- RESULTS of rider 2 (r7): code `192d00529` on `origin/m8` `64e384e2a` (RS-R1)

Supersedes `RESULTS-r5.md`. Declaration `DECLARATION-WA1-r7.md` (committed `2ffe681e0`, before the first leg); chain
`r7/orchestrate.out`.

## Rulings (Fable re-review 2026-11-19)

| ruling | disposition |
|---|---|
| 1 (C1') | MEASURED, then DROPPED: 57 kOk regularized steps on TRAIN, 34 sequences; 19 of 33 land at or above the no-rule f after the following pass, 18 of the 21 row-restoring endings (`r7/c1k-train.txt`). A kOk face whose merit rises is a counted decline; `EqpResult::x_regularized` removed. Assertion added: the pass after a footprint ride keeps its gain unless it is the counted decline. |
| 2 (I-a) | DONE: the ride's landing point is re-checked after pass-overs; outside the budget the ride is refused and the pass solved again without it (`ascent_footprint_ride_refusals`, `kRideRefused`); test `SyntheticRidePassedOverOutOfTheBudgetIsRefused`. |
| 3 (I-b) | DONE: revert kept; counters doc fixed; footprint rungs no longer hold shared slots (ruling 5), so no slot return is needed; open item registered. |
| 4 (I-c) | DONE: `last_drop`, standstill, stagnation, gray-hold and gray-return state restored on every pass solved again without moving. |
| 5 (I-d) | MEASURED, ADOPTED: own cap of one rung; TRAIN -1.38% qp minors, -3.12% factorizations vs the shared budget, no status change; MSS1.D 17636 -> 9525 minors (MSS1.K 5903 -> 11116). MSS1.D disclosed as a cost mover (A 9003, r5 15627, r7 9525). |
| 6 (minors) | DONE: argument structs, tally excludes kMeritFalls, portable decline assertion, narrative trimmed, revert/rung/refusal record fields. |

## Gate

| leg | result |
|---|---|
| P1 ctest | Release and Debug: 100% tests passed, 0 failed out of 3925 (`r7/suites/*.tail.txt`) |
| P2 replay x3 | 18/18 comparisons 0 differences vs arm A's and B's r7 dev rows (walk, ssn, ipm); no replay mover: REPLAY_AS_DECLARED |
| P3 TRAIN 238 x D/K | 476 rows, 0 missing/capped, 7 movers == declared (0 BETTER, 7 NEUTRAL, 0 WORSE), every row == its dev row: I4_PASS |
| P4 runtime (walk, serial, solo) | counter identity 81/81, 162 processes 0 throttled, filtered B/A = 1.000045 (`r7/runtime-analysis.txt`) |
| P5 mutations | control clean; N1-N6, N8-N12, N14, N15, N17-N19, N22, N23 all KILLED; N20, N21 recorded (no killer): MUTATIONS_AS_DECLARED |
| P6 install smoke | smoke and export contract rc 0, GCC default and clang++ |

ALL_LEGS_PASSED (`r7/orchestrate.out`, 18:32-19:56 2026-11-19). Raw outputs in `.scratch/raw-wa1-r7` (hashes in
`r7/MANIFEST-dropped.txt`).

## Movers

- TRAIN vs arm A: NEUTRAL only -- HS101.D (qp minors 698 -> 692), HS101.K (1039 -> 1032), MESH.D (4011 -> 4011, factorizations
  7177 -> 7544), MSS1.D (9003 -> 9525), MSS1.K (9521 -> 11116), SPANHYD.D (23158 -> 18208), SPANHYD.K (20309 -> 20306; all
  numerical_error or max_iter on both arms except HS101). Totals: qp minors -1.36%, factorizations -2.93%.
- Lost against r5: CRESC100.K's BETTER (infeasible on both arms again) and HS56.D's move -- both rode on the dropped kOk step.
- Replay: none.
- Tests: the qp26 pass-42 fixture regresses from kOptimal (r5) to kNumericalError (906 minors, eq drift 126): its kOk face
  declines and that ordinary step leaves the rows. The kWrong ride still fires and falls.

## qp25 close note

qp25's Release pin moved at r5 from -1398.63412864 to -5.79957813652: both are known certified points of the fixture, and the
footprint ride takes the walk to the one with the much higher objective -- a gap of about 1393 in f that the rule does not close
and does not claim to. On the r7 code Debug certifies the same point (-5.79957813652, 80 minors, kOptimal). **Debug passing
depends on the rung revert**: with the revert removed (mutation N15), Debug qp25 ends kNumericalError after 87 minors on both
algebras (`r7/qp25-debug/`).

## CI

Code commit `192d00529`: run 44626643408 green on all four lanes (linux release, linux coverage, macOS, Windows). Evidence head: see the hand-back.
