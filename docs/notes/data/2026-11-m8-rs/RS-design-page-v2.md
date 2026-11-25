# RS: the code-organisation restructure of hven (design page v2)

**Basis.** Read-only on origin/m8 @ `a4cf8feec`, using a throwaway worktree that has since been removed; no shared tree was modified. The include graph, the move map and the layer check are all script-generated. 680 tracked files under include/src/tests are covered, every file is placed, and no two files land on the same path. The full map is in `RS-move-map-v2.txt`, next to this page.

## Change log against v1

| item | change |
|---|---|
| M1 | **One layer order for everything**: public headers, detail headers and src TUs. The order is core < util < barrier < linear < model < qp-data < kkt < qp-engines < warmstart < solver < {ipm, sqp} public < trace < engine implementations. "TUs may include any public header" is gone. The allow-list is file-to-file pairs only: 12 pairs, all measured today (§2.3). non_linear_program.h → barrier is legal by order. |
| M2 | **detail/qp is split** into the QP data layer (qp_problem, working_set) plus `walk/`, `ssn/` and `ipqp/`, mirrored in src/qp. Applying M5 rule 1 also moves `bordered_eqp.h` and `eqp_solve.h` into `walk/`, because the walk is their only library includer. That extends M2's list (argued in §2.2). v1's eqp_solve→kkt move is withdrawn. |
| M3 | **RS-6a is mandatory**: one `hven_<component>_tests` executable per tests/ directory. Seam and allocation executables are handled in §5.4. RS-6b (lifting the UNIX-only gate) is a declared new-evidence step. |
| M4 | Added to the change list: hvenConfig.cmake.in:97 (sentinel), .gitattributes:7, the core-layering scan of detail/core with a file count, the PCH gate on RS-1a/1b/1c, and RS-0's static-initialiser check with symbol-map preparation. |
| M5 | Placement rules rewritten as three rules; every placement re-checked against them (§2.2, §3). |
| S1 | inertia_regularization.h → detail/kkt/. barrier/ = {barrier_math, bound_set}, below model. |
| S2 | kkt_calls.h/.cpp → kkt_factor.h/.cpp (file names only). The two factor families are stated, and their unification registered. |
| S3 | finite_screen.h → detail/core/. |
| S4 | Walk files renamed inside RS (walk_engine, anti_cycling_arm, degeneracy_arm, walk_trace, certificate_seam), using git mv plus include rewrites only. Symbol renames come later. |
| S5 | The 5 util headers with zero includers and fmtlib.{h,cpp} are deleted in RS-1a, **CONDITIONAL on owner confirmation**. New finding added to the same question: color_text.{h,cpp} looks dead too. memory_management is a separate decision. |
| S6 | Names: sqp `globalization.h` → `funnel.h`; `test_types.cpp` → `test_qp_types.cpp`; `test_scaffold.cpp` → `tests/linear/test_backend_scaffold.cpp`. |
| S7 | Loose fixtures go into support/. New tests/bench/ directory. test_dense_oracle sits beside its fixture. The two stop/backend seam tests go to solver/. Chosen: a configure-time check that no basename appears in two support/ directories. |
| S8 | RS-1a..1e are five commits, each verified individually, landed in one push. |
| S9 | Policy for string literals added. Origin names are swept from comments. |
| S10 | Exception 4 (nlp_model.h → qp_types.h) is removed in RS-4. Exceptions 2, 3 and core/solver_counters.h are recorded as debt, each with a proposed owner. |
| also | §2 notes the src/sqp/globalization vs src/ipm asymmetry, and the warmstart line points at continuation's new home. bench/ keeps its layout (only forced edits) and is registered. |

**Counts (v2).** 357 map entries:
- 328 pure moves;
- 15 renames;
- 14 deletions: 7 unconditional (.gitkeep files) and 7 conditional (S5).

If the owner declines S5, the 7 conditional deletions become moves, giving 335 / 15 / 7. There are 0 splits and 0 merges. Only 16 public headers change path, and only in their directory part.

**Supersession.** This reverses AP-1 §7's ruling "KEEP the directories" (`docs/notes/2026-11-m8-ap1-api-review.md:487`). The owner should record that.

---

## 1. Inventory: the layout as it actually is

### include/hven (55 public headers, 128 detail headers)

| dir | files | what is there |
|---|---|---|
| `hven.h` | 1 | the umbrella |
| `core/` | 11 | types, errors, solver_status, solver_counters (2559 lines: QP, IPM and SQP counters), start_level, stop, ledger, pattern_hash, type_storage, compiler, version |
| `core/detail/` | 3 | aggregate_arity, inf_norm, stop_poll. **The only `<component>/detail/` directory** |
| `linear/` | 3 | symmetric_factor, dense_symmetric_factor, dense_policy |
| `model/` | 18 + .gitkeep | the problem contract |
| `qp/` | 2 + .gitkeep | qp_types, ipqp_evidence |
| `warmstart/` | 4 + .gitkeep | warm_start (the currency), seeding, sqp_warm_start, ipm_polish_extension |
| `drivers/` | 16 + .gitkeep | **both engines' public API, the shared surface and the trace sinks** |
| `globalization/`, `interior/`, `kkt/` | .gitkeep only | **empty public placeholder directories** |
| `detail/drivers/` | 20 | 5 IPM, 8 SQP and 7 shared/plumbing headers |
| `detail/globalization/` | 23 | **used only by the IPM.** One exception: inertia_regularization, which ipqp_engine.cpp also uses. Three are barrier-parameter governors, not globalization |
| `detail/globalization/sqp/` | 9 | SQP globalization **plus 4 quasi-Newton Hessian files** |
| `detail/interior/` | 7 (+typedefs 2, +utils 11) | IPM-only, IPM+IPQP-shared and model-side files mixed together; typedefs and utils are generic. 5 utils headers have zero includers in hven |
| `detail/kkt/` | 6 | border_ops, bordered_eqp, kkt_assembly, kkt_calls, schur_complement, dense_selection |
| `detail/linear/` | 8 | backend sessions. Named in `notices/eigen-mpl2.txt` and CLAUDE.md §6 |
| `detail/model/` | 11 | the engine problem forms and bridges |
| `detail/qp/` | 14 | three QP engines and the QP data, flat; the walk's files are spelled `qp_*` |
| `detail/warmstart/` | 7 | crossover ingest (**named `warm_start.h`, the same basename as the public currency header**), sqp_warm_state, mesh_transfer, access doors, **continuation and predictor (which include sqp_solver_impl.h)** |

### src (58 compiled TUs)
- `src/drivers/` (22): both engine loops (sqp_solver.cpp 10.5k lines, ipm_solver.cpp 7.7k) plus shared code. `solve_status.cpp` implements `ipm_phase_status.h`.
- `src/globalization/sqp/` (6).
- `src/interior/` (1) and `src/interior/utils/` (7; fmtlib.cpp is not compiled; color_text.cpp is compiled but has no caller, see S5).
- `src/warmstart/warm_start.cpp` is the crossover.
- Unchanged: `src/core` (4), `src/linear` (6), `src/model` (8).
- `src/qp` (5) and `src/kkt` (1) are reorganised (§3).

