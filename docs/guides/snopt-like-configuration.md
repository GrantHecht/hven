# Configuring the SQP engine to behave like SNOPT (first derivatives only)

Audience: a coding agent writing a SNOPT-compatible shim over `hven::SqpSolver`
that has first derivatives only. Every hven name below exists at the commit this
guide was written against; header paths are given so each can be checked.

Status warning: hven's API is not locked until the M8 close. Option fields may
move between headers (an options-registry change is planned), and the
`advanced` tier may change in any milestone. Pin the hven commit and re-read
`include/hven/sqp/sqp_options.h` when bumping it.

SNOPT facts here come from SNOPT's public documentation (user guide parameter
descriptions) as remembered by the author. The author did not re-fetch the
public pages while writing this, so every SNOPT default and meaning below is
flagged "verify against the SNOPT user guide" where it matters. No SNOPT source
was consulted.

## 1. Recommended configuration

| Concern | hven setting | Header |
| --- | --- | --- |
| Engine | `SqpSolver` (`SqpOptions`) | `hven/sqp/sqp_solver.h`, `hven/sqp/sqp_options.h` |
| First derivatives only | `hessian_mode = SqpHessianMode::kLbfgsDamped`; the Nlp returns `has_hessian() == false` and declares no Hessian structure | `sqp_options.h`, `hven/model/nlp.h` |
| Memory | `lbfgs_memory` (default 10); `max_lbfgs_full_memory_dimension` (default 400): at or below that many quasi-Newton variables a dense full-memory damped BFGS replaces the window | `sqp_options.h` |
| Curvature on nonlinear variables only | `advanced.lbfgs_nonlinear_only = true` (needs a declared Hessian pattern to know which variables are nonlinear; with none it falls back to all variables) | `sqp_options.h` |
| QP engine | `qp_mode = QpMode::kWalk` (the default) | `sqp_options.h` |
| Globalization | funnel + trust region, built in; `make_strategy` replaces it (internal interface) | `sqp_options.h` |
| Infeasibility | restoration phase plus an elastic ladder; `max_restoration_entries`, `advanced.restoration_proximity` | `sqp_options.h` |
| Scaling | `column_scaling` (default `kAuto`), `row_scaling` (default false), `column_scaling_factor_max`, `caller_step_limit` | `sqp_options.h` |
| Infinite bound | `common.infinite_bound` (default 1e20) | `hven/solver/common_options.h` |
| Derivative checking | none (see section 4) | |

Why `kWalk`. The walk (`walk_engine.h`) is a primal active-set method: one
working-set change per minor iteration, warm-started from the previous
subproblem's working set. SNOPT's QP solver (SQOPT) is also an active-set
method that changes the working set one constraint at a time, so minor-iteration
counts, degeneracy behaviour and warm-start behaviour are the closest analogue.
`kSsn` (semismooth Newton) and `kIpqp` (interior point) change many constraints
per step and are not SNOPT-like.

Why damped L-BFGS. `kLbfgsDamped` builds a positive-definite damped compact
L-BFGS approximation of the Lagrangian Hessian and never calls `eval_hessian`
(`SqpCounters::eval_hess_calls == 0`). This is the analogue of SNOPT's limited
memory quasi-Newton Hessian (the "Hessian updates" parameter). It is
positive definite by construction, so the subproblems are convex QPs. Note that
`validate()` refuses `kLbfgsDamped` together with `advanced.restoration_proximity > 0`.

Globalization, and how it differs. hven's SQP uses a trust region (an
l-infinity radius, `trust_region_initial/max/min`) with a funnel acceptance test
on a feasibility measure h and the objective f, with second-order-correction
rescue (`advanced.second_order_correction`, default true). It is a
filter-like, two-measure method: constraint violation and objective are tracked
separately. SNOPT instead minimizes a single augmented-Lagrangian merit
function in a line search along the QP direction, with the multiplier and slack
estimates as line-search variables. Consequences: iterate trajectories,
iteration counts and "step limit" behaviour will not match SNOPT's, and there is
no merit-function value to report. Do not try to emulate one.

Infeasibility handling. SNOPT enters "elastic mode" when the QP is infeasible
or multipliers blow up: constraints are relaxed with penalised elastic
variables (penalty = Elastic weight). hven has two related mechanisms: (a) an
elastic ladder inside the QP tier (rho escalation over relaxed subproblems,
`advanced.elastic_rung_engine`, `elastic_ladder_early_exit`), and (b) a
feasibility restoration phase (a nested solve of an infeasibility measure,
bounded by `advanced.max_restoration_entries`, default 10) which, when it
ends at a stationary point of the violation that is still infeasible, returns
`SolveStatus::kInfeasible` with `SolveResult::infeasibility_certified == true`.
Neither takes an elastic weight option. Both are automatic; there is no
switch to force "elastic mode from the start".

