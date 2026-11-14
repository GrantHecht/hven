> Written from the DN-0 implementer's hand-back (2026-11-07), committed by owner approval (Grant, 2026-11-08). Settler rulings on this round: C259 (DN-1 fires for the SQP under the declared reading; pooled agreement is a binding DN-1 design input; IPM undecided) and C262 (IPM re-measure after the dsecnd() fix, docs/notes/data/2026-11-m8-dn0-ipm-r1/: declared reading fires but carries no projected-saving weight). The secondary IPM reading below is NON-EVIDENTIAL (results/rule-notes.txt). The SQP OpenMP start-up correction is in docs/notes/2026-11-m8-dn1-design.md.

# M8 DN-0 — the small-dense measurement round (REPORT)

Declaration `DECLARATION.md` (`90489dbf`, before the first leg, not edited). Every leg ran as declared; no failure, no successor. hven `58dc14bc`; harness `6ad47d7` (evidence-only pin move, reverted in `6f2dfe9`). 238 TRAIN cells (HS TRAIN 22 + small-dense CUTEst TRAIN 97, both engines); HOLDOUT not run. All figures MKL Pardiso, Linux x86-64; Apple/Accelerate UNOBSERVED. Instruction counts asserted; wall-clock informational. Re-derive: `python3 tools/rule.py <run dir> results/`.

## 1. Rule outcome — DN-1 fires (SQP)

| median cell | (B_sym+B_fac)/total | rho | neg-eig agree | DN-1 | B_kkt (DN-2) | dominant |
|---|---|---|---|---|---|---|
| IPM / HS: hs026 (11/22) | 0.0006 | 0.041 | 20/20 | no | 0.000 | B_setup 0.999 |
| IPM / small-dense: HS67 (49/97) | 0.0013 | 0.164 | 12/12 | no | 0.000 | B_setup 0.998 |
| SQP / HS: hs043 (11/22) | 0.877 | 0.061 | 7/7 | yes | 0.019 | B_fac |
| SQP / small-dense: HS84 (49/97) | 0.863 | 0.046 | 50/50 | yes | 0.028 | B_fac |

- SQP: DN-1 fires (both median cells hold; sampling s.e. on shares about 0.014).
- IPM: DN-1 does not fire. B_setup is at least 0.98 on every IPM cell, because of the process-once init (section 3).
- Milestone: DN-1. DN-2 does not fire (no cell has B_kkt >= 0.25); per the brief it is re-read after DN-1.
- Secondary steady-state reading (informational): DN-1 for both engines. Its IPM half is NOT reliable (about 3-6 samples per cell after the init is removed; O1).

## 2. Bucket shares (primary; per-cell median [instruction-weighted aggregate])

| | B_sym | B_fac | B_kkt | B_eval | B_setup | B_eng |
|---|---|---|---|---|---|---|
| IPM / HS | 0.000 [0.000] | 0.001 [0.001] | 0.000 [0.000] | 0.000 [0.000] | 0.999 [0.999] | 0.000 [0.000] |
| IPM / small-dense | 0.000 [0.000] | 0.001 [0.007] | 0.000 [0.000] | 0.000 [0.005] | 0.998 [0.987] | 0.000 [0.001] |
| SQP / HS | 0.000 [0.001] | 0.874 [0.876] | 0.024 [0.025] | 0.004 [0.005] | 0.012 [0.009] | 0.086 [0.084] |
| SQP / small-dense | 0.001 [0.001] | 0.867 [0.933] | 0.024 [0.045] | 0.006 [0.000] | 0.017 [0.000] | 0.074 [0.021] |

- On the SQP, (B_sym+B_fac) >= 0.40 on 21/22 HS and 95/97 small-dense cells. The exceptions (hs025, S365, S365MOD) do not factorize.
- B_fac is Pardiso's per-call fixed cost on tiny systems. The top self-symbols are `get_from_c_structure`, `__kmp_serialized_parallel`, `__kmp_fork_call`, `pardiso_c`/`do_all_pardiso_fc`, a timer, and `strcmp` (each 2-5 %), almost all under `FactorSession::run_phase`.
- B_sym is at most 0.006 everywhere.
- Attribution quality:
  - unmatched share at most 0.02 on the median cells (cell max 0.20, hs025, 165 samples);
  - leaf fallback at most 0.013 on the median cells;
  - no lost samples in any cell;
  - B_proc (excluded) is 13-27 % of the process on the SQP median cells.

## 3. The IPM record and the registered lever (B_setup)

`hven::detail::ensure_solver_initialized()` (`src/drivers/solver_init.cpp`) calls MKL `dsecnd()` once per process. Its first call spins to calibrate the clock (`mkl_serv_get_clocks_frequency` -> `difftime`).

