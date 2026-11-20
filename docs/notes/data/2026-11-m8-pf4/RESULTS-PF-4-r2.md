# RESULTS-PF-4-r2 -- the settler's accuracy question, and the Schur border batch rebased on origin/m8 331b31837

Declarations:
- `DECLARATION-PF-4-r2.md`: the round; its chain is `r2/gate/`.
- `-r3.md`: after an instrument defect at P4a; `r2/gate-r3/`.
- `-r4.md`: after a throttle void at P4a; `r2/gate-r4/`.

Code commit `4a256c7a30c4d1953cc3f84fed0fac7b497436c3` on `origin/m8` `331b31837`, branch `pf4-r2`. AMD Ryzen 7 5800X3D,
clang 22.1.8, MKL 2026.2, `MKL_NUM_THREADS=1`. No wall-clock is quoted; every performance figure is `instructions:u`.

## 1. Verdict

**ALL_LEGS_PASSED.**
- r2: P1, P2 and P3 pass; P4a stopped on an instrument defect before any process ran.
- r3: P4a void on one throttled process in a box lane.
- r4: P4a (exclusive box), P4b, P5 and P6 pass.

CI run 44761741351 on `18d15c8d0` (code = the code commit): four lanes green on attempt 2. On attempt 1, Windows failed
one off-reference pin that passed on attempt 2 (section 6).

**No accuracy gap; no remedy built.** Batched K0 columns are as backward-stable as single solves. The r1 movers are the
sensitivity of those trajectories to perturbations below either solve's own error. This is a declared numerics event,
re-derived on the new base with every mover identical to r1. Shipping is the settler's decision.

## 2. Backward error, batch vs single (probe 3, `r2/probe/backward_error.txt`)

- Scope: every column of every k >= 2 Schur batch, answered both ways on the numerics the solve used, with
  eta = ||K0 x - b||inf / (||K0||inf ||x||inf + ||b||inf).
- 5.55 million columns in 73 367 batches; all batches ran on plain numerics (no retake occurred on these cells).
- The probe reproduces lanes A and B row for row, so these are the shipped trajectories.

| cell | perturbed pivots | columns | eta batch median / p99 / max | eta single median / p99 / max | mean log10(eta_b/eta_s) |
|---|---|---:|---|---|---:|
| DALLASM | 0 | 6 187 | 2.1e-20 / 1.3e-17 / 7.5e-17 | 2.1e-20 / 1.3e-17 / 7.5e-17 | +0.003 |
| STEENBRC | 0 | 26 102 | 1.6e-17 / 1.2e-11 / 2.3e-10 | 1.6e-17 / 8.8e-12 / 1.6e-10 | +0.031 |
| STEENBRC | > 0 | 23 803 | 5.1e-11 / 6.9e-10 / 4.8e-09 | 4.8e-11 / 5.6e-10 / 4.0e-09 | +0.042 |
| ACOPP118 | 0 | 51 493 | 2.9e-11 / 1.1e-10 / 5.2e-10 | 2.9e-11 / 1.1e-10 / 4.8e-10 | +0.023 |
| SPANHYD D | 0 | 1 303 247 | 8.8e-19 / 2.6e-07 / 1.0e-06 | 8.6e-19 / 2.6e-07 / 1.7e-06 | +0.006 |
| SPANHYD D | > 0 | 104 821 | 6.6e-18 / 2.8e-17 / 3.1e-17 | 6.3e-18 / 2.3e-17 / 2.8e-17 | -0.003 |
| SPANHYD K | 0 | 1 227 120 | 4.9e-19 / 2.7e-07 / 1.9e-06 | 5.3e-19 / 2.5e-07 / 1.1e-06 | +0.012 |

The single-trajectory runs agree to the second digit. Refinement steps (iparm[6]): 2 on every column, both ways. On 12
STEENBRC columns a single solve stopped at 1 step where the batch ran 2: the batch's one count covers all its columns.

## 3. The cause

Neither path is less accurate. The multi-RHS phase-33 solve sums its blocked substitutions in a different order than k
single solves do. That last-ulp difference per operation is amplified by cond(K0), and further by refinement that stalls
on perturbed-pivot factors. The result is forward differences of 1.1e-3 (STEENBRC), 1e-4 (SPANHYD) and 3.5e-6 (ACOPP118)
between two answers of equal backward error.

What it is not:
- **Iterative refinement.** Both paths run the same 2 steps, and the batch never fewer.
- **The retake arms.** `solve_mat` forms `solve_vec`'s exact products. Those arms are now pinned by
  `Schur.BatchOnRetakeNumericsLandsThePerAddColumns`, the killer of R1 and R2.
- **Workspace or layout.** The same session and phase carry `nrhs`.
- **iparm.** No iparm is involved.

## 4. Trajectory sensitivity (probe 3j, `r2/probe/jitter.txt`)

Landed k >= 2 columns were perturbed by K0^-1 delta, with ||delta||inf = 2^-52 ||b||inf: a backward perturbation at the
unit roundoff, below both solves' own error. Seeds 1-3; seed 0 is the unperturbed arm.

| cell | arm A (single) | arm B (batch) |
|---|---|---|
| ACOPP118 minors | 10361 / 5739 / 5639 / 2546, all optimal | 33922 / 954 / 954 / 24491 numerical_error |
| CRESC100.D iterations | 40 / 44 / 44 / 44 | 211 / 40 / 38 / 40 |
| STEENBRB objective | 9086.19 / 9119.97 / 9444.38 numerical_error / 9119.97 | 9203.80 / 9075.86 / 9075.86 / 9159.69 |
| STEENBRD iterations | 85 x 4 (9030.08) | 107 (9074.19) / 73 / 50 / 62 (9030.08) |
| MESH.K | infeasible / NE / infeasible / NE | NE / NE / NE / max_iter |
| CRESC50.K | NE x 4 | max_iter x 3 / NE |

