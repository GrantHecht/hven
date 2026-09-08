# M6 W5 — migration guide

The W5 window is hven's API break window: one declared event, a commit sequence
on `m6`, each entry written WITH the task that breaks something and consolidated
at the window's close. This file is what a consumer reads to move across it.
Entries are in landing order. Nothing here is speculative — every entry
describes a break that has already landed.

---

## T1 — `OptimizationProblemBase` is folded into `NLPSolver`

Landed as `refactor(model): M6 W5 T1 (1/3)` (the fold) and `(2/3)` (the break).

### What changed

1. **`include/hven/drivers/optimization_problem_base.h` IS DELETED.** Include
   `hven/model/nlp_solver.h` instead; it is the only header that ever declared
   anything you could construct.
2. **`hven::solvers::OptimizationProblemBase` no longer exists.** Everything it
   declared now lives in `hven::solvers::NLPSolver`, with the same names, the
   same bodies and the same initialisers: `num_partitions_`, `jet_job_mode_`,
   `nlp_`, `optimizer_`, `default_num_partitions()`, `init_partitions()`,
   `set_num_partitions()`, `jet_initialize()`, `jet_release()`, `jet_run()`,
   `run_nlp_solver()`, `strto_jet_job_mode()` and both `set_jet_job_mode()`
   overloads.
3. **Two nested types are RE-SCOPED.**
   `OptimizationProblemBase::JetJobModes` → `NLPSolver::JetJobModes` (all seven
   enumerators unchanged, `DoNothing` still parsed and still refused by
   `jet_run()` and `run_nlp_solver()`), and
   `OptimizationProblemBase::NlpSolveOutput` → `NLPSolver::NlpSolveOutput`.
4. **`NLPSolver` is `final` and NOT polymorphic.** No virtual functions, no
   virtual destructor, no vtable. `static_assert(std::is_final_v<NLPSolver>)`
   and `static_assert(!std::is_polymorphic_v<NLPSolver>)` hold.
5. **The public header GUARANTEES fewer includes than the deleted one did.**
   After `(3/3)`, `hven/model/nlp_solver.h` directly includes none of these
   seven: `<algorithm>`, `<fmt/color.h>`, `<fmt/core.h>`, `<fmt/format.h>`,
   `<stdexcept>`, `hven/detail/interior/utils/thread_pool.h`,
   `hven/detail/interior/utils/get_core_count.h`.

   Measured at this head, all seven are still REACHABLE through the header, via
   `hven/drivers/interior_point_solver.h` (each verified by a one-TU
   `-fsyntax-only` probe that includes `hven/model/nlp_solver.h` and nothing
   else). So nothing breaks today. What changed is the guarantee: they are no
   longer provided by this header's own include list, and a later window that
   trims `interior_point_solver.h` will take them away without touching
   `nlp_solver.h`. **Do not rely on them transitively — a TU that uses any of
   them should include it itself.** `<stdexcept>` is the one that bites first:
   catching the `std::invalid_argument` these entry points throw is the normal
   use of this surface.

### What did NOT change

Solve semantics, thread and partition defaults, message text, and the modes:
`solve` runs SOE, `optimize` runs OPT, `solve_optimize` runs both and always
runs both, and `solve_optimize_solve` / `optimize_solve` skip their trailing SOE
phase exactly when OPT reported `ConvergenceFlags::CONVERGED`. All of it is
pinned in `tests/interior/test_nlp_solver.cpp` (`NLPSolverJobModeTest`,
`NLPSolverModeSemanticsTest`), landed before the break for that reason.

A caller that holds an `NLPSolver` by value or through
`std::shared_ptr<NLPSolver>` and calls the `x0`-taking entry points needs **no
source change at all**.

### Migration 1 — naming `JetJobModes` or `NlpSolveOutput`

```diff
-hven::solvers::OptimizationProblemBase::JetJobModes mode =
-    hven::solvers::OptimizationProblemBase::JetJobModes::Optimize;
+hven::solvers::NLPSolver::JetJobModes mode =
+    hven::solvers::NLPSolver::JetJobModes::Optimize;

-using Out = hven::solvers::OptimizationProblemBase::NlpSolveOutput;
+using Out = hven::solvers::NLPSolver::NlpSolveOutput;
```

