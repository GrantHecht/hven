# M7 W2 M4b — **POST-HOC**: one full period of `ACOPP57`'s tail,
# record by record, arm `off` against arm `E`

Written by `wrappers/acopp57_period_m4b.py` from the committed slices
`traces-m4b/off-ACOPP57-solve6.csv` and `traces-m4b/E-ACOPP57-solve6.csv`.
**No solver ran here.** **POST-HOC**: `RULE.md` §6 does not pre-register
this reconstruction, and nothing in `verdict.txt` rests on it.

The solve is `ACOPP57` solve 6 — the run's last QP, the one the flat
trace's own header describes (`solve_index=6`, `max_iter=1795`,
`exit_status=max_iter`). Its maximal exactly-periodic suffix on the
bit-exact F0 state key has **period 2** under both arms
(`tables/readouts-m4b.md`). The two records below are minors 1780 and 1781,
well inside that suffix.

## Record 1 of the period — minor 1780

| column | `off` | `E` | |
|---|---|---|---|
| `solve` | `6` | `6` |  |
| `minor` | `1780` | `1780` |  |
| `action` | `add` | `add` |  |
| `block` | `bound` | `bound` |  |
| `block_index` | `14` | `14` |  |
| `block_side` | `lower` | `lower` |  |
| `drop_route` | `none` | `none` |  |
| `bound_gap` | `0.25` | `0.25` |  |
| `alpha` | `-0` | `7.0061491147510168e-08` | **DIFFERS** |
| `p_inf` | `3.3341635608911957e-06` | `3.3341635608911957e-06` |  |
| `dx_inf` | `-0` | `2.3359647080572948e-13` | **DIFFERS** |
| `step_tol` | `1.0000000000000001e-09` | `1.0000000000000001e-09` |  |
| `degenerate` | `1` | `1` |  |
| `ride` | `0` | `0` |  |
| `raw_ratio` | `-0` | `7.0061491147510168e-08` | **DIFFERS** |
| `ratio_tied` | `1` | `1` |  |
| `ratio_capped` | `1` | `1` |  |
| `decision_multiplier` | `nan` | `nan` |  |
| `decision_tol` | `nan` | `nan` |  |
| `best_multiplier` | `nan` | `nan` |  |
| `drop_candidates` | `0` | `0` |  |
| `drop_tied` | `0` | `0` |  |
| `shift_adds` | `0` | `0` |  |
| `ws_size_in` | `21` | `21` |  |
| `ws_sig_in` | `13695891117004593176` | `13695891117004593176` |  |
| `ws_size_out` | `22` | `22` |  |
| `ws_sig_out` | `7832398285949506436` | `7832398285949506436` |  |
| `x_inf` | `0.125` | `0.125` |  |
| `x_l1` | `7.1252949701126038` | `7.1252949701126038` |  |
| `inertia_verdict` | `2` | `2` |  |
| `inertia_state` | `observed` | `observed` |  |
| `inertia_pos` | `127` | `127` |  |
| `inertia_neg` | `115` | `115` |  |
| `inertia_zero` | `0` | `0` |  |
| `inertia_zero_derived` | `1` | `1` |  |
| `inertia_perturbed` | `0` | `0` |  |
| `inertia_expected_pos` | `118` | `118` |  |
| `inertia_expected_neg` | `124` | `124` |  |
| `eqp_path` | `border_schur` | `border_schur` |  |
| `border_attempted` | `1` | `1` |  |
| `k0_rebuilt` | `0` | `0` |  |
| `border_latched` | `0` | `0` |  |
| `border_needs_refactorization` | `0` | `0` |  |
| `border_dim` | `21` | `21` |  |
| `eqp_call_refine_steps` | `1` | `1` |  |
| `pass_factorizations` | `0` | `0` |  |
| `pass_eqp_refine_steps` | `0` | `0` |  |
| `pass_border_refine_steps` | `1` | `1` |  |
| `pass_verdict_refine_steps` | `0` | `0` |  |
| `pass_schur_updates` | `1` | `1` |  |
| `post_drop` | `1` | `1` |  |
| `curvature_fired` | `0` | `0` |  |
| `curvature` | `2188.772320394291` | `2188.772320394291` |  |
| `curvature_tol` | `4.7024262380827144e-07` | `4.7024262380827144e-07` |  |
| `p_hp` | `2.4463883625668657e-07` | `2.4463883625668657e-07` |  |
| `p_sq` | `1.1176988761106807e-10` | `1.1176988761106807e-10` |  |
| `ride_outcome` | `not_attempted` | `not_attempted` |  |
| `ride_sign_chosen` | `0` | `0` |  |
| `ride_slope` | `-1.9653271104449577e-07` | `-1.9653271104449577e-07` |  |
| `ride_slope_tol` | `nan` | `nan` |  |
| `ride_leaves` | `nan` | `nan` |  |
| `ride_leaves_tol` | `nan` | `nan` |  |
| `released_block` | `bound` | `bound` |  |
| `released_index` | `14` | `14` |  |
| `released_side` | `lower` | `lower` |  |
| `p_released` | `-6.993856282166977e-07` | `-6.993856282166977e-07` |  |
| `released_directional` | `6.993856282166977e-07` | `6.993856282166977e-07` |  |
| `candidates_recorded` | `0` | `0` |  |
| `candidates` | `` | `` |  |
| `primal_delta` | `1e-08` | `1e-08` |  |
| `dual_mu` | `1e-08` | `1e-08` |  |
| `stat_free_inf` | `0.088419918923745511` | `0.088419918923745511` |  |
| `stat_scale` | `4332.4005192050518` | `4332.4005192050518` |  |
| `active_res_inf` | `9.0398276400811836e-05` | `9.0398276400811836e-05` |  |

