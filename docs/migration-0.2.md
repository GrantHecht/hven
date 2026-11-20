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

- `IterationSnapshot` (`<hven/solver/solve_result.h>`): an owning copy of an
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
  iteration callback is installed, or `SolveBudget::stop` can fire. Best by the
  one shared ordering (see G-RB): interior-point over the last phase that ran, never a
  restoration iterate; SQP over the caller's own solve, never a restoration sub-solve's row. Tracking
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

What changed (`src/qp/walk/walk_engine.cpp`):

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

Round 2 (M8 kopt-1x r2; `docs/notes/data/2026-11-m8-kopt-1x-r2/`). On an Intel AVX-512 runner YORKNET qp5 still certified
row 246 at 6.88x, so the repair paths changed (no hard `kNumericalError` gate; that waits for a census that finds the
gray zone empty):

- The polish's acceptance also reads each side's `worst_row_ratio` with its OWN multipliers and requires the
  candidate's to be within `max(1, incumbent's)`; a row-triggered entry prefers a stage-1 candidate within 1x and
  falls back to stage 1 as it was if stage 2 adopts nothing. (A re-priced multiplier set shrinks the `dual_mu*|lambda|`
  footprint and used to expose a row the incumbent's covered.)
- The gray hold is spent only by an ordinary step that runs; a rank-repair or border-retreat `continue` leaves it.
- After polish adoption, refusal or revert, the 1x ratio is re-read with the final multipliers; past 1x the walk
  goes back for one ordinary step (the next KKT point holds), at most `kGrayReturnBudget` (16) times per solve, then
  certifies and counts `optimal_row_gray_exits`.
- The verdict-site face refinement keeps the classifier's 10x target and trigger. A 1x target was built and measured
  and not enabled: run at a gray row it changed whole SQP trajectories (ACOPP30 615 -> 2494 minors), and asked for at a
  structural dead end it moved the classifier's refined point (a T7 pin: 9 -> 10 steps, residual 2.943e-4 -> 2.981e-4).
  A refined point that certifies past 1x is covered by the return above.
- The row contract reads a working INEQUALITY row one-sided (violation only): the first fix's read counted the slack of
  a zero-multiplier working row as a violation (ratio 1e8 on CRESC4 at a point 415 inside its row), so its hold and its
  gray-exit counter fired on points that were primal feasible.

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

## G6 -- `IpmCounters` and the regrouped `IpmResult`

Names move; no value changes. The interior-point counters that lived flat on `IpmResult`
are one group now, `IpmResult::counters` (`IpmCounters`, `hven/core/solver_counters.h`,
with its `HVEN_IPM_COUNTERS_FIELDS` table beside the SQP one), and the nine `last_*`
fields move onto the phase they describe. A name shared with `SqpCounters` has the same
meaning there.

| before | after | rewrite |
|---|---|---|
| `r.nonfinite_step_rollbacks` | `r.counters.nonfinite_step_rollbacks` | prefix |
| `r.polish_ignored` | `r.counters.polish_ignored` | prefix |
| `r.payload_ignored` | `r.counters.payload_ignored` | prefix |
| `r.watchdog_activations` | `r.counters.watchdog_activations` | prefix (not `SqpCounters::watchdog_restores`: arms, not restores) |
| `r.recovery_depth_histogram` | `r.counters.recovery_depth_histogram` | prefix |
| `r.second_order_corrections` | `r.counters.soc_steps` | rename, the SQP counter's name |
| `r.kkt_analyses_this_call` | `r.counters.symbolic_analyses` | rename (`kkt_analyses_total`, a solver-lifetime figure, stays on `IpmResult`) |
| `r.dense_residual_ratio_triggers` | `r.counters.dense_solve_residual_refusals` | rename, the SQP counter's name (one threshold, `kDenseResidualRatioSingular`) |
| `r.dense_factorizations` | `r.counters.dense_factorizations` | prefix |
| (`r.work.factorizations`) | `r.counters.factorizations` also | new; equal to `work.factorizations` |
| `r.last_feasibility_restoration_entries` (`optional`, last phase) | `r.counters.restoration_entries` (`Index`, whole solve) | the sum of the per-phase values; 0 when no restoration strategy is constructed (was `nullopt`) |
| `r.last_feasibility_restoration_iterations` (`optional`, last phase) | `r.counters.restoration_iters` (`Index`, whole solve) | as above |
| `r.last_funnel_width` | `last_ran_phase(r)->funnel_width` | `optional<double>`, nullopt rules unchanged |
| `r.last_filter_size` | `last_ran_phase(r)->filter_size` | `optional<Index>` |
| `r.last_filter_resets` | `last_ran_phase(r)->filter_resets` | `optional<Index>` |
| `r.last_monotone_switches` | `last_ran_phase(r)->monotone_switches` | `optional<Index>` |
| `r.last_monotone_iterations` | `last_ran_phase(r)->monotone_iterations` | `optional<Index>` |
| `r.last_feasibility_restoration_entries` | `r.phases[i].restoration_entries` | the per-phase value, `optional<Index>` (the counter above is its sum) |
| `r.last_feasibility_restoration_iterations` | `r.phases[i].restoration_iters` | the per-phase value, `optional<Index>` |
| `r.last_proximal_regularization_primal` | `last_ran_phase(r)->proximal_regularization_primal` | `optional<double>` |
| `r.last_proximal_regularization_dual` | `last_ran_phase(r)->proximal_regularization_dual` | `optional<double>` |
| `r.last_kkt_info` (`Eigen::ComputationInfo`) | `last_ran_phase(r)->kkt_factor_status` (`IpmKktFactorStatus`) | the Eigen enum no longer leaks; names `kSuccess`, `kNumericalIssue`, `kNoConvergence`, `kInvalidInput`; a plain enum, default `kSuccess` (it was never absent) |
| `ipm.phase.exit` key `last_kkt_info` | `kkt_factor_status` | trace key; the event field `IpmPhaseExitTraceEvent::last_kkt_info` is renamed the same way |
| `IpmSolveRecord` (no counters) | `IpmSolveRecord::counters` (`IpmCounters`) | new; its `analyses`, `soc_steps_taken`, `watchdog_activations` keep their names and values and are read from it |

New free function `const IpmPhaseReport *last_ran_phase(const IpmResult &)`: the last
entry of `phases` whose `ran` is true, or `nullptr` when none ran. The retired `last_*`
fields read "the last phase wins", which is this phase; a skipped conditional phase is
passed over exactly as before.

What did not move, stated so nobody looks for it:

- The `ipm.solve.end` line has no counter key and gains none. The only trace key of this
  group that renames is `ipm.phase.exit`'s `kkt_factor_status` (above); every other line
  is byte-identical.
- `IpmCounters::{start_level_used, inertia_corrections}` (design 2.1) are NOT in this
  commit: both are new values, not renames, and design section 5 puts `start_level_used`
  with G7. Neither exists on the result yet.
- `IpmPhaseExitTraceEvent` carries no key for the ten new phase diagnostics (they are
  filled after the exit line is written).

Meaning changes on a multi-phase solve only: `restoration_entries` and `restoration_iters`
are the whole-solve sums, where `last_feasibility_restoration_*` read the last phase. On a
single-phase solve (the default) the two readings coincide, so no corpus value moves; the
declaration counted the corpus cells that are multi-phase and enter restoration before the
first leg.

Consumer catch-up: `docs/notes/data/2026-11-m8-ap5-g6/CATCH-UP.txt`.

## G7 -- the remaining symmetry, and one stop rule in restoration

Renames and moves, two new counters per engine-side gap, one throw-path field, and
two stop-path behaviour changes. A solve nobody stops and nobody throws out of is
bitwise unchanged.

| before | after | rewrite |
|---|---|---|
| `IpmResult::convergence.kkt` | `convergence.stationarity` | rename (Q7): `SqpConvergence`'s word |
| `IpmResult::convergence.barrier` | `convergence.complementarity` | rename (Q7) |
| (none) | `IpmResult::convergence.feasibility` | new: `max(equality, inequality)`, NaN if either is |
| `IpmOptions::termination.acceptable_kkt_tol` | `termination.acceptable_stationarity_tol` | rename (Q7); validation messages and `dump_options` lines follow |
| `IpmOptions::termination.acceptable_barrier_tol` | `termination.acceptable_complementarity_tol` | rename (Q7) |
| `IpmOptions::termination.divergence_kkt_tol` | `termination.divergence_stationarity_tol` | rename (Q7) |
| `IpmOptions::termination.divergence_barrier_tol` | `termination.divergence_complementarity_tol` | rename (Q7) |
| `SqpResult::engine_seconds` | `SqpResult::timing.engine_seconds` (`SqpTiming`) | prefix (R4); both results read `result.timing.*` |
| `ipm_worker_options(o)` | `worker_options(o)` | rename (E4); a `worker_options(SqpOptions)` overload is new (threads 1, silent) |
| `SqpSolver::ssn_tier_threads()`, `ipqp_tier_threads()` | `detail::SqpSolverAccess::ssn_tier_threads(solver)`, `ipqp_tier_threads(solver)` | moved off the public handle (E3); test introspection |
| (none) | `SqpCounters::payload_ignored` | new (R7): 1 when a non-empty warm start (payload or native) was ignored because `common.start_level` is kCold; appended last, so `sqp.solve.end`'s counters object gains one trailing key |
| (none) | `IpmCounters::start_level_used` | new (R7): the level the payload was applied at -- kWarm (its point), kSeeded (multipliers only, set when the phase they seed is reached), else kCold |
| (none) | `IpmCounters::inertia_corrections` | new (design 2.1): inertia-correction factorizations the iteration ladders took, whole solve |
| SQP `last_result().z` empty | the bound multipliers at the returned point | E2: read off the iterate's last evaluation (no model call); empty when the solve holds none for that point |

Notes, so nobody looks for what is not there:

- `IpmConvergence::feasibility` is NOT `SqpConvergence::feasibility`'s norm. The
  interior-point `inequality` is the slack-form residual (each inequality row with
  its slack), and the interior iterate never violates a variable bound; the SQP
  measure is `max(|cE|, max(cI)+, bound violation)` in infinity norm. The two
  definitions coincide only on a problem with equality rows and no bounds. For a
  measure that is the same on both engines read the shared `SolveResult::feasibility`.
- The trace keeps its column names (`kkt_inf`, `barr_inf`, `acc_kkt_tol`, ...);
  renaming trace keys is the trace upgrade's.
- `IpmCounters::start_level_used` never reads kHot (the engine has no hot handle).
  It reads kCold on a seed whose phase never ran (a stop ended the sequence first,
  or the sequence has no optimize phase).
- `inertia_corrections` sums the ladder count each iteration's KKT factorization
  returned (plus the dense guard's re-run), counted where it is returned, so an
  iteration a restoration door then abandons still counts, and the `IpmIterate::h_facs`
  sentinel -1 (non-finite direction) does not subtract. A ladder a stop token cut
  short counts nothing (its factorizations are in `work.factorizations`).

Behaviour changes (stop path only; a solve nobody stops is bitwise unchanged). One
rule now holds on both engines and at every depth: **a stop that lands inside
feasibility restoration returns the point restoration was entered from**, whether a
token or a callback asked for it; and **a stop the token already decided is not
relabelled by a callback's answer**.

| before | after | rewrite |
|---|---|---|
| IPM: a callback `kStop` on a `recovering` event returned the restoration iterate it showed | returns the point restoration was entered from, as the token stop and the SQP already did: `returned_pre_restoration_point` true, the message suffix, the entry point's `warm_start` (no polish extension). `kStopReturnBest` with a recorded best still returns the best; with none, the entry point. The entry stash is now also taken when only a callback is installed | a consumer that wanted the restoration iterate takes `snapshot(event)` before returning `kStop` |
| SQP: `kStop`/`kStopReturnBest` answered on a restoration sub-solve's terminal row that the token had already stopped labelled the parent's exit `kCallbackStop` and, under `kStopReturnBest`, returned the parent's best (G4's depth-1 rule) | the sub-solve's own label decides: `kStopRequested`, the restoration-entry point, `returned_best_iterate` false -- what the same answer does on the parent's own token-stopped row | none |

G2's "open for the API revisit" line and G4's depth-1 `kStopReturnBest` sentence
describe the behaviour before this group.

Consumer catch-up: `docs/notes/data/2026-11-m8-ap5-g7/CATCH-UP.txt`.

## AP-4 -- API friction fixes (before the API lock)

Four small interface changes and their before and after. Each subsection stands alone.

### AP-4 G4 -- `solve(problem, x0, budget)`

Addition; nothing moves. Both `IpmSolver` and `SqpSolver` gain a cold-start overload that takes
a `SolveBudget` as the third argument:

| before | after |
|---|---|
| `solver.solve(problem, x0, {}, budget)` | `solver.solve(problem, x0, budget)` (the old spelling still compiles) |
| `solver.solve(problem, x0, {}, SolveBudget{.stop = token})` | `solver.solve(problem, x0, SolveBudget{.stop = token})` |

The overload is constrained to `SolveBudget` itself, so a bare `{}` as the third argument still
selects the `WarmStart` overload, and `{.stop = token}` without the type name is not a
`SolveBudget` initializer there (spell `SolveBudget{.stop = token}`). A `SolveBudget` lvalue or
prvalue selects the new overload, which forwards to `solve(problem, x0, WarmStart{}, budget)`.

### AP-4 G5 -- dense structure helpers

Addition; nothing moves. `<hven/model/nlp.h>` gains four free functions for the common case of
a dense Jacobian or a dense Hessian triangle:

| before | after |
|---|---|
| `{.jacobian_nonzeros = 8, ...}` and `r << 0,0,0,0,1,1,1,1; c << 0,1,2,3,0,1,2,3;` | `.jacobian_nonzeros = dense_jacobian_nonzeros(2, 4)` and `dense_jacobian_structure(2, 4, r, c);` |
| `{.hessian_nonzeros = 10, ...}` and `r << 0,1,1,2,2,2,3,3,3,3; c << 0,0,1,0,1,2,0,1,2,3;` | `.hessian_nonzeros = dense_lower_hessian_nonzeros(4)` and `dense_lower_hessian_structure(4, r, c);` |

The Jacobian order is row-major, `(0,0), (0,1), ..., (0,n-1), (1,0), ...`; the Hessian is the lower
triangle row by row, `(0,0), (1,0), (1,1), (2,0), ...`. `eval_jacobian` / `eval_hessian` write their
values in the same order. A size that cannot be a dense pattern (`m < 0`, `n < 1`, an overflowing
count) and an output whose length is not the pattern's throw `std::invalid_argument`. Examples
01, 06, 08 and 10 use them.

## G-RB -- one `return_best`, one "best", both engines

The two engines' return-best options become one `CommonOptions::return_best`, and "best" gets one
meaning. A solve with the option off (the default) is bitwise unchanged.

| before | after | rewrite |
|---|---|---|
| `IpmOptions::return_best = true` | `ipm.common.return_best = ReturnBest::kOnAnyNonOptimal` | break (Q5); `false` is `ReturnBest::kNever` |
| `SqpOptions::return_best_on_max_iterations = true` | `sqp.common.return_best = ReturnBest::kOnLimit` | break (Q5); `false` is `ReturnBest::kNever` |
| (none) | `hven::ReturnBest {kNever, kOnLimit, kOnAnyNonOptimal}`, `CommonOptions::return_best` | new; the dump prints `common.return_best` where `return_best` / `return_best_on_max_iterations` printed (line counts unchanged) |
| `IpmOptions::best_iterate_criterion` (`IpmBestIterateCriterion` `kEquality` default, `kInequality`, `kKkt`, `kObjective`) | **removed**, with the enum (owner ruling 2026-11-17); the shared ordering below is always used, on both engines | break; delete the assignment. There is no single-measure override: the old default's pick (equality residual alone, `<=` ties) is not reproducible. The dump loses the `best_iterate_criterion` line (IPM dump 67 -> 66 lines); only a solve that tracks a best (return-best, a callback or a token) is affected |

What each value does, on both engines:

- `kOnLimit` substitutes the best iterate on kIterationLimit, kWorkLimit and kTimeLimit exits. The
  SQP's old option read only the iteration cap; it now reads the work and time limits too. A
  `SolveBudget::max_minor_iterations` probe-budget stop is not one of them and keeps its ordinary
  exit.
- `kOnAnyNonOptimal` adds every other non-optimal exit (kStalled, kDiverging, kNumericalError,
  kAcceptable). The SQP gains it here: its non-finite iterate and subproblem-failure exits and its
  restoration exits now return the best iterate in full when one was recorded, as the interior-point
  engine does. The IPM gains `kOnLimit` (it had only the all-or-nothing bool).
- **A kInfeasible exit never substitutes**, certified or not, and neither does kInterrupted or
  kOptimal. A best point is not evidence about infeasibility.
- The interior-point engine's work and time limit exits under return-best used to claim
  `returned_best_iterate` and read the best iterate's residual row while returning the last
  primal point. They now return the best point in full (fixed with the widening).

"Best" is one ordering on both engines, always (no override): smaller
feasibility measure first (each engine's own convergence measure: the IPM's
`max(equality, inequality)`, the SQP's `feasibility`), every point within `common.feasibility_tol`
tied, then the smaller objective; a later point replaces the best only if strictly better. Before,
the IPM default compared the equality residual alone with `<=` (on a problem with no equalities it
picked the latest iterate) and the SQP compared (l1 violation, f) with no band.

Consumer catch-up: `docs/notes/data/2026-11-m8-ap5-grb/CATCH-UP.txt`.

## G-S -- the interior-point restoration-locally-infeasible exit is `kInfeasible`

A declared status event, one situation, one label across the engines. The SQP engine already
labelled "restoration ends at a point that is still infeasible" `kInfeasible`; the interior-point
engine reported it as `kStalled`.

| before | after | rewrite |
|---|---|---|
| IPM `status == kStalled`, `stop_reason == kRestorationLocallyInfeasible`, message `stalled: restoration reached a locally infeasible point` | `status == kInfeasible`, the same `stop_reason`, `infeasibility_certified == false`, message `locally infeasible (uncertified: restoration reached a locally infeasible point)` | a consumer that read kStalled here as "no progress" now reads kInfeasible: switch on `stop_reason` (and `infeasibility_certified`) to tell this exit from the SQP's certified one |
| the same exit's phase status (`phases[].status`, the `ipm.phase.exit` event's `status`, the console `exit` line) `stalled` | `infeasible` | text consumers: the status word of those three |

- `kStageStalled` (the stage stopped making progress) is still `kStalled`.
- Uncertified: the engine claims nothing about the model, only that restoration stopped at a point that
  is still infeasible. Like every kInfeasible exit it never substitutes the best iterate under
  `common.return_best` (the exit used to substitute under the old bool; G-RB's rule says no).
- The reporting order is unchanged (`kStalled` < `kInfeasible` < `kDiverging`): a phase sequence continues
  past this exit exactly as it did past the stall.
- Points are unchanged: every corpus column except `status` (and the status word inside `phases`) is
  bit-identical; two interior-corpus rows move, and the interior baseline is re-derived as
  `bench/baselines/2026-11-m8-ap5-gs-ipm-leg/interior_baseline.csv` (the G1 one stays frozen).

Consumer catch-up: `docs/notes/data/2026-11-m8-ap5-gs/CATCH-UP.txt`.

## TR-A -- trace v1 and the human trace

The machine trace is schema v1 (`docs/trace-schema-v1.md`) and the console is re-cut. Both engines write
the same four records.

| 0.1 | 0.2 |
|---|---|
| `TraceSink::on_sqp_major`, `on_ipm_iter` | `on_iter(const IterTraceEvent &)`; the row is `event.record`, an `IterationRecord` (core + `ipm` or `sqp` block) |
| `on_sqp_solve_begin`, `on_ipm_solve_begin` | `on_solve_begin(const SolveBeginTraceEvent &)` |
| `on_sqp_solve_end`, `on_ipm_solve_end` | `on_solve_end(const SolveEndTraceEvent &)`; written by the public handle after the result is finished |
| `JsonLinesTraceSink::reset_nesting()` | removed: the engines call the new non-emitting `TraceSink::on_solve_abort()` on every exceptional exit, so a sink needs no recovery call after a throwing solve; a custom `TraceSink` that tracks per-solve state overrides it (default: nothing) |
| `ipm.phase.exit` keys `kkt_inf`, `barr_inf` | `stationarity`, `complementarity` (engine scale) |
| `on_ipm_message` | `on_message(const MessageTraceEvent &)` |
| `on_ipm_restoration_exit_row` | removed; read `message{kind: restoration_exited, s: locally_infeasible}` in `on_message` |
| `IpmMessageKind::kX` | `MessageKind::kX`; `kRestorationLocallyInfeasible` -> `kRestorationExited` with `s == "locally_infeasible"`; `kInterruptAtIteration`/`kStopRequestedAtIteration` -> `kStopAtIteration` with `s == "callback"`/`"token"` |
| `ConsoleTraceSink::Format{wide, verbosity}` | `Format{verbosity, color, timing}` (the `legend` member was removed in TR-S1; the column reference is docs/trace-schema-v1.md section 8.1); delete the first initializer member |
| `IpmOptions::wide_console` | removed (one console width) |
| `format_iteration_table` and its five pieces | removed; attach a `ConsoleTraceSink` (or set `verbosity`) for live rows |
| `Verbosity` | gains `kDetail` |
| trace key renames | `docs/trace-schema-v1.md` section 7 |

### TR-B -- messages

No API change. The SQP now writes `message` records (restoration entered/exited/refused, subproblem failure,
elastic exhausted, inconsistent equalities, start point clamped, scaled convergence refused, stop at iteration),
and the IPM writes `restoration_entered` and every `restoration_exited` value (`resumed`, `failed`, `non_finite`,
`interrupted` as well as `locally_infeasible`). The IPM's non-finite evaluation abort now writes its `iter` row and
fires its callback event (M7-19): a reader that counted one fewer row than `iterations` on that exit now sees them
equal. Console output at `kWarnings` and above gains the warning lines; at `kIterations` the notices too.

## TR-D -- one iteration history for both engines

`IpmResult::history` is new and `SqpResult::history` changes its element type: both are
`std::vector<IterationRecord>` (`hven/solver/iteration_record.h`), the rows the trace writes, always built (no
sink needed; one copy per solve plus a handful of scalar stores per iteration). `SqpIterate` is retired (the driver
keeps an internal working row; it is not in the public headers). The SQP row gains `factorizations`. Identity:
`history.size()` == depth-0 `iter` lines == callback events (`docs/trace-schema-v1.md` section 4).

| 0.1 (`SqpIterate h`) | 0.2 (`IterationRecord h`; engine block `h.sqp`, `std::optional`) |
|---|---|
| `h.trial` | `h.iteration` |
| `h.f` | `h.f` (`std::optional<double>`: depth 0; `h.f_recovery` at depth 1) |
| `h.stationarity`, `h.feasibility`, `h.complementarity` | same names, core members |
| `h.kkt_residual` | removed: `std::max(h.stationarity, h.feasibility)` |
| `h.violation_l1` | `h.sqp->violation_l1` |
| `h.tr_radius` | `h.sqp->trust_radius` |
| `h.mu` | `h.sqp->regularization_dual` (`nullopt` when `!qp_solved`; `*` of it elsewhere) |
| (trace JSON, `sqp` block of `iter`) key `dual_regularization` | `regularization_dual`, the `ipm` block's key for the same quantity (a spelling inside the v1 freeze, not a v2) |
| `h.step_norm` | `h.step_norm` (`std::optional<double>`; `nullopt` when no step leaves the row) |
| `h.qp_solved` | `h.sqp->qp_solved` |
| `h.qp_status`, `h.qp_minor_iters`, `h.qp_factorizations` | `h.sqp->qp_status`, `qp_minor_iterations`, `qp_factorizations` (`std::optional`, `nullopt` when `!qp_solved`) |
| `h.verdict` | `h.sqp->verdict` (`std::optional`, `nullopt` when `!qp_solved`) |
| `h.tr_binding` | `h.sqp->tr_binding` |
| `h.soc_applied` | `h.soc` |
| `h.elastic_applied`, `h.elastic_rho0_ceiling_hit` | `h.sqp->elastic`, `h.sqp->elastic_clamped` |
| `h.restoration_seed_used` | `h.sqp->restoration_seed_used` |
| `h.watchdog_restored` | `h.watchdog` |
| `h.rolled_back` | `h.rolled_back` |
| `h.ipqp_least_infeasible_primal`, `h.ipqp_farkas_corroborated` | `h.sqp->ipqp_least_infeasible_primal`, `ipqp_farkas_corroborated` (`nullopt` on a row that is not an escape row) |
| `h.active_set_delta`, `weak_active_rows`, `near_active_rows`, `active_rows`, `active_lower_sides`, `active_upper_sides` | the same names under `h.sqp->` |
| (none) | `h.factorizations`: the (sub-)solve's running factorization count at the row; `h.sqp->mode`, `h.sqp->depth`, `h.tier`, `h.recovering`, `h.accepted` |
| `IpmResult` had no history | `IpmResult::history`, `h.ipm->` block (`mu`, `alpha_primal`, ...; `docs/trace-schema-v1.md` section 3) |

A field the row did not measure is `nullopt`, where 0.1 wrote a default (`mu`, `step_norm`, `qp_status`, the QP
counters and `verdict` on a row with no solved subproblem).

Consumer catch-up: `docs/notes/data/2026-11-m8-tr-d/CATCH-UP.txt`.

## TR-E -- `dump_counters` and the v1 reader fixtures

Two additions, no break.

`hven::dump_counters(const IpmResult &, std::ostream &)` and `dump_counters(const SqpResult &, std::ostream &)`
(declared beside the results, `hven/ipm/ipm_solver_types.h`, `sqp_solver_types.h`) write every counter the
result holds as `name = value` lines, the twin of `dump_options`. They are written from the X-macro tables
`solve.end.counters` is written from, so a key the trace carries the dump carries, with the same value. IPM:
the `IpmCounters` fields in declaration order, `kkt_analyses_total`, then the 13 fields of
`kkt_factor_counters` under that prefix. SQP: the direct `SqpCounters` fields, then `ssn.` and `ipqp.` blocks.
An integer prints as itself, a double at its shortest round-trip form, an enum by `to_string`, the IPM
histogram as `[a, b, c, d, e]`, and a counter its table declares absent prints `null`. Counters only: a sum over
a `Ledger` and the residuals are not here.

The v1 reader fixtures are three byte-exact streams under `tests/trace/golden/trace_v1/`
(`ipm_complete.jsonl`, `sqp_complete.jsonl`, `ipm_aborted.jsonl`), written from hand-filled records and held by
`TraceV1Fixtures.*`, for readers outside the tree. Regenerate: `HVEN_REGEN_GOLDENS=1 ./tests/trace/hven_trace_tests
--gtest_filter='TraceV1Fixtures.*'` (a declared event for every reader).

Consumer catch-up: `docs/notes/data/2026-11-m8-tr-e/CATCH-UP.txt`.

## The options registry and the generated options reference (M8 AP-4 G1, G2)

Additive; nothing is renamed or removed and no consumer has to change. Before the registry, the two
`dump_options` functions were hand-written, one line per field. Each options struct now has one table
(`include/hven/detail/<layer>/..._options_fields.h`), one row per field in declaration order, and the
dump, the new metadata and the generated reference are walks over those tables.

- `dump_options(const IpmOptions &, std::ostream &)` and `dump_options(const SqpOptions &, std::ostream &)`:
  same signatures, byte-identical output for `IpmOptions{}`, every `ipm_preset`, `SqpOptions{}` and an
  all-fields-perturbed value of each (fixtures captured from the old printers, `tests/solver/fixtures/options_dump/`).
- New public header `hven/solver/options_reference.h` (also reached through `hven/hven.h`): `OptionTier`
  (`kUser`, `kAdvanced`), `OptionInfo` (`name`, `type`, `default_value`, `range`, `doc`, `tier`),
  `ipm_option_info()`, `sqp_option_info()` (every field of the engine's options, in declaration order, names as
  dumped, defaults the value-initialized struct's) and `write_options_reference(std::ostream &)`.
- New checked-in `docs/options-reference.md`: the shared `common.` options with each engine's default, each engine's
  user and advanced options, and the enumerator legend. It is `write_options_reference()`'s output; a test fails when
  the two differ, and `cmake --build <dir> --target options-reference` regenerates it.
- Internal, not part of the API: `hven::detail::for_each_option(Options &, F)` walks a (const) `IpmOptions` or
  `SqpOptions` and calls `f(meta, value)` with typed access, for the bindings. There is no string-to-field setter.
- Adding an options field now means adding a row to its table: a field without a row, or a row out of declaration
  order, does not compile, and the field's range and one-line doc are stated where it is declared in the table.

Rebuild: nothing changes size or layout.

## Include paths (RS) -- the source and test restructure (M8 RS-1, RS-2)

The source tree was reorganised by engine: public headers that lived in the
`drivers/` catch-all now live with the component that owns them, the interior
engine's `interior/` and the `globalization/` directories became `ipm/` and
per-engine `globalization/` subdirectories, and the tests were re-laid to mirror
the components. Nothing changed but paths: no symbol, namespace, option or
behaviour moved (the proof is the P-SYM and counter-identity gates recorded
under `docs/notes/data/2026-11-m8-rs/gates/`). `docs/notes/data/2026-11-m8-rs/RS-move-map-v2-amended.txt`
is the complete file-by-file map; the commits are listed in `.git-blame-ignore-revs`.

### Public headers (`<hven/...>`): before, after

A consumer that includes `<hven/hven.h>` needs no change. Any other consumer
rewrites the include line:

| before | after |
|---|---|
| `<hven/drivers/common_options.h>` | `<hven/solver/common_options.h>` |
| `<hven/drivers/console_trace_sink.h>` | `<hven/trace/console_trace_sink.h>` |
| `<hven/drivers/ipm_enums.h>` | `<hven/ipm/ipm_enums.h>` |
| `<hven/drivers/ipm_iterate.h>` | `<hven/ipm/ipm_iterate.h>` |
| `<hven/drivers/ipm_options.h>` | `<hven/ipm/ipm_options.h>` |
| `<hven/drivers/ipm_phase_report.h>` | `<hven/ipm/ipm_phase_report.h>` |
| `<hven/drivers/ipm_solver.h>` | `<hven/ipm/ipm_solver.h>` |
| `<hven/drivers/ipm_solver_types.h>` | `<hven/ipm/ipm_solver_types.h>` |
| `<hven/drivers/iteration_record.h>` | `<hven/solver/iteration_record.h>` |
| `<hven/drivers/json_lines_trace_sink.h>` | `<hven/trace/json_lines_trace_sink.h>` |
| `<hven/drivers/solve_result.h>` | `<hven/solver/solve_result.h>` |
| `<hven/drivers/sqp_options.h>` | `<hven/sqp/sqp_options.h>` |
| `<hven/drivers/sqp_solver.h>` | `<hven/sqp/sqp_solver.h>` |
| `<hven/drivers/sqp_solver_types.h>` | `<hven/sqp/sqp_solver_types.h>` |
| `<hven/drivers/step_verdict.h>` | `<hven/solver/step_verdict.h>` |
| `<hven/drivers/trace.h>` | `<hven/trace/trace.h>` |

Rewrite: a `sed` over the consumer's tree is enough, because every row is a
one-to-one file move with an unchanged basename:

```
sed -i -E \
  -e 's#hven/drivers/common_options.h#hven/solver/common_options.h#' \
  -e 's#hven/drivers/console_trace_sink.h#hven/trace/console_trace_sink.h#' \
  -e 's#hven/drivers/ipm_enums.h#hven/ipm/ipm_enums.h#' \
  -e 's#hven/drivers/ipm_iterate.h#hven/ipm/ipm_iterate.h#' \
  -e 's#hven/drivers/ipm_options.h#hven/ipm/ipm_options.h#' \
  -e 's#hven/drivers/ipm_phase_report.h#hven/ipm/ipm_phase_report.h#' \
  -e 's#hven/drivers/ipm_solver.h#hven/ipm/ipm_solver.h#' \
  -e 's#hven/drivers/ipm_solver_types.h#hven/ipm/ipm_solver_types.h#' \
  -e 's#hven/drivers/iteration_record.h#hven/solver/iteration_record.h#' \
  -e 's#hven/drivers/json_lines_trace_sink.h#hven/trace/json_lines_trace_sink.h#' \
  -e 's#hven/drivers/solve_result.h#hven/solver/solve_result.h#' \
  -e 's#hven/drivers/sqp_options.h#hven/sqp/sqp_options.h#' \
  -e 's#hven/drivers/sqp_solver.h#hven/sqp/sqp_solver.h#' \
  -e 's#hven/drivers/sqp_solver_types.h#hven/sqp/sqp_solver_types.h#' \
  -e 's#hven/drivers/step_verdict.h#hven/solver/step_verdict.h#' \
  -e 's#hven/drivers/trace.h#hven/trace/trace.h#' \
  $(git ls-files '*.cpp' '*.h' '*.hpp')