Each r1 cost mover is one draw from its cell's spread at the unit roundoff. Arm A's own draws break "no cost regression
on any solved cell" (STEENBRB reaches kNumericalError; ACOPP118 spans 4x in minors).

**The guard that was tried** (probe 3g): one keep-while-falling refinement step on columns with eta > 1e-12.
- Offline it makes the batch's columns more accurate than singles on every cell, at an extra solve on 14-82 % of the
  columns.
- In the solver: ACOPP118 6133 / 11120 / 6229 minors; MESH.K kInfeasible x3; STEENBRB/D unchanged; **CRESC100.D
  kIterationLimit at 2 of 3 seeds**, worse than any draw of either arm.
- Not built.

## 5. The gate

| leg | result |
|---|---|
| P1 ctest | Release and Debug: "100% tests passed, 0 tests failed out of 3965" (`r2/gate/suites/`) |
| P2 replay x3 | `REPLAY_AS_DECLARED`: each run vs arm A walk 3 (the residual-class columns of `f7_n800_path_warm`), ssn 0, ipm 0; vs B's dev rows 0 (`r2/gate/replay/`) |
| P3 TRAIN 238 x D/K | `I4_PASS`: 476 rows equal their dev rows; 33 movers exactly `r2/movers-train.txt` (1 BETTER, 32 NEUTRAL, 0 WORSE); hven-ipm B/A 1.001183, hven-sqp B/A 0.591169 (`r2/gate/train/`) |
| P4a corpus runtime (walk) | r2 VOID (instrument: `analyze.py`'s root one level short); r3 VOID (one throttled process in a box lane); r4 on the exclusive box: 81 pairs, counter identity holds, 162 processes, 0 throttled, filtered B/A 0.999684 <= 1.005 (`r2/gate-r4/runtime/`) |
| P4b nlp-35 | `NLP_AS_DECLARED`: 14 movers exactly `r2/movers-nlp.txt`, none WORSE; total B/A 0.998967 (`r2/gate-r4/nlp/`) |
| P5 mutations | `MUTATIONS_AS_DECLARED`: control clean; B1-B7, T2-T6, **R1, R2** each KILLED (`r2/gate-r4/mutation/`) |
| P6 install smoke | rc 0 on GCC and clang++ (`r2/gate-r4/install-smoke/`) |

## 6. Movers vs arm A (331b31837), and the saving

- **The movers are r1's, value for value.** I17 moved only harness ledger `side.*` columns on 40 TRAIN rows of each
  arm, and none of the A-vs-B differences.
- **TRAIN**: 33 hven-sqp movers, 0 hven-ipm.
  - CRESC100.K kInfeasible -> kOptimal.
  - CRESC100.D 40 -> 211 iterations, though its instructions fall to B/A 0.359.
  - CRESC50.K kNumericalError -> kIterationLimit.
  - MESH.K kInfeasible -> kNumericalError.
  - The rest are unchanged statuses (`DECLARATION-PF-4.md` section 3).
- **nlp-35**: every status unchanged.
  - ACOPP118 x2.04 instructions (minors 10361 -> 33922).
  - STEENBRB 1.05 (objective 9086.19 -> 9203.80).
  - STEENBRD 0.886 (9030.08 -> 9074.19).
  - The other 12 cells: 0.649-0.995.
- **The saving survives whole** (no remedy built).
  - TRAIN hven-sqp instructions B/A 0.5912 (D 0.5302, K 0.6543).
  - The PF-3 add_border cells: SPANHYD 0.49-0.58, DNIEPER 0.54-0.62, MESH 0.60-0.72, CRESC100.D 0.36.
  - nlp-35 0.9990 in total.
  - Corpus walk 0.9997 with counter identity.
- **CI Windows**: `SqpElasticWitnessStart.SawpathMajor0RungSolvesFromItsWitness` failed on attempt 1 (stationarity 8.38e-6
  > 1e-6 on a kOptimal answer, its off-reference portable contract) and passed on attempt 2 of the same commit. It is a
  run-to-run flip of the off-reference Windows build on a cell that WX-R2 already route-pins. It is recorded, not
  re-pinned.

## 7. Open items

1. **Ship or not (settler).** Equal accuracy (section 2), with the movers inside the unit-roundoff spread (section 4).
   Does the ship bar move from "no cost regression on any cell" to a distributional reading? A wider probe 3j ensemble
   (all 238 TRAIN cells, both arms) is the instrument for that, if wanted.
2. **SPANHYD K0 column accuracy (both arms, not PF-4).** Unperturbed columns keep eta up to 1.9e-6 after Pardiso's two
   refinement steps; a third step reaches 1e-10. Its relation to SPANHYD's kNumericalError is unmeasured.
3. **The box rule** (two lanes, 2026-11-20) arrived mid-round. P4a's throttle check is sensitive to the whole box's perf
   interrupt load (`perf_event_max_sample_rate` read 63000 after r3). It needs `--exclusive` even though it asserts no
   wall-clock; the coordinator may want that recorded for other runtime legs.
4. Carried from r1: symbolic reuse, the trace reader's two keys, and Accelerate's multi-RHS rounding (UNOBSERVED beyond
   the CI fixture).
5. Windows flake of the SAWPATH witness pin (section 6), off-reference, if anyone wants it widened.
