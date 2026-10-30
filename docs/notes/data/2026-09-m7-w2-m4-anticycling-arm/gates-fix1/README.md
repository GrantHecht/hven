# M7 W2 M4 FIX ROUND 1 — the six gates, declared before they ran and read afterwards

Branch `m7`. **BEFORE** `f5205770` (M7 W2 M3c's close, which is also M4's own
base — the claim this gate carries is *"the whole anti-cycling arm, as
corrected, leaves the production library byte-identical to the tree that has no
arm in it"*, not *"the fix round moved nothing"*). **HEAD** `3935eb9d`, tree
clean, unpushed. Chain `wrappers/chain_gates_m4fix1.sh`, detached with `setsid`
as pid `2968508` at 2026-09-22T10:28:46-04:00, `CHAIN_END 11:00:12-04:00`
(`chain.txt`).

The expectations were derived and committed BEFORE the first of the three chains
was detached, in `psym-derivation-m4fix1.txt` (commit `90a31da4`,
derived-at 09:20:59). **That file has not been edited since, and it governed all
three runs unchanged.** Nothing below was adjusted after seeing what it
rejected. Wall-clock in this file is informational and is never a timing claim.

## The result in one line

**All six gates met their declared expectations, and the fold-proof library
folded clean.** `GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`
(`gates.txt`), `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0` and
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain.txt`, `fold-proofs.txt`). The
production library is unmoved by the corrected arm: zero differing objects in
both configurations, all 42 library objects byte-identical in Release, 41 in
Debug with `src/qp/qp_engine.cpp.o` differing only in debug sections, 0
non-debug movers in either configuration.

**This is run 3.** Runs 1 and 2 were refused by their own runner, for two
different defects in that runner and for nothing about the tree. Both are on the
record beside this one, with their transcripts, in `run1-refused/` and
`run2-refused/`. The three-run history is below.

## Declared vs measured

Every MEASURED cell is read from the raw transcript named in it. No number is
retyped from a summary and no exit code is treated as a result.

| gate | what it runs | DECLARED (before run 1) | MEASURED (from the raw transcript) | |
|---|---|---|---|---|
| G1 | full Debug suite | 2714 tests, 0 failed | `100% tests passed, 0 tests failed out of 2714` (`suite-debug.txt:5435`); 1 skipped + 2 disabled did not run (`gates.txt`); `ASSERT OK G1 Debug suite total = 2714` (`gates.txt:26`) | **MET** |
| G2 | the one >600 s Debug test, alone under the lock | 1 test, 0 failed | `1/1 Test #1523: Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 681.17 sec`, `100% tests passed, 0 tests failed out of 1` (`suite-debug-long.txt`) | **MET** |
| G3 | full Release suite | 2715 tests, 0 failed | `100% tests passed, 0 tests failed out of 2715` (`suite-release.txt:5437`); `ASSERT OK G3 Release suite total = 2715` (`gates.txt:51`) | **MET** |
| G4 | U0 replay vs the M6 close-gate pins | 27 cells × 75 columns × 3 engines, 0 differences | three lines, each `cells compared: 27  columns compared: 75  differences: 0` (`replay/comparison.txt`), one per engine (walk, ssn, ipm) | **MET** |
| G5 | P-SYM + the fail-closed section check, both configurations | DIFFERING 0 = class A 0 + class B 0 in both; DEBUG_ONLY Release 0, Debug 1 (`src/qp/qp_engine.cpp.o`); negative control ≥ 1; `psym_compare.sh compare` exits 0 | classifier rc 0 in both, `DIFFERING OBJECTS: 0`, class A 0, class B 0, UNEXPLAINED 0 (`psym-classification-{Release,Debug}.txt`); `PSYM_COMPARE_RC_Release=0`, `PSYM_COMPARE_RC_Debug=0` (`gates.txt`); sections below; negative control 69 Release / 68 Debug; all 8 `assert_psym` clauses and all 6 `assert_psym_cross_config` clauses OK, `G5_ASSERT_RC=0` | **MET** |
| G6 | install smoke, export contract, clang-format, `git diff --check`, dirty-file count | all green; align 32 | `== hven install smoke: PASSED`, both solvers converged through one generic consumer, 19 standalone-include TUs (`install-smoke.txt`); `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32`, `== export contract OK for platform=linux, align=32` (`export-contract.txt`); `CLANG_FORMAT_RC=0`, `DIFF_CHECK_RC=0`, `DIRTY_FILES=0` (`gates.txt:168-170`); `G6_ASSERT_RC=0` | **MET** |
| — | `fold_proofs_m3b.sh` (the gate-assert library folded against itself) | `FOLD_PROOFS_FAILED=0` | `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0` (`fold-proofs.txt`) | **MET** |

The **16** `QpAntiCyclingArm.*` fixtures — M4's eleven plus this round's five —
and the **20** pre-existing `QpWalkSeam.*` fixtures are `Passed` **by name** in
BOTH configurations, and the two name lists are identical
(`suite-debug.txt`, `suite-release.txt`). The five added here are
`ArmEsKthIterationRunsAtTheExpandedToleranceAndTheResetFollowsIt`,
`ArmEsResetCountsANontrivialReProjection`,
`ArmEsFloorStepOnTheNegativeCurvatureRideIsNotRePinned`,
`ArmEsNegligiblePivotParameterDecidesWhichConstraintsCanBlock` and
`AnArmedSolveDoesNotReachIntoAnUnarmedOneRunningBesideIt`; two pre-existing
fixtures were RENAMED, not added, which is why 2709→2714 and 2710→2715 is a
clean +5 against M4's met declaration.

## G5 in detail — what the section check and the classifier read

| | Release | Debug |
|---|---|---|
| objects in the capture universe (both arms) | 146 | 146 |
| capture return codes (after, before) | 0, 0 | 0, 0 |
| `psym_compare.sh compare` rc | 0 | 0 |
| byte-identical in the P-SYM universe | 143 | 141 |
| differing within the accepted NOISE class | 3 (0 in the production library) | 4 (1 in the production library — `qp_engine.cpp.o`) |
| UNCLASSIFIED differences | 0 | 0 |
| classifier rc / DIFFERING / class A / class B / UNEXPLAINED | 0 / 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 / 0 |
| library objects compared by the section check | 42 | 42 |
| BYTE_IDENTICAL | 42 | 41 |
| DEBUG_ONLY | 0 | 1 — `CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o` |
| NON_DEBUG_MOVERS | 0 | 0 |
| REFUSED by the section check | 0 | 0 |
| negative control (classifier's own, whole build tree) | 69 | 68 |
| — of which are PRODUCTION LIBRARY objects | 8 | 7 |

Sources: `psym-compare-{Release,Debug}.txt`,
`psym-sections-{Release,Debug}.txt`,
`psym-classification-{Release,Debug}.txt`, `psym-libobjs-{Release,Debug}.txt`,
and the asserts in `gates.txt`.

Two things about the negative control, both inherited from M3b's script and
MEASURED rather than repaired here, exactly as M4's `gates/README.md` recorded
them. It is taken over every `.o` under the build tree, which includes test
objects the capture never covered; the number restricted to the production
library — the one that actually stands behind the "0 differing" claim — is **8
in Release and 7 in Debug**, counted from the classifier's own printed list.
The declared bar was `≥ 1`, and both readings clear it either way.

The classifier is `wrappers/classify_psym_m4.py`, M4's repair, which DERIVES its
changed-header list from `<base>..<head>` and carries no literal. Its derivation
is printed into its own transcript and into `gates.txt`:

```
cmd  : git -C /home/ghecht/Projects/hven diff --name-only f5205770..3935eb9d91d2ed839a5314c1e1ad2bbbe742a6d4 -- include/
list : ['include/hven/detail/qp/qp_anti_cycling_arm.h', 'include/hven/detail/qp/qp_walk_trace.h']
HEADER LIST ACCEPTED: [...the same two...]
```

The runner still derives its own list independently and hands it over as
`--changed-headers`, so two derivations disagreeing is still a refusal. That is
the repair for the defect that made M4's own G5 fail; it held here.

## The three chains, and why there are three

| run | pid | detached | ended | HEAD | outcome | cause |
|---|---|---|---|---|---|---|
| 1 | 2911680 | 09:21:24 | 09:52:34 | `90a31da4` | **REFUSED** | six gate segments CARRIED M4's scratch path — `S=$ROOT/.scratch/m7w2m4gates` — so G1–G4 built and wrote into the previous round's tree while the asserts read this one's. `GATES_SUMMARY G1=1 G2=1 G3=1 G4=1 G5=0 G6=0`, `FOLD_PROOFS_AGGREGATE_RC=3`. Full account: `run1-refused/WHY.md` |
| 2 | 2950851 | 09:56:14 | 10:22:07 | `2a148849` | **REFUSED** | the guard added to stop that was `grep -n 'scratch'` over the whole segment and fired on a **comment** in `g5.sh`. G5 and G6 never ran. `GATES_M4FIX1_RC=2`, `CHAIN_SUMMARY gates=2 fold_proofs=0`. Full account: `run2-refused/WHY.md` |
| 3 | 2968508 | 10:28:46 | 11:00:12 | `3935eb9d` | **GREEN** | — |

**Neither refusal moved the declaration.** `psym-derivation-m4fix1.txt` was
committed at `90a31da4`, before run 1, and is unedited. What moved between the
runs is the RUNNER, and only ever in the direction of judging more, earlier:

* `2a148849` made every segment read `${GATES_SCRATCH:?}` instead of naming a
  path, and added `check_segment`.
* `3935eb9d` replaced that textual check with a **value assertion** —
  `assert_segment_scratch` evaluates the segment's own prologue in a subshell
  with `GATES_SCRATCH` set as the segment will see it, and requires the resolved
  `$S` and `$G` to be this chain's tree and its `gates/`. The residual textual
  checks strip comments first. All six segments are emitted and judged in a
  **pre-flight before G1**, and the round now holds **one** scratch literal
  (`gates-m4fix1.sh:91`, answering `--print-scratch`).

Run 3's pre-flight is the first six lines of `gates.txt` after the header:
six `SEGMENT_OK ... resolves S=.../m7w2m4fix1gates  G=.../m7w2m4fix1gates/gates`
lines and `PREFLIGHT_OK: 6 segments written and asserted against GATES_SCRATCH,
before the first leg`.

**Re-running a gate after repairing its runner is not re-measuring an
experiment.** Nothing about what the gate WANTS moved across the three runs;
what moved is whether the runner could hand its own asserts the files it had
just written, and whether its guard could express the property it was guarding.
That argument is made on the record in both `run1-refused/WHY.md` and
`run2-refused/WHY.md`, and it is the same standing distinction the settler
applied when M4's G5 was recorded FAILED rather than re-run.

One residual, named rather than fixed: the fold-proof segment's existence check
on `$DBG_TREE/CTestTestfile.cmake` still runs after the legs, not in the
pre-flight. It **cannot** be hoisted — G1 produces that file. What could be
pre-flighted was the agreement of the two `$S` literals, and `3935eb9d` removed
the second literal instead, which is the stronger fix.

## Disclosures

**1. Run 1's captures overwrote M4's SCRATCH capture directories. M4's
committed evidence is untouched.** Run 1's G5 segment carried
`.scratch/m7w2m4gates` and took its P-SYM captures there, overwriting the
`before-*`, `after-*`, `*-obj-*` directories and the comparison/classification
/sections/libobjs transcripts that M4's own chain had left in that scratch tree.
**Nothing M4 pinned moved.** `docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/gates/`
is M4's committed gate record and it is unchanged; the overwrite touched a
scratch tree only. This is said here because the brief asked that it be on the
record, not because any evidence was lost.

**2. Run 1's raw P-SYM artifacts are now committed.** They survived in
`.scratch/m7w2m4gates/gates/psym/` and are copied verbatim into
`run1-refused/psym-raw/` — `psym-compare-{Release,Debug}.txt`,
`psym-sections-{Release,Debug}.txt`,
`psym-classification-{Release,Debug}.txt`, `psym-libobjs-{Release,Debug}.txt`.
Their own header lines identify them as run 1's: `head : 90a31da4...` in the
classifications, and `BEFORE_DIR=/…/.scratch/m7w2m4gates/…` in the sections
files. They read the same shape run 3 read (DIFFERING 0 both configurations;
Release 42/42 byte-identical, DEBUG_ONLY 0; Debug 41/42, DEBUG_ONLY 1; 0
non-debug movers; negative control 69 / 68), which is a reading and **not** a
gate result — run 1 is refused. G5's readings were already preserved
transcribed in `run1-refused/gates.txt`; the raw files are added so the
transcription can be checked against its source.

**3. Naming.** The two refused runs are `run1-refused/` and `run2-refused/`, and
each carries its account in `WHY.md`. Part B's brief said `run1-FAILED/README`;
run 1's implementer chose `run1-refused/WHY.md`, run 2 followed the same
convention, and both are left as written rather than renamed after the fact.
Said here so a reader following the brief is not hunting for a directory that
does not exist.

**4. The process note the round earned is now written down.** The artifact's
front-page `README.md` gains a fifth process finding covering both defects: *a
runner checks every path it hands a sibling AND every path it hands ITSELF* —
one `S` defined once, every segment asserting that the directory it resolves is
the chain's own, fail-closed before the first leg — and *a guard over TEXT
cannot test a property about VALUES*, with the corollary that a guard which can
only say "I found nothing bad" is weaker than one that says "this resolves to
my tree". Run 1 is the **fourth** instance of the carried-declaration error
class (M3a fix1 classifier, M3b fix1 classifier, M4's G5 classifier, M4 fix1's
own runner); run 2 is the first instance of the second class. M4's own
`gates/README.md` is M4's pinned gate record and is **not** edited to carry a
finding from a later round.

## Files

| file | what |
|---|---|
| `psym-derivation-m4fix1.txt` | the expectations, derived and committed at `90a31da4` BEFORE run 1, unedited, governing all three runs |
| `chain.txt` | run 3's chain log: path checks, gate rc, fold-proof aggregate, `CHAIN_SUMMARY` |
| `gates.txt` | run 3's six-gate assert transcript, including the pre-flight |
| `suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt` | run 3's raw ctest transcripts |
| `replay/` | run 3's three head CSVs and the U0 comparison |
| `psym-compare-{Release,Debug}.txt` | the raw P-SYM comparisons |
| `psym-sections-{Release,Debug}.txt` | the fail-closed non-debug section check |
| `psym-classification-{Release,Debug}.txt` | the classifier's derivation, classes and negative control |
| `psym-libobjs-{Release,Debug}.txt` | the 42 library objects each configuration compared |
| `fold-proofs.txt` | the gate-assert library folded against itself, 46/0 |
| `install-smoke.txt`, `export-contract.txt` | G6's two long transcripts |
| `run1-refused/` | run 1's chain and gate logs, its leg transcripts, `WHY.md`, and `psym-raw/` |
| `run2-refused/` | run 2's chain and gate logs, its leg transcripts, `g5-as-generated.sh` (the segment the guard wrongly refused), `WHY.md` |
