# M8 AP-2 B8: the naming rules and the rename table

Date: 2026-11-05 (riders 2026-11-06). Base: `m8` at `fa727270` (B7 landed); riders on `cf164dcf`. This is B8's record for the AP-1 page
(`2026-11-m8-ap1-api-review.md` §7 and §13, group B8) and the reference tycho's catch-up reads.
After B8 and its riders (C242/C243) the public names are frozen until the next break window, the
`advanced` option tiers included.

## 1. The rules

A reader who knows these rules should be able to predict any public name. (Amended by the B8 riders,
C242/C243; the amendments are marked *[riders]*.)

**N — namespaces.** The library is `hven`. The factorization layer is `hven::linear`. Everything
declared in a `detail/` header is in `hven::detail` (`hven::linear::detail` for the factor layer)
*[riders: the internal types and functions of `detail/` headers moved there from `hven`]*; test seams
are `hven::detail::testing`. There is no other namespace. A `detail/` header that specializes a public
template (`SolverInterfaceAdapter<...>`) does so in `hven`, by its qualified name.

**H — headers.** One umbrella, `<hven/hven.h>`, includes every public header; the export contract
checks that it does. A header is named for the principal type it declares, in `snake_case`
(`SolverIndexingData` → `solver_indexing_data.h`, `WarmStart` → `warm_start.h`, `JsonLinesTraceSink` →
`json_lines_trace_sink.h` *[riders]*), or for the family it declares when there are several
(`piece_interfaces.h`). The directories are kept (AP-1 §7). No forwarding headers (C201); a header that
only gathered others is deleted (`model/pieces.h` *[riders]*). Translation units keep their names.

**T — types.** `PascalCase` words. An acronym is written as a word: `Nlp`, `Qp`, `Kkt`, `Ipm`, `Sqp`,
`Ipqp`, `Ssn`, `Io`, `Lbfgs`. A type that belongs to one component carries that component's prefix:
`Ipm` (the interior-point engine), `Sqp` (the SQP major-iteration driver), `Qp` (the SQP's subproblem
tier: its dispatch and the walk engine), `Ssn`, `Ipqp` (the two other subproblem kernels). Shared
types carry none (`SolveStatus`, `Verbosity`, `StartLevel`, `WarmStart`, `StepVerdict`).
Exceptions, stated: the Eigen-shaped aliases of `core/types.h` keep Eigen's short vocabulary (`Vec`,
`Mat`, `SpMatRM`, `VecRef`, ...); the factor layer's types are scoped by `hven::linear` and carry no
prefix (`linear::Decomposition`, `SymmetricFactor::Options::Ordering`).

**R — trace records** *[riders]*. A trace-event type is `<Prefix><Event>TraceEvent`, `<Event>` the
schema event it mirrors in its own spelling (`IpmIterTraceEvent` for `ipm.iter`, `IpqpRegTraceEvent`
for `ipqp.reg`, `QpModeTraceEvent` for `qp.mode`). A value type used only inside trace events is
`<Prefix>Trace<Noun>`, the prefix that of the component whose events use it (`IpqpTraceRegDirection`,
`IpqpTraceRouteTarget`, `QpTraceMode`, `QpTraceModeSite`, `SqpTraceFallbackVerdict`). Trace keys and
the strings an event prints are frozen (schema v0) and never follow a type or enumerator rename.

**E — enumerators and enum names.** Enumerators are `kPascalCase` words, acronyms as words
(`Decomposition::kLdlt`), and name what they select rather than abbreviate it
(`IpmLineSearchMode::kAugmentedLagrangian`, not `kAugLang`; `StepVerdict::kAcceptObjectiveStep`, not
`kAcceptF` *[riders]*). One concept, one enumerator: an iteration cap reached is `kIterationLimit` on
every status and stop-reason enum *[riders]*; `to_string` keeps each enum's frozen string. An enum type
is singular. Its suffix says what kind of choice it is: `Mode` selects how a component runs (which
algorithm or behaviour); `Rule` selects the formula for one quantity (`IpmStepLengthRule`,
`IpqpBarrierInitialRule` *[riders]*); a noun names the thing chosen when the choice IS that thing
(`Engine`, `Criterion`, `Treatment`, `Policy`, `Scaling`, `Curvature`, `Phase`, `Level`, `Ordering`,
`PivotStrategy`, `SolveParallelism`, `Decomposition`). `Kind` is reserved for enums that classify a
reported value (`IpmMessageKind`, `NlpRowKind`) and is never an option type. A public option enum has
no enumerator its option refuses (hence `IpmSolvePhaseMode` without `kInit`, `linear::Decomposition`
without `kLlt`/`kLu` *[riders]*).
 - *[riders]* An option enum read by one component carries that component's prefix (rule T):
   `IpmAcceptanceMode`, `IpmBarrierMode`, `IpmOrdering`, ... ; `SqpHessianMode`, `SqpElasticRungEngine`;
   the subproblem tier's own enums keep the tier's prefix even though `SqpOptions` holds them
   (`QpMode`, `QpWorkingSetAlgebra`, `SsnSigmaRule`, `IpqpBarrierInitialRule`). Bare only when shared
   (`Verbosity`, `StartLevel`).
 - *[riders]* One vocabulary across layers: an engine option that configures the factor layer uses
   that layer's words (`IpmLinearOptions::ordering: IpmOrdering`, `pivot_strategy: IpmPivotStrategy`,
   `solve_parallelism: IpmSolveParallelism::kAdaptivePartitioning`, `weighted_matching`,
   `matrix_scaling`, `pivot_perturbation_exponent`, `max_refinement_steps`, as
   `linear::SymmetricFactor::Options` spells them). The engine keeps its own enum when the layer's
   enum has enumerators the engine refuses (`kBackendDefault`, the no-auto-refine pivots).

