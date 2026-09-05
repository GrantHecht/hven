# hven machine-trace schema v0

The JSON-lines event stream `hven::solvers::JsonLinesTraceSink` writes
(`include/hven/drivers/trace_writer.h`, `src/drivers/trace_writer.cpp`). **This
document is the schema.** M7's heuristics and M8's human trace read it, and
`docs/notes/2026-08-m6-w1-ipqp-spec.md` §7's block is superseded by reference.

Everything below is MEASURED ON MKL / LINUX. Where a field's value depends on the
sparse backend, the Apple/Accelerate reading is **UNOBSERVED** and is recorded as
such, never inferred (CLAUDE.md §6).

The stream is **instrumentation**. Counters and statuses in it are assertable;
every wall-clock field is informational and no pin reads one (CLAUDE.md §7).

---

## 1. The envelope

Every line is one JSON object, `\n`-terminated, no pretty-printing, no padding.
The first four keys are always these, in this order:

| key | type | meaning |
|---|---|---|
| `v` | integer | schema version. **0** for the whole of M6. |
| `ev` | string | the event name, from §3's table. |
| `seq` | integer | **per SINK**, not per solve: a monotone counter starting at 1, so a reader can prove it saw every line. Two solves on one sink share one numbering. |
| `depth` | integer | the number of ENCLOSING `sqp.solve` brackets. 0 at the top level; the restoration sub-solve's lines — its own `sqp.solve` pair included — read 1. |

`seq` and `depth` are **sink-owned**: no event struct carries either. `depth`
moves on `sqp.solve` begin/end only; the `ipm.solve` pair does **not** move it
(the interior-point driver nests no driver of its own).

## 2. Types, absence, and text

**Doubles** are written with `fmt`'s `{:.17g}`. "17 significant digits" is a
**round-trip property of the text**, not a count of printed digits: `0.5` prints
as `0.5`, and what is guaranteed is that parsing the text back yields the same
binary64 (pinned over −0.0, a subnormal, 0.1, 1e-300 and DBL_MAX, plus a
1,409,400-pattern sweep at W4 T1). JSON has no spelling for the non-finite
values, so a double field's type is **"number, or one of the three strings
`nan` / `inf` / `-inf`"** — a sentinel number would be a fabricated value.

**Integers** are 64-bit (`hven::Index`). A reader must parse them as int64: a
JavaScript reader silently loses precision above 2^53; Python does not.

**Bools** are `true` / `false`.

**ABSENCE IS `null`** — never omitted, never zero-filled (CLAUDE.md §6). A key
that can be absent is always present with the value `null`, so "absent" and "not
in this event" are never confused. §4 names, per field, the sentinel each `null`
maps from and what that absence MEANS; two fields can both read `null` for
different reasons and a reader that collapses them loses the distinction.

**Strings** are escaped per RFC 8259 §7 (the two mandatory escapes, the five
short forms, and `\u00xx` for the rest of C0). Bytes above 0x1f pass through
unchanged: `facts` is opaque CALLER text and **must be valid UTF-8** — a caller
that hands the sink ill-formed UTF-8 gets a line that is not JSON text (RFC 8259
§8.1). The library never sets the field; it is always `""` today.

