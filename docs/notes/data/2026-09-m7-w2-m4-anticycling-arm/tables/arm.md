# M7 W2 M4 — `tables/arm.md`

Written by `wrappers/table_arm_m4.py` out of `rows/`, the rows this task's chain wrote. **No solver ran here and no number is retyped.**

RULE.md §12 asks for one row per cell × arm with §7.1's fields, §6.6's read-outs and the verdict. **§8 stopped all three arms at their neutrality legs, so PHASE 3 never ran and the ten cells of §1.1 have no arm leg at all.** Their OFF-control line is given first, because it is the whole of what this task has on them, and then the three neutrality cells × four arms in full.

## 1. The OFF controls — RULE.md §5's identity gate, phase 1

Read from `logs/gate-off.txt`: **REPRODUCES 10 / 10, DIFFERS 0, CENSORED 0, REFUSED 0, MISSING 0**, zero near-ulp acceptances. Every one of §1.1's ten cells reproduced M3c's committed pass-1 row on every compared field, so every cell was carried forward (`logs/passed-cells.txt`). No `OFF_CONTROL_FAILED` line exists.

| cell | status | majors | minors | facts | f | stationarity | feas_e | gate |
|---|---|---|---|---|---|---|---|---|
| ACOPP57 | `numerical_error` | 7 | 7248 | 128 | 41737.543107411046 | 0.6386400287759757 | 0.00011385871678863679 | REPRODUCES |
| DALLASM | `numerical_error` | 5 | 10583 | 4919 | 61974881.482906975 | 89474.28830762775 | 4.496403249731884e-14 | REPRODUCES |
| STEENBRB | `numerical_error` | 5 | 20637 | 25973 | 4090860686442.3218 | 25043075565569.852 | 2.2737367542337972e-13 | REPRODUCES |
| ACOPP118 | `numerical_error` | 7 | 19917 | 20501 | 129648.67425337316 | 25.149138631597573 | 0.0065403173774409851 | REPRODUCES |
| ACOPR57 | `numerical_error` | 15 | 13874 | 4848 | 41737.784920103695 | 1.0170747807984053 | 0.053054942831474383 | REPRODUCES |
| STEENBRC | `numerical_error` | 6 | 35324 | 42946 | 183453796271334.12 | 1567584111728531.2 | 9.0949470177292824e-13 | REPRODUCES |
| STEENBRG | `numerical_error` | 6 | 35429 | 43096 | 183453796271333.81 | 1567584111728531.2 | 9.0949470177292824e-13 | REPRODUCES |
| YORKNET | `infeasible` | 1 | 50241 | 52180 | 110670.29950761795 | 1959.2999458312988 | 4099.2829199999996 | REPRODUCES |
| SAWPATH | `interrupted` | 1 | 77037 | 83624 | 1483.1937415552059 | 10.81874 | 0 | REPRODUCES |
| BRITGAS | `numerical_error` | 4 | 92309 | 93749 | 0.075494336118951622 | 1.9999950748483581 | 1.9982835739868769 | REPRODUCES |

**No arm leg was run on any of them.** RULE.md §7.5's second `HARM` clause is `N/A` on these ten because OFF solves none of them; that is stated rather than printed as a vacuous "0 harmed", exactly as §7.5 requires. §7.3's `CONVERTED`, §7.4's `improved, unverified` and §7.7's SNOPT cost read-out have no cell to apply to.

## 2. The three neutrality cells × four arms — every §7.1 field

A cell is compared against **its own `off` leg** (RULE.md §5.5), at the STRICT bar. A `<<` marks a value that differs from that cell's `off`.

### READING6