**M — data members.** A data member is spelled `snake_case_` (trailing underscore) if and only if it
is private or protected. A public data member is bare `snake_case`, whatever the type (CLAUDE.md §4:
aggregates, settings, records, events; and the raw piece protocol's public structs).
 - *[riders]* A public record member is words, like an option field: `last_feasibility_restoration_entries`,
   not `last_feas_rest_entries`; `column_spread`, not `col_spread`; `second_order_corrections`, not
   `soc_steps_taken`; `factor_memory`. A duration is `<what>_seconds` (`IpmTiming::kkt_seconds`,
   `SqpResult::engine_seconds`, `SolveResult::wall_seconds`). A member holding another type's value is
   named for the type (`SqpResult::sqp_warm_start: SqpWarmStart`, beside `warm_start: WarmStart`).
   Math notation stays where the member IS the symbol of the engine's formulation (`x`, `lambda`, `z`,
   `g`, `f`, `mu`, `rho`, `NlpDimensions::n`/`m` -- the Ipopt TNLP vocabulary `Nlp` mirrors).
 - Exceptions, stated: a member that is serialized as a trace key keeps the key's spelling
   (`IpmIterate::prim_obj` is key `prim_obj`; every member of `SqpIterate` and of the `*TraceEvent`
   types); a member of a counter or ledger record keeps the counters contract's name
   (`docs/counters.md`; `SqpCounters::major_iters`), which frozen evidence CSV columns carry.

**C — counts** *[riders]*. A function returning a count is `num_<things>()`
(`Problem::num_variables()`, `num_constraints()`, `num_partitions()`). A record member holding a count
is the plural noun (`ProblemStatistics::variables`, `equality_rows`; `IpmResult::second_order_corrections`).
A cap is `max_<counted things>` (rule O). A thread count is `threads` everywhere: the field
(`CommonOptions::threads`, `linear::SymmetricFactor::Options::threads`,
`conditional_reproducibility_threads`), the accessor (`SqpSolver::ssn_tier_threads()`,
`DenseSymmetricFactor::threads()`) and the setter (`set_threads`). A free function is named for what it
reads or writes (`serialize_warm_start`, `serialize_ipm_polish`).

**O — option fields.** Option fields are `snake_case` words; an abbreviation is expanded
(`trust_region_initial`, not `tr_init`; `working_set_algebra`, not `ws_algebra`; and in the `advanced`
tiers *[riders]* `second_order_correction`, not `enable_soc`; `primal_regularization_initial`, not
`rho_init`; `lbfgs_update_on_reject`, not `qn_update_on_reject`), except the `_tol` suffix and
established names (`lbfgs`, `ruiz`, `farkas`, the IPOPT-named `scaling_max_gradient`, `bound_push`).
 - Tolerances are `<measure>_tol` (`optimality_tol`, `acceptable_kkt_tol`, `accelerate_zero_tol`).
 - A cap on a count is `max_<counted things>` (`max_iterations`, `max_factorizations`,
   `max_restoration_entries`, `max_dense_dimension`, `max_schur_border_dimension`,
   `max_consecutive_qp_failures`, `max_warm_iterations`); a second cap on the same count takes a
   qualifier in front (`IpqpOptions::hard_max_iterations`).
 - A bound or start value of a parameter is `<parameter>_initial` / `_max` / `_min`
   (`barrier_initial`, `barrier_max`, `trust_region_min`, `regularization_min`, `scaling_factor_max`),
   Ipopt's `mu_init`/`mu_max` order. The same parameter has the same name on every engine
   (`fraction_to_boundary`, `barrier_initial`, `barrier_min` on the IPM and the IPQP tier).
 - A field holding an option enum is that enum's name in `snake_case` with the component prefix dropped
   -- the struct already names the component -- and, when the enum's last words repeat the struct's
   context, those words dropped too; an optional context word may lead (`acceptance_mode:
   IpmAcceptanceMode`, `optimize_barrier_mode: IpmBarrierMode`, `fixed_variable_treatment:
   IpmFixedVariableTreatment`, `column_scaling: SqpColumnScaling`, `restoration_curvature:
   SqpRestorationCurvature`, `elastic_rung_engine: SqpElasticRungEngine`, `working_set_algebra:
   QpWorkingSetAlgebra`, `barrier_initial_rule: IpqpBarrierInitialRule`, `decomposition:
   linear::Decomposition`) *[riders: amended to match the code]*.
 - A `bool` is named for the behaviour it turns on, without `enable_` (`row_scaling`, `return_best`,
   `second_order_correction`, `ruiz_equilibration`).
 - One concept has one name and one floor on both engines (`max_restoration_entries >= 0` on both;
   0 refuses restoration). Its counted unit is stated where it differs: the interior-point engine counts
   restoration entries per phase, the SQP per solve *[riders]*.
 - A public struct's layout does not depend on a macro a consumer may not share: a backend-specific
   field is declared on every platform and documented as ignored where it does not apply
   (`IpmLinearOptions::accelerate_zero_tol`) *[riders]*. A build-supplied default has an `#ifndef`
   fallback in the header that reads it (`HVEN_DEFAULT_IPM_THREADS`).

