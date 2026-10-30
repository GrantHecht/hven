# M7 W2 M3b — FIX ROUND 1 — the gates evidence

Fix round 1 of M3b (`.superpowers/m7-w2-m3b-fix1-brief.md`, settler adjudications
F1–F4) answered Codex sol's FIX-ROUND (4): it made the walk observer's state
`thread_local` (F1), added the discriminating record fields the re-trace needs
(F3(b)), and replaced the Debug "DWARF only" argument with a fail-closed
per-section comparison over every non-debug byte (F2). This directory holds the
raw transcripts of the six-gate chain run at that change's head, plus both sets
of fold proofs.

`gates/` beside it is M3b task A's frozen evidence and is **not** edited by this
round. Format precedent: `gates/`, which follows
`../../2026-09-m7-w2-m3a-request-probe/gates/`. As there, build and configure
logs are NOT committed; the chain log, the suite tails, the replay arms, the
P-SYM raw output and classification, the **new** section-comparison transcripts,
the install-smoke and export-contract tails, and the fold proofs are.

## What landed

| commit | what |
|---|---|
| `542db548` | F1 + F3(b): the seam header goes `thread_local` and gains the discriminating record fields; `src/qp/qp_engine.cpp` gains the hooks |
| `8fce7fdb` | the 8 new fixtures (3 concurrency + 5 field oracles) and two repairs to the landed helpers |
| `63c2a277` | `docs/testing.md`: the per-thread contract, the qualified record-per-pass claim, the other-observer register |
| `35294210` | F2 tooling: `check_sections_fix1.sh`, `section_digest_fix1.py`, `classify_psym_fix1.py`, `fold_proofs_psym_fix1.sh` |
| `31774717` | `gates-fix1.sh`, `chain_gates_fix1.sh` — the declared shape, before the run; **the chain's HEAD** |

This round moves NO pin: no counter field, no struct size, no golden line. The
only test-count movement is additive — eight new `TEST()` blocks in the existing
`hven_qp_walk_seam_tests` executable, 11 → 19.

## The chain

Detached chain (`setsid`), PID **2579855**, HEAD **`31774717`**, `dirty-files=0`,
`BEFORE=56032eed`, 2026-09-21T14:38:01 → 2026-09-21T15:10:54 America/New_York.
Six lock segments plus a seventh for the fold proofs, each with its own
`PGREP_EMPTY` line and `FOREGROUND_START`/`FOREGROUND_END` markers inside the
lock.

Expectations were declared IN the runner at `31774717`, i.e. before the run:
`EXPECT_DEBUG_TESTS=2697`, `EXPECT_RELEASE_TESTS=2698`, `EXPECT_LONG_TESTS=1`,
replay `27/75/0` over `3` engines, the P-SYM mover shape
`EXPECT_PSYM_DIFFERING=0 / CLASS_A=0 / CLASS_B=0`, and the new per-configuration
debug-only shape `EXPECT_DEBUG_ONLY_RELEASE=0`, `EXPECT_DEBUG_ONLY_DEBUG=3`.

**An exit code is not a result.** Every number in the table below was re-checked
by this task's successor implementer against the raw transcript named in the last
column — not against the runner's own `ASSERT OK` lines, and not against
`GATES_SUMMARY`, `GATES_AGGREGATE` or `CHAIN_SUMMARY`.

**Two rows below are FAILED.** Both are declaration defects, in the runner, not
defects in the change under test; both are analysed in full in the two sections
after the table. Neither gate was re-run to green: no source changed after
`31774717`, so a re-run would measure the same thing and record a number that had
been discovered rather than declared.