| field | off | E | L | L+ |
|---|---|---|---|---|
| `status` | `optimal` | `optimal` | `optimal` | `numerical_error` << |
| `major_iters` | `13` | `13` | `13` | `1` << |
| `qp_minor_iters` | `3833` | `3119` << | `3833` | `8761` << |
| `factorizations` | `6837` | `5424` << | `6837` | `10568` << |
| `f` | `-144.65970057279054` | `-144.65970057279054` | `-144.65970057279054` | `0` << |
| `stationarity` | `1.2616574451840279e-12` | `1.2616574451840279e-12` | `1.2616574451840279e-12` | `2.2522192640193177` << |
| `feasibility_e` | `2.7977620220553945e-13` | `2.7977620220553945e-13` | `2.7977620220553945e-13` | `2.0356603120000001` << |
| `feasibility_i` | `0` | `0` | `0` | `0` |
| `steps_accepted` | `9` | `9` | `9` | `0` << |
| `restoration_entries` | `0` | `0` | `0` | `1` << |
| `restoration_sub_status` | `not_run` | `not_run` | `not_run` | `numerical_error` << |
| `first_drop_ties` | `0` | `0` | `0` | `0` |
| `first_ratio_ties` | `0` | `0` | `0` | `0` |
| `first_degenerate_fraction` | `0` | `0` | `0` | `0` |
| `qp_maxiter_exits` | `3` | `2` << | `3` | `4` << |
| `qp_numerical_exits` | `0` | `0` | `0` | `0` |
| `qp_failures_routed` | `3` | `2` << | `3` | `0` << |
| `deadline_fired` | `0` | `0` | `0` | `0` |
| `arm_min_steps` | `0` | `799` << | `0` | `0` |
| `arm_pivot_switches` | `0` | `135` << | `0` | `0` |
| `arm_unpinned_adds` | `0` | `799` << | `0` | `0` |
| `arm_lex_drop_switches` | `0` | `0` | `13` << | `3673` << |
| `arm_lex_ratio_switches` | `0` | `0` | `0` | `0` |
| `arm_resets` | `0` | `0` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `5.0009800000000004e-10` << | `5.0000000000000003e-10` << | `5.0000000000000003e-10` << |
| `arm_expand_iter_at_exit` | `0` | `2` << | `0` | `0` |

§7.1 also names `qp_failures_in_a_row_peak`, `restoration_h_in`, `restoration_h_out`, `restoration_f_in`, `restoration_f_out`; **this probe does not print them**, so they are ABSENT from every row here and are named rather than skipped.

Solve-summary aggregates, summed over the run's own per-solve table in the same row file:

| | off | E | L | L+ |
|---|---|---|---|---|
| QP solves | 41 | 41 | 41 | 7 << |
| minors | 3833 | 3119 << | 3833 | 8761 << |
| degenerate minors | 875 | 806 << | 875 | 2452 << |
| rides | 165 | 165 | 165 | 357 << |
| drops | 1366 | 1104 << | 1366 | 3682 << |
| adds | 1794 | 1544 << | 1794 | 3699 << |

minors ratio arm / off: **E 0.8137**, **L 1.0000**, **L+ 2.2857**

### SMBANK

| field | off | E | L | L+ |
|---|---|---|---|---|
| `status` | `optimal` | `optimal` | `optimal` | `optimal` |
| `major_iters` | `29` | `29` | `29` | `29` |
| `qp_minor_iters` | `334` | `328` << | `334` | `334` |
| `factorizations` | `29` | `29` | `29` | `29` |
| `f` | `-7129292` | `-7129292` | `-7129292` | `-7129292` |
| `stationarity` | `9.2748031187081952e-10` | `9.2748034416649174e-10` << | `9.2748031187081952e-10` | `9.2748031187081952e-10` |
| `feasibility_e` | `9.4587448984384537e-11` | `9.4587448984384537e-11` | `9.4587448984384537e-11` | `9.4587448984384537e-11` |
| `feasibility_i` | `0` | `0` | `0` | `0` |
| `steps_accepted` | `29` | `29` | `29` | `29` |
| `restoration_entries` | `0` | `0` | `0` | `0` |
| `restoration_sub_status` | `not_run` | `not_run` | `not_run` | `not_run` |
| `first_drop_ties` | `0` | `0` | `0` | `0` |
| `first_ratio_ties` | `0` | `0` | `0` | `0` |
| `first_degenerate_fraction` | `0` | `0.029411764705882353` << | `0` | `0` |
| `qp_maxiter_exits` | `0` | `0` | `0` | `0` |
| `qp_numerical_exits` | `0` | `0` | `0` | `0` |
| `qp_failures_routed` | `0` | `0` | `0` | `0` |
| `deadline_fired` | `0` | `0` | `0` | `0` |
| `arm_min_steps` | `0` | `1` << | `0` | `0` |
| `arm_pivot_switches` | `0` | `3` << | `0` | `0` |
| `arm_unpinned_adds` | `0` | `1` << | `0` | `0` |
| `arm_lex_drop_switches` | `0` | `0` | `0` | `6` << |
| `arm_lex_ratio_switches` | `0` | `0` | `0` | `0` |
| `arm_resets` | `0` | `0` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `5.0009800000000004e-10` << | `5.0000000000000003e-10` << | `5.0000000000000003e-10` << |
| `arm_expand_iter_at_exit` | `0` | `2` << | `0` | `0` |

§7.1 also names `qp_failures_in_a_row_peak`, `restoration_h_in`, `restoration_h_out`, `restoration_f_in`, `restoration_f_out`; **this probe does not print them**, so they are ABSENT from every row here and are named rather than skipped.