### Misplacements and inconsistencies
1. The IPM is spread over 6 directories and the SQP over 5.
2. `globalization/` has no shared content. The O8 ruling only dodged 4 file-name collisions, and it made the IPM the default occupant of a layer that was supposed to be shared.
3. Quasi-Newton code is filed under globalization.
4. `interior/` holds generic utilities and spells "interior" where the classes say `Ipm`.
5. `drivers/` holds the solvers, the shared surface and trace.
6. There are two conventions for internals, and empty public placeholder directories.
7. Two files are both called `warm_start.h`.
8. Both `<hven/...>` and `"hven/...` spellings are in use (include/ 316/216, src/ 78/200, tests/ 568/447).
9. Tests are laid out by origin project. Fixtures live in four places. Test executables follow CMake history. `hven_sqp_tests` is UNIX-only.
10. The QP walk engine's files are named `qp_*`, while its peers are named after their modes (`ssn_engine`, `ipqp_engine`).
11. CLAUDE.md §2 is stale, and so is §1's "Neither engine is in this repository yet".
12. 17 detail headers are included only by src TUs, but are installed. Registered, not part of RS.

## 2. Target layout

### 2.1 Organisation: by engine, with shared layers
Grant's question is "where is the IPM, the SQP, a QP engine, the linear algebra?". A purely layered tree answers it badly here, because the measured "layers" are mostly single-engine. hven therefore has **five engines**: `ipm` and `sqp` (the NLP solvers) and `walk`, `ssn` and `ipqp` (the QP engines, grouped under `qp/`). They sit on shared layers. Precedent: HiGHS (MIT) keeps `src/{ipm,simplex,qpsolver,mip}` beside `lp_data/` and `util/`. Component X lives in `include/hven/X/` (public), `include/hven/detail/X/` (internal), `src/X/` (TUs) and `tests/X/` (tests).

### 2.2 Placement rules (M5)
1. **A file lives with the component that owns its concept.** Engine-private code lives in the engine. A shared layer exists when at least 2 of the five engines (ipm, sqp, walk, ssn, ipqp) include it, or when the file is a named bridge. Generic infrastructure (core, util, linear) is owned by concept, not by count. The named bridges:
   - **model**: the IPM's problem form (`non_linear_program.h`, `make_nlp_program.h`, `nlp_adapter.h`); the SQP's problem form (`nlp_model.h`, `nlp_problem_model.h`, `nlp_model_assembly.h`); the engines' door into `Problem` (`problem_access.h`).
   - **warmstart**: the hand-off types: the `WarmStart` currency, `SqpWarmStart`/`sqp_warm_state.h`, the IPM polish extension, the IPM→SQP crossover (`crossover.h`), mesh transfer, and the two access doors.
   - **solver**: the surface both solvers present (options, budget, result, iteration record) and the plumbing behind it.
   - **trace**: the sink interface every engine emits into.
2. **A fixture lives with the component whose objects it builds, and may be used by tests of higher layers.** If it needs a higher layer's headers, it lives at that higher layer. A test whose vehicle already reaches a layer may use that layer's fixtures. `tests/support/` holds only fixtures owned by no component.
3. **A test lives with the component whose contract it pins, whatever vehicle drives it.** Tests in `tests/X/` mirror at component level only.
- **Placing TUs.** A TU goes in the deepest src directory that contains every header it implements.
  - The IPM's globalization bodies live in `src/ipm/ipm_solver_globalization.cpp`, which implements headers in both `detail/ipm/globalization/` and `detail/ipm/`, so it sits at `src/ipm/`.
  - The SQP has one TU per globalization concern, so it gets `src/sqp/globalization/`.
  - That asymmetry is the rule working, not drift.
- **Public vs detail.** Public headers are reachable from `hven.h` and never include `detail/` (already enforced by `check_export_contract.sh`). `detail/` is installed with no compatibility promise. Keep `detail/<component>/` (12 directories against 1), which also leaves `detail/linear/` (named in notices and §6) untouched.

**Re-check of the contested placements:**
- **finite_screen** → `detail/core`. It is a generic numeric predicate, like inf_norm. Its banner says both engines run it, but today only the IPM includes it.
- **bound_set** → `barrier`. Its consumers are the IPM, IPQP (via barrier_math) and model's NonLinearProgram, so barrier sits below model in the order.
- **mesh_transfer** → `warmstart`. It is a bridge (maps SqpWarmState). It has no library includer today; tests are its only consumers.
- **bordered_eqp, eqp_solve** → `qp/walk`. Their only library includers are walk files (walk_engine.h, ws_independence.{h,cpp}), so by rule 1 they are walk-private. kkt keeps only what at least two engines use:
  - kkt_assembly, border_ops and schur_complement: walk and sqp's predictor;
  - kkt_factor: walk, ssn and sqp;
  - dense_selection, kkt_factorization and inertia_regularization: ipm and ipqp.
- **continuation, predictor** → `sqp`. They drive SqpSolverImpl.
- **accelerate_threads, solver_init** → `ipm`. Only the IPM includes them.
- The `detail/solver` entries are each included by both engines.

### 2.3 Layering (M1): one order for every file

```
core < util < barrier < linear < model < qp-data < kkt < qp-engines < warmstart
     < solver < {ipm, sqp} (public types) < trace < engine implementations < hven.h
```

- qp-data is `include/hven/qp/*.h` plus `detail/qp/*.h` (qp_problem, working_set).
- qp-engines is `detail/qp/{walk,ssn,ipqp}/` plus `src/qp/**`.
- Engine implementations are `detail/ipm/**`, `detail/sqp/**`, `src/ipm/**` and `src/sqp/**`.
- Every other src/X and detail/X sits at X's level.

**Trace is structural.** Its events carry the engines' public types, and the engine implementations emit into it. ipm and sqp are siblings: neither may include the other. A file may include files of its own level or below.

**Allow-list, file-to-file only.** These are the complete set of today's violations under the v2 map, as measured. The RS-5 list is these 12 pairs:

| # | file → included | why / debt owner |
|---|---|---|
| 1 | detail/model/fixed_variable_treatment.h → ipm/ipm_enums.h | the fixed-variable enum is an IpmOptions field. **Debt**, proposed owner: model-contract unification (T8 §6 group) |
| 2 | detail/qp/ipqp/ipqp_engine.h → sqp/sqp_solver_types.h | IpqpOptions/SsnOptions/QpMode are declared in SqpOptions. **Debt**, proposed owner: AP-4 options registry (move them to qp/) |
| 3 | detail/qp/ssn/ssn_engine.h → sqp/sqp_solver_types.h | as 2 |
| 4 | src/model/problem.cpp → solver/solve_result.h | Problem's warm hand-off from a result |
| 5 | src/model/problem.cpp → detail/solver/result_access.h | as 4 |
| 6 | src/qp/ipqp/ipqp_engine.cpp → trace/trace.h | the IPQP engine emits its own events |
| 7-8 | src/solver/counters_dump.cpp → ipm/ipm_solver_types.h, sqp/sqp_solver_types.h | one TU prints both engines' counters. Per-engine split registered (RS-7) |
| 9-11 | src/solver/result_message.cpp → ipm/ipm_options.h, sqp/sqp_options.h, sqp/sqp_solver_types.h | as 7 |
| 12 | src/trace/trace_writer.cpp → detail/sqp/globalization/funnel.h | the serializer names the funnel's verdict types |

