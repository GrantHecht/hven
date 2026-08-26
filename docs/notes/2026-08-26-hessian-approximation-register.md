# Registered future feature — quasi-Newton Hessian approximation (first-order-only mode)

Date registered: 2026-08-26. Status: REGISTERED, not scheduled; no
milestone owns it yet. Owner intent on the record: **hven should beat
SNOPT on most if not all problems when only first derivatives are
available.** That target is the feature's acceptance frame, measured
per §7 discipline (counters asserted, wall informational) on the
AMPL-NLP benchmarking goal already registered.

## What the feature is

Both engines usable when the provider supplies only first-order
derivatives (gradients/Jacobians, no Lagrangian Hessian): hven builds
and maintains a Hessian approximation internally.

## Initial thinking (recorded, explicitly NOT locked in)

From the 2026-08-26 discussion:

- Plain dense BFGS is likely the wrong default for hven, for two
  reasons. (1) Its guaranteed positive-definiteness discards negative
  curvature that our inertia-controlled KKT machinery and
  trust-region/SOC/restoration globalization can actually exploit —
  SR1 pairs naturally with that stack. (2) Any *global dense* rank
  update densifies the Hessian, a non-starter at transcribed
  optimal-control scales.
- The candidate sketch: **partitioned (block-wise) quasi-Newton
  updates** (Griewank & Toint) riding the M4 model contract's
  partitioned pieces — per-piece SR1 on small dense blocks with a
  Powell-damped-BFGS fallback (SR1 skip rule / convex-model legs),
  assembled through the existing declaration machinery so declared
  KKT sparsity and layout/analysis reuse are untouched. Serves both
  engines (the IPM's inertia correction tolerates indefinite blocks
  the same way).
- Gauss-Newton for least-squares-shaped pieces composes with the
  partitioned scheme — piece type decides its update rule.
- Approximation state is per-piece dense blocks keyed by the
  declaration — the shape of a future warm-start extension (e.g.
  "hven.sqp.hessapprox.v1") carried across continuation / mesh
  refinement.

## First step when the work opens

A **thorough literature review before any design commitment** — the
sketch above is one candidate, not the decision. The review covers at
least: SR1 (trust-region pairing, skip/boundedness safeguards), damped
and limited-memory BFGS variants, partitioned/structured secant
updates and their convergence theory, Gauss-Newton and generalized
Gauss-Newton, and whatever the recent literature adds (including
structured updates in modern OCP solvers). Selection criteria:
sparsity preservation at scale, fit with inertia-controlled KKT
solves and the funnel/trust-region globalization, both-engines
applicability, warm-start currency fit, and expected performance on
the SNOPT-comparison problem set.

## Binding constraints on the work

- **SNOPT source firewall (absolute, CLAUDE.md §6)**: no SNOPT source,
  headers, or decompiled material, ever. Published algorithmic papers
  (e.g. the SNOPT paper itself) are legitimate references — the
  firewall is about source, not literature.
- **Math-only license discipline** for GPL/LGPL-tainted solvers'
  papers, per §6.
- Benchmark claims against SNOPT follow §7 (serialized wall-asserting
  runs; counters as the asserted currency).
