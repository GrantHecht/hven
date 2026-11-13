# M8 AP-5: one interface for both engines, and the user callback / stop design

Status: DESIGN, for the owner's approval (G-API follow-on). No code has changed. Base: `origin/m8`
at `09718f3dd`. Line numbers below are at that commit.

Owner requirements this page answers:

- C295 (2026-11-12): "a consistent interface to the two solvers and their results." Already approved:
  `IpmCounters` nested as `IpmResult::counters`, mirroring `SqpResult::counters`, with one name per
  counter the two engines share. This page audits every other IPM/SQP asymmetry in options, results,
  counters, statuses, messages and entry points, and proposes a fix or a KEEP for each one.
- C298 and its amendment (2026-11-12): a callback interface that can end a solve early and still hand
  back the current iterate, plus live access to the iterate and the multipliers (for plotting in a GUI).
  Points 1-7 of the amendment were settled without objection and are taken as given here.

The B1-B8 break froze the public names. Every rename below is therefore listed as a break, with its
migration line (§4).

---

## 1. Inventory: the current surface, side by side

`✓` = symmetric. `✗` = asymmetry, with its fix ID (§2/§3). `=` = engine-specific by nature (KEEP).

### 1.1 Entry points (`drivers/ipm_solver.h`, `drivers/sqp_solver.h`)

| | IPM | SQP | |
|---|---|---|---|
| construct / move / `options()` / `set_options()` | `ipm_solver.h:85-108` | `sqp_solver.h:52-76` | ✓ (SQP rebuilds its QP engines `:69-76`: =) |
| `solve(problem, x0, WarmStart, SolveBudget)` | `:181` | `:147` | ✓ |
| native warm overload | — | `:162` (`SqpWarmStart`) | = |
| `last_result()` after an escaped exception | `:184-196`, carries `x, lambda, z` | `:165-177`, **`z` left empty** (`:171`) | ✗ E2 |
| `last_stop_reason()` | `:114`, the LAST PHASE's `IpmStopReason` | `:183`, whole solve, `SqpStopReason` | ✗ E1 (on the solver, not the result; two enums) |
| `set_iteration_callback(cb, payload, scope)` / `iteration_payload()` / `iteration_scope()` / `clear_iteration_callback()` | `:239-252` | `:237-252` | ✓ |
| `attach_trace` / `attach_ledger` | `:272`, `:279` | `:265`, `:273` | ✓; doc names a retired `common.print_level` (`ipm_solver.h:265`, `sqp_solver.h:257`) ✗ D1 |
| KKT hook | `KktHook` `:79`, `set_kkt_hook` `:256` | — | = (the one IPM extension, stated `:60-63`) |
| introspection | `kkt_pattern_is_analyzed` `:121` | `ssn_tier_threads` `:81`, `ipqp_tier_threads` `:85` | ✗ E3 (SQP test introspection on the public handle) |
| presets | `ipm_preset` (`ipm_options.h:280`) | `sqp_preset` (`sqp_options.h:936`) | ✓ |
| worker preset | `ipm_worker_options` (`ipm_options.h:302`) | — | ✗ E4 |
| `validate`, `dump_options` | `ipm_options.h:272`, `:298` | `sqp_options.h:928`, `:948` | ✓ |
| iteration-table helpers | console sink only | `format_iteration_table` + five pieces (`sqp_solver.h:288-326`) | = until TR-2 |

### 1.2 Options (`drivers/common_options.h`, `ipm_options.h`, `sqp_options.h`)

| | IPM | SQP | |
|---|---|---|---|
| `common` (tolerances, limits, verbosity, threads, start level, `infinite_bound`) | `ipm_options.h:194` | `sqp_options.h:714` | ✓ |
| `common.max_iterations` | per phase, ≥1, default 1000 | whole solve including restoration majors, ≥0, default 100 (`common_options.h:57-62`) | = (phases vs a nested sub-solve); documented |
| `common.threads` | default `HVEN_DEFAULT_IPM_THREADS`, ≥1 | default 0 = backend default (`:85-100`) | = documented |
| return-best | `return_best` (any non-optimal exit except an interrupt) + `best_iterate_criterion` (`ipm_options.h:242-244`) | `return_best_on_max_iterations` (kIterationLimit only; best by (h, f)) (`sqp_options.h:774`) | ✗ O1 |
| termination tolerances | `termination` {acceptable_*, divergence_*} (`ipm_options.h:93-114`) | none (SQP has no acceptable level) | = |
| `linear` | 12 fields (`:45-85`) | 2 fields (`sqp_options.h:676-688`); the shared two have the same names | ✓/= |
| `advanced.max_restoration_entries` | 2 (`ipm_options.h:151`) | 10 (`sqp_options.h:672`) | ✓ name; = defaults |
| objective scaling | `objective_scale` fixed factor (`:237`) | `row_scaling`, `column_scaling` (automatic) | = (different features) |
| console | `wide_console` (`:247`) | — | = |
| doc refers to `kkt_tol` (meaning `common.optimality_tol`) | — | `sqp_solver_types.h:121`, `:513`, `:555`; `solve_result.h:162` | ✗ D2 |

### 1.3 Results (`drivers/solve_result.h`, `ipm_solver_types.h`, `sqp_solver_types.h`)

The shared `SolveResult` base (`solve_result.h:124-274`) is already symmetric: `status`, `message`,
`returned_best_iterate`, `infeasibility_certified`, `x`, `lambda`, `z`, `f`, the three shared
diagnostics, `g`, `iterations`, `work`, `evaluation_errors`, `wall_seconds`, `warm_start`. What follows
is everything above the base.