- The measured 13th pair, detail/model/nlp_model.h → qp/qp_types.h, is **removed in RS-4**. The header needs only core/types.h. Anything that relied on getting qp_types.h through it gains an explicit include, proven by P-SYM.
- **Further debt**: core/solver_counters.h holds engine-specific counter structs at the bottom of the order. Proposed owner: the consistent-solver-interface track.

### 2.4 Tree (file counts after RS, with S5 applied)
```
include/hven/
  hven.h                      umbrella: every public header
  core/ (11)                  shared vocabulary: types, errors, statuses, counters, stop, ledger, hash, version
  linear/ (3)                 sparse + dense symmetric factorizations (Pardiso / Accelerate / LAPACK)
  model/ (18)                 the problem contract: Nlp, Problem, pieces and assembly
  qp/ (2)                     QP subproblem types, IPQP evidence blocks
  warmstart/ (4)              warm-start currency, seeding, SQP native warm start, IPM polish extension
  solver/ (4)                 what both solvers share: CommonOptions/SolveBudget, SolveResult, IterationRecord, StepVerdict
  ipm/ (6)                    the interior-point solver: IpmSolver, IpmOptions, enums, iterate, phase report, result types
  sqp/ (3)                    the SQP solver: SqpSolver, SqpOptions, result types
  trace/ (3)                  trace-sink interface, console sink, JSON-lines sink
  detail/
    core/ (6)                 inf_norm, finite_screen, stop_poll, aggregate_arity, eigen_types, super_scalar_traits
    util/ (5)                 thread pool, timer, bump allocator, core count, tuple iterator
    barrier/ (2)              barrier-function kernels and bound sets (IPM + IPQP) -- kernels, not the mu strategy (see ipm/)
    linear/ (8)               backend sessions (unchanged)
    model/ (12)               engine-facing problem forms and bridges (+ fixed_variable_row)
    qp/ (2)                   QP data: qp_problem, working_set
      walk/ (8)               the active-set walk: walk_engine, anti_cycling_arm, degeneracy_arm, walk_trace,
                              certificate_seam, ws_independence, bordered_eqp, eqp_solve
      ssn/ (1)                the semismooth-Newton engine
      ipqp/ (4)               the interior-point QP engine: engine, KKT layout, math, fault injection
    kkt/ (7)                  KKT systems shared by engines: kkt_factor (QP-side KktFactor), kkt_factorization
                              (IPM/IPQP KktFactorization), kkt_assembly, schur_complement, border_ops,
                              dense_selection, inertia_regularization
    warmstart/ (5)            crossover ingest, SQP warm state, mesh transfer, access doors
                              (continuation lives in detail/sqp/)
    solver/ (6)               plumbing both engines share: callback rows, diagnostics, result access/message,
                              return-best, option validation
    ipm/ (13)                 IpmSolverImpl, context, barrier-parameter (mu) governors, phase status, presets,
                              KKT vector, runtime init, Accelerate threads, test seam
      globalization/ (18)     acceptance (filter/funnel/merit/switching), line search, SOC, watchdog,
                              restoration, recovery chain
    sqp/ (10)                 SqpSolverImpl, major row, eval seam, scaling, equality basis, continuation,
                              predictor, test seams
      globalization/ (5)      funnel (FunnelStrategy + the GlobalizationStrategy interface), trust region,
                              SOC, elastic tier, restoration
      quasi_newton/ (4)       damped L-BFGS, QN models, mixed lift, QN-lift bridge
src/
  hven_pch.h
  core/ (4)   util/ (5; 3 if color_text goes)   linear/ (6)   model/ (8)   kkt/ (2: kkt_factor, kkt_factorization)
  qp/walk/ (2)  qp/ssn/ (1)  qp/ipqp/ (2)   warmstart/ (3)   solver/ (5)   trace/ (4 incl. private trace_names.h)
  ipm/ (6)    sqp/ (8)  sqp/globalization/ (2)  sqp/quasi_newton/ (4)
tests/
  support/ (4)            console_capture, mkl_dispatch_probe/_signature, route_reference
  core/ (7)  linear/ (10)  model/ (20 + support/ 12 + compile_fail/ 22)  kkt/ (8)
  qp/ (45 + support/ 8 + fixtures/)  warmstart/ (7)  solver/ (20 + support/ 1)  trace/ (8 + support/ 1 + golden/)
  ipm/ (25 + support/ 4)  sqp/ (30 + support/ 4 + cmake/)  bench/ (5)
  golden_rig/ (unchanged)  install_smoke/ (unchanged files, includes updated)
```

**The two factor families (S2).**
- `KktFactor` (detail/kkt/kkt_factor.h) is the QP-side wrapper over `linear::SymmetricFactor`, with dense selection and a pattern cache. The walk, SSN and the SQP predictor use it.
- `KktFactorization` (detail/kkt/kkt_factorization.h) is the IPM/IPQP assembly buffer plus factor.

Their unification is registered as RS-7. Namespaces are untouched.

## 3. Move map

The full list is in `RS-move-map-v2.txt`: one `old -> new` per line, deletions written as `-> DELETE` or `-> DELETE (conditional …)`. Here it is grouped by commit.

**RS-1a, foundations**
- include/hven/core/detail/{aggregate_arity,inf_norm,stop_poll}.h -> include/hven/detail/core/
- include/hven/detail/interior/typedefs/{eigen_types,super_scalar_traits}.h -> include/hven/detail/core/
- include/hven/detail/drivers/finite_screen.h -> include/hven/detail/core/
- include/hven/detail/interior/utils/{get_core_count,memory_management,thread_pool,timer,tuple_iterator}.h -> include/hven/detail/util/
- include/hven/detail/interior/utils/accelerate_threads.h -> include/hven/detail/ipm/
- src/interior/utils/{color_text.cpp,color_text.h,get_core_count.cpp,memory_management.cpp,thread_pool.cpp} -> src/util/ (the PRIVATE include directory moves too; see Q2 for get_core_count.cpp)
- **CONDITIONAL (S5, owner asked):** DELETE include/hven/detail/interior/utils/{flat_map,function_return_type,math_functions,sizing_helpers,std_extensions}.h and src/interior/utils/fmtlib.{h,cpp}.
  - Deleting fmtlib.h needs one edit: drop `#include "fmtlib.h"` from color_text.h.
  - If declined, the 5 headers go to detail/util/ and fmtlib.{h,cpp} to src/util/.
  - Same owner question, new: `hven::detail::enable_color_console` (color_text.{h,cpp}) has no caller. Deleting it removes a compiled TU (`_hven_expected_source_count` 58→57) and the PRIVATE include directory. Proof: all executables link, and P-SYM shows only the removed symbols. One check first: fmt is consumed header-only, so color_text.o is not the TU that supplies fmt's out-of-line definitions.

