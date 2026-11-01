# M7 W2 M4b — `tables/arm.md`

Written by `wrappers/table_arm_m4b.py` out of `rows/E-<CELL>.txt` and
`rows-off/off-<CELL>.txt`. **No solver ran here and no number is
retyped.** The consensus column is machine-read from M3c's committed
`../2026-09-m7-w2-m3c-default-census/tables/census.md`.

**The subject is arm E AS IMPLEMENTED IN THE PINNED M4 BINARY**
(`RULE.md` §3.1, sha256 `c1be17d2…fe50a720`), never "EXPAND" and never
"canonical EXPAND". Two departures from the paper live in that binary
and are disclosed in `README.md`; the columns they bear on are flagged
in §3 below.

## 1. The ten cells, E against its copied OFF control

`RULE.md` §12's asked-for columns. `minors E/off` is printed to four
places. `f vs OFF` is the relative move `|f_E − f_off| / max(1,|f_off|)`;
`f vs consensus` is `|f_E − consensus| / max(1,|consensus|)` where a
consensus exists.

| cell | status off → E | majors off → E | minors off → E | minors E/off | cap exits off → E | restoration iters off → E | `arm_min_steps` | `arm_unpinned_adds` | `arm_pivot_switches` | `arm_resets` | f vs OFF | f vs consensus | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ACOPP57 | `numerical_error` → `numerical_error` | 7 → 7 | 7248 → 7248 | **1.0000** | 4 → 4 | 0 → 0 | 3555 | 3555 | 0 | 0 | 0.000e+00 | 6.155e-06 | none |
| DALLASM | `numerical_error` → `numerical_error` | 5 → 5 | 10583 → 10720 | **1.0129** | 4 → 4 | 2 → 2 | 5285 | 5285 | 1908 | 0 | 2.431e-04 | 1.287e+03 | none |
| STEENBRB | `numerical_error` → `numerical_error` | 5 → 5 | 20637 → 20470 | **0.9919** | 4 → 4 | 11 → 11 | 9508 | 9401 | 3110 | 0 | 2.834e-01 | 3.230e+08 | none |
| ACOPP118 | `numerical_error` → `numerical_error` | 7 → 7 | 19917 → 19842 | **0.9962** | 4 → 4 | 0 → 0 | 9229 | 9229 | 14 | 0 | 0.000e+00 | 9.506e-05 | none |
| ACOPR57 | `numerical_error` → `numerical_error` | 15 → 15 | 13874 → 13857 | **0.9988** | 5 → 5 | 0 → 0 | 3000 | 3000 | 28 | 0 | 2.441e-15 | 3.613e-07 | none |
| STEENBRC | `numerical_error` → `numerical_error` | 6 → 5 | 35324 → 29498 | **0.8351** | 5 → 4 | 16 → 14 | 12936 | 12783 | 8335 | 0 | 6.900e-01 | ABSENT (no consensus) | none |
| STEENBRG | `numerical_error` → `numerical_error` | 6 → 6 | 35429 → 30084 | **0.8491** | 5 → 4 | 16 → 14 | 13998 | 13819 | 6740 | 0 | 8.256e-01 | ABSENT (no consensus) | none |
| YORKNET | `infeasible` → `infeasible` | 1 → 1 | 50241 → 67259 | **1.3387** | 0 → 0 | 18 → 34 | 24993 | 24230 | 20409 | 0 | 0.000e+00 | 6.772e+00 | none |
| SAWPATH | `interrupted` → `interrupted` | 1 → 1 | 77037 → 59600 | **0.7737** | 4 → 3 | 4 → 3 | 24088 | 24088 | 1946 | 3 | 0.000e+00 | 9.764e-01 | **TRUNCATED** |
| BRITGAS | `numerical_error` → `numerical_error` | 4 → 4 | 92309 → 70436 | **0.7630** | 9 → 7 | 7 → 6 | 29897 | 29862 | 1288 | 4 | 2.732e-01 | 3.487e-01 | none |

The `verdict` column is `verdict.txt`'s, applied here by the same
rank order (M4 `RULE.md` §7.2) so the table and the verdict file cannot
drift. `none` means rank(E) == rank(OFF): not `CONVERTED`, not `HARM`,
not `improved, unverified`.

The consensus strings, verbatim from M3c's census:

* `ACOPP57` — consensus 4.17378e+04 (6 significant digits): lanes hven-ipm, ipopt, snopt
* `DALLASM` — consensus -4.81982e+04 (6 significant digits): lanes hven-ipm, ipopt, snopt
* `STEENBRB` — consensus 9.07586e+03 (6 significant digits): lanes hven-ipm, ipopt, snopt
* `ACOPP118` — consensus 1.29661e+05 (6 significant digits): lanes hven-ipm, ipopt, snopt
* `ACOPR57` — consensus 4.17378e+04 (6 significant digits): lanes ipopt, snopt
* `STEENBRC` — ABSENT (no consensus): counted lanes hven-ipm=2.84286e+04, ipopt=2.85195e+04 agree nowhere to 6 significant digits
* `STEENBRG` — ABSENT (no consensus): counted lanes hven-ipm=2.82230e+04, ipopt=7.44656e+04 agree nowhere to 6 significant digits
* `YORKNET` — consensus 1.42400e+04 (6 significant digits): lanes hven-ipm, ipopt
* `SAWPATH` — consensus 7.50459e+02 (6 significant digits): lanes hven-ipm, snopt
* `BRITGAS` — consensus 0 (the |f| <= 1e-06 arm): lanes hven-ipm, snopt

## 2. Every M4 §7.1 field, plus §6.6's arm counters, per cell

`<<` marks a value that differs from that cell's OFF control.

### ACOPP57

| field | off | E |
|---|---|---|
| `status` | `numerical_error` | `numerical_error` |
| `major_iters` | `7` | `7` |
| `qp_minor_iters` | `7248` | `7248` |
| `factorizations` | `128` | `128` |
| `f` | `41737.543107411046` | `41737.543107411046` |
| `stationarity` | `0.6386400287759757` | `0.6386400287759757` |
| `feasibility_e` | `0.00011385871678863679` | `0.00011385871678863679` |
| `feasibility_i` | `0` | `0` |
| `steps_accepted` | `3` | `3` |
| `restoration_entries` | `0` | `0` |
| `restoration_sub_status` | `not_run` | `not_run` |
| `restoration_iters` | `0` | `0` |
| `first_drop_ties` | `0` | `0` |
| `first_ratio_ties` | `2` | `2` |
| `first_degenerate_fraction` | `0.058823529411764705` | `0.058823529411764705` |
| `qp_maxiter_exits` | `4` | `4` |
| `qp_numerical_exits` | `0` | `0` |
| `qp_failures_routed` | `3` | `3` |
| `deadline_fired` | `0` | `0` |
| `arm_min_steps` | `0` | `3555` << |
| `arm_unpinned_adds` | `0` | `3555` << |
| `arm_pivot_switches` | `0` | `0` |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `5.8795500000001216e-10` << |
| `arm_expand_iter_at_exit` | `0` | `1795` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 7 | 7 |
| minors | 7248 | 7248 |
| degenerate minors | 3555 | 3555 |
| rides | 0 | 0 |
| drops | 3574 | 3574 |
| adds | 3624 | 3624 |

### DALLASM

| field | off | E |
|---|---|---|
| `status` | `numerical_error` | `numerical_error` |
| `major_iters` | `5` | `5` |
| `qp_minor_iters` | `10583` | `10720` << |
| `factorizations` | `4919` | `5041` << |
| `f` | `61974881.482906975` | `61959815.32108219` << |
| `stationarity` | `89474.28830762775` | `89474.28830762775` |
| `feasibility_e` | `4.496403249731884e-14` | `5.6843418860808015e-14` << |
| `feasibility_i` | `0` | `0` |
| `steps_accepted` | `0` | `0` |
| `restoration_entries` | `1` | `1` |
| `restoration_sub_status` | `optimal` | `optimal` |
| `restoration_iters` | `2` | `2` |
| `first_drop_ties` | `0` | `0` |
| `first_ratio_ties` | `1` | `77` << |
| `first_degenerate_fraction` | `0.0086206896551724137` | `0.02100840336134454` << |
| `qp_maxiter_exits` | `4` | `4` |
| `qp_numerical_exits` | `0` | `0` |
| `qp_failures_routed` | `3` | `3` |
| `deadline_fired` | `0` | `0` |
| `arm_min_steps` | `0` | `5285` << |
| `arm_unpinned_adds` | `0` | `5285` << |
| `arm_pivot_switches` | `0` | `1908` << |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `5.9604000000001328e-10` << |
| `arm_expand_iter_at_exit` | `0` | `1960` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 8 | 10 << |
| minors | 10583 | 10720 << |
| degenerate minors | 5163 | 5264 << |
| rides | 3 | 0 << |
| drops | 4815 | 4897 << |
| adds | 5664 | 5712 << |

