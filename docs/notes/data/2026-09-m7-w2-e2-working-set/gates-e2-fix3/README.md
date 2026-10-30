# M7 W2 E2 fix round 3 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the fix3 chain ran, nothing in
it adjusted after): `../wrappers-gates-fix3/psym-derivation-e2fix3.txt`,
runner `../wrappers-gates-fix3/{gates-e2fix3.sh,chain_gates_e2fix3.sh}`
(committed at `3f4bc9f6`). Base `a47753a8` (the E2-D4 read-out; branch `m7`),
range `a47753a8..ef3b5225` for every source/header derivation — `ef3b5225`
(fix3: R-A, R-C′, S27; R-B and R-D deferred); the declaration commit
`3f4bc9f6` adds no source, header or test. Chain: detached, started
`CHAIN_START 2026-09-24T18:09:12-04:00` / `GATES_START
2026-09-24T18:09:12-04:00` at HEAD `3f4bc9f6` (the declaration commit) with 0
dirty files (`chain-e2fix3.txt:1-2`; `gates-e2fix3.txt:1-2,5`), ended
`CHAIN_END 2026-09-24T19:41:41-04:00` (`chain-e2fix3.txt:26`). Runner
transcript: `gates-e2fix3.txt` (copied verbatim from
`.scratch/m7w2e2fix3gates/logs/gates-e2fix3.log`). Chain wrapper transcript:
`chain-e2fix3.txt` (copied verbatim from
`.scratch/m7w2e2fix3gates/logs/chain-e2fix3.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2e2fix3gates/gates/`:
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
declaration and already committed alongside it under `../wrappers-gates-fix3/`
and `../wrappers-gates-fix3/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e2fix3.txt` §3) | MEASURED |
|---|---|---|
| tests | 2830 | 2830 — `gates-e2fix3.txt:38`; `suite-debug.txt:5667` |
| failed | 0 | 0 — `gates-e2fix3.txt:37`; `suite-debug.txt:5667` |
| by name | fix2 baseline + 4 ADDED, none removed | asserted — `gates-e2fix3.txt:39` (2832 started names == 2828 + 4) |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e2fix3.txt:52`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e2fix3.txt:51`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone
(`gates-e2fix3.txt:43-44`; `suite-debug-long.txt:2-3`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2831 | 2831 — `gates-e2fix3.txt:64`; `suite-release.txt:5669` |
| failed | 0 | 0 — `gates-e2fix3.txt:63`; `suite-release.txt:5669` |
| by name | fix2 baseline + 4 ADDED, none removed | asserted — `gates-e2fix3.txt:65` (2833 started names == 2829 + 4) |

By name against the fix-round-2 gate transcripts
(`../gates-e2-fix2/suite-{debug,release}.txt`, 2828/2829 started names;
`gates-e2fix3.txt:17`): four new instances, none removed —
`QpE2WorkingSet.{AStructuralDependencyDecidesFirstOnTheSteenbrbElasticQp,
TheLockedVertexOnTheAcopr57PrimalQpsStillEndsN3,
AMuFreeInertiaReReadCertifiesTheSingularPsdFaceOfAcopp57,
AMuFreeSuspectReadDoesNotCertifyASaddle}` (declaration §3).

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences under the 1e-5 residual gate | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e2fix3.txt:71,80,82` (CMP_RC=0 at `:79,81,83`; asserted `:86-87`); `replay/comparison.txt:1,9,10` |
| within-gate walk residual moves | 7, on f7_n800 / f7_n1000_path_warm, recorded not asserted | 7, all on those two cells — `gates-e2fix3.txt:72-78`; `replay/comparison.txt:2-8` |

ssn and ipm show 0 within-gate moves (`gates-e2fix3.txt:80,82`). No counter,
status or objective column moved on any cell.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 38 = 3 A + 35 B | 38 — `gates-e2fix3.txt:109`, asserted `:116,120` | 38 — `gates-e2fix3.txt:158`, asserted `:165,169` |
| class A (source changed) | 3 | 3 — `gates-e2fix3.txt:110`, asserted `:121` | 3 — `gates-e2fix3.txt:159`, asserted `:170` |
| class B (includer, source untouched) | 35 | 35 — `gates-e2fix3.txt:111`, asserted `:122` | 35 — `gates-e2fix3.txt:160`, asserted `:171` |
| UNEXPLAINED | 0 | 0 — `gates-e2fix3.txt:112`, asserted `:118` | 0 — `gates-e2fix3.txt:161`, asserted `:167` |
| NOISE-ONLY | 2 Release (0 lib) / 38 Debug (7 lib) | 2 (0 in the library) — `psym-classification-Release.txt:52` | 38 (7 in the library, all explained) — `psym-classification-Debug.txt:52` |
| negative control | 105 / 69 | 105 — `gates-e2fix3.txt:113,119`; `psym-classification-Release.txt:56` | 69 — `gates-e2fix3.txt:162,168`; `psym-classification-Debug.txt:92` |
| compare rc | 1 | 1 — `gates-e2fix3.txt:98,114` | 1 — `gates-e2fix3.txt:147,163` |
| capture pair | after = before = 151 | 151 — `gates-e2fix3.txt:92-94` | 151 — `gates-e2fix3.txt:141-143` |
| outside-universe seam object | built, 0 in the manifest | `gates-e2fix3.txt:95-96` | `gates-e2fix3.txt:144-145` |
| ONLY-AFTER | `[]` | `gates-e2fix3.txt:99` | `gates-e2fix3.txt:148` |
| library ADDED | none | `[]` — `gates-e2fix3.txt:123` | `[]` — `gates-e2fix3.txt:172` |
| class B symmetry | EQUAL (35 both configs) | asserted — `gates-e2fix3.txt:195` ("class B sets equal (35 object(s))") | same assert (cross-config) |
| class A cross-config identical | yes | asserted — `gates-e2fix3.txt:198-200` | same |
| class B by name | matches the declared 35-name list | `gates-e2fix3.txt:204` | `gates-e2fix3.txt:205` |

The header derivation (`gates-e2fix3.txt:101-107,150-156`) accepted exactly
the three declared headers (`solver_counters.h`, `qp_engine.h`,
`ws_independence.h`). Class A (3, identical in both configurations) is the 4
MODIFIED `.cpp` less the 1 outside the P-SYM universe
(`tests/qp/test_qp_anti_cycling_arm.cpp`, built under
`hven_qp_walk_seam_tests`, an `HVEN_TESTING` recompile target;
`gates-e2fix3.txt:10`): `CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o`,
`tests/sqp/CMakeFiles/hven_sqp_tests.dir/{test_hs_battery,test_qp_engine_indefinite}.cpp.o`
(`psym-classification-Release.txt:10-13`). Class B is symmetric — Release and
Debug sets equal, 35 objects, declared by name in the runner
(`gates-e2fix3.txt:16`); the library objects among them are
`src/core/ledger.cpp.o`, `src/drivers/sqp_solver.cpp.o`,
`src/globalization/sqp/soc_elastic_restoration.cpp.o`,
`src/qp/ipqp_engine.cpp.o`. The asymmetry admission list is EMPTY this round.
The mover lists are in `psym-classification-{Release,Debug}.txt`, the full
byte-difference dumps in `psym-compare-{Release,Debug}.txt`, the object
inventories in `psym-libobjs{,-both}-{Release,Debug}.txt`.

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 5 = {qp_engine.cpp.o} (derived) + {ledger.cpp.o, sqp_solver.cpp.o, soc_elastic_restoration.cpp.o, ipqp_engine.cpp.o} (declared) | exactly that, 5 — `gates-e2fix3.txt:125-129,133`, asserted `:137` | exactly that, 5 — `gates-e2fix3.txt:174,178,180-182,189`, asserted `:193` |
| objects compared | 44 | 44 — `gates-e2fix3.txt:130`; `psym-sections-Release.txt` | 44 — `gates-e2fix3.txt:186`; `psym-sections-Debug.txt` |
| byte-identical | — | 39 — `gates-e2fix3.txt:131` | 32 — `gates-e2fix3.txt:187` |
| DEBUG_ONLY | Release 0 / Debug 7 | 0 — `gates-e2fix3.txt:132`, asserted `:138` | 7 (`console_trace_sink.cpp.o`, `ipm_solver.cpp.o`, `sqp_print.cpp.o`, `trace_writer.cpp.o`, `ssn_engine.cpp.o`, `ws_independence.cpp.o`, `continuation.cpp.o`) — `gates-e2fix3.txt:175-177,179,183-185,188`, asserted `:194` |
| REFUSED | 0 | 0 — `gates-e2fix3.txt:134` | 0 — `gates-e2fix3.txt:190` |

`qp_engine.cpp.o`'s Release section count moved 834 → 827
(`gates-e2fix3.txt:129`).

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e2fix3.txt:209,211`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e2fix3.txt:214,216,219`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp (incl. tests/*.h) | rc 0 over the derived files — `gates-e2fix3.txt:206,221` |
| git diff --check | clean | rc 0 — `gates-e2fix3.txt:222` |
| dirty files | 0 | 0 — `gates-e2fix3.txt:223` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e2fix3.txt:24`; `fold-proofs.txt:99` |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its
foreground start (`gates-e2fix3.txt:27-28,40-41,53-54,66-67,88-89,207-208`;
`chain-e2fix3.txt:16-17`).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e2fix3.txt:227`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e2fix3.txt:229`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-e2fix3.txt:27`).

No DECLARED FINDING this round: every declared value in
`psym-derivation-e2fix3.txt` (§§1-6) matched the measured chain exactly, on
the first leg; there is no superseded prior chain for fix round 3.
