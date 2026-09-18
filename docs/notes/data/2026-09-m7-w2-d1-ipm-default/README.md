# M7 W2 D1 — the IPM default event: the evidence

The declared re-derivation (CLAUDE.md §7) that made `filter` acceptance over the
`monitored` barrier governor, with `max_iters = 1000`, hven's shipped
interior-point default. Owner-approved 2026-09-18; the ruling is the W2 A4-2
close in `docs/notes/2026-09-m7-ledger.md`.

`INVENTORY.md` is the substance of this directory and is not repeated here: it
is the 65-item pin enumeration taken and committed BEFORE any pin was touched,
its §8 appendix on the eight pins the suite and the re-capture found that the
inventory had misclassified, and its §9 on the corpus growth this event
deliberately did not carry. This file is the index and the verdicts.

## 1. What shipped

Three literals in `include/hven/drivers/ipm_solver_types.h`, nothing else.
`restoration_mode` stays `off`.

| field | before | after |
|---|---|---|
| `acceptance_strategy` | `classic_merit` | `filter` |
| `barrier_governor` | `classic_adaptive` | `monitored` |
| `max_iters` | 500 | 1000 |

The evidence the event ships ON was measured elsewhere and is CITED, not re-run:
hven-bench `40ef0ef` (`docs/w2-a4-2-presets.md`) and harness `75d3eaf`
(`runs/2026-09-m7-w2-a4-2b-probe1000-filter/`), both at hven pin `3904a477`.

## 2. The OFF proof — the old default is still reachable, bit-identically

Two instruments, deliberately different in kind.

**(a) A fixture, over the whole options value.**
`tests/interior/test_ipm_solver_presets.cpp` dumps `ipm_preset("classic")` with
`max_iters = 500` and dumps `IpmOptions{}`, and diffs the two line by line.
`dump_options()` prints every field in declaration order, so this is a diff of
two complete configurations: **exactly three lines differ**, they are asserted
by name, and every other field is byte-identical. The reconstruction also
`validate()`s, so it is a value a consumer can hand to the solver.

**(b) A measurement, over 43 cells.** `hven_sqp_corpus --engine interior` run
under the declared pre-event pin `HVEN_LEG_IPM_PRE_D1_DEFAULTS` reproduces the
previous contents of `bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv`
**byte-identically outside `wall_s`**, twice.

**Corrected in fix1 (MEDIUM-2):** the leg that produced the numbers above had
no wrapper transcript committed anywhere — the bytes were sound (proven by
recomputation against the committed CSVs) but nothing showed the tree was
clean when they were taken. `legs/baseline-recapture-fix1/` is a CLEAN
re-take: build dir removed, re-configured and rebuilt from HEAD `5bf6760c`,
both arms run twice at the same 11-cell scope, with the wrapper stdout
(`wrapper-stdout.txt`, `=== HEAD 5bf6760c dirty-files=0 ===` included)
committed verbatim this time. Result: the pinned arms are byte-identical
outside `wall_s` to the PRE-EVENT baseline (commit `806ca757`'s content, before
`2a75cecd` re-derived the file), the shipped arms are byte-identical outside
`wall_s` to the file AS CURRENTLY COMMITTED, both arms deterministic across
their two captures, and `libhven.a`'s sha256
(`a2840d3a5eaff83cc1a4a23beecce2661edba7a466cc98324e9b3540fc5d92a1`) matches
`PROVENANCE.txt`'s record for `2a75cecd`'s build exactly. The bytes did not
move; only the record's completeness did.

## 3. The verdicts

| gate | result |
|---|---|
| Release suite (`806ca757`) | **2659 / 2659** |
| Debug suite, long test excluded | **2658 / 2658** |
| `Continuation.ProbeBudgetBoundsAFailingProposal`, alone | passed |
| OFF proof (b): pinned capture vs the previous baseline | **byte-identical outside `wall_s`**, ×2 |
| the re-derivation: shipped capture vs the previous baseline | **43 cells × 30 columns, 89 differences** |
| determinism, both arms | 43 × 30, **0 differences**, ×2 each |
| U0 27-cell three-arm replay vs the M6 close pin — **the control** | **27 / 75 / 0** on walk, ssn AND ipm |
| install smoke, both front ends | passed |
| export contract | `linux`, align 32 |
| clang-format, `git diff --check` | clean |
| P-SYM Release | 17 differing = 6 class A + 11 class B, **0 unexplained** (negative control 5, corrected in fix1) |
| P-SYM Debug | 7 differing = 5 class A + 2 class B, **0 unexplained** (negative control 15, corrected in fix1) |

