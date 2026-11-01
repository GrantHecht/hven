# M8 AP-1: the API review

Date: 2026-11-01. Status: for owner approval at G-API. Base: `m8` at `12ca92e7`.
Inputs: the EX-1 friction log (`docs/notes/2026-11-m8-ex1-friction.md`, 31 entries); the eight
`examples/` programs; the public headers under `include/hven/`; the M6 break's group pattern
(`docs/migration/2026-09-m6-api-break.md`); the T8 design's §5/§6 registrations
(`docs/notes/2026-09-m6-w5-t8-design.md:457-501`); `docs/trace-schema-v0.md` §3.
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
- **The model-contract call.** The engine-side unification (T8 §6: the KKT workspace leaves
  `NonLinearProgram`; both engines consume the claim stream; SQP restoration on the common
  contract) is recommended for M9. The size test ("about a third of the milestone") is
  indeterminate at this precision (§8.3), so the recommendation rests on the critical path. The
  eight-coupling specification the brief asked for is not written on this page; it is
  recommended as an M9-W0 task. The user-visible half is delivered in this break (group B2): one
  problem type reaches both engines through public headers, so M9 changes internals only.
- **Friction dispositions.** 26 FIX IN THE BREAK, 3 DOCUMENT ONLY, 1 KEEP, 1 RESOLVED (§11).
- **The break.** Eight groups. Seven are functional, in dependency order. The rename sweep comes
  last. Estimated total: 18-19.5 tasks (§13). B5, B6 and B7 are separable and can be cut without
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
| problem contracts | one | two public callback contracts with opposite conventions, plus a public program struct with 69 public data members and 131 public member functions |
| where the problem goes | one solve entry | the IPM takes a program, the SQP takes a model; the routes between them are not all public |
| absent bound | a threshold (1e19 / 1e20 / 1e20) | the IPM and the SQP's NLP-level tests treat every finite value as real; every SQP QP mode treats a bound at or beyond 1e20 as absent |
| evaluation failure | a defined rule | the IPM rejects a trial on any exception; the SQP propagates every exception (§4.6) |
| tolerances | 2 (optimality, feasibility) | IPM 12 (four measures, three tiers); SQP 2, under different names |
| work limits | iterations, evaluations, time | iterations only; no factorization or time ceiling (F3) |
| public headers | self-contained | 40 `detail/` includes across 10 public headers; `fmt` in 10; gtest names in one |

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
    virtual NlpDimensions dimensions() const = 0;                 // re-queried at EVERY solve
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
// engines treat the trial as rejected. Any other exception propagates (§4.6).

// --- What both engines solve. Owns the engine-side transcriptions. ---
class Problem {
  public:
    explicit Problem(std::shared_ptr<Nlp> nlp);
    explicit Problem(AssemblyDeclaration pieces);   // §9; the SQP refuses it by name until M9
    Index num_variables() const noexcept;
    Index num_constraints() const noexcept;
    const ProblemStatistics &statistics() const noexcept; // row-kind and bound-kind census (§10.4)
    Index partitions() const noexcept;                    // the adopted count (pieces)
};

