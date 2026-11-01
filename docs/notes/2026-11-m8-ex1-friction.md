# M8 EX-1 friction log: the eight examples against the CURRENT public API

Written while writing `examples/` (eight programs, one problem each) against
hven at `origin/m8` d0f8cd99, exactly as the public headers stand: no library
header or source was touched. Entries are logged as met, not curated; they are
observations, not redesigns. AP-1 dispositions them.

Method. Each example consumes only `include/hven/` (what an install tree
ships). Where something could only be done by reaching into `detail/`, it was
done that way and logged. The experiment notes (marked TRIED) record what the
API did when a natural thing was attempted.

Severity. **blocker**: a stranger cannot get the thing working without reading
`detail/` or the library's own sources, or the failure is silent and wrong.
**confusing**: works, but the right spelling is not discoverable, is documented
only in prose, or is inconsistent between the two engines. **cosmetic**:
friction with no consequence for the result.

Paths are `include/hven/...`; lines are at d0f8cd99.

## Entries

### Model contracts and construction

**1. Two model types, one problem (ex 1, 2, 3, 5, 6, 7, 8; blocker for ex 7).**
Wanted: state HS071 once and solve it on both engines. Had to: write an
`NlpTripletModel`, then for the interior-point engine call
`make_nlp_program(problem)` (a `NonLinearProgram`, `drivers/ipm_solver.h:404`
takes `NonLinearProgram &`) and for the SQP engine wrap it in
`NlpProblemModel` (a `NlpModel`, `drivers/sqp_solver.h:1036` takes
`const NlpModel &`). The triplet model is the only declaration both engines can
reach. A problem written directly as an `NlpModel` (ex 7) reaches the
interior-point engine only through the core-taking `make_nlp_program` overload (`model/make_nlp_program.h:48`), whose `NLPAdapterCore` argument is constructible from an `NlpModel` solely in `detail/model/nlp_adapter.h:135`; no public header completes the path. Pre-seeded item, hit.
Severity: confusing for the triplet route, blocker for an `NlpModel` author.

**2. The two contracts disagree on conventions (ex 7).** The triplet model
declares the *lower* triangle of the Lagrangian Hessian as `(row, col)` triplets
with 32-bit `int` counts (`model/nlp_triplet_model.h:69, 94`); `NlpModel`
returns the *upper* triangle as a row-major sparse matrix with 64-bit `Index`
counts (`model/nlp_model.h:74, 107`). Multipliers are one block per problem
(`lambda`) on one side and split `lambda_e`/`lambda_i` on the other; free
variables are `-inf/+inf` in both. Ex 7's Hessian is diagonal, so the triangle
convention was read from the docs and not exercised. Severity: confusing.

**3. `NlpModel` makes the author override six deprecated functions (ex 7).**
`eval_grad`, `eval_ce`, `eval_ci`, `eval_hess`, `eval_jac_e`, `eval_jac_i` are
pure virtual *and* `[[deprecated]]` (`model/nlp_model.h:84-117`); the
non-deprecated `*_in_place` forms are optional overrides whose defaults call the
deprecated ones. The only way to write a model is to override the deprecated
set, wrapped in `HVEN_SUPPRESS_DEPRECATED_BEGIN/END` (`core/compiler.h`); with
install-tree `-isystem` includes no warning appeared even without the bracket,
so the bracket is only needed in-tree. Severity: confusing.

**4. QN mode still demands the Hessian (ex 7).** `hessian_mode = kLbfgsDamped`
never calls `eval_hess` (observed `eval_hess_calls == 0`), but `eval_hess` is
pure virtual on `NlpModel`, so a first-derivative-only model must supply a
Hessian it will never use (`model/nlp_model.h:107`). Severity: cosmetic now;
relevant to the M9 first-order mode.

**5. Zero-row blocks need explicit empty overrides (ex 2, ex 7).** A
bound-constrained triplet model must still define `eval_g`, `jac_structure`,
`eval_jac` with empty bodies (`model/nlp_triplet_model.h:87-101`); an `NlpModel`
with `me() == 0` must still return `Vec(0)` and `SpMatRM(0, n)`. Severity:
cosmetic.

**6. Asymmetric solver construction (all examples).** `IpmSolver ipm;` and
`IpmSolver(IpmOptions = {})` (`drivers/ipm_solver.h:254`) against
`SqpSolver sqp{SqpOptions{}}`, whose constructor has no default
(`drivers/sqp_solver.h:662`). Pre-seeded item, hit. Severity: cosmetic.

