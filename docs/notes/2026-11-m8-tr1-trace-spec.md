# M8 TR-1: trace v1 specification (machine schema, human trace, cost contract, consumers)

Status: DESIGN, for the owner's approval. No code has changed. Base: `origin/m8` at `7e5ed61de`. Line
numbers below are at that commit. Inputs: the M8 brief §TR and §1 (6) (`docs/notes/2026-11-m8-brief.md:352-366`,
`:93-94`); `docs/trace-schema-v0.md`; the AP-5 interface design (`docs/notes/2026-11-m8-ap5-interface-design.md`,
not yet approved); the AP-1 items routed here (`docs/notes/2026-11-m8-ap1-api-review.md:433-436`, `:699-724`);
hven-bench at `eb292e5` (read-only).

**Dependency markers.** `[AP5:x]` marks every place this spec depends on the AP-5 page's outcome. If AP-5's
item x is not approved, the fallback is stated in place. Appendix A lists them all.

SNOPT/IPOPT references are to their public output documentation only: the Ipopt documentation's output
section and Wächter & Biegler (Math. Prog. 106, 2006), and the SNOPT User's Guide's description of the
major iteration log. No source of either solver was read.

---

## 0. Findings that shape the design

| # | finding | evidence | consequence |
|---|---|---|---|
| F1 | **hven-bench reads no trace lines.** Its "trace schema v1" is the harness's own CSV schema, a name collision with this one. The lane reads result structs by hand: IPM flat fields, and per-pin hand-enumerated `SqpCounters` blocks. It notes "hven has no counters dump … That dump is the harness's standing request." | hven-bench `cmake/hven_pin.cmake:561-562`; `schema/v1.md:1`; `lanes/hven/hven_bench_cell.cpp:766-800`, `:810-1350` | v0 has no external consumer, so v1 replaces it with no dual-write window (§2.9). TR-3's real migration is counters and work, not trace parsing (§5). |
| F2 | **Three surfaces carry a per-iteration row, with three shapes.** `ipm.iter` = `IpmIterate` verbatim (engine-internal names). `sqp.major` = `SqpIterate` verbatim = `SqpResult::history`. `IterationEvent` = declared (caller-unit) diagnostics plus four engine optionals. | `trace_writer.cpp:931-968`, `:1024-1069`; `sqp_solver.cpp:6931-6955`; `solve_result.h:390-451` | v1 defines one row, `IterationRecord`: a shared core plus one engine block. The trace, `IpmResult::history` (AP-5 Q8) and optionally `SqpResult::history` all carry it (§2.4, Q1). |
| F3 | **v0 warts that only a version bump can fix.** (a) The IPM's four unfactorized exit rows write `barr_obj`, `merit_val`, `ls_iters` = 0 and all alphas = 1: fresh defaults, not measurements. (b) `qp_status`, `qp_minor_iters` and `qp_factorizations` are "meaningful iff `qp_solved`" yet written as values. (c) `mu` means the barrier parameter on `ipm.iter` and the dual regularization on `sqp.major`. (d) The phase `label` carries a trailing space. (e) `ipm.restoration_exit_row` repeats the `restoration_locally_infeasible` message's two numbers. (f) There are two bound censuses (`vars_*`) while `ProblemStatistics` already gives the declared census on both engines. (g) M7-19: the `ipm.iter` count has a hole. | v0 `:467-472`, `sqp_solver_types.h:183-187`, `:151`, v0 `:501`, v0 `:606`/`:491-494`, `model/problem.h:38-54`, v0 `:474-477` | Each is fixed in v1 (§2.10 map). |
| F4 | **The off-path is already one pointer test per emit site**, and the event is built inside the guard. The console is not constructed at `kSilent`. | `ipm_solver.cpp:2605`, `:3390-3393`, `:4523-4527`, `:6212-6225`; `sqp_solver.cpp:6943-6946`, `:3292-3308` | The cost contract (§4) states this as a rule, adds a source scan and the proofs, and bans new unconditional bookkeeping. |
| F5 | **The SQP emits no structured messages** (AP-5 M1). Its only console text is the table, `Status`, `Start Level` and `Scaling` lines. | `sqp_print.cpp:77-121`; AP-5 §1.5 | v1 gives both engines one `message` record and adds the SQP kinds (§2.7). |
| F6 | **The SQP history row describes the point and the step taken FROM it.** The IPM `IterationEvent` extras describe the step that REACHED the point (AP-5 C5). The `ipm.iter` row's own step-field timing is not stated anywhere in v0. | `sqp_solver_types.h:111-161`; AP-5 §3.1 C5 | v1 keeps each engine's timing, states it in the legend, and pins it with a test before the legend freezes (V1, §2.11). |

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
| `sid` | int | **new.** Per-sink ordinal of the enclosing depth-0 `solve.begin`, from 1. Sink-owned, like `seq`. It replaces v0 reader rule 3 (partition by begin/end pairs) with a key. |
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
   byte-reproducible once those keys are masked (Q9).

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
The C++ shape is the same: `IterationRecord { core; std::optional<IpmIterationInfo> ipm;
std::optional<SqpIterationInfo> sqp; }` [AP5:§3.1]. The engine blocks reuse AP-5's struct names and
field names and add the trace-only fields listed below. If AP-5 §3.1 is not approved, the same names are
introduced here, under the same spelling.