| concept | `IpmResult` (`ipm_solver_types.h`) | `SqpResult` (`sqp_solver_types.h`) | |
|---|---|---|---|
| counter tree | **none: flat fields** `:94-250` | `counters` (`SqpCounters`) `:528` | ✗ R1 (approved fix) |
| per-iteration history | none (trace and callback only) | `history` (`std::vector<SqpIterate>`) `:531` | ✗ R2 (Q8) |
| engine convergence measure | `convergence {kkt, barrier, equality, inequality}` `:35-44`, `:147` | `convergence {stationarity, feasibility, complementarity, kkt_residual}` `:508-519`, `:534` | ✗ R3 (Q7) |
| timing | `timing` (`IpmTiming`, 6 fields) `:50-71`, `:150` | `engine_seconds` (flat) `:546` | ✗ R4 |
| last-phase diagnostics | nine `last_*` fields `:183-224`, one of them `Eigen::ComputationInfo` `:224` | — | ✗ R5, R6 |
| per-phase account | `phases` (`IpmPhaseReport`) `:231` | — | = |
| stop reason on the result | `phases[i].stop_reason` only (`ipm_phase_report.h:34`) | none | ✗ E1 |
| factorization snapshot | `factor_memory`, `factor_flops`, `kkt_analyses_total`, `kkt_factor_counters` `:154-156`, `:239-244` | — | = |
| fixed-variable rows | `fixed_variable_treatment`, `internal_fixed_ce`, `internal_fixed_lambda_e` `:123-143` | — | = |
| scaling / equality basis / native warm | — | `scaling`, `equality_basis`, `sqp_warm_start` `:556-576` | = |
| warm-start bookkeeping | `payload_ignored` `:94`, `polish_ignored` `:113` | `counters.start_level_used` (`solver_counters.h:1627`), `counters.polish_ignored` (`:1818`) | ✗ R7 |
| ledger record | `IpmSolveRecord` **carries no counters** (`core/ledger.h:141-145`) | `SqpSolveRecord` carries them | ✗ R8 |

### 1.4 Counters

IPM counters that already exist, all flat on `IpmResult`, against their SQP counterparts:

| IPM today | SQP counterpart | same quantity? |
|---|---|---|
| `work.factorizations` (`solve_result.h:96`) | `counters.factorizations` (`solver_counters.h:1495`), which equals `work.factorizations` (`solve_result.h:87-89`) | yes |
| `nonfinite_step_rollbacks` `:101` | `nonfinite_step_rollbacks` `:2260` | yes, same name ✓ |
| `polish_ignored` `:113` | `polish_ignored` `:1818` | yes, same name ✓ |
| `second_order_corrections` `:163` (one per attempt) | `soc_steps` `:1498` (one per attempt) | yes ✗ |
| `watchdog_activations` `:168` (arms) | `watchdog_restores` `:1644` (restores) | **no**: different events, so the names stay different |
| `last_feasibility_restoration_entries` `:204` (last phase, optional) | `restoration_entries` `:2009` (whole solve) | yes once whole-solve ✗ |
| `last_feasibility_restoration_iterations` `:208` | `restoration_iters` `:1504` | yes once whole-solve ✗ |
| `kkt_analyses_this_call` `:241` | `symbolic_analyses` `:1606` | yes ✗ |
| `dense_residual_ratio_triggers` `:250` | `dense_solve_residual_refusals` `:2322` (approved C299) | yes: the same dense solve check, one threshold (`kDenseResidualRatioSingular`, C297 R1) ✗ |
| `dense_factorizations` (approved C294, lands with DN-3 proper) | `dense_factorizations` | yes |
| `recovery_depth_histogram` `:176` | — | IPM only |
| — | `start_level_used` `:1627` | the IPM has the fact but no counter ✗ R7 |

### 1.5 Statuses and messages (`core/solver_status.h`, `src/drivers/solve_status.cpp`, `result_message.cpp`)

| | IPM | SQP | |
|---|---|---|---|
| reachable `SolveStatus` | never `kInfeasible` | never `kAcceptable`, `kStalled`, `kDiverging` (`solver_status.h:30-34`) | = |
| restoration ends locally infeasible | **`kStalled`** (`solve_status.cpp:35-37`) | **`kInfeasible`**, certified or not (`solver_status.h:36-47`) | ✗ S1 (Q6) |
| stop-reason enum | `IpmStopReason` (`:128-164`): uses `kIterationLimit`/`kWorkLimit`/`kTimeLimit` | `SqpStopReason` (`:173-213`): `kNone` when the status already names the cause | ✗ E1 |
| `returned_best_iterate` | any non-optimal exit except an interrupt (`solve_result.h:136-144`) | kIterationLimit only (`:145-148`) | ✗ O1 |
| structured messages | `IpmMessageKind` (`solver_status.h:230`) → `ipm.message` | none | ✗ M1 → TR-1 (trace upgrade) |
| `message` on a callback stop | "interrupted by the iteration callback" (`result_message.cpp:110`) | same (`:171`) | ✓ |
| `iterations` | includes the in-place restoration iterations | excludes restoration majors (`solve_result.h:218-220`) | = documented; `counters.restoration_iters` lets a caller subtract on both |

### 1.6 Callbacks: what the code does today

- One `IterationCallback` for both engines (`solve_result.h:462`), `CallbackAction {kContinue, kStop}`
  (`:277-288`), `IterationPayload {kScalars, kVectors, kDiagnostics, kAll}` (`:302-317`),
  `CallbackScope {kTopLevel, kIncludeRestoration}` (`:338-346`), `IterationEvent` (`:390-451`).
  Engine-specific fields are top-level optionals: `phase`, `mu` (IPM) and `depth`, `radius` (SQP) ✗ C4.
- **The IPM returns the point it showed** (verified). The continuing event fires at the top of the
  iteration, before factorization (`ipm_solver.cpp:3702-3706`). A `kStop` is read immediately after
  (`:3720-3724`) and returns that point. `return_best` deliberately does not substitute on that exit
  (`:3715-3718`, `:4924-4927`). The stop also ends the phase sequence (`:6579-6588`).
- **The SQP returns one row later.** Its event fires when a history row is pushed, after that row's
  subproblem has been solved (`sqp_solver.cpp:6931-6955`). The latch is set at `:6646-6647` and read at
  the next pass's exit conjunction (`:7418-7425`). The documented consequence (`sqp_solver.h:202-203`):
  a stop returned on row k is honoured at row k+1, whose own terminal event fires. So on both engines
  the returned point is the point of the **last event delivered**, but the SQP spends one more step to
  get there ✗ C2 (KEEP, stated and tested; §3.4).
