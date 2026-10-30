# M7 W2 M3b — TASK A — the gates evidence

Task A of M3b (`.superpowers/m7-w2-m3b-brief.md`) added a READ-ONLY per-minor
observer on the QP active-set walk — `include/hven/detail/qp/qp_walk_trace.h`,
reached through seven `#ifdef HVEN_TESTING` hooks in `src/qp/qp_engine.cpp` — and
changed no behaviour, no default and no counter. This directory holds the raw
transcripts of the six-gate chain run at that change's head, plus the fold proofs
of the runner and of the P-SYM assertion layer that produced them.

Format precedent: `../../2026-09-m7-w2-m3a-request-probe/gates/`. As there, build
and configure logs are NOT committed; the chain log, the suite tails, the replay
arms, the P-SYM raw output and classification, the install-smoke and
export-contract tails, and the fold proofs are.

## What landed

| commit | what |
|---|---|
| `01a956a7` | the observer header, the seven `HVEN_TESTING` hooks, the `hven_qp_walk_seam_tests` target, `tests/qp/test_qp_walk_seam.cpp`, the `docs/testing.md` section |
| `8f54d714` | the gate machinery: `classify_psym_m3b.py` (the registered harvest defect FIXED; R2 becomes `--expect-differing`), `derive_psym_m3b.sh` |
| `efabc2bb` | `psym-derivation.txt` (the PRE-RUN derivation), the runner declaring ZERO movers, `analyse_objects_m3b.sh`, the 11th fixture, the narrowed byte-identity claim |
| `2c1a318f` | two fold-proof corrections |
| `9808a726` | the whole-runner folds stop calling a function version 2 retired |
| `e891041e` | the detached chain script — the chain's HEAD |

This task moves NO pin: no counter field, no struct size, no golden line. The
only test-count movement is additive — eleven new `TEST()` blocks in a NEW
executable.

## The chain

Detached chain, PID **2493543**, HEAD **`e891041e`**, `dirty-files=0`,
`BEFORE=9fee5545`, 2026-09-21T12:10:47 → 2026-09-21T12:50:40 America/New_York.
Six lock segments plus a seventh for the fold proofs, each with its own
`PGREP_EMPTY` line and `FOREGROUND_START`/`FOREGROUND_END` markers inside the
lock.

Expectations were declared IN the runner at `e891041e` and derived IN
`psym-derivation.txt` at `efabc2bb`, i.e. before the run, not discovered from it:
`EXPECT_DEBUG_TESTS=2689`, `EXPECT_RELEASE_TESTS=2690`, `EXPECT_LONG_TESTS=1`,
replay `27/75/0` over `3` engines, and the P-SYM mover shape
`EXPECT_PSYM_DIFFERING=0`, `EXPECT_PSYM_CLASS_A=0`, `EXPECT_PSYM_CLASS_B=0` with
`NEGATIVE CONTROL` 70 in both configurations.

**An exit code is not a result.** Every number in the table below was re-checked
by this task's successor implementer against the raw transcript named in the last
column — not against the runner's own `ASSERT OK` lines, and not against
`GATES_SUMMARY`, `GATES_AGGREGATE` or `CHAIN_SUMMARY`.

