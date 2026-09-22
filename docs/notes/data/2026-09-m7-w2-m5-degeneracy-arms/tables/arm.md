# M7 W2 M5 -- the degeneracy arms, cell by cell and arm by arm

Written by `wrappers/readout_m5.py` at read-out time from the files named
in its header.  **No solver ran to produce this file.**  RULE.md was not
edited.  Wall-clock is INFORMATIONAL throughout (CLAUDE.md section 7);
counters are the asserted currency.

## 1. RULE.md section 7.1's compared fields, every cell x arm

`OFF` is M4's committed row for that cell, COPIED (never re-run) and
verified against `rows-off/SHA256-off-rows.txt` by the chain's STEP 0.

| cell | arm | status | OFF status | f | OFF f | stationarity | feas_e | feas_i | majors | minors | OFF minors | min ratio | facts | steps acc | restor entries / sub | deadline | wall s (info) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `ACOPP118` | `b1` | `numerical_error` | `numerical_error` | 129649 | 129649 | 25.1491 | 0.00654032 | 0 | 7 | 19872 | 19917 | 0.9977 | 20447 | 3 | 0 / `not_run` | 0 | 50.0 |
| `ACOPP118` | `a+b1` | `numerical_error` | `numerical_error` | 129649 | 129649 | 25.1491 | 0.00654032 | 0 | 7 | 19872 | 19917 | 0.9977 | 20447 | 3 | 0 / `not_run` | 0 | 49.0 |
| `ACOPP57` | `b1` | `numerical_error` | `numerical_error` | 41737.5 | 41737.5 | 0.63864 | 0.000113859 | 0 | 7 | 7242 | 7248 | 0.9992 | 108 | 3 | 0 / `not_run` | 0 | 2.0 |
| `ACOPP57` | `a+b1` | `numerical_error` | `numerical_error` | 41737.5 | 41737.5 | 0.63864 | 0.000113859 | 0 | 7 | 7242 | 7248 | 0.9992 | 108 | 3 | 0 / `not_run` | 0 | 2.0 |
| `ACOPR57` | `b1` | `numerical_error` | `numerical_error` | 41737.8 | 41737.8 | 18.0549 | 0.177855 | 0.0646719 | 13 | 11649 | 13874 | 0.8396 | 5813 | 7 | 0 / `not_run` | 0 | 9.0 |
| `ACOPR57` | `a+b1` | `numerical_error` | `numerical_error` | 41737.8 | 41737.8 | 18.0549 | 0.177855 | 0.0646719 | 13 | 11649 | 13874 | 0.8396 | 5813 | 7 | 0 / `not_run` | 0 | 9.0 |
| `BRITGAS` | `a+b1` | `numerical_error` | `numerical_error` | 0 | 0.0754943 | 0.22 | 2 | 0 | 4 | 55926 | 92309 | 0.6059 | 56440 | 0 | 1 / `numerical_error` | 0 | 410.0 |
| `DALLASM` | `b1` | `numerical_error` | `numerical_error` | 6.10494e+07 | 6.19749e+07 | 88488.4 | 1.52374e-06 | 1.24745e-06 | 5 | 8227 | 10583 | 0.7774 | 309 | 1 | 0 / `not_run` | 0 | 4.0 |
| `DALLASM` | `a+b1` | `numerical_error` | `numerical_error` | 6.10494e+07 | 6.19749e+07 | 88488.4 | 1.52374e-06 | 1.24745e-06 | 5 | 8227 | 10583 | 0.7774 | 309 | 1 | 0 / `not_run` | 0 | 5.0 |
| `SAWPATH` | `b1` | `interrupted` | `interrupted` | 1483.19 | 1483.19 | 10.8187 | 0 | 3.556 | 1 | 77037 | 77037 | 1.0000 | 83624 | 0 | 1 / `numerical_error` | 1 | 354.0 |
| `SAWPATH` | `a+b1` | `interrupted` | `interrupted` | 1483.19 | 1483.19 | 10.8187 | 0 | 3.556 | 1 | 77037 | 77037 | 1.0000 | 83624 | 0 | 1 / `numerical_error` | 1 | 353.0 |
| `STEENBRB` | `b1` | `numerical_error` | `numerical_error` | 4.16999e+12 | 4.09086e+12 | 4.15738e+13 | 1.13687e-12 | 6.48535e-13 | 5 | 25348 | 20637 | 1.2283 | 36940 | 0 | 1 / `optimal` | 0 | 460.0 |
| `STEENBRB` | `a+b1` | `numerical_error` | `numerical_error` | 4.16999e+12 | 4.09086e+12 | 4.15738e+13 | 1.13687e-12 | 6.48535e-13 | 5 | 25105 | 20637 | 1.2165 | 36681 | 0 | 1 / `optimal` | 0 | 459.0 |
| `STEENBRC` | `b1` | `numerical_error` | `numerical_error` | 4.46743e+13 | 1.83454e+14 | 1.43641e+14 | 3.18323e-12 | 2.31776e-18 | 6 | 33941 | 35324 | 0.9608 | 40587 | 0 | 1 / `optimal` | 0 | 455.0 |
| `STEENBRC` | `a+b1` | `interrupted` | `numerical_error` | 2.65101e+11 | 1.83454e+14 | 1.37797e+12 | 9486 | 6.75553e-14 | 1 | 7276 | 35324 | 0.2060 | 17938 | 0 | 1 / `interrupted` | 1 | 612.0 |
| `STEENBRG` | `b1` | `numerical_error` | `numerical_error` | 4.46743e+13 | 1.83454e+14 | 1.43641e+14 | 3.18323e-12 | 2.31776e-18 | 6 | 34407 | 35429 | 0.9712 | 41105 | 0 | 1 / `optimal` | 0 | 455.0 |
| `STEENBRG` | `a+b1` | `interrupted` | `numerical_error` | 2.65101e+11 | 1.83454e+14 | 1.37797e+12 | 9486 | 6.75553e-14 | 1 | 7818 | 35429 | 0.2207 | 18521 | 0 | 1 / `interrupted` | 1 | 608.0 |
| `YORKNET` | `a` | `numerical_error` | `infeasible` | 123344 | 110670 | 2258.1 | 158.723 | 0 | 5 | 42854 | 50241 | 0.8530 | 44014 | 3 | 1 / `numerical_error` | 0 | 66.0 |
| `YORKNET` | `a+b1` | `numerical_error` | `infeasible` | 110670 | 110670 | 1959.3 | 4099.28 | 131.08 | 1 | 13892 | 50241 | 0.2765 | 13977 | 0 | 1 / `numerical_error` | 0 | 47.0 |
| `READING6` | `a` | `optimal` | `optimal` | -144.66 | -144.66 | 1.26166e-12 | 2.79776e-13 | 0 | 13 | 3820 | 3833 | 0.9966 | 6709 | 9 | 0 / `not_run` | 0 | 25.0 |
| `READING6` | `b1` | `numerical_error` | `optimal` | 0 | -144.66 | 0 | 2.218 | 0 | 4 | 8 | 3833 | 0.0021 | 4 | 0 | 0 / `not_run` | 0 | 0.0 |
| `READING6` | `a+b1` | `numerical_error` | `optimal` | 0 | -144.66 | 0 | 2.218 | 0 | 4 | 8 | 3833 | 0.0021 | 4 | 0 | 0 / `not_run` | 0 | 0.0 |
| `SMBANK` | `a` | `optimal` | `optimal` | -7129292 | -7129292 | 9.2748e-10 | 9.45874e-11 | 0 | 29 | 334 | 334 | 1.0000 | 29 | 29 | 0 / `not_run` | 0 | 0.0 |
| `SMBANK` | `b1` | `optimal` | `optimal` | -7129292 | -7129292 | 9.2748e-10 | 9.45874e-11 | 0 | 29 | 334 | 334 | 1.0000 | 29 | 29 | 0 / `not_run` | 0 | 0.0 |
| `SMBANK` | `a+b1` | `optimal` | `optimal` | -7129292 | -7129292 | 9.2748e-10 | 9.45874e-11 | 0 | 29 | 334 | 334 | 1.0000 | 29 | 29 | 0 / `not_run` | 0 | 0.0 |
| `ZAMB2-8` | `a` | `optimal` | `optimal` | -0.152936 | -0.152936 | 6.22126e-12 | 7.22928e-10 | 0 | 17 | 7686 | 6666 | 1.1530 | 6540 | 13 | 1 / `optimal` | 0 | 41.0 |
| `ZAMB2-8` | `b1` | `numerical_error` | `optimal` | 0.0180685 | -0.152936 | 0.0109791 | 2.33846 | 2.839 | 5 | 3080 | 6666 | 0.4620 | 3144 | 0 | 1 / `numerical_error` | 0 | 6.0 |
| `ZAMB2-8` | `a+b1` | `numerical_error` | `optimal` | 0.0180685 | -0.152936 | 0.0109791 | 2.33846 | 2.839 | 5 | 2963 | 6666 | 0.4445 | 2910 | 0 | 1 / `numerical_error` | 0 | 4.0 |