```

The seven `.gitkeep` placeholders under `include/hven/` were deleted, and the directories
`drivers/`, `globalization/` and `interior/` no longer exist (an installed tree has no
`include/hven/drivers/`). The `hvenConfig.cmake` sentinel is now `hven/hven.h`.

### Internal headers (`<hven/detail/...>`): directory-level map

`detail/` is installed but carries no compatibility promise; the full map is
the move map above. By directory (file counts in parentheses):

| before | after |
|---|---|
| `core/detail/` | `detail/core/` (3) |
| `detail/drivers/` | `detail/sqp/` (8) |
| `detail/drivers/` | `detail/solver/` (6) |
| `detail/drivers/` | `detail/core/` (1) |
| `detail/drivers/` | `detail/ipm/` (5) |
| `detail/globalization/` | `detail/ipm/globalization/` (18) |
| `detail/globalization/` | `detail/ipm/` (4) |
| `detail/globalization/` | `detail/kkt/` (1) |
| `detail/globalization/sqp/` | `detail/sqp/globalization/` (5) |
| `detail/globalization/sqp/` | `detail/sqp/quasi_newton/` (4) |
| `detail/interior/` | `detail/ipm/` (3) |
| `detail/interior/` | `detail/barrier/` (2) |
| `detail/interior/` | `detail/model/` (1) |
| `detail/interior/` | `detail/kkt/` (1) |
| `detail/interior/typedefs/` | `detail/core/` (2) |
| `detail/interior/utils/` | `detail/ipm/` (1) |
| `detail/interior/utils/` | `detail/util/` (5) |
| `detail/kkt/` | `detail/qp/walk/` (1) |
| `detail/kkt/` | `detail/kkt/` (1) |
| `detail/qp/` | `detail/qp/walk/` (7) |
| `detail/qp/` | `detail/qp/ipqp/` (4) |
| `detail/qp/` | `detail/qp/ssn/` (1) |
| `detail/warmstart/` | `detail/sqp/` (2) |
| `detail/warmstart/` | `detail/warmstart/` (1) |

### Renamed headers (the basename changed too)

| before | after |
|---|---|
| `detail/globalization/sqp/globalization.h` | `detail/sqp/globalization/funnel.h` |
| `detail/kkt/kkt_calls.h` | `detail/kkt/kkt_factor.h` |
| `detail/qp/qp_anti_cycling_arm.h` | `detail/qp/walk/anti_cycling_arm.h` |
| `detail/qp/qp_certificate_seam.h` | `detail/qp/walk/certificate_seam.h` |
| `detail/qp/qp_degeneracy_arm.h` | `detail/qp/walk/degeneracy_arm.h` |
| `detail/qp/qp_engine.h` | `detail/qp/walk/walk_engine.h` |
| `detail/qp/qp_walk_trace.h` | `detail/qp/walk/walk_trace.h` |
| `detail/warmstart/warm_start.h` | `detail/warmstart/crossover.h` |

`detail/ipm/globalization/` and `detail/sqp/globalization/` both carry a `soc.h`
and a `restoration.h`: always spell the full path.

### Removed headers

Deleted, with no replacement (nothing in the tree included them):
`detail/interior/utils/{flat_map,function_return_type,math_functions,sizing_helpers,std_extensions}.h`,
and the private `src/interior/utils/{fmtlib,color_text}.{h,cpp}` (`enable_color_console`
had no caller). The private include directory `src/interior/utils` is gone with them.

### Tests and sources

`src/{drivers,interior,globalization}/` became `src/{ipm,sqp,solver,trace,kkt,util,warmstart}/`;
`tests/{interior,drivers,common_support}/` became `tests/{ipm,solver,support}/`, and
the fixtures, goldens (`tests/trace/golden/`) and compile-fail probes
(`tests/model/compile_fail/`) moved with their components. Executable and target
names did not change in RS-1/RS-2; they did in RS-6a, which gave every `tests/`
directory its own executable: `hven_{core,linear,model,kkt,qp,warmstart,solver,trace,ipm,sqp,bench}_tests`,
plus the seam executables `hven_{linear,kkt,qp,solver,ipm,sqp}_seam_tests` and
`hven_linear_alloc_tests` (see docs/testing.md, "The test executables since RS-6a").
`hven_tests`, `hven_interior_tests`, `hven_drivers_tests`, `hven_fault_injection_tests`,
`hven_ipqp_seam_tests`, `hven_qp_walk_seam_tests` and `hven_dense_alloc_tests` no
longer exist; a documented command line such as `./hven_drivers_tests --gtest_filter=...`
now names the executable of the component that owns the test
(`./tests/solver/hven_solver_tests --gtest_filter=...`). ctest names (`Suite.Test`) did
not change, so `ctest -R` and `ctest -N` are unaffected.

### Stale path literals deliberately left in place

A string literal that can reach an artifact (a CSV provenance row, bench output,
golden bytes) is never rewritten by a prose pass, because rewriting it would
change what the artifact says it ran. These still carry the pre-RS paths:

| where | literal | what it is |
|---|---|---|
| `bench/bench_corpus.cpp:2333` | `# cells: tests/sqp/support/hs_problems.h hs_numbers()` | provenance row written into every corpus CSV |
| `bench/bench_corpus.cpp:2133` | `tests/sqp/support/hs_problems.h` | a provenance argument (now `tests/model/support/hs_problems.h`) |
| `bench/bench_corpus.cpp:326`, `:392`, `:394` | `tests/sqp/support/hs_problems.h`, `src/drivers/sqp_solver.cpp:4562`, `include/hven/drivers/solve_result.h:228` | `--help`/usage text of the corpus runner |
| `bench/bench_scale.cpp:280` | `tests/sqp/support/{parametric_families,` ... | `--help` text of the scale bench (now `tests/model/support/`) |
| `bench/baselines/**/*.csv` provenance headers | origin and pre-RS paths | frozen evidence; CLAUDE.md section 1 |
| `tests/core/test_core_layering.cpp` (the `EXPECT_TRUE(violations.empty())` message), `tests/solver/test_options_dump.cpp` (two drift-pin messages), `tests/sqp/test_disproved_entry.cpp` (one failure message), `tests/qp/test_ipqp_acceptance.cpp` (`kA2Usage`) | tier names and `src/drivers/...` / `tests/sqp/...` | diagnostic and usage text compiled into test objects: left so a comments-only stage changes no test object |
| `scripts/rs_object_map_selftest.sh` | `src/drivers/...`, `tests/interior/...` | the self-test's fixture map (pre-RS names are its input) |
| `src/CMakeLists.txt` | `drivers/...`, `interior/...` in the measured PCH table | a recorded measurement, "left as it ran" |