Scaling. `column_scaling = kAuto` (default) scales variables (and engages row
and objective scaling for that solve) only when the problem is badly scaled
(column-factor spread above `column_scaling_threshold`). `row_scaling = true`
forces objective and row scaling from start-point derivatives
(`scaling_max_gradient`, default 100; `scaling_factor_max`, default 1e12).
Closest to SNOPT's default "Scale option" is leaving `kAuto` alone. `kAlways` is
NOT a stand-in for SNOPT's "Scale option 2": SNOPT scales from the caller's own
`x` scales and the constraint magnitudes, hven's `kAlways` rescales every variable by
a power-of-two factor equilibrating the start-point Jacobian, and a variable the
Jacobian barely touches (a weak sensitivity) is amplified hardest, which in turn inflates
the trust region in caller units for that variable (|dx_j| <= d_j * radius). A shim that maps
"Scale option 2" to `kAlways` can move a weak variable (an angle, say) by hundreds of
degrees in one step. Recommended instead: give the model physical caller scales (a variable
that is O(1) at the solution in its own units), leave `column_scaling` at `kAuto` or set
`kOff`, and bound steps in caller units if wanted (`caller_step_limit`). If `kAlways` is used,
`column_scaling_factor_max` (default 1024) bounds the amplification of any one column (it does not
touch the row and objective factors, which `scaling_factor_max` still clamps, and `kAuto` still
clamps its column factors with `scaling_factor_max`).

Caller-unit step limit. `caller_step_limit = r` (default 0, off) narrows every QP's variable box
to `|dx_j| <= r * (1 + |x_j|)` in the caller's units, whatever the scaling and QP mode, including
the restoration phase's steps in the original variables (its elastic slacks are not limited). A
variable stopped by the limit is reported free (zero bound multiplier) and the step counts as
trust-region-limited. The trust region stays in the engine's units. These options are SQP-only:
the interior-point solver has no trust region, column layer or walk QP.