`opt_feas_tol` IS NOT PRINTED BY THIS PROBE.  It is one of the fields M4's
own OFF-control gate lists as absent from the row, and M4b recorded the
same absence.  It is not substituted from M3c's row and not typed in from
the rule.  Where this artifact calls a cell INFEASIBLE-EXIT the printed
`feasibility_e` exceeds 1e-6 by more than six orders of magnitude, so the
call does not turn on the exact tolerance; no borderline cell is called.

## 2. Arm A's read-out (RULE.md section 4)

| cell | arm | drops made | candidates refused | **drops prevented** | max refused ratio | scale unavailable passes |
|---|---|---|---|---|---|---|
| `ACOPP118` | `a+b1` | 9582 | 0 | **0** | 0 | 0 |
| `ACOPP57` | `a+b1` | 3580 | 0 | **0** | 0 | 0 |
| `ACOPR57` | `a+b1` | 4427 | 0 | **0** | 0 | 0 |
| `BRITGAS` | `a+b1` | 26001 | 4403 | **0** | 9.90044e-10 | 0 |
| `DALLASM` | `a+b1` | 3904 | 0 | **0** | 0 | 0 |
| `SAWPATH` | `a+b1` | 35636 | 3883096 | **0** | 9.9995e-10 | 0 |
| `STEENBRB` | `a+b1` | 11458 | 40139 | **1** | 7.91108e-10 | 0 |
| `STEENBRC` | `a+b1` | 2863 | 6991 | **1** | 1.96905e-10 | 0 |
| `STEENBRG` | `a+b1` | 3132 | 8685 | **1** | 2.21566e-10 | 0 |
| `YORKNET` | `a` | 19218 | 55042 | **7** | 9.5507e-10 | 0 |
| `YORKNET` | `a+b1` | 6386 | 508 | **1** | 8.48287e-10 | 0 |
| `READING6` | `a` | 1362 | 45 | **1** | 8.087e-10 | 0 |
| `READING6` | `a+b1` | 0 | 0 | **0** | 0 | 0 |
| `SMBANK` | `a` | 18 | 0 | **0** | 0 | 0 |
| `SMBANK` | `a+b1` | 18 | 0 | **0** | 0 | 0 |
| `ZAMB2-8` | `a` | 3071 | 1037 | **4** | 9.34316e-10 | 0 |
| `ZAMB2-8` | `a+b1` | 1225 | 1299 | **4** | 8.8852e-10 | 0 |

`scale_unavailable_passes` is **0 on every leg**, so no arm-A read-out is
invalidated by RULE.md section 4's own condition.  `b1`-only legs are
omitted from this table: arm A is off there and all five read 0.

## 3. Arm b1's counters, per cell x arm (RULE.md sections 5 and 6)

