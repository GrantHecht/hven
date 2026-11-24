# hven machine-trace schema v1

The JSON-lines event stream `hven::JsonLinesTraceSink` writes
(`include/hven/trace/json_lines_trace_sink.h`, `src/trace/json_lines_trace_sink.cpp`). **This document is the
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
| `sid` | int | per-sink ordinal of the enclosing depth-0 `solve.begin`, from 1 (0 before any begin). A solve that throws writes no `solve.end`; its `sid` has a begin and no end, and the next solve takes `sid + 1`. A reader treats that `sid` as an aborted solve, not a malformed stream. The engine tells the sink about the exit (`TraceSink::on_solve_abort()`, non-emitting, once per solve level that began), so the next solve on the same sink starts at `depth` 0 with no recovery call; there is no `reset_nesting`. |
| `depth` | int | enclosing SQP solves; 0 on the interior-point engine, whose restoration runs in place |
| `eng` | string `ipm`/`sqp` | which engine wrote the line |

## 2. Types, absence, text

v0 section 2 carries over: `{:.17g}` doubles; `"nan"`/`"inf"`/`"-inf"` strings for a non-finite
MEASUREMENT; int64 integers; lower-snake enum strings; RFC 8259 escaping; no `"unknown"`. Rules v1
tightens:

1. **A field the emitting site did not compute is `null`.** Never a default, sentinel or projection.
2. **No vector-valued field.** Vectors belong to the callback (`IterationPayload`).
3. **Units.** A double is on the caller's objective scale unless its row says "engine scale". `_s` keys are
   wall-clock. Only `solve.end`, `ipm.phase.exit`, `ipm.kkt_analysis` and the SQP `iter` block's `t_s`
   carry them, so a deterministic solve's stream is byte-reproducible once those keys are masked.
   `solver_initialized` carries no milliseconds on the wire; a byte comparison drops that one kind.

## 3. Records

