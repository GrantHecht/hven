# Migrating to the 0.2 interface groups (M8 AP-5)

The owner-approved interface design (`docs/notes/2026-11-m8-ap5-interface-design.md`)
lands in groups G1-G7. It names no migration-guide location, so its migration
lines collect here, one section per group, each line in the M6 guide's form:
before, after, rewrite. Groups not yet landed have no section.

## G1 -- the stop token and one `StopReason`

New, no rewrite needed:

- `hven::StopSource` / `hven::StopToken` (`<hven/core/stop.h>`, also in the
  umbrella `<hven/hven.h>`): a cooperative stop request.
  `StopSource::request_stop()` may be called from any thread, lock-free.
- `SolveBudget::stop` (a `StopToken`, default empty): hand one to a solve to be
  able to stop it. The solve ends at its next iteration boundary with
  `SolveStatus::kInterrupted` and `StopReason::kStopRequested`, returning the
  current iterate. A token is per solve: an already-requested source stops the
  next solve at its first boundary.
- `SolveResult::stop_reason` (`StopReason`), on both engines' results.
- `StopReason::kStopRequested`.
- Until group G2, the token is not forwarded into the SQP restoration sub-solve:
  a request made during restoration takes effect when that sub-solve returns.

Breaks:

| before | after | rewrite |
|---|---|---|
| `hven::IpmStopReason`, `hven::SqpStopReason` | `hven::StopReason` (their union, one vocabulary for both engines) | rename the type; values below |
| `ipm_solver.last_stop_reason()`, `sqp_solver.last_stop_reason()` | removed | `result.stop_reason`; after a solve an exception escaped, `solver.last_result().stop_reason` (`StopReason::kException`) |
| `IpmStopReason::kInterrupted`, `SqpStopReason::kInterrupted` | `StopReason::kCallbackStop` | rename; a token stop is the separate `StopReason::kStopRequested` |
| `IpmStopReason::kIterationLimit`, `kWorkLimit`, `kTimeLimit` | `StopReason::kNone` | read `result.status` (`kIterationLimit`, `kWorkLimit`, `kTimeLimit`): the kNone rule is "the status names the cause" on both engines now |
| every other `IpmStopReason::kX` / `SqpStopReason::kX` | `StopReason::kX` | rename the qualifier |
| `IpmPhaseReport::stop_reason` of type `IpmStopReason` | `IpmPhaseReport::stop_reason` of type `StopReason`, same kNone rule | as the three rows above |
| `to_string(IpmStopReason)`, `to_string(SqpStopReason)` | `to_string(StopReason)` | rename; spellings below |
| trace `ipm.phase.exit` key `stop_reason`: `iteration_cap`, `work_limit`, `time_limit`, `interrupted` | `none`, `none`, `none`, `callback_stop`; new `stop_requested` | a trace reader that branched on the old strings reads `status` for the three limits and `callback_stop` for the callback's stop |
| interior corpus CSV (`hven_sqp_corpus --engine interior`) column `stop_reason` | the same map as the trace key | baselines re-derived through the declared column map (`bench/baselines/2026-11-m8-ap5-g1-ipm-leg/`) |
| SQP: a callback `kStop` on row `k` was honoured at row `k+1`'s exit (one more step taken) | honoured right after row `k` when `k` records an accepted step: the step is not committed, the history ends on row `k` (`qp_solved == true`, `history.size() == counters.major_iters`), the result is row `k`'s point. On any other row, unchanged (the next exit test, at the same point) | a consumer that indexed `history[counters.major_iters]` after a callback stop reads `history.back()` (SqpCounters' note already says so) |
| `SolveBudget` was trivially copyable | it carries a `StopToken` (a `shared_ptr`), so it is no longer trivially copyable and no longer a literal type: it cannot be a `constexpr` object, and `memcpy`-style copies are invalid | still an aggregate, and `SolveBudget{cap, minors}` still compiles at run time; build a `constexpr` budget's members separately, or make the object `const`/non-constexpr |
| `ipm_solver.last_stop_reason()` read from inside the iteration callback (mid-solve) | removed with no G1 replacement: `result.stop_reason` exists only once the solve returns, and `IterationEvent` carries no stop reason | none in G1; a callback that needs to know why an earlier phase ended reads `status` on the returned result |
| `StopReason` integer values | renumbered against BOTH old enums (table below) | never persist or compare the integers across the change; compare enumerators, or `to_string` |
| IPM: `kStageStalled` / `kRestorationLocallyInfeasible` recorded beside a converged status (a stall at an acceptable iterate resolves `kAcceptable`) | `result.stop_reason` and the phase's `stop_reason` read `kNone` whenever the status is `kOptimal` or `kAcceptable` | read `status` first |

The integer values, old to new (`IpmStopReason` / `SqpStopReason` to `StopReason`):

| enumerator | `IpmStopReason` | `SqpStopReason` | `StopReason` |
|---|---|---|---|
| `kNone` | 0 | 0 | 0 |
| `kCallbackStop` (was `kInterrupted`) | 4 | 1 | 1 |
| `kStopRequested` | -- | -- | 2 |
| `kException` | 10 | 2 | 3 |
| `kNonFiniteStart` | 7 | 3 | 4 |
| `kNonFiniteIterate` | 8 | 4 | 5 |
| `kEvaluationFailure` | 9 | -- | 6 |
| `kStageStalled` | 3 | -- | 7 |
| `kRestorationLocallyInfeasible` | 2 | -- | 8 |
| `kSubproblemFailure` | -- | 5 | 9 |
| `kInconsistentEqualities` | -- | 6 | 10 |
| `kRestorationRefused` | -- | 7 | 11 |
| `kRestorationNonFinite` | -- | 8 | 12 |
| `kRestorationNoClaim` | -- | 9 | 13 |
| `kRestorationFailed` | -- | 10 | 14 |
| `kIterationLimit`, `kWorkLimit`, `kTimeLimit` (1, 5, 6; IPM) | 1, 5, 6 | -- | folded into `kNone` |

A token stop has its own `ipm.message` kinds, `stop_requested_at_iteration` and
`stop_requested_skipping_phases` (console: "Solve stopped by a stop request
(SolveBudget::stop) ..."); the callback's `interrupt_at_iteration` and
`interrupt_skipping_phases` are unchanged and are not emitted on a token stop.
A trace reader that switches over the kind exhaustively adds the two.
