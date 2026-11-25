# Choosing a solver: IPM, SQP, or SQP with a quasi-Newton Hessian

hven has two engines behind one problem type and one result shape, so changing
engine is a one-line edit (`examples/01_hs071_both_engines.cpp`). This page says
which to try first and what the evidence for that is. It is configuration
guidance, not a theorem: the numbers below are measurements on named sets, and
the sets are small.

## 1. The short answer

| You have | Start with | Why |
| --- | --- | --- |
| An NLP with an exact Hessian (or you can supply one), no particular reason otherwise | `IpmSolver` with its defaults | It solved the most cells of the CUTEst training set in the M7 close measurement (17 of 23, against 16 for the exact-Hessian SQP) |
| An IPM run that ends `kStalled`, `kIterationLimit` or `kNumericalError` | `SqpSolver` on the same `Problem` | The two engines fail on different problems. The cells the IPM did not finish and the SQP did, and the reverse, are both non-empty |
| First derivatives only (no `eval_hessian`) | `SqpSolver` with `hessian_mode = SqpHessianMode::kLbfgsDamped` | It is the only engine that accepts `has_hessian() == false`; the IPM refuses such a problem by name |
| A good point already (a continuation step, a re-solve, a rough IPM answer) and you want a tight, active-set answer | `SqpSolver` started from a `WarmStart` | The SQP's active-set walk is the engine that benefits from a good guess about the active set; see section 4 |
| A SNOPT-like configuration | `docs/guides/snopt-like-configuration.md` | That guide names the settings |

Everything on this page is a starting point. If the first engine does not
finish, run the other: both read the same `Problem`, and the cost of trying is
one `solve` call.

## 2. What the measurements say

All figures are from the M7 close artifact (`docs/notes/2026-10-m7-close.md`
section 3), taken at the close gate's suite v1 leg, on the TRAIN halves of the
two sets. The holdout halves had not been run. One run per cell. The SNOPT and
IPOPT columns are the harness's own rows for those solvers, not figures quoted
from anywhere else.

| set | hven IPM | hven SQP (exact Hessian) | IPOPT | SNOPT |
| --- | --- | --- | --- | --- |
| Hock-Schittkowski TRAIN (22 problems) | 22 optimal | 22 optimal | 22 optimal | 22 optimal |
| CUTEst TRAIN (23 problems) | 17 optimal | 16 optimal | 15 optimal | 15 optimal |

What the CUTEst row does and does not show:

- The IPM's 17 is 14 `kOptimal` and 3 `kAcceptable`. If you count only
  `kOptimal`, it is 14.
- Its 6 unfinished cells were 4 iteration-budget exits and 2 wall-clock
  exits; the SQP's were 6 wall-clock exits and 1 error. A wall-clock exit is a
  property of the harness's per-cell ceiling (300 s), not of the problem alone.
- The two hven engines are complementary by cell. That is the reason the
  guidance is "try the other" rather than "pick the better": no rule that
  chooses between them from the problem's shape alone was found. The M7 close
  records that an adaptive between-solve switch was dropped from M7 for exactly
  that reason, and registers the cross-engine question for later.
- Five Hock-Schittkowski cells end as reference failures: they converge to an
  alternate local optimum and are counted optimal. A solver finding a different
  local solution is a property of non-convex problems, not of an engine.
- The IPM default (filter acceptance with the monitored barrier governor) was
  selected by a pre-registered rule over those training families. One cell,
  STEENBRD, is on the record as a caveat.

## 3. Quasi-Newton: when and how much to expect

`SqpOptions::hessian_mode = SqpHessianMode::kLbfgsDamped` makes the SQP build
its own damped limited-memory BFGS approximation of the Lagrangian Hessian from
gradient differences. The model's `eval_hessian` is never called, so the
problem may declare `has_hessian() == false` and no Hessian structure
(`examples/07_sqp_quasi_newton.cpp`, `examples/10_first_derivatives_only.cpp`).
The interior-point engine has no such mode.

Measured, on the same CUTEst training set:

- Exact Hessian: 16 of 23. Quasi-Newton, with only `hessian_mode` different: 8
  of 23.
- The union of the two is 17, so quasi-Newton converted a few cells the exact
  mode did not (the cells named in the M7 close are BIGBANK, NET3, STEENBRC and
  STEENBRG on the hven side). At an earlier retake it also lost cells the exact
  mode solved.

So: quasi-Newton is the answer to "I have no second derivatives", not a speed
or robustness upgrade over an exact Hessian. If you can supply `eval_hessian`,
supply it. If you cannot, expect roughly half the reach of the exact mode on
hard problems, and expect more iterations on the ones it does solve.

## 4. Warm starts and the two-stage route

Both engines return `result.warm_start` and accept one as the third argument of
`solve`. A warm start made by one engine is accepted by the other: the
interior-point answer carries the information the SQP needs to begin from the
right active set. Two things follow:

- **Continuation** (a parameter swept in small steps): keep one `Problem`, one
  solver, and pass the previous result's `warm_start`. `examples/05_warm_continuation.cpp`
  compares cold and warm sweeps on both engines and asserts the warm sweep costs
  no more iterations.
- **Rough then tight**: solve with the IPM at a loose tolerance, hand
  `result.warm_start` to the SQP at a tight one. `examples/06_ipm_to_sqp_polish.cpp`
  runs it three ways (staged, direct, cold) so you can compare. That example
  prints iteration counts; read them for your own problem, because the benefit
  depends on it.

The ceiling on how much of an offered warm start a solve trusts is
`common.start_level` (`kCold`, `kSeeded`, `kWarm`, `kHot`; default `kWarm`).

## 5. Reading a failure, and what to try next

| Status | Typical cause | Try |
| --- | --- | --- |
| `kIterationLimit` | The budget ran out; the iterate may still be improving | Raise `common.max_iterations`, or run the other engine |
| `kStalled` | No acceptable step could be found | The other engine; for the SQP, a different `qp_mode` is an advanced experiment, not a first move |
| `kInfeasible` | A locally infeasible stationary point of the violation | A different start point; a certified infeasible problem sets `result.infeasibility_certified` |
| `kNumericalError` | A non-finite evaluation the solve could not step around | Check the model's evaluations; an `EvaluationError` thrown at a trial point is a rejected trial, any other exception propagates |
| `kAcceptable` | The iterate met the looser acceptable tolerances | Read `result.convergence`; decide whether that is enough for the application |
| `kWorkLimit`, `kTimeLimit` | You set `common.max_factorizations` or `common.time_limit_seconds` | Those are your own ceilings; the time limit is wall-clock and not reproducible |
| `kInterrupted` | A callback or a stop token ended the solve | `result.stop_reason` says which; the result is the iterate at that point |

The status vocabulary, the stop-reason vocabulary and what each engine can
report are in `include/hven/core/solver_status.h`; the trace's exit line prints
the same facts (`docs/getting-started.md`, section 5).

## 6. What this page does not claim

- No claim about problems outside those two sets. The training sets are a few
  dozen problems each.
- No cost comparison: wall-clock is informational in this project, and the
  counts above are solved counts, not times.
- No claim that the order IPM-first is right for your problem. It is the order
  that solved the most training cells.
- Labels such as `default` and `sqp-qn` are names of configurations in the
  benchmark harness; in the library they are `SqpOptions` with and without
  `hessian_mode = kLbfgsDamped`, nothing else.
