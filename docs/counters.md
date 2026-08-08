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
- **`linear/`** — documented as the component lands.
- **`model/`** — documented as the component lands.
- **`kkt/`** — documented as the component lands.
- **`interior/`** — documented as the component lands.
- **`qp/`** — documented as the component lands.
- **`globalization/`** — documented as the component lands.
- **`warmstart/`** — documented as the component lands.
- **`drivers/`** — documented as the component lands.