## Record 2 of the period — minor 1781

| column | `off` | `E` | |
|---|---|---|---|
| `solve` | `6` | `6` |  |
| `minor` | `1781` | `1781` |  |
| `action` | `drop_dantzig` | `drop_dantzig` |  |
| `block` | `bound` | `bound` |  |
| `block_index` | `14` | `14` |  |
| `block_side` | `lower` | `lower` |  |
| `drop_route` | `dantzig` | `dantzig` |  |
| `bound_gap` | `0.25` | `0.25` |  |
| `alpha` | `nan` | `nan` |  |
| `p_inf` | `0` | `2.3359647075287156e-13` | **DIFFERS** |
| `dx_inf` | `nan` | `nan` |  |
| `step_tol` | `1.0000000000000001e-09` | `1.0000000000000001e-09` |  |
| `degenerate` | `0` | `0` |  |
| `ride` | `0` | `0` |  |
| `raw_ratio` | `nan` | `nan` |  |
| `ratio_tied` | `0` | `0` |  |
| `ratio_capped` | `1` | `1` |  |
| `decision_multiplier` | `0.10734366316506212` | `0.10734366316506212` |  |
| `decision_tol` | `1.0000000000000001e-09` | `1.0000000000000001e-09` |  |
| `best_multiplier` | `0.10734366316506212` | `0.10734366316506212` |  |
| `drop_candidates` | `9` | `9` |  |
| `drop_tied` | `1` | `1` |  |
| `shift_adds` | `0` | `0` |  |
| `ws_size_in` | `22` | `22` |  |
| `ws_sig_in` | `7832398285949506436` | `7832398285949506436` |  |
| `ws_size_out` | `21` | `21` |  |
| `ws_sig_out` | `13695891117004593176` | `13695891117004593176` |  |
| `x_inf` | `0.125` | `0.12500000000004899` | **DIFFERS** |
| `x_l1` | `7.1252949701126038` | `7.1252949701170047` | **DIFFERS** |
| `inertia_verdict` | `2` | `2` |  |
| `inertia_state` | `observed` | `observed` |  |
| `inertia_pos` | `127` | `127` |  |
| `inertia_neg` | `115` | `115` |  |
| `inertia_zero` | `0` | `0` |  |
| `inertia_zero_derived` | `1` | `1` |  |
| `inertia_perturbed` | `0` | `0` |  |
| `inertia_expected_pos` | `117` | `117` |  |
| `inertia_expected_neg` | `125` | `125` |  |
| `eqp_path` | `border_schur` | `border_schur` |  |
| `border_attempted` | `1` | `1` |  |
| `k0_rebuilt` | `0` | `0` |  |
| `border_latched` | `0` | `0` |  |
| `border_needs_refactorization` | `0` | `0` |  |
| `border_dim` | `22` | `22` |  |
| `eqp_call_refine_steps` | `1` | `1` |  |
| `pass_factorizations` | `0` | `0` |  |
| `pass_eqp_refine_steps` | `0` | `0` |  |
| `pass_border_refine_steps` | `1` | `1` |  |
| `pass_verdict_refine_steps` | `0` | `0` |  |
| `pass_schur_updates` | `1` | `1` |  |
| `post_drop` | `0` | `0` |  |
| `curvature_fired` | `0` | `0` |  |
| `curvature` | `nan` | `nan` |  |
| `curvature_tol` | `nan` | `nan` |  |
| `p_hp` | `nan` | `nan` |  |
| `p_sq` | `0` | `5.4863423109202466e-25` | **DIFFERS** |
| `ride_outcome` | `not_attempted` | `not_attempted` |  |
| `ride_sign_chosen` | `0` | `0` |  |
| `ride_slope` | `nan` | `nan` |  |
| `ride_slope_tol` | `nan` | `nan` |  |
| `ride_leaves` | `nan` | `nan` |  |
| `ride_leaves_tol` | `nan` | `nan` |  |
| `released_block` | `none` | `none` |  |
| `released_index` | `-1` | `-1` |  |
| `released_side` | `free` | `free` |  |
| `p_released` | `nan` | `nan` |  |
| `released_directional` | `nan` | `nan` |  |
| `candidates_recorded` | `8` | `8` |  |
| `candidates` | `bound:14:lower:0.10734366316506212; bound:8:lower:0.042016826292560761; bound:106:lower:0.034040831116967249; bound:46:lower:0.033268160271048522; bound:50:lower:0.023179303751837743; bound:12:lower:0.018271248357186833; bound:38:lower:0.0066255244528510273; bound:104:lower:0.0011170299204592551` | `bound:14:lower:0.10734366316506212; bound:8:lower:0.042016826292560761; bound:106:lower:0.034040831116967249; bound:46:lower:0.033268160271048522; bound:50:lower:0.023179303751837743; bound:12:lower:0.018271248357186833; bound:38:lower:0.0066255244528510273; bound:104:lower:0.0011170299204592551` |  |
| `primal_delta` | `1e-08` | `1e-08` |  |
| `dual_mu` | `1e-08` | `1e-08` |  |
| `stat_free_inf` | `0.083394424498233949` | `0.083394428876541443` | **DIFFERS** |
| `stat_scale` | `4332.4007441701169` | `4332.4007441701169` |  |
| `active_res_inf` | `9.0398276400811836e-05` | `9.0398277397006404e-05` | **DIFFERS** |