`strto_jet_job_mode` is unchanged and still accepts every spelling it accepted:
`solve`/`Solve`, `optimize`/`Optimize`,
`solve_optimize`/`SolveOptimize`/`Solve_Optimize`,
`solve_optimize_solve`/`SolveOptimizeSolve`/`Solve_Optimize_Solve`,
`optimize_solve`/`OptimizeSolve`/`Optimize_Solve`,
`DoNothing`/`do_nothing`/`Do_Nothing`.

### Migration 2 — a class that DERIVED from the base

This is the breaking case, and there is exactly one known instance of it:
tycho's `tests/cpp/solvers/test_conversion_equivalence.cpp`, whose
`ConvEquivNativeDoor` derived from `OptimizationProblemBase` to get
`optimizer_`, `nlp_` and `run_nlp_solver()` — plus, silently, the base
constructor's defaults — without `NLPSolver`'s transcription step. Its whole
point is that the comparison against `NLPSolver` is FAIR: the optimizer object
and its default partition and QP-thread settings had to be literally the same.

So the replacement must apply those defaults itself, explicitly. The base's
constructor did exactly two things:

```cpp
this->optimizer_ = std::make_shared<InteriorPointSolver>();
this->init_partitions();   // num_partitions_ = default_num_partitions();
                           // optimizer_->set_qp_threads(
                           //     std::min(HVEN_DEFAULT_QP_THREADS,
                           //              utils::get_core_count()));
```

Both `default_num_partitions()` and `HVEN_DEFAULT_QP_THREADS` are still
reachable: the first is `NLPSolver::default_num_partitions()`, a public static;
the second is a PUBLIC compile definition carried on the `hven::hven` usage
requirement, exactly as before. `hven::utils::get_core_count()` is
`hven/detail/interior/utils/get_core_count.h`, which the public header no longer
includes for you (see 5 above).

Before:

```cpp
struct ConvEquivNativeDoor : OptimizationProblemBase {
    std::shared_ptr<NlpModel> model_;
    std::shared_ptr<NLPAdapterCore> core_;
    std::string name_;
    Eigen::VectorXd active_variables_, active_eq_lmults_, active_iq_lmults_;
    bool do_transcription_ = true;

    ConvEquivNativeDoor(std::shared_ptr<NlpModel> model, std::string name)
        : model_(std::move(model)), name_(std::move(name)) {}

    void transcribe() {
        this->core_ = std::make_shared<NLPAdapterCore>(this->model_, this->name_);
        this->nlp_ = make_nlp_program(this->core_);
        this->optimizer_->set_nlp(this->nlp_);
        this->do_transcription_ = false;
    }

    tycho::ConvergenceFlags run_optimize(const Eigen::VectorXd &x0) {
        if (this->do_transcription_) { this->transcribe(); }
        auto out = this->run_nlp_solver(JetJobModes::Optimize, x0);
        this->active_variables_ = out.variables_;
        this->active_eq_lmults_ = out.eq_lmults_;
        this->active_iq_lmults_ = out.iq_lmults_;
        return out.flag_;
    }

    // ... the seven pure virtuals, all delegating to run_optimize ...
};
```

After — no base, an owned optimizer, and the base constructor's two defaults
written out so the fairness argument still holds:

```cpp
#include <algorithm>
#include "hven/detail/interior/utils/get_core_count.h"
#include "hven/drivers/interior_point_solver.h"
#include "hven/model/nlp_solver.h" // NLPSolver::default_num_partitions()

struct ConvEquivNativeDoor {
    std::shared_ptr<hven::solvers::InteriorPointSolver> optimizer_;
    std::shared_ptr<hven::solvers::NonLinearProgram> nlp_;
    int num_partitions_ = 1;

    std::shared_ptr<NlpModel> model_;
    std::shared_ptr<NLPAdapterCore> core_;
    std::string name_;
    Eigen::VectorXd active_variables_, active_eq_lmults_, active_iq_lmults_;
    bool do_transcription_ = true;

    ConvEquivNativeDoor(std::shared_ptr<NlpModel> model, std::string name)
        : model_(std::move(model)), name_(std::move(name)) {
        // EXACTLY what OptimizationProblemBase's constructor did, so this door
        // and NLPSolver still start from identical solver settings.
        this->optimizer_ = std::make_shared<hven::solvers::InteriorPointSolver>();
        this->num_partitions_ = hven::solvers::NLPSolver::default_num_partitions();
        this->optimizer_->set_qp_threads(
            std::min(HVEN_DEFAULT_QP_THREADS, hven::utils::get_core_count()));
    }

    void transcribe() {
        this->core_ = std::make_shared<NLPAdapterCore>(this->model_, this->name_);
        this->nlp_ = make_nlp_program(this->core_);
        this->optimizer_->set_nlp(this->nlp_);
        this->do_transcription_ = false;
    }

    tycho::ConvergenceFlags run_optimize(const Eigen::VectorXd &x0) {
        if (this->do_transcription_) { this->transcribe(); }
        // run_nlp_solver() is NLPSolver's member now, so the dispatch is
        // written out: one entry point, then the same three result reads it
        // performed.
        this->active_variables_ = this->optimizer_->optimize(x0);
        const auto &r = this->optimizer_->result();
        this->active_eq_lmults_ = r.eq_lmults_;
        this->active_iq_lmults_ = r.iq_lmults_;
        return r.converge_flag_;
    }

    // The five no-argument mode entry points and jet_initialize/jet_release
    // existed only because the base declared them pure. Delete them, or keep
    // whichever ones this file actually calls -- WITHOUT `override`.
};
```

Two details worth stating rather than leaving to be rediscovered:

* `run_nlp_solver(Optimize, x0)` and `optimizer_->optimize(x0)` are the same
  call. The wrapper's only other work was copying `result().eq_lmults_`,
  `result().iq_lmults_` and `result().converge_flag_` into its output struct,
  which the snippet above does directly.
* Dropping `override` is required, not cosmetic: with no base there is nothing
  to override, and `override` on a non-overriding member is an error.

### Migration 3 — deleting through a base pointer

`std::shared_ptr<OptimizationProblemBase>` and `OptimizationProblemBase *` have
no replacement, because there is no base and `NLPSolver` has no virtual
destructor. Hold `std::shared_ptr<NLPSolver>` (or the concrete type by value).
Nothing in hven ever held a base pointer, and `Jet::map` is a template over the
problem type — it calls `optprobs[i]->jet_run()` through
`std::shared_ptr<T>`, which is a direct call and needs nothing polymorphic.

---

## T2 — the early callback's three vectors are read-only

Landed as `refactor(interior): M6 W5 T2 — early-callback views const; restoration
entry takes RHS const (DECLARED BREAK)`.

### What changed

`InteriorPointSolver::EarlyCallBackType`'s three VECTOR arguments — XSL, PGX and
RHS — are now `hven::ConstEigenRef<Eigen::VectorXd>`, i.e.
`const Eigen::Ref<const Eigen::VectorXd> &`. The KKT matrix argument is
unchanged and still mutable: writing new values into the entries the matrix
already carries is a use this callback exists for, it is documented on
`EarlyCallBackType`, and it is pinned. `LateCallBackType` was already const in
both of its vectors and is untouched.

### The break, stated plainly

Callers lose the ability to WRITE XSL, PGX or RHS from the early callback. That
was never a documented mutation point — the documented mutation contract is the
KKT matrix's alone — but it is not true that a write would have been discarded.
Measured at the call site, a write to any of the three reached the solve:

* **PGX** is READ six lines later into the Newton right-hand side, where
  `v_rhs.prim_grad() += PGX` folds the objective gradient in — before any later
  refill of that storage. (It is refilled eventually: on the soft-step path it
  is handed to `try_soft_feasibility_step` as `GX` and zeroed and rewritten. That
  is well after the step this iteration computes, so it does not make a callback
  write harmless.)
* **RHS**'s constraint blocks are not written again between the hand-out and the
  factorization, so they ARE the right-hand side the step is computed from. Its
  primal block is added to, not replaced.
* **XSL** is the live iterate, so a write to it lands in the solve in flight.
  The solver may later restore or re-initialise it — the step commits with
  `XSL += alpha*DXSL`, the restoration entry re-initialises its two multiplier
  blocks, and a return-best exit replaces the whole vector — but every one of
  those happens after the step computed from what the callback left there.

So a consumer that relied on writing one of them changes BEHAVIOUR, not merely
compilation. No such consumer is known: all eleven early-callback lambdas in
this repository read only, and so does tycho's single one (below). If you have
one, there is no in-flight replacement — re-declare the problem and solve again.