Solve-summary aggregates, summed over the run's own per-solve table in the same row file:

| | off | E | L | L+ |
|---|---|---|---|---|
| QP solves | 29 | 29 | 29 | 29 |
| minors | 334 | 328 << | 334 | 334 |
| degenerate minors | 1 | 1 | 1 | 1 |
| rides | 0 | 0 | 0 | 0 |
| drops | 18 | 18 | 18 | 18 |
| adds | 241 | 235 << | 241 | 241 |

minors ratio arm / off: **E 0.9820**, **L 1.0000**, **L+ 1.0000**

### ZAMB2-8

| field | off | E | L | L+ |
|---|---|---|---|---|
| `status` | `optimal` | `optimal` | `numerical_error` << | `infeasible` << |
| `major_iters` | `11` | `9` << | `2` << | `2` << |
| `qp_minor_iters` | `6666` | `9947` << | `5016` << | `10356` << |
| `factorizations` | `8803` | `12421` << | `6870` << | `11666` << |
| `f` | `-0.15293592733713887` | `-0.1529359281433445` << | `-0.093440994397389732` << | `0.018068535257036302` << |
| `stationarity` | `1.0198462227847685e-10` | `5.9880877247833406e-11` << | `0.99399504942264105` << | `0.010979121771749999` << |
| `feasibility_e` | `1.7950063938010352e-07` | `4.5707274076711712e-08` << | `1.7690471076822223` << | `2.3384617034999997` << |
| `feasibility_i` | `0` | `0` | `0` | `2.839` << |
| `steps_accepted` | `8` | `7` << | `0` << | `0` << |
| `restoration_entries` | `1` | `1` | `1` | `1` |
| `restoration_sub_status` | `optimal` | `optimal` | `numerical_error` << | `infeasible` << |
| `first_drop_ties` | `0` | `0` | `0` | `0` |
| `first_ratio_ties` | `55` | `39` << | `55` | `55` |
| `first_degenerate_fraction` | `0.48768115942028983` | `0.47898550724637678` << | `0.48768115942028983` | `0.47971014492753622` << |
| `qp_maxiter_exits` | `2` | `3` << | `1` << | `2` |
| `qp_numerical_exits` | `2` | `1` << | `7` << | `0` << |
| `qp_failures_routed` | `2` | `1` << | `1` << | `1` << |
| `deadline_fired` | `0` | `0` | `0` | `0` |
| `arm_min_steps` | `0` | `4187` << | `0` | `0` |
| `arm_pivot_switches` | `0` | `306` << | `0` | `0` |
| `arm_unpinned_adds` | `0` | `4187` << | `0` | `0` |
| `arm_lex_drop_switches` | `0` | `0` | `31` << | `4632` << |
| `arm_lex_ratio_switches` | `0` | `0` | `0` | `0` |
| `arm_resets` | `0` | `0` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` | `0` | `0` |
| `arm_delta_at_exit` | `0` | `5.0009800000000004e-10` << | `5.0000000000000003e-10` << | `5.0000000000000003e-10` << |
| `arm_expand_iter_at_exit` | `0` | `2` << | `0` | `0` |

§7.1 also names `qp_failures_in_a_row_peak`, `restoration_h_in`, `restoration_h_out`, `restoration_f_in`, `restoration_f_out`; **this probe does not print them**, so they are ABSENT from every row here and are named rather than skipped.

Solve-summary aggregates, summed over the run's own per-solve table in the same row file:

| | off | E | L | L+ |
|---|---|---|---|---|
| QP solves | 35 | 33 << | 28 << | 16 << |
| minors | 6666 | 9947 << | 5016 << | 10356 << |
| degenerate minors | 2300 | 4212 << | 1892 << | 4234 << |
| rides | 531 | 191 << | 218 << | 68 << |
| drops | 2792 | 4432 << | 2043 << | 4644 << |
| adds | 2961 | 4918 << | 2415 << | 5118 << |

minors ratio arm / off: **E 1.4922**, **L 0.7525**, **L+ 1.5536**

## 3. The neutrality verdicts, as the gate that ran recorded them

Read from `logs/gate-neutral-{E,L,L+}.txt`; the fields are that transcript's own `FIELDS DIFFER` line.

| arm | cell | verdict | near-ulp acceptances | fields that differ |
|---|---|---|---|---|
| E | READING6 | DOES NOT REPRODUCE | 0 | `border_refine_steps`, `factorizations`, `qp_failures_routed`, `qp_maxiter_exits`, `qp_minor_iters`, `summary_records`, `trace_out`, `trace_records` |
| E | SMBANK | DOES NOT REPRODUCE | 1 | `border_refine_steps`, `first_border_refine_steps`, `first_degenerate_fraction`, `first_degenerate_run_max`, `first_degenerate_steps`, `first_distinct_bound_added`, `first_minor_iters`, `first_schur_updates`, `first_ws_adds`, `first_ws_adds_bound`, `first_ws_drops`, `first_ws_drops_bound`, `qp_minor_iters`, `stationarity`, `summary_records`, `trace_out`, `trace_records` |
| E | ZAMB2-8 | DOES NOT REPRODUCE | 0 | `border_refine_steps`, `callback_events`, `f`, `factorizations`, `feasibility_e`, `first_border_refine_steps`, `first_degenerate_fraction`, `first_degenerate_run_max`, `first_degenerate_steps`, `first_distinct_bound_added`, `first_factorizations`, `first_ratio_ties`, `first_schur_updates`, `first_ws_adds`, `first_ws_adds_bound`, `first_ws_adds_minus_drops`, `first_ws_net_change`, `ledger_qp_records`, `major_iters`, `qp_failures_routed`, `qp_maxiter_exits`, `qp_minor_iters`, `qp_numerical_exits`, `rejected_steps`, `stationarity`, `steps_accepted`, `summary_records`, `summary_solves_seen`, `trace_out`, `trace_records`, `trace_solve_index`, `trace_solves_seen` |
| L | READING6 | DOES NOT REPRODUCE | 0 | `trace_out` |
| L | SMBANK | DOES NOT REPRODUCE | 0 | `trace_out` |
| L | ZAMB2-8 | DOES NOT REPRODUCE | 0 | `border_refine_steps`, `callback_events`, `complementarity`, `f`, `factorizations`, `feasibility_e`, `ledger_qp_records`, `ledger_sqp_status`, `major_iters`, `qp_failures_routed`, `qp_maxiter_exits`, `qp_minor_iters`, `qp_numerical_exits`, `rejected_steps`, `restoration_iters`, `restoration_sub_status`, `stationarity`, `status`, `steps_accepted`, `summary_records`, `summary_solves_seen`, `symbolic_analyses`, `trace_exit_status`, `trace_exit_status_raw`, `trace_incoming_ws_signature`, `trace_incoming_ws_size`, `trace_max_iter`, `trace_n`, `trace_out`, `trace_records`, `trace_solve_index`, `trace_solves_seen` |
| L+ | READING6 | DOES NOT REPRODUCE | 0 | `border_refine_steps`, `callback_events`, `complementarity`, `f`, `factorizations`, `feasibility_e`, `ledger_qp_records`, `ledger_sqp_status`, `major_iters`, `qp_failures_routed`, `qp_maxiter_exits`, `qp_minor_iters`, `rejected_steps`, `restoration_entries`, `restoration_iters`, `restoration_sub_status`, `stationarity`, `status`, `steps_accepted`, `summary_records`, `summary_solves_seen`, `symbolic_analyses`, `trace_exit_status`, `trace_exit_status_raw`, `trace_incoming_ws_signature`, `trace_incoming_ws_size`, `trace_max_iter`, `trace_n`, `trace_out`, `trace_records`, `trace_solve_index`, `trace_solves_seen` |
| L+ | SMBANK | DOES NOT REPRODUCE | 0 | `trace_out` |
| L+ | ZAMB2-8 | DOES NOT REPRODUCE | 0 | `border_refine_steps`, `callback_events`, `f`, `factorizations`, `feasibility_e`, `feasibility_i`, `first_degenerate_fraction`, `first_degenerate_run_max`, `first_degenerate_steps`, `first_distinct_bound_added`, `first_schur_updates`, `first_ws_adds`, `first_ws_adds_bound`, `first_ws_adds_minus_drops`, `first_ws_drops`, `first_ws_drops_bound`, `first_ws_net_change`, `ledger_qp_records`, `ledger_sqp_status`, `major_iters`, `qp_failures_routed`, `qp_minor_iters`, `qp_numerical_exits`, `rejected_steps`, `restoration_iters`, `restoration_sub_status`, `stationarity`, `status`, `steps_accepted`, `summary_records`, `summary_solves_seen`, `symbolic_analyses`, `trace_exit_status`, `trace_exit_status_raw`, `trace_incoming_ws_signature`, `trace_incoming_ws_size`, `trace_max_iter`, `trace_n`, `trace_out`, `trace_records`, `trace_solve_index`, `trace_solves_seen` |