**Core**, in wire order:

| key | type | IPM | SQP | `null` when |
|---|---|---|---|---|
| `iter` | int | `IpmIterate::iter`, per phase | the history index (`major`); restarts at 0 in each depth-1 sub-solve | never |
| `phase` | int\|null | phase index (`trace_phase_`) | — | always on SQP (the engine has no phases) |
| `recovering` | bool | restoration active at this iterate (`ipm_solver.cpp:2384-2386`'s predicate) | `depth >= 1` | never [AP5:`IterationEvent::recovering`, same predicate] |
| `tier` | enum | `bar`, or `rest` while recovering | `rest` if `depth>=1`; else `elastic` if the elastic tier supplied the step; else the dispatch arm `acq` (IPQP) \| `ssn` \| `walk` | never |
| `f` | double\|null | caller objective, `prim_obj / obj_scale`, when measured (the rule at `ipm_solver.cpp:2519-2520`) | `SqpIterate::f` at depth 0 | `recovering`; IPM phases that never evaluate the declared objective (SOE/OPTNO) |
| `f_recovery` | double\|null | `prim_obj` (the restoration objective) while recovering | `SqpIterate::f` at depth 1 (the feasibility objective) | `!recovering` |
| `stationarity` | double | `kkt_inf / obj_scale` | `SqpIterate::stationarity` | never (`"nan"` on a non-finite measurement, e.g. a `rolled_back` row) |
| `feasibility` | double | `max(econ_inf, icon_inf)` | `SqpIterate::feasibility` | never |
| `complementarity` | double | `barr_inf / obj_scale` | `SqpIterate::complementarity` | never |
| `step` | double\|null | inf-norm of the primal step (V2) | `SqpIterate::step_norm` | no step on this row (the IPM's four exit sites; SQP `qp_solved == false`) |
| `accepted` | bool\|null | `IpmIterate::accepted` | `verdict ∈ {accept_f, accept_h}` | no step on this row |
| `soc` | bool | a second-order correction won this iteration's step (V3) | `soc_applied` | never |
| `watchdog` | bool | the watchdog **armed** this iteration (V3) | the watchdog **restored** this pass (`watchdog_restored`) | never. The meanings differ by engine, as do their counters (AP-5 §1.4); the legend says so. |
| `rolled_back` | bool | a non-finite step rollback this iteration (V3) | `rolled_back` | never |
| `factorizations` | int | the call's running KKT factorize count at emission (V4) | the (sub-)solve's running `counters.factorizations` (V4) | never |

**These residuals are the ENGINE's convergence measures**, the ones each engine's test reads, put on the
caller's objective scale. They correspond to `result.convergence`, not to `result.stationarity` and its
siblings, which are the declared diagnostics. `IterationEvent`'s three scalars are the declared ones. The
name overlap is the result struct's own (both `SolveResult::stationarity` and
`SolveResult::convergence.stationarity` exist), so v1 follows it and the legend states it. Q8 asks whether
the declared set should also go on rows.

**`ipm` block** (`IpmIterationInfo`, extending AP-5's eight fields [AP5:§3.1]), in wire order:

| key | from `IpmIterate` | `null` when |
|---|---|---|
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

**`sqp` block** (`SqpIterationInfo`, extending AP-5's ten fields [AP5:§3.1]), in wire order:

| key | from `SqpIterate` / event | `null` when |
|---|---|---|
| `mode` | `SqpMajorTraceEvent::mode` (`ipqp\|walk\|ssn`, v0 spelling) | never |
| `verdict` | `verdict` | never (the documented `reject` default on a no-solve row is a real reading, `sqp_solver_types.h:190-197`) |
| `trust_radius` | `tr_radius` | never |
| `violation_l1` | `violation_l1` | never |
| `dual_regularization` | `mu` (renamed, F3(c)) | `!qp_solved` |
| `qp_solved` | `qp_solved` | never |
| `qp_status`, `qp_minor_iterations`, `qp_factorizations` | the three | `!qp_solved` (F3(b)) |
| `tr_binding` | `tr_binding` | never |
| `elastic`, `elastic_clamped` | `elastic_applied`, `elastic_rho0_ceiling_hit` | never |
| `restoration_seed_used` | same | never |
| `ipqp_least_infeasible_primal`, `ipqp_farkas_corroborated` | same | never |
| `active_set_delta`, `weak_active_rows`, `near_active_rows`, `active_rows`, `active_lower_sides`, `active_upper_sides` | same (v0 §6 rules) | never |

v0 `trial` (== `major`) and `kkt_residual` (`max(stationarity, feasibility)`) are dropped as derivable.

**Join and order rules** carry over from v0 §7: QP-tier lines belong to the next `iter` at the same depth,
and a restoration-requesting row follows its nested solve(s). One rule is added: an IPM row joins its phase
events on (`phase`, `iter`).

### 2.5 `solve.begin`

Core: the declared problem's census, **verbatim `ProblemStatistics`** (`model/problem.h:38-54`, read from
`Problem::statistics()` `:129`). This is identical on both engines by construction, and it replaces v0's
two `vars_*` censuses and answers M7-18's row-kind question (AP-1 §10.4). Keys in order: `variables`,
`free_variables`, `lower_bounded_variables`, `upper_bounded_variables`, `boxed_variables`,
`fixed_variables`, `constraints`, `equality_rows`, `lower_bounded_rows`, `upper_bounded_rows`, `range_rows`,
`free_rows`, `absent_bounds`. These are followed by the `CommonOptions` values in force: `optimality_tol`,
`feasibility_tol`, `max_iterations`, `verbosity` (enum), and `hven_version` (string, `core/version.h`).

`ipm` block: `n_reduced`, `me`, `mi` (the solver's counts), `internal_fixed_rows`, `phases`,
`max_acceptable_iterations`, `initial_mu`, `obj_scale`, the four acceptable tolerances (names per
[AP5:Q7]: `acceptable_stationarity_tol`, `acceptable_complementarity_tol`, `acceptable_equality_tol`,
`acceptable_inequality_tol`; fallback: v0 `acc_*_tol` spellings), `inertia_mode`, `restoration_mode`,
`kkt_dim`, `kkt_nnz`. v0's `wide_console` is dropped (Q4).

`sqp` block: `me`, `mi` (the solver's split counts), `qp_mode`, `ws_algebra`, `hessian_mode`
(`SqpOptions::hessian_mode`, new), `row_scaling`, `column_scaling` (the settings, enum).

### 2.6 `solve.end`

Core, in order: `status` (`SolveStatus`, v0 spelling); `stop_reason` (lower-snake `StopReason`
[AP5:§2.3]; fallback: the engine's own `IpmStopReason`/`SqpStopReason` spelled lower-snake under the same
key); `message` (`SolveResult::message`); `iterations`; `returned_best_iterate`;
`infeasibility_certified`; `f`; `kkt` = `{stationarity, feasibility, complementarity}`, the DECLARED set
(`SolveResult`, `solve_result.h:203-208`); `convergence` = the engine measures, whose last value equals the
final `iter` row's core, an identity (§2.8); `work` = `WorkCounters` (`solve_result.h:64-71`, six keys);
`counters` = the engine counter tree, generated from the X-macros (`HVEN_SQP_COUNTERS_FIELDS` and the two
nested tables, as v0 §5; IPM: `HVEN_IPM_COUNTERS_FIELDS` [AP5:G6; fallback: the IPM writes `counters:
null` until G6 lands, and TR-A does not ship the IPM half]); `wall_s` (informational).

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
| the nine v0 IPM kinds (`solver_initialized` … `interrupt_skipping_phases`), **less `restoration_locally_infeasible`** | ipm | v0 console tier: `solver_initialized`, `interrupt_*` notice; rest warning | v0 §4.19 | v0 |
| `restoration_entered` | both | notice | `a` = violation at entry (IPM theta, SQP `h`); `iter` = the requesting row | lines == `counters.restoration_entries` [AP5:G6 for the IPM counter] |
| `restoration_exited` | both | notice; warning unless `s == resumed` | `s` = `resumed\|locally_infeasible\|failed\|non_finite\|interrupted`; `a` = violation at exit; `b` = the threshold judged against (IPM theta threshold; SQP `feasibility_tol`) | one per entered |
| `restoration_refused` | sqp | warning | `s` = `nested\|entry_rule\|budget` | lines == the refusals (V5) |
| `subproblem_failure` | sqp | notice when routed, warning when it ends the solve | `s` = `iteration_limit\|numerical_error\|non_finite`; `k` = failures in a row | lines == `qp_maxiter_exits + qp_numerical_exits + nonfinite_subproblems − nonfinite_box_projections` (V5) |
| `elastic_exhausted` | sqp | warning | `k` = rungs climbed | V5 |
| `inconsistent_equalities` | sqp | warning | — | ≤ 1 |
| `start_point_clamped` | sqp | notice | `k` = `start_point_clamps` | ≤ 1, iff that counter > 0 |
| `scaled_convergence_refused` | sqp | notice | `iter` | lines == `scaled_convergence_refusals` |
| `interrupt_at_iteration` | **both** (new on SQP) | notice | `iter` | ≤ 1 |

`restoration_exited{s: locally_infeasible, a, b}` replaces both `ipm.restoration_exit_row` and the
`restoration_locally_infeasible` kind (F3(e)). The SQP list covers each `SqpStopReason` the driver can
reach mid-solve, plus the routed (non-terminal) cases a user needs to see. Each SQP emit site is held by a
source scan in the pattern of `tests/sqp/test_qp_mode_sites.cpp` (§6, TR-B).

### 2.8 Identities a v1 stream satisfies (asserted in tests)

1. `iter` lines at depth 0 == `iterations` on the SQP (== `history.size()`), and == `iterations` on the IPM
   **with no hole**. M7-19 is closed by emitting the row on the restoration-break path (§5.3).
2. The final depth-0 `iter` core == `solve.end.convergence` (engine measures, caller objective scale).
3. `factorizations` on the last row ≤ `solve.end.work.factorizations`. The difference is the
   factorizations spent after the last row was emitted, for example an IPM inter-phase re-initialization.
   It is pinned per engine on the HS battery. The SQP's polish stage-2 re-solves are outside both
   figures (AP-5 §2.1 coverage caveat).
4. The message identities in §2.7, and v0's QP-tier identities unchanged (v0 §4.4-§4.11).
5. Callback vs trace: one `IterationEvent` per depth-0 `iter` line. SQP fields agree row for row. The IPM
   step fields follow the rule in V1.

### 2.9 Versioning and compatibility

- **One sink writes one version.** `JsonLinesTraceSink` writes v1 only from TR-A on. There is no v0 mode
  and no dual-write, because v0 has no external consumer (F1). In-tree v0 readers are the golden tests,
  `examples/08_results_trace_options.cpp` and `bench/ipm_corpus_leg.cpp`'s counting sink, which counts
  events and does not parse them. All three move in TR-A.
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
| `ipm.iter` | `iter` + `ipm` block. `prim_obj`→core `f`/`f_recovery`. `kkt_inf`→core `stationarity`. `barr_inf`→core `complementarity`. `econ_inf`/`icon_inf`→`ipm.feasibility_eq`/`_ineq` (their max → core `feasibility`). Other names per §2.4's `ipm` table. |
| `ipm.restoration_exit_row` | removed; `message{kind: restoration_exited, s: locally_infeasible}` |
| `ipm.message` | `message`. The `restoration_locally_infeasible` kind folds into `restoration_exited`. |
| `sqp.solve.end`, `ipm.solve.end` | `solve.end`. `majors`→`iterations`. `iters`→`iterations`. SQP scaling keys → `sqp` block. IPM timing → `ipm` block. |
| `ipm.phase.begin` / `.end` | `label` dropped |
| QP-tier events | unchanged after the envelope |

### 2.11 Verification items (TR-A's first step pins each before the schema doc freezes)

| # | item | fallback if it fails |
|---|---|---|
| V1 | **Row timing.** A test pins whether an IPM row's step fields describe the step that reached its point or the step leaving it, and whether the four exit sites' rows are reached by a step at all. The legend text follows the test. | none needed; the legend states what the test shows |
| V2 | The IPM step norm: `last_step_norm` is computed at `ipm_solver.cpp:4672`, after the ordinary emit at `:4525`. The norm of the step the row's alphas describe is computed inside the guard. | none (on-cost only) |
| V3 | IPM per-row `soc`/`watchdog`/`rolled_back`: derived inside the guard as deltas of the solve's counters against a last-emitted snapshot held by the impl and written only inside the guard (zero off-cost). | drop the key by a declared amendment of this page |
| V4 | Running factorization counts readable at both emit sites without new unconditional bookkeeping. | drop `factorizations` from the core by declared amendment |
| V5 | SQP message sites: each refusal, failure and ladder-exhaustion site is located and its identity confirmed against counters. | a kind whose identity has no counter is pinned by the tee test (v0 rule 9) |

---

## 3. Human trace (TR-2)

### 3.1 The line

A fixed-width line per `iter`, in the IPOPT/SNOPT tradition. That tradition is: one line per iteration;
log-scale barrier and regularization columns; a restoration marker; and a trailing column of one-letter
condition codes. The first 59 columns are identical on both engines; engine columns follow. Formats,
in fmt syntax, with `-` right-aligned for `null`:

```
shared   {iter:>5} {t} {obj:>15.8e} {stat:>8.2e} {feas:>8.2e} {comp:>8.2e} {step:>8.2e}
IPM      {log10(mu):>6.2f} {alpha_primal:>8.2e} {alpha_dual:>8.2e} {ls:>2} {lg_rg:>6} {codes}
SQP      {trust_radius:>8.2e} {violation_l1:>8.2e} {qp_minor_iterations:>5} {qp_factorizations:>4} {v} {codes}
```

- `obj` is `f`, or `f_recovery` on `R` rows. The tier letter tells which.
- `lg_rg` is `log10(regularization_primal_total)` to `.1f`, or `-` when it is zero or `null`, as in
  Ipopt's `lg(rg)`.
- `v` is the SQP verdict: `F` accept-f, `H` accept-h, `x` reject, `R` restore, `-` no subproblem.
- Line width is ≤ 100 characters. There is no colour on rows (Q5).

**Tier column `t`** (the brief's mode/tier column):

| `t` | wire `tier` | meaning |
|---|---|---|
| `B` | `bar` | IPM barrier iteration |
| `A` | `acq` | SQP step from the acquisition tier (IPQP) |
| `S` | `ssn` | SQP step from the semismooth Newton tier |
| `W` | `walk` | SQP step from the active-set walk |
| `E` | `elastic` | SQP step supplied by the elastic tier (any arm) |
| `R` | `rest` | restoration iterate: IPM in place, SQP depth-1 sub-solve |

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

A message renders as one line at its stream position: `{'':>5} {mark} {text}`, where `!` marks a warning
and `-` a notice. The text comes from a per-kind template (TR-2 commits the table).

### 3.2 Blocks

- **Header** (kSummary+), 4 lines:
  - the version and engine;
  - the problem census (`ProblemStatistics`);
  - engine facts (IPM: KKT dim/nnz, inertia and restoration modes, phases; SQP: QP mode and algebra,
    Hessian mode, scaling settings);
  - the tolerances and the iteration cap.
- **Legend** (kIterations, once per solve, before the first column header), 3 lines per engine, rendered
  from the tables in §3.1. `Format::legend = false` suppresses it.
- **Column header** before the first row, after every 20 rows, and after any message line. The current
  IPM table repeats every 10 rows, keyed on `iter % 10`, which restarts per phase. v1 counts printed rows.
- **IPM phase lines** (kSummary+): `phase k: <entry> … <status>` from the phase events. The current
  per-phase exit block moves to kDetail.
- **Footer** (kSummary+; the `exit` line also at kWarnings), the brief's exit condition, counters and KKT set:

```
exit        <Status> (<stop_reason>) -- <SolveResult::message>
iterations  <iterations> (restoration <restoration_iters>) · factorizations <work.factorizations>
objective   <f>
KKT         stationarity <kkt.stationarity> · feasibility <…> · complementarity <…>   (declared)
counters    IPM: soc · watchdog · inertia corrections · restoration entries
            SQP: soc · elastic · restoration entries · qp minors · start level · scaling
time        <wall_s> s (informational)                      [Format::timing; goldens set false]
```

### 3.3 The print-level ladder (`CommonOptions::verbosity`)

| rung | both engines |
|---|---|
| `kSilent` | nothing (the default; no console sink is constructed) |
| `kWarnings` | `warning` messages and the footer's `exit` line |
| `kSummary` | + header, IPM phase lines, full footer |
| `kIterations` | + legend, column headers, every `iter` line **including `R` rows on both engines** (Q7), `notice` messages |
| `kDetail` (new, Q6) | + one condensed line per QP-tier event (`qp.mode`, `ipqp.escape`, `fallback.verdict`, `ipqp.restart`), the IPM KKT-analysis block and phase-exit blocks |

### 3.4 Examples

**ILLUSTRATIVE.** These were rendered by hand from the formats above with made-up values. They are not a
solve's output. TR-2's goldens are rendered from real solves (§6).

Interior point, with an in-place restoration excursion:

```
hven 0.2.0 -- interior-point engine
problem    variables 4 (boxed 4) · constraints 2 (equality 1, lower 1) · absent bounds 0
engine     KKT dim 10, nnz 26 · inertia classic · restoration proximal_switch · phases 1
tolerances optimality 1e-06 · feasibility 1e-06 · max iterations 1000
legend  t: B barrier, R restoration | stat feas comp: convergence measures | step: |dx|inf
        lg(mu), lg(rg): log10 barrier, Hessian shift | alpha_pr/du: step lengths | ls: backtracks
        codes: s SOC  w watchdog armed  r rollback  k backtracked  i inertia fix  x eval error  m rejected
 iter t       objective     stat     feas     comp     step lg(mu) alpha_pr alpha_du ls lg(rg) codes
    0 B  2.50000000e+01 1.30e+00 4.00e+00 1.00e-01 4.10e-01  -1.00 1.00e+00 1.00e+00  0      -
    1 B  2.18000000e+01 6.20e-01 1.10e+00 8.40e-02 3.30e-01  -1.00 7.90e-01 1.00e+00  1   -4.0 ik
    2 B  2.07000000e+01 4.10e-01 9.00e-01 6.00e-02 2.00e-02  -1.00 2.50e-02 1.00e+00  3   -4.0 ik
      - restoration entered at iter 3: theta 9.0e-01
    3 R  4.50000000e-01 2.00e-01 9.00e-01 5.00e-02 3.00e-01  -1.00 1.00e+00 1.00e+00  0      -
    4 R  1.20000000e-01 9.00e-02 2.40e-01 3.00e-02 2.60e-01  -1.00 1.00e+00 1.00e+00  0      -
      - restoration exited at iter 5: resumed, theta 2.4e-01
    5 B  1.84000000e+01 8.00e-02 2.40e-01 2.00e-02 1.90e-01  -2.00 1.00e+00 1.00e+00  0      -
    6 B  1.71000000e+01 1.10e-02 1.30e-02 1.50e-03 6.00e-02  -3.00 1.00e+00 1.00e+00  0      - s
    7 B  1.70142000e+01 2.20e-04 3.00e-05 2.00e-05 4.00e-03  -5.00 1.00e+00 1.00e+00  0      -
    8 B  1.70140174e+01 3.10e-09 2.20e-15 2.50e-09        -  -9.00        -        -  -      -
exit        Optimal (none) -- converged to the optimality tolerance
iterations  9 (restoration 2) · factorizations 12
objective   1.70140174e+01
KKT         stationarity 1.2e-09 · feasibility 2.2e-15 · complementarity 2.5e-09   (declared)
counters    soc 1 · watchdog 0 · inertia corrections 2 · restoration entries 1
```

SQP, with an elastic step, a restoration sub-solve and a hand-off to SSN:

```
hven 0.2.0 -- SQP engine
problem    variables 4 (boxed 4) · constraints 2 (equality 1, lower 1) · absent bounds 0
engine     QP mode walk (schur_border) · Hessian exact · row scaling auto · column scaling auto
tolerances optimality 1e-06 · feasibility 1e-06 · max iterations 100
legend  t: A acquisition  S SSN  W walk  E elastic  R restoration | step: |p|inf taken from this point
        radius: trust region | h(l1): l1 violation | qp_it/qp_fa: QP minors/factorizations | v: F H x R
        codes: s SOC  w watchdog restored  r rollback  b TR binding  c elastic clamp  d seed  n QP not optimal
 iter t       objective     stat     feas     comp     step   radius    h(l1) qp_it qp_fa v codes
    0 W  2.50000000e+01 1.30e+00 4.00e+00 0.00e+00 1.00e+00 1.00e+00 6.10e+00     4    2 F
    1 W  2.31000000e+01 9.00e-01 2.20e+00 1.00e-02 1.00e+00 1.00e+00 3.40e+00     3    1 H b
    2 W  2.31000000e+01 9.00e-01 2.20e+00 1.00e-02 1.00e+00 1.00e+00 3.40e+00     5    2 x n
    3 E  1.90000000e+01 4.00e-01 9.00e-01 2.00e-02 5.00e-01 5.00e-01 1.40e+00    11    3 H c
      - restoration entered at iter 4: h 1.4e+00
    0 R  1.40000000e+00 3.00e-01 9.00e-01 0.00e+00 2.00e-01 1.00e+00 1.40e+00     6    2 F
    1 R  2.00000000e-02 1.00e-02 1.50e-02 0.00e+00 4.00e-02 1.00e+00 2.00e-02     3    1 F
    2 R  0.00000000e+00 3.00e-09 1.00e-12 0.00e+00        - 1.00e+00 1.00e-12     -    - -
      - restoration exited at iter 4: resumed, h 1.0e-12
    4 W  1.86000000e+01 3.80e-01 8.50e-01 0.00e+00 6.00e-01 5.00e-01 1.40e+00     4    1 R d
    5 S  1.70260000e+01 2.00e-03 4.00e-06 0.00e+00 1.20e-02 1.00e+00 4.00e-06     2    1 F s
    6 S  1.70140174e+01 4.40e-10 1.10e-16 0.00e+00        - 2.00e+00 1.10e-16     -    - -
exit        Optimal (none) -- converged to the optimality tolerance
iterations  7 (restoration 3) · factorizations 15
objective   1.70140174e+01
KKT         stationarity 4.4e-10 · feasibility 1.1e-16 · complementarity 0.0e+00   (declared)
counters    soc 1 · elastic 1 · restoration entries 1 · qp minors 41 · start level cold · scaling off
```

Row 4 prints after its sub-solve because a restoration-requesting row is pushed with its final values
(v0 §4.3, kept). The `restoration entered` notice is what puts the excursion in reading order.

---

## 4. Cost contract: zero cost when tracing is off

**"Off"** means no sink attached and `verbosity == kSilent`.

| rule | how it holds | evidence today |
|---|---|---|
| C1. Every emit is one pointer load and test; the event is built inside the guard | the existing pattern, extended to every new site (`message` sites, row-flag snapshots) | F4 |
| C2. No unconditional new bookkeeping | V3's snapshots and V2's norm are written only inside the guard. `factorizations` reads an existing counter (V4). A field that would need unconditional state is dropped (V3/V4 fallbacks). | — |
| C3. No console object at `kSilent` | unchanged composition | `ipm_solver.cpp:6212-6225`, `sqp_solver.cpp:3292-3308` |
| C4. Formatting cost lives in sinks | `JsonLinesTraceSink` builds strings; `ConsoleTraceSink` formats. Neither exists when off. | `trace_writer.cpp:783-803` |
| C5. Any history added (TR-D) is a RESULT feature, priced separately | see TR-D's own gate (§6) | — |

**Proof, per group that touches an engine TU** (thresholds frozen in the group's declaration before its
first leg, per brief §0):

1. **Structural.** A source-scan test fails when any `on_*` call in `src/drivers/` or `src/qp/` is not
   lexically inside a `!= nullptr` guard. It extends `tests/sqp/test_qp_mode_sites.cpp`'s mechanism, and
   its falsification is a planted unguarded call.
2. **Counters off.** The 27-cell replay × 3 engines, byte-exact against the pre-group pin, with no sink
   (AP-2 proof 5).
3. **Counters on.** The same cells with a `JsonLinesTraceSink` attached and verbosity `kDetail` give the
   same counters and statuses. The trace is observation.
4. **Instructions off.** Pass A/B instruction counts (`instructions:u`) with no sink, on the IPM corpus leg
   (`bench/ipm_corpus_leg.cpp`) and the SQP corpus per mode. Proposed gate: aggregate |Δ| ≤ 0.1 % and no
   cell beyond 0.5 %. Instruction counts are deterministic, so they bound "zero" more tightly than
   wall-clock can.
5. **Runtime off.** The P5 serial runtime gate (the `6a9fbfe39` instrument: filtered B/A per mode within
   ±0.5 %), run solo under CLAUDE.md §7.
6. **On-cost** of the JSON sink and the console at each rung is reported as informational, never gated.

---

## 5. TR-3: the consumers

### 5.1 hven-bench (the HB lane executes; hven-bench is read-only from hven)

Because of F1, the migration is about what the lane reads, not about parsing a trace format:

1. **Pin bump** to the TR-E head, as a declared pin event in `cmake/hven_pin.cmake`. This is the one move
   that also absorbs AP-5 G6's IPM renames.
2. **Counters through `dump_counters`.** It writes the same JSON object as `solve.end.counters`, from the
   same X-macro generator. It replaces the hand enumerations (`hven_bench_cell.cpp:766-800` for the IPM,
   and the per-pin `add_*_counts` blocks `:810-1350` for the SQP) with one generic loop. The side-file
   column names change to the v1/AP-5 names: a declared harness schema event, with an old→new map.
3. **Evaluation columns.** `eval_f/g/jac/hess` are `null` on both hven lanes today (hven-bench
   `schema/v1.md` cols 18-21). They fill from `SolveResult::work` (`solve_result.h:64-71`), as a
   declared harness schema event.
4. **Optional per-cell trace.** A lane flag writes the v1 stream to `<run>/traces/<cell_id>.jsonl` for TU
   runs. A traced cell carries a `traced` pin column, so traced and untraced rows are never compared
   unknowingly. `wall_s` is informational in either case.
5. **Reader.** `schema/trace_v1.py` validates `v`, the `seq` contiguity, the begin/end pairing per `sid`,
   and the absence spelling. Its self-test fixtures are hven's golden lines, copied with their provenance.
   It is the public reader friction #29 asked for (AP-1 §6).
6. **`probe1500-trace-` transcripts** (`hven_bench_cell.cpp:380-386`) change format at the pin bump.
   They are used for quoting only, and old transcripts stay as recorded.
7. **Recommended (HB's call):** rename the harness CSV's "trace schema v1" to "row schema v1" so that "v1"
   names one thing.

### 5.2 M7-4's residual (brief §TR; the A6 list's group 3)

M7-4's residual is the +0.6 % IPM attached-callback instruction overhead left unattributed after the
`IterationPayload` mask (`docs/notes/2026-09-m7-ledger.md:280-300`). Re-measure it on the rebuilt payload
at the TR-E head, with W0 T2's instrument: callback attached at `kScalars` against no callback, on the
IPM corpus leg, pass A/B instructions. If it lands within ±0.5 %, M7-4 closes; otherwise it is registered
with its number. If AP-5 G5 rebuilds the event fill [AP5:G5], the measurement runs after G5, so it reads
the payload that ships.

### 5.3 M7-19 (rides TR-B)

v0 §4.14's hole: under the locally-infeasible restoration break, the driver leaves without a final row.
v1 identity 1 (§2.8) requires that path to emit its row and its `restoration_exited` message. AP-5 §2.1
places M7-19 as its own declared commit after G6 [AP5:G6]. Whichever lands first takes the code change;
TR-B owns the identity test either way.

---

## 6. Landable groups

Effort is in AP-1 §13's units, as in AP-5 §5. Every group carries AP-2's common six. No group can move
the walk (§4 proof 2/3), so none needs a census. All groups land after DN-3 proper and after AP-5 G5/G6
where marked, and **before TU-1** (brief §1 (6)).

| group | content | behaviour change | proof beyond the common six | est. |
|---|---|---|---|---|
| **TR-A** schema + writer | V1-V4 pinned first. `IterationRecord` and the engine blocks; the 4 shared records; the envelope v1; the v0 writer retired; `docs/trace-schema-v1.md` (exit criterion 8) with its freeze table; v0 doc header pointer; example 08 and the bench counting sink moved. [AP5:G5, G6] | trace bytes only (declared: every golden line re-derived) | a golden line per record and block (hand-filled structs, aggregate-arity `static_assert` per struct, as v0); identities 1-5 on the HS battery, both engines; §4 proofs 1-5 | 2.5 |
| **TR-B** messages | SQP kinds; `restoration_entered/exited` on both engines; the IPM fold; message-site source scan; M7-19 identity test | none (observation) | §2.7 identities on HS plus the restoration fixtures (`test_callback.cpp:668`, `test_evaluation_failure.cpp:886`); scan falsified by a planted site; §4 proofs 1-4 | 1.5 |
| **TR-C** human trace | `ConsoleTraceSink` re-cut; legend, header and footer; the ladder (incl. `kDetail` if Q6); colour policy (Q5); retire `wide_console`, the IPM banner and `format_iteration_table` (Q4) | console bytes (declared; the 159/119-column tables go) | **human-trace goldens** rendered from real solves: 4 HS problems × 2 engines × 4 rungs, plus one restoration and one elastic fixture, `Format{color: kNever, timing: false}`; one golden per message template; §4 proof 5 | 2 |
| **TR-D** IPM history [AP5:Q8] | `IpmResult::history` as `std::vector<IterationRecord>`, built from the impl's existing per-phase `iters` at exit (O(iterations), once) | new result field | `history` == the depth-0 `iter` stream, row for row, both engines; replay byte-exact | 1 |
| TR-D' (if Q1 = full) | `SqpResult::history` → `std::vector<IterationRecord>`; `SqpIterate` retires | **break** (mechanical: `h.tr_radius` → `h.sqp->trust_radius`) | replay byte-exact; every history reader in tests re-pointed; migration-guide lines | 2 |
| **TR-E** consumers (TR-3) | hven: `dump_counters` × 2 and the v1 reader fixtures; hven-bench (HB lane): §5.1 items 1-6; M7-4 re-measure | harness schema events (declared, in hven-bench) | bench comparator self-tests; one TRAIN leg compared old pin vs new pin through the column map, 0 counter diffs; M7-4 verdict | 2 |
| | **total** | | | **9 (11 with TR-D')** |

**Order:** TR-A → TR-B → TR-C → TR-E. TR-D lands after TR-A. TR-A, TR-B and TR-C revert in reverse
order; TR-D and TR-E revert alone.

---

## 7. Public names for approval

| name | kind | new / break |
|---|---|---|
| `IterationRecord` | struct (the v1 row: core + `ipm`/`sqp`) | new (Q1) |
| `IterationTier {kBarrier, kAcquisition, kSsn, kWalk, kElastic, kRestoration}` | enum; wire `bar\|acq\|ssn\|walk\|elastic\|rest` | new |
| `IpmIterationInfo`, `SqpIterationInfo` | structs | AP-5 names, extended with the §2.4 fields [AP5:§3.1] |
| `SolveBeginTraceEvent`, `IterTraceEvent`, `MessageTraceEvent`, `SolveEndTraceEvent` | trace event structs | **break**: replace `Sqp/IpmSolveBeginTraceEvent`, `SqpMajorTraceEvent`, `IpmIterTraceEvent`, `IpmMessageTraceEvent`, `Sqp/IpmSolveEndTraceEvent` |
| `TraceSink::{on_solve_begin, on_iter, on_message, on_solve_end}` | virtuals (no-op defaults) | **break**: replace the six `on_sqp_*`/`on_ipm_*` pairs; `on_ipm_restoration_exit_row` removed |
| `MessageKind`, `MessageSeverity {kNotice, kWarning}` | enums | **break**: `IpmMessageKind` → `MessageKind` (its values plus §2.7's; `kRestorationLocallyInfeasible` removed) |
| `JsonLinesTraceSink::kSchemaVersion` | constant (= 1) | new |
| `ConsoleTraceSink::Format::{color, legend, timing}`, `ConsoleColor {kAuto, kAlways, kNever}` | fields, enum | new; `Format::wide` removed (break) |
| `Verbosity::kDetail` | enumerator | new (Q6) |
| `IpmOptions::wide_console` | field | **removed** (Q4) |
| `format_iteration_table` and its five pieces (`sqp_solver.h:294-325`) | functions | **removed** (Q4); `render_history(const SolveResult-derived&)` if a post-hoc table is wanted |
| `dump_counters(const IpmResult&, std::ostream&)`, `dump_counters(const SqpResult&, std::ostream&)` | functions | new (Q11) |
| `IpmResult::history` | field (`std::vector<IterationRecord>`) | new [AP5:Q8] |
| `SqpResult::history` element type | field | **break** if Q1 = full |
| `tools/trace_v1.py` (hven) / `schema/trace_v1.py` (bench) | tooling | new (not C++) |

Detail-internal (not for approval): the IPM row builder, the counter-delta snapshot, the message-site scan,
and the per-kind message templates.

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
| Q8 | Put the declared (caller-unit) diagnostics on every row as well? | No. On the IPM they cost the O(n+m) expansion the callback pays only under `kDiagnostics`. The rows carry engine measures, and the declared set is on `solve.end.kkt` and on the callback. |
| Q9 | Per-row wall time? | No. It keeps a deterministic solve's stream byte-reproducible with three `_s`-bearing records masked. |
| Q10 | Ask the HB lane to rename the harness CSV's "trace schema v1"? | Yes (§5.1 item 7). |
| Q11 | Ship `dump_counters` (the harness's standing request)? | Yes. It is the same generator as `solve.end.counters`, so there is no second bookkeeping path. |
| Q12 | Tier letters and wire spellings as in §3.1? | Approve. They are the brief's five modes plus the IPM's `B`. |

---

## Appendix A. Dependencies on the AP-5 page

| marker | depends on | effect if not approved |
|---|---|---|
| [AP5:§3.1] | `IpmIterationInfo`/`SqpIterationInfo` names; `IterationEvent::recovering` | TR-1 introduces the same names itself; `recovering` stays trace-only |
| [AP5:§2.3] | `StopReason` | `solve.end.stop_reason` spells the engine's own enum, under the same key |
| [AP5:Q7] | the IPM tolerance renames | `ipm` block keeps the v0 `acc_*_tol` spellings |
| [AP5:G5] | the event-fill rebuild | M7-4 is measured at TR-E regardless; after G5 if G5 lands |
| [AP5:G6] | `IpmCounters`, `HVEN_IPM_COUNTERS_FIELDS`, `counters.restoration_entries` on the IPM | IPM `solve.end.counters` is `null`; the IPM `restoration_entered` identity is pinned by the tee test; TR-A ships the SQP half |
| [AP5:Q8] | `IpmResult::history` | TR-D is not built; the row schema still serves the trace |
| AP-5 M1 | structured SQP messages | owned here (§2.7) |