What the three views ARE is now pinned rather than described
(`tests/interior/test_structure_epoch_gating.cpp`, `EarlyCallbackViews`): at
iteration i the callback's XSL primal block is the x the model was evaluated at,
PGX is `obj_scale` times the gradient the model returned, and the RHS constraint
blocks are the residuals it returned less the row bounds — exactly, as the same
doubles. They are a snapshot in place at the iteration's evaluation stage: the
model has been evaluated and the KKT matrix assembled, and the factorization has
not run.

### Migration — the lambda signature

```diff
 solver.set_early_callback(
-    [&](int iter, double obj_scale, Eigen::Ref<Eigen::VectorXd> xsl, double prim_obj,
-        Eigen::Ref<Eigen::VectorXd> pgx, Eigen::Ref<Eigen::VectorXd> rhs,
+    [&](int iter, double obj_scale, hven::ConstEigenRef<Eigen::VectorXd> xsl, double prim_obj,
+        hven::ConstEigenRef<Eigen::VectorXd> pgx, hven::ConstEigenRef<Eigen::VectorXd> rhs,
         Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt) -> int {
         // reads unchanged; kkt stays writable
         return 0;
     });
```

`hven::ConstEigenRef<Eigen::VectorXd>` and
`const Eigen::Ref<const Eigen::VectorXd> &` are the same type; either spelling
compiles. A lambda that took its vectors by non-const `Eigen::Ref` does not
merely warn — `std::function` refuses to store it — so this is a compile error,
found at every site, not a silent behaviour change.

### tycho

At tycho `48038a2f` (the tree consuming hven pin `b62dbc5`) there is exactly one
consumer, `tests/cpp/solvers/test_feasibility_switch.cpp:813` — a friend-test
lambda that captures elastic step state from the component and writes none of
its vectors. Its three parameter types change and nothing else:

```diff
-    [comp, &cap](int i, double, Eigen::Ref<Eigen::VectorXd>, double,
-                 Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
+    [comp, &cap](int i, double, hven::ConstEigenRef<Eigen::VectorXd>, double,
+                 hven::ConstEigenRef<Eigen::VectorXd>, hven::ConstEigenRef<Eigen::VectorXd>,
                  Eigen::SparseMatrix<double, Eigen::RowMajor> &) -> int {
```

`psiopt/`'s own copy of the engine is the pre-move one and is not consumed here.

### Internal, named for completeness

`hven::solvers::ConstKKTVector` is new in
`include/hven/detail/interior/kkt_vector.h`: the read-only twin of `KKTVector`,
same four-block layout, const accessors only, implicitly convertible FROM
`KKTVector`. `KKTVector` itself is unchanged, and so is every signature that
names it — the twin is a second non-template class precisely so that no
`KKTVector` consumer's mangled name moves.

Three private member signatures took the read-only view with it:
`InteriorPointSolver::constraint_violation_l1`,
`enter_feasibility_restoration` and `dispatch_restoration_entry` (the last two
now take `const Eigen::VectorXd &RHS`). They are private, so this is not a
source break for any consumer; it is listed because a friend test harness that
reaches them — tycho has such harnesses for other members — sees the change.

---

## T4 — the trace sink is public: `hven/drivers/trace.h`, and `IpqpTraceSink` is `TraceSink`

Landed as `refactor(drivers): M6 W5 T4 — trace sink to drivers/trace.h,
IpqpTraceSink → TraceSink, evidence structs to detail/qp/ipqp_evidence.h
(DECLARED BREAK)`.

### What changed

1. **The schema header MOVED out of `detail/`.**
   `hven/detail/qp/ipqp_trace.h` → **`hven/drivers/trace.h`**. It was always a
   public surface in fact — a harness consumes the writer and its events — and
   `drivers/trace_writer.h` already included it, so the only thing `detail/` was
   doing was telling consumers not to.
2. **`hven::solvers::IpqpTraceSink` → `hven::solvers::TraceSink`.** Same 14
   virtuals, same signatures, same pure/non-pure split, same out-of-line
   destructor. **No alias for the old name is kept**: the rename IS the break, so
   a consumer that still names `IpqpTraceSink` gets a compile error rather than a
   deprecation it can ignore.