**D — comments** *[riders]*. A public header's comments state constraints tersely: the rule, the
unit, the default, the refusal. Milestone, task and review codenames, settlement history and CI run ids
live in `docs/notes`, not in a public header.

**Scope.** The rules bind the public surface (everything a public header declares), the `advanced`
option tiers included *[riders: swept by C242; they freeze with the rest]*. Internals under `detail/`
follow N (namespace) and the trailing-underscore half of M; their names were changed where a public
rename reached them. Trace keys, golden lines, counter names and frozen evidence CSV headers are not
renamed (CLAUDE.md §1), and neither are a bench's CLI lever names or the strings it prints.

## 2. The behaviour-neutral exceptions B8 carries

- `max_restoration_entries` on the SQP accepts 0 (it refused it): the first restoration request
  is then refused by the exit a refused later request takes (`kNumericalError`,
  `SqpStopReason::kRestorationRefused`). Every value accepted before behaves as before.
- `AlgorithmMode::kInit` is no longer a public option value: `IpmAdvancedOptions::solve_phase_mode`
  is a `SolvePhaseMode` of three values, mapped once per phase onto the engine's internal mode.
- Option dumps and validation messages print the new names.
- The version is 0.2.0 (`CHANGELOG.md`).

The riders (C242/C243; evidence `docs/notes/data/2026-11-m8-ap2-b8-riders/`) add:

- `IpmLinearOptions::accelerate_zero_tol` is declared on every platform (it was under
  `USE_ACCELERATE_SPARSE`): validated finite and > 0 everywhere, ignored on MKL, and printed by
  `dump_options` everywhere (68 lines). The struct's layout moves with it on MKL builds
  (`sizeof(IpmLinearOptions)` 48 → 56, `sizeof(IpmOptions)` 472 → 480 on Linux x86-64); it sits after
  `dense_factorization`, where the padding leaves the smallest growth.
- `linear::Decomposition` has the one value both backends implement; a value outside it throws
  `std::logic_error` at construction as `kLlt`/`kLu` did, with the new message.
- `HVEN_DEFAULT_IPM_THREADS` replaces `HVEN_DEFAULT_QP_THREADS` as the target's PUBLIC compile
  definition (every TU that links `hven::hven` sees the new name; same value, 8) and has an `#ifndef`
  fallback in `drivers/ipm_options.h`.
- Messages print the new names (`deserialize_warm_start: ...`, `SymmetricFactor::set_threads: ...`,
  `advanced.ipqp.barrier_initial ...`), and so do option dumps (`kIpqp`, `kAdaptivePartitioning`).
- The install smoke has 36 standalone-include TUs (37: `model/pieces.h` deleted, `trace_writer.h`
  renamed).

## 3. The complete old → new table

Generated from the manifests the renames were applied from, composed so a name renamed by B8 and again
by a rider reads oldest → final (`docs/notes/data/2026-11-m8-ap2-b8-riders/manifest/gen_tables.py`,
reading `docs/notes/data/2026-11-m8-ap2-b8/manifest/` and `docs/notes/data/2026-11-m8-ap2-b8-riders/manifest/`).
### Namespaces

| old | new |
|---|---|
| `hven::solvers::X` | `hven::X (every public name)` |
| `hven::solvers::detail::X` | `hven::detail::X` |
| `hven::solvers::detail::testing::X` | `hven::detail::testing::X` |
| `hven::utils::TypeStorage, hven::utils::TypeStorageBase` | `hven::TypeStorage, hven::TypeStorageBase` |
| `hven::utils::X (internal utilities), hven::utils::detail::X` | `hven::detail::X` |
| internal types and functions declared in `detail/` headers (were hven::solvers::X, then hven::X) | `hven::detail::X (the list: B8 riders manifest/detail_moved_names.txt)` |
| hven::GlobalizationStrategy (named by `SqpAdvancedOptions::make_strategy`) | `hven::detail::GlobalizationStrategy` |
| `hven::linear, hven::linear::detail` | `unchanged` |

### Headers

| old | new |
|---|---|
| `<hven/model/indexing_data.h>` | `<hven/model/solver_indexing_data.h>` |
| `<hven/model/sizing_specs.h>` | `<hven/model/sizable_spec.h>` |
| `<hven/model/solver_interface_specs.h>` | `<hven/model/piece_interfaces.h>` |
| `<hven/solver_interface_adapter.h>` | `<hven/model/solver_interface_adapter.h>` |
| `<hven/drivers/ipm_iterate_info.h>` | `<hven/drivers/ipm_iterate.h>` |
| `<hven/warmstart/warm_start_data.h>` | `<hven/warmstart/warm_start.h>` |
| `<hven/drivers/trace_writer.h>` | `<hven/drivers/json_lines_trace_sink.h>` |
| `<hven/model/pieces.h>` | deleted; `<hven/hven.h>` includes every public header |
| (none) | `<hven/hven.h>`, the umbrella: every public header |