| cell | arm | passes | cands examined | passed over | dep tests | QR builds | builds/pass | test failures | blocker switched | no-blocker passes | budget truncations | **enum mismatches** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `ACOPP118` | `b1` | 21 | 21 | 0 | 357 | 339 | 16.14 | 0 | 0 | 0 | 0 | **0** |
| `ACOPP118` | `a+b1` | 21 | 21 | 0 | 357 | 339 | 16.14 | 0 | 0 | 0 | 0 | **0** |
| `ACOPP57` | `b1` | 2735 | 3143 | 410 | 3194 | 126 | 0.05 | 0 | 6 | 1 | 0 | **0** |
| `ACOPP57` | `a+b1` | 2735 | 3143 | 410 | 3194 | 126 | 0.05 | 0 | 6 | 1 | 0 | **0** |
| `ACOPR57` | `b1` | 23 | 292 | 272 | 368 | 196 | 8.52 | 0 | 1 | 1 | 0 | **0** |
| `ACOPR57` | `a+b1` | 23 | 292 | 272 | 368 | 196 | 8.52 | 0 | 1 | 1 | 0 | **0** |
| `BRITGAS` | `a+b1` | 16265 | 447991 | 431763 | 462591 | 1936 | 0.12 | 0 | 9272 | 32 | 0 | **0** |
| `DALLASM` | `b1` | 120 | 2958 | 2866 | 2958 | 133 | 1.11 | 0 | 70 | 28 | 0 | **0** |
| `DALLASM` | `a+b1` | 120 | 2958 | 2866 | 2958 | 133 | 1.11 | 0 | 70 | 28 | 0 | **0** |
| `SAWPATH` | `b1` | 0 | 0 | 0 | 0 | 22 | - | 0 | 0 | 0 | 0 | **0** |
| `SAWPATH` | `a+b1` | 0 | 0 | 0 | 0 | 22 | - | 0 | 0 | 0 | 0 | **0** |
| `STEENBRB` | `b1` | 3262 | 5375 | 2113 | 19416 | 12577 | 3.86 | 0 | 191 | 0 | 0 | **0** |
| `STEENBRB` | `a+b1` | 3294 | 5026 | 1732 | 19022 | 12610 | 3.83 | 0 | 227 | 0 | 0 | **0** |
| `STEENBRC` | `b1` | 3617 | 6348 | 2731 | 16025 | 8158 | 2.26 | 0 | 219 | 0 | 0 | **0** |
| `STEENBRC` | `a+b1` | 3720 | 7179 | 3459 | 20742 | 12130 | 3.26 | 0 | 256 | 0 | 0 | **0** |
| `STEENBRG` | `b1` | 3555 | 6557 | 3002 | 16367 | 8104 | 2.28 | 0 | 2891 | 0 | 0 | **0** |
| `STEENBRG` | `a+b1` | 3644 | 3809 | 166 | 17509 | 12072 | 3.31 | 0 | 48 | 0 | 0 | **0** |
| `YORKNET` | `a+b1` | 6882 | 22494 | 15659 | 26039 | 1188 | 0.17 | 0 | 3672 | 46 | 0 | **0** |
| `READING6` | `b1` | 4 | 200 | 200 | 604 | 212 | 53.00 | 0 | 4 | 4 | 0 | **0** |
| `READING6` | `a+b1` | 4 | 200 | 200 | 604 | 212 | 53.00 | 0 | 4 | 4 | 0 | **0** |
| `SMBANK` | `b1` | 0 | 0 | 0 | 0 | 58 | - | 0 | 0 | 0 | 0 | **0** |
| `SMBANK` | `a+b1` | 0 | 0 | 0 | 0 | 58 | - | 0 | 0 | 0 | 0 | **0** |
| `ZAMB2-8` | `b1` | 1769 | 7117 | 5456 | 8191 | 736 | 0.42 | 0 | 292 | 108 | 0 | **0** |
| `ZAMB2-8` | `a+b1` | 1713 | 6738 | 5111 | 7812 | 680 | 0.40 | 0 | 260 | 86 | 0 | **0** |

**`indep_enumeration_mismatches` is 0 on every leg.**  RULE.md section 7
deviation 4's invalidation never fires: the arm's re-enumeration of the
ratio test's candidates agreed with production's own blocker on every pass
it took.

## 4. A8's three-mechanism split (RULE.md section 3.1)

| cell | arm | 1. entry removals | 2. screen-and-QR pass-overs | 3. QR-only pass-overs |
|---|---|---|---|---|
| `ACOPP118` | `b1` | 0 | 0 | 30 |
| `ACOPP118` | `a+b1` | 0 | 0 | 30 |
| `ACOPP57` | `b1` | 0 | 180 | 234 |
| `ACOPP57` | `a+b1` | 0 | 180 | 234 |
| `ACOPR57` | `b1` | 219 | 272 | 50 |
| `ACOPR57` | `a+b1` | 219 | 272 | 50 |
| `BRITGAS` | `a+b1` | 0 | 8984 | 437266 |
| `DALLASM` | `b1` | 0 | 2658 | 208 |
| `DALLASM` | `a+b1` | 0 | 2658 | 208 |
| `SAWPATH` | `b1` | 0 | 0 | 0 |
| `SAWPATH` | `a+b1` | 0 | 0 | 0 |
| `STEENBRB` | `b1` | 0 | 0 | 4481 |
| `STEENBRB` | `a+b1` | 0 | 0 | 4073 |
| `STEENBRC` | `b1` | 1 | 0 | 5462 |
| `STEENBRC` | `a+b1` | 1 | 0 | 6165 |
| `STEENBRG` | `b1` | 5 | 0 | 5853 |
| `STEENBRG` | `a+b1` | 5 | 0 | 3008 |
| `YORKNET` | `a+b1` | 12 | 1379 | 17782 |
| `READING6` | `b1` | 0 | 0 | 400 |
| `READING6` | `a+b1` | 0 | 0 | 400 |
| `SMBANK` | `b1` | 0 | 0 | 0 |
| `SMBANK` | `a+b1` | 0 | 0 | 0 |
| `ZAMB2-8` | `b1` | 0 | 3128 | 3387 |
| `ZAMB2-8` | `a+b1` | 0 | 2877 | 3293 |

**READ THE SPLIT AGAINST THE SITES, NOT AGAINST THE PASS-OVERS.**
Mechanisms 2 and 3 count every DEPENDENT VERDICT the shared predicate
returned, at whichever site asked; `indep_candidates_passed_over` counts
only the ordinary add's.  Mechanism 1 has its own counter and does not go
through the predicate.  The identity that follows is CHECKED here on
every armed leg rather than asserted:

    screen_and_qr + qr_only  ==  passed_over + ride_refusals
                                 + shift_refusals + repair_pin_refusals
                                 + probe_restore_dependent

