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

---

## T8.2 — `SolveStatus`

Landed as `feat(drivers): M6 W5 T8.2 — SolveStatus, the nine-value status both
engines will report; kStalled split from NOTCONVERGED by stop reason`.

### What changed

1. **A new public header, `hven/drivers/solve_status.h`.** It declares
   `hven::solvers::SolveStatus` — the nine values both engines will report:
   `kOptimal`, `kAcceptable`, `kMaxIter`, `kInfeasible`, `kStalled`,
   `kDiverging`, `kNumericalError`, `kBudgetExhausted`, `kInterrupted` — with
   `to_string`, a total `severity` order over all nine, and the two mappings
   `to_solve_status(ConvergenceFlags, IpmStopReason)` and
   `to_solve_status(SqpStatus)`.
2. **`hven::ConvergenceFlags` and `hven::solvers::SqpStatus` are UNCHANGED and
   still what every surface returns.** Nothing on either engine returns
   `SolveStatus` yet; T8.4 makes that switch and removes the two old enums. This
   task is the vocabulary and the instrumentation behind it.
3. **The interior-point engine records WHY its loop ended.**
   `hven::solvers::IpmStopReason` — `kNone`, `kIterationCap`,
   `kRestorationLocallyInfeasible`, `kStageStalled` — is written per phase and
   read back through `InteriorPointSolver::last_stop_reason()`. It is reset to
   `kNone` at each phase start, so a multi-phase call reports the last phase that
   ran; `kNone` means that phase left by a door the verdict itself explains (see
   the fix-round-1 amendment below for the exact enumeration).
4. **No trajectory moves.** The stores are writes to a member nothing else in the
   engine reads; the U0 walk/ssn/ipm replay and the interior leg are both
   identical across the change.

### What you do

Nothing yet. `last_stop_reason()` is available if you want the split before T8.4
switches the surfaces over. When it does, this is the mapping:

| interior-point `ConvergenceFlags` | stop reason | `SolveStatus` |
|---|---|---|
| `CONVERGED` | any | `kOptimal` |
| `ACCEPTABLE` | any | `kAcceptable` |
| `DIVERGING` | any | `kDiverging` |
| `SINGULAR_KKT` | any | `kNumericalError` |
| `NOTCONVERGED` | `kIterationCap` or `kNone` | `kMaxIter` |
| `NOTCONVERGED` | `kStageStalled` | `kStalled` |
| `NOTCONVERGED` | `kRestorationLocallyInfeasible` | `kStalled` |

| SQP `SqpStatus` | `SolveStatus` |
|---|---|
| `kOptimal` / `kMaxIter` / `kInfeasible` / `kNumericalError` / `kBudgetExhausted` | the same name |

Two rules the table encodes. A verdict the convergence check already holds wins
over the stop reason, so a stall at an acceptable iterate reports `kAcceptable`.
A stall that coincides with the iteration cap reports `kStalled`, because the
stall is recorded in the iteration that reaches the terminal conjunction and the
cap store defers to any reason already in place.

Reachability, per engine: the interior-point engine reports neither `kInfeasible`
nor `kBudgetExhausted`, the SQP engine reports none of `kAcceptable`, `kStalled`
and `kDiverging`, and neither reports `kInterrupted` today.

### What is pinned

Three unit tests in the new `hven_drivers_tests` target
(`tests/drivers/test_solve_status.cpp`): the severity order is total and as
documented, the interior-point mapping splits `NOTCONVERGED` by reason and keeps
the stronger verdict, and the SQP mapping is the identity on its five.

Four LIVE pins, each driving a real solve: the iteration cap on HS071 at
`max_iters = 1` and the reset-per-call rule
(`tests/interior/test_nlp_solver.cpp`); the stalled feasibility stage, the
restoration that converged to a locally infeasible point, and the
stall-beats-cap tie (`tests/interior/test_ipm_stop_reason.cpp`).

The replay leg grew with the task: `bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv`
is re-derived — a declared re-derivation, CLAUDE.md §7 — with a `stop_reason`
column and four abnormal-exit rows, and its 33 base rows are byte-identical to
T8.1's outside the added `stop_reason` column and `wall_s`, which is
informational and which the replay comparator excludes by name.

### Amended in fix round 1

Landed as `fix(drivers): M6 W5 T8.2 fix1 — …`. Four things change what the entry
above says; the baseline and every counter in the leg are untouched.

