# Counters contract

hven's counters are exact integers, not estimates. They are the asserted
currency against which tests and benchmarks are checked; wall-clock timings
are informational only and are never used, on their own, to assert
correctness or to gate a test or benchmark.

## Curvature source labels

Two one-line tags live in the headers, on declarations rather than in this page,
and they are labels only -- no field, no enum, no behaviour. They discharge the
cost-model half of the 2026-08-31 roadmap amendment
(`docs/notes/2026-08-26-roadmap-m6-m8-draft.md:342-362`), which carries two
constraints into M7 so the M9 first-order (Hessian-approximation) mode does not
have to reopen M7's work: "every curvature-reading heuristic (ladder counters,
rho demanded, the item-4 read, SSN certification tiers) is labelled with the
curvature source it assumes -- no decision, a label", and "telemetry keyed on the
bridge-lay `+1 eval_hess` identity is tagged exact-Hessian-specific".

- `CURVATURE SOURCE: exact model Hessian (M9 label; ...)` marks a quantity whose
  MEANING assumes H is the model's exact second derivative. Four kinds carry it,
  stated as applied rather than as a principle:
  - A COUNTER that reports a curvature reading or counts curvature-driven events.
    Ladder counters are in even where a rung's TRIGGER is a perturbed-pivot report
    rather than a wrong inertia (`ipqp_pivot_reroute_primal`, `_dual_fallback`):
    what a rung MOVES is `rho_dem`, the curvature modification, and a first-order
    mode re-derives the ladder wholesale. `ipqp_ladder_reclimbs`' "not a statement
    about curvature" is about that trigger, not about what the ladder reads.
  - A MIXED-CAUSE counter is in when one of its causes IS a curvature reading and
    that branch is the one a first-order mode would misread: `ipqp_escape_numerical`
    counts a factorization failure, a non-finite iterate AND an unreadable inertia
    read, and it is the third that changes meaning. A partition over causes none of
    which is a reading -- `ipqp_escapes`, `ipqp_to_*` -- stays out.
  - An OPTION that selects WHETHER or HOW such a reading is taken, a positive
    control included (`SsnOptions::safeguards`: `kBare` runs no inertia gate at
    all). NOT an option that selects an ENGINE: `qp_mode` and
    `elastic_rung_engine` are deliberately unlabelled, because every kernel reads
    curvature on its own exit -- the walk probes zero-multiplier drops and rides
    negative curvature exactly as the tier reads its final inertia -- so an engine
    selector asserts nothing about H, and the readings it routes to are labelled
    where they happen.
  - A RESULT FIELD carrying a curvature verdict the driver branches on, and the
    PREDICATE that decides from one.

  Under a first-order mode these are re-derived or re-labelled, not merely
  rescaled: under a positive-definite family, e.g., no inertia ladder ever arms,
  so the ladder counters and `rho` demanded read zero for a reason that is not the
  problem's. M9a has not chosen the family -- an indefinite SR1 would arm the
  ladder -- so that is an illustration, not a premise.

  RELABELLED AT M7 QN STAGE 3 on the SQP side (the interior-point tags are
  unchanged): `SqpOptions::hessian_mode = kLbfgsDamped` makes the subproblem
  Hessian the damped L-BFGS B (positive definite), and every SQP-side tag now
  reads `CURVATURE SOURCE: the subproblem Hessian -- ... the PD quasi-Newton B, so
  this certifies B, not the NLP`. The quantities are still REPORTED in that mode --
  a second-order verdict there is a statement about the model B, and the
  certificate the driver reports remains first-order.
- `EXACT-HESSIAN-SPECIFIC (M9 label): ...` marks telemetry keyed on the bridge-lay
  `+1 eval_hess` identity -- the relation `SqpCounters` states between
  `steps_accepted` and the optimality phase's `eval_hess` calls, and the three
  seam/adapter/driver comments that state the same shape. That identity counts
  `eval_hess` calls and is zero under a first-order mode, so no cost model may
  budget curvature acquisition from it there.