### Types and constants

| old | new |
|---|---|
| `NLPAdapterCore` | `NlpAdapterCore` |
| `NLPConstraintPiece` | `NlpConstraintPiece` |
| `NLPObjectivePiece` | `NlpObjectivePiece` |
| `NLPCoordinate` | `NlpCoordinate` |
| `NLPRowClassification` | `NlpRowClassification` |
| `NLPRowKind` | `NlpRowKind` |
| `ConstKKTVector` | `ConstKktVector` |
| `KKTVector` | `KktVector` |
| `ParsedIOFlags` | `ParsedIoFlags` |
| `IterateInfo` | `IpmIterate` |
| `IpCrossoverOptions` | `IpmCrossoverOptions` |
| `kIpActivityFactor` | `kIpmActivityFactor` |
| `AcceptanceKind` | `IpmAcceptanceMode` |
| `BarrierGovernorKind` | `IpmBarrierGovernorMode` |
| `AlgorithmMode` (public, `IpmAdvancedOptions::solve_phase_mode`) | `IpmSolvePhaseMode`; `AlgorithmMode` is internal |
| `FixedVariableTreatment` | `IpmFixedVariableTreatment` |
| `KktOrdering` | `IpmOrdering` |
| `KktPivoting` | `IpmPivotStrategy` |
| `KktSolveParallelism` | `IpmSolveParallelism` |
| `BarrierMode` | `IpmBarrierMode` |
| `LineSearchMode` | `IpmLineSearchMode` |
| `PrimalDualStepStrategy` | `IpmStepLengthRule` |
| `MeritPenaltyRule` | `IpmMeritPenaltyRule` |
| `RestorationMode` | `IpmRestorationMode` |
| `InertiaMode` | `IpmInertiaMode` |
| `BestIterateCriterion` | `IpmBestIterateCriterion` |
| `ElasticRungEngine` | `SqpElasticRungEngine` |
| `WorkingSetLinearAlgebra` | `QpWorkingSetAlgebra` |
| `IpqpMu0Rule` | `IpqpBarrierInitialRule` |
| `FactorKind` | `Decomposition` |
| `SolveOverrides` | `QpSolveOverrides` |
| `IpqpTraceIterEvent` | `IpqpIterTraceEvent` |
| `IpqpTraceRegEvent` | `IpqpRegTraceEvent` |
| `IpqpTraceRestartEvent` | `IpqpRestartTraceEvent` |
| `IpqpTraceRouteEvent` | `IpqpRouteTraceEvent` |
| `IpqpTraceCertifyEvent` | `IpqpCertifyTraceEvent` |
| `IpqpTraceEscapeEvent` | `IpqpEscapeTraceEvent` |
| `IpqpTraceRegDir` | `IpqpTraceRegDirection` |
| `IpqpTraceRouteTo` | `IpqpTraceRouteTarget` |
| `IpqpTraceQpMode` | `QpTraceMode` |
| `IpqpTraceOutcome` | `QpTraceOutcome` |
| `QpModeSite` | `QpTraceModeSite` |
| `SqpFallbackVerdict` | `SqpTraceFallbackVerdict` |
| `kSeededIqMultFloor` | `kSeededInequalityMultiplierFloor` |
| `kSeededMultInitMax` | `kSeededMultiplierMax` |
| `linear::FactorKind` | `linear::Decomposition` (only `kLdlt`; `kLlt`/`kLu`, refused by both backends, removed) |
| `HVEN_DEFAULT_QP_THREADS` (macro) | `HVEN_DEFAULT_IPM_THREADS` (with an `#ifndef` fallback of 8) |

### Enumerators