**Added in fix1 — which nine cells moved, and which way** (LOW-3: "89
differences" named no cell). Of the 43 shared rows, exactly nine differ on
`iter_num`/`factorizations`/`stop_reason`/`obj_val`; the other 34 are
byte-identical outside `wall_s`, which is what "89 differences" spreads over:

| cell | iterations | factorizations | stop reason | note |
|---|---|---|---|---|
| `f7_n2000_bound_physics/{MakeParameter,MakeConstraint,RelaxBounds}` (×3) | 5 → 12 | 5 → 12 | `none` → `none` | 2.4× cost, same objective to 9 digits |
| `f7_n5000_bound_physics/{MakeParameter,MakeConstraint,RelaxBounds}` (×3) | 5 → 6 | 5 → 6 | `none` → `none` | |
| `hs071_x1_fixed/MakeParameter/solve_optimize` | 19 → 13 | 19 → 13 | `none` → `none` | |
| `infeas2_spike/MakeParameter/stalled` | 141 → 123 | 142 → 123 | `stage_stalled` → `restoration_locally_infeasible` | status stays `stalled`; see §5 |
| `infeas2_stationary/MakeParameter/resto_infeasible` | 25 → 22 | 30 → 27 | `restoration_locally_infeasible` → `restoration_locally_infeasible` | `obj_val` flips sign (`stalled`) |

Net over the 43 rows: **452 → 449 iterations, 460 → 456 factorizations**. None
of this is a defect — the TRAIN evidence decided the default, not this leg —
it is the declared re-derivation naming what it moved.

The suite grew by 3 (2656 → 2659): two assertions in the inverted drift tripwire
and one direct `classic_adaptive` dispatch pin. Debug is one below Release for
the pre-existing reason — the long test runs in its own leg.

**The replay is the control, not the proof.** The brief named its `ipm` arm for
the OFF proof; that arm is `QpMode::kIpm`, the SQP driver's IPQP *QP subproblem*
engine, and reads no `IpmOptions` field. All three arms are SQP-driver arms, so
27/75/0 is exactly what this event should leave them at, and it is the evidence
that the SQP lane is untouched. Declared deviation; `INVENTORY.md` §5 carries it.

## 4. P-SYM, and the one object that needed a hand

Exit 1 is the expected status for a default event: three default initialisers
moved, so every TU that constructs an `IpmOptions` emits different immediates.
The gate is the classification, and it is 0 unexplained in both configs.

**Corrected in fix1:** the classification below was originally run with
`classify_psym.py`'s `HEADERS` left at M1/M4's SQP list rather than this
task's own three changed headers
(`include/hven/detail/drivers/ipm_solver_presets.h`,
`include/hven/drivers/ipm_solver.h`, `include/hven/drivers/ipm_solver_types.h`).
Reclassified against the right list — pure Python, no rebuild — the 6+11/5+2
partitions and the 0-unexplained verdict both stand, but the Release object
below needs no by-hand resolution at all once the classifier is checking the
headers this task actually edited: `unity_3_cxx.cxx.o`'s dep-log entry lists
`ipm_solver.h`/`ipm_solver_types.h` directly and lands in class B
automatically, the same way ten other objects do. See the dated corrections
appended to `psym/classification-{Release,Debug}.txt` and the fresh
`psym/classification-{Release,Debug}.corrected.txt` transcripts for the full
account, including the negative control (5 / 15, not 93 / 101) and the
unity_3 delta (10 differing symbols, 7 structural, 3 immediate/store-width
changes — not uniformly "same instruction, wider register").

One Release object needed resolving by content rather than by the classifier's
Ninja-dep match under the ORIGINAL (wrong-header) run:
`tests/interior/.../Unity/unity_3_cxx.cxx.o`, a CMake unity TU whose "source"
is a generated aggregate, which no class-A path match can reach. Its two
members are `test_switching_acceptance.cpp` and `test_watchdog.cpp`, **neither
edited by this task**, so it is CLASS B — an includer — and the route is
direct: the build's own dep database lists both edited headers, and
`test_watchdog.cpp` builds `InertSolverContext`, which holds a
default-constructed `IpmOptions`. Debug needed no such record because at
`-O0` no object was left unclassified under either header list. The full
by-hand record (as originally written, kept verbatim) is appended to
`psym/classification-Release.txt` beneath the classifier's own verbatim
verdict; the fix1 correction to it is appended beneath that.

## 5. What is registered rather than fixed

- **Restoration's near-feasible test flips to a local-infeasibility declaration
  under the shipped PAIR** (corrected in fix1 — LOW-4: not isolated to the
  governor. `make_stall_solver` and the bench row pin both
  `acceptance_strategy` and `barrier_governor`, so nothing in this task says
  whether `filter` or `monitored`, or only the pair together, flips the
  verdict; a one-field probe would say which and has not been run), now with
  two independent reproducers: the unit suite's `make_stall_solver`
  (`PowerSpikeProblem`) and the bench leg's `infeas2_spike/.../stalled` row.
  It belongs beside the IPM stall-termination item registered at the W2 A4-1
  close, whose reproducer is `NET3`. Nothing here acts on it.
- **`--cells all` and `expected_interior_cells()` have drifted apart by fourteen
  cells** (`INVENTORY.md` §9).
- Two pre-existing `-Wswitch` warnings over `SolveStatus`, in
  `tests/sqp/test_eqp_refine_ab.cpp:141` and `tests/sqp/test_hs_battery.cpp:239`.
- **The two cap measurements (LOW-5) live only in comments**: the 5/7 callback
  indices and 6/8 phase iterations that pin `kCap = 7` / `max_iters = 7` were
  taken on an unstamped, reverted working-tree probe with no output kept in
  the artifact. Acceptable — the two are mutually consistent (terminal index =
  iterations − 1), the suite at 7 proves both tests' assertions, and the
  assertions bound the value to (6, 8), so 7 is unique given the measurement —
  but the measurement itself is the unrecorded part.
- **Two copies of the pre-event pin, honoured by every binary that links the
  leg (LOW-6)**: `bench/ipm_corpus_leg.cpp:537-541` and `:966-970` write the
  same three pre-event values; two copies can only promise "applies the pin
  exactly as that function applies it" until one is edited without the other.
  `HVEN_LEG_IPM_PRE_D1_DEFAULTS` is read by `configure_interior_solver()`,
  which `hven_sqp_tests` also links — a suite run with that variable set in
  the environment would run any live interior leg at the old defaults.
  Nothing in-tree does this; worth one sentence in the stamp function's
  header doc, not acted on here.
- **Carried forward**: the P-SYM classifier's header list is a per-task input
  and must be set from the task's own `git diff --name-only -- include/`, not
  inherited from the previous brief that used the script (MEDIUM-1, this
  round).
- `restoration_mode = l1_nested` and `soc_recovery_l1`'s NET3 divergence remain
  M8 items, untouched, as the A4-2 close left them.

## 6. The files

```
INVENTORY.md      the 65-item pin enumeration (§1-§7), the eight pins found
                  later (§8), and the corpus-growth decision (§9)
PROVENANCE.txt    toolchain, hardware, date, commits, binary checksums
gates/            both suites, the long test alone, smoke + export, chain3's ledger
legs/             the interior leg: the cells-all transcript whose rc=1 §9
                  explains, and the scoped re-capture's two arms;
                  baseline-recapture-fix1/ is the fix1 CLEAN re-take with its
                  wrapper stdout (MEDIUM-2)
psym/             both configs' compare output and classification, the Release
                  one with the unity-TU resolution appended, and each
                  classification.txt's fix1 correction appended beneath it;
                  classification-{Release,Debug}.corrected.txt are the fresh
                  transcripts re-run against this task's own three headers
wrappers/         every script that produced the above, verbatim, plus
                  stage_recap_fix1.sh (fix1's clean re-take)
```

Nothing in this directory is a wall-clock measurement.
