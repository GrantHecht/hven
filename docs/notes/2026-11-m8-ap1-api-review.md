# M8 AP-1: the API review

Date: 2026-11-01. Status: for owner approval at G-API. Base: `m8` at `12ca92e7`.
Inputs: the EX-1 friction log (`docs/notes/2026-11-m8-ex1-friction.md`, 31 entries); the eight
`examples/` programs; the public headers under `include/hven/`; the M6 break's group pattern
(`docs/migration/2026-09-m6-api-break.md`); the T8 design's §5/§6 registrations
(`docs/notes/2026-09-m6-w5-t8-design.md:288-333`); `docs/trace-schema-v0.md` §3.
Binding rulings: C201 (one declared break, no shims, 0.1.0 → 0.2.0; model-contract unification in
scope iff it fits about a third of the milestone; no migration guide; at most one break window
per milestone; the dense-arm names are set now).

No code changes ride this page. AP-2 starts after G-API.

## 1. Summary

- **The target.** One problem type that both engines solve (`hven::Problem`, built from a
  callback NLP or from pieces). One namespace (`hven`) and one umbrella header
  (`<hven/hven.h>`). Engine classes behind a private implementation, so public headers stop
  including `detail/`, stop naming test fixtures, and the solvers become movable. Shared
  settings spelled once (two tolerances, one iteration cap, one verbosity). Results reported in
  the problem's own row space, with each engine's own convergence measures kept apart from the
  shared diagnostics. One warm-start value from a result.
- **The model-contract size call.** The engine-side unification (T8 §6: the KKT workspace leaves
  `NonLinearProgram`; both engines consume the claim stream; SQP restoration on the common
  contract) **moves to M9.** Estimated cost: 10-11 tasks as specified, 15-16 at this project's
  observed overrun, against about 16 for a third of M8. It also sits on M8's critical path, and
  its specification is not yet written. Its user-visible half is delivered in this break
  (group B2): one problem type reaches both engines through public headers. M9 therefore changes
  internals only, behind a surface that does not move again. §8 gives the reasoning.
- **Friction dispositions.** 26 FIX IN THE BREAK, 3 DOCUMENT ONLY, 2 KEEP (§11).
- **The break.** Eight groups. Seven are functional, in dependency order. The rename sweep comes
  last. Estimated total: 16.5-18.5 tasks (§13). B5, B6 and B7 are separable and can be cut without
  touching the others.
- **Not built.** The serialized SQP warm envelope (§10.1), with a stated reopen trigger. The
  break makes `SqpWarmStart` opaque so the envelope can be added later without a second break.

## 2. The bar and the reference points

The surface is judged against established NLP solver APIs, not only against the friction log:

- **Ipopt.** One callback contract (TNLP: sizes, bounds, values, triplet structures, lower-triangle
  Hessian). Bounds are read on every optimize call. Absent bounds are written as `±1e19` or
  beyond. Callbacks report evaluation failure by returning `false`, and the line search backs off.
  An intermediate callback sees iteration 0.
- **Knitro.** One problem object, with structure declared once and values by callback. Bounds at
  or beyond `±1e20` mean absent. There are separate optimality and feasibility tolerances, and
  limits on iterations, function evaluations and time.
- **SNOPT.** Major optimality and major feasibility tolerances, an infinite-bound threshold
  (`1e20`), and iteration and time limits.

hven already matches or exceeds these on the result core, on counters as the currency, and on
warm-start identity checking. It falls short on the points below. Each short point is a
section of this page.

| point | Ipopt / Knitro / SNOPT | hven today |
|---|---|---|
| problem contracts | one | two public callback contracts with opposite conventions, plus a public 108-member program struct |
| where the problem goes | one solve entry | the IPM takes a program, the SQP takes a model; the routes between them are not all public |
| absent bound | a threshold (1e19 / 1e20 / 1e20) | every finite value is real on the IPM route, but the SSN QP mode cuts at 1e20 |
| evaluation failure | a defined rule | the IPM rejects a trial on an evaluation exception; the SQP propagates it (§4.6) |
| tolerances | 2 (optimality, feasibility) | IPM 12 (four measures, three tiers); SQP 2, under different names |
| work limits | iterations, evaluations, time | iterations only; no factorization or time ceiling (F3) |
| public headers | self-contained | 40 `detail/` includes across 10 public headers; gtest names in one |

## 3. The target surface at a glance

Everything below is in namespace `hven`, reachable from `<hven/hven.h>`.

```cpp
// --- core/types.h gains two names beside Vec, VecRef, ConstVecRef, MatRef. ---
using IndexVecRef = Eigen::Ref<Eigen::Matrix<Index, Eigen::Dynamic, 1>>;
inline constexpr double kInfinity = std::numeric_limits<double>::infinity();

// --- The callback NLP (Ipopt's TNLP shape). Was NlpTripletModel. ---
struct NlpDimensions { Index n = 0, m = 0, jacobian_nonzeros = 0, hessian_nonzeros = 0; };

class Nlp {
  public:
    virtual ~Nlp() = default;
    virtual NlpDimensions dimensions() const = 0;                 // read once per structure
    virtual void bounds(VecRef x_lower, VecRef x_upper,
                        VecRef g_lower, VecRef g_upper) const = 0; // read at EVERY solve
    virtual double eval_f(ConstVecRef x) const = 0;
    virtual void eval_grad_f(ConstVecRef x, VecRef grad) const = 0;
    // Constraint block: the defaults serve m == 0 and throw if m > 0.
    virtual void eval_g(ConstVecRef x, VecRef g) const;
    virtual void jacobian_structure(IndexVecRef rows, IndexVecRef cols) const;
    virtual void eval_jacobian(ConstVecRef x, VecRef values) const;
    // Hessian: optional. has_hessian() == false is legal for quasi-Newton
    // solves; the IPM refuses it by name.
    virtual bool has_hessian() const { return true; }
    virtual void hessian_structure(IndexVecRef rows, IndexVecRef cols) const; // lower triangle
    virtual void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                              VecRef values) const;
    virtual std::string name() const { return "nlp"; }
};
// Evaluation failure: throw hven::EvaluationError at a trial point and both
// engines treat the trial as rejected. Any other exception propagates.

// --- What both engines solve. Owns the engine-side transcriptions. ---
class Problem {
  public:
    explicit Problem(std::shared_ptr<Nlp> nlp);
    explicit Problem(AssemblyDeclaration pieces);   // §9; the SQP refuses it by name until M9
    Index num_variables() const;
    Index num_constraints() const;
    const ProblemStatistics &statistics() const;    // row-kind and bound-kind census (§10.4)
    int partitions() const;                         // the adopted count (pieces)
};

// --- Engines: one shape. IpmSolver shown; SqpSolver is identical. ---
class IpmSolver {
  public:
    explicit IpmSolver(IpmOptions options = {});
    IpmSolver(IpmSolver &&) noexcept;                         // movable (private impl)
    IpmResult solve(Problem &problem, ConstVecRef x0,
                    const WarmStart &warm = {}, const SolveBudget &budget = {});
    void set_options(IpmOptions options);
    const IpmOptions &options() const;
    void set_iteration_callback(IterationCallback cb,
                                IterationPayload payload = IterationPayload::kAll,
                                CallbackScope scope = CallbackScope::kTopLevel);
    void clear_iteration_callback();
    void attach_trace(TraceSink *sink);                       // borrowed
    void attach_ledger(Ledger *ledger, std::string label_prefix);
    void set_kkt_hook(KktHook hook);                          // labelled IPM-only extension
};
// SqpSolver adds one labelled SQP-only entry:
//   SqpResult solve(Problem &, ConstVecRef x0, const SqpWarmStart &, const SolveBudget & = {});

// --- Shared settings, embedded once per engine. ---
struct CommonOptions {
    double optimality_tol = 1e-6;           // stationarity (+ IPM complementarity)
    double feasibility_tol = 1e-6;          // equality and inequality residuals
    Index max_iterations;                   // engine default: IPM 1000 per phase, SQP 100
    Index max_factorizations = 0;           // 0 = none (F3)
    double time_limit_seconds = inf;        // wall clock; never set in measured runs
    Verbosity verbosity;                    // kSilent / kWarnings / kSummary / kIterations
    int threads;                            // as today
    StartLevel start_level = StartLevel::kWarm;
    double infinite_bound = 1e20;           // §10.2, subject to the owner's decision
};

// --- One result core. ---
struct SolveResult {
    SolveStatus status;
    bool returned_best_iterate = false;     // replaces kBudgetExhausted (§5)
    Vec x, lambda, z, g;                    // lambda and g: one entry per problem row
    double f;
    double stationarity, feasibility, complementarity;   // shared, declared space
    Index iterations;
    double wall_seconds;                    // informational
    WarmStart warm_start;                   // empty() when no usable point
};
```

