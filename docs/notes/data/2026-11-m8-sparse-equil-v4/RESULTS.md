# M8 sparse-equil v4 -- RESULTS of DECLARATION-SEQ4

Code commit `16efab6248d81a28a033d520ca1b5909083f1ff7` on `origin/m8` 66cc44ccc; declaration commit e647ef874; chain run
2026-11-18 21:55 -> 23:06 under the box lock (`orchestrate.out`). **ALL_LEGS_PASSED.**

| leg | result |
|---|---|
| P1 ctest | Release and Debug: "100% tests passed, 0 tests failed out of 3897" (`suites/`) |
| P2 replay 27 x 3 | 18 comparisons (3 reps x walk/ssn/ipm, vs arm A dev rows and vs B dev rows): 0 differences each; REPLAY_AS_DECLARED (`replay/`) |
| P3 TRAIN 238 x D/K | 476 rows, 0 missing/capped; movers vs arm A = the declared 9 (1 BETTER ALLINITC.K, 8 NEUTRAL, 0 WORSE); every row equals B's dev row; I4_PASS (`train/train_check.txt`) |
| P4 runtime gate (walk, serial, solo) | THROTTLE 0; counter identity 81/81 (round, cell) pairs; filtered B/A instructions:u = 1.000004 (rounds 1.000001-1.000004), WITHIN [0.995, 1.005] (`runtime/walk/analysis.txt`) |
| P5 mutations | control clean; all nine KILLED; tree restored; MUTATIONS_AS_DECLARED (`mutation/`) |
| P6 install smoke | default and clang front ends rc 0; both export-contract checks rc 0 (`install-smoke/`) |

Mutation killers (`mutation/*.killers.txt`):
- Mt1 (no trigger), Mt2 (tolerance blind): BRITGAS qp4 route, F3 fixture, the tiered unit test, qp26 escalation.
- Mt3 (request taken on an unperturbed factorization): `ARequestLeavesAnUnperturbedFactorizationOnThePlatform`.
- Me1 (escalation ignores the engine's refusal): the tiered unit test only -- the qp26 fixture does NOT isolate the refusal clause
  (its route still reaches a matched retake without it); recorded, not hidden.
- Me2 (scaled numerics never judged): the tiered unit test and qp26.
- Mm1 (row duals not updated), Mm2 (scaling reads u only), Mm3 (no augmenting search): the matching unit tests (brute force,
  Hungarian, scaling bounds) and the seam tests.
- Mm4 (scaled solve not scaled back): F3 fixture and the tiered unit test.

Ship rule (no WORSE status on TRAIN, no lost certified cell, cost within the declared bound): MET. Cost movers on TRAIN are
declared (HS101.K factorizations 542 -> 1075, still optimal; MESH.K 707 -> 782, infeasible both).

CI (all four lanes) on e647ef874 (code identical to the code commit): run 44446130175 -- linux-clang-release, linux-clang-coverage,
macos-clang-release (Accelerate: the response's every-factorization check and scaled retake ran in the suite), windows-clang-release:
all success.

Open (not this round): CRESC100.K (pairing class) stays infeasible under the specified escalation rule; `diag/escalation.txt` rule B
wins it but loses ALLINITC.K.

Raw outputs not committed: `MANIFEST.txt` (sha256).
