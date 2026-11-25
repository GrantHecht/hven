# RESULTS-I19-2a

Round: M8 I19-2a, measured under `DECLARATION-I19-2a.md` (frozen at hven commit `b6c5fe9cbe72ba4af0765dc6872ba2c464555bc4`,
2026-11-24T12:22:48-05:00, settler ledger C524; the file was not edited after the freeze). Evidence: this directory
(`gate-I19-2a/`); stores (not committed, git-ignored in the harness checkout, which `run_serial` requires to stay clean):
`hven-bench-i19/.scratch/i19-2a/store-{R,H,Hbase,alone}`; the rows the tables read are reproduced by the scripts in `scripts/`.
Code head under test `bae35b204`; binaries per `BINARIES.txt` (R lane, H lane, grid), harness `i19` at `56fa03fdf40acb38275f0a46058ddbd2da925f0e`.

## Verdict

**Nothing ships. No candidate. No composite K.** No G-arm and neither comparator passed stage 1; the placebo P (the default
radius perturbed in the 15th digit) itself fails the stage-1 "no -1 sign" condition and moves 198 live rows, so the instrument's
noise floor is as large as the largest arm effects. The V6 owner replay is not required (see `OWNER-REPLAY-REQUEST.md`, which
carries the arm labels and file shape for an optional replay). Elastic persistence and warm-start carry are not part of this round (O3).
The round did not STOP on a declared stop. Two harness defects and one runner slip are disclosed under Deviations; the official
`stage2` read stops on one of them (A10 vs the ten NLP-46 cap rows), which does not change the outcome (no arm reaches stage 2 as a candidate).

## Provenance and co-run protocol

