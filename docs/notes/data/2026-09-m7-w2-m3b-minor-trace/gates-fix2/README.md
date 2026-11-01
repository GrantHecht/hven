# M7 W2 M3b — FIX ROUND 2 — the gates evidence

Fix round 2 (`.superpowers/m7-w2-m3b-fix2-brief.md`, settler adjudications
S1–S3) answered the settler's scoped re-review of fix round 1. It made the
walk observer's inertia evidence a SINGLE READ at both verdict sites
(eliminated ~:1593/1602, border ~:1810/1813) instead of a second, independently
fallible `inertia()` call (S1); forced real thread overlap in the two
concurrency fixtures with a start barrier sized so both threads are inside
`QpEngine::run` together (S2); corrected the README's YORKNET causal account
and withdrew RULE-G's unsound 1e-6 active-row bar derivation, without editing
RULE-G.md itself (S3); and carried four accepted minors (narrowed allocator
comment, corrected "mutually exclusive by construction" note, two new ELF
digest refusals — unsupported `EI_DATA`, out-of-range/unterminated
section-name offset — with two new folds, and the padding-mask comment fix).

This directory holds the raw transcripts of the six-gate chain run at that
change's head, plus both sets of fold proofs. `gates/` (M3b task A) and
`gates-fix1/` (fix round 1) are frozen evidence from earlier rounds and are
**not** edited or superseded by this round. Format precedent: `gates-fix1/`.
As there, build and configure logs are NOT committed (`install-smoke.txt` is
the one log that is committed whole, following the same precedent set in
`gates-fix1/`); the chain log, the suite tails, the replay arms, the P-SYM raw
output and classification, the section-comparison transcripts, the
install-smoke and export-contract tails, and the fold proofs are.

## What landed

| commit | what |
|---|---|
| `d178b0ea` | S1 + S2 + two minors: single-read inertia at both verdict sites; the concurrency fixtures' start gate and wider problems; the new backend-idempotence fixture (`TheBackendsInertiaAccessorAnswersTheSameThingTwiceOnThisPlatform`); the narrowed allocator comment; the repaired padding-mask comment |
| `33c4df66` | `docs/testing.md`: the single-read section, the forced-overlap section, the TSan **registration**, the narrowed allocator claim |
| `ca1bdbdc` | `README.md`: "Corrections (fix2)" (C9–C16) and the health warning at the top of the re-trace section |
| `29073877` | the wrappers: `gates-fix2.sh`, `chain_gates_fix2.sh` (path defect repaired), `check_sections_fix2.sh`, `section_digest_fix2.py` (two new refusals), `fold_proofs_psym_fix2.sh` (PART 6), and `gates-fix2/psym-derivation-fix2.txt` — the pre-run declaration, committed before the chain was detached |

This round moves NO pin: no counter field, no struct size, no golden line. The
only test-count movement is additive — one new `TEST()` block in the existing
`hven_qp_walk_seam_tests` executable, 19 → 20.

## The chain