It holds on **24 of 24** armed legs.

So `READING6`'s 400 QR-only verdicts are 200 ordinary pass-overs plus 200
repair-pin refusals, and `STEENBRC`'s 5462 under b1 are 2731 pass-overs
plus 2731 ride refusals -- two independent counters that happen to be
equal on that cell, not one counter printed twice.

## 5. The rank invariant, and the three counters that make a cell UNSUPPORTED

`indep_disarmed` / `indep_disarm_site` are **per solve** -- `begin_solve()`
re-arms and clears them -- so they report the LAST solve's state, not the
cell's.  The cumulative counters beside them are what a verdict reads.
Sites: 0 ordinary add, 1 ride add, 2 seed entry, 3 shift add, 4 probe
restore (check-only), 5 repair pin.

| cell | arm | solves | max(me+\|W\|-n) | max rank deficit | **rank violations (A9)** | **mandatory-dep refusals (A3)** | refused / solves | last-solve disarm site |
|---|---|---|---|---|---|---|---|---|
| `ACOPP118` | `b1` | 7 | -1 | 1 | **5** | **0** | 0.00 | 5 |
| `ACOPP118` | `a+b1` | 7 | -1 | 1 | **5** | **0** | 0.00 | 5 |
| `ACOPP57` | `b1` | 7 | 0 | 1 | **1** | **0** | 0.00 | (not disarmed at harvest) |
| `ACOPP57` | `a+b1` | 7 | 0 | 1 | **1** | **0** | 0.00 | (not disarmed at harvest) |
| `ACOPR57` | `b1` | 15 | 49 | 50 | **14** | **0** | 0.00 | 5 |
| `ACOPR57` | `a+b1` | 15 | 49 | 50 | **14** | **0** | 0.00 | 5 |
| `BRITGAS` | `a+b1` | 8 | 0 | 1 | **5** | **0** | 0.00 | (not disarmed at harvest) |
| `DALLASM` | `b1` | 8 | 0 | 2 | **0** | **5** | 0.62 | 2 |
| `DALLASM` | `a+b1` | 8 | 0 | 2 | **0** | **5** | 0.62 | 2 |
| `SAWPATH` | `b1` | 11 | -1 | 878 | **0** | **11** | 1.00 | 2 |
| `SAWPATH` | `a+b1` | 11 | -1 | 878 | **0** | **11** | 1.00 | 2 |
| `STEENBRB` | `b1` | 23 | -11 | 45 | **0** | **5** | 0.22 | 2 |
| `STEENBRB` | `a+b1` | 23 | -20 | 45 | **0** | **5** | 0.22 | 2 |
| `STEENBRC` | `b1` | 22 | -9 | 37 | **0** | **6** | 0.27 | 2 |
| `STEENBRC` | `a+b1` | 15 | -8 | 14 | **0** | **1** | 0.07 | (not disarmed at harvest) |
| `STEENBRG` | `b1` | 23 | -31 | 37 | **1** | **6** | 0.26 | 2 |
| `STEENBRG` | `a+b1` | 16 | -31 | 14 | **1** | **1** | 0.06 | (not disarmed at harvest) |
| `YORKNET` | `a+b1` | 4 | 12 | 12 | **1** | **0** | 0.00 | (not disarmed at harvest) |
| `READING6` | `b1` | 4 | -1 | 0 | **0** | **0** | 0.00 | (not disarmed at harvest) |
| `READING6` | `a+b1` | 4 | -1 | 0 | **0** | **0** | 0.00 | (not disarmed at harvest) |
| `SMBANK` | `b1` | 29 | -37 | 1 | **0** | **29** | 1.00 | 2 |
| `SMBANK` | `a+b1` | 29 | -37 | 1 | **0** | **29** | 1.00 | 2 |
| `ZAMB2-8` | `b1` | 10 | 0 | 0 | **0** | **0** | 0.00 | (not disarmed at harvest) |
| `ZAMB2-8` | `a+b1` | 10 | 0 | 0 | **0** | **0** | 0.00 | (not disarmed at harvest) |

## 6. AMENDMENT 1's exceedances-against-the-QR-pivot-spread-proxy column (C4)

A READING, never a verdict.  Every row with `indep_leak_bound_exceeded > 0`
keeps UNSUPPORTED whatever `indep_max_cond_r` says.  `nan` means NO
DECOMPOSITION WAS TAKEN -- it is printed as absent and never floored to 1.

| cell | arm | **A10 exceedances** | max passed-over violation | `indep_max_cond_r` (per solve high-water) |
|---|---|---|---|---|
| `ACOPP118` | `b1` | **0** | 0 | 7.46397e+07 |
| `ACOPP118` | `a+b1` | **0** | 0 | 7.46397e+07 |
| `ACOPP57` | `b1` | **1** | 0.310227 | 9.89067e+07 |
| `ACOPP57` | `a+b1` | **1** | 0.310227 | 9.89067e+07 |
| `ACOPR57` | `b1` | **1** | 2.52197 | 4.7595e+07 |
| `ACOPR57` | `a+b1` | **1** | 2.52197 | 4.7595e+07 |
| `BRITGAS` | `a+b1` | **426962** | 107302 | 5.02403e+07 |
| `DALLASM` | `b1` | **46** | 0.56 | 16.0624 |
| `DALLASM` | `a+b1` | **46** | 0.56 | 16.0624 |
| `SAWPATH` | `b1` | **0** | 0 | 4.15308e+07 |
| `SAWPATH` | `a+b1` | **0** | 0 | 4.15308e+07 |
| `STEENBRB` | `b1` | **1137** | 1998.9 | 8.00001e+06 |
| `STEENBRB` | `a+b1` | **727** | 1998.9 | 6.92821e+06 |
| `STEENBRC` | `b1` | **1477** | 1999.4 | 4e+07 |
| `STEENBRC` | `a+b1` | **1406** | 1998.9 | 3.4641e+07 |
| `STEENBRG` | `b1` | **2839** | 3.78009e-12 | 1.51186e+07 |
| `STEENBRG` | `a+b1` | **1** | 2.04636e-12 | 1.48861e+07 |
| `YORKNET` | `a+b1` | **12847** | 126243 | 6.74374e+07 |
| `READING6` | `b1` | **188** | 6.23767 | 2.94219 |
| `READING6` | `a+b1` | **188** | 6.23767 | 2.94219 |
| `SMBANK` | `b1` | **0** | 0 | 5.28493 |
| `SMBANK` | `a+b1` | **0** | 0 | 5.28493 |
| `ZAMB2-8` | `b1` | **3617** | 4.40862e+09 | 69223.2 |
| `ZAMB2-8` | `a+b1` | **3429** | 4.40862e+09 | 69223.2 |