| | process instructions |
|---|---|
| IPM process | median 4.01e9 (range 6.3e8-5.3e9 over 714 processes) |
| SQP process, HS | median 1.66e7 |
| SQP process, small-dense | median 3.41e7 |
| IPM steady-state solve | about 2e6 (HS) / 4e6 (small-dense) |

- The spin is wall-time dependent. IPM round-to-round spread has a median of 3-10 % and a maximum of 55 % (SQP 0.1 %).
- Wall-clock (informational): about 1.0 s per IPM cell against about 0.2 s for the SQP.
- Registered lever: this one warm-up call. It is walk-neutral and feeds only `solver_initialization_seconds` and the `kSolverInitialized` message. It needs its own task. DN-0 takes no action.

## 4. rho

| | median [q25-q75] | <= 0.5 | rho_engine_weighted median |
|---|---|---|---|
| IPM / HS | 0.048 [0.041-0.059] | 22/22 | 0.194 |
| IPM / small-dense | 0.067 [0.058-0.097] | 90/95 | 0.314 |
| SQP / HS | 0.042 [0.038-0.046] | 21/21 | 0.106 |
| SQP / small-dense | 0.058 [0.044-0.077] | 92/95 | 0.140 |

- rho > 0.5 only on the largest KKT systems. These are the inputs for DN-3's size threshold:
  - IPM: ACOPP30 1.82 (n 296), CRESC100 1.76 (n 406), LAKES 1.00, NET1 0.72, CRESC50 0.69;
  - SQP: LAKES 1.35 (n 246), CRESC100 1.12, NET1 0.71.
- The weighted variant is < 1 on 95/95 SQP small-dense cells and 82/95 IPM small-dense cells.
- Densification adds about 3 %.
- 48 cells exceeded the first-50 dump cap.

## 5. Negative-eigenvalue agreement

On the median cells it is 100 %. Pooled over 5501 matrices:

| | pooled agreement | cells >= 99 % |
|---|---|---|
| IPM / HS | 1.000 | 22/22 |
| SQP / HS | 1.000 | 21/21 |
| IPM / small-dense | 0.976 | 85/95 |
| SQP / small-dense | 0.933 | 77/95 |

- Cells below 99 % are mostly near-singular or failing walks, where pivot perturbation and Bunch-Kaufman split near-zero pivots differently:
  - SQP: OPTPRLOC 0.32, CRESC50 0.42, ACOPP30 0.56, MESH 0.58, CRESC100 0.60, ALLINITC, HS99EXP, BURKEHAN, LOOTSMA, CRESC4, LAUNCH, HS92, MSS1, MISTAKE, HS108, SPANHYD, HS88, LEWISPOL;
  - IPM: BT8 0.09, LAKES 0.61, LEWISPOL 0.64, ACOPP30, MSS1, LSNNODOC, SPANHYD, HS61, WATER, DECONVC.
- Engine-recorded inertia against dense agrees at the same rate.
- Replay fidelity is 1.000 on the SQP and 0.988 on the IPM (the IPM reuses its symbolic analysis with weighted matching).
- The iparm identity check passed on 5501/5501 matrices.

## 6. Status rows and caveats

| | HS | small-dense |
|---|---|---|
| IPM | 22/22 optimal | 89 optimal, 1 acceptable, 3 max_iter, 3 numerical_error, 1 diverging |
| SQP | 22/22 optimal | 81 optimal, 8 infeasible, 7 numerical_error, 1 max_iter |

- No cell was capped; the longest was SQP SPANHYD at 59 s.
- Reference exit 3 on hs025 (both engines) and hs033 (SQP) are the known non-optima.
- Carried caveats are in `cells.csv`. HS67, the IPM small-dense median cell, carries a derivative-check mismatch; its rule reading does not depend on derivatives.
- LOOTSMA reproduces as infeasible (the registered false-infeasibility candidate).

## 7. Deviations

- No hven change. The KKT dump is a link-time `--wrap=pardiso` in a separate build.
- The sampling parameters and the secondary reading came from the pre-declaration check on non-population cells (DECLARATION section 6).
- The pin is built with `-fno-asynchronous-unwind-tables`, so SQP chains stop near `solve_impl_body`. This affects only B_setup/B_eng anchoring.
- The IPM median selection by total instructions is effectively random among init-dominated cells; the outcome is the same for all of them.

## 8. Open items

- O1: IPM steady-state shares need an instrument that excludes init. Done in r1, docs/notes/data/2026-11-m8-dn0-ipm-r1/.
- O2: inertia on degenerate systems is an input to the DN-1 design page.
- O3: the dsecnd() lever as its own task. Done in F1, docs/notes/data/2026-11-m8-f1-init/.
- O4: the Pardiso per-solve overhead supports the post-M8 dense QP updates.