## 4. Settings

### 4.1 Shared settings spelled once (#15, #16, #7, F3)

- **Tolerances.** `CommonOptions::optimality_tol` and `feasibility_tol` replace IPM `kkt_tol`,
  `bar_tol`, `econ_tol` and `icon_tol`, and SQP `kkt_tol` and `feas_tol`. The IPM maps
  `optimality_tol` to its KKT and barrier tests and `feasibility_tol` to its equality and
  inequality tests. A separate complementarity tolerance stays available as an IPM override
  (`IpmOptions::termination.complementarity_tol`, unset by default, meaning "use
  `optimality_tol`"). The IPM's acceptable and divergence tiers keep their four-way split, under
  `IpmOptions::termination`. Every default is 1e-6 today, so no default moves.
- **What a tolerance is measured against** stays each engine's own measure (IPM `kkt_inf`, SQP
  `sqp_stationarity`). KEEP, for this reason: moving either engine's termination test onto the
  shared declared diagnostics changes when solves stop. That is an algorithmic change with TRAIN
  evidence, not an API change. The doc states which measure each tolerance reads.
- **Iteration cap.** One name, `max_iterations`, on `CommonOptions` and on `SolveBudget`. It
  replaces `IpmOptions::max_iters` and `SqpOptions::max_iter`. The defaults stay per engine, and
  the IPM's cap stays per phase, documented as such.
- **Work and time limits (F3).** `CommonOptions::max_factorizations` (0 = none) is a deterministic
  ceiling, read at iteration boundaries on both engines. Its exit is a new `kWorkLimit`.
  `time_limit_seconds` is wall-clock and reports a new `kTimeLimit`. It is never set in a
  replay, census or harness row (CLAUDE.md §7: a deadline outcome is not
  scheduling-invariant). Both default to off, so no trajectory moves. Owner decision 5.
- **Verbosity.** `Verbosity` is an enum with four rungs, not an inverted integer (0 = everything,
  3+ = silent, the opposite of Ipopt and Knitro). The default should be the same on both engines.
  It is silent today on the SQP and full on the IPM. Owner decision 4 (recommend silent on both).
  The banner's stale "InteriorPointSolver" string is fixed.