## 7. Which SITE acted (RULE.md section 5's site table)

| cell | arm | ride refusals (site 1) | shift refusals (site 3) | probe-restore dependent (site 4, check-only) | repair-pin refusals (site 5) | OFF shift_adds | arm shift_adds |
|---|---|---|---|---|---|---|---|
| `ACOPP118` | `b1` | 0 | 0 | 0 | 30 | 0 | 0 |
| `ACOPP118` | `a+b1` | 0 | 0 | 0 | 30 | 0 | 0 |
| `ACOPP57` | `b1` | 0 | 0 | 0 | 4 | 0 | 0 |
| `ACOPP57` | `a+b1` | 0 | 0 | 0 | 4 | 0 | 0 |
| `ACOPR57` | `b1` | 0 | 50 | 0 | 0 | 1 | 1 |
| `ACOPR57` | `a+b1` | 0 | 50 | 0 | 0 | 1 | 1 |
| `BRITGAS` | `a+b1` | 14487 | 0 | 0 | 0 | 0 | 0 |
| `DALLASM` | `b1` | 0 | 0 | 0 | 0 | 0 | 0 |
| `DALLASM` | `a+b1` | 0 | 0 | 0 | 0 | 0 | 0 |
| `SAWPATH` | `b1` | 0 | 0 | 0 | 0 | 0 | 0 |
| `SAWPATH` | `a+b1` | 0 | 0 | 0 | 0 | 0 | 0 |
| `STEENBRB` | `b1` | 2356 | 0 | 0 | 12 | 0 | 0 |
| `STEENBRB` | `a+b1` | 2337 | 0 | 0 | 4 | 0 | 0 |
| `STEENBRC` | `b1` | 2731 | 0 | 0 | 0 | 0 | 0 |
| `STEENBRC` | `a+b1` | 2704 | 0 | 0 | 2 | 0 | 0 |
| `STEENBRG` | `b1` | 2839 | 0 | 0 | 12 | 0 | 0 |
| `STEENBRG` | `a+b1` | 2839 | 0 | 0 | 3 | 0 | 0 |
| `YORKNET` | `a+b1` | 3502 | 0 | 0 | 0 | 0 | 0 |
| `READING6` | `b1` | 0 | 0 | 0 | 200 | 0 | 0 |
| `READING6` | `a+b1` | 0 | 0 | 0 | 200 | 0 | 0 |
| `SMBANK` | `b1` | 0 | 0 | 0 | 0 | 0 | 0 |
| `SMBANK` | `a+b1` | 0 | 0 | 0 | 0 | 0 | 0 |
| `ZAMB2-8` | `b1` | 1059 | 0 | 0 | 0 | 0 | 0 |
| `ZAMB2-8` | `a+b1` | 1059 | 0 | 0 | 0 | 0 | 0 |

`counters.shift_adds` is not a field of the SQP-level row; the ledger's
FIRST-QP `first_shift_adds` is what both rows print, and it is what the
last two columns carry.

## 8. The core drop -> same-bound-re-add unit count (`wrappers/m4c_tails.py`, RUN)

The OFF column for the TEN is **M4c's committed `tables/tails.json`**, read
and not recomputed.  The OFF column for the three neutrality cells is this
round's own run of the SAME script over M4's pinned OFF traces (M4c never
ran it on those three).  As a check the script was also re-run on M4's
`off-YORKNET.csv` and reproduced M4c's committed 2538 / 3994 exactly.

| cell | arm | recurrent solves | tail units | **core units** | OFF recurrent solves | OFF tail units | **OFF core units** | core ratio |
|---|---|---|---|---|---|---|---|---|
| `ACOPP118` | `b1` | 4 | 9212 | **9212** | 4 | 9212 | **9212** | 1.000 |
| `ACOPP118` | `a+b1` | 4 | 9212 | **9212** | 4 | 9212 | **9212** | 1.000 |
| `ACOPP57` | `b1` | 4 | 3552 | **3552** | 4 | 3540 | **3540** | 1.003 |
| `ACOPP57` | `a+b1` | 4 | 3552 | **3552** | 4 | 3540 | **3540** | 1.003 |
| `ACOPR57` | `b1` | 5 | 3749 | **3205** | 6 | 4975 | **4225** | 0.759 |
| `ACOPR57` | `a+b1` | 5 | 3749 | **3205** | 6 | 4975 | **4225** | 0.759 |
| `BRITGAS` | `a+b1` | 7 | 25294 | **18732** | 10 | 39524 | **23868** | 0.785 |
| `DALLASM` | `b1` | 4 | 3788 | **3788** | 5 | 4703 | **3778** | 1.003 |
| `DALLASM` | `a+b1` | 4 | 3788 | **3788** | 5 | 4703 | **3778** | 1.003 |
| `SAWPATH` | `b1` | 5 | 32878 | **32878** | 5 | 32878 | **32878** | 1.000 |
| `SAWPATH` | `a+b1` | 5 | 32878 | **32878** | 5 | 32878 | **32878** | 1.000 |
| `STEENBRB` | `b1` | 5 | 11265 | **6886** | 4 | 8935 | **4520** | 1.523 |
| `STEENBRB` | `a+b1` | 5 | 11247 | **6868** | 4 | 8935 | **4520** | 1.519 |
| `STEENBRC` | `b1` | 6 | 15346 | **10417** | 6 | 15130 | **10276** | 1.014 |
| `STEENBRC` | `a+b1` | 1 | 2702 | **2702** | 6 | 15130 | **10276** | 0.263 |
| `STEENBRG` | `b1` | 6 | 15451 | **10542** | 6 | 15341 | **5216** | 2.021 |
| `STEENBRG` | `a+b1` | 1 | 2839 | **2839** | 6 | 15341 | **5216** | 0.544 |
| `YORKNET` | `a` | 6 | 17685 | **17685** | 2 | 6532 | **2538** | 6.968 |
| `YORKNET` | `a+b1` | 2 | 5996 | **5996** | 2 | 6532 | **2538** | 2.362 |
| `READING6` | `a` | 3 | 902 | **687** | 3 | 902 | **687** | 1.000 |
| `READING6` | `b1` | 0 | 0 | **0** | 3 | 902 | **687** | 0.000 |
| `READING6` | `a+b1` | 0 | 0 | **0** | 3 | 902 | **687** | 0.000 |
| `SMBANK` | `a` | 0 | 0 | **0** | 0 | 0 | **0** | 0/0 |
| `SMBANK` | `b1` | 0 | 0 | **0** | 0 | 0 | **0** | 0/0 |
| `SMBANK` | `a+b1` | 0 | 0 | **0** | 0 | 0 | **0** | 0/0 |
| `ZAMB2-8` | `a` | 4 | 2502 | **1446** | 3 | 2221 | **1544** | 0.937 |
| `ZAMB2-8` | `b1` | 1 | 1058 | **1058** | 3 | 2221 | **1544** | 0.685 |
| `ZAMB2-8` | `a+b1` | 1 | 1058 | **1058** | 3 | 2221 | **1544** | 0.685 |

