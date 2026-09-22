# M7 W2 M5 Task A — the six gates, declared before they ran and read afterwards

Branch `m7`. **BEFORE** `f255bae9` (this task's own base). **HEAD** `0c6709ce`,
tree clean, unpushed. Chain `wrappers/chain_gates_m5.sh`, detached with `setsid`
as pid `3037580` at 2026-09-22T12:34:20-04:00, exited at 13:07:08-04:00.

The expectations were derived and committed BEFORE the chain was detached, in
`psym-derivation-m5.txt` (commit `0c6709ce`); the chain's own header carries the
same numbers. Nothing below was adjusted after seeing what it reported.

**WHAT IS UNDER GATE.** Three commits — `f68dbbe8` (arm A, the scaled drop
tolerance, plus the diagnostic half), `3899bad6` (arm b1 as first built, whose
regularized oracle amendment A1 superseded) and `02d8cd98` (arm b1 REDONE on
amendments A1–A12: the unregularized rank-revealing QR as the sole oracle, RANK
as the invariant, one shared predicate at every mutation site) — plus
`0c6709ce`, the declaration itself. All three are `HVEN_TESTING`-only and OFF by
default, so the production bar is the point of the round, and it is MEASURED
here rather than inferred from the `#ifdef`s.

**A PREVIOUS DECLARATION IS NOT QUOTED ANYWHERE.** The first chain (pid
`3024935`, head `ced56152`) was stopped six minutes in, inside G1, when the
settler's amendments superseded the design it gated. It never reached G5, it
produced no measurement, and `psym-derivation-m5.txt` §0 records that its
numbers are not a pin. Nothing in this directory came from it.

## The result in one line

**All six gates met their declared expectations, and the fold proofs under them
are 46/0.** The production library is unmoved: zero `DIFFERS` lines in both
configurations, all 42 library objects byte-identical in Release, 41
byte-identical in Debug with `src/qp/qp_engine.cpp.o` differing in debug
sections only, 0 non-debug movers in either configuration.

## Declared vs measured

Every MEASURED cell below is quoted from the RAW transcript named beside it, not
from the chain's summary line.

| gate | what it runs | DECLARED (before the run) | MEASURED (from the raw transcript) | |
|---|---|---|---|---|
| G1 | full Debug suite | 2731 tests, 0 failed | `100% tests passed, 0 tests failed out of 2731` (`suite-debug.txt`); 1 skipped + 2 disabled did not run | **MET** |
| G2 | the one >600 s Debug test, alone | 1 test, 0 failed | `1/1 Test #1540: Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 676.89 sec` (`suite-debug-long.txt`) | **MET** |
| G3 | full Release suite | 2732 tests, 0 failed | `100% tests passed, 0 tests failed out of 2732` (`suite-release.txt`) | **MET** |
| G4 | U0 replay vs the M6 close-gate pins | 27 cells × 75 columns × 3 engines, 0 differences | three lines, each `cells compared: 27  columns compared: 75  differences: 0` (`replay/comparison.txt`) | **MET** |
| G5 | P-SYM + the fail-closed section check, both configurations | DIFFERING 0 = class A 0 + class B 0 in both; `psym_compare.sh compare` rc 0; DEBUG_ONLY Release 0, Debug 1 (`src/qp/qp_engine.cpp.o`); negative control ≥ 1 | `DIFFERING OBJECTS: 0`, class A 0, class B 0, UNEXPLAINED 0 in BOTH (`psym-classification-{Release,Debug}.txt`); `SECTIONS_Release_BYTE_IDENTICAL=42 DEBUG_ONLY=0`, `SECTIONS_Debug_BYTE_IDENTICAL=41 DEBUG_ONLY=1` with the one object named, `NON_DEBUG_MOVERS=0` and `REFUSED=0` in both (`psym-sections-*.txt`); negative control 70 / 69 | **MET** |
| G6 | install smoke, export contract, clang-format, `git diff --check`, dirty-file count | all green; align 32 | `== hven install smoke: PASSED`, both solvers converged through one generic consumer, 19 standalone-include TUs (`install-smoke.txt`); `== export contract OK for platform=linux, align=32` (`export-contract.txt`); `CLANG_FORMAT_RC=0`; `DIFF_CHECK_RC=0`; `DIRTY_FILES=0` (`gates.txt`) | **MET** |
| — | `fold_proofs_m3b.sh` (the gate-assert library folded against itself) | 0 failed | `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0` (`fold-proofs.txt`) | **MET** |

The chain's own aggregate lines agree with the table read by hand:
`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` and
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`gates.txt`, `chain.txt`).

## The new fixtures, by name, in both configurations

The seventeen new `QpDegeneracyArm.*` fixtures, the sixteen `QpAntiCyclingArm.*`
fixtures M4 and its fix round left standing, and the twenty pre-existing
`QpWalkSeam.*` fixtures — **53 in the standalone `hven_qp_walk_seam_tests`
target** — are `Passed` **by name** in BOTH configurations, and the two name
lists are IDENTICAL (`suite-debug.txt`, `suite-release.txt`, read by name and
compared, not counted):