**RS-1b, shared numerics and the qp split**
- include/hven/detail/interior/{barrier_math,bound_set}.h -> include/hven/detail/barrier/
- include/hven/detail/globalization/inertia_regularization.h -> include/hven/detail/kkt/
- include/hven/detail/interior/kkt_factorization.h -> include/hven/detail/kkt/ ; src/interior/kkt_factorization.cpp -> src/kkt/
- RENAME include/hven/detail/kkt/kkt_calls.h -> include/hven/detail/kkt/kkt_factor.h ; RENAME src/kkt/kkt_calls.cpp -> src/kkt/kkt_factor.cpp
- include/hven/detail/kkt/bordered_eqp.h -> include/hven/detail/qp/walk/
- include/hven/detail/qp/{eqp_solve,ws_independence}.h -> include/hven/detail/qp/walk/
- RENAME include/hven/detail/qp/qp_engine.h -> .../qp/walk/walk_engine.h ; qp_anti_cycling_arm.h -> walk/anti_cycling_arm.h ; qp_degeneracy_arm.h -> walk/degeneracy_arm.h ; qp_walk_trace.h -> walk/walk_trace.h ; qp_certificate_seam.h -> walk/certificate_seam.h
- include/hven/detail/qp/ssn_engine.h -> include/hven/detail/qp/ssn/
- include/hven/detail/qp/{ipqp_engine,ipqp_fault_injection,ipqp_kkt_layout,ipqp_math}.h -> include/hven/detail/qp/ipqp/
- RENAME src/qp/qp_engine.cpp -> src/qp/walk/walk_engine.cpp ; src/qp/ws_independence.cpp -> src/qp/walk/ ; src/qp/ssn_engine.cpp -> src/qp/ssn/ ; src/qp/{ipqp_engine,ipqp_kkt_layout}.cpp -> src/qp/ipqp/
- include/hven/detail/interior/fixed_variable_row.h -> include/hven/detail/model/

**RS-1c, interior-point engine**
- include/hven/drivers/{ipm_enums,ipm_iterate,ipm_options,ipm_phase_report,ipm_solver,ipm_solver_types}.h -> include/hven/ipm/
- include/hven/detail/drivers/{ipm_dense_testing,ipm_phase_status,ipm_solver_impl,ipm_solver_presets,solver_init}.h -> include/hven/detail/ipm/
- include/hven/detail/globalization/{barrier_governor,classic_adaptive_governor,monitored_governor,solver_context}.h -> include/hven/detail/ipm/
- include/hven/detail/globalization/{acceptance_mode,backtracking_line_search,feasibility_stall,feasibility_switch_recovery,filter_acceptance,funnel_acceptance,globalization_mechanism,l1_restoration,merit_acceptance,modern_merit,noop_recovery,progress_measures,proximal_restoration,recovery_chain,restoration,soc,switching_acceptance,watchdog}.h -> include/hven/detail/ipm/globalization/
- include/hven/detail/interior/{aggregate_views,eval_error_log,kkt_vector}.h -> include/hven/detail/ipm/
- src/drivers/{ipm_options,ipm_solver,ipm_solver_globalization,ipm_solver_handle,solver_init}.cpp -> src/ipm/ ; RENAME src/drivers/solve_status.cpp -> src/ipm/ipm_phase_status.cpp
- cmake/hvenConfig.cmake.in:97: re-point the sentinel from `hven/drivers/ipm_solver.h` to `hven/hven.h` (M4). It must be in this commit, because this is the commit that moves the old sentinel.

**RS-1d, SQP engine**
- include/hven/drivers/{sqp_options,sqp_solver,sqp_solver_types}.h -> include/hven/sqp/
- include/hven/detail/drivers/{assembly_eval_seam,column_scaling,equality_basis,equality_basis_testing,ladder_testing,problem_scaling,sqp_major_row,sqp_solver_impl}.h -> include/hven/detail/sqp/
- RENAME include/hven/detail/globalization/sqp/globalization.h -> include/hven/detail/sqp/globalization/funnel.h (S6). Rename the header, not funnel.cpp: FunnelStrategy is its only implementation, the TU is already funnel.cpp, and `globalization/globalization.h` stutters. The GlobalizationStrategy interface stays declared in it.
- include/hven/detail/globalization/sqp/{elastic,restoration,soc,trust_region}.h -> include/hven/detail/sqp/globalization/
- include/hven/detail/globalization/sqp/{lbfgs_damped,mixed_lift,qn_lift_bridge,qn_models}.h -> include/hven/detail/sqp/quasi_newton/
- include/hven/detail/warmstart/{continuation,predictor}.h -> include/hven/detail/sqp/ ; src/warmstart/continuation.cpp -> src/sqp/
- src/drivers/{assembly_eval_seam,equality_basis,problem_scaling,sqp_options,sqp_print,sqp_solver,sqp_solver_handle}.cpp -> src/sqp/
- src/globalization/sqp/{funnel,soc_elastic_restoration}.cpp -> src/sqp/globalization/ ; src/globalization/sqp/{lbfgs_damped,mixed_lift,qn_lift_bridge,qn_models}.cpp -> src/sqp/quasi_newton/

**RS-1e, shared surface, trace, clean-up**
- include/hven/drivers/{common_options,iteration_record,solve_result,step_verdict}.h -> include/hven/solver/
- include/hven/drivers/{console_trace_sink,json_lines_trace_sink,trace}.h -> include/hven/trace/
- include/hven/detail/drivers/{callback_rows,common_options_validate,declared_diagnostics,result_access,result_message,return_best}.h -> include/hven/detail/solver/
- src/drivers/{callback_rows,common_options,counters_dump,result_message,solve_result}.cpp -> src/solver/ ; src/drivers/{console_trace_sink.cpp,trace.cpp,trace_names.h,trace_writer.cpp} -> src/trace/
- RENAME include/hven/detail/warmstart/warm_start.h -> include/hven/detail/warmstart/crossover.h ; RENAME src/warmstart/warm_start.cpp -> src/warmstart/crossover.cpp
- DELETE include/hven/{drivers,globalization,interior,kkt,model,qp,warmstart}/.gitkeep

**RS-2, tests (executable membership unchanged; see RS-6a)**