### For hven-bench

hven-bench (checked read-only at its HEAD 6d1a8a0) needs the following to build
against an RS head; none is a behaviour change:

- **Must re-point (live code):** `lanes/external/declared_point.h:87`,
  `detail/drivers/declared_diagnostics.h` becomes `detail/solver/declared_diagnostics.h`.
- Archived probe, only if rebuilt: `runs/2026-09-m7-w2-c1-sqp-cost-census/probe/c1_probe.cpp:114-116`
  (`drivers/solve_result.h`, `drivers/sqp_solver.h`, `drivers/sqp_solver_types.h`, per the table above).
- Historical retake chains name hven paths in diffs (`scripts/w2_retake{9,12,13}_prechain.sh`,
  `w2_retake9_compare.py`): they break only if re-run against a new head.
- Prose and provenance strings (`schema/v1.md`, `schema/fixtures/trace_v1/PROVENANCE.txt`, `sets/*.json`,
  `sets/hs/*.cpp`, `cmake/hven_pin.cmake` comments) name stale paths with no behaviour effect;
  `tests/drivers/golden/trace_v1/` is now `tests/trace/golden/trace_v1/`, with the bytes unchanged
  (the recorded sha256 copies still match).
- A hven pin bump is needed to consume the RS head.

## GL-0 -- globalization diagnostics

