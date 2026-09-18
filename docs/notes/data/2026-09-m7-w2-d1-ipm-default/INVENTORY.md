# M7 W2 D1 — THE PIN INVENTORY, taken BEFORE any pin was touched

This file is the first commit of the declared IPM-default re-derivation event
(CLAUDE.md §7). It is the complete enumeration of everything in this repository
whose value, text or verdict depends on the three `IpmOptions` defaults the
event moves, together with the DISPOSITION chosen for each — re-derive at the
new default, or pin explicitly to the pre-event value because the item is about
a mechanism the old default exercised.

Taken at `m7` HEAD `33f68b1e`, tree clean. Nothing below had been edited when
this file was written; the disposition column is the plan, and the evidence for
each disposition lands in this directory's `README.md`.

## 0. The event

| field | before | after |
|---|---|---|
| `IpmOptions::acceptance_strategy` | `AcceptanceStrategies::classic_merit` | `AcceptanceStrategies::filter` |
| `IpmOptions::barrier_governor` | `BarrierGovernors::classic_adaptive` | `BarrierGovernors::monitored` |
| `IpmOptions::max_iters` | `500` | `1000` |

Nothing else. `restoration_mode` stays `off` (the `l1_nested` question is a
registered one-field decision for M8). The new default is therefore the
harness's `ipm-acc-filter` label plus 1000, NOT the `filter_l1` preset.

Evidence the event ships on: hven-bench `40ef0ef` (`docs/w2-a4-2-presets.md`,
`runs/2026-09-m7-w2-a4-2-presets/`, `runs/2026-09-m7-w2-a4-1b-budget-1000/`) and
harness `75d3eaf` (`runs/2026-09-m7-w2-a4-2b-probe1000-filter/`), at hven pin
`3904a477`. Not re-run here; cited.

## 1. How the three fields reach behaviour

- `acceptance_strategy` — `src/drivers/ipm_solver.cpp` component construction,
  and `validate()` combination guard 1.
- `barrier_governor` — `src/drivers/ipm_solver.cpp` component construction, and
  BOTH `validate()` combination guards.
- `max_iters` — the effective per-phase cap, the `ipm.solve.begin` trace field,
  and one `dump_options()` line.

No globalization component reads any of the three off `SolverContext`, so every
component-level unit test that drives a strategy or a governor directly through
`InertSolverContext` is UNAFFECTED by construction. That is why the long list of
mechanism tests in §2.D below needs no edit at all.

### 1a. The guard consequence, and it is the sharpest item in this inventory

`validate()` refuses `never_monotone == true` together with
`barrier_governor == monitored`. With `monitored` becoming the default, **any
caller that sets `never_monotone = true` and does not also set
`barrier_governor` now throws.** Three in-tree tests are in exactly that shape
(§2.A). This is the intended semantics — `never_monotone` is the escape hatch
for running filter/funnel ABOVE a free-only governor, and it is meaningless over
a governor that already carries the monotone safeguard — but it is a behaviour
break for a partially specified options value and it is declared here as one.

## 2. Tests

### 2.A Deterministic breaks — a pin, re-derived or re-pinned, per item

| # | item | what depends on the default | disposition |
|---|---|---|---|
| T1 | `tests/interior/test_ipm_solver_presets.cpp:46-63` `PresetGateClassicMatchesFreshDefaultSettings` | the file IS the default-drift tripwire: it asserts `ipm_preset("classic")` equals a fresh `IpmOptions{}` over the nine globalization fields. Fails on 2 of 9. | **RE-DERIVE, inverted.** The tripwire fired and the conscious decision its header asks for is made here: `classic` KEEPS its pinned pre-event meaning. The test becomes "classic differs from the fresh default in EXACTLY `acceptance_strategy` and `barrier_governor`, and matches on the other seven", plus the §3 OFF fixture. |
| T2 | `tests/drivers/test_options_dump.cpp:219-241` `IpmDumpHeadIsVerbatim` | pins the dump head verbatim, whose first line is `max_iters = 500`. | **RE-DERIVE** to `max_iters = 1000`, with the event named in a comment and the old value stated. |
| T3 | `tests/interior/test_recovery_dispatch_gate.cpp` `ValidateAcceptsFilterWithWatchdog` (~:309) | sets `filter` + `never_monotone = true`, leaves the governor at the default. Under the new default that is guard 2's refusal. | **PIN to the pre-event governor.** The test is about the watchdog composing with a non-classic strategy ABOVE a free-only governor; `barrier_governor = classic_adaptive` written explicitly restores exactly the combination it was written to exercise. |
| T4 | `tests/interior/test_recovery_dispatch_gate.cpp` `FunnelSelectionConstructsFunnelAcceptance` (~:421) | same shape; `set_options()` now throws. | **PIN to the pre-event governor**, same reason: the comment in the test says "never_monotone rather than a governor change, so the only dispatch input that moves is the acceptance one this test is about" — writing the governor explicitly is what keeps that sentence true. |
| T5 | `tests/interior/test_recovery_dispatch_gate.cpp` `FilterSelectionConstructsFilterAcceptance` (~:442) | same shape. | **PIN to the pre-event governor.** |
| T6 | `tests/interior/test_recovery_dispatch_gate.cpp` comment at ~:463-467 | states that `classic_adaptive` "is covered implicitly by every other test in this file, which never touches barrier_governor". The new default makes that claim FALSE. | **RE-DERIVE the claim**: the comment is corrected and the coverage it asserted is replaced by an EXPLICIT `classic_adaptive` construction test, so the free-only governor keeps a direct dispatch pin. |