Console and trace units. With column scaling firing, the console `step` and `radius` columns
(and `sqp.major`'s `step` and `trust_radius`) are in the engine's SCALED units; the returned
`x` and the iteration events' `x` are in caller units. `SqpResult::scaling` reports whether the
columns were scaled and the factors' range.

Elastic-ladder diagnostics. When the elastic ladder is exhausted the console line and the
`message`/`elastic_exhausted` record state how it was entered (verified infeasible in the trust
region, working-set numerical termination, refused certificate, IPQP fallback), the main QP's
status, the final rung's status and penalty, and `closed`/`reduced`/`promises_f`/`slack_l1`; the
`sqp.major` row keeps the MAIN QP's `qp_status` and carries the ladder in its `ladder` block
(`docs/trace-schema-v1.md`).

Failed-QP dump. `advanced.qp_dump_dir` plus `advanced.qp_dump_policy` (`QpDumpPolicy::kOff`
default, `kFailed`, `kAll`) write each qualifying walk solve (the major's QP, SOC, cap retry, the
certified fallback's walks, each ladder rung, restoration's included) as a new "D4QP 1" text file,
`qp_p<pid>_s<solver>_<seq>_d<depth>_m<major>_<site>.txt`, never overwriting an existing one. The
format and the names are in `docs/qp-dump-format.md`.

### Complete snippet

Based on `examples/10_first_derivatives_only.cpp` (the same configuration on HS071),
`examples/07_sqp_quasi_newton.cpp` and `examples/09_callbacks_and_stop.cpp` (the callback and the
cooperative stop). The model is a toy; the shim would put its
own `Nlp` subclass in its place.

```cpp
#include <cstdio>
#include <memory>

#include <hven/hven.h>

namespace {

using namespace hven;

// min (x0-1)^2 + (x1-2)^2   s.t.  x0^2 + x1^2 <= 2,  -5 <= x <= 5.  First derivatives only.
struct Toy : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 1, .jacobian_nonzeros = 2, .hessian_nonzeros = 0};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-5.0);
        xu.setConstant(5.0);
        gl << -kInfinity; // a SNOPT "infinite" bound maps to common.infinite_bound
        gu << 2.0;
    }
    double eval_f(ConstVecRef x) const override {
        return (x[0] - 1) * (x[0] - 1) + (x[1] - 2) * (x[1] - 2);
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << 2 * (x[0] - 1), 2 * (x[1] - 2);
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x.squaredNorm(); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v = 2.0 * x; }
    bool has_hessian() const override { return false; }
};

} // namespace

int main() {
    SqpOptions o;

    // Engine behaviour: limited-memory quasi-Newton, active-set QP.
    o.hessian_mode = SqpHessianMode::kLbfgsDamped;
    o.lbfgs_memory = 10;                      // "Hessian updates" analogue
    o.advanced.lbfgs_nonlinear_only = true;   // SNOPT-style: curvature on nonlinear variables
    o.qp_mode = QpMode::kWalk;                // primal active set (the default)

    // Tolerances and limits.
    o.common.optimality_tol = 1e-6;           // "Major optimality tolerance" (definition differs)
    o.common.feasibility_tol = 1e-6;          // "Major feasibility tolerance" (definition differs)
    o.common.max_iterations = 1000;           // "Major iterations limit" (counts subproblems)
    o.qp.max_iterations = 500;                // per-QP minor cap ("Minor iterations limit" analogue)
    o.common.time_limit_seconds = 600.0;      // "Time limit"; wall clock, not reproducible
    o.common.infinite_bound = 1e20;           // "Infinite bound"

    // Scaling: the default is kAuto; this line only makes the choice explicit.
    o.column_scaling = SqpColumnScaling::kAuto;

    // Output: silent by default; kIterations prints the iteration table to stdout.
    o.common.verbosity = Verbosity::kSilent;

    SqpSolver solver(o); // validates; throws std::invalid_argument naming the bad field

    // Optional: per-iteration callback (early stop, live x / multipliers).
    solver.set_iteration_callback(
        [](const IterationEvent &e) {
            (void)e;
            return CallbackAction::kContinue; // kStop / kStopReturnBest end the solve (kInterrupted)
        },
        IterationPayload::kVectors | IterationPayload::kDiagnostics);

    Problem problem(std::make_shared<Toy>());
    Vec x0(2);
    x0 << 0.0, 0.0;
    SqpResult r = solver.solve(problem, x0);

    std::printf("status=%s iterations=%lld minors=%lld f=%.8g fevals=%lld gevals=%lld\n",
                to_string(r.status), static_cast<long long>(r.iterations),
                static_cast<long long>(r.counters.qp_minor_iters), r.f,
                static_cast<long long>(r.work.objective_evals),
                static_cast<long long>(r.work.constraint_evals));
    return r.status == SolveStatus::kOptimal ? 0 : 1;
}
```

## 2. SNOPT parameter to hven option

SNOPT names are from the public user guide's option descriptions; the defaults
in parentheses are from memory and should be verified.

| SNOPT parameter | hven option | Notes |
| --- | --- | --- |
| Major iterations limit (1000) | `common.max_iterations` (default 100) | hven counts subproblems solved, restoration majors included, not accepted steps. `SolveBudget::max_iterations` can only tighten it. |
| Minor iterations limit (500) | `qp.max_iterations` (`std::optional`, per QP solve; empty derives a cap from the subproblem size) | SNOPT's limit is per QP; this one is too. `SolveBudget::max_minor_iterations` is a different thing: a total probe budget across the solve (status `kWorkLimit`). |
| Iterations limit (total major+minor) | `SolveBudget::max_minor_iterations` is the nearest, for minors only; `common.max_factorizations` caps total work | No single cap on majors plus minors. The shim can enforce it in an iteration callback by summing counters. Approximate. |
| Major optimality tolerance (1e-6) | `common.optimality_tol` (1e-6) | Not the same measure; see section 4. |
| Major feasibility tolerance (1e-6) | `common.feasibility_tol` (1e-6) | Also the bound-activity tolerance of the stationarity measure. Not the same definition. |
| Minor feasibility tolerance (1e-6) | `qp.feasibility_tol` (QP engine default 1e-9) | Plausible analogue; confirm it is not overridden per solve before relying on it. |
| Minor optimality tolerance | `qp.optimality_tol` (1e-9) | As above. |
| Hessian updates (limited memory, 10) | `lbfgs_memory` | Limited-memory only; no equivalent of a "full memory" Hessian that is user-sized, except `max_lbfgs_full_memory_dimension` makes a dense full BFGS automatic at or below that dimension. |
| Hessian full memory / limited memory | `hessian_mode = kLbfgsDamped` plus `max_lbfgs_full_memory_dimension` (0 forces the window) | Approximate. |
| Infinite bound (1e20) | `common.infinite_bound` (1e20) | Same convention, applied to variable and row bounds, lower and upper. |
| Scale option | `column_scaling`, `row_scaling`, `scaling_max_gradient`, `scaling_factor_max`, `column_scaling_factor_max` | Not one-to-one; hven scaling is derivative-based from the start point, and `kAlways` is not "Scale option 2" (see Scaling above). Prefer physical caller scales with `kAuto` or `kOff`. |
| Verify level | no equivalent | hven has no derivative checker. The shim must check derivatives itself if it wants Verify behaviour. |
| Elastic weight | no equivalent | Restoration and the elastic ladder are automatic; rho escalation is internal. |
| Major step limit (2.0) | `trust_region_initial` / `trust_region_max` | Different mechanism: an l-infinity trust-region radius on the step, not a bound on the line-search step relative to the QP direction. Approximate at best. |
| Function precision | no equivalent | hven has no noise-aware tolerance. Tolerances are used as given. |
| Derivative option (0/1) | no equivalent needed | First derivatives only is selected by `hessian_mode = kLbfgsDamped` (Hessian), and all gradient and Jacobian entries must be supplied analytically: hven has no finite-difference mode. The shim must supply any differencing itself. |
| Time limit | `common.time_limit_seconds` | Wall clock from solve entry, read at iteration boundaries; status `kTimeLimit`. |
| Print file / Summary file | `common.verbosity` (stdout) and the trace sinks (`hven/trace/*.h`: `console_trace_sink.h`, `json_lines_trace_sink.h`) | Console output goes to stdout; the shim must redirect or format into its own print/summary files from `SqpResult::history` or a trace sink. hven's layout is not SNOPT's. |

## 3. Result mapping

`SolveResult::status` is a `SolveStatus` (`hven/core/solver_status.h`);
`SolveResult::stop_reason` (`StopReason`) gives the cause. The mapping to
SNOPT's INFO families, approximate throughout; take the numeric codes from the
SNOPT user guide:

| hven `SolveStatus` | SNOPT INFO family |
| --- | --- |
| `kOptimal` | finished successfully (optimality conditions satisfied) |
| `kAcceptable` | not reported by the SQP engine |
| `kInfeasible`, `infeasibility_certified == true` | "problem appears to be infeasible" (nonlinear infeasibilities minimized) |
| `kInfeasible`, not certified (`kSubproblemFailure`, `kInconsistentEqualities`) | infeasible family, weaker: no claim about the model. `kInconsistentEqualities` is nearest to the infeasible linear/nonlinear constraints codes. |
| `kIterationLimit` | iteration limit reached (major) |
| `kWorkLimit` | iteration/work limit family (minor-iteration budget or factorization cap) |
| `kTimeLimit` | no standard SNOPT INFO; the shim must choose (a resource-limit code, or its own) |
| `kNumericalError` | numerical difficulties; also covers unserviceable restoration and non-finite evaluation (`StopReason::kNonFiniteStart`, `kNonFiniteIterate`, `kRestorationRefused`, ...). SNOPT's "function evaluation error" family is the right target for the non-finite stops. |
| `kInterrupted` | user requested stop (callback `kStop`, `SolveBudget::stop`, or an escaped exception: `StopReason::kCallbackStop`, `kStopRequested`, `kException`) |
| unbounded | The SQP engine has no unbounded status. An unbounded problem presents as `kNumericalError` or an iteration limit. Do not report SNOPT's unbounded code from hven's result without extra evidence. |
| `kStalled`, `kDiverging` | not reported by the SQP engine (interior point only) |

Counts for the shim's outputs (`hven/solver/solve_result.h`,
`hven/core/solver_counters.h`):

