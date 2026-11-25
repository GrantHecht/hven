# hven documentation

Two kinds of page live here: the pages a user reads to solve a problem, and the
engineering records that say how the library is built and measured. If you have
an NLP and want a solution, read the first table in order and stop.

## Using hven

| Page | What it is |
| --- | --- |
| [getting-started.md](getting-started.md) | Install, the first solve walked through, reading the human trace and the solve summary, callbacks and early stop, warm starts, and where to go next |
| [guides/choosing-a-solver.md](guides/choosing-a-solver.md) | Interior-point, SQP, or SQP with a quasi-Newton Hessian: which to try first, what the measurements behind that say, and what to do when a solve does not finish |
| [options-reference.md](options-reference.md) | Every option of both engines with its type, default and range. **Generated** from the options registry; a test fails when it drifts |
| [trace-schema-v1.md](trace-schema-v1.md) | The machine trace (JSON lines): every record and key; section 8 is the human trace, with the column reference for the console rows |
| [counters.md](counters.md) | The operation counters in a result and what each one means |
| [concurrency.md](concurrency.md) | What N solvers on N threads may assume, what they may not share |
| [assembly-protocol.md](assembly-protocol.md) | Building a problem from pieces (index maps, claim slots, the scatter, the lock protocol): for the partitioned route only |
| [guides/snopt-like-configuration.md](guides/snopt-like-configuration.md) | Configuring the SQP engine (first-derivative L-BFGS mode) to behave like SNOPT, for a SNOPT-compatible shim |
| [../examples/README.md](../examples/README.md) | Ten one-problem programs, each adding one idea; the reference for the public surface |

## Moving between versions

| Page | What it is |
| --- | --- |
| [migration-0.2.md](migration-0.2.md) | The interface groups that landed after the break (stop token, `StopReason`, regrouped results, trace v1, include paths, return-best), before and after, one section each |
| [migration/2026-09-m6-api-break.md](migration/2026-09-m6-api-break.md) | The earlier M6 break (names before `hven::solvers` was dropped) |
| [../CHANGELOG.md](../CHANGELOG.md) | One before-and-after line per change |
| [trace-schema-v0.md](trace-schema-v0.md) | The superseded v0 trace schema, kept for the frozen v0 artifacts under `notes/data/` |

## Engineering records

| Page | What it is |
| --- | --- |
| [testing.md](testing.md) | The test-seam convention, the fault-injection seam, and the golden-numerics rig |
| [measurement.md](measurement.md) | The measurement discipline: what is asserted and what is informational, the serial rule for wall-clock, the co-run protocol, the provenance stamp |
| [ci.md](ci.md) | The three CI lanes, and the route from the macOS lane to a real Accelerate observation |
| [build.md](build.md) | The precompiled header and the ccache settings it needs |
| [pattern-hash.md](pattern-hash.md) | The structural key: what it hashes and therefore what it does and does not detect |
| [qp-dump-format.md](qp-dump-format.md) | The "D4QP 1" text format of the SQP's QP dumps |
| [consumed-surface-audit.md](consumed-surface-audit.md) | What the two engines consume from the sparse backends, reconciled against the frozen option set |
| [retarget-design.md](retarget-design.md), [retarget-design-sqp.md](retarget-design-sqp.md) | Historical design records of the linear-solver retarget (names are those of their time) |
| [dockets/](dockets/README.md) | The migration docket: what each fail-by-design trace found |
| [notes/](notes/) | Dated design notes, ledgers and measurement records. Not user documentation |