### 2.B Trajectory-dependent — a default solve whose asserted numbers may move

Every file here runs at least one IPM solve with all three fields at their
defaults. None of them asserts a hard-coded iteration count taken at the old
default (checked); what they assert is statuses, self-relative comparisons
between two arms of the same build, structural block sizes, and counter
relations. The disposition for all of them is therefore **RE-DERIVE BY
RE-RUNNING**: the suite is the evidence, and any cell that moves is listed in
this directory's `README.md` with the reason. Any that turn out to need a value
edited gets its own row there.

`tests/interior/test_ipm_trace.cpp`, `tests/interior/test_ipm_warm_start.cpp`
(the non-preset arms; the `ipm_preset("filter_l1")` arms are immune because a
preset pins all nine fields), `tests/interior/test_ipm_solver_entry.cpp`,
`tests/interior/test_structure_epoch_gating.cpp`,
`tests/interior/test_objective_scale_reporting.cpp`,
`tests/interior/test_nlp_multiplier_seeding.cpp`,
`tests/interior/test_kkt_factorization.cpp`,
`tests/drivers/test_callback.cpp`, `tests/drivers/test_warm_protocol.cpp`,
`tests/drivers/test_concurrency.cpp`, `tests/drivers/test_solve_result.cpp`,
`tests/drivers/test_options.cpp` (its three default HS071 solves),
`tests/sqp/test_crossover_legs.cpp` (the IPM half of the crossover),
`tests/sqp/test_sqp_warm_currency.cpp` (its one IPM producing solve),
`tests/install_smoke/main.cpp` (a default-path cold + warm HS071 solve through
the installed package).

Two named hazards inside that list, recorded now so a later move is not a
surprise:

- `tests/interior/test_ipm_stop_reason.cpp` — every fixture is tuned so a
  CLASSIC-MERIT trajectory lands on a specific stop reason (`kStall`,
  `kRestorationLocallyInfeasible`, `kIterationCap`, `kDiverging`), and several
  arms are parameterised over `opt_ls_mode`, which is a classic-merit-path knob
  and is inert under `filter`. If a fixture no longer reaches its stop reason,
  the disposition is **PIN to the pre-event acceptance strategy** — the test is
  about the stop reason's mechanism, not about the default.
- `tests/interior/test_ipm_solver_entry.cpp` HS071 option-variant comparisons —
  they compare a DEFAULT run against `opt_ls_mode` / `opt_bar_mode` variants.
  Under `filter` the line-search-mode knob no longer drives the accepted path,
  so those comparisons risk becoming vacuous rather than failing. Same
  disposition if so.

### 2.C Explicitly parameterised — safe by construction, listed so the check is on the record

- `tests/drivers/test_options.cpp:~504` `EXPECT_EQ(o.max_iters, IpmOptions{}.max_iters)` —
  written RELATIVE to the default, so it tracks 500 → 1000 with no edit.
- `tests/drivers/test_options.cpp:~270-296` — asserts `common.threads`,
  `print_level`, `start_level`, `kkt_tol`; none of the three.
- `tests/drivers/test_options.cpp:~153-166` — the two combination guards; both
  arms write `barrier_governor` explicitly, so both still refuse.
- `tests/drivers/test_options_dump.cpp` line-count and `sizeof`/arity pins —
  value-independent.