| target's fixture family | Debug | Release |
|---|---|---|
| `QpDegeneracyArm.*` (new this round) | 17 | 17 |
| `QpAntiCyclingArm.*` | 16 | 16 |
| `QpWalkSeam.*` | 20 | 20 |
| total | **53** | **53** |

The 17 is the same 17 the declaration derived from
`grep -c '^TEST(' tests/qp/test_qp_degeneracy_arm.cpp`, so the suite-count
derivation and the by-name reading agree.

No build warning in either configuration names any file this round touched. The
16 warnings per configuration are the tree's pre-existing ones — 7
`-Wpotentially-evaluated-expression`, 5 `-Wdeprecated-declarations` (the M6 W5
deprecations), 3 `-Wswitch`, 1 `-Wcharacter-conversion` — the same census M4
recorded.

## G5 — what it measured, and the one looseness it carries

| | Release | Debug |
|---|---|---|
| objects in the capture universe | 146 | 146 |
| before/after manifests name the same objects | yes | yes |
| `DIFFERS` lines in the P-SYM comparison | 0 | 0 |
| `psym_compare.sh compare` rc | 0 | 0 |
| classify rc | 0 | 0 |
| class A / class B / UNEXPLAINED | 0 / 0 / 0 | 0 / 0 / 0 |
| library objects compared by the section check | 42 | 42 |
| byte-identical | 42 | 41 |
| DEBUG-ONLY | 0 | 1 — `src/qp/qp_engine.cpp.o` |
| NON_DEBUG_MOVERS | 0 | 0 |
| REFUSED by the section check | 0 | 0 |
| both capture return codes | 0, 0 | 0, 0 |

**THE CLASSIFIER DERIVED ITS HEADER LIST; IT DID NOT CARRY ONE.** M4's G5 failed
because a copied classifier carried M3b's literal header tuple. This chain runs
M4's REPAIRED `classify_psym_m4.py`, which takes `--base`/`--head` and runs the
derivation itself; its transcript prints it
(`psym-classification-{Release,Debug}.txt`):

```
  cmd  : git -C /home/ghecht/Projects/hven diff --name-only f255bae9..0c6709ce… -- include/
  list : ['include/hven/detail/qp/qp_degeneracy_arm.h', 'include/hven/detail/qp/qp_walk_trace.h']
HEADER LIST ACCEPTED: [...]
```

Two headers, derived from THIS round's own range, and `qp_degeneracy_arm.h` is
the header M5 added. The runner's independently-derived list agreed with it, so
the cross-check passed rather than refusing.

**THE LOOSENESS, MEASURED RATHER THAN REPAIRED.** The classifier's negative
control (70 Release, 69 Debug) is taken over every `.o` under the build tree,
which includes test and bench objects the capture never covered. That is M3b's
pre-existing looseness, carried forward unchanged. The control **restricted to
the captured library universe** — the number that actually stands behind the "0
differing" claim — is **8 in Release and 7 in Debug**, computed here by
intersecting the classification's negative-control list with
`psym-libobjs-{Release,Debug}.txt`:

```
assembly_eval_seam.cpp.o  console_trace_sink.cpp.o  sqp_print.cpp.o
sqp_solver.cpp.o  soc_elastic_restoration.cpp.o  ipqp_engine.cpp.o
continuation.cpp.o                     [+ qp_engine.cpp.o in Release only]
```

Debug's list is Release's less `qp_engine.cpp.o`, which is the declared
debug-only exception. These are the same 8 / 7 M4 measured, which is what
`psym-derivation-m5.txt` §2 predicted when it checked that no compiled surface
moved between M4 fix round 1's base and this round's.

**THREE NOISE-ONLY OBJECTS, NONE OF THEM IN THE LIBRARY.**
`bench_corpus.cpp.o`, `bench_crossover.cpp.o` and `ipqp_e1_arm.cpp.o` differ in
bytes with every disassembly difference classified as `__LINE__`-class noise; all
three are bench targets built with `HVEN_TESTING`, none is in the production
library, and the classifier explains all three by this round's changed source or
header.

## What this gate does and does not establish

* It establishes that **the whole M5 degeneracy experiment — arm A, arm b1 and
  the diagnostic half together — leaves the production library byte-identical**
  to a tree with no degeneracy arm in it, on the objects themselves, in both
  configurations.
* It establishes that the **17 new fixtures pass in both configurations** and
  that the pre-existing suite is unchanged (2731 / 2732, 0 failed).
* **It says nothing about either arm's numerical result.** No leg has run at
  this point; the arms' behaviour is Task B's subject, governed by `RULE.md`.
* **No timing claim is made.** The chain ran under the box lock, one leg at a
  time, with `PGREP_EMPTY` recorded before each; every duration in these
  transcripts is informational (CLAUDE.md §7).

## Files

| file | what |
|---|---|
| `psym-derivation-m5.txt` | the expectations, derived and committed BEFORE the run (`0c6709ce`) |
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