// --- Engines: one shape. IpmSolver shown; SqpSolver is identical. ---
class IpmSolver {
  public:
    explicit IpmSolver(IpmOptions options = {});              // validate() runs here
    IpmSolver(IpmSolver &&) noexcept;                         // movable (private impl)
    IpmSolver &operator=(IpmSolver &&) noexcept;              // moved-from: destroy or assign only
    IpmResult solve(Problem &problem, ConstVecRef x0,
                    const WarmStart &warm = {}, const SolveBudget &budget = {});
    void set_options(IpmOptions options);                     // validate() runs here
    const IpmOptions &options() const noexcept;
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
// SqpWarmStart declares no default constructor (undeclared, not = delete), so a
// braced {} third argument selects the WarmStart overload.

// --- "No limit" is an empty optional, everywhere. ---
struct SolveBudget {
    std::optional<Index> max_iterations;    // effective cap = min(this, options' cap)
    std::optional<Index> minor_budget;      // SQP probe budget; ignored by the IPM
};

// --- Shared settings, embedded once per engine. ---
struct CommonOptions {
    double optimality_tol = 1e-6;           // stationarity (+ IPM complementarity)
    double feasibility_tol = 1e-6;          // equality and inequality residuals
    Index max_iterations;                   // engine default: IPM 1000 per phase, SQP 100
    std::optional<Index> max_factorizations;   // F3; empty = no ceiling
    std::optional<double> time_limit_seconds;  // wall clock; never set in measured runs
    Verbosity verbosity;                    // kSilent / kWarnings / kSummary / kIterations
    int threads;                            // as today
    StartLevel start_level = StartLevel::kWarm;
    double infinite_bound = 1e20;           // §10.2, subject to the owner's decision
};

// --- One result core. ---
struct WorkCounters {                       // counted at the Problem boundary, both engines
    Index objective_evals = 0, gradient_evals = 0, constraint_evals = 0,
          jacobian_evals = 0, hessian_evals = 0, factorizations = 0;
};
struct SolveResult {
    SolveStatus status;
    std::string message;                    // why the solve stopped, one line
    bool returned_best_iterate = false;     // replaces kBudgetExhausted (§5)
    bool infeasibility_certified = false;   // kInfeasible only; per-engine rule in §5
    Vec x, lambda, z, g;                    // lambda and g: one entry per problem row
    double f;
    double stationarity, feasibility, complementarity;   // shared, declared space
    Index iterations;
    WorkCounters work;
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
  the IPM's cap stays per phase, documented as such. Precedence, as today: an engaged
  `SolveBudget::max_iterations` gives an effective cap of the smaller of the two, so a budget
  only tightens the options' cap.
- **"No limit" is an empty `std::optional`** on every limit: `max_factorizations`,
  `time_limit_seconds`, and both `SolveBudget` members (today `0 = none`). No numeric sentinel
  means "unlimited" anywhere on the public surface.
- **Work and time limits (F3).** `CommonOptions::max_factorizations` is a deterministic ceiling,
  read at iteration boundaries on both engines. Its exit is a new `kWorkLimit`.
  `time_limit_seconds` is wall-clock and reports a new `kTimeLimit`. It is never set in a
  replay, census or harness row (CLAUDE.md §7: a deadline outcome is not
  scheduling-invariant). Both default to empty, so no trajectory moves. The two statuses land in
  the same group as the two limits (B3). Owner decision 5.
- **The SQP minor budget's exit label.** `SolveBudget::minor_budget` exhaustion reports
  `kMaxIter` today (`drivers/solve_result.h:63-67`). Recommended: relabel it `kWorkLimit`, since
  a minor budget is a work ceiling, not a major-iteration cap. The returned point is unchanged.
  Every corpus cell runs under a minor budget (`bench/corpus_cells.h:1656`), so the affected
  cells are the corpus rows with `probe_budget_stops == 1`, counted in B3's declaration before
  its first leg and re-derived as a status-column change. The alternative is to keep `kMaxIter`
  and document it. Owner decision 5.
- **Validation of the new fields.** `validate()` refuses `infinite_bound <= 0` or NaN, an engaged
  `time_limit_seconds <= 0` or NaN, an engaged `max_factorizations < 0`, and an engaged
  `SolveBudget` member `< 1`. `validate()` runs at construction and at `set_options`; a budget is
  validated at `solve` entry.
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
  rename group (the T8.10 lesson, `t8-design.md:425-438`).
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
| public option | `IpmOptions::linear.dense_factorization`, `SqpOptions::linear.dense_factorization` | `DensePolicy`, `kNever` |
| enum | `hven::linear::DensePolicy` | `kNever`, `kAlways`, `kAuto` (kAuto declared with DN-3) |
| factor-layer selection | `dense_factorization` | `DensePolicy`, `kNever` |
| factor-layer threshold, kAuto only (DN-3) | `dense_max_dimension` | `Index`, set by DN-3's rule |
| counter | `dense_factorize_count` | `Index`, beside `analyze_count` and `factorize_count` |

- **The public name is the invariant.** Where the factor-layer field and counter live follows
  the seam DN-1's design page picks (`2026-11-m8-brief.md:268-269`): on
  `SymmetricFactor::Options` and `SymmetricFactor::Counters` if the arm sits inside the factor,
  or on the consumers' own options and counters if the choice is made at the four consumers.
  The names do not change with the seam.
- **Placement: `linear`, on both engines.** The policy configures the KKT factorization, as the
  IPM's other factor knobs do (§4.2), so it sits beside them. The SQP gains a `linear`
  sub-struct holding this one field. If DN-1 fires for one engine only, the field is added on
  that engine's `linear` alone, and the other engine's is added when it follows, additively.

### 4.5 Bounds: absent-bound normalization (F10, #13)

See §10.2. The setting is `CommonOptions::infinite_bound`.

### 4.6 Evaluation failure and the exception contract

**Today.** The IPM catches every exception (`std::exception` and `...`) at seven trial-point
sites and records it in `EvalErrorLog`. In its four line searches an un-evaluable trial is a
merit rejection and the step backs off (`ipm_solver_globalization.cpp:247-266`). In the
soft-feasibility step it reports no reduction, so the caller escalates to the full restoration
entry (`ipm_solver.cpp:1793-1802`). The two SOC sites abandon the correction. The SQP solve loop
has no such catch: every exception from an SQP evaluation propagates out of `solve`. Neither rule
is in a public header.

**Target (B5).** `hven::EvaluationError` is the one signal. Thrown at a trial point, both
engines reject the trial at the same sites as today (the IPM as above; the SQP rejects the step
and shrinks its radius). Thrown at the start point it propagates.

**Declared behaviour change: the IPM narrows.** Its seven sites catch `hven::EvaluationError`
only. Any other exception at a trial point, which the IPM swallows today, propagates after B5.
No corpus cell takes a throw path. hven-bench's CUTEst adapter is audited for the types it
throws, and its evaluation failures are made `EvaluationError` before B5 lands.

**The exception contract** (stated in `<hven/hven.h>`'s overview and on each entry):

- **Contract violations** (bad sizes or bounds, invalid options, a structure change between
  solves, a Hessian-less problem on the IPM, a pieces problem on the SQP): `std::invalid_argument`,
  as today.
- **Backend failures** the engine cannot report as a status (a factorization library error,
  backend memory exhaustion): `hven::BackendError`.
- **Evaluation failures:** `hven::EvaluationError`, thrown by the user, rejected at a trial point
  and propagated from the start point.
- **One base.** `hven::Error : std::runtime_error` is the base of `EvaluationError` and
  `BackendError`, so every runtime failure hven raises is caught in one clause. Internal
  invariant breaks stay `std::logic_error`.
- **After a throw (basic guarantee).** The solver and the `Problem` stay valid and reusable. The
  next `solve` starts from the `Problem`'s re-read state (§8.2) and returns the same status and
  point as the same solve on a fresh solver. B5 pins this on both engines.
- **`noexcept`:** the move operations, `options()`, and `Problem`'s accessors (§3).

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
- **The stop reason as text.** `SolveResult::message` is one line naming why the solve stopped
  (for example the IPM's `IpmStopReason`, the SQP's exit), so a caller can log a result without a
  per-engine switch.
- **Shared work counters.** `SolveResult::work` (`WorkCounters`, all `Index`): objective,
  gradient, constraint, Jacobian and Hessian evaluations, and factorizations. Evaluations are
  counted at the `Problem`'s evaluation boundary, so both engines count them the same way and no
  engine-internal path moves. The engine counter trees (`IpmResult::counters`,
  `SqpResult::counters`) stay as they are.
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
  - Add `kWorkLimit` (factorization ceiling; the SQP minor budget if owner decision 5 relabels
    it) and, if owner decision 5 approves, `kTimeLimit`. Both land in B3 with the limits.
  - `kInfeasible` keeps its name, and its doc says "locally". `infeasibility_certified` moves to
    `SolveResult`, with its rule stated per engine:
    - **SQP:** `kInfeasible` is certified when restoration reached a stationary point of the
      infeasibility measure above `feasibility_tol`; uncertified otherwise (the proximity gate,
      the inconsistent-equalities exit). Unchanged from today (`core/solver_status.h:45-70`).
    - **IPM:** never reports `kInfeasible`. Its restoration's locally-infeasible exit reports
      `kStalled`, with `message` naming it (`solve_status.cpp:35-37`), and
      `infeasibility_certified` is always false. Relabelling that exit would be a status change
      and is not part of the break.
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
  `detail/qp/working_set.h`. It becomes a handle. It declares no default constructor (left
  undeclared, not `= delete`: a deleted one still takes part in overload resolution and would
  make `{}` ambiguous), so a braced `{}` third argument selects `WarmStart`. The native state can
  grow a serialized form later without a break (§10.1). It is built by `to_sqp_warm_start`, by mesh
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
  includes (`sqp_solver.h` 14, `ipm_solver.h` 7, `non_linear_program.h` 7, and others).
  `ipm_solver.h` forward-declares 17 test names (11 gtest fixtures, 6 harness classes) and
  befriends 5 of them; the other 12 declarations are dead. The engines move to a private
  implementation (B1), and the fixtures befriend the implementation class in a `detail/` header.
  B1 removes the 21 engine-header includes. The other 19 leave with the group that moves what
  they carry:

  | public header | `detail/` includes | removed by |
  |---|---|---|
  | `model/non_linear_program.h` | 7 | B2 (the header moves to `detail/`) |
  | `model/nlp_problem_model.h`, `model/nlp_triplet_model.h` | 1 each (`eigen_types.h`) | B2 (both leave the public surface) |
  | `drivers/ipm_solver_types.h` | `ipm_solver_fwd.h` | B3 (`IpmPhase` and the option enums move to the options header) |
  | `drivers/ipm_solver_types.h` | `eval_error_log.h`, `kkt_factorization.h` | B4 (public counter and evaluation-error types) |
  | `drivers/sqp_solver_types.h` | `globalization/sqp/globalization.h` | B3 (the `make_strategy` hook moves with the `advanced` tier) |
  | `drivers/sqp_solver_types.h` | `warmstart/warm_start.h` | B4 (`WarmStart`) |
  | `drivers/trace.h` | `ipm_solver_fwd.h` | B3 |
  | `drivers/trace.h` | `iterate_info.h`, `ipqp_evidence.h` | B4 (trace interface) |
  | `warmstart/ipm_polish_extension.h` | `warmstart/warm_start.h` | B4 |
  | `warmstart/sqp_warm_start.h` | `qp/working_set.h` | B4 (`SqpWarmStart` opaque) |

  `core/solver_counters.h` includes `core/detail/aggregate_arity.h` for three static_asserts that
  pin the counter tables to their structs. It is moved, not exempted: the asserts go to
  `src/drivers/trace_writer.cpp`, which already includes the header, so a struct-table mismatch
  still fails the library build. B1 does this.

  `check_export_contract.sh` gains the rule "no public header includes `detail/`". From B1 to B3
  it checks the residual table above as an allow-list that may only shrink. The rule is armed
  without exceptions at B4's close.
- **`fmt` leaves the public headers.** Ten include it today. `ipm_solver.h` and `sqp_solver.h`
  lose it with the private implementation (B1); `non_linear_program.h` with B2;
  `sqp_warm_start.h` with B4. In `core/ledger.h` the include is unused and kept only to hold
  preprocessed input steady; B1 removes it under its P-SYM run. `drivers/console_trace_sink.h`
  declares `ipm_residual_color`, which returns `fmt::text_style`; B1 moves it to `detail/`.
  In `model/claim_space.h`, `structure_identity.h`, `candidate_point.h` and `nlp_assembly.h`,
  `fmt` builds throw messages. B1 moves each message into an out-of-line cold function in `src/`
  and keeps the check inline. None of these sits in a
  per-element loop: the most frequent, `validate_bound_destination` (`nlp_assembly.h:282`), runs
  once per assembly call. B1's runtime protocol covers them.
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
  - Sizes and counts are `hven::Index` on every public signature and result field. The triplet
    contract's `int` counts and `VectorXi` structures go, and so do result counts such as
    `IpmResult`'s `int soc_steps_taken` and `int watchdog_activations`.
  - Callback signatures use `hven::ConstVecRef`, `VecRef` and `IndexVecRef` from `core/types.h`,
    not `ConstEigenRef` from `detail/`.
- **Version.** `0.2.0`, with a CHANGELOG entry at the break's close.

## 8. The problem contract and the model-contract unification

### 8.1 Today

- Three problem forms are public:
  - `NlpTripletModel`: TNLP shape, lower-triangle triplets, `int` counts.
  - `NlpModel`: split form, `c_E = 0`, `c_I <= 0`, upper-triangle sparse matrices returned by
    value, six of its pure virtuals `[[deprecated]]`. `ParametricNlpModel` sits on top of it.
  - `NonLinearProgram`: a struct with 69 public data members and 131 public member functions
    (non-implicit declarations under `public:`, from clang's AST,
    `-ast-dump-filter=NonLinearProgram`). It holds the IPM's KKT workspace, built by
    `make_nlp_program` or `adopt_declaration`.
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
    is O(nnz) per solve, against at least one factorization;
  - **a bound change that flips a row's kind is a structure change, refused by name.** Row kinds
    (equality, one-sided, range, free) are classified from the row bounds
    (`NLPRowClassification::classify`, `nlp_problem_model.cpp:148`), and they fix the `ce`/`ci`
    sizes and the range-row doubling. A flip, including one caused by `infinite_bound`, would
    re-lay the row space and the multiplier map. Refusing is recommended over re-laying: no
    consumer needs it, and re-laying would add a second layout path to prove.

  A `WarmStart` taken before a re-laid elimination does **not** match afterwards. Its
  `DeclarationKey` digests each variable's bound structure (finite lower, finite upper, fixed;
  `structure_identity.h:177-191`), so a changed fixed set, or any finite-to-absent flip, is an
  identity mismatch and is refused by the existing rule. A value-only bound change keeps the key.

