# M8 AP-2 B8: the naming rules and the rename table

Date: 2026-11-05. Base: `m8` at `fa727270` (B7 landed). This is B8's record for the AP-1 page
(`2026-11-m8-ap1-api-review.md` §7 and §13, group B8) and the reference tycho's catch-up reads.
After B8 the public names are frozen until the next break window, except the `advanced` option
tiers (owner decision 6).

## 1. The rules

A reader who knows these rules should be able to predict any public name.

**N — namespaces.** The library is `hven`. The factorization layer is `hven::linear`. Everything in a
`detail` namespace is `hven::detail` (`hven::linear::detail` for the factor layer); test seams are
`hven::detail::testing`. There is no other namespace. (`hven::solvers` and `hven::utils` are gone.)

**H — headers.** One umbrella, `<hven/hven.h>`, includes every public header; the export contract
checks that it does. A header is named for the principal type it declares, in `snake_case`
(`SolverIndexingData` → `solver_indexing_data.h`), or for the family it declares when there are
several (`piece_interfaces.h`: `ConstraintInterface` and `ObjectiveInterface`). The directories are
kept (AP-1 §7). No forwarding headers (C201).

**T — types.** `PascalCase` words. An acronym is written as a word: `Nlp`, `Qp`, `Kkt`, `Ipm`, `Sqp`,
`Ipqp`, `Ssn`, `Io`, `Lbfgs`. A type that belongs to one engine's results or records carries that
engine's prefix (`IpmResult`, `IpmIterate`, `SqpIterate`, `IpmCrossoverOptions`); shared types carry
none. Exceptions, stated: the Eigen-shaped aliases of `core/types.h` keep Eigen's short vocabulary
(`Vec`, `Mat`, `SpMatRM`, `VecRef`, ...); trace-event types keep the spelling of the schema event they
mirror (`IpmIterTraceEvent` for `ipm.iter`, `IpqpTraceRegEvent` for `ipqp.reg`).

**E — enumerators and enum names.** Enumerators are `kPascalCase` words, acronyms as words
(`FactorKind::kLdlt`), and name what they select rather than abbreviate it
(`LineSearchMode::kAugmentedLagrangian`, not `kAugLang`). An enum type is singular. Its suffix says
what kind of choice it is: `Mode` selects how a component runs (which algorithm or behaviour);
`Rule` selects the formula for one quantity; a noun names the thing chosen when the choice IS that
thing (`Engine`, `Criterion`, `Treatment`, `Policy`, `Scaling`, `Curvature`, `Phase`, `Level`).
`Kind` is reserved for enums that classify a reported value (`FactorKind`, `IpmMessageKind`) and is
never an option type. A public option enum has no enumerator its option refuses (hence
`SolvePhaseMode` without `kInit`).

