# M8 kopt-1x -- RESULTS (DECLARATION.md, code head 006ccc49663611dc27440989860d1a1f468e4090)

Chain: `legs/orchestrate.sh` (output `orchestrate.out`), declaration commit `34ee68ebc`. **ALL_LEGS_PASSED**; no criterion changed after data.

| leg | verdict | evidence |
|---|---|---|
| P1 Release | 100% tests passed, 0 failed out of 3734 | `suites/ctest-rel.log` |
| P1 Debug | 100% tests passed, 0 failed out of 3734 | `suites/ctest-dbg.log` |
| P2 replay | 3 repetitions x walk / ssn / ipm, each vs arm A's dev row and B's dev row: 0 differences in all 18 comparisons | `replay/cmp-*.txt` |
| P3 TRAIN | `I4_PASS`: 476 rows, 14 movers = the declared 14 (better 0, worse 0, neutral 14), none missing or capped, every row equals B's dev row | `train/train_check.txt`; raw cells dropped (`MANIFEST-dropped.txt`) |
| P4 runtime (walk, serial, solo) | counter identity on 81 (round, cell) pairs; 162 processes, 0 throttled; filtered instructions:u **B/A = 1.000054** (rounds 1.000052 / 1.000054 / 1.000056), WITHIN [0.995, 1.005] | `runtime/walk/analysis.txt` |
| P5 mutations | control clean; M1 killed by `QpRowContractPolish.TheEntryTestOpensOnAGrayZoneRowAndOnlyThen`; M2 by `QpRowContract.FlatNormalGrayZoneSeedIsRepairedToOneFloorUnit`; M3 by `QpRowContractPolish.ARowTriggeredEntryFloorsTheIncumbentsStationarity`; M4 by `QpCertificateSign.AnOptimalExitPastOneFloorUnitIsCountedAGrayExit`; tree restored | `mutation/` |

## Movers

TRAIN (14, all as declared, all NEUTRAL by status rank): ACOPP30 D,K (optimal both; iterations 7 -> 11); CRESC100 D (optimal; qp minors 23791 -> 13586);
CRESC4 D,K (optimal; iterations 65 -> 26 and 48 -> 26); CRESC50 K (numerical_error -> max_iter, both unsolved: the one status-label move);
HS84 D,K, HS92 D,K, HS99EXP K, MESH D (+1 to +2 qp minors); OPTPRLOC D,K (iterations 40 -> 42). No solved cell became unsolved.
Replay: none. Suites: HsSweepCorpus re-pins (HS10 warm Release 28/42 -> 26/40; total minors Release 3355 -> 3359, Debug 3346 -> 3355) and the +5 new tests.

## Gray-zone census (`optimal_row_gray_exits`)

HS battery (every arm and lever cell): Release **4**, all HS15's warm arm (one exit in each of its four lever cells; the arm that also
holds the corpus's one failing solve); Debug **0**. The TRAIN lane and the replay corpus do not emit the counter. The ruling's
"no hard refusal now" stands: HS15 warm still ends kOptimal there. A later census decides.

## The reproducer

The flat-normal fixture (`QpRowContract.*`, both algebras, k = 1.5, 3, 6.9, 9.5) certifies kOptimal with the row at exactly 0 after the
walk steps once (minors 2, polish not needed); at the base it certified at minors 1 with the residual at k floor units. The polish path is
pinned separately (`QpRowContractPolish.*`: entry test, stage-1 adoption with `terminal_polish_solves` 0, the stationarity floor).

## CI

See `CI.md`.