- **Construction.** `SqpSolver(SqpOptions = {})`, matching `IpmSolver` (#6).

### 4.2 Engine options: tiers and names

- **Two tiers per engine.** The user tier holds documented, stable fields. The `advanced`
  sub-struct holds research levers (the IPQP, SSN and elastic ladders, the restoration
  internals, the IPM's globalization dispatch). Owner decision 6: the `advanced` tier may change
  outside a break window and says so in its header.
- **Redundant prefixes go.** `SqpOptions::ipqp.ipqp_max_iter` becomes `ipqp.max_iterations`, and
  likewise for every `ipqp_*` field of `IpqpOptions`.
- **The IPM's sparse-solver knobs** (`qp_ord`, `qp_pivot_strategy`, `qp_matching`, `qp_scaling`,
  `qp_pivot_perturb`, `qp_ref_steps`, `qp_par_solve`, `qp_print`, `fast_factor_alg`,
  `accel_*`) configure the KKT factorization, not a QP. They move to `IpmOptions::linear`
  (`ordering`, `pivoting`, `matching`, `scaling`, `pivot_perturbation`, `refinement_steps`,
  `parallel_solve`, ...). These fields feed the Pardiso `iparm` array, so the group carries the
  **IPARM-SURFACE** label. Its validation evidence is the `iparm` array, byte-identical before
  and after, from a test that dumps it for the default and for each non-default field.
- **Cryptic abbreviations** get names that describe them: `incr_h`/`decr_h`/`delta_h` become the
  inertia-correction increase, decrease and initial values; `alpha_red`, `max_refac`, `soe_mode`,
  `opt_bar_mode`, `pd_step_strategy` are renamed on the same rule. The full field map is AP-2's
  manifest. Moving a field into a sub-struct is a layout change, so it sits in B3, not in the
  rename group (the T8.10 lesson, `t8-design.md:256-269`).
- **Enums used by options:** singular type names (`BarrierMode`, not `BarrierModes`) and
  `kPascalCase` enumerators everywhere. Today some are `filter`, `wmno`, `off`, `classic` and
  `MakeParameter`. The `k...Count` sentinels leave the public enums.
- **`IpmPhase`** is a public option type defined in `detail/drivers/ipm_solver_fwd.h`. It moves to
  the public options header.

### 4.3 KEEP in settings

- Options are values, validated by `validate()`, with string presets (`ipm_preset`,
  `sqp_preset`), `ipm_worker_options`, and `set_options`. This is the M6 shape; nothing in EX-1
  argues against it.
- String parsing of options stays with the bindings (T8 §5).
- `common.threads` keeps its two defaults and its call-scope semantics. Promoting the backend
  knobs or CNR to `CommonOptions` still needs a process-level owner (T8 §5).
- `dump_options` keeps its role. Its doubles print at shortest round-trip (`1e-06`, not
  `9.9999999999999995e-07`) on both engines (#30). AP-4's registry regenerates the dump on top
  of that.

### 4.4 The dense arm (Q4)

Named now. Nothing is declared until DN-1 builds, and nothing lands if DN-0 does not fire.

| layer | name | type, default |
|---|---|---|
| factor | `hven::linear::SymmetricFactor::Options::dense_factorization` | `hven::linear::DensePolicy`, `kNever` |
| factor, kAuto only (DN-3) | `SymmetricFactor::Options::dense_max_dimension` | `Index`, set by DN-3's rule |
| public option | `CommonOptions::dense_factorization` | `DensePolicy`, `kNever` |
| enum | `hven::linear::DensePolicy` | `kNever`, `kAlways`, `kAuto` (kAuto declared with DN-3) |
| counter | `SymmetricFactor::Counters::dense_factorizations` | `Index` |

One concept, one field name, at both layers. If DN-1 fires for one engine only, the public field
goes on that engine's options under the same name, and moves to `CommonOptions` when the second
engine follows. That move is additive in a later window.

### 4.5 Bounds: absent-bound normalization (F10, #13)

See §10.2. The setting is `CommonOptions::infinite_bound`.

### 4.6 Evaluation failure

The IPM converts an evaluation exception at a trial point into a rejected trial
(`detail/interior/eval_error_log.h`). The SQP solve loop has no equivalent catch: an exception
from an SQP evaluation propagates out of `solve`. Neither rule is in a public header.

Target: `hven::EvaluationError` is the one signal. Thrown at a trial point, both engines reject
the trial (the IPM backs off its line search; the SQP rejects the step and shrinks its radius).
Thrown at the start point, or any other exception type, it propagates. This changes behaviour
only on throw paths, which no corpus cell takes. It is a group of its own (B5), declared as a
behaviour change.

## 5. Results and diagnostics

- **The problem's own row space.** For an `Nlp` problem, `lambda` and `g` have one entry per
  declared row, in Ipopt's sign convention (`L = f + λᵀg`). The internal equality/inequality split
  and the range-row doubling stay inside the engine. For a pieces problem the row space is the
  declaration's own (`[equality rows | inequality rows]`, `c_I <= 0`). `lambda_e`, `lambda_i`,
  `ce` and `ci` leave `SolveResult`. Today a triplet user reads `lambda_e[0]` for a row they
  wrote as `g_lower == g_upper`, and reads a lower-bounded row as a sign-flipped `lambda_i`.
  `compose_user_multipliers` (`model/nlp_problem_model.h`) already holds the map.
- **Shared diagnostics.** `stationarity`, `feasibility`, `complementarity`. `feasibility` is the
  maximum of today's `feasibility_e` and `feasibility_i`, a derived value with no new
  evaluation. Definitions are unchanged, and NaN still means unmeasured.
- **Engine measures under one name (#26).** `IpmResult::convergence {kkt, barrier, equality,
  inequality}` (today `kkt_inf`, `barr_inf`, `econ_inf`, `icon_inf`) and `SqpResult::convergence
  {stationarity, feasibility, complementarity, kkt_residual}` (today `sqp_*`, `kkt_residual`).
  The field a tolerance reads is named in the tolerance's doc.
- **Absent is optional, not -1.** `IpmResult`'s `last_funnel_width = -1.0`, `last_filter_size =
  -1` and the seven other `last_*` sentinels become `std::optional`. The IPM timing fields move
  under `IpmResult::timing`.
- **No `detail/` types in results.** `IpmResult::kkt_factor_counters`
  (`KktFactorization::Counters`) and `eval_error_log` (`EvalErrorLog`) become public types: the
  factor layer's public `SymmetricFactor::Counters`, and a public `EvaluationErrors {count,
  last_message}`.
- **Status vocabulary.** It says why the solve stopped; which point came back is a separate field:
  - `kBudgetExhausted` retires. A `budget_mode` exhaustion reports `kMaxIter` with
    `returned_best_iterate = true`, the same flag the IPM's `return_best` sets. The returned
    point is unchanged; only the label moves. `budget_mode` is off by default and in every
    corpus cell.
  - Add `kWorkLimit` (factorization ceiling; the SQP minor budget when exhausted) and, if owner
    decision 5 approves, `kTimeLimit`.
  - `kInfeasible` keeps its name. Its doc says "locally", and `infeasibility_certified` stays the
    certificate flag.
- **Callbacks (#27, #28).** `CallbackScope::kTopLevel`, the default, delivers depth-0 rows only.
  `kIncludeRestoration` delivers today's stream. The count difference in #27 is documented: the
  SQP's row 0 is the start point, and the IPM's rows equal its iterations.
- **Internal functions leave the public surface.** `compute_declared_diagnostics`,
  `compute_declared_diagnostics_from_grad_lag` and `resolve_ipm_phase_status` are engine
  internals with no consumer outside the library. They move to `detail/`.
- **KEEP: the shared result is a non-polymorphic base, sliced by value.** The M6 shape fits:
  one function written against `SolveResult` serves both engines.
- **KEEP: IPM bound multipliers carry interior noise (#31).** Zeroing them would report a value
  the engine did not compute. Documented.

## 6. Warm start and composition

- **One value from a result (#24).** `SolveResult::warm_start` is a field of type `WarmStart` (was
  `WarmStartData`; `std::optional` and the `export_warm_start()` accessor go). A default-built
  `WarmStart` is empty and means "no warm start", so `solve(p, x0, warm)` takes it on every
  call, as in a loop. `SqpSolver::export_warm_start()` is removed (it duplicates the result).
  `SqpResult::warm_start` (native) becomes `SqpResult::native_warm_start`, labelled SQP-only.
- **`WarmStart` is a class.** Its fields are private (today `primal_`, `eq_lmults_` and so on are
  public trailing-underscore members of an aggregate). It is built by an engine, or by
  `WarmStart::seed(problem, x, lambda, z)` in the problem's row space. A seed with an empty `x`
  is the multipliers-only form. `serialize` and `deserialize` are kept. The identity rule (an
  identity mismatch refuses; pattern and value defects degrade) is unchanged.
- **`SqpWarmStart` is opaque.** Today it holds a `WorkingSet` by value through
  `detail/qp/working_set.h`. It becomes a handle. It is not default-constructible, so a braced
  `{}` third argument selects `WarmStart` without ambiguity, and the native state can grow a
  serialized form later without a break (§10.1). It is built by `to_sqp_warm_start`, by mesh
  transfer and continuation, or taken from a result.
- **The polish composition (#25).** `to_sqp_warm_start(const WarmStart &, const Problem &)` takes
  the problem: the bounds and the declaration key come from it, so the caller no longer keeps
  the transcribed program alive. The two routes (staged and payload) stay. They are different
  tools: the staged one lets a caller inspect or edit the crossover's active set. The docs say
  both reach at most `kSeeded`. The "-1e20 convention" sentence in the staging function's doc
  is replaced by §10.2's rule.
- **`starting_multipliers` leaves the callback contract.** The multipliers-only `WarmStart`
  replaces it: one seed route, not two.
- **KEEP: the native route.** `solve(problem, x0, const SqpWarmStart &)` stays the labelled
  SQP-only entry. Without it `kWarm` and `kHot` are unreachable (T8 §2.4), and continuation and
  mesh transfer build native states for structures a payload cannot describe.
- **KEEP: borrowed trace sinks (#29).** `attach_trace(TraceSink *)` borrows. The caller owns the
  sink and usually its stream. Shared ownership would force a heap allocation per sink and
  hide when an output closes. The lifetime rule is stated on both attach calls. A public reader
  for JSON-lines traces is tooling and goes to TR-1.

## 7. Naming: namespace, header layout, umbrella, spelling

- **Namespace (#9).** `hven::solvers` becomes `hven`, as CLAUDE.md §1 states. `hven::linear` stays
  the public factor layer. Internals live in `hven::detail`.
- **Umbrella (#8).** `<hven/hven.h>` includes the whole stable surface. Every example includes
  that one header.
- **Public headers do not include `detail/` (#10, #12).** Today 10 public headers carry 40 such
  includes (`sqp_solver.h` 14, `ipm_solver.h` 7, `non_linear_program.h` 7, and others), and
  `ipm_solver.h` forward-declares 17 gtest names and befriends them. The engines move to a
  private implementation (B1). The fixtures befriend the implementation class in a `detail/`
  header. `fmt` leaves the public headers. `check_export_contract.sh` gains the rule "no public
  header includes `detail/`". Any survivor is named and argued on B1's ledger line.
- **Header comments are contract text.** Public header prose today carries settlement history
  (task ids, percentages, review outcomes; for example `drivers/common_options.h`, and
  `drivers/solve_result.h`'s `IterationPayload` note). B1 rewrites the public headers to state the
  contract only. The history stays in `docs/notes/`.
- **Header layout: KEEP the directories** (`core/`, `linear/`, `model/`, `warmstart/`, `drivers/`).
  Reason: with an umbrella, paths are secondary, and CLAUDE.md §2 maps the tree. Renaming
  `drivers/` to `solvers/` would cost a sweep and buy nothing a consumer can use.
  `solver_interface_adapter.h` moves from the include root into `model/`.
- **Spelling.**
  - Acronyms are written as words: `Nlp`, `Qp`, `Kkt`, `Ipm`, `Sqp`. Today `NLPRowClassification`,
    `NLPAdapterCore` and `QPAlgModes` sit beside `NlpModel` and `QpMode`.
  - Aggregate members are bare `snake_case` (CLAUDE.md §4). Today `AssemblyDeclaration`,
    `VariableBound`, `WarmExtension`, `IpmPolishData` and `NLPRowClassification` use trailing
    underscores.
  - Sizes and counts are `hven::Index` on every public signature. The triplet contract's `int`
    counts and `VectorXi` structures go.
  - Callback signatures use `hven::ConstVecRef`, `VecRef` and `IndexVecRef` from `core/types.h`,
    not `ConstEigenRef` from `detail/`.
- **Version.** `0.2.0`, with a CHANGELOG entry at the break's close.

## 8. The problem contract and the model-contract unification

### 8.1 Today

- Three problem forms are public:
  - `NlpTripletModel`: TNLP shape, lower-triangle triplets, `int` counts.
  - `NlpModel`: split form, `c_E = 0`, `c_I <= 0`, upper-triangle sparse matrices returned by
    value, six of its pure virtuals `[[deprecated]]`. `ParametricNlpModel` sits on top of it.
  - `NonLinearProgram`: a struct with 108 public members. It holds the IPM's KKT workspace, built
    by `make_nlp_program` or `adopt_declaration`.
- Three bridges: `NlpProblemModel` (triplet → `NlpModel`), `NlpModelAssembly` (`NlpModel` →
  claim stream), and `NLPAdapterCore` (`NlpModel` → program; constructible only in `detail/`).
- The IPM takes `NonLinearProgram &`; the SQP takes `const NlpModel &` or `NlpModelAssembly &`.

### 8.2 Target (group B2)

- **One public callback contract, `hven::Nlp`** (§3). `NlpModel`, `ParametricNlpModel`,
  `NonLinearProgram`, `NlpProblemModel`, `NlpModelAssembly` and `make_nlp_program` move to
  `detail/`. `NlpModel` stays the SQP's internal form. Owner decision 2: hven-bench rewrites its
  `NlpModel` adapter to `Nlp`.
- **One engine-facing type, `hven::Problem`.** It owns the engine-side transcriptions, built
  lazily on first solve by each engine. The IPM's form is today's `make_nlp_program` path; the
  SQP's is today's `NlpProblemModel` path. The internal routes are unchanged, so the corpus
  identity holds.
- **Values may change between solves; structure may not (#23).** At each solve entry:
  - the engine re-reads `bounds()` and clears its per-iterate caches;
  - an unchanged bound set is a no-op, so counters stay identical;
  - a changed set re-lays bounds, and a change in which variables are fixed re-lays the
    elimination (counted);
  - `dimensions()` and both structures are re-queried, and a change is refused by name. The cost
    is O(nnz) per solve, against at least one factorization.

  This gives the IPM the parametric route it lacks today (#23). It retires `ParametricNlpModel`:
  a parameter is the user's own member, and the continuation driver (`detail/warmstart/`) reads
  `Nlp` through the same handle.
- **Convention fixes:** one triangle (lower) and one multiplier vector (#2); no deprecated
  overrides (#3); an optional Hessian (#4); zero-row blocks need no overrides (#5); public type
  aliases (#10).
- **Concurrency rule, stated:** one solve at a time per `Problem`. Two engines may alternate on
  one `Problem`. An IPM alternation re-analyses, as `analyzed_owner_id` does today, and is
  counted.
- **Pieces** (`Problem(AssemblyDeclaration)`): §9. The SQP refuses a pieces problem by name until
  M9 (owner decision 8).
- Tests and bench keep building `NonLinearProgram` and `NlpModel` directly through `detail/`
  entries (`detail::problem_from_program`, `detail::problem_from_model`). The 27-cell replay and
  census binaries take the same internal route they take today.

### 8.3 The engine-side unification (T8 §6): the size call

What T8 §6 specifies, with my estimate in tasks (one task = one implementer task and its review
round, the size of an M7 W-task):

| item (T8 §6) | estimate |
|---|---|
| specification of the eight KKT-workspace couplings, the cache-key rules and the ordering constraint (not yet written) | 1-1.5 |
| `NonLinearProgram` derives `ClaimStreamSource` (signatures already match) | 0.25 |
| SQP-shape support in `assemble_impl`, consumer-owned binding with accumulation, duplicate coordinates in `AggregateEvalSeam` | 2 |
| the IPM's KKT workspace leaves the program: solver coefficients, shared diagonals, solver-only blocks, RHS fills, claim numbering, reduction maps; IPM replay byte-exact | 3-4 |
| fixed-variable treatment leaves the model (ordering constraint); retire the drop path | 1 |
| SQP restoration and the elastic seed on the common contract | 1.5 |
| workspace cache key | 0.5-1 |
| proof matrix (both engines × both providers, overlap, restoration, treatments, alternation) | 1.5 |
| **total** | **10-11 as specified; 15-16 at observed overrun** |

"Observed overrun" is the M6 W5 T8 record: group 1 was estimated at two T6s and ran nine
sub-tasks plus fix rounds.

M8's size, by the same unit: the brief names 31 tasks outside AP-2 (EX 3, AP 3, DN 4 of which 3
conditional, PF 3, HB 5, TR 3, TU 4, CP 3, DC 1, WX about 2). The break (§13) is 16.5-18.5. M8 is
therefore about 48-50 tasks, and a third is about 16.

**Call: the engine-side unification does not fit, and moves to M9.** The reasons:

1. At the realistic estimate it reaches or passes the bound on its own, and it would land in the
   same window as a 16.5-18.5-task break.
2. It is on the critical path. TR, AP-4, TU and CP all wait for AP-2 to close, and the brief
   does not allow it to trail as a late group.
3. Its first deliverable, the coupling specification, does not exist yet. The astra review
   described the work as "an M4-contract completion" (T8 §0 item 3).
4. Its user-visible half lands now. B2 puts one problem type on both engines through public
   headers. The bridges stay, internal. What M9 adds is the SQP on pieces problems and a solver
   that owns its workspace. That work is behind `Problem`'s face, so M9 needs no second break.
   Its first consumer is tycho, which is out of sync and catches up later.

The brief's interim route ("the harness bridge documented in AP-3's example 1") is superseded:
B2 makes the route public library code, and hven-bench's bridge retires.

## 9. Pieces (the partitioned route)

The piece protocol (#11, #18-#22) is today's only route for tycho's shape. Example 4 needed
three `detail/` includes and about 250 lines of hand-written claim and scatter code.

Target (group B6):

- **A local-block piece layer, public and the default route.** A piece states `kInputs` and
  `kOutputs`, and evaluates its value, its dense local Jacobian (`outputs × inputs`) and its
  dense local Lagrangian Hessian (`inputs × inputs`, lower triangle read) into buffers the
  library provides. An optional local sparsity pattern trims the blocks. The library writes the
  claims, the scatter, the lock protocol, the elimination of fixed variables (`-1` slots) and
  the partitioning. This is one generic adapter over today's raw protocol: the same slots
  written with the same arithmetic per entry. The raw protocol is untouched underneath.
- **The raw protocol stays public as the expert route,** with a written specification
  (`docs/assembly-protocol.md`). It covers what `inner_*_starts_` index, claim slots versus
  `c_loc`, Jacobian claim coordinates, eliminated slots, which flag guards which lock, the
  additivity of `num_kkt_elements`, and contiguity. tycho's existing pieces implement it. The
  public types it needs (`ObjectiveFunction`, `ConstraintFunction`, the interfaces,
  `SolverIndexingData`, `kkt_canonical_lock_col`) move out of `detail/`.
- **Objective pieces carry no constraint surface (#19).** The type-erased objective handle
  forwards only the objective members.
- **No adapter specialization for plain types (#20).** `SolverInterfaceAdapter<T>` defaults to
  direct storage. A type exposing the `pack_into_*_interface` hook is detected and forwarded.
  The single-dispatch rule is unchanged. A handle without a hook is still stored whole, which
  was already the outcome, and is now stated in the doc instead of enforced by a
  `static_assert` that every plain type has to placate.
- **The partition count is given in one place (#21).** `AssemblyDeclaration::set_partitions(k)`.
  `NonLinearProgram(int)` is internal. `Problem::partitions()` reports the adopted count, and
  the clamp rule is documented beside it.
- **One population route (#22).** The master-list `make_nlp(PV, EQ, IQ)` route goes internal
  with `NonLinearProgram`.
- **`AssemblyDeclaration` gains a builder** (`add_objective`, `add_equality`, `add_inequality`,
  `set_bounds`). Row counts are derived and checked, not hand-set.
- Owner decision 7: the local-block layer costs about 2 tasks. The alternative is to document
  the raw protocol only, at about 0.5.

## 10. The other in-scope items

### 10.1 The lossless serialized SQP warm envelope (T8 §5, `hven.sqp.state.v1`): NOT BUILT

- **Consumer.** None crosses a process with SQP state today. In-process, `kWarm` and `kHot` are
  reachable through the native route, and continuation and mesh transfer use it.
- **Evidence.** The case for it would be a measured gap between payload-route `kSeeded` starts and
  native `kWarm` starts on a sequenced workload. HB-5's chain lane is that reader, and M7-29
  (warm 24 against cold 5) already shows that a warmer start is not uniformly cheaper.
- **What the break does for it.** `SqpWarmStart` becomes opaque (§6), so a serialized form can be
  added as a `WarmStart` extension later, additively.
- **Reopen trigger.** A consumer needs a cross-process SQP resume, or HB-5's rows show the
  payload route losing to the native route on the chain lane. One related design is registered
  with the same reader and the same trigger: a `WarmStart` that carries the SQP's in-memory state
  as a non-serialized attachment, so a result's one warm-start value reaches `kWarm` on the SQP.
  It changes trajectories for callers who pass an SQP result's payload today, so it is a
  declared behaviour event with evidence and does not belong in a rename window.

### 10.2 Absent-bound normalization (F10, #13)

- **Today.** Every finite bound is real on the IPM and kWalk routes. The SSN QP mode treats
  `|b| >= 1e20` as absent. Two of two external model sources (hven-bench F9/F10) write absent as
  `±1e20`. EX-1 found `1e30` accepted silently as a real bound.
- **Target, recommended (owner decision 3).** `CommonOptions::infinite_bound = 1e20`.
  - A bound with `|b| >= infinite_bound` is absent. The rule is applied once, at the `Problem`
    boundary, for both engines and every route. The SSN's private cutoff retires into it.
  - Setting `infinite_bound = inf` restores "every finite value is real".
  - Normalized bounds are counted and reported in the solve-begin trace event.
  - An equality row declared at `±infinite_bound` is refused by name, as an equality at infinity
    is today.
- **This is a default change.** It is its own declared default event (B7), with TRAIN evidence:
  the cells whose declared bounds include `|b| >= 1e20`, counted in the declaration before the
  first leg. Census, because the SSN cutoff moves. It does not ride a rename.
- **Alternatives:** refuse `|b| >= 1e20` finite bounds with an error naming the variable or row;
  or keep "verbatim" and remove the SSN cutoff, so the one inconsistency inside hven goes away
  either way.

### 10.3 The trace-sink balance

- **Counts.** `docs/trace-schema-v0.md` §3 lists 20 events: 9 `ipm.*` and 11 on the SQP side
  (`sqp.*` 3, `qp.mode`, `ipqp.*` 6, `fallback.verdict`). The brief's "12" is off by one against
  that table.
- **The API question is the interface shape, and it is fixed in the break (B4).** `TraceSink` has
  20 virtuals, and the 8 SQP-tier ones are pure. A sink written to watch the IPM must implement
  eight SQP callbacks, and every new event is a break for every implementer. Target: every
  `on_*` has a no-op default, and new events are additive.
- **The content balance goes to TR-1, decided by the reader, not by the count.** The candidates
  are the IPM decisions the SQP side exposes and the IPM does not: inertia-correction and
  regularization moves (the `ipqp.reg` analogue), restoration entry (the IPM has only the
  locally-infeasible exit row), and line-search or filter acceptance decisions. TR-1 checks each
  against `ipm.iter`'s existing fields before adding an event.

### 10.4 M7-18: the row-kind census, as a model-contract question

- **The question.** Can the engines derive the five row kinds (equality, upper-bounded,
  lower-bounded, range, free)? On today's SQP contract they cannot (`NlpModel` is the split form;
  `trace-schema-v0.md:129`).
- **After B2, yes, on every public route.** `Nlp` rows carry two-sided bounds, and the
  classification already exists (`NLPRowClassification`). Pieces rows are equality or
  inequality by declaration, so a range row cannot occur.
- **Delivery.** `Problem::statistics()` returns the row-kind counts and the five-way variable-bound
  census, public, as Ipopt prints them. Emitting them on `*.solve.begin` is a trace-content
  change and goes to TR-1.
- **Linearity** (which rows are linear) is declared by no contract. It is registered. An optional
  `Nlp::linear_rows()` hook would be additive later and needs no break.

### 10.5 F3 (the whole-solve factorization ceiling)

§4.1: `max_factorizations`, deterministic, reporting `kWorkLimit`.

## 11. Friction dispositions

| # | entry | disposition | where |
|---|---|---|---|
| 1 | two model types, one problem | FIX IN THE BREAK: `hven::Problem` solved by both engines | B2 |
| 2 | the two contracts disagree on conventions | FIX IN THE BREAK: one public callback contract (`Nlp`) | B2 |
| 3 | `NlpModel` forces six deprecated overrides | FIX IN THE BREAK: `NlpModel` leaves the public surface | B2 |
| 4 | QN mode still demands a Hessian | FIX IN THE BREAK: `has_hessian()`; the IPM refuses a Hessian-less problem by name | B2 |
| 5 | zero-row blocks need empty overrides | FIX IN THE BREAK: defaults serve `m == 0` | B2 |
| 6 | asymmetric solver construction | FIX IN THE BREAK: `SqpSolver(SqpOptions = {})` | B3 |
| 7 | defaults differ; IPM prints by default; stale banner | FIX IN THE BREAK: `Verbosity` enum, one default (owner decision 4), banner fixed | B3 |
| 8 | no umbrella header | FIX IN THE BREAK: `<hven/hven.h>` | B8 |
| 9 | `hven::solvers` with strays in `hven` | FIX IN THE BREAK: namespace `hven` | B8 |
| 10 | a public virtual's signature from `detail/` | FIX IN THE BREAK: `ConstVecRef`/`VecRef`/`IndexVecRef` from `core/types.h` | B2 |
| 11 | pieces need three `detail/` includes | FIX IN THE BREAK: piece types public; local-block layer | B6 |
| 12 | test names in the engine header | FIX IN THE BREAK: private implementation; fixtures befriend it in `detail/` | B1 |
| 13 | `±1e30` accepted as a real bound | FIX IN THE BREAK: `infinite_bound` (owner decision 3); declared default event | B7 |
| 14 | multiplier signs must be read, then verified | DOCUMENT ONLY: the conventions are correct; DC-1's getting-started page states them, and B2's per-row `lambda` puts the `Nlp` route on Ipopt's sign | DC-1 |
| 15 | tolerances spelled differently | FIX IN THE BREAK: `optimality_tol`, `feasibility_tol` | B3 |
| 16 | three spellings of one iteration cap | FIX IN THE BREAK: `max_iterations` everywhere | B3 |
| 17 | `std::string` piece corrupts the heap | KEEP: already fixed by DF-1 (`12ca92e7`, `TypeStorage` relocates by move construction). Nothing is left for the break, and B6's piece docs state that any movable type is legal | — |
| 18 | the piece protocol has no public specification | FIX IN THE BREAK: local-block layer as the default; the raw protocol specified | B6 |
| 19 | an objective piece carries the constraint surface | FIX IN THE BREAK: the objective handle forwards objective members only | B6 |
| 20 | every piece type needs an adapter specialization | FIX IN THE BREAK: direct storage by default; hook detected | B6 |
| 21 | partition count clamped silently, stated twice | FIX IN THE BREAK: one place; `Problem::partitions()`; clamp documented | B6 |
| 22 | two ways to populate a program | FIX IN THE BREAK: the master-list route goes internal | B2/B6 |
| 23 | parameter changed in a live model: wrong answer, optimal status | FIX IN THE BREAK: bounds re-read and caches cleared at every solve; structure changes refused | B2 |
| 24 | three places to get an SQP warm start | FIX IN THE BREAK: `result.warm_start` (one value); native labelled; solver export removed | B4 |
| 25 | polish: two routes, five headers, conflicting doc | FIX IN THE BREAK: `to_sqp_warm_start(warm, problem)`; doc conflict removed; both routes kept and documented | B4 |
| 26 | two stationarity numbers per result | FIX IN THE BREAK: engine measures under `convergence` | B4 |
| 27 | `iterations` and callback count disagree by engine | DOCUMENT ONLY: the SQP's row 0 is the start point; the IPM's rows equal its iterations | B4 docs |
| 28 | depth filter is the caller's job | FIX IN THE BREAK: `CallbackScope::kTopLevel` by default | B4 |
| 29 | raw non-owning sinks; no trace reader | KEEP: the caller owns the sink and its stream (§6); a reader is TR-1 tooling | — |
| 30 | `dump_options` at 17 digits, two formats | FIX IN THE BREAK: shortest round-trip on both; AP-4 regenerates on top | B3 |
| 31 | IPM bound multipliers carry noise | DOCUMENT ONLY: interior-method behaviour; zeroing would report a value not computed | B4 docs |

Counts: **FIX IN THE BREAK 26, DOCUMENT ONLY 3, KEEP 2.** If owner decision 3 chooses "verbatim",
#13 becomes DOCUMENT ONLY (with the SSN cutoff removed in B7).

## 12. The examples, rewritten to the target

Sketches, not compiled. The HS071 callbacks are shown once (example 1). The others elide what
does not change.

### Example 1: HS071 on both engines

```cpp
#include <hven/hven.h>

struct Hs071 : hven::Nlp {
    hven::NlpDimensions dimensions() const override {
        return {.n = 4, .m = 2, .jacobian_nonzeros = 8, .hessian_nonzeros = 10};
    }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        xl.setConstant(1.0);
        xu.setConstant(5.0);
        gl << 25.0, 40.0;
        gu << hven::kInfinity, 40.0;
    }
    double eval_f(hven::ConstVecRef x) const override { return x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2]; }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override { /* as today */ }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override { /* as today */ }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override { /* as today */ }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override { /* as today */ }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override { /* lower */ }
    void eval_hessian(hven::ConstVecRef x, double obj_factor, hven::ConstVecRef lambda,
                      hven::VecRef v) const override { /* as today */ }
};

