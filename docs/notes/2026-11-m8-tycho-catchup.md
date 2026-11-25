# M8 tycho catch-up: API and behaviour changes on hven m8

A catch-up list, not a gate: nothing waits on tycho, and tycho consumes the whole break window on resume.
Draft, written against origin/m8 at afb65b804 (2026-11-24).

**Baseline.** No tycho consume of an M8 head is recorded in this repository, so the baseline is the M7 close
(main at the M7 merge). Everything below is on m8 and not on that baseline. Per-group detail lives in
`docs/migration-0.2.md` (before / after / rewrite tables) and in the per-group `TYCHO-CATCH-UP.txt` files named
below, which are the authoritative item lists and are not repeated in full here. Nothing in this note was
checked against tycho's tree.

## 1. Most recent changes (read these first)

### 1.1 SQP trace and callback: `tr_binding` split, I19-1 telemetry

Source of truth: `docs/trace-schema-v1.md` (binding-flags paragraph and the change-log row "M8 I19-1");
`include/hven/solver/iteration_record.h`.

- `SqpIterationInfo::tr_binding` (and the trace `sqp` key `tr_binding`) now means the trust RADIUS held the step.
  A `caller_step_limit` pin no longer counts. The old merged value is `tr_binding || cap_binding`.
  Decisions are unchanged (the engine still reads the merged flag internally); only the exported meaning moved.
  A tycho consumer that read `tr_binding` as "something limited the step" must read the OR of the two.
  The console code `b` keeps the merged meaning.
- New `cap_binding`: a `caller_step_limit` pin limited the step.
- The 11 new trailing keys of the trace `sqp` block (arity 25 -> 36), in order: `cap_binding`, `pred_df`,
  `f_trial`, `h_trial`, `soc_f`, `soc_h`, `h_lin`, `qp_minor_iterations_total`, `qp_factorizations_total`,
  `factorizations_delta`, `t_s`. Report-only: no decision reads any of them. `t_s` is wall-clock (mask it in any
  pinned comparison). All are also fields on `SqpIterationInfo`. `pred_df`/`f_trial`/`h_trial` are optional,
  present when the row's QP solved to optimal; `soc_f`/`soc_h` only when a SOC re-solve solved; `h_lin` is null for
  an equality-basis reduction or a non-finite step.
- The `ladder` object gains `rho_0`, `minor_iterations`, `factorizations` (also in the `message` record).
- Consequence for tycho: any golden pin or strict-arity trace reader of the SQP `iter` or `message` records moves.
  The v1 freeze is untagged, so this is a pre-tag correction, not a v2.

### 1.2 RF-1: the SQP's failure exits are labelled honestly; RF-1c stall window

Source: `docs/migration-0.2.md` section "RF-1", `docs/notes/2026-11-m8-rf1-design.md`, `docs/options-reference.md`.

- SQP exits that ended `kNumericalError` beside `kSubproblemFailure`, `kRestorationRefused`, `kRestorationFailed`
  or `kRestorationNoClaim` at a point inside the acceptable tier now report `kAcceptable` (message
  `converged to an acceptable level: ...: exit <reason>`, `stop_reason == kNone`). The tier is the new
  `SqpOptions::termination` (the IPM's four `acceptable_*_tol` names and defaults). A last QP that stopped at its
  minor cap now ends `kStalled` with the new `StopReason::kSubproblemIterationLimit`. Points, iterations and
  counters are unchanged (reporting only). A relabelled exit carries no `best_iterate`.
- RF-1c semantics of `advanced.restoration_stall_factorizations` (default 5000, >= 0): the phase ends when its best
  parent violation has not fallen by 10% over that many factorizations AND the reference sample lies at least five
  passes back, so a stall verdict rests on at least six passes of the current run. A window of 0 now means "after
  five passes" (before RF-1c the window counted across the phase's runs, and one pass costing more than the window could fire
  it). The window is per run: a regularized restart starts a fresh window but keeps the best. A stalled phase ends
  at its best-h point; a stall the resume tests refuse ends the solve `kStalled` with
  `StopReason::kRestorationWorkLimit`. `common.max_factorizations` stays the hard cap on the whole solve.
  Effect measured in-repo: BRITGAS and NET3 default returned to optimal. Any tycho SQP solve that set a small
  window should re-check its restoration behaviour.

### 1.3 Other late m8 items

- PF-HB (`docs/notes/data/2026-11-m8-pf-huge-bound/TYCHO-CATCH-UP.txt`): no public API change. Interior-point
  golden pins can move if a row slack reaches 2^26 (the far-pair multiplier recovery); below that every step is
  bit-identical.
- GL-0 globalization diagnostics (`switching_ratio` trace key, eight trailing `solve.end.counters` keys); PF-4 / PF-5
  (analysis tally, batched Schur border solve, symbolic-analysis reuse); the platform-solve response of the SQP's
  sparse KKT factors; `qp/qp_options.h` (QP-layer option types). Each has a section in `docs/migration-0.2.md`.
