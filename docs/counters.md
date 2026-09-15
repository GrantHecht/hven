# Counters contract

hven's counters are exact integers, not estimates. They are the asserted
currency against which tests and benchmarks are checked; wall-clock timings
are informational only and are never used, on their own, to assert
correctness or to gate a test or benchmark.

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
| `SsnCounters` | `core/solver_counters.h:210-336` | 18 | the SSN QP tier, `qp/` |
| `IpqpCounters` | `core/solver_counters.h:454-860` | 39 | the IPQP QP tier, `qp/` |
| `SqpCounters` | `core/solver_counters.h:1009-1329` | 38 direct, plus the nested `ssn` and `ipqp` | the SQP driver, `drivers/` |
| `SymmetricFactor::Counters` | `linear/symmetric_factor.h:646-682` | 5 | every sparse symmetric factor, `linear/` |

115 fields across the four `core/` groups; `SqpCounters`' 38 do not include the
two nested groups' 57, which it carries whole.

THREE OF THESE ARE PINNED AGAINST A MACRO TABLE, not merely declared:
`SsnCounters`, `IpqpCounters` and `SqpCounters` each carry an `X()` list used to
serialize them, and an aggregate-arity `static_assert` beside it fails to
compile if a field is added without an entry in its declaration position
(`core/detail/aggregate_arity.h`; the asserts are at `solver_counters.h:417`,
`:906` and `:1382`, with the offset-order asserts beside them). A field added to
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
  reachable as `SqpResult::counters` (`drivers/sqp_solver_types.h:991`) and on
  every `Ledger` driver record.

  THE INTERIOR-POINT ENGINE HAS NO AGGREGATE COUNTER STRUCT. Its countable
  quantities are loose fields on `IpmResult`. The KKT-analysis count this
  section used to name as `IpmSolver::kkt_analysis_count()` is among them: that
  ACCESSOR WAS RETIRED at M6 W5 T8.4 (`drivers/ipm_solver.h:344` records the
  retirement) and the quantity is now two snapshot fields on the returned
  result, `IpmResult::kkt_analyses_total` and `::kkt_analyses_this_call`
  (`drivers/ipm_solver_types.h:655`, `:657`), beside
  `IpmResult::kkt_factor_counters` (`:660`), which is the linear layer's group
  as of that solve. `kkt_analyses_total` is the LIFETIME count for the producing
  solver -- it moves once when the structure is first laid and once more at any
  solve entry that finds the model's structure epoch moved -- and
  `kkt_analyses_this_call` is how much of that happened in the call being
  reported. Snapshots rather than accessors, so they describe the solve that
  produced them and cannot go stale behind a caller's back.
