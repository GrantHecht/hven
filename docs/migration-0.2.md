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

## G2 -- recovery-aware stop

Mechanics behind `SolveBudget::stop` and the callback's `kStop` inside feasibility
restoration. Interior-point: the token is polled at the top of every iteration,
restoration iterations included; the point restoration was entered from is stashed
at entry (only when a token that can fire is installed), and a callback `kStop` on a
`recovering` event instead returns the point that event showed. SQP: the token is
forwarded into the restoration sub-solve, which polls at its own boundaries; a
callback `kStop` on a depth-1 row is forwarded the same way (under
`CallbackScope::kIncludeRestoration`); the entry point is stashed only when one of
the two can reach the sub-solve. Open for the API revisit: the two engines differ on a
callback `kStop` on a recovering event (interior-point returns the shown point, SQP
the entry point).

New, no rewrite needed:

- `IterationEvent::recovering` (`bool`, declared right after `iteration`): the
  event's point is inside feasibility restoration. Interior-point: restoration
  is in place, so its rows are part of `iterations` and are delivered (under
  either `CallbackScope`) with the flag set; `f` and the three diagnostics are
  NaN there. SQP: `depth >= 1` (those rows exist only under
  `kIncludeRestoration`). A consumer that plots caller-space quantities filters
  on it. Interior-point: the point a token stop returns from inside restoration is
  the last non-recovering event's, also at the stall door (that iteration is
  popped before dispatch).
- `SolveResult::returned_pre_restoration_point` (`bool`, beside
  `returned_best_iterate`): true when the returned point is the one feasibility
  restoration was entered from, because a stop cut restoration short (token, or a
  callback `kStop` on an SQP restoration row). Only with `kInterrupted`. The
  `message` suffix stays; read the field, not the text.

Behaviour changes (stop path only; a solve nobody stops is bitwise unchanged):

| before | after | rewrite |
|---|---|---|
| IPM: a `SolveBudget::stop` request landing while restoration (proximal or nested l1) was active returned the restoration iterate | returns the point restoration was entered from (taken before the entry's multiplier initialisation: its `x`, `lambda`, `z`, `f`, diagnostics and `g`), and `message` ends "; returned the point before feasibility restoration". A callback `kStop` on a `recovering` event is unchanged: the point that event showed | a consumer that read `result.x` after a token stop as "the iterate restoration was working on" reads it as the last non-restoration point; filter on `IterationEvent::recovering` to tell which kind of point a callback is being shown |
| IPM: the `warm_start` of such a stop | the entry point's, with no polish extension (the same conservative capture every restoration-flagged exit takes) | none |
| SQP: a stop (callback `kStop` on a depth-1 event, or a requested token) inside the restoration sub-solve returned the restored point `x_r` (or resumed first and stopped at the next major) | the token is forwarded into the sub-solve; a stop that cuts the sub-solve short ends the solve `kInterrupted` at the point restoration was entered from (the requesting major's iterate and multipliers), never resumes, earns no certificate, and sets `returned_pre_restoration_point` | a consumer that read `result.x` after such a stop as the restored point reads the requesting iterate; the sub-solve's own point is no longer returned |
| SQP: a callback `kStop` on the restoration sub-solve's converged terminal row exited `kInterrupted` around the certificate it had earned | converged beats stop inside the sub-solve, so the stop is a no-op for it: the exit is the unstopped run's (`kInfeasible` at `x_r`, `infeasibility_certified`, `(x, lambda, z)` the certificate), as the token already behaved on that row. When that phase instead resumes (a feasible restored point), the latched stop ends the solve at the restored point at its next boundary, `returned_pre_restoration_point` false | a consumer that treated `kInterrupted` + `infeasibility_certified` as the pair for this row reads `kInfeasible` |
| SQP: a token stop during restoration took effect only when the sub-solve returned (G1's interim note) | takes effect at the sub-solve's own next boundary | none; the interim caveat is gone |
| the interior-point `set_iteration_callback` documentation called nested l1 restoration "silent" | corrected: restoration is in place and its rows are delivered with `recovering` set | none |
| `IterationEvent` positional aggregate initialisation | `recovering` is the second member, so a positional `IterationEvent{iteration, phase, ...}` shifts by one | use designated initialisers, or insert `false` after `iteration` |
