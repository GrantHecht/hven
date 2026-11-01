# M7 W2 E1 fix round 1 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed in `dd157ecb` *before* the chain ran,
nothing in it adjusted after): `psym-derivation-e1fix1.txt`. Chain: pid
3712822 (pgid/sid 3712822), detached `setsid`, started
2026-09-23T02:33:02-04:00 at HEAD `dd157ecb` with 0 dirty files
(`chain.txt:2`), ended 2026-09-23T03:25:45-04:00 (`chain.txt:26`). Runner
transcript: `gates-e1fix1.txt` (copied verbatim from
`.scratch/m7w2e1fix1gates/logs/gates-e1fix1.log`). Chain wrapper transcript:
`chain.txt` (copied verbatim from `.scratch/m7w2e1fix1gates/logs/chain.log`).
Full test and build transcripts, copied verbatim from
`.scratch/m7w2e1fix1gates/gates/`: `suite-debug.txt`, `suite-release.txt`,
`suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`,
`fold-proofs.txt`, `psym-*.txt` (from `gates/psym/`), `replay/` (the
`run-*.log` files renamed `.txt`). Rehearsal transcripts, measured before
this round's own declaration and cited by its §2 (already committed in
`dd157ecb`): `psym-rehearsal-g5.txt`, `psym-rehearsal-classification-
{Release,Debug}.txt`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README (not the derivation, not a
summary of the derivation).

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e1fix1.txt` §3) | MEASURED |
|---|---|---|
| tests | 2776 | 2776 — `gates-e1fix1.txt:32`; `suite-debug.txt:5559` |
| failed | 0 | 0 — `gates-e1fix1.txt:31`; `suite-debug.txt:5559` |

Not run (`gates-e1fix1.txt:26-28`): `FailByDesignControl...` (skipped) and
the two disabled `EqpRefinementAb` fixtures, none of them this round's.

## G2 — the long test, alone

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-e1fix1.txt:45`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-e1fix1.txt:44`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, 669.10 sec
(`gates-e1fix1.txt:37`; `suite-debug-long.txt:3`), run alone.

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2777 | 2777 — `gates-e1fix1.txt:57`; `suite-release.txt:5561` |
| failed | 0 | 0 — `gates-e1fix1.txt:56`; `suite-release.txt:5561` |

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences | 27 × 75, 0 differences on walk, ssn and ipm — `gates-e1fix1.txt:63,65,67` (CMP_RC=0 at `:64,66,68`); `replay/comparison.txt:1-3`; asserted `gates-e1fix1.txt:71-72` |

No cell moved. `SqpOptions::equality_basis` defaults OFF this round (C2), so
no U0 cell computes a split and every QP is the pre-E1 QP, identical to the
M6 close pin exactly as E1 (basis ON, every cell full rank) and E4 fix1 also
measured 0 differences. There is no DECLARED FINDING from this gate.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 51 | 51 — `gates-e1fix1.txt:92`, asserted `:99,103` | 51 — `gates-e1fix1.txt:139`, asserted `:146,150` |
| class A (source changed) | 8 | 8 — `gates-e1fix1.txt:93`, asserted `:100,104` | 8 — `gates-e1fix1.txt:140`, asserted `:147,151` |
| class B (includer, source untouched) | 43 | 43 — `gates-e1fix1.txt:94`, asserted `:100,105` | 43 — `gates-e1fix1.txt:141`, asserted `:147,152` |
| UNEXPLAINED | 0 | 0 — `gates-e1fix1.txt:95,101` | 0 — `gates-e1fix1.txt:142,148` |
| negative control | ≥ 1 (rehearsal: 22 / 18) | 22 — `gates-e1fix1.txt:96,102` | 18 — `gates-e1fix1.txt:143,149` |
| compare rc | 1 | 1 — `gates-e1fix1.txt:81,97` | 1 — `gates-e1fix1.txt:128,144` |
| capture pair | after = before + ADDED(0) = 149 | 149 = 149 + 0 — `gates-e1fix1.txt:77-79` | 149 = 149 + 0 — `gates-e1fix1.txt:124-126` |
| ONLY-AFTER | `[]` (no ADDED source) | `[]` — `gates-e1fix1.txt:82` | `[]` — `gates-e1fix1.txt:129` |
| library ADDED | none present after, absent before | none — `gates-e1fix1.txt:106` | none — `gates-e1fix1.txt:153` |

The mover lists are in `psym-classification-{Release,Debug}.txt`.
Cross-configuration checks (`gates-e1fix1.txt:171-176`): class B sets equal
(43 objects), both unexplained sets empty, both class A sets all
changed-source and identical to each other. The `before` arms re-configured
and ran `ninja -t cleandead` with 0 outputs removed
(`gates-e1fix1.txt:76,123`). The tree was restored to `m7 dd157ecb` after
each arm (`gates-e1fix1.txt:80,127`).

## G5b — sections (object-section diff, library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | {equality_basis, sqp_solver} + {sqp_options, ipqp_engine, continuation} | exactly that — `gates-e1fix1.txt:108-112`, compared to the declaration `:119`, asserted `:120` | exactly that — `gates-e1fix1.txt:155-161`, compared `:168`, asserted `:169` |
| objects compared | 43 (no ADDED object this round) | 43 — `gates-e1fix1.txt:113` | 43 — `gates-e1fix1.txt:162` |
| byte-identical | — | 38 — `gates-e1fix1.txt:114` | 36 — `gates-e1fix1.txt:163` |
| DEBUG_ONLY | Release 0 / Debug 2 | 0 — `gates-e1fix1.txt:115,121` | 2 — `gates-e1fix1.txt:164,170` (`sqp_print` `:157`, `trace_writer` `:159`) |
| REFUSED | 0 | 0 — `gates-e1fix1.txt:117` | 0 — `gates-e1fix1.txt:166` |

`equality_basis.cpp.o`'s section count moved 686 → 731 in Release
(`gates-e1fix1.txt:108`) and 7777 → 8824 in Debug (`gates-e1fix1.txt:155`).
`sqp_solver.cpp.o` moved 799 → 802 in Release (`gates-e1fix1.txt:110`) and
9990 → 10060 in Debug (`gates-e1fix1.txt:158`). `continuation.cpp.o`'s
symbol list gains `SqpResult`'s special members in both configurations
(`gates-e1fix1.txt:112,161`, the four new report fields per M1); Debug
additionally shows `SqpEqualityBasisReport`'s own special members not
inlined away under Release optimization (`gates-e1fix1.txt:161` vs `:112`).

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e1fix1.txt:183-185`; `install-smoke.txt:1188` |
| export contract | OK, align 32 | OK, align 32 — `gates-e1fix1.txt:188,190,193`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp | rc 0 — `gates-e1fix1.txt:195` (file list `:180`) |
| `git diff --check` | rc 0 | rc 0 — `gates-e1fix1.txt:196` |
| dirty files | 0 | 0 — `gates-e1fix1.txt:197` |

## Fold proofs

| | DECLARED (STATE) | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain.txt:24`; `fold-proofs.txt:99` |

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-e1fix1.txt:201`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-e1fix1.txt:203`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain.txt:27`). No DECLARED FINDING
this round: G4 held at 0 differences and no G5/G5b mover fell outside the
declared class-A/class-B/DEBUG_ONLY partition.
