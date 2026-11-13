# M8 TR-1: trace v1 specification (machine schema, human trace, cost contract, consumers)

Status: DESIGN, for the owner's approval. No code has changed. Base: `origin/m8`. Line numbers below were
taken at `7e5ed61de` and re-verified against `origin/m8` at `569ec61fb` (the engine TUs and public headers
are unchanged between them). Inputs: the M8 brief §TR and §1 (6) (`docs/notes/2026-11-m8-brief.md:352-366`,
`:93-94`); `docs/trace-schema-v0.md`; the AP-5 interface design
(`docs/notes/2026-11-m8-ap5-interface-design.md`, owner-approved in full); the AP-1 items routed here
(`docs/notes/2026-11-m8-ap1-api-review.md:433-436`, `:699-724`); hven-bench at `eb292e5` (read-only).

**Cross-references.** `[AP5:x]` marks each place this spec builds on an item of the AP-5 page. AP-5 is
approved, so every marker is a plain dependency, not a conditional. Appendix A lists them.

SNOPT/IPOPT references are to their public output documentation only: the Ipopt documentation's output
section and Wächter & Biegler (Math. Prog. 106, 2006), and the SNOPT User's Guide's description of the
major iteration log. No source of either solver was read.

---

## 0. Findings that shape the design

| # | finding | evidence | consequence |
|---|---|---|---|
| F1 | **hven-bench reads no trace lines.** Its "trace schema v1" is the harness's own CSV schema, a name collision with this one. The lane reads result structs by hand: IPM flat fields, and per-pin hand-enumerated `SqpCounters` blocks. It notes "hven has no counters dump … That dump is the harness's standing request." | hven-bench `cmake/hven_pin.cmake:561-562`; `schema/v1.md:1`; `lanes/hven/hven_bench_cell.cpp:766-800`, `:810-1350` | v0 has no external consumer, so v1 replaces it with no dual-write window (§2.9). TR-3's real migration is counters and work, not trace parsing (§5). |
| F2 | **Three surfaces carry a per-iteration row, with three shapes.** `ipm.iter` = `IpmIterate` verbatim (engine-internal names). `sqp.major` = `SqpIterate` verbatim = `SqpResult::history`. `IterationEvent` = declared (caller-unit) diagnostics plus engine optionals. | `trace_writer.cpp:931-968`, `:1024-1069`; `sqp_solver.cpp:6931-6955`; `drivers/solve_result.h:390-451` | v1 defines one row, `IterationRecord`: a shared core plus one engine block with its own row types (`IpmIterationRow`, `SqpIterationRow`). The trace, `IpmResult::history` and optionally `SqpResult::history` all carry it (§2.4, Q1). |
| F3 | **v0 warts that only a version bump can fix.** (a) The IPM's four unfactorized exit rows write `barr_obj`, `merit_val`, `ls_iters` = 0 and all alphas = 1: fresh defaults, not measurements. (b) `qp_status`, `qp_minor_iters` and `qp_factorizations` are "meaningful iff `qp_solved`" yet written as values. (c) `mu` means the barrier parameter on `ipm.iter` and the dual regularization on `sqp.major`. (d) The phase `label` carries a trailing space. (e) `ipm.restoration_exit_row` repeats the `restoration_locally_infeasible` message's two numbers. (f) There are two bound censuses (`vars_*`) while `ProblemStatistics` already gives the declared census on both engines. (g) M7-19: a record counted in `iterations` has no trace line and no callback event: the non-finite evaluation abort (§2.8, §5.3). | v0 `:467-472`, `sqp_solver_types.h:183-187`, `:151`, v0 `:501`, v0 `:606`/`:491-494`, `model/problem.h:38-54`, `ipm_solver.cpp:2933-2934`, `:2973`, `:4885` | Each is fixed in v1 (§2.10 map). v0 §4.14 (`:474-477`) names the locally-infeasible door as the hole; that is stale (§5.3). |
| F4 | **The off-path is one pointer test per emit site, with exceptions.** Row emits build their event inside the guard. `IpmSolverImpl::emit_message` takes an event already built at the call site (`ipm_solver.cpp:2603-2607`), so message events are constructed even when no sink is attached. `SqpFallbackVerdictTraceEvent` is constructed unguarded at `sqp_solver.cpp:5424` and read by control flow. The console is not constructed at `kSilent`. | `ipm_solver.cpp:2603-2607`, `:3390-3393`, `:4523-4527`, `:6208-6226`; `sqp_solver.cpp:5424`, `:6943-6946`, `:3292-3308` | The cost contract (§4) turns "build inside the guard" into a rule with one named allowlist entry, makes `emit_message` take a builder, adds a source scan, and bans new unconditional bookkeeping beyond the five row fields it names (R-list in §4). |
| F5 | **The SQP emits no structured messages** (AP-5 M1). Its only console text is the table, `Status`, `Start Level` and `Scaling` lines. | `sqp_print.cpp:77-121`; AP-5 §1.5 | v1 gives both engines one `message` record and adds the SQP kinds (§2.7). |
| F6 | **Both engines' history rows describe the point and the step taken FROM it. The callback event differs on the IPM.** The IPM record is pushed after the step is computed (`iters.push_back(Citer)`, `ipm_solver.cpp:4493-4499`), emitted at `:4525`, and the step is applied at `:4663`. The SQP row is pushed after its subproblem. The IPM `IterationEvent` extras carry the step that REACHED the point (AP-5 §3.1). | `ipm_solver.cpp:4493-4499`, `:4525`, `:4663`; `sqp_solver_types.h:111-161`; AP-5 §3.1 | The row is one thing on both engines. The IPM callback runs one row behind it (identity 5). V1 pins both with a test before the legend freezes (§2.4, §2.11). |

---

## 1. What v1 changes, in one paragraph

Four records become **shared by both engines**, with the same keys and the same meanings: `solve.begin`,
`iter`, `message`, `solve.end`. Each carries a shared core and a nested engine block (`ipm` or `sqp`). The
remaining events are engine-specific by nature and keep their names: the IPM phase and KKT-analysis events,
and the SQP's QP-tier events. Field depth is balanced, not event count. The IPM gains a counters object on
`solve.end`, restoration entry/exit messages and row flags. The SQP gains structured messages and a
problem census. The envelope gains `eng` and `sid`. Absence is `null` throughout, including the v0 fields
that wrote defaults in place of a value.

---

## 2. Machine trace, schema v1

### 2.1 Envelope

Every line is one JSON object, `\n`-terminated. The first five keys, in this order:

| key | type | meaning |
|---|---|---|
| `v` | int | **1**. A reader rejects any other value (§2.9). |
| `ev` | string | the record name (§2.3) |
| `seq` | int | per SINK, from 1, +1 per line attempted (v0 rule, unchanged) |
| `sid` | int | **new.** Per-sink ordinal of the enclosing depth-0 `solve.begin`, from 1. Sink-owned, like `seq`. It replaces v0 reader rule 3 (partition by begin/end pairs) with a key. A solve that throws writes no `solve.end` (v0 rule), so its `sid` has a `solve.begin` and no end; the next solve takes `sid + 1`. A reader treats such a `sid` as an aborted solve, not as a malformed stream. |
| `depth` | int | enclosing SQP solves; 0 on the IPM, whose restoration runs in place (v0 rule, unchanged) |
| `eng` | string `ipm\|sqp` | **new.** Which engine wrote the line. Needed because one sink may serve both engines (`examples/06_ipm_to_sqp_polish.cpp`). |

### 2.2 Types, absence, text

v0 §2 carries over unchanged: `{:.17g}` round-trip doubles; `"nan"`/`"inf"`/`"-inf"` for a non-finite
MEASUREMENT; int64 integers; lower-snake enum strings; RFC 8259 escaping; no `"unknown"`. Three rules are
tightened:

1. **A field the emitting site did not compute is `null`.** This never means a default, a sentinel or a
   projection. The rule fixes F3(a) and F3(b). Every key that can be `null` names its condition in the tables below.
2. **No vector-valued field** (v0 rule kept). Vectors belong to the callback (`IterationPayload`).
3. **Units.** A double field is on the caller's objective scale unless its table row says "engine scale".
   `_s` keys are wall-clock and informational, never asserted (CLAUDE.md §7). **Only `solve.end`,
   `ipm.phase.exit` and `ipm.kkt_analysis` carry `_s` keys**, so a deterministic solve's stream is
   byte-reproducible once those keys are masked (Q9). The `solver_initialized` message appears only in the
   first solve of a process; it carries no milliseconds on the wire (§2.7), and a byte-comparison drops that
   one kind.

### 2.3 Records