int main() {
    hven::Problem problem(std::make_shared<Hs071>());
    const hven::Vec x0{{1.0, 5.0, 5.0, 1.0}};

    const hven::IpmResult a = hven::IpmSolver{}.solve(problem, x0);   // silent by default
    const hven::SqpResult b = hven::SqpSolver{}.solve(problem, x0);
    return report("IpmSolver", a) && report("SqpSolver", b) ? 0 : 1;  // report() takes SolveResult
}
```

Removed: two model types, `make_nlp_program`, three includes, the IPM print override, and the
`SqpOptions{}` construction.

### Example 2: bound-constrained Rosenbrock

```cpp
struct BoundedRosenbrock : hven::Nlp {
    hven::NlpDimensions dimensions() const override { return {.n = 2, .hessian_nonzeros = 3}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef, hven::VecRef) const override {
        xl << -2.0, -hven::kInfinity;
        xu << 0.5, hven::kInfinity;
    }
    double eval_f(hven::ConstVecRef x) const override { /* as today */ }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override { /* as today */ }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override { /* as today */ }
    void eval_hessian(hven::ConstVecRef x, double s, hven::ConstVecRef, hven::VecRef v) const override;
    // No eval_g / jacobian_structure / eval_jacobian: m == 0 needs none.
};
// ... solve on both engines as in example 1; check r.z[0] < 0 (active upper bound).
```

### Example 3: equality-constrained, reading the multiplier

```cpp
// Sphere : hven::Nlp, one row with g_lower == g_upper == 4.
const hven::SqpResult r = hven::SqpSolver{}.solve(problem, x0);
const bool ok = r.status == hven::SolveStatus::kOptimal &&
                std::abs(r.lambda[0] - lambda_star) < 1e-5 &&    // per declared row, Ipopt sign
                r.feasibility < 1e-6;
