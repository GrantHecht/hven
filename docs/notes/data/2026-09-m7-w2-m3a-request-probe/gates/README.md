# M7 W2 M3a — TASK A — the gates evidence

Task A of M3a (`.superpowers/m7-w2-m3a-brief.md`) added ONE instrument field to the
S5 restoration record — `SqpCounters::restoration_sub_status`, the feasibility
sub-solve's own terminal status at the last entry — and changed no behaviour. This
directory holds the raw transcripts of the six-gate chain that was run at that
change's head, plus the fold proofs of the runner that produced them.

Format precedent: `../../2026-09-m7-w2-m2b-proximity/gates-fix2/`. As there, build
and configure logs are NOT committed; the chain log, the suite tails, the replay
arms, the P-SYM raw output and classification, the install-smoke and
export-contract tails, and the fold proofs are.

## What landed, and the pins it moved

Commits: `8e6aacdf` (the field) and `67b81138` (the gate runner, copied from M2b fix
round 2's LANDED one at `252201c7` and re-pointed at this task's artifact dir,
scratch tree, `$BEFORE`, test counts, and mover shape).

`SolveStatus restoration_sub_status = SolveStatus::kOptimal;` sits last in
`SqpCounters`, written at both measured arms from `rs.status` through
`record_restoration_excursion`'s new sixth parameter. There is no separate
"never entered" sentinel: the record's own absence rule is
`restoration_entries == 0`, and the field keeps the enum representation the
dump/trace schema already uses for statuses. (The predecessor raised this as an
open question; the settler ACCEPTED the representation as documented, with no
follow-up.)

Declared and re-derived, by one key, in the same commit:

| pin | before | after |
|---|---|---|
| `kSqpCountersFieldCount` | 49 | 50 |
| `sizeof(SqpCounters)` | 848 | 856 |
| `test_threads.cpp`'s field-table stride | `49 * 8` | `50 * 8` |
| the two golden lines in `test_trace_writer.cpp` | — | one additive trailing key |

Tests: the four `SqpDriverRestorationExcursion` fixtures extended in place, plus
ONE new fixture, `SqpDriverRestorationExcursion.TheSubStatusTellsACutOffSubSolveFromAConvergedOne`
(one model, two budgets; sub-solve `kOptimal` vs `kMaxIter`). Per the process rule
carried over from M2b fix round 2's failed Release gate, that new fixture was run
in BOTH configurations before this chain was detached, and it passed in both here
too: Debug `Test #2429 ... Passed 0.03 sec`, Release `Test #2430 ... Passed 0.01 sec`.

Baseline test counts at `e7ef78b0` were Debug 2675 / Release 2676; the one new TEST
block takes them to the 2676 / 2677 the runner declared before the run.

## The chain

Detached chain, PID **2237591**, HEAD **`67b81138`**, `dirty-files=0`,
`BEFORE=e7ef78b0`, 2026-09-21T06:12:09 → 2026-09-21T07:04:23 America/New_York.
Six lock segments, each with its own `PGREP_EMPTY` line and
`FOREGROUND_START`/`FOREGROUND_END` markers inside the lock.

Expectations were declared IN the runner at `67b81138`, i.e. before the run, not
discovered from it: `EXPECT_DEBUG_TESTS=2676`, `EXPECT_RELEASE_TESTS=2677`,
`EXPECT_LONG_TESTS=1`, replay `27/75/0` over `3` engines, and the P-SYM mover shape
`EXPECT_PSYM_DIFFERING=54`, `EXPECT_PSYM_CLASS_A=6`, `EXPECT_PSYM_CLASS_B=48`.

**An exit code is not a result.** Every number in the table below was re-checked by
this task's successor implementer against the raw transcript named in the last
column — not against the runner's own `ASSERT OK` lines, and not against
`GATES_SUMMARY`.

