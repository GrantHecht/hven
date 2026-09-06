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
| `elastic_initial_rho` | `:854` | `:893`, `:1014` | kernels only |
| `elastic_evidence_seed` | `:880` | `:1021` | kernels only |
| `trace_outcome_of` | `:963` | `:1050`, `:1169`, `:1212`, **`:4224`**, **`:4532`** | **SHARED** |
| `emit_qp_mode_line` | `:972` | `:1050`, `:1169`, `:1212` | kernels only |

`trace_outcome_of` is the ONE symbol used by both sides of cut (d) — three call sites inside
`run_elastic_ladder`/`certified_feasibility_fallback` and two inside `solve_impl_body`. It is the
symbol the plan means when it says the block "would gain external linkage, a declared surface". The
other three are kernels-side only and stay internal to the new TU.

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
linkage, or it gains a header declaration. Both callers stay DRIVER-side under the §1.7 moving set,
so internal linkage in the driver TU is the natural disposition — **decided at T6.d, not here**;
recorded so it is decided rather than inherited.

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

---

## §2. The state universe

The two state types of §5 F1 are not the whole universe (astra §2). Four dispositions:

* **BORROWED** — lives outside the solve; the body reads or drives it and never owns it.
* **`SolveState`** — ONE authoritative mutable instance, initialised in place by `prepare_solve`,
  alive for the whole solve.
* **`MajorState`** — constructed at the loop top, dies with the major; nothing in `SolveState` may
  reference it and no reference to it may escape.
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

**Set before the switch and read inside or after it:** `tr_shrink_retry` (`:3610`, read `:4030`,
`:4076`) · `qp` rebuilt if stale (`:3611-3615`) · `overrides.tr_radius` (`:3618`) and
`overrides.dual_mu` (`:3654`, adaptive-mu only) · `row.mu` / `last_dual_mu` (`:3656-3657`) ·
`offer_hot` (`:3670`, read `:4214`, `:4230`) · `use_crash` / `crash_seed` (`:3678-3689`, read
`:4216`) · `qs` (`:3702`) · `fallback_report` (`:3705`) · `walk_owns_this_qp` (`:3706`, **initialised
true**) · `ipqp_chain_owns_the_step` (`:3710`) · `row_qp_mode` (`:2702`) · `ssn_prox_ingested`
(`:2407`) · `ipqp_ladder` (`:2046`) · `ipqp_analysis_epoch` (`:2056`) · both budget charges (`:2442`,
`:2449`) · `delta`, `seed`, `have_seed`, `ev`, `x`.

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
| 9 | `:4660` | **RECOVERY** | verdict `kRestore` (`:4655`), **NO floor condition** → `enter_restoration({&x_trial, &ev_trial})` `:4659` — the **ORIGINAL measured trial**, even when SOC ran | restoration → push → `if (restored) continue` `:4661-4663` → else check `:4666` → `return finish(…, restoration_exit_*, …)` `:4667-4672` |
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
`tests/sqp/test_sqp_driver.cpp:8178`,
`TEST(SqpDriverSsnMode, ARefusedFaceRefinementIsChargedEvenWhenTheCertificateIsWithdrawn)`.

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
the row is emitted; accepted rows emitted before radius/iterate updates; both exactly-once checks
stay; trace emit ORDER unchanged (§7). The restoration outcome obeys §5 in full.

### §10.4 Cut (d) — the kernels TU (EXPERIMENT with a VETO)

Extract WITHIN the TU first (a–c are that); then the separate TU as an **independently revertible**
commit with the CLAUDE.md §5 proof. **The moving set is exactly §1.7's "KERNELS" rows plus anonymous
namespace #2's three kernels-only helpers** — `run_elastic_ladder`, `certified_feasibility_fallback`,
`elastic_initial_rho`, `elastic_evidence_seed`, `emit_qp_mode_line` — with `trace_outcome_of` (§1.2)
the ONE symbol that changes linkage: it gains external linkage, a declared surface, and goes in the
migration guide. `predicted_decrease` is shared across the cut but already declared, so it costs no
new surface (§1.7). Build integration per §1.6. The two undeclared helpers (§1.4, `:779` and `:805`)
are decided here.

P-SYM alone cannot prove the semantic identity of moved bodies, literals or static initialisation
(astra F4/G4): cut (d) supplements it with a mapped source-body / literal audit and an audit of any
moved static initialisation or tables.

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