```

### Example 4: a partitioned chain built from pieces (local blocks)

```cpp
// y_{i+1} - y_i - h*(u_i - y_i^2), inputs (y_i, y_{i+1}, u_i), one row.
struct Defect {
    double h;
    static constexpr int kInputs = 3, kOutputs = 1;
    void value(hven::ConstVecRef v, hven::VecRef out) const {
        out[0] = v[1] - v[0] - h * (v[2] - v[0] * v[0]);
    }
    void jacobian(hven::ConstVecRef v, hven::MatRef J) const { J << -1.0 + 2.0 * h * v[0], 1.0, -h; }
    void hessian(hven::ConstVecRef, hven::ConstVecRef lambda, hven::MatRef H) const {
        H.setZero();
        H(0, 0) = 2.0 * h * lambda[0];                      // lower triangle read
    }
};
// weight * (v - ref)^2 on one variable.
struct Quadratic {
    double weight, ref;
    static constexpr int kInputs = 1;
    double value(hven::ConstVecRef v) const { return weight * (v[0] - ref) * (v[0] - ref); }
    void gradient(hven::ConstVecRef v, hven::VecRef g) const { g[0] = 2.0 * weight * (v[0] - ref); }
    void hessian(hven::ConstVecRef, hven::MatRef H) const { H(0, 0) = 2.0 * weight; }
};
struct Boundary { /* two rows: y_0 - a, y_N - b */ };