| gate | expectation | measured | checked against |
|---|---|---|---|
| G1 Debug suite | 0 failed, 2676 total | `100% tests passed, 0 tests failed out of 2676`; 0 `***Failed`/`***Exception` lines | `suite-debug.txt`, ctest summary line read directly |
| G2 the long test alone | 1/1 passed | `Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 670.72 sec`; `100% tests passed, 0 tests failed out of 1` | `suite-debug-long.txt` |
| G3 Release suite | 0 failed, 2677 total | `100% tests passed, 0 tests failed out of 2677`; 0 `***Failed`/`***Exception` lines | `suite-release.txt` |
| G3 the new fixture, both configs | Passed in Debug AND Release | `Test #2429 ... Passed 0.03 sec` (Debug), `Test #2430 ... Passed 0.01 sec` (Release) | `suite-debug.txt`, `suite-release.txt`, grepped by name |
| G4 U0 replay, 3 arms | 27 cells / 75 columns / 0 differences, ×3 (walk, ssn, ipm) | all three arms `cells compared: 27  columns compared: 75  differences: 0` | `replay/comparison.txt` |
| G5 P-SYM, Release | compare rc 1 (expected); `DIFFERING OBJECTS: 54` = class A 6 + class B 48; `UNEXPLAINED: 0`; negative control 54 (≥ 1); both capture rcs 0 | matches on every count; `PSYM_COMPARE_RC_Release=1`, `CLASSIFY_RC_Release=0` | `psym-compare-Release.txt`, `psym-classification-Release.txt`, `gates.txt` lines `capture after-Release: rc=0`, `capture before-Release: rc=0` |
| G5 P-SYM, Debug | the same shape | matches on every count; `PSYM_COMPARE_RC_Debug=1`, `CLASSIFY_RC_Debug=0` | `psym-compare-Debug.txt`, `psym-classification-Debug.txt`, `gates.txt` lines `capture after-Debug: rc=0`, `capture before-Debug: rc=0` |
| G5 negative control is a control | the 54 byte-identical objects are DISJOINT from the 54 movers | the two object lists were parsed out of the classification file and intersected: `overlap = 0` | `psym-classification-Release.txt`, parsed directly |
| G5 classification, Release vs Debug | byte-identical | `diff psym-classification-{Debug,Release}.txt` empty | direct `diff` of the two committed files |
| G5 capture pair | both arms enumerate the same object count | `capture pair object count = 146` in both configs | `gates.txt` |
| G6 install smoke | PASSED, both solvers converged through one generic consumer, 19 standalone-include TUs | `IpmSolver converged, objective = 17.0140174`; `SqpSolver converged, objective = 17.0140173`; `== hven install smoke: PASSED` | `install-smoke.txt` |
| G6 export contract | align = 32 | `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32`; `== export contract OK for platform=linux, align=32` | `export-contract.txt` |
| G6 clang-format | rc 0, over the seven changed C++ files derived from git | `CLANG_FORMAT_RC=0` | `gates.txt` |
| G6 `git diff --check` | rc 0, `DIRTY_FILES=0` | `DIFF_CHECK_RC=0`, `DIRTY_FILES=0` | `gates.txt` |
| stamp | HEAD `67b81138`, `dirty-files=0`, no `-dirty` anywhere | `=== HEAD 67b81138  dirty-files=0 ===`; a recursive grep for `-dirty` over every file in this directory finds nothing | `gates.txt` header line; direct grep |
| runner identity | the transcripts were produced by what landed | `fold-proofs.txt`: `HEAD 67b81138... dirty-files=0`, and all four wrappers `(worktree == HEAD)` by sha256 | `fold-proofs.txt` |
| fold proofs | 43 folds, 0 failed | `FOLD_PROOFS_TOTAL=43  FOLD_PROOFS_FAILED=0` | `fold-proofs.txt` |
| aggregate | `GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`; `GATES_AGGREGATE rc=0` | exact match | `gates.txt` tail |

The P-SYM mover shape is what the field's placement predicts and nothing more: the
six class-A objects are exactly the six sources this task changed
(`sqp_solver.cpp`, `trace_writer.cpp`, `test_callback.cpp`, `test_threads.cpp`,
`test_sqp_restoration.cpp`, `test_trace_writer.cpp`), and the 48 class-B objects are
untouched TUs that include the one changed header,
`include/hven/core/solver_counters.h` — a size change in `SqpCounters` moves their
code without their source moving. `UNEXPLAINED` is empty, and 54 further objects
that also include that header are byte-identical, so "includes the header" alone
does not explain a move.

Two tests did not run in either configuration, as on every gate of this branch:
`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas` (Skipped)
and the two `EqpRefinementAb` cases (Disabled). Wall-clock in these transcripts is
informational only; the counters and statuses are the currency.

## Files

```
gates.txt                               the chain log: HEAD stamp, the declared expectations,
                                         segment boundaries, every lock marker, ASSERT and rc
suite-debug.txt                         the Debug suite (long test excluded)
suite-debug-long.txt                    the long test, alone
suite-release.txt                       the Release suite
replay/head-{walk,ssn,ipm}.csv          the three U0 replay arms at HEAD 67b81138
replay/comparison.txt                   their comparison against the M6-close pin
psym-compare-{Release,Debug}.txt        the raw object-by-object P-SYM output
psym-classification-{Release,Debug}.txt the classification, which IS the gate
install-smoke.txt                       the install smoke tail
export-contract.txt                     the export contract output
fold-proofs.txt                         the fold proofs run against this task's copy of the
                                         runner on the clean tree at 67b81138: 43 folds, 0 failed,
                                         all four wrappers worktree == HEAD
```
