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

## Per-component status

- **`core/`** — no counters yet; the shared type aliases and `pattern_hash`
  seeded here carry none.
- **`linear/`** — `SymmetricFactor::Counters`
  (`include/hven/linear/symmetric_factor.h`) carries four counters. All are
  per engine instance, and all count calls that reached the backend and
  returned: a call rejected by validation (wrong pattern hash, size mismatch,
  wrong lifecycle order) throws and increments nothing, and neither does one
  that fails inside the backend and throws.
  - `analyze_count` — +1 per completed symbolic analysis. `factorize()` never
    increments it, which is what makes "the symbolic was reused" checkable
    rather than merely claimed. Adopting a shared factorization starts a
    fresh instance at 0 and does not increment: no analysis was performed.
  - `factorize_count` — +1 per numeric factorization the backend completed,
    whether it succeeded or reported an error. It counts work done, not work
    that worked; what committed is tracked by the epoch, not by this counter.
  - `solve_count` — +1 per full solve call, single- or multi-RHS, regardless
    of how many right-hand sides that call carried. A multi-RHS solve with
    zero columns does no backend work and does not increment.
  - `partial_solve_count` — +1 per phase-split (forward / diagonal /
    backward) solve. Counted separately from `solve_count` on purpose:
    composing three partial solves is not one full solve, and the traces
    that assert on these need to tell them apart.

  `DenseSymmetricFactor` exposes no counters; one gets added when a consumer
  reads it.
- **`model/`** — documented as the component lands.
- **`kkt/`** — documented as the component lands.
- **`interior/`** — documented as the component lands.
- **`qp/`** — documented as the component lands.
- **`globalization/`** — documented as the component lands.
- **`warmstart/`** — documented as the component lands.
- **`drivers/`** — documented as the component lands.