1. **The iteration-cap label is stored at BOTH cap doors, and reads no verdict.**
   It is written in the terminal conjunction when that iteration is the cap
   iteration, and after the loop when the loop ran out of iterations without
   taking any of its three outer breaks (five `continue`s can bypass the
   conjunction on the cap iteration). Both stores are guarded on the reason still
   being `kNone` — the stall is recorded first and keeps the tie — and on nothing
   else. So `last_stop_reason()` is PHASE-LOCAL and exact for every loop exit:
   the restoration locally-infeasible break, the converge-check early exit
   (`kNone` — that phase's verdict is the whole explanation), the terminal
   conjunction, exhaustion, and an exception, which produces no result at all.
2. **What that label does NOT claim is agreement with the verdict.**
   `result().converge_flag_`'s lifetime is the CALL, not the phase: in a
   multi-phase call whose later phase leaves without assigning it, the reported
   verdict is the EARLIER phase's while the reason correctly describes the later
   one. That verdict lifetime is pre-existing engine behaviour, unchanged by this
   task and REGISTERED for T8.4's per-phase results; `to_solve_status()` reads
   the verdict first, so the status it produces is only ever as good as the
   verdict. The one thing the fix removes is the instrumentation's own dependence
   on it.
3. **`kStageStalled` is demonstrated reachable only with divergence detection
   lifted; default-threshold reachability is undemonstrated.** The one fixture
   that reaches the stall needs `set_div_tols` raised past the violation spike it
   is built around: at the default thresholds the same fixture and the same lever
   set report `DIVERGING` within a few iterations, far short of the detector's
   fifty-iteration window. Both are now pinned side by side in
   `tests/interior/test_ipm_stop_reason.cpp`.
4. **The tie pin asserts the terminal LOOP INDEX**, not just the label: at the
   derived tie cap the stall must fire on the cap iteration itself
   (`terminal index == cap - 1`), so an implementation that let the cap win the
   tie cannot pass by stalling one cap later. `result().iter_num_` is the history
   size, which the restoration transitions pop, and is not the loop count; the
   pins read the index through the late callback instead.

### Amended in fix round 2

Landed as `fix(drivers): M6 W5 T8.2 fix2 — …`. One thing changes what fix round 1
says; the baseline and every counter in the leg are untouched.

1. **A phase that CONVERGES on exactly its cap iteration reads `kNone`, not
   `kIterationCap`.** Fix round 1 guarded the terminal conjunction's cap store on
   the reason alone, which meant a phase whose last iteration was both the cap
   iteration and a converged one carried `iteration_cap` beside a `CONVERGED`
   verdict. That store is now additionally guarded on `alg_impl`'s own
   phase-LOCAL exit code — `converge_check`'s answer for THIS iteration, upgraded
   in place to `DIVERGING`/`ACCEPTABLE`/`SINGULAR_KKT` — so the cap label is
   written only when the local verdict is `NOTCONVERGED`. The per-CALL
   `result().converge_flag_` is still never read, so point 2 below is unchanged:
   the label stays phase-local. The consumer-visible rule is now simply **the
   reason never contradicts the verdict it is reported beside**: the cap label
   means "ran out of iterations with nothing better to say", and `kNone` beside a
   `CONVERGED` verdict means the convergence, not the cap, ended the phase.
   Exhaustion (the second cap door, below the loop) is unchanged: a fall-through
   reaches no conjunction and so has no local verdict, and the cap is the only
   way to get there.
2. Everything fix round 1's points 2, 3 and 4 say still holds, unchanged.

---

## T8.3 — options as values: `CommonOptions`, `IpmOptions`, `SqpOptions::common`, `set_options()`

### What changed

**The interior-point engine's settings are a value, and the setters are gone.**
`InteriorPointSolver::Settings` is now `hven::solvers::IpmOptions`, in the new
public header `hven/drivers/ipm_solver_types.h`. Same 65 knobs, same declaration
order, same defaults, **trailing underscores dropped**; two of them moved into
the new `hven::solvers::CommonOptions` (`hven/drivers/common_options.h`), which
both engines embed as a member called `common`:

| before | now |
|---|---|
| `Settings::qp_threads_` | `IpmOptions::common.threads` (same default, `HVEN_DEFAULT_QP_THREADS`) |
| `Settings::print_level_` | `IpmOptions::common.print_level` (same default, `0` = full output) |

`CommonOptions` also carries `start_level`, which the interior-point engine
CARRIES BUT DOES NOT READ in T8.3 — T8.5 is where its warm-start entry starts
consulting it. Nothing behaves differently because of this task: the fields
moved, their defaults did not, and every read site reads the same value it read
before.

**Removed from `InteriorPointSolver`:** the 58 declared `set_*()` methods
(including the six string-taking overloads), the four static `strto_*()`
parsers, `apply_preset()`, `settings()` (both overloads) and the nested
`Settings` struct itself. **Added:** `const IpmOptions &options() const noexcept`,
`void set_options(IpmOptions)`, and `explicit InteriorPointSolver(IpmOptions = {})`.
The `shared_ptr<NonLinearProgram>`-taking constructor is unchanged (it goes at
T8.4).

**The eight mode enums moved out of the class** — `BarrierModes`,
`LineSearchModes`, `AlgorithmModes`, `QPAlgModes`, `QPOrderingModes`,
`BestCriteriaModes`, `QPPivotModes`, `PDStepStrategies` are now at namespace
scope in `hven/drivers/ipm_solver_types.h`, so a caller can name an option's
value without including the engine. `InteriorPointSolver` keeps a member alias
for each, so **every `InteriorPointSolver::BarrierModes::LOQO` spelling in your
tree still compiles and still names the same type.**

**The presets are free functions returning a full value.**
`IpmOptions ipm_preset(std::string_view)` replaces `apply_preset()`. It applies
the same nine fields to a DEFAULT-constructed value and returns it, rather than
overlaying them on whatever the solver already held — so knobs you want kept are
written on top of the preset, not before it.

**Validation is one free function.** `void validate(const IpmOptions &)` is the
old `Settings::validate()` body, with `common.threads` and `common.print_level`
checked under those names. It runs at construction, at `set_options()` and again
at `run_phase_sequence()` entry.

**`src/drivers/interior_point_solver_settings.cpp` was RENAMED to
`src/drivers/ipm_options.cpp`.** The library's source count is unchanged (42).

### What you do

```cpp
// before
hven::solvers::InteriorPointSolver solver;
solver.set_max_iters(200);
solver.set_print_level(10);
solver.set_tols(1e-6, 1e-6, 1e-6, 1e-6);
solver.apply_preset("filter_l1");

// now
auto o = hven::solvers::ipm_preset("filter_l1");   // start from the preset
o.max_iters = 200;
o.common.print_level = 10;
o.kkt_tol = o.econ_tol = o.icon_tol = o.bar_tol = 1e-6;
hven::solvers::InteriorPointSolver solver(o);      // or solver.set_options(std::move(o));
```

Reading a setting: `solver.settings().max_iters_` becomes
`solver.options().max_iters`. There is no mutable accessor — a field is changed
by replacing the whole value.

The setter → field table, in the header's own order:

| removed setter | write instead |
|---|---|
| `set_max_iters(v)` | `o.max_iters = v` |
| `set_max_acc_iters(v)` | `o.max_acc_iters = v` |
| `set_max_ls_iters(v)` | `o.max_ls_iters = v` |
| `set_all_max_iters(a, b)` | `o.max_iters = a; o.max_acc_iters = b` |
| `set_max_soc(v)` | `o.max_soc = v` |
| `set_ls_extended_iters(v)` | `o.ls_extended_iters = v` |
| `set_max_feas_rest(v)` | `o.max_feas_rest = v` |
| `set_kkt_tol(v)` | `o.kkt_tol = v` |
| `set_bar_tol(v)` | `o.bar_tol = v` |
| `set_econ_tol(v)` | `o.econ_tol = v` |
| `set_icon_tol(v)` | `o.icon_tol = v` |
| `set_tols(k, e, i, b)` | `o.kkt_tol = k; o.econ_tol = e; o.icon_tol = i; o.bar_tol = b` |
| `set_acc_kkt_tol(v)` | `o.acc_kkt_tol = v` |
| `set_acc_bar_tol(v)` | `o.acc_bar_tol = v` |
| `set_acc_econ_tol(v)` | `o.acc_econ_tol = v` |
| `set_acc_icon_tol(v)` | `o.acc_icon_tol = v` |
| `set_acc_tols(k, e, i, b)` | `o.acc_kkt_tol = k; o.acc_econ_tol = e; o.acc_icon_tol = i; o.acc_bar_tol = b` |
| `set_div_kkt_tol(v)` | `o.div_kkt_tol = v` |
| `set_div_bar_tol(v)` | `o.div_bar_tol = v` |
| `set_div_econ_tol(v)` | `o.div_econ_tol = v` |
| `set_div_icon_tol(v)` | `o.div_icon_tol = v` |
| `set_div_tols(k, e, i, b)` | `o.div_kkt_tol = k; o.div_econ_tol = e; o.div_icon_tol = i; o.div_bar_tol = b` |
| `set_bound_fraction(v)` | `o.bound_fraction = v` |
| `set_bound_push(v)` | `o.bound_push = v` |
| `set_bound_interval_push(v)` | `o.bound_interval_push = v` |
| `set_bound_relax_factor(v)` | `o.bound_relax_factor = v` |
| `set_fixed_variable_treatment(v)` | `o.fixed_variable_treatment = v` |
| `set_alpha_red(v)` | `o.alpha_red = v` |
| `set_delta_h(v)` | `o.delta_h = v` |
| `set_incr_h(v)` | `o.incr_h = v` |
| `set_decr_h(v)` | `o.decr_h = v` |
| `set_hpert_params(d, i, r)` | `o.delta_h = d; o.incr_h = i; o.decr_h = r` |
| `set_print_level(v)` | `o.common.print_level = v` |
| `set_init_mu(v)` | `o.init_mu = v` |
| `set_min_mu(v)` | `o.min_mu = v` |
| `set_max_mu(v)` | `o.max_mu = v` |
| `set_neg_slack_reset(v)` | `o.neg_slack_reset = v` |
| `set_qp_threads(v)` | `o.common.threads = v` |
| `set_qp_pivot_perturb(v)` | `o.qp_pivot_perturb = v` |
| `set_qp_matching(v)` | `o.qp_matching = v` |
| `set_qp_scaling(v)` | `o.qp_scaling = v` |
| `set_qp_ref_steps(v)` | `o.qp_ref_steps = v` |
| `set_qp_par_solve(v)` | `o.qp_par_solve = v` |
| `set_obj_scale(v)` | `o.obj_scale = v` |
| `set_qp_ordering_mode(m) / (str)` | `o.qp_ord = m` |
| `set_opt_bar_mode(m) / (str)` | `o.opt_bar_mode = m` |
| `set_soe_bar_mode(m) / (str)` | `o.soe_bar_mode = m` |
| `set_opt_ls_mode(m) / (str)` | `o.opt_ls_mode = m` |
| `set_soe_ls_mode(m) / (str)` | `o.soe_ls_mode = m` |
| `set_best_criteria(m) / (str)` | `o.best_criteria = m` |
| `set_accel_pivot_tolerance(v)` | `o.accel_pivot_tolerance = v (Accelerate builds)` |
| `set_accel_zero_tolerance(v)` | `o.accel_zero_tolerance = v (Accelerate builds)` |

The four `strto_*()` parsers and the six string-taking setter overloads have no
replacement: name the enumerator. `InteriorPointSolver::strto_BarrierMode("LOQO")`
becomes `hven::solvers::BarrierModes::LOQO`.

### The rules `set_options()` adds

1. **Transactional.** `validate(o)` runs first. A throw leaves the previous
   options in force and the solver usable — never a half-applied value. This is
   a real change from the per-field setters, which validated one field at a
   time: a sequence that passed through an invalid INTERMEDIATE state (a
   tolerance tightened before its acceptable partner was) is now refused as a
   whole. Build the value, then hand it over once.
2. **Between solves only.** A replacement attempted from inside an iteration
   callback throws `std::logic_error`; the options do not move and the guard
   clears on the unwind, so the solve finishes and later replacements work. The
   previous behaviour — a setting written mid-call took effect on the NEXT call
   — is REPLACED by the refusal.
3. **The twelve backend-configuration fields are refused once a program is
   attached.** `qp_ord`, `qp_pivot_perturb`, `qp_ref_steps`, `qp_matching`,
   `qp_scaling`, `qp_pivot_strategy`, `qp_alg`, `qp_par_solve`, `qp_print`,
   `cnr_mode` and (on Accelerate) `accel_pivot_tolerance` /
   `accel_zero_tolerance` are read exactly once, inside `set_qp_params()`, which
   runs from `set_nlp()`. Changing one on an attached solver was SILENTLY INERT
   under the old setters; `set_options()` refuses it by name instead.

   **The recovery sequence, which is what the refusal message names.** "Re-attach
   the program after the replacement" is not on its own a route: the attachment
   is what refuses the replacement. The executable order is

   ```cpp
   auto saved = np;              // the shared_ptr you attached; there is no
                                 // accessor returning the attached program
   solver.release();             // nothing attached now
   solver.set_options(changed);  // accepted: nothing to be inert against
   solver.set_nlp(saved);        // re-transcribes under the new value
   ```

   or construct a fresh solver over the options you want. Pinned by
   `Options.IpmAnAttachOnlyFieldChangesThroughReleaseReplaceReattach`.

   **T8.4 CHANGES THIS RULE.** Once the solver borrows the model per call rather
   than holding it across calls, there is no "attached" state for a replacement
   to be inert against, and the twelve stop being a special case. Treat the
   refusal as a T8.3-era rule with a scheduled end, not as the permanent shape of
   the surface.

   **Every other field takes effect on the next solve**, including
   `fixed_variable_treatment` and `bound_relax_factor` (`run_phase_sequence()`
   re-applies them through `configure_variable_treatment()` at every entry) and
   `common.threads` -- with ONE exception worth knowing about.

   **`common.threads` is MIXED CADENCE.** Every solve entry refreshes the ordinary
   backend thread count onto the live factor, so a replacement between two solves
   reaches it. But `set_qp_params()` also derives `cnr_threads` from it at
   ATTACHMENT (`opts.cnr_threads = cnr_mode ? common.threads : 0`), and that
   number is not refreshed: a later thread-count change leaves the CNR count where
   attachment put it. That is not a new regression -- it is exactly what
   `KktFactorization`'s contract (`include/hven/detail/interior/kkt_factorization.h:71`)
   preserves on purpose -- but a per-solve classification of the field conceals it,
   so it is stated here. `cnr_mode` itself is one of the twelve and cannot change
   while attached at all.
4. **The constructor caps `common.threads` at this machine's core count**, which
   is exactly what the default constructor has always done. A count written
   AFTER construction is the caller's explicit word and is taken verbatim, as
   `set_qp_threads()` always did.

### What is pinned

`tests/drivers/test_options.cpp`: one representative per refusal class of the
old `Settings::validate()` body; the transactional rule; the
backend-configuration refusal and the `fixed_variable_treatment` counter-example;
the mid-solve `logic_error` with `options()` unchanged and a later solve still
green; every shipped preset's nine fields plus the defaults outside them; and an
unknown preset name. The interior corpus leg (`bench/ipm_corpus_leg.cpp`, 37
rows) re-captures byte-identically: its levers and its four abnormal variants are
`IpmOptions` field writes now, and its CSV does not move.

`tests/install_smoke/`: `include_common_options.cpp` and
`include_ipm_solver_types.cpp` — the two new public headers stand alone against
an install prefix (10 standalone TUs -> 12).

### Two test-visible behaviour changes, both declared

* `ObjectiveScaleReporting.ANegativeScaleIsRefusedAtBothDoors` is renamed
  `…IsRefusedByValidate`: there is one door now. The second door it named — the
  solve-entry check catching a scale written PAST the setter through the mutable
  `settings()` reference — has nothing left to catch, because that reference is
  gone. `run_phase_sequence()` still validates at entry as defense in depth.
* `ObjectiveScaleReporting.TheScaleACallRanAtIsTheScaleItsOutputsAreReportedOn`
  now pins the mid-call REFUSAL rather than the mid-call deferral. The
  observable outcome it was written for is unchanged: one call runs at one
  scale.

### The SQP engine's half

`SqpOptions` gains **one field, LAST**: `CommonOptions common`, at the SQP
engine's own defaults — `threads = 0` ("leave the backend alone", which is what
this engine has always done) and `print_level = 3` (silent, which is what this
engine has always been). **Neither is read in T8.3**: T8.7 gives this engine a
console table at `print_level` and T8.8 makes a non-zero `threads` reach every
factor path. `common.start_level` is carried beside `SqpOptions::start_level`,
which is still the field the driver caps a warm start with, until T8.10 folds the
two. `common` is last so that no existing field's offset moves.

`void validate(const SqpOptions &)` is the new name for the whole-value check;
its body is `validate_sqp_options`'s plus the two `common` checks. **`validate_sqp_options`
stays as a one-line forwarder** — every existing call site keeps compiling and
keeps meaning the same thing. T8.10 removes the old name.
`SqpOptions sqp_preset(std::string_view)` accepts `"default"` and refuses
anything else, listing the valid names.

`SqpDriver` gains `options()` and `set_options(SqpOptions)`, and its `engine_`
member moves behind a `std::unique_ptr<QpEngine>` — **only** because
`set_options()` has to replace it (a `QpEngine` owns a live backend session and
declares no assignment). The pointer is never null between constructor and
destructor.

`set_options()` on the driver is TRANSACTIONAL in four steps: `validate(o)`; then
a replacement `QpEngine` is constructed into a temporary from `o.qp` and given
the same ledger attachment **and the same solve counter**, so the record labels
keep counting rather than restarting at `<prefix>_qp_0`; then the swap, and the
lazily-built SSN and IPQP engines are dropped (each holds its own COPY of the
`QpOptions`, so dropping them is necessary and sufficient; the trace sink stays
on the driver and re-applies at the next first-use construction); then the
options are adopted. A throw at any point leaves the previous options AND the
previous engines in force.