3. **The `Ipqp*` EVENT names are UNCHANGED.** `IpqpTraceIterEvent`,
   `IpqpTraceRegEvent`, `IpqpTraceRestartEvent`, `IpqpTraceRouteEvent`,
   `IpqpTraceCertifyEvent`, `IpqpTraceEscapeEvent`, `IpqpTraceEscapeEvidence`,
   `QpModeTraceEvent`, `SqpFallbackVerdictTraceEvent`, the three `Sqp*` and three
   `Ipm*` whole-solve events, every enum, `VariableBoundCensus` and
   `census_variable_bounds` all keep their spelling and all move with the header.
   Only the SINK is renamed here; T8 is the naming sweep and this is not it.
4. **The two escape-evidence blocks SPLIT out of the engine header.**
   `IpqpStallEvidence` and `IpqpInfeasibilityEvidence` — the only two engine
   types the schema names — now live in **`hven/detail/qp/ipqp_evidence.h`**,
   whose entire include list is `hven/core/types.h`. Their definitions are
   unchanged: same fields, same order, same defaults, same documentation.
   `ipqp_engine.h` includes the new header, so `IpqpResult` still carries both
   and **a TU that already included `ipqp_engine.h` needs no change**. What the
   split buys is that `drivers/trace.h` no longer includes `ipqp_engine.h` at
   all.
5. **The .cpp moved with it**: `src/qp/ipqp_trace.cpp` → `src/drivers/trace.cpp`.
   Internal, listed so a downstream build that names hven's sources sees it.

### What a consumer includes now

| you want | include |
|---|---|
| the sink to derive from, and the event structs | `hven/drivers/trace.h` |
| the JSON-lines writer (`JsonLinesTraceSink`) | `hven/drivers/trace_writer.h` — it includes `trace.h` for you |
| `attach_trace` on `SqpDriver` / `InteriorPointSolver` / `IpqpEngine` | nothing new: `sqp_driver.h` includes the schema, and `interior_point_solver.h` forward-declares `class TraceSink;`, which is all a pointer argument needs |

`JsonLinesTraceSink`'s own surface is untouched: same constructor, same
`failed()`, `lines_written()`, `depth()` and `reset_nesting()`, same 14
overrides, same bytes on the wire. **Every golden line in
`tests/sqp/test_trace_writer.cpp`, `tests/sqp/test_ipqp_trace.cpp` and
`tests/interior/test_ipm_trace.cpp` is byte-unchanged** — the JSON schema carries
no C++ names — and `docs/trace-schema-v0.md` is untouched.

### Migration

```diff
-#include <hven/detail/qp/ipqp_trace.h>
+#include <hven/drivers/trace.h>

-class MySink : public hven::solvers::IpqpTraceSink {
+class MySink : public hven::solvers::TraceSink {
     void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &e) override;
     // ... the other seven pure virtuals; the six W4 defaults are optional
 };

-void attach(hven::solvers::SqpDriver &d, hven::solvers::IpqpTraceSink *s) {
+void attach(hven::solvers::SqpDriver &d, hven::solvers::TraceSink *s) {
     d.attach_trace(s);
 }
```

That is the whole change: one include path, one type name. Nothing else about
writing or reading a trace moves.

### tycho

**No consumer, either way.** A read-only grep of tycho at `48038a2f` (the tree
consuming hven pin `b62dbc5`) and at `origin/main` `599f506a`, excluding `dep/`,
for `ipqp_trace`, `IpqpTraceSink`, `TraceSink`, `attach_trace`, `trace_writer.h`
and `hven/drivers/trace`: **zero matches**. tycho does not attach a trace sink
today, so T4 costs its consume nothing. The first tycho consumer includes
`hven/drivers/trace.h` for the sink and the events, `hven/drivers/trace_writer.h`
for the writer, and derives from `hven::solvers::TraceSink`.

---

## T3 — the six by-value `NlpModel` evaluations are `[[deprecated]]`

Landed as `refactor(model): M6 W5 T3 — deprecate the six by-value evaluations;
migrate library and bench calls to in-place (DECLARED BREAK)`.

### What changed