## The period repeats EXACTLY

Every column of minors 1780–1781 **except `solve` and `minor`, which are
the record's ADDRESS and not its state**, is byte-identical to the same
column of minors 1782–1783: under `off` **TRUE**, under `E` **TRUE**.
That is 72 of the trace's 74 columns — a SUPERSET of the key the tail
finder itself uses (`classify_cause_fix1.F0` is
`COLUMNS[2:COLUMNS.index("inertia_verdict")]`, which stops before the
inertia and cost columns). This check does not stop there and still
finds exact repetition.

## The identities this read asserts, re-derived in IEEE double

`tau` is this leg's own printed `arm_tau` = `4.9000000000000006e-14`.

| identity | left | right | BIT-equal |
|---|---|---|---|
| `alpha` == `tau / \|p_released\|` (the EXPAND floor `alpha_min`) | `7.006149114751017e-08` | `7.006149114751017e-08` | **True** |
| `dx_inf` == `alpha * p_inf` | `2.335964708057295e-13` | `2.335964708057295e-13` | **True** |
| `alpha * \|p_released\|` == `tau` (the released variable moves exactly tau) | `4.9000000000000006e-14` | `4.9000000000000006e-14` | **True** |
| next record's `x_inf` == this record's `x_inf` + `tau` | `0.125000000000049` | `0.125000000000049` | **True** |
| `off`'s `alpha` on the same record is exactly zero | `-0` | `0` | **True** |
| `off`'s `x_inf` does NOT move across the period | `0.125` | `0.125` | **True** |