| `ev` | eng | one per | v0 source |
|---|---|---|---|
| `solve.begin` | both | accepted public call (refused arguments write nothing, v0 rule) | `sqp.solve.begin`, `ipm.solve.begin` |
| `iter` | both | iteration row: SQP history row (both depths); IPM iteration record | `sqp.major`, `ipm.iter` |
| `message` | both | diagnostic occurrence (§2.7) | `ipm.message`, `ipm.restoration_exit_row` (folded) |
| `solve.end` | both | normal return (an exception writes no end, v0 rule) | `sqp.solve.end`, `ipm.solve.end` |
| `ipm.phase.begin` / `.end` / `.exit` | ipm | phase that ran | same names; `label` dropped (derivable from `entry`, F3(d)) |
| `ipm.kkt_analysis` | ipm | `init_impl` | unchanged |
| `qp.mode`, `ipqp.iter`, `ipqp.reg`, `ipqp.restart`, `ipqp.route`, `ipqp.certify`, `ipqp.escape`, `fallback.verdict` | sqp | as v0 §4.4-§4.11 | **bytes unchanged after the envelope**. They are QP-tier internals; the M7 telemetry artifacts read them as written. |

That is 16 records against v0's 20. The 4 shared records carry what both engines know.

### 2.4 `iter`: the shared row (`IterationRecord`)

Wire shape: `{envelope, core…, "ipm":{…}}` or `{envelope, core…, "sqp":{…}}`. Exactly one block is present.
The C++ shape is the same: `IterationRecord { core; std::optional<IpmIterationRow> ipm;
std::optional<SqpIterationRow> sqp; }`, with `null` meaning absent.

