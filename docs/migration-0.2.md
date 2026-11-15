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

## G3 -- best-so-far, `kStopReturnBest`, `snapshot()`

Observational: nothing here moves a trajectory, a counter or a status. Everything is
new, so there is no rewrite.

- `IterationSnapshot` (`<hven/drivers/solve_result.h>`): an owning copy of an
  `IterationEvent`, with the event's field names (`iteration`, `recovering`, `phase`,
  `depth`, `f`, the three diagnostics, `step_norm`, `radius`, `mu`, `x`, `lambda`, `z`,
  `elapsed_seconds`) plus `g`, the row values in `lambda`'s row space. `snapshot(event)`
  is a deep copy of what the event's payload mask delivered (`g` is left empty: an event
  carries no row values). It never aliases engine storage, so another thread may read it
  and it survives the solve. The vectors are empty and the diagnostics NaN where the
  event delivered none.
- `SolveResult::best_iterate` (`std::optional<IterationSnapshot>`): the best iterate
  the solve visited, with `x`, `lambda`, `z`, `g`, `f` and the three diagnostics
  filled. Present when the tracker was armed and the solve did not end `kOptimal` or
  `kAcceptable` (or the best was substituted for the primary point, whatever the
  status); otherwise `nullopt`. **Armed** when a return-best option is on
  (`IpmOptions::return_best`, `SqpOptions::return_best_on_max_iterations`), an
  iteration callback is installed, or `SolveBudget::stop` can fire. Best by each
  engine's own ordering: interior-point, `IpmOptions::best_iterate_criterion` over the
  last phase that ran, never a restoration iterate; SQP, feasibility first (`h`, then
  `f`) over the caller's own solve, never a restoration sub-solve's row. Tracking
  copies and never steers: an armed solve is the unarmed solve, counter for counter.
  The cost is O(n+m) per improving iterate, none when unarmed.