1. **Six pure virtuals carry `[[deprecated]]`.** Nothing is removed and no
   signature moves: this is **source-preserving and ABI-preserving**. It is
   nonetheless a **declared WARNING-POLICY build break**, and that is the
   precise statement — a translation unit that CALLS one of the six through a
   `NlpModel` reference now emits `-Wdeprecated-declarations`, so a consumer
   building with `-Werror` (or `-Werror=deprecated-declarations`) **fails to
   compile until it brackets or migrates that call**. Consumers on plain
   `-Wall` see a warning naming the replacement and keep building.

   | deprecated | replacement |
   |---|---|
   | `Vec eval_grad(const Vec &x) const` | `void eval_grad_in_place(const Vec &x, Vec &out) const` |
   | `Vec eval_ce(const Vec &x) const` | `void eval_ce_in_place(const Vec &x, Vec &out) const` |
   | `Vec eval_ci(const Vec &x) const` | `void eval_ci_in_place(const Vec &x, Vec &out) const` |
   | `SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &le, const Vec &li) const` | `void eval_hess_in_place(const Vec &x, double obj_scale, const Vec &le, const Vec &li, SpMatRM &out) const` |
   | `SpMatRM eval_jac_e(const Vec &x) const` | `void eval_jac_e_in_place(const Vec &x, SpMatRM &out) const` |
   | `SpMatRM eval_jac_i(const Vec &x) const` | `void eval_jac_i_in_place(const Vec &x, SpMatRM &out) const` |

   The in-place forms are not new — they have shipped since M5 with a default
   that delegates to the by-value counterpart, so a model that overrides only
   the by-value forms already answers both. That default is why **no model
   implementation needs an edit**; what the deprecation reaches is CALL SITES,
   and only those whose static type is `NlpModel` itself (see 4).
2. **`eval_f` is NOT deprecated** (plan Q-S2). It returns a `double`; the
   in-place twins exist to remove a vector allocation, and there is none to
   remove. Six deprecations, not seven. `eval_values` is not deprecated either.
3. **REMOVAL IS NOT PROMISED HERE** (plan Q-O2). The by-value forms stay pure
   virtual and required. A later removal is its own declared break, decided on
   its own evidence — this entry makes no commitment about when, or that it
   happens at all.
4. **OVERRIDING a deprecated virtual does not warn** on any clang-family
   frontend. A model that implements `eval_grad` and friends needs **no change
   at all**: the override definition is silent, and callers that hold the model
   by its own concrete type are silent too. What warns is a CALL that resolves
   to `NlpModel`'s declaration — the shape a driver, a wrapper, or a generic
   helper taking `const NlpModel &` has.
5. **hven's own library and bench calls migrated**: 47 call sites in 7 files
   — `sqp_driver.cpp` 9, `nlp_model_aggregate.cpp` 10,
   `soc_elastic_restoration.cpp` 5, `predictor.h` 8, `crossover_legs.h` 8,
   `snopt_f7_driver.h` 6, `corpus_cells.h` 1. Evaluation COUNT, guards,
   ordering, destination shape, compression and aliasing are unchanged at every
   one. `src/model/nlp_adapter.cpp` (the IPM path) already used the in-place
   forms and is untouched.
6. **`hven/core/compiler.h` is new**, carrying
   `HVEN_SUPPRESS_DEPRECATED_BEGIN` / `HVEN_SUPPRESS_DEPRECATED_END`.

### Migrating a call

```diff
-ev.grad = model.eval_grad(x);
+model.eval_grad_in_place(x, ev.grad);

-qp.H = model.eval_hess(x, obj_scale, le, li);
+model.eval_hess_in_place(x, obj_scale, le, li, qp.H);
 qp.H.makeCompressed();   // unchanged: the contract still requires compressed
```

Three rules the migration inside hven followed, and a consumer should too:

* **Never substitute `eval_values` for a single-quantity call.** It evaluates
  f, cE and cI; a site that wanted one of them would pay for three.
* **A guarded call stays guarded, and the else branch stays explicit.** A
  ternary like `dst = rows > 0 ? model.eval_ce(x) : Vec(0);` becomes an `if`
  with an `else dst = Vec(0);` — dropping the else leaves a reused destination
  at whatever size it last had.
* **The destination must be a real `Vec &` / `SpMatRM &`.** An `Eigen::Ref`
  destination cannot receive an in-place call; fill a local and assign, which
  is the same one temporary the by-value call already made.

### If you WANT to keep calling the by-value form

