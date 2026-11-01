# M7 W2 M3a — FIX ROUND 1 — the gates evidence

Fix round 1 of M3a (`.superpowers/m7-w2-m3a-fix1-brief.md`) carried the settler's
six adjudications R1–R6 on task A. R1 is the only one that touched compiled code:
`SqpCounters::restoration_sub_status` stops being a `SolveStatus` that defaults to
`kOptimal` on a solve that never restored, and becomes a dedicated
`RestorationSubStatus` with an explicit `kNotRun = 0`, reached through a total
`to_restoration_sub_status(SolveStatus)` with an exhaustive switch and no default
arm. R2–R6 hardened the artifact's own checkers and corrected artifact text.

This directory holds the raw transcripts of the six-gate chain run at that change's
head, the fold proofs of the runner that produced them, and — added by the
settlement round that read those transcripts — the corrected P-SYM assertion layer's
own record and fold proofs.

Format precedent: `../gates/` (task A). As there, build and configure logs are NOT
committed; the chain and runner logs, the suite tails, the replay arms, the P-SYM
raw output and classification, the install-smoke and export-contract tails, and the
fold proofs are.

**G5 (P-SYM) FAILED.** It is recorded as FAILED, it was NOT re-run, and the chain's
aggregate return code is 1. Sections "The chain", "Why G5 failed" and "What the
substantive P-SYM criterion says" below are the whole account; nothing in this
directory presents G5 as green.

## What landed

Three commits at the head this chain tested, plus one written afterwards by the
settlement round:

| commit | what |
|---|---|
| `cc60978e` | R1 — `RestorationSubStatus`, the total map, the resume-note correction, docs, and the two new TEST blocks |
| `b96795ff` | R2–R6 — the hardened checkers, chain/fold exit codes, fold proofs, the re-runs on committed rows, every artifact-text correction, README "Corrections (fix1)", the ZAMB smoke disclosure |
| `ea027804` | the fix round's own `gates-fix1.sh`, `classify_psym_fix1.py`, `fold_proofs_fix1.sh` and `gates-fix1/psym-derivation.txt` — **the head this chain ran at** |
| `79c31263` | (after the run, settlement round) `gate_asserts_v2.sh`, `psym_assert_only.sh`, `fold_proofs_psym_v2.sh` — the corrected assertion layer |

`kSqpCountersFieldCount` stays **50** and `sizeof(SqpCounters)` stays **856**: R1
changed a field's TYPE, not the struct's layout. That is exactly why this round's
mover universe could not be derived the way task A's was, and it is the root of the
declaration error described below.

Two new TEST blocks, both run in BOTH configurations before the chain was detached
and both passing in this chain's own suites:

* `RestorationSubStatus.TheMapIsTotalAndTheSpellingsAreSolveStatusSPlusNotRun`
  — Debug `Test #1088 … Passed 0.00 sec`, Release `Test #1088 … Passed 0.00 sec`
* `SqpDriverRestorationExcursion.TheSubStatusDiscriminatesAtOneParentStatus`
  — Debug `Test #2432 … Passed 0.02 sec`, Release `Test #2431 … Passed 0.03 sec`

Baseline at `11b066e8` was Debug 2676 / Release 2677; +2 gives the 2678 / 2679 the
runner declared before the run.

## The chain

Detached chain, PID **2336366**, HEAD **`ea027804`**, `dirty-files=0`,
`BEFORE=11b066e8`, 2026-09-21T10:01:05 → 2026-09-21T10:47:31 America/New_York, six
lock segments, each with its own `PGREP_EMPTY` line and
`FOREGROUND_START`/`FOREGROUND_END` markers inside the lock (`chain.txt`,
`gates.txt`).

Expectations were declared IN the runner at `ea027804`, i.e. before the run:
`EXPECT_DEBUG_TESTS=2678`, `EXPECT_RELEASE_TESTS=2679`, `EXPECT_LONG_TESTS=1`,
replay `27/75/0` over `3` engines, and the P-SYM mover shape
`EXPECT_PSYM_DIFFERING=12`, `EXPECT_PSYM_CLASS_A=9`, `EXPECT_PSYM_CLASS_B=3`,
required of BOTH configurations, with `psym-derivation.txt` as the pre-run
derivation of those three numbers.

**An exit code is not a result.** Every number below was re-checked by the
settlement round against the raw transcript named in the last column — not against
the runner's own `ASSERT OK` lines, and not against `GATES_SUMMARY`.