**One rule, no fast path: a replacement with IDENTICAL options rebuilds too.**
What the rebuild costs is the driver's own cached K0 border. What it does NOT
cost is a hot handle's reuse — see the hot-reuse section below.

**Legal between solves only.** The guard sits at `solve_impl()`, the one point
all four public `solve()` overloads reach exactly once (they nest, so a
per-overload flag would double-set). A replacement from inside
`SqpOptions::make_strategy` — or any other hook that runs under the solve —
throws `std::logic_error`; the options do not move and the guard clears on the
unwind. The restoration phase builds a distinct nested driver, so it never
re-enters this driver's guard.

One test-visible change beyond the pins: `tests/sqp/test_qp_mode_sites.cpp`'s
shape matcher now accepts `->refine_on_face(` as well as `.refine_on_face(`. The
`unique_ptr` moved four call lines to the arrow spelling and the scan lost them;
its own ">= 10 kernel call sites" floor probe is what caught it.

### Hot-handle reuse is now keyed on the producing engine's options

**What changed.** `HotState` (the opaque payload behind `WarmStart::hot`) gains
one field, `std::uint64_t engine_options_hash`, and `QpEngine::run` adopts a
handle only when that stamp equals the adopting engine's own. The stamp is
`hven::solvers::options_fingerprint(const QpOptions &, int threads)` — a
field-wise hash over **all nine** `QpOptions` fields (`primal_delta`, `dual_mu`,
`feas_tol`, `opt_tol`, `max_iter`, `schur_cap`, `schur_cond_max`, `ws_algebra`,
`tr_radius`) plus the thread count in force. It is padding-safe (a field-wise
fold, never a byte hash) and hashes doubles by bit pattern.

**Why.** The existing reuse conditions (a)–(e) fingerprint the PROBLEM (the
structural and value hashes, the effective delta/mu, the working set) and the
FACTOR OBJECT (its `(session_id, epoch)` pair and its usable inertia). None of
them fingerprints the ENGINE OPTIONS the K0 was built under, so an engine with a
different `schur_cap`, `ws_algebra` or regularization would adopt a matching
handle and reuse a factorization built for other settings. That was a real hole
before this task; it is closed now.

**What it does NOT do.** It is not a per-instance key. Cross-engine adoption is
what the hot handle is FOR — `run()` consults a handle only when its own border
cache is invalid, so a driver's own second solve is kHot through that cache and
never looks at the handle at all. A fresh engine with the same options adopts a
foreign handle exactly as it always did, and every existing kHot pin
(`WarmStart.HotReusesFactorization`, `HotReuseIsNeverAnswerObservable`,
`LedgerFactorizationsSavedTracksHotVsDegradedWarm`, the poisoned-handle control,
`QpWarmStart.HotStateEmitsCommittedIdentityNotLive`) stands unchanged.

**What you may notice.** After `SqpDriver::set_options()`:

| replacement | the old handle |
|---|---|
| identical options | still adopted — kHot |
| any changed `qp` field | refused — kWarm by construction |
| changed `common.threads` | refused — kWarm by construction |
| a replacement that threw | never happened; the previous engine is still in force, and on a driver that has already solved it is that engine's OWN live cache -- not the handle -- that carries the next solve |

"kWarm by construction" is exact rather than a degradation: the refusal happens
at the adoption gate, before the reuse bookkeeping is snapshotted, so nothing was
adopted, `k0_reused` reads false and the DETACH branch is not reached. The values
and the working set still come from `seed`, which is independent of `hot`.

`common.threads` is hashed **from T8.3 although this engine carries rather than
applies the count until T8.8** — deliberately, so that a pin written against it
now means the same thing after T8.8 lands.

`QpEngine`'s constructor gains a defaulted second parameter,
`explicit QpEngine(const QpOptions &opts, int threads = 0)`, so every existing
construction site keeps compiling and keeps hashing the `0` the SQP lane has
always passed. `QpEngine::num_threads()` reports it.

Pinned in `tests/sqp/test_warm_start.cpp` (four new tests beside the kHot chain:
identical-options rebuild still adopts, changed `qp` refuses, changed
`common.threads` refuses, a failed replacement leaves reuse intact) and in
`tests/drivers/test_options.cpp` (the fingerprint is stable, and each of the nine
fields plus the thread count moves it when flipped alone).

### One ABI consequence of moving the mode enums (source-compatible, MANGLING-breaking)

The eight mode enums moved from `InteriorPointSolver`'s scope to `hven::solvers`,
with member aliases left behind. **Every spelling in your source still compiles
and still names the same type** — that is what the aliases are for. But an alias
is not the enum's name: the compiler mangles the CANONICAL one, so any function
whose signature mentions one of the eight now has a different mangled symbol.
Twenty-seven declarations are affected — the acceptance / mechanism / governor /
recovery interfaces are the bulk of them, e.g.

```
hven::solvers::GlobalizationMechanism::run_acceptance_backtrack(
    hven::solvers::InteriorPointSolver::LineSearchModes, …)   // before
hven::solvers::GlobalizationMechanism::run_acceptance_backtrack(
    hven::solvers::LineSearchModes, …)                        // after
```

**What this means for you: nothing, unless you link objects compiled against two
different hven header sets.** That is already broken by this task for a separate
reason (`InteriorPointSolver::Settings` no longer exists), and hven ships a
static library that consumers rebuild or re-install wholesale. It is recorded
here because it is the one change in T8.3 that is invisible at compile time and
visible at link time. Rebuild, do not mix.

The P-SYM roll for this task lists **43 unique ONLY-BEFORE names**, of which
**27 are these enum-signature rename pairs** — each with its canonicalized
counterpart in ONLY-AFTER; they are signature renames, not added or removed
behaviour. The other **16** are not renames at all: constructors, a QP
destructor, and renamed test infrastructure. (Corrected in fix round 1; the
addendum first said all 43 were enum renames.)

---

## T8.4 — one result core: `SolveResult`, `SolveBudget`, the shared declared diagnostics

This section grows commit by commit. The first commit ADDS a header and adds
nothing else: no existing declaration moved, no existing behaviour changed, and
nothing a consumer compiles today stopped compiling.

### The new header

`#include <hven/drivers/solve_result.h>` — a public header that names neither
engine, so a consumer can write one reporting function against both:

| type | what it is |
|---|---|
| `hven::solvers::SolveBudget` | `{Index minor_budget = 0; Index max_iterations = 0;}` — the per-call work ceiling both engines will take from T8.4's later commits. Both zeros mean "the engine's own options decide". |
| `hven::solvers::SolveResult` | the base every engine result derives from: `status`, `x`, `lambda_e`, `lambda_i`, `z`, `f`, the four shared diagnostics, `ce`/`ci`, `iterations`, `wall_seconds`, `export_warm_start()`. |
| `hven::solvers::DeclaredDiagnostics` | the four shared diagnostics as a value. |
| `compute_declared_diagnostics(...)` | their ONE definition, over the DECLARED problem in CALLER units. |

### The four shared diagnostics, defined

Over the declared problem, in the caller's units, from an evaluation the engine
ALREADY HOLDS — this function evaluates nothing:

- `stationarity` — inf-norm of `grad f + Je^T lambda_e + Ji^T lambda_i - z` over
  the declared coordinates that were measured.
- `feasibility_e` — inf-norm of `ce`.
- `feasibility_i` — inf-norm of the positive part of the declared inequality
  rows (`ci <= 0` is feasible) and of the declared bound violations.
- `complementarity` — inf-norm over `lambda_i o ci` and the **canonical** bound
  products `max(z,0) o (x - l)` and `max(-z,0) o (u - x)`.

**Why canonical.** A signed `z = zL - zU` cannot recover two separate prices at
a two-sided bound where both are positive. The shared diagnostic says so and
prices each side from the sign of `z`; an engine that holds both prices keeps
its own two-price measure on its own result (the interior-point engine's
`barr_inf`). Pinned by
`DeclaredDiagnostics.TwoSidedBoundUsesCanonicalSplit`.

**NaN means UNMEASURED**, everywhere in this core, and `ce`/`ci` are EMPTY
rather than zero when nothing was measured. Absent is never zero-filled.

**An excluded coordinate is not measured.** `excluded_coordinates` names the
coordinates the producing engine has no row for at all — the interior-point
engine's `MakeParameter` eliminations, whose reduced gradient reports `0` there.
A `0` meaning "no row" must not enter an inf-norm beside `0`s meaning
"stationary". Pinned by
`DeclaredDiagnostics.ExcludedCoordinateIsNotMeasured`.

### Two entry points, one arithmetic

`compute_declared_diagnostics(x, lambda_e, lambda_i, z, grad, Je, Ji, ce, ci,
lower, upper, excluded)` forms `grad f + Je^T lambda_e + Ji^T lambda_i` and
calls straight through to
`compute_declared_diagnostics_from_grad_lag(x, lambda_i, z, grad_lag, ce, ci,
lower, upper, excluded)`.

The second exists because **the interior-point engine has no separated gradient
or Jacobian at the returned iterate**: the model's derivatives are scattered
directly into one compound KKT buffer, fused there with the Hessian, the barrier
diagonals and any inertia perturbation, and left at the LAST EVALUATED iterate
rather than the reported one. What it does hold, in its right-hand side at the
returned iterate, is exactly `grad_lag`. A signature demanding `(grad, Je, Ji)`
would force it to evaluate the model again — moving `evals_full` on every solve.
The SQP's `SqpKkt` carries `grad_lag` outright for the same reason: forming it
twice is arithmetic with a chance of disagreeing.

### Source count

`src/CMakeLists.txt`'s `_hven_expected_source_count` moves **42 -> 43**
(`src/drivers/solve_result.cpp`), and the install smoke gains a thirteenth
standalone-include TU.

### The interior-point engine: one entry, the model borrowed, `IpmResult` by value

**Everything below is a source break.** Rebuild against the new header; there is
no deprecation shim.

#### The five phase-named entries become one, and the sequence becomes an option

| before | after |
|---|---|
| `Eigen::VectorXd optimize(x0)` | `IpmResult solve(model, x0)` with `opts.phases = {kOptimize}` (the DEFAULT) |
| `Eigen::VectorXd solve(x0)` | `opts.phases = {kSolve}` |
| `Eigen::VectorXd solve_optimize(x0)` | `opts.phases = {kSolve, kOptimize}` |
| `Eigen::VectorXd optimize_solve(x0)` | `opts.phases = {kOptimize, kSolve}` |
| `Eigen::VectorXd solve_optimize_solve(x0)` | `opts.phases = {kSolve, kOptimize, kSolve}` |

The conditional trailing solve those last two carried is not a property of the
entry any more. **The rule, for any sequence:** phases run in order; a `kSolve`
that FOLLOWS a `kOptimize` runs only if that optimize phase did not report
`kOptimal`; a `kOptimize` is never conditional; a verdict of `kDiverging` or
worse short-circuits the rest. `validate()` refuses an EMPTY sequence.

#### The program is an argument, borrowed for the call

| before | after |
|---|---|
| `InteriorPointSolver(std::shared_ptr<NonLinearProgram>)` | REMOVED — construct over options, hand the program to `solve()` |
| `set_nlp(np)` | REMOVED — `solve(model, x0)` |
| `release()` | REMOVED — nothing is held to release |
| `kkt_pattern_is_analyzed()` | `kkt_pattern_is_analyzed(const NonLinearProgram &model)` |