**Enum strings** are lower snake. **No reader should ever see `"unknown"`.** The
serializer's switches carry no `default` label, so `-Wswitch` warns when an
enumerator is added without a spelling — but this tree carries no `-Werror`
(CLAUDE.md §7's uniform flag regime), so the `"unknown"` fallthrough remains
REACHABLE from a library-only build that ignores the warning. The net is
test-side: `-Wswitch` is an ERROR over one exhaustive switch per enum in
`tests/sqp/test_trace_writer.cpp`, plus a no-`"unknown"`-line assertion over
every whole-solve stream. `"unknown"` in an artifact means the producing build
ignored a compiler warning.

**Vectors.** Schema v0 carries **no vector-valued field anywhere** (settler
ruling, W4 T1). `ipqp.escape` therefore carries spec §6.3's least-infeasible
point's two SCALARS, never the point; `ipm.iter` carries none of the callback's
`XSL` / `RHS`.

**Nesting.** Three events carry a nested object: `ipqp.escape`'s `evidence`,
`sqp.solve.end`'s `counters`, and `counters`' own `ssn` / `ipqp` sub-objects.
`evidence` is **not flattenable** — `fired` and `window` appear in both of its
blocks with different meanings.

## 3. The events

| `ev` | written by | one per |
|---|---|---|
| `sqp.solve.begin` | `SqpDriver::solve` | one accepted call |
| `sqp.solve.end` | `SqpDriver::solve` | one normal return |
| `sqp.major` | `SqpDriver`'s `push_history` | one exported `SqpSolution::history` row |
| `qp.mode` | `SqpDriver` | one KERNEL INVOCATION |
| `ipqp.iter` | `IpqpEngine` | one predictor+corrector pair |
| `ipqp.reg` | `IpqpEngine` | one `(rho, delta)` schedule move |
| `ipqp.restart` | `IpqpEngine` | one warm restart |
| `ipqp.route` | `SqpDriver`'s kIpm arm | one routing decision |
| `ipqp.certify` | `SqpDriver`'s kIpm arm | one certification READ actually taken |
| `ipqp.escape` | `IpqpEngine` | one tier escape |
| `fallback.verdict` | `certified_feasibility_fallback` | one ENTRY, fired or not |
| `ipm.solve.begin` | `InteriorPointSolver` | one public entry point call |
| `ipm.solve.end` | `InteriorPointSolver` | one normal return |
| `ipm.iter` | `InteriorPointSolver` | one interior-point iteration |

## 4. Every event, every field

Key order below **is** the wire order.

### 4.1 `sqp.solve.begin`

Written after the call's arguments are ACCEPTED — argument validation is hoisted
ahead of it, so a refused `x0` writes no line at all and cannot leave a sink's
nesting stuck open.

| key | type | unit / meaning |
|---|---|---|
| `n` | integer | primal variables |
| `me` | integer | equality rows (`NlpModel::eval_ce`, = 0) |
| `mi` | integer | inequality rows (`NlpModel::eval_ci`, ≤ 0) |
| `vars_free` | integer | both box sides infinite |
| `vars_lower_only` | integer | finite lower, infinite upper |
| `vars_upper_only` | integer | infinite lower, finite upper |
| `vars_ranged` | integer | both finite and NOT equal |
| `vars_fixed` | integer | both finite and equal (a zero-width box) |
| `qp_mode` | string `ipqp\|walk\|ssn` | the SETTING, not an outcome |
| `ws_algebra` | string `refactorize\|schur_border` | `SqpOptions::qp.ws_algebra` |

**The five-way census is over VARIABLES, exhaustive and disjoint**: the five
counts sum to `n`. There is **no row-kind census** on this contract and none is
invented: the SQP model contract is the Level-1 SPLIT form (`eval_ce` = 0,
`eval_ci` ≤ 0, bounds on variables only), so the five NLP row kinds cannot be
derived here. That census is REGISTERED for M7's model-contract work.

### 4.2 `sqp.solve.end`

| key | type | meaning |
|---|---|---|
| `status` | string `optimal\|max_iter\|infeasible\|numerical_error\|budget_exhausted` | `SqpStatus` |
| `majors` | integer | `SqpCounters::major_iters` |
| `counters` | object | the whole of `SqpCounters`, §5 |

Written on every NORMAL exit. **A solve that leaves by an exception writes its
`begin` and no `end`** — the honest record of one. A caller that intends to keep
using the sink calls `JsonLinesTraceSink::reset_nesting()` after catching, which
forgets the open nesting (`depth` only; `seq` is never reset, because
renumbering would hide the lines the abandoned solve did write).

### 4.3 `sqp.major`

The exported history row, VERBATIM, in `SqpIterate` declaration order, in CALLER
units (the event is emitted inside `push_history` after its scaling map), plus
two trailing keys.

| key | type | notes |
|---|---|---|
| `trial` | integer | trial steps this major took |
| `f` | double | objective |
| `stationarity`, `feasibility`, `complementarity`, `kkt_residual`, `violation_l1` | double | the residual block |
| `tr_radius`, `mu`, `step_norm` | double | |
| `qp_solved` | bool | |
| `ipqp_least_infeasible_primal` | double | recorded from the escape evidence |
| `ipqp_farkas_corroborated` | bool | |
| `qp_status` | string `optimal\|max_iter\|infeasible\|numerical_error` | |
| `qp_minor_iters`, `qp_factorizations` | integer | |
| `tr_binding` | bool | **the reader's separator for the census below** |
| `verdict` | string `accept_f\|accept_h\|reject\|restore` | |
| `soc_applied`, `elastic_applied`, `elastic_rho0_ceiling_hit`, `restoration_seed_used`, `watchdog_restored` | bool | |
| `active_set_delta` | integer | §6 (a) |
| `weak_active_rows`, `near_active_rows` | integer | §6 (b) |
| `active_rows`, `active_lower_sides`, `active_upper_sides` | integer | §6 (c) |
| `major` | integer | this row's index in `SqpSolution::history` — the stream and the vector share one numbering |
| `mode` | string `ipqp\|walk\|ssn` | **THE DISPATCH ARM that produced this row's step** |

**`sqp.major` count == `SqpSolution::history.size()`.** One event per row, from
all ten push sites, and the exactly-once guard is asserted in the driver at both
ends of a major.

**`mode` is not `qp.mode`'s reading.** This field records which ARM owned the
major; `qp.mode` records an INVOCATION. They agree at kWalk and kSsn. Under kIpm
they differ on exactly the majors the tier routes to the SSN warm grade: the row
reads `ssn` while the major's DISPATCH `qp.mode` line reads `ipqp` with
`outcome` `routed`. A kernel that runs INSIDE the kIpm arm — the fallback's rung
B, either elastic ladder, the SOC re-solve — leaves the row at `ipqp`. On a row
with `qp_solved == false` no kernel ran and `mode` reports the solve's
CONFIGURED mode; the same line's `qp_solved` tells the two apart.

**A restoration-requesting row's `sqp.major` line FOLLOWS the nested solve** it
asked for. The row is pushed with its FINAL values, and `restoration_seed_used`
is only known after the sub-solve returns — so a depth-0 bracket can contain a
whole depth-1 solve, and a reader joining on `seq` must expect it.

### 4.4 `qp.mode`

| key | type | notes |
|---|---|---|
| `mode` | string `ipqp\|walk\|ssn` | which KERNEL ran |
| `outcome` | string `optimal\|routed\|escaped` | the map below |
| `facts` | string | opaque, always `""` |
| `iters` | integer | this invocation's own minor count |
| `site` | string `dispatch\|ssn_warm_grade\|fallback_rung_b\|elastic_rung\|soc_resolve` | which CALL SITE ran it (M6 W4 T5) |

**THE OUTCOME MAP, per kernel** — three kernels report three different objects:

* `walk` — `QpStatus::kOptimal` is `optimal`; kMaxIter, kInfeasible and
  kNumericalError are all `escaped`. The walk has no successor kernel, so
  nothing it exits with is a route.
* `ssn` — a usable, certified exit is `optimal`; every other exit (engine escape
  or the trust-region gate's refusal) is `routed`, since the walk re-solves the
  subproblem. `escaped` is UNREACHABLE in both SSN sites today.
* `ipqp` — `optimal` on the refine route, `routed` to the SSN warm grade,
  `escaped` on a genuine escape to the walk.

**THE FIVE SITES, and the counter each reconciles against.** Every kernel
invocation in `src/drivers/sqp_driver.cpp` writes a line; the list is held in
place by a SOURCE SCAN (`tests/sqp/test_qp_mode_sites.cpp`) that fails when a
call site is added without an emit or an explicit `// trace:` marker.

| `site` | the call | count identity |
|---|---|---|
| `dispatch` | the major's own arm: the walk invocation, the kSsn arm, the kIpm routing chain | between 1 and 2 per major with `qp_solved` — one for the arm, one more when an arm hands off to the walk |
| `ssn_warm_grade` | `route_through_ssn_warm_grade`'s SSN call | `== ipqp_to_ssn` |
| `fallback_rung_b` | `certified_feasibility_fallback`'s cold walk, at both return sites | `== unfired entries + ipqp_fallback_rung_b` |
| `elastic_rung` | `run_elastic_ladder`'s walk, ONCE PER RUNG | `== elastic_activations + elastic_escalations` |
| `soc_resolve` | the second-order correction's re-solve | `== soc_steps` |

`elastic_rung` is why `site` exists: a ladder's climb is UNBOUNDED per major, so
no per-major reading can reconcile the walk invocations a solve made. (HS10 at
kSsn: 14 majors, 8 ladders, 48 escalations, 56 `elastic_rung` lines.)

**Two calls are excluded, by kind.** `QpEngine::refine_on_face` is the tier-3
face EQP run on a face a kernel already produced, not a kernel the dispatch
chooses between, and it reports no minor count for `iters` to carry. The
restoration sub-solve is a whole nested SQP solve and writes its own stream,
dispatch lines included, at depth 1.

**`qp.mode` sums to no counter of its own** — see §7's tee note.

### 4.5 `ipqp.iter`

| key | type | notes |
|---|---|---|
| `solve` | integer | the engine instance's own per-attach solve counter |
| `major` | integer | the SQP major this subproblem belongs to (driver-set) |
| `it` | integer | 1-based iteration within this solve |
| `mu` | double | barrier parameter |
| `rho` | double | `rho_sched + rho_dem`, the total primal shift in force |
| `delta` | double | the final (possibly escalated) dual shift |
| `res_p` | double | `max(primal_eq, primal_iq)` |
| `res_d` | double | stationarity |
| `res_c` | double | complementarity |
| `sigma` | double | Mehrotra's centering parameter |
| `alpha_p`, `alpha_d` | double | fraction-to-boundary steps |
| `inertia` | `[np, nn, nz]` or **`null`** | ABSENT when the read was unavailable or unreadable. Never `[0,0,0]` — a zero-filled read would be indistinguishable from a real one. |
| `zero_derived` | bool | `InertiaEvidence::zero_is_derived`; meaningful only when `inertia` is non-null |
| `perturbed` | integer or **`null`** | perturbed-pivot count. ABSENT when the backend reports none — **Apple Accelerate, UNOBSERVED here** — never zero-filled. |
| `facts` | string | opaque, always `""` |

**`ipqp.iter` count == `ipqp_iters`** over a solve.

### 4.6 `ipqp.reg`

`dir` (`down\|up`), `rho` (double), `delta` (double), `reason`
(`accept\|inertia\|stall\|floor`).

**`ipqp.reg` count == `ipqp_reg_increases + ipqp_reg_decreases`**: the engine has
exactly three emit sites and each sits beside its own increment — the ladder's
climb (`inertia`, up), the evidence-failure conservative floor (`floor`, up),
and the proximal-centre gate's applied decrease (`accept`, down). **`stall` is
spelled but UNREACHED today**: no site emits it, and the spelling is kept
because it is the spec's.

### 4.7 `ipqp.restart`

`grade` (`cold\|base\|full`), `repaired` (bool), `shift_p`, `shift_d`, `mu0`,
`mu_payload` (double), `adopted`, `abandoned` (bool). `shift_p`/`shift_d` are the
§5 SAY shift's own `delta_p`/`delta_d`, and are `0.0` when that shift never ran.

### 4.8 `ipqp.route`

`to` (`refine\|ssn\|walk`), `uncertain` (integer, `ipqp_face_uncertain` for this
subproblem), `face_rows`, `face_bounds` (integer). One per consulted subproblem,
emitted together with its `dispatch` `qp.mode` line.

### 4.9 `ipqp.certify`

`final_inertia` (`ok\|wrong\|unreadable`), `downgraded` (bool). Emitted **only
when a read was actually attempted** — `ipqp_final_inertia_read` values 0/1/2,
never the "not performed" value 3.

### 4.10 `ipqp.escape`

`reason` (`budget\|stall\|indefinite\|numerical\|infeasible_suspect`), then
`evidence`, a nested object of two blocks in declaration order:

* `stall`: `fired` (bool), `window` (integer), `mu_ratio`,
  `residual_improvement`, `min_alpha`, `max_step_alpha` (double).
* `infeasibility`: `fired`, `exhaustion_route` (bool), `window` (integer),
  `primal_start`, `primal_end`, `primal_improvement`, `dual_norm_start`,
  `dual_norm_end`, `dual_growth`, `dual_step_growth` (double),
  `farkas_corroborated` (bool), `farkas_residual`, `farkas_gap`,
  `least_infeasible_primal`, `least_infeasible_mu` (double).

`stall.fired` iff `reason == stall`; `infeasibility.fired` iff `reason ==
infeasible_suspect`. The evidence block's five `Vec` members are **out of the
schema** — v0 carries no vector-valued field — so the last two doubles above are
the least-infeasible POINT's two scalars, not the point.

**`ipqp.escape` count == `ipqp_escapes`.**

### 4.11 `fallback.verdict`

| key | type | notes |
|---|---|---|
| `entered_rung_a` | bool | |
| `verdict` | string `disproved\|relaxed\|exhausted\|rung_b\|unfired` | |
| `rho_0` | double or **`null`** | the LAST rung-A attempt's first-rung penalty (the RETRY's when `floor_retry`). ABSENT — not zero-filled — with no rung A. |
| `rho0_ceiling_hit` | bool | THIS ENTRY's placement was clamped, by headroom or by `dual_mu` |
| `floor_retry` | bool | a declined rung A above the floor was re-run once at it |
| `qp_minor_iters`, `qp_factorizations` | integer | the reported ladder's stopping rung's counts; 0 with no rung A. A CLASSIFICATION, not a cost record. |

**ONE EVENT PER ENTRY, fired or not.** The counts satisfy:

```
fallback.verdict lines           == ipqp_to_walk − ipqp_declined_pinned
disproved + relaxed + exhausted + rung_b  == fired
unfired                          == lines − fired
fired                            == elastic_from_ipqp_escape − elastic_floor_retries
disproved                        == ipqp_suspicion_disproved
rung_b                           == ipqp_fallback_rung_b
```

The first line is the one a reader gets wrong: **a PINNED decline routes to the
walk without entering the fallback**, so it moves `ipqp_to_walk` and writes no
verdict line. `kUnfired` is OUTSIDE the four-way partition and charges nothing —
it is its own value so that summing the stream by `verdict` reproduces the
counters without also filtering on `entered_rung_a`.

### 4.12 `ipm.solve.begin`

| key | type | notes |
|---|---|---|
| `n` | integer | declared primal variables (the CALLER's space) |
| `n_reduced` | integer | variables the solver iterates in; differs from `n` exactly when the fixed-variable treatment eliminated one |
| `me` | integer | **the SOLVER's count** — the caller's equality rows PLUS the internal fixing rows under the MakeConstraint treatment |
| `mi` | integer | the solver's inequality rows |
| `vars_free`, `vars_lower_only`, `vars_upper_only`, `vars_ranged`, `vars_fixed` | integer | the SAME five keys, same order, same arithmetic as `sqp.solve.begin` — one census, one spelling |
| `phases` | integer | phase steps this entry point REQUESTED; a conditional step later skipped still counts |
| `max_iters`, `max_acc_iters` | integer | the per-phase cap, and the ACCEPTABLE run length |
| `kkt_tol`, `econ_tol`, `icon_tol`, `bar_tol`, `init_mu`, `obj_scale` | double | settings captured for this call |
| `inertia_mode` | string `classic\|proximal_regularization` | |
| `restoration_mode` | string `off\|proximal_switch\|l1_nested` | |

### 4.13 `ipm.solve.end`

`status` (`converged\|acceptable\|not_converged\|diverging\|singular_kkt`,
`ConvergenceFlags`), `iters` (integer, `SolveResult::iter_num_` summed over the
phases), then the driver's FULL timing set: `total_time_s`, `pre_time_s`,
`func_time_s`, `kkt_time_s`, `print_time_s`, `solver_init_time_s`,
`misc_time_s`. **Every `_s` field is wall-clock and INFORMATIONAL** (§7 below);
the set is complete rather than a subset, so a reader cannot mistake the parts
for the whole.

### 4.14 `ipm.iter`

`IterateInfo`'s own fields in declaration order with the trailing underscores
dropped, plus one trailing key.

`iter`, `mu`, `prim_obj`, `barr_obj`, `kkt_inf`, `barr_inf`, `econ_inf`,
`icon_inf`, `pen_par1`, `pen_par2`, `ls_iters`, `alpha_p`, `alpha_d`, `alpha_t`,
`h_pert`, `h_facs`, `h_pert_cum`, `prox_reg_primal`, `prox_reg_dual`,
`p_pivots`, `max_e_mult`, `max_i_mult`, `merit_val`, `accepted`,
`first_rejection_iter`, `theta_at_first_rejection`, `eval_exceptions`, `phase`.

`h_facs` is **inertia-ladder STEPS, not factorizations** — the count of
perturbation steps that iteration's factorization took, 0 when it needed none.
(`IterateInfo` carries no doc comment for it; this is its first definition.)

`phase` is additive (M6 W4 T4) and is the IPM's own `major`: `IterateInfo::iter_`
restarts at 0 in every phase, so on a multi-phase entry point it is not a key on
its own.

**SIX KEYS CAN READ `null`, FOR FIVE DIFFERENT REASONS.** A reader that collapses
them loses the distinction:

| key | `null` means | and what a real 0 means |
|---|---|---|
| `prox_reg_primal` | proximal regularization is OFF, **or** this iteration never factorized (the converge-check exit keeps both −1 defaults even with proximal mode on) | a shift of zero |
| `prox_reg_dual` | the same two | a shift of zero (including the suppression inside a nested l1 restoration phase) |
| `first_rejection_iter` | no rejection was recorded this line search | **the FIRST trial was rejected** |
| `theta_at_first_rejection` | no rejection recorded, **or** the LANG acceptance variant ran, which records no theta | a feasible reading of zero |
| `h_facs` | the Newton direction came back non-finite, so no inertia ladder ran and there is no step count | the factorization needed no ladder step (and an unfactorized converge-check record honestly took none) |
| `p_pivots` | no perturbed-pivot count was OBSERVED: the backend keeps none (**Apple Accelerate — UNOBSERVED here**), or this iterate was never factorized | the backend observed zero perturbed pivots |

`p_pivots` deserves its own sentence. The solver's own bookkeeping projects an
absent backend count to the integer 0 (`.value_or(0)`); the stream must not
repeat that fabrication, so `IterateInfo::p_pivots_observed_` is the predicate
and it carries no key of its own.

**BOTH LATE-CALLBACK SITES EMIT**, immediately before the callback, so the late
callback is the event's oracle. The two sites are one loop's two exits: the
converge-check early exit, which leaves the iterate unfactorized, and the
ordinary end-of-iteration site. A line from the first carries `barr_obj`,
`merit_val` and `ls_iters` at 0 with all three alphas at 1 — the fields
`fill_iter_info` never got to write.

**`ipm.iter` count == the IPM's iteration count**, with ONE hole: under the
locally-infeasible restoration break the driver leaves the loop by a path that
writes no final record. That break is dead at the default
`restoration_mode == off`.

## 5. The `counters` object

`sqp.solve.end`'s `counters` is the whole of `SqpCounters`, GENERATED from three
X-macro tables in `include/hven/core/solver_counters.h`
(`HVEN_SQP_COUNTERS_FIELDS`, `HVEN_SSN_COUNTERS_FIELDS`,
`HVEN_IPQP_COUNTERS_FIELDS`) rather than hand-typed. **Key order is the table's
order, which `static_assert`s hold equal to the struct's declaration order** (an
aggregate-arity count plus an `offsetof`-monotone check, not a `sizeof`). The
tables are therefore the authoritative field list and are not duplicated here.

Shape: the 37 direct fields, then `"ssn": { 18 fields }`, then
`"ipqp": { 39 fields }`.

`start_level_used` is an enum, written `cold\|seeded\|warm\|hot` — the schema's
lower-snake alphabet, not core's PascalCase display spelling.

**Three counters carry a documented ABSENCE SENTINEL and serialize as `null`**
through the tables' own predicate column. The counter and the bench CSV keep the
sentinel; only the stream maps it:

| counter | sentinel | `null` means |
|---|---|---|
| `ipqp.ipqp_alpha_p_min` | `+inf` | no fraction-to-boundary step was ever taken |
| `ipqp.ipqp_alpha_d_min` | `+inf` | the same, dual side |
| `ipqp.ipqp_tier_retired_after` | `0` | the tier was never retired (it is a 1-based MAJOR index, max-folded, so 0 cannot be a reading) |

## 6. The mode-selection telemetry on `sqp.major`

**(a) `active_set_delta`** — the symmetric-difference count between this major's
QP active set and the previous major's: inequality rows (`ineq_active`) plus
bound SIDES (`bound_state`, a side that changed lower ↔ upper ↔ free counting
once). The **first major counts against the EMPTY set**, so everything active
counts. A row with `qp_solved == false` reports 0 and the previous set is carried
unchanged. The restoration sub-driver runs its own sequence.

**(b) `weak_active_rows` / `near_active_rows`** — a **strict-complementarity
MAGNITUDE margin**, `kWeakActivityMargin = 1e-6` (a constant, not a setting).
`weak_active_rows` counts ACTIVE rows with
`|λ_i| ≤ margin · max(1, ‖λ‖∞)`; `near_active_rows` counts INACTIVE rows whose
nonnegative QP slack `s = b − A p` satisfies the same relative test, in the QP's
own units.

**This is NOT the SSN's notion.** The SSN uncertain set is a dimensionless KINK
BAND (`|(λ − s)/ρ| ≤ 0.1` with 3× leave hysteresis), which reads a PURE state at
`s = 0` for any `λ` and so never flags a small multiplier on an active row. Both
readings flag a tie fixture, for different reasons. What the relative scaling
means in practice: with `max(1, ‖λ‖∞)` scaling, a row at `λ = 10` reads "weak"
beside a row at `5e7` — acceptable for telemetry, and stated rather than hidden.
A driver-level kink measure is REGISTERED for M7 if a heuristic wants the SSN's
own reading; bound-side weak/near is registered too.

**(c) `active_rows` / `active_lower_sides` / `active_upper_sides`** — the census
counts **REAL bound sides only**. Every `QpSolution` producer reports `kFree` for
a variable the trust region is holding and flags `tr_active` instead, so a
TR-held side does not appear here; **`tr_binding` on the same row is the
reader's separator.** A variable oscillating between a real bound and a TR pin
registers a delta every major.

**The exhausted-ladder row's zero is a POLICY, not a measurement.** Such a row
follows `tr_binding`'s own rule — one rule for the row block — and reports 0.

**The folds.** Four `SqpCounters` fields aggregate these, on the
`ssn_uncertain_peak` convention: `active_set_delta_total` (sum),
`active_set_delta_peak`, `weak_active_peak`, `near_active_peak` (max). Nothing in
M6 reads them.

## 7. Reader rules

1. **Order is the join key.** Every `qp.mode`, `fallback.verdict` and IPQP line
   belongs to the major whose `sqp.major` line comes NEXT at the same depth.
2. **A depth-0 bracket may contain a whole depth-1 solve**, and a
   restoration-requesting row's `sqp.major` line FOLLOWS that nested solve
   (§4.3).
3. **`sqp.solve` begin/end PARTITIONS a multi-solve stream.** Two solves on one
   sink share `seq` contiguously and are told apart by their pairs.
4. **Tolerate exactly one malformed line, at a truncation point**, and take the
   `seq` gap as the authority. A partial-line splice is what an interrupted
   writer leaves; `seq` counts lines ATTEMPTED, so `lines_written()` minus the
   artifact's line count is the number lost, process-side, and the gap is the
   same number artifact-side.
5. **There is no flush at destruction.** The stream is BORROWED: the sink never
   opens, closes or flushes it. A reader that opens the file before the
   `ofstream` is closed sees a short artifact, and that is not corruption.
6. **`facts` must be valid UTF-8** — a CALLER contract, not validated (§2).
   Substituting bytes would mutate a record.
7. **No reader should ever see `"unknown"`** (§2).
8. **`evidence` is not flattenable** (§2).
9. **THE TEE NOTE: three events sum to no counter.** `ipqp.certify`,
   `ipqp.route` and `qp.mode` have no solve-wide counter to check against —
   `accumulate_ipqp_counters` ASSIGNS `ipqp_final_inertia_read` rather than
   summing it, so no count of reads TAKEN exists in the currency. Their line
   counts are INTERNAL-CONSISTENCY checks only, asserted against a recording tee
   in the tests. `qp.mode`'s per-`site` identities (§4.4) are the exception that
   proves the rule: they hold against counters that price the INVOCATIONS, not
   against a count of the event.
10. **`begin` means the arguments were ACCEPTED.** A refused call writes nothing.
11. **`JsonLinesTraceSink::failed()` is NON-ATTRIBUTING.** It reports "the stream
    reported failure at or before one of this sink's writes" and cannot say
    whose write it was — a co-writer on the same stream reads the same. It is
    sticky, and it is **never set under an ARMED exception mask**, because the
    throw precedes the read.
12. **Exceptions have two cases.** Under the DEFAULT mask the sink never throws
    and can never end a solve. Under a mask the caller ARMED
    (`exceptions(std::ios::badbit)`), `std::ios_base::failure` propagates out of
    `SqpDriver::solve` BY DESIGN — a caller who arms exceptions asked for them.
    `reset_nesting()` is the recovery.
13. **`_s` fields are wall-clock and informational, never asserted** (CLAUDE.md
    §7).
14. **17 significant digits is a round-trip property of the text**, not a printed
    digit count (§2).
15. **v0 carries no vector-valued field** (§2).

## 8. The freeze (plan §2 rule 6, as clarified at W4 T5)

v0 is **FROZEN**: no field is renamed, removed or retyped, and a frozen event's
golden line **moves only by a DECLARED ADDITIVE TRAILING KEY, never otherwise**.
ADDITIVE changes — new events, new enum strings, new trailing fields — stay v0
until the schema has an external consumer (M7's first heuristic). Anything
non-additive is v1.

| `ev` | frozen since | declared additive re-derivations |
|---|---|---|
| `ipqp.iter` | W4 T1 | — |
| `ipqp.reg` | W4 T1 | — |
| `ipqp.restart` | W4 T1 | — |
| `ipqp.route` | W4 T1 | — |
| `ipqp.certify` | W4 T1 | — |
| `ipqp.escape` | W4 T1 | — |
| `fallback.verdict` | W4 T1 | — |
| `qp.mode` | W4 T1 | T2 (the `walk` / `ssn` mode strings); **T5 (the trailing `site` key)** |
| `sqp.major` | **W4 T5** | added T2; T3 (six activity fields, §6) |
| `sqp.solve.begin` | **W4 T5** | added T2 |
| `sqp.solve.end` | **W4 T5** | added T2; T3 (four folds, through the counters tables) |
| `ipm.iter` | **W4 T5** | added T4; T4 fix1 (`p_pivots` and `h_facs` null sentinels) |
| `ipm.solve.begin` | **W4 T5** | added T4 |
| `ipm.solve.end` | **W4 T5** | added T4 |

**`qp.mode`'s T5 line is the ONE W1/W2 golden line M6 W4 moves**, and it moves by
exactly one trailing key: every byte before `,"site"` is T1's, unchanged
(settler ruling, 2026-09-04).

Each event's byte-exact golden line lives in `tests/sqp/test_trace_writer.cpp`,
built from a hand-filled struct with distinct values per field. An
aggregate-arity `static_assert` per event struct fails the build when a field is
added without a key, so the freeze is enforced at compile time rather than by
review.

## 9. Evidence

The v0 exemplar streams, the telemetry census over the HS battery, and the
provenance under which all of them were taken:
`docs/notes/data/2026-09-m6-w4-acceptance/`.