| gate | expectation (declared before the run) | measured | verdict | checked against |
|---|---|---|---|---|
| G1 Debug suite | 0 failed, 2678 total | `100% tests passed, 0 tests failed out of 2678`; 0 `***Failed`/`***Exception`/`***Timeout` lines | PASS | `suite-debug.txt`, ctest summary line read directly |
| G2 the long test alone | 1/1 passed | `Continuation.ProbeBudgetBoundsAFailingProposal … Passed 673.00 sec`; `100% tests passed, 0 tests failed out of 1` | PASS | `suite-debug-long.txt` |
| G3 Release suite | 0 failed, 2679 total | `100% tests passed, 0 tests failed out of 2679`; 0 `***Failed`/`***Exception`/`***Timeout` lines | PASS | `suite-release.txt` |
| G3 the two new fixtures, both configs | Passed in Debug AND Release | all four lines `Passed` (listed above) | PASS | `suite-debug.txt`, `suite-release.txt`, grepped by name |
| G4 U0 replay, 3 engines | `27 / 75 / 0` on walk, ssn, ipm | three lines, each `cells compared: 27  columns compared: 75  differences: 0` | PASS | `replay/comparison.txt`, with `replay/head-{walk,ssn,ipm}.csv` |
| G4 binary provenance | no `-dirty` stamp | `# binary: ea0278040378` | PASS | `replay/head-walk.csv` header |
| G5 P-SYM capture, Release | rc 0 both arms, equal object counts | `after` rc 0 / 146 objects, `before` rc 0 / 146 objects, pair count 146 | PASS | `gates.txt` |
| G5 P-SYM capture, Debug | rc 0 both arms, equal object counts | `after` rc 0 / 146 objects, `before` rc 0 / 146 objects, pair count 146 | PASS | `gates.txt` |
| G5 P-SYM compare/classify rc | compare 1, classify 0, both configs | compare 1, classify 0, both configs | PASS | `gates.txt` |
| G5 P-SYM substance, Release | class A + class B = differing, UNEXPLAINED 0, control ≥ 1 | `10 = 7 + 3`, UNEXPLAINED 0, control 107 | PASS | `psym-classification-Release.txt` |
| G5 P-SYM substance, Debug | class A + class B = differing, UNEXPLAINED 0, control ≥ 1 | `11 = 8 + 3`, UNEXPLAINED 0, control 106 | PASS | `psym-classification-Debug.txt` |
| **G5 P-SYM declared shape, Release** | **`DIFFERING 12 = class A 9 + class B 3`** | **`DIFFERING 10 = class A 7 + class B 3`** | **FAIL** | `gates.txt`, `psym-classification-Release.txt` |
| **G5 P-SYM declared shape, Debug** | **`DIFFERING 12 = class A 9 + class B 3`** | **`DIFFERING 11 = class A 8 + class B 3`** | **FAIL** | `gates.txt`, `psym-classification-Debug.txt` |
| **G5 cross-configuration** | **the two classifications byte-identical** | **they differ on ONE object, `…/src/drivers/trace_writer.cpp.o`** | **FAIL (bad rule)** | `gates.txt`'s diff hunk, both classification files |
| G6 install smoke | rc 0, both solvers through one generic consumer | `both solvers converged … 19 standalone-include TUs compiled and linked`, `== hven install smoke: PASSED` | PASS | `install-smoke.txt` |
| G6 export contract | platform linux, align 32, no `INTERFACE_COMPILE_OPTIONS`, include dir once | `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32`; `== export contract OK for platform=linux, align=32` | PASS | `export-contract.txt` |
| G6 clang-format / `git diff --check` / dirty | rc 0 / rc 0 / 0 | `CLANG_FORMAT_RC=0`, `DIFF_CHECK_RC=0`, `DIRTY_FILES=0` | PASS | `gates.txt` |
| chain aggregate | — | `GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=1 G6=0`, `GATES_AGGREGATE rc=1`, `gates=1 fold_proofs=0` | **FAILED at G5** | `gates.txt`, `chain.txt` |
| fold proofs (runner in force at `ea027804`) | 43 folds, 0 failures | `FOLD_PROOFS_TOTAL=43  FOLD_PROOFS_FAILED=0` | PASS | `fold-proofs.txt` |

Five `ASSERT FAIL` lines are in `gates.txt`, and they are the five bolded rows:
Release differing, Release class A, Debug differing, Debug class A, and the
cross-configuration comparison.

## Why G5 failed

Two independent defects, and only one of them was a bad declaration.

### (1) The declaration's object universe was wrong — both configurations, one object

`psym-derivation.txt` measured the mover set by hashing **every object in a dev
build tree, 181 of them**, and predicted twelve, among them
`tests/CMakeFiles/hven_ipqp_seam_tests.dir/__/src/core/enum_names.cpp.o` (the seam
target compiles `src/core/enum_names.cpp` a second time, and R1 added two functions
to that file).