- `CallbackAction::kStopReturnBest`: stops as `kStop` does, and the returned primary
  point is `best_iterate` (`returned_best_iterate == true`, `message` ends "; returned
  the best iterate"). With no best recorded the result is `kStop`'s and
  `returned_best_iterate` is false. A token stop returns the current iterate as before
  and reports the best in `best_iterate` only. Interior-point: on a stop the current
  point is scored into the tracker first (the ordinary tracking runs after an
  iteration's step, which a stop never reaches), so a stop at an improving point
  returns it. On a `recovering` event it returns the best, not the restoration point
  the callback was shown. SQP: on a depth-1 event it returns the parent's best, not the
  restoration-entry point a `kStop` gets (`returned_pre_restoration_point` false).
  Converged beats stop, as for `kStop`.

Per-field notes on `best_iterate`: `iteration` is the iterate's own row/iteration
index (SQP: its `history` index; interior-point: the per-phase index); `step_norm`
and `phase`/`depth`/`radius`/`mu` are those of its own event; `elapsed_seconds` is the
time the iterate was reached. On `returned_best_iterate` solves the primary fields
and `best_iterate` agree bit for bit on an unscaled solve, with two exceptions: in an
interior-point phase that is not objective-bearing `best_iterate.f` is NaN (nothing
measured the objective there) while the primary `f` is the objective evaluated at the
returned point; and on a scaled SQP solve the primary diagnostics are re-measured at
the caller's scale and may differ from `best_iterate`'s in the last digits.
`returned_best_iterate` implies `best_iterate` is set, on both engines, `kAcceptable`
exits included; the SQP engine's `best_iterate` inequality multipliers carry the same
sign repair as the primary's.

Not in this group (arrive with the payload group): `g`, `violation`, `z_lower` and
`z_upper` on `IterationEvent`, the `IpmIterationInfo`/`SqpIterationInfo` blocks, and the
matching `IterationSnapshot` fields. `IterationSnapshot` mirrors the event as it is
today.

## G4 -- stops inside an iteration

A requested `SolveBudget::stop` is now also seen inside an iteration, at the two
places where abandoning the work loses nothing: interior-point, between
inertia-correction attempts; SQP, once the major's subproblem is built and in the
minor loop of that subproblem (walk, SSN or IPQP, the SSN warm grade the IPQP chain
routes to, and the L-BFGS lift that wraps any of them) and of its elastic re-solves,
before any trial point is evaluated. These are not stop points: the line search, the
second-order correction's re-solve, the watchdog, the SQP cap retry
(`qp_iteration_limit_retry`, never armed) and the interior-point dense guard's re-run of
the inertia ladder; a request made during them is honoured at the next boundary. A
solve without a token is unchanged, and so is one whose token is never requested.

After an SQP stop inside a subproblem, the warm-start export can differ from a stop at
the top of the same major. Its hot handle is the state the abandoned solve left the
walk engine in, so a later solve may forfeit the hot start and start warm. Two
counters/stamps follow the abandoned subproblem rather than the previous major:
`counters.start_level_used` (major 0 only: what the abandoned first subproblem
observed) and the export's dual-regularization stamp (the walk configuration the
abandoned subproblem was dispatched at). Neither moves the returned point, its
multipliers or its history row. A stop that arrives after the subproblem is built but
before a kernel starts leaves both as a top-of-iteration stop does.

With G3's best tracker. An intra-iteration stop is a token stop, so it never substitutes
the best iterate; `best_iterate` is published as at any stop (the tracker is armed
whenever a token can fire). Interior-point: a stop between ladder attempts scores the
iteration's start point into the tracker once, in the same exit as the iteration-top
stop, so both publish the same `best_iterate`. No `kStopReturnBest` can be pending
there: the callback ran before the iteration-top poll, which would have taken it. SQP:
the abandon is labelled the token's before the abandoned row's event fires, so a
`kStopReturnBest` answered on that row is what it is on the exit test's token-stopped
row, a no-op (`x_k` is returned, `returned_best_iterate` false). Inside the restoration
sub-solve, the same answer on the sub-solve's abandoned row (a depth-1 event) takes
G3's depth-1 rule, as on its exit test's row: the parent's best is returned with
`kCallbackStop`.

Behaviour changes (stop path only):

| before | after | rewrite |
|---|---|---|
| IPM: a token requested while the inertia ladder ran was seen at the next iteration's top, after the iteration's step | the iteration is abandoned between ladder attempts and the solve returns the point it started from, the same result (point, multipliers, diagnostics, `g`, warm start, message) as a stop at that iteration's top; the ladder's factorizations already spent are counted in `work.factorizations`. Inside restoration the G2 rule holds: the point restoration was entered from | none |
| SQP: a token requested while a subproblem was being solved was seen after the major's trial, at the next exit test or the post-push poll | the subproblem is abandoned and the major records the row for its own iterate `x_k` (`qp_solved == false`, the exit test's row shape), whose event fires; the solve returns `x_k` with `history.size() == counters.major_iters + 1` (the abandoned major is not counted; its work is: factorizations in `counters.factorizations`, minors where a handed-off subproblem's go -- `qp_minor_iters` for the walk, `ssn.ssn_iters` and `ipqp.ipqp_iters` for the tiers). The abandoned kernel leaves through its iteration-budget exit, so a stopped SSN or IPQP subproblem is counted (and, under a trace, reported) as a budget escape; it is not counted as a routing destination (`ipqp_to_ssn`, `ipqp_to_refine`), so the IPQP routing partition (`IpqpCounters::ipqp_to_walk`'s note) holds over the subproblems that were not abandoned. Inside the restoration sub-solve the G2 rule holds: the point restoration was entered from (the parent's best instead under a depth-1 `kStopReturnBest`, as above) | a consumer that expected the last row of a token-stopped solve to carry a solved subproblem reads `qp_solved` |

## Zero-step short-circuit (driver numerics, not an interface change)

The SQP driver's zero-step short-circuit (KLV Algorithm 2's `||d|| = 0`: accept an
unjudged step the subproblem answered with p = 0) now reads the QP kernels' own no-motion
tolerance, `qp_no_motion_tol(qp.feasibility_tol, ||p||inf)` (default 1e-9), instead of the
fixed `1e-12 * max(1, ||x||inf)` it used before. `kZeroStepScale` (1e-12) remains the
L-BFGS pair-skip scale only.

Why the short-circuit exists: the convergence test measures stationarity with the iterate's
current multipliers, refreshed only on an accepted step. At a point where the subproblem's
answer is p = 0 the QP has just priced the exact multipliers, yet the funnel rejects the
zero step on rounding alone (pred_df and actual decrease are both noise), the multipliers
are thrown away and the radius halves forever. Measured on the restoration phase's own
feasibility problem entered at an infeasible stationary point: 64 consecutive rejections.

Why 1e-12 was wrong: the walk counts a step within `feasibility_tol * max(1, |x|)` as no
motion and snaps onto it, so its answer at such a point is only determined to that scale.
A platform whose rounding leaves 1e-10 where another leaves exactly 0 (macOS/Accelerate
returned 1.07e-10 inside the restoration sub-solve on the T7F1 and T3 fixtures; MKL
returned 0) put the noise step between the two thresholds: the driver judged it, the
funnel rejected it 34 times, the multipliers were discarded, the trust region collapsed
and the solve ended kNumericalError uncertified. The driver now agrees with the kernel.

