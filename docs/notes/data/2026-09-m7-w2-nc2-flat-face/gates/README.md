# M7 W2 NC2 (the flat face) + fix round 1 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the chain was detached, nothing in it adjusted
after): `../wrappers-gates/psym-derivation-nc2.txt`, plus `../wrappers-gates/preflight.txt` and
`../wrappers-gates/rehearsal/`. **One declaration covers two commits** (settler ruling M16): base
`828dc917` (the NC1 ledger docs record; its sources are NC1's gated sources `5732a1f3`, checked —
no file under `include/`, `src/` or `tests/` differs), range `828dc917..0fa59cdc` for every
source/header derivation (`71f54c50` = NC2, the flat face; `0fa59cdc` = NC2 fix round 1, the
cherry-pick of `7b36a01e`). Declaration commit `52b16a8e` adds no source, header or test
(docs/notes only). Chain: HEAD at start `52b16a8e`, 0 dirty files (`chain-nc2.txt:2`; `dirty=0` in
`gates-nc2.txt:2`), started `GATES_START 2026-09-26T14:57:27-04:00` (`gates-nc2.txt:1`), ended
`CHAIN_END 2026-09-26T15:58:18-04:00` (`chain-nc2.txt:26`). Runner transcript: `gates-nc2.txt`
(copied verbatim from `.scratch/m7w2nc2gates/logs/gates-nc2.log`). Chain wrapper transcript:
`chain-nc2.txt` (copied verbatim from `.scratch/m7w2nc2gates/logs/chain-nc2.log`). Full test and
build transcripts, copied verbatim from `.scratch/m7w2nc2gates/gates/`: `suite-debug.txt`,
`suite-release.txt`, `suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`,
`fold-proofs.txt`, `psym-*.txt` (from `gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`, `libobjs-both-{Release,Debug}.txt`,
`sections-{Release,Debug}.txt`), `replay/` (the `run-*.log` files renamed `.txt`, `head-*.csv` and
`comparison.txt` kept as-is). The build trees and object directories and the build/configure logs
are not copied. All 25 files under this directory (plus `replay/`) were produced with `cp` from
those sources and `cmp`-checked clean against them (0 output, rc 0 on every file).

Rehearsal, pre-flight and offline transcripts, measured before the declaration and already
committed alongside it under `../wrappers-gates/` and `../wrappers-gates/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the raw transcript copied
alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-nc2.txt` §3) | MEASURED |
|---|---|---|
| tests | 2861 | 2861 — `gates-nc2.txt:39`; `suite-debug.txt:5729` |
| failed | 0 | 0 — `gates-nc2.txt:38`; `suite-debug.txt:5729` |
| by name | baseline 2855 started names − 0 REMOVED + 8 ADDED | asserted — `gates-nc2.txt:40` (2863 started names == 2855 − 0 + 8) |