### STEENBRB

| field | off | E |
|---|---|---|
| `status` | `numerical_error` | `numerical_error` |
| `major_iters` | `5` | `5` |
| `qp_minor_iters` | `20637` | `20470` << |
| `factorizations` | `25973` | `24457` << |
| `f` | `4090860686442.3218` | `2931397405519.7446` << |
| `stationarity` | `25043075565569.852` | `17646045803189.473` << |
| `feasibility_e` | `2.2737367542337972e-13` | `1.3073986337985843e-12` << |
| `feasibility_i` | `2.1316282072803006e-14` | `3.1675341477781286e-13` << |
| `steps_accepted` | `0` | `0` |
| `restoration_entries` | `1` | `1` |
| `restoration_sub_status` | `optimal` | `optimal` |
| `restoration_iters` | `11` | `11` |
| `first_drop_ties` | `19` | `32` << |
| `first_ratio_ties` | `10` | `16` << |
| `first_degenerate_fraction` | `0.021765417170495769` | `0.01437125748502994` << |
| `qp_maxiter_exits` | `4` | `4` |
| `qp_numerical_exits` | `0` | `0` |
| `qp_failures_routed` | `3` | `3` |
| `deadline_fired` | `0` | `0` |
| `arm_min_steps` | `0` | `9508` << |
| `arm_unpinned_adds` | `0` | `9401` << |
| `arm_pivot_switches` | `0` | `3110` << |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `7.2932000000003166e-10` << |
| `arm_expand_iter_at_exit` | `0` | `4680` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 17 | 17 |
| minors | 20637 | 20470 << |
| degenerate minors | 7400 | 9514 << |
| rides | 2423 | 204 << |
| drops | 9398 | 9337 << |
| adds | 8715 | 10843 << |

### ACOPP118

| field | off | E |
|---|---|---|
| `status` | `numerical_error` | `numerical_error` |
| `major_iters` | `7` | `7` |
| `qp_minor_iters` | `19917` | `19842` << |
| `factorizations` | `20501` | `20354` << |
| `f` | `129648.67425337316` | `129648.67425337316` |
| `stationarity` | `25.149138631597573` | `25.149138631597573` |
| `feasibility_e` | `0.0065403173774409851` | `0.0065403173774409851` |
| `feasibility_i` | `0` | `0` |
| `steps_accepted` | `3` | `3` |
| `restoration_entries` | `0` | `0` |
| `restoration_sub_status` | `not_run` | `not_run` |
| `restoration_iters` | `0` | `0` |
| `first_drop_ties` | `0` | `0` |
| `first_ratio_ties` | `14` | `15` << |
| `first_degenerate_fraction` | `0.015789473684210527` | `0.0045714285714285718` << |
| `qp_maxiter_exits` | `4` | `4` |
| `qp_numerical_exits` | `0` | `0` |
| `qp_failures_routed` | `3` | `3` |
| `deadline_fired` | `0` | `0` |
| `arm_min_steps` | `0` | `9229` << |
| `arm_unpinned_adds` | `0` | `9229` << |
| `arm_pivot_switches` | `0` | `14` << |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `7.3079000000003186e-10` << |
| `arm_expand_iter_at_exit` | `0` | `4710` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 7 | 7 |
| minors | 19917 | 19842 << |
| degenerate minors | 9229 | 9220 << |
| rides | 0 | 0 |
| drops | 9629 | 9606 << |
| adds | 9860 | 9833 << |

### ACOPR57

