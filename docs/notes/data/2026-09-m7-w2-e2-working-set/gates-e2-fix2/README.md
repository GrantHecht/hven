# M7 W2 E2 fix round 2 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the fix2 chain ran, nothing in
it adjusted after): `../wrappers-gates-fix2/psym-derivation-e2fix2.txt`,
runner `../wrappers-gates-fix2/{gates-e2fix2.sh,chain_gates_e2fix2.sh}`
(committed at `7a28bd5c`). Base `763cda00` (the E1b close ledger; branch
`m7`), range `763cda00..d1edc391` for every source/header derivation —
`d1edc391` (fix2: S23, S24, S25 + S25'); the declaration commit `7a28bd5c`
adds no source, header or test. Chain: detached, started
`CHAIN_START 2026-09-24T10:39:53-04:00` / `GATES_START
2026-09-24T10:39:53-04:00` at HEAD `7a28bd5c` (the declaration commit) with 0
dirty files (`chain-e2fix2.txt:1-2`; `gates-e2fix2.txt:1-2,5`), ended
`CHAIN_END 2026-09-24T11:42:10-04:00` (`chain-e2fix2.txt:26`). Runner
transcript: `gates-e2fix2.txt` (copied verbatim from
`.scratch/m7w2e2fix2gates/logs/gates-e2fix2.log`). Chain wrapper transcript:
`chain-e2fix2.txt` (copied verbatim from
`.scratch/m7w2e2fix2gates/logs/chain-e2fix2.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2e2fix2gates/gates/`:
`suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt`,
`install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, `psym-*.txt`
(from `gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`,
`libobjs-both-{Release,Debug}.txt`, `sections-{Release,Debug}.txt`),
`replay/` (the `run-*.log` files renamed `.txt`, `head-*.csv` kept as-is). The
build trees and object directories (`gates-dbg/`, `gates-rel/`,
`psym-build/`, `psym/{before,after}{,-obj}-*`, `smoke-work/`) and the
build/configure logs are not copied.

Rehearsal, pre-flight and offline-G4 transcripts, measured before the
declaration and already committed alongside it under `../wrappers-gates-fix2/`
and `../wrappers-gates-fix2/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e2fix2.txt` §3) | MEASURED |
|---|---|---|
| tests | 2826 | 2826 — `gates-e2fix2.txt:37`; `suite-debug.txt:5659` |
| failed | 0 | 0 — `gates-e2fix2.txt:36`; `suite-debug.txt:5659` |
| by name | baseline + 9 ADDED, none removed | asserted — `gates-e2fix2.txt:38` (2828 started names == 2819 + 9) |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e2fix2.txt:51`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e2fix2.txt:50`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone
(`gates-e2fix2.txt:42-43`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2827 | 2827 — `gates-e2fix2.txt:63`; `suite-release.txt:5661` |
| failed | 0 | 0 — `gates-e2fix2.txt:62`; `suite-release.txt:5661` |
| by name | baseline + 9 ADDED, none removed | asserted — `gates-e2fix2.txt:64` (2829 started names == 2820 + 9) |

By name against the E1b fix-round-3 gate transcripts
(`../../2026-09-m7-w2-e1-equality-basis/gates-e1b-fix3/suite-{debug,release}.txt`,
2817/2818; `gates-e2fix2.txt:17`): nine new instances, none removed — S23/S24
`WsIndependenceCalibration.{RefinedStatisticSeparatesAtARowRatioOf1e10BothAlgebras,
AStalledRefinementAboveTheUndecidableBandIsAdmittedAndFlagged,
AConvergedRhoBetweenKDepTolAndKDepUndecidableIsIndependent}`,
`BothAlgebras/WsIndependence.{AStalledTestAboveTheUndecidableBandIsAdmittedAndCounted,
TheUncertifiedFallbackStillEndsAtAFirstOrderPoint}/{0,1}`; S25
`QpE2WorkingSet.ATolerancedCertificateOnAnUnboundedCoordinateEndsNumericalErrorInsideTheBox`;
S25' `QpE2WorkingSet.AnExactlyDependentBlockerOnFreeVariablesIsCertifiedInfeasibleAfterRefinement`
(declaration §3).

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences under the 1e-5 residual gate | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e2fix2.txt:70,79,81` (CMP_RC=0 at `:78,80,82`; asserted `:85-86`); `replay/comparison.txt:1,9,10` |
| within-gate walk residual moves | 7, on f7_n800 / f7_n1000_path_warm, recorded not asserted | 7, all on those two cells — `gates-e2fix2.txt:71-77`; `replay/comparison.txt:2-8` |

ssn and ipm show 0 within-gate moves (`gates-e2fix2.txt:79,81`). The seven
moves are the residual-class last digits fix round 1b recorded; no counter,
status or objective column moved on any cell.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 39 = 3 A + 36 B | 39 — `gates-e2fix2.txt:108`, asserted `:115,119` | 39 — `gates-e2fix2.txt:158`, asserted `:165,169` |
| class A (source changed) | 3 | 3 — `gates-e2fix2.txt:109`, asserted `:120` | 3 — `gates-e2fix2.txt:159`, asserted `:170` |
| class B (includer, source untouched) | 36 | 36 — `gates-e2fix2.txt:110`, asserted `:121` | 36 — `gates-e2fix2.txt:160`, asserted `:171` |
| UNEXPLAINED | 0 | 0 — `gates-e2fix2.txt:111`, asserted `:117` | 0 — `gates-e2fix2.txt:161`, asserted `:167` |
| NOISE-ONLY | 2 Release (0 lib) / 37 Debug (6 lib) | 2 (0 in the library) — `psym-classification-Release.txt:53` | 37 (6 in the library, all explained) — `psym-classification-Debug.txt:53` |
| negative control | 104 / 69 | 104 — `gates-e2fix2.txt:112,118`; `psym-classification-Release.txt:57` | 69 — `gates-e2fix2.txt:162,168`; `psym-classification-Debug.txt:92` |
| compare rc | 1 | 1 — `gates-e2fix2.txt:97,113` | 1 — `gates-e2fix2.txt:147,163` |
| capture pair | after = before = 151 | 151 — `gates-e2fix2.txt:91-93` | 151 — `gates-e2fix2.txt:141-143` |
| outside-universe seam object | built, 0 in the manifest | `gates-e2fix2.txt:94-95` | `gates-e2fix2.txt:144-145` |
| ONLY-AFTER | `[]` | `gates-e2fix2.txt:98` | `gates-e2fix2.txt:148` |
| library ADDED | none | `[]` — `gates-e2fix2.txt:122` | `[]` — `gates-e2fix2.txt:172` |
| class B symmetry | EQUAL (36 both configs) | asserted — `gates-e2fix2.txt:195` ("class B sets equal (36 object(s))") | same assert (cross-config) |
| class A cross-config identical | yes | asserted — `gates-e2fix2.txt:198-200` | same |
| class B by name | matches the declared 36-name list | `gates-e2fix2.txt:204` | `gates-e2fix2.txt:205` |

The header derivation (`gates-e2fix2.txt:100-106,150-156`) accepted exactly
the four declared headers (`solver_counters.h`, `qp_engine.h`, `qp_problem.h`,
`ws_independence.h`). Class A (3, identical in both configurations) is the 4
MODIFIED `.cpp` less the 1 outside the P-SYM universe
(`tests/qp/test_qp_anti_cycling_arm.cpp`, built under
`hven_qp_walk_seam_tests`, an `HVEN_TESTING` recompile target):
`CMakeFiles/hven.dir/src/qp/{qp_engine,ws_independence}.cpp.o`,
`tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_ws_independence.cpp.o`.
Class B is symmetric — Release and Debug sets equal, 36 objects, declared by
name in the runner (`gates-e2fix2.txt:16`); the library objects among them
are `src/core/ledger.cpp.o`, `src/drivers/sqp_solver.cpp.o`,
`src/globalization/sqp/soc_elastic_restoration.cpp.o`,
`src/qp/ipqp_engine.cpp.o`. The asymmetry admission list is EMPTY this round.
The mover lists are in `psym-classification-{Release,Debug}.txt`, the full
byte-difference dumps in `psym-compare-{Release,Debug}.txt`, the object
inventories in `psym-libobjs{,-both}-{Release,Debug}.txt`.

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 6 = {qp_engine.cpp.o, ws_independence.cpp.o} (derived) + {ledger.cpp.o, sqp_solver.cpp.o, soc_elastic_restoration.cpp.o, ipqp_engine.cpp.o} (declared) | exactly that, 6 — `gates-e2fix2.txt:124-129,133`, asserted `:137` | exactly that, 6 — `gates-e2fix2.txt:174,178,180-182,184,189`, asserted `:193` |
| objects compared | 44 | 44 — `gates-e2fix2.txt:130`; `psym-sections-Release.txt` | 44 — `gates-e2fix2.txt:186`; `psym-sections-Debug.txt` |
| byte-identical | — | 38 — `gates-e2fix2.txt:131` | 32 — `gates-e2fix2.txt:187` |
| DEBUG_ONLY | Release 0 / Debug 6 | 0 — `gates-e2fix2.txt:132,138` | 6 (`console_trace_sink.cpp.o`, `ipm_solver.cpp.o`, `sqp_print.cpp.o`, `trace_writer.cpp.o`, `ssn_engine.cpp.o`, `continuation.cpp.o`) — `gates-e2fix2.txt:175-177,179,183,185,188`, asserted `:194` |
| REFUSED | 0 | 0 — `gates-e2fix2.txt:134` | 0 — `gates-e2fix2.txt:190` |

`qp_engine.cpp.o`'s Debug section count moved 9071 → 9167
(`gates-e2fix2.txt:182`; `psym-sections-Debug.txt:14`); `ws_independence.cpp.o`'s
Debug section list moved (`gates-e2fix2.txt:184`).

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e2fix2.txt:209,211`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e2fix2.txt:214,216,219`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp | rc 0 over the 8 derived files — `gates-e2fix2.txt:206,221` |
| git diff --check | clean | rc 0 — `gates-e2fix2.txt:222` |
| dirty files | 0 | 0 — `gates-e2fix2.txt:223` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e2fix2.txt:24`; `fold-proofs.txt:99` |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its
foreground start (`gates-e2fix2.txt:26-27,39-40,52-53,65-66,87-88,207-208`;
`chain-e2fix2.txt:16-17`).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e2fix2.txt:227`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e2fix2.txt:229`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-e2fix2.txt:27`).

No DECLARED FINDING this round: every declared value in
`psym-derivation-e2fix2.txt` (§§1-6) matched the measured chain exactly, on
the first leg; there is no superseded prior chain for fix round 2.
