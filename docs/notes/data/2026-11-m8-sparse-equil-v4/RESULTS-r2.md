# M8 sparse-equil v4 r2 -- RESULTS of DECLARATION-SEQ4-r2 (run 1) and DECLARATION-SEQ4-r3 (the continuation)

Code commit `4d70afbd025889a0b845a2902863edd17a5ecc3f` on `origin/m8` 44f74446f; declarations r2 15edbee36 and r3
167281e61 (after run 1); frozen binaries `r2/FROZEN.txt` (0d74f105d). r2 run 1 2026-11-19 03:20 -> 03:50 (STOP at P4, void); r3 2026-11-19 04:30 -> 05:13. **ALL_LEGS_PASSED** (P1-P3 from r2 run 1, P4-P6 from r3, P0 in both).

| leg | result |
|---|---|
| P0 code identity | run 1 and r3: code paths == the code commit == the three trees DECLARATION-SEQ4-r2 section 6 declared; untouched paths == 44f74446f; frozen sha256 checked |
| P1 ctest (r2 run 1) | Release and Debug: "100% tests passed, 0 tests failed out of 3909" (`r2/suites/`) |
| P2 replay 27 x 3 (r2 run 1) | 18 comparisons (3 reps x walk/ssn/ipm, vs A's dev rows and vs B's dev rows): 0 differences each; REPLAY_AS_DECLARED (`r2/replay/`) |
| P3 TRAIN 238 x D/K (r2 run 1) | 476 rows, 0 missing/capped; movers vs A = the declared 9 (1 BETTER ALLINITC.K, 8 NEUTRAL, 0 WORSE); every row equals the r2 dev row; I4_PASS (`r2/train/train_check.txt`) |
| P4 runtime gate (walk, serial, solo) | run 1: VOID before any measurement (the copied analyzer's repo-root depth; DECLARATION-SEQ4-r3 section 0). r3: THROTTLE 0 (162 processes); counter identity 81/81 (round, cell) pairs; filtered B/A instructions:u = 0.999997 (rounds 0.999997, 0.999995, 0.999997), WITHIN [0.995, 1.005] (`r3/runtime/walk/analysis.txt`) |
| P5 mutations (r3) | control clean; all 15 KILLED (v4's 9 + Mi1 Mr1 Mr2 Mb1 Md1 Md2); Me1 killed by the integration fixture; tree restored; MUTATIONS_AS_DECLARED (`r3/mutation/`) |
| P6 install smoke (r3) | default and clang front ends rc 0; both export-contract checks rc 0 (`r3/install-smoke/`) |

## Movers (TRAIN, B vs A)

The same 9 rows v4 declared: ALLINITC.K BETTER (numerical_error 78 -> optimal 28); HS101.K, HS90.K, HS99EXP.K, LAKES.D,
LAKES.K, LEWISPOL.K, MESH.K, MSS1.K NEUTRAL. Against v4's own B rows only `side.eqp_refine_iters` moves, on 5 rows (HS101.K
1->2, HS90.K 0->1, HS99EXP.K 686->687, LAKES.D/K 4845->4853): M1, the abandoned solve rounds' residual-check solves now counted.
`factorizations` is unchanged against v4 on every row (the recursion's extra platform refactorization was never counted;
the in-place retake no longer pays it). RS-4 and RS-5 are row-neutral (A and B dev rows identical on bc8ed0e12 and 44f74446f).

## I4 -- the refusal clause

- qp26: unchanged without the clause (kOptimal, 23 minors, 1 scaled, 1 matched, both algebras); in the shipped code the
  escalating scaled answer misses the backward error as well as the residual check (`r2/diag/qp26-seqdiag-*.txt`).
  RECONCILED: `RESULTS.md`'s "the qp26 fixture does NOT isolate the refusal clause" is right; `diag/escalation.txt`'s minor-15
  reading (eta 1.07e-7, refused) came from the v2 dev instrument, which judged a different answer. The qp26 test comment now
  says so.
- TRAIN 238 x 2 without the clause: exactly LAKES.D and LAKES.K move (numerical_error in every arm; status, iterations,
  objective identical): matched retakes 3 -> 0, retake refusals 0 -> 2, factorizations 5380 -> 5379, eqp_refine_iters 4853 ->
  4843 (`r2/dev/train_check-Me1-vs-diag-dev.txt`).
- Firings: 3 refusal-only escalations per LAKES arm (dual_mu 1e-8, 78 rows on 103-108 free variables, not over-determined),
  each answered by a matched retake the residual check accepted; at dual_mu = 0, ZERO refusal-only escalations over TRAIN, and
  no scaled-tier verdict on an over-determined face (`r2/diag/train-scaled-tier-verdicts.txt`).
- Rows move: the clause is KEPT, pinned by `QpPlatformRetake.LakesQp0EscalatesOnTheEngineRefusal` (fixture
  `SEQ4_LAKES_qp0.txt`, a dumped LAKES walk solve; replays the in-cell route exactly: kOptimal, 131 minors, 3 scaled, 2 matched,
  0 retake refusals; without the clause: 2 scaled, 0 matched, 1 refusal).

## Cost (M2: the scope of "within the declared bound")

P4 measures the instruction count of the walk-mode corpus on Pardiso (MKL), where the platform solve check is idle unless a
factorization reports perturbed pivots. Accelerate runs the check on EVERY platform factorization (it reports no
perturbed-pivot count); that cost is UNOBSERVED (no Mac measurement; macOS CI is correctness only). Cost movers on TRAIN are
v4's (HS101.K factorizations 542 -> 1075, still optimal; MESH.K 707 -> 782, infeasible both).

## Mutations

| id | what | killers (`r3/mutation/*.killers.txt`) |
|---|---|---|
| Mt1 | verdict never asks for kScaled (the one function, both sites) | BRITGAS qp4 route, LAKES qp0, the bordered verdict test, F3 every scale, tiered unit, qp26 |
| Mt2 | trigger tolerance 1e+5 | the same six |
| Mt3 | request taken on an unperturbed factorization | ARequestLeavesAnUnperturbedFactorizationOnThePlatform |
| Me1 | escalation ignores the engine's refusal | LAKES qp0 (the integration must-kill), tiered unit |
| Me2 | scaled numerics never judged | AFailedRetake..., LAKES qp0, tiered unit, qp26 |
| Mm1 / Mm2 / Mm3 | matching duals / scaling exponent / no augmenting search | the matching unit tests (brute force, Hungarian, bounds), and response fixtures |
| Mm4 | scaled solve not scaled back | LAKES qp0, bordered verdict, F3, tiered unit, analysis-reuse test |
| Mi1 | take_retake catches nothing (I1) | AFailedRetakeLeavesThePlatformNumericsDisarmed |
| Mr1 | no request clear at run() entry (I2) | ARequestDoesNotSurviveIntoTheNextSolve |
| Mr2 | latch no-op test ignores a pending request (I2) | APendingK0RetakeIsNotTheNoOpLatch |
| Mb1 | bordered site never consults the verdict (I3) | TheBorderedAnswerCarriesTheVerdict, F3 every scale |
| Md1 / Md2 | failed in-place / requested retake leaves the check armed (M8) | AFailedRetakeLeavesThePlatformNumericsDisarmed |

## CI

All four lanes on 3c8e8907d (code identical to the code commit 4d70afbd0): run 44503904751 -- linux-clang-release, linux-clang-coverage, macos-clang-release (Accelerate: the response's every-factorization check and scaled retake ran in the suite; the new tests' Apple arms), windows-clang-release: all success.

Raw outputs not committed: `r2/MANIFEST.txt` (sha256).