- `tests/drivers/test_console_sink.cpp` + `console_sink_base_transcript.inc`,
  `tests/sqp/test_trace_writer.cpp` — SCRIPTED event streams with
  `max_iters = 200` written into the event by hand. No solve, no default read.
- `tests/interior/test_ipm_trace.cpp:~864` `EXPECT_EQ(field(b,"max_iters"), "40")` —
  explicit.

### 2.D Unaffected by construction — no edit, and the reason

- Every component-level mechanism test that drives a strategy or governor
  through `InertSolverContext` rather than through the engine:
  `test_monitored_governor.cpp`, `test_filter_acceptance.cpp`,
  `test_merit_rules.cpp`, `test_classic_merit_restoration.cpp`,
  `test_funnel.cpp`, `test_soc.cpp`, `test_watchdog.cpp`,
  `test_proximal_restoration.cpp`, `test_l1_restoration.cpp`,
  `test_acceptance_diagnostics.cpp`, `test_nlp_adapter.cpp`,
  `test_nlp_assembly_contract.cpp`. `InertSolverContext` holds a
  default-constructed `IpmOptions`, but no component reads the three fields off
  it.
- `tests/golden_rig/traces_interior_point.cpp` — "interior point" there is the
  LINEAR-ALGEBRA KKT seam (`hven::linear`). Zero `IpmSolver` / `IpmOptions`
  references in `tests/golden_rig/`.
- `tests/warmstart/test_ipm_polish_extension.cpp` — engine-free by declaration.

## 3. Bench, baselines and replay artifacts

| # | item | depends? | disposition |
|---|---|---|---|
| B1 | `bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv` (43 rows, `hven_sqp_corpus --engine interior --cells all`) | **YES.** `bench/ipm_corpus_leg.cpp`'s `configure_interior_solver()` writes `max_iters` from its own lever (200) but writes NEITHER `acceptance_strategy` NOR `barrier_governor`, so every row was measured at the old default acceptance strategy and governor. | **RE-DERIVE at the new default**, declared in the file's own provenance header; AND capture the same leg once more under a declared pre-event pin, which must come back byte-identical to the committed file outside `wall_s`. That pair is OFF-proof (b) — see §5. |
| B2 | `bench/baselines/2026-09-02-t10b-ipm/ipm_baseline.csv` and the U0 27-cell three-arm replay (`docs/notes/data/2026-09-m6-close-gate/replay/head-{walk,ssn,ipm}.csv`) | **NO — and this correction matters.** `hven_sqp_corpus --engine ipm` selects `QpMode::kIpm`, the SQP driver's IPQP *QP subproblem* engine. It is not the interior-point NLP driver and reads no `IpmOptions` field. All three arms are SQP-driver arms. | **UNCHANGED, and run as a CONTROL.** The replay is re-taken at the new default and must still report 27 cells / 75 columns / 0 differences against the M6 close pin on all three arms. That result is the "SQP lane untouched" evidence §2 of the brief asserts. |
| B3 | `bench/baselines/2026-08-16-u0-corpus`, `2026-08-06-corpus`, `2026-08-08-gate-battery`, `2026-08-09-task6b-resweep{,-walk}`, `2026-09-01-t9-ipm` | **NO.** SQP/walk/SSN corpus and QP-engine artifacts; no interior-point NLP solve. `2026-09-01-t9-ipm` is the superseded t9 QP-engine baseline, retained unrewritten. | no edit |
| B4 | `bench/ipm_corpus_leg.{h,cpp}` `InteriorLevers::max_iters = 200` | the leg PINS `max_iters` itself, so the 500 → 1000 half of the event is invisible to it. | no edit to the lever; it is the pre-event pin's carrier (§5) |
| B5 | `bench/crossover_legs.h:~607-615`, `bench/bench_crossover.cpp` | **YES** for the two globalization fields: the leg seeds from `ipm.options()` and writes only `common.print_level`, `max_iters`, and the four tolerances. Not a committed baseline gated in the suite. | covered by the suite re-run (`tests/sqp/test_crossover_legs.cpp`); no committed artifact to re-derive |

**One finding worth stating on its own, because it halves the blast radius:**
NO committed artifact in this repository was measured at `max_iters = 500`.
Every interior-point leg pins its own cap — 200 in `InteriorLevers`, and 1 / 600
/ 200 in the three variants that override it; the crossover leg pins 200 as
well. The 500 → 1000 half of the event therefore moves exactly two things in
this tree: the `dump_options()` head pinned by `test_options_dump.cpp` (T2), and
the header/comment text that quotes the number (D1, D9). Every baseline that
moves, moves because of the two GLOBALIZATION fields.

