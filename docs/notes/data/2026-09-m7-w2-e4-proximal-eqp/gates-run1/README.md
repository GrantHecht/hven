# M7 W2 E4 (the proximal EQP) + fix rounds 1-3 — run 1 gate evidence: DECLARED vs MEASURED

> **RUN 1 OF TWO. G1–G5 GREEN, G6 FAIL.** Superseded on G6 by run 2 (`../gates/`, declaration
> `bba2fba6`, fix rounds 4 and 4b). This directory is the frozen record of run 1 and is not
> corrected or replaced — see `../README.md`'s fix-rounds table and the top-of-file note there.

Declaration (written and committed **before** the chain was detached, nothing in it adjusted
after): `../wrappers-gates/psym-derivation-e4.txt`, plus `../wrappers-gates/preflight.txt` and
`../wrappers-gates/rehearsal/`. **One declaration covers four rounds and two code commits**
(settler ruling E4-30): base `84ea0385` (the E3 docs record, whose sources are NC2 fix round 1's
gated sources — `git diff --stat 85dc01ac 84ea0385 -- include src tests cmake` and the same
against `0fa59cdc` are both EMPTY), range `84ea0385..0d519a2b` (`25e11280` = the squash of rounds
0/fix1/fix2; `0d519a2b` = fix round 3, the cherry-pick of `5ed7bf16`). Declaration commit
`13013b4c` adds no source, header or test (docs/notes only) — the chain therefore ran at HEAD
`13013b4c`, whose sources are `0d519a2b`'s. Chain: HEAD at start `13013b4c`, 0 dirty files
(`chain-e4.txt:2`; `dirty-files=0` in `gates-e4.txt:2`), started `GATES_START
2026-09-27T11:16:02-04:00` (`gates-e4.txt:1`; `chain-e4.txt:1`), ended `CHAIN_END
2026-09-27T12:23:26-04:00` (`chain-e4.txt:26`). Runner transcript: `gates-e4.txt` (copied verbatim
from `.scratch/m7w2e4gates/logs/gates-e4.log`). Chain wrapper transcript: `chain-e4.txt` (copied
verbatim from `.scratch/m7w2e4gates/logs/chain-e4.log`). Full test and build transcripts, copied
verbatim from `.scratch/m7w2e4gates/gates/`: `suite-debug.txt`, `suite-release.txt`,
`suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, `psym-*.txt`
(from `gates/psym/`: `classification-{Release,Debug}.txt`, `compare-{Release,Debug}.txt`,
`libobjs-{Release,Debug}.txt`, `libobjs-both-{Release,Debug}.txt`, `sections-{Release,Debug}.txt`),
`replay/` (the `run-*.log` files renamed `.txt`, `head-*.csv` and `comparison.txt` kept as-is). The
build trees and object directories and the build/configure logs are not copied. All 25 files under
this directory (plus `replay/`) were produced with `cp` from those sources and `cmp`-checked clean
against them (0 output, rc 0 on every file).

Rehearsal, pre-flight and offline transcripts, measured before the declaration and already
committed alongside it under `../wrappers-gates/` and `../wrappers-gates/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the raw transcript copied
alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e4.txt` §3) | MEASURED |
|---|---|---|
| tests | 2869 | 2869 — `gates-e4.txt:40`; `suite-debug.txt:5745` |
| failed | 0 | 0 — `gates-e4.txt:39`; `suite-debug.txt:5745` |
| by name | baseline 2863 started names − 24 REMOVED + 32 ADDED = 2871 | asserted — `gates-e4.txt:41` |

Three declared Debug `GTEST_SKIP`s (`QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent`,
`QpE4ProximalEqp.TheYorknetRestorationQpIsDriftFreeAndOptimal`,
`QpNc2FlatFace.TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeBorder`, each over M6's 600 s budget
and each PASSING in Release) sit inside ctest's `out of N` denominator and off the failure count,
so none moves the total or the by-name count (`suite-debug.txt:5749-5752` lists them under "The
following tests did not run"; `:5753` lists the pre-existing fourth Debug skip,
`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`, which is not one of the
round's three and moves nothing either).

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e4.txt:54`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e4.txt:53`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone, 617.04 s (`suite-debug-long.txt:3`;
informational — wall is never a gate value).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2870 | 2870 — `gates-e4.txt:66`; `suite-release.txt:5747` |
| failed | 0 | 0 — `gates-e4.txt:65`; `suite-release.txt:5747` |
| by name | baseline 2864 started names − 24 REMOVED + 32 ADDED = 2872 | asserted — `gates-e4.txt:67` |

By name against the derived baseline (NC2's gate record, `docs/notes/data/2026-09-m7-w2-nc2-flat-face/gates/suite-{debug,release}.txt`, 2863/2864 started names, ran at head
`52b16a8e` — no test file differs between that head and BEFORE, checked in the pre-flight): the
FIXTURE RENAME EVENT — 24 of the anti-cycling/degeneracy-arm names REMOVED because after E4 no
cell in this tree still cycles, 32 ADDED (24 renames + 8 of `QpE4ProximalEqp`'s own new cases), net
+8 per config, every name checked one-line (`psym-derivation-e4.txt` §3).

## G4 — the U0 replay (behavioural gate) — **43 DECLARED DIFFERENCES, NOT 0**

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75; the MEASURED difference set must EQUAL the declared 43-entry list, per-engine walk 17 / ssn 13 / ipm 13 | walk 17, ssn 13, ipm 13 = 43 — `replay/comparison.txt:1,90,172` ("differences: 17" / "differences: 13" / "differences: 13"); `CMP_RC=1` at `gates-e4.txt:162,245,332` (each engine's gated comparison against the M6 pin returns rc 1, i.e. differences exist, as declared) |
| `--check-g4` set-equality assert | rc 0, "the MEASURED difference set EQUALS the DECLARED 43-entry list" | `gates-e4.txt:335-342` — replay header lines = 3, walk shape 27/75/17, walk diffs 17, ssn shape 27/75/13, ssn diffs 13, ipm shape 27/75/13, ipm diffs 13, and the final set-equality ASSERT OK at `gates-e4.txt:342` |
| the one declared WORSE cell | `ipm f7_n1000_path_warm kkt_stationarity`, 1.3e-05 relative, in the list on the same terms as the other 42 | `replay/comparison.txt:176` — `A=3.449707634e-07  B=3.449751647e-07` |

No counter, status or integer column moved outside the declared 5 `qp_minors` cells; the residual
gate (1e-5 relative, 1e-13 absolute floor) applies to every cell not in the declared list.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 69 = 30 A + 39 B | 69 — `gates-e4.txt:366`, asserted `:373-374` | 70 — `gates-e4.txt:421`, asserted `:428-429` |
| class A (source changed) | 30 | 30 — `gates-e4.txt:367`, asserted `:378` | 30 — `gates-e4.txt:422`, asserted `:433` |
| class B (includer, source untouched) | Release 39 / Debug 40 | 39 — `gates-e4.txt:368`, asserted `:379` | 40 — `gates-e4.txt:423`, asserted `:434` |
| UNEXPLAINED | 0 | 0 — `gates-e4.txt:369`, asserted `:375` | 0 — `gates-e4.txt:424`, asserted `:430` |
| negative control | informational, ≥ 1 | 78 — `gates-e4.txt:376` | 61 — `gates-e4.txt:431` |
| compare rc | 1 (differences exist, as declared) | 1 — asserted `gates-e4.txt:371` | 1 — asserted `gates-e4.txt:426` |
| capture pair | after = before + ADDED(0) = 151 | 151 — `gates-e4.txt:349` | 151 — `gates-e4.txt:404` |
| outside-universe objects (3 `hven_qp_walk_seam_tests` .cpp.o) | built, 0 in the manifest | `gates-e4.txt:350-353` | `gates-e4.txt:405-408` |
| ONLY-AFTER | `[]` | `gates-e4.txt:356` | `gates-e4.txt:411` |
| library ADDED | none | `[]` — `gates-e4.txt:380` | `[]` — `gates-e4.txt:435` |
| class B by name | matches the declared 39/40-name list | `gates-e4.txt:473` | `gates-e4.txt:474` |
| class A by source | matches the declared 30-source list | `gates-e4.txt:475` | `gates-e4.txt:476` |

**The one configuration-asymmetric class B object**, `tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_solve_status.cpp.o` — Debug-only, byte-identical in Release, found in the Release
negative control: `gates-e4.txt:460` (asymmetry == declared), `:461` (negative-control listing
COMPLETE, 78 of 78 printed), `:462` (BYTE-IDENTICAL in Release, in the negative control), `:463`
(cross-config copy removes exactly that one object). Cross-config checks: class B sets equal (39
objects, `gates-e4.txt:464`), Release/Debug UNEXPLAINED 0 by count and list (`:465-466`), the two
class A sets identical (`:467-469`). `G5_ASSERT_RC=0` (`gates-e4.txt:470`).

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 9 = 3 class A (`sqp_solver`, `qp_engine`, `ws_independence`) + 6 class B (`ledger`, `sqp_print`, `trace_writer`, `soc_elastic_restoration`, `ipqp_engine`, `continuation`) | exactly that, asserted `gates-e4.txt:398` | exactly that, asserted `gates-e4.txt:458` |
| DEBUG_ONLY | Release 0 / Debug 5 | 0, asserted `gates-e4.txt:399` | 5, asserted `gates-e4.txt:459` |
| REFUSED | 0 | `SECTIONS_Release_RC=1` (all sections accounted, `gates-e4.txt:396`) | `SECTIONS_Debug_RC=1` (`gates-e4.txt:456`) |

## G6 — install smoke, export contract, format/diff cleanliness — **FAIL**

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | **FAILED, rc 1** — `INSTALL_SMOKE_RC=1` (`gates-e4.txt:480`) |
| export contract | OK, align 32 | **FAILED, rc 1, as a consequence of the install never happening** — `EXPORT_CONTRACT_RC=1` (`gates-e4.txt:487`); `export-contract.txt:1` reads `FAIL: no such file: .../hven-install/lib64/cmake/hven/hvenTargets.cmake` |
| clang-format | rc 0 over the 40 derived files | rc 0 — `CLANG_FORMAT_RC=0` (`gates-e4.txt:490`) |
| git diff --check | clean | rc 0 — `DIFF_CHECK_RC=0` (`gates-e4.txt:491`) |
| dirty files | 0 | 0 — `DIRTY_FILES=0` (`gates-e4.txt:492`) |
| G6 verdict | expected CLEAN | **`G6_ASSERT_RC=1`** (`gates-e4.txt:493`) |

**The failure mechanism, and why it is not an E4-source finding** (settler rulings E4-31/E4-32,
`REPORT-fix4.md`): the install smoke's default arm builds with the **system compiler** — GCC on
this box and on CI's Linux runner. A GCC precompiled header (`.gch`) records no input validation:
`install-smoke.txt:79` shows `[17/46] Building CXX object
CMakeFiles/hven.dir/cmake_pch.hxx.gch`, and the build then dies at `install-smoke.txt:86` with
`error: multiple definition of 'enum class hven::solvers::QpStatus'` out of
`src/drivers/ipm_solver.cpp`, reached both via `solver_counters.h <- ledger.h <- ipm_solver.cpp`
and via `detail/drivers/ipm_solver_fwd.h <- drivers/ipm_solver.h` — ccache's preprocessor-mode key
is the preprocessed text, which a comment-only, line-count-preserving header edit (in this range's
own commits) leaves byte-identical, so a `.gch` stale for the header is replayed and every
consuming TU re-parses it. The build stops at `install-smoke.txt:132` ("ninja: build stopped:
subcommand failed"). No engine source is implicated. Fixed by `5756530e` (fix round 4, the
Clang-only PCH guard) and `e212a308` (fix round 4b, test-only) — see run 2, `../gates/`.

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e4.txt:24`; `fold-proofs.txt` (final line, `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0`) |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its foreground start
(`gates-e4.txt:30,43,56,69,344,479`; `chain-e4.txt:17`).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=1` (`gates-e4.txt:496`); `GATES_AGGREGATE rc=1 over
[0 0 0 0 0 1]` (`gates-e4.txt:498`); `CHAIN_SUMMARY gates=1 fold_proofs=0` (`chain-e4.txt:27`).
`GATES_E4_RC=1` (`chain-e4.txt:15`).

## Copy verification

All 25 files under this directory (plus `replay/`) were produced with `cp` from the sources listed
above and then `cmp`-checked against those sources. Every comparison reported no differences (`cmp`
exit 0, no output) — `chain-e4.txt`, `gates-e4.txt`, `suite-debug.txt`, `suite-release.txt`,
`suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, the ten
`psym-*.txt` files, and the seven `replay/` files (the three `run-*.txt` compared against their
source `run-*.log`, byte-for-byte, only the filename changed).
