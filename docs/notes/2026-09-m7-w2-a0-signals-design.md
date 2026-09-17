# M7 W2 A0 — the signals surface, and the per-toggle contract every W2 heuristic follows

Date: 2026-09-17. Author: the hven W2 A0 implementer lane. Status: a DESIGN
PAGE. **No code lands with it.** Branch `m7`, HEAD at the start of the task
`465e5320` (the A-SQP read's close), and every `file:line` below was re-located
at that HEAD.

Authority: the M7 brief of record `docs/notes/2026-09-m7-brief.md` §1 W2 A0 —
"every detection signal a heuristic will read is reachable from the result or
the iteration callback (promote the ledger-only walk degeneracy fractions and
the trace-only IPM inertia/rank evidence); every heuristic below is one named
field with its validation and its OFF state" — and §0's settings contract:
"every heuristic and every switching mode is a named, individually
enable/disable-able field with hand-written validation … nothing adaptive ships
only as a bundle; enabled-defaults come from TRAIN measurement."

Inputs: `.superpowers/m7-planning-facts.md` §C.1–C.8 (the five signal families,
their producers and their reach; the no-mid-solve-handoff finding; the survey's
triggers), the A-SQP read `docs/notes/2026-09-m7-w2-sqp-cutest-read.md` §2 and
§5 (the five mechanisms M1–M5 and the seven signals S1–S7 that round could not
read), `docs/counters.md`, `include/hven/core/solver_counters.h`,
`include/hven/drivers/solve_result.h`, the two options headers and
`docs/trace-schema-v0.md`.

---

## 0. The three findings this page rests on, stated before the tables

**(i) No detection signal is reachable from the per-iteration callback, and W0
T2 did not change that.** `IterationEvent`
(`include/hven/drivers/solve_result.h:311-368`) carries `iteration`, `phase`,
`depth`, `f`, the four declared diagnostics, `step_norm`, `radius`, `mu`, four
borrowed vectors and `elapsed_seconds` — no active-set count, no
weak-activity count, no inertia, no stall and no degeneracy field. W0 T2 added
`IterationPayload` (`:247-263`), which is a **subtractive** mask: `kScalars`,
`kVectors`, `kDiagnostics`, `kAll = kVectors | kDiagnostics`. It lets a consumer
ask for LESS than the pre-existing shape; it adds no new quantity. So the
brief's phrase "reachable from the result or the iteration callback" is today
satisfied, for every signal in §1, by the RESULT arm alone.

**(ii) The only LIVE surface is the trace sink, and it cannot hand off.**
`CallbackAction` has exactly `kContinue` and `kStop`
(`include/hven/drivers/solve_result.h:199-212`); `set_options` throws
`std::logic_error` "if a solve is in flight on this driver"
(`drivers/sqp_solver.h:988-990`, `ipm_solver.h:308`); `attach_trace` is "LEGAL
BETWEEN SOLVES ONLY, on `set_options`' rule and for its reason"
(`sqp_solver.h:728`, `:745`); and the export snapshot is written "by the engine
at solve exit and by nothing else" (`solve_result.h:186-187`), with
`export_warm_start()` throwing if no solve has COMPLETED (`sqp_solver.h:1202`). Facts §C.6 is
confirmed at HEAD. **Every W2 heuristic is therefore either ENGINE-INTERNAL
(A1, A2, A3, M1–M5 — the engine reads its own state, which needs no promotion
at all) or BETWEEN-SOLVE (A4, A5, A7 — the layer reads a finished
`SqpResult`/`IpmResult`).** No W2 heuristic is an in-flight consumer of a
published signal, and A0 does not build the seam that would make one possible.

**(iii) So "promotion" in this page is promotion FOR MEASUREMENT, not for
consumption.** A heuristic that reads `qp_failures_in_a_row`
(`src/drivers/sqp_solver.cpp:2849` — driver-local state, not a counter) reads
it directly. What nothing can do today is *measure* that it fired, or decide a
default from TRAIN, or let a between-solve layer see it. That is the gap §1
inventories, and it is why the per-toggle contract's clause 4 — a counter that
proves the toggle fired — is the clause that drives the whole promotion plan.

### Reach classes used below

| class | means |
|---|---|
| **R** | on a public result struct post-solve: `SqpResult::counters` (`sqp_solver_types.h:1056`), `IpmResult`'s loose fields, `SqpResult` scalars |
| **H** | `SqpResult::history` rows only (`sqp_solver_types.h:1059`) — per-major, post-solve, and **not** in the 76-column corpus CSV |
| **T** | a trace event, live to an attached `TraceSink` |
| **L** | a `Ledger` record, post-solve, one per QP subproblem |
| **P** | the iteration callback's `IterationEvent` — **empty of every signal in this page** (finding (i)) |
| **I** | internal only, or not produced at all |

### Promotion cost classes used below

| class | what it costs | the precedent |
|---|---|---|
| **none** | already R | — |
| **fold** | one `+=` onto `SqpCounters` from a tier's `QpCounters`/`SsnCounters`, one `X()` entry, `kSqpCountersFieldCount` moves, the arity + offset `static_assert`s at `solver_counters.h:1423`/`:1438`, **the trace `counters` object gains one key** → two golden lines in `tests/sqp/test_trace_writer.cpp` re-derived and `docs/trace-schema-v0.md` §5's "38 direct fields" moves | `polish_ignored`, M6 W5 T8.5 — `docs/trace-schema-v0.md:620-641` states the whole recipe, including "LAST in the table and last in the struct … so every field offset the struct already had is unmoved" |
| **counter** | a newly measured `SqpCounters` field; identical cost to **fold** plus the increment site and its exact-increment doc (`docs/counters.md`'s rules) | same |
| **ipm-field** | a loose field on `IpmResult`. The interior engine **has no aggregate counter struct** (`docs/counters.md`, `drivers/` bullet), no X-macro table and no trace `counters` object, so this moves NO golden trace line — and the interior bench arm writes "a DIFFERENT, 20-column schema" (`bench/bench_corpus.cpp:431`), which this page does not touch | `kkt_analyses_total`/`_this_call` (`ipm_solver_types.h:659`, `:661`) |
| **payload** | an additive `IterationPayload` bit plus its `IterationEvent` fields. **Constraint: a new bit must NOT join `kAll`** — `kAll` (`solve_result.h:261`) is what the one-argument `set_iteration_callback()` installs (`:245-246`), so a bit folded into it re-imposes on every existing consumer exactly the per-event O(n) cost W0 T2 removed | `IterationPayload` itself |
| **options** | +1 field, +1 line in `dump_options()` in declaration position, and the drift pin's constants moved together: `kSqpOptionsFields = 27` / `kSqpDumpLines = 58` / `sizeof(SqpOptions) == 384` (`tests/drivers/test_options_dump.cpp:72`, `:139`, `:112`), or the IPM's `63`/`65`/`376` (`:70`, `:137`, `:108`). Harness-side it moves every retained row's `options_digest` | W0 T3's `elastic_rung_engine`, W0 T4's `ipqp_reseed_after_disproval` — both recorded in that test's own comment |
| **CORPUS COLUMN — REGISTER, DO NOT TAKE** | `bench/bench_corpus.cpp:823`'s `kIpqpColumns = 76` moves, and with it the reader's EXACT-WIDTH list (`:1183-1190`: "14, 31, 37 or 76" — "a partial tail must never be scored") and the provenance stamp `# schema:` (`:873`). **No signal in this page requires one.** A `SqpCounters` field is NOT a corpus column: the CSV's columns are hand-listed, so the two move independently, and M6 W6's close line records the same separation ("no bench column added — the 76-column schema is untouched by the whole window", `docs/notes/2026-08-m6-ledger.md:2204`) |

**The corpus-schema invariant, stated once:** a new corpus column is a declared
schema move and is **not decided here**. Every promotion §3 orders is a
`SqpCounters` field, an `IpmResult` field, or harness-side.

---

## 1. The signal inventory

Every detection signal a W2 heuristic could read. Grouped by facts §C's five
families, then the A-SQP read's S1–S7. "Mechanism / heuristic" cites the read's
M1–M5 and the brief's A1–A7.

### (a) Active-set stability and bulk-flip rate — facts §C.1

| signal | producer (HEAD) | reach | serves | promotion |
|---|---|---|---|---|
| `SqpIterate::active_set_delta` | `drivers/sqp_solver_types.h:941` | H + T | A4 (the survey's per-family criteria), A5 | none for a between-solve reader; the row is post-solve by construction |
| `SqpCounters::active_set_delta_total` / `_peak` | `core/solver_counters.h:1314`, `:1318` | R + T | A4, A7 | **none** |
| `SqpIterate::active_rows` / `active_lower_sides` / `active_upper_sides` | `sqp_solver_types.h:961`, `:964`, `:966` | H + T | A1 (the active-set margin against the accuracy floor), A4 | none. NOTE the census counts REAL bound sides only — a variable the trust region holds reports `kFree` and flags `tr_active` (`docs/trace-schema-v0.md:672-675`), which is exactly the distinction M3's second candidate turns on |
| `SsnCounters::ssn_bulk_flips` | `solver_counters.h:225` (flat dup `core/ledger.h:123`) | R + T + L | A4 | **none** |
| `QpCounters::ws_adds` / `ws_drops` / `ws_adds_bound` / `ws_drops_bound` / `distinct_ineq_added` / `distinct_bound_added` | `solver_counters.h:117`, `:125`, `:161`, `:165`, `:175`, `:183` | **L only** | A4, M3 | **fold** ×6 if a heuristic wants the walk's own churn; none is needed by any W2 fix on the read's list, so this stays REGISTERED, not ordered |
| `SqpCounters::ip_activity_inferred` | `solver_counters.h:1304` | R + T | A5 (the crossover composition's own record) | **none** |
| `SqpWarmStart::qp_working_set` | `warmstart/sqp_warm_start.h:200` | R | A5 | **none** |

### (b) Weak-activity fraction — facts §C.2

| signal | producer (HEAD) | reach | serves | promotion |
|---|---|---|---|---|
| `SqpIterate::weak_active_rows` / `near_active_rows` | `sqp_solver_types.h:949`, `:959` | H + T | **A1** (its whole premise), A4 | none for the between-solve reader |
| `SqpCounters::weak_active_peak` / `near_active_peak` | `solver_counters.h:1322`, `:1326` | R + T | A1, A4 | **none** |
| `kWeakActivityMargin = 1e-6` | `sqp_solver_types.h:319` | a public constant, **not a setting** | A1 | **options** if A1 needs to move it. M7-20 is the PRECONDITION the brief already names for anything reading `near_active_rows` |
| `IpqpCounters::ipqp_face_uncertain` | `solver_counters.h:747` (via `SqpCounters::ipqp`) | R + T | A3, A4 | **none** |
| `IpqpTraceRouteEvent::uncertain` / `face_rows` / `face_bounds` | `drivers/trace.h:119-121` | **T only** | A3 | fold if wanted; `ipqp_face_uncertain` already carries the first of the three at R |
| `SsnCounters::ssn_uncertain_peak` | `solver_counters.h:243` | R + T | A4 | **none** — but it is a DIFFERENT measure (a dimensionless kink band that "never flags a small multiplier on an active row", `docs/trace-schema-v0.md:660-667`) and a heuristic must not read it as the SQP's |
| `ipqp_read_kept_tight_sides` / `_barrier_noise_sides` | `solver_counters.h:890`, `:895` | R + T | A3, A5 | **none** |
| **bound-side weak/near counts** | **not produced** | I | A1 | **counter** ×2. `docs/trace-schema-v0.md:669-670` registers it: "A driver-level kink measure is REGISTERED for M7 if a heuristic wants the SSN's own reading; bound-side weak/near is registered too." That is D19 / M7-18, and it is A1's to take if A1's rule needs a bound-side term |

### (c) Inertia and conditioning evidence — facts §C.3

| signal | producer (HEAD) | reach | serves | promotion |
|---|---|---|---|---|
| `InertiaEvidence{state, n_pos, n_neg, n_zero, zero_is_derived, perturbed_pivots}` | `linear/symmetric_factor.h:103` | a public type, **on no result struct** | A4 | — |
| `inertia` / `zero_derived` / `perturbed` on `ipqp.*` | `drivers/trace.h:84`, `:85`, `:88` | **T only** | A3, A4 | fold candidates; not ordered by W2's five fixes |
| `IpqpTraceRegEvent{dir, rho, delta, reason}` | `trace.h:93` | **T only** | A3 | — |
| `IpqpTraceCertifyEvent::final_inertia` | `trace.h:128` | **T only** | A3 | `ipqp_final_inertia_read` already carries it at R (`solver_counters.h:656`) |
| `ipqp_inertia_retries`, `ipqp_final_inertia_read` | `solver_counters.h:544`, `:656` | R + T | A3, A4 | **none** |
| `ipqp_rho_demanded_max` / `_last` | `solver_counters.h:520`, `:532` | R + T | A3 | **none** |
| `ipqp_ladder_reclimbs`, `ipqp_pivot_reroute_primal`, `_dual_fallback`, `ipqp_iters_ladder_armed_no_advance` | `:575`, `:593`, `:609`, `:623` | R + T | A3 | **none** |
| `ipqp_reg_increases` / `_decreases` | `:675`, `:668` | R + T | A3 | **none** |
| **the NLP-level IPM's inertia story** — `IterateInfo::h_facs_`, `p_pivots_`, `p_pivots_observed_`, `prox_reg_primal_`, `prox_reg_dual_` | `detail/interior/iterate_info.h:47`, `:63`, `:74`, `:58`, `:59` — held by reference on `ipm.iter` | **T only** | A4 (per-family engine selection), A2 | **ipm-field**. This is one of the two promotions the brief names by name |
| `ipm.message` kind `inertia_exhausted` (`k/p/n/z/expected_p/expected_n`) | `trace.h:653`, `:683-690` | **T only** | A4 | **ipm-field** (a count, not the payload) |
| `IpmResult::last_prox_reg_primal` / `_dual` | `drivers/ipm_solver_types.h:621`, `:624` | R — **last factorized iteration of the last phase only** | A4 | a max-fold would make it a solve-level reading; **ipm-field** |
| `IpmResult::last_kkt_info`, `kkt_factor_counters`, `kkt_analyses_total` / `_this_call` | `ipm_solver_types.h:643`, `:664`, `:659`, `:661` | R | A4 | **none** |
| `row_scale_min` / `row_scale_max` | `trace.h:337-338`; `SqpResult`'s scaling block `sqp_solver_types.h:1019-1020` | T + R | A4 | **none** |
| `QpCounters::symbolic_analyses` → `SqpCounters::symbolic_analyses` | `solver_counters.h:83` → `:1162` | R + T | **M3** (the ACOPR cells' second refactorization trigger) | **none** — it is already aggregated, and it is what separated the two triggers in the read's §M3 |
| `QpCounters::factorizations` → `SqpCounters::factorizations` | `:31` → `:1051` | R + T | M3 | **none** |
| `QpCounters::border_refine_steps` → `SqpCounters::border_refine_steps` | `:35` → `:1132` | R + T | M3 | **none** |
| `QpCounters::schur_updates` | `:32` | **L only — DELIBERATELY NOT AGGREGATED** (`solver_counters.h:986-988`: "schur_updates is deliberately NOT aggregated") | **M3**, and it is the ONE direct reading of "was bordering available?" | **fold** — this is **S4** |
| **any `rcond` / condition-number estimate** | **nowhere in the tree** | I | — | not proposed. "Conditioning evidence" here means inertia triples, perturbed-pivot counts, ladder rungs, regularization shifts and the row-scaling ratio, and `perturbed` is `null` on Accelerate — UNOBSERVED, never zero (`docs/trace-schema-v0.md:277`; `linear/symmetric_factor.h:120-125`) |

### (d) Progress stagnation — facts §C.4

| signal | producer (HEAD) | reach | serves | promotion |
|---|---|---|---|---|
| `IpqpStallEvidence{fired, window, mu_ratio, residual_improvement, min_alpha, …}` | `detail/qp/ipqp_evidence.h:32-57`, nested at `trace.h:135-137` | **T only** | A3 | deliberately not counters -- "in the stall escape's own evidence block, never as counters here" (`solver_counters.h:822`, and `:838` again); leave as is |
| `ipqp_escape_stall` / `_budget` / `_indefinite` / `_numerical` / `_infeasible_suspect` | `solver_counters.h:839`, `:832`, `:851`, `:862`, `:870` | R + T | A3, A4 | **none** |
| `SqpOptions::ipqp_stall_window` | `sqp_solver_types.h:250` | a public option | A3 | **none** — it is already the per-toggle shape |
| `QpCounters::suspect_escalations` → `SqpCounters::suspect_escalations` | `solver_counters.h:63` → `:1149` | L → R + T | A4; the W2 register's suspicion item | **none**. The read's §4 measures it at **0 on every one of the 15 CUTEst cells with rows**, which its own header calls the expected reading |
| `ipm.message` kind `feasibility_stall` | `trace.h:655` | **T only** | A4 | **ipm-field** (a count) |
| `SolveStatus::kStalled`, `IpmPhaseStatus::kStageStalled`, `kFeasibilityStall` | `core/solver_status.h:85`, `:100`, `:146` | R — **terminal only** | A4 | — |
| **a per-major stagnation count on the SQP driver** ("majors since the last residual decrease") | **not produced** | I | A4; D31 / M7-27's "α = 1 but nothing moves" fourth stall form | **counter**. A consumer can derive it from `SqpResult::history`'s residual columns, so it is a convenience, not a gap — REGISTERED, not ordered |

### (e) Degeneracy indicators — facts §C.5

| signal | producer (HEAD) | reach | serves | promotion |
|---|---|---|---|---|
| `QpCounters::degenerate_steps`, `degenerate_run_max` | `solver_counters.h:143`, `:151` | **L only** | A4, M3 | **fold** ×2 — the first of the brief's two named promotions. `solver_counters.h:97` calls `degenerate_steps / minor_iters` *the* degeneracy fraction, and `:141-142` states that "qp_engine.h implements no anti-cycling rule and this field changes no decision" (the field at `:143`) |
| `QpCounters::drop_ties`, `ratio_ties` | `:189`, `:199` | **L only** | A4 | **fold** ×2, with the pair above |
| `SqpCounters::weak_active_peak` (strict-complementarity proxy) | `:1322` | R + T | A1, A4 | **none** |
| `ipqp_face_uncertain`, `ipqp_refine_refused` | `:747`, `:763` | R + T | A3 | **none** |
| `ipm.message` kind `rank_deficiency` — **no payload at all**, and several may fire per iteration | `trace.h:651`, `:660-663` | **T only** | A4 | **ipm-field** (a count) — the second of the brief's two named promotions |
| `ipqp_escape_indefinite` | `:851` | R + T | A3, A4 | **none** |

### The A-SQP read's S1–S7 — `docs/notes/2026-09-m7-w2-sqp-cutest-read.md` §5

| # | signal | producer today | reach | mechanism | promotion |
|---|---|---|---|---|---|
| **S1** | `dump_counters()`, the twin of `dump_options()` | nothing — `dump_options` exists at `ipm_solver_types.h:746` and `sqp_solver_types.h:743`, its counters twin does not | I | every mechanism: 20 of `SqpCounters`' 38 direct fields, the whole elastic ladder included, reached no artifact this round | **a new free function**, not a counter and not an option. The X-macro tables (`HVEN_SQP_COUNTERS_FIELDS` at `solver_counters.h:1372`) already generate the trace's `counters` object at `src/drivers/trace_writer.cpp:445-458` — so the dump is a second consumer of an existing table, and it **cannot drift**: the arity assert at `:1423` already refuses a field with no `X()` entry. No corpus column, no counter, no golden trace line |
| **S2** | `QpStatus::kMaxIter` exits and `kNumericalError` exits, **counted apart** | neither exists. `map_status` folds three verdicts into one arm (`src/drivers/sqp_solver.cpp:6268-6277`) and `rejected_steps` conflates them BY DESIGN ("Tell them apart by the row's `qp_status`", `solver_counters.h:993-995`); `SqpIterate::qp_status` (`sqp_solver_types.h:849`) carries the fact at **H** and no consumer surface publishes it | I (H for the per-major fact) | **M1** | **counter** ×2 on `SqpCounters` |
| **S3** | `qp_failures_in_a_row`'s peak, and the count of routed failures | driver-local state only, `src/drivers/sqp_solver.cpp:2849`, incremented `:5755`, cleared `:5806`/`:4606`, and the gate that ends the solve reads `== 0` at `:5754` | **I** | **M1** | **counter** ×2 (a max-fold and a sum) |
| **S4** | `schur_updates` aggregated | `QpCounters::schur_updates` (`solver_counters.h:32`), explicitly NOT summed (`:986-988`) | **L only** | **M3** | **fold** — one `+=` beside `factorizations` |
| **S5** | a restoration excursion record `(h_in, h_out, f_in, f_out)` per entry | nothing. `restoration_iters` (`:1060`) is a count, and the phase "produces no history rows" by contract (`sqp_solver_types.h:353`) | **I** | **M2**, **M5** | **counter** (max-folded ratios: the cheapest shape that fits `SqpCounters` without a new event) **or** a `sqp.restoration` trace event (a schema move on `docs/trace-schema-v0.md`, larger). §3 orders the counter shape |
| **S6** | a start-point clamp count | nothing on either engine. The SQP validates `x0`'s size and finiteness and stops — "The box is checked at the model boundary, not here" (`src/drivers/sqp_solver.cpp:2490-2514`); the IPM's interior push moves bounded coordinates with no count at all (`src/drivers/ipm_solver.cpp:491-543`) | **I** | **M4** | **counter** on `SqpCounters` + its twin as an **ipm-field** |
| **S7** | the history, or a trace, published by the bench lane | `SqpResult::history` (`sqp_solver_types.h:1059`) already carries `qp_status` (`:849`), `qp_minor_iters` (`:851`), `qp_factorizations` (`:853`), `verdict` (`:864`), `tr_radius` (`:808`), `violation_l1` (`:803`) and `elastic_applied` (`:890`); `JsonLinesTraceSink::flush()` landed at W0 T5(c) | **H + T, already sufficient** | M1, M2, M3, M5 | **HARNESS-SIDE — not hven's.** Register for H5/H6; §3 does not design it |

**Inventory totals — 53 rows.** By reach class, counting each row once by the
class its own cell names:

| reach | rows | note |
|---|---:|---|
| already at **R** (alone or with T/L) | **24** | no promotion needed; A4's pre-registered rule may name any of these today |
| **H** only (plus T) | **4** | a `SqpResult::history` row a between-solve reader already has, and which the 76-column corpus CSV does not carry |
| **T** only | **9** | live to an attached sink, invisible to a result reader — 8 of the 9 are IPQP or IPM |
| **L** only | **5** | one `Ledger` record per QP subproblem; the walk tier's whole degeneracy and churn story |
| **I** — internal, or not produced at all | **8** | S1–S3, S5, S6, the bound-side weak/near counts, the driver-level stagnation count, `qp_failures_in_a_row` |
| neither — a public constant, a public option, a public type on no result struct | **3** | `kWeakActivityMargin`, `ipqp_stall_window`, `InertiaEvidence` |

**Zero of the 53 are reachable from `IterationEvent`**, which is finding (i)
counted rather than asserted.

---

## 2. The per-toggle contract

Stated once here, then applied. It is the M7 brief §0's settings contract made
checkable, and its shape is not invented: `SqpOptions::budget_mode`
(`sqp_solver_types.h:478`) already reads "**DEFAULT FALSE, and off reproduces
plain kMaxIter-at-the-last-iterate behaviour byte-identically**", which is
clauses 3 and 4 in one sentence.

**A W2 heuristic ships as, and only as:**

1. **ONE named field**, on `SqpOptions` (`sqp_solver_types.h:364`) or
   `IpmOptions`, at a declared position, never a bundle and never a preset.
   `sqp_preset()` returns a default-constructed value and is "the only name
   until M8's labeled configs" (`sqp_solver_types.h:716-723`) — M7 adds no
   second preset.
2. **Hand-written validation** in `validate()` (`sqp_solver_types.h:714`;
   `ipm_solver_types.h:686`), written as **the NEGATION of the acceptance
   condition so that NaN is rejected rather than admitted** (`:700-702`),
   throwing `std::invalid_argument` "naming the option and the value it had"
   (`:705`). A toggle with no refusable value still gets its line, because the
   next field added beside it inherits the habit.
3. **An OFF state that is byte-identical to today**, and the default IS that
   state at the moment the field lands. The proof is fixed and is the same
   three legs for every toggle:
   - the U0 27-cell three-arm replay against the M6 close HEAD's committed CSVs
     (`docs/notes/data/2026-09-m6-close-gate/replay/head-{walk,ssn,ipm}.csv`)
     through `scripts/compare_replay.py`, expecting **27/75/0 ×3**;
   - `tests/sqp/test_corpus_cells.cpp` and the golden trace bytes
     (`tests/sqp/test_trace_writer.cpp`) unmoved;
   - **P-SYM** (`scripts/psym_compare.sh`) over the library objects, expecting
     every object byte-identical except the TUs the field is declared and
     printed in, and there only by only-after symbols.
   **A toggle whose OFF state moves a replay cell is not OFF**, and the finding
   is the diff, not the toggle.
4. **An ON state proved by a counter** — one already at **R**, or one §1 marks
   for promotion — that is non-zero exactly when the heuristic fired and zero
   when it did not. **No W2 toggle ships whose firing cannot be read from
   `SqpResult::counters` or `IpmResult`.** This clause is why §3 exists.
5. **The dump, in the same commit.** The field gets its `name = value` line in
   `dump_options()` in declaration position, and the drift pin's constants move
   together — `kSqpOptionsFields` 27 → 28, `kSqpDumpLines` 58 → 59, and
   `sizeof(SqpOptions)` off its 384 (`tests/drivers/test_options_dump.cpp:72`,
   `:139`, `:112`); the IPM's are 63, 65 and 376 (`:70`, `:137`, `:108`), with
   the Accelerate arm's 65/67 beside them. Harness-side this moves every
   retained row's `options_digest`, which is a declared re-derivation on the
   bench lane, not a gate here.
6. **The TRAIN read decides the default, and only TRAIN.** A measured win over
   the toggle's own OFF state on H2's TRAIN split; the holdout is not run until
   M8. W3's rule applies verbatim: **a lever that cannot show a win stays at the
   shipped default**, and a default move is its own declared event with the
   replay re-derivation budgeted.
7. **Labelling.** A toggle that "selects WHETHER or HOW such a reading is
   taken" carries the `CURVATURE SOURCE:` label (`docs/counters.md`, the
   curvature-source section); an **engine selector** does not — `qp_mode` and
   `elastic_rung_engine` are deliberately unlabelled "because every kernel reads
   curvature on its own exit". A4's selection rule is an engine selector and is
   therefore **unlabelled by rule**.

### 2.1 The contract applied — A1–A5, A7 and M1–M5

Field names are PROPOSALS. A0 proposes no default that differs from today's
behaviour; every "OFF = today" below is the shipped default at landing.

| heuristic | proposed field | validation refuses | OFF proof | fired-counter | TRAIN read |
|---|---|---|---|---|---|
| **A1** — stopping-μ vs bound geometry | `SqpOptions::active_margin_stopping` (bool) + `active_margin` (double) if `kWeakActivityMargin` must move | a non-positive or NaN margin; `!(m > 0)` form | OFF ⇒ the margin is unread; replay 27/75/0, corpus + golden unmoved | `weak_active_peak` / `near_active_peak` (`solver_counters.h:1322`, `:1326`) already at R; **plus** a `stopping_margin_deferred` count if the rule ever defers a stop | the gate-8 Σ≈250 fixture, `e1_f7_n20000_af30_m1e-6`, and the C1 accuracy/stopping coupling, against OFF. **PRECONDITION: M7-20**, which the brief already names for anything reading `near_active_rows` |
| **A2** — complementarity-derived `mu_0` | `IpmOptions::mu0_rule` (enum: `kFixed` = today, `kComplementarity`) | `kMuRuleCount` refused by name, on `QpMode::kQpModeCount`'s precedent (`sqp_solver_types.h:712`: "names no kernel and is refused") | OFF = `kFixed` ⇒ the arithmetic that runs today runs; replay's `head-ipm.csv` unmoved | an **ipm-field** `mu0_rule_used` (or the existing first-iterate `mu` on `ipm.iter`, promoted) — today nothing on `IpmResult` says which rule set `mu_0` | per family on TRAIN against the measured fixed default; the per-family table (HS 1e-3, else 1e-2) becomes a measured choice, not a table |
| **A3a** — `ipqp_converge_slack` as an adaptivity lever | **exists**: `SqpOptions::ipqp_converge_slack = 1e2` (`sqp_solver_types.h:214`) | already validated with the IPQP block, unconditionally (`sqp_solver_types.h:710-711`) | its current value IS the OFF state; no new field, no dump move | `ipqp_escape_*` family + `ipqp_iters_*` at R | the 29-cell reading (slack 1 = +47 iterations) re-taken on TRAIN |
| **A3b** — `kIpqpLadderSkipAfter` | `SqpOptions::ipqp` gains `ladder_skip_after` (Index), today a `constexpr int = 3` (`detail/qp/ipqp_engine.h:92`) | a negative value | OFF = 3 ⇒ byte-identical; **`kIpqpOptionsFields` 22 → 23** and `kSqpDumpLines` 58 → 59 | `ipqp_ladder_reclimbs` (`:575`), `ipqp_iters_ladder_armed_no_advance` (`:623`) at R | TRAIN; the sticky trial stays NOT ADOPTED unless TRAIN reverses it |
| **A4** — static per-family selection vs switching | **no hven field at all if static selection wins** — it ships as configuration guidance over the existing `qp_mode`. If it loses, A5 carries the field | — | nothing changes in hven | `qp_mode` is printed by `dump_options()`, so every row already records the arm | the DECISION RULE is **pre-registered before any TRAIN run** (which counters, which margin, over which families) — the brief's own clause, and A0 adds only that the counters it names must all be at **R** on the day it is registered, which §3 is ordered to make true |
| **A5** — the between-solve switching layer | `SqpOptions::auto_switch` (bool) + its trigger fields, **only if A4 says so** | a trigger threshold that is NaN or out of range | OFF ⇒ no export is staged and no second solve is entered; the explicit user composition (`ipm_solver.h`'s `solve` → `export_warm_start()` → `to_sqp_warm_start()` → `SqpSolver::solve`) is UNTOUCHED, which is the design constraint | `polish_ignored` (`:1369`), `ip_activity_inferred` (`:1304`) and `start_level_used` — facts §C.7's "observable record of *which* composition happened", which a solver-initiated switch must not make ambiguous | TRAIN, per family |
| **A7** — the `ws_algebra` selection rule | `QpOptions::ws_algebra` **exists** (`qp/qp_types.h:141`); the RULE is `SqpOptions::ws_algebra_rule` (enum: `kFixed` = today, `kByProvenance`) | the enum's count sentinel | OFF = `kFixed`; **note** `QpOptions`'s reuse key covers all nine fields by construction (`qp_types.h:182-187`) so a *value* change is already fingerprinted — adding the rule to `SqpOptions` adds no `QpOptions` field and moves no reuse key | `schur_updates` once folded (**S4**) is the direct reading of which algebra ran | TRAIN against the fixed choice |
| **M1(1)** — report the cap as itself | **no toggle.** Additive counters only (S2, S3) | — | additive; replay 27/75/0 by construction | the S2/S3 counters themselves | none — it is a measurement, not a heuristic |
| **M1(2)** — the failure budget | `SqpOptions::qp_failure_budget` (Index) | `!(k >= 1)`; a zero budget would refuse the first retry the driver takes today | **OFF = 1**, which is `st.qp_failures_in_a_row == 0` at `src/drivers/sqp_solver.cpp:5754` exactly | S3's `qp_failures_peak`; `rejected_steps` (`:1053`) moves with it | ACOPP57 and ACOPP118 to `optimal` **without SMBANK or ZAMB2-8 leaving it**. **A declared re-derivation of `head-{walk,ssn,ipm}.csv` is budgeted, not hoped against** — this toggle at k>1 changes trajectories on any cell that ever fails a subproblem |
| **M1(3)** — re-solve at a raised cap | `SqpOptions::qp_cap_retry` (bool) | — | OFF = false | S2's `qp_maxiter_exits` plus a `qp_cap_retries` count | as M1(2); same re-derivation |
| **M2** — the restoration resume rule | `SqpOptions::restoration_resume_rule` (enum: `kUnconditional` = today, `kJudged`, `kObjectiveBounded`) | the count sentinel; a non-positive or NaN bound under `kObjectiveBounded` | **OFF = `kUnconditional`**, which is `if (h_r <= opts_.feas_tol)` at `src/drivers/sqp_solver.cpp:4577-4580` unchanged | **S5**'s excursion record, plus a `restoration_resume_refused` count under the two non-default arms | STEENBRB to `optimal` at `9075.855`, **ZAMB2-8 unmoved** (it takes 11 restoration iterations and still converges). `tests/sqp/test_sqp_restoration.cpp` including the certificate's own re-derivation. **The settler rules on the shape before the field is named** — the read argues against all three |
| **M3** — the border-stack latch | **no new field.** `QpOptions::schur_cap` exists (`qp_types.h:139`, default 128); the change is a **sentinel reading** on it — `0` = size-derived — on `max_iter`'s own discipline (`qp_types.h:117-138`) | `!(cap >= 0)` | **OFF = 128 literal.** A sentinel that is not the default costs no replay motion at the default | **S4**'s folded `schur_updates`, plus a `schur_latch_engaged` count | no cell's `factorizations` grows over the 15 cells with rows. **Widest blast radius on the list**: it moves both engines' trajectories, the QP suites (`test_qp_engine*.cpp`, `test_schur.cpp`, `test_border_ops.cpp`), and — if the VALUE moves — the reuse key and the options digest of every retained bench row on both lanes |
| **M4** — the out-of-box start point | `SqpOptions::start_point_projection` (enum: `kRefuse` = today, `kClamp`), and its IPM twin if the IPM's push is to be counted | the count sentinel | **OFF = `kRefuse`**, and the gate is unusually strong: M4 changes no in-box solve, so the three replay CSVs and the corpus suite **coming back UNMOVED is the whole proof** — a clamp that moved one pinned cell would be clamping something it should not | **S6**'s `start_point_clamped` | the three cells producing a solve at all; STEENBRD/STEENBRE are the plausible wins, NET3 is not. A new fixture: an out-of-box `x0` on both engines with the clamp counter read. **It is a contract change and belongs in `docs/` as well as in code** |
| **M5** — YORKNET's uncertified `kInfeasible` | **preferably no toggle.** The narrow fix is REPORTING: `SqpResult::infeasibility_certified` (`sqp_solver_types.h:1148`) already exists and `core/solver_status.h:43-68` already says false means "no claim is made about the model" | — | a documented consumer rule moves no byte | `infeasibility_certified` at R; **S5** and the elastic ladder's existing `elastic_escalations` (`:1059`), `elastic_rho0_ceiling_hits` (`:1114`), `elastic_floor_retries` (`:1124`) — all at R already, and **none reached any artifact this round**, which is S1's case, not a promotion's | YORKNET producing either `optimal` at 14 239.97 or an honest non-answer not spelled `infeasible`. If a status enumerator is chosen instead, it is an enum change with a migration cost and is the settler's, not a toggle |

**One reading that falls out of the table.** Of the five mechanisms, **two
need no option field at all** (M1's label half, M5), **one reuses an existing
field's sentinel** (M3), and only **three** are genuine new toggles (M1's
budget and retry, M2's rule, M4's projection). The settings contract costs W2
about four `SqpOptions` fields, not fifteen.

---

## 3. The promotion plan, ordered by what W2's next tasks need first

The order is the brief's, and it is also the A-SQP read's §3 order read
backwards through clause 4 of §2: each fix's fired-counter must exist before
the fix is measured.

| # | promotion | cost class | what it unblocks | the task it rides |
|---|---|---|---|---|
| **1** | **S6** — `SqpCounters::start_point_clamped` (an Index: components moved), and its **ipm-field** twin for the interior push | **counter** + **ipm-field** | **M4**, which is W2's next fix and the only one whose gate is "nothing moves" | **rides M4's own task.** It is one counter and one `IpmResult` field, and M4 cannot be measured without it |
| **2** | **S2** — `qp_maxiter_exits` and `qp_numerical_exits`, counted apart; **S3** — `qp_failures_peak` (max-fold) and `qp_failures_routed` (sum) | **counter** ×4 | **M1**, both pieces. Today a capped subproblem and a broken factorization are one number in every consumer column | **rides M1's task**, and they are the part of it that is risk-free: additive counters change no trajectory, so they can land ahead of the failure-budget field and be measured against the pinned corpora unmoved |
| **3** | **S5** — a restoration excursion record on `SqpCounters`: the **counter** shape is max-folded ratios (`restoration_f_ratio_max`, `restoration_h_ratio_min` as scaled integers or as doubles beside `ipqp_rho_demanded_max`'s precedent at `solver_counters.h:520`, which is a `double` in a counters struct and therefore not a novelty) | **counter** ×2–4 | **M2**, and it is also the only reading M5 has of how far each of YORKNET's 19 ladder entries climbed | **rides M2's task.** The `sqp.restoration` trace-event alternative is a schema move on `docs/trace-schema-v0.md` and is **registered, not ordered** — the counter shape answers §M2's question and the event does not answer a different one |
| **4** | **S4** — `schur_updates` folded onto `SqpCounters`, one `+=` beside `factorizations` | **fold** ×1 | **M3**, whose whole §M3 is "a correlation with a named mechanism" that this one sum turns into a measurement — it separates the pin latch from the row-pattern rebuild | **rides M3's task**, and it should land at that task's HEAD **before** any `schur_cap` change, so the before/after is readable. It reverses one explicit decision (`solver_counters.h:986-988`), so the commit declares that reversal in the header comment, per `docs/counters.md`'s "an intentional change to a counter's semantics … is declared explicitly" |
| **5** | **S1** — `dump_counters(const SqpCounters &, std::ostream &)`, the twin of `dump_options()` | **a free function**, no counter, no option, no corpus column | every mechanism's TRAIN read, and it retires the harness's hand-enumerated side file, which is why 20 of `SqpCounters`' 38 direct fields reached no artifact this round | **its own small task**, and it is the cheapest item on this list: the X-macro tables already exist and are already `static_assert`ed against their structs, so the dump is a second consumer of a table that cannot drift. It should land EARLY — every row after it records more than the row before |
| **6** | the **ledger-only walk degeneracy fractions**: `degenerate_steps`, `degenerate_run_max`, `drop_ties`, `ratio_ties` folded onto `SqpCounters` (the brief's first named promotion) | **fold** ×4 | **A4**'s pre-registered decision rule, which may not name a counter that is not at R on the day it is registered | **its own small task, before A4's rule is pre-registered.** Nothing in M1–M5 needs it, so it does not ride a fix |
| **7** | the **trace-only IPM inertia/rank evidence** (the brief's second named promotion): a solve-level inertia-retry count, a `rank_deficiency` message count, a `feasibility_stall` count, and the max-fold of `prox_reg_primal`/`_dual` over the whole solve rather than the last factorized iteration of the last phase | **ipm-field** ×4–5 | **A4** again — the asymmetry facts §C.3 names is that the IPQP tier's inertia story is countable while the NLP-level IPM's is trace-only, and A4 compares the two engines | **its own small task, beside (6).** It moves no golden trace line and no corpus column: the IPM has no counters object and the interior bench arm writes its own 20-column schema |
| **8** | **S7** — the history, or a trace, published by the bench lane | **HARNESS-SIDE** | every "what would separate them" in the read's §2, including which of hven's three restoration request sources fed YORKNET's exit | **`hven-bench`'s, registered for H5/H6. NOT DESIGNED HERE.** hven's surface is already sufficient: `SqpResult::history` carries the seven columns the read names, and `JsonLinesTraceSink::flush()` landed at W0 T5(c) |

**What this plan does NOT contain, and deliberately.** No corpus column
(§0's invariant). No `IterationPayload` bit — finding (ii) says no W2 heuristic
is an in-flight consumer, and a bit that is not in `kAll` is read by nobody
while a bit that is in `kAll` re-imposes T2's cost on every existing caller. No
bound-side weak/near counts and no driver-level kink measure: both are
REGISTERED at `docs/trace-schema-v0.md:669-670` (D19 / M7-18) and neither is
needed until A1's rule is written, so A1 takes them or nobody does.

**Cost, totalled.** S2 ×2, S3 ×2, S5 ×2–4, S6 ×1 and S4 ×1 (a fold), plus item
6's four folds — **twelve to fourteen `SqpCounters` fields across the whole of
W2**, each one key in the trace `counters` object,
each one moving `docs/trace-schema-v0.md` §5's direct-field count and the two
golden lines `GoldenLineSqpSolveEndWithEveryCounterDistinct` and
`GoldenLineSqpSolveEndWritesTheAbsenceSentinelsAsNull`
(`docs/trace-schema-v0.md:751`). Those two lines are re-derived **once per
task**, not once per field. Plus five or six `IpmResult` fields, which move no
golden line. Plus one free function. **Zero corpus columns.**

---

## 4. What A0 does not decide

1. **No default moves.** Every toggle §2.1 names lands at today's behaviour,
   and the TRAIN measurement that would move it is each fix's own, run on H2's
   split. The holdout is not run until M8.
2. **No corpus schema move.** `bench/bench_corpus.cpp:823`'s 76 columns and the
   reader's exact-width list (`:1183-1190`) are untouched by every promotion §3
   orders. A new column, if one is ever wanted, is a declared schema move with
   its own re-derivation and its own task.
3. **No mid-solve handoff.** Facts §C.6 is confirmed at HEAD:
   `CallbackAction` is `kContinue` or `kStop`
   (`include/hven/drivers/solve_result.h:199-212`) and nothing else;
   `set_options` throws while a solve is in flight (`drivers/sqp_solver.h:988-990`);
   the sink is fixed for the duration of a solve (`:728`); the export snapshot
   is written at exit only (`solve_result.h:186-187`). **A true
   mid-solve handoff needs a new seam — a mid-solve snapshot point and a resume
   entry — and A0 neither designs nor reserves one.** A5, if A4 calls for it,
   is BETWEEN-SOLVE. That is the whole of what this page says about it.
4. **Not which shape M2 takes.** The read argues against all three
   (judge it / bound the excursion / return the best point) and says the
   decision is the settler's. §2.1 gives the field the shape would wear; it
   does not pick the shape.
5. **Not whether M4 projects or refuses.** §2.1 names the enum and the counter
   either way. The read recommends projecting; that is a contract change and
   belongs in `docs/` as well as in code, and it is the settler's.
6. **Not S7.** The bench lane's publication surface is `hven-bench`'s and is
   registered for H5/H6.
7. **No option is added by this page.** Every field name above is a PROPOSAL
   carried by the task that implements its heuristic, under §2's seven clauses.

---

## 5. Cite drift and one staleness, reported and not resolved

**`.superpowers/m7-planning-facts.md` §C's line numbers were taken at the M7
open (`40356b5a`) and W0 landed source between there and HEAD** — 21 files,
+1742/−207 (`git diff --stat 40356b5a..HEAD -- include/ src/`). Every §C cite
this page reuses was re-located at `465e5320` by field name; the carriers are
all still there and none changed meaning. The systematic drifts, for a reader
holding the facts file:

| facts §C cite | at HEAD | cause |
|---|---|---|
| `solve_result.h:239-295` (`IterationEvent`) | `:311-368` | W0 T2 inserted `IterationPayload` and its two helpers above it |
| `sqp_solver_types.h:855` etc. (`SqpIterate`'s telemetry block) | `+86` lines throughout — `active_set_delta` at `:941`, `weak_active_rows` `:949`, `near_active_rows` `:959`, `active_rows` `:961` | W0 T3/T4 added `elastic_rung_engine` and `ipqp_reseed_after_disproval` with their doc blocks |
| `solver_counters.h:1273`/`:1277` (`active_set_delta_total`/`_peak`) | `:1314`/`:1318`; `weak_active_peak` `:1322`, `near_active_peak` `:1326`, `ip_activity_inferred` `:1304` | W0 added 41 lines to that header |
| `solver_counters.h:159`/`:164` (`ws_adds`/`ws_drops`) | `:117`/`:125` — the `:159`/`:164` cites land on `ws_adds_bound`'s and `ws_drops_bound`'s doc comments (the fields are at `:161`/`:165`) | the facts file cited the doc-comment line for some fields and the field line for others; both are re-located here by name |
| `solver_counters.h:526`, `:626`, `:506`, `:516`, `:553`, `:569`, `:583`, `:595`, `:643`, `:638`, `:715`, `:731`, `:807`, `:800`, `:817`, `:826`, `:834`, `:854`, `:859` (the IPQP block) | `:544`, `:656`, `:520`, `:532`, `:575`, `:593`, `:609`, `:623`, `:675`, `:668`, `:747`, `:763`, `:839`, `:832`, `:851`, `:862`, `:870`, `:890`, `:895` | same header |
| `ipm_solver_types.h:608`/`:617` (`last_prox_reg_primal`/`_dual`) | `:621`/`:624`; `kkt_factor_counters` `:664` | W0 T5(b)'s `dump_options` declaration and its doc block |
| `iterate_info.h:41`, `:57`, `:68`, `:52`, `:53` | `:47`, `:63`, `:74`, `:58`, `:59` | W0 T7's curvature-source labels |
| `trace.h`'s cites (`:84`, `:85`, `:88`, `:93-98`, `:119-121`, `:127-129`, `:135-137`, `:337-338`, `:651-690`) | **unmoved** — `include/hven/drivers/trace.h` is not in W0's diff | — |
| `symmetric_factor.h:103-126`, `sqp_warm_start.h:200`, `qp_types.h:139`, `solver_status.h:85`/`:100`/`:146`, `ipqp_evidence.h:32-57` | **unmoved** | — |

**No contradiction of substance was found between the facts file and HEAD.**
Every §C carrier exists, in the reach class §C assigned it, with the semantics
§C described. The one discrepancy worth naming is not a drift: §C.3's table
lists `IpmResult::last_kkt_info, kkt_factor_counters` at "`ipm_solver_types.h:630-637`
and the `KktFactorization::Counters` field"; at HEAD those are three separate
fields at `:643`, `:664`, plus `kkt_analyses_total`/`_this_call` at `:659`/`:661`
which §C does not list and `docs/counters.md` does.

**One staleness in `docs/`, already reported by the A-SQP read's §6(1) and
still true at HEAD** — `docs/trace-schema-v0.md:699-701`, reader rule 5, reads
"There is no flush at destruction. The stream is BORROWED: the sink never
opens, closes or flushes it", which W0 T5(c) changed
(`include/hven/drivers/trace_writer.h:36-42`: "It DOES flush it, on request
(`flush()`) and once more at destruction"). Nothing on this page depends on
it. **Reported, not resolved** — this task is docs-only on its own page and
does not edit another.

---

## 6. What is here

```
docs/notes/2026-09-m7-w2-a0-signals-design.md    this page
```

No data artifact: A0 measured nothing. It is a durable design record and lives
as a `docs/notes/` entry only — `docs/README.md`'s table indexes the top-level
`docs/` pages and carries no `docs/notes/` row, so it gains none here.