Toolchain clang 22.1.8 (Fedora), Release, `HVEN_FP_MODE=SAFER_FAST`, sequential MKL, no LTO, Linux x86-64; one box lane (4 physical
cores) per co-run leg, one process per pinned physical core, SMT siblings idle, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`;
`--exclusive` used only for the NLP-46 exclusive tier. Counters, statuses and objectives asserted; wall-clock never read.
Apple and Windows readings are CI only. Hardware: the shared 8-physical-core box. Dates 2026-11-24/25.

## Leg verdicts against the bars

| leg | result | bar | verdict |
|---|---|---|---|
| B | `B/legB.txt`: R lane, H lane and grid binary shas equal `BINARIES.txt`; the R and H installs and both lane builds (pre-freeze logs, rc 0) | every build rc 0, shas recorded | PASS (builds were pre-freeze; not repeated, Deviation 2) |
| C | `C/`: Release 4175 of 4175, Debug 4175 of 4175 (`ctest -j4`), at the freeze commit (code == `bae35b204`); 120 I19GrowthRule / I19RadiusCycle / I19SensGrid result lines in each log, all passed | 100% both | PASS |
| N1 | `N1/`: 27 cells x walk, ssn, ipm; base x1 (R `afb65b804`), head x3; `compare_replay.py` 75 columns each: 0 differences in all nine comparisons | 0 differences | PASS (recorded INSENSITIVE to SQP globalization) |
| N2 | `N2/N2.txt`: R vs H0, **822 cells** (TRAIN-238, CAP-238-1, CAP-238-01, NLP-46 complete, PILOT-62 complete): **0 differences**; H-only side columns exactly the six declared counters, none undeclared. `N2/N2-first-VOID-pilot-rows.txt` is the first read, void for PILOT-62 (Deviation 5) | 0 differences | PASS |
| N2b | `N2b/N2b.txt`: H0 under the i19-base lane binary (harness `b026453`, sha `08e176dd...`) vs H0 under the head lane: 776 complete cells (TRAIN-238, both CAP corpora, PILOT-62) plus the 26 NLP-46 cells present on both sides: **0 differences** | 0 differences | PASS, REDUCED (NLP-46 co-run cells only; Deviation 6) |
| N3 | `N3/N3.txt`: G6 vs H0 on TRAIN-238, 238 cells, no moved row | any moved row STOPS | PASS (the expected VACUOUS reading) |
| M | `M/legM.stdout`: control clean; 53 mutations: 48 killed, 5 survived = {M18, M21} (declared EXPECTED survivors, script-encoded) and {M32, M33, M35} (declared ARGUED, C524-accepted, "not must-kill" in the table). Script verdict string `MUTATION_SURVIVED` because its `ARGUED` list holds only M18 and M21; against the frozen table every must-kill is killed | every must-kill killed | PASS AS DECLARED BY THE TABLE (Deviation 7; flagged for the settler) |
| S1 | 10 arms (P, G1-G7, C1, C2) plus H0 on TRAIN-238, CAP-238-1, CAP-238-01 and SENS-P: `S1/stage1.txt`, tables below | stage-1 table per 5.2 | DONE: **no arm passes** |
| S1b | K does not exist ("K members: none"); K and K-i not run | reported | N/A |
| stage-2 selection | `S2/final-tables.txt` (i19_rule.stage2_arm_set): arms **H0, P, C1**; C1 candidate = False | mechanical | DONE |
| S2-N | H0, P, C1 on NLP-46 (tiers from H0's assignment, 12 exclusive cells); informational read `S2/info-stage2.txt` (the official `stage2` read STOPS, Deviation 4) | 5.3 inputs | DONE, see tables |
| S2-P | PILOT-62 under H0, P, C1: `S2/pilot-summary.txt` | V2; solved counts and movers reported | DONE |
| certs / V2 | `certs/certs.txt`, `certs/certificates.csv`: 244 certificate rows over every arm and corpus incl. PILOT-62; all STATIONARY (3 C1 rows APPROXIMATELY STATIONARY); **0 false certificates**, none under H0; all re-runs reproduced | zero false certificates | PASS |
| V6 owner replay | not run (not mine; not required: no candidate) | | N/A |
| Sm | `Sm/`: install smoke and export contract, default and clang front ends, all rc 0 | rc 0 | PASS |
| CI | run `45528580294` on commit `c0422e1ed1e884ee93680758211ea2537d614d25` (code identical to the head; only docs after it): macos-clang-release 34m18s, windows-clang-release 40m10s, linux-clang-release 58m3s, linux-clang-coverage 58m36s, **all four green** (`CI/ci-watch.txt`) | four lanes green | PASS |

## Stage-1 table (TRAIN-238 + CAP-238-1 + CAP-238-01 + SENS-P; live cells = TRAIN and CAP, SENS-P a guard/veto corpus)

Conditions per 5.2: (a) V1 (no family loses a solve; strict and optimal+acceptable), (b) no family sign of -1 on any family, (c) V3'
(PASS / VACUOUS / FAIL), (d) V4 tail guard, (e) moves a row. Pass = (a)-(e). "plus" = K-entry signals (none reached K entry
because no arm passed). LA/EV net = pooled net wins / sign on the live cells.
| arm | pass | (a) V1 | (b) no -1 | (c) V3' | (d) tail | (e) moves | lost | tail cells | live moved | pooled net strict | LA net/sign | EV net/sign | plus |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| P | NO | yes | NO | PASS | yes | yes | 0 | 0 | 198 | +0 | +4/+1 | -3/-1 | - |
| G1 | NO | NO | NO | PASS | NO | yes | 2 | 5 | 76 | -2 | +6/+1 | +9/+1 | LA,EV |
| G2 | NO | NO | NO | PASS | NO | yes | 1 | 2 | 16 | +0 | +1/+1 | +1/+1 | - |
| G3 | NO | yes | yes | PASS | NO | yes | 0 | 1 | 25 | +0 | +3/+1 | +1/+1 | - |
| G4 | NO | NO | NO | VACUOUS | yes | yes | 2 | 0 | 18 | -1 | -4/-1 | -2/-1 | - |
| G5 | NO | NO | NO | PASS | NO | yes | 3 | 2 | 44 | -1 | +7/+1 | +11/+1 | LA,EV |
| G6 | NO | yes | NO | PASS | NO | yes | 0 | 3 | 79 | +1 | +3/+1 | +7/+1 | EV,cap |
| G7 | NO | NO | NO | PASS | NO | yes | 2 | 3 | 43 | -1 | +4/+1 | +0/+0 | - |
| C1 | NO | NO | NO | FAIL | NO | yes | 20 | 50 | 107 | -4 | +14/+1 | +19/+1 | LA,EV |
| C2 | NO | NO | NO | FAIL | NO | yes | 10 | 38 | 59 | +0 | +3/+1 | +4/+1 | EV |
Placebo floor (P's own absolute movement on the live cells): solved 0, LA 4, EV 3 (for G6's CAP-only live set: solved 2, LA 4, EV 1).
P's own conditions: (a) yes, (b) NO (family -1 signs: cap01-x LA/EV, cap1-q EV, hs-q EV, hs-x EV, sd-q EV), (c) PASS (precheck vacuous), (d) yes, (e) yes.

Why each arm fails: G1 (a)(b)(d); G2 (a)(b)(d); **G3 fails (d) only** (one cell, HS102/sqp-qn rejected steps 9 -> 35; threshold max(29, 27) = 29; reproduced alone);
G4 (a)(b) and VACUOUS V3' as declared-expected; G5 (a)(b)(d); **G6 fails (b) and (d)** (cap01-x EV -1, cap1-q LA/EV -1; MESH and CRESC100 tails); G7 (a)(b)(d); C1 and C2 (a)(b)(c)(d).
That (b) fails for P itself means a "-1" family sign is within placebo noise on this corpus; the rule is applied as frozen.

### Lost cells and tail-guard cells per arm (stage-1 corpora)
Lost cells (strict-optimal under H0, not under the arm; every one re-run alone, reproduced) and R1 tail-guard cells, per arm (stage-1 corpora; SENS-P lost cells are sensfix rows).

### G1: 2 lost, 5 tail lines
- LOST TRAIN-238 ACOPP30/default sd: optimal -> numerical_error
- LOST TRAIN-238 DEMBO7/sqp-qn sd: optimal -> stalled
- TAIL CAP-238-1 DEMBO7/sqp-qn sd rejected_steps 1 -> 28
- TAIL CAP-238-1 HS91/default sd rejected_steps 11 -> 66
- TAIL CAP-238-1 HS91/default sd reject_funnel_ceiling 0 -> 54
- TAIL TRAIN-238 HS91/default sd rejected_steps 11 -> 66
- TAIL TRAIN-238 HS91/default sd reject_funnel_ceiling 0 -> 54

### G2: 1 lost, 2 tail lines
- LOST CAP-238-1 CRESC4/default sd: optimal -> infeasible
- TAIL CAP-238-01 CRESC100/default sd majors 2 -> 46
- TAIL CAP-238-01 CRESC100/default sd EV 13 -> 101

### G3: 0 lost, 1 tail lines
- TAIL TRAIN-238 HS102/sqp-qn sd rejected_steps 9 -> 35

### G4: 2 lost, 0 tail lines
- LOST CAP-238-1 CRESC100/default sd: optimal -> max_iter
- LOST TRAIN-238 CRESC100/default sd: optimal -> max_iter

### G5: 3 lost, 2 tail lines
- LOST CAP-238-01 DECONVC/sqp-qn sd: optimal -> max_iter
- LOST CAP-238-1 CRESC100/default sd: optimal -> max_iter
- LOST TRAIN-238 CRESC100/default sd: optimal -> max_iter
- TAIL CAP-238-01 CRESC100/default sd majors 2 -> 40
- TAIL CAP-238-01 CRESC100/default sd EV 13 -> 110

### G6: 0 lost, 3 tail lines
- TAIL CAP-238-01 CRESC100/default sd EV 13 -> 43
- TAIL CAP-238-01 MESH/default sd majors 52 -> 268
- TAIL CAP-238-01 MESH/default sd EV 141 -> 562

### G7: 2 lost, 3 tail lines
- LOST CAP-238-1 CRESC4/default sd: optimal -> infeasible
- LOST TRAIN-238 CRESC100/default sd: optimal -> max_iter
- TAIL CAP-238-01 CRESC100/default sd EV 13 -> 44
- TAIL TRAIN-238 HS99EXP/sqp-qn sd majors 1 -> 171
- TAIL TRAIN-238 LAKES/sqp-qn sd EV 44 -> 192

### C1: 20 lost, 50 tail lines
- LOST CAP-238-01 CRESC100/sqp-qn sd: optimal -> numerical_error
- LOST CAP-238-01 DECONVC/sqp-qn sd: optimal -> max_iter
- LOST CAP-238-01 HS74/sqp-qn sd: optimal -> max_iter
- LOST CAP-238-01 HS75/sqp-qn sd: optimal -> max_iter
- LOST CAP-238-01 HS92/default sd: optimal -> numerical_error
- LOST CAP-238-1 CRESC100/default sd: optimal -> stalled
- LOST CAP-238-1 HS88/sqp-qn sd: optimal -> infeasible
- LOST CAP-238-1 HS89/sqp-qn sd: optimal -> numerical_error
- LOST CAP-238-1 HS90/sqp-qn sd: optimal -> infeasible
- LOST CAP-238-1 HS91/default sd: optimal -> infeasible
- LOST CAP-238-1 HS92/sqp-qn sd: optimal -> infeasible
- LOST SENS-P F-CAP/default sensfix: optimal -> max_iter
- LOST SENS-P F-CAP/sqp-qn sensfix: optimal -> max_iter
- LOST SENS-P F-SOC/default sensfix: optimal -> max_iter
- LOST SENS-P F-SOC/sqp-qn sensfix: optimal -> max_iter
- LOST TRAIN-238 ACOPP30/default sd: optimal -> numerical_error
- LOST TRAIN-238 CRESC4/sqp-qn sd: optimal -> max_iter
- LOST TRAIN-238 HS104/default sd: optimal -> infeasible
- LOST TRAIN-238 HS90/default sd: optimal -> infeasible
- LOST TRAIN-238 HS91/default sd: optimal -> infeasible
- (50 tail lines: see stage1.txt)

### C2: 10 lost, 38 tail lines
- LOST CAP-238-01 CRESC100/sqp-qn sd: optimal -> numerical_error
- LOST CAP-238-01 HS74/sqp-qn sd: optimal -> max_iter
- LOST CAP-238-01 HS75/sqp-qn sd: optimal -> max_iter
- LOST CAP-238-1 CRESC100/default sd: optimal -> infeasible
- LOST CAP-238-1 HS101/sqp-qn sd: optimal -> max_iter
- LOST SENS-P F-CAP/default sensfix: optimal -> max_iter
- LOST SENS-P F-CAP/sqp-qn sensfix: optimal -> max_iter
- LOST SENS-P F-SOC/default sensfix: optimal -> max_iter
- LOST SENS-P F-SOC/sqp-qn sensfix: optimal -> max_iter
- LOST TRAIN-238 HS101/sqp-qn sd: optimal -> max_iter
- (38 tail lines: see stage1.txt)


All 58 TRAIN/CAP cells above (lost or tail) were re-run ALONE (one process, serial, `alone/alone-run.log`) and compared with the co-run
rows by the harness comparator: **58 cells, 0 differences** (`alone/alone-compare.txt`). SENS-P cells come from the grid binary, which runs its cells serially in one process; not re-run.

### Per-family V3' (SENS-P) and floor / restoration reports
Per-family V3' (SENS-P families), per arm: precheck cells / moved rows / violations (H0-optimal cells non-optimal under the arm)

| arm | sens-x | sens-q | sensfix-x | sensfix-q |
|---|---|---|---|---|
| G1 | 0 / 0 / 0 | 0 / 0 / 0 | 4 / 3 / 0 | 4 / 3 / 0 |
| G2 | 0 / 0 / 0 | 0 / 0 / 0 | 4 / 2 / 0 | 4 / 2 / 0 |
| G3 | 0 / 0 / 0 | 0 / 0 / 0 | 4 / 4 / 0 | 4 / 4 / 0 |
| G4 | 0 / 0 / 0 | 0 / 0 / 0 | 4 / 0 / 0 | 4 / 0 / 0 |
| G5 | 0 / 0 / 0 | 0 / 2 / 0 | 4 / 3 / 0 | 4 / 4 / 0 |
| G6 | 0 / 0 / 0 | 0 / 0 / 0 | 4 / 1 / 0 | 4 / 1 / 0 |
| G7 | 0 / 0 / 0 | 0 / 0 / 0 | 4 / 4 / 0 | 4 / 4 / 0 |
| C1 | 0 / 0 / 0 | 0 / 1 / 0 | 4 / 2 / 2 | 4 / 2 / 2 |
| C2 | 0 / 0 / 0 | 0 / 1 / 0 | 4 / 2 / 2 | 4 / 2 / 2 |

Floor / restoration reports over the stage-1 corpora (stage-1 report REPORT lines): floor-hit cells, trust_region_floor_exits, restoration entries, restoration cells, radius-floor QN resets

| arm | H0 | arm | tail-guard violation lines |
|---|---|---|---|
| G1 | 4/4/211/113/21 | 7/7/213/113/15 | 5 |
| G2 | 4/4/211/113/21 | 6/6/205/113/16 | 2 |
| G3 | 4/4/211/113/21 | 5/5/203/111/19 | 1 |
| G4 | 4/4/211/113/21 | 6/6/207/111/17 | 0 |
| G5 | 4/4/211/113/21 | 4/4/204/107/16 | 2 |
| G6 | 4/4/211/113/21 | 2/2/217/113/19 | 3 |
| G7 | 4/4/211/113/21 | 5/5/209/112/15 | 3 |
| C1 | 4/4/211/113/21 | 4/4/201/109/12 | 50 |
| C2 | 4/4/211/113/21 | 4/4/204/114/11 | 38 |

G4 at ceiling 3 is VACUOUS for V3' as declared-expected (moves no SENS-P row). `sens-x` and `sens-q` precheck cells: 0 (near-vacuous, as the development reading said); the `sensfix` families carry V3'.

## Stage 2 (H0, P, C1; selection is mechanical)

`stage2_arm_set`: passing stage-1 arms none, K none, so the arm set is H0, P and C1 and "nothing ships" (5.2). C1 is not a candidate (failed stage 1).
The official `i19_run.py stage2` STOPS before reading NLP-46 (A10; `S2/stage2-official.txt`). The informational read below uses the
harness's own `i19_rule` functions on a local loader that treats a row with no counters side row as not-ended (Deviation 4).
It decides nothing.

```
H0 cells 864 (NLP-46 46; not-ended NLP cells 10)
== P: common cells 864 (NLP-46 46)
NLP-46 stand-alone (R3): ok=False {"pooled": {"LA": {"wins": 3, "losses": 3, "ties": 15, "net": 0, "sign": 0}, "EV": {"wins": 2, "losses": 3, "ties": 16, "net": -1, "sign": -1}}, "families": {"nlp-q": {"LA": {"wins": 1, "losses": 0, "ties": 5, "net": 1, "sign": 1}, "EV": {"wins": 0, "losses": 0, "ties": 6, "net": 0, "sign": 0}}, "nlp-x": {"LA": {"wins": 2, "losses": 3, "ties": 10, "net": -1, "sign": -1}, "EV": {"wins": 2, "losses": 3, "ties": 10, "net": -1, "sign": -1}}}, "geo": {"n": 21, "majors": 0.987494084616538, "factorizations": 0.9042907891864956, "eval_f": 0.9829383456461187, "eval_jac": 0.9847038730992168, "evals_values": 0.9533021101808785, "LA": 0.9042907891864956, "EV": 0.98127612033499}, "negative": [["pooled", "EV"], ["nlp-x", "LA"], ["nlp-x", "EV"]]}
NLP-46 pooled: solved {'h0_strict': 24, 'x_strict': 21, 'h0_acc': 25, 'x_acc': 23} | tally {'LA': (0, 0), 'EV': (-1, -1)} | geo {'n': 21, 'majors': 0.987494084616538, 'factorizations': 0.9042907891864956, 'eval_f': 0.9829383456461187, 'eval_jac': 0.9847038730992168, 'evals_values': 0.9533021101808785, 'LA': 0.9042907891864956, 'EV': 0.98127612033499}
NLP-46 tail guard (V4) violations: 1
  TAIL (('NLP-46', 'nlp', 'MSS2', 'sqp-qn'), 'majors', 7, 37)
