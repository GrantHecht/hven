# Changelog

## 0.2.0 — the M8 API break

One declared break window (groups B1-B8). There is no migration guide: the rewritten `examples/`
programs are the reference for the new surface, and the full rename table is
`docs/notes/2026-11-m8-ap2-b8-naming.md`.

- **B1 — header hygiene.** Before: `IpmSolver`/`SqpSolver` declared their internals in public
  headers, which included `detail/`, `fmt` and test-fixture names. After: both engines sit behind
  a private implementation, are movable, and no public header includes `detail/`.
- **B2 — one problem.** Before: two public callback contracts (`NlpTripletModel`, `NlpModel`) and a
  public program struct, one per engine. After: one callback contract, `hven::Nlp`, and one
  problem type, `hven::Problem`, that both engines solve; bounds are re-read at every solve.
- **B3 — options and limits.** Before: tolerances, iteration caps and verbosity spelled per engine.
  After: `CommonOptions` (`optimality_tol`, `feasibility_tol`, `max_iterations`,
  `max_factorizations`, `time_limit_seconds`, `verbosity`, `infinite_bound`) on both engines, the
  `kWorkLimit` and `kTimeLimit` statuses, `IpmOptions::linear`/`termination`/`advanced`,
  `SqpOptions::advanced`, and `SqpSolver(SqpOptions = {})`.
- **B4 — results, warm start, callbacks, trace.** Before: per-engine result shapes, three ways to
  get an SQP warm start, a trace sink with pure virtuals. After: `convergence` sub-structs,
  `message` and `work` on every result, one `result.warm_start` value (`WarmStart`), an opaque
  `SqpWarmStart`, `to_sqp_warm_start()`, `CallbackScope`, and a `TraceSink` whose events all have
  no-op defaults.
- **B5 — evaluation failure.** Before: the IPM absorbed any exception at a trial point; the SQP
  propagated all of them. After: `hven::Error`, `EvaluationError`, `BackendError`; an
  `EvaluationError` or a non-finite value at a trial point is a rejected trial on both engines,
  any other exception propagates, and `last_result()` keeps the interrupted solve's result.
- **B6 — pieces.** Before: building a problem from pieces needed `detail/` includes and adapter
  specializations. After: the local-block layer and the `AssemblyDeclaration` builder; the raw
  piece protocol is public and specified (`docs/assembly-protocol.md`).
- **B7 — absent bounds.** Before: the IPM treated every finite bound as real while the SQP's QP
  tier treated `|b| >= 1e20` as absent. After: `CommonOptions::infinite_bound` (default 1e20)
  decides absence once, at the `Problem` boundary, for both engines.
- **B8 — names.** Before: `hven::solvers`, acronym-cased types (`NLPRowClassification`,
  `ParsedIOFlags`), bare enumerators (`ThreadingFlags::MainThread`), aggregate members with
  trailing underscores (`VariableBound::index_`), abbreviated option names (`QpOptions::feas_tol`,
  `SqpOptions::tr_init`). After: one namespace `hven` (internals in `hven::detail`), the umbrella
  `<hven/hven.h>`, words for acronyms, `kPascalCase` enumerators, bare public data members, option
  names on one rule, and the raw-protocol headers named for what they declare.
- **B8 riders — the names freeze.** Before: bare interior-point option enums beside `Sqp`-prefixed
  ones (`AcceptanceMode`, `KktOrdering`), `kMaxIter` and `kIterationCap` for one fact, abbreviated
  `advanced` tiers (`rho_init`, `enable_soc`, `qn_*`) and result records (`IpmTiming::pre`,
  `SqpScalingReport::col_spread`), internals declared in `hven`, and a public struct whose layout
  depended on `USE_ACCELERATE_SPARSE`. After: every option enum carries its component's prefix
  (`IpmAcceptanceMode`, `IpmOrdering`, `SqpElasticRungEngine`), one `kIterationLimit`, the `advanced`
  tiers and result records in words (`primal_regularization_initial`, `second_order_correction`,
  `kkt_seconds`, `column_spread`), the IPM's linear options in the factor layer's vocabulary,
  `linear::Decomposition` (was `FactorKind`, without the refused `kLlt`/`kLu`), every internal in
  `hven::detail`, `warmstart/warm_start.h` and `drivers/json_lines_trace_sink.h`, and
  `IpmLinearOptions::accelerate_zero_tol` on every platform. Trace keys and strings are unchanged.

