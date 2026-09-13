# Migrating across hven's M6 W5 API break

This is the one document a consumer reads to move a tree from the hven that
existed before the M6 W5 window to the hven that exists after it. It is
organised by **what you have to change**, not by the task that changed it.

**Scope.** W5 is hven's API break window: one declared event, a commit sequence
on `m6`, opened after the W4 close and closed by the ledger entry this guide
ships with. Everything described here has already landed. Nothing here is
speculative and nothing here is scheduled.

**Provenance — read this before quoting anything.** Every sentence below is
drawn from a per-task entry in `docs/notes/2026-09-m6-w5-migration-guide.md` (the
window's record, written WITH each breaking task and retained unchanged) or from
a close line in `docs/notes/2026-08-m6-ledger.md`. **This document makes no new
claim and states no new number.** Where a number appears it is the one the
per-task entry states, and the pointer beside it names the entry to check it
against. Where the two documents disagree, the per-task entry is the authority
and this one is wrong.

**What is NOT here.** The reasoning, the proofs, the P-SYM transcripts, the
correction records and the review history. Those are the per-task entries and
the ledger. This guide carries the before, the after, and the rewrite.

---

## 0. Start here

The break falls into five groups. Most consumers meet them in this order:

| # | group | you are affected if | section |
|---|---|---|---|
| 1 | **the names** | you name any hven type, enumerator or header path | [§1](#1-the-names) |
| 2 | **the solver interface** | you construct, configure, run or read either engine | [§2](#2-the-solver-interface--one-shape-on-both-engines) |
| 3 | **the model layer** | you derive from `NlpModel`, call its evaluations, or derived from the old problem base | [§3](#3-the-model-layer) |
| 4 | **the trace** | you attach a trace sink or derive from one | [§4](#4-the-trace) |
| 5 | **nothing** | — two tasks in this window changed nothing you can see | [§5](#5-what-did-not-change-for-a-consumer) |

A practical order: do §1 first with the rename tables (it is mechanical and it
touches everything), then §2 (it is the real work), then §3 and §4. §6 is the
runtime statement — read it, change nothing for it.

---

## 1. The names

The Scheme 1 rename sweep landed as ONE commit, driven by three manifests and
an applier rather than a bare sed. Matching is **exact, case-sensitive and
word-boundary**, so a compound identifier is a different identifier and was
left alone — `SqpDriverContract`, `InteriorPointSolverPresetFields`,
`AggregateEvalSeamTestAccess`, `NlpModelAggregateBoundary` and the frozen
corpus cell id `InteriorPointSolver_PolarLT_256seg` all still read the way they
read before.

*Per-task entry: the guide's `## T8.10` §§1–4.*

### 1.1 Types and functions

| was | is | |
|---|---|---|
| `InteriorPointSolver` | `IpmSolver` | |
| `SqpDriver` | `SqpSolver` | |
| `SqpSolution` | `SqpResult` | the old name is GONE (alias removed — §1.4) |
| `WarmStart` | `SqpWarmStart` | the old name is GONE (alias removed — §1.4) |
| `validate_sqp_options` | `validate` | the old name is GONE (alias removed — §1.4) |
| `HVEN_SQP_DRIVER_SOURCE` | `HVEN_SQP_SOLVER_SOURCE` | |
| `NLPProblem` | `NlpTripletModel` | |
| `NlpAggregate` | `NlpAssembly` | |
| `AggregateDeclaration` | `AssemblyDeclaration` | |
| `AggregatePiece` | `AssemblyPiece` | |
| `ConstraintAggregatePiece` | `ConstraintAssemblyPiece` | |
| `ObjectiveAggregatePiece` | `ObjectiveAssemblyPiece` | |
| `ObjectiveAggregateSurface` | `ObjectiveAssemblySurface` | |
| `AggregateCapability` | `AssemblyCapability` | |
| `NlpModelAggregate` | `NlpModelAssembly` | |
| `AggregateEvalSeam` | `AssemblyEvalSeam` | |

**`NlpModel` STAYS.** `NlpProblemModel` stays — it is the triplet-to-native
adapter, not the triplet model. `WarmStartData` stays: it is the SHARED payload
currency and was never the SQP's native object.

### 1.2 Enumerators

The interior-point engine's eight selector enums take `kPascalCase`
enumerators (CLAUDE.md §4). The member type aliases on the solver are
unchanged, so `IpmSolver::BarrierModes::kLoqo` names what
`InteriorPointSolver::BarrierModes::LOQO` named.

| enum | was → is |
|---|---|
| `BarrierModes` | `PROBE`→`kProbe`, `LOQO`→`kLoqo` |
| `LineSearchModes` | `AUGLANG`→`kAugLang`, `LANG`→`kLang`, `L1`→`kL1`, `NOLS`→`kNoLs` |
| `AlgorithmModes` | `OPT`→`kOpt`, `OPTNO`→`kOptNo`, `SOE`→`kSoe`, `INIT`→`kInit` |
| `QPAlgModes` | `Classic`→`kClassic`, `TwoLevel`→`kTwoLevel` |
| `QPOrderingModes` | `MINDEG`→`kMinDeg`, `METIS`→`kMetis`, `PARMETIS`→`kParMetis` |
| `BestCriteriaModes` | `ECONS`→`kEcons`, `ICONS`→`kIcons`, `KKT`→`kKkt`, `OBJ`→`kObj` |
| `QPPivotModes` | `OneByOne`→`kOneByOne`, `TwoByTwo`→`kTwoByTwo`, `E4`→`kE4`, `E6`→`kE6`, `E8`→`kE8`, `E13`→`kE13` |
| `PDStepStrategies` | `PrimSlackEq_Iq`→`kPrimSlackEqSplitIq`, `AllMinimum`→`kAllMinimum`, `PrimSlack_EqIq`→`kPrimSlackSplitEqIq`, `MaxEq`→`kMaxEq` |

**One of those is not a mechanical fold and is called out.**
`PDStepStrategies`' `PrimSlackEq_Iq` and `PrimSlack_EqIq` differ ONLY in where
the underscore falls — it separates the variables that take the PRIMAL step
from those that take the DUAL one — and a plain camel-case fold collides them
both onto one name. The separator is spelled `Split`: `kPrimSlackEqSplitIq` and
`kPrimSlackSplitEqIq`. Same two strategies, same order, same underlying values.

### 1.3 Headers and sources

Every `#include` moves with its header. If you include hven's umbrella headers
you are unaffected; if you name these paths, this is the whole list.

| was | is |
|---|---|
| `include/hven/drivers/interior_point_solver.h` | `include/hven/drivers/ipm_solver.h` |
| `include/hven/detail/drivers/interior_point_solver_fwd.h` | `include/hven/detail/drivers/ipm_solver_fwd.h` |
| `include/hven/detail/drivers/interior_point_solver_presets.h` | `include/hven/detail/drivers/ipm_solver_presets.h` |
| `src/drivers/interior_point_solver.cpp` | `src/drivers/ipm_solver.cpp` |
| `src/drivers/interior_point_solver_globalization.cpp` | `src/drivers/ipm_solver_globalization.cpp` |
| `tests/install_smoke/include_interior_point_solver.cpp` | `tests/install_smoke/include_ipm_solver.cpp` |
| `tests/interior/test_interior_point_solver_presets.cpp` | `tests/interior/test_ipm_solver_presets.cpp` |
| `include/hven/drivers/sqp_driver.h` | `include/hven/drivers/sqp_solver.h` |
| `include/hven/drivers/sqp_types.h` | `include/hven/drivers/sqp_solver_types.h` |
| `src/drivers/sqp_driver.cpp` | `src/drivers/sqp_solver.cpp` |
| `tests/install_smoke/include_sqp_driver.cpp` | `tests/install_smoke/include_sqp_solver.cpp` |
| `tests/sqp/test_sqp_driver.cpp` | `tests/sqp/test_sqp_solver.cpp` |
| `include/hven/model/nlp_problem.h` | `include/hven/model/nlp_triplet_model.h` |
| `tests/install_smoke/include_nlp_problem.cpp` | `tests/install_smoke/include_nlp_triplet_model.cpp` |
| `include/hven/model/nlp_aggregate.h` | `include/hven/model/nlp_assembly.h` |
| `tests/install_smoke/include_nlp_aggregate.cpp` | `tests/install_smoke/include_nlp_assembly.cpp` |
| `tests/interior/test_nlp_aggregate_contract.cpp` | `tests/interior/test_nlp_assembly_contract.cpp` |
| `include/hven/model/aggregate_declaration.h` | `include/hven/model/assembly_declaration.h` |
| `src/model/aggregate_declaration.cpp` | `src/model/assembly_declaration.cpp` |
| `tests/install_smoke/include_aggregate_declaration.cpp` | `tests/install_smoke/include_assembly_declaration.cpp` |
| `include/hven/model/nlp_model_aggregate.h` | `include/hven/model/nlp_model_assembly.h` |
| `src/model/nlp_model_aggregate.cpp` | `src/model/nlp_model_assembly.cpp` |
| `tests/install_smoke/include_nlp_model_aggregate.cpp` | `tests/install_smoke/include_nlp_model_assembly.cpp` |
| `tests/model/test_nlp_model_aggregate.cpp` | `tests/model/test_nlp_model_assembly.cpp` |
| `include/hven/detail/drivers/aggregate_eval_seam.h` | `include/hven/detail/drivers/assembly_eval_seam.h` |
| `src/drivers/aggregate_eval_seam.cpp` | `src/drivers/assembly_eval_seam.cpp` |
| `tests/sqp/test_aggregate_eval_seam.cpp` | `tests/sqp/test_assembly_eval_seam.cpp` |

`src/drivers/sqp_print.cpp` and `src/drivers/sqp_options.cpp` KEEP their names —
they are the SQP engine's printing and options TUs, not the driver's.

**`include/hven/detail/warmstart/warm_start.h` is NOT deleted.** Below the
removed alias it carries the live interior-point CROSSOVER —
`from_interior_point`, `IpCrossoverOptions`, `kIpActivityFactor` — so the file
and the crossover stay where they stand.

Two headers arrived earlier in the window and are named here because they are
new paths a consumer includes: **`hven/drivers/trace.h`** (§4) and
**`hven/core/compiler.h`** (§3.2). The other new public headers are named in
§2 where they are used.

### 1.4 The four alias removals — and the one field fold

Each alias was a spelling group 1 kept so its own task would not have to sweep
the call sites. This is that sweep.

1. **`SqpSolution` is gone; say `SqpResult`.** Same type since T8.4.
2. **`WarmStart` is gone; say `SqpWarmStart`.** Same struct since T8.5, which
   moved it to `warmstart/sqp_warm_start.h`. **The 51 gtest suites literally
   named `WarmStart` were NOT renamed** — a suite name is a different
   identifier from a type.
3. **`validate_sqp_options(o)` is gone; say `validate(o)`.** Same check since
   T8.3; the one-line forwarder is removed and the contract its doc carried now
   sits on `validate(const SqpOptions &)`.
4. **`InteriorPointSolver::kSeededIqMultFloor` and `::kSeededMultInitMax` are
   gone; say `hven::solvers::kSeededIqMultFloor` /
   `hven::solvers::kSeededMultInitMax`** (`warmstart/seeding.h`). Same values,
   same three-policies-not-unified rule; only the class-scope aliases are
   dropped.

**And the fifth thing: `SqpOptions::start_level` is folded into
`common.start_level`.** The field is deleted from `SqpOptions`;
`CommonOptions::start_level` — carried beside it and unread since T8.3 — is now
the ceiling the SQP engine caps a warm start with, and carries the SQP's own
contract in its doc. Both engines now read the same field. The default is
unchanged (`StartLevel::kWarm`), the ceiling semantics are unchanged, and
`SqpOptions`' remaining fields keep their declaration order.

```cpp
// before                                    // after
SqpOptions o;                                SqpOptions o;
o.start_level = StartLevel::kSeeded;         o.common.start_level = StartLevel::kSeeded;
validate_sqp_options(o);                     validate(o);
SqpSolution s = driver.solve(m, x0);         SqpResult s = solver.solve(m, x0);
WarmStart w = s.warm_start;                  SqpWarmStart w = s.warm_start;
```

**The fold is a LAYOUT change and is declared as one** — see §6.

### 1.5 What kept an old name (the residual policy)

A residual is a CLASSIFIED decision, never a leftover. The classes:

* **History is not rewritten.** `docs/notes/**` and `docs/superpowers/plans/**`
  are preserved by path policy: a note states what was true when it was
  written.
* **Frozen artifacts are pinned by their bytes.** `bench/baselines/**` and
  `docs/notes/data/**` were not read at all — CLAUDE.md §1's second exception.
  The interior leg's own baseline names old identifiers in its `#` comment
  lines; the comparator ignores comments and the data columns are the pin.
* **Measurement records inside live files.** `src/CMakeLists.txt`'s PCH sweep
  table and its copy in `docs/build.md` name TUs *as they were named when the
  sweep ran*, and say so in their own words.
* **The console banner's label stays `InteriorPointSolver`.**
  `tests/drivers/console_sink_base_transcript.inc` is the pre-T8.7b printer's
  own capture; renaming the label would move a pin that cannot be re-derived.
* **Compound identifiers are different identifiers** — the gtest suite names,
  the preset table's own `InteriorPointSolverPresetFields` /
  `kInteriorPointSolverPresets`, the test double `FakeAggregate`, the bench
  adapter `ModelAsNlpProblem`, and the C++ sense of "aggregate" in
  `core/detail/aggregate_arity.h` and `detail/interior/aggregate_views.h`.

### 1.6 The mechanical rewrite

Every rule in §§1.1–1.3 is an exact, case-sensitive, word-boundary
substitution, so the sed-equivalent of each row is

```sh
# one identifier or enumerator row (\b is the word boundary the manifest used)
sed -i -E 's/\bInteriorPointSolver\b/IpmSolver/g'  <files>
sed -i -E 's/\bLOQO\b/kLoqo/g'                     <files>   # inside BarrierModes' scope
# one path row: move the file, then rewrite every #include naming it
git mv include/hven/drivers/sqp_driver.h include/hven/drivers/sqp_solver.h
sed -i -E 's#\bhven/drivers/sqp_driver\.h#hven/drivers/sqp_solver.h#g' <files>
```

and the fold of §1.4 is

```sh
# on an SqpOptions value named <recv>
sed -i -E 's/\b(<recv>)\.start_level\b/\1.common.start_level/g' <files>
```

**Run these only against your own tree, and only over source.** Applying a
name rule to a frozen artifact, a provenance header or a historical note is
exactly what §1.5 forbids; and a bare global sed will hit the compound
identifiers §1.5 keeps. The sweep inside hven used explicit manifests with a
protect list and a residual checker for that reason.

---

## 2. The solver interface — one shape on both engines

The interior-point engine and the SQP engine now present ONE shape:
`SolveStatus`, an options value with `options()` / `set_options()`, a result
value deriving from `SolveResult`, a `SolveBudget`, one warm-start payload
protocol, one iteration callback, an attachable ledger and console, and
`common.threads`. **This whole section is a source break: rebuild against the
new headers. There is no deprecation shim for any of it.**

*Per-task entries: the guide's `## T8.2` … `## T8.9` and `## T8 group 1`.*

### 2.1 `SolveStatus` — one status vocabulary

`hven::ConvergenceFlags` and `hven::solvers::SqpStatus` are **gone**. Both
engines report `hven::solvers::SolveStatus`, whose nine values are
`kOptimal`, `kAcceptable`, `kMaxIter`, `kInfeasible`, `kStalled`, `kDiverging`,
`kNumericalError`, `kBudgetExhausted`, `kInterrupted`.

`SolveStatus` and `IpmStopReason`, with `to_string()` and `severity()`, live in
`<hven/core/solver_status.h>`. `<hven/drivers/solve_status.h>` is what is left:
`resolve_ipm_phase_status()`, a statement about one engine's exits.

| before (interior-point) | after |
|---|---|
| `CONVERGED` | `SolveStatus::kOptimal` |
| `ACCEPTABLE` | `SolveStatus::kAcceptable` |
| `NOTCONVERGED` | `SolveStatus::kMaxIter`, or `kStalled` at the stall and locally-infeasible-restoration exits |
| `DIVERGING` | `SolveStatus::kDiverging` |
| `SINGULAR_KKT` | `SolveStatus::kNumericalError` |
| `operator<=>(ConvergenceFlags, …)` | `severity(SolveStatus)` — orders all nine, old five in old relative order |

For the SQP, the five old enumerators exist on `SolveStatus` **under the same
names**, so the substitution is textual: `SqpStatus::kOptimal` →
`SolveStatus::kOptimal`, and so on.

> **The substitution is by NAME and never by ordinal.** `SolveStatus` runs
> `kOptimal, kAcceptable, kMaxIter, kInfeasible, kStalled, kDiverging,
> kNumericalError, kBudgetExhausted, kInterrupted`, so `kNumericalError` and
> `kBudgetExhausted` sit at ordinals 6 and 7, not 3 and 4. Anything that
> persisted, serialized or switched on the old enum's INTEGER values has to be
> re-derived by name.

**A `switch` over the old five needs a `default:` or the four new cases.**

**`to_string()` spells all nine in LOWER SNAKE**, where `to_string(SqpStatus)`
spelled its five capitalised. The printed SQP status line reads
`Status: optimal`. The corpus and crossover CSVs KEEP the capitalised
vocabulary through a bench-local table, and the machine trace does not move at
all.

**A caller no longer applies the stall split itself:** the interior-point engine
resolves each phase's verdict against that phase's own stop reason at the
phase's exit, so a stalled solve reports `kStalled` outright.
`resolve_ipm_phase_status` is idempotent and remains public for a caller
resolving a verdict it obtained some other way.

`hven::solvers::IpmStopReason` — `kNone`, `kIterationCap`,
`kRestorationLocallyInfeasible`, `kStageStalled`, `kInterrupted` — is readable
per phase on `r.phases[i].stop_reason`, and `IpmSolver::last_stop_reason()` is
KEPT (a callback has no result yet). **A consumer switching over that enum
without a `default` will need the `kInterrupted` arm**, which T8.6 added.

*Per-task entries: `## T8.2` (incl. both fix rounds), `## T8.4` "`hven::ConvergenceFlags` is gone".*

### 2.2 Options are values

**The setters are gone on both engines.** `IpmSolver::Settings` is now
`hven::solvers::IpmOptions` in the new public header
`hven/drivers/ipm_solver_types.h`: same 65 knobs, same declaration order, same
defaults, **trailing underscores dropped**. Two of them moved into the new
`hven::solvers::CommonOptions` (`hven/drivers/common_options.h`), which both
engines embed as a member called `common`:

| before | now |
|---|---|
| `Settings::qp_threads_` | `IpmOptions::common.threads` (same default, `HVEN_DEFAULT_QP_THREADS`) |
| `Settings::print_level_` | `IpmOptions::common.print_level` (same default, `0` = full output) |

Removed from `IpmSolver`: the 58 declared `set_*()` methods (the six
string-taking overloads included), the four static `strto_*()` parsers,
`apply_preset()`, `settings()` and the nested `Settings` struct.
Added: `const IpmOptions &options() const noexcept`, `void set_options(IpmOptions)`,
and `explicit IpmSolver(IpmOptions = {})`.

```cpp
// before
hven::solvers::IpmSolver solver;
solver.set_max_iters(200);
solver.set_print_level(10);
solver.set_tols(1e-6, 1e-6, 1e-6, 1e-6);
solver.apply_preset("filter_l1");

// now
auto o = hven::solvers::ipm_preset("filter_l1");   // start FROM the preset
o.max_iters = 200;
o.common.print_level = 10;
o.kkt_tol = o.econ_tol = o.icon_tol = o.bar_tol = 1e-6;
hven::solvers::IpmSolver solver(o);      // or solver.set_options(std::move(o));
```

Reading a setting: `solver.settings().max_iters_` becomes
`solver.options().max_iters`. There is no mutable accessor — a field is changed
by replacing the whole value. **The complete `set_*()` → field table, all 54
rows, is in the per-task entry** (`## T8.3`, "The setter → field table, in the
header's own order"); the mechanical rule is `set_foo(v)` → `o.foo = v`, with
`set_print_level` and `set_qp_threads` going to `o.common.print_level` and
`o.common.threads`, and the multi-argument setters expanding to one assignment
per field.

**The presets are free functions returning a full value.**
`IpmOptions ipm_preset(std::string_view)` applies its nine fields to a
DEFAULT-constructed value and returns it, rather than overlaying them on
whatever the solver already held — so knobs you want kept are written ON TOP of
the preset, not before it. `SqpOptions sqp_preset(std::string_view)` accepts
`"default"` and refuses anything else, listing the valid names.

**Validation is one free function**: `void validate(const IpmOptions &)` and
`void validate(const SqpOptions &)`.

**The eight mode enums moved out of the class** to namespace scope in
`hven/drivers/ipm_solver_types.h`, with a member alias left behind for each, so
`IpmSolver::BarrierModes::kLoqo` still compiles and still names the same type.
The four `strto_*()` parsers and the six string-taking setter overloads have no
replacement: name the enumerator.

**Four rules `set_options()` adds.**

1. **Transactional.** `validate(o)` runs first; a throw leaves the previous
   options in force and the solver usable. A sequence that passed through an
   invalid INTERMEDIATE state under the per-field setters is now refused as a
   whole. Build the value, then hand it over once.
2. **Between solves only.** A replacement attempted from inside an iteration
   callback throws `std::logic_error`; the options do not move and the guard
   clears on the unwind. The old "a setting written mid-call takes effect on
   the NEXT call" behaviour is REPLACED by the refusal.
3. **On the SQP, a replacement rebuilds the QP engine** — transactionally, with
   the ledger attachment and the solve counter carried over, and the lazily
   built SSN and IPQP tier engines dropped. **One rule, no fast path: a
   replacement with IDENTICAL options rebuilds too.**
4. **The constructor caps `common.threads` at this machine's core count**,
   exactly as the default constructor always did. A count written AFTER
   construction is the caller's explicit word and is taken verbatim.

**T8.3's twelve-field refusal is GONE.** T8.3 refused a change to the twelve
transcription-time backend fields while a program was attached. T8.4 removed
"attached" as a state (§2.4), so the change is ACCEPTED and marked, and the
next solve re-transcribes under the new value. Nothing is refused and nothing
is silently ignored. If you wrote the release/replace/re-attach recovery
sequence T8.3's entry describes, delete it.

**Hot-handle reuse is keyed on the producing engine's options.** `HotState`
carries an `engine_options_hash` from
`hven::solvers::options_fingerprint(const QpOptions &, int threads)`, and a
handle is adopted only when that stamp matches. After `SqpSolver::set_options()`:
identical options still adopt (kHot); any changed `qp` field or a changed
`common.threads` refuses — **kWarm by construction**, which is exact rather
than a degradation, because the refusal happens at the adoption gate.

**One ABI consequence, source-compatible and MANGLING-breaking.** Moving the
eight mode enums out of the class changed the mangled symbol of every function
whose signature mentions one — 27 declarations. Every spelling in your source
still compiles. **Rebuild; do not mix objects compiled against two different
hven header sets.**

*Per-task entry: `## T8.3`, and `## T8.4` "`set_options()`: the twelve-field refusal is gone".*

### 2.3 One result core: `SolveResult`, `SolveBudget`, the shared diagnostics

`#include <hven/drivers/solve_result.h>` — a public header that names neither
engine, so a consumer can write one reporting function against both.

| type | what it is |
|---|---|
| `hven::solvers::SolveBudget` | `{Index minor_budget = 0; Index max_iterations = 0;}` — the per-call work ceiling. Both zeros mean "the engine's own options decide". |
| `hven::solvers::SolveResult` | the base every engine result derives from: `status`, `x`, `lambda_e`, `lambda_i`, `z`, `f`, the four shared diagnostics, `ce`/`ci`, `iterations`, `wall_seconds`, `export_warm_start()`. |
| `hven::solvers::DeclaredDiagnostics` | the four shared diagnostics as a value. |
| `compute_declared_diagnostics(...)` | their ONE definition, over the DECLARED problem in CALLER units. |

**The four shared diagnostics**, over the declared problem, in the caller's
units, from an evaluation the engine ALREADY HOLDS — the function evaluates
nothing: `stationarity` (inf-norm of `grad f + Je^T lambda_e + Ji^T lambda_i - z`
over the declared coordinates that were measured), `feasibility_e` (inf-norm of
`ce`), `feasibility_i` (inf-norm of the positive part of the declared inequality
rows and of the declared bound violations), and `complementarity` (inf-norm over
`lambda_i o ci` and the canonical bound products).

> **NaN means UNMEASURED**, everywhere in this core, and `ce`/`ci` are EMPTY
> rather than zero when nothing was measured. Absent is never zero-filled. The
> four are reported only from an evaluation that is (a) objective-bearing,
> (b) at the returned point and (c) taken with restoration inactive; failing
> (a) makes `stationarity` alone NaN, failing (b) or (c) makes all four NaN and
> (c) additionally EMPTIES `ce` and `ci`.

**Reading an interior-point result.** `result()` and the diagnostic accessors
become fields on the returned value:

| before | after |
|---|---|
| `solver.result()` | the value `solve()` returns |
| `result().iter_num_` | `r.iterations` |
| `result().obj_val_` | `r.f` |
| `result().converge_flag_` | `r.status` (`SolveStatus`) |
| `result().primals_` | `r.x` |
| `result().eq_lmults_` / `.eq_cons_` | `r.lambda_e` / `r.ce` — **DECLARED rows only** |
| `result().iq_lmults_` / `.iq_cons_` | `r.lambda_i` / `r.ci` |
| `result().bound_lmults_` | `r.z` — **DECLARED width always** |
| `result().kkt_inf_` and the other three | `r.kkt_inf`, `r.barr_inf`, `r.econ_inf`, `r.icon_inf` |
| the six timings, the `last_*` eleven, the factor/SOC/watchdog counters | same names, trailing underscore dropped |
| `result().reset_accumulators()` | RETIRED — a solve builds a fresh result |
| `solver.kkt_analysis_count()` | `r.kkt_analyses_total` (plus `r.kkt_analyses_this_call`, new) |
| `solver.kkt_factor_counters()` | `r.kkt_factor_counters` |
| `solver.eval_error_log()` | `r.eval_error_log` |

**Two shapes changed, both toward the declared problem.** Under
`MakeConstraint`, the treatment's internal fixing rows used to sit in the tail
of the equality block; they are reported separately now as
`r.internal_fixed_lambda_e` / `r.internal_fixed_ce`. And `r.z` is
DECLARED-WIDTH always, where `bound_lmults_` was dense over the solver's reduced
space and empty on a problem with no finite bounds — an eliminated coordinate
reads `0`, and under `MakeConstraint` a fixed coordinate reads `-lambda_fix`.

**Reading an SQP result.** `SqpResult` derives from `SolveResult`. Three engine
measurements are renamed because the base now carries names with DIFFERENT
definitions:

| before | after | why |
|---|---|---|
| `sol.stationarity` | `sol.sqp_stationarity` | the base's `stationarity` has a different definition |
| `sol.feasibility` | `sol.sqp_feasibility` | the base has `feasibility_e` / `feasibility_i` |
| `sol.complementarity` | `sol.sqp_complementarity` | the base's has a different definition |
| `sol.wall_seconds` | `sol.solve_impl_seconds` | the base's `wall_seconds` is a DIFFERENT boundary |

> **Read this before repointing anything.** The first three still COMPILE if
> left alone, because `stationarity` and `complementarity` exist on the base —
> which is precisely why they are listed: **silently reading a different
> quantity is the failure mode.** A tolerance comparison must use the `sqp_*`
> ones; a comparison against another engine's solve of the same problem must
> use the shared ones. `kkt_residual` is unchanged and is still
> `max(sqp_stationarity, sqp_feasibility)`.

`solve_impl_seconds` keeps its OLD BOUNDARY exactly. The base's `wall_seconds`
is the whole public call — it starts at the public entry on every overload of
both engines — so it is never the smaller of the two.

**`SolveBudget` replaces the bare `Index minor_budget`.** An aggregate has no
converting constructor, so **every** call that passed a fourth argument
changes:

```cpp
sol = driver.solve(model, x0, warm, 20);                              // before
sol = solver.solve(model, x0, warm, SolveBudget{20});                 // after (a MINOR budget)
sol = solver.solve(model, x0, warm, SolveBudget{.minor_budget = 20}); // the same, spelled
```

**Positional order is `{minor_budget, max_iterations}`** — use the designated
form for anything but a bare minor budget. `budget.max_iterations`, when
non-zero, TIGHTENS ONLY: an effective cap of `min(max_iterations, opts.max_iter)`
on the SQP (restoration budgeted from that cap), and `min(budget, opts.max_iters)`
per PHASE on the interior-point engine, which IGNORES `minor_budget` and
documents that it has no minor loop. `SolveBudget{}` is the identity.

*Per-task entry: `## T8.4`, including "T8.4 fix round 1 — six contract changes a consumer can see".*

### 2.4 The program is an argument; the phase sequence is an option

**The five phase-named interior-point entries become one**, and the sequence
becomes an option:

| before | after |
|---|---|
| `Eigen::VectorXd optimize(x0)` | `IpmResult solve(model, x0)` with `opts.phases = {IpmPhase::kOptimize}` (the DEFAULT) |
| `Eigen::VectorXd solve(x0)` | `opts.phases = {IpmPhase::kSolve}` |
| `Eigen::VectorXd solve_optimize(x0)` | `opts.phases = {IpmPhase::kSolve, IpmPhase::kOptimize}` |
| `Eigen::VectorXd optimize_solve(x0)` | `opts.phases = {IpmPhase::kOptimize, IpmPhase::kSolve}` |
| `Eigen::VectorXd solve_optimize_solve(x0)` | `opts.phases = {IpmPhase::kSolve, IpmPhase::kOptimize, IpmPhase::kSolve}` |

**The rule, for any sequence:** phases run in order; a `kSolve` that FOLLOWS a
`kOptimize` runs only if that optimize phase did not report `kOptimal`; a
`kOptimize` is never conditional; a verdict of `kDiverging` or worse
short-circuits the rest. `validate()` refuses an EMPTY sequence.

**The program is borrowed for the call:**

| before | after |
|---|---|
| `IpmSolver(std::shared_ptr<NonLinearProgram>)` | REMOVED — construct over options, hand the program to `solve()` |
| `set_nlp(np)` | REMOVED — `solve(model, x0)` |
| `release()` | REMOVED — nothing is held to release |
| `kkt_pattern_is_analyzed()` | `kkt_pattern_is_analyzed(const NonLinearProgram &model)` |

The solver keeps NO pointer to the program between calls. **The cross-call reuse
you had from `set_nlp()`-once, `solve()`-many is unchanged in EFFECT** — the
symbolic analysis, the pattern hash and the partition setup all survive — but it
is keyed on the program's own identity now: its structure key, its structure
epoch, the fixed-variable treatment in force, and that the analysis was laid
against THIS program's tables. The identity token is a process-unique,
never-reused owner id (`NonLinearProgram::analyzed_owner_id()`), not an address.

**The per-phase account.** `r.phases` carries one
`IpmPhaseReport{phase, status, iterations, phase_seconds, stop_reason, ran}`
per REQUESTED phase, skipped ones included (`ran` tells them apart); `r.status`
is the last RAN phase's and `r.iterations` the sum over the phases that ran.
The verdict is reset at every phase start — previously it was per CALL, so a
later phase that left without assigning one reported the EARLIER phase's answer
beside this phase's reason.

**A `return_best` exit reports the BEST iterate's objective**: `IpmResult::f`
used to come from the last iterate while `x`, the multipliers and the residuals
came from the best one.

*Per-task entry: `## T8.4` "The interior-point engine: one entry, the model borrowed".*

### 2.5 Warm start: one payload protocol

**Staging is gone from both engines.** A warm start is an ARGUMENT to the solve
it applies to, and there are TWO of them with two different jobs: the shared,
serializable, identity-stamped payload `hven::solvers::WarmStartData`
(`warmstart/warm_start_data.h`), and the SQP's native
`hven::solvers::SqpWarmStart` (`warmstart/sqp_warm_start.h`, a new public
header), which claims no identity and can reach `kHot`.

| before | after |
|---|---|
| `sqp.stage_warm_start(p); sqp.solve(model, x0);` | `sqp.solve(model, x0, p);` |
| `sqp.stage_warm_start(p); sqp.solve(bridge, x0, budget);` | `sqp.solve(bridge, x0, p, budget);` |
| `ipm.stage_warm_start(p); ipm.solve(model, x0);` | `ipm.solve(model, x0, p);` |
| `ipm.clear_staged_warm_start();` | *(delete the line — a payload lives exactly as long as its call)* |
| `ipm.set_initial_multipliers(eq, iq); ipm.solve(model, x0);` | `ipm.solve(model, x0, seed);` — the multipliers-only seed, below |
| `ipm.clear_initial_multipliers();` | *(delete the line)* |
| `ipm.export_warm_start()` | `result.export_warm_start()` — a `std::optional<WarmStartData>` on the value `solve()` returned |
| `SqpSolver::export_warm_start()` | **unchanged** |

**Removed with no replacement:** the SQP's two-warm-sources refusal. A call
names exactly one warm-start source — its own argument.

**The multipliers-only seed.** A `WarmStartData` whose `primal_` is EMPTY is the
SEED FORM: it carries multipliers and nothing else, and **`x0` is the start**.
`primal_` and `bound_lmults_` are EITHER BOTH EMPTY OR ONE LENGTH;
`eq_lmults_` / `iq_lmults_` must always match the declared row counts. **On the
SQP it resolves `kSeeded`** — it did not before, where an empty `primal_` failed
the plausibility gate, the object resolved `kCold` and the multipliers were
silently DROPPED. **A polish extension on a seed is IGNORED and COUNTED**
(`SqpCounters::polish_ignored` / `IpmResult::polish_ignored`).

**The kIpm refusal — M5 ruling 4 is RETIRED for this case.** Under
`SqpOptions::qp_mode == QpMode::kIpm`, a payload whose block lengths or
declaration stamp did not match was COLD-GRADED — silently discarded, the solve
running cold — where `kWalk` and `kSsn` threw. **It now throws in every mode**,
with the same message. **What this can break for you:** a kIpm caller that
handed over a stale payload and relied on the solve running cold now gets an
exception. Catch it, or check the stamp before you hand it over. Pattern and
value defects still degrade in every mode; only identity refuses.

**`common.start_level` is a CEILING on the interior-point payload route**, never
a floor: `kCold` applies nothing (`payload_ignored == 1`), `kSeeded` the
multipliers only, `kWarm` the whole payload, and `kHot` is identical to `kWarm`
because that engine has no hot handle to adopt. **Identity is checked at EVERY
rung, `kCold` included.**

**The seeding constants moved** to `#include <hven/warmstart/seeding.h>`, with
their values deliberately NOT unified — `kSeededIqMultFloor` (`1e-8`),
`kSeededMultInitMax` (`1e6`), `kSeededDualClampTol` (`1e-6`). The class-scope
aliases for the first two are gone (§1.4).

**Overload resolution — spell the type.** The set is unambiguous for every call
that names its argument's type. The ONE ambiguous SPELLING is a BRACED third
argument, `solver.solve(bridge, x0, {})`, which is a compile error rather than a
silent precedence. Write `SolveBudget{}` (or the type you meant).

*Per-task entry: `## T8.5`, including its fix-round corrections.*

### 2.6 The iteration callback

Both engines take ONE per-iteration callback over one event type, and both can
be told to stop.

| before | after | notes |
|---|---|---|
| `ipm.set_late_callback(f)` | `ipm.set_iteration_callback(f)` | different signature |
| `ipm.disable_late_callback()` | `ipm.clear_iteration_callback()` | CLEARS rather than disarms |
| `ipm.set_early_callback(f)` | `ipm.set_kkt_hook(f)` | **same signature, same semantics** |
| `ipm.disable_early_callback()` | `ipm.clear_kkt_hook()` | now also clears the stored hook |
| `IpmSolver::EarlyCallBackType` | `IpmSolver::KktHook` | same `std::function` type |
| `IpmSolver::LateCallBackType` | *(deleted)* | `hven::solvers::IterationCallback` |
| *(nothing)* | `sqp.set_iteration_callback(f)` / `sqp.clear_iteration_callback()` | new on the SQP |

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

**Porting a late callback**: drop `return 0;` in favour of
`return CallbackAction::kContinue;` and it behaves exactly as before — the old
`int` return was ignored at both of its sites. The new one's return is READ.

**At `depth` 0** everything is in declared space and caller units, exactly as
`SolveResult` is, and the four diagnostics are the four SHARED ones —
**NaN means unmeasured, never zero**. **At `depth` 1 the space is the SQP
restoration sub-problem's, not the caller's**: a caller comparing events against
its own model filters on `depth == 0`. The four vector views alias the engine's
own storage and are valid **for the duration of the call only** — copy what you
need; copying the EVENT does not deepen them. **Neither engine evaluates
anything to build an event**, so attaching a callback cannot move an evaluation
counter.

**`hven::solvers::IterateInfo` leaves the CALLBACK surface** with
`LateCallBackType`. It does not leave the public surface: it stays the TRACE row
type in `drivers/trace.h`, handed out as `IpmIterTraceEvent::iterate`. A
consumer that wants the engine's own residual columns attaches a `TraceSink`
and reads that — one event per iterate, the same rows the callback saw.

**WHERE IT FIRES, and the one behaviour change.** On the SQP: from the single
site that emits a history row, one event per row. On the interior-point engine:
one event per `ipm.iter` row, at the **top** of the iteration that row belongs
to, after that iterate's residuals have been measured and BEFORE its KKT matrix
is factorized — where the late callback fired at the BOTTOM, below the
factorization and the line search. **The point it describes is the same point**
and the iterate index is the same number. What moves: the event now carries the
diagnostics of the COMMITTED point coherently, and anything a mid-iteration step
assigns (the barrier parameter's own update, the restoration entry's `mu`) is
visible on the NEXT event rather than on the one whose iteration performed it.

**`kStop`.** Both engines then report `SolveStatus::kInterrupted` — unreachable
before this window — with the ordinary cleanup and the ordinary trace end event.
On the SQP the stop is LATCHED and honoured at the next major's exit
conjunction, above `build_subproblem`: no QP is solved for an answer the caller
no longer wants. On the interior-point engine it is honoured where the event
fired, pre-factorization, so the solve returns the very point the callback was
shown, and **`IpmOptions::return_best` is not honoured on an interrupted exit**
for that reason. **A stop returned on the public call's own terminal row is a
no-op**, whatever that row's verdict; precedence at that exit is
`converged > interrupted > probe-exhausted > max_iter`, so **converged beats
stop**. A stop ends the interior-point PHASE SEQUENCE, not only the phase.

**Setting or clearing from inside a callback is safe** — the change DEFERS to
the statement after the invocation returns, so a callback may disarm itself and
go on touching its own captures. **From anywhere else while a solve runs —
which since T8.7b means a SINK METHOD — the change lands at the NEXT solve's
entry**, and the solve in progress is bitwise the solve it would have been. A
caller who relied on installing from a sink mid-solve installs from the callback
instead. **One slot per setter, and the last write wins.** A DIRECT call also
clears any pending deferral.

**A throwing callback** propagates out of `solve()` on both engines; the
abandoned solve leaves no end trace event and no ledger record, both solvers
remain usable, and the next solve is bit-for-bit what a fresh one would produce
over every deterministic result field.

**`set_kkt_hook` is IPM-only and stays that way** — it hands out the assembled,
not-yet-factorized KKT matrix of the interior-point Newton system, a structure
the SQP engine does not have. The interior-point engine's nested restoration
sub-solver receives NO callback; the SQP's does, through a forwarder that stamps
`depth`.

*Per-task entry: `## T8.6`, with the deferral rules amended in `## T8.7b` §5.*

### 2.7 The ledger and the console

**`attach_ledger` on the interior-point engine**, exactly as the SQP has it:

```cpp
hven::solvers::Ledger ledger;
solver.attach_ledger(&ledger, "ipm");
solver.solve(model, x0);
for (const hven::solvers::IpmSolveRecord &r : ledger.ipm_records()) { /* ... */ }
std::string table = ledger.ipm_summary_table();
```

One record per public `solve()` that RETURNS; a call that leaves by an exception
writes nothing and consumes no label number. Three deliberate differences from
the SQP's: no `"_qp"` forwarding, nothing re-forwarded on `set_options`, and the
nested feasibility restoration writes no record. **`factorizations` is a
PER-CALL delta** and is never negative; `analyses` is
`IpmResult::kkt_analyses_this_call`, honestly 0 on a second solve of the same
program. `total_time` and `wall_seconds` are informational and never asserted,
so `ipm_summary_table()` has **no timing column**.

**The console table is a `TraceSink` you can point anywhere.** New public header
`hven/drivers/console_trace_sink.h`:

```cpp
hven::solvers::ConsoleTraceSink console({/*wide=*/false, /*print_level=*/0}, stdout);
hven::solvers::FanOutTraceSink fan(&my_sink, &console);
```

`ConsoleTraceSink` renders BOTH engines' tables from the event stream;
`FanOutTraceSink` forwards every event to two sinks, first then second, either
half nullable. `ipm_residual_color(value, target, acceptable)` is the five-band
colouring that was the solver's private `calculate_color`, now shared.

**On the interior-point engine, nothing a caller sees changes**: the table is
byte-identical at the same print levels, and the solver attaches a
`ConsoleTraceSink` for itself when `common.print_level < 3`. **Attaching a
console never displaces your sink** — the two are fanned out, yours first.
**On the SQP this is a GAIN**: before the window the driver printed nothing at
any level; it now prints the same table at the same tiers. At `CommonOptions`'
shipped `print_level` of 3 — the SQP's effective default — **nothing is printed,
exactly as before.**

**Both engines' `attach_trace` now REFUSE a call made during a solve**, throwing
`std::logic_error`: the effective sink is composed at solve entry and fixed for
the solve. Between solves nothing changes, and attach order relative to
`set_options` is free.

**Deleted**: `IpmSolver::print_header()` (public, static) — `ConsoleTraceSink`
writes the rule; the private `print_banner()`, `print_stats()`,
`print_last_iterate()`, `print_timing_summary()`, `print_beginning()`,
`print_finished()`, `print_exit_stats()` and `calculate_color()`. There is no
`fmt::print` left in the interior-point solve path; `ConsoleTraceSink` writes
the whole transcript.

**Two consequences for a sink author.** `TraceSink` gained six non-pure
virtuals with empty defaults — `on_ipm_restoration_exit_row` plus T8.7b's five
(`ipm.phase.begin`/`.end`, `ipm.kkt_analysis`, `ipm.phase.exit`, `ipm.message`)
— so a sink written before the window keeps compiling and simply ignores them.
And `hven/drivers/sqp_solver.h` no longer includes
`hven/drivers/console_trace_sink.h`: **a TU that used to reach
`ConsoleTraceSink` or `fmt/color.h` THROUGH `sqp_solver.h` must include
`hven/drivers/console_trace_sink.h` itself.**

*Per-task entries: `## T8.7` and `## T8.7b`. The trace schema's own additions are §4.*

### 2.8 `common.threads`

**Nothing a caller wrote has to change, and nothing a caller ran changes.** The
SQP default is `0` and always has been, and `0` means "leave the backend's own
default alone" — no thread scope is engaged anywhere, so a defaulted solve is
bit-for-bit the pre-window solve. This is for the caller who sets
`common.threads` to something else, which the SQP engine previously accepted,
fingerprinted, and then ignored.

The count now reaches **eight** factor construction sites — every one the SQP
has, the dense Schur border factor included — plus every one inside a nested
restoration solve. **It is applied at CALL scope and undone on every exit**: a
stack-local RAII scope around the backend call that SAVES AND RESTORES the
thread-local override it replaced, never a reset to zero. A caller who runs hven
at 2 threads on a thread it had itself pinned to 3 still has 3 afterward.
Nothing writes a process global or an environment variable.

New accessors report what each layer actually holds:
`SqpSolver::ssn_tier_num_threads()` and `ipqp_tier_num_threads()` (`-1` when
that lazy tier has not been built yet), `QpEngine::num_threads()` and
`carried_num_threads()`, `SsnEngine::num_threads()`,
`IpqpEngine::num_threads()`, `KktFactorization::session_num_threads()`,
`DenseSymmetricFactor::num_threads()` / `set_num_threads(int)`, and
`SchurComplement::num_threads()`. **`QpEngine::num_threads()` changed meaning**:
it returns the LIVE K0 factor's count now, where it returned the carried int
before; the carried value is available under its own name.

**A negative `common.threads` is refused by `validate` before any engine is
built**, in both constructors and in `set_options()`, with the message naming
the option you set.

**On Apple this is stored and applied to nothing — UNOBSERVED.** The Accelerate
sparse session records `num_threads` and hands it to no backend call, and the
dense path's LAPACK is Accelerate's own. The SQP deliberately does NOT mirror
the interior-point solver's process-wide `accelerate_set_num_threads()`, which
is never restored. No Apple value is estimated, interpolated or zero-filled
anywhere.

*Per-task entry: `## T8.8`.*

### 2.9 `NLPSolver` is retired

**If you constructed `hven::solvers::NLPSolver`, your code no longer compiles.**
The wrapper, its header `hven/model/nlp_solver.h` and its TU are deleted, and so
is `hven/detail/interior/jet.h` (`Jet`, `MklLocalPinGuard`). Every
responsibility it held has a named replacement:

| the wrapper did | its replacement |
|---|---|
| lazy triplet-model transcription | **`make_nlp_program(problem, num_partitions = 1)`** — `detail/model/nlp_adapter.h` |
| starting-multiplier staging from `NlpTripletModel::starting_multipliers()` | `NlpProblemModel::split_user_multipliers` into a multipliers-only `WarmStartData`, handed to `solve()` |
| `return_multipliers()` | `NlpProblemModel::compose_user_multipliers(result.lambda_e, result.lambda_i)` |
| `return_x()` | `IpmResult::x` — bitwise the same vector |
| `result()`, `last_result_` | the `IpmResult` **`solve()` returns** |
| the five jet modes and `strto_jet_job_mode` | `IpmOptions::phases` (§2.4) |
| `jet_initialize()` / `jet_release()` / `jet_run()` | `ipm_worker_options(IpmOptions)` plus the program's `negotiate_partition_count(1)` |
| `num_partitions_`, `default_num_partitions()`, `set_num_partitions()`, `init_partitions()` | the **program's** count: `make_nlp_program(problem, N)` or `program.negotiate_partition_count(N)` |
| `run_nlp_solver(mode, x0[, seed])` | `solver.solve(program, x0)` / `solver.solve(program, x0, payload)` |
| `NlpSolveOutput` | `IpmResult` |

```cpp
// BEFORE
hven::solvers::NLPSolver solver(problem);
auto o = solver.optimizer_->options();
o.max_iters = 200;
solver.optimizer_->set_options(std::move(o));
const auto flag = solver.optimize(x0);
const Eigen::VectorXd x = solver.return_x();

// AFTER
const auto program = hven::solvers::make_nlp_program(problem);
hven::solvers::IpmSolver solver;
auto o = solver.options();
o.max_iters = 200;
o.phases = {hven::solvers::IpmPhase::kOptimize};   // the default; write it for another sequence
solver.set_options(std::move(o));
const hven::solvers::IpmResult result = solver.solve(*program, x0);
const hven::solvers::SolveStatus flag = result.status;
const Eigen::VectorXd &x = result.x;
```

The multipliers in the PROBLEM's own row space need the model, so build it and
use the core-taking overload:

```cpp
const auto model = std::make_shared<hven::solvers::NlpProblemModel>(problem);
const auto program2 = hven::solvers::make_nlp_program(
    std::make_shared<hven::solvers::NLPAdapterCore>(model, problem->name()));
const Eigen::VectorXd lam =
    model->compose_user_multipliers(result.lambda_e, result.lambda_i);
```

**The problem's own multiplier seed, spelled out** — what `NLPSolver::run()`
did on every solve, now the caller's six lines:

```cpp
Eigen::VectorXd lam = Eigen::VectorXd::Zero(model->num_declared_rows());
std::optional<hven::solvers::WarmStartData> seed;
if (problem->starting_multipliers(lam)) {
    Eigen::VectorXd eqm, iqm;
    model->split_user_multipliers(lam, eqm, iqm);
    hven::solvers::WarmStartData s;                  // primal_ stays EMPTY
    s.eq_lmults_ = std::move(eqm);
    s.iq_lmults_ = std::move(iqm);
    s.structure_key_ = hven::solvers::declaration_key(program->declaration());
    seed = std::move(s);
}
const auto r = seed ? solver.solve(*program, x0, *seed) : solver.solve(*program, x0);
```

**`ipm_worker_options`** writes `common.threads = 1` and
`common.print_level = 10` and nothing else — the whole of `jet_initialize()`
apart from the partition count, which is the PROGRAM's. Two things the old
worker context had that it does not: `MklLocalPinGuard` pinned the whole worker
THREAD's MKL for the job's lifetime, where `common.threads = 1` pins only
hven's own bracketed backend calls (same outcome for this engine's
factorizations, a narrower promise about the thread); and the evaluation pool
(`hven::utils::set_num_threads`) is process-global and was never touched by
`jet_initialize()` — it is not touched here either.

**`DoNothing` and `NotSet` parsed and then dispatched to nothing.** Their
successor is the EMPTY sequence, which `validate()` REFUSES. The no-argument
entry points are gone with the iterate the wrapper kept between calls: every
solve names its own start point.

**Partitions through the adapter are LAYOUT ONLY.** `make_nlp_program(problem, N)`
reaches the layout, where the wrapper's `num_partitions_` reached nothing — but
it lays partitions, it does not parallelise the evaluation: all three adapter
pieces are `ThreadingFlags::MainThread`, so `N` means N−1 empty partitions plus
the whole problem evaluated serially. **The count is CLAMPED, not refused** (at
one partition per 1000 KKT elements), so **read the adopted count off the
returned program**, never off your request. A non-positive request is refused by
name.

*Per-task entry: `## T8.9`.*

### 2.10 The fifteen declared behaviour changes

Everything outside this list is identity. With the section that describes each:

| # | behaviour change | here |
|---|---|---|
| 1 | the SQP's identity-mismatch refusal in every mode | §2.5 |
| 2 | `kInterrupted` on both engines | §2.6 |
| 3 | the interior-point continuing callback dispatch observes the committed point | §2.6 |
| 4 | the SQP thread count when non-zero | §2.8 |
| 5 | a multipliers-only seed resolves `kSeeded` on the SQP | §2.5 |
| 6 | `kStalled` split out of NOTCONVERGED | §2.1 |
| 7 | the shared diagnostics on every result | §2.3 |
| 8 | `set_options` rebuilds the SQP engines, with fingerprinted hot reuse | §2.2 |
| 9 | the restoration-exit row as an event | §2.6, §4 |
| 10 | the common `wall_seconds` boundary | §2.3 |
| 11 | SQP console output | §2.7 |
| 12 | `kkt_pattern_is_analyzed` takes the model | §2.4 |
| 13 | the polish extension ignored on a multipliers-only payload | §2.5 |
| 14 | restoration budgeted from the effective cap | §2.3 |
| 15 | an option change during a solve is REFUSED rather than deferred | §2.2 |

*Per-task entry: `## T8 group 1`, "The fourteen behaviour changes named in the design's §2.7" plus (15).*

---

## 3. The model layer

### 3.1 `OptimizationProblemBase` is folded away

**`include/hven/drivers/optimization_problem_base.h` IS DELETED** and
`hven::solvers::OptimizationProblemBase` no longer exists. Everything it
declared moved into `NLPSolver` with the same names, the same bodies and the
same initialisers — and `NLPSolver` itself was then retired by §2.9, so on a
tree that crosses the whole window the destination of this fold is the engine
and the program, not the wrapper.

Two nested types were RE-SCOPED on the way (`JetJobModes`, `NlpSolveOutput`),
and the class became `final` and NOT polymorphic — no virtual functions, no
virtual destructor, no vtable.

**The breaking case is a class that DERIVED from the base** to get `optimizer_`,
`nlp_` and `run_nlp_solver()` — plus, silently, the base constructor's two
defaults. The replacement owns its optimizer and applies those defaults
explicitly, because the base's constructor did exactly two things:

```cpp
this->optimizer_ = std::make_shared<IpmSolver>();
this->init_partitions();   // num_partitions_ = default_num_partitions();
                           // optimizer_->set_qp_threads(
                           //     std::min(HVEN_DEFAULT_QP_THREADS,
                           //              utils::get_core_count()));
```

`HVEN_DEFAULT_QP_THREADS` is still a PUBLIC compile definition carried on the
`hven::hven` usage requirement, and `hven::utils::get_core_count()` is still
`hven/detail/interior/utils/get_core_count.h`. **Dropping `override` is
required, not cosmetic**: with no base there is nothing to override, and
`override` on a non-overriding member is an error. The per-task entry carries
the full before/after of a real derived class.

`std::shared_ptr<OptimizationProblemBase>` and `OptimizationProblemBase *` have
**no replacement**, because there is no base and nothing that succeeded it has a
virtual destructor. Hold the concrete type.

**One include guarantee narrowed, and it is the one that bites first.** The
public header stopped GUARANTEEING `<algorithm>`, `<fmt/color.h>`,
`<fmt/core.h>`, `<fmt/format.h>`, `<stdexcept>`, `thread_pool.h` and
`get_core_count.h`. They were still reachable transitively when the fold landed,
so nothing broke that day — what changed is the guarantee. **Do not rely on them
transitively; a TU that uses any of them should include it itself.**
`<stdexcept>` is the one to check: catching the `std::invalid_argument` these
entry points throw is the normal use of this surface.

*Per-task entry: `## T1`.*

### 3.2 The six by-value `NlpModel` evaluations are `[[deprecated]]`

Nothing is removed and no signature moves: this is **source-preserving and
ABI-preserving**. It is nonetheless a **declared WARNING-POLICY build break** —
a TU that CALLS one of the six through an `NlpModel` reference now emits
`-Wdeprecated-declarations`, so a consumer building with `-Werror` (or
`-Werror=deprecated-declarations`) **fails to compile until it brackets or
migrates that call.**

| deprecated | replacement |
|---|---|
| `Vec eval_grad(const Vec &x) const` | `void eval_grad_in_place(const Vec &x, Vec &out) const` |
| `Vec eval_ce(const Vec &x) const` | `void eval_ce_in_place(const Vec &x, Vec &out) const` |
| `Vec eval_ci(const Vec &x) const` | `void eval_ci_in_place(const Vec &x, Vec &out) const` |
| `SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &le, const Vec &li) const` | `void eval_hess_in_place(const Vec &x, double obj_scale, const Vec &le, const Vec &li, SpMatRM &out) const` |
| `SpMatRM eval_jac_e(const Vec &x) const` | `void eval_jac_e_in_place(const Vec &x, SpMatRM &out) const` |
| `SpMatRM eval_jac_i(const Vec &x) const` | `void eval_jac_i_in_place(const Vec &x, SpMatRM &out) const` |

**`eval_f` is NOT deprecated** — it returns a `double` and there is no vector
allocation to remove. Neither is `eval_values`. Six, not seven.

**REMOVAL IS NOT PROMISED.** The by-value forms stay pure virtual and required.
A later removal would be its own declared break, decided on its own evidence.

**OVERRIDING a deprecated virtual does not warn**, so a model that implements
these needs **no change at all**, and callers that hold the model by its own
concrete type are silent too. What warns is a CALL that resolves to
`NlpModel`'s declaration — the shape a driver, a wrapper, or a generic helper
taking `const NlpModel &` has.

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
  destination cannot receive an in-place call; fill a local and assign, which is
  the same one temporary the by-value call already made.

**If you WANT to keep calling the by-value form** — mid-migration, or because
the by-value evaluation is an independent ORACLE of the in-place path — bracket
it with the same pair `nlp_model.h` uses around its own in-place defaults:

```cpp
#include <hven/core/compiler.h>

HVEN_SUPPRESS_DEPRECATED_BEGIN
// by-value on purpose: <the reason>
const hven::Vec g = model.eval_grad(x);
HVEN_SUPPRESS_DEPRECATED_END
```

`hven/core/compiler.h` is a new public header. The pair is a scoped push/pop
with clang, gcc and MSVC branches; **the MSVC branch is UNOBSERVED.**

*Per-task entry: `## T3`, whose tycho table shows how a real tree's sites split
between warn and silent by static type.*

### 3.3 The interior-point early callback's three vectors are read-only

The KKT hook's three VECTOR arguments — XSL, PGX and RHS — are now
`hven::ConstEigenRef<Eigen::VectorXd>`, i.e.
`const Eigen::Ref<const Eigen::VectorXd> &`. **The KKT matrix argument is
unchanged and still mutable**: writing new values into the entries the matrix
already carries is a use this hook exists for, it is documented, and it is
pinned.

```diff
 solver.set_kkt_hook(
-    [&](int iter, double obj_scale, Eigen::Ref<Eigen::VectorXd> xsl, double prim_obj,
-        Eigen::Ref<Eigen::VectorXd> pgx, Eigen::Ref<Eigen::VectorXd> rhs,
+    [&](int iter, double obj_scale, hven::ConstEigenRef<Eigen::VectorXd> xsl, double prim_obj,
+        hven::ConstEigenRef<Eigen::VectorXd> pgx, hven::ConstEigenRef<Eigen::VectorXd> rhs,
         Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt) -> int {
         // reads unchanged; kkt stays writable
         return 0;
     });
```

A lambda that took its vectors by non-const `Eigen::Ref` does not merely warn —
`std::function` refuses to store it — **so this is a compile error found at
every site, not a silent behaviour change.**

**The break, stated plainly.** Callers lose the ability to WRITE those three.
That was never a documented mutation point, but it is not true that a write
would have been discarded: PGX is read six lines later into the Newton
right-hand side, RHS's constraint blocks ARE the right-hand side the step is
computed from, and XSL is the live iterate. **A consumer that relied on writing
one of them changes BEHAVIOUR, not merely compilation**; there is no in-flight
replacement — re-declare the problem and solve again.

What the three views ARE is now pinned rather than described: at iteration i the
XSL primal block is the x the model was evaluated at, PGX is `obj_scale` times
the gradient the model returned, and the RHS constraint blocks are the residuals
it returned less the row bounds — exactly, as the same doubles.

`hven::solvers::ConstKKTVector` (`detail/interior/kkt_vector.h`) is new: the
read-only twin of `KKTVector`, same four-block layout, const accessors only,
implicitly convertible FROM `KKTVector`. **`KKTVector` itself is unchanged**,
and so is every signature that names it — the twin is a second non-template
class precisely so that no `KKTVector` consumer's mangled name moves.

*Per-task entry: `## T2`. (The setter's own rename — `set_early_callback` →
`set_kkt_hook` — is §2.6.)*

---

## 4. The trace

**The schema header moved out of `detail/`:** `hven/detail/qp/ipqp_trace.h` →
**`hven/drivers/trace.h`**. It was always a public surface in fact.

**`hven::solvers::IpqpTraceSink` → `hven::solvers::TraceSink`.** Same virtuals,
same signatures, same pure/non-pure split. **No alias for the old name is
kept**: the rename IS the break, so a consumer that still names the old one gets
a compile error rather than a deprecation it can ignore.

```diff
-#include <hven/detail/qp/ipqp_trace.h>
+#include <hven/drivers/trace.h>

-class MySink : public hven::solvers::IpqpTraceSink {
+class MySink : public hven::solvers::TraceSink {
     void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &e) override;
     // ... the other pure virtuals; the non-pure defaults are optional
 };

-void attach(hven::solvers::SqpSolver &d, hven::solvers::IpqpTraceSink *s) {
+void attach(hven::solvers::SqpSolver &d, hven::solvers::TraceSink *s) {
     d.attach_trace(s);
 }
```

**The `Ipqp*` EVENT names are UNCHANGED** — `IpqpTraceIterEvent` and its
siblings, the whole-solve `Sqp*` and `Ipm*` events, every enum,
`VariableBoundCensus` and `census_variable_bounds` all keep their spelling and
all move with the header. Only the SINK was renamed. **The rename sweep of §1
did not touch them either**, and **`docs/trace-schema-v0.md`'s record names are
unchanged.**

**The two escape-evidence blocks split out of the engine header**:
`IpqpStallEvidence` and `IpqpInfeasibilityEvidence` now live in
`hven/detail/qp/ipqp_evidence.h`, definitions unchanged. `ipqp_engine.h`
includes it, so **a TU that already included `ipqp_engine.h` needs no change**;
what the split buys is that `drivers/trace.h` no longer includes the engine.

| you want | include |
|---|---|
| the sink to derive from, and the event structs | `hven/drivers/trace.h` |
| the JSON-lines writer (`JsonLinesTraceSink`) | `hven/drivers/trace_writer.h` — it includes `trace.h` for you |
| the console sink / fan-out | `hven/drivers/console_trace_sink.h` (§2.7) |
| `attach_trace` on `SqpSolver` / `IpmSolver` / `IpqpEngine` | nothing new |

`JsonLinesTraceSink`'s own surface is untouched: same constructor, same
`failed()`, `lines_written()`, `depth()` and `reset_nesting()`, same bytes on
the wire.

**What the STREAM gained** (all additive; no key moved, and no golden line that
predates each addition moves by one byte): eight keys on `ipm.solve.begin`, five
on `sqp.solve.end`, one key at the end of `sqp.solve.end`'s counters object
(`polish_ignored`), the new `ipm.restoration_exit_row` line, and T8.7b's five
new events (`ipm.phase.begin`/`.end`, `ipm.kkt_analysis`, `ipm.phase.exit`,
`ipm.message`). The interior-point `status` field spells `SolveStatus` now
(`converged` → `optimal`, `not_converged` → `max_iter` or `stalled`,
`singular_kkt` → `numerical_error`).

**Two counting rules that are easy to get wrong**: `ipm.kkt_analysis` is NOT one
per phase (the identity is `1 + #{phases that ran, were not the last step, and
did not break}`), and several `ipm.message` lines per iteration are normal. The
whole-stream arithmetic is `lines = 2 + 3P + A + R + M + D`. A consumer counting
`ipm.iter` lines on a solve that ends
`IpmStopReason::kRestorationLocallyInfeasible` sees ONE MORE line than before —
that exit now emits the row it returns.

*Per-task entries: `## T4`, plus the schema deltas in `## T8.5`, `## T8.7` §4
and `## T8.7b` §§2–3, §8. `docs/trace-schema-v0.md` is the document of record.*

---

## 5. What did NOT change for a consumer

**The SQP driver's kernels TU (T6): proposed, measured, and abandoned.** A cut
that moved six definitions out of the driver TU into a new
`src/drivers/sqp_kernels.cpp` landed on `m6` and was **reverted** — an
experiment with a veto from the day it was designed. `run_elastic_ladder` and
`certified_feasibility_fallback` are where they always were, declared in the
SQP engine's own header with the same signatures and the same behaviour;
`hven::solvers::detail::trace_outcome_of` DOES NOT EXIST, and
`src/drivers/sqp_kernels.cpp` and `sqp_kernels_internal.h` do not exist. The
ONE symbol-table change from it that survives is that two internal hand-off
guards keep INTERNAL linkage — they had external linkage and no declaration
anywhere, an accident rather than a surface, and nothing could legally have
named either. This entry exists because a consumer who saw the intermediate
commits, or a symbol table taken from one of them, should be able to find out
what they were looking at. *Per-task entry: `## T6`.*

**The header comment cleanup (T7): nothing changed for you.** No API break, no
behaviour change, no symbol change: ten headers had their comments rewritten —
operative contracts stay beside their declarations in terse Doxygen, and the
discussion prose moved, verbatim and stamped with its source commit, path and
original line numbers, to `docs/notes/2026-09-header-prose-archive.md`. If you
are looking for exactly what a comment used to say, the authority is
`git diff 1997159 -- <path>`, which is immutable; the archive is where the
*reasoning* went. One non-comment change rode it: thirteen dead `friend`
declarations naming test harnesses and gtest classes that no longer exist were
removed, and a friend declaration emits nothing. *Per-task entry: `## T7`.*

---

## 6. The runtime declaration

**Three sentences, then the numbers, each with its pointer.** The window's
restructure and its interface unification were measured once each under
CLAUDE.md §7's serial rule, and **the owner ruled KEEP on both.** On the SQP
corpus leg the interface work is FLAT in all three modes, with the counters
byte-identical between the arms; on the top-level interior-point leg it is not
flat, and its carrier is unidentified after six legs. **Nothing here asks a
consumer to do anything** — it is stated so that a consumer measuring hven
across this window knows what has already been measured, and how.

| reading | figure | what it is |
|---|---|---|
| the driver restructure (T6), post-T3 → the revert head | ipm **1.0023**, ssn **1.0015**, walk **0.9993** | inside the ±0.5 % corpus bar in every mode, FLAT per cell, 0/27 outside 0.99–1.01; no owner ruling needed |
| the interface unification (group 1), post-T7 → the group-1 head, SQP corpus leg | ipm **1.00062**, ssn **0.99924**, walk **1.00014** | **FLAT**; 0/27 cells outside 0.99–1.01 in every mode; counters byte-identical, 27 × 75 × 3 × 3 |
| the same, instructions | **+0.03…0.13 %** | ONE quantity, attributed entirely to the shared declared diagnostics computed once per call — the cost of the shared result core (§2.3) |
| the same, the top-level interior-point leg IN THE LEG PROCESS | **1.0249** | 29/29 banded rows outside the band, 28 slower in all three rounds — **MOVED**, carrier UNIDENTIFIED; one commit's, inside the result-core task |
| the carrier pair in a SINGLE-ROW process, on the solve bracket | **+1.10 %**, informational | at instruction counts flat within the instrument; the whole process unchanged at +0.04 % — a bracket boundary move |

**The owner's KEEP (2026-09-11, restated and confirmed on the corrected
reading)** rests on those grounds: one solve per process is flat on the SQP; the
instruction increase is the declared per-call diagnostics the shared result core
was for; and the top-level interior-point step is one commit's, whose carrier
six legs refuted rather than found. A many-solves-per-process leg on both
engines is REGISTERED for M7.

**The field fold of §1.4 is a LAYOUT change, and is declared as one.**
Deleting `SqpOptions::start_level` shrinks the struct and moves every field
after it, so it is not instruction-neutral by construction and no proof
technique could make it so. It changes no behaviour, no default and no
semantics; it is named here because the rename sweep is otherwise a mapped
rename and this one thing in it is not.

**What is NOT measured, said plainly.** The cost of an ATTACHED iteration
callback or an ATTACHED trace sink is **UNMEASURED** — the bench harness has no
callback lever, and the two library call sites that run only with something
attached were executed zero times. **Apple/Accelerate, Windows and the Intel
pass-B events are UNOBSERVED.** Every wall-clock figure above is informational
under CLAUDE.md §7; the asserted currency is counters.

*Per-task entries: `## T6` "Why it was abandoned", `## T8 group 1` "T8.9r — the
runtime reading", and `## T8.10` §6 for the fold. The T8.9r evidence artifact is
`docs/notes/data/2026-09-m6-w5-t8-runtime/` — read its `PROVENANCE.txt` first.*

---

## Where the authority lives

| you want | read |
|---|---|
| the reasoning behind any line above | `docs/notes/2026-09-m6-w5-migration-guide.md`, the entry this guide points at |
| what closed each task, and on what evidence | `docs/notes/2026-08-m6-ledger.md`, the W5 section |
| the trace schema | `docs/trace-schema-v0.md` |
| the W5 acceptance evidence | `docs/notes/data/2026-09-m6-w5-acceptance/` |
| the T8 runtime protocol's own artifact | `docs/notes/data/2026-09-m6-w5-t8-runtime/` |
| the prose lifted out of the headers at T7 | `docs/notes/2026-09-header-prose-archive.md` |
