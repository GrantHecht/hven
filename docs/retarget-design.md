# Interior-point linear-solver retarget design

> **SURFACE VERDICT (adjudicated):** the current
> `hven::linear::SymmetricFactor::Options` surface does not reproduce every
> *act* in psiopt's proven configuration. Three gaps were found; each now
> carries a ruled disposition, so the retarget is mechanical:
>
> 1. psiopt writes `iparm[10] = 0`; `Options::matrix_scaling = false` means
>    don't write `iparm[10]`.
> 2. psiopt's default CNR-off path writes `iparm[33] = 0` through
>    `set_params()`; `Options::cnr_threads = 0` means don't write `iparm[33]`.
> 3. on Accelerate, psiopt actively applies `qp_threads_`; hven stores
>    `Options::num_threads` but intentionally applies no Accelerate thread
>    control.
>
> The first two have the same *effect* as psiopt on the audited MKL, but they
> are different acts and are exposed to backend-default drift. The
> consumed-surface audit explicitly distinguishes those two ideas for
> `iparm[20]` ([`docs/consumed-surface-audit.md:177-187`](consumed-surface-audit.md));
> the same standard applies here. None of these differences is licensed as a
> golden-rig numerical delta.
>
> **Disposition, gaps 1-2 — effect-parity by written decision, guarded by a
> default-drift canary.** The surface does NOT gain write-the-default
> semantics: an active write at an option's default was already rejected for
> the flop-report entry (a cancel-write contradicts don't-write-by-default),
> and the same reasoning governs here — an engine's habit of writing a
> backend's own default is not a semantic the surface adopts. The exposure
> that remains is a future MKL default move for `iparm[10]` or `iparm[33]`,
> and that is covered by a canary test asserting `pardisoinit` leaves both
> entries at the values this decision assumes (0 and 0) on the linked MKL.
> If the canary fires, this decision is re-opened with evidence in hand —
> the drift becomes a failing test, never a silent bit-identity break.
>
> **Disposition, gap 3 — resolved by ownership routing.** The engine retains
> its version-split Accelerate thread helper (which migrates with the engine
> sources); `Options::num_threads` on Accelerate remains store-only per the
> frozen threading contract. This note's call-site inventory and hazards
> section already implement that routing.

The audit's raw write table independently records both disputed acts:
`iparm_[10] = scaling_` and `iparm_[33] = threads_`
(`docs/audit/static-scan-2026-08-09.csv:16,32`). The audit's summarized
user-setting table calls the default CNR path "unwritten" because the engine
only conditionally changes `threads_`; the backend call still writes the
member's initialized zero. Exact-act parity follows the write table and the
executed `set_params()` body, not that shorthand.

This note maps the shipped, proven psiopt configuration onto the
`hven::linear` surface and lists the source edits needed to replace the two
backend-specific Eigen-derived interfaces. The authoritative consumption
inventory is [`docs/consumed-surface-audit.md`](consumed-surface-audit.md).
Names and enum values below are copied from
[`include/hven/linear/symmetric_factor.h`](../include/hven/linear/symmetric_factor.h),
especially `Options` at lines 243-545 and the evidence types at lines 89-202.

## Configuration mapping

The values below describe psiopt's shipped configuration. `qp_threads_` is
runtime-dependent: both constructors replace its compile-time value 8 with
`min(8, get_core_count())` (`psiopt/src/psiopt.cpp:570-576`; the standalone
definition is `psiopt/CMakeLists.txt:171-174`). All other values shown are the
shipped settings from `psiopt/include/tycho/detail/solvers/psiopt.h:397-417`.

| `hven::linear::SymmetricFactor::Options` member | MKL value | Accelerate value | Grounding and exact effect |
| --- | --- | --- | --- |
| `kind` | `FactorKind::kLDLT` | `FactorKind::kLDLT` | The old members are `Eigen::PardisoLDLT<..., Eigen::Upper>` and `Eigen::AccelerateLDLTTPP<..., Eigen::Upper>` (`psiopt/include/tycho/detail/solvers/psiopt.h:1118-1123`). `FactorKind`'s exact enumerators are at `symmetric_factor.h:78-87`. |
| `num_threads` | `settings_.qp_threads_` | `settings_.qp_threads_`, but see the blocking caveat | The audit records seven `mkl_set_num_threads_local` sites and the two Accelerate mechanisms (`consumed-surface-audit.md:108-110,287-306`). psiopt applies the value in `set_qp_params()` (`psiopt.cpp:111`) and again at solve entry (`psiopt.cpp:3307-3313`). MKL hven applies a positive value at backend-call scope. Accelerate hven stores the value but applies nothing (`symmetric_factor.h:247-253`), so the legacy version-split action must remain external. |
| `pivot_perturb_exp` | `8` | `8` | psiopt writes `qp_pivot_perturb_` to `iparm[9]` (`pardiso_interface.h:625-628`); the audit records default 8 (`consumed-surface-audit.md:87-90`). The hven member is exactly `pivot_perturb_exp` (`symmetric_factor.h:255-258`). On Accelerate, the explicit zero-tolerance override below prevents the exponent-derived default from changing psiopt's threshold. |
| `max_refinement_iters` | `0` | `0` | psiopt writes `qp_ref_steps_` to `iparm[7]` (`pardiso_interface.h:625`) and its shipped value is 0 (`consumed-surface-audit.md:87-90`). Accelerate separately disables refinement when the value is zero (`psiopt.cpp:112-113`). The hven member and the partial-solve exception are at `symmetric_factor.h:260-262`. |
| `ordering` | `Options::Ordering::kNestedDissection` | `Options::Ordering::kNestedDissection` | psiopt's `METIS` value is 2 and is written to `iparm[1]` (`psiopt.cpp:120-125`; `pardiso_interface.h:618-620`); the audit records the default as METIS (`consumed-surface-audit.md:85-90`). The Accelerate branch explicitly calls `set_order(SparseOrderMetis)` (`psiopt.cpp:94-97`), also confirmed by the audit (`consumed-surface-audit.md:315-321`). The exact hven enum maps `kNestedDissection` to MKL 2 and Accelerate `SparseOrderMetis` (`symmetric_factor.h:264-300`). Do not use `kBackendDefault`. |
| `weighted_matching` | `true` | `false` | psiopt writes `iparm[12] = qp_matching_`, shipped as 1 (`pardiso_interface.h:630-632`; `consumed-surface-audit.md:87-90`). `true` is the only hven value that explicitly writes 1 (`symmetric_factor.h:302-312`). Accelerate has no matching control, and psiopt's Accelerate branch makes no matching call; `true` would throw, so its value there is `false`. |
| `matrix_scaling` | `false` **(effect only; act not expressible)** | `false` | psiopt writes `iparm[10] = qp_scaling_`, shipped as 0 (`pardiso_interface.h:628`; `consumed-surface-audit.md:92-93,162-170`). The exact hven contract says `false` leaves `iparm[10]` untouched and only `true` writes 1 (`symmetric_factor.h:314-339`). Thus no current Options value pins scaling off as psiopt does. Accelerate must use `false`; its inf-norm scaling is a different, unconditional backend mechanism. |
| `pivot_strategy` | `Options::PivotStrategy::kTwoByTwo` | `Options::PivotStrategy::kBackendDefault` | psiopt writes `iparm[20] = 1` (`psiopt.cpp:129`; `pardiso_interface.h:641`), recorded as the shipped `TwoByTwo` value by the audit (`consumed-surface-audit.md:93,177-198`). The exact hven enum is `kBackendDefault`, `kOneByOne`, `kTwoByTwo`, `kOneByOneNoAutoRefine`, `kTwoByTwoNoAutoRefine` (`symmetric_factor.h:341-382`); `kTwoByTwo` explicitly writes 1. Accelerate exposes no selector, so only `kBackendDefault` is legal. |
| `factorization_algorithm` | `Options::FactorizationAlgorithm::kClassic` | `Options::FactorizationAlgorithm::kBackendDefault` | psiopt writes `iparm[23] = 0` (`pardiso_interface.h:642`), and the audit names `Classic` as the shipped value (`consumed-surface-audit.md:94,202-220`). `kClassic` explicitly writes 0; `kBackendDefault` does not (`symmetric_factor.h:384-408`). Accelerate has no corresponding control. |
| `solve_parallelism` | `Options::SolveParallelism::kAdaptivePartitioning` | `Options::SolveParallelism::kBackendDefault` | psiopt writes `iparm[24] = 0` (`pardiso_interface.h:643`), recorded by the audit at `consumed-surface-audit.md:95,202-220`. The exact hven enum is `kBackendDefault`, `kAdaptivePartitioning`, `kSequential`, `kMatrixPartitionParallel`; `kAdaptivePartitioning` explicitly writes 0 (`symmetric_factor.h:410-458`). Accelerate has no corresponding control. |
| `cnr_threads` | `0` **(effect only; act not expressible)** | `0` | CNR is off by default (`consumed-surface-audit.md:96,227-247`). The engine assigns `kkt_sol_.threads_` only when `cnr_mode_` is true (`psiopt.cpp:137-138`), but the old member starts at 0 and `set_params()` unconditionally executes `iparm_[33] = threads_` (`pardiso_interface.h:616-617,647`). The exact hven contract says 0 leaves the entry untouched and a positive value writes it (`symmetric_factor.h:460-492`). Accelerate has no CNR concept and requires 0. |
| `collect_factor_mflops` | `true` | `false` | psiopt explicitly writes `iparm[18] = -1` and then reads it after every numeric factorization (`pardiso_interface.h:254-255,277-278,338-339,356-357,637-638`; audit `consumed-surface-audit.md:249-285`). `true` makes hven write `-1` and populate `FactorEvidence::factor_mflops` (`symmetric_factor.h:494-531`). Accelerate exposes no cost estimate and rejects `true`; its legacy `flops_` is hardcoded to 0 (`accelerate_interface.h:466-479`). |
| `accelerate_zero_tolerance` | `std::nullopt` | `1e-4 * std::numeric_limits<double>::epsilon()` | psiopt calls `set_zero_tolerance(settings_.accel_zero_tolerance_)` (`psiopt.cpp:115`), whose shipped value is the expression shown (`psiopt.h:415-417`). The audit identifies this as the previously uncovered Accelerate write (`consumed-surface-audit.md:333-348`). The exact hven member is `std::optional<double> accelerate_zero_tolerance` (`symmetric_factor.h:533-544`). It is Accelerate-only and must remain absent on MKL. |

The Accelerate pivot tolerance needs no Options assignment for the proven
configuration: psiopt writes 0.01 (`psiopt.cpp:114`; default at
`psiopt.h:416`), and hven's Accelerate LDLT path uses the same fixed value
(`src/linear/accelerate_session.cpp:174-213`).
This statement is only about the shipped value; a future retarget that promises
to preserve psiopt's non-default `set_accel_pivot_tolerance()` setting needs a
separate public-surface decision.

`iparm[4] = store_perm_ = 2` is deliberately not an Options gap. The audit
proved that the returned permutation is never consumed
(`consumed-surface-audit.md:112-121`). Reserved slots, output-slot
initialization, zero-based CSR selection, matrix type, and release phases are
adapter protocol rather than engine configuration. hven owns those acts
internally. Similarly, psiopt's shipped `msglvl_ = false` (`psiopt.cpp:135`)
matches hven's silent backend calls without needing an engine option.

The intended platform factories, after the two explicit-off gaps are fixed or
accepted, are therefore:

```cpp
hven::linear::SymmetricFactor::Options mkl_options;
mkl_options.kind = hven::linear::FactorKind::kLDLT;
mkl_options.num_threads = settings_.qp_threads_;
mkl_options.pivot_perturb_exp = 8;
mkl_options.max_refinement_iters = 0;
mkl_options.ordering = hven::linear::SymmetricFactor::Options::Ordering::kNestedDissection;
mkl_options.weighted_matching = true;
mkl_options.matrix_scaling = false; // GAP: currently does not explicitly write 0
mkl_options.pivot_strategy =
    hven::linear::SymmetricFactor::Options::PivotStrategy::kTwoByTwo;
mkl_options.factorization_algorithm =
    hven::linear::SymmetricFactor::Options::FactorizationAlgorithm::kClassic;
mkl_options.solve_parallelism =
    hven::linear::SymmetricFactor::Options::SolveParallelism::kAdaptivePartitioning;
mkl_options.cnr_threads = 0; // GAP: currently does not explicitly write 0
mkl_options.collect_factor_mflops = true;
mkl_options.accelerate_zero_tolerance = std::nullopt;
```

```cpp
hven::linear::SymmetricFactor::Options accelerate_options;
accelerate_options.kind = hven::linear::FactorKind::kLDLT;
accelerate_options.num_threads = settings_.qp_threads_; // stored by hven; applied externally
accelerate_options.pivot_perturb_exp = 8;
accelerate_options.max_refinement_iters = 0;
accelerate_options.ordering =
    hven::linear::SymmetricFactor::Options::Ordering::kNestedDissection;
accelerate_options.weighted_matching = false;
accelerate_options.matrix_scaling = false;
accelerate_options.pivot_strategy =
    hven::linear::SymmetricFactor::Options::PivotStrategy::kBackendDefault;
accelerate_options.factorization_algorithm =
    hven::linear::SymmetricFactor::Options::FactorizationAlgorithm::kBackendDefault;
accelerate_options.solve_parallelism =
    hven::linear::SymmetricFactor::Options::SolveParallelism::kBackendDefault;
accelerate_options.cnr_threads = 0;
accelerate_options.collect_factor_mflops = false;
accelerate_options.accelerate_zero_tolerance =
    1e-4 * std::numeric_limits<double>::epsilon();
```

These initializers describe the proven values, not the entire historical
psiopt tuning domain. In particular, psiopt's undocumented `iparm[20]` codes
4, 6, 8, and 13 are intentionally absent from hven; the audit records that
decision at `consumed-surface-audit.md:177-198`.

## Ownership and lifecycle

Replace the backend-dependent `kkt_sol_` member with three engine-owned pieces:

1. a `hven::SpMatRM kkt_matrix_` assembly buffer;
2. a `hven::linear::SymmetricFactor kkt_factor_`, constructed from the
   platform options above; and
3. a small compatibility cache containing the last `FactorizeOutcome`, the
   legacy `Eigen::ComputationInfo` projection, and the integer evidence values
   current engine code consumes.

The separate matrix is necessary because psiopt writes values and sometimes a
new pattern through `kkt_sol_.get_matrix()`. hven instead takes the matrix by
reference at `analyze()` and `factorize()` (`symmetric_factor.h:597-622`). Every
old `get_matrix()` use becomes `kkt_matrix_` with no arithmetic change.

The old `compute_internal()` means analyze plus numeric factorization; the old
`refactorize_internal()` means numeric factorization against the existing
symbolic. Preserve `claim_kkt_analysis()`'s decision:

- when it returns true, call `kkt_factor_.analyze(kkt_matrix_)` exactly once,
  then `kkt_factor_.factorize(kkt_matrix_)`;
- otherwise call only `kkt_factor_.factorize(kkt_matrix_)`;
- when variable treatment rebuilds the pattern, refill `kkt_matrix_`, mark the
  analysis stale, and let the next compute path call `analyze()` explicitly.

This keeps the current symbolic-reuse behavior while making the lifecycle
explicit. `release()` reconstructs or clears the factor owner and clears
`kkt_matrix_`; there is no hven `release()` method to call.

## Evidence plumbing

The old Pardiso interface copies output slots after all four numeric entry
paths: `iparm[21]`, `iparm[22]`, `iparm[13]`, `iparm[18]`, and `iparm[17]`
(`pardiso_interface.h:250-255,274-278,335-339,353-357`). Accelerate caches its
native inertia at `accelerate_interface.h:443-457`, reports a factor byte size
and no flop count at lines 466-479, and fabricates `ppivs() == 0` at lines
414-418. The audit's read table records the engine destinations at
`consumed-surface-audit.md:98-106,249-285`.

Capture every `FactorizeOutcome` immediately. On a successful factorization,
the compatibility cache is populated as follows:

| Old engine-facing value | New source | Compatibility projection |
| --- | --- | --- |
| `peigs()` | `outcome.inertia.n_pos` | cache the integer and leave all existing inertia arithmetic unchanged |
| `neigs()` | `outcome.inertia.n_neg` | cache the integer and leave all existing inertia arithmetic unchanged |
| zero count used by diagnostics | `outcome.inertia.n_zero` is available, but current psiopt computes `kkt_dim_ - peigs - neigs` | continue the current computation; do not adopt the honesty state in this retarget |
| `ppivs()` / `IterateInfo::p_pivots_` | `outcome.inertia.perturbed_pivots` | use the value when present; use 0 when absent so the engine sees the same Accelerate value it saw before |
| `result_.factor_mem_` on MKL | `outcome.factor.factor_nonzeros` | require presence and copy the count |
| `result_.factor_mem_` on Accelerate | `outcome.factor.factor_size_bytes` | require presence and copy the byte size; do not fold it into `factor_nonzeros` |
| `result_.factor_flops_` on MKL | `outcome.factor.factor_mflops` | require presence because MKL options set `collect_factor_mflops = true` |
| `result_.factor_flops_` on Accelerate | no hven field exists | write 0, matching the old interface and the existing `> 0` print guard |
| `kkt_sol_.info()` | `outcome.status` and `outcome.backend_code` | cache the same legacy `Eigen::ComputationInfo` category used only for reporting; do not introduce a new control-flow verdict |

`FactorEvidence` deliberately splits an MKL nonzero count, an MKL Mflop
estimate, and an Accelerate byte size (`symmetric_factor.h:125-172`). Keeping
the projection at the engine boundary preserves the current meanings of
`result_.factor_mem_` and `result_.factor_flops_` without corrupting hven's
evidence types.

`collect_factor_mflops = true` is mandatory on MKL. psiopt requests and reads
the report unconditionally, and `init_impl()` copies it to
`result_.factor_flops_` (`psiopt.cpp:3140-3159`). The option is
**reporting-only**: it controls hven's request and public return, not whether
the backend internally performs or avoids the counting work. In particular,
`false` does not guarantee cost avoidance because `pardisoinit` may already
have left `iparm[18] = -1` (`symmetric_factor.h:494-531`).

The engine's inertia consumers stay exactly as they are. `factor_impl()` tests
`neigs - constraint_count`, tests `neigs + peigs - kkt_dim_`, and prints those
same derived values (`psiopt.cpp:1473-1484,1635-1642`). Adopting
`InertiaEvidence::State`, distinguishing query failure from unavailability,
or changing `IterateInfo::p_pivots_` to an optional is outside this retarget.
The compatibility cache presents the same integers the engine saw before.
That boundary does not erase the honest hven evidence; it postpones an engine
policy change so it cannot be confused with the backend replacement.

## Solve mapping

| Old backend operation | hven operation |
| --- | --- |
| Pardiso phase 33, or the Accelerate `SparseSolve` reached through Eigen's `.solve(rhs)` expression | pre-size the destination and call `kkt_factor_.solve(rhs, x)` |
| Pardiso phase 331 | `kkt_factor_.solve_partial(SymmetricFactor::SolvePhase::kForward, rhs, x)` |
| Pardiso phase 332 | `kkt_factor_.solve_partial(SymmetricFactor::SolvePhase::kDiagonal, rhs, x)` |
| Pardiso phase 333 | `kkt_factor_.solve_partial(SymmetricFactor::SolvePhase::kBackward, rhs, x)` |

The old psiopt interface's actual full solve is phase 33
(`pardiso_interface.h:364-409`). The direct psiopt call sites are
`psiopt.cpp:2480` and `psiopt.cpp:3168`; two more are reached through the
`SolverContext` passed at `psiopt.cpp:733` and `psiopt.cpp:1718`
(`psiopt_globalization.cpp:1207,1410`). Preserve the existing direct-assign,
then-negate order so floating-point behavior does not change.

There is **no phase-331/332/333 call site in the psiopt source being retargeted**.
The audit attributes the observed phase-33x force-zero/restore rule to the SQP
seam, specifically `tycho_sqp/.../kkt_system.h:327-335`
(`consumed-surface-audit.md:123-156`). The phase mapping is recorded here so a
future partial-solve consumer has no raw phase arithmetic to invent, not as an
instruction to introduce partial solves into psiopt. With psiopt's required
`weighted_matching = true`, hven also conservatively reports
`supports_partial_solve() == false` (`symmetric_factor.h:661-678`).

Iterative refinement has two rules:

- full solves use `Options::max_refinement_iters`, corresponding to psiopt's
  `iparm[7] = qp_ref_steps_` and Accelerate refinement setters
  (`pardiso_interface.h:625`; `psiopt.cpp:112-113`);
- every hven partial solve forces refinement off for the call, regardless of
  the configured full-solve cap (`symmetric_factor.h:647-659`). This subsumes
  the old save, set-zero, phase-33x call, and restore sequence verified by the
  audit (`consumed-surface-audit.md:123-156`). Callers must not mutate an
  option or raw parameter around `solve_partial()`.

## Call-site inventory

This is the edit checklist for every live old-interface touch in
`psiopt/src/psiopt.cpp` and `psiopt/include/tycho/detail/solvers/jet.h`.
Nearby sites with one replacement are grouped, but every source line is named.

| Source site | Current touch | Replacement |
| --- | --- | --- |
| `psiopt.cpp:92,97,103,105` | Accelerate ordering setters | Populate `Options::ordering`; shipped METIS is `Ordering::kNestedDissection`. |
| `psiopt.cpp:111` | Accelerate solver thread setter | Populate `Options::num_threads`, but retain the external version-split thread action described below because hven applies none on Accelerate. |
| `psiopt.cpp:112-113` | Accelerate iterative-refinement enable/count | One `Options::max_refinement_iters` assignment. |
| `psiopt.cpp:114` | Accelerate pivot tolerance | Remove for the shipped 0.01 value, which hven fixes internally; do not silently drop a non-default user value. |
| `psiopt.cpp:115` | Accelerate zero tolerance | `Options::accelerate_zero_tolerance`. |
| `psiopt.cpp:124` | MKL `ord_` write | Exhaustive mapping to `Options::Ordering`. |
| `psiopt.cpp:129` | MKL `pivotstrat_` write | `Options::pivot_strategy = PivotStrategy::kTwoByTwo` for the proven configuration. |
| `psiopt.cpp:130` | MKL `pivotpert_` write | `Options::pivot_perturb_exp`. |
| `psiopt.cpp:131` | MKL `matching_` write | `Options::weighted_matching`. |
| `psiopt.cpp:132` | MKL `scaling_` write | `Options::matrix_scaling`; blocked for explicit-off parity as noted at the top. |
| `psiopt.cpp:133` | MKL `iterref_` write | `Options::max_refinement_iters`. |
| `psiopt.cpp:134` | MKL `alg_` write | `Options::factorization_algorithm = FactorizationAlgorithm::kClassic`. |
| `psiopt.cpp:135` | MKL `msglvl_` write | Remove; shipped false matches hven's silent backend. Preserve or reject a future non-default setting explicitly. |
| `psiopt.cpp:138` | conditional MKL `threads_` write | `Options::cnr_threads`; blocked for explicit-off parity on the default path. |
| `psiopt.cpp:139` | MKL `parsolve_` write | `Options::solve_parallelism = SolveParallelism::kAdaptivePartitioning`. |
| `psiopt.cpp:140` | `set_params()` | Construct/reconstruct `SymmetricFactor` from the completed platform Options object. |
| `psiopt.cpp:163` | old solver `release()` | Clear/reconstruct the factor owner and clear `kkt_matrix_`; retain `qp_analyzed_ = false`. |
| `psiopt.cpp:624,631` | process-entry Accelerate/MKL thread setters | On MKL, remove in favor of hven's call-scoped `num_threads`. On Accelerate, retain the version-split helper contract. |
| `psiopt.cpp:634` | sparsity transcription through `get_matrix()` | Transcribe into `kkt_matrix_`. |
| `psiopt.cpp:638` | Accelerate internal matrix reinitialization | Delete; hven receives the pattern at explicit `analyze(kkt_matrix_)`. |
| `psiopt.cpp:733` | old solver reference captured in `ClassicMeritAcceptance`'s `SolverContext` | Change the context's solver reference to the compatibility adapter or hven factor-plus-output-buffer view. |
| `psiopt.cpp:878` | `ppivs()` read into iteration record | Read the compatibility cache's `perturbed_pivots.value_or(0)` projection. |
| `psiopt.cpp:1362` | trial evaluation through `get_matrix()` | Pass `kkt_matrix_`. |
| `psiopt.cpp:1474,1481,1484` | inertia reads | Read cached `n_neg`/`n_pos`; do not change the arithmetic. |
| `psiopt.cpp:1503` | `info()` read | Read the cached reporting-only `Eigen::ComputationInfo` projection. |
| `psiopt.cpp:1514,1519` | primal/constraint diagonal perturbations through `get_matrix()` | Mutate `kkt_matrix_`. |
| `psiopt.cpp:1516` | `refactorize_internal()` lambda | Call `factorize(kkt_matrix_)` only and refresh the compatibility cache. |
| `psiopt.cpp:1517` | `compute_internal()` lambda | Call `analyze(kkt_matrix_)`, then `factorize(kkt_matrix_)`, and refresh the cache. |
| `psiopt.cpp:1639-1640` | exhausted-ladder inertia diagnostics | Use the same cached `n_pos`/`n_neg` integers and unchanged derived-zero expression. |
| `psiopt.cpp:1718` | per-phase `SolverContext` old solver reference | Same context adapter change as line 733; this reaches the predictor and SOC full solves. |
| `psiopt.cpp:1789,1807,1838` | main evaluation, barrier Hessian, and callback through `get_matrix()` | Pass `kkt_matrix_`. |
| `psiopt.cpp:2480` | main full solve expression | Pre-size/use `DXSL`, call `kkt_factor_.solve(RHS, DXSL)`, then negate in the existing next statement. |
| `psiopt.cpp:3113,3134` | initializer evaluation/slack Hessian through `get_matrix()` | Pass `kkt_matrix_`. |
| `psiopt.cpp:3140,3142` | initializer compute/refactor | Map to analyze-plus-factorize or factorize-only, respectively. |
| `psiopt.cpp:3148,3149` | raw flop/memory member reads | Project `FactorEvidence` as specified above. |
| `psiopt.cpp:3168` | initializer full solve expression | Size `dx`, call `kkt_factor_.solve(RHS, dx)`, then retain the next-statement negate. |
| `psiopt.cpp:3308,3312` | per-solve-entry Accelerate/MKL thread setters | Same rule as lines 624/631: remove MKL direct mutation; retain Accelerate's version-split action. |
| `psiopt.cpp:3342` | rebuilt sparsity through `get_matrix()` | Transcribe into `kkt_matrix_`. |
| `psiopt.cpp:3344` | Accelerate reinitialization after rebuilt pattern | Delete; the stale-analysis path calls hven `analyze()` explicitly. |
| `jet.h:55,56,172` | MKL thread-local single-thread guard | Keep only if Jet intends to pin non-hven MKL work too. The factor itself gets the Jet-adjusted `qp_threads_` through Options and restores the caller's prior local value per call. |
| `jet.h:150,170` | Accelerate single-thread requests | Retain the version-split helper; hven `num_threads` does not replace these calls. |

The context change also requires replacing its backend-dependent
`KktSolverType` alias and the full solves at
`psiopt_globalization.cpp:1207,1410`. Those calls are downstream of the two
`SolverContext` construction sites above; listing them here prevents a compile
failure from being mistaken for a new design decision.

## Migration hazards

### Accelerate ordering is an explicit request

Set `ordering = Options::Ordering::kNestedDissection` on both platforms.
`kBackendDefault` is not equivalent on Accelerate: hven maps it to Apple's
`SparseOrderDefault`, documented as AMD for symmetric matrices, while psiopt
explicitly selected `SparseOrderMetis` and defaulted `qp_ord_` to METIS. This
hazard is stated in the public header itself (`symmetric_factor.h:264-300`) and
confirmed from psiopt by the audit (`consumed-surface-audit.md:308-328`).

### macOS threading remains version-split

Do not treat `Options::num_threads` as replacing psiopt's Accelerate control.
The old helper's exact contract is:

- on macOS 15 and later, `BLASSetThreading` is per-calling-thread through
  thread-local storage and is binary: `<= 1` selects single-threaded; `> 1`
  returns to the cached multi-thread maximum;
- before macOS 15, the fallback writes `VECLIB_MAXIMUM_THREADS`, which is
  process-global and is effective only before the first BLAS call;
- `VECLIB_MAXIMUM_THREADS` is cached at the first BLAS call, so startup must
  set it before initialization; a later `setenv` is inert.

Those facts are in `accelerate_utils.h:21-31,108-140` and summarized by the
audit at `consumed-surface-audit.md:287-306`. Keep the startup initialization,
the solve-entry repair at `psiopt.cpp:3307-3313`, and Jet's per-worker pin at
`jet.h:163-173` until an hven surface explicitly adopts the same contract.
Whether these BLAS controls govern Accelerate Sparse itself remains unobserved;
the audit says exactly that at `consumed-surface-audit.md:300-306`.

### Inertia policy does not change here

The engine continues to consume the same integer projections and make the same
inertia-correction decisions. hven's honest `State`, derived-zero marker, and
optional perturbed-pivot counter remain visible at the adapter boundary, but
the engine does not adopt them in this change. This keeps the backend retarget
separate from an algorithm-policy change and satisfies the requirement that
the adapter present the values the engine saw before.

## Behavioral-delta ledger

Every old-vs-new golden-rig difference must appear in this ledger. Anything
else is unexplained and blocks the migration gate.

| Golden behavior | Expected result | Reason |
| --- | --- | --- |
| `P1_IterateLoopLifecycle` | unchanged | Analyze/factorize/solve counts and numerical outputs retain the current lifecycle; only the owning API changes. |
| `P2_InertiaCorrectionLadderReplay` | unchanged | The compatibility cache supplies the same positive/negative counts to the unchanged ladder arithmetic. |
| `P3_SingularVerdictAndControl` | unchanged | The engine's singularity test remains `n_neg + n_pos - dim != 0`; no honesty-state policy is adopted. |
| `P4_PerturbationEvidencePresenceIsBackendHonest`, MKL | unchanged | Pardiso's `iparm[13]` count moves to `InertiaEvidence::perturbed_pivots` with the same value. |
| `P4_PerturbationEvidencePresenceIsBackendHonest`, Accelerate | **legitimate raw linear-layer difference** | Old psiopt returns a present hardcoded 0; hven returns absence. Named docket: [`2026-08-09-psiopt-accelerate-perturbed-pivots.md`](dockets/2026-08-09-psiopt-accelerate-perturbed-pivots.md), especially lines 16-46 and 78-103. The engine adapter still projects absence to 0 for this retarget. |
| `P5_InertiaBeforeFactorizationIsAnExplicitState`, MKL | golden rows unchanged; implementation defect removed | The old members are uninitialized, so the rig adapter refuses to read them and reports `kUnavailable`; hven intrinsically reports `kUnavailable`. Named docket: [`2026-08-09-psiopt-mkl-inertia-before-factorization.md`](dockets/2026-08-09-psiopt-mkl-inertia-before-factorization.md), lines 21-49 and 76-99. There is no defined old value to preserve. |
| `P5_InertiaBeforeFactorizationIsAnExplicitState`, Accelerate | **legitimate raw linear-layer difference** | Old psiopt reports an observed-looking `(0,0,0)` before factorization and after query failure; hven reports `kUnavailable` before factorization and `kQueryFailed` on query failure, with invalid counts. Named docket: [`2026-08-09-psiopt-accelerate-inertia-zero-fill.md`](dockets/2026-08-09-psiopt-accelerate-inertia-zero-fill.md), lines 14-63 and 114-150. The old-seam Apple half remains unobserved. |
| `P6_RefinementStepEvidence` | unchanged | Full-solve cap 0 is preserved; any later nonzero cap maps to `max_refinement_iters`. Partial solves, if ever introduced, are always refinement-off. |
| factor size and flop reporting | unchanged at the engine boundary | MKL nonzeros, Accelerate bytes, MKL Mflops, and Accelerate zero are projected back into the same two result fields. `collect_factor_mflops = true` prevents losing psiopt's unconditional MKL report. |
| solver-status reporting | unchanged | The cached `Eigen::ComputationInfo` projection remains reporting-only, as the current `CheckInfo` lambda is (`psiopt.cpp:1490-1511`). |
| thread mechanism | unchanged only if the external Accelerate contract above is retained | hven's absent Accelerate control is already a known linear-surface difference; dropping psiopt's helper would be a new, unexplained engine behavior change. |
| explicit MKL option-write observations | effect-equal by ruled decision; drift-guarded | `iparm[10] = 0` and `iparm[33] = 0` are not reproduced as acts (see the adjudicated verdict at the top): the observed values are equal because `pardisoinit` supplies both zeros, and the canary test pins exactly that premise so a backend-default move fails loudly instead of silently. |

The only licensed raw linear-layer numerical/evidence differences are the
three named docket cases above: MKL pre-factorization undefined state removed,
Accelerate pre-factorization/query-failure zero fill replaced by explicit
states, and Accelerate's fabricated perturbed-pivot zero replaced by absence.
The compatibility adapter deliberately prevents those evidence-shape changes
from becoming simultaneous engine-policy changes.