The solver keeps NO pointer to the program between calls: it is bound after the
argument checks and nulled on every exit, a throw included. The cross-call reuse
you had from `set_nlp()`-once, `solve()`-many is unchanged in EFFECT — the
symbolic analysis, the pattern hash and the partition setup all survive — but it
is now keyed on the program's own identity rather than on a retained pointer:
its structure key, its structure epoch, the fixed-variable treatment in force,
and that the analysis was laid against THIS program's tables. A different
program, a bumped epoch or a changed treatment re-transcribes; the same program
unchanged does not.

#### `result()` and the diagnostic accessors become fields on the returned value

| before | after |
|---|---|
| `solver.result()` | the value `solve()` returns |
| `result().iter_num_` | `r.iterations` |
| `result().obj_val_` | `r.f` |
| `result().converge_flag_` (`ConvergenceFlags`) | `r.status` (`SolveStatus`) |
| `result().primals_` | `r.x` |
| `result().eq_lmults_` / `.eq_cons_` | `r.lambda_e` / `r.ce` — **DECLARED rows only**, see below |
| `result().iq_lmults_` / `.iq_cons_` | `r.lambda_i` / `r.ci` |
| `result().bound_lmults_` | `r.z` — **DECLARED width**, see below |
| `result().kkt_inf_` and the other three | `r.kkt_inf`, `r.barr_inf`, `r.econ_inf`, `r.icon_inf` |
| the six timings, the `last_*` eleven, the factor/SOC/watchdog counters | same names, trailing underscore dropped |
| `result().reset_accumulators()` | RETIRED — a solve builds a fresh result; a default `IpmResult` carries every sentinel the reset wrote |
| `solver.kkt_analysis_count()` | `r.kkt_analyses_total` (plus `r.kkt_analyses_this_call`, new) |
| `solver.kkt_factor_counters()` | `r.kkt_factor_counters` |
| `solver.eval_error_log()` | `r.eval_error_log` |
| `solver.last_stop_reason()` | KEPT (a callback has no result yet); also per phase on `r.phases[i].stop_reason` |

**Two shapes changed, both toward the declared problem.**

- `r.lambda_e` / `r.ce` are the DECLARED equality rows exactly. Under
  `MakeConstraint` the treatment's internal fixing rows used to sit in the tail
  of that block; they are reported separately now, as
  `r.internal_fixed_lambda_e` / `r.internal_fixed_ce`.
- `r.z` is DECLARED-WIDTH always, where `bound_lmults_` was dense over the
  solver's REDUCED space and empty on a problem with no finite bounds. An
  eliminated coordinate reads `0`; under `MakeConstraint` a fixed coordinate
  reads `-lambda_fix`, the sign `grad f + J'lambda - z = 0` forces.

#### `hven::ConvergenceFlags` is gone

Both engines report `hven::solvers::SolveStatus`, and the interior-point engine
uses it internally too — so there is no mapping step left to get out of step
with the verdict it maps.

| before | after |
|---|---|
| `CONVERGED` | `SolveStatus::kOptimal` |
| `ACCEPTABLE` | `SolveStatus::kAcceptable` |
| `NOTCONVERGED` | `SolveStatus::kMaxIter`, or `kStalled` at the stall and locally-infeasible-restoration exits |
| `DIVERGING` | `SolveStatus::kDiverging` |
| `SINGULAR_KKT` | `SolveStatus::kNumericalError` |
| `operator<=>(ConvergenceFlags, …)` | `severity(SolveStatus)` — orders all nine, old five in old relative order |
| `to_solve_status(ConvergenceFlags, IpmStopReason)` | `resolve_ipm_phase_status(SolveStatus, IpmStopReason)` |

**A caller reading `r.status` no longer applies the split itself:** the engine
resolves each phase's verdict against that phase's own stop reason at the phase's
exit, so a stalled solve reports `kStalled` outright.
`resolve_ipm_phase_status` is idempotent and remains public for a caller
resolving a verdict it obtained some other way.

#### Two behaviour changes, declared

1. **The per-phase verdict.** The stop reason was phase-scoped and the verdict
   was per CALL, so a later phase that left without assigning one reported the
   EARLIER phase's answer beside this phase's reason (design §2.3 registered
   this). The engine resets the verdict at every phase start now. `r.phases`
   carries one `IpmPhaseReport{phase, status, iterations, phase_seconds,
   stop_reason, ran}` per REQUESTED phase, skipped ones included (`ran` tells
   them apart); `r.status` is the last RAN phase's and `r.iterations` the sum
   over the phases that ran.
2. **The machine trace's interior-point status vocabulary.** `ipm.solve.end`'s
   `status` field spells `SolveStatus` now: `converged` → `optimal`,
   `not_converged` → `max_iter` (or `stalled`, which the old vocabulary could
   not express at all), `singular_kkt` → `numerical_error`; `acceptable` and
   `diverging` are unchanged. Nothing pinned compares those bytes — unlike the
   corpus CSV, which keeps its capitalised spellings through a bench-local table
   for exactly that reason.

#### `set_options()`: the twelve-field refusal is gone

T8.3 REFUSED a change to any of the twelve transcription-time fields while a
program was attached, because they were read once inside `set_qp_params()`,
which ran from `set_nlp()`. There is no attachment now and `set_qp_params()`
runs from the solve that transcribes, so the change is ACCEPTED — and marked, so
the next solve re-transcribes under the new value rather than reusing an
analysis laid under the old one. `kkt_pattern_is_analyzed(model)` reads `false`
in between. Nothing is refused and nothing is silently ignored.

#### `SolveBudget` on the interior-point entry

`solve(model, x0, budget)`. `budget.max_iterations`, when non-zero, caps EACH
PHASE at `min(budget, opts.max_iters)` — a caller may tighten this engine's own
limit, never loosen it. `budget.minor_budget` is IGNORED and documented so: this
engine has no minor loop. The default `{}` is the identity, which is what makes
the feature trajectory-neutral on every existing path.

#### Warm-start staging: which refusal fires where

`stage_warm_start()` holds no program, so the AGAINST-THE-PROBLEM block-size
refusal moved to solve entry (where it sits with the stamp check, and where the
staged value is consumed either way). What still refuses at the STAGING call is
everything internal to the payload: a non-finite number, an unreadable polish
extension, an extension whose blocks disagree with the core blocks beside them.
Messages are unchanged. (T8.5 replaces staging with an argument entirely.)

#### `NLPSolver`

Its five entries keep their names and now return `SolveStatus`. It gained
`result()`, returning the last solve's whole `IpmResult` — a WRAPPER
accommodation, not the engine's: `InteriorPointSolver::result()` is what made a
finished solve readable from a solver that had gone on living, and this class's
own entries return only a status. It is retired in T8.9 and this goes with it.
`NlpSolveOutput::eq_lmults_` is the DECLARED block now (see the shape note
above), which is what `return_multipliers()` composes over anyway.

### The SQP engine: `SqpResult`, `SolveBudget`, and `SqpStatus` removed

`SqpSolution` is now an alias for `SqpResult`, which derives from `SolveResult`.
**The type name still works**; what moved is where some of its fields live and
what three of them are called.

#### `hven::solvers::SqpStatus` is gone

Its five enumerators exist on `SolveStatus` UNDER THE SAME NAMES, so the
substitution is textual and name-for-name: `SqpStatus::kOptimal` →
`SolveStatus::kOptimal`, and so on. `to_solve_status(SqpStatus)` went with it —
the mapping was the identity on the names, so there is nothing left to map.

**Correction (fix1, 2026-09-08):** an earlier revision of this section said the
five "ARE the first five of `SolveStatus`". They are not — `SolveStatus` runs
`kOptimal, kAcceptable, kMaxIter, kInfeasible, kStalled, kDiverging,
kNumericalError, kBudgetExhausted, kInterrupted`, so `kNumericalError` and
`kBudgetExhausted` sit at ordinals 6 and 7. **The substitution is by NAME and
never by ordinal**; anything that persisted, serialized or switched on the old
enum's integer values has to be re-derived by name.

`SolveStatus` and `IpmStopReason`, with `to_string()` and `severity()`, live in
`<hven/core/solver_status.h>` — the bottom tier, where `SqpStatus` lived, and
where `core/ledger.h` can report a status without reaching up a tier.
`<hven/drivers/solve_status.h>` is what is left: `resolve_ipm_phase_status()`,
a statement about one engine's exits.

**`to_string()` spells all nine in LOWER SNAKE**, where `to_string(SqpStatus)`
spelled its five capitalised (`Optimal`, `MaxIter`, …). Three consequences:

- The printed SQP status line reads `Status: optimal`. Declared.
- The corpus and crossover CSVs KEEP the capitalised vocabulary, through a
  bench-local `corpus::legacy_status_string(SolveStatus)`: their committed
  baselines and the t10b control are pinned to those bytes, and moving column 7
  of every row for a cosmetic rename is not this task's business. The lower-case
  rename of that vocabulary is REGISTERED as a declared re-derivation of four
  baselines and ~10 pins.
- The machine trace does not move at all: `to_json(SqpStatus)` already spelled
  its five exactly as `to_string(SolveStatus)` does, so the two folded into one
  overload with no byte changing.

#### Three engine measurements renamed

| before | after | why |
|---|---|---|
| `sol.stationarity` | `sol.sqp_stationarity` | the base now has a `stationarity` with a DIFFERENT definition |
| `sol.feasibility` | `sol.sqp_feasibility` | the base has `feasibility_e` / `feasibility_i` |
| `sol.complementarity` | `sol.sqp_complementarity` | the base now has a `complementarity` with a different definition |
| `sol.wall_seconds` | `sol.solve_impl_seconds` | the base's `wall_seconds` is a DIFFERENT boundary |

**Read this before repointing anything.** The base's `stationarity`,
`feasibility_e`, `feasibility_i` and `complementarity` are over the DECLARED
problem in CALLER units, computed by one shared definition both engines feed.
The `sqp_*` fields are what THIS engine's convergence test gated on, in the
space it ran in. A tolerance comparison must use the `sqp_*` ones; a comparison
against another engine's solve of the same problem must use the shared ones.
`kkt_residual` is unchanged and is still `max(sqp_stationarity, sqp_feasibility)`.

`solve_impl_seconds` keeps its OLD BOUNDARY exactly — `solve_impl` alone, never
model construction, the seam lay, the ingest or the ledger. The base's
`wall_seconds` is the whole public call, so it is never the smaller of the two.

#### `SolveBudget` replaces `Index minor_budget`

```cpp
sol = driver.solve(model, x0, warm, 20);                       // before
sol = driver.solve(model, x0, warm, SolveBudget{20});          // after (a MINOR budget)
sol = driver.solve(model, x0, warm, SolveBudget{.minor_budget = 20});  // the same, spelled
```

An aggregate has no converting constructor, so **every** call that passed a
fourth argument changes: 19 sites in 7 files in this repository, and any of
yours. `budget.minor_budget` is the probe budget verbatim. `budget.max_iterations`
is new: when non-zero it gives an EFFECTIVE major cap of
`min(max_iterations, opts.max_iter)` — tightening only — and **restoration is
budgeted from that cap**, at all three places the engine reads its major limit
(the exit conjunction, the restoration refusal, the restoration sub-driver's
budget). `SolveBudget{}` is the identity, so a call that passed no budget behaves
exactly as before.