**7. Defaults differ between the engines and are not what the first run needs
(ex 1).** `IpmSolver` with default options prints a banner, a 14-column
colour-coded table and timing blocks (`IpmOptions` default `print_level` 0;
`drivers/common_options.h:73` states the shared default 3 is the SQP's). Every
example sets `common.print_level = 3` on the IPM options first. The banner
still reads "InteriorPointSolver" (`src/drivers/console_trace_sink.cpp:278`),
the pre-rename name. Severity: confusing.

### Namespaces, headers and includes

**8. No umbrella header; the include set per example is 4-6 (all examples).**
There is no `hven/hven.h`. Ex 1 needs `drivers/ipm_solver.h`,
`drivers/sqp_solver.h`, `model/make_nlp_program.h`,
`model/nlp_problem_model.h`, `model/nlp_triplet_model.h`; ex 6 adds
`model/structure_identity.h` and `warmstart/ipm_polish_extension.h`; ex 8 adds
`drivers/trace_writer.h`. `to_string(SolveStatus)` comes transitively via the
engine headers (`core/solver_status.h`). Pre-seeded item, hit. Severity:
cosmetic.

**9. The namespace is `hven::solvers` with strays in `hven` (ex 1-8).** The
problem classes and engines are `hven::solvers::*`; `hven::ConstEigenRef`
(needed in every triplet override), `hven::Vec`, `hven::SpMatRM`,
`hven::Index` live one level up. A consumer writes `using namespace
hven::solvers;` and still spells `hven::ConstEigenRef` in every callback
signature. CLAUDE.md section 1 states `hven::`. Pre-seeded item, hit.
Severity: cosmetic.

**10. A public virtual's signature comes from a `detail/` header
(`model/nlp_triplet_model.h:10`).** `NlpTripletModel`'s pure virtuals take
`hven::ConstEigenRef<...>`, defined in
`detail/interior/typedefs/eigen_types.h`; the public header includes the detail
header so that the user's override can name it. Pre-seeded item (detail/
includes), hit. Severity: cosmetic.

**11. Declaring pieces needs three `detail/` includes (ex 4).**
`ObjectiveFunction` and `ConstraintFunction` are only forward-declared in the
public `model/assembly_declaration.h:31`; to put a piece into
`AssemblyDeclaration::objectives_`/`equality_constraints_` the example includes
`detail/interior/objective_function.h`, `detail/interior/constraint_function.h`
and `detail/solvers/solver_interface_specs.h` (for `ConstraintInterface`,
`ObjectiveInterface`, `SolverIndexingData`). The scatter protocol also uses
`kkt_canonical_lock_col` from `detail/interior/indexing_data.h`. The only
public header on the path is `solver_interface_adapter.h`. Pre-seeded item,
hit. Severity: blocker for the partitioned route as a public API.

**12. Test-internal names sit in the engine header's include closure
(`drivers/ipm_solver.h:48-66`, `:884-889`).** Twenty-two gtest fixture/harness
forward declarations and `friend class ::..._Test` grants in a public header.
Every example that includes `ipm_solver.h` carries them; they cause no failure
and were noticed only while reading the header to find the constructor.
Pre-seeded item, seen, not an obstacle. Severity: cosmetic.

### Bounds and conventions

**13. TRIED `+/-1e30` for "no bound" (ex 3).** The Ipopt-convention spelling was
the first thing written for free variables. It is accepted silently as a real
finite bound (the solve returned optimal in 5 iterations on both engines); only
prose in `model/nlp_triplet_model.h:43-60` says the library reads it as a bound
and that the spelling must be `+/-infinity`. Nothing warns or refuses at
`1e20`/`1e30`. The example uses `std::numeric_limits<double>::infinity()`.
The same comment (`:56-59`) records that the SQP engine's SSN QP mode applies a 1e20 cutoff of its own, so a bound of 1e30 is finite on the interior-point route and on the default kWalk route, and absent under kSsn.
Pre-seeded item (`+/-1e20`), hit in the 1e30 variant. Severity: confusing.

**14. Multiplier signs must be read, then verified (ex 2, 3).** `z` is `<= 0` at
an active upper bound (observed `-1.0000` in ex 2); the equality multiplier
follows `L = f + lambda^T c` (ex 3 compares against the analytic value, which
matches). Both are stated at `drivers/solve_result.h:101-117`; neither is
guessable. Severity: cosmetic (the documentation is correct and complete).