NLP-46 V1 ok=False lost=[(('NLP-46', 'nlp', 'ACOPR118', 'default'), 'strict', 'optimal', 'acceptable'), (('NLP-46', 'nlp', 'BRITGAS', 'default'), 'strict', 'optimal', 'max_iter'), (('NLP-46', 'nlp', 'SAWPATH', 'sqp-qn'), 'strict', 'optimal', 'numerical_error')]
NLP-46 family signs {'nlp-q': (1, 0), 'nlp-x': (-1, -1)}
restoration/floor {"H0": {"trust_region_floor_exits": 2, "floor_hit_cells": 2, "restoration_entries": 42, "restoration_cells": 19, "qn_resets_radius_floor": 2}, "X": {"trust_region_floor_exits": 2, "floor_hit_cells": 2, "restoration_entries": 45, "restoration_cells": 21, "qn_resets_radius_floor": 3}}
== C1: common cells 864 (NLP-46 46)
NLP-46 stand-alone (R3): ok=False {"pooled": {"LA": {"wins": 6, "losses": 4, "ties": 13, "net": 2, "sign": 1}, "EV": {"wins": 7, "losses": 3, "ties": 13, "net": 4, "sign": 1}}, "families": {"nlp-q": {"LA": {"wins": 1, "losses": 1, "ties": 5, "net": 0, "sign": 0}, "EV": {"wins": 2, "losses": 1, "ties": 4, "net": 1, "sign": 1}}, "nlp-x": {"LA": {"wins": 5, "losses": 3, "ties": 8, "net": 2, "sign": 1}, "EV": {"wins": 5, "losses": 2, "ties": 9, "net": 3, "sign": 1}}}, "geo": {"n": 23, "majors": 1.0694877617010106, "factorizations": 0.9497761878393989, "eval_f": 0.9965308510025055, "eval_jac": 1.0531237822835102, "evals_values": 0.9242057053885251, "LA": 0.9497761878393989, "EV": 1.0211414987219596}, "negative": []}
NLP-46 pooled: solved {'h0_strict': 24, 'x_strict': 23, 'h0_acc': 25, 'x_acc': 25} | tally {'LA': (1, 2), 'EV': (1, 4)} | geo {'n': 23, 'majors': 1.0694877617010106, 'factorizations': 0.9497761878393989, 'eval_f': 0.9965308510025055, 'eval_jac': 1.0531237822835102, 'evals_values': 0.9242057053885251, 'LA': 0.9497761878393989, 'EV': 1.0211414987219596}
NLP-46 tail guard (V4) violations: 5
  TAIL (('NLP-46', 'nlp', 'ACOPR118', 'default'), 'majors', 22, 192)
  TAIL (('NLP-46', 'nlp', 'ACOPR118', 'default'), 'EV', 47, 289)
  TAIL (('NLP-46', 'nlp', 'ACOPR118', 'default'), 'rejected_steps', 6, 98)
  TAIL (('NLP-46', 'nlp', 'ACOPR118', 'default'), 'reject_funnel_ceiling', 6, 88)
  TAIL (('NLP-46', 'nlp', 'YORKNET', 'default'), 'majors', 12, 55)
