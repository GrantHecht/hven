# Task 6 wall-leg record (M4, hven side)

Instrument: `.scratch/task-6/ab/ipm_wall_leg.sh` (copied here) — whole solves of a
dense-Jacobian NLP through NLPSolver, 9 reps × 15 inner solves, base/head alternated
per rep, serial arm (MKL_NUM_THREADS=1, pinned) and threaded arm. `aggregate.py`
prints median and minimum estimators with the base rep-spread; read deltas off the
MINIMUM estimator. Binaries built by `build_ipm_time.sh` against each commit's
libhven.a (flags in `provenance_common.txt`). BASE = 3208dcd for every run except
the isolating A/B, whose base is 6b5fc12.

Standing policy: +3.0% band at serial n=240; positive evidence auto-closes; hot-path
symbol growth investigates at any size. Identity: every run bit-identical
(flag/xnorm2 columns equal across base/head at n=60/120/240).

| run | HEAD | serial n=240 median / min | box | status |
|---|---|---|---|---|
| run1 | cf7f2fb | +1.43% / +0.21% | not recorded as contended | ASSERTED; small-n +6–9% → nm showed eval_hessian_values 172→1498 B (deep copies) → fix A |
| run2 | 239a35e | −2.44% / −2.34% | quiet (0.4–1.7%) | ASSERTED; copies gone (eval_jac_e 0x8c1→0x53, eval_hess 0xada→0x59) |
| run3 | 6b5fc12 | −2.29% / −2.02% | CONTENDED (two OpenCode sessions; spread 5–28%) | INFORMATIONAL (settler ruling) |
| run3p | 7f72857 | +0.005% / +0.48% | quiet | INFORMATIONAL: in band, but nm showed the in-function throw outlined write_adjoint_gradient (new 0x280 symbol; adjointhessian piece 0x2de→0x41f) → fix 46d6ff8. The 7f72857 binary was overwritten before an nm dump was saved; sizes quoted from the session record |
| run3pp | 46d6ff8 | +0.06% / +0.06% (threaded −0.15% / +0.25%) | quiet (n=240 spread 1.07%) | ASSERTED — in band, closes on positive evidence |
| ab_6b5fc12_vs_46d6ff8 | 46d6ff8 vs base 6b5fc12 | +1.69% / +2.31% | quiet but noisy (base spread 4.4% at n=240) | ISOLATING: the 6b5fc12 −2.3% is not retained at HEAD; mechanism not established (per-iteration source delta = one integer compare + branch; symbol sizes restored per nm_head_46d6ff8 vs nm_head_6b5fc12) |

Asserted series: run1, run2, run3pp. Informational: run3, run3p. Small-n (+5% n=60,
+2–3% min n=120) is constant across runs 2–3pp and closed by attribution to the
once-per-transcription setup evaluation (pins NlpAdapterHostTest.TranscriptionSpendsOneStartPointAndThreeDerivativeCalls,
NLPSolverTest.ASecondSolveTranscribesNothingAndSpendsNoFurtherSetupEvaluation).

Registered Task 9 (IPM) re-measure: at the next IPM-path change, re-run the leg and
check whether the 6b5fc12 reading returns; if the ~2% A/B delta reproduces, objdump
the adjoint fill loop at both commits.

Files: `<run>_per_rep.csv` (raw per-rep rows: arm, rep, side, n, inner, median_s,
min_s, flag, xnorm2), `<run>_aggregate.txt`, `<run>_provenance_header.txt`;
`nm_<side>_<commit>.txt` symbol-size dumps (NLPAdapterCore / NLPConstraintPiece /
NlpProblemModel / nlp_require_* filtered) for 3208dcd, cf7f2fb, 239a35e, 6b5fc12,
46d6ff8 (nm_head2/nm_head3 = the run2/run3 fuller dumps); `identity2/3.txt`.
