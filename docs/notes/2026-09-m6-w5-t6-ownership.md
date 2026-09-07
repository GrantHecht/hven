# M6 W5 T6.0 — the ownership table for `SqpDriver::solve_impl_body`

**BASE `50f616a`** (the W5 T3 ledger commit; code head `2c4b59b`). Every line number below was
re-derived at BASE by reading `src/drivers/sqp_driver.cpp` — T3 moved the file's evaluation call
sites, so no number is carried over from the W5 inventory
(`.superpowers/w5-inventory/driver-locations.tsv`, pinned parent `d0c7aa0`), from the SQP-lane
pre-read (`.superpowers/w5-t6-brief-tycho-addendum.md`, read at `154d20a`) or from the astra
plan-level review (`.superpowers/w5-t6-plan-review-astra.md`, read at `154d20a`). Where a carried
number and the BASE number differ, §12 says so.

`solve_impl_body` is `src/drivers/sqp_driver.cpp:2025-4732` — 2708 lines, ONE call site
(`src/drivers/sqp_driver.cpp:2017`, inside `solve_impl`). The loop begins at
`src/drivers/sqp_driver.cpp:2680`; `prepare_solve` (cut (a)) is therefore exactly
`src/drivers/sqp_driver.cpp:2030-2678`.

This document is the DESIGN of cuts (a)–(d). Nothing in T6.a–T6.d may contradict it without an
amendment written here first.

Two sentences carried verbatim from the plan (`docs/notes/2026-09-m6-w5-plan.md` §1 W5.T6), at the
SQP lane's instruction (`.superpowers/w5-t6-brief-tycho-addendum.md` §5), because they are the
reason cut (d)'s proof is re-run rather than cited and the reason its veto is not negotiable:

> the earlier carve's preserved-inlining record is history, not proof

> "informational" does not waive neutrality

---

## §1. The TU's symbol inventory

`src/drivers/sqp_driver.cpp` is 5133 lines in ONE namespace, `hven::solvers`
(`src/drivers/sqp_driver.cpp:35`, closing at `:5133`), with **THREE** anonymous-namespace blocks —
not two, as the brief's §0 said (astra G6; confirmed at BASE):

| block | extent | closes |
|---|---|---|
| anon #1 | `src/drivers/sqp_driver.cpp:37` | `:320` |
| anon #2 | `src/drivers/sqp_driver.cpp:849` | `:985` |
| anon #3 | `src/drivers/sqp_driver.cpp:1242` | `:1350` |

There is also one named inner namespace, `hven::solvers::detail`
(`src/drivers/sqp_driver.cpp:472-546`), holding one function.

### §1.1 Anonymous namespace #1 — `:37-320` (internal linkage)

| symbol | line | called from |
|---|---|---|
| `borrow_model` | `:52` | `:1532`, `:1542` |
| `require_declared_box` | `:72` | `:1531`, `:1541`, `:1986` |
| `require_consistent_core` | `:91` | `:1687` |
| `require_finite_core` | `:105` | `:1688`, `:1757` |
| `validate_staged_polish` | `:131` | `:1689` |
| `record_terminal_kkt` | `:223` | `:231`, **`:2828`**, `:5042`, `:5080`, `:5100`, `:5103` |
| `record_scaling_report` | `:238` | **`:2821`**, `:5065` |
| `drop_scaled_space_state` | `:265` | **`:2859`**, `:5126`, `:5127` |
| `struct CallerScaleRowMeasures` | `:291` | `:296`, `:298`, **`:2714`** |
| `caller_scale_row_measures` | `:296` | **`:2728`** |

Bold = reached from inside `solve_impl_body`. All ten are DRIVER-side; none is used by the elastic
ladder or the certified fallback. Under cut (d) this block does not move.

### §1.2 Anonymous namespace #2 — `:849-985` (internal linkage)

| symbol | line | called from | side |
|---|---|---|---|
| `elastic_initial_rho` | `:854` | `:1014` | kernels only |
| `elastic_evidence_seed` | `:880` | `:1021` | kernels only |
| `trace_outcome_of` | `:963` | `:1050`, `:1169`, `:1212`, **`:4224`**, **`:4532`** | **SHARED** |
| `emit_qp_mode_line` | `:972` | `:1050`, `:1169`, `:1212` | kernels only |

**CORRECTED at the (d) design review (astra Minor; §12 item 21).** `elastic_initial_rho`'s row as
first written listed `:893` as a call site. `:893` is a COMMENT — "which with `elastic_initial_rho`'s
own floor makes a non-fired arm W1's ladder exactly", inside `elastic_evidence_seed`'s body — and a
prose mention is not a call. The helper has exactly ONE call site, `:1014` inside
`run_elastic_ladder`. The count matters because cut (d)'s claim is built from this table.

`trace_outcome_of` is the ONE symbol used by both sides of cut (d) — three call sites inside
`run_elastic_ladder`/`certified_feasibility_fallback` and two inside `solve_impl_body`. It is the
symbol the plan means when it says the block "would gain external linkage, a declared surface". The
other three are kernels-side only and stay internal to the new TU.

**ITS DESTINATION IS DECIDED (astra item 2, binding).** `trace_outcome_of` becomes
`hven::solvers::detail::trace_outcome_of(QpStatus)`, DECLARED in a NEW **source-private** header
`src/drivers/sqp_kernels_internal.h` and DEFINED in `src/drivers/sqp_kernels.cpp`; the five call
sites are qualified. The declaration does NOT go in `include/hven/drivers/sqp_driver.h`, and it does
NOT go under `include/hven/detail/` either: `include/hven/**/*.h` is installed WHOLESALE by
`CMakeLists.txt:613`, `detail/` included, so a header under `include/` is a shipped file whatever
its directory says. A header under `src/` is not installed at all, which removes the ambiguity
rather than arguing about it — and the install smoke proves it at the landing commit.

The migration-guide entry says: an anonymous-namespace helper became
`hven::solvers::detail::trace_outcome_of`, declared source-privately, with external linkage SOLELY
because this internal TU boundary needs it; there is no supported consumer migration and no new
public API.

**THE REDRAW ALTERNATIVE IS RECORDED, not adopted** (astra item 2): a private INLINE definition of
the mapping in that same source-private header, visible to both TUs, which emits no external symbol
and preserves optimisation through the mapping. It is the FIRST targeted redraw if the mapper's
calls cause the veto. Adopting it up front would require amending the moving/linkage claim, so the
experiment under review is the external-declaration form and the inline form is the named remedy.

### §1.3 Anonymous namespace #3 — `:1242-1350` (internal linkage)

| symbol | line | called from | side |
|---|---|---|---|
| `trace_mode_of` | `:1249` | `:1286`, **`:2702`** | driver only |
| `make_solve_begin_event` | `:1271` | `:2015` (`solve_impl`) | driver only |
| `build_ipqp_staged_seed` | `:1294` | `:1807` (`consume_staged_warm_start`) | driver only |
| `jacobian_values_finite` | `:1338` | **`:3240`**, **`:3262`** (the restoration closure) | driver only |

Astra G6 is confirmed at BASE: `trace_mode_of` has driver-side callers only; `trace_outcome_of` is
the shared one. Linkage is decided PER HELPER, not per block.

### §1.4 External-linkage free functions in the TU

`hven::solvers` scope, `:337-471` and `:548-848` and `:987-1241` and `:1355-1478`. All but one are
declared in `include/hven/drivers/sqp_driver.h`:

`eval_nlp` `:337` · `eval_nlp_values` `:394` · `upgrade_to_full` `:427` ·
`constraint_violation_l1` `:456` · `detail::evaluate_kkt_over` `:474` · `evaluate_kkt` `:548`,
`:554`, `:626` · `build_subproblem` `:559`, `:579` · `predicted_decrease` `:584` ·
`crash_basis_seed` `:594` · `qp_failure_is_retryable` `:632` · `ssn_exit_is_a_usable_step` `:652` ·
`ssn_result_to_qp_solution` `:662` · `sweep_negative_face_prices` `:677` ·
`ssn_start_from_qp_seed` `:687` · `ssn_fb_tol_for` `:700` · `charge_ssn_subproblem_cost` `:704` ·
`charge_refused_face_refinement` `:709` · `accumulate_ssn_counters` `:718` ·
`ipqp_exit_is_a_usable_step` `:756` · `assert_ssn_warm_grade_window` `:779` ·
`assert_ipqp_hand_off_window` `:805` · `ipqp_result_to_qp_solution` `:826` ·
`charge_ipqp_subproblem_cost` `:844` · `run_elastic_ladder` `:987` ·
`certified_feasibility_fallback` `:1141` · `census_major_activity` `:1355` ·
`accumulate_ipqp_counters` `:1424`.

**FINDING (new, not in the skeleton or the review). TWO of these have EXTERNAL linkage and no
declaration anywhere in the tree**, and both were found only by sweeping all 26 against
`include/hven/drivers/sqp_driver.h`:

| symbol | definition | its only caller | why it is silent |
|---|---|---|---|
| `assert_ssn_warm_grade_window` | `:779` | `:4830`, inside `route_through_ssn_warm_grade` | the header names it only in a COMMENT (`include/hven/drivers/sqp_driver.h:3068`); the only other mention in the tree is a comment in `tests/sqp/test_ipqp_dispatch.cpp:746` |
| `assert_ipqp_hand_off_window` | `:805` | `:4124`, inside the kIpm arm | a repository-wide grep finds exactly two mentions, the definition and that call |

Every OTHER external-linkage free function in this TU is declared in
`include/hven/drivers/sqp_driver.h`. Neither is caught by a warning because neither `CMakeLists.txt`
enables `-Wmissing-prototypes` / `-Wmissing-declarations`. Cut (d) must not carry the defect across a
TU boundary: either each function moves into the same TU as its only caller and takes internal
linkage, or it gains a header declaration.

**DECIDED AT THE (d) DESIGN REVIEW (astra item 1, binding).** The rule is **one owning TU →
internal linkage; a necessary cross-TU dependency → private declaration**. Both callers stay
DRIVER-side under the §1.7 moving set, so both helpers KEEP THEIR BODIES IN THE DRIVER TU and GAIN
INTERNAL LINKAGE. **Neither needs a header declaration of any kind.**

| helper | definition | its sole caller | disposition |
|---|---|---|---|
| `assert_ssn_warm_grade_window` | `:779` | `route_through_ssn_warm_grade` | body stays in the driver TU; internal linkage |
| `assert_ipqp_hand_off_window` | `:805` | `route_through_ipqp_tier` | body stays in the driver TU; internal linkage |

**AND IT IS A SEPARATELY CLAIMED PREPARATION COMMIT, not part of the TU commit.** Internalising them
inside the six-symbol experiment would contradict that experiment's own "exactly ONE symbol changes
linkage" claim and would put a second linkage variable inside the measurement. The preparation
commit lands FIRST, with its own P-SYM claim, its own replay and its own suites; the TU commit is
then measured against a tree in which these two are already internal. That claim is astra's, and it
is quoted VERBATIM at §10.4.

### §1.7 Cut (d)'s MOVING SET — the side of every external-linkage free function

§1.2 names the one INTERNAL-linkage symbol the split forces external. This is the other half: cut (d)
moves free functions, and its P-SYM claim is built from this table. "K" = called from inside the
kernels region (`src/drivers/sqp_driver.cpp:987-1241`, the two ladder/fallback bodies); "D" = called
from anywhere else in the TU. Call sites were enumerated over the whole TU; the definition's own line
is excluded.

| symbol | def | K call sites | D call sites | side |
|---|---|---|---|---|
| `run_elastic_ladder` | `:987` | `:1183`, `:1194` | `:4294` | **KERNELS** |
| `certified_feasibility_fallback` | `:1141` | — | `:4187` | **KERNELS** |
| `predicted_decrease` | `:584` | `:1117` | `:4481` | **SHARED** |
| `eval_nlp` | `:337` | — | `:556`, `:581`, `:1937`, `:2567`, `:2642`, `:3237`, `:3457`, `:5076` | driver |
| `eval_nlp_values` | `:394` | — | `:2246`, `:2253`, `:4474`, `:4551` | driver |
| `upgrade_to_full` | `:427` | — | (no in-TU caller) | driver |
| `constraint_violation_l1` | `:456` | — | `:2724`, `:3230`, `:3241`, `:3253`, `:3478`, `:4480`, `:4557` | driver |
| `evaluate_kkt` | `:548`, `:554`, `:626` | — | `:556`, `:2695`, `:2971`, `:3461`, `:5078` | driver |
| `build_subproblem` | `:559`, `:579` | — | `:581`, `:2261`, `:3612`, `:4965` | driver |
| `crash_basis_seed` | `:594` | — | `:3689` | driver |
| `qp_failure_is_retryable` | `:632` | — | `:4392` | driver |
| `ssn_exit_is_a_usable_step` | `:652` | — | `:3787`, `:3827`, `:4843` | driver |
| `ssn_result_to_qp_solution` | `:662` | — | `:3788`, `:3847`, `:4874` | driver |
| `sweep_negative_face_prices` | `:677` | — | `:5056` | driver |
| `ssn_start_from_qp_seed` | `:687` | — | `:3748`, `:4820` | driver |
| `ssn_fb_tol_for` | `:700` | — | `:4768` | driver |
| `charge_ssn_subproblem_cost` | `:704` | — | `:3961`, `:4865` | driver |
| `charge_refused_face_refinement` | `:709` | — | `:3935` | driver |
| `accumulate_ssn_counters` | `:718` | — | `:3439`, `:3811`, `:4834` | driver |
| `ipqp_exit_is_a_usable_step` | `:756` | — | `:4056` | driver |
| `assert_ssn_warm_grade_window` | `:779` | — | `:4830` | driver |
| `assert_ipqp_hand_off_window` | `:805` | — | `:4124` | driver |
| `ipqp_result_to_qp_solution` | `:826` | — | `:4122`, `:4819` | driver |
| `charge_ipqp_subproblem_cost` | `:844` | — | `:4155`, `:4167`, `:4183` | driver |
| `census_major_activity` | `:1355` | — | `:4371` | driver |
| `accumulate_ipqp_counters` | `:1424` | — | `:3447`, `:4051` | driver |
| `detail::evaluate_kkt_over` | `:474` | — | `:550`, `:628` | driver |

**THE MOVING SET IS THEREFORE**: `run_elastic_ladder` and `certified_feasibility_fallback`, plus
anonymous namespace #2's three kernels-only helpers `elastic_initial_rho` (`:854`),
`elastic_evidence_seed` (`:880`) and `emit_qp_mode_line` (`:972`), plus `trace_outcome_of` (`:963`),
**which is the ONE symbol that changes linkage** (§1.2). Nothing else moves.

**`predicted_decrease` is a SECOND symbol used by both sides**, and the doc's earlier claim that
`trace_outcome_of` is "the ONE shared symbol" is true only of the INTERNAL-linkage ones. It costs no
new declared surface, because it is already declared in `include/hven/drivers/sqp_driver.h` — the
kernels TU includes that header and calls it exactly as it does today. It is named here so T6.d's
P-SYM claim lists it rather than meeting it.

**The moving set has a TEST-FACING surface — THREE TUs, not four.** Corrected by the SQP lane at
the T6.0 fix1 review (A3) and re-checked at source: the DIRECT calls are 15 in
`tests/sqp/test_sqp_driver.cpp`, 3 in `tests/sqp/test_ipqp_dispatch.cpp`, **0** in
`tests/sqp/test_qp_mode_sites.cpp` — its three mentions are all COMMENTS — and 1 in
`tests/sqp/test_trace_writer.cpp`; `predicted_decrease` adds `tests/sqp/test_sqp_driver.cpp`.

**Their expectation under (d) is BYTE-IDENTICAL, and the reason is structural, not hopeful.** Both
kernels are DEFINED in `src/drivers/sqp_driver.cpp` and only DECLARED in the header, so no test TU
has ever seen a body to inline: every one of those 19 calls is already a relocated call to an
external symbol, and moving the definition to another TU of the same library changes neither the
call nor the relocation. The three objects are therefore in the P-SYM claim as byte-identical, and
a DIFFERS on any of them is a finding. The de-inlining cut (d) actually risks is INSIDE
`sqp_driver.cpp`, where the bodies are visible today — which is what the §11.4 caller disassembly
must name.

#### §1.7.1 THE INLINING-EXPOSURE TABLE — the five edges the boundary actually cuts

Astra's item 3, at HEAD `39d7a8e`. These are the source call sites that lose callee-body visibility,
and they are the edges the §11.4 relocation-aware disassembly must name ONE BY ONE. Cuts (a)–(c)
moved the callers, so this table is re-derived at HEAD and does not reuse §1.7's BASE coordinates.

| caller and call site | callee becoming unavailable | exposure |
|---|---|---|
| `SqpDriver::route_through_ipqp_tier`, `:3404` | `certified_feasibility_fallback` | the conditional IPQP escape route inside a major. Whole-body inlining is possible at compiler discretion; hotness depends on escape frequency |
| `SqpDriver::solve_with_walk`, `:3447` | `trace_outcome_of` | a tiny pure mapping and a very plausible inline candidate. Executed per dispatched walk **only with a trace sink attached** |
| `SqpDriver::run_major`, `:4570` | `run_elastic_ladder` | conditional infeasibility recovery, skipped when a fallback report already owns the answer. Large body with an internal rung loop; whole-body inlining is unlikely, but interprocedural optimisation remains exposed |
| `SqpDriver::run_major`, `:4792` | `trace_outcome_of` | the tiny mapping again, after an SOC solve; trace-enabled only |

And the REVERSE-DIRECTION edge, which is the one the plan's "loop-used arithmetic is not cold"
sentence is about, and which no caller-side reading of the split would find on its own:

| caller | callee REMAINING in the driver TU | exposure |
|---|---|---|
| `run_elastic_ladder`, `:1117` | `predicted_decrease` | once after the rung loop, on an optimal result. A short source spelling that expands into Eigen arithmetic and exception machinery; body visibility is lost in this direction |

What stays together and is therefore NOT exposed: the fallback's two ladder calls (`:1183`, `:1194`)
move with it, its three mapper uses and the three kernels-only helpers keep local visibility inside
the new TU, and `run_major → predicted_decrease` (`:4744`) retains visibility because both ends stay
in the driver.

**AND THE OBSERVED BASELINE CORRECTS THE PREMISE.** The addendum's framing — that (d) replaces
inlined large-kernel bodies with calls — is NOT a description of this tree. The retained post-(c)
Release object
(`.scratch/w5t6c/rel/snap-fix2/CMakeFiles/hven.dir/src/drivers/sqp_driver.cpp.o`) already carries
`R_X86_64_PLT32` relocated calls for `route_through_ipqp_tier → certified_feasibility_fallback`,
`run_major → run_elastic_ladder` (both sites) and `run_elastic_ladder → predicted_decrease`; the
ladder body is 14,865 bytes and the fallback 1,384. **The two kernels are already externally linked
and already out of line.** `trace_outcome_of` has NO standalone symbol in that object — it is fully
inlined today — and is therefore the CLEAREST new call exposure of the whole cut.