The tags are NOT repeated on the mirrors of a labelled declaration -- trace event
fields, console and CSV columns, `enum_names`, the X-macro rolls, the options
dump -- nor on the linear-algebra primitive (`SymmetricFactor::inertia`,
`InertiaEvidence`), which reports the signature of whatever matrix was factored
and is true under any Hessian. The source is labelled once, where it is declared.

To find every labelled site:

```
grep -rn "CURVATURE SOURCE:\|EXACT-HESSIAN-SPECIFIC" include/ src/
```

## Rules

- A counter increments by exactly the amount its owning header documents —
  no fuzz, no "approximately".
- Every component that exposes a counter documents that counter's exact
  increment semantics — what increments it, by how much, and under which
  condition — in the header where the counter is declared, next to the
  field itself.
- A test or benchmark that asserts on a counter is asserting on that
  documented semantic, not on a value nobody wrote down.
- An intentional change to a counter's semantics — what it counts, when it
  increments, its starting value — is declared explicitly, in the header
  comment and in the commit that makes the change. It is never a silent
  side effect of an unrelated change.

## The inventory

Four counter GROUPS live in `include/hven/core/solver_counters.h`, plus one in
the linear layer. Each is an aggregate of `Index` fields (and one `bool`), and
**the per-field documentation lives in the header, at the field** — the rules
above require it there, and repeating it here would give a reader two places to
believe. This table says only what exists, where, and how many.

| group | header, line range | fields | produced by |
|---|---|---|---|
| `QpCounters` | `core/solver_counters.h:34-583` | 100 | the walk (active-set) QP tier, `qp/` |
| `SsnCounters` | `core/solver_counters.h:210-348` | 18 | the SSN QP tier, `qp/` |
| `IpqpCounters` | `core/solver_counters.h:466-896` | 39 | the IPQP QP tier, `qp/` |
| `SqpCounters` | `core/solver_counters.h:1048-1683` | 50 direct, plus the nested `ssn` and `ipqp` | the SQP driver, `drivers/` |
| `SymmetricFactor::Counters` | `linear/symmetric_factor.h:646-682` | 5 | every sparse symmetric factor, `linear/` |

127 fields across the four `core/` groups; `SqpCounters`' 50 do not include the
two nested groups' 57, which it carries whole. (115 and 38 until M7 W2 M4, which
appended `start_point_clamps`; 116 and 39 until M7 W2 M1, which appended the
five-field subproblem-exit group -- `qp_maxiter_exits`, `qp_numerical_exits`,
`qp_failures_routed`, `qp_failures_in_a_row_peak` and `qp_cap_retries`; 121 and
44 until M7 W2 M2, which appended the five-field restoration excursion record --
`restoration_entries`, `restoration_h_in`, `restoration_h_out`,
`restoration_f_in` and `restoration_f_out`, the first `double`s the SQP driver's
own direct block has ever carried; 126 and 49 until M7 W2 M3a appended
`restoration_sub_status` to that same record -- the feasibility sub-solve's own
terminal status at the last measured entry, the block's second enum after
`start_level_used` and the only field in it narrower than 8 bytes.)

**The aggregate is no longer "`Index` fields (and one `bool`)" alone**, and has
not been since M7 W2 M2: `SqpCounters` carries four `double`s and, since M7 W2
M3a, two enums (`start_level_used`, a `StartLevel`, and `restoration_sub_status`,
a `RestorationSubStatus`). A CATEGORICAL reading is a legitimate counter here
when it is per-solve and overwritten rather than accumulated --
`ipqp_rho_demanded_last` and `ipqp_final_inertia_read` are the precedent -- and
each one's serializer overload lives beside the others in
`src/drivers/trace_writer.cpp`.