## Unreleased — M8 TR-S1 (the trace prints what the tolerances test)

- **Scaled measures in the trace; no legend.** Before: the console `stat feas comp` columns and the
  JSON `iter` keys `stationarity`/`feasibility`/`complementarity` printed the measures mapped to the
  caller's units (IPM: divided by the objective scale; SQP: row/column scaling removed), so under
  scaling the printout hid why a solve continued (a printed stationarity of 1.4e-11 against an
  `optimality_tol` of 1e-6, with the objective scaled up 7e4 times). After: those columns and keys are
  the scaled problem's values, exactly what the convergence test compares with the tolerances; the
  headers read `stat_scl feas_scl comp_scl`; the caller-unit values are the new trailing `iter` keys
  `stationarity_caller`, `feasibility_caller`, `complementarity_caller` and the unchanged
  `IterationRecord::{stationarity, feasibility, complementarity}` (`history`, callback).
  `IterationRecord` gains `tested_stationarity`, `tested_feasibility`, `tested_complementarity`. The
  printed `legend` block and `ConsoleTraceSink::Format::legend` are removed; the column reference is
  `docs/trace-schema-v1.md` section 8.1. Unscaled solves print the same numbers as before.

## Unreleased — M8 I18 (issue 18: a feasible walk QP ended kNumericalError)

- **The walk's EQP accuracy stop.** Before: the EQP answer was accepted once its residual fell
  below the regularization's footprint, which on a narrow box (an SQP trust region of 3.8e-6) let an
  answer off by three radii through, and the walk ended `kNumericalError` on a feasible QP. After: on
  the independent working set's walk, when the first refinement moves a free variable farther than
  the trust-region radius, refinement continues until the last correction is at most half the step,
  keeping only steps that contract tenfold. Both working-set algebras.
- The test loader (`tests/qp/support/qp_dump.h`) sets `hessian_known_convex` from a dump's `hkc=`.

## Unreleased — M8 I17 (issue 17: weak columns under `column_scaling = kAlways`)

- **Ladder diagnostics.** Before: an exhausted elastic ladder printed only its rung count, and the
  `sqp.major` row's `qp_status` was overwritten by the last rung's. After: the row keeps the main
  QP's status and carries a `ladder` block (also on a usable ladder, the certified fallback's
  included); the console line and the `elastic_exhausted` message (its `ladder` key) state the
  entry route, both statuses, the final penalty and `closed`/`reduced`/`promises_f`/`slack_l1`;
  restoration QPs are recorded in an attached `Ledger`
  (`<prefix>_resto<entry>_<run>_<lifetime run>_qp_<k>`).
- **`SqpOptions::caller_step_limit`** (default 0, off): `|dx_j| <= r (1 + |x_j|)` in caller units,
  folded into every QP box, restoration's x-block included; a variable stopped by it is reported
  free and the step trust-region-limited.
- **`SqpOptions::column_scaling_factor_max`** (default 1024): the clamp on the `kAlways` column
  factors; `scaling_factor_max` keeps the rows and objective, and `kAuto`'s columns.
- **`SqpAdvancedOptions::qp_dump_dir` / `qp_dump_policy`** (`QpDumpPolicy`): the walk's QPs as
  "D4QP 1" files (`docs/qp-dump-format.md`).
- These options are SQP-only: the interior-point solver has no trust region, column layer or walk QP.

## Unreleased — the M8 AP-5 interface groups

Migration lines for these groups: `docs/migration-0.2.md`.

- **G1 — the stop token and one stop reason.** Before: a solve could be stopped only from
  its iteration callback; the engines reported why they stopped through two enums
  (`IpmStopReason`, `SqpStopReason`) read off the solver (`last_stop_reason()`), and the SQP
  honoured a callback stop one step late. After: `StopSource`/`StopToken`
  (`core/stop.h`) and `SolveBudget::stop` stop a solve from any thread at its next
  iteration boundary; one `StopReason` on the result (`SolveResult::stop_reason`,
  `IpmPhaseReport::stop_reason`) with `kCallbackStop` and `kStopRequested`, `kNone`
  whenever the status names the cause; the SQP honours a callback stop at the row it was
  returned on. The trace's `stop_reason` strings follow (declared). A token stop prints its
  own console lines ("Solve stopped by a stop request (SolveBudget::stop) ...") from two new
  `ipm.message` kinds, `stop_requested_at_iteration` and `stop_requested_skipping_phases`.