| field | off | E |
|---|---|---|
| `status` | `numerical_error` | `numerical_error` |
| `major_iters` | `15` | `15` |
| `qp_minor_iters` | `13874` | `13857` << |
| `factorizations` | `4848` | `8179` << |
| `f` | `41737.784920103695` | `41737.784920103797` << |
| `stationarity` | `1.0170747807984053` | `1.017074780796734` << |
| `feasibility_e` | `0.053054942831474383` | `0.053054942862555077` << |
| `feasibility_i` | `0.019326699816629656` | `0.019326699816885562` << |
| `steps_accepted` | `8` | `8` |
| `restoration_entries` | `0` | `0` |
| `restoration_sub_status` | `not_run` | `not_run` |
| `restoration_iters` | `0` | `0` |
| `first_drop_ties` | `0` | `0` |
| `first_ratio_ties` | `2` | `2` |
| `first_degenerate_fraction` | `0.068181818181818177` | `0.068181818181818177` |
| `qp_maxiter_exits` | `5` | `5` |
| `qp_numerical_exits` | `0` | `0` |
| `qp_failures_routed` | `4` | `4` |
| `deadline_fired` | `0` | `0` |
| `arm_min_steps` | `0` | `3000` << |
| `arm_unpinned_adds` | `0` | `3000` << |
| `arm_pivot_switches` | `0` | `28` << |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `6.0192000000001409e-10` << |
| `arm_expand_iter_at_exit` | `0` | `2080` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 17 | 17 |
| minors | 13874 | 13857 << |
| degenerate minors | 4322 | 3010 << |
| rides | 0 | 0 |
| drops | 5654 | 5275 << |
| adds | 5504 | 5161 << |

### STEENBRC

| field | off | E |
|---|---|---|
| `status` | `numerical_error` | `numerical_error` |
| `major_iters` | `6` | `5` << |
| `qp_minor_iters` | `35324` | `29498` << |
| `factorizations` | `42946` | `35378` << |
| `f` | `183453796271334.12` | `56874874174968.398` << |
| `stationarity` | `1567584111728531.2` | `295720530811416.19` << |
| `feasibility_e` | `9.0949470177292824e-13` | `3.1832314562052488e-12` << |
| `feasibility_i` | `9.0949470177292824e-13` | `3.7704546681392107e-18` << |
| `steps_accepted` | `0` | `0` |
| `restoration_entries` | `1` | `1` |
| `restoration_sub_status` | `optimal` | `optimal` |
| `restoration_iters` | `16` | `14` << |
| `first_drop_ties` | `4` | `28` << |
| `first_ratio_ties` | `1166` | `21` << |
| `first_degenerate_fraction` | `0.21777777777777776` | `0.023613963039014373` << |
| `qp_maxiter_exits` | `5` | `4` << |
| `qp_numerical_exits` | `1` | `0` << |
| `qp_failures_routed` | `4` | `3` << |
| `deadline_fired` | `0` | `0` |
| `arm_min_steps` | `0` | `12936` << |
| `arm_unpinned_adds` | `0` | `12783` << |
| `arm_pivot_switches` | `0` | `8335` << |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `7.6460000000003653e-10` << |
| `arm_expand_iter_at_exit` | `0` | `5400` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 28 | 25 << |
| minors | 35324 | 29498 << |
| degenerate minors | 15049 | 11265 << |
| rides | 1609 | 366 << |
| drops | 16216 | 12829 << |
| adds | 16843 | 14538 << |

### STEENBRG

| field | off | E |
|---|---|---|
| `status` | `numerical_error` | `numerical_error` |
| `major_iters` | `6` | `6` |
| `qp_minor_iters` | `35429` | `30084` << |
| `factorizations` | `43096` | `36106` << |
| `f` | `183453796271333.81` | `31988092421329.922` << |
| `stationarity` | `1567584111728531.2` | `124746481525945.16` << |
| `feasibility_e` | `9.0949470177292824e-13` | `2.7284841053187847e-12` << |
| `feasibility_i` | `9.0949470177292824e-13` | `4.9184475936492333e-12` << |
| `steps_accepted` | `0` | `1` << |
| `restoration_entries` | `1` | `1` |
| `restoration_sub_status` | `optimal` | `optimal` |
| `restoration_iters` | `16` | `14` << |
| `first_drop_ties` | `8` | `17` << |
| `first_ratio_ties` | `1165` | `22` << |
| `first_degenerate_fraction` | `0.21888888888888888` | `0.019715224534501644` << |
| `qp_maxiter_exits` | `5` | `4` << |
| `qp_numerical_exits` | `1` | `0` << |
| `qp_failures_routed` | `4` | `3` << |
| `deadline_fired` | `0` | `0` |
| `arm_min_steps` | `0` | `13998` << |
| `arm_unpinned_adds` | `0` | `13819` << |
| `arm_pivot_switches` | `0` | `6740` << |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `7.6460000000003653e-10` << |
| `arm_expand_iter_at_exit` | `0` | `5400` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 28 | 26 << |
| minors | 35429 | 30084 << |
| degenerate minors | 14194 | 13944 << |
| rides | 2777 | 450 << |
| drops | 16518 | 13936 << |
| adds | 15976 | 15607 << |