The representative released identity of every recurrent tail, per leg:

| leg | tails (solve : representative released identity) | classes |
|---|---|---|
| `off-ACOPP118` | `s3:bound:22:upper`, `s4:bound:22:upper`, `s5:bound:22:upper`, `s6:bound:22:upper` | {'b': 4} |
| `b1-ACOPP118` | `s3:bound:22:upper`, `s4:bound:22:upper`, `s5:bound:22:upper`, `s6:bound:22:upper` | {'b': 4} |
| `a+b1-ACOPP118` | `s3:bound:22:upper`, `s4:bound:22:upper`, `s5:bound:22:upper`, `s6:bound:22:upper` | {'b': 4} |
| `off-ACOPP57` | `s3:bound:8:lower`, `s4:bound:12:lower`, `s5:bound:14:lower`, `s6:bound:14:lower` | {'e': 2, 'b': 2} |
| `b1-ACOPP57` | `s3:bound:6:lower`, `s4:bound:12:lower`, `s5:bound:6:lower`, `s6:bound:8:lower` | {'e': 4} |
| `a+b1-ACOPP57` | `s3:bound:6:lower`, `s4:bound:12:lower`, `s5:bound:6:lower`, `s6:bound:8:lower` | {'e': 4} |
| `off-ACOPR57` | `s4:bound:114:lower`, `s11:bound:14:lower`, `s13:bound:106:lower`, `s14:bound:14:lower`, `s15:bound:14:lower`, `s16:bound:123:lower` | {'b': 6} |
| `b1-ACOPR57` | `s4:bound:114:lower`, `s11:bound:14:lower`, `s12:bound:52:lower`, `s13:ineq:216:free`, `s14:bound:90:upper` | {'b': 5} |
| `a+b1-ACOPR57` | `s4:bound:114:lower`, `s11:bound:14:lower`, `s12:bound:52:lower`, `s13:ineq:216:free`, `s14:bound:90:upper` | {'b': 5} |
| `off-BRITGAS` | `s0:bound:403:lower`, `s1:bound:309:lower`, `s2:bound:382:lower`, `s4:bound:99:upper`, `s5:bound:360:lower`, `s6:bound:307:lower`, `s8:bound:688:lower`, `s9:bound:441:upper`, `s10:bound:863:lower`, `s11:bound:245:lower` | {'e': 6, 'c': 4} |
| `a+b1-BRITGAS` | `s0:bound:305:lower`, `s1:bound:290:lower`, `s2:bound:415:lower`, `s4:bound:205:upper`, `s5:bound:448:lower`, `s6:bound:345:lower`, `s7:bound:106:lower` | {'e': 4, 'd': 1, 'b': 1, 'c': 1} |
| `off-DALLASM` | `s1:bound:0:upper`, `s4:bound:21:upper`, `s5:bound:25:lower`, `s6:bound:21:upper`, `s7:bound:21:upper` | {'b': 5} |
| `b1-DALLASM` | `s4:bound:21:upper`, `s5:bound:21:upper`, `s6:bound:21:upper`, `s7:bound:21:upper` | {'b': 4} |
| `a+b1-DALLASM` | `s4:bound:21:upper`, `s5:bound:21:upper`, `s6:bound:21:upper`, `s7:bound:21:upper` | {'b': 4} |
| `off-SAWPATH` | `s6:bound:499:upper`, `s7:bound:980:lower`, `s8:bound:793:lower`, `s9:bound:1231:lower`, `s10:bound:1237:lower` | {'e': 1, 'b': 4} |
| `b1-SAWPATH` | `s6:bound:499:upper`, `s7:bound:980:lower`, `s8:bound:793:lower`, `s9:bound:1231:lower`, `s10:bound:1237:lower` | {'e': 1, 'b': 4} |
| `a+b1-SAWPATH` | `s6:bound:499:upper`, `s7:bound:980:lower`, `s8:bound:793:lower`, `s9:bound:1231:lower`, `s10:bound:1237:lower` | {'e': 1, 'b': 4} |
| `off-STEENBRB` | `s13:bound:84:upper`, `s14:bound:295:lower`, `s15:bound:269:upper`, `s16:bound:232:upper` | {'d': 1, 'b': 3} |
| `b1-STEENBRB` | `s1:bound:377:lower`, `s19:bound:136:upper`, `s20:bound:124:lower`, `s21:bound:216:upper`, `s22:bound:421:lower` | {'c': 1, 'd': 1, 'b': 3} |
| `a+b1-STEENBRB` | `s1:bound:69:lower`, `s19:bound:136:upper`, `s20:bound:124:lower`, `s21:bound:216:upper`, `s22:bound:421:lower` | {'c': 1, 'd': 1, 'b': 3} |
| `off-STEENBRC` | `s0:bound:514:upper`, `s7:bound:244:lower`, `s24:bound:393:upper`, `s25:bound:339:lower`, `s26:bound:163:lower`, `s27:bound:487:lower` | {'d': 1, 'b': 5} |
| `b1-STEENBRC` | `s0:bound:514:upper`, `s2:bound:23:upper`, `s18:bound:315:upper`, `s19:bound:201:upper`, `s20:bound:373:upper`, `s21:bound:147:lower` | {'d': 1, 'c': 1, 'b': 4} |
| `a+b1-STEENBRC` | `s1:bound:47:upper` | {'c': 1} |
| `off-STEENBRG` | `s0:bound:113:upper`, `s7:bound:322:lower`, `s24:bound:94:upper`, `s25:bound:339:lower`, `s26:bound:146:lower`, `s27:bound:146:lower` | {'d': 4, 'b': 2} |
| `b1-STEENBRG` | `s0:bound:113:upper`, `s3:bound:96:lower`, `s19:bound:315:upper`, `s20:bound:67:lower`, `s21:bound:186:upper`, `s22:bound:357:lower` | {'d': 2, 'c': 1, 'b': 3} |
| `a+b1-STEENBRG` | `s2:bound:29:lower` | {'c': 1} |
| `off-YORKNET` | `s7:bound:393:lower`, `s145:bound:1:lower` | {'a': 1, 'b': 1} |
| `a-YORKNET` | `s8:bound:93:lower`, `s26:bound:18:upper`, `s27:bound:212:upper`, `s28:bound:479:lower`, `s29:bound:236:upper`, `s30:bound:249:lower` | {'e': 2, 'b': 3, 'd': 1} |
| `a+b1-YORKNET` | `s2:bound:16:lower`, `s3:bound:683:upper` | {'b': 1, 'c': 1} |
| `off-READING6` | `s8:bound:47:lower`, `s9:bound:46:lower`, `s35:bound:44:upper` | {'b': 3} |
| `a-READING6` | `s8:bound:47:lower`, `s9:bound:46:lower`, `s35:bound:44:upper` | {'b': 3} |
| `b1-READING6` | (none) | {} |
| `a+b1-READING6` | (none) | {} |
| `off-SMBANK` | (none) | {} |
| `a-SMBANK` | (none) | {} |
| `b1-SMBANK` | (none) | {} |
| `a+b1-SMBANK` | (none) | {} |
| `off-ZAMB2-8` | `s0:bound:98:lower`, `s7:bound:62:lower`, `s27:bound:26:upper` | {'b': 1, 'a': 1, 'c': 1} |
| `a-ZAMB2-8` | `s0:bound:98:lower`, `s14:bound:6:lower`, `s28:bound:26:upper`, `s33:bound:57:lower` | {'b': 2, 'c': 2} |
| `b1-ZAMB2-8` | `s9:bound:95:upper` | {'e': 1} |
| `a+b1-ZAMB2-8` | `s9:bound:95:upper` | {'e': 1} |