NLP-46 V1 ok=False lost=[(('NLP-46', 'nlp', 'BRITGAS', 'default'), 'strict', 'optimal', 'numerical_error')]
NLP-46 family signs {'nlp-q': (0, 1), 'nlp-x': (1, 1)}
restoration/floor {"H0": {"trust_region_floor_exits": 2, "floor_hit_cells": 2, "restoration_entries": 42, "restoration_cells": 19, "qn_resets_radius_floor": 2}, "X": {"trust_region_floor_exits": 2, "floor_hit_cells": 2, "restoration_entries": 44, "restoration_cells": 19, "qn_resets_radius_floor": 0}}

```

### H0 coverage, counters, prechecks (`S2/final-tables.txt`)
```
STAGE-2 SELECTION (i19_rule.stage2_arm_set): arms ['H0', 'P', 'C1'] ; C1 candidate=False ; K=none ; passing stage-1 arms: none

COVERAGE (H0 rows per touched site; 864 H0 cells incl. NLP-46):
  growth                 trust_region_growths           hit  617 of  854 rows  HIT
  judged_reject          rejected_steps                 hit  224 of  854 rows  HIT
  floor_test             trust_region_floor_exits       hit    6 of  854 rows  HIT
  soc_applied            soc_applied                    hit   96 of  854 rows  HIT
  nonfinite_rollback     nonfinite_step_rollbacks       hit    1 of  854 rows  HIT
  restoration_sub_solve  restoration_entries            hit  132 of  854 rows  HIT
  cap_binding            step_limit_binding_majors      hit  360 of  854 rows  HIT