Two things follow, and both bind the claim. Lost inferred properties and constant propagation can
still change a caller, so the edges above stay on the disassembly list. But **WORK-MOVED is a
properly registered expected RISK, not a predetermined result**, and no exception may excuse a
fallback caller by asserting a de-inlining that its own BEFORE disassembly disproves.

**AND THE BANNER IS REVISED AT COMMIT 3, not before.** `src/drivers/sqp_driver.cpp:14` says the free
functions are in this TU "since `solve_impl` and its neighbours in this TU are the only in-library
callers any of them have" — a REAL dependency, and true of every free function until (d) moves six of
them. Cuts (a)–(c) changed function boundaries but kept every body in this TU, so the wording held
through all three. After (d) the blanket "every free function" claim is false, and the banner is
rewritten to the MEASURED boundary — which six left, why, and what the measurement said — at the TU
commit itself rather than in a later tidy-up.

**LTO IS OUTSIDE (d), and the fact is recorded as observed rather than as configured.** The retained
Release build has `HVEN_LINK_TIME_OPT:BOOL=OFF` (`build/CMakeCache.txt:356`); the driver's compile
command carries no LTO and no PCH consumption (`build/compile_commands.json:165`); the corpus link
carries no LTO option (`build/build.ninja:3571`). The repository supports optional LTO, default OFF
(`CMakeLists.txt:79`). Turning it on inside (d) would add a second experimental variable AND change
the uniform flag regime CLAUDE.md §7 fixes. **(d) must not flip it.**

### §1.5 `SqpDriver` member functions in this TU

`solve(model)` `:1479` · `attach_ledger` `:1481` · `attach_trace` `:1488` · `emit_trace_route`
`:1495` · `emit_trace_qp_mode` `:1501` · `emit_trace_fallback_verdict` `:1507` ·
`emit_trace_sqp_major` `:1513` · `solve(model,x0)` `:1530` · `solve(model,x0,warm,…)` `:1536` ·
`solve(bridge,x0)` `:1546` · `solve(bridge,x0,warm,…)` `:1571` · `record_solve` `:1588` ·
`export_warm_start` `:1660` · `stage_warm_start` `:1673` · `refuse_two_warm_sources` `:1695` ·
`consume_staged_warm_start` `:1714` · `capture_completed_warm_start` `:1846` ·
`install_solve_scaling` `:1924` · `solve_impl` `:1945` · **`solve_impl_body` `:2025`** ·
`map_status` `:4734` · `shrink_hits_floor` `:4746` · `shrunk_radius` `:4750` ·
`restoration_restart_radius` `:4754` · `ssn_engine` `:4759` · `ssn_options` `:4766` ·
`ipqp_engine` `:4780` · `ipqp_options` `:4791` · `route_through_ssn_warm_grade` `:4800` ·
`make_warm_start` `:4904` · `finish` `:4972`.

### §1.6 Build integration (astra G6)

`src/drivers/sqp_driver.cpp` is listed in `target_sources` at `src/CMakeLists.txt:197`. The
source-count guard is `set(_hven_expected_source_count 41)` at `src/CMakeLists.txt:446`, enforced by
postcondition 3 at `src/CMakeLists.txt:593-601`. The PCH opt-in list is `_hven_pch_sources` at
`src/CMakeLists.txt:307-313`; **`drivers/sqp_driver.cpp` is NOT in it**, so it carries
`SKIP_PRECOMPILE_HEADERS ON` (applied at `src/CMakeLists.txt:548-551`). A kernels TU added by cut (d)
must therefore (i) join `target_sources`, (ii) move the count 41 → 42, and (iii) inherit the same
opt-out disposition — or be measured with `scripts/check_pch_neutrality.sh` and argued into
`_hven_pch_sources`. Adding a filename alone fails the configure step.

**CONFIRMED at the (d) design review (astra item 7), with (iii) SETTLED rather than left open.** The
new TU is **PCH-OPTED-OUT**: it simply stays off `_hven_pch_sources`, and the deferred block at
`src/CMakeLists.txt:548` then sets `SKIP_PRECOMPILE_HEADERS ON` for it as it does for every unlisted
source, with postcondition 2 reading the property back. The existing six-consumer opt-in list is
UNCHANGED, and **no PCH experimentation rides on this cut** — the alternative branch above (measure
it and argue it in) is explicitly NOT taken at (d), because it would put a second variable inside a
veto-grade measurement. The count guard's enforcement at `src/CMakeLists.txt:593` is preserved.

---

## §2. The state universe

The two state types of §5 F1 are not the whole universe (astra §2). Four dispositions:

* **BORROWED** — lives outside the solve; the body reads or drives it and never owns it.
* **`SolveState`** — ONE authoritative mutable instance, initialised in place by `prepare_solve`,
  alive for the whole solve.
* **`MajorState`** — constructed at the loop top, dies with the major; nothing in `SolveState` may
  reference it and no reference to it may escape. **"At the loop top" is the POST-(c) TARGET STATE,
  not what stands today: at cut (b) the bundle is the four routing outputs and is constructed AT THE
  DISPATCH, for the construction-timing reason §10.2 gives. The loop top is where it lands once
  `kkt`, `row`, `row_qp_mode`, `caller_row` and the trial/SOC objects join it at (c). See §10.2.**
* **LOCAL SCRATCH** — setup-only probes, arm-local objects, restoration-only objects. Not a third
  state type; ordinary locals in whatever function ends up owning the block.

### §2.1 BORROWED

| name | site | what it is | notes |
|---|---|---|---|
| `seam` | param, `:2025` | `AggregateEvalSeam &` | MUTATED by `install_solve_scaling` (`:2070`) and, after the body returns, by `finish` (`:5075` installs the identity and restores). Outlives the solve. |
| `bridge` | param, `:2025` | `NlpModelAggregate &` | read at `:3281` (`bridge.model()` into `RestorationModel`), `:4315-4316` (`lower()`/`upper()` for the elastic clamp). |
| `x0` | param, `:2026` | `const Vec &` | read `:2070`, `:2261`, `:2289`, `:2637`. |
| `warm` | param, `:2026` | `const WarmStart &` | read across the ingest block `:2216-2408` and at `:4214` (`warm.hot`, the hot offer). |
| `minor_budget` | param, `:2026` | `Index` | read `:3102-3103`. |
| `opts_` | member | `SqpOptions` | read everywhere; never written. |
| `engine_` | member, header `:3220` | `QpEngine` | the walk engine. Driven at `:4214-4217` (dispatch walk), `:3791`/`:3879`/`:4131` (`refine_on_face`), `:4187` (through `certified_feasibility_fallback`), `:4294` (through `run_elastic_ladder`), `:4525` (SOC). `hot_state()` read at every `make_warm_start`. **Outlives the solve.** |
| `ssn_engine_` | member, header `:3239` | `unique_ptr<SsnEngine>` | reached only via `ssn_engine()` (`:4759`); lazily constructed. Driven at `:3748`, `:3803`, `:3805`, `:3808`. Outlives the solve. |
| `ipqp_engine_` | member, header `:3249` | `unique_ptr<IpqpEngine>` | `reset_warm_carry()` at `:2051` and `:4083`; `warm_carry()` at `:4025`; `set_trace_major` `:4045`; `solve` `:4048`. Outlives the solve. |
| `ipqp_trace_` | member, header `:3253` | `TraceSink *` | nullable; the sink every emit tests. **Handed to the nested restoration driver at `:3345`.** Outlives the solve. |
| `ipqp_staged_seed_` | member, header `:3265` | `optional<IpqpSeed>` | READ at `:4023-4024`, **`reset()` at `:4049`** — after `ipqp_engine().solve` at `:4048` consumed the pointer. |
| `ssn_prox_sigma_out_`, `ssn_prox_center_sigma_out_`, `ssn_prox_center_x_out_`, `ssn_prox_center_lambda_out_` | members, header `:3275-3280` | export accumulators | cleared at `:2059-2062`, raised at `:3826` and `:3841-3845`. **Read by `record_solve` at `:1615-1620`, AFTER the body returns** — they must not become `SolveState` members. |
| `allow_restoration_` | member, header `:3283` | `bool` | read at `:3180`. |
| `ledger_` | member, header `:3287` | `Ledger *` | read by `record_solve`, not by the body. |

### §2.2 `SolveState` — one instance, `prepare_solve` initialises it in place

Grouped by role; every name listed. "Mutated at" is exhaustive within `solve_impl_body`.

