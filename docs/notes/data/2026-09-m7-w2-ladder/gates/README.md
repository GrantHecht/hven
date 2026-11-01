# M7 W2 ladder round — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the ladder chain ran, nothing in
it adjusted after): `../wrappers-gates/psym-derivation-ladder.txt`, runner
`../wrappers-gates/{gates-ladder.sh,chain_gates_ladder.sh}` (committed at
`52650ca8`). Base `c1f1e704` (the E2 close ledger; branch `m7`), range
`c1f1e704..0275d16d` for every source/header derivation — `0275d16d` (the
ladder: f2, f1/E5, F4 with the guard); the declaration commit `52650ca8` adds
no source, header or test. Chain: detached, started `CHAIN_START
2026-09-25T04:13:44-04:00` / `GATES_START 2026-09-25T04:13:44-04:00` at HEAD
`52650ca8` (the declaration commit) with 0 dirty files (`chain-ladder.txt:1-2`;
`gates-ladder.txt:1-2,5`), ended `CHAIN_END 2026-09-25T05:13:18-04:00`
(`chain-ladder.txt:26`). Runner transcript: `gates-ladder.txt` (copied verbatim
from `.scratch/m7w2laddergates/logs/gates-ladder.log`). Chain wrapper
transcript: `chain-ladder.txt` (copied verbatim from
`.scratch/m7w2laddergates/logs/chain-ladder.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2laddergates/gates/`:
`suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt`,
`install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, `psym-*.txt`
(from `gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`,
`libobjs-both-{Release,Debug}.txt`, `sections-{Release,Debug}.txt`),
`replay/` (the `run-*.log` files renamed `.txt`, `head-*.csv` kept as-is). The
build trees and object directories (`gates-dbg/`, `gates-rel/`,
`psym-build/`, `psym/{before,after}{,-obj}-*`, `smoke-work/`) and the
build/configure logs are not copied.

Rehearsal and pre-flight transcripts, measured before the declaration, are
already committed alongside it under `../wrappers-gates/` and
`../wrappers-gates/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-ladder.txt` §3) | MEASURED |
|---|---|---|
| tests | 2843 | 2843 — `gates-ladder.txt:39`; `suite-debug.txt:5693` |
| failed | 0 | 0 — `gates-ladder.txt:38`; `suite-debug.txt:5693` |
| by name | derived baseline − 0 REMOVED + 11 ADDED | asserted — `gates-ladder.txt:40` (2845 started names == 2834 − 0 + 11) |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-ladder.txt:53`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-ladder.txt:52`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone, 700.95 s
(`gates-ladder.txt:44-45`; `suite-debug-long.txt:2-3`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2844 | 2844 — `gates-ladder.txt:65`; `suite-release.txt:5695` |
| failed | 0 | 0 — `gates-ladder.txt:64`; `suite-release.txt:5695` |
| by name | derived baseline − 0 REMOVED + 11 ADDED | asserted — `gates-ladder.txt:66` (2846 started names == 2835 − 0 + 11) |

By name against the DERIVED baseline — the E2 fix-round-5 gate transcripts
(`../../2026-09-m7-w2-e2-working-set/gates-e2-fix5/suite-{debug,release}.txt`,
2834/2835 started names; `gates-ladder.txt:18`): eleven ADDED, each by exactly
one TEST line in the range, none REMOVED (declaration §3 lists the eleven by
name). `tests/CMakeLists.txt` adds exactly `sqp/test_sqp_ladder_seams.cpp`
and removes no non-comment line (`gates-ladder.txt:19`).

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences under the 1e-5 residual gate | 27 × 75, 0 differences on walk, ssn and ipm — `gates-ladder.txt:72,81,83` (CMP_RC=0 at `:80,82,84`; asserted `:87-88`); `replay/comparison.txt:1,9,10` |
| within-gate walk residual moves | 7, the known f7 warm residual digits, recorded not asserted | 7, all on f7_n1000_path_warm / f7_n800_path_warm — `gates-ladder.txt:73-79`; `replay/comparison.txt:2-8` |

ssn and ipm show 0 within-gate moves (`gates-ladder.txt:81,83`). No counter,
status or objective column moved on any cell.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 8 Release / 7 Debug | 8 — `gates-ladder.txt:110`, asserted `:117,121` | 7 — `gates-ladder.txt:158`, asserted `:165,169` |
| class A (source changed) | 8 / 7 | 8 — `gates-ladder.txt:111`, asserted `:122` | 7 — `gates-ladder.txt:159`, asserted `:170` |
| class B (includer, source untouched) | 0 / 0 | 0 — `gates-ladder.txt:112`, asserted `:123` | 0 — `gates-ladder.txt:160`, asserted `:171` |
| UNEXPLAINED | 0 | 0 — `gates-ladder.txt:113`, asserted `:119` | 0 — `gates-ladder.txt:161`, asserted `:167` |
| NOISE-ONLY | 44 (2 lib) / 59 (9 lib) | 44 (2 in the library) — `psym-classification-Release.txt:22` | 59 (9 in the library) — `psym-classification-Debug.txt:21` |
| negative control | 107 / 93 | 107 — `gates-ladder.txt:114,120`; `psym-classification-Release.txt:68` | 93 — `gates-ladder.txt:162,168`; `psym-classification-Debug.txt:82` |
| compare rc | 1 | 1 — `gates-ladder.txt:99,115` | 1 — `gates-ladder.txt:147,163` |
| capture pair | after = before = 151 | 151 — `gates-ladder.txt:93-95` | 151 — `gates-ladder.txt:141-143` |
| outside-universe object (`test_sqp_ladder_seams.cpp.o`, ADDED) | built, 0 in the manifest | `gates-ladder.txt:96-97` | `gates-ladder.txt:144-145` |
| ONLY-AFTER | `[]` | `gates-ladder.txt:100` | `gates-ladder.txt:148` |
| library ADDED | none | `[]` — `gates-ladder.txt:124` | `[]` — `gates-ladder.txt:172` |
| class B symmetry | EQUAL (0 both configs) | asserted — `gates-ladder.txt:194` | same assert (cross-config) |
| class A all changed-source | yes | asserted — `gates-ladder.txt:197` | asserted — `gates-ladder.txt:198` |
| class A asymmetry | `test_hs_battery.cpp.o` only (Debug NOISE-ONLY) | admitted by clause (4) — `gates-ladder.txt:199-200` | same |
| class B by name | `[]` | `gates-ladder.txt:204` | `gates-ladder.txt:205` |
| class A by source (git) | the 8 MODIFIED `.cpp` / less `test_hs_battery.cpp` | `gates-ladder.txt:206` | `gates-ladder.txt:207` |
| Debug NOISE-ONLY class A | `tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_hs_battery.cpp.o` | — | `gates-ladder.txt:208`; `psym-compare-Debug.txt:403`; `psym-classification-Debug.txt:57` |

The header derivation (`gates-ladder.txt:102-108,150-156`) accepted exactly
the six declared headers. Class A, Release (8) is the 8 MODIFIED `.cpp`
(`psym-classification-Release.txt:10-18`); Debug (7) is those less
`test_hs_battery.cpp.o` (`psym-classification-Debug.txt:10-17`), whose only
change — re-pinned integers — P-SYM's Debug reading classes as
`__LINE__`-class immediates (declared by name, declaration §2). The ADDED
`tests/sqp/test_sqp_ladder_seams.cpp` lies outside the P-SYM universe
(`hven_sqp_seam_tests`; `gates-ladder.txt:9,11`). Class B is EMPTY in both
configurations; the asymmetry admission list is EMPTY. The mover lists are in
`psym-classification-{Release,Debug}.txt`, the full byte-difference dumps in
`psym-compare-{Release,Debug}.txt`, the object inventories in
`psym-libobjs{,-both}-{Release,Debug}.txt`.

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 4 = {sqp_solver.cpp.o, soc_elastic_restoration.cpp.o} (derived, class A) + {sqp_options.cpp.o, ipqp_engine.cpp.o} (declared NOISE-ONLY) | exactly that, 4 — `gates-ladder.txt:126-129,133,136`, asserted `:137` | exactly that, 4 — `gates-ladder.txt:177,179,181-182,188,191`, asserted `:192` |
| NOISE-ONLY library movers checked | sqp_options.cpp.o, ipqp_engine.cpp.o | `gates-ladder.txt:209-210` | `gates-ladder.txt:211-212` |
| objects compared | 44 | 44 — `gates-ladder.txt:130`; `psym-sections-Release.txt:10` | 44 — `gates-ladder.txt:185`; `psym-sections-Debug.txt:17` |
| byte-identical | 40 Release / 33 Debug | 40 — `gates-ladder.txt:131`; `psym-sections-Release.txt:11` | 33 — `gates-ladder.txt:186`; `psym-sections-Debug.txt:18` |
| DEBUG_ONLY | Release 0 / Debug 7 | 0 — `gates-ladder.txt:132`, asserted `:138` | 7 (`ledger.cpp.o`, `console_trace_sink.cpp.o`, `ipm_solver.cpp.o`, `sqp_print.cpp.o`, `trace_writer.cpp.o`, `qp_engine.cpp.o`, `continuation.cpp.o`) — `gates-ladder.txt:174-176,178,180,183-184,187`, asserted `:193` |
| REFUSED | 0 | 0 — `gates-ladder.txt:134` | 0 — `gates-ladder.txt:189` |

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-ladder.txt:216,218`; `install-smoke.txt:1192` |
| export contract | OK, align 32 | OK, align 32 — `gates-ladder.txt:221,223,226`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp (incl. tests) | rc 0 over the derived files — `gates-ladder.txt:213,228` |
| git diff --check | clean | rc 0 — `gates-ladder.txt:229` |
| dirty files | 0 | 0 — `gates-ladder.txt:230` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-ladder.txt:24`; `fold-proofs.txt:99` |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its
foreground start (`gates-ladder.txt:28-29,41-42,54-55,67-68,89-90,214-215`;
`chain-ladder.txt:16-17`).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-ladder.txt:234`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-ladder.txt:236`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-ladder.txt:27`).

No DECLARED FINDING this round: every declared value in
`psym-derivation-ladder.txt` (§§1-6) matched the measured chain exactly, on
the first leg; there is no superseded prior chain for the ladder round.