  This gives the IPM the parametric route it lacks today (#23). It retires `ParametricNlpModel`:
  a parameter is the user's own member, and the continuation driver (`detail/warmstart/`) reads
  `Nlp` through the same handle.
- **Convention fixes:** one triangle (lower) and one multiplier vector (#2); no deprecated
  overrides (#3); an optional Hessian (#4); zero-row blocks need no overrides (#5); public type
  aliases (#10).
- **Concurrency rule, stated** (carrying `docs/concurrency.md`'s contract to the new types):
  - one `Problem` serves one thread, one solve at a time;
  - N concurrent solves need N `Problem`s (and N solvers);
  - an `Nlp` shared by several `Problem`s must be safe to call through its `const` members
    concurrently;
  - two engines may alternate on one `Problem`; an IPM alternation re-analyses, as
    `analyzed_owner_id` does today, and is counted.

  B2 updates `docs/concurrency.md` to these types.
- **Pieces** (`Problem(AssemblyDeclaration)`): §9. **Decided: the SQP refuses a pieces problem
  by name until M9.** A temporary copying bridge would cost 1-2 tasks that M9 deletes, and its
  one consumer (tycho) is not yet in sync.
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
conditional, PF 3, HB 5, TR 3, TU 4, CP 3, DC 1, WX about 2). The break (§13) is 18-19.5.

**The size test is indeterminate at this precision.**

- As specified (10-11), it fits a third of an M8 without it (49-51 tasks, a third about 16-17).
- At observed overrun (15-16), it sits at that bound.
- Counted in the denominator (59-67 tasks), a third is about 20-22, and even the overrun
  figure fits.

**Recommendation: move it to M9, on critical-path grounds.**

1. TR, AP-4, TU and CP all wait for AP-2 to close, and the brief does not allow the unification
   to trail as a late group. At 10-16 tasks inside the break, it delays every one of them.
2. Its user-visible half lands now. B2 puts one problem type on both engines through public
   headers. The bridges stay, internal. What M9 adds is the SQP on pieces problems and a solver
   that owns its workspace. That work is behind `Problem`'s face, so M9 needs no second break.
   Its first consumer is tycho, which is out of sync and catches up later.

**The eight-coupling specification is not on this page.** The brief asked AP-1 for it; it was not
written here. Recommended: M9's first task (W0), so the M9 brief is sized from it. Alternative:
an M8 side task (1-1.5) off the critical path, if the owner wants the size settled on evidence
before deciding.

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
  with `NonLinearProgram`, in B2.
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

- **Today: the IPM and the SQP disagree.** The IPM treats every finite bound as real. Inside the
  SQP, every QP subproblem mode treats `|b| >= 1e20` as absent, through four private constants:
  the walk's `detail::kEngineInfBound` (`detail/qp/qp_engine.h:866`, used throughout
  `src/qp/qp_engine.cpp`), IPQP's `kIpqpInfBound` (`detail/qp/ipqp_math.h:29`), SSN's
  `kSsnInfBound` (`detail/qp/ssn_engine.h:54`), and the degeneracy arm's
  `QpDegeneracyParams::inf_bound` (`detail/qp/qp_degeneracy_arm.h:470`). The SQP's NLP-level
  tests treat the same bounds as real (`std::isfinite`, for example
  `src/drivers/sqp_solver.cpp:1165`). Two of two external model sources (hven-bench F9/F10)
  write absent as `±1e20`. EX-1 found `1e30` accepted silently as a real bound.
- **Target, recommended (owner decision 3).** `CommonOptions::infinite_bound = 1e20`.
  - A bound with `|b| >= infinite_bound` is absent. The rule is applied once, at the `Problem`
    boundary, for both engines and every route. The four QP-tier constants retire into it.
  - Setting `infinite_bound = kInfinity` restores "every finite value is real".
  - Normalized bounds are counted and reported in the solve-begin trace event.
  - An equality row declared at `±infinite_bound` is refused by name, as an equality at infinity
    is today.
- **This is a default change.** It is its own declared default event (B7), with TRAIN evidence:
  the cells whose declared bounds include `|b| >= 1e20`, counted in the declaration before the
  first leg. The declaration and the census cover every QP mode (walk, IPQP, SSN, degeneracy
  arm), since each one's cutoff moves to the boundary. It does not ride a rename.
- **Alternatives:** refuse `|b| >= 1e20` finite bounds with an error naming the variable or row;
  or keep "verbatim" and remove the four QP-tier cutoffs. Either way the IPM-versus-SQP
  inconsistency goes.

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
- **User scaling** is registered on the same terms: an optional `Nlp` hook beside
  `linear_rows()` that supplies objective, variable and row scale factors, additive later.

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
| 17 | `std::string` piece corrupts the heap | RESOLVED (DF-1, `12ca92e7`: `TypeStorage` relocates by move construction). B6's piece docs state that any movable type is legal | — |
| 18 | the piece protocol has no public specification | FIX IN THE BREAK: local-block layer as the default; the raw protocol specified | B6 |
| 19 | an objective piece carries the constraint surface | FIX IN THE BREAK: the objective handle forwards objective members only | B6 |
| 20 | every piece type needs an adapter specialization | FIX IN THE BREAK: direct storage by default; hook detected | B6 |
| 21 | partition count clamped silently, stated twice | FIX IN THE BREAK: one place; `Problem::partitions()`; clamp documented | B6 |
| 22 | two ways to populate a program | FIX IN THE BREAK: the master-list route goes internal with `NonLinearProgram` | B2 |
| 23 | parameter changed in a live model: wrong answer, optimal status | FIX IN THE BREAK: bounds re-read and caches cleared at every solve; structure changes refused | B2 |
| 24 | three places to get an SQP warm start | FIX IN THE BREAK: `result.warm_start` (one value); native labelled; solver export removed | B4 |
| 25 | polish: two routes, five headers, conflicting doc | FIX IN THE BREAK: `to_sqp_warm_start(warm, problem)`; doc conflict removed; both routes kept and documented | B4 |
| 26 | two stationarity numbers per result | FIX IN THE BREAK: engine measures under `convergence` | B4 |
| 27 | `iterations` and callback count disagree by engine | DOCUMENT ONLY: the SQP's row 0 is the start point; the IPM's rows equal its iterations | B4 docs |
| 28 | depth filter is the caller's job | FIX IN THE BREAK: `CallbackScope::kTopLevel` by default | B4 |
| 29 | raw non-owning sinks; no trace reader | KEEP: the caller owns the sink and its stream (§6); a reader is TR-1 tooling | — |
| 30 | `dump_options` at 17 digits, two formats | FIX IN THE BREAK: shortest round-trip on both; AP-4 regenerates on top | B3 |
| 31 | IPM bound multipliers carry noise | DOCUMENT ONLY: interior-method behaviour; zeroing would report a value not computed | B4 docs |

Counts: **FIX IN THE BREAK 26, DOCUMENT ONLY 3, KEEP 1, RESOLVED 1.** If owner decision 3 chooses
"verbatim", #13 becomes DOCUMENT ONLY (with the four QP-tier cutoffs removed in B7).

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
    static constexpr hven::Index kInputs = 3, kOutputs = 1;
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
    static constexpr hven::Index kInputs = 1;
    double value(hven::ConstVecRef v) const { return weight * (v[0] - ref) * (v[0] - ref); }
    void gradient(hven::ConstVecRef v, hven::VecRef g) const { g[0] = 2.0 * weight * (v[0] - ref); }
    void hessian(hven::ConstVecRef, hven::MatRef H) const { H(0, 0) = 2.0 * weight; }
};
struct Boundary { /* two rows: y_0 - a, y_N - b */ };

hven::Problem make_chain(hven::Index steps, hven::Index partitions) {
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
std::cout << "adopted " << chain.partitions() << " partitions\n";
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
print_result("IpmSolver", a);                               // status, message, x, ..., work
std::printf("convergence.kkt %.2e\n", a.convergence.kkt);   // the engine's own measure

hven::SqpSolver sqp;
sqp.set_iteration_callback([&](const hven::IterationEvent &) { ++seen_sqp; return hven::CallbackAction::kContinue; });
// depth-0 rows only by default; no depth filter in the callback
const hven::SqpResult b = sqp.solve(problem, x0);
std::cout << b.message << "  convergence.stationarity " << b.convergence.stationarity
          << "  factorizations " << b.work.factorizations << '\n';
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
| **B1** header hygiene and private implementation | `IpmSolver`/`SqpSolver` behind a private impl; movable; gtest friends into `detail/`; the 21 engine-header `detail/` includes removed and the residual listed by group (§7); `aggregate_arity.h` asserts moved to `src/`; `fmt` out of the engine headers and `ledger.h`, throw messages out of line (§7); header comments rewritten to contract text | 3 | Compile-flags diff, P-SYM over the engine TUs with the impl boundary as the named exception, and the runtime protocol (pass A + B, ±0.5 % instructions) on the SQP arms and the top-level IPM leg (CLAUDE.md §5: a boundary drawn through engine code is proven neutral). The export contract gains "no `detail/` include" with §7's residual as a shrink-only allow-list; the rule is armed without exceptions at B4's close. A move-construction test runs a solve, moves the solver, and solves again bitwise-equal; the moved-from solver is destroyed and assigned to. |
| **B2** one problem | `Nlp`, `Problem`; `NlpModel`, `ParametricNlpModel`, `NonLinearProgram`, `NlpProblemModel`, `NlpModelAssembly`, `make_nlp_program`, master-list `make_nlp` to `detail/`; bounds re-read and caches cleared per solve; structure-change and row-kind-flip refusal; results in the problem's row space; `starting_multipliers` retired; `docs/concurrency.md` updated | 4-5 | Replay through the `detail::problem_from_*` entries, unchanged. New pins: the public `Problem(Nlp)` route is counter-identical to today's triplet route (`make_nlp_program` for the IPM, `NlpProblemModel` for the SQP) on the 27 replay cells that dual-bind as a triplet model (`interior_cell_refusal` empty; the subset is named in the declaration); one `Problem` on both engines; the EX-1 in-place sweep (entry 23) correct on both engines; an unchanged-bounds re-solve counter-identical to today's re-solve; a changed fixed set re-laid and counted, and a prior `WarmStart` refused as an identity mismatch; a structure change and a row-kind flip refused by name; the per-row `lambda`/`g` map round-trip on every row kind; IPM alternation counted. hven-bench re-baselines its harness rows at this group if decision 2 is approved (its adapter changes route). |
| **B3** options and limits | `CommonOptions` (tolerances, `max_iterations`, `max_factorizations`, `time_limit_seconds`, `Verbosity`, `infinite_bound` declared but inert until B7); `std::optional` for every "no limit", `SolveBudget` included; `kWorkLimit` and `kTimeLimit`; the minor-budget label (decision 5); validation of the new fields; `IpmOptions::linear`, `termination`, `advanced`; `SqpOptions` prefix drops, `advanced`; `IpmPhase` and option enums public, singular; `k...Count` sentinels removed; `SqpSolver(SqpOptions = {})`; banner; dump format | 2 | **IPARM-SURFACE label**, with the `iparm` array byte-identical for the default and each non-default linear field. Options-dump drift pin re-derived (declared). P-SYM census of the layout change. The verbosity default (decision 4) is output-only, proven by the replay and by console byte-pins at each rung. `max_factorizations` and `time_limit_seconds` are empty by default; the new exits are pinned by forced-limit tests. If the minor-budget relabel is approved, the corpus rows with `probe_budget_stops == 1` are counted in the declaration and re-derived as a status-column change, point unchanged. Validation pins for each refused value. |
| **B4** results, warm start, callbacks, trace interface | `convergence` sub-structs; `message`; `work` counters; `infeasibility_certified` on `SolveResult`; optionals for `-1` sentinels; `Index` result counts; public counter types; `kBudgetExhausted` → `kMaxIter` + `returned_best_iterate`; `WarmStart` class, `result.warm_start`, solver export removed, `native_warm_start`; `SqpWarmStart` opaque; `to_sqp_warm_start(warm, problem)`; `CallbackScope`; `TraceSink` no pure virtuals; internal free functions to `detail/` | 2.5-3 | T8 §2.7's full warm matrix re-run under the new names. `kBudgetExhausted` retirement: identical point and a pinned label on the `budget_mode` cells. Callback scope: the default stream equals today's stream filtered to depth 0, element for element. `work` counts pinned against hand-counted calls on HS071 on both engines. Trace goldens unchanged (sink interface only). The export contract's `detail/` rule armed without exceptions at close. |
| **B5** evaluation failure and exceptions | `hven::Error`, `EvaluationError`, `BackendError`; the SQP rejects a trial on `EvaluationError`; **the IPM narrows** its seven catch sites to `EvaluationError` (declared behaviour change, §4.6); the exception contract stated | 1.5 | Declared behaviour change on throw paths only. **Obligation before landing: the hven-bench CUTEst adapter throw-type audit**, with its evaluation failures thrown as `EvaluationError`. New pins on both engines: a trial-point `EvaluationError` is a rejected trial; a start-point throw propagates; another exception type at a trial point propagates (on the IPM this is the narrowing); after each throw, a re-solve on the same solver and `Problem` returns the fresh-solver status and point. The replay confirms no corpus cell takes the path. |
| **B6** pieces | local-block layer; raw protocol public and specified; objective-only surface; adapter default by detection; one partition count; declaration builder | 2 | Existing pieces unchanged (replay). Example 4 in both forms agrees: same status, objective to 1e-10 relative, `x` to 1e-8, and the same claim and slot multiset (iteration counts declared if they differ). A plain-type piece compiles with no specialization, and a hook type still single-dispatches (instruction pin). |
| **B7** absent-bound normalization | `infinite_bound` live; the four QP-tier constants (`kEngineInfBound`, `kIpqpInfBound`, `kSsnInfBound`, `QpDegeneracyParams::inf_bound`) retired into it | 1.5 | **A declared default event, not a rename rider.** TRAIN evidence: the cells with a declared bound at or beyond 1e20 are counted in the declaration before the first leg, per QP mode (walk, IPQP, SSN, degeneracy arm); their rows are re-derived and declared; every other cell is byte-exact. Census over every QP mode. |
| **B8** rename sweep (last) | `hven::solvers` → `hven`; acronyms as words; enumerators `kPascalCase`; aggregate members bare `snake_case`; umbrella `<hven/hven.h>`; `solver_interface_adapter.h` into `model/`; version 0.2.0; CHANGELOG | 1.5 | P-SYM with the object and symbol maps (mangled names move; the map is the proof); the explicit manifests (identifier, path, allowed residual; frozen artifacts excluded by hash); the inverse-manifest revert test. |
| | **total** | **18-19.5** | |

**Closing the break.** Each group writes a ledger line. B8 adds `CHANGELOG.md` with the 0.2.0
entry: a short release note naming each group and its one-line "before → after". There is no
migration guide (C201 amendment). The AP-3 examples are tycho's catch-up reference.

**If M8 runs long inside the break:** B7, then B6, then B5 can be held to the next window
without touching the others. That would be the next milestone's single window, under the
one-window rule. B1-B4 and B8 are the break.

## 14. Owner decisions needed at G-API

1. **Move the deeper engine rework to M9.** Recommendation: approve. This is the rework that
   moves the IPM's internal workspace out of the problem object and lets the SQP solve piece-built
   problems. Whether it fits "about a third of the milestone" cannot be settled at this
   precision: it fits as specified, sits at the bound at this project's usual overrun, and fits
   again if its own size is counted in the milestone. The recommendation rests instead on the
   schedule: it would sit on M8's critical path, ahead of tracing, tuning and the comparison.
   The user-facing part (one problem type both engines accept) is in this break, so the M9 work
   changes no public names. Its detailed specification is not yet written. Recommended: write it
   as M9's first task. Alternative: write it now, as an M8 side task of about one to one and a
   half tasks off the critical path, if you want the size settled on evidence before deciding.
2. **One public way to write a problem.** Recommendation: yes. The SQP's own model form
   (`NlpModel`: split blocks, upper-triangle matrices, deprecated overrides) leaves the public
   surface, and `Nlp` (the Ipopt-style form) is the only callback contract. Cost: the benchmark
   harness rewrites its problem adapter and re-takes its rows once, at the break.
3. **What a huge bound means.** Recommendation: treat any bound at or beyond ±1e20 as "no bound",
   on both engines, the convention Ipopt, Knitro and SNOPT use, with an option to turn it off.
   It lands as its own declared default change with training-set evidence across every QP mode.
   The alternatives are rejecting such bounds with an error, or keeping "every finite number is a
   real bound". Today the two engines disagree: the SQP's QP subproblems already treat such
   bounds as absent, while the IPM treats them as real. That disagreement goes under all three.
4. **Console output by default.** Recommendation: silent on both engines, one line to turn it on.
   Today the SQP is silent and the IPM prints a full table. Printing by default is the other
   coherent choice, and is what Ipopt and Knitro do.
5. **Work and time limits.** Recommendation: add both. A factorization ceiling is deterministic and
   fits the counters-first measurement rule. A wall-clock time limit is what every comparable
   solver offers. It is documented as non-reproducible and never used in measured runs. With
   them, one label question: when the SQP's per-call budget of inner QP iterations runs out, the
   solve reports "iteration limit" today. Recommendation: report it as "work limit", since it is
   a work ceiling; the returned point does not change, and the benchmark rows that stop this way
   are counted and re-derived. The alternative is to keep "iteration limit" and document it.
6. **A research-knob tier outside the break rule.** Recommendation: yes. Each engine's `advanced`
   options (the QP-tier ladders, restoration internals and similar) may change in any milestone,
   and the header says so. Only the user tier is held to one break window per milestone.
7. **A simpler way to write pieces, now.** Recommendation: yes, about 2 tasks. Pieces state their
   small dense derivative blocks, and the library does the matrix bookkeeping. Example 4 drops
   from about 250 lines to about 60. The current low-level protocol stays, fully documented,
   for tycho's existing code. The alternative is documenting the low-level protocol only (about
   half a task).
8. **Disclosure, no choice asked: the IPM stops absorbing arbitrary errors.** Today the IPM
   treats any exception thrown during a trial-point evaluation as a rejected step and carries on.
   After the break it does so only for the new `EvaluationError`; any other exception reaches the
   caller, as it already does on the SQP. No benchmark problem takes this path. The benchmark
   harness's CUTEst adapter is checked and adjusted before the change lands.
