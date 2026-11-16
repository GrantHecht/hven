# hven machine-trace schema v1

The JSON-lines event stream `hven::JsonLinesTraceSink` writes
(`include/hven/drivers/json_lines_trace_sink.h`, `src/drivers/trace_writer.cpp`). **This document is the
schema.** It replaces `docs/trace-schema-v0.md` for every stream written from M8 TR-A on; v0 stays as the
schema of the frozen artifacts under `docs/notes/data/**` (v0 §8's pointer to this page is its only
change). The design is `docs/notes/2026-11-m8-tr1-trace-spec.md`; where this page and the code disagree,
the code (with its goldens) is the authority and this page is wrong.

Measured on MKL / Linux. A backend-dependent value read on Apple/Accelerate is **UNOBSERVED**, never
inferred (CLAUDE.md section 6). Counters and statuses in a stream are assertable; every wall-clock field
is informational (CLAUDE.md section 7).

`JsonLinesTraceSink::kSchemaVersion` is 1.

## 1. Envelope

Every line is one JSON object, `\n`-terminated. The first six keys, in this order:

| key | type | meaning |
|---|---|---|
| `v` | int | **1.** A reader rejects any other value. |
| `ev` | string | the record name (section 3) |
| `seq` | int | per SINK, from 1, +1 per line attempted |
| `sid` | int | per-sink ordinal of the enclosing depth-0 `solve.begin`, from 1 (0 before any begin). A solve that throws writes no `solve.end`; its `sid` has a begin and no end, and the next solve takes `sid + 1`. A reader treats that `sid` as an aborted solve, not a malformed stream. |
| `depth` | int | enclosing SQP solves; 0 on the interior-point engine, whose restoration runs in place |
| `eng` | string `ipm`/`sqp` | which engine wrote the line |

## 2. Types, absence, text

v0 section 2 carries over: `{:.17g}` doubles; `"nan"`/`"inf"`/`"-inf"` strings for a non-finite
MEASUREMENT; int64 integers; lower-snake enum strings; RFC 8259 escaping; no `"unknown"`. Rules v1
tightens:

1. **A field the emitting site did not compute is `null`.** Never a default, sentinel or projection.
2. **No vector-valued field.** Vectors belong to the callback (`IterationPayload`).
3. **Units.** A double is on the caller's objective scale unless its row says "engine scale". `_s` keys are
   wall-clock. Only `solve.end`, `ipm.phase.exit` and `ipm.kkt_analysis` carry them, so a deterministic
   solve's stream is byte-reproducible once those keys are masked. `solver_initialized` carries no
   milliseconds on the wire; a byte comparison drops that one kind.

## 3. Records

| `ev` | eng | one per |
|---|---|---|
| `solve.begin` | both | accepted public call (refused arguments write nothing) |
| `iter` | both | iteration row |
| `message` | both | diagnostic occurrence |
| `solve.end` | both | normal return (an exception writes none) |
| `ipm.phase.begin`, `ipm.phase.end`, `ipm.phase.exit` | ipm | phase that ran |
| `ipm.kkt_analysis` | ipm | `init_impl` |
| `qp.mode`, `ipqp.iter`, `ipqp.reg`, `ipqp.restart`, `ipqp.route`, `ipqp.certify`, `ipqp.escape`, `fallback.verdict` | sqp | as v0 sections 4.4-4.11; **bytes unchanged after the envelope** |

### 3.1 `iter`

`{envelope, core..., "ipm":{...}}` or `{envelope, core..., "sqp":{...}}`; exactly one block. C++:
`IterationRecord { core; std::optional<IpmIterationRow> ipm; std::optional<SqpIterationRow> sqp; }`.

**A row is the point plus the step taken FROM it, on both engines.** The IPM pushes its record after the
step is computed; its four unfactorized exit rows are reached by a step and have none leaving them (step
fields `null`). The IPM `IterationEvent` for point k+1 carries the step that REACHED it (identity 5).

Core, in wire order:

| key | type | IPM | SQP | `null` when |
|---|---|---|---|---|
| `iter` | int | per phase, restarts at 0 each phase; **not contiguous** (five `pop_back` sites) | history index; restarts at 0 per depth-1 sub-solve | never |
| `recovering` | bool | restoration active at this iterate | `depth >= 1` | never |
| `tier` | enum `bar`/`ipqp`/`ssn`/`walk`/`elastic`/`rest` | `bar`, `rest` while recovering | `rest` at depth 1; `elastic` if the elastic tier supplied the step; else the dispatch arm | never |
| `f` | double | `prim_obj / obj_scale` when the declared objective was measured | `SqpIterate::f` at depth 0 | recovering; phases that never evaluate it |
| `f_recovery` | double | `prim_obj` while recovering | `SqpIterate::f` at depth 1 | not recovering |
| `stationarity` | double | `kkt_inf / obj_scale` | `SqpIterate::stationarity` | never (`"nan"` if non-finite) |
| `feasibility` | double | `max(econ_inf, icon_inf)` | `SqpIterate::feasibility` | never |
| `complementarity` | double | `barr_inf / obj_scale` | `SqpIterate::complementarity` | never |
| `step` | double | inf-norm of the primal step leaving the point | `SqpIterate::step_norm` | no step leaves the point (IPM exit rows, non-finite direction; SQP `!qp_solved` or QP not optimal) |
| `accepted` | bool | `IpmIterate::accepted` | `verdict` is an accept | no step |
| `soc` | bool | SOC won this iteration's step (`resolved_depth == kRecoveryDepthSoc`) | `soc_applied` | never |
| `watchdog` | bool | the watchdog **armed** this iteration | the watchdog **restored** this pass | never; the meanings differ by engine |
| `rolled_back` | bool | a non-finite step rollback happened at this iteration | `rolled_back` | never |
| `factorizations` | int | the call's running KKT factorization count at the row | the (sub-)solve's running `counters.factorizations` | never |

The residuals are the ENGINE's convergence measures mapped to the caller's objective scale. They are not
the number the convergence test gated on under active scaling; `solve.end.convergence` is.

`ipm` block (`IpmIterationRow`): `phase`; `mu` (engine scale); `alpha_primal`, `alpha_dual` (the effective
lengths, `alpha_scale * ` the fraction-to-boundary maxima); `alpha_scale`; `line_search_trials`;
`regularization_primal`; `regularization_primal_total`; `regularization_dual`; `proximal_primal`;
`inertia_corrections`; `perturbed_pivots`; `barrier_objective`; `feasibility_eq`; `feasibility_ineq`;
`merit`; `merit_penalty_1`; `merit_penalty_2`; `max_multiplier_eq`; `max_multiplier_ineq`;
`first_rejected_trial`; `theta_first_rejected`; `eval_exceptions`. `null` when: no step leaves the point
(`alpha_*`, `line_search_trials`, `regularization_primal*`, `inertia_corrections`, `barrier_objective`,
`merit`); proximal mode off or not factorized (`regularization_dual`, `proximal_primal`); a non-finite
direction (`inertia_corrections`); the backend does not report it (`perturbed_pivots`, Accelerate
**UNOBSERVED**); no rejection recorded (`first_rejected_trial`, `theta_first_rejected`).

`sqp` block (`SqpIterationRow`): `depth`; `mode` (`ipqp`/`walk`/`ssn`); `verdict`; `trust_radius`;
`violation_l1`; `dual_regularization`; `qp_solved`; `qp_status`; `qp_minor_iterations`;
`qp_factorizations`; `tr_binding`; `elastic`; `elastic_clamped`; `restoration_seed_used`;
`ipqp_least_infeasible_primal`; `ipqp_farkas_corroborated`; `active_set_size` (= `active_rows +
active_lower_sides + active_upper_sides`); `active_set_delta`; `weak_active_rows`; `near_active_rows`;
`active_rows`; `active_lower_sides`; `active_upper_sides`. `null` when `!qp_solved`: `verdict`,
`dual_regularization`, `qp_status`, `qp_minor_iterations`, `qp_factorizations`. The two `ipqp_*` keys are
`null` unless the row is an IPQP escape row, which is observable as `ipqp_least_infeasible_primal != 0 ||
ipqp_farkas_corroborated` (the engine writes zero/false on every other row).

Join rules (v0 section 7 carries over): QP-tier lines belong to the next `iter` at the same depth; a
restoration-requesting row follows its nested solve(s); an IPM row joins its phase events on
(`ipm.phase`, `iter`).

### 3.2 `solve.begin`

The declared problem's census, verbatim `ProblemStatistics`, then the options in force:
`variables`, `free_variables`, `lower_bounded_variables`, `upper_bounded_variables`, `boxed_variables`,
`fixed_variables`, `constraints`, `equality_rows`, `lower_bounded_rows`, `upper_bounded_rows`,
`range_rows`, `free_rows`, `absent_bounds`, `optimality_tol`, `feasibility_tol`, `max_iterations`,
`hven_version` (string). A nested (depth 1) begin repeats the parent's census.

`ipm`: `n_reduced`, `me`, `mi`, `internal_fixed_rows`, `phases`, `max_acceptable_iterations`,
`initial_mu`, `obj_scale`, `acceptable_stationarity_tol`, `acceptable_complementarity_tol`,
`acceptable_equality_tol`, `acceptable_inequality_tol`, `inertia_mode`, `restoration_mode`, `kkt_dim`,
`kkt_nnz`.
`sqp`: `me`, `mi`, `qp_mode`, `ws_algebra`, `hessian_mode` (`exact`/`lbfgs_damped`), `row_scaling`
(bool), `column_scaling` (`off`/`always`/`auto`).

