# M7 W2 M4 Task A — the six gates, declared before they ran and read afterwards

Branch `m7`. **BEFORE** `f5205770` (M3c's close). **HEAD** `196f0ef0`, tree
clean, unpushed. Chain `wrappers/chain_gates_m4.sh`, detached with `setsid` as
pid `2834224` at 2026-09-22T06:26:55-04:00, exited at 07:00:34-04:00.

The expectations were derived and committed BEFORE the chain was detached, in
`psym-derivation-m4.txt` (commit `196f0ef0`); the chain's own header carries the
same numbers. Nothing below was adjusted after seeing what it rejected.

## The result in one line

**Five of the six gates met their declared expectations. G5 FAILED — against its
own machinery, not against the tree.** The production library is unmoved: zero
`DIFFERS` lines in both configurations, all 42 library objects byte-identical in
Release, 41 byte-identical in Debug with `src/qp/qp_engine.cpp.o` differing in
`.debug_info`, `.debug_line`, `.debug_str` and `.rela.debug_line` and in no
other section, 0 non-debug movers in either configuration. That is the
substantive criterion G5 exists to carry, and it is met. The GATE is still
recorded FAILED.

## Declared vs measured

| gate | what it runs | DECLARED (before the run) | MEASURED (from the raw transcript) | |
|---|---|---|---|---|
| G1 | full Debug suite | 2709 tests, 0 failed | `100% tests passed, 0 tests failed out of 2709` (`suite-debug.txt`); 1 skipped + 2 disabled did not run | **MET** |
| G2 | the one >600 s Debug test, alone | 1 test, 0 failed | `1/1 Test #1518: Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 671.65 sec` (`suite-debug-long.txt`) | **MET** |
| G3 | full Release suite | 2710 tests, 0 failed | `100% tests passed, 0 tests failed out of 2710` (`suite-release.txt`) | **MET** |
| G4 | U0 replay vs the M6 close-gate pins | 27 cells × 75 columns × 3 engines, 0 differences | three lines, each `cells compared: 27  columns compared: 75  differences: 0` (`replay/comparison.txt`) | **MET** |
| G5 | P-SYM + the fail-closed section check, both configurations | DIFFERING 0 = class A 0 + class B 0 in both; DEBUG_ONLY Release 0, Debug 1 (`src/qp/qp_engine.cpp.o`); negative control ≥ 1 | the SUBSTANTIVE half met in full (below); the CLASSIFIER REFUSED (rc 2) and the five-count and two cross-config asserts had nothing to read | **FAILED** |
| G6 | install smoke, export contract, clang-format, `git diff --check`, dirty-file count | all green; align 32 | `HVEN_INSTALL_SMOKE ... PASSED`, both solvers converged through one generic consumer, 19 standalone-include TUs; `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32`; `CLANG_FORMAT_RC=0`; `DIFF_CHECK_RC=0`; `DIRTY_FILES=0` | **MET** |
| — | `fold_proofs_m3b.sh` (the gate-assert library folded against itself) | 0 failed | `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0` (`fold-proofs.txt`) | **MET** |

The eleven new `QpAntiCyclingArm.*` fixtures and the twenty pre-existing
`QpWalkSeam.*` fixtures — 31 in the standalone `hven_qp_walk_seam_tests` target
— are `Passed` **by name** in BOTH configurations; the two name lists are
identical (`suite-debug.txt`, `suite-release.txt`). No build warning in either
configuration names any file this round touched; the 16 warnings per
configuration are the tree's pre-existing ones (`-Wswitch`,
`-Wdeprecated-declarations` on the M6 W5 deprecations,
`-Wpotentially-evaluated-expression`, `-Wcharacter-conversion`).

## G5 — what failed, and what did not

**What failed.** The chain copies `classify_psym_fix1.py` out of M3b's artifact
and runs it. That script carried its changed-header list as a LITERAL tuple —
`('include/hven/detail/qp/qp_walk_trace.h',)`, M3b's list. This round's own
`git diff --name-only f5205770..196f0ef0 -- include/` names TWO headers, the
walk seam and the new `qp_anti_cycling_arm.h`. The runner derived ITS list
correctly, from git; the script's `--changed-headers` cross-check compared the
two, found them different, and refused with rc 2 and this message:

```
REFUSED: the round's changed-header list is ['include/hven/detail/qp/qp_anti_cycling_arm.h',
'include/hven/detail/qp/qp_walk_trace.h'] but this script declares
['include/hven/detail/qp/qp_walk_trace.h']
```

The refusal is CORRECT behaviour by a checker that was fed a stale declaration.
With no classification written, `assert_psym`'s five-count check failed in both
configurations and `assert_psym_cross_config`'s UNEXPLAINED-count clause failed
twice more. Six `ASSERT FAIL` lines, all six downstream of the one refusal, none
of them about an object. See `psym-classification-{Release,Debug}-REFUSED.txt`
— the two files, 203 bytes each, that the failed run left behind.

**What did not fail.** Everything G5 measures about the tree:

| | Release | Debug |
|---|---|---|
| `DIFFERS` lines in the P-SYM comparison | 0 | 0 |
| objects in the capture universe | 146 | 146 |
| before/after manifests name the same objects | yes | yes |
| `psym_compare.sh compare` rc | 0 | 0 |
| library objects compared by the section check | 42 | 42 |
| byte-identical | 42 | 41 |
| DEBUG-ONLY | 0 | 1 — `src/qp/qp_engine.cpp.o` |
| NON_DEBUG_MOVERS | 0 | 0 |
| REFUSED by the section check | 0 | 0 |
| both capture return codes | 0, 0 | 0, 0 |

`byte-verify.txt` re-takes all of that from the SAVED OBJECTS with a script
(`wrappers/verify_bytes_m4.sh`) that shares no code with `psym_compare.sh`, the
section checker or the classifier, and adds the two readings those three do not
print: the sections in which the one Debug object actually moved — exactly
`.debug_info`, `.debug_line`, `.debug_str`, `.rela.debug_line`, and nothing else,
out of 8 556 sections present identically on both arms — and the negative
control object by object, by name: 8 of 8 `qp_walk_trace.h` includers
byte-identical in Release, 7 of 8 in Debug (the eighth being the declared
exception).

**And the refusal cost nothing substantively.** The header the classifier did
not know about, `qp_anti_cycling_arm.h`, is opened by exactly four objects in
the tree, all of them in `HVEN_TESTING` test targets that `psym_compare.sh`
deliberately does not capture (`hven_qp_walk_seam_tests`' three objects and
`hven_ipqp_seam_tests`' `ipqp_engine.cpp.o`). It contributes ZERO production
includers, so a classifier that knew about it would have computed the same class
B (0) and the same production negative control. The stale list changed the
verdict, not the facts.

## The post-hoc record — a second reading, not a second run

`psym-posthoc.txt` runs the REPAIRED classifier
(`wrappers/classify_psym_m4.py`) and the same assertion layer, OFFLINE, over
the captures the failed chain already took. It is a POST-HOC RECORD and it is
labelled as one wherever it is quoted. **It does not make G5 pass.** The chain
is not re-run: no file under `src/`, `include/`, `tests/`, `bench/` or `cmake/`
has changed since `196f0ef0`, so a re-run would rebuild the same objects and
re-take the same captures, and a gate is judged by the run it had.

It reports, in both configurations: classify rc 0, DIFFERING 0, class A 0,
class B 0, UNEXPLAINED 0, and all eight `assert_psym` clauses plus all six
`assert_psym_cross_config` clauses OK.

Two things it declares about itself. First, the dependency map comes from each
configuration's OWN after-arm build tree (G3's for Release, G1's for Debug),
because `psym-build` — the tree the captures were taken from — was left standing
at the BEFORE arm when G5's last checkout was restored, so its `ninja -t deps`
predates the new header. Second, the classifier's negative control (70 Release,
69 Debug) is taken over every `.o` under the build tree, which includes test
objects the capture never covered; that is a pre-existing looseness of M3b's
script and it is MEASURED rather than repaired here. The control restricted to
the captured universe — the number that actually stands behind the "0 differing"
claim — is **8 in Release and 7 in Debug**, and the post-hoc runner asserts that
restricted number separately.

## The repair, and the fold proofs under it