hven::Problem make_chain(int steps, int partitions) {
    hven::AssemblyDeclaration d(/*variables=*/2 * steps + 1);
    d.add_objective(Quadratic{h, 0.0}, u_idx);               // index maps as today
    d.add_objective(Quadratic{h, 0.3}, y_idx);
    d.add_equality(Defect{h}, defect_vars, defect_rows);
    d.add_equality(Boundary{0.0, 0.5}, boundary_vars, boundary_rows);
    d.set_bounds(u_first, u_last, -2.0, 2.0);
    d.set_partitions(partitions);
    return hven::Problem(std::move(d));
}

hven::Problem chain = make_chain(500, 4);
const hven::IpmResult r = hven::IpmSolver{}.solve(chain, x0);
std::printf("adopted %d partitions\n", chain.partitions());
```

Removed: three `detail/` includes, `NoConstraintSurface`, `kkt_add` and the lock protocol, three
adapter specializations, `get_kkt_space`/`num_kkt_elements`, and the two partition counts. About
250 lines become about 60. The raw-protocol version of this example stays in the tree as the
expert-route test.

### Example 5: warm continuation along a parameter, in place

```cpp
struct Disc : hven::Nlp {
    double p = 0.0;                                         // the user's own member
    // bounds(): gu << 1.0 + 0.5 * p;   eval_f/eval_grad_f use p
};

