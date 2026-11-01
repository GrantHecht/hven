# M7 W2 E4 (the proximal EQP) + fix rounds 1-4b — run 2 gate evidence: DECLARED vs MEASURED

> **RUN 2 OF TWO, THE SUCCESSOR DECLARATION. G1–G6 GREEN.** Run 1 (`../gates-run1/`, declaration
> `13013b4c`) measured G1–G5 GREEN and G6 FAIL (a stale GCC `.gch` replayed by ccache after a
> comment-only header edit). Fix rounds 4 (`5756530e`, the Clang-only PCH guard) and 4b
> (`e212a308`, the `residual_columns_agree` absolute floor) land on top, and this declaration
> (`bba2fba6`) re-derives and re-measures every value on the new range `84ea0385..e212a308` —
> nothing is carried from run 1. Run 1's files are byte-frozen; see `../README.md`.

Declaration (written and committed **before** the r2 chain was detached, nothing in it adjusted
after): `../wrappers-gates/psym-derivation-e4-r2.txt`, plus `../wrappers-gates/preflight-r2.txt`
and `../wrappers-gates/rehearsal/rehearse-r2.sh` + `offline-r2.sh` + the `rehearsal-r2-*.txt`
transcripts. Range `84ea0385..e212a308`, which contains run 1's own declaration commit `13013b4c`
(adds only `docs/notes/data/`, invisible to every derivation) plus fix round 4 (`5756530e`,
cherry-pick of `a8184985`) and fix round 4b (`e212a308`, cherry-pick of `84fb520e`). Declaration
commit `bba2fba6` likewise adds only docs/notes data. Chain: HEAD at start `bba2fba6`, 0 dirty
files (`chain-e4-r2.txt:2`; `dirty-files=0` in `gates-e4-r2.txt:2`), started `GATES_START
2026-09-27T15:47:43-04:00` (`gates-e4-r2.txt:1`; `chain-e4-r2.txt:1`), ended `CHAIN_END
2026-09-27T16:55:38-04:00` (`chain-e4-r2.txt:26`). Runner transcript: `gates-e4-r2.txt` (copied
verbatim from `.scratch/m7w2e4gates-r2/logs/gates-e4-r2.log`). Chain wrapper transcript:
`chain-e4-r2.txt` (copied verbatim from `.scratch/m7w2e4gates-r2/logs/chain-e4-r2.log`). Full test
and build transcripts, copied verbatim from `.scratch/m7w2e4gates-r2/gates/`: `suite-debug.txt`,
`suite-release.txt`, `suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`,
`fold-proofs.txt`, `psym-*.txt` (from `gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`, `libobjs-both-{Release,Debug}.txt`,
`sections-{Release,Debug}.txt`), `replay/` (the `run-*.log` files renamed `.txt`, `head-*.csv` and
`comparison.txt` kept as-is). The build trees, object directories and build/configure logs are not
copied. All 25 files under this directory (plus `replay/`) were produced with `cp` from those
sources and `cmp`-checked clean against them (0 output, rc 0 on every file).

Rehearsal, pre-flight and offline transcripts, measured before the declaration and already
committed alongside it under `../wrappers-gates/` and `../wrappers-gates/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the raw transcript copied
alongside this README. Where a value comes out the same as run 1's, that is noted as a comparison,
never as the derivation — every number here is this chain's own measurement.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e4-r2.txt` §3) | MEASURED |
|---|---|---|
| tests | 2869 | 2869 — `gates-e4-r2.txt:41`; `suite-debug.txt:5745` ("100% tests passed, 0 tests failed out of 2869") |
| failed | 0 | 0 — `gates-e4-r2.txt:40`; `suite-debug.txt:5745` |
| by name | baseline 2863 started names − 24 REMOVED + 32 ADDED = 2871 | asserted — `gates-e4-r2.txt:42` |

Fix round 4 touches no file under `tests/`; fix round 4b touches one (`test_corpus_cells.cpp`,
already one of the 33 MODIFIED sources) but adds/removes 0 `TEST(`/`TEST_F(` lines over
`5756530e..e212a308`, so the by-name sets could not move — re-derived and `cmp`-identical to the
runner's literals regardless (§3). The same three declared Debug `GTEST_SKIP`s as run 1 sit inside
ctest's `out of N` denominator and off the failure count.

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e4-r2.txt:55`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e4-r2.txt:54`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone, 618.44 s (`suite-debug-long.txt:3`;
informational).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2870 | 2870 — `gates-e4-r2.txt:67`; `suite-release.txt:5747` |
| failed | 0 | 0 — `gates-e4-r2.txt:66`; `suite-release.txt:5747` |
| by name | baseline 2864 started names − 24 REMOVED + 32 ADDED = 2872 | asserted — `gates-e4-r2.txt:68` |

