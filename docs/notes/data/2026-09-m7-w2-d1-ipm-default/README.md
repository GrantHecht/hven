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
| P-SYM Release | 17 differing = 6 class A + 11 class B, **0 unexplained** (negative control 93) |
| P-SYM Debug | 7 differing = 5 class A + 2 class B, **0 unexplained** (negative control 101) |

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

One Release object needed resolving by content rather than by the classifier's
Ninja-dep match: `tests/interior/.../Unity/unity_3_cxx.cxx.o`, a CMake unity TU
whose "source" is a generated aggregate, which no class-A path match can reach.
Its two members are `test_switching_acceptance.cpp` and `test_watchdog.cpp`,
**neither edited by this task**, so it is CLASS B — an includer — and the route
is direct: the build's own dep database lists both edited headers, and
`test_watchdog.cpp` builds `InertSolverContext`, which holds a
default-constructed `IpmOptions`. The delta is a constant store widened from
`xmm0` to `ymm0`: same instruction, wider register, no call or branch moved.
Debug needed no such record because at `-O0` the width choice never arises. The
full record is appended, append-only, to `psym/classification-Release.txt`
beneath the classifier's own verbatim verdict.

## 5. What is registered rather than fixed

- **Restoration's near-feasible test flips to a local-infeasibility declaration
  under the monitored governor**, now with two independent reproducers: the unit
  suite's `make_stall_solver` (`PowerSpikeProblem`) and the bench leg's
  `infeas2_spike/.../stalled` row. It belongs beside the IPM stall-termination
  item registered at the W2 A4-1 close, whose reproducer is `NET3`. Nothing here
  acts on it.
- **`--cells all` and `expected_interior_cells()` have drifted apart by fourteen
  cells** (`INVENTORY.md` §9).
- Two pre-existing `-Wswitch` warnings over `SolveStatus`, in
  `tests/sqp/test_eqp_refine_ab.cpp:141` and `tests/sqp/test_hs_battery.cpp:239`.
- `restoration_mode = l1_nested` and `soc_recovery_l1`'s NET3 divergence remain
  M8 items, untouched, as the A4-2 close left them.

## 6. The files

```
INVENTORY.md      the 65-item pin enumeration (§1-§7), the eight pins found
                  later (§8), and the corpus-growth decision (§9)
PROVENANCE.txt    toolchain, hardware, date, commits, binary checksums
gates/            both suites, the long test alone, smoke + export, chain3's ledger
legs/             the interior leg: the cells-all transcript whose rc=1 §9
                  explains, and the scoped re-capture's two arms
psym/             both configs' compare output and classification, the Release
                  one with the unity-TU resolution appended
wrappers/         every script that produced the above, verbatim
```

Nothing in this directory is a wall-clock measurement.
