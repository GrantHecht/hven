# Migrating across hven's M8 API break (0.2.0)

This is the page a consumer reads to move a tree from the hven that existed
before the M8 API break window to the hven after it. It is organised by what you
have to change. The M6 break has its own guide
(`docs/migration/2026-09-m6-api-break.md`); that one covers the names before
`hven::solvers` was dropped.

**Scope.** The M8 break landed as eight groups, B1 to B8, in one window. The
one-line before and after of each group is in `CHANGELOG.md` (the 0.2.0 entry).
The complete old-to-new name table, generated from the rename's own manifests,
is `docs/notes/2026-11-m8-ap2-b8-naming.md` section 3. This page carries the
rewrite rules and the parts a reader trips on; it does not repeat the 500-row
table.

**What comes after the break.** Interface groups that landed later in M8 (the
stop token, one `StopReason`, the regrouped `IpmResult`, the trace and
include-path changes, return-best, and so on) have their own sections in
`docs/migration-0.2.md`. If you are coming from a pre-M8 tree, read this page
first and that one second: that page's "before" is the state after this one.

**Names are frozen.** After the B8 riders the public names do not change until
the next break window, the `advanced` option tiers included.

## 0. Start here

Most consumers meet the groups in this order.

| step | group | you are affected if | section |
| --- | --- | --- | --- |
| 1 | B8 names | you name any hven type, enumerator, namespace, header or option | [1](#1-the-names-b8) |
| 2 | B2 one problem | you write a model, build a program, or call `solve` | [2](#2-one-problem-b2) |
| 3 | B3 options | you configure either engine | [3](#3-options-and-limits-b3) |
| 4 | B4 results | you read a result, warm-start, or attach a callback or trace sink | [4](#4-results-warm-start-callbacks-and-trace-b4) |
| 5 | B5 evaluation failure | your model can fail at a trial point, or you catch exceptions | [5](#5-evaluation-failure-b5) |
| 6 | B6 pieces | you assemble a problem from pieces | [6](#6-pieces-b6) |
| 7 | B7 absent bounds | you declare bounds at or beyond 1e20 | [7](#7-absent-bounds-b7) |
| 8 | B1 headers | you include from `detail/` or relied on a solver header's transitive includes | [8](#8-header-hygiene-b1) |

Do the names first: it is mechanical and touches everything. Then the problem
type (the real work), then options and results. The order of sections on this
page follows that advice, not the order the groups landed in.

## 1. The names (B8)

One namespace, `hven`. The rules that let you predict a name are in the naming
note, section 1; the ones you need to rewrite code are these.

### 1.1 Namespaces and headers

| before | after | rewrite |
| --- | --- | --- |
| `hven::solvers::X` | `hven::X` | drop `solvers::` |
| `hven::solvers::detail::X`, `hven::utils::X` | `hven::detail::X` | internals: do not name them; if you did, name them from `detail/` and expect them to move |
| `#include <hven/...>` of individual public headers | `#include <hven/hven.h>` | the one umbrella includes every public header |

The umbrella is the supported include. Individual public headers still exist
(and compile standalone, which the install smoke checks), but their paths moved
during M8; the full before and after path list is the include-path section of
`docs/migration-0.2.md`.

### 1.2 Spelling rules, and what they did to the names you use

| rule | example before | example after |
| --- | --- | --- |
| acronyms are words | `NLPRowClassification`, `ParsedIOFlags`, `KKTVector` | `NlpRowClassification`, `ParsedIoFlags`, `KktVector` |
| enumerators are `kPascalCase` | `ThreadingFlags::MainThread`, `NlpRowKind::Equality` | `ThreadingFlags::kMainThread`, `NlpRowKind::kEquality` |
| enumerators say what they select | `kAugLang`, `kNoLs`, `kAcceptF` | `kAugmentedLagrangian`, `kNone`, `kAcceptObjectiveStep` |
| one concept, one enumerator | `kMaxIter`, `kIterationCap` | `kIterationLimit` |
| an option enum carries its component's prefix | `AcceptanceKind`, `KktOrdering`, `LineSearchMode` | `IpmAcceptanceMode`, `IpmOrdering`, `IpmLineSearchMode` |
| public data members are bare `snake_case` | `VariableBound::index_` | `VariableBound::index` |
| option fields are words | `QpOptions::feas_tol`, `SqpOptions::tr_init` | `QpOptions::feasibility_tol`, `SqpOptions::trust_region_initial` |
| the `advanced` tiers are words too | `enable_soc`, `rho_init`, `qn_update_on_reject` | `second_order_correction`, `primal_regularization_initial`, `lbfgs_update_on_reject` |
| a count function is `num_<things>()` | (varied) | `Problem::num_variables()`, `num_constraints()` |
| a duration is `<what>_seconds` | (varied) | `SolveResult::wall_seconds`, `IpmTiming::kkt_seconds` |
| the factor layer's `FactorKind` | `linear::FactorKind` with `kLDLT`, `kLLT`, `kLU` | `linear::Decomposition` with `kLdlt` only (the other two were refused by both backends) |

Frozen, not renamed: trace keys and the strings an enum prints
(`to_string(SolveStatus::kIterationLimit)` still prints the old frozen
spelling), counter names, and evidence CSV headers. A trace reader or a log
parser written against the old strings keeps working.

Two behaviour-neutral exceptions to know: `max_restoration_entries` on the SQP
now accepts 0 (it refused it), meaning restoration is refused outright; and
`IpmAdvancedOptions::solve_phase_mode` is an `IpmSolvePhaseMode` of three values
(`kOptimize`, `kOptimizeWithoutObjective`, `kSolveEquations`), with the old
`kInit` no longer a public value.

The compile definition changed: `HVEN_DEFAULT_QP_THREADS` is now
`HVEN_DEFAULT_IPM_THREADS` (same value, 8).

### 1.3 A mechanical rewrite

The rename is word-boundary and case-sensitive. A sed pass over your tree built
from the naming note's tables is a reasonable first step, but review the
enumerator rows by hand: `Equality`, `Range`, `Free`, `Heap`, `Inline` and
`Empty` are common words in code that is not hven's.

## 2. One problem (B2)

**Before.** Two public callback contracts, `NlpTripletModel` (Ipopt-shaped, with
triplets) and `NlpModel` (split equality and inequality blocks), and a public
program struct per engine (`NonLinearProgram` for the interior-point engine,
`NlpProblemModel` or `NlpModelAssembly` for the SQP). The two engines took
different objects.

**After.** One callback contract, `hven::Nlp`, and one problem type,
`hven::Problem`, that both engines solve. `NlpModel`, `ParametricNlpModel`,
`NonLinearProgram`, `NlpProblemModel`, `NlpModelAssembly`, `make_nlp_program`
and the master-list `make_nlp` moved to `detail/`.

| before | after |
| --- | --- |
| derive from `NlpTripletModel` | derive from `Nlp` |
| derive from `NlpModel` (split form: `c_E = 0`, `c_I <= 0`, upper triangle returned as a sparse matrix) | derive from `Nlp`: one `g` with `bounds()`, lower-triangle triplets |
| `make_nlp_program(model)` then `solver.solve(program, ...)` | `Problem problem(std::make_shared<MyNlp>()); solver.solve(problem, x0)` |
| `lambda_e`, `lambda_i`, `ce`, `ci` on the result | `lambda` and `g`, one entry per row **of your own `Nlp`**, in declared order |
| `starting_multipliers` on the callback contract | `WarmStart::seed(problem, x, lambda, z)` (multipliers only: pass an empty `x`) |

Rewrite of a model. A triplet model already has the target shape; rename the
base and the methods the compiler names. A split-form model needs one merge:

```cpp
struct MyNlp : hven::Nlp {
    hven::NlpDimensions dimensions() const override;   // n, m, jacobian_nonzeros, hessian_nonzeros
    void bounds(hven::VecRef xl, hven::VecRef xu,
                hven::VecRef gl, hven::VecRef gu) const override; // an equality row: gl == gu
    double eval_f(hven::ConstVecRef x) const override;
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override;
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override;
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override;
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override;
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override;   // lower triangle
    void eval_hessian(hven::ConstVecRef x, double obj_factor,
                      hven::ConstVecRef lambda, hven::VecRef v) const override;
};
```

`examples/01_hs071_both_engines.cpp` is a complete one.

What else changed in this group:

- **Bounds are re-read at every solve**, and `dimensions()` is re-queried at
  every solve. Changing a parameter of your `Nlp` between solves, including a
  bound value, is legal on one `Problem`; the engines clear their per-iterate
  caches. A value-only bound change is adopted without re-analysis. Changing
  the dimensions, a sparsity structure, or a row's kind (equality to
  inequality, say) is refused by name. `examples/05_warm_continuation.cpp` shows
  the in-place sweep. A warm start taken before a change to which variables are
  fixed is refused as an identity mismatch.
- **`lambda` and `g` are in your row space.** A row you wrote as
  `gl == gu` is one `lambda` entry, not an entry in a separate equality vector;
  a lower-bounded row is no longer reported as a sign-flipped inequality
  multiplier. The sign convention is Ipopt's: `L = f + lambda' g`
  (`examples/03_equality_constrained.cpp` reads it).
- **One `Problem` serves both engines** (`examples/01_hs071_both_engines.cpp`).
  The rules for sharing a `Problem` across threads are in `docs/concurrency.md`.
- **A quasi-Newton SQP never reads the Hessian.** `has_hessian() == false` is
  legal for it, restoration included; the interior-point engine and an
  exact-Hessian SQP refuse such a problem by name.

## 3. Options and limits (B3)

**Before.** Tolerances, iteration caps and verbosity were spelled per engine,
and "no limit" was a sentinel value.

**After.** `CommonOptions` is embedded once in each engine's options as
`common`, and every "no limit" is an empty `std::optional`.

```cpp
hven::IpmOptions o;
o.common.optimality_tol      = 1e-8;
o.common.feasibility_tol     = 1e-8;
o.common.max_iterations      = 500;
o.common.max_factorizations  = 2000;   // optional: empty means no ceiling
o.common.time_limit_seconds  = 30.0;   // optional; wall-clock, not reproducible
o.common.verbosity           = hven::Verbosity::kIterations;
hven::IpmSolver ipm(o);                // validate() runs in the constructor
```

| before | after |
| --- | --- |
| per-engine tolerance fields (`kkt_tol`, `econ_tol`, `icon_tol`, ...) | `common.optimality_tol`, `common.feasibility_tol` (IPM complementarity: `termination.complementarity_tol`, default `common.optimality_tol`) |
| per-engine iteration cap | `common.max_iterations` (IPM: per phase; SQP: subproblems solved) |
| (new) | `common.max_factorizations`, `common.time_limit_seconds`, ending `kWorkLimit` and `kTimeLimit` |
| IPM options as one flat struct | `IpmOptions::linear` (the factor layer's choices), `termination`, `advanced` |
| `SqpSolver(options)` required | `SqpSolver(SqpOptions = {})` |
| (shared) | `common.verbosity`, silent by default on both engines |
| `SqpOptions` fields carrying a `sqp_` or per-engine prefix | the prefix dropped; the struct names the engine |

Every option, with type, default and range, is in the generated
`docs/options-reference.md`; a test fails if a field is added without its
entry. The shared options are spelled once there, with the per-engine defaults
side by side (`max_iterations` is 1000 for the IPM and 100 for the SQP).

**Changes of the Pardiso `iparm` surface** rode this group under the
`IPARM-SURFACE` label; the array is byte-identical for the default and each
non-default linear field.

`SolveBudget` (the per-call budget) is now two `std::optional` fields plus a
stop token: it can only tighten the option caps, never raise them. The stop
token is described in `docs/migration-0.2.md` (section on the stop token).

## 4. Results, warm start, callbacks and trace (B4)

### 4.1 Results

| before | after |
| --- | --- |
| a result shape per engine, `-1` for "absent" in `last_*` fields | one `SolveResult` base: `status`, `message` (one line, why it stopped), `x`, `lambda`, `z`, `g`, `f`, `stationarity`, `feasibility`, `complementarity`, `iterations`, `work`, `wall_seconds`, `warm_start` |
| `feasibility_e`, `feasibility_i` | `feasibility`, the maximum of the two |
| engine counters only | `result.work` (objective, gradient, constraint, Jacobian, Hessian evaluations and factorizations), counted the same way on both engines |
| `kBudgetExhausted` | `kIterationLimit` with `result.returned_best_iterate == true`; the returned point is unchanged |
| `infeasibility_certified` per engine | `result.infeasibility_certified` on the shared result (the SQP sets it when restoration reached a stationary point of the infeasibility measure; the interior-point engine's `kInfeasible` exit is uncertified, see the G-S section of `docs/migration-0.2.md`) |
| `-1.0` / `-1` sentinels | `std::optional` |
| `detail/` types in result fields | public `EvaluationErrors` and the factor layer's `SymmetricFactor::Counters` |

Engine-specific measures live under `result.convergence` (and the IPM's
`counters`, `timing`, `phases`). Later regrouping of `IpmResult` is in
`docs/migration-0.2.md`.

### 4.2 Warm start

| before | after |
| --- | --- |
| `WarmStartData` with public trailing-underscore members | `WarmStart`, a class; build it by taking `result.warm_start` or `WarmStart::seed(problem, x, lambda, z)` |
| `std::optional<WarmStartData>` and `solver.export_warm_start()` | `result.warm_start`, a plain value; an empty one means "no warm start" |
| three ways to get an SQP warm start | one `WarmStart` for either engine, plus the labelled SQP-only native `SqpWarmStart` (opaque; `SqpResult::sqp_warm_start`) |
| `to_sqp_warm_start` taking the transcribed program, kept alive by the caller | `to_sqp_warm_start(warm, problem)` |
| `solve(problem, x0, warm)` guarded by `if (warm)` | `solve(problem, x0, warm)` on every call; pass the previous result's `warm_start` in a loop |

`examples/05_warm_continuation.cpp` and `examples/06_ipm_to_sqp_polish.cpp` are
the reference. Note `SqpWarmStart` declares no default constructor on purpose, so
a braced `{}` third argument selects the `WarmStart` overload.

### 4.3 Callbacks

`set_iteration_callback(callback, payload, scope)` is one shape on both engines.
The callback gets an `IterationEvent` whose vectors are borrowed for the call
only; `snapshot(event)` makes an owning copy. `CallbackScope::kTopLevel` (the
default) delivers the caller's own iterations; `kIncludeRestoration` also
delivers the SQP's restoration sub-solve rows (the old behaviour). Count
difference to expect: the SQP's row 0 is the start point, and the IPM's rows
equal its iterations. `examples/09_callbacks_and_stop.cpp` is the reference.

### 4.4 Trace sinks

`TraceSink` has no pure virtuals: every event handler has a no-op default, so a
sink overrides only what it reads. Sinks are borrowed (`attach_trace(TraceSink *)`):
the caller owns the sink and its stream and keeps both alive across the solve. The
JSON-lines sink is `JsonLinesTraceSink`, header `json_lines_trace_sink.h`
(the header was `drivers/trace_writer.h`). The trace schema moved to v1 later in M8; the
before and after is in `docs/trace-schema-v1.md` section 7.

## 5. Evaluation failure (B5)

**Before.** The interior-point engine absorbed any exception at a trial point; the
SQP propagated all of them.

**After.** Three public exception types: `hven::Error` (base),
`hven::EvaluationError`, `hven::BackendError`. On both engines:

- An `EvaluationError` thrown at a **trial** point, or a NaN or infinity in the
  objective, a constraint or a derivative entry at a trial point, is a rejected
  trial: the engine backs off and continues.
- A throw at the **start** point propagates; a non-finite start ends
  `kNumericalError` with a message naming it.
- **Any other exception type propagates unchanged**, from both engines. **This
  narrows the IPM** (it used to swallow them at seven catch sites). If your model
  throws `std::runtime_error` or a library's own type to say "cannot evaluate
  here", throw `hven::EvaluationError` instead, or the solve will now abort.
- `solver.last_result()` then holds the partial result (`kInterrupted`, the
  iterations taken, the last accepted iterate and multipliers), until the next
  `solve` clears it. After a throw the same solver and `Problem` can solve again.

Audit your model's `eval_*` methods for what they throw: this is the one B-group
change that alters behaviour on a path a consumer can reach.

## 6. Pieces (B6)

**Before.** Building a problem from pieces needed `detail/` includes and a
`SolverInterfaceAdapter` specialization for each piece type.

**After.** A local-block layer, the `AssemblyDeclaration` builder, and a public,
specified raw protocol (`docs/assembly-protocol.md`). The adapter default is
chosen by detection: a plain-type piece compiles with no specialization, and a
type that declares the hooks still dispatches through them.

| before | after |
| --- | --- |
| `#include <hven/detail/...>` for pieces | `#include <hven/hven.h>` |
| `SolverInterfaceAdapter<MyPiece>` specialization | none for a plain piece; the hooks, if you have them, stay |
| `hven/solver_interface_adapter.h` | `hven/model/solver_interface_adapter.h` |
| header names `indexing_data.h`, `sizing_specs.h`, `solver_interface_specs.h` | `solver_indexing_data.h`, `sizable_spec.h`, `piece_interfaces.h` |
| `model/pieces.h` (a gathering header) | deleted; use the umbrella |
| `Problem` from pieces with the SQP | `Problem(AssemblyDeclaration)` is interior-point only; the SQP refuses it by name |

`examples/04_partitioned_chain.cpp` is the reference.

## 7. Absent bounds (B7)

**Before.** The interior-point engine treated every finite bound as real, while
the SQP's QP tier treated a bound of magnitude 1e20 or more as absent (four
separate constants).

**After.** `common.infinite_bound` (default 1e20) decides absence once, at the
`Problem` boundary, for both engines. A bound at or beyond it is absent. Set it
to `hven::kInfinity` to make every finite value a real bound.

This is the one **declared default event** in the break. A model that declares a
finite bound of magnitude 1e20 or more (for example `1e30` meaning "no bound")
used to have it treated as a real bound by the interior-point engine and by the
SQP's own NLP-level tests, while the SQP's QP subproblems already read it as
absent. Now every route reads it as absent. An equality row declared at the
infinite bound is refused by name. Writing `hven::kInfinity` (or
`-hven::kInfinity`) is the clean spelling and is unaffected.

## 8. Header hygiene (B1)

`IpmSolver` and `SqpSolver` sit behind a private implementation. They are
movable (a moved-from solver may only be destroyed or assigned to). No public
header includes `detail/`, `fmt` or a test fixture.

What breaks, if anything:

- **Transitive includes.** Code that included an engine header and silently
  relied on `<fmt/format.h>`, `detail/` headers or Eigen helpers arriving with
  it must include what it uses. `#include <hven/hven.h>` provides the public
  surface.
- **`friend`-based test access** to solver internals moved to `detail/`.
- Public header comments now state contracts only: history and codenames live
  in `docs/notes/`.

## 9. Checklist

1. Run the rename pass (section 1), then fix up by compiler error.
2. Replace your model base with `Nlp` and your solve calls with `Problem`
   (section 2). Re-read your multiplier indexing: `lambda` is now in your own
   row order.
3. Move option assignments under `common.` and the IPM sub-structs
   (section 3). Build once and read `docs/options-reference.md` for the spelling
   of any field you set.
4. Move warm-start and callback code to `WarmStart`, `result.warm_start` and the
   shared `IterationEvent` (section 4).
5. Audit what your model throws at a trial point (section 5).
6. Replace "very large" bounds with `hven::kInfinity` (section 7).
7. Include only `<hven/hven.h>`.
8. Read `docs/migration-0.2.md` for the interface groups that landed after the
   break.