## 9. Neutrality, DESCRIPTIVE AND EXTENDED (RULE.md section 8 step 4, A7)

`f` is printed EXACTLY as the row carries it, not rounded: M3c's
consensus rule agrees to 6 significant digits, and a difference below
that is a difference a rounded column would hide.

| cell | arm | status | f | feasibility_e | feasibility_i | stationarity | minors | minors ratio vs OFF | trace sha == OFF's? |
|---|---|---|---|---|---|---|---|---|---|
| `READING6` | `off` | `optimal` | -144.65970057279054 | 2.7977620220553945e-13 | 0 | 1.2616574451840279e-12 | 3833 | 1.0000 | (the control) |
| `READING6` | `a` | `optimal` | -144.65970057279054 | 2.7977620220553945e-13 | 0 | 1.2616574451840279e-12 | 3820 | 0.9966 | no |
| `READING6` | `b1` | `numerical_error` | 0 | 2.218 | 0 | 0 | 8 | 0.0021 | no |
| `READING6` | `a+b1` | `numerical_error` | 0 | 2.218 | 0 | 0 | 8 | 0.0021 | no |
| `SMBANK` | `off` | `optimal` | -7129292 | 9.4587448984384537e-11 | 0 | 9.2748031187081952e-10 | 334 | 1.0000 | (the control) |
| `SMBANK` | `a` | `optimal` | -7129292 | 9.4587448984384537e-11 | 0 | 9.2748031187081952e-10 | 334 | 1.0000 | YES |
| `SMBANK` | `b1` | `optimal` | -7129292 | 9.4587448984384537e-11 | 0 | 9.2748031187081952e-10 | 334 | 1.0000 | YES |
| `SMBANK` | `a+b1` | `optimal` | -7129292 | 9.4587448984384537e-11 | 0 | 9.2748031187081952e-10 | 334 | 1.0000 | YES |
| `ZAMB2-8` | `off` | `optimal` | -0.15293592733713887 | 1.7950063938010352e-07 | 0 | 1.0198462227847685e-10 | 6666 | 1.0000 | (the control) |
| `ZAMB2-8` | `a` | `optimal` | -0.15293592838839792 | 7.2292827280051597e-10 | 0 | 6.2212644636057933e-12 | 7686 | 1.1530 | no |
| `ZAMB2-8` | `b1` | `numerical_error` | 0.018068535257036302 | 2.3384617034999997 | 2.839 | 0.010979121771749999 | 3080 | 0.4620 | no |
| `ZAMB2-8` | `a+b1` | `numerical_error` | 0.018068535257036302 | 2.3384617034999997 | 2.839 | 0.010979121771749999 | 2963 | 0.4445 | no |

Read against M3c's consensus bar (agreement to 6 significant digits):
under `a`, READING6's and SMBANK's `f` are IDENTICAL to the control's
digit for digit, and ZAMB2-8's agrees to 6 significant digits while
differing at the 9th (`-0.15293592838839792` against
`-0.15293592733713887`), on a leg that took 1.1530x the control's minors.
Under `b1` and `a+b1` two of the three cells do not reach an objective to
compare.

## 10. A11's COST, DECLARED -- what the per-pass QR bought and spent