### YORKNET

| field | off | E |
|---|---|---|
| `status` | `infeasible` | `infeasible` |
| `major_iters` | `1` | `1` |
| `qp_minor_iters` | `50241` | `67259` << |
| `factorizations` | `52180` | `70688` << |
| `f` | `110670.29950761795` | `110670.29950761795` |
| `stationarity` | `1959.2999458312988` | `1959.2999458312988` |
| `feasibility_e` | `4099.2829199999996` | `4099.2829199999996` |
| `feasibility_i` | `131.08000000000001` | `131.08000000000001` |
| `steps_accepted` | `0` | `0` |
| `restoration_entries` | `1` | `1` |
| `restoration_sub_status` | `infeasible` | `infeasible` |
| `restoration_iters` | `18` | `34` << |
| `first_drop_ties` | `27` | `45` << |
| `first_ratio_ties` | `101` | `84` << |
| `first_degenerate_fraction` | `0.40554156171284633` | `0.27500000000000002` << |
| `qp_maxiter_exits` | `0` | `0` |
| `qp_numerical_exits` | `0` | `0` |
| `qp_failures_routed` | `0` | `0` |
| `deadline_fired` | `0` | `0` |
| `arm_min_steps` | `0` | `24993` << |
| `arm_unpinned_adds` | `0` | `24230` << |
| `arm_pivot_switches` | `0` | `20409` << |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `5.0068600000000013e-10` << |
| `arm_expand_iter_at_exit` | `0` | `14` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 146 | 280 << |
| minors | 50241 | 67259 << |
| degenerate minors | 18280 | 30432 << |
| rides | 4380 | 3443 << |
| drops | 16605 | 18766 << |
| adds | 26017 | 40570 << |

### SAWPATH

| field | off | E |
|---|---|---|
| `status` | `interrupted` | `interrupted` |
| `major_iters` | `1` | `1` |
| `qp_minor_iters` | `77037` | `59600` << |
| `factorizations` | `83624` | `66176` << |
| `f` | `1483.1937415552059` | `1483.1937415552059` |
| `stationarity` | `10.81874` | `10.81874` |
| `feasibility_e` | `0` | `0` |
| `feasibility_i` | `3.556` | `3.556` |
| `steps_accepted` | `0` | `0` |
| `restoration_entries` | `1` | `1` |
| `restoration_sub_status` | `numerical_error` | `interrupted` << |
| `restoration_iters` | `4` | `3` << |
| `first_drop_ties` | `0` | `0` |
| `first_ratio_ties` | `0` | `0` |
| `first_degenerate_fraction` | `0` | `0` |
| `qp_maxiter_exits` | `4` | `3` << |
| `qp_numerical_exits` | `0` | `0` |
| `qp_failures_routed` | `0` | `0` |
| `deadline_fired` | `1` | `1` |
| `arm_min_steps` | `0` | `24088` << |
| `arm_unpinned_adds` | `0` | `24088` << |
| `arm_pivot_switches` | `0` | `1946` << |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `3` << |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `8.6382500000005021e-10` << |
| `arm_expand_iter_at_exit` | `0` | `7425` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 11 | 10 << |
| minors | 77037 | 59600 << |
| degenerate minors | 35516 | 18209 << |
| rides | 0 | 0 |
| drops | 35636 | 24871 << |
| adds | 38651 | 27024 << |

### BRITGAS

