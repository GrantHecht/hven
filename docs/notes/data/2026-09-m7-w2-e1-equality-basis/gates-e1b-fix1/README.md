# M7 W2 E1b fix round 1 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the fix1 chain ran, nothing in
it adjusted after): `../wrappers-e1b-fix1/psym-derivation-e1b-fix1.txt`,
runner `../wrappers-e1b-fix1/{gates-e1b-fix1.sh,chain_gates_e1b_fix1.sh}`.
Base `4858019e` (the E2 gates record), range `4858019e..89ee3e13` for every
source/header derivation (`42fc511d` F1-F4+F6, `89ee3e13` F5). Chain:
detached, started `CHAIN_START 2026-09-23T19:44:38-04:00` at HEAD
`eb3196ac` with 0 dirty files (`chain-e1b-fix1.txt:1-2`;
`gates-e1b-fix1.txt:1-2`), ended `CHAIN_END 2026-09-23T20:56:55-04:00`
(`chain-e1b-fix1.txt:26`). Runner transcript: `gates-e1b-fix1.txt` (copied
verbatim from `.scratch/m7w2e1bfix1gates/logs/gates-e1b-fix1.log`). Chain
wrapper transcript: `chain-e1b-fix1.txt` (copied verbatim from
`.scratch/m7w2e1bfix1gates/logs/chain-e1b-fix1.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2e1bfix1gates/gates/`:
`suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt`,
`install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, `psym-*.txt`
(from `gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`,
`libobjs-both-{Release,Debug}.txt`, `sections-{Release,Debug}.txt`),
`replay/` (the `run-*.log` files renamed `.txt`). The build trees and object
directories (`gates-dbg/`, `gates-rel/`, `psym/{before,after}{,-obj}-*`,
`smoke-work/`) are not copied.

Rehearsal and pre-flight transcripts, measured before the declaration and
already committed alongside it under `../wrappers-e1b-fix1/`:
`preflight.txt`, `rehearsal/offline-g4-and-rehearsal.sh`,
`rehearsal/offline-g4-comparison.txt`,
`rehearsal/rehearsal-classification-{Release,Debug}.txt`,
`rehearsal/rehearsal-sections-{Release,Debug}-summary.txt`,
`rehearsal/rehearsal-log-summary.txt`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e1b-fix1.txt` §3) | MEASURED |
|---|---|---|
| tests | 2807 | 2807 — `gates-e1b-fix1.txt:36`; `suite-debug.txt:5621` |
| failed | 0 | 0 — `gates-e1b-fix1.txt:35`; `suite-debug.txt:5621` |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e1b-fix1.txt:49`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e1b-fix1.txt:48`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone
(`gates-e1b-fix1.txt:40-41`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2808 | 2808 — `gates-e1b-fix1.txt:61`; `suite-release.txt:5623` |
| failed | 0 | 0 — `gates-e1b-fix1.txt:60`; `suite-release.txt:5623` |

By name against E2's 2803/2802: five new instances added, none removed —
`EqualityBasis.T8NumericalRankContractOnTheTwoRowPair`,
`SqpEqualityBasisSeam.T8RefusedWarmProbeIsCountedAndDegradesTheWarmLevel`,
`QpModes/EqualityBasisModes.F10ReportTelemetryOnConstructedDependentSets/{0,1,2}`
(`psym-derivation-e1b-fix1.txt` §3).

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e1b-fix1.txt:67,76,78` (CMP_RC=0 at `:75,77,79`); `replay/comparison.txt:1,9,10` |

7 within-gate residual moves recorded on the walk engine, not asserted
(`gates-e1b-fix1.txt:68-74`; `replay/comparison.txt:2-8`), matching the
declared "7 within-gate walk residual moves recorded, not asserted." E1b
converts nothing on the full-rank replay set and the telemetry changes no
numerics (`psym-derivation-e1b-fix1.txt` §5).

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 50 = 4 A + 46 B | 50 — `gates-e1b-fix1.txt:105`, asserted `:112` | 50 — `gates-e1b-fix1.txt:152`, asserted `:159` |
| class A (source changed) | 4 | 4 — `gates-e1b-fix1.txt:106`, asserted `:117` | 4 — `gates-e1b-fix1.txt:153`, asserted `:164` |
| class B (includer, source untouched) | 46 | 46 — `gates-e1b-fix1.txt:107`, asserted `:118` | 46 — `gates-e1b-fix1.txt:154`, asserted `:165` |
| UNEXPLAINED | 0 | 0 — `gates-e1b-fix1.txt:108`, asserted `:114` | 0 — `gates-e1b-fix1.txt:155`, asserted `:161` |
| NOISE-ONLY | 0 Release / 3 Debug | 0 — `psym-classification-Release.txt:84` | 3 — `psym-classification-Debug.txt:84` (`sqp_print.cpp.o` + two `ipm_corpus_leg.cpp.o` instances, all explained) |
| negative control | ≥ 1 | 42 — `gates-e1b-fix1.txt:109,115`; `psym-classification-Release.txt:86` | 39 — `gates-e1b-fix1.txt:156,162`; `psym-classification-Debug.txt:97` |
| compare rc | 1 | 1 — `gates-e1b-fix1.txt:94,110` | 1 — `gates-e1b-fix1.txt:141,157` |
| capture pair | after = before = 151 | 151 — `gates-e1b-fix1.txt:88-90` | 151 — `gates-e1b-fix1.txt:135-137` |
| outside-universe seam object | built, 0 in the manifest | `gates-e1b-fix1.txt:91-92` | `gates-e1b-fix1.txt:138-139` |
| ONLY-AFTER | `[]` (the ADDED sources' objects; 0 added) | `gates-e1b-fix1.txt:95` | `gates-e1b-fix1.txt:142` |
| library ADDED | none | `[]` — `gates-e1b-fix1.txt:119` | `[]` — `gates-e1b-fix1.txt:166` |
| class B symmetry | EQUAL (46 both configs) | asserted — `gates-e1b-fix1.txt:181` ("class B sets equal (46 object(s))") | same assert (cross-config) |
| class A cross-config identical | yes | asserted — `gates-e1b-fix1.txt:184-186` | same |
| class B by name | matches the declared 46-name list | `gates-e1b-fix1.txt:190` | `gates-e1b-fix1.txt:191` |

The mover lists are in `psym-classification-{Release,Debug}.txt`, with the
full byte-difference dumps in `psym-compare-{Release,Debug}.txt` and the
object inventories in `psym-libobjs-{Release,Debug}.txt` /
`psym-libobjs-both-{Release,Debug}.txt`. Class A (4, identical in both
configurations) is the 5 MODIFIED `.cpp` less the 1 outside the P-SYM
universe (`tests/sqp/test_sqp_equality_basis_seams.cpp`, built under
`hven_sqp_seam_tests`):
`CMakeFiles/hven.dir/src/drivers/{equality_basis,sqp_solver}.cpp.o`,
`tests/sqp/CMakeFiles/hven_sqp_tests.dir/{test_equality_basis,
test_sqp_equality_basis}.cpp.o`. Class B is symmetric — Release and Debug
sets equal, 46 objects, declared by name in the runner; the only library
object among them is `src/warmstart/continuation.cpp.o`. The
Debug-only/asymmetry admission list is EMPTY this round.

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 3 = {equality_basis.cpp.o, sqp_solver.cpp.o} (derived) + {continuation.cpp.o} (declared) | exactly that, 3 — `gates-e1b-fix1.txt:121-123,127`, asserted `:130-131` | exactly that, 3 — `gates-e1b-fix1.txt:168-171,175`, asserted `:178-179` |
| objects compared | 44 | 44 — `gates-e1b-fix1.txt:124`; `psym-sections-Release.txt` | 44 — `gates-e1b-fix1.txt:172`; `psym-sections-Debug.txt` |
| byte-identical | — | 41 — `gates-e1b-fix1.txt:125` | 40 — `gates-e1b-fix1.txt:173` |
| DEBUG_ONLY | Release 0 / Debug 1 | 0 — `gates-e1b-fix1.txt:126,132` | 1 (`sqp_print.cpp.o`) — `gates-e1b-fix1.txt:169,174,180` |
| REFUSED | 0 | 0 — `gates-e1b-fix1.txt:128` | 0 — `gates-e1b-fix1.txt:176` |

`sqp_solver.cpp.o`'s section count moved 806 → 809 in Release
(`gates-e1b-fix1.txt:122`) and 10080 → 10108 in Debug (`gates-e1b-fix1.txt:170`).

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e1b-fix1.txt:195`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e1b-fix1.txt:200,205`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp | rc 0 — `gates-e1b-fix1.txt:207` |
| dirty files | 0 | 0 — `gates-e1b-fix1.txt:209` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e1b-fix1.txt:24`; `fold-proofs.txt:99` |

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e1b-fix1.txt:213`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e1b-fix1.txt:215`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-e1b-fix1.txt:27`).

No DECLARED FINDING this round: G4 held at 0 differences under the 1e-5
residual gate and no G5/G5b mover fell outside the declared class-A/class-B/
DEBUG_ONLY partition. Every declared value in
`psym-derivation-e1b-fix1.txt` (§§1-8) matched the measured chain exactly;
this is the first E1b fix-round-1 declaration and it ran green on its first
leg — there is no superseded prior chain to record here (contrast the E2
fix1 README's "first chain" section, which recorded a refused predecessor).
