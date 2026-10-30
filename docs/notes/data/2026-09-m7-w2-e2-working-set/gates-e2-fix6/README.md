# M7 W2 E2 fix round 6 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the fix6 chain ran, nothing in
it adjusted after): `../wrappers-gates-fix6/psym-derivation-e2fix6.txt`, plus
`../wrappers-gates-fix6/preflight.txt` and `../wrappers-gates-fix6/rehearsal/`
(committed at `63c687f7`). Base `e07f8173` (the previous round's gates
declaration commit), range `e07f8173..a53dbb88` for every source/header
derivation (`a53dbb88` is fix6: R-E, the converged grey band re-read to its
end, with the rounding-floor guard; R-D retired; ACOPR57 qp4 closes) — the
declaration commit `63c687f7` adds no source, header or test. Chain: HEAD at
start `63c687f7`, 0 dirty files (`chain-e2fix6.txt:2`), started
`CHAIN_START 2026-09-25T07:42:02-04:00` / `GATES_START
2026-09-25T07:42:02-04:00` (`chain-e2fix6.txt:1`; `gates-e2fix6.txt:1`), ended
`CHAIN_END 2026-09-25T08:50:56-04:00` (`chain-e2fix6.txt:26`). Runner
transcript: `gates-e2fix6.txt` (copied verbatim from
`.scratch/m7w2e2fix6gates/logs/gates-e2fix6.log`). Chain wrapper transcript:
`chain-e2fix6.txt` (copied verbatim from
`.scratch/m7w2e2fix6gates/logs/chain-e2fix6.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2e2fix6gates/gates/`:
`suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt`,
`install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, `psym-*.txt`
(from `gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`,
`libobjs-both-{Release,Debug}.txt`, `sections-{Release,Debug}.txt`),
`replay/` (the `run-*.log` files renamed `.txt`, `head-*.csv` and
`comparison.txt` kept as-is). The build trees and object directories
(`gates-dbg/`, `gates-rel/`, `psym-build/`, `psym/{before,after}{,-obj}-*`,
`smoke-work/`) and the build/configure logs are not copied.

Rehearsal, pre-flight and offline transcripts, measured before the
declaration and already committed alongside it under
`../wrappers-gates-fix6/` and `../wrappers-gates-fix6/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README (all 25 copies are verbatim `cp`;
`cmp` against each source is clean — see **Copy verification** below).

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e2fix6.txt` §3) | MEASURED |
|---|---|---|
| tests | 2847 | 2847 — `gates-e2fix6.txt:39`; `suite-debug.txt:5701` |
| failed | 0 | 0 — `gates-e2fix6.txt:38`; `suite-debug.txt:5701` |
| by name | baseline 2845 started names − 1 REMOVED + 5 ADDED | asserted — `gates-e2fix6.txt:40` (2849 started names == 2845 − 1 + 5) |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e2fix6.txt:53`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e2fix6.txt:52`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone
(`gates-e2fix6.txt:44-45`; `suite-debug-long.txt:2-3`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2848 | 2848 — `gates-e2fix6.txt:65`; `suite-release.txt:5703` |
| failed | 0 | 0 — `gates-e2fix6.txt:64`; `suite-release.txt:5703` |
| by name | baseline 2846 started names − 1 REMOVED + 5 ADDED | asserted — `gates-e2fix6.txt:66` (2850 started names == 2846 − 1 + 5) |

By name against the derived baseline (the previous round's gate record,
`docs/notes/data/2026-09-m7-w2-ladder/gates/suite-{debug,release}.txt`,
2845/2846 started names, ran at `52650ca8`; no test source differs between
`52650ca8` and `e07f8173`, checked in the pre-flight — `gates-e2fix6.txt:18`):
five renames/additions, one removal, net +4 per config —

- REMOVED `QpEngine.ObjectiveInflatedMultiplierHidesASmallContradiction` (the rename);
- ADDED `QpE2WorkingSet.AConvergedGreyPinOnDallasmIsReReadDependent`,
  `QpE2WorkingSet.AGreyPinOnYorknetThatIsIndependentStaysIndependent`,
  `QpE2WorkingSet.AGreyReReadSolvesTheAcopr57RestorationQp`,
  `QpEngine.ObjectiveInflatedMultiplierContradictionIsCertifiedOnTheIndependentWorkingSet`,
  `WsIndependenceCalibration.AGreyReReadNeverFlipsACandidateAboveKDepTol`.

Three ADDED non-`.cpp` test files (`tests/qp/fixtures/e2-d4/{ACOPR57_def-qp4,
DALLASM_def-qp0, YORKNET_def-qp1}.txt`); `tests/CMakeLists.txt` unchanged
(`gates-e2fix6.txt:19`).

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences under the 1e-5 residual gate | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e2fix6.txt:72,81,83` (CMP_RC=0 at `:80,82,84`; asserted `:87-88`); `replay/comparison.txt:1,9,10` |
| within-gate walk residual moves | 7, on f7_n800 / f7_n1000_path_warm, recorded not asserted | 7, all on those two cells — `gates-e2fix6.txt:73-79`; `replay/comparison.txt:2-8` |

ssn and ipm show 0 within-gate moves (`gates-e2fix6.txt:81,83`;
`replay/comparison.txt:9,10`). No counter, status or objective column moved
on any cell. §5 of the derivation records this same result confirmed
offline first, against both the fix6 scratch build and exactly against
`52650ca8`'s head CSVs, before the chain ran.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 40 = 6 A + 34 B | 40 — `gates-e2fix6.txt:110`, asserted `:117,121` | 39 = 5 A + 34 B — `gates-e2fix6.txt:160`, asserted `:167,171` |
| class A (source changed) | Release 6 / Debug 5 | 6 — `gates-e2fix6.txt:111`, asserted `:122` | 5 — `gates-e2fix6.txt:161`, asserted `:172` |
| class B (includer, source untouched) | 34 | 34 — `gates-e2fix6.txt:112`, asserted `:123` | 34 — `gates-e2fix6.txt:162`, asserted `:173` |
| UNEXPLAINED | 0 | 0 — `gates-e2fix6.txt:113`, asserted `:119` | 0 — `gates-e2fix6.txt:163`, asserted `:169` |
| NOISE-ONLY | 2 Release (0 lib) / 37 Debug (6 lib, `test_continuation.cpp.o`) | 2 (0 in the library) — `psym-classification-Release.txt:54` | 37 (6 in the library, all explained) — `psym-classification-Debug.txt:53` |
| negative control | 104 / 70 | 104 — `gates-e2fix6.txt:114,120`; `psym-classification-Release.txt:58` | 70 — `gates-e2fix6.txt:164,170`; `psym-classification-Debug.txt:92` |
| compare rc | 1 | 1 — `gates-e2fix6.txt:99,115` | 1 — `gates-e2fix6.txt:149,165` |
| capture pair | after = before = 151 | 151 — `gates-e2fix6.txt:93-95` | 151 — `gates-e2fix6.txt:143-145` |
| outside-universe object (`test_qp_anti_cycling_arm.cpp.o`) | built, 0 in the manifest | `gates-e2fix6.txt:96-97` | `gates-e2fix6.txt:146-147` |
| ONLY-AFTER | `[]` | `gates-e2fix6.txt:100` | `gates-e2fix6.txt:150` |
| library ADDED | none | `[]` — `gates-e2fix6.txt:124` | `[]` — `gates-e2fix6.txt:174` |
| class B symmetry | EQUAL (34 both configs) | asserted — `gates-e2fix6.txt:197` ("class B sets equal (34 object(s))") | same assert (cross-config) |
| class A cross-config | Release 6 / Debug 5, `test_continuation.cpp.o` NOISE-ONLY in Debug only | asserted — `gates-e2fix6.txt:200-202` | same |
| class B by name | matches the declared 34-name list | `gates-e2fix6.txt:207` | `gates-e2fix6.txt:208` |
| class A by source | matches the declared source list | `gates-e2fix6.txt:209` (6 files) | `gates-e2fix6.txt:210` (5 files, `test_continuation.cpp` excluded) |

The header derivation (`gates-e2fix6.txt:102-108,152-158`) accepted exactly
the three declared headers (`solver_counters.h`, `qp_engine.h`,
`ws_independence.h`), identically in both configurations. Class A (6
Release / 5 Debug) is the 7 in-universe MODIFIED `.cpp` less the one outside
the P-SYM universe (`tests/qp/test_qp_anti_cycling_arm.cpp`, built into
`hven_qp_walk_seam_tests`; `gates-e2fix6.txt:8,10`), less
`tests/sqp/test_continuation.cpp` in Debug only (NOISE-ONLY there — declared
by name, checked MODIFIED in source and re-pinned integers/comments only):
`src/qp/qp_engine.cpp`, `src/qp/ws_independence.cpp`,
`tests/sqp/test_continuation.cpp` (Release only),
`tests/sqp/test_qp_engine.cpp`, `tests/sqp/test_qp_ws_independence.cpp`,
`tests/sqp/test_scale_problems.cpp` (`gates-e2fix6.txt:209-211`). Class B is
symmetric — Release and Debug sets equal, 34 objects, declared by name in the
runner (`gates-e2fix6.txt:17`). The library objects among them are
`src/core/ledger.cpp.o`, `src/drivers/sqp_solver.cpp.o`,
`src/globalization/sqp/soc_elastic_restoration.cpp.o`,
`src/qp/ipqp_engine.cpp.o` (`psym-classification-Release.txt:18-21`). The
asymmetry admission list is EMPTY this round. The mover lists are in
`psym-classification-{Release,Debug}.txt`, the full byte-difference dumps in
`psym-compare-{Release,Debug}.txt`, the object inventories in
`psym-libobjs{,-both}-{Release,Debug}.txt`.

**Unexplained movers: 0 in both configurations** (measured and asserted,
cited above — no discrepancy from the declared 0).

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 6 = {qp_engine.cpp.o, ws_independence.cpp.o} (derived, class A) + {ledger.cpp.o, sqp_solver.cpp.o, soc_elastic_restoration.cpp.o, ipqp_engine.cpp.o} (declared, class B) | exactly that, 6 — `gates-e2fix6.txt:126-131,138`, asserted `:139` | exactly that, 6 — `gates-e2fix6.txt:176,180,182-184,186,194`, asserted `:195` |
| objects compared | 44 | 44 — `gates-e2fix6.txt:132`; `psym-sections-Release.txt:11` | 44 — `gates-e2fix6.txt:188`; `psym-sections-Debug.txt:17` |
| byte-identical | 38 Release / 32 Debug | 38 — `gates-e2fix6.txt:133` | 32 — `gates-e2fix6.txt:189` |
| DEBUG_ONLY | Release 0 / Debug 6 | 0 — `gates-e2fix6.txt:134`, asserted `:140` | 6 (`console_trace_sink.cpp.o`, `ipm_solver.cpp.o`, `sqp_print.cpp.o`, `trace_writer.cpp.o`, `ssn_engine.cpp.o`, `continuation.cpp.o`) — `gates-e2fix6.txt:177-179,181,185,187`, asserted `:196` |
| REFUSED | 0 | 0 — `gates-e2fix6.txt:136` | 0 — `gates-e2fix6.txt:192` |

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e2fix6.txt:215,217`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e2fix6.txt:220,222,225`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp (incl. tests/*.h) | rc 0 over the derived files — `gates-e2fix6.txt:212,227` |
| git diff --check | clean | rc 0 — `gates-e2fix6.txt:228` |
| dirty files | 0 | 0 — `gates-e2fix6.txt:229` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e2fix6.txt:24`; `fold-proofs.txt` (final line, `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0`) |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its
foreground start (`gates-e2fix6.txt:29,42,55,68,90,214`;
`chain-e2fix6.txt:17`).

## ASSERT OK count

57 `ASSERT OK` lines in `gates-e2fix6.txt`, 0 assert failures (every
`^ASSERT ` line in the transcript reads `ASSERT OK`; no `ASSERT FAIL` or
refusal appears outside the pre-flight's declared negative-control refusals,
which live in `../wrappers-gates-fix6/preflight.txt`, not in this chain).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e2fix6.txt:233`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e2fix6.txt:235`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-e2fix6.txt:27`).

No DECLARED FINDING this round: every declared value in
`psym-derivation-e2fix6.txt` (§§1-8) matched the measured chain exactly, on
the first leg; there is no superseded prior chain for fix round 6 (the
rehearsal's hypothesis refusal, recorded in
`../wrappers-gates-fix6/rehearsal/`, predates the declaration and is not a
chain leg).

## Copy verification

All 25 files under this directory (plus `replay/`) were produced with `cp`
from the sources listed above and then `cmp`-checked against those sources.
Every comparison reported no differences (`cmp` exit 0, no output) —
`chain-e2fix6.txt`, `gates-e2fix6.txt`, `suite-debug.txt`,
`suite-release.txt`, `suite-debug-long.txt`, `install-smoke.txt`,
`export-contract.txt`, `fold-proofs.txt`, the ten `psym-*.txt` files, and the
seven `replay/` files (the three `run-*.txt` compared against their source
`run-*.log`, byte-for-byte, only the filename changed).