H0 default-path values of the six new counters (sum over H0 rows, side column present):
  trust_region_growths                   rows  854 sum       6081 max      135
  trust_region_growth_refusals           rows  854 sum          0 max        0
  trust_region_shrinks_to_step           rows  854 sum          0 max        0
  trust_region_grown_step_rejections     rows  854 sum       1206 max      117
  trust_region_unbound_rejections        rows  854 sum       1174 max       77
  step_limit_binding_majors              rows  854 sum       4092 max      234

CONFOUNDED block (BRITGAS, DALLASM, YORKNET), per arm, over TRAIN/CAP/SENS/NLP-46:
  P   cells=6 status-changed=1 restoration H0=7 X=5  X_R1_violations=0
  G1  cells=0 status-changed=0 restoration H0=0 X=0  X_R1_violations=0
  G2  cells=0 status-changed=0 restoration H0=0 X=0  X_R1_violations=0
  G3  cells=0 status-changed=0 restoration H0=0 X=0  X_R1_violations=0
  G4  cells=0 status-changed=0 restoration H0=0 X=0  X_R1_violations=0
  G5  cells=0 status-changed=0 restoration H0=0 X=0  X_R1_violations=0
  G6  cells=0 status-changed=0 restoration H0=0 X=0  X_R1_violations=0
  G7  cells=0 status-changed=0 restoration H0=0 X=0  X_R1_violations=0
  C1  cells=6 status-changed=2 restoration H0=7 X=8  X_R1_violations=1
  C2  cells=0 status-changed=0 restoration H0=0 X=0  X_R1_violations=0
  P itself: R1 violations on the confounded cells: 0 (P trips R1 there: no); status changes 1