auto disc = std::make_shared<Disc>();
hven::Problem problem(disc);
hven::IpmSolver ipm;
hven::WarmStart warm;                                       // empty: the first solve is cold
for (double p : {0.2, 0.4, 0.6, 0.8, 1.0}) {
    disc->p = p;                                            // legal between solves
    const hven::IpmResult r = ipm.solve(problem, x0, warm);
    check(r, exact_objective(p));
    warm = r.warm_start;
}
// The SQP sweep is the same loop with hven::SqpSolver.
```

Removed: the rebuild per `p`, the `std::optional` plumbing, and the two-branch solve call. The
in-place mutation that failed silently in EX-1 is now the documented route.

### Example 6: IPM to SQP polish

```cpp
hven::Problem problem(std::make_shared<Hs071>());

hven::IpmOptions rough;
rough.common.optimality_tol = rough.common.feasibility_tol = 1e-3;
const hven::IpmResult r = hven::IpmSolver(rough).solve(problem, x0);

hven::SqpOptions tight;
tight.common.optimality_tol = tight.common.feasibility_tol = 1e-10;
hven::SqpSolver sqp(tight);

const hven::SqpResult direct = sqp.solve(problem, r.x, r.warm_start);    // crossover inside
const hven::SqpResult staged =
    sqp.solve(problem, r.x, hven::to_sqp_warm_start(r.warm_start, problem));  // inspectable crossover
const hven::SqpResult cold = sqp.solve(problem, x0);
// staged.counters.start_level_used == hven::StartLevel::kSeeded on both routes.
```

Removed: four tolerance assignments down to one line per engine, and the declaration key, the
kept-alive program and the model's bounds as crossover arguments.

### Example 7: SQP in quasi-Newton mode

```cpp
struct Ball : hven::Nlp {
    hven::NlpDimensions dimensions() const override { return {.n = 4, .m = 1, .jacobian_nonzeros = 4}; }
    bool has_hessian() const override { return false; }    // first derivatives only
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        xl.setConstant(-hven::kInfinity);
        xu.setConstant(hven::kInfinity);
        gl << -hven::kInfinity;
        gu << 4.0;                                          // |x|^2 <= 4
    }
    // eval_f, eval_grad_f, eval_g, jacobian_structure, eval_jacobian
};

hven::Problem problem(std::make_shared<Ball>());
hven::SqpOptions qn;
qn.hessian_mode = hven::SqpHessianMode::kLbfgsDamped;
qn.qn_memory = 10;
const hven::SqpResult r = hven::SqpSolver(qn).solve(problem, x0);
// hven::IpmSolver{}.solve(problem, x0) throws: "problem declares no Hessian".
```

Removed: the second contract, the deprecated-suppression bracket, the never-called Hessian, and
the empty equality block. The exact-Hessian comparison solve becomes a second `Ball` with
`has_hessian() == true`.

### Example 8: results, callbacks, trace, options

```cpp
hven::IpmOptions ipm_options;
hven::dump_options(ipm_options, std::cout);                 // kkt tolerance prints as 1e-06
hven::IpmSolver ipm(ipm_options);

std::ostringstream ipm_trace;
hven::JsonLinesTraceSink ipm_sink(ipm_trace);               // borrowed: outlives the solves
ipm.attach_trace(&ipm_sink);
int seen = 0;
ipm.set_iteration_callback([&](const hven::IterationEvent &) { ++seen; return hven::CallbackAction::kContinue; },
                           hven::IterationPayload::kScalars);

const hven::IpmResult a = ipm.solve(problem, x0);
print_result("IpmSolver", a);                               // status, x, lambda, z, f, g, three diagnostics
std::printf("convergence.kkt %.2e\n", a.convergence.kkt);   // the engine's own measure

hven::SqpSolver sqp;
sqp.set_iteration_callback([&](const hven::IterationEvent &) { ++seen_sqp; return hven::CallbackAction::kContinue; });
// depth-0 rows only by default; no depth filter in the callback
const hven::SqpResult b = sqp.solve(problem, x0);
std::printf("convergence.stationarity %.2e  factorizations %lld\n",
            b.convergence.stationarity, static_cast<long long>(b.counters.factorizations));