- Major iterations: `SolveResult::iterations` (steps taken at top level, nested
  restoration majors not added) or `SqpResult::counters.major_iters` (subproblems
  solved, restoration folded in). `steps_accepted` and `rejected_steps` split
  them; `restoration_iters` counts restoration majors.
- Minor iterations: `SqpResult::counters.qp_minor_iters`.
- Evaluations: `SolveResult::work` (`WorkCounters`: `objective_evals`,
  `gradient_evals`, `constraint_evals`, `jacobian_evals`, `hessian_evals`,
  `factorizations`), counted at the user-callback boundary per solve. Under
  `kLbfgsDamped`, `hessian_evals` should read 0.
- Final residuals: `SqpResult::convergence` (`stationarity`, `feasibility`,
  `complementarity`, `kkt_residual`) and the same three on `SolveResult`.
- Wall time: `SolveResult::wall_seconds`. Text: `SolveResult::message`
  (informational, never to be parsed).

Multiplier signs. hven's convention is Ipopt's: for an `Nlp` the row multipliers
`SolveResult::lambda` satisfy L = f + lambda^T g - z^T x, where g is the row
function. Bound multipliers `z = z_lower - z_upper`: an entry is >= 0 at an
active lower bound and <= 0 at an active upper bound. Rows are the Nlp's rows
(declared bounds `gl <= g(x) <= gu`), one lambda per row, no slack or objective
row. SNOPT's sign convention for its multiplier output (Lamda / pi, and the
reduced-cost reading of the x entries) must be read from the SNOPT user guide
and verified numerically on a small problem with an active upper and an active
lower bound before the shim negates anything. Do not assume the two agree.