**A. Solve invariants (const after `prepare_solve`).** Five of the names in this table and in group
I turned out NOT to be solve-scope state when cut (a) came to type them, and are recorded here rather than left as a
discrepancy between this table and the code: `ipm_mode` (`:2042`, read only at `:2050`),
`warm_dims_plausible` (`:2216`), `ingest_allowed` (`:2231`) and `probe_is_worth_running` (`:2239`)
from this table, and `warm_ingest` (`:2277`) from group I, are read NOWHERE after the loop begins
(`warm_ingest`'s last read is `:2659`), so they are `prepare_solve` locals and not `SolveState`
members. The membership test is exactly that: a name is
in the bundle iff something at or after `:2680` reads or writes it.

| name | decl | init | disposition |
|---|---|---|---|
| `n` | `:2030` | `seam.n()` | const |
| `ssn_mode` | `:2038` | `opts_.qp_mode == kSsn` | const |
| `ipm_mode` | `:2042` | `opts_.qp_mode == kIpm` | const |
| `solve_scaling` | `:2075` | `seam.scaling()` | **const COPY, never a reference into the seam** — `finish` overwrites the seam's scaling at `:5075` |
| `warm_dims_plausible` | `:2216` | — | const, setup only |
| `ingest_allowed` | `:2231` | — | const, setup only |
| `probe_is_worth_running` | `:2239` | — | const, setup only |
| `warm_state_ingest` | `:2284` | — | const; read again at `:2891`, `:2908` |

**B. Output.** `out` (`:2032`, `SqpSolution`). Counters accumulate from `:2070` to `:4723`;
`out.history` is written by `push_history` and by nothing else (the invariant at `:2769` is what
makes that true); `out.infeasibility_certified` is set at `:3571`. `std::move(out)` into `finish` at
`:3127`, `:3140`, `:4340`, `:4414`, `:4443`, `:4639`, `:4667`; returned by hand at `:2862`.

**C. Iterate and multipliers.** `x` (`:2289`), `lambda_e` (`:2290`), `lambda_i` (`:2291`).
Mutated at `:2303-2311` (caller→engine dual rescale), `:2587` (inactive-inequality dual zeroing),
`:2610` (seeded clamp), `:2637-2639` (seeded degrade-to-cold), `:2966-2968` (watchdog restore),
`:3471-3472` (restoration measurement failure — CLEARED), `:3482`/`:3487-3488` (restoration resume),
`:3528`/`:3533-3534` (restoration terminal adopt), `:4696`/`:4698-4699` (SOC commit),
`:4714`/`:4718-4719` (plain commit). ONE authoritative instance.

**D. Evaluation.** `ev` (`:2567`, `NlpEval`, from `seam.eval_nlp`). Replaced at `:2642`
(degrade re-evaluate), `:2970` (`std::move(fs_best_ev)`), `:3483` (`std::move(ev_r)`), `:4697`
(`std::move(ev_soc)`), `:4717` (`std::move(ev_trial)`). ONE authoritative instance; T3 made it the
persistent destination worth having.

**E. Subproblem and seeds.** `qp` (`:2456`), `subproblem_is_stale` (`:2457`), `qp_built` (`:2511`),
`seed` (`:2342`), `have_seed` (`:2343`), `crash_pending` (`:2375`), `crash_seed` (`:2376`).
`qp` is rebuilt at `:3612` when stale. `subproblem_is_stale` set at `:3022`, `:3510`, `:4725`,
cleared at `:3613`. `seed` replaced at `:4432`, `:4649`, `:4701`, `:4721`, each followed by
`seed.x.setZero()` (`:4433`, `:4650`, `:4729`) — the trust-region centre is `p = 0`, never the step.
`have_seed` false at `:2640`, `:3514`; true at `:2355`, `:4434`, `:4651`, `:4730`.

**F. Radius, prices, budgets, retry counts.** `delta` (`:2391`; `:2393` warm clamp, `:3507`
restoration restart, `:4423`/`:4648` shrink, `:4692` grow), `last_dual_mu` (`:2512`; `:3657`),
`ssn_budget_charge` (`:2442`; `:3817`, `:3910`, `:3935`, and by reference at `:4157`/`:4169`),
`ipqp_budget_charge` (`:2449`; `:4055`, `:4144`, `:4154`), `rejections_at_iterate` (`:2450`; `:3017`,
`:3508`, `:4394`, `:4619`, `:4724`), `qp_failures_in_a_row` (`:2455`; `:3509`, `:4393`, `:4448`).

**G. IPQP retirement and epoch.** `ipqp_ladder` (`:2046`, `IpqpEscapeLadder`; `retired()` read at
`:3993`, `record()` at `:4076`, `retired_after()` at `:4077`), `ipqp_analysis_epoch` (`:2056`,
`optional<StructureEpoch>`; read at `:4016`, written at `:4050`). One per solve, by construction.

**H. SSN proximal carry.** `ssn_prox_ingested` (`:2407`). Spent ONE-SHOT: read at `:3734` then
zeroed at `:3735` in the kSsn arm; passed BY REFERENCE at `:4157`/`:4169` into
`route_through_ssn_warm_grade`, which reads it at `:4808` and zeroes it at `:4809`. Two spend sites,
one spend per solve.

**I. Ingest state.** `resolved_level` (`:2193`; written `:2237`, `:2263`, `:2268`, `:2631`; **read
at `:3670`**, `const bool offer_hot = iter == 0 && resolved_level == StartLevel::kHot;` — that one
read is what makes it solve-scope under the membership test in group A),
`warm_ingest` (`:2277`; `:2636`), `duals_ingested` (`:2659`; `:2969`, `:3498`, `:3545`, `:4700`,
`:4720`), `funnel_started` (`:2647`; `:2914`).

**J. Push accounting.** `rows_pushed` (`:2670`; `++` at `:2766`), `rows_at_major_entry` (`:2671`;
`:2688`). These two ARE the exactly-once invariant; they may not be duplicated.

**K. Activity carry.** `prev_ineq_active` (`:2676`), `prev_bound_state` (`:2677`), `activity_slack`
(`:2678`). Read and written by `census_major_activity` at `:4371`; `prev_*` reassigned at
`:4372-4373`.

**L. The restoration-exit sub-bundle (NAMED, not flat).** `restoration_used` (`:2459`; set `:3202`),
`restoration_exit_status` (`:2462`), `restoration_exit_kkt` (`:2463`), `restoration_exit_f`
(`:2464`), `restoration_exit_multipliers_are_caller_scale` (`:2472`; set `:3541`),
`restoration_moved_x` (`:2513`; reset `:3175`, set `:3532`). Written only inside the closure; read
only at the four `finish` sites `:4340-4346`, `:4414-4420`, `:4639-4645`, `:4667-4672` and, for
`restoration_moved_x`, in each of those sites' `make_warm_start` seed choice.

**M. The full-step watchdog snapshot (NAMED sub-bundle) + one pointer.**
`full_step_funnel` (`:2526`, `FunnelStrategy *`) — **POINTER IDENTITY**, non-owning, aimed at
`strategy` by the `dynamic_cast` at `:2909-2911`, nulled at `:3016` and `:3506`, tested at `:2925`
and `:4261`. The snapshot proper: `fs_best_x`, `fs_best_lambda_e`, `fs_best_lambda_i` (`:2530`),
`fs_best_ev` (`:2531`), `fs_best_residual` (`:2532`), `fs_best_duals_ingested` (`:2537`),
`fs_growth_in_a_row` (`:2542`), `fs_majors_since_best` (`:2543`), `fs_prev_residual` (`:2544`).
Captured at `:2942-2949` — **independent copies, including the snapshot's own dual-ingest flag at
`:2946`** — and restored by MOVE at `:2966-2970`.

**N. The budget-best snapshot (NAMED sub-bundle).** `mb_best_x`, `mb_best_lambda_e`,
`mb_best_lambda_i` (`:2555`), `mb_best_kkt` (`:2556`), `mb_best_h` (`:2557`), `mb_best_f` (`:2558`).
Captured at `:3071-3076`; read at `:3125` and `:3128-3129`. Independent copies.

**O. The strategy.** `strategy` (parameter `:2027`, `unique_ptr<GlobalizationStrategy>`, moved in
from `:2017`). **POINTER IDENTITY.** `reset` `:2870`; `dynamic_cast` `:2892`, `:2909`;
`resume_from_restoration` `:3014`, `:3505`; `judge` `:4496`, `:4562`; `strategy.get()` handed to
`make_warm_start` at `:2845`, `:3132`, `:3145`, `:4345`, `:4418`, `:4446`, `:4643`, `:4671`. It is a
`SolveState` member, not re-derived and not re-wrapped.

### §2.3 `MajorState` — constructed at the loop top, dies with the major

**THIS TABLE IS THE POST-(c) TARGET, AND THE HEADING WITH IT.** At cut (b) `MajorState` holds only
the four routing outputs (`qs`, `fallback_report`, `walk_owns_this_qp`, `ipqp_chain_owns_the_step`)
and is constructed **at the dispatch**, where those four were declared — see §10.2 for the decision
and the construction-timing hazard that forced it. Every other row below joins the bundle at cut (c),
and that is when the construction moves to the loop top.

| name | decl | notes |
|---|---|---|
| `kkt` | `:2695` | `evaluate_kkt` over the existing `ev` — **no model call**. Re-taken at `:2971` after a watchdog restore. |
| `row` | `:2697` | `row.trial = iter` at `:2698`. Mutated by `measure_iterate`, by `certified_feasibility_fallback` (passed by reference at `:4189`), at `:3054`, `:3278`, `:3656`, `:4299-4308`, `:4354-4366`, `:4371`, `:4607-4608`. |
| `row_qp_mode` | `:2702` | `trace_mode_of(opts_.qp_mode)`; corrected to `kSsn` at `:4159`/`:4171`, to `kWalk` at `:4213`. Read by `push_history` at `:2763`. |
| `caller_row` | `:2714` | `CallerScaleRowMeasures`; written by `measure_iterate` at `:2726-2737`; read by `push_history` at `:2754-2755`. Untouched and unread on an unscaled solve. |
| `measure_iterate` | `:2718` | lambda, `[&]` |
| `push_history` | `:2749` | lambda, `[&]`, takes `SqpIterate exported` **BY VALUE** — export operates on a ROW COPY |
| `check_major_pushed_once` | `:2780` | lambda, `[&]` |
| `converged`, `probe_exhausted` | `:3091`, `:3101` | const |
| `struct RestorationCandidate` | `:3166` | two raw pointers: `const Vec *x`, `NlpEval *values_ev` |
| `enter_restoration` | `:3170` | lambda, `[&]`, takes `RestorationCandidate` **BY VALUE** |
| `tr_shrink_retry` | `:3610` | `!subproblem_is_stale` — read at `:4030` and `:4076`; a shrink-retry major neither re-centres the IPQP carry nor charges the escape ladder |
| `overrides` | `:3617` | `tr_radius` `:3618`, `dual_mu` `:3654` |
| `adaptive_mu_active` | `:3648` | const |
| `offer_hot` | `:3670` | const; read `:4214`, `:4230` |
| `use_crash` | `:3678` | `:3689`; read `:4216` |
| `qs` | `:3702` | assigned by every arm or by the successor |
| `fallback_report` | `:3705` | `optional<ElasticLadderReport>`; filled through `:4190` |
| `walk_owns_this_qp` | `:3706` | initialised **true** (the unhandled-mode safety net named in the `static_assert` at `:3716-3722`) |
| `ipqp_chain_owns_the_step` | `:3710` | `:4136`, `:4158`, `:4170`; read `:4209` |
| `elastic_applied`, `rho0_ceiling_hit` | `:4279`, `:4280` | |
| `x_trial`, `ev_trial` | `:4473`, `:4474` | **not constructed before the early exits** — see §10.2 |
| `ctx` | `:4476` | `StepContext`; overwritten by `soc_ctx` at `:4566` |
| `x_scale`, `zero_step`, `verdict` | `:4493`, `:4494`, `:4496` | |
| `soc_applied`, `ev_soc`, `qs_soc`, `x_soc` | `:4503-4506` | filled iff `soc_applied` |
| `actual_df` | `:4689` | |

### §2.4 LOCAL SCRATCH (not a state type)

* **Setup-only probes**: `probe_ev` (`:2253`), `probe` (`:2260`), `hint_is_empty` (`:2345`),
  `degrade_to_cold` (`:2604`). Die inside `prepare_solve`.
* **kSsn arm**: `sopts` `:3734`, `ssn_overrides` `:3736`, `sres` `:3745`, `r5_refined` `:3783`,
  `r5_have` `:3784`, `r5_took` `:3785`, `r5_face` `:3788`, `refined` `:3871`, `took` `:3872`,
  `refine_facts` `:3881`, `refine_steps` `:3882`.
* **kIpm arm**: `ipqp_overrides` `:3983`, `ipqp_box` `:4002`, `structure_epoch_moved` `:4015`,
  `iopts` `:4017`, `ipqp_seed` `:4021`, `ipqp_carry_across_majors` `:4022`, `ires` `:4048`,
  `usable` `:4056`, `ladder_outcome` `:4072`, `ipqp_trace_face_rows` `:4088`,
  `ipqp_trace_face_bounds` `:4089`, `emit_ipqp_route_and_mode` `:4098`, `face` `:4122`,
  `refined` `:4128`, `took` `:4131`, `refine_facts` `:4132`, `refine_steps` `:4133`,
  `fallback_verdict` `:4186`.
* **Crash block**: `seeded_rows` `:3687`, `seeded_bounds` `:3688`.
* **Infeasible-QP block**: `window` `:4287`, `elastic_seed_source` `:4288`, `report` `:4291`,
  `have_p` `:4312`, `x_elastic` `:4314`, `elastic_cand` `:4318`.
* **SOC block**: `soc_qp` `:4517`, `seed_soc` `:4521`, `soc_overrides` `:4523`, `soc_ctx` `:4553`,
  `soc_verdict` `:4562`.
* **Restoration-only** (astra §2: no persistent third type): `spent` `:3178`, `resto_ev` `:3223`,
  `x_start` `:3228`, `h_entry` `:3230`, `ev_cand` `:3231`, `take` `:3232`, `probe` `:3249`,
  `feasibility` `:3281`, `ropts` `:3282`, `sub` `:3341`, `rs` `:3346`, `x_r` `:3455`,
  `ev_r` `:3456`, `kkt_r` `:3461`, `h_r` `:3478`.

### §2.5 Aliasing rules (astra §2, binding)

References to owning `Vec`/`NlpEval` members may intentionally observe updates; **cached Eigen views
or element pointers may never be assumed valid across an assignment or a move**. `SolveState`
addresses stay stable for the solve. **No reference to `MajorState` may escape the major.** And
there is ONE authoritative restoration payload — **RULED in §5 constraint 5: the STATE BUNDLE owns
it** (§2.2 L's five fields stay `SolveState` members) and the outcome type is a TAG, never a second
copy.

---

## §3. astra's §2 disposition table, VERBATIM

Reproduced from `.superpowers/w5-t6-plan-review-astra.md` §2. **The table CONTENT is verbatim; only
the LINK FORM was converted** — astra wrote absolute `/home/ghecht/Projects/hven/...` targets, which
are that reviewer's citation form and not content, and a machine-specific path does not belong in a
committed document (SQP-lane M1 / Claude-substitute M1, settler RULED at the T6.0 fix round). 19
occurrences across 15 lines in this file were rewritten to repo-relative; not a character of the
table's text changed. The BASE-re-derived numbers are §2 above.

> | Object/view | Required disposition |
> |---|---|
> | Scaling | Copy: `finish` temporarily replaces seam scaling. [2075](src/drivers/sqp_driver.cpp:2075), [5075](src/drivers/sqp_driver.cpp:5075) |
> | Watchdog and budget-best state | Independent vector/evaluation/KKT snapshots, including the snapshot’s dual-ingest flag. [2942](src/drivers/sqp_driver.cpp:2942), [3071](src/drivers/sqp_driver.cpp:3071) |
> | Major row and caller-scale measurements | Preserve the requesting iterate’s measurements across restoration’s mutations; do not recompute them from the resumed `SolveState`. Export operates on a row copy. [2714](src/drivers/sqp_driver.cpp:2714), [2749](src/drivers/sqp_driver.cpp:2749) |
> | IPQP seed pointer | Consume before staged-seed reset or carry replacement; preserve the copied/recentered cross-major seed. [4021](src/drivers/sqp_driver.cpp:4021), [4048](src/drivers/sqp_driver.cpp:4048) |
> | SOC seed | Copy `qs` before zeroing the seed’s primal block. [4521](src/drivers/sqp_driver.cpp:4521) |

All five rows verified at BASE: `:2075`/`:5075`; `:2942`/`:3071`; `:2714`/`:2749`;
`:4021`/`:4048` (with the staged-seed `reset()` at `:4049`, i.e. AFTER the solve at `:4048`
consumed the pointer, and the cross-major re-centred copy at `:4030-4036`); `:4521`
(`QpSolution seed_soc = qs;` — a copy, because `qs` is still read at `:4641`, `:4669` and `:4721`).

---

## §4. The two state types (§5 F1), as they fall out of lifetimes

`SolveState` — ONE instance, initialised **IN PLACE** by `prepare_solve(SolveState &, …)`, never a
returned self-referencing object (the plan is explicit and the reason is `full_step_funnel` and
`strategy`: a returned aggregate would have to be re-pointed after the move). Contents: §2.2 groups
A–O, with L, M and N as NAMED sub-bundles rather than a flat list.

`MajorState` — §2.3. It does not persist and nothing in `SolveState` may reference it.

The three lambdas capture both scopes by reference today; after cut (a) they take
`(SolveState &, MajorState &)` and nothing else.

`prepare_solve` is `:2030-2678` inclusive; its last acts are the first `eval_nlp` at `:2567` and the
flags through `:2678`. Cut (a) changes `solve_impl_body` and adds exactly one ONLY-AFTER symbol.

---

## §5. The restoration closure — the EXACT sequence (astra G1 governs)

`enter_restoration` is `src/drivers/sqp_driver.cpp:3170-3599`, a `[&]` lambda: **everything by
reference**. The only by-value thing is its parameter `RestorationCandidate cand`
(`:3166-3169`) — two raw pointers, `const Vec *x` and `NlpEval *values_ev`.

**§5 F2's sentence "all decision inputs are READ before any mutation" is FALSE.** The actual
sequence, in order, is:

| # | step | lines |
|---|---|---|
| 0 | **WRITE** `restoration_moved_x = false` | `:3175` |
| 1 | read `iter`, `out.counters.restoration_iters` → `spent` | `:3178` |
| 2 | gate: `!allow_restoration_ \|\| restoration_used` → **WRITE** exit fields from the CURRENT `kkt`/`ev.f`, `return false` | `:3180-3191` |
| 3 | gate: `spent >= opts_.max_iter` → **WRITE** exit fields from the CURRENT `kkt`/`ev.f`, `return false` | `:3192-3201` |
| 4 | **WRITE** `restoration_used = true` | `:3202` |
| 5 | copy `ev` → `resto_ev`, re-scale to caller units | `:3223-3224` |
| 6 | `x_start = &x`; if a candidate is present and finite: measure `h_entry` on `resto_ev` | `:3228-3230` |
| 7a | **unmeasured route** (`cand.values_ev == nullptr`): evaluate the candidate fully (`+1 evals_full`), re-scale, then **READ the newly-evaluated `ev_cand`** for finiteness, Jacobian finiteness and `h < h_entry` | `:3233-3241` |
| 7b | **measured route**: re-scale a COPY (`probe`) of the values bundle, compare `h < h_entry`; only then `refresh_derivatives` on **the ORIGINAL candidate** (`+1 evals_full`, `−1 evals_values`) — **a MUTATION** — then **RE-READ** `all_finite` and `jacobian_values_finite` on the mutated bundle; only then `std::move` it into `ev_cand` and re-scale | `:3242-3268` |
| 8 | if taken: `x_start = cand.x`, `resto_ev = std::move(ev_cand)`, **WRITE `row.restoration_seed_used = true`** | `:3269-3279` |
| 9 | build `RestorationModel feasibility`, `ropts`, sub-driver `sub`, attach the SAME trace sink | `:3281-3345` |
| 10 | **NESTED SOLVE** `rs = sub.solve(feasibility, feasibility.start_point())` | `:3346` |
| 11 | fold ~25 counter families into `out.counters` | `:3362-3447` |
| 12 | measure the returned point: `x_r`, `ev_r` (`+1 evals_full` iff finite), `kkt_r` | `:3455-3463` |
| 13 | **failure path**: `!x_r.allFinite() \|\| !kkt_r.finite` → **multipliers CLEARED FIRST** (`:3471-3472`), and only THEN the OLD requesting `kkt` and `ev.f` are copied into the exit payload (`:3474-3475`), `return false` | `:3464-3477` |
| 14 | **resume path**: `h_r <= feas_tol` → `x = x_r`, `ev = move(ev_r)`, multipliers zeroed, `duals_ingested = false`, `strategy->resume_from_restoration(h_r)`, `full_step_funnel = nullptr`, `delta`, three counters, `have_seed = false`, **`return true`** | `:3480-3515` |
| 15 | **adopting terminal path**: `x = x_r` and multipliers OVERWRITTEN (`:3528`, `:3533-3534`) **BEFORE** `kkt_r`, the restoration bound prices (`feasibility.original_x(rs.z)`), `ev_r.f` and `rs.status` are read into the payload (`:3546`, `:3559`, `:3560`, `:3561-3597`); `restoration_moved_x = true` (`:3532`), `restoration_exit_multipliers_are_caller_scale = true` (`:3541`), `duals_ingested = false` (`:3545`), `out.infeasibility_certified = true` on the certified arm (`:3571`); **`return false`** | `:3528-3598` |

**The outcome type remains viable, on these terms.**

`RestorationOutcome { REFUSED, RESUMED, EXITED }` plus an exit payload
(`status, kkt, f, multipliers_are_caller_scale, moved_x`). Classification:

* **REFUSED** — the pre-run gates only: `:3180-3191`, `:3192-3201`.
* **RESUMED** — the successful continuation at `:3515`.
* **EXITED** — **ALL post-run terminal paths**, including the numerical-error path at `:3476`
  which adopts no point.

Binding constraints on the extraction:

1. The body must retain the sequence in the table above, step for step. Keeping the call before
   `push_history` is **necessary, not sufficient**.
2. The call point stays where it is on every path: after the trial's evaluation and BEFORE
   `push_history` (`:4329-4330`, `:4403-4404`, `:4629-4630`, `:4659-4660`), because
   `row.restoration_seed_used` is set INSIDE (`:3278`) and must be in the row that is pushed.
3. The outcome **CONSUMES** the candidate bundle exactly as the `std::move` at `:3263` does, and the
   consumption stays **CONDITIONAL** on the re-read at `:3262` — it is not an unconditional
   consumption on entry. The two measured callers pass per-major locals `{&x_trial, &ev_trial}`
   (`:4629`, `:4659`) that die with the major; the elastic caller passes `x_elastic` only
   (`:4318-4320`). The moved-from bundle is never read again today.
4. Certification (`:3571`), the caller-scale multiplier flag (`:3541`) and the warm-start
   seed-selection flag `restoration_moved_x` (`:3532`) are carried EXPLICITLY. **They are never
   inferred from the outcome tag.**
5. ONE authoritative payload — **RULED here rather than left to T6.c: the STATE BUNDLE owns it.**
   astra §2 asked the doc to choose, and §2.2 L already settles it at source: the five fields
   `restoration_exit_status` (`:2462`), `restoration_exit_kkt` (`:2463`), `restoration_exit_f`
   (`:2464`), `restoration_exit_multipliers_are_caller_scale` (`:2472`) and `restoration_moved_x`
   (`:2513`) are SOLVE-scope declarations, written only inside the closure and read at the four
   `finish` sites (`:4340-4346`, `:4414-4420`, `:4639-4645`, `:4667-4672`) and in each site's
   `make_warm_start` seed choice. They stay `SolveState` members. **`RestorationOutcome` is therefore
   a TAG** — `{REFUSED, RESUMED, EXITED}` plus whatever §6 G1 requires it to carry explicitly — and
   it holds NO copy of the payload. Leaving the choice open into T6.c is exactly the shape astra
   warned against ("independently mutable copies in both").

**The registered decide/run split is NOT needed** and stays registered, not T6: REFUSED *is* the
decision and the sub-solve *is* the run. The brief's §2 clause resolves in the "not T6" direction.

---

## §6. The routing switch — crossing state

The dispatch is `src/drivers/sqp_driver.cpp:3723-4205`; the shared successor is `:4209-4229`.

**Set before the switch and read BY A ROUTING ARM OR THE SHARED ROUTING SUCCESSOR:**
`tr_shrink_retry` (`:3610`, read `:4030`,
`:4076`) · `qp` rebuilt if stale (`:3611-3615`) · `overrides.tr_radius` (`:3618`) and
`overrides.dual_mu` (`:3654`, adaptive-mu only) · `row.mu` (`:3656`) ·
`offer_hot` (`:3670`, read `:4214`, `:4230`) · `use_crash` / `crash_seed` (`:3678-3689`, read
`:4216`) · **`warm`** (read `:4214` — the successor passes it, and `warm.hot` is read inside
`solve_with_walk`) · `qs` (`:3702`) · `fallback_report` (`:3705`) · `walk_owns_this_qp` (`:3706`,
**initialised true**) · `ipqp_chain_owns_the_step` (`:3710`) · `row_qp_mode` (`:2702`) ·
`ssn_prox_ingested`
(`:2407`) · `ipqp_ladder` (`:2046`) · `ipqp_analysis_epoch` (`:2056`) · both budget charges (`:2442`,
`:2449`) · `delta`, `seed`, `have_seed`, `ev` · **`seam`** and **`iter`**.

**THE HEADING IS THE SCOPE, and it was widened once and narrowed once.** It read "read inside or
after it", which is TOO BROAD: `x` and `last_dual_mu` are both read later in the major — at the
trial point and at the `finish` sites — and a list headed that way has no reason to exclude them,
which is how they got in. What this list is FOR is the extraction: it is the set an arm or the one
shared successor needs, and therefore the set that has to cross a function boundary. Anything the
rest of the major reads is cut (c)'s and (d)'s business, not this list's.

**CORRECTED AT CUT (b) (2026-09-06), because this list is what (b)'s parameters were named from and
it was wrong in both directions; the `last_dual_mu` entry and the missing `warm` were caught in the
FIX ROUND on that correction, so the list above is the third state, not the second.** Five items,
each re-derived at cut (b)'s BASE `03e1b34` by reading the two arm bodies:

* **`seam` ADDED.** The kIpm arm reads `seam.epoch()` TWICE — the symbolic hoist's epoch gate
  (`structure_epoch_moved`) and the post-solve stamp into `ipqp_analysis_epoch`. The list named the
  epoch and not the object the epoch is read from, so an extraction typed from it alone would have
  had no way to compute either. `seam` is BORROWED (§2.1) and crosses as a parameter.
* **`iter` ADDED.** The kIpm arm reads the major index three times: `set_trace_major(iter + 1)`, the
  escape ladder's `record(ladder_outcome, iter + 1)`, and the engine-invariant throw's message. It
  is the loop variable and is in neither state bundle, so it too crosses as a parameter.
* **`x` REMOVED.** No routing arm reads the iterate. Every `x` inside the two arm bodies is
  `ires.x`, `ipqp_carry_across_majors.x`, `qp.n()` or prose; the arms work on `qp`, `seed` and
  `ires`, and the trial point is built AFTER the dispatch.
* **`last_dual_mu` REMOVED** (`row.mu` stays: the kIpm chain corrects it in the successor block).
  `last_dual_mu` is WRITTEN before the switch (`last_dual_mu = row.mu`) and read only at the
  `finish` sites, never inside the switch or by the successor. **It was struck from the bullet list
  at the first correction but LEFT STANDING in the set above, which is the contradiction the fix
  round closed; the set above no longer names it.**
* **`warm` ADDED.** It was missing altogether. `warm` crosses EXPLICITLY into the shared successor
  (`solve_with_walk(st, mj, warm, …)`) and `warm.hot` is read inside it, so it is a parameter of the
  one function every route can reach. A list that omits it cannot name the successor's signature,
  which is exactly what this list is for.

**Written inside the switch and read after it:** `qs` (every arm or the successor) ·
`walk_owns_this_qp` (`:3919`, `:3994`, `:4007`, `:4156`, `:4168`) · `ipqp_chain_owns_the_step`
(`:4136`, `:4158`, `:4170`) · `row_qp_mode` (`:4159`, `:4171`, then `:4213`) · `fallback_report`
(through `:4190`) · `row` (through `certified_feasibility_fallback`'s by-reference parameter at
`:4189`) · `ssn_budget_charge` (`:3817`, `:3910`, `:3935`, `:4157`, `:4169`) ·
`ipqp_budget_charge` (`:4055`, `:4144`, `:4154`) · `ssn_prox_ingested` (zeroed `:3735` or `:4809`) ·
`ipqp_analysis_epoch` (`:4050`) · `ipqp_ladder` (`:4076`) · `ipqp_staged_seed_` (reset `:4049`) ·
`ssn_prox_*_out_` (`:3826`, `:3841-3845`) · `out.counters` throughout.

### §6.1 The four routes and the ONE shared successor

`walk_owns_this_qp` reaches the successor true from exactly four places:

1. `kWalk` — `:3724-3725`, a bare `break`, no emit.
2. **kIpm retirement** — `:3993-3995`, `break`, **NO emit**.
3. **kIpm domain decline** — `:4004-4008`, `break`, **NO emit** (two counters bumped).
4. **kSsn hand-off** — `:3919` (and, through the kIpm chain, `:4156`/`:4168` when
   `route_through_ssn_warm_grade` returns true).

The successor at `:4212-4229` is that one conditional walk site. A major routed to the walk by (2)
or (3) shows only the successor's `qp.mode kWalk` line — today's behaviour, and it must stay.

Note the successor is preceded by a SECOND conditional at `:4209-4211`
(`if (ipqp_chain_owns_the_step) row.mu = opts_.qp.dual_mu;`) — a mutation between the switch and the
walk. "ONE conditional site after routing" is a statement about the WALK INVOCATION, not about the
number of `if`s in the successor block.

### §6.2 The declared SECOND walk site (the exclusion — brief §5 F3)

At `src/drivers/sqp_driver.cpp:4187-4190`, `certified_feasibility_fallback(engine_, …)` runs the
elastic ladder **on the walk engine, inside the kIpm arm**, assigns `qs`, and leaves
`walk_owns_this_qp` **FALSE**. This is a deliberate second walk invocation (W2's contract: the
ladder's walk is not the dispatch walk, and its `qp.mode` line is the ladder's own, emitted from
`emit_qp_mode_line` at `:1169`/`:1212`). It is **not** one of the four routes the successor serves.
**T6.b must not fold it into the successor.** It stays where it is.

---

## §7. Trace emits, in order, with their position relative to everything else

`ipqp_trace_` is the sink; every emit tests it for null first. Sites reachable from
`solve_impl_body`:

| # | site | event | position |
|---|---|---|---|
| E1 | `:2762` (inside `push_history`) | `sqp.major` | AFTER the caller-unit map (`:2750-2757`), BEFORE `out.history.push_back` (`:2765`) — so `major` is this row's index in the vector, and all ten push sites are covered by ONE emit site. |
| E2 | `:3345` | — | `sub.attach_trace(ipqp_trace_)`: the NESTED restoration solve emits into the SAME sink, between step 9 and step 10 of §5. Everything the sub-driver emits is interleaved here, before the requesting row is pushed. |
| E3 | `:3966-3973` (kSsn arm) | `qp.mode kSsn`, site `dispatch` | END of the arm, AFTER the hand-off decision at `:3919` whose value it reads for `outcome = kRouted : kOptimal`, and BEFORE `break` at `:3975`. |
| E4 | `:4855` (in `route_through_ssn_warm_grade`) | `qp.mode kSsn`, site `ssn_warm_grade` | Emitted INSIDE the callee, which is invoked at `:4156`/`:4168` — therefore **BEFORE** E5/E6 on those two paths. |
| E5 | `:4108` (in `emit_ipqp_route_and_mode`) | `ipqp.route` | fired at `:4145`, `:4160`, `:4172`, `:4192` |
| E6 | `:4114` (same lambda) | `qp.mode kIpqp`, site `dispatch` | immediately after E5, same four call sites |
| E7 | `:4191` | `sqp.fallback_verdict` | AFTER `certified_feasibility_fallback` returns (`:4187-4190`) and **BEFORE** E5/E6 at `:4192` |
| E8 | `:4227` (the successor) | `qp.mode kWalk`, site `dispatch` | after `engine_.solve` at `:4214-4217` |
| E9 | `:4535` (SOC block) | `qp.mode kWalk`, site `soc_resolve` | after `engine_.solve(soc_qp,…)` at `:4525` |

Plus the emits made from inside the kernels: `emit_qp_mode_line` at `:1050` (`run_elastic_ladder`),
`:1169` and `:1212` (`certified_feasibility_fallback`), and the IPQP engine's own trace during
`ipqp_engine().solve` at `:4048`.

**The ordering rule for cut (b), binding:** each routing function **EMITS BEFORE RETURNING**, never
from the caller. The kIpm arm's emits read arm-local state (`ires`, the face census at
`:4088-4096`) that an outcome type would otherwise have to carry, and the one way to break the order
is to return an outcome and emit from the caller after the successor. Emit inside; then
arm-emit → successor-emit is preserved by construction.

**Per-arm emit order to preserve, exactly:**

* kSsn hand-off: E3 (`kRouted`) → E8.
* kIpm refine-accepted: E5+E6 (`kRefine`, `kOptimal`) at `:4145`; no E8.
* kIpm → SSN (both routes, `:4156-4160` and `:4168-4172`): **E4 → E5+E6 (`kSsn`, `kRouted`)**, then
  E8 iff the grade handed off to the walk.
* kIpm escape/fallback: the ladder's own `emit_qp_mode_line` lines → **E7 (`:4191`) → E5+E6
  (`kWalk`, `kEscaped`) at `:4192`**. The fallback verdict precedes the route/mode pair; that order
  is astra's "preserve fallback-verdict → IPQP route/mode order inside that arm".
* kIpm retirement / decline: nothing → E8 only.
* kWalk: E8 only.

The assertions are the W4 trace goldens and the `sqp.major` count == `history.size()` pin.

**THOSE PINS DO NOT ESTABLISH THAT AN ACCEPTED ROW IS EMITTED BEFORE THE COMMIT MUTATES STATE, and
this document said they did** (astra's cut-(c) design review, F3,
`.superpowers/w5-t6-c-design-review-astra.md`). What they pin is the row SCHEMA and BYTES, the row
COUNT, the field VALUES including the caller-scale export, the per-major event bracketing, and the
restoration-seed agreement between history row and stream — every one of them a statement about
CONTENT. Moving the push BELOW the radius growth and the iterate commit would preserve all of it:
the row was already measured, so its values do not change, and neither does the count. Row-value
equality is not a proof of execution order and must not be described as one.

**Two additions close it, and (c) owns both.**

* **A registered ORDERING ASSERTION.** Reuse `ThrowsOnSecondGradientModel`
  (`tests/sqp/test_trace_writer.cpp:2984`, `:3040`): the test today checks nesting recovery, and
  must additionally REQUIRE that the accepted major row is ALREADY PRESENT in the trace when the
  second gradient throws. That is the smallest boundary observation that detects moving emission
  after the direct-accept derivative refresh, and it is a real execution-order assertion rather than
  a value comparison.
* **A SOURCE-ORDER AUDIT, as (c)'s own gate.** The ordering assertion does NOT independently detect
  moving only the SILENT radius and iterate assignments, which throw nothing. Cut (c) therefore
  audits, at source and in its claim, the sequence **emit → return `CommitAccepted` → commit
  updates** at site 10, and states it as a gate the reviewer checks. A narrow internal ordering pin
  is the alternative if executable coverage of those assignments is required; it is not built here.

---

## §8. The ten push sites

### §8.1 The BASE-derived map, with each path's push → check → return ORDER

| # | line | class | path | order after the push |
|---|---|---|---|---|
| 1 | `:2807` | **EARLY EXIT** | non-finite KKT at the iterate | push → output assembled BY HAND (`:2808-2860`, **bypassing `finish`**) → `check_major_pushed_once(iter)` `:2861` → `return out` `:2862` |
| 2 | `:3106` | **EARLY EXIT** | converged / probe-budget exhausted / `max_iter` (`:3104-3105`) | push → `probe_budget_stops` `:3111` → **budget-mode branch**: `best_is_current` `:3125` → check `:3126` → `return finish(… kBudgetExhausted, mb_best_*)` `:3127-3132`; **otherwise** check `:3139` → `return finish(… converged ? kOptimal : kMaxIter …)` `:3140-3145` |
| 3 | `:4330` | **RECOVERY** | elastic ladder UNUSABLE (`:4298`) → `enter_restoration(elastic_cand)` `:4329` | restoration → push → `if (elastic_restored) continue` `:4331-4333` → else check `:4339` → `return finish(…, restoration_exit_*, …)` `:4340-4346` |
| 4 | `:4404` | **RECOVERY** | **RETRYABLE QP failure** (`:4392`) with the radius **at its floor** (`:4401`) → `enter_restoration()` (NO candidate) `:4403` | restoration → push → `if (restored) continue` `:4405-4407` → else check `:4413` → `return finish(…, restoration_exit_*, …)` `:4414-4420` |
| 5 | `:4422` | **RECOVERY** | retryable QP failure, shrink-and-retry, no restoration | push → `delta = shrunk_radius(delta)` `:4423` → `seed = std::move(qs)` `:4432` → `seed.x.setZero()` `:4433` → `have_seed = true` `:4434` → `continue` `:4435` |
| 6 | `:4437` | **EARLY EXIT** | QP failure, retries exhausted / not retryable | push → check `:4442` → `return finish(… map_status(qs.status), kkt, row.f …)` `:4443-4446` |
| 7 | `:4630` | **RECOVERY** | trial REJECTED (`:4612`) with the radius **at its floor** (`:4625`) → `enter_restoration({&x_trial, &ev_trial})` `:4629` | restoration → push → `if (restored) continue` `:4631-4633` → else check `:4638` → `return finish(…, restoration_exit_*, …)` `:4639-4645` |
| 8 | `:4647` | **RECOVERY** | trial rejected, shrink | push → `delta = shrunk_radius(delta)` `:4648` → `seed = std::move(qs)` `:4649` → `seed.x.setZero()` `:4650` → `have_seed = true` `:4651` → `continue` `:4652` |
| 9 | `:4660` | **RECOVERY** | verdict `kRestore` (`:4655`), **NO floor condition** → `enter_restoration({&x_trial, &ev_trial})` `:4659` — the **ORIGINAL measured trial** | restoration → push → `if (restored) continue` `:4661-4663` → else check `:4666` → `return finish(…, restoration_exit_*, …)` `:4667-4672` |
| 10 | `:4675` | **ACCEPTED COMMIT** | the step is accepted | push **FIRST**, then radius growth `:4689-4693`, then the iterate/multiplier/seed commit `:4695-4722`, then counters `:4723`, `rejections_at_iterate = 0` `:4724`, `subproblem_is_stale = true` `:4725`, `seed.x.setZero()` `:4729`, `have_seed = true` `:4730`, loop |

**3 early exits (1, 2, 6) / 6 recoveries (3, 4, 5, 7, 8, 9 — four through restoration, two by
shrink-and-retry) / 1 accepted commit (10).**

The T6.c rules map onto them exactly: restoration sets `row.restoration_seed_used` (`:3278`) before
sites 3/4/7/9 push; site 10 pushes before every radius/iterate update; `check_major_pushed_once`
guards every `return` out of the loop. The two exactly-once checks are `push_history`'s own
`rows_pushed == history.size()` (`:2769`) and `check_major_pushed_once` (`:2781`), with the
loop-entry check at `:2681`. All three survive as they are.

### §8.2 astra's §7 corrected push map, VERBATIM

Reproduced from `.superpowers/w5-t6-plan-review-astra.md` §7. **This is the T6.0 table**
(brief §6 G2); §8.1 is its BASE-verified expansion. As in §3, the table CONTENT is verbatim and only
the absolute link targets were converted to repo-relative.

> | Push site | Classification and continuation |
> |---|---|
> | [2807](src/drivers/sqp_driver.cpp:2807) | Early exit; manual output assembly → terminal check → return, bypassing `finish`. |
> | [3106](src/drivers/sqp_driver.cpp:3106) | Early exit; convergence/probe/major budget → terminal check → appropriate `finish`. |
> | [4330](src/drivers/sqp_driver.cpp:4330) | Recovery after unusable elastic ladder; restoration → push → continue, or check → finish. |
> | [4404](src/drivers/sqp_driver.cpp:4404) | Recovery after retryable QP failure at floor; same restoration ordering. |
> | [4422](src/drivers/sqp_driver.cpp:4422) | Recovery; push → shrink → move `qs` into seed and recenter → continue. |
> | [4437](src/drivers/sqp_driver.cpp:4437) | Early exit; push → check → finish failed QP. |
> | [4630](src/drivers/sqp_driver.cpp:4630) | Recovery from rejected trial at floor; measured-candidate restoration before push. |
> | [4647](src/drivers/sqp_driver.cpp:4647) | Recovery; push → shrink → replace/recenter seed → continue. |
> | [4660](src/drivers/sqp_driver.cpp:4660) | Recovery for `kRestore`; original measured trial, no floor condition. |
> | [4675](src/drivers/sqp_driver.cpp:4675) | Accepted commit; push precedes radius, iterate, derivative-refresh, and seed updates. |
>
> That confirms **3 early exits / 6 recoveries / 1 accepted commit**.

The SQP-lane skeleton's rows 4 and 9 were wrong and are superseded by this table: #4 follows a
retryable QP failure at floor, not an infeasible-QP result; #9 handles `kRestore` with no floor
condition and passes the ORIGINAL measured trial (not the SOC-corrected one).

**BINDING QUALIFICATION ON THE LAST ROW — the historical table OVERSTATES the derivative ordering**
(astra's cut-(c) design review, F1, `.superpowers/w5-t6-c-design-review-astra.md`). The quoted row
reads "push precedes radius, iterate, derivative-refresh, and seed updates", and the
derivative-refresh half is true of ONE of the two acceptance routes only. What binds cut (c) is:

> **Direct-accept derivative refresh FOLLOWS the push (`:5048`); promoted-SOC refresh remains BEFORE
> the push (`:4907`). Both iterate commits and radius growth follow the push.**

The table above is left VERBATIM because it is a quoted artefact; this qualification is what (c) is
held to, and a claim that repeats the unqualified sentence is wrong about the SOC route.

---

## §9. The SSN deferred refinement — one producer, two mutually exclusive consumers

Guarded by `sres.certification_deferred` (`:3786`), which `ssn_options` sets from
`opts_.ssn_certify_from_face` (`:4773`).

* **Producer** — the hoisted `engine_.refine_on_face(qp, r5_face, ssn_overrides, r5_refined)` at
  `src/drivers/sqp_driver.cpp:3791`, with `r5_have = true` at `:3792`.
* **Resolution, inside the arm**: `discard_deferred_certification()` at `:3803` (took) or `:3808`
  (exit not usable); `finish_deferred_certification(&sres)` at `:3805` (refused). **`:3805` may
  change usability, so it must stay before the later gate at `:3827` and before the counter fold at
  `:3811`.**
* **Consumer A — usable result**: `:3873-3875`, `took = r5_took; refined = std::move(r5_refined);`
  — do NOT refine again.
* **Consumer B — withdrawal/hand-off charge**: `:3934-3935`,
  `charge_refused_face_refinement(out.counters, r5_refined, ssn_budget_charge)`.

A and B are on opposite sides of the `ssn_exit_is_a_usable_step` branch at `:3827`/`:3911`, so there
is exactly **one consumption per execution**. The extracted SSN routing function OWNS finish/discard,
and its outcome carries `sres`'s counters already charged. Nothing after the switch reads a deferral.

Deferral is **forced off** on the IPQP→SSN warm-grade route: `sopts.defer_certification = false` at
`src/drivers/sqp_driver.cpp:4813`, inside `route_through_ssn_warm_grade`, because the kSsn arm's
hoisted face solve — the thing that owns a pending certification — is not on that route. This must
stay.

**The pin T6.b's claim names**:
`tests/sqp/test_sqp_driver.cpp:8179`,
`TEST(SqpDriverSsnMode, ARefusedFaceRefinementIsChargedEvenWhenTheCertificateIsWithdrawn)`.
(`:8178` as first written, and §12 item 10's gloss with it, were off by one in the other direction —
see §12 item 16. The file is byte-identical at `50f616a` and at cut (b)'s BASE `03e1b34`, so the
number is the same at both.)

### §9.1 REGISTERED PIN — the duplicated producer no driver counter can see

**Owner: the T7 test round, and BEFORE any `ssn_certify_from_face` default flip.** Registered at the
T6.b review (Codex D5 and the SQP lane §4, which reached the same finding independently). NOT built
at (b) or (c) — it is a test-side pin, it is inert at the shipped default, and neither cut touches
the SSN arm, which is now a function.

**The gap.** Falsifier 2 of the T6.b pin — consumer A re-runs the producer and DROPS the hoisted
result unread — moves NO driver counter, and the suite passes 87/87 with the mutation in place. That
is not a weak test; it is a property of where the counters come from. The driver FOLDS its counts out
of the returned `QpSolution` (`refined.counters.factorizations`), and `refine_on_face` RESETS its
output counters on entry (`src/qp/qp_engine.cpp:77`) and records only that call
(`:160`). A dropped result therefore drops its count, by construction. No existing
cumulative driver counter can see the duplication.

**The counter that DOES see it is one layer down.** The linear layer counts work DONE, not work
READ: `SymmetricFactor`'s `factorize_count` (`include/hven/linear/symmetric_factor.h:656`) is
incremented once per `factorize()` that reached the backend
(`src/linear/symmetric_factor_mkl.cpp:678`), and the golden rig already pins on it
(`tests/golden_rig/traces_sqp.cpp:90,156,250,348,626`). A doubled `refine_on_face` doubles it.

**What has to be built.** A test-side path from the driver's walk engine to its factor's counters —
an `HVEN_TESTING` seam in the Apache-2.0 adapter (CLAUDE §6/§8), or an engine accessor; the rig has
its own handle and is the model — asserting on the accepted-deferred path that
`factorize_count` delta == `ssn_refine_factorizations` delta.

**Why the deadline is the flip, not just T7.** `ssn_certify_from_face`'s default flip already carries
one stated precondition from the Phase-7 record (a weak-active-indefinite fixture). This is a SECOND
one: the deferral path is what the flip turns on, and it is the path with no cumulative observer.

---

## §10. Cut-by-cut disposition and the hazards each one owns

### §10.1 Cut (a) — `prepare_solve(SolveState &, …)`

`:2030-2678` moves. Caller-owned `SolveState`, initialised in place; never a returned
self-referencing object. Hazards: `full_step_funnel` and `strategy` (§2.2 O, M) are pointer-identity
members; `solve_scaling` is a const COPY, not a seam reference (§3); the setup-only probes
(`:2253`, `:2260`) stay local to `prepare_solve` and do not join any state type.
P-SYM claim: `solve_impl_body` itself DIFFERS; `prepare_solve` is ONLY-AFTER; everything else
identical.

### §10.2 Cut (b) — SSN and IPQP routing

Extract each arm as a function with explicit input/output state. `kWalk` is a `break`; the walk
invocation is the shared successor (§6.1). Hazards: the second walk site (§6.2) is excluded by
contract; routing functions emit BEFORE returning (§7); the deferred refinement must not run twice
(§9). Replay 0/75 on **all three arms** — this cut touches all three modes.

**FIVE MEMBERS NO EXTRACTED FUNCTION MAY WRITE (cut (a) fix1, from the Claude-substitute F-M3).**
`n`, `ssn_mode`, `warm_state_ingest` and `resolved_level` were `const` LOCALS at BASE, so a write to
one was a compile error. Cut (a) makes them non-`const` members that `solve_impl_body` reads once
into `const` copies, and `solve_scaling` a `const &`. Nothing writes any of the five today — over
BASE `:2680-4733` they have 3, 1, 2 and 1 uses and all are reads — but a function extracted at (b)
or (c) taking `SolveState &` COULD write `st.n`, and the loop's copy would not see it. **(b)'s and
(c)'s P-SYM claim names these five as write-forbidden**, and the alternative (taking them as
`const &`) is available only with a neutrality argument, since a const reference into an object
whose address has escaped may cost reloads the copies do not.

**WHAT CUT (b) ACTUALLY BUILT (landed 2026-09-06), so cut (c) inherits a written record rather than
a reading of the code.** `MajorState` exists and holds the FOUR ROUTING OUTPUTS — `qs`,
`fallback_report`, `walk_owns_this_qp`, `ipqp_chain_owns_the_step` — and it is constructed WHERE
THOSE FOUR WERE DECLARED, at the dispatch, NOT at the loop top. The reason is the
construction-timing paragraph immediately below, applied to the bundle itself: all four are declared
after push sites 1 and 2, either of which can leave the major, so a bundle built at the loop top
would construct a `QpSolution` on paths that never build one today. The table's remaining per-major
names — `kkt`, `row`, `row_qp_mode`, `caller_row` and the trial/SOC objects — do NOT join it at (b);
they cross into the routing functions as explicit parameters and join the bundle at **cut (c)**,
which is where the loop top, the three lambdas and the ten push sites are restructured and where the
bundle's construction therefore moves to the loop top. `row_qp_mode` in particular cannot join at (b)
at all: it is live from the loop top (`push_history` reads it) and a bundle built at the dispatch
cannot hold it.

**Construction timing (astra G6).** Putting the trial/SOC objects into `MajorState` must not
eagerly evaluate or touch an engine before the existing early exits. `x_trial` (`:4473`) and
`ev_trial` (`:4474` — a REAL model call, `seam.eval_nlp_values`) are constructed *after* push sites
1, 2, 4, 5 and 6, all of which can leave the major. `ev_soc`/`qs_soc`/`x_soc` (`:4504-4506`) are
default-constructed and filled only inside the SOC block. A `MajorState` whose constructor built any
of these would move an evaluation earlier and change `evals_values` on those paths.

**Reads after moving `fallback_state` (astra G6).** `fallback_report` is MOVED into a local
`report` at `:4291-4295` (`std::move(*fallback_report)`), and its SCALAR diagnostics are read
LATER at `:4360-4363` (`fallback_report->qp_minor_iters`, `fallback_report->qp_factorizations`).
The optional still has a value; the report inside it is moved-from, and only the scalars survive.
A routing outcome that reshapes this must preserve those fields and the optional's engagement.
`rho0_ceiling_hit` is read from the optional at `:4280` and re-read from `report` at `:4296`.

### §10.3 Cut (c) — the ten push sites and restoration

Separate MAJOR and RESTORATION outcome types (continue / finish / accepted-step commit). Each path's
push/check/return order preserved exactly as §8.1 writes it. `row.restoration_seed_used` set before
the row is emitted; accepted rows emitted before radius/iterate updates — subject to §8.2's binding
qualification on the two acceptance routes; both exactly-once checks stay; trace emit ORDER
unchanged (§7), with §7's registered ordering assertion and source-order audit as (c)'s own gate.
The restoration outcome obeys §5 in full.

#### §10.3.1 THE OUTCOME-TYPE CONTRACT — astra's cut-(c) design review, item 3, VERBATIM

From `.superpowers/w5-t6-c-design-review-astra.md` item 3 (its F2). The table CONTENT and the code
are verbatim; only the absolute link targets were converted to repo-relative. **This is the contract
cut (c) is dispatched against**, and it settles what §10.3's one-line "continue / finish /
accepted-step commit" left open: how `Finish` distinguishes manual assembly, budget-best output,
ordinary output, failed-QP output, and the two restoration activity sources.

> I recommend two payload-free discriminants:
>
> ```cpp
> enum class RestorationOutcome { Refused, Resumed, Exited };
>
> enum class MajorOutcome {
>     Continue,
>     CommitAccepted,
>     FinishManual,
>     FinishCurrent,
>     FinishBudgetBest,
>     FinishQpFailure,
>     FinishRestorationSeed,
>     FinishRestorationQp
> };
> ```
>
> `MajorOutcome` encodes the three requested action classes, with finish provenance encoded in the
> same tag. An equivalent `Kind` plus finish-only `FinishRoute` representation is reasonable; the
> single discriminant avoids invalid combinations.
>
> What crosses **by value** is the control decision. Neither outcome needs a `SqpSolution`, vector,
> evaluation, KKT, objective, warm start, or restoration flag.
>
> - `st.resto` retains `used`, `status`, `kkt`, `f`, `multipliers_are_caller_scale`, and `moved_x`,
>   as implemented at [driver:2053](src/drivers/sqp_driver.cpp:2053).
> - `st.fs` and `st.mb` retain their independent snapshots; neither belongs in an outcome.
>   [driver:2074](src/drivers/sqp_driver.cpp:2074), [driver:2096](src/drivers/sqp_driver.cpp:2096)
> - `MajorState` retains requesting measurements, convergence decision, dispatch outputs, trial/SOC
>   objects, context and SOC-selection flag through caller dispatch.
>
> The finish refinements are necessary information if finalization moves to the caller:
>
> | Finish tag | Caller selects |
> |---|---|
> | Manual | Already assembled `st.out`; check, then move it. |
> | Current | Current iterate/multipliers, major KKT and row objective; status from preserved convergence decision. |
> | BudgetBest | `st.mb` vectors/KKT/objective; compute `best_is_current` before checking, preserving the existing warm-start probe arguments. |
> | QpFailure | Current measurements, mapped `mj.qs.status`, activity from `mj.qs`. |
> | RestorationSeed | `st.resto` payload; activity from `st.seed` only when `!moved_x && have_seed`. |
> | RestorationQp | `st.resto` payload; activity from `mj.qs` only when `!moved_x`. |
>
> Do not merge the last two by assuming every restoration requester has usable `qs` activity: site 3
> deliberately falls back to the prior seed. Preserve `finish` itself, including its scaling copy,
> multiplier-scale handling and restoration bound-price exception.
> [driver:4676](src/drivers/sqp_driver.cpp:4676), [driver:5316](src/drivers/sqp_driver.cpp:5316),
> [driver:5341](src/drivers/sqp_driver.cpp:5341), [driver:5428](src/drivers/sqp_driver.cpp:5428)
>
> The doc leaves finalization placement open. **Recommend caller finalization after an emitted-row
> outcome**, with manual assembly completed before returning its tag. Keep the terminal check before
> every actual solve return and before `make_warm_start`/`finish` on ordinary terminal paths.

**THE PER-SITE OUTCOME COLUMN**, from the same review's item 1 table, against §8.1's ten sites.
The line numbers here are astra's, taken at `f9ca2bd`; §8.1's are the BASE-derived ones.

| site (§8.1 #) | HEAD push | outcome to return / caller action |
|---|---|---|
| 1 | `:3615` | `FinishManual` after assembly; caller checks and moves `st.out`, bypassing `finish`. |
| 2 | `:3917` | `FinishBudgetBest` or `FinishCurrent`; caller preserves those respective sequences. |
| 3 | `:4663` | `Continue` or `FinishRestorationSeed`. |
| 4 | `:4737` | `Continue` or `FinishRestorationQp`. |
| 5 | `:4755` | `Continue` after shrink → move `qs` into seed → zero primal → `have_seed`. |
| 6 | `:4770` | `FinishQpFailure`; caller checks, then finalizes using this QP's activity. |
| 7 | `:4963` | `Continue` or `FinishRestorationQp` (restoration on the ORIGINAL `{x_trial, ev_trial}`). |
| 8 | `:4980` | `Continue` after shrink → move `qs` into seed → zero primal → `have_seed`. |
| 9 | `:4993` | `Continue` or `FinishRestorationQp`. |
| 10 | `:5008` | `CommitAccepted` IMMEDIATELY after the push; the caller performs the commit sequence while `MajorState` is still alive. |

**THE RESTORATION SIGNATURE, and the decide/run split stays REGISTERED** (astra item 4). The bare
`(SolveState &, MajorState &, RestorationOutcome &)` shorthand at §4 is INCOMPLETE with the bundles
as defined: the closure also needs the borrowed seam and model, the major index, and its by-value
candidate descriptor, and it reaches the driver through `this` for options, restoration permission,
the trace sink and the restart-radius policy. The complete private member interface is:

```cpp
RestorationOutcome enter_restoration(
    SolveState &, MajorState &,
    AggregateEvalSeam &, NlpModelAggregate &,
    Index iter, RestorationCandidate cand = {});
```

An output-reference form is equivalent; returning the enum is simpler. The candidate's two pointers
stay temporary borrows and the pointed-to evaluation is consumed only at the existing conditional
move (§5, G1). **Adding these explicit parameters resolves the captures WITHOUT splitting decision
from execution: the decide/run split remains REGISTERED and outside (c).**

#### §10.3.2 CUT (c) IS TWO COMMITS (astra item 6)

A single commit is mechanically possible; two is what (c) does.

1. **Outcome types, restoration extraction and caller dispatch**, consuming the aliases that belong
   to the extracted blocks.
2. **The alias removal** — the remaining `solve_impl_body` bindings replaced by direct `st.` / `mj.`
   accesses — **with its own claim: `solve_impl_body` is the ONLY intentional code difference**,
   file-local layout movement subject to the established P-SYM rules (§11.4, and the tool's own
   class (c)).

Splitting the commits relaxes nothing: each verifies the five write-forbidden members (§10.2), and
each compares against its IMMEDIATE PREDECESSOR **and** cumulatively.

**"NO ALIAS SURVIVES (c)" MEANS THE TRANSITIONAL ALIASES IN `solve_impl_body`, AND NOTHING ELSE.**
The 53 `SolveState` bindings of the body's prologue and the two per-major `mj.` aliases cut (b)
declared in advance are the set. The routing functions cut (b) created have their OWN local aliases
(e.g. `src/drivers/sqp_driver.cpp:3141`) and those STAY: if the rule meant the whole TU, removing
them would edit those functions too, and a body-only P-SYM claim would then be false.

#### §10.3.3 WHAT CUT (c) ACTUALLY BUILT (landed 2026-09-06)

Written here for the reason §10.2 was: so cut (d) and the reviewers inherit a record rather than a
reading of the code, and so the places where the code and this document differ are stated rather
than left to be discovered.

**A THIRD FUNCTION, and it is the one decision the dispatch left open.** The per-major body is now
`SqpDriver::run_major(SolveState &, MajorState &, AggregateEvalSeam &, NlpModelAggregate &,
const WarmStart &, Index minor_budget, Index iter)`, returning `MajorOutcome`; `solve_impl_body`
keeps the loop, the bundle, the terminal check and the outcome dispatch, and goes from 1653 lines to
225. §10.3.1's contract is stated in terms of a CALLER that finalises and holds `MajorState` alive
while it does, which requires the push and the finalisation to be in different scopes; and the ten
push sites sit at four nesting depths, so a common dispatch reachable from all of them needs a
function boundary and `return`. That is the shape built.

**WHAT IS ON `MajorState`, AND THE MEMBERSHIP TEST IT WAS BUILT TO.** §2.3 is headed "the POST-(c)
TARGET" and lists more names than joined. The test actually applied is §10.2's own, one step on: a
name is on the bundle iff it CROSSES the `run_major` → caller boundary. Joined at (c): `kkt`, `row`,
`row_qp_mode`, `caller_row`, the trial/SOC objects (`x_trial`, `ev_trial`, `ctx`, `soc_applied`,
`ev_soc`, `qs_soc`, `x_soc`) — §10.2's list — **plus `converged`**, which is one name beyond it and
is there because the caller's ordinary terminal exit reports `kOptimal : kMaxIter` from it. NOT
joined, because none of them crosses: `tr_shrink_retry`, `overrides`, `adaptive_mu_active`,
`offer_hot`, `use_crash`, `probe_exhausted`, `elastic_applied`, `rho0_ceiling_hit`, `x_scale`,
`zero_step`, `verdict` (all `run_major` locals) and `actual_df` (a caller local). The three lambdas
of §2.3's table are not members either: `measure_iterate` and `push_history` are `run_major` locals,
`check_major_pushed_once` is a `solve_impl_body` local, and `enter_restoration` is a member
FUNCTION. `RestorationCandidate` is a private nested type of `SqpDriver`.

**MEMBER ORDER ON `MajorState` IS LOAD-BEARING, and this is the finding cut (c) lands.** Cut (b)'s
four routing outputs must stay FIRST and in their original order. `route_through_ssn_tier` is not
edited by (c) and reaches `mj.qs` and `mj.walk_owns_this_qp` BY OFFSET; the first arrangement, which
put (c)'s members ahead of them, moved `qs` off offset 0 and turned `mov %rbx,%rdi` into
`lea 0x118(%rbx),%rdi` at three sites — **two extra instructions in a function nobody edited**,
measured by a full two-arm disassembly audit (910 lines each, 37 changed, every one of them a member
offset, that address form, the register renaming it forces, or a reshaped pad). Fixed by ordering,
not excused; the reason is in the struct's own banner so a later insertion does not undo it.

**THE CONSTRUCTION-TIMING HAZARD IS DISCHARGED, NOT WAIVED.** The bundle is built at the LOOP TOP,
which is what §2.3 always said and what (b) could not do. Nothing on it evaluates anything when it
is constructed: `ev_trial` is the one member a model call ever fills, and it is default-constructed
at the loop top and ASSIGNED at the point the call has always been made — after the early exits — so
no evaluation moves earlier and no counter moves. Everything else is empty vectors, an empty
optional and scalars. `route_through_ipqp_tier` and `solve_with_walk` consequently stop taking `row`
and `row_qp_mode` as parameters.

**TWO SPELLINGS DIFFER FROM §10.3.1's QUOTED CODE, AND NOTHING ELSE DOES.** The enumerators are
`k`-prefixed (`kRefused`/`kResumed`/`kExited`, `kContinue`/`kCommitAccepted`/`kFinishManual`/
`kFinishCurrent`/`kFinishBudgetBest`/`kFinishQpFailure`/`kFinishRestorationSeed`/
`kFinishRestorationQp`), because CLAUDE.md §4 puts compile-time constants in `kPascalCase` and every
enum in this tree follows it — and because this document itself writes the restoration values two
different ways (§5 `{REFUSED, RESUMED, EXITED}`, §10.3.1 `{Refused, Resumed, Exited}`), so the
spelling is not the contract. Membership, count and semantics are astra's exactly. And
`enter_restoration`'s default argument is spelled `{nullptr, nullptr}` rather than `{}`: a default
argument in the enclosing class's own body may not reach a nested class's default member
initializers, and clang refuses `= {}` outright. The type keeps its two initializers.

**§7's REGISTERED ORDERING ASSERTION IS BUILT**, inside
`JsonLinesTraceSink.ResetNestingRecoversASinkAfterASolveThatThrewMidBody`
(`tests/sqp/test_trace_writer.cpp`): the accepted major's row must ALREADY be in the stream when the
second gradient throws — exactly one `sqp.major` line, `trial` 0, `major` 0, an accepting `verdict`.
Falsified in the round: inverting the emit with the direct-accept refresh fails it. §7's other
addition, the source-order audit, is in `.superpowers/w5-t6-c-report.md` §1.6 with line numbers.

### §10.4 Cut (d) — the kernels TU (EXPERIMENT with a VETO)

Extract WITHIN the TU first (a–c are that); then the separate TU as an **independently revertible**
commit with the CLAUDE.md §5 proof. **The moving set is exactly §1.7's "KERNELS" rows plus anonymous
namespace #2's three kernels-only helpers** — `run_elastic_ladder`, `certified_feasibility_fallback`,
`elastic_initial_rho`, `elastic_evidence_seed`, `emit_qp_mode_line` — with `trace_outcome_of` (§1.2)
the ONE symbol that changes linkage: it gains external linkage, a declared surface, and goes in the
migration guide. `predicted_decrease` is shared across the cut but already declared, so it costs no
new surface (§1.7). Build integration per §1.6. The two undeclared helpers (§1.4, `:779` and `:805`)
are decided here.

**THE TWO UNDECLARED HELPERS ARE DECIDED (§1.4): both keep their bodies in the driver TU and gain
INTERNAL linkage, in a SEPARATELY CLAIMED PREPARATION COMMIT that lands BEFORE the TU commit.**
Astra's claim for it, VERBATIM, and it is the claim the preparation commit's P-SYM run is judged
against:

> Only `assert_ssn_warm_grade_window` and `assert_ipqp_hand_off_window` change linkage, remaining in
> the driver TU with statement-identical bodies. Their external symbols disappear; corresponding
> internal bodies may appear or be inlined away. Their sole callers,
> `route_through_ssn_warm_grade` and `route_through_ipqp_tier`, may differ only through those named
> linkage/optimization changes. Every other body is unchanged, subject to established P-SYM noise.
> State layouts and `route_through_ssn_tier` remain unchanged. Runtime neutrality is separately
> verified.

**THE WITHIN-TU EXTRACTION THE PLAN ASKS FOR IS ALREADY DONE.** Cuts (a)–(c) ARE that step, as this
section has said from the start; astra confirms it. No redundant extraction commit is required, and
none is to be invented.

#### §10.4.1 The mapped-body / literal audit, made concrete

P-SYM alone cannot prove the semantic identity of moved bodies, literals or static initialisation
(astra F4/G4): cut (d) supplements it with a mapped source-body / literal audit and an audit of any
moved static initialisation or tables. Astra's item 6 makes the instrument concrete, and its four
parts are binding.

**(i) A NORMALISED STATEMENT-LEVEL COMPARISON of all SIX moved definitions**, `elastic_evidence_seed`'s
`tight` lambda (`:909`) INCLUDED — the lambda is a definition inside a definition and is easy to lose
in a whole-function diff. Normalise comments, whitespace, and the explicitly declared namespace
changes, and NOTHING ELSE: expression order, branches, constants, copies, moves and initialisation
are all load-bearing and any difference in them is a finding, not a normalisation.

**(ii) EXCLUSIVE LITERALS MIGRATE; SHARED VALUES NEED NOT LEAVE THE DRIVER OBJECT.** This is astra's
one amendment to the instrument as first proposed. The kernel-specific diagnostic strings at `:995`,
`:1001` and `:1152` are exclusive to the moved bodies and travel with them; `predicted_decrease`'s
diagnostic at `:587` stays with its driver-side definition. A value that both objects legitimately
use stays in both.

**(iii) NO ONE-FOR-ONE `.LCPI` LABEL TRANSFER IS REQUIRED.** Pooling, duplication and materialisation
can all change across a split. What must be preserved is the mapped data's **meaning and bytes**,
with every disappearance, addition and retained shared value EXPLAINED. LOCAL-FAMILY counts are
reconciliation evidence, not the literal proof.

**(iv) THE SIX NAMED CONSTANTS DO NOT MOVE.** The source region contains no kernel-owned static
local, no mutable file-static table and no dynamic initialiser; its non-enumerator named constants
already live in a header and stay there:

| constant | value | existing home, unchanged |
|---|---:|---|
| `kElasticRhoInit` | `1e2` | `include/hven/detail/globalization/sqp/elastic.h:47` |
| `kElasticRhoMax` | `1e8` | `elastic.h:48` |
| `kElasticRhoFactor` | `10.0` | `elastic.h:49` |
| `kElasticRhoDualMuSafety` | `1e-2` | `elastic.h:71` |
| `kElasticStallScale` | `1e-12` | `elastic.h:78` |
| `kNoSlack` | `-1` | `elastic.h:81` |

**(v) THE HEADER-GENERATED INITIALISERS ARE NAMED IN THE CLAIM — the case a source-only "no statics"
reading misses.** Two variables initialise dynamically out of headers the new TU will also include:

* `hven::solvers::detail::kSsnComplementarityFactor` (`include/hven/detail/qp/ssn_engine.h:455`);
* `hven::solvers::kSsnTrViolationFactor` (`include/hven/drivers/sqp_driver.h:1756`).

The driver object emits TWO `__cxx_global_var_init` functions keyed to them today. Including the same
header in `sqp_kernels.cpp` may emit additional GUARDED copies. The claim must name them and the
audit must verify variable IDENTITY, the guards, the initialisation dependency and the linker
coalescing — and must show that **no independent state is created** and that no startup instruction
is misattributed to solver-call overhead.

**(vi) AND THE SHARED fmt CONSTANT TABLES ARE CENSUSED** for the same reason, since moved formatting
code carries them: `digits2::data` (`dep/fmt/include/fmt/format.h:1036`), the Dragonbox power table
(`format-inl.h:375`) and `is_printable`'s singleton/normal tables (`format-inl.h:1795`). Identical
constant/COMDAT copies in the second object are PERMISSIBLE; an independently duplicated MUTABLE
static is a finding.

> the earlier carve's preserved-inlining record is history, not proof

---

## §11. The (d) measurement protocol, pinned here (astra G3 / brief §6 G3)

### §11.1 The acceptance bands, and the gap that is now closed

* **FLAT** — per-cell median ratio in **0.99–1.01** AND whole-corpus within **±0.5 %**, in all three
  modes, on BOTH legs.
* **MOVED** — whole-corpus beyond ±1 % in any mode, OR ≥ 3 cells outside 0.98–1.02 with the same
  sign in all three alternating runs.
* **The gap is closed by ruling**: **any result that is not FLAT is UNRESOLVED pending
  re-measurement.** A reproducible slowdown — three solo runs, same sign — on ANY cell of ANY mode
  on EITHER leg is a **veto trigger**, regardless of the corpus total. "Within noise, direction
  noted" is a description, never a pass.

* **LAYOUT-MOVED vs WORK-MOVED — the owner's amendment, ruled 2026-09-06 on cut (a)'s numbers
  (relayed by the settler; source: the SQP lane's T6.a review §9).** A non-FLAT result is not by
  itself a redraw trigger, because two different things can produce one and only one of them is a
  cost the code chose. Every bench leg therefore carries `perf stat -e
  instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u` BESIDE wall time, on
  the same discipline as the timing (3× alternating, solo, pinned).
  * **LAYOUT-MOVED** — instruction and branch counts identical within **1e-4**, and the cycle delta
    accounted for by the icache/branch-miss deltas. **Reported with its numbers; NOT a redraw
    trigger.** The machine is fetching the same work from a different place.
  * **WORK-MOVED** — instructions UP. **Remains the veto**, unchanged.
  A non-FLAT result that is neither — instructions flat but cycles unexplained by the miss counters
  — stays UNRESOLVED pending re-measurement, as above.

* **AND THE BAR IS CUMULATIVE.** Per-cut LAYOUT-MOVED verdicts do not compose: three of them at
  +0.7 % each are a 2 % regression that no per-cut rule catches. **At T6's close the post-T3 base
  (`50f616a`) is benched against the post-(d) head under the same recipe and must hold the corpus
  bar (±0.5 %).** If it does not, Grant rules again, with the numbers.

> "informational" does not waive neutrality

#### §11.1.1 THE PRECEDENCE, and the disposition table (astra item 4, BINDING)

The bands above did not completely determine a disposition: "instructions UP", the `1e-4` identity
tolerance, FLAT timing and owner discretion overlapped, so two different readings of one measurement
could both be defensible. Astra's item 4 closes that, and the two overlaps are resolved as it
recommends.

**OVERLAP 1 — does demonstrated added work trigger the veto even when the timing meets FLAT?
ANSWER: YES. There is NO automatic KEEP.** A bounded, attributable instruction increase from call
overhead is WORK-MOVED under "instructions UP". Being expected, being small, or being accompanied by
fewer cache misses does not turn it into layout noise.

**OVERLAP 2 — does "instructions UP" mean any measured positive ratio, or a reproducible increase
distinguished from the `1e-4` identity tolerance? ANSWER: the latter, in BOTH directions.** Sampling
variation inside the identity band is not classified as work; and demonstrated executed call
overhead is not hidden inside that band either. The attribution is RECORDED, and where the
distinction remains unresolved it goes to the owner rather than being settled by the implementer.

**AND BUILD BENEFIT NEVER PURCHASES RUNTIME WORK.** CLAUDE.md §5's "never at measurable runtime
cost" outranks its "use separate TUs" clause, in this document as in the file it comes from.

"Build benefit" below means a PREDECLARED, reproducible wall-clock benefit with an acceptable
measured RSS result, on the threshold fixed at §11.4.1 BEFORE any (d) number was read.

| result after required evidence | build-side result | disposition |
|---|---|---|
| Correctness, mapped-body, linkage, effective-flags, or control-symbol failure | Any | No KEEP. Correct the candidate and repeat proof; REDRAW or ABANDON if the boundary cannot satisfy it. |
| Missing leg, unexercised affected path, unstable calibration, missing build measurement | Missing/inconclusive | **UNRESOLVED**; complete or repeat measurement. |
| c→d FLAT in every mode on both legs; no demonstrated added work; cumulative corpus inside ±0.5% | Demonstrated benefit | **KEEP recommendation**, presented under §11.5's owner disposition. |
| Non-FLAT, instructions/branches within identity band, placement accounting closed by passes A/B; cumulative inside bar | Demonstrated benefit | **KEEP eligible under the owner's LAYOUT-MOVED amendment**; report all cell movements. Non-FLAT alone does not force redraw. |
| Demonstrated instructions UP, including call overhead, whether timing is FLAT or non-FLAT | Any | **WORK-MOVED / veto. Owner receives numbers. REDRAW** if a concrete alternative removes the cost; otherwise **ABANDON** by reverting the TU commit. No automatic KEEP exception. |
| Instructions/branches flat, cycle movement unexplained | Any | **UNRESOLVED**; repeat passes A/B. If still unexplained, owner finding; neither automatic KEEP nor automatic REDRAW. |
| Instruction decrease outside the identity band, or another result fitting neither defined class | Any | Owner classification after semantic checks. The existing taxonomy does not cover it explicitly. |
| Any local KEEP candidate, but cumulative corpus outside ±0.5% in any required comparison | Any | **Owner ruling required**; per-cut permission does not discharge the cumulative bar. By the current wording, this includes the faster side. |
| Runtime acceptable, but no demonstrated build benefit or an unacceptable build/RSS regression | None/negative | **ABANDON recommendation**; REDRAW only with a concrete build hypothesis. An owner choice to retain it for maintainability must be explicit. |

Every final outcome still goes to the owner under §11.5. The table makes the evidence
CLASSIFICATION and the DEFAULT RECOMMENDATION deterministic; it cannot make an expressly
discretionary owner ruling automatic, and it does not try to.

**WHAT A REDRAW MEANS**, so the word is not a placeholder: a DIFFERENT boundary, with a new claim and
fresh comparisons. Three concrete ones are named — retain the tiny mapper as a private inline
definition (§1.2's recorded alternative), keep an exposed arithmetic body with its caller, or move a
smaller genuinely cold subset. **`[[gnu::always_inline]]` and LTO are OUTSIDE this experiment**:
an attribute on a declaration cannot supply a missing body across a non-LTO boundary, and forcing
large kernels inline is not a credible unmeasured remedy.

**AND A COUNT IS NOT A PREDICTION.** The TU is 5,522 lines and the proposed region about 392. A
41 → 42 source count says nothing about compile benefit: template emission may make the region
disproportionately expensive, and duplicated header parsing may erase the gain entirely. §11.4.1
measures it rather than assuming it.

### §11.2 The comparisons

Each cut against its **immediate predecessor** AND **cumulatively** against the post-T3 base
(`50f616a`). Cut (d) needs a **c → d** comparison to isolate the boundary's own cost; BASE-vs-HEAD
alone can hide (d)'s regression behind an earlier cut's improvement.

**AND (d)'s PREPARATION COMMIT ADDS A THIRD (astra item 5).** Because §1.4's helper-internalisation
is a code-changing commit of its own, cut (d) retains THREE comparisons, not two:

| comparison | what it isolates |
|---|---|
| **preparation → TU** | the BOUNDARY's own cost, with the linkage change already paid |
| **`39d7a8e` → final** | the COMPLETE cut, preparation included |
| **post-T3 `50f616a` → final** | T6's CUMULATIVE bar (§11.1) |

Note which parent the revert arm restores: reverting the TU commit restores its IMMEDIATE PARENT —
the preparation commit — not an earlier head that lacked it.

### §11.3 The two legs

**Leg 1 — the U0 27-cell corpus, serialized.** `bench/bench_corpus.cpp` with `--engine walk|ssn|ipm`
and `--cells`; the T2 recipe (solo, `taskset -c 2`, `MKL_NUM_THREADS=1`, `OMP_NUM_THREADS=1`,
3× alternating A/B/A/B/A/B). Reference: T2 gave whole-corpus 23.41–23.47 s on both arms (0.26 %
spread) with per-cell 0.994–1.005; T3 gave per-cell 0.986–1.009, corpus 0.993.

**Leg 2 — an HS-scale leg, which (d) cannot do without.** Every corpus cell is F7 at n ≥ 800, where
the QP dominates and a de-inlined per-major helper (constraint reduction, predicted decrease, KKT
assembly) is invisible. The plan's own sentence — "loop-used arithmetic is not cold" — is a
statement about MAJORS, not about n. So:

* **Cells**: the 27 Hock-Schittkowski problems of `tests/sqp/support/hs_problems.h`
  (`hs_numbers()` at `:2277-2282`: 1, 3, 5, 6, 7, 10, 11, 12, 14, 15, 22, 24, 25, 26, 27, 28, 30,
  33, 35, 38, 39, 40, 43, 45, 76, 77, 79), driven through `SqpDriver` in all three modes.
* **Repeat count**: calibrated at T6.d time — the HS cells run in milliseconds, so N is raised until
  the per-cell run-to-run dispersion of the A arm alone is inside ±0.5 %, and the calibration
  transcript is part of the evidence. **Recorded here as a gap: no `--repeat` exists on
  `bench_corpus` today** (`bench/bench_corpus.cpp:378-458` has no such flag). *(The gap was closed at
  T6.d commit 1, `e696871`.)*

  **AMENDED AT T6.d FROM THE CALIBRATION ITSELF — the statistic named above cannot converge, and the
  wall clock on this leg cannot resolve the bar.** Settler ruling 2026-09-07 under the delegated
  numerics judgement, with the owner to see it; the numbers are the calibration transcript's.

  **(i) THE CALIBRATION COLUMN IS `median_se_pct`, NOT `spread_pct`.** "Raise N until the per-cell
  SPREAD is inside ±0.5 %" names max−min, which is monotonically NON-DECREASING in N by construction
  — more samples are more chances to catch a straggler — so the instruction cannot be followed.
  Measured on the bench box, Release, solo, `taskset -c 2`, MKL/OMP threads 1, warmup 1, worst
  per-cell reading:

  | N | 1 | 3 | 5 | 10 | 20 | 40 |
  |---|---|---|---|---|---|---|
  | ipm | 0.000 % | 9.306 % | 19.205 % | 23.904 % | 15.287 % | 24.117 % |
  | walk | 0.000 % | 7.865 % | 9.892 % | 11.203 % | 26.166 % | 38.225 % |

  The 0.000 % at N=1 is a tautology (max == min == the one sample) and every larger N is worse.
  `median_se_pct` — the standard error of the REPORTED median as a percentage of it, 1.2533·σ/√N over
  the median — asks the question the calibration is actually asking and falls as 1/√N: ipm's median
  across cells goes 0.694 (N=3) → 0.599 → 0.577 → 0.443 → 0.322 (N=40). **N is raised against that
  column.** `spread_pct` stays as the raw sample cloud and is explicitly not a calibration statistic.

  **(ii) LEG 2's VERDICT RESTS ON INSTRUCTIONS AND BRANCHES; ITS WALL CLOCK IS READ ONLY THROUGH THE
  PAIRED A/B RATIO.** Three runs of the SAME binary, solo and pinned, disagree by up to **1.4 %** at
  the walk corpus level (N=80: ipm corpus 0.149 %, walk corpus 0.998 %, worst cell 1.8–2.8 %). The HS
  cells are 0.3–3 ms, and at that scale the between-run variation is a PER-PROCESS CONSTANT — address
  layout, allocator state, page placement — not sampling noise, so `--repeat` cannot average it away
  at any N. **Leg 2's absolute wall figure therefore cannot resolve the 0.5 % effect the veto turns
  on.** On the same three runs `perf stat instructions:u` reproduces to **1.7e-6** between runs 2 and
  3 (run 1 is a first-run allocator/page-cache outlier that a median of three discards) — well inside
  §11.1's 1e-4 identity band, where the wall figure is four orders of magnitude too coarse.

  So: **pass A instructions and branches carry leg 2's verdict; pass B explains placement; the wall
  clock is informational and read only as the paired A/B ratio of the 3× alternating runs this
  section already prescribes, never as either arm's absolute corpus figure. Leg 1 — the U0 corpus,
  seconds-scale cells — remains the wall-clock leg.** This is not a weakening of §11.1: its veto
  trigger IS an instruction question ("WORK-MOVED — instructions UP"), and leg 2 exists to expose the
  two trace-enabled mapper call sites to a major-dense workload, which is exactly a question about
  executed work.

  **The lane's calibrated counts, declared per mode and IDENTICAL across arms**: N=1000 for ipm and
  walk, N=2000 for ssn.
* **The harness route is BENCH-SIDE, and it is T6.d's FIRST SUB-STEP.** Not a test binary: a test
  target is a different link and a different flag surface, which is a poor instrument for a
  veto-grade neutrality claim. It is also unnecessary — `bench/CMakeLists.txt:72` already puts
  `tests/sqp` on `hven_sqp_corpus`'s include path and `bench/ipqp_e1_arm.cpp:24` already includes
  `support/hs_problems.h` from a bench target, so an HS mode inside `bench_corpus` (plus `--repeat N`
  or an external loop) keeps the bench flag regime and ONE binary. **The terms**: the harness change
  lands ONCE, BEFORE any T6.d number is taken. A `--repeat` added between arms VOIDS the leg. No code
  for it lands with T6.0.

  **THE "IDENTICAL BINARY" TERM IS AMENDED (astra item 3), because as written it was unmeetable.**
  The two arms link two different static implementations of `libhven.a` — that is the whole
  experiment — so ONE executable file cannot run both. What must be identical is the **harness SOURCE
  and its CONFIGURATION**; each arm is then SEPARATELY LINKED, and **each arm's executable hash is
  RETAINED** so a reader can prove the two differ only in the library under them. That is the
  strongest form of the guarantee the original sentence was reaching for, and it is the form the
  evidence can actually carry.

  **AND LEG 2 MUST EXERCISE BOTH MAPPER CALL SITES.** `trace_outcome_of` runs at `:3447` and `:4792`
  ONLY WITH A TRACE SINK ATTACHED (§1.7.1), so a null-sink leg measures the split's clearest new call
  exposure exactly zero times. Leg 2 therefore carries **trace-enabled dispatch and SOC variants with
  a real sink**, and keeps the **null-sink timings separately identifiable** rather than averaged in
  — they are different populations, and reporting them as one would hide the site the veto is most
  likely to turn on.
* **Aggregation**: per-cell MEDIAN of the N repeats, then the median of the three alternating runs;
  the corpus figure is the sum of per-cell medians, not a mean of ratios.
* **The fallback-heavy cell**: at least one cell that drives the elastic ladder / certified fallback
  (`:4187`, `:4294`), with the **counters proving the path fired** in the same run
  (`elastic_activations`, `elastic_escalations`, `elastic_from_ipqp_escape`, `ipqp_fallback_rung_b`
  non-zero). A cell that does not fire the path is not evidence about it.

  **MEASURED AT T6.d, AND ONE ARM OF IT IS UNEXERCISED ON BOTH LEGS — recorded as a GAP, not
  reported as a pass.** The coverage statement, from the lane's leg-1 counters and the settler's
  leg-2 run:

  * **On leg 1 (U0)** the corpus CSV carries no `elastic_*` or rung-B columns at all, and
    `ipqp_escapes` is **0 on all 27 cells in all three modes** — so `certified_feasibility_fallback`
    is **never ENTERED** on leg 1.
  * **On leg 2 (HS), ipm**: the fallback's evidence-NOT-fired arm fires (`:1169`, 3 rung-B emits) and
    **rung A** fires (`elastic_from_ipqp_escape` 6, `elastic_activations` 13, `elastic_escalations`
    65). The ladder is exercised heavily and the fallback body is entered.
  * **The post-rung-A rung-B TAIL (`++ipqp_fallback_rung_b`, `:1229` at commit 2) is UNEXERCISED ON BOTH
    TIMING LEGS.** It is the path where the engine DECLINES a feasible rung A, and neither corpus makes
    it do so.

    **ITS BEHAVIOURAL HALF IS DISCHARGED BY THE UNIT SUITE, and the two halves are separated here so
    neither is overclaimed** (settler ruling at the T6.d fix round, on Codex's I3).
    `tests/sqp/test_trace_writer.cpp:1630`,
    `JsonLinesTraceSink.TheVerdictStreamReproducesTheWHOLEPartitionOnAFiveClassPopulation`, calls
    `certified_feasibility_fallback(...)` DIRECTLY (`:1675`) with a live `QpEngine` over six entries
    chosen to produce all five verdict spellings, and asserts `EXPECT_EQ(rung_b_n, 2)` —
    "the declined entry and the declined-above-the-floor retry" — together with
    `EXPECT_EQ(rung_b_n, out.ipqp_fallback_rung_b)`, which is what ties the emitted stream to the
    counter rather than to the JSON. So `++out.ipqp_fallback_rung_b` executes twice, from a REAL judge,
    at every landing (PASSED at `f47da07` and at the landing sha, both configs).

    **The RUNTIME half is not discharged and is not claimed to be.** No cell of either timing leg
    reaches this tail, so nothing measures its cost. What the record can say is static: the body is
    statement-identical across the move and the moved code is instruction-identical across its object
    move. That half goes to the OWNER as an amendment beside the disposition.

  So three of the four named counters fire, on leg 2, in ipm; the fourth fires nowhere. **No harness
  change is made to chase it** — the harness is frozen before the boundary arms, and changing it now
  would VOID the leg. The gap is carried into T6.d's report and its disposition, and any claim about
  the moved fallback body's rung-B tail is a claim about UNMEASURED code.
* **Conditions**: solo, alternating, all three modes, one process, `MKL_NUM_THREADS=1`,
  `OMP_NUM_THREADS=1`, pinned; `pgrep` pasted; wall-clock quoted only under these terms (CLAUDE.md
  §7).

**THE COUNTER SET GAINS A FRONT-END MEASUREMENT — REQUIRED BEFORE CUT (c) IS BENCHED** (the SQP
lane's T6.b review §10.4, its second required-before-(c) item, beside commit 0 fix3).

**The requirement is stated by MECHANISM, because event NAMES are per-vendor and the wrong ones are
unmeetable on the machine that runs the leg.** Every leg from (c) on must measure, beside
instructions and cycles: **(i) a front-end-bound fraction** — how much of the cycle budget the front
end failed to deliver uops for — **and (ii) a decoded-uop-cache hit/miss pair**. Those two are the
mechanism cut (b)'s walk figure points at; the events that carry them differ by vendor.

| | events |
|---|---|
| **pass A** — unchanged, on every leg | `instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u` |
| **pass B — Zen 3 (this box, AMD Ryzen 7 5800X3D)** | `instructions:u,cycles:u,de_dis_uop_queue_empty_di0:u,op_cache_hit_miss.op_cache_miss:u,op_cache_hit_miss.op_cache_hit:u,ic_fetch_stall.ic_stall_any:u` |
| **pass B — Intel** | `topdown-fe-bound`, or `idq.dsb_uops`/`idq.mite_uops`. **UNOBSERVED** — no leg has run on such a machine, and no value is recorded for them until one does |

**TWO PASSES, NOT ONE, and the reason is measured rather than assumed.** Zen 3 has six programmable
counters; a single combined seven-event probe MULTIPLEXES and the lane's trial of one returned
`<not counted>`. Pass B is exactly six events. Both passes run per binary per mode on the same
discipline as the wall clock (3× alternating, solo, pinned, `MKL_NUM_THREADS=1`,
`OMP_NUM_THREADS=1`), and `instructions` + `cycles` appear in BOTH so the two passes are tied to each
other. Reported per arm beside the existing rows: **`de_dis_uop_queue_empty_di0/cycles`** (the
front-end-bound fraction) and the **op-cache miss ratio**.

Pass A is unchanged so that cut (a)'s and cut (b)'s recorded rows stay comparable with everything
that follows.

**Why.** At cut (b) the walk mode arrived with an accounting gap the existing counters could not
close: instructions and branches identical to 1e-5, cycles +0.59 % (≈ 24 M), L1-icache-load-misses
DOWN (×0.76), and the ~45 k extra branch misses worth only ~1 M cycles. The remaining cycles are
front-end PLACEMENT the pass-A set does not name — decoded-uop-cache residency and fetch alignment as
loop heads move by +16 bytes — and inferring that from an icache counter that moved the OTHER way is
inference, not measurement.

**WHAT AN UNCLOSED ACCOUNTING MEANS FOR THE VERDICT — §11.1 IS CANONICAL AND IS NOT REINTERPRETED
HERE.** §11.1 requires, for LAYOUT-MOVED, instruction and branch counts identical within 1e-4 **AND**
the cycle delta accounted for by the icache/branch-miss deltas; a result that is neither that nor
WORK-MOVED is **UNRESOLVED pending re-measurement**. That rule stands as the owner gave it. Two
things follow, and they are not in tension:

* **An unexplained cycle gap is NOT a veto signal and NOT a redraw trigger.** Instruction identity is
  the half that distinguishes work from placement, and with instructions identical there is no work
  to veto. Cut (c) is not gated on such a cell.
* **But the verdict is UNRESOLVED, not LAYOUT-MOVED, until the accounting closes.** The pass-B
  measurement is the DISCHARGE: it either names the mechanism, at which point the cell is
  LAYOUT-MOVED, or it does not, at which point the cell is a finding to take to the owner. Recording
  it as LAYOUT-MOVED before that is the thing §11.1 forbids.

Cut (b)'s walk cell is the first result recorded on those terms (§11.5).

### §11.4 The rest of the CLAUDE.md §5 proof

* **Effective compile-flags diff** — the actual flags the two TUs receive, with the PCH disposition
  (§1.6) and any LTO setting **recorded as observed**, not as configured.
* **Relocation-aware caller disassembly** — the **actual caller artefact** disassembled (not a
  proxy object); callers listed as expected-to-differ **with the de-inlined callee named**.
* **Counter/trajectory checks** — replay 0/75 all three arms; the W4 trace goldens; `history`
  byte-identical on the HS cells.
* **`perf stat` beside every timing leg** — `instructions:u,branches:u,cycles:u,branch-misses:u,
  L1-icache-load-misses:u`, same discipline as the wall clock, so a non-FLAT result can be
  classified LAYOUT-MOVED or WORK-MOVED rather than only reported (§11.1).
* **Build side** — parallel build **wall-clock** and **peak RSS**, with build parallelism and cache
  conditions FIXED (`CCACHE_DISABLE=1`, a stated `-j`), and the RSS measurement defined (the metric
  and the tool, e.g. peak RSS of the compile step from `/usr/bin/time -v`, stated before the run).
  **This bullet is NECESSARY BUT NOT SUFFICIENT** — it does not specify the isolated per-TU
  compilations, which are the only measurement that can say whether the SPLIT itself bought anything.
  §11.4.1 completes it.
* **Captures after a TU move start from an EMPTY build directory** — `rm -rf` the build directory
  and CONFIGURE AGAIN for BOTH arms, at the same absolute path. This bullet as first written said
  `ninja -t cleandead && ninja -t clean` is not enough, which is wrong and contradicted the
  instrument it cites: the P-SYM tool header (`scripts/psym_compare.sh`, the M6 W5 T6 commit 0
  block) carries a five-row MEASUREMENT on ninja 1.13.2 — after a TU rename, `ninja -t clean` leaves
  the stale object (T4 D2 exactly) and `ninja -t cleandead` removes it. The empty-directory rule is
  the version-free form, and it is the one that binds, because `cleandead` reconstructs its
  dead-output list from a `.ninja_log` that may be truncated, deleted or written by a ninja whose
  behaviour differs. PROVENANCE, so the error is not re-made: T4's D2 says only that
  `ninja -t clean` is insufficient; `cleandead` entered the protocol through the SQP lane's T4
  review M3, which proposed "`cleandead && clean` — OR rebuild from an empty directory"; the T6
  brief's §1 parenthetical inverted that, and the inversion is the settler's, recorded in the
  ledger.

#### §11.4.1 THE REQUIRED BUILD NUMBERS, and the REVERT ARM (astra items 4 and 5.9, BINDING)

**These numbers accompany REDRAW and ABANDON too**, not only a KEEP: a boundary abandoned for
runtime reasons still has to say what its build side was worth, or the next window re-argues it from
nothing.

1. **Three isolated compilations of the ORIGINAL driver TU** (pre-split), individual times retained.
2. **Three isolated compilations of EACH resulting TU** — the post-split driver and
   `sqp_kernels.cpp` — reporting individual times, their SERIAL SUM, and the **two-TU PARALLEL SPAN**
   (the wall of compiling both at once, which is the figure a parallel build actually experiences).
3. **Three comparable PARALLEL builds before and after**: alone on the machine, a FIXED target set, a
   STATED `-j`, `CCACHE_DISABLE=1`, fixed cache conditions, with every sample and the spread retained
   — not a single number.
4. **PEAK RSS DEFINED TWICE, because the two answer different questions**: (i) the LARGEST INDIVIDUAL
   COMPILER RSS, from `/usr/bin/time -v` on the compile step, which is what decides whether a TU
   fits; and (ii) the PEAK CONCURRENT BUILD-PROCESS FOOTPRINT, which is what decides whether a `-j`
   level is survivable. A single "peak RSS" figure conflates them.

These are TIMING legs under CLAUDE.md §7: solo, serialised under the box lock against the lane's
bench, nothing else running, the `pgrep` pasted.

**AND THE NEW TU NEEDS ITS OWN COMPILE MEASUREMENT EVEN THOUGH IT IS PCH-OPTED-OUT.** Neither
updating the source-count guard nor passing `scripts/check_pch_neutrality.sh` proves the split has a
build benefit: the guard proves membership ACCOUNTING, and the PCH script proves ENGAGEMENT and BYTE
NEUTRALITY (`scripts/check_pch_neutrality.sh:18`), never compile speed. PCH admission requires BOTH
faster compilation and byte-identical output (`src/CMakeLists.txt:253`), and PCH experimentation
stays OUT of the initial boundary experiment.

**THE EFFECTIVE-FLAGS PROOF COMPARES ACTUAL COMMANDS**, three of them plus the relevant links:
pre-driver, post-driver, post-kernels. Record compiler, optimisation/FP/ISA flags, definitions,
include environment, PCH consumption and LTO — and SEPARATE source/output paths and declared
provenance macros from effective CODE-GENERATION differences, so a path string is never read as a
flag change. **Do not infer PCH consumption from the globally present `-fno-pch-timestamp`**: that
option is on every command in this tree and says nothing about whether a PCH was consumed.

**THE REVERT ARM IS A REQUIRED LEG**, and it is what turns "independently revertible" from an
assertion into a measurement. After the TU commit: `git revert` it in the measurement checkout,
rebuild FROM AN EMPTY DIRECTORY at the same absolute path, and compare against the TU commit's
IMMEDIATE PARENT. Required: source membership and the source COUNT restored, NO kernels object
present, library and test objects BYTE-IDENTICAL, and P-SYM passing **without the split's own
instruction exceptions** — an exception still needed there would mean the revert did not restore what
it claimed to. The three bench objects retain only their declared provenance-stamp allowance;
demanding literal whole-object identity of a newly stamped bench object would be a contradictory
requirement, so it is not demanded.

#### §11.4.2 THE PRE-REGISTERED BUILD-BENEFIT THRESHOLD

**Fixed by the settler BEFORE any (d) number was read**, which is the only condition under which a
threshold means anything. The owner may amend it; if they do, the amendment is recorded HERE with its
date, so a moved goalpost is visible as a moved goalpost.

A **KEEP-eligible build benefit** means ALL THREE of:

1. the **two-TU parallel span ≤ 0.85 ×** the original driver TU's isolated compile wall (medians of
   three, and the gap beyond the stated spread);
2. the **full parallel build wall not worse** than the pre-(d) median plus its spread;
3. **peak concurrent RSS ≤ 1.05 ×** pre-(d).

Below that bar, the runtime-acceptable row of §11.1.1's table reads **ABANDON-recommended**.

### §11.5 The decision, and the record of each cut's

Redraw or ABANDON if runtime moves. Grant rules on the outcome (keep / redraw / abandon), presented
with the numbers.

**CUT (a) — RULED KEEP by the owner, 2026-09-06** (relayed by the settler; the numbers are the SQP
lane's T6.a review §9). It was **non-FLAT by the letter in all three modes**:

| mode | corpus | per-cell envelope |
|---|---|---|
| ipm | **+0.72 %** | 1.001–1.009 |
| ssn | **+0.39 %** | 0.998–1.013 |
| walk | **+0.53 %** | 0.980–1.010 |

and `perf stat` says why: **instructions 1.00001, branches 1.00001** — identical to 1e-5, five
times inside the 1e-4 band — **cycles 1.0059**, and **L1-icache load misses ×1.92**. The mechanism
is PLACEMENT, not work: the same instructions and the same branches, fetched from a different place.
That is **LAYOUT-MOVED** under §11.1, and the owner ruled **KEEP**. The cumulative bar still applies
at T6's close and this result is one of the contributions it will be measured against.

**CUT (b) — ipm/ssn LAYOUT-MOVED; walk LAYOUT-MOVED (closed at cut (c)'s ledger line from the registered re-measurement, below); NO REDRAW** (the SQP lane's T6.b review §10, run on
their own binaries from both commits; the immediate comparison is BASE `03e1b34` vs code head
`25f586e`):

| mode | corpus | per-cell envelope | cells outside 0.99–1.01 | verdict |
|---|---|---|---|---|
| ipm | **0.9995** (−0.05 %) | 0.996–1.006 | none | FLAT, **LAYOUT-MOVED** — accounting closed |
| ssn | **1.0013** (+0.13 %) | 0.997–1.008 | none | FLAT, **LAYOUT-MOVED** — accounting closed |
| walk | **1.0029** (+0.29 %) | 0.991–1.011 | one — `f7_n800_path_warm` at 1.011 | inside the corpus bar; **LAYOUT-MOVED — closed 2026-09-07 from the pass-B re-measurement (see "THE DISCHARGE", below)** |

| counter (user), ratio HEAD/BASE | ipm | ssn | walk |
|---|---|---|---|
| instructions | 1.00000 | 1.00000 | 1.00000 |
| branches | 1.00000 | 1.00000 | 1.00000 |
| cycles | 0.99786 | 1.00173 | 1.00589 |
| branch-misses | 0.99616 | 1.00454 | 1.00402 |
| L1-icache-load-misses | 0.47328 | 0.62228 | 0.76125 |

**Instructions and branches are identical in every mode** — the extraction of the three arms into
calls taking `st` and `mj` by reference added no executed work — so **nothing here is WORK-MOVED and
there is no redraw trigger anywhere in cut (b)**. ipm's icache count swung back from cut (a)'s ×1.92
to ×0.47, and in ipm and ssn the cycle delta IS accounted for by the icache/branch-miss deltas, which
is the whole of §11.1's LAYOUT-MOVED test. Those two cells are closed.

**THE WALK CELL IS UNRESOLVED PENDING RE-MEASUREMENT, and is recorded as such rather than as
LAYOUT-MOVED.** Its +0.59 % of cycles (≈ 24 M) is NOT closed by the two named counters: icache misses
went DOWN (×0.76), and the extra ~45 k branch misses are worth ~1 M cycles. The residue is front-end
placement the pass-A counter set does not name. §11.1 is explicit that a result which is neither
accounted-for LAYOUT-MOVED nor WORK-MOVED is UNRESOLVED pending re-measurement, and the fix1 text
that classified this cell LAYOUT-MOVED on instruction identity alone was reading the canonical rule
against itself (Codex Important 2 at the fix1 review). Instruction identity is why it is **not a
veto and not a redraw trigger**; it is not why it would be resolved.

The one cell at 1.011 (`f7_n800_path_warm`) is a §11.1 veto-trigger cell BY THE LETTER of the
wall-clock band, with the same sign in all three runs; it is inside the same unresolved figure and
carried with it.

**THE DISCHARGE IS REGISTERED, and it is cheap because the binaries are retained.** The SQP lane
re-measures cut (a) vs cut (b) in WALK mode with **pass B** (§11.3) on its retained post-(a) and
post-(b) binaries, as part of the cut (c) bench leg, and **this row is closed from that measurement
at cut (c)'s ledger line** — LAYOUT-MOVED if the front-end-bound fraction and the op-cache ratio
name the mechanism, a finding for the owner if they do not. **Cut (c) is NOT gated on it**: no
instruction moved, so nothing about (c)'s dispatch waits on this cell.

**THE DISCHARGE RAN, AND THE CELL IS CLOSED — LAYOUT-MOVED (settler ruling 2026-09-07, under the
numerics judgement the owner delegated; recorded at cut (c)'s ledger line).** The SQP lane re-measured
cut (a) vs cut (b) in WALK mode with pass B on its retained T6.b binaries (`w5t6b/corpus-base` =
`03e1b34` = post-(a) code vs `w5t6b/corpus-head` = post-(b) code; the three perf cells, 3× alternating,
solo, pinned, `MKL_NUM_THREADS=OMP_NUM_THREADS=1`; `.superpowers/w5-t6-c-review-tycho.md` §6):

| counter (user) | ratio (b)/(a) | 3-pair range |
|---|---|---|
| instructions | 1.00000 | 1.00000..1.00000 |
| cycles | 0.99928 | 0.979..1.017 |
| `de_dis_uop_queue_empty_di0` | 0.99698 | 0.852..1.215 |
| `op_cache_hit_miss.op_cache_miss` | **1.06824** | 1.048..1.096 |
| `op_cache_hit_miss.op_cache_hit` | 0.99850 | 0.9985..1.0009 |
| `ic_fetch_stall.ic_stall_any` | 0.99432 | 0.959..1.023 |

FE-bound fraction 0.0263 → 0.0263; op-cache miss share 0.0274 → 0.0293. **The +0.59 % cycle delta T6.b
measured on these cells did not reproduce** (0.999, ±2 % over three pairs); the one counter that moves
consistently in all three pairs is the decoded-uop-cache miss count, +6.8 %, with instructions identical
to 1e-5. That is the mechanism §11.3 was amended to look for — uop-cache residency as loop heads moved —
and its cycle cost on these cells is inside run-to-run noise. Under §11.1 the classification rests on
instruction identity AND an accounting that closes: with no reproducible cycle delta there is nothing
left to account for, and the op-cache counter names what moved. The dispatch-queue-empty counter is
too noisy on three short cells to carry weight (range 0.85–1.22) and is recorded, not relied on. The
corpus-level walk figure (+0.29 % at T6.b, instructions identical) stands as measured; the
`f7_n800_path_warm` 1.011 cell is carried inside it.

**CUT (c) — LAYOUT-MOVED in the FASTER direction in all three modes, NO REDRAW** (the SQP lane's T6.c
review §4, its own binaries, BASE `a10adc3` vs commit 2 `01fab70`, 27 U0 cells, 3× alternating, pass A
and pass B):

| mode | corpus | per-cell envelope | outside 0.99–1.01 | letter of §11.1 | instructions | icache |
|---|---|---|---|---|---|---|
| ipm | **0.9961** (−0.39 %) | 0.993–1.000 | none | FLAT | 0.99999 | ×0.46 |
| ssn | **0.9918** (−0.82 %) | 0.987–1.001 | three, all faster | outside the FLAT letter on the faster side | 1.00002 | ×0.57 |
| walk | **0.9925** (−0.75 %) | 0.986–1.000 | five, all faster | outside the FLAT letter on the faster side | 0.99992 | ×0.59 |

Branches identical; cycles down in every mode; branch misses −2 %. Pass B: dq-empty +6 % ipm / −2.5 %
ssn / flat walk, op-cache misses +2–4 %. Nothing WORK-MOVED; the three unanticipated Eigen bodies
(two de-inlined, one the `x_trial` `operator=` source change) cost no measurable instructions.

**THE CUMULATIVE READING AFTER (c) — post-T3 (`58989d7` = `50f616a` code) vs `01fab70`, ipm: +0.30 %,
INSIDE the ±0.5 % corpus bar** (was +0.70 % after (b)): corpus 1.0030, envelope 0.997–1.006, no cell
outside the band, instructions 1.00000, branches 1.00000, cycles 1.00271, branch-misses 0.99928, icache
×1.19, dq-empty ×1.086, op-cache misses ×1.047 — mechanism named, accounting closed. A reading for T6's
close against the post-(d) head, per §11.1; no owner ruling is needed at this reading.

**THE CUMULATIVE READING TODAY — SUPERSEDED by the post-(c) reading immediately above, and kept as
HISTORY rather than deleted.** It is the post-(b) reading; it was the current one when it was
written, and it is no longer. Nothing in cut (d) is to be argued from the +0.70 % figure below: the
reading that stands is **post-T3 → `01fab70`, ipm +0.30 %**, and the reading that will BIND is the
one taken afresh at the post-(d) head (§11.1). Recorded because the two arithmetics together show
what (c) actually moved.

**post-T3 `50f616a` vs post-(b), ipm: +0.70 %, ABOVE the ±0.5 %
corpus bar, and it is all cut (a)'s.** The arithmetic: (a) +0.72 %, (b) −0.05 %, cumulative +0.70 %.
Cumulatively instructions are 1.00001 and icache misses are FLAT (0.99) — so the cycles cut (a)
booked against icache ×1.92 are still there after (b) restored the icache count, which says the
mechanism is broader front-end placement than icache alone (branch-misses +1.5 % cumulatively is
~5 M of the ~68 M extra cycles). **This is a reading, not a ruling.** Per §11.1 the cumulative bar
is READ AT T6's CLOSE against the post-(d) head, and (c) — which restructures the loop top, the three
lambdas and the ten push sites — will re-place everything again. If the close reading is still above
+0.5 %, Grant rules with the numbers, as §11.1 says.

**CUT (d) — RULED ABANDON by the owner, 2026-09-07 (presented with the numbers; the settler's
recommendation was ABANDON).** The SQP lane's T6.d review §5, its own binaries, BASE = the prep commit
`7565936` (code) vs the TU `f47da07`, 27 U0 cells, 3× alternating, pass A and pass B:

| mode | corpus | per-cell envelope | outside 0.99–1.01 | letter of §11.1 | instructions |
|---|---|---|---|---|---|
| ipm | **1.0098** (+0.98 %) | 1.0030–1.0159 (`f7_n800_path_warm`) | **15/27** | NOT FLAT; LAYOUT-MOVED (cycles 1.0138 accounted by dq-empty ×1.283, branch-misses ×1.160, icache ×1.279, op-cache misses ×1.079) | 1.00000 (Δ ≤ 4e-7) |
| ssn | 1.0030 | 0.9962–1.0156 (`f7_n2000_bound_warm`) | 3/27 | corpus inside the FLAT letter, cells outside; cycles 1.0024 | 1.00000 |
| walk | 0.9978 | 0.9926–1.0093 | 0/27 | FLAT; cycles 1.0013 | 1.00000 (+1.45–1.74 M executed on the HS walk-sink arm only) |

**THE CUMULATIVE READING AT THE POST-(d) HEAD — post-T3 `50f616a` → `f47da07`, ipm: 1.0121, +1.21 %,
OUTSIDE the ±0.5 % corpus bar** (envelope 1.0061–1.0155, 20/27 cells outside; ssn 1.0048, 6/27, inside by 0.02 %; walk 0.9981, 1/27, inside). §11.1 said this reading binds, and it did: the
owner ruled with the numbers. The build half of the §5 proof failed its pre-registered threshold on the
same day (§11.4). The TU was reverted as a new commit, 1997159; the library sources at that commit are
tree-identical to the pre-TU docs commit `14cf243`.

**THE T6 CLOSE READING — post-T3 `50f616a` → the post-revert head `1997159` — is recorded below when the SQP lane's measured reading lands (`.superpowers/w5-t6-d-abandon-review-tycho.md`).**

---

## §12. Corrections and findings this document lands

1. **Three anonymous namespaces, not two** (`:37`, `:849`, `:1242`). The brief's §0 said two; astra
   G6 caught it; confirmed at BASE. §1.
2. **`trace_mode_of` is driver-side only**; `trace_outcome_of` is the one INTERNAL-LINKAGE symbol
   used by both sides (`:1050`, `:1169`, `:1212` kernels; `:4224`, `:4532` driver), and so the ONE
   that changes linkage under (d). It is NOT the only shared symbol: `predicted_decrease` (`:584`)
   is called from the kernels at `:1117` and from the driver at `:4481`, and costs no new surface
   only because it is already declared. §1.2, §1.3, §1.7.
3. **TWO external-linkage free functions have no declaration anywhere** —
   `assert_ssn_warm_grade_window` (`:779`, caller `:4830`; the header names it only in a comment at
   `include/hven/drivers/sqp_driver.h:3068`) and `assert_ipqp_hand_off_window` (`:805`, caller
   `:4124`). Found by sweeping all 26 against the header; the first was MISSED in this document as
   first written, and the SQP lane's review found it. §1.4.
4. **§5 F2's "all decision inputs are read before any mutation" is FALSE.** The corrected sequence
   is §5's table (astra G1). The consumption of the candidate is CONDITIONAL (`:3262-3263`), not
   unconditional on entry.
5. **Push #4 and #9 were misclassified in the lane skeleton.** #4 = retryable QP failure at floor
   (`:4392`, `:4401`); #9 = `kRestore` with no floor condition, passing the ORIGINAL measured trial.
   astra's §8.2 table is the T6.0 map.
6. **A trace emit the skeleton did not list**: `route_through_ssn_warm_grade` emits `qp.mode kSsn`
   at site `ssn_warm_grade` from `:4855` — INSIDE the callee, therefore BEFORE the kIpm arm's
   route/mode pair at `:4160`/`:4172`. §7 E4.
7. **The successor block has two conditionals** (`:4209` and `:4212`); "one conditional site after
   routing" is about the WALK INVOCATION. §6.1.
8. **`tr_shrink_retry` (`:3610`)** is crossing state the skeleton did not name: it suppresses both
   the IPQP cross-major re-centring (`:4030`) and the escape-ladder charge (`:4076`). §2.3, §6.
9. **PCH and source-count integration**: `drivers/sqp_driver.cpp` is PCH-OPT-OUT; the guard is
   `_hven_expected_source_count 41` at `src/CMakeLists.txt:446`. §1.6.
10. **Two citation offsets** in the reviews, recorded so no one re-derives them: astra's
    `sqp_driver.cpp:4810` points at the comment head; the statement
    `sopts.defer_certification = false;` is at `:4813`. The second half of this item was WRONG in
    the other direction and is corrected in item 16: astra's `test_sqp_driver.cpp:8179` is the
    `TEST(...)` line itself.
11. **Cut (d)'s MOVING SET is named** (§1.7): the two kernels plus anonymous namespace #2's three
    kernels-only helpers, with `trace_outcome_of` the one linkage change and `predicted_decrease`
    the one already-declared shared symbol; **THREE** test TUs call the kernels directly
    (`test_sqp_driver.cpp` 15, `test_ipqp_dispatch.cpp` 3, `test_trace_writer.cpp` 1;
    `test_qp_mode_sites.cpp` has ZERO — its three mentions are comments), so the moving set has a
    test-facing surface T6.d's P-SYM claim must cover. **This item as first written said FOUR**,
    contradicting §1.7's own corrected paragraph two pages above it; the lane corrected §1.7 at the
    T6.0 fix1 review (A3) and this sentence was left behind. Corrected at the (d) design review
    (astra Minor).
12. **The restoration payload OWNERSHIP is ruled** (§5 constraint 5): the state bundle owns it, the
    outcome type is a tag. astra §2 asked the doc to choose; §2.2 L already determined the answer.
13. **§11.4's empty-build-directory bullet, as first written, was WRONG** and contradicted the
    instrument it cites — it repeated the T6 brief's inversion of the SQP lane's own T4 review M3.
    The measurement (five rows, ninja 1.13.2) is in the P-SYM tool header; the bullet now carries
    the version-free rule and the provenance.
14. **§11.3's Leg 2 fallback would have timed a TEST binary.** Replaced by the bench-side route the
    Claude-substitute review found (`bench/CMakeLists.txt:72` already puts `tests/sqp` on
    `hven_sqp_corpus`'s include path; `bench/ipqp_e1_arm.cpp:24` already includes
    `support/hs_problems.h`), landing ONCE as T6.d's first sub-step.
15. **No line number moved between `154d20a` and BASE `50f616a`** inside `solve_impl_body`: T3's nine
    migrated calls in this TU are at `:345-562`, outside the body, and its edits were line-neutral.
    Every number in the addendum and in the astra review that falls inside `:2025-4732` was
    re-checked at BASE and holds. The one number that differs is the W5 inventory's, taken at
    `d0c7aa0`: `solve_impl_body` `2026-4733` there, `2025-4732` here — a uniform one-line shift.
16. **§6's crossing list was wrong in BOTH directions, and §9's pin line was off by one** — found
    at cut (b), which typed its parameters from them. `seam` and `iter` were MISSING (the kIpm arm
    reads `seam.epoch()` twice and the major index three times); `x` and `last_dual_mu` were
    OVER-LISTED (no arm reads either inside the switch); and `warm`, which crosses explicitly into
    the shared successor, was missing too. **The first correction (`f9ca2bd`) struck `last_dual_mu`
    in its bullet but left it standing in the set itself, and did not add `warm`** — both closed in
    the T6.b fix round, together with the heading, which said "read inside or after it" when the set
    it heads is "read by a routing arm or the shared routing successor". §6 now carries all five with
    the re-derivation. And the withdrawal pin's `TEST(...)` is at `tests/sqp/test_sqp_driver.cpp:8179`,
    not `:8178` — `:8178` is a blank line; §12 item 10's gloss had astra's citation inverted, and
    astra's `:8179` was right. §6, §9, §12 item 10.
17. **Cut (b)'s `MajorState` is the ROUTING OUTPUTS ONLY and is built at the dispatch**, not at the
    loop top; `kkt`, `row`, `row_qp_mode`, `caller_row` and the trial/SOC objects join it at cut (c).
    Recorded in §10.2 so (c) inherits the decision and its reason rather than re-deriving them.

18. **astra's cut-(c) design review (VERDICT AMEND, no Criticals) lands four corrections and one
    contract**, `.superpowers/w5-t6-c-design-review-astra.md`. **F1**: §8.2's quoted accepted-commit
    row overstates the derivative ordering — direct-accept refresh FOLLOWS the push (`:5048`),
    promoted-SOC refresh remains BEFORE it (`:4907`); both iterate commits and radius growth follow
    the push. The quote stays verbatim and the qualification beside it is what binds. **F2**: the
    outcome names had no caller contract; §10.3.1 now carries astra's item 3 verbatim — the two
    payload-free enums, the finish-tag → caller-selects table, the per-site outcome column, and the
    complete `enter_restoration` signature, with the decide/run split staying REGISTERED and outside
    (c). **F3**: §7 claimed the trace goldens and the `history.size()` pin establish accepted
    emission before mutation; they do not — they pin CONTENT, and moving the push below the updates
    preserves all of it. §7 now names a registered ordering assertion
    (`ThrowsOnSecondGradientModel` must require the accepted row already present when the second
    gradient throws) and a source-order audit as (c)'s own gate. **F4**: §8.1 site 9's "even when
    SOC ran" is removed — SOC runs only for the original `kReject` and the `kRestore` route does not
    follow it; site 7's original-trial qualification stays. **And item 6**: cut (c) is TWO commits,
    and "no alias survives (c)" means the transitional aliases in `solve_impl_body` only — the
    routing functions' own local aliases stay (§10.3.2).
19. **The cut (b) WALK cell is UNRESOLVED, not LAYOUT-MOVED, and §11.3's front-end events were
    Intel names on an AMD box** — both found at the T6.b fix1 review. Codex Important 2: §11.1 makes
    an unexplained cycle delta UNRESOLVED pending re-measurement, and the fix1 text classified the
    walk cell LAYOUT-MOVED on instruction identity alone, which reads the canonical rule against
    itself. §11.1 is the owner's ruling and is NOT reinterpreted; §11.3 and §11.5 are corrected to
    it, with the discharge registered against cut (c)'s bench leg on the lane's retained binaries.
    Settler addendum: `perf list` on the bench box (AMD Ryzen 7 5800X3D) has none of
    `topdown-fe-bound` / `idq.dsb_uops` / `idq.mite_uops`, so §11.3 as first written was unmeetable
    on the machine that runs the leg; the requirement is now stated by MECHANISM with per-vendor
    event names, Zen 3 pass B named explicitly, and the Intel names kept as UNOBSERVED.
20. **Cut (c) lands one new hazard and three places where §2.3 overstates the target**, all in
    §10.3.3. The hazard: **`MajorState` member ORDER is load-bearing** — cut (b)'s four routing
    outputs must stay first, because `route_through_ssn_tier`, which (c) does not edit, reaches
    `mj.qs` and `mj.walk_owns_this_qp` by offset, and putting (c)'s members ahead of them cost that
    untouched function two instructions (measured, then fixed by ordering). The overstatements:
    twelve of §2.3's rows did NOT join the bundle because they do not cross the `run_major` → caller
    boundary; `converged` DID join and is not in §10.2's list; and the three lambdas of that table
    are locals or a member function, not members. Also recorded there: the two spellings that differ
    from §10.3.1's quoted code (the `k` prefix, CLAUDE.md §4; and the `{nullptr, nullptr}` default
    argument, which the language forces), and that §7's ordering assertion is built and falsified.

21. **astra's cut-(d) design review (VERDICT AMEND, no Criticals) lands five Important amendments and
    three Minors**, `.superpowers/w5-t6-d-design-review-astra.md`, SIGNOFF
    W5-T6-D-DESIGN-REVIEW-ASTRA, read at HEAD `39d7a8e`. The MOVING SET SURVIVED unchanged — the six
    rows of §1.7/§10.4 are correct at HEAD and no §1.7 function is mis-sided — but the document was
    **not sufficient to make (d)'s outcome unambiguous**, and every amendment below is binding on the
    cut. **A1**: the runtime categories did not determine a disposition; §11.1.1 now carries the
    precedence, the two resolved overlaps (demonstrated added work vetoes even at FLAT timing; and
    "instructions UP" is a reproducible increase, neither hidden inside nor conjured out of the 1e-4
    band) and astra's disposition table verbatim. **A2**: the linkage destination and the two
    undeclared helpers were undecided — §1.2 names the source-private header
    `src/drivers/sqp_kernels_internal.h` (NOT under `include/`, which is installed wholesale at
    `CMakeLists.txt:613`) and records the private-inline REDRAW alternative; §1.4 gives both helpers
    internal linkage in a SEPARATELY CLAIMED preparation commit, because internalising them inside
    the TU commit would contradict its own "ONE symbol changes linkage" claim. **A3**: leg 2 must
    exercise the two trace-enabled mapper sites, and "the IDENTICAL binary runs both arms" was
    unmeetable for two statically linked library implementations — §11.3 now requires identical
    harness SOURCE and CONFIGURATION, separately linked arms with RETAINED HASHES, and null-sink
    timings kept separately identifiable. **A4**: §10.4.1 makes the mapped-body/literal audit
    concrete, with the exclusive-vs-shared literal rule, no one-for-one `.LCPI` transfer, and the
    header-generated initialisers (`kSsnComplementarityFactor`, `kSsnTrViolationFactor` — two
    `__cxx_global_var_init` in the driver object today) and the fmt constant tables named. **A5**:
    "independently revertible" was asserted and never tested, and the compile-cost experiment was
    unspecified — §11.4.1 requires the REVERT ARM and the four build measurements, and §11.4.2 fixes
    the KEEP-eligible threshold BEFORE any number was read. **The three Minors**: §1.2's
    `elastic_initial_rho :893` was a COMMENT counted as a call; item 11 above said four test TUs
    where §1.7 says three; §11.5's "CUMULATIVE READING TODAY" was the superseded post-(b) reading.
    **And one correction to the addendum's premise, made from the retained object rather than from
    argument** (§1.7.1): the two kernels are ALREADY externally linked and ALREADY out of line
    (14,865 and 1,384 bytes, with PLT32 calls at three of the five edges), so "calls replace inlined
    large-kernel bodies" does not describe this baseline; `trace_outcome_of`, which has no standalone
    symbol at all today, is the clearest NEW call exposure, and **WORK-MOVED is a registered expected
    RISK, not a predetermined result**. LTO is OFF as observed and (d) must not flip it.
---

## §13. What T6 must not change (restated so the cuts are checked against it)

Counters and trajectories (replay 0/75, all three arms, at every landing); the W4 trace goldens and
both per-major exactly-once checks; `history` byte-identical on the HS cells; the elastic /
certified-fallback contracts pinned in W2 (their tests unchanged); `include/hven/drivers/sqp_driver.h`'s
public surface (T6 is `.cpp`-internal except the declared kernels surface at T6.d);
`enter_restoration`'s decide/run split — REGISTERED, **not** T6 (§5 settles this: the ownership
table shows the closure CAN be typed without it).