### 3.3 `solve.end`

`status`; `stop_reason`; `message`; `iterations`; `returned_best_iterate`; `infeasibility_certified`; `f`;
`diagnostics` = `{stationarity, feasibility, complementarity}` (the DECLARED set, `SolveResult`);
`convergence` = the engine measures, byte-equal to the result struct's `convergence` (IPM:
`stationarity`, `complementarity`, `equality`, `inequality`, `feasibility`; SQP: `stationarity`,
`feasibility`, `complementarity`, `kkt_residual`); `work` = `{objective_evals, gradient_evals,
constraint_evals, jacobian_evals, hessian_evals, factorizations}`; `counters` (the engine counter tree,
generated from `HVEN_IPM_COUNTERS_FIELDS` / `HVEN_SQP_COUNTERS_FIELDS`; v0 section 5 describes the SQP
tree; `recovery_depth_histogram` is a 5-integer array); `wall_s`.

`ipm`: `total_time_s`, `pre_time_s`, `func_time_s`, `kkt_time_s`, `print_time_s`, `solver_init_time_s`,
`misc_time_s`, `phases_ran`, `kkt_analyses_total`, `factor_memory`, `factor_flops` (`null` when never
analysed).
`sqp`: `scaling_active`, `obj_scale`, `row_scale_min`, `row_scale_max`, `scaled_kkt_residual`,
`columns_active`, `col_spread`, `start_level_used`.

The record is written by the public handle after it has finished the result, so it carries the
handle-derived fields. A depth-1 SQP end is written by the sub-driver and has `stop_reason`, `message`,
`diagnostics` and `work` all `null`.

### 3.4 `message`

Keys: `kind`, `severity` (`notice`/`warning`), `phase`, `iter`, `a`, `b`, `k`, `s`, `p`, `n`, `z`,
`expected_p`, `expected_n`; a slot a kind does not use is `null`.

| `kind` | severity | payload |
|---|---|---|
| `solver_initialized` | notice | none on the wire (`a` is `null`; the console prints the milliseconds from the event) |
| `rank_deficiency` | warning | `phase`, `iter` |
| `factorization_hard_error` | warning | `phase`, `iter`, `k` = the backend's info code |
| `inertia_exhausted` | warning | `phase`, `iter`, `k` attempts, `p`, `n`, `z`, `expected_p`, `expected_n` |
| `feasibility_stall` | warning | `phase`, `iter`, `a` = theta now, `b` = theta at the last restoration entry |
| `phase_diverged` | warning | `phase` |
| `interrupt_skipping_phases` | notice | `phase` |
| `stop_requested_skipping_phases` | notice | `phase` |
| `restoration_exited` | warning unless `s == resumed` | `s`, `a` = violation at exit, `b` = threshold judged against; TR-A emits `s = locally_infeasible` on the IPM |
| `stop_at_iteration` | notice | `s` = `callback`/`token`, `phase`, `iter` |
| `restoration_entered`, `restoration_refused`, `subproblem_failure`, `elastic_exhausted`, `inconsistent_equalities`, `start_point_clamped`, `scaled_convergence_refused` | per spec 2.7 | enumerated and given console templates; **emitted from TR-B** |

### 3.5 Unchanged records

`ipm.phase.begin`/`ipm.phase.end` lose their `label` key (derivable from `entry`). `ipm.phase.exit` keeps
its v0 keys, including `prim_obj`, `kkt_inf`, `barr_inf`, `econ_inf`, `icon_inf` **at engine scale**
(v0's names; the `iter` record's `stationarity`/`complementarity` are the caller-scale values of the same
residuals). The QP-tier records are v0's.

## 4. Identities a v1 stream satisfies (asserted in tests)

1. Every record counted in `iterations` has one `iter` line and one callback event. At depth 0, `iter`
   lines == `iterations` on both engines. A count, not a range: the IPM `iter` index is not contiguous.
   The non-finite evaluation abort is the known hole (M7-19, owned by its own commit).
2. When `returned_best_iterate == false` and scaling is inactive, the final depth-0 `iter` core residuals
   equal `solve.end.convergence`'s.
3. `factorizations` on the last row <= `solve.end.work.factorizations`.
4. The message identities of spec 2.7, and v0's QP-tier identities.
5. One `IterationEvent` per depth-0 `iter` line. SQP: event extras and row agree row for row. IPM, one-row
   lag: the event for point k+1 carries what row k's step fields hold, with one scaling caveat: the
   callback's `alpha_primal`/`alpha_dual` are the fraction-to-boundary maxima, while the row's are those
   times `alpha_scale`.