Suites 2869 / 2870 = the derived baseline 2861 / 2862 (NC2's gate record, `docs/notes/data/2026-09-m7-w2-nc2-flat-face/gates/suite-{debug,release}.txt`) **− 24 REMOVED + 32 ADDED** — the
FIXTURE RENAME EVENT declared identically to run 1 (§3): 24 anti-cycling/degeneracy-arm names
removed because after E4 no cell in this tree still cycles, 32 added (24 renames + 8 of
`QpE4ProximalEqp`'s own new cases). Same as run 1 — a comparison, not a re-derivation shortcut;
the runner re-derived and `cmp`-checked all 56 names fresh against `84ea0385..e212a308`.

## G4 — the U0 replay (behavioural gate) — **43 DECLARED DIFFERENCES, NOT 0**

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75; MEASURED set must EQUAL the declared 43-entry list, walk 17 / ssn 13 / ipm 13 | walk 17, ssn 13, ipm 13 = 43 — `replay/comparison.txt:1,90,172` ("differences: 17" / "differences: 13" / "differences: 13"); `CMP_RC=1` at `gates-e4-r2.txt:163,246,333` |
| `--check-g4` set-equality assert | rc 0 | `gates-e4-r2.txt:336-343` — replay header lines = 3, walk shape 27/75/17, walk diffs 17, ssn shape 27/75/13, ssn diffs 13, ipm shape 27/75/13, ipm diffs 13, set-equality ASSERT OK at `:343` |
| the one declared WORSE cell | `ipm f7_n1000_path_warm kkt_stationarity`, 1.3e-05 relative | `replay/comparison.txt:176` — `A=3.449707634e-07  B=3.449751647e-07` |

**Identical to run 1's own gate CSVs on every measured value**: `diff` of `replay/comparison.txt`
against `../gates-run1/replay/comparison.txt` shows exactly 3 differing lines out of 257 — the
three engine-header label lines ("M7 W2 E4" → "M7 W2 E4 R2", `comparison.txt:1,90,172`), the label
change §9 of the derivation documents as the only intended difference in the emitted `g4` segment.
Every data line, including line 176's worse-cell values (`A=3.449707634e-07  B=3.449751647e-07`,
identical in both files), is byte-for-byte the same, confirming the derivation's claim that fix 4
(build-only, PCH guard inert on Clang — `rehearsal/rehearsal-r2-configure-Release.txt` reads "The
CXX compiler identification is Clang 22.1.8") and fix 4b (proven behaviourally unchanged on
`scripts/compare_replay.py`, AST-identical modulo docstrings) moved nothing on the replay.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 69 = 30 A + 39 B | 69 — `gates-e4-r2.txt:367`, asserted `:374-375` | 70 — `gates-e4-r2.txt:422`, asserted `:429-430` |
| class A (source changed) | 30 | 30 — `gates-e4-r2.txt:368`, asserted `:379` | 30 — `gates-e4-r2.txt:423`, asserted `:434` |
| class B (includer, source untouched) | Release 39 / Debug 40 | 39 — `gates-e4-r2.txt:369`, asserted `:380` | 40 — `gates-e4-r2.txt:424`, asserted `:435` |
| UNEXPLAINED | 0 | 0 — `gates-e4-r2.txt:370`, asserted `:376` | 0 — `gates-e4-r2.txt:425`, asserted `:431` |
| negative control | informational, ≥ 1 | 78 — `gates-e4-r2.txt:377` | 61 — `gates-e4-r2.txt:432` |
| compare rc | 1 | 1 — asserted `gates-e4-r2.txt:372` | 1 — asserted `gates-e4-r2.txt:427` |
| capture pair | after = before + ADDED(0) = 151 | 151 — `gates-e4-r2.txt:350` | 151 — `gates-e4-r2.txt:405` |
| outside-universe objects (3 `hven_qp_walk_seam_tests` .cpp.o) | built, 0 in the manifest | `gates-e4-r2.txt:351-354` | `gates-e4-r2.txt:406-409` |
| ONLY-AFTER | `[]` | `gates-e4-r2.txt:357` | `gates-e4-r2.txt:412` |
| library ADDED | none | `[]` — `gates-e4-r2.txt:381` | `[]` — `gates-e4-r2.txt:436` |
| class B by name | matches the declared 39/40-name list | `gates-e4-r2.txt:474` | `gates-e4-r2.txt:475` |
| class A by source | matches the declared 30-source list | `gates-e4-r2.txt:476` | `gates-e4-r2.txt:477` |

**Identical to run 1's own numbers** (69/70, 30 A, 39/40 B, UNEXPLAINED 0 both) — predicted because
fix 4 is build-only and fix 4b is test-only with `libhven.a` byte-identical, then MEASURED to hold
by this chain's own G5 leg, independent of run 1's.

**The one configuration-asymmetric class B object**, `tests/drivers/CMakeFiles/hven_drivers_tests.dir/test_solve_status.cpp.o` — Debug-only, byte-identical in Release, found in the Release
negative control: `gates-e4-r2.txt:461` (asymmetry == declared), `:462` (negative-control listing
COMPLETE, 78 of 78 printed), `:463` (BYTE-IDENTICAL in Release, in the negative control), `:464`
(cross-config copy removes exactly that one object). Cross-config checks: class B sets equal (39
objects, `gates-e4-r2.txt:465`), Release/Debug UNEXPLAINED 0 by count and list (`:466-467`), the
two class A sets identical (`:468-470`). `G5_ASSERT_RC=0` (`gates-e4-r2.txt:471`).

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 9 = 3 class A (`sqp_solver`, `qp_engine`, `ws_independence`) + 6 class B (`ledger`, `sqp_print`, `trace_writer`, `soc_elastic_restoration`, `ipqp_engine`, `continuation`) | exactly that, asserted `gates-e4-r2.txt:399` | exactly that, asserted `gates-e4-r2.txt:459` |
| DEBUG_ONLY | Release 0 / Debug 5 | 0, asserted `gates-e4-r2.txt:400` | 5, asserted `gates-e4-r2.txt:460` |
| REFUSED | 0 | `SECTIONS_Release_RC=1` (all sections accounted, `gates-e4-r2.txt:397`) | `SECTIONS_Debug_RC=1` (`gates-e4-r2.txt:457`) |

## G6 — install smoke, export contract, format/diff cleanliness — **GREEN**

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED, no `.gch` on the GCC arm | **PASSED, rc 0** — `INSTALL_SMOKE_RC=0` (`gates-e4-r2.txt:481`); `install-smoke.txt:1239` reads `== hven install smoke: PASSED`; compiler used is `GNU 16.2.1` (`install-smoke.txt:3,1194`); **zero** `.gch` occurrences in the whole transcript (`grep -c '\.gch' install-smoke.txt` → `0`), confirming the Clang-only PCH guard left this GCC build with no precompiled header at all |
| export contract | OK, align 32 | **OK, rc 0** — `EXPORT_CONTRACT_RC=0` (`gates-e4-r2.txt:488`); `export-contract.txt:3-5`: `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32`, `INTERFACE_COMPILE_OPTIONS OK: absent`, `INTERFACE_INCLUDE_DIRECTORIES OK: appears exactly once`, final line `export-contract.txt:6`: `== export contract OK for platform=linux, align=32` |
| clang-format | rc 0 over the 40 derived files | rc 0 — `CLANG_FORMAT_RC=0` (`gates-e4-r2.txt:493`) |
| git diff --check | clean (a working-tree check, not a range check — see §6's note) | rc 0 — `DIFF_CHECK_RC=0` (`gates-e4-r2.txt:494`) |
| dirty files | 0 | 0 — `DIRTY_FILES=0` (`gates-e4-r2.txt:495`) |
| G6 verdict | expected CLEAN | **`G6_ASSERT_RC=0`** (`gates-e4-r2.txt:496`) |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e4-r2.txt:24`; `fold-proofs.txt` (final line, `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0`) |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its foreground start
(`gates-e4-r2.txt:31,44,57,70,345,480`; `chain-e4-r2.txt:17`).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e4-r2.txt:499`); `GATES_AGGREGATE rc=0 over
[0 0 0 0 0 0]` (`gates-e4-r2.txt:501`); `CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-e4-r2.txt:27`).
`GATES_E4_R2_RC=0` (`chain-e4-r2.txt:15`).

## No declared-vs-measured mismatch this round

Every value `psym-derivation-e4-r2.txt` declares was measured exactly as declared by this chain,
including the six sub-gates of G6, which is the sub-gate this round exists to re-run. Nothing
above required a re-derivation.

## Copy verification

All 25 files under this directory (plus `replay/`) were produced with `cp` from the sources listed
above and then `cmp`-checked against those sources. Every comparison reported no differences (`cmp`
exit 0, no output) — `chain-e4-r2.txt`, `gates-e4-r2.txt`, `suite-debug.txt`, `suite-release.txt`,
`suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, the ten
`psym-*.txt` files, and the seven `replay/` files (the three `run-*.txt` compared against their
source `run-*.log`, byte-for-byte, only the filename changed).