- Trace schema events in order (see the change-log table in `docs/trace-schema-v1.md`): TR-S1 (the core
  `stationarity`/`feasibility`/`complementarity` now carry the scaled, tested values; the caller-unit values moved to
  new trailing `*_caller` keys), TR-D (`dual_regularization` spelled `regularization_dual`), I17 (`ladder`), GL-0.

## 2. The API break window (AP-2 B5-B8, AP-5 G1-G7, AP-4)

### 2.1 AP-5 groups G1-G7 and follow-ons (`docs/migration-0.2.md`, one section each)

G1 stop token and one `StopReason` (`StopSource`/`StopToken`, `SolveBudget::stop`; `IpmStopReason`/`SqpStopReason`
merged into `StopReason`; `last_stop_reason()` removed; an SQP callback `kStop` is honoured right after row k);
G2 recovery-aware stop; G3 best-so-far, `kStopReturnBest`, `snapshot()`; G4 stops inside an iteration; the zero-step
short-circuit; the walk kOptimal row contract; G5 the widened callback payload; G6 `IpmCounters` and the regrouped
`IpmResult`; G7 the remaining symmetry and one stop rule in restoration; AP-4 friction fixes; G-RB one
`return_best`; G-S the IPM restoration-locally-infeasible exit is `kInfeasible`; TR-A/TR-D/TR-E (trace v1, one
iteration history, `dump_counters`); the options registry and generated `docs/options-reference.md`.

### 2.2 AP-2 groups B5-B8 (`docs/notes/data/2026-11-m8-ap2-b{5,6,7,8}*/TYCHO-CATCH-UP.txt`, items T1-T31)

- B5 (T1-T8): "cannot evaluate here" is `EvaluationError` (or a non-finite return). A model that throws a standard
  exception for it was absorbed as a rejected trial by the IPM before and now PROPAGATES out of `IpmSolver::solve`:
  audit every throw reachable from a model callback. Non-finite values are failed trials on both engines; the IPM's
  "line search failed ... could not be evaluated" `std::runtime_error` is now `kNumericalError` with
  `kEvaluationFailure`; foreign exceptions propagate unchanged and `last_result()` keeps the interrupted solve;
  thread-pool dispatch rethrows one exception, not a composite message; backend failures are `BackendError`.
- B6 (T1-T5 plus riders R-T1..R-T5): piece-protocol headers left `detail/`; objective handle forwards the objective
  surface only; `SolverInterfaceAdapter`'s primary template is the default; `AssemblyDeclaration` builder;
  local-block layer; shared Jacobian entries are serialised by the layout (`unique_constraints`: a raw scatter must
  lock when it is false); pieces must be copy-constructible; `print_data` removed (tycho's twelve call sites move
  into tycho); the local-block payload member is `piece`.
- B7 (T1-T13): one infinite-bound rule on both engines (`common.infinite_bound`, default 1e20: a model writing "no
  bound" as +/-1e20 now gets no barrier term on the IPM, so iteration counts move); empty bound pairs refused by
  name; removed `kEngineInfBound`/`kIpqpInfBound`/`kSsnInfBound`; `absent_bounds` trailing key on both
  `solve.begin` lines; `NlpModel::source_infinite_bound()`; `evaluate_kkt`/`build_subproblem`/`predict` take the
  threshold.
- B8 and riders (T14-T31): the namespace is `hven::` (`hven::solvers` gone); one include `<hven/hven.h>`; public data
  members lose the trailing underscore; enumerators are kPascalCase; the IPM iterate record is `IpmIterate`; a large
  option/field/enumerator rename sweep (T19, T22-T27) with the full old-to-new table in
  `docs/notes/2026-11-m8-ap2-b8-naming.md` section 3; `IpmStopReason::kIterationCap`/`kMaxIter` family become
  `kIterationLimit` (strings unchanged); internals live in `hven::detail`; build macro
  `HVEN_DEFAULT_QP_THREADS` -> `HVEN_DEFAULT_IPM_THREADS`; version 0.2.0.

### 2.3 Include paths and layout

The RS restructure moved headers (`drivers/` split into `solver/`, `sqp/`, etc.); the old-to-new map is the
"Include paths (RS)" section of `docs/migration-0.2.md`. No forwarding headers exist. Installed consumers must still
pass `-DEIGEN_MAX_ALIGN_BYTES=32`.

## 3. Open points for the settler

- The true last-consumed tycho baseline is not in this repository; if tycho consumed any M8 head, drop the groups it
  already has.
- B8's rename tables predate later riders only up to the "oldest spelling -> final" note cited in 2.2; names are
  declared frozen, but confirm against `docs/migration-0.2.md` when compiling tycho.
- No tycho source was read for this note; the tycho call-site counts quoted inside the per-group files are as
  recorded there (2026-11-04).
