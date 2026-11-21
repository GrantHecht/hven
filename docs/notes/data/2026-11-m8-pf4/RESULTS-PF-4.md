# RESULTS-PF-4 -- the Schur border batch's one multi-RHS K0 solve, and the boundary analysis tally

Declarations: `DECLARATION-PF-4.md` (run 1, `gate/`), `DECLARATION-PF-4-r1.md` (run r1, `gate-r1/`, after an instrument
defect at P4a). Code commit `ea71502aa7789f27931e63d1ab6d10fa02b20e8b` on `origin/m8` `aca3c0afd`. AMD Ryzen 7 5800X3D,
clang 22.1.8, MKL 2026.2, `MKL_NUM_THREADS=1`. No wall-clock is quoted anywhere below; every performance figure is
`instructions:u` (perf stat), the asserted currency (CLAUDE.md section 7).

## 1. Verdict

**ALL_LEGS_PASSED** (run 1: P1-P3, `gate/orchestrate.out`; r1: P4a, P4b, P5, P6, `gate-r1/orchestrate.out`). Every declared
mover moved exactly as declared and nothing else did; no TRAIN or nlp-35 mover is WORSE by status. CI green on all four
lanes on the code commit (run 44712385076) and on the declaration head `9b09a934f` (run 44723560726).

**This is a declared numerics event.** Pardiso's multi-RHS solve does not reproduce k single solves bit for bit, so the
SQP trajectories move on hard cells. Whether it ships is the settler's decision (section 5).

## 2. Step 1 (before building)

- Batch size (callgrind, adds per sync that added a border): ACOPP57 8.2, SMBANK 1.3, DALLASM 13.3; on the probe's
  fourteen profile cells the mean batch is 1.3 (SMBANK) to 508 (BIGBANK), 25-94 on the add_border-dominated cells.
- Microbenchmark, inside the real batches (`step1/summary.txt`): one multi-RHS solve costs 0.36-0.72 of the k single
  solves (k >= 2), the same at k = 1. The saving is 10-51 % of the cell's whole instruction count on every
  add_border-dominated cell (SPANHYD 51 %, DNIEPER 47 %, MESH 40 %, READING6 36 %, CRESC50 26 %, ACOPP118 21 %, DALLASM 17 %,
  BIGBANK 17 %, CRESC100 16 %, YORKNET 13 %, STEENBRC 12 %, MSS1 10 %, ACOPP57 10 %; SMBANK 0.7 %). GO.
- Bit identity: NO. Sparse-arm columns differ up to 1.1e-3 relative (STEENBRC), the dense arm's `dsytrs` at 2.2e-16;
  k = 1 columns are identical.

## 3. The symbolic-analysis counter

- `side.symbolic_analyses` counts the border/rebuild path's analysis decisions (plus the verdict refinement's fresh
  factor); the elimination path's EQP factors (`solve_eqp` -> `factorize_checked`, a new `KktFactor` per call site) analyze
  uncounted. callgrind: DALLASM 296 backend analyses, 294 through `solve_eqp`, against the counter's 3; ACOPP57 50 vs 1.
  The `QpCounters` doc's first sentence over-claimed and is corrected; the `SqpCounters` doc already said so.
- New boundary counter: `hven::linear::AnalysisTally` + `SqpCounters::backend_symbolic_analyses` / `_repeats`.
- Repeat rate (probe): process-level 0-84 % (ACOPP118 84, SPANHYD 57, YORKNET 57, MESH 55, CRESC100 49, CRESC50 37,
  ACOPP57 26, READING6 14, MSS1 11, STEENBRC 9, DALLASM 3, BIGBANK 0, SMBANK 0); on the SAME factor (what a per-factor
  reuse could take): ACOPP118 7 491 of 8 950, YORKNET 7 697 of 14 520, CRESC100 2 842 of 17 092, CRESC50 1 421 of 3 969,
  the rest small. Reuse NOT built (DECLARATION-PF-4 section 7).

## 4. The gate

| leg | result |
|---|---|
| P1 ctest | Release and Debug: "100% tests passed, 0 tests failed out of 3937" (`gate/suites/`) |
| P2 replay x3 | `REPLAY_AS_DECLARED`: each run vs arm A walk 3 (the residual-class columns of `f7_n800_path_warm`), ssn 0, ipm 0; vs arm B's dev rows 0 in every run and mode (`gate/replay/`) |
| P3 TRAIN 238 x D/K | `I4_PASS`: 476 rows equal their dev rows; 33 movers exactly `movers-train.txt` (1 BETTER, 32 NEUTRAL, 0 WORSE); 0 missing or capped. `P3_INSTRUCTIONS` hven-ipm B/A 1.000024, hven-sqp B/A 0.591225 (`gate/train/`) |
| P4a corpus runtime (walk) | run 1 VOID (instrument defect: `analyze_r5.py` not copied; `DECLARATION-PF-4-r1.md`); r1: 81 (round, cell) pairs, counter identity holds (residual columns of the declared cell excepted), each arm's rounds identical; 162 processes, 0 throttled; filtered B/A 0.999701 <= 1.005 (`gate-r1/runtime/`) |
| P4b nlp-35 | `NLP_AS_DECLARED`: A and B rows equal their dev rows, 14 movers exactly `movers-nlp.txt`, none WORSE; total instructions B/A 0.998930 (reported, no threshold) (`gate-r1/nlp/`) |
| P5 mutations | `MUTATIONS_AS_DECLARED`: control clean; B1-B7, T2-T6 KILLED; R1, R2 recorded with no killer (`gate-r1/mutation/`) |
| P6 install smoke | smoke and export contract rc 0 on GCC and clang++ (`gate-r1/install-smoke/`) |