**Positional order is `{minor_budget, max_iterations}`.** Use the designated
form for anything but a bare minor budget.

#### The base's fields on an SQP result

`iterations` is `counters.major_iters` of the TOP-LEVEL solve (nested
restoration majors stay in `counters`). `ce`/`ci` are the declared constraint
residuals at the returned point. `export_warm_start()` carries the shared
currency snapshot; `warm_start` is still the engine-native one. The four shared
diagnostics come from a STASHED evaluation — the one the engine already held at
the point it returns — and **no exit takes a fresh evaluation to fill them**, so
`counters.evals_full` is unchanged on every path. Where no finite evaluation of
the returned point exists (the non-finite-start exit), all four are NaN and
`ce`/`ci` are empty.

#### tycho's break list

`tycho/src/solvers/engines.cpp` reads ten fields off `SqpSolution`. Four move:

| tycho reads | after | same or changed meaning |
|---|---|---|
| `sol.stationarity` → `report.kkt_residual_` | `sol.sqp_stationarity` | SAME quantity, new name. Do NOT leave it pointed at `stationarity`, which still compiles and now means the DECLARED-space diagnostic. |
| `sol.feasibility` | `sol.sqp_feasibility` | same quantity, new name |
| `sol.complementarity` | `sol.sqp_complementarity` | same quantity, new name |
| `sol.wall_seconds` → `report.wall_time_s_` | `sol.solve_impl_seconds` (same value) or `sol.wall_seconds` (the new, larger, whole-call measurement) | CHANGED VALUE if left as written — decide which boundary the report means |
| `sol.status` (a `switch` over five) | `SolveStatus`, NINE enumerators | the switch needs a `default:` or the four new cases; the five it handles keep their names |
| `counters.*`, `f`, `infeasibility_certified` | unchanged | — |

The first three still COMPILE if left alone only in the sense that
`stationarity` and `complementarity` exist on the base — which is precisely why
they are listed: silently reading a different quantity is the failure mode.

### The interior-point leg's artifact: a declared re-derivation

`bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv` is re-derived, and
the reasons are in the file's own provenance header. In short:

- The `status` column takes the new vocabulary: `CONVERGED` → `optimal`,
  `NOTCONVERGED` → `max_iter` at the two cap rows and `stalled` at the two
  abnormal ones. **The last two rows are now identical in `status` and are told
  apart by `stop_reason` alone**, so the column T8.2 added is load-bearing in
  this artifact from here on.
- Outside that column, **T8.2's 37 rows did not move**: `compare_replay.py`
  between the two files with column 6 cut reported `37 / 18 / 0`.
- The schema gains eleven columns — `phase_count`, `phases_ran`, `phases`
  (the packed per-phase account), the four declared block widths, and the four
  shared declared diagnostics — and two rows, the `/solve_optimize`
  ({kSolve, kOptimize}) variant on one F7 cell and on HS071.
- `# schema:` reads 31.

If you read this artifact, three columns are new and load-bearing and one is
newly so: `phases` (packed as `kSolve:optimal:5|kOptimize:optimal:7`, with
`:skipped` for a conditional phase that did not run), the four `*_size` widths,
the four shared diagnostics, and `stop_reason`.

### T8.4 fix round 1 — six contract changes a consumer can see

Round 1 of the review fixes changes six things that are visible from outside the
library. Each is a correction, not a new feature; each is pinned.

**1. Declared `stationarity` is NaN after a feasibility-only interior-point
phase.** The `kSolve` phase evaluates the model with the objective scaled to
zero and then zeroes the primal gradient blocks outright, so the right-hand side
it leaves behind carries no declared objective gradient. A declared stationarity
computed from it measures something else, and T8.4 reported it as measured — the
committed leg printed `0.000000000e+00` for a problem whose honest declared
stationarity is at least 1. The rule now:

> **The four shared diagnostics are reported only from an evaluation that is
> (a) objective-bearing, (b) at the returned point and (c) taken with
> restoration inactive. Failing (a) makes `stationarity` alone NaN — the other
> three read `ce`, `ci`, `x`, the box, `lambda_i` and `z`, all of which a
> feasibility phase does evaluate. Failing (b) or (c) makes all four NaN, and
> failing (c) additionally EMPTIES `ce` and `ci`, because nested restoration
> replaced those rows with its own condensed residuals.**

`f` is unaffected: every non-optimality exit assembles the true objective at the
returned primals.

**2. A `return_best` interior-point exit reports the BEST iterate's objective.**
`IpmResult::f` used to come from the last iterate while `x`, the multipliers and
the residuals came from the best one.

**3. The SQP's shared diagnostics describe the RETURNED duals.** They are taken
at the exported `lambda_e`, `lambda_i` and `z` — post-sign-sweep, and at the
restoration certificate's bound price where that is what leaves. Where the
prices moved after the measurement they were taken from and no evaluation at the
exported ones exists, `stationarity` and `complementarity` are NaN while
`feasibility_e`, `feasibility_i`, `ce` and `ci` — which read no price — stand.
The engine's own `sqp_*` columns keep their pre-sweep disclosure; **the shared
contract does not inherit it**, and the note that said it did is gone from
`sqp_types.h`.

**4. `SolveBudget` on every public overload.** New:
`SqpDriver::solve(const NlpModel &, const Vec &x0, SolveBudget)` and
`SqpDriver::solve(NlpModelAggregate &, const Vec &x0, SolveBudget)`. The cold
and bridge entries hardcoded `SolveBudget{}` before, which left a STAGED warm
start with no budgeted door at all (the warm-start overloads refuse to run
beside a staged value). On the interior-point side a large `max_iterations` is
now clamped in `Index` BEFORE it is narrowed to `int`: `1 << 32` used to narrow
to 0 and skip the loop.

**5. `wall_seconds` starts at the public entry, on every overload of both
engines.** The SQP's bridge lay and seam lay are inside it; the interior-point
engine's starts in `solve()` immediately after the start-point size check, which
also MOVED there from inside `run_phase_sequence` — a mis-sized `x0` is now
refused before any transcription runs, with the same message.

**6. The analysis-identity token is an owner id, not an address.**
`InteriorPointSolver::kkt_pattern_is_analyzed` and the cross-call reuse path
compare a process-unique, never-reused id issued at each analysis and recorded
on the program (`NonLinearProgram::analyzed_owner_id()`), instead of the
captured KKT value-array address. A solver can die while the program it analysed
lives on, and a later solver's buffer can land on the freed allocation; an id
that is never reused cannot be coincided with. Nothing a consumer writes
changes — the query's answer is the one that gets safer. The MakeConstraint
fixed coordinate's bound price (`z = -lambda_fix`) also now appears in the
EXPORT snapshot, which carried 0 there while the result carried the price.

---

## T8.5 — warm start: one payload protocol on both engines

Staging is gone from both engines. A warm start is an ARGUMENT to the solve it
applies to, and there are TWO of them, with two different jobs.

### 1. The two identities

