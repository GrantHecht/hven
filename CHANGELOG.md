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