**`restoration_sub_status` carries an EXPLICIT not-run value**, and is the first
counter here that can say "absent" in its own vocabulary. `SolveStatus` has no
"never entered" value, so the field takes a dedicated enum that does:
`RestorationSubStatus` (`core/solver_status.h`) is `kNotRun = 0` -- the default,
spelled `not_run` on the trace -- followed by one enumerator per `SolveStatus`
value, under the same names and the same spellings, with the total map
`to_restoration_sub_status` between them. The four `double` absolutes beside it
keep `restoration_entries == 0` as their absence reading, because `0.0` is a
legal violation and a legal objective and they have no spare value to spend;
this field had one and spends it.

*(M7 W2 M3a fix1 re-derivation.* The field shipped as a bare `SolveStatus`
defaulting to the enum's zero, `kOptimal`, with absence left to the count. That
made a never-restored solve report `optimal` for a sub-solve it never ran -- a
legal success value doing duty as an absence, which the review called a consumer
hazard. The settler withdrew it. The replacement has the SAME underlying type
and therefore the same width, so no pin below moves: 50 fields, `sizeof` 856,
and the only trace byte that changes is this key's own value on a solve that
never restored, which now reads `"not_run"` instead of `"optimal"`.)

THREE OF THESE ARE PINNED AGAINST A MACRO TABLE, not merely declared:
`SsnCounters`, `IpqpCounters` and `SqpCounters` each carry an `X()` list used to
serialize them, and an aggregate-arity `static_assert` beside it fails to
compile if a field is added without an entry in its declaration position
(`core/detail/aggregate_arity.h`; the asserts are at `solver_counters.h:429`,
`:942` and `:1423`, with the offset-order asserts beside them). A field added to
one of those three therefore cannot silently miss the trace.

## Per-component status

- **`core/`** — declares all four groups above, in `solver_counters.h`. It
  produces none of them itself: the type aliases and `pattern_hash` seeded here
  carry no counter, and `Ledger` (`core/ledger.h:33`, `:64`) STORES a
  `QpCounters` and a `SqpCounters` per record rather than incrementing either.
- **`linear/`** — produces `SymmetricFactor::Counters`, per engine instance.
  Four of the five count BACKEND CALLS that reached the backend and returned: a
  call rejected by validation (wrong pattern hash, size mismatch, wrong
  lifecycle order) throws and increments nothing, and neither does one that
  fails inside the backend and throws. The fifth, `pattern_verify_count`, counts
  the guard runs rather than a backend call, including the runs that then
  REJECTED. Each field's exact increment semantics are at the field.
  `DenseSymmetricFactor` exposes no counters; one gets added when a consumer
  reads it.
- **`model/`** — produces none.
- **`kkt/`** — produces none of its own. The interior-point engine's KKT factor
  re-exports the linear layer's group unchanged: `KktFactorization::Counters` is
  an ALIAS for `SymmetricFactor::Counters`
  (`detail/interior/kkt_factorization.h:32`), not a fifth group.
- **`interior/`** — produces none of its own.
- **`qp/`** — produces `QpCounters`, `SsnCounters` and `IpqpCounters`, one tier
  each. A tier's counters reach a caller through the SQP driver's aggregate:
  `SqpCounters::ssn` and `SqpCounters::ipqp` carry the latter two whole, and
  `QpCounters` reaches a `Ledger` record per QP subproblem solve.
