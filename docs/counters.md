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
| `QpCounters` | `core/solver_counters.h:30-200` | 20 | the walk (active-set) QP tier, `qp/` |
| `SsnCounters` | `core/solver_counters.h:210-348` | 18 | the SSN QP tier, `qp/` |
| `IpqpCounters` | `core/solver_counters.h:466-896` | 39 | the IPQP QP tier, `qp/` |
| `SqpCounters` | `core/solver_counters.h:1048-1520` | 44 direct, plus the nested `ssn` and `ipqp` | the SQP driver, `drivers/` |
| `SymmetricFactor::Counters` | `linear/symmetric_factor.h:646-682` | 5 | every sparse symmetric factor, `linear/` |

121 fields across the four `core/` groups; `SqpCounters`' 44 do not include the
two nested groups' 57, which it carries whole. (115 and 38 until M7 W2 M4, which
appended `start_point_clamps`; 116 and 39 until M7 W2 M1, which appended the
five-field subproblem-exit group -- `qp_maxiter_exits`, `qp_numerical_exits`,
`qp_failures_routed`, `qp_failures_in_a_row_peak` and `qp_cap_retries`.)

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
  the difference is the sub-solve's own.

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