## 4. Documentation and header text

Every item here is a ONE-LINE statement of the event, never a rewritten
measurement.

| # | item | disposition |
|---|---|---|
| D1 | `include/hven/drivers/ipm_solver_types.h:106` `Default 500` | **RE-DERIVE** to 1000, naming the event and the old value |
| D2 | `include/hven/drivers/ipm_solver_types.h:194-201` `acceptance_strategy` doc (`classic_merit (the default)`) | **RE-DERIVE**; name the event and the old default |
| D3 | `include/hven/drivers/ipm_solver_types.h:202-217` `barrier_governor` doc (`classic_adaptive (the default)`) and the `never_monotone` note | **RE-DERIVE**; the note gains §1a's consequence |
| D4 | `include/hven/drivers/ipm_solver_types.h:31` the usage example `o.max_iters = 200;` | no edit — an explicit override, correct at any default |
| D5 | `include/hven/drivers/ipm_solver_types.h:675-677` `validate()`'s doc of the two combination guards | **RE-DERIVE**: guard 1's parenthetical calls `classic_adaptive` the default |
| D6 | `include/hven/detail/drivers/ipm_solver_presets.h:6-9` and `:54-56` — "`classic` is the stock `Settings{}` baseline", "(bit-identical default path)" | **RE-DERIVE** to "the pre-2026-09-18 default, pinned as literals" (the brief names this one verbatim). The code needs no change: the table was pinned as literals precisely so this could happen. |
| D7 | `src/drivers/ipm_options.cpp:193-206` — guard 1's COMMENT and its **user-visible throw message**, both of which call `classic_adaptive` "the default" | **RE-DERIVE.** Highest severity in this table: it is a string an end user reads at runtime. |
| D8 | `src/drivers/ipm_options.cpp:37-39` — "`classic_merit` never reaches this helper" | no edit; mechanically still true |
| D9 | `src/drivers/ipm_options.cpp:408` — `std::hex turns max_iters = 500 into max_iters = 1f4` | **RE-DERIVE** to the new literal and its hex |
| D10 | `docs/counters.md` | **ONE LINE.** The page has no default-value statement to correct (grep for `default`/`500`/`acceptance`/`governor`/`merit`/`filter` returns nothing); what it owes the reader is the fact that the stock solve now populates the `last_filter_*` / `last_monotone_*` diagnostics that used to sit at their `-1` "not selected" sentinel. |
| D11 | `docs/measurement.md` §7 "The provenance stamp" | **ONE LINE.** No IPM default is stated anywhere in the page; §7's definition of a full artifact stamp lists toolchain/hardware/date/flags and no solver options, which is the structural reason no committed artifact records the two globalization fields (§3 B1). The line records that a leg whose trajectory depends on an option field stamps that field. |
| D12 | `docs/trace-schema-v0.md:376` — the `max_iters` schema row | **ONE LINE** beside it. The row states no value, so nothing is wrong; the line names the event so a reader of an old trace knows which cap it was taken under. (`:455` "dead at the default `restoration_mode == off`" is a DIFFERENT, unchanged field — not a hit.) |
| D13 | `docs/migration/2026-09-m6-api-break.md:313-314` — "same 65 knobs, same declaration order, same **defaults**" | **RE-DERIVE** with an as-of scope: the claim was true of the M6 API break and three of those defaults have since moved, here. |
| D14 | `docs/notes/2026-09-m7-w2-sqp-cutest-read.md:132-142` — quotes `max_iters`' 500 with a `file:106` citation and derives "five times tighter than the IPM lane's" from 100 vs 500 | **DATED NOTE, not a rewrite.** The read is a measurement record taken at the old default; its arithmetic was right when written. A dated line states the new value and that the ratio is now ten. |
| D15 | `docs/notes/2026-09-header-prose-archive.md:3199,3202,3216,3222,4475` — four verbatim archived header lines calling `classic_merit`/`classic_adaptive` the default | **NO EDIT**, by that file's own charter at `:20-21` ("Nothing in this file is a contract... a record of what was once written next to the code"). Recorded here so the omission is deliberate. |
| D16 | `docs/notes/2026-09-m7-ledger.md:867,872,891`; `docs/notes/2026-08-m6-ledger.md`; `docs/notes/2026-09-m6-w5-*.md`; `docs/notes/2026-09-m6-w5-t8-plan.md` | **NO EDIT.** Append-only history. `m7-ledger.md:891` is the AUTHORISING RULING for this event and is cited by the re-derived artifacts. |
| D17 | every `docs/notes/data/**` README / PROVENANCE / CSV | **NO EDIT**, two reasons that agree: the standing rule freezes `docs/notes/data/**` outside this task's own directory, and CLAUDE.md §1's permanent exception keeps a pinned artifact's provenance bytes as they are. An exhaustive grep of all 78 such files found NOT ONE that claims its values were taken "at the stock defaults" — the dependence was invisible in the evidence layer and recoverable only from the driver source. That finding is itself recorded in this directory's `README.md`, together with the clusters it applies to: `2026-09-m6-w5-t8-runtime/**` (the interior attribution set), `2026-09-m6-w6-attached-cost/**`, `2026-09-m7-w0-t2-callback-payload/**`, `2026-09-m6-w5-acceptance/leg/head-interior-capture{1,2}.csv`, `2026-08-m5-w5-crossover/leg_a_ipm_only.csv`, `m4-ipm-wall-leg/**`, `2026-08-24-m4-task9-gate/**`, and the three coverage-summary directories. |
| D18 | `bench/bench_corpus.cpp` `write_interior_provenance_head()` — the interior CSV's `# levers:` stamp | **RE-DERIVE (a defect this event exposes).** The committed baseline's own header claims each row is "reproducible from this file alone", and that is false while two trajectory-determining fields go unstamped. The stamp gains `acceptance_strategy` and `barrier_governor` BEFORE the re-capture, so the new artifact is honest. Interior-only emitter; no other engine's header moves. |
| D19 | `docs/notes/data/2026-08-21-m4-task3-legs/ab_probe_modes.cpp` | **NO EDIT, and it is the counter-example worth naming:** the one artifact in the tree whose producer sets both globalization fields by hand. Its outputs are safe, and it shows the lever gap elsewhere is an omission rather than a convention. |