Additions only; no behaviour change. The SQP reports where globalization spends work without progress, so a reader of a
solve (or of the tuning pilot's rows) can tell *why* a step was refused. All of it is always on: each counter is one
increment at a site that already incremented `rejected_steps` or `steps_accepted`, and the iteration row's new field is
one stored double in a row that is built regardless of a sink.

`SqpCounters` gains eight fields, appended after `payload_ignored` (so `solve.end.counters` and `dump_counters` gain eight
trailing keys, the last an array):

| field | counts |
|---|---|
| `reject_funnel_ceiling` | rejected trials with `h_new` above the funnel width (KLV Eq. (8)) |
| `reject_f_armijo` | rejected f-type trials that failed the Armijo test (Eq. (11)) |
| `reject_h_funnel` | rejected h-type trials inside the funnel that failed the funnel sufficient decrease (Eq. (12)) |
| `reject_qp_failed` | majors whose QP subproblem failed and was routed to a radius shrink (no judged trial, or the failed solve's own step was offered under `qp_failure_step` and refused) |
| `reject_other` | a non-finite trial, or a rejection by a caller's own `make_strategy` strategy that reports no cause |
| `accepted_f_type` | accepted steps with verdict `kAcceptObjectiveStep` (this includes the zero-step accept and any full-step-mode accept, which no Eq. (11) test judged) |
| `accepted_h_type` | accepted steps with verdict `kAcceptFeasibilityStep` |
| `rejection_run_histogram` | `std::array<Index, 5>`: each rejection placed by its position in the run at one iterate; entry `k-1` counts rejections that were the `k`-th consecutive, the last entry is 5 or more |

Identities, on every solve: `rejected_steps == reject_funnel_ceiling + reject_f_armijo + reject_h_funnel +
reject_qp_failed + reject_other`; `steps_accepted == accepted_f_type + accepted_h_type`; `sum(rejection_run_histogram) ==
rejected_steps`. Entry 0 of the histogram is the number of rejection runs and entry `k` the number that lasted past `k`.
The cause is the original trial's: a trial whose second-order correction is also refused is one rejection, counted by the
first judgement's cause. A trial answered `kRestore` is not a rejection and is in none of these. Like `rejected_steps`
and `steps_accepted`, none is folded from a restoration sub-solve. Second-order-correction attempts and rescues are the
existing `soc_steps` and `soc_applied` (`soc_steps == soc_applied + soc_qp_infeasible + soc_rejected`).

The switching ratio is the iteration row's: `SqpIterationRow::switching_ratio` (`std::optional<double>`), the `sqp` block's
trailing key `switching_ratio` in the trace. It is `pred_df / (0.999 * h^2)` at the row's point (KLV Eq. (10)); `>= 1` selects
an f-type trial. `nullopt`/`null` on a row whose subproblem did not solve to `kOptimal`; `+/-inf` or NaN at `h == 0`. The
v1 freeze table admits a new trailing key in a block (`docs/trace-schema-v1.md` section 6), so it is a row field and not a
per-solve summary.

Custom strategies: `detail::GlobalizationStrategy::last_reject_cause()` is a new virtual with a default (`kUnclassified`);
a decorator around `FunnelStrategy` should forward it, or its rejections count as `reject_other`.

Rebuild: the new virtual (appended last in `GlobalizationStrategy`'s vtable) and the enlarged `SqpCounters` and result
aggregates change object layouts. Custom globalization strategies and any code that includes these headers require a full
rebuild against this version; mixed objects built against the old and new headers are not supported.

Consumer catch-up: `docs/notes/data/2026-11-m8-gl0/CATCH-UP.txt`.

## The platform-solve response of the SQP's sparse KKT factors (M8 sparse-equil-v4)

Numerics only where a factorization fails; no interface change beyond three counters. The SQP factors its KKT matrices on
the platform session without weighted matching or scaling (`sqp_kkt_options`). MKL Pardiso replaces a pivot it cannot
take by a static perturbation and leans on refinement; two failure classes were measured -- a badly scaled K, and a
saddle-point K whose pivots must pair into 2x2 blocks -- and the walk now answers both.

- The trigger: in `solve_eqp` and `solve_bordered_eqp`, on platform numerics that reported perturbed pivots (on
  Accelerate, which reports no count: every platform factorization), the answer refined against K is measured by its
  normwise backward error in the Ruiz-equilibrated metric. Above `detail::kPlatformSolveFailureTol` = 1e-5 the
  factorization has failed.
- The response, per factorization (never sticky): the same K is retaken as D K D on a plain session, D the
  maximum-product-matching symmetric scaling (`hven/detail/linear/matching_scaling.h`, Duff & Koster 2001, Duff & Pralet
  2005), and the EQP is solved again. If those numerics still perturb pivots and the answer fails again -- on the backward
  error or on the EQP-site residual check, which sees the near-null blow-up the backward error is blind to -- K is
  retaken on a weighted-matching session (Pardiso only). The next factorization is the platform's own again.
- Where it runs (r2): the eliminated path retakes its own factorization of the same K in place and solves again (at most
  three solve rounds); a Schur stack's K0 cannot (its C was formed through K0), so a failed bordered answer requests the
  retake for the NEXT K0 factorization, a request that never outlives the QP solve. A retake that fails or throws leaves the
  platform numerics, check disarmed. Retake sessions are freed at the end of every walk solve except the one live numerics
  use.
- Unaffected: unperturbed factorizations (bit-identical), the dense arm (its own check), the SSN and IPQP tiers' factors.
- New counters `sparse_scaled_retakes`, `sparse_matched_retakes`, `sparse_retake_refusals` (`QpCounters`, `SqpCounters`,
  trace keys after `rejection_run_histogram`). The first two count the retake factorizations run, none of which is in
  `factorizations` (the active-set search's accounting); their analyses are in no counter (`solver_counters.h` says when
  one is paid). `eqp_refine_iters` counts every solve round's residual-check solves.
- IPARM-SURFACE: the SQP now configures `iparm[12] = 1` (without `iparm[10]`) on the on-demand matched session.
  Evidence: `docs/notes/data/2026-11-m8-sparse-equil-v4/`.

Rebuild: `QpCounters` and `SqpCounters` change size; code built against the old headers needs a rebuild.

## `qp/qp_options.h` -- the QP-layer option types (M8 RS-R1)

Additive. `IpqpOptions`, `IpqpBarrierInitialRule`, `QpMode`, `SsnSigmaRule`, `SsnHintRule` and
`SsnInfeasibilityRule` moved verbatim from `hven/sqp/sqp_options.h` to the new public header
`hven/qp/qp_options.h`, and `sqp_options.h` includes it, so every existing consumer compiles
unchanged and `hven.h` still reaches all of them. The QP engines (`detail/qp/ipqp`, `detail/qp/ssn`)
now include `qp_options.h` instead of the SQP types, and `validate(const IpqpOptions &)` is declared
there: it is the code `validate(const SqpOptions &)` runs for `advanced.ipqp`, at the same position,
with the same messages. No name, enumerator value or field changed.

## The analysis tally and the batched Schur border solve (M8 PF-4)

Additive; no result moves (`docs/notes/data/2026-11-m8-pf4/`).

- `hven::linear::AnalysisTally` (`hven/linear/symmetric_factor.h`): an RAII scope counting, on the calling thread, every
  symbolic analysis the platform (sparse) backend completes on any `SymmetricFactor` while it is open, and the repeats
  among them (an earlier analysis in the same tally carried the same key: pattern hash, order, stored entries, the options
  that reach the symbolic phase). Observation only.
- New counters `SqpCounters::backend_symbolic_analyses` and `backend_symbolic_repeats` (trace keys after
  `sparse_retake_refusals`): the tally over one SQP solve. `symbolic_analyses` keeps its meaning -- the border/rebuild
  path's analyses and the IPQP's -- and reads far lower on a solve that uses the elimination path, whose short-lived EQP
  factors each pay an analysis the new counter sees.
- `detail::SchurComplement`'s batch (`begin_batch`/`end_batch`, the walk's `sync_borders`) lands its borders with ONE
  multi-right-hand-side K0 solve (`detail::solve_mat`) instead of one solve per border.

Rebuild: `SqpCounters` changes size; code built against the old headers needs a rebuild.
