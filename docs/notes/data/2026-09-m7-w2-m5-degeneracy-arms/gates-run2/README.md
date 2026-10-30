# M7 W2 M5 Task A — RUN 2: the six gates on the cond(R) head, declared before they ran

Branch `m7`. **BEFORE** `f255bae9` (this task's own base, UNCHANGED from run 1).
**HEAD** `e5f1c15b`, tree clean, unpushed. Chain
`wrappers/chain_gates_m5.sh`, detached with `setsid` as pid `3080447` at
2026-09-22T13:24:43-04:00, exited at 13:58:31-04:00.

**WHY THERE IS A RUN 2.** The settler ruled option 2 on the C4 halt: the
statistic C4 asks for is not reachable from the boundary, so it was BUILT.
`2ca21aa0` adds `add_b1_cond_r` (per add record) and `indep_max_cond_r` (per
solve), both `HVEN_TESTING`-only, both read off the QR the dependency test
already builds. `include/` moved, so run 1's gate no longer covers the head the
probe will be pinned to, and a new chain over `f255bae9..e5f1c15b` was required.

**RUN 1 STANDS AS THE RECORD FOR `0c6709ce`.** `../gates/` is exactly as
committed at `82d02cc5` and is NOT rewritten, re-read or re-interpreted here.
Its numbers are its own; this directory's numbers are this run's. Neither
declaration is adjusted to the other.

The expectations below were RE-DERIVED for this head and committed BEFORE the
chain was detached, in `psym-derivation-m5-run2.txt` (commit `e5f1c15b`); the
chain's own header carries the same constants
(`=== BEFORE=f255bae9  EXPECT_DEBUG_TESTS=2732  EXPECT_RELEASE_TESTS=2733 ===`,
`gates.txt`). Nothing below was adjusted after seeing what it reported.

**WHAT IS UNDER GATE.** Four commits — `f68dbbe8` (arm A, the scaled drop
tolerance plus the diagnostic half), `3899bad6` (arm b1 as first built),
`02d8cd98` (arm b1 REDONE on amendments A1–A12: the unregularized
rank-revealing QR as the sole oracle, RANK as the invariant, one shared
predicate at every mutation site) and `2ca21aa0` (cond(R)) — plus `0c6709ce`
and `e5f1c15b`, the two declarations. Every one of them is `HVEN_TESTING`-only
and OFF by default, so the production bar is the point of the round, and it is
MEASURED here rather than inferred from the `#ifdef`s.

## The result in one line

**All six gates met their declared expectations, and the fold proofs under them
are 46/0.** The production library is unmoved by cond(R) as it was unmoved by
the arm: zero `DIFFERING` objects in both configurations, all 42 library
objects byte-identical in Release, 41 byte-identical in Debug with
`src/qp/qp_engine.cpp.o` differing in DEBUG SECTIONS ONLY, 0 non-debug movers
in either configuration.

## Declared vs measured

Every MEASURED cell below is quoted from the RAW transcript named beside it,
not from the chain's summary line.

| gate | what it runs | DECLARED (before the run) | MEASURED (from the raw transcript) | |
|---|---|---|---|---|
| G1 | full Debug suite | 2732 tests, 0 failed | `100% tests passed, 0 tests failed out of 2732` (`suite-debug.txt`); 1 skipped + 2 disabled did not run | **MET** |
| G2 | the one >600 s Debug test, alone | 1 test, 0 failed | `1/1 Test #1541: Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 675.55 sec`, `100% tests passed, 0 tests failed out of 1` (`suite-debug-long.txt`) | **MET** |
| G3 | full Release suite | 2733 tests, 0 failed | `100% tests passed, 0 tests failed out of 2733` (`suite-release.txt`) | **MET** |
| G4 | U0 replay vs the M6 close-gate pins | 27 cells × 75 columns × 3 engines, 0 differences | three lines, each `cells compared: 27  columns compared: 75  differences: 0` (`replay/comparison.txt`) | **MET** |
| G5 | P-SYM + the fail-closed section check, both configurations | DIFFERING 0 = class A 0 + class B 0 in both; `psym_compare.sh compare` rc 0; DEBUG_ONLY Release 0, Debug 1 (`src/qp/qp_engine.cpp.o`); negative control ≥ 1 | `DIFFERING OBJECTS: 0`, class A 0, class B 0, UNEXPLAINED 0 in BOTH (`psym-classification-{Release,Debug}.txt`); `SECTIONS_Release_BYTE_IDENTICAL=42 DEBUG_ONLY=0`, `SECTIONS_Debug_BYTE_IDENTICAL=41 DEBUG_ONLY=1` with the one object NAMED, `NON_DEBUG_MOVERS=0` and `REFUSED=0` in both (`psym-sections-*.txt`); negative control 70 / 69 | **MET** |
| G6 | install smoke, export contract, clang-format, `git diff --check`, dirty-file count | all green; align 32 | `== hven install smoke: PASSED`, both solvers converged through one generic consumer, 19 standalone-include TUs (`install-smoke.txt`); `== export contract OK for platform=linux, align=32` (`export-contract.txt`); `CLANG_FORMAT_RC=0`; `DIFF_CHECK_RC=0`; `DIRTY_FILES=0` (`gates.txt`) | **MET** |
| — | `fold_proofs_m3b.sh` (the gate-assert library folded against itself) | 0 failed | `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0` (`fold-proofs.txt`) | **MET** |

The chain's own aggregate lines agree with the table read by hand:
`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates.txt`) and
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain.txt`).

**THE TEST COUNTS ARE THE DECLARATION'S TWO AGREEING DERIVATIONS, MET.**
`psym-derivation-m5-run2.txt` §4 derived 2732 Debug / 2733 Release twice — from
`ctest -N` 2735 less M2b's derived offset, and from run 1's MEASURED 2731/2732
plus the ONE new `TEST()` block (`grep -c '^TEST(' ...` = 18, run 1's head had
17). Both transcripts report exactly those numbers.

## The new fixtures, by name, in both configurations

The eighteen `QpDegeneracyArm.*` fixtures (seventeen from run 1 plus
`CondROnTheQrIsFilledWhereADecompositionRanAndSizesTheConditioning`), the
sixteen `QpAntiCyclingArm.*` fixtures M4 and its fix round left standing, and
the twenty pre-existing `QpWalkSeam.*` fixtures — **54 in the standalone
`hven_qp_walk_seam_tests` target** — are `Passed` **by name** in BOTH
configurations, and the two name lists are IDENTICAL (read by name out of
`suite-debug.txt` and `suite-release.txt` and compared, not counted):

| target's fixture family | Debug | Release |
|---|---|---|
| `QpDegeneracyArm.*` | 18 | 18 |
| `QpAntiCyclingArm.*` | 16 | 16 |
| `QpWalkSeam.*` | 20 | 20 |
| total | **54** | **54** |

The 18 is the same 18 the declaration derived from
`grep -c '^TEST(' tests/qp/test_qp_degeneracy_arm.cpp`, so the suite-count
derivation and the by-name reading agree, and 54 is the 54/54 the declaration's
§9 recorded by hand before the chain ran.

No build warning in either configuration names any file this round touched
(`qp_degeneracy_arm.h`, `qp_walk_trace.h`, `qp_engine.cpp`,
`test_qp_degeneracy_arm.cpp`: zero warning lines). The 16 warnings per
configuration are the tree's pre-existing ones — 7
`-Wpotentially-evaluated-expression`, 5 `-Wdeprecated-declarations`, 3
`-Wswitch`, 1 `-Wcharacter-conversion` — the same census M4 and run 1 recorded.

## G5 — what it measured, and the one looseness it carries

| | Release | Debug |
|---|---|---|
| objects in the capture universe | 146 | 146 |
| before/after manifests name the same objects | yes | yes |
| `DIFFERING OBJECTS` in the classification | 0 | 0 |
| `psym_compare.sh compare` rc | 0 | 0 |
| classify rc | 0 | 0 |
| class A / class B / UNEXPLAINED | 0 / 0 / 0 | 0 / 0 / 0 |
| library objects compared by the section check | 42 | 42 |
| byte-identical | 42 | 41 |
| DEBUG-ONLY | 0 | 1 — `src/qp/qp_engine.cpp.o` |
| NON_DEBUG_MOVERS | 0 | 0 |
| REFUSED by the section check | 0 | 0 |
| both capture return codes | 0, 0 | 0, 0 |

**THE CLASSIFIER DERIVED ITS HEADER LIST; IT DID NOT CARRY ONE.** The chain
runs M4's REPAIRED `classify_psym_m4.py`, which takes `--base`/`--head` and
runs the derivation itself; its transcript prints it
(`psym-classification-{Release,Debug}.txt`):

```
  cmd  : git -C /home/ghecht/Projects/hven diff --name-only f255bae9..e5f1c15ba… -- include/
  list : ['include/hven/detail/qp/qp_degeneracy_arm.h', 'include/hven/detail/qp/qp_walk_trace.h']