## 4. Differences the shim must not paper over

- **Tolerances are not equivalent in definition.** hven's `optimality_tol` is
  read against the engine's stationarity measure and NLP complementarity;
  `feasibility_tol` is an inf-norm of residuals. SNOPT scales its
  optimality tolerance by multiplier magnitude (a relative, scaled condition on
  the reduced gradient) and its feasibility test is relative to the size of the
  iterate; verify against the user guide. A solve SNOPT calls optimal may be
  declared optimal by hven at a different point and vice versa. Report hven's
  residuals honestly.
- **Scaled versus caller units.** With scaling engaged (`column_scaling`
  `kAuto` firing, or `row_scaling`), hven gates convergence on scaled
  residuals but reports `x`, `f`, multipliers and the four terminal residuals
  in caller units. A `kOptimal` result may therefore show a caller-scale
  `kkt_residual` above `optimality_tol` (`SqpResult::scaling` carries both).
  Column-scaled solves also require a caller-unit feasibility check and a coarse
  caller-unit stationarity bound (`SqpCounters::scaled_convergence_refusals`).
- **Limits count differently.** `common.max_iterations` counts subproblems
  including restoration ones; SNOPT's major iteration count is not the same
  quantity. The minor cap is per QP in both, but hven's walk and SNOPT's QP will
  not take the same number.
- **Different globalization and failure paths.** No augmented-Lagrangian merit,
  no line-search step-length reporting, no SNOPT "elastic mode" log, no
  warm-start-from-basis (hven's warm starts are its own `SqpWarmStart` /
  `WarmStart` objects, not SNOPT's Hot/Warm start tokens). The solver may exit in
  restoration where SNOPT would continue in elastic mode.
- **No derivative check, no finite differences.** hven never differences user
  functions. Every gradient entry and every declared Jacobian nonzero must be
  supplied. SNOPT's Verify level has no analogue.
- **Objective-row-in-F convention.** hven has no such convention: the objective
  is `eval_f` / `eval_grad_f` and constraints are separate rows `g` with bounds
  `gl`/`gu` (`hven/model/nlp.h`). A shim over a SNOPT-style single `F` vector
  (objective row index, linear and nonlinear parts, constant offsets) must split
  `F` itself and translate the Jacobian triplets (`jacobian_structure`,
  `eval_jacobian`) accordingly. Linear rows are not distinguished from nonlinear
  ones; hven treats all rows as general nonlinear.
- **Callbacks and early stop.** `SqpSolver::set_iteration_callback(cb, payload,
  scope)` fires once per history row with an `IterationEvent` (iteration, f, x,
  lambda, z, stationarity, feasibility, complementarity, step norm, elapsed
  seconds) under the payload mask given. Returning `CallbackAction::kStop` ends
  the solve with `kInterrupted` and `StopReason::kCallbackStop` at the current
  point; `kStopReturnBest` returns `best_iterate` instead. A cooperative stop
  from another thread is `SolveBudget::stop` (`hven/core/stop.h`). A stop on an
  already converged row still reports `kOptimal`. Exceptions thrown by the
  callback propagate out of `solve()`. A model's `eval_*` reports a failed
  evaluation by throwing `EvaluationError` or returning a non-finite value; the
  engine tries to recover by shrinking the step, which is SNOPT's evaluation
  error return analogue, not identical to it.
- **API not locked until the M8 close.** Options may move headers in the
  options-registry work; names are expected to remain but check at each pin
  bump. The `advanced` tier carries no stability promise at all. Build hven on
  Linux with MKL; the shim should treat an `std::invalid_argument` from
  `SqpSolver(o)` or `solve()` as a programming/boundary error and report it as
  an invalid-input INFO.
