# M7 W2 E2 fix rounds 1 + 1b — gate evidence: DECLARED vs MEASURED

**Naming note.** The task that produced this directory named the destination
`gates-e2-fix1/`, mirroring how the E1b fix1 gates were recorded. That exact
path is already occupied by a *different, already-committed* round — the base
E2 gate record (declaration `e44b8e18`, committed at `4858019e`), whose files
(`README.md`, `gates-e2-fix1.txt`, `suite-debug.txt`, `psym-classification-
*.txt`, `replay/`, etc.) share names with what this round would have written.
Writing this round's files into that directory would have silently overwritten
committed, historical gate evidence from an unrelated round — exactly what "do
not delete anything" forbids in spirit as well as letter. This round is
recorded in a sibling directory, `gates-e2-fix1b/`, instead: same declaration
lineage (`wrappers-gates-fix1/`), same shape, no collision. Nothing under
`gates-e2-fix1/` was read, moved, or modified to produce this record.

Declaration (written and committed **before** the fix1+1b chain ran, nothing
in it adjusted after): `../wrappers-gates-fix1/psym-derivation-e2fix1.txt`,
runner `../wrappers-gates-fix1/{gates-e2fix1.sh,chain_gates_e2fix1.sh}`
(committed at `3e2bed07`). Base `6d123e75` (the E2-D1 readout head; branch
`m7`), range `6d123e75..b2dfe97a` for every source/header derivation —
`b25ab7ed` (fix1: F1-F3 of the E2-D1 diagnosis), `bf37c305` (fix1b: the
removal certificate for E2-D2), `b2dfe97a` (readout, docs/notes/data only).
Chain: detached, started `CHAIN_START 2026-09-24T03:14:52-04:00` /
`GATES_START 2026-09-24T03:14:52-04:00` at HEAD `3e2bed07` (the declaration
commit) with 0 dirty files (`chain-e2fix1.txt:1-2`; `gates-e2fix1.txt:1-2`),
ended `CHAIN_END 2026-09-24T04:10:25-04:00` (`chain-e2fix1.txt:26`). Runner
transcript: `gates-e2fix1.txt` (copied verbatim from
`.scratch/m7w2e2fix1gates/logs/gates-e2fix1.log`). Chain wrapper transcript:
`chain-e2fix1.txt` (copied verbatim from
`.scratch/m7w2e2fix1gates/logs/chain-e2fix1.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2e2fix1gates/gates/`:
`suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt`,
`install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, `psym-*.txt`
(from `gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`,
`libobjs-both-{Release,Debug}.txt`, `sections-{Release,Debug}.txt`),
`replay/` (the `run-*.log` files renamed `.txt`, `head-*.csv` kept as-is). The
build trees and object directories (`gates-dbg/`, `gates-rel/`,
`psym-build/`, `psym/{before,after}{,-obj}-*`, `smoke-work/`) are not copied.

Rehearsal, pre-flight and provenance-check transcripts, measured before the
declaration and already committed alongside it under `../wrappers-gates-fix1/`
and `../wrappers-gates-fix1/rehearsal/`: `preflight.txt`,
`rehearsal/offline-g4-and-rehearsal.sh`,
`rehearsal/offline-g4-comparison.txt`,
`rehearsal/rehearsal-classification-{Release,Debug}.txt`,
`rehearsal/rehearsal-log-summary.txt`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## fix1 (`b25ab7ed`) was never gated alone

The E2 fix1 commit was never run through this gate chain by itself: its
offline G4 check found the two f7 walk movers that motivated fix1b, and that
STOP is recorded in
`docs/notes/data/2026-09-m7-w2-e2-d2-f7-regression/REPORT.md` (the f7
warm-cell regression trace, candidates A-G, and the removal certificate). This
chain gates fix1 **and** fix1b together, against the base that precedes
fix1 (`6d123e75`).

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e2fix1.txt` §3) | MEASURED |
|---|---|---|
| tests | 2817 | 2817 — `gates-e2fix1.txt:36`; `suite-debug.txt:5641` |
| failed | 0 | 0 — `gates-e2fix1.txt:35`; `suite-debug.txt:5641` |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e2fix1.txt:49`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e2fix1.txt:48`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone
(`gates-e2fix1.txt:40-41`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2818 | 2818 — `gates-e2fix1.txt:61`; `suite-release.txt:5643` |
| failed | 0 | 0 — `gates-e2fix1.txt:60`; `suite-release.txt:5643` |

By name against E1b fix1's 2808/2807 (§3): ten new instances, none removed —
five from fix1 (`QpE2WorkingSet.ALockedPositiveMemberStillGetsTheVerified-
Certificate`, `QpE2WorkingSet.TheUndecidableBorderPingPongRetreatsOnceAnd-
EndsOptimal`, `QpE2WorkingSet.AFullyPinnedRowDecidesAnUndecidablePinExactly`,
`BothAlgebras/WsIndependence.AnOptionalPinInADependentEqualityChainIs-
Removed/{0,1}`, `WsIndependenceDetector.TheEliminatedSupportNamesThePins-
TheDependencyUses`) and five from fix1b
(`WsIndependenceDetector.APinInTheSupportByASmallCoefficientIsNotOfferedFor-
Removal`, `WsIndependenceDetector.WithNoRemovableMemberCertifiedTheBest-
CertifiedOneIsOfferedAlone`, `BothAlgebras/WsIndependence.AnUncertifiedPinIs-
KeptAndACertifiedRowRemoved/{0,1}`), per `psym-derivation-e2fix1.txt` §3.

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e2fix1.txt:67,76,78` (CMP_RC=0 at `:75,77,79`); `replay/comparison.txt:1,9,10` |

7 within-gate residual moves recorded on the walk engine, not asserted
(`gates-e2fix1.txt:68-74`; `replay/comparison.txt:2-8`), all on the two f7
warm cells (`f7_n1000_path_warm`, `f7_n800_path_warm`) — matching the declared
"7 within-gate walk residual moves recorded, not asserted." Both f7 cells are
back at their M6 shape: Optimal, 2 factorizations, 6/12 minors
(`psym-derivation-e2fix1.txt` §5); ssn and ipm show 0 within-gate moves
(`gates-e2fix1.txt:76,78`; `replay/comparison.txt:9-10`).

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 39 = 3 A + 36 B | 39 — `gates-e2fix1.txt:105`, asserted `:112` | 39 — `gates-e2fix1.txt:155`, asserted `:162` |
| class A (source changed) | 3 | 3 — `gates-e2fix1.txt:106`, asserted `:117` | 3 — `gates-e2fix1.txt:156`, asserted `:167` |
| class B (includer, source untouched) | 36 | 36 — `gates-e2fix1.txt:107`, asserted `:118` | 36 — `gates-e2fix1.txt:157`, asserted `:168` |
| UNEXPLAINED | 0 | 0 — `gates-e2fix1.txt:108`, asserted `:114` | 0 — `gates-e2fix1.txt:158`, asserted `:164` |
| NOISE-ONLY | 2 Release / 37 Debug | 2 (0 in the library) — `psym-classification-Release.txt:53` | 37 (6 in the library, all explained) — `psym-classification-Debug.txt:53` |
| negative control | ≥ 1 | 104 — `gates-e2fix1.txt:109,115`; `psym-classification-Release.txt:57` | 69 — `gates-e2fix1.txt:159,165`; `psym-classification-Debug.txt:92` |
| compare rc | 1 | 1 — `gates-e2fix1.txt:94,110` | 1 — `gates-e2fix1.txt:144,160` |
| capture pair | after = before = 151 | 151 — `gates-e2fix1.txt:88-90` | 151 — `gates-e2fix1.txt:138-140` |
| outside-universe seam object | built, 0 in the manifest | `gates-e2fix1.txt:91-92` | `gates-e2fix1.txt:141-142` |
| ONLY-AFTER | `[]` (0 added inside the universe) | `gates-e2fix1.txt:95` | `gates-e2fix1.txt:145` |
| library ADDED | none | `[]` — `gates-e2fix1.txt:119` | `[]` — `gates-e2fix1.txt:169` |
| class B symmetry | EQUAL (36 both configs) | asserted — `gates-e2fix1.txt:192` ("class B sets equal (36 object(s))") | same assert (cross-config) |
| class A cross-config identical | yes | asserted — `gates-e2fix1.txt:195-197` | same |
| class B by name | matches the declared 36-name list | `gates-e2fix1.txt:201` | `gates-e2fix1.txt:202` |

The mover lists are in `psym-classification-{Release,Debug}.txt`, with the
full byte-difference dumps in `psym-compare-{Release,Debug}.txt` and the
object inventories in `psym-libobjs-{Release,Debug}.txt` /
`psym-libobjs-both-{Release,Debug}.txt`. Class A (3, identical in both
configurations) is the 4 MODIFIED `.cpp` less the 1 outside the P-SYM
universe (`tests/qp/test_qp_anti_cycling_arm.cpp`, built under
`hven_qp_walk_seam_tests`, an `HVEN_TESTING` recompile target):
`CMakeFiles/hven.dir/src/qp/{qp_engine,ws_independence}.cpp.o`,
`tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_qp_ws_independence.cpp.o`.
Class B is symmetric — Release and Debug sets equal, 36 objects, declared by
name in the runner; the library objects among them are
`src/core/ledger.cpp.o`, `src/drivers/sqp_solver.cpp.o`,
`src/globalization/sqp/soc_elastic_restoration.cpp.o`,
`src/qp/ipqp_engine.cpp.o`. The asymmetry admission list is EMPTY this round.

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 6 = {qp_engine.cpp.o, ws_independence.cpp.o} (derived) + {ledger.cpp.o, sqp_solver.cpp.o, soc_elastic_restoration.cpp.o, ipqp_engine.cpp.o} (declared) | exactly that, 6 — `gates-e2fix1.txt:121-126,130`, asserted `:133-134` | exactly that, 6 — `gates-e2fix1.txt:171-181,186`, asserted `:189-190` |
| objects compared | 44 | 44 — `gates-e2fix1.txt:127`; `psym-sections-Release.txt` | 44 — `gates-e2fix1.txt:183`; `psym-sections-Debug.txt` |
| byte-identical | — | 38 — `gates-e2fix1.txt:128` | 32 — `gates-e2fix1.txt:184` |
| DEBUG_ONLY | Release 0 / Debug 6 | 0 — `gates-e2fix1.txt:129,135` | 6 (`console_trace_sink.cpp.o`, `ipm_solver.cpp.o`, `sqp_print.cpp.o`, `trace_writer.cpp.o`, `ssn_engine.cpp.o`, `continuation.cpp.o`) — `gates-e2fix1.txt:172-174,176,180,182,185` |
| REFUSED | 0 | 0 — `gates-e2fix1.txt:131` | 0 — `gates-e2fix1.txt:187` |

`qp_engine.cpp.o`'s section count moved 831 → 834 in Release
(`gates-e2fix1.txt:125`) and 9065 → 9071 in Debug (`gates-e2fix1.txt:179`);
`ws_independence.cpp.o` moved 455 → 465 in Release (`gates-e2fix1.txt:126`)
and 5347 → 5366 in Debug (`gates-e2fix1.txt:181`).

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e2fix1.txt:206,208`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e2fix1.txt:213,216`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp | rc 0 — `gates-e2fix1.txt:218` |
| dirty files | 0 | 0 — `gates-e2fix1.txt:220` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e2fix1.txt:24`; `fold-proofs.txt:99` |

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e2fix1.txt:224`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e2fix1.txt:226`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-e2fix1.txt:27`).

No DECLARED FINDING this round: G4 held at 0 differences under the 1e-5
residual gate (the two f7 cells' within-gate residual moves matched the
declared count exactly) and no G5/G5b mover fell outside the declared
class-A/class-B/DEBUG_ONLY partition. Every declared value in
`psym-derivation-e2fix1.txt` (§§1-8) matched the measured chain exactly; this
round ran green on its first leg — there is no superseded prior chain to
record here for fix1+1b (contrast the base E2 gates round's "first chain"
section in `../gates-e2-fix1/README.md`, which recorded a refused
predecessor for an unrelated declaration).