| enum (final name) | old | new |
|---|---|---|
| `IpmSolvePhaseMode` (was `AlgorithmMode`) | `kOpt`, `kOptNo`, `kSoe` | `kOptimize`, `kOptimizeWithoutObjective`, `kSolveEquations` |
| `IpmSolvePhaseMode` (was `AlgorithmMode`) | `kInit` | removed from the public enum (it is never a phase mode) |
| `IpmLineSearchMode` (was `LineSearchMode`) | `kAugLang` | `kAugmentedLagrangian` |
| `IpmLineSearchMode` (was `LineSearchMode`) | `kLang` | `kLagrangian` |
| `IpmLineSearchMode` (was `LineSearchMode`) | `kNoLs` | `kNone` |
| `IpmStepLengthRule` (was `PrimalDualStepStrategy`) | `kPrimSlackEqSplitIq` | `kEqualityMultipliersPrimal` |
| `IpmStepLengthRule` (was `PrimalDualStepStrategy`) | `kPrimSlackSplitEqIq` | `kEqualityMultipliersDual` |
| `IpmStepLengthRule` (was `PrimalDualStepStrategy`) | `kMaxEq` | `kEqualityMultipliersMaximum` |
| `IpmMeritPenaltyRule` (was `MeritPenaltyRule`) | `kWmno` | `kSinglePenalty` |
| `linear::Decomposition` (was `FactorKind`) | `kLDLT` | `kLdlt` |
| `linear::Decomposition` (was `FactorKind`) | `kLLT` | `kLlt` -- removed by the riders |
| `linear::Decomposition` (was `FactorKind`) | `kLU` | `kLu` -- removed by the riders |
| `TypeStorage::Kind` | `Empty` | `kEmpty` |
| `TypeStorage::Kind` | `Inline` | `kInline` |
| `TypeStorage::Kind` | `Heap` | `kHeap` |
| `HessOwner` | `Objective` | `kObjective` |
| `HessOwner` | `EqPiece` | `kEqualityPiece` |
| `HessOwner` | `IqPiece` | `kInequalityPiece` |
| `NlpRowKind` | `Equality` | `kEquality` |
| `NlpRowKind` | `UpperBounded` | `kUpperBounded` |
| `NlpRowKind` | `LowerBounded` | `kLowerBounded` |
| `NlpRowKind` | `Range` | `kRange` |
| `NlpRowKind` | `Free` | `kFree` |
| `ParsedIoFlags` | `HiddenInput` | `kHiddenInput` |
| `ParsedIoFlags` | `IngoreOutput` | `kIgnoreOutput` |
| `ParsedIoFlags` | `NotContiguous` | `kNotContiguous` |
| `ParsedIoFlags` | `Contiguous` | `kContiguous` |
| `ThreadingFlags` | `RoundRobin` | `kRoundRobin` |
| `ThreadingFlags` | `MainThread` | `kMainThread` |
| `ThreadingFlags` | `NeedsPool` | `kNeedsPool` |
| `ThreadingFlags` | `ByApplication` | `kByApplication` |
| `ThreadingFlags` | `ThreadN` | `kThreadN` |
| `QpMode` | `kIpm` | `kIpqp` (the trace string is unchanged) |
| `IpmSolveParallelism` | `kAdaptive` | `kAdaptivePartitioning` |
| `SolveStatus`, `QpStatus`, `RestorationSubStatus` | `kMaxIter` | `kIterationLimit` (`to_string` keeps the frozen string) |
| `IpmStopReason` | `kIterationCap` | `kIterationLimit` (`to_string` keeps the frozen string) |
| `StepVerdict` | `kAcceptF` | `kAcceptObjectiveStep` (`to_string` keeps the frozen string) |
| `StepVerdict` | `kAcceptH` | `kAcceptFeasibilityStep` (`to_string` keeps the frozen string) |

### Members, functions and option fields