Fixtures:
- tests/common_support/{console_capture.h,mkl_dispatch_probe.cpp,mkl_dispatch_signature.h,route_reference.h} -> tests/support/
- tests/common_support/ipm_row_view.h -> tests/ipm/support/
- tests/common_support/iteration_record_eq.h -> tests/solver/support/
- tests/drivers/support/hs071_problem.h -> tests/model/support/
- tests/drivers/support/trace_battery.h -> tests/trace/support/
- tests/sqp/support/{claim_stream_double,derivative_check,elastic_rung_vehicle,hs_problems,hs_sweeps,parametric_families,scale_problems}.h -> tests/model/support/
- tests/sqp/support/{border_test_utils,dense_oracle,disproved_entry_vehicle,indefinite_fixtures,ipqp_test_support,qp_as_nlp_model,ssn_fixtures}.h -> tests/qp/support/
- Staying in tests/sqp/support/: e1_cells, e1b_off_golden, e1b_off_golden_data, nlp_kkt_check. Each needs SQP headers.
- tests/interior/adapter_fixture_functions.h -> tests/model/support/
- tests/interior/{declared_route,progress_measures_test_utils,solver_test_utils}.h -> tests/ipm/support/

Tests:
- tests/drivers/{CMakeLists.txt,test_best_iterate,test_callback,test_concurrency,test_counters_dump,test_dense_arm_engines,test_dense_auto,test_evaluation_failure,test_exit_door_walk,test_infinite_bound,test_iteration_history,test_limits,test_message_sites,test_options,test_options_dump,test_solve_result,test_solver_move,test_stop_token,test_threads}(.cpp) -> tests/solver/
- tests/sqp/{test_backend_error_seams,test_stop_poll_seams}.cpp -> tests/solver/
- tests/drivers/{test_console_sink,test_human_trace_goldens,test_print_tables,test_trace_messages,test_trace_off_path,test_trace_v1_contract,test_trace_v1_fixtures}.cpp -> tests/trace/ ; tests/sqp/test_trace_writer.cpp -> tests/trace/
- tests/drivers/golden/{human_trace/*.txt (35), trace_v1/{ipm_aborted,ipm_complete,sqp_complete}.jsonl} -> tests/trace/golden/ (same subdirectories). Update `.gitattributes:7` to `tests/trace/golden/trace_v1/*.jsonl text eol=lf` in the same commit (M4). The bytes are unchanged, so hven-bench's sha256 copies still match.
- tests/drivers/{test_ipm_counters,test_stop_recovery}.cpp -> tests/ipm/ ; RENAME tests/drivers/test_solve_status.cpp -> tests/ipm/test_ipm_phase_status.cpp ; tests/sqp/test_ipm_dense_guard_seams.cpp -> tests/ipm/
- tests/interior/{CMakeLists.txt + the 21 IPM tests: test_acceptance_diagnostics, test_acceptance_retraction, test_classic_merit_restoration, test_filter_acceptance, test_funnel_acceptance, test_ipm_solver_entry, test_ipm_solver_presets, test_ipm_stop_reason, test_ipm_trace, test_ipm_warm_start, test_l1_restoration, test_merit_rules, test_monitored_governor, test_nlp_multiplier_seeding, test_objective_scale_reporting, test_proximal_restoration, test_recovery_dispatch_gate, test_soc, test_structure_epoch_gating, test_switching_acceptance, test_watchdog} -> tests/ipm/
- tests/interior/{test_assembly_allocation,test_nlp_adapter,test_nlp_assembly_contract,test_nlp_problem_model,test_solver_interface_adapter,test_string_piece}.cpp -> tests/model/ ; tests/interior/compile_fail/* (22 files) -> tests/model/compile_fail/ ; tests/drivers/test_problem.cpp -> tests/model/ ; tests/sqp/{test_nlp_model,test_parametric_families,test_scale_problems}.cpp -> tests/model/ (the last two pin model/support fixtures, so they sit beside them)
- tests/interior/{test_kkt_factorization,test_kkt_factorization_faults}.cpp -> tests/kkt/ ; tests/sqp/{test_border_ops,test_kkt_assembly,test_kkt_calls,test_kkt_inertia_probe,test_kkt_partial_solve,test_schur}.cpp -> tests/kkt/
- tests/sqp/{test_dense_oracle,test_dense_solve_wound_seams,test_eqp_refine_ab,test_eqp_solve,test_ipqp_acceptance,test_ipqp_certification,test_ipqp_counters,test_ipqp_engine,test_ipqp_kkt_layout,test_ipqp_ladder_skip,test_ipqp_math,test_ipqp_mu0_rule,test_ipqp_options,test_ipqp_seams,test_ipqp_trace,test_ipqp_warm_restart,test_qp_engine,test_qp_engine_border,test_qp_engine_indefinite,test_qp_engine_tr,test_qp_row_contract,test_qp_warm_start,test_qp_ws_independence,test_ssn_engine}.cpp -> tests/qp/ ; RENAME tests/sqp/test_types.cpp -> tests/qp/test_qp_types.cpp
- tests/sqp/{test_mesh_transfer,test_sqp_warm_currency,test_warm_start,test_warm_start_battery}.cpp -> tests/warmstart/ ; tests/drivers/test_warm_protocol.cpp -> tests/warmstart/
- tests/sqp/{test_bench_dump,test_corpus_cells,test_crossover_legs,test_snopt_bridge_gate}.cpp -> tests/bench/ ; tests/model/test_model_surface_kkt.cpp -> tests/bench/. These are the tests that pin a bench/*.h instrument.
- tests/sqp/test_accelerate_probes.cpp -> tests/linear/ ; RENAME tests/sqp/test_scaffold.cpp -> tests/linear/test_backend_scaffold.cpp ; tests/sqp/test_ledger.cpp -> tests/core/ ; tests/interior/test_type_storage.cpp -> tests/core/

**Re-check of tests that drive solvers (rule 3):**
- The IPQP engine's own contract goes to qp/: test_ipqp_{acceptance, counters, options, trace, warm_restart}. test_ipqp_acceptance includes bench headers, but what it pins is IPQP acceptance, so it goes to qp/ and declares the bench dependency.
- The walk's EQP contract goes to qp/: test_eqp_solve, test_eqp_refine_ab.
- The SQP's own contract stays in sqp/: test_ipqp_dispatch, test_qp_{cap_levers, lift_oracle, mode_sites}, test_activity_telemetry, test_hs_battery, test_scale_smoke, test_start_point_projection, test_trace_messages_sqp. test_hs_sweeps is mixed (a fixture guard plus SQP policy adjudication) and stays in sqp; a split is registered.
- Shared engine behaviour goes to solver/: test_backend_error_seams, test_stop_poll_seams, test_evaluation_failure, test_threads, test_concurrency, test_dense_auto, test_dense_arm_engines, test_infinite_bound.
- An engine's own trace emission stays with the engine (test_ipm_trace, test_trace_messages_sqp, test_ipqp_trace). The schema, sinks and serializer go to trace/.
- Registered, not in RS: renaming test_qp_engine*.cpp to test_walk_engine*.cpp and test_kkt_calls.cpp to test_kkt_factor.cpp.

**Fixture include convention (S7).** Every test-local include is spelled from the tests root (`"qp/support/ssn_fixtures.h"`, `"support/console_capture.h"`), with tests/ as an include root for tests and bench. **Chosen guard**: tests/CMakeLists.txt checks at configure time that no basename appears in two support/ directories, and fails the configure if one does. That rules out quote-include shadowing. There are no duplicates today.

**Scale of edits.**
- 891 `#include <hven/...>` lines in 298 files (include 210, src 167, tests 489, bench 25).
- 205 test-local include lines in 137 files.

## 4. Impact on public include paths, and the compatibility policy

Only 16 public paths change, and only in their directory part. `core/`, `linear/`, `model/`, `qp/`, `warmstart/` and `hven.h` are unchanged; the examples include only `hven.h`.

| before | after |
|---|---|
| `hven/drivers/ipm_{enums,iterate,options,phase_report,solver,solver_types}.h` | `hven/ipm/…` |
| `hven/drivers/sqp_{options,solver,solver_types}.h` | `hven/sqp/…` |
| `hven/drivers/{common_options,solve_result,iteration_record,step_verdict}.h` | `hven/solver/…` |
| `hven/drivers/{trace,console_trace_sink,json_lines_trace_sink}.h` | `hven/trace/…` |

Consumer sed: `sed -i -E 's#hven/drivers/(ipm_[a-z_]+)\.h#hven/ipm/\1.h#; s#hven/drivers/(sqp_[a-z_]+)\.h#hven/sqp/\1.h#; s#hven/drivers/(common_options|solve_result|iteration_record|step_verdict)\.h#hven/solver/\1.h#; s#hven/drivers/(trace|console_trace_sink|json_lines_trace_sink)\.h#hven/trace/\1.h#'`. The full detail table, generated from the map, goes in `docs/migration-0.2.md` § "Include paths (RS)".

**Consumers.**
- hven-bench: 3 public paths, plus `detail/drivers/declared_diagnostics.h` → `detail/solver/`.
- tycho: on pre-M6 names; this adds rows to its catch-up list.

**Recommendation: a clean break, no forwarding headers.** M8 is the break window, the version is 0.2.0, and forwarding headers would break the umbrella-completeness rule and install two spellings of every path. Prepare the hven-bench update alongside RS.

## 5. Verification plan (CLAUDE.md §5 and §7)

**No group splits or merges a TU**, except the conditional deletion of whole TUs in RS-1a. So the §5 split protocol is not triggered, and every group runs its cheap parts anyway.

**The claim.** P-SYM (`scripts/psym_compare.sh`) per-symbol instruction identity with `--object-map` (and `--symbol-map` where needed), plus a check that `.text*`/`.rodata*`/`.data*` section hashes match per object in Release. A byte-identical libhven.a is not claimed: STT_FILE entries, renamed archive members and gtest `__FILE__` strings change.

### 5.1 Order of operations
- **RS-0, instrument prep (its own push).**
  - Make psym_compare.sh derive per-target object directories from the build. Today it hard-codes `tests/interior/...` and `tests/drivers/...` at lines 1146-1150, and RS-2 and RS-6a move both.
  - Add an object-map generator driven by `RS-move-map-v2.txt`. Self-test it on a build compared with itself, and on a synthetic rename.
  - **Static-initialiser check (M4).** Run `nm` on each renamed TU's object and look for `_GLOBAL__sub_I_<file>`:
    - library: solve_status→ipm_phase_status, warm_start→crossover, qp_engine→walk_engine, kkt_calls→kkt_factor;
    - tests: test_solve_status, test_scaffold, test_types.
    - Write a `--symbol-map` entry for each one present. The test TUs certainly have one, because gtest registration is a dynamic initialiser. A source scan of the four library TUs finds no namespace-scope dynamic objects and no `<iostream>`, but nm is the proof.
  - Baselines:
    - a P-SYM capture;
    - **per platform**, the sorted `--gtest_list_tests` for every executable;
    - `ctest -N`;
    - the installed file listing;
    - the trace-scanner site counts;
    - the core-layering file count.
  - Do not change the instrument inside the change it measures.
- **RS-1a..1e, five commits, single push (S8).**
  - Each commit is `git mv` plus include-line rewrites plus CMake path strings plus `hven_pch.h` plus the umbrella plus install-smoke include TUs plus re-pointing path-coupled tests, and nothing else. RS-1a's conditional deletions (if confirmed) plus the one color_text.h line are the only other edits.
  - Each commit passes the full §5.2 gate locally (Linux) before the next is written. The five are pushed to m8 once. CI (Linux, macOS, Windows) runs on that head, so macOS and Windows evidence is for the head only, and the push note says so.
  - `target_sources` order is preserved.
  - **Do not re-sort includes.** Run clang-format with `--sort-includes=false` (a stated deviation from §4 for these commits). Re-sorting changes template-instantiation order, and would break the `hven_pch.h` ↔ `ipm_solver.cpp` order identity.
  - **Core layering (M4).** From RS-1a on, `tests/core/test_core_layering.cpp` scans both include/hven/core and include/hven/detail/core, and asserts the file count (11 public + 6 detail after RS-1a). A new core file then has to be acknowledged explicitly.
- **RS-2, test re-layout.**
  - Moves plus CMake path updates (`add_subdirectory(interior→ipm, drivers→solver)`, golden and compile_fail directories), `.gitattributes:7`, the tests include root, and the duplicate-basename check.
  - Executable membership and target names stay the same.
- **RS-3, prose and literals (S9).**
  - Comments and banners in include/src/tests, `docs/*.md`, CMake banner comments, the CLAUDE.md §2 rewrite (owner approves), and the migration section. Plus `.git-blame-ignore-revs`.
  - **Literal policy:**
    - A string literal that can reach an artifact is either a declared change or left alone. Example: bench/bench_corpus.cpp:2333's CSV provenance line `# cells: tests/sqp/support/hs_problems.h` is **left alone**: bench is untouched, and the stale path is recorded in the migration note.
    - Golden-rig recipe description strings (tests/golden_rig/recipes.cpp:283, 447, 535) may be updated.
    - Origin-project names are swept from comments: tests/CMakeLists.txt:3 and :596, root CMakeLists.txt:226-227, and the rest found by a grep. The two CLAUDE.md §1 exceptions stay: the old-seam adapters and frozen CSV headers.
  - `docs/notes/**` and evidence artifacts are never rewritten.
- **RS-4, includes.**
  - clang-format with sorting on, plus unify on `<hven/...>`.
  - Replace `nlp_model.h`'s `qp/qp_types.h` include with `core/types.h`, adding explicit includes wherever something relied on getting qp_types.h through it. This removes exception 4.
  - Proof: P-SYM and `check_pch_neutrality.sh`.
- **RS-5, layering test.**
  - Generalise test_core_layering into `tests/core/test_layering.cpp`. It enforces §2.3's single order over include/, detail/ and src/, with the 12-pair allow-list. Every pair carries a debt note.
  - Add to `check_export_contract.sh`: nothing at the include/hven root except hven.h, and every public directory is a known component.
  - Mutation proof: a planted violation makes it fail.
- **RS-6a, executables (MANDATORY, M3).** See §5.4.
- **RS-6b (declared new evidence).** Lift the UNIX-only gate for sources that do not need the fork/exec corpus runner. Windows CI is the evidence, and a failure is a finding.
- **RS-7 (registered, not RS):**
  - symbol renames (`QpEngine`→`WalkEngine` and the arms);
  - unifying the two factor families;
  - per-engine split of counters_dump and result_message;
  - moving the QP option types to qp/ (debt 2/3);
  - src-private headers;
  - the memory_management decision;
  - splitting test_hs_sweeps;
  - renaming test files;
  - a review of bench/'s layout.

### 5.2 Gate for each RS-1a..1e, RS-2, RS-3 and RS-4 commit
1. Clean configure with `CCACHE_DISABLE=1`, linux-clang-release, build library + tests + bench.
2. P-SYM before/after with the generated `--object-map` and the RS-0 `--symbol-map`: 0 UNCLASSIFIED, empty exceptions file. Covers library, test and bench objects. A conditional deletion shows up as removed symbols only.
3. compile_commands.json flag diff, normalised for source and `-o` paths: identical. The only expected delta is the PRIVATE `interior/utils→util` include directory (RS-1a), or its removal.
4. **`check_pch_neutrality.sh` on RS-1a, RS-1b and RS-1c** (all touch `hven_pch.h`'s include set), and on RS-4.
5. Debug and Release ctest pass. Sorted gtest list per executable and the `ctest -N` set are identical to the RS-0 baseline. For RS-6a, see §5.4.
6. **Path-coupled tests are re-pointed and proven not vacuous.**
   - test_trace_off_path scans `src/{drivers,qp}` and is guarded only by ">20 sites". Re-point it to `{ipm,sqp,solver,qp,warmstart}` (recursive) and assert the site count **equals** the RS-0 count.
   - test_message_sites.cpp (reads ipm_solver.cpp and sqp_solver.cpp).
   - `HVEN_SQP_SOLVER_SOURCE` (tests/sqp/CMakeLists.txt:135).
   - The golden and fixture directory definitions.
   - The compile_fail directory.
   - The core-layering file count.
7. Install: `check_install_smoke.sh` on both front ends, and `check_export_contract.sh`. The installed listing equals the RS-0 listing with the map applied. Old directories are absent. The public header count is 55. The hvenConfig sentinel resolves.
8. Golden rig passes, with expected tables unchanged.
9. 27-cell replay times three engines, byte-exact against the pre-break pin. Co-run allowed: counters only, `MKL_NUM_THREADS=1`, pinned physical cores, protocol declared in the artifact.
10. CI passes on all three lanes on the pushed head.

### 5.3 CMake and other non-source changes
- **src/CMakeLists.txt**:
  - path strings, with order kept;
  - the PRIVATE include directory moves to `util`, or is removed if color_text goes;
  - `_hven_pch_sources` (`ipm/ipm_solver.cpp`, `ipm/ipm_options.cpp`);
  - `_hven_expected_source_count` stays 58 (57 if color_text goes);
  - the historical PCH table keeps its old paths, as the file already does.
- **tests/CMakeLists.txt and the per-directory CMakeLists**:
  - source lists;
  - the seam executables' `${HVEN_SRC_DIR}/…` lists (kkt_factorization, kkt_factor, `qp/walk/…`, `qp/ipqp/…`, solver/trace TUs);
  - `add_subdirectory` names;
  - the include root;
  - the duplicate-basename check.
- **cmake/hvenConfig.cmake.in:97**: sentinel → `hven/hven.h` (RS-1c). Optionally warn if `<prefix>/include/hven/drivers/` exists.
- **.gitattributes:7**: golden jsonl path (RS-2).
- **bench/**: its layout is untouched. The only forced edits are include lines (RS-1, RS-2) and the `hven_sqp_tests` hook at bench/CMakeLists.txt:160-169 (RS-6a, which re-targets it to `hven_bench_tests`).
- **scripts/psym_compare.sh**: RS-0 (derived directories), plus the target names in RS-6a.
- **ci.yml**: comments and target names (RS-6a). The PCH derivation is already path-agnostic.
- **docs**: testing.md (seam inventory with the new paths and executable names), build.md, concurrency.md, counters.md, the trace-schema docs, migration-0.2.md (also the `./hven_drivers_tests` regeneration line).

### 5.4 RS-6a: one executable per tests/ directory (mandatory)

**Normal executables.** Each tests/X/CMakeLists.txt defines `hven_X_tests`: core, linear, model, kkt, qp, warmstart, solver, trace, ipm, sqp and bench. Sources come from:
- `hven_tests` (split into core, linear, warmstart and qp);
- `hven_interior_tests`;
- `hven_drivers_tests`;
- `hven_model_tests`;
- `hven_sqp_tests`.

**Two kinds of process stay separate, each documented in its CMakeLists:**
- **Seam executables** recompile library TUs with `HVEN_TESTING` and cannot share a binary with ordinary tests. Rule: at most one `hven_X_seam_tests` per directory.
  - linear (test_fault_injection) and kkt (test_kkt_factorization_faults), from `hven_fault_injection_tests`;
  - qp: test_ipqp_seams plus the 5 walk-seam tests plus test_dense_solve_wound_seams, merging `hven_ipqp_seam_tests`, `hven_qp_walk_seam_tests` and one source of `hven_sqp_seam_tests`;
  - solver: test_backend_error_seams, test_stop_poll_seams;
  - ipm: test_ipm_dense_guard_seams;
  - sqp: the equality-basis and ladder seams.
  - Today `hven_sqp_seam_tests` recompiles the whole library. Splitting it means up to three whole-library HVEN_TESTING recompiles (solver, ipm, sqp, and possibly qp). That cost is measured in RS-6a on parallel wall-clock and peak RSS (see Q9).
- **`hven_linear_alloc_tests`** (from `hven_dense_alloc_tests`). It replaces global operator new and wraps malloc at link time, so it needs its own process.

**Preservation rules.**
- **Platform gates per source.** Every former `hven_sqp_tests` source keeps `if(NOT WIN32)` in its new list. The `if(NOT APPLE)` and `if(APPLE)` source gates move with their sources.
- **Unity per source.** Only `hven_interior_tests` used `UNITY_BUILD ON` (batch 8). A target receiving former-interior sources (ipm, model, kkt, core) turns UNITY on and sets `SKIP_UNITY_BUILD_INCLUSION` on every source that was not unity before.
  - Batches will be re-composed, so watch for anonymous-namespace and static-helper clashes (they are compile errors) and for macro leakage between merged TUs. The gtest-list and pass-set identity in the proof below catches behaviour changes.
- **Per-target state follows its sources.** Audit each `HVEN_*` definition and give it only to targets whose sources reference it:
  - the 9 baseline-CSV definitions;
  - `HVEN_QP_E2D4_FIXTURES`;
  - `HVEN_SQP_SOLVER_SOURCE`;
  - `HVEN_SRC_ROOT`;
  - the golden directories.
  - Likewise for `bench/ipm_corpus_leg.cpp` and the `hven_sqp_corpus` dependency (both go to `hven_bench_tests`), the continuation-probe timeout include (stays with sqp), and the compile_fail probes (to tests/model).

**Proof.**
- **Per platform** (Linux, macOS, Windows), the union over all test executables of the sorted gtest names equals the RS-0 baseline union, exactly. Same for the `ctest -N` names.
- All pass on all three lanes.
- P-SYM: test objects are identical apart from the unity-batch objects, which are compared per symbol through the object map.
- Update psym_compare.sh's target table, docs/testing.md, docs/migration-0.2.md and the CI comments to the new target names.

## 6. Risks and open questions (each with a recommendation)

- **Q1. Superseding AP-1 §7.** The owner records RS as the ruling that replaces it.
- **Q2. `notices/boost-bsl1.txt` names `src/interior/utils/get_core_count.cpp`, and notices are protected.** Recommend the owner sanctions an appended relocation record (existing text untouched). RS-1a waits on the ruling. `detail/linear` stays put, so `eigen-mpl2.txt` and §6 stay valid.
- **Q3. Names.** ipm/sqp (matching the type prefixes), solver/, util/, barrier/, walk/ssn/ipqp/. Recommend as written.
- **Q4. Compatibility policy.** Recommend a clean break.
- **Q5. NonLinearProgram and NlpModel stay in model as bridges.** Moving them would create model→engine edges. Revisit with the T8 §6 group.
- **Q6. bordered_eqp and eqp_solve in walk/** go beyond M2's list. This is rule 1 applied: the walk is their only library includer. If the settler prefers the literal M2 list, both go to kkt, and kkt then includes nothing from walk either way.
- **Q7. S5 deletions are conditional on owner confirmation**, and color_text has been added to the same question. memory_management (the BumpAllocator; only tycho uses it, on an old pin) is a separate decision.
- **Q8. Debt owners** (§2.3): exceptions 2/3 → AP-4 options registry; exception 1 → model-contract group; solver_counters → consistent-interface track; 7-11 → RS-7 split. The settler confirms the owners.
- **Q9. Seam-executable build cost** in RS-6a. If the measured parallel wall-clock or peak RSS is unacceptable, the fallback is one documented exception, `hven_seam_tests`, for the whole-library seam sources. The owner decides on the measurement.
- **Q10. Conflicts with in-flight branches.** RS touches about 300 files. Land it in a quiet window after AP-5, before AP-4 and the trace upgrade. Give rebasing agents the generated sed map, and announce on the tycho lane.
- **Q11. Stale install prefixes.** Document it, plus an optional config-time warning.
- **Q12. CLAUDE.md §1 is stale** (owner wording), alongside §2.
- **Q13. Same basenames under different engines** (`detail/{ipm,sqp}/globalization/{soc,restoration}.h`). Accept: full paths are always used. Drop self-path lines from banners in RS-3.
- **Q14. Coverage and blame continuity.** `git mv` with no content edits beyond include lines, plus `.git-blame-ignore-revs`. Codecov history breaks; that is cosmetic.
- **Q15. Single push (S8).** An intermediate commit's macOS and Windows behaviour is never CI-observed. Bisecting inside the five commits works on Linux only. This is accepted as the price of not landing a half-moved tree.

## 7. Proposed CLAUDE.md §2 rewrite

```
## 2. Repository structure

hven is organised by engine, with shared layers. There are five engines: the NLP solvers
ipm and sqp, and the QP engines walk, ssn and ipqp (under qp/). A component name X appears
in up to four places with one meaning: include/hven/X/ (public headers),
include/hven/detail/X/ (internal headers), src/X/ (translation units), tests/X/ (tests).
A file lives with the component that owns its concept. Engine-private code lives in the
engine. A shared layer exists when at least two engines include it, or when it is a named
bridge (model's problem forms, the warm-start hand-off types, the shared solver surface,
trace).

include/hven/
  hven.h           the umbrella: includes every public header
  core/            shared vocabulary: types, errors, statuses, the counters contract,
                    stop token, ledger, pattern hash, version
  linear/          sparse and dense symmetric factorizations (MKL Pardiso / Apple
                    Accelerate, LAPACK dense arm)
  model/           the problem contract: Nlp, Problem, the piece and assembly surface
  qp/              QP subproblem types and IPQP evidence
  warmstart/       the warm-start currency, seeding, the SQP native warm start, the IPM
                    polish extension (continuation lives in detail/sqp/)
  solver/          what both solvers share: CommonOptions, SolveBudget, SolveResult,
                    iteration records
  ipm/             the interior-point solver (IpmSolver, IpmOptions, results)
  sqp/             the SQP solver (SqpSolver, SqpOptions, results)
  trace/           the trace-sink interface and the console / JSON-lines sinks
  detail/          internals: installed, no compatibility promise
    core/ util/    internal vocabulary; generic utilities
    barrier/       barrier-function kernels and bound sets (IPM + IPQP); kernels,
                    not the mu strategy (see ipm/)
    linear/        backend sessions (MPL-derived files: see §6)
    model/         engine-facing problem forms and bridges
    qp/            QP data; walk/ ssn/ ipqp/ = the three QP engines
    kkt/           KKT systems shared by engines (KktFactor, KktFactorization,
                    assembly, Schur border, inertia regularization)
    warmstart/     crossover ingest, SQP warm state, mesh transfer
    solver/        plumbing both solvers share
    ipm/           IPM internals and mu governors; globalization/ = acceptance,
                    line search, SOC, watchdog, restoration
    sqp/           SQP internals, continuation, predictor; globalization/ = funnel,
                    trust region, SOC, elastic, restoration; quasi_newton/ = Hessian
                    approximations
src/               translation units, mirroring the components. A TU sits in the
                    deepest directory holding every header it implements (so
                    src/sqp/globalization/ exists, while the IPM's globalization
                    bodies live in src/ipm/). hven_pch.h is the opt-in PCH
tests/             tests/X pins component X's contract, whatever drives it; one
                    hven_X_tests executable per directory (plus hven_X_seam_tests where
                    HVEN_TESTING seams exist). Fixtures live in X/support/;
                    support/ = fixtures owned by no component; bench/ = gates for
                    the bench instruments; golden_rig/; install_smoke/
bench/             benchmark harness and tracked baselines
examples/          one-problem-per-program examples, built and run in CI
bindings/          reserved for the optional Python module (nothing builds yet)
scripts/           gates and instruments (install smoke, export contract, P-SYM,
                    PCH neutrality, census and replay runners)
cmake/             Find* modules, compile options, the package config template
dep/               vendored submodules (Eigen, fmt)
notices/           third-party license notices (protected: §6)
docs/              engineering documentation; docs/notes/ holds frozen records

Layering, for every header and TU: core < util < barrier < linear < model < qp data <
kkt < qp engines < warmstart < solver < {ipm, sqp} public types < trace < engine
implementations. A file includes only its own level or lower ones; ipm and sqp never
include each other. Public headers never include detail/. The file-to-file exceptions,
each with its debt owner, are listed in tests/core/test_layering.cpp, which enforces this
order.
```