The old argument ("four to six decades tighter than kkt_tol, so a zero step cannot move any
residual the driver measures") is restated for 1e-9: it is three decades below the default
kkt_tol (1e-6). An accepted zero step moves x by at most that, so any residual it moves is
moved by at most ||A|| * 1e-9, below the 1e-6 tolerance the driver tests against. The walk
is the only kernel with a no-motion test; SSN and IPQP answers are accurate to the same
`qp.feasibility_tol`, so one definition serves all three modes. The scale is the step's own
size, as in the walk (whose iterate is the step): for a step under the tolerance the
tolerance is absolute, `qp.feasibility_tol`.

## Walk kOptimal's row contract (M8 kopt-1x)

The QP walk's `kOptimal` promises that every equality row and every working inequality row is within 1x of
`row_tolerance(row_scale, lambda)` (the tolerance still includes the `dual_mu * |lambda|` footprint). The 10x margin
(`kInfeasibilityMarginFactor`) stays only as the evidence margin of the `kInfeasible` classifier.

Why: the KKT-point test reused the classifier's 10x predicate, so a point with a row 1x to 10x off was certified where
it stood, and nothing downstream repaired it (a gray-zone row carries complementarity near 1e-9 and stationarity at
rounding, so the terminal polish's entry test stayed closed). YORKNET qp5 on an Intel AVX-512 runner read row 246 at
6.88x. A flat-normal synthetic (n = 2, H a projector onto null(a), seed k floor units off the row) reproduces it
for k = 1.5, 3, 6.9, 9.5.

What changed (`src/qp/qp_engine.cpp`):

- `polish_footprint_open` gained a third disjunct, any equality or working row past `row_tolerance`
  (`worst_row_ratio > 1`). A row-triggered entry floors the acceptance test's stationarity comparison at
  `kTerminalPolishStatFactor * opt_tol`, so an incumbent whose stationarity is at rounding cannot veto the candidate
  that fixes its rows.
- A KKT point with nothing to drop and a row past 1x takes the pass's ordinary step instead of certifying (at most one
  consecutive hold; the polish repairs what a step cannot). The hold sits at the terminal site only. Moving the 1x
  requirement into the per-pass stationarity test itself changed mid-walk drop decisions and sent ACOPP118 qp1 (border)
  to the iteration cap at 4710 minors; see `docs/notes/data/2026-11-m8-kopt-1x/`.
- `worst_structural_violation` and `violation_is_structural` keep the 10x: they are the infeasibility classifier
  (terminal classification and its refinement re-reads, the equality-drift check, the certificate's bound half).
- New counter `optimal_row_gray_exits` (`QpCounters`, `SqpCounters`, trace key after `dense_factorizations`):
  kOptimal exits whose worst row is still past 1x. 0 is the contract; a later census decides whether a nonzero count
  becomes a hard `kNumericalError` refusal.

Consumers: `QpCounters`/`SqpCounters` gain one trailing field (aggregate initialization by position, trace keys by
name); a walk subproblem's route can differ by a step where a row sat between 1x and 10x.

## G5 -- the widened payload

Every addition is delivered only to a callback that asks for it, so nothing here moves a
trajectory, a counter or a status. The four engine-specific top-level optionals of
`IterationEvent` move into one block per engine.

New, no rewrite needed:

- `IterationPayload::kConstraints`: `IterationEvent::g` (the row values at `x`, the
  definition of `SolveResult::g`: a free Nlp row reads NaN) and `IterationEvent::violation`
  (the row violation, `>= 0`, 0 on a free row: `|c_E|` on an equality row, the positive
  part of the violated side on an inequality or range row), both in `lambda`'s row space.
- `IterationPayload::kBoundMultipliers`: `IterationEvent::z_lower`, `z_upper`, declared
  width, both `>= 0`, with `z_lower - z_upper == z`. Interior-point: the engine's own
  per-side multipliers, divided by the objective scale and scattered as `z` is (equal to
  `z` to rounding; an eliminated or fixing-row coordinate takes the sign split of its
  `z`). SQP: the sign split of `z` (exact).
- `IpmIterationInfo` (`IterationEvent::ipm`): `phase`, `mu` (both moved, below), and the
  facts of the step that REACHED the iterate: `alpha_primal`, `alpha_dual`,
  `regularization_primal` (the inertia-correction delta of that step's factorization,
  `IpmIterate::h_pert`), `regularization_dual` (the constraint-row shift: the proximal
  mode's `delta_c`, or the classic ladder's when it engaged one; 0 if none),
  `inertia_corrections` (`h_facs`, -1 when the direction came back non-finite),
  `line_search_trials`. At a phase's first iterate the alphas are NaN and the rest 0 (as
  `step_norm` is 0). The two regularizations are the engine's KKT values, at its
  objective scale.
- `SqpIterationInfo` (`IterationEvent::sqp`): `depth`, `trust_radius` (both moved, below),
  `violation_l1` (h(x), the funnel's measure), `regularization_dual` (`SqpIterate::mu`; the
  same name and quantity as the interior-point block's, both at the engine's objective
  scale while `IpmIterationInfo::mu` is at the caller's),
  `qp_solved`, `qp_status`, `qp_minor_iterations`, `active_set_size` (active rows plus
  active lower and upper bound sides), `verdict`, `elastic` -- the subproblem solved AT the
  major's point, the row's own columns (caller units on a scaled solve).
- Exactly one of `ipm`/`sqp` is engaged. Both are delivered under every mask (copies only).
- `IterationSnapshot` mirrors the event: `violation`, `z_lower`, `z_upper`, `ipm`, `sqp`
  added; `snapshot(event)` copies every view the mask delivered, `g` included (it is no
  longer left empty). `SolveResult::best_iterate` fills every vector (`x`, `lambda`, `z`,
  `g`, `violation`, `z_lower`, `z_upper`) and its own event's engine block.

Two notes on the event rows. The SQP engine composes `g`/`violation` whenever the mask
asks for them and the point was measured, so a problem whose rows are all free (an Nlp with
`m > 0` and no bounded row) still gets `m` NaN values and zero violations, as IPM events,
`SolveResult::g` and `best_iterate` do. `SqpResult::best_iterate.lambda` carries the export
boundary's sign repair of the inequality prices and may differ from the lambda of its own
event.

Where the new views are empty: outside their mask bit; on an interior-point `recovering`
event (no caller-space evaluation of a restoration iterate exists; `x`, `lambda`, `z` are
still delivered); on an SQP row with no finite measurement (`g`, `violation`). On an SQP
depth-1 event they are the restoration sub-problem's own (its rows, its variables, elastic
ones included), as `x` and `lambda` already were; on a column-scaled solve the first n
entries of `z_lower`/`z_upper` are in caller units, as `x` and `z` are.

`step_norm` and the engine blocks keep each engine's timing (design section 3.1, C5):
interior-point, the step that reached the point (the event fires before the iterate
factorizes); SQP, the step taken from it and the subproblem solved at it (the event fires
once that subproblem is solved, so the event, the `history` row and the `sqp.major` line
stay one row). An `IterationSnapshot` and `best_iterate` carry the same quantity as the
event on the same engine.

Breaks:

| before | after | rewrite |
|---|---|---|
| `IterationPayload::kAll` == `kVectors \| kDiagnostics` | `kAll` adds `kConstraints \| kBoundMultipliers` | a consumer that wants exactly the old payload passes `kVectors \| kDiagnostics` |
| `set_iteration_callback(cb)` (default mask `kAll`) | default mask `kVectors \| kDiagnostics` (public as `hven::kDefaultIterationPayload`), the old `kAll` payload; `iteration_payload()` reads the same when no callback is installed or after a clear / an empty callable | none for the delivered payload; a test that compared `iteration_payload()` with `kAll` after a default attach or a clear compares with `kVectors \| kDiagnostics` |
| `IterationEvent::phase` | `IterationEvent::ipm->phase` | `e.phase.value_or(d)` -> `e.ipm ? e.ipm->phase : d` |
| `IterationEvent::mu` | `IterationEvent::ipm->mu` | as `phase` |
| `IterationEvent::depth` | `IterationEvent::sqp->depth` | `e.depth.value_or(0)` -> `e.sqp ? e.sqp->depth : 0` (or read `e.recovering`, which is `depth >= 1` on the SQP) |
| `IterationEvent::radius` | `IterationEvent::sqp->trust_radius` | as `depth` |
| (G5 development name) `SqpIterationInfo::dual_regularization` | `SqpIterationInfo::regularization_dual`, the interior-point block's name for the same quantity | rename; never in a tagged release, listed for consumers that tracked `m8` |
| `IterationSnapshot::{phase, mu, depth, radius}` | `IterationSnapshot::ipm`/`sqp`, as on the event | as on the event |
| `IterationEvent` member order: `iteration, recovering, phase, depth, f, stationarity, feasibility, complementarity, step_norm, radius, mu, x, lambda, z, elapsed_seconds` | `iteration, recovering, f, stationarity, feasibility, complementarity, step_norm, elapsed_seconds, x, lambda, z, g, violation, z_lower, z_upper, ipm, sqp` | designated initialisers in the new order; the seven `Eigen::Ref` views have no default and must all be bound |
| `IterationSnapshot` member order | `iteration, recovering, f, stationarity, feasibility, complementarity, step_norm, elapsed_seconds, x, lambda, z, g, violation, z_lower, z_upper, ipm, sqp` | designated initialisers |
| `best_iterate->lambda`/`g` mapped into the row space by `Problem` from private engine blocks | composed by the engine with the event's own row map, as an event's are | none; an internal (detail) route with no `Problem` now gets the engines' `[lambda_e; lambda_i]` / `[c_E; c_I]` concatenation, as its events do, where it got empty vectors |