## 5. The OFF proof: the old default stays reachable, proven two ways

(a) **A fixture.** `dump_options()` of a default-constructed `IpmOptions{}` is
diffed against `dump_options()` of `ipm_preset("classic")` with
`max_iters = 500` written on it. The fixture asserts the two dumps differ on
EXACTLY three lines and names them. Everything else about the two
configurations — all 60-odd other fields — is asserted equal by the same diff.

(b) **The interior leg.** `hven_sqp_corpus --engine interior --cells all` run
twice at the shipped HEAD: once plain, and once under a declared environment pin
that writes the three pre-event default values into
`configure_interior_solver()` ABOVE the leg's own levers, so every explicit
lever and variant override still wins exactly as it did. The pinned run must be
byte-identical to the committed `interior_baseline.csv` outside `wall_s`; the
plain run is the declared re-derivation and becomes the new committed baseline.

The brief's §3(b) named the U0 27-cell replay's `ipm` arm for this role. That
arm is the SQP driver's IPQP QP engine (§3 B2) and reads no `IpmOptions` field,
so it cannot carry the proof; it is run as the control instead, and the interior
leg — which genuinely runs the interior-point NLP driver at the defaults — takes
its place. Declared deviation.

## 6. The count

**65 inventoried items**, in six classes:

| class | count | items |
|---|---|---|
| deterministic breaks, each re-derived or re-pinned with a reason | 6 | T1-T6 |
| default solves whose asserted numbers may move — re-derived by re-running | 15 | §2.B |
| explicitly parameterised, verified safe | 6 | §2.C |
| unaffected by construction, verified | 14 | §2.D |
| bench / baselines / replay | 5 | B1-B5 |
| documentation and header text | 19 | D1-D19 |

Of those, **8 are edits to a pinned or asserted VALUE** (T1, T2, T3, T4, T5, T6,
B1, D18) and **9 are text-only re-derivations** (D1, D2, D3, D5, D6, D7, D9,
D13, D14). Three more are one-line statements of the event in a live doc (D10,
D11, D12) and six are explicit NO-EDIT rulings (D4, D8, D15, D16, D17, D19) —
recorded so that "no edit" is a decision on the record rather than an omission.
The 15 items of §2.B carry no planned edit at all: they are re-derived by being
re-run, and any one of them that turns out to need a value changed gets its own
row in this directory's `README.md` with the reason.

## 7. Not in scope, restated

No preset is tuned. `soc_recovery_l1` is untouched (its NET3 divergence is
registered separately). No stall detector. No harness change — the harness re-pin
is the next task and takes this HEAD. Apple/Accelerate values are UNOBSERVED and
are never adjusted here; the macOS lane reports them.