CAP precheck (H0 cells with step_limit_binding_majors > 0) per CAP family:
  cap01-q   114 of 119 cells cap-binding
  cap01-x   113 of 119 cells cap-binding
  cap1-q     31 of 119 cells cap-binding
  cap1-x     52 of 119 cells cap-binding

SENS-P H0 precheck (growth and rejection both > 0) per family:
  sens-q       0 of  24 cells
  sens-x       0 of  24 cells
  senscap-q    1 of  24 cells
  senscap-x    0 of  24 cells
  sensfix-q    4 of   4 cells
  sensfix-x    4 of   4 cells

H0 solved counts per family (strict/acc):
  cap01-q    cells 119 strict 104 acc 104
  cap01-x    cells 119 strict 105 acc 105
  cap1-q     cells 119 strict 105 acc 105
  cap1-x     cells 119 strict 104 acc 104
  hs-q       cells  22 strict  22 acc  22
  hs-x       cells  22 strict  22 acc  22
  nlp-q      cells  23 strict   7 acc   8
  nlp-x      cells  23 strict  17 acc  17
  sd-q       cells  97 strict  83 acc  83
  sd-x       cells  97 strict  83 acc  83
  sens-q     cells  24 strict  24 acc  24
  sens-x     cells  24 strict  24 acc  24
  senscap-q  cells  24 strict  23 acc  23
  senscap-x  cells  24 strict  24 acc  24
  sensfix-q  cells   4 strict   4 acc   4
  sensfix-x  cells   4 strict   4 acc   4