## 5. Reader contract

A reader rejects an unknown `v`; ignores unknown keys and unknown `ev`; does not rely on key order;
tolerates one malformed line at a truncation point; keeps a default arm on any enum switch.

## 6. Freeze table

Allowed within v1: a new record, a new trailing key in a record or block, a new enum value. Each is
declared in this table and its golden lines re-derived. Anything else (rename, removal, retype, change of
meaning) is v2.

| change | date | record | note |
|---|---|---|---|
| v1 | M8 TR-A | all | initial freeze |

## 7. v0 to v1 map

| v0 | v1 |
|---|---|
| `sqp.solve.begin`, `ipm.solve.begin` | `solve.begin`; `vars_free`/`vars_lower_only`/`vars_upper_only`/`vars_ranged`/`vars_fixed` -> `free_variables`/`lower_bounded_variables`/`upper_bounded_variables`/`boxed_variables`/`fixed_variables`; `n` -> `variables`; `me`/`mi` into the engine block; `kkt_tol`/`econ_tol`/`icon_tol`/`bar_tol`/`max_iters` -> `optimality_tol`/`feasibility_tol`/`max_iterations`; `init_mu` -> `ipm.initial_mu`; `max_acc_iters` -> `ipm.max_acceptable_iterations`; `acc_kkt_tol`/`acc_bar_tol`/`acc_econ_tol`/`acc_icon_tol` -> `ipm.acceptable_stationarity_tol`/`_complementarity_tol`/`_equality_tol`/`_inequality_tol` (the G7 names; the spec's map predates G7 and is aligned here); `wide_console` dropped |
| `sqp.major` | `iter` + `sqp`; `trial`, `kkt_residual` dropped; `mu` -> `sqp.dual_regularization`; `qp_minor_iters` -> `sqp.qp_minor_iterations`; `tr_radius` -> `sqp.trust_radius`; `soc_applied` -> `soc`; `watchdog_restored` -> `watchdog`; `elastic_applied` -> `sqp.elastic`; `elastic_rho0_ceiling_hit` -> `sqp.elastic_clamped`; `major` -> `iter`; `step_norm` -> `step` |
| `ipm.iter` | `iter` + `ipm`; `prim_obj` -> `f`/`f_recovery`; `kkt_inf` -> `stationarity`; `barr_inf` -> `complementarity`; `econ_inf`/`icon_inf` -> `ipm.feasibility_eq`/`_ineq`; `ls_iters` -> `line_search_trials`; `alpha_p`/`alpha_d`/`alpha_t` -> `alpha_primal`/`alpha_dual`/`alpha_scale`; `h_pert` -> `regularization_primal`; `h_pert_cum` -> `regularization_primal_total`; `h_facs` -> `inertia_corrections`; `prox_reg_primal` -> `proximal_primal`; `prox_reg_dual` -> `regularization_dual`; `p_pivots` -> `perturbed_pivots`; `barr_obj` -> `barrier_objective`; `merit_val` -> `merit`; `pen_par1`/`pen_par2` -> `merit_penalty_1`/`_2`; `max_e_mult`/`max_i_mult` -> `max_multiplier_eq`/`_ineq`; `first_rejection_iter` -> `first_rejected_trial`; `theta_at_first_rejection` -> `theta_first_rejected` |
| `ipm.restoration_exit_row` | removed; `message{kind: restoration_exited, s: locally_infeasible}` |
| `ipm.message` | `message`; `restoration_locally_infeasible` folds into `restoration_exited`; `interrupt_at_iteration` and `stop_requested_at_iteration` become `stop_at_iteration{s: callback/token}`; `solver_initialized.a` is `null` |
| `sqp.solve.end`, `ipm.solve.end` | `solve.end`; `majors`/`iters` -> `iterations`; scaling keys -> `sqp` block; timing -> `ipm` block |
| `ipm.phase.begin`/`.end` | `label` dropped |
| QP-tier events | unchanged after the envelope |

## 8. Human trace

`ConsoleTraceSink` renders the same events as text: header, legend, fixed-width `iter` lines, message lines,
footer. The formats, the tier/verdict/code letters and the verbosity ladder are in spec section 3 and are
pinned by `tests/drivers/test_console_sink.cpp`. Rungs: `kWarnings` warnings and the footer's `exit` line;
`kSummary` header, IPM phase lines (only on multi-phase solves) and the full footer; `kIterations` legend,
column headers, rows (restoration rows included) and notices; `kDetail` one condensed `.` line per QP-tier
event plus the IPM KKT-analysis and phase-exit lines. `Format{color, legend, timing}`: colour applies only
to the warning mark and the `exit` line.