- **Stop inside restoration.** IPM: the restoration runs in place, so the stop at `:3720` returns the
  restoration iterate. SQP: the stop latches through the sub-solve's forwarder (`:6037-6038`) and the
  exit returns the restored point `st.x` (`:8892-8906`, ruling R4). Neither returns the last normal
  iterate, which the C298 amendment requires ✗ C1.
- **Stale contract text.** The public header says IPM nested l1 restoration "is silent: its iterations
  fire nothing" (`ipm_solver.h:212-213`). The impl note says the l1 subproblem "is solved by a DISTINCT
  IpmSolver" (`ipm_solver_impl.h:510-516`). Both contradict `l1_restoration.h:55-57` ("an in-place
  phase … rather than a separate nested solver instance") and `core/ledger.h:146-149`. On a reading of
  the code, an iteration that stays in restoration falls through (`ipm_solver.cpp:3226-3231`) to the
  continuing dispatch, and `fire_iteration_event` already reports `f` and the diagnostics as NaN while
  restoration is active (`:2383-2385`, `:2519-2520`). So the rows are probably delivered today. G2's
  first step pins this with a test before anything relies on it ✗ D3.
- **Best-iterate tracking exists only when the return-best option is on**: IPM `ipm_solver.cpp:3479`
  (and it is suspended while restoration is active), SQP `sqp_solver.cpp:7319`.
- **The poll points that exist** are the work and time limits: IPM at the iteration top
  (`ipm_solver.cpp:3744-3759`, `limit_reached` `:166-179`), SQP at the exit conjunction
  (`sqp_solver.cpp:7401-7419`). Inside an iteration there is none: not in the IPM inertia ladder
  (`:2091`, up to `max_inertia_correction_attempts` = 15 factorizations), and not in the SQP QP minor
  loops (walk `qp_engine.cpp:763`, SSN `ssn_engine.cpp:781`, IPQP `ipqp_engine.cpp:899` budget).

---

## 2. Target shape: options, results, counters

### 2.1 `IpmCounters` (new, `core/solver_counters.h`, with `HVEN_IPM_COUNTERS_FIELDS` beside the SQP X-macro)

A name in **bold** is shared with `SqpCounters`, with the same meaning, as C295 requires.

| field | type | from (old → new) | meaning |
|---|---|---|---|
| **`factorizations`** | Index | new; equals `work.factorizations` | KKT factorizations, the whole solve |
| **`symbolic_analyses`** | Index | `IpmResult::kkt_analyses_this_call` | KKT pattern analyses during this call |
| **`restoration_entries`** | Index | `IpmResult::last_feasibility_restoration_entries` (optional, last phase) | restoration entries, **whole solve**; 0 when no strategy is constructed |
| **`restoration_iters`** | Index | `IpmResult::last_feasibility_restoration_iterations` | iterations in restoration mode, whole solve |
| **`soc_steps`** | Index | `IpmResult::second_order_corrections` | second-order correction attempts |
| **`nonfinite_step_rollbacks`** | Index | `IpmResult::nonfinite_step_rollbacks` | unchanged |
| **`polish_ignored`** | Index | `IpmResult::polish_ignored` | unchanged |
| **`start_level_used`** | StartLevel | new | the level the payload resolved to (kCold when none was given or `payload_ignored`) |
| `payload_ignored` | Index | `IpmResult::payload_ignored` | unchanged (also added to SQP, R7) |
| `watchdog_activations` | Index | `IpmResult::watchdog_activations` | unchanged; deliberately ≠ SQP's `watchdog_restores` |
| `recovery_depth_histogram` | `std::array<Index,5>` | `IpmResult::recovery_depth_histogram` | unchanged |
| `inertia_corrections` | Index | new (optional; drop if not wanted) | sum of `IpmIterate::h_facs` over the solve, i.e. ladder factorizations |
| **`dense_solve_residual_refusals`** | Index | `IpmResult::dense_residual_ratio_triggers` | dense solves refused by the residual check (shared threshold) |
| **`dense_factorizations`** | Index | C294 name; lands with DN-3 proper | dense-arm factorizations |

On the first single-phase default the whole-solve and last-phase values coincide, so the corpus values
of the two restoration counters do not move.

`restoration_entries`/`restoration_iters` change meaning (last phase → whole solve) only on a
multi-phase solve. The per-phase value moves to `IpmPhaseReport` (§2.2).

M7-19 (the IPM count-identity hole in restoration) rides this group, as TR-3 planned, because it moves
IPM counters.

### 2.2 `IpmResult` regrouped

- The `last_*` fields move onto the phase they describe: `IpmPhaseReport` gains `funnel_width`,
  `filter_size`, `filter_resets`, `monotone_switches`, `monotone_iterations`,
  `restoration_entries`, `restoration_iters`, `proximal_regularization_primal`,
  `proximal_regularization_dual`, `kkt_factor_status`, all `std::optional` with today's nullopt
  rules. A free helper `const IpmPhaseReport *last_ran_phase(const IpmResult &)` replaces the "last
  phase wins" reading.
- `last_kkt_info` (`Eigen::ComputationInfo`, a leaked Eigen enum) becomes
  `IpmPhaseReport::kkt_factor_status` of type `IpmKktFactorStatus` (`solver_status.h:273`, whose map is
  already the identity on the four names).
- KEEP on `IpmResult`: `fixed_variable_treatment`, `internal_fixed_*`, `factor_memory`, `factor_flops`,
  `kkt_analyses_total` (a solver-lifetime figure, not per solve), `kkt_factor_counters`, `phases`,
  `timing`, `convergence`.
- `IpmSolveRecord` gains `counters` (R8). Its existing fields that duplicate a counter are read from it.

### 2.3 Shared result additions (`SolveResult`)

```cpp
StopReason stop_reason = StopReason::kNone;          // E1: one enum, on the result
std::optional<IterationSnapshot> best_iterate;        // C298 point 2 (§3.3)
```