```

## 13. The break: groups and proof obligations

Each group is one ledger line and lands as a standalone green checkpoint. Functional groups come
first; the rename sweep is last and carries mapped renames only. A field that moves to a
sub-struct moves in its functional group under its final name.

Revertibility, stated as T8 §4 states it:
- B5, B6 and B7 touch disjoint code and revert alone.
- B1-B4 revert in reverse order, and any prefix is a legal stopping point.
- B8 reverts by its inverse manifest, tested once against B7's head.

**The pre-break pin** is the `m8` head immediately before B1. PF-1 and PF-2 land before it or after
B8 closes.

**The common proof (AP-2's six), on every group:** (1) both suites; (2) the install smoke on both
front ends; (3) the export contract; (4) the golden rig; (5) the 27-cell replay × 3 engines,
byte-exact against the pre-break pin; (6) the census when the group can move the walk. The
group-specific obligations follow.

| group | content | estimate | proof beyond the common six |
|---|---|---|---|
| **B1** header hygiene and private implementation | `IpmSolver`/`SqpSolver` behind a private impl; movable; gtest friends into `detail/`; no `detail/` or `fmt` in public headers; header comments rewritten to contract text | 3 | Compile-flags diff, P-SYM over the engine TUs with the impl boundary as the named exception, and the runtime protocol (pass A + B, ±0.5 % instructions) on the SQP arms and the top-level IPM leg (CLAUDE.md §5: a boundary drawn through engine code is proven neutral). The export contract gains "no `detail/` include" and goes red on a violation. A move-construction test runs a solve, moves the solver, and solves again bitwise-equal. |
| **B2** one problem | `Nlp`, `Problem`; `NlpModel`, `ParametricNlpModel`, `NonLinearProgram`, `NlpProblemModel`, `NlpModelAssembly`, `make_nlp_program` to `detail/`; bounds re-read and caches cleared per solve; structure-change refusal; results in the problem's row space; `starting_multipliers` retired | 4-5 | Replay through the `detail::problem_from_*` entries, unchanged. New pins: one `Problem` on both engines; the EX-1 in-place sweep (entry 23) correct on both engines; an unchanged-bounds re-solve counter-identical to today's re-solve; a changed fixed set re-laid and counted; a structure change refused by name; the per-row `lambda`/`g` map round-trip on every row kind; IPM alternation counted. hven-bench re-baselines its harness rows at this group if decision 2 is approved (its adapter changes route). |
| **B3** options | `CommonOptions` (tolerances, `max_iterations`, `max_factorizations`, `time_limit_seconds`, `Verbosity`, `infinite_bound` declared but inert until B7); `IpmOptions::linear`, `termination`, `advanced`; `SqpOptions` prefix drops, `advanced`; option enums singular; `k...Count` sentinels removed; `SqpSolver(SqpOptions = {})`; banner; dump format | 2 | **IPARM-SURFACE label**, with the `iparm` array byte-identical for the default and each non-default linear field. Options-dump drift pin re-derived (declared). P-SYM census of the layout change. The verbosity default (decision 4) is output-only, proven by the replay and by console byte-pins at each rung. `max_factorizations` and `time_limit` are off by default; the new exits are pinned by forced-limit tests. |
| **B4** results, warm start, callbacks, trace interface | `convergence` sub-structs; optionals for `-1` sentinels; public counter types; `kBudgetExhausted` → `kMaxIter` + `returned_best_iterate`; `kWorkLimit` (+ `kTimeLimit`); `WarmStart` class, `result.warm_start`, solver export removed, `native_warm_start`; `SqpWarmStart` opaque; `to_sqp_warm_start(warm, problem)`; `CallbackScope`; `TraceSink` no pure virtuals; internal free functions to `detail/` | 2-3 | T8 §2.7's full warm matrix re-run under the new names. `kBudgetExhausted` retirement: identical point and a pinned label on the `budget_mode` cells. Callback scope: the default stream equals today's stream filtered to depth 0, element for element. Trace goldens unchanged (sink interface only). |
| **B5** evaluation failure | `hven::EvaluationError`; the SQP rejects a trial on it; both engines' rules stated | 1 | Declared behaviour change on throw paths only. New pins on both engines: a trial-point throw is a rejected trial; a start-point throw propagates; another exception type propagates. The replay confirms no corpus cell takes the path. |
| **B6** pieces | local-block layer; raw protocol public and specified; objective-only surface; adapter default by detection; one partition count; declaration builder | 2 | Existing pieces unchanged (replay). Example 4 in both forms agrees: same status, objective to 1e-10 relative, `x` to 1e-8, and the same claim and slot multiset (iteration counts declared if they differ). A plain-type piece compiles with no specialization, and a hook type still single-dispatches (instruction pin). |
| **B7** absent-bound normalization | `infinite_bound` live; SSN cutoff retired into it | 1 | **A declared default event, not a rename rider.** TRAIN evidence: the cells with `|b| >= 1e20` are counted in the declaration before the first leg; their rows are re-derived and declared; every other cell is byte-exact. Census (the SSN cutoff moves the walk on any affected cell). |
| **B8** rename sweep (last) | `hven::solvers` → `hven`; acronyms as words; enumerators `kPascalCase`; aggregate members bare `snake_case`; umbrella `<hven/hven.h>`; `solver_interface_adapter.h` into `model/`; version 0.2.0; CHANGELOG | 1.5 | P-SYM with the object and symbol maps (mangled names move; the map is the proof); the explicit manifests (identifier, path, allowed residual; frozen artifacts excluded by hash); the inverse-manifest revert test. |
| | **total** | **16.5-18.5** | |

**Closing the break.** Each group writes a ledger line. B8 adds `CHANGELOG.md` with the 0.2.0
entry: a short release note naming each group and its one-line "before → after". There is no
migration guide (C201 amendment). The AP-3 examples are tycho's catch-up reference.

**If M8 runs long inside the break:** B7, then B6, then B5 can be held to the next window
without touching the others. That would be the next milestone's single window, under the
one-window rule. B1-B4 and B8 are the break.

## 14. Owner decisions needed at G-API

1. **Move the deeper engine rework to M9.** Recommendation: approve. This is the rework that
   moves the IPM's internal workspace out of the problem object and lets the SQP solve piece-built
   problems. It costs about 15-16 tasks realistically, roughly a third of M8 on its own, and it
   would sit on M8's critical path. The user-facing part (one problem type both engines accept)
   is in this break, so the M9 work changes no public names.
2. **One public way to write a problem.** Recommendation: yes. The SQP's own model form
   (`NlpModel`: split blocks, upper-triangle matrices, deprecated overrides) leaves the public
   surface, and `Nlp` (the Ipopt-style form) is the only callback contract. Cost: the benchmark
   harness rewrites its problem adapter and re-takes its rows once, at the break.
3. **What a huge bound means.** Recommendation: treat any bound at or beyond ±1e20 as "no bound",
   on both engines, the convention Ipopt, Knitro and SNOPT use, with an option to turn it off.
   It lands as its own declared default change with training-set evidence. The alternatives are
   rejecting such bounds with an error, or keeping "every finite number is a real bound". The
   one internal inconsistency (the SSN QP mode already cuts at 1e20) goes away under all three.
4. **Console output by default.** Recommendation: silent on both engines, one line to turn it on.
   Today the SQP is silent and the IPM prints a full table. Printing by default is the other
   coherent choice, and is what Ipopt and Knitro do.
5. **Work and time limits.** Recommendation: add both. A factorization ceiling is deterministic and
   fits the counters-first measurement rule. A wall-clock time limit is what every comparable
   solver offers. It is documented as non-reproducible and never used in measured runs.
6. **A research-knob tier outside the break rule.** Recommendation: yes. Each engine's `advanced`
   options (the QP-tier ladders, restoration internals and similar) may change in any milestone,
   and the header says so. Only the user tier is held to one break window per milestone.
7. **A simpler way to write pieces, now.** Recommendation: yes, about 2 tasks. Pieces state their
   small dense derivative blocks, and the library does the matrix bookkeeping. Example 4 drops
   from about 250 lines to about 60. The current low-level protocol stays, fully documented,
   for tycho's existing code. The alternative is documenting the low-level protocol only (about
   half a task).
8. **The SQP on piece-built problems in M8.** Recommendation: refuse them with a clear message
   until M9's rework. A temporary copying bridge would cost 1-2 tasks that M9 deletes, and the
   one consumer that needs it (tycho) is not yet in sync.