HEADER LIST ACCEPTED: [...]
```

The same two headers run 1 derived — cond(R) touched both of them and added
none — derived here from RUN 2's own range, not carried from run 1. The
runner's independently-derived list agreed with it, so the cross-check passed
rather than refusing.

**THE LOOSENESS, MEASURED RATHER THAN REPAIRED.** The classifier's negative
control (70 Release, 69 Debug) is taken over every `.o` under the build tree,
which includes test and bench objects the capture never covered. That is M3b's
pre-existing looseness, carried forward unchanged. The control **restricted to
the captured library universe** — the number that actually stands behind the
"0 differing" claim — is **8 in Release and 7 in Debug**, computed here by
intersecting the classification's negative-control list with
`psym-libobjs-{Release,Debug}.txt`:

```
assembly_eval_seam.cpp.o  console_trace_sink.cpp.o  sqp_print.cpp.o
sqp_solver.cpp.o  soc_elastic_restoration.cpp.o  ipqp_engine.cpp.o
continuation.cpp.o                     [+ qp_engine.cpp.o in Release only]
```

Debug's list is Release's less `qp_engine.cpp.o`, which is the declared
debug-only exception. These are the same 8 / 7 M4 and run 1 measured.

**THE NOISE-ONLY OBJECTS, AND THE ONE LIBRARY OBJECT AMONG THEM.** Release:
three — `bench_corpus.cpp.o`, `bench_crossover.cpp.o`, `ipqp_e1_arm.cpp.o`,
all bench targets built with `HVEN_TESTING`, none in the production library.
Debug: those three plus `src/qp/qp_engine.cpp.o`, whose difference the section
check then resolves to DEBUG SECTIONS ONLY (`CHANGED 0; UNCLASSIFIED 0` in the
comparison, `DEBUG-ONLY` in `psym-sections-Debug.txt`). `libhven.a` itself is
reported as differing in Debug and the tool says why in its own words: archive
metadata — member timestamps and the symbol index — is not instructions. This
is the identical shape run 1 recorded.

## What this gate does and does not establish

* It establishes that **the whole M5 degeneracy experiment — arm A, arm b1, the
  diagnostic half AND the cond(R) read-out — leaves the production library
  byte-identical** to a tree with no degeneracy arm in it, on the objects
  themselves, in both configurations. cond(R) added 225 lines and moved nothing
  a production build can see.
* It establishes that the **18 fixtures pass in both configurations** and that
  the pre-existing suite is unchanged (2732 / 2733, 0 failed).
* **It says nothing about either arm's numerical result, and nothing about what
  cond(R) will show.** No leg had run when this chain exited; the arms'
  behaviour is Task B's subject, governed by `RULE.md` and its AMENDMENT 1.
* **No timing claim is made.** The chain ran under the box lock, one leg at a
  time, with `PGREP_EMPTY` recorded before each; every duration in these
  transcripts is informational (CLAUDE.md §7).

## Files

| file | what |
|---|---|
| `psym-derivation-m5-run2.txt` | the RE-DERIVED expectations, committed BEFORE the run (`e5f1c15b`) |
| `chain.txt` | the chain's own log: path checks, gate rc, fold-proof aggregate |
| `gates.txt` | the six gates' assert transcript |
| `suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt` | the raw ctest transcripts |
| `replay/` | the three head CSVs and the U0 comparison |
| `psym-compare-{Release,Debug}.txt` | the raw P-SYM comparisons |
| `psym-classification-{Release,Debug}.txt` | the classifier's derivation and classes |
| `psym-sections-{Release,Debug}.txt` | the fail-closed non-debug section check |
| `psym-libobjs-{Release,Debug}.txt` | the 42 library objects each configuration compared |
| `install-smoke.txt`, `export-contract.txt` | G6's two long transcripts |
| `fold-proofs.txt` | the gate-assert library folded against itself, 46/0 |

`../gates/` holds RUN 1, for `0c6709ce`. It is the record for that head and is
not amended by anything here.