`IpmStopReason`, `SqpStopReason`, and both `last_stop_reason()` accessors retire. One `StopReason`
holds the union of their values. As with `SolveStatus`, each engine reaches a subset. The kNone rule
is SQP's: `kNone` whenever the status alone names the cause (kOptimal, kAcceptable, kIterationLimit,
kWorkLimit, kTimeLimit, a certified kInfeasible). The IPM's `kIterationLimit`/`kWorkLimit`/`kTimeLimit`
reasons therefore become `kNone`, and `IpmPhaseReport::stop_reason` takes the same type and rule.

| `StopReason` | was | engines |
|---|---|---|
| `kNone` | both `kNone`; IPM `kIterationLimit`, `kWorkLimit`, `kTimeLimit` | both |
| `kCallbackStop` | both `kInterrupted` | both |
| `kStopRequested` | new (stop token) | both |
| `kException` | both | both |
| `kNonFiniteStart`, `kNonFiniteIterate` | both | both |
| `kEvaluationFailure`, `kStageStalled`, `kRestorationLocallyInfeasible` | IPM | IPM |
| `kSubproblemFailure`, `kInconsistentEqualities`, `kRestorationRefused`, `kRestorationNonFinite`, `kRestorationNoClaim`, `kRestorationFailed` | SQP | SQP |

### 2.4 SQP-side additions

- `SqpCounters::payload_ignored` (new; R7): 1 when a non-empty `WarmStart` was ignored at a kCold
  ceiling. `start_level_used` already exists.
- `SqpResult::timing` (`SqpTiming { double engine_seconds; }`) replaces the flat `engine_seconds` (R4),
  so both results read `result.timing.*`. Informational only.
- `last_result()` fills `z` at the current iterate (E2), as the IPM does.
- `ssn_tier_threads()` / `ipqp_tier_threads()` move to `detail::SqpSolverAccess` (E3).

### 2.5 Options

- **O1 return-best (Q5).** Recommended: one `CommonOptions::return_best` of type
  `ReturnBest {kNever, kOnLimit, kOnAnyNonOptimal}`.
  - `kOnLimit` covers kIterationLimit/kWorkLimit/kTimeLimit. It is SQP's
    `return_best_on_max_iterations = true` widened from the iteration cap to all three limits. That
    widening is a declared event, but no corpus cell has the option on.
  - `kOnAnyNonOptimal` is IPM's `return_best = true`. The SQP implements it in G-RB (non-finite exits
    substitute the best iterate in full, as the IPM does).
  - `IpmOptions::best_iterate_criterion` stays IPM-only. Each engine's "best" ordering is documented:
    IPM by criterion, SQP by (h, f).
  - Every engine excludes interrupts, as today.
- **E4.** `worker_options(IpmOptions)` / `worker_options(SqpOptions)` overloads (threads = 1, silent)
  replace `ipm_worker_options`.
- **Q7 (recommended).** One vocabulary for the engine measures: `IpmConvergence {stationarity,
  complementarity, equality, inequality}`. Correspondingly `termination.acceptable_kkt_tol →
  acceptable_stationarity_tol`, `acceptable_barrier_tol → acceptable_complementarity_tol`,
  `divergence_kkt_tol → divergence_stationarity_tol`, `divergence_barrier_tol →
  divergence_complementarity_tol`. The `equality`/`inequality` split stays IPM-only (its test gates
  them separately).
- KEEP, documented: the meaning and floor of `max_iterations`, the `threads` defaults, `termination`,
  `linear` depth, the scaling features, `wide_console`, the `max_restoration_entries` defaults.
- D1/D2: the doc fixes (`print_level` → `verbosity`, `kkt_tol` → `common.optimality_tol`).

---

## 3. Callback and stop design

### 3.1 Types and signatures

```cpp
// solve_result.h
struct SolveBudget {
    std::optional<Index> max_iterations;
    std::optional<Index> max_minor_iterations;
    std::stop_token stop;          // NEW. Default: no stop source (stop_possible() == false).
};

enum class CallbackAction {
    kContinue = 0,
    kStop = 1,                 // primary point = the current iterate (today's meaning)
    kStopReturnBest = 2,       // NEW: primary point = best_iterate; returned_best_iterate = true
};

enum class IterationPayload : unsigned {
    kScalars = 0, kVectors = 1, kDiagnostics = 2,
    kConstraints = 4,          // NEW: g (row space) and violation (row space, >= 0)
    kBoundMultipliers = 8,     // NEW: z_lower, z_upper (declared width, both >= 0)
    kAll = kVectors | kDiagnostics | kConstraints | kBoundMultipliers,   // widened (Q9)
};

struct IpmIterationInfo {          // delivered under every mask (copies only)
    Index phase;
    double mu;
    double alpha_primal, alpha_dual;          // of the step that REACHED this point; NaN at a phase's first iterate
    double regularization_primal;             // delta_w of the factorization that produced that step (IpmIterate::h_pert); 0 if none
    double regularization_dual;               // delta_c (prox_reg_dual / the classic ladder's), 0 if none
    Index inertia_corrections;                // h_facs of that factorization; -1 sentinel kept (non-finite direction)
    Index line_search_trials;                 // ls_iters of that step
};
struct SqpIterationInfo {
    Index depth;                              // 0 caller's solve, 1 restoration sub-solve
    double trust_radius;                      // SqpIterate::tr_radius
    double violation_l1;                      // h(x), the funnel's measure (the SQP has no merit function on its default)
    double dual_regularization;               // SqpIterate::mu
    bool qp_solved;
    QpStatus qp_status;
    Index qp_minor_iterations;
    Index active_set_size;                    // active_rows + active_lower_sides + active_upper_sides
    StepVerdict verdict;
    bool elastic;                             // SqpIterate::elastic_applied
};

struct IterationEvent {
    Index iteration;
    bool recovering;           // NEW: IPM restoration active; SQP depth >= 1
    double f, stationarity, feasibility, complementarity, step_norm, elapsed_seconds;
    Eigen::Ref<const Vec> x, lambda, z;          // kVectors (BORROWED)
    Eigen::Ref<const Vec> g, violation;          // kConstraints (BORROWED), NEW
    Eigen::Ref<const Vec> z_lower, z_upper;      // kBoundMultipliers (BORROWED), NEW
    std::optional<IpmIterationInfo> ipm;         // exactly one engaged
    std::optional<SqpIterationInfo> sqp;
    // REMOVED (moved): phase, mu -> ipm->phase, ipm->mu; depth, radius -> sqp->depth, sqp->trust_radius
};

struct IterationSnapshot {        // owner-copyable; same field names, owning storage
    Index iteration; bool recovering;
    double f, stationarity, feasibility, complementarity, step_norm, elapsed_seconds;
    Vec x, lambda, z, g, violation, z_lower, z_upper;   // empty where the event carried none
    std::optional<IpmIterationInfo> ipm;
    std::optional<SqpIterationInfo> sqp;
};
IterationSnapshot snapshot(const IterationEvent &event);   // deep copy, O(size of what the mask delivered)
```