## 5. Movers and costs (for the settler)

Full tables: `DECLARATION-PF-4.md` section 3; rows in `gate/train/train_check.txt`, `gate-r1/nlp/nlp_check.txt`.

- **TRAIN (DN-3 roster, 476 rows)**: 33 hven-sqp movers, 0 hven-ipm. BETTER: CRESC100.K (kInfeasible -> kOptimal). NEUTRAL
  with a cost or route worth naming: CRESC100.D kOptimal 40 -> 211 iterations (factorizations 26023 -> 16762); CRESC50.K
  kNumericalError -> kIterationLimit (138 -> 279 iterations); MESH.K kInfeasible at 5 iterations -> kNumericalError at 93
  (objective -0.245 -> -56.7; a definite claim replaced by a failure); MSS1.D/.K, SPANHYD.D/.K stay kNumericalError;
  HS99EXP.K stays kIterationLimit. The rest are optimal rows with the same objective to 1e-9 relative and small route
  differences. Summed over hven-sqp: iterations +8.2 % (D) / +12.5 % (K), qp minors -0.2 % / +0.2 %, factorizations -8.2 % /
  -3.5 %, optimal rows 105 -> 105 (D), 104 -> 105 (K).
- **nlp-35 (15 cells)**: 14 move, every status unchanged. ACOPP118 kOptimal with qp minors 10361 -> 33922 and iterations 8 ->
  14 (instructions x2.04); STEENBRB and STEENBRD reach kOptimal at a HIGHER local objective (9086.19 -> 9203.80, +1.3 %;
  9030.08 -> 9074.19, +0.5 %; iterations 67 -> 107, 85 -> 107); the other twelve move in route counters or residual digits only.
- **Replay**: one residual-class difference set (f7_n800_path_warm, ~3e-8 relative); counters and statuses identical in all
  three modes. **Tests**: the STEENBRB structural-first premise (dep_structural_first >= 1 on the border algebra, Release)
  is lost and recorded instead of asserted.

## 6. Instruction savings on the profile cells

Asserted instruction evidence (perf stat `instructions:u`, serial, `MKL_NUM_THREADS=1`):

- TRAIN hven-sqp total B/A **0.5912** (D 0.5302, K 0.6544); hven-ipm 1.0000. On the 205 hven-sqp rows whose counters did not
  move, B/A 0.82 (dev). The PF-3 add_border-dominated cells (TRAIN rows, B/A): SPANHYD.D 0.579, SPANHYD.K 0.487, MESH.D 0.602,
  MESH.K 0.716, DNIEPER.D 0.541, DNIEPER.K 0.615, CRESC50.D 0.708, CRESC50.K 0.926, CRESC100.D 0.359, CRESC100.K 0.737, MSS1.D
  0.507, MSS1.K 0.860 (these are movers: trajectory and per-solve savings mix; the probe isolates the per-solve part, section 2).
- nlp-35 (B/A): READING6 0.649, STEENBRE 0.833, BIGBANK 0.834, STEENBRG 0.842, YORKNET 0.871, STEENBRC 0.882, STEENBRD
  0.886, ZAMB2-8 0.887, DALLASM 0.905, ACOPP57 0.925, ACOPR57 0.981, SAWPATH 0.992, SMBANK 0.996, STEENBRB 1.052, ACOPP118
  2.036; total 0.9989 (ACOPP118's trajectory cancels the rest).
- Corpus walk (27 synthetic QP cells, counters identical): filtered B/A 0.9997.
- Largest increases on rows that did not move: +0.2-0.4 % on 7-30 M-instruction HS/small-dense cells (inside PF-3's measured
  0.2-1.2 % run-to-run spread for cells of that size); the batch-of-one rule removed an earlier +3.5 % on HS33.D.

## 7. Open items

1. **Ship or not (settler)**: the numerics event and its cost movers (section 5) against the savings (section 6).
2. **Symbolic reuse** not built: per-factor cache design with its hazards (DECLARATION-PF-4 section 7); worth about 16 % on
   ACOPP118 and 10 % on YORKNET by the probe.
3. **Rebase** onto `origin/m8` (RS-6b advanced it, tests and Windows only) before merge; the gate is on `aca3c0afd`.
4. The trace reader (hven-bench `schema/trace_v1.py`) and the lane schema pick up two new keys / `side.*` columns.
5. Accelerate: the batch's multi-RHS rounding against single solves on real hardware is UNOBSERVED beyond the CI fixture
   (10 x 10, passed bit-identical on the macOS lane).
6. R1/R2: `solve_mat`'s retake arms have no killing fixture (no test lands a border batch on retake numerics).