**The row types are their own.** `IpmIterationRow` and `SqpIterationRow` are not AP-5's `IpmIterationInfo` and
`SqpIterationInfo`. Those stay as AP-5 approved them (8 and 10 fields, delivered under every callback
mask, in the callback's timing: AP-5 §3.1). The row types reuse AP-5's spelling wherever the concept is
the same (`mu`, `alpha_primal`, `alpha_dual`, `regularization_primal`, `regularization_dual`,
`inertia_corrections`, `line_search_trials`; `trust_radius`, `violation_l1`, `dual_regularization`,
`qp_solved`, `qp_status`, `qp_minor_iterations`, `active_set_size`, `verdict`, `elastic`) and add the trace
fields listed below. The two families share names, not storage, because they differ in timing.

**Timing: a row is the point plus the step taken FROM it, on both engines.** The IPM pushes its record after
the step is computed and the alphas are written (`ipm_solver.cpp:4493-4499`), emits it at `:4525`, and applies
the step at `:4663`. The SQP pushes its row after the subproblem at that point. The callback event differs
on the IPM: its extras carry the step that REACHED the point (AP-5 §3.1; the event fires before the
iteration factorizes). So the IPM event for point k+1 carries what row k's step fields hold; the SQP event
and row agree row for row. Identity 5 (§2.8) states the one-row lag; V1 (§2.11) pins it. The IPM's four
unfactorized exit rows are reached by a step but have none leaving them, so their step fields are `null`.

**Core**, in wire order. C++ member names follow `IterationEvent` where the concept is the same
(`iteration`, `step_norm`); the wire keys are the short forms.

| key (C++ member) | type | IPM | SQP | `null` when |
|---|---|---|---|---|
| `iter` (`iteration`) | int | `IpmIterate::iter`, per phase, restarting at each phase | the history index (`major`); restarts at 0 in each depth-1 sub-solve | never. **Not contiguous on the IPM**: five `iters.pop_back()` sites discard a record (`ipm_solver.cpp:3187`, `:3222`, `:3278`, `:3320`, `:3649`), so a row index can skip. |
| `recovering` | bool | restoration active at this iterate (`ipm_solver.cpp:2384-2386`'s predicate) | `depth >= 1` | never [AP5:`IterationEvent::recovering`, same predicate] |
| `tier` | enum | `bar`, or `rest` while recovering | `rest` if `depth>=1`; else `elastic` if the elastic tier supplied the step; else the dispatch arm `ipqp` \| `ssn` \| `walk` | never |
| `f` | double\|null | caller objective, `prim_obj / obj_scale`, when measured (the rule at `ipm_solver.cpp:2519-2520`) | `SqpIterate::f` at depth 0 | `recovering`; IPM phases that never evaluate the declared objective (SOE/OPTNO) |
| `f_recovery` | double\|null | `prim_obj` (the restoration objective) while recovering | `SqpIterate::f` at depth 1 (the feasibility objective) | `!recovering` |
| `stationarity` | double | `kkt_inf / obj_scale` | `SqpIterate::stationarity` | never (`"nan"` on a non-finite measurement, e.g. a `rolled_back` row) |
| `feasibility` | double | `max(econ_inf, icon_inf)` | `SqpIterate::feasibility` | never |
| `complementarity` | double | `barr_inf / obj_scale` | `SqpIterate::complementarity` | never |
| `step` (`step_norm`) | double\|null | inf-norm of the primal step leaving the point (V2) | `SqpIterate::step_norm` | no step on this row (the IPM's four exit sites; SQP `qp_solved == false`, a routed QP-failure row, or a rejected SOC row) |
| `accepted` | bool\|null | `IpmIterate::accepted` | `verdict ∈ {accept_f, accept_h}` | no step on this row |
| `soc` | bool | IPM: derived, `resolved_depth == kRecoveryDepthSoc` (V3) | `soc_applied` | never |
| `watchdog` | bool | the watchdog **armed** this iteration (V3) | the watchdog **restored** this pass (`watchdog_restored`) | never. The meanings differ by engine, as do their counters (AP-5 §1.4); the legend says so. |
| `rolled_back` | bool | a non-finite step rollback this iteration (V3) | `rolled_back` | never |
| `factorizations` | int | the call's running KKT factorize count at the row (V4) | the (sub-)solve's running `counters.factorizations` (V4) | never |

**These residuals are the ENGINE's convergence measures**, the ones each engine's test reads, mapped to the
caller's objective scale. They are the engine residuals on that scale, **not the number the convergence test
gated on when scaling is active**: under active scaling the gate reads the scaled value, and `solve.end.convergence`
(which is the result struct, byte for byte) is where that number lives. They correspond to
`result.convergence`, not to `result.stationarity` and its siblings, which are the declared diagnostics. The
name overlap is the result struct's own (both `SolveResult::stationarity` and
`SolveResult::convergence.stationarity` exist), so v1 follows it and the legend states it. Q8 asks whether
the declared set should also go on rows; Q15 asks the owner to approve the row-scalar statement.

**`soc` on the IPM.** At `ipm_solver.cpp:4291-4295` the recovery chain returns the depth that resolved the
rejection in `resolved_depth` (`kRecoveryDepthSoc = 0` … `kRecoveryDepthRestoration = 4`,
`recovery_chain.h:31-39`), and the solver already histograms it (`:4289`, `:4463`). So `soc` is derived from
the depth that resolved the iteration: SOC **won** the step, matching the SQP's `soc_applied`, and not merely
"was attempted". The variable is block-local at `:4291`, so it is hoisted to loop scope (one int); V3 pins
the equivalence against `second_order_corrections` deltas.

**`ipm` block** (`IpmIterationRow`), in wire order:

| key | from `IpmIterate` | `null` when |
|---|---|---|
| `phase` | phase index (`trace_phase_`) | never |
| `mu` | `mu` | never |
| `alpha_primal`, `alpha_dual` | `alpha_t*alpha_p`, `alpha_t*alpha_d` (the effective lengths the narrow console prints) | no step |
| `alpha_scale` | `alpha_t` | no step |
| `line_search_trials` | `ls_iters` | no step |
| `regularization_primal` | `h_pert` (the last delta) | no step |
| `regularization_primal_total` | `h_pert_cum` | no step |
| `regularization_dual` | `prox_reg_dual` under proximal mode | proximal off, or not factorized (v0 rule) |
| `proximal_primal` | `prox_reg_primal` | the same |
| `inertia_corrections` | `h_facs` | non-finite direction (v0 sentinel −1), or no step |
| `perturbed_pivots` | `p_pivots` | not observed (`!p_pivots_observed`; Accelerate **UNOBSERVED**) |
| `barrier_objective` | `barr_obj` | no step (v0 wrote 0) |
| `feasibility_eq`, `feasibility_ineq` | `econ_inf`, `icon_inf` | never |
| `merit` | `merit_val` | no step (v0 wrote 0) |
| `merit_penalty_1`, `merit_penalty_2` | `pen_par1`, `pen_par2` | never |
| `max_multiplier_eq`, `max_multiplier_ineq` | `max_e_mult`, `max_i_mult` | never |
| `first_rejected_trial` | `first_rejection_iter` | no rejection recorded (v0 rule) |
| `theta_first_rejected` | `theta_at_first_rejection` | no rejection recorded, or the LANG variant (v0 rule) |
| `eval_exceptions` | `eval_exceptions` | never |

**`sqp` block** (`SqpIterationRow`), in wire order:

| key | from `SqpIterate` / event | `null` when |
|---|---|---|
| `depth` | the solve depth (also in the envelope; kept in the block so a row is self-describing outside a stream, AP-5 spelling) | never |
| `mode` | `SqpMajorTraceEvent::mode` (`ipqp\|walk\|ssn`, v0 spelling) | never |
| `verdict` | `verdict` | `!qp_solved` (no verdict was reached; the documented `reject` default at `sqp_solver_types.h:190-197` is a placeholder there, not a reading) |
| `trust_radius` | `tr_radius` | never |
| `violation_l1` | `violation_l1` | never |
| `dual_regularization` | `mu` (renamed, F3(c)) | `!qp_solved` |
| `qp_solved` | `qp_solved` | never |
| `qp_status`, `qp_minor_iterations`, `qp_factorizations` | the three | `!qp_solved` (F3(b)) |
| `tr_binding` | `tr_binding` | never |
| `elastic`, `elastic_clamped` | `elastic_applied`, `elastic_rho0_ceiling_hit` | never |
| `restoration_seed_used` | same | never |
| `ipqp_least_infeasible_primal`, `ipqp_farkas_corroborated` | same | the row is not an IPQP escape row |
| `active_set_size` | `active_rows + active_lower_sides + active_upper_sides` | never |
| `active_set_delta`, `weak_active_rows`, `near_active_rows`, `active_rows`, `active_lower_sides`, `active_upper_sides` | same (v0 §6 rules) | never |

v0 `trial` (== `major`) and `kkt_residual` (`max(stationarity, feasibility)`) are dropped as derivable.

**Join and order rules** carry over from v0 §7: QP-tier lines belong to the next `iter` at the same depth,
and a restoration-requesting row follows its nested solve(s). One rule is added: an IPM row joins its phase
events on (`ipm.phase`, `iter`).

### 2.5 `solve.begin`

Core: the declared problem's census, **verbatim `ProblemStatistics`** (`model/problem.h:38-54`, read from
`Problem::statistics()` `:129`). This is identical on both engines by construction, and it replaces v0's
two `vars_*` censuses and answers M7-18's row-kind question (AP-1 §10.4). Keys in order: `variables`,
`free_variables`, `lower_bounded_variables`, `upper_bounded_variables`, `boxed_variables`,
`fixed_variables`, `constraints`, `equality_rows`, `lower_bounded_rows`, `upper_bounded_rows`, `range_rows`,
`free_rows`, `absent_bounds`. These are followed by the `CommonOptions` values in force: `optimality_tol`,
`feasibility_tol`, `max_iterations`, and `hven_version` (string, `core/version.h`). v0 had no `verbosity`
key and v1 does not add one: it is a console setting and the trace is the same at every rung.

`ipm` block: `n_reduced`, `me`, `mi` (the solver's counts), `internal_fixed_rows`, `phases`,
`max_acceptable_iterations`, `initial_mu`, `obj_scale`, the four acceptable tolerances (names per
[AP5:Q7]: `acceptable_stationarity_tol`, `acceptable_complementarity_tol`, `acceptable_equality_tol`,
`acceptable_inequality_tol`), `inertia_mode`, `restoration_mode`, `kkt_dim`, `kkt_nnz`. v0's `wide_console`
is dropped (Q4).

`sqp` block: `me`, `mi` (the solver's split counts), `qp_mode`, `ws_algebra`, `hessian_mode`
(`SqpOptions::hessian_mode`, new), `row_scaling`, `column_scaling` (the settings, enum).

### 2.6 `solve.end`

Core, in order: `status` (`SolveStatus`, v0 spelling); `stop_reason` (lower-snake `StopReason`
[AP5:§2.3]); `message` (`SolveResult::message`); `iterations`; `returned_best_iterate`;
`infeasibility_certified`; `f`; `diagnostics` = `{stationarity, feasibility, complementarity}`, the DECLARED
set (`SolveResult`, `drivers/solve_result.h:203-208`; renamed from `kkt` so that it does not read as the
engine's KKT measure); `convergence` = the engine measures, **byte-equal to the result struct's
`convergence`** (so it is the number the convergence test gated on, which differs from the row scalars
under active scaling, §2.4); `work` = `WorkCounters` (`drivers/solve_result.h:90-97`; six keys:
`objective_evals`, `gradient_evals`, `constraint_evals`, `jacobian_evals`, `hessian_evals`,
`factorizations`); `counters` = the engine counter tree, generated from the X-macros
(`HVEN_SQP_COUNTERS_FIELDS` and the two nested tables, as v0 §5; IPM: `HVEN_IPM_COUNTERS_FIELDS`
[AP5:G6]); `wall_s` (informational).

`ipm` block: the v0 timing set (`total_time_s` … `misc_time_s`), `phases_ran`, `kkt_analyses_total`,
`factor_memory`, `factor_flops` (`null` when never analysed).
`sqp` block: the v0 scaling seven (`scaling_active` … `col_spread`, unchanged meanings), `start_level_used`.

### 2.7 `message`

One record for both engines. Keys: `kind` (string); `severity` (`notice\|warning`, new); `phase`, `iter`
(int\|null); `a`, `b` (double\|null); `k` (int\|null); `s` (string\|null, new: a per-kind enum value); `p`,
`n`, `z`, `expected_p`, `expected_n` (int\|null, the IPM inertia slots, v0). A slot a kind does not use is
`null` (v0 rule).

| `kind` | eng | sev | payload | count identity |
|---|---|---|---|---|
| the v0 IPM kinds (`solver_initialized` … `interrupt_skipping_phases`), **less `restoration_locally_infeasible`** (folded) **and `interrupt_at_iteration`** (replaced by `stop_at_iteration`) | ipm | v0 console tier: `solver_initialized`, `interrupt_skipping_phases` notice; rest warning | v0 §4.19, except **`solver_initialized.a` is `null` on the wire** (the milliseconds are wall-clock, so they would break byte-reproducibility; the console still prints them from the event) | v0 |
| `restoration_entered` | both | notice | `a` = violation at entry (IPM theta, SQP `h`); `iter` = the requesting row | lines == `counters.restoration_entries` [AP5:G6 for the IPM counter] |
| `restoration_exited` | both | notice; warning unless `s == resumed` | `s` = `resumed\|locally_infeasible\|failed\|non_finite\|interrupted`; `a` = violation at exit; `b` = the threshold judged against (IPM theta threshold; SQP `feasibility_tol`) | one per entered |
| `restoration_refused` | sqp | warning | `s` = `nested\|entry_rule\|budget` | lines == the refusals (V5) |
| `subproblem_failure` | sqp | notice when routed, warning when it ends the solve | `s` = `iteration_limit\|numerical_error\|non_finite`; `k` = failures in a row | lines, **at both depths** (the restoration sub-solve's counters fold into the parent's), == `qp_maxiter_exits + qp_numerical_exits + nonfinite_subproblems − nonfinite_box_projections`. One line per failed EXIT: a cap retry that fails again is one subproblem and two exits, so it yields two lines, matching the counter (`solver_counters.h:1975-1978`) (V5) |
| `elastic_exhausted` | sqp | warning | `k` = rungs climbed | V5 |
| `inconsistent_equalities` | sqp | warning | — | ≤ 1 |
| `start_point_clamped` | sqp | notice | `k` = `start_point_clamps` | ≤ 1, iff that counter > 0 |
| `scaled_convergence_refused` | sqp | notice | `iter` | lines == `scaled_convergence_refusals` |
| `stop_at_iteration` | **both** | notice | `s` = `callback\|token`; `iter` = the row the stop landed on. A token stop and a callback stop use one kind, with `s` telling them apart (replaces `interrupt_at_iteration`, which the IPM had for callbacks only). On the SQP, a stop inside the main subproblem (AP-5 G4) lands on the **stopped-at row** pushed with `qp_solved = false`; `iter` is that row | ≤ 1, and == 1 iff `stop_reason ∈ {callback_stop, stop_requested}` [AP5:§2.3] |

`restoration_exited{s: locally_infeasible, a, b}` replaces both `ipm.restoration_exit_row` and the
`restoration_locally_infeasible` kind (F3(e)). The SQP list covers each `SqpStopReason` the driver can
reach mid-solve, plus the routed (non-terminal) cases a user needs to see. Each SQP emit site is held by a
source scan in the pattern of `tests/sqp/test_qp_mode_sites.cpp` (§6, TR-B).

### 2.8 Identities a v1 stream satisfies (asserted in tests)

1. **Every record counted in `iterations` has exactly one `iter` line and one callback event.** At depth 0 the
   SQP's `iter` lines == `iterations` == `history.size()`, and the IPM's `iter` lines == `iterations`. It is a
   count, not a range: the IPM's `iter` index is not contiguous (five `pop_back` sites, §2.4), so no check
   may assume `iter` runs 0..N-1. The live hole is the **non-finite evaluation abort**: it pushes a
   record (`ipm_solver.cpp:2933-2934`) and leaves the loop (`:2973`), the record is counted
   (`result_.iterations += iters.size()`, `:4885`), and no trace line or event is emitted for it. The
   locally-infeasible restoration door already emits its row (`:3390-3399`). Proof: a test that walks every
   exit door (non-finite abort, locally infeasible, callback stop, token stop, a limit, and the interrupted-point
   path at `:7050`) and checks the count identity at each (§5.3).
2. When `returned_best_iterate == false`, the final depth-0 `iter` core == `solve.end.convergence`'s residual
   fields. It is asserted byte-equal on cells with scaling inactive. Under active scaling `convergence` is the
   number the test gated on and the row is the engine residual mapped to the caller's scale (§2.4), so the
   pair is compared through that map, not for equality. When the best iterate was returned, the final row
   is the last iterate and the result describes the best one, so the identity is not asserted.
3. `factorizations` on the last row ≤ `solve.end.work.factorizations`. The difference is the
   factorizations spent after the last row was emitted, for example an IPM inter-phase re-initialization.
   It is pinned per engine on the HS battery. The SQP's polish stage-2 re-solves are outside both
   figures (AP-5 §2.1 coverage caveat).
4. The message identities in §2.7, and v0's QP-tier identities unchanged (v0 §4.4-§4.11).
5. Callback vs trace: one `IterationEvent` per depth-0 `iter` line. **SQP**: event extras and row agree row for
   row. **IPM, one-row lag**: the event for point k+1 carries the step that reached it, which is what row k's
   step fields hold (`alpha_primal`, `alpha_dual`, `regularization_*`, `inertia_corrections`,
   `line_search_trials`, and the step norm); the first event of each phase has none (AP-5 §3.1: NaN), and the
   row of a point with no step leaving it has `null` there.

### 2.9 Versioning and compatibility

- **One sink writes one version.** `JsonLinesTraceSink` writes v1 only from TR-A on. There is no v0 mode
  and no dual-write, because v0 has no external consumer (F1). In-tree readers of the v0 sink API, the
  v0 stream or the console tables, all of which move in TR-A: the golden tests
  (`tests/sqp/test_trace_writer.cpp`, `tests/interior/test_ipm_trace.cpp`,
  `tests/drivers/test_console_sink.cpp` with `console_sink_base_transcript.inc`),
  `examples/08_results_trace_options.cpp`, `bench/ipm_corpus_leg.cpp`'s counting sink (which counts events
  and does not parse them, `bench/counting_trace_sink.h`), `tests/install_smoke/main.cpp`,
  `tests/drivers/test_callback.cpp`, `test_warm_protocol.cpp`, `test_infinite_bound.cpp`,
  `tests/interior/test_ipm_stop_reason.cpp`, `tests/sqp/test_qp_cap_levers.cpp`,
  `test_qp_mode_sites.cpp`, and `tests/common_support/console_capture.h`. A grep of `tests/`, `examples/` and
  `bench/` for the sink API and the v0 event and function names finds 36 files; TR-A's first commit lists
  the full set and re-points each.
- **Frozen v0 artifacts stay v0.** `docs/notes/data/**` streams, such as
  `2026-09-m6-w4-acceptance/`, are never rewritten (CLAUDE.md §7). `docs/trace-schema-v0.md` stays as
  their schema, with one header line pointing to v1. §2.10 maps every v0 key.
- **The v1 freeze rule** is v0 §8, made binding from day one because TR-3 gives v1 an external consumer:
  - Allowed within v1: a new record, a new trailing key in a record or block, a new enum value. Each is
    declared in `docs/trace-schema-v1.md`'s freeze table, and the affected golden lines are re-derived.
  - Anything else is v2: a rename, a removal, a retype, or a change of meaning.
- **Reader contract.** A reader rejects an unknown `v`. It ignores unknown keys and unknown `ev`. It does
  not rely on key order (writers keep order for the goldens). It tolerates one malformed line at a
  truncation point (v0 rule 4). Because new enum values are allowed within v1, a reader that branches on
  an enum keeps a default arm.
- `JsonLinesTraceSink::kSchemaVersion = 1`, public, so a consumer can static-check the version it was
  built against.

### 2.10 v0 → v1 map (every changed key)

| v0 | v1 |
|---|---|
| `sqp.solve.begin`, `ipm.solve.begin` | `solve.begin`. The `vars_*` five become `ProblemStatistics` keys (`vars_free`→`free_variables`, `vars_lower_only`→`lower_bounded_variables`, `vars_upper_only`→`upper_bounded_variables`, `vars_ranged`→`boxed_variables`, `vars_fixed`→`fixed_variables`). `n`→`variables`. The solver's `me`/`mi` move into the engine block. `kkt_tol`/`econ_tol`/`icon_tol`/`bar_tol`/`max_iters` → the `CommonOptions` keys. `init_mu`→`ipm.initial_mu`. `max_acc_iters`→`ipm.max_acceptable_iterations`. `acc_*_tol` → [AP5:Q7] names. `wide_console` dropped. |
| `sqp.major` | `iter` + `sqp` block. `trial`, `kkt_residual` dropped. `mu`→`sqp.dual_regularization`. `qp_minor_iters`→`sqp.qp_minor_iterations`. `tr_radius`→`sqp.trust_radius`. `soc_applied`→core `soc`. `watchdog_restored`→core `watchdog`. `rolled_back`→core. `elastic_applied`→`sqp.elastic`. `elastic_rho0_ceiling_hit`→`sqp.elastic_clamped`. `major`→core `iter`. |
| `ipm.iter` | `iter` + `ipm` block. `prim_obj`→core `f`/`f_recovery`. `kkt_inf`→core `stationarity`. `barr_inf`→core `complementarity`. `econ_inf`/`icon_inf`→`ipm.feasibility_eq`/`_ineq` (their max → core `feasibility`). `phase` → `ipm.phase`. Other names per §2.4's `ipm` table. |
| `ipm.restoration_exit_row` | removed; `message{kind: restoration_exited, s: locally_infeasible}` |
| `ipm.message` | `message`. The `restoration_locally_infeasible` kind folds into `restoration_exited`; `interrupt_at_iteration` becomes `stop_at_iteration{s: callback}`; `solver_initialized.a` is `null`. |
| `sqp.solve.end`, `ipm.solve.end` | `solve.end`. `majors`→`iterations`. `iters`→`iterations`. SQP scaling keys → `sqp` block. IPM timing → `ipm` block. The declared set is `diagnostics`. |
| `ipm.phase.begin` / `.end` | `label` dropped |
| QP-tier events | unchanged after the envelope |

### 2.11 Verification items (TR-A's first step pins each before the schema doc freezes)

| # | item | outcome rule |
|---|---|---|
| V1 | **Row timing.** One test pins the decision of §2.4: an IPM row's step fields and its V3 flags (`soc`, `watchdog`, `rolled_back`) describe the step leaving its point; the four exit sites' rows are reached by a step and have none leaving them (`null`); the IPM callback event for point k+1 carries row k's step fields (identity 5). The SQP event and row agree. The legend text follows the test. | none needed; the legend states what the test shows |
| V2 | The IPM step norm: today `last_step_norm` is computed at `ipm_solver.cpp:4672`, after the row's emit at `:4525` and only when a callback is installed. The row needs the norm of the step leaving its point, computed before the record is pushed and **unconditionally** (R3, §4): O(primal vars), once per iteration, carried through proofs 2/4/5. | none (the cost is declared) |
| V3 | IPM per-row `soc`/`watchdog`/`rolled_back`: written into the per-iterate record unconditionally. `soc` is `resolved_depth == kRecoveryDepthSoc` (§2.4); `watchdog` and `rolled_back` are set at the sites that already act on them. A test pins each against its counter delta on the HS battery. | drop the key by a declared amendment of this page |
| V4 | The running factorization count at both emit sites, stored in the record unconditionally (an existing counter's value, one int). | drop `factorizations` from the core by declared amendment |
| V5 | SQP message sites: each refusal, failure and ladder-exhaustion site is located and its identity confirmed against counters. | a kind whose identity has no counter is pinned by the tee test (v0 rule 9) |

---

## 3. Human trace (TR-2)

### 3.1 The line

A fixed-width line per `iter`, in the IPOPT/SNOPT tradition. That tradition is: one line per iteration;
log-scale barrier and regularization columns; a restoration marker; and a trailing column of one-letter
condition codes. The first 58 columns are identical on both engines; engine columns follow. Formats,
in fmt syntax, with `-` right-aligned for `null`:

```
shared   {iter:>5} {t} {obj:>14.7e} {stat:>8.2e} {feas:>8.2e} {comp:>8.2e} {step:>8.2e}
IPM      {log10(mu):>5.1f} {alpha_primal:>8.2e} {alpha_dual:>8.2e} {ls:>2} {lg_rg:>6} {codes}
SQP      {trust_radius:>8.2e} {violation_l1:>8.2e} {qp_minor_iterations:>5} {qp_factorizations:>4} {v} {codes}
```

- `obj` is `f`, or `f_recovery` on `R` rows. The tier letter tells which.
- `lg_rg` is `log10(regularization_primal_total)` to `.1f`, or `-` when it is zero or `null`, as in
  Ipopt's `lg(rg)`.
- `v` is the SQP verdict: `F` accept-f, `H` accept-h, `J` reject, `Q` restore, `-` no subproblem (`null`).
- `codes` is the condensed event letters below, with none printed when none apply (a `-` in any other
  column always means `null`).
- Line width is ≤ 100 characters: the IPM line is 58 + 42 = 100 with all seven of its codes, the SQP line
  is 97 with all seven of its. There is no colour on rows (Q5).

**Tier column `t`** (the brief's mode/tier column):

| `t` | wire `tier` | meaning |
|---|---|---|
| `B` | `bar` | IPM barrier iteration |
| `I` | `ipqp` | SQP step from the IPQP tier |
| `S` | `ssn` | SQP step from the semismooth Newton tier |
| `W` | `walk` | SQP step from the active-set walk |
| `E` | `elastic` | SQP step supplied by the elastic tier (any arm) |
| `R` | `rest` | restoration iterate: IPM in place, SQP depth-1 sub-solve |

The wire `tier` for the SQP's dispatch arm spells the same word as `sqp.mode` (`ipqp`), so there is one
spelling for the concept.

**Condensed event codes** (fixed order, absent letters omitted). Shared concepts use the same letter on
both engines:

| code | IPM | SQP | source |
|---|---|---|---|
| `s` | SOC won the step | SOC won the step | core `soc` |
| `w` | watchdog armed | watchdog restored | core `watchdog` |
| `r` | non-finite step rolled back | the same | core `rolled_back` |
| `k` | backtracked (first trial rejected) | — | `ipm.first_rejected_trial != null` |
| `i` | inertia corrected | — | `ipm.inertia_corrections > 0` |
| `x` | evaluation exception(s) | — | `ipm.eval_exceptions > 0` |
| `m` | step not accepted | — | `accepted == false` |
| `b` | — | trust region binding | `sqp.tr_binding` |
| `c` | — | elastic placement clamped | `sqp.elastic_clamped` |
| `d` | — | restoration started from a seed | `sqp.restoration_seed_used` |
| `n` | — | QP solved, not optimal | `qp_solved && qp_status != optimal` |

Letters are unique within each of the three letter spaces (tier, verdict, codes). The verdict and tier
spaces share no letter with each other (the v0-draft collisions, verdict `x`/`R` against code `x` and tier
`R`, are resolved by `J` and `Q`).

A message renders as one line at its stream position: `{'':>5} {mark} {text}`, where `!` marks a warning
and `-` a notice. The text comes from a per-kind template (TR-2 commits the table).

### 3.2 Blocks

- **Header** (kSummary+), 4 lines:
  - the version and engine;
  - the problem census (`ProblemStatistics`);
  - engine facts (IPM: KKT dim/nnz, inertia and restoration modes, phases; SQP: QP mode and algebra,
    Hessian mode, scaling settings);
  - the tolerances and the iteration cap.
- **Legend** (kIterations, once per solve, before the first column header), 5 lines per engine, rendered
  from the tables in §3.1: the column key; the engine columns; the codes; the verdict letters and tier
  letters; and a line for the conventions below. `Format::legend = false` suppresses it. The conventions
  it states: `-` = no value (`null`); `!` and `-` as message marks (warning, notice); `obj` on an `R` row is the
  restoration objective; the iteration counter restarts at 0 in each IPM phase and in each SQP restoration
  sub-solve, and is not contiguous; the step columns describe the step leaving the row's point, on both
  engines.
- **Column header** before the first row, after every 20 rows, and after any message line. The current
  IPM table repeats every 10 rows, keyed on `iter % 10`, which restarts per phase. v1 counts printed rows.
- **IPM phase lines** (kSummary+): `phase k: <entry> … <status>` from the phase events. The current
  per-phase exit block moves to kDetail.
- **Footer** (kSummary+; the `exit` line also at kWarnings), the brief's exit condition, counters and the
  declared diagnostics:

```
exit        <Status> (<stop_reason>) -- <SolveResult::message>
iterations  <iterations> · restoration entries <counters.restoration_entries> · factorizations <work.factorizations>
objective   <f>
diagnostics stationarity <diagnostics.stationarity> · feasibility <…> · complementarity <…>   (declared)
counters    IPM: soc · watchdog · inertia corrections
            SQP: soc · elastic · qp minors · start level · scaling
time        <wall_s> s (informational)                      [Format::timing; goldens set false]
```

The footer does not print `restoration_iters` as a count of `R` rows: that SQP counter counts the sub-solve's
majors (`solver_counters.h:1477`), the IPM has no such field, and `iter` is not contiguous. A reader who
wants the `R`-row count counts the rows.

### 3.3 The print-level ladder (`CommonOptions::verbosity`)

| rung | both engines |
|---|---|
| `kSilent` | nothing (the default; no console sink is constructed) |
| `kWarnings` | `warning` messages and the footer's `exit` line |
| `kSummary` | + header, IPM phase lines, full footer |
| `kIterations` | + legend, column headers, every `iter` line **including `R` rows on both engines** (Q7), `notice` messages |
| `kDetail` (new, Q6) | + one condensed line per QP-tier event (`qp.mode`, `ipqp.escape`, `fallback.verdict`, `ipqp.restart`), the IPM KKT-analysis block and phase-exit blocks |

### 3.4 Examples

**ILLUSTRATIVE.** These were rendered from the formats above with made-up values (the rows by script, so
the widths are exact). They are not a solve's output. TR-2's goldens are rendered from real solves (§6).

Interior point, with an in-place restoration excursion:

```
hven 0.2.0 -- interior-point engine
problem    variables 4 (boxed 4) · constraints 2 (equality 1, lower 1) · absent bounds 0
engine     KKT dim 10, nnz 26 · inertia classic · restoration proximal_switch · phases 1
tolerances optimality 1e-06 · feasibility 1e-06 · max iterations 1000
legend  t: B barrier, R restoration (obj = restoration objective) | stat feas comp: engine measures
        step: |dx|inf leaving this point | alpha_pr/du: step lengths | ls: backtracks
        lgmu, lg(rg): log10 barrier, Hessian shift | '-' = no value | iter restarts at 0 per phase
        codes: s SOC  w watchdog armed  r rollback  k backtrack  i inertia fix  x eval err  m reject
        messages: '!' warning, '-' notice
 iter t      objective     stat     feas     comp     step  lgmu alpha_pr alpha_du ls lg(rg) codes
    0 B  2.5000000e+01 1.30e+00 4.00e+00 1.00e-01 4.10e-01  -1.0 1.00e+00 1.00e+00  0      -
    1 B  2.1800000e+01 6.20e-01 1.10e+00 8.40e-02 3.30e-01  -1.0 7.90e-01 1.00e+00  1   -4.0 ki
    2 B  2.0700000e+01 4.10e-01 9.00e-01 6.00e-02 2.00e-02  -1.0 2.50e-02 1.00e+00  3   -4.0 ki
      - restoration entered at iter 3: theta 9.0e-01
    3 R  4.5000000e-01 2.00e-01 9.00e-01 5.00e-02 3.00e-01  -1.0 1.00e+00 1.00e+00  0      -
    4 R  1.2000000e-01 9.00e-02 2.40e-01 3.00e-02 2.60e-01  -1.0 1.00e+00 1.00e+00  0      -
      - restoration exited at iter 5: resumed, theta 2.4e-01
    5 B  1.8400000e+01 8.00e-02 2.40e-01 2.00e-02 1.90e-01  -2.0 1.00e+00 1.00e+00  0      -
    6 B  1.7100000e+01 1.10e-02 1.30e-02 1.50e-03 6.00e-02  -3.0 1.00e+00 1.00e+00  0      - s
    7 B  1.7014200e+01 2.20e-04 3.00e-05 2.00e-05 4.00e-03  -5.0 1.00e+00 1.00e+00  0      -
    8 B  1.7014017e+01 3.10e-09 2.20e-15 2.50e-09        -  -9.0        -        -  -      -
exit        Optimal (none) -- converged to the optimality tolerance
iterations  9 · restoration entries 1 · factorizations 12
objective   1.70140174e+01
diagnostics stationarity 1.2e-09 · feasibility 2.2e-15 · complementarity 2.5e-09   (declared)
counters    soc 1 · watchdog 0 · inertia corrections 2
```

SQP, with an elastic step, a restoration sub-solve and a hand-off to SSN:

```
hven 0.2.0 -- SQP engine
problem    variables 4 (boxed 4) · constraints 2 (equality 1, lower 1) · absent bounds 0
engine     QP mode walk (schur_border) · Hessian exact · row scaling auto · column scaling auto
tolerances optimality 1e-06 · feasibility 1e-06 · max iterations 100
legend  t: I IPQP  S SSN  W walk  E elastic  R restoration (obj = restoration objective)
        stat feas comp: engine measures | step: |p|inf from this point | radius: trust region
        h(l1): l1 violation | qp_it/qp_fa: QP minors/factorizations | '-' = no value
        v: F accept-f  H accept-h  J reject  Q restore | iter restarts per restoration sub-solve
        codes: s SOC  w watchdog restored  r rollback  b TR bound  c clamp  d seed  n QP not optimal
        messages: '!' warning, '-' notice
 iter t      objective     stat     feas     comp     step   radius    h(l1) qp_it qp_fa v codes
    0 W  2.5000000e+01 1.30e+00 4.00e+00 0.00e+00 1.00e+00 1.00e+00 6.10e+00     4    2 F
    1 W  2.3100000e+01 9.00e-01 2.20e+00 1.00e-02 1.00e+00 1.00e+00 3.40e+00     3    1 H b
    2 W  2.3100000e+01 9.00e-01 2.20e+00 1.00e-02 1.00e+00 1.00e+00 3.40e+00     5    2 J n
    3 E  1.9000000e+01 4.00e-01 9.00e-01 2.00e-02 5.00e-01 5.00e-01 1.40e+00    11    3 H c
      - restoration entered at iter 4: h 1.4e+00
    0 R  1.4000000e+00 3.00e-01 9.00e-01 0.00e+00 2.00e-01 1.00e+00 1.40e+00     6    2 F
    1 R  2.0000000e-02 1.00e-02 1.50e-02 0.00e+00 4.00e-02 1.00e+00 2.00e-02     3    1 F
    2 R  0.0000000e+00 3.00e-09 1.00e-12 0.00e+00        - 1.00e+00 1.00e-12     -    - -
      - restoration exited at iter 4: resumed, h 1.0e-12
    4 W  1.8600000e+01 3.80e-01 8.50e-01 0.00e+00 6.00e-01 5.00e-01 1.40e+00     4    1 Q d
    5 S  1.7026000e+01 2.00e-03 4.00e-06 0.00e+00 1.20e-02 1.00e+00 4.00e-06     2    1 F s
    6 S  1.7014017e+01 4.40e-10 1.10e-16 0.00e+00        - 2.00e+00 1.10e-16     -    - -
exit        Optimal (none) -- converged to the optimality tolerance
iterations  7 · restoration entries 1 · factorizations 15
objective   1.70140174e+01
diagnostics stationarity 4.4e-10 · feasibility 1.1e-16 · complementarity 0.0e+00   (declared)
counters    soc 1 · elastic 1 · qp minors 41 · start level cold · scaling off
```

Row 4 prints after its sub-solve because a restoration-requesting row is pushed with its final values
(v0 §4.3, kept). The `restoration entered` notice is what puts the excursion in reading order. The SQP's
`iterations` counts the depth-0 rows (7); the three depth-1 `R` rows are the sub-solve's own.

---

## 4. Cost contract: zero cost when tracing is off

**"Off"** means no sink attached and `verbosity == kSilent`. **There is one deliberate exception to "no new
work off": the IPM's per-iterate record gains five fields unconditionally** (R3, Q14): `step`, `soc`,
`watchdog`, `rolled_back`, `factorizations`, so that `IpmResult::history` can be always on, symmetric with
the SQP's. They are a handful of scalar stores per iteration plus the O(primal vars) step norm (V2). Their
cost is not argued; it is carried through proofs 2, 4 and 5 below and gated.

| rule | how it holds | evidence today |
|---|---|---|
| C1. Every emit is one pointer load and test, and **the event is constructed only inside the guard** | the row emits already are. `emit_message` changes to take a **builder** (a lambda returning the event), invoked only after the sink test, so a message event is no longer built when nothing listens. The one other construction outside a guard is `SqpFallbackVerdictTraceEvent` (read by control flow), on a named allowlist. | F4 |
| C2. No new unconditional bookkeeping beyond the five row fields | the five fields above, and nothing else: no counter-delta snapshots, no per-sink state. `factorizations` reads an existing counter (V4); `soc` reads an existing local (V3). | — |
| C3. No console object at `kSilent` | unchanged composition | `ipm_solver.cpp:6208-6226`, `sqp_solver.cpp:3292-3308` |
| C4. Formatting cost lives in sinks | `JsonLinesTraceSink` builds strings; `ConsoleTraceSink` formats. Neither exists when off. | `trace_writer.cpp:783-803` |
| C5. `IpmResult::history` (TR-D) is a RESULT feature, always on | its cost is the five fields (priced above) plus an O(iterations) copy at exit; TR-D's proof runs with **no sink attached** | see TR-D's own gate (§6) |

**Proof, per group that touches an engine TU** (thresholds frozen in the group's declaration before its
first leg, per brief §0):

1. **Structural.** A source-scan test fails when any `*TraceEvent` construction or `emit_*` call in
   `src/drivers/` or `src/qp/` is not lexically inside a `!= nullptr` guard or inside a builder lambda passed
   to a guarded emit, except for the named allowlist (today one entry: `SqpFallbackVerdictTraceEvent` at
   `sqp_solver.cpp:5424`). It extends `tests/sqp/test_qp_mode_sites.cpp`'s mechanism, and its falsification
   is a planted unguarded construction and a planted unguarded call.
2. **Counters off.** The 27-cell replay × 3 engines, byte-exact against the pre-group pin, with no sink
   (AP-2 proof 5). Counter and status columns are exact; residual-class columns are compared by the calibrated
   near-ulp gate (CLAUDE.md §7: MKL kernels are address-sensitive, so a residual can differ in its last digits
   between processes).
3. **Counters on.** The same cells with a `JsonLinesTraceSink` attached and verbosity `kDetail` give the
   same counters and statuses (residuals by the same near-ulp gate). A second leg attaches **both an iteration
   callback (at `kScalars` and at `kAll`) and a sink**. The trace and the callback are observation.
4. **Instructions off.** Pass A/B instruction counts (`instructions:u`) with no sink, on the IPM corpus leg
   (`bench/ipm_corpus_leg.cpp`) and the SQP corpus per mode. Proposed gate: aggregate |Δ| ≤ 0.1 % and no
   cell beyond 0.5 %. **Before that gate is frozen, an A/A calibration leg** (the same build against itself,
   same cells) measures the instrument's own spread, and the gate is set above it. Instruction counts are
   deterministic, so they bound "zero" more tightly than wall-clock can.
5. **Runtime off.** The P5 serial runtime gate (the `6a9fbfe39` instrument: filtered B/A per mode within
   ±0.5 %), run solo under CLAUDE.md §7.
6. **On-cost** of the JSON sink and the console at each rung is reported as informational, never gated.

---

## 5. TR-3: the consumers

### 5.1 hven-bench (the HB lane executes; hven-bench is read-only from hven)

Because of F1, the migration is about what the lane reads, not about parsing a trace format:

1. **Pin bump** to the TR-E head, as a declared pin event in `cmake/hven_pin.cmake`. This is the one move
   that also absorbs AP-5 G6's IPM renames.
2. **Counters through `dump_counters`.** It writes `key = value` text, one counter per line, the twin of
   `dump_options` (`ipm_options.h:298`, `sqp_options.h:948`), from the same X-macro generator as
   `solve.end.counters`, and it includes the IPM's `kkt_factor_counters`
   (`ipm_solver_types.h:244`). It replaces the hand enumerations (`hven_bench_cell.cpp:766-800` for the IPM,
   and the per-pin `add_*_counts` blocks `:810-1350` for the SQP) with one generic loop **for the counters
   the engines hold**. The **Ledger-sum blocks** (`hven_bench_cell.cpp:905-1248`, which sum
   per-record `QpCounters` over `ledger.records()`) stay harness-side: `dump_counters` does not cover them. The
   side-file column names change to the v1/AP-5 names: a declared harness schema event, with an old→new
   map. **Comparing the old side files with the new ones is new comparator work** in TR-E (a name map
   plus the generic loop's order), not something `dump_counters` provides.
3. **Evaluation columns.** `eval_f/g/jac/hess` are `null` on both hven lanes today (hven-bench
   `schema/v1.md` cols 18-21). They fill from `SolveResult::work` (`drivers/solve_result.h:90-97`), as a
   declared harness schema event.
4. **Optional per-cell trace.** A lane flag writes the v1 stream to `<run>/traces/<cell_id>.jsonl` for TU
   runs. A traced cell carries a `traced` pin column, so traced and untraced rows are never compared
   unknowingly. `wall_s` is informational in either case.
5. **Reader.** `schema/trace_v1.py` validates `v`, the `seq` contiguity, the begin/end pairing per `sid`
   (a `sid` with no end is an aborted solve, §2.1), and the absence spelling. Its self-test fixtures are
   hven's golden lines, copied with their provenance. It is the public reader friction #29 asked for
   (AP-1 §6).
6. **`probe1500-trace-` transcripts** (`hven_bench_cell.cpp:380-386`) change format at the pin bump.
   They are used for quoting only, and old transcripts stay as recorded.
7. **Recommended (HB's call):** rename the harness CSV's "trace schema v1" to "row schema v1" so that "v1"
   names one thing.

### 5.2 M7-4's residual (brief §TR; the A6 list's group 3)

M7-4's residual is the +0.6 % IPM attached-callback instruction overhead left unattributed after the
`IterationPayload` mask (`docs/notes/2026-09-m7-ledger.md:280-300`). **AP-5 G5 already re-measures it** on
the rebuilt event fill (AP-5 §5, G5's proof). TR-E repeats the measurement at the TR-E head, which also
carries the five unconditional IPM record fields (§4), with W0 T2's instrument: callback attached at
`kScalars` against no callback, on the IPM corpus leg, pass A/B instructions. Both numbers are quoted. If
the TR-E number lands within ±0.5 %, M7-4 closes; otherwise it is registered with its number.

### 5.3 M7-19 (rides TR-B)

M7-19 is restated as identity 1 (§2.8): **every record counted in `iterations` has exactly one trace line and
one callback event.** The live hole is the non-finite evaluation abort (`ipm_solver.cpp:2933-2934`, break
`:2973`, counted at `:4885`): it pushes a record and writes neither line nor event. The locally-infeasible
restoration door is not a hole: it already emits its row (`:3390-3399`), so v0 §4.14's account
(`docs/trace-schema-v0.md:474-477`) is stale and is corrected when the v0 header pointer is added.
**Ownership:** AP-5 §2.1 places M7-19 as its own declared commit after G6; that commit owns the code change
(emit the row and its callback event on the abort door). TR-B owns the test: a walk over every exit door
(non-finite abort, locally infeasible, callback stop, token stop, a limit, and the `:7050` path) asserting
the count identity at each. TR-B's test lands red-first against the pre-fix tree and goes green with the AP-5
commit.

---

## 6. Landable groups

Effort is in AP-1 §13's units, as in AP-5 §5. Every group carries AP-2's common six. No group can move
the walk (§4 proof 2/3), so none needs a census. All groups land after DN-3 proper and after AP-5 G5/G6
where marked, and **before TU-1** (brief §1 (6)).

| group | content | behaviour change | proof beyond the common six | est. |
|---|---|---|---|---|
| **TR-A** schema + writer + human trace | V1-V4 pinned first. `IterationRecord`, `IpmIterationRow`, `SqpIterationRow`; the five unconditional IPM record fields; the 4 shared records; the envelope v1; the v0 writer retired; **the console sink re-cut in the same group** (legend, header, footer, the ladder incl. `kDetail` if Q6, colour policy Q5; `wide_console`, the IPM banner and `format_iteration_table` retired, Q4) so there is no interim re-render of the old tables; `emit_message` builder; `docs/trace-schema-v1.md` (exit criterion 8) with its freeze table; v0 doc header pointer; every in-tree v0 reader moved (§2.9). [AP5:G5, G6] | trace bytes and console bytes, as **one declared console-bytes event** (every golden line re-derived; the 159/119-column tables go) | a golden line per record and block (hand-filled structs, aggregate-arity `static_assert` per struct, as v0); **human-trace goldens** rendered from real solves: 4 HS problems × 2 engines × 4 rungs, plus one restoration and one elastic fixture, `Format{color: kNever, timing: false}`, and one golden per message template; identities 1-5 on the HS battery, both engines; §4 proofs 1-5 | 3.5-4 |
| **TR-B** messages | SQP kinds; `restoration_entered/exited` on both engines; `stop_at_iteration`; the IPM fold; message-site source scan; **the M7-19 exit-door walk test** (§5.3) | none (observation) | §2.7 identities on HS plus the restoration fixtures (`test_callback.cpp:668`, `test_evaluation_failure.cpp:886`); scan falsified by a planted site; §4 proofs 1-4 | 2 |
| **TR-C** | folded into TR-A (R6). No separate group. | | | 0 |
| **TR-D** IPM history [AP5:Q8] | `IpmResult::history` as `std::vector<IterationRecord>`, **always on**, built from the impl's per-phase records at exit (O(iterations), once). Its cost is the five record fields, already landed in TR-A and priced there | new result field | `history` == the depth-0 `iter` stream, row for row, both engines; replay byte-exact; **§4 proofs 2/4/5 run with no sink attached** | 2 |
| TR-D' (if Q1 = full) | `SqpResult::history` → `std::vector<IterationRecord>`; `SqpIterate` retires; **adds `factorizations` to the SQP row** unconditionally (the same always-on cost, same proofs) | **break** (mechanical: `h.tr_radius` → `h.sqp->trust_radius`) | replay byte-exact; every history reader in tests re-pointed; migration-guide lines | 2 |
| **TR-E** consumers (TR-3) | hven: `dump_counters` × 2 and the v1 reader fixtures; hven-bench (HB lane): §5.1 items 1-6, including the side-file comparator; M7-4 re-measure | harness schema events (declared, in hven-bench) | bench comparator self-tests; one TRAIN leg compared old pin vs new pin through the column map, 0 counter diffs; M7-4 verdict | 2-3 |
| | **total** | | | **10-11 (12-13 with TR-D')** |

The estimates are the reviewer's re-estimate (TR-A rises from 2.5 plus 2 for the console to 3.5-4 with
the always-on fields and the proofs they carry; the total from 9/11).

**Order:** TR-A → TR-B → TR-E. TR-D lands after TR-A. TR-A and TR-B revert in reverse
order; TR-D and TR-E revert alone.

---

## 7. Public names for approval

Every break carries a migration line, and **all of them ride the same approved API-break window as AP-5**
(Q16), so one migration guide covers both.

| name | kind | new / break | migration |
|---|---|---|---|
| `IterationRecord` | struct (the v1 row: core + `ipm`/`sqp`) | new (Q1) | — |
| `IpmIterationRow`, `SqpIterationRow` | structs (the engine row blocks; own types, not AP-5's callback info structs, Q13) | new | — |
| `IterationTier {kBarrier, kIpqp, kSsn, kWalk, kElastic, kRestoration}` | enum; wire `bar\|ipqp\|ssn\|walk\|elastic\|rest` | new | — |
| `IpmIterationInfo`, `SqpIterationInfo` | structs | AP-5's names, **unchanged** by this page | — |
| `SolveBeginTraceEvent`, `IterTraceEvent`, `MessageTraceEvent`, `SolveEndTraceEvent` | trace event structs | **break**: replace `Sqp/IpmSolveBeginTraceEvent`, `SqpMajorTraceEvent`, `IpmIterTraceEvent`, `IpmMessageTraceEvent`, `Sqp/IpmSolveEndTraceEvent` | A custom `TraceSink` overrides the four new virtuals instead of the old pairs; each old event's fields map per §2.10 (the row moves from the event's flat fields into an `IterationRecord`). |
| `TraceSink::{on_solve_begin, on_iter, on_message, on_solve_end}` | virtuals (no-op defaults) | **break**: replace the six `on_sqp_*`/`on_ipm_*` pairs; `on_ipm_restoration_exit_row` removed | Rename each override per the §2.3 table; handle the removed restoration-exit-row call by reading `message{kind: restoration_exited, s: locally_infeasible}` in `on_message`. |
| `MessageKind`, `MessageSeverity {kNotice, kWarning}` | enums | **break**: `IpmMessageKind` → `MessageKind` (its values plus §2.7's; the restoration-locally-infeasible and interrupt-at-iteration kinds removed) | `IpmMessageKind::kX` → `MessageKind::kX`; the restoration-locally-infeasible kind → `kRestorationExited` with `s == locally_infeasible`; the interrupt-at-iteration kind → the stop-at-iteration kind. |
| `JsonLinesTraceSink::kSchemaVersion` | constant (= 1) | new | — |
| `ConsoleTraceSink::Format::{color, legend, timing}`, `ConsoleColor {kAuto, kAlways, kNever}` | fields, enum | new; `Format::wide` removed (break) | Delete any `Format{wide, …}` initializer's first member; the width is now fixed. |
| `Verbosity::kDetail` | enumerator | new (Q6) | — |
| `IpmOptions::wide_console` | field | **removed** (Q4) | Delete the assignment; there is one console width. |
| `format_iteration_table` and its five pieces (`sqp_solver.h:294-325`) | functions | **removed** (Q4); `render_history(const SolveResult-derived&)` if a post-hoc table is wanted | Attach a `ConsoleTraceSink` (or set `verbosity`) for live rows; for a post-hoc table, format `history` yourself or use `render_history` if adopted. |
| `dump_counters(const IpmResult&, std::ostream&)`, `dump_counters(const SqpResult&, std::ostream&)` | functions | new (Q11) | — |
| `IpmResult::history` | field (`std::vector<IterationRecord>`), always on (Q14) | new [AP5:Q8] | — |
| `SqpResult::history` element type | field | **break** if Q1 = full | `h.tr_radius` → `h.sqp->trust_radius`, `h.mu` → `h.sqp->dual_regularization`, `h.major` → `h.iter`, `h.step_norm` → `h.step_norm` (a core member, unchanged spelling); the migration guide carries the full field map from §2.10. |
| `tools/trace_v1.py` (hven) / `schema/trace_v1.py` (bench) | tooling | new (not C++) | — |

Detail-internal (not for approval): the IPM row builder, the message-site scan, the `emit_message`
builder signature, and the per-kind message templates.

---

## 8. Owner questions

| # | question | recommendation |
|---|---|---|
| Q1 | One row type, `IterationRecord`, for the trace, `IpmResult::history` and also `SqpResult::history`? | **Yes, full unification**: one interface for both solvers. The SQP half (TR-D') is a 2-unit mechanical break that rides the same break window. The alternative is trace + IPM history only, keeping `SqpIterate`: 0 units, but two history shapes stay. |
| Q2 | Engine block nested (`"ipm":{…}`) or flat? | Nested. It mirrors the C++ type, makes the shared core structurally visible to a generic reader, and `json_normalize` flattens it in one call. |
| Q3 | Retire v0 at TR-A with no dual-write? | Yes. There is no external v0 consumer (F1), and frozen v0 artifacts keep their v0 doc. |
| Q4 | The human trace replaces today's IPM banner and coloured 119/159-column table, `IpmOptions::wide_console`, and `format_iteration_table` with its five pieces? | Yes. One concise line per engine; detail lives in the machine trace and `kDetail`. |
| Q5 | Colour on the console? | `ConsoleColor::kAuto` by default (colour only on a tty, and then only for the warning mark and the exit line); rows stay plain. Goldens use `kNever`. |
| Q6 | Add `Verbosity::kDetail` for QP-tier and IPM phase-exit lines? | Yes. Tuning runs need it, and it keeps `kIterations` concise. |
| Q7 | Show SQP restoration sub-solve rows (tier `R`) at `kIterations`? | Yes. That makes the two engines symmetric (the IPM already shows its in-place restoration rows). The callback's `CallbackScope` default stays as AP-5 has it. |
| Q8 | Put the declared (caller-unit) diagnostics on every row as well? | No. On the IPM they cost the O(n+m) expansion the callback pays only under `kDiagnostics`. The rows carry engine measures, and the declared set is on `solve.end.diagnostics` and on the callback. |
| Q9 | Per-row wall time? | No. It keeps a deterministic solve's stream byte-reproducible with three `_s`-bearing records masked. |
| Q10 | Ask the HB lane to rename the harness CSV's "trace schema v1"? | Yes (§5.1 item 7). |
| Q11 | Ship `dump_counters` (the harness's standing request), as `key = value` text, the twin of `dump_options`? | Yes. It is the same generator as `solve.end.counters` and covers the counters the engines hold (not the harness's Ledger sums), so there is no second bookkeeping path. |
| Q12 | Tier letters and wire spellings as in §3.1? | Approve. They are the brief's five modes plus the IPM's `B`, with the IPQP tier spelled `ipqp` everywhere. |
| Q13 | Approve separate row types (`IpmIterationRow`, `SqpIterationRow`) distinct from AP-5's callback info structs, and the timing statement: a row is the point plus the step taken FROM it on both engines, while the IPM callback event carries the step that REACHED the point (one-row lag, identity 5)? | **Yes.** It keeps AP-5 as approved and states the lag in one place, pinned by V1. |
| Q14 | Make `IpmResult::history` always on (symmetric with `SqpResult::history`), adding five fields (`step`, `soc`, `watchdog`, `rolled_back`, `factorizations`) to the IPM's per-iterate record unconditionally? | **Yes.** One shape, one default, no snapshot machinery. The cost (a few scalar stores plus one O(primal vars) norm per iteration) is carried through proofs 2/4/5, with TR-D's proof run with no sink attached. |
| Q15 | State the row scalars as the engine residuals mapped to the caller's objective scale, **not** the gated number when scaling is active, with `solve.end.convergence` byte-equal to the result struct and identity 2 restricted to `returned_best_iterate == false`? | **Yes.** It keeps `convergence` the number the test read, and the rows honest about what they are. |
| Q16 | Do the breaks in §7 (trace events, sink virtuals, message enum, `Format::wide`, `wide_console`, `format_iteration_table`, `SqpResult::history`) ride the same approved API-break window as AP-5, with one migration guide for both? | **Yes.** |

---

## Appendix A. Dependencies on the AP-5 page (owner-approved)

| marker | what it supplies |
|---|---|
| [AP5:§3.1] | `IpmIterationInfo`/`SqpIterationInfo` (unchanged here); `IterationEvent::recovering`; the callback's step timing |
| [AP5:§2.3] | `StopReason`, the source of `solve.end.stop_reason` and the `stop_at_iteration` identity |
| [AP5:Q7] | the IPM tolerance renames in the `ipm` block |
| [AP5:G5] | the event-fill rebuild; its M7-4 re-measurement (§5.2) |
| [AP5:G6] | `IpmCounters`, `HVEN_IPM_COUNTERS_FIELDS`, `counters.restoration_entries` on the IPM; the post-G6 M7-19 commit |
| [AP5:Q8] | `IpmResult::history`, to be built on this page's row (TR-D) |
| AP-5 M1 | structured SQP messages, owned here (§2.7) |

---

## Review riders folded (2026-11-12)

| rider | landed in |
|---|---|
| R1 own row types, timing, identity 5, V1, pinning test, `phase`/`depth`/`active_set_size`, member names | header status; F2, F6; §2.4 (row types, timing, core and blocks); §2.8 identity 5; §2.11 V1/V3; §7; Q13 |
| R2 M7-19 restated, hole located, test over every exit door, `iter` not contiguous, footer, ownership | F3(g); §2.4 core `iter`; §2.8 identity 1; §3.2 footer; §3.4 examples; §5.3; §6 TR-B |
| R3 IPM history always on, five fields, no-sink proof, TR-D' factorizations | §4 preamble and C2/C5; §2.11 V2-V4; §6 TR-D, TR-D'; Q14 |
| R4 engine residuals on the caller scale, `convergence` byte-equal, identity 2, `diagnostics` | §2.4 residual paragraph; §2.6; §2.8 identity 2; §2.10; §3.2 footer; Q8, Q15 |
| R5 `emit_message` builder, scan over `*TraceEvent` and `emit_*` with allowlist, F4 wording | F4; §4 C1 and proof 1 |
| R6 console sink with TR-A, in-tree reader list, re-estimate | §2.9; §6 (TR-A, TR-C folded, estimates) |
| R7 migration lines, break-window question | §7 (migration column); Q16 |
| R8 `dump_counters` text format, `kkt_factor_counters`, covered subset, comparator work | §5.1 item 2; §6 TR-E; Q11 |
| R9 null-rule fixes (verdict, ipqp escape fields, step null rows, `soc` derived) | §2.4 core `step`, `soc`; `sqp` block rows for `verdict` and `ipqp_*` |
| R10 `solver_initialized` milliseconds, `solve.begin.verbosity` | §2.2 rule 3; §2.5; §2.7 first row; §2.10 |
| R11 `stop_at_iteration`, token and mid-QP stopped-at row, `subproblem_failure` identity | §2.7 |
| R12 callback+sink leg, near-ulp gate, A/A calibration, G5 re-measures M7-4 | §4 proofs 2-4; §5.2 |
| R13 IPM line ≤ 100 chars, legend, letter collisions, `acq`/`ipqp` | §3.1 (formats, tier and verdict letters); §3.2 legend; §3.4 examples; §7 `IterationTier`; Q12 |
| R14 `WorkCounters` keys and cites, `sid` after an exception | §2.1 `sid`; §2.6; §5.1 item 3; header cites |