- **The event is a view and the snapshot is a value.** A GUI callback calls `snapshot(e)` and hands the
  result to its own queue. The snapshot never aliases engine storage, so another thread may read it
  freely.
- **Engine extras of a symmetric shape** (C298 point 3). They are scalars the engine already holds in
  `IpmIterate`/`SqpIterate`, so they cost a copy and need no mask bit. The SQP's "merit/penalty" is
  `violation_l1`; the elastic ρ is not on the row today and is left out until a consumer asks for it.
- **The extras keep the per-engine timing of `step_norm`** (`solve_result.h:423-427`). IPM extras
  describe the step that REACHED the point (read from the previous `IpmIterate`, because the event
  fires before this iteration factorizes). SQP extras describe the subproblem solved AT the point.
  Aligning them would mean moving the SQP event ahead of its QP, which breaks the identity between the
  history, the trace and the callback (KEEP, documented: C5).
- `CallbackScope` keeps its meaning. It selects the SQP's nested sub-solve rows. On the IPM, restoration
  is in place: its rows are part of `iterations` and are delivered with `recovering = true` under
  either scope (Q11).

### 3.2 Stop semantics: one rule, then each safe point

**The rule.** On a stop (callback `kStop`/`kStopReturnBest` or `budget.stop.stop_requested()`):

1. status `kInterrupted`; `stop_reason` `kCallbackStop` or `kStopRequested`;
2. the primary fields (`x, lambda, z, f`, the diagnostics, `g`, `warm_start`) are the **last normal
   iterate**: the point of the last `recovering == false` event at depth 0 that was delivered, or would
   have been delivered had a callback been attached;
3. under `kStopReturnBest` the primary fields are `best_iterate` instead, and `returned_best_iterate`
   is true;
4. `best_iterate` is filled whenever tracking was armed (§3.3);
5. a converged or acceptable verdict at the same safe point outranks the stop. The precedence is the
   SQP's existing one, which the IPM matches (`ipm_solver.cpp:3720` precedes `:3750`): converged >
   interrupted > work > time > cap;
6. a stop never spends a factorization or a subproblem on the iterate it returns.

| safe point | IPM | SQP | returned point |
|---|---|---|---|
| normal iteration boundary | top of iteration, beside `limit_reached()` (`ipm_solver.cpp:3720`/`:3750`): after the event, before factorization | exit conjunction (`sqp_solver.cpp:7418`): before the next subproblem | the current iterate, i.e. the last event's point (G1) |
| inertia correction | between ladder attempts (`ipm_solver.cpp:2091`): abandon the iteration | n/a | the iteration's start point. Its evaluation is intact (no trial yet), so the result is exact (G4) |
| line search / SOC / extended backtracking / watchdog | **not a safe point**: trials have overwritten the evaluation, and the default ladder is short (2 backtracks) | SOC re-solve: not a safe point, for the same reason | honoured at the next boundary |
| QP subproblem | n/a | main subproblem's minor loop (walk `qp_engine.cpp:763`, SSN `ssn_engine.cpp:781`, IPQP main loop), and its elastic re-solves before any trial | abandon the QP and push the stopped-at row for x_k (`qp_solved = false`, an existing row shape, `sqp_solver.h:303-307`); its event fires; return x_k (G4) |
| restoration (IPM proximal and in-place nested l1) | at the iteration top while restoration is active | inside the sub-solve, which is forwarded the token, at its own boundaries and QP minors | the **last normal iterate**, stashed at restoration entry (below) (G2) |
| nested l1 (IPM) | same as restoration (it runs in the same loop) | — | same |

**The last-normal-iterate stash.** At restoration entry (IPM `enter_feasibility_restoration`,
`ipm_solver.cpp:1484`; SQP `enter_restoration`), when a stop is armed (callback installed, or
`budget.stop.stop_possible()`), the engine copies the entry point: x, multipliers, bound duals, its
declared diagnostics and `f`. That is O(n+m) per restoration entry, and only when armed. IPM
mechanics: the four members `track_best_iterate` already snapshots (`:2226-2238`). SQP mechanics: the
`st.mb` pattern (`sqp_solver.cpp:7327-7338`). Because the stash is pure observation, an armed solve
that never stops is bitwise the unarmed one. On a stop-in-recovery exit, `warm_start` is the stash's
point and multipliers at kSeeded resolution, and `message` gains "; returned the last iterate before
feasibility restoration".

These are **behaviour changes on the stop path only**, and are declared. Today an IPM stop inside
restoration returns the restoration iterate, and an SQP one returns the restored point (`:8892-8906`).
The tests `SqpStopInsideRestorationLatchesAndReachesTheParent` and
`SqpStopInsideRestorationArrivesAsTheSubSolvesOwnVerdict` (`tests/drivers/test_callback.cpp:668`,
`:728`) are re-derived with their new expected point. `infeasibility_certified` keeps its rule
(`SqpStopOnTheRestorationsConvergedRowInterruptsAndKeepsTheCertificate`, `:927`, unchanged).