One of the eight ADDED instances, `QpNc2FlatFace.TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeBorder`,
is a declared `GTEST_SKIP` in Debug (745 s against M6's 600 s budget, M25) — ctest keeps a Skipped
test inside its `out of N` denominator and off the failure count, and it still gets a `Start` line,
so it moves neither the total nor the by-name count (`gates-nc2.txt:32`; `suite-debug.txt:5734`).

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-nc2.txt:53`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-nc2.txt:52`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone (`gates-nc2.txt:44-45`;
`suite-debug-long.txt:2-3`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2862 | 2862 — `gates-nc2.txt:65`; `suite-release.txt:5731` |
| failed | 0 | 0 — `gates-nc2.txt:64`; `suite-release.txt:5731` |
| by name | baseline 2856 started names − 0 REMOVED + 8 ADDED | asserted — `gates-nc2.txt:66` (2864 started names == 2856 − 0 + 8) |

By name against the derived baseline (the previous round's gate record,
`docs/notes/data/2026-09-m7-w2-nc1-declined-ride/gates/suite-{debug,release}.txt`, 2855/2856
started names, ran at `9d0fb512`; no test file differs between that head and BEFORE, checked in
the pre-flight): eight additions, no removals, net +8 per config —

- ADDED `QpNc2FlatFace.ANoiseLevelDropIsUndoneAndCountedNotRidden`,
  `QpNc2FlatFace.AWrongSignedDropOnAnLpFaceRidesTheRayToItsBlocker`,
  `QpNc2FlatFace.TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeBorder`,
  `QpNc2FlatFace.TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeRefactorize`,
  `QpNc2FlatFace.TheSteenbrbRestorationLpQp3NeverAscendsTheSlopeBorder`,
  `QpNc2FlatFace.TheSteenbrbRestorationLpQp3NeverAscendsTheSlopeRefactorize`,
  `QpNc2FlatFace.TheSteenbrbRestorationLpQp4NeverAscendsTheSlopeBorder`,
  `QpNc2FlatFace.TheSteenbrbRestorationLpQp4NeverAscendsTheSlopeRefactorize`
  — six of these eight names were renamed by fix round 1 (from
  `...TakesTheMultipliersSign{Border,Refactorize}`), so the declaration re-derived the by-name set
  fresh on the final pair rather than carrying NC2-only draft numbers;
- REMOVED (none).

One ADDED non-.cpp test file family, the three R2 dumps (`tests/qp/fixtures/e2-d4/
R2_Cs_STEENBRB_qp{2,3,4}.txt`); `tests/CMakeLists.txt` adds exactly one non-comment line
(`qp/test_qp_flat_face_saddle.cpp`) and removes none (`gates-nc2.txt:19`). One source ADDED
OUTSIDE the P-SYM universe, `tests/qp/test_qp_flat_face_saddle.cpp` (an `hven_qp_walk_seam_tests`
recompile target, not a `PSYM_GROUPS` entry) — `gates-nc2.txt:9,11`.

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences under the 1e-5 residual gate | 27 × 75, 0 differences on walk, ssn and ipm — `gates-nc2.txt:72,81,83` (CMP_RC=0 at `:80,82,84`); `replay/comparison.txt:1,9,10` |
| within-gate walk residual moves | 7, on f7_n800_path_warm / f7_n1000_path_warm, recorded not asserted | 7, all on those two cells — `gates-nc2.txt:73-79`; `replay/comparison.txt:2-8` |

ssn and ipm show 0 within-gate moves (`gates-nc2.txt:81,83`; `replay/comparison.txt:9,10`). No
counter, status or objective column moved on any cell. §5 of the derivation records this same
result confirmed offline first, against both the M6 close pin and NC1's gate record's head CSVs,
before the chain ran.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 38 = 1 A + 37 B | 38 — `gates-nc2.txt:110`, asserted `:117-118` | 38 — `gates-nc2.txt:159`, asserted `:166-167` |
| class A (source changed) | 1 | 1 — `gates-nc2.txt:111`, asserted `:122` | 1 — `gates-nc2.txt:160`, asserted `:171` |
| class B (includer, source untouched) | 37 | 37 — `gates-nc2.txt:112`, asserted `:123` | 37 — `gates-nc2.txt:161`, asserted `:172` |
| UNEXPLAINED | 0 | 0 — `gates-nc2.txt:113`, asserted `:119` | 0 — `gates-nc2.txt:162`, asserted `:168` |
| negative control | **declared (rehearsal) 107** | **measured 106 — informational, asserted ≥ 1 (`gates-nc2.txt:114,120`); explained: the rehearsal sample (`rehearsal-classification-Release.txt:56`) included the round's one ADDED outside-universe object, `test_qp_flat_face_saddle.cpp.o`, which does not exist at BEFORE and so cannot be a negative control; the chain's 106 is the right count. Registered (M27): the control universe should exclude objects ADDED in the range.** | 71 — `gates-nc2.txt:163,169` (declared 71, matches — no ADDED-object contamination on this side) |
| compare rc | 1 | 1 — `gates-nc2.txt:99,115` | 1 — `gates-nc2.txt:148,164` |
| capture pair | after = before = 151 | 151 — `gates-nc2.txt:93-95` | 151 — `gates-nc2.txt:142-144` |
| outside-universe object (`test_qp_flat_face_saddle.cpp.o`) | built, 0 in the manifest | `gates-nc2.txt:96-97` | `gates-nc2.txt:145-146` |
| ONLY-AFTER | `[]` | `gates-nc2.txt:100` | `gates-nc2.txt:149` |
| library ADDED | none | `[]` — `gates-nc2.txt:124` | `[]` — `gates-nc2.txt:173` |
| class B symmetry | EQUAL (37 both configs) | asserted — `gates-nc2.txt:195` ("class B sets equal (37 object(s))") | same assert (cross-config) |
| class A cross-config | Release 1 / Debug 1, identical sets | asserted — `gates-nc2.txt:198-200` | same |
| class B by name | matches the declared 37-name list | `gates-nc2.txt:204` | `gates-nc2.txt:205` |
| class A by source | matches the declared source list | `gates-nc2.txt:206` (`src/qp/qp_engine.cpp`) | `gates-nc2.txt:207` (`src/qp/qp_engine.cpp`) |

The header derivation (`gates-nc2.txt:102-108,151-157`) accepted exactly the two declared headers
(`solver_counters.h`, `qp_engine.h`), identically in both configurations. Class A (1, both
configurations) is the one in-universe MODIFIED `.cpp` — `src/qp/qp_engine.cpp` (the flat-face
rule, as fix round 1 left it). Class B is symmetric — Release and Debug sets equal, 37 objects,
declared by name in the runner. The library objects among them are `src/core/ledger.cpp.o`,
`src/drivers/sqp_solver.cpp.o`, `src/globalization/sqp/soc_elastic_restoration.cpp.o`,
`src/qp/ipqp_engine.cpp.o` (`psym-classification-Release.txt:14-17`). The cause for all 37: each
holds, copies or destroys a `QpCounters` by value, and `QpCounters` grew by two `Index` fields
(`ride_predicted_signs`, `eq_drift_violations`) — sizes, member offsets, copy/move/zeroing and the
inlined destructors move. Against NC1's 36 (compared, not carried): +1,
`tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_hs_sweeps.cpp.o`, class A in NC1 (that range re-pinned
it) and class B here (this range does not touch it); nothing else differs.

**Unexplained movers: 0 in both configurations** (measured and asserted, cited above — no
discrepancy from the declared 0).

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 5 = {qp_engine.cpp.o} (derived, class A) + {ledger.cpp.o, sqp_solver.cpp.o, soc_elastic_restoration.cpp.o, ipqp_engine.cpp.o} (declared, class B) | exactly that, 5 — `gates-nc2.txt:126-130,137`, asserted `:138` | exactly that, 5 — `gates-nc2.txt:175,179,181,183,192`, asserted `:193` |
| objects compared | 44 | 44 — `gates-nc2.txt:131`; `psym-sections-Release.txt` | 44 — `gates-nc2.txt:186`; `psym-sections-Debug.txt` |
| byte-identical | 39 Release / 33 Debug | 39 — `gates-nc2.txt:132` | 33 — `gates-nc2.txt:187` |
| DEBUG_ONLY | Release 0 / Debug 6 | 0 — `gates-nc2.txt:133`, asserted `:139` | 6 (`console_trace_sink.cpp.o`, `ipm_solver.cpp.o`, `sqp_print.cpp.o`, `trace_writer.cpp.o`, `ssn_engine.cpp.o`, `continuation.cpp.o`) — `gates-nc2.txt:176-178,180,184-185`, asserted `:194` |
| REFUSED | 0 | 0 — `gates-nc2.txt:135` | 0 — `gates-nc2.txt:190` |

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-nc2.txt:211,213`; `install-smoke.txt:1192` |
| export contract | OK, align 32 | OK, align 32 — `gates-nc2.txt:216,218,221`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp (4 files) | rc 0 over the derived files — `gates-nc2.txt:208,223` |
| git diff --check | clean | rc 0 — `gates-nc2.txt:224` |
| dirty files | 0 | 0 — `gates-nc2.txt:225` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-nc2.txt:24`; `fold-proofs.txt` (final line, `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0`) |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its foreground start
(`gates-nc2.txt:29,42,55,68,90,210`; `chain-nc2.txt:17`).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-nc2.txt:229`); `GATES_AGGREGATE rc=0 over
[0 0 0 0 0 0]` (`gates-nc2.txt:231`); `CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-nc2.txt:27`).

## The one declared-vs-measured difference this round, and why it is not a finding

The P-SYM Release negative-control count is the only value in `psym-derivation-nc2.txt` that does
not match its own chain measurement: the declaration (backed by the pre-chain rehearsal's own
classification of `0fa59cdc`, `rehearsal-classification-Release.txt:56`) states 107; the gate
chain, run at HEAD `52b16a8e` (a docs-only commit — no source, header or test differs from the
rehearsal's `0fa59cdc`), measured 106 (`gates-nc2.txt:114,120`; `psym-classification-Release.txt:56`).
The runner asserts this value only as `>= 1` (`gates-nc2.txt:120`, "(>= 1)"), never against the
declared figure, so `GATES_NC2_RC` and `G5_ASSERT_RC` are unaffected (both 0). Settler ruling M27
(`.superpowers/m7-w2-nc2-brief.md` §7): the negative control is informational, NC1 recorded the
same way (106, also asserted only `>= 1`), and the 107 → 106 gap is explained by the one object the
rehearsal's printed sample differed on — `test_qp_flat_face_saddle.cpp.o`, the round's one ADDED,
outside-the-universe source. That object does not exist at BEFORE, so it cannot be a
before/after-identical "negative control" by the classifier's own definition; the rehearsal's
inclusion of it was the error, and the chain's 106 is the correct count. Registered per M27: a
future revision of the classifier should exclude objects ADDED in the range from the negative-
control universe by construction, rather than relying on this per-round explanation. No other
declared value in `psym-derivation-nc2.txt` differs from what the chain measured.

## Copy verification

All 25 files under this directory (plus `replay/`) were produced with `cp` from the sources listed
above and then `cmp`-checked against those sources. Every comparison reported no differences (`cmp`
exit 0, no output) — `chain-nc2.txt`, `gates-nc2.txt`, `suite-debug.txt`, `suite-release.txt`,
`suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, the ten
`psym-*.txt` files, and the seven `replay/` files (the three `run-*.txt` compared against their
source `run-*.log`, byte-for-byte, only the filename changed).