**15. Tolerances are spelled differently on the two engines (ex 6).** The
interior-point options carry four convergence tolerances,
`kkt_tol`, `econ_tol`, `icon_tol`, `bar_tol`
(`drivers/ipm_solver_types.h:157-163`); the SQP options carry `kkt_tol` and
`feas_tol` (`drivers/sqp_solver_types.h:450-456`). Loosening the IPM to 1e-3
took four assignments, tightening the SQP to 1e-10 took two. Severity:
confusing.

**16. Three spellings of one iteration cap, visible in the options dump (ex 8).**
`IpmOptions::max_iters` (`drivers/ipm_solver_types.h:113`, per phase, first line
of `dump_options`), `SqpOptions::max_iter` (`drivers/sqp_solver_types.h:462`,
third line of its dump) and `SolveBudget::max_iterations`
(`drivers/solve_result.h:78`). No example set a cap; the dump in ex 8 shows the
first two side by side. Pre-seeded item, seen. Severity: cosmetic.

### The piece protocol (ex 4)

**17. A piece type stored with a `std::string` member corrupts the heap
(ex 4).** `QuadraticTerm` first carried `std::string label`. It compiled; the
program died with `free(): invalid pointer` inside
`NonLinearProgram::analyze_partitioning` (found with gdb). The erased handle
stores a small object inline and moves it by `memcpy`, so pieces must be
trivially relocatable and a `std::string` with small-string optimisation is not.
This is stated only in `detail/interior/utils/type_storage.h:23, 61, 75`
(comments, "std::string SSO ... NOT safe"); nothing in a public header says it,
and nothing is checked at compile time (the comment explains why a
`static_assert` is not available). Fixed by holding a `const char *`. Severity:
blocker (silent corruption, discoverable only from a `detail/` comment).
Review verdict: library DEFECT -- unenforced, publicly undocumented trivially-relocatable precondition; violation is memory corruption (TypeStorage move = memcpy, type_storage.h:104). Registered as M8 defect DF-1.