A consumer mid-migration, a model that overrides the by-value forms and calls
them itself, or a test that keeps a by-value evaluation as an independent oracle
of the in-place path: bracket it.

```cpp
#include <hven/core/compiler.h>

HVEN_SUPPRESS_DEPRECATED_BEGIN
// by-value on purpose: <the reason>
const hven::Vec g = model.eval_grad(x);
HVEN_SUPPRESS_DEPRECATED_END
```

The pair is a scoped push/pop and has clang, gcc and MSVC branches. It is the
same bracket `nlp_model.h` uses around its own six in-place defaults, which
delegate to the deprecated forms by design.

### tycho

**This is a READ-ONLY GREP of tycho at `48038a2f` (the tree consuming hven pin
`b62dbc5`), excluding `dep/`. tycho was NOT BUILT against this header, so the
warn/silent column below is derived from each call's STATIC TYPE, not observed
from a compiler.** No tycho file was edited.

23 by-value grep sites in three files. Which of them warn is decided entirely
by the static type at the call: `NlpModel` warns, a concrete derived class that
redeclares the method does not (see 4 above).

| file | sites | verdict |
|---|---|---|
| `test_conversion_equivalence.cpp` `:347-362` (7 sites, 12 calls) | `conv_equiv_expect_models_agree(const NlpModel &native, const NlpModel &converted, …)` | **CERTAIN WARN** — the only base-reference block in tycho. This is where an hven consumer on `-Werror` breaks |
| `test_conversion_equivalence.cpp` `:519, :522, :526` (3) | `out = this->eval_jac_e(x)` inside tycho's own `ConvEquivEqBoundNativeInPlace` | **SILENT** — the same mutual-default shape `nlp_model.h` brackets, and `this` is the derived type, which redeclares the method |
| `test_conversion_equivalence.cpp` `:1068-1076` (5) | `const ConvEquivEqBoundNativeInPlace model;` — compares in-place results against by-value ones | **SILENT** — a concrete derived type. It is also a genuine oracle: bracket it if it ever starts warning, do not rewrite it to call one API twice |
| `test_model_contract_pins.cpp` `:396-439` (7) | `NlpProblemModel model(…)`, `model.eval_hess(…)` inside `EXPECT_THROW`/`EXPECT_NO_THROW` | **SILENT** — concrete derived type. Contract pins on the by-value entry; bracket, do not migrate |
| `psiopt/src/nlp_adapter.cpp:258` (1) | `problem_->eval_hess(…)` on **`NLPProblem`** | **OUT OF SCOPE** — a different interface, untouched by T3 |

`src/solvers/engines.cpp`'s five `eval_*` definitions are OVERRIDES and stay
silent (overriding a deprecated virtual never warns). tycho's own models need no
edit. **The one place tycho must act is the base-reference block**, and only if
it builds hven consumers with `-Werror`; the bracket or the in-place migration
are both fine, at tycho's discretion — neither is forced.

---

## T6 — the SQP driver's kernels TU: **PROPOSED, MEASURED, AND ABANDONED**

**NOTHING CHANGED FOR YOU. There is no migration, because the change was
reverted before it ever reached a release.** This entry exists because the guide
records what happened in the window, and a consumer who saw the intermediate
commits on `m6` — or a symbol table taken from one of them — should be able to
find out what they were looking at.

### What was proposed

M6 W5 T6 cut (d) moved six definitions out of `src/drivers/sqp_driver.cpp` into
a new `src/drivers/sqp_kernels.cpp`: `run_elastic_ladder`,
`certified_feasibility_fallback` (both keeping the external linkage and the
public-header declarations they already had), the three helpers that serve only
them, and the anonymous-namespace `trace_outcome_of`, which would have become
`hven::solvers::detail::trace_outcome_of(QpStatus)` declared in a
SOURCE-PRIVATE header under `src/` — never installed, never public API.

It landed at `f47da07` and was **reverted**. It was an experiment with a veto
from the day it was designed, not a decision being undone.

### Why it was abandoned (owner's ruling, 2026-09-07)

The experiment ran to its answer, and the answer was no on both halves:

* **Runtime**: LAYOUT-MOVED, but ipm was not FLAT at **1.0098**, and the
  cumulative post-T3 ipm reading came out at **1.0121** — **outside the ±0.5 %
  corpus bar**.