`scripts/psym_compare.sh` **deliberately excludes `hven_ipqp_seam_tests` from its
capture** — it and `hven_fault_injection_tests` recompile library TUs under
`HVEN_TESTING`, so their objects are not the production library's code; the
`hven_golden_rig*`, `hven_compile_fail_*` and `hven_sqp_snopt_f7` targets are out
for their own stated reasons. The script's header says so in as many words. The
gate's capture therefore holds **146 objects**, not 181, and grep confirms zero
`ipqp_seam` entries in all four `.psym-manifest` files. That object could not appear
in the differing set under any outcome.

Both configurations lose exactly it: **12 → 11, class A 9 → 8.** It is still listed
in each classification's NEGATIVE CONTROL, because the classifier's control walks
the whole build directory rather than the P-SYM manifest — which is why the
derivation's error was not visible from the classification's own totals.

No ccache, no differing build paths, and no DWARF miscount is involved: the
derivation's step 3 (per-symbol disassembly of the 82 byte-differing Debug objects,
82 → 12) was sound. The error is that its universe was the build tree and the
gate's universe is P-SYM's declared capture set.

### (2) The cross-configuration rule was wrong — Release only, one object

That accounts for one of Release's two missing objects. The second is
`CMakeFiles/hven.dir/src/drivers/trace_writer.cpp.o`, whose **source this round did
change** (it gains the `RestorationSubStatus` spellings) and which is class A in
Debug and absent from the differing set in Release. The per-symbol compare output
says why, and it is not inference:

* **Debug** (`psym-compare-Debug.txt`): the helpers R1 edits are emitted as
  out-of-line symbols whose mangled names carry the type —
  `ONLY-BEFORE key_value(…, SolveStatus, bool)`,
  `ONLY-BEFORE counter_never_absent(SolveStatus)`,
  `ONLY-AFTER key_value(…, RestorationSubStatus, bool)`,
  `ONLY-AFTER to_json(RestorationSubStatus)`,
  `ONLY-AFTER counter_never_absent(RestorationSubStatus)`. Two symbols leave and
  three arrive, the normalized listing moves 39369 → 39381 lines, and P-SYM's
  object verdict is `DIFFERS`.
* **Release** (`psym-compare-Release.txt`): those helpers are **inlined** into
  `counters_object[abi:cxx11](SqpCounters const&)`, so no such symbols exist on
  either side. The object-level line is
  `NOISE-ONLY … COUNTS 27870 insns; CHANGED 0; UNCLASSIFIED 0` — the instruction
  stream is unchanged — and the single residue is one relocation whose target is
  renamed, `- RELOC … to_string(SolveStatus)` / `+ RELOC … to_string(RestorationSubStatus)`.
  `to_string` lives in `enum_names.cpp` and is the one helper that cannot be inlined
  across the TU boundary, so it is the one that leaves a trace. P-SYM's object
  verdict is `NOISE-ONLY` plus a `RELOCATIONS` line.

So a source a round changes **may legitimately compile identically in one
configuration and not in the other**, and `assert_same_classification`'s premise —
"which TU changed and which TU includes a changed header are properties of the
dependency graph, not of the optimiser" — is true of the class A / class B
*predicates* and false of the *differing set* they are applied to. The rule forbade
a fact that is allowed to be true.

### A defect this exposed, registered and NOT silently fixed

`classify_psym*.py` harvests the differing set with `^DIFFERS\s+(\S+)` only. An
object whose P-SYM verdict is `NOISE-ONLY` **plus** a `RELOCATIONS` line — exactly
Release's `trace_writer.cpp.o` — is therefore never classified, and falls into the
negative control, which prints it under the heading "objects that ALSO include one
of those headers and **are byte-identical**". Its bytes are not identical: the
pre-run derivation's own Release byte comparison lists `trace_writer.cpp.o` among
the twelve byte-differing objects, and the differing byte is the renamed relocation.
The label is wrong for that one object.

This is recorded, not repaired here: changing the harvest would change the recorded
classification numbers of a run that is already evidence. It is carried to the
report as a registered item for the next round that touches the classifier.

## What the substantive P-SYM criterion says

The criterion that certifies layout neutrality — every mover is class A (source
changed by this task) or class B (an includer of a changed header), UNEXPLAINED = 0,
negative control ≥ 1, both capture rcs 0, capture-pair object counts equal — was
verified from the raw captures, by the settlement round, in **both** configurations:

| | Release | Debug |
|---|---|---|
| capture rc, after / before | 0 / 0 | 0 / 0 |
| capture pair object count | 146 = 146 | 146 = 146 |
| compare rc / classify rc | 1 / 0 | 1 / 0 |
| DIFFERING | 10 | 11 |
| class A (source changed) | 7 | 8 |
| class B (includes a changed header) | 3 | 3 |
| UNEXPLAINED | 0 | 0 |
| negative control | 107 | 106 |

The two class B sets are equal, name for name. The two class A sets differ on
exactly one object, `…/src/drivers/trace_writer.cpp.o`, whose source this round
changed. **It holds.** That is a statement about the captures, not a re-run gate:
G5 remains FAILED.

## The runner change (`79c31263`, written after the run)

`gate_asserts.sh`, `gates-fix1.sh`, `classify_psym_fix1.py` and
`fold_proofs_fix1.sh` are **not edited** — they produced the transcripts in `../gates/`
and in this directory, and a gate's evidence must stay reproducible from the code
that produced it. The corrected layer is three new files in `../wrappers/`:

* **`gate_asserts_v2.sh`** — version 1 with the P-SYM section replaced and every
  other helper carried over byte for byte. The expected mover shape becomes **per
  configuration** (`assert_psym` is called once with Release's shape and once with
  Debug's); `assert_same_classification` is replaced by **`assert_psym_cross_config`**,
  whose rule is: the two **class B sets must be equal**; **UNEXPLAINED must be 0 in
  both**, as a count and as a list; and the **symmetric difference of the two class A
  sets must be a subset of the objects whose source the task changed**. Anything
  else fails. It is paired with an internal-consistency check that every class A
  member of either configuration is a changed-source object — without that pairing
  the symmetric-difference clause would hold vacuously, since the classifier applies
  the same predicate itself; with it, a hand-edited, truncated or wrong-round
  classification is refused. A new `psym_set` helper reads the object LISTS, which
  version 1 never did — it compared counts only, and two classifications can agree
  on `class B: 3` while naming three different objects.
* **`psym_assert_only.sh`** — the assertion layer alone: no configure, no build, no
  capture, no `psym_compare.sh`, no classifier. Its inputs are two classification
  files that already exist. It prints a banner saying whether its expectation was
  written before or after the capture was seen.
* **`fold_proofs_psym_v2.sh`** — **17 folds, 0 failures** (`fold-proofs-psym-v2.txt`),
  every one of them on a **copy of the real capture** broken one property at a time:
  the real pair accepted (twice, once end-to-end and once through the cross-config
  rule alone); the round's own 12/9/3-for-both declaration refused, which reproduces
  G5's failure from the transcripts alone; each configuration's shape refused when
  given the other's, proving per-configuration is real; a class B set mismatch
  refused; an UNEXPLAINED object refused in Release, in Debug, and through the
  cross-config rule in isolation; a class A difference on a source the round did NOT
  change refused, both in isolation and end-to-end with the per-configuration shape
  declared to match the doctored file so that only the cross-config rule can refuse
  it; an empty changed-source list, a missing file, an empty file, a classification
  missing one of its five counts, and a zero negative control each refused; and a
  contrast fold in which **version 1's `assert_same_classification` refuses the real,
  correct capture pair** — the failure this round's G5 actually recorded.

`psym-assert-record.txt` is that layer run over the captures already in this
directory, with the corrected per-configuration numbers. **Its first lines say that
its expectation was written after the capture was seen and that it is a record, not
a pre-registered pass.** `PSYM_ASSERT_ONLY_RC=0`.

## Files

| file | what |
|---|---|
| `chain.txt` | the detached chain's own log: HEAD, `GATES_FIX1_RC=1`, `FOLD_PROOFS_FIX1_RC=0`, `gates=1 fold_proofs=0` |
| `gates.txt` | the six segments, every assertion line, `GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=1 G6=0`, `GATES_AGGREGATE rc=1` |
| `suite-debug.txt` | G1, Debug, the long test excluded |
| `suite-debug-long.txt` | G2, `Continuation.ProbeBudgetBoundsAFailingProposal` alone |
| `suite-release.txt` | G3, Release, everything |
| `replay/` | G4: `comparison.txt` and the three engine CSVs |
| `psym-derivation.txt` | the PRE-RUN derivation of the declared shape — committed at `ea027804`, left byte-identical, and wrong for the reason given above |
| `psym-compare-{Release,Debug}.txt` | G5 raw P-SYM output |
| `psym-classification-{Release,Debug}.txt` | G5 classification |
| `psym-assert-record.txt` | the corrected assertion layer over those captures; a RECORD, written after |
| `install-smoke.txt`, `export-contract.txt` | G6 |
| `fold-proofs.txt` | the 43 folds of the runner in force at `ea027804` |
| `fold-proofs-psym-v2.txt` | the 17 folds of the corrected assertion layer |