```

### PILOT-62 (S2-P)
```
S2-P PILOT-62 (62 cells per arm). Labels are the pilot's own with the arm's keys merged in (collisions: 0 for H0; see the harness plan).
H0 {'optimal': 20, 'error': 32, 'dnf_budget': 10} kills: 0 certified: 0
P {'optimal': 21, 'dnf_budget': 16, 'error': 25} kills: 0 certified: 0
C1 {'optimal': 10, 'error': 14, 'dnf_budget': 8, 'infeasible': 1} kills: 0 certified: 1
```
P moves 60 of 62 pilot cells and C1 many more (`S2/pilot-summary.txt` lists the movers): the pilot cells are failure-path cells and the
radius perturbation alone changes them, which is the same noise floor read on TRAIN. No solved-count conclusion is drawn.

## The candidate and the decision

Candidate: **none**. Decision: NOTHING_SHIPS (a result). The levers stay as documented opt-in research-tier options (5.4).
The V6 replay is not pending. Closest arms, for the settler's successor decision only: G3 (no lost cell, no -1 family; fails V4 on one cell, HS102/sqp-qn, reproduced alone) and G6 (cap corpora; fails (b), (d)).

## Confounded block

The confounded cells exist only in NLP-46 (6 cells: BRITGAS, DALLASM, YORKNET x 2 base labels; the declaration's wording "TRAIN-238, both CAP corpora" is empty for these problems, they are nlp-35 members)
and PILOT-62 (27). Over NLP-46 (`S2/final-tables.txt`, and the tail/restoration lines in the informational read): P: status changed on 1 of 6, restoration entries H0 7 / P 5, R1 violations 0, so **P does not trip R1 on them**;
C1: status changed on 2 of 6, restoration entries 7 / 8, 1 R1 violation (YORKNET/default majors 12 -> 55). The G-arms have no NLP-46 rows. The restoration-entry counts of every arm over the stage-1 corpora are in the floor/restoration table above.

## Tail-guard, floor and restoration reports

Per-arm V4 violations and floor / restoration quantities: tables above (stage-1 corpora) and the informational block (NLP-46: H0 floor hit cells 2, floor exits 2, restoration entries 42; P 2 / 2 / 45; C1 2 / 2 / 44; radius-floor QN resets H0 2, P 3, C1 0).
NLP-46 read for the record (not a ship read): P loses three H0-optimal cells (ACOPR118/default to acceptable, BRITGAS/default max_iter, SAWPATH/sqp-qn numerical_error) and has one tail cell (MSS2/sqp-qn majors 7 -> 37), i.e. the placebo alone fails the NLP-46 stand-alone conditions; C1 loses BRITGAS/default and has 5 tail lines.

## Deviations (all of them)

1. **Freeze edit.** Declaration lines 3-4 ("FREEZE-READY DRAFT ... fields still marked SETTLER ...") were replaced by the single sentence "The harness fields are filled from the hven-bench `i19` branch." (the first half of that draft sentence became untrue once committed) and `SETTLER: C4xx` became `C524`; nothing else; the freeze commit is the only commit that touches the declaration.
2. **Leg B** verified the pre-freeze builds (BINARIES shas equal the files on disk; build logs rc 0) and did not rebuild: a rebuild changes the shas that the frozen BINARIES.txt names.
3. **Legs overlapped on the two lanes** (S1 arms, N2 and N2b arms, M and the pilot legs were queued concurrently through `box-lane`); the declared order held as to dependencies (no arm result was read before its leg bar's input existed except that stage-1 rows were produced while leg M ran; leg M's verdict does not depend on them, and the rows are under the frozen binaries). The harness checkout must stay clean for `run_serial`, so stores live in its git-ignored `.scratch/`. Decoded CUTEst instances (`.scratch/h3/decoded`, 132 dirs) and the SIF sources (`sets/cutest/sif`) were copied from sibling checkouts (hven-bench-tu1, -dn1, -pf7), byte-compared where they overlap.
4. **Harness defect, A10 vs dnf_wall.** The ten NLP-46 cells H0 runs in the exclusive tier and which hit the 300 s wall cap (MSS2, MSS3, READING8, TAX213322, TAX53322 default; BIGBANK, MSS3, READING8, TAX213322, TAX53322 sqp-qn) end `dnf_wall` with no counters side row, on R and on H0 alike (so N2, which compares both sides, is unaffected). `i19_run.load_arm_data` stops on any such row ("A10: no zero-fill"), so the official `stage2` and `coverage` commands STOP (`S2/stage2-official.txt`, `N2/coverage.txt` is empty because `Stop` hides its message). I did not touch the harness. The informational reads (`scripts/info_stage2.py`, `scripts/final_tables.py`) re-implement only the loader and call the harness's own `i19_rule`. A successor fix to the loader (a dnf_wall row has no side row by construction) is the settler's call; it cannot change this round's outcome because no arm is a stage-2 candidate. The same ten cells hit the cap alone in the exclusive tier, so they are not co-run artifacts.
5. **Runner slip: pilot binary path.** `pilot.sh` passed the lane binary as a relative path and `pilot_worker` runs it from the pilot run directory (rc 127 on all 62 cells for both the R and the H H0 pilots). Those two runs and the first N2 read were VOID (`arms/VOID-*-pilot-relpath.log`, `N2/N2-first-VOID-pilot-rows.txt`; the void store directories are kept as `VOID-PILOT-62-relpath-rc127`); `pilot.sh` was fixed (absolute path) and all pilots rerun; N2 and N2b above are the re-reads on valid rows. The `arm` runs were unaffected (run_serial). Also, `stage2_chain.sh` invoked `pilot.sh` for P and C1 after I had run them manually in parallel: the worker skips finished cells (row counts stayed 63, verified).
6. **N2b scope and binary.** The i19-base lane is not in the frozen BINARIES.txt, so it was built from a detached harness worktree at `b026453` against the H install, with a supplementary `N2b/BINARIES-N2b.txt` (tag `I19 lane H i19-base`) used for the runner's sha check. To avoid a second 2.5-hour exclusive tier, the base side ran NLP-46 co-run cells only; the comparison covers the 26 NLP-46 cells present on both sides (`scripts/n2b.py`). TRAIN-238, both CAP corpora and PILOT-62 are complete.
7. **Leg M verdict string.** Declared survivors M32, M33, M35 are marked ARGUED in the frozen table (C524: "ACCEPTED as argued per F5"), but `mutations_i19.sh` (frozen, in `legs/`) only lists M18 and M21 in `ARGUED`, so its last line reads `VERDICT MUTATION_SURVIVED`. I read the bar as the table states it (every must-kill killed; M32/M33/M35 "not must-kill") and did not stop. If the settler reads the script literally, leg M is a declared stop and this round needs a successor declaration.
8. **Alone re-runs** use `scripts/alone.py` (not a declared script): the bar-deciding TRAIN/CAP cells of every stage-1 arm, run serially in one process each through `box-lane` (not `--exclusive`, as the declaration reserves that for the NLP-46 tier). NLP-46 deviating cells for P and C1 (informational only) were not re-run alone.
9. **Base build for N1** ran in the existing detached R worktree `i19-arm-afb65b8040a5` (Release, `HVEN_FP_MODE=SAFER_FAST`, bench ON) with the head build's configuration; `scripts/replay.sh` is the TU-0 replay script copied verbatim. N1 compared the base binary built there with the head binary built in the i19-2a worktree.
10. **Stage-1 was run only on the declared stage-1 corpora**; G-arms have no NLP-46 or PILOT-62 rows (declared: stage 2 runs only H0, P and C1 when nothing passes). Stage 1b did not run (no K).
11. Clock note: `STATE.md` working timestamps are approximate; the evidence files carry the authoritative stamps.

## Files

`S1/stage1.txt` (the harness report), `S1/stage1-table.txt`, `S1/stage1-lost-tail-lists.md`, `S1/stage1-v3-floor-tables.md`, `S2/` (official STOP, informational read, final tables, pilot summary), `alone/`, `certs/`, `N1/`, `N2/`, `N2b/`, `N3/`, `M/`, `C/`, `B/`, `Sm/`, `CI/`, `arms/` (every arm and pilot log, start and end stamps), `scripts/`, `OWNER-REPLAY-REQUEST.md`.