RULE.md section 3 declares the cost in advance: one dense rank-revealing
QR of an `n x (me+|W|)` matrix per pass, `O(n (me+|W|)^2)`, plus one
projection per candidate examined and one least-squares solve per
candidate called dependent.  **Wall-clock is INFORMATIONAL** (CLAUDE.md
section 7): the legs ran one at a time, pinned, under one lock hold, but
no claim in this artifact rests on a timing and none is quoted as a
measurement.  The column is here because a DEADLINE is what censored four
legs, and a reader is owed the arithmetic behind that censoring.

| cell | arm | n | me | QR builds | dep tests | minors | minors vs OFF | deadline s | wall s (info) | deadline fired |
|---|---|---|---|---|---|---|---|---|---|---|
| `ACOPP118` | `b1` | 344 | 236 | 339 | 357 | 19872 | 0.9977 | 600 | 50.0 | 0 |
| `ACOPP118` | `a+b1` | 344 | 236 | 339 | 357 | 19872 | 0.9977 | 600 | 49.0 | 0 |
| `ACOPP57` | `b1` | 128 | 114 | 126 | 3194 | 7242 | 0.9992 | 600 | 2.0 | 0 |
| `ACOPP57` | `a+b1` | 128 | 114 | 126 | 3194 | 7242 | 0.9992 | 600 | 2.0 | 0 |
| `ACOPR57` | `b1` | 128 | 114 | 196 | 368 | 11649 | 0.8396 | 600 | 9.0 | 0 |
| `ACOPR57` | `a+b1` | 128 | 114 | 196 | 368 | 11649 | 0.8396 | 600 | 9.0 | 0 |
| `BRITGAS` | `a+b1` | 450 | 360 | 1936 | 462591 | 55926 | 0.6059 | 600 | 410.0 | 0 |
| `DALLASM` | `b1` | 196 | 151 | 133 | 2958 | 8227 | 0.7774 | 600 | 4.0 | 0 |
| `DALLASM` | `a+b1` | 196 | 151 | 133 | 2958 | 8227 | 0.7774 | 600 | 5.0 | 0 |
| `SAWPATH` | `b1` | 583 | 578 | 22 | 0 | 77037 | 1.0000 | 300 | 354.0 | 1 |
| `SAWPATH` | `a+b1` | 583 | 578 | 22 | 0 | 77037 | 1.0000 | 300 | 353.0 | 1 |
| `STEENBRB` | `b1` | 468 | 108 | 12577 | 19416 | 25348 | 1.2283 | 600 | 460.0 | 0 |
| `STEENBRB` | `a+b1` | 468 | 108 | 12610 | 19022 | 25105 | 1.2165 | 600 | 459.0 | 0 |
| `STEENBRC` | `b1` | 540 | 126 | 8158 | 16025 | 33941 | 0.9608 | 600 | 455.0 | 0 |
| `STEENBRC` | `a+b1` | 540 | 126 | 12130 | 20742 | 7276 | 0.2060 | 600 | 612.0 | 1 |
| `STEENBRG` | `b1` | 540 | 126 | 8104 | 16367 | 34407 | 0.9712 | 600 | 455.0 | 0 |
| `STEENBRG` | `a+b1` | 540 | 126 | 12072 | 17509 | 7818 | 0.2207 | 600 | 608.0 | 1 |
| `YORKNET` | `a+b1` | 312 | 256 | 1188 | 26039 | 13892 | 0.2765 | 600 | 47.0 | 0 |
| `READING6` | `b1` | 102 | 50 | 212 | 604 | 8 | 0.0021 | 600 | 0.0 | 0 |
| `READING6` | `a+b1` | 102 | 50 | 212 | 604 | 8 | 0.0021 | 600 | 0.0 | 0 |
| `SMBANK` | `b1` | 117 | 64 | 58 | 0 | 334 | 1.0000 | 600 | 0.0 | 0 |
| `SMBANK` | `a+b1` | 117 | 64 | 58 | 0 | 334 | 1.0000 | 600 | 0.0 | 0 |
| `ZAMB2-8` | `b1` | 138 | 48 | 736 | 8191 | 3080 | 0.4620 | 600 | 6.0 | 0 |
| `ZAMB2-8` | `a+b1` | 138 | 48 | 680 | 7812 | 2963 | 0.4445 | 600 | 4.0 | 0 |

## 11. A6's prediction, both halves (RULE.md section 4)

**OFFLINE (the pre-registered check, run BEFORE the dynamic leg and
recorded in `logs/a6-offline.txt` and STEP 2 of `logs/chain-m5.txt`):**
2538 core drops examined over M4's pinned `off-YORKNET.csv` -- the exact
number A6 predicted, and cross-checked against `m4c_tails.py`'s own
`tails.json` (2538 == 2538) rather than taken on trust.  The scaled test
REFUSES 2538 and ADMITS 0; no pass lacked a published scale; the ratio
`|lambda|/(opt_tol*scale)` is 1.84849e-10 at min, p50, p90 and max.
**A6's offline half HOLDS.**

**DYNAMIC (A6's refutation condition, verbatim): refutation means the
SAME candidate identity is still dropped under the arm, or the SAME
matched recurrence persists.  A DIFFERENT tail surviving is NOT a
refutation and is reported as _diagnosis untested by this trajectory_.**

| leg | recurrent solves | representative released identities | classes |
|---|---|---|---|
| `off-YORKNET` | 2 | `s7:bound:393:lower`, `s145:bound:1:lower` | {'a': 1, 'b': 1} |
| `a-YORKNET` | 6 | `s8:bound:93:lower`, `s26:bound:18:upper`, `s27:bound:212:upper`, `s28:bound:479:lower`, `s29:bound:236:upper`, `s30:bound:249:lower` | {'e': 2, 'b': 3, 'd': 1} |
| `a+b1-YORKNET` | 2 | `s2:bound:16:lower`, `s3:bound:683:upper` | {'b': 1, 'c': 1} |

* `a-YORKNET`: OFF's cycling identities are `bound:1:lower`, `bound:393:lower`; the arm's surviving
  tails release `bound:18:upper`, `bound:212:upper`, `bound:236:upper`, `bound:249:lower`, `bound:479:lower`, `bound:93:lower`.  Intersection: **EMPTY**.
* `a+b1-YORKNET`: OFF's cycling identities are `bound:1:lower`, `bound:393:lower`; the arm's surviving
  tails release `bound:16:lower`, `bound:683:upper`.  Intersection: **EMPTY**.