| `ev` | eng | one per |
|---|---|---|
| `solve.begin` | both | accepted public call (refused arguments write nothing) |
| `iter` | both | iteration row |
| `message` | both | diagnostic occurrence |
| `solve.end` | both | normal return (an exception writes none; the sink is unwound by `on_solve_abort()`) |
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
| `stationarity` | double | `kkt_inf` (the scaled problem's units: exactly what `converge_check` compares with `optimality_tol`) | `SqpKkt::stationarity` as measured on the engine's scaled problem (what the convergence test compares with `optimality_tol`) | never (`"nan"` if non-finite) |
| `feasibility` | double | `max(econ_inf, icon_inf)` (each compared with `feasibility_tol`) | `SqpKkt::feasibility`, engine units (compared with `feasibility_tol`) | never |
| `complementarity` | double | `barr_inf` (compared with the effective complementarity tolerance) | `SqpKkt::complementarity`, engine units (compared with `optimality_tol`) | never |
| `step` | double | inf-norm of the primal step leaving the point | `SqpIterate::step_norm` | no step leaves the point (IPM exit rows, non-finite direction; SQP `!qp_solved` or QP not optimal) |
| `accepted` | bool | `IpmIterate::accepted` | `verdict` is an accept | no step |
| `soc` | bool | SOC won this iteration's step (`resolved_depth == kRecoveryDepthSoc`) | `soc_applied` | never |
| `watchdog` | bool | the watchdog **armed** this iteration | the watchdog **restored** this pass | never; the meanings differ by engine |
| `rolled_back` | bool | a non-finite step rollback happened at this iteration | `rolled_back` | never |
| `factorizations` | int | the call's running KKT factorization count at the row; each stepped row's count exceeds the previous row's by exactly `1 + inertia_corrections` | the (sub-)solve's running `counters.factorizations` (the SOLVE'S TOTAL so far, not the row's own work: the `sqp` block's `factorizations_delta` is the row's) | never |
| `stationarity_caller` | double | `kkt_inf / obj_scale`: the caller's objective scale | `SqpKkt::stationarity / obj_scale` when row scaling is active, else as `stationarity` | never |
| `feasibility_caller` | double | `max(econ_inf, icon_inf)` (the IPM scales no rows: equal to `feasibility`) | feasibility of the caller's rows and box (row/column factors removed); equals `feasibility` on an unscaled solve | never |
| `complementarity_caller` | double | `barr_inf / obj_scale` | `SqpKkt::complementarity / obj_scale` when row scaling is active, else as `complementarity` | never |

**`stationarity`, `feasibility`, `complementarity` are the quantities the termination test compares with the
tolerances** (TR-S1): the scaled problem's own values, so a reader sees convergence against `optimality_tol` /
`feasibility_tol` (section 3.2) directly in the stream. On an unscaled solve they equal the `_caller` keys. The `_caller`
keys are the same measures mapped back to the caller's units (the values `IterationRecord`'s unprefixed fields, `history`
and the callback carry); under scaling they are what the user's own check of the returned point would read, and can be
orders of magnitude from the tested value (an objective scaled up by 7e4 reads a tested stationarity 7e4 times the
caller's). `solve.end.convergence` is the engine measure of the returned point.

`ipm` block (`IpmIterationRow`): `phase`; `mu` (engine scale: the barrier parameter the step leaving this point was formed at, i.e. after the governor's update for that iteration; the starting value is `ipm.initial_mu` on `solve.begin` and the callback event for point 0. Row 0 can therefore sit at `barrier_min` (HS071: 1e-12, the adaptive governor's first update clamps there) while row 1 reads about 5.6e-4: both are the values the steps used, and the event for point k+1 carries row k's); `alpha_primal`, `alpha_dual` (the effective
lengths, `alpha_scale * ` the fraction-to-boundary maxima); `alpha_scale`; `line_search_trials`;
`regularization_primal`; `regularization_primal_total`; `regularization_dual`; `proximal_primal`;
`inertia_corrections`; `perturbed_pivots`; `barrier_objective`; `feasibility_eq`; `feasibility_ineq`;
`merit`; `merit_penalty_1`; `merit_penalty_2`; `max_multiplier_eq`; `max_multiplier_ineq` (both on the caller's objective scale, like every price);
`first_rejected_trial`; `theta_first_rejected`; `eval_exceptions`. `null` when: no step leaves the point
(`alpha_*`, `line_search_trials`, `regularization_primal*`, `inertia_corrections`, `barrier_objective`,
`merit`; `barrier_objective` is gated on a step leaving the point and nothing else); proximal mode off or not factorized (`regularization_dual`, `proximal_primal`); a non-finite
direction (`inertia_corrections`); the backend does not report it (`perturbed_pivots`, Accelerate
**UNOBSERVED**); no rejection recorded (`first_rejected_trial`, `theta_first_rejected`).

`sqp` block (`SqpIterationRow`): `depth`; `mode` (`ipqp`/`walk`/`ssn`); `verdict`; `trust_radius`;
`violation_l1`; `regularization_dual`; `qp_solved`; `qp_status`; `qp_minor_iterations`;
`qp_factorizations`; `tr_binding`; `elastic`; `elastic_clamped`; `restoration_seed_used`;
`ipqp_least_infeasible_primal`; `ipqp_farkas_corroborated`; `active_set_size` (= `active_rows +
active_lower_sides + active_upper_sides`); `active_set_delta`; `weak_active_rows`; `near_active_rows`;
`active_rows`; `active_lower_sides`; `active_upper_sides`; `switching_ratio` (GL-0); `ladder` (M8 I17); then the I19-1
keys, in order: `cap_binding`; `pred_df`; `f_trial`; `h_trial`; `soc_f`; `soc_h`; `h_lin`; `qp_minor_iterations_total`;
`qp_factorizations_total`; `factorizations_delta`; `t_s`; then `trust_region_growth_refusal_reasons` (M8 I19-2a, the last key). `null` when `!qp_solved`: `verdict`,
`regularization_dual`, `qp_status`, `qp_minor_iterations`, `qp_factorizations`. `switching_ratio` is
`pred_df / (0.999 * h^2)` at the row's point (KLV Eq. (10): `>= 1` selects an f-type trial), `null` on a row whose
subproblem did not solve to `kOptimal` (so a judged `qp_failure_step` row is `null`), and `"inf"`/`"-inf"`/`"nan"`
at `h == 0`. It is computed in ENGINE scale (the scaled `pred_df` and `h`), so on a solve with row/objective scaling
active it does not reproduce from the caller-unit `pred_df` and `violation_l1` of the same row. The two `ipqp_*` keys are
`null` unless the row is an IPQP escape row, which is observable as `ipqp_least_infeasible_primal != 0 ||
ipqp_farkas_corroborated` (the engine writes zero/false on every other row).

`ladder` is `null` unless the row's major ran an elastic ladder; then an object with `entry` (`verified_infeasible`
-- the main QP was verified infeasible in box and trust region; `working_set_termination` -- its working set ended
numerically at a violated iterate; `refused_certificate` -- its infeasibility certificate was refused;
`ipqp_fallback` -- the certified fallback's rung A), `main_status` (the main QP's status, before the ladder -- not necessarily the row's `qp_status`, see below;
on `ipqp_fallback` the interior-point tier's status at its escape),
`rung_status` (the final rung's), `rho` (the final rung's penalty), `rungs` (climbed above the first), `closed`,
`reduced`, `promises_f`, `slack_l1` (0 unless the final rung is optimal), `exhausted`, then (I19-1) `rho_0` (the
surviving ladder's first penalty: after a certified fallback's floor retry that is the floor, not the first
attempt's value), `minor_iterations` and `factorizations` (the ladder's own QP work over every rung it
solved; a certified fallback's floor retry includes its first ladder). The same object is the `message`
`ladder` slot. `main_status` is the main QP's status BEFORE the ladder, which is not necessarily the row's
`qp_status`: on an exhausted ladder the row's `qp_status` is the MAIN QP's (equal to `main_status`); on a
usable ladder it is the status of the rung that supplied the step (normally `optimal`). So `qp_status:
"optimal"` with `ladder.main_status: "infeasible"` is a main QP that was infeasible and an elastic rung that
produced the step.

**Binding flags (I19-1).** `tr_binding` is true when the trust RADIUS held at least one variable of the
step; `cap_binding` is true when a `caller_step_limit` pin did. Before I19-1 `tr_binding` was the merged flag
(radius or cap); that merged value, `tr_binding || cap_binding`, is still the one the radius-growth rule, the
switching strategy and the zero-step test read, unchanged, but it is no longer exported under one key. The
active-side census (`active_lower_sides`, `active_upper_sides`, `active_set_size`) counts real bound activity
and leaves out both kinds of pin, so `tr_binding || cap_binding` is what separates a pinned variable from a
bound-active one there. The console code `b` keeps the merged meaning. Both flags are on `SqpIterationInfo` in the iteration callback.

**Row work (I19-1).** The row's `qp_minor_iterations` and `qp_factorizations` are the QP that SUPPLIED the step:
on a ladder row the rung that supplied the step (the last rung, or, under `elastic_last_optimal_rung`, the
retained earlier optimal rung), and on an exhausted ladder the final rung's counts (see `qp_status` above). The
total work the row cost is `qp_minor_iterations_total` / `qp_factorizations_total`: the QP minors and
factorizations charged since the previous row of the same solve and depth (main QP, a cap retry, every ladder
rung, the SOC re-solve), excluding the work a restoration phase folded into the solve (that phase writes its own
depth-1 rows). `factorizations_delta` is the change in the core `factorizations` (the solve's running total)
since the previous row, restoration work included; the two agree except across a restoration phase. The
first row of a solve counts from zero, so it carries whatever the solve charged before its first row.

**Trial diagnostics (I19-1).** All in the caller's units (objective divided by the objective scale; violations
in the caller's rows), all report-only: no decision reads them. `pred_df` is the model's predicted objective
decrease for the QP step `p` (the quantity the switching test and the trust-region growth rule use; the
funnel's SOC test reuses it for the SOC point); `f_trial` and `h_trial` are the objective and l1 constraint
violation at `x + p`, and `pred_df`, `f_trial`, `h_trial` are present exactly where `switching_ratio` is (the
row's QP solved to `optimal`). A trial the model could not evaluate has a NaN objective, written as the string `"nan"` (never `null`; `null` means
the quantity was not computed): `f_trial` and `soc_f` read `"nan"` and `h_trial` / `soc_h` reflect the unevaluable
bundle's violation (`"inf"` or `"nan"`). `soc_f` and `soc_h` are the same two at the second-order-correction point and are
`null` unless a SOC re-solve solved and its point was evaluated (the row's `soc` flag says the correction won the
step; `soc_*` can be present on a row where it did not). `h_lin` is the l1 violation the QP step leaves in the
QP's own linearized rows (`sum |Ae p - be| + sum max(0, Ai p - bi)`), so `h_old - h_lin` is the violation the
model promised to remove; `null` when the step is not finite or the QP's rows are not row for row the model's
(an equality-basis reduction). Bound violations are not in `h_*`.

**Growth refusal (M8 I19-2a).** `trust_region_growth_refusal_reasons` is an integer bit set naming the
`SqpAdvancedOptions::trust_region_*` levers that refused the row's radius growth: 1 the violation ratio
(`trust_region_growth = kRatioByStepType` on an h-type step), 2 an acceptance through SOC
(`trust_region_growth_after_soc`), 4 the rejection memory, 8 the ratio ceiling, 16 a step held only by cap pins
(`trust_region_growth_on_step_limit`), 32 the funnel headroom. A lever is consulted only when the growth rule granted
the growth (or `kRatioByStepType` granted it on the violation ratio), so a growth the rule itself refused carries 0. It
is 0 on every row that is not an accepted step and at the default levers, and never `null`.

**Elastic activation.** A row's major ran an elastic ladder if and only if its `ladder` block is non-null (the
`elastic` flag marks the narrower case that an elastic solve supplied the step).

**Clock.** `t_s` is seconds since the solve's entry on a monotonic clock, informational: it is wall-clock, so
mask it when comparing streams byte for byte. A depth-1 row counts from the nested solve's own entry.

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

`stop_reason` is `to_string(StopReason)`'s spelling. M8 RF-1 (declared): `subproblem_iteration_limit` is new
(SQP, beside `status` `stalled`: the last QP stopped at its minor-iteration cap), as is `trust_region_floor`
(SQP, beside `stalled`: the radius reached its floor at a feasible point whose rejected step promised less than
the objective's rounding floor), as is `restoration_work_limit` (SQP, beside `stalled`: the restoration phase
spent its own factorization budget), and an SQP failure exit whose
returned point passes `termination`'s acceptable tier is written `status` `acceptable`, `stop_reason` `none`,
with the exit it came from named at the end of `message` (`exit restoration_refused`, ...).

The record is written by the public handle after it has finished the result, so it carries the
handle-derived fields. A depth-1 SQP end is written by the sub-driver and has `stop_reason`, `message`,
`diagnostics` and `work` all `null`.

### 3.4 `message`

Keys: `kind`, `severity` (`notice`/`warning`), `phase`, `iter`, `a`, `b`, `k`, `s`, `p`, `n`, `z`,
`expected_p`, `expected_n`, `ladder`; a slot a kind does not use is `null`. `ladder` is the `sqp` block's `ladder`
object (section 3.1), set on `elastic_exhausted` only.

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
| `restoration_entered` | notice | both engines. `a` = violation at entry (IPM theta, SQP `h`), `iter` = the requesting row. Lines == `counters.restoration_entries` on the IPM; on the SQP the counter counts completed excursions, so lines == `restoration_entries` + the `restoration_exited{s: non_finite}` lines |
| `restoration_exited` | warning unless `s == resumed` | both engines; one per entered. `s` = `resumed`/`locally_infeasible`/`failed`/`non_finite`/`interrupted`; `a` = violation at exit (absent for `non_finite` on the SQP), `b` = the threshold judged against (IPM: 100x `feasibility_tol`; SQP: `feasibility_tol`) |
| `restoration_refused` | warning | SQP. `s` = `nested`/`entry_rule`/`budget`; written at both depths (a refusal inside a restoration sub-solve is `nested`) |
| `subproblem_failure` | notice while the solve continues, warning when it ends it | SQP, both depths. `s` = `iteration_limit`/`numerical_error`/`non_finite`; `k` = the driver's run of consecutive failures once the exit was handled. Lines == `qp_maxiter_exits + qp_numerical_exits + nonfinite_subproblems` (the spec's `- nonfinite_box_projections` does not apply: the projection precedes the guard and is not in `nonfinite_subproblems`) |
| `elastic_exhausted` | warning | SQP. `k` = rungs climbed above the first; `ladder` = the exhausted ladder's block |
| `inconsistent_equalities` | warning | SQP, depth 0, at most once |
| `start_point_clamped` | notice | SQP, depth 0, once, first message; `k` = `start_point_clamps` |
| `scaled_convergence_refused` | notice | SQP. `iter`; lines == `scaled_convergence_refusals` |
| `stop_at_iteration` | notice | both engines, depth 0. `s` = `callback`/`token`, `iter` = the row the stop landed on; at most one, and one iff `stop_reason` is `callback_stop` or `stop_requested` |

### 3.5 Unchanged records

`ipm.phase.begin`/`ipm.phase.end` lose their `label` key (derivable from `entry`). `ipm.phase.exit` keeps
its v0 keys `prim_obj`, `econ_inf`, `icon_inf` and renames `kkt_inf` -> `stationarity`, `barr_inf` ->
`complementarity` (the G7 names, docs/migration-0.2.md), all **at engine scale**: the same residuals as the
`iter` record's `stationarity`/`complementarity` (since TR-S1 those are engine scale too; the `_caller` keys are divided by the objective scale). The QP-tier records are v0's.

## 4. Identities a v1 stream satisfies (asserted in tests)

1. At depth 0, `history.size()` == the number of `iter` lines == the number of callback events, on both
   engines: `IpmResult::history` and `SqpResult::history` are those rows (`IterationRecord`), row for row,
   always built, with or without a sink. IPM: every record counted in `iterations` has one line and one event,
   so `history.size()` == `iterations`. SQP: row 0 is the start point (no step reached it), and a stop or abort
   door pushes the stopped-at row without a step, so `history.size()` is read off `history`, never derived from
   `iterations`. A count, not a range: the IPM `iter` index is not contiguous. The IPM non-finite evaluation
   abort used to be the one hole (M7-19, closed in TR-B): it now writes its row and fires its event.
2. When `returned_best_iterate == false` and scaling is inactive, the final depth-0 `iter` core residuals
   (`stationarity`, `feasibility`, `complementarity`, and equally the `_caller` keys) equal `solve.end.convergence`'s.
   A row whose `stationarity <= optimality_tol`, `feasibility <= feasibility_tol` and `complementarity <= tolerance`
   is the row a converged solve stops on (the test is on these printed values).
3. `factorizations` on the last row <= `solve.end.work.factorizations`.
4. The message identities of spec 2.7, and v0's QP-tier identities.
5. One `IterationEvent` per depth-0 `iter` line. SQP: event extras and row agree row for row. IPM, one-row
   lag: the event for point k+1 carries what row k's step fields hold, with one scaling caveat: the
   callback's `alpha_primal`/`alpha_dual` are the fraction-to-boundary maxima, while the row's are those
   times `alpha_scale` (the row's alpha is `alpha_scale` x the callback's pre-line-search maximum).

## 5. Reader contract

A reader rejects an unknown `v`; ignores unknown keys and unknown `ev`; does not rely on key order;
tolerates one malformed line at a truncation point; keeps a default arm on any enum switch.

Golden streams for a reader's self-test (byte-exact, held by `TraceV1Fixtures.*`):
`tests/trace/golden/trace_v1/{ipm_complete,sqp_complete,ipm_aborted}.jsonl`. `dump_counters` (the text twin of
the `solve.end` `counters` object) is documented in `docs/migration-0.2.md`, section TR-E.

## 6. Freeze table

Allowed within v1: a new record, a new trailing key in a record or block, a new enum value. Each is
declared in this table and its golden lines re-derived. Anything else (rename, removal, retype, change of
meaning) is v2.

| change | date | record | note |
|---|---|---|---|
| v1 | M8 TR-A | all | initial freeze; `ipm.phase.exit` carries `stationarity`/`complementarity` (engine scale) in place of `kkt_inf`/`barr_inf` |
| none | M8 TR-B | `message` | the SQP kinds and the IPM `restoration_entered` and the other `restoration_exited` values are emitted; every kind, key and enum value was already in the v1 freeze, so no schema change |
| new trailing key | M8 GL-0 | `iter`, `sqp` block | `switching_ratio` (double or `null`), after `active_upper_sides`; the `sqp` block's arity pin moves 23 -> 24. `solve.end.counters` (SQP) gains eight trailing keys after `payload_ignored`: `reject_funnel_ceiling`, `reject_f_armijo`, `reject_h_funnel`, `reject_qp_failed`, `reject_other`, `accepted_f_type`, `accepted_h_type`, `rejection_run_histogram` (a 5-integer array); the three golden lines are re-derived |
| new trailing key | M8 I17 | `iter` `sqp` block; `message` | `ladder` (object or `null`), after `switching_ratio`; the `sqp` block's arity pin moves 24 -> 25. `message` gains the same `ladder` key after `expected_n`, `null` on every kind but `elastic_exhausted` (the `MessageTraceEvent` arity pin moves 14 -> 15); the exhausted row's `qp_status` is the main QP's. The golden `sqp_complete.jsonl` and the affected unit lines are re-derived |
| meaning change + new trailing keys (pre-tag) | M8 TR-S1 | `iter` | the core keys `stationarity`, `feasibility`, `complementarity` carry the scaled problem's measures, the quantities the termination tests compare with the tolerances (IPM: `kkt_inf`, `max(econ_inf, icon_inf)`, `barr_inf`, previously divided by the objective scale; SQP: the engine-unit `SqpKkt` measures, previously mapped to caller units). New trailing core keys after `factorizations`: `stationarity_caller`, `feasibility_caller`, `complementarity_caller` carry the former values. Equal to the former values on an unscaled solve. The v1 freeze has not been tagged (same ground as the TR-D rider), so this is a pre-tag correction, not a v2. The golden `ipm_complete.jsonl`, `sqp_complete.jsonl` and `ipm_aborted.jsonl` lines are re-derived |
| meaning change + new trailing keys (pre-tag) | M8 I19-1 | `iter` `sqp` block; `message` | `tr_binding` now means the trust RADIUS held the step (a `caller_step_limit` pin no longer counts): the old merged value is `tr_binding \|\| cap_binding`. The decision input is unchanged (an internal merged flag). New trailing `sqp`-block keys after `ladder`: `cap_binding`, `pred_df`, `f_trial`, `h_trial`, `soc_f`, `soc_h`, `h_lin`, `qp_minor_iterations_total`, `qp_factorizations_total`, `factorizations_delta`, `t_s` (wall-clock: mask it); the `sqp` block's arity pin moves 25 -> 36. The `ladder` object gains `rho_0`, `minor_iterations`, `factorizations` after `exhausted` (also in `message`). The v1 freeze has not been tagged (same ground as TR-S1), so a pre-tag correction, not a v2. The golden `sqp_complete.jsonl` and the affected unit lines are re-derived |
| new trailing keys | M8 I19-2a | `iter` `sqp` block; `solve.end` | `trust_region_growth_refusal_reasons` (integer bit set, never `null`), after `t_s`; the `sqp` block's arity pin moves 36 -> 37. `solve.end.counters` (SQP) gains `trust_region_growths`, `trust_region_growth_refusals`, `trust_region_shrinks_to_step`, `trust_region_grown_step_rejections`, `trust_region_unbound_rejections`, `step_limit_binding_majors` after `soc_qp_iteration_limits` (before the `ssn` object). Additive; the golden `sqp_complete.jsonl` and the affected unit lines are re-derived |
| none | M8 TR-D rider | `iter` | the `sqp` block key `dual_regularization` is spelled `regularization_dual`, the `ipm` block's key for the same quantity (one name for both engines, as the G5 callback block already had); the v1 freeze has not been tagged, so a spelling in the freeze, not a v2 |

## 7. v0 to v1 map

| v0 | v1 |
|---|---|
| `sqp.solve.begin`, `ipm.solve.begin` | `solve.begin`; `vars_free`/`vars_lower_only`/`vars_upper_only`/`vars_ranged`/`vars_fixed` -> `free_variables`/`lower_bounded_variables`/`upper_bounded_variables`/`boxed_variables`/`fixed_variables`; `n` -> `variables`; `me`/`mi` into the engine block; `kkt_tol`/`econ_tol`/`icon_tol`/`bar_tol`/`max_iters` -> `optimality_tol`/`feasibility_tol`/`max_iterations`; `init_mu` -> `ipm.initial_mu`; `max_acc_iters` -> `ipm.max_acceptable_iterations`; `acc_kkt_tol`/`acc_bar_tol`/`acc_econ_tol`/`acc_icon_tol` -> `ipm.acceptable_stationarity_tol`/`_complementarity_tol`/`_equality_tol`/`_inequality_tol` (the G7 names; the spec's map predates G7 and is aligned here); `wide_console` dropped |
| `sqp.major` | `iter` + `sqp`; `trial`, `kkt_residual` dropped; `mu` -> `sqp.regularization_dual`; `qp_minor_iters` -> `sqp.qp_minor_iterations`; `tr_radius` -> `sqp.trust_radius`; `soc_applied` -> `soc`; `watchdog_restored` -> `watchdog`; `elastic_applied` -> `sqp.elastic`; `elastic_rho0_ceiling_hit` -> `sqp.elastic_clamped`; `major` -> `iter`; `step_norm` -> `step` |
| `ipm.iter` | `iter` + `ipm`; `prim_obj` -> `f`/`f_recovery`; `kkt_inf` -> `stationarity`; `barr_inf` -> `complementarity`; `econ_inf`/`icon_inf` -> `ipm.feasibility_eq`/`_ineq`; `ls_iters` -> `line_search_trials`; `alpha_p`/`alpha_d`/`alpha_t` -> `alpha_primal`/`alpha_dual`/`alpha_scale`; `h_pert` -> `regularization_primal`; `h_pert_cum` -> `regularization_primal_total`; `h_facs` -> `inertia_corrections`; `prox_reg_primal` -> `proximal_primal`; `prox_reg_dual` -> `regularization_dual`; `p_pivots` -> `perturbed_pivots`; `barr_obj` -> `barrier_objective`; `merit_val` -> `merit`; `pen_par1`/`pen_par2` -> `merit_penalty_1`/`_2`; `max_e_mult`/`max_i_mult` -> `max_multiplier_eq`/`_ineq`; `first_rejection_iter` -> `first_rejected_trial`; `theta_at_first_rejection` -> `theta_first_rejected` |
| `ipm.restoration_exit_row` | removed; `message{kind: restoration_exited, s: locally_infeasible}` |
| `ipm.message` | `message`; `restoration_locally_infeasible` folds into `restoration_exited`; `interrupt_at_iteration` and `stop_requested_at_iteration` become `stop_at_iteration{s: callback/token}`; `solver_initialized.a` is `null` |
| `sqp.solve.end`, `ipm.solve.end` | `solve.end`; `majors`/`iters` -> `iterations`; scaling keys -> `sqp` block; timing -> `ipm` block |
| `ipm.phase.begin`/`.end` | `label` dropped |
| `ipm.phase.exit` | `kkt_inf` -> `stationarity`, `barr_inf` -> `complementarity` (G7 names; still engine scale) |
| QP-tier events | unchanged after the envelope |

## 8. Human trace

`ConsoleTraceSink` renders the same events as text: header, fixed-width `iter` lines, message lines,
footer. (There is no legend: the column reference is section 8.1; the printed `legend` block and `Format::legend` were removed in TR-S1.) The formats, the tier/verdict/code letters and the verbosity ladder are in spec section 3 and are
pinned by `tests/trace/test_console_sink.cpp`. One-solve/one-thread contract: a sink serves one solve at a time on one thread (a restoration sub-solve shares it sequentially); nothing is synchronized. Rulings (TR-A): the phase-line shapes follow spec 3.4's examples; the message folds `restoration_exited{s}` and `stop_at_iteration{s}` stand; the `Max iter` wording stands; the QP-tier `.` lines print at every depth at `kDetail`. Rungs: `kWarnings` warnings and the footer's `exit` line;
`kSummary` header, IPM phase lines (only on multi-phase solves) and the full footer; `kIterations` column headers,
 rows (restoration rows included) and notices; `kDetail` one condensed `.` line per QP-tier
event plus the IPM KKT-analysis and phase-exit lines. `Format{verbosity, color, timing}`: colour applies only
to the warning mark and the `exit` line.

### 8.1 Console column reference

The column header is reprinted at the first row, every 20 rows and after a message line. A `-` is a value that does not
exist at that row (no step leaves the point). Every `stat_scl feas_scl comp_scl` value is **the number the convergence
test compares with its tolerance**, in the scaled problem's units (`_scl`: the units the solver works in; on an unscaled
solve they are the caller's units). The tolerances are on the `tolerances` header line; a solve ends `Optimal` when
every column below that has a tolerance is at or under it (the IPM tests strictly under). Caller-unit values are the
`*_caller` keys of the JSON `iter` record, not console columns.

Common to both engines:

| column | meaning | tested against |
|---|---|---|
| `iter` | iteration index. IPM: restarts at 0 per phase, not contiguous. SQP: history index, restarts at 0 per restoration sub-solve | |
| `t` | tier that supplied the step. IPM: `B` barrier, `R` restoration. SQP: `I` IPQP, `S` SSN, `W` walk, `E` elastic, `R` restoration | |
| `objective` | the objective in the caller's units; on a restoration row, the restoration objective | |
| `stat_scl` | stationarity: the inf-norm of the Lagrangian gradient (IPM: z-form dual infeasibility incl. bound multipliers), scaled units | `optimality_tol` |
| `feas_scl` | feasibility: IPM `max(equality, inequality residual)`; SQP `max(\|cE\|, max(cI)+, bound violation)`, scaled units. A scaled SQP solve also requires the caller-unit feasibility (`feasibility_caller`) at or under `feasibility_tol` before it ends Optimal; a refusal is a `scaled_convergence_refused` notice | `feasibility_tol` |
| `comp_scl` | complementarity: IPM `max(s_i * lambda_i)` over slack and bound pairs; SQP `max_j \|lambda_j * cI_j(x)\|`, scaled units | IPM: `complementarity_tol` (default `optimality_tol`); SQP: `optimality_tol` |
| `step` | inf-norm of the primal step leaving this point (IPM `\|dx\|`, SQP `\|p\|`) | |
| `codes` | per-row flags; engine-specific, below | |

IPM only: `lgmu` log10 of the barrier parameter the step was formed at; `alpha_pr`, `alpha_du` effective primal and dual
step lengths; `ls` line-search backtracks; `lg(rg)` log10 of the total primal Hessian shift (`-` when none). Codes: `s`
second-order correction, `w` watchdog armed, `r` rollback, `k` backtrack, `i` inertia fix, `x` evaluation error, `m` reject.

SQP only: `radius` trust region; `h(l1)` l1 constraint violation (the globalization measure, not `feas_scl`); `qp_it`,
`qp_fa` QP minor iterations and factorizations; `v` verdict: `F` accept f-type, `H` accept h-type, `J` reject, `Q`
restore. Codes: `s` SOC, `w` watchdog restored, `r` rollback, `b` trust-region bound, `c` clamp, `d` restoration seed, `n`
QP not optimal.

Message lines: `!` marks a warning, `-` a notice. The JSON stream's `stationarity`, `feasibility`, `complementarity`
are the three tested columns; `solve.begin` carries `optimality_tol` and `feasibility_tol`.