**18. The piece contract is a hand-written protocol with no public
specification (ex 4).** To write four pieces the example had to learn, from
`detail/model/nlp_adapter.h` (the library's own pieces) and
`detail/interior/indexing_data.h`: what `inner_constraint_starts_`,
`inner_gradient_starts_` and `inner_kkt_starts_` index; that `FX` and `GX` are
claim slots while `L` is addressed by `c_loc`; that a Jacobian claim is
`(c_loc + con_offset, v_scatter_loc)` and an eliminated variable still claims a
slot as `(-1, -1)`; which flag (`unique_constraints_`, `KKTClashes[col] != -1`)
guards which lock and which column keys it; that `num_kkt_elements` must be
additive in its two flags; and that each application's claims are contiguous.
`model/assembly_declaration.h:41-100` gives the concept (member names and
types) and none of this. The example's four pieces are about 250 lines for a
chain problem. Severity: blocker for a stranger.

**19. An objective piece must also carry the constraint surface (ex 4).**
`ObjectiveAssemblyPiece` is `ConstraintAssemblyPiece && ObjectiveAssemblySurface`
(`model/assembly_declaration.h:108`): the five `constraints_*` evaluations and
`output_rows` must exist on an objective, and the library's own objective piece
fills them with throwing stubs (`detail/model/nlp_adapter.h`, `NLPObjectivePiece`);
the example copies that. Severity: confusing.

**20. Every piece type needs a `SolverInterfaceAdapter<T>` specialization (ex 4).**
Even a plain value type must be registered with
`template <> struct hven::solvers::SolverInterfaceAdapter<T> :
DirectFunctionModel<T> {}` (`solver_interface_adapter.h:177`); an
unregistered type is a `static_assert`, which is well authored, but the
registration is boilerplate for the overwhelmingly common case. Severity:
cosmetic.

**21. Requested partition count is clamped silently, and stated twice (ex 4).**
The chain (500 steps) asked for 4 partitions and got 3
(`program.num_partitions_` after `adopt_declaration`), and asked for 1 and got 1.
The clamp is documented at `model/make_nlp_program.h:51-60`. The count is also
given in two places, `NonLinearProgram(int)` (`model/non_linear_program.h:203`)
and `AssemblyDeclaration::partition_count_` (`model/assembly_declaration.h:208`);
`adopt_declaration` takes the declaration's. Severity: confusing.

**22. Two ways to populate a `NonLinearProgram` (ex 4).** Public master lists
plus `make_nlp(PV, EQ, IQ)`, or an `AssemblyDeclaration` plus
`adopt_declaration` (`model/non_linear_program.h:227, 285`); the example uses
the declaration because it is the documented provider boundary, but both are
public and the list route is what the library's own transcription uses.
Severity: cosmetic.

### Parameters, warm start and composition

**23. TRIED changing a parameter inside a live model (ex 5): wrong answers,
status optimal.** The natural parameter sweep keeps one model and one program
and mutates `p` between solves. Result on both engines: every solve reports
`optimal` and the objective is wrong (the bound `1 + p/2` is read once, at `NlpProblemModel` construction / transcription -- src/model/nlp_problem_model.cpp:145 -- and nothing in `model/nlp_triplet_model.h` says `bounds()` is read only then), and the "warm" sweep took
1 iteration per step after the first. The contract says so in prose
(`model/nlp_triplet_model.h:19-30`: structures queried once, callbacks pure,
"results are cached per iterate"), but a pure function of `x` is exactly what a
parametric model is not. The only supported route on the interior-point side is
to build a new model and transcribe again per `p`; `ParametricNlpModel` with
`set_parameters` exists (`model/nlp_model.h:251`) for the SQP's `NlpModel` only.
The warm-start payload stays valid across re-transcriptions (the stamp keys on
structure), which is what makes the rebuild workable. The shipped example
rebuilds per `p`. Severity: blocker (silent wrong result with an optimal
status).
Review verdict: documented misuse with a documentation gap (callbacks must be pure, nlp_triplet_model.h:26-29); no parametric route into the interior-point engine.

**24. Three places to get a warm start from the SQP engine (ex 5, 6).**
`SqpResult::warm_start` (native `SqpWarmStart`,
`drivers/sqp_solver_types.h:1817`), `result.export_warm_start()` (the shared
`WarmStartData`, `drivers/solve_result.h:183`, `std::optional`; the SQP never
returns empty) and `SqpSolver::export_warm_start()` (throws if no solve has
completed, `drivers/sqp_solver.h:1236`). The interior-point engine has only the
middle one. The solve call is selected by the argument type (`SqpWarmStart` vs
`WarmStartData`, plus an optional third `SolveBudget`); a braced `{}` third
argument is ambiguous (stated in the header). Severity: confusing.

**25. Polish composition: two routes, five headers, reaches only "seeded"
(ex 6).** `to_sqp_warm_start(payload, model.lower(), model.upper(),
declaration_key(program->declaration()))` needs `warmstart/ipm_polish_extension.h`,
`model/structure_identity.h`, the transcribed program kept alive for its
declaration, and the SQP model for its bounds
(`warmstart/ipm_polish_extension.h:112-125`); handing the payload to
`solve(model, x0, payload)` does the same with none of that and gave identical
results (2 iterations against 6 cold, start level `Seeded` both ways). The
header says a payload can never resolve above `kSeeded`
(`drivers/sqp_solver.h:1138-1165`). The doc of the staging function refers to
"nlp_model.h's -1e20 convention" for absent bounds, which `model/nlp_model.h`
contradicts (`+/-inf`, "nothing in this layer treats a finite magnitude as a
stand-in for infinity"). Severity: confusing (docs conflict, two routes).

### Results, callbacks and diagnostics

**26. Two stationarity numbers on `SqpResult`, two on `IpmResult` (ex 6, 8).**
`SolveResult::stationarity` (shared, declared space) and the engine's own
(`SqpResult::sqp_stationarity`, `drivers/sqp_solver_types.h:1709`;
`IpmResult::kkt_inf`) are printed side by side in ex 8; they agree at the
solution here and are documented to differ in general. Severity: cosmetic.

**27. `iterations` and the callback count disagree by engine (ex 8).** The
interior-point solve reported 10 iterations and 10 callbacks; the SQP solve
reported 5 iterations and 6 `depth == 0` callbacks (the first is the start
row). `IterationEvent::iteration` is "the major's index... also this row's index
in history" (`drivers/solve_result.h:316`). Severity: cosmetic.

**28. A depth filter is the caller's job in SQP callbacks (ex 8).**
Restoration sub-solves call the same callback with `depth >= 1` and a different
variable space (`drivers/solve_result.h`, `IterationEvent::depth`); ex 8 filters
on `depth.value_or(0) == 0`. Severity: cosmetic.

**29. Sinks are raw non-owning pointers; no way to read a trace back (ex 8).**
`attach_trace(TraceSink *)` (`drivers/ipm_solver.h:784`) borrows the sink, which
must outlive every later solve; the JSON-lines output has no public reader, so
ex 8 tallies events with a `std::string::find` for `"ev":"`. The schema is in
`docs/trace-schema-v0.md`. In this solve the IPM emitted 8 distinct `ipm.*`
event names and the SQP 4 (`sqp.*`, `qp.mode`). Severity: cosmetic.

**30. `dump_options` prints floats at 17 significant digits and skips nothing
(ex 8).** `kkt_tol = 9.9999999999999995e-07` for the default `1e-6`; the IPM dump
and the SQP dump format doubles differently from the console and each other
(`drivers/ipm_solver_types.h:771`, `drivers/sqp_solver_types.h:1229`). Severity:
cosmetic.

**31. IPM bound multipliers carry solver noise where the SQP reports exact
zeros (ex 8).** `z` at HS071's solution: `(1.09, -1.5e-10, -3.7e-12, 3.5e-11)`
(IPM) against `(1.09, 0, 0, 0)` (SQP). Documented behaviour of an interior
method; mentioned because the shared `SolveResult` makes the two look like the
same quantity. Severity: cosmetic.

### Not hit

Recorded so the pre-seeded list is accounted for: the whole-solve factorization
ceiling (hven-bench F3) was not touched by any example; no example needed a
checkout of `docs/` to build against the install tree (the CMake package re-finds
MKL and carries `EIGEN_MAX_ALIGN_BYTES=32` on the target, so a consumer passes
no flag, `cmake/hvenConfig.cmake.in`).

## Appendix: build and run record

Toolchain: clang 22.1.8 (library and in-tree examples), g++ 16.2.1 (the
stranger-default front end for the second install-tree lane); Linux 7.2.12
x86_64, 16 cores; Release; `MKL_NUM_THREADS=1`; one process at a time under
`flock /tmp/box-build.lock`; library at `origin/m8` d0f8cd99 plus the
`examples/` tree and the root CMake option. Library installed with
`cmake --install` into a scratch prefix; consumer configured as its own
top-level project with `-DCMAKE_PREFIX_PATH=<prefix>` (`examples/CMakeLists.txt`
detects the absent `hven::hven` target and calls `find_package(hven)`).

Lanes: (a) in tree, `-DHVEN_BUILD_EXAMPLES=ON`; (b) install tree, clang++;
(c) install tree, g++. All eight build with no warnings and exit 0 on all three
lanes; summary lines are byte-identical across lanes (wall-clock lines
excluded; timings are informational).

| Example | Result line(s) (status, iterations) |
|---|---|
| 01 HS071 both engines | IPM optimal 10 its, f = 17.0140173; SQP optimal 5 its, f = 17.0140173 |
| 02 bounded Rosenbrock | IPM optimal 17 its; SQP optimal 15 its; x = (0.5, 0.25), f = 0.25, z_x = -1 |
| 03 equality constrained | IPM optimal 5 its (f = 3.03337044, lambda = 0.870829); SQP optimal 5 its (f = 3.03337045, lambda = 0.870829) |
| 04 partitioned chain (500 steps) | 1 partition requested: adopted 1, optimal, 4 its, f = 0.368214320; 4 requested: adopted 3, optimal, 4 its, f identical |
| 05 warm continuation | IPM cold 3 3 9 10 11 (36) vs warm 3 2 8 5 4 (22); SQP cold 1 1 4 5 6 (17) vs warm 1 1 3 3 3 (11) |
| 06 IPM to SQP polish | IPM rough optimal 8 its (tol 1e-3); SQP polish staged 2 its, payload 2 its (both start level Seeded), cold 6 its; f = 17.0140172892 |
| 07 SQP quasi-Newton | exact optimal 6 its, 6 eval_hess calls; L-BFGS optimal 5 its, 0 eval_hess calls; f = 12.09109770 |
| 08 results, trace, options | IPM optimal 10 its, 10 callbacks, 8 distinct `ipm.*` events; SQP optimal 5 its, 6 callbacks, events `qp.mode`=5 `sqp.major`=6 `sqp.solve.begin/end` |

Experiments that are not shipped (kept out of `examples/`): the in-place
parameter mutation of entry 23 (run as a scratch copy of ex 5; all four sweeps
reported FAILED while every status was optimal) and the `std::string` piece of
entry 17.