**The token.** `std::stop_token` is a shared atomic. `std::stop_source::request_stop()` may be called
from any thread, a GUI thread included, with no callback attached and no lock. The solver never writes
it. This fits `docs/concurrency.md`: the token is per call, inside `SolveBudget`, so N concurrent solves
take N independent sources. A source that is already stopped stops the next solve at its first safe
point; this is documented as "a token is per solve". Re-entering `solve()` from a callback stays
forbidden (`concurrency.md:69-73`).

### 3.3 Best-so-far

`best_iterate` is the engine's own best tracker, held as an `IterationSnapshot` with `x`, `lambda`,
`z`, `g` and the diagnostics always filled:

- IPM: `track_best_iterate` by `best_iterate_criterion`, suspended in restoration (`ipm_solver.cpp:3479`),
  per phase (last phase wins);
- SQP: `st.mb` by (h, f). The parent's tracker only, so the best is always a normal, caller-space
  iterate.

**Armed** when the return-best option is on, or a stop is armed. When armed it is filled on every exit
other than kOptimal/kAcceptable (Q4). Tracking is observational: it copies, it never steers.

### 3.4 What stays as it is (stated and tested)

- C2: the SQP's callback stop at row k returns at row k+1. That is the last event's point, so rule 2
  holds on both engines. A **token** stop on the SQP is honoured at the conjunction of the pass that
  measured the row and does not spend that extra step.
- A callback that only returns `kContinue` cannot move a counter, and neither can an armed token that
  never fires (extends `AReadOnlyCallbackChangesNothingOnEitherEngine`, `test_callback.cpp:1813`).
- No mid-solve option mutation (C298 point 6). `set_options` already throws with a solve in flight
  (`ipm_solver.h:106-107`, `sqp_solver.h:75`).

### 3.5 Cost when unused, and how it is proven

**Unused** means no callback and a default `stop` token. The cost is then:

- one `stop_requested()` per iteration, IPM and SQP major. A default token has no state, so the call is
  a null-pointer test;
- in the QP minor loops and the inertia ladder (G4), the engines receive a
  `const std::stop_token *` that is `nullptr` unless `stop_possible()`, so the test there is one
  well-predicted null test per minor or attempt. Its signature is detail-internal;
- the stashes and best tracking are not armed.

**The proof**, per group:

1. counter/status bit-identity on the 27-cell replay × 3 engines (AP-2 proof 5), plus the P2/P3 replay
   legs in the DN-3b pattern (`cbaf26467`);