| gate | expectation | measured | checked against |
|---|---|---|---|
| G1 Debug suite | 0 failed, 2689 total | `100% tests passed, 0 tests failed out of 2689`; 0 `***Failed`/`***Exception` lines | `suite-debug.txt`, ctest summary line read directly |
| G2 the long test alone | 1/1 passed | `Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 670.02 sec`; `100% tests passed, 0 tests failed out of 1` | `suite-debug-long.txt` |
| G3 Release suite | 0 failed, 2690 total | `100% tests passed, 0 tests failed out of 2690`; 0 `***Failed`/`***Exception` lines | `suite-release.txt` |
| G1/G3 the eleven NEW fixtures, both configs | all eleven `Passed` in Debug AND Release | eleven distinct `QpWalkSeam.*` names, each with exactly one `Passed` line, in each transcript; zero `Failed` | `suite-debug.txt`, `suite-release.txt`, grepped by name and tallied |
| G4 U0 replay, 3 arms | 27 cells / 75 columns / 0 differences, ×3 (walk, ssn, ipm) | all three arms `cells compared: 27  columns compared: 75  differences: 0` | `replay/comparison.txt` |
| G5 P-SYM, Release | compare rc 0 (a PASS, because ZERO movers is what this round declares); `DIFFERING OBJECTS: 0` = class A 0 + class B 0; `UNEXPLAINED: 0`; negative control 70 (≥ 1); both capture rcs 0 | matches on every count; `PSYM_COMPARE_RC_Release=0`, `CLASSIFY_RC_Release=0` | `psym-compare-Release.txt`, `psym-classification-Release.txt`, `gates.txt` lines `capture after-Release: rc=0`, `capture before-Release: rc=0` |
| G5 P-SYM, Debug | the same shape | matches on every count; `PSYM_COMPARE_RC_Debug=0`, `CLASSIFY_RC_Debug=0` | `psym-compare-Debug.txt`, `psym-classification-Debug.txt`, `gates.txt` lines `capture after-Debug: rc=0`, `capture before-Debug: rc=0` |
| G5 no mover escaped the harvest | 0 `DIFFERS` lines and 0 `RELOCATIONS` lines in either comparison | `grep -c` over both raw comparisons: `DIFFERS` 0/0, `RELOCATIONS` 0/0 | `psym-compare-{Release,Debug}.txt`, grepped directly |
| G5 no relocation-differing object labelled byte-identical | vacuous here and CHECKED to be vacuous, not assumed | there is no relocation-differing object in either configuration (previous row), so none can appear in the control; the harvest that would catch one is fold-proven at FOLD 31–33 | `psym-compare-*.txt` + `fold-proofs-psym.txt` |
| G5 classification, Release vs Debug | byte-identical | `diff psym-classification-{Debug,Release}.txt` empty | direct `diff` of the two committed files |
| G5 capture pair | both arms enumerate the same object count | `capture pair object count = 146` in both configs; `146 object(s)` in each of the four manifests | `gates.txt` |
| G5 cross-configuration (the `gate_asserts_v2` layer) | class-B sets equal; UNEXPLAINED 0 in both; class-A sets identical | all six cross-config asserts OK, every set empty | `gates.txt`, `G5_ASSERT_RC=0` |
| G6 install smoke | PASSED, both solvers converged through one generic consumer, 19 standalone-include TUs | `IpmSolver converged, objective = 17.0140174`; `SqpSolver converged, objective = 17.0140173`; `== hven install smoke: PASSED` | `install-smoke.txt` |
| G6 export contract | align = 32 | `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32`; `== export contract OK for platform=linux, align=32` | `export-contract.txt` |
| G6 clang-format | rc 0, over the four changed C++ files derived from git | `CLANG_FORMAT_RC=0`; the derived list is `qp_engine.h qp_walk_trace.h qp_engine.cpp test_qp_walk_seam.cpp` | `gates.txt` |
| G6 `git diff --check` | rc 0, `DIRTY_FILES=0` | `DIFF_CHECK_RC=0`, `DIRTY_FILES=0` | `gates.txt` |
| stamp | HEAD `e891041e`, `dirty-files=0`, no `-dirty` anywhere | `=== HEAD e891041e  dirty-files=0 ===`; a recursive grep for `-dirty` over every transcript finds nothing | `gates.txt` header line; direct grep |
| runner identity | the transcripts were produced by what landed | `fold-proofs.txt` and `fold-proofs-psym.txt`: `HEAD e891041e... dirty-files=0`, and all seven wrapper paths `(worktree == HEAD)` by sha256 | both fold-proof transcripts |
| fold proofs, runner | 46 folds, 0 failed | `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0` | `fold-proofs.txt` |
| fold proofs, P-SYM layer | 37 folds, 0 failed | `PSYM_FOLD_PROOFS_M3B_TOTAL=37  PSYM_FOLD_PROOFS_M3B_FAILED=0` | `fold-proofs-psym.txt` |
| aggregate | `GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`; `GATES_AGGREGATE rc=0`; `CHAIN_SUMMARY gates=0 fold_proofs=0` | exact match | `gates.txt` tail, `chain.txt` |

## What the zero means, and the one caveat in how it is printed

A declaration of **zero movers** is a stronger claim than a non-zero one, and
what could make a false zero pass is a capture that compared nothing. Four
independent things stand against that here and all four are in the transcripts:
both capture return codes 0 with non-empty manifests; equal object counts on the
two arms (146 = 146); a **negative control of 70** — seventy objects that DO
include one of the two changed headers and are identical anyway, so "0 differing"
is not "the capture saw nothing"; and the fold proofs, which show the classifier
REFUSING an empty comparison (R1), a shape other than the declared one (R2), a
control below the floor (R3) and an unexplained object (R4).

**The caveat, stated because the label is imprecise and the measurement is not.**
`classify_psym_m3b.py` harvests the differing set from `^DIFFERS` and
`^RELOCATIONS` — this round's fix to the defect M3a fix1 registered, fold-proven
at FOLD 31–33. It still does not harvest `^NOISE-ONLY`, so objects whose P-SYM
verdict is `NOISE-ONLY` land in the negative control, which prints its members
under the heading "…and are byte-identical". In THIS capture that heading is
inaccurate for 3 objects in Release and 29 in Debug. Those objects are named and
explained one by one in `psym-derivation.txt`, written BEFORE the run:

* the three in Release are `bench_corpus.cpp.o`, `bench_crossover.cpp.o` and
  `ipqp_e1_arm.cpp.o`, the three targets `bench/CMakeLists.txt` gives a
  `*_GIT_DESCRIBE` definition; the `before` arm is a detached checkout, so
  `git describe` yields a different string constant. Every instruction is
  identical (`CHANGED 0; UNCLASSIFIED 0` with an empty delta list) and none of
  them is in the production library.
* Debug's 29 are those three plus the TUs that include `qp_engine.h`, which
  gained one line: a DWARF line programme that shifts is invisible to the
  `objdump -d` P-SYM compares.

The gate's own claim is therefore the NARROWED one `psym-derivation.txt` states
and this README repeats rather than quietly widening: see the next section.

## The production library, three properties, stated separately

42 objects under `CMakeFiles/hven.dir`, compared object by object by
`wrappers/analyse_objects_m3b.sh` before the chain (its numbers are transcribed
in `psym-derivation.txt`; the chain re-measured the classification, not this
breakdown).

* **Release** — byte-identical 42 / 42. Text, symbols and relocations therefore
  identical outright.
* **Debug** — byte-identical 39 / 42. The three that differ are the three
  `qp_engine.h` includers (`sqp_solver.cpp.o`, `ipqp_engine.cpp.o`,
  `qp_engine.cpp.o`). Measured one property at a time they are **text-identical**
  (`objdump -d`), **symbol-identical** (`nm -C`) and **relocation-identical
  outside `.rela.debug_line`**; inside that section only `r_offset` moves, with
  symbol, type and addend unchanged on all 3033 differing records. Their bytes
  differ in debug information only.
* **No seam symbol reaches the library**: `nm -C libhven.a` finds 0 `QpWalk` and
  0 `detail::testing` in both configurations.

The placement of the seam `#include` in `qp_engine.h` rather than at the top of
`qp_engine.cpp` is what buys the TEXT property in Debug: that TU carries an
`assert()` above the walk loop, `assert` materialises `__LINE__` as an immediate,
and an include at the top of the TU would have moved it.

## Two process disclosures

* The Debug P-SYM **derivation** leg (before this chain, at `efabc2bb`) exceeded
  the 600 s foreground budget and the harness moved it to the background; it kept
  the same lock hold and ran to completion, rc 0, alone, with the tree restored.
  It should have been detached from the start. No result is affected.
* `derive_psym_m3b.sh`'s own three-property breakdown compared `objdump -d`
  output still carrying its filename line and `readelf -r` output still carrying
  section file offsets, and so reported TEXT and RELOCATION differences that are
  not there. `analyse_objects_m3b.sh` re-does that breakdown correctly from the
  objects the derivation had already saved, and the section above is its result.
  The capture, compare, classification and byte counts are unaffected and were
  not re-run.

Two tests did not run in either configuration, as on every gate of this branch:
`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`
(Skipped) and the two `EqpRefinementAb` cases (Disabled). Wall-clock in these
transcripts is informational only; the counters and statuses are the currency.

## Files

```
psym-derivation.txt                     the PRE-RUN derivation (committed at efabc2bb,
                                         before the chain): the universe, the changed
                                         surface, the declared shape, the three
                                         production-library properties, the test counts
gates.txt                               the chain log: HEAD stamp, the declared expectations,
                                         segment boundaries, every lock marker, ASSERT and rc
chain.txt                               the outer chain log: CHAIN_START/END, CHAIN_SUMMARY
suite-debug.txt                         the Debug suite (long test excluded)
suite-debug-long.txt                    the long test, alone
suite-release.txt                       the Release suite
replay/head-{walk,ssn,ipm}.csv          the three U0 replay arms at HEAD e891041e
replay/comparison.txt                   their comparison against the M6-close pin
psym-compare-{Release,Debug}.txt        the raw object-by-object P-SYM output
psym-classification-{Release,Debug}.txt the classification, which IS the gate
install-smoke.txt                       the install smoke tail
export-contract.txt                     the export contract output
fold-proofs.txt                         the runner's fold proofs on the clean tree at
                                         e891041e: 46 folds, 0 failed, wrappers == HEAD
fold-proofs-psym.txt                    the P-SYM assertion layer's fold proofs: 37 folds,
                                         0 failed, incl. FOLD 31-33, the registered
                                         harvest defect and its fix
```