- **`globalization/`** — produces none.
- **`warmstart/`** — produces none.
- **`drivers/`** — produces `SqpCounters`, the aggregate for a whole SQP solve,
  reachable as `SqpResult::counters` (`drivers/sqp_solver_types.h:1056`) and on
  every `Ledger` driver record.

  **`SqpCounters::start_point_clamps` IS STATED AGAINST THE `x0` ARGUMENT, AND
  ON A WARM SOLVE THAT IS NOT WHERE THE LOOP BEGINS** (M7 W2 M4 fix1). The
  counter says how many components of the `x0` handed to `solve()` lay strictly
  outside the declared box, and `SqpOptions::project_start_point` projects that
  same vector. But `prepare_solve` starts a solve at `warm.x` whenever a warm
  object is ingested, and for the MULTIPLIERS-ONLY payload form `warm.x` is a
  copy of `x0` taken one frame ABOVE the projection -- so on that route a
  non-zero count at `project_start_point == true` reports an argument that was
  clamped while the loop began at the unclamped point. **Every COLD solve is
  free of this**, which is every solve the M4 measurement covers. The reading is
  therefore exact on a cold solve and is a statement about the ARGUMENT, not
  about the start point, on a warm one. Widening the projection to the resolved
  start point is registered, not done: it would have to run above the payload
  ingest, and the scaling rule already evaluates the model at `x0` below.

  **THE SUBPROBLEM-EXIT GROUP SPLITS INTO A WORK HALF AND A ROUTING HALF, AND
  THE RESTORATION SUB-SOLVE FOLDS ONLY THE FIRST** (M7 W2 M1).
  `qp_maxiter_exits`, `qp_numerical_exits` and `qp_cap_retries` count QP SOLVES
  and fold from the restoration sub-solve on `qp_minor_iters`' own argument.
  `qp_failures_routed` and `qp_failures_in_a_row_peak` describe what happened to
  the CALLER's iterate -- how many times the radius shrank under it without it
  moving -- and do NOT fold, on `rejected_steps`' own argument, which is the
  counter they are a subset of. A consumer summing "exits" against "routed"
  across a solve that restored will therefore find more exits than routings, and
  the difference is the sub-solve's own. A solve that ends `numerical_error` with
  `qp_maxiter_exits + qp_numerical_exits == 0` ended on the THIRD source,
  `nonfinite_subproblems` (M7 W2 R2): a subproblem whose data was not finite,
  refused before any kernel ran.

  **THE FAILED-SUBPROBLEM STEP GROUP FOLDS WHOLE** (M7 W2 R2).
  `qp_failure_steps_judged`, `qp_failure_steps_accepted` and
  `nonfinite_subproblems` count work and events of the sub-solve's own majors, on
  `soc_steps`' argument, so all three fold. An accepted failure step is ALSO a
  `steps_accepted` (it commits through the ordinary accept arm) and is never a
  `qp_failures_routed`; a judged, rejected one IS routed but does not move
  `qp_failures_in_a_row_peak`'s run, because it is not charged to the budget.
  `nonfinite_subproblems` is at most one per driver, so a solve that restored
  can fold up to two.
  `nonfinite_box_projections` (M7 W2 W1) folds with them: a projection is an
  event of the sub-solve's own major. It counts projections TAKEN -- an iterate
  outside its box only by rounding, at which the subproblem was non-finite -- so
  a projection after which the data is still non-finite is counted here AND in
  `nonfinite_subproblems`.
  `nonfinite_step_rollbacks` (M7 W2 R2-1, appended last after `qn_dimension`)
  folds with them too. It counts ACCEPTED steps rejected after the fact: the
  strategy judges a trial on f and c alone, so the first major after a commit can
  find the new point's gradient (the iterate measurement) or its H, A or g (the
  subproblem guard) non-finite -- YORKNET's free pipe flows landing on q = 0
  exactly, where the |q|^1.852 element's second derivative is infinite. The
  previous iterate, its evaluation and its prices are restored, the funnel width
  an h-type acceptance of the step tightened is put back
  (`FunnelStrategy::retract_last_judgement`), the radius the step was taken
  under is shrunk, and that major's row describes the abandoned
  point with no subproblem solved. A rolled-back step stays counted in
  `steps_accepted` (and in `qp_failure_steps_accepted` when it was one); it is
  not a `rejected_steps`. Bounded by the radius floor: no rollback is taken where
  the shrink would cross `tr_min`, nor in the full-step mode, nor where the point
  was not reached by a commit -- those still end through `nonfinite_subproblems`
  (or the non-finite-iterate exit), which therefore counts only exits.

  **`scaled_convergence_refusals` IS THE CALLER-UNIT FEASIBILITY GUARD** (M7 W2
  SCL, appended last after `restoration_feas_out`). On a solve that ran scaled --
  the row/objective layer (`SqpOptions::enable_scaling`, or `column_scaling ==
  kAuto` firing) or the column layer (`SqpOptions::column_scaling`) -- the
  convergence test's two conjuncts are read in the engine's units, so a row the
  row layer shrank by `s_j < 1` passes at `|c_j| <= feas_tol / s_j` and a box
  violation is read in units of the column factor. The guard requires the same
  `feas_tol` of the CALLER-unit measure (each row residual over its row factor,
  each box violation times its column factor) and counts each measured iterate at
  which the scaled test passed and the caller-unit one did not; such an iterate
  is not converged and the solve continues. 0 on every unscaled solve (the guard
  is not entered), so the shipped default never moves it. Not folded: the
  restoration sub-solve runs unscaled. Where the caller-unit rounding floor
  exceeds `feas_tol`, every iterate is refused and the solve ends `max_iter`: a
  refusal streak to the cap is the expected spelling of that case.

  **THE COLUMN RULE'S JACOBIAN PROBE IS AN `evals_values`** (M7 W2 SCL rider).
  Whenever `SqpOptions::column_scaling != kOff` the rule evaluates the
  Jacobians once at the start point; that query is charged one `evals_values`,
  as the warm-resolution probe's is, so the `evals_full` / `evals_values`
  partition stays a count of every model query. A solve the column layer scaled
  also pays one `evals_full` for the caller-unit re-measurement of the returned
  point. Both are on the counters before the `sqp.solve.end` emission, so the
  trace, the result and the ledger agree. Under `kOff` neither exists.

  **THE RESTORATION EXCURSION RECORD IS A PER-ENTRY READING, OVERWRITTEN, AND
  ITS ABSENCE IS READ OFF ITS OWN COUNT** (M7 W2 M2, the A0 signals page's S5).
  `restoration_entries` counts the restoration phases that RAN and were
  MEASURED -- the resume arm and the not-feasible-enough arm. The four
  absolutes beside it, `restoration_h_in`/`_h_out`/`_f_in`/`_f_out`, describe
  the LAST such entry and are overwritten by each one, on
  `ipqp_rho_demanded_last`'s precedent: the entry violation of two phases does
  not add up to anything. Four rules a consumer needs and cannot read off the
  names:

  * **The absence reading is `restoration_entries == 0`, not a sentinel in the
    four.** They default to `0.0`, and `0.0` is a legal observed violation and a
    legal observed objective, so the count is what says whether a reading
    exists.
  * **The `in` half is the REQUESTING iterate**, `x_k` -- not the seed the phase
    may have started from (`SqpIterate::restoration_seed_used`). The excursion
    being measured is the caller's iterate's.
  * **They are the ENGINE's units** -- the caller's unless
    `SqpOptions::enable_scaling` is on -- the units the resume test is taken
    in, and a record with one half in each space would report a scale factor as
    an excursion.
  * **The resume reading is `restoration_feas_out`, not `h_out`** (M7 W2 SNR).
    The resume test reads the convergence test's own feasibility measure --
    `SqpKkt::feasibility`, the inf-norm over rows and box -- and
    `restoration_feas_out` (appended last in the struct) records it at the
    returned point: `restoration_feas_out <= feas_tol` iff the entry RESUMED,
    except at `SqpOptions::restoration_resume_reduction > 0` (M7 W2 P1E), where
    an entry counted in `restoration_reduction_resumes` resumed above it. Until
    SNR the test read the l1 sum `h_out + box` and `h_out <= feas_tol` was the
    resume reading (registered C1: a point every row of which the convergence
    test called feasible was refused). `h_out` is still the l1 sum, which
    bounds the rows' inf-norm from above, so `h_out <= feas_tol` on an in-box
    point still implies the entry resumed -- one way only.
  * **Absolutes, never ratios.** An objective ratio is undefined at `f_in` near
    zero and meaningless across a sign change, and the TRAIN cells in
    restoration's neighbourhood include negative objectives.

  **AT `SqpOptions::restoration_proximity > 0` THE RECORD'S MEANING IS
  UNCHANGED AND THE SUB-SOLVE'S IS NOT** (M7 W2 M2b). The four absolutes are
  `h` and `f` of the CALLER's model at the requesting and the returned iterate,
  which is what they are at every weight. What the weight changes is the
  feasibility wrapper's own objective -- `h + (zeta/2)||D(x - x_k)||^2` rather
  than `h` -- so a depth-1 iteration event's `f` is no longer the violation at
  that iterate, and `restoration_feas_out <= feas_tol` still says the entry
  RESUMED. The shipped weight is 0, at which none of this applies.

  **THE RECORD'S SIXTH FIELD SAYS ITS OWN ABSENCE** (M7 W2 M3a, corrected in
  fix1). `restoration_sub_status` is the sub-solve's terminal status at that
  same last entry, overwritten by each one on the four absolutes' rule -- and,
  unlike them, it defaults to a value that is not a reading: `kNotRun`, spelled
  `not_run`. The first bullet above is therefore a statement about the four
  `double`s and not about the whole record. Two things it does NOT say, and
  both are read elsewhere: whether the entry RESUMED (the parent takes that
  decision on `restoration_feas_out <= feas_tol` alone, before it inspects
  the sub-status at all, so a feasible enough point returned with `max_iter` or
  `interrupted` resumes exactly as an `optimal` one does; the one exception is
  the sufficient-reduction resume below, which reads it), and whether the
  solve holds a CERTIFICATE (that one does require the sub-solve's `kOptimal`,
  and `SqpResult::infeasibility_certified` is what reports it). The certificate
  also requires the restoration point's inf-norm violation to exceed
  `kCertificateViolationMargin * feas_tol` (= 2 * feas_tol,
  `detail/globalization/sqp/restoration.h`): a `kOptimal` sub-solve at inf-norm
  <= 2 * feas_tol takes the no-claim exit, or the reduction resume below when
  `restoration_resume_reduction > 0`.

  **THE SUFFICIENT-REDUCTION RESUME** (M7 W2 P1E,
  `SqpOptions::restoration_resume_reduction = k`, default 0 = off). At `k > 0`
  an entry also RESUMES at `h_out <= k * h_in` with its box held, when the
  sub-solve certified nothing (`numerical_error`, or an `optimal` inside QC-1's
  no-claim margin); `restoration_reduction_resumes` counts those entries, so
  with it on the resume reading is `restoration_feas_out <= feas_tol ||
  restoration_reduction_resumes moved`. A
  certifying `optimal`, `max_iter` and `interrupted` never take it.
  `elastic_last_rung_fallbacks` (the same round) counts elastic ladders that
  handed back their last solved rung (`SqpOptions::elastic_last_optimal_rung`);
  it folds from the restoration sub-solve like `elastic_escalations`.

  **THE CURVATURE-SOURCE GROUP** (M7 QN stage 3, appended last: `eval_hess_calls`
  and thirteen `qn_*` fields; `qn_nonpd_seen` appended after them at the QN
  stages 1-3 review, `qn_dimension` at the QN-quality riders). `eval_hess_calls` counts every Lagrangian-Hessian
  evaluation the solve made -- each subproblem build and each warm-start structure
  probe under `SqpOptions::hessian_mode = kExact` -- and is 0 under `kLbfgsDamped`
  by construction. It does NOT count the assembly bridge's lay-time pattern walk:
  laying the structures reads the Hessian's pattern through one `eval_hess` per
  lay, so under `kLbfgsDamped` one call per solve and one per restoration entry
  still reaches the model, uncounted here. The `qn_*` fields are all 0 under
  `kExact`: `qn_updates` / `qn_damped` (pairs accepted, of those Powell-damped),
  `qn_skipped_*` by reason (non-finite, zero step, curvature, conditioning),
  `qn_rank_dropped`, `qn_resets_*` by reason (consecutive non-zero-step skips, the
  radius floor, a restoration resume), the two PEAKS `qn_pairs_peak` and
  `qn_lift_rank_peak` (max-folded), and two BUG FLAGS under a positive-definite B,
  counted and never acted on: `qn_kwrong_seen` -- lifted subproblem solves whose
  engine reported a wrong-inertia reading -- and `qn_nonpd_seen` -- lifts built
  while the model did not read positive definite (rounding can push the eigen
  form's smallest eigenvalue to or below zero). `qn_dimension` is the number of
  variables the model carries curvature for (max-folded): the structural count,
  or the nonlinear variables only under `SqpOptions::qn_nonlinear_only` (default
  off). Under the shipped default -- carried dimension at most
  `SqpOptions::qn_full_memory_max` (400) -- the carrier is a full-memory dense
  BFGS with no window, and `qn_pairs_peak` is the pair count since the last reset;
  above that cap it is the L-BFGS window's depth. The whole group FOLDS from the
  restoration sub-solve, which runs its own QN instance on the wrapper's x-block.

  THE THREE REFUSALS AND THE NON-FINITE EXIT WRITE NOTHING. A nested
  restoration, the once-per-solve cap and a spent budget never enter the phase;
  the non-finite exit entered it and measured nothing, and the driver's rule
  there is that nothing unmeasured may be reported as evidence. Nothing folds
  from the restoration sub-solve either: nested restoration is refused, so its
  own record is structurally empty.

  THE INTERIOR-POINT ENGINE HAS NO AGGREGATE COUNTER STRUCT. Its countable
  quantities are loose fields on `IpmResult`. The KKT-analysis count this
  section used to name as `IpmSolver::kkt_analysis_count()` is among them: that
  ACCESSOR WAS RETIRED at M6 W5 T8.4 (`drivers/ipm_solver.h:344` records the
  retirement) and the quantity is now two snapshot fields on the returned
  result, `IpmResult::kkt_analyses_total` and `::kkt_analyses_this_call`
  (`drivers/ipm_solver_types.h:659`, `:661`), beside
  `IpmResult::kkt_factor_counters` (`:664`), which is the linear layer's group
  as of that solve. `kkt_analyses_total` is the LIFETIME count for the producing
  solver -- it moves once when the structure is first laid and once more at any
  solve entry that finds the model's structure epoch moved -- and
  `kkt_analyses_this_call` is how much of that happened in the call being
  reported. Snapshots rather than accessors, so they describe the solve that
  produced them and cannot go stale behind a caller's back.

## What the shipped IPM default makes live (M7 W2 D1, 2026-09-18)

`IpmOptions`' defaults moved that day — `acceptance_strategy` `classic_merit`
-> `filter`, `barrier_governor` `classic_adaptive` -> `monitored`, `max_iters`
500 -> 1000 — as one declared re-derivation event
(`docs/notes/data/2026-09-m7-w2-d1-ipm-default/`). Nothing in this page's
contract changes; what changes is which of the interior-point engine's
diagnostics a STOCK solve populates. `IpmResult::last_filter_size`,
`::last_filter_resets`, `::last_monotone_switches` and `::last_monotone_iters`
each carry a `-1` sentinel meaning "the selected strategy or governor is not the
one that reports this", and on a default-configured solve all four used to sit
at that sentinel. They no longer do. A consumer reading `-1` from one of them is
now reading a deliberate non-default configuration, and one reading `-1` off a
run recorded before 2026-09-18 is reading the old default.