| gate | expectation | measured | verdict | checked against |
|---|---|---|---|---|
| G1 Debug suite | 0 failed, 2697 total | `100% tests passed, 0 tests failed out of 2697` | OK | `suite-debug.txt`, ctest summary line read directly |
| G2 the long test alone | 1/1 passed | `Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 675.15 sec`; `100% tests passed, 0 tests failed out of 1` | OK | `suite-debug-long.txt` |
| G3 Release suite | 0 failed, 2698 total | `100% tests passed, 0 tests failed out of 2698` | OK | `suite-release.txt` |
| G1/G3 all 19 `QpWalkSeam.*` fixtures, both configs | every one `Passed` in Debug AND Release, the 11 landed and the 8 new | 19 distinct `QpWalkSeam.*` names each with exactly one `Passed` line in each transcript; the two name sets `diff` empty; 0 `Failed` | OK | `suite-debug.txt`, `suite-release.txt`, grepped by name and tallied |
| G1/G3 the 8 NEW fixtures by name | present and green in both | `ArmingOnOneThreadIsInvisibleOnAnother`, `TwoEnginesOnTwoThreadsBothArmed`, `TwoEnginesOnTwoThreadsOneArmedOneNot` (F1's concurrency three); `EveryPassRecordsTheMainVerdictAndThePathThatProducedIt`, `APostDropPassRecordsWhatWasReleasedAndWhyTheRideDidOrDidNotRun`, `TheDropCandidateSetIsRecordedLargestViolationFirst`, `TheRegularizationAndResidualsAreRecordedOnEveryPass`, `ADegenerateStepIsAnAdditionThatMovedNoFurtherThanTheStepTolerance` (F3(b)'s five field oracles) | OK | both suite transcripts |
| G4 U0 replay, 3 arms | 27 cells / 75 columns / 0 differences, ×3 (walk, ssn, ipm) | all three arms `cells compared: 27  columns compared: 75  differences: 0` | OK | `replay/comparison.txt` |
| G5 P-SYM, Release | compare rc 0 (ZERO movers is what this round declares); `DIFFERING OBJECTS: 0` = class A 0 + class B 0; `UNEXPLAINED: 0`; negative control ≥ 1; both capture rcs 0 | matches on every count; `PSYM_COMPARE_RC_Release=0`, `CLASSIFY_RC_Release=0`, control 72 | OK | `psym-compare-Release.txt`, `psym-classification-Release.txt`, `gates.txt` |
| G5 P-SYM, Debug | the same shape | matches on every count; `PSYM_COMPARE_RC_Debug=0`, `CLASSIFY_RC_Debug=0`, control 71 | OK | `psym-compare-Debug.txt`, `psym-classification-Debug.txt`, `gates.txt` |
| G5 no mover escaped the harvest | 0 `DIFFERS` and 0 `RELOCATIONS` lines in either comparison | `grep -c`: Release `DIFFERS` 0, `RELOCATIONS` 0, `NOISE-ONLY` 0; Debug `DIFFERS` 0, `RELOCATIONS` 0, `NOISE-ONLY` 1 | OK | both raw comparisons, grepped directly |
| G5 the three-way labels are disjoint (F2) | mover / noise-only / byte-identical, no object in two | Release 0 / 0 / 72; Debug 0 / 1 / 71. The one Debug `NOISE-ONLY` object, `hven.dir/src/qp/qp_engine.cpp.o`, is printed under its own heading and is **absent** from the negative control; in Release, where it is byte-identical, it **is** in the control. R5 (no object carrying both verdict lines) asserted, not assumed | OK | both classifications, read side by side |
| G5 capture pair | both arms enumerate the same object count | `capture pair object count = 146` in both configs; `146 object(s)` in each of the four manifests | OK | `gates.txt` |
| G5 cross-configuration | class-B sets equal; UNEXPLAINED 0 in both; class-A sets identical | all six cross-config asserts OK, every set empty | OK | `gates.txt` |
| **G5 sections, Release (F2's fail-closed check)** | 42 objects; every one BYTE-IDENTICAL or DEBUG-ONLY; `NON_DEBUG_MOVERS=0`; `REFUSED=0`; `DEBUG_ONLY=0` as declared | `OBJECTS=42 BYTE_IDENTICAL=42 DEBUG_ONLY=0 NON_DEBUG_MOVERS=0 REFUSED=0 RC=0` | OK | `psym-sections-Release.txt` |
| **G5 sections, Debug — substance** | 42 objects; every one BYTE-IDENTICAL or DEBUG-ONLY; `NON_DEBUG_MOVERS=0`; `REFUSED=0` | `OBJECTS=42 BYTE_IDENTICAL=41 DEBUG_ONLY=1 NON_DEBUG_MOVERS=0 REFUSED=0 RC=0` | OK | `psym-sections-Debug.txt` |
| **G5 sections, Debug — the declaration** | `DEBUG_ONLY=3` | `DEBUG_ONLY=1` | **FAILED** | `gates.txt`: `ASSERT FAIL sections Debug: DEBUG_ONLY is '1' and this round DECLARED '3'` |
| G6 install smoke | PASSED, both solvers converged through one generic consumer, 19 standalone-include TUs | `== hven install smoke: PASSED`; `INSTALL_SMOKE_RC=0` | OK | `install-smoke.txt` |
| G6 export contract | align = 32 | `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32`; `== export contract OK for platform=linux, align=32` | OK | `export-contract.txt` |
| G6 clang-format | rc 0, over the changed C++ files derived from git | `CLANG_FORMAT_RC=0`; derived list `qp_walk_trace.h qp_engine.cpp test_qp_walk_seam.cpp` | OK | `gates.txt` |
| G6 `git diff --check` | rc 0, `DIRTY_FILES=0` | `DIFF_CHECK_RC=0`, `DIRTY_FILES=0` | OK | `gates.txt` |
| stamp | HEAD `31774717`, `dirty-files=0`, no `-dirty` anywhere | `=== HEAD 31774717  dirty-files=0 ===`; a recursive grep for `-dirty` over every transcript in this directory finds nothing | OK | `gates.txt` header line; direct grep |
| runner identity | the transcripts were produced by what landed | both fold-proof transcripts stamp `HEAD 31774717...  dirty-files=0` and print all ten wrapper paths `(worktree == HEAD)` by sha256; the three scripts G5 copies into scratch (`classify_psym.py`, `check_sections.sh`, `section_digest_fix1.py`) were re-hashed against `git show HEAD:<path>` and agree | OK | both fold-proof transcripts; direct `sha256sum` vs `git show` |
| fold proofs, P-SYM layer | 53 folds, 0 failed | `PSYM_FOLD_PROOFS_FIX1_TOTAL=53  PSYM_FOLD_PROOFS_FIX1_FAILED=0` | OK | `fold-proofs-psym.txt` |
| fold proofs, runner (`fold_proofs_m3b.sh`) | 46 folds, 0 failed | `FOLD_PROOFS_TOTAL=45  FOLD_PROOFS_FAILED=1` — one leg SKIPPED, no fold gave an unexpected verdict | **FAILED** | `fold-proofs.txt` |
| aggregate | all green | `GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=1 G6=0`; `GATES_AGGREGATE rc=1`; `CHAIN_SUMMARY gates=1 fold_proofs=1` | records the two above | `gates.txt` tail, `chain.txt` |

Two tests did not run in either configuration, as on every gate of this branch:
`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`
(Skipped) and the two `EqpRefinementAb` cases (Disabled). Wall-clock in these
transcripts is informational only; the counters and statuses are the currency.

## FAILED #1 — G5's Debug `DEBUG_ONLY` declaration: 3 declared, 1 measured

**The substance is better than the declaration, and the gate still failed.** That
is the correct outcome: `gates-fix1.sh` says at its own site that "a shape that
is not the declared one is re-derived and declared again, never discovered", and
a runner that accepted 1 where it declared 3 would be discovering. It is recorded
FAILED here, and it was **not** re-run: no source changed after `31774717`.

### The substantive criterion, verified independently of the failing assertion

* **Release**: all 42 production-library objects BYTE-IDENTICAL. `DEBUG_ONLY=0`,
  `NON_DEBUG_MOVERS=0`, `REFUSED=0`, section-check `RC=0`.
* **Debug**: 41 of 42 BYTE-IDENTICAL; 1 DEBUG-ONLY; `NON_DEBUG_MOVERS=0`,
  `REFUSED=0`, section-check `RC=0`.
* Both arms of the section check (before→after and after→before) agree in both
  configurations; nothing was refused for an unreadable or unmatched section.
* On the one DEBUG-ONLY object, `CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o`,
  the digest enumerates **8556 sections, of which 13 are debug-class and 8543 are
  not**. Exactly **four** digests differ, all debug-class: `.debug_info`,
  `.debug_str`, `.debug_line` (+22 bytes) and `.rela.debug_line`. `.debug_info`
  and `.debug_str` keep their byte sizes exactly. **All 8543 non-debug sections
  are byte-identical**, `.text`, `.rodata`, `.eh_frame`, `.symtab` and `.strtab`
  among them.
* Because `.symtab` and `.strtab` are byte-identical on every one of the 42
  objects in both configurations, no seam symbol reached the production library.
  That is a strictly stronger statement than M3b's `nm -C libhven.a` grep, and it
  is why this round does not repeat that grep.

So: **zero movers, zero non-debug section differences across all 42 library
objects in both configurations, both arms agreeing, nothing refused.**

### Why 3 was declared and 1 measured — established from evidence, not assumed

`gates-fix1.sh`'s declaration block states its own derivation: *"The universe is
unchanged since M3b's own derivation … The changed surface is the same TWO files
that round changed, changed the same way … so the measured shape that round
recorded is the shape declared here."* The declaration was therefore **reused
from M3b's derivation rather than derived against this round's own base**, and
the two rounds do not have the same changed surface.

1. `git log -S'qp_walk_trace.h' -- include/hven/detail/qp/qp_engine.h` names
   exactly one commit: **`01a956a7`**, M3b task A's seam commit. That is where
   the line `#include <hven/detail/qp/qp_walk_trace.h>` entered `qp_engine.h`
   (now at `qp_engine.h:366`).
2. M3b's P-SYM base predates `01a956a7`, so **M3b's changed-header set contained
   `qp_engine.h` itself**. Inserting a line into a header shifts every line below
   it, and so shifts the DWARF line programme of every TU that includes it —
   which is exactly why M3b measured three Debug library movers
   (`qp_engine.cpp.o`, `sqp_solver.cpp.o`, `ipqp_engine.cpp.o`) and 29 Debug
   `NOISE-ONLY` objects overall.
3. **This round's base `56032eed` already contains `01a956a7`.**
   `git diff --name-only 56032eed 31774717 -- include/ src/` returns exactly
   `include/hven/detail/qp/qp_walk_trace.h` and `src/qp/qp_engine.cpp`;
   `git diff 56032eed 31774717 -- include/hven/detail/qp/qp_engine.h` is **empty**,
   and `gates.txt`'s own header line reads `CHANGED HEADERS:
   include/hven/detail/qp/qp_walk_trace.h` — one file.
4. `qp_walk_trace.h` grew, but it **cannot** move any includer's line programme:
   `#pragma once` is at line 4, `#ifdef HVEN_TESTING` at line 132 and the
   matching `#endif // HVEN_TESTING` at line 852, the last line of the file. No
   target in the P-SYM capture set defines `HVEN_TESTING`, so in the production
   build the header contributes no tokens and no line-table entries, and how many
   lines it contains is invisible downstream. The includer's own line numbering
   is unaffected by the length of what it includes.
5. Therefore **only a TU whose own source lines moved can shift its DWARF line
   programme**, and this round moved source lines in exactly one file:
   `src/qp/qp_engine.cpp`. One object, not three.
6. Confirmed object by object, by sha256 over the captured before/after bytes:

   | object | Debug | Release |
   |---|---|---|
   | `hven.dir/src/qp/qp_engine.cpp.o` | `421001552a066f35…` → `3d75f588e53d8f3a…` **DIFFERS** | `bf8b394ee5649c15…` → `bf8b394ee5649c15…` byte-identical |
   | `hven.dir/src/drivers/sqp_solver.cpp.o` | `66ad5aa3360523c1…` → `66ad5aa3360523c1…` **byte-identical** | byte-identical |
   | `hven.dir/src/qp/ipqp_engine.cpp.o` | `39469d44db5dd770…` → `39469d44db5dd770…` **byte-identical** | byte-identical |

   The two objects the declaration expected to move are byte-identical in both
   configurations. The one that moved is the one whose source changed.

The Release arm carries the same evidence from the other side: `qp_engine.cpp.o`
is byte-identical there, which is what `#ifdef HVEN_TESTING` plus the
`__LINE__`-preserving include placement was for, and it is why Release's
`DEBUG_ONLY=0` declaration was met exactly.

## FAILED #2 — the runner fold proofs: one leg skipped on a wrong build path

`chain_gates_fix1.sh` invokes `fold_proofs_m3b.sh "$S/dbg"`, but `gates-fix1.sh`
configures its Debug tree at `$S/gates-dbg`. No `CTestTestfile.cmake` exists at
`$S/dbg`, so the one fold that runs a real `ctest --no-tests=error` over a
no-match filter took the script's else branch —

```
FOLD --  SKIPPED: no configured tree at .../dbg for the --no-tests=error leg
```

— and `fold_proofs_m3b.sh` **fail-closes on a skip** (`fails=$((fails + 1))`),
which is the correct behaviour and is why the defect is visible at all. Hence
`FOLD_PROOFS_TOTAL=45  FOLD_PROOFS_FAILED=1` against M3b's own `46 / 0`.

**Every fold that ran gave its expected verdict.** The 45 were diffed line by
line against `gates/fold-proofs.txt`, M3b's control at the identical
`fold_proofs_m3b.sh` digest (`9d5d27d3…`, `worktree == HEAD` in both
transcripts). The two transcripts agree on the accept/refuse verdict and the
reason line of every fold, with exactly two differences, both accounted for by
the same wrong path: the renumbering that follows the skip, and FOLD 07's reason
line, which reports the negative control as `0` here and `71` there because
`all_objects()` enumerates the build tree at `$S/dbg` and there is none. FOLD 07
REFUSED with rc 5 in both, which is its point.

The single absent leg is belt-and-braces over `--no-tests=error`; the
substantive protection it backs — that a zero-test green, a wrong total, a
nonzero failed count or an empty log cannot pass — is carried by `assert_ctest`
and fold-proven independently at **FOLD 08–11**, all four of which ran and
refused as expected in this transcript. It was not re-run, for the same reason
G5 was not.

## A third defect, registered here and not fixed in this round

`classify_psym_fix1.py` builds its NEGATIVE CONTROL from `all_objects(build)` —
every object in the build tree that includes a changed header — while the mover
and noise sets come from the **capture manifest**, which
`scripts/psym_compare.sh` deliberately restricts to 146 objects and which
excludes the `HVEN_TESTING` seam-test targets. Three of the control's members are
therefore objects that were never compared, printed under the heading "…and are
BYTE-IDENTICAL":

```
tests/CMakeFiles/hven_ipqp_seam_tests.dir/__/src/qp/ipqp_engine.cpp.o
tests/CMakeFiles/hven_qp_walk_seam_tests.dir/__/src/qp/qp_engine.cpp.o
tests/CMakeFiles/hven_qp_walk_seam_tests.dir/qp/test_qp_walk_seam.cpp.o
```

Two of the three are recompiles of sources this round changed, with
`HVEN_TESTING` defined, and so certainly are **not** byte-identical. This is the
same family as the label defect F2 was raised about, one level out: the control's
universe is the build tree, its claim is about the comparison.

**It changes no outcome.** The gate's assertion is `negative control ≥ 1`, and
the genuinely-compared control is **69** in Release and **68** in Debug — both
far above the floor, and both still doing the work the control exists for. The
mover, noise-only and byte-identical labels remain mutually disjoint over the
compared set. Registered for a later round: the control should be intersected
with the capture manifest, or the three excluded objects named as excluded.

## The process note for the ledger

**Twice in two consecutive rounds a pre-run declaration was derived against the
wrong base or the wrong universe**, and a third time a runner pointed a gate at a
path its own sibling does not create:

* M3a fix1 — the same shape of failure: a P-SYM shape declared over the wrong
  object universe (`DIFFERING 12 = 9 + 3` declared, `10 = 7 + 3` and `11 = 8 + 3`
  measured), recorded FAILED and not re-run. See
  `../../2026-09-m7-w2-m3a-request-probe/gates-fix1/README.md`.
* M3b fix1 G5 — `EXPECT_DEBUG_ONLY_DEBUG=3` reused M3b's measured shape, whose
  base predates the `#include` line in `qp_engine.h`; against this round's base
  that line already exists.
* M3b fix1 fold proofs — `chain_gates_fix1.sh` passed `$S/dbg` where
  `gates-fix1.sh` builds `$S/gates-dbg`.

In all three the substance was sound and the declaration was not. The rule that
follows, and that the next runner must satisfy:

> **A declaration is derived by diffing the task's OWN `base..head`, and the
> derivation is stated in the runner.** Not carried over from the previous
> round's measured shape, not reasoned from "the same files changed the same
> way". A runner that reuses a shape must name the base that shape was measured
> against and show that base is this round's base. Every path a runner hands a
> sibling script is a path that runner is responsible for creating or naming.

The second half is what the two failures have in common: both declarations were
*inherited*, and inheritance is what the rule forbids. `gates-fix1.sh`'s own
comment block is the artefact — it argues the reuse explicitly and carefully, and
is wrong anyway, because the argument was about the files rather than about the
bases.

## Files

```
gates.txt                               the chain log: HEAD stamp, the declared expectations,
                                         segment boundaries, every lock marker, ASSERT and rc
chain.txt                               the outer chain log: CHAIN_START/END, CHAIN_SUMMARY
suite-debug.txt                         the Debug suite (long test excluded), 2697/0
suite-debug-long.txt                    the long test, alone, 1/1
suite-release.txt                       the Release suite, 2698/0
replay/head-{walk,ssn,ipm}.csv          the three U0 replay arms at HEAD 31774717
replay/comparison.txt                   their comparison against the M6-close pin, 27/75/0 ×3
psym-compare-{Release,Debug}.txt        the raw object-by-object P-SYM output
psym-classification-{Release,Debug}.txt the classification, with F2's three-way labelling
psym-sections-{Release,Debug}.txt       F2's FAIL-CLOSED per-section comparison over every
                                         non-debug byte of all 42 library objects -- NEW
                                         this round, and what carries the Debug claim
install-smoke.txt                       the install smoke tail
export-contract.txt                     the export contract output, align 32
fold-proofs.txt                         the runner's fold proofs: 45 folds, 1 SKIPPED-and-
                                         counted-failed (see FAILED #2), wrappers == HEAD
fold-proofs-psym.txt                    the P-SYM assertion layer's fold proofs: 53 folds,
                                         0 failed, wrappers == HEAD
```
