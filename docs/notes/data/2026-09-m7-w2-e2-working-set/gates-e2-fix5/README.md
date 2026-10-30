# M7 W2 E2 fix round 5 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the fix5 chain ran, nothing in
it adjusted after): `../wrappers-gates-fix5/psym-derivation-e2fix5.txt`,
runner `../wrappers-gates-fix5/{gates-e2fix5.sh,chain_gates_e2fix5.sh}`
(committed at `723712e0`). Base `0447288f` (the fix4 gate declaration,
source-identical to the fix4 engine commit; branch `m7`), range
`0447288f..34e100de` for every source/header derivation — `34e100de` (fix5:
the locked-vertex unlock under the min-ratio rule, R-B, on the refined terminal
check; R-D deferred); the commits `f96da576` (the fix4 gate record) and
`723712e0` (the declaration) add no source, header or test. Chain: detached,
started `CHAIN_START 2026-09-24T22:54:48-04:00` / `GATES_START
2026-09-24T22:54:48-04:00` at HEAD `723712e0` (the declaration commit) with 0
dirty files (`chain-e2fix5.txt:1-2`; `gates-e2fix5.txt:1-2,5`), ended
`CHAIN_END 2026-09-24T23:50:53-04:00` (`chain-e2fix5.txt:26`). Runner
transcript: `gates-e2fix5.txt` (copied verbatim from
`.scratch/m7w2e2fix5gates/logs/gates-e2fix5.log`). Chain wrapper transcript:
`chain-e2fix5.txt` (copied verbatim from
`.scratch/m7w2e2fix5gates/logs/chain-e2fix5.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2e2fix5gates/gates/`:
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
declaration and already committed alongside it under `../wrappers-gates-fix5/`
and `../wrappers-gates-fix5/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e2fix5.txt` §3) | MEASURED |
|---|---|---|
| tests | 2832 | 2832 — `gates-e2fix5.txt:38`; `suite-debug.txt:5671` |
| failed | 0 | 0 — `gates-e2fix5.txt:37`; `suite-debug.txt:5671` |
| by name | fix4 baseline − 3 REMOVED + 3 ADDED (three renames) | asserted — `gates-e2fix5.txt:39` (2834 started names == 2834 − 3 + 3) |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e2fix5.txt:52`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e2fix5.txt:51`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone
(`gates-e2fix5.txt:43-44`; `suite-debug-long.txt:2-3`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2833 | 2833 — `gates-e2fix5.txt:64`; `suite-release.txt:5673` |
| failed | 0 | 0 — `gates-e2fix5.txt:63`; `suite-release.txt:5673` |
| by name | fix4 baseline − 3 REMOVED + 3 ADDED (three renames) | asserted — `gates-e2fix5.txt:65` (2835 started names == 2835 − 3 + 3) |

By name against the fix-round-4 gate transcripts
(`../gates-e2-fix4/suite-{debug,release}.txt`, 2834/2835 started names, run at
`0447288f` = BEFORE; `gates-e2fix5.txt:17`): three renames, totals unchanged
(declaration §3) —

- REMOVED `QpE2WorkingSet.TheLockedVertexOnTheAcopr57PrimalQpsStillEndsN3`,
  `QpEngineStructuralViolation.T6bTheStiffWALKSOwnPathologyAboveTheShippedDualMuIsUNCHANGED`,
  `SqpDriverElasticSeed.T6bTheBandsHIGHRungsRemainAPREEXISTINGResidue`;
- ADDED `QpE2WorkingSet.TheLockedVertexUnlocksOnTheAcopr57PrimalQps`,
  `QpEngineStructuralViolation.T6bTheStiffBorderWalkAboveTheShippedDualMuReachesTheOptimum`,
  `SqpDriverElasticSeed.T6bTheBandsHIGHRungsReachTheOptimum`.

No added test file; `tests/CMakeLists.txt` unchanged (`gates-e2fix5.txt:18`).

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences under the 1e-5 residual gate | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e2fix5.txt:71,80,82` (CMP_RC=0 at `:79,81,83`; asserted `:86-87`); `replay/comparison.txt:1,9,10` |
| within-gate walk residual moves | 7, on f7_n800 / f7_n1000_path_warm, recorded not asserted | 7, all on those two cells — `gates-e2fix5.txt:72-78`; `replay/comparison.txt:2-8` |

ssn and ipm show 0 within-gate moves (`gates-e2fix5.txt:80,82`). No counter,
status or objective column moved on any cell. The 7 within-gate values are
the same digits the fix4 record shows (`../gates-e2-fix4/replay/comparison.txt:2-8`).

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 38 = 2 A + 36 B | 38 — `gates-e2fix5.txt:109`, asserted `:116,120` | 38 — `gates-e2fix5.txt:158`, asserted `:165,169` |
| class A (source changed) | 2 | 2 — `gates-e2fix5.txt:110`, asserted `:121` | 2 — `gates-e2fix5.txt:159`, asserted `:170` |
| class B (includer, source untouched) | 36 | 36 — `gates-e2fix5.txt:111`, asserted `:122` | 36 — `gates-e2fix5.txt:160`, asserted `:171` |
| UNEXPLAINED | 0 | 0 — `gates-e2fix5.txt:112`, asserted `:118` | 0 — `gates-e2fix5.txt:161`, asserted `:167` |
| NOISE-ONLY | 2 Release (0 lib) / 37 Debug (6 lib) | 2 (0 in the library) — `psym-classification-Release.txt:52` | 37 (6 in the library, all explained) — `psym-classification-Debug.txt:52` |
| negative control | 105 / 70 | 105 — `gates-e2fix5.txt:113,119`; `psym-classification-Release.txt:56` | 70 — `gates-e2fix5.txt:162,168`; `psym-classification-Debug.txt:91` |
| compare rc | 1 | 1 — `gates-e2fix5.txt:98,114` | 1 — `gates-e2fix5.txt:147,163` |
| capture pair | after = before = 151 | 151 — `gates-e2fix5.txt:92-94` | 151 — `gates-e2fix5.txt:141-143` |
| outside-universe object (`test_qp_anti_cycling_arm.cpp.o`) | built, 0 in the manifest | `gates-e2fix5.txt:95-96` | `gates-e2fix5.txt:144-145` |
| ONLY-AFTER | `[]` | `gates-e2fix5.txt:99` | `gates-e2fix5.txt:148` |
| library ADDED | none | `[]` — `gates-e2fix5.txt:123` | `[]` — `gates-e2fix5.txt:172` |
| class B symmetry | EQUAL (36 both configs) | asserted — `gates-e2fix5.txt:194` ("class B sets equal (36 object(s))") | same assert (cross-config) |
| class A cross-config identical | yes | asserted — `gates-e2fix5.txt:197-199` | same |
| class B by name | matches the declared 36-name list | `gates-e2fix5.txt:203` | `gates-e2fix5.txt:204` |

The header derivation (`gates-e2fix5.txt:101-107,150-156`) accepted exactly
the two declared headers (`solver_counters.h`, `qp_engine.h`). Class A (2,
identical in both configurations) is the 3 MODIFIED `.cpp` less the one outside
the P-SYM universe (`tests/qp/test_qp_anti_cycling_arm.cpp`, built into
`hven_qp_walk_seam_tests`; `gates-e2fix5.txt:8,10`):
`CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o`,
`tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_solver.cpp.o`
(`psym-classification-Release.txt:10-12`). Class B is symmetric — Release and
Debug sets equal, 36 objects, declared by name in the runner
(`gates-e2fix5.txt:16`); against fix4's 35, `test_qp_engine_indefinite.cpp.o`
joined (class A there, unmodified here). The library objects among them are
`src/core/ledger.cpp.o`, `src/drivers/sqp_solver.cpp.o`,
`src/globalization/sqp/soc_elastic_restoration.cpp.o`,
`src/qp/ipqp_engine.cpp.o` (`psym-classification-Release.txt:14-17`). The
asymmetry admission list is EMPTY this round. The mover lists are in
`psym-classification-{Release,Debug}.txt`, the full byte-difference dumps in
`psym-compare-{Release,Debug}.txt`, the object inventories in
`psym-libobjs{,-both}-{Release,Debug}.txt`.

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 5 = {qp_engine.cpp.o} (derived) + {ledger.cpp.o, sqp_solver.cpp.o, soc_elastic_restoration.cpp.o, ipqp_engine.cpp.o} (declared) | exactly that, 5 — `gates-e2fix5.txt:125-129,133`, asserted `:137` | exactly that, 5 — `gates-e2fix5.txt:174,178,180-182,188`, asserted `:192` |
| objects compared | 44 | 44 — `gates-e2fix5.txt:130`; `psym-sections-Release.txt:11` | 44 — `gates-e2fix5.txt:185`; `psym-sections-Debug.txt:17` |
| byte-identical | 39 Release / 33 Debug | 39 — `gates-e2fix5.txt:131` | 33 — `gates-e2fix5.txt:186` |
| DEBUG_ONLY | Release 0 / Debug 6 | 0 — `gates-e2fix5.txt:132`, asserted `:138` | 6 (`console_trace_sink.cpp.o`, `ipm_solver.cpp.o`, `sqp_print.cpp.o`, `trace_writer.cpp.o`, `ssn_engine.cpp.o`, `continuation.cpp.o`) — `gates-e2fix5.txt:175-177,179,183-184,187`, asserted `:193` |
| REFUSED | 0 | 0 — `gates-e2fix5.txt:134` | 0 — `gates-e2fix5.txt:189` |

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e2fix5.txt:208,210`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e2fix5.txt:213,215,218`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp (incl. tests/*.h) | rc 0 over the derived files — `gates-e2fix5.txt:205,220` |
| git diff --check | clean | rc 0 — `gates-e2fix5.txt:221` |
| dirty files | 0 | 0 — `gates-e2fix5.txt:222` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e2fix5.txt:24`; `fold-proofs.txt:99` |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its
foreground start (`gates-e2fix5.txt:27-28,40-41,53-54,66-67,88-89,206-207`;
`chain-e2fix5.txt:16-17`).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e2fix5.txt:226`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e2fix5.txt:228`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-e2fix5.txt:27`).

No DECLARED FINDING this round: every declared value in
`psym-derivation-e2fix5.txt` (§§1-6) matched the measured chain exactly, on
the first leg; there is no superseded prior chain for fix round 5.