| | the SHARED PAYLOAD | the SQP's NATIVE object |
|---|---|---|
| type | `hven::solvers::WarmStartData` (`warmstart/warm_start_data.h`) | `hven::solvers::SqpWarmStart` (`warmstart/sqp_warm_start.h`, **new home**) |
| entries | `IpmSolver`* and `SqpDriver`, model- and bridge-taking | `SqpDriver` only — **labelled SQP-only**, model- and bridge-taking |
| carries a declaration stamp | YES | no |
| serializable / crosses engines | YES | no (holds a process-local hot handle) |
| highest level reachable | **per engine** — on the SQP `kSeeded` (it carries structure hash 0 by construction); on the interior-point engine the WHOLE payload applies at a `kWarm`/`kHot` ceiling (§5) | `kHot` |
| identity mismatch | **REFUSES** — `std::invalid_argument` | nothing to refuse: it claims no identity |
| pattern mismatch | n/a on the SQP (always mismatched, hence THAT engine's kSeeded cap); the interior-point engine reads no structure hash from a payload | DEGRADES to `kSeeded` |
| value defects | DEGRADE (clamp band, floor/cap), counted — **except a NON-FINITE core block, which REFUSES at the hand-over** | DEGRADE, counted, non-finite included (`kCold`) |

\* the class is still spelled `InteriorPointSolver` until group 2's rename.

**Correction (fix1, 2026-09-09).** The first version of this table gave one
"highest level reachable" for the payload column, `kSeeded`, and one "value
defects DEGRADE" rule. Both were the SQP's, stated as if they were the
protocol's. The kSeeded cap is the SQP's structural-hash ceiling and does not
apply to the interior-point engine, which applies a whole payload at `kWarm`
(§5's ladder is the authority); and a non-finite value in a PAYLOAD's core
blocks is refused on both engines rather than graded down — only the NATIVE
route degrades one.

`SqpWarmStart` is the same struct `detail/warmstart/warm_start.h::WarmStart`
always was, moved to a public header. **`WarmStart` still names it** — that
header keeps `using WarmStart = SqpWarmStart;` until T8.10 — so no existing call
site had to change. New code should say `SqpWarmStart`.

### 2. Staging → the argument form

| before | after |
|---|---|
| `sqp.stage_warm_start(p); sqp.solve(model, x0);` | `sqp.solve(model, x0, p);` |
| `sqp.stage_warm_start(p); sqp.solve(bridge, x0);` | `sqp.solve(bridge, x0, p);` |
| `sqp.stage_warm_start(p); sqp.solve(bridge, x0, budget);` | `sqp.solve(bridge, x0, p, budget);` |
| `ipm.stage_warm_start(p); ipm.solve(model, x0);` | `ipm.solve(model, x0, p);` |
| `ipm.clear_staged_warm_start();` | *(delete the line — a payload lives exactly as long as its call)* |
| `ipm.set_initial_multipliers(eq, iq); ipm.solve(model, x0);` | `ipm.solve(model, x0, seed);` where `seed` is a `WarmStartData` with an EMPTY `primal_`, `eq_lmults_ = eq`, `iq_lmults_ = iq` and the program's `declaration_key(...)` as its stamp — see §4 |
| `ipm.clear_initial_multipliers();` | *(delete the line)* |
| `ipm.export_warm_start()` | `result.export_warm_start()` — a `std::optional<WarmStartData>` on the value `solve()` returned (`drivers/solve_result.h`), engaged whenever the capture succeeded |
| `SqpDriver::export_warm_start()` | **unchanged** — the SQP driver keeps its own export entry this task |

**tycho's two sites** (`engines.cpp:504` interior, `:611` SQP) become
`engine.solve(bridge, x0, *warm)` / `driver.solve(model, x0, *warm)`. `:513`'s
`engine.result()` was already deleted in T8.4; its staging call joins that flip.

**`NLPSolver` keeps its surface.** The jet wrapper still honours
`NLPProblem::starting_multipliers()`; internally it now builds the
multipliers-only seed and passes it as an argument. It is retired whole in T8.9.

What actually moved inside it is `apply_starting_multipliers()`, which became
`starting_multiplier_seed()` returning a `std::optional<WarmStartData>` — NOT a
`stage_warm_start` entry, which this class never had (the brief and the SQP
lane's pre-read both named one; there was none).

**`run_nlp_solver` keeps BOTH arities** (fix1). The three-argument form
`run_nlp_solver(mode, input, seed)` is the one the wrapper's own `run()` calls;
the two-argument `run_nlp_solver(mode, input)` is the pre-T8.5 signature,
forwarding with `std::nullopt`. An existing caller of the two-argument entry
compiles and behaves exactly as before — without it, T8.5 was an undeclared
break of a public entry.

**Removed with no replacement:** `SqpDriver`'s two-warm-sources refusal. A call
names exactly one warm-start source — its own argument — so there is no second
source to contradict, and the `std::invalid_argument` that named both is gone.

### 3. The kIpm refusal — **M5 ruling 4 is RETIRED for this case**

Under `SqpOptions::qp_mode == QpMode::kIpm`, a payload whose block lengths or
declaration stamp did not match was COLD-GRADED — silently discarded, the solve
running cold — where `kWalk` and `kSsn` threw. **It now throws in every mode**,
with the same message.

The owner's reason: the old shape made an IDENTITY answer depend on which QP
KERNEL the driver happened to be configured for, so the same payload against the
same problem was refused or silently discarded according to something identity
has nothing to do with. Pattern and value defects still degrade, in every mode,
exactly as before — only identity refuses.

**What this can break for you:** a kIpm caller that handed over a stale payload
and relied on the solve running cold now gets an exception. Catch it, or check
the stamp before you hand it over. The two tests that pinned the old behaviour
are rewritten as declared flips
(`SqpWarmCurrency.KIpmRefusesAWrongSizedPayloadLikeKWalk`,
`KIpmRefusesAStampMismatchLikeKWalk`).

### 4. The multipliers-only seed

A `WarmStartData` whose `primal_` is EMPTY is the SEED FORM: it carries
multipliers and nothing else, and **`x0` is the start**.

- **Hand-over rule:** `primal_` and `bound_lmults_` are EITHER BOTH EMPTY OR
  ONE LENGTH. `eq_lmults_`/`iq_lmults_` must always match the declared row
  counts. An empty `primal_` beside a populated `bound_lmults_` is REFUSED where
  you stand — it claims bound prices at a point the payload does not name.
- **On the SQP** it resolves `kSeeded`. It did NOT before: an empty `primal_`
  failed the plausibility gate, the object resolved `kCold`, and the multipliers
  were silently DROPPED. That is behaviour change (5).
- **On the interior-point engine** the multipliers go through the same install
  site, clamps and objective-scale handling the removed
  `set_initial_multipliers()` fed.
- **A polish extension on a seed is IGNORED and COUNTED** — behaviour change
  (13). Its bound duals and inequality values are stated at the EXPORTER's
  point, and the solve stands at `x0`. The count is
  `SqpCounters::polish_ignored` / `IpmResult::polish_ignored`.
- **`z` is never ingested from any seed on the SQP** — today's rule, restated
  because the payload carries a `bound_lmults_` block that looks ingestable.

### 5. `common.start_level` on the interior-point payload route — four rungs

`IpmOptions::common.start_level` was carried unread by that engine until now. On
the payload route it is a CEILING, never a floor:

| rung | what applies | counters |
|---|---|---|
| `kCold` | NOTHING. The call is the solve it would have been with no payload. | `payload_ignored == 1` |
| `kSeeded` | the MULTIPLIERS only; the point and the polish are dropped | `polish_ignored == 1` when an extension was present |
| `kWarm` | the WHOLE payload — point, multipliers, polish | both 0 |
| `kHot` | IDENTICAL to `kWarm` — this engine has no hot handle to adopt | both 0 |

**Identity is checked at EVERY rung, `kCold` included.** A ceiling says what of a
payload to apply; it never says a payload describing a different problem is
acceptable.

`IpmResult` gains the two `int` counters above. `SqpCounters` gains
`polish_ignored`, **appended last**, so every existing field offset is unmoved —
and the `sqp.solve.end` trace line's counters object gains one key at the end,
between `near_active_peak` and `ssn`. Declared; the two golden lines in
`tests/sqp/test_trace_writer.cpp` are re-derived. The 76-column corpus CSV does
**not** gain it.

**Correction (fix1, 2026-09-09) — the trace carried the wrong value.** In the
first version of T8.5 the `sqp.solve.end` line was emitted BEFORE the driver
assigned `polish_ignored`, so a multipliers-only payload carrying a polish
extension produced `"polish_ignored":0` in the trace beside a returned result
and a ledger record that both said `1`. If you consumed a T8.5 trace between
`f836d74` and this fix, that key is unreliable in it; the returned result and
the ledger were always right. Fixed by assigning before the emission, and pinned
end to end by
`WarmProtocol.ThePolishIgnoredCountAgreesAcrossTraceResultAndLedger`.

### 6. The seeding constants moved

`#include <hven/warmstart/seeding.h>` now owns all three, **with their values
deliberately NOT unified** — three policies, three derivations:

| constant | value | whose |
|---|---|---|
| `hven::solvers::kSeededIqMultFloor` | `1e-8` | interior-point: the lower end of the interior the barrier method is defined on |
| `hven::solvers::kSeededMultInitMax` | `1e6` | interior-point: the magnitude ceiling on a seeded multiplier |
| `hven::solvers::kSeededDualClampTol` | `1e-6` | SQP: the sign band inside which a slightly negative inequality price is a rounding artefact |

Both old spellings still work. `kSeededDualClampTol` was already at this
namespace scope and is simply defined elsewhere now;
`InteriorPointSolver::kSeededIqMultFloor` and `::kSeededMultInitMax` are ALIASES
of the namespace-scope constants until T8.10 drops them.

### 7. Overload resolution — spell the type

The set is unambiguous for every call that names its argument's type: neither
`SqpWarmStart` nor `WarmStartData` converts to the other, and `SolveBudget`
converts from neither, so an lvalue of either selects its own overload. The ONE
ambiguous SPELLING is a BRACED third argument —

```cpp
driver.solve(bridge, x0, {});   // ambiguous: SolveBudget? WarmStartData? SqpWarmStart?
```

— which is a compile error, not a silent precedence. No such call exists in hven
or in tycho. Write `SolveBudget{}` (or the type you meant).

### 8. New public headers, and the install surface

`warmstart/sqp_warm_start.h` and `warmstart/seeding.h` are installed by the
existing header glob, so `check_export_contract.sh` needed no change.
`sqp_warm_start.h` holds a `WorkingSet` BY VALUE, which pulls
`detail/qp/working_set.h` onto the installed surface with it; the proof that the
glob actually shipped what it needs is the two new standalone install-smoke TUs
(`include_sqp_warm_start.cpp`, `include_seeding.cpp`), compiled against the
installed prefix the way a consumer sees it. `HotState` stays a forward
declaration.

---

## T8.6 — the iteration callback

Both engines now take ONE per-iteration callback, over one event type, and both
can be told to stop. The interior-point engine's two old callbacks split: the
LATE one is replaced by the shared callback, the EARLY one is renamed and
labelled as what it is — the one interior-point-only extension of the shared
surface.

### 1. The setter table

| before | after | notes |
|---|---|---|
| `ipm.set_late_callback(f)` | `ipm.set_iteration_callback(f)` | different signature — §2 |
| `ipm.disable_late_callback()` | `ipm.clear_iteration_callback()` | CLEARS rather than disarms |
| `ipm.set_early_callback(f)` | `ipm.set_kkt_hook(f)` | **same signature, same semantics** |
| `ipm.disable_early_callback()` | `ipm.clear_kkt_hook()` | now also clears the stored hook |
| `InteriorPointSolver::EarlyCallBackType` | `InteriorPointSolver::KktHook` | same `std::function` type |
| `InteriorPointSolver::LateCallBackType` | *(deleted)* | `hven::solvers::IterationCallback` |
| *(nothing)* | `sqp.set_iteration_callback(f)` / `sqp.clear_iteration_callback()` | new on the SQP |

`hven::solvers::IterateInfo` leaves the **callback** surface with
`LateCallBackType`. It does **not** leave the public surface: it is the
interior-point engine's own per-iteration record, and it remains the TRACE row
type in `drivers/trace.h`, handed out by reference as
`IpmIterTraceEvent::iterate` exactly as the IPQP's trace event types are handed
out. What changed is that nothing on the shared CALLBACK surface hands it out
any more.

So a consumer that genuinely wants the engine's own residual columns
(`kkt_inf`, `barr_inf`, `econ_inf`, `icon_inf`, the alphas, the inertia ladder's
counts) attaches a `TraceSink` and reads `IpmIterTraceEvent::iterate` — one
event per iterate, the same rows the callback sees.
`tests/interior/test_nlp_solver.cpp`'s
`TheReportedKktResidualsDescribeTheIterateTheResultDescribes` is the worked
example of that substitution.

**Setting or clearing from inside a callback is safe.** `set_iteration_callback`,
`clear_iteration_callback`, `set_kkt_hook` and `clear_kkt_hook` all DEFER when
called while the callable they replace is on the stack: the stored
`std::function` is left alone until the invocation returns, and the change is
applied at that point (or, if the callback departed by throwing, at the next
solve entry). A callback may therefore disarm itself and go on touching its own
captures. `disable_early_callback()` gave that safety for free by only flipping
a flag; `clear_kkt_hook()` clears the stored hook, so the rule is now explicit
rather than incidental.

`set_kkt_hook` is IPM-only and stays that way. Nothing equivalent is invented
for the SQP engine: the hook hands out the assembled, not-yet-factorized KKT
matrix of the interior-point Newton system, which is a structure the SQP engine
does not have.

### 2. The event

```cpp
enum class CallbackAction { kContinue, kStop };

struct IterationEvent {
    Index iteration;
    std::optional<Index> phase;    // interior-point only
    std::optional<Index> depth;    // SQP only
    double f, stationarity, feasibility_e, feasibility_i, complementarity, step_norm;
    std::optional<double> radius;  // SQP only
    std::optional<double> mu;      // interior-point only
    Eigen::Ref<const Vec> x, lambda_e, lambda_i, z;   // BORROWED, valid for the call only
    double elapsed_seconds;
};
using IterationCallback = std::function<CallbackAction(const IterationEvent &)>;
```

**At `depth` 0** — every interior-point event and every top-level SQP major —
all of it is in **declared space and caller units**, exactly as `SolveResult`
is: the interior-point engine's reduced primal space is scattered back, its
internal fixing rows come off the equality block and reappear in the bound price
as `z = -lambda_fix`, and a scaled SQP solve's prices carry no factor of the
engine's. The four diagnostics are the four SHARED ones of
`drivers/solve_result.h`, by the same definition, and **NaN means unmeasured**
— never zero. Neither engine evaluates anything to build an event, so attaching
a callback cannot move an evaluation counter.

**At `depth` 1 the space is the restoration sub-problem's, not the caller's.**
A depth-1 event is the feasibility sub-solve's own row: `x` is that problem's
variable vector (whose width need not be the caller's `n`), `f` is its
feasibility objective, and the four diagnostics are measured on it. The
forwarder stamps `depth` and maps nothing else back — the design's rule is that
depth-1 rows are the sub-solve's own. A caller comparing events against its own
model, or against `SqpResult`'s fields, filters on `depth == 0`.

**`f` obeys the same provenance rule the four diagnostics do.** Because an event
evaluates nothing, the interior-point engine reports `f` as NaN wherever it has
not evaluated the caller's objective at the event's point: on a FEASIBILITY
phase (`IpmPhase::kSolve`, which never evaluates a declared objective) and while
feasibility restoration is active (where the engine's own objective is the
restoration subproblem's). `IpmResult::f` still reports a number on those
exits — it evaluates for one at the exit — so the event's NaN is a statement
about what an event may claim without evaluating, not about the solve.

| field | interior-point | SQP |
|---|---|---|
| `iteration` | the per-phase iterate index (`ipm.iter`'s own) | the major's index, which is the row's index in `SqpResult::history` |
| `phase` | index into `IpmResult::phases` | `nullopt` |
| `depth` | `nullopt` | restoration nesting: 0 at the top, 1 inside a restoration sub-solve. A non-zero depth changes the SPACE the whole event is in — see above |
| `radius` | `nullopt` | `SqpIterate::tr_radius` |
| `mu` | the barrier parameter the iterate was EVALUATED under, on the caller's objective scale | `nullopt` |
| `step_norm` | inf-norm of the primal block of the committed `alpha * DXSL`; **0 at a phase's first iterate** | `SqpIterate::step_norm` |
| `f`, the four diagnostics | from the iterate's own measurement, under the result's own provenance rule (NaN where unmeasured) | from the ROW's own measurement — the point, the prices and the two constraint blocks are all snapshotted when the row is measured, so a row pushed after a restoration return still describes the point it was measured at |

The four vector views alias the engine's own storage and are valid **for the
duration of the call only**. Copy what you need; copying the EVENT does not
deepen them.

The old late callback's `int` return was ignored at both of its sites. The new
one's return is READ (§4), so a callback ported by dropping `return 0;` in
favour of `return CallbackAction::kContinue;` behaves exactly as before, and one
that returns `kStop` changes the solve.

### 3. WHERE IT FIRES — and the one behaviour change on the interior-point engine

**SQP** — from the single site that emits a history row, after the `sqp.major`
trace line and before the push. One event per row, `history.size()` of them,
rejected trials and the non-finite-start exit included.

**INTERIOR-POINT** — one event per `ipm.iter` row, at the **top of the iteration
that row belongs to**, after that iterate's residuals have been measured and
BEFORE its KKT matrix is factorized. The exits that leave from BELOW that
statement fire a TERMINAL event of their own: the convergence-check early exit,
the pre-factorization interrupt exit, and the restoration-locally-infeasible
door. Each fires it AFTER any `return_best` substitution has chosen the point,
so the last event describes the point the phase HANDS BACK.

The one case where the last event is not the returned point: a `return_best`
solve that ends at the BOTTOM-of-loop terminal conjunction and substitutes.
There the returned iterate is the best one — which an EARLIER event described,
since every iterate gets one — rather than the last.

That placement is the behaviour change (design §2.7 item **(3)**, "the IPM's
continuing callback dispatch observes the committed point"; item (2) is
`kInterrupted` on both engines). The late callback fired at the BOTTOM of the
iteration, below the factorization and the line search; the new event fires
above them. **The point it describes is the same point** — the iterate the
iteration starts from, which is the point committed by the previous step — and
the iterate index is the same number, so a callback that recorded
`IterateInfo::iter_` or read the terminal row's index sees no difference. What
DOES move:

* the event now carries the diagnostics of the COMMITTED point coherently: `x`,
  the prices, `f` and the four diagnostics are all of one iterate;
* anything a mid-iteration step assigns — the barrier parameter's own update,
  and the restoration entry's `mu <- entry_mu()` — is visible on the NEXT
  event, not on the one whose iteration performed it.
  `tests/interior/test_ipm_warm_start.cpp`'s
  `ARestorationEntryZeroesTheEqualityMultipliersAndRaisesMuToTheEntryFloor`
  reads `late_mu[1]` where it used to read `late_mu[0]`, and says so;
* a callback armed from INSIDE another callback now sees its first hand-out one
  site earlier. The KKT hook's own arming rule is unchanged.

The trace is unmoved with ONE addition: the restoration-locally-infeasible exit
now emits the `ipm.iter` line for the row it returns. That exit keeps the record
it pushed (`IpmResult::iterations` counts it) but emitted no line for it and
fired no event, so the stream was one row short of the iterations the result
reported. A consumer counting `ipm.iter` lines on a solve that ends
`IpmStopReason::kRestorationLocallyInfeasible` sees one more line than before;
no CSV column, counter or golden moves, because no artifact traces that exit.
Everywhere else `ipm.iter` and `sqp.major` are emitted exactly where they were,
and the event count equals the row count on both engines.

### 4. `kStop`, per engine

Returning `CallbackAction::kStop` asks the engine to stop. Both then report
`SolveStatus::kInterrupted` — which was unreachable before this task — at the
point they were standing on, with the ordinary cleanup and the ordinary trace
end event. **The ledger record is the SQP's alone today**: the interior-point
engine has no `attach_ledger`, and T8.7 adds one (with `IpmSolveRecord` and
`Ledger::ipm_records()`). An interrupted SQP solve writes its `sqp.solve` record
exactly as any other terminal exit does.

**SQP.** The stop is LATCHED and honoured at the next major's exit conjunction,
which sits above `build_subproblem`: no QP is solved for an answer the caller no
longer wants, and the identity `history.size() == counters.major_iters + 1`
holds exactly as it does on a capped solve.

**Where the stopped solve ends up.** A stop returned on row `k` is honoured at
row `k+1`'s exit conjunction, so the solve returns AFTER row `k+1`:
`history.size() == k + 2` and `counters.major_iters == k + 1`. The point handed
back is the one the LAST event describes — the committed point row `k`'s
accepted step reached, measured once and stepped from never. It costs one
derivative refresh and NO subproblem: the exit conjunction sits above the
rebuild, which is exactly where the probe budget stops a solve too.

**A stop returned on the public call's own terminal row is a no-op**, whatever
that row's verdict. The latch is read as it stood BEFORE that row's own event
fired, because the row's exit and its verdict were both decided when the
callback was shown it. So a converged terminal row still reports `kOptimal`
(converged beats stop) and a NON-converged cap terminal row still reports
`kMaxIter`. Only a CONTINUING dispatch's `kStop` moves a public verdict; that is
design §2.5's rule and not an omission.

Precedence at that exit is

```
converged  >  interrupted  >  probe-exhausted  >  max_iter
```

so a stop returned on a CONVERGED row reports `kOptimal` — **converged beats
stop** — and under `SqpOptions::budget_mode` an interrupt takes the ORDINARY
exit at the current point rather than the budget-best substitution, exactly as a
probe-budget stop does. `kNumericalError` (the non-finite-start exit) and the
QP-failure exit carry their own verdicts and are not demoted by a stop.

A stop returned INSIDE a restoration sub-solve reaches the parent two ways, and
both work: the sub-solve is handed a FORWARDER (which stamps `depth`, re-reads
the parent's clock, and latches on the parent), and, when that sub-solve exits
at a still-infeasible point, its own `kInterrupted` arrives at the parent as the
restoration phase's verdict. **The parent exits `kInterrupted` on EVERY
restoration-return arm**, at the point it holds once restoration has returned:
the resumed route goes round the loop and the next major's exit conjunction
reads the latch, and the exited routes read it at their own restoration-return
finish. What the sub-solve itself reached is a fact about the SUB-SOLVE and does
not become the caller's status.

**The certificate is that sub-solve's fact, and it is orthogonal to the
parent's status.** `SqpResult::infeasibility_certified` stays TRUE when the
restoration phase ran to its own `kOptimal` at an infeasible point, even if the
callback stopped on that phase's converged terminal row — a real proof is not
withheld. It stays FALSE when the sub-solve was itself stopped before it got
there, because a phase the caller interrupted has proved nothing about the
model.

**INTERIOR-POINT.** The stop is honoured where the event fired —
pre-factorization — so the solve returns the very point the callback was shown,
having spent no factorization and no KKT solve on the iteration it stopped. The
phase records `IpmStopReason::kInterrupted` (a new fifth value of that enum, and
a new `resolve_ipm_phase_status` case) and `resolve_ipm_phase_status` maps
`kMaxIter + kInterrupted -> kInterrupted`; a verdict the convergence check
already holds still wins, so converged beats stop here too. **A stop ends the
PHASE SEQUENCE**, not only the phase: later phases report `ran == false`.

`IpmStopReason` gains `kInterrupted = 4` and `to_string` gains `"interrupted"`.
A consumer switching over that enum without a `default` will need the new arm.

**`IpmOptions::return_best` is not honoured on an interrupted exit**: the solve
returns the point the callback was shown. Handing back a different iterate than
the one the caller stopped on would make the event a lie. The option's own doc
carries the same sentence.

**The interior-point engine's NESTED restoration sub-solver receives no
callback.** Under `RestorationModes::l1_nested` the feasibility subproblem is
solved by a distinct `InteriorPointSolver`, which carries no callback of its
own: its iterations fire nothing, and a `kStop` is honoured only once control is
back in the outer loop. The SQP engine forwards into its restoration sub-solve;
this one does not. Forwarding here is registered as a T8-close disposition item.

### 5. A throwing callback

An exception thrown from the callback propagates out of `solve()` on both
engines. The abandoned solve leaves **no end trace event and no ledger record**
— the begin/end pair and the record are deliberately non-RAII, and this is
stated rather than changed. The QP-engine records of the subproblems that solve
DID run **stay**: they record work that really happened. Both solvers remain
usable, and the next solve is bit-for-bit what a fresh one would produce, over
every deterministic result field. The two deductions on the interior-point side
are the PER-SOLVER quantities and not per-solve results:
`IpmResult::kkt_factor_counters` is a lifetime total (a reused solver carries the
abandoned solve's own factorizations, and must) and `kkt_analyses_this_call` is
0 on a solver that has already analysed. Timing is excluded on both engines, and
only timing.

### 6. What the event costs

Nothing at all when no callback is installed. On the SQP the snapshot the event
is built from — the point, the three price blocks in caller units and the four
declared diagnostics — is taken under `if (iteration_callback_)` at the one
statement that measures a row, and the driver REFUSES (by throwing
`std::logic_error`) to push a row whose snapshot is populated on a solve with no
callback, so "a solve without a callback copies nothing" is a checked fact. With
a callback installed it is one copy of `x`, `lambda_e`, `lambda_i` and `z` per
ROW (not per event: the watchdog re-takes them with the row it restores), plus
the four declared diagnostics computed once from the measurement already in
hand. The interior-point engine builds its own declared-space vectors per event,
from storage the iteration already wrote.

**Neither engine evaluates anything to build an event.** That is what makes the
interior corpus leg's second capture — every row run again with a counting
callback attached, every column identical — an identity proof rather than a
coincidence.

### 7. What did NOT change

No counter was added on either engine — the callback is not a counted quantity,
and the SQP counts its events in solve-scope state so that every W4 trace golden
stays byte-identical. `SqpCounters`, `SqpIterate`, `IpmResult`'s layout and the
trace schema are untouched. No new public header: `CallbackAction`,
`IterationEvent` and `IterationCallback` live in the already-installed
`drivers/solve_result.h`, so the install smoke and the export contract needed no
new TU.

---

## T8.7 — the ledger and the console

Two things arrive together because they are the same move: **instrumentation
that used to be private to a solver becomes something a caller can attach.** The
interior-point engine gains `attach_ledger`, exactly as the SQP driver has it;
and the console table both engines show (or, on the SQP, did not show) becomes a
`TraceSink` you can point anywhere, that the solver attaches for itself when
`common.print_level` says printing is on.

### 1. `attach_ledger` on the interior-point engine

```cpp
hven::solvers::Ledger ledger;
solver.attach_ledger(&ledger, "ipm");
solver.solve(model, x0);
solver.solve(model, x0);
for (const hven::solvers::IpmSolveRecord &r : ledger.ipm_records()) { ... }
std::string table = ledger.ipm_summary_table();
```

`IpmSolveRecord` carries `label`, `status`, `iterations`, `phases_run`,
`total_time`, `factorizations`, `analyses`, `soc_steps_taken`,
`watchdog_activations` and `wall_seconds`. One record per public `solve()` call
that RETURNS; a call that leaves by an exception writes nothing and consumes no
label number. `attach_ledger` resets the counter, so labels are `"ipm_0"`,
`"ipm_1"`, ….

Three differences from `SqpDriver::attach_ledger`, each deliberate:

* **No `"_qp"` forwarding.** The SQP driver forwards the same ledger to its
  internal `QpEngine` under a `<prefix>_qp` label; this engine owns no
  subordinate engine with a ledger of its own.
* **Nothing is re-forwarded on `set_options`.** The SQP rebuilds its QP engine
  there; this one rebuilds nothing.
* **The nested feasibility restoration writes no record.** It runs INSIDE the
  solve rather than as a separate solver, and its iterations are already inside
  `iterations` — the same shape as the SQP's restoration sub-driver, which
  receives `attach_trace` but no `attach_ledger`.

  Said exactly (fix1, since two review documents describe it the other way):
  **no interior-point restoration mode constructs a second solver.** `off`
  builds no restoration strategy at all; `proximal_switch` builds a
  `ProximalSwitchRestoration` and `l1_nested` a `NestedL1Restoration`, and both
  are STRATEGY objects driving an in-place phase on the outer barrier
  algorithm's own KKT system (`detail/globalization/l1_restoration.h`: "an
  in-place phase reusing the outer barrier algorithm's KKT system rather than a
  separate nested solver instance"). "Nested" in `l1_nested` names the nested
  PHASE. The SQP is the engine that really does construct a nested driver.

**`factorizations` is a PER-CALL delta**, not `kkt_factor_counters.factorize_count`.
That field is a snapshot of the LINEAR ENGINE's own counters, so on a reused
solver it would charge each record for every earlier call's work as well.
`analyses` is `IpmResult::kkt_analyses_this_call`, which is already per call —
and is honestly 0 on a second solve of the same program, where the analysis is
reused.

*Correction (fix1).* The delta is taken over
`InteriorPointSolver::lifetime_factorize_count()`, a new private accessor, and
**not** over `kkt_sol_.counters().factorize_count`. `SymmetricFactor::Counters` counts calls made
through one ENGINE INSTANCE and starts again at zero when the analysis is
replaced — which is what a solve of a DIFFERENT program on the same solver does
at its entry. Differencing that counter across such a call therefore produced a
NEGATIVE `factorizations`; it was −3 on the first fixture that tried it. The
solver now retires the outgoing engine's count into an accumulator of its own,
so its reading is monotone and the difference is this call's own work whether or
not the analysis moved. `factorizations` is never negative. `counters()` itself
is unchanged, and so is its per-instance contract — and nothing was added to
`KktFactorization`, which `IpqpEngine` embeds and whose layout the QP kernels'
byte-identity depends on.
`total_time` and `wall_seconds` are wall-clock and INFORMATIONAL, never asserted
(CLAUDE.md §7); `ipm_summary_table()` therefore has **no timing column**.

### 2. `ConsoleTraceSink` and `FanOutTraceSink`

New public header `hven/drivers/console_trace_sink.h` (a new installed TU;
the install smoke compiles it standalone).

```cpp
hven::solvers::ConsoleTraceSink console({/*wide=*/false, /*print_level=*/0}, stdout);
hven::solvers::FanOutTraceSink fan(&my_sink, &console);
```

`ConsoleTraceSink` renders BOTH engines' tables from the event stream: the
interior-point iteration table with `print_last_iterate`'s exact bytes, and the
SQP table with `format_iteration_table`'s. `FanOutTraceSink` forwards every
event to two sinks, first then second; either half may be null.

`ipm_residual_color(value, target, acceptable)` is declared beside them: the
five-band colouring that was `InteriorPointSolver::calculate_color`, now one
copy shared by the sink and the engine's remaining `print_exit_stats`.

### 3. Printing: what a caller sees change

**On the interior-point engine, nothing.** The table is byte-identical to the
one the solver used to print itself, at the same print levels, in the same
order. What moved is WHERE it is produced: the solver now attaches a
`ConsoleTraceSink` at solve entry when `common.print_level < 3`, and if you had
also attached a sink of your own, the two are FANNED OUT — your sink first, the
console second. **Attaching a console never displaces your sink**, and your
stream is byte-identical with printing on and off — with ONE line excepted, the
same exception its test applies (fix1): `ipm.solve.end` carries seven
WALL-CLOCK fields, which CLAUDE.md §7 makes informational and never asserted, so
two runs of one solve differ there whether or not a console is attached. Every
other line, the begin line and every `ipm.iter` row included, is compared byte
for byte, and the end line on its two deterministic fields.

**On the SQP driver, this is a GAIN.** Before T8.7 the driver printed nothing at
any level (`format_iteration_table` was called only by tests). It now prints the
same table at the interior-point engine's tiers. At `CommonOptions`' shipped
`print_level` of 3 — the SQP's effective default — **nothing is printed, exactly
as before.** Behaviour change (11) in design §2.7.

The tiers, per line kind, unchanged from the interior-point engine's own:

| level | what prints |
|---|---|
| `== 0` | iteration rows; the interior-point Problem Statistics block |
| `< 2` | headers, `Beginning:`/`Finished:`, the timing summary, the SQP table's head and its `Start Level` / `Scaling` trailer |
| `< 3` | warnings, exit verdicts, the SQP table's `Status` line |

`IpmOptions::wide_console` **stays an interior-point option**: the solver hands
it to its own console, and it also rides `ipm.solve.begin` so a foreign sink can
render the same layout. Nothing moves for a caller. The SQP renderings ignore
`wide`.

**Both engines' `attach_trace` now REFUSE a call made during a solve**, throwing
`std::logic_error` on `set_options`' rule and for its reason: the effective sink
is composed at solve entry and fixed for the solve, so a mid-solve change could
not take effect in it. Between solves it is unchanged, and attach order relative
to `set_options` is free.

*Correction (fix1).* `SqpDriver::attach_trace` refused from the start;
`InteriorPointSolver::attach_trace` did not, and that was a hole rather than a
difference. A call made from inside an iteration callback replaced the
interior-point composition for the rest of the solve — the console fell silent
mid-table — and a sink that detached itself from inside `on_ipm_iter` left the
restoration door's very next emit dereferencing a null. Both engines now refuse,
and every interior-point emit site reads the sink pointer once per emit.

**The restoration sub-driver is the one driver that never prints.** Every other
`SqpDriver` builds its console at the tiers `common.print_level` names —
including one constructed with restoration disabled, which is a different fact
(fix1: the console's condition used to be that other fact).

**`IpmResult::print_time` now brackets the EMIT** at the four row sites and the
restoration-exit marker, not a print — so a sink of your own is inside that
measurement too. It was informational before and it is informational now; no pin
reads it.

### 4. Trace schema: two moved lines, one new one

* `ipm.solve.begin` gains EIGHT keys: `acc_kkt_tol`, `acc_econ_tol`,
  `acc_icon_tol`, `acc_bar_tol` (the upper half of the row colouring's scale),
  `wide_console`, and `kkt_dim` / `kkt_nnz` / `internal_fixed_rows` (the Problem
  Statistics block's own inputs). `internal_fixed_rows` is NOT `vars_fixed`:
  that is the declared box's census, which equals the fixing-row count only
  under the MakeConstraint treatment.
* `sqp.solve.end` gains FIVE: `scaling_active`, `obj_scale`, `row_scale_min`,
  `row_scale_max`, `scaled_kkt_residual` — `SqpSolution::scaling`'s own values,
  which the console's `Scaling:` trailer had no other source for.
* **`ipm.restoration_exit_row` is NEW.** The restoration-locally-infeasible exit
  door marks the row it hands back, carrying `iter`, `phase`, `theta` and
  `threshold`. The ROW itself is not repeated — the adjacent `ipm.iter` line
  carries all of it — so this line says WHICH of the four NOTCONVERGED doors a
  reader is looking at, which the stream could not say before.
  `TraceSink::on_ipm_restoration_exit_row` is non-pure with an empty default, so
  no existing sink is touched by its arrival. `ConsoleTraceSink` renders nothing
  for it, on purpose: the row is already rendered from `ipm.iter`.

No `ipm.iter`, `ipm.solve.end`, `sqp.major` or `sqp.solve.begin` key moved; the
HS line-count goldens are unchanged.

### 5. Deleted, and what replaced it

| gone | replacement |
|---|---|
| `InteriorPointSolver::print_header()` (public, static) | `ConsoleTraceSink` writes the rule |
| `print_banner()`, `print_stats()`, `print_last_iterate()`, `print_timing_summary()` (private) | `ConsoleTraceSink` |
| `calculate_color()` (private, static) | `ipm_residual_color()` (free, public) |
| `print_settings()` (private) | **nothing** — declared, defined, and called by nobody |

`print_beginning()`, `print_finished()` and `print_exit_stats()` **survive** as
direct prints, with the KKT-analysis block and the nine warnings. Each reports
something the per-CALL `ipm.solve` pair cannot carry — a phase label, an
analysis, a phase's own verdict and times — and each is declared as **moved by
T8.7b**, which adds `on_ipm_phase_begin/end`, `on_ipm_kkt_analysis`,
`on_ipm_phase_exit` and `on_ipm_message`, serializes them, and deletes
`src/drivers/interior_point_solver_print.cpp`.

### 5b. Not a caller-visible change, but worth knowing (fix1)

* **`InteriorPointSolver::lifetime_factorize_count()`** — a new PRIVATE
  accessor, monotone across the engine replacement `set_qp_params()` performs.
  See §1's correction. `KktFactorization` is unchanged but for a comment.
* **`SqpDriver`'s two constructors and its destructor are defined out of line**,
  and `hven/drivers/sqp_driver.h` no longer includes
  `hven/drivers/console_trace_sink.h` — it forward-declares both sink types, as
  `hven/drivers/interior_point_solver.h` already did. A translation unit that
  used to reach `ConsoleTraceSink` or `fmt/color.h` THROUGH `sqp_driver.h` must
  include `hven/drivers/console_trace_sink.h` itself. Nothing else moved: the
  class, its members and its behaviour are unchanged.

### 6. A direct `set_*`/`clear_*` supersedes a pending deferral

The T8.6 rule — a set or clear made from INSIDE a callback is deferred to the
statement after it returns — is unchanged. What is fixed is the case where the
callback then THROWS: the deferral was never applied, and a caller who installed
a new callback directly used to have it silently replaced by the stale deferred
clear at the next solve entry. **A direct call now clears any pending deferral.**
Both directions hold, on both engines and on the KKT hook: with a direct call
after the throw the NEW callable fires; with no direct call the deferred clear
still applies.
