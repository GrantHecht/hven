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
  (`include/hven/linear/symmetric_factor.h`) carries five counters. All are
  per engine instance. The four BACKEND-CALL counters below — everything but
  `pattern_verify_count` — count calls that reached the backend and returned:
  a call rejected by validation (wrong pattern hash, size mismatch, wrong
  lifecycle order) throws and increments nothing, and neither does one that
  fails inside the backend and throws.
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

  - `pattern_verify_count` — +1 per `factorize()` call that actually RAN the
    pattern guard: recomputed the matrix's pattern hash and compared it
    against the analyzed key. The one counter here that does not count a
    backend call, and the one that counts a call the validation then
    REJECTED — the rejection throws, and `factorize_count` does not move, but
    the guard did run. It exists so that "the guard was skipped" is checkable
    the way `analyze_count` makes "the symbolic was reused" checkable: a run
    of factorizations under `PatternCheck::kAssumeAnalyzed` advances
    `factorize_count` and leaves this one standing, and under
    `PatternCheck::kVerify` the two move together.

  `DenseSymmetricFactor` exposes no counters; one gets added when a consumer
  reads it.
- **`model/`** — documented as the component lands.
- **`kkt/`** — documented as the component lands.
- **`interior/`** — documented as the component lands.
- **`qp/`** — documented as the component lands.
- **`globalization/`** — documented as the component lands.
- **`warmstart/`** — documented as the component lands.
- **`drivers/`** — `InteriorPointSolver::kkt_analysis_count()`
  (`include/hven/drivers/interior_point_solver.h`) is the one counter this
  component exposes so far. +1 per KKT sparsity analysis this solver instance
  has laid, over the object's LIFETIME rather than per call: it moves once at
  `set_nlp()` and once more at any solve entry that finds the model's
  structure epoch moved since the last analysis, and `release()` returns it to
  zero along with the analysis it counts. Deliberately not a `SolveResult`
  field — that struct is reset per call, and "did a second solve against
  unchanged structures analyze again?" is a cross-call question. The rest of
  the component's diagnostics are documented as they land.
