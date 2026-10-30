# M7 W2 E2 fix round 4 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the fix4 chain ran, nothing in
it adjusted after): `../wrappers-gates-fix4/psym-derivation-e2fix4.txt`,
runner `../wrappers-gates-fix4/{gates-e2fix4.sh,chain_gates_e2fix4.sh}`
(committed at `0447288f`). Base `ef3b5225` (fix3: R-A, R-C′, S27; branch `m7`),
range `ef3b5225..6c368913` for every source/header derivation — `6c368913`
(fix4: the terminal point is refined μ-free before any kInfeasible-by-violation
verdict); the commits `1742b645` (the fix3 gate record) and `0447288f` (the
declaration) add no source, header or test. Chain: detached, started
`CHAIN_START 2026-09-24T20:33:31-04:00` / `GATES_START
2026-09-24T20:33:31-04:00` at HEAD `0447288f` (the declaration commit) with 0
dirty files (`chain-e2fix4.txt:1-2`; `gates-e2fix4.txt:1-2,5`), ended
`CHAIN_END 2026-09-24T21:48:03-04:00` (`chain-e2fix4.txt:26`). Runner
transcript: `gates-e2fix4.txt` (copied verbatim from
`.scratch/m7w2e2fix4gates/logs/gates-e2fix4.log`). Chain wrapper transcript:
`chain-e2fix4.txt` (copied verbatim from
`.scratch/m7w2e2fix4gates/logs/chain-e2fix4.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2e2fix4gates/gates/`:
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
declaration and already committed alongside it under `../wrappers-gates-fix4/`
and `../wrappers-gates-fix4/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e2fix4.txt` §3) | MEASURED |
|---|---|---|
| tests | 2832 | 2832 — `gates-e2fix4.txt:38`; `suite-debug.txt:5671` |
| failed | 0 | 0 — `gates-e2fix4.txt:37`; `suite-debug.txt:5671` |
| by name | fix3 baseline + 2 ADDED, none removed | asserted — `gates-e2fix4.txt:39` (2834 started names == 2832 + 2) |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e2fix4.txt:52`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e2fix4.txt:51`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone
(`gates-e2fix4.txt:43-44`; `suite-debug-long.txt:2`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2833 | 2833 — `gates-e2fix4.txt:64`; `suite-release.txt:5673` |
| failed | 0 | 0 — `gates-e2fix4.txt:63`; `suite-release.txt:5673` |
| by name | fix3 baseline + 2 ADDED, none removed | asserted — `gates-e2fix4.txt:65` (2835 started names == 2833 + 2) |

By name against the fix-round-3 gate transcripts
(`../gates-e2-fix3/suite-{debug,release}.txt`, 2832/2833 started names;
`gates-e2fix4.txt:17`): two new instances, none removed —
`QpEngineStructuralViolation.{E2Fix4ARealTerminalViolationStillEndsInfeasible,
E2Fix4TheTerminalMuPointIsRefinedBeforeAnInfeasibleVerdict}` (declaration §3).

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences under the 1e-5 residual gate | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e2fix4.txt:71,80,82` (CMP_RC=0 at `:79,81,83`; asserted `:86-87`); `replay/comparison.txt:1,9,10` |
| within-gate walk residual moves | 7, on f7_n800 / f7_n1000_path_warm, recorded not asserted | 7, all on those two cells — `gates-e2fix4.txt:72-78`; `replay/comparison.txt:2-8` |

ssn and ipm show 0 within-gate moves (`gates-e2fix4.txt:80,82`). No counter,
status or objective column moved on any cell.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 38 = 3 A + 35 B | 38 — `gates-e2fix4.txt:108`, asserted `:115,119` | 38 — `gates-e2fix4.txt:156`, asserted `:163,167` |
| class A (source changed) | 3 | 3 — `gates-e2fix4.txt:109`, asserted `:120` | 3 — `gates-e2fix4.txt:157`, asserted `:168` |
| class B (includer, source untouched) | 35 | 35 — `gates-e2fix4.txt:110`, asserted `:121` | 35 — `gates-e2fix4.txt:158`, asserted `:169` |
| UNEXPLAINED | 0 | 0 — `gates-e2fix4.txt:111`, asserted `:117` | 0 — `gates-e2fix4.txt:159`, asserted `:165` |
| NOISE-ONLY | 2 Release (0 lib) / 37 Debug (6 lib) | 2 (0 in the library) — `psym-classification-Release.txt:52` | 37 (6 in the library, all explained) — `psym-classification-Debug.txt:52` |
| negative control | 105 / 70 | 105 — `gates-e2fix4.txt:112,118`; `psym-classification-Release.txt:56` | 70 — `gates-e2fix4.txt:160,166`; `psym-classification-Debug.txt:91` |
| compare rc | 1 | 1 — `gates-e2fix4.txt:97,113` | 1 — `gates-e2fix4.txt:145,161` |
| capture pair | after = before = 151 | 151 — `gates-e2fix4.txt:92-94` | 151 — `gates-e2fix4.txt:140-142` |
| outside-universe seam object | built, 0 in the manifest | `gates-e2fix4.txt:95` | `gates-e2fix4.txt:143` |
| ONLY-AFTER | `[]` | `gates-e2fix4.txt:98` | `gates-e2fix4.txt:146` |
| library ADDED | none | `[]` — `gates-e2fix4.txt:122` | `[]` — `gates-e2fix4.txt:170` |
| class B symmetry | EQUAL (35 both configs) | asserted — `gates-e2fix4.txt:192` ("class B sets equal (35 object(s))") | same assert (cross-config) |
| class A cross-config identical | yes | asserted — `gates-e2fix4.txt:195-197` | same |
| class B by name | matches the declared 35-name list | `gates-e2fix4.txt:201` | `gates-e2fix4.txt:202` |

The header derivation (`gates-e2fix4.txt:100-106,148-154`) accepted exactly
the two declared headers (`solver_counters.h`, `qp_engine.h`). Class A (3,
identical in both configurations) is the 3 MODIFIED `.cpp`, none outside the
P-SYM universe (`gates-e2fix4.txt:8,10`):
`CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o`,
`tests/sqp/CMakeFiles/hven_sqp_tests.dir/{test_qp_engine_indefinite,test_sqp_solver}.cpp.o`.
Class B is symmetric — Release and Debug sets equal, 35 objects, declared by
name in the runner (`gates-e2fix4.txt:16`); the library objects among them are
`src/core/ledger.cpp.o`, `src/drivers/sqp_solver.cpp.o`,
`src/globalization/sqp/soc_elastic_restoration.cpp.o`,
`src/qp/ipqp_engine.cpp.o`. The asymmetry admission list is EMPTY this round.
The mover lists are in `psym-classification-{Release,Debug}.txt`, the full
byte-difference dumps in `psym-compare-{Release,Debug}.txt`, the object
inventories in `psym-libobjs{,-both}-{Release,Debug}.txt`.

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 5 = {qp_engine.cpp.o} (derived) + {ledger.cpp.o, sqp_solver.cpp.o, soc_elastic_restoration.cpp.o, ipqp_engine.cpp.o} (declared) | exactly that, 5 — `gates-e2fix4.txt:124-128,132`, asserted `:136` | exactly that, 5 — `gates-e2fix4.txt:172,176,178-180,186`, asserted `:190` |
| objects compared | 44 | 44 — `gates-e2fix4.txt:129`; `psym-sections-Release.txt` | 44 — `gates-e2fix4.txt:183`; `psym-sections-Debug.txt` |
| byte-identical | — | 39 — `gates-e2fix4.txt:130` | 33 — `gates-e2fix4.txt:184` |
| DEBUG_ONLY | Release 0 / Debug 6 | 0 — `gates-e2fix4.txt:131`, asserted `:137` | 6 (`console_trace_sink.cpp.o`, `ipm_solver.cpp.o`, `sqp_print.cpp.o`, `trace_writer.cpp.o`, `ssn_engine.cpp.o`, `continuation.cpp.o`) — `gates-e2fix4.txt:173-175,177,181-182,185`, asserted `:191` |
| REFUSED | 0 | 0 — `gates-e2fix4.txt:133` | 0 — `gates-e2fix4.txt:187` |

`qp_engine.cpp.o`'s Release section count moved 827 → 834
(`gates-e2fix4.txt:128`).

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e2fix4.txt:206,208`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e2fix4.txt:211,213,216`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp (incl. tests/*.h) | rc 0 over the derived files — `gates-e2fix4.txt:203,218` |
| git diff --check | clean | rc 0 — `gates-e2fix4.txt:219` |
| dirty files | 0 | 0 — `gates-e2fix4.txt:220` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e2fix4.txt:24`; `fold-proofs.txt:99` |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its
foreground start (`gates-e2fix4.txt:27-28,40-41,53-54,66-67,88-89,204-205`;
`chain-e2fix4.txt:16-17`).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e2fix4.txt:224`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e2fix4.txt:226`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-e2fix4.txt:27`).

No DECLARED FINDING this round: every declared value in
`psym-derivation-e2fix4.txt` (§§1-6) matched the measured chain exactly, on
the first leg; there is no superseded prior chain for fix round 4.