* **Build**: the pre-registered threshold FAILED — two-TU parallel span
  **0.9718** against a required ≤ 0.85, and serial compile CPU went **×1.604**.
  This library's build is dominated by a per-TU header-parse floor, not by file
  length, and splitting a TU duplicates that floor.
* REDRAW was not indicated: there was no cheaper boundary that would have
  changed either number.

### What this means for a consumer

* `hven::solvers::run_elastic_ladder` and
  `hven::solvers::certified_feasibility_fallback` are where they always were,
  declared in `hven/drivers/sqp_driver.h`, with the same signatures and the same
  behaviour. They never moved as far as any released artifact is concerned.
* **`hven::solvers::detail::trace_outcome_of` DOES NOT EXIST.** If you saw it in
  a symbol table taken from `f47da07`, it was internal, it was never public API,
  and it is gone.
* `src/drivers/sqp_kernels.cpp` and `src/drivers/sqp_kernels_internal.h` do not
  exist. The library's source count is back to 41.

### What did NOT go back

Three things from the same task are DELIBERATELY KEPT, because none of them
depended on the split and each stands on its own:

1. **The two hand-off guards keep INTERNAL linkage.**
   `assert_ssn_warm_grade_window` and `assert_ipqp_hand_off_window` had external
   linkage and no declaration anywhere in the tree — an accident, not a surface.
   Their symbols are gone from `libhven.a` and stay gone. **This is the only
   symbol-table change from T6.d that survives**, and nothing could legally have
   named either function.
2. **`bench_corpus` keeps its HS suite**, `--repeat`, `--hs-cells`,
   `--hs-trace` and `--hs-warmup`. Bench-only; no library effect.
3. The engineering notes and measurement protocol amendments.

---

## T7 — header comments only: **NOTHING CHANGED FOR YOU**

**No API break, no behaviour change, no symbol change.** M6 W5 T7 rewrote the
comments in ten headers: the operative contracts — ordering, ownership and
lifetime, exceptions, numerical constraints, and what is emitted when something
is absent — stay beside their declarations in terse Doxygen, and the discussion
prose that used to surround them moved, verbatim and stamped with its source
commit, path and original line numbers, to
`docs/notes/2026-09-header-prose-archive.md`.

**The ten are the set the M6 W5 plan designated,** not simply the ten largest.
Nine are hven's largest headers by comment-line count at `1997159`; the tenth,
`include/hven/detail/qp/ipqp_engine.h`, ranks eleventh. The actual tenth,
`include/hven/model/nlp_aggregate.h`, was excluded because M6 W5 T8 renames it
and a source stamp on a name about to change is the churn the stamp exists to
avoid.

**What the archive does and does not hold.** It holds every block of discussion
prose that was lifted out whole — 301 stamped entries. It does not hold the
lines that were *rewritten* rather than removed: 355 of them (204 `///`,
151 `//`) were recast into the terse form in place, and their operative content
is still beside the declaration. So the archive is not a complete record of
every byte that changed. If you are looking for exactly what a comment used to
say, the authority is `git diff 1997159 -- <path>`, which is immutable; the
archive is the place to read the *reasoning* that no longer belongs in an API
header.

**One non-comment change rides the task:** thirteen dead `friend` declarations
naming test harnesses and gtest classes that no longer exist were removed —
twelve from `include/hven/drivers/interior_point_solver.h`, and the twin (plus
its forward declaration) from
`include/hven/detail/globalization/feasibility_switch_recovery.h`. A friend
declaration emits nothing.

**Objects: 160 of 164 byte-identical, and the other four differ for a reason
that is not this task's code.** `bench_corpus.cpp.o`, `bench_crossover.cpp.o`,
`ipqp_e1_arm.cpp.o` and the golden rig's `trace_support.cpp.o` each differ in
exactly twelve bytes, all inside `.rodata.str1.1`, and all of them the
`git describe --always --dirty --abbrev=12` stamp that `bench/CMakeLists.txt`
embeds at configure time. That is a **real object change**, not a byte-identity
result with an excuse attached: the bytes are not equal. It is also
non-instructional — every `.text` byte, every relocation, every other section
and the instruction counts are identical, and the differing content is a commit
sha, not a decision the code makes. `libhven.a` itself is byte-identical.
