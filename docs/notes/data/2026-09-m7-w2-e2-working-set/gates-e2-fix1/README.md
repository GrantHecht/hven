# M7 W2 E2 fix round 1 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the fix1 chain ran, nothing in it
adjusted after): `../wrappers-gates/psym-derivation-e2-fix1.txt`, runner
`../wrappers-gates/{gates-e2-fix1.sh,chain_gates_e2_fix1.sh}`. Base
`0747050c` (the E1b gates record), range `0747050c..a100fbc3` for every
source/header derivation. Chain: detached, started `GATES_START
2026-09-23T15:59:43-04:00` at HEAD `e44b8e18` with 0 dirty files
(`chain-fix1.txt:1-2`; `gates-e2-fix1.txt:1-2`), ended `CHAIN_END
2026-09-23T17:18:25-04:00` (`chain-fix1.txt:26`). Runner transcript:
`gates-e2-fix1.txt` (copied verbatim from
`.scratch/m7w2e2gatesfix1/logs/gates-e2-fix1.log`). Chain wrapper transcript:
`chain-fix1.txt` (copied verbatim from
`.scratch/m7w2e2gatesfix1/logs/chain-fix1.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2e2gatesfix1/gates/`:
`suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt`,
`install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, `psym-*.txt`
(from `gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`,
`libobjs-both-{Release,Debug}.txt`, `sections-{Release,Debug}.txt`),
`replay/` (the `run-*.log` files renamed `.txt`). The build trees and object
directories (`gates-dbg/`, `gates-rel/`, `psym-build/`, `smoke-work/`) are not
copied. This round declares `EXPECT_PSYM_B_DEBUG_ONLY` empty
(`../wrappers-gates/gates-e2-fix1.sh:110`, `psym-derivation-e2-fix1.txt` §2:
"Class B IS SYMMETRIC" / "Debug-only admission list stays EMPTY"), so no
`classB-*.txt` files are produced this round — unlike E1b, which had one
Debug-only class B object and so did emit them. Their absence here is the
declared shape, not an omission.

Rehearsal transcripts, measured before the declaration and already committed
alongside it: `preflight.txt`, `preflight-refusals.txt`,
`rehearsal-classification-{Release,Debug}.txt`,
`rehearsal-sections-{Release,Debug}.txt`, `rehearsal-psym.log`,
`rehearsal-psym-run.sh`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e2-fix1.txt` §3, §6) | MEASURED |
|---|---|---|
| tests | 2802 | 2802 — `gates-e2-fix1.txt:35`; `suite-debug.txt:5611` |
| failed | 0 | 0 — `gates-e2-fix1.txt:34`; `suite-debug.txt:5611` |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e2-fix1.txt:48`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e2-fix1.txt:47`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone
(`gates-e2-fix1.txt:39-40`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2803 | 2803 — `gates-e2-fix1.txt:60`; `suite-release.txt:5613` |
| failed | 0 | 0 — `gates-e2-fix1.txt:59`; `suite-release.txt:5613` |

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e2-fix1.txt:66,75,77` (CMP_RC=0 at `:74,76,78`); `replay/comparison.txt:1,8,9` |

7 within-gate residual moves recorded on the walk engine, not asserted
(`gates-e2-fix1.txt:67-73`; `replay/comparison.txt:2-7`), matching the
declared "7 within-gate residual moves on the walk engine recorded, not
asserted."

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 70 = 13 A + 57 B | 70 — `gates-e2-fix1.txt:106`, asserted `:113` | 70 — `gates-e2-fix1.txt:160`, asserted `:167` |
| class A (source changed) | 13 | 13 — `gates-e2-fix1.txt:107`, asserted `:118` | 13 — `gates-e2-fix1.txt:161`, asserted `:172` |
| class B (includer, source untouched) | 57 | 57 — `gates-e2-fix1.txt:108`, asserted `:119` | 57 — `gates-e2-fix1.txt:162`, asserted `:173` |
| UNEXPLAINED | 0 | 0 — `gates-e2-fix1.txt:109`, asserted `:115` | 0 — `gates-e2-fix1.txt:163`, asserted `:169` |
| NOISE-ONLY | 0 Release / 11 Debug | 0 — `psym-classification-Release.txt:84` | 11 — `psym-classification-Debug.txt:84` |
| negative control | ≥ 1 | 71 — `gates-e2-fix1.txt:110,116`; `psym-classification-Release.txt:86` | 60 — `gates-e2-fix1.txt:164,170`; `psym-classification-Debug.txt:97` |
| compare rc | 1 | 1 — `gates-e2-fix1.txt:95,111` | 1 — `gates-e2-fix1.txt:149,165` |
| capture pair | after = before = 149 + ADDED 2 = 151 | 151 = 149 + 2 — `gates-e2-fix1.txt:87-89` | 151 = 149 + 2 — `gates-e2-fix1.txt:141-143` |
| outside-universe seam objects | built, 0 in the manifest | `gates-e2-fix1.txt:90-93` | `gates-e2-fix1.txt:144-147` |
| ONLY-AFTER | `{test_qp_ws_independence.cpp.o, ws_independence.cpp.o}` | `gates-e2-fix1.txt:96` | `gates-e2-fix1.txt:150` |
| library ADDED | `ws_independence.cpp.o` | `gates-e2-fix1.txt:120` | `gates-e2-fix1.txt:174` |
| class B symmetry / Debug-only | EMPTY (symmetric) | asserted — `gates-e2-fix1.txt:197` ("class B sets equal (57 object(s))") | same assert (cross-config) |
| class A cross-config identical | yes | asserted — `gates-e2-fix1.txt:200-202` | same |

The mover lists are in `psym-classification-{Release,Debug}.txt`, with the
full byte-difference dumps in `psym-compare-{Release,Debug}.txt` and the
object inventories in `psym-libobjs-{Release,Debug}.txt` /
`psym-libobjs-both-{Release,Debug}.txt`. The DIFFERS set of this round is
by name the first chain's 70 (per `psym-derivation-e2-fix1.txt` §2: "The
70-object DIFFERS set of this rehearsal is IDENTICAL, by name, to the first
chain's, per configuration"), corrected only in its class A/B partition (see
the "first chain" section below).

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 8 = {sqp_options, qp_engine} (derived) + {ledger, sqp_solver, soc_elastic_restoration, ipqp_engine, ssn_engine, continuation} (declared) | exactly that, 8 — `gates-e2-fix1.txt:122-129,133`, compared/asserted `:136-137` | exactly that, 8 — `gates-e2-fix1.txt:176-183,191`, compared/asserted `:195` |
| objects compared | 43 | 43 — `gates-e2-fix1.txt:130`; `psym-sections-Release.txt` | 43 — `gates-e2-fix1.txt:188`; `psym-sections-Debug.txt` |
| byte-identical | — | 35 — `gates-e2-fix1.txt:131` | 31 — `gates-e2-fix1.txt:189` |
| DEBUG_ONLY | Release 0 / Debug 4 | 0 — `gates-e2-fix1.txt:132,138` | 4 — `gates-e2-fix1.txt:190,196` (`console_trace_sink`, `ipm_solver`, `sqp_print`, `trace_writer`) |
| REFUSED | 0 | 0 — `gates-e2-fix1.txt:134` | 0 — `gates-e2-fix1.txt:192` |

`qp_engine.cpp.o`'s section count moved 804 → 831 in Release
(`gates-e2-fix1.txt:127`); `sqp_solver.cpp.o` moved 802 → 806
(`gates-e2-fix1.txt:124,178`).

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e2-fix1.txt:209-211`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e2-fix1.txt:216,219`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp | rc 0 — `gates-e2-fix1.txt:221` |
| dirty files | 0 | 0 — `gates-e2-fix1.txt:223` |

Fixed since the first chain's G6 defect: `a100fbc3` clang-formatted
`tests/sqp/test_warm_start_battery.cpp` (29 pre-existing violations, `git
diff -w` empty), and this round's `clang-format --dry-run -Werror` over the
`0747050c..a100fbc3` changed `.cpp`/`.h` set is clean
(`psym-derivation-e2-fix1.txt` §0).

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-fix1.txt:24`; `fold-proofs.txt:99` |

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e2-fix1.txt:227`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e2-fix1.txt:229`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-fix1.txt:27`).

## The first chain (declaration `aa349474`), superseded

The first E2 gate declaration, `aa349474`, ran a chain that ended `G1=0 G2=0
G3=0 G4=0 G5=1 G6=1`, fold proofs 46/0 — two mechanical, non-behavioural
defects. That chain's transcripts were written under
`.scratch/m7w2e2gates/` and were never copied into this tree (only its
rehearsal and preflight transcripts, taken before the declaration, live
alongside this README as `rehearsal-*.txt`/`preflight*.txt`); its raw
GATES_SUMMARY/GATES_AGGREGATE lines, read from
`/home/ghecht/Projects/hven/.scratch/m7w2e2gates/logs/gates-e2.log`, are
recorded here verbatim since that file is not part of this commit:

```
GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=1 G6=1
GATES_AGGREGATE rc=1 over [0 0 0 0 1 1]
```

Per `psym-derivation-e2-fix1.txt` §0:

- **G5 — REFUSED (R2), 71 declared, 70 measured in both configurations.**
  The first declaration counted one object twice:
  `tests/sqp/CMakeFiles/hven_sqp_tests.dir/test_sqp_equality_basis.cpp.o`.
  Its own rehearsal (base `0747050c` vs `67a3fde6`, E2 step 2) found the
  object's source unmodified, so the classifier put it in class B (70 = 12 A
  + 58 B). The later merge re-pin commit `aa3ed096` modified that file's
  source, and the declaration added it as "the 13th class A object" (71 =
  13 + 58) without removing it from the class B list — self-consistent but
  wrong. At `a100fbc3`/`e44b8e18` it is class A only, so the true shape is
  70 = 13 A + 57 B, as measured above.
- **G6 — `CLANG_FORMAT_RC=1`**: `tests/sqp/test_warm_start_battery.cpp`
  carried 29 pre-existing clang-format violations and was MODIFIED in the
  range, so the derived changed-file list included it. Fixed in `a100fbc3`
  (see G6 above).

Both commits that superseded the first chain are on this branch:
`a100fbc3` (style fix, clang-format only) and `e44b8e18` (the fix1 gate
declaration commit, which this README records the result of).

No DECLARED FINDING this round: G4 held at 0 differences under the 1e-5
residual gate and no G5/G5b mover fell outside the declared class-A/class-B/
DEBUG_ONLY partition.