| field | off | E |
|---|---|---|
| `status` | `numerical_error` | `numerical_error` |
| `major_iters` | `4` | `4` |
| `qp_minor_iters` | `92309` | `70436` << |
| `factorizations` | `93749` | `72752` << |
| `f` | `0.075494336118951622` | `0.34868407913843513` << |
| `stationarity` | `1.9999950748483581` | `1.9999995510848132` << |
| `feasibility_e` | `1.9982835739868769` | `1.9811314964381976` << |
| `feasibility_i` | `0` | `0` |
| `steps_accepted` | `0` | `0` |
| `restoration_entries` | `1` | `1` |
| `restoration_sub_status` | `numerical_error` | `numerical_error` |
| `restoration_iters` | `7` | `6` << |
| `first_drop_ties` | `0` | `0` |
| `first_ratio_ties` | `11` | `47` << |
| `first_degenerate_fraction` | `0.47866666666666668` | `0.47933333333333333` << |
| `qp_maxiter_exits` | `9` | `7` << |
| `qp_numerical_exits` | `0` | `0` |
| `qp_failures_routed` | `3` | `3` |
| `deadline_fired` | `0` | `0` |
| `arm_min_steps` | `0` | `29897` << |
| `arm_unpinned_adds` | `0` | `29862` << |
| `arm_pivot_switches` | `0` | `1288` << |
| `arm_lex_drop_switches` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` |
| `arm_resets` | `0` | `4` << |
| `arm_reset_nontrivial_moves` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `5.8330000000001152e-10` << |
| `arm_expand_iter_at_exit` | `0` | `1700` << |

Solve-summary aggregates, summed over the run's own per-solve table
in the same row file:

| | off | E |
|---|---|---|
| QP solves | 12 | 11 << |
| minors | 92309 | 70436 << |
| degenerate minors | 30047 | 29850 << |
| rides | 8257 | 1111 << |
| drops | 41666 | 32562 << |
| adds | 36315 | 34180 << |

## 3. The two departures in the pinned binary, and the columns they bite

Both were found by the Codex review of M4
(`.superpowers/m7-w2-m4arm-review-sol.md` §1a, §4). They are properties
of the binary every row above came from, so they are printed here beside
the counters they bear on rather than only argued in prose.

**(1) The EXPAND reset fires one iteration early.** `begin_minor()`
returns true at `expand_iter >= K` and the caller resets *before* that
minor's ratio test, so the K-th iteration runs at `delta_0` and not at
the expanded `delta`; the paper performs K expanded iterations and resets
afterwards. M4's neutrality legs never reached a reset, so M4's data
could not see this. **THESE LEGS DID.**

| cell | `arm_resets` | `arm_reset_nontrivial_moves` | `arm_expand_iter_at_exit` | `arm_delta_at_exit` | did the reset path run? |
|---|---|---|---|---|---|
| ACOPP57 | 0 | 0 | 1795 | 5.8795500000001216e-10 | no |
| DALLASM | 0 | 0 | 1960 | 5.9604000000001328e-10 | no |
| STEENBRB | 0 | 0 | 4680 | 7.2932000000003166e-10 | no |
| ACOPP118 | 0 | 0 | 4710 | 7.3079000000003186e-10 | no |
| ACOPR57 | 0 | 0 | 2080 | 6.0192000000001409e-10 | no |
| STEENBRC | 0 | 0 | 5400 | 7.6460000000003653e-10 | no |
| STEENBRG | 0 | 0 | 5400 | 7.6460000000003653e-10 | no |
| YORKNET | 0 | 0 | 14 | 5.0068600000000013e-10 | no |
| SAWPATH | 3 | 0 | 7425 | 8.6382500000005021e-10 | **YES — 3 time(s), under the off-by-one** |
| BRITGAS | 4 | 0 | 1700 | 5.8330000000001152e-10 | **YES — 4 time(s), under the off-by-one** |

**(2) The negative-curvature ride calls the armed ratio test but its
add path always re-pins the blocking bound exactly**, defeating the
`alpha_min` floor on that path. `arm_min_steps` counts every minor whose
step was the floor; `arm_unpinned_adds` counts those on which the exact
re-pin was actually skipped. **Equality means the ride floor path was
never exercised on that cell. Inequality means it was, and the
difference is the number of floor steps whose displacement was silently
re-pinned away.**

| cell | `arm_min_steps` | `arm_unpinned_adds` | difference = HIDDEN RE-PINS | ride floor path exercised? |
|---|---|---|---|---|
| ACOPP57 | 3555 | 3555 | **0** | no (equal) |
| DALLASM | 5285 | 5285 | **0** | no (equal) |
| STEENBRB | 9508 | 9401 | **107** | **YES** |
| ACOPP118 | 9229 | 9229 | **0** | no (equal) |
| ACOPR57 | 3000 | 3000 | **0** | no (equal) |
| STEENBRC | 12936 | 12783 | **153** | **YES** |
| STEENBRG | 13998 | 13819 | **179** | **YES** |
| YORKNET | 24993 | 24230 | **763** | **YES** |
| SAWPATH | 24088 | 24088 | **0** | no (equal) |
| BRITGAS | 29897 | 29862 | **35** | **YES** |
| **total** | | | **1237** | |

