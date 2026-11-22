# M8 PF-5 -- results (gate: DECLARATION-PF-5.md, -r1, -r2)

Code commit `01e84165d7ca200483c5ccedf0dac05608893360` (`a8cd3005c` + comment-only counter docs); arm A base `0ee37c2ca`.
Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, `MKL_NUM_THREADS=1`; co-run per CLAUDE.md section 7
(one process per physical core, box-lane, wall-clock informational only). Evidence: `gate-run1/` (P1, P2), `gate-r2/` (P3-P7).
`gate-r1/` + `gate/` are r1's void P3 (output-directory split, see DECLARATION-PF-5-r2.md) and are not gate evidence.

## Legs

| Leg | Result | Source |
|---|---|---|
| P1 ctest Release + Debug | PASS, 4049/4049 each | gate-run1 |
| P2 replay A vs B x3 (walk/ssn/ipm) | PASS, 0 differences (27 cells, 75 columns each) | gate-run1 |
| P3 TRAIN co-run (476 rows) | PASS, ROWS_AS_DECLARED, 0 failing, 0 in-gate residual diffs | gate-r2/train-check.txt |
| P4 nlp-35 (15 rows) | PASS, ROWS_AS_DECLARED, 0 failing | gate-r2/nlp-check.txt |
| P5 instructions criteria | PASS: TRAIN sqp B/A 0.9913, ipm 0.99999; ACOPP118 B/A 0.852 (<=0.9); YORKNET 0.889 (<=0.95) | gate-r2/P5.txt |
| P6 mutations | PASS, MUTATIONS_AS_DECLARED (control 0 killers; X1 D1 F1 F2 M1 M2 M3 C1 T1 Q1 each killed) | gate-r2/P6.out |
| P7 install smoke | PASS (export contract OK, clang rc=0) | gate-r2/P7.out |

## symbolic_analysis_reuses

| Population | rows with hits | total reuses | sparse-arm (platform analyses saved) | dense-arm |
|---|---|---|---|---|
| TRAIN (476 rows; 238 sqp) | 52 (21.8% of sqp rows; ipm rows carry none) | 15 631 | 9 691 | 5 940 |
| nlp-35 (15 rows) | 12 (80%) | 24 744 | 24 726 | 18 |

Backend analyses (sum, hven-sqp rows, A -> B): TRAIN 94 977 -> 85 286 (-9 691, = sparse-arm reuses exactly);
nlp 210 682 -> 185 956 (-24 726, = sparse-arm reuses exactly). Reuse hit rate against analyses that would otherwise run
(reuses / A backend analyses): TRAIN 16.5% (all arms) / 10.2% (sparse arm); nlp 11.7% / 11.7%. `backend_symbolic_repeats`
(PF-4): TRAIN 46 579 -> 36 888, nlp 52 741 -> 28 015. The TRAIN counter totals equal run 1's diagnostic reading
(15 631 / 9 691 / 5 940), reproduced from scratch in r2.

## Notes

- Code-path comparison: HEAD's code paths equal the code commit (orchestrator-checked); P1/P2 of run 1 stand per r1/r2.
- CI: run 44879627250 @ a8cd3005c green; the only code-path delta since is comment-only (`solver_counters.h`).
- UNOBSERVED: no Apple/Accelerate timing or counters beyond CI's identity tests (macOS PF-5 tests passed).
