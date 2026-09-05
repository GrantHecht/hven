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