`wrappers/classify_psym_m4.py` DERIVES its header list and carries nothing:
`--base` and `--head` are required, it runs
`git -C <repo> diff --name-only <base>..<head> -- include/` itself, PRINTS the
derivation into its own transcript (the command, both revisions resolved to full
shas, the resulting list), and refuses (new R6, exit 7) if the derivation cannot
be taken or comes back empty — an empty list would mean no class B and no
negative control, i.e. a classification that could not be non-vacuous.
`--changed-headers` survives as what it always should have been: the RUNNER's
independently-derived list, so that two derivations disagreeing is still a
refusal. `base..head` is used rather than bare `base` because the latter
compares against the WORKING TREE, which is right for a gate running on a clean
checkout of head and wrong for a reading taken offline afterwards.

`fold-proofs-classifier.txt` — 10 folds, 0 failed
(`wrappers/fold_proofs_classifier_m4.sh`):

| | fold | wanted |
|---|---|---|
| 1 | the WRONG runner list (M3b's one header) is REFUSED | rc 2 |
| 2 | the RIGHT runner list (this round's two headers) is ACCEPTED | rc 0 |
| 3 | a runner list with an EXTRA header is REFUSED | rc 2 |
| 4 | no runner list: the derivation alone governs and ACCEPTS | rc 0 |
| 5 | an unresolvable base REFUSES (R6) | rc 7 |
| 6 | an EMPTY derivation REFUSES (R6), never passes vacuously | rc 7 |
| 7 | the reversed range names the same two headers and ACCEPTS | rc 0 |
| 8 | a missing comparison file still REFUSES | rc 3 |
| 9 | a comparison with no `P-SYM: ` summary still REFUSES (R1) | rc 3 |
| 10 | declaring 1 mover over a 0-mover comparison still REFUSES (R2) | rc 4 |

Folds 1 and 2 are the pair that matters: the first reproduces this round's own
failure, the second proves the repair did not turn the check off.

## Process notes

* **A copied classifier must DERIVE, never CARRY, its header list.** This is the
  THIRD instance of one error class — a declaration inherited from the previous
  task instead of derived from this task's own `base..head`. M7 W2 M3a fix1 was
  the first, M3b fix1 the second, and M4 the third. M3b's own copy of the script
  named the trap in the comment directly above the literal it then kept
  ("Task A's copy of this script carried the ONE header task A touched; copying
  its list here would have credited this round's differing objects to a header
  it did not edit") and the literal survived anyway. A comment warning about a
  hazard is not a guard against it; the derivation is. The repaired copy has no
  literal to go stale.
* **The gate failed and stays failed.** The settler's standing ruling — the same
  one applied at M3a fix1 and M3b fix1 — is that a gate which fails against its
  own machinery is recorded FAILED with the cause established, not re-run to
  green. Re-running a chain until it agrees is how a gate stops being one.
* **Everything a sibling script is handed is checked to exist first.** The
  chain's `PATH_OK` lines (`chain.txt`) are that check; twelve paths were
  verified before the first gate ran.
* **The declaration that governed was committed first** (`196f0ef0`,
  `psym-derivation-m4.txt`), before the chain was detached, and is unedited.

## Files

| file | what |
|---|---|
| `psym-derivation-m4.txt` | the expectations, derived and committed BEFORE the run |
| `chain.txt` | the chain's own log: path checks, gate rc, fold-proof aggregate |
| `gates.txt` | the six gates' assert transcript |
| `suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt` | the raw ctest transcripts |
| `replay/` | the three head CSVs and the U0 comparison |
| `psym-compare-{Release,Debug}.txt` | the raw P-SYM comparisons |
| `psym-sections-{Release,Debug}.txt` | the fail-closed non-debug section check |
| `psym-libobjs-{Release,Debug}.txt` | the 42 library objects each configuration compared |
| `psym-classification-*-REFUSED.txt` | what the FAILED run left on disk |
| `psym-classification-*-posthoc.txt`, `psym-posthoc.txt` | the post-hoc reading |
| `byte-verify.txt` | the substantive criterion re-taken from the saved objects |
| `fold-proofs.txt` | the gate-assert library folded against itself, 46/0 |
| `fold-proofs-classifier.txt` | the repaired classifier folded against itself, 10/0 |
| `install-smoke.txt`, `export-contract.txt` | G6's two long transcripts |