2. the P5 serial runtime gate, filtered B/A per mode within ±0.5 % (`6a9fbfe39`'s instrument), run
   solo under CLAUDE.md §7. It is required for G1 and G4, which touch hot loops;
3. for G4, pass A/B instruction counts on the walk, SSN and IPQP arms (AP-1 §13 B1's practice);
4. for G3/G5, the attached-callback leg (the M7-4 residual, +0.6 %) re-measured at kScalars and kAll,
   with the new kAll's cost reported separately.

### 3.6 Test plan

| # | case | engines | mechanism |
|---|---|---|---|
| T1 | token stopped before `solve()` → kInterrupted at the start point, 0 iterations, `kStopRequested` | both | plain |
| T2 | cross-thread stop: the callback at event k signals a second thread, which calls `request_stop()` and acks; returned x == snapshot of the last delivered event | both | `std::thread` + latch handshake (deterministic, no sleeps) |
| T3 | token stop mid inertia ladder → the iteration's start point, 0 extra iterations, diagnostics equal to that event's | IPM | KKT hook (`ipm_solver.h:79`) calls `request_stop()` on the 2nd factorization of a ladder |
| T4 | token stop mid QP minor → stopped-at row `qp_solved == false`, returned x == x_k, `history.size() == major_iters + 1` | SQP, per qp_mode | `HVEN_TESTING` walk observer (`qp_engine.cpp:742`); SSN/IPQP via their observers in the test executable |
| T5 | stop inside restoration (callback on a `recovering` event, and token) → the last normal iterate; `recovering == false` on the event that showed it; `best_iterate` present | IPM (proximal, nested l1), SQP | existing restoration fixtures (`test_callback.cpp:668`, `test_evaluation_failure.cpp:886`) |
| T6 | converged beats stop (token set on the converging row) → kOptimal | both | extends `SqpConvergedBeatsStop` `:640`, `IpmStopEndsThePhaseSequence` `:1340` |
| T7 | `kStopReturnBest` → primary == `best_iterate`, `returned_best_iterate` | both | — |
| T8 | precedence at one boundary: stop with a work limit and a time limit also reached → kInterrupted | both | `max_factorizations` forced |
| T9 | armed-and-unfired == unarmed, counter for counter, history row for row | both | extends `:1813` |
| T10 | N solvers on N threads, one stopped, the others equal to serial | both | extends `Concurrency.NInstancesOnNThreadsMatchSerial` |
| T11 | callback throws after a stop request → the exception contract wins, and `last_result()` carries `z` on the SQP (E2) | both | extends `:1646` |
| T12 | each new payload bit delivers exactly its fields; `snapshot()` survives the solve; the views are empty when not requested | both | extends the `CallbackPayload` suite |
| T13 | multi-phase IPM: a token ends the sequence; the remaining phases have `ran == false` | IPM | — |
| T14 | IPM in-place restoration rows are delivered with `recovering = true` (pins D3 first) | IPM | restoration fixture |
| TSan | T2/T10 under one local ThreadSanitizer build (no CI lane) | both | declared |

### 3.7 Python (later), and the example program

- The binding releases the GIL for `solve()`. The callback trampoline re-acquires it per event, so a
  GUI should use kScalars with decimation, or a callback written in C++.
- The trampoline must call `snapshot()` into numpy while holding the GIL. It must never hand Python a
  view of borrowed memory.
- `StopSource.request_stop()` is lock-free and safe from any Python thread. A SIGINT handler that calls
  it gives Ctrl-C a clean kInterrupted that carries the iterate.
- A Python exception in a callback propagates as today (`last_result()`).
- Example (AP-3 re-cut): `examples/08_live_progress.cpp`, with no GUI dependency. A `std::jthread`
  worker solves while the main thread drains snapshots, prints a running table of the iterate and the
  multipliers, and calls `request_stop()` on a user condition. The matplotlib version lands with the
  bindings.

---

## 4. Names for approval

Every row is a public name. **break** = an existing public name changes, with its migration line.

| new public name | kind | was / migration |
|---|---|---|
| `IpmCounters`, `IpmResult::counters` | type, field | approved (C295); `r.<field>` → `r.counters.<field>` per §2.1 |
| `IpmCounters::{factorizations, symbolic_analyses, restoration_entries, restoration_iters, soc_steps, start_level_used, inertia_corrections, dense_solve_residual_refusals}` | fields | **break**: `kkt_analyses_this_call`, `last_feasibility_restoration_entries` (optional → Index, whole solve), `last_feasibility_restoration_iterations`, `second_order_corrections`, `dense_residual_ratio_triggers`; the rest are new |
| `IpmCounters::{nonfinite_step_rollbacks, polish_ignored, payload_ignored, watchdog_activations, recovery_depth_histogram}` | fields | **break** (moved): `r.x` → `r.counters.x` |
| `IpmPhaseReport::{funnel_width, filter_size, filter_resets, monotone_switches, monotone_iterations, restoration_entries, restoration_iters, proximal_regularization_primal, proximal_regularization_dual, kkt_factor_status}` | fields | **break**: `r.last_<name>` → `last_ran_phase(r)-><name>`; `last_kkt_info` (Eigen) → `kkt_factor_status` (`IpmKktFactorStatus`) |
| `last_ran_phase(const IpmResult&)` | free function | new |
| `SqpCounters::payload_ignored` | field | new |
| `SqpTiming`, `SqpResult::timing` | type, field | **break**: `r.engine_seconds` → `r.timing.engine_seconds` |
| `StopReason` (values as §2.3), `SolveResult::stop_reason`, `IpmPhaseReport::stop_reason : StopReason` | enum, fields | **break**: `IpmStopReason`/`SqpStopReason` retire; `solver.last_stop_reason()` → `result.stop_reason` (after a throw, `solver.last_result().stop_reason`); `k*::kInterrupted` → `StopReason::kCallbackStop`; IPM `kIterationLimit`/`kWorkLimit`/`kTimeLimit` reasons → `kNone` (the status names them) |
| `StopReason::kStopRequested` | value | new |
| `SolveBudget::stop` (`std::stop_token`) | field | new (Q1, Q2) |
| `CallbackAction::kStopReturnBest` | value | new |
| `SolveResult::best_iterate` | field | new |
| `IterationSnapshot`, `snapshot(const IterationEvent&)` | type, free function | new |
| `IterationPayload::kConstraints`, `::kBoundMultipliers`; `kAll` widened | values | new; `kAll` meaning widens (Q9); the old shape is `kVectors \| kDiagnostics` |
| `IterationEvent::{recovering, g, violation, z_lower, z_upper, ipm, sqp}` | fields | new |
| `IpmIterationInfo{phase, mu, alpha_primal, alpha_dual, regularization_primal, regularization_dual, inertia_corrections, line_search_trials}` | type | new |
| `SqpIterationInfo{depth, trust_radius, violation_l1, dual_regularization, qp_solved, qp_status, qp_minor_iterations, active_set_size, verdict, elastic}` | type | new |
| — | removed fields | **break**: `e.phase` → `e.ipm->phase`; `e.mu` → `e.ipm->mu`; `e.depth` → `e.sqp->depth`; `e.radius` → `e.sqp->trust_radius` |
| `ReturnBest {kNever, kOnLimit, kOnAnyNonOptimal}`, `CommonOptions::return_best` | enum, field | **break** (Q5): `ipm.return_best = true` → `ipm.common.return_best = ReturnBest::kOnAnyNonOptimal`; `sqp.return_best_on_max_iterations = true` → `sqp.common.return_best = ReturnBest::kOnLimit` |
| `worker_options(IpmOptions)`, `worker_options(SqpOptions)` | free functions | **break**: `ipm_worker_options(o)` → `worker_options(o)` |
| `IpmConvergence::{stationarity, complementarity}` | fields | **break** (Q7): `kkt` → `stationarity`, `barrier` → `complementarity` |
| `IpmTerminationOptions::{acceptable_stationarity_tol, acceptable_complementarity_tol, divergence_stationarity_tol, divergence_complementarity_tol}` | fields | **break** (Q7): `*_kkt_tol` → `*_stationarity_tol`, `*_barrier_tol` → `*_complementarity_tol` |
| `IpmSolveRecord::counters` | field | new |

**Detail-internal (not for approval):** the `const std::stop_token *` plumbing into `QpEngine`,
`SsnEngine` and `IpqpEngine`; the recovery-entry stash; the armed predicate; best-tracker arming;
`detail::SqpSolverAccess::{ssn_tier_threads, ipqp_tier_threads}` (moved from the public handle, E3);
the `HVEN_IPM_COUNTERS_FIELDS` X-macro; the test observers.

---

## 5. Implementation plan

Effort is in AP-1 §13's units. Every group carries AP-2's common six (both suites, install smoke, export
contract, golden rig, 27-cell replay × 3 byte-exact, census if it can move the walk). Groups that touch
`ipm_solver.cpp`/`sqp_solver.cpp` land **after DN-3 proper**, which is in flight on another branch and
also adds `dense_factorizations`.

| group | content | behaviour change | proof beyond the common six | est. |
|---|---|---|---|---|
| **G1** stop token + `StopReason` | `SolveBudget::stop`; polls at the two existing boundaries; `StopReason` replaces both enums and `last_stop_reason()`; `SolveResult::stop_reason`; message text; D1/D2 doc fixes | none without a token; stop-reason columns renamed (values mapped, declared) | P5 runtime gate (all four modes); T1, T2, T6, T8, T10, T13 | 1.5 |
| **G2** recovery-aware stop | pin D3 (T14); the stash at restoration entry (both engines); the token forwarded into the SQP sub-solve; `recovering` flag; IPM/SQP stop-in-recovery returns the last normal iterate; fix the `ipm_solver.h:212`/`ipm_solver_impl.h:510` text | **declared, stop path only** (the IPM and SQP stop-in-restoration point) | the replay byte-exact (no corpus cell stops); T5, T9; the two SQP restoration-stop tests re-derived | 1.5 |
| **G3** best-so-far + actions | `IterationSnapshot`, `snapshot()`, `SolveResult::best_iterate`, `kStopReturnBest`, best-tracker arming | none (observational) | replay byte-exact; T7, T9; attached-callback leg | 1.5 |
| **G4** intra-iteration safe points | IPM ladder poll; SQP main-QP minor polls (walk, SSN, IPQP) + elastic re-solves; the stopped-at row | none without a token | P5 runtime + pass A/B instructions on walk/SSN/IPQP; T3, T4 | 2 |
| **G5** payload | `kConstraints`, `kBoundMultipliers`, `kAll` widened; `ipm`/`sqp` info structs; the four top-level optionals removed | none (callback-only; the trajectory is untouched) | replay; T12; attached-callback leg at kScalars/kAll (M7-4 residual re-measured) | 1.5 |
| **G6** `IpmCounters` + IPM result regroup | §2.1, §2.2; `IpmSolveRecord::counters`; `ipm.solve.end` trace keys follow the new names; M7-19 rides | none in value; names move | replay byte-exact through a column map (old → new, in the declaration); trace goldens re-derived (declared); ledger record pinned | 2 |
| **G7** remaining symmetry | E2 (SQP `last_result().z`), E3, E4, R4 (`SqpTiming`), R7 (`payload_ignored` on SQP, `start_level_used` on IPM), Q7 renames if approved | E2 fills a field on the throw path only | replay; options-dump drift pin re-derived (declared); T11 | 1 |
| G-RB (if Q5) | `CommonOptions::return_best`; SQP `kOnAnyNonOptimal`; SQP `kOnLimit` widened to work/time limits | **declared**: substitution on more exits, off by default | replay byte-exact (off in every cell); forced-exit pins per value | 1.5 |
| G-S (if Q6) | IPM restoration-locally-infeasible → `kInfeasible` (uncertified) | **declared status event** | the affected corpus cells counted in the declaration before the first leg; their status column re-derived; points unchanged | 0.5 |
| EX | `examples/08_live_progress.cpp` in the AP-3 re-cut | — | runs as a test in CI on all three lanes | 0.5 |
| | **total** | | | **11.5-13.5** |

**Order:** G1 → G2 → G3 → G4 → G5 → G6 → G7, then G-RB and G-S as separate declared events, and EX with
AP-3. Revertibility: G1-G3 revert in reverse order. G4, G5 and G6 touch disjoint code and revert alone.
G7 reverts alone.

---

## 6. Open questions for the owner

| # | question | recommendation |
|---|---|---|
| Q1 | Token type: `std::stop_token`, or an own `hven::StopSource`/`StopToken`? | `std::stop_token`. It is standard, lock-free, and `std::jthread` hands one to a worker. G1's first CI run is the AppleClang libc++ check; if it is missing there, fall back to a two-member own type with the same API. |
| Q2 | Where the token goes: `SolveBudget::stop`, a solver setter, or a renamed `SolveBudget`? | `SolveBudget::stop`. It is per call (concurrency-clean), with no solver state and no rename. |
| Q3 | One `StopReason` on the result, replacing both enums and `last_stop_reason()`, with `kCallbackStop` distinct from `kStopRequested`? | Yes to both. |
| Q4 | Fill `best_iterate` only on stops, or on every non-optimal exit where tracking was armed? | Every non-optimal exit where it was armed. It costs nothing more once armed, and a continuation driver wants it on limits too. |
| Q5 | Unify return-best as `CommonOptions::return_best` (kNever/kOnLimit/kOnAnyNonOptimal) and implement the missing SQP value? | Yes, full unification (G-RB, 1.5). The alternative is the rename only, with each engine refusing the value it lacks (0.5). The best ordering stays per engine. |
| Q6 | Relabel the IPM's restoration-locally-infeasible exit from `kStalled` to `kInfeasible` (uncertified)? AP-1 §5 kept it. | Yes, as its own declared status event (G-S). It is the same situation the SQP already labels `kInfeasible`, and `infeasibility_certified = false` keeps it honest. |
| Q7 | Rename `IpmConvergence {kkt, barrier}` and the matching termination tolerances to stationarity/complementarity? | Yes. It gives one vocabulary across the results, `CommonOptions` and the SQP. |
| Q8 | Add `IpmResult::history` (the IPM counterpart of `SqpResult::history`)? | Not now. Exposing `IpmIterate` would freeze engine-internal column names (`prim_obj`, `pen_par1`, …). Let TR-1's v1 row schema define the row, then add `history` on it. |
| Q9 | Widen `kAll` to include the new bits? | Yes. "All" should mean all. The cost is O(n+m) per event on consumers who chose everything, and is re-measured in G5. |
| Q10 | Intra-iteration safe points (G4): the IPM inertia ladder and the SQP main-QP minors only? Should `time_limit_seconds` share them? | Yes to the ladder and the minors; no to the line search and SOC (they would need a re-evaluation). For the time limit: not now, registered; it would move kTimeLimit exit points, which is its own event. |
| Q11 | IPM restoration rows under `kTopLevel`: keep delivering them (flagged `recovering`), or suppress them for symmetry with the SQP? | Keep. They are part of the IPM's `iterations`, and suppressing them breaks "events == iterations" (`ipm_solver.h:235-236`). `recovering` lets a consumer filter. |
| Q12 | Move `ssn_tier_threads()`/`ipqp_tier_threads()` off the public SQP handle into `detail`? | Yes. They are test introspection with no consumer. |

Out of scope, registered: the structured SQP messages (M1) go to TR-1. The elastic penalty ρ on SQP
events waits for a consumer to ask for it.