> "informational" does not waive neutrality

### §11.2 The comparisons

Each cut against its **immediate predecessor** AND **cumulatively** against the post-T3 base
(`50f616a`). Cut (d) needs a **c → d** comparison to isolate the boundary's own cost; BASE-vs-HEAD
alone can hide (d)'s regression behind an earlier cut's improvement.

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
  the per-cell run-to-run spread of the A arm alone is inside ±0.5 %, and the calibration transcript
  is part of the evidence. **Recorded here as a gap: no `--repeat` exists on `bench_corpus` today**
  (`bench/bench_corpus.cpp:378-458` has no such flag).
* **The harness route is BENCH-SIDE, and it is T6.d's FIRST SUB-STEP.** Not a test binary: a test
  target is a different link and a different flag surface, which is a poor instrument for a
  veto-grade neutrality claim. It is also unnecessary — `bench/CMakeLists.txt:72` already puts
  `tests/sqp` on `hven_sqp_corpus`'s include path and `bench/ipqp_e1_arm.cpp:24` already includes
  `support/hs_problems.h` from a bench target, so an HS mode inside `bench_corpus` (plus `--repeat N`
  or an external loop) keeps the bench flag regime and ONE binary. **The terms**: the harness change
  lands ONCE, BEFORE any T6.d number is taken, and the IDENTICAL binary runs both arms. A `--repeat`
  added between arms VOIDS the leg. No code for it lands with T6.0.
* **Aggregation**: per-cell MEDIAN of the N repeats, then the median of the three alternating runs;
  the corpus figure is the sum of per-cell medians, not a mean of ratios.
* **The fallback-heavy cell**: at least one cell that drives the elastic ladder / certified fallback
  (`:4187`, `:4294`), with the **counters proving the path fired** in the same run
  (`elastic_activations`, `elastic_escalations`, `elastic_from_ipqp_escape`, `ipqp_fallback_rung_b`
  non-zero). A cell that does not fire the path is not evidence about it.
* **Conditions**: solo, alternating, all three modes, one process, `MKL_NUM_THREADS=1`,
  `OMP_NUM_THREADS=1`, pinned; `pgrep` pasted; wall-clock quoted only under these terms (CLAUDE.md
  §7).

### §11.4 The rest of the CLAUDE.md §5 proof

* **Effective compile-flags diff** — the actual flags the two TUs receive, with the PCH disposition
  (§1.6) and any LTO setting **recorded as observed**, not as configured.
* **Relocation-aware caller disassembly** — the **actual caller artefact** disassembled (not a
  proxy object); callers listed as expected-to-differ **with the de-inlined callee named**.
* **Counter/trajectory checks** — replay 0/75 all three arms; the W4 trace goldens; `history`
  byte-identical on the HS cells.
* **Build side** — parallel build **wall-clock** and **peak RSS**, with build parallelism and cache
  conditions FIXED (`CCACHE_DISABLE=1`, a stated `-j`), and the RSS measurement defined (the metric
  and the tool, e.g. peak RSS of the compile step from `/usr/bin/time -v`, stated before the run).
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

### §11.5 The decision

Redraw or ABANDON if runtime moves. Grant rules on the outcome (keep / redraw / abandon), presented
with the numbers.

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
    `sopts.defer_certification = false;` is at `:4813`. Astra's `test_sqp_driver.cpp:8179` is the
    test's first body line; the `TEST(...)` is at `:8178`.
11. **Cut (d)'s MOVING SET is named** (§1.7): the two kernels plus anonymous namespace #2's three
    kernels-only helpers, with `trace_outcome_of` the one linkage change and `predicted_decrease`
    the one already-declared shared symbol; four test TUs call the kernels directly, so the moving
    set has a test-facing surface T6.d's P-SYM claim must cover.
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

---

## §13. What T6 must not change (restated so the cuts are checked against it)

Counters and trajectories (replay 0/75, all three arms, at every landing); the W4 trace goldens and
both per-major exactly-once checks; `history` byte-identical on the HS cells; the elastic /
certified-fallback contracts pinned in W2 (their tests unchanged); `include/hven/drivers/sqp_driver.h`'s
public surface (T6 is `.cpp`-internal except the declared kernels surface at T6.d);
`enter_restoration`'s decide/run split — REGISTERED, **not** T6 (§5 settles this: the ownership
table shows the closure CAN be typed without it).
