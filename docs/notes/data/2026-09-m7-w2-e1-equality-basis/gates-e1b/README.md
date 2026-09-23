# M7 W2 E1b — gate evidence: DECLARED vs MEASURED

Declaration (written and committed in `27827f14` *before* the chain ran,
nothing in it adjusted after): `psym-derivation-e1b.txt`, runner
`../wrappers-e1b/{gates-e1b.sh,chain_gates_e1b.sh}`. Chain: pid 4148639
(pgid/sid 4148639), detached `setsid`, started 2026-09-23T10:18:45-04:00 at
HEAD `27827f14` with 0 dirty files (`chain.txt:1-2`), ended
2026-09-23T11:24:36-04:00 (`chain.txt:26`). Runner transcript:
`gates-e1b.txt` (copied verbatim from
`.scratch/m7w2e1bgates/logs/gates-e1b.log`). Chain wrapper transcript:
`chain.txt` (copied verbatim from `.scratch/m7w2e1bgates/logs/chain.log`).
Full test and build transcripts, copied verbatim from
`.scratch/m7w2e1bgates/gates/`: `suite-debug.txt`, `suite-release.txt`,
`suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`,
`fold-proofs.txt`, `psym-*.txt` (from `gates/psym/`, including the class B
lists `psym-classB-{Release,Debug}.txt`, the original Debug classification
`psym-classification-Debug.txt` and the cross-configuration copy
`psym-classification-Debug.xcfg.txt` that G5 change (5) hands the shared
assert), `replay/` (the `run-*.log` files renamed `.txt`). The build trees and
object directories are not copied. Rehearsal transcripts, measured before the
declaration and cited by its §2 (already committed in `27827f14`):
`psym-rehearsal-g5.txt`, `psym-rehearsal-classification-{Release,Debug}.txt`,
`psym-rehearsal-run.sh`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e1b.txt` §3) | MEASURED |
|---|---|---|
| tests | 2782 | 2782 — `gates-e1b.txt:34`; `suite-debug.txt:5571` |
| failed | 0 | 0 — `gates-e1b.txt:33`; `suite-debug.txt:5571` |

Not run (`gates-e1b.txt:27-30`): `FailByDesignControl...` (skipped) and the
two disabled `EqpRefinementAb` fixtures, none of them this round's.

## G2 — the long test, alone

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e1b.txt:47`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e1b.txt:46`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, 693.87 sec
(`gates-e1b.txt:39`), run alone.

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2783 | 2783 — `gates-e1b.txt:59`; `suite-release.txt:5573` |
| failed | 0 | 0 — `gates-e1b.txt:58`; `suite-release.txt:5573` |

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e1b.txt:65,67,69` (CMP_RC=0 at `:66,68,70`); `replay/comparison.txt:1-3`; asserted `gates-e1b.txt:73-74` |

No cell moved. `SqpOptions::equality_basis` defaults ON this round; every U0
cell is full rank, so the basis is the identity and every QP is the pre-E1 QP,
identical to the M6 close pin.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 49 / 50 | 49 — `gates-e1b.txt:96`, asserted `:103,107` | 50 — `gates-e1b.txt:144`, asserted `:151,155` |
| class A (source changed) | 7 | 7 — `gates-e1b.txt:97`, asserted `:104,108` | 7 — `gates-e1b.txt:145`, asserted `:152,156` |
| class B (includer, source untouched) | 42 / 43 | 42 — `gates-e1b.txt:98`, asserted `:109` | 43 — `gates-e1b.txt:146`, asserted `:157` |
| UNEXPLAINED | 0 | 0 — `gates-e1b.txt:99,105` | 0 — `gates-e1b.txt:147,153` |
| negative control | ≥ 1 | 25 — `gates-e1b.txt:100,106` | 17 — `gates-e1b.txt:148,154` |
| compare rc | 1 | 1 — `gates-e1b.txt:85,101` | 1 — `gates-e1b.txt:133,149` |
| capture pair | after = before = 149 (ADDED source outside the universe) | 149 = 149 — `gates-e1b.txt:79-81` | 149 = 149 — `gates-e1b.txt:127-129` |
| outside-universe seam object | built, 0 objects of `hven_sqp_seam_tests.dir` in the manifest | `gates-e1b.txt:82-83` | `gates-e1b.txt:130-131` |
| ONLY-AFTER | `[]` | `[]` — `gates-e1b.txt:86` | `[]` — `gates-e1b.txt:134` |
| library ADDED | none | none — `gates-e1b.txt:110` | none — `gates-e1b.txt:158` |

The mover lists are in `psym-classification-{Release,Debug}.txt` and
`psym-classB-{Release,Debug}.txt`. G5 change (5), ruled ACCEPTED by the
settler: the one Debug-only class B object,
`bench/CMakeFiles/hven_sqp_f7_cold.dir/bench_f7_cold.cpp.o` (Debug −O0 frame
offsets +8 from `SqpSolver`'s new member `eq_prox_lift_failures_`), is checked
fail-closed (`gates-e1b.txt:176-178`: Debug-only set == declared, NOISE-ONLY in
Release, the cross-configuration copy removes exactly that line). The shared
cross-configuration assert, unedited, then holds over the copy
(`gates-e1b.txt:179-184`): class B sets equal (42), both unexplained sets
empty, both class A sets all changed-source and identical. The `before` arms
ran `ninja -t cleandead` with 0 outputs removed (`gates-e1b.txt:78,126`); the
tree was restored to `m7 27827f14` after each arm (`gates-e1b.txt:84,132`).

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | {equality_basis, sqp_solver} + {sqp_options, ipqp_engine} | exactly that — `gates-e1b.txt:112-115`, compared `:122`, asserted `:123` | exactly that — `gates-e1b.txt:160-165`, compared `:173`, asserted `:174` |
| objects compared | 43 | 43 — `gates-e1b.txt:116` | 43 — `gates-e1b.txt:167` |
| byte-identical | — | 39 — `gates-e1b.txt:117` | 36 — `gates-e1b.txt:168` |
| DEBUG_ONLY | Release 0 / Debug 3 | 0 — `gates-e1b.txt:118,124` | 3 — `gates-e1b.txt:169,175` (`sqp_print` `:162`, `trace_writer` `:164`, `continuation` `:166`) |
| REFUSED | 0 | 0 — `gates-e1b.txt:120` | 0 — `gates-e1b.txt:171` |

`equality_basis.cpp.o`'s section count moved 731 → 920 in Release
(`gates-e1b.txt:112`) and 8824 → 18018 in Debug (`gates-e1b.txt:160`).

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e1b.txt:191-193`; `install-smoke.txt:1189` |
| export contract | OK, align 32 | OK, align 32 — `gates-e1b.txt:196,198,201`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp | rc 0 — `gates-e1b.txt:203` (file list `:188`) |
| `git diff --check` | rc 0 | rc 0 — `gates-e1b.txt:204` |
| dirty files | 0 | 0 — `gates-e1b.txt:205` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain.txt:24`; `fold-proofs.txt:99` |

(`fold-proofs.txt` carries `ASSERT FAIL` / `REFUSED` lines by design: each
fold proof feeds a deliberately broken input and passes when the gate
refuses it.)

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e1b.txt:209`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e1b.txt:211`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain.txt:27`). No DECLARED FINDING
this round: G4 held at 0 differences and no G5/G5b mover fell outside the
declared class-A/class-B/DEBUG_ONLY partition.