**M — data members.** A data member is spelled `snake_case_` (trailing underscore) if and only if it
is private or protected. A public data member is bare `snake_case`, whatever the type (CLAUDE.md §4:
aggregates, settings, records, events; and the raw piece protocol's public structs). A record that
is serialized into the trace schema names each member exactly as its trace key
(`IpmIterate::prim_obj` is key `prim_obj`): the keys are frozen (schema v0), so those members keep
the keys' spelling.

**O — option fields.** Option fields are `snake_case` words; an abbreviation is expanded
(`trust_region_initial`, not `tr_init`; `working_set_algebra`, not `ws_algebra`), except the
`_tol` suffix and established names (`lbfgs`, the IPOPT-named `scaling_max_gradient`).
 - Tolerances are `<measure>_tol` (`optimality_tol`, `feasibility_tol`, `acceptable_kkt_tol`).
 - A cap on a count is `max_<counted things>` (`max_iterations`, `max_factorizations`,
   `max_restoration_entries`, `max_line_search_backtracks`, `max_schur_border_dimension`).
 - A bound or start value of a parameter is `<parameter>_initial` / `_max` / `_min`
   (`barrier_initial`, `barrier_max`, `trust_region_min`, `scaling_factor_max`,
   `schur_condition_max`), Ipopt's `mu_init`/`mu_max` order.
 - A field holding an option enum is that enum's name in `snake_case`, with an optional context
   prefix (`acceptance_mode: AcceptanceMode`, `optimize_barrier_mode: BarrierMode`,
   `best_iterate_criterion: BestIterateCriterion`).
 - A `bool` is named for the behaviour it turns on, without `enable_` (`row_scaling`,
   `return_best`, `return_best_on_max_iterations`).
 - One concept has one name and one floor on both engines (`max_restoration_entries >= 0` on both;
   0 refuses restoration).

**Scope.** The rules bind the public surface (everything a public header declares). The `advanced`
tiers were swept only where a registered item named them (owner decision 6 lets them change in
any milestone); their remaining abbreviations (`IpqpOptions::rho_init`, `SqpAdvancedOptions::enable_soc`,
`qn_*`, ...) are registered for the milestone that next touches them. Internals under `detail/`
were renamed where the page named them (`NLPRowClassification`, `NLPAdapterCore`) or where a
public rename reached them; the internal types declared in `detail/` headers outside a `detail`
namespace (they were `hven::solvers`, now `hven`) are registered to move into `hven::detail`
(internal, no consumer break). Trace keys, golden lines and frozen evidence CSV headers are not
renamed (CLAUDE.md §1).

## 2. The behaviour-neutral exceptions B8 carries

- `max_restoration_entries` on the SQP accepts 0 (it refused it): the first restoration request
  is then refused by the exit a refused later request takes (`kNumericalError`,
  `SqpStopReason::kRestorationRefused`). Every value accepted before behaves as before.
- `AlgorithmMode::kInit` is no longer a public option value: `IpmAdvancedOptions::solve_phase_mode`
  is a `SolvePhaseMode` of three values, mapped once per phase onto the engine's internal mode.
- Option dumps and validation messages print the new names.
- The version is 0.2.0 (`CHANGELOG.md`).

## 3. The complete old → new table

Generated from the manifests the rename was applied from (`docs/notes/data/2026-11-m8-ap2-b8/manifest/`).
### Namespaces

| old | new |
|---|---|
| `hven::solvers::X` | `hven::X (every name)` |
| `hven::solvers::detail::X` | `hven::detail::X` |
| `hven::solvers::detail::testing::X` | `hven::detail::testing::X` |
| `hven::utils::TypeStorage, hven::utils::TypeStorageBase` | `hven::TypeStorage, hven::TypeStorageBase` |
| `hven::utils::X (internal utilities), hven::utils::detail::X` | `hven::detail::X` |
| `hven::linear, hven::linear::detail` | `unchanged` |

### Headers

| old | new |
|---|---|
| `<hven/model/indexing_data.h>` | `<hven/model/solver_indexing_data.h>` |
| `<hven/model/sizing_specs.h>` | `<hven/model/sizable_spec.h>` |
| `<hven/model/solver_interface_specs.h>` | `<hven/model/piece_interfaces.h>` |
| `<hven/solver_interface_adapter.h>` | `<hven/model/solver_interface_adapter.h>` |
| `<hven/drivers/ipm_iterate_info.h>` | `<hven/drivers/ipm_iterate.h>` |
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
| `AcceptanceKind` | `AcceptanceMode` |
| `BarrierGovernorKind` | `BarrierGovernorMode` |
| `AlgorithmMode` (public, `IpmAdvancedOptions::solve_phase_mode`) | `SolvePhaseMode`; `AlgorithmMode` is internal (`detail/drivers/ipm_solver_impl.h`) |

### Enumerators

| enum | old | new |
|---|---|---|
| `SolvePhaseMode` (was `AlgorithmMode`) | `kOpt`, `kOptNo`, `kSoe` | `kOptimize`, `kOptimizeWithoutObjective`, `kSolveEquations` |
| `SolvePhaseMode` (was `AlgorithmMode`) | `kInit` | removed from the public enum (it is never a phase mode) |
| `LineSearchMode` | `kAugLang` | `kAugmentedLagrangian` |
| `LineSearchMode` | `kLang` | `kLagrangian` |
| `LineSearchMode` | `kNoLs` | `kNone` |
| `PrimalDualStepStrategy` | `kPrimSlackEqSplitIq` | `kEqualityMultipliersPrimal` |
| `PrimalDualStepStrategy` | `kPrimSlackSplitEqIq` | `kEqualityMultipliersDual` |
| `PrimalDualStepStrategy` | `kMaxEq` | `kEqualityMultipliersMaximum` |
| `MeritPenaltyRule` | `kWmno` | `kSinglePenalty` |
| `linear::FactorKind` | `kLDLT` | `kLdlt` |
| `linear::FactorKind` | `kLLT` | `kLlt` |
| `linear::FactorKind` | `kLU` | `kLu` |
| `TypeStorage::Kind` | `Empty` | `kEmpty` |
| `TypeStorage::Kind` | `Inline` | `kInline` |
| `TypeStorage::Kind` | `Heap` | `kHeap` |
| `NlpAdapterCore::HessOwner (internal)` | `Objective` | `kObjective` |
| `NlpAdapterCore::HessOwner (internal)` | `EqPiece` | `kEqualityPiece` |
| `NlpAdapterCore::HessOwner (internal)` | `IqPiece` | `kInequalityPiece` |
| `NlpRowKind (internal)` | `Equality` | `kEquality` |
| `NlpRowKind (internal)` | `UpperBounded` | `kUpperBounded` |
| `NlpRowKind (internal)` | `LowerBounded` | `kLowerBounded` |
| `NlpRowKind (internal)` | `Range` | `kRange` |
| `NlpRowKind (internal)` | `Free` | `kFree` |
| `ParsedIoFlags (was ParsedIOFlags)` | `HiddenInput` | `kHiddenInput` |
| `ParsedIoFlags (was ParsedIOFlags)` | `IngoreOutput` | `kIgnoreOutput` |
| `ParsedIoFlags (was ParsedIOFlags)` | `NotContiguous` | `kNotContiguous` |
| `ParsedIoFlags (was ParsedIOFlags)` | `Contiguous` | `kContiguous` |
| `ThreadingFlags` | `RoundRobin` | `kRoundRobin` |
| `ThreadingFlags` | `MainThread` | `kMainThread` |
| `ThreadingFlags` | `NeedsPool` | `kNeedsPool` |
| `ThreadingFlags` | `ByApplication` | `kByApplication` |
| `ThreadingFlags` | `ThreadN` | `kThreadN` |

### Members and option fields

| struct | old | new |
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
| `SolveOverrides` | `tr_radius` | `trust_region_radius` |
| `SolveOverrides` | `primal_delta` | `primal_regularization` |
| `SolveOverrides` | `dual_mu` | `dual_regularization` |
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

### Option dump keys (`dump_options`) and validation messages

Every renamed option field above prints and is refused under its new name: the dump key is the
field path (`trust_region_initial`, `qp.feasibility_tol`, `advanced.max_restoration_entries`, ...).
Enumerator values print as the new enumerator names (`kAugmentedLagrangian`, `kSolveEquations`,
`kSinglePenalty`, `kEqualityMultipliersPrimal`, ...).