Detached chain (`setsid`), PID **2652959** (session leader, `ppid 1`), HEAD
**`29073877`**, `dirty-files=0`, `BEFORE=f61199bd` (fix round 1's head),
2026-09-21T16:56:31 → 2026-09-21T17:29:26 America/New_York. Six lock segments
plus a seventh for the fold proofs, each with its own `PGREP_EMPTY` line and
`FOREGROUND_START`/`FOREGROUND_END` markers inside the lock.

Expectations were declared in `psym-derivation-fix2.txt`, committed at
`29073877` **before** the chain was detached, and transcribed from there into
`gates-fix2.sh`: `EXPECT_DEBUG_TESTS=2698`, `EXPECT_RELEASE_TESTS=2699`, replay
`27/75/0` over 3 engines, the P-SYM mover shape `EXPECT_PSYM_DIFFERING=0 /
CLASS_A=0 / CLASS_B=0`, and the per-configuration debug-only shape
`EXPECT_DEBUG_ONLY_RELEASE=0`, `EXPECT_DEBUG_ONLY_DEBUG=1` (the one object
named `qp_engine.cpp.o`). Unlike fix round 1, this declaration was derived
against **this round's own** `f61199bd..HEAD` diff, not carried over from an
earlier round's measured shape — the process rule fix round 1's two FAILED
rows established.

**An exit code is not a result.** Every number in the table below was
re-checked against the raw transcript named in the last column — not against
the runner's own `ASSERT OK` lines, and not against `GATES_SUMMARY`,
`GATES_AGGREGATE` or `CHAIN_SUMMARY`. **Every row below matches its
declaration.** No row failed, and nothing was re-run.

| gate | declared | measured | verdict | checked against |
|---|---|---|---|---|
| G1 Debug suite | 0 failed, 2698 total | `100% tests passed, 0 tests failed out of 2698` | OK | `suite-debug.txt`, ctest summary line read directly |
| G2 the long test alone | 1/1 passed | `Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 674.95 sec`; `100% tests passed, 0 tests failed out of 1` | OK | `suite-debug-long.txt` |
| G3 Release suite | 0 failed, 2699 total | `100% tests passed, 0 tests failed out of 2699` | OK | `suite-release.txt` |
| G1/G3 all 20 `QpWalkSeam.*` fixtures, both configs | every one `Passed` in Debug AND Release | 20 distinct `QpWalkSeam.*` names each with exactly one `Passed` line in each transcript; the two name sets `diff` empty (`TheBackendsInertiaAccessorAnswersTheSameThingTwiceOnThisPlatform` among them); 0 `Failed` | OK | `suite-debug.txt`, `suite-release.txt`, grepped by name and diffed |
| G4 U0 replay, 3 arms | 27 cells / 75 columns / 0 differences, ×3 (walk, ssn, ipm) | all three arms `cells compared: 27  columns compared: 75  differences: 0` | OK | `replay/comparison.txt` |
| G5 P-SYM, Release | compare rc 0; `DIFFERING OBJECTS: 0` = class A 0 + class B 0; `UNEXPLAINED: 0`; negative control ≥ 1; both capture rcs 0 | matches on every count; `PSYM_COMPARE_RC_Release=0`, `CLASSIFY_RC_Release=0`, control 72 | OK | `psym-compare-Release.txt`, `psym-classification-Release.txt`, `gates.txt` |
| G5 P-SYM, Debug | the same shape | matches on every count; `PSYM_COMPARE_RC_Debug=0`, `CLASSIFY_RC_Debug=0`, control 71 | OK | `psym-compare-Debug.txt`, `psym-classification-Debug.txt`, `gates.txt` |
| G5 the three-way labels are disjoint | mover / noise-only / byte-identical, no object in two | Release 0 / 0 / 72; Debug 0 / 1 / 71. The one Debug NOISE-ONLY object, `hven.dir/src/qp/qp_engine.cpp.o`, is printed under its own heading and is absent from the negative control; in Release it is byte-identical and is in the control | OK | both classifications, read side by side |
| G5 capture pair | both arms enumerate the same object count | `146 object(s)` in each of the four manifests; `capture pair object count = 146` in both configs | OK | `gates.txt` |
| G5 cross-configuration | class-B sets equal; UNEXPLAINED 0 in both; class-A sets identical | all six cross-config asserts OK, every set empty | OK | `gates.txt` |
| G5 sections, Release | 42 objects; every one BYTE-IDENTICAL or DEBUG-ONLY; `NON_DEBUG_MOVERS=0`; `REFUSED=0`; `DEBUG_ONLY=0` as declared | `OBJECTS=42 BYTE_IDENTICAL=42 DEBUG_ONLY=0 NON_DEBUG_MOVERS=0 REFUSED=0 RC=0` | OK | `psym-sections-Release.txt` |
| G5 sections, Debug | 42 objects; 41 BYTE-IDENTICAL, exactly 1 DEBUG-ONLY named `qp_engine.cpp.o`; `NON_DEBUG_MOVERS=0`; `REFUSED=0` | `OBJECTS=42 BYTE_IDENTICAL=41 DEBUG_ONLY=1 NON_DEBUG_MOVERS=0 REFUSED=0 RC=0`; the one DEBUG-ONLY object named `CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o` | OK | `psym-sections-Debug.txt` |
| G6 install smoke | PASSED, both solvers converged through one generic consumer, 19 standalone-include TUs | `== hven install smoke: PASSED`; `INSTALL_SMOKE_RC=0`; `IpmSolver converged, objective = 17.0140174 (OK)`; `SqpSolver converged, objective = 17.0140173 (OK)` | OK | `install-smoke.txt` |
| G6 export contract | align = 32 | `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32`; `== export contract OK for platform=linux, align=32` | OK | `export-contract.txt` |
| G6 clang-format | rc 0, over the changed C++ files derived from git | `CLANG_FORMAT_RC=0`; derived list `qp_walk_trace.h qp_engine.cpp test_qp_walk_seam.cpp` | OK | `gates.txt` |
| G6 `git diff --check` | rc 0, `DIRTY_FILES=0` | `DIFF_CHECK_RC=0`, `DIRTY_FILES=0` | OK | `gates.txt` |
| stamp | HEAD `29073877`, `dirty-files=0`, no `-dirty` anywhere | `=== HEAD 29073877  dirty-files=0 ===`; a recursive grep for `-dirty` over every raw transcript in `.scratch/m7w2m3bfix2/gates/` and `.scratch/m7w2m3bfix2/logs/` finds nothing | OK | `gates.txt` header line; direct grep over the raw scratch transcripts |
| runner identity | the transcripts were produced by what landed | both fold-proof transcripts stamp `HEAD 29073877...  dirty-files=0` and print every wrapper path `(worktree == HEAD)` by sha256 (`gates-m3b.sh`, `gate_asserts_m3b.sh`, `classify_psym_m3b.py`, `fold_proofs_m3b.sh` in `fold-proofs.txt`; `gate_asserts_m3b.sh`, `psym_assert_only.sh`, `fold_proofs_psym_fix2.sh`, `classify_psym_fix1.py`, `check_sections_fix2.sh`, `section_digest_fix2.py`, `gates-fix2.sh`, `chain_gates_fix2.sh` in `fold-proofs-psym.txt`) | OK | both fold-proof transcripts |
| fold proofs, runner (`fold_proofs_m3b.sh`) | 46 folds, 0 failed | `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0` | OK | `fold-proofs.txt` |
| fold proofs, P-SYM layer (`fold_proofs_psym_fix2.sh`) | 60 folds, 0 failed | `PSYM_FOLD_PROOFS_FIX2_TOTAL=60  PSYM_FOLD_PROOFS_FIX2_FAILED=0` (includes PART 6's six new folds over the ELF digest's two new refusals) | OK | `fold-proofs-psym.txt` |
| aggregate | all green | `GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`; `GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]`; `CHAIN_SUMMARY gates=0 fold_proofs=0` | OK | `gates.txt` tail, `chain.txt` |

Two tests did not run in either configuration, as on every gate of this
branch: `FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`
(Skipped) and the two `EqpRefinementAb` cases (Disabled). Wall-clock in these
transcripts is informational only; the counters and statuses are the
currency.

## No re-trace

This round is gates-only. `RULE.md`, `RULE-G.md`, and every committed
`rows/`, `traces/`, `verdict.txt` and `fold-walk.txt` under `gates/` and
`gates-fix1/` stand unedited and were not re-derived. S3's corrections are
textual, in `README.md`'s "Corrections (fix2)" section and its re-trace
section's own health warning; no leg reran and no number in this directory
is a re-trace number. S1 states in the README (per the settler's adjudication)
that the committed traces were taken on MKL, whose accessor rereads cached
session fields, so their inertia columns already equal the walk's own
evidence — the single-read change alters no committed trace value.

## S1: no query-count seam exists

S1's fixture, `QpWalkSeam.TheBackendsInertiaAccessorAnswersTheSameThingTwiceOnThisPlatform`,
asserts backend-accessor idempotence rather than a query count: no existing
observability on this seam counts backend `inertia()` calls, so "the count of
backend inertia queries is identical attached vs unattached" is asserted BY
CONSTRUCTION — the walk's own read site is stored into observer scratch
without a second call, so there is structurally one call where there used to
be a candidate for two, not a count that could be measured and might disagree.

## Files

```
gates.txt                               the chain log: HEAD stamp, the declared expectations,
                                         segment boundaries, every lock marker, ASSERT and rc
chain.txt                               the outer chain log: CHAIN_START/END, CHAIN_SUMMARY
suite-debug.txt                         the Debug suite (long test excluded), 2698/0
suite-debug-long.txt                    the long test, alone, 1/1
suite-release.txt                       the Release suite, 2699/0
replay/head-{walk,ssn,ipm}.csv          the three U0 replay arms at HEAD 29073877
replay/comparison.txt                   their comparison against the M6-close pin, 27/75/0 ×3
psym-compare-{Release,Debug}.txt        the raw object-by-object P-SYM output
psym-classification-{Release,Debug}.txt the classification, with the three-way labelling
psym-sections-{Release,Debug}.txt       the fail-closed per-section comparison over every
                                         non-debug byte of all 42 library objects
install-smoke.txt                       the install smoke tail, in full (ninja build lines
                                         included, following gates-fix1/'s precedent)
export-contract.txt                     the export contract output, align 32
fold-proofs.txt                         the runner's fold proofs: 46 folds, 0 failed,
                                         wrappers == HEAD
fold-proofs-psym.txt                    the P-SYM assertion layer's fold proofs: 60 folds,
                                         0 failed (PART 6's six new folds over the ELF
                                         digest's two new refusals included), wrappers == HEAD
psym-derivation-fix2.txt                the pre-run declaration this round's expectations
                                         were transcribed from, derived against THIS round's
                                         own f61199bd..HEAD (committed before the chain ran)
```