| struct (final name) | old | new |
|---|---|---|
| `IpmIterate` | `iter_` | `iter` |
| `IpmIterate` | `mu_` | `mu` |
| `IpmIterate` | `prim_obj_` | `prim_obj` |
| `IpmIterate` | `barr_obj_` | `barr_obj` |
| `IpmIterate` | `kkt_inf_` | `kkt_inf` |
| `IpmIterate` | `barr_inf_` | `barr_inf` |
| `IpmIterate` | `econ_inf_` | `econ_inf` |
| `IpmIterate` | `icon_inf_` | `icon_inf` |
| `IpmIterate` | `pen_par1_` | `pen_par1` |
| `IpmIterate` | `pen_par2_` | `pen_par2` |
| `IpmIterate` | `ls_iters_` | `ls_iters` |
| `IpmIterate` | `alpha_p_` | `alpha_p` |
| `IpmIterate` | `alpha_d_` | `alpha_d` |
| `IpmIterate` | `alpha_t_` | `alpha_t` |
| `IpmIterate` | `h_pert_` | `h_pert` |
| `IpmIterate` | `h_facs_` | `h_facs` |
| `IpmIterate` | `h_pert_cum_` | `h_pert_cum` |
| `IpmIterate` | `prox_reg_primal_` | `prox_reg_primal` |
| `IpmIterate` | `prox_reg_dual_` | `prox_reg_dual` |
| `IpmIterate` | `p_pivots_` | `p_pivots` |
| `IpmIterate` | `p_pivots_observed_` | `p_pivots_observed` |
| `IpmIterate` | `max_e_mult_` | `max_e_mult` |
| `IpmIterate` | `max_i_mult_` | `max_i_mult` |
| `IpmIterate` | `merit_val_` | `merit_val` |
| `IpmIterate` | `accepted_` | `accepted` |
| `IpmIterate` | `first_rejection_iter_` | `first_rejection_iter` |
| `IpmIterate` | `theta_at_first_rejection_` | `theta_at_first_rejection` |
| `IpmIterate` | `eval_exceptions_` | `eval_exceptions` |
| `VariableBound` | `index_` | `index` |
| `VariableBound` | `lower_` | `lower` |
| `VariableBound` | `upper_` | `upper` |
| `AssemblyDeclaration` | `objectives_` | `objectives` |
| `AssemblyDeclaration` | `equality_constraints_` | `equality_constraints` |
| `AssemblyDeclaration` | `inequality_constraints_` | `inequality_constraints` |
| `AssemblyDeclaration` | `primal_vars_` | `primal_vars` |
| `AssemblyDeclaration` | `equality_rows_` | `equality_rows` |
| `AssemblyDeclaration` | `inequality_rows_` | `inequality_rows` |
| `AssemblyDeclaration` | `fixing_rows_` | `fixing_rows` |
| `AssemblyDeclaration` | `equality_shared_row_overcount_` | `equality_shared_row_overcount` |
| `AssemblyDeclaration` | `inequality_shared_row_overcount_` | `inequality_shared_row_overcount` |
| `AssemblyDeclaration` | `partition_count_` | `partition_count` |
| `AssemblyDeclaration` | `rows_derived_` | `rows_derived` |
| `AssemblyDeclaration` | `variable_bounds_` | `variable_bounds` |
| `CandidatePoint` | `x_` | `x` |
| `CandidatePoint` | `equality_multipliers_` | `equality_multipliers` |
| `CandidatePoint` | `inequality_multipliers_` | `inequality_multipliers` |
| `CandidatePoint` | `objective_scale_` | `objective_scale` |
| `CandidateValues` | `objective_` | `objective` |
| `CandidateValues` | `equality_residuals_` | `equality_residuals` |
| `CandidateValues` | `inequality_residuals_` | `inequality_residuals` |
| `CandidateFirstOrder` | `values_` | `values` |
| `CandidateFirstOrder` | `objective_gradient_` | `objective_gradient` |
| `CandidateFirstOrder` | `constraint_adjoint_gradient_` | `constraint_adjoint_gradient` |
| `ClaimBlock` | `start_` | `start` |
| `ClaimBlock` | `count_` | `count` |
| `SolverFunctionBase` | `function_` | `function` |
| `SolverFunctionBase` | `index_data_` | `index_data` |
| `ConstraintFunction` | `function_` | `function` |
| `ConstraintFunction` | `index_data_` | `index_data` |
| `ObjectiveFunction` | `function_` | `function` |
| `ObjectiveFunction` | `index_data_` | `index_data` |
| `ConstraintInterface` | `storage_` | `storage` |
| `ObjectiveInterface` | `storage_` | `storage` |
| `ConstraintModel` | `data_` | `piece` |
| `ObjectiveModel` | `data_` | `piece` |
| `DeclarationKey` | `declaration_digest_` | `declaration_digest` |
| `DeclarationKey` | `bound_digest_` | `bound_digest` |
| `IdentityProbe` | `epoch_` | `epoch` |
| `IdentityProbe` | `value_digest_` | `value_digest` |
| `ModelStructureKey` | `claim_digest_` | `claim_digest` |
| `ModelStructureKey` | `partition_count_` | `partition_count` |
| `ModelStructureKey` | `bound_digest_` | `bound_digest` |
| `IpmPolishData` | `mu_` | `mu` |
| `IpmPolishData` | `z_lower_` | `z_lower` |
| `IpmPolishData` | `z_upper_` | `z_upper` |
| `IpmPolishData` | `iq_values_` | `iq_values` |
| `KktClaimSpace` | `rows_` | `rows` |
| `KktClaimSpace` | `cols_` | `cols` |
| `KktClaimSpace` | `next_free_` | `next_free` |
| `KktClaimSpace` | `constraint_row_offset_` | `constraint_row_offset` |
| `KktClaimSpace` | `domains_` | `domains` |
| `KktClaimSpace` | `storage_` | `storage` |
| `KktScatterView` | `values_` | `values` |
| `KktScatterView` | `size_` | `size` |
| `KktScatterView` | `locations_` | `locations` |
| `RhsArenaView` | `values_` | `values` |
| `RhsArenaView` | `size_` | `size` |
| `RhsArenaView` | `locations_` | `locations` |
| `RhsClaimSpace` | `rows_` | `rows` |
| `RhsClaimSpace` | `next_free_` | `next_free` |
| `RhsClaimSpace` | `arena_` | `arena` |
| `RhsScatterView` | `objective_gradient_` | `objective_gradient` |
| `RhsScatterView` | `constraint_adjoint_gradient_` | `constraint_adjoint_gradient` |
| `RhsScatterView` | `equality_residuals_` | `equality_residuals` |
| `RhsScatterView` | `inequality_residuals_` | `inequality_residuals` |
| `RhsScatterView` | `objective_` | `objective` |
| `SolverIndexingData` | `input_size_` | `input_size` |
| `SolverIndexingData` | `output_size_` | `output_size` |
| `SolverIndexingData` | `num_funcappl_` | `num_funcappl` |
| `SolverIndexingData` | `vindex_init_` | `vindex_init` |
| `SolverIndexingData` | `cindex_init_` | `cindex_init` |
| `SolverIndexingData` | `unique_constraints_` | `unique_constraints` |
| `SolverIndexingData` | `v_index_` | `v_index` |
| `SolverIndexingData` | `c_index_` | `c_index` |
| `SolverIndexingData` | `v_out_index_` | `v_out_index` |
| `SolverIndexingData` | `v_out_reduced_` | `v_out_reduced` |
| `SolverIndexingData` | `v_index_continuity_` | `v_index_continuity` |
| `SolverIndexingData` | `c_index_continuity_` | `c_index_continuity` |
| `SolverIndexingData` | `inner_constraint_starts_` | `inner_constraint_starts` |
| `SolverIndexingData` | `inner_gradient_starts_` | `inner_gradient_starts` |
| `SolverIndexingData` | `inner_kkt_starts_` | `inner_kkt_starts` |
| `QpOptions` | `primal_delta` | `primal_regularization` |
| `QpOptions` | `dual_mu` | `dual_regularization` |
| `QpOptions` | `feas_tol` | `feasibility_tol` |
| `QpOptions` | `opt_tol` | `optimality_tol` |
| `QpOptions` | `max_iter` | `max_iterations` |
| `QpOptions` | `schur_cap` | `max_schur_border_dimension` |
| `QpOptions` | `schur_cond_max` | `schur_condition_max` |
| `QpOptions` | `ws_algebra` | `working_set_algebra` |
| `QpOptions` | `tr_radius` | `trust_region_radius` |
| `QpSolveOverrides` | `tr_radius` | `trust_region_radius` |
| `QpSolveOverrides` | `primal_delta` | `primal_regularization` |
| `QpSolveOverrides` | `dual_mu` | `dual_regularization` |
| `SqpOptions` | `tr_init` | `trust_region_initial` |
| `SqpOptions` | `tr_max` | `trust_region_max` |
| `SqpOptions` | `tr_min` | `trust_region_min` |
| `SqpOptions` | `budget_mode` | `return_best_on_max_iterations` |
| `SqpOptions` | `enable_scaling` | `row_scaling` |
| `SqpOptions` | `scaling_factor_limit` | `scaling_factor_max` |
| `SqpOptions` | `qn_memory` | `lbfgs_memory` |
| `SqpAdvancedOptions` | `restoration_max_entries` | `max_restoration_entries` |
| `IpmOptions` | `initial_barrier` | `barrier_initial` |
| `IpmOptions` | `max_barrier` | `barrier_max` |
| `IpmOptions` | `min_barrier` | `barrier_min` |
| `IpmOptions` | `best_criterion` | `best_iterate_criterion` |
| `IpmAdvancedOptions` | `acceptance_strategy` | `acceptance_mode` |
| `IpmAdvancedOptions` | `barrier_governor` | `barrier_governor_mode` |
| `IpmAdvancedOptions` | `restoration_max_entries` | `max_restoration_entries` |
| `IpmAdvancedOptions` | `line_search_max_backtracks` | `max_line_search_backtracks` |
| `IpmAdvancedOptions` | `line_search_extended_backtracks` | `max_extended_backtracks` |
| `IpmAdvancedOptions` | `inertia_correction_max_attempts` | `max_inertia_correction_attempts` |
| `NlpRowClassification` | `kinds_` | `kinds` |
| `NlpRowClassification` | `eq_row_` | `eq_row` |
| `NlpRowClassification` | `iq_upper_row_` | `iq_upper_row` |
| `NlpRowClassification` | `iq_lower_row_` | `iq_lower_row` |
| `NlpRowClassification` | `num_eq_` | `num_eq` |
| `NlpRowClassification` | `num_iq_` | `num_iq` |
| `IpmLinearOptions` | `pivoting` | `pivot_strategy` |
| `IpmLinearOptions` | `matching` | `weighted_matching` |
| `IpmLinearOptions` | `scaling` | `matrix_scaling` |
| `IpmLinearOptions` | `pivot_perturbation` | `pivot_perturbation_exponent` |
| `IpmLinearOptions` | `refinement_steps` | `max_refinement_steps` |
| `IpmLinearOptions` | `parallel_solve` | `solve_parallelism` |
| `IpmLinearOptions` | `accelerate_zero_tolerance` | `accelerate_zero_tol` |
| `IpmLinearOptions` | `dense_max_dimension` | `max_dense_dimension` |
| `IpmAdvancedOptions` | `primal_dual_step_strategy` | `step_length_rule` |
| `SqpLinearOptions` | `dense_max_dimension` | `max_dense_dimension` |
| `IpqpOptions` | `hard_iteration_cap` | `hard_max_iterations` |
| `IpqpOptions` | `init_mu` | `barrier_initial` |
| `IpqpOptions` | `min_mu` | `barrier_min` |
| `IpqpOptions` | `rho_init` | `primal_regularization_initial` |
| `IpqpOptions` | `delta_init` | `dual_regularization_initial` |
| `IpqpOptions` | `reg_floor` | `regularization_min` |
| `IpqpOptions` | `reg_max` | `regularization_max` |
| `IpqpOptions` | `reg_decrease` | `regularization_decrease` |
| `IpqpOptions` | `tau` | `fraction_to_boundary` |
| `IpqpOptions` | `face_kappa` | `face_ratio_threshold` |
| `IpqpOptions` | `converge_slack` | `convergence_slack` |
| `IpqpOptions` | `ruiz` | `ruiz_equilibration` |
| `IpqpOptions` | `warm_iter_budget` | `max_warm_iterations` |
| `IpqpOptions` | `mu_adopt_factor` | `barrier_adoption_factor` |
| `IpqpOptions` | `retire_after` | `max_consecutive_escapes` |
| `IpqpOptions` | `hoist_symbolic` | `hoist_symbolic_analysis` |
| `IpqpOptions` | `mu0_rule` | `barrier_initial_rule` |
| `IpqpOptions` | `mu_residual_floor` | `barrier_residual_floor` |
| `SqpAdvancedOptions` | `enable_soc` | `second_order_correction` |
| `SqpAdvancedOptions` | `adaptive_mu` | `adaptive_dual_regularization` |
| `SqpAdvancedOptions` | `ssn_prox_carry` | `ssn_proximal_carry` |
| `SqpAdvancedOptions` | `qp_failure_budget` | `max_consecutive_qp_failures` |
| `SqpAdvancedOptions` | `qp_cap_retry` | `qp_iteration_limit_retry` |
| `SqpAdvancedOptions` | `qn_update_on_reject` | `lbfgs_update_on_reject` |
| `SqpAdvancedOptions` | `qn_nonlinear_only` | `lbfgs_nonlinear_only` |
| `SqpAdvancedOptions` | `qn_full_memory_max` | `max_lbfgs_full_memory_dimension` |
| `linear::SymmetricFactor::Options` | `num_threads` | `threads` |
| `linear::SymmetricFactor::Options` | `pivot_perturb_exp` | `pivot_perturbation_exponent` |
| `linear::SymmetricFactor::Options` | `max_refinement_iters` | `max_refinement_steps` |
| `linear::SymmetricFactor::Options` | `cnr_threads` | `conditional_reproducibility_threads` |
| `linear::SymmetricFactor::Options` | `accelerate_zero_tolerance` | `accelerate_zero_tol` |
| `linear::SymmetricFactor::Options` | `kind` | `decomposition` |
| `linear::InertiaEvidence` | `n_pos` | `positive` |
| `linear::InertiaEvidence` | `n_neg` | `negative` |
| `linear::InertiaEvidence` | `n_zero` | `zero` |
| `linear::SolveInfo` | `refinement_iters` | `refinement_steps` |
| `linear::SymmetricFactor` | `set_num_threads()` | `set_threads()` |
| `linear::DenseSymmetricFactor` | `set_num_threads()` | `set_threads()` |
| `linear::DenseSymmetricFactor` | `num_threads()` | `threads()` |
| `IpmResult` | `factor_mem` | `factor_memory` |
| `IpmResult` | `soc_steps_taken` | `second_order_corrections` |
| `IpmResult` | `last_monotone_iters` | `last_monotone_iterations` |
| `IpmResult` | `last_feas_rest_entries` | `last_feasibility_restoration_entries` |
| `IpmResult` | `last_feas_rest_iters` | `last_feasibility_restoration_iterations` |
| `IpmResult` | `last_prox_reg_primal` | `last_proximal_regularization_primal` |
| `IpmResult` | `last_prox_reg_dual` | `last_proximal_regularization_dual` |
| `IpmTiming` | `total` | `total_seconds` |
| `IpmTiming` | `pre` | `preprocessing_seconds` |
| `IpmTiming` | `func` | `evaluation_seconds` |
| `IpmTiming` | `kkt` | `kkt_seconds` |
| `IpmTiming` | `print` | `print_seconds` |
| `IpmTiming` | `solver_init` | `solver_initialization_seconds` |
| `IpmTiming` | `misc()` | `other_seconds()` |
| `SqpScalingReport` | `obj` | `objective` |
| `SqpScalingReport` | `col_min` | `column_min` |
| `SqpScalingReport` | `col_max` | `column_max` |
| `SqpScalingReport` | `col_spread` | `column_spread` |
| `SqpResult` | `solve_impl_seconds` | `engine_seconds` |
| `SqpResult` | `native_warm_start` | `sqp_warm_start` |
| `SolveBudget` | `minor_budget` | `max_minor_iterations` |
| `Problem` | `partitions()` | `num_partitions()` |
| `SqpSolver` | `ssn_tier_num_threads()` | `ssn_tier_threads()` |
| `SqpSolver` | `ipqp_tier_num_threads()` | `ipqp_tier_threads()` |
| `IpmCrossoverOptions` | `activity_rel_tol` | `activity_relative_tol` |
| `linear::SymmetricFactor` | `num_threads()` | `threads()` |
| (free function) | `serialize(const WarmStart &)` | `serialize_warm_start(const WarmStart &)` |
| (free function) | `deserialize(std::span<const std::byte>)` | `deserialize_warm_start(std::span<const std::byte>)` |
| `IpmLinearOptions` | `accelerate_zero_tol` declared only under `USE_ACCELERATE_SPARSE` | declared on every platform (validated everywhere, ignored on MKL) |
| `InteriorPointSolverPresetFields` (internal) | `acceptance_strategy_` | `acceptance_mode_` |

### Option dump keys (`dump_options`) and validation messages

Every renamed option field above prints and is refused under its new name: the dump key is the field path
(`trust_region_initial`, `qp.feasibility_tol`, `advanced.max_restoration_entries`, `advanced.ipqp.barrier_initial`,
`linear.pivot_strategy`, ...). Enumerator values print as the new enumerator names (`kAugmentedLagrangian`,
`kSolveEquations`, `kSinglePenalty`, `kEqualityMultipliersPrimal`, `kIpqp`, `kAdaptivePartitioning`, ...).
`IpmOptions`' dump has `linear.accelerate_zero_tol` on every platform (68 lines).
